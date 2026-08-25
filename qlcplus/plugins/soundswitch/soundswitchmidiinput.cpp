/*
  Q Light Controller Plus
  soundswitchmidiinput.cpp

  Licensed under the Apache License, Version 2.0.
*/

#include "soundswitchmidiinput.h"

#include <QMetaObject>
#include <QMutexLocker>
#include <QString>

namespace
{
constexpr quint8 kShiftNote = 52;
constexpr quint32 kShiftedNoteBase = 128;
constexpr quint32 kControllerBase = 256;
constexpr quint32 kShiftedControllerBase = 384;
constexpr quint32 kAutoplayVariantBase = 330;
constexpr quint32 kAutoplayConfigBase = 480;
constexpr quint32 kPlaybackStoppedChannel = 469;
constexpr quint32 kSpeedPresetBase = 470;
constexpr quint32 kOrderStateBase = 485;
constexpr quint32 kPerformancePageChannel = 510;
constexpr quint32 kTouchStripChannel = 511;
constexpr quint32 kAutoBankChannelBase = 494;
constexpr quint32 kAutoAllChannel = 498;
constexpr quint32 kManualLoopChannel = 499;
constexpr quint32 kStaticModeChannel = 502;
constexpr quint32 kPriorityLookChannelBase = 600;
constexpr quint32 kPriorityLookChannelCount = 32;
constexpr quint32 kAutoplaySeekChannel = 632;
constexpr quint32 kUiBankChannelBase = 800;
constexpr quint32 kUiDwellChannelBase = 804;
constexpr quint32 kUiPlayPauseChannel = 809;
constexpr quint32 kUiOrderChannel = 810;
constexpr quint32 kUiModeChannel = 811;
constexpr quint32 kUiSpeedChannelBase = 812;
constexpr int kAutoplayMeasureCount = 5;
constexpr int kSpeedPresetCount = 5;

quint32 autoplayConfigChannel(int measureIndex)
{
    return kAutoplayConfigBase + static_cast<quint32>(measureIndex);
}

quint32 autoplayVariantChannel(int bank, bool allBanks, bool randomized)
{
    const int target = allBanks ? 4 : qBound(0, bank, 3);
    return kAutoplayVariantBase + static_cast<quint32>(target * 2)
            + (randomized ? 1U : 0U);
}

uchar autoplaySeekValue(int bank, int pad, bool allBanks)
{
    // QLC+ Cue List step mode runs from 255 (first step) down to 0. Both
    // supported parent sizes divide the 256-value MIDI range exactly.
    const int stepsCount = allBanks ? 128 : 32;
    const int stepIndex = allBanks
        ? qBound(0, bank, 3) * 32 + qBound(0, pad, 31)
        : qBound(0, pad, 31);
    return static_cast<uchar>(255 - stepIndex * (256 / stepsCount));
}

bool isLatchedShiftSpecial(quint8 note)
{
    return note == 45 || note == 46 || note == 47;
}

bool isRelativeTurnControl(quint8 controller)
{
    // Select, Pan/Speed and Tilt/Color are endless turn controls. Pan/Speed
    // and Tilt/Color are touch-sensitive, but none of these turns is a push.
    return controller == 22 || controller == 71 || controller == 72;
}
}

SoundSwitchMidiInput::SoundSwitchMidiInput(QObject *parent)
    : QObject(parent)
{
}

SoundSwitchMidiInput::~SoundSwitchMidiInput()
{
    close();
    closeFeedback();
}

int SoundSwitchMidiInput::findControlOneDevice()
{
    const UINT count = midiInGetNumDevs();
    for (UINT index = 0; index < count; ++index)
    {
        MIDIINCAPSW caps{};
        if (midiInGetDevCapsW(index, &caps, sizeof(caps)) != MMSYSERR_NOERROR)
            continue;

        const QString name = QString::fromWCharArray(caps.szPname);
        if (isControlOneName(name))
            return static_cast<int>(index);
    }
    return -1;
}

int SoundSwitchMidiInput::findControlOneOutputDevice()
{
    const UINT count = midiOutGetNumDevs();
    for (UINT index = 0; index < count; ++index)
    {
        MIDIOUTCAPSW caps{};
        if (midiOutGetDevCapsW(index, &caps, sizeof(caps)) != MMSYSERR_NOERROR)
            continue;

        const QString name = QString::fromWCharArray(caps.szPname);
        if (isControlOneName(name))
            return static_cast<int>(index);
    }
    return -1;
}

bool SoundSwitchMidiInput::isControlOneName(const QString &name)
{
    return name.contains(QStringLiteral("SoundSwitch Control One"),
                         Qt::CaseInsensitive);
}

bool SoundSwitchMidiInput::isPresent()
{
    return findControlOneDevice() >= 0;
}

bool SoundSwitchMidiInput::open()
{
    return ensureConnected();
}

void SoundSwitchMidiInput::releaseHandle(HMIDIIN handle)
{
    if (handle == nullptr)
        return;
    midiInStop(handle);
    midiInReset(handle);
    midiInClose(handle);
}

void SoundSwitchMidiInput::releaseOutputHandle(HMIDIOUT handle)
{
    if (handle != nullptr)
        midiOutClose(handle);
}

