#pragma once

#include "showcore/types.hpp"

#include <array>
#include <chrono>
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

enum class SoundSwitchMicroLifecycleState : std::uint8_t {
    Disabled,
    Detecting,
    Opening,
    Inspecting,
    Initializing,
    Settling,
    WarmingUp,
    Streaming,
    Recovering,
    Fault,
    Closing
};

inline constexpr std::array kSoundSwitchMicroOpenSequence{
    SoundSwitchMicroLifecycleState::Detecting,
    SoundSwitchMicroLifecycleState::Opening,
    SoundSwitchMicroLifecycleState::Inspecting,
    SoundSwitchMicroLifecycleState::Initializing,
    SoundSwitchMicroLifecycleState::Settling,
    SoundSwitchMicroLifecycleState::WarmingUp,
    SoundSwitchMicroLifecycleState::Streaming};

struct SoundSwitchMicroSessionConfig {
    SoundSwitchMicroFraming framing{SoundSwitchMicroFraming::NativeJls1};
    std::chrono::milliseconds transfer_timeout{500};
    std::chrono::milliseconds settling_interval{200};
    std::chrono::milliseconds frame_interval{25};
    std::uint16_t warmup_blackout_frames{50U};
    std::uint16_t close_blackout_frames{3U};
};

struct SoundSwitchMicroSessionStatus {
    SoundSwitchMicroLifecycleState state{SoundSwitchMicroLifecycleState::Disabled};
    std::uint32_t last_error{0U};
    std::uint32_t initialization_attempts{0U};
    std::uint32_t initialization_successes{0U};
    std::uint32_t initialization_failures{0U};
    std::uint32_t reconnect_count{0U};
    std::uint64_t frames_attempted{0U};
    std::uint64_t frames_accepted{0U};
    std::uint64_t frames_failed{0U};
    std::uint16_t warmup_frames_completed{0U};
    std::uint8_t configuration_value{0U};
    std::uint8_t interface_number{0U};
    std::uint8_t alternate_setting{0U};
    std::uint8_t bulk_out_pipe{0U};
    std::uint16_t maximum_packet_size{0U};
    bool device_present{false};
    bool handle_open{false};
    bool warmup_complete{false};
};

[[nodiscard]] const char* soundswitch_micro_lifecycle_name(
    SoundSwitchMicroLifecycleState state) noexcept;

[[nodiscard]] bool valid_soundswitch_micro_session_config(
    const SoundSwitchMicroSessionConfig& config) noexcept;

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

class SoundSwitchMicroSession {
public:
    SoundSwitchMicroSession() noexcept;
    ~SoundSwitchMicroSession() noexcept;

    SoundSwitchMicroSession(const SoundSwitchMicroSession&) = delete;
    SoundSwitchMicroSession& operator=(const SoundSwitchMicroSession&) = delete;

    [[nodiscard]] static bool supported() noexcept;
    [[nodiscard]] bool open(SoundSwitchMicroFraming framing) noexcept;
    [[nodiscard]] bool open(const SoundSwitchMicroSessionConfig& config) noexcept;
    void close() noexcept;
    [[nodiscard]] bool is_open() const noexcept;
    [[nodiscard]] bool send(const DmxUniverse& universe) noexcept;
    [[nodiscard]] bool send_blackout(
        std::uint16_t repetitions,
        std::chrono::milliseconds interval = std::chrono::milliseconds{25}) noexcept;
    [[nodiscard]] std::uint32_t last_error() const noexcept;
    [[nodiscard]] SoundSwitchMicroFraming framing() const noexcept;
    [[nodiscard]] SoundSwitchMicroSessionStatus status() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Compatibility name for call sites that predate the shared session contract.
using SoundSwitchMicroSender = SoundSwitchMicroSession;

}  // namespace showcore
