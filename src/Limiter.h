#pragma once

#include <cmath>
#include <algorithm>
#include <atomic>

// Transparent soft limiter / saturation to avoid digital harsh clipping
class SoftLimiter {
public:
    explicit SoftLimiter(float threshold = 0.85f, float ceiling = 0.99f)
        : m_threshold(threshold), m_ceiling(ceiling) {}

    // Process interleaved stereo buffer in-place
    void process(float* stereoFrames, size_t frameCount) {
        if (!stereoFrames || frameCount == 0) return;

        bool active = false;
        for (size_t i = 0; i < frameCount * 2; ++i) {
            float x = stereoFrames[i];
            float absX = std::abs(x);

            if (absX > m_threshold) {
                active = true;
                float sign = (x >= 0.0f) ? 1.0f : -1.0f;
                float excess = absX - m_threshold;
                float headroom = m_ceiling - m_threshold;
                float compressed = m_threshold + headroom * std::tanh(excess / (headroom * 1.2f));
                if (compressed > m_ceiling) compressed = m_ceiling;
                stereoFrames[i] = sign * compressed;
            }
        }

        m_isActive.store(active, std::memory_order_relaxed);
    }

    bool isLimiterActive() const {
        return m_isActive.load(std::memory_order_relaxed);
    }

private:
    float m_threshold;
    float m_ceiling;
    std::atomic<bool> m_isActive{false};
};
