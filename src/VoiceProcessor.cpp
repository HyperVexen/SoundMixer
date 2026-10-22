#include "VoiceProcessor.h"
#include <cmath>
#include <algorithm>
#include <cstring>

constexpr double PI = 3.14159265358979323846;

void VoiceProcessor::BiquadFilter::configureBandpass(float centerFreq, float Q, float sampleRate) {
    float w0 = static_cast<float>(2.0 * PI * centerFreq / sampleRate);
    float alpha = std::sin(w0) / (2.0f * (std::max)(0.1f, Q));

    float b0_raw = alpha;
    float b1_raw = 0.0f;
    float b2_raw = -alpha;
    float a0_raw = 1.0f + alpha;
    float a1_raw = -2.0f * std::cos(w0);
    float a2_raw = 1.0f - alpha;

    b0 = b0_raw / a0_raw;
    b1 = b1_raw / a0_raw;
    b2 = b2_raw / a0_raw;
    a1 = a1_raw / a0_raw;
    a2 = a2_raw / a0_raw;
}

void VoiceProcessor::BiquadFilter::configureLowShelf(float freq, float gainDb, float sampleRate) {
    if (std::fabs(gainDb) < 0.05f) {
        b0 = 1.0f; b1 = 0.0f; b2 = 0.0f;
        a1 = 0.0f; a2 = 0.0f;
        return;
    }
    float A = std::pow(10.0f, gainDb / 40.0f);
    float w0 = static_cast<float>(2.0 * PI * freq / sampleRate);
    float cos_w0 = std::cos(w0);
    float sin_w0 = std::sin(w0);
    float alpha = sin_w0 / 2.0f * std::sqrt(2.0f); // Q = 0.707

    float a0 = (A + 1.0f) + (A - 1.0f) * cos_w0 + 2.0f * std::sqrt(A) * alpha;
    b0 = (A * ((A + 1.0f) - (A - 1.0f) * cos_w0 + 2.0f * std::sqrt(A) * alpha)) / a0;
    b1 = (2.0f * A * ((A - 1.0f) - (A + 1.0f) * cos_w0)) / a0;
    b2 = (A * ((A + 1.0f) - (A - 1.0f) * cos_w0 - 2.0f * std::sqrt(A) * alpha)) / a0;
    a1 = (-2.0f * ((A - 1.0f) + (A + 1.0f) * cos_w0)) / a0;
    a2 = ((A + 1.0f) + (A - 1.0f) * cos_w0 - 2.0f * std::sqrt(A) * alpha) / a0;
}

void VoiceProcessor::BiquadFilter::configurePeaking(float centerFreq, float gainDb, float Q, float sampleRate) {
    if (std::fabs(gainDb) < 0.05f) {
        b0 = 1.0f; b1 = 0.0f; b2 = 0.0f;
        a1 = 0.0f; a2 = 0.0f;
        return;
    }
    float A = std::pow(10.0f, gainDb / 40.0f);
    float w0 = static_cast<float>(2.0 * PI * centerFreq / sampleRate);
    float cos_w0 = std::cos(w0);
    float sin_w0 = std::sin(w0);
    float alpha = sin_w0 / (2.0f * (std::max)(0.1f, Q));

    float a0 = 1.0f + alpha / A;
    b0 = (1.0f + alpha * A) / a0;
    b1 = (-2.0f * cos_w0) / a0;
    b2 = (1.0f - alpha * A) / a0;
    a1 = (-2.0f * cos_w0) / a0;
    a2 = (1.0f - alpha / A) / a0;
}

void VoiceProcessor::BiquadFilter::configureHighShelf(float freq, float gainDb, float sampleRate) {
    if (std::fabs(gainDb) < 0.05f) {
        b0 = 1.0f; b1 = 0.0f; b2 = 0.0f;
        a1 = 0.0f; a2 = 0.0f;
        return;
    }
    float A = std::pow(10.0f, gainDb / 40.0f);
    float w0 = static_cast<float>(2.0 * PI * freq / sampleRate);
    float cos_w0 = std::cos(w0);
    float sin_w0 = std::sin(w0);
    float alpha = sin_w0 / 2.0f * std::sqrt(2.0f); // Q = 0.707

    float a0 = (A + 1.0f) - (A - 1.0f) * cos_w0 + 2.0f * std::sqrt(A) * alpha;
    b0 = (A * ((A + 1.0f) + (A - 1.0f) * cos_w0 + 2.0f * std::sqrt(A) * alpha)) / a0;
    b1 = (-2.0f * A * ((A - 1.0f) + (A + 1.0f) * cos_w0)) / a0;
    b2 = (A * ((A + 1.0f) + (A - 1.0f) * cos_w0 - 2.0f * std::sqrt(A) * alpha)) / a0;
    a1 = (2.0f * ((A - 1.0f) - (A + 1.0f) * cos_w0)) / a0;
    a2 = ((A + 1.0f) - (A - 1.0f) * cos_w0 - 2.0f * std::sqrt(A) * alpha) / a0;
}

