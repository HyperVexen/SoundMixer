#pragma once

#include <string>
#include <atomic>
#include <memory>
#include <windows.h>
#include "RingBuffer.h"
#include "Resampler.h"
#include "AudioCommon.h"

enum class SourceType {
    Microphone,
    Application
};

class AudioSource {
public:
    AudioSource(SourceType type, const std::string& id, const std::string& name)
        : m_type(type), m_id(id), m_displayName(name),
          m_ringBuffer(AudioConfig::RING_BUFFER_CAPACITY) {}

    virtual ~AudioSource() = default;

    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
    virtual std::string getStatusText() const = 0;

    SourceType getType() const { return m_type; }
    const std::string& getId() const { return m_id; }
    const std::string& getName() const { return m_displayName; }
    void setName(const std::string& name) { m_displayName = name; }

    // Volume & Mute control (safe from UI thread)
    void setVolume(float vol) { m_volume.store(vol, std::memory_order_relaxed); }
    float getVolume() const { return m_volume.load(std::memory_order_relaxed); }

    float getSmoothedGain() const { return m_smoothedGain; }
    void setSmoothedGain(float g) { m_smoothedGain = g; }

    void setMuted(bool mute) { m_muted.store(mute, std::memory_order_relaxed); }
    bool isMuted() const { return m_muted.load(std::memory_order_relaxed); }

    // Metering (updated by mixer or capture thread)
    void setLevels(float pL, float pR, float rL, float rR) {
        m_peakL.store(pL, std::memory_order_relaxed);
        m_peakR.store(pR, std::memory_order_relaxed);
        m_rmsL.store(rL, std::memory_order_relaxed);
        m_rmsR.store(rR, std::memory_order_relaxed);
    }

    void getLevels(float& outPL, float& outPR, float& outRL, float& outRR) const {
        outPL = m_peakL.load(std::memory_order_relaxed);
        outPR = m_peakR.load(std::memory_order_relaxed);
        outRL = m_rmsL.load(std::memory_order_relaxed);
        outRR = m_rmsR.load(std::memory_order_relaxed);
    }

    RingBuffer& getRingBuffer() { return m_ringBuffer; }
    StereoResampler& getResampler() { return m_resampler; }

    uint32_t getGlitchCount() const { return m_glitchCount.load(std::memory_order_relaxed); }
    void incrementGlitchCount() { m_glitchCount.fetch_add(1, std::memory_order_relaxed); }

    // Drift statistics
    float getFillPercentage() const {
        size_t available = m_ringBuffer.getAvailableRead();
        return static_cast<float>(available) / static_cast<float>(AudioConfig::TARGET_RING_FILL * 4);
    }

protected:
    SourceType m_type;
    std::string m_id;
    std::string m_displayName;

    RingBuffer m_ringBuffer;
    StereoResampler m_resampler;

    std::atomic<float> m_volume{1.0f};
    float m_smoothedGain{1.0f};
    std::atomic<bool> m_muted{false};

    std::atomic<float> m_peakL{0.0f};
    std::atomic<float> m_peakR{0.0f};
    std::atomic<float> m_rmsL{0.0f};
    std::atomic<float> m_rmsR{0.0f};

    std::atomic<uint32_t> m_glitchCount{0};
};
