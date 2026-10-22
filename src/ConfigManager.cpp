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
        if (j.contains("sidetoneEnabled")) outConfig.sidetoneEnabled = j["sidetoneEnabled"].get<bool>();

        if (j.contains("sfxMasterVolume")) outConfig.sfxMasterVolume = j["sfxMasterVolume"].get<float>();
        if (j.contains("sfxMasterMuted")) outConfig.sfxMasterMuted = j["sfxMasterMuted"].get<bool>();
        if (j.contains("stopAllModifiers")) outConfig.stopAllModifiers = j["stopAllModifiers"].get<uint32_t>();
        if (j.contains("stopAllVk")) outConfig.stopAllVk = j["stopAllVk"].get<uint32_t>();
        if (j.contains("stopAllText")) outConfig.stopAllText = j["stopAllText"].get<std::string>();

        if (j.contains("voiceFx")) {
            const auto& v = j["voiceFx"];
            if (v.contains("enabled")) outConfig.voiceFx.enabled = v["enabled"].get<bool>();
            if (v.contains("preset")) outConfig.voiceFx.preset = v["preset"].get<int>();
            if (v.contains("pitchSemitones")) outConfig.voiceFx.pitchSemitones = v["pitchSemitones"].get<float>();
            if (v.contains("robotEnabled")) outConfig.voiceFx.robotEnabled = v["robotEnabled"].get<bool>();
            if (v.contains("robotFreq")) outConfig.voiceFx.robotFreq = v["robotFreq"].get<float>();
            if (v.contains("radioEnabled")) outConfig.voiceFx.radioEnabled = v["radioEnabled"].get<bool>();
            if (v.contains("reverbWet")) outConfig.voiceFx.reverbWet = v["reverbWet"].get<float>();
            if (v.contains("gateEnabled")) outConfig.voiceFx.gateEnabled = v["gateEnabled"].get<bool>();
            if (v.contains("gateThresholdDb")) outConfig.voiceFx.gateThresholdDb = v["gateThresholdDb"].get<float>();

            if (v.contains("formantSemitones")) outConfig.voiceFx.formantSemitones = v["formantSemitones"].get<float>();
            if (v.contains("compEnabled")) outConfig.voiceFx.compEnabled = v["compEnabled"].get<bool>();
            if (v.contains("compThresholdDb")) outConfig.voiceFx.compThresholdDb = v["compThresholdDb"].get<float>();
            if (v.contains("compRatio")) outConfig.voiceFx.compRatio = v["compRatio"].get<float>();
            if (v.contains("compMakeupDb")) outConfig.voiceFx.compMakeupDb = v["compMakeupDb"].get<float>();
            if (v.contains("eqEnabled")) outConfig.voiceFx.eqEnabled = v["eqEnabled"].get<bool>();
            if (v.contains("eqLowGainDb")) outConfig.voiceFx.eqLowGainDb = v["eqLowGainDb"].get<float>();
            if (v.contains("eqMidGainDb")) outConfig.voiceFx.eqMidGainDb = v["eqMidGainDb"].get<float>();
            if (v.contains("eqHighGainDb")) outConfig.voiceFx.eqHighGainDb = v["eqHighGainDb"].get<float>();
        }

        outConfig.customPresets.clear();
        if (j.contains("customPresets") && j["customPresets"].is_array()) {
            for (const auto& cp : j["customPresets"]) {
                CustomVoicePreset p;
                if (cp.contains("id")) p.id = cp["id"].get<std::string>();
                if (cp.contains("name")) p.name = cp["name"].get<std::string>();
                if (cp.contains("description")) p.description = cp["description"].get<std::string>();
                if (cp.contains("pitchSemitones")) p.pitchSemitones = cp["pitchSemitones"].get<float>();
                if (cp.contains("formantSemitones")) p.formantSemitones = cp["formantSemitones"].get<float>();
                if (cp.contains("robotEnabled")) p.robotEnabled = cp["robotEnabled"].get<bool>();
                if (cp.contains("robotFreq")) p.robotFreq = cp["robotFreq"].get<float>();
                if (cp.contains("radioEnabled")) p.radioEnabled = cp["radioEnabled"].get<bool>();
                if (cp.contains("reverbWet")) p.reverbWet = cp["reverbWet"].get<float>();
                if (cp.contains("gateEnabled")) p.gateEnabled = cp["gateEnabled"].get<bool>();
                if (cp.contains("gateThresholdDb")) p.gateThresholdDb = cp["gateThresholdDb"].get<float>();
                if (cp.contains("compressorEnabled")) p.compressorEnabled = cp["compressorEnabled"].get<bool>();
                if (cp.contains("compThresholdDb")) p.compThresholdDb = cp["compThresholdDb"].get<float>();
                if (cp.contains("compRatio")) p.compRatio = cp["compRatio"].get<float>();
                if (cp.contains("compMakeupDb")) p.compMakeupDb = cp["compMakeupDb"].get<float>();
                if (cp.contains("eqEnabled")) p.eqEnabled = cp["eqEnabled"].get<bool>();
                if (cp.contains("eqLowGainDb")) p.eqLowGainDb = cp["eqLowGainDb"].get<float>();
                if (cp.contains("eqMidGainDb")) p.eqMidGainDb = cp["eqMidGainDb"].get<float>();
                if (cp.contains("eqHighGainDb")) p.eqHighGainDb = cp["eqHighGainDb"].get<float>();
                if (cp.contains("hotkeyModifiers")) p.hotkeyModifiers = cp["hotkeyModifiers"].get<uint32_t>();
                if (cp.contains("hotkeyVk")) p.hotkeyVk = cp["hotkeyVk"].get<uint32_t>();
                if (cp.contains("hotkeyText")) p.hotkeyText = cp["hotkeyText"].get<std::string>();
                outConfig.customPresets.push_back(p);
            }
        }

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
                if (item.contains("routeMode")) out.routeMode = item["routeMode"].get<int>();
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
        j["sidetoneEnabled"] = config.sidetoneEnabled;

        j["sfxMasterVolume"] = config.sfxMasterVolume;
        j["sfxMasterMuted"] = config.sfxMasterMuted;
        j["stopAllModifiers"] = config.stopAllModifiers;
        j["stopAllVk"] = config.stopAllVk;
        j["stopAllText"] = config.stopAllText;

        json v;
        v["enabled"] = config.voiceFx.enabled;
        v["preset"] = config.voiceFx.preset;
        v["pitchSemitones"] = config.voiceFx.pitchSemitones;
        v["robotEnabled"] = config.voiceFx.robotEnabled;
        v["robotFreq"] = config.voiceFx.robotFreq;
        v["radioEnabled"] = config.voiceFx.radioEnabled;
        v["reverbWet"] = config.voiceFx.reverbWet;
        v["gateEnabled"] = config.voiceFx.gateEnabled;
        v["gateThresholdDb"] = config.voiceFx.gateThresholdDb;
        v["formantSemitones"] = config.voiceFx.formantSemitones;
        v["compEnabled"] = config.voiceFx.compEnabled;
        v["compThresholdDb"] = config.voiceFx.compThresholdDb;
        v["compRatio"] = config.voiceFx.compRatio;
        v["compMakeupDb"] = config.voiceFx.compMakeupDb;
        v["eqEnabled"] = config.voiceFx.eqEnabled;
        v["eqLowGainDb"] = config.voiceFx.eqLowGainDb;
        v["eqMidGainDb"] = config.voiceFx.eqMidGainDb;
        v["eqHighGainDb"] = config.voiceFx.eqHighGainDb;
        j["voiceFx"] = v;

        json cpArray = json::array();
        for (const auto& cp : config.customPresets) {
            json item;
            item["id"] = cp.id;
            item["name"] = cp.name;
            item["description"] = cp.description;
            item["pitchSemitones"] = cp.pitchSemitones;
            item["formantSemitones"] = cp.formantSemitones;
            item["robotEnabled"] = cp.robotEnabled;
            item["robotFreq"] = cp.robotFreq;
            item["radioEnabled"] = cp.radioEnabled;
            item["reverbWet"] = cp.reverbWet;
            item["gateEnabled"] = cp.gateEnabled;
            item["gateThresholdDb"] = cp.gateThresholdDb;
            item["compressorEnabled"] = cp.compressorEnabled;
            item["compThresholdDb"] = cp.compThresholdDb;
            item["compRatio"] = cp.compRatio;
            item["compMakeupDb"] = cp.compMakeupDb;
            item["eqEnabled"] = cp.eqEnabled;
            item["eqLowGainDb"] = cp.eqLowGainDb;
            item["eqMidGainDb"] = cp.eqMidGainDb;
            item["eqHighGainDb"] = cp.eqHighGainDb;
            item["hotkeyModifiers"] = cp.hotkeyModifiers;
            item["hotkeyVk"] = cp.hotkeyVk;
            item["hotkeyText"] = cp.hotkeyText;
            cpArray.push_back(item);
        }
        j["customPresets"] = cpArray;

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
            item["routeMode"] = out.routeMode;
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
    cfg.sidetoneEnabled = engine.getMixer().isSidetoneEnabled();

    auto outputs = engine.getOutputs();
    for (const auto& out : outputs) {
        SavedOutputConfig o;
        o.id = out->getId();
        o.name = out->getName();
        o.volume = out->getVolume();
        o.muted = out->isMuted();
        o.isPrimary = out->isPrimary();
        o.routeMode = static_cast<int>(out->getRouteMode());
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

            auto mic = std::dynamic_pointer_cast<MicCaptureSource>(src);
            if (mic) {
                auto& vp = mic->getVoiceProcessor();
                cfg.voiceFx.enabled = vp.isEnabled();
                cfg.voiceFx.preset = static_cast<int>(vp.getPreset());
                cfg.voiceFx.pitchSemitones = vp.getPitchSemitones();
                cfg.voiceFx.robotEnabled = vp.isRobotEnabled();
                cfg.voiceFx.robotFreq = vp.getRobotFreq();
                cfg.voiceFx.radioEnabled = vp.isRadioEnabled();
                cfg.voiceFx.reverbWet = vp.getReverbWet();
                cfg.voiceFx.gateEnabled = vp.isNoiseGateEnabled();
                cfg.voiceFx.gateThresholdDb = vp.getNoiseGateThresholdDb();
                cfg.voiceFx.formantSemitones = vp.getFormantSemitones();
                cfg.voiceFx.compEnabled = vp.isCompressorEnabled();
                cfg.voiceFx.compThresholdDb = vp.getCompressorThresholdDb();
                cfg.voiceFx.compRatio = vp.getCompressorRatio();
                cfg.voiceFx.compMakeupDb = vp.getCompressorMakeupGainDb();
                cfg.voiceFx.eqEnabled = vp.isEqEnabled();
                cfg.voiceFx.eqLowGainDb = vp.getEqLowGainDb();
                cfg.voiceFx.eqMidGainDb = vp.getEqMidGainDb();
                cfg.voiceFx.eqHighGainDb = vp.getEqHighGainDb();
            }
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
    engine.getMixer().setSidetoneEnabled(config.sidetoneEnabled);

    // Outputs
    if (!config.outputs.empty()) {
        engine.clearOutputs();
        for (const auto& out : config.outputs) {
            if (engine.addOutput(out.id, out.name, out.volume, out.muted)) {
                auto allOuts = engine.getOutputs();
                for (auto& o : allOuts) {
                    if (o->getId() == out.id) {
                        o->setRouteMode(static_cast<OutputRouteMode>(out.routeMode));
                    }
                }
            }
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

                    // Restore Voice FX settings
                    auto& vp = mic->getVoiceProcessor();
                    vp.setEnabled(config.voiceFx.enabled);
                    vp.setPreset(static_cast<VoicePreset>(config.voiceFx.preset));
                    vp.setPitchSemitones(config.voiceFx.pitchSemitones);
                    vp.setRobotEnabled(config.voiceFx.robotEnabled);
                    vp.setRobotFreq(config.voiceFx.robotFreq);
                    vp.setRadioEnabled(config.voiceFx.radioEnabled);
                    vp.setReverbWet(config.voiceFx.reverbWet);
                    vp.setNoiseGateEnabled(config.voiceFx.gateEnabled);
                    vp.setNoiseGateThresholdDb(config.voiceFx.gateThresholdDb);
                    vp.setFormantSemitones(config.voiceFx.formantSemitones);
                    vp.setCompressorEnabled(config.voiceFx.compEnabled);
                    vp.setCompressorThresholdDb(config.voiceFx.compThresholdDb);
                    vp.setCompressorRatio(config.voiceFx.compRatio);
                    vp.setCompressorMakeupGainDb(config.voiceFx.compMakeupDb);
                    vp.setEqEnabled(config.voiceFx.eqEnabled);
                    vp.setEqLowGainDb(config.voiceFx.eqLowGainDb);
                    vp.setEqMidGainDb(config.voiceFx.eqMidGainDb);
                    vp.setEqHighGainDb(config.voiceFx.eqHighGainDb);

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
