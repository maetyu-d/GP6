#include "ErhcetuaPluginEditor.h"

namespace
{
const auto bgA = juce::Colour::fromRGB(10, 12, 16);
const auto bgB = juce::Colour::fromRGB(22, 28, 36);
const auto panel = juce::Colour::fromRGBA(18, 23, 30, 220);
const auto panelOutline = juce::Colour::fromRGBA(136, 164, 190, 32);
const auto controlPanel = juce::Colour::fromRGBA(14, 19, 25, 196);
const auto textBright = juce::Colour::fromRGB(236, 242, 247);
const auto textDim = juce::Colour::fromRGB(137, 153, 168);
const auto accent = juce::Colour::fromRGB(126, 242, 202);
const auto accentSoft = juce::Colour::fromRGB(108, 161, 255);
const auto danger = juce::Colour::fromRGB(255, 126, 140);

juce::Font uiFont(const juce::String& family, float size, int styleFlags)
{
    return juce::Font(juce::FontOptions(family, size, styleFlags));
}
}

ErhcetuaVisualizer::ErhcetuaVisualizer(ErhcetuaAudioProcessor& processorToUse)
    : processor(processorToUse)
{
    setInterceptsMouseClicks(false, false);
    startTimerHz(24);
}

void ErhcetuaVisualizer::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();

    g.setColour(panel);
    g.fillRoundedRectangle(area, 20.0f);
    g.setColour(panelOutline);
    g.drawRoundedRectangle(area.reduced(0.75f), 20.0f, 1.0f);

    auto content = area.reduced(16.0f);
    auto heading = content.removeFromTop(26.0f);
    auto stages = content.removeFromTop(52.0f);
    auto footer = content.removeFromBottom(40.0f);
    auto grid = content;

    g.setColour(textBright);
    g.setFont(uiFont("Avenir Next", 15.0f, juce::Font::bold));
    g.drawText(snapshot.grammarName, heading.removeFromLeft(heading.getWidth() * 0.3f).toNearestInt(), juce::Justification::centredLeft);

    g.setColour(textDim);
    g.setFont(uiFont("Menlo", 10.0f, juce::Font::plain));
    g.drawText(snapshot.algorithmSummary, heading.toNearestInt(), juce::Justification::centredRight);

    drawStageFlow(g, stages, snapshot);
    drawStepField(g, grid, snapshot);
    drawFooter(g, footer, snapshot);
}

void ErhcetuaVisualizer::timerCallback()
{
    snapshot = processor.getSnapshot();
    repaint();
}

void ErhcetuaVisualizer::drawStageFlow(juce::Graphics& g, juce::Rectangle<float> area, const ErhcetuaSnapshot& data)
{
    if (data.stages.empty())
        return;

    auto cards = area.reduced(0.0f, 2.0f);
    const auto cardGap = 6.0f;
    const auto cardWidth = (cards.getWidth() - cardGap * (float) (data.stages.size() - 1)) / (float) data.stages.size();

    for (auto index = 0; index < (int) data.stages.size(); ++index)
    {
        auto x = cards.getX() + (cardWidth + cardGap) * (float) index;
        auto card = juce::Rectangle<float>(x, cards.getY(), cardWidth, cards.getHeight());
        auto stageColour = index % 2 == 0 ? accent : accentSoft;

        g.setColour(juce::Colour::fromRGBA(255, 255, 255, 5));
        g.fillRoundedRectangle(card, 10.0f);

        auto header = card.reduced(8.0f);
        auto bar = header.removeFromBottom(4.0f);
        const auto fillWidth = juce::jmax(6.0f, bar.getWidth() * data.stages[(size_t) index].amount);

        g.setColour(textBright);
        g.setFont(uiFont("Avenir Next", 11.0f, juce::Font::bold));
        g.drawText(data.stages[(size_t) index].name, header.removeFromTop(14.0f).toNearestInt(), juce::Justification::centredLeft);

        g.setColour(textDim);
        g.setFont(uiFont("Menlo", 9.0f, juce::Font::plain));
        g.drawFittedText(data.stages[(size_t) index].detail, header.toNearestInt(), juce::Justification::topLeft, 1);

        g.setColour(juce::Colours::white.withAlpha(0.05f));
        g.fillRoundedRectangle(bar, 4.0f);
        g.setColour(stageColour);
        g.fillRoundedRectangle(bar.withWidth(fillWidth), 4.0f);

        if (index < (int) data.stages.size() - 1)
        {
            const auto lineY = card.getCentreY();
            const auto lineX = card.getRight() + cardGap * 0.5f;
            g.setColour(textDim.withAlpha(0.42f));
            g.drawLine(card.getRight() + 2.0f, lineY, lineX + 6.0f, lineY, 1.0f);
            juce::Path arrow;
            arrow.startNewSubPath(lineX + 6.0f, lineY);
            arrow.lineTo(lineX + 2.0f, lineY - 3.0f);
            arrow.lineTo(lineX + 2.0f, lineY + 3.0f);
            arrow.closeSubPath();
            g.fillPath(arrow);
        }
    }
}

