#include "emberlights/autoloop_fixture_controls.hpp"

#include "showcore/autoloop_program.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace emberlights {
namespace {

template <typename Collection>
[[nodiscard]] auto find_by_id(Collection& collection, std::string_view id) {
    return std::find_if(
        collection.begin(), collection.end(),
        [id](const auto& value) { return value.id == id; });
}

[[nodiscard]] bool normalized(float value) noexcept {
    return std::isfinite(value) && value >= 0.0F && value <= 1.0F;
}

[[nodiscard]] bool valid_source_id(std::string_view value) noexcept {
    return !value.empty() &&
        value.size() <= kMaximumAutoloopSourceIdentifierLength;
}

[[nodiscard]] bool overlaps(
    MusicalTick first_start,
    MusicalTick first_end,
    MusicalTick second_start,
    MusicalTick second_end) noexcept {
    return first_start < second_end && second_start < first_end;
}

[[nodiscard]] bool has_opaque_ownership(
    AutoloopEventKind kind) noexcept {
    return kind == AutoloopEventKind::LegacyLook ||
        kind == AutoloopEventKind::Palette ||
        kind == AutoloopEventKind::Position ||
        kind == AutoloopEventKind::Attribute ||
        kind == AutoloopEventKind::Movement;
}

[[nodiscard]] bool target_selects_fixture(
    const ProjectDocument& project,
    const AutoloopTargetDefinition& target,
    std::string_view fixture_id) {
    switch (target.kind) {
    case AutoloopTargetKind::Master:
        return true;
    case AutoloopTargetKind::Fixture:
        return target.stable_ref == fixture_id;
    case AutoloopTargetKind::Group: {
        const auto group = std::find_if(
            project.groups.begin(), project.groups.end(),
            [&target](const auto& candidate) {
                return candidate.id == target.stable_ref;
            });
        return group != project.groups.end() &&
            std::find(
                group->fixture_ids.begin(), group->fixture_ids.end(),
                fixture_id) != group->fixture_ids.end();
    }
    case AutoloopTargetKind::RoleSelector: {
        const auto fixture = std::find_if(
            project.fixtures.begin(), project.fixtures.end(),
            [fixture_id](const auto& candidate) {
                return candidate.id == fixture_id;
            });
        return fixture != project.fixtures.end() &&
            std::find(
                fixture->roles.begin(), fixture->roles.end(),
                target.stable_ref) != fixture->roles.end();
    }
    case AutoloopTargetKind::Count:
        break;
    }
    return false;
}

[[nodiscard]] bool can_add(
    std::size_t count,
    std::size_t addition,
    std::size_t limit) noexcept {
    return count <= limit && addition <= limit - count;
}

[[nodiscard]] std::size_t total_targets(
    const AutoloopSourceDocument& source) noexcept {
    std::size_t result = 0U;
    for (const auto& program : source.programs) {
        if (program.targets.size() >
            std::numeric_limits<std::size_t>::max() - result) {
            return std::numeric_limits<std::size_t>::max();
        }
        result += program.targets.size();
    }
    return result;
}

[[nodiscard]] std::size_t total_events(
    const AutoloopSourceDocument& source) noexcept {
    std::size_t result = 0U;
    for (const auto& program : source.programs) {
        if (program.events.size() >
            std::numeric_limits<std::size_t>::max() - result) {
            return std::numeric_limits<std::size_t>::max();
        }
        result += program.events.size();
    }
    return result;
}

[[nodiscard]] AutoloopFixtureControlProposal proposal_result(
    const AutoloopAuthoringSnapshot& snapshot,
    const AutoloopFixtureControlRequest& request,
    AutoloopFixtureControlResult result,
    std::string message) {
    AutoloopFixtureControlProposal proposal;
    proposal.result = result;
    proposal.expected_generation = request.expected_generation;
    proposal.current_generation = snapshot.generation;
    proposal.message = std::move(message);
    return proposal;
}

[[nodiscard]] AutoloopFixtureControlOutcome outcome_from_proposal(
    AutoloopFixtureControlProposal proposal) {
    AutoloopFixtureControlOutcome outcome;
    outcome.result = proposal.result;
    outcome.expected_generation = proposal.expected_generation;
    outcome.generation = proposal.current_generation;
    outcome.validation = std::move(proposal.validation);
    outcome.writes = std::move(proposal.writes);
    outcome.warnings = std::move(proposal.warnings);
    outcome.message = std::move(proposal.message);
    return outcome;
}

}  // namespace

