#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "Gp6PluginProcessor.h"

class Gp6PanelVisualizer : public juce::Component, private juce::Timer
{
public:
    explicit Gp6PanelVisualizer(Gp6AudioProcessor& processorToUse);

    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;
    void drawHeader(juce::Graphics& g, juce::Rectangle<float> area, const Gp6Snapshot& snapshot);
    void drawPatchbay(juce::Graphics& g, juce::Rectangle<float> area, const Gp6Snapshot& snapshot);
    void drawIntegratorLanes(juce::Graphics& g, juce::Rectangle<float> area, const Gp6Snapshot& snapshot);
    void drawStatus(juce::Graphics& g, juce::Rectangle<float> area, const Gp6Snapshot& snapshot);

    Gp6AudioProcessor& processor;
    Gp6Snapshot snapshot;
};

class Gp6PatchMatrixComponent : public juce::Component, private juce::Timer
{
public:
    explicit Gp6PatchMatrixComponent(Gp6AudioProcessor& processorToUse);
    ~Gp6PatchMatrixComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    struct PatchControl
    {
        juce::String amountParameterID;
        juce::String sourceParameterID;
        juce::String destinationParameterID;
        juce::Colour colour;
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    struct NodePosition
    {
        juce::String name;
        juce::Point<float> point;
    };

    void timerCallback() override;
    void configureControl(PatchControl& control, int cableIndex);
    void drawBackdrop(juce::Graphics& g, juce::Rectangle<float> area);
    void drawPatchCable(juce::Graphics& g,
                        juce::Point<float> start,
                        juce::Point<float> end,
                        juce::Colour colour,
                        float amount) const;
    std::vector<NodePosition> sourceNodes(juce::Rectangle<float> area) const;
    std::vector<NodePosition> destinationNodes(juce::Rectangle<float> area) const;
    juce::Point<float> currentSourcePoint(int cableIndex, juce::Rectangle<float> area) const;
    juce::Point<float> currentDestinationPoint(int cableIndex, juce::Rectangle<float> area) const;
    int findCableEndpoint(const juce::Point<float>& position, juce::Rectangle<float> area, bool& draggingSource) const;
    int findCableBody(const juce::Point<float>& position, juce::Rectangle<float> area) const;
    int findNearestSourceNode(const juce::Point<float>& position, juce::Rectangle<float> area) const;
    int findNearestDestinationNode(const juce::Point<float>& position, juce::Rectangle<float> area) const;
    void setChoiceParameter(const juce::String& parameterID, int value);
    void layoutControls();

    Gp6AudioProcessor& processor;
    Gp6Snapshot snapshot;
    std::array<PatchControl, 12> controls;
    int selectedCableIndex = -1;
    int draggedCableIndex = -1;
    bool draggedEndpointIsSource = false;
    juce::Point<float> dragPosition;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Gp6PatchMatrixComponent)
};

class Gp6AudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit Gp6AudioProcessorEditor(Gp6AudioProcessor&);
    ~Gp6AudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void configureSlider(juce::Slider& slider);
    void configureCombo(juce::ComboBox& box);
    void configureLabel(juce::Label& label, const juce::String& text);

    Gp6AudioProcessor& audioProcessor;
    Gp6PanelVisualizer visualizer;
    Gp6PatchMatrixComponent patchMatrix;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    juce::ComboBox presetBox;
    juce::ComboBox patchBox;
    juce::ComboBox operatingModeBox;
    juce::ComboBox repetitiveModeBox;

    juce::Label presetLabel;
    juce::Label patchLabel;
    juce::Label operatingModeLabel;
    juce::Label repetitiveModeLabel;

    juce::Slider timeScaleSlider;
    juce::Slider patchDensitySlider;
    juce::Slider feedbackSlider;
    juce::Slider multiplierSlider;
    juce::Slider nonlinearitySlider;
    juce::Slider rotationSlider;
    juce::Slider swingSlider;
    juce::Slider probabilitySlider;
    juce::Slider overloadSenseSlider;
    juce::Slider rootNoteSlider;
    juce::Slider spreadSlider;
    juce::Slider channelSlider;
    juce::Slider gateSlider;

    juce::Label timeScaleLabel;
    juce::Label patchDensityLabel;
    juce::Label feedbackLabel;
    juce::Label multiplierLabel;
    juce::Label nonlinearityLabel;
    juce::Label rotationLabel;
    juce::Label swingLabel;
    juce::Label probabilityLabel;
    juce::Label overloadSenseLabel;
    juce::Label rootNoteLabel;
    juce::Label spreadLabel;
    juce::Label channelLabel;
    juce::Label gateLabel;

    std::unique_ptr<ComboAttachment> presetAttachment;
    std::unique_ptr<ComboAttachment> patchAttachment;
    std::unique_ptr<ComboAttachment> operatingModeAttachment;
    std::unique_ptr<ComboAttachment> repetitiveModeAttachment;
    std::unique_ptr<SliderAttachment> timeScaleAttachment;
    std::unique_ptr<SliderAttachment> patchDensityAttachment;
    std::unique_ptr<SliderAttachment> feedbackAttachment;
    std::unique_ptr<SliderAttachment> multiplierAttachment;
    std::unique_ptr<SliderAttachment> nonlinearityAttachment;
    std::unique_ptr<SliderAttachment> rotationAttachment;
    std::unique_ptr<SliderAttachment> swingAttachment;
    std::unique_ptr<SliderAttachment> probabilityAttachment;
    std::unique_ptr<SliderAttachment> overloadSenseAttachment;
    std::unique_ptr<SliderAttachment> rootNoteAttachment;
    std::unique_ptr<SliderAttachment> spreadAttachment;
    std::unique_ptr<SliderAttachment> channelAttachment;
    std::unique_ptr<SliderAttachment> gateAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Gp6AudioProcessorEditor)
};
