#include "AudioFileReader.h"
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <cstring>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

static std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return std::wstring();
    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
    std::wstring wide(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wide[0], sizeNeeded);
    return wide;
}

const char* AudioFileReader::getBuiltInName(BuiltInSoundType type) {
    switch (type) {
        case BuiltInSoundType::Airhorn: return "Airhorn Blast";
        case BuiltInSoundType::Fanfare: return "Victory Fanfare";
        case BuiltInSoundType::Rimshot: return "Rimshot (Ba-Dum-Tss)";
        case BuiltInSoundType::CoinChime: return "Coin Chime";
        case BuiltInSoundType::Buzzer: return "Wrong Buzzer";
        default: return "Custom SFX";
    }
}

bool AudioFileReader::loadAudioFile(const std::string& filePath,
                                    std::vector<float>& outStereoPcm48k,
                                    float& outDurationSeconds) {
    outStereoPcm48k.clear();
    outDurationSeconds = 0.0f;

    if (filePath.empty()) return false;

    // Check if it's a built-in virtual sound
    if (filePath.rfind("builtin:", 0) == 0) {
        std::string tag = filePath.substr(8);
        if (tag == "airhorn") return generateBuiltIn(BuiltInSoundType::Airhorn, outStereoPcm48k, outDurationSeconds);
        if (tag == "fanfare") return generateBuiltIn(BuiltInSoundType::Fanfare, outStereoPcm48k, outDurationSeconds);
        if (tag == "rimshot") return generateBuiltIn(BuiltInSoundType::Rimshot, outStereoPcm48k, outDurationSeconds);
        if (tag == "coin") return generateBuiltIn(BuiltInSoundType::CoinChime, outStereoPcm48k, outDurationSeconds);
        if (tag == "buzzer") return generateBuiltIn(BuiltInSoundType::Buzzer, outStereoPcm48k, outDurationSeconds);
    }

    // Try Windows Media Foundation first (decodes WAV, MP3, M4A, FLAC, AAC, WMA directly to 48kHz float)
    std::wstring widePath = Utf8ToWide(filePath);
    if (loadViaMediaFoundation(widePath, outStereoPcm48k)) {
        if (!outStereoPcm48k.empty()) {
            outDurationSeconds = static_cast<float>(outStereoPcm48k.size() / 2) / 48000.0f;
            return true;
        }
    }

    // Fallback: try raw RIFF Wave parsing
    if (loadViaRiffWave(filePath, outStereoPcm48k)) {
        if (!outStereoPcm48k.empty()) {
            outDurationSeconds = static_cast<float>(outStereoPcm48k.size() / 2) / 48000.0f;
            return true;
        }
    }

    return false;
}

