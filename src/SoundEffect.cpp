#include "SoundEffect.h"
#include <algorithm>
#include <cmath>

SoundEffect::SoundEffect(const std::string& id, const std::string& name, const std::string& filePath)
    : m_id(id), m_name(name), m_filePath(filePath) {}

void SoundEffect::play() {
    m_playCursor.store(0, std::memory_order_release);
    m_isPlaying.store(true, std::memory_order_release);
}

void SoundEffect::stop() {
    m_isPlaying.store(false, std::memory_order_release);
    m_playCursor.store(0, std::memory_order_release);
    m_smoothedGain = 0.0f;
    m_peakL.store(0.0f, std::memory_order_relaxed);
    m_peakR.store(0.0f, std::memory_order_relaxed);
}

void SoundEffect::pause() {
    m_isPlaying.store(false, std::memory_order_release);
}

void SoundEffect::resume() {
    if (m_pcmData && !m_pcmData->empty()) {
        m_isPlaying.store(true, std::memory_order_release);
    }
}

float SoundEffect::getPlaybackProgress() const {
    if (!m_pcmData || m_pcmData->empty()) return 0.0f;
    size_t totalFrames = m_pcmData->size() / 2;
    if (totalFrames == 0) return 0.0f;
    size_t cur = m_playCursor.load(std::memory_order_relaxed);
    return static_cast<float>(cur) / static_cast<float>(totalFrames);
}

float SoundEffect::getCurrentTimeSeconds() const {
    size_t cur = m_playCursor.load(std::memory_order_relaxed);
    return static_cast<float>(cur) / 48000.0f;
}

void SoundEffect::setAudioData(std::shared_ptr<std::vector<float>> data) {
    m_pcmData = data;
    if (data && !data->empty()) {
        m_durationSeconds = static_cast<float>(data->size() / 2) / 48000.0f;
    } else {
        m_durationSeconds = 0.0f;
    }
}

void SoundEffect::renderAudio(float* outMixBuffer, size_t frameCount) {
    if (!m_isPlaying.load(std::memory_order_acquire)) {
        m_peakL.store(0.0f, std::memory_order_relaxed);
        m_peakR.store(0.0f, std::memory_order_relaxed);
        return;
    }

    if (!m_pcmData || m_pcmData->empty() || !outMixBuffer || frameCount == 0) {
        return;
    }

    const float* pcm = m_pcmData->data();
    size_t totalFrames = m_pcmData->size() / 2;
    size_t cursor = m_playCursor.load(std::memory_order_relaxed);
    bool loop = m_loop.load(std::memory_order_relaxed);

    float targetGain = m_volume.load(std::memory_order_relaxed);
    float startGain = m_smoothedGain;
    float gainStep = (targetGain - startGain) / static_cast<float>(frameCount);

    float peakL = 0.0f;
    float peakR = 0.0f;

    size_t framesWritten = 0;
    while (framesWritten < frameCount && (m_isPlaying.load(std::memory_order_relaxed))) {
        size_t framesAvailable = totalFrames - cursor;
        size_t framesToCopy = (std::min)(frameCount - framesWritten, framesAvailable);

        for (size_t i = 0; i < framesToCopy; ++i) {
            float g = startGain + gainStep * static_cast<float>(framesWritten + i);
            float sL = pcm[(cursor + i) * 2 + 0] * g;
            float sR = pcm[(cursor + i) * 2 + 1] * g;

            outMixBuffer[(framesWritten + i) * 2 + 0] += sL;
            outMixBuffer[(framesWritten + i) * 2 + 1] += sR;

            float absL = std::fabs(sL);
            float absR = std::fabs(sR);
            if (absL > peakL) peakL = absL;
            if (absR > peakR) peakR = absR;
        }

        cursor += framesToCopy;
        framesWritten += framesToCopy;

        if (cursor >= totalFrames) {
            if (loop) {
                cursor = 0;
            } else {
                m_isPlaying.store(false, std::memory_order_release);
                cursor = 0;
                break;
            }
        }
    }

    m_playCursor.store(cursor, std::memory_order_release);
    m_smoothedGain = targetGain;
    m_peakL.store(peakL, std::memory_order_relaxed);
    m_peakR.store(peakR, std::memory_order_relaxed);
}
