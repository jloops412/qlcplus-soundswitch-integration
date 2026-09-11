/* Tests execute the actual MIDI translator with Qt's queued event delivery. */
#include "../soundswitchmidiinput.h"
#include "../soundswitchperformance.h"

#include <QCoreApplication>
#include <QEvent>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

struct SoundSwitchMidiTestAccess
{
    static void note(SoundSwitchMidiInput &input, int note, bool down)
    { input.handleShortMessage((down ? 0x90U : 0x80U) | (note << 8) | (down ? 127U << 16 : 0)); }
    static void disconnect(SoundSwitchMidiInput &input)
    { input.handleInputDisconnected(input.m_handle); }
    static bool running(const SoundSwitchMidiInput &input) { return input.m_playbackRunning; }
    static void restoreLeds(SoundSwitchMidiInput &input) { input.restoreHardwareFeedback(); }
    static void post(SoundSwitchMidiInput &input, quint32 channel, uchar value)
    { input.postValue(channel, value); }
};

namespace
{
using Event = std::pair<quint32, uchar>;
void check(bool result, const char *message)
{ if (!result) throw std::runtime_error(message); }

struct Rig
{
    SoundSwitchMidiInput input;
    std::vector<Event> events;
    Rig()
    {
        SoundSwitchMidiTestBackend::reset();
        QObject::connect(&input, &SoundSwitchMidiInput::valueChanged,
                         [&events = events](quint32 channel, uchar value) {
                             events.emplace_back(channel, value);
                         });
    }
    void flush()
    { for (int pass = 0; pass < 8; ++pass) QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall); }
    void clear() { flush(); events.clear(); }
    void note(int note, bool down) { SoundSwitchMidiTestAccess::note(input, note, down); flush(); }
    void tap(int note) { this->note(note, true); this->note(note, false); }
    void feedback(quint32 channel, uchar value) { input.applyFeedback(channel, value); flush(); }
    int count(quint32 channel, uchar value) const
    {
        int count = 0;
        for (const auto &event : events) if (event == Event(channel, value)) ++count;
        return count;
    }
};

void notePairsAndHolds()
{
    Rig rig;
    rig.note(2, true);
    rig.note(2, true);
    rig.note(2, false);
    check(rig.count(2, 255) == 1, "manual note press/echo/release toggled more than once");
    rig.clear();
    rig.note(52, true);
    rig.note(36, true);
    rig.note(52, false);
    rig.note(36, false);
    check(rig.count(164, 255) == 1 && rig.count(164, 0) == 1,
          "releasing Shift before color lost shifted Note Off");
    check(rig.count(36, 0) == 0, "hold release altered normal color latch");
}

void disconnectReleasesMomentary()
{
    for (bool staleHandle : {false, true})
    {
        Rig rig;
        SoundSwitchMidiTestBackend::present = true;
        check(rig.input.open(), "fake input did not open");
        rig.flush(); rig.clear();
        rig.tap(36); // latched red remains underneath held blue
        rig.note(52, true);
        rig.note(41, true);
        rig.note(52, false);
        rig.note(45, true); // ordinary white hold
        rig.clear();
        if (staleHandle)
        {
            SoundSwitchMidiTestBackend::present = false;
            rig.input.ensureConnected();
        }
        else SoundSwitchMidiTestAccess::disconnect(rig.input);
        rig.flush();
        check(rig.count(169, 0) == 1 && rig.count(45, 0) == 1,
              "disconnect left a color or white hold active");
        check(rig.count(36, 0) == 0, "disconnect released a show latch");
    }
}

void autoplayOwnership()
{
    Rig rig;
    rig.feedback(338, 255); // mouse starts Auto All from a fresh session
    rig.clear(); rig.tap(51);
    check(rig.count(338, 255) == 1, "Play/Pause cannot stop mouse-started Auto All");
    rig.feedback(338, 0);
    rig.clear(); rig.tap(51);
    check(rig.count(338, 255) == 1, "Play/Pause cannot resume Auto All");
    rig.feedback(338, 255);
    rig.feedback(339, 255); // new random owner before old sequential stops
    rig.feedback(338, 0);
    check(SoundSwitchMidiTestAccess::running(rig.input), "old owner off stopped replacement owner state");

    Rig bank;
    bank.feedback(332, 255); // Bank 2
    bank.feedback(803, 255); // browse Bank 4
    bank.clear(); bank.tap(55);
    check(bank.count(333, 255) == 1 && bank.count(337, 255) == 0,
          "changing order after browsing changed the active Bank scope");

    Rig manual;
    manual.feedback(2, 255);
    manual.feedback(811, 255); // display Priority while manual loop continues
    manual.feedback(2, 0); // QLC Stop must remain authoritative in either mode
    check(!SoundSwitchMidiTestAccess::running(manual.input),
          "manual stop feedback ignored while Priority surface was displayed");
}

