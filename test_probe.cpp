#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <functiondiscoverykeys_devpkey.h>
#include <avrt.h>
#include <iostream>
#include <string>

int main() {
    std::cout << "[SoundMixer] Checking audio environment..." << std::endl;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        std::cerr << "CoInitializeEx failed: " << hr << std::endl;
        return 1;
    }

    IMMDeviceEnumerator* pEnum = nullptr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnum);
    if (FAILED(hr)) {
        std::cerr << "CoCreateInstance MMDeviceEnumerator failed: " << hr << std::endl;
        CoUninitialize();
        return 1;
    }

    // List Render endpoints
    std::cout << "\n--- PLAYBACK (RENDER) DEVICES ---" << std::endl;
    IMMDeviceCollection* pRenderCol = nullptr;
    if (SUCCEEDED(pEnum->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pRenderCol))) {
        UINT count = 0;
        pRenderCol->GetCount(&count);
        for (UINT i = 0; i < count; ++i) {
            IMMDevice* pDev = nullptr;
            if (SUCCEEDED(pRenderCol->Item(i, &pDev))) {
                IPropertyStore* pProps = nullptr;
                if (SUCCEEDED(pDev->OpenPropertyStore(STGM_READ, &pProps))) {
                    PROPVARIANT varName;
                    PropVariantInit(&varName);
                    pProps->GetValue(PKEY_Device_FriendlyName, &varName);
                    LPWSTR id = nullptr;
                    pDev->GetId(&id);
                    wprintf(L"[%u] %s (ID: %s)\n", i, varName.pwszVal, id);
                    CoTaskMemFree(id);
                    PropVariantClear(&varName);
                    pProps->Release();
                }
                pDev->Release();
            }
        }
        pRenderCol->Release();
    }

    // List Capture endpoints
    std::cout << "\n--- RECORDING (CAPTURE) DEVICES ---" << std::endl;
    IMMDeviceCollection* pCaptureCol = nullptr;
    if (SUCCEEDED(pEnum->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &pCaptureCol))) {
        UINT count = 0;
        pCaptureCol->GetCount(&count);
        for (UINT i = 0; i < count; ++i) {
            IMMDevice* pDev = nullptr;
            if (SUCCEEDED(pCaptureCol->Item(i, &pDev))) {
                IPropertyStore* pProps = nullptr;
                if (SUCCEEDED(pDev->OpenPropertyStore(STGM_READ, &pProps))) {
                    PROPVARIANT varName;
                    PropVariantInit(&varName);
                    pProps->GetValue(PKEY_Device_FriendlyName, &varName);
                    LPWSTR id = nullptr;
                    pDev->GetId(&id);
                    wprintf(L"[%u] %s (ID: %s)\n", i, varName.pwszVal, id);
                    CoTaskMemFree(id);
                    PropVariantClear(&varName);
                    pProps->Release();
                }
                pDev->Release();
            }
        }
        pCaptureCol->Release();
    }

    pEnum->Release();

    // Check Process Loopback symbol presence
    AUDIOCLIENT_ACTIVATION_PARAMS params = {};
    params.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
    params.ProcessLoopbackParams.TargetProcessId = GetCurrentProcessId();
    params.ProcessLoopbackParams.ProcessLoopbackMode = PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;
    std::cout << "\nProcess Loopback API struct compiled OK! TargetProcessId=" << params.ProcessLoopbackParams.TargetProcessId << std::endl;

    CoUninitialize();
    return 0;
}
