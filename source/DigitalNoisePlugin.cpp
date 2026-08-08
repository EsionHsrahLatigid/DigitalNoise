#include "DigitalNoisePlugin.h"

#include "DigitalNoiseEditor.h"
#include "DigitalNoiseStateMigration.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace
{
constexpr char stateMagic[] = { 'D', 'N', 'Z', '1' };
constexpr int stateVersion = 2;
constexpr int legacyStateVersion = 1;
constexpr std::array<yup::uint32, 12> hostParameterIDs {{
    0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u,
    10u, 11u,
    8u, 9u
}};

yup::NormalisableRange<float> makeRateRange()
{
    auto range = yup::NormalisableRange<float> (20.0f, 24000.0f);
    range.setSkewForCentre (1000.0f);
    return range;
}

yup::String percentString (float value)
{
    return yup::String (std::lround (std::clamp (value, 0.0f, 1.0f) * 100.0f)) + "%";
}

yup::String decoderString (float value)
{
    constexpr std::array<const char*, 8> names {{
        "S8", "U8", "S16 LE", "S16 BE", "S24 LE", "S24 BE", "1-bit", "Delta8"
    }};
    const auto position = std::clamp (value, 0.0f, 1.0f) * static_cast<float> (names.size());
    const auto mode = std::min (names.size() - 1u, static_cast<std::size_t> (position));
    const auto fracture = std::lround ((position - static_cast<float> (mode)) * 17.0f);
    return yup::String (names[mode]) + " / +" + yup::String (fracture) + "B";
}

constexpr std::array<std::array<float, 12>, 4> presetValues {{
    {{ 8600.0f, 0.52f, 0.28f, 0.72f, 0.68f, 0.38f, 0.58f, 0.88f, 0.78f, 0.29f, -6.0f, 17041.0f }},
    {{ 13200.0f, 0.76f, 0.62f, 0.93f, 0.91f, 0.73f, 0.84f, 1.0f, 0.98f, 0.48f, -5.5f, 31337.0f }},
    {{ 4200.0f, 0.94f, 0.91f, 0.48f, 0.79f, 0.88f, 0.31f, 0.96f, 0.90f, 0.82f, -5.5f, 48113.0f }},
    {{ 360.0f, 0.18f, 0.97f, 0.95f, 0.91f, 0.80f, 0.96f, 1.0f, 1.0f, 0.97f, -4.5f, 8191.0f }}
}};
} // namespace

