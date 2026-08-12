#pragma once

#include "emberlights/runner.hpp"

#include <array>
#include <cstdint>
#include <string_view>

namespace emberlights {

enum class UiStateUpdateClass : std::uint8_t {
    Event,
    Health,
    Realtime
};

struct UiStateDefinition {
    std::string_view id{};
    UiStateUpdateClass update_class{UiStateUpdateClass::Event};
};

inline constexpr auto kLiveCoreUiStates = std::to_array<UiStateDefinition>({
    {"project.active.id", UiStateUpdateClass::Event},
    {"project.active.name", UiStateUpdateClass::Event},
    {"runner.state", UiStateUpdateClass::Event},
    {"runner.generation", UiStateUpdateClass::Event},
    {"runner.frames", UiStateUpdateClass::Realtime},
    {"runner.health", UiStateUpdateClass::Health},
    {"transport.bpm", UiStateUpdateClass::Realtime},
    {"transport.beatPosition", UiStateUpdateClass::Realtime},
    {"transport.clockSource", UiStateUpdateClass::Health},
    {"transport.syncState", UiStateUpdateClass::Health},
    {"connection.os2l.status", UiStateUpdateClass::Health},
    {"controller.input.status", UiStateUpdateClass::Health},
    {"controller.output.status", UiStateUpdateClass::Health},
    {"output.artnet.status", UiStateUpdateClass::Health},
    {"output.sacn.status", UiStateUpdateClass::Health},
    {"output.dmxUsbPro[0].status", UiStateUpdateClass::Health},
    {"output.dmxUsbPro[1].status", UiStateUpdateClass::Health},
    {"output.micro.status", UiStateUpdateClass::Health},
    {"output.blackout", UiStateUpdateClass::Event},
    {"output.workLight", UiStateUpdateClass::Event},
    {"safety.hazard.fog.armed", UiStateUpdateClass::Event},
    {"safety.hazard.haze.armed", UiStateUpdateClass::Event},
    {"safety.hazard.laser.armed", UiStateUpdateClass::Event},
    {"safety.hazard.spark.armed", UiStateUpdateClass::Event},
    {"staticLook.active.id", UiStateUpdateClass::Event},
    {"autoloop.active.id", UiStateUpdateClass::Event},
    {"autoloop.active.bank", UiStateUpdateClass::Event},
    {"autoloop.active.slot", UiStateUpdateClass::Event},
    {"autoloop.active.progress", UiStateUpdateClass::Realtime},
    {"autoloop.active.repeat", UiStateUpdateClass::Event},
    {"autoloop.active.completedCycles", UiStateUpdateClass::Event},
    {"autoloop.bankFilter.mask", UiStateUpdateClass::Event},
    {"trackScript.active.id", UiStateUpdateClass::Event},
    {"trackScript.elapsedBeat", UiStateUpdateClass::Realtime},
    {"trackScript.consumedCueCount", UiStateUpdateClass::Event},
    {"override.activePropertyCount", UiStateUpdateClass::Event},
    {"output.controlOne.status", UiStateUpdateClass::Health},
    {"output.controlOne.experimental", UiStateUpdateClass::Event},
});

struct LiveCoreUiState {
    std::uint64_t generation{0U};
    std::uint64_t frames{0U};
    RunnerState runner{RunnerState::Stopped};
    AdapterState os2l{AdapterState::Disabled};
    AdapterState midi_input{AdapterState::Disabled};
    AdapterState midi_output{AdapterState::Disabled};
    AdapterState artnet{AdapterState::Disabled};
    AdapterState sacn{AdapterState::Disabled};
    std::array<AdapterState, showcore::kV1UniverseCount> dmx_usb_pro{
        AdapterState::Disabled,
        AdapterState::Disabled};
    AdapterState soundswitch_micro{AdapterState::Disabled};
    std::array<showcore::OutputBackendHealth, 6U> output_backends{};
    showcore::SyncState sync{showcore::SyncState::Waiting};
    showcore::ClockSource clock_source{showcore::ClockSource::None};
    double bpm{0.0};
    double beat_position{0.0};
    std::int32_t active_look{-1};
    showcore::AutoloopAddress active_autoloop{};
    showcore::AutoloopRepeat active_autoloop_repeat{showcore::AutoloopRepeat::Once};
    float active_autoloop_progress{0.0F};
    std::uint32_t active_autoloop_completed_cycles{0U};
    std::uint64_t active_autoloop_bank_mask{~std::uint64_t{0}};
    std::int32_t active_track_script{-1};
    double active_track_script_beat{0.0};
    std::uint32_t active_track_script_consumed_cues{0U};
    bool blackout{false};
    bool work_light{false};
    bool fog_armed{false};
    bool haze_armed{false};
    bool laser_armed{false};
    bool spark_armed{false};
    std::uint16_t active_override_count{0U};
    AdapterState control_one{AdapterState::Disabled};
};

[[nodiscard]] LiveCoreUiState make_live_core_ui_state(
    const RunnerStatus& status) noexcept;

}  // namespace emberlights
