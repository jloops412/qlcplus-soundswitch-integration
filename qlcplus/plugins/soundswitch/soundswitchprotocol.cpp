/*
  Q Light Controller Plus
  soundswitchprotocol.cpp

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

#include "soundswitchprotocol.h"

#include <algorithm>

namespace SoundSwitchProtocol
{

Packet buildDmxPacket(std::uint8_t port, const Universe &universe) noexcept
{
    Packet packet{};
    packet[0] = static_cast<std::uint8_t>('s');
    packet[1] = static_cast<std::uint8_t>('T');
    packet[2] = static_cast<std::uint8_t>('R');
    packet[3] = static_cast<std::uint8_t>('t');
    packet[4] = 0x01U;
    packet[5] = 0x00U;
    packet[6] = 0x02U;
    packet[7] = 0x02U;
    packet[8] = port;
    packet[9] = 0x00U;
    std::copy(universe.begin(), universe.end(), packet.begin() + 10);
    return packet;
}

} // namespace SoundSwitchProtocol
