/* Deterministic WinMM boundary for MIDI translation tests. No hardware I/O. */
#ifndef SOUNDSWITCHMIDITESTBACKEND_H
#define SOUNDSWITCHMIDITESTBACKEND_H

#include <climits>
#include <cstdint>
#include <cwchar>
#include <vector>

using UINT = unsigned int;
using DWORD = std::uint32_t;
using DWORD_PTR = std::uintptr_t;
using MMRESULT = UINT;
using HMIDIIN = void *;
using HMIDIOUT = void *;
#define CALLBACK
constexpr MMRESULT MMSYSERR_NOERROR = 0;
constexpr UINT CALLBACK_FUNCTION = 0;
constexpr UINT MIM_DATA = 1;
constexpr UINT MIM_CLOSE = 2;
constexpr UINT MOM_CLOSE = 3;
struct MIDIINCAPSW { wchar_t szPname[64]{}; };
using MIDIOUTCAPSW = MIDIINCAPSW;

namespace SoundSwitchMidiTestBackend
{
inline bool present = false;
inline bool inputValid = true;
inline bool outputValid = true;
inline std::uintptr_t nextHandle = 10;
inline std::vector<DWORD> messages;
inline void reset()
{
    present = false;
    inputValid = true;
    outputValid = true;
    messages.clear();
}
}

inline UINT midiInGetNumDevs() { return SoundSwitchMidiTestBackend::present ? 1 : 0; }
inline UINT midiOutGetNumDevs() { return midiInGetNumDevs(); }
inline MMRESULT midiInGetDevCapsW(UINT, MIDIINCAPSW *caps, UINT)
{
    std::wcscpy(caps->szPname, L"SoundSwitch Control One");
    return SoundSwitchMidiTestBackend::present ? 0 : 1;
}
inline MMRESULT midiOutGetDevCapsW(UINT id, MIDIOUTCAPSW *caps, UINT size)
{ return midiInGetDevCapsW(id, caps, size); }
inline MMRESULT midiInGetID(HMIDIIN, UINT *id)
{ *id = 0; return SoundSwitchMidiTestBackend::inputValid ? 0 : 1; }
inline MMRESULT midiOutGetID(HMIDIOUT, UINT *id)
{ *id = 0; return SoundSwitchMidiTestBackend::outputValid ? 0 : 1; }
inline MMRESULT midiInOpen(HMIDIIN *handle, UINT, DWORD_PTR, DWORD_PTR, UINT)
{
    *handle = reinterpret_cast<void *>(SoundSwitchMidiTestBackend::nextHandle++);
    SoundSwitchMidiTestBackend::inputValid = true;
    return 0;
}
inline MMRESULT midiOutOpen(HMIDIOUT *handle, UINT, DWORD_PTR, DWORD_PTR, UINT)
{
    *handle = reinterpret_cast<void *>(SoundSwitchMidiTestBackend::nextHandle++);
    SoundSwitchMidiTestBackend::outputValid = true;
    return 0;
}
inline MMRESULT midiInStart(HMIDIIN) { return 0; }
inline MMRESULT midiInStop(HMIDIIN) { return 0; }
inline MMRESULT midiInReset(HMIDIIN) { return 0; }
inline MMRESULT midiInClose(HMIDIIN) { return 0; }
inline MMRESULT midiOutClose(HMIDIOUT) { return 0; }
inline MMRESULT midiOutShortMsg(HMIDIOUT, DWORD message)
{
    SoundSwitchMidiTestBackend::messages.push_back(message);
    return 0;
}
#endif
