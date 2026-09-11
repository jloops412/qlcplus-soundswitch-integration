/*
  Q Light Controller Plus
  soundswitchintensitytest.cpp

  Copyright (c) 2026 QLC+ SoundSwitch Integration contributors

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
*/

#include "../soundswitchintensity.h"

#include <array>
#include <cstdint>
#include <cstdlib>

namespace
{
using SoundSwitchIntensity::ChannelSpans;
using SoundSwitchIntensity::Levels;

constexpr std::size_t kUniverseSize = 512;

bool isMapped(std::size_t channel, std::size_t group)
{
    for (const auto &span : ChannelSpans)
    {
        if (span.group == group && channel >= span.first &&
            channel < span.first + span.count)
        {
            return true;
        }
    }
    return false;
}

bool mapIsSafe()
{
    std::array<int, kUniverseSize> owners{};
    std::array<std::size_t, SoundSwitchIntensity::TargetCount> counts{};
    owners.fill(-1);
    for (const auto &span : ChannelSpans)
    {
        if (span.count == 0 || span.first + span.count > kUniverseSize ||
            span.group < SoundSwitchIntensity::Group1Target ||
            span.group > SoundSwitchIntensity::Group4Target)
        {
            return false;
        }
        for (std::size_t channel = span.first;
             channel < span.first + span.count; ++channel)
        {
            if (owners[channel] != -1)
                return false;
            owners[channel] = static_cast<int>(span.group);
            ++counts[span.group];
        }
    }

    return counts[SoundSwitchIntensity::Group1Target] == 4 &&
        counts[SoundSwitchIntensity::Group2Target] == 36 &&
        counts[SoundSwitchIntensity::Group3Target] == 160 &&
        counts[SoundSwitchIntensity::Group4Target] == 4 &&
        isMapped(0, SoundSwitchIntensity::Group1Target) &&
        isMapped(10, SoundSwitchIntensity::Group1Target) &&
        isMapped(20, SoundSwitchIntensity::Group1Target) &&
        isMapped(30, SoundSwitchIntensity::Group1Target) &&
        isMapped(44, SoundSwitchIntensity::Group2Target) &&
        isMapped(79, SoundSwitchIntensity::Group2Target) &&
        isMapped(89, SoundSwitchIntensity::Group4Target) &&
        isMapped(91, SoundSwitchIntensity::Group4Target) &&
        isMapped(107, SoundSwitchIntensity::Group4Target) &&
        isMapped(109, SoundSwitchIntensity::Group4Target) &&
        isMapped(174, SoundSwitchIntensity::Group3Target) &&
        isMapped(333, SoundSwitchIntensity::Group3Target) &&
        !isMapped(40, SoundSwitchIntensity::Group2Target) &&
        !isMapped(43, SoundSwitchIntensity::Group2Target) &&
        !isMapped(88, SoundSwitchIntensity::Group4Target) &&
        !isMapped(90, SoundSwitchIntensity::Group4Target) &&
        !isMapped(106, SoundSwitchIntensity::Group4Target) &&
        !isMapped(108, SoundSwitchIntensity::Group4Target);
}

bool identityAtFull()
{
    std::array<std::uint8_t, kUniverseSize> frame{};
    for (std::size_t channel = 0; channel < frame.size(); ++channel)
        frame[channel] = static_cast<std::uint8_t>((channel * 73 + 19) & 0xffU);
    const auto original = frame;
    Levels levels{{255, 255, 255, 255, 255, 255}};
    SoundSwitchIntensity::scaleFrame(frame.data(), frame.size(), levels);
    return frame == original;
}

bool globalBlackoutIsEmitterOnly()
{
    std::array<std::uint8_t, kUniverseSize> frame{};
    frame.fill(201);
    Levels levels{{0, 255, 255, 255, 255, 255}};
    SoundSwitchIntensity::scaleFrame(frame.data(), frame.size(), levels);

    for (std::size_t channel = 0; channel < frame.size(); ++channel)
    {
        bool mapped = false;
        for (std::size_t group = SoundSwitchIntensity::Group1Target;
             group <= SoundSwitchIntensity::Group4Target; ++group)
        {
            mapped = mapped || isMapped(channel, group);
        }
        if (frame[channel] != (mapped ? 0 : 201))
            return false;
    }
    return true;
}

bool groupsAreIsolated()
{
    for (std::size_t selected = SoundSwitchIntensity::Group1Target;
         selected <= SoundSwitchIntensity::Group4Target; ++selected)
    {
        std::array<std::uint8_t, kUniverseSize> frame{};
        frame.fill(177);
        Levels levels{{255, 255, 255, 255, 255, 255}};
        levels[selected] = 0;
        SoundSwitchIntensity::scaleFrame(frame.data(), frame.size(), levels);

        for (std::size_t channel = 0; channel < frame.size(); ++channel)
        {
            const std::uint8_t expected = isMapped(channel, selected) ? 0 : 177;
            if (frame[channel] != expected)
                return false;
        }
    }
    return true;
}

bool roundingAndShortFramesAreDeterministic()
{
    std::array<std::uint8_t, kUniverseSize> frame{};
    frame.fill(201);
    Levels levels{{173, 255, 91, 255, 255, 255}};
    SoundSwitchIntensity::scaleFrame(frame.data(), frame.size(), levels);
    // Global 173 x Group 2 level 91 rounds to scale 62; source 201 then
    // rounds to output 49. Group 1 uses the global level directly -> 136.
    if (frame[44] != 49 || frame[79] != 49 || frame[0] != 136 ||
        frame[40] != 201 || frame[80] != 201)
    {
        return false;
    }

    std::array<std::uint8_t, 45> shortFrame{};
    shortFrame.fill(200);
    levels = Levels{{0, 255, 255, 255, 255, 255}};
    SoundSwitchIntensity::scaleFrame(
        shortFrame.data(), shortFrame.size(), levels);
    return shortFrame[0] == 0 && shortFrame[10] == 0 &&
        shortFrame[20] == 0 && shortFrame[30] == 0 &&
        shortFrame[40] == 200 && shortFrame[43] == 200 &&
        shortFrame[44] == 0;
}

bool softwareFallbackIsPageAware()
{
    Levels levels{{255, 255, 255, 255, 255, 255}};
    int target = 0;

    if (!SoundSwitchIntensity::applySurfaceFeedback(
            507U, 255U, target, levels) || target != 4)
    {
        return false;
    }
    if (SoundSwitchIntensity::applySurfaceFeedback(
            506U, 0U, target, levels) || target != 4)
    {
        return false;
    }

    const std::uint32_t group4Slider =
        (4U << 16) | SoundSwitchIntensity::TouchStripChannel;
    if (!SoundSwitchIntensity::applySurfaceFeedback(
            group4Slider, 0U, target, levels) || target != 4 || levels[4] != 0)
    {
        return false;
    }

    const std::uint32_t group2Slider =
        (2U << 16) | SoundSwitchIntensity::TouchStripChannel;
    if (!SoundSwitchIntensity::applySurfaceFeedback(
            group2Slider, 93U, target, levels) || target != 2 || levels[2] != 93)
    {
        return false;
    }

    const auto before = levels;
    const std::uint32_t invalidPage =
        (6U << 16) | SoundSwitchIntensity::TouchStripChannel;
    return !SoundSwitchIntensity::applySurfaceFeedback(
               invalidPage, 17U, target, levels) &&
        !SoundSwitchIntensity::applySurfaceFeedback(
               510U, 17U, target, levels) &&
        target == 2 && levels == before;
}
}

int main()
{
    return mapIsSafe() && identityAtFull() && globalBlackoutIsEmitterOnly() &&
        groupsAreIsolated() && roundingAndShortFramesAreDeterministic() &&
        softwareFallbackIsPageAware()
        ? EXIT_SUCCESS
        : EXIT_FAILURE;
}
