#include "Gp6PluginProcessor.h"
#include "Gp6PluginEditor.h"

namespace
{
const std::array<const char*, 51> parameterIDs {
    "preset",
    "patchProgram",
    "operatingMode",
    "repetitiveMode",
    "timeScale",
    "patchDensity",
    "feedback",
    "multiplier",
    "nonlinearity",
    "rotation",
    "swing",
    "probability",
    "overloadSense",
    "rootNote",
    "spread",
    "cable1Source",
    "cable1Destination",
    "cable1Amount",
    "cable2Source",
    "cable2Destination",
    "cable2Amount",
    "cable3Source",
    "cable3Destination",
    "cable3Amount",
    "cable4Source",
    "cable4Destination",
    "cable4Amount",
    "cable5Source",
    "cable5Destination",
    "cable5Amount",
    "cable6Source",
    "cable6Destination",
    "cable6Amount",
    "cable7Source",
    "cable7Destination",
    "cable7Amount",
    "cable8Source",
    "cable8Destination",
    "cable8Amount",
    "cable9Source",
    "cable9Destination",
    "cable9Amount",
    "cable10Source",
    "cable10Destination",
    "cable10Amount",
    "cable11Source",
    "cable11Destination",
    "cable11Amount",
    "cable12Source",
    "cable12Destination",
    "cable12Amount"
};
}

Gp6AudioProcessor::Gp6AudioProcessor()
    : AudioProcessor(BusesProperties()),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    for (const auto* id : parameterIDs)
        apvts.addParameterListener(id, this);

    refreshSnapshot();
}

Gp6AudioProcessor::~Gp6AudioProcessor()
{
    for (const auto* id : parameterIDs)
        apvts.removeParameterListener(id, this);
}

const juce::String Gp6AudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool Gp6AudioProcessor::acceptsMidi() const
{
    return true;
}

bool Gp6AudioProcessor::producesMidi() const
{
    return true;
}

bool Gp6AudioProcessor::isMidiEffect() const
{
    return true;
}

double Gp6AudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int Gp6AudioProcessor::getNumPrograms()
{
    return (int) getPresets().size();
}

int Gp6AudioProcessor::getCurrentProgram()
{
    return getRoundedParameter("preset");
}

void Gp6AudioProcessor::setCurrentProgram(int index)
{
    applyPreset(index);
}

const juce::String Gp6AudioProcessor::getProgramName(int index)
{
    const auto names = presetNames();
    return names[index];
}

void Gp6AudioProcessor::changeProgramName(int, const juce::String&)
{
}

void Gp6AudioProcessor::prepareToPlay(double sampleRate, int)
{
    currentSampleRate = sampleRate;
    previousBlockPpq = -1.0;
    pendingNoteOffs.clear();
    transportWasPlaying = false;
}

void Gp6AudioProcessor::releaseResources()
{
}

bool Gp6AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    juce::ignoreUnused(layouts);
    return true;
}

