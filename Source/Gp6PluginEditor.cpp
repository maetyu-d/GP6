#include "Gp6PluginEditor.h"

namespace
{
const auto bgTop = juce::Colour::fromRGB(33, 30, 24);
const auto bgBottom = juce::Colour::fromRGB(92, 82, 66);
const auto shell = juce::Colour::fromRGBA(39, 35, 28, 228);
const auto shellOutline = juce::Colour::fromRGBA(255, 236, 186, 34);
const auto moduleFace = juce::Colour::fromRGBA(25, 23, 19, 208);
const auto textBright = juce::Colour::fromRGB(243, 229, 198);
const auto textDim = juce::Colour::fromRGB(181, 161, 130);
const auto brass = juce::Colour::fromRGB(199, 149, 78);
const auto warning = juce::Colour::fromRGB(255, 98, 80);
const auto trace = juce::Colour::fromRGB(255, 214, 111);
const auto mint = juce::Colour::fromRGB(107, 222, 191);
const auto steel = juce::Colour::fromRGB(152, 177, 214);

class CableFaderLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawLinearSlider(juce::Graphics& g,
                          int x,
                          int y,
                          int width,
                          int height,
                          float sliderPos,
                          float minSliderPos,
                          float maxSliderPos,
                          const juce::Slider::SliderStyle style,
                          juce::Slider& slider) override
    {
        juce::ignoreUnused(minSliderPos, maxSliderPos);

        auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(10.0f, 4.0f);
        if (style != juce::Slider::LinearVertical)
            return LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);

        const auto track = juce::Rectangle<float>(bounds.getCentreX() - 2.0f, bounds.getY(), 4.0f, bounds.getHeight());
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillRoundedRectangle(track, 2.0f);

        auto activeTrack = juce::Rectangle<float>(track.getX(), sliderPos, track.getWidth(), track.getBottom() - sliderPos);
        g.setColour(slider.findColour(juce::Slider::trackColourId).withAlpha(0.95f));
        g.fillRoundedRectangle(activeTrack, 2.0f);

        g.setColour(slider.findColour(juce::Slider::thumbColourId));
        g.fillEllipse(bounds.getCentreX() - 7.0f, sliderPos - 7.0f, 14.0f, 14.0f);
        g.setColour(textBright.withAlpha(0.22f));
        g.drawEllipse(bounds.getCentreX() - 7.0f, sliderPos - 7.0f, 14.0f, 14.0f, 1.0f);
    }
};

CableFaderLookAndFeel cableFaderLookAndFeel;

juce::Font uiFont(const juce::String& family, float size, int styleFlags)
{
    return juce::Font(juce::FontOptions(family, size, styleFlags));
}

juce::Point<float> patchCurvePoint(juce::Point<float> start, juce::Point<float> end, float t)
{
    const auto bendY = juce::jmin(start.y, end.y) + std::abs(end.y - start.y) * 0.42f;
    const auto oneMinusT = 1.0f - t;
    return {
        oneMinusT * oneMinusT * oneMinusT * start.x
            + 3.0f * oneMinusT * oneMinusT * t * start.x
            + 3.0f * oneMinusT * t * t * end.x
            + t * t * t * end.x,
        oneMinusT * oneMinusT * oneMinusT * start.y
            + 3.0f * oneMinusT * oneMinusT * t * bendY
            + 3.0f * oneMinusT * t * t * bendY
            + t * t * t * end.y
    };
}

juce::Point<float> patchCurveTangent(juce::Point<float> start, juce::Point<float> end, float t)
{
    const auto bendY = juce::jmin(start.y, end.y) + std::abs(end.y - start.y) * 0.42f;
    const auto oneMinusT = 1.0f - t;
    return {
        6.0f * oneMinusT * t * (end.x - start.x),
        3.0f * oneMinusT * oneMinusT * (bendY - start.y) + 3.0f * t * t * (end.y - bendY)
    };
}

}

Gp6PanelVisualizer::Gp6PanelVisualizer(Gp6AudioProcessor& processorToUse)
    : processor(processorToUse)
{
    setInterceptsMouseClicks(false, false);
    startTimerHz(24);
}

void Gp6PanelVisualizer::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour(shell);
    g.fillRoundedRectangle(area, 22.0f);
    g.setColour(shellOutline);
    g.drawRoundedRectangle(area.reduced(0.75f), 22.0f, 1.0f);

    auto content = area.reduced(14.0f);
    auto header = content.removeFromTop(56.0f);
    auto footer = content.removeFromBottom(82.0f);
    auto patchbay = content.removeFromTop(content.getHeight() * 0.33f);
    auto lanes = content;

    drawHeader(g, header, snapshot);
    drawPatchbay(g, patchbay, snapshot);
    drawIntegratorLanes(g, lanes, snapshot);
    drawStatus(g, footer, snapshot);
}

void Gp6PanelVisualizer::timerCallback()
{
    snapshot = processor.getSnapshot();
    repaint();
}

void Gp6PanelVisualizer::drawHeader(juce::Graphics& g, juce::Rectangle<float> area, const Gp6Snapshot& data)
{
    auto left = area.removeFromLeft(area.getWidth() * 0.44f);
    g.setColour(textBright);
    g.setFont(uiFont("Avenir Next Condensed", 24.0f, juce::Font::bold));
    g.drawText("COMDYNA GP-6", left.removeFromTop(28.0f).toNearestInt(), juce::Justification::centredLeft);
    g.setColour(textDim);
    g.setFont(uiFont("Menlo", 11.0f, juce::Font::plain));
    g.drawText(data.summary, left.toNearestInt(), juce::Justification::centredLeft);

    auto right = area.reduced(0.0f, 6.0f);
    g.setColour(textDim);
    g.setFont(uiFont("Avenir Next", 11.0f, juce::Font::plain));
    g.drawText("Patch", right.removeFromTop(16.0f).toNearestInt(), juce::Justification::centredRight);
    g.setColour(textBright);
    g.setFont(uiFont("Avenir Next", 13.0f, juce::Font::bold));
    g.drawText(data.patchName + "  /  " + data.modeName + "  /  " + data.repeaterName,
               right.toNearestInt(),
               juce::Justification::centredRight);
}

