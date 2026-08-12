#include "emberlights/raw_hardware_test.hpp"

#include "emberlights/file_identity.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace emberlights {
namespace {

constexpr std::chrono::milliseconds kMinimumObservationTimeout{100};
constexpr std::chrono::milliseconds kMaximumObservationTimeout{600000};
constexpr std::chrono::milliseconds kMinimumSessionTimeout{1000};
constexpr std::chrono::milliseconds kMaximumSessionTimeout{86400000};
constexpr std::uint16_t kMaximumBlackoutRepetitions = 20U;
constexpr std::size_t kMaximumAuditPayloadBytes = 4U * 1024U * 1024U;
constexpr std::size_t kMaximumOperatorBytes = 255U;
constexpr std::size_t kMaximumCriterionBytes = 1024U;
constexpr std::size_t kMaximumObservationBytes = 2048U;
constexpr std::size_t kMaximumTerminalMessageBytes = 2048U;

class CanonicalWriter {
public:
    void field(std::string_view value) {
        value_.append(std::to_string(value.size()));
        value_.push_back(':');
        value_.append(value);
    }

    template <typename Integer>
    void number(Integer value) {
        field(std::to_string(static_cast<unsigned long long>(value)));
    }

    [[nodiscard]] const std::string& value() const noexcept { return value_; }

private:
    std::string value_;
};

class CanonicalReader {
public:
    explicit CanonicalReader(std::string_view value) : value_(value) {}

    [[nodiscard]] bool field(std::string& output) {
        const auto separator = value_.find(':', cursor_);
        if (separator == std::string_view::npos || separator == cursor_) {
            return false;
        }
        std::size_t length = 0U;
        const auto text = value_.substr(cursor_, separator - cursor_);
        const auto converted = std::from_chars(
            text.data(), text.data() + text.size(), length);
        if (converted.ec != std::errc{} ||
            converted.ptr != text.data() + text.size() ||
            length > value_.size() - separator - 1U) {
            return false;
        }
        const auto start = separator + 1U;
        output.assign(value_.substr(start, length));
        cursor_ = start + length;
        return true;
    }

    template <typename Integer>
    [[nodiscard]] bool number(Integer& output) {
        std::string text;
        if (!field(text) || text.empty()) {
            return false;
        }
        unsigned long long value = 0U;
        const auto converted = std::from_chars(
            text.data(), text.data() + text.size(), value);
        if (converted.ec != std::errc{} ||
            converted.ptr != text.data() + text.size() ||
            value > static_cast<unsigned long long>(
                        std::numeric_limits<Integer>::max())) {
            return false;
        }
        output = static_cast<Integer>(value);
        return true;
    }

