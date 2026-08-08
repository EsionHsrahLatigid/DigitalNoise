#include "DigitalNoiseEngine.h"
#include "DigitalNoiseStateMigration.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

using digitalnoise::DigitalNoiseEngine;
using digitalnoise::Parameters;

namespace
{

std::vector<float> renderLeft (std::uint32_t seed, Parameters params, int samples)
{
    DigitalNoiseEngine engine;
    engine.prepare (48000.0);
    engine.setParameters (params);
    engine.reset (seed);

    std::vector<float> output;
    output.reserve (static_cast<std::size_t> (samples));

    for (int i = 0; i < samples; ++i)
        output.push_back (engine.processSample().left);

    return output;
}

void testDeterministicSameSeed()
{
    Parameters params;
    params.clockHz = 9000.0f;
    params.topology = 0.66f;
    params.mutation = 0.42f;

    assert (renderLeft (12345u, params, 512) == renderLeft (12345u, params, 512));
}

void testDifferentSeedsDiverge()
{
    Parameters params;
    const auto a = renderLeft (111u, params, 512);
    const auto b = renderLeft (222u, params, 512);

    int different = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        different += a[i] != b[i] ? 1 : 0;

    assert (different > 400);
}

void testFiniteBoundedExtremeParameters()
{
    Parameters params;
    params.clockHz = 1000000.0f;
    params.topology = -10.0f;
    params.mutation = 10.0f;
    params.memoryDepth = 10.0f;
    params.addressScramble = 10.0f;
    params.feedback = 10.0f;
    params.stereoDivergence = 10.0f;
    params.intensity = 10.0f;
    params.rawMisread = 10.0f;
    params.formatSmash = 10.0f;
    params.outputGain = 10.0f;

    DigitalNoiseEngine engine;
    engine.prepare (0.0);
    engine.setParameters (params);
    engine.reset (999u);

    for (int i = 0; i < 2048; ++i)
    {
        const auto frame = engine.processSample();
        assert (std::isfinite (frame.left));
        assert (std::isfinite (frame.right));
        assert (frame.left >= -0.9501f && frame.left <= 0.9501f);
        assert (frame.right >= -0.9501f && frame.right <= 0.9501f);
    }
}

void testResetReproducibility()
{
    Parameters params;
    params.clockHz = 12000.0f;
    params.feedback = 0.91f;
    params.memoryDepth = 0.35f;

    DigitalNoiseEngine engine;
    engine.prepare (44100.0);
    engine.setParameters (params);
    engine.reset (0xabcdefu);

    std::vector<float> first;
    for (int i = 0; i < 256; ++i)
        first.push_back (engine.processSample().right);

    for (int i = 0; i < 97; ++i)
        (void) engine.processSample();

    engine.reset (0xabcdefu);
    for (int i = 0; i < 256; ++i)
        assert (first[static_cast<std::size_t> (i)] == engine.processSample().right);
}

void testNontrivialStructure()
{
    Parameters params;
    params.clockHz = 7000.0f;
    params.intensity = 1.0f;

    DigitalNoiseEngine engine;
    engine.prepare (48000.0);
    engine.setParameters (params);
    engine.reset (42u);

    float minValue = 1.0f;
    float maxValue = -1.0f;
    int zeroCrossings = 0;
    auto previous = engine.processSample().left;

    for (int i = 1; i < 4096; ++i)
    {
        const auto current = engine.processSample().left;
        minValue = std::min (minValue, current);
        maxValue = std::max (maxValue, current);
        if ((previous < 0.0f && current >= 0.0f) || (previous >= 0.0f && current < 0.0f))
            ++zeroCrossings;
        previous = current;
    }

    assert (maxValue - minValue > 0.25f);
    assert (zeroCrossings > 12);
    assert (zeroCrossings < 3900);
}

void testStereoDivergence()
{
    Parameters params;
    params.stereoDivergence = 1.0f;
    params.clockHz = 11025.0f;

    DigitalNoiseEngine engine;
    engine.prepare (48000.0);
    engine.setParameters (params);
    engine.reset (2026u);

    float accumulatedDifference = 0.0f;
    for (int i = 0; i < 1024; ++i)
    {
        const auto frame = engine.processSample();
        accumulatedDifference += std::fabs (frame.left - frame.right);
    }

    assert (accumulatedDifference > 10.0f);
}

void testNonFiniteParametersFallBackSafely()
{
    Parameters params;
    params.clockHz = std::numeric_limits<float>::infinity();
    params.topology = std::numeric_limits<float>::quiet_NaN();
    params.mutation = -std::numeric_limits<float>::infinity();
    params.memoryDepth = std::numeric_limits<float>::quiet_NaN();
    params.addressScramble = std::numeric_limits<float>::infinity();
    params.feedback = std::numeric_limits<float>::quiet_NaN();
    params.stereoDivergence = std::numeric_limits<float>::infinity();
    params.intensity = std::numeric_limits<float>::quiet_NaN();
    params.rawMisread = std::numeric_limits<float>::infinity();
    params.formatSmash = std::numeric_limits<float>::quiet_NaN();
    params.outputGain = std::numeric_limits<float>::infinity();

    DigitalNoiseEngine engine;
    engine.prepare (std::numeric_limits<double>::infinity());
    engine.setParameters (params);
    engine.reset (77u);

    for (int i = 0; i < 512; ++i)
    {
        const auto frame = engine.processSample();
        assert (std::isfinite (frame.left));
        assert (std::isfinite (frame.right));
        assert (frame.left >= -0.9501f && frame.left <= 0.9501f);
        assert (frame.right >= -0.9501f && frame.right <= 0.9501f);
    }
}

void testClockExposesSampleAndHoldStructure()
{
    Parameters params;
    params.clockHz = 100.0f;
    params.outputGain = 0.5f;

    DigitalNoiseEngine engine;
    engine.prepare (48000.0);
    engine.setParameters (params);
    engine.reset (123u);

    const auto initial = engine.processSample();
    for (int i = 1; i < 470; ++i)
    {
        const auto held = engine.processSample();
        assert (held.left == initial.left);
        assert (held.right == initial.right);
    }

    bool changedAfterTick = false;
    for (int i = 0; i < 32; ++i)
    {
        const auto frame = engine.processSample();
        changedAfterTick = changedAfterTick || frame.left != initial.left || frame.right != initial.right;
    }
    assert (changedAfterTick);
}

void testStructuralParametersChangeTheSequence()
{
    Parameters first;
    first.topology = 0.05f;
    first.memoryDepth = 0.05f;
    first.addressScramble = 0.1f;

    Parameters second = first;
    second.topology = 0.95f;
    second.memoryDepth = 0.95f;
    second.addressScramble = 0.9f;

    const auto a = renderLeft (4242u, first, 2048);
    const auto b = renderLeft (4242u, second, 2048);
    int different = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        different += a[i] != b[i] ? 1 : 0;

    assert (different > 1800);
}

void testRawSignedEightBitDecoderLoopsOverBlob()
{
    Parameters params;
    params.clockHz = 1.0f;
    params.rawMisread = 1.0f;
    params.formatSmash = 0.0f;
    params.outputGain = 0.5f;

    const auto output = renderLeft (0x444e5a31u, params, 8192);
    for (std::size_t i = 0; i < 4096; ++i)
        assert (output[i] == output[i + 4096]);
}

void testRawDecoderFormatsProduceDifferentStructures()
{
    Parameters signedEight;
    signedEight.clockHz = 1.0f;
    signedEight.rawMisread = 1.0f;
    signedEight.formatSmash = 0.0f;

    Parameters bigEndianTwentyFour = signedEight;
    bigEndianTwentyFour.formatSmash = 0.70f;

    const auto a = renderLeft (31337u, signedEight, 2048);
    const auto b = renderLeft (31337u, bigEndianTwentyFour, 2048);
    int different = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        different += a[i] != b[i] ? 1 : 0;

    assert (different > 1900);
}

void testRawDecoderExposesStructuredBinarySuperblock()
{
    Parameters params;
    params.clockHz = 1.0f;
    params.rawMisread = 1.0f;
    params.formatSmash = 0.0f;
    params.stereoDivergence = 0.0f;
    params.outputGain = 1.0f;

    DigitalNoiseEngine engine;
    engine.prepare (48000.0);
    engine.setParameters (params);
    engine.reset (17041u);

    const auto first = engine.processSample();
    const auto second = engine.processSample();
    assert (std::fabs (first.left - (66.0f / 128.0f)) < 0.0001f);
    assert (std::fabs (first.right - (76.0f / 128.0f)) < 0.0001f);
    assert (std::fabs (second.left - (79.0f / 128.0f)) < 0.0001f);
    assert (std::fabs (second.right - (66.0f / 128.0f)) < 0.0001f);
}

void testRawDecoderExposesMixedContainerStructures()
{
    Parameters params;
    params.clockHz = 1.0f;
    params.rawMisread = 1.0f;
    params.formatSmash = 0.0f;
    params.stereoDivergence = 0.0f;
    params.outputGain = 1.0f;

    DigitalNoiseEngine engine;
    engine.prepare (48000.0);
    engine.setParameters (params);
    engine.reset (17041u);

    std::vector<digitalnoise::StereoFrame> output;
    output.reserve (1200u);
    for (int i = 0; i < 1200; ++i)
        output.push_back (engine.processSample());

    assert (std::fabs (output[258u].left - (102.0f / 128.0f)) < 0.0001f);
    assert (std::fabs (output[258u].right - (116.0f / 128.0f)) < 0.0001f);
    assert (std::fabs (output[259u].left - (121.0f / 128.0f)) < 0.0001f);
    assert (std::fabs (output[259u].right - (112.0f / 128.0f)) < 0.0001f);

    for (const auto frame : { 1024u, 1072u, 1120u })
    {
        assert (std::fabs (output[frame].left - (80.0f / 128.0f)) < 0.0001f);
        assert (std::fabs (output[frame].right - (75.0f / 128.0f)) < 0.0001f);
        assert (std::fabs (output[frame + 1u].left - (84.0f / 128.0f)) < 0.0001f);
        assert (std::fabs (output[frame + 1u].right - (33.0f / 128.0f)) < 0.0001f);
    }
}

void testLegacyStateParameterMigration()
{
    const std::array<float, digitalnoise::legacyStateParameterCount> legacy {
        8600.0f, 0.52f, 0.28f, 0.72f, 0.68f,
        0.38f, 0.58f, 0.86f, -18.0f, 4242.0f
    };

    const auto migrated = digitalnoise::migrateLegacyStateParameters (legacy, 0.29f);
    for (std::size_t i = 0; i < 8; ++i)
        assert (migrated[i] == legacy[i]);
    assert (migrated[8] == 0.0f);
    assert (migrated[9] == 0.29f);
    assert (migrated[10] == -18.0f);
    assert (migrated[11] == 4242.0f);
}

} // namespace

int main()
{
    testDeterministicSameSeed();
    testDifferentSeedsDiverge();
    testFiniteBoundedExtremeParameters();
    testResetReproducibility();
    testNontrivialStructure();
    testStereoDivergence();
    testNonFiniteParametersFallBackSafely();
    testClockExposesSampleAndHoldStructure();
    testStructuralParametersChangeTheSequence();
    testRawSignedEightBitDecoderLoopsOverBlob();
    testRawDecoderFormatsProduceDifferentStructures();
    testRawDecoderExposesStructuredBinarySuperblock();
    testRawDecoderExposesMixedContainerStructures();
    testLegacyStateParameterMigration();
    std::cout << "DigitalNoiseEngineTests passed\n";
    return 0;
}
