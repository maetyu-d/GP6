#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

struct ErhcetuaStepSnapshot
{
    bool active = false;
    bool accent = false;
    int note = 60;
    int repeats = 1;
    bool flutter = false;
    float intensity = 0.0f;
};

struct ErhcetuaLaneEventSnapshot
{
    bool active = false;
    bool accent = false;
    bool flutter = false;
    int note = 60;
    int repeats = 1;
    float intensity = 0.0f;
    float gateMod = 0.5f;
    float drift = 0.0f;
};

struct ErhcetuaLaneSnapshot
{
    juce::String name;
    std::vector<ErhcetuaLaneEventSnapshot> steps;
};

struct ErhcetuaModRowSnapshot
{
    juce::String name;
    std::vector<float> values;
};

struct ErhcetuaStageSnapshot
{
    juce::String name;
    juce::String detail;
    float amount = 0.0f;
};

struct ErhcetuaSnapshot
{
    juce::String grammarName;
    juce::String presetName;
    juce::String resetModeName;
    juce::String ruleName;
    juce::String scaleName;
    juce::String algorithmSummary;
    std::vector<ErhcetuaStepSnapshot> steps;
    std::vector<ErhcetuaLaneSnapshot> lanes;
    std::vector<ErhcetuaModRowSnapshot> modulationRows;
    std::vector<ErhcetuaStageSnapshot> stages;
    std::vector<bool> resetMarkers;
    int stepCount = 0;
    int currentStep = -1;
    float swing = 0.0f;
    float mutation = 0.0f;
    float flutter = 0.0f;
    float drift = 0.0f;
};

class ErhcetuaRhythmEngine
{
public:
    struct Settings
    {
        int grammar = 0;
        int resetMode = 0;
        int ruleSet = 0;
        int scale = 0;
        float density = 0.52f;
        float mutation = 0.48f;
        float ratchet = 0.22f;
        float flutter = 0.0f;
        int rotation = 0;
        float swing = 0.06f;
        float gateSkew = 0.38f;
        float pitchSpread = 0.45f;
        float probability = 0.92f;
        float drift = 0.08f;
        int rootNote = 48;
        int octaveSpan = 2;
    };

    static juce::StringArray grammars()
    {
        return {
            "Lattice Drift",
            "Slipstream",
            "Xor Bloom",
            "Recursive Fold",
            "Signal Haze"
        };
    }

    static juce::StringArray resetModes()
    {
        return {
            "Fixed Window",
            "Probabilistic",
            "Bar Edge",
            "Accent Trigger"
        };
    }

    static juce::StringArray ruleSets()
    {
        return {
            "None",
            "Post-Reset Accent",
            "Suppress After Burst",
            "Third Reset Transpose",
            "Mirror Recovery"
        };
    }

    static juce::StringArray scales()
    {
        return {
            "Chromatic",
            "Minor",
            "Phrygian",
            "Whole Tone",
            "Octatonic"
        };
    }

