#pragma once

#include "emberlights/fixture_capabilities.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace emberlights {

enum class FixtureControllerBindingError : std::uint8_t {
    None,
    TargetNotFound,
    TargetEmpty,
    ChoiceNotFound,
    SafetyGateRequired,
    InconsistentChoice,
    UnsupportedPrototype,
    ProjectCapacityExceeded,
    GestureFanoutExceeded
};

struct FixtureControllerBindingOptions {
    // Safety-gated profile functions are never planned unless the authoring
    // caller supplies an explicit, separately confirmed opt-in.
    bool allow_safety_gated{false};
};

struct FixtureControllerBindingPlan {
    FixtureControllerBindingError error{FixtureControllerBindingError::None};
    bool target_found{false};
    bool group{false};
    bool expanded_to_fixtures{false};
    std::vector<MidiMappingDefinition> mappings;
    std::vector<std::string> warnings;
    std::string message;

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == FixtureControllerBindingError::None &&
            !mappings.empty();
    }
};

// Plans an atomic, persistent controller mapping from one profile-backed fixture
// function. The prototype supplies only the input device/gesture/transform
// contract; target, action, property, semantic range, and stable authoring
// provenance are derived from the shared fixture-control catalog.
//
// A homogeneous group uses one SetGroupProperty mapping. A mixed-profile
// group expands to exact per-fixture SetProperty mappings, or fails without
// returning a partial plan when bounded runtime/project capacity is exceeded.
[[nodiscard]] FixtureControllerBindingPlan plan_fixture_controller_binding(
    const ProjectDocument& project,
    std::string_view target_id,
    std::string_view fixture_control_choice_id,
    const MidiMappingDefinition& prototype,
    FixtureControllerBindingOptions options = {});

}  // namespace emberlights
