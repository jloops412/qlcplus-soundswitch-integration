#include "emberlights/hardware_qualification.hpp"

#include "emberlights/compiler.hpp"
#include "showcore/layer_resolver.hpp"
#include "showcore/look.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace emberlights {

ProjectDocument make_ir4_6ch_qualification_project() {
    auto project = make_starter_project();
    project.id = "emberlights-ir4-6ch-qualification";
    project.name = "IR-4 6CH SoundSwitch Micro Qualification";
    project.connections.os2l_enabled = false;
    project.connections.artnet_enabled = false;
    project.connections.sacn_enabled = false;
    project.connections.dmx_usb_pro_ports = {};
    project.connections.soundswitch_micro_universe = 1U;
    project.connections.soundswitch_micro_framing =
        showcore::SoundSwitchMicroFraming::NativeJls1;
    project.connections.frame_rate = 40U;
    project.fixtures.clear();
    project.groups.clear();
    project.looks.clear();
    project.autoloops.clear();
    project.audio_assets.clear();
    project.track_scripts.clear();
    project.midi_mappings.clear();

    project.fixtures.push_back({
        "ir4-bench-001",
        "Both Lighting IR-4 Bench Fixture",
        std::string(kBothLightingIr4SixChannelProfileId),
        1U,
        1U,
        {"qualification", "uplight", "color"}});

    LookDefinition red;
    red.id = "ir4-bench-red";
    red.name = "IR-4 Exact Red";
    red.fade_ms = 0U;
    red.assignments = {
        {"ir4-bench-001", showcore::Property::Red, showcore::PropertyValue::set(1.0F)},
        {"ir4-bench-001", showcore::Property::Green, showcore::PropertyValue::force_zero()},
        {"ir4-bench-001", showcore::Property::Blue, showcore::PropertyValue::force_zero()},
        {"ir4-bench-001", showcore::Property::White, showcore::PropertyValue::force_zero()},
        {"ir4-bench-001", showcore::Property::Amber, showcore::PropertyValue::force_zero()},
        {"ir4-bench-001", showcore::Property::UV, showcore::PropertyValue::force_zero()}};
    project.looks.push_back(std::move(red));
    return project;
}

FrameComparison compare_dmx_frames(
    const showcore::DmxUniverse& expected,
    const showcore::DmxUniverse& actual) noexcept {
    FrameComparison result;
    for (std::size_t index = 0U; index < expected.size(); ++index) {
        if (expected[index] == actual[index]) {
            continue;
        }
        if (result.differing_slots == 0U) {
            result.first_differing_channel = static_cast<std::uint16_t>(index + 1U);
            result.expected = expected[index];
            result.actual = actual[index];
        }
        ++result.differing_slots;
    }
    return result;
}

PacketComparison compare_soundswitch_micro_packets(
    const showcore::SoundSwitchMicroPacket& expected,
    const showcore::SoundSwitchMicroPacket& actual) noexcept {
    PacketComparison result;
    const auto maximum = std::max(expected.length, actual.length);
    for (std::size_t index = 0U; index < maximum; ++index) {
        const auto expected_byte = index < expected.length ? expected.bytes[index] : 0U;
        const auto actual_byte = index < actual.length ? actual.bytes[index] : 0U;
        if (expected_byte == actual_byte &&
            (index < expected.length) == (index < actual.length)) {
            continue;
        }
        if (result.differing_bytes == 0U) {
            result.first_differing_offset = index;
            result.expected = expected_byte;
            result.actual = actual_byte;
        }
        ++result.differing_bytes;
    }
    return result;
}

Ir4QualificationFrames build_ir4_6ch_red_qualification() {
    Ir4QualificationFrames result;
    result.raw_reference[0] = 255U;
    result.raw_packet = showcore::build_soundswitch_micro_packet(
        result.raw_reference,
        showcore::SoundSwitchMicroFraming::NativeJls1);

    auto project = make_ir4_6ch_qualification_project();
    result.validation = validate_project(project);
    if (!result.validation.ok()) {
        result.error = Ir4QualificationError::InvalidProject;
        return result;
    }

    auto compilation = compile_project(project);
    result.validation = std::move(compilation.validation);
    if (!compilation) {
        result.error = Ir4QualificationError::CompilationFailed;
        return result;
    }

    const auto* look = compilation.show->look(0U);
    if (look == nullptr) {
        result.error = Ir4QualificationError::MissingLook;
        return result;
    }
    showcore::LayerBuffer layer;
    const auto look_result = showcore::compile_static_look(*look, layer);
    if (!look_result) {
        result.error = Ir4QualificationError::LookCompilationFailed;
        return result;
    }
    auto& engine = compilation.show->engine();
    engine.layers().replace_layer(showcore::LayerId::EventMoment, layer);
    engine.tick();
    result.runner_rendered = engine.frames().universes[0];
    result.runner_packet = showcore::build_soundswitch_micro_packet(
        result.runner_rendered,
        showcore::SoundSwitchMicroFraming::NativeJls1);
    result.frame_comparison = compare_dmx_frames(
        result.raw_reference, result.runner_rendered);
    result.packet_comparison = compare_soundswitch_micro_packets(
        result.raw_packet, result.runner_packet);
    if (!result.frame_comparison.exact()) {
        result.error = Ir4QualificationError::FrameMismatch;
    } else if (!result.packet_comparison.exact()) {
        result.error = Ir4QualificationError::PacketMismatch;
    }
    return result;
}

}  // namespace emberlights