DigitalNoisePlugin::DigitalNoisePlugin()
    : yup::AudioProcessor ("DigitalNoise",
                           yup::AudioBusLayout ({}, { yup::AudioBus ("main", yup::AudioBus::Audio, yup::AudioBus::Output, 2) }))
{
    static_assert (parameterCount == digitalnoise::currentStateParameterCount);
    static_assert (parameterCount == hostParameterIDs.size());
    static_assert (hostParameterIDs[output] == 8u && hostParameterIDs[seed] == 9u);
    static_assert (hostParameterIDs[rawMisread] == 10u && hostParameterIDs[formatSmash] == 11u);
    parameters[rate] = yup::AudioParameterBuilder()
                           .withID ("rate")
                           .withName ("Machine Rate")
                           .withHostID (hostParameterIDs[rate])
                           .withRange (makeRateRange())
                           .withDefault (8600.0f)
                           .withSmoothing (12.0f)
                           .withModulatable (true)
                           .withUnit (yup::AudioParameter::ParameterUnit::Hertz)
                           .build();
    parameters[topology] = yup::AudioParameterBuilder()
                               .withID ("topology")
                               .withName ("CA Topology")
                               .withHostID (hostParameterIDs[topology])
                               .withRange (0.0f, 1.0f)
                               .withDefault (0.52f)
                               .withSmoothing (25.0f)
                               .withModulatable (true)
                               .build();
    parameters[mutation] = yup::AudioParameterBuilder()
                               .withID ("mutation")
                               .withName ("Mutation")
                               .withHostID (hostParameterIDs[mutation])
                               .withRange (0.0f, 1.0f)
                               .withDefault (0.28f)
                               .withSmoothing (25.0f)
                               .withModulatable (true)
                               .build();
    parameters[memoryDepth] = yup::AudioParameterBuilder()
                                  .withID ("memory_depth")
                                  .withName ("Memory Depth")
                                  .withHostID (hostParameterIDs[memoryDepth])
                                  .withRange (0.0f, 1.0f)
                                  .withDefault (0.72f)
                                  .withSmoothing (20.0f)
                                  .withModulatable (true)
                                  .build();
    parameters[addressScramble] = yup::AudioParameterBuilder()
                                      .withID ("address_scramble")
                                      .withName ("Address Scramble")
                                      .withHostID (hostParameterIDs[addressScramble])
                                      .withRange (0.0f, 1.0f)
                                      .withDefault (0.68f)
                                      .withSmoothing (20.0f)
                                      .withModulatable (true)
                                      .build();
    parameters[feedback] = yup::AudioParameterBuilder()
                               .withID ("feedback")
                               .withName ("Memory Feedback")
                               .withHostID (hostParameterIDs[feedback])
                               .withRange (0.0f, 1.0f)
                               .withDefault (0.38f)
                               .withSmoothing (30.0f)
                               .withModulatable (true)
                               .build();
    parameters[stereoDivergence] = yup::AudioParameterBuilder()
                                       .withID ("stereo")
                                       .withName ("Stereo Divergence")
                                       .withHostID (hostParameterIDs[stereoDivergence])
                                       .withRange (0.0f, 1.0f)
                                       .withDefault (0.58f)
                                       .withSmoothing (20.0f)
                                       .withModulatable (true)
                                       .build();
    parameters[intensity] = yup::AudioParameterBuilder()
                                .withID ("intensity")
                                .withName ("Bitplane Intensity")
                                .withHostID (hostParameterIDs[intensity])
                                .withRange (0.0f, 1.0f)
                                .withDefault (0.88f)
                                .withSmoothing (15.0f)
                                .withModulatable (true)
                                .build();
    parameters[rawMisread] = yup::AudioParameterBuilder()
                                 .withID ("raw_misread")
                                 .withName ("Raw Misread")
                                 .withHostID (hostParameterIDs[rawMisread])
                                 .withRange (0.0f, 1.0f)
                                 .withDefault (0.78f)
                                 .withSmoothing (8.0f)
                                 .withModulatable (true)
                                 .withValueToString ([] (float value) { return percentString (value); })
                                 .build();
    parameters[formatSmash] = yup::AudioParameterBuilder()
                                  .withID ("format_smash")
                                  .withName ("Format Smash")
                                  .withHostID (hostParameterIDs[formatSmash])
                                  .withRange (0.0f, 1.0f)
                                  .withDefault (0.29f)
                                  .withSmoothing (0.0f)
                                  .withModulatable (true)
                                  .withValueToString ([] (float value) { return decoderString (value); })
                                  .build();
    parameters[output] = yup::AudioParameterBuilder()
                             .withID ("output")
                             .withName ("Output")
                             .withHostID (hostParameterIDs[output])
                             .withRange (-48.0f, 6.0f)
                             .withDefault (-6.0f)
                             .withSmoothing (30.0f)
                             .withModulatable (true)
                             .withUnit (yup::AudioParameter::ParameterUnit::Decibels)
                             .build();
    parameters[seed] = yup::AudioParameterBuilder()
                           .withID ("seed")
                           .withName ("Graph Seed")
                           .withHostID (hostParameterIDs[seed])
                           .withRange (1.0f, 65535.0f)
                           .withDefault (17041.0f)
                           .withStepped (true)
                           .withAutomatable (false)
                           .withModulatable (false)
                           .build();

    for (const auto& parameter : parameters)
        addParameter (parameter);
}

void DigitalNoisePlugin::prepareToPlay (const yup::AudioSpec& spec)
{
    engine.prepare (spec.sampleRate);

    for (std::size_t i = 0; i < parameterHandles.size(); ++i)
        parameterHandles[i] = yup::AudioParameterHandle (*parameters[i], spec.sampleRate);

    resetGraph (readSeedParameter());
}

void DigitalNoisePlugin::releaseResources()
{
}

