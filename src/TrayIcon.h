#pragma once

#include <windows.h>
#include <shellapi.h>
#include <string>
#include <functional>

#define WM_TRAYICON (WM_USER + 101)

class TrayIcon {
public:
    TrayIcon();
    ~TrayIcon();

    bool initialize(HWND hwnd, HICON hIcon, const std::wstring& tooltip);
    void remove();
    void showBalloon(const std::wstring& title, const std::wstring& message);
    void handleTrayMessage(WPARAM wParam, LPARAM lParam);

    void setOnRestoreCallback(std::function<void()> cb) { m_onRestore = cb; }
    void setOnExitCallback(std::function<void()> cb) { m_onExit = cb; }

private:
    HWND m_hwnd = nullptr;
    NOTIFYICONDATAW m_nid = {};
    bool m_installed = false;
    std::function<void()> m_onRestore;
    std::function<void()> m_onExit;
};