bool SoundSwitchMidiInput::ensureConnected()
{
    const int deviceIndex = findControlOneDevice();
    HMIDIIN staleHandle = nullptr;

    {
        QMutexLocker lock(&m_mutex);
        if (m_handle != nullptr)
        {
            UINT handleId = UINT_MAX;
            MIDIINCAPSW caps{};
            const bool handleValid = midiInGetID(m_handle, &handleId)
                    == MMSYSERR_NOERROR
                && midiInGetDevCapsW(handleId, &caps, sizeof(caps))
                    == MMSYSERR_NOERROR;
            const bool sameDevice = handleValid
                && handleId == static_cast<UINT>(deviceIndex)
                && isControlOneName(QString::fromWCharArray(caps.szPname));
            if (deviceIndex >= 0 && sameDevice)
                return true;

            // Windows can leave the old value non-null after a USB reset. It
            // is no longer a usable input handle, so detach it without
            // clearing the performer's bank/mode/latch state.
            staleHandle = m_handle;
            m_handle = nullptr;
        }
    }

    releaseHandle(staleHandle);

    if (deviceIndex < 0)
        return false;

    HMIDIIN handle = nullptr;
    const MMRESULT opened = midiInOpen(
        &handle, static_cast<UINT>(deviceIndex),
        reinterpret_cast<DWORD_PTR>(&SoundSwitchMidiInput::midiCallback),
        reinterpret_cast<DWORD_PTR>(this), CALLBACK_FUNCTION);
    if (opened != MMSYSERR_NOERROR)
        return false;

    if (midiInStart(handle) != MMSYSERR_NOERROR)
    {
        midiInClose(handle);
        return false;
    }

    {
        QMutexLocker lock(&m_mutex);
        if (m_handle == nullptr)
        {
            m_handle = handle;
            QMetaObject::invokeMethod(this, &SoundSwitchMidiInput::restoreLogicalState,
                                      Qt::QueuedConnection);
            return true;
        }
    }

    // A second caller completed the reconnect first.
    releaseHandle(handle);
    return true;
}

bool SoundSwitchMidiInput::ensureFeedbackConnected()
{
    const int deviceIndex = findControlOneOutputDevice();
    HMIDIOUT staleHandle = nullptr;

    {
        QMutexLocker lock(&m_mutex);
        if (m_outputHandle != nullptr)
        {
            UINT handleId = UINT_MAX;
            MIDIOUTCAPSW caps{};
            const bool handleValid = midiOutGetID(m_outputHandle, &handleId)
                    == MMSYSERR_NOERROR
                && midiOutGetDevCapsW(handleId, &caps, sizeof(caps))
                    == MMSYSERR_NOERROR;
            const bool sameDevice = handleValid
                && handleId == static_cast<UINT>(deviceIndex)
                && isControlOneName(QString::fromWCharArray(caps.szPname));
            if (deviceIndex >= 0 && sameDevice)
                return true;
            staleHandle = m_outputHandle;
            m_outputHandle = nullptr;
        }
    }

    releaseOutputHandle(staleHandle);
    if (deviceIndex < 0)
        return false;

    HMIDIOUT handle = nullptr;
    if (midiOutOpen(
            &handle, static_cast<UINT>(deviceIndex),
            reinterpret_cast<DWORD_PTR>(&SoundSwitchMidiInput::midiOutputCallback),
            reinterpret_cast<DWORD_PTR>(this), CALLBACK_FUNCTION)
        != MMSYSERR_NOERROR)
        return false;

    {
        QMutexLocker lock(&m_mutex);
        if (m_outputHandle == nullptr)
        {
            m_outputHandle = handle;
            QMetaObject::invokeMethod(
                this, &SoundSwitchMidiInput::restoreHardwareFeedback,
                Qt::QueuedConnection);
            return true;
        }
    }

    releaseOutputHandle(handle);
    return true;
}

void SoundSwitchMidiInput::resetControllerStateLocked()
{
    m_shiftHeld = false;
    m_staticMode = false;
    m_selectedBank = 0;
    m_autoplayBank = -1;
    m_autoplayActive = false;
    m_autoplayAll = false;
    m_autoplayMeasureIndex = 3;
    m_autoplayRandom = false;
    m_speedIndex = 2;
    m_manualBank = -1;
    m_manualPad = -1;
    m_playbackRunning = false;
    m_transportPaused = false;
    m_latchedStaticNote = -1;
    m_latchedOverrideNote = -1;
    m_intensityTarget = 0;
    m_pressedNotes.clear();
    m_shiftedPressedNotes.clear();
    m_latchedShiftNotes.clear();
    m_controllerValues.clear();
}

void SoundSwitchMidiInput::close()
{
    HMIDIIN handle = nullptr;
    {
        QMutexLocker lock(&m_mutex);
        handle = m_handle;
        m_handle = nullptr;
        resetControllerStateLocked();
    }
    releaseHandle(handle);
}

void SoundSwitchMidiInput::closeFeedback()
{
    HMIDIOUT handle = nullptr;
    {
        QMutexLocker lock(&m_mutex);
        handle = m_outputHandle;
        m_outputHandle = nullptr;
    }
    releaseOutputHandle(handle);
}

