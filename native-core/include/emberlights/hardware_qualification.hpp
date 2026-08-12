#pragma once

#include "emberlights/fixture_profile_ids.hpp"
#include "emberlights/project.hpp"
#include "showcore/soundswitch_micro.hpp"
#include "showcore/types.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace emberlights {

enum class Ir4QualificationError : std::uint8_t {
    None,
    InvalidProject,
    CompilationFailed,
    MissingLook,
    LookCompilationFailed,
    FrameMismatch,
    PacketMismatch
};

enum class MicroPhysicalQualificationResult : std::uint8_t {
    Passed,
    SoftwareFrameMismatch,
    InitialOpenFailed,
    RawWriteFailed,
    RawObservationFailed,
    RepeatOpenFailed,
    RunnerWriteFailed,
    RunnerObservationFailed,
    BlackoutFailed,
    DisconnectNotObserved,
    ReconnectNotDetected,
    ReconnectOpenFailed,
    ReconnectWriteFailed,
    ReconnectObservationFailed,
    ReconnectBlackoutFailed
};

struct MicroPhysicalQualificationEvidence {
    bool software_frame_match{false};
    bool initial_open_succeeded{false};
    bool raw_writes_succeeded{false};
    bool raw_visible_red_and_blackout{false};
    bool repeat_open_succeeded{false};
    bool runner_writes_succeeded{false};
    bool runner_visible_match_and_blackout{false};
    bool bounded_blackouts_succeeded{false};
    bool disconnect_observed{false};
    bool reconnect_detected{false};
    bool reconnect_open_succeeded{false};
    bool reconnect_writes_succeeded{false};
    bool reconnect_visible_red_and_blackout{false};
    bool reconnect_blackout_succeeded{false};
};

struct FrameComparison {
    std::size_t differing_slots{0U};
    std::uint16_t first_differing_channel{0U};
    std::uint8_t expected{0U};
    std::uint8_t actual{0U};

    [[nodiscard]] bool exact() const noexcept { return differing_slots == 0U; }
};

struct PacketComparison {
    std::size_t differing_bytes{0U};
    std::size_t first_differing_offset{0U};
    std::uint8_t expected{0U};
    std::uint8_t actual{0U};

    [[nodiscard]] bool exact() const noexcept { return differing_bytes == 0U; }
};

struct Ir4QualificationFrames {
    Ir4QualificationError error{Ir4QualificationError::None};
    ProjectValidation validation{};
    showcore::DmxUniverse raw_reference{};
    showcore::DmxUniverse runner_rendered{};
    showcore::SoundSwitchMicroPacket raw_packet{};
    showcore::SoundSwitchMicroPacket runner_packet{};
    FrameComparison frame_comparison{};
    PacketComparison packet_comparison{};

    [[nodiscard]] bool exact() const noexcept {
        return error == Ir4QualificationError::None &&
            frame_comparison.exact() && packet_comparison.exact();
    }
};

// Evidence records are Studio-side, append-only graduation artifacts. They do
// not authorize the raw hardware-test path itself and are never consulted from
// the realtime Runner.
inline constexpr std::uint32_t kFixtureQualificationAttestationVersion = 1U;
inline constexpr std::size_t kMaximumFixtureQualificationRequirements = 513U;
inline constexpr std::string_view kFixtureQualificationAttestationRecord =
    "FIXTURE_QUALIFICATION_ATTESTATION";
inline constexpr std::string_view kFixtureQualificationSupersessionRecord =
    "QUALIFICATION_SUPERSESSION";
inline constexpr std::string_view kFixtureQualificationRevocationRecord =
    "QUALIFICATION_REVOCATION";
inline constexpr std::string_view kRawHardwareTestAttemptRecord =
    "RAW_HARDWARE_TEST_ATTEMPT";

enum class FixtureQualificationRequirementKind : std::uint8_t {
    Blackout,
    OneHot
};

enum class FixtureQualificationStatus : std::uint8_t {
    Passed,
    InvalidSchema,
    InvalidDigest,
    InvalidProjectBasis,
    InvalidBinding,
    InvalidRequirements,
    InvalidObservation,
    MarkerMissing,
    Replay,
    EvidenceMissing,
    EvidenceContradictory,
    EvidenceRevoked
};

struct FixtureQualificationBinding {
    std::string fixture_id;
    std::string unit_label;
    std::string manufacturer;
    std::string model;
    std::string mode;
    std::string profile_id;
    std::string profile_revision;
    std::string behavior_fingerprint;
    std::uint8_t universe{0U};
    std::uint16_t address{0U};
    std::string output_backend;
    std::string safety_policy_sha256;
};