void seekMemoryAndMouse()
{
    Rig rig;
    rig.feedback(338, 255);
    rig.clear();
    rig.feedback(900 + 2 * 32 + 7, 255);
    rig.feedback(900 + 2 * 32 + 7, 0);
    const uchar selected = SoundSwitchPerformance::autoplaySeekValue(2, 7, true, false);
    check(rig.count(632, selected) == 1 && rig.count(7, 255) == 0,
          "mouse pad replaced autoplay or handled command trailing zero");
    rig.feedback(800, 255); // browse another bank before pause/resume
    rig.tap(51); rig.feedback(338, 0);
    rig.clear(); rig.tap(51);
    check(rig.count(632, selected) == 1, "resume forgot exact selected bank and pad");

    Rig manual;
    manual.feedback(900 + 3 * 32 + 5, 255);
    manual.feedback(900 + 3 * 32 + 5, 0);
    check(manual.count(35, 255) == 1 && manual.count(5, 255) == 1,
          "mouse manual pad did not dispatch the selected bank exactly once");
}

void latchesFollowFunctionState()
{
    Rig rig;
    rig.feedback(850, 255); rig.feedback(850, 0);
    check(rig.count(36, 255) == 1 && rig.count(36, 0) == 0,
          "mouse color command failed to latch");
    rig.feedback(36, 255); rig.feedback(36, 0); // actual QLC Stop
    rig.clear(); rig.tap(36);
    check(rig.count(36, 255) == 1, "stale color latch survived QLC Stop");
    rig.note(52, true); rig.note(0, true); rig.note(52, false); rig.note(0, false);
    check(rig.count(128, 255) == 1 && rig.count(128, 0) == 0,
          "position latch released with physical Note Off");
    rig.feedback(128, 255); rig.feedback(128, 0);
    rig.clear(); rig.feedback(860, 255); rig.feedback(860, 0);
    check(rig.count(128, 255) == 1, "mouse position could not restart after QLC Stop");
}

void scopeChangesAndReconnect()
{
    {
        Rig bank;
        bank.feedback(332, 255); // Auto Bank 2 owns playback.
        bank.feedback(803, 255); // Browse Bank 4 without changing that owner.
        SoundSwitchMidiTestBackend::present = true;
        check(bank.input.open(), "fake reconnect input did not open");
        bank.flush();
        SoundSwitchMidiTestAccess::disconnect(bank.input);
        bank.clear();
        check(bank.input.open(), "fake reconnect input did not reopen");
        bank.flush();
        check(bank.count(495, 255) == 1 && bank.count(497, 255) == 0,
              "reconnect restored browsed bank instead of active autoplay scope");
        check(bank.count(35, 255) == 1 && bank.count(332, 255) == 0,
              "reconnect changed browsing or restarted the playback owner");
    }

    for (int gesture : {32, 60}) // Shift+Bank1 and Shift+AutoLoop
    {
        Rig rig;
        rig.feedback(338, 255); // Start All.
        rig.feedback(900 + 2 * 32 + 7, 255); // Explicit Bank 3 / pad 8 seek.
        rig.note(52, true);
        rig.tap(gesture); // Change to Bank 1 scope.
        rig.note(52, false);
        rig.feedback(330, 255); // Native owner confirms the new scope.
        rig.clear(); rig.tap(55);
        check(std::none_of(rig.events.begin(), rig.events.end(),
                           [](const Event &event) { return event.first == 632; }),
              "hardware scope change retained a seek from the old scope");

        rig.feedback(331, 255); // Confirm new random order in this same scope.
        rig.feedback(900 + 7, 255); // Choose a new pad in Bank 1.
        rig.clear(); rig.tap(55); // Change only order back to sequential.
        const uchar selected = SoundSwitchPerformance::autoplaySeekValue(0, 7, false, false);
        check(rig.count(632, selected) == 1,
              "same-scope order change discarded the current selected pad");
    }
}

void priorityLedRestore()
{
    Rig rig;
    rig.feedback(338, 255);
    rig.feedback(811, 255);
    rig.feedback(605, 255);
    SoundSwitchMidiTestBackend::present = true;
    SoundSwitchMidiTestBackend::messages.clear();
    SoundSwitchMidiTestAccess::restoreLeds(rig.input);
    const DWORD expected = 0x90U | (5U << 8) | (127U << 16);
    bool found = false;
    for (DWORD message : SoundSwitchMidiTestBackend::messages) if (message == expected) found = true;
    check(found, "Priority pad LED not restored over active autoplay");
}

