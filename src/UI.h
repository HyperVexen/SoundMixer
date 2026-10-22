#pragma once

#include <string>
#include <vector>
#include <memory>
#include "AudioEngine.h"
#include "ProcessEnumerator.h"
#include "VirtualDeviceBridge.h"

class SoundMixerUI {
public:
    SoundMixerUI(AudioEngine& engine);
    ~SoundMixerUI();

    void applyDarkTheme();
    void render();

    bool shouldMinimizeToTray() const { return m_minimizeToTrayRequested; }
    void clearMinimizeToTrayRequest() { m_minimizeToTrayRequested = false; }

private:
    void renderHeader();
    void renderSourceList();
    void renderOutputsList();
    void renderMasterSection();
    void renderSoundboardTab();
    void renderVoiceFxTab();
    void renderAddSourceModal();
    void renderAddOutputModal();
    void renderHotkeyModal();
    void renderPanicHotkeyModal();
    void openFileAndAddSound();
    void renderVUMeter(const char* label, float peakL, float peakR, float rmsL, float rmsR, float width = 110.0f, float height = 16.0f);
    bool renderVolumeFader(const char* str_id, float* pVolume, float width = 140.0f, float height = 22.0f);

    AudioEngine& m_engine;
    VirtualDeviceBridge m_bridge;

    int m_activeTab = 0; // 0 = Mixer, 1 = Soundboard & Macros, 2 = Voice FX

    bool m_showAddSourceModal = false;
    bool m_showAddOutputModal = false;
    bool m_showHotkeyModal = false;
    bool m_showPanicHotkeyModal = false;
    bool m_showBridgeHelpModal = false;

    // Hotkey recording state
    std::string m_hotkeyTargetSfxId;
    std::string m_hotkeyTargetName;
    uint32_t m_recordedModifiers = 0;
    uint32_t m_recordedVk = 0;
    std::string m_recordedText;

    int m_addSourceTab = 0;
    std::string m_appSearchFilter;
    std::vector<AppProcessInfo> m_cachedApps;
    bool m_appsNeedRefresh = true;

    bool m_minimizeToTrayRequested = false;
    std::string m_statusMessage;
    float m_statusTimer = 0.0f;
};
