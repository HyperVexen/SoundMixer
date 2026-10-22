#include "VoiceProcessor.h"
#include <cmath>
#include <algorithm>
#include <cstring>

constexpr double PI = 3.14159265358979323846;

void VoiceProcessor::Biquad::configureBandpass(float centerFreq, float Q, float sampleRate) {
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

    z1_L = z2_L = z1_R = z2_R = 0.0f;
}

VoiceProcessor::VoiceProcessor() {
    // Initialize Radio Megaphone bandpass filter: center 1300 Hz, Q 1.1
    m_radioFilter.configureBandpass(1300.0f, 1.1f, 48000.0f);

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
        case VoicePreset::Clean:      return "Clean (Bypass)";
        case VoicePreset::DeepDemon:  return "Deep Demon / Monster";
        case VoicePreset::Chipmunk:   return "Chipmunk / Helium";
        case VoicePreset::Robot:      return "Robot / Dalek";
        case VoicePreset::Radio:      return "Radio / Walkie-Talkie";
        case VoicePreset::CaveReverb: return "Cave / Cathedral";
        case VoicePreset::Alien:      return "Alien Transmission";
        default:                      return "Custom";
    }
}

void VoiceProcessor::setPreset(VoicePreset preset) {
    m_preset.store(preset, std::memory_order_relaxed);

    switch (preset) {
        case VoicePreset::Clean:
            m_enabled.store(false, std::memory_order_relaxed);
            m_pitchSemitones.store(0.0f, std::memory_order_relaxed);
            m_robotEnabled.store(false, std::memory_order_relaxed);
            m_radioEnabled.store(false, std::memory_order_relaxed);
            m_reverbWet.store(0.0f, std::memory_order_relaxed);
            break;

        case VoicePreset::DeepDemon:
            m_enabled.store(true, std::memory_order_relaxed);
            m_pitchSemitones.store(-7.0f, std::memory_order_relaxed);
            m_robotEnabled.store(false, std::memory_order_relaxed);
            m_radioEnabled.store(false, std::memory_order_relaxed);
            m_reverbWet.store(0.18f, std::memory_order_relaxed);
            break;

        case VoicePreset::Chipmunk:
            m_enabled.store(true, std::memory_order_relaxed);
            m_pitchSemitones.store(8.5f, std::memory_order_relaxed);
            m_robotEnabled.store(false, std::memory_order_relaxed);
            m_radioEnabled.store(false, std::memory_order_relaxed);
            m_reverbWet.store(0.05f, std::memory_order_relaxed);
            break;

        case VoicePreset::Robot:
            m_enabled.store(true, std::memory_order_relaxed);
            m_pitchSemitones.store(0.0f, std::memory_order_relaxed);
            m_robotEnabled.store(true, std::memory_order_relaxed);
            m_robotFreq.store(110.0f, std::memory_order_relaxed);
            m_radioEnabled.store(false, std::memory_order_relaxed);
            m_reverbWet.store(0.12f, std::memory_order_relaxed);
            break;

        case VoicePreset::Radio:
            m_enabled.store(true, std::memory_order_relaxed);
            m_pitchSemitones.store(0.0f, std::memory_order_relaxed);
            m_robotEnabled.store(false, std::memory_order_relaxed);
            m_radioEnabled.store(true, std::memory_order_relaxed);
            m_reverbWet.store(0.0f, std::memory_order_relaxed);
            break;

        case VoicePreset::CaveReverb:
            m_enabled.store(true, std::memory_order_relaxed);
            m_pitchSemitones.store(0.0f, std::memory_order_relaxed);
            m_robotEnabled.store(false, std::memory_order_relaxed);
            m_radioEnabled.store(false, std::memory_order_relaxed);
            m_reverbWet.store(0.55f, std::memory_order_relaxed);
            break;

        case VoicePreset::Alien:
            m_enabled.store(true, std::memory_order_relaxed);
            m_pitchSemitones.store(-3.5f, std::memory_order_relaxed);
            m_robotEnabled.store(true, std::memory_order_relaxed);
            m_robotFreq.store(85.0f, std::memory_order_relaxed);
            m_radioEnabled.store(true, std::memory_order_relaxed);
            m_reverbWet.store(0.25f, std::memory_order_relaxed);
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
        return;
    }

    // 2. Real-Time Granular Pitch Shifter (~8ms window, 0 algorithmic wait delay)
    float pitchSemi = m_pitchSemitones.load(std::memory_order_relaxed);
    if (std::fabs(pitchSemi) > 0.1f) {
        float pitchRatio = std::pow(2.0f, pitchSemi / 12.0f);
        constexpr float windowSize = 512.0f; // 10.6 ms at 48kHz
        constexpr float halfWindow = windowSize * 0.5f;
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

    // 3. Robotizer / Ring Modulator (Zero latency)
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

    // 4. Radio / Walkie-Talkie (Bandpass + asymmetric saturation, Zero latency)
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

    // 5. Low-Latency Schroeder Reverb
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
