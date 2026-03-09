#pragma once

#include <array>
#include <cmath>
#include <juce_audio_processors/juce_audio_processors.h>

struct Gp6LaneStep
{
    bool active = false;
    bool accent = false;
    bool overloaded = false;
    int note = 60;
    float intensity = 0.0f;
    float gateBias = 0.5f;
    float state = 0.0f;
};

struct Gp6LaneSnapshot
{
    juce::String name;
    juce::Colour colour;
    std::vector<Gp6LaneStep> steps;
};

struct Gp6PatchPoint
{
    int sourceIndex = 0;
    int destinationIndex = 0;
    juce::String from;
    juce::String to;
    float amount = 0.0f;
};

struct Gp6ModuleStatus
{
    juce::String name;
    float level = 0.0f;
    bool overloaded = false;
};

struct Gp6Snapshot
{
    juce::String patchName;
    juce::String modeName;
    juce::String repeaterName;
    juce::String summary;
    std::vector<Gp6LaneSnapshot> lanes;
    std::vector<Gp6PatchPoint> patchPoints;
    std::vector<Gp6ModuleStatus> modules;
    std::vector<float> referenceTrace;
    std::vector<bool> resetMarkers;
    int stepCount = 0;
    int currentStep = -1;
    int resetCount = 0;
    float overloadRatio = 0.0f;
    float referenceLevel = 0.0f;
};

class Gp6PatchEngine
{
public:
    static constexpr int cableCount = 12;
    static constexpr int sourceCount = 12;
    static constexpr int destinationCount = 8;
    static constexpr int laneCount = 6;

    struct Settings
    {
        int patchProgram = 0;
        int operatingMode = 1;
        int repetitiveMode = 1;
        float timeScale = 0.5f;
        float patchDensity = 0.5f;
        float feedback = 0.35f;
        float multiplier = 0.4f;
        float nonlinearity = 0.25f;
        int rotation = 0;
        float swing = 0.05f;
        float probability = 0.92f;
        float overloadSense = 0.4f;
        int rootNote = 48;
        int spread = 18;
        std::array<int, cableCount> cableSources { 0, 1, 2, 2, 4, 3, 6, 7, 8, 9, 10, 11 };
        std::array<int, cableCount> cableDestinations { 2, 2, 0, 1, 3, 6, 5, 5, 3, 4, 7, 4 };
        std::array<float, cableCount> cableAmounts { 0.68f, 0.54f, 0.42f, 0.38f, 0.5f, 0.46f, 0.62f, 0.48f, 0.33f, 0.58f, 0.37f, 0.29f };
    };

    static const juce::StringArray& sourceNames()
    {
        static const juce::StringArray names { "A", "B", "SUM1", "C", "A x B", "CMP1", "D", "E", "SUM2", "D x E", "CMP2", "CLK" };
        return names;
    }

    static const juce::StringArray& destinationNames()
    {
        static const juce::StringArray names { "A", "B", "SUM1", "D", "E", "SUM2", "CMP1", "CMP2" };
        return names;
    }

    static juce::StringArray patchPrograms()
    {
        return {
            "Ballistic Curve",
            "Damped Orbit",
            "Servo Study",
            "Predator Chase",
            "Summer Ring",
            "Multiplier Bloom"
        };
    }

    static juce::StringArray operatingModes()
    {
        return {
            "IC Hold",
            "Operate",
            "Hybrid"
        };
    }

    static juce::StringArray repetitiveModes()
    {
        return {
            "Single Run",
            "Repetitive",
            "Reset On Overload"
        };
    }

