#include "ProcessCaptureSource.h"
#include <avrt.h>
#include <ksmedia.h>
#include <wrl/implements.h>
#include <wrl/client.h>
#include <iostream>

using namespace Microsoft::WRL;

class ProcessLoopbackActivationHandler : public RuntimeClass<
    RuntimeClassFlags<ClassicCom>,
    IActivateAudioInterfaceCompletionHandler,
    FtmBase>
{
public:
    ProcessLoopbackActivationHandler() {
        m_hCompletedEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    }

    ~ProcessLoopbackActivationHandler() override {
        if (m_hCompletedEvent) {
            CloseHandle(m_hCompletedEvent);
        }
    }

    STDMETHODIMP ActivateCompleted(IActivateAudioInterfaceAsyncOperation* activateOperation) override {
        HRESULT hrActivate = E_FAIL;
        IUnknown* pUnk = nullptr;
        HRESULT hr = activateOperation->GetActivateResult(&hrActivate, &pUnk);
        if (SUCCEEDED(hr) && SUCCEEDED(hrActivate) && pUnk) {
            pUnk->QueryInterface(__uuidof(IAudioClient), (void**)&m_pAudioClient);
            pUnk->Release();
        }
        m_hrResult = SUCCEEDED(hr) ? hrActivate : hr;
        SetEvent(m_hCompletedEvent);
        return S_OK;
    }

    HANDLE getCompletedEvent() const { return m_hCompletedEvent; }
    HRESULT getResult() const { return m_hrResult; }
    IAudioClient* getAudioClient() {
        if (m_pAudioClient) {
            m_pAudioClient->AddRef();
        }
        return m_pAudioClient;
    }

private:
    HANDLE m_hCompletedEvent = nullptr;
    HRESULT m_hrResult = E_FAIL;
    IAudioClient* m_pAudioClient = nullptr;
};

ProcessCaptureSource::ProcessCaptureSource(DWORD pid, const std::string& exeName)
    : AudioSource(SourceType::Application, std::to_string(pid) + ":" + exeName, exeName + " (" + std::to_string(pid) + ")"),
      m_targetPid(pid), m_exeName(exeName) {}

ProcessCaptureSource::~ProcessCaptureSource() {
    stop();
}

std::string ProcessCaptureSource::getStatusText() const {
    return m_statusText;
}

void ProcessCaptureSource::convertToStereoFloat(
    const BYTE* srcData,
    UINT32 frameCount,
    const WAVEFORMATEX* wfx,
    std::vector<float>& outStereo
) {
    outStereo.resize(frameCount * 2);
    if (!srcData || frameCount == 0 || !wfx) {
        std::fill(outStereo.begin(), outStereo.end(), 0.0f);
        return;
    }

    WORD channels = wfx->nChannels;
    WORD bitsPerSample = wfx->wBitsPerSample;

    bool isFloat = (wfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT);
    if (wfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        const auto* ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(wfx);
        if (ext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
            isFloat = true;
        }
    }

    for (UINT32 i = 0; i < frameCount; ++i) {
        float l = 0.0f;
        float r = 0.0f;

        if (isFloat && bitsPerSample == 32) {
            const float* srcFloat = reinterpret_cast<const float*>(srcData + i * wfx->nBlockAlign);
            if (channels == 1) {
                l = r = srcFloat[0];
            } else if (channels == 2) {
                l = srcFloat[0];
                r = srcFloat[1];
            } else if (channels >= 6) { // 5.1 or 7.1 downmix
                l = srcFloat[0] + 0.707f * srcFloat[2] + 0.707f * srcFloat[4];
                r = srcFloat[1] + 0.707f * srcFloat[2] + 0.707f * srcFloat[5];
            } else {
                l = srcFloat[0];
                r = srcFloat[1];
            }
        } else if (!isFloat && bitsPerSample == 16) {
            const int16_t* src16 = reinterpret_cast<const int16_t*>(srcData + i * wfx->nBlockAlign);
            constexpr float scale = 1.0f / 32768.0f;
            if (channels == 1) {
                l = r = static_cast<float>(src16[0]) * scale;
            } else if (channels >= 2) {
                l = static_cast<float>(src16[0]) * scale;
                r = static_cast<float>(src16[1]) * scale;
            }
        } else if (!isFloat && bitsPerSample == 24) {
            const BYTE* b = srcData + i * wfx->nBlockAlign;
            auto read24 = [](const BYTE* ptr) -> float {
                int32_t val = (ptr[0] << 8) | (ptr[1] << 16) | (ptr[2] << 24);
                return static_cast<float>(val >> 8) * (1.0f / 8388608.0f);
            };
            if (channels == 1) {
                l = r = read24(b);
            } else if (channels >= 2) {
                l = read24(b);
                r = read24(b + 3);
            }
        } else if (!isFloat && bitsPerSample == 32) {
            const int32_t* src32 = reinterpret_cast<const int32_t*>(srcData + i * wfx->nBlockAlign);
            constexpr float scale = 1.0f / 2147483648.0f;
            if (channels == 1) {
                l = r = static_cast<float>(src32[0]) * scale;
            } else if (channels >= 2) {
                l = static_cast<float>(src32[0]) * scale;
                r = static_cast<float>(src32[1]) * scale;
            }
        }

        outStereo[i * 2 + 0] = l;
        outStereo[i * 2 + 1] = r;
    }
}

