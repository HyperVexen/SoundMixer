#pragma once

#include <vector>
#include <atomic>
#include <cstdint>
#include <algorithm>
#include <cstring>

// Lock-free Single-Producer Single-Consumer (SPSC) ring buffer for stereo audio frames.
// Each frame consists of 2 float samples (Left, Right).
class RingBuffer {
public:
    explicit RingBuffer(size_t capacityFrames = 16384) {
        // Round up capacity to next power of 2 for fast wrapping
        size_t cap = 1;
        while (cap < capacityFrames) cap <<= 1;
        m_capacity = cap;
        m_mask = cap - 1;
        m_buffer.resize(m_capacity * 2, 0.0f); // 2 channels (stereo)
        m_writeIndex.store(0, std::memory_order_relaxed);
        m_readIndex.store(0, std::memory_order_relaxed);
    }

    size_t getCapacity() const { return m_capacity; }

    // Returns number of frames currently available to read
    size_t getAvailableRead() const {
        size_t writeIdx = m_writeIndex.load(std::memory_order_acquire);
        size_t readIdx = m_readIndex.load(std::memory_order_relaxed);
        return writeIdx - readIdx;
    }

    // Returns number of frames space available to write
    size_t getAvailableWrite() const {
        size_t writeIdx = m_writeIndex.load(std::memory_order_relaxed);
        size_t readIdx = m_readIndex.load(std::memory_order_acquire);
        size_t used = writeIdx - readIdx;
        return (m_capacity > used) ? (m_capacity - used) : 0;
    }

    // Write stereo frames to ring buffer (producer thread only).
    // frames: pointer to interleaved stereo float samples (count * 2 floats).
    // Returns actual number of frames written.
    size_t write(const float* frames, size_t count) {
        if (!frames || count == 0) return 0;

        size_t writeIdx = m_writeIndex.load(std::memory_order_relaxed);
        size_t readIdx = m_readIndex.load(std::memory_order_acquire);
        size_t available = m_capacity - (writeIdx - readIdx);
        size_t toWrite = (std::min)(count, available);

        if (toWrite == 0) return 0;

        size_t startPos = (writeIdx & m_mask) * 2;
        size_t samplesToWrite = toWrite * 2;
        size_t bufferTotalSamples = m_capacity * 2;

        if (startPos + samplesToWrite <= bufferTotalSamples) {
            std::memcpy(&m_buffer[startPos], frames, samplesToWrite * sizeof(float));
        } else {
            size_t firstPart = bufferTotalSamples - startPos;
            std::memcpy(&m_buffer[startPos], frames, firstPart * sizeof(float));
            std::memcpy(&m_buffer[0], frames + firstPart, (samplesToWrite - firstPart) * sizeof(float));
        }

        m_writeIndex.store(writeIdx + toWrite, std::memory_order_release);
        return toWrite;
    }

    // Read stereo frames from ring buffer (consumer thread only).
    // dest: pointer to destination buffer for interleaved stereo float samples (count * 2 floats).
    // Returns actual number of frames read.
    size_t read(float* dest, size_t count) {
        if (!dest || count == 0) return 0;

        size_t writeIdx = m_writeIndex.load(std::memory_order_acquire);
        size_t readIdx = m_readIndex.load(std::memory_order_relaxed);
        size_t available = writeIdx - readIdx;
        size_t toRead = (std::min)(count, available);

        if (toRead == 0) return 0;

        size_t startPos = (readIdx & m_mask) * 2;
        size_t samplesToRead = toRead * 2;
        size_t bufferTotalSamples = m_capacity * 2;

        if (startPos + samplesToRead <= bufferTotalSamples) {
            std::memcpy(dest, &m_buffer[startPos], samplesToRead * sizeof(float));
        } else {
            size_t firstPart = bufferTotalSamples - startPos;
            std::memcpy(dest, &m_buffer[startPos], firstPart * sizeof(float));
            std::memcpy(dest + firstPart, &m_buffer[0], (samplesToRead - firstPart) * sizeof(float));
        }

        m_readIndex.store(readIdx + toRead, std::memory_order_release);
        return toRead;
    }

    // Discard frames from the ring buffer (e.g. on flush/reset)
    void flush() {
        size_t writeIdx = m_writeIndex.load(std::memory_order_acquire);
        m_readIndex.store(writeIdx, std::memory_order_release);
    }

private:
    std::vector<float> m_buffer;
    size_t m_capacity;
    size_t m_mask;
    alignas(64) std::atomic<size_t> m_writeIndex{0};
    alignas(64) std::atomic<size_t> m_readIndex{0};
};
