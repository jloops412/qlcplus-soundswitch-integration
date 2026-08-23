/*
  Q Light Controller Plus
  soundswitchmidiinput.h

  Licensed under the Apache License, Version 2.0.
*/

#ifndef SOUNDSWITCHMIDIINPUT_H
#define SOUNDSWITCHMIDIINPUT_H

#include <QObject>
#include <QHash>
#include <QMutex>
#include <QSet>

#include <windows.h>
#include <mmsystem.h>

class SoundSwitchMidiInput final : public QObject
{
    Q_OBJECT

public:
    explicit SoundSwitchMidiInput(QObject *parent = nullptr);
    ~SoundSwitchMidiInput() override;

    bool open();
    bool ensureConnected();
    void close();
    bool isOpen() const;
    bool ensureFeedbackConnected();
    void closeFeedback();
    bool sendFeedback(quint32 channel, uchar value);
    void applyFeedback(quint32 channel, uchar value);
    static bool isPresent();

signals:
    void valueChanged(quint32 channel, uchar value);

private:
    static int findControlOneDevice();
    static int findControlOneOutputDevice();
    static bool isControlOneName(const QString &name);
    static void releaseHandle(HMIDIIN handle);
    static void releaseOutputHandle(HMIDIOUT handle);
    void resetControllerStateLocked();
    static void CALLBACK midiCallback(HMIDIIN handle, UINT message,
                                      DWORD_PTR instance, DWORD_PTR param1,
                                      DWORD_PTR param2);
    void handleShortMessage(DWORD packedMessage);
    void postValue(quint32 channel, uchar value);
    void postPulse(quint32 channel);
    void postPlaybackState(quint32 channel);
    void dispatchAutoplay(int bank, bool allBanks, bool randomized,
                          bool restoreStaticMode);
    void dispatchManual(int bank, int pad, bool restoreStaticMode);
    void restoreLogicalState();

private:
    mutable QMutex m_mutex;
    HMIDIIN m_handle{nullptr};
    HMIDIOUT m_outputHandle{nullptr};
    bool m_shiftHeld{false};
    bool m_staticMode{false};
    int m_selectedBank{0};
    int m_autoplayBank{-1};
    bool m_autoplayActive{false};
    bool m_autoplayAll{false};
    int m_autoplayMeasureIndex{3};
    bool m_autoplayRandom{false};
    int m_speedIndex{2};
    int m_manualBank{-1};
    int m_manualPad{-1};
    bool m_playbackRunning{false};
    bool m_transportPaused{false};
    int m_latchedStaticNote{-1};
    int m_latchedOverrideNote{-1};
    QSet<quint8> m_pressedNotes;
    QSet<quint8> m_shiftedPressedNotes;
    QSet<quint8> m_latchedShiftNotes;
    QHash<quint32, uchar> m_controllerValues;
};

#endif
