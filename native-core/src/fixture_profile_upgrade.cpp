#include "emberlights/fixture_profile_upgrade.hpp"

#include "emberlights/compiler.hpp"
#include "emberlights/file_identity.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace emberlights {
namespace {

using showcore::ChannelEncoding;
using showcore::FixtureProfileSource;
using showcore::Property;

[[nodiscard]] ChannelDefinition channel(Property property, std::uint16_t offset) {
    return {
        property,
        offset,
        -1,
        ChannelEncoding::Linear8,
        0U,
        255U,
        0U};
}

[[nodiscard]] ChannelDefinition ranged_channel(
    Property property,
    std::uint16_t offset,
    std::uint8_t minimum,
    std::uint8_t maximum,
    std::uint8_t safe_default) {
    return {
        property,
        offset,
        -1,
        ChannelEncoding::Ranged8,
        minimum,
        maximum,
        safe_default};
}

[[nodiscard]] ChannelDefinition constant_channel(
    std::uint16_t offset,
    std::uint8_t safe_default) {
    return {
        Property::Count,
        offset,
        -1,
        ChannelEncoding::Constant8,
        0U,
        255U,
        safe_default};
}

[[nodiscard]] FixtureProfileDefinition make_ir4_profile(
    std::string id,
    std::string mode,
    std::initializer_list<Property> properties) {
    FixtureProfileDefinition profile;
    profile.id = std::move(id);
    profile.manufacturer = "Both Lighting";
    profile.model = "BO-IR4 LED Mini Spotlight";
    profile.mode = std::move(mode);
    profile.name = "Both Lighting BO-IR4 LED Mini Spotlight (" + profile.mode + ")";
    profile.source = FixtureProfileSource::BuiltIn;
    profile.source_revision = std::string(kBothLightingBoIr4ManualRevision);
    profile.footprint = static_cast<std::uint16_t>(properties.size());
    std::uint16_t offset = 0U;
    for (const auto property : properties) {
        profile.channels.push_back(channel(property, offset++));
    }
    return profile;
}

[[nodiscard]] bool exact_old_channel(
    const ChannelDefinition& actual,
    Property property,
    std::uint16_t offset) noexcept {
    return actual.property == property &&
        actual.coarse_offset == offset &&
        actual.fine_offset == -1 &&
        actual.encoding == ChannelEncoding::Linear8 &&
        actual.dmx_min == 0U &&
        actual.dmx_max == 255U &&
        actual.default_value == 0U;
}

[[nodiscard]] bool is_exact_stale_ir4_profile(
    const FixtureProfileDefinition& profile) noexcept {
    constexpr std::array<Property, 10U> stale_properties{{
        Property::Intensity,
        Property::Red,
        Property::Green,
        Property::Blue,
        Property::Amber,
        Property::White,
        Property::UV,
        Property::Strobe,
        Property::Custom1,
        Property::Custom2}};
    if (profile.id != "soundswitch.both-lighting.bo-ir4.mode1" ||
        profile.manufacturer != "Both Lighting" ||
        profile.model != "BO-IR4 LED Mini Spotlight" ||
        profile.mode != "Mode 1" ||
        profile.name != "Both Lighting BO-IR4 LED Mini Spotlight (Mode 1)" ||
        profile.source != FixtureProfileSource::Local ||
        profile.source_revision != "soundswitch-2.10.3-safe-v1" ||
        profile.footprint != 10U ||
        profile.channels.size() != stale_properties.size()) {
        return false;
    }
    for (std::size_t index = 0U; index < stale_properties.size(); ++index) {
        if (!exact_old_channel(
                profile.channels[index],
                stale_properties[index],
                static_cast<std::uint16_t>(index))) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool same_channel_definition(
    const ChannelDefinition& first,
    const ChannelDefinition& second) noexcept {
    return first.property == second.property &&
        first.coarse_offset == second.coarse_offset &&
        first.fine_offset == second.fine_offset &&
        first.encoding == second.encoding &&
        first.dmx_min == second.dmx_min &&
        first.dmx_max == second.dmx_max &&
        first.default_value == second.default_value;
}

[[nodiscard]] bool same_profile_definition(
    const FixtureProfileDefinition& first,
    const FixtureProfileDefinition& second) noexcept {
    return first.id == second.id &&
        first.manufacturer == second.manufacturer &&
        first.model == second.model &&
        first.mode == second.mode &&
        first.name == second.name &&
        first.source == second.source &&
        first.source_revision == second.source_revision &&
        first.footprint == second.footprint &&
        first.channels.size() == second.channels.size() &&
        std::equal(
            first.channels.begin(),
            first.channels.end(),
            second.channels.begin(),
            same_channel_definition);
}

[[nodiscard]] showcore::ProfileResult validate_profile_definition(
    const FixtureProfileDefinition& profile) {
    std::vector<showcore::ChannelMapping> channels;
    channels.reserve(profile.channels.size());
    for (const auto& definition : profile.channels) {
        channels.push_back({
            definition.property,
            definition.coarse_offset,
            definition.fine_offset,
            definition.encoding,
            definition.dmx_min,
            definition.dmx_max,
            definition.default_value});
    }
    const showcore::FixtureProfile runtime{
        profile.name.c_str(),
        channels.data(),
        channels.size(),
        profile.footprint};
    return showcore::validate_fixture_profile(runtime);
}

[[nodiscard]] std::string profile_error_text(showcore::ProfileError error) {
    using showcore::ProfileError;
    switch (error) {
    case ProfileError::None: return "valid";
    case ProfileError::MissingName: return "missing profile name";
    case ProfileError::MissingChannels: return "no channel mappings";
    case ProfileError::InvalidFootprint: return "invalid footprint";
    case ProfileError::InvalidProperty: return "invalid channel property";
    case ProfileError::ConstantHasProperty: return "constant channel has a semantic property";
    case ProfileError::OffsetOutsideFootprint: return "channel offset is outside the footprint";
    case ProfileError::FineOffsetRequired: return "16-bit channel is missing its fine channel";
    case ProfileError::FineOffsetNotAllowed: return "8-bit channel unexpectedly has a fine channel";
    case ProfileError::DuplicateOffset: return "two mappings claim the same channel";
    case ProfileError::DefaultOutOfRange: return "default DMX value is outside the supported range";
    }
    return "unknown profile error";
}

[[nodiscard]] std::string humanize_identifier(std::string_view value) {
    std::string output;
    output.reserve(value.size() + 4U);
    for (std::size_t index = 0U; index < value.size(); ++index) {
        const auto character = static_cast<unsigned char>(value[index]);
        if (index != 0U && std::isupper(character) != 0 &&
            std::islower(static_cast<unsigned char>(value[index - 1U])) != 0) {
            output.push_back(' ');
        }
        output.push_back(static_cast<char>(
            index == 0U ? std::toupper(character) : character));
    }
    return output;
}

[[nodiscard]] std::string encoding_display_name(showcore::ChannelEncoding encoding) {
    using showcore::ChannelEncoding;
    switch (encoding) {
    case ChannelEncoding::Linear8: return "Linear 8-bit";
    case ChannelEncoding::Linear16: return "Linear 16-bit";
    case ChannelEncoding::Discrete8: return "Discrete 8-bit";
    case ChannelEncoding::Ranged8: return "Ranged 8-bit";
    case ChannelEncoding::Constant8: return "Safe constant";
    }
    return "Unknown encoding";
}

[[nodiscard]] FixtureProfileWhiteAmberCorrectionError correction_error_for(
    const FixtureProfileMappingSummary& summary,
    showcore::FixtureProfileSource source) noexcept {
    if (summary.white_mapping_count == 0U) {
        return FixtureProfileWhiteAmberCorrectionError::MissingWhite;
    }
    if (summary.white_mapping_count > 1U) {
        return FixtureProfileWhiteAmberCorrectionError::AmbiguousWhite;
    }
    if (summary.amber_mapping_count == 0U) {
        return FixtureProfileWhiteAmberCorrectionError::MissingAmber;
    }
    if (summary.amber_mapping_count > 1U) {
        return FixtureProfileWhiteAmberCorrectionError::AmbiguousAmber;
    }
    if (!summary.profile_valid) {
        return FixtureProfileWhiteAmberCorrectionError::InvalidProfile;
    }
    if (source != showcore::FixtureProfileSource::Local) {
        return FixtureProfileWhiteAmberCorrectionError::NotUserOwned;
    }
    return FixtureProfileWhiteAmberCorrectionError::None;
}

[[nodiscard]] std::string correction_message(
    FixtureProfileWhiteAmberCorrectionError error,
    const FixtureProfileMappingSummary& summary) {
    using Error = FixtureProfileWhiteAmberCorrectionError;
    switch (error) {
    case Error::None:
        return "Mapping is valid. White is CH" + std::to_string(summary.white_channel) +
            " and Amber is CH" + std::to_string(summary.amber_channel) +
            ". This Local profile can swap those two labels without changing any other channel.";
    case Error::MissingWhite:
        return "White/Amber correction is unavailable because this profile has no White mapping.";
    case Error::MissingAmber:
        return "White/Amber correction is unavailable because this profile has no Amber mapping.";
    case Error::AmbiguousWhite:
        return "White/Amber correction is unavailable because more than one mapping is labeled White.";
    case Error::AmbiguousAmber:
        return "White/Amber correction is unavailable because more than one mapping is labeled Amber.";
    case Error::InvalidProfile:
        return "White/Amber correction is unavailable until the profile validation error is fixed.";
    case Error::NotUserOwned:
        return "This source profile is read-only. Duplicate it as a Local profile before correcting White/Amber.";
    }
    return "White/Amber correction is unavailable.";
}

[[nodiscard]] bool canonical_ir4_definition_is_expected(
    const FixtureProfileDefinition& profile,
    bool ten_channel) {
    if (profile.source != FixtureProfileSource::BuiltIn ||
        profile.source_revision != kBothLightingBoIr4ManualRevision ||
        !validate_profile_definition(profile)) {
        return false;
    }
    constexpr std::array<Property, 6U> emitters{{
        Property::Red,
        Property::Green,
        Property::Blue,
        Property::White,
        Property::Amber,
        Property::UV}};
    const auto first_emitter = ten_channel ? 1U : 0U;
    const auto expected_footprint = ten_channel ? 10U : 6U;
    if (profile.footprint != expected_footprint ||
        profile.channels.size() != expected_footprint) {
        return false;
    }
    if (ten_channel && !exact_old_channel(
            profile.channels[0], Property::Intensity, 0U)) {
        return false;
    }
    for (std::size_t index = 0U; index < emitters.size(); ++index) {
        if (!exact_old_channel(
                profile.channels[first_emitter + index],
                emitters[index],
                static_cast<std::uint16_t>(first_emitter + index))) {
            return false;
        }
    }
    if (!ten_channel) {
        return true;
    }
    const auto& strobe = profile.channels[7];
    if (strobe.property != Property::Strobe ||
        strobe.coarse_offset != 7U ||
        strobe.fine_offset != -1 ||
        strobe.encoding != ChannelEncoding::Ranged8 ||
        strobe.dmx_min != 1U ||
        strobe.dmx_max != 255U ||
        strobe.default_value != 0U) {
        return false;
    }
    for (std::size_t index = 8U; index < 10U; ++index) {
        const auto& definition = profile.channels[index];
        if (definition.property != Property::Count ||
            definition.coarse_offset != index ||
            definition.fine_offset != -1 ||
            definition.encoding != ChannelEncoding::Constant8 ||
            definition.default_value != 0U) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::vector<std::string> fixture_ids_referencing(
    const ProjectDocument& project,
    std::string_view profile_id) {
    std::vector<std::string> ids;
    for (const auto& fixture : project.fixtures) {
        if (fixture.profile_id == profile_id) {
            ids.push_back(fixture.id);
        }
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

[[nodiscard]] bool reviewed_fixture_ids_match(
    const ProjectDocument& project,
    const FixtureProfileUpgradeChange& change) {
    auto reviewed = change.affected_fixture_ids;
    std::sort(reviewed.begin(), reviewed.end());
    if (std::adjacent_find(reviewed.begin(), reviewed.end()) != reviewed.end()) {
        return false;
    }
    return reviewed == fixture_ids_referencing(project, change.source_profile_id);
}

[[nodiscard]] bool profile_slot_is_compatible(
    const ProjectDocument& project,
    const FixtureProfileDefinition& expected) noexcept {
    std::size_t matches = 0U;
    for (const auto& profile : project.fixture_profiles) {
        if (profile.id != expected.id) {
            continue;
        }
        ++matches;
        if (!same_profile_definition(profile, expected)) {
            return false;
        }
    }
    return matches <= 1U;
}

[[nodiscard]] bool has_profile_id(
    const ProjectDocument& project,
    std::string_view id) noexcept {
    return std::any_of(
        project.fixture_profiles.begin(),
        project.fixture_profiles.end(),
        [id](const auto& profile) { return profile.id == id; });
}

[[nodiscard]] std::string fixture_profile_snapshot_fingerprint(
    const FixtureProfileDefinition& profile) {
    std::ostringstream canonical;
    const auto text_field = [&canonical](std::string_view value) {
        canonical << value.size() << ':' << value << ';';
    };
    text_field(profile.id);
    text_field(profile.manufacturer);
    text_field(profile.model);
    text_field(profile.mode);
    text_field(profile.name);
    canonical << static_cast<unsigned int>(profile.source) << ';';
    text_field(profile.source_revision);
    canonical << profile.footprint << ';';
    for (const auto& definition : profile.channels) {
        canonical << static_cast<unsigned int>(definition.property) << ','
                  << definition.coarse_offset << ','
                  << definition.fine_offset << ','
                  << static_cast<unsigned int>(definition.encoding) << ','
                  << static_cast<unsigned int>(definition.dmx_min) << ','
                  << static_cast<unsigned int>(definition.dmx_max) << ','
                  << definition.default_value << ';';
    }
    return "sha256:" + sha256_text(canonical.str());
}

[[nodiscard]] std::string fit_suffix(
    std::string_view base,
    std::string_view suffix) {
    const auto maximum = showcore::kFixtureProfileTextLength;
    if (suffix.size() >= maximum) {
        return std::string(suffix.substr(suffix.size() - maximum));
    }
    return std::string(base.substr(0U, std::min(base.size(), maximum - suffix.size()))) +
        std::string(suffix);
}

[[nodiscard]] std::string make_unique_white_amber_profile_id(
    const ProjectDocument& project,
    std::string_view source_profile_id) {
    for (std::size_t ordinal = 1U;
         ordinal <= showcore::kMaxCompiledFixtureProfiles + 1U;
         ++ordinal) {
        const auto suffix = ordinal == 1U
            ? std::string(".wa-corrected")
            : std::string(".wa-corrected-") + std::to_string(ordinal);
        const auto candidate = fit_suffix(source_profile_id, suffix);
        if (!has_profile_id(project, candidate)) {
            return candidate;
        }
    }
    return {};
}

[[nodiscard]] std::string corrected_profile_name(std::string_view source_name) {
    return fit_suffix(source_name, " (Physical W/A corrected)");
}

[[nodiscard]] std::string corrected_source_revision(
    const FixtureProfileDefinition& source) {
    constexpr std::string_view prefix = "white-amber-correction-";
    const auto digest = fixture_profile_snapshot_fingerprint(source);
    const auto digest_text = digest.starts_with("sha256:")
        ? std::string_view(digest).substr(7U)
        : std::string_view(digest);
    return std::string(prefix) + std::string(digest_text.substr(0U, 16U));
}

[[nodiscard]] std::vector<FixtureProfileWhiteAmberRebindTarget>
white_amber_rebind_targets(
    const ProjectDocument& project,
    std::string_view profile_id) {
    std::vector<FixtureProfileWhiteAmberRebindTarget> targets;
    for (const auto& fixture : project.fixtures) {
        if (fixture.profile_id != profile_id) {
            continue;
        }
        targets.push_back({
            fixture.id,
            fixture.name,
            fixture.universe,
            fixture.address});
    }
    std::sort(
        targets.begin(), targets.end(),
        [](const auto& first, const auto& second) {
            return first.fixture_id < second.fixture_id;
        });
    return targets;
}

[[nodiscard]] bool same_rebind_targets(
    const std::vector<FixtureProfileWhiteAmberRebindTarget>& first,
    const std::vector<FixtureProfileWhiteAmberRebindTarget>& second) noexcept {
    if (first.size() != second.size()) {
        return false;
    }
    for (std::size_t index = 0U; index < first.size(); ++index) {
        if (first[index].fixture_id != second[index].fixture_id ||
            first[index].fixture_name != second[index].fixture_name ||
            first[index].universe != second[index].universe ||
            first[index].address != second[index].address) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::string rebind_target_fingerprint(
    const std::vector<FixtureProfileWhiteAmberRebindTarget>& targets) {
    std::ostringstream canonical;
    for (const auto& target : targets) {
        canonical << target.fixture_id.size() << ':' << target.fixture_id << ';'
                  << target.fixture_name.size() << ':' << target.fixture_name << ';'
                  << static_cast<unsigned int>(target.universe) << ';'
                  << target.address << ';';
    }
    return "sha256:" + sha256_text(canonical.str());
}

[[nodiscard]] FixtureProfileDefinition corrected_profile_from_plan(
    const FixtureProfileDefinition& source,
    const FixtureProfileWhiteAmberCorrectionPlan& plan,
    FixtureProfileWhiteAmberCorrectionResult& correction) {
    auto corrected = source;
    if (plan.creates_local_copy) {
        corrected.id = plan.replacement_profile_id;
        corrected.name = plan.replacement_profile_name;
        corrected.source = FixtureProfileSource::Local;
        corrected.source_revision = plan.replacement_source_revision;
    }
    correction = correct_fixture_profile_white_amber(corrected);
    return corrected;
}

[[nodiscard]] std::string first_project_error_message(
    const ProjectValidation& validation) {
    const auto issue = std::find_if(
        validation.issues.begin(), validation.issues.end(),
        [](const auto& candidate) {
            return candidate.severity == ProjectIssueSeverity::Error;
        });
    return issue == validation.issues.end()
        ? std::string("The project did not pass validation.")
        : issue->code + ": " + issue->message;
}

[[nodiscard]] std::string json_escape(std::string_view value) {
    std::ostringstream output;
    for (const auto character : value) {
        switch (character) {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (static_cast<unsigned char>(character) < 0x20U) {
                output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                       << static_cast<unsigned int>(static_cast<unsigned char>(character));
            } else {
                output << character;
            }
        }
    }
    return output.str();
}

[[nodiscard]] bool has_unknown_record(
    const ProjectDocument& project,
    std::string_view prefix) {
    return std::any_of(
        project.unknown_records.begin(),
        project.unknown_records.end(),
        [prefix](const auto& record) { return record.starts_with(prefix); });
}

}  // namespace

FixtureProfileDefinition make_both_lighting_bo_ir4_6ch_profile() {
    // The manual calls channel 6 "Purple" in the DMX table and UV elsewhere.
    // Keep the native semantic as UV; the source-name ambiguity remains in the
    // revision/evidence report until the physical emitter is observed.
    return make_ir4_profile(
        std::string(kBothLightingBoIr4SixChannelProfileId),
        "6 Channel (manual-matched; CH6 Purple/UV)",
        {Property::Red, Property::Green, Property::Blue, Property::White,
         Property::Amber, Property::UV});
}

FixtureProfileDefinition make_both_lighting_bo_ir4_10ch_profile() {
    FixtureProfileDefinition profile;
    profile.id = std::string(kBothLightingBoIr4TenChannelProfileId);
    profile.manufacturer = "Both Lighting";
    profile.model = "BO-IR4 LED Mini Spotlight";
    profile.mode = "10 Channel (manual-matched; CH7 Purple/UV; CH9-10 quarantined)";
    profile.name = "Both Lighting BO-IR4 (10CH manual-safe)";
    profile.source = FixtureProfileSource::BuiltIn;
    profile.source_revision = std::string(kBothLightingBoIr4ManualRevision);
    profile.footprint = 10U;
    profile.channels = {
        channel(Property::Intensity, 0U),
        channel(Property::Red, 1U),
        channel(Property::Green, 2U),
        channel(Property::Blue, 3U),
        channel(Property::White, 4U),
        channel(Property::Amber, 5U),
        channel(Property::UV, 6U),
        // The manual identifies all 0-255 values as strobe but does not name
        // a separate open range. Zero remains the safe/default byte and any
        // positive semantic value enters 1-255.
        ranged_channel(Property::Strobe, 7U, 1U, 255U, 0U),
        // CH9 is a compound off/preset/program/sound selector and CH10 changes
        // color/speed within that mode. The current one-property schema cannot
        // represent those ranges honestly. Hold both at zero and do not expose
        // them as generic sliders until the range model and hardware bench are
        // qualified.
        constant_channel(8U, 0U),
        constant_channel(9U, 0U)};
    return profile;
}

Ir4ProfileAvailabilityResult ensure_manual_backed_both_lighting_bo_ir4_profiles(
    ProjectDocument& project) {
    Ir4ProfileAvailabilityResult result;
    const auto six_channel = make_both_lighting_bo_ir4_6ch_profile();
    const auto ten_channel = make_both_lighting_bo_ir4_10ch_profile();
    if (!canonical_ir4_definition_is_expected(six_channel, false) ||
        !canonical_ir4_definition_is_expected(ten_channel, true)) {
        result.error = Ir4ProfileAvailabilityError::CanonicalDefinitionInvalid;
        result.message = "The bundled IR-4 definitions do not match the reviewed manual contract.";
        return result;
    }

    const auto inspect_slot = [&](const FixtureProfileDefinition& expected) {
        std::size_t matches = 0U;
        bool exact = true;
        for (const auto& profile : project.fixture_profiles) {
            if (profile.id != expected.id) {
                continue;
            }
            ++matches;
            exact = exact && same_profile_definition(profile, expected);
        }
        return std::pair{matches, exact};
    };
    const auto [six_matches, six_exact] = inspect_slot(six_channel);
    if (six_matches > 1U || !six_exact) {
        result.error = Ir4ProfileAvailabilityError::ConflictingSixChannelId;
        result.message = "The canonical IR-4 6CH ID is occupied by different or duplicate profile data; nothing was changed.";
        return result;
    }
    const auto [ten_matches, ten_exact] = inspect_slot(ten_channel);
    if (ten_matches > 1U || !ten_exact) {
        result.error = Ir4ProfileAvailabilityError::ConflictingTenChannelId;
        result.message = "The canonical IR-4 10CH ID is occupied by different or duplicate profile data; nothing was changed.";
        return result;
    }

    const auto profiles_needed = static_cast<std::size_t>(six_matches == 0U) +
        static_cast<std::size_t>(ten_matches == 0U);
    if (project.fixture_profiles.size() + profiles_needed >
        showcore::kMaxCompiledFixtureProfiles) {
        result.error = Ir4ProfileAvailabilityError::ProfileCapacity;
        result.message = "The project has no remaining fixture-profile capacity; nothing was changed.";
        return result;
    }

    auto candidate = project.fixture_profiles;
    if (six_matches == 0U) {
        candidate.push_back(six_channel);
        result.six_channel_added = true;
    }
    if (ten_matches == 0U) {
        candidate.push_back(ten_channel);
        result.ten_channel_added = true;
    }
    project.fixture_profiles = std::move(candidate);
    result.message = result.six_channel_added || result.ten_channel_added
        ? "The manual-backed IR-4 6CH and 10CH profiles are available. Physical fixture mode and output qualification remain separate."
        : "The exact manual-backed IR-4 6CH and 10CH profiles were already available.";
    return result;
}

FixtureProfileMappingSummary summarize_fixture_profile_mapping(
    const FixtureProfileDefinition& profile) {
    FixtureProfileMappingSummary summary;
    const auto validation = validate_profile_definition(profile);
    summary.profile_valid = static_cast<bool>(validation);
    summary.profile_error = validation.error;
    summary.profile_error_mapping_index = validation.mapping_index;
    summary.channels.reserve(profile.channels.size());

    for (std::size_t index = 0U; index < profile.channels.size(); ++index) {
        const auto& definition = profile.channels[index];
        FixtureChannelMappingSummary channel;
        channel.source_index = index;
        channel.coarse_channel = static_cast<std::uint16_t>(
            static_cast<std::size_t>(definition.coarse_offset) + 1U);
        channel.fine_channel = definition.fine_offset < 0
            ? 0U
            : static_cast<std::uint16_t>(
                static_cast<std::size_t>(definition.fine_offset) + 1U);
        channel.property = definition.property;
        channel.encoding = definition.encoding;
        channel.dmx_min = definition.dmx_min;
        channel.dmx_max = definition.dmx_max;
        channel.default_value = definition.default_value;

        std::ostringstream line;
        line << "CH" << channel.coarse_channel;
        if (channel.fine_channel != 0U) {
            line << "/CH" << channel.fine_channel;
        }
        line << " | "
             << (channel.property == Property::Count
                     ? std::string("Constant / unused")
                     : humanize_identifier(property_name(channel.property)))
             << " | " << encoding_display_name(channel.encoding)
             << " | DMX " << static_cast<unsigned int>(channel.dmx_min)
             << '-' << static_cast<unsigned int>(channel.dmx_max)
             << " | default " << channel.default_value;
        channel.line = line.str();
        summary.channels.push_back(std::move(channel));

        if (definition.property == Property::White) {
            ++summary.white_mapping_count;
            summary.white_channel = static_cast<std::uint16_t>(
                static_cast<std::size_t>(definition.coarse_offset) + 1U);
        } else if (definition.property == Property::Amber) {
            ++summary.amber_mapping_count;
            summary.amber_channel = static_cast<std::uint16_t>(
                static_cast<std::size_t>(definition.coarse_offset) + 1U);
        }
    }
    if (summary.white_mapping_count != 1U) {
        summary.white_channel = 0U;
    }
    if (summary.amber_mapping_count != 1U) {
        summary.amber_channel = 0U;
    }
    std::stable_sort(
        summary.channels.begin(), summary.channels.end(),
        [](const auto& first, const auto& second) {
            return first.coarse_channel < second.coarse_channel;
        });

    summary.correction_error = correction_error_for(summary, profile.source);
    summary.validation_message = correction_message(
        summary.correction_error, summary);
    if (!summary.profile_valid) {
        summary.validation_message =
            "Profile validation failed at mapping " +
            std::to_string(summary.profile_error_mapping_index + 1U) + ": " +
            profile_error_text(summary.profile_error) + ". " +
            summary.validation_message;
    }

    std::ostringstream text;
    text << (profile.name.empty() ? "Unnamed fixture profile" : profile.name)
         << '\n'
         << "Mode: " << (profile.mode.empty() ? "(missing)" : profile.mode)
         << " | Footprint: " << profile.footprint << " channel"
         << (profile.footprint == 1U ? "" : "s") << '\n'
         << summary.validation_message << '\n';
    for (const auto& channel : summary.channels) {
        text << channel.line << '\n';
    }
    summary.text = text.str();
    return summary;
}

FixtureProfileWhiteAmberCorrectionResult correct_fixture_profile_white_amber(
    FixtureProfileDefinition& profile) {
    FixtureProfileWhiteAmberCorrectionResult result;
    const auto summary = summarize_fixture_profile_mapping(profile);
    result.error = summary.correction_error;
    result.white_channel_before = summary.white_channel;
    result.amber_channel_before = summary.amber_channel;
    result.before_behavior_fingerprint = fixture_profile_behavior_fingerprint(profile);
    if (!summary.can_correct_white_amber()) {
        result.message = summary.validation_message;
        return result;
    }

    const auto white = std::find_if(
        profile.channels.begin(), profile.channels.end(),
        [](const auto& definition) {
            return definition.property == Property::White;
        });
    const auto amber = std::find_if(
        profile.channels.begin(), profile.channels.end(),
        [](const auto& definition) {
            return definition.property == Property::Amber;
        });
    if (white == profile.channels.end() || amber == profile.channels.end()) {
        // The structured summary already proved these iterators exist. Keep a
        // defensive fail-closed boundary if that implementation ever changes.
        result.error = FixtureProfileWhiteAmberCorrectionError::InvalidProfile;
        result.message = "The profile changed while White/Amber correction was being prepared; nothing was changed.";
        return result;
    }

    auto candidate = profile;
    const auto white_index = static_cast<std::size_t>(
        std::distance(profile.channels.begin(), white));
    const auto amber_index = static_cast<std::size_t>(
        std::distance(profile.channels.begin(), amber));
    std::swap(
        candidate.channels[white_index].property,
        candidate.channels[amber_index].property);
    const auto corrected = summarize_fixture_profile_mapping(candidate);
    if (!corrected.profile_valid ||
        corrected.white_mapping_count != 1U ||
        corrected.amber_mapping_count != 1U) {
        result.error = FixtureProfileWhiteAmberCorrectionError::InvalidProfile;
        result.message = "The corrected profile candidate failed validation; nothing was changed.";
        return result;
    }

    result.white_channel_after = corrected.white_channel;
    result.amber_channel_after = corrected.amber_channel;
    result.after_behavior_fingerprint = fixture_profile_behavior_fingerprint(candidate);
    profile = std::move(candidate);
    result.error = FixtureProfileWhiteAmberCorrectionError::None;
    result.applied = true;
    result.message = "White and Amber labels were swapped. All offsets, ranges, defaults, and unrelated channels were preserved; hardware confirmation is still required.";
    return result;
}

FixtureProfileWhiteAmberCorrectionPlanResult
plan_fixture_profile_white_amber_correction(
    const ProjectDocument& project,
    std::string_view source_profile_id,
    FixtureProfileWhiteAmberProjectCorrectionMode mode) {
    using ProjectError = FixtureProfileWhiteAmberProjectCorrectionError;

    FixtureProfileWhiteAmberCorrectionPlanResult result;
    result.validation = validate_project(project);
    if (!result.validation.ok()) {
        result.error = ProjectError::InvalidProject;
        result.message =
            "White/Amber correction was not planned because the current project is invalid: " +
            first_project_error_message(result.validation);
        return result;
    }

    std::size_t source_matches = 0U;
    std::size_t source_index = 0U;
    for (std::size_t index = 0U; index < project.fixture_profiles.size(); ++index) {
        if (project.fixture_profiles[index].id != source_profile_id) {
            continue;
        }
        source_index = index;
        ++source_matches;
    }
    if (source_matches == 0U) {
        result.error = ProjectError::SourceProfileMissing;
        result.message = "The selected fixture profile no longer exists; nothing was changed.";
        return result;
    }
    if (source_matches != 1U) {
        result.error = ProjectError::SourceProfileAmbiguous;
        result.message = "More than one profile has the selected ID; repair the project before applying a physical channel correction.";
        return result;
    }

    const auto& source = project.fixture_profiles[source_index];
    bool creates_local_copy = false;
    switch (mode) {
    case FixtureProfileWhiteAmberProjectCorrectionMode::Auto:
        creates_local_copy = source.source != FixtureProfileSource::Local;
        break;
    case FixtureProfileWhiteAmberProjectCorrectionMode::CreateLocalCopy:
        creates_local_copy = true;
        break;
    case FixtureProfileWhiteAmberProjectCorrectionMode::UpdateLocalInPlace:
        if (source.source != FixtureProfileSource::Local) {
            result.error = ProjectError::SourceProfileReadOnly;
            result.profile_error =
                FixtureProfileWhiteAmberCorrectionError::NotUserOwned;
            result.message = "The selected source is immutable. Use Auto or Create Local Copy so the source snapshot stays preserved.";
            return result;
        }
        break;
    default:
        result.error = ProjectError::InvalidProject;
        result.message = "The requested White/Amber correction mode is invalid.";
        return result;
    }

    if (creates_local_copy) {
        std::size_t channel_count = source.channels.size();
        for (const auto& profile : project.fixture_profiles) {
            channel_count += profile.channels.size();
        }
        if (project.fixture_profiles.size() >=
                showcore::kMaxCompiledFixtureProfiles ||
            channel_count > showcore::kMaxCompiledChannelMappings) {
            result.error = ProjectError::ProfileCapacity;
            result.message = "There is no remaining compiled profile/channel capacity for a corrected Local copy; nothing was changed.";
            return result;
        }
    }

    auto& plan = result.plan;
    plan.source_profile_index = source_index;
    plan.source_profile_id = source.id;
    plan.source_snapshot_fingerprint =
        fixture_profile_snapshot_fingerprint(source);
    plan.creates_local_copy = creates_local_copy;
    plan.replacement_profile_id = creates_local_copy
        ? make_unique_white_amber_profile_id(project, source.id)
        : source.id;
    if (plan.replacement_profile_id.empty()) {
        result.error = ProjectError::ReplacementIdUnavailable;
        result.message = "A unique Local profile ID could not be generated; nothing was changed.";
        return result;
    }
    plan.replacement_profile_name = creates_local_copy
        ? corrected_profile_name(source.name)
        : source.name;
    plan.replacement_source_revision = creates_local_copy
        ? corrected_source_revision(source)
        : source.source_revision;
    plan.affected_fixtures = white_amber_rebind_targets(project, source.id);

    const auto before = summarize_fixture_profile_mapping(source);
    plan.before_mapping = before;
    plan.white_channel_before = before.white_channel;
    plan.amber_channel_before = before.amber_channel;
    plan.before_behavior_fingerprint =
        fixture_profile_behavior_fingerprint(source);

    FixtureProfileWhiteAmberCorrectionResult profile_correction;
    const auto corrected = corrected_profile_from_plan(
        source, plan, profile_correction);
    result.profile_error = profile_correction.error;
    if (!profile_correction.applied) {
        result.error = ProjectError::CorrectionUnavailable;
        result.message = profile_correction.message;
        return result;
    }
    plan.white_channel_after = profile_correction.white_channel_after;
    plan.amber_channel_after = profile_correction.amber_channel_after;
    plan.after_mapping = summarize_fixture_profile_mapping(corrected);
    plan.after_behavior_fingerprint =
        profile_correction.after_behavior_fingerprint;
    plan.replacement_snapshot_fingerprint =
        fixture_profile_snapshot_fingerprint(corrected);

    result.error = ProjectError::None;
    result.profile_error = FixtureProfileWhiteAmberCorrectionError::None;
    const auto fixture_count = plan.affected_fixtures.size();
    result.message =
        std::string(creates_local_copy
            ? "Ready to create a corrected Local snapshot and atomically rebind "
            : "Ready to correct the Local snapshot used by ") +
        std::to_string(fixture_count) + " patched fixture" +
        (fixture_count == 1U ? "." : "s.") +
        " White moves from CH" + std::to_string(plan.white_channel_before) +
        " to CH" + std::to_string(plan.white_channel_after) +
        "; Amber moves from CH" + std::to_string(plan.amber_channel_before) +
        " to CH" + std::to_string(plan.amber_channel_after) + ".";
    return result;
}

FixtureProfileWhiteAmberProjectCorrectionResult
apply_fixture_profile_white_amber_correction(
    ProjectDocument& project,
    const FixtureProfileWhiteAmberCorrectionPlan& plan) {
    using ProjectError = FixtureProfileWhiteAmberProjectCorrectionError;

    FixtureProfileWhiteAmberProjectCorrectionResult result;
    result.plan = plan;
    result.error = ProjectError::StalePlan;

    if (plan.source_profile_index >= project.fixture_profiles.size()) {
        result.message = "The fixture-profile list changed after review; nothing was changed.";
        return result;
    }
    const auto& source = project.fixture_profiles[plan.source_profile_index];
    if (source.id != plan.source_profile_id ||
        fixture_profile_snapshot_fingerprint(source) !=
            plan.source_snapshot_fingerprint) {
        result.message = "The source profile changed after review; nothing was changed.";
        return result;
    }
    const auto active_targets = white_amber_rebind_targets(
        project, plan.source_profile_id);
    if (!same_rebind_targets(active_targets, plan.affected_fixtures)) {
        result.message = "The fixtures using this profile changed after review; nothing was changed.";
        return result;
    }
    if (plan.creates_local_copy) {
        if (plan.replacement_profile_id == plan.source_profile_id ||
            has_profile_id(project, plan.replacement_profile_id)) {
            result.message = "The reviewed corrected-profile ID is no longer available; nothing was changed.";
            return result;
        }
        std::size_t channel_count = source.channels.size();
        for (const auto& profile : project.fixture_profiles) {
            channel_count += profile.channels.size();
        }
        if (project.fixture_profiles.size() >=
                showcore::kMaxCompiledFixtureProfiles ||
            channel_count > showcore::kMaxCompiledChannelMappings) {
            result.error = ProjectError::ProfileCapacity;
            result.message = "Profile capacity changed after review; nothing was changed.";
            return result;
        }
    } else if (source.source != FixtureProfileSource::Local ||
               plan.replacement_profile_id != source.id ||
               plan.replacement_profile_name != source.name ||
               plan.replacement_source_revision != source.source_revision) {
        result.message = "The reviewed in-place correction no longer matches a Local source; nothing was changed.";
        return result;
    }

    FixtureProfileWhiteAmberCorrectionResult profile_correction;
    auto corrected = corrected_profile_from_plan(
        source, plan, profile_correction);
    result.profile_error = profile_correction.error;
    if (!profile_correction.applied ||
        profile_correction.white_channel_before != plan.white_channel_before ||
        profile_correction.amber_channel_before != plan.amber_channel_before ||
        profile_correction.white_channel_after != plan.white_channel_after ||
        profile_correction.amber_channel_after != plan.amber_channel_after ||
        profile_correction.before_behavior_fingerprint !=
            plan.before_behavior_fingerprint ||
        profile_correction.after_behavior_fingerprint !=
            plan.after_behavior_fingerprint ||
        fixture_profile_snapshot_fingerprint(corrected) !=
            plan.replacement_snapshot_fingerprint) {
        result.message = "The corrected profile no longer matches the reviewed before/after mapping; nothing was changed.";
        return result;
    }

    auto candidate = project;
    if (plan.creates_local_copy) {
        candidate.fixture_profiles.push_back(corrected);
        std::size_t rebound = 0U;
        for (auto& fixture : candidate.fixtures) {
            if (fixture.profile_id != plan.source_profile_id) {
                continue;
            }
            fixture.profile_id = plan.replacement_profile_id;
            ++rebound;
        }
        if (rebound != plan.affected_fixtures.size()) {
            result.message = "The exact reviewed fixture set could not be rebound; nothing was changed.";
            return result;
        }
    } else {
        candidate.fixture_profiles[plan.source_profile_index] = corrected;
    }

    candidate.unknown_records.push_back(
        "FIXTURE_PROFILE_CORRECTION\twhite-amber-v1\t" +
        plan.source_snapshot_fingerprint + "\t" +
        plan.replacement_snapshot_fingerprint + "\t" +
        rebind_target_fingerprint(plan.affected_fixtures) + "\t" +
        std::to_string(plan.affected_fixtures.size()) + "\t" +
        (plan.creates_local_copy ? "local-copy" : "local-in-place") +
        "\thardware-confirmation-required");

    result.validation = validate_project(candidate);
    if (!result.validation.ok()) {
        result.error = ProjectError::CandidateValidationFailed;
        result.message =
            "The complete corrected project failed validation; nothing was changed: " +
            first_project_error_message(result.validation);
        return result;
    }
    const auto compilation = compile_project_with_persisted_autoloops(candidate);
    if (!compilation) {
        result.error = ProjectError::CandidateCompilationFailed;
        result.validation = compilation.validation;
        result.message =
            "The complete corrected project did not compile; nothing was changed: " +
            first_project_error_message(result.validation);
        return result;
    }

    project = std::move(candidate);
    result.error = ProjectError::None;
    result.profile_error = FixtureProfileWhiteAmberCorrectionError::None;
    result.applied = true;
    const auto fixture_count = plan.affected_fixtures.size();
    result.message =
        std::string(plan.creates_local_copy
            ? "Created the corrected Local profile and rebound "
            : "Corrected the active Local profile used by ") +
        std::to_string(fixture_count) + " fixture" +
        (fixture_count == 1U ? ". " : "s. ") +
        "White is now CH" + std::to_string(plan.white_channel_after) +
        " and Amber is now CH" + std::to_string(plan.amber_channel_after) +
        "; physical emitter confirmation is still required.";
    return result;
}

FixtureProfileWhiteAmberProjectCorrectionResult
correct_and_rebind_fixture_profile_white_amber(
    ProjectDocument& project,
    std::string_view source_profile_id,
    FixtureProfileWhiteAmberProjectCorrectionMode mode) {
    const auto planned = plan_fixture_profile_white_amber_correction(
        project, source_profile_id, mode);
    if (planned) {
        return apply_fixture_profile_white_amber_correction(
            project, planned.plan);
    }
    FixtureProfileWhiteAmberProjectCorrectionResult result;
    result.error = planned.error;
    result.profile_error = planned.profile_error;
    result.plan = planned.plan;
    result.validation = planned.validation;
    result.message = planned.message;
    return result;
}

std::string fixture_profile_behavior_fingerprint(
    const FixtureProfileDefinition& profile) {
    std::ostringstream canonical;
    const auto text_field = [&canonical](std::string_view value) {
        canonical << value.size() << ':' << value << ';';
    };
    text_field(profile.manufacturer);
    text_field(profile.model);
    text_field(profile.mode);
    canonical << profile.footprint << ';';
    for (const auto& definition : profile.channels) {
        canonical << static_cast<unsigned int>(definition.property) << ','
                  << definition.coarse_offset << ','
                  << definition.fine_offset << ','
                  << static_cast<unsigned int>(definition.encoding) << ','
                  << static_cast<unsigned int>(definition.dmx_min) << ','
                  << static_cast<unsigned int>(definition.dmx_max) << ','
                  << definition.default_value << ';';
    }
    return "sha256:" + sha256_text(canonical.str());
}

FixtureProfileUpgradePlan plan_known_fixture_profile_upgrades(
    const ProjectDocument& project) {
    FixtureProfileUpgradePlan plan;
    const auto replacement = make_both_lighting_bo_ir4_10ch_profile();
    for (std::size_t index = 0U; index < project.fixture_profiles.size(); ++index) {
        const auto& profile = project.fixture_profiles[index];
        if (!is_exact_stale_ir4_profile(profile)) {
            continue;
        }
        FixtureProfileUpgradeChange change;
        change.source_profile_index = index;
        change.source_profile_id = profile.id;
        change.replacement_profile_id = replacement.id;
        change.before_behavior_fingerprint = fixture_profile_behavior_fingerprint(profile);
        change.after_behavior_fingerprint = fixture_profile_behavior_fingerprint(replacement);
        for (const auto& fixture : project.fixtures) {
            if (fixture.profile_id == profile.id) {
                change.affected_fixture_ids.push_back(fixture.id);
            }
        }
        if (change.affected_fixture_ids.empty()) {
            continue;
        }
        plan.changes.push_back(std::move(change));
    }
    return plan;
}

FixtureProfileUpgradeResult apply_fixture_profile_upgrade_plan(
    ProjectDocument& project,
    const FixtureProfileUpgradePlan& plan) {
    FixtureProfileUpgradeResult result;
    if (plan.empty()) {
        result.message = "No exact known-bad fixture profile signature was found.";
        return result;
    }
    const auto replacement = make_both_lighting_bo_ir4_10ch_profile();
    const auto six_channel = make_both_lighting_bo_ir4_6ch_profile();
    if (plan.changes.size() != 1U) {
        result.message = "The reviewed fixture upgrade plan is not an exact single-profile repair.";
        return result;
    }
    if (!profile_slot_is_compatible(project, replacement) ||
        !profile_slot_is_compatible(project, six_channel)) {
        result.message = "A built-in IR-4 profile ID is already occupied by different data.";
        return result;
    }
    for (const auto& requested : plan.changes) {
        if (requested.upgrade != KnownFixtureProfileUpgrade::BothLightingBoIr4StaleTenChannel ||
            requested.replacement_profile_id != replacement.id ||
            requested.after_behavior_fingerprint !=
                fixture_profile_behavior_fingerprint(replacement)) {
            result.message = "The fixture upgrade plan does not name the reviewed IR-4 replacement.";
            return result;
        }
        if (requested.source_profile_index >= project.fixture_profiles.size()) {
            result.message = "The project changed after the fixture upgrade was reviewed.";
            return result;
        }
        const auto& source = project.fixture_profiles[requested.source_profile_index];
        if (!is_exact_stale_ir4_profile(source) || source.id != requested.source_profile_id ||
            fixture_profile_behavior_fingerprint(source) !=
                requested.before_behavior_fingerprint) {
            result.message = "The fixture profile no longer matches the reviewed upgrade plan.";
            return result;
        }
        if (!reviewed_fixture_ids_match(project, requested)) {
            result.message = "The fixtures referencing the stale profile changed after review.";
            return result;
        }
    }

    // Validate the complete transaction on a copy. The caller's document is
    // changed only after every reviewed precondition and native invariant holds.
    auto candidate = project;
    if (!has_profile_id(candidate, replacement.id)) {
        candidate.fixture_profiles.push_back(replacement);
    }
    if (!has_profile_id(candidate, six_channel.id)) {
        candidate.fixture_profiles.push_back(six_channel);
    }
    for (const auto& requested : plan.changes) {
        std::size_t rebound = 0U;
        for (auto& fixture : candidate.fixtures) {
            if (fixture.profile_id != requested.source_profile_id ||
                std::find(
                    requested.affected_fixture_ids.begin(),
                    requested.affected_fixture_ids.end(),
                    fixture.id) == requested.affected_fixture_ids.end()) {
                continue;
            }
            fixture.profile_id = requested.replacement_profile_id;
            ++rebound;
        }
        if (rebound != requested.affected_fixture_ids.size()) {
            result.message = "The exact reviewed fixture set could not be rebound.";
            return result;
        }
    }

    if (!has_unknown_record(candidate, "FIXTURE_PROFILE_UPGRADE\tbo-ir4-stale-10ch")) {
        const auto& change = plan.changes.front();
        candidate.unknown_records.push_back(
            "FIXTURE_PROFILE_UPGRADE\tbo-ir4-stale-10ch\t" +
            change.before_behavior_fingerprint + "\t" +
            change.after_behavior_fingerprint + "\tmanual-review-candidate");
    }
    if (!has_unknown_record(candidate, "MIGRATED_PATCH_UNVERIFIED")) {
        candidate.unknown_records.push_back(
            "MIGRATED_PATCH_UNVERIFIED\tfixture-mode-address-universe-review-required");
    }
    if (!has_unknown_record(candidate, "QUALIFICATION_INVALIDATED\tfixtureProfileUpgrade")) {
        candidate.unknown_records.push_back(
            "QUALIFICATION_INVALIDATED\tfixtureProfileUpgrade\tbo-ir4\t2026-08-11");
    }
    if (!validate_project(candidate).ok()) {
        result.message = "The upgraded project candidate did not pass native validation.";
        return result;
    }

    project = std::move(candidate);
    result.changes = plan.changes;
    result.applied = true;
    result.message = "Created a review candidate with the exact stale IR-4 profile repaired; "
        "the original profile remains preserved and physical mode confirmation is still required.";
    return result;
}

std::string serialize_fixture_profile_upgrade_report(
    const FixtureProfileUpgradeResult& result,
    std::string_view input_path,
    std::string_view output_path,
    std::string_view output_sha256,
    std::string_view input_sha256) {
    std::ostringstream output;
    output << "{\n"
           << "  \"format\": \"emberlights-fixture-profile-upgrade\",\n"
           << "  \"formatVersion\": 1,\n"
           << "  \"applied\": " << (result.applied ? "true" : "false") << ",\n"
           << "  \"inputProject\": \"" << json_escape(input_path) << "\",\n"
           << "  \"inputSha256\": \"" << json_escape(input_sha256) << "\",\n"
           << "  \"outputProject\": \"" << json_escape(output_path) << "\",\n"
           << "  \"outputSha256\": \"" << json_escape(output_sha256) << "\",\n"
           << "  \"message\": \"" << json_escape(result.message) << "\",\n"
           << "  \"physicalQualificationInvalidated\": "
           << (result.applied ? "true" : "false") << ",\n"
           << "  \"modeConfirmationRequired\": "
           << (result.applied ? "true" : "false") << ",\n"
           << "  \"changes\": [";
    for (std::size_t index = 0U; index < result.changes.size(); ++index) {
        const auto& change = result.changes[index];
        output << (index == 0U ? "\n" : ",\n")
               << "    {\"sourceProfileId\": \""
               << json_escape(change.source_profile_id)
               << "\", \"replacementProfileId\": \""
               << json_escape(change.replacement_profile_id)
               << "\", \"beforeBehaviorFingerprint\": \""
               << json_escape(change.before_behavior_fingerprint)
               << "\", \"afterBehaviorFingerprint\": \""
               << json_escape(change.after_behavior_fingerprint)
               << "\", \"affectedFixtureIds\": [";
        for (std::size_t fixture = 0U;
             fixture < change.affected_fixture_ids.size();
             ++fixture) {
            output << (fixture == 0U ? "" : ", ") << "\""
                   << json_escape(change.affected_fixture_ids[fixture]) << "\"";
        }
        output << "]}";
    }
    if (!result.changes.empty()) {
        output << '\n';
    }
    output << "  ],\n"
           << "  \"manualSource\": "
           << "\"Both Lighting IR-4 User Manual, PDF page 5, SHA-256 1267e289b2c0577ec749f0de5265105db5e86b6ae3b2e12414cc00777fd3c03a\",\n"
           << "  \"manualUrl\": "
           << "\"https://cdn.shopify.com/s/files/1/0716/8645/5572/files/BL_IR-4_BO-IR4.pdf?v=1679519527\"\n"
           << "}\n";
    return output.str();
}

}  // namespace emberlights