void Gp6PanelVisualizer::drawPatchbay(juce::Graphics& g, juce::Rectangle<float> area, const Gp6Snapshot& data)
{
    g.setColour(moduleFace);
    g.fillRoundedRectangle(area, 16.0f);

    auto content = area.reduced(12.0f);
    auto title = content.removeFromTop(18.0f);
    g.setColour(textBright);
    g.setFont(uiFont("Avenir Next", 12.0f, juce::Font::bold));
    g.drawText("Patched matrix", title.removeFromLeft(140.0f).toNearestInt(), juce::Justification::centredLeft);
    g.setColour(textDim);
    g.setFont(uiFont("Menlo", 10.0f, juce::Font::plain));
    g.drawText("live summary of the patch tab", title.toNearestInt(), juce::Justification::centredRight);

    const std::array<juce::String, 6> columns { "A", "B", "SUM", "C", "A x B", "CMP" };
    const std::array<juce::String, 4> rows { "Int A", "Int B", "Comp C", "Mult D" };
    const auto rowHeight = content.getHeight() / (float) rows.size();
    const auto leftLabelWidth = 64.0f;
    const auto gridWidth = content.getWidth() - leftLabelWidth;
    const auto cellWidth = gridWidth / (float) columns.size();

    for (auto column = 0; column < (int) columns.size(); ++column)
    {
        auto cell = juce::Rectangle<float>(content.getX() + leftLabelWidth + cellWidth * (float) column,
                                           content.getY(),
                                           cellWidth,
                                           18.0f);
        g.setColour(textDim.withAlpha(0.9f));
        g.setFont(uiFont("Menlo", 9.0f, juce::Font::plain));
        g.drawText(columns[(size_t) column], cell.toNearestInt(), juce::Justification::centred);
    }

    for (auto row = 0; row < (int) rows.size(); ++row)
    {
        const auto y = content.getY() + rowHeight * (float) row + 18.0f;
        auto label = juce::Rectangle<float>(content.getX(), y, leftLabelWidth, rowHeight - 4.0f);
        g.setColour(textDim);
        g.setFont(uiFont("Avenir Next", 10.0f, juce::Font::bold));
        g.drawText(rows[(size_t) row], label.toNearestInt(), juce::Justification::centredLeft);
    }

    for (const auto& point : data.patchPoints)
    {
        int columnIndex = (int) columns.size() - 1;
        for (auto i = 0; i < (int) columns.size(); ++i)
            if (columns[(size_t) i] == point.from || columns[(size_t) i] == point.to)
                columnIndex = i;

        auto rowIndex = 0;
        if (point.to.containsIgnoreCase("B"))
            rowIndex = 1;
        else if (point.to.containsIgnoreCase("CMP") || point.to.containsIgnoreCase("C"))
            rowIndex = 2;
        else if (point.to.containsIgnoreCase("D"))
            rowIndex = 3;

        auto cell = juce::Rectangle<float>(content.getX() + leftLabelWidth + cellWidth * (float) columnIndex,
                                           content.getY() + rowHeight * (float) rowIndex + 24.0f,
                                           cellWidth - 8.0f,
                                           rowHeight - 18.0f);

        g.setColour(brass.withAlpha(0.12f + point.amount * 0.76f));
        g.fillRoundedRectangle(cell, 6.0f);
        g.setColour(textBright.withAlpha(0.56f + point.amount * 0.32f));
        g.setFont(uiFont("Menlo", 8.0f, juce::Font::plain));
        g.drawText(point.from + ">" + point.to + " " + juce::String(point.amount, 2),
                   cell.toNearestInt(),
                   juce::Justification::centred);
    }
}