bool AudioFileReader::loadViaMediaFoundation(const std::wstring& widePath,
                                            std::vector<float>& outStereoPcm48k) {
    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) return false;

    IMFSourceReader* pReader = nullptr;
    hr = MFCreateSourceReaderFromURL(widePath.c_str(), nullptr, &pReader);
    if (FAILED(hr) || !pReader) {
        return false;
    }

    // Deselect all streams, select only first audio stream
    pReader->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
    pReader->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);

    // Request 48,000 Hz, 2 Channels (Stereo), 32-bit Float
    IMFMediaType* pPartialType = nullptr;
    hr = MFCreateMediaType(&pPartialType);
    if (SUCCEEDED(hr) && pPartialType) {
        pPartialType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        pPartialType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
        pPartialType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
        pPartialType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 48000);
        pPartialType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 32);
        pPartialType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 8);
        pPartialType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 48000 * 8);

        hr = pReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pPartialType);
        pPartialType->Release();
    }

    if (FAILED(hr)) {
        pReader->Release();
        return false;
    }

    IMFMediaType* pActualType = nullptr;
    UINT32 actualChannels = 2;
    UINT32 actualSampleRate = 48000;
    if (SUCCEEDED(pReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pActualType)) && pActualType) {
        pActualType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &actualChannels);
        pActualType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &actualSampleRate);
        pActualType->Release();
    }

    std::vector<float> decodedPcm;

    // Read audio samples until end of stream
    while (true) {
        DWORD streamIndex = 0;
        DWORD flags = 0;
        LONGLONG timestamp = 0;
        IMFSample* pSample = nullptr;

        hr = pReader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, &streamIndex, &flags, &timestamp, &pSample);
        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
            if (pSample) pSample->Release();
            break;
        }

        if (pSample) {
            IMFMediaBuffer* pBuffer = nullptr;
            if (SUCCEEDED(pSample->ConvertToContiguousBuffer(&pBuffer)) && pBuffer) {
                BYTE* pData = nullptr;
                DWORD currentLength = 0;
                if (SUCCEEDED(pBuffer->Lock(&pData, nullptr, &currentLength)) && pData) {
                    size_t floatCount = currentLength / sizeof(float);
                    const float* pFloats = reinterpret_cast<const float*>(pData);
                    decodedPcm.insert(decodedPcm.end(), pFloats, pFloats + floatCount);
                    pBuffer->Unlock();
                }
                pBuffer->Release();
            }
            pSample->Release();
        }
    }

    pReader->Release();
    if (decodedPcm.empty()) return false;

    // Convert to stereo if decoded as mono
    std::vector<float> nativeStereo;
    if (actualChannels == 1) {
        nativeStereo.resize(decodedPcm.size() * 2);
        for (size_t i = 0; i < decodedPcm.size(); ++i) {
            nativeStereo[i * 2 + 0] = decodedPcm[i];
            nativeStereo[i * 2 + 1] = decodedPcm[i];
        }
    } else if (actualChannels == 2) {
        nativeStereo = std::move(decodedPcm);
    } else {
        size_t frames = decodedPcm.size() / actualChannels;
        nativeStereo.resize(frames * 2);
        for (size_t f = 0; f < frames; ++f) {
            nativeStereo[f * 2 + 0] = decodedPcm[f * actualChannels + 0];
            nativeStereo[f * 2 + 1] = decodedPcm[f * actualChannels + 1];
        }
    }

    // Resample to 48000 Hz if needed
    if (actualSampleRate == 48000) {
        outStereoPcm48k = std::move(nativeStereo);
    } else {
        size_t inFrames = nativeStereo.size() / 2;
        double ratio = 48000.0 / static_cast<double>(actualSampleRate);
        size_t outFrames = static_cast<size_t>(inFrames * ratio);
        outStereoPcm48k.resize(outFrames * 2);

        for (size_t i = 0; i < outFrames; ++i) {
            double srcIdx = static_cast<double>(i) / ratio;
            size_t idx0 = static_cast<size_t>(srcIdx);
            size_t idx1 = (std::min)(idx0 + 1, inFrames - 1);
            float frac = static_cast<float>(srcIdx - idx0);

            outStereoPcm48k[i * 2 + 0] = nativeStereo[idx0 * 2 + 0] * (1.0f - frac) + nativeStereo[idx1 * 2 + 0] * frac;
            outStereoPcm48k[i * 2 + 1] = nativeStereo[idx0 * 2 + 1] * (1.0f - frac) + nativeStereo[idx1 * 2 + 1] * frac;
        }
    }

    return !outStereoPcm48k.empty();
}

