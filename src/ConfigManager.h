#pragma once

#include <string>
#include <vector>

struct SavedSourceConfig {
    std::string type; // "mic" or "app"
    std::string id;   // device ID or exe name
    std::string name;
    float volume = 1.0f;
    bool muted = false;
};

struct SavedOutputConfig {
    std::string id;
    std::string name;
    float volume = 1.0f;
    bool muted = false;
    bool isPrimary = false;
};

struct SavedSoundEffectConfig {
    std::string id;
    std::string name;
    std::string filePath;
    float volume = 1.0f;
    bool loop = false;
    uint32_t hotkeyModifiers = 0;
    uint32_t hotkeyVk = 0;
    std::string hotkeyText;
};

struct AppConfig {
    float masterVolume = 1.0f;
    bool masterMuted = false;

    float sfxMasterVolume = 1.0f;
    bool sfxMasterMuted = false;
    uint32_t stopAllModifiers = 0;
    uint32_t stopAllVk = 0;
    std::string stopAllText;

    std::vector<SavedOutputConfig> outputs;
    std::vector<SavedSourceConfig> sources;
    std::vector<SavedSoundEffectConfig> soundEffects;
};

class AudioEngine;

class ConfigManager {
public:
    static bool loadConfig(const std::string& filePath, AppConfig& outConfig);
    static bool saveConfig(const std::string& filePath, const AppConfig& config);
    static std::string getDefaultConfigPath();

    static void applyToEngine(AudioEngine& engine, const AppConfig& config);
    static AppConfig extractFromEngine(AudioEngine& engine);
};
