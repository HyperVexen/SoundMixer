#include "AudioMixer.h"
#include <cstring>
#include <algorithm>

AudioMixer::AudioMixer()
    : m_limiter(0.85f, 0.99f),
      m_monitorRingBuffer(AudioConfig::RING_BUFFER_CAPACITY) {
    std::fill(std::begin(m_tempSourceBuffer), std::end(m_tempSourceBuffer), 0.0f);
}

AudioMixer::~AudioMixer() {
    clearSources();
}

bool AudioMixer::addSource(std::shared_ptr<AudioSource> source) {
    if (!source) return false;

    std::lock_guard<std::mutex> lock(m_sourceMutex);

    // Check if source with same ID already exists
    for (size_t i = 0; i < MAX_SOURCES; ++i) {
        if (m_slots[i].active.load(std::memory_order_relaxed) &&
            m_slots[i].source &&
            m_slots[i].source->getId() == source->getId()) {
            return false;
        }
    }

    // Find empty slot
    for (size_t i = 0; i < MAX_SOURCES; ++i) {
        if (!m_slots[i].active.load(std::memory_order_relaxed)) {
            m_slots[i].source = source;
            m_slots[i].active.store(true, std::memory_order_release);
            return true;
        }
    }

    return false; // No empty slot
}

bool AudioMixer::removeSource(const std::string& sourceId) {
    std::lock_guard<std::mutex> lock(m_sourceMutex);

    for (size_t i = 0; i < MAX_SOURCES; ++i) {
        if (m_slots[i].active.load(std::memory_order_relaxed) &&
            m_slots[i].source &&
            m_slots[i].source->getId() == sourceId) {
            
            m_slots[i].active.store(false, std::memory_order_release);
            // Delay releasing the shared_ptr until audio thread sees active=false
            auto srcToStop = m_slots[i].source;
            m_slots[i].source.reset();
            if (srcToStop) {
                srcToStop->stop();
            }
            return true;
        }
    }
    return false;
}

void AudioMixer::clearSources() {
    std::lock_guard<std::mutex> lock(m_sourceMutex);

    for (size_t i = 0; i < MAX_SOURCES; ++i) {
        if (m_slots[i].active.load(std::memory_order_relaxed)) {
            m_slots[i].active.store(false, std::memory_order_release);
            auto src = m_slots[i].source;
            m_slots[i].source.reset();
            if (src) {
                src->stop();
            }
        }
    }
}

std::vector<std::shared_ptr<AudioSource>> AudioMixer::getSources() {
    std::lock_guard<std::mutex> lock(m_sourceMutex);
    std::vector<std::shared_ptr<AudioSource>> list;
    for (size_t i = 0; i < MAX_SOURCES; ++i) {
        if (m_slots[i].active.load(std::memory_order_acquire) && m_slots[i].source) {
            list.push_back(m_slots[i].source);
        }
    }
    return list;
}

void AudioMixer::processAudio(float* outMixBuffer, size_t frameCount) {
    if (!outMixBuffer || frameCount == 0) return;

    // Zero out the mix buffer
    std::memset(outMixBuffer, 0, frameCount * 2 * sizeof(float));

    size_t clampedFrames = (std::min)(frameCount, sizeof(m_tempSourceBuffer) / (2 * sizeof(float)));

    // Iterate through slots without holding any lock (Audio Thread Rule)
    for (size_t s = 0; s < MAX_SOURCES; ++s) {
        if (!m_slots[s].active.load(std::memory_order_acquire)) {
            continue;
        }

        auto& source = m_slots[s].source;
        if (!source) continue;

        RingBuffer& ring = source->getRingBuffer();
        size_t available = ring.getAvailableRead();

        // Drift correction:
        // Adjust resampler pitch factor based on ring buffer fill level
        constexpr size_t targetFill = AudioConfig::TARGET_RING_FILL;
        if (available > targetFill + 128) {
            source->getResampler().setDriftFactor(1.0004); // Speed up slightly
        } else if (available < targetFill - 64 && available > 0) {
            source->getResampler().setDriftFactor(0.9996); // Slow down slightly
        } else {
            source->getResampler().setDriftFactor(1.0);
        }

        // Pull audio frames
        size_t readFrames = ring.read(m_tempSourceBuffer, clampedFrames);
        if (readFrames < clampedFrames) {
            // Buffer underrun on this source
            std::memset(&m_tempSourceBuffer[readFrames * 2], 0, (clampedFrames - readFrames) * 2 * sizeof(float));
            source->incrementGlitchCount();
            m_totalGlitches.fetch_add(1, std::memory_order_relaxed);
        }

        // Determine target volume (0.0 if muted)
        float targetVol = source->isMuted() ? 0.0f : source->getVolume();
        float startGain = source->getSmoothedGain();
        float endGain = targetVol;
        float gainStep = (endGain - startGain) / static_cast<float>(clampedFrames);

        // Sum into master mix with smooth de-zippering
        if (startGain > 1e-4f || endGain > 1e-4f) {
            float g = startGain;
            for (size_t i = 0; i < clampedFrames; ++i) {
                g += gainStep;
                outMixBuffer[i * 2 + 0] += m_tempSourceBuffer[i * 2 + 0] * g;
                outMixBuffer[i * 2 + 1] += m_tempSourceBuffer[i * 2 + 1] * g;
            }
        }
        source->setSmoothedGain(endGain);
    }

    // Mix soundboard sound effects into master bus
    m_soundboard.processAudio(outMixBuffer, clampedFrames);

    // Apply master volume with smooth de-zippering
    float targetMasterVol = m_masterMuted.load(std::memory_order_relaxed) ? 0.0f : m_masterVolume.load(std::memory_order_relaxed);
    float startMasterGain = m_smoothedMasterGain;
    float endMasterGain = targetMasterVol;
    float masterGainStep = (endMasterGain - startMasterGain) / static_cast<float>(frameCount);

    if (startMasterGain > 1e-4f || endMasterGain > 1e-4f) {
        float g = startMasterGain;
        for (size_t i = 0; i < frameCount; ++i) {
            g += masterGainStep;
            outMixBuffer[i * 2 + 0] *= g;
            outMixBuffer[i * 2 + 1] *= g;
        }
    } else {
        std::memset(outMixBuffer, 0, frameCount * 2 * sizeof(float));
    }
    m_smoothedMasterGain = endMasterGain;

    // Apply transparent soft limiter to prevent clipping
    m_limiter.process(outMixBuffer, frameCount);

    // Compute master metering
    float pL, pR, rL, rR;
    ComputeStereoLevels(outMixBuffer, frameCount, pL, pR, rL, rR);
    m_masterPeakL.store(pL, std::memory_order_relaxed);
    m_masterPeakR.store(pR, std::memory_order_relaxed);
    m_masterRmsL.store(rL, std::memory_order_relaxed);
    m_masterRmsR.store(rR, std::memory_order_relaxed);

    // If monitor is enabled, write to monitor ring buffer
    if (m_monitorEnabled.load(std::memory_order_relaxed)) {
        m_monitorRingBuffer.write(outMixBuffer, frameCount);
    }
}