bool AudioFileReader::loadViaRiffWave(const std::string& filePath,
                                     std::vector<float>& outStereoPcm48k) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return false;

    char header[12];
    if (!file.read(header, 12)) return false;
    if (std::memcmp(header, "RIFF", 4) != 0 || std::memcmp(header + 8, "WAVE", 4) != 0) {
        return false;
    }

    uint16_t audioFormat = 0;
    uint16_t numChannels = 0;
    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 0;
    std::vector<uint8_t> rawAudioData;

    while (file) {
        char chunkId[4];
        uint32_t chunkSize = 0;
        if (!file.read(chunkId, 4)) break;
        if (!file.read(reinterpret_cast<char*>(&chunkSize), 4)) break;

        if (std::memcmp(chunkId, "fmt ", 4) == 0) {
            if (chunkSize >= 16) {
                file.read(reinterpret_cast<char*>(&audioFormat), 2);
                file.read(reinterpret_cast<char*>(&numChannels), 2);
                file.read(reinterpret_cast<char*>(&sampleRate), 4);
                uint32_t byteRate = 0;
                uint16_t blockAlign = 0;
                file.read(reinterpret_cast<char*>(&byteRate), 4);
                file.read(reinterpret_cast<char*>(&blockAlign), 2);
                file.read(reinterpret_cast<char*>(&bitsPerSample), 2);
                if (chunkSize > 16) {
                    file.seekg(chunkSize - 16, std::ios::cur);
                }
            } else {
                file.seekg(chunkSize, std::ios::cur);
            }
        } else if (std::memcmp(chunkId, "data", 4) == 0) {
            rawAudioData.resize(chunkSize);
            file.read(reinterpret_cast<char*>(rawAudioData.data()), chunkSize);
            break;
        } else {
            file.seekg(chunkSize, std::ios::cur);
        }
    }

    if (rawAudioData.empty() || numChannels == 0 || sampleRate == 0) return false;

    // Convert raw PCM to float
    std::vector<float> nativePcm;
    size_t bytesPerSample = bitsPerSample / 8;
    if (bytesPerSample == 0) return false;
    size_t sampleCount = rawAudioData.size() / bytesPerSample;

    nativePcm.resize(sampleCount);
    if (audioFormat == 1) { // PCM integer
        if (bitsPerSample == 16) {
            const int16_t* p16 = reinterpret_cast<const int16_t*>(rawAudioData.data());
            for (size_t i = 0; i < sampleCount; ++i) {
                nativePcm[i] = static_cast<float>(p16[i]) / 32768.0f;
            }
        } else if (bitsPerSample == 24) {
            const uint8_t* p24 = rawAudioData.data();
            for (size_t i = 0; i < sampleCount; ++i) {
                int32_t val = (p24[i * 3 + 0] << 8) | (p24[i * 3 + 1] << 16) | (p24[i * 3 + 2] << 24);
                nativePcm[i] = static_cast<float>(val) / 2147483648.0f;
            }
        } else if (bitsPerSample == 32) {
            const int32_t* p32 = reinterpret_cast<const int32_t*>(rawAudioData.data());
            for (size_t i = 0; i < sampleCount; ++i) {
                nativePcm[i] = static_cast<float>(p32[i]) / 2147483648.0f;
            }
        } else {
            return false;
        }
    } else if (audioFormat == 3) { // IEEE Float
        if (bitsPerSample == 32) {
            const float* pF = reinterpret_cast<const float*>(rawAudioData.data());
            std::memcpy(nativePcm.data(), pF, sampleCount * sizeof(float));
        } else {
            return false;
        }
    } else {
        return false;
    }

    // Convert to stereo if mono
    std::vector<float> nativeStereo;
    if (numChannels == 1) {
        nativeStereo.resize(sampleCount * 2);
        for (size_t i = 0; i < sampleCount; ++i) {
            nativeStereo[i * 2 + 0] = nativePcm[i];
            nativeStereo[i * 2 + 1] = nativePcm[i];
        }
    } else if (numChannels == 2) {
        nativeStereo = std::move(nativePcm);
    } else {
        // Multi-channel downmix first 2 channels
        size_t frames = sampleCount / numChannels;
        nativeStereo.resize(frames * 2);
        for (size_t f = 0; f < frames; ++f) {
            nativeStereo[f * 2 + 0] = nativePcm[f * numChannels + 0];
            nativeStereo[f * 2 + 1] = nativePcm[f * numChannels + 1];
        }
    }

    // Resample to 48000 Hz if needed
    if (sampleRate == 48000) {
        outStereoPcm48k = std::move(nativeStereo);
    } else {
        size_t inFrames = nativeStereo.size() / 2;
        double ratio = 48000.0 / static_cast<double>(sampleRate);
        size_t outFrames = static_cast<size_t>(inFrames * ratio);
        outStereoPcm48k.resize(outFrames * 2);

        for (size_t i = 0; i < outFrames; ++i) {
            double srcIdx = static_cast<double>(i) / ratio;
            size_t idx0 = static_cast<size_t>(srcIdx);
            size_t idx1 = (std::min)(idx0 + 1, inFrames - 1);
            float frac = static_cast<float>(srcIdx - idx0);

            outStereoPcm48k[i * 2 + 0] = nativeStereo[idx0 * 2 + 0] * (1.0f - frac) + nativeStereo[idx1 * 2 + 0] * frac;
            outStereoPcm48k[i * 2 + 1] = nativeStereo[idx0 * 2 + 1] * (1.0f - frac) + nativeStereo[idx1 * 2 + 1] * frac;
        }
    }

    return !outStereoPcm48k.empty();
}

