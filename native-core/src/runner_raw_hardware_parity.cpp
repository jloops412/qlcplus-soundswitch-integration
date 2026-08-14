#include "emberlights/runner_raw_hardware_parity.hpp"

#include "emberlights/file_identity.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <locale>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace emberlights {
namespace {

struct AttemptCandidate {
    RawHardwareTestAttempt attempt;
    bool current_project_basis{false};
    bool stable_patch{false};
    bool reference_requirement_matched{false};
    bool routed_requirement_matched{false};
};

[[nodiscard]] bool audit_record(std::string_view record) noexcept {
    return record.size() > kRawHardwareTestAttemptRecord.size() &&
        record.starts_with(kRawHardwareTestAttemptRecord) &&
        record[kRawHardwareTestAttemptRecord.size()] == '\t';
}

[[nodiscard]] const FixtureQualificationObservation* find_observation(
    const FixtureQualificationAttestation& attestation,
    const FixtureQualificationRequirement* requirement) noexcept {
    if (requirement == nullptr) {
        return nullptr;
    }
    const auto found = std::find_if(
        attestation.observations.begin(),
        attestation.observations.end(),
        [requirement](const auto& observation) {
            return observation.requirement_id == requirement->id &&
                observation.raw_frame_sha256 == requirement->raw_frame_sha256;
        });
    return found == attestation.observations.end() ? nullptr : &*found;
}

[[nodiscard]] const FixtureQualificationRequirement* find_requirement(
    const FixtureQualificationAttestation& attestation,
    std::string_view raw_frame_sha256) noexcept {
    const auto found = std::find_if(
        attestation.requirements.begin(),
        attestation.requirements.end(),
        [raw_frame_sha256](const auto& requirement) {
            return requirement.raw_frame_sha256 == raw_frame_sha256;
        });
    return found == attestation.requirements.end() ? nullptr : &*found;
}

[[nodiscard]] bool stable_fixture_patch(
    const ProjectDocument& project,
    const FixtureQualificationBinding& binding) noexcept {
    const auto fixture = std::find_if(
        project.fixtures.begin(), project.fixtures.end(),
        [&binding](const auto& candidate) {
            return candidate.id == binding.fixture_id;
        });
    if (fixture == project.fixtures.end() ||
        fixture->universe != binding.universe ||
        fixture->address != binding.address ||
        fixture->profile_id != binding.profile_id) {
        return false;
    }
    const auto profile = std::find_if(
        project.fixture_profiles.begin(), project.fixture_profiles.end(),
        [&binding](const auto& candidate) {
            return candidate.id == binding.profile_id;
        });
    return profile != project.fixture_profiles.end() &&
        profile->manufacturer == binding.manufacturer &&
        profile->model == binding.model && profile->mode == binding.mode;
}

[[nodiscard]] bool intrinsic_attempt_evidence_valid(
    const RawHardwareTestAttempt& attempt) noexcept {
    const auto& attestation = attempt.attestation;
    if (attestation.schema_version != kFixtureQualificationAttestationVersion ||
        !is_sha256_digest(attestation.input_project_sha256) ||
        !is_sha256_digest(attestation.candidate_project_sha256) ||
        !is_sha256_digest(attestation.content_sha256) ||
        attestation.operator_id.empty() ||
        attestation.observed_at_utc != attempt.completed_at_utc ||
        attestation.binding.fixture_id.empty() ||
        attestation.binding.unit_label.empty() ||
        attestation.binding.profile_id.empty() ||
        attestation.requirements.empty() ||
        attestation.requirements.size() >
            kMaximumFixtureQualificationRequirements ||
        attestation.observations.size() != attestation.requirements.size() ||
        attestation.candidate_binding_sha256 !=
            fixture_qualification_binding_sha256(attestation.binding) ||
        attempt.frames_attempted == 0U ||
        attempt.frames_accepted != attempt.frames_attempted) {
        return false;
    }
    for (std::size_t index = 0U;
         index < attestation.requirements.size(); ++index) {
        const auto& requirement = attestation.requirements[index];
        if (requirement.id.empty() || requirement.expected_behavior.empty() ||
            !requirement.require_no_spill ||
            requirement.raw_frame_sha256 !=
                fixture_qualification_expected_frame_sha256(requirement)) {
            return false;
        }
        for (std::size_t other = 0U; other < index; ++other) {
            if (attestation.requirements[other].id == requirement.id ||
                attestation.requirements[other].raw_frame_sha256 ==
                    requirement.raw_frame_sha256) {
                return false;
            }
        }
        const auto* observation = find_observation(attestation, &requirement);
        if (observation == nullptr || observation->observed_behavior.empty() ||
            !observation->passed || !observation->no_spill_observed ||
            !observation->blackout_before || !observation->blackout_after ||
            observation->timed_out || observation->device_lost ||
            !observation->failure.empty()) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool historical_requirements_match_current_patch(
    const ProjectDocument& project,
    const RawHardwareTestAttempt& attempt) noexcept {
    const auto& binding = attempt.attestation.binding;
    const auto profile = std::find_if(
        project.fixture_profiles.begin(), project.fixture_profiles.end(),
        [&binding](const auto& candidate) {
            return candidate.id == binding.profile_id;
        });
    if (profile == project.fixture_profiles.end() || profile->footprint == 0U ||
        profile->footprint > showcore::kUniverseSlots || binding.address == 0U ||
        static_cast<std::uint32_t>(binding.address) + profile->footprint >
            showcore::kUniverseSlots + 1U ||
        attempt.attestation.requirements.size() !=
            static_cast<std::size_t>(profile->footprint) + 1U) {
        return false;
    }
    std::array<bool, showcore::kUniverseSlots> covered{};
    std::size_t blackout_count = 0U;
    for (const auto& requirement : attempt.attestation.requirements) {
        if (requirement.kind == FixtureQualificationRequirementKind::Blackout) {
            ++blackout_count;
            continue;
        }
        const auto first = static_cast<std::uint32_t>(binding.address);
        const auto end = first + profile->footprint;
        if (requirement.kind != FixtureQualificationRequirementKind::OneHot ||
            requirement.absolute_channel < first ||
            requirement.absolute_channel >= end) {
            return false;
        }
        const auto offset = static_cast<std::size_t>(
            requirement.absolute_channel - binding.address);
        if (covered[offset]) {
            return false;
        }
        covered[offset] = true;
    }
    return blackout_count == 1U &&
        std::all_of(
            covered.begin(), covered.begin() + profile->footprint,
            [](bool value) { return value; });
}

[[nodiscard]] std::string micro_backend(std::uint8_t universe) {
    return "soundswitch-micro:u" + std::to_string(universe);
}

[[nodiscard]] bool candidate_better(
    const AttemptCandidate& candidate,
    const AttemptCandidate& current) noexcept {
    const auto score = [](const AttemptCandidate& value) {
        unsigned int result = 0U;
        result += value.routed_requirement_matched ? 8U : 0U;
        result += value.reference_requirement_matched ? 4U : 0U;
        result += value.current_project_basis ? 2U : 0U;
        result += value.stable_patch ? 1U : 0U;
        return result;
    };
    const auto candidate_score = score(candidate);
    const auto current_score = score(current);
    if (candidate_score != current_score) {
        return candidate_score > current_score;
    }
    if (candidate.attempt.completed_at_utc != current.attempt.completed_at_utc) {
        return candidate.attempt.completed_at_utc > current.attempt.completed_at_utc;
    }
    return candidate.attempt.content_sha256 < current.attempt.content_sha256;
}

[[nodiscard]] RunnerRawRequirementBinding make_requirement_binding(
    const FixtureQualificationRequirement* requirement,
    const FixtureQualificationObservation* observation) {
    RunnerRawRequirementBinding result;
    if (requirement == nullptr || observation == nullptr) {
        return result;
    }
    result.matched = true;
    result.requirement_id = requirement->id;
    result.kind = requirement->kind;
    result.absolute_channel = requirement->absolute_channel;
    result.value = requirement->value;
    result.raw_frame_sha256 = requirement->raw_frame_sha256;
    result.expected_behavior = requirement->expected_behavior;
    result.observed_behavior = observation->observed_behavior;
    result.passed = observation->passed;
    result.no_spill_observed = observation->no_spill_observed;
    result.blackout_before = observation->blackout_before;
    result.blackout_after = observation->blackout_after;
    return result;
}

[[nodiscard]] std::string yes_no(bool value) {
    return value ? "yes" : "no";
}

void format_requirement(
    std::ostringstream& output,
    std::string_view label,
    const RunnerRawRequirementBinding& binding) {
    output << label << ": ";
    if (!binding.matched) {
        output << "no matching raw-tested frame\r\n";
        return;
    }
    output << binding.requirement_id;
    if (binding.kind == FixtureQualificationRequirementKind::Blackout) {
        output << " (blackout)";
    } else {
        output << " (CH" << binding.absolute_channel << "="
               << static_cast<unsigned int>(binding.value) << ")";
    }
    output << "\r\n  expected: " << binding.expected_behavior
           << "\r\n  raw frame SHA-256: " << binding.raw_frame_sha256
           << "\r\n  prior operator observation: "
           << binding.observed_behavior
           << "\r\n  prior pass/no-spill/blackout-before/blackout-after: "
           << yes_no(binding.passed) << "/"
           << yes_no(binding.no_spill_observed) << "/"
           << yes_no(binding.blackout_before) << "/"
           << yes_no(binding.blackout_after) << "\r\n";
}

}  // namespace

RunnerRawHardwareParityReport bind_runner_frame_to_raw_hardware_attempt(
    const ProjectDocument& project,
    const RunnerOutputSnapshot* snapshot,
    std::uint8_t universe,
    const showcore::DmxUniverse& authored_reference,
    const RunnerFrameInspectionOptions& options) {
    RunnerRawHardwareParityReport report;
    report.universe = universe;
    report.authored_reference_sha256 =
        runner_dmx_universe_sha256(authored_reference);
    if (universe == 0U || universe > showcore::kV1UniverseCount) {
        report.status = RunnerRawHardwareParityStatus::InvalidUniverse;
        return report;
    }

    const auto inspection = inspect_runner_frame(project, snapshot, options);
    report.snapshot_available = inspection.snapshot_available;
    report.snapshot_stale = inspection.stale ||
        inspection.status == RunnerFrameInspectionStatus::InvalidTimestamp;
    report.snapshot_generation = inspection.generation;
    report.snapshot_sequence = inspection.sequence;
    report.snapshot_age_ms = inspection.snapshot_age_ms;
    if (!inspection.snapshot_available || snapshot == nullptr) {
        report.status = RunnerRawHardwareParityStatus::NoSnapshot;
        return report;
    }
    const auto universe_index = static_cast<std::size_t>(universe - 1U);
    report.routed_frame_sha256 =
        inspection.routed_universe_sha256[universe_index];
    report.routed_matches_authored_reference =
        report.routed_frame_sha256 == report.authored_reference_sha256;

    const auto micro_kind = showcore::OutputBackendKind::SoundSwitchMicro;
    const auto route = std::find_if(
        inspection.routes.begin(), inspection.routes.end(),
        [micro_kind](const auto& candidate) {
            return candidate.kind == micro_kind;
        });
    if (route != inspection.routes.end()) {
        report.micro_route = *route;
        const auto first = static_cast<unsigned int>(route->first_source_universe);
        const auto end = first + route->source_universe_count;
        report.micro_route_covers_universe = route->configured &&
            universe >= first && universe < end;
        report.current_host_route_accepted =
            report.micro_route_covers_universe && route->attempted_frames > 0U &&
            route->accepted_frames == route->attempted_frames &&
            route->last_error == 0U;
    }

    std::vector<AttemptCandidate> candidates;
    for (const auto& record : project.unknown_records) {
        if (!audit_record(record)) {
            continue;
        }
        ++report.audit_records_seen;
        RawHardwareTestAttempt attempt;
        const auto parsed = parse_raw_hardware_test_attempt_record(record, attempt);
        if (!parsed.ok()) {
            ++report.invalid_audit_records;
            continue;
        }
        auto resealed = attempt;
        resealed.content_sha256.clear();
        const auto sealed = seal_raw_hardware_test_attempt(resealed);
        if (!sealed.ok() || resealed.content_sha256 != attempt.content_sha256) {
            ++report.invalid_audit_records;
            continue;
        }
        if (attempt.terminal_phase != RawHardwareTestPhase::Complete ||
            attempt.terminal_error != RawHardwareTestError::None ||
            attempt.attestation.binding.universe != universe ||
            attempt.attestation.binding.output_backend != micro_backend(universe)) {
            continue;
        }
        if (!intrinsic_attempt_evidence_valid(attempt)) {
            ++report.invalid_audit_records;
            continue;
        }
        ++report.completed_attempts_considered;
        AttemptCandidate candidate;
        candidate.attempt = std::move(attempt);
        candidate.current_project_basis =
            validate_raw_hardware_test_attempt(project, candidate.attempt).ok();
        candidate.stable_patch = stable_fixture_patch(
            project, candidate.attempt.attestation.binding);
        if (!candidate.current_project_basis &&
            (!candidate.stable_patch ||
             !historical_requirements_match_current_patch(
                 project, candidate.attempt))) {
            ++report.completed_attempts_rejected_for_stale_patch;
            continue;
        }
        candidate.reference_requirement_matched = find_requirement(
            candidate.attempt.attestation,
            report.authored_reference_sha256) != nullptr;
        candidate.routed_requirement_matched = find_requirement(
            candidate.attempt.attestation,
            report.routed_frame_sha256) != nullptr;
        candidates.push_back(std::move(candidate));
    }

    if (!candidates.empty()) {
        const auto best = std::max_element(
            candidates.begin(), candidates.end(),
            [](const auto& left, const auto& right) {
                return candidate_better(right, left);
            });
        const auto& candidate = *best;
        report.attempt_bound = true;
        report.attempt_matches_current_project_basis =
            candidate.current_project_basis;
        report.historical_fixture_patch_stable = candidate.stable_patch;
        report.attempt_sha256 = candidate.attempt.content_sha256;
        report.attestation_sha256 =
            candidate.attempt.attestation.content_sha256;
        report.completed_at_utc = candidate.attempt.completed_at_utc;
        report.operator_id = candidate.attempt.attestation.operator_id;
        report.binding = candidate.attempt.attestation.binding;
        const auto* reference_requirement = find_requirement(
            candidate.attempt.attestation, report.authored_reference_sha256);
        const auto* routed_requirement = find_requirement(
            candidate.attempt.attestation, report.routed_frame_sha256);
        report.authored_reference_requirement = make_requirement_binding(
            reference_requirement,
            find_observation(
                candidate.attempt.attestation, reference_requirement));
        report.routed_requirement = make_requirement_binding(
            routed_requirement,
            find_observation(candidate.attempt.attestation, routed_requirement));
        report.routed_frame_was_raw_tested = report.routed_requirement.matched;
        report.routed_frame_prior_observation_passed =
            report.routed_requirement.matched && report.routed_requirement.passed &&
            report.routed_requirement.no_spill_observed &&
            report.routed_requirement.blackout_before &&
            report.routed_requirement.blackout_after;
    }

    if (report.snapshot_stale) {
        report.status = RunnerRawHardwareParityStatus::StaleSnapshot;
    } else if (!report.attempt_bound) {
        if (report.completed_attempts_rejected_for_stale_patch > 0U) {
            report.status = RunnerRawHardwareParityStatus::NoBindableAttempt;
        } else {
            report.status = report.invalid_audit_records > 0U
                ? RunnerRawHardwareParityStatus::InvalidAuditEvidence
                : RunnerRawHardwareParityStatus::NoCompletedAttempt;
        }
    } else if (!report.micro_route_covers_universe) {
        report.status = RunnerRawHardwareParityStatus::RouteNotConfigured;
    } else if (!report.current_host_route_accepted) {
        report.status = RunnerRawHardwareParityStatus::RouteNotAccepted;
    } else if (!report.routed_frame_prior_observation_passed) {
        report.status = RunnerRawHardwareParityStatus::RoutedFrameNotRawTested;
    } else {
        report.status = RunnerRawHardwareParityStatus::PriorObservationBound;
    }
    return report;
}

std::string format_runner_raw_hardware_parity_report(
    const RunnerRawHardwareParityReport& report) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << "Raw Hardware Test -> Runner parity\r\n"
           << "Status: " << runner_raw_hardware_parity_status_name(report.status)
           << "\r\nUniverse: U" << static_cast<unsigned int>(report.universe)
           << "  snapshot generation/sequence/age-ms: "
           << report.snapshot_generation << "/"
           << static_cast<unsigned int>(report.snapshot_sequence) << "/"
           << report.snapshot_age_ms << "\r\n"
           << "Routed SHA-256: "
           << (report.routed_frame_sha256.empty()
                   ? "unavailable" : report.routed_frame_sha256)
           << "\r\nAuthored-reference SHA-256: "
           << (report.authored_reference_sha256.empty()
                   ? "unavailable" : report.authored_reference_sha256)
           << "\r\nRouted frame matches authored reference: "
           << yes_no(report.routed_matches_authored_reference) << "\r\n"
           << "SoundSwitch Micro route covers U"
           << static_cast<unsigned int>(report.universe) << ": "
           << yes_no(report.micro_route_covers_universe)
           << "  host accepted: " << yes_no(report.current_host_route_accepted)
           << "  frame sends: "
           << static_cast<unsigned int>(report.micro_route.accepted_frames)
           << "/"
           << static_cast<unsigned int>(report.micro_route.attempted_frames)
           << "  last error: " << report.micro_route.last_error << "\r\n"
           << "Embedded attempt records/invalid/completed considered: "
           << report.audit_records_seen << "/" << report.invalid_audit_records
           << "/" << report.completed_attempts_considered
           << "  stale-patch rejected: "
           << report.completed_attempts_rejected_for_stale_patch << "\r\n";
    if (!report.attempt_bound) {
        output << "Attempt binding: none. Open a project graduated by the bounded "
                  "Raw Hardware Test. Historical evidence is bindable only while "
                  "fixture ID, profile ID, manufacturer/model/mode, universe, and "
                  "address remain stable.\r\n";
    } else {
        output << "Attempt SHA-256: " << report.attempt_sha256
               << "\r\nAttestation SHA-256: " << report.attestation_sha256
               << "\r\nPrior observation: " << report.completed_at_utc
               << " by " << report.operator_id
               << "\r\nFixture/unit/profile/patch: " << report.binding.fixture_id
               << " / " << report.binding.unit_label << " / "
               << report.binding.profile_id << "@"
               << report.binding.profile_revision << " / U"
               << static_cast<unsigned int>(report.binding.universe) << " CH"
               << report.binding.address << "\r\n"
               << "Attempt matches current project basis: "
               << yes_no(report.attempt_matches_current_project_basis)
               << "  historical fixture patch still stable: "
               << yes_no(report.historical_fixture_patch_stable) << "\r\n";
        format_requirement(
            output, "Authored-reference raw requirement",
            report.authored_reference_requirement);
        format_requirement(
            output, "Current routed-frame raw requirement",
            report.routed_requirement);
    }
    output << "Evidence boundary: the host route result and prior raw observation "
              "are independently recorded. Current physical fixture response "
              "observed by EmberLights: no. Re-run the bounded raw test after "
              "profile, patch, safety, output, or hardware changes.\r\n";
    return output.str();
}

std::string_view runner_raw_hardware_parity_status_name(
    RunnerRawHardwareParityStatus status) noexcept {
    switch (status) {
    case RunnerRawHardwareParityStatus::InvalidUniverse:
        return "invalid-universe";
    case RunnerRawHardwareParityStatus::NoSnapshot:
        return "no-runner-snapshot";
    case RunnerRawHardwareParityStatus::StaleSnapshot:
        return "stale-runner-snapshot";
    case RunnerRawHardwareParityStatus::NoCompletedAttempt:
        return "no-completed-raw-attempt";
    case RunnerRawHardwareParityStatus::NoBindableAttempt:
        return "no-safely-bindable-raw-attempt";
    case RunnerRawHardwareParityStatus::InvalidAuditEvidence:
        return "invalid-raw-audit-evidence";
    case RunnerRawHardwareParityStatus::RouteNotConfigured:
        return "micro-route-not-configured";
    case RunnerRawHardwareParityStatus::RouteNotAccepted:
        return "micro-route-not-accepted";
    case RunnerRawHardwareParityStatus::RoutedFrameNotRawTested:
        return "routed-frame-not-previously-raw-tested";
    case RunnerRawHardwareParityStatus::PriorObservationBound:
        return "prior-raw-observation-bound";
    }
    return "invalid";
}

}  // namespace emberlights
