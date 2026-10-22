#include "ProcessEnumerator.h"
#include <tlhelp32.h>
#include <psapi.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <algorithm>
#include <unordered_map>

static std::string wideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), &result[0], size, nullptr, nullptr);
    return result;
}

static std::string toLower(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return lower;
}

std::unordered_set<DWORD> ProcessEnumerator::getAudioSessionProcessIds() {
    std::unordered_set<DWORD> audioPids;

    IMMDeviceEnumerator* pEnum = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnum))) {
        return audioPids;
    }

    // Check all active render endpoints
    IMMDeviceCollection* pCol = nullptr;
    if (SUCCEEDED(pEnum->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCol))) {
        UINT count = 0;
        pCol->GetCount(&count);
        for (UINT i = 0; i < count; ++i) {
            IMMDevice* pDev = nullptr;
            if (SUCCEEDED(pCol->Item(i, &pDev))) {
                IAudioSessionManager2* pMgr = nullptr;
                if (SUCCEEDED(pDev->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void**)&pMgr))) {
                    IAudioSessionEnumerator* pSesEnum = nullptr;
                    if (SUCCEEDED(pMgr->GetSessionEnumerator(&pSesEnum))) {
                        int sessionCount = 0;
                        pSesEnum->GetCount(&sessionCount);
                        for (int s = 0; s < sessionCount; ++s) {
                            IAudioSessionControl* pControl = nullptr;
                            if (SUCCEEDED(pSesEnum->GetSession(s, &pControl))) {
                                IAudioSessionControl2* pControl2 = nullptr;
                                if (SUCCEEDED(pControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pControl2))) {
                                    DWORD pid = 0;
                                    if (SUCCEEDED(pControl2->GetProcessId(&pid)) && pid != 0) {
                                        audioPids.insert(pid);
                                    }
                                    pControl2->Release();
                                }
                                pControl->Release();
                            }
                        }
                        pSesEnum->Release();
                    }
                    pMgr->Release();
                }
                pDev->Release();
            }
        }
        pCol->Release();
    }
    pEnum->Release();

    return audioPids;
}

struct WindowEnumContext {
    std::unordered_map<DWORD, std::wstring> windowTitles;
};

static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    if (!IsWindowVisible(hwnd)) return TRUE;

    // Check if window has text
    int len = GetWindowTextLengthW(hwnd);
    if (len == 0) return TRUE;

    // Skip windows without title bar or minimization capability
    LONG style = GetWindowLongW(hwnd, GWL_STYLE);
    if (!(style & WS_VISIBLE)) return TRUE;

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) return TRUE;

    std::wstring title(len + 1, L'\0');
    GetWindowTextW(hwnd, &title[0], len + 1);
    title.resize(len);

    auto* ctx = reinterpret_cast<WindowEnumContext*>(lParam);
    if (ctx->windowTitles.find(pid) == ctx->windowTitles.end() && !title.empty()) {
        ctx->windowTitles[pid] = title;
    }
    return TRUE;
}