    static ErhcetuaSnapshot generate(const Settings& settings, const juce::String& presetName = {})
    {
        const auto spec = getGrammarSpec(settings.grammar);
        const auto scaleDef = getScaleSpec(settings.scale);

        ErhcetuaSnapshot snapshot;
        snapshot.grammarName = spec.name;
        snapshot.presetName = presetName;
        snapshot.resetModeName = resetModes()[juce::jlimit(0, resetModes().size() - 1, settings.resetMode)];
        snapshot.ruleName = ruleSets()[juce::jlimit(0, ruleSets().size() - 1, settings.ruleSet)];
        snapshot.scaleName = scaleDef.name;
        snapshot.algorithmSummary = "seed -> reset -> mask -> rules -> lanes";
        snapshot.stepCount = spec.steps;
        snapshot.swing = settings.swing;
        snapshot.mutation = settings.mutation;
        snapshot.flutter = settings.flutter;
        snapshot.drift = settings.drift;
        snapshot.steps.resize((size_t) spec.steps);
        snapshot.resetMarkers.assign((size_t) spec.steps, false);
        snapshot.lanes = {
            { "Anchor", std::vector<ErhcetuaLaneEventSnapshot>((size_t) spec.steps) },
            { "Metal", std::vector<ErhcetuaLaneEventSnapshot>((size_t) spec.steps) },
            { "Air", std::vector<ErhcetuaLaneEventSnapshot>((size_t) spec.steps) }
        };
        snapshot.modulationRows = {
            { "Vel", std::vector<float>((size_t) spec.steps, 0.0f) },
            { "Gate", std::vector<float>((size_t) spec.steps, 0.0f) },
            { "Oct", std::vector<float>((size_t) spec.steps, 0.0f) },
            { "Rst", std::vector<float>((size_t) spec.steps, 0.0f) }
        };

        const auto pulseCount = juce::jlimit(2,
                                             spec.steps - 2,
                                             juce::roundToInt((0.16f + settings.density * 0.54f) * (float) spec.steps));
        const auto seedPattern = euclidean(spec.steps, pulseCount);
        const auto rotation = wrap(settings.rotation, spec.steps);
        const auto octaveSpan = juce::jlimit(1, 4, settings.octaveSpan);
        const auto baseResetSpan = juce::jlimit(3,
                                                spec.steps,
                                                juce::roundToInt(juce::jmap(settings.flutter, 0.0f, 1.0f, (float) spec.steps, (float) juce::jmax(3, spec.steps / 4))));

        auto resetCounter = 0;
        auto stepsSinceReset = spec.steps;
        auto previousAggregateAccent = false;
        auto previousAggregateFlutter = false;

        for (auto step = 0; step < spec.steps; ++step)
        {
            const auto sourceStep = wrap(step - rotation, spec.steps);
            const auto contour = rhythmicContour(sourceStep, spec.steps, settings.mutation, spec.contourWarp);
            const auto stepWithinBar = wrap(step, juce::jmax(1, spec.steps / 4));
            const auto resetProbability = juce::jlimit(0.0f,
                                                       1.0f,
                                                       0.08f + settings.flutter * 0.62f + contour * 0.2f
                                                       + (settings.resetMode == 1 ? 0.16f : 0.0f));

            auto resetNow = false;
            switch (settings.resetMode)
            {
                case 0:
                    resetNow = step == 0 || stepsSinceReset >= baseResetSpan;
                    break;
                case 1:
                    resetNow = step == 0 || (stepsSinceReset >= 2
                                             && deterministic01(spec.xorSeed + step * 5, sourceStep + 71) < resetProbability);
                    break;
                case 2:
                    resetNow = step == 0 || stepWithinBar == 0 || stepsSinceReset >= baseResetSpan + 1;
                    break;
                case 3:
                    resetNow = step == 0 || previousAggregateAccent || (previousAggregateFlutter && stepsSinceReset >= 2);
                    break;
                default:
                    break;
            }

            if (resetNow)
            {
                stepsSinceReset = 0;
                ++resetCounter;
                snapshot.resetMarkers[(size_t) step] = true;
            }

            const auto reseededStep = wrap(stepsSinceReset + resetCounter * (spec.seedOffset + 1), spec.steps);
            const auto grayCode = reseededStep ^ (reseededStep >> 1);
            const auto resetSeed = spec.xorSeed + resetCounter * 11 + juce::roundToInt(settings.flutter * 23.0f);
            const auto xorBitCount = juce::jlimit(0, 8, countBits((uint32_t) (grayCode ^ resetSeed)));
            const auto maskGate = ((grayCode + spec.maskOffset + resetCounter) & spec.mask) < spec.threshold;
            const auto seedGate = seedPattern[(size_t) wrap(reseededStep + spec.seedOffset + resetCounter, spec.steps)];
            const auto clockGate = ((stepsSinceReset + spec.clockOffset) % spec.clockDiv) == 0;
            const auto mutationGate = maskGate && contour > (0.34f - settings.mutation * 0.12f);

            const auto velocityMod = juce::jlimit(0.0f, 1.0f, 0.18f + contour * 0.7f + (resetNow ? 0.12f : 0.0f));
            const auto gateMod = juce::jlimit(0.0f, 1.0f, 0.22f + (1.0f - settings.gateSkew) * 0.52f + contour * 0.16f - (resetNow ? 0.08f : 0.0f));
            const auto octaveMod = juce::jlimit(0.0f, 1.0f, deterministic01(resetSeed + 19, reseededStep + 33) * (0.35f + settings.pitchSpread * 0.65f));

            snapshot.modulationRows[0].values[(size_t) step] = velocityMod;
            snapshot.modulationRows[1].values[(size_t) step] = gateMod;
            snapshot.modulationRows[2].values[(size_t) step] = octaveMod;
            snapshot.modulationRows[3].values[(size_t) step] = resetProbability;

            auto laneActiveFlags = std::array<bool, 3> {
                seedGate || resetNow,
                mutationGate || (clockGate && settings.density > 0.34f),
                maskGate || (settings.flutter > 0.2f && contour > 0.44f)
            };

            auto laneAccents = std::array<bool, 3> {
                resetNow || clockGate,
                xorBitCount >= juce::jmax(2, spec.accentBitCount - 1),
                contour > 0.72f
            };

            auto laneFlutters = std::array<bool, 3> {
                false,
                false,
                false
            };

            auto laneRepeats = std::array<int, 3> { 1, 1, 1 };
            auto laneIntensities = std::array<float, 3> {
                juce::jlimit(0.0f, 1.0f, 0.36f + velocityMod * 0.44f + (resetNow ? 0.12f : 0.0f)),
                juce::jlimit(0.0f, 1.0f, 0.24f + velocityMod * 0.52f),
                juce::jlimit(0.0f, 1.0f, 0.18f + velocityMod * 0.6f)
            };

            for (auto laneIndex = 0; laneIndex < 3; ++laneIndex)
            {
                if (! laneActiveFlags[(size_t) laneIndex])
                    continue;

                const auto repeatBias = deterministic01(resetSeed + 9 + laneIndex * 13, reseededStep + laneIndex * 37);
                laneRepeats[(size_t) laneIndex] = 1 + juce::roundToInt(settings.ratchet * (2.0f + (float) laneIndex) * laneIntensities[(size_t) laneIndex]
                                                                       + settings.mutation * repeatBias);
                laneRepeats[(size_t) laneIndex] = juce::jlimit(1, laneIndex == 0 ? 3 : 4, laneRepeats[(size_t) laneIndex]);

                const auto flutterBias = deterministic01(resetSeed + 41 + laneIndex * 17, reseededStep + grayCode + laneIndex * 13);
                laneFlutters[(size_t) laneIndex] = settings.flutter > (laneIndex == 0 ? 0.32f : 0.18f)
                                                   && (resetNow || laneAccents[(size_t) laneIndex] || laneIndex == 2)
                                                   && flutterBias < (settings.flutter * (0.34f + laneIntensities[(size_t) laneIndex] * 0.44f));

                if (laneFlutters[(size_t) laneIndex])
                    laneRepeats[(size_t) laneIndex] = juce::jlimit(3, laneIndex == 0 ? 5 : 8, laneRepeats[(size_t) laneIndex] + 2);
            }

            applyConditionalRules(settings.ruleSet,
                                  resetCounter,
                                  resetNow,
                                  previousAggregateFlutter,
                                  laneActiveFlags,
                                  laneAccents,
                                  laneRepeats,
                                  laneIntensities);

            auto aggregateActive = false;
            auto aggregateAccent = false;
            auto aggregateFlutter = false;
            auto aggregateIntensity = 0.0f;
            auto aggregateRepeats = 1;
            auto aggregateNote = settings.rootNote;

            for (auto laneIndex = 0; laneIndex < 3; ++laneIndex)
            {
                auto& laneStep = snapshot.lanes[(size_t) laneIndex].steps[(size_t) step];
                laneStep.active = laneActiveFlags[(size_t) laneIndex];
                laneStep.accent = laneAccents[(size_t) laneIndex] && laneStep.active;
                laneStep.flutter = laneFlutters[(size_t) laneIndex] && laneStep.active;
                laneStep.repeats = laneRepeats[(size_t) laneIndex];
                laneStep.intensity = laneStep.active ? laneIntensities[(size_t) laneIndex] : contour * 0.08f;
                laneStep.gateMod = juce::jlimit(0.0f, 1.0f, gateMod + laneIndex * 0.06f - (laneStep.flutter ? 0.14f : 0.0f));
                laneStep.drift = (deterministicSigned(resetSeed + 101 + laneIndex * 29, reseededStep + laneIndex * 7)
                                  * settings.drift
                                  * (laneStep.flutter ? 1.35f : 1.0f));

                const auto degreeIndex = wrap(reseededStep * spec.pitchStride + laneIndex * 2 + xorBitCount + resetCounter, (int) scaleDef.degrees.size());
                const auto degree = scaleDef.degrees[(size_t) degreeIndex];
                const auto octaveIndex = juce::jlimit(0, octaveSpan - 1, juce::roundToInt(octaveMod * (float) (octaveSpan - 1) + laneIndex * 0.25f));
                laneStep.note = juce::jlimit(24,
                                             96,
                                             settings.rootNote + degree + 12 * octaveIndex + spec.laneOffsets[(size_t) laneIndex]);

                aggregateActive = aggregateActive || laneStep.active;
                aggregateAccent = aggregateAccent || laneStep.accent;
                aggregateFlutter = aggregateFlutter || laneStep.flutter;

                if (laneStep.intensity >= aggregateIntensity)
                {
                    aggregateIntensity = laneStep.intensity;
                    aggregateRepeats = laneStep.repeats;
                    aggregateNote = laneStep.note;
                }
            }

            auto& aggregate = snapshot.steps[(size_t) step];
            aggregate.active = aggregateActive;
            aggregate.accent = aggregateAccent;
            aggregate.flutter = aggregateFlutter;
            aggregate.repeats = aggregateRepeats;
            aggregate.note = aggregateNote;
            aggregate.intensity = aggregateActive ? aggregateIntensity : contour * 0.12f;

            previousAggregateAccent = aggregateAccent;
            previousAggregateFlutter = aggregateFlutter;
            ++stepsSinceReset;
        }

        snapshot.stages = {
            { "Seed", juce::String(pulseCount) + "/" + juce::String(spec.steps), settings.density },
            { "Reset", snapshot.resetModeName, settings.flutter },
            { "Rules", snapshot.ruleName, juce::jmap((float) settings.ruleSet, 0.0f, 4.0f, 0.08f, 1.0f) },
            { "Scale", snapshot.scaleName, settings.pitchSpread },
            { "Drift", juce::String(settings.drift, 2), settings.drift }
        };

        return snapshot;
    }

