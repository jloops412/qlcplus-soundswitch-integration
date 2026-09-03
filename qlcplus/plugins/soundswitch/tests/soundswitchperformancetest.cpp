/*
  Q Light Controller Plus
  soundswitchperformancetest.cpp

  Licensed under the Apache License, Version 2.0.
*/

#include "../soundswitchperformance.h"

#include <iostream>
#include <set>

namespace
{
int decodedCueStep(uchar value, int stepsCount)
{
    const int level = 255 - static_cast<int>(value);
    const double stepSize = 256.0 / static_cast<double>(stepsCount);
    if (level >= 256.0 - stepSize)
        return stepsCount - 1;
    return static_cast<int>(level / stepSize);
}

bool verifyPermutation(int stepsCount, int target)
{
    std::set<int> values;
    for (int position = 0; position < stepsCount; ++position)
    {
        const int logical = SoundSwitchPerformance::shuffledStepAt(
            position, stepsCount, target);
        values.insert(logical);
        if (SoundSwitchPerformance::shuffledPositionForStep(
                logical, stepsCount, target) != position)
            return false;
    }
    return static_cast<int>(values.size()) == stepsCount &&
           *values.begin() == 0 && *values.rbegin() == stepsCount - 1;
}
}

int main()
{
    for (int target = 0; target < 4; ++target)
    {
        if (!verifyPermutation(32, target))
        {
            std::cerr << "32-step randomized cycle is not invertible\n";
            return 1;
        }
        for (int pad = 0; pad < 32; ++pad)
        {
            const uchar sequential = SoundSwitchPerformance::autoplaySeekValue(
                target, pad, false, false);
            if (decodedCueStep(sequential, 32) != pad)
            {
                std::cerr << "sequential bank seek selected the wrong loop\n";
                return 2;
            }

            const uchar randomized = SoundSwitchPerformance::autoplaySeekValue(
                target, pad, false, true);
            const int position = decodedCueStep(randomized, 32);
            if (SoundSwitchPerformance::shuffledStepAt(
                    position, 32, target) != pad)
            {
                std::cerr << "randomized bank seek did not start requested loop\n";
                return 3;
            }
            if (decodedCueStep(
                    SoundSwitchPerformance::autoplaySeekNudgeValue(
                        randomized, false), 32) != position)
            {
                std::cerr << "32-step repeat nudge changed the loop\n";
                return 4;
            }
        }
    }

    if (!verifyPermutation(128, 4))
    {
        std::cerr << "128-step randomized cycle is not invertible\n";
        return 5;
    }
    for (int bank = 0; bank < 4; ++bank)
    {
        for (int pad = 0; pad < 32; ++pad)
        {
            const int logical = bank * 32 + pad;
            const uchar randomized = SoundSwitchPerformance::autoplaySeekValue(
                bank, pad, true, true);
            const int position = decodedCueStep(randomized, 128);
            if (SoundSwitchPerformance::shuffledStepAt(
                    position, 128, 4) != logical)
            {
                std::cerr << "randomized all-bank seek selected wrong loop\n";
                return 6;
            }
            if (decodedCueStep(
                    SoundSwitchPerformance::autoplaySeekNudgeValue(
                        randomized, true), 128) != position)
            {
                std::cerr << "128-step repeat nudge changed the loop\n";
                return 7;
            }
        }
    }

    std::cout << "PASS: exact sequential/randomized seek and repeat nudge\n";
    return 0;
}
