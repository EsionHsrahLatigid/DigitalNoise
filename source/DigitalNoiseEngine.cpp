#include "DigitalNoiseEngine.h"

#include <algorithm>
#include <cmath>

namespace digitalnoise
{

DigitalNoiseEngine::DigitalNoiseEngine()
{
    reset (1u);
}

void DigitalNoiseEngine::prepare (double newSampleRate) noexcept
{
    sampleRate = std::isfinite (newSampleRate) && newSampleRate > 1.0 ? newSampleRate : 44100.0;
    phase = 0.0f;
}

void DigitalNoiseEngine::reset (std::uint32_t seed) noexcept
{
    reseedGraph (seed);
    reseedMemory();
}

void DigitalNoiseEngine::reseedGraph (std::uint32_t seed) noexcept
{
    initialSeed = seed != 0u ? seed : 1u;
    lfsrA = mixSeed (initialSeed ^ 0x9e3779b9u) | 1u;
    lfsrB = mixSeed (initialSeed ^ 0x85ebca6bu) | 1u;
    cellsA = mixSeed (initialSeed ^ 0xc2b2ae35u);
    cellsB = mixSeed (initialSeed ^ 0x27d4eb2fu);
    address = mixSeed (initialSeed ^ 0x165667b1u) & memoryMask;
    graphWord = mixSeed (initialSeed ^ 0xd3a2646cu);
    holdLeft = graphWord;
    holdRight = rotateLeft (graphWord, 13);
    sampleCounter = 0u;
    blobCursor = 0u;
    deltaLeft = 0;
    deltaRight = 0;
    sequencerIndex = 0;
    phase = 0.0f;
    rebuildSequencer();
}

void DigitalNoiseEngine::setParameters (const Parameters& parameters) noexcept
{
    const auto topology = clamp (parameters.topology, 0.0f, 1.0f);
    const auto mutation = clamp (parameters.mutation, 0.0f, 1.0f);
    const auto graphShapeChanged = topology != params.topology || mutation != params.mutation;

    params.clockHz = clamp (parameters.clockHz, 1.0f, 24000.0f);
    params.topology = topology;
    params.mutation = mutation;
    params.memoryDepth = clamp (parameters.memoryDepth, 0.0f, 1.0f);
    params.addressScramble = clamp (parameters.addressScramble, 0.0f, 1.0f);
    params.feedback = clamp (parameters.feedback, 0.0f, 1.0f);
    params.stereoDivergence = clamp (parameters.stereoDivergence, 0.0f, 1.0f);
    params.intensity = clamp (parameters.intensity, 0.0f, 1.0f);
    params.rawMisread = clamp (parameters.rawMisread, 0.0f, 1.0f);
    params.formatSmash = clamp (parameters.formatSmash, 0.0f, 1.0f);
    params.outputGain = clamp (parameters.outputGain, 0.0f, 2.0f);

    if (graphShapeChanged)
        rebuildSequencer();
}

StereoFrame DigitalNoiseEngine::processSample() noexcept
{
    const auto increment = static_cast<float> (params.clockHz / sampleRate);
    phase += increment;

    if (phase >= 1.0f)
    {
        do
        {
            phase -= 1.0f;
            tickGraph();
        } while (phase >= 1.0f);
    }

    const auto depth = memoryDepthSamples();
    const auto smear = rotateLeft (graphWord ^ cellsA, static_cast<int> ((params.addressScramble * 31.0f) + 1.0f));
    const auto tapA = (address - ((smear ^ lfsrA) % depth)) & memoryMask;
    const auto tapB = (address - ((rotateLeft (smear, 11) ^ lfsrB) % depth)) & memoryMask;
    const auto memA = memory[tapA];
    const auto memB = memory[tapB];

    const auto leftWord = holdLeft ^ memA ^ rotateLeft (cellsA, 7) ^ (lfsrA + graphWord);
    const auto rightWord = holdRight ^ memB ^ rotateLeft (cellsB, 19) ^ (lfsrB + rotateLeft (graphWord, 5));

    const auto cross = bitsToSignedFloat ((leftWord & 0xffff0000u) | (rightWord >> 16));
    const auto left = bitsToSignedFloat (leftWord) * (0.55f + 0.45f * params.intensity);
    const auto right = bitsToSignedFloat (rightWord) * (0.55f + 0.45f * params.intensity);

    const auto monoBlend = 0.5f * (1.0f - params.stereoDivergence);
    const auto diverge = params.stereoDivergence;
    const auto graphLeft = left * (1.0f - monoBlend) + right * monoBlend + cross * 0.15f * diverge;
    const auto graphRight = right * (1.0f - monoBlend) + left * monoBlend - cross * 0.15f * diverge;

    const auto raw = decodeBlobFrame();
    const auto rawAmount = params.rawMisread;
    const auto mixedLeft = graphLeft + (raw.left - graphLeft) * rawAmount;
    const auto mixedRight = graphRight + (raw.right - graphRight) * rawAmount;

    constexpr float softLimit = 0.95f;
    const auto gain = params.outputGain;
    return { clamp (mixedLeft * gain, -softLimit, softLimit),
             clamp (mixedRight * gain, -softLimit, softLimit) };
}

void DigitalNoiseEngine::process (float* left, float* right, int numSamples) noexcept
{
    if (left == nullptr || right == nullptr || numSamples <= 0)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto frame = processSample();
        left[i] = frame.left;
        right[i] = frame.right;
    }
}

