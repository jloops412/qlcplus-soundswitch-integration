#pragma once

#include <cstdint>

#define CALLBACK

using BOOL = int;
using BYTE = std::uint8_t;
using CHAR = char;
using DWORD = std::uint32_t;
using DWORD_PTR = std::uintptr_t;
using HANDLE = void*;
using UINT = unsigned int;
using UINT_PTR = std::uintptr_t;
using ULONGLONG = unsigned long long;
using WORD = std::uint16_t;

struct DCB {
    DWORD DCBlength;
    DWORD BaudRate;
    DWORD fBinary;
    DWORD fParity;
    DWORD fOutxCtsFlow;
    DWORD fOutxDsrFlow;
    DWORD fDtrControl;
    DWORD fDsrSensitivity;
    DWORD fTXContinueOnXoff;
    DWORD fOutX;
    DWORD fInX;
    DWORD fErrorChar;
    DWORD fNull;
    DWORD fRtsControl;
    DWORD fAbortOnError;
    BYTE ByteSize;
    BYTE Parity;
    BYTE StopBits;
};

struct COMMTIMEOUTS {
    DWORD ReadIntervalTimeout;
    DWORD ReadTotalTimeoutMultiplier;
    DWORD ReadTotalTimeoutConstant;
    DWORD WriteTotalTimeoutMultiplier;
    DWORD WriteTotalTimeoutConstant;
};

#define FALSE 0
#define TRUE 1
#define INVALID_HANDLE_VALUE ((HANDLE)(std::intptr_t)-1)

inline constexpr DWORD ERROR_SUCCESS = 0U;
inline constexpr DWORD ERROR_INVALID_NAME = 123U;
inline constexpr DWORD ERROR_NOT_ENOUGH_MEMORY = 8U;
inline constexpr DWORD ERROR_WRITE_FAULT = 29U;
inline constexpr DWORD MAXDWORD = 0xFFFFFFFFU;
inline constexpr DWORD GENERIC_READ = 0x80000000U;
inline constexpr DWORD GENERIC_WRITE = 0x40000000U;
inline constexpr DWORD OPEN_EXISTING = 3U;
inline constexpr DWORD FILE_ATTRIBUTE_NORMAL = 0x80U;
inline constexpr DWORD CBR_115200 = 115200U;
inline constexpr BYTE NOPARITY = 0U;
inline constexpr BYTE ONESTOPBIT = 0U;
inline constexpr DWORD DTR_CONTROL_DISABLE = 0U;
inline constexpr DWORD RTS_CONTROL_DISABLE = 0U;
inline constexpr DWORD PURGE_TXABORT = 0x0001U;
inline constexpr DWORD PURGE_RXABORT = 0x0002U;
inline constexpr DWORD PURGE_TXCLEAR = 0x0004U;
inline constexpr DWORD PURGE_RXCLEAR = 0x0008U;

ULONGLONG GetTickCount64();
DWORD QueryDosDeviceA(const CHAR*, CHAR*, DWORD);
HANDLE CreateFileA(const CHAR*, DWORD, DWORD, void*, DWORD, DWORD, HANDLE);
BOOL GetCommState(HANDLE, DCB*);
BOOL SetCommState(HANDLE, DCB*);
BOOL SetCommTimeouts(HANDLE, COMMTIMEOUTS*);
BOOL SetupComm(HANDLE, DWORD, DWORD);
BOOL PurgeComm(HANDLE, DWORD);
BOOL WriteFile(HANDLE, const void*, DWORD, DWORD*, void*);
BOOL CloseHandle(HANDLE);
DWORD GetLastError();
