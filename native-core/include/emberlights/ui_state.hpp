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

inline constexpr std::array<UiStateDefinition, 14U> kLiveCoreUiStates{{
    {"runner.state", UiStateUpdateClass::Event},
    {"runner.generation", UiStateUpdateClass::Event},
    {"runner.health", UiStateUpdateClass::Health},
    {"transport.bpm", UiStateUpdateClass::Realtime},
    {"transport.syncState", UiStateUpdateClass::Health},
    {"output.blackout", UiStateUpdateClass::Event},
    {"output.workLight", UiStateUpdateClass::Event},
    {"safety.hazard.fog.armed", UiStateUpdateClass::Event},
    {"safety.hazard.haze.armed", UiStateUpdateClass::Event},
    {"safety.hazard.laser.armed", UiStateUpdateClass::Event},
    {"safety.hazard.spark.armed", UiStateUpdateClass::Event},
    {"override.activePropertyCount", UiStateUpdateClass::Event},
    {"output.controlOne.status", UiStateUpdateClass::Health},
    {"output.controlOne.experimental", UiStateUpdateClass::Event},
}};

struct LiveCoreUiState {
    std::uint64_t generation{0U};
    RunnerState runner{RunnerState::Stopped};
    showcore::SyncState sync{showcore::SyncState::Waiting};
    double bpm{0.0};
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
