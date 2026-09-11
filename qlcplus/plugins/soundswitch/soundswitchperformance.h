/*
  Q Light Controller Plus
  soundswitchperformance.h

  Licensed under the Apache License, Version 2.0.
*/

#ifndef SOUNDSWITCHPERFORMANCE_H
#define SOUNDSWITCHPERFORMANCE_H

#include <QtGlobal>

namespace SoundSwitchPerformance
{
/**
 * Return the logical step stored at a position in the stable, non-repeating
 * randomized cycle used by V30. The supported sizes are 32 and 128.
 */
int shuffledStepAt(int position, int stepsCount, int target);

/** Return the randomized-cycle position containing a logical step. */
int shuffledPositionForStep(int logicalStep, int stepsCount, int target);

/**
 * Encode a Control One bank/pad selection for QLC+ Cue List Steps mode.
 * In randomized mode the selected logical loop is translated to its position
 * in the stable randomized cycle, so the requested loop always runs first.
 */
uchar autoplaySeekValue(int bank, int pad, bool allBanks, bool randomized);

/**
 * Return a second MIDI value inside the same QLC+ Cue List step bucket.
 * Sending this before autoplaySeekValue makes repeated seeks to the same loop
 * observable without ever selecting a different loop.
 */
uchar autoplaySeekNudgeValue(uchar seekValue, bool allBanks);
}

#endif
