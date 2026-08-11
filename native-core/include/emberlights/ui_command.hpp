#pragma once

#include "emberlights/runner.hpp"

#include <array>
#include <cstdint>
#include <string_view>

namespace emberlights {

enum class UiCommandId : std::uint8_t {
    ShowStart,
    ShowStop,
    ShowToggleRunning,
    BlackoutSet,
    BlackoutToggle,
    WorkLightSet,
    WorkLightToggle,
    ReleaseAllOverrides,
    ManualBpmSet,
    TapTempo,
    HazardSetArmed,
    HazardDisarmAll,
    Count
};

enum class UiInvocationResult : std::uint8_t {
    Accepted,
    NoChange,
    Unavailable,
    InvalidArguments,
    QueueFull,
    SafetyRejected,
    Unsupported,
    InternalError
};

enum class UiCommandInteraction : std::uint8_t {
    Trigger,
    Toggle,
    Absolute
};

struct UiCommandDefinition {
    UiCommandId command{UiCommandId::ShowStart};
    std::string_view id{};
    std::string_view label{};
    UiCommandInteraction interaction{UiCommandInteraction::Trigger};
    bool emergency{false};
    bool midi_bindable{true};
    bool keyboard_bindable{true};
};

inline constexpr std::array<UiCommandDefinition,
                            static_cast<std::size_t>(UiCommandId::Count)>
    kUiCommandDefinitions{{
        {UiCommandId::ShowStart, "show.start", "Start show", UiCommandInteraction::Trigger},
        {UiCommandId::ShowStop, "show.stop", "Stop show", UiCommandInteraction::Trigger},
        {UiCommandId::ShowToggleRunning, "show.toggleRunning", "Start or stop show", UiCommandInteraction::Toggle},
        {UiCommandId::BlackoutSet, "output.blackout.set", "Set blackout", UiCommandInteraction::Absolute, true},
        {UiCommandId::BlackoutToggle, "output.blackout.toggle", "Toggle blackout", UiCommandInteraction::Toggle, true},
        {UiCommandId::WorkLightSet, "output.workLight.set", "Set work light", UiCommandInteraction::Absolute},
        {UiCommandId::WorkLightToggle, "output.workLight.toggle", "Toggle work light", UiCommandInteraction::Toggle},
        {UiCommandId::ReleaseAllOverrides, "override.releaseAll", "Release all overrides", UiCommandInteraction::Trigger},
        {UiCommandId::ManualBpmSet, "transport.manualBpm.set", "Set manual BPM", UiCommandInteraction::Absolute},
        {UiCommandId::TapTempo, "transport.tap", "Tap tempo", UiCommandInteraction::Trigger},
        {UiCommandId::HazardSetArmed, "safety.hazard.setArmed", "Set hazard armed", UiCommandInteraction::Absolute},
        {UiCommandId::HazardDisarmAll, "safety.hazard.disarmAll", "Disarm all hazards", UiCommandInteraction::Trigger},
    }};

struct UiCommandInvocation {
    UiCommandId command{UiCommandId::ShowStart};
    bool bool_value{false};
    double number_value{0.0};
    showcore::Property property{showcore::Property::Count};
};

class UiAppCommandHost {
public:
    virtual ~UiAppCommandHost() = default;
    [[nodiscard]] virtual UiInvocationResult ui_start_show() noexcept = 0;
    [[nodiscard]] virtual UiInvocationResult ui_stop_show() noexcept = 0;
};

class UiCommandFacade {
public:
    UiCommandFacade(RunnerService& runner, UiAppCommandHost& app) noexcept
        : runner_(runner), app_(app) {}

    [[nodiscard]] UiInvocationResult invoke(
        const UiCommandInvocation& invocation) noexcept;

private:
    RunnerService& runner_;
    UiAppCommandHost& app_;
};

[[nodiscard]] const UiCommandDefinition* find_ui_command(
    std::string_view id) noexcept;
[[nodiscard]] const char* ui_invocation_result_name(
    UiInvocationResult result) noexcept;

}  // namespace emberlights
