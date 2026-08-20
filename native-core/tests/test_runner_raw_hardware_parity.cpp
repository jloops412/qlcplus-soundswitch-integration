#include "emberlights/hardware_qualification.hpp"
#include "emberlights/raw_hardware_test.hpp"
#include "emberlights/runner_raw_hardware_parity.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>

namespace {

int failures = 0;

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ \
                  << " - " #condition "\n"; \
        ++failures; \
    } \
} while (false)

constexpr std::string_view kFixtureId = "ir4-bench-001";
const std::string kMarker =
    "MIGRATED_PATCH_UNVERIFIED\tfixture-mode-address-universe-review-required\t"
    "ir4-bench-001";

[[nodiscard]] emberlights::ProjectDocument make_candidate() {
    auto project = emberlights::make_ir4_6ch_qualification_project();
    project.unknown_records.push_back(kMarker);
    return project;
}

[[nodiscard]] std::string observed_behavior(std::uint16_t channel) {
    switch (channel) {
    case 0U: return "Fixture and neighboring units stayed fully dark.";
    case 1U: return "Red emitter responded alone.";
    case 2U: return "Green emitter responded alone.";
    case 3U: return "Blue emitter responded alone.";
    case 4U: return "Amber emitter responded alone on physical CH4.";
    case 5U: return "White emitter responded alone on physical CH5.";
    case 6U: return "UV emitter responded alone.";
    default: return "Unexpected channel.";
    }
}

[[nodiscard]] emberlights::RawHardwareTestAttempt make_attempt(
    const emberlights::ProjectDocument& project) {
    emberlights::FixtureQualificationAttestation attestation;
    CHECK(emberlights::make_fixture_qualification_binding(
              project,
              kFixtureId,
              "IR-4 physical unit A",
              "soundswitch-micro:u1",
              attestation.binding).ok());
    attestation.operator_id = "bench-operator-001";
    attestation.observed_at_utc = "2026-08-13T14:05:00Z";
    attestation.markers_to_supersede = {kMarker};

    emberlights::FixtureQualificationRequirement blackout;
    blackout.id = "blackout";
    blackout.kind =
        emberlights::FixtureQualificationRequirementKind::Blackout;
    blackout.expected_behavior = "All emitters remain dark.";
    blackout.raw_frame_sha256 =
        emberlights::fixture_qualification_expected_frame_sha256(blackout);
    attestation.requirements.push_back(blackout);

    for (std::uint16_t channel = 1U; channel <= 6U; ++channel) {
        emberlights::FixtureQualificationRequirement requirement;
        requirement.id = "raw-ch" + std::to_string(channel);
        requirement.kind =
            emberlights::FixtureQualificationRequirementKind::OneHot;
        requirement.absolute_channel = channel;
        requirement.value = 255U;
        requirement.expected_behavior = "Document isolated physical CH" +
            std::to_string(channel) + " response.";
        requirement.raw_frame_sha256 =
            emberlights::fixture_qualification_expected_frame_sha256(requirement);
        attestation.requirements.push_back(std::move(requirement));
    }

    for (const auto& requirement : attestation.requirements) {
        attestation.observations.push_back({
            requirement.id,
            requirement.raw_frame_sha256,
            observed_behavior(requirement.absolute_channel),
            true,
            true,
            true,
            true,
            false,
            false,
            {}});
    }
    CHECK(emberlights::seal_fixture_qualification_attestation(
              project, attestation).ok());

    emberlights::RawHardwareTestAttempt attempt;
    attempt.attestation = std::move(attestation);
    attempt.started_at_utc = "2026-08-13T14:00:00Z";
    attempt.completed_at_utc = "2026-08-13T14:05:00Z";
    attempt.terminal_phase = emberlights::RawHardwareTestPhase::Complete;
    attempt.terminal_error = emberlights::RawHardwareTestError::None;
    attempt.frames_attempted = 42U;
    attempt.frames_accepted = 42U;
    CHECK(emberlights::seal_raw_hardware_test_attempt(attempt).ok());
    CHECK(emberlights::validate_raw_hardware_test_attempt(project, attempt).ok());
    return attempt;
}

