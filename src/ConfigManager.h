#pragma once

#include <string>
#include <vector>
#include "VoiceProcessor.h"

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
    int routeMode = 0; // 0 = FullMix, 1 = VirtualMicStream
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

struct SavedVoiceFxConfig {
    bool enabled = false;
    int preset = 0;
    float pitchSemitones = 0.0f;
    float formantSemitones = 0.0f;
    bool robotEnabled = false;
    float robotFreq = 120.0f;
    bool radioEnabled = false;
    float reverbWet = 0.0f;
    bool gateEnabled = true;
    float gateThresholdDb = -48.0f;
    bool compEnabled = false;
    float compThresholdDb = -18.0f;
    float compRatio = 3.0f;
    float compMakeupDb = 3.0f;
    bool eqEnabled = false;
    float eqLowGainDb = 0.0f;
    float eqMidGainDb = 0.0f;
    float eqHighGainDb = 0.0f;
};

struct AppConfig {
    float masterVolume = 1.0f;
    bool masterMuted = false;
    bool sidetoneEnabled = false;

    float sfxMasterVolume = 1.0f;
    bool sfxMasterMuted = false;
    uint32_t stopAllModifiers = 0;
    uint32_t stopAllVk = 0;
    std::string stopAllText;

    SavedVoiceFxConfig voiceFx;
    std::vector<CustomVoicePreset> customPresets;

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
