#include "SoundboardManager.h"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <cmath>

SoundboardManager::SoundboardManager() {
    std::memset(m_tempSfxBuffer, 0, sizeof(m_tempSfxBuffer));

    // Default voice preset macro hotkeys (Ctrl+1 through Ctrl+7)
    for (int i = 0; i < 7; ++i) {
        m_voiceHotkeys[i].modifiers = MOD_CONTROL;
        m_voiceHotkeys[i].vk = '1' + i;
        m_voiceHotkeys[i].text = std::string("Ctrl+") + static_cast<char>('1' + i);
    }
}

SoundboardManager::~SoundboardManager() {
    if (m_registeredHwnd) {
        unregisterHotkeys(m_registeredHwnd);
    }
}

std::shared_ptr<SoundEffect> SoundboardManager::addSound(const std::string& filePath, const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Find free slot
    int freeSlot = -1;
    for (size_t i = 0; i < MAX_SOUND_EFFECTS; ++i) {
        if (!m_slots[i].active.load(std::memory_order_relaxed)) {
            freeSlot = static_cast<int>(i);
            break;
        }
    }
    if (freeSlot == -1) return nullptr;

    // Generate unique ID
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    std::string sfxId = "sfx_" + std::to_string(freeSlot) + "_" + std::to_string(now);

    // Determine default display name if none provided
    std::string displayName = name;
    if (displayName.empty()) {
        size_t lastSlash = filePath.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            displayName = filePath.substr(lastSlash + 1);
        } else {
            displayName = filePath;
        }
        // Remove extension
        size_t dot = displayName.find_last_of('.');
        if (dot != std::string::npos && dot > 0) {
            displayName = displayName.substr(0, dot);
        }
    }

    auto sfx = std::make_shared<SoundEffect>(sfxId, displayName, filePath);

    // Load audio data into RAM
    auto pcmBuffer = std::make_shared<std::vector<float>>();
    float duration = 0.0f;
    if (AudioFileReader::loadAudioFile(filePath, *pcmBuffer, duration)) {
        sfx->setAudioData(pcmBuffer);
    } else {
        // Failed to load
        return nullptr;
    }

    m_slots[freeSlot].sound = sfx;
    m_slots[freeSlot].active.store(true, std::memory_order_release);

    // Register hotkey if window is active
    if (m_registeredHwnd && sfx->hasHotkey()) {
        RegisterHotKey(m_registeredHwnd, BASE_HOTKEY_ID + freeSlot, sfx->getHotkeyModifiers() | MOD_NOREPEAT, sfx->getHotkeyVk());
    }

    return sfx;
}

std::shared_ptr<SoundEffect> SoundboardManager::addBuiltIn(BuiltInSoundType type) {
    std::string path;
    switch (type) {
        case BuiltInSoundType::Airhorn:   path = "builtin:airhorn"; break;
        case BuiltInSoundType::Fanfare:   path = "builtin:fanfare"; break;
        case BuiltInSoundType::Rimshot:   path = "builtin:rimshot"; break;
        case BuiltInSoundType::CoinChime: path = "builtin:coin"; break;
        case BuiltInSoundType::Buzzer:    path = "builtin:buzzer"; break;
    }
    return addSound(path, AudioFileReader::getBuiltInName(type));
}

bool SoundboardManager::removeSound(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (size_t i = 0; i < MAX_SOUND_EFFECTS; ++i) {
        if (m_slots[i].active.load(std::memory_order_relaxed) && m_slots[i].sound) {
            if (m_slots[i].sound->getId() == id) {
                if (m_registeredHwnd) {
                    UnregisterHotKey(m_registeredHwnd, BASE_HOTKEY_ID + static_cast<int>(i));
                }
                m_slots[i].active.store(false, std::memory_order_release);
                m_slots[i].sound->stop();
                m_slots[i].sound.reset();
                return true;
            }
        }
    }
    return false;
}

void SoundboardManager::clearSounds() {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (size_t i = 0; i < MAX_SOUND_EFFECTS; ++i) {
        if (m_slots[i].active.load(std::memory_order_relaxed)) {
            if (m_registeredHwnd) {
                UnregisterHotKey(m_registeredHwnd, BASE_HOTKEY_ID + static_cast<int>(i));
            }
            m_slots[i].active.store(false, std::memory_order_release);
            if (m_slots[i].sound) {
                m_slots[i].sound->stop();
                m_slots[i].sound.reset();
            }
        }
    }
}

