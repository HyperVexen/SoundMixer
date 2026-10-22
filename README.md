# SoundMixer — Low-Latency Audio Mixer for Windows & Valorant

A high-performance, low-latency native Windows application that mixes app audio (Spotify, YouTube in Chrome/Edge, Discord) and physical microphones into a single virtual microphone (VB-Cable) with live volume control, stereo VU meters, soft limiter, and headphone monitoring.

Built with **C++17**, **WASAPI** (`IAudioClient3` low-latency shared mode & Process Loopback API), **MMCSS Pro Audio**, **DirectX 11**, and **Dear ImGui**.

---

## Features

- **Multi-Output Routing (New!):** Route your mix to any number of output playback devices simultaneously (e.g. VB-Cable for Valorant, physical Headphones for local listening, Speakers, and secondary capture devices) each with its own independent volume fader, mute toggle, and live stereo VU meter.
- **Multi-App Process Loopback:** Capture Spotify, Chrome, Edge, Discord, or games independently targeting their entire process tree.
- **Multiple Microphones:** Combine your main headset mic and secondary input devices into one stream with zero crackles.
- **Ultra-Low Latency:**
  - Microphones: ~3–10 ms via WASAPI event-driven shared stream registered with MMCSS "Pro Audio".
  - Apps: ~10–20 ms via Windows 10/11 Process Loopback API.
  - End-to-end total latency: ~10–25 ms.
- **Dynamic Clock Drift Compensation:** SPSC lock-free ring buffers continuously monitored with fractional Hermite cubic resampler micro-adjustments, ensuring zero buffer underruns or overflows even after hours of continuous playback.
- **Transparent Soft Limiter:** Smooth asymptotic saturation curve preventing harsh digital clipping when screaming in mic while loud music plays.
- **Headphones Monitoring:** Hear your exact mixed stream in your headphones with a single click, with independent monitor volume and mute.
- **Dark Gaming UI:** Compact Dear ImGui interface with dual-channel horizontal stereo VU meters (Green/Yellow/Red zones and peak hold ticks), volume sliders, and live status indicators.
- **System Tray Integration:** Minimize to system tray (`Shell_NotifyIconW`) while in-game in Valorant to keep your taskbar clean.
- **Automatic Configuration Persistence:** Remembers all volumes, active sources, output endpoints, and monitor preferences across restarts (`soundmixer_config.json`).

---

## Architecture Overview

```
[Spotify.exe] ──┐  Process Loopback capture (agile WRL)
[Chrome.exe ] ──┤        │
[Mic 1      ] ──┤  WASAPI capture ──► per-source SPSC lock-free ring buffer
[Mic 2      ] ──┘                              │
                                               ▼
                              Mixer thread (MMCSS Pro Audio)
                         gain × mute → sum → soft limiter → float32
                                               │
                                               ▼
                         WASAPI render ──► VB-Cable Input ──► Valorant mic
                                               │
                                   (optional) monitor out to headphones
```

### Real-Time Audio Guarantees:
- **Audio Thread Rules:** The audio render callback performs zero heap allocations, zero mutex locks, zero file/console logging, and zero waits on UI threads.
- **Internal Standard:** 48,000 Hz, 32-bit float, Stereo.

---

## Quick Start & Valorant Setup

### 1. Requirements
- Windows 10 (Version 2004 or later) / Windows 11
- [VB-Audio Virtual Cable](https://vb-audio.com/Cable/) (already installed)

### 2. Multi-Output Setup
- In **SoundMixer**, the **AUDIO OUTPUTS** panel lists all configured target devices.
- By default, it automatically pairs:
  1. **Primary Output:** Your physical Headphones / Speakers (drives the low-jitter hardware DMA clock and local audio).
  2. **Secondary Output:** `Speakers (VB-Audio Virtual Cable)` (feeds your Valorant / Discord microphone stream).
- You can click **`+ Add Output`** at any time to add additional playback endpoints (e.g. Speakers, HDMI output, or secondary virtual cables).
- Each output device has:
  - An independent volume slider (0% to 150%)
  - An independent mute button (`LIVE` / `MUTED`)
  - A dedicated stereo VU meter
  - A `Set as Primary Clock` option to change the master hardware sync device.

### 3. Setting Up Valorant
1. In **SoundMixer**:
   - Master Output is automatically set to **`Speakers (VB-Audio Virtual Cable)`** (or CABLE Input).
   - Click **`+ Add Source`** -> **`Applications`** tab -> select **`Spotify.exe`** or **`chrome.exe`**.
   - In the **`Microphones / Inputs`** tab -> click **`+ Add`** next to your microphone.
2. In **Valorant**:
   - Open **Settings** -> **Audio** -> **Voice Chat**.
   - Set **Input Device** to: **`CABLE Output (VB-Audio Virtual Cable)`**.
   - Your teammates will now hear both your voice and Spotify/YouTube mixed clearly at the levels you set!
3. In **SoundMixer**:
   - Check **"Monitor Output to Headphones"** if you want to hear your own music and mic mix in your headphones while gaming.

---

## Project Structure

```
SoundMixer/
├── src/
│   ├── AudioCommon.h            # Sample rate, channel constants, VU meter math
│   ├── RingBuffer.h             # SPSC lock-free circular buffer with acquire/release semantics
│   ├── Resampler.h              # Hermite cubic resampler with dynamic pitch drift adjustment
│   ├── Limiter.h                # Transparent soft-knee limiter & saturation curve
│   ├── AudioSource.h            # Base audio source interface with atomic controls
│   ├── MicCaptureSource.h/.cpp  # Low-latency WASAPI microphone capture
│   ├── ProcessCaptureSource.h/.cpp # Windows Process Loopback capture
│   ├── ProcessEnumerator.h/.cpp # App and process tree discovery (Spotify, Chrome, etc.)
│   ├── AudioMixer.h/.cpp        # Real-time multi-source summing and drift control
│   ├── AudioDeviceManager.h/.cpp # Playback/Recording device discovery & IMMNotificationClient
│   ├── AudioEngine.h/.cpp       # Master WASAPI render stream & MMCSS Pro Audio engine
│   ├── ConfigManager.h/.cpp     # JSON persistence for mixer settings
│   ├── TrayIcon.h/.cpp          # Windows system tray integration
│   ├── UI.h/.cpp                # Dear ImGui dark UI, custom stereo VU meters, modals
│   └── main.cpp                 # WinMain, DirectX 11 swapchain, ImGui message loop
├── third_party/
│   ├── imgui/                   # Dear ImGui v1.91.8 + Win32/DX11 backends
│   └── nlohmann/json.hpp        # Modern JSON parser
├── CMakeLists.txt               # CMake build configuration
├── build.bat                    # One-click build script
├── run_mixer.bat                # One-click launcher
└── Low-Latency Audio Mixer Build Plan.md
```

---

## Building from Source

To rebuild manually:
```cmd
build.bat
```
Or via CMake:
```cmd
cmake -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release --target SoundMixer
```
