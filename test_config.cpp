#include <iostream>
#include <cassert>
#include "ConfigManager.h"
#include "AudioEngine.h"

int main() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    std::cout << "[Test Config Manager] Starting config tests..." << std::endl;

    AppConfig cfg;
    cfg.masterVolume = 0.9f;
    cfg.masterMuted = false;

    SavedOutputConfig o1;
    o1.id = "test_output_vb_cable";
    o1.name = "Speakers (VB-Audio Virtual Cable)";
    o1.volume = 1.0f;
    o1.muted = false;
    o1.isPrimary = true;
    cfg.outputs.push_back(o1);

    SavedOutputConfig o2;
    o2.id = "test_output_headphones";
    o2.name = "Headphones (Realtek(R) Audio)";
    o2.volume = 0.8f;
    o2.muted = false;
    o2.isPrimary = false;
    cfg.outputs.push_back(o2);

    SavedSourceConfig s1;
    s1.type = "mic";
    s1.id = "mic_dev_id";
    s1.name = "Microphone (Realtek(R) Audio)";
    s1.volume = 1.0f;
    s1.muted = false;
    cfg.sources.push_back(s1);

    SavedSourceConfig s2;
    s2.type = "app";
    s2.id = "Spotify.exe";
    s2.name = "Spotify.exe (Music)";
    s2.volume = 0.75f;
    s2.muted = false;
    cfg.sources.push_back(s2);

    std::string testPath = "test_config.json";
    bool saved = ConfigManager::saveConfig(testPath, cfg);
    assert(saved);
    std::cout << "   -> Config saved successfully to " << testPath << std::endl;

    AppConfig loaded;
    bool loadOk = ConfigManager::loadConfig(testPath, loaded);
    assert(loadOk);
    assert(loaded.outputs.size() == 2);
    assert(loaded.outputs[0].id == "test_output_vb_cable");
    assert(loaded.outputs[0].isPrimary == true);
    assert(loaded.outputs[1].id == "test_output_headphones");
    assert(std::abs(loaded.outputs[1].volume - 0.8f) < 1e-4f);
    assert(std::abs(loaded.masterVolume - 0.9f) < 1e-4f);
    assert(loaded.sources.size() == 2);
    assert(loaded.sources[0].name == "Microphone (Realtek(R) Audio)");
    assert(loaded.sources[1].id == "Spotify.exe");
    std::cout << "   -> Multi-output config loaded and matched perfectly!" << std::endl;

    // Remove temp file
    DeleteFileA(testPath.c_str());

    std::cout << "\n>>> CONFIG MANAGER TESTS PASSED! <<<" << std::endl;
    CoUninitialize();
    return 0;
}