void ErhcetuaVisualizer::drawStepField(juce::Graphics& g, juce::Rectangle<float> area, const ErhcetuaSnapshot& data)
{
    g.setColour(juce::Colour::fromRGBA(255, 255, 255, 8));
    g.fillRoundedRectangle(area, 16.0f);

    if (data.stepCount <= 0 || data.lanes.empty())
        return;

    auto content = area.reduced(12.0f);
    auto titleRow = content.removeFromTop(18.0f);
    auto modArea = content.removeFromBottom(72.0f);
    auto grid = content.reduced(0.0f, 2.0f);
    const auto laneGridOriginX = grid.getX() + 44.0f;
    const auto columnAreaTop = grid.getY();

    g.setColour(textBright);
    g.setFont(uiFont("Avenir Next", 12.0f, juce::Font::bold));
    g.drawText("Lanes", titleRow.removeFromLeft(titleRow.getWidth() * 0.32f).toNearestInt(), juce::Justification::centredLeft);
    g.setColour(textDim);
    g.setFont(uiFont("Menlo", 10.0f, juce::Font::plain));
    g.drawText(data.presetName + "  /  " + data.resetModeName, titleRow.toNearestInt(), juce::Justification::centredRight);

    const auto stepGap = 5.0f;
    const auto columnWidth = (grid.getWidth() - stepGap * (float) (data.stepCount - 1)) / (float) data.stepCount;
    const auto laneGap = 6.0f;
    const auto laneHeight = (grid.getHeight() - laneGap * (float) (data.lanes.size() - 1)) / (float) data.lanes.size();

    for (auto laneIndex = 0; laneIndex < (int) data.lanes.size(); ++laneIndex)
    {
        auto row = grid.removeFromTop(laneHeight);
        if (laneIndex < (int) data.lanes.size() - 1)
            grid.removeFromTop(laneGap);

        auto labelArea = row.removeFromLeft(44.0f);
        auto laneGrid = row;

        g.setColour(textDim);
        g.setFont(uiFont("Avenir Next", 10.0f, juce::Font::bold));
        g.drawText(data.lanes[(size_t) laneIndex].name, labelArea.toNearestInt(), juce::Justification::centredLeft);

        for (auto stepIndex = 0; stepIndex < data.stepCount; ++stepIndex)
        {
            const auto x = laneGrid.getX() + (columnWidth + stepGap) * (float) stepIndex;
            auto cell = juce::Rectangle<float>(x, laneGrid.getY(), columnWidth, laneGrid.getHeight());
            const auto& step = data.lanes[(size_t) laneIndex].steps[(size_t) stepIndex];

            g.setColour(juce::Colours::white.withAlpha(stepIndex == data.currentStep ? 0.06f : 0.025f));
            g.fillRoundedRectangle(cell, 3.0f);

            if (data.resetMarkers[(size_t) stepIndex])
            {
                g.setColour(accentSoft.withAlpha(0.45f));
                g.drawVerticalLine((int) std::round(cell.getX()), cell.getY(), cell.getBottom());
            }

            if (! step.active)
                continue;

            const auto height = juce::jmax(6.0f, cell.getHeight() * step.intensity);
            auto block = juce::Rectangle<float>(cell.getX(),
                                                cell.getBottom() - height,
                                                cell.getWidth(),
                                                height);
            auto colour = step.flutter ? accentSoft : (step.accent ? danger : accent);
            if (stepIndex == data.currentStep)
                colour = colour.brighter(0.15f);

            g.setColour(colour);
            g.fillRoundedRectangle(block, 3.0f);

            if (step.repeats > 1)
            {
                const auto markerHeight = 2.0f;
                const auto markerGap = 1.0f;
                const auto markerWidth = juce::jmin(3.0f, (cell.getWidth() - markerGap * (float) (step.repeats - 1)) / (float) step.repeats);
                for (auto repeat = 0; repeat < step.repeats; ++repeat)
                {
                    const auto markerX = cell.getX() + repeat * (markerWidth + markerGap);
                    g.fillRect(markerX, cell.getY() + 1.0f, markerWidth, markerHeight);
                }
            }
        }
    }

    auto labels = modArea.removeFromLeft(44.0f);
    auto modGrid = modArea;
    const auto modGap = 4.0f;
    const auto modHeight = (modGrid.getHeight() - modGap * (float) (data.modulationRows.size() - 1)) / (float) data.modulationRows.size();

    for (auto rowIndex = 0; rowIndex < (int) data.modulationRows.size(); ++rowIndex)
    {
        auto row = modGrid.removeFromTop(modHeight);
        if (rowIndex < (int) data.modulationRows.size() - 1)
            modGrid.removeFromTop(modGap);

        auto label = juce::Rectangle<float>(labels.getX(), row.getY(), labels.getWidth(), row.getHeight());
        g.setColour(textDim.withAlpha(0.8f));
        g.setFont(uiFont("Menlo", 9.0f, juce::Font::plain));
        g.drawText(data.modulationRows[(size_t) rowIndex].name, label.toNearestInt(), juce::Justification::centredLeft);

        for (auto stepIndex = 0; stepIndex < data.stepCount; ++stepIndex)
        {
            const auto x = row.getX() + (columnWidth + stepGap) * (float) stepIndex;
            auto cell = juce::Rectangle<float>(x, row.getY(), columnWidth, row.getHeight());
            const auto value = data.modulationRows[(size_t) rowIndex].values[(size_t) stepIndex];
            g.setColour(juce::Colours::white.withAlpha(0.04f));
            g.fillRoundedRectangle(cell, 2.0f);

            if (data.resetMarkers[(size_t) stepIndex])
            {
                g.setColour(accentSoft.withAlpha(0.25f));
                g.drawVerticalLine((int) std::round(cell.getX()), cell.getY(), cell.getBottom());
            }

            g.setColour(rowIndex == 3 ? accentSoft.withAlpha(0.85f) : accent.withAlpha(0.78f));
            g.fillRoundedRectangle(juce::Rectangle<float>(cell.getX(),
                                                          cell.getBottom() - cell.getHeight() * value,
                                                          cell.getWidth(),
                                                          juce::jmax(2.0f, cell.getHeight() * value)),
                                   2.0f);
        }
    }

    for (auto stepIndex = 0; stepIndex < data.stepCount; ++stepIndex)
    {
        const auto x = laneGridOriginX + (columnWidth + stepGap) * (float) stepIndex;
        auto label = juce::Rectangle<float>(x, columnAreaTop + area.getHeight() - 24.0f, columnWidth, 10.0f);
        g.setColour(textDim.withAlpha(0.7f));
        g.setFont(uiFont("Menlo", 8.5f, juce::Font::plain));
        g.drawText(juce::String(stepIndex + 1), label.toNearestInt(), juce::Justification::centred);
    }
}