void DigitalNoisePlugin::processBlock (yup::AudioProcessContext<float>& context)
{
    auto& audio = context.audio;
    const auto numSamples = audio.getNumSamples();
    const auto numChannels = audio.getNumChannels();

    for (std::size_t i = 0; i < parameterHandles.size(); ++i)
        parameterHandles[i].prepareBlock (context.params, parameters[i]->getIndexInContainer());

    const auto requestedSeed = readSeedParameter();
    if (requestedSeed != currentSeed)
        queueGraphReset (requestedSeed);
    if (const auto queuedSeed = pendingSeed.exchange (0u, std::memory_order_acq_rel); queuedSeed != 0u)
        resetGraph (queuedSeed);

    auto midi = context.midi.begin();
    const auto midiEnd = context.midi.end();
    auto* left = numChannels > 0 ? audio.getWritePointer (0) : nullptr;
    auto* right = numChannels > 1 ? audio.getWritePointer (1) : nullptr;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        while (midi != midiEnd && (*midi).samplePosition <= sample)
        {
            const auto& message = (*midi).getMessage();
            if (message.isNoteOn())
            {
                const auto note = std::clamp (message.getNoteNumber(), 0, 127);
                midiRateMultiplier = std::exp2 ((static_cast<float> (note) - 60.0f) / 12.0f);
                engine.reseedGraph (currentSeed ^ (static_cast<std::uint32_t> (note + 1) * 0x9e3779b9u));
            }
            ++midi;
        }

        updateEngineParameters (sample);
        const auto frame = engine.processSample();

        if (left != nullptr)
            left[sample] = frame.left;
        if (right != nullptr)
            right[sample] = frame.right;

        for (int channel = 2; channel < numChannels; ++channel)
            audio.getWritePointer (channel)[sample] = 0.0f;
    }

    context.midi.clear();
}

void DigitalNoisePlugin::flush()
{
    midiRateMultiplier = 1.0f;
    queueGraphReset (readSeedParameter());
}

bool DigitalNoisePlugin::acceptsMidi() const noexcept
{
    return true;
}

int DigitalNoisePlugin::getCurrentPreset() const noexcept
{
    return currentPreset.load (std::memory_order_relaxed);
}

void DigitalNoisePlugin::setCurrentPreset (int index) noexcept
{
    if (! yup::isPositiveAndBelow (index, static_cast<int> (presetValues.size())))
        return;

    currentPreset.store (index, std::memory_order_relaxed);
    for (std::size_t i = 0; i < parameters.size(); ++i)
        parameters[i]->setValue (presetValues[static_cast<std::size_t> (index)][i]);

    queueGraphReset (readSeedParameter());
}

int DigitalNoisePlugin::getNumPresets() const
{
    return static_cast<int> (presetNames.size());
}

yup::String DigitalNoisePlugin::getPresetName (int index) const
{
    if (yup::isPositiveAndBelow (index, static_cast<int> (presetNames.size())))
        return presetNames[static_cast<std::size_t> (index)];
    return "Invalid Preset";
}

void DigitalNoisePlugin::setPresetName (int index, yup::StringRef newName)
{
    if (yup::isPositiveAndBelow (index, static_cast<int> (presetNames.size())))
        presetNames[static_cast<std::size_t> (index)] = newName;
}

yup::Result DigitalNoisePlugin::loadStateFromMemory (const yup::MemoryBlock& data)
{
    constexpr auto headerSize = sizeof (stateMagic) + sizeof (int) * 2;
    if (data.getSize() < headerSize)
        return yup::Result::fail ("Invalid DigitalNoise state size");

    yup::MemoryInputStream stream (data, false);
    char magic[sizeof (stateMagic)] {};
    if (stream.read (magic, sizeof (magic)) != static_cast<int> (sizeof (magic)))
        return yup::Result::fail ("Invalid DigitalNoise state header");

    for (std::size_t i = 0; i < sizeof (stateMagic); ++i)
        if (magic[i] != stateMagic[i])
            return yup::Result::fail ("Invalid DigitalNoise state header");

    const auto version = stream.readInt();
    if (version != stateVersion && version != legacyStateVersion)
        return yup::Result::fail ("Unsupported DigitalNoise state version");

    const auto savedParameterCount = version == legacyStateVersion
                                       ? digitalnoise::legacyStateParameterCount
                                       : digitalnoise::currentStateParameterCount;
    const auto expectedSize = headerSize + sizeof (float) * static_cast<std::size_t> (savedParameterCount);
    if (data.getSize() != expectedSize)
        return yup::Result::fail ("Invalid DigitalNoise state size");

    const auto loadedPreset = stream.readInt();
    if (! yup::isPositiveAndBelow (loadedPreset, getNumPresets()))
        return yup::Result::fail ("Invalid DigitalNoise preset index");

    std::array<float, parameterCount> values {};
    for (std::size_t i = 0; i < values.size(); ++i)
        values[i] = parameters[i]->getDefaultValue();

    if (version == legacyStateVersion)
    {
        std::array<float, digitalnoise::legacyStateParameterCount> legacyValues {};
        for (auto& value : legacyValues)
            value = stream.readFloat();

        values = digitalnoise::migrateLegacyStateParameters (
            legacyValues,
            parameters[formatSmash]->getDefaultValue());
    }
    else
    {
        for (auto& value : values)
            value = stream.readFloat();
    }

    for (std::size_t i = 0; i < values.size(); ++i)
    {
        if (! std::isfinite (values[i])
            || values[i] < parameters[i]->getMinimumValue()
            || values[i] > parameters[i]->getMaximumValue())
            return yup::Result::fail ("Invalid DigitalNoise parameter value");
    }

    currentPreset.store (loadedPreset, std::memory_order_relaxed);
    for (std::size_t i = 0; i < values.size(); ++i)
        parameters[i]->setValue (values[i]);
    queueGraphReset (static_cast<std::uint32_t> (std::lround (values[seed])));
    return yup::Result::ok();
}

