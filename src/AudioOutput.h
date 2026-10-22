#pragma once

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <functional>
#include "RingBuffer.h"
#include "AudioCommon.h"

enum class OutputRouteMode {
    VirtualMicStream = 0, // Mic + Voice FX + Soundboard (for Discord / Games)
    FullMix = 1          // Full Master Mix (for physical speakers / headphones)
};

class AudioOutput {
public:
    using AudioRenderCallback = std::function<void(float* outStereoBuffer, UINT32 frameCount)>;

    AudioOutput(const std::string& deviceId, const std::string& deviceName, bool isPrimary = false);
    ~AudioOutput();

    bool start();
    void stop();
    bool isRunning() const { return m_running.load(std::memory_order_relaxed); }

    const std::string& getId() const { return m_deviceId; }
    const std::string& getName() const { return m_deviceName; }

    bool isPrimary() const { return m_isPrimary.load(std::memory_order_relaxed); }
    void setPrimary(bool primary) { m_isPrimary.store(primary, std::memory_order_relaxed); }

    OutputRouteMode getRouteMode() const { return m_routeMode.load(std::memory_order_relaxed); }
    void setRouteMode(OutputRouteMode mode) { m_routeMode.store(mode, std::memory_order_relaxed); }

    bool isVirtualDevice() const { return m_isVirtualDevice; }
    void setVirtualDevice(bool v) { m_isVirtualDevice = v; }

    // Callback used when this output is Primary to fetch fresh mixed frames
    void setAudioCallback(AudioRenderCallback cb) { m_audioCallback = cb; }

    // Volume & Mute (safe from UI thread)
    void setVolume(float vol) { m_volume.store(vol, std::memory_order_relaxed); }
    float getVolume() const { return m_volume.load(std::memory_order_relaxed); }

    void setMuted(bool mute) { m_muted.store(mute, std::memory_order_relaxed); }
    bool isMuted() const { return m_muted.load(std::memory_order_relaxed); }

    // Metering
    void getLevels(float& outPL, float& outPR, float& outRL, float& outRR) const {
        outPL = m_peakL.load(std::memory_order_relaxed);
        outPR = m_peakR.load(std::memory_order_relaxed);
        outRL = m_rmsL.load(std::memory_order_relaxed);
        outRR = m_rmsR.load(std::memory_order_relaxed);
    }

    uint32_t getGlitchCount() const { return m_glitchCount.load(std::memory_order_relaxed); }
    uint32_t getBufferPeriodFrames() const { return m_bufferPeriodFrames; }
    float getLatencyMs() const {
        return (m_bufferPeriodFrames > 0)
            ? (static_cast<float>(m_bufferPeriodFrames) * 1000.0f / static_cast<float>(AudioConfig::SAMPLE_RATE))
            : 0.0f;
    }

    // Audio input to this output's ring buffer (called when this is a secondary output)
    void pushFrames(const float* stereoFrames, size_t frameCount);

    RingBuffer& getRingBuffer() { return m_ringBuffer; }

private:
    void renderThreadFunc();
    void convertAndWrite(const float* stereoSrc, BYTE* destBytes, UINT32 frameCount);

    std::string m_deviceId;
    std::string m_deviceName;
    std::wstring m_wideDeviceId;

    std::atomic<bool> m_isPrimary{false};
    std::atomic<OutputRouteMode> m_routeMode{OutputRouteMode::FullMix};
    bool m_isVirtualDevice = false;
    AudioRenderCallback m_audioCallback;

    std::atomic<float> m_volume{1.0f};
    std::atomic<bool> m_muted{false};
    float m_smoothedGain = 1.0f;

    std::atomic<float> m_peakL{0.0f};
    std::atomic<float> m_peakR{0.0f};
    std::atomic<float> m_rmsL{0.0f};
    std::atomic<float> m_rmsR{0.0f};
    std::atomic<uint32_t> m_glitchCount{0};

    RingBuffer m_ringBuffer;

    std::thread m_thread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopSignal{false};

    IMMDevice* m_pDevice = nullptr;
    IAudioClient* m_pAudioClient = nullptr;
    IAudioRenderClient* m_pRenderDataClient = nullptr;
    HANDLE m_hRenderEvent = nullptr;
    WAVEFORMATEX* m_pWfx = nullptr;

    UINT32 m_totalBufferFrames = 0;
    uint32_t m_bufferPeriodFrames = 0;

    std::vector<float> m_scratchStereo;
};
