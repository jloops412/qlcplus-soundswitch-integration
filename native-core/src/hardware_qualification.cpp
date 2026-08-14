#include "emberlights/hardware_qualification.hpp"

#include "emberlights/compiler.hpp"
#include "emberlights/file_identity.hpp"
#include "emberlights/fixture_profile_upgrade.hpp"
#include "emberlights/project_io.hpp"
#include "emberlights/raw_hardware_test.hpp"
#include "showcore/layer_resolver.hpp"
#include "showcore/look.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace emberlights {
namespace {

constexpr std::size_t kMaximumQualificationPayloadBytes = 2U * 1024U * 1024U;
constexpr std::size_t kMaximumQualificationTextBytes = 4096U;

struct Ir4SafeLookDefinition {
    Ir4SixChannelSafeLook kind;
    std::string_view id;
    std::string_view name;
    showcore::Property active_property;
    std::uint16_t active_channel;
};

constexpr std::array<Ir4SafeLookDefinition, kIr4SixChannelSafeLookCount>
    kIr4SafeLooks{{
        {Ir4SixChannelSafeLook::Blackout,
         "ir4-bench-blackout",
         "Blackout",
         showcore::Property::Count,
         0U},
        {Ir4SixChannelSafeLook::Red,
         "ir4-bench-red",
         "Red",
         showcore::Property::Red,
         1U},
        {Ir4SixChannelSafeLook::Green,
         "ir4-bench-green",
         "Green",
         showcore::Property::Green,
         2U},
        {Ir4SixChannelSafeLook::Blue,
         "ir4-bench-blue",
         "Blue",
         showcore::Property::Blue,
         3U},
        {Ir4SixChannelSafeLook::White,
         "ir4-bench-white",
         "White",
         showcore::Property::White,
         4U},
        {Ir4SixChannelSafeLook::Amber,
         "ir4-bench-amber",
         "Amber",
         showcore::Property::Amber,
         5U},
    }};

constexpr std::array kIr4SixChannelProperties{
    showcore::Property::Red,
    showcore::Property::Green,
    showcore::Property::Blue,
    showcore::Property::White,
    showcore::Property::Amber,
    showcore::Property::UV};

[[nodiscard]] const Ir4SafeLookDefinition* ir4_safe_look_definition(
    Ir4SixChannelSafeLook look) noexcept {
    const auto found = std::find_if(
        kIr4SafeLooks.begin(), kIr4SafeLooks.end(),
        [look](const auto& candidate) { return candidate.kind == look; });
    return found == kIr4SafeLooks.end() ? nullptr : &*found;
}

[[nodiscard]] LookDefinition make_ir4_safe_look(
    const Ir4SafeLookDefinition& definition) {
    LookDefinition look;
    look.id = definition.id;
    look.name = definition.name;
    look.fade_ms = 0U;
    look.assignments.reserve(kIr4SixChannelProperties.size());
    for (const auto property : kIr4SixChannelProperties) {
        look.assignments.push_back({
            "ir4-bench-001",
            property,
            property == definition.active_property
                ? showcore::PropertyValue::set(1.0F)
                : showcore::PropertyValue::force_zero()});
    }
    return look;
}

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

    void flag(bool value) { field(value ? "1" : "0"); }

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
        const auto length_text = value_.substr(cursor_, separator - cursor_);
        const auto converted = std::from_chars(
            length_text.data(), length_text.data() + length_text.size(), length);
        if (converted.ec != std::errc{} ||
            converted.ptr != length_text.data() + length_text.size() ||
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
        if (converted.ec != std::errc{} || converted.ptr != text.data() + text.size() ||
            value > static_cast<unsigned long long>(std::numeric_limits<Integer>::max())) {
            return false;
        }
        output = static_cast<Integer>(value);
        return true;
    }

    [[nodiscard]] bool flag(bool& output) {
        std::string text;
        if (!field(text) || (text != "0" && text != "1")) {
            return false;
        }
        output = text == "1";
        return true;
    }

    [[nodiscard]] bool finished() const noexcept { return cursor_ == value_.size(); }

private:
    std::string_view value_;
    std::size_t cursor_{0U};
};

[[nodiscard]] FixtureQualificationCheck qualification_check(
    FixtureQualificationStatus status,
    std::string subject,
    std::string message) {
    return {status, std::move(subject), std::move(message)};
}

[[nodiscard]] bool record_starts_with(
    std::string_view record,
    std::string_view prefix) noexcept {
    return record == prefix ||
        (record.size() > prefix.size() && record.starts_with(prefix) &&
         record[prefix.size()] == '\t');
}

[[nodiscard]] bool is_qualification_artifact(std::string_view record) noexcept {
    return record_starts_with(record, kFixtureQualificationAttestationRecord) ||
        record_starts_with(record, kFixtureQualificationSupersessionRecord) ||
        record_starts_with(record, kFixtureQualificationRevocationRecord) ||
        record_starts_with(record, kRawHardwareTestAttemptRecord);
}

[[nodiscard]] bool is_qualification_gate_marker(std::string_view record) noexcept {
    return record_starts_with(record, "MIGRATED_PATCH_UNVERIFIED") ||
        record_starts_with(record, "QUALIFICATION_INVALIDATED");
}

[[nodiscard]] std::vector<std::string_view> tab_fields(std::string_view record) {
    std::vector<std::string_view> fields;
    std::size_t begin = 0U;
    while (begin <= record.size()) {
        const auto end = record.find('\t', begin);
        fields.push_back(record.substr(
            begin, end == std::string_view::npos ? record.size() - begin : end - begin));
        if (end == std::string_view::npos) {
            break;
        }
        begin = end + 1U;
    }
    return fields;
}

[[nodiscard]] std::string hexadecimal(std::string_view value) {
    constexpr std::string_view digits = "0123456789abcdef";
    std::string encoded;
    encoded.reserve(value.size() * 2U);
    for (const auto character : value) {
        const auto byte = static_cast<unsigned char>(character);
        encoded.push_back(digits[byte >> 4U]);
        encoded.push_back(digits[byte & 0x0FU]);
    }
    return encoded;
}

[[nodiscard]] bool decode_hexadecimal(
    std::string_view encoded,
    std::string& value) {
    if (encoded.size() % 2U != 0U || encoded.size() / 2U > kMaximumQualificationPayloadBytes) {
        return false;
    }
    const auto nibble = [](char character, std::uint8_t& result) {
        if (character >= '0' && character <= '9') {
            result = static_cast<std::uint8_t>(character - '0');
            return true;
        }
        if (character >= 'a' && character <= 'f') {
            result = static_cast<std::uint8_t>(character - 'a' + 10);
            return true;
        }
        return false;
    };
    value.clear();
    value.reserve(encoded.size() / 2U);
    for (std::size_t index = 0U; index < encoded.size(); index += 2U) {
        std::uint8_t high = 0U;
        std::uint8_t low = 0U;
        if (!nibble(encoded[index], high) || !nibble(encoded[index + 1U], low)) {
            return false;
        }
        value.push_back(static_cast<char>((high << 4U) | low));
    }
    return true;
}