std::vector<std::shared_ptr<SoundEffect>> SoundboardManager::getSounds() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::shared_ptr<SoundEffect>> list;
    for (size_t i = 0; i < MAX_SOUND_EFFECTS; ++i) {
        if (m_slots[i].active.load(std::memory_order_acquire) && m_slots[i].sound) {
            list.push_back(m_slots[i].sound);
        }
    }
    return list;
}

std::shared_ptr<SoundEffect> SoundboardManager::getSound(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (size_t i = 0; i < MAX_SOUND_EFFECTS; ++i) {
        if (m_slots[i].active.load(std::memory_order_acquire) && m_slots[i].sound) {
            if (m_slots[i].sound->getId() == id) {
                return m_slots[i].sound;
            }
        }
    }
    return nullptr;
}

void SoundboardManager::stopAllSounds() {
    for (size_t i = 0; i < MAX_SOUND_EFFECTS; ++i) {
        if (m_slots[i].active.load(std::memory_order_acquire) && m_slots[i].sound) {
            m_slots[i].sound->stop();
        }
    }
}

void SoundboardManager::playSound(const std::string& id) {
    auto sfx = getSound(id);
    if (sfx) {
        sfx->play();
    }
}

void SoundboardManager::processAudio(float* outMixBuffer, size_t frameCount) {
    if (!outMixBuffer || frameCount == 0) return;

    size_t clampedFrames = (std::min)(frameCount, sizeof(m_tempSfxBuffer) / (2 * sizeof(float)));
    std::memset(m_tempSfxBuffer, 0, clampedFrames * 2 * sizeof(float));

    bool anySoundRendered = false;
    for (size_t i = 0; i < MAX_SOUND_EFFECTS; ++i) {
        if (!m_slots[i].active.load(std::memory_order_acquire)) continue;
        auto& sound = m_slots[i].sound;
        if (!sound || !sound->isPlaying()) continue;

        sound->renderAudio(m_tempSfxBuffer, clampedFrames);
        anySoundRendered = true;
    }

    if (!anySoundRendered) {
        m_masterPeakL.store(0.0f, std::memory_order_relaxed);
        m_masterPeakR.store(0.0f, std::memory_order_relaxed);
        return;
    }

    // Apply master soundboard gain with de-zippering
    float targetGain = m_masterMuted.load(std::memory_order_relaxed) ? 0.0f : m_masterVolume.load(std::memory_order_relaxed);
    float startGain = m_smoothedMasterGain;
    float gainStep = (targetGain - startGain) / static_cast<float>(clampedFrames);

    float peakL = 0.0f, peakR = 0.0f;
    for (size_t f = 0; f < clampedFrames; ++f) {
        float g = startGain + gainStep * static_cast<float>(f);
        float sL = m_tempSfxBuffer[f * 2 + 0] * g;
        float sR = m_tempSfxBuffer[f * 2 + 1] * g;

        outMixBuffer[f * 2 + 0] += sL;
        outMixBuffer[f * 2 + 1] += sR;

        float absL = std::fabs(sL);
        float absR = std::fabs(sR);
        if (absL > peakL) peakL = absL;
        if (absR > peakR) peakR = absR;
    }

    m_smoothedMasterGain = targetGain;
    m_masterPeakL.store(peakL, std::memory_order_relaxed);
    m_masterPeakR.store(peakR, std::memory_order_relaxed);
}

