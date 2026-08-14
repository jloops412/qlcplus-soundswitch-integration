#pragma once

#include "windows.h"

using HMIDIIN = void*;
using HMIDIOUT = void*;
using LPHMIDIIN = HMIDIIN*;
using LPHMIDIOUT = HMIDIOUT*;
using MMRESULT = UINT;

struct MIDIINCAPSA {
    WORD wMid;
    WORD wPid;
    DWORD vDriverVersion;
    CHAR szPname[32];
    DWORD dwSupport;
};

struct MIDIOUTCAPSA {
    WORD wMid;
    WORD wPid;
    DWORD vDriverVersion;
    CHAR szPname[32];
    WORD wTechnology;
    WORD wVoices;
    WORD wNotes;
    WORD wChannelMask;
    DWORD dwSupport;
};

inline constexpr MMRESULT MMSYSERR_NOERROR = 0;
inline constexpr MMRESULT MMSYSERR_NOMEM = 7;
inline constexpr UINT MIM_DATA = 0x3C3;
inline constexpr UINT MIM_MOREDATA = 0x3CC;
inline constexpr DWORD CALLBACK_NULL = 0;
inline constexpr DWORD CALLBACK_FUNCTION = 0x00030000;
inline constexpr DWORD MIDI_IO_STATUS = 0x00000020;

UINT midiInGetNumDevs();
MMRESULT midiInGetDevCapsA(UINT_PTR, MIDIINCAPSA*, UINT);
MMRESULT midiInOpen(LPHMIDIIN, UINT, DWORD_PTR, DWORD_PTR, DWORD);
MMRESULT midiInStart(HMIDIIN);
MMRESULT midiInStop(HMIDIIN);
MMRESULT midiInReset(HMIDIIN);
MMRESULT midiInClose(HMIDIIN);
MMRESULT midiInGetID(HMIDIIN, UINT*);

UINT midiOutGetNumDevs();
MMRESULT midiOutGetDevCapsA(UINT_PTR, MIDIOUTCAPSA*, UINT);
MMRESULT midiOutOpen(LPHMIDIOUT, UINT, DWORD_PTR, DWORD_PTR, DWORD);
MMRESULT midiOutReset(HMIDIOUT);
MMRESULT midiOutClose(HMIDIOUT);
MMRESULT midiOutShortMsg(HMIDIOUT, DWORD);