bool SoundSwitchMidiInput::sendFeedback(quint32 channel, uchar value)
{
    if (!ensureFeedbackConnected())
        return false;

    const quint32 logicalChannel = channel & 0xffffU;
    quint8 status = 0;
    quint8 data1 = 0;
    if (logicalChannel < 128U)
    {
        status = 0x90;
        data1 = static_cast<quint8>(logicalChannel);
    }
    else if (logicalChannel >= kPriorityLookChannelBase &&
             logicalChannel < kPriorityLookChannelBase + kPriorityLookChannelCount)
    {
        // Priority Look buttons use synthetic, page-independent QLC channels
        // but illuminate the same physical 32 performance pads.
        status = 0x90;
        data1 = static_cast<quint8>(logicalChannel - kPriorityLookChannelBase);
    }
    else if (logicalChannel >= kControllerBase &&
             logicalChannel < kControllerBase + 128U)
    {
        status = 0xb0;
        data1 = static_cast<quint8>(logicalChannel - kControllerBase);
    }
    else
    {
        // Synthetic QLC state/config channels have no direct physical LED.
        return false;
    }

    HMIDIOUT handle = nullptr;
    {
        QMutexLocker lock(&m_mutex);
        handle = m_outputHandle;
    }
    if (handle == nullptr)
        return false;

    const quint8 midiValue = static_cast<quint8>((value * 127U + 127U) / 255U);
    const DWORD message = static_cast<DWORD>(status)
        | (static_cast<DWORD>(data1) << 8)
        | (static_cast<DWORD>(midiValue) << 16);
    if (midiOutShortMsg(handle, message) == MMSYSERR_NOERROR)
        return true;

    // A USB reset can invalidate WinMM's output handle between the liveness
    // check above and the actual LED write. Detach it, reconnect once and
    // retry the same message so feedback recovers without restarting QLC+.
    HMIDIOUT staleHandle = nullptr;
    {
        QMutexLocker lock(&m_mutex);
        if (m_outputHandle == handle)
        {
            staleHandle = m_outputHandle;
            m_outputHandle = nullptr;
        }
    }
    releaseOutputHandle(staleHandle);

    if (!ensureFeedbackConnected())
        return false;

    {
        QMutexLocker lock(&m_mutex);
        handle = m_outputHandle;
    }
    return handle != nullptr
        && midiOutShortMsg(handle, message) == MMSYSERR_NOERROR;
}

bool SoundSwitchMidiInput::isOpen() const
{
    QMutexLocker lock(&m_mutex);
    return m_handle != nullptr;
}

void CALLBACK SoundSwitchMidiInput::midiCallback(HMIDIIN handle, UINT message,
                                                  DWORD_PTR instance,
                                                  DWORD_PTR param1,
                                                  DWORD_PTR param2)
{
    Q_UNUSED(param2)

    if (instance == 0)
        return;

    auto *input = reinterpret_cast<SoundSwitchMidiInput *>(instance);
    if (message == MIM_DATA)
    {
        input->handleShortMessage(static_cast<DWORD>(param1));
    }
    else if (message == MIM_CLOSE)
    {
        QMetaObject::invokeMethod(
            input,
            [input, handle]() { input->handleInputDisconnected(handle); },
            Qt::QueuedConnection);
    }
}

void CALLBACK SoundSwitchMidiInput::midiOutputCallback(HMIDIOUT handle,
                                                        UINT message,
                                                        DWORD_PTR instance,
                                                        DWORD_PTR param1,
                                                        DWORD_PTR param2)
{
    Q_UNUSED(param1)
    Q_UNUSED(param2)

    if (message != MOM_CLOSE || instance == 0)
        return;

    auto *input = reinterpret_cast<SoundSwitchMidiInput *>(instance);
    QMetaObject::invokeMethod(
        input,
        [input, handle]() { input->handleOutputDisconnected(handle); },
        Qt::QueuedConnection);
}

void SoundSwitchMidiInput::handleInputDisconnected(HMIDIIN handle)
{
    QMutexLocker lock(&m_mutex);
    if (m_handle != handle)
        return;

    m_handle = nullptr;
    // Physical down/up pairs cannot be completed after a cable pull. Clear
    // only transient gesture state; keep every logical show selection so the
    // normal two-second rescan can restore it on the replacement handle.
    m_shiftHeld = false;
    m_pressedNotes.clear();
    m_shiftedPressedNotes.clear();
}

void SoundSwitchMidiInput::handleOutputDisconnected(HMIDIOUT handle)
{
    QMutexLocker lock(&m_mutex);
    if (m_outputHandle == handle)
        m_outputHandle = nullptr;
}

void SoundSwitchMidiInput::postValue(quint32 channel, uchar value)
{
    QMetaObject::invokeMethod(
        this,
        [this, channel, value]() { emit valueChanged(channel, value); },
        Qt::QueuedConnection);
}

void SoundSwitchMidiInput::postPulse(quint32 channel)
{
    postValue(channel, UCHAR_MAX);
    postValue(channel, 0);
}

void SoundSwitchMidiInput::postPlaybackState(quint32 channel)
{
    postPulse(channel);
    if (channel == 500U)
        sendFeedback(51U, 255);
    else if (channel == 501U || channel == kPlaybackStoppedChannel)
        sendFeedback(51U, 0);
}

