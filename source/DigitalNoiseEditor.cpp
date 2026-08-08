#include "DigitalNoiseEditor.h"

#include "DigitalNoisePlugin.h"

#include <algorithm>

DigitalNoiseEditor::DigitalNoiseEditor (DigitalNoisePlugin& processor)
{
    const auto processorParameters = processor.getParameters();
    parameters.assign (processorParameters.begin(), processorParameters.end());

    labels.reserve (parameters.size());
    sliders.reserve (parameters.size());
    valueLabels.reserve (parameters.size());

    for (const auto& parameter : parameters)
    {
        auto label = std::make_unique<yup::Label>();
        label->setText (parameter->getName(), yup::dontSendNotification);
        label->setJustification (yup::Justification::center);
        addAndMakeVisible (*label);
        labels.push_back (std::move (label));

        auto slider = std::make_unique<yup::Slider> (yup::Slider::RotaryVerticalDrag);
        slider->setRange (parameter->getMinimumValue(),
                          parameter->getMaximumValue(),
                          parameter->isStepped() ? 1.0 : 0.0);
        slider->setDefaultValue (parameter->getDefaultValue());
        slider->setValue (parameter->getValue(), yup::dontSendNotification);
        slider->setTextBoxStyle (yup::Slider::NoTextBox);
        slider->setPopupDisplayEnabled (false);
        slider->setMouseCursor (yup::MouseCursor::Hand);
        slider->onDragStart = [parameter] (const yup::MouseEvent&)
        {
            parameter->beginChangeGesture();
        };
        slider->onValueChanged = [parameter] (double value)
        {
            parameter->setValueNotifyingHost (static_cast<float> (value));
        };
        slider->onDragEnd = [parameter] (const yup::MouseEvent&)
        {
            parameter->endChangeGesture();
        };
        addAndMakeVisible (*slider);
        sliders.push_back (std::move (slider));

        auto valueLabel = std::make_unique<yup::Label>();
        valueLabel->setText (parameter->toString(), yup::dontSendNotification);
        valueLabel->setJustification (yup::Justification::center);
        addAndMakeVisible (*valueLabel);
        valueLabels.push_back (std::move (valueLabel));
    }

    setSize (getPreferredSize().to<float>());
    startTimerHz (30);
}

bool DigitalNoiseEditor::isResizable() const
{
    return true;
}

bool DigitalNoiseEditor::shouldPreserveAspectRatio() const
{
    return true;
}

yup::Size<int> DigitalNoiseEditor::getPreferredSize() const
{
    return { 1100, 410 };
}

void DigitalNoiseEditor::paint (yup::Graphics& graphics)
{
    graphics.setFillColor (0xff101216);
    graphics.fillAll();
}

void DigitalNoiseEditor::resized()
{
    constexpr int columns = 6;
    constexpr float margin = 20.0f;
    constexpr float gap = 12.0f;
    constexpr float labelHeight = 24.0f;
    constexpr float valueHeight = 24.0f;
    constexpr float controlGap = 4.0f;

    const auto bounds = getLocalBounds();
    const auto cellWidth = (bounds.getWidth() - (2.0f * margin) - (gap * (columns - 1))) / columns;
    const auto rows = std::max (1, static_cast<int> ((sliders.size() + columns - 1) / columns));
    const auto cellHeight = (bounds.getHeight() - (2.0f * margin) - (gap * (rows - 1))) / rows;

    for (std::size_t i = 0; i < sliders.size(); ++i)
    {
        const auto column = static_cast<int> (i) % columns;
        const auto row = static_cast<int> (i) / columns;
        const auto x = margin + column * (cellWidth + gap);
        const auto y = margin + row * (cellHeight + gap);
        const auto controlHeight = cellHeight - labelHeight - valueHeight - (2.0f * controlGap);
        const auto controlSize = std::min (cellWidth - 8.0f, controlHeight);
        const auto controlX = x + ((cellWidth - controlSize) * 0.5f);
        const auto controlY = y + labelHeight + controlGap;

        labels[i]->setBounds (x, y, cellWidth, labelHeight);
        sliders[i]->setBounds (controlX, controlY, controlSize, controlSize);
        valueLabels[i]->setBounds (x,
                                   y + cellHeight - valueHeight,
                                   cellWidth,
                                   valueHeight);
    }
}

void DigitalNoiseEditor::timerCallback()
{
    for (std::size_t i = 0; i < sliders.size(); ++i)
    {
        if (! sliders[i]->isCurrentlyBeingDragged())
            sliders[i]->setValue (parameters[i]->getValue(), yup::dontSendNotification);

        valueLabels[i]->setText (parameters[i]->toString(), yup::dontSendNotification);
    }
}
