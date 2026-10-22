#pragma once

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <cstdint>

class SoundEffect {
public:
    SoundEffect(const std::string& id, const std::string& name, const std::string& filePath);
    ~SoundEffect() = default;

    const std::string& getId() const { return m_id; }
    const std::string& getName() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }
    const std::string& getFilePath() const { return m_filePath; }

    float getVolume() const { return m_volume.load(std::memory_order_relaxed); }
    void setVolume(float v) { m_volume.store(v, std::memory_order_relaxed); }

    bool isLooping() const { return m_loop.load(std::memory_order_relaxed); }
    void setLooping(bool loop) { m_loop.store(loop, std::memory_order_relaxed); }

    // Macro Hotkey
    uint32_t getHotkeyModifiers() const { return m_hotkeyModifiers; }
    uint32_t getHotkeyVk() const { return m_hotkeyVk; }
    const std::string& getHotkeyText() const { return m_hotkeyText; }
    void setHotkey(uint32_t modifiers, uint32_t vk, const std::string& text) {
        m_hotkeyModifiers = modifiers;
        m_hotkeyVk = vk;
        m_hotkeyText = text;
    }
    void clearHotkey() {
        m_hotkeyModifiers = 0;
        m_hotkeyVk = 0;
        m_hotkeyText.clear();
    }
    bool hasHotkey() const { return m_hotkeyVk != 0; }

    // Playback control
    void play();
    void stop();
    void pause();
    void resume();
    bool isPlaying() const { return m_isPlaying.load(std::memory_order_acquire); }

    float getPlaybackProgress() const;
    float getCurrentTimeSeconds() const;
    float getDurationSeconds() const { return m_durationSeconds; }
    bool isLoaded() const { return m_pcmData && !m_pcmData->empty(); }

    void setAudioData(std::shared_ptr<std::vector<float>> data);

    // Audio thread rendering (Zero locks, zero allocs)
    void renderAudio(float* outMixBuffer, size_t frameCount);

    void getLevels(float& outPeakL, float& outPeakR) const {
        outPeakL = m_peakL.load(std::memory_order_relaxed);
        outPeakR = m_peakR.load(std::memory_order_relaxed);
    }

private:
    std::string m_id;
    std::string m_name;
    std::string m_filePath;

    std::atomic<float> m_volume{1.0f};
    std::atomic<bool> m_loop{false};

    uint32_t m_hotkeyModifiers = 0;
    uint32_t m_hotkeyVk = 0;
    std::string m_hotkeyText;

    std::shared_ptr<std::vector<float>> m_pcmData; // 48kHz stereo float
    float m_durationSeconds = 0.0f;

    std::atomic<bool> m_isPlaying{false};
    std::atomic<size_t> m_playCursor{0};
    float m_smoothedGain{1.0f};

    std::atomic<float> m_peakL{0.0f};
    std::atomic<float> m_peakR{0.0f};
};