void write_binding(CanonicalWriter& writer, const FixtureQualificationBinding& binding) {
    writer.field(binding.fixture_id);
    writer.field(binding.unit_label);
    writer.field(binding.manufacturer);
    writer.field(binding.model);
    writer.field(binding.mode);
    writer.field(binding.profile_id);
    writer.field(binding.profile_revision);
    writer.field(binding.behavior_fingerprint);
    writer.number(binding.universe);
    writer.number(binding.address);
    writer.field(binding.output_backend);
    writer.field(binding.safety_policy_sha256);
}

[[nodiscard]] bool read_binding(
    CanonicalReader& reader,
    FixtureQualificationBinding& binding) {
    return reader.field(binding.fixture_id) &&
        reader.field(binding.unit_label) &&
        reader.field(binding.manufacturer) &&
        reader.field(binding.model) &&
        reader.field(binding.mode) &&
        reader.field(binding.profile_id) &&
        reader.field(binding.profile_revision) &&
        reader.field(binding.behavior_fingerprint) &&
        reader.number(binding.universe) &&
        reader.number(binding.address) &&
        reader.field(binding.output_backend) &&
        reader.field(binding.safety_policy_sha256);
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

[[nodiscard]] std::string attestation_payload(
    const FixtureQualificationAttestation& attestation) {
    CanonicalWriter writer;
    writer.number(attestation.schema_version);
    writer.field(attestation.input_project_sha256);
    writer.field(attestation.candidate_project_sha256);
    writer.field(attestation.candidate_binding_sha256);
    writer.field(attestation.operator_id);
    writer.field(attestation.observed_at_utc);
    write_binding(writer, attestation.binding);
    writer.number(attestation.requirements.size());
    for (const auto& requirement : attestation.requirements) {
        writer.field(requirement.id);
        writer.number(requirement.kind);
        writer.number(requirement.absolute_channel);
        writer.number(requirement.value);
        writer.field(requirement.expected_behavior);
        writer.flag(requirement.require_no_spill);
        writer.field(requirement.raw_frame_sha256);
    }
    writer.number(attestation.observations.size());
    for (const auto& observation : attestation.observations) {
        writer.field(observation.requirement_id);
        writer.field(observation.raw_frame_sha256);
        writer.field(observation.observed_behavior);
        writer.flag(observation.passed);
        writer.flag(observation.no_spill_observed);
        writer.flag(observation.blackout_before);
        writer.flag(observation.blackout_after);
        writer.flag(observation.timed_out);
        writer.flag(observation.device_lost);
        writer.field(observation.failure);
    }
    writer.number(attestation.markers_to_supersede.size());
    for (const auto& marker : attestation.markers_to_supersede) {
        writer.field(marker);
    }
    return writer.value();
}

[[nodiscard]] bool read_attestation_payload(
    std::string_view payload,
    FixtureQualificationAttestation& attestation) {
    CanonicalReader reader(payload);
    std::size_t requirement_count = 0U;
    std::size_t observation_count = 0U;
    std::size_t marker_count = 0U;
    if (!reader.number(attestation.schema_version) ||
        !reader.field(attestation.input_project_sha256) ||
        !reader.field(attestation.candidate_project_sha256) ||
        !reader.field(attestation.candidate_binding_sha256) ||
        !reader.field(attestation.operator_id) ||
        !reader.field(attestation.observed_at_utc) ||
        !read_binding(reader, attestation.binding) ||
        !reader.number(requirement_count) ||
        requirement_count > kMaximumFixtureQualificationRequirements) {
        return false;
    }
    attestation.requirements.assign(requirement_count, {});
    for (auto& requirement : attestation.requirements) {
        std::uint8_t kind = 0U;
        if (!reader.field(requirement.id) || !reader.number(kind) ||
            kind > static_cast<std::uint8_t>(FixtureQualificationRequirementKind::OneHot) ||
            !reader.number(requirement.absolute_channel) ||
            !reader.number(requirement.value) ||
            !reader.field(requirement.expected_behavior) ||
            !reader.flag(requirement.require_no_spill) ||
            !reader.field(requirement.raw_frame_sha256)) {
            return false;
        }
        requirement.kind = static_cast<FixtureQualificationRequirementKind>(kind);
    }
    if (!reader.number(observation_count) ||
        observation_count > kMaximumFixtureQualificationRequirements) {
        return false;
    }
    attestation.observations.assign(observation_count, {});
    for (auto& observation : attestation.observations) {
        if (!reader.field(observation.requirement_id) ||
            !reader.field(observation.raw_frame_sha256) ||
            !reader.field(observation.observed_behavior) ||
            !reader.flag(observation.passed) ||
            !reader.flag(observation.no_spill_observed) ||
            !reader.flag(observation.blackout_before) ||
            !reader.flag(observation.blackout_after) ||
            !reader.flag(observation.timed_out) ||
            !reader.flag(observation.device_lost) ||
            !reader.field(observation.failure)) {
            return false;
        }
    }
    if (!reader.number(marker_count) || marker_count > 64U) {
        return false;
    }
    attestation.markers_to_supersede.assign(marker_count, {});
    for (auto& marker : attestation.markers_to_supersede) {
        if (!reader.field(marker)) {
            return false;
        }
    }
    return reader.finished();
}

[[nodiscard]] bool valid_qualification_text(
    std::string_view value,
    std::size_t maximum = kMaximumQualificationTextBytes) noexcept {
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

[[nodiscard]] bool physical_output_requested(const ProjectDocument& project) noexcept {
    return project.connections.artnet_enabled || project.connections.sacn_enabled ||
        project.connections.soundswitch_micro_universe != 0U ||
        project.connections.soundswitch_control_one_experimental ||
        std::any_of(
            project.connections.dmx_usb_pro_ports.begin(),
            project.connections.dmx_usb_pro_ports.end(),
            [](const auto& port) { return !port.empty(); });
}

[[nodiscard]] std::string marker_digest(std::string_view marker) {
    return sha256_text(marker);
}

[[nodiscard]] std::string supersession_record(
    std::string_view marker,
    std::string_view attestation_digest) {
    return std::string(kFixtureQualificationSupersessionRecord) + "\t1\t" +
        marker_digest(marker) + "\t" + std::string(attestation_digest);
}

[[nodiscard]] bool contains_record(
    const ProjectDocument& project,
    std::string_view record) noexcept {
    return std::find(project.unknown_records.begin(), project.unknown_records.end(), record) !=
        project.unknown_records.end();
}

[[nodiscard]] std::string advertised_attestation_digest(std::string_view record) {
    const auto fields = tab_fields(record);
    if (fields.size() >= 3U && fields[0] == kFixtureQualificationAttestationRecord &&
        is_sha256_digest(fields[2])) {
        return std::string(fields[2]);
    }
    return {};
}

struct SupersessionKey {
    std::string marker_sha256;
    std::string attestation_sha256;
};

[[nodiscard]] bool has_supersession(
    const std::vector<SupersessionKey>& supersessions,
    std::string_view marker_sha256,
    std::string_view attestation_sha256) noexcept {
    return std::any_of(
        supersessions.begin(), supersessions.end(),
        [&](const auto& item) {
            return item.marker_sha256 == marker_sha256 &&
                item.attestation_sha256 == attestation_sha256;
        });
}

[[nodiscard]] std::vector<const FixtureDefinition*> fixtures_affected_by_marker(
    const ProjectDocument& project,
    std::string_view marker) {
    std::vector<const FixtureDefinition*> affected;
    const auto fields = tab_fields(marker);
    for (const auto& fixture : project.fixtures) {
        if (std::find(fields.begin() + 1, fields.end(), fixture.id) != fields.end()) {
            affected.push_back(&fixture);
        }
    }
    if (affected.empty()) {
        for (const auto& fixture : project.fixtures) {
            affected.push_back(&fixture);
        }
    }
    return affected;
}

}  // namespace

ProjectDocument make_ir4_6ch_qualification_project() {
    auto project = make_starter_project();
    project.id = "emberlights-ir4-6ch-qualification";
    project.name = "IR-4 6CH SoundSwitch Micro Qualification";
    project.connections.os2l_enabled = false;
    project.connections.artnet_enabled = false;
    project.connections.sacn_enabled = false;
    project.connections.dmx_usb_pro_ports = {};
    project.connections.soundswitch_micro_universe = 1U;
    project.connections.soundswitch_micro_framing =
        showcore::SoundSwitchMicroFraming::NativeJls1;
    project.connections.soundswitch_control_one_experimental = false;
    project.connections.frame_rate = 40U;
    project.connections.midi_input_index = -1;
    project.connections.midi_output_index = -1;
    std::erase_if(
        project.fixture_profiles,
        [](const auto& profile) {
            return profile.id != kBothLightingIr4SixChannelProfileId;
        });
    project.fixtures.clear();
    project.groups.clear();
    project.color_palettes.clear();
    project.looks.clear();
    project.autoloops.clear();
    project.audio_assets.clear();
    project.track_scripts.clear();
    project.midi_mappings.clear();

    project.fixtures.push_back({
        "ir4-bench-001",
        "Both Lighting IR-4 Bench Fixture",
        std::string(kBothLightingIr4SixChannelProfileId),
        1U,
        1U,
        {"qualification", "uplight", "color"}});

    project.looks.reserve(kIr4SafeLooks.size());
    for (const auto& definition : kIr4SafeLooks) {
        project.looks.push_back(make_ir4_safe_look(definition));
    }
    return project;
}

ProjectDocument make_ir4_6ch_operator_bench_project() {
    auto project = make_ir4_6ch_qualification_project();
    project.id = "emberlights-ir4-6ch-operator-bench";
    project.name = "EmberLights IR-4 6CH Editable Bench";
    // Packaged operator assets must never begin emitting merely because they
    // were opened. Studio requires an explicit Connections Save & Apply to
    // SoundSwitch Micro universe 1 before this becomes an active bench.
    project.connections.soundswitch_micro_universe = 0U;
    if (project.fixture_profiles.size() == 1U && project.fixtures.size() == 1U) {
        auto& profile = project.fixture_profiles.front();
        profile.id = kIr4SixChannelOperatorBenchProfileId;
        profile.name =
            "Both Lighting BO-IR4 LED Mini Spotlight (6 Channel Operator Clone)";
        profile.source = showcore::FixtureProfileSource::Local;
        profile.source_revision =
            "local-clone:" + std::string(kBothLightingBoIr4ManualRevision);
        project.fixtures.front().profile_id = profile.id;
    }
    return project;
}

OneFixtureBenchContract inspect_one_fixture_bench_contract(
    const ProjectDocument& project) noexcept {
    OneFixtureBenchContract result;
    result.exactly_one_fixture = project.fixtures.size() == 1U;
    result.exactly_one_profile = project.fixture_profiles.size() == 1U;
    if (result.exactly_one_fixture) {
        const auto& fixture = project.fixtures.front();
        result.fixture_on_selected_output = fixture.universe == 1U;
        result.fixture_profile_present = std::any_of(
            project.fixture_profiles.begin(), project.fixture_profiles.end(),
            [&](const auto& profile) {
                return profile.id == fixture.profile_id &&
                    !profile.manufacturer.empty() && !profile.model.empty() &&
                    !profile.mode.empty() && !profile.source_revision.empty() &&
                    profile.source != showcore::FixtureProfileSource::Unknown;
            });
    }
    result.soundswitch_micro_universe_one_only =
        project.connections.soundswitch_micro_universe == 1U &&
        project.connections.soundswitch_micro_framing ==
            showcore::SoundSwitchMicroFraming::NativeJls1 &&
        !project.connections.artnet_enabled && !project.connections.sacn_enabled &&
        !project.connections.soundswitch_control_one_experimental &&
        std::all_of(
            project.connections.dmx_usb_pro_ports.begin(),
            project.connections.dmx_usb_pro_ports.end(),
            [](const auto& port) { return port.empty(); });
    result.os2l_disabled = !project.connections.os2l_enabled;
    result.midi_disabled = project.midi_mappings.empty() &&
        project.connections.midi_input_index < 0 &&
        project.connections.midi_output_index < 0;
    result.automation_disabled =
        project.autoloops.empty() && project.track_scripts.empty();
    result.isolated_content = project.groups.empty() &&
        project.color_palettes.empty() && project.audio_assets.empty() &&
        !project.looks.empty();
    return result;
}

FrameComparison compare_dmx_frames(
    const showcore::DmxUniverse& expected,
    const showcore::DmxUniverse& actual) noexcept {
    FrameComparison result;
    for (std::size_t index = 0U; index < expected.size(); ++index) {
        if (expected[index] == actual[index]) {
            continue;
        }
        if (result.differing_slots == 0U) {
            result.first_differing_channel = static_cast<std::uint16_t>(index + 1U);
            result.expected = expected[index];
            result.actual = actual[index];
        }
        result.differing_channels[result.differing_slots] = {
            static_cast<std::uint16_t>(index + 1U),
            expected[index],
            actual[index]};
        ++result.differing_slots;
    }
    return result;
}

PacketComparison compare_soundswitch_micro_packets(
    const showcore::SoundSwitchMicroPacket& expected,
    const showcore::SoundSwitchMicroPacket& actual) noexcept {
    PacketComparison result;
    result.expected_length = expected.length;
    result.actual_length = actual.length;
    result.expected_length_valid = expected.length <= expected.bytes.size();
    result.actual_length_valid = actual.length <= actual.bytes.size();
    const auto expected_length = std::min(expected.length, expected.bytes.size());
    const auto actual_length = std::min(actual.length, actual.bytes.size());
    const auto maximum = std::max(expected_length, actual_length);
    for (std::size_t index = 0U; index < maximum; ++index) {
        const auto expected_present = index < expected_length;
        const auto actual_present = index < actual_length;
        const auto expected_byte = static_cast<std::uint8_t>(
            expected_present ? expected.bytes[index] : 0U);
        const auto actual_byte = static_cast<std::uint8_t>(
            actual_present ? actual.bytes[index] : 0U);
        if (expected_byte == actual_byte && expected_present == actual_present) {
            continue;
        }
        if (result.differing_bytes == 0U) {
            result.first_differing_offset = index;
            result.expected = expected_byte;
            result.actual = actual_byte;
        }
        result.differing_byte_rows[result.differing_bytes] = {
            index,
            expected_byte,
            actual_byte,
            expected_present,
            actual_present};
        ++result.differing_bytes;
    }
    return result;
}

FixtureBenchQualificationSet build_fixture_bench_qualifications(
    const ProjectDocument& project,
    std::span<const FixtureBenchLookExpectation> expectations) {
    FixtureBenchQualificationSet set;
    if (expectations.empty() || expectations.size() > kMaximumStaticLooks) {
        return set;
    }
    set.looks.reserve(expectations.size());

    const auto contract = inspect_one_fixture_bench_contract(project);
    const auto project_validation = validate_project(project);
    auto compilation = compile_project(project);

    for (const auto& expectation : expectations) {
        FixtureBenchQualificationFrames result;
        result.look_id = expectation.look_id;
        result.expected_universe = expectation.universe;
        result.validation = project_validation;
        result.raw_reference = expectation.expected_frame;

        if (!contract.exact()) {
            result.error = FixtureBenchQualificationError::InvalidBenchContract;
            set.looks.push_back(std::move(result));
            continue;
        }
        if (expectation.universe == 0U ||
            expectation.universe > showcore::kV1UniverseCount) {
            result.error = FixtureBenchQualificationError::InvalidExpectedUniverse;
            set.looks.push_back(std::move(result));
            continue;
        }
        result.raw_packet = showcore::build_soundswitch_micro_packet(
            result.raw_reference,
            showcore::SoundSwitchMicroFraming::NativeJls1);
        if (!project_validation.ok()) {
            result.error = FixtureBenchQualificationError::InvalidProject;
            set.looks.push_back(std::move(result));
            continue;
        }
        if (!compilation) {
            result.validation = compilation.validation;
            result.error = FixtureBenchQualificationError::CompilationFailed;
            set.looks.push_back(std::move(result));
            continue;
        }

        const auto source_look = std::find_if(
            project.looks.begin(), project.looks.end(),
            [&](const auto& look) { return look.id == expectation.look_id; });
        if (source_look == project.looks.end()) {
            result.error = FixtureBenchQualificationError::MissingLook;
            set.looks.push_back(std::move(result));
            continue;
        }
        const auto look_index = static_cast<std::size_t>(
            std::distance(project.looks.begin(), source_look));
        const auto* look = compilation.show->look(look_index);
        if (look == nullptr) {
            result.error = FixtureBenchQualificationError::MissingLook;
            set.looks.push_back(std::move(result));
            continue;
        }

        showcore::LayerBuffer layer;
        const auto look_result = showcore::compile_static_look(*look, layer);
        if (!look_result) {
            result.error = FixtureBenchQualificationError::LookCompilationFailed;
            set.looks.push_back(std::move(result));
            continue;
        }
        auto& engine = compilation.show->engine();
        engine.layers().replace_layer(showcore::LayerId::EventMoment, layer);
        engine.tick();
        result.runner_frames = engine.frames();
        result.runner_rendered =
            result.runner_frames.universes[expectation.universe - 1U];
        for (std::size_t universe = 0U;
             universe < result.runner_frames.universes.size(); ++universe) {
            if (universe == expectation.universe - 1U) {
                continue;
            }
            result.unrelated_output_nonzero_slots += static_cast<std::size_t>(
                std::count_if(
                    result.runner_frames.universes[universe].begin(),
                    result.runner_frames.universes[universe].end(),
                    [](std::uint8_t value) { return value != 0U; }));
        }
        result.runner_packet = showcore::build_soundswitch_micro_packet(
            result.runner_rendered,
            showcore::SoundSwitchMicroFraming::NativeJls1);
        result.frame_comparison = compare_dmx_frames(
            result.raw_reference, result.runner_rendered);
        result.packet_comparison = compare_soundswitch_micro_packets(
            result.raw_packet, result.runner_packet);
        if (!result.frame_comparison.exact()) {
            result.error = FixtureBenchQualificationError::FrameMismatch;
        } else if (!result.packet_comparison.exact()) {
            result.error = FixtureBenchQualificationError::PacketMismatch;
        } else if (result.unrelated_output_nonzero_slots != 0U) {
            result.error = FixtureBenchQualificationError::UnexpectedOutput;
        }
        set.looks.push_back(std::move(result));
    }
    return set;
}

std::string_view ir4_6ch_safe_look_id(
    Ir4SixChannelSafeLook look) noexcept {
    const auto* definition = ir4_safe_look_definition(look);
    return definition == nullptr ? std::string_view{} : definition->id;
}

showcore::DmxUniverse ir4_6ch_safe_look_expected_frame(
    Ir4SixChannelSafeLook look) noexcept {
    showcore::DmxUniverse frame{};
    const auto* definition = ir4_safe_look_definition(look);
    if (definition != nullptr && definition->active_channel != 0U) {
        frame[definition->active_channel - 1U] = 255U;
    }
    return frame;
}

FixtureBenchQualificationSet build_ir4_6ch_safe_qualifications() {
    std::array<FixtureBenchLookExpectation, kIr4SixChannelSafeLookCount>
        expectations{};
    for (std::size_t index = 0U; index < expectations.size(); ++index) {
        const auto look = static_cast<Ir4SixChannelSafeLook>(index);
        expectations[index] = {
            ir4_6ch_safe_look_id(look),
            1U,
            ir4_6ch_safe_look_expected_frame(look)};
    }
    return build_fixture_bench_qualifications(
        make_ir4_6ch_qualification_project(), expectations);
}

Ir4QualificationFrames build_ir4_6ch_red_qualification() {
    auto set = build_ir4_6ch_safe_qualifications();
    const auto red_id = ir4_6ch_safe_look_id(Ir4SixChannelSafeLook::Red);
    const auto found = std::find_if(
        set.looks.begin(), set.looks.end(),
        [&](const auto& look) { return look.look_id == red_id; });
    if (found != set.looks.end()) {
        return std::move(*found);
    }
    Ir4QualificationFrames result;
    result.look_id = red_id;
    result.error = FixtureBenchQualificationError::MissingLook;
    return result;
}

std::string fixture_qualification_project_basis_sha256(
    const ProjectDocument& project) {
    auto basis = project;
    std::erase_if(
        basis.unknown_records,
        [](const auto& record) { return is_qualification_artifact(record); });
    return sha256_text(serialize_project(basis));
}

std::string fixture_qualification_safety_policy_sha256(
    const SafetySettings& safety) {
    CanonicalWriter writer;
    writer.flag(safety.fog_requires_arm);
    writer.flag(safety.haze_requires_arm);
    writer.flag(safety.laser_requires_arm);
    writer.flag(safety.spark_requires_arm);
    writer.flag(safety.strobe_allowed);
    writer.number(std::bit_cast<std::uint32_t>(safety.max_strobe));
    writer.number(std::bit_cast<std::uint32_t>(safety.max_intensity));
    return sha256_text(writer.value());
}

std::string fixture_qualification_binding_sha256(
    const FixtureQualificationBinding& binding) {
    CanonicalWriter writer;
    write_binding(writer, binding);
    return sha256_text(writer.value());
}

std::string fixture_qualification_expected_frame_sha256(
    const FixtureQualificationRequirement& requirement) {
    showcore::DmxUniverse frame{};
    if (requirement.kind == FixtureQualificationRequirementKind::Blackout) {
        if (requirement.absolute_channel != 0U || requirement.value != 0U) {
            return {};
        }
    } else if (requirement.kind == FixtureQualificationRequirementKind::OneHot) {
        if (requirement.absolute_channel == 0U ||
            requirement.absolute_channel > showcore::kUniverseSlots ||
            requirement.value == 0U) {
            return {};
        }
        frame[requirement.absolute_channel - 1U] = requirement.value;
    } else {
        return {};
    }
    return sha256_bytes(frame);
}

std::vector<std::string> fixture_qualification_enabled_backends(
    const ProjectDocument& project,
    std::string_view fixture_id) {
    std::vector<std::string> backends;
    const auto* fixture = find_fixture(project, fixture_id);
    if (fixture == nullptr || fixture->universe == 0U ||
        fixture->universe > showcore::kV1UniverseCount) {
        return backends;
    }
    const auto universe = std::to_string(fixture->universe);
    if (project.connections.artnet_enabled) {
        backends.push_back("artnet:u" + universe);
    }
    if (project.connections.sacn_enabled) {
        backends.push_back("sacn:u" + universe);
    }
    if (!project.connections.dmx_usb_pro_ports[fixture->universe - 1U].empty()) {
        backends.push_back("dmx-usb-pro:u" + universe);
    }
    if (project.connections.soundswitch_micro_universe == fixture->universe) {
        backends.push_back("soundswitch-micro:u" + universe);
    }
    if (project.connections.soundswitch_control_one_experimental) {
        backends.push_back("soundswitch-control-one:u" + universe);
    }
    return backends;
}

FixtureQualificationCheck make_fixture_qualification_binding(
    const ProjectDocument& project,
    std::string_view fixture_id,
    std::string_view unit_label,
    std::string_view output_backend,
    FixtureQualificationBinding& binding) {
    const auto* fixture = find_fixture(project, fixture_id);
    if (fixture == nullptr) {
        return qualification_check(
            FixtureQualificationStatus::InvalidBinding,
            std::string(fixture_id),
            "The qualification fixture is not present in the current project.");
    }
    const auto* profile = find_profile(project, fixture->profile_id);
    if (profile == nullptr) {
        return qualification_check(
            FixtureQualificationStatus::InvalidBinding,
            fixture->id,
            "The qualification fixture references a missing profile.");
    }
    if (!valid_qualification_text(unit_label, 255U) ||
        !valid_qualification_text(output_backend, 128U)) {
        return qualification_check(
            FixtureQualificationStatus::InvalidBinding,
            fixture->id,
            "A bounded unit label and output backend are required.");
    }
    const auto enabled = fixture_qualification_enabled_backends(project, fixture->id);
    if (std::find(enabled.begin(), enabled.end(), output_backend) == enabled.end()) {
        return qualification_check(
            FixtureQualificationStatus::InvalidBinding,
            fixture->id,
            "The attested output backend is not enabled for this fixture universe.");
    }
    binding.fixture_id = fixture->id;
    binding.unit_label = unit_label;
    binding.manufacturer = profile->manufacturer;
    binding.model = profile->model;
    binding.mode = profile->mode;
    binding.profile_id = profile->id;
    binding.profile_revision = profile->source_revision;
    binding.behavior_fingerprint = fixture_profile_behavior_fingerprint(*profile);
    binding.universe = fixture->universe;
    binding.address = fixture->address;
    binding.output_backend = output_backend;
    binding.safety_policy_sha256 =
        fixture_qualification_safety_policy_sha256(project.safety);
    return {};
}

FixtureQualificationCheck seal_fixture_qualification_attestation(
    const ProjectDocument& project,
    FixtureQualificationAttestation& attestation) {
    const auto candidate_project_sha256 =
        fixture_qualification_project_basis_sha256(project);
    if (attestation.input_project_sha256.empty()) {
        attestation.input_project_sha256 = candidate_project_sha256;
    }
    attestation.candidate_project_sha256 = candidate_project_sha256;
    attestation.candidate_binding_sha256 =
        fixture_qualification_binding_sha256(attestation.binding);
    attestation.content_sha256 = sha256_text(attestation_payload(attestation));
    return validate_fixture_qualification_attestation(project, attestation);
}

FixtureQualificationCheck validate_fixture_qualification_attestation(
    const ProjectDocument& project,
    const FixtureQualificationAttestation& attestation) {
    if (attestation.schema_version != kFixtureQualificationAttestationVersion) {
        return qualification_check(
            FixtureQualificationStatus::InvalidSchema,
            attestation.binding.fixture_id,
            "The fixture qualification attestation schema is unsupported.");
    }
    if (!is_sha256_digest(attestation.input_project_sha256)) {
        return qualification_check(
            FixtureQualificationStatus::InvalidProjectBasis,
            attestation.binding.fixture_id,
            "The active input project identity is missing or malformed.");
    }
    if (!is_sha256_digest(attestation.candidate_project_sha256) ||
        attestation.candidate_project_sha256 !=
            fixture_qualification_project_basis_sha256(project)) {
        return qualification_check(
            FixtureQualificationStatus::InvalidProjectBasis,
            attestation.binding.fixture_id,
            "The evidence was captured for a different or stale project basis.");
    }
    if (!valid_qualification_text(attestation.operator_id, 255U) ||
        !valid_utc_timestamp(attestation.observed_at_utc)) {
        return qualification_check(
            FixtureQualificationStatus::InvalidSchema,
            attestation.binding.fixture_id,
            "Operator identity and a UTC observation timestamp are required.");
    }

    FixtureQualificationBinding current;
    const auto binding_result = make_fixture_qualification_binding(
        project,
        attestation.binding.fixture_id,
        attestation.binding.unit_label,
        attestation.binding.output_backend,
        current);
    if (!binding_result.ok() || !same_binding(current, attestation.binding) ||
        !is_sha256_digest(attestation.candidate_binding_sha256) ||
        attestation.candidate_binding_sha256 !=
            fixture_qualification_binding_sha256(attestation.binding)) {
        return qualification_check(
            FixtureQualificationStatus::InvalidBinding,
            attestation.binding.fixture_id,
            "Fixture, profile, mode, patch, backend, safety, or behavior binding is stale.");
    }

    const auto* fixture = find_fixture(project, attestation.binding.fixture_id);
    const auto* profile = fixture == nullptr ? nullptr : find_profile(project, fixture->profile_id);
    if (fixture == nullptr || profile == nullptr || profile->footprint == 0U ||
        profile->footprint >= kMaximumFixtureQualificationRequirements ||
        attestation.requirements.size() !=
            static_cast<std::size_t>(profile->footprint) + 1U) {
        return qualification_check(
            FixtureQualificationStatus::InvalidRequirements,
            attestation.binding.fixture_id,
            "Evidence must contain one blackout and one raw one-hot test for every fixture slot.");
    }

    std::unordered_set<std::string_view> requirement_ids;
    std::vector<bool> covered(profile->footprint, false);
    std::size_t blackout_count = 0U;
    for (const auto& requirement : attestation.requirements) {
        if (!valid_qualification_text(requirement.id, 255U) ||
            !requirement_ids.insert(requirement.id).second ||
            !valid_qualification_text(requirement.expected_behavior, 1024U) ||
            !requirement.require_no_spill ||
            !is_sha256_digest(requirement.raw_frame_sha256) ||
            requirement.raw_frame_sha256 !=
                fixture_qualification_expected_frame_sha256(requirement)) {
            return qualification_check(
                FixtureQualificationStatus::InvalidRequirements,
                attestation.binding.fixture_id,
                "A qualification requirement is duplicate, unbounded, or does not match its raw frame.");
        }
        if (requirement.kind == FixtureQualificationRequirementKind::Blackout) {
            ++blackout_count;
            continue;
        }
        const auto first = fixture->address;
        const auto end = static_cast<std::uint32_t>(fixture->address) + profile->footprint;
        if (requirement.kind != FixtureQualificationRequirementKind::OneHot ||
            requirement.absolute_channel < first ||
            requirement.absolute_channel >= end) {
            return qualification_check(
                FixtureQualificationStatus::InvalidRequirements,
                attestation.binding.fixture_id,
                "A raw one-hot channel falls outside the exact fixture footprint.");
        }
        const auto offset = requirement.absolute_channel - first;
        if (covered[offset]) {
            return qualification_check(
                FixtureQualificationStatus::InvalidRequirements,
                attestation.binding.fixture_id,
                "The same fixture slot is tested more than once.");
        }
        covered[offset] = true;
    }
    if (blackout_count != 1U ||
        std::find(covered.begin(), covered.end(), false) != covered.end()) {
        return qualification_check(
            FixtureQualificationStatus::InvalidRequirements,
            attestation.binding.fixture_id,
            "Blackout and complete fixture-footprint coverage are mandatory.");
    }

    if (attestation.observations.size() != attestation.requirements.size()) {
        return qualification_check(
            FixtureQualificationStatus::InvalidObservation,
            attestation.binding.fixture_id,
            "Every qualification requirement needs exactly one observation.");
    }
    std::unordered_map<std::string_view, const FixtureQualificationRequirement*> requirements;
    for (const auto& requirement : attestation.requirements) {
        requirements.emplace(requirement.id, &requirement);
    }
    std::unordered_set<std::string_view> observed;
    for (const auto& observation : attestation.observations) {
        const auto requirement = requirements.find(observation.requirement_id);
        if (requirement == requirements.end() ||
            !observed.insert(observation.requirement_id).second ||
            observation.raw_frame_sha256 != requirement->second->raw_frame_sha256 ||
            !valid_qualification_text(observation.observed_behavior, 2048U) ||
            !observation.passed || !observation.no_spill_observed ||
            !observation.blackout_before || !observation.blackout_after ||
            observation.timed_out || observation.device_lost ||
            !observation.failure.empty()) {
            return qualification_check(
                FixtureQualificationStatus::InvalidObservation,
                attestation.binding.fixture_id,
                "Observed behavior, no-spill, bounded blackout, timeout, device, or failure evidence is incomplete.");
        }
    }

    if (attestation.markers_to_supersede.empty()) {
        return qualification_check(
            FixtureQualificationStatus::MarkerMissing,
            attestation.binding.fixture_id,
            "An exact migration or invalidation marker must be named for graduation.");
    }
    std::unordered_set<std::string_view> marker_set;
    for (const auto& marker : attestation.markers_to_supersede) {
        if (!is_qualification_gate_marker(marker) ||
            !marker_set.insert(marker).second || !contains_record(project, marker)) {
            return qualification_check(
                FixtureQualificationStatus::MarkerMissing,
                attestation.binding.fixture_id,
                "A named marker is missing, duplicate, or is not a qualification gate marker.");
        }
    }

    if (!is_sha256_digest(attestation.content_sha256) ||
        attestation.content_sha256 != sha256_text(attestation_payload(attestation))) {
        return qualification_check(
            FixtureQualificationStatus::InvalidDigest,
            attestation.binding.fixture_id,
            "The immutable attestation content digest does not match its payload.");
    }
    return {};
}

std::string serialize_fixture_qualification_attestation_record(
    const FixtureQualificationAttestation& attestation) {
    const auto payload = attestation_payload(attestation);
    return std::string(kFixtureQualificationAttestationRecord) + "\t1\t" +
        attestation.content_sha256 + "\t" + hexadecimal(payload);
}

FixtureQualificationCheck parse_fixture_qualification_attestation_record(
    std::string_view record,
    FixtureQualificationAttestation& attestation) {
    const auto fields = tab_fields(record);
    if (fields.size() != 4U || fields[0] != kFixtureQualificationAttestationRecord ||
        fields[1] != "1" || !is_sha256_digest(fields[2])) {
        return qualification_check(
            FixtureQualificationStatus::InvalidSchema,
            {},
            "The embedded fixture qualification record envelope is malformed.");
    }
    std::string payload;
    if (!decode_hexadecimal(fields[3], payload) || sha256_text(payload) != fields[2]) {
        return qualification_check(
            FixtureQualificationStatus::InvalidDigest,
            {},
            "The embedded fixture qualification payload is malformed or tampered.");
    }
    FixtureQualificationAttestation parsed;
    if (!read_attestation_payload(payload, parsed)) {
        return qualification_check(
            FixtureQualificationStatus::InvalidSchema,
            {},
            "The embedded fixture qualification payload cannot be decoded.");
    }
    parsed.content_sha256 = fields[2];
    attestation = std::move(parsed);
    return {};
}

FixtureQualificationCheck graduate_fixture_qualification(
    ProjectDocument& project,
    const FixtureQualificationAttestation& attestation) {
    const auto validation = validate_fixture_qualification_attestation(project, attestation);
    if (!validation.ok()) {
        return validation;
    }
    const auto record = serialize_fixture_qualification_attestation_record(attestation);
    if (contains_record(project, record) ||
        std::any_of(
            project.unknown_records.begin(), project.unknown_records.end(),
            [&](const auto& candidate) {
                return advertised_attestation_digest(candidate) == attestation.content_sha256;
            })) {
        return qualification_check(
            FixtureQualificationStatus::Replay,
            attestation.binding.fixture_id,
            "This attestation digest is already embedded in the project.");
    }
    auto candidate = project;
    candidate.unknown_records.push_back(record);
    for (const auto& marker : attestation.markers_to_supersede) {
        if (!contains_record(project, marker)) {
            return qualification_check(
                FixtureQualificationStatus::MarkerMissing,
                attestation.binding.fixture_id,
                "The project changed before its exact marker could be superseded.");
        }
        candidate.unknown_records.push_back(
            supersession_record(marker, attestation.content_sha256));
    }
    project = std::move(candidate);
    return {};
}

FixtureQualificationCheck revoke_fixture_qualification(
    ProjectDocument& project,
    std::string_view attestation_sha256,
    std::string_view reason) {
    if (!is_sha256_digest(attestation_sha256) ||
        !valid_qualification_text(reason, 1024U)) {
        return qualification_check(
            FixtureQualificationStatus::InvalidDigest,
            std::string(attestation_sha256),
            "Revocation requires an embedded attestation digest and a bounded reason.");
    }
    const auto found = std::any_of(
        project.unknown_records.begin(), project.unknown_records.end(),
        [&](const auto& record) {
            return advertised_attestation_digest(record) == attestation_sha256;
        });
    if (!found) {
        return qualification_check(
            FixtureQualificationStatus::EvidenceMissing,
            std::string(attestation_sha256),
            "The attestation digest is not embedded in this project.");
    }
    for (const auto& record : project.unknown_records) {
        const auto fields = tab_fields(record);
        if (fields.size() >= 3U && fields[0] == kFixtureQualificationRevocationRecord &&
            fields[2] == attestation_sha256) {
            return qualification_check(
                FixtureQualificationStatus::Replay,
                std::string(attestation_sha256),
                "The attestation is already revoked.");
        }
    }
    project.unknown_records.push_back(
        std::string(kFixtureQualificationRevocationRecord) + "\t1\t" +
        std::string(attestation_sha256) + "\t" + sha256_text(reason) + "\t" +
        hexadecimal(reason));
    return {};
}

FixtureQualificationGate evaluate_fixture_qualification_gate(
    const ProjectDocument& project) {
    FixtureQualificationGate gate;
    gate.physical_output_requested = physical_output_requested(project);

    std::unordered_set<std::string> raw_attempt_digests;
    for (const auto& record : project.unknown_records) {
        if (!record_starts_with(record, kRawHardwareTestAttemptRecord)) {
            continue;
        }
        RawHardwareTestAttempt attempt;
        const auto parsed = parse_raw_hardware_test_attempt_record(record, attempt);
        auto resealed = attempt;
        const auto original_digest = attempt.content_sha256;
        const auto integrity = parsed.ok()
            ? seal_raw_hardware_test_attempt(resealed)
            : parsed;
        if (!integrity.ok() || resealed.content_sha256 != original_digest) {
            gate.issues.push_back({
                "qualification.auditInvalid",
                project.id,
                parsed.ok()
                    ? "A Raw Hardware Test audit record is internally contradictory or tampered."
                    : parsed.message});
            continue;
        }
        if (!raw_attempt_digests.insert(original_digest).second) {
            gate.issues.push_back({
                "qualification.evidenceContradictory",
                original_digest,
                "The same Raw Hardware Test attempt appears more than once in the append-only audit."});
        }
    }

    std::vector<std::string_view> markers;
    for (const auto& record : project.unknown_records) {
        if (is_qualification_gate_marker(record)) {
            markers.push_back(record);
        }
    }

    std::unordered_set<std::string> revoked;
    for (const auto& record : project.unknown_records) {
        if (!record_starts_with(record, kFixtureQualificationRevocationRecord)) {
            continue;
        }
        const auto fields = tab_fields(record);
        std::string reason;
        if (fields.size() != 5U || fields[1] != "1" ||
            !is_sha256_digest(fields[2]) || !is_sha256_digest(fields[3]) ||
            !decode_hexadecimal(fields[4], reason) ||
            !valid_qualification_text(reason, 1024U) ||
            sha256_text(reason) != fields[3]) {
            gate.issues.push_back({
                "qualification.evidenceInvalid",
                project.id,
                "A qualification revocation record is malformed or tampered."});
            continue;
        }
        if (!revoked.emplace(fields[2]).second) {
            gate.issues.push_back({
                "qualification.evidenceContradictory",
                std::string(fields[2]),
                "The same qualification attestation was revoked more than once."});
        }
    }

    std::vector<SupersessionKey> supersessions;
    for (const auto& record : project.unknown_records) {
        if (!record_starts_with(record, kFixtureQualificationSupersessionRecord)) {
            continue;
        }
        const auto fields = tab_fields(record);
        if (fields.size() != 4U || fields[1] != "1" ||
            !is_sha256_digest(fields[2]) || !is_sha256_digest(fields[3])) {
            gate.issues.push_back({
                "qualification.evidenceInvalid",
                project.id,
                "A qualification marker supersession is malformed or tampered."});
            continue;
        }
        const SupersessionKey supersession{std::string(fields[2]), std::string(fields[3])};
        if (has_supersession(
                supersessions,
                supersession.marker_sha256,
                supersession.attestation_sha256)) {
            gate.issues.push_back({
                "qualification.evidenceContradictory",
                project.id,
                "The same qualification marker supersession was embedded more than once."});
            continue;
        }
        supersessions.push_back(supersession);
    }

    std::vector<FixtureQualificationAttestation> attestations;
    std::unordered_set<std::string> active_digests;
    std::unordered_set<std::string> embedded_digests;
    for (const auto& record : project.unknown_records) {
        if (!record_starts_with(record, kFixtureQualificationAttestationRecord)) {
            continue;
        }
        FixtureQualificationAttestation attestation;
        const auto parsed = parse_fixture_qualification_attestation_record(record, attestation);
        if (!parsed.ok()) {
            gate.issues.push_back({
                "qualification.evidenceInvalid",
                project.id,
                parsed.message});
            continue;
        }
        embedded_digests.emplace(attestation.content_sha256);
        if (revoked.contains(attestation.content_sha256)) {
            continue;
        }
        const auto validation = validate_fixture_qualification_attestation(project, attestation);
        if (!validation.ok()) {
            gate.issues.push_back({
                "qualification.evidenceInvalid",
                validation.subject,
                validation.message});
            continue;
        }
        if (!active_digests.insert(attestation.content_sha256).second) {
            gate.issues.push_back({
                "qualification.evidenceContradictory",
                attestation.binding.fixture_id,
                "The same active qualification attestation was embedded more than once."});
            continue;
        }
        attestations.push_back(std::move(attestation));
    }

    std::unordered_set<std::string> marker_digests;
    for (const auto marker : markers) {
        marker_digests.emplace(marker_digest(marker));
    }
    for (const auto& supersession : supersessions) {
        if (!marker_digests.contains(supersession.marker_sha256) ||
            !embedded_digests.contains(supersession.attestation_sha256)) {
            gate.issues.push_back({
                "qualification.evidenceInvalid",
                project.id,
                "A qualification supersession references a missing marker or attestation."});
        }
    }
    for (const auto& digest : revoked) {
        if (!embedded_digests.contains(digest)) {
            gate.issues.push_back({
                "qualification.evidenceInvalid",
                digest,
                "A qualification revocation references a missing attestation."});
        }
    }

    if (!gate.physical_output_requested && !markers.empty()) {
        gate.issues.push_back({
            "qualification.evidenceRequired",
            project.id,
            "Physical output remains disabled while migration or fixture qualification evidence is pending."});
    }

    if (gate.physical_output_requested) {
        for (const auto marker : markers) {
            const auto affected = fixtures_affected_by_marker(project, marker);
            const auto marker_sha256 = marker_digest(marker);
            for (const auto* fixture : affected) {
                for (const auto& backend :
                     fixture_qualification_enabled_backends(project, fixture->id)) {
                    std::size_t matches = 0U;
                    for (const auto& attestation : attestations) {
                        if (attestation.binding.fixture_id != fixture->id ||
                            attestation.binding.output_backend != backend ||
                            std::find(
                                attestation.markers_to_supersede.begin(),
                                attestation.markers_to_supersede.end(),
                                marker) == attestation.markers_to_supersede.end() ||
                            !has_supersession(
                                supersessions,
                                marker_sha256,
                                attestation.content_sha256)) {
                            continue;
                        }
                        ++matches;
                    }
                    if (matches == 0U) {
                        gate.issues.push_back({
                            "qualification.evidenceRequired",
                            fixture->id,
                            "Physical output through " + backend +
                                " is blocked until this exact fixture unit and marker are qualified."});
                    } else if (matches > 1U) {
                        gate.issues.push_back({
                            "qualification.evidenceContradictory",
                            fixture->id,
                            "Multiple active attestations claim the same fixture/backend/marker; revoke stale evidence and requalify."});
                    }
                }
            }
        }
    }

    gate.allowed = !gate.physical_output_requested || gate.issues.empty();
    return gate;
}

MicroPhysicalQualificationResult evaluate_micro_physical_qualification(
    const MicroPhysicalQualificationEvidence& evidence) noexcept {
    if (!evidence.software_frame_match) {
        return MicroPhysicalQualificationResult::SoftwareFrameMismatch;
    }
    if (!evidence.initial_open_succeeded) {
        return MicroPhysicalQualificationResult::InitialOpenFailed;
    }
    if (!evidence.raw_writes_succeeded) {
        return MicroPhysicalQualificationResult::RawWriteFailed;
    }
    if (!evidence.raw_visible_red_and_blackout) {
        return MicroPhysicalQualificationResult::RawObservationFailed;
    }
    if (!evidence.repeat_open_succeeded) {
        return MicroPhysicalQualificationResult::RepeatOpenFailed;
    }
    if (!evidence.runner_writes_succeeded) {
        return MicroPhysicalQualificationResult::RunnerWriteFailed;
    }
    if (!evidence.runner_visible_match_and_blackout) {
        return MicroPhysicalQualificationResult::RunnerObservationFailed;
    }
    if (!evidence.bounded_blackouts_succeeded) {
        return MicroPhysicalQualificationResult::BlackoutFailed;
    }
    if (!evidence.disconnect_observed) {
        return MicroPhysicalQualificationResult::DisconnectNotObserved;
    }
    if (!evidence.reconnect_detected) {
        return MicroPhysicalQualificationResult::ReconnectNotDetected;
    }
    if (!evidence.reconnect_open_succeeded) {
        return MicroPhysicalQualificationResult::ReconnectOpenFailed;
    }
    if (!evidence.reconnect_writes_succeeded) {
        return MicroPhysicalQualificationResult::ReconnectWriteFailed;
    }
    if (!evidence.reconnect_visible_red_and_blackout) {
        return MicroPhysicalQualificationResult::ReconnectObservationFailed;
    }
    if (!evidence.reconnect_blackout_succeeded) {
        return MicroPhysicalQualificationResult::ReconnectBlackoutFailed;
    }
    return MicroPhysicalQualificationResult::Passed;
}

const char* micro_physical_qualification_result_name(
    MicroPhysicalQualificationResult result) noexcept {
    switch (result) {
    case MicroPhysicalQualificationResult::Passed:
        return "passed";
    case MicroPhysicalQualificationResult::SoftwareFrameMismatch:
        return "software-frame-mismatch";
    case MicroPhysicalQualificationResult::InitialOpenFailed:
        return "initial-open-failed";
    case MicroPhysicalQualificationResult::RawWriteFailed:
        return "raw-write-failed";
    case MicroPhysicalQualificationResult::RawObservationFailed:
        return "raw-observation-failed";
    case MicroPhysicalQualificationResult::RepeatOpenFailed:
        return "repeat-open-failed";
    case MicroPhysicalQualificationResult::RunnerWriteFailed:
        return "runner-write-failed";
    case MicroPhysicalQualificationResult::RunnerObservationFailed:
        return "runner-observation-failed";
    case MicroPhysicalQualificationResult::BlackoutFailed:
        return "blackout-failed";
    case MicroPhysicalQualificationResult::DisconnectNotObserved:
        return "disconnect-not-observed";
    case MicroPhysicalQualificationResult::ReconnectNotDetected:
        return "reconnect-not-detected";
    case MicroPhysicalQualificationResult::ReconnectOpenFailed:
        return "reconnect-open-failed";
    case MicroPhysicalQualificationResult::ReconnectWriteFailed:
        return "reconnect-write-failed";
    case MicroPhysicalQualificationResult::ReconnectObservationFailed:
        return "reconnect-observation-failed";
    case MicroPhysicalQualificationResult::ReconnectBlackoutFailed:
        return "reconnect-blackout-failed";
    }
    return "unknown";
}

}  // namespace emberlights
