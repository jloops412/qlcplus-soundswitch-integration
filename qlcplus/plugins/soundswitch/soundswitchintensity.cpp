/*
  Q Light Controller Plus
  soundswitchintensity.cpp

  Copyright (c) 2026 QLC+ SoundSwitch Integration contributors

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include "soundswitchintensity.h"

namespace SoundSwitchIntensity
{
void scaleFrame(std::uint8_t *data, std::size_t size,
                const Levels &levels)
{
    if (data == nullptr || size == 0)
        return;

    const int global = levels[GlobalTarget];
    for (const ChannelSpan &span : ChannelSpans)
    {
        const int groupScale =
            (global * levels[span.group] + 127) / 255;
        const std::size_t end = span.first < size
            ? (span.count < size - span.first
                ? span.first + span.count
                : size)
            : span.first;
        for (std::size_t channel = span.first; channel < end; ++channel)
        {
            const int source = data[channel];
            data[channel] = static_cast<std::uint8_t>(
                (source * groupScale + 127) / 255);
        }
    }
}

bool applySurfaceFeedback(std::uint32_t channel, std::uint8_t value,
                          int &target, Levels &levels)
{
    const std::uint32_t logicalChannel = channel & 0xffffU;
    const std::uint32_t inputPage = (channel >> 16) & 0xffffU;

    if (logicalChannel >= SelectorChannelBase &&
        logicalChannel < SelectorChannelBase + TargetCount)
    {
        // A QLC button's release sends a trailing zero. Only its positive
        // edge selects a target, otherwise the old page would win again as a
        // SoloFrame turns it off.
        if (value == 0)
            return false;
        target = static_cast<int>(logicalChannel - SelectorChannelBase);
        return true;
    }

    if (logicalChannel != TouchStripChannel || inputPage >= TargetCount)
        return false;

    // Each of the six stacked sliders uses the same low logical channel and
    // encodes its page in the high 16 bits. Accept zero as a real intensity.
    target = static_cast<int>(inputPage);
    levels[inputPage] = value;
    return true;
}
}