void SoundSwitchMidiInput::restoreLogicalState()
{
    int bank = 0;
    int measureIndex = 0;
    int speedIndex = 0;
    bool staticMode = false;
    bool randomized = false;
    bool running = false;
    bool paused = false;
    bool autoplayActive = false;
    bool autoplayAll = false;
    {
        QMutexLocker lock(&m_mutex);
        bank = m_selectedBank;
        measureIndex = m_autoplayMeasureIndex;
        speedIndex = m_speedIndex;
        staticMode = m_staticMode;
        randomized = m_autoplayRandom;
        running = m_playbackRunning;
        paused = m_transportPaused;
        autoplayActive = m_autoplayActive;
        autoplayAll = m_autoplayAll;
    }

    postPulse(kPerformancePageChannel);
    postPulse(staticMode ? kStaticModeChannel : 60U);
    postPulse(32U + static_cast<quint32>(bank));
    postPulse(autoplayConfigChannel(measureIndex));
    postPulse(kSpeedPresetBase + static_cast<quint32>(speedIndex));
    postPulse(kOrderStateBase + (randomized ? 1U : 0U));
    if (paused)
        postPulse(501U);
    else if (running)
        postPulse(500U);
    else
        postPulse(kPlaybackStoppedChannel);

    if (running && autoplayActive)
        postPulse(autoplayAll ? kAutoAllChannel
                             : kAutoBankChannelBase + static_cast<quint32>(bank));
    else if (running)
        postPulse(kManualLoopChannel);

    restoreHardwareFeedback();
}

void SoundSwitchMidiInput::restoreHardwareFeedback()
{
    int bank = 0;
    int manualPad = -1;
    int staticPad = -1;
    int overrideNote = -1;
    int intensityTarget = 0;
    bool staticMode = false;
    bool randomized = false;
    bool running = false;
    bool autoplayActive = false;
    QSet<quint8> shiftedLatches;
    {
        QMutexLocker lock(&m_mutex);
        bank = m_selectedBank;
        manualPad = m_manualPad;
        staticPad = m_latchedStaticNote;
        overrideNote = m_latchedOverrideNote;
        intensityTarget = m_intensityTarget;
        staticMode = m_staticMode;
        randomized = m_autoplayRandom;
        running = m_playbackRunning;
        autoplayActive = m_autoplayActive;
        shiftedLatches = m_latchedShiftNotes;
    }

    for (int note = 32; note <= 35; ++note)
        sendFeedback(static_cast<quint32>(note), note - 32 == bank ? 255 : 0);
    sendFeedback(51U, running ? 255 : 0);
    sendFeedback(55U, randomized ? 255 : 0);
    sendFeedback(60U, staticMode ? 0 : 255);

    sendFeedback(61U, intensityTarget == 0 || intensityTarget == 5 ? 255 : 0);
    sendFeedback(62U, intensityTarget == 1 || intensityTarget == 2 ? 255 : 0);
    sendFeedback(63U, intensityTarget == 3 || intensityTarget == 4 ? 255 : 0);

    for (int note = 36; note <= 44; ++note)
        sendFeedback(static_cast<quint32>(note), note == overrideNote ? 255 : 0);
    for (int note = 45; note <= 47; ++note)
        sendFeedback(static_cast<quint32>(note),
                     shiftedLatches.contains(static_cast<quint8>(note)) ? 255 : 0);

    // During Autoplay, QLC+'s active child Function remains the authoritative
    // pad indicator. Manual and Priority Look owners are fully known here and
    // can be restored deterministically after a USB reconnect.
    if (!autoplayActive)
    {
        const int activePad = staticMode ? staticPad : (running ? manualPad : -1);
        for (int note = 0; note <= 31; ++note)
            sendFeedback(static_cast<quint32>(note), note == activePad ? 255 : 0);
    }
}

void SoundSwitchMidiInput::setIntensityTarget(int target)
{
    int selectedTarget = 0;
    {
        QMutexLocker lock(&m_mutex);
        m_intensityTarget = qBound(0, target, 5);
        selectedTarget = m_intensityTarget;
    }
    sendFeedback(61U, selectedTarget == 0 || selectedTarget == 5 ? 255 : 0);
    sendFeedback(62U, selectedTarget == 1 || selectedTarget == 2 ? 255 : 0);
    sendFeedback(63U, selectedTarget == 3 || selectedTarget == 4 ? 255 : 0);
}

void SoundSwitchMidiInput::togglePerformanceMode()
{
    bool chooseStatic = false;
    int bank = 0;
    {
        QMutexLocker lock(&m_mutex);
        m_staticMode = !m_staticMode;
        chooseStatic = m_staticMode;
        bank = m_selectedBank;
    }

    postPulse(kPerformancePageChannel);
    postPulse(chooseStatic ? kStaticModeChannel : 60U);
    if (!chooseStatic)
        postPulse(32U + static_cast<quint32>(bank));

    sendFeedback(60U, chooseStatic ? 0 : 255);
}

