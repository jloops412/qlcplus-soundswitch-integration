#pragma once

#include "emberlights/project.hpp"
#include "showcore/soundswitch_micro.hpp"
#include "showcore/types.hpp"

#include <cstddef>
#include <cstdint>

namespace emberlights {

inline constexpr std::string_view kBothLightingIr4SixChannelProfileId =
    "builtin.both-lighting.ir-4.6ch";

enum class Ir4QualificationError : std::uint8_t {
    None,
    InvalidProject,
    CompilationFailed,
    MissingLook,
    LookCompilationFailed,
    FrameMismatch,
    PacketMismatch
};

enum class MicroPhysicalQualificationResult : std::uint8_t {
    Passed,
    SoftwareFrameMismatch,
    InitialOpenFailed,
    RawWriteFailed,
    RawObservationFailed,
    RepeatOpenFailed,
    RunnerWriteFailed,
    RunnerObservationFailed,
    BlackoutFailed,
    DisconnectNotObserved,
    ReconnectNotDetected,
    ReconnectOpenFailed,
    ReconnectWriteFailed,
    ReconnectObservationFailed,
    ReconnectBlackoutFailed
};

struct MicroPhysicalQualificationEvidence {
    bool software_frame_match{false};
    bool initial_open_succeeded{false};
    bool raw_writes_succeeded{false};
    bool raw_visible_red_and_blackout{false};
    bool repeat_open_succeeded{false};
    bool runner_writes_succeeded{false};
    bool runner_visible_match_and_blackout{false};
    bool bounded_blackouts_succeeded{false};
    bool disconnect_observed{false};
    bool reconnect_detected{false};
    bool reconnect_open_succeeded{false};
    bool reconnect_writes_succeeded{false};
    bool reconnect_visible_red_and_blackout{false};
    bool reconnect_blackout_succeeded{false};
};

struct FrameComparison {
    std::size_t differing_slots{0U};
    std::uint16_t first_differing_channel{0U};
    std::uint8_t expected{0U};
    std::uint8_t actual{0U};

    [[nodiscard]] bool exact() const noexcept { return differing_slots == 0U; }
};

struct PacketComparison {
    std::size_t differing_bytes{0U};
    std::size_t first_differing_offset{0U};
    std::uint8_t expected{0U};
    std::uint8_t actual{0U};

    [[nodiscard]] bool exact() const noexcept { return differing_bytes == 0U; }
};

struct Ir4QualificationFrames {
    Ir4QualificationError error{Ir4QualificationError::None};
    ProjectValidation validation{};
    showcore::DmxUniverse raw_reference{};
    showcore::DmxUniverse runner_rendered{};
    showcore::SoundSwitchMicroPacket raw_packet{};
    showcore::SoundSwitchMicroPacket runner_packet{};
    FrameComparison frame_comparison{};
    PacketComparison packet_comparison{};

    [[nodiscard]] bool exact() const noexcept {
        return error == Ir4QualificationError::None &&
            frame_comparison.exact() && packet_comparison.exact();
    }
};

[[nodiscard]] ProjectDocument make_ir4_6ch_qualification_project();

[[nodiscard]] FrameComparison compare_dmx_frames(
    const showcore::DmxUniverse& expected,
    const showcore::DmxUniverse& actual) noexcept;

[[nodiscard]] PacketComparison compare_soundswitch_micro_packets(
    const showcore::SoundSwitchMicroPacket& expected,
    const showcore::SoundSwitchMicroPacket& actual) noexcept;

[[nodiscard]] Ir4QualificationFrames build_ir4_6ch_red_qualification();

[[nodiscard]] MicroPhysicalQualificationResult evaluate_micro_physical_qualification(
    const MicroPhysicalQualificationEvidence& evidence) noexcept;

[[nodiscard]] const char* micro_physical_qualification_result_name(
    MicroPhysicalQualificationResult result) noexcept;

}  // namespace emberlights
