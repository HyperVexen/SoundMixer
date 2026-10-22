#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <unordered_set>

struct AppProcessInfo {
    DWORD pid = 0;
    std::string exeName;
    std::string windowTitle;
    std::string fullPath;
    bool hasAudioSession = false;
    bool isBrowser = false;
};

class ProcessEnumerator {
public:
    static std::vector<AppProcessInfo> getRunningApplications();
    static std::unordered_set<DWORD> getAudioSessionProcessIds();
    static DWORD findRootProcessId(DWORD pid);
    static std::vector<DWORD> getProcessTreePids(DWORD rootPid);
};