void SoundboardManager::registerHotkeys(HWND hwnd) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!hwnd) return;
    m_registeredHwnd = hwnd;

    // Register sound hotkeys
    for (size_t i = 0; i < MAX_SOUND_EFFECTS; ++i) {
        if (m_slots[i].active.load(std::memory_order_relaxed) && m_slots[i].sound) {
            auto& sfx = m_slots[i].sound;
            if (sfx->hasHotkey()) {
                RegisterHotKey(hwnd, BASE_HOTKEY_ID + static_cast<int>(i),
                               sfx->getHotkeyModifiers() | MOD_NOREPEAT,
                               sfx->getHotkeyVk());
            }
        }
    }

    // Register stop all hotkey
    if (m_stopAllVk != 0) {
        RegisterHotKey(hwnd, STOP_ALL_HOTKEY_ID, m_stopAllModifiers | MOD_NOREPEAT, m_stopAllVk);
    }

    // Register voice preset macro hotkeys
    for (int i = 0; i < 7; ++i) {
        if (m_voiceHotkeys[i].vk != 0) {
            RegisterHotKey(hwnd, VOICE_PRESET_BASE_HOTKEY_ID + i,
                           m_voiceHotkeys[i].modifiers | MOD_NOREPEAT,
                           m_voiceHotkeys[i].vk);
        }
    }

    // Register mic mute hotkey
    if (m_micMuteHotkey.vk != 0) {
        RegisterHotKey(hwnd, MIC_MUTE_HOTKEY_ID,
                       m_micMuteHotkey.modifiers | MOD_NOREPEAT,
                       m_micMuteHotkey.vk);
    }

    // Register sidetone hotkey
    if (m_sidetoneHotkey.vk != 0) {
        RegisterHotKey(hwnd, SIDETONE_HOTKEY_ID,
                       m_sidetoneHotkey.modifiers | MOD_NOREPEAT,
                       m_sidetoneHotkey.vk);
    }
}

void SoundboardManager::unregisterHotkeys(HWND hwnd) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!hwnd) return;

    for (size_t i = 0; i < MAX_SOUND_EFFECTS; ++i) {
        UnregisterHotKey(hwnd, BASE_HOTKEY_ID + static_cast<int>(i));
    }
    UnregisterHotKey(hwnd, STOP_ALL_HOTKEY_ID);

    for (int i = 0; i < 7; ++i) {
        UnregisterHotKey(hwnd, VOICE_PRESET_BASE_HOTKEY_ID + i);
    }
    UnregisterHotKey(hwnd, MIC_MUTE_HOTKEY_ID);
    UnregisterHotKey(hwnd, SIDETONE_HOTKEY_ID);

    m_registeredHwnd = nullptr;
}

void SoundboardManager::reregisterHotkeys() {
    if (m_registeredHwnd) {
        registerHotkeys(m_registeredHwnd);
    }
}

bool SoundboardManager::handleHotkey(int hotkeyId) {
    if (hotkeyId == STOP_ALL_HOTKEY_ID) {
        stopAllSounds();
        return true;
    }

    if (hotkeyId >= VOICE_PRESET_BASE_HOTKEY_ID && hotkeyId < VOICE_PRESET_BASE_HOTKEY_ID + 7) {
        if (m_voicePresetCb) {
            m_voicePresetCb(hotkeyId - VOICE_PRESET_BASE_HOTKEY_ID);
        }
        return true;
    }

    if (hotkeyId == MIC_MUTE_HOTKEY_ID) {
        if (m_micMuteCb) {
            m_micMuteCb();
        }
        return true;
    }

    if (hotkeyId == SIDETONE_HOTKEY_ID) {
        if (m_sidetoneCb) {
            m_sidetoneCb();
        }
        return true;
    }

    if (hotkeyId >= BASE_HOTKEY_ID && hotkeyId < BASE_HOTKEY_ID + static_cast<int>(MAX_SOUND_EFFECTS)) {
        size_t idx = static_cast<size_t>(hotkeyId - BASE_HOTKEY_ID);
        if (idx < MAX_SOUND_EFFECTS && m_slots[idx].active.load(std::memory_order_acquire) && m_slots[idx].sound) {
            m_slots[idx].sound->play();
            return true;
        }
    }
    return false;
}

void SoundboardManager::setStopAllHotkey(uint32_t modifiers, uint32_t vk, const std::string& text) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_registeredHwnd) {
        UnregisterHotKey(m_registeredHwnd, STOP_ALL_HOTKEY_ID);
    }
    m_stopAllModifiers = modifiers;
    m_stopAllVk = vk;
    m_stopAllText = text;

    if (m_registeredHwnd && m_stopAllVk != 0) {
        RegisterHotKey(m_registeredHwnd, STOP_ALL_HOTKEY_ID, m_stopAllModifiers | MOD_NOREPEAT, m_stopAllVk);
    }
}

void SoundboardManager::clearStopAllHotkey() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_registeredHwnd) {
        UnregisterHotKey(m_registeredHwnd, STOP_ALL_HOTKEY_ID);
    }
    m_stopAllModifiers = 0;
    m_stopAllVk = 0;
    m_stopAllText.clear();
}