void Gp6PanelVisualizer::drawIntegratorLanes(juce::Graphics& g, juce::Rectangle<float> area, const Gp6Snapshot& data)
{
    g.setColour(moduleFace);
    g.fillRoundedRectangle(area, 16.0f);

    if (data.stepCount <= 0 || data.lanes.empty())
        return;

    auto content = area.reduced(10.0f);
    auto traceArea = content.removeFromTop(78.0f);
    auto lanesArea = content;

    g.setColour(textDim);
    g.setFont(uiFont("Avenir Next", 11.0f, juce::Font::plain));
    g.drawText("Reference trace", traceArea.removeFromTop(14.0f).toNearestInt(), juce::Justification::centredLeft);

    auto graph = traceArea.reduced(0.0f, 6.0f);
    g.setColour(juce::Colours::black.withAlpha(0.18f));
    g.fillRoundedRectangle(graph, 8.0f);
    g.setColour(brass.withAlpha(0.2f));
    g.drawLine(graph.getX(), graph.getCentreY(), graph.getRight(), graph.getCentreY(), 1.0f);

    juce::Path path;
    for (auto step = 0; step < data.stepCount; ++step)
    {
        const auto x = graph.getX() + graph.getWidth() * (float) step / (float) juce::jmax(1, data.stepCount - 1);
        const auto y = graph.getCentreY() - data.referenceTrace[(size_t) step] * graph.getHeight() * 0.38f;
        if (step == 0)
            path.startNewSubPath(x, y);
        else
            path.lineTo(x, y);

        if (data.resetMarkers[(size_t) step])
        {
            g.setColour(warning.withAlpha(0.48f));
            g.drawVerticalLine((int) std::round(x), graph.getY(), graph.getBottom());
        }
    }

    g.setColour(trace);
    g.strokePath(path, juce::PathStrokeType(2.0f));

    const auto laneGap = 6.0f;
    const auto laneHeight = (lanesArea.getHeight() - laneGap * (float) (data.lanes.size() - 1)) / (float) data.lanes.size();
    for (auto laneIndex = 0; laneIndex < (int) data.lanes.size(); ++laneIndex)
    {
        auto row = lanesArea.removeFromTop(laneHeight);
        if (laneIndex < (int) data.lanes.size() - 1)
            lanesArea.removeFromTop(laneGap);

        auto label = row.removeFromLeft(108.0f);
        auto grid = row;
        const auto gap = 4.0f;
        const auto cellWidth = (grid.getWidth() - gap * (float) (data.stepCount - 1)) / (float) data.stepCount;
        const auto& lane = data.lanes[(size_t) laneIndex];

        g.setColour(lane.colour);
        g.setFont(uiFont("Avenir Next", 11.0f, juce::Font::bold));
        g.drawText(lane.name, label.removeFromTop(16.0f).toNearestInt(), juce::Justification::centredLeft);
        g.setColour(textDim);
        g.setFont(uiFont("Menlo", 9.0f, juce::Font::plain));
        g.drawText("note " + juce::String(lane.steps.front().note), label.toNearestInt(), juce::Justification::centredLeft);

        for (auto step = 0; step < data.stepCount; ++step)
        {
            const auto x = grid.getX() + (cellWidth + gap) * (float) step;
            auto cell = juce::Rectangle<float>(x, grid.getY(), cellWidth, grid.getHeight());
            const auto& laneStep = lane.steps[(size_t) step];

            g.setColour(juce::Colours::white.withAlpha(step == data.currentStep ? 0.08f : 0.03f));
            g.fillRoundedRectangle(cell, 3.0f);

            if (! laneStep.active)
                continue;

            const auto height = juce::jmax(5.0f, cell.getHeight() * laneStep.intensity);
            auto block = juce::Rectangle<float>(cell.getX(), cell.getBottom() - height, cell.getWidth(), height);
            auto colour = laneStep.overloaded ? warning : lane.colour;
            if (laneStep.accent)
                colour = colour.brighter(0.18f);
            g.setColour(colour);
            g.fillRoundedRectangle(block, 3.0f);

            if (laneStep.overloaded)
            {
                g.setColour(textBright.withAlpha(0.9f));
                g.fillEllipse(cell.getCentreX() - 2.0f, cell.getY() + 2.0f, 4.0f, 4.0f);
            }
        }
    }
}

void Gp6PanelVisualizer::drawStatus(juce::Graphics& g, juce::Rectangle<float> area, const Gp6Snapshot& data)
{
    if (data.modules.empty())
        return;

    const auto moduleCount = (int) data.modules.size();
    const auto columns = juce::jmin(3, moduleCount);
    const auto rows = (moduleCount + columns - 1) / columns;
    const auto gap = 8.0f;
    const auto cardWidth = (area.getWidth() - gap * (float) (columns - 1)) / (float) columns;
    const auto cardHeight = (area.getHeight() - gap * (float) (rows - 1)) / (float) rows;

    for (auto index = 0; index < moduleCount; ++index)
    {
        const auto column = index % columns;
        const auto row = index / columns;
        auto card = juce::Rectangle<float>(area.getX() + (cardWidth + gap) * (float) column,
                                           area.getY() + (cardHeight + gap) * (float) row,
                                           cardWidth,
                                           cardHeight);
        g.setColour(moduleFace);
        g.fillRoundedRectangle(card, 12.0f);

        auto body = card.reduced(10.0f);
        const auto& module = data.modules[(size_t) index];
        const auto moduleName = module.name.isNotEmpty() ? module.name : juce::String("Module");
        g.setColour(textBright);
        g.setFont(uiFont("Avenir Next", 11.0f, juce::Font::bold));
        g.drawText(moduleName, body.removeFromTop(14.0f).toNearestInt(), juce::Justification::centredLeft);

        auto meter = body.removeFromBottom(8.0f);
        g.setColour(textDim);
        g.setFont(uiFont("Menlo", 10.0f, juce::Font::plain));
        g.drawText(module.overloaded ? "over" : "stable", body.toNearestInt(), juce::Justification::centredLeft);

        g.setColour(juce::Colours::black.withAlpha(0.22f));
        g.fillRoundedRectangle(meter, 4.0f);
        g.setColour(module.overloaded ? warning : brass);
        g.fillRoundedRectangle(meter.withWidth(meter.getWidth() * module.level), 4.0f);
    }
}