int lastPadLed(int pad)
{
    for (auto it = SoundSwitchMidiTestBackend::messages.rbegin();
         it != SoundSwitchMidiTestBackend::messages.rend(); ++it)
    {
        if ((*it & 0xffU) == 0x90U && ((*it >> 8) & 0xffU) == static_cast<DWORD>(pad))
            return static_cast<int>((*it >> 16) & 0x7fU);
    }
    return -1;
}

void rawLoopLedFeedback()
{
    Rig rig;
    rig.feedback(338, 255);
    SoundSwitchMidiTestBackend::present = true;
    rig.clear();
    rig.feedback(1100 + 4, 255);
    check(lastPadLed(4) == 127 && rig.events.empty(),
          "raw monitor did not light active pad or emitted playback input");
    SoundSwitchMidiTestAccess::post(rig.input, 1100 + 4, 255);
    rig.flush();
    check(rig.events.empty(), "feedback-only raw channel was admitted as playback input");
    rig.feedback(1100 + 32 + 4, 255); // new bank, same physical pad
    rig.feedback(1100 + 4, 0); // delayed old bank off
    check(lastPadLed(4) == 127 && rig.events.empty(),
          "inactive bank off cleared active Autoplay pad");
    rig.feedback(1100 + 32 + 9, 255);
    rig.feedback(1100 + 32 + 4, 0);
    check(lastPadLed(9) == 127 && lastPadLed(4) == 0 && rig.events.empty(),
          "hardware pad did not follow actual raw Chaser advance");
    rig.feedback(811, 255);
    rig.feedback(605, 255);
    rig.clear();
    rig.feedback(1100 + 64 + 20, 255);
    rig.feedback(1100 + 32 + 9, 0);
    check(lastPadLed(5) == 127 && lastPadLed(20) != 127 && rig.events.empty(),
          "underlying raw feedback overwrote Priority surface LEDs");
    rig.feedback(811, 255);
    check(lastPadLed(20) == 127 && lastPadLed(5) == 0,
          "returning from Priority did not restore actual current Autoplay pad");
    SoundSwitchMidiTestBackend::messages.clear();
    SoundSwitchMidiTestAccess::restoreLeds(rig.input);
    check(lastPadLed(20) == 127, "reconnect forgot native Autoplay pad state");
    rig.clear();
    rig.feedback(1100 + 64 + 20, 0);
    check(lastPadLed(20) == 0 && rig.events.empty(),
          "stopped raw Chaser left a stale pad LED");
}

void momentaryReleaseReturnsToLive()
{
    Rig rig;
    rig.tap(36); // persistent color latch
    rig.note(52, true); rig.note(41, true); rig.note(52, false);
    rig.clear(); rig.note(41, false);
    check(rig.events == std::vector<Event>{{510, 255}, {510, 0}, {169, 0}},
          "momentary release was not queued after returning to Live");
    rig.clear(); rig.tap(36);
    check(rig.count(36, 0) == 1 && rig.count(510, 255) == 0,
          "ordinary persistent latch release changed the selected page");

    Rig disconnected;
    disconnected.note(52, true); disconnected.note(36, true);
    disconnected.note(52, false); disconnected.clear();
    SoundSwitchMidiTestAccess::disconnect(disconnected.input);
    disconnected.flush();
    check(disconnected.events == std::vector<Event>{{510, 255}, {510, 0}, {164, 0}},
          "disconnect cleanup did not restore Live before releasing held color");
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    int failures = 0;
    const std::pair<const char *, void (*)()> cases[] = {
        {"note pairs and Shift release order", notePairsAndHolds},
        {"disconnect momentary cleanup", disconnectReleasesMomentary},
        {"autoplay ownership and scope", autoplayOwnership},
        {"exact seek memory and mouse pads", seekMemoryAndMouse},
        {"color/position latches follow QLC state", latchesFollowFunctionState},
        {"hardware scope changes and reconnect", scopeChangesAndReconnect},
        {"Priority LED restoration", priorityLedRestore},
        {"native raw-loop LED feedback", rawLoopLedFeedback},
        {"momentary release page routing", momentaryReleaseReturnsToLive},
    };
    for (const auto &test : cases)
    {
        try
        {
            test.second();
            std::cout << "PASS: " << test.first << '\n';
        }
        catch (const std::exception &error)
        {
            ++failures;
            std::cerr << "FAIL: " << test.first << ": " << error.what() << '\n';
        }
    }
    return failures == 0 ? 0 : 1;
}