std::string SoundboardManager::formatHotkeyText(uint32_t modifiers, uint32_t vk) {
    if (vk == 0) return "None";

    std::ostringstream ss;
    if (modifiers & MOD_CONTROL) ss << "Ctrl+";
    if (modifiers & MOD_ALT)     ss << "Alt+";
    if (modifiers & MOD_SHIFT)   ss << "Shift+";
    if (modifiers & MOD_WIN)     ss << "Win+";

    // Handle Function keys
    if (vk >= VK_F1 && vk <= VK_F24) {
        ss << "F" << (vk - VK_F1 + 1);
        return ss.str();
    }
    // Handle Numpad
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        ss << "Num " << (vk - VK_NUMPAD0);
        return ss.str();
    }
    switch (vk) {
        case VK_MULTIPLY: ss << "Num *"; return ss.str();
        case VK_ADD:      ss << "Num +"; return ss.str();
        case VK_SUBTRACT: ss << "Num -"; return ss.str();
        case VK_DECIMAL:  ss << "Num ."; return ss.str();
        case VK_DIVIDE:   ss << "Num /"; return ss.str();
        case VK_SPACE:    ss << "Space"; return ss.str();
        case VK_TAB:      ss << "Tab"; return ss.str();
        case VK_ESCAPE:   ss << "Esc"; return ss.str();
        case VK_PRIOR:    ss << "PageUp"; return ss.str();
        case VK_NEXT:     ss << "PageDown"; return ss.str();
        case VK_END:      ss << "End"; return ss.str();
        case VK_HOME:     ss << "Home"; return ss.str();
        case VK_INSERT:   ss << "Insert"; return ss.str();
        case VK_DELETE:   ss << "Delete"; return ss.str();
        case VK_UP:       ss << "Up"; return ss.str();
        case VK_DOWN:     ss << "Down"; return ss.str();
        case VK_LEFT:     ss << "Left"; return ss.str();
        case VK_RIGHT:    ss << "Right"; return ss.str();
        default: break;
    }

    // Letters & Numbers
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) {
        ss << static_cast<char>(vk);
        return ss.str();
    }

    // Fallback virtual key code
    ss << "Key0x" << std::hex << std::uppercase << vk;
    return ss.str();
}

void SoundboardManager::setVoicePresetHotkey(int presetIndex, uint32_t mods, uint32_t vk, const std::string& text) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (presetIndex < 0 || presetIndex >= 7) return;
    if (m_registeredHwnd) {
        UnregisterHotKey(m_registeredHwnd, VOICE_PRESET_BASE_HOTKEY_ID + presetIndex);
    }
    m_voiceHotkeys[presetIndex].modifiers = mods;
    m_voiceHotkeys[presetIndex].vk = vk;
    m_voiceHotkeys[presetIndex].text = text;
    if (m_registeredHwnd && vk != 0) {
        RegisterHotKey(m_registeredHwnd, VOICE_PRESET_BASE_HOTKEY_ID + presetIndex, mods | MOD_NOREPEAT, vk);
    }
}

SoundboardManager::HotkeyConfig SoundboardManager::getVoicePresetHotkey(int presetIndex) const {
    if (presetIndex < 0 || presetIndex >= 7) return {};
    return m_voiceHotkeys[presetIndex];
}

void SoundboardManager::setMicMuteHotkey(uint32_t mods, uint32_t vk, const std::string& text) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_registeredHwnd) {
        UnregisterHotKey(m_registeredHwnd, MIC_MUTE_HOTKEY_ID);
    }
    m_micMuteHotkey.modifiers = mods;
    m_micMuteHotkey.vk = vk;
    m_micMuteHotkey.text = text;
    if (m_registeredHwnd && vk != 0) {
        RegisterHotKey(m_registeredHwnd, MIC_MUTE_HOTKEY_ID, mods | MOD_NOREPEAT, vk);
    }
}

void SoundboardManager::setSidetoneHotkey(uint32_t mods, uint32_t vk, const std::string& text) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_registeredHwnd) {
        UnregisterHotKey(m_registeredHwnd, SIDETONE_HOTKEY_ID);
    }
    m_sidetoneHotkey.modifiers = mods;
    m_sidetoneHotkey.vk = vk;
    m_sidetoneHotkey.text = text;
    if (m_registeredHwnd && vk != 0) {
        RegisterHotKey(m_registeredHwnd, SIDETONE_HOTKEY_ID, mods | MOD_NOREPEAT, vk);
    }
}
