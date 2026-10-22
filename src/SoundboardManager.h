#pragma once

#include "SoundEffect.h"
#include "AudioFileReader.h"
#include <windows.h>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <array>
#include <string>
#include <functional>

class SoundboardManager {
public:
    static constexpr size_t MAX_SOUND_EFFECTS = 64;
    static constexpr int BASE_HOTKEY_ID = 0x5000;
    static constexpr int STOP_ALL_HOTKEY_ID = 0x5FFF;
    static constexpr int VOICE_PRESET_BASE_HOTKEY_ID = 0x6000;
    static constexpr int MIC_MUTE_HOTKEY_ID = 0x6020;
    static constexpr int SIDETONE_HOTKEY_ID = 0x6021;

    struct HotkeyConfig {
        uint32_t modifiers = 0;
        uint32_t vk = 0;
        std::string text;
    };

    using VoicePresetCallback = std::function<void(int presetIndex)>;
    using ActionCallback = std::function<void()>;

    SoundboardManager();
    ~SoundboardManager();

    // Sound effect collection management (safe from UI thread)
    std::shared_ptr<SoundEffect> addSound(const std::string& filePath, const std::string& name = "");
    std::shared_ptr<SoundEffect> addBuiltIn(BuiltInSoundType type);
    bool removeSound(const std::string& id);
    void clearSounds();
    std::vector<std::shared_ptr<SoundEffect>> getSounds();
    std::shared_ptr<SoundEffect> getSound(const std::string& id);

    // Audio thread processing (Zero locks, zero allocs)
    void processAudio(float* outMixBuffer, size_t frameCount);

    // Master soundboard controls
    void setMasterVolume(float vol) { m_masterVolume.store(vol, std::memory_order_relaxed); }
    float getMasterVolume() const { return m_masterVolume.load(std::memory_order_relaxed); }
    void setMasterMuted(bool mute) { m_masterMuted.store(mute, std::memory_order_relaxed); }
    bool isMasterMuted() const { return m_masterMuted.load(std::memory_order_relaxed); }

    void stopAllSounds();
    void playSound(const std::string& id);

    // Metering for Soundboard bus
    void getMasterLevels(float& outPeakL, float& outPeakR) const {
        outPeakL = m_masterPeakL.load(std::memory_order_relaxed);
        outPeakR = m_masterPeakR.load(std::memory_order_relaxed);
    }

    // Hotkey / Macro handling (Windows RegisterHotKey)
    void registerHotkeys(HWND hwnd);
    void unregisterHotkeys(HWND hwnd);
    void reregisterHotkeys();
    bool handleHotkey(int hotkeyId);
    HWND getRegisteredHwnd() const { return m_registeredHwnd; }

    void setOnVoicePresetHotkey(VoicePresetCallback cb) { m_voicePresetCb = cb; }
    void setOnMicMuteHotkey(ActionCallback cb) { m_micMuteCb = cb; }
    void setOnSidetoneHotkey(ActionCallback cb) { m_sidetoneCb = cb; }

    void setVoicePresetHotkey(int presetIndex, uint32_t mods, uint32_t vk, const std::string& text);
    HotkeyConfig getVoicePresetHotkey(int presetIndex) const;

    void setMicMuteHotkey(uint32_t mods, uint32_t vk, const std::string& text);
    HotkeyConfig getMicMuteHotkey() const { return m_micMuteHotkey; }

    void setSidetoneHotkey(uint32_t mods, uint32_t vk, const std::string& text);
    HotkeyConfig getSidetoneHotkey() const { return m_sidetoneHotkey; }

    // Stop All hotkey
    void setStopAllHotkey(uint32_t modifiers, uint32_t vk, const std::string& text);
    uint32_t getStopAllModifiers() const { return m_stopAllModifiers; }
    uint32_t getStopAllVk() const { return m_stopAllVk; }
    const std::string& getStopAllText() const { return m_stopAllText; }
    void clearStopAllHotkey();

    static std::string formatHotkeyText(uint32_t modifiers, uint32_t vk);

private:
    struct Slot {
        std::atomic<bool> active{false};
        std::shared_ptr<SoundEffect> sound;
    };

    std::array<Slot, MAX_SOUND_EFFECTS> m_slots;
    std::mutex m_mutex;

    std::atomic<float> m_masterVolume{1.0f};
    std::atomic<bool> m_masterMuted{false};
    float m_smoothedMasterGain = 1.0f;

    std::atomic<float> m_masterPeakL{0.0f};
    std::atomic<float> m_masterPeakR{0.0f};

    HWND m_registeredHwnd = nullptr;
    uint32_t m_stopAllModifiers = MOD_CONTROL | MOD_SHIFT;
    uint32_t m_stopAllVk = 'X';
    std::string m_stopAllText = "Ctrl+Shift+X";

    VoicePresetCallback m_voicePresetCb;
    ActionCallback m_micMuteCb;
    ActionCallback m_sidetoneCb;

    std::array<HotkeyConfig, 7> m_voiceHotkeys;
    HotkeyConfig m_micMuteHotkey{MOD_CONTROL | MOD_SHIFT, 'M', "Ctrl+Shift+M"};
    HotkeyConfig m_sidetoneHotkey{MOD_CONTROL | MOD_SHIFT, 'H', "Ctrl+Shift+H"};

    // Scratch buffer for soundboard mixing to isolate its master volume
    alignas(64) float m_tempSfxBuffer[8192 * 2];
};
