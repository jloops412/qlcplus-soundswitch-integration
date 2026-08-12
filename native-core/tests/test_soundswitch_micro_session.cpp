#include "emberlights/hardware_qualification.hpp"
#include "emberlights/project.hpp"
#include "emberlights/project_io.hpp"
#include "showcore/soundswitch_micro.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

int failures = 0;

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ \
                  << " - " #condition "\n"; \
        ++failures; \
    } \
} while (false)

constexpr std::string_view kMigrationMarker =
    "MIGRATED_PATCH_UNVERIFIED\tfixture-mode-address-universe-review-required";

[[nodiscard]] bool has_issue(
    const emberlights::ProjectValidation& validation,
    std::string_view code) {
    return std::any_of(
        validation.issues.begin(), validation.issues.end(),
        [&](const auto& issue) { return issue.code == code; });
}

[[nodiscard]] emberlights::FixtureQualificationAttestation make_complete_attestation(
    const emberlights::ProjectDocument& project,
    std::string_view fixture_id,
    std::string_view unit_label,
    std::string_view timestamp = "2026-08-12T16:00:00Z") {
    emberlights::FixtureQualificationAttestation attestation{};
    const auto binding = emberlights::make_fixture_qualification_binding(
        project,
        fixture_id,
        unit_label,
        "soundswitch-micro:u1",
        attestation.binding);
    CHECK(binding.ok());
    attestation.operator_id = "bench-operator-001";
    attestation.observed_at_utc = timestamp;
    attestation.markers_to_supersede = {std::string(kMigrationMarker)};

    const auto fixture = std::find_if(
        project.fixtures.begin(), project.fixtures.end(),
        [&](const auto& candidate) { return candidate.id == fixture_id; });
    CHECK(fixture != project.fixtures.end());
    if (fixture == project.fixtures.end()) {
        return attestation;
    }
    const auto profile = std::find_if(
        project.fixture_profiles.begin(), project.fixture_profiles.end(),
        [&](const auto& candidate) { return candidate.id == fixture->profile_id; });
    CHECK(profile != project.fixture_profiles.end());
    if (profile == project.fixture_profiles.end()) {
        return attestation;
    }

    emberlights::FixtureQualificationRequirement blackout{};
    blackout.id = "blackout";
    blackout.kind = emberlights::FixtureQualificationRequirementKind::Blackout;
    blackout.expected_behavior = "all emitters dark";
    blackout.require_no_spill = true;
    blackout.raw_frame_sha256 =
        emberlights::fixture_qualification_expected_frame_sha256(blackout);
    attestation.requirements.push_back(blackout);

    for (std::uint16_t offset = 0U; offset < profile->footprint; ++offset) {
        emberlights::FixtureQualificationRequirement requirement{};
        requirement.id = "slot-" + std::to_string(offset + 1U);
        requirement.kind = emberlights::FixtureQualificationRequirementKind::OneHot;
        requirement.absolute_channel = static_cast<std::uint16_t>(
            fixture->address + offset);
        requirement.value = 255U;
        requirement.expected_behavior = "manual-observed fixture slot " +
            std::to_string(offset + 1U);
        requirement.require_no_spill = true;
        requirement.raw_frame_sha256 =
            emberlights::fixture_qualification_expected_frame_sha256(requirement);
        attestation.requirements.push_back(std::move(requirement));
    }

    attestation.observations.reserve(attestation.requirements.size());
    for (const auto& requirement : attestation.requirements) {
        attestation.observations.push_back({
            requirement.id,
            requirement.raw_frame_sha256,
            requirement.expected_behavior,
            true,
            true,
            true,
            true,
            false,
            false,
            {}});
    }
    const auto sealed =
        emberlights::seal_fixture_qualification_attestation(project, attestation);
    CHECK(sealed.ok());
    return attestation;
}

}  // namespace

