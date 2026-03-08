#include "ErhcetuaPluginProcessor.h"
#include "ErhcetuaPluginEditor.h"

namespace
{
const std::array<const char*, 19> parameterIDs {
    "preset",
    "grammar",
    "resetMode",
    "ruleSet",
    "scale",
    "density",
    "mutation",
    "ratchet",
    "flutter",
    "rotation",
    "swing",
    "gateSkew",
    "pitchSpread",
    "probability",
    "drift",
    "rootNote",
    "octaveSpan",
    "channel",
    "gate"
};
}

ErhcetuaAudioProcessor::ErhcetuaAudioProcessor()
    : AudioProcessor(BusesProperties()),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    for (const auto* id : parameterIDs)
        apvts.addParameterListener(id, this);
    refreshSnapshot();
}

ErhcetuaAudioProcessor::~ErhcetuaAudioProcessor()
{
    for (const auto* id : parameterIDs)
        apvts.removeParameterListener(id, this);
}

const juce::String ErhcetuaAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ErhcetuaAudioProcessor::acceptsMidi() const
{
    return true;
}

bool ErhcetuaAudioProcessor::producesMidi() const
{
    return true;
}

bool ErhcetuaAudioProcessor::isMidiEffect() const
{
    return true;
}

double ErhcetuaAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int ErhcetuaAudioProcessor::getNumPrograms()
{
    return (int) getPresets().size();
}

int ErhcetuaAudioProcessor::getCurrentProgram()
{
    return getRoundedParameter("preset");
}

void ErhcetuaAudioProcessor::setCurrentProgram(int index)
{
    applyPreset(index);
}

const juce::String ErhcetuaAudioProcessor::getProgramName(int index)
{
    const auto names = presetNames();
    return names[index];
}

void ErhcetuaAudioProcessor::changeProgramName(int, const juce::String&)
{
}

void ErhcetuaAudioProcessor::prepareToPlay(double sampleRate, int)
{
    currentSampleRate = sampleRate;
    previousBlockPpq = -1.0;
    pendingNoteOffs.clear();
    transportWasPlaying = false;
}

void ErhcetuaAudioProcessor::releaseResources()
{
}

bool ErhcetuaAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    juce::ignoreUnused(layouts);
    return true;
}

void ErhcetuaAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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
    ErhcetuaSnapshot localSnapshot;
    {
        const juce::SpinLock::ScopedLockType lock(snapshotLock);
        localSnapshot = snapshot;
    }

    const auto cycleLength = ErhcetuaRhythmEngine::cycleLengthQuarterNotes(localSnapshot.stepCount);
    if (cycleLength > 0.0)
    {
        const auto firstCycle = (int) std::floor(blockStartPpq / cycleLength);
        const auto lastCycle = (int) std::floor(blockEndPpq / cycleLength);
        const auto midiChannel = juce::jlimit(1, 16, getRoundedParameter("channel"));
        const auto gate = juce::jlimit(0.08f, 0.95f, getFloatParameter("gate"));
        const auto gateSkew = juce::jlimit(0.0f, 1.0f, getFloatParameter("gateSkew"));

        for (auto cycle = firstCycle; cycle <= lastCycle; ++cycle)
        {
            const auto cycleStart = cycle * cycleLength;

            for (auto step = 0; step < localSnapshot.stepCount; ++step)
            {
                const auto stepOffset = ErhcetuaRhythmEngine::stepStartQuarterNotes(step, localSnapshot.stepCount, settings.swing);
                const auto stepLength = ErhcetuaRhythmEngine::swungStepLengthQuarterNotes(step, localSnapshot.stepCount, settings.swing);
                const auto stepPpq = cycleStart + stepOffset;

                for (auto laneIndex = 0; laneIndex < (int) localSnapshot.lanes.size(); ++laneIndex)
                {
                    if (! ErhcetuaRhythmEngine::shouldTriggerLaneStep(localSnapshot, laneIndex, step, cycle, settings.probability))
                        continue;

                    const auto& laneStep = localSnapshot.lanes[(size_t) laneIndex].steps[(size_t) step];
                    const auto subOffsets = ErhcetuaRhythmEngine::subStepOffsets(laneStep, stepLength);
                    const auto velocity = ErhcetuaRhythmEngine::velocityForLaneStep(localSnapshot, laneIndex, step);
                    const auto repeatGate = gate * juce::jmap(laneStep.gateMod,
                                                              0.0f,
                                                              1.0f,
                                                              0.56f - gateSkew * 0.12f,
                                                              1.02f - gateSkew * 0.44f);

                    for (auto repeatIndex = 0; repeatIndex < (int) subOffsets.size(); ++repeatIndex)
                    {
                        const auto driftPpq = laneStep.drift * (stepLength / 4.0);
                        const auto onsetPpq = stepPpq + subOffsets[(size_t) repeatIndex] + driftPpq;
                        if (onsetPpq < blockStartPpq || onsetPpq >= blockEndPpq)
                            continue;

                        const auto sampleOffset = juce::jlimit(0,
                                                               juce::jmax(0, buffer.getNumSamples() - 1),
                                                               (int) std::llround(quarterNotesToSamples(onsetPpq - blockStartPpq, bpm)));

                        midiMessages.addEvent(juce::MidiMessage::noteOn(midiChannel, laneStep.note, velocity), sampleOffset);

                        const auto repeatLength = laneStep.repeats > 1
                                                    ? stepLength * (laneStep.flutter ? 0.42 : 0.72) / static_cast<double>(laneStep.repeats)
                                                    : stepLength;
                        const auto noteOffPpq = onsetPpq + repeatLength * repeatGate;
                        pendingNoteOffs.push_back({ noteOffPpq, laneStep.note, midiChannel });
                    }
                }
            }
        }

        auto stepProgress = std::fmod(blockStartPpq, cycleLength);
        if (stepProgress < 0.0)
            stepProgress += cycleLength;

        auto activeStep = 0;
        for (auto step = 0; step < localSnapshot.stepCount; ++step)
        {
            const auto stepStart = ErhcetuaRhythmEngine::stepStartQuarterNotes(step, localSnapshot.stepCount, settings.swing);
            const auto nextStepStart = stepStart + ErhcetuaRhythmEngine::swungStepLengthQuarterNotes(step, localSnapshot.stepCount, settings.swing);

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

juce::AudioProcessorEditor* ErhcetuaAudioProcessor::createEditor()
{
    return new ErhcetuaAudioProcessorEditor(*this);
}

bool ErhcetuaAudioProcessor::hasEditor() const
{
    return true;
}

void ErhcetuaAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        std::unique_ptr<juce::XmlElement> xml(state.createXml());
        copyXmlToBinary(*xml, destData);
    }
}

void ErhcetuaAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));

    refreshSnapshot();
}

ErhcetuaSnapshot ErhcetuaAudioProcessor::getSnapshot() const
{
    const juce::SpinLock::ScopedLockType lock(snapshotLock);
    return snapshot;
}

juce::String ErhcetuaAudioProcessor::getCurrentPresetName() const
{
    return presetNames()[getRoundedParameter("preset")];
}