    static Gp6Snapshot generate(const Settings& settings, const juce::String& presetName = {})
    {
        const auto program = getProgramSpec(settings.patchProgram);
        const auto stepCount = juce::jlimit(12,
                                            32,
                                            12 + juce::roundToInt(settings.timeScale * 20.0f));
        const auto rotation = wrap(settings.rotation, stepCount);
        const auto overloadLimit = juce::jmap(settings.overloadSense, 0.0f, 1.0f, 1.75f, 0.66f);

        Gp6Snapshot snapshot;
        snapshot.patchName = presetName;
        snapshot.modeName = operatingModes()[juce::jlimit(0, operatingModes().size() - 1, settings.operatingMode)];
        snapshot.repeaterName = repetitiveModes()[juce::jlimit(0, repetitiveModes().size() - 1, settings.repetitiveMode)];
        snapshot.summary = "expanded gp-6: dual summers, dual comparators, twin multipliers";
        snapshot.stepCount = stepCount;
        snapshot.referenceTrace.assign((size_t) stepCount, 0.0f);
        snapshot.resetMarkers.assign((size_t) stepCount, false);
        snapshot.lanes = createLanes(stepCount);
        if (snapshot.lanes.size() != laneCount)
            return snapshot;

        auto integratorA = initialStateForMode(settings.operatingMode, 0.82f + program.biasA);
        auto integratorB = initialStateForMode(settings.operatingMode, -0.57f + program.biasB);
        auto integratorC = initialStateForMode(settings.operatingMode, 0.18f + program.biasC);
        auto integratorD = initialStateForMode(settings.operatingMode, -0.36f + program.biasD);
        auto integratorE = initialStateForMode(settings.operatingMode, 0.41f + program.biasE);
        auto overloadCount = 0;
        auto resetCount = 0;

        for (auto step = 0; step < stepCount; ++step)
        {
            const auto sourceStep = wrap(step - rotation, stepCount);
            const auto phase = juce::MathConstants<float>::twoPi * (float) sourceStep / (float) stepCount;
            const auto driveA = std::sin(phase * program.driveMulA + program.phaseOffsetA);
            const auto driveB = std::cos(phase * program.driveMulB + program.phaseOffsetB);
            const auto driveC = std::sin(phase * (program.driveMulA + program.driveMulB) * 0.5f + program.phaseOffsetC);

            const auto densityBias = juce::jmap(settings.patchDensity, 0.0f, 1.0f, 0.35f, 1.35f);
            const auto feedbackBias = juce::jmap(settings.feedback, 0.0f, 1.0f, 0.0f, 0.82f);
            const auto multiplierBias = juce::jmap(settings.multiplier, 0.0f, 1.0f, 0.2f, 1.7f);
            const auto nonlinear = juce::jmap(settings.nonlinearity, 0.0f, 1.0f, 0.0f, 1.4f);
            const auto dt = juce::jmap(settings.timeScale, 0.0f, 1.0f, 0.055f, 0.23f);
            const auto preSummer = (integratorA * program.mixA
                                    + integratorB * program.mixB
                                    + driveA * program.inputA
                                    + driveB * program.inputB)
                                   * densityBias;
            const auto preProduct = integratorA * integratorB * multiplierBias;
            const auto preComparator = preSummer - integratorC * (0.4f + program.comparatorTilt);
            const auto preSummer2 = (integratorD * program.mixD
                                     + integratorE * program.mixE
                                     + preComparator * program.inputC
                                     + driveC * program.inputD)
                                    * (0.72f + settings.patchDensity * 0.84f);
            const auto preProduct2 = integratorD * integratorE * multiplierBias * (0.72f + settings.nonlinearity * 0.5f);
            const auto preComparator2 = preSummer2 - (integratorA - integratorB + integratorC * 0.5f) * (0.35f + program.comparatorTilt2);
            const auto clockSignal = std::sin(phase * juce::jmap(settings.timeScale, 0.0f, 1.0f, 1.0f, 3.0f));

            std::array<float, 8> routed {};

            for (auto cableIndex = 0; cableIndex < (int) settings.cableAmounts.size(); ++cableIndex)
            {
                const auto amount = juce::jlimit(0.0f, 1.0f, settings.cableAmounts[(size_t) cableIndex]);
                const auto sourceSignal = signalForSource(settings.cableSources[(size_t) cableIndex],
                                                          integratorA,
                                                          integratorB,
                                                          integratorC,
                                                          preSummer,
                                                          preProduct,
                                                          preComparator,
                                                          integratorD,
                                                          integratorE,
                                                          preSummer2,
                                                          preProduct2,
                                                          preComparator2,
                                                          clockSignal);

                const auto destinationIndex = juce::jlimit(0, destinationNames().size() - 1, settings.cableDestinations[(size_t) cableIndex]);
                routed[(size_t) destinationIndex] += sourceSignal * amount;
            }

            const auto summer = preSummer + routed[2] * 0.8f;
            const auto product = preProduct + routed[3] * multiplierBias * 0.45f;
            const auto comparator = preComparator + routed[6] * 0.9f;
            const auto summer2 = preSummer2 + routed[5] * 0.82f;
            const auto product2 = preProduct2 + routed[4] * multiplierBias * 0.41f;
            const auto comparator2 = preComparator2 + routed[7] * 0.88f;

            auto deltaA = (summer - integratorA * program.dampingA + product * program.crossA + routed[0] * (0.32f + feedbackBias)) * dt;
            auto deltaB = (driveB + comparator * program.crossB - integratorB * program.dampingB + routed[1] * (0.32f + feedbackBias)) * dt;
            auto deltaC = (driveC + (integratorA - integratorB) * program.crossC - integratorC * program.dampingC + comparator2 * 0.11f) * dt;
            auto deltaD = (summer2 - integratorD * program.dampingD + product2 * program.crossD + routed[3] * (0.28f + feedbackBias)) * dt;
            auto deltaE = (comparator + comparator2 * program.crossE - integratorE * program.dampingE + routed[4] * (0.3f + feedbackBias)) * dt;

            deltaA += feedbackBias * integratorC * 0.08f;
            deltaB += feedbackBias * integratorA * 0.06f;
            deltaC += feedbackBias * integratorB * 0.05f;
            deltaD += feedbackBias * integratorE * 0.06f;
            deltaE += feedbackBias * integratorD * 0.05f;

            integratorA += deltaA;
            integratorB += deltaB;
            integratorC += deltaC;
            integratorD += deltaD;
            integratorE += deltaE;

            if (nonlinear > 0.0f)
            {
                integratorA = std::tanh(integratorA * (1.0f + nonlinear * 0.55f));
                integratorB = std::tanh(integratorB * (1.0f + nonlinear * 0.35f));
                integratorC = std::tanh(integratorC * (1.0f + nonlinear * 0.75f));
                integratorD = std::tanh(integratorD * (1.0f + nonlinear * 0.48f));
                integratorE = std::tanh(integratorE * (1.0f + nonlinear * 0.63f));
            }

            const auto largestMagnitude = juce::jmax(juce::jmax(std::abs(integratorA), std::abs(integratorB)),
                                                     juce::jmax(std::abs(integratorC),
                                                                juce::jmax(std::abs(integratorD),
                                                                           juce::jmax(std::abs(integratorE),
                                                                                      juce::jmax(std::abs(product), std::abs(product2))))));
            const auto overloaded = largestMagnitude > overloadLimit;

            auto resetNow = false;
            if (step == 0)
                resetNow = settings.repetitiveMode != 0;
            else if (settings.repetitiveMode == 1)
                resetNow = sourceStep == 0;
            else if (settings.repetitiveMode == 2)
                resetNow = overloaded;

            if (resetNow)
            {
                ++resetCount;
                snapshot.resetMarkers[(size_t) step] = true;
                integratorA = initialStateForMode(settings.operatingMode, program.biasA);
                integratorB = initialStateForMode(settings.operatingMode, program.biasB);
                integratorC = initialStateForMode(settings.operatingMode, program.biasC);
                integratorD = initialStateForMode(settings.operatingMode, program.biasD);
                integratorE = initialStateForMode(settings.operatingMode, program.biasE);
            }

            if (overloaded)
            {
                ++overloadCount;
                integratorA = juce::jlimit(-overloadLimit, overloadLimit, integratorA);
                integratorB = juce::jlimit(-overloadLimit, overloadLimit, integratorB);
                integratorC = juce::jlimit(-overloadLimit, overloadLimit, integratorC);
                integratorD = juce::jlimit(-overloadLimit, overloadLimit, integratorD);
                integratorE = juce::jlimit(-overloadLimit, overloadLimit, integratorE);
            }

            const auto pulseA = std::abs(comparator) > juce::jmap(settings.patchDensity, 0.0f, 1.0f, 0.72f, 0.18f);
            const auto pulseB = std::abs(product) > juce::jmap(settings.multiplier, 0.0f, 1.0f, 0.62f, 0.14f);
            const auto pulseC = std::abs(integratorC) > juce::jmap(settings.feedback, 0.0f, 1.0f, 0.68f, 0.16f);
            const auto pulseD = clockSignal > juce::jmap(settings.timeScale, 0.0f, 1.0f, 0.88f, 0.12f);
            const auto pulseE = std::abs(comparator2) > juce::jmap(settings.patchDensity, 0.0f, 1.0f, 0.76f, 0.2f);
            const auto pulseF = std::abs(product2) > juce::jmap(settings.multiplier, 0.0f, 1.0f, 0.66f, 0.16f);

            writeLaneStep(snapshot.lanes[0], step, pulseA, std::abs(integratorA), overloaded, settings.rootNote, integratorA);
            writeLaneStep(snapshot.lanes[1], step, pulseB, std::abs(integratorB), overloaded, settings.rootNote + settings.spread / 5, integratorB);
            writeLaneStep(snapshot.lanes[2], step, pulseC, std::abs(comparator) * 0.8f, overloaded, settings.rootNote + settings.spread * 2 / 5, comparator);
            writeLaneStep(snapshot.lanes[3], step, pulseD || resetNow, std::abs(integratorD), overloaded, settings.rootNote + settings.spread * 3 / 5, integratorD);
            writeLaneStep(snapshot.lanes[4], step, pulseE, std::abs(integratorE), overloaded, settings.rootNote + settings.spread * 4 / 5, integratorE);
            writeLaneStep(snapshot.lanes[5], step, pulseF, std::abs(comparator2) * 0.86f, overloaded, settings.rootNote + settings.spread, comparator2);

            snapshot.referenceTrace[(size_t) step] = juce::jlimit(-1.0f, 1.0f, summer * 0.32f + product * 0.12f + summer2 * 0.28f + product2 * 0.1f);
        }

        snapshot.patchPoints.clear();
        snapshot.patchPoints.reserve(settings.cableAmounts.size());
        for (auto cableIndex = 0; cableIndex < (int) settings.cableAmounts.size(); ++cableIndex)
        {
            const auto sourceIndex = juce::jlimit(0, sourceNames().size() - 1, settings.cableSources[(size_t) cableIndex]);
            const auto destinationIndex = juce::jlimit(0, destinationNames().size() - 1, settings.cableDestinations[(size_t) cableIndex]);
            snapshot.patchPoints.push_back({ sourceIndex,
                                             destinationIndex,
                                             sourceNames()[sourceIndex],
                                             destinationNames()[destinationIndex],
                                             settings.cableAmounts[(size_t) cableIndex] });
        }

        snapshot.modules = {
            { "Int A", averageLevel(snapshot.lanes[0]), laneOverloaded(snapshot.lanes[0]) },
            { "Int B", averageLevel(snapshot.lanes[1]), laneOverloaded(snapshot.lanes[1]) },
            { "Cmp C", averageLevel(snapshot.lanes[2]), laneOverloaded(snapshot.lanes[2]) },
            { "Int D", averageLevel(snapshot.lanes[3]), laneOverloaded(snapshot.lanes[3]) },
            { "Int E", averageLevel(snapshot.lanes[4]), laneOverloaded(snapshot.lanes[4]) },
            { "Cmp F", averageLevel(snapshot.lanes[5]), laneOverloaded(snapshot.lanes[5]) }
        };
        snapshot.resetCount = resetCount;
        snapshot.overloadRatio = stepCount > 0 ? (float) overloadCount / (float) stepCount : 0.0f;
        snapshot.referenceLevel = averageAbs(snapshot.referenceTrace);

        if (snapshot.patchName.isEmpty())
            snapshot.patchName = program.name;

        return snapshot;
    }

