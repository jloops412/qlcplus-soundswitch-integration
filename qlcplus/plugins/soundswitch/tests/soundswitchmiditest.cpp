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

int lastPadLed(int pad);

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
    // Inject a Function Flash on/off report to test translator bookkeeping.
    // This does not execute QLC+'s engine or prove native StopAll unFlashes.
    rig.feedback(36, 255); rig.feedback(36, 0);
    rig.clear(); rig.tap(36);
    check(rig.count(36, 255) == 1, "color latch ignored Function off feedback");
    rig.note(52, true); rig.note(0, true); rig.note(52, false); rig.note(0, false);
    check(rig.count(128, 255) == 1 && rig.count(128, 0) == 0,
          "position latch released with physical Note Off");
    rig.feedback(128, 255); rig.feedback(128, 0);
    rig.clear(); rig.feedback(860, 255); rig.feedback(860, 0);
    check(rig.count(128, 255) == 1, "mouse position ignored Function off feedback");
}

void stopReleasesEveryFlashBeforeNativeStop()
{
    Rig rig;
    rig.feedback(338, 255); // Simulate a running native Auto All owner.
    rig.tap(36);
    rig.note(52, true); rig.tap(45); rig.tap(0); rig.note(52, false);
    rig.note(45, true);
    rig.note(52, true); rig.note(41, true); rig.note(52, false);
    rig.clear();
    rig.feedback(817, 255);
    check(rig.events.size() > 2 && rig.events[0] == Event(510, 255) &&
          rig.events[1] == Event(510, 0),
          "STOP did not select Live before releasing Flash owners");
    const auto stop = std::find(rig.events.begin(), rig.events.end(), Event(818, 255));
    check(stop != rig.events.end(), "STOP did not request native StopAll");
    for (const auto range : {std::pair<int, int>{36, 44}, {164, 172},
                             {45, 47}, {173, 175}, {128, 136}})
    {
        for (int channel = range.first; channel <= range.second; ++channel)
        {
            const auto release = std::find(rig.events.begin(), stop, Event(channel, 0));
            check(release != stop, "native StopAll preceded a required Flash release");
            check(rig.count(channel, 0) == 1 && rig.count(channel, 255) == 0,
                  "STOP failed to release each Flash channel exactly once");
        }
    }
    check(rig.count(818, 255) == 1 && rig.events.back() == Event(818, 0),
          "native StopAll was duplicated or was not the final command");
    check(SoundSwitchMidiTestAccess::running(rig.input),
          "STOP invented stopped state before native Function feedback");
    check(rig.count(469, 255) == 0,
          "STOP emitted synthetic stopped feedback as proof of engine state");
    rig.clear(); rig.feedback(817, 0);
    check(rig.events.empty(), "STOP command Scene ending dispatched a second stop");
    rig.feedback(338, 0); // Simulated native owner acknowledgement, not engine execution.
    check(!SoundSwitchMidiTestAccess::running(rig.input),
          "STOP ignored the native playback owner's off feedback");
}

void stopIsIdempotentAndCancelsHeldKeys()
{
    Rig rig;
    rig.note(52, true);
    for (int note : {0, 36, 45, 46, 47}) rig.note(note, true);
    rig.clear(); rig.feedback(817, 255);
    const auto firstStop = rig.events;
    rig.clear(); rig.feedback(817, 255);
    check(rig.events == firstStop, "repeated STOP did not repeat the safe release contract");
    rig.clear();
    for (int note : {0, 36, 45, 46, 47})
    {
        rig.note(note, true); // Echo while the physical key is still down.
        rig.note(note, false);
        rig.note(note, false); // Duplicate/orphan release.
    }
    check(rig.events.empty(), "late held-key events after STOP re-armed or altered an owner");
    rig.note(52, false);
    check(rig.events == std::vector<Event>{{52,0}},
          "STOP lost physical Shift before its actual release");
    rig.clear();
    rig.note(52, true);
    for (int note : {0, 45, 46, 47}) rig.tap(note);
    rig.note(52, false);
    check(rig.count(128, 255) == 1 && rig.count(128, 0) == 0,
          "STOP left stale position-latch translation state");
    for (int channel : {173, 174, 175})
        check(rig.count(channel, 255) == 1 && rig.count(channel, 0) == 0,
              "STOP left stale White/Black/UV latch translation state");
    rig.clear(); rig.tap(36);
    check(rig.count(36, 255) == 1 && rig.count(36, 0) == 0,
          "color could not latch on the first new press after STOP");
}

void hardwareAndMouseStopUseTheSameRoute()
{
    Rig mouse;
    mouse.feedback(338, 255);
    mouse.note(52, true); mouse.note(45, true);
    mouse.clear(); mouse.feedback(817, 255);
    const auto mouseStop = mouse.events;

    Rig hardware;
    hardware.feedback(338, 255);
    hardware.note(52, true); hardware.note(45, true);
    hardware.clear(); hardware.note(51, true); // Shift + Play/Pause is STOP.
    check(hardware.events == std::vector<Event>{{510,255}, {510,0}, {819,255}, {819,0}},
          "hardware STOP did not request the native command Scene on Live");
    check(hardware.count(818, 255) == 0 && SoundSwitchMidiTestAccess::running(hardware.input),
          "hardware STOP bypassed native start-queue acknowledgement");
    hardware.clear(); hardware.note(51, true); // Echo while awaiting native acknowledgement.
    check(hardware.events.empty(), "pending hardware STOP dispatched duplicate requests");
    // Simulated running-monitor acknowledgement tests translator ordering only.
    // The separate pinned-engine harness proves the real start-queue barrier.
    hardware.feedback(817, 255);
    check(hardware.events == mouseStop, "acknowledged hardware STOP did not match mouse/OS2L STOP");
    check(hardware.count(179, 255) == 0 && hardware.count(338, 255) == 0,
          "hardware STOP also dispatched an old gesture or toggled playback");
    hardware.clear();
    hardware.note(51, true); hardware.note(51, false);
    hardware.note(45, false); hardware.note(52, false);
    check(hardware.events == std::vector<Event>{{52,0}},
          "hardware STOP release/echo changed an owner or lost physical Shift");

    Rig pause;
    pause.feedback(338, 255); pause.clear(); pause.tap(51);
    check(pause.count(338, 255) == 1 && pause.count(818, 255) == 0,
          "unshifted Play/Pause was incorrectly changed into STOP");
}