void SoundSwitchMidiInput::toggleOrder()
{
    int bank = 0;
    bool randomized = false;
    bool active = false;
    bool allBanks = false;
    bool restoreStatic = false;
    {
        QMutexLocker lock(&m_mutex);
        m_autoplayRandom = !m_autoplayRandom;
        bank = m_selectedBank;
        randomized = m_autoplayRandom;
        active = m_autoplayActive;
        allBanks = m_autoplayAll;
        restoreStatic = m_staticMode;
    }

    postPulse(kOrderStateBase + (randomized ? 1U : 0U));
    if (active)
        dispatchAutoplay(bank, allBanks, randomized, restoreStatic);

    sendFeedback(55U, randomized ? 255 : 0);
}

void SoundSwitchMidiInput::togglePlayback()
{
    int bank = -1;
    int pad = -1;
    bool autoplay = false;
    bool allBanks = false;
    bool randomized = false;
    bool restoreStatic = false;
    bool startPlayback = false;
    {
        QMutexLocker lock(&m_mutex);
        autoplay = m_autoplayActive;
        allBanks = m_autoplayAll;
        randomized = m_autoplayRandom;
        restoreStatic = m_staticMode;
        bank = autoplay ? m_autoplayBank : m_manualBank;
        pad = m_manualPad;
        if (bank < 0 || (!autoplay && pad < 0))
        {
            m_playbackRunning = false;
            m_transportPaused = false;
        }
        else
        {
            startPlayback = !m_playbackRunning;
            m_playbackRunning = startPlayback;
            m_transportPaused = !startPlayback;
        }
    }

    if (bank >= 0 && (autoplay || pad >= 0))
    {
        if (autoplay)
            dispatchAutoplay(bank, allBanks, randomized, restoreStatic);
        else
            dispatchManual(bank, pad, restoreStatic);
    }
    postPlaybackState(startPlayback ? 500U
        : (bank >= 0 ? 501U : kPlaybackStoppedChannel));
    if (startPlayback)
        postPlaybackState(autoplay
            ? (allBanks ? kAutoAllChannel
                        : kAutoBankChannelBase + static_cast<quint32>(bank))
            : kManualLoopChannel);
}

void SoundSwitchMidiInput::applyFeedback(quint32 channel, uchar value)
{
    enum class UiAction { None, PlayPause, Order, Mode };

    const quint32 logicalChannel = channel & 0xffffU;
    const int inputPage = static_cast<int>((channel >> 16) & 0xffffU);
    bool postState = false;
    quint32 stateChannel = kPlaybackStoppedChannel;
    bool postUiCommand = false;
    quint32 uiCommandChannel = 0;
    int feedbackBank = -1;
    UiAction uiAction = UiAction::None;

    {
        QMutexLocker lock(&m_mutex);
        if (logicalChannel >= kUiBankChannelBase &&
            logicalChannel < kUiBankChannelBase + 4U && value != 0)
        {
            // Empty UI command Scenes start and stop immediately. Only their
            // positive edge is a click; processing the trailing zero would
            // dispatch every command twice.
            const int bank = static_cast<int>(logicalChannel - kUiBankChannelBase);
            m_selectedBank = bank;
            uiCommandChannel = 32U + static_cast<quint32>(bank);
            postUiCommand = true;
            feedbackBank = bank;
        }
        else if (logicalChannel >= kUiDwellChannelBase &&
                 logicalChannel < kUiDwellChannelBase +
                     static_cast<quint32>(kAutoplayMeasureCount) &&
                 value != 0)
        {
            // The visible dwell buttons feed the original native SpeedDial
            // presets. This changes a running autoplay immediately without
            // restarting it, while chase speed remains a separate control.
            const int index = static_cast<int>(logicalChannel - kUiDwellChannelBase);
            m_autoplayMeasureIndex = index;
            uiCommandChannel = autoplayConfigChannel(index);
            postUiCommand = true;
        }
        else if (logicalChannel == kUiPlayPauseChannel && value != 0)
        {
            uiAction = UiAction::PlayPause;
        }
        else if (logicalChannel == kUiOrderChannel && value != 0)
        {
            uiAction = UiAction::Order;
        }
        else if (logicalChannel == kUiModeChannel && value != 0)
        {
            uiAction = UiAction::Mode;
        }
        else if (logicalChannel >= kUiSpeedChannelBase &&
                 logicalChannel < kUiSpeedChannelBase +
                     static_cast<quint32>(kSpeedPresetCount) &&
                 value != 0)
        {
            const int index = static_cast<int>(
                logicalChannel - kUiSpeedChannelBase);
            m_speedIndex = index;
            uiCommandChannel = kSpeedPresetBase + static_cast<quint32>(index);
            postUiCommand = true;
        }
        else if (logicalChannel >= kAutoplayConfigBase &&
            logicalChannel < kAutoplayConfigBase + kAutoplayMeasureCount &&
            value != 0)
        {
            m_autoplayMeasureIndex = static_cast<int>(logicalChannel - kAutoplayConfigBase);
        }
        else if (logicalChannel >= kSpeedPresetBase &&
                 logicalChannel < kSpeedPresetBase + kSpeedPresetCount &&
                 value != 0)
        {
            m_speedIndex = static_cast<int>(logicalChannel - kSpeedPresetBase);
        }
        else if ((logicalChannel == kOrderStateBase ||
                  logicalChannel == kOrderStateBase + 1U) && value != 0)
        {
            m_autoplayRandom = logicalChannel == kOrderStateBase + 1U;
        }
        else if (logicalChannel >= kAutoplayVariantBase &&
                 logicalChannel < kAutoplayVariantBase + 10U)
        {
            const int variant = static_cast<int>(logicalChannel - kAutoplayVariantBase);
            const int target = variant / 2;
            if (value != 0)
            {
                m_autoplayRandom = (variant % 2) != 0;
                m_autoplayActive = true;
                m_autoplayAll = target == 4;
                if (!m_autoplayAll)
                    m_autoplayBank = target;
                m_playbackRunning = true;
                m_transportPaused = false;
                stateChannel = m_autoplayAll
                    ? kAutoAllChannel
                    : kAutoBankChannelBase + static_cast<quint32>(m_autoplayBank);
                postState = true;
            }
            else if (m_autoplayActive &&
                     (m_autoplayAll ? target == 4 : target == m_autoplayBank))
            {
                m_playbackRunning = false;
                stateChannel = kPlaybackStoppedChannel;
                postState = true;
            }
        }
        else if (logicalChannel >= kPriorityLookChannelBase &&
                 logicalChannel < kPriorityLookChannelBase +
                     kPriorityLookChannelCount)
        {
            const int note = static_cast<int>(
                logicalChannel - kPriorityLookChannelBase);
            if (value != 0)
                m_latchedStaticNote = note;
            else if (m_latchedStaticNote == note)
                m_latchedStaticNote = -1;
        }
        else if (logicalChannel <= 31U && !m_staticMode)
        {
            if (value != 0)
            {
                m_selectedBank = qBound(0, inputPage, 3);
                m_manualBank = m_selectedBank;
                m_manualPad = static_cast<int>(logicalChannel);
                m_autoplayActive = false;
                m_autoplayAll = false;
                m_playbackRunning = true;
                m_transportPaused = false;
                stateChannel = kManualLoopChannel;
                postState = true;
            }
            else if (!m_autoplayActive && m_manualBank == inputPage &&
                     m_manualPad == static_cast<int>(logicalChannel))
            {
                m_playbackRunning = false;
                stateChannel = kPlaybackStoppedChannel;
                postState = true;
            }
        }
    }

    if (postUiCommand)
        postPulse(uiCommandChannel);
    if (feedbackBank >= 0)
    {
        for (int note = 32; note <= 35; ++note)
            sendFeedback(static_cast<quint32>(note),
                         note - 32 == feedbackBank ? 255 : 0);
    }
    if (uiAction == UiAction::PlayPause)
        togglePlayback();
    else if (uiAction == UiAction::Order)
        toggleOrder();
    else if (uiAction == UiAction::Mode)
        togglePerformanceMode();
    if (postState)
        postPlaybackState(stateChannel);
    sendFeedback(channel, value);
}

