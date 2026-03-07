#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

struct RhythmLaneSnapshot
{
    juce::String name;
    std::vector<float> intensities;
    std::vector<bool> hits;
    std::vector<bool> accents;
    int midiNote = 60;
};

struct RhythmSnapshot
{
    juce::String familyName;
    juce::String presetName;
    std::vector<float> intensities;
    std::vector<bool> hits;
    std::vector<bool> accents;
    std::vector<RhythmLaneSnapshot> lanes;
    int stepCount = 0;
    int currentStep = -1;
    float swing = 0.0f;
};

class RhythmEngine
{
public:
    struct Settings
    {
        int family = 0;
        float density = 0.55f;
        float complexity = 0.35f;
        int rotation = 0;
        float swing = 0.0f;
        float probability = 1.0f;
        std::array<int, 4> laneNotes { 76, 67, 48, 82 };
    };

    static constexpr int laneCount = 4;

    static juce::StringArray families()
    {
        return {
            "Ewe Bell",
            "Bembe",
            "Kpanlogo",
            "Gahu",
            "Sikyi",
            "Cross-Rhythm"
        };
    }

    static juce::StringArray laneNames()
    {
        return {
            "Bell",
            "Support",
            "Bass",
            "Shaker"
        };
    }

    static RhythmSnapshot generate(const Settings& settings, const juce::String& presetName = {})
    {
        const auto spec = getFamilySpec(settings.family);

        RhythmSnapshot snapshot;
        snapshot.familyName = spec.name;
        snapshot.presetName = presetName;
        snapshot.stepCount = spec.steps;
        snapshot.currentStep = -1;
        snapshot.swing = settings.swing;
        snapshot.hits.assign((size_t) spec.steps, false);
        snapshot.accents.assign((size_t) spec.steps, false);
        snapshot.intensities.assign((size_t) spec.steps, 0.0f);
        snapshot.lanes.reserve(laneCount);

        auto bell = createLane("Bell", settings.laneNotes[0], spec.steps);
        auto support = createLane("Support", settings.laneNotes[1], spec.steps);
        auto bass = createLane("Bass", settings.laneNotes[2], spec.steps);
        auto shaker = createLane("Shaker", settings.laneNotes[3], spec.steps);

        for (auto index : spec.accentSeeds)
        {
            const auto step = wrap(index + settings.rotation, spec.steps);
            bell.hits[(size_t) step] = true;
            bell.accents[(size_t) step] = true;
            bell.intensities[(size_t) step] = 1.0f;
        }

        for (auto step = 0; step < spec.steps; ++step)
        {
            const auto shifted = wrap(step - settings.rotation, spec.steps);
            const bool modularPulse = ((shifted + spec.modOffsetA) % spec.modulusA == 0)
                                   || ((shifted + spec.modOffsetB) % spec.modulusB == 0);

            if (modularPulse)
            {
                bell.hits[(size_t) step] = true;
                bell.intensities[(size_t) step] = juce::jmax(bell.intensities[(size_t) step], 0.68f);
            }
        }

        const auto targetPulses = juce::jlimit(spec.minimumPulses,
                                               spec.steps - 1,
                                               spec.minimumPulses + juce::roundToInt(settings.density * spec.densityRange));

        const auto distributed = euclidean(spec.steps, targetPulses);
        for (auto step = 0; step < spec.steps; ++step)
        {
            if (! distributed[(size_t) wrap(step - settings.rotation, spec.steps)])
                continue;

            const auto weight = rhythmicWeight(step, spec.steps, settings.complexity);
            if (weight > 0.48f)
            {
                bell.hits[(size_t) step] = true;
                bell.intensities[(size_t) step] = juce::jmax(bell.intensities[(size_t) step], weight);
            }
        }

        for (auto step = 0; step < spec.steps; ++step)
        {
            const auto mirrored = wrap(spec.steps - step + settings.rotation, spec.steps);
            const auto complexityBias = settings.complexity * 0.45f;

            if (bell.accents[(size_t) step] && complexityBias > 0.2f)
            {
                bell.hits[(size_t) mirrored] = true;
                bell.intensities[(size_t) mirrored] = juce::jmax(bell.intensities[(size_t) mirrored], 0.58f + complexityBias * 0.3f);
            }

            if (bell.hits[(size_t) step] && ! bell.accents[(size_t) step] && complexityBias < 0.12f)
                bell.intensities[(size_t) step] *= 0.88f;
        }

        const auto supportPulses = juce::jlimit(3, spec.steps - 1, 3 + juce::roundToInt(settings.density * (spec.steps / 3.0f)));
        const auto supportPattern = euclidean(spec.steps, supportPulses);
        for (auto step = 0; step < spec.steps; ++step)
        {
            const auto rotated = wrap(step + settings.rotation / 2, spec.steps);
            if (! supportPattern[(size_t) rotated])
                continue;

            const auto weight = rhythmicWeight(step + spec.modOffsetA, spec.steps, 1.0f - settings.complexity * 0.45f);
            if (weight > 0.35f)
            {
                support.hits[(size_t) step] = true;
                support.intensities[(size_t) step] = juce::jlimit(0.0f, 1.0f, 0.34f + weight * 0.54f);
                support.accents[(size_t) step] = (step % spec.modulusA) == 0;
            }
        }

        const auto bassAnchorSpacing = juce::jmax(2, spec.steps / 4);
        for (auto step = 0; step < spec.steps; ++step)
        {
            const auto shifted = wrap(step - settings.rotation, spec.steps);
            const bool anchor = (shifted % bassAnchorSpacing) == 0;
            const bool echo = settings.complexity > 0.52f && ((shifted + spec.modOffsetB) % juce::jmax(2, spec.modulusB - 1) == 0);

            if (anchor || echo)
            {
                bass.hits[(size_t) step] = true;
                bass.accents[(size_t) step] = anchor;
                bass.intensities[(size_t) step] = anchor ? 0.96f : juce::jlimit(0.0f, 1.0f, 0.42f + settings.complexity * 0.34f);
            }
        }

        const auto shakerThreshold = juce::jmap(settings.density, 0.0f, 1.0f, 0.76f, 0.28f);
        for (auto step = 0; step < spec.steps; ++step)
        {
            const auto energy = rhythmicWeight(step + spec.modulusB, spec.steps, settings.complexity * 0.4f);
            const bool keep = energy > shakerThreshold || bell.hits[(size_t) step];

            if (keep)
            {
                shaker.hits[(size_t) step] = true;
                shaker.accents[(size_t) step] = bell.accents[(size_t) step];
                shaker.intensities[(size_t) step] = juce::jlimit(0.0f, 0.78f, 0.26f + energy * 0.48f);
            }
        }

        snapshot.lanes.push_back(std::move(bell));
        snapshot.lanes.push_back(std::move(support));
        snapshot.lanes.push_back(std::move(bass));
        snapshot.lanes.push_back(std::move(shaker));

        for (const auto& lane : snapshot.lanes)
        {
            for (auto step = 0; step < spec.steps; ++step)
            {
                if (lane.hits[(size_t) step])
                    snapshot.hits[(size_t) step] = true;

                if (lane.accents[(size_t) step])
                    snapshot.accents[(size_t) step] = true;

                snapshot.intensities[(size_t) step] = juce::jmax(snapshot.intensities[(size_t) step], lane.intensities[(size_t) step]);
            }
        }

        return snapshot;
    }