int main() {
    const showcore::SoundSwitchMicroSessionConfig defaults;
    CHECK(showcore::valid_soundswitch_micro_session_config(defaults));
    CHECK(defaults.transfer_timeout == std::chrono::milliseconds(500));
    CHECK(defaults.settling_interval == std::chrono::milliseconds(200));
    CHECK(defaults.frame_interval == std::chrono::milliseconds(25));
    CHECK(defaults.warmup_blackout_frames == 50U);
    CHECK(defaults.close_blackout_frames == 3U);

    auto invalid = defaults;
    invalid.transfer_timeout = std::chrono::milliseconds(0);
    CHECK(!showcore::valid_soundswitch_micro_session_config(invalid));
    invalid = defaults;
    invalid.frame_interval = std::chrono::milliseconds(0);
    CHECK(!showcore::valid_soundswitch_micro_session_config(invalid));
    invalid = defaults;
    invalid.warmup_blackout_frames = 401U;
    CHECK(!showcore::valid_soundswitch_micro_session_config(invalid));

    CHECK(showcore::kSoundSwitchMicroOpenSequence.front() ==
          showcore::SoundSwitchMicroLifecycleState::Detecting);
    CHECK(showcore::kSoundSwitchMicroOpenSequence.back() ==
          showcore::SoundSwitchMicroLifecycleState::Streaming);
    CHECK(std::string_view(showcore::soundswitch_micro_lifecycle_name(
              showcore::SoundSwitchMicroLifecycleState::WarmingUp)) == "warming-up");
    CHECK(std::string_view(showcore::soundswitch_micro_lifecycle_name(
              showcore::SoundSwitchMicroLifecycleState::Fault)) == "fault");

    showcore::DmxUniverse universe{};
    universe[0] = 0x11U;
    universe[511] = 0xFEU;
    const auto packet = showcore::build_soundswitch_micro_packet(
        universe, showcore::SoundSwitchMicroFraming::NativeJls1);
    CHECK(packet.length == 522U);
    CHECK(packet.bytes[10] == 0x11U);
    CHECK(packet.bytes[521] == 0xFEU);

    const auto project = emberlights::make_starter_project();
    const auto ir4_profile = std::find_if(
        project.fixture_profiles.begin(),
        project.fixture_profiles.end(),
        [](const auto& profile) {
            return profile.id == emberlights::kBothLightingIr4SixChannelProfileId;
        });
    CHECK(ir4_profile != project.fixture_profiles.end());
    if (ir4_profile != project.fixture_profiles.end()) {
        CHECK(ir4_profile->manufacturer == "Both Lighting");
        CHECK(ir4_profile->model == "BO-IR4 LED Mini Spotlight");
        CHECK(ir4_profile->mode == "6 Channel (manual-matched; CH6 Purple/UV)");
        CHECK(ir4_profile->footprint == 6U);
        CHECK(ir4_profile->channels.size() == 6U);
        constexpr std::array properties{
            showcore::Property::Red,
            showcore::Property::Green,
            showcore::Property::Blue,
            showcore::Property::White,
            showcore::Property::Amber,
            showcore::Property::UV};
        for (std::size_t index = 0U;
             index < properties.size() && index < ir4_profile->channels.size(); ++index) {
            CHECK(ir4_profile->channels[index].property == properties[index]);
            CHECK(ir4_profile->channels[index].coarse_offset == index);
        }
    }

    const auto qualification = emberlights::build_ir4_6ch_red_qualification();
    CHECK(qualification.exact());
    CHECK(qualification.validation.ok());
    CHECK(qualification.raw_reference[0] == 255U);
    CHECK(qualification.runner_rendered[0] == 255U);
    for (std::size_t index = 1U; index < showcore::kUniverseSlots; ++index) {
        CHECK(qualification.raw_reference[index] == 0U);
        CHECK(qualification.runner_rendered[index] == 0U);
    }
    CHECK(qualification.raw_packet.length == 522U);
    CHECK(qualification.runner_packet.length == 522U);
    CHECK(qualification.frame_comparison.differing_slots == 0U);
    CHECK(qualification.packet_comparison.differing_bytes == 0U);

    auto mismatch = qualification.runner_rendered;
    mismatch[4] = 1U;
    const auto comparison = emberlights::compare_dmx_frames(
        qualification.raw_reference, mismatch);
    CHECK(!comparison.exact());
    CHECK(comparison.differing_slots == 1U);
    CHECK(comparison.first_differing_channel == 5U);
    CHECK(comparison.expected == 0U);
    CHECK(comparison.actual == 1U);

    emberlights::MicroPhysicalQualificationEvidence physical{};
    CHECK(emberlights::evaluate_micro_physical_qualification(physical) ==
          emberlights::MicroPhysicalQualificationResult::SoftwareFrameMismatch);
    physical.software_frame_match = true;
    CHECK(emberlights::evaluate_micro_physical_qualification(physical) ==
          emberlights::MicroPhysicalQualificationResult::InitialOpenFailed);
    physical.initial_open_succeeded = true;
    physical.raw_writes_succeeded = true;
    physical.raw_visible_red_and_blackout = true;
    physical.repeat_open_succeeded = true;
    physical.runner_writes_succeeded = true;
    physical.runner_visible_match_and_blackout = true;
    physical.bounded_blackouts_succeeded = true;
    CHECK(emberlights::evaluate_micro_physical_qualification(physical) ==
          emberlights::MicroPhysicalQualificationResult::DisconnectNotObserved);
    physical.disconnect_observed = true;
    physical.reconnect_detected = true;
    physical.reconnect_open_succeeded = true;
    physical.reconnect_writes_succeeded = true;
    physical.reconnect_visible_red_and_blackout = true;
    CHECK(emberlights::evaluate_micro_physical_qualification(physical) ==
          emberlights::MicroPhysicalQualificationResult::ReconnectBlackoutFailed);
    physical.reconnect_blackout_succeeded = true;
    CHECK(emberlights::evaluate_micro_physical_qualification(physical) ==
          emberlights::MicroPhysicalQualificationResult::Passed);
    CHECK(std::string_view(emberlights::micro_physical_qualification_result_name(
              emberlights::MicroPhysicalQualificationResult::Passed)) == "passed");

    auto pending = emberlights::make_ir4_6ch_qualification_project();
    pending.unknown_records.push_back(std::string(kMigrationMarker));
    const auto pending_gate = emberlights::evaluate_fixture_qualification_gate(pending);
    CHECK(pending_gate.physical_output_requested);
    CHECK(!pending_gate.allowed);
    const auto pending_validation = emberlights::validate_project(pending);
    CHECK(!pending_validation.ok());
    CHECK(has_issue(pending_validation, "qualification.evidenceRequired"));

    auto output_disabled = pending;
    output_disabled.connections.soundswitch_micro_universe = 0U;
    const auto disabled_gate =
        emberlights::evaluate_fixture_qualification_gate(output_disabled);
    CHECK(!disabled_gate.physical_output_requested);
    CHECK(disabled_gate.allowed);
    const auto disabled_validation = emberlights::validate_project(output_disabled);
    CHECK(disabled_validation.ok());
    CHECK(has_issue(disabled_validation, "qualification.evidenceRequired"));

    const auto basis_before =
        emberlights::fixture_qualification_project_basis_sha256(pending);
    auto attestation = make_complete_attestation(
        pending, "ir4-bench-001", "IR-4 physical unit A");
    CHECK(attestation.requirements.size() == 7U);
    CHECK(attestation.observations.size() == 7U);
    CHECK(attestation.input_project_sha256 == basis_before);
    CHECK(attestation.candidate_project_sha256 == basis_before);
    CHECK(attestation.candidate_binding_sha256 ==
          emberlights::fixture_qualification_binding_sha256(attestation.binding));

    const auto embedded =
        emberlights::serialize_fixture_qualification_attestation_record(attestation);
    emberlights::FixtureQualificationAttestation parsed_attestation;
    CHECK(emberlights::parse_fixture_qualification_attestation_record(
              embedded, parsed_attestation).ok());
    CHECK(parsed_attestation.content_sha256 == attestation.content_sha256);
    CHECK(parsed_attestation.binding.fixture_id == "ir4-bench-001");
    CHECK(parsed_attestation.requirements.size() == attestation.requirements.size());

    auto graduated = pending;
    CHECK(emberlights::graduate_fixture_qualification(graduated, attestation).ok());
    CHECK(std::find(
              graduated.unknown_records.begin(),
              graduated.unknown_records.end(),
              kMigrationMarker) != graduated.unknown_records.end());
    CHECK(emberlights::fixture_qualification_project_basis_sha256(graduated) == basis_before);
    CHECK(emberlights::evaluate_fixture_qualification_gate(graduated).allowed);
    CHECK(emberlights::validate_project(graduated).ok());
    emberlights::ProjectDocument persisted_graduation;
    CHECK(emberlights::parse_project(
              emberlights::serialize_project(graduated), persisted_graduation));
    CHECK(emberlights::evaluate_fixture_qualification_gate(
              persisted_graduation).allowed);
    CHECK(emberlights::graduate_fixture_qualification(graduated, attestation).status ==
          emberlights::FixtureQualificationStatus::Replay);

    auto partial = attestation;
    partial.observations.pop_back();
    CHECK(emberlights::seal_fixture_qualification_attestation(pending, partial).status ==
          emberlights::FixtureQualificationStatus::InvalidObservation);

    auto timeout = attestation;
    timeout.observations[1].timed_out = true;
    CHECK(emberlights::seal_fixture_qualification_attestation(pending, timeout).status ==
          emberlights::FixtureQualificationStatus::InvalidObservation);
    auto device_lost = attestation;
    device_lost.observations[1].device_lost = true;
    CHECK(emberlights::seal_fixture_qualification_attestation(pending, device_lost).status ==
          emberlights::FixtureQualificationStatus::InvalidObservation);
    auto missing_blackout = attestation;
    missing_blackout.observations[1].blackout_after = false;
    CHECK(emberlights::seal_fixture_qualification_attestation(pending, missing_blackout).status ==
          emberlights::FixtureQualificationStatus::InvalidObservation);
    auto spilled = attestation;
    spilled.observations[1].no_spill_observed = false;
    CHECK(emberlights::seal_fixture_qualification_attestation(pending, spilled).status ==
          emberlights::FixtureQualificationStatus::InvalidObservation);

    auto incomplete_coverage = attestation;
    incomplete_coverage.requirements[1].absolute_channel =
        incomplete_coverage.requirements[2].absolute_channel;
    incomplete_coverage.requirements[1].raw_frame_sha256 =
        emberlights::fixture_qualification_expected_frame_sha256(
            incomplete_coverage.requirements[1]);
    incomplete_coverage.observations[1].raw_frame_sha256 =
        incomplete_coverage.requirements[1].raw_frame_sha256;
    CHECK(emberlights::seal_fixture_qualification_attestation(
              pending, incomplete_coverage).status ==
          emberlights::FixtureQualificationStatus::InvalidRequirements);

    auto cross_project = pending;
    cross_project.name += " changed";
    CHECK(emberlights::validate_fixture_qualification_attestation(
              cross_project, attestation).status ==
          emberlights::FixtureQualificationStatus::InvalidProjectBasis);
    auto missing_marker_project = pending;
    missing_marker_project.unknown_records.clear();
    CHECK(emberlights::validate_fixture_qualification_attestation(
              missing_marker_project, attestation).status ==
          emberlights::FixtureQualificationStatus::InvalidProjectBasis);

    auto tampered_record = embedded;
    tampered_record.back() = tampered_record.back() == '0' ? '1' : '0';
    emberlights::FixtureQualificationAttestation rejected_tamper;
    CHECK(emberlights::parse_fixture_qualification_attestation_record(
              tampered_record, rejected_tamper).status ==
          emberlights::FixtureQualificationStatus::InvalidDigest);
    auto tampered_project = graduated;
    const auto embedded_position = std::find(
        tampered_project.unknown_records.begin(),
        tampered_project.unknown_records.end(),
        embedded);
    CHECK(embedded_position != tampered_project.unknown_records.end());
    if (embedded_position != tampered_project.unknown_records.end()) {
        *embedded_position = tampered_record;
    }
    CHECK(!emberlights::evaluate_fixture_qualification_gate(tampered_project).allowed);

    auto revoked = graduated;
    CHECK(emberlights::revoke_fixture_qualification(
              revoked, attestation.content_sha256, "profile or unit must be rechecked").ok());
    CHECK(!emberlights::evaluate_fixture_qualification_gate(revoked).allowed);
    CHECK(emberlights::revoke_fixture_qualification(
              revoked, attestation.content_sha256, "duplicate").status ==
          emberlights::FixtureQualificationStatus::Replay);
    auto requalified_attestation = make_complete_attestation(
        revoked,
        "ir4-bench-001",
        "IR-4 physical unit A",
        "2026-08-12T17:00:00Z");
    CHECK(emberlights::graduate_fixture_qualification(
              revoked, requalified_attestation).ok());
    CHECK(emberlights::evaluate_fixture_qualification_gate(revoked).allowed);

    auto contradictory = graduated;
    auto second_attestation = make_complete_attestation(
        contradictory,
        "ir4-bench-001",
        "IR-4 physical unit A",
        "2026-08-12T18:00:00Z");
    CHECK(emberlights::graduate_fixture_qualification(
              contradictory, second_attestation).ok());
    CHECK(!emberlights::evaluate_fixture_qualification_gate(contradictory).allowed);
    CHECK(emberlights::revoke_fixture_qualification(
              contradictory,
              attestation.content_sha256,
              "superseded by later bounded test").ok());
    CHECK(emberlights::evaluate_fixture_qualification_gate(contradictory).allowed);

    auto two_units = pending;
    two_units.fixtures.push_back({
        "ir4-bench-002",
        "Both Lighting IR-4 Bench Fixture B",
        std::string(emberlights::kBothLightingIr4SixChannelProfileId),
        1U,
        20U,
        {"qualification", "uplight", "color"}});
    auto first_unit = make_complete_attestation(
        two_units, "ir4-bench-001", "IR-4 physical unit A");
    CHECK(emberlights::graduate_fixture_qualification(two_units, first_unit).ok());
    CHECK(!emberlights::evaluate_fixture_qualification_gate(two_units).allowed);
    auto second_unit = make_complete_attestation(
        two_units,
        "ir4-bench-002",
        "IR-4 physical unit B",
        "2026-08-12T19:00:00Z");
    CHECK(emberlights::graduate_fixture_qualification(two_units, second_unit).ok());
    CHECK(emberlights::evaluate_fixture_qualification_gate(two_units).allowed);

    if (failures == 0) {
        std::cout << "SoundSwitch Micro session tests passed\n";
        return 0;
    }
    return 1;
}
