#include "MicCaptureSource.h"
#include <avrt.h>
#include <iostream>
#include <ksmedia.h>

MicCaptureSource::MicCaptureSource(const std::string& deviceId, const std::string& name)
    : AudioSource(SourceType::Microphone, deviceId, name) {
    if (!deviceId.empty()) {
        int size = MultiByteToWideChar(CP_UTF8, 0, deviceId.data(), (int)deviceId.size(), nullptr, 0);
        m_wideDeviceId.resize(size);
        MultiByteToWideChar(CP_UTF8, 0, deviceId.data(), (int)deviceId.size(), &m_wideDeviceId[0], size);
    }
}

MicCaptureSource::~MicCaptureSource() {
    stop();
}

std::string MicCaptureSource::getStatusText() const {
    return m_statusText;
}

void MicCaptureSource::convertToStereoFloat(
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
            } else if (channels >= 2) {
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
            const BYTE* frameBytes = srcData + i * wfx->nBlockAlign;
            constexpr float scale = 1.0f / 8388608.0f;
            auto read24 = [](const BYTE* b) -> float {
                int32_t val = (b[0] << 8) | (b[1] << 16) | (b[2] << 24);
                return static_cast<float>(val >> 8) * (1.0f / 8388608.0f);
            };
            if (channels == 1) {
                l = r = read24(frameBytes);
            } else if (channels >= 2) {
                l = read24(frameBytes);
                r = read24(frameBytes + 3);
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

bool MicCaptureSource::start() {
    if (m_running.load()) return true;

    stop();
    m_stopSignal.store(false);

    IMMDeviceEnumerator* pEnum = nullptr;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&pEnum
    );
    if (FAILED(hr) || !pEnum) {
        m_statusText = "Failed to create device enumerator";
        return false;
    }

    if (m_wideDeviceId.empty()) {
        hr = pEnum->GetDefaultAudioEndpoint(eCapture, eCommunications, &m_pDevice);
        if (FAILED(hr)) {
            hr = pEnum->GetDefaultAudioEndpoint(eCapture, eMultimedia, &m_pDevice);
        }
    } else {
        hr = pEnum->GetDevice(m_wideDeviceId.c_str(), &m_pDevice);
    }
    pEnum->Release();

    if (FAILED(hr) || !m_pDevice) {
        m_statusText = "Device not found or unplugged";
        return false;
    }

    hr = m_pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&m_pAudioClient);
    if (FAILED(hr) || !m_pAudioClient) {
        m_statusText = "Failed to activate audio client";
        if (m_pDevice) { m_pDevice->Release(); m_pDevice = nullptr; }
        return false;
    }

    hr = m_pAudioClient->GetMixFormat(&m_pWfx);
    if (FAILED(hr) || !m_pWfx) {
        m_statusText = "Failed to get mix format";
        stop();
        return false;
    }

    m_deviceSampleRate = m_pWfx->nSamplesPerSec;
    m_deviceChannels = m_pWfx->nChannels;
    m_resampler.reset(m_deviceSampleRate, AudioConfig::SAMPLE_RATE);

    m_hCaptureEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!m_hCaptureEvent) {
        m_statusText = "Failed to create event";
        stop();
        return false;
    }

    // Try IAudioClient3 for low-latency shared mode
    IAudioClient3* pClient3 = nullptr;
    hr = m_pAudioClient->QueryInterface(__uuidof(IAudioClient3), (void**)&pClient3);
    bool client3Init = false;

    if (SUCCEEDED(hr) && pClient3) {
        UINT32 defaultPeriod = 0, fundamentalPeriod = 0, minPeriod = 0, maxPeriod = 0;
        hr = pClient3->GetSharedModeEnginePeriod(m_pWfx, &defaultPeriod, &fundamentalPeriod, &minPeriod, &maxPeriod);
        if (SUCCEEDED(hr) && minPeriod > 0) {
            // Pick lowest period supported
            UINT32 periodToUse = minPeriod;
            hr = pClient3->InitializeSharedAudioStream(
                AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                periodToUse,
                m_pWfx,
                nullptr
            );
            if (SUCCEEDED(hr)) {
                client3Init = true;
                m_bufferPeriodFrames = periodToUse;
            }
        }
        pClient3->Release();
    }

    if (!client3Init) {
        // Fallback to standard IAudioClient event-driven shared mode
        REFERENCE_TIME hnsBuffer = 100000; // 10 ms
        hr = m_pAudioClient->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
            hnsBuffer,
            0,
            m_pWfx,
            nullptr
        );
        if (FAILED(hr)) {
            m_statusText = "Failed to initialize audio client: " + std::to_string(hr);
            stop();
            return false;
        }
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
    m_statusText = "Active (Low-Latency Capture)";
    m_thread = std::thread(&MicCaptureSource::captureThreadFunc, this);
    return true;
}

void MicCaptureSource::stop() {
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

    if (m_pDevice) {
        m_pDevice->Release();
        m_pDevice = nullptr;
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

void MicCaptureSource::captureThreadFunc() {
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
