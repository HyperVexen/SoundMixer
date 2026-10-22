#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>
#include <memory>
#include "RingBuffer.h"
#include "Resampler.h"
#include "Limiter.h"
#include "AudioMixer.h"

// Mock AudioSource for testing mixer logic
class MockSource : public AudioSource {
public:
    MockSource(const std::string& id, const std::string& name, float frequency, uint32_t sampleRate = 48000)
        : AudioSource(SourceType::Application, id, name), m_freq(frequency), m_rate(sampleRate) {
        m_resampler.reset(sampleRate, 48000);
    }

    bool start() override { m_running = true; return true; }
    void stop() override { m_running = false; }
    bool isRunning() const override { return m_running; }
    std::string getStatusText() const override { return "Mock Running"; }

    // Feeds synthesized sine wave frames into the ring buffer
    void feedFrames(size_t frameCount) {
        std::vector<float> frames(frameCount * 2);
        double phaseInc = 2.0 * 3.14159265358979323846 * m_freq / m_rate;
        for (size_t i = 0; i < frameCount; ++i) {
            float s = static_cast<float>(0.3 * std::sin(m_phase));
            m_phase += phaseInc;
            frames[i * 2 + 0] = s;
            frames[i * 2 + 1] = s;
        }

        std::vector<float> resampled(frameCount * 4);
        size_t outFrames = m_resampler.process(frames.data(), frameCount, resampled.data(), resampled.size() / 2);
        m_ringBuffer.write(resampled.data(), outFrames);
    }

private:
    float m_freq;
    uint32_t m_rate;
    double m_phase = 0.0;
    bool m_running = false;
};

int main() {
    std::cout << "[Test Mixer Core] Starting tests..." << std::endl;

    // 1. Test RingBuffer SPSC
    std::cout << "1. Testing RingBuffer SPSC lock-free operations..." << std::endl;
    RingBuffer ring(1024);
    assert(ring.getCapacity() >= 1024);
    assert(ring.getAvailableRead() == 0);
    assert(ring.getAvailableWrite() >= 1024);

    std::vector<float> writeData(256 * 2, 0.42f);
    size_t written = ring.write(writeData.data(), 256);
    assert(written == 256);
    assert(ring.getAvailableRead() == 256);

    std::vector<float> readData(256 * 2, 0.0f);
    size_t readCount = ring.read(readData.data(), 128);
    assert(readCount == 128);
    assert(ring.getAvailableRead() == 128);
    for (size_t i = 0; i < 128 * 2; ++i) {
        assert(std::abs(readData[i] - 0.42f) < 1e-6f);
    }
    std::cout << "   -> RingBuffer passed!" << std::endl;

    // 2. Test SoftLimiter
    std::cout << "2. Testing SoftLimiter..." << std::endl;
    SoftLimiter limiter(0.85f, 0.99f);
    std::vector<float> loudFrames = { 2.5f, -3.0f, 0.5f, -0.5f };
    limiter.process(loudFrames.data(), 2);
    assert(limiter.isLimiterActive() == true);
    for (float sample : loudFrames) {
        assert(std::abs(sample) <= 0.99f);
    }
    std::cout << "   -> SoftLimiter clamped high signals smoothly to ceiling!" << std::endl;

    // 3. Test Resampler & Drift adjustment
    std::cout << "3. Testing Hermite Resampler (44.1 kHz -> 48 kHz)..." << std::endl;
    StereoResampler resampler;
    resampler.reset(44100, 48000);
    std::vector<float> in441(441 * 2, 0.5f);
    std::vector<float> out480(1000 * 2, 0.0f);
    size_t produced = resampler.process(in441.data(), 441, out480.data(), 1000);
    assert(produced >= 470 && produced <= 490);
    std::cout << "   -> Produced " << produced << " frames for 441 input frames (Expected ~480). Resampler passed!" << std::endl;

    // 4. Test AudioMixer multi-source summing and drift control
    std::cout << "4. Testing AudioMixer with 4 concurrent sources..." << std::endl;
    AudioMixer mixer;
    auto src1 = std::make_shared<MockSource>("src1", "Spotify (44.1k)", 440.0f, 44100);
    auto src2 = std::make_shared<MockSource>("src2", "Chrome (48k)", 880.0f, 48000);
    auto src3 = std::make_shared<MockSource>("src3", "Mic (48k)", 220.0f, 48000);
    auto src4 = std::make_shared<MockSource>("src4", "Discord (96k)", 1000.0f, 96000);

    mixer.addSource(src1);
    mixer.addSource(src2);
    mixer.addSource(src3);
    mixer.addSource(src4);

    std::vector<float> mixBuffer(128 * 2, 0.0f);

    // Simulate 500 render blocks of 128 frames (~1.33 seconds of 48kHz audio)
    for (int block = 0; block < 500; ++block) {
        // Feed sources with slightly varied packet sizes to simulate clock drift
        src1->feedFrames(118);
        src2->feedFrames(128);
        src3->feedFrames(129);
        src4->feedFrames(256);

        mixer.processAudio(mixBuffer.data(), 128);

        // Verify output is non-zero and within limiter ceiling
        for (size_t s = 0; s < 128 * 2; ++s) {
            assert(!std::isnan(mixBuffer[s]));
            assert(std::abs(mixBuffer[s]) <= 1.0f);
        }
    }

    std::cout << "   -> 500 blocks processed cleanly with 4 sources!" << std::endl;
    float pl, pr, rl, rr;
    mixer.getMasterLevels(pl, pr, rl, rr);
    std::cout << "   -> Master Peak: " << pl << " / " << pr << ", RMS: " << rl << " / " << rr << std::endl;
    assert(pl > 0.05f);

    // 5. Test Mute
    std::cout << "5. Testing Master Mute..." << std::endl;
    mixer.setMasterMuted(true);
    mixer.processAudio(mixBuffer.data(), 128);
    for (size_t s = 0; s < 128 * 2; ++s) {
        assert(mixBuffer[s] == 0.0f);
    }
    std::cout << "   -> Master Mute verified silent!" << std::endl;

    std::cout << "\n>>> ALL AUDIO MIXER CORE TESTS PASSED! <<<" << std::endl;
    return 0;
}
