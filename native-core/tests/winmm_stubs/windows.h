#pragma once

#include <cstdint>

#define CALLBACK

using BOOL = int;
using CHAR = char;
using DWORD = std::uint32_t;
using DWORD_PTR = std::uintptr_t;
using UINT = unsigned int;
using UINT_PTR = std::uintptr_t;
using ULONGLONG = unsigned long long;
using WORD = std::uint16_t;

ULONGLONG GetTickCount64();