juce::AudioProcessorValueTreeState::ParameterLayout ErhcetuaAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;

    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("preset", "Preset", presetNames(), 0));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("grammar", "Grammar", ErhcetuaRhythmEngine::grammars(), 0));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("resetMode", "Reset Mode", ErhcetuaRhythmEngine::resetModes(), 0));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("ruleSet", "Rule Set", ErhcetuaRhythmEngine::ruleSets(), 0));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("scale", "Scale", ErhcetuaRhythmEngine::scales(), 1));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("density", "Density", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.52f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("mutation", "Mutation", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.48f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("ratchet", "Ratchet", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.22f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("flutter", "Flutter", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.18f));
    parameters.push_back(std::make_unique<juce::AudioParameterInt>("rotation", "Rotation", -8, 8, 0));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("swing", "Swing", juce::NormalisableRange<float>(0.0f, 0.45f, 0.005f), 0.06f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("gateSkew", "Gate Skew", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.38f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("pitchSpread", "Pitch Spread", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.45f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("probability", "Probability", juce::NormalisableRange<float>(0.1f, 1.0f, 0.01f), 0.92f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("drift", "Drift", juce::NormalisableRange<float>(0.0f, 0.35f, 0.005f), 0.08f));
    parameters.push_back(std::make_unique<juce::AudioParameterInt>("rootNote", "Root Note", 24, 84, 48));
    parameters.push_back(std::make_unique<juce::AudioParameterInt>("octaveSpan", "Octave Span", 1, 4, 2));
    parameters.push_back(std::make_unique<juce::AudioParameterInt>("channel", "Channel", 1, 16, 1));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("gate", "Gate", juce::NormalisableRange<float>(0.08f, 0.95f, 0.01f), 0.42f));

    return { parameters.begin(), parameters.end() };
}

juce::StringArray ErhcetuaAudioProcessor::presetNames()
{
    juce::StringArray names;
    for (const auto& preset : getPresets())
        names.add(preset.name);
    return names;
}

ErhcetuaRhythmEngine::Settings ErhcetuaAudioProcessor::currentSettings() const
{
    ErhcetuaRhythmEngine::Settings settings;
    settings.grammar = getRoundedParameter("grammar");
    settings.resetMode = getRoundedParameter("resetMode");
    settings.ruleSet = getRoundedParameter("ruleSet");
    settings.scale = getRoundedParameter("scale");
    settings.density = getFloatParameter("density");
    settings.mutation = getFloatParameter("mutation");
    settings.ratchet = getFloatParameter("ratchet");
    settings.flutter = getFloatParameter("flutter");
    settings.rotation = getRoundedParameter("rotation");
    settings.swing = getFloatParameter("swing");
    settings.gateSkew = getFloatParameter("gateSkew");
    settings.pitchSpread = getFloatParameter("pitchSpread");
    settings.probability = getFloatParameter("probability");
    settings.drift = getFloatParameter("drift");
    settings.rootNote = getRoundedParameter("rootNote");
    settings.octaveSpan = getRoundedParameter("octaveSpan");
    return settings;
}

void ErhcetuaAudioProcessor::refreshSnapshot()
{
    const auto latest = ErhcetuaRhythmEngine::generate(currentSettings(), getCurrentPresetName());
    const juce::SpinLock::ScopedLockType lock(snapshotLock);
    snapshot = latest;
}

void ErhcetuaAudioProcessor::addPendingNoteOffs(juce::MidiBuffer& midi, double blockStartPpq, double blockEndPpq, double bpm, int blockSize)
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

void ErhcetuaAudioProcessor::sendAllNotesOff(juce::MidiBuffer& midi)
{
    for (auto channel = 1; channel <= 16; ++channel)
        midi.addEvent(juce::MidiMessage::allNotesOff(channel), 0);
}

double ErhcetuaAudioProcessor::quarterNotesToSamples(double quarterNotes, double bpm) const
{
    if (bpm <= 0.0)
        return 0.0;

    return quarterNotes * 60.0 * currentSampleRate / bpm;
}

int ErhcetuaAudioProcessor::getRoundedParameter(const juce::String& id) const
{
    if (auto* parameter = apvts.getRawParameterValue(id))
        return juce::roundToInt(parameter->load());

    return 0;
}

float ErhcetuaAudioProcessor::getFloatParameter(const juce::String& id) const
{
    if (auto* parameter = apvts.getRawParameterValue(id))
        return parameter->load();

    return 0.0f;
}

void ErhcetuaAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (isApplyingPreset.load())
        return;

    if (parameterID == "preset")
    {
        applyPreset(juce::roundToInt(newValue));
        return;
    }

    refreshSnapshot();
}

void ErhcetuaAudioProcessor::applyPreset(int presetIndex)
{
    const auto& presets = getPresets();
    const auto index = juce::jlimit(0, (int) presets.size() - 1, presetIndex);
    const auto& preset = presets[(size_t) index];

    isApplyingPreset.store(true);
    setParameterValue("preset", (float) index);
    setParameterValue("grammar", (float) preset.grammar);
    setParameterValue("resetMode", (float) preset.resetMode);
    setParameterValue("ruleSet", (float) preset.ruleSet);
    setParameterValue("scale", (float) preset.scale);
    setParameterValue("density", preset.density);
    setParameterValue("mutation", preset.mutation);
    setParameterValue("ratchet", preset.ratchet);
    setParameterValue("flutter", preset.flutter);
    setParameterValue("rotation", (float) preset.rotation);
    setParameterValue("swing", preset.swing);
    setParameterValue("gateSkew", preset.gateSkew);
    setParameterValue("pitchSpread", preset.pitchSpread);
    setParameterValue("probability", preset.probability);
    setParameterValue("drift", preset.drift);
    setParameterValue("rootNote", (float) preset.rootNote);
    setParameterValue("octaveSpan", (float) preset.octaveSpan);
    setParameterValue("gate", preset.gate);
    setParameterValue("channel", (float) preset.channel);
    isApplyingPreset.store(false);

    refreshSnapshot();
}

void ErhcetuaAudioProcessor::setParameterValue(const juce::String& id, float plainValue)
{
    if (auto* parameter = apvts.getParameter(id))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

const std::vector<ErhcetuaAudioProcessor::PresetDefinition>& ErhcetuaAudioProcessor::getPresets()
{
    static const std::vector<PresetDefinition> presets {
        { "Draun Quarter", 0, 0, 0, 1, 0.48f, 0.54f, 0.22f, 0.11f, 1, 0.07f, 0.36f, 0.44f, 0.94f, 0.05f, 45, 2, 0.42f, 1 },
        { "Slip Grid Manual", 1, 0, 0, 0, 0.37f, 0.33f, 0.12f, 0.04f, -1, 0.03f, 0.28f, 0.26f, 0.98f, 0.01f, 48, 1, 0.54f, 1 },
        { "Oversteps Fold", 3, 2, 1, 4, 0.61f, 0.66f, 0.31f, 0.19f, -2, 0.09f, 0.48f, 0.58f, 0.88f, 0.09f, 43, 3, 0.36f, 1 },
        { "Amber Mask", 2, 1, 3, 2, 0.55f, 0.72f, 0.43f, 0.27f, 0, 0.05f, 0.52f, 0.64f, 0.84f, 0.12f, 50, 2, 0.31f, 1 },
        { "Null Staircase", 1, 0, 2, 1, 0.42f, 0.49f, 0.18f, 0.06f, 3, 0.12f, 0.30f, 0.36f, 0.97f, 0.03f, 57, 2, 0.48f, 1 },
        { "Signal Cartography", 4, 3, 4, 3, 0.69f, 0.63f, 0.38f, 0.34f, -1, 0.10f, 0.61f, 0.71f, 0.82f, 0.16f, 40, 4, 0.29f, 1 },
        { "Flutter Study", 2, 3, 1, 4, 0.58f, 0.77f, 0.44f, 0.86f, 1, 0.04f, 0.57f, 0.69f, 0.83f, 0.18f, 52, 2, 0.24f, 1 },
        { "Probable Alloy", 4, 1, 2, 3, 0.63f, 0.58f, 0.26f, 0.41f, -3, 0.08f, 0.55f, 0.62f, 0.73f, 0.14f, 38, 4, 0.33f, 1 },
        { "Mirror Static", 0, 2, 4, 2, 0.51f, 0.61f, 0.29f, 0.17f, 2, 0.06f, 0.40f, 0.48f, 0.91f, 0.08f, 47, 2, 0.44f, 1 },
        { "Whole Tone Engine", 3, 0, 3, 3, 0.57f, 0.74f, 0.34f, 0.23f, 0, 0.11f, 0.46f, 0.83f, 0.79f, 0.10f, 42, 3, 0.35f, 1 },
        { "Accent Trigger Bloom", 2, 3, 1, 1, 0.66f, 0.69f, 0.41f, 0.57f, 1, 0.05f, 0.51f, 0.55f, 0.86f, 0.13f, 49, 2, 0.27f, 1 },
        { "Barline Pressure", 4, 2, 3, 4, 0.72f, 0.52f, 0.36f, 0.22f, -2, 0.09f, 0.43f, 0.67f, 0.89f, 0.11f, 36, 4, 0.30f, 1 },
        { "Phrygian Scatter", 1, 1, 0, 2, 0.46f, 0.81f, 0.24f, 0.38f, 4, 0.02f, 0.58f, 0.49f, 0.76f, 0.17f, 53, 2, 0.26f, 1 },
        { "Dry Machine Choir", 0, 0, 2, 0, 0.34f, 0.27f, 0.09f, 0.03f, 0, 0.01f, 0.18f, 0.22f, 0.99f, 0.00f, 60, 1, 0.62f, 1 },
        { "Octatonic Relay", 3, 1, 4, 4, 0.59f, 0.64f, 0.32f, 0.29f, 2, 0.07f, 0.47f, 0.73f, 0.81f, 0.15f, 44, 3, 0.32f, 1 },
        { "Airless Reset Maze", 2, 3, 2, 1, 0.54f, 0.83f, 0.47f, 0.71f, -4, 0.03f, 0.63f, 0.52f, 0.68f, 0.20f, 46, 2, 0.21f, 1 }
    };

    return presets;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ErhcetuaAudioProcessor();
}