VoiceProcessor::VoiceProcessor() {
    // Initialize Radio Megaphone bandpass filter: center 1300 Hz, Q 1.1
    m_radioFilter.configureBandpass(1300.0f, 1.1f, 48000.0f);

    // Initialize 3-Band Parametric Studio Equalizer (Flat unity gain)
    m_eqLowFilter.configureLowShelf(120.0f, 0.0f, 48000.0f);
    m_eqMidFilter.configurePeaking(2200.0f, 0.0f, 1.0f, 48000.0f);
    m_eqHighFilter.configureHighShelf(7500.0f, 0.0f, 48000.0f);

    // Initialize Formant Resonators
    m_formantF1Filter.configurePeaking(800.0f, 0.0f, 1.5f, 48000.0f);
    m_formantF2Filter.configurePeaking(2200.0f, 0.0f, 1.5f, 48000.0f);

    // Initialize Schroeder Reverb delay lines (prime lengths for smooth diffusion)
    m_combsL[0].init(1116, 0.77f);
    m_combsL[1].init(1188, 0.79f);
    m_combsL[2].init(1277, 0.75f);
    m_combsL[3].init(1356, 0.73f);
    m_allpassL[0].init(225, 0.5f);
    m_allpassL[1].init(341, 0.5f);

    m_combsR[0].init(1139, 0.77f);
    m_combsR[1].init(1211, 0.79f);
    m_combsR[2].init(1300, 0.75f);
    m_combsR[3].init(1379, 0.73f);
    m_allpassR[0].init(248, 0.5f);
    m_allpassR[1].init(364, 0.5f);

    m_pitchReadPos1 = 0.0f;
    m_pitchReadPos2 = 256.0f;
}

const char* VoiceProcessor::getPresetName(VoicePreset preset) {
    switch (preset) {
        case VoicePreset::Clean:           return "Clean (Natural)";
        case VoicePreset::DeepDemon:       return "Deep Demon";
        case VoicePreset::Chipmunk:        return "Chipmunk";
        case VoicePreset::Robot:           return "Robot / Dalek";
        case VoicePreset::Radio:           return "Walkie-Talkie";
        case VoicePreset::CaveReverb:      return "Cathedral Space";
        case VoicePreset::Alien:           return "Alien Transmission";
        case VoicePreset::StudioBroadcast: return "Studio Broadcast Mic";
        case VoicePreset::FemaleVoice:     return "Natural Female Voice";
        case VoicePreset::DeepRadioHost:   return "Deep Radio Host";
        case VoicePreset::Custom:          return "Custom Profile";
        default:                           return "Custom";
    }
}

void VoiceProcessor::setFormantSemitones(float semitones) {
    m_formantSemitones.store(semitones, std::memory_order_relaxed);
    if (std::fabs(semitones) < 0.05f) {
        m_formantF1Filter.configurePeaking(800.0f, 0.0f, 1.5f, 48000.0f);
        m_formantF2Filter.configurePeaking(2200.0f, 0.0f, 1.5f, 48000.0f);
    } else {
        float factor = std::pow(2.0f, semitones / 12.0f);
        float f1 = (std::min)(2500.0f, (std::max)(300.0f, 800.0f * factor));
        float f2 = (std::min)(5500.0f, (std::max)(900.0f, 2200.0f * factor));
        float gain = semitones > 0.0f ? 4.5f : -4.0f;
        m_formantF1Filter.configurePeaking(f1, gain, 1.6f, 48000.0f);
        m_formantF2Filter.configurePeaking(f2, -gain * 0.7f, 1.6f, 48000.0f);
    }
}

void VoiceProcessor::setEqLowGainDb(float db) {
    m_eqLowGainDb.store(db, std::memory_order_relaxed);
    m_eqLowFilter.configureLowShelf(120.0f, db, 48000.0f);
}

