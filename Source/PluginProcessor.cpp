#include "PluginProcessor.h"
#include "PluginEditor.h"

AfricanRhythmsAudioProcessor::AfricanRhythmsAudioProcessor()
    : AudioProcessor(BusesProperties()),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    apvts.addParameterListener("preset", this);
    refreshSnapshot();
}

AfricanRhythmsAudioProcessor::~AfricanRhythmsAudioProcessor()
{
    apvts.removeParameterListener("preset", this);
}

const juce::String AfricanRhythmsAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AfricanRhythmsAudioProcessor::acceptsMidi() const
{
    return true;
}

bool AfricanRhythmsAudioProcessor::producesMidi() const
{
    return true;
}

bool AfricanRhythmsAudioProcessor::isMidiEffect() const
{
    return true;
}

double AfricanRhythmsAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AfricanRhythmsAudioProcessor::getNumPrograms()
{
    return (int) getPresets().size();
}

int AfricanRhythmsAudioProcessor::getCurrentProgram()
{
    return getRoundedParameter("preset");
}

void AfricanRhythmsAudioProcessor::setCurrentProgram(int index)
{
    applyPreset(index);
}

const juce::String AfricanRhythmsAudioProcessor::getProgramName(int index)
{
    const auto names = presetNames();
    return names[index];
}

void AfricanRhythmsAudioProcessor::changeProgramName(int, const juce::String&)
{
}

void AfricanRhythmsAudioProcessor::prepareToPlay(double sampleRate, int)
{
    currentSampleRate = sampleRate;
    previousBlockPpq = -1.0;
    pendingNoteOffs.clear();
    transportWasPlaying = false;
}

void AfricanRhythmsAudioProcessor::releaseResources()
{
}

bool AfricanRhythmsAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    juce::ignoreUnused(layouts);
    return true;
}

void AfricanRhythmsAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    auto playHead = getPlayHead();
    juce::AudioPlayHead::CurrentPositionInfo position;
    const auto hasPosition = playHead != nullptr && playHead->getCurrentPosition(position);

    const auto bpm = hasPosition && position.bpm > 0.0 ? position.bpm : 120.0;
    const auto blockStartPpq = hasPosition ? position.ppqPosition : previousBlockPpq;
    const auto blockQuarterNotes = (static_cast<double>(buffer.getNumSamples()) / currentSampleRate) * (bpm / 60.0);
    const auto blockEndPpq = blockStartPpq + blockQuarterNotes;

    refreshSnapshot();

    if (! hasPosition || ! position.isPlaying)
    {
        if (transportWasPlaying)
            sendAllNotesOff(midiMessages);

        pendingNoteOffs.clear();
        transportWasPlaying = false;
        previousBlockPpq = blockEndPpq;
        return;
    }

    if (previousBlockPpq >= 0.0 && blockStartPpq + 0.001 < previousBlockPpq)
        pendingNoteOffs.clear();

    addPendingNoteOffs(midiMessages, blockStartPpq, blockEndPpq, bpm, buffer.getNumSamples());

    const auto settings = currentSettings();
    RhythmSnapshot localSnapshot;
    {
        const juce::SpinLock::ScopedLockType lock(snapshotLock);
        localSnapshot = snapshot;
    }

    const auto cycleLength = RhythmEngine::cycleLengthQuarterNotes(localSnapshot.stepCount);
    if (cycleLength > 0.0)
    {
        const auto firstCycle = (int) std::floor(blockStartPpq / cycleLength);
        const auto lastCycle = (int) std::floor(blockEndPpq / cycleLength);
        const auto midiChannel = juce::jlimit(1, 16, getRoundedParameter("channel"));
        const auto gate = juce::jlimit(0.08f, 0.95f, getFloatParameter("gate"));

        for (auto cycle = firstCycle; cycle <= lastCycle; ++cycle)
        {
            const auto cycleStart = cycle * cycleLength;

            for (auto step = 0; step < localSnapshot.stepCount; ++step)
            {
                const auto stepOffset = RhythmEngine::stepStartQuarterNotes(step, localSnapshot.stepCount, settings.swing);
                const auto stepLength = RhythmEngine::swungStepLengthQuarterNotes(step, localSnapshot.stepCount, settings.swing);
                const auto stepPpq = cycleStart + stepOffset;

                if (stepPpq < blockStartPpq || stepPpq >= blockEndPpq)
                    continue;

                const auto sampleOffset = juce::jlimit(0,
                                                       juce::jmax(0, buffer.getNumSamples() - 1),
                                                       (int) std::llround(quarterNotesToSamples(stepPpq - blockStartPpq, bpm)));

                for (auto laneIndex = 0; laneIndex < (int) localSnapshot.lanes.size(); ++laneIndex)
                {
                    if (! RhythmEngine::shouldTriggerLaneStep(localSnapshot, laneIndex, step, cycle, settings.probability))
                        continue;

                    const auto velocity = RhythmEngine::velocityForLaneStep(localSnapshot, laneIndex, step);
                    const auto noteNumber = localSnapshot.lanes[(size_t) laneIndex].midiNote;
                    midiMessages.addEvent(juce::MidiMessage::noteOn(midiChannel, noteNumber, velocity), sampleOffset);

                    const auto laneGate = gate * (laneIndex == 2 ? 0.85f : laneIndex == 3 ? 0.5f : 1.0f);
                    const auto noteOffPpq = stepPpq + stepLength * laneGate;
                    pendingNoteOffs.push_back({ noteOffPpq, noteNumber, midiChannel });
                }
            }
        }

        auto stepProgress = std::fmod(blockStartPpq, cycleLength);
        if (stepProgress < 0.0)
            stepProgress += cycleLength;

        auto activeStep = 0;
        for (auto step = 0; step < localSnapshot.stepCount; ++step)
        {
            const auto stepStart = RhythmEngine::stepStartQuarterNotes(step, localSnapshot.stepCount, settings.swing);
            const auto nextStepStart = stepStart + RhythmEngine::swungStepLengthQuarterNotes(step, localSnapshot.stepCount, settings.swing);

            if (stepProgress >= stepStart && stepProgress < nextStepStart)
            {
                activeStep = step;
                break;
            }
        }

        const juce::SpinLock::ScopedLockType lock(snapshotLock);
        snapshot.currentStep = activeStep;
    }

    transportWasPlaying = true;
    previousBlockPpq = blockEndPpq;
}

juce::AudioProcessorEditor* AfricanRhythmsAudioProcessor::createEditor()
{
    return new AfricanRhythmsAudioProcessorEditor(*this);
}

bool AfricanRhythmsAudioProcessor::hasEditor() const
{
    return true;
}

void AfricanRhythmsAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        std::unique_ptr<juce::XmlElement> xml(state.createXml());
        copyXmlToBinary(*xml, destData);
    }
}

void AfricanRhythmsAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));

    refreshSnapshot();
}

RhythmSnapshot AfricanRhythmsAudioProcessor::getSnapshot() const
{
    const juce::SpinLock::ScopedLockType lock(snapshotLock);
    return snapshot;
}

juce::String AfricanRhythmsAudioProcessor::getCurrentPresetName() const
{
    return presetNames()[getRoundedParameter("preset")];
}

