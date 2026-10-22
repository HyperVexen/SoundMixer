#include "TrayIcon.h"

TrayIcon::TrayIcon() = default;

TrayIcon::~TrayIcon() {
    remove();
}

bool TrayIcon::initialize(HWND hwnd, HICON hIcon, const std::wstring& tooltip) {
    m_hwnd = hwnd;
    ZeroMemory(&m_nid, sizeof(m_nid));
    m_nid.cbSize = sizeof(NOTIFYICONDATAW);
    m_nid.hWnd = hwnd;
    m_nid.uID = 1;
    m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAYICON;
    m_nid.hIcon = hIcon ? hIcon : LoadIcon(nullptr, IDI_APPLICATION);
    wcsncpy_s(m_nid.szTip, tooltip.c_str(), _TRUNCATE);

    m_installed = Shell_NotifyIconW(NIM_ADD, &m_nid) == TRUE;
    return m_installed;
}

void TrayIcon::remove() {
    if (m_installed) {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
        m_installed = false;
    }
}

void TrayIcon::showBalloon(const std::wstring& title, const std::wstring& message) {
    if (!m_installed) return;
    m_nid.uFlags |= NIF_INFO;
    wcsncpy_s(m_nid.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(m_nid.szInfo, message.c_str(), _TRUNCATE);
    m_nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &m_nid);
}

void TrayIcon::handleTrayMessage(WPARAM, LPARAM lParam) {
    if (lParam == WM_LBUTTONDBLCLK || lParam == WM_LBUTTONUP) {
        if (m_onRestore) m_onRestore();
    } else if (lParam == WM_RBUTTONUP) {
        HMENU hMenu = CreatePopupMenu();
        AppendMenuW(hMenu, MF_STRING, 1001, L"Open SoundMixer");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, 1002, L"Exit");

        POINT pt;
        GetCursorPos(&pt);
        SetForegroundWindow(m_hwnd);
        int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, m_hwnd, nullptr);
        DestroyMenu(hMenu);

        if (cmd == 1001 && m_onRestore) {
            m_onRestore();
        } else if (cmd == 1002 && m_onExit) {
            m_onExit();
        }
    }
}