void VoiceProcessor::setEqMidGainDb(float db) {
    m_eqMidGainDb.store(db, std::memory_order_relaxed);
    m_eqMidFilter.configurePeaking(2200.0f, db, 1.0f, 48000.0f);
}

void VoiceProcessor::setEqHighGainDb(float db) {
    m_eqHighGainDb.store(db, std::memory_order_relaxed);
    m_eqHighFilter.configureHighShelf(7500.0f, db, 48000.0f);
}

void VoiceProcessor::applyCustomPreset(const CustomVoicePreset& preset) {
    m_preset.store(VoicePreset::Custom, std::memory_order_relaxed);
    m_enabled.store(preset.enabled, std::memory_order_relaxed);

    m_pitchSemitones.store(preset.pitchSemitones, std::memory_order_relaxed);
    setFormantSemitones(preset.formantSemitones);

    m_robotEnabled.store(preset.robotEnabled, std::memory_order_relaxed);
    m_robotFreq.store(preset.robotFreq, std::memory_order_relaxed);

    m_radioEnabled.store(preset.radioEnabled, std::memory_order_relaxed);
    m_reverbWet.store(preset.reverbWet, std::memory_order_relaxed);

    m_gateEnabled.store(preset.gateEnabled, std::memory_order_relaxed);
    m_gateThresholdDb.store(preset.gateThresholdDb, std::memory_order_relaxed);

    m_compEnabled.store(preset.compressorEnabled, std::memory_order_relaxed);
    m_compThresholdDb.store(preset.compThresholdDb, std::memory_order_relaxed);
    m_compRatio.store(preset.compRatio, std::memory_order_relaxed);
    m_compMakeupGainDb.store(preset.compMakeupDb, std::memory_order_relaxed);

    m_eqEnabled.store(preset.eqEnabled, std::memory_order_relaxed);
    setEqLowGainDb(preset.eqLowGainDb);
    setEqMidGainDb(preset.eqMidGainDb);
    setEqHighGainDb(preset.eqHighGainDb);
}

CustomVoicePreset VoiceProcessor::exportCurrentPreset(const std::string& id, const std::string& name, const std::string& desc) const {
    CustomVoicePreset p;
    p.id = id;
    p.name = name;
    p.description = desc;
    p.enabled = isEnabled();
    p.pitchSemitones = getPitchSemitones();
    p.formantSemitones = getFormantSemitones();
    p.robotEnabled = isRobotEnabled();
    p.robotFreq = getRobotFreq();
    p.radioEnabled = isRadioEnabled();
    p.reverbWet = getReverbWet();
    p.gateEnabled = isNoiseGateEnabled();
    p.gateThresholdDb = getNoiseGateThresholdDb();
    p.compressorEnabled = isCompressorEnabled();
    p.compThresholdDb = getCompressorThresholdDb();
    p.compRatio = getCompressorRatio();
    p.compMakeupDb = getCompressorMakeupGainDb();
    p.eqEnabled = isEqEnabled();
    p.eqLowGainDb = getEqLowGainDb();
    p.eqMidGainDb = getEqMidGainDb();
    p.eqHighGainDb = getEqHighGainDb();
    return p;
}