bool ProcessCaptureSource::start() {
    if (m_running.load()) return true;

    stop();
    m_stopSignal.store(false);

    AUDIOCLIENT_ACTIVATION_PARAMS params = {};
    params.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
    params.ProcessLoopbackParams.TargetProcessId = m_targetPid;
    params.ProcessLoopbackParams.ProcessLoopbackMode = PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;

    PROPVARIANT activateParams = {};
    activateParams.vt = VT_BLOB;
    activateParams.blob.cbSize = sizeof(params);
    activateParams.blob.pBlobData = reinterpret_cast<BYTE*>(&params);

    auto pHandler = Make<ProcessLoopbackActivationHandler>();

    ComPtr<IActivateAudioInterfaceAsyncOperation> asyncOp;
    HRESULT hr = ActivateAudioInterfaceAsync(
        VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK,
        __uuidof(IAudioClient),
        &activateParams,
        pHandler.Get(),
        &asyncOp
    );

    if (FAILED(hr)) {
        m_statusText = "ActivateAudioInterfaceAsync call failed: " + std::to_string(hr);
        return false;
    }

    // Wait for activation completion (up to 2 seconds)
    DWORD waitRes = WaitForSingleObject(pHandler->getCompletedEvent(), 2000);
    if (waitRes != WAIT_OBJECT_0 || FAILED(pHandler->getResult())) {
        m_statusText = "Process Loopback activation failed or timed out: " + std::to_string(pHandler->getResult());
        return false;
    }

    m_pAudioClient = pHandler->getAudioClient();
    if (!m_pAudioClient) {
        m_statusText = "Failed to obtain IAudioClient from activation";
        return false;
    }

    // Process loopback devices do not implement GetMixFormat (returns E_NOTIMPL).
    // We construct our desired standard format: 48 kHz, stereo, 32-bit float.
    WAVEFORMATEXTENSIBLE wfxExt = {};
    wfxExt.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wfxExt.Format.nChannels = 2;
    wfxExt.Format.nSamplesPerSec = AudioConfig::SAMPLE_RATE;
    wfxExt.Format.wBitsPerSample = 32;
    wfxExt.Format.nBlockAlign = (wfxExt.Format.nChannels * wfxExt.Format.wBitsPerSample) / 8;
    wfxExt.Format.nAvgBytesPerSec = wfxExt.Format.nSamplesPerSec * wfxExt.Format.nBlockAlign;
    wfxExt.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    wfxExt.Samples.wValidBitsPerSample = 32;
    wfxExt.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
    wfxExt.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

    m_pWfx = (WAVEFORMATEX*)CoTaskMemAlloc(sizeof(WAVEFORMATEXTENSIBLE));
    std::memcpy(m_pWfx, &wfxExt, sizeof(WAVEFORMATEXTENSIBLE));

    m_deviceSampleRate = m_pWfx->nSamplesPerSec;
    m_resampler.reset(m_deviceSampleRate, AudioConfig::SAMPLE_RATE);

    m_hCaptureEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!m_hCaptureEvent) {
        std::string err = "Failed to create event";
        stop();
        m_statusText = err;
        return false;
    }

    // Initialize in loopback shared event-driven mode
    REFERENCE_TIME hnsBuffer = 200000; // 20 ms buffer
    hr = m_pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        hnsBuffer,
        0,
        m_pWfx,
        nullptr
    );

    if (FAILED(hr)) {
        // Try without AUDCLNT_STREAMFLAGS_LOOPBACK
        hr = m_pAudioClient->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
            hnsBuffer,
            0,
            m_pWfx,
            nullptr
        );
    }

    if (FAILED(hr)) {
        // Fallback to 16-bit PCM if float is rejected
        WAVEFORMATEX wfx16 = {};
        wfx16.wFormatTag = WAVE_FORMAT_PCM;
        wfx16.nChannels = 2;
        wfx16.nSamplesPerSec = AudioConfig::SAMPLE_RATE;
        wfx16.wBitsPerSample = 16;
        wfx16.nBlockAlign = (wfx16.nChannels * wfx16.wBitsPerSample) / 8;
        wfx16.nAvgBytesPerSec = wfx16.nSamplesPerSec * wfx16.nBlockAlign;
        wfx16.cbSize = 0;

        std::memcpy(m_pWfx, &wfx16, sizeof(WAVEFORMATEX));

        hr = m_pAudioClient->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
            hnsBuffer,
            0,
            m_pWfx,
            nullptr
        );
        if (FAILED(hr)) {
            hr = m_pAudioClient->Initialize(
                AUDCLNT_SHAREMODE_SHARED,
                AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                hnsBuffer,
                0,
                m_pWfx,
                nullptr
            );
        }
    }

    if (FAILED(hr)) {
        std::string err = "Process loopback client Initialize failed: " + std::to_string(hr);
        stop();
        m_statusText = err;
        return false;
    }

    hr = m_pAudioClient->SetEventHandle(m_hCaptureEvent);
    if (FAILED(hr)) {
        m_statusText = "Failed to set event handle";
        stop();
        return false;
    }

    hr = m_pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&m_pCaptureClient);
    if (FAILED(hr) || !m_pCaptureClient) {
        m_statusText = "Failed to get capture client";
        stop();
        return false;
    }

    hr = m_pAudioClient->Start();
    if (FAILED(hr)) {
        m_statusText = "Failed to start capture client";
        stop();
        return false;
    }

    m_running.store(true);
    m_statusText = "Capturing (Process Loopback)";
    m_thread = std::thread(&ProcessCaptureSource::captureThreadFunc, this);
    return true;
}