Gp6PatchMatrixComponent::Gp6PatchMatrixComponent(Gp6AudioProcessor& processorToUse)
    : processor(processorToUse),
      controls {{
          { "cable1Amount", "cable1Source", "cable1Destination", brass, {}, {}, nullptr },
          { "cable2Amount", "cable2Source", "cable2Destination", mint, {}, {}, nullptr },
          { "cable3Amount", "cable3Source", "cable3Destination", trace, {}, {}, nullptr },
          { "cable4Amount", "cable4Source", "cable4Destination", juce::Colour::fromRGB(146, 186, 255), {}, {}, nullptr },
          { "cable5Amount", "cable5Source", "cable5Destination", warning, {}, {}, nullptr },
          { "cable6Amount", "cable6Source", "cable6Destination", juce::Colour::fromRGB(242, 233, 126), {}, {}, nullptr },
          { "cable7Amount", "cable7Source", "cable7Destination", juce::Colour::fromRGB(234, 154, 214), {}, {}, nullptr },
          { "cable8Amount", "cable8Source", "cable8Destination", juce::Colour::fromRGB(120, 213, 255), {}, {}, nullptr },
          { "cable9Amount", "cable9Source", "cable9Destination", juce::Colour::fromRGB(148, 238, 153), {}, {}, nullptr },
          { "cable10Amount", "cable10Source", "cable10Destination", juce::Colour::fromRGB(255, 171, 107), {}, {}, nullptr },
          { "cable11Amount", "cable11Source", "cable11Destination", juce::Colour::fromRGB(255, 126, 173), {}, {}, nullptr },
          { "cable12Amount", "cable12Source", "cable12Destination", juce::Colour::fromRGB(196, 182, 255), {}, {}, nullptr }
      }}
{
    for (auto index = 0; index < (int) controls.size(); ++index)
        configureControl(controls[(size_t) index], index);

    startTimerHz(24);
}

Gp6PatchMatrixComponent::~Gp6PatchMatrixComponent() = default;

void Gp6PatchMatrixComponent::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    drawBackdrop(g, area);
}

void Gp6PatchMatrixComponent::resized()
{
    layoutControls();
}

void Gp6PatchMatrixComponent::timerCallback()
{
    snapshot = processor.getSnapshot();
    layoutControls();
    repaint();
}

void Gp6PatchMatrixComponent::mouseDown(const juce::MouseEvent& event)
{
    bool draggingSource = false;
    const auto patchArea = getLocalBounds().reduced(18).toFloat();
    draggedCableIndex = findCableEndpoint(event.position, patchArea, draggingSource);
    if (draggedCableIndex < 0)
        draggedCableIndex = findCableBody(event.position, patchArea);
    draggedEndpointIsSource = draggingSource;
    dragPosition = event.position;
    selectedCableIndex = draggedCableIndex;
    layoutControls();
    if (draggedCableIndex >= 0)
        repaint();
}

void Gp6PatchMatrixComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (draggedCableIndex < 0)
        return;

    dragPosition = event.position;
    layoutControls();
    repaint();
}

void Gp6PatchMatrixComponent::mouseUp(const juce::MouseEvent& event)
{
    if (draggedCableIndex < 0)
        return;

    const auto area = getLocalBounds().reduced(18).toFloat();
    if (draggedEndpointIsSource)
    {
        const auto sourceIndex = findNearestSourceNode(event.position, area);
        if (sourceIndex >= 0)
            setChoiceParameter(controls[(size_t) draggedCableIndex].sourceParameterID, sourceIndex);
    }
    else
    {
        const auto destinationIndex = findNearestDestinationNode(event.position, area);
        if (destinationIndex >= 0)
            setChoiceParameter(controls[(size_t) draggedCableIndex].destinationParameterID, destinationIndex);
    }

    draggedCableIndex = -1;
    layoutControls();
    repaint();
}

void Gp6PatchMatrixComponent::configureControl(PatchControl& control, int cableIndex)
{
    control.slider.setSliderStyle(juce::Slider::LinearVertical);
    control.slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    control.slider.setLookAndFeel(&cableFaderLookAndFeel);
    control.slider.setColour(juce::Slider::trackColourId, control.colour);
    control.slider.setColour(juce::Slider::thumbColourId, textBright);
    addAndMakeVisible(control.slider);

    control.label.setText("Cable " + juce::String(cableIndex + 1), juce::dontSendNotification);
    control.label.setJustificationType(juce::Justification::centred);
    control.label.setColour(juce::Label::textColourId, textDim);
    control.label.setFont(uiFont("Menlo", 9.0f, juce::Font::plain));
    addAndMakeVisible(control.label);

    control.attachment = std::make_unique<SliderAttachment>(processor.apvts, control.amountParameterID, control.slider);
}

