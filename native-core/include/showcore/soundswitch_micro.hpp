#pragma once

#include "showcore/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace showcore {

// The SoundSwitch Micro reports one vendor-specific 64-byte bulk OUT pipe.
// These candidate payloads are isolated protocol-discovery evidence; none is
// considered qualified until it produces a physical DMX frame on the target
// interface. The largest candidate is ENTTEC-compatible application framing.
inline constexpr std::size_t kSoundSwitchMicroRawFrameSize = kUniverseSlots + 1U;
inline constexpr std::size_t kSoundSwitchMicroMaximumFrameSize =
    kSoundSwitchMicroRawFrameSize + 5U;

enum class SoundSwitchMicroFraming : std::uint8_t {
    RawDmxWithStartCode,
    RawSlotsOnly,
    EnttecUsbPro
};

struct SoundSwitchMicroPacket {
    std::array<std::uint8_t, kSoundSwitchMicroMaximumFrameSize> bytes{};
    std::size_t length{0U};
    bool terminate_with_short_packet{false};
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
