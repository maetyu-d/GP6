#include "PluginEditor.h"

namespace
{
const auto backgroundTop = juce::Colour::fromRGB(24, 18, 13);
const auto backgroundBottom = juce::Colour::fromRGB(83, 44, 24);
const auto panelColour = juce::Colour::fromRGBA(15, 12, 10, 190);
const auto copper = juce::Colour::fromRGB(205, 133, 63);
const auto sand = juce::Colour::fromRGB(244, 222, 179);
const auto ember = juce::Colour::fromRGB(236, 92, 45);
const auto clay = juce::Colour::fromRGB(137, 76, 48);

juce::Font uiFont(const juce::String& family, float size, int styleFlags)
{
    return juce::Font(juce::FontOptions(family, size, styleFlags));
}

juce::Colour laneColour(int laneIndex)
{
    static const std::array<juce::Colour, 4> colours {
        copper,
        juce::Colour::fromRGB(224, 172, 87),
        ember,
        juce::Colour::fromRGB(242, 214, 122)
    };

    return colours[(size_t) juce::jlimit(0, (int) colours.size() - 1, laneIndex)];
}
}

RhythmVisualizer::RhythmVisualizer(AfricanRhythmsAudioProcessor& processorToUse)
    : processor(processorToUse)
{
    setInterceptsMouseClicks(false, false);
    startTimerHz(24);
}

void RhythmVisualizer::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced(8.0f);
    g.setColour(panelColour);
    g.fillRoundedRectangle(area, 22.0f);

    auto wheelArea = area.removeFromTop(area.getHeight() * 0.56f).reduced(8.0f, 8.0f);
    auto timelineArea = area.reduced(8.0f, 4.0f);

    drawWheel(g, wheelArea, snapshot);
    drawTimeline(g, timelineArea, snapshot);
}

void RhythmVisualizer::timerCallback()
{
    snapshot = processor.getSnapshot();
    repaint();
}

void RhythmVisualizer::drawWheel(juce::Graphics& g, juce::Rectangle<float> area, const RhythmSnapshot& data)
{
    const auto radius = juce::jmin(area.getWidth(), area.getHeight()) * 0.42f;
    const auto centre = area.getCentre();
    const auto outer = radius;
    const auto inner = radius * 0.5f;

    g.setColour(clay.withAlpha(0.7f));
    g.fillEllipse(centre.x - outer, centre.y - outer, outer * 2.0f, outer * 2.0f);

    g.setColour(juce::Colours::black.withAlpha(0.28f));
    g.fillEllipse(centre.x - inner, centre.y - inner, inner * 2.0f, inner * 2.0f);

    if (data.stepCount <= 0)
        return;

    for (auto laneIndex = 0; laneIndex < (int) data.lanes.size(); ++laneIndex)
    {
        const auto laneRadius = radius * (0.56f + 0.11f * (float) laneIndex);
        const auto& lane = data.lanes[(size_t) laneIndex];

        for (auto step = 0; step < data.stepCount; ++step)
        {
            const auto angle = juce::MathConstants<float>::twoPi * static_cast<float>(step) / static_cast<float>(data.stepCount)
                             - juce::MathConstants<float>::halfPi;
            const auto stepPos = juce::Point<float>(centre.x + std::cos(angle) * laneRadius,
                                                    centre.y + std::sin(angle) * laneRadius);
            const auto isHit = lane.hits[(size_t) step];
            const auto isAccent = lane.accents[(size_t) step];
            const auto isCurrent = step == data.currentStep;
            const auto size = isAccent ? 15.0f : 9.0f;
            const auto baseColour = laneColour(laneIndex);

            g.setColour(isHit ? (isCurrent ? ember.brighter(0.1f) : baseColour) : sand.withAlpha(0.08f));
            g.fillEllipse(stepPos.x - size * 0.5f, stepPos.y - size * 0.5f, size, size);

            if (isHit)
            {
                g.setColour(baseColour.withAlpha(isCurrent ? 0.72f : 0.36f));
                g.drawLine(centre.x, centre.y, stepPos.x, stepPos.y, isCurrent ? 2.1f : 0.9f);
            }
        }
    }

    g.setColour(sand);
    g.setFont(uiFont("Avenir Next Condensed", 22.0f, juce::Font::bold));
    g.drawFittedText(data.familyName, area.toNearestInt().reduced(24), juce::Justification::centred, 2);

    g.setColour(sand.withAlpha(0.72f));
    g.setFont(uiFont("Avenir Next", 13.0f, juce::Font::plain));
    g.drawFittedText(data.presetName, area.toNearestInt().reduced(40, 52), juce::Justification::centredBottom, 1);
}

