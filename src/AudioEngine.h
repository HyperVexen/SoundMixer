#pragma once

#include "AudioMixer.h"
#include "AudioDeviceManager.h"
#include "AudioOutput.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    bool initialize();
    void shutdown();

    bool start();
    void stop();
    bool isRunning() const { return m_running.load(std::memory_order_relaxed); }

    // Multi-Output Management
    bool addOutput(const std::string& deviceId, const std::string& name, float volume = 1.0f, bool muted = false);
    bool removeOutput(const std::string& deviceId);
    void clearOutputs();
    std::vector<std::shared_ptr<AudioOutput>> getOutputs();
    std::shared_ptr<AudioOutput> getPrimaryOutput();
    void setPrimaryOutput(const std::string& deviceId);

    AudioMixer& getMixer() { return m_mixer; }
    SoundboardManager& getSoundboard() { return m_mixer.getSoundboard(); }
    AudioDeviceManager& getDeviceManager() { return m_deviceManager; }

private:
    void updatePrimaryAssignment();
    void onPrimaryNeedAudio(float* outStereoBuffer, UINT32 frameCount);

    AudioMixer m_mixer;
    AudioDeviceManager m_deviceManager;

    std::mutex m_outputsMutex;
    std::vector<std::shared_ptr<AudioOutput>> m_outputs;

    // Fast copy of secondary outputs for audio thread callback (no lock contention)
    std::vector<std::shared_ptr<AudioOutput>> m_secondaryOutputsSnapshot;

    alignas(64) float m_primaryBuffer[8192 * 2];
    alignas(64) float m_broadcastBuffer[8192 * 2];

    std::atomic<bool> m_running{false};
};
