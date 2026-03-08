#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "ErhcetuaPluginProcessor.h"

class ErhcetuaVisualizer : public juce::Component, private juce::Timer
{
public:
    explicit ErhcetuaVisualizer(ErhcetuaAudioProcessor& processorToUse);

    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;
    void drawStageFlow(juce::Graphics& g, juce::Rectangle<float> area, const ErhcetuaSnapshot& snapshot);
    void drawStepField(juce::Graphics& g, juce::Rectangle<float> area, const ErhcetuaSnapshot& snapshot);
    void drawFooter(juce::Graphics& g, juce::Rectangle<float> area, const ErhcetuaSnapshot& snapshot);

    ErhcetuaAudioProcessor& processor;
    ErhcetuaSnapshot snapshot;
};

class ErhcetuaAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ErhcetuaAudioProcessorEditor(ErhcetuaAudioProcessor&);
    ~ErhcetuaAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void configureSlider(juce::Slider& slider);
    void configureLabel(juce::Label& label, const juce::String& text);
    void configureCombo(juce::ComboBox& box);

    ErhcetuaAudioProcessor& audioProcessor;
    ErhcetuaVisualizer visualizer;

    juce::ComboBox presetBox;
    juce::ComboBox grammarBox;
    juce::ComboBox resetModeBox;
    juce::ComboBox scaleBox;
    juce::ComboBox ruleBox;
    juce::Label presetLabel;
    juce::Label grammarLabel;
    juce::Label resetModeLabel;
    juce::Label scaleLabel;
    juce::Label ruleLabel;

    juce::Slider densitySlider;
    juce::Slider mutationSlider;
    juce::Slider ratchetSlider;
    juce::Slider flutterSlider;
    juce::Slider rotationSlider;
    juce::Slider swingSlider;
    juce::Slider driftSlider;
    juce::Slider gateSkewSlider;
    juce::Slider pitchSpreadSlider;
    juce::Slider probabilitySlider;
    juce::Slider rootNoteSlider;
    juce::Slider octaveSpanSlider;
    juce::Slider channelSlider;
    juce::Slider gateSlider;

    juce::Label densityLabel;
    juce::Label mutationLabel;
    juce::Label ratchetLabel;
    juce::Label flutterLabel;
    juce::Label rotationLabel;
    juce::Label swingLabel;
    juce::Label driftLabel;
    juce::Label gateSkewLabel;
    juce::Label pitchSpreadLabel;
    juce::Label probabilityLabel;
    juce::Label rootNoteLabel;
    juce::Label octaveSpanLabel;
    juce::Label channelLabel;
    juce::Label gateLabel;

    std::unique_ptr<ComboAttachment> presetAttachment;
    std::unique_ptr<ComboAttachment> grammarAttachment;
    std::unique_ptr<ComboAttachment> resetModeAttachment;
    std::unique_ptr<ComboAttachment> scaleAttachment;
    std::unique_ptr<ComboAttachment> ruleAttachment;
    std::unique_ptr<SliderAttachment> densityAttachment;
    std::unique_ptr<SliderAttachment> mutationAttachment;
    std::unique_ptr<SliderAttachment> ratchetAttachment;
    std::unique_ptr<SliderAttachment> flutterAttachment;
    std::unique_ptr<SliderAttachment> rotationAttachment;
    std::unique_ptr<SliderAttachment> swingAttachment;
    std::unique_ptr<SliderAttachment> driftAttachment;
    std::unique_ptr<SliderAttachment> gateSkewAttachment;
    std::unique_ptr<SliderAttachment> pitchSpreadAttachment;
    std::unique_ptr<SliderAttachment> probabilityAttachment;
    std::unique_ptr<SliderAttachment> rootNoteAttachment;
    std::unique_ptr<SliderAttachment> octaveSpanAttachment;
    std::unique_ptr<SliderAttachment> channelAttachment;
    std::unique_ptr<SliderAttachment> gateAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ErhcetuaAudioProcessorEditor)
};