AutoloopFixtureControlProposal plan_autoloop_fixture_control(
    const AutoloopAuthoringSnapshot& snapshot,
    const ProjectDocument& project,
    const AutoloopFixtureControlRequest& request) {
    if (request.expected_generation != snapshot.generation) {
        return proposal_result(
            snapshot, request,
            AutoloopFixtureControlResult::StaleGeneration,
            "The Autoloop source changed after this Fixture Attribute edit began.");
    }
    if (request.program_id.empty() || request.target_id.empty() ||
        request.choice_id.empty() || request.stable_id_prefix.empty() ||
        request.maximum_fixture_writes == 0U ||
        !normalized(request.position)) {
        return proposal_result(
            snapshot, request,
            AutoloopFixtureControlResult::InvalidRequest,
            "The Fixture Attribute request is incomplete or outside its bounded values.");
    }

    const auto source_program = find_by_id(
        snapshot.source.programs, request.program_id);
    if (source_program == snapshot.source.programs.end()) {
        return proposal_result(
            snapshot, request,
            AutoloopFixtureControlResult::ProgramNotFound,
            "The requested Autoloop program does not exist.");
    }
    if (request.start_tick < 0 || request.end_tick <= request.start_tick ||
        request.end_tick > source_program->length_ticks) {
        return proposal_result(
            snapshot, request,
            AutoloopFixtureControlResult::InvalidRequest,
            "The Fixture Attribute event needs a valid half-open range inside the program.");
    }

    const auto catalog = fixture_control_choices(
        project, request.target_id, request.position);
    if (!catalog.target_found) {
        return proposal_result(
            snapshot, request,
            AutoloopFixtureControlResult::TargetNotFound,
            "The requested fixture or group no longer exists.");
    }
    if (catalog.target_fixture_count == 0U) {
        return proposal_result(
            snapshot, request,
            AutoloopFixtureControlResult::EmptyTarget,
            "The requested target contains no patched fixtures.");
    }
    const auto selected = std::find_if(
        catalog.choices.begin(), catalog.choices.end(),
        [&request](const auto& choice) {
            return choice.id == request.choice_id;
        });
    if (selected == catalog.choices.end() || selected->values.empty()) {
        return proposal_result(
            snapshot, request,
            AutoloopFixtureControlResult::ChoiceNotFound,
            "The Fixture Attribute is missing or no longer supported.");
    }
    if (selected->safety_gated()) {
        return proposal_result(
            snapshot, request,
            AutoloopFixtureControlResult::SafetyGateRequired,
            "Safety-gated Fixture Attributes require a future explicit safety-authoring contract.");
    }
    if (selected->values.size() > request.maximum_fixture_writes) {
        return proposal_result(
            snapshot, request,
            AutoloopFixtureControlResult::CapacityExceeded,
            "The Fixture Attribute exceeds this authoring gesture's write budget.");
    }

    std::unordered_set<std::string_view> fixture_ids;
    for (const auto& value : selected->values) {
        if (value.fixture_id.empty() ||
            value.property >= showcore::Property::Count ||
            value.property != selected->property ||
            !normalized(value.normalized_value) ||
            !fixture_ids.insert(value.fixture_id).second) {
            return proposal_result(
                snapshot, request,
                AutoloopFixtureControlResult::ValidationFailed,
                "The resolved Fixture Attribute values are not a unique normalized semantic set.");
        }
    }

    const showcore::AutoloopCompileLimits compile_limits;
    const auto write_count = selected->values.size();
    if (!can_add(
            total_targets(snapshot.source), write_count,
            compile_limits.maximum_target_spans) ||
        !can_add(
            total_events(snapshot.source), write_count,
            compile_limits.maximum_events) ||
        !can_add(
            source_program->targets.size(), write_count,
            source_program->targets.max_size()) ||
        !can_add(
            source_program->lanes.size(), write_count,
            source_program->lanes.max_size()) ||
        !can_add(
            source_program->events.size(), write_count,
            source_program->events.max_size())) {
        return proposal_result(
            snapshot, request,
            AutoloopFixtureControlResult::CapacityExceeded,
            "The Autoloop source cannot accept the expanded Fixture Attribute records.");
    }

    AutoloopFixtureControlProposal proposal;
    proposal.result = AutoloopFixtureControlResult::Prepared;
    proposal.expected_generation = request.expected_generation;
    proposal.current_generation = snapshot.generation;
    proposal.writes.reserve(write_count);
    proposal.warnings = catalog.warnings;
    if (selected->partial()) {
        proposal.warnings.push_back(
            selected->name + " resolves for " +
            std::to_string(selected->supported_fixture_count) + " of " +
            std::to_string(selected->target_fixture_count) +
            " fixtures; unsupported members receive no event.");
    }
    if (catalog.group && !selected->shared_value) {
        proposal.warnings.push_back(
            selected->name +
            " uses profile-specific values; the Autoloop keeps one exact fixture event per profile realization.");
    }

    for (std::size_t index = 0U; index < write_count; ++index) {
        const auto suffix = std::to_string(index + 1U);
        AutoloopFixtureControlWrite write;
        write.fixture_id = selected->values[index].fixture_id;
        write.target_id = request.stable_id_prefix + ".target." + suffix;
        write.lane_id = request.stable_id_prefix + ".lane." + suffix;
        write.event_id = request.stable_id_prefix + ".event." + suffix;
        write.property = selected->values[index].property;
        write.normalized_value = selected->values[index].normalized_value;
        if (!valid_source_id(write.target_id) ||
            !valid_source_id(write.lane_id) ||
            !valid_source_id(write.event_id)) {
            return proposal_result(
                snapshot, request,
                AutoloopFixtureControlResult::InvalidRequest,
                "Derived Fixture Attribute record IDs exceed the source identifier limit.");
        }
        if (find_by_id(source_program->targets, write.target_id) !=
                source_program->targets.end() ||
            find_by_id(source_program->lanes, write.lane_id) !=
                source_program->lanes.end() ||
            find_by_id(source_program->events, write.event_id) !=
                source_program->events.end()) {
            return proposal_result(
                snapshot, request,
                AutoloopFixtureControlResult::IdentifierCollision,
                "A derived Fixture Attribute record ID already exists in the program.");
        }
        proposal.writes.push_back(std::move(write));
    }

    std::unordered_map<
        std::string_view, const AutoloopTargetDefinition*> targets;
    for (const auto& target : source_program->targets) {
        targets.emplace(target.id, &target);
    }
    struct LaneContext {
        const AutoloopLaneDefinition* lane{nullptr};
        const AutoloopTargetDefinition* target{nullptr};
    };
    std::unordered_map<std::string_view, LaneContext> lanes;
    for (const auto& lane : source_program->lanes) {
        const auto target = targets.find(lane.target_id);
        lanes.emplace(
            lane.id,
            LaneContext{
                &lane,
                target == targets.end() ? nullptr : target->second});
    }
    for (const auto& write : proposal.writes) {
        for (const auto& event : source_program->events) {
            const auto lane = lanes.find(event.lane_id);
            if (lane == lanes.end() || lane->second.lane == nullptr ||
                lane->second.target == nullptr ||
                lane->second.lane->priority != request.lane_priority ||
                !target_selects_fixture(
                    project, *lane->second.target, write.fixture_id) ||
                !overlaps(
                    event.start_tick, event.end_tick,
                    request.start_tick, request.end_tick)) {
                continue;
            }
            if (event.property == write.property ||
                has_opaque_ownership(event.kind)) {
                return proposal_result(
                    snapshot, request,
                    AutoloopFixtureControlResult::OwnershipConflict,
                    "An equal-priority event already owns this fixture property in the requested range.");
            }
        }
    }

    proposal.candidate = snapshot.source;
    auto candidate_program = find_by_id(
        proposal.candidate.programs, request.program_id);
    if (candidate_program == proposal.candidate.programs.end()) {
        return proposal_result(
            snapshot, request,
            AutoloopFixtureControlResult::ProgramNotFound,
            "The requested Autoloop program disappeared while building the candidate.");
    }
    for (const auto& write : proposal.writes) {
        candidate_program->targets.push_back({
            write.target_id,
            AutoloopTargetKind::Fixture,
            write.fixture_id,
            {write.property}});
        candidate_program->lanes.push_back({
            write.lane_id,
            write.target_id,
            request.lane_priority});
        AutoloopEventDefinition event;
        event.id = write.event_id;
        event.lane_id = write.lane_id;
        event.kind = AutoloopEventKind::PropertyBlock;
        event.start_tick = request.start_tick;
        event.end_tick = request.end_tick;
        event.property = write.property;
        event.value = showcore::PropertyValue::set(write.normalized_value);
        event.interpolation = AutoloopInterpolation::Hold;
        event.payload_version = 1U;
        candidate_program->events.push_back(std::move(event));
    }
    normalize_autoloop_source(proposal.candidate);
    proposal.validation = validate_autoloop_source(proposal.candidate);
    if (!proposal.validation.ok()) {
        proposal.result = AutoloopFixtureControlResult::ValidationFailed;
        proposal.message =
            "The expanded Fixture Attribute source candidate failed validation.";
        proposal.candidate = {};
        return proposal;
    }
    const auto canonical = serialize_autoloop_source(proposal.candidate);
    if (canonical.empty()) {
        proposal.result = AutoloopFixtureControlResult::ValidationFailed;
        proposal.message =
            "The expanded Fixture Attribute source candidate could not be canonicalized.";
        proposal.candidate = {};
        return proposal;
    }
    if (canonical.size() > compile_limits.maximum_canonical_bytes) {
        proposal.result = AutoloopFixtureControlResult::CapacityExceeded;
        proposal.message =
            "The expanded Fixture Attribute source exceeds the compiled canonical-byte arena.";
        proposal.candidate = {};
        return proposal;
    }
    proposal.message =
        "The Fixture Attribute is ready as exact per-fixture semantic events.";
    return proposal;
}

