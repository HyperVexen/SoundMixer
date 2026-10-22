#include "VirtualDeviceBridge.h"
#include "AudioEngine.h"
#include "AudioOutput.h"
#include <algorithm>
#include <iostream>
#include <shellapi.h>
#include <shlobj.h>
#include <filesystem>
#include <mmdeviceapi.h>

// Undocumented Windows COM interface for setting default audio endpoints
interface DECLSPEC_UUID("f8679f50-850a-41cf-9c72-430f290290c8")
IPolicyConfig : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE GetMixFormat(PCWSTR, WAVEFORMATEX**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDeviceFormat(PCWSTR, INT, WAVEFORMATEX**) = 0;
    virtual HRESULT STDMETHODCALLTYPE ResetDeviceFormat(PCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDeviceFormat(PCWSTR, WAVEFORMATEX*, WAVEFORMATEX*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetProcessingPeriod(PCWSTR, INT, PINT64, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetProcessingPeriod(PCWSTR, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetShareMode(PCWSTR, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetShareMode(PCWSTR, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPropertyValue(PCWSTR, const PROPERTYKEY&, PROPVARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPropertyValue(PCWSTR, const PROPERTYKEY&, PROPVARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(PCWSTR wszDeviceId, ERole eRole) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetEndpointVisibility(PCWSTR, INT) = 0;
};

static const CLSID CLSID_PolicyConfigClient = 
    { 0x870af99c, 0x171d, 0x4f9e, { 0xaf, 0x0d, 0xe6, 0x3d, 0xf4, 0x0c, 0x2b, 0xc9 } };

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)::tolower(c); });
    return s;
}

VirtualDeviceBridge::VirtualDeviceBridge(AudioEngine& engine)
    : m_engine(engine) {
    refreshStatus();
}

bool VirtualDeviceBridge::isVirtualPlaybackDevice(const std::string& name) {
    std::string lower = toLower(name);
    return (lower.find("cable input") != std::string::npos ||
            lower.find("vb-audio") != std::string::npos ||
            lower.find("voicemod") != std::string::npos ||
            lower.find("virtual audio cable") != std::string::npos ||
            lower.find("soundmixer virtual") != std::string::npos);
}

bool VirtualDeviceBridge::isVirtualRecordingDevice(const std::string& name) {
    std::string lower = toLower(name);
    return (lower.find("cable output") != std::string::npos ||
            lower.find("vb-audio") != std::string::npos ||
            lower.find("voicemod") != std::string::npos ||
            lower.find("virtual audio cable") != std::string::npos ||
            lower.find("soundmixer virtual") != std::string::npos);
}

bool VirtualDeviceBridge::isRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
                                 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(nullptr, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin == TRUE;
}

VirtualDeviceInfo VirtualDeviceBridge::refreshStatus() {
    m_info = VirtualDeviceInfo();

    auto playDevs = m_engine.getDeviceManager().getRenderDevices();
    auto captDevs = m_engine.getDeviceManager().getCaptureDevices();

    // Look for virtual playback endpoint (SoundMixer feeds audio here)
    for (const auto& dev : playDevs) {
        if (isVirtualPlaybackDevice(dev.nameUtf8)) {
            m_info.hasPlaybackDevice = true;
            m_info.playbackDevice = dev;
            if (toLower(dev.nameUtf8).find("voicemod") != std::string::npos) {
                m_info.deviceBrand = "Voicemod";
            } else if (toLower(dev.nameUtf8).find("vb-audio") != std::string::npos ||
                       toLower(dev.nameUtf8).find("cable") != std::string::npos) {
                m_info.deviceBrand = "VB-Cable";
            } else {
                m_info.deviceBrand = "Virtual Audio Bridge";
            }
            break;
        }
    }

    // Look for virtual recording endpoint (Discord/games listen from here)
    for (const auto& dev : captDevs) {
        if (isVirtualRecordingDevice(dev.nameUtf8)) {
            m_info.hasRecordingDevice = true;
            m_info.recordingDevice = dev;
            break;
        }
    }

    if (!m_info.hasPlaybackDevice) {
        m_info.status = VirtualBridgeStatus::NotInstalled;
        return m_info;
    }

    // Check if the playback device is currently in the active engine output list
    bool isConnected = false;
    auto activeOutputs = m_engine.getOutputs();
    for (const auto& out : activeOutputs) {
        if (out->getId() == m_info.playbackDevice.idUtf8) {
            isConnected = true;
            break;
        }
    }

    m_info.status = isConnected ? VirtualBridgeStatus::ActiveAndConnected : VirtualBridgeStatus::InstalledNotConnected;

    // Check if the virtual recording device is set as the default communication microphone in Windows
    if (m_info.hasRecordingDevice) {
        IMMDeviceEnumerator* pEnum = nullptr;
        if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                       __uuidof(IMMDeviceEnumerator), (void**)&pEnum)) && pEnum) {
            IMMDevice* pDefComm = nullptr;
            if (SUCCEEDED(pEnum->GetDefaultAudioEndpoint(eCapture, eCommunications, &pDefComm)) && pDefComm) {
                LPWSTR defId = nullptr;
                if (SUCCEEDED(pDefComm->GetId(&defId)) && defId) {
                    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, defId, -1, nullptr, 0, nullptr, nullptr);
                    if (sizeNeeded > 0) {
                        std::string defIdUtf8(sizeNeeded - 1, 0);
                        WideCharToMultiByte(CP_UTF8, 0, defId, -1, &defIdUtf8[0], sizeNeeded, nullptr, nullptr);
                        if (defIdUtf8 == m_info.recordingDevice.idUtf8) {
                            m_info.isDefaultCommunicationMic = true;
                        }
                    }
                    CoTaskMemFree(defId);
                }
                pDefComm->Release();
            }
            pEnum->Release();
        }
    }

    return m_info;
}

