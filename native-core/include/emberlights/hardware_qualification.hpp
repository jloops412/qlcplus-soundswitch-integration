#pragma once

#include "emberlights/fixture_profile_ids.hpp"
#include "emberlights/project.hpp"
#include "showcore/soundswitch_micro.hpp"
#include "showcore/types.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace emberlights {

enum class FixtureBenchQualificationError : std::uint8_t {
    None,
    InvalidProject,
    CompilationFailed,
    MissingLook,
    LookCompilationFailed,
    FrameMismatch,
    PacketMismatch,
    InvalidBenchContract,
    InvalidExpectedUniverse,
    UnexpectedOutput
};

// Compatibility name retained for the original IR-4 red-only API.
using Ir4QualificationError = FixtureBenchQualificationError;

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

struct FrameDifferenceRow {
    std::uint16_t channel{0U};
    std::uint8_t expected{0U};
    std::uint8_t actual{0U};
};

struct FrameComparison {
    std::size_t differing_slots{0U};
    std::uint16_t first_differing_channel{0U};
    std::uint8_t expected{0U};
    std::uint8_t actual{0U};
    // Fixed capacity covers every DMX slot, so a comparison never truncates
    // the actionable channel-by-channel evidence.
    std::array<FrameDifferenceRow, showcore::kUniverseSlots> differing_channels{};

    [[nodiscard]] bool exact() const noexcept { return differing_slots == 0U; }
    [[nodiscard]] std::span<const FrameDifferenceRow> rows() const noexcept {
        return {differing_channels.data(), differing_slots};
    }
};

struct PacketDifferenceRow {
    std::size_t offset{0U};
    std::uint8_t expected{0U};
    std::uint8_t actual{0U};
    bool expected_present{false};
    bool actual_present{false};
};

struct PacketComparison {
    std::size_t differing_bytes{0U};
    std::size_t first_differing_offset{0U};
    std::uint8_t expected{0U};
    std::uint8_t actual{0U};
    std::size_t expected_length{0U};
    std::size_t actual_length{0U};
    bool expected_length_valid{true};
    bool actual_length_valid{true};
    // JLS1 is bounded to the exact maximum packet size. Length mismatches are
    // represented by a row at each missing/extra byte offset.
    std::array<
        PacketDifferenceRow,
        showcore::kSoundSwitchMicroMaximumFrameSize> differing_byte_rows{};

    [[nodiscard]] bool exact() const noexcept {
        return differing_bytes == 0U && expected_length_valid &&
            actual_length_valid && expected_length == actual_length;
    }
    [[nodiscard]] std::span<const PacketDifferenceRow> rows() const noexcept {
        return {differing_byte_rows.data(), differing_bytes};
    }
};

struct FixtureBenchQualificationFrames {
    FixtureBenchQualificationError error{FixtureBenchQualificationError::None};
    std::string look_id;
    std::uint8_t expected_universe{1U};
    ProjectValidation validation{};
    showcore::DmxUniverse raw_reference{};
    showcore::DmxUniverse runner_rendered{};
    showcore::DmxFrames runner_frames{};
    showcore::SoundSwitchMicroPacket raw_packet{};
    showcore::SoundSwitchMicroPacket runner_packet{};
    FrameComparison frame_comparison{};
    PacketComparison packet_comparison{};
    std::size_t unrelated_output_nonzero_slots{0U};

    [[nodiscard]] bool exact() const noexcept {
        return error == FixtureBenchQualificationError::None &&
            frame_comparison.exact() && packet_comparison.exact() &&
            unrelated_output_nonzero_slots == 0U;
    }
};

// Compatibility type retained for the original IR-4 red-only API.
using Ir4QualificationFrames = FixtureBenchQualificationFrames;

struct OneFixtureBenchContract {
    bool exactly_one_fixture{false};
    bool exactly_one_profile{false};
    bool fixture_profile_present{false};
    bool fixture_on_selected_output{false};
    bool soundswitch_micro_universe_one_only{false};
    bool os2l_disabled{false};
    bool midi_disabled{false};
    bool automation_disabled{false};
    bool isolated_content{false};

    [[nodiscard]] bool exact() const noexcept {
        return exactly_one_fixture && exactly_one_profile &&
            fixture_profile_present && fixture_on_selected_output &&
            soundswitch_micro_universe_one_only && os2l_disabled &&
            midi_disabled && automation_disabled && isolated_content;
    }
};

struct FixtureBenchLookExpectation {
    std::string_view look_id;
    std::uint8_t universe{1U};
    showcore::DmxUniverse expected_frame{};
};

struct FixtureBenchQualificationSet {
    std::vector<FixtureBenchQualificationFrames> looks;

    [[nodiscard]] bool exact() const noexcept {
        return !looks.empty() && std::all_of(
            looks.begin(), looks.end(),
            [](const auto& look) { return look.exact(); });
    }
};

enum class Ir4SixChannelSafeLook : std::uint8_t {
    Blackout,
    Red,
    Green,
    Blue,
    White,
    Amber,
    Count
};

inline constexpr std::size_t kIr4SixChannelSafeLookCount =
    static_cast<std::size_t>(Ir4SixChannelSafeLook::Count);
inline constexpr std::string_view kIr4SixChannelOperatorBenchProfileId =
    "local.both-lighting.bo-ir4.6ch.operator-bench-v1";

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
// Editable diagnostic clone for the packaged Studio workbench. The immutable
// qualification project above remains the source of manual-backed truth.
[[nodiscard]] ProjectDocument make_ir4_6ch_operator_bench_project();

[[nodiscard]] OneFixtureBenchContract inspect_one_fixture_bench_contract(
    const ProjectDocument& project) noexcept;

[[nodiscard]] FrameComparison compare_dmx_frames(
    const showcore::DmxUniverse& expected,
    const showcore::DmxUniverse& actual) noexcept;

[[nodiscard]] PacketComparison compare_soundswitch_micro_packets(
    const showcore::SoundSwitchMicroPacket& expected,
    const showcore::SoundSwitchMicroPacket& actual) noexcept;

[[nodiscard]] FixtureBenchQualificationSet build_fixture_bench_qualifications(
    const ProjectDocument& project,
    std::span<const FixtureBenchLookExpectation> expectations);

[[nodiscard]] std::string_view ir4_6ch_safe_look_id(
    Ir4SixChannelSafeLook look) noexcept;
[[nodiscard]] showcore::DmxUniverse ir4_6ch_safe_look_expected_frame(
    Ir4SixChannelSafeLook look) noexcept;
[[nodiscard]] FixtureBenchQualificationSet
build_ir4_6ch_safe_qualifications();

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
