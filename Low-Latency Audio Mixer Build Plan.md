# Low-Latency Audio Mixer: Build Plan

A native Windows app that mixes app audio (Spotify, YouTube in Chrome/Edge) and one or more microphones into a single virtual mic, so you can play music to friends in Valorant.

## 1. Goals

- Combine audio from multiple apps into one stream.
- Combine multiple input devices (mics) into the same stream.
- Send the mixed result to a virtual cable that Valorant uses as its microphone.
- Simple dark UI: per-source volume, mute and level meter.
- Lowest latency Windows allows, with no crackles or dropouts.

## 2. Latency: what is realistic

| Path | Realistic latency | Notes |
| --- | --- | --- |
| Mic / input device capture | about 3 ms | WASAPI `IAudioClient3` low-latency shared mode (around 128 frames at 48 kHz) |
| Per-app capture (Spotify, Chrome) | about 10-20 ms | Process Loopback API is shared mode only |
| Full mix to virtual cable | about 10-25 ms total | Dominated by the app-capture leg |

A true 1-2 ms end to end is not achievable for per-app capture on Windows. It also does not matter here: Valorant voice chat adds 50 ms or more of network and codec delay, and music at 10-20 ms is imperceptible. The goal is therefore "as low as possible without glitches".

## 3. Stack

- **Language:** C++17, MSVC, single `.exe`, built with CMake.
- **Audio:** WASAPI directly, event-driven, audio threads registered with MMCSS "Pro Audio".
- **UI:** Dear ImGui with the DirectX 11 backend (small, dark, quick for sliders and meters).
- **Virtual mic:** VB-Cable (free). The mixer renders to "CABLE Input"; Valorant uses "CABLE Output" as its mic. No custom driver needed.
- **Config:** a small JSON file (nlohmann/json).

## 4. Architecture

```
[Spotify]──┐  Process Loopback capture
[Chrome ]──┤        │
[Mic 1  ]──┤  WASAPI capture ──► per-source lock-free ring buffer
[Mic 2  ]──┘                              │
                                          ▼
                         Mixer thread (MMCSS Pro Audio)
                   gain × mute → sum → soft limiter → float32
                                          │
                                          ▼
                    WASAPI render ──► VB-Cable Input ──► Valorant mic
                                          │
                              (optional) monitor out to headphones
```

**Threads**

- One capture thread per source, writing into its own single-producer/single-consumer ring buffer.
- One mixer/render thread that pulls a buffer's worth from each ring, applies gain and mute, sums, limits and writes to the render device.
- UI thread (ImGui) that only touches atomics for gain, mute and meter values.

**Audio thread rules:** no allocation, no locks, no file or logging calls, no waiting on the UI.

**Format handling:** everything internal is 48 kHz float32 stereo. Sources in other formats go through a resampler (linear first, polyphase if quality needs it).

**Drift handling:** each ring buffer's fill level is monitored; a tiny resampling ratio adjustment keeps it near the target so independent device clocks do not cause underruns or overruns.

## 5. Features

1. Add source: pick a running app (process list) or an input device.
2. Per source: volume slider, mute button, live level meter.
3. Master: volume, limiter to prevent clipping, output device picker (default CABLE Input).
4. Monitor toggle: also play the mix to your headphones.
5. Save and load config so the app reopens as you left it.

"Combine apps" and "combine inputs" are the same mixer, so there are no separate modes.

## 6. Build phases

| Phase | Deliverable | Done when |
| --- | --- | --- |
| 1. Skeleton | CMake project, device enumeration, test tone to CABLE Input | Tone is audible in a recording app set to CABLE Output |
| 2. Mic capture | Low-latency capture into a ring buffer, passthrough to output | Mic is audible with no crackles at the smallest period the device allows |
| 3. Process loopback | Capture audio of a chosen app, including Chrome/Edge process trees | Spotify and a browser tab are captured independently |
| 4. Mixer core | Gain, mute, sum, soft limiter, resampler, drift correction | Four sources run for 30 minutes with zero underruns |
| 5. UI | ImGui window with source list, sliders, meters, device pickers | Full workflow usable without touching code or config files |
| 6. Polish | Config save/load, tray icon, glitch counter, buffer-size setting | App survives device unplug/replug and restarts cleanly |

## 7. Measuring latency

- Loopback test: play a click through the mixer to CABLE Input, record CABLE Output, and compare timestamps.
- Show current buffer size and glitch count in the UI so tuning is visible.
- Test with Valorant open in a custom game with a friend before relying on it in a match.

## 8. Risks and mitigations

- **Clock drift between devices:** causes crackles. Mitigated by ring-buffer fill monitoring and slight resampling.
- **Chrome/Edge multi-process audio:** target the process tree, not a single PID.
- **Process Loopback availability:** needs Windows 10 version 2004 or later, or Windows 11.
- **Device changes while running:** handle with `IMMNotificationClient` and reconnect gracefully.
- **Valorant / Vanguard:** a virtual mic is normally fine; if Valorant does not pick it up, also set the Windows default input to CABLE Output.
- **Feedback loops:** if the monitor output is also captured as a source, the app should warn or exclude itself.

## 9. Setup checklist for you

- Install VB-Cable.
- Install Visual Studio 2022 (Desktop development with C++) and CMake.
- Confirm Windows version is 10 2004 or later.
- In Valorant, set the input device to "CABLE Output".

## 10. Next step

Start with phases 1 and 2 (project skeleton, test tone, mic passthrough) so you can measure real latency on your machine before building the rest.