float DigitalNoiseEngine::clamp (float value, float low, float high) noexcept
{
    if (! std::isfinite (value))
        return low;
    return value < low ? low : (value > high ? high : value);
}

std::uint32_t DigitalNoiseEngine::mixSeed (std::uint32_t value) noexcept
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value != 0u ? value : 0x13579bdfu;
}

std::uint32_t DigitalNoiseEngine::stepLfsr (std::uint32_t state) noexcept
{
    const auto lsb = state & 1u;
    state >>= 1;
    state ^= static_cast<std::uint32_t> (0u - lsb) & 0x80200003u;
    return state != 0u ? state : 1u;
}

std::uint32_t DigitalNoiseEngine::rotateLeft (std::uint32_t value, int bits) noexcept
{
    const auto amount = static_cast<unsigned> (bits) & 31u;
    return amount == 0u ? value : ((value << amount) | (value >> (32u - amount)));
}

float DigitalNoiseEngine::bitsToSignedFloat (std::uint32_t value) noexcept
{
    const auto folded = static_cast<int> (((value >> 1) & 0xffffu) ^ ((value >> 17) & 0xffffu));
    return (static_cast<float> (folded) / 32767.5f) - 1.0f;
}

std::uint32_t DigitalNoiseEngine::unitToMask (float value) noexcept
{
    if (value <= 0.0f)
        return 0u;
    if (value >= 1.0f)
        return 0xffffffffu;
    return static_cast<std::uint32_t> (static_cast<double> (value) * 4294967295.0);
}

void DigitalNoiseEngine::rebuildSequencer() noexcept
{
    auto state = mixSeed (initialSeed ^ static_cast<std::uint32_t> (params.topology * 65535.0f));
    for (auto& step : sequencer)
    {
        state = mixSeed (state + 0x9e3779b9u);
        const auto bitPlane = rotateLeft (state, static_cast<int> (params.mutation * 23.0f));
        step = (state & 0x000000ffu)
             | ((bitPlane & 0x0000000fu) << 8)
             | ((state & 0x00070000u) >> 4)
             | (rotateLeft (state, 17) & 0x00f00000u);
    }
}