void ErhcetuaVisualizer::drawFooter(juce::Graphics& g, juce::Rectangle<float> area, const ErhcetuaSnapshot& data)
{
    auto left = area.removeFromLeft(area.getWidth() * 0.66f);

    g.setColour(textDim);
    g.setFont(uiFont("Avenir Next", 11.0f, juce::Font::plain));
    g.drawText("Live grammar", left.removeFromTop(14.0f).toNearestInt(), juce::Justification::centredLeft);

    g.setColour(textBright);
    g.setFont(uiFont("Menlo", 10.0f, juce::Font::plain));
    g.drawText(data.resetModeName + " / " + data.scaleName + " / drift " + juce::String(data.drift, 2),
               left.toNearestInt(),
               juce::Justification::centredLeft);

    g.setColour(textDim);
    g.setFont(uiFont("Avenir Next", 11.0f, juce::Font::plain));
    g.drawText("Rule", area.removeFromTop(14.0f).toNearestInt(), juce::Justification::centredLeft);

    g.setColour(textBright);
    g.setFont(uiFont("Menlo", 10.0f, juce::Font::plain));
    g.drawText(data.ruleName, area.toNearestInt(), juce::Justification::centredLeft);
}

ErhcetuaAudioProcessorEditor::ErhcetuaAudioProcessorEditor(ErhcetuaAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), visualizer(p)
{
    setSize(1062, 684);

    configureCombo(presetBox);
    configureCombo(grammarBox);
    configureCombo(resetModeBox);
    configureCombo(scaleBox);
    configureCombo(ruleBox);
    presetBox.addItemList(ErhcetuaAudioProcessor::presetNames(), 1);
    grammarBox.addItemList(ErhcetuaRhythmEngine::grammars(), 1);
    resetModeBox.addItemList(ErhcetuaRhythmEngine::resetModes(), 1);
    scaleBox.addItemList(ErhcetuaRhythmEngine::scales(), 1);
    ruleBox.addItemList(ErhcetuaRhythmEngine::ruleSets(), 1);
    configureLabel(presetLabel, "Preset");
    configureLabel(grammarLabel, "Grammar");
    configureLabel(resetModeLabel, "Reset");
    configureLabel(scaleLabel, "Scale");
    configureLabel(ruleLabel, "Rules");

    configureSlider(densitySlider);
    configureSlider(mutationSlider);
    configureSlider(ratchetSlider);
    configureSlider(flutterSlider);
    configureSlider(rotationSlider);
    configureSlider(swingSlider);
    configureSlider(driftSlider);
    configureSlider(gateSkewSlider);
    configureSlider(pitchSpreadSlider);
    configureSlider(probabilitySlider);
    configureSlider(rootNoteSlider);
    configureSlider(octaveSpanSlider);
    configureSlider(channelSlider);
    configureSlider(gateSlider);

    configureLabel(densityLabel, "Density");
    configureLabel(mutationLabel, "Mutation");
    configureLabel(ratchetLabel, "Ratchet");
    configureLabel(flutterLabel, "Flutter Reset");
    configureLabel(rotationLabel, "Rotation");
    configureLabel(swingLabel, "Swing");
    configureLabel(driftLabel, "Drift");
    configureLabel(gateSkewLabel, "Gate Skew");
    configureLabel(pitchSpreadLabel, "Pitch Spread");
    configureLabel(probabilityLabel, "Probability");
    configureLabel(rootNoteLabel, "Root Note");
    configureLabel(octaveSpanLabel, "Octave Span");
    configureLabel(channelLabel, "Channel");
    configureLabel(gateLabel, "Gate");

    addAndMakeVisible(visualizer);

    presetAttachment = std::make_unique<ComboAttachment>(audioProcessor.apvts, "preset", presetBox);
    grammarAttachment = std::make_unique<ComboAttachment>(audioProcessor.apvts, "grammar", grammarBox);
    resetModeAttachment = std::make_unique<ComboAttachment>(audioProcessor.apvts, "resetMode", resetModeBox);
    scaleAttachment = std::make_unique<ComboAttachment>(audioProcessor.apvts, "scale", scaleBox);
    ruleAttachment = std::make_unique<ComboAttachment>(audioProcessor.apvts, "ruleSet", ruleBox);
    densityAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "density", densitySlider);
    mutationAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "mutation", mutationSlider);
    ratchetAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "ratchet", ratchetSlider);
    flutterAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "flutter", flutterSlider);
    rotationAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "rotation", rotationSlider);
    swingAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "swing", swingSlider);
    driftAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "drift", driftSlider);
    gateSkewAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "gateSkew", gateSkewSlider);
    pitchSpreadAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "pitchSpread", pitchSpreadSlider);
    probabilityAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "probability", probabilitySlider);
    rootNoteAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "rootNote", rootNoteSlider);
    octaveSpanAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "octaveSpan", octaveSpanSlider);
    channelAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "channel", channelSlider);
    gateAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "gate", gateSlider);
}