void VoiceProcessor::setPreset(VoicePreset preset) {
    m_preset.store(preset, std::memory_order_relaxed);

    switch (preset) {
        case VoicePreset::Clean:
            m_enabled.store(false, std::memory_order_relaxed);
            m_pitchSemitones.store(0.0f, std::memory_order_relaxed);
            setFormantSemitones(0.0f);
            m_robotEnabled.store(false, std::memory_order_relaxed);
            m_radioEnabled.store(false, std::memory_order_relaxed);
            m_reverbWet.store(0.0f, std::memory_order_relaxed);
            m_compEnabled.store(false, std::memory_order_relaxed);
            m_eqEnabled.store(false, std::memory_order_relaxed);
            break;

        case VoicePreset::DeepDemon:
            m_enabled.store(true, std::memory_order_relaxed);
            m_pitchSemitones.store(-7.0f, std::memory_order_relaxed);
            setFormantSemitones(-3.0f);
            m_robotEnabled.store(false, std::memory_order_relaxed);
            m_radioEnabled.store(false, std::memory_order_relaxed);
            m_reverbWet.store(0.18f, std::memory_order_relaxed);
            m_compEnabled.store(true, std::memory_order_relaxed);
            m_compThresholdDb.store(-16.0f, std::memory_order_relaxed);
            m_compRatio.store(4.0f, std::memory_order_relaxed);
            m_compMakeupGainDb.store(3.0f, std::memory_order_relaxed);
            m_eqEnabled.store(true, std::memory_order_relaxed);
            setEqLowGainDb(4.0f);
            setEqMidGainDb(0.0f);
            setEqHighGainDb(-2.0f);
            break;

        case VoicePreset::Chipmunk:
            m_enabled.store(true, std::memory_order_relaxed);
            m_pitchSemitones.store(8.5f, std::memory_order_relaxed);
            setFormantSemitones(4.0f);
            m_robotEnabled.store(false, std::memory_order_relaxed);
            m_radioEnabled.store(false, std::memory_order_relaxed);
            m_reverbWet.store(0.05f, std::memory_order_relaxed);
            m_compEnabled.store(false, std::memory_order_relaxed);
            m_eqEnabled.store(false, std::memory_order_relaxed);
            break;

        case VoicePreset::Robot:
            m_enabled.store(true, std::memory_order_relaxed);
            m_pitchSemitones.store(0.0f, std::memory_order_relaxed);
            setFormantSemitones(0.0f);
            m_robotEnabled.store(true, std::memory_order_relaxed);
            m_robotFreq.store(110.0f, std::memory_order_relaxed);
            m_radioEnabled.store(false, std::memory_order_relaxed);
            m_reverbWet.store(0.12f, std::memory_order_relaxed);
            m_compEnabled.store(false, std::memory_order_relaxed);
            m_eqEnabled.store(false, std::memory_order_relaxed);
            break;

        case VoicePreset::Radio:
            m_enabled.store(true, std::memory_order_relaxed);
            m_pitchSemitones.store(0.0f, std::memory_order_relaxed);
            setFormantSemitones(0.0f);
            m_robotEnabled.store(false, std::memory_order_relaxed);
            m_radioEnabled.store(true, std::memory_order_relaxed);
            m_reverbWet.store(0.0f, std::memory_order_relaxed);
            m_compEnabled.store(true, std::memory_order_relaxed);
            m_compThresholdDb.store(-22.0f, std::memory_order_relaxed);
            m_compRatio.store(5.0f, std::memory_order_relaxed);
            m_compMakeupGainDb.store(4.0f, std::memory_order_relaxed);
            m_eqEnabled.store(false, std::memory_order_relaxed);
            break;

        case VoicePreset::CaveReverb:
            m_enabled.store(true, std::memory_order_relaxed);
            m_pitchSemitones.store(0.0f, std::memory_order_relaxed);
            setFormantSemitones(0.0f);
            m_robotEnabled.store(false, std::memory_order_relaxed);
            m_radioEnabled.store(false, std::memory_order_relaxed);
            m_reverbWet.store(0.55f, std::memory_order_relaxed);
            m_compEnabled.store(false, std::memory_order_relaxed);
            m_eqEnabled.store(false, std::memory_order_relaxed);
            break;

        case VoicePreset::Alien:
            m_enabled.store(true, std::memory_order_relaxed);
            m_pitchSemitones.store(-3.5f, std::memory_order_relaxed);
            setFormantSemitones(2.0f);
            m_robotEnabled.store(true, std::memory_order_relaxed);
            m_robotFreq.store(85.0f, std::memory_order_relaxed);
            m_radioEnabled.store(true, std::memory_order_relaxed);
            m_reverbWet.store(0.25f, std::memory_order_relaxed);
            m_compEnabled.store(false, std::memory_order_relaxed);
            m_eqEnabled.store(false, std::memory_order_relaxed);
            break;

        case VoicePreset::StudioBroadcast:
            m_enabled.store(true, std::memory_order_relaxed);
            m_pitchSemitones.store(0.0f, std::memory_order_relaxed);
            setFormantSemitones(0.0f);
            m_robotEnabled.store(false, std::memory_order_relaxed);
            m_radioEnabled.store(false, std::memory_order_relaxed);
            m_reverbWet.store(0.0f, std::memory_order_relaxed);
            m_compEnabled.store(true, std::memory_order_relaxed);
            m_compThresholdDb.store(-18.0f, std::memory_order_relaxed);
            m_compRatio.store(3.5f, std::memory_order_relaxed);
            m_compMakeupGainDb.store(3.0f, std::memory_order_relaxed);
            m_eqEnabled.store(true, std::memory_order_relaxed);
            setEqLowGainDb(3.0f);
            setEqMidGainDb(2.0f);
            setEqHighGainDb(3.5f);
            break;

        case VoicePreset::FemaleVoice:
            m_enabled.store(true, std::memory_order_relaxed);
            m_pitchSemitones.store(4.0f, std::memory_order_relaxed);
            setFormantSemitones(2.8f);
            m_robotEnabled.store(false, std::memory_order_relaxed);
            m_radioEnabled.store(false, std::memory_order_relaxed);
            m_reverbWet.store(0.08f, std::memory_order_relaxed);
            m_compEnabled.store(true, std::memory_order_relaxed);
            m_compThresholdDb.store(-16.0f, std::memory_order_relaxed);
            m_compRatio.store(2.5f, std::memory_order_relaxed);
            m_compMakeupGainDb.store(2.0f, std::memory_order_relaxed);
            m_eqEnabled.store(true, std::memory_order_relaxed);
            setEqLowGainDb(-2.0f);
            setEqMidGainDb(1.5f);
            setEqHighGainDb(3.0f);
            break;

        case VoicePreset::DeepRadioHost:
            m_enabled.store(true, std::memory_order_relaxed);
            m_pitchSemitones.store(-3.5f, std::memory_order_relaxed);
            setFormantSemitones(-2.2f);
            m_robotEnabled.store(false, std::memory_order_relaxed);
            m_radioEnabled.store(false, std::memory_order_relaxed);
            m_reverbWet.store(0.06f, std::memory_order_relaxed);
            m_compEnabled.store(true, std::memory_order_relaxed);
            m_compThresholdDb.store(-18.0f, std::memory_order_relaxed);
            m_compRatio.store(4.0f, std::memory_order_relaxed);
            m_compMakeupGainDb.store(3.5f, std::memory_order_relaxed);
            m_eqEnabled.store(true, std::memory_order_relaxed);
            setEqLowGainDb(5.0f);
            setEqMidGainDb(2.5f);
            setEqHighGainDb(1.0f);
            break;

        case VoicePreset::Custom:
            break;
    }
}