void Gp6AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    auto* playHead = getPlayHead();
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
    Gp6Snapshot localSnapshot;
    {
        const juce::SpinLock::ScopedLockType lock(snapshotLock);
        localSnapshot = snapshot;
    }

    const auto cycleLength = Gp6PatchEngine::cycleLengthQuarterNotes(localSnapshot.stepCount);
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
                const auto stepOffset = Gp6PatchEngine::stepStartQuarterNotes(step, localSnapshot.stepCount, settings.swing);
                const auto stepLength = Gp6PatchEngine::swungStepLengthQuarterNotes(step, localSnapshot.stepCount, settings.swing);
                const auto stepPpq = cycleStart + stepOffset;

                if (stepPpq < blockStartPpq || stepPpq >= blockEndPpq)
                    continue;

                const auto sampleOffset = juce::jlimit(0,
                                                       juce::jmax(0, buffer.getNumSamples() - 1),
                                                       (int) std::llround(quarterNotesToSamples(stepPpq - blockStartPpq, bpm)));

                for (auto laneIndex = 0; laneIndex < (int) localSnapshot.lanes.size(); ++laneIndex)
                {
                    if (! Gp6PatchEngine::shouldTriggerLaneStep(localSnapshot, laneIndex, step, cycle, settings.probability))
                        continue;

                    const auto& laneStep = localSnapshot.lanes[(size_t) laneIndex].steps[(size_t) step];
                    const auto velocity = Gp6PatchEngine::velocityForLaneStep(localSnapshot, laneIndex, step);
                    midiMessages.addEvent(juce::MidiMessage::noteOn(midiChannel, laneStep.note, velocity), sampleOffset);

                    const auto laneGate = gate * laneStep.gateBias;
                    const auto noteOffPpq = stepPpq + stepLength * laneGate;
                    pendingNoteOffs.push_back({ noteOffPpq, laneStep.note, midiChannel });
                }
            }
        }

        auto stepProgress = std::fmod(blockStartPpq, cycleLength);
        if (stepProgress < 0.0)
            stepProgress += cycleLength;

        auto activeStep = 0;
        for (auto step = 0; step < localSnapshot.stepCount; ++step)
        {
            const auto stepStart = Gp6PatchEngine::stepStartQuarterNotes(step, localSnapshot.stepCount, settings.swing);
            const auto nextStepStart = stepStart + Gp6PatchEngine::swungStepLengthQuarterNotes(step, localSnapshot.stepCount, settings.swing);
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

juce::AudioProcessorEditor* Gp6AudioProcessor::createEditor()
{
    return new Gp6AudioProcessorEditor(*this);
}

bool Gp6AudioProcessor::hasEditor() const
{
    return true;
}

void Gp6AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        std::unique_ptr<juce::XmlElement> xml(state.createXml());
        copyXmlToBinary(*xml, destData);
    }
}

void Gp6AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));

    refreshSnapshot();
}

Gp6Snapshot Gp6AudioProcessor::getSnapshot() const
{
    const juce::SpinLock::ScopedLockType lock(snapshotLock);
    return snapshot;
}

juce::String Gp6AudioProcessor::getCurrentPresetName() const
{
    return presetNames()[getRoundedParameter("preset")];
}