AutoloopFixtureControlOutcome apply_autoloop_fixture_control(
    AutoloopAuthoringService& service,
    const ProjectDocument& project,
    const AutoloopFixtureControlRequest& request) {
    auto proposal = plan_autoloop_fixture_control(
        service.snapshot(), project, request);
    if (!proposal) {
        return outcome_from_proposal(std::move(proposal));
    }

    auto writes = std::move(proposal.writes);
    auto applied = service.apply_candidate(
        request.expected_generation, std::move(proposal.candidate));
    AutoloopFixtureControlOutcome outcome;
    outcome.expected_generation = request.expected_generation;
    outcome.generation = applied.generation;
    outcome.validation = std::move(applied.validation);
    outcome.writes = std::move(writes);
    outcome.warnings = std::move(proposal.warnings);
    outcome.message = std::move(applied.message);
    switch (applied.result) {
    case AutoloopAuthoringResult::Applied:
        outcome.result = AutoloopFixtureControlResult::Applied;
        break;
    case AutoloopAuthoringResult::NoChange:
        outcome.result = AutoloopFixtureControlResult::NoChange;
        break;
    case AutoloopAuthoringResult::StaleGeneration:
        outcome.result = AutoloopFixtureControlResult::StaleGeneration;
        outcome.writes.clear();
        break;
    case AutoloopAuthoringResult::CapacityExceeded:
        outcome.result = AutoloopFixtureControlResult::CapacityExceeded;
        outcome.writes.clear();
        break;
    case AutoloopAuthoringResult::GenerationExhausted:
        outcome.result = AutoloopFixtureControlResult::GenerationExhausted;
        outcome.writes.clear();
        break;
    case AutoloopAuthoringResult::ValidationFailed:
    case AutoloopAuthoringResult::InvalidCandidate:
    case AutoloopAuthoringResult::MissingAsset:
    case AutoloopAuthoringResult::MissingPlacement:
    case AutoloopAuthoringResult::OccupiedPlacement:
    case AutoloopAuthoringResult::DependencyConflict:
    case AutoloopAuthoringResult::UndoUnavailable:
    case AutoloopAuthoringResult::RedoUnavailable:
        outcome.result = AutoloopFixtureControlResult::ValidationFailed;
        outcome.writes.clear();
        break;
    }
    return outcome;
}