    static bool shouldTriggerLaneStep(const RhythmSnapshot& snapshot, int laneIndex, int stepIndex, int cycleIndex, float probability)
    {
        if (snapshot.stepCount <= 0 || laneIndex < 0 || laneIndex >= (int) snapshot.lanes.size())
            return false;

        const auto wrappedStep = wrap(stepIndex, snapshot.stepCount);
        const auto& lane = snapshot.lanes[(size_t) laneIndex];
        if (! lane.hits[(size_t) wrappedStep])
            return false;

        const auto probabilityScale = laneIndex == 0 ? 1.0f : laneIndex == 1 ? 0.9f : laneIndex == 2 ? 0.78f : 0.96f;
        const auto value = deterministic01(cycleIndex + laneIndex * 17, wrappedStep + laneIndex * 31);
        const auto intensity = lane.intensities[(size_t) wrappedStep];
        const auto threshold = juce::jlimit(0.05f, 1.0f, probability * probabilityScale * (0.58f + intensity * 0.42f));
        return value <= threshold;
    }

    static float velocityForLaneStep(const RhythmSnapshot& snapshot, int laneIndex, int stepIndex)
    {
        const auto wrappedStep = wrap(stepIndex, snapshot.stepCount);
        const auto& lane = snapshot.lanes[(size_t) laneIndex];
        const auto intensity = lane.intensities[(size_t) wrappedStep];
        const auto accentBoost = lane.accents[(size_t) wrappedStep] ? 0.18f : 0.0f;
        const auto laneBoost = laneIndex == 2 ? 0.12f : 0.0f;
        return juce::jlimit(0.12f, 1.0f, 0.28f + intensity * 0.48f + accentBoost + laneBoost);
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
    struct FamilySpec
    {
        const char* name;
        int steps;
        std::vector<int> accentSeeds;
        int modulusA;
        int modulusB;
        int modOffsetA;
        int modOffsetB;
        int minimumPulses;
        int densityRange;
    };

    static FamilySpec getFamilySpec(int family)
    {
        static const std::array<FamilySpec, 6> specs {{
            { "Ewe Bell", 12, { 0, 2, 5, 7, 9 }, 3, 4, 0, 2, 5, 4 },
            { "Bembe", 12, { 0, 2, 4, 7, 9 }, 3, 5, 1, 4, 5, 5 },
            { "Kpanlogo", 16, { 0, 3, 6, 10, 12 }, 4, 5, 0, 2, 6, 6 },
            { "Gahu", 16, { 0, 3, 7, 10, 13 }, 3, 4, 0, 1, 6, 6 },
            { "Sikyi", 16, { 0, 4, 7, 11, 14 }, 4, 6, 0, 3, 5, 5 },
            { "Cross-Rhythm", 12, { 0, 3, 6, 8 }, 3, 2, 0, 1, 4, 5 }
        }};

        return specs[(size_t) juce::jlimit(0, (int) specs.size() - 1, family)];
    }

    static RhythmLaneSnapshot createLane(const juce::String& name, int midiNote, int steps)
    {
        RhythmLaneSnapshot lane;
        lane.name = name;
        lane.midiNote = midiNote;
        lane.hits.assign((size_t) steps, false);
        lane.accents.assign((size_t) steps, false);
        lane.intensities.assign((size_t) steps, 0.0f);
        return lane;
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

    static float rhythmicWeight(int stepIndex, int steps, float complexity)
    {
        const auto phase = juce::MathConstants<float>::twoPi * static_cast<float>(stepIndex) / static_cast<float>(juce::jmax(1, steps));
        const auto waveA = 0.5f + 0.5f * std::sin(phase * 2.0f);
        const auto waveB = 0.5f + 0.5f * std::cos(phase * 3.0f);
        const auto blend = juce::jlimit(0.0f, 1.0f, 0.35f + complexity * 0.55f);
        return juce::jlimit(0.0f, 1.0f, waveA * (1.0f - blend) + waveB * blend);
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
};
