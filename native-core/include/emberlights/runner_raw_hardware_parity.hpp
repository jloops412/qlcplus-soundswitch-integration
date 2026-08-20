#pragma once

#include "emberlights/project.hpp"
#include "emberlights/raw_hardware_test.hpp"
#include "emberlights/runner_frame_inspector.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace emberlights {

// This report joins two already-existing evidence sources. It never emits DMX
// and it never treats a prior operator observation as a current sensor reading.
enum class RunnerRawHardwareParityStatus : std::uint8_t {
    InvalidUniverse,
    NoSnapshot,
    StaleSnapshot,
    NoCompletedAttempt,
    NoBindableAttempt,
    InvalidAuditEvidence,
    RouteNotConfigured,
    RouteNotAccepted,
    RoutedFrameNotRawTested,
    PriorObservationBound
};

struct RunnerRawRequirementBinding {
    bool matched{false};
    std::string requirement_id;
    FixtureQualificationRequirementKind kind{
        FixtureQualificationRequirementKind::Blackout};
    std::uint16_t absolute_channel{0U};
    std::uint8_t value{0U};
    std::string raw_frame_sha256;
    std::string expected_behavior;
    std::string observed_behavior;
    bool passed{false};
    bool no_spill_observed{false};
    bool blackout_before{false};
    bool blackout_after{false};
};

struct RunnerRawHardwareParityReport {
    RunnerRawHardwareParityStatus status{
        RunnerRawHardwareParityStatus::NoSnapshot};
    std::uint8_t universe{0U};

    bool snapshot_available{false};
    bool snapshot_stale{false};
    std::uint64_t snapshot_generation{0U};
    std::uint8_t snapshot_sequence{0U};
    std::uint64_t snapshot_age_ms{0U};
    std::string routed_frame_sha256;
    std::string authored_reference_sha256;
    bool routed_matches_authored_reference{false};

    RunnerOutputRouteResult micro_route{};
    bool micro_route_covers_universe{false};
    bool current_host_route_accepted{false};

    std::size_t audit_records_seen{0U};
    std::size_t invalid_audit_records{0U};
    std::size_t completed_attempts_considered{0U};
    std::size_t completed_attempts_rejected_for_stale_patch{0U};
    bool attempt_bound{false};
    bool attempt_matches_current_project_basis{false};
    bool historical_fixture_patch_stable{false};
    std::string attempt_sha256;
    std::string attestation_sha256;
    std::string completed_at_utc;
    std::string operator_id;
    FixtureQualificationBinding binding{};

    RunnerRawRequirementBinding authored_reference_requirement{};
    RunnerRawRequirementBinding routed_requirement{};
    bool routed_frame_was_raw_tested{false};
    bool routed_frame_prior_observation_passed{false};

    // Always false. EmberLights has no fixture-side optical sensor in this
    // workflow. A fresh physical response requires another bounded test.
    bool current_physical_response_observed{false};
};

[[nodiscard]] RunnerRawHardwareParityReport bind_runner_frame_to_raw_hardware_attempt(
    const ProjectDocument& project,
    const RunnerOutputSnapshot* snapshot,
    std::uint8_t universe,
    const showcore::DmxUniverse& authored_reference,
    const RunnerFrameInspectionOptions& options);

[[nodiscard]] std::string format_runner_raw_hardware_parity_report(
    const RunnerRawHardwareParityReport& report);

[[nodiscard]] std::string_view runner_raw_hardware_parity_status_name(
    RunnerRawHardwareParityStatus status) noexcept;

}  // namespace emberlights