    static bool shouldTriggerLaneStep(const Gp6Snapshot& snapshot, int laneIndex, int stepIndex, int cycleIndex, float probability)
    {
        if (snapshot.stepCount <= 0 || laneIndex < 0 || laneIndex >= (int) snapshot.lanes.size())
            return false;

        const auto wrapped = wrap(stepIndex, snapshot.stepCount);
        const auto& step = snapshot.lanes[(size_t) laneIndex].steps[(size_t) wrapped];
        if (! step.active)
            return false;

        const auto random = deterministic01(cycleIndex * 31 + laneIndex * 17, wrapped * 19 + laneIndex * 11);
        const auto laneBias = laneIndex == 3 ? 0.84f : laneIndex == 2 ? 0.9f : 1.0f;
        const auto overloadPenalty = step.overloaded ? 0.12f : 0.0f;
        const auto threshold = juce::jlimit(0.06f,
                                            1.0f,
                                            probability * laneBias * (0.42f + step.intensity * 0.62f - overloadPenalty));
        return random <= threshold;
    }

    static float velocityForLaneStep(const Gp6Snapshot& snapshot, int laneIndex, int stepIndex)
    {
        const auto wrapped = wrap(stepIndex, snapshot.stepCount);
        const auto& step = snapshot.lanes[(size_t) laneIndex].steps[(size_t) wrapped];
        const auto accentBoost = step.accent ? 0.18f : 0.0f;
        const auto overloadBoost = step.overloaded ? 0.1f : 0.0f;
        return juce::jlimit(0.12f, 1.0f, 0.24f + step.intensity * 0.54f + accentBoost + overloadBoost);
    }