void DigitalNoiseEngine::reseedMemory() noexcept
{
    auto state = mixSeed (initialSeed ^ 0x94d049bbu);
    auto ca = cellsA;
    for (auto& word : memory)
    {
        state = stepLfsr (state);
        const auto left = rotateLeft (ca, 1);
        const auto right = rotateLeft (ca, 31);
        ca = left ^ (ca | right) ^ state; // Bit-parallel Rule 30 with LFSR injection.
        word = state ^ rotateLeft (ca, static_cast<int> (state & 31u));
    }

    // Overlay a deliberately inconsistent structured binary image onto the
    // generated payload. The image mixes a superblock, directory, one ISO-box
    // fragment, aligned sparse space, fixed-stride packets, a node tree,
    // pointers, and a footer. The graph later corrupts it in-place, making the
    // structural repetition and its destruction audible without file I/O.
    const auto writeHeader = [this] (std::uint32_t offset,
                                     std::uint32_t size,
                                     std::uint32_t type) noexcept
    {
        writeBlobBigEndian32 (offset, size);
        writeBlobBigEndian32 (offset + 4u, type);
    };

    writeBlobBigEndian32 (0u, 0x424c4f42u); // BLOB
    writeBlobBigEndian32 (4u, 3u);
    writeBlobBigEndian32 (8u, blobByteSize);
    writeBlobBigEndian32 (12u, 64u);
    writeBlobBigEndian32 (16u, 8u);
    writeBlobBigEndian32 (20u, initialSeed);
    writeBlobBigEndian32 (24u, mixSeed (initialSeed ^ 0x504f4c59u));
    writeBlobBigEndian32 (28u, 0x00010007u);

    const auto writeDirectoryEntry = [this] (std::uint32_t index,
                                              std::uint32_t type,
                                              std::uint32_t offset,
                                              std::uint32_t length,
                                              std::uint32_t flags) noexcept
    {
        const auto base = 64u + index * 16u;
        writeBlobBigEndian32 (base, type);
        writeBlobBigEndian32 (base + 4u, offset);
        writeBlobBigEndian32 (base + 8u, length);
        writeBlobBigEndian32 (base + 12u, flags);
    };

    writeDirectoryEntry (0u, 0x49534f42u, 512u, 128u, 0x00000001u);   // ISOB
    writeDirectoryEntry (1u, 0x4d455441u, 640u, 384u, 0x80000002u);   // META
    writeDirectoryEntry (2u, 0x53505253u, 1024u, 1024u, 0x00000004u); // SPRS
    writeDirectoryEntry (3u, 0x504b5453u, 2048u, 2048u, 0x00000008u); // PKTS
    writeDirectoryEntry (4u, 0x54524545u, 4096u, 1024u, 0x00000010u); // TREE
    writeDirectoryEntry (5u, 0x50545253u, 5120u, 512u, 0x00000020u);  // PTRS
    writeDirectoryEntry (6u, 0x44415441u, 5632u, 2304u, 0x00000040u); // DATA
    writeDirectoryEntry (7u, 0x464f4f54u, 7936u, 256u, 0x00000080u);  // FOOT

    // One ISO-BMFF-like region is retained as an example of box structure,
    // but it is only one entry in the larger contradictory image.
    writeHeader (512u, 32u, 0x66747970u); // ftyp
    writeBlobBigEndian32 (520u, 0x69736f6du); // isom
    writeBlobBigEndian32 (524u, 0x00000200u);
    writeBlobBigEndian32 (528u, 0x69736f6du);
    writeBlobBigEndian32 (532u, 0x6d703431u); // mp41
    writeHeader (544u, 96u, 0x6d6f6f76u); // moov
    writeHeader (552u, 40u, 0x6d766864u); // mvhd
    writeHeader (592u, 48u, 0x7472616bu); // trak

    for (std::uint32_t offset = 1024u; offset < 2048u; ++offset)
        writeBlobByte (offset, 0u);
    writeBlobBigEndian32 (1024u, 0x53505253u); // SPRS
    writeBlobBigEndian32 (1028u, 1024u);
    writeBlobBigEndian32 (2040u, 0x454e4421u); // END!

    constexpr std::uint32_t packetStride = 96u;
    for (std::uint32_t base = 2048u, sequence = 0u;
         base + packetStride <= 4096u;
         base += packetStride, ++sequence)
    {
        writeBlobBigEndian32 (base, 0x504b5421u); // PKT!
        writeBlobBigEndian32 (base + 4u, sequence);
        writeBlobBigEndian32 (base + 8u, packetStride - 24u);
        writeBlobBigEndian32 (base + 12u, sequence * 1024u);
        writeBlobBigEndian32 (base + 16u, mixSeed (initialSeed ^ sequence));
        writeBlobBigEndian32 (base + 20u, sequence & 3u);
    }

    for (std::uint32_t node = 0u; node < 16u; ++node)
    {
        const auto base = 4096u + node * 64u;
        writeBlobBigEndian32 (base, 0x4e4f4445u); // NODE
        writeBlobBigEndian32 (base + 4u, node);
        writeBlobBigEndian32 (base + 8u, node == 0u ? 0xffffffffu : (node - 1u) / 2u);
        writeBlobBigEndian32 (base + 12u, 5632u + ((node * 137u) % 2304u));
        writeBlobBigEndian32 (base + 16u, 16u + ((node * 29u) & 127u));
    }

    for (std::uint32_t pointer = 0u; pointer < 128u; ++pointer)
        writeBlobBigEndian32 (5120u + pointer * 4u,
                              5632u + ((pointer * 73u + initialSeed) % 2304u));

    writeBlobBigEndian32 (7936u, 0x464f4f54u); // FOOT
    writeBlobBigEndian32 (7940u, 64u);
    writeBlobBigEndian32 (7944u, mixSeed (initialSeed ^ 0x454e4421u));
    for (std::uint32_t offset = 8000u; offset < 8192u; offset += 32u)
    {
        writeBlobBigEndian32 (offset, 0x454e4421u); // END!
        writeBlobBigEndian32 (offset + 4u, offset);
    }
}

