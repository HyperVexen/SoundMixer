#include "AudioDeviceManager.h"
#include <iostream>

AudioDeviceManager::AudioDeviceManager() = default;

AudioDeviceManager::~AudioDeviceManager() {
    shutdown();
}

bool AudioDeviceManager::initialize() {
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&m_pEnumerator
    );

    if (FAILED(hr) || !m_pEnumerator) {
        return false;
    }

    m_pEnumerator->RegisterEndpointNotificationCallback(this);
    refreshDevices();
    return true;
}

void AudioDeviceManager::shutdown() {
    if (m_pEnumerator) {
        m_pEnumerator->UnregisterEndpointNotificationCallback(this);
        m_pEnumerator->Release();
        m_pEnumerator = nullptr;
    }
}

std::string AudioDeviceManager::wideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), &result[0], size, nullptr, nullptr);
    return result;
}

std::vector<AudioDeviceInfo> AudioDeviceManager::enumerateEndpoints(EDataFlow flow) {
    std::vector<AudioDeviceInfo> devices;
    if (!m_pEnumerator) return devices;

    IMMDevice* pDefaultDev = nullptr;
    std::wstring defaultId;
    if (SUCCEEDED(m_pEnumerator->GetDefaultAudioEndpoint(flow, eMultimedia, &pDefaultDev))) {
        LPWSTR defId = nullptr;
        if (SUCCEEDED(pDefaultDev->GetId(&defId)) && defId) {
            defaultId = defId;
            CoTaskMemFree(defId);
        }
        pDefaultDev->Release();
    }

    IMMDeviceCollection* pCollection = nullptr;
    if (FAILED(m_pEnumerator->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, &pCollection))) {
        return devices;
    }

    UINT count = 0;
    pCollection->GetCount(&count);

    for (UINT i = 0; i < count; ++i) {
        IMMDevice* pDev = nullptr;
        if (FAILED(pCollection->Item(i, &pDev))) continue;

        AudioDeviceInfo info;
        LPWSTR idStr = nullptr;
        if (SUCCEEDED(pDev->GetId(&idStr)) && idStr) {
            info.id = idStr;
            info.idUtf8 = wideToUtf8(info.id);
            CoTaskMemFree(idStr);
        }

        IPropertyStore* pProps = nullptr;
        if (SUCCEEDED(pDev->OpenPropertyStore(STGM_READ, &pProps))) {
            PROPVARIANT var;
            PropVariantInit(&var);
            if (SUCCEEDED(pProps->GetValue(PKEY_Device_FriendlyName, &var)) && var.pwszVal) {
                info.name = var.pwszVal;
                info.nameUtf8 = wideToUtf8(info.name);
            }
            PropVariantClear(&var);
            pProps->Release();
        }

        if (info.name.empty()) {
            info.name = L"Audio Device " + std::to_wstring(i);
            info.nameUtf8 = wideToUtf8(info.name);
        }

        info.isDefault = (!defaultId.empty() && info.id == defaultId);

        // Detect if this is a VB-Cable endpoint
        if (info.name.find(L"VB-Audio") != std::wstring::npos ||
            info.name.find(L"CABLE") != std::wstring::npos) {
            info.isVBCable = true;
        }

        devices.push_back(info);
        pDev->Release();
    }

    pCollection->Release();
    return devices;
}

void AudioDeviceManager::refreshDevices() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_renderDevices = enumerateEndpoints(eRender);
    m_captureDevices = enumerateEndpoints(eCapture);
}

std::vector<AudioDeviceInfo> AudioDeviceManager::getRenderDevices() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_renderDevices;
}

std::vector<AudioDeviceInfo> AudioDeviceManager::getCaptureDevices() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_captureDevices;
}

AudioDeviceInfo AudioDeviceManager::getDefaultRenderDevice() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& dev : m_renderDevices) {
        if (dev.isDefault) return dev;
    }
    if (!m_renderDevices.empty()) return m_renderDevices[0];
    return {};
}

AudioDeviceInfo AudioDeviceManager::getDefaultCaptureDevice() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& dev : m_captureDevices) {
        if (dev.isDefault) return dev;
    }
    if (!m_captureDevices.empty()) return m_captureDevices[0];
    return {};
}

AudioDeviceInfo AudioDeviceManager::getVBCableRenderDevice() {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Prefer VB-Audio Virtual Cable / CABLE In
    for (const auto& dev : m_renderDevices) {
        if (dev.isVBCable) return dev;
    }
    return getDefaultRenderDevice();
}

void AudioDeviceManager::setDeviceChangeCallback(DeviceChangeCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_onDeviceChange = cb;
}

STDMETHODIMP AudioDeviceManager::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (riid == __uuidof(IUnknown) || riid == __uuidof(IMMNotificationClient)) {
        *ppv = static_cast<IMMNotificationClient*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) AudioDeviceManager::AddRef() {
    return InterlockedIncrement(&m_refCount);
}

STDMETHODIMP_(ULONG) AudioDeviceManager::Release() {
    ULONG res = InterlockedDecrement(&m_refCount);
    return res;
}

STDMETHODIMP AudioDeviceManager::OnDeviceStateChanged(LPCWSTR, DWORD) {
    refreshDevices();
    if (m_onDeviceChange) m_onDeviceChange();
    return S_OK;
}

STDMETHODIMP AudioDeviceManager::OnDeviceAdded(LPCWSTR) {
    refreshDevices();
    if (m_onDeviceChange) m_onDeviceChange();
    return S_OK;
}

STDMETHODIMP AudioDeviceManager::OnDeviceRemoved(LPCWSTR) {
    refreshDevices();
    if (m_onDeviceChange) m_onDeviceChange();
    return S_OK;
}

STDMETHODIMP AudioDeviceManager::OnDefaultDeviceChanged(EDataFlow, ERole, LPCWSTR) {
    refreshDevices();
    if (m_onDeviceChange) m_onDeviceChange();
    return S_OK;
}

STDMETHODIMP AudioDeviceManager::OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) {
    return S_OK;
}
