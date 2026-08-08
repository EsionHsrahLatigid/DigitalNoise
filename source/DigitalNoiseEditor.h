#pragma once

#include <yup_audio_processors/yup_audio_processors.h>
#include <yup_gui/yup_gui.h>

#include <memory>
#include <vector>

class DigitalNoisePlugin;

class DigitalNoiseEditor final
    : public yup::AudioProcessorEditor
    , private yup::Timer
{
public:
    explicit DigitalNoiseEditor (DigitalNoisePlugin& processor);

    bool isResizable() const override;
    bool shouldPreserveAspectRatio() const override;
    yup::Size<int> getPreferredSize() const override;
    void paint (yup::Graphics& graphics) override;
    void resized() override;

private:
    void timerCallback() override;

    std::vector<yup::AudioParameter::Ptr> parameters;
    std::vector<std::unique_ptr<yup::Label>> labels;
    std::vector<std::unique_ptr<yup::Slider>> sliders;
    std::vector<std::unique_ptr<yup::Label>> valueLabels;
};