void RhythmVisualizer::drawTimeline(juce::Graphics& g, juce::Rectangle<float> area, const RhythmSnapshot& data)
{
    g.setColour(juce::Colours::black.withAlpha(0.24f));
    g.fillRoundedRectangle(area, 16.0f);

    if (data.stepCount <= 0 || data.lanes.empty())
        return;

    auto content = area.reduced(12.0f, 10.0f);
    const auto labelWidth = 86.0f;
    auto laneArea = content;

    for (auto laneIndex = 0; laneIndex < (int) data.lanes.size(); ++laneIndex)
    {
        const auto remainingLanes = (float) ((int) data.lanes.size() - laneIndex);
        auto row = laneArea.removeFromTop(laneArea.getHeight() / remainingLanes);
        row = row.reduced(0.0f, 4.0f);
        auto labelArea = row.removeFromLeft(labelWidth);
        auto gridArea = row.reduced(6.0f, 0.0f);
        const auto& lane = data.lanes[(size_t) laneIndex];
        const auto gap = 5.0f;
        const auto width = (gridArea.getWidth() - gap * (data.stepCount - 1)) / (float) data.stepCount;

        g.setColour(laneColour(laneIndex));
        g.setFont(uiFont("Avenir Next", 13.0f, juce::Font::bold));
        g.drawFittedText(lane.name, labelArea.removeFromTop(18.0f).toNearestInt(), juce::Justification::centredLeft, 1);
        g.setColour(sand.withAlpha(0.68f));
        g.setFont(uiFont("Avenir Next", 12.0f, juce::Font::plain));
        g.drawText("Note " + juce::String(lane.midiNote), labelArea.toNearestInt(), juce::Justification::centredLeft);

        for (auto step = 0; step < data.stepCount; ++step)
        {
            const auto x = gridArea.getX() + step * (width + gap);
            auto block = juce::Rectangle<float>(x, gridArea.getY() + 4.0f, width, gridArea.getHeight() - 8.0f);
            const auto intensity = lane.intensities[(size_t) step];
            const auto activeHeight = juce::jmap(intensity, 0.0f, 1.0f, block.getHeight() * 0.14f, block.getHeight());
            block.setY(block.getBottom() - activeHeight);
            block.setHeight(activeHeight);

            const auto isCurrent = step == data.currentStep;
            const auto colour = lane.hits[(size_t) step] ? (isCurrent ? ember : laneColour(laneIndex)) : sand.withAlpha(0.06f);

            g.setColour(colour);
            g.fillRoundedRectangle(block, width * 0.28f);

            if (lane.accents[(size_t) step])
            {
                g.setColour(sand.withAlpha(0.9f));
                g.fillEllipse(x + width * 0.3f, gridArea.getY() - 1.0f, width * 0.4f, width * 0.4f);
            }
        }
    }
}

AfricanRhythmsAudioProcessorEditor::AfricanRhythmsAudioProcessorEditor(AfricanRhythmsAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), visualizer(p)
{
    setSize(1080, 720);
    presetBox.addItemList(AfricanRhythmsAudioProcessor::presetNames(), 1);
    addAndMakeVisible(presetBox);
    configureLabel(presetLabel, "Preset Bank");

    familyBox.addItemList(RhythmEngine::families(), 1);
    addAndMakeVisible(familyBox);
    configureLabel(familyLabel, "Family");

    configureSlider(densitySlider, "");
    configureSlider(complexitySlider, "");
    configureSlider(rotationSlider, "");
    configureSlider(swingSlider, "");
    configureSlider(probabilitySlider, "");
    configureNoteSlider(bellNoteSlider);
    configureNoteSlider(supportNoteSlider);
    configureNoteSlider(bassNoteSlider);
    configureNoteSlider(shakerNoteSlider);
    configureSlider(channelSlider, "");
    configureSlider(gateSlider, "");

    configureLabel(densityLabel, "Density");
    configureLabel(complexityLabel, "Complexity");
    configureLabel(rotationLabel, "Rotation");
    configureLabel(swingLabel, "Swing");
    configureLabel(probabilityLabel, "Probability");
    configureLabel(bellNoteLabel, "Bell Note");
    configureLabel(supportNoteLabel, "Support Note");
    configureLabel(bassNoteLabel, "Bass Note");
    configureLabel(shakerNoteLabel, "Shaker Note");
    configureLabel(channelLabel, "Channel");
    configureLabel(gateLabel, "Gate");

    addAndMakeVisible(visualizer);

    presetAttachment = std::make_unique<ComboAttachment>(audioProcessor.apvts, "preset", presetBox);
    familyAttachment = std::make_unique<ComboAttachment>(audioProcessor.apvts, "family", familyBox);
    densityAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "density", densitySlider);
    complexityAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "complexity", complexitySlider);
    rotationAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "rotation", rotationSlider);
    swingAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "swing", swingSlider);
    probabilityAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "probability", probabilitySlider);
    bellNoteAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "bellNote", bellNoteSlider);
    supportNoteAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "supportNote", supportNoteSlider);
    bassNoteAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "bassNote", bassNoteSlider);
    shakerNoteAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "shakerNote", shakerNoteSlider);
    channelAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "channel", channelSlider);
    gateAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "gate", gateSlider);
}