StereoFrame DigitalNoiseEngine::decodeBlobFrame() noexcept
{
    constexpr int decoderModes = 8;
    const auto decoderPosition = params.formatSmash * static_cast<float> (decoderModes);
    const auto mode = std::min (decoderModes - 1, static_cast<int> (decoderPosition));
    const auto modeFraction = decoderPosition - static_cast<float> (mode);
    const auto sampleWidth = mode == 2 || mode == 3 ? 2 : (mode == 4 || mode == 5 ? 3 : 1);
    const auto misalignment = static_cast<std::uint32_t> (modeFraction * 5.0f);
    const auto channelSkew = static_cast<std::uint32_t> (params.stereoDivergence * 29.0f);
    const auto channelGap = static_cast<std::uint32_t> (sampleWidth) + channelSkew;
    const auto leftAddress = (blobCursor + misalignment) & blobByteMask;
    const auto rightAddress = (leftAddress + channelGap) & blobByteMask;
    const auto bitIndex = static_cast<int> ((blobCursor >> 1u) & 7u);

    const StereoFrame frame {
        decodeBlobSample (leftAddress, mode, bitIndex, 0),
        decodeBlobSample (rightAddress, mode, 7 - bitIndex, 1)
    };

    const auto skippedBytes = static_cast<std::uint32_t> (modeFraction * 17.0f);
    blobCursor = (blobCursor + static_cast<std::uint32_t> (sampleWidth * 2) + skippedBytes) & blobByteMask;
    return frame;
}

float DigitalNoiseEngine::decodeBlobSample (std::uint32_t byteAddress, int mode, int bitIndex, int channel) noexcept
{
    const auto byte0 = static_cast<std::uint32_t> (readBlobByte (byteAddress));
    const auto byte1 = static_cast<std::uint32_t> (readBlobByte (byteAddress + 1u));
    const auto byte2 = static_cast<std::uint32_t> (readBlobByte (byteAddress + 2u));

    const auto signExtend = [] (std::uint32_t value, std::uint32_t signBit, std::uint32_t range) noexcept
    {
        return value >= signBit ? -static_cast<std::int32_t> (range - value)
                                : static_cast<std::int32_t> (value);
    };

    switch (mode)
    {
        case 0:
            return static_cast<float> (signExtend (byte0, 0x80u, 0x100u)) / 128.0f;
        case 1:
            return (static_cast<float> (byte0) - 128.0f) / 128.0f;
        case 2:
            return static_cast<float> (signExtend (byte0 | (byte1 << 8u), 0x8000u, 0x10000u)) / 32768.0f;
        case 3:
            return static_cast<float> (signExtend ((byte0 << 8u) | byte1, 0x8000u, 0x10000u)) / 32768.0f;
        case 4:
            return static_cast<float> (signExtend (byte0 | (byte1 << 8u) | (byte2 << 16u),
                                                   0x800000u,
                                                   0x1000000u)) / 8388608.0f;
        case 5:
            return static_cast<float> (signExtend ((byte0 << 16u) | (byte1 << 8u) | byte2,
                                                   0x800000u,
                                                   0x1000000u)) / 8388608.0f;
        case 6:
            return ((byte0 >> static_cast<unsigned> (bitIndex)) & 1u) != 0u ? 0.92f : -0.92f;
        case 7:
        {
            auto& accumulator = channel == 0 ? deltaLeft : deltaRight;
            const auto delta = signExtend (byte0, 0x80u, 0x100u) * 257;
            accumulator = static_cast<std::int32_t> ((static_cast<std::uint32_t> (accumulator)
                                                     + static_cast<std::uint32_t> (delta)) & 0xffffu);
            const auto signedAccumulator = accumulator >= 0x8000 ? accumulator - 0x10000 : accumulator;
            return static_cast<float> (signedAccumulator) / 32768.0f;
        }
        default:
            return 0.0f;
    }
}

std::uint8_t DigitalNoiseEngine::readBlobByte (std::uint32_t byteAddress) const noexcept
{
    const auto wrapped = byteAddress & blobByteMask;
    const auto word = memory[static_cast<std::size_t> (wrapped >> 2u)];
    const auto shift = (wrapped & 3u) * 8u;
    return static_cast<std::uint8_t> ((word >> shift) & 0xffu);
}

void DigitalNoiseEngine::writeBlobByte (std::uint32_t byteAddress, std::uint8_t value) noexcept
{
    const auto wrapped = byteAddress & blobByteMask;
    auto& word = memory[static_cast<std::size_t> (wrapped >> 2u)];
    const auto shift = (wrapped & 3u) * 8u;
    const auto byteMask = 0xffu << shift;
    word = (word & ~byteMask) | (static_cast<std::uint32_t> (value) << shift);
}