const char* autoloop_fixture_control_result_name(
    AutoloopFixtureControlResult result) noexcept {
    switch (result) {
    case AutoloopFixtureControlResult::Prepared: return "prepared";
    case AutoloopFixtureControlResult::Applied: return "applied";
    case AutoloopFixtureControlResult::NoChange: return "noChange";
    case AutoloopFixtureControlResult::StaleGeneration: return "staleGeneration";
    case AutoloopFixtureControlResult::InvalidRequest: return "invalidRequest";
    case AutoloopFixtureControlResult::TargetNotFound: return "targetNotFound";
    case AutoloopFixtureControlResult::EmptyTarget: return "emptyTarget";
    case AutoloopFixtureControlResult::ChoiceNotFound: return "choiceNotFound";
    case AutoloopFixtureControlResult::SafetyGateRequired:
        return "safetyGateRequired";
    case AutoloopFixtureControlResult::ProgramNotFound: return "programNotFound";
    case AutoloopFixtureControlResult::IdentifierCollision:
        return "identifierCollision";
    case AutoloopFixtureControlResult::OwnershipConflict:
        return "ownershipConflict";
    case AutoloopFixtureControlResult::CapacityExceeded: return "capacityExceeded";
    case AutoloopFixtureControlResult::ValidationFailed: return "validationFailed";
    case AutoloopFixtureControlResult::GenerationExhausted:
        return "generationExhausted";
    }
    return "unknown";
}

}  // namespace emberlights
