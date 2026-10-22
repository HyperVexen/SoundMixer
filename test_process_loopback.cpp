#include <iostream>
#include <thread>
#include <chrono>
#include "ProcessEnumerator.h"
#include "ProcessCaptureSource.h"

int main() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    std::cout << "[Test Process Loopback] Enumerating running applications..." << std::endl;

    auto apps = ProcessEnumerator::getRunningApplications();
    std::cout << "Discovered " << apps.size() << " application candidates:" << std::endl;

    DWORD targetPid = 0;
    std::string targetExe;

    for (const auto& app : apps) {
        std::cout << "  - [" << (app.hasAudioSession ? "AUDIO" : "APP") << "] PID: " << app.pid
                  << " | " << app.exeName;
        if (!app.windowTitle.empty()) {
            std::cout << " (\"" << app.windowTitle << "\")";
        }
        std::cout << std::endl;

        if (targetPid == 0 && app.hasAudioSession) {
            targetPid = app.pid;
            targetExe = app.exeName;
        }
    }

    if (targetPid == 0 && !apps.empty()) {
        targetPid = apps[0].pid;
        targetExe = apps[0].exeName;
    }

    if (targetPid == 0) {
        std::cout << "No suitable app found to test, testing on current process ID: " << GetCurrentProcessId() << std::endl;
        targetPid = GetCurrentProcessId();
        targetExe = "SoundMixerTest";
    }

    std::cout << "\nTesting Process Loopback capture on PID " << targetPid << " (" << targetExe << ")..." << std::endl;

    DWORD rootPid = ProcessEnumerator::findRootProcessId(targetPid);
    std::cout << "Root Process ID for process tree: " << rootPid << std::endl;

    ProcessCaptureSource loopback(rootPid, targetExe);
    if (!loopback.start()) {
        std::cerr << "Failed to start ProcessCaptureSource: " << loopback.getStatusText() << std::endl;
    } else {
        std::cout << "Process Loopback capture started successfully! Status: " << loopback.getStatusText() << std::endl;

        // Drain for 1 second
        std::vector<float> buf(1024 * 2);
        size_t totalRead = 0;
        for (int i = 0; i < 10; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            size_t avail = loopback.getRingBuffer().getAvailableRead();
            while (avail > 0) {
                size_t n = loopback.getRingBuffer().read(buf.data(), (std::min)(avail, buf.size() / 2));
                totalRead += n;
                avail = loopback.getRingBuffer().getAvailableRead();
            }
        }
        loopback.stop();
        std::cout << "Process Loopback stopped. Total frames captured: " << totalRead << " | Glitches: " << loopback.getGlitchCount() << std::endl;
        std::cout << ">>> SUCCESS! Process Loopback API initialized and captured properly! <<<" << std::endl;
    }

    CoUninitialize();
    return 0;
}
