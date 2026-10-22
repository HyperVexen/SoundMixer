#include "AudioOutput.h"
#include <avrt.h>
#include <ksmedia.h>
#include <iostream>
#include <cstring>
#include <algorithm>

static std::wstring utf8ToWide(const std::string& str) {
    if (str.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &result[0], size);
    return result;
}

AudioOutput::AudioOutput(const std::string& deviceId, const std::string& deviceName, bool isPrimary)
    : m_deviceId(deviceId), m_deviceName(deviceName), m_isPrimary(isPrimary),
      m_wideDeviceId(utf8ToWide(deviceId)),
      m_ringBuffer(AudioConfig::RING_BUFFER_CAPACITY) {
    std::string lowerName = deviceName;
    for (auto& c : lowerName) c = static_cast<char>(tolower(c));
    if (lowerName.find("cable") != std::string::npos ||
        lowerName.find("vb-audio") != std::string::npos ||
        lowerName.find("voicemod") != std::string::npos ||
        lowerName.find("virtual") != std::string::npos) {
        m_isVirtualDevice = true;
        m_routeMode.store(OutputRouteMode::VirtualMicStream, std::memory_order_relaxed);
    }
}

AudioOutput::~AudioOutput() {
    stop();
}

void AudioOutput::pushFrames(const float* stereoFrames, size_t frameCount) {
    if (!stereoFrames || frameCount == 0) return;
    m_ringBuffer.write(stereoFrames, frameCount);
}

bool AudioOutput::start() {
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
    if (FAILED(hr) || !pEnum) return false;

    if (m_wideDeviceId.empty()) {
        hr = pEnum->GetDefaultAudioEndpoint(eRender, eMultimedia, &m_pDevice);
    } else {
        hr = pEnum->GetDevice(m_wideDeviceId.c_str(), &m_pDevice);
    }
    pEnum->Release();

    if (FAILED(hr) || !m_pDevice) return false;

    hr = m_pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&m_pAudioClient);
    if (FAILED(hr) || !m_pAudioClient) return false;

    hr = m_pAudioClient->GetMixFormat(&m_pWfx);
    if (FAILED(hr) || !m_pWfx) return false;

    m_hRenderEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!m_hRenderEvent) return false;

    // Try IAudioClient3 for lowest latency
    IAudioClient3* pClient3 = nullptr;
    hr = m_pAudioClient->QueryInterface(__uuidof(IAudioClient3), (void**)&pClient3);
    bool client3Init = false;

    if (SUCCEEDED(hr) && pClient3) {
        UINT32 defP = 0, fundP = 0, minP = 0, maxP = 0;
        hr = pClient3->GetSharedModeEnginePeriod(m_pWfx, &defP, &fundP, &minP, &maxP);
        if (SUCCEEDED(hr) && minP > 0) {
            UINT32 period = minP;
            hr = pClient3->InitializeSharedAudioStream(
                AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                period,
                m_pWfx,
                nullptr
            );
            if (SUCCEEDED(hr)) {
                client3Init = true;
                m_bufferPeriodFrames = period;
            }
        }
        pClient3->Release();
    }

    if (!client3Init) {
        REFERENCE_TIME hnsBuffer = 100000; // 10 ms
        hr = m_pAudioClient->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
            hnsBuffer,
            0,
            m_pWfx,
            nullptr
        );
        if (FAILED(hr)) return false;
    }

    hr = m_pAudioClient->SetEventHandle(m_hRenderEvent);
    if (FAILED(hr)) return false;

    hr = m_pAudioClient->GetBufferSize(&m_totalBufferFrames);
    if (FAILED(hr)) return false;

    if (m_bufferPeriodFrames == 0) {
        m_bufferPeriodFrames = m_totalBufferFrames / 2;
    }

    hr = m_pAudioClient->GetService(__uuidof(IAudioRenderClient), (void**)&m_pRenderDataClient);
    if (FAILED(hr) || !m_pRenderDataClient) return false;

    // Pre-fill buffer with silence
    BYTE* pData = nullptr;
    if (SUCCEEDED(m_pRenderDataClient->GetBuffer(m_totalBufferFrames, &pData))) {
        std::memset(pData, 0, m_totalBufferFrames * m_pWfx->nBlockAlign);
        m_pRenderDataClient->ReleaseBuffer(m_totalBufferFrames, 0);
    }

    // Pre-cushion the ring buffer for secondary outputs to minimal low-latency target
    m_ringBuffer.flush();
    if (!m_isPrimary.load(std::memory_order_relaxed)) {
        constexpr size_t cushionFrames = AudioConfig::TARGET_RING_FILL;
        std::vector<float> startupCushion(cushionFrames * 2, 0.0f);
        m_ringBuffer.write(startupCushion.data(), cushionFrames);
    }

    hr = m_pAudioClient->Start();
    if (FAILED(hr)) return false;

    m_running.store(true);
    m_thread = std::thread(&AudioOutput::renderThreadFunc, this);
    return true;
}