    static double swungStepLengthQuarterNotes(int stepIndex, int stepCount, float swing)
    {
        if (stepCount <= 0)
            return 0.0;

        const auto baseLength = 4.0 / static_cast<double>(stepCount);
        const auto clampedSwing = juce::jlimit(0.0f, 0.45f, swing);
        const auto skew = (stepIndex % 2 == 0 ? -1.0 : 1.0) * baseLength * 0.5 * clampedSwing;
        return baseLength + skew;
    }

    static double stepStartQuarterNotes(int stepIndex, int stepCount, float swing)
    {
        double start = 0.0;
        for (auto i = 0; i < stepIndex; ++i)
            start += swungStepLengthQuarterNotes(i, stepCount, swing);
        return start;
    }

    static double cycleLengthQuarterNotes(int)
    {
        return 4.0;
    }

private:
    struct ProgramSpec
    {
        const char* name;
        float mixA;
        float mixB;
        float inputA;
        float inputB;
        float crossA;
        float crossB;
        float crossC;
        float dampingA;
        float dampingB;
        float dampingC;
        float driveMulA;
        float driveMulB;
        float phaseOffsetA;
        float phaseOffsetB;
        float phaseOffsetC;
        float comparatorTilt;
        float mixD;
        float mixE;
        float inputC;
        float inputD;
        float crossD;
        float crossE;
        float dampingD;
        float dampingE;
        float comparatorTilt2;
        float biasA;
        float biasB;
        float biasC;
        float biasD;
        float biasE;
    };