void Gp6PatchMatrixComponent::drawBackdrop(juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour(shell);
    g.fillRoundedRectangle(area, 22.0f);
    g.setColour(shellOutline);
    g.drawRoundedRectangle(area.reduced(0.75f), 22.0f, 1.0f);

    auto content = area.reduced(12.0f);
    content.removeFromTop(8.0f);
    auto footer = content.removeFromBottom(34.0f);
    auto patchField = content.reduced(2.0f, 10.0f);

    g.setColour(juce::Colours::black.withAlpha(0.10f));
    g.fillRoundedRectangle(patchField, 16.0f);
    g.setColour(shellOutline.withAlpha(0.7f));
    g.drawRoundedRectangle(patchField, 16.0f, 1.0f);

    const auto nodeArea = patchField.reduced(34.0f, 24.0f);
    const auto sources = sourceNodes(nodeArea);
    const auto destinations = destinationNodes(nodeArea);

    auto sourceDeck = juce::Rectangle<float>(patchField.getX() + 18.0f, patchField.getY() + 28.0f, patchField.getWidth() - 36.0f, patchField.getHeight() * 0.25f);
    auto destinationDeck = juce::Rectangle<float>(patchField.getX() + 18.0f, patchField.getBottom() - 112.0f, patchField.getWidth() - 36.0f, 82.0f);
    g.setColour(brass.withAlpha(0.06f));
    g.fillRoundedRectangle(sourceDeck, 18.0f);
    g.setColour(steel.withAlpha(0.06f));
    g.fillRoundedRectangle(destinationDeck, 18.0f);

    for (const auto& node : sources)
    {
        auto bounds = juce::Rectangle<float>(node.point.x - 22.0f, node.point.y - 22.0f, 44.0f, 44.0f);
        g.setColour(brass.withAlpha(0.18f));
        g.fillEllipse(bounds.expanded(6.0f));
        g.setColour(moduleFace.brighter(0.12f));
        g.fillEllipse(bounds);
        g.setColour(brass.withAlpha(0.7f));
        g.drawEllipse(bounds.reduced(1.0f), 2.0f);
        g.setColour(textBright);
        g.setFont(uiFont("Menlo", 9.0f, juce::Font::plain));
        g.drawText(node.name, bounds.toNearestInt(), juce::Justification::centred);
        g.setColour(textDim);
        g.setFont(uiFont("Avenir Next", 9.0f, juce::Font::bold));
        g.drawText("OUT",
                   juce::Rectangle<float>(bounds.getX() - 6.0f, bounds.getBottom() + 4.0f, bounds.getWidth() + 12.0f, 12.0f).toNearestInt(),
                   juce::Justification::centred);
    }

    for (const auto& node : destinations)
    {
        auto bounds = juce::Rectangle<float>(node.point.x - 22.0f, node.point.y - 22.0f, 44.0f, 44.0f);
        g.setColour(steel.withAlpha(0.16f));
        g.fillRoundedRectangle(bounds.expanded(6.0f), 12.0f);
        g.setColour(moduleFace.brighter(0.08f));
        g.fillEllipse(bounds);
        g.setColour(steel.withAlpha(0.7f));
        g.drawEllipse(bounds.reduced(1.0f), 2.0f);
        g.setColour(textBright);
        g.setFont(uiFont("Menlo", 9.0f, juce::Font::plain));
        g.drawText(node.name, bounds.toNearestInt(), juce::Justification::centred);
        g.setColour(textDim);
        g.setFont(uiFont("Avenir Next", 9.0f, juce::Font::bold));
        g.drawText("IN",
                   juce::Rectangle<float>(bounds.getX() - 6.0f, bounds.getY() - 18.0f, bounds.getWidth() + 12.0f, 12.0f).toNearestInt(),
                   juce::Justification::centred);
    }

    for (auto index = 0; index < (int) controls.size(); ++index)
    {
        const auto isSelected = index == selectedCableIndex;
        const auto controlAlpha = isSelected || selectedCableIndex < 0 ? 1.0f : 0.32f;
        const auto amount = snapshot.patchPoints.size() > (size_t) index ? snapshot.patchPoints[(size_t) index].amount
                                                                          : (float) controls[(size_t) index].slider.getValue();
        auto start = currentSourcePoint(index, nodeArea);
        auto end = currentDestinationPoint(index, nodeArea);
        if (draggedCableIndex == index)
        {
            if (draggedEndpointIsSource)
                start = dragPosition;
            else
                end = dragPosition;
        }

        drawPatchCable(g,
                       start,
                       end,
                       controls[(size_t) index].colour.withMultipliedAlpha(controlAlpha),
                       amount);

        g.setColour(textBright.withAlpha(0.92f * controlAlpha));
        g.fillEllipse(start.x - 6.0f, start.y - 6.0f, 12.0f, 12.0f);
        g.fillEllipse(end.x - 6.0f, end.y - 6.0f, 12.0f, 12.0f);

        if (snapshot.patchPoints.size() > (size_t) index)
        {
            controls[(size_t) index].label.setText(snapshot.patchPoints[(size_t) index].from + " -> "
                                                       + snapshot.patchPoints[(size_t) index].to + "  "
                                                       + juce::String(amount, 2),
                                                   juce::dontSendNotification);
        }

        controls[(size_t) index].slider.setAlpha(controlAlpha);
        controls[(size_t) index].label.setAlpha(isSelected || selectedCableIndex < 0 ? 1.0f : 0.45f);
    }

    g.setColour(moduleFace);
    g.fillRoundedRectangle(footer, 12.0f);
    g.setColour(textDim);
    g.setFont(uiFont("Avenir Next", 11.0f, juce::Font::plain));
    g.drawText("Resets " + juce::String(snapshot.resetCount) + "   Overload " + juce::String(snapshot.overloadRatio, 2)
               + "   Ref " + juce::String(snapshot.referenceLevel, 2),
               footer.reduced(14.0f).toNearestInt(),
               juce::Justification::centredLeft);
}

std::vector<Gp6PatchMatrixComponent::NodePosition> Gp6PatchMatrixComponent::sourceNodes(juce::Rectangle<float> area) const
{
    std::vector<NodePosition> nodes;
    const auto names = Gp6PatchEngine::sourceNames();
    nodes.reserve((size_t) names.size());
    const auto left = area.getX() + area.getWidth() * 0.04f;
    const auto right = area.getRight() - area.getWidth() * 0.04f;
    const auto top = area.getY() + area.getHeight() * 0.06f;
    const auto rowGap = area.getHeight() * 0.14f;
    const auto perRow = 6;

    for (auto index = 0; index < names.size(); ++index)
    {
        const auto row = index / perRow;
        const auto column = index % perRow;
        const auto x = juce::jmap((float) column, 0.0f, (float) (perRow - 1), left, right);
        const auto y = top + rowGap * (float) row;
        nodes.push_back({ names[index], { x, y } });
    }

    return nodes;
}

