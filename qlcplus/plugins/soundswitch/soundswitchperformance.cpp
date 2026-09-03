/*
  Q Light Controller Plus
  soundswitchperformance.cpp

  Licensed under the Apache License, Version 2.0.
*/

#include "soundswitchperformance.h"

#include <array>

namespace
{
constexpr std::array<int, 5> kShuffleMultipliers{{13, 21, 29, 37, 45}};
constexpr std::array<int, 5> kShuffleOffsets{{7, 19, 3, 25, 51}};

int bitCountForSteps(int stepsCount)
{
    return stepsCount == 128 ? 7 : 5;
}

int reverseLowBits(int value, int bits)
{
    int reversed = 0;
    for (int bit = 0; bit < bits; ++bit)
        reversed = (reversed << 1) | ((value >> bit) & 1);
    return reversed;
}

int normalisedTarget(int target)
{
    return qBound(0, target,
                  static_cast<int>(kShuffleMultipliers.size()) - 1);
}
}

namespace SoundSwitchPerformance
{
int shuffledStepAt(int position, int stepsCount, int target)
{
    const int count = stepsCount == 128 ? 128 : 32;
    const int mask = count - 1;
    const int selectedTarget = normalisedTarget(target);
    const int reversed = reverseLowBits(qBound(0, position, mask),
                                        bitCountForSteps(count));
    return (reversed *
                kShuffleMultipliers[static_cast<std::size_t>(selectedTarget)] +
            kShuffleOffsets[static_cast<std::size_t>(selectedTarget)]) & mask;
}

int shuffledPositionForStep(int logicalStep, int stepsCount, int target)
{
    const int count = stepsCount == 128 ? 128 : 32;
    const int wanted = qBound(0, logicalStep, count - 1);
    for (int position = 0; position < count; ++position)
    {
        if (shuffledStepAt(position, count, target) == wanted)
            return position;
    }
    return 0;
}

uchar autoplaySeekValue(int bank, int pad, bool allBanks, bool randomized)
{
    const int stepsCount = allBanks ? 128 : 32;
    const int target = allBanks ? 4 : qBound(0, bank, 3);
    const int logicalStep = allBanks
        ? qBound(0, bank, 3) * 32 + qBound(0, pad, 31)
        : qBound(0, pad, 31);
    const int position = randomized
        ? shuffledPositionForStep(logicalStep, stepsCount, target)
        : logicalStep;
    return static_cast<uchar>(255 - position * (256 / stepsCount));
}

uchar autoplaySeekNudgeValue(uchar seekValue, bool allBanks)
{
    // Both supported Cue List sizes have at least two input values per step.
    // The selected seek value is the upper value of each inverted bucket, so
    // one lower value stays in the same bucket, including the final bucket.
    Q_UNUSED(allBanks)
    return seekValue == 0 ? static_cast<uchar>(1)
                          : static_cast<uchar>(seekValue - 1);
}
}
