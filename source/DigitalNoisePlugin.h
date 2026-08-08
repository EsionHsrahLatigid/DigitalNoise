#pragma once

#include "DigitalNoiseEngine.h"

#include <yup_audio_processors/yup_audio_processors.h>

#include <array>
#include <atomic>
#include <cstdint>

class DigitalNoisePlugin final : public yup::AudioProcessor
{
public:
    DigitalNoisePlugin();

    void prepareToPlay (const yup::AudioSpec& spec) override;
    void releaseResources() override;
    void processBlock (yup::AudioProcessContext<float>& context) override;
    void flush() override;

    bool acceptsMidi() const noexcept override;

    int getCurrentPreset() const noexcept override;
    void setCurrentPreset (int index) noexcept override;
    int getNumPresets() const override;
    yup::String getPresetName (int index) const override;
    void setPresetName (int index, yup::StringRef newName) override;

    yup::Result loadStateFromMemory (const yup::MemoryBlock& data) override;
    yup::Result saveStateIntoMemory (yup::MemoryBlock& data) override;

    bool hasEditor() const override;
    yup::AudioProcessorEditor* createEditor() override;

private:
    enum ParameterIndex
    {
        rate,
        topology,
        mutation,
        memoryDepth,
        addressScramble,
        feedback,
        stereoDivergence,
        intensity,
        rawMisread,
        formatSmash,
        output,
        seed,
        parameterCount
    };

    void updateEngineParameters (int samplePosition);
    void resetGraph (std::uint32_t newSeed) noexcept;
    std::uint32_t readSeedParameter() const noexcept;
    void queueGraphReset (std::uint32_t newSeed) noexcept;

    std::array<yup::AudioParameter::Ptr, parameterCount> parameters;
    std::array<yup::AudioParameterHandle, parameterCount> parameterHandles;
    digitalnoise::DigitalNoiseEngine engine;

    std::uint32_t currentSeed = 17041u;
    std::atomic<std::uint32_t> pendingSeed { 0u };
    float midiRateMultiplier = 1.0f;
    std::atomic<int> currentPreset { 0 };
    std::array<yup::String, 4> presetNames {
        "Packet Storm",
        "Address Fire",
        "Rule Collapse",
        "Byte Ruins"
    };
};