bool VirtualDeviceBridge::connectVirtualDevice() {
    refreshStatus();
    if (!m_info.hasPlaybackDevice) return false;

    // Add device to outputs if not already present
    bool alreadyPresent = false;
    auto currentOutputs = m_engine.getOutputs();
    for (auto& out : currentOutputs) {
        if (out->getId() == m_info.playbackDevice.idUtf8) {
            alreadyPresent = true;
            out->setRouteMode(OutputRouteMode::VirtualMicStream);
            break;
        }
    }

    if (!alreadyPresent) {
        if (m_engine.addOutput(m_info.playbackDevice.idUtf8, m_info.playbackDevice.nameUtf8, 1.0f, false)) {
            auto updatedOutputs = m_engine.getOutputs();
            for (auto& out : updatedOutputs) {
                if (out->getId() == m_info.playbackDevice.idUtf8) {
                    out->setRouteMode(OutputRouteMode::VirtualMicStream);
                    break;
                }
            }
        }
    }

    refreshStatus();
    return m_info.status == VirtualBridgeStatus::ActiveAndConnected;
}

bool VirtualDeviceBridge::disconnectVirtualDevice() {
    if (!m_info.hasPlaybackDevice) return false;
    m_engine.removeOutput(m_info.playbackDevice.idUtf8);
    refreshStatus();
    return true;
}

bool VirtualDeviceBridge::setAsDefaultCommunicationDevice() {
    refreshStatus();
    if (!m_info.hasRecordingDevice) return false;

    int wideLen = MultiByteToWideChar(CP_UTF8, 0, m_info.recordingDevice.idUtf8.c_str(), -1, nullptr, 0);
    if (wideLen <= 0) return false;

    std::wstring wideId(wideLen - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, m_info.recordingDevice.idUtf8.c_str(), -1, &wideId[0], wideLen);

    IPolicyConfig* pPolicy = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_PolicyConfigClient, nullptr, CLSCTX_ALL,
                                  __uuidof(IPolicyConfig), (void**)&pPolicy);
    if (FAILED(hr) || !pPolicy) {
        return false;
    }

    // Set as default communications device for Discord, Skype, Zoom, and games
    hr = pPolicy->SetDefaultEndpoint(wideId.c_str(), eCommunications);
    pPolicy->Release();

    refreshStatus();
    return SUCCEEDED(hr);
}

bool VirtualDeviceBridge::installDriver(std::string& outErrorMessage) {
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    std::wstring workDir = std::wstring(tempPath) + L"SoundMixer_VirtualAudioBridge\\";
    CreateDirectoryW(workDir.c_str(), nullptr);

    std::wstring zipPath = workDir + L"vbcable.zip";
    std::wstring installerExe = workDir + L"VBCABLE_Setup_x64.exe";

    // 1. Check if installer already extracted
    if (GetFileAttributesW(installerExe.c_str()) == INVALID_FILE_ATTRIBUTES) {
        // Download using Windows built-in curl.exe
        std::wstring curlCmd = L"curl.exe -L -s -o \"" + zipPath + L"\" \"https://download.vb-audio.com/Download_AUR/VBCABLE_Driver_Pack43.zip\"";
        
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        std::vector<wchar_t> cmdBuf(curlCmd.begin(), curlCmd.end());
        cmdBuf.push_back(0);

        if (CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 30000); // 30s timeout
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }

        // Extract using Windows built-in tar.exe
        std::wstring tarCmd = L"tar.exe -xf \"" + zipPath + L"\" -C \"" + workDir + L"\"";
        std::vector<wchar_t> tarBuf(tarCmd.begin(), tarCmd.end());
        tarBuf.push_back(0);

        if (CreateProcessW(nullptr, tarBuf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 15000);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }

    if (GetFileAttributesW(installerExe.c_str()) == INVALID_FILE_ATTRIBUTES) {
        outErrorMessage = "Failed to download or extract virtual audio driver.";
        return false;
    }

    // 2. Launch silent driver installation with Administrator UAC prompt
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"runas"; // Elevate to admin
    sei.lpFile = installerExe.c_str();
    sei.lpParameters = L"-i -h"; // Silent install
    sei.nShow = SW_HIDE;
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;

    if (!ShellExecuteExW(&sei) || !sei.hProcess) {
        outErrorMessage = "Installation was cancelled or UAC permission was denied.";
        return false;
    }

    // Wait up to 30 seconds for driver registration to complete
    WaitForSingleObject(sei.hProcess, 30000);
    CloseHandle(sei.hProcess);

    // Give Windows PnP subsystem a moment to enumerate new audio endpoints
    Sleep(1500);

    // Refresh devices in engine
    m_engine.getDeviceManager().refreshDevices();
    refreshStatus();

    if (m_info.hasPlaybackDevice) {
        connectVirtualDevice();
        return true;
    }

    outErrorMessage = "Driver installed, but Windows needs a moment or system reboot to initialize audio endpoints.";
    return false;
}
