#pragma once

#include "showcore/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace showcore {

// The SoundSwitch Micro reports one vendor-specific 64-byte bulk OUT pipe. Its
// native frame is an eight-byte JLS1 header followed by a zero-based output
// port, the DMX start code, and 512 slots.
inline constexpr std::size_t kSoundSwitchMicroHeaderSize = 8U;
inline constexpr std::size_t kSoundSwitchMicroPayloadSize = kUniverseSlots + 2U;
inline constexpr std::size_t kSoundSwitchMicroMaximumFrameSize =
    kSoundSwitchMicroHeaderSize + kSoundSwitchMicroPayloadSize;
inline constexpr std::size_t kSoundSwitchMicroControlPacketSize = 12U;
inline constexpr std::uint8_t kSoundSwitchMicroLegacyMaximumFramingValue = 2U;
inline constexpr std::array<
    std::array<std::uint8_t, kSoundSwitchMicroControlPacketSize>, 2>
    kSoundSwitchMicroInitializationPackets{{
        {{'s', 'T', 'R', 't', 0x02U, 0x00U, 0x04U, 0x00U,
          0x00U, 0x00U, 0x01U, 0x00U}},
        {{'s', 'T', 'R', 't', 0x02U, 0x00U, 0x04U, 0x00U,
          0x01U, 0x00U, 0xFFU, 0xFFU}},
    }};

enum class SoundSwitchMicroFraming : std::uint8_t {
    NativeJls1 = 0U
};

struct SoundSwitchMicroPacket {
    std::array<std::uint8_t, kSoundSwitchMicroMaximumFrameSize> bytes{};
    std::size_t length{0U};
};

[[nodiscard]] SoundSwitchMicroPacket build_soundswitch_micro_packet(
    const DmxUniverse& universe,
    SoundSwitchMicroFraming framing) noexcept;

[[nodiscard]] constexpr std::uint8_t soundswitch_micro_framing_value(
    SoundSwitchMicroFraming framing) noexcept {
    return static_cast<std::uint8_t>(framing);
}

class SoundSwitchMicroSender {
public:
    SoundSwitchMicroSender() noexcept;
    ~SoundSwitchMicroSender() noexcept;

    SoundSwitchMicroSender(const SoundSwitchMicroSender&) = delete;
    SoundSwitchMicroSender& operator=(const SoundSwitchMicroSender&) = delete;

    [[nodiscard]] static bool supported() noexcept;
    [[nodiscard]] bool open(SoundSwitchMicroFraming framing) noexcept;
    void close() noexcept;
    [[nodiscard]] bool is_open() const noexcept;
    [[nodiscard]] bool send(const DmxUniverse& universe) noexcept;
    [[nodiscard]] std::uint32_t last_error() const noexcept;
    [[nodiscard]] SoundSwitchMicroFraming framing() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace showcore
