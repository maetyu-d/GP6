#pragma once

#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Gp6PatchEngine.h"

class Gp6AudioProcessor : public juce::AudioProcessor,
                          private juce::AudioProcessorValueTreeState::Listener
{
public:
    Gp6AudioProcessor();
    ~Gp6AudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    Gp6Snapshot getSnapshot() const;
    juce::String getCurrentPresetName() const;

    juce::AudioProcessorValueTreeState apvts;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static juce::StringArray presetNames();

private:
    struct PresetDefinition
    {
        const char* name;
        int patchProgram;
        int operatingMode;
        int repetitiveMode;
        float timeScale;
        float patchDensity;
        float feedback;
        float multiplier;
        float nonlinearity;
        int rotation;
        float swing;
        float probability;
        float overloadSense;
        int rootNote;
        int spread;
        std::array<int, 12> cableSources;
        std::array<int, 12> cableDestinations;
        std::array<float, 12> cableAmounts;
        float gate;
        int channel;
    };

    struct PendingNoteOff
    {
        double ppq = 0.0;
        int note = 60;
        int channel = 1;
    };

    void parameterChanged(const juce::String& parameterID, float newValue) override;
    Gp6PatchEngine::Settings currentSettings() const;
    void refreshSnapshot();
    void addPendingNoteOffs(juce::MidiBuffer& midi, double blockStartPpq, double blockEndPpq, double bpm, int blockSize);
    void sendAllNotesOff(juce::MidiBuffer& midi);
    double quarterNotesToSamples(double quarterNotes, double bpm) const;
    int getRoundedParameter(const juce::String& id) const;
    float getFloatParameter(const juce::String& id) const;
    void applyPreset(int presetIndex);
    void setParameterValue(const juce::String& id, float plainValue);
    static const std::vector<PresetDefinition>& getPresets();

    mutable juce::SpinLock snapshotLock;
    Gp6Snapshot snapshot;
    std::vector<PendingNoteOff> pendingNoteOffs;
    double currentSampleRate = 44100.0;
    double previousBlockPpq = -1.0;
    bool transportWasPlaying = false;
    std::atomic<bool> isApplyingPreset { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Gp6AudioProcessor)
};