    static bool shouldTriggerLaneStep(const ErhcetuaSnapshot& snapshot, int laneIndex, int stepIndex, int cycleIndex, float probability)
    {
        if (snapshot.stepCount <= 0 || laneIndex < 0 || laneIndex >= (int) snapshot.lanes.size())
            return false;

        const auto wrapped = wrap(stepIndex, snapshot.stepCount);
        const auto& lane = snapshot.lanes[(size_t) laneIndex];
        const auto& step = lane.steps[(size_t) wrapped];

        if (! step.active)
            return false;

        const auto randomValue = deterministic01(cycleIndex + 17 + laneIndex * 29, wrapped + step.note);
        const auto threshold = juce::jlimit(0.05f,
                                            1.0f,
                                            probability * (0.52f + step.intensity * 0.44f)
                                            + (step.accent ? 0.12f : 0.0f)
                                            + (step.flutter ? 0.06f : 0.0f));
        return randomValue <= threshold;
    }

    static float velocityForLaneStep(const ErhcetuaSnapshot& snapshot, int laneIndex, int stepIndex)
    {
        const auto wrapped = wrap(stepIndex, snapshot.stepCount);
        const auto& lane = snapshot.lanes[(size_t) laneIndex];
        const auto& step = lane.steps[(size_t) wrapped];
        return juce::jlimit(0.12f,
                            1.0f,
                            0.22f + step.intensity * 0.56f + (step.accent ? 0.12f : 0.0f) + (laneIndex == 0 ? 0.08f : 0.0f));
    }

