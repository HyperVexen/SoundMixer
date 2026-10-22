#pragma once

#include <vector>
#include <atomic>
#include <cstdint>
#include <string>

enum class VoicePreset {
    Clean = 0,
    DeepDemon,
    Chipmunk,
    Robot,
    Radio,
    CaveReverb,
    Alien
};

class VoiceProcessor {
public:
    VoiceProcessor();
    ~VoiceProcessor() = default;

    // Real-time audio processing (Stereo interleaved 48 kHz, zero locks, zero allocs)
    void process(float* stereoFrames, size_t frameCount);

    // Master enable / bypass
    void setEnabled(bool enabled) { m_enabled.store(enabled, std::memory_order_relaxed); }
    bool isEnabled() const { return m_enabled.load(std::memory_order_relaxed); }

    // Preset selection
    void setPreset(VoicePreset preset);
    VoicePreset getPreset() const { return m_preset.load(std::memory_order_relaxed); }
    static const char* getPresetName(VoicePreset preset);

    // Fine parameter controls (safe from UI thread)
    void setPitchSemitones(float semitones) { m_pitchSemitones.store(semitones, std::memory_order_relaxed); }
    float getPitchSemitones() const { return m_pitchSemitones.load(std::memory_order_relaxed); }

    void setRobotEnabled(bool enabled) { m_robotEnabled.store(enabled, std::memory_order_relaxed); }
    bool isRobotEnabled() const { return m_robotEnabled.load(std::memory_order_relaxed); }

    void setRobotFreq(float freq) { m_robotFreq.store(freq, std::memory_order_relaxed); }
    float getRobotFreq() const { return m_robotFreq.load(std::memory_order_relaxed); }

    void setRadioEnabled(bool enabled) { m_radioEnabled.store(enabled, std::memory_order_relaxed); }
    bool isRadioEnabled() const { return m_radioEnabled.load(std::memory_order_relaxed); }

    void setReverbWet(float wet) { m_reverbWet.store(wet, std::memory_order_relaxed); }
    float getReverbWet() const { return m_reverbWet.load(std::memory_order_relaxed); }

    void setNoiseGateEnabled(bool enabled) { m_gateEnabled.store(enabled, std::memory_order_relaxed); }
    bool isNoiseGateEnabled() const { return m_gateEnabled.load(std::memory_order_relaxed); }

    void setNoiseGateThresholdDb(float db) { m_gateThresholdDb.store(db, std::memory_order_relaxed); }
    float getNoiseGateThresholdDb() const { return m_gateThresholdDb.load(std::memory_order_relaxed); }

    bool isGateOpen() const { return m_gateOpen.load(std::memory_order_relaxed); }

    // Sidetone / Voice Monitoring
    void setMonitoringEnabled(bool enabled) { m_monitoringEnabled.store(enabled, std::memory_order_relaxed); }
    bool isMonitoringEnabled() const { return m_monitoringEnabled.load(std::memory_order_relaxed); }

private:
    std::atomic<bool> m_enabled{false};
    std::atomic<VoicePreset> m_preset{VoicePreset::Clean};

    std::atomic<float> m_pitchSemitones{0.0f};
    std::atomic<bool> m_robotEnabled{false};
    std::atomic<float> m_robotFreq{120.0f};
    std::atomic<bool> m_radioEnabled{false};
    std::atomic<float> m_reverbWet{0.0f};

    std::atomic<bool> m_gateEnabled{true};
    std::atomic<float> m_gateThresholdDb{-48.0f};
    std::atomic<bool> m_gateOpen{true};

    std::atomic<bool> m_monitoringEnabled{false};

    // Low-Latency Granular Pitch Shifter Circular Buffer (Window ~512 samples = ~10.6ms)
    static constexpr size_t PITCH_BUF_SIZE = 4096;
    float m_pitchBufL[PITCH_BUF_SIZE] = { 0 };
    float m_pitchBufR[PITCH_BUF_SIZE] = { 0 };
    size_t m_pitchWriteIdx = 0;
    float m_pitchReadPos1 = 0.0f;
    float m_pitchReadPos2 = 0.0f;

    // Robotizer Phase
    double m_robotPhase = 0.0;

    // Biquad Bandpass Filter for Radio / Megaphone (Direct Form II)
    struct Biquad {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        float z1_L = 0.0f, z2_L = 0.0f;
        float z1_R = 0.0f, z2_R = 0.0f;

        void configureBandpass(float centerFreq, float Q, float sampleRate);
        inline void process(float inL, float inR, float& outL, float& outR) {
            float outL_val = b0 * inL + z1_L;
            z1_L = b1 * inL - a1 * outL_val + z2_L;
            z2_L = b2 * inL - a2 * outL_val;
            outL = outL_val;

            float outR_val = b0 * inR + z1_R;
            z1_R = b1 * inR - a1 * outR_val + z2_R;
            z2_R = b2 * inR - a2 * outR_val;
            outR = outR_val;
        }
    } m_radioFilter;

    // Low-Latency Schroeder Reverb (4 comb filters + 2 allpass diffusers)
    struct CombFilter {
        std::vector<float> buffer;
        size_t idx = 0;
        float feedback = 0.78f;

        void init(size_t size, float fb) {
            buffer.assign(size, 0.0f);
            idx = 0;
            feedback = fb;
        }
        inline float process(float input) {
            if (buffer.empty()) return input;
            float output = buffer[idx];
            buffer[idx] = input + output * feedback;
            if (++idx >= buffer.size()) idx = 0;
            return output;
        }
    };

    struct AllPassFilter {
        std::vector<float> buffer;
        size_t idx = 0;
        float feedback = 0.5f;

        void init(size_t size, float fb = 0.5f) {
            buffer.assign(size, 0.0f);
            idx = 0;
            feedback = fb;
        }
        inline float process(float input) {
            if (buffer.empty()) return input;
            float bufOut = buffer[idx];
            float output = -input + bufOut;
            buffer[idx] = input + (bufOut * feedback);
            if (++idx >= buffer.size()) idx = 0;
            return output;
        }
    };

    CombFilter m_combsL[4];
    CombFilter m_combsR[4];
    AllPassFilter m_allpassL[2];
    AllPassFilter m_allpassR[2];

    // Noise Gate Envelope
    float m_gateGain = 1.0f;
};
