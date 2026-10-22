#pragma once

#include <string>
#include <vector>
#include <memory>
#include <windows.h>
#include "AudioDeviceManager.h"

class AudioEngine;

enum class VirtualBridgeStatus {
    NotInstalled,             // No virtual audio driver detected in Windows
    InstalledNotConnected,    // Driver installed in Windows, but not yet added to SoundMixer
    ActiveAndConnected        // Driver actively receiving Voice FX + Soundboard stream
};

struct VirtualDeviceInfo {
    bool hasPlaybackDevice = false;
    AudioDeviceInfo playbackDevice; // The render pin SoundMixer feeds (e.g. "CABLE Input")

    bool hasRecordingDevice = false;
    AudioDeviceInfo recordingDevice; // The capture pin Discord/games listen to (e.g. "CABLE Output")

    VirtualBridgeStatus status = VirtualBridgeStatus::NotInstalled;
    bool isDefaultCommunicationMic = false;
    std::string deviceBrand; // "VB-Cable", "Voicemod", etc.
};

class VirtualDeviceBridge {
public:
    explicit VirtualDeviceBridge(AudioEngine& engine);
    ~VirtualDeviceBridge() = default;

    // Scan and refresh status of virtual endpoints
    VirtualDeviceInfo refreshStatus();
    const VirtualDeviceInfo& getInfo() const { return m_info; }

    // Connect / bind the virtual output device to SoundMixer's dual-bus engine
    bool connectVirtualDevice();
    bool disconnectVirtualDevice();

    // Set the virtual recording device as Windows default communication mic (for Discord/games)
    bool setAsDefaultCommunicationDevice();

    // Install driver silently (prompts UAC once)
    bool installDriver(std::string& outErrorMessage);

    // Check if the current user process has Administrator privileges
    static bool isRunningAsAdmin();

    // Helper to determine if a device name matches a known virtual audio cable / bridge
    static bool isVirtualPlaybackDevice(const std::string& name);
    static bool isVirtualRecordingDevice(const std::string& name);

private:
    AudioEngine& m_engine;
    VirtualDeviceInfo m_info;
};