    [[nodiscard]] bool finished() const noexcept {
        return cursor_ == value_.size();
    }

private:
    std::string_view value_;
    std::size_t cursor_{0U};
};

[[nodiscard]] RawHardwareTestCheck check(
    RawHardwareTestError error,
    std::string subject,
    std::string message) {
    return {error, std::move(subject), std::move(message)};
}

[[nodiscard]] bool bounded_text(
    std::string_view value,
    std::size_t maximum) noexcept {
    return !value.empty() && value.size() <= maximum;
}

[[nodiscard]] bool valid_utc_timestamp(std::string_view value) noexcept {
    if (value.size() != 20U || value[4] != '-' || value[7] != '-' ||
        value[10] != 'T' || value[13] != ':' || value[16] != ':' ||
        value[19] != 'Z') {
        return false;
    }
    constexpr std::array digit_positions{
        0U, 1U, 2U, 3U, 5U, 6U, 8U, 9U,
        11U, 12U, 14U, 15U, 17U, 18U};
    if (!std::all_of(
            digit_positions.begin(), digit_positions.end(),
            [&](const auto index) {
                return value[index] >= '0' && value[index] <= '9';
            })) {
        return false;
    }
    const auto two_digits = [&](std::size_t index) {
        return static_cast<unsigned int>(value[index] - '0') * 10U +
            static_cast<unsigned int>(value[index + 1U] - '0');
    };
    const auto month = two_digits(5U);
    const auto day = two_digits(8U);
    const auto hour = two_digits(11U);
    const auto minute = two_digits(14U);
    const auto second = two_digits(17U);
    return month >= 1U && month <= 12U && day >= 1U && day <= 31U &&
        hour <= 23U && minute <= 59U && second <= 59U;
}

[[nodiscard]] bool marker_record(std::string_view marker) noexcept {
    const auto matches = [&](std::string_view prefix) {
        return marker == prefix ||
            (marker.size() > prefix.size() && marker.starts_with(prefix) &&
             marker[prefix.size()] == '\t');
    };
    return matches("MIGRATED_PATCH_UNVERIFIED") ||
        matches("QUALIFICATION_INVALIDATED");
}

[[nodiscard]] bool attempt_record(std::string_view record) noexcept {
    return record.size() > kRawHardwareTestAttemptRecord.size() &&
        record.starts_with(kRawHardwareTestAttemptRecord) &&
        record[kRawHardwareTestAttemptRecord.size()] == '\t';
}

[[nodiscard]] const FixtureDefinition* find_fixture(
    const ProjectDocument& project,
    std::string_view fixture_id) noexcept {
    const auto found = std::find_if(
        project.fixtures.begin(), project.fixtures.end(),
        [&](const auto& fixture) { return fixture.id == fixture_id; });
    return found == project.fixtures.end() ? nullptr : &*found;
}

[[nodiscard]] const FixtureProfileDefinition* find_profile(
    const ProjectDocument& project,
    std::string_view profile_id) noexcept {
    const auto found = std::find_if(
        project.fixture_profiles.begin(), project.fixture_profiles.end(),
        [&](const auto& profile) { return profile.id == profile_id; });
    return found == project.fixture_profiles.end() ? nullptr : &*found;
}

[[nodiscard]] bool same_binding(
    const FixtureQualificationBinding& left,
    const FixtureQualificationBinding& right) noexcept {
    return left.fixture_id == right.fixture_id &&
        left.unit_label == right.unit_label &&
        left.manufacturer == right.manufacturer &&
        left.model == right.model && left.mode == right.mode &&
        left.profile_id == right.profile_id &&
        left.profile_revision == right.profile_revision &&
        left.behavior_fingerprint == right.behavior_fingerprint &&
        left.universe == right.universe && left.address == right.address &&
        left.output_backend == right.output_backend &&
        left.safety_policy_sha256 == right.safety_policy_sha256;
}

[[nodiscard]] bool valid_config(const RawHardwareTestConfig& config) noexcept {
    return config.observation_timeout >= kMinimumObservationTimeout &&
        config.observation_timeout <= kMaximumObservationTimeout &&
        config.session_timeout >= kMinimumSessionTimeout &&
        config.session_timeout <= kMaximumSessionTimeout &&
        config.session_timeout >= config.observation_timeout &&
        config.blackout_frame_repetitions > 0U &&
        config.blackout_frame_repetitions <= kMaximumBlackoutRepetitions;
}

[[nodiscard]] bool valid_behavior_fingerprint(std::string_view value) noexcept {
    constexpr std::string_view prefix = "sha256:";
    return value.starts_with(prefix) &&
        is_sha256_digest(value.substr(prefix.size()));
}

[[nodiscard]] bool valid_binding_text(
    const FixtureQualificationBinding& binding) noexcept {
    return bounded_text(binding.fixture_id, 255U) &&
        bounded_text(binding.unit_label, 255U) &&
        bounded_text(binding.manufacturer, 255U) &&
        bounded_text(binding.model, 255U) && bounded_text(binding.mode, 255U) &&
        bounded_text(binding.profile_id, 255U) &&
        bounded_text(binding.profile_revision, 255U) &&
        bounded_text(binding.output_backend, 128U);
}

[[nodiscard]] std::vector<std::string_view> tab_fields(std::string_view record) {
    std::vector<std::string_view> fields;
    std::size_t begin = 0U;
    while (begin <= record.size()) {
        const auto end = record.find('\t', begin);
        fields.push_back(record.substr(
            begin,
            end == std::string_view::npos ? record.size() - begin : end - begin));
        if (end == std::string_view::npos) {
            break;
        }
        begin = end + 1U;
    }
    return fields;
}

[[nodiscard]] std::string hexadecimal(std::string_view value) {
    constexpr std::array digits{
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    std::string result;
    result.resize(value.size() * 2U);
    for (std::size_t index = 0U; index < value.size(); ++index) {
        const auto byte = static_cast<unsigned char>(value[index]);
        result[index * 2U] = digits[byte >> 4U];
        result[index * 2U + 1U] = digits[byte & 0x0FU];
    }
    return result;
}

[[nodiscard]] bool decode_hexadecimal(
    std::string_view value,
    std::string& result) {
    if (value.size() % 2U != 0U ||
        value.size() / 2U > kMaximumAuditPayloadBytes) {
        return false;
    }
    const auto nibble = [](char character) -> int {
        if (character >= '0' && character <= '9') {
            return character - '0';
        }
        if (character >= 'a' && character <= 'f') {
            return 10 + character - 'a';
        }
        if (character >= 'A' && character <= 'F') {
            return 10 + character - 'A';
        }
        return -1;
    };
    result.clear();
    result.reserve(value.size() / 2U);
    for (std::size_t index = 0U; index < value.size(); index += 2U) {
        const auto high = nibble(value[index]);
        const auto low = nibble(value[index + 1U]);
        if (high < 0 || low < 0) {
            result.clear();
            return false;
        }
        result.push_back(static_cast<char>((high << 4) | low));
    }
    return true;
}

[[nodiscard]] std::string attempt_payload(
    const RawHardwareTestAttempt& attempt) {
    CanonicalWriter writer;
    writer.number(attempt.schema_version);
    writer.field(attempt.started_at_utc);
    writer.field(attempt.completed_at_utc);
    writer.number(attempt.terminal_phase);
    writer.number(attempt.terminal_error);
    writer.field(attempt.terminal_message);
    writer.number(attempt.frames_attempted);
    writer.number(attempt.frames_accepted);
    writer.field(serialize_fixture_qualification_attestation_record(
        attempt.attestation));
    return writer.value();
}

[[nodiscard]] bool read_attempt_payload(
    std::string_view payload,
    RawHardwareTestAttempt& attempt) {
    CanonicalReader reader(payload);
    std::uint8_t phase = 0U;
    std::uint8_t error = 0U;
    std::string embedded_attestation;
    if (!reader.number(attempt.schema_version) ||
        !reader.field(attempt.started_at_utc) ||
        !reader.field(attempt.completed_at_utc) ||
        !reader.number(phase) || !reader.number(error) ||
        phase > static_cast<std::uint8_t>(RawHardwareTestPhase::Cancelled) ||
        error > static_cast<std::uint8_t>(RawHardwareTestError::Replay) ||
        !reader.field(attempt.terminal_message) ||
        !reader.number(attempt.frames_attempted) ||
        !reader.number(attempt.frames_accepted) ||
        !reader.field(embedded_attestation) || !reader.finished()) {
        return false;
    }
    attempt.terminal_phase = static_cast<RawHardwareTestPhase>(phase);
    attempt.terminal_error = static_cast<RawHardwareTestError>(error);
    return parse_fixture_qualification_attestation_record(
               embedded_attestation, attempt.attestation)
        .ok();
}

[[nodiscard]] bool requirements_match_project(
    const ProjectDocument& project,
    const FixtureQualificationAttestation& attestation) {
    const auto* fixture = find_fixture(project, attestation.binding.fixture_id);
    const auto* profile = fixture == nullptr
        ? nullptr
        : find_profile(project, fixture->profile_id);
    if (fixture == nullptr || profile == nullptr || profile->footprint == 0U ||
        profile->footprint >= kMaximumFixtureQualificationRequirements ||
        attestation.requirements.size() !=
            static_cast<std::size_t>(profile->footprint) + 1U) {
        return false;
    }
    if (attestation.requirements.front().kind !=
            FixtureQualificationRequirementKind::Blackout ||
        attestation.requirements.front().absolute_channel != 0U ||
        attestation.requirements.front().value != 0U) {
        return false;
    }
    for (std::size_t index = 1U; index < attestation.requirements.size(); ++index) {
        const auto& requirement = attestation.requirements[index];
        if (requirement.kind != FixtureQualificationRequirementKind::OneHot ||
            requirement.absolute_channel !=
                static_cast<std::uint16_t>(fixture->address + index - 1U) ||
            requirement.value == 0U || !requirement.require_no_spill ||
            !bounded_text(requirement.expected_behavior, kMaximumCriterionBytes) ||
            requirement.raw_frame_sha256 !=
                fixture_qualification_expected_frame_sha256(requirement)) {
            return false;
        }
    }
    return attestation.requirements.front().raw_frame_sha256 ==
        fixture_qualification_expected_frame_sha256(
            attestation.requirements.front());
}

[[nodiscard]] bool partial_observations_consistent(
    const FixtureQualificationAttestation& attestation) {
    if (attestation.observations.size() > attestation.requirements.size()) {
        return false;
    }
    for (std::size_t index = 0U; index < attestation.observations.size(); ++index) {
        const auto& observation = attestation.observations[index];
        const auto& requirement = attestation.requirements[index];
        if (observation.requirement_id != requirement.id ||
            observation.raw_frame_sha256 != requirement.raw_frame_sha256 ||
            !bounded_text(observation.observed_behavior, kMaximumObservationBytes) ||
            observation.failure.size() > kMaximumTerminalMessageBytes ||
            (observation.passed &&
             (!observation.no_spill_observed || !observation.blackout_before ||
              !observation.blackout_after || observation.timed_out ||
              observation.device_lost || !observation.failure.empty()))) {
            return false;
        }
    }
    return true;
}

}  // namespace

SoundSwitchMicroRawHardwareTestTransport::
SoundSwitchMicroRawHardwareTestTransport(
    showcore::SoundSwitchMicroSessionConfig config) noexcept
    : config_(config) {}

bool SoundSwitchMicroRawHardwareTestTransport::open(
    const FixtureQualificationBinding& binding) noexcept {
    const auto expected =
        "soundswitch-micro:u" + std::to_string(binding.universe);
    if (binding.output_backend != expected || binding.universe == 0U ||
        !showcore::valid_soundswitch_micro_session_config(config_)) {
        return false;
    }
    return session_.open(config_);
}

bool SoundSwitchMicroRawHardwareTestTransport::connected() const noexcept {
    const auto current = session_.status();
    return current.state == showcore::SoundSwitchMicroLifecycleState::Streaming &&
        current.device_present && current.handle_open && current.warmup_complete;
}

bool SoundSwitchMicroRawHardwareTestTransport::send(
    const showcore::DmxUniverse& frame) noexcept {
    return connected() && session_.send(frame);
}

void SoundSwitchMicroRawHardwareTestTransport::close() noexcept {
    session_.close();
}

showcore::SoundSwitchMicroSessionStatus
SoundSwitchMicroRawHardwareTestTransport::status() const noexcept {
    return session_.status();
}

const char* raw_hardware_test_phase_name(RawHardwareTestPhase phase) noexcept {
    switch (phase) {
    case RawHardwareTestPhase::Idle: return "idle";
    case RawHardwareTestPhase::Opening: return "opening";
    case RawHardwareTestPhase::BlackoutBefore: return "blackout-before";
    case RawHardwareTestPhase::Stimulus: return "stimulus";
    case RawHardwareTestPhase::AwaitingObservation: return "awaiting-observation";
    case RawHardwareTestPhase::BlackoutAfter: return "blackout-after";
    case RawHardwareTestPhase::Complete: return "complete";
    case RawHardwareTestPhase::Failed: return "failed";
    case RawHardwareTestPhase::Cancelled: return "cancelled";
    }
    return "unknown";
}

const char* raw_hardware_test_error_name(RawHardwareTestError error) noexcept {
    switch (error) {
    case RawHardwareTestError::None: return "none";
    case RawHardwareTestError::InvalidConfiguration: return "invalid-configuration";
    case RawHardwareTestError::InvalidProjectBasis: return "invalid-project-basis";
    case RawHardwareTestError::InvalidBinding: return "invalid-binding";
    case RawHardwareTestError::InvalidCriteria: return "invalid-criteria";
    case RawHardwareTestError::InvalidMarkers: return "invalid-markers";
    case RawHardwareTestError::InvalidPlan: return "invalid-plan";
    case RawHardwareTestError::AlreadyStarted: return "already-started";
    case RawHardwareTestError::OpenFailed: return "open-failed";
    case RawHardwareTestError::DeviceLost: return "device-lost";
    case RawHardwareTestError::BlackoutFailed: return "blackout-failed";
    case RawHardwareTestError::FrameWriteFailed: return "frame-write-failed";
    case RawHardwareTestError::ObservationRejected: return "observation-rejected";
    case RawHardwareTestError::TimedOut: return "timed-out";
    case RawHardwareTestError::Cancelled: return "cancelled";
    case RawHardwareTestError::Incomplete: return "incomplete";
    case RawHardwareTestError::StaleProject: return "stale-project";
    case RawHardwareTestError::AttestationRejected: return "attestation-rejected";
    case RawHardwareTestError::InvalidAuditRecord: return "invalid-audit-record";
    case RawHardwareTestError::Replay: return "replay";
    }
    return "unknown";
}

RawHardwareTestCheck build_raw_hardware_test_plan(
    const ProjectDocument& candidate_project,
    std::string_view input_project_sha256,
    std::string_view fixture_id,
    std::string_view unit_label,
    std::string_view output_backend,
    std::span<const RawHardwareTestSlotCriterion> criteria,
    std::span<const std::string> markers_to_supersede,
    const RawHardwareTestConfig& config,
    RawHardwareTestPlan& plan) {
    if (!is_sha256_digest(input_project_sha256)) {
        return check(
            RawHardwareTestError::InvalidProjectBasis,
            std::string(fixture_id),
            "The active input project SHA-256 is required before hardware output opens.");
    }
    if (!valid_config(config)) {
        return check(
            RawHardwareTestError::InvalidConfiguration,
            std::string(fixture_id),
            "Raw Hardware Test timeouts or blackout repetitions are outside bounded limits.");
    }

    FixtureQualificationBinding binding;
    const auto binding_result = make_fixture_qualification_binding(
        candidate_project, fixture_id, unit_label, output_backend, binding);
    if (!binding_result.ok()) {
        return check(
            RawHardwareTestError::InvalidBinding,
            binding_result.subject,
            binding_result.message);
    }
    const auto* fixture = find_fixture(candidate_project, fixture_id);
    const auto* profile = fixture == nullptr
        ? nullptr
        : find_profile(candidate_project, fixture->profile_id);
    if (fixture == nullptr || profile == nullptr || profile->footprint == 0U ||
        profile->footprint >= kMaximumFixtureQualificationRequirements ||
        static_cast<std::uint32_t>(fixture->address) + profile->footprint - 1U >
            showcore::kUniverseSlots) {
        return check(
            RawHardwareTestError::InvalidBinding,
            std::string(fixture_id),
            "The exact fixture footprint cannot be represented by a bounded DMX universe.");
    }
    if (criteria.size() != profile->footprint) {
        return check(
            RawHardwareTestError::InvalidCriteria,
            std::string(fixture_id),
            "Every fixture slot requires one explicit reviewed behavior criterion.");
    }

    std::vector<bool> covered(profile->footprint, false);
    for (const auto& criterion : criteria) {
        if (criterion.relative_slot == 0U ||
            criterion.relative_slot > profile->footprint ||
            covered[criterion.relative_slot - 1U] ||
            criterion.one_hot_value == 0U || !criterion.require_no_spill ||
            !bounded_text(criterion.expected_behavior, kMaximumCriterionBytes)) {
            return check(
                RawHardwareTestError::InvalidCriteria,
                std::string(fixture_id),
                "Criteria must uniquely cover every one-based slot with a nonzero value, behavior, and no-spill requirement.");
        }
        covered[criterion.relative_slot - 1U] = true;
    }

    if (markers_to_supersede.empty() || markers_to_supersede.size() > 64U) {
        return check(
            RawHardwareTestError::InvalidMarkers,
            std::string(fixture_id),
            "At least one exact qualification gate marker is required.");
    }
    std::unordered_set<std::string_view> unique_markers;
    for (const auto& marker : markers_to_supersede) {
        if (!marker_record(marker) || !unique_markers.insert(marker).second ||
            std::find(
                candidate_project.unknown_records.begin(),
                candidate_project.unknown_records.end(),
                marker) == candidate_project.unknown_records.end()) {
            return check(
                RawHardwareTestError::InvalidMarkers,
                std::string(fixture_id),
                "A marker is missing, duplicated, or is not an exact qualification gate marker.");
        }
    }

    RawHardwareTestPlan candidate;
    candidate.input_project_sha256 = std::string(input_project_sha256);
    candidate.candidate_project_sha256 =
        fixture_qualification_project_basis_sha256(candidate_project);
    candidate.binding = std::move(binding);
    candidate.candidate_binding_sha256 =
        fixture_qualification_binding_sha256(candidate.binding);
    candidate.footprint = profile->footprint;
    candidate.config = config;
    candidate.markers_to_supersede.assign(
        markers_to_supersede.begin(), markers_to_supersede.end());
    candidate.requirements.reserve(static_cast<std::size_t>(profile->footprint) + 1U);

    FixtureQualificationRequirement blackout;
    blackout.id = candidate.binding.fixture_id + ":blackout";
    blackout.kind = FixtureQualificationRequirementKind::Blackout;
    blackout.expected_behavior =
        "All emitters on this physical unit remain dark; no patched neighbor responds.";
    blackout.require_no_spill = true;
    blackout.raw_frame_sha256 =
        fixture_qualification_expected_frame_sha256(blackout);
    candidate.requirements.push_back(std::move(blackout));

    for (std::uint16_t slot = 1U; slot <= profile->footprint; ++slot) {
        const auto found = std::find_if(
            criteria.begin(), criteria.end(),
            [&](const auto& criterion) { return criterion.relative_slot == slot; });
        FixtureQualificationRequirement requirement;
        requirement.id = candidate.binding.fixture_id + ":slot-" +
            std::to_string(slot);
        requirement.kind = FixtureQualificationRequirementKind::OneHot;
        requirement.absolute_channel = static_cast<std::uint16_t>(
            fixture->address + slot - 1U);
        requirement.value = found->one_hot_value;
        requirement.expected_behavior = found->expected_behavior;
        requirement.require_no_spill = true;
        requirement.raw_frame_sha256 =
            fixture_qualification_expected_frame_sha256(requirement);
        candidate.requirements.push_back(std::move(requirement));
    }

    const auto validation = validate_raw_hardware_test_plan(candidate);
    if (!validation.ok()) {
        return validation;
    }
    plan = std::move(candidate);
    return {};
}

RawHardwareTestCheck validate_raw_hardware_test_plan(
    const RawHardwareTestPlan& plan) {
    if (plan.schema_version != kRawHardwareTestPlanVersion ||
        !valid_config(plan.config)) {
        return check(
            RawHardwareTestError::InvalidConfiguration,
            plan.binding.fixture_id,
            "The Raw Hardware Test plan schema or bounded timing configuration is invalid.");
    }
    if (!is_sha256_digest(plan.input_project_sha256) ||
        !is_sha256_digest(plan.candidate_project_sha256)) {
        return check(
            RawHardwareTestError::InvalidProjectBasis,
            plan.binding.fixture_id,
            "The plan does not bind both active input and candidate project identities.");
    }
    if (!is_sha256_digest(plan.candidate_binding_sha256) ||
        plan.candidate_binding_sha256 !=
            fixture_qualification_binding_sha256(plan.binding) ||
        !valid_binding_text(plan.binding) ||
        !valid_behavior_fingerprint(plan.binding.behavior_fingerprint) ||
        !is_sha256_digest(plan.binding.safety_policy_sha256) ||
        plan.binding.universe == 0U || plan.binding.address == 0U ||
        plan.footprint == 0U ||
        static_cast<std::uint32_t>(plan.binding.address) + plan.footprint - 1U >
            showcore::kUniverseSlots) {
        return check(
            RawHardwareTestError::InvalidBinding,
            plan.binding.fixture_id,
            "The fixture, unit, profile behavior, patch, backend, or safety binding is malformed.");
    }
    if (plan.requirements.size() !=
            static_cast<std::size_t>(plan.footprint) + 1U ||
        plan.requirements.empty()) {
        return check(
            RawHardwareTestError::InvalidPlan,
            plan.binding.fixture_id,
            "The plan must contain one blackout plus one one-hot frame per fixture slot.");
    }
    std::unordered_set<std::string_view> ids;
    for (std::size_t index = 0U; index < plan.requirements.size(); ++index) {
        const auto& requirement = plan.requirements[index];
        if (!bounded_text(requirement.id, 255U) ||
            !ids.insert(requirement.id).second ||
            !bounded_text(requirement.expected_behavior, kMaximumCriterionBytes) ||
            !requirement.require_no_spill ||
            requirement.raw_frame_sha256 !=
                fixture_qualification_expected_frame_sha256(requirement)) {
            return check(
                RawHardwareTestError::InvalidPlan,
                plan.binding.fixture_id,
                "A plan requirement is duplicated, unbounded, permits spill, or has a tampered raw frame digest.");
        }
        if (index == 0U) {
            if (requirement.kind != FixtureQualificationRequirementKind::Blackout ||
                requirement.absolute_channel != 0U || requirement.value != 0U) {
                return check(
                    RawHardwareTestError::InvalidPlan,
                    plan.binding.fixture_id,
                    "The first test requirement must be an all-zero blackout frame.");
            }
            continue;
        }
        if (requirement.kind != FixtureQualificationRequirementKind::OneHot ||
            requirement.absolute_channel != static_cast<std::uint16_t>(
                plan.binding.address + index - 1U) ||
            requirement.value == 0U) {
            return check(
                RawHardwareTestError::InvalidPlan,
                plan.binding.fixture_id,
                "One-hot requirements must cover the exact fixture footprint in deterministic slot order.");
        }
    }
    if (plan.markers_to_supersede.empty() ||
        plan.markers_to_supersede.size() > 64U) {
        return check(
            RawHardwareTestError::InvalidMarkers,
            plan.binding.fixture_id,
            "The plan does not bind exact qualification markers.");
    }
    std::unordered_set<std::string_view> markers;
    for (const auto& marker : plan.markers_to_supersede) {
        if (!bounded_text(marker, 4096U) || !marker_record(marker) ||
            !markers.insert(marker).second) {
            return check(
                RawHardwareTestError::InvalidMarkers,
                plan.binding.fixture_id,
                "The plan contains a malformed or duplicate qualification marker.");
        }
    }
    return {};
}

RawHardwareTestCheck make_raw_hardware_test_frame(
    const RawHardwareTestPlan& plan,
    std::size_t requirement_index,
    showcore::DmxUniverse& frame) {
    const auto validation = validate_raw_hardware_test_plan(plan);
    if (!validation.ok()) {
        return validation;
    }
    if (requirement_index >= plan.requirements.size()) {
        return check(
            RawHardwareTestError::InvalidPlan,
            plan.binding.fixture_id,
            "The requested Raw Hardware Test frame is outside the bounded plan.");
    }
    frame.fill(0U);
    const auto& requirement = plan.requirements[requirement_index];
    if (requirement.kind == FixtureQualificationRequirementKind::OneHot) {
        frame[requirement.absolute_channel - 1U] = requirement.value;
    }
    return {};
}

RawHardwareTestSession::~RawHardwareTestSession() {
    if (snapshot_.transport_open && transport_ != nullptr) {
        snapshot_.shutdown_blackout_attempted = true;
        snapshot_.shutdown_blackout_succeeded = send_blackout();
        close_transport();
    }
}

RawHardwareTestCheck RawHardwareTestSession::begin(
    RawHardwareTestPlan plan,
    RawHardwareTestRunIdentity identity,
    RawHardwareTestTransport& transport,
    TimePoint now) {
    if (has_plan_ || snapshot_.phase != RawHardwareTestPhase::Idle) {
        return check(
            RawHardwareTestError::AlreadyStarted,
            plan.binding.fixture_id,
            "A Raw Hardware Test session is single-use and has already started.");
    }
    const auto plan_validation = validate_raw_hardware_test_plan(plan);
    if (!plan_validation.ok()) {
        return plan_validation;
    }
    if (!bounded_text(identity.operator_id, kMaximumOperatorBytes) ||
        !valid_utc_timestamp(identity.started_at_utc)) {
        return check(
            RawHardwareTestError::InvalidConfiguration,
            plan.binding.fixture_id,
            "A bounded operator identity and exact UTC start timestamp are required.");
    }

    plan_ = std::move(plan);
    identity_ = std::move(identity);
    transport_ = &transport;
    has_plan_ = true;
    observations_.clear();
    observations_.reserve(plan_.requirements.size());
    snapshot_.total_requirements = plan_.requirements.size();
    snapshot_.phase = RawHardwareTestPhase::Opening;
    session_deadline_ = now + plan_.config.session_timeout;

    if (!transport_->open(plan_.binding)) {
        finish_failure(
            RawHardwareTestError::OpenFailed,
            "The bounded output transport could not open.",
            false,
            false);
        return check(snapshot_.error, plan_.binding.fixture_id, snapshot_.message);
    }
    snapshot_.transport_open = true;
    if (!transport_->connected()) {
        finish_failure(
            RawHardwareTestError::DeviceLost,
            "The output device was not connected after opening.",
            false,
            true);
        return check(snapshot_.error, plan_.binding.fixture_id, snapshot_.message);
    }
    return start_current(now);
}

RawHardwareTestCheck RawHardwareTestSession::start_current(TimePoint now) {
    if (!has_plan_ || transport_ == nullptr ||
        snapshot_.current_requirement >= plan_.requirements.size()) {
        finish_failure(
            RawHardwareTestError::InvalidPlan,
            "The session attempted to leave its bounded fixture plan.",
            false,
            false);
        return check(snapshot_.error, plan_.binding.fixture_id, snapshot_.message);
    }
    if (now >= session_deadline_) {
        finish_failure(
            RawHardwareTestError::TimedOut,
            "The bounded Raw Hardware Test session timed out.",
            true,
            false);
        return check(snapshot_.error, plan_.binding.fixture_id, snapshot_.message);
    }
    if (!transport_->connected()) {
        finish_failure(
            RawHardwareTestError::DeviceLost,
            "The output device disconnected before the next raw frame.",
            false,
            true);
        return check(snapshot_.error, plan_.binding.fixture_id, snapshot_.message);
    }

    current_blackout_before_ = false;
    snapshot_.phase = RawHardwareTestPhase::BlackoutBefore;
    if (!send_blackout()) {
        finish_failure(
            RawHardwareTestError::BlackoutFailed,
            "The mandatory blackout-before sequence was not fully accepted.",
            false,
            false);
        return check(snapshot_.error, plan_.binding.fixture_id, snapshot_.message);
    }
    current_blackout_before_ = true;

    showcore::DmxUniverse frame{};
    const auto frame_result = make_raw_hardware_test_frame(
        plan_, snapshot_.current_requirement, frame);
    if (!frame_result.ok()) {
        finish_failure(
            frame_result.error,
            frame_result.message,
            false,
            false);
        return frame_result;
    }
    snapshot_.phase = RawHardwareTestPhase::Stimulus;
    if (!send_frame(frame)) {
        finish_failure(
            RawHardwareTestError::FrameWriteFailed,
            "The exact bounded raw stimulus frame was not accepted.",
            false,
            false);
        return check(snapshot_.error, plan_.binding.fixture_id, snapshot_.message);
    }
    snapshot_.phase = RawHardwareTestPhase::AwaitingObservation;
    observation_deadline_ = now + plan_.config.observation_timeout;
    return {};
}

RawHardwareTestCheck RawHardwareTestSession::poll(TimePoint now) {
    if (snapshot_.phase == RawHardwareTestPhase::Complete) {
        return {};
    }
    if (snapshot_.phase == RawHardwareTestPhase::Failed ||
        snapshot_.phase == RawHardwareTestPhase::Cancelled) {
        return check(snapshot_.error, plan_.binding.fixture_id, snapshot_.message);
    }
    if (snapshot_.phase != RawHardwareTestPhase::AwaitingObservation ||
        transport_ == nullptr) {
        return check(
            RawHardwareTestError::Incomplete,
            has_plan_ ? plan_.binding.fixture_id : std::string{},
            "The Raw Hardware Test is not awaiting an observation.");
    }
    if (!transport_->connected()) {
        finish_failure(
            RawHardwareTestError::DeviceLost,
            "The output device disconnected while awaiting observation.",
            false,
            true);
    } else if (now >= observation_deadline_ || now >= session_deadline_) {
        finish_failure(
            RawHardwareTestError::TimedOut,
            "The operator observation window expired and output was shut down.",
            true,
            false);
    }
    return snapshot_.error == RawHardwareTestError::None
        ? RawHardwareTestCheck{}
        : check(snapshot_.error, plan_.binding.fixture_id, snapshot_.message);
}

RawHardwareTestCheck RawHardwareTestSession::submit_observation(
    RawHardwareTestObservedResult observation,
    TimePoint now) {
    const auto current = poll(now);
    if (!current.ok()) {
        return current;
    }
    if (!bounded_text(observation.observed_behavior, kMaximumObservationBytes)) {
        finish_failure(
            RawHardwareTestError::ObservationRejected,
            "Observed behavior is missing or exceeds the bounded audit field.",
            false,
            false);
        return check(snapshot_.error, plan_.binding.fixture_id, snapshot_.message);
    }

    const auto& requirement = plan_.requirements[snapshot_.current_requirement];
    FixtureQualificationObservation recorded;
    recorded.requirement_id = requirement.id;
    recorded.raw_frame_sha256 = requirement.raw_frame_sha256;
    recorded.observed_behavior = std::move(observation.observed_behavior);
    recorded.passed = observation.passed;
    recorded.no_spill_observed = observation.no_spill_observed;
    recorded.blackout_before = current_blackout_before_;

    snapshot_.phase = RawHardwareTestPhase::BlackoutAfter;
    recorded.blackout_after = send_blackout();
    if (!recorded.blackout_after) {
        recorded.passed = false;
        recorded.failure = "blackout-after-write-failed";
        observations_.push_back(std::move(recorded));
        finish_failure(
            RawHardwareTestError::BlackoutFailed,
            "The mandatory blackout-after sequence was not fully accepted.",
            false,
            false);
        return check(snapshot_.error, plan_.binding.fixture_id, snapshot_.message);
    }
    if (!recorded.passed || !recorded.no_spill_observed) {
        recorded.passed = false;
        recorded.failure = recorded.no_spill_observed
            ? "operator-rejected-observation"
            : "spill-or-neighbor-response-observed";
        observations_.push_back(std::move(recorded));
        finish_failure(
            RawHardwareTestError::ObservationRejected,
            "The observed fixture behavior or no-spill criterion did not pass.",
            false,
            false);
        return check(snapshot_.error, plan_.binding.fixture_id, snapshot_.message);
    }

    observations_.push_back(std::move(recorded));
    ++snapshot_.completed_requirements;
    ++snapshot_.current_requirement;
    if (snapshot_.current_requirement == plan_.requirements.size()) {
        snapshot_.phase = RawHardwareTestPhase::Complete;
        snapshot_.error = RawHardwareTestError::None;
        snapshot_.message.clear();
        close_transport();
        return {};
    }
    return start_current(now);
}

RawHardwareTestCheck RawHardwareTestSession::cancel(
    std::string_view reason,
    TimePoint) {
    if (snapshot_.phase != RawHardwareTestPhase::AwaitingObservation &&
        snapshot_.phase != RawHardwareTestPhase::Opening &&
        snapshot_.phase != RawHardwareTestPhase::BlackoutBefore &&
        snapshot_.phase != RawHardwareTestPhase::Stimulus &&
        snapshot_.phase != RawHardwareTestPhase::BlackoutAfter) {
        return check(
            RawHardwareTestError::Incomplete,
            has_plan_ ? plan_.binding.fixture_id : std::string{},
            "Only an active Raw Hardware Test can be cancelled.");
    }
    if (!bounded_text(reason, kMaximumTerminalMessageBytes)) {
        return check(
            RawHardwareTestError::ObservationRejected,
            plan_.binding.fixture_id,
            "Cancellation requires a bounded audit reason.");
    }
    finish_failure(
        RawHardwareTestError::Cancelled,
        std::string(reason),
        false,
        false);
    return check(snapshot_.error, plan_.binding.fixture_id, snapshot_.message);
}

RawHardwareTestCheck RawHardwareTestSession::make_attempt(
    const ProjectDocument& candidate_project,
    std::string_view completed_at_utc,
    RawHardwareTestAttempt& attempt) const {
    if (!has_plan_ ||
        (snapshot_.phase != RawHardwareTestPhase::Complete &&
         snapshot_.phase != RawHardwareTestPhase::Failed &&
         snapshot_.phase != RawHardwareTestPhase::Cancelled)) {
        return check(
            RawHardwareTestError::Incomplete,
            has_plan_ ? plan_.binding.fixture_id : std::string{},
            "Only a terminal Raw Hardware Test can produce an immutable audit attempt.");
    }
    if (!valid_utc_timestamp(completed_at_utc) ||
        completed_at_utc < identity_.started_at_utc) {
        return check(
            RawHardwareTestError::InvalidAuditRecord,
            plan_.binding.fixture_id,
            "The audit completion timestamp is malformed or precedes the start.");
    }
    if (fixture_qualification_project_basis_sha256(candidate_project) !=
        plan_.candidate_project_sha256) {
        return check(
            RawHardwareTestError::StaleProject,
            plan_.binding.fixture_id,
            "The candidate project changed while the hardware test was running.");
    }

    RawHardwareTestAttempt candidate;
    candidate.started_at_utc = identity_.started_at_utc;
    candidate.completed_at_utc = std::string(completed_at_utc);
    candidate.terminal_phase = snapshot_.phase;
    candidate.terminal_error = snapshot_.error;
    candidate.terminal_message = snapshot_.message;
    candidate.frames_attempted = snapshot_.frames_attempted;
    candidate.frames_accepted = snapshot_.frames_accepted;
    candidate.attestation.input_project_sha256 = plan_.input_project_sha256;
    candidate.attestation.operator_id = identity_.operator_id;
    candidate.attestation.observed_at_utc = candidate.completed_at_utc;
    candidate.attestation.binding = plan_.binding;
    candidate.attestation.requirements = plan_.requirements;
    candidate.attestation.observations = observations_;
    candidate.attestation.markers_to_supersede = plan_.markers_to_supersede;

    const auto attestation_result = seal_fixture_qualification_attestation(
        candidate_project, candidate.attestation);
    if (snapshot_.phase == RawHardwareTestPhase::Complete) {
        if (!attestation_result.ok()) {
            return check(
                RawHardwareTestError::AttestationRejected,
                attestation_result.subject,
                attestation_result.message);
        }
    } else if (candidate.attestation.content_sha256.empty()) {
        return check(
            RawHardwareTestError::InvalidAuditRecord,
            plan_.binding.fixture_id,
            "The interrupted attempt could not be content-addressed.");
    }

    const auto sealed = seal_raw_hardware_test_attempt(candidate);
    if (!sealed.ok()) {
        return sealed;
    }
    attempt = std::move(candidate);
    return {};
}

RawHardwareTestSnapshot RawHardwareTestSession::snapshot() const {
    return snapshot_;
}

const RawHardwareTestPlan* RawHardwareTestSession::plan() const noexcept {
    return has_plan_ ? &plan_ : nullptr;
}

const std::vector<FixtureQualificationObservation>&
RawHardwareTestSession::observations() const noexcept {
    return observations_;
}

bool RawHardwareTestSession::send_frame(
    const showcore::DmxUniverse& frame) noexcept {
    if (transport_ == nullptr || !snapshot_.transport_open) {
        return false;
    }
    ++snapshot_.frames_attempted;
    if (!transport_->send(frame)) {
        return false;
    }
    ++snapshot_.frames_accepted;
    return true;
}

bool RawHardwareTestSession::send_blackout() noexcept {
    showcore::DmxUniverse blackout{};
    bool accepted = true;
    for (std::uint16_t count = 0U;
         count < plan_.config.blackout_frame_repetitions;
         ++count) {
        if (!send_frame(blackout)) {
            accepted = false;
        }
    }
    return accepted;
}

void RawHardwareTestSession::finish_failure(
    RawHardwareTestError error,
    std::string message,
    bool timed_out,
    bool device_lost) {
    snapshot_.error = error;
    snapshot_.message = std::move(message);
    snapshot_.phase = error == RawHardwareTestError::Cancelled
        ? RawHardwareTestPhase::Cancelled
        : RawHardwareTestPhase::Failed;

    bool shutdown_succeeded = false;
    if (snapshot_.transport_open && transport_ != nullptr) {
        snapshot_.shutdown_blackout_attempted = true;
        shutdown_succeeded = send_blackout();
        snapshot_.shutdown_blackout_succeeded = shutdown_succeeded;
    }

    if (has_plan_ &&
        snapshot_.current_requirement < plan_.requirements.size() &&
        observations_.size() == snapshot_.completed_requirements) {
        const auto& requirement =
            plan_.requirements[snapshot_.current_requirement];
        FixtureQualificationObservation observation;
        observation.requirement_id = requirement.id;
        observation.raw_frame_sha256 = requirement.raw_frame_sha256;
        observation.observed_behavior = snapshot_.message;
        observation.blackout_before = current_blackout_before_;
        observation.blackout_after = shutdown_succeeded;
        observation.timed_out = timed_out;
        observation.device_lost = device_lost;
        observation.failure = std::string(raw_hardware_test_error_name(error));
        observations_.push_back(std::move(observation));
    }
    close_transport();
}

void RawHardwareTestSession::close_transport() noexcept {
    if (snapshot_.transport_open && transport_ != nullptr) {
        transport_->close();
    }
    snapshot_.transport_open = false;
}

RawHardwareTestCheck seal_raw_hardware_test_attempt(
    RawHardwareTestAttempt& attempt) {
    FixtureQualificationAttestation parsed_attestation;
    const auto embedded_result = parse_fixture_qualification_attestation_record(
        serialize_fixture_qualification_attestation_record(attempt.attestation),
        parsed_attestation);
    if (attempt.schema_version != kRawHardwareTestAttemptVersion ||
        !valid_utc_timestamp(attempt.started_at_utc) ||
        !valid_utc_timestamp(attempt.completed_at_utc) ||
        attempt.completed_at_utc < attempt.started_at_utc ||
        attempt.frames_accepted > attempt.frames_attempted ||
        attempt.attestation.content_sha256.empty() || !embedded_result.ok()) {
        return check(
            RawHardwareTestError::InvalidAuditRecord,
            attempt.attestation.binding.fixture_id,
            "The Raw Hardware Test attempt metadata cannot be content-addressed.");
    }
    const auto terminal =
        attempt.terminal_phase == RawHardwareTestPhase::Complete ||
        attempt.terminal_phase == RawHardwareTestPhase::Failed ||
        attempt.terminal_phase == RawHardwareTestPhase::Cancelled;
    const auto cancelled = attempt.terminal_phase == RawHardwareTestPhase::Cancelled;
    if (!terminal ||
        (attempt.terminal_phase == RawHardwareTestPhase::Complete &&
         (attempt.terminal_error != RawHardwareTestError::None ||
          !attempt.terminal_message.empty())) ||
        (attempt.terminal_phase != RawHardwareTestPhase::Complete &&
         (attempt.terminal_error == RawHardwareTestError::None ||
          !bounded_text(
              attempt.terminal_message, kMaximumTerminalMessageBytes))) ||
        (cancelled !=
         (attempt.terminal_error == RawHardwareTestError::Cancelled))) {
        return check(
            RawHardwareTestError::InvalidAuditRecord,
            attempt.attestation.binding.fixture_id,
            "The audit terminal phase, error, and message are contradictory.");
    }
    attempt.content_sha256 = sha256_text(attempt_payload(attempt));
    return {};
}

RawHardwareTestCheck validate_raw_hardware_test_attempt(
    const ProjectDocument& project,
    const RawHardwareTestAttempt& attempt) {
    auto content_copy = attempt;
    content_copy.content_sha256.clear();
    const auto sealed = seal_raw_hardware_test_attempt(content_copy);
    if (!sealed.ok() || !is_sha256_digest(attempt.content_sha256) ||
        attempt.content_sha256 != content_copy.content_sha256) {
        return check(
            RawHardwareTestError::InvalidAuditRecord,
            attempt.attestation.binding.fixture_id,
            "The Raw Hardware Test audit digest is malformed or tampered.");
    }
    if (attempt.attestation.candidate_project_sha256 !=
        fixture_qualification_project_basis_sha256(project)) {
        return check(
            RawHardwareTestError::StaleProject,
            attempt.attestation.binding.fixture_id,
            "The Raw Hardware Test audit belongs to a different project basis.");
    }
    if (attempt.attestation.schema_version !=
            kFixtureQualificationAttestationVersion ||
        !is_sha256_digest(attempt.attestation.input_project_sha256) ||
        !bounded_text(
            attempt.attestation.operator_id, kMaximumOperatorBytes) ||
        attempt.attestation.observed_at_utc != attempt.completed_at_utc ||
        !valid_utc_timestamp(attempt.attestation.observed_at_utc)) {
        return check(
            RawHardwareTestError::InvalidAuditRecord,
            attempt.attestation.binding.fixture_id,
            "The audit input identity, operator, or observation timestamp is malformed.");
    }
    FixtureQualificationBinding current_binding;
    const auto binding_result = make_fixture_qualification_binding(
        project,
        attempt.attestation.binding.fixture_id,
        attempt.attestation.binding.unit_label,
        attempt.attestation.binding.output_backend,
        current_binding);
    if (!binding_result.ok() ||
        !same_binding(current_binding, attempt.attestation.binding) ||
        attempt.attestation.candidate_binding_sha256 !=
            fixture_qualification_binding_sha256(attempt.attestation.binding) ||
        !requirements_match_project(project, attempt.attestation) ||
        !partial_observations_consistent(attempt.attestation)) {
        return check(
            RawHardwareTestError::InvalidAuditRecord,
            attempt.attestation.binding.fixture_id,
            "The audit fixture binding, requirements, or partial observations are inconsistent.");
    }
    if (attempt.attestation.markers_to_supersede.empty() ||
        attempt.attestation.markers_to_supersede.size() > 64U) {
        return check(
            RawHardwareTestError::InvalidAuditRecord,
            attempt.attestation.binding.fixture_id,
            "The audit does not bind exact qualification markers.");
    }
    std::unordered_set<std::string_view> markers;
    for (const auto& marker : attempt.attestation.markers_to_supersede) {
        if (!bounded_text(marker, 4096U) || !marker_record(marker) ||
            !markers.insert(marker).second ||
            std::find(
                project.unknown_records.begin(),
                project.unknown_records.end(),
                marker) == project.unknown_records.end()) {
            return check(
                RawHardwareTestError::InvalidAuditRecord,
                attempt.attestation.binding.fixture_id,
                "An audit marker is missing, duplicated, or malformed.");
        }
    }
    if (attempt.terminal_phase == RawHardwareTestPhase::Complete) {
        const auto attestation_result =
            validate_fixture_qualification_attestation(project, attempt.attestation);
        if (!attestation_result.ok()) {
            return check(
                RawHardwareTestError::AttestationRejected,
                attestation_result.subject,
                attestation_result.message);
        }
    }
    return {};
}

std::string serialize_raw_hardware_test_attempt_record(
    const RawHardwareTestAttempt& attempt) {
    const auto payload = attempt_payload(attempt);
    return std::string(kRawHardwareTestAttemptRecord) + "\t1\t" +
        attempt.content_sha256 + "\t" + hexadecimal(payload);
}

RawHardwareTestCheck parse_raw_hardware_test_attempt_record(
    std::string_view record,
    RawHardwareTestAttempt& attempt) {
    const auto fields = tab_fields(record);
    if (fields.size() != 4U || fields[0] != kRawHardwareTestAttemptRecord ||
        fields[1] != "1" || !is_sha256_digest(fields[2])) {
        return check(
            RawHardwareTestError::InvalidAuditRecord,
            {},
            "The Raw Hardware Test audit envelope is malformed.");
    }
    std::string payload;
    if (!decode_hexadecimal(fields[3], payload) ||
        sha256_text(payload) != fields[2]) {
        return check(
            RawHardwareTestError::InvalidAuditRecord,
            {},
            "The Raw Hardware Test audit payload is malformed or tampered.");
    }
    RawHardwareTestAttempt parsed;
    if (!read_attempt_payload(payload, parsed)) {
        return check(
            RawHardwareTestError::InvalidAuditRecord,
            {},
            "The Raw Hardware Test audit payload cannot be decoded.");
    }
    parsed.content_sha256 = std::string(fields[2]);
    attempt = std::move(parsed);
    return {};
}

RawHardwareTestCheck record_raw_hardware_test_attempt(
    ProjectDocument& project,
    const RawHardwareTestAttempt& attempt) {
    const auto validation = validate_raw_hardware_test_attempt(project, attempt);
    if (!validation.ok()) {
        return validation;
    }
    for (const auto& record : project.unknown_records) {
        if (!attempt_record(record)) {
            continue;
        }
        const auto fields = tab_fields(record);
        if (fields.size() >= 3U && fields[2] == attempt.content_sha256) {
            return check(
                RawHardwareTestError::Replay,
                attempt.attestation.binding.fixture_id,
                "This Raw Hardware Test attempt is already in the append-only audit.");
        }
    }
    project.unknown_records.push_back(
        serialize_raw_hardware_test_attempt_record(attempt));
    return {};
}

RawHardwareTestCheck graduate_raw_hardware_test_attempt(
    ProjectDocument& project,
    const RawHardwareTestAttempt& attempt) {
    if (attempt.terminal_phase != RawHardwareTestPhase::Complete ||
        attempt.terminal_error != RawHardwareTestError::None) {
        return check(
            RawHardwareTestError::AttestationRejected,
            attempt.attestation.binding.fixture_id,
            "Only a fully completed Raw Hardware Test attempt can graduate a candidate.");
    }
    const auto validation = validate_raw_hardware_test_attempt(project, attempt);
    if (!validation.ok()) {
        return validation;
    }
    auto candidate = project;
    const auto recorded = record_raw_hardware_test_attempt(candidate, attempt);
    if (!recorded.ok()) {
        return recorded;
    }
    const auto graduated =
        graduate_fixture_qualification(candidate, attempt.attestation);
    if (!graduated.ok()) {
        return check(
            RawHardwareTestError::AttestationRejected,
            graduated.subject,
            graduated.message);
    }
    project = std::move(candidate);
    return {};
}

}  // namespace emberlights