    static std::vector<double> subStepOffsets(const ErhcetuaLaneEventSnapshot& step, double stepLengthQuarterNotes)
    {
        std::vector<double> offsets;
        const auto repeats = juce::jmax(1, step.repeats);
        offsets.reserve((size_t) repeats);

        if (repeats == 1)
        {
            offsets.push_back(0.0);
            return offsets;
        }

        const auto span = stepLengthQuarterNotes * (step.flutter ? 0.42 : 0.72);
        const auto delta = span / static_cast<double>(repeats);

        for (auto index = 0; index < repeats; ++index)
        {
            auto offset = delta * static_cast<double>(index);
            if (step.flutter)
            {
                const auto curve = static_cast<double>(index) / static_cast<double>(juce::jmax(1, repeats - 1));
                offset *= 0.5 + curve * 0.8;
            }

            offsets.push_back(offset);
        }

        return offsets;
    }

    static double swungStepLengthQuarterNotes(int stepIndex, int stepCount, float swing)
    {
        if (stepCount <= 0)
            return 0.0;

        const auto baseLength = 4.0 / static_cast<double>(stepCount);
        const auto clampedSwing = juce::jlimit(0.0f, 0.45f, swing);
        const auto offset = (stepIndex % 2 == 0 ? -1.0 : 1.0) * baseLength * 0.5 * clampedSwing;
        return baseLength + offset;
    }

