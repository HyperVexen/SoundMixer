#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

enum class BuiltInSoundType {
    Airhorn,
    Fanfare,
    Rimshot,
    CoinChime,
    Buzzer
};

class AudioFileReader {
public:
    // Decodes any audio file (WAV, MP3, M4A, FLAC, AAC, WMA) into 48,000 Hz 32-bit float stereo PCM.
    // Returns true on success, false on error.
    static bool loadAudioFile(const std::string& filePath,
                              std::vector<float>& outStereoPcm48k,
                              float& outDurationSeconds);

    // Procedural sound synthesizer for built-in effects
    static bool generateBuiltIn(BuiltInSoundType type,
                                std::vector<float>& outStereoPcm48k,
                                float& outDurationSeconds);

    static const char* getBuiltInName(BuiltInSoundType type);

private:
    static bool loadViaMediaFoundation(const std::wstring& widePath,
                                       std::vector<float>& outStereoPcm48k);

    static bool loadViaRiffWave(const std::string& filePath,
                                std::vector<float>& outStereoPcm48k);
};