std::vector<Gp6PatchMatrixComponent::NodePosition> Gp6PatchMatrixComponent::destinationNodes(juce::Rectangle<float> area) const
{
    std::vector<NodePosition> nodes;
    const auto names = Gp6PatchEngine::destinationNames();
    nodes.reserve((size_t) names.size());
    const auto left = area.getX() + area.getWidth() * 0.05f;
    const auto right = area.getRight() - area.getWidth() * 0.05f;
    const auto y = area.getBottom() - area.getHeight() * 0.08f;

    for (auto index = 0; index < names.size(); ++index)
    {
        const auto x = juce::jmap((float) index, 0.0f, (float) juce::jmax(1, names.size() - 1), left, right);
        nodes.push_back({ names[index], { x, y } });
    }

    return nodes;
}

juce::Point<float> Gp6PatchMatrixComponent::currentSourcePoint(int cableIndex, juce::Rectangle<float> area) const
{
    const auto nodes = sourceNodes(area);
    const auto sourceIndex = snapshot.patchPoints.size() > (size_t) cableIndex
                               ? juce::jlimit(0, (int) nodes.size() - 1, snapshot.patchPoints[(size_t) cableIndex].sourceIndex)
                               : 0;
    return nodes[(size_t) sourceIndex].point;
}

juce::Point<float> Gp6PatchMatrixComponent::currentDestinationPoint(int cableIndex, juce::Rectangle<float> area) const
{
    const auto nodes = destinationNodes(area);
    const auto destinationIndex = snapshot.patchPoints.size() > (size_t) cableIndex
                                    ? juce::jlimit(0, (int) nodes.size() - 1, snapshot.patchPoints[(size_t) cableIndex].destinationIndex)
                                    : 0;
    return nodes[(size_t) destinationIndex].point;
}

int Gp6PatchMatrixComponent::findCableEndpoint(const juce::Point<float>& position, juce::Rectangle<float> area, bool& draggingSource) const
{
    auto bestDistance = 22.0f;
    auto bestIndex = -1;
    draggingSource = false;

    for (auto cableIndex = 0; cableIndex < (int) controls.size(); ++cableIndex)
    {
        const auto sourceDistance = position.getDistanceFrom(currentSourcePoint(cableIndex, area));
        if (sourceDistance < bestDistance)
        {
            bestDistance = sourceDistance;
            bestIndex = cableIndex;
            draggingSource = true;
        }

        const auto destinationDistance = position.getDistanceFrom(currentDestinationPoint(cableIndex, area));
        if (destinationDistance < bestDistance)
        {
            bestDistance = destinationDistance;
            bestIndex = cableIndex;
            draggingSource = false;
        }
    }

    return bestIndex;
}

int Gp6PatchMatrixComponent::findCableBody(const juce::Point<float>& position, juce::Rectangle<float> area) const
{
    auto bestDistance = 14.0f;
    auto bestIndex = -1;

    for (auto cableIndex = 0; cableIndex < (int) controls.size(); ++cableIndex)
    {
        const auto start = currentSourcePoint(cableIndex, area);
        const auto end = currentDestinationPoint(cableIndex, area);
        const auto bendY = juce::jmin(start.y, end.y) + std::abs(end.y - start.y) * 0.42f;

        juce::Point<float> previous = start;
        for (auto segment = 1; segment <= 24; ++segment)
        {
            const auto t = (float) segment / 24.0f;
            const auto oneMinusT = 1.0f - t;
            const auto point = juce::Point<float>(
                oneMinusT * oneMinusT * oneMinusT * start.x
                + 3.0f * oneMinusT * oneMinusT * t * start.x
                + 3.0f * oneMinusT * t * t * end.x
                + t * t * t * end.x,
                oneMinusT * oneMinusT * oneMinusT * start.y
                + 3.0f * oneMinusT * oneMinusT * t * bendY
                + 3.0f * oneMinusT * t * t * bendY
                + t * t * t * end.y);

            juce::Line<float> line(previous, point);
            juce::Point<float> pointOnLine;
            const auto distance = line.getDistanceFromPoint(position, pointOnLine);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestIndex = cableIndex;
            }

            previous = point;
        }
    }

    return bestIndex;
}

int Gp6PatchMatrixComponent::findNearestSourceNode(const juce::Point<float>& position, juce::Rectangle<float> area) const
{
    const auto nodes = sourceNodes(area);
    auto bestDistance = 30.0f;
    auto bestIndex = -1;

    for (auto index = 0; index < (int) nodes.size(); ++index)
    {
        const auto distance = position.getDistanceFrom(nodes[(size_t) index].point);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestIndex = index;
        }
    }

    return bestIndex;
}

int Gp6PatchMatrixComponent::findNearestDestinationNode(const juce::Point<float>& position, juce::Rectangle<float> area) const
{
    const auto nodes = destinationNodes(area);
    auto bestDistance = 30.0f;
    auto bestIndex = -1;

    for (auto index = 0; index < (int) nodes.size(); ++index)
    {
        const auto distance = position.getDistanceFrom(nodes[(size_t) index].point);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestIndex = index;
        }
    }

    return bestIndex;
}

void Gp6PatchMatrixComponent::setChoiceParameter(const juce::String& parameterID, int value)
{
    if (auto* parameter = processor.apvts.getParameter(parameterID))
        parameter->setValueNotifyingHost(parameter->convertTo0to1((float) value));
}

