/*
  Q Light Controller Plus
  soundswitchprioritytest.cpp

  Licensed under the Apache License, Version 2.0.
*/

#include "../soundswitchpriority.h"

#include <iostream>

int main()
{
    SoundSwitchPriorityState state;
    const QByteArray base("base");
    const QByteArray first("first");
    const QByteArray stale("stale");
    const QByteArray second("second");

    if (state.compose(base) != base)
    {
        std::cerr << "inactive priority state replaced base frame\n";
        return 1;
    }

    state.setFrame(stale);
    state.updateLook(600U, 255U);
    if (!state.active() || state.hasFrame() || state.compose(base) != base)
    {
        std::cerr << "new priority ownership exposed a stale frame\n";
        return 2;
    }

    state.setFrame(first);
    if (state.compose(base) != first)
    {
        std::cerr << "active priority frame did not own output\n";
        return 3;
    }

    // QLC+ may report the replacement Look before releasing the old one.
    state.updateLook(601U, 255U);
    if (!state.active() || state.hasFrame() || state.compose(base) != base)
    {
        std::cerr << "replacement Look exposed the previous frame\n";
        return 4;
    }
    state.setFrame(first);
    state.updateLook(600U, 0U);
    if (!state.active() || !state.contains(601U) || state.activeCount() != 1 ||
        state.compose(base) != first)
    {
        std::cerr << "remaining priority owner was lost\n";
        return 5;
    }

    state.updateLook(601U, 0U);
    if (state.active() || state.hasFrame() || state.compose(base) != base)
    {
        std::cerr << "priority release did not restore base frame\n";
        return 6;
    }

    state.setFrame(stale);
    state.updateLook(602U, 255U);
    state.setFrame(second);
    if (state.compose(base) != second)
    {
        std::cerr << "second priority activation did not use fresh frame\n";
        return 7;
    }

    std::cout << "PASS: priority ownership, stale-frame guard, base restore\n";
    return 0;
}
