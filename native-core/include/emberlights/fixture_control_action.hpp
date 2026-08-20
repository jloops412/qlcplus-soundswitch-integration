#pragma once

#include "emberlights/ember_action_executor.hpp"
#include "emberlights/fixture_capabilities.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace emberlights {

enum class FixtureControlActionError : std::uint8_t {
    None,
    InvalidSelection,
    TargetNotFound,
    TargetEmpty,
    ChoiceNotFound,
    StaleChoice,
    ProtectedChoice,
    SafetyGateRequired,
    FixtureNotLiveCompatible,
    GroupNotLiveCompatible,
    CompilationFailed
};

struct FixtureControlActionOptions {
    // The position used to obtain `choice` from fixture_control_choices().
    // Slot functions ignore it. Continuous functions are deliberately frozen
    // at this exact position until executable Ember Actions support MapValue.
    float catalog_position{0.5F};

    // This is authoring confirmation only. The generated Action still invokes
    // the ordinary registered property command, so its core safety gate remains
    // authoritative at execution time.
    bool allow_safety_gated{false};
};

struct FixtureControlActionPlan {
    FixtureControlActionError error{FixtureControlActionError::InvalidSelection};
    bool target_found{false};
    bool group{false};
    bool continuous_fixed_position{false};
    bool release_on_release{false};
    bool release_on_deactivate{false};
    float normalized_value{0.0F};

    std::string action_id;
    std::string target_id;
    std::string target_parameter_name;
    std::string property_id;
    std::string property_parameter_name;
    std::string canonical_source;
    std::string content_hash;
    std::string message;
    std::vector<std::string> warnings;
    std::vector<EmberActionDiagnostic> diagnostics;

    std::shared_ptr<const EmberActionPreparedSource> prepared;
    std::shared_ptr<const EmberActionIrFoundation> foundation;
    std::shared_ptr<const EmberActionExecutableIr> executable;

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == FixtureControlActionError::None &&
            prepared != nullptr && foundation != nullptr &&
            executable != nullptr;
    }
};

// Pure authoring/compiler planner. It re-resolves the supplied catalog choice
// against the current immutable project before emitting anything. One fixture
// compiles to fixture.override.property.{set,release}; only a complete,
// semantically uniform, single-profile group compiles to the corresponding
// group commands. A mixed/partial group fails rather than becoming a sequence
// of non-atomic Runner commands.
//
// The current executable Action subset cannot encode typed StableId/enum
// literals. The generated artifact therefore declares two defaulted bound
// parameters; the property enum is a singleton, and the invocation helper
// supplies the exact stable target captured by the plan. Use
// fixture_control_action_runtime_parameters() when invoking this plan; it
// returns those parameters in the compiled slot order.
[[nodiscard]] FixtureControlActionPlan plan_fixture_control_action(
    const ProjectDocument& project,
    std::string_view target_id,
    const FixtureControlChoice& choice,
    const GeneratedUiRegistryEmberActionView& registry,
    FixtureControlActionOptions options = {});

[[nodiscard]] std::vector<EmberActionRuntimeValue>
fixture_control_action_runtime_parameters(
    const FixtureControlActionPlan& plan);

[[nodiscard]] const char* fixture_control_action_error_name(
    FixtureControlActionError error) noexcept;

}  // namespace emberlights