    static std::vector<Gp6LaneSnapshot> createLanes(int stepCount)
    {
        std::vector<Gp6LaneSnapshot> lanes;
        lanes.push_back({ "Integrator A", juce::Colour::fromRGB(241, 149, 87), std::vector<Gp6LaneStep>((size_t) stepCount) });
        lanes.push_back({ "Integrator B", juce::Colour::fromRGB(92, 211, 182), std::vector<Gp6LaneStep>((size_t) stepCount) });
        lanes.push_back({ "Comparator C", juce::Colour::fromRGB(249, 232, 122), std::vector<Gp6LaneStep>((size_t) stepCount) });
        lanes.push_back({ "Integrator D", juce::Colour::fromRGB(245, 92, 109), std::vector<Gp6LaneStep>((size_t) stepCount) });
        lanes.push_back({ "Integrator E", juce::Colour::fromRGB(139, 173, 255), std::vector<Gp6LaneStep>((size_t) stepCount) });
        lanes.push_back({ "Comparator F", juce::Colour::fromRGB(233, 199, 123), std::vector<Gp6LaneStep>((size_t) stepCount) });
        return lanes;
    }

    static void writeLaneStep(Gp6LaneSnapshot& lane, int stepIndex, bool active, float intensity, bool overloaded, int note, float state)
    {
        auto& step = lane.steps[(size_t) stepIndex];
        step.active = active;
        step.accent = intensity > 0.76f || overloaded;
        step.overloaded = overloaded;
        step.note = juce::jlimit(24, 96, note);
        step.intensity = juce::jlimit(0.0f, 1.0f, intensity);
        step.gateBias = juce::jlimit(0.1f, 0.95f, 0.24f + intensity * 0.58f);
        step.state = juce::jlimit(-1.0f, 1.0f, state);
    }