    static double stepStartQuarterNotes(int stepIndex, int stepCount, float swing)
    {
        double start = 0.0;
        for (auto i = 0; i < stepIndex; ++i)
            start += swungStepLengthQuarterNotes(i, stepCount, swing);
        return start;
    }

    static double cycleLengthQuarterNotes(int stepCount)
    {
        return stepCount > 0 ? 4.0 : 0.0;
    }

private:
    struct GrammarSpec
    {
        const char* name;
        int steps;
        int clockDiv;
        int clockOffset;
        int xorSeed;
        int mask;
        int threshold;
        int maskOffset;
        int seedOffset;
        int accentBitCount;
        int pitchStride;
        float contourWarp;
        std::array<int, 3> laneOffsets;
    };

    struct ScaleSpec
    {
        const char* name;
        std::vector<int> degrees;
    };

    static GrammarSpec getGrammarSpec(int grammar)
    {
        static const std::array<GrammarSpec, 5> specs {{
            { "Lattice Drift", 16, 5, 0, 0x13, 0x07, 4, 1, 1, 3, 3, 0.18f, { -12, 0, 12 } },
            { "Slipstream", 24, 7, 2, 0x1d, 0x0b, 6, 3, 2, 3, 5, 0.26f, { -12, 5, 17 } },
            { "Xor Bloom", 16, 4, 1, 0x2b, 0x0f, 7, 0, 3, 4, 2, 0.42f, { -12, 0, 19 } },
            { "Recursive Fold", 20, 6, 3, 0x35, 0x1f, 10, 2, 0, 4, 4, 0.34f, { -12, 3, 15 } },
            { "Signal Haze", 12, 5, 1, 0x29, 0x0d, 5, 1, 1, 3, 6, 0.5f, { -12, 7, 12 } }
        }};

        return specs[(size_t) juce::jlimit(0, (int) specs.size() - 1, grammar)];
    }

