#pragma once

#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>

// High-quality Hermite cubic resampler with dynamic pitch adjustment for clock drift compensation.
// Resamples any input rate into 48,000 Hz stereo float.
class StereoResampler {
public:
    StereoResampler() {
        reset(48000, 48000);
    }

    void reset(uint32_t inputSampleRate, uint32_t outputSampleRate = 48000) {
        m_inputRate = (inputSampleRate > 0) ? inputSampleRate : 48000;
        m_outputRate = (outputSampleRate > 0) ? outputSampleRate : 48000;
        m_baseRatio = static_cast<double>(m_inputRate) / static_cast<double>(m_outputRate);
        m_driftFactor = 1.0;
        m_phase = 0.0;

        // Reset history (4 points for cubic interpolation)
        for (int i = 0; i < 4; ++i) {
            m_histL[i] = 0.0f;
            m_histR[i] = 0.0f;
        }
    }

    // Set micro-pitch adjustment factor for clock drift compensation (e.g. 0.999 to 1.001)
    void setDriftFactor(double factor) {
        // Clamp to safe range (-2% to +2% max drift)
        m_driftFactor = (std::max)(0.98, (std::min)(1.02, factor));
    }

    double getDriftFactor() const {
        return m_driftFactor;
    }

    // Resamples input stereo frames to output buffer.
    // inFrames: pointer to interleaved stereo float samples
    // inCount: number of input stereo frames
    // outBuffer: destination buffer for interleaved stereo float samples
    // maxOutFrames: max capacity of outBuffer in stereo frames
    // Returns number of output stereo frames produced.
    size_t process(const float* inFrames, size_t inCount, float* outBuffer, size_t maxOutFrames) {
        if (!inFrames || inCount == 0 || !outBuffer || maxOutFrames == 0) {
            return 0;
        }

        double step = m_baseRatio * m_driftFactor;
        size_t outProduced = 0;

        // If ratio is practically 1.0 (exact match and no drift factor), we can do a fast copy if phase is 0
        if (std::abs(step - 1.0) < 1e-9 && m_phase < 1e-9) {
            size_t copyFrames = (std::min)(inCount, maxOutFrames);
            for (size_t i = 0; i < copyFrames * 2; ++i) {
                outBuffer[i] = inFrames[i];
            }
            // Update history with last samples
            if (copyFrames >= 4) {
                for (int i = 0; i < 4; ++i) {
                    size_t idx = copyFrames - 4 + i;
                    m_histL[i] = inFrames[idx * 2 + 0];
                    m_histR[i] = inFrames[idx * 2 + 1];
                }
            }
            return copyFrames;
        }

        // Hermite cubic spline interpolation
        while (outProduced < maxOutFrames) {
            size_t inIdx = static_cast<size_t>(m_phase);
            if (inIdx + 1 >= inCount) {
                // Exhausted input frames
                break;
            }

            double frac = m_phase - inIdx;
            float fFrac = static_cast<float>(frac);

            // Get 4 surrounding samples for Left and Right channels
            float y0_L, y1_L, y2_L, y3_L;
            float y0_R, y1_R, y2_R, y3_R;

            if (inIdx == 0) {
                y0_L = m_histL[3];
                y0_R = m_histR[3];
            } else {
                y0_L = inFrames[(inIdx - 1) * 2 + 0];
                y0_R = inFrames[(inIdx - 1) * 2 + 1];
            }

            y1_L = inFrames[inIdx * 2 + 0];
            y1_R = inFrames[inIdx * 2 + 1];

            y2_L = inFrames[(inIdx + 1) * 2 + 0];
            y2_R = inFrames[(inIdx + 1) * 2 + 1];

            if (inIdx + 2 < inCount) {
                y3_L = inFrames[(inIdx + 2) * 2 + 0];
                y3_R = inFrames[(inIdx + 2) * 2 + 1];
            } else {
                y3_L = y2_L;
                y3_R = y2_R;
            }

            outBuffer[outProduced * 2 + 0] = interpolateHermite(y0_L, y1_L, y2_L, y3_L, fFrac);
            outBuffer[outProduced * 2 + 1] = interpolateHermite(y0_R, y1_R, y2_R, y3_R, fFrac);

            outProduced++;
            m_phase += step;
        }

        // Advance phase relative to the input chunk consumed
        size_t consumed = static_cast<size_t>(m_phase);
        if (consumed > inCount) consumed = inCount;

        // Save last samples to history
        for (int i = 0; i < 4; ++i) {
            int idx = static_cast<int>(inCount) - 4 + i;
            if (idx >= 0) {
                m_histL[i] = inFrames[idx * 2 + 0];
                m_histR[i] = inFrames[idx * 2 + 1];
            }
        }

        m_phase -= consumed;
        return outProduced;
    }

private:
    static inline float interpolateHermite(float y0, float y1, float y2, float y3, float t) {
        float c0 = y1;
        float c1 = 0.5f * (y2 - y0);
        float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + c0;
    }

    uint32_t m_inputRate = 48000;
    uint32_t m_outputRate = 48000;
    double m_baseRatio = 1.0;
    double m_driftFactor = 1.0;
    double m_phase = 0.0;
    float m_histL[4] = {0};
    float m_histR[4] = {0};
};