void DigitalNoiseEngine::writeBlobBigEndian32 (std::uint32_t byteAddress, std::uint32_t value) noexcept
{
    writeBlobByte (byteAddress, static_cast<std::uint8_t> (value >> 24u));
    writeBlobByte (byteAddress + 1u, static_cast<std::uint8_t> (value >> 16u));
    writeBlobByte (byteAddress + 2u, static_cast<std::uint8_t> (value >> 8u));
    writeBlobByte (byteAddress + 3u, static_cast<std::uint8_t> (value));
}

void DigitalNoiseEngine::tickGraph() noexcept
{
    const auto sequenceWord = sequencer[static_cast<std::size_t> (sequencerIndex)];
    sequencerIndex = (sequencerIndex + 1) & (sequencerLength - 1);

    lfsrA = stepLfsr (lfsrA ^ (cellsB & static_cast<std::uint32_t> (0u - ((sequenceWord >> 8) & 1u))));
    lfsrB = stepLfsr (lfsrB ^ rotateLeft (cellsA, static_cast<int> (sequenceWord & 15u)));

    const auto ruleA = static_cast<std::uint8_t> (currentRule() ^ (sequenceWord & 0x1fu));
    const auto ruleB = static_cast<std::uint8_t> (rotateLeft (currentRule(), 3) ^ ((sequenceWord >> 4) & 0xffu));
    cellsA = cellularStep (cellsA ^ lfsrA ^ rotateLeft (graphWord, 3), ruleA);
    cellsB = cellularStep (cellsB ^ lfsrB ^ rotateLeft (graphWord, 9), ruleB);

    const auto mutationMask = unitToMask (params.mutation);
    const auto arithmetic = (lfsrA + rotateLeft (cellsA, static_cast<int> (sequenceWord & 31u)))
                          ^ (lfsrB * ((sequenceWord & 31u) + 1u));
    graphWord = arithmetic ^ (mutationMask & rotateLeft (cellsA ^ cellsB, static_cast<int> ((sequenceWord >> 11) & 31u)));

    const auto depth = memoryDepthSamples();
    const auto scramble = rotateLeft (graphWord ^ sequenceWord, static_cast<int> (1.0f + params.addressScramble * 30.0f));
    address = (address + 1u + ((scramble ^ cellsA) % depth)) & memoryMask;

    const auto readA = (address - ((lfsrA ^ scramble) % depth)) & memoryMask;
    const auto readB = (address - ((lfsrB ^ rotateLeft (scramble, 13)) % depth)) & memoryMask;
    const auto feedbackWord = memory[readA] ^ rotateLeft (memory[readB], static_cast<int> (sequenceWord & 31u));
    const auto feedbackMask = unitToMask (params.feedback);

    holdLeft = graphWord ^ feedbackWord ^ rotateLeft (cellsA, static_cast<int> ((sequenceWord >> 16) & 31u));
    holdRight = rotateLeft (graphWord, 11) ^ rotateLeft (feedbackWord, 5) ^ cellsB;
    memory[address] = graphWord ^ (feedbackWord & feedbackMask) ^ rotateLeft (sampleCounter++, static_cast<int> (sequenceWord & 31u));
}

std::uint32_t DigitalNoiseEngine::cellularStep (std::uint32_t cells, std::uint8_t rule) noexcept
{
    std::uint32_t next = 0u;
    for (int bit = 0; bit < 32; ++bit)
    {
        const auto left = (cells >> ((bit + 31) & 31)) & 1u;
        const auto center = (cells >> bit) & 1u;
        const auto right = (cells >> ((bit + 1) & 31)) & 1u;
        const auto neighbourhood = (left << 2) | (center << 1) | right;
        next |= ((static_cast<std::uint32_t> (rule) >> neighbourhood) & 1u) << bit;
    }
    return next;
}

std::uint32_t DigitalNoiseEngine::currentRule() const noexcept
{
    const auto baseRule = static_cast<std::uint32_t> (30.0f + params.topology * 195.0f) & 0xffu;
    const auto morph = static_cast<std::uint32_t> (params.mutation * 255.0f) & 0xffu;
    return (baseRule ^ rotateLeft (morph, static_cast<int> (params.topology * 7.0f))) & 0xffu;
}

std::uint32_t DigitalNoiseEngine::memoryDepthSamples() const noexcept
{
    const auto scaled = 1.0f + params.memoryDepth * static_cast<float> (memorySize - 1);
    auto depth = static_cast<std::uint32_t> (scaled);
    if (depth == 0u)
        depth = 1u;
    return depth > memorySize ? memorySize : depth;
}

} // namespace digitalnoise