yup::Result DigitalNoisePlugin::saveStateIntoMemory (yup::MemoryBlock& data)
{
    yup::MemoryOutputStream stream (data, false);
    if (! stream.write (stateMagic, sizeof (stateMagic))
        || ! stream.writeInt (stateVersion)
        || ! stream.writeInt (currentPreset.load (std::memory_order_relaxed)))
        return yup::Result::fail ("Failed to write DigitalNoise state header");

    for (const auto& parameter : parameters)
    {
        const auto value = parameter->getValue();
        if (! std::isfinite (value) || ! stream.writeFloat (value))
            return yup::Result::fail ("Failed to write DigitalNoise parameter state");
    }

    stream.flush();
    return yup::Result::ok();
}

bool DigitalNoisePlugin::hasEditor() const
{
    return true;
}

yup::AudioProcessorEditor* DigitalNoisePlugin::createEditor()
{
    return new DigitalNoiseEditor (*this);
}

void DigitalNoisePlugin::updateEngineParameters (int samplePosition)
{
    for (auto& handle : parameterHandles)
        handle.advanceToSample (samplePosition);

    digitalnoise::Parameters engineParameters;
    engineParameters.clockHz = std::clamp (parameterHandles[rate].getNextValue() * midiRateMultiplier, 1.0f, 24000.0f);
    engineParameters.topology = parameterHandles[topology].getNextValue();
    engineParameters.mutation = parameterHandles[mutation].getNextValue();
    engineParameters.memoryDepth = parameterHandles[memoryDepth].getNextValue();
    engineParameters.addressScramble = parameterHandles[addressScramble].getNextValue();
    engineParameters.feedback = parameterHandles[feedback].getNextValue();
    engineParameters.stereoDivergence = parameterHandles[stereoDivergence].getNextValue();
    engineParameters.intensity = parameterHandles[intensity].getNextValue();
    engineParameters.rawMisread = parameterHandles[rawMisread].getNextValue();
    engineParameters.formatSmash = parameterHandles[formatSmash].getNextValue();
    engineParameters.outputGain = std::pow (10.0f, parameterHandles[output].getNextValue() / 20.0f);

    engine.setParameters (engineParameters);
}

void DigitalNoisePlugin::resetGraph (std::uint32_t newSeed) noexcept
{
    currentSeed = newSeed != 0u ? newSeed : 1u;
    engine.reset (currentSeed);
}

std::uint32_t DigitalNoisePlugin::readSeedParameter() const noexcept
{
    const auto value = parameters[seed]->getValue();
    if (! std::isfinite (value))
        return 1u;
    return static_cast<std::uint32_t> (std::lround (std::clamp (value, 1.0f, 65535.0f)));
}

void DigitalNoisePlugin::queueGraphReset (std::uint32_t newSeed) noexcept
{
    pendingSeed.store (newSeed != 0u ? newSeed : 1u, std::memory_order_release);
}

extern "C" yup::AudioProcessor* createPluginProcessor()
{
    return new DigitalNoisePlugin();
}