void ProcessCaptureSource::stop() {
    m_stopSignal.store(true);
    if (m_hCaptureEvent) {
        SetEvent(m_hCaptureEvent);
    }

    if (m_thread.joinable()) {
        m_thread.join();
    }

    if (m_pAudioClient) {
        m_pAudioClient->Stop();
    }

    if (m_pCaptureClient) {
        m_pCaptureClient->Release();
        m_pCaptureClient = nullptr;
    }

    if (m_pAudioClient) {
        m_pAudioClient->Release();
        m_pAudioClient = nullptr;
    }

    if (m_pWfx) {
        CoTaskMemFree(m_pWfx);
        m_pWfx = nullptr;
    }

    if (m_hCaptureEvent) {
        CloseHandle(m_hCaptureEvent);
        m_hCaptureEvent = nullptr;
    }

    m_running.store(false);
    m_statusText = "Stopped";
}

void ProcessCaptureSource::captureThreadFunc() {
    DWORD taskIndex = 0;
    HANDLE hAvrt = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);

    std::vector<float> convertedStereo;
    std::vector<float> resampledStereo(8192 * 2);

    while (!m_stopSignal.load(std::memory_order_relaxed)) {
        DWORD waitRes = WaitForSingleObject(m_hCaptureEvent, 100);
        if (waitRes != WAIT_OBJECT_0) {
            continue;
        }

        if (m_stopSignal.load(std::memory_order_relaxed)) break;

        UINT32 packetLength = 0;
        HRESULT hr = m_pCaptureClient->GetNextPacketSize(&packetLength);
        if (FAILED(hr)) break;

        while (packetLength > 0) {
            BYTE* pData = nullptr;
            UINT32 numFramesRead = 0;
            DWORD flags = 0;

            hr = m_pCaptureClient->GetBuffer(&pData, &numFramesRead, &flags, nullptr, nullptr);
            if (FAILED(hr)) break;

            if (numFramesRead > 0) {
                if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                    convertedStereo.assign(numFramesRead * 2, 0.0f);
                } else {
                    convertToStereoFloat(pData, numFramesRead, m_pWfx, convertedStereo);
                }

                // Resample to 48 kHz stereo
                size_t outFrames = m_resampler.process(
                    convertedStereo.data(),
                    numFramesRead,
                    resampledStereo.data(),
                    resampledStereo.size() / 2
                );

                if (outFrames > 0) {
                    // Metering (pre-fader)
                    float pL, pR, rL, rR;
                    ComputeStereoLevels(resampledStereo.data(), outFrames, pL, pR, rL, rR);
                    setLevels(pL, pR, rL, rR);

                    // Push into ring buffer
                    size_t written = m_ringBuffer.write(resampledStereo.data(), outFrames);
                    if (written < outFrames) {
                        incrementGlitchCount();
                    }
                }
            }

            m_pCaptureClient->ReleaseBuffer(numFramesRead);
            hr = m_pCaptureClient->GetNextPacketSize(&packetLength);
            if (FAILED(hr)) break;
        }
    }

    if (hAvrt) {
        AvRevertMmThreadCharacteristics(hAvrt);
    }
}
