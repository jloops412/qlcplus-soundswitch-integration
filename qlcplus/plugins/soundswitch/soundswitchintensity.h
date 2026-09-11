/*
  Q Light Controller Plus
  soundswitchintensity.h

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

#ifndef SOUNDSWITCHINTENSITY_H
#define SOUNDSWITCHINTENSITY_H

#include <array>
#include <cstddef>
#include <cstdint>

namespace SoundSwitchIntensity
{
constexpr std::size_t TargetCount = 6;
constexpr int GlobalTarget = 0;
constexpr int Group1Target = 1;
constexpr int Group2Target = 2;
constexpr int Group3Target = 3;
constexpr int Group4Target = 4;
constexpr int ScriptedTarget = 5;

constexpr std::uint32_t SelectorChannelBase = 503;
constexpr std::uint32_t TouchStripChannel = 511;

constexpr std::size_t WashFxHexBase = 40;
constexpr std::size_t WashFxHexChannels = 40;
constexpr std::size_t FocusSpotTwoLeftBase = 80;
constexpr std::size_t FocusSpotTwoRightBase = 98;
constexpr std::size_t FocusSpotTwoChannels = 18;
constexpr std::size_t TubeBase = 174;

static_assert(WashFxHexBase + WashFxHexChannels == FocusSpotTwoLeftBase);
static_assert(FocusSpotTwoLeftBase + FocusSpotTwoChannels ==
              FocusSpotTwoRightBase);
static_assert(FocusSpotTwoRightBase + FocusSpotTwoChannels <= TubeBase);

using Levels = std::array<std::uint8_t, TargetCount>;

struct ChannelSpan
{
    std::size_t first;
    std::size_t count;
    std::size_t group;
};

// QLC and the workspace store DMX addresses as zero-based indexes. Only raw
// emitter or dimmer bytes belong here; mode, shutter, movement and optical
// control bytes must remain unscaled.
constexpr std::array<ChannelSpan, 10> ChannelSpans{{
    {0, 1, Group1Target},
    {10, 1, Group1Target},
    {20, 1, Group1Target},
    {30, 1, Group1Target},
    {WashFxHexBase + 4, 36, Group2Target},
    {FocusSpotTwoLeftBase + 9, 1, Group4Target},
    {FocusSpotTwoLeftBase + 11, 1, Group4Target},
    {FocusSpotTwoRightBase + 9, 1, Group4Target},
    {FocusSpotTwoRightBase + 11, 1, Group4Target},
    {TubeBase, 4 * 40, Group3Target}
}};

void scaleFrame(std::uint8_t *data, std::size_t size,
                const Levels &levels);

// Applies page-aware Virtual Console feedback to the same state used by the
// physical touch strip. Returns true only when the channel is an intensity
// selector or one of the six page-coded intensity sliders.
bool applySurfaceFeedback(std::uint32_t channel, std::uint8_t value,
                          int &target, Levels &levels);
}

#endif