    static ScaleSpec getScaleSpec(int scale)
    {
        static const std::array<ScaleSpec, 5> defs {{
            { "Chromatic", { 0, 1, 2, 3, 5, 6, 7, 9, 10, 11 } },
            { "Minor", { 0, 2, 3, 5, 7, 8, 10, 12, 14, 15 } },
            { "Phrygian", { 0, 1, 3, 5, 7, 8, 10, 12, 13, 15 } },
            { "Whole Tone", { 0, 2, 4, 6, 8, 10, 12, 14 } },
            { "Octatonic", { 0, 1, 3, 4, 6, 7, 9, 10, 12, 13 } }
        }};

        return defs[(size_t) juce::jlimit(0, (int) defs.size() - 1, scale)];
    }

    static void applyConditionalRules(int ruleSet,
                                      int resetCounter,
                                      bool resetNow,
                                      bool previousFlutter,
                                      std::array<bool, 3>& activeFlags,
                                      std::array<bool, 3>& accents,
                                      std::array<int, 3>& repeats,
                                      std::array<float, 3>& intensities)
    {
        switch (ruleSet)
        {
            case 1:
                if (resetNow)
                {
                    accents[0] = true;
                    intensities[0] = juce::jmin(1.0f, intensities[0] + 0.16f);
                }
                break;
            case 2:
                if (previousFlutter)
                {
                    activeFlags[2] = false;
                    repeats[1] = juce::jmax(1, repeats[1] - 1);
                }
                break;
            case 3:
                if (resetNow && resetCounter % 3 == 0)
                {
                    intensities[2] = juce::jmin(1.0f, intensities[2] + 0.18f);
                    repeats[2] = juce::jlimit(1, 8, repeats[2] + 1);
                }
                break;
            case 4:
                if (! resetNow)
                {
                    activeFlags[1] = activeFlags[0] || activeFlags[1];
                    accents[1] = accents[0] || accents[1];
                }
                break;
            default:
                break;
        }
    }

    static int wrap(int value, int modulus)
    {
        if (modulus <= 0)
            return 0;

        auto result = value % modulus;
        if (result < 0)
            result += modulus;
        return result;
    }

    static std::vector<bool> euclidean(int steps, int pulses)
    {
        std::vector<bool> pattern((size_t) steps, false);

        if (steps <= 0 || pulses <= 0)
            return pattern;

        if (pulses >= steps)
        {
            std::fill(pattern.begin(), pattern.end(), true);
            return pattern;
        }

        for (auto step = 0; step < steps; ++step)
            pattern[(size_t) step] = ((step * pulses) % steps) < pulses;

        return pattern;
    }

    static float rhythmicContour(int stepIndex, int steps, float mutation, float warp)
    {
        const auto phase = juce::MathConstants<float>::twoPi * static_cast<float>(stepIndex) / static_cast<float>(juce::jmax(1, steps));
        const auto waveA = 0.5f + 0.5f * std::sin(phase * (1.7f + warp));
        const auto waveB = 0.5f + 0.5f * std::cos(phase * (2.4f + mutation * 2.8f));
        const auto waveC = 0.5f + 0.5f * std::sin(phase * (4.0f + warp * 2.0f));
        return juce::jlimit(0.0f, 1.0f, waveA * 0.34f + waveB * (0.36f + mutation * 0.22f) + waveC * 0.3f);
    }

    static int countBits(uint32_t value)
    {
        auto count = 0;
        while (value != 0)
        {
            count += (int) (value & 1u);
            value >>= 1u;
        }

        return count;
    }

    static float deterministic01(int cycleIndex, int stepIndex)
    {
        const auto cycleBits = (uint32_t) static_cast<int32_t>(cycleIndex);
        const auto stepBits = (uint32_t) static_cast<int32_t>(stepIndex);
        uint32_t x = (cycleBits * 0x45d9f3bu) ^ (stepBits * 0x119de1f3u);
        x ^= x >> 16;
        x *= 0x7feb352du;
        x ^= x >> 15;
        x *= 0x846ca68bu;
        x ^= x >> 16;
        return (float) (x & 0x00ffffffu) / (float) 0x01000000u;
    }

    static float deterministicSigned(int cycleIndex, int stepIndex)
    {
        return deterministic01(cycleIndex, stepIndex) * 2.0f - 1.0f;
    }
};
