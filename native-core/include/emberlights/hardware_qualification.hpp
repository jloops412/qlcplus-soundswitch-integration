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

}  // namespace emberlights
