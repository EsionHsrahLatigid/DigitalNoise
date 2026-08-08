#pragma once

#include <array>
#include <cstdint>

namespace digitalnoise
{

struct Parameters
{
    // Internal clamps:
    // clockHz [1, 24000], topology [0, 1], mutation [0, 1],
    // memoryDepth [0, 1], addressScramble [0, 1], feedback [0, 1],
    // stereoDivergence [0, 1], intensity [0, 1], rawMisread [0, 1],
    // formatSmash [0, 1], outputGain [0, 2].
    float clockHz = 8000.0f;
    float topology = 0.5f;
    float mutation = 0.2f;
    float memoryDepth = 0.75f;
    float addressScramble = 0.6f;
    float feedback = 0.4f;
    float stereoDivergence = 0.35f;
    float intensity = 0.85f;
    float rawMisread = 0.0f;
    float formatSmash = 0.0f;
    float outputGain = 0.35f;
};

struct StereoFrame
{
    float left = 0.0f;
    float right = 0.0f;
};

class DigitalNoiseEngine
{
public:
    DigitalNoiseEngine();

    void prepare (double sampleRate) noexcept;
    void reset (std::uint32_t seed) noexcept;
    void reseedGraph (std::uint32_t seed) noexcept;
    void setParameters (const Parameters& parameters) noexcept;

    StereoFrame processSample() noexcept;
    void process (float* left, float* right, int numSamples) noexcept;

private:
    static constexpr int memorySize = 2048;
    static constexpr int memoryMask = memorySize - 1;
    static constexpr int blobByteSize = memorySize * static_cast<int> (sizeof (std::uint32_t));
    static constexpr int blobByteMask = blobByteSize - 1;
    static constexpr int sequencerLength = 16;

    struct ClampedParameters
    {
        float clockHz = 8000.0f;
        float topology = 0.5f;
        float mutation = 0.2f;
        float memoryDepth = 0.75f;
        float addressScramble = 0.6f;
        float feedback = 0.4f;
        float stereoDivergence = 0.35f;
        float intensity = 0.85f;
        float rawMisread = 0.0f;
        float formatSmash = 0.0f;
        float outputGain = 0.35f;
    };

    static float clamp (float value, float low, float high) noexcept;
    static std::uint32_t mixSeed (std::uint32_t value) noexcept;
    static std::uint32_t stepLfsr (std::uint32_t state) noexcept;
    static std::uint32_t rotateLeft (std::uint32_t value, int bits) noexcept;
    static std::uint32_t unitToMask (float value) noexcept;
    static float bitsToSignedFloat (std::uint32_t value) noexcept;
    void rebuildSequencer() noexcept;
    void reseedMemory() noexcept;
    void tickGraph() noexcept;
    StereoFrame decodeBlobFrame() noexcept;
    float decodeBlobSample (std::uint32_t byteAddress, int mode, int bitIndex, int channel) noexcept;
    std::uint8_t readBlobByte (std::uint32_t byteAddress) const noexcept;
    void writeBlobByte (std::uint32_t byteAddress, std::uint8_t value) noexcept;
    void writeBlobBigEndian32 (std::uint32_t byteAddress, std::uint32_t value) noexcept;
    std::uint32_t cellularStep (std::uint32_t cells, std::uint8_t rule) noexcept;
    std::uint32_t currentRule() const noexcept;
    std::uint32_t memoryDepthSamples() const noexcept;

    ClampedParameters params;
    double sampleRate = 44100.0;
    float phase = 0.0f;

    std::uint32_t initialSeed = 1u;
    std::uint32_t lfsrA = 0x6d2b79f5u;
    std::uint32_t lfsrB = 0xa5a5f00du;
    std::uint32_t cellsA = 0xf0ccaabeu;
    std::uint32_t cellsB = 0x7351c0deu;
    std::uint32_t address = 0u;
    std::uint32_t graphWord = 0u;
    std::uint32_t holdLeft = 0u;
    std::uint32_t holdRight = 0u;
    std::uint32_t sampleCounter = 0u;
    std::uint32_t blobCursor = 0u;
    std::int32_t deltaLeft = 0;
    std::int32_t deltaRight = 0;
    int sequencerIndex = 0;

    std::array<std::uint32_t, memorySize> memory {};
    std::array<std::uint32_t, sequencerLength> sequencer {};
};

} // namespace digitalnoise