bool AudioFileReader::generateBuiltIn(BuiltInSoundType type,
                                      std::vector<float>& outStereoPcm48k,
                                      float& outDurationSeconds) {
    constexpr int sampleRate = 48000;
    outStereoPcm48k.clear();

    switch (type) {
        case BuiltInSoundType::Airhorn: {
            // Iconic 3-burst staccato stadium airhorn!
            outDurationSeconds = 1.6f;
            size_t totalFrames = static_cast<size_t>(outDurationSeconds * sampleRate);
            outStereoPcm48k.resize(totalFrames * 2, 0.0f);

            // 3 bursts: [0.0 - 0.25], [0.35 - 0.60], [0.70 - 1.45]
            struct Burst { float start; float end; };
            Burst bursts[] = { { 0.02f, 0.26f }, { 0.34f, 0.58f }, { 0.68f, 1.45f } };

            double phase1 = 0.0, phase2 = 0.0;
            for (size_t f = 0; f < totalFrames; ++f) {
                float t = static_cast<float>(f) / sampleRate;

                // Check active burst
                float burstEnv = 0.0f;
                for (auto& b : bursts) {
                    if (t >= b.start && t <= b.end) {
                        float dur = b.end - b.start;
                        float localT = t - b.start;
                        float attack = (std::min)(localT / 0.02f, 1.0f);
                        float release = (std::min)((dur - localT) / 0.04f, 1.0f);
                        burstEnv = attack * release;
                        break;
                    }
                }

                if (burstEnv > 0.0f) {
                    // Multi-tone dissonant brass chord (E4 ~330Hz, G#4 ~415Hz)
                    double f1 = 330.0 + sin(t * 18.0) * 8.0;
                    double f2 = 415.0 + sin(t * 18.0) * 10.0;

                    phase1 += f1 / sampleRate;
                    phase2 += f2 / sampleRate;
                    if (phase1 >= 1.0) phase1 -= 1.0;
                    if (phase2 >= 1.0) phase2 -= 1.0;

                    // Sawtooth + square with punch
                    float saw1 = static_cast<float>(2.0 * phase1 - 1.0);
                    float saw2 = static_cast<float>(2.0 * phase2 - 1.0);
                    float sq1 = (phase1 < 0.5) ? 0.7f : -0.7f;

                    float s = (saw1 * 0.45f + saw2 * 0.45f + sq1 * 0.2f) * burstEnv * 0.75f;
                    // Soft saturation
                    s = std::tanh(s * 1.5f) * 0.8f;

                    outStereoPcm48k[f * 2 + 0] = s;
                    outStereoPcm48k[f * 2 + 1] = s;
                }
            }
            return true;
        }

        case BuiltInSoundType::Fanfare: {
            // Victory Fanfare / Level Up: C5 -> E5 -> G5 -> C6
            outDurationSeconds = 1.2f;
            size_t totalFrames = static_cast<size_t>(outDurationSeconds * sampleRate);
            outStereoPcm48k.resize(totalFrames * 2, 0.0f);

            struct Note { float freq; float start; float end; };
            Note notes[] = {
                { 523.25f, 0.00f, 0.18f }, // C5
                { 659.25f, 0.18f, 0.36f }, // E5
                { 783.99f, 0.36f, 0.54f }, // G5
                { 1046.50f, 0.54f, 1.20f } // C6
            };

            double phase = 0.0;
            float curFreq = 523.25f;
            for (size_t f = 0; f < totalFrames; ++f) {
                float t = static_cast<float>(f) / sampleRate;
                float env = 0.0f;

                for (auto& n : notes) {
                    if (t >= n.start && t < n.end) {
                        curFreq = n.freq;
                        float localT = t - n.start;
                        float dur = n.end - n.start;
                        float attack = (std::min)(localT / 0.015f, 1.0f);
                        float decay = std::exp(-localT * 3.5f / dur);
                        env = attack * decay;
                        break;
                    }
                }

                phase += curFreq / sampleRate;
                if (phase >= 1.0) phase -= 1.0;

                // Bell-like chime with 2nd harmonic
                float s = (sin(phase * 2.0 * 3.14159265) * 0.7f +
                           sin(phase * 4.0 * 3.14159265) * 0.3f) * env * 0.8f;

                outStereoPcm48k[f * 2 + 0] = s;
                outStereoPcm48k[f * 2 + 1] = s;
            }
            return true;
        }

        case BuiltInSoundType::Rimshot: {
            // Punchy snare / rimshot (Ba-Dum-Tss)
            outDurationSeconds = 1.0f;
            size_t totalFrames = static_cast<size_t>(outDurationSeconds * sampleRate);
            outStereoPcm48k.resize(totalFrames * 2, 0.0f);

            // Ba (0.00 - 0.15s), Dum (0.18 - 0.35s), Tss (0.38 - 1.0s)
            for (size_t f = 0; f < totalFrames; ++f) {
                float t = static_cast<float>(f) / sampleRate;
                float s = 0.0f;

                // Ba
                if (t >= 0.0f && t < 0.16f) {
                    float localT = t;
                    float freq = 180.0f * std::exp(-localT * 30.0f) + 80.0f;
                    float env = std::exp(-localT * 22.0f);
                    s += sin(localT * freq * 2.0 * 3.14159265) * env * 0.6f;
                }
                // Dum
                if (t >= 0.18f && t < 0.36f) {
                    float localT = t - 0.18f;
                    float freq = 150.0f * std::exp(-localT * 25.0f) + 65.0f;
                    float env = std::exp(-localT * 18.0f);
                    s += sin(localT * freq * 2.0 * 3.14159265) * env * 0.7f;
                }
                // Tss (cymbal noise burst)
                if (t >= 0.38f && t < 1.0f) {
                    float localT = t - 0.38f;
                    float env = std::exp(-localT * 7.0f);
                    float whiteNoise = ((rand() % 2000) - 1000) / 1000.0f;
                    s += whiteNoise * env * 0.65f;
                }

                s = std::tanh(s) * 0.85f;
                outStereoPcm48k[f * 2 + 0] = s;
                outStereoPcm48k[f * 2 + 1] = s;
            }
            return true;
        }

        case BuiltInSoundType::CoinChime: {
            // Retro arcade coin chime
            outDurationSeconds = 0.5f;
            size_t totalFrames = static_cast<size_t>(outDurationSeconds * sampleRate);
            outStereoPcm48k.resize(totalFrames * 2, 0.0f);

            double phase = 0.0;
            for (size_t f = 0; f < totalFrames; ++f) {
                float t = static_cast<float>(f) / sampleRate;
                float freq = (t < 0.09f) ? 987.77f : 1318.51f; // B5 then E6
                float env = (t < 0.09f) ? 1.0f : std::exp(-(t - 0.09f) * 8.0f);

                phase += freq / sampleRate;
                if (phase >= 1.0) phase -= 1.0;

                float sq = (phase < 0.5) ? 0.5f : -0.5f;
                float s = sq * env * 0.55f;

                outStereoPcm48k[f * 2 + 0] = s;
                outStereoPcm48k[f * 2 + 1] = s;
            }
            return true;
        }

        case BuiltInSoundType::Buzzer: {
            // Game show wrong buzzer
            outDurationSeconds = 0.8f;
            size_t totalFrames = static_cast<size_t>(outDurationSeconds * sampleRate);
            outStereoPcm48k.resize(totalFrames * 2, 0.0f);

            double phase = 0.0;
            for (size_t f = 0; f < totalFrames; ++f) {
                float t = static_cast<float>(f) / sampleRate;
                float env = (std::min)(t / 0.02f, 1.0f) * (std::min)((0.8f - t) / 0.05f, 1.0f);
                float freq = 130.0f - (t * 20.0f);

                phase += freq / sampleRate;
                if (phase >= 1.0) phase -= 1.0;

                float saw = static_cast<float>(2.0 * phase - 1.0);
                float s = saw * env * 0.65f;

                outStereoPcm48k[f * 2 + 0] = s;
                outStereoPcm48k[f * 2 + 1] = s;
            }
            return true;
        }
    }

    return false;
}