AfricanRhythmsAudioProcessorEditor::~AfricanRhythmsAudioProcessorEditor() = default;

void AfricanRhythmsAudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient gradient(backgroundTop, 0.0f, 0.0f, backgroundBottom, 0.0f, (float) getHeight(), false);
    gradient.addColour(0.4, clay.withAlpha(0.9f));
    g.setGradientFill(gradient);
    g.fillAll();

    g.setColour(sand.withAlpha(0.06f));
    for (auto i = 0; i < getWidth(); i += 28)
        g.drawVerticalLine(i, 0.0f, (float) getHeight());

    auto glowBounds = getLocalBounds().toFloat().reduced(32.0f).removeFromTop(200.0f);
    g.setColour(ember.withAlpha(0.08f));
    g.fillEllipse(glowBounds.withTrimmedLeft(getWidth() * 0.35f));
}

void AfricanRhythmsAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(20);
    auto body = area;
    auto controls = body.removeFromLeft(336);
    visualizer.setBounds(body.reduced(6));

    auto presetArea = controls.removeFromTop(58);
    presetLabel.setBounds(presetArea.removeFromTop(18));
    presetBox.setBounds(presetArea.removeFromTop(30));

    auto familyArea = controls.removeFromTop(58);
    familyLabel.setBounds(familyArea.removeFromTop(18));
    familyBox.setBounds(familyArea.removeFromTop(30));

    auto placeSlider = [](juce::Rectangle<int>& source, juce::Label& label, juce::Slider& slider)
    {
        auto row = source.removeFromTop(45);
        label.setBounds(row.removeFromTop(14));
        slider.setBounds(row.removeFromTop(30));
    };

    auto placeNoteSlider = [](juce::Rectangle<int>& source, juce::Label& label, juce::Slider& slider)
    {
        auto row = source.removeFromTop(58);
        label.setBounds(row.removeFromTop(14));
        slider.setBounds(row.removeFromTop(40));
    };

    placeSlider(controls, densityLabel, densitySlider);
    placeSlider(controls, complexityLabel, complexitySlider);
    placeSlider(controls, rotationLabel, rotationSlider);
    placeSlider(controls, swingLabel, swingSlider);
    placeSlider(controls, probabilityLabel, probabilitySlider);
    placeNoteSlider(controls, bellNoteLabel, bellNoteSlider);
    placeNoteSlider(controls, supportNoteLabel, supportNoteSlider);
    placeNoteSlider(controls, bassNoteLabel, bassNoteSlider);
    placeNoteSlider(controls, shakerNoteLabel, shakerNoteSlider);
    placeSlider(controls, channelLabel, channelSlider);
    placeSlider(controls, gateLabel, gateSlider);
}

void AfricanRhythmsAudioProcessorEditor::configureSlider(juce::Slider& slider, const juce::String&)
{
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    slider.setColour(juce::Slider::trackColourId, copper);
    slider.setColour(juce::Slider::thumbColourId, sand);
    slider.setColour(juce::Slider::backgroundColourId, clay.withAlpha(0.45f));
    slider.setColour(juce::Slider::textBoxTextColourId, sand);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::black.withAlpha(0.18f));
    addAndMakeVisible(slider);
}

void AfricanRhythmsAudioProcessorEditor::configureNoteSlider(juce::Slider& slider)
{
    slider.setSliderStyle(juce::Slider::IncDecButtons);
    slider.setIncDecButtonsMode(juce::Slider::incDecButtonsDraggable_AutoDirection);
    slider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 114, 38);
    slider.setColour(juce::Slider::trackColourId, copper);
    slider.setColour(juce::Slider::thumbColourId, sand);
    slider.setColour(juce::Slider::backgroundColourId, clay.withAlpha(0.3f));
    slider.setColour(juce::Slider::textBoxTextColourId, sand);
    slider.setColour(juce::Slider::textBoxOutlineColourId, copper.withAlpha(0.35f));
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::black.withAlpha(0.28f));
    slider.setNumDecimalPlacesToDisplay(0);
    addAndMakeVisible(slider);
}

void AfricanRhythmsAudioProcessorEditor::configureLabel(juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setColour(juce::Label::textColourId, sand.withAlpha(0.78f));
    label.setFont(uiFont("Avenir Next", 13.0f, juce::Font::plain));
    addAndMakeVisible(label);
}
