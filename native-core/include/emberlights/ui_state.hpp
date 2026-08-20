#pragma once

#include "emberlights/generated/ui_registry.generated.hpp"
#include "emberlights/runner.hpp"

#include <array>
#include <cstdint>

namespace emberlights {

struct LiveCoreUiState {
    std::uint64_t generation{0U};
    std::uint64_t frames{0U};
    RunnerState runner{RunnerState::Stopped};
    AdapterState os2l{AdapterState::Disabled};
    AdapterState os2l_discovery{AdapterState::Disabled};
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
    RunnerStaticLookActivation static_look{};
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

[[nodiscard]] const char* static_look_owner_kind_name(
    StaticLookOwnerKind kind) noexcept;
[[nodiscard]] const char* static_look_behavior_name(
    StaticLookBehavior behavior) noexcept;
[[nodiscard]] const char* static_look_activation_status_name(
    StaticLookActivationStatus status) noexcept;

}  // namespace emberlights