void Gp6PatchMatrixComponent::layoutControls()
{
    auto area = getLocalBounds().reduced(12).toFloat();
    area.removeFromTop(24.0f);
    area.removeFromBottom(34.0f);
    area = area.reduced(34.0f, 24.0f);

    for (auto index = 0; index < (int) controls.size(); ++index)
    {
        const auto sourcePoint = currentSourcePoint(index, area);
        const auto destinationPoint = currentDestinationPoint(index, area);
        const auto midpoint = patchCurvePoint(sourcePoint, destinationPoint, 0.5f);
        auto tangent = patchCurveTangent(sourcePoint, destinationPoint, 0.5f);
        if (tangent.getDistanceFromOrigin() < 0.001f)
            tangent = destinationPoint - sourcePoint;
        if (tangent.getDistanceFromOrigin() < 0.001f)
            tangent = { 1.0f, 0.0f };

        const auto angle = std::atan2(tangent.y, tangent.x) + juce::MathConstants<float>::pi;
        auto sliderBounds = juce::Rectangle<int>((int) std::round(midpoint.x - 14.0f),
                                                 (int) std::round(midpoint.y - 42.0f),
                                                 28,
                                                 84).constrainedWithin(area.toNearestInt().reduced(6));
        auto labelBounds = juce::Rectangle<int>((int) std::round(midpoint.x - 50.0f),
                                                (int) std::round(midpoint.y + 36.0f),
                                                100,
                                                18).constrainedWithin(area.toNearestInt().reduced(6));

        controls[(size_t) index].slider.setTransform({});
        controls[(size_t) index].slider.setBounds(sliderBounds);
        controls[(size_t) index].slider.setTransform(juce::AffineTransform::rotation(angle,
                                                                                      (float) sliderBounds.getCentreX(),
                                                                                      (float) sliderBounds.getCentreY()));
        controls[(size_t) index].label.setBounds(labelBounds);

        if (index == selectedCableIndex)
        {
            controls[(size_t) index].slider.toFront(false);
            controls[(size_t) index].label.toFront(false);
        }
    }
}

void Gp6PatchMatrixComponent::drawPatchCable(juce::Graphics& g,
                                             juce::Point<float> start,
                                             juce::Point<float> end,
                                             juce::Colour colour,
                                             float amount) const
{
    juce::Path cable;
    const auto bendY = juce::jmin(start.y, end.y) + std::abs(end.y - start.y) * 0.42f;
    cable.startNewSubPath(start);
    cable.cubicTo(start.x, bendY, end.x, bendY, end.x, end.y);

    g.setColour(colour.withAlpha(0.14f + amount * 0.22f));
    g.strokePath(cable, juce::PathStrokeType(10.0f));
    g.setColour(colour.withAlpha(0.36f + amount * 0.56f));
    g.strokePath(cable, juce::PathStrokeType(2.4f + amount * 3.2f));
}

Gp6AudioProcessorEditor::Gp6AudioProcessorEditor(Gp6AudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), visualizer(p), patchMatrix(p)
{
    setSize(1110, 760);

    configureCombo(presetBox);
    configureCombo(patchBox);
    configureCombo(operatingModeBox);
    configureCombo(repetitiveModeBox);

    presetBox.addItemList(Gp6AudioProcessor::presetNames(), 1);
    patchBox.addItemList(Gp6PatchEngine::patchPrograms(), 1);
    operatingModeBox.addItemList(Gp6PatchEngine::operatingModes(), 1);
    repetitiveModeBox.addItemList(Gp6PatchEngine::repetitiveModes(), 1);

    configureLabel(presetLabel, "Preset");
    configureLabel(patchLabel, "Program");
    configureLabel(operatingModeLabel, "Mode");
    configureLabel(repetitiveModeLabel, "Repeat");

    configureSlider(timeScaleSlider);
    configureSlider(patchDensitySlider);
    configureSlider(feedbackSlider);
    configureSlider(multiplierSlider);
    configureSlider(nonlinearitySlider);
    configureSlider(rotationSlider);
    configureSlider(swingSlider);
    configureSlider(probabilitySlider);
    configureSlider(overloadSenseSlider);
    configureSlider(rootNoteSlider);
    configureSlider(spreadSlider);
    configureSlider(channelSlider);
    configureSlider(gateSlider);

    configureLabel(timeScaleLabel, "Time Scale");
    configureLabel(patchDensityLabel, "Patch Density");
    configureLabel(feedbackLabel, "Feedback");
    configureLabel(multiplierLabel, "Multiplier");
    configureLabel(nonlinearityLabel, "Nonlinearity");
    configureLabel(rotationLabel, "Rotation");
    configureLabel(swingLabel, "Swing");
    configureLabel(probabilityLabel, "Probability");
    configureLabel(overloadSenseLabel, "Overload Sense");
    configureLabel(rootNoteLabel, "Root Note");
    configureLabel(spreadLabel, "Spread");
    configureLabel(channelLabel, "Channel");
    configureLabel(gateLabel, "Gate");

    tabs.setTabBarDepth(34);
    tabs.addTab("Monitor", juce::Colours::transparentBlack, &visualizer, false);
    tabs.addTab("Patch Panel", juce::Colours::transparentBlack, &patchMatrix, false);
    tabs.setColour(juce::TabbedComponent::backgroundColourId, juce::Colours::transparentBlack);
    tabs.setColour(juce::TabbedButtonBar::tabOutlineColourId, brass.withAlpha(0.2f));
    tabs.setColour(juce::TabbedButtonBar::frontOutlineColourId, brass.withAlpha(0.32f));
    tabs.setColour(juce::TabbedButtonBar::tabTextColourId, textBright);
    addAndMakeVisible(tabs);

    presetAttachment = std::make_unique<ComboAttachment>(audioProcessor.apvts, "preset", presetBox);
    patchAttachment = std::make_unique<ComboAttachment>(audioProcessor.apvts, "patchProgram", patchBox);
    operatingModeAttachment = std::make_unique<ComboAttachment>(audioProcessor.apvts, "operatingMode", operatingModeBox);
    repetitiveModeAttachment = std::make_unique<ComboAttachment>(audioProcessor.apvts, "repetitiveMode", repetitiveModeBox);
    timeScaleAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "timeScale", timeScaleSlider);
    patchDensityAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "patchDensity", patchDensitySlider);
    feedbackAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "feedback", feedbackSlider);
    multiplierAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "multiplier", multiplierSlider);
    nonlinearityAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "nonlinearity", nonlinearitySlider);
    rotationAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "rotation", rotationSlider);
    swingAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "swing", swingSlider);
    probabilityAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "probability", probabilitySlider);
    overloadSenseAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "overloadSense", overloadSenseSlider);
    rootNoteAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "rootNote", rootNoteSlider);
    spreadAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "spread", spreadSlider);
    channelAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "channel", channelSlider);
    gateAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "gate", gateSlider);
}