ErhcetuaAudioProcessorEditor::~ErhcetuaAudioProcessorEditor() = default;

void ErhcetuaAudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient gradient(bgA, 0.0f, 0.0f, bgB, (float) getWidth(), (float) getHeight(), false);
    gradient.addColour(0.42, juce::Colour::fromRGBA(17, 31, 46, 255));
    g.setGradientFill(gradient);
    g.fillAll();

    auto bounds = getLocalBounds().toFloat().reduced(16.0f);
    auto left = bounds.removeFromLeft(304.0f);
    auto topRail = bounds.removeFromTop(92.0f);

    g.setColour(controlPanel);
    g.fillRoundedRectangle(left, 18.0f);
    g.fillRoundedRectangle(topRail, 18.0f);
    g.setColour(panelOutline);
    g.drawRoundedRectangle(left.reduced(0.5f), 18.0f, 1.0f);
    g.drawRoundedRectangle(topRail.reduced(0.5f), 18.0f, 1.0f);
}

void ErhcetuaAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(16);
    auto controls = area.removeFromLeft(304);
    auto topRail = area.removeFromTop(92);
    visualizer.setBounds(area.reduced(4, 0));

    auto placeTopCombo = [](juce::Rectangle<int>& row, juce::Label& label, juce::ComboBox& box)
    {
        auto col = row.removeFromLeft(row.getWidth() / 5).reduced(6, 8);
        label.setBounds(col.removeFromTop(12));
        box.setBounds(col.removeFromTop(22));
    };

    placeTopCombo(topRail, presetLabel, presetBox);
    placeTopCombo(topRail, grammarLabel, grammarBox);
    placeTopCombo(topRail, resetModeLabel, resetModeBox);
    placeTopCombo(topRail, scaleLabel, scaleBox);
    placeTopCombo(topRail, ruleLabel, ruleBox);

    auto placeControl = [](juce::Rectangle<int>& source, juce::Label& label, juce::Slider& slider)
    {
        auto row = source.removeFromTop(38).reduced(8, 2);
        label.setBounds(row.removeFromTop(12));
        slider.setBounds(row);
    };

    auto macroGroup = controls.removeFromTop(274);
    auto utilityGroup = controls;

    placeControl(macroGroup, densityLabel, densitySlider);
    placeControl(macroGroup, mutationLabel, mutationSlider);
    placeControl(macroGroup, ratchetLabel, ratchetSlider);
    placeControl(macroGroup, flutterLabel, flutterSlider);
    placeControl(macroGroup, swingLabel, swingSlider);
    placeControl(macroGroup, driftLabel, driftSlider);
    placeControl(macroGroup, probabilityLabel, probabilitySlider);

    placeControl(utilityGroup, rotationLabel, rotationSlider);
    placeControl(utilityGroup, gateSkewLabel, gateSkewSlider);
    placeControl(utilityGroup, pitchSpreadLabel, pitchSpreadSlider);
    placeControl(utilityGroup, rootNoteLabel, rootNoteSlider);
    placeControl(utilityGroup, octaveSpanLabel, octaveSpanSlider);
    placeControl(utilityGroup, channelLabel, channelSlider);
    placeControl(utilityGroup, gateLabel, gateSlider);
}

void ErhcetuaAudioProcessorEditor::configureSlider(juce::Slider& slider)
{
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 18);
    slider.setColour(juce::Slider::trackColourId, accent);
    slider.setColour(juce::Slider::thumbColourId, textBright);
    slider.setColour(juce::Slider::backgroundColourId, juce::Colour::fromRGBA(255, 255, 255, 12));
    slider.setColour(juce::Slider::textBoxTextColourId, textBright);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour::fromRGBA(255, 255, 255, 10));
    slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
    addAndMakeVisible(slider);
}

void ErhcetuaAudioProcessorEditor::configureLabel(juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setColour(juce::Label::textColourId, textDim);
    label.setFont(uiFont("Avenir Next", 11.0f, juce::Font::plain));
    addAndMakeVisible(label);
}

void ErhcetuaAudioProcessorEditor::configureCombo(juce::ComboBox& box)
{
    box.setColour(juce::ComboBox::backgroundColourId, juce::Colour::fromRGBA(255, 255, 255, 12));
    box.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    box.setColour(juce::ComboBox::textColourId, textBright);
    box.setColour(juce::ComboBox::arrowColourId, accent);
    addAndMakeVisible(box);
}
