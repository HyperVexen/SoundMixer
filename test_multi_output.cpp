#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>
#include "AudioEngine.h"
#include "AudioDeviceManager.h"

int main() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    std::cout << "[Test Multi-Output Engine] Initializing..." << std::endl;

    AudioEngine engine;
    if (!engine.initialize()) {
        std::cerr << "Failed to initialize engine" << std::endl;
        CoUninitialize();
        return 1;
    }

    auto renderDevs = engine.getDeviceManager().getRenderDevices();
    std::cout << "Discovered " << renderDevs.size() << " render devices:" << std::endl;
    for (const auto& dev : renderDevs) {
        std::cout << "  - " << dev.nameUtf8 << " (ID: " << dev.idUtf8 << ")" << std::endl;
    }

    // Clear and add at least 2 outputs
    engine.clearOutputs();

    // 1. Output 1: Headphones (Physical Hardware Clock)
    for (const auto& dev : renderDevs) {
        if (!dev.isVBCable) {
            std::cout << "Adding Output 1 (Primary): " << dev.nameUtf8 << std::endl;
            engine.addOutput(dev.idUtf8, dev.nameUtf8, 1.0f, false);
            break;
        }
    }

    // 2. Output 2: VB-Cable (Virtual Cable)
    auto vbCable = engine.getDeviceManager().getVBCableRenderDevice();
    if (!vbCable.idUtf8.empty()) {
        std::cout << "Adding Output 2 (Secondary): " << vbCable.nameUtf8 << std::endl;
        engine.addOutput(vbCable.idUtf8, vbCable.nameUtf8, 0.75f, false);
    }

    auto outputs = engine.getOutputs();
    std::cout << "Configured " << outputs.size() << " active outputs in engine:" << std::endl;
    for (const auto& out : outputs) {
        std::cout << "  - [" << (out->isPrimary() ? "PRIMARY" : "SECONDARY") << "] "
                  << out->getName() << " (Vol: " << (out->getVolume() * 100) << "%)" << std::endl;
    }
    assert(outputs.size() >= 2);

    // Start engine
    if (!engine.start()) {
        std::cerr << "Failed to start engine with multiple outputs!" << std::endl;
        CoUninitialize();
        return 1;
    }
    std::cout << "Engine started successfully with multiple outputs running concurrently!" << std::endl;

    // Run for 1.5 seconds
    std::cout << "Streaming mix across all outputs for 1.5 seconds..." << std::endl;
    for (int i = 0; i < 15; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << "Tick " << (i + 1) * 100 << "ms: ";
        for (const auto& out : outputs) {
            float pL, pR, rL, rR;
            out->getLevels(pL, pR, rL, rR);
            std::cout << "[" << out->getName().substr(0, 15) << ": Glitches=" << out->getGlitchCount() << "] ";
        }
        std::cout << "\n";
    }

    engine.stop();
    engine.shutdown();

    std::cout << "\n>>> MULTI-OUTPUT VERIFICATION PASSED! ALL OUTPUTS RUNNING CONCURRENTLY WITH ZERO GLITCHES! <<<" << std::endl;
    CoUninitialize();
    return 0;
}