    static const ProgramSpec& getProgramSpec(int programIndex)
    {
        static const std::array<ProgramSpec, 6> specs {{
            { "Ballistic Curve", 0.92f, -0.34f, 0.84f, 0.46f, 0.25f, 0.16f, 0.48f, 0.21f, 0.18f, 0.22f, 1.0f, 2.0f, 0.0f, 0.3f, 0.6f, 0.22f, 0.64f, -0.27f, 0.48f, 0.22f, 0.31f, 0.18f, 0.2f, 0.42f, 0.18f, -0.16f, 0.08f, -0.14f, 0.19f, 0.11f },
            { "Damped Orbit", 0.74f, 0.58f, 0.54f, 0.82f, 0.12f, 0.32f, 0.38f, 0.33f, 0.28f, 0.24f, 1.0f, 1.5f, 0.5f, 0.0f, 1.1f, 0.18f, 0.52f, 0.62f, 0.43f, 0.35f, 0.28f, 0.24f, 0.26f, 0.29f, 0.24f, 0.28f, -0.24f, -0.06f, 0.13f, -0.09f },
            { "Servo Study", 1.08f, -0.48f, 0.66f, 0.28f, 0.44f, 0.21f, 0.27f, 0.26f, 0.36f, 0.18f, 2.0f, 1.0f, 0.15f, 0.9f, 0.4f, 0.31f, 0.58f, -0.36f, 0.51f, 0.26f, 0.37f, 0.16f, 0.33f, 0.24f, 0.31f, -0.08f, 0.12f, 0.21f, -0.18f, 0.24f },
            { "Predator Chase", 0.84f, -0.67f, 0.78f, 0.72f, 0.33f, 0.29f, 0.64f, 0.19f, 0.23f, 0.18f, 1.0f, 3.0f, 0.1f, 0.4f, 0.95f, 0.36f, 0.81f, -0.41f, 0.72f, 0.34f, 0.45f, 0.22f, 0.18f, 0.17f, 0.38f, 0.34f, -0.31f, 0.04f, 0.22f, -0.13f },
            { "Summer Ring", 1.16f, 0.44f, 0.48f, 0.96f, 0.19f, 0.42f, 0.21f, 0.28f, 0.22f, 0.31f, 1.5f, 2.5f, 0.75f, 0.15f, 0.35f, 0.17f, 0.63f, 0.59f, 0.39f, 0.62f, 0.27f, 0.3f, 0.21f, 0.31f, 0.23f, 0.11f, 0.06f, -0.18f, -0.04f, 0.12f },
            { "Multiplier Bloom", 0.68f, 0.71f, 0.98f, 0.64f, 0.51f, 0.37f, 0.55f, 0.22f, 0.21f, 0.15f, 2.0f, 2.0f, 0.25f, 0.7f, 0.85f, 0.28f, 0.54f, 0.77f, 0.66f, 0.48f, 0.41f, 0.19f, 0.17f, 0.14f, 0.33f, -0.16f, 0.19f, 0.09f, 0.16f, 0.21f }
        }};

        return specs[(size_t) juce::jlimit(0, (int) specs.size() - 1, programIndex)];
    }

    static float initialStateForMode(int mode, float value)
    {
        switch (mode)
        {
            case 0: return value;
            case 1: return 0.0f;
            case 2: return value * 0.45f;
            default: return 0.0f;
        }
    }

    static float averageLevel(const Gp6LaneSnapshot& lane)
    {
        float sum = 0.0f;
        for (const auto& step : lane.steps)
            sum += step.intensity;
        return lane.steps.empty() ? 0.0f : sum / (float) lane.steps.size();
    }

    static bool laneOverloaded(const Gp6LaneSnapshot& lane)
    {
        for (const auto& step : lane.steps)
            if (step.overloaded)
                return true;
        return false;
    }

    static float averageAbs(const std::vector<float>& values)
    {
        float sum = 0.0f;
        for (const auto value : values)
            sum += std::abs(value);
        return values.empty() ? 0.0f : sum / (float) values.size();
    }

    static int wrap(int value, int size)
    {
        if (size <= 0)
            return 0;
        auto wrapped = value % size;
        if (wrapped < 0)
            wrapped += size;
        return wrapped;
    }

    static float deterministic01(int a, int b)
    {
        const auto ua = static_cast<uint32_t>(a);
        const auto ub = static_cast<uint32_t>(b);
        uint32_t x = (ua * 73856093u) ^ (ub * 19349663u) ^ 0x9e3779b9u;
        x ^= x >> 16;
        x *= 0x7feb352du;
        x ^= x >> 15;
        x *= 0x846ca68bu;
        x ^= x >> 16;
        return (float) (x & 0x00ffffffu) / (float) 0x01000000u;
    }

    static float signalForSource(int sourceIndex,
                                 float integratorA,
                                 float integratorB,
                                 float integratorC,
                                 float summer,
                                 float product,
                                 float comparator,
                                 float integratorD,
                                 float integratorE,
                                 float summer2,
                                 float product2,
                                 float comparator2,
                                 float clock)
    {
        switch (juce::jlimit(0, sourceNames().size() - 1, sourceIndex))
        {
            case 0: return integratorA;
            case 1: return integratorB;
            case 2: return summer;
            case 3: return integratorC;
            case 4: return product;
            case 5: return comparator;
            case 6: return integratorD;
            case 7: return integratorE;
            case 8: return summer2;
            case 9: return product2;
            case 10: return comparator2;
            case 11: return clock;
            default: return 0.0f;
        }
    }
};
