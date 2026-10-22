#pragma once

#include <windows.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <string>
#include <vector>
#include <mutex>
#include <functional>

struct AudioDeviceInfo {
    std::wstring id;
    std::wstring name;
    std::string idUtf8;
    std::string nameUtf8;
    bool isDefault = false;
    bool isVBCable = false;
};

class AudioDeviceManager : public IMMNotificationClient {
public:
    using DeviceChangeCallback = std::function<void()>;

    AudioDeviceManager();
    ~AudioDeviceManager();

    bool initialize();
    void shutdown();

    void refreshDevices();

    std::vector<AudioDeviceInfo> getRenderDevices();
    std::vector<AudioDeviceInfo> getCaptureDevices();

    AudioDeviceInfo getDefaultRenderDevice();
    AudioDeviceInfo getDefaultCaptureDevice();
    AudioDeviceInfo getVBCableRenderDevice();

    void setDeviceChangeCallback(DeviceChangeCallback cb);

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IMMNotificationClient
    STDMETHODIMP OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState) override;
    STDMETHODIMP OnDeviceAdded(LPCWSTR pwstrDeviceId) override;
    STDMETHODIMP OnDeviceRemoved(LPCWSTR pwstrDeviceId) override;
    STDMETHODIMP OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId) override;
    STDMETHODIMP OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key) override;

private:
    std::vector<AudioDeviceInfo> enumerateEndpoints(EDataFlow flow);
    static std::string wideToUtf8(const std::wstring& wstr);

    IMMDeviceEnumerator* m_pEnumerator = nullptr;
    std::mutex m_mutex;
    std::vector<AudioDeviceInfo> m_renderDevices;
    std::vector<AudioDeviceInfo> m_captureDevices;
    DeviceChangeCallback m_onDeviceChange;
    LONG m_refCount = 1;
};
