/*
  Q Light Controller Plus
  soundswitchprotocol.h

  Copyright (c) 2026 EmberLights contributors

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#ifndef SOUNDSWITCHPROTOCOL_H
#define SOUNDSWITCHPROTOCOL_H

#include <array>
#include <cstddef>
#include <cstdint>

namespace SoundSwitchProtocol
{

constexpr std::uint16_t VendorId = 0x15E4U;
constexpr std::uint16_t MicroProductId = 0x0053U;
constexpr std::uint16_t ControlOneProductId = 0x0054U;
constexpr std::size_t UniverseSlots = 512U;
constexpr std::size_t FrameSize = 522U;
constexpr std::size_t ControlPacketSize = 12U;
constexpr std::uint8_t BulkOutPipe = 0x01U;
constexpr std::uint16_t BulkPacketSize = 64U;
constexpr std::uint8_t ConfigurationValue = 1U;
constexpr std::uint8_t InterfaceNumber = 0U;
constexpr std::uint8_t AlternateSetting = 0U;
constexpr std::uint8_t ControlOneOutputCount = 2U;

enum class DeviceKind : std::uint8_t
{
    Micro,
    ControlOne
};

using Universe = std::array<std::uint8_t, UniverseSlots>;
using Packet = std::array<std::uint8_t, FrameSize>;
using ControlPacket = std::array<std::uint8_t, ControlPacketSize>;

Packet buildDmxPacket(std::uint8_t port, const Universe &universe) noexcept;

constexpr std::array<ControlPacket, 2> MicroInitializationPackets{{
    {{'s', 'T', 'R', 't', 0x02U, 0x00U, 0x04U, 0x00U,
      0x00U, 0x00U, 0x01U, 0x00U}},
    {{'s', 'T', 'R', 't', 0x02U, 0x00U, 0x04U, 0x00U,
      0x01U, 0x00U, 0xFFU, 0xFFU}}
}};

constexpr std::array<ControlPacket, 4> ControlOneInitializationPackets{{
    {{'s', 'T', 'R', 't', 0x02U, 0x00U, 0x04U, 0x00U,
      0x00U, 0x00U, 0x01U, 0x00U}},
    {{'s', 'T', 'R', 't', 0x02U, 0x00U, 0x04U, 0x00U,
      0x01U, 0x00U, 0xFFU, 0xFFU}},
    {{'s', 'T', 'R', 't', 0x02U, 0x00U, 0x04U, 0x00U,
      0x00U, 0x00U, 0x01U, 0x00U}},
    {{'s', 'T', 'R', 't', 0x02U, 0x00U, 0x04U, 0x00U,
      0x01U, 0x00U, 0x01U, 0x00U}}
}};

} // namespace SoundSwitchProtocol

#endif