void repeatedStopPreservesHeldShift()
{
    Rig rig;
    rig.feedback(338, 255);
    rig.note(52, true);
    for (int attempt = 0; attempt < 4; ++attempt)
    {
        rig.clear(); rig.tap(51);
        check(rig.events == std::vector<Event>{{510,255}, {510,0}, {819,255}, {819,0}},
              "repeated Shift+Play became playback instead of a native STOP request");
        rig.feedback(817, 255); // Simulated native monitor acknowledgement.
        check(rig.count(818, 255) == 1 && rig.count(52, 0) == 0,
              "STOP released the physically held Shift modifier");
        rig.feedback(338, 0);
    }
    rig.clear(); rig.note(52, false);
    check(rig.events == std::vector<Event>{{52,0}}, "actual Shift release was lost after repeated STOP");
    rig.clear(); rig.tap(51);
    check(rig.count(338, 255) == 1 && rig.count(819, 255) == 0,
          "Play/Pause stayed shifted after the physical Shift key was released");
}

void performanceHoldFeedbackDoesNotClearLatches()
{
    for (int note : {45, 46, 47})
    {
        Rig rig;
        SoundSwitchMidiTestBackend::present = true;
        rig.note(52, true); rig.tap(note); rig.note(52, false);
        rig.feedback(128 + note, 255); // Separate native latch Scene is active.
        check(lastPadLed(note) == 127, "performance latch feedback did not light its hardware LED");
        rig.note(note, true); rig.feedback(note, 255);
        rig.note(note, false); rig.feedback(note, 0); // Native momentary Scene ends.
        check(lastPadLed(note) == 127, "hold release darkened the still-latched performance LED");
        rig.clear(); rig.note(52, true); rig.tap(note); rig.note(52, false);
        check(rig.count(128 + note, 0) == 1 && rig.count(128 + note, 255) == 0,
              "ordinary hold feedback cleared independent performance latch state");

        rig.clear(); rig.feedback(128 + note, 255); // Mouse/native latch feedback.
        rig.feedback(note, 255); rig.feedback(note, 0); // Mouse hold and release.
        rig.note(52, true); rig.tap(note); rig.note(52, false);
        check(rig.count(128 + note, 0) == 1 && rig.count(128 + note, 255) == 0,
              "mouse hold release cleared independent performance latch state");
        rig.feedback(note, 255); rig.feedback(128 + note, 0);
        check(lastPadLed(note) == 127, "latch release darkened the still-held performance LED");
        SoundSwitchMidiTestAccess::restoreLeds(rig.input);
        check(lastPadLed(note) == 127, "feedback reconnect failed to restore active hold LED");
        rig.feedback(note, 0);
        check(lastPadLed(note) == 0, "performance LED stayed lit after both owners released");
    }
}

void stopWorksAfterUnplugAndDoesNotRestoreOverlays()
{
    Rig rig;
    SoundSwitchMidiTestBackend::present = true;
    check(rig.input.open(), "fake input did not open for STOP unplug test");
    rig.note(52, true); rig.tap(45); rig.note(52, false);
    rig.note(45, true);
    rig.note(52, true); rig.note(41, true); rig.note(52, false);
    rig.clear(); SoundSwitchMidiTestAccess::disconnect(rig.input); rig.flush();
    check(rig.count(45, 0) == 1 && rig.count(169, 0) == 1 && rig.count(173, 0) == 0,
          "unplug did not release holds independently of established latches");
    rig.clear(); rig.feedback(817, 255);
    check(rig.count(173, 0) == 1 && rig.count(818, 255) == 1,
          "mouse STOP could not release latches after MIDI unplug");
    rig.clear();
    check(rig.input.open(), "fake input did not reconnect after STOP"); rig.flush();
    check(std::none_of(rig.events.begin(), rig.events.end(), [](const Event &event) {
        return event.second != 0 && ((event.first >= 36 && event.first <= 47) ||
               (event.first >= 128 && event.first <= 175));
    }), "reconnecting after STOP re-armed a canceled overlay");
    rig.clear(); rig.note(52, true); rig.tap(45); rig.note(52, false);
    check(rig.count(173, 255) == 1 && rig.count(173, 0) == 0,
          "first new latch after STOP/reconnect used stale translation state");
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
        {"STOP releases all Flash owners before native StopAll", stopReleasesEveryFlashBeforeNativeStop},
        {"STOP idempotence and held-key cancellation", stopIsIdempotentAndCancelsHeldKeys},
        {"hardware and mouse STOP share one route", hardwareAndMouseStopUseTheSameRoute},
        {"repeated STOP preserves physically held Shift", repeatedStopPreservesHeldShift},
        {"performance hold/latch feedback independence", performanceHoldFeedbackDoesNotClearLatches},
        {"STOP after unplug and reconnect", stopWorksAfterUnplugAndDoesNotRestoreOverlays},
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
