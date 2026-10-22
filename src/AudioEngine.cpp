#include "AudioEngine.h"
#include <iostream>

AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine() {
    shutdown();
}

bool AudioEngine::initialize() {
    if (!m_deviceManager.initialize()) {
        return false;
    }

    // Default configuration:
    // 1. Primary output: Physical headphones / speakers (for lowest-jitter hardware clock and local listening)
    auto renderDevs = m_deviceManager.getRenderDevices();
    std::string primaryId, primaryName;
    for (const auto& dev : renderDevs) {
        if (!dev.isVBCable) {
            primaryId = dev.idUtf8;
            primaryName = dev.nameUtf8;
            break;
        }
    }
    if (primaryId.empty()) {
        auto defDev = m_deviceManager.getDefaultRenderDevice();
        primaryId = defDev.idUtf8;
        primaryName = defDev.nameUtf8;
    }

    if (!primaryId.empty()) {
        addOutput(primaryId, primaryName, 1.0f, false);
    }

    // 2. Secondary output: VB-Cable (for Valorant / Discord virtual microphone)
    auto vbCable = m_deviceManager.getVBCableRenderDevice();
    if (!vbCable.idUtf8.empty() && vbCable.idUtf8 != primaryId) {
        addOutput(vbCable.idUtf8, vbCable.nameUtf8, 1.0f, false);
    }

    return true;
}

void AudioEngine::shutdown() {
    stop();
    m_mixer.clearSources();
    clearOutputs();
    m_deviceManager.shutdown();
}

void AudioEngine::updatePrimaryAssignment() {
    // Ensure the first active output is designated as Primary
    m_secondaryOutputsSnapshot.clear();

    bool hasPrimary = false;
    for (size_t i = 0; i < m_outputs.size(); ++i) {
        if (!hasPrimary) {
            m_outputs[i]->setPrimary(true);
            m_outputs[i]->setAudioCallback([this](float* buf, UINT32 frames) {
                onPrimaryNeedAudio(buf, frames);
            });
            hasPrimary = true;
        } else {
            m_outputs[i]->setPrimary(false);
            m_outputs[i]->setAudioCallback(nullptr);
            m_secondaryOutputsSnapshot.push_back(m_outputs[i]);
        }
    }
}

void AudioEngine::onPrimaryNeedAudio(float* outStereoBuffer, UINT32 frameCount) {
    size_t clamped = (std::min)(static_cast<size_t>(frameCount), sizeof(m_broadcastBuffer) / (2 * sizeof(float)));

    // 1. Dual-bus real-time mixing:
    // outStereoBuffer = Primary Headphone mix (Apps + Soundboard + Sidetone Mic if enabled)
    // m_broadcastBuffer = Broadcast Virtual Mic mix (Mic with Voice FX + Soundboard)
    m_mixer.processAudioDual(outStereoBuffer, m_broadcastBuffer, clamped);

    // 2. Route to secondary outputs based on their selected route mode
    for (const auto& sec : m_secondaryOutputsSnapshot) {
        if (sec && sec->isRunning()) {
            if (sec->getRouteMode() == OutputRouteMode::VirtualMicStream) {
                sec->pushFrames(m_broadcastBuffer, clamped);
            } else {
                sec->pushFrames(outStereoBuffer, clamped);
            }
        }
    }
}

bool AudioEngine::addOutput(const std::string& deviceId, const std::string& name, float volume, bool muted) {
    std::lock_guard<std::mutex> lock(m_outputsMutex);

    // Check if output already exists
    for (const auto& out : m_outputs) {
        if (out->getId() == deviceId) {
            return false;
        }
    }

    bool isPrimary = m_outputs.empty();
    auto newOutput = std::make_shared<AudioOutput>(deviceId, name, isPrimary);
    newOutput->setVolume(volume);
    newOutput->setMuted(muted);

    if (m_running.load()) {
        newOutput->start();
    }

    m_outputs.push_back(newOutput);
    updatePrimaryAssignment();
    return true;
}

bool AudioEngine::removeOutput(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(m_outputsMutex);

    for (auto it = m_outputs.begin(); it != m_outputs.end(); ++it) {
        if ((*it)->getId() == deviceId) {
            (*it)->stop();
            m_outputs.erase(it);
            updatePrimaryAssignment();
            return true;
        }
    }
    return false;
}

void AudioEngine::clearOutputs() {
    std::lock_guard<std::mutex> lock(m_outputsMutex);
    for (auto& out : m_outputs) {
        out->stop();
    }
    m_outputs.clear();
    m_secondaryOutputsSnapshot.clear();
}

std::vector<std::shared_ptr<AudioOutput>> AudioEngine::getOutputs() {
    std::lock_guard<std::mutex> lock(m_outputsMutex);
    return m_outputs;
}

std::shared_ptr<AudioOutput> AudioEngine::getPrimaryOutput() {
    std::lock_guard<std::mutex> lock(m_outputsMutex);
    for (const auto& out : m_outputs) {
        if (out->isPrimary()) return out;
    }
    if (!m_outputs.empty()) return m_outputs[0];
    return nullptr;
}

void AudioEngine::setPrimaryOutput(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(m_outputsMutex);
    // Find output and move to front
    for (auto it = m_outputs.begin(); it != m_outputs.end(); ++it) {
        if ((*it)->getId() == deviceId) {
            auto primary = *it;
            m_outputs.erase(it);
            m_outputs.insert(m_outputs.begin(), primary);
            break;
        }
    }
    updatePrimaryAssignment();
}

bool AudioEngine::start() {
    if (m_running.load()) return true;

    std::lock_guard<std::mutex> lock(m_outputsMutex);
    updatePrimaryAssignment();

    for (auto& out : m_outputs) {
        out->start();
    }

    m_running.store(true);
    return true;
}

void AudioEngine::stop() {
    std::lock_guard<std::mutex> lock(m_outputsMutex);
    for (auto& out : m_outputs) {
        out->stop();
    }
    m_running.store(false);
}
