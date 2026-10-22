#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>
#include <string>

namespace AudioConfig {
    constexpr uint32_t SAMPLE_RATE = 48000;
    constexpr uint32_t CHANNELS = 2; // Stereo
    constexpr float DEFAULT_BUFFER_MS = 10.0f;
    constexpr size_t RING_BUFFER_CAPACITY = 32768; // ~682 ms at 48 kHz
    constexpr size_t TARGET_RING_FILL = 256;       // ~5.3 ms target buffer fill
}

struct LevelMeter {
    float peakL = 0.0f;
    float peakR = 0.0f;
    float rmsL = 0.0f;
    float rmsR = 0.0f;

    static float linearToDb(float lin) {
        if (lin <= 1e-5f) return -100.0f;
        return 20.0f * std::log10(lin);
    }
};

// Fast audio level computation for stereo float buffer
inline void ComputeStereoLevels(const float* stereoFrames, size_t frameCount, float& outPeakL, float& outPeakR, float& outRmsL, float& outRmsR) {
    if (!stereoFrames || frameCount == 0) {
        outPeakL = outPeakR = outRmsL = outRmsR = 0.0f;
        return;
    }

    float maxL = 0.0f, maxR = 0.0f;
    float sumSqL = 0.0f, sumSqR = 0.0f;

    for (size_t i = 0; i < frameCount; ++i) {
        float l = std::abs(stereoFrames[i * 2 + 0]);
        float r = std::abs(stereoFrames[i * 2 + 1]);

        if (l > maxL) maxL = l;
        if (r > maxR) maxR = r;

        sumSqL += l * l;
        sumSqR += r * r;
    }

    outPeakL = maxL;
    outPeakR = maxR;
    outRmsL = std::sqrt(sumSqL / static_cast<float>(frameCount));
    outRmsR = std::sqrt(sumSqR / static_cast<float>(frameCount));
}
