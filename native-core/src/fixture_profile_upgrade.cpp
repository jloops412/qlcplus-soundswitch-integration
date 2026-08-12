#include "emberlights/fixture_profile_upgrade.hpp"

#include "emberlights/file_identity.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string_view>

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
