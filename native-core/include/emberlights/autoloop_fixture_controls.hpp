#pragma once

#include "emberlights/autoloop_authoring.hpp"
#include "emberlights/fixture_capabilities.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace emberlights {

// One bounded authoring gesture may expand a mixed-profile group into one
// exact semantic event per patched fixture. The default matches the project
// fixture bound and can be reduced by callers that need a smaller work budget.
inline constexpr std::size_t kDefaultAutoloopFixtureControlWriteBudget =
    showcore::kMaxFixtures;

struct AutoloopFixtureControlRequest {
    StudioDocumentGeneration expected_generation{0U};
    std::string program_id;
    std::string target_id;
    std::string choice_id;
    // Stable caller-owned prefix used to derive target/lane/event IDs. Reusing
    // a prefix is an explicit collision, never an overwrite operation.
    std::string stable_id_prefix;
    // Canonical half-open musical range [start_tick, end_tick).
    MusicalTick start_tick{0};
    MusicalTick end_tick{kMusicalTicksPerQuarter};
    float position{0.5F};
    std::uint16_t lane_priority{0U};
    std::size_t maximum_fixture_writes{
        kDefaultAutoloopFixtureControlWriteBudget};
};

enum class AutoloopFixtureControlResult : std::uint8_t {
    Prepared,
    Applied,
    NoChange,
    StaleGeneration,
    InvalidRequest,
    TargetNotFound,
    EmptyTarget,
    ChoiceNotFound,
    SafetyGateRequired,
    ProgramNotFound,
    IdentifierCollision,
    OwnershipConflict,
    CapacityExceeded,
    ValidationFailed,
    GenerationExhausted
};

// Toolkit-neutral evidence for every exact semantic source record emitted by
// the gesture. It intentionally contains no DMX channel or byte value.
struct AutoloopFixtureControlWrite {
    std::string fixture_id;
    std::string target_id;
    std::string lane_id;
    std::string event_id;
    showcore::Property property{showcore::Property::Count};
    float normalized_value{0.0F};
};

struct AutoloopFixtureControlProposal {
    AutoloopFixtureControlResult result{
        AutoloopFixtureControlResult::InvalidRequest};
    StudioDocumentGeneration expected_generation{0U};
    StudioDocumentGeneration current_generation{0U};
    AutoloopSourceDocument candidate;
    AutoloopSourceValidation validation;
    std::vector<AutoloopFixtureControlWrite> writes;
    std::vector<std::string> warnings;
    std::string message;

    [[nodiscard]] explicit operator bool() const noexcept {
        return result == AutoloopFixtureControlResult::Prepared;
    }
};

struct AutoloopFixtureControlOutcome {
    AutoloopFixtureControlResult result{
        AutoloopFixtureControlResult::InvalidRequest};
    StudioDocumentGeneration expected_generation{0U};
    StudioDocumentGeneration generation{0U};
    AutoloopSourceValidation validation;
    std::vector<AutoloopFixtureControlWrite> writes;
    std::vector<std::string> warnings;
    std::string message;

    [[nodiscard]] explicit operator bool() const noexcept {
        return result == AutoloopFixtureControlResult::Applied ||
            result == AutoloopFixtureControlResult::NoChange;
    }
};

// Pure proposal builder over one immutable authoring snapshot. Fixture attributes
// come only from fixture_control_choices. Group functions always expand to
// per-fixture targets so profile-specific normalized values remain exact.
[[nodiscard]] AutoloopFixtureControlProposal
plan_autoloop_fixture_control(
    const AutoloopAuthoringSnapshot& snapshot,
    const ProjectDocument& project,
    const AutoloopFixtureControlRequest& request);

// Rebuilds a proposal from the service's current snapshot and commits its
// complete candidate through AutoloopAuthoringService as one generation-
// checked transaction.
[[nodiscard]] AutoloopFixtureControlOutcome
apply_autoloop_fixture_control(
    AutoloopAuthoringService& service,
    const ProjectDocument& project,
    const AutoloopFixtureControlRequest& request);

[[nodiscard]] const char* autoloop_fixture_control_result_name(
    AutoloopFixtureControlResult result) noexcept;

}  // namespace emberlights
