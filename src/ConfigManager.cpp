#include "ConfigManager.h"
#include "AudioEngine.h"
#include "MicCaptureSource.h"
#include "ProcessCaptureSource.h"
#include "ProcessEnumerator.h"
#include "../third_party/nlohmann/json.hpp"
#include <fstream>
#include <iostream>

using json = nlohmann::json;

std::string ConfigManager::getDefaultConfigPath() {
    return "soundmixer_config.json";
}

bool ConfigManager::loadConfig(const std::string& filePath, AppConfig& outConfig) {
    std::ifstream file(filePath);
    if (!file.is_open()) return false;

    try {
        json j;
        file >> j;

        if (j.contains("masterVolume")) outConfig.masterVolume = j["masterVolume"].get<float>();
        if (j.contains("masterMuted")) outConfig.masterMuted = j["masterMuted"].get<bool>();

        if (j.contains("sfxMasterVolume")) outConfig.sfxMasterVolume = j["sfxMasterVolume"].get<float>();
        if (j.contains("sfxMasterMuted")) outConfig.sfxMasterMuted = j["sfxMasterMuted"].get<bool>();
        if (j.contains("stopAllModifiers")) outConfig.stopAllModifiers = j["stopAllModifiers"].get<uint32_t>();
        if (j.contains("stopAllVk")) outConfig.stopAllVk = j["stopAllVk"].get<uint32_t>();
        if (j.contains("stopAllText")) outConfig.stopAllText = j["stopAllText"].get<std::string>();

        outConfig.soundEffects.clear();
        if (j.contains("soundEffects") && j["soundEffects"].is_array()) {
            for (const auto& item : j["soundEffects"]) {
                SavedSoundEffectConfig sfx;
                if (item.contains("id")) sfx.id = item["id"].get<std::string>();
                if (item.contains("name")) sfx.name = item["name"].get<std::string>();
                if (item.contains("filePath")) sfx.filePath = item["filePath"].get<std::string>();
                if (item.contains("volume")) sfx.volume = item["volume"].get<float>();
                if (item.contains("loop")) sfx.loop = item["loop"].get<bool>();
                if (item.contains("hotkeyModifiers")) sfx.hotkeyModifiers = item["hotkeyModifiers"].get<uint32_t>();
                if (item.contains("hotkeyVk")) sfx.hotkeyVk = item["hotkeyVk"].get<uint32_t>();
                if (item.contains("hotkeyText")) sfx.hotkeyText = item["hotkeyText"].get<std::string>();
                outConfig.soundEffects.push_back(sfx);
            }
        }

        outConfig.outputs.clear();
        if (j.contains("outputs") && j["outputs"].is_array()) {
            for (const auto& item : j["outputs"]) {
                SavedOutputConfig out;
                if (item.contains("id")) out.id = item["id"].get<std::string>();
                if (item.contains("name")) out.name = item["name"].get<std::string>();
                if (item.contains("volume")) out.volume = item["volume"].get<float>();
                if (item.contains("muted")) out.muted = item["muted"].get<bool>();
                if (item.contains("isPrimary")) out.isPrimary = item["isPrimary"].get<bool>();
                outConfig.outputs.push_back(out);
            }
        } else if (j.contains("outputDeviceId")) {
            // Legacy config compatibility
            SavedOutputConfig out;
            out.id = j["outputDeviceId"].get<std::string>();
            if (j.contains("outputDeviceName")) out.name = j["outputDeviceName"].get<std::string>();
            out.isPrimary = true;
            outConfig.outputs.push_back(out);

            if (j.contains("monitorEnabled") && j["monitorEnabled"].get<bool>() && j.contains("monitorDeviceId")) {
                SavedOutputConfig mon;
                mon.id = j["monitorDeviceId"].get<std::string>();
                if (j.contains("monitorDeviceName")) mon.name = j["monitorDeviceName"].get<std::string>();
                if (j.contains("monitorVolume")) mon.volume = j["monitorVolume"].get<float>();
                if (j.contains("monitorMuted")) mon.muted = j["monitorMuted"].get<bool>();
                outConfig.outputs.push_back(mon);
            }
        }

        outConfig.sources.clear();
        if (j.contains("sources") && j["sources"].is_array()) {
            for (const auto& item : j["sources"]) {
                SavedSourceConfig src;
                if (item.contains("type")) src.type = item["type"].get<std::string>();
                if (item.contains("id")) src.id = item["id"].get<std::string>();
                if (item.contains("name")) src.name = item["name"].get<std::string>();
                if (item.contains("volume")) src.volume = item["volume"].get<float>();
                if (item.contains("muted")) src.muted = item["muted"].get<bool>();
                outConfig.sources.push_back(src);
            }
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Config load error: " << e.what() << std::endl;
        return false;
    }
}

bool ConfigManager::saveConfig(const std::string& filePath, const AppConfig& config) {
    try {
        json j;
        j["masterVolume"] = config.masterVolume;
        j["masterMuted"] = config.masterMuted;

        j["sfxMasterVolume"] = config.sfxMasterVolume;
        j["sfxMasterMuted"] = config.sfxMasterMuted;
        j["stopAllModifiers"] = config.stopAllModifiers;
        j["stopAllVk"] = config.stopAllVk;
        j["stopAllText"] = config.stopAllText;

        json sfxArray = json::array();
        for (const auto& sfx : config.soundEffects) {
            json item;
            item["id"] = sfx.id;
            item["name"] = sfx.name;
            item["filePath"] = sfx.filePath;
            item["volume"] = sfx.volume;
            item["loop"] = sfx.loop;
            item["hotkeyModifiers"] = sfx.hotkeyModifiers;
            item["hotkeyVk"] = sfx.hotkeyVk;
            item["hotkeyText"] = sfx.hotkeyText;
            sfxArray.push_back(item);
        }
        j["soundEffects"] = sfxArray;

        json outputsArray = json::array();
        for (const auto& out : config.outputs) {
            json item;
            item["id"] = out.id;
            item["name"] = out.name;
            item["volume"] = out.volume;
            item["muted"] = out.muted;
            item["isPrimary"] = out.isPrimary;
            outputsArray.push_back(item);
        }
        j["outputs"] = outputsArray;

        json sourcesArray = json::array();
        for (const auto& src : config.sources) {
            json item;
            item["type"] = src.type;
            item["id"] = src.id;
            item["name"] = src.name;
            item["volume"] = src.volume;
            item["muted"] = src.muted;
            sourcesArray.push_back(item);
        }
        j["sources"] = sourcesArray;

        std::ofstream file(filePath);
        if (!file.is_open()) return false;
        file << j.dump(4);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Config save error: " << e.what() << std::endl;
        return false;
    }
}

AppConfig ConfigManager::extractFromEngine(AudioEngine& engine) {
    AppConfig cfg;
    cfg.masterVolume = engine.getMixer().getMasterVolume();
    cfg.masterMuted = engine.getMixer().isMasterMuted();

    auto outputs = engine.getOutputs();
    for (const auto& out : outputs) {
        SavedOutputConfig o;
        o.id = out->getId();
        o.name = out->getName();
        o.volume = out->getVolume();
        o.muted = out->isMuted();
        o.isPrimary = out->isPrimary();
        cfg.outputs.push_back(o);
    }

    auto sources = engine.getMixer().getSources();
    for (const auto& src : sources) {
        SavedSourceConfig s;
        s.name = src->getName();
        s.volume = src->getVolume();
        s.muted = src->isMuted();

        if (src->getType() == SourceType::Microphone) {
            s.type = "mic";
            s.id = src->getId();
        } else {
            s.type = "app";
            auto appSrc = std::dynamic_pointer_cast<ProcessCaptureSource>(src);
            if (appSrc) {
                s.id = appSrc->getExeName();
            } else {
                s.id = src->getId();
            }
        }
        cfg.sources.push_back(s);
    }

    auto& sb = engine.getSoundboard();
    cfg.sfxMasterVolume = sb.getMasterVolume();
    cfg.sfxMasterMuted = sb.isMasterMuted();
    cfg.stopAllModifiers = sb.getStopAllModifiers();
    cfg.stopAllVk = sb.getStopAllVk();
    cfg.stopAllText = sb.getStopAllText();

    auto sfxList = sb.getSounds();
    for (const auto& sfx : sfxList) {
        SavedSoundEffectConfig s;
        s.id = sfx->getId();
        s.name = sfx->getName();
        s.filePath = sfx->getFilePath();
        s.volume = sfx->getVolume();
        s.loop = sfx->isLooping();
        s.hotkeyModifiers = sfx->getHotkeyModifiers();
        s.hotkeyVk = sfx->getHotkeyVk();
        s.hotkeyText = sfx->getHotkeyText();
        cfg.soundEffects.push_back(s);
    }

    return cfg;
}

void ConfigManager::applyToEngine(AudioEngine& engine, const AppConfig& config) {
    // Master settings
    engine.getMixer().setMasterVolume(config.masterVolume);
    engine.getMixer().setMasterMuted(config.masterMuted);

    // Outputs
    if (!config.outputs.empty()) {
        engine.clearOutputs();
        for (const auto& out : config.outputs) {
            engine.addOutput(out.id, out.name, out.volume, out.muted);
        }
    }

    // Reconstruct sources
    auto runningApps = ProcessEnumerator::getRunningApplications();
    auto captureDevs = engine.getDeviceManager().getCaptureDevices();

    for (const auto& s : config.sources) {
        if (s.type == "mic") {
            for (const auto& dev : captureDevs) {
                if (dev.idUtf8 == s.id || dev.nameUtf8 == s.name) {
                    auto mic = std::make_shared<MicCaptureSource>(dev.idUtf8, dev.nameUtf8);
                    mic->setVolume(s.volume);
                    mic->setMuted(s.muted);
                    if (mic->start()) {
                        engine.getMixer().addSource(mic);
                    }
                    break;
                }
            }
        } else if (s.type == "app") {
            for (const auto& app : runningApps) {
                if (app.exeName == s.id) {
                    DWORD rootPid = ProcessEnumerator::findRootProcessId(app.pid);
                    auto appSource = std::make_shared<ProcessCaptureSource>(rootPid, app.exeName);
                    appSource->setVolume(s.volume);
                    appSource->setMuted(s.muted);
                    if (appSource->start()) {
                        engine.getMixer().addSource(appSource);
                    }
                    break;
                }
            }
        }
    }

    // Configure Soundboard
    auto& sb = engine.getSoundboard();
    sb.setMasterVolume(config.sfxMasterVolume);
    sb.setMasterMuted(config.sfxMasterMuted);
    if (config.stopAllVk != 0) {
        sb.setStopAllHotkey(config.stopAllModifiers, config.stopAllVk, config.stopAllText);
    }

    if (!config.soundEffects.empty()) {
        sb.clearSounds();
        for (const auto& s : config.soundEffects) {
            auto added = sb.addSound(s.filePath, s.name);
            if (added) {
                added->setVolume(s.volume);
                added->setLooping(s.loop);
                if (s.hotkeyVk != 0) {
                    added->setHotkey(s.hotkeyModifiers, s.hotkeyVk, s.hotkeyText);
                }
            }
        }
    } else {
        // Pre-populate with standard sample sound effects on fresh launch!
        auto s1 = sb.addBuiltIn(BuiltInSoundType::Airhorn);
        if (s1) s1->setHotkey(MOD_CONTROL | MOD_ALT, VK_F8, "Ctrl+Alt+F8");

        auto s2 = sb.addBuiltIn(BuiltInSoundType::Rimshot);
        if (s2) s2->setHotkey(MOD_CONTROL | MOD_ALT, VK_F9, "Ctrl+Alt+F9");

        auto s3 = sb.addBuiltIn(BuiltInSoundType::Fanfare);
        if (s3) s3->setHotkey(MOD_CONTROL | MOD_ALT, VK_F10, "Ctrl+Alt+F10");

        auto s4 = sb.addBuiltIn(BuiltInSoundType::CoinChime);
        if (s4) s4->setHotkey(MOD_CONTROL | MOD_ALT, VK_F11, "Ctrl+Alt+F11");
    }
}