void AudioOutput::stop() {
    m_stopSignal.store(true);
    if (m_hRenderEvent) {
        SetEvent(m_hRenderEvent);
    }

    if (m_thread.joinable()) {
        m_thread.join();
    }

    if (m_pAudioClient) {
        m_pAudioClient->Stop();
    }
    if (m_pRenderDataClient) {
        m_pRenderDataClient->Release();
        m_pRenderDataClient = nullptr;
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
    if (m_hRenderEvent) {
        CloseHandle(m_hRenderEvent);
        m_hRenderEvent = nullptr;
    }

    m_running.store(false);
}

void AudioOutput::convertAndWrite(const float* stereoSrc, BYTE* destBytes, UINT32 frameCount) {
    if (!stereoSrc || !destBytes || frameCount == 0 || !m_pWfx) return;

    WORD channels = m_pWfx->nChannels;
    WORD bitsPerSample = m_pWfx->wBitsPerSample;

    bool isFloat = (m_pWfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT);
    if (m_pWfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        const auto* ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(m_pWfx);
        if (ext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
            isFloat = true;
        }
    }

    if (isFloat && bitsPerSample == 32) {
        float* destFloat = reinterpret_cast<float*>(destBytes);
        for (UINT32 i = 0; i < frameCount; ++i) {
            float l = stereoSrc[i * 2 + 0];
            float r = stereoSrc[i * 2 + 1];

            if (channels == 1) {
                destFloat[i] = 0.5f * (l + r);
            } else if (channels == 2) {
                destFloat[i * 2 + 0] = l;
                destFloat[i * 2 + 1] = r;
            } else {
                destFloat[i * channels + 0] = l;
                destFloat[i * channels + 1] = r;
                for (WORD c = 2; c < channels; ++c) {
                    destFloat[i * channels + c] = 0.0f;
                }
            }
        }
    } else if (!isFloat && bitsPerSample == 16) {
        int16_t* dest16 = reinterpret_cast<int16_t*>(destBytes);
        for (UINT32 i = 0; i < frameCount; ++i) {
            float l = (std::max)(-1.0f, (std::min)(1.0f, stereoSrc[i * 2 + 0]));
            float r = (std::max)(-1.0f, (std::min)(1.0f, stereoSrc[i * 2 + 1]));

            int16_t sL = static_cast<int16_t>(l * 32767.0f);
            int16_t sR = static_cast<int16_t>(r * 32767.0f);

            if (channels == 1) {
                dest16[i] = static_cast<int16_t>((sL + sR) / 2);
            } else if (channels == 2) {
                dest16[i * 2 + 0] = sL;
                dest16[i * 2 + 1] = sR;
            } else {
                dest16[i * channels + 0] = sL;
                dest16[i * channels + 1] = sR;
                for (WORD c = 2; c < channels; ++c) {
                    dest16[i * channels + c] = 0;
                }
            }
        }
    } else if (!isFloat && bitsPerSample == 24) {
        BYTE* destPtr = destBytes;
        for (UINT32 i = 0; i < frameCount; ++i) {
            float l = (std::max)(-1.0f, (std::min)(1.0f, stereoSrc[i * 2 + 0]));
            float r = (std::max)(-1.0f, (std::min)(1.0f, stereoSrc[i * 2 + 1]));

            int32_t sL = static_cast<int32_t>(l * 8388607.0f);
            int32_t sR = static_cast<int32_t>(r * 8388607.0f);

            BYTE* frameBase = destPtr + i * m_pWfx->nBlockAlign;
            frameBase[0] = static_cast<BYTE>(sL & 0xFF);
            frameBase[1] = static_cast<BYTE>((sL >> 8) & 0xFF);
            frameBase[2] = static_cast<BYTE>((sL >> 16) & 0xFF);

            if (channels >= 2) {
                frameBase[3] = static_cast<BYTE>(sR & 0xFF);
                frameBase[4] = static_cast<BYTE>((sR >> 8) & 0xFF);
                frameBase[5] = static_cast<BYTE>((sR >> 16) & 0xFF);
            }
        }
    }
}

void AudioOutput::renderThreadFunc() {
    DWORD taskIndex = 0;
    HANDLE hAvrt = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);

    m_scratchStereo.resize(8192 * 2);

    while (!m_stopSignal.load(std::memory_order_relaxed)) {
        DWORD waitRes = WaitForSingleObject(m_hRenderEvent, 100);
        if (waitRes != WAIT_OBJECT_0) {
            continue;
        }

        if (m_stopSignal.load(std::memory_order_relaxed)) break;

        UINT32 padding = 0;
        HRESULT hr = m_pAudioClient->GetCurrentPadding(&padding);
        if (FAILED(hr)) break;

        UINT32 framesAvailable = (m_totalBufferFrames > padding) ? (m_totalBufferFrames - padding) : 0;
        if (framesAvailable > 0) {
            BYTE* pData = nullptr;
            hr = m_pRenderDataClient->GetBuffer(framesAvailable, &pData);
            if (SUCCEEDED(hr) && pData) {
                if (m_scratchStereo.size() < framesAvailable * 2) {
                    m_scratchStereo.resize(framesAvailable * 2);
                }

                if (m_isPrimary.load(std::memory_order_relaxed) && m_audioCallback) {
                    // Primary output generates the fresh master mix and broadcasts to secondary outputs
                    m_audioCallback(m_scratchStereo.data(), framesAvailable);
                } else {
                    // Secondary output pulls from its own ring buffer
                    size_t queued = m_ringBuffer.getAvailableRead();
                    if (queued > 1024) {
                        // Maintain lowest latency by trimming backed up frames
                        size_t discard = queued - 512;
                        m_ringBuffer.read(m_scratchStereo.data(), (std::min)(discard, (size_t)256));
                    }

                    size_t read = m_ringBuffer.read(m_scratchStereo.data(), framesAvailable);
                    if (read < framesAvailable) {
                        std::memset(&m_scratchStereo[read * 2], 0, (framesAvailable - read) * 2 * sizeof(float));
                        m_glitchCount.fetch_add(1, std::memory_order_relaxed);
                    }
                }

                // Apply per-output volume & mute with smooth de-zippering
                float targetVol = m_muted.load(std::memory_order_relaxed) ? 0.0f : m_volume.load(std::memory_order_relaxed);
                float startGain = m_smoothedGain;
                float endGain = targetVol;
                float gainStep = (endGain - startGain) / static_cast<float>(framesAvailable);

                if (startGain > 1e-4f || endGain > 1e-4f) {
                    float g = startGain;
                    for (size_t i = 0; i < framesAvailable; ++i) {
                        g += gainStep;
                        m_scratchStereo[i * 2 + 0] *= g;
                        m_scratchStereo[i * 2 + 1] *= g;
                    }
                } else {
                    std::fill_n(m_scratchStereo.data(), framesAvailable * 2, 0.0f);
                }
                m_smoothedGain = endGain;

                // Compute per-output meter levels
                float pL, pR, rL, rR;
                ComputeStereoLevels(m_scratchStereo.data(), framesAvailable, pL, pR, rL, rR);
                m_peakL.store(pL, std::memory_order_relaxed);
                m_peakR.store(pR, std::memory_order_relaxed);
                m_rmsL.store(rL, std::memory_order_relaxed);
                m_rmsR.store(rR, std::memory_order_relaxed);

                // Convert to target hardware endpoint format and write
                convertAndWrite(m_scratchStereo.data(), pData, framesAvailable);
                m_pRenderDataClient->ReleaseBuffer(framesAvailable, 0);
            }
        }
    }

    if (hAvrt) {
        AvRevertMmThreadCharacteristics(hAvrt);
    }
}
