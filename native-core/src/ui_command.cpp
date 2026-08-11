#include "emberlights/ui_command.hpp"

#include <algorithm>
#include <cmath>

namespace emberlights {
namespace {

[[nodiscard]] bool is_hazard_property(showcore::Property property) noexcept {
    return property == showcore::Property::Fog ||
        property == showcore::Property::Haze ||
        property == showcore::Property::Laser ||
        property == showcore::Property::Spark;
}

}  // namespace

const UiCommandDefinition* find_ui_command(std::string_view id) noexcept {
    const auto found = std::find_if(
        kUiCommandDefinitions.begin(),
        kUiCommandDefinitions.end(),
        [id](const auto& command) { return command.id == id; });
    return found == kUiCommandDefinitions.end() ? nullptr : &*found;
}

const char* ui_invocation_result_name(UiInvocationResult result) noexcept {
    switch (result) {
    case UiInvocationResult::Accepted: return "accepted";
    case UiInvocationResult::NoChange: return "noChange";
    case UiInvocationResult::Unavailable: return "unavailable";
    case UiInvocationResult::InvalidArguments: return "invalidArguments";
    case UiInvocationResult::QueueFull: return "queueFull";
    case UiInvocationResult::SafetyRejected: return "safetyRejected";
    case UiInvocationResult::Unsupported: return "unsupported";
    case UiInvocationResult::InternalError: return "internalError";
    }
    return "internalError";
}

UiInvocationResult UiCommandFacade::invoke(
    const UiCommandInvocation& invocation) noexcept {
    const auto status = runner_.status();
    switch (invocation.command) {
    case UiCommandId::ShowStart:
        return status.state == RunnerState::Stopped
            ? app_.ui_start_show()
            : UiInvocationResult::NoChange;
    case UiCommandId::ShowStop:
        return status.state == RunnerState::Stopped
            ? UiInvocationResult::NoChange
            : app_.ui_stop_show();
    case UiCommandId::ShowToggleRunning:
        return status.state == RunnerState::Stopped
            ? app_.ui_start_show()
            : app_.ui_stop_show();
    case UiCommandId::BlackoutSet:
        if (status.blackout == invocation.bool_value) {
            return UiInvocationResult::NoChange;
        }
        runner_.set_blackout(invocation.bool_value);
        return UiInvocationResult::Accepted;
    case UiCommandId::BlackoutToggle:
        runner_.set_blackout(!status.blackout);
        return UiInvocationResult::Accepted;
    case UiCommandId::WorkLightSet:
        if (status.state == RunnerState::Stopped) {
            return UiInvocationResult::Unavailable;
        }
        if (status.work_light == invocation.bool_value) {
            return UiInvocationResult::NoChange;
        }
        runner_.set_work_light(invocation.bool_value);
        return UiInvocationResult::Accepted;
    case UiCommandId::WorkLightToggle:
        if (status.state == RunnerState::Stopped) {
            return UiInvocationResult::Unavailable;
        }
        runner_.set_work_light(!status.work_light);
        return UiInvocationResult::Accepted;
    case UiCommandId::ReleaseAllOverrides:
        if (status.state == RunnerState::Stopped) {
            return UiInvocationResult::Unavailable;
        }
        return runner_.clear_manual_overrides()
            ? UiInvocationResult::Accepted
            : UiInvocationResult::QueueFull;
    case UiCommandId::ManualBpmSet:
        if (!std::isfinite(invocation.number_value) ||
            invocation.number_value < 20.0 || invocation.number_value > 300.0) {
            return UiInvocationResult::InvalidArguments;
        }
        if (status.state == RunnerState::Stopped) {
            return UiInvocationResult::Unavailable;
        }
        return runner_.set_manual_bpm(invocation.number_value)
            ? UiInvocationResult::Accepted
            : UiInvocationResult::QueueFull;
    case UiCommandId::TapTempo:
        if (status.state == RunnerState::Stopped) {
            return UiInvocationResult::Unavailable;
        }
        return runner_.tap_tempo()
            ? UiInvocationResult::Accepted
            : UiInvocationResult::QueueFull;
    case UiCommandId::HazardSetArmed:
        if (!is_hazard_property(invocation.property)) {
            return UiInvocationResult::InvalidArguments;
        }
        if (status.state == RunnerState::Stopped) {
            return UiInvocationResult::Unavailable;
        }
        return runner_.set_hazard_armed(invocation.property, invocation.bool_value)
            ? UiInvocationResult::Accepted
            : UiInvocationResult::QueueFull;
    case UiCommandId::HazardDisarmAll: {
        if (status.state == RunnerState::Stopped) {
            return UiInvocationResult::Unavailable;
        }
        bool accepted = true;
        accepted = runner_.set_hazard_armed(showcore::Property::Fog, false) && accepted;
        accepted = runner_.set_hazard_armed(showcore::Property::Haze, false) && accepted;
        accepted = runner_.set_hazard_armed(showcore::Property::Laser, false) && accepted;
        accepted = runner_.set_hazard_armed(showcore::Property::Spark, false) && accepted;
        return accepted ? UiInvocationResult::Accepted : UiInvocationResult::QueueFull;
    }
    case UiCommandId::Count: break;
    }
    return UiInvocationResult::Unsupported;
}

}  // namespace emberlights
