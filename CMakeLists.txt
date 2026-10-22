cmake_minimum_required(VERSION 3.20)
project(SoundMixer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files for Dear ImGui
set(IMGUI_SOURCES
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_win32.cpp
    third_party/imgui/backends/imgui_impl_dx11.cpp
)

# Application source files
set(APP_SOURCES
    src/AudioDeviceManager.cpp
    src/AudioOutput.cpp
    src/AudioMixer.cpp
    src/AudioEngine.cpp
    src/AudioFileReader.cpp
    src/SoundEffect.cpp
    src/SoundboardManager.cpp
    src/VoiceProcessor.cpp
    src/MicCaptureSource.cpp
    src/ProcessCaptureSource.cpp
    src/ProcessEnumerator.cpp
    src/VirtualDeviceBridge.cpp
    src/ConfigManager.cpp
    src/TrayIcon.cpp
    src/UI.cpp
    src/main.cpp
    ${IMGUI_SOURCES}
)

add_executable(SoundMixer WIN32 ${APP_SOURCES})

target_include_directories(SoundMixer PRIVATE
    src
    third_party
    third_party/imgui
    third_party/imgui/backends
)

target_link_libraries(SoundMixer PRIVATE
    d3d11
    dxgi
    d3dcompiler
    ole32
    propsys
    avrt
    mmdevapi
    windowsapp
    shell32
    user32
    gdi32
    advapi32
    comdlg32
    mfplat
    mfreadwrite
    mfuuid
)

# Test targets
add_executable(test_probe test_probe.cpp)
target_link_libraries(test_probe PRIVATE ole32 propsys avrt mmdevapi)

add_executable(test_cable_loop test_cable_loop.cpp)
target_link_libraries(test_cable_loop PRIVATE ole32 propsys avrt mmdevapi)

add_executable(test_mixer_core test_mixer_core.cpp src/AudioMixer.cpp src/SoundboardManager.cpp src/SoundEffect.cpp src/AudioFileReader.cpp)
target_include_directories(test_mixer_core PRIVATE src third_party)
target_link_libraries(test_mixer_core PRIVATE ole32 propsys avrt mmdevapi mfplat mfreadwrite mfuuid)

add_executable(test_mic_capture test_mic_capture.cpp src/MicCaptureSource.cpp src/AudioDeviceManager.cpp src/VoiceProcessor.cpp)
target_include_directories(test_mic_capture PRIVATE src)
target_link_libraries(test_mic_capture PRIVATE ole32 propsys avrt mmdevapi)

add_executable(test_process_loopback test_process_loopback.cpp src/ProcessCaptureSource.cpp src/ProcessEnumerator.cpp)
target_include_directories(test_process_loopback PRIVATE src)
target_link_libraries(test_process_loopback PRIVATE ole32 propsys avrt mmdevapi windowsapp)

add_executable(test_config test_config.cpp src/ConfigManager.cpp src/AudioOutput.cpp src/AudioMixer.cpp src/AudioEngine.cpp src/AudioDeviceManager.cpp src/MicCaptureSource.cpp src/ProcessCaptureSource.cpp src/ProcessEnumerator.cpp src/SoundboardManager.cpp src/SoundEffect.cpp src/AudioFileReader.cpp src/VoiceProcessor.cpp)
target_include_directories(test_config PRIVATE src third_party)
target_link_libraries(test_config PRIVATE ole32 propsys avrt mmdevapi windowsapp mfplat mfreadwrite mfuuid)

add_executable(test_multi_output test_multi_output.cpp src/AudioOutput.cpp src/AudioMixer.cpp src/AudioEngine.cpp src/AudioDeviceManager.cpp src/SoundboardManager.cpp src/SoundEffect.cpp src/AudioFileReader.cpp)
target_include_directories(test_multi_output PRIVATE src third_party)
target_link_libraries(test_multi_output PRIVATE ole32 propsys avrt mmdevapi windowsapp mfplat mfreadwrite mfuuid)