struct FixtureQualificationRequirement {
    std::string id;
    FixtureQualificationRequirementKind kind{
        FixtureQualificationRequirementKind::Blackout};
    std::uint16_t absolute_channel{0U};
    std::uint8_t value{0U};
    std::string expected_behavior;
    bool require_no_spill{true};
    std::string raw_frame_sha256;
};

struct FixtureQualificationObservation {
    std::string requirement_id;
    std::string raw_frame_sha256;
    std::string observed_behavior;
    bool passed{false};
    bool no_spill_observed{false};
    bool blackout_before{false};
    bool blackout_after{false};
    bool timed_out{false};
    bool device_lost{false};
    std::string failure;
};

struct FixtureQualificationAttestation {
    std::uint32_t schema_version{kFixtureQualificationAttestationVersion};
    std::string input_project_sha256;
    std::string candidate_project_sha256;
    std::string candidate_binding_sha256;
    std::string operator_id;
    std::string observed_at_utc;
    FixtureQualificationBinding binding;
    std::vector<FixtureQualificationRequirement> requirements;
    std::vector<FixtureQualificationObservation> observations;
    std::vector<std::string> markers_to_supersede;
    std::string content_sha256;
};

struct FixtureQualificationCheck {
    FixtureQualificationStatus status{FixtureQualificationStatus::Passed};
    std::string subject;
    std::string message;

    [[nodiscard]] bool ok() const noexcept {
        return status == FixtureQualificationStatus::Passed;
    }
};

struct FixtureQualificationGateIssue {
    std::string code;
    std::string subject;
    std::string message;
};

struct FixtureQualificationGate {
    bool physical_output_requested{false};
    bool allowed{true};
    std::vector<FixtureQualificationGateIssue> issues;
};

[[nodiscard]] ProjectDocument make_ir4_6ch_qualification_project();

[[nodiscard]] FrameComparison compare_dmx_frames(
    const showcore::DmxUniverse& expected,
    const showcore::DmxUniverse& actual) noexcept;

[[nodiscard]] PacketComparison compare_soundswitch_micro_packets(
    const showcore::SoundSwitchMicroPacket& expected,
    const showcore::SoundSwitchMicroPacket& actual) noexcept;

[[nodiscard]] Ir4QualificationFrames build_ir4_6ch_red_qualification();

[[nodiscard]] std::string fixture_qualification_project_basis_sha256(
    const ProjectDocument& project);
[[nodiscard]] std::string fixture_qualification_safety_policy_sha256(
    const SafetySettings& safety);
[[nodiscard]] std::string fixture_qualification_binding_sha256(
    const FixtureQualificationBinding& binding);
[[nodiscard]] std::string fixture_qualification_expected_frame_sha256(
    const FixtureQualificationRequirement& requirement);

[[nodiscard]] std::vector<std::string> fixture_qualification_enabled_backends(
    const ProjectDocument& project,
    std::string_view fixture_id);

[[nodiscard]] FixtureQualificationCheck make_fixture_qualification_binding(
    const ProjectDocument& project,
    std::string_view fixture_id,
    std::string_view unit_label,
    std::string_view output_backend,
    FixtureQualificationBinding& binding);

// Completes the project/binding/content digests after an operator has supplied
// the bounded observations. Sealing does not write to or graduate the project.
[[nodiscard]] FixtureQualificationCheck seal_fixture_qualification_attestation(
    const ProjectDocument& project,
    FixtureQualificationAttestation& attestation);
[[nodiscard]] FixtureQualificationCheck validate_fixture_qualification_attestation(
    const ProjectDocument& project,
    const FixtureQualificationAttestation& attestation);

[[nodiscard]] std::string serialize_fixture_qualification_attestation_record(
    const FixtureQualificationAttestation& attestation);
[[nodiscard]] FixtureQualificationCheck parse_fixture_qualification_attestation_record(
    std::string_view record,
    FixtureQualificationAttestation& attestation);

// Graduation is transactional and append-only: the original marker remains,
// followed by the immutable evidence and exact marker/digest supersessions.
[[nodiscard]] FixtureQualificationCheck graduate_fixture_qualification(
    ProjectDocument& project,
    const FixtureQualificationAttestation& attestation);
[[nodiscard]] FixtureQualificationCheck revoke_fixture_qualification(
    ProjectDocument& project,
    std::string_view attestation_sha256,
    std::string_view reason);

[[nodiscard]] FixtureQualificationGate evaluate_fixture_qualification_gate(
    const ProjectDocument& project);

[[nodiscard]] MicroPhysicalQualificationResult evaluate_micro_physical_qualification(
    const MicroPhysicalQualificationEvidence& evidence) noexcept;

[[nodiscard]] const char* micro_physical_qualification_result_name(
    MicroPhysicalQualificationResult result) noexcept;

}  // namespace emberlights
