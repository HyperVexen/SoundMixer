#pragma once

#include "AudioSource.h"
#include "Limiter.h"
#include "AudioCommon.h"
#include "SoundboardManager.h"
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <array>

class AudioMixer {
public:
    static constexpr size_t MAX_SOURCES = 32;

    AudioMixer();
    ~AudioMixer();

    // Source management (called from UI / main thread)
    bool addSource(std::shared_ptr<AudioSource> source);
    bool removeSource(const std::string& sourceId);
    void clearSources();
    std::vector<std::shared_ptr<AudioSource>> getSources();

    // Master controls (safe from UI thread)
    void setMasterVolume(float vol) { m_masterVolume.store(vol, std::memory_order_relaxed); }
    float getMasterVolume() const { return m_masterVolume.load(std::memory_order_relaxed); }

    void setMasterMuted(bool mute) { m_masterMuted.store(mute, std::memory_order_relaxed); }
    bool isMasterMuted() const { return m_masterMuted.load(std::memory_order_relaxed); }

    // Master metering
    void getMasterLevels(float& outPL, float& outPR, float& outRL, float& outRR) const {
        outPL = m_masterPeakL.load(std::memory_order_relaxed);
        outPR = m_masterPeakR.load(std::memory_order_relaxed);
        outRL = m_masterRmsL.load(std::memory_order_relaxed);
        outRR = m_masterRmsR.load(std::memory_order_relaxed);
    }

    bool isLimiterActive() const {
        return m_limiter.isLimiterActive();
    }

    uint32_t getTotalGlitches() const {
        return m_totalGlitches.load(std::memory_order_relaxed);
    }

    // Real-time audio rendering call (AUDIO THREAD ONLY: no locks, no allocs)
    // Fills outMixBuffer with frameCount interleaved stereo float samples (48kHz)
    void processAudio(float* outMixBuffer, size_t frameCount);

    // Monitor output ring buffer
    void setMonitorEnabled(bool enabled) { m_monitorEnabled.store(enabled, std::memory_order_relaxed); }
    bool isMonitorEnabled() const { return m_monitorEnabled.load(std::memory_order_relaxed); }
    RingBuffer& getMonitorRingBuffer() { return m_monitorRingBuffer; }

    SoundboardManager& getSoundboard() { return m_soundboard; }

private:
    struct SourceSlot {
        std::atomic<bool> active{false};
        std::shared_ptr<AudioSource> source;
    };

    std::array<SourceSlot, MAX_SOURCES> m_slots;
    std::mutex m_sourceMutex; // Only acquired when adding/removing sources, NOT in audio callback

    // Master volume & mute
    std::atomic<float> m_masterVolume{1.0f};
    std::atomic<bool> m_masterMuted{false};
    float m_smoothedMasterGain = 1.0f;

    // Master meters
    std::atomic<float> m_masterPeakL{0.0f};
    std::atomic<float> m_masterPeakR{0.0f};
    std::atomic<float> m_masterRmsL{0.0f};
    std::atomic<float> m_masterRmsR{0.0f};
    std::atomic<uint32_t> m_totalGlitches{0};

    SoftLimiter m_limiter;

    SoundboardManager m_soundboard;

    // Monitor stream
    std::atomic<bool> m_monitorEnabled{false};
    RingBuffer m_monitorRingBuffer;

    // Pre-allocated scratch buffers for audio thread to avoid allocations
    alignas(64) float m_tempSourceBuffer[8192 * 2];
};
