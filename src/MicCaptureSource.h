#pragma once

#include "AudioSource.h"
#include "VoiceProcessor.h"
#include <thread>
#include <atomic>
#include <vector>
#include <audioclient.h>
#include <mmdeviceapi.h>

class MicCaptureSource : public AudioSource {
public:
    MicCaptureSource(const std::string& deviceId, const std::string& name);
    ~MicCaptureSource() override;

    bool start() override;
    void stop() override;
    bool isRunning() const override { return m_running.load(std::memory_order_relaxed); }
    std::string getStatusText() const override;

    uint32_t getDeviceSampleRate() const { return m_deviceSampleRate; }
    uint32_t getDeviceChannels() const { return m_deviceChannels; }
    uint32_t getBufferPeriodFrames() const { return m_bufferPeriodFrames; }

    VoiceProcessor& getVoiceProcessor() { return m_voiceProcessor; }

private:
    void captureThreadFunc();
    static void convertToStereoFloat(
        const BYTE* srcData,
        UINT32 frameCount,
        const WAVEFORMATEX* wfx,
        std::vector<float>& outStereo
    );

    VoiceProcessor m_voiceProcessor;

    std::wstring m_wideDeviceId;
    std::thread m_thread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopSignal{false};
    std::string m_statusText{"Stopped"};

    IMMDevice* m_pDevice = nullptr;
    IAudioClient* m_pAudioClient = nullptr;
    IAudioCaptureClient* m_pCaptureClient = nullptr;
    HANDLE m_hCaptureEvent = nullptr;

    WAVEFORMATEX* m_pWfx = nullptr;
    uint32_t m_deviceSampleRate = 48000;
    uint32_t m_deviceChannels = 2;
    uint32_t m_bufferPeriodFrames = 0;
};