std::vector<AppProcessInfo> ProcessEnumerator::getRunningApplications() {
    std::vector<AppProcessInfo> apps;
    auto audioPids = getAudioSessionProcessIds();

    WindowEnumContext winCtx;
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&winCtx));

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) {
        return apps;
    }

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);

    DWORD currentPid = GetCurrentProcessId();
    DWORD currentSessionId = 0;
    ProcessIdToSessionId(currentPid, &currentSessionId);

    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (pe.th32ProcessID <= 4 || pe.th32ProcessID == currentPid) {
                continue; // Skip system idle, system, and self
            }

            std::wstring exeW = pe.szExeFile;
            std::string exeName = wideToUtf8(exeW);
            std::string exeLower = toLower(exeName);

            // Filter out internal background system processes
            if (exeLower == "svchost.exe" || exeLower == "services.exe" ||
                exeLower == "lsass.exe" || exeLower == "csrss.exe" ||
                exeLower == "smss.exe" || exeLower == "wininit.exe" ||
                exeLower == "registry" || exeLower == "fontdrvhost.exe" ||
                exeLower == "dwm.exe" || exeLower == "sihost.exe" ||
                exeLower == "taskhostw.exe" || exeLower == "searchhost.exe") {
                continue;
            }

            DWORD procSessionId = 0;
            if (!ProcessIdToSessionId(pe.th32ProcessID, &procSessionId) || procSessionId != currentSessionId) {
                continue; // Only inspect processes in the user's desktop session
            }

            bool hasAudio = (audioPids.find(pe.th32ProcessID) != audioPids.end());
            auto itTitle = winCtx.windowTitles.find(pe.th32ProcessID);
            bool hasWindow = (itTitle != winCtx.windowTitles.end());

            // Check if browser
            bool isBrowser = (exeLower == "chrome.exe" || exeLower == "msedge.exe" ||
                              exeLower == "firefox.exe" || exeLower == "brave.exe" ||
                              exeLower == "opera.exe");

            // Filter out common helper or headless system host binaries
            if (exeLower == "conhost.exe" || exeLower == "backgroundtaskhost.exe" ||
                exeLower == "runtimebroker.exe" || exeLower == "smartscreen.exe" ||
                exeLower == "ctfmon.exe" || exeLower == "searchapp.exe" ||
                exeLower == "startmenuexperiencehost.exe") {
                continue;
            }

            // Include if user session process
            AppProcessInfo info;
            info.pid = pe.th32ProcessID;
            info.exeName = exeName;
            if (hasWindow) {
                info.windowTitle = wideToUtf8(itTitle->second);
            }
            info.hasAudioSession = hasAudio;
            info.isBrowser = isBrowser;

            // Try to get full path
            HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
            if (hProc) {
                WCHAR pathBuf[MAX_PATH];
                DWORD pathLen = MAX_PATH;
                if (QueryFullProcessImageNameW(hProc, 0, pathBuf, &pathLen)) {
                    info.fullPath = wideToUtf8(std::wstring(pathBuf, pathLen));
                }
                CloseHandle(hProc);
            }

            apps.push_back(info);
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);

    // Sort: apps with active audio first, then alphabetical by name
    std::sort(apps.begin(), apps.end(), [](const AppProcessInfo& a, const AppProcessInfo& b) {
        if (a.hasAudioSession != b.hasAudioSession) {
            return a.hasAudioSession > b.hasAudioSession;
        }
        return a.exeName < b.exeName;
    });

    return apps;
}

DWORD ProcessEnumerator::findRootProcessId(DWORD pid) {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return pid;

    std::unordered_map<DWORD, DWORD> parentMap;
    std::unordered_map<DWORD, std::wstring> nameMap;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);

    if (Process32FirstW(hSnap, &pe)) {
        do {
            parentMap[pe.th32ProcessID] = pe.th32ParentProcessID;
            nameMap[pe.th32ProcessID] = pe.szExeFile;
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);

    // Traverse up while parent has the same executable name
    DWORD current = pid;
    auto itName = nameMap.find(current);
    if (itName == nameMap.end()) return pid;
    std::wstring exe = itName->second;

    while (true) {
        auto itParent = parentMap.find(current);
        if (itParent == parentMap.end()) break;
        DWORD parent = itParent->second;
        if (parent == 0 || parent == current) break;

        auto itParentName = nameMap.find(parent);
        if (itParentName == nameMap.end() || itParentName->second != exe) break;

        current = parent;
    }

    return current;
}

std::vector<DWORD> ProcessEnumerator::getProcessTreePids(DWORD rootPid) {
    std::vector<DWORD> pids = { rootPid };

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return pids;

    std::unordered_map<DWORD, std::vector<DWORD>> childrenMap;
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);

    if (Process32FirstW(hSnap, &pe)) {
        do {
            childrenMap[pe.th32ParentProcessID].push_back(pe.th32ProcessID);
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);

    // Breadth-first search
    size_t idx = 0;
    while (idx < pids.size()) {
        DWORD curr = pids[idx++];
        auto it = childrenMap.find(curr);
        if (it != childrenMap.end()) {
            for (DWORD child : it->second) {
                pids.push_back(child);
            }
        }
    }

    return pids;
}