void SoundSwitchMidiInput::dispatchAutoplay(int bank, bool allBanks,
                                            bool randomized,
                                            bool restoreStaticMode)
{
    // Autoplay launch buttons live inside the Autoloop page. Manual and AUTO
    // one-child Collection buttons share one outer SoloFrame, so native QLC
    // exclusivity replaces the previous owner without a handoff pulse. If a
    // Priority Looks are on screen, momentarily expose the Autoloop page,
    // start the selected Collection, then restore Priority Looks. The active
    // Look keeps owning the private output layer throughout.
    postPulse(kPerformancePageChannel);
    if (restoreStaticMode)
        postPulse(60);
    postPulse(32 + static_cast<quint32>(qBound(0, bank, 3)));
    postPulse(allBanks ? kAutoAllChannel
                       : kAutoBankChannelBase + static_cast<quint32>(bank));
    postPulse(autoplayVariantChannel(bank, allBanks, randomized));
    if (restoreStaticMode)
        postPulse(kStaticModeChannel);
}

void SoundSwitchMidiInput::dispatchManual(int bank, int pad,
                                          bool restoreStaticMode)
{
    postPulse(kPerformancePageChannel);
    if (restoreStaticMode)
        postPulse(60);
    postPulse(32U + static_cast<quint32>(qBound(0, bank, 3)));
    // The four bank lanes are nested QLC frame pages. QLC routes one plain
    // pad channel to the currently visible lane; adding page bits here would
    // encode that page a second time and make Banks 2-4 miss the event.
    postPulse(static_cast<quint32>(qBound(0, pad, 31)));
    if (restoreStaticMode)
        postPulse(kStaticModeChannel);
}

