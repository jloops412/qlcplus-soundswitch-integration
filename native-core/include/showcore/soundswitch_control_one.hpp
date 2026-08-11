#pragma once

#include "showcore/types.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace showcore {

inline constexpr std::uint16_t kSoundSwitchVendorId = 0x15E4U;
inline constexpr std::uint16_t kSoundSwitchControlOneProductId = 0x0054U;
inline constexpr std::uint8_t kSoundSwitchControlOneConfiguration = 1U;
inline constexpr std::uint8_t kSoundSwitchControlOneInterface = 0U;
inline constexpr std::uint8_t kSoundSwitchControlOneBulkOutPipe = 0x01U;
inline constexpr std::uint16_t kSoundSwitchControlOneBulkPacketSize = 64U;
inline constexpr std::size_t kSoundSwitchControlOneFrameSize = 522U;
inline constexpr std::size_t kSoundSwitchControlOneControlPacketSize = 12U;
inline constexpr std::uint8_t kSoundSwitchControlOneOutputCount = 2U;

enum class SoundSwitchControlOnePort : std::uint8_t {
    One = 0U,
    Two = 1U
};

// JLC1 inherits the JLS1 transport. The first two packets are the base JLS1
// device controls. The final two put both Control One jacks into DMX output
// mode. These are deliberately kept separate from MIDI/OLED/storage traffic.
inline constexpr std::array<
    std::array<std::uint8_t, kSoundSwitchControlOneControlPacketSize>, 4>
    kSoundSwitchControlOneInitializationPackets{{
        {{'s', 'T', 'R', 't', 0x02U, 0x00U, 0x04U, 0x00U,
          0x00U, 0x00U, 0x01U, 0x00U}},
        {{'s', 'T', 'R', 't', 0x02U, 0x00U, 0x04U, 0x00U,
          0x01U, 0x00U, 0xFFU, 0xFFU}},
        {{'s', 'T', 'R', 't', 0x02U, 0x00U, 0x04U, 0x00U,
          0x00U, 0x00U, 0x01U, 0x00U}},
        {{'s', 'T', 'R', 't', 0x02U, 0x00U, 0x04U, 0x00U,
          0x01U, 0x00U, 0x01U, 0x00U}},
    }};

enum class SoundSwitchControlOneLifecycleState : std::uint8_t {
    Disabled,
    Detecting,
    Opening,
    Inspecting,
    Initializing,
    WarmingUp,
    Streaming,
    Fault,
    Closing
};

struct SoundSwitchControlOneSessionConfig {
    std::chrono::milliseconds transfer_timeout{500};
    std::chrono::milliseconds frame_interval{25};
    std::uint16_t warmup_blackout_pairs{2U};
    std::uint16_t close_blackout_pairs{3U};
};

struct SoundSwitchControlOneSessionStatus {
    SoundSwitchControlOneLifecycleState state{
        SoundSwitchControlOneLifecycleState::Disabled};
    std::uint32_t last_error{0U};
    std::uint32_t open_attempts{0U};
    std::uint32_t open_successes{0U};
    std::uint32_t reconnect_count{0U};
    std::uint64_t frames_attempted{0U};
    std::uint64_t frames_accepted{0U};
    std::uint64_t frames_failed{0U};
    std::uint8_t configuration_value{0U};
    std::uint8_t interface_number{0U};
    std::uint8_t alternate_setting{0U};
    std::uint8_t bulk_out_pipe{0U};
    std::uint16_t maximum_packet_size{0U};
    bool device_present{false};
    bool handle_open{false};
    bool warmup_complete{false};
};

struct SoundSwitchControlOnePacket {
    std::array<std::uint8_t, kSoundSwitchControlOneFrameSize> bytes{};
    std::size_t length{0U};
};

[[nodiscard]] const char* soundswitch_control_one_lifecycle_name(
    SoundSwitchControlOneLifecycleState state) noexcept;
[[nodiscard]] bool valid_soundswitch_control_one_session_config(
    const SoundSwitchControlOneSessionConfig& config) noexcept;
[[nodiscard]] SoundSwitchControlOnePacket build_soundswitch_control_one_packet(
    SoundSwitchControlOnePort port,
    const DmxUniverse& universe) noexcept;

class SoundSwitchControlOneSession {
public:
    SoundSwitchControlOneSession() noexcept;
    ~SoundSwitchControlOneSession() noexcept;

    SoundSwitchControlOneSession(const SoundSwitchControlOneSession&) = delete;
    SoundSwitchControlOneSession& operator=(
        const SoundSwitchControlOneSession&) = delete;

    [[nodiscard]] static bool supported() noexcept;
    [[nodiscard]] bool open(
        const SoundSwitchControlOneSessionConfig& config = {}) noexcept;
    void close() noexcept;
    [[nodiscard]] bool is_open() const noexcept;
    [[nodiscard]] bool send(
        SoundSwitchControlOnePort port,
        const DmxUniverse& universe) noexcept;
    [[nodiscard]] bool send_pair(
        const std::array<DmxUniverse, kSoundSwitchControlOneOutputCount>& universes)
        noexcept;
    [[nodiscard]] bool send_blackout(
        std::uint16_t repetitions,
        std::chrono::milliseconds interval = std::chrono::milliseconds{25})
        noexcept;
    [[nodiscard]] std::uint32_t last_error() const noexcept;
    [[nodiscard]] SoundSwitchControlOneSessionStatus status() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace showcore