[[nodiscard]] emberlights::RunnerOutputSnapshot make_snapshot(
    const showcore::DmxUniverse& universe,
    bool accepted = true) {
    emberlights::RunnerOutputSnapshot snapshot;
    snapshot.generation = 7U;
    snapshot.sequence = 11U;
    snapshot.rendered_at_ms = 900U;
    snapshot.pre_blackout_frames.universes[0] = universe;
    snapshot.routed_frames.universes[0] = universe;
    auto& route = snapshot.routes[
        static_cast<std::size_t>(showcore::OutputBackendKind::SoundSwitchMicro)];
    route.kind = showcore::OutputBackendKind::SoundSwitchMicro;
    route.first_source_universe = 1U;
    route.source_universe_count = 1U;
    route.configured = true;
    route.attempted_frames = 1U;
    route.accepted_frames = accepted ? 1U : 0U;
    route.last_error = accepted ? 0U : 31U;
    return snapshot;
}

[[nodiscard]] emberlights::RunnerFrameInspectionOptions options(
    std::uint64_t now = 1000U) {
    emberlights::RunnerFrameInspectionOptions result;
    result.inspected_at_ms = now;
    result.stale_after_ms = 2000U;
    return result;
}

}  // namespace

int main() {
    auto candidate = make_candidate();
    const auto attempt = make_attempt(candidate);
    auto graduated = candidate;
    CHECK(emberlights::graduate_raw_hardware_test_attempt(
              graduated, attempt).ok());

    const auto white_reference =
        emberlights::ir4_6ch_safe_look_expected_frame(
            emberlights::Ir4SixChannelSafeLook::White);
    const auto amber_reference =
        emberlights::ir4_6ch_safe_look_expected_frame(
            emberlights::Ir4SixChannelSafeLook::Amber);

    {
        const auto report =
            emberlights::bind_runner_frame_to_raw_hardware_attempt(
                graduated, nullptr, 1U, white_reference, options());
        CHECK(report.status ==
              emberlights::RunnerRawHardwareParityStatus::NoSnapshot);
        CHECK(!report.current_physical_response_observed);
    }
    {
        const auto snapshot = make_snapshot(white_reference);
        auto no_audit = candidate;
        const auto report =
            emberlights::bind_runner_frame_to_raw_hardware_attempt(
                no_audit, &snapshot, 1U, white_reference, options());
        CHECK(report.status ==
              emberlights::RunnerRawHardwareParityStatus::NoCompletedAttempt);
        CHECK(report.current_host_route_accepted);
        CHECK(!report.attempt_bound);
    }
    {
        const auto snapshot = make_snapshot(white_reference);
        auto malformed = candidate;
        malformed.unknown_records.push_back(
            "RAW_HARDWARE_TEST_ATTEMPT\t1\tnot-a-digest\t00");
        const auto report =
            emberlights::bind_runner_frame_to_raw_hardware_attempt(
                malformed, &snapshot, 1U, white_reference, options());
        CHECK(report.status ==
              emberlights::RunnerRawHardwareParityStatus::InvalidAuditEvidence);
        CHECK(report.invalid_audit_records == 1U);
    }
    {
        const auto snapshot = make_snapshot(white_reference);
        const auto report =
            emberlights::bind_runner_frame_to_raw_hardware_attempt(
                graduated, &snapshot, 1U, white_reference, options());
        CHECK(report.status ==
              emberlights::RunnerRawHardwareParityStatus::PriorObservationBound);
        CHECK(report.attempt_bound);
        CHECK(report.attempt_matches_current_project_basis);
        CHECK(report.historical_fixture_patch_stable);
        CHECK(report.current_host_route_accepted);
        CHECK(report.routed_matches_authored_reference);
        CHECK(report.routed_requirement.matched);
        CHECK(report.routed_requirement.absolute_channel == 4U);
        CHECK(report.routed_requirement.observed_behavior.find("Amber") !=
              std::string::npos);
        CHECK(report.routed_frame_prior_observation_passed);
        CHECK(!report.current_physical_response_observed);
        const auto text =
            emberlights::format_runner_raw_hardware_parity_report(report);
        CHECK(text.find("prior-raw-observation-bound") != std::string::npos);
        CHECK(text.find("physical CH4") != std::string::npos);
        CHECK(text.find("Current physical fixture response observed by "
                        "EmberLights: no") != std::string::npos);
    }
    {
        // A profile revision changes the project basis, but an unchanged
        // fixture/profile ID and U1/CH1 patch keeps the old raw observation
        // useful as explicitly historical bit-level evidence.
        auto edited = graduated;
        edited.fixture_profiles.front().source_revision += ".wa-exchange";
        const auto snapshot = make_snapshot(amber_reference);
        const auto report =
            emberlights::bind_runner_frame_to_raw_hardware_attempt(
                edited, &snapshot, 1U, white_reference, options());
        CHECK(report.status ==
              emberlights::RunnerRawHardwareParityStatus::PriorObservationBound);
        CHECK(!report.attempt_matches_current_project_basis);
        CHECK(report.historical_fixture_patch_stable);
        CHECK(!report.routed_matches_authored_reference);
        CHECK(report.authored_reference_requirement.absolute_channel == 4U);
        CHECK(report.routed_requirement.absolute_channel == 5U);
        CHECK(report.routed_requirement.observed_behavior.find("White") !=
              std::string::npos);
    }
    {
        auto repatched = graduated;
        repatched.fixtures.front().address = 20U;
        const auto snapshot = make_snapshot(white_reference);
        const auto report =
            emberlights::bind_runner_frame_to_raw_hardware_attempt(
                repatched, &snapshot, 1U, white_reference, options());
        CHECK(!report.attempt_bound);
        CHECK(report.completed_attempts_rejected_for_stale_patch == 1U);
        CHECK(report.status ==
              emberlights::RunnerRawHardwareParityStatus::NoBindableAttempt);
    }
    {
        const auto snapshot = make_snapshot(white_reference, false);
        const auto report =
            emberlights::bind_runner_frame_to_raw_hardware_attempt(
                graduated, &snapshot, 1U, white_reference, options());
        CHECK(report.status ==
              emberlights::RunnerRawHardwareParityStatus::RouteNotAccepted);
        CHECK(!report.current_host_route_accepted);
        CHECK(report.routed_frame_prior_observation_passed);
    }
    {
        showcore::DmxUniverse untested{};
        untested[3U] = 127U;
        const auto snapshot = make_snapshot(untested);
        const auto report =
            emberlights::bind_runner_frame_to_raw_hardware_attempt(
                graduated, &snapshot, 1U, white_reference, options());
        CHECK(report.status ==
              emberlights::RunnerRawHardwareParityStatus::RoutedFrameNotRawTested);
        CHECK(!report.routed_frame_was_raw_tested);
    }
    {
        const auto snapshot = make_snapshot(white_reference);
        const auto report =
            emberlights::bind_runner_frame_to_raw_hardware_attempt(
                graduated, &snapshot, 1U, white_reference, options(4000U));
        CHECK(report.status ==
              emberlights::RunnerRawHardwareParityStatus::StaleSnapshot);
        CHECK(report.snapshot_stale);
    }
    {
        const auto snapshot = make_snapshot(white_reference);
        const auto report =
            emberlights::bind_runner_frame_to_raw_hardware_attempt(
                graduated, &snapshot, 3U, white_reference, options());
        CHECK(report.status ==
              emberlights::RunnerRawHardwareParityStatus::InvalidUniverse);
    }

    if (failures == 0) {
        std::cout << "Runner/raw Hardware Test parity tests passed\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    return 1;
}