Gp6AudioProcessorEditor::~Gp6AudioProcessorEditor() = default;

void Gp6AudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient gradient(bgTop, 0.0f, 0.0f, bgBottom, 0.0f, (float) getHeight(), false);
    gradient.addColour(0.38, juce::Colour::fromRGB(63, 54, 40));
    g.setGradientFill(gradient);
    g.fillAll();

    g.setColour(brass.withAlpha(0.08f));
    for (auto y = 0; y < getHeight(); y += 24)
        g.drawHorizontalLine(y, 0.0f, (float) getWidth());

    auto halo = getLocalBounds().toFloat().reduced(24.0f).removeFromTop(180.0f);
    g.setColour(trace.withAlpha(0.07f));
    g.fillEllipse(halo.withTrimmedLeft(getWidth() * 0.36f));
}

void Gp6AudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(14);
    auto leftRail = area.removeFromLeft(250);
    auto topRail = area.removeFromTop(72);
    tabs.setBounds(area);

    auto placeCombo = [](juce::Rectangle<int> bounds, juce::Label& label, juce::ComboBox& box)
    {
        label.setBounds(bounds.removeFromTop(13));
        box.setBounds(bounds.removeFromTop(25));
    };

    auto topContent = topRail.reduced(6, 4);
    const auto gap = 6;
    const auto comboWidth = (topContent.getWidth() - gap * 3) / 4;

    auto presetArea = topContent.removeFromLeft(comboWidth);
    placeCombo(presetArea, presetLabel, presetBox);
    topContent.removeFromLeft(gap);

    auto patchArea = topContent.removeFromLeft(comboWidth);
    placeCombo(patchArea, patchLabel, patchBox);
    topContent.removeFromLeft(gap);

    auto modeArea = topContent.removeFromLeft(comboWidth);
    placeCombo(modeArea, operatingModeLabel, operatingModeBox);
    topContent.removeFromLeft(gap);

    auto repeatArea = topContent.removeFromLeft(comboWidth);
    placeCombo(repeatArea, repetitiveModeLabel, repetitiveModeBox);

    auto leftContent = leftRail.reduced(6, 4);
    auto columnA = leftContent.removeFromTop(leftContent.getHeight() / 2);
    auto columnB = leftContent;

    auto placeSlider = [](juce::Rectangle<int>& source, juce::Label& label, juce::Slider& slider)
    {
        auto row = source.removeFromTop(36).reduced(0, 1);
        label.setBounds(row.removeFromTop(11));
        slider.setBounds(row.removeFromTop(22));
    };

    placeSlider(columnA, timeScaleLabel, timeScaleSlider);
    placeSlider(columnA, patchDensityLabel, patchDensitySlider);
    placeSlider(columnA, feedbackLabel, feedbackSlider);
    placeSlider(columnA, multiplierLabel, multiplierSlider);
    placeSlider(columnA, nonlinearityLabel, nonlinearitySlider);
    placeSlider(columnA, rotationLabel, rotationSlider);

    placeSlider(columnB, swingLabel, swingSlider);
    placeSlider(columnB, probabilityLabel, probabilitySlider);
    placeSlider(columnB, overloadSenseLabel, overloadSenseSlider);
    placeSlider(columnB, rootNoteLabel, rootNoteSlider);
    placeSlider(columnB, spreadLabel, spreadSlider);
    placeSlider(columnB, channelLabel, channelSlider);
    placeSlider(columnB, gateLabel, gateSlider);
}

void Gp6AudioProcessorEditor::configureSlider(juce::Slider& slider)
{
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    slider.setColour(juce::Slider::trackColourId, brass);
    slider.setColour(juce::Slider::thumbColourId, trace);
    slider.setColour(juce::Slider::backgroundColourId, juce::Colours::black.withAlpha(0.24f));
    slider.setColour(juce::Slider::textBoxTextColourId, textBright);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::black.withAlpha(0.16f));
    addAndMakeVisible(slider);
}

void Gp6AudioProcessorEditor::configureCombo(juce::ComboBox& box)
{
    box.setColour(juce::ComboBox::backgroundColourId, juce::Colours::black.withAlpha(0.18f));
    box.setColour(juce::ComboBox::outlineColourId, brass.withAlpha(0.3f));
    box.setColour(juce::ComboBox::textColourId, textBright);
    box.setColour(juce::ComboBox::arrowColourId, trace);
    addAndMakeVisible(box);
}

void Gp6AudioProcessorEditor::configureLabel(juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setColour(juce::Label::textColourId, textDim);
    label.setFont(uiFont("Avenir Next", 12.0f, juce::Font::plain));
    addAndMakeVisible(label);
}