juce::AudioProcessorValueTreeState::ParameterLayout AfricanRhythmsAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;

    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("preset", "Preset", presetNames(), 0));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("family", "Family", RhythmEngine::families(), 0));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("density", "Density", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.58f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("complexity", "Complexity", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.4f));
    parameters.push_back(std::make_unique<juce::AudioParameterInt>("rotation", "Rotation", -8, 8, 0));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("swing", "Swing", juce::NormalisableRange<float>(0.0f, 0.45f, 0.005f), 0.08f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("probability", "Probability", juce::NormalisableRange<float>(0.1f, 1.0f, 0.01f), 0.95f));
    parameters.push_back(std::make_unique<juce::AudioParameterInt>("bellNote", "Bell Note", 24, 96, 76));
    parameters.push_back(std::make_unique<juce::AudioParameterInt>("supportNote", "Support Note", 24, 96, 67));
    parameters.push_back(std::make_unique<juce::AudioParameterInt>("bassNote", "Bass Note", 24, 96, 48));
    parameters.push_back(std::make_unique<juce::AudioParameterInt>("shakerNote", "Shaker Note", 24, 96, 82));
    parameters.push_back(std::make_unique<juce::AudioParameterInt>("channel", "Channel", 1, 16, 10));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("gate", "Gate", juce::NormalisableRange<float>(0.08f, 0.95f, 0.01f), 0.45f));

    return { parameters.begin(), parameters.end() };
}

juce::StringArray AfricanRhythmsAudioProcessor::presetNames()
{
    juce::StringArray names;
    for (const auto& preset : getPresets())
        names.add(preset.name);
    return names;
}

RhythmEngine::Settings AfricanRhythmsAudioProcessor::currentSettings() const
{
    RhythmEngine::Settings settings;
    settings.family = getRoundedParameter("family");
    settings.density = getFloatParameter("density");
    settings.complexity = getFloatParameter("complexity");
    settings.rotation = getRoundedParameter("rotation");
    settings.swing = getFloatParameter("swing");
    settings.probability = getFloatParameter("probability");
    settings.laneNotes = {
        getRoundedParameter("bellNote"),
        getRoundedParameter("supportNote"),
        getRoundedParameter("bassNote"),
        getRoundedParameter("shakerNote")
    };
    return settings;
}

void AfricanRhythmsAudioProcessor::refreshSnapshot()
{
    const auto latest = RhythmEngine::generate(currentSettings(), getCurrentPresetName());
    const juce::SpinLock::ScopedLockType lock(snapshotLock);
    snapshot = latest;
}

void AfricanRhythmsAudioProcessor::addPendingNoteOffs(juce::MidiBuffer& midi, double blockStartPpq, double blockEndPpq, double bpm, int blockSize)
{
    std::vector<PendingNoteOff> remaining;
    remaining.reserve(pendingNoteOffs.size());

    for (const auto& noteOff : pendingNoteOffs)
    {
        if (noteOff.ppq >= blockStartPpq && noteOff.ppq < blockEndPpq)
        {
            const auto sampleOffset = juce::jlimit(0,
                                                   juce::jmax(0, blockSize - 1),
                                                   (int) std::llround(quarterNotesToSamples(noteOff.ppq - blockStartPpq, bpm)));
            midi.addEvent(juce::MidiMessage::noteOff(noteOff.channel, noteOff.note), sampleOffset);
        }
        else if (noteOff.ppq >= blockEndPpq)
        {
            remaining.push_back(noteOff);
        }
    }

    pendingNoteOffs = std::move(remaining);
}

void AfricanRhythmsAudioProcessor::sendAllNotesOff(juce::MidiBuffer& midi)
{
    for (auto channel = 1; channel <= 16; ++channel)
        midi.addEvent(juce::MidiMessage::allNotesOff(channel), 0);
}

double AfricanRhythmsAudioProcessor::quarterNotesToSamples(double quarterNotes, double bpm) const
{
    if (bpm <= 0.0)
        return 0.0;

    return quarterNotes * 60.0 * currentSampleRate / bpm;
}

int AfricanRhythmsAudioProcessor::getRoundedParameter(const juce::String& id) const
{
    if (auto* parameter = apvts.getRawParameterValue(id))
        return juce::roundToInt(parameter->load());

    return 0;
}

float AfricanRhythmsAudioProcessor::getFloatParameter(const juce::String& id) const
{
    if (auto* parameter = apvts.getRawParameterValue(id))
        return parameter->load();

    return 0.0f;
}

void AfricanRhythmsAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID != "preset" || isApplyingPreset.load())
        return;

    applyPreset(juce::roundToInt(newValue));
}

void AfricanRhythmsAudioProcessor::applyPreset(int presetIndex)
{
    const auto& presets = getPresets();
    const auto index = juce::jlimit(0, (int) presets.size() - 1, presetIndex);
    const auto& preset = presets[(size_t) index];

    isApplyingPreset.store(true);
    setParameterValue("preset", (float) index);
    setParameterValue("family", (float) preset.family);
    setParameterValue("density", preset.density);
    setParameterValue("complexity", preset.complexity);
    setParameterValue("rotation", (float) preset.rotation);
    setParameterValue("swing", preset.swing);
    setParameterValue("probability", preset.probability);
    setParameterValue("bellNote", (float) preset.laneNotes[0]);
    setParameterValue("supportNote", (float) preset.laneNotes[1]);
    setParameterValue("bassNote", (float) preset.laneNotes[2]);
    setParameterValue("shakerNote", (float) preset.laneNotes[3]);
    setParameterValue("gate", preset.gate);
    setParameterValue("channel", (float) preset.channel);
    isApplyingPreset.store(false);

    refreshSnapshot();
}

void AfricanRhythmsAudioProcessor::setParameterValue(const juce::String& id, float plainValue)
{
    if (auto* parameter = apvts.getParameter(id))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

const std::vector<AfricanRhythmsAudioProcessor::PresetDefinition>& AfricanRhythmsAudioProcessor::getPresets()
{
    static const std::vector<PresetDefinition> presets {
        { "Ewe Bell / Agbekor Drive", 0, 0.56f, 0.36f, 0, 0.06f, 0.97f, { 76, 67, 48, 82 }, 0.46f, 10 },
        { "Ewe Bell / Procession Lift", 0, 0.71f, 0.49f, 1, 0.09f, 0.93f, { 77, 69, 45, 82 }, 0.43f, 10 },
        { "Bembe / Spiral Conversation", 1, 0.62f, 0.58f, -1, 0.12f, 0.91f, { 75, 67, 43, 80 }, 0.41f, 10 },
        { "Bembe / Night Circle", 1, 0.5f, 0.31f, 2, 0.08f, 0.98f, { 73, 65, 47, 83 }, 0.48f, 10 },
        { "Kpanlogo / Dancer Grid", 2, 0.67f, 0.46f, 0, 0.1f, 0.94f, { 78, 70, 45, 84 }, 0.44f, 10 },
        { "Kpanlogo / Open-Air Break", 2, 0.77f, 0.68f, 2, 0.14f, 0.88f, { 79, 72, 43, 86 }, 0.38f, 10 },
        { "Gahu / Rolling Street", 3, 0.64f, 0.53f, 1, 0.11f, 0.92f, { 76, 68, 41, 83 }, 0.42f, 10 },
        { "Gahu / Ceremony Push", 3, 0.81f, 0.73f, -2, 0.16f, 0.86f, { 77, 70, 40, 85 }, 0.36f, 10 },
        { "Sikyi / Swaying Lines", 4, 0.52f, 0.28f, 0, 0.07f, 0.99f, { 74, 66, 47, 81 }, 0.5f, 10 },
        { "Sikyi / Brass Courtyard", 4, 0.69f, 0.57f, 1, 0.11f, 0.9f, { 75, 67, 45, 84 }, 0.4f, 10 },
        { "Cross-Rhythm / 3-2 Weave", 5, 0.61f, 0.63f, 0, 0.09f, 0.95f, { 81, 72, 48, 88 }, 0.39f, 10 },
        { "Cross-Rhythm / Polymeter Surge", 5, 0.8f, 0.8f, 3, 0.15f, 0.84f, { 83, 74, 43, 89 }, 0.34f, 10 }
    };

    return presets;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AfricanRhythmsAudioProcessor();
}
