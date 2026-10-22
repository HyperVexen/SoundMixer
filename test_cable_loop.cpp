#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <thread>
#include <chrono>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int main() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    IMMDeviceEnumerator* pEnum = nullptr;
    CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnum);
    if (!pEnum) {
        std::cerr << "Failed to create MMDeviceEnumerator\n";
        return 1;
    }

    // Find VB-Cable render endpoint
    IMMDevice* pRenderDev = nullptr;
    IMMDeviceCollection* pRenderCol = nullptr;
    pEnum->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pRenderCol);
    UINT renderCount = 0;
    pRenderCol->GetCount(&renderCount);
    for (UINT i = 0; i < renderCount; ++i) {
        IMMDevice* dev = nullptr;
        pRenderCol->Item(i, &dev);
        IPropertyStore* props = nullptr;
        dev->OpenPropertyStore(STGM_READ, &props);
        PROPVARIANT var;
        PropVariantInit(&var);
        props->GetValue(PKEY_Device_FriendlyName, &var);
        if (var.pwszVal && wcsstr(var.pwszVal, L"VB-Audio Virtual Cable")) {
            pRenderDev = dev;
            pRenderDev->AddRef();
            wprintf(L"Selected Render Device: %s\n", var.pwszVal);
            PropVariantClear(&var);
            props->Release();
            dev->Release();
            break;
        }
        PropVariantClear(&var);
        props->Release();
        dev->Release();
    }
    pRenderCol->Release();

    // Find VB-Cable capture endpoint
    IMMDevice* pCaptureDev = nullptr;
    IMMDeviceCollection* pCapCol = nullptr;
    pEnum->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &pCapCol);
    UINT capCount = 0;
    pCapCol->GetCount(&capCount);
    for (UINT i = 0; i < capCount; ++i) {
        IMMDevice* dev = nullptr;
        pCapCol->Item(i, &dev);
        IPropertyStore* props = nullptr;
        dev->OpenPropertyStore(STGM_READ, &props);
        PROPVARIANT var;
        PropVariantInit(&var);
        props->GetValue(PKEY_Device_FriendlyName, &var);
        if (var.pwszVal && wcsstr(var.pwszVal, L"CABLE Output")) {
            pCaptureDev = dev;
            pCaptureDev->AddRef();
            wprintf(L"Selected Capture Device: %s\n", var.pwszVal);
            PropVariantClear(&var);
            props->Release();
            dev->Release();
            break;
        }
        PropVariantClear(&var);
        props->Release();
        dev->Release();
    }
    pCapCol->Release();

    if (!pRenderDev || !pCaptureDev) {
        std::cerr << "Could not find VB-Cable render or capture endpoints!\n";
        return 1;
    }

    // Initialize Render Client
    IAudioClient* pRenderClient = nullptr;
    pRenderDev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pRenderClient);
    WAVEFORMATEX* pRenderWfx = nullptr;
    pRenderClient->GetMixFormat(&pRenderWfx);
    std::cout << "Render MixFormat: " << pRenderWfx->nSamplesPerSec << " Hz, " << pRenderWfx->nChannels << " channels, " << pRenderWfx->wBitsPerSample << " bits\n";

    REFERENCE_TIME hnsBuffer = 200000; // 20 ms buffer
    pRenderClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, hnsBuffer, 0, pRenderWfx, nullptr);
    IAudioRenderClient* pRenderData = nullptr;
    pRenderClient->GetService(__uuidof(IAudioRenderClient), (void**)&pRenderData);

    // Initialize Capture Client
    IAudioClient* pCaptureClient = nullptr;
    pCaptureDev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pCaptureClient);
    WAVEFORMATEX* pCaptureWfx = nullptr;
    pCaptureClient->GetMixFormat(&pCaptureWfx);
    std::cout << "Capture MixFormat: " << pCaptureWfx->nSamplesPerSec << " Hz, " << pCaptureWfx->nChannels << " channels, " << pCaptureWfx->wBitsPerSample << " bits\n";

    pCaptureClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, hnsBuffer, 0, pCaptureWfx, nullptr);
    IAudioCaptureClient* pCaptureData = nullptr;
    pCaptureClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureData);

    pRenderClient->Start();
    pCaptureClient->Start();

    UINT32 bufferFrameCount = 0;
    pRenderClient->GetBufferSize(&bufferFrameCount);

    double phase = 0.0;
    double freq = 440.0;
    double phaseInc = 2.0 * M_PI * freq / pRenderWfx->nSamplesPerSec;

    std::cout << "\nPlaying 440 Hz tone for 1.5 seconds and capturing from CABLE Output..." << std::endl;

    double maxCapturedAmp = 0.0;
    UINT64 totalCapturedFrames = 0;

    auto startTime = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count() < 1500) {
        // Render
        UINT32 padding = 0;
        pRenderClient->GetCurrentPadding(&padding);
        UINT32 framesAvailable = bufferFrameCount - padding;
        if (framesAvailable > 0) {
            BYTE* pData = nullptr;
            pRenderData->GetBuffer(framesAvailable, &pData);
            float* fData = (float*)pData;
            for (UINT32 f = 0; f < framesAvailable; ++f) {
                float sample = (float)(0.5 * sin(phase));
                phase += phaseInc;
                if (phase >= 2.0 * M_PI) phase -= 2.0 * M_PI;
                for (WORD c = 0; c < pRenderWfx->nChannels; ++c) {
                    fData[f * pRenderWfx->nChannels + c] = sample;
                }
            }
            pRenderData->ReleaseBuffer(framesAvailable, 0);
        }

        // Capture
        UINT32 packetLength = 0;
        pCaptureData->GetNextPacketSize(&packetLength);
        while (packetLength > 0) {
            BYTE* pData = nullptr;
            UINT32 numFramesRead = 0;
            DWORD flags = 0;
            pCaptureData->GetBuffer(&pData, &numFramesRead, &flags, nullptr, nullptr);
            float* fData = (float*)pData;
            if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT) && fData) {
                for (UINT32 f = 0; f < numFramesRead; ++f) {
                    for (WORD c = 0; c < pCaptureWfx->nChannels; ++c) {
                        double val = std::abs((double)fData[f * pCaptureWfx->nChannels + c]);
                        if (val > maxCapturedAmp) maxCapturedAmp = val;
                    }
                }
            }
            totalCapturedFrames += numFramesRead;
            pCaptureData->ReleaseBuffer(numFramesRead);
            pCaptureData->GetNextPacketSize(&packetLength);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    pRenderClient->Stop();
    pCaptureClient->Stop();

    std::cout << "\nResults:" << std::endl;
    std::cout << "Total captured frames: " << totalCapturedFrames << std::endl;
    std::cout << "Max captured amplitude from CABLE Output: " << maxCapturedAmp << std::endl;

    if (maxCapturedAmp > 0.1) {
        std::cout << ">>> SUCCESS! Test tone rendered to CABLE Input was successfully captured from CABLE Output! <<<" << std::endl;
    } else {
        std::cout << "Warning: amplitude low or not received." << std::endl;
    }

    CoTaskMemFree(pRenderWfx);
    CoTaskMemFree(pCaptureWfx);
    pRenderData->Release();
    pCaptureData->Release();
    pRenderClient->Release();
    pCaptureClient->Release();
    pRenderDev->Release();
    pCaptureDev->Release();
    pEnum->Release();
    CoUninitialize();
    return 0;
}
