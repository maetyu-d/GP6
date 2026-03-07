#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

class RhythmVisualizer : public juce::Component, private juce::Timer
{
public:
    explicit RhythmVisualizer(AfricanRhythmsAudioProcessor& processorToUse);

    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;
    void drawWheel(juce::Graphics& g, juce::Rectangle<float> area, const RhythmSnapshot& snapshot);
    void drawTimeline(juce::Graphics& g, juce::Rectangle<float> area, const RhythmSnapshot& snapshot);

    AfricanRhythmsAudioProcessor& processor;
    RhythmSnapshot snapshot;
};

class AfricanRhythmsAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    explicit AfricanRhythmsAudioProcessorEditor(AfricanRhythmsAudioProcessor&);
    ~AfricanRhythmsAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void configureSlider(juce::Slider& slider, const juce::String& suffix);
    void configureNoteSlider(juce::Slider& slider);
    void configureLabel(juce::Label& label, const juce::String& text);

    AfricanRhythmsAudioProcessor& audioProcessor;
    RhythmVisualizer visualizer;

    juce::ComboBox presetBox;
    juce::Label presetLabel;
    juce::ComboBox familyBox;
    juce::Label familyLabel;

    juce::Slider densitySlider;
    juce::Slider complexitySlider;
    juce::Slider rotationSlider;
    juce::Slider swingSlider;
    juce::Slider probabilitySlider;
    juce::Slider bellNoteSlider;
    juce::Slider supportNoteSlider;
    juce::Slider bassNoteSlider;
    juce::Slider shakerNoteSlider;
    juce::Slider channelSlider;
    juce::Slider gateSlider;

    juce::Label densityLabel;
    juce::Label complexityLabel;
    juce::Label rotationLabel;
    juce::Label swingLabel;
    juce::Label probabilityLabel;
    juce::Label bellNoteLabel;
    juce::Label supportNoteLabel;
    juce::Label bassNoteLabel;
    juce::Label shakerNoteLabel;
    juce::Label channelLabel;
    juce::Label gateLabel;

    std::unique_ptr<ComboAttachment> presetAttachment;
    std::unique_ptr<ComboAttachment> familyAttachment;
    std::unique_ptr<SliderAttachment> densityAttachment;
    std::unique_ptr<SliderAttachment> complexityAttachment;
    std::unique_ptr<SliderAttachment> rotationAttachment;
    std::unique_ptr<SliderAttachment> swingAttachment;
    std::unique_ptr<SliderAttachment> probabilityAttachment;
    std::unique_ptr<SliderAttachment> bellNoteAttachment;
    std::unique_ptr<SliderAttachment> supportNoteAttachment;
    std::unique_ptr<SliderAttachment> bassNoteAttachment;
    std::unique_ptr<SliderAttachment> shakerNoteAttachment;
    std::unique_ptr<SliderAttachment> channelAttachment;
    std::unique_ptr<SliderAttachment> gateAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AfricanRhythmsAudioProcessorEditor)
};
