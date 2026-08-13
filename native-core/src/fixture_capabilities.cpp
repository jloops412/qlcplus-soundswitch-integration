#include "emberlights/fixture_capabilities.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

namespace emberlights {
namespace {

[[nodiscard]] bool is_direct_emitter(showcore::Property property) noexcept {
    return std::find(
               kDirectEmitterProperties.begin(),
               kDirectEmitterProperties.end(),
               property) != kDirectEmitterProperties.end();
}

void initialize_property_rows(FixtureTargetCapabilities& result) noexcept {
    for (std::size_t index = 0U; index < result.properties.size(); ++index) {
        result.properties[index].property = static_cast<showcore::Property>(index);
    }
}

void append_fixture(
    const ProjectDocument& project,
    std::size_t fixture_index,
    std::array<bool, showcore::kMaxFixtures>& included,
    FixtureTargetCapabilities& result) {
    if (fixture_index >= project.fixtures.size() || fixture_index >= included.size() ||
        included[fixture_index]) {
        return;
    }
    included[fixture_index] = true;
    auto capability = inspect_fixture_capabilities(project, fixture_index);
    for (std::size_t property = 0U; property < capability.properties.size(); ++property) {
        if (capability.properties[property]) {
            ++result.properties[property].supported_fixture_count;
        }
    }
    result.has_direct_emitters = result.has_direct_emitters || capability.has_direct_emitters;
    result.any_master_intensity =
        result.any_master_intensity || capability.has_master_intensity;
    result.fixtures.push_back(std::move(capability));
}

}  // namespace

const TargetPropertyCapability& FixtureTargetCapabilities::capability(
    showcore::Property property) const noexcept {
    static const TargetPropertyCapability unsupported{};
    const auto index = static_cast<std::size_t>(property);
    return index < properties.size() ? properties[index] : unsupported;
}

const FixtureProfileDefinition* find_fixture_profile(
    const ProjectDocument& project,
    std::string_view profile_id) noexcept {
    const auto found = std::find_if(
        project.fixture_profiles.begin(),
        project.fixture_profiles.end(),
        [profile_id](const auto& profile) { return profile.id == profile_id; });
    return found == project.fixture_profiles.end() ? nullptr : &*found;
}

bool fixture_profile_supports_property(
    const FixtureProfileDefinition& profile,
    showcore::Property property) noexcept {
    if (property >= showcore::Property::Count) {
        return false;
    }
    return std::any_of(
        profile.channels.begin(),
        profile.channels.end(),
        [property](const auto& channel) {
            if (channel.encoding == showcore::ChannelEncoding::Constant8) {
                return false;
            }
            if (channel.property == property) {
                return true;
            }
            return std::any_of(
                channel.capabilities.begin(),
                channel.capabilities.end(),
                [property](const auto& capability) {
                    return capability.property == property &&
                        capability.access !=
                            showcore::ChannelCapabilityAccess::Protected;
                });
        });
}

FixtureCapabilityView inspect_fixture_capabilities(
    const ProjectDocument& project,
    std::size_t fixture_index) noexcept {
    FixtureCapabilityView result;
    result.fixture_index = fixture_index;
    if (fixture_index >= project.fixtures.size()) {
        return result;
    }
    const auto& fixture = project.fixtures[fixture_index];
    result.fixture_id = fixture.id;
    result.fixture_name = fixture.name;
    result.profile_id = fixture.profile_id;
    const auto* profile = find_fixture_profile(project, fixture.profile_id);
    if (profile == nullptr) {
        return result;
    }
    result.complete = true;
    result.profile_name = profile->name;
    result.manufacturer = profile->manufacturer;
    result.model = profile->model;
    result.mode = profile->mode;
    result.source_revision = profile->source_revision;
    for (const auto& channel : profile->channels) {
        if (channel.encoding == showcore::ChannelEncoding::Constant8) {
            continue;
        }
        if (channel.property < showcore::Property::Count) {
            const auto property = static_cast<std::size_t>(channel.property);
            result.properties[property] = true;
            result.has_direct_emitters =
                result.has_direct_emitters || is_direct_emitter(channel.property);
        }
        for (const auto& capability : channel.capabilities) {
            if (capability.property >= showcore::Property::Count ||
                capability.access == showcore::ChannelCapabilityAccess::Protected) {
                continue;
            }
            result.properties[static_cast<std::size_t>(capability.property)] = true;
            result.has_direct_emitters = result.has_direct_emitters ||
                is_direct_emitter(capability.property);
        }
    }
    result.has_master_intensity =
        result.properties[static_cast<std::size_t>(showcore::Property::Intensity)];
    return result;
}

FixtureTargetCapabilities inspect_fixture_target(
    const ProjectDocument& project,
    std::string_view target_id) {
    FixtureTargetCapabilities result;
    initialize_property_rows(result);
    result.target_id = target_id;
    std::array<bool, showcore::kMaxFixtures> included{};

    const auto fixture = std::find_if(
        project.fixtures.begin(),
        project.fixtures.end(),
        [target_id](const auto& candidate) { return candidate.id == target_id; });
    if (fixture != project.fixtures.end()) {
        result.target_found = true;
        result.target_name = fixture->name;
        append_fixture(
            project,
            static_cast<std::size_t>(fixture - project.fixtures.begin()),
            included,
            result);
    } else {
        const auto group = std::find_if(
            project.groups.begin(),
            project.groups.end(),
            [target_id](const auto& candidate) { return candidate.id == target_id; });
        if (group == project.groups.end()) {
            return result;
        }
        result.target_found = true;
        result.group = true;
        result.target_name = group->name;
        for (const auto& fixture_id : group->fixture_ids) {
            const auto member = std::find_if(
                project.fixtures.begin(),
                project.fixtures.end(),
                [&fixture_id](const auto& candidate) { return candidate.id == fixture_id; });
            if (member == project.fixtures.end()) {
                result.warnings.push_back(
                    "Group member " + fixture_id + " is not patched.");
                continue;
            }
            append_fixture(
                project,
                static_cast<std::size_t>(member - project.fixtures.begin()),
                included,
                result);
        }
    }

    for (auto& property : result.properties) {
        property.target_fixture_count = result.fixtures.size();
    }
    std::size_t color_fixtures = 0U;
    std::size_t color_fixtures_with_master = 0U;
    for (const auto& member : result.fixtures) {
        if (!member.has_direct_emitters) {
            continue;
        }
        ++color_fixtures;
        if (member.has_master_intensity) {
            ++color_fixtures_with_master;
        }
    }
    result.all_color_fixtures_have_master_intensity =
        color_fixtures != 0U && color_fixtures == color_fixtures_with_master;
    if (result.group) {
        for (const auto& property : result.properties) {
            if (property.partial()) {
                result.warnings.push_back(
                    std::string(property_name(property.property)) + " is supported by " +
                    std::to_string(property.supported_fixture_count) + " of " +
                    std::to_string(property.target_fixture_count) + " fixtures.");
            }
        }
    }
    if (result.target_found && result.fixtures.empty()) {
        result.warnings.push_back("The selected target has no patched fixtures.");
    }
    return result;
}

}  // namespace emberlights