void SoundSwitchMidiInput::handleShortMessage(DWORD packedMessage)
{
    const quint8 status = static_cast<quint8>(packedMessage & 0xff);
    const quint8 data1 = static_cast<quint8>((packedMessage >> 8) & 0xff);
    const quint8 data2 = static_cast<quint8>((packedMessage >> 16) & 0xff);
    const quint8 command = status & 0xf0;

    if (command == 0x80 || command == 0x90)
    {
        const bool pressed = command == 0x90 && data2 != 0;
        const uchar value = pressed ? UCHAR_MAX : 0;

        bool shifted = false;
        bool staticMode = false;
        bool duplicatePress = false;
        {
            QMutexLocker lock(&m_mutex);
            if (data1 == kShiftNote)
            {
                m_shiftHeld = pressed;
                shifted = m_shiftHeld;
            }
            else if (pressed)
            {
                // Some Control One/Windows MIDI paths can echo an LED update
                // back as a second Note On. Treat one physical down/up cycle
                // as one edge so QLC Toggle buttons cannot immediately undo
                // themselves and appear to flash.
                duplicatePress = m_pressedNotes.contains(data1);
                m_pressedNotes.insert(data1);
                // Remember the modifier state at Note On. A performer can
                // release Shift before the target button; its Note Off must
                // still return to the same logical shifted channel.
                shifted = m_shiftHeld;
                if (shifted)
                    m_shiftedPressedNotes.insert(data1);
            }
            else
            {
                m_pressedNotes.remove(data1);
                shifted = m_shiftedPressedNotes.remove(data1) > 0;
            }
            staticMode = m_staticMode;
        }

        if (duplicatePress)
            return;

        // Shift remains available as a normal logical channel for feedback or
        // diagnostics. Every other note is routed to a distinct shifted bank
        // while Shift is held, so QLC widgets can map layered gestures without
        // a helper program or a core fork.
        if (data1 == kShiftNote)
            postValue(data1, value);
        else if (data1 == 60)
        {
            if (!pressed)
                return;

            if (shifted)
            {
                int bank = 0;
                bool randomized = false;
                bool chooseAll = false;
                bool restoreStatic = false;
                {
                    QMutexLocker lock(&m_mutex);
                    bank = m_selectedBank;
                    chooseAll = m_autoplayActive ? !m_autoplayAll : false;
                    m_autoplayActive = true;
                    m_autoplayBank = bank;
                    m_autoplayAll = chooseAll;
                    randomized = m_autoplayRandom;
                    restoreStatic = m_staticMode;
                    m_playbackRunning = true;
                    m_transportPaused = false;
                }
                // Shift + Auto Loop is now the quick scope control: current
                // bank, then all banks, while keeping the selected length and
                // Sequential/Random order.
                dispatchAutoplay(bank, chooseAll, randomized, restoreStatic);
                postPlaybackState(chooseAll ? kAutoAllChannel
                    : kAutoBankChannelBase + static_cast<quint32>(bank));
                postPlaybackState(500U);
            }
            else
            {
                // One unmodified button now alternates the two 32-pad roles.
                togglePerformanceMode();
            }
        }
        else if (!shifted && data1 >= 32 && data1 <= 35)
        {
            if (pressed)
            {
                QMutexLocker lock(&m_mutex);
                m_selectedBank = data1 - 32;
            }
            postValue(data1, value);
            if (pressed)
            {
                for (int note = 32; note <= 35; ++note)
                    sendFeedback(static_cast<quint32>(note),
                                 note == data1 ? 255 : 0);
            }
        }
        else if (!shifted && data1 == 55)
        {
            postValue(data1, value);
            if (pressed)
                // Autoloop Override is redundant in this Autoloop-first show,
                // so it toggles Sequential/Random and immediately restarts an
                // active autoplay selection with the new order.
                toggleOrder();
        }
        else if (shifted && data1 >= 32 && data1 <= 35)
        {
            // Shift + Bank retains direct bank selection, now using the live
            // measure and order settings rather than a hard-coded 8M preset.
            postValue(kShiftedNoteBase + data1, value);
            if (pressed)
            {
                const int bank = data1 - 32;
                bool chooseAll = false;
                bool randomized = false;
                bool restoreStatic = false;
                {
                    QMutexLocker lock(&m_mutex);
                    chooseAll = m_autoplayActive && m_autoplayBank == bank
                            && !m_autoplayAll;
                    m_selectedBank = bank;
                    m_autoplayActive = true;
                    m_autoplayBank = bank;
                    m_autoplayAll = chooseAll;
                    randomized = m_autoplayRandom;
                    restoreStatic = m_staticMode;
                    m_playbackRunning = true;
                    m_transportPaused = false;
                }
                dispatchAutoplay(bank, chooseAll, randomized, restoreStatic);
                postPlaybackState(chooseAll ? kAutoAllChannel
                    : kAutoBankChannelBase + static_cast<quint32>(bank));
                postPlaybackState(500U);
            }
        }
        else if (!shifted && data1 == 51)
        {
            if (!pressed)
                return;
            togglePlayback();
        }
        else if (!shifted && staticMode && data1 <= 31)
        {
            // The QLC Toggle button is authoritative. A positive pulse starts
            // or stops the selected Scene/Chaser, and its feedback keeps the
            // hardware LED plus the internal Priority Layer state in sync.
            if (pressed)
                postPulse(kPriorityLookChannelBase + data1);
        }
        else if (!shifted && !staticMode && data1 <= 31)
        {
            // A pad selects a native looping owner in manual mode. During
            // Auto Bank/All it seeks the already-running parent Chaser instead,
            // preserving scope, order, dwell and the priority-look layer.
            if (pressed)
            {
                bool remainsRunning = true;
                bool seekAutoplay = false;
                bool allBanks = false;
                int bank = 0;
                {
                    QMutexLocker lock(&m_mutex);
                    bank = m_selectedBank;
                    seekAutoplay = m_autoplayActive && m_playbackRunning;
                    allBanks = m_autoplayAll;
                    if (seekAutoplay)
                    {
                        if (!allBanks && m_autoplayBank >= 0)
                            bank = m_autoplayBank;
                        m_manualPad = static_cast<int>(data1);
                    }
                    else
                    {
                        const bool sameRunningLoop = !m_autoplayActive &&
                            m_playbackRunning && m_manualBank == bank &&
                            m_manualPad == static_cast<int>(data1);
                        m_autoplayBank = -1;
                        m_autoplayActive = false;
                        m_autoplayAll = false;
                        m_manualBank = bank;
                        m_manualPad = static_cast<int>(data1);
                        m_playbackRunning = !sameRunningLoop;
                        m_transportPaused = false;
                    }
                    remainsRunning = m_playbackRunning;
                }

                if (seekAutoplay)
                {
                    postValue(kAutoplaySeekChannel,
                              autoplaySeekValue(bank, data1, allBanks));
                }
                else
                {
                    // A Toggle button only needs one positive edge. Ignoring
                    // the physical release prevents press/release paths from
                    // being interpreted as a momentary flash.
                    postPulse(data1);
                    postPlaybackState(remainsRunning ? kManualLoopChannel
                                                      : kPlaybackStoppedChannel);
                    postPlaybackState(remainsRunning ? 500U
                                                      : kPlaybackStoppedChannel);
                }
            }
        }
        else if (!shifted && data1 >= 36 && data1 <= 44)
        {
            // Color Overrides use the same press-on/press-off behavior. Keep
            // them exclusive so two contradictory palettes cannot be active.
            if (pressed)
            {
                int previous = -1;
                bool activate = false;
                {
                    QMutexLocker lock(&m_mutex);
                    previous = m_latchedOverrideNote;
                    activate = previous != data1;
                    m_latchedOverrideNote = activate ? data1 : -1;
                }
                if (previous >= 0 && previous != data1)
                    postValue(static_cast<quint32>(previous), 0);
                postValue(data1, activate ? UCHAR_MAX : 0);
            }
        }
        else if (shifted && isLatchedShiftSpecial(data1))
        {
            // SoundSwitch uses Shift + White/Black/UV as press-on/press-off
            // latches. Emit one alternating absolute state per press and
            // ignore the physical release so a QLC Flash button can retain
            // its dedicated priority until the next shifted press.
            if (pressed)
            {
                bool active = false;
                {
                    QMutexLocker lock(&m_mutex);
                    if (m_latchedShiftNotes.contains(data1))
                        m_latchedShiftNotes.remove(data1);
                    else
                        m_latchedShiftNotes.insert(data1);
                    active = m_latchedShiftNotes.contains(data1);
                }
                postValue(kShiftedNoteBase + data1,
                          active ? UCHAR_MAX : 0);
            }
        }
        else
            postValue((shifted ? kShiftedNoteBase : 0) + data1, value);
    }
    else if (command == 0xb0)
    {
        bool shiftHeld = false;
        bool staticMode = false;
        {
            QMutexLocker lock(&m_mutex);
            shiftHeld = m_shiftHeld;
            staticMode = m_staticMode;
        }

        if (data1 == 71 && !shiftHeld && !staticMode)
        {
            const int delta = data2 <= 63 ? data2
                                          : static_cast<int>(data2) - 128;
            if (delta != 0)
            {
                int speedIndex = 0;
                {
                    QMutexLocker lock(&m_mutex);
                    m_speedIndex = qBound(
                        0, m_speedIndex + (delta > 0 ? 1 : -1),
                        kSpeedPresetCount - 1);
                    speedIndex = m_speedIndex;
                }
                const uchar selectorValue = static_cast<uchar>(
                    speedIndex == kSpeedPresetCount - 1
                        ? UCHAR_MAX : speedIndex * 64);
                postValue(kControllerBase + data1, selectorValue);
                // Pan/Speed changes only the internal beat-synced rate of raw
                // Autoloops. Autoplay dwell remains on its own live selector.
                postPulse(kSpeedPresetBase + static_cast<quint32>(speedIndex));
            }
            return;
        }

        quint32 channel = 0;
        uchar value = 0;
        {
            QMutexLocker lock(&m_mutex);
            channel = (shiftHeld ? kShiftedControllerBase : kControllerBase) + data1;

            if (isRelativeTurnControl(data1))
            {
                // Control One uses two's-complement relative MIDI: 1..63 turn
                // clockwise and 65..127 turn counter-clockwise. Convert that
                // to a stable absolute 0..255 value before QLC sees it.
                const int delta = data2 <= 63 ? data2 : static_cast<int>(data2) - 128;
                const int current = m_controllerValues.value(channel, 127);
                const int next = qBound(0, current + delta * 2, 255);
                value = static_cast<uchar>(next);
                m_controllerValues.insert(channel, value);
            }
            else
            {
                value = static_cast<uchar>((data2 * UCHAR_MAX + 63) / 127);
            }
        }
        postValue(channel, value);
    }
    else if (command == 0xe0)
    {
        // The touch strip sends the standard 14-bit MIDI pitch-bend message.
        // Give it one absolute QLC channel so it can drive the active page's
        // intensity slider directly.
        const quint16 bend = static_cast<quint16>(data1 | (data2 << 7));
        const uchar value = static_cast<uchar>((bend * UCHAR_MAX + 8191) / 16383);
        postValue(kTouchStripChannel, value);
    }
}