juce::AudioProcessorValueTreeState::ParameterLayout Gp6AudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;

    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("preset", "Preset", presetNames(), 0));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("patchProgram", "Patch Program", Gp6PatchEngine::patchPrograms(), 0));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("operatingMode", "Operating Mode", Gp6PatchEngine::operatingModes(), 1));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>("repetitiveMode", "Repetitive Mode", Gp6PatchEngine::repetitiveModes(), 1));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("timeScale", "Time Scale", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.54f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("patchDensity", "Patch Density", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.52f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("feedback", "Feedback", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.34f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("multiplier", "Multiplier", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.42f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("nonlinearity", "Nonlinearity", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.24f));
    parameters.push_back(std::make_unique<juce::AudioParameterInt>("rotation", "Rotation", -8, 8, 0));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("swing", "Swing", juce::NormalisableRange<float>(0.0f, 0.45f, 0.005f), 0.05f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("probability", "Probability", juce::NormalisableRange<float>(0.1f, 1.0f, 0.01f), 0.94f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("overloadSense", "Overload Sense", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.38f));
    parameters.push_back(std::make_unique<juce::AudioParameterInt>("rootNote", "Root Note", 24, 84, 48));
    parameters.push_back(std::make_unique<juce::AudioParameterInt>("spread", "Spread", 7, 36, 18));
    for (auto cableIndex = 0; cableIndex < 12; ++cableIndex)
    {
        const auto index = juce::String(cableIndex + 1);
        parameters.push_back(std::make_unique<juce::AudioParameterChoice>("cable" + index + "Source",
                                                                          "Cable " + index + " Source",
                                                                          Gp6PatchEngine::sourceNames(),
                                                                          cableIndex == 0 ? 0 : cableIndex == 1 ? 1 : cableIndex == 2 || cableIndex == 3 ? 2
                                                                                      : cableIndex == 4 ? 4 : cableIndex == 5 ? 3 : cableIndex == 6 ? 6
                                                                                      : cableIndex == 7 ? 7 : cableIndex == 8 ? 8 : cableIndex == 9 ? 9
                                                                                      : cableIndex == 10 ? 10 : 11));
        parameters.push_back(std::make_unique<juce::AudioParameterChoice>("cable" + index + "Destination",
                                                                          "Cable " + index + " Destination",
                                                                          Gp6PatchEngine::destinationNames(),
                                                                          cableIndex == 0 || cableIndex == 1 ? 2 : cableIndex == 2 ? 0 : cableIndex == 3 ? 1
                                                                                      : cableIndex == 4 ? 3 : cableIndex == 5 ? 6 : cableIndex == 6 || cableIndex == 7 ? 5
                                                                                      : cableIndex == 8 ? 3 : cableIndex == 9 ? 4 : cableIndex == 10 ? 7 : 4));
        parameters.push_back(std::make_unique<juce::AudioParameterFloat>("cable" + index + "Amount",
                                                                         "Cable " + index + " Amount",
                                                                         juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
                                                                         cableIndex == 0 ? 0.68f : cableIndex == 1 ? 0.54f : cableIndex == 2 ? 0.42f : cableIndex == 3 ? 0.38f
                                                                                      : cableIndex == 4 ? 0.5f : cableIndex == 5 ? 0.46f : cableIndex == 6 ? 0.62f
                                                                                      : cableIndex == 7 ? 0.48f : cableIndex == 8 ? 0.33f : cableIndex == 9 ? 0.58f
                                                                                      : cableIndex == 10 ? 0.37f : 0.29f));
    }
    parameters.push_back(std::make_unique<juce::AudioParameterInt>("channel", "Channel", 1, 16, 2));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>("gate", "Gate", juce::NormalisableRange<float>(0.08f, 0.95f, 0.01f), 0.41f));

    return { parameters.begin(), parameters.end() };
}

juce::StringArray Gp6AudioProcessor::presetNames()
{
    juce::StringArray names;
    for (const auto& preset : getPresets())
        names.add(preset.name);
    return names;
}

Gp6PatchEngine::Settings Gp6AudioProcessor::currentSettings() const
{
    Gp6PatchEngine::Settings settings;
    settings.patchProgram = getRoundedParameter("patchProgram");
    settings.operatingMode = getRoundedParameter("operatingMode");
    settings.repetitiveMode = getRoundedParameter("repetitiveMode");
    settings.timeScale = getFloatParameter("timeScale");
    settings.patchDensity = getFloatParameter("patchDensity");
    settings.feedback = getFloatParameter("feedback");
    settings.multiplier = getFloatParameter("multiplier");
    settings.nonlinearity = getFloatParameter("nonlinearity");
    settings.rotation = getRoundedParameter("rotation");
    settings.swing = getFloatParameter("swing");
    settings.probability = getFloatParameter("probability");
    settings.overloadSense = getFloatParameter("overloadSense");
    settings.rootNote = getRoundedParameter("rootNote");
    settings.spread = getRoundedParameter("spread");
    for (auto cableIndex = 0; cableIndex < 12; ++cableIndex)
    {
        const auto index = juce::String(cableIndex + 1);
        settings.cableSources[(size_t) cableIndex] = getRoundedParameter("cable" + index + "Source");
        settings.cableDestinations[(size_t) cableIndex] = getRoundedParameter("cable" + index + "Destination");
        settings.cableAmounts[(size_t) cableIndex] = getFloatParameter("cable" + index + "Amount");
    }
    return settings;
}

void Gp6AudioProcessor::refreshSnapshot()
{
    const auto latest = Gp6PatchEngine::generate(currentSettings(), getCurrentPresetName());
    const juce::SpinLock::ScopedLockType lock(snapshotLock);
    snapshot = latest;
}

void Gp6AudioProcessor::addPendingNoteOffs(juce::MidiBuffer& midi,
                                           double blockStartPpq,
                                           double blockEndPpq,
                                           double bpm,
                                           int blockSize)
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

void Gp6AudioProcessor::sendAllNotesOff(juce::MidiBuffer& midi)
{
    for (auto channel = 1; channel <= 16; ++channel)
        midi.addEvent(juce::MidiMessage::allNotesOff(channel), 0);
}

double Gp6AudioProcessor::quarterNotesToSamples(double quarterNotes, double bpm) const
{
    if (bpm <= 0.0)
        return 0.0;

    return quarterNotes * 60.0 * currentSampleRate / bpm;
}

int Gp6AudioProcessor::getRoundedParameter(const juce::String& id) const
{
    if (auto* parameter = apvts.getRawParameterValue(id))
        return juce::roundToInt(parameter->load());

    return 0;
}

float Gp6AudioProcessor::getFloatParameter(const juce::String& id) const
{
    if (auto* parameter = apvts.getRawParameterValue(id))
        return parameter->load();

    return 0.0f;
}

void Gp6AudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
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

void Gp6AudioProcessor::applyPreset(int presetIndex)
{
    const auto& presets = getPresets();
    const auto index = juce::jlimit(0, (int) presets.size() - 1, presetIndex);
    const auto& preset = presets[(size_t) index];

    isApplyingPreset.store(true);
    setParameterValue("preset", (float) index);
    setParameterValue("patchProgram", (float) preset.patchProgram);
    setParameterValue("operatingMode", (float) preset.operatingMode);
    setParameterValue("repetitiveMode", (float) preset.repetitiveMode);
    setParameterValue("timeScale", preset.timeScale);
    setParameterValue("patchDensity", preset.patchDensity);
    setParameterValue("feedback", preset.feedback);
    setParameterValue("multiplier", preset.multiplier);
    setParameterValue("nonlinearity", preset.nonlinearity);
    setParameterValue("rotation", (float) preset.rotation);
    setParameterValue("swing", preset.swing);
    setParameterValue("probability", preset.probability);
    setParameterValue("overloadSense", preset.overloadSense);
    setParameterValue("rootNote", (float) preset.rootNote);
    setParameterValue("spread", (float) preset.spread);
    for (auto cableIndex = 0; cableIndex < 12; ++cableIndex)
    {
        const auto indexLabel = juce::String(cableIndex + 1);
        setParameterValue("cable" + indexLabel + "Source", (float) preset.cableSources[(size_t) cableIndex]);
        setParameterValue("cable" + indexLabel + "Destination", (float) preset.cableDestinations[(size_t) cableIndex]);
        setParameterValue("cable" + indexLabel + "Amount", preset.cableAmounts[(size_t) cableIndex]);
    }
    setParameterValue("gate", preset.gate);
    setParameterValue("channel", (float) preset.channel);
    isApplyingPreset.store(false);

    refreshSnapshot();
}

void Gp6AudioProcessor::setParameterValue(const juce::String& id, float plainValue)
{
    if (auto* parameter = apvts.getParameter(id))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

const std::vector<Gp6AudioProcessor::PresetDefinition>& Gp6AudioProcessor::getPresets()
{
    static const std::vector<PresetDefinition> presets {
        { "Ballistic Clock", 0, 1, 1, 0.46f, 0.58f, 0.33f, 0.43f, 0.19f, 0, 0.04f, 0.97f, 0.24f, 48, 18,
            { 0, 1, 2, 4, 3, 6, 7, 8, 9, 10, 11, 5 },
            { 2, 2, 0, 3, 6, 5, 4, 1, 4, 7, 0, 6 },
            { 0.82f, 0.63f, 0.41f, 0.52f, 0.37f, 0.66f, 0.46f, 0.31f, 0.55f, 0.48f, 0.22f, 0.34f }, 0.41f, 2 },
        { "Orbit Relay", 1, 2, 1, 0.64f, 0.62f, 0.51f, 0.36f, 0.31f, 1, 0.08f, 0.93f, 0.36f, 45, 20,
            { 1, 2, 0, 4, 3, 6, 8, 7, 9, 10, 11, 5 },
            { 2, 0, 1, 3, 6, 5, 3, 4, 5, 7, 0, 6 },
            { 0.61f, 0.77f, 0.54f, 0.44f, 0.58f, 0.62f, 0.39f, 0.49f, 0.68f, 0.43f, 0.28f, 0.47f }, 0.37f, 2 },
        { "Servo Lock", 2, 1, 0, 0.33f, 0.45f, 0.24f, 0.28f, 0.13f, -2, 0.02f, 0.99f, 0.19f, 52, 14,
            { 2, 0, 1, 4, 3, 6, 7, 8, 10, 11, 9, 5 },
            { 0, 2, 2, 3, 6, 5, 4, 1, 7, 0, 3, 6 },
            { 0.88f, 0.36f, 0.57f, 0.22f, 0.31f, 0.65f, 0.41f, 0.24f, 0.38f, 0.18f, 0.29f, 0.26f }, 0.48f, 2 },
        { "Predator Spiral", 3, 2, 2, 0.59f, 0.74f, 0.58f, 0.53f, 0.39f, 2, 0.11f, 0.86f, 0.62f, 43, 24,
            { 0, 1, 2, 4, 3, 6, 7, 9, 8, 10, 11, 5 },
            { 1, 2, 0, 3, 6, 5, 4, 5, 3, 7, 0, 6 },
            { 0.66f, 0.85f, 0.61f, 0.71f, 0.63f, 0.79f, 0.57f, 0.74f, 0.42f, 0.52f, 0.31f, 0.48f }, 0.28f, 2 },
        { "Ring Balance", 4, 0, 1, 0.53f, 0.5f, 0.37f, 0.57f, 0.2f, 0, 0.05f, 0.95f, 0.29f, 50, 18,
            { 0, 1, 2, 4, 3, 6, 7, 8, 9, 10, 11, 5 },
            { 2, 2, 1, 3, 6, 5, 4, 0, 3, 7, 0, 6 },
            { 0.73f, 0.69f, 0.36f, 0.59f, 0.52f, 0.61f, 0.49f, 0.28f, 0.44f, 0.41f, 0.24f, 0.33f }, 0.4f, 2 },
        { "Multiplier Lab", 5, 1, 2, 0.69f, 0.77f, 0.52f, 0.82f, 0.44f, -1, 0.09f, 0.82f, 0.71f, 40, 28,
            { 4, 9, 0, 1, 2, 3, 5, 6, 7, 8, 10, 11 },
            { 3, 4, 2, 2, 0, 6, 1, 5, 5, 3, 7, 4 },
            { 0.94f, 0.89f, 0.48f, 0.43f, 0.56f, 0.97f, 0.74f, 0.62f, 0.53f, 0.41f, 0.58f, 0.35f }, 0.23f, 2 },
        { "IC Memory Run", 0, 0, 1, 0.4f, 0.48f, 0.55f, 0.24f, 0.09f, 3, 0.03f, 0.98f, 0.15f, 55, 12,
            { 2, 2, 0, 1, 4, 3, 6, 7, 8, 10, 11, 5 },
            { 0, 1, 2, 2, 3, 6, 5, 4, 1, 7, 0, 6 },
            { 0.91f, 0.27f, 0.78f, 0.23f, 0.18f, 0.39f, 0.64f, 0.33f, 0.26f, 0.36f, 0.17f, 0.29f }, 0.55f, 2 },
        { "Overload Chorus", 5, 2, 2, 0.73f, 0.84f, 0.69f, 0.88f, 0.55f, -3, 0.13f, 0.77f, 0.86f, 36, 31,
            { 5, 4, 2, 3, 0, 1, 10, 9, 8, 6, 7, 11 },
            { 7, 3, 0, 1, 2, 2, 4, 5, 3, 5, 4, 6 },
            { 0.63f, 0.91f, 0.74f, 0.79f, 1.0f, 0.88f, 0.71f, 0.83f, 0.61f, 0.57f, 0.68f, 0.52f }, 0.19f, 2 }
    };

    return presets;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Gp6AudioProcessor();
}