void VoiceProcessor::process(float* stereoFrames, size_t frameCount) {
    if (!stereoFrames || frameCount == 0) return;

    // 1. Ultra-Low-Latency Noise Gate
    if (m_gateEnabled.load(std::memory_order_relaxed)) {
        float threshDb = m_gateThresholdDb.load(std::memory_order_relaxed);
        float threshLin = std::pow(10.0f, threshDb / 20.0f);

        constexpr float attackCoeff = 0.25f;   // Very fast attack ~0.5ms
        constexpr float releaseCoeff = 0.002f; // Smooth release ~40ms

        for (size_t i = 0; i < frameCount; ++i) {
            float inLevel = (std::max)(std::fabs(stereoFrames[i * 2 + 0]), std::fabs(stereoFrames[i * 2 + 1]));
            float targetGain = (inLevel >= threshLin) ? 1.0f : 0.0f;

            if (targetGain > m_gateGain) {
                m_gateGain += (targetGain - m_gateGain) * attackCoeff;
            } else {
                m_gateGain += (targetGain - m_gateGain) * releaseCoeff;
            }

            stereoFrames[i * 2 + 0] *= m_gateGain;
            stereoFrames[i * 2 + 1] *= m_gateGain;
        }

        m_gateOpen.store(m_gateGain > 0.05f, std::memory_order_relaxed);
    } else {
        m_gateOpen.store(true, std::memory_order_relaxed);
    }

    if (!m_enabled.load(std::memory_order_relaxed)) {
        m_compGainReductionDb.store(0.0f, std::memory_order_relaxed);
        return;
    }

    // 2. Formant Resonance Shifter
    float formant = m_formantSemitones.load(std::memory_order_relaxed);
    if (std::fabs(formant) > 0.05f) {
        for (size_t i = 0; i < frameCount; ++i) {
            float inL = stereoFrames[i * 2 + 0];
            float inR = stereoFrames[i * 2 + 1];

            float f1L, f1R;
            m_formantF1Filter.process(inL, inR, f1L, f1R);

            float f2L, f2R;
            m_formantF2Filter.process(f1L, f1R, f2L, f2R);

            stereoFrames[i * 2 + 0] = f2L;
            stereoFrames[i * 2 + 1] = f2R;
        }
    }

    // 3. Real-Time Granular Pitch Shifter (~8ms window, 0 algorithmic wait delay)
    float pitchSemi = m_pitchSemitones.load(std::memory_order_relaxed);
    if (std::fabs(pitchSemi) > 0.1f) {
        float pitchRatio = std::pow(2.0f, pitchSemi / 12.0f);
        constexpr float windowSize = 512.0f; // 10.6 ms at 48kHz
        float readSpeed = pitchRatio;

        for (size_t i = 0; i < frameCount; ++i) {
            float inL = stereoFrames[i * 2 + 0];
            float inR = stereoFrames[i * 2 + 1];

            // Write into circular pitch buffer
            m_pitchBufL[m_pitchWriteIdx] = inL;
            m_pitchBufR[m_pitchWriteIdx] = inR;

            // Compute delay offset relative to write head
            float d1 = m_pitchReadPos1;
            float d2 = m_pitchReadPos2;

            // Wrap within window
            while (d1 >= windowSize) d1 -= windowSize;
            while (d1 < 0.0f)       d1 += windowSize;
            while (d2 >= windowSize) d2 -= windowSize;
            while (d2 < 0.0f)       d2 += windowSize;

            // Interpolated read from circular buffer
            auto readSample = [](const float* buf, size_t writeHead, float delay) -> float {
                float pos = static_cast<float>(writeHead) - delay;
                while (pos < 0.0f) pos += static_cast<float>(PITCH_BUF_SIZE);
                size_t idx0 = static_cast<size_t>(pos) % PITCH_BUF_SIZE;
                size_t idx1 = (idx0 + 1) % PITCH_BUF_SIZE;
                float frac = pos - std::floor(pos);
                return buf[idx0] * (1.0f - frac) + buf[idx1] * frac;
            };

            float s1_L = readSample(m_pitchBufL, m_pitchWriteIdx, d1);
            float s1_R = readSample(m_pitchBufR, m_pitchWriteIdx, d1);
            float s2_L = readSample(m_pitchBufL, m_pitchWriteIdx, d2);
            float s2_R = readSample(m_pitchBufR, m_pitchWriteIdx, d2);

            // Crossfade envelopes (smooth triangular / Hann blend)
            float w1 = 0.5f * (1.0f - std::cos(static_cast<float>(2.0 * PI) * d1 / windowSize));
            float w2 = 0.5f * (1.0f - std::cos(static_cast<float>(2.0 * PI) * d2 / windowSize));

            float sumW = w1 + w2;
            if (sumW > 1e-4f) {
                stereoFrames[i * 2 + 0] = (s1_L * w1 + s2_L * w2) / sumW;
                stereoFrames[i * 2 + 1] = (s1_R * w1 + s2_R * w2) / sumW;
            }

            // Advance write and read pointers
            m_pitchWriteIdx = (m_pitchWriteIdx + 1) % PITCH_BUF_SIZE;
            m_pitchReadPos1 += (1.0f - readSpeed);
            m_pitchReadPos2 += (1.0f - readSpeed);

            if (m_pitchReadPos1 >= windowSize) m_pitchReadPos1 -= windowSize;
            if (m_pitchReadPos1 < 0.0f)        m_pitchReadPos1 += windowSize;
            if (m_pitchReadPos2 >= windowSize) m_pitchReadPos2 -= windowSize;
            if (m_pitchReadPos2 < 0.0f)        m_pitchReadPos2 += windowSize;
        }
    }

    // 4. Robotizer / Ring Modulator (Zero latency)
    if (m_robotEnabled.load(std::memory_order_relaxed)) {
        float freq = m_robotFreq.load(std::memory_order_relaxed);
        double phaseStep = (2.0 * PI * freq) / 48000.0;

        for (size_t i = 0; i < frameCount; ++i) {
            float mod = static_cast<float>(std::sin(m_robotPhase));
            stereoFrames[i * 2 + 0] *= mod;
            stereoFrames[i * 2 + 1] *= mod;

            m_robotPhase += phaseStep;
            if (m_robotPhase >= 2.0 * PI) m_robotPhase -= 2.0 * PI;
        }
    }

    // 5. Radio / Walkie-Talkie (Bandpass + asymmetric saturation, Zero latency)
    if (m_radioEnabled.load(std::memory_order_relaxed)) {
        for (size_t i = 0; i < frameCount; ++i) {
            float inL = stereoFrames[i * 2 + 0];
            float inR = stereoFrames[i * 2 + 1];

            float filL, filR;
            m_radioFilter.process(inL, inR, filL, filR);

            // Saturation drive + soft clip
            filL = std::tanh(filL * 2.8f) * 0.85f;
            filR = std::tanh(filR * 2.8f) * 0.85f;

            stereoFrames[i * 2 + 0] = filL;
            stereoFrames[i * 2 + 1] = filR;
        }
    }

    // 6. 3-Band Parametric Studio Equalizer (EQ)
    if (m_eqEnabled.load(std::memory_order_relaxed)) {
        for (size_t i = 0; i < frameCount; ++i) {
            float inL = stereoFrames[i * 2 + 0];
            float inR = stereoFrames[i * 2 + 1];

            float lowL, lowR;
            m_eqLowFilter.process(inL, inR, lowL, lowR);

            float midL, midR;
            m_eqMidFilter.process(lowL, lowR, midL, midR);

            float highL, highR;
            m_eqHighFilter.process(midL, midR, highL, highR);

            stereoFrames[i * 2 + 0] = highL;
            stereoFrames[i * 2 + 1] = highR;
        }
    }

    // 7. Studio Broadcast Vocal Compressor
    if (m_compEnabled.load(std::memory_order_relaxed)) {
        float threshDb = m_compThresholdDb.load(std::memory_order_relaxed);
        float ratio = (std::max)(1.0f, m_compRatio.load(std::memory_order_relaxed));
        float makeupDb = m_compMakeupGainDb.load(std::memory_order_relaxed);

        constexpr float compAttack = 0.005f;   // ~5ms attack
        constexpr float compRelease = 0.0003f; // ~80ms release

        float maxReduction = 0.0f;

        for (size_t i = 0; i < frameCount; ++i) {
            float inL = stereoFrames[i * 2 + 0];
            float inR = stereoFrames[i * 2 + 1];

            float peak = (std::max)(std::fabs(inL), std::fabs(inR));
            float levelDb = (peak > 1e-5f) ? (20.0f * std::log10(peak)) : -100.0f;

            float targetGainDb = 0.0f;
            if (levelDb > threshDb) {
                targetGainDb = (threshDb - levelDb) * (1.0f - 1.0f / ratio);
            }

            if (targetGainDb < m_compEnvDb) {
                m_compEnvDb += (targetGainDb - m_compEnvDb) * compAttack;
            } else {
                m_compEnvDb += (targetGainDb - m_compEnvDb) * compRelease;
            }

            float currentReduction = -m_compEnvDb;
            if (currentReduction > maxReduction) maxReduction = currentReduction;

            float linearGain = std::pow(10.0f, (m_compEnvDb + makeupDb) / 20.0f);
            stereoFrames[i * 2 + 0] = inL * linearGain;
            stereoFrames[i * 2 + 1] = inR * linearGain;
        }

        m_compGainReductionDb.store(maxReduction, std::memory_order_relaxed);
    } else {
        m_compGainReductionDb.store(0.0f, std::memory_order_relaxed);
    }

    // 8. Low-Latency Schroeder Reverb
    float wet = m_reverbWet.load(std::memory_order_relaxed);
    if (wet > 0.01f) {
        float dry = 1.0f - wet;
        for (size_t i = 0; i < frameCount; ++i) {
            float inL = stereoFrames[i * 2 + 0];
            float inR = stereoFrames[i * 2 + 1];

            // 4 parallel combs
            float cL = m_combsL[0].process(inL) + m_combsL[1].process(inL) +
                       m_combsL[2].process(inL) + m_combsL[3].process(inL);
            float cR = m_combsR[0].process(inR) + m_combsR[1].process(inR) +
                       m_combsR[2].process(inR) + m_combsR[3].process(inR);

            // 2 series allpass diffusers
            float apL = m_allpassL[1].process(m_allpassL[0].process(cL * 0.25f));
            float apR = m_allpassR[1].process(m_allpassR[0].process(cR * 0.25f));

            stereoFrames[i * 2 + 0] = inL * dry + apL * wet;
            stereoFrames[i * 2 + 1] = inR * dry + apR * wet;
        }
    }
}
