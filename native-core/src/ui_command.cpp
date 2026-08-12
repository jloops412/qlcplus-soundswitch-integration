#include "emberlights/ui_command.hpp"

#include "emberlights/fixture_capabilities.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

namespace emberlights {
namespace {

inline constexpr std::size_t kMissingIndex = std::numeric_limits<std::size_t>::max();

[[nodiscard]] bool is_hazard_property(showcore::Property property) noexcept {
    return property == showcore::Property::Fog ||
        property == showcore::Property::Haze ||
        property == showcore::Property::Laser ||
        property == showcore::Property::Spark;
}

[[nodiscard]] bool hazard_is_armed(
    const RunnerStatus& status,
    showcore::Property property) noexcept {
    switch (property) {
    case showcore::Property::Fog: return status.fog_armed;
    case showcore::Property::Haze: return status.haze_armed;
    case showcore::Property::Laser: return status.laser_armed;
    case showcore::Property::Spark: return status.spark_armed;
    default: return true;
    }
}

[[nodiscard]] std::size_t fixture_index(
    const ProjectDocument& project,
    std::string_view id) noexcept {
    const auto found = std::find_if(
        project.fixtures.begin(), project.fixtures.end(),
        [id](const auto& fixture) { return fixture.id == id; });
    return found == project.fixtures.end()
        ? kMissingIndex
        : static_cast<std::size_t>(found - project.fixtures.begin());
}

[[nodiscard]] std::size_t look_index(
    const ProjectDocument& project,
    std::string_view id) noexcept {
    const auto found = std::find_if(
        project.looks.begin(), project.looks.end(),
        [id](const auto& look) { return look.id == id; });
    return found == project.looks.end()
        ? kMissingIndex
        : static_cast<std::size_t>(found - project.looks.begin());
}

[[nodiscard]] std::size_t track_script_index(
    const ProjectDocument& project,
    std::string_view id) noexcept {
    const auto found = std::find_if(
        project.track_scripts.begin(), project.track_scripts.end(),
        [id](const auto& script) { return script.id == id; });
    return found == project.track_scripts.end()
        ? kMissingIndex
        : static_cast<std::size_t>(found - project.track_scripts.begin());
}

[[nodiscard]] const GroupDefinition* find_group(
    const ProjectDocument& project,
    std::string_view id) noexcept {
    const auto found = std::find_if(
        project.groups.begin(), project.groups.end(),
        [id](const auto& group) { return group.id == id; });
    return found == project.groups.end() ? nullptr : &*found;
}

struct ResolvedGroup {
    showcore::FixtureGroup fixtures{};
    bool valid{false};
    bool supports_property{false};
};

[[nodiscard]] ResolvedGroup resolve_group(
    const ProjectDocument& project,
    const GroupDefinition& definition,
    showcore::Property property) noexcept {
    ResolvedGroup result;
    std::array<bool, showcore::kMaxFixtures> included{};
    bool has_member = false;
    for (const auto& fixture_id : definition.fixture_ids) {
        const auto index = fixture_index(project, fixture_id);
        if (index == kMissingIndex || index >= included.size()) {
            return result;
        }
        const auto* profile = find_fixture_profile(
            project, project.fixtures[index].profile_id);
        if (profile == nullptr) {
            return result;
        }
        if (!included[index]) {
            included[index] = true;
            has_member = true;
            if (fixture_profile_supports_property(*profile, property)) {
                result.supports_property = true;
            } else {
                continue;
            }
            if (!result.fixtures.add(static_cast<std::uint16_t>(index))) {
                return result;
            }
        }
    }
    result.valid = has_member;
    return result;
}

[[nodiscard]] showcore::AutoloopAddress resolve_autoloop(
    const ProjectDocument& project,
    const UiCommandInvocation& invocation) noexcept {
    if (!invocation.target_id.empty()) {
        const auto found = std::find_if(
            project.autoloops.begin(), project.autoloops.end(),
            [&](const auto& loop) { return loop.id == invocation.target_id; });
        return found == project.autoloops.end()
            ? showcore::AutoloopAddress{}
            : showcore::AutoloopAddress{found->bank, found->slot};
    }
    if (!invocation.autoloop_address.valid()) {
        return {};
    }
    const auto found = std::find_if(
        project.autoloops.begin(), project.autoloops.end(),
        [&](const auto& loop) {
            return loop.bank == invocation.autoloop_address.bank &&
                loop.slot == invocation.autoloop_address.slot;
        });
    return found == project.autoloops.end()
        ? showcore::AutoloopAddress{}
        : invocation.autoloop_address;
}

[[nodiscard]] bool valid_override_value(double value) noexcept {
    return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}

[[nodiscard]] UiInvocationResult validate_override_safety(
    const ProjectDocument& project,
    const RunnerStatus& status,
    showcore::Property property,
    double value) noexcept {
    if (value <= 0.0) {
        return UiInvocationResult::Accepted;
    }
    if (is_hazard_property(property) && !hazard_is_armed(status, property)) {
        return UiInvocationResult::SafetyRejected;
    }
    if (property == showcore::Property::Strobe && !project.safety.strobe_allowed) {
        return UiInvocationResult::SafetyRejected;
    }
    return UiInvocationResult::Accepted;
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
    case UiInvocationResult::NotFound: return "notFound";
    case UiInvocationResult::ValidationFailed: return "validationFailed";
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
    case UiCommandId::StaticLookActivate:
    case UiCommandId::StaticLookToggle:
    case UiCommandId::StaticLookHold: {
        if (invocation.target_id.empty()) {
            return UiInvocationResult::InvalidArguments;
        }
        if (status.state != RunnerState::Running || active_project_ == nullptr) {
            return UiInvocationResult::Unavailable;
        }
        const auto index = look_index(*active_project_, invocation.target_id);
        if (index == kMissingIndex) {
            return UiInvocationResult::NotFound;
        }
        if (index > std::numeric_limits<std::uint16_t>::max()) {
            return UiInvocationResult::ValidationFailed;
        }
        auto owner = invocation.static_look_owner;
        if (owner.kind == StaticLookOwnerKind::None) {
            owner.kind = StaticLookOwnerKind::Ui;
        }
        bool posted = false;
        if (invocation.command == UiCommandId::StaticLookActivate) {
            posted = runner_.trigger_look(
                static_cast<std::uint16_t>(index), owner);
        } else if (invocation.command == UiCommandId::StaticLookToggle) {
            posted = runner_.toggle_look(
                static_cast<std::uint16_t>(index), owner);
        } else {
            if (owner.feedback_token == 0U) {
                return UiInvocationResult::InvalidArguments;
            }
            const auto& current = status.static_look;
            const bool same_owner =
                current.owner_kind == owner.kind &&
                current.owner_feedback_token == owner.feedback_token;
            if (invocation.bool_value &&
                current.look_index == static_cast<std::int32_t>(index) &&
                current.behavior == StaticLookBehavior::Hold &&
                current.status != StaticLookActivationStatus::None &&
                current.status != StaticLookActivationStatus::Releasing &&
                same_owner) {
                return UiInvocationResult::NoChange;
            }
            if (!invocation.bool_value &&
                (owner.expected_package_generation == 0U ||
                 owner.expected_activation_generation == 0U)) {
                return UiInvocationResult::InvalidArguments;
            }
            if (!invocation.bool_value &&
                (current.look_index != static_cast<std::int32_t>(index) ||
                 current.behavior != StaticLookBehavior::Hold ||
                 current.status == StaticLookActivationStatus::None ||
                 current.status == StaticLookActivationStatus::Releasing ||
                 !same_owner ||
                 current.package_generation !=
                     owner.expected_package_generation ||
                 current.activation_generation !=
                     owner.expected_activation_generation)) {
                return UiInvocationResult::NoChange;
            }
            posted = runner_.hold_look(
                static_cast<std::uint16_t>(index), invocation.bool_value, owner);
        }
        return posted ? UiInvocationResult::Accepted : UiInvocationResult::QueueFull;
    }
    case UiCommandId::StaticLookClear:
        if (status.state != RunnerState::Running) {
            return UiInvocationResult::Unavailable;
        }
        if (status.static_look.status == StaticLookActivationStatus::None ||
            status.static_look.status == StaticLookActivationStatus::Releasing) {
            return UiInvocationResult::NoChange;
        }
        return runner_.clear_look()
            ? UiInvocationResult::Accepted
            : UiInvocationResult::QueueFull;
    case UiCommandId::AutoloopLaunch: {
        if (invocation.target_id.empty() && !invocation.autoloop_address.valid()) {
            return UiInvocationResult::InvalidArguments;
        }
        if (status.state != RunnerState::Running || active_project_ == nullptr) {
            return UiInvocationResult::Unavailable;
        }
        const auto address = resolve_autoloop(*active_project_, invocation);
        if (!address.valid()) {
            return UiInvocationResult::NotFound;
        }
        return runner_.trigger_autoloop(address)
            ? UiInvocationResult::Accepted
            : UiInvocationResult::QueueFull;
    }
    case UiCommandId::AutoloopClear:
        if (status.state != RunnerState::Running) {
            return UiInvocationResult::Unavailable;
        }
        if (!status.active_autoloop.valid()) {
            return UiInvocationResult::NoChange;
        }
        return runner_.clear_autoloop()
            ? UiInvocationResult::Accepted
            : UiInvocationResult::QueueFull;
    case UiCommandId::AutoloopNext:
        if (status.state != RunnerState::Running) {
            return UiInvocationResult::Unavailable;
        }
        return runner_.next_autoloop()
            ? UiInvocationResult::Accepted
            : UiInvocationResult::QueueFull;
    case UiCommandId::AutoloopPrevious:
        if (status.state != RunnerState::Running) {
            return UiInvocationResult::Unavailable;
        }
        return runner_.previous_autoloop()
            ? UiInvocationResult::Accepted
            : UiInvocationResult::QueueFull;
    case UiCommandId::AutoloopBankFilterEnableAll:
        if (status.state != RunnerState::Running) {
            return UiInvocationResult::Unavailable;
        }
        if (status.active_autoloop_bank_mask ==
            std::numeric_limits<std::uint64_t>::max()) {
            return UiInvocationResult::NoChange;
        }
        return runner_.select_all_autoloop_banks()
            ? UiInvocationResult::Accepted
            : UiInvocationResult::QueueFull;
    case UiCommandId::AutoloopBankFilterSelectExclusive:
        if (invocation.bank >= showcore::kMaxAutoloopBanks) {
            return UiInvocationResult::InvalidArguments;
        }
        if (status.state != RunnerState::Running) {
            return UiInvocationResult::Unavailable;
        }
        if (status.active_autoloop_bank_mask ==
            (std::uint64_t{1} << invocation.bank)) {
            return UiInvocationResult::NoChange;
        }
        return runner_.select_exclusive_autoloop_bank(invocation.bank)
            ? UiInvocationResult::Accepted
            : UiInvocationResult::QueueFull;
    case UiCommandId::AutoloopBankFilterSetEnabled: {
        if (invocation.bank >= showcore::kMaxAutoloopBanks) {
            return UiInvocationResult::InvalidArguments;
        }
        if (status.state != RunnerState::Running) {
            return UiInvocationResult::Unavailable;
        }
        const bool enabled = (status.active_autoloop_bank_mask &
                              (std::uint64_t{1} << invocation.bank)) != 0U;
        if (enabled == invocation.bool_value) {
            return UiInvocationResult::NoChange;
        }
        return runner_.set_autoloop_bank_enabled(invocation.bank, invocation.bool_value)
            ? UiInvocationResult::Accepted
            : UiInvocationResult::QueueFull;
    }
    case UiCommandId::TrackScriptStart: {
        if (invocation.target_id.empty()) {
            return UiInvocationResult::InvalidArguments;
        }
        if (status.state != RunnerState::Running || active_project_ == nullptr) {
            return UiInvocationResult::Unavailable;
        }
        const auto index = track_script_index(*active_project_, invocation.target_id);
        if (index == kMissingIndex) {
            return UiInvocationResult::NotFound;
        }
        if (index > std::numeric_limits<std::uint16_t>::max()) {
            return UiInvocationResult::ValidationFailed;
        }
        return runner_.trigger_track_script(static_cast<std::uint16_t>(index))
            ? UiInvocationResult::Accepted
            : UiInvocationResult::QueueFull;
    }
    case UiCommandId::TrackScriptClear:
        if (status.state != RunnerState::Running) {
            return UiInvocationResult::Unavailable;
        }
        if (status.active_track_script < 0) {
            return UiInvocationResult::NoChange;
        }
        return runner_.clear_track_script()
            ? UiInvocationResult::Accepted
            : UiInvocationResult::QueueFull;
    case UiCommandId::FixtureOverridePropertySet:
    case UiCommandId::FixtureOverridePropertyRelease: {
        const bool active = invocation.command == UiCommandId::FixtureOverridePropertySet;
        if (invocation.target_id.empty() ||
            invocation.property >= showcore::Property::Count ||
            (active && !valid_override_value(invocation.number_value))) {
            return UiInvocationResult::InvalidArguments;
        }
        if (status.state != RunnerState::Running || active_project_ == nullptr) {
            return UiInvocationResult::Unavailable;
        }
        const auto index = fixture_index(*active_project_, invocation.target_id);
        if (index == kMissingIndex) {
            return UiInvocationResult::NotFound;
        }
        const auto* profile = find_fixture_profile(
            *active_project_, active_project_->fixtures[index].profile_id);
        if (profile == nullptr) {
            return UiInvocationResult::ValidationFailed;
        }
        if (!fixture_profile_supports_property(*profile, invocation.property)) {
            return UiInvocationResult::Unsupported;
        }
        if (active) {
            const auto safety = validate_override_safety(
                *active_project_, status, invocation.property, invocation.number_value);
            if (safety != UiInvocationResult::Accepted) {
                return safety;
            }
        }
        return runner_.set_property(
                   static_cast<std::uint16_t>(index),
                   invocation.property,
                   active ? static_cast<float>(invocation.number_value) : 0.0F,
                   active)
            ? UiInvocationResult::Accepted
            : UiInvocationResult::QueueFull;
    }
    case UiCommandId::GroupOverridePropertySet:
    case UiCommandId::GroupOverridePropertyRelease: {
        const bool active = invocation.command == UiCommandId::GroupOverridePropertySet;
        if (invocation.target_id.empty() ||
            invocation.property >= showcore::Property::Count ||
            (active && !valid_override_value(invocation.number_value))) {
            return UiInvocationResult::InvalidArguments;
        }
        if (status.state != RunnerState::Running || active_project_ == nullptr) {
            return UiInvocationResult::Unavailable;
        }
        const auto* definition = find_group(*active_project_, invocation.target_id);
        if (definition == nullptr) {
            return UiInvocationResult::NotFound;
        }
        const auto group = resolve_group(
            *active_project_, *definition, invocation.property);
        if (!group.valid) {
            return UiInvocationResult::ValidationFailed;
        }
        if (!group.supports_property) {
            return UiInvocationResult::Unsupported;
        }
        if (active) {
            const auto safety = validate_override_safety(
                *active_project_, status, invocation.property, invocation.number_value);
            if (safety != UiInvocationResult::Accepted) {
                return safety;
            }
        }
        return runner_.set_group_property(
                   group.fixtures,
                   invocation.property,
                   active ? static_cast<float>(invocation.number_value) : 0.0F,
                   active)
            ? UiInvocationResult::Accepted
            : UiInvocationResult::QueueFull;
    }
    case UiCommandId::Count: break;
    }
    return UiInvocationResult::Unsupported;
}

}  // namespace emberlights
