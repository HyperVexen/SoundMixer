#pragma once

#include "AudioSource.h"
#include <thread>
#include <atomic>
#include <vector>
#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <mmdeviceapi.h>

class ProcessCaptureSource : public AudioSource {
public:
    ProcessCaptureSource(DWORD pid, const std::string& exeName);
    ~ProcessCaptureSource() override;

    bool start() override;
    void stop() override;
    bool isRunning() const override { return m_running.load(std::memory_order_relaxed); }
    std::string getStatusText() const override;

    DWORD getProcessId() const { return m_targetPid; }
    const std::string& getExeName() const { return m_exeName; }

private:
    void captureThreadFunc();
    static void convertToStereoFloat(
        const BYTE* srcData,
        UINT32 frameCount,
        const WAVEFORMATEX* wfx,
        std::vector<float>& outStereo
    );

    DWORD m_targetPid = 0;
    std::string m_exeName;

    std::thread m_thread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopSignal{false};
    std::string m_statusText{"Stopped"};

    IAudioClient* m_pAudioClient = nullptr;
    IAudioCaptureClient* m_pCaptureClient = nullptr;
    HANDLE m_hCaptureEvent = nullptr;

    WAVEFORMATEX* m_pWfx = nullptr;
    uint32_t m_deviceSampleRate = 48000;
};
