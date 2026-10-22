#include <windows.h>
#include <d3d11.h>
#include <tchar.h>
#include <iostream>
#include <memory>

#include "AudioEngine.h"
#include "MicCaptureSource.h"
#include "ProcessCaptureSource.h"
#include "ConfigManager.h"
#include "TrayIcon.h"
#include "UI.h"

#include "../third_party/imgui/imgui.h"
#include "../third_party/imgui/backends/imgui_impl_win32.h"
#include "../third_party/imgui/backends/imgui_impl_dx11.h"

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static UINT g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
static TrayIcon g_trayIcon;
static HWND g_hwnd = nullptr;

static bool CreateDeviceD3D(HWND hWnd);
static void CleanupDeviceD3D();
static void CreateRenderTarget();
static void CleanupRenderTarget();
static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // Enable DPI awareness
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    // Register window class
    WNDCLASSEXW wc = {
        sizeof(wc),
        CS_CLASSDC,
        WndProc,
        0L,
        0L,
        hInstance,
        LoadIcon(nullptr, IDI_APPLICATION),
        LoadCursor(nullptr, IDC_ARROW),
        nullptr,
        nullptr,
        L"SoundMixerWindowClass",
        LoadIcon(nullptr, IDI_APPLICATION)
    };
    RegisterClassExW(&wc);

    // Create application window
    int windowWidth = 780;
    int windowHeight = 580;
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenWidth - windowWidth) / 2;
    int posY = (screenHeight - windowHeight) / 2;

    HWND hwnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"SoundMixer - Low-Latency Audio Mixer",
        WS_OVERLAPPEDWINDOW,
        posX,
        posY,
        windowWidth,
        windowHeight,
        nullptr,
        nullptr,
        wc.hInstance,
        nullptr
    );

    g_hwnd = hwnd;

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        if (SUCCEEDED(hrCo)) CoUninitialize();
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Initialize Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr; // Don't create imgui.ini

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Initialize Audio Engine
    AudioEngine audioEngine;
    audioEngine.initialize();

    // Setup Tray Icon
    g_trayIcon.initialize(hwnd, wc.hIcon, L"SoundMixer - Low-Latency Audio Mixer");
    g_trayIcon.setOnRestoreCallback([hwnd]() {
        ShowWindow(hwnd, SW_RESTORE);
        SetForegroundWindow(hwnd);
    });
    g_trayIcon.setOnExitCallback([hwnd]() {
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
    });

    // Try to load saved config; if none, auto-add default mic
    AppConfig savedConfig;
    std::string configPath = ConfigManager::getDefaultConfigPath();
    if (ConfigManager::loadConfig(configPath, savedConfig)) {
        ConfigManager::applyToEngine(audioEngine, savedConfig);
    } else {
        auto defMic = audioEngine.getDeviceManager().getDefaultCaptureDevice();
        if (!defMic.idUtf8.empty()) {
            auto micSrc = std::make_shared<MicCaptureSource>(defMic.idUtf8, defMic.nameUtf8);
            if (micSrc->start()) {
                audioEngine.getMixer().addSource(micSrc);
            }
        }
    }

    // Start Audio Engine
    audioEngine.start();

    // Setup Voice Preset and Action hotkey callbacks
    audioEngine.getSoundboard().setOnVoicePresetHotkey([&audioEngine](int presetIndex) {
        if (presetIndex >= 0 && presetIndex <= 6) {
            VoicePreset preset = static_cast<VoicePreset>(presetIndex);
            for (auto& src : audioEngine.getMixer().getSources()) {
                if (src->getType() == SourceType::Microphone) {
                    auto mic = std::dynamic_pointer_cast<MicCaptureSource>(src);
                    if (mic) {
                        mic->getVoiceProcessor().setPreset(preset);
                    }
                }
            }
        }
    });

    audioEngine.getSoundboard().setOnMicMuteHotkey([&audioEngine]() {
        for (auto& src : audioEngine.getMixer().getSources()) {
            if (src->getType() == SourceType::Microphone) {
                src->setMuted(!src->isMuted());
            }
        }
    });

    audioEngine.getSoundboard().setOnSidetoneHotkey([&audioEngine]() {
        bool current = audioEngine.getMixer().isSidetoneEnabled();
        audioEngine.getMixer().setSidetoneEnabled(!current);
    });

    // Register global macro hotkeys for Soundboard, Voice Presets, Mic Mute, and Sidetone
    audioEngine.getSoundboard().registerHotkeys(hwnd);

    // Create UI Controller
    SoundMixerUI ui(audioEngine);

    // Main loop
    bool done = false;
    while (!done) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) {
                done = true;
            } else if (msg.message == WM_HOTKEY) {
                audioEngine.getSoundboard().handleHotkey((int)msg.wParam);
            }
        }
        if (done) break;

        // Handle window resize
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        // Check if user clicked Minimize to Tray in UI
        if (ui.shouldMinimizeToTray()) {
            ui.clearMinimizeToTrayRequest();
            ShowWindow(hwnd, SW_HIDE);
            g_trayIcon.showBalloon(L"SoundMixer Running", L"SoundMixer is minimized to the system tray.");
        }

        // Start Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Render our Audio Mixer UI
        ui.render();

        // Rendering
        ImGui::Render();
        const float clearColor[4] = { 0.08f, 0.09f, 0.11f, 1.00f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clearColor);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0); // VSync
    }

    // Auto-save configuration on exit
    auto finalConfig = ConfigManager::extractFromEngine(audioEngine);
    ConfigManager::saveConfig(configPath, finalConfig);

    audioEngine.getSoundboard().unregisterHotkeys(hwnd);

    // Stop and shutdown audio
    audioEngine.stop();
    audioEngine.shutdown();

    // Remove tray icon
    g_trayIcon.remove();

    // Cleanup ImGui
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    // Cleanup DirectX
    CleanupRenderTarget();
    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    if (SUCCEEDED(hrCo)) {
        CoUninitialize();
    }

    return 0;
}

static bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0,
    };
    HRESULT res = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        featureLevelArray,
        2,
        D3D11_SDK_VERSION,
        &sd,
        &g_pSwapChain,
        &g_pd3dDevice,
        &featureLevel,
        &g_pd3dDeviceContext
    );

    if (res == DXGI_ERROR_UNSUPPORTED) {
        res = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            createDeviceFlags,
            featureLevelArray,
            2,
            D3D11_SDK_VERSION,
            &sd,
            &g_pSwapChain,
            &g_pd3dDevice,
            &featureLevel,
            &g_pd3dDeviceContext
        );
    }
    if (res != S_OK) return false;

    CreateRenderTarget();
    return true;
}

static void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

static void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer) {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

static void CleanupRenderTarget() {
    if (g_mainRenderTargetView) {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }
}

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) {
            return 0;
        }
        g_ResizeWidth = (UINT)LOWORD(lParam);
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;

    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_MINIMIZE) {
            ShowWindow(hWnd, SW_HIDE);
            g_trayIcon.showBalloon(L"SoundMixer Running", L"SoundMixer is minimized to the system tray.");
            return 0;
        }
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;

    case WM_TRAYICON:
        g_trayIcon.handleTrayMessage(wParam, lParam);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}
