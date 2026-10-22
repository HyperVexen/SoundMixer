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
    Alien,
    StudioBroadcast,
    FemaleVoice,
    DeepRadioHost,
    Custom
};

struct CustomVoicePreset {
    std::string id;
    std::string name;
    std::string description;

    bool enabled = true;

    // Pitch & Formant
    float pitchSemitones = 0.0f;
    float formantSemitones = 0.0f;

    // Modulators
    bool robotEnabled = false;
    float robotFreq = 120.0f;

    bool radioEnabled = false;

    float reverbWet = 0.0f;

    // Dynamics
    bool gateEnabled = true;
    float gateThresholdDb = -48.0f;

    bool compressorEnabled = false;
    float compThresholdDb = -18.0f;
    float compRatio = 3.0f;
    float compMakeupDb = 3.0f;

    // Equalizer
    bool eqEnabled = false;
    float eqLowGainDb = 0.0f;
    float eqMidGainDb = 0.0f;
    float eqHighGainDb = 0.0f;

    // Macro Hotkey
    uint32_t hotkeyModifiers = 0;
    uint32_t hotkeyVk = 0;
    std::string hotkeyText;
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

    // Apply custom preset profile
    void applyCustomPreset(const CustomVoicePreset& preset);
    CustomVoicePreset exportCurrentPreset(const std::string& id, const std::string& name, const std::string& desc) const;

    // Fine parameter controls (safe from UI thread)
    void setPitchSemitones(float semitones) { m_pitchSemitones.store(semitones, std::memory_order_relaxed); }
    float getPitchSemitones() const { return m_pitchSemitones.load(std::memory_order_relaxed); }

    void setFormantSemitones(float semitones);
    float getFormantSemitones() const { return m_formantSemitones.load(std::memory_order_relaxed); }

    void setRobotEnabled(bool enabled) { m_robotEnabled.store(enabled, std::memory_order_relaxed); }
    bool isRobotEnabled() const { return m_robotEnabled.load(std::memory_order_relaxed); }

    void setRobotFreq(float freq) { m_robotFreq.store(freq, std::memory_order_relaxed); }
    float getRobotFreq() const { return m_robotFreq.load(std::memory_order_relaxed); }

    void setRadioEnabled(bool enabled) { m_radioEnabled.store(enabled, std::memory_order_relaxed); }
    bool isRadioEnabled() const { return m_radioEnabled.load(std::memory_order_relaxed); }

    void setReverbWet(float wet) { m_reverbWet.store(wet, std::memory_order_relaxed); }
    float getReverbWet() const { return m_reverbWet.load(std::memory_order_relaxed); }

    // Noise Gate
    void setNoiseGateEnabled(bool enabled) { m_gateEnabled.store(enabled, std::memory_order_relaxed); }
    bool isNoiseGateEnabled() const { return m_gateEnabled.load(std::memory_order_relaxed); }

    void setNoiseGateThresholdDb(float db) { m_gateThresholdDb.store(db, std::memory_order_relaxed); }
    float getNoiseGateThresholdDb() const { return m_gateThresholdDb.load(std::memory_order_relaxed); }

    bool isGateOpen() const { return m_gateOpen.load(std::memory_order_relaxed); }

    // Broadcast Vocal Compressor
    void setCompressorEnabled(bool enabled) { m_compEnabled.store(enabled, std::memory_order_relaxed); }
    bool isCompressorEnabled() const { return m_compEnabled.load(std::memory_order_relaxed); }

    void setCompressorThresholdDb(float db) { m_compThresholdDb.store(db, std::memory_order_relaxed); }
    float getCompressorThresholdDb() const { return m_compThresholdDb.load(std::memory_order_relaxed); }

    void setCompressorRatio(float ratio) { m_compRatio.store(ratio, std::memory_order_relaxed); }
    float getCompressorRatio() const { return m_compRatio.load(std::memory_order_relaxed); }

    void setCompressorMakeupGainDb(float db) { m_compMakeupGainDb.store(db, std::memory_order_relaxed); }
    float getCompressorMakeupGainDb() const { return m_compMakeupGainDb.load(std::memory_order_relaxed); }

    float getGainReductionDb() const { return m_compGainReductionDb.load(std::memory_order_relaxed); }

    // 3-Band Parametric Equalizer (EQ)
    void setEqEnabled(bool enabled) { m_eqEnabled.store(enabled, std::memory_order_relaxed); }
    bool isEqEnabled() const { return m_eqEnabled.load(std::memory_order_relaxed); }

    void setEqLowGainDb(float db);
    float getEqLowGainDb() const { return m_eqLowGainDb.load(std::memory_order_relaxed); }

    void setEqMidGainDb(float db);
    float getEqMidGainDb() const { return m_eqMidGainDb.load(std::memory_order_relaxed); }

    void setEqHighGainDb(float db);
    float getEqHighGainDb() const { return m_eqHighGainDb.load(std::memory_order_relaxed); }

    // Sidetone / Voice Monitoring
    void setMonitoringEnabled(bool enabled) { m_monitoringEnabled.store(enabled, std::memory_order_relaxed); }
    bool isMonitoringEnabled() const { return m_monitoringEnabled.load(std::memory_order_relaxed); }

private:
    std::atomic<bool> m_enabled{false};
    std::atomic<VoicePreset> m_preset{VoicePreset::Clean};

    std::atomic<float> m_pitchSemitones{0.0f};
    std::atomic<float> m_formantSemitones{0.0f};
    std::atomic<bool> m_robotEnabled{false};
    std::atomic<float> m_robotFreq{120.0f};
    std::atomic<bool> m_radioEnabled{false};
    std::atomic<float> m_reverbWet{0.0f};

    // Dynamics: Gate
    std::atomic<bool> m_gateEnabled{true};
    std::atomic<float> m_gateThresholdDb{-48.0f};
    std::atomic<bool> m_gateOpen{true};

    // Dynamics: Broadcast Compressor
    std::atomic<bool> m_compEnabled{false};
    std::atomic<float> m_compThresholdDb{-18.0f};
    std::atomic<float> m_compRatio{3.0f};
    std::atomic<float> m_compMakeupGainDb{3.0f};
    std::atomic<float> m_compGainReductionDb{0.0f};
    float m_compEnvDb = 0.0f;

    // 3-Band Studio EQ
    std::atomic<bool> m_eqEnabled{false};
    std::atomic<float> m_eqLowGainDb{0.0f};
    std::atomic<float> m_eqMidGainDb{0.0f};
    std::atomic<float> m_eqHighGainDb{0.0f};

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

    // Biquad Filter (Direct Form II)
    struct BiquadFilter {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        float z1_L = 0.0f, z2_L = 0.0f;
        float z1_R = 0.0f, z2_R = 0.0f;

        void configureLowShelf(float freq, float gainDb, float sampleRate = 48000.0f);
        void configurePeaking(float centerFreq, float gainDb, float Q = 1.0f, float sampleRate = 48000.0f);
        void configureHighShelf(float freq, float gainDb, float sampleRate = 48000.0f);
        void configureBandpass(float centerFreq, float Q, float sampleRate = 48000.0f);

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

        void reset() {
            z1_L = z2_L = z1_R = z2_R = 0.0f;
        }
    };

    BiquadFilter m_radioFilter;
    BiquadFilter m_eqLowFilter;
    BiquadFilter m_eqMidFilter;
    BiquadFilter m_eqHighFilter;
    BiquadFilter m_formantF1Filter;
    BiquadFilter m_formantF2Filter;

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
