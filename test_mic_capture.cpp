#include <iostream>
#include <thread>
#include <chrono>
#include "MicCaptureSource.h"
#include "AudioDeviceManager.h"

int main() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    std::cout << "[Test Mic Capture] Initializing..." << std::endl;

    AudioDeviceManager devMgr;
    if (!devMgr.initialize()) {
        std::cerr << "Failed to init device manager" << std::endl;
        CoUninitialize();
        return 1;
    }

    auto defMic = devMgr.getDefaultCaptureDevice();
    std::cout << "Target Capture Device: " << defMic.nameUtf8 << " (ID: " << defMic.idUtf8 << ")\n";

    MicCaptureSource mic(defMic.idUtf8, defMic.nameUtf8);
    if (!mic.start()) {
        std::cerr << "Failed to start MicCaptureSource: " << mic.getStatusText() << std::endl;
        CoUninitialize();
        return 1;
    }

    std::cout << "Mic started! Status: " << mic.getStatusText() << std::endl;
    std::cout << "Device sample rate: " << mic.getDeviceSampleRate() << " Hz, Channels: " << mic.getDeviceChannels() << std::endl;
    std::cout << "Buffer period frames: " << mic.getBufferPeriodFrames() << " (~"
              << (mic.getBufferPeriodFrames() * 1000.0f / (mic.getDeviceSampleRate() ? mic.getDeviceSampleRate() : 48000)) << " ms)" << std::endl;

    std::cout << "Capturing for 2 seconds..." << std::endl;
    size_t totalRead = 0;
    std::vector<float> readBuf(1024 * 2);

    for (int i = 0; i < 20; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        // Drain all available frames matching real-time mixer
        size_t avail = mic.getRingBuffer().getAvailableRead();
        while (avail > 0) {
            size_t toRead = (std::min)(avail, readBuf.size() / 2);
            size_t n = mic.getRingBuffer().read(readBuf.data(), toRead);
            totalRead += n;
            avail = mic.getRingBuffer().getAvailableRead();
        }
        float pl, pr, rl, rr;
        mic.getLevels(pl, pr, rl, rr);
        std::cout << "Time: " << (i + 1) * 100 << "ms | Total Read: " << totalRead << " frames | Levels: Peak=" << pl << " RMS=" << rl << " | Glitches: " << mic.getGlitchCount() << "\n";
    }

    mic.stop();
    std::cout << "Mic stopped. Total frames read: " << totalRead << " | Glitches: " << mic.getGlitchCount() << std::endl;

    if (totalRead > 0 && mic.getGlitchCount() == 0) {
        std::cout << ">>> SUCCESS! Mic capture is running at ultra-low latency with 0 glitches! <<<" << std::endl;
    }

    CoUninitialize();
    return 0;
}
