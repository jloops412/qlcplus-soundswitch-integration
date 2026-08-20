#include "emberlights/fixture_capabilities.hpp"

#include "emberlights/fixture_parameter_catalog.hpp"
#include "emberlights/fixture_profile_editor.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <map>
#include <string>
#include <tuple>
#include <utility>

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

[[nodiscard]] std::string_view parameter_id(
    showcore::Property property) noexcept {
    const auto* descriptor = fixture_parameter_descriptor(property);
    return descriptor == nullptr ? property_name(property) : descriptor->stable_id;
}

[[nodiscard]] std::string control_choice_key(
    const ChannelDefinition& channel,
    const ChannelCapabilityDefinition& capability,
    std::size_t occurrence) {
    return channel.owner + "\x1f" + std::string(parameter_id(capability.property)) +
        "\x1f" + capability.id + "\x1f" +
        std::to_string(static_cast<unsigned int>(capability.behavior)) + "\x1f" +
        std::to_string(static_cast<unsigned int>(capability.role)) + "\x1f" +
        std::to_string(occurrence);
}

[[nodiscard]] std::string control_choice_id(
    std::string_view target_id,
    const ChannelDefinition& channel,
    const ChannelCapabilityDefinition& capability,
    std::size_t occurrence) {
    return "target:" + std::string(target_id) +
        "|owner:" + channel.owner +
        "|parameter:" + std::string(parameter_id(capability.property)) +
        "|function:" + capability.id +
        "|behavior:" +
        std::to_string(static_cast<unsigned int>(capability.behavior)) +
        "|role:" +
        std::to_string(static_cast<unsigned int>(capability.role)) +
        "|occurrence:" + std::to_string(occurrence);
}

[[nodiscard]] std::string direct_choice_capability_id(
    showcore::Property property) {
    return "direct." + std::string(parameter_id(property));
}

[[nodiscard]] std::string direct_control_choice_key(
    const ChannelDefinition& channel,
    std::size_t occurrence) {
    return channel.owner + "\x1f" + std::string(parameter_id(channel.property)) +
        "\x1f" + direct_choice_capability_id(channel.property) +
        "\x1f" + std::to_string(occurrence);
}

[[nodiscard]] std::string direct_control_choice_id(
    std::string_view target_id,
    const ChannelDefinition& channel,
    std::size_t occurrence) {
    return "target:" + std::string(target_id) +
        "|owner:" + channel.owner +
        "|parameter:" + std::string(parameter_id(channel.property)) +
        "|function:direct" +
        "|occurrence:" + std::to_string(occurrence);
}

[[nodiscard]] std::uint16_t encode_direct_value(
    const ChannelDefinition& channel,
    float position) noexcept {
    if (channel.encoding == showcore::ChannelEncoding::Linear16) {
        return static_cast<std::uint16_t>(std::lround(position * 65535.0F));
    }
    if (channel.encoding == showcore::ChannelEncoding::Ranged8 &&
        position <= 0.0F) {
        return channel.default_value;
    }
    return static_cast<std::uint16_t>(std::lround(
        static_cast<float>(channel.dmx_min) + position *
            static_cast<float>(channel.dmx_max - channel.dmx_min)));
}

[[nodiscard]] showcore::ChannelCapabilityAccess direct_choice_access(
    showcore::Property property) noexcept {
    const auto* descriptor = fixture_parameter_descriptor(property);
    return descriptor != nullptr && descriptor->safety_restricted()
        ? showcore::ChannelCapabilityAccess::SafetyGated
        : showcore::ChannelCapabilityAccess::Selectable;
}

[[nodiscard]] int parameter_category_rank(showcore::Property property) noexcept {
    const auto* descriptor = fixture_parameter_descriptor(property);
    return descriptor == nullptr
        ? static_cast<int>(FixtureParameterCategory::Custom)
        : static_cast<int>(descriptor->category);
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

FixtureControlChoiceCatalog fixture_control_choices(
    const ProjectDocument& project,
    std::string_view target_id,
    float position) {
    FixtureControlChoiceCatalog result;
    const auto target = inspect_fixture_target(project, target_id);
    result.target_found = target.target_found;
    result.group = target.group;
    result.target_id = std::string(target.target_id);
    result.target_name = std::string(target.target_name);
    result.target_fixture_count = target.fixtures.size();
    result.warnings = target.warnings;
    if (!target.target_found || target.fixtures.empty()) {
        return result;
    }
    position = std::isfinite(position)
        ? std::clamp(position, 0.0F, 1.0F)
        : 0.5F;

    struct AccumulatedChoice {
        std::string key;
        FixtureControlChoice choice;
    };
    std::vector<AccumulatedChoice> accumulated;

    for (const auto& fixture : target.fixtures) {
        if (!fixture.complete) {
            continue;
        }
        const auto* profile = find_fixture_profile(project, fixture.profile_id);
        if (profile == nullptr) {
            continue;
        }
        std::map<std::string, std::size_t> occurrences;
        for (const auto& channel : profile->channels) {
            const auto one_based_channel = static_cast<std::uint16_t>(
                static_cast<std::size_t>(channel.coarse_offset) + 1U);
            if (channel.encoding != showcore::ChannelEncoding::Constant8 &&
                channel.property < showcore::Property::Count &&
                channel.capabilities.empty()) {
                const auto occurrence_base = channel.owner + "\x1f" +
                    std::string(parameter_id(channel.property)) +
                    "\x1f" + std::string("direct");
                const auto occurrence = occurrences[occurrence_base]++;
                const auto key = direct_control_choice_key(channel, occurrence);
                auto found = std::find_if(
                    accumulated.begin(), accumulated.end(),
                    [&key](const auto& candidate) {
                        return candidate.key == key;
                    });
                if (found == accumulated.end()) {
                    AccumulatedChoice entry;
                    entry.key = key;
                    entry.choice.id = direct_control_choice_id(
                        target_id, channel, occurrence);
                    entry.choice.capability_id =
                        direct_choice_capability_id(channel.property);
                    const auto* descriptor =
                        fixture_parameter_descriptor(channel.property);
                    entry.choice.name = descriptor == nullptr
                        ? std::string(property_name(channel.property))
                        : std::string(descriptor->display_name);
                    entry.choice.owner = channel.owner;
                    entry.choice.kind =
                        FixtureControlChoiceKind::DirectAttribute;
                    entry.choice.property = channel.property;
                    entry.choice.behavior =
                        showcore::ChannelCapabilityBehavior::Continuous;
                    entry.choice.access = direct_choice_access(channel.property);
                    entry.choice.role = FixtureChannelCapabilityRole::Function;
                    accumulated.push_back(std::move(entry));
                    found = std::prev(accumulated.end());
                } else if (direct_choice_access(channel.property) ==
                           showcore::ChannelCapabilityAccess::SafetyGated) {
                    found->choice.access =
                        showcore::ChannelCapabilityAccess::SafetyGated;
                }

                const auto encoded = encode_direct_value(channel, position);
                FixtureControlChoiceValue value;
                value.fixture_id = std::string(fixture.fixture_id);
                value.profile_id = profile->id;
                value.binding_id = profile->id + "/ch" +
                    std::to_string(one_based_channel) + "/direct." +
                    std::string(parameter_id(channel.property));
                value.channel = one_based_channel;
                value.property = channel.property;
                value.normalized_value = position;
                value.semantic_min = 0.0F;
                value.semantic_max = 1.0F;
                value.raw_value = channel.encoding ==
                        showcore::ChannelEncoding::Linear16
                    ? static_cast<std::uint8_t>((encoded >> 8U) & 0xFFU)
                    : static_cast<std::uint8_t>(encoded & 0xFFU);
                value.dmx_min = channel.dmx_min;
                value.dmx_max = channel.dmx_max;
                value.encoding = channel.encoding;
                if (channel.encoding == showcore::ChannelEncoding::Linear16 &&
                    channel.fine_offset >= 0) {
                    value.fine_channel = static_cast<std::uint16_t>(
                        static_cast<std::size_t>(channel.fine_offset) + 1U);
                    value.raw_fine_value =
                        static_cast<std::uint8_t>(encoded & 0xFFU);
                }
                value.default_value = channel.default_value;
                value.blackout_value = channel.blackout_value;
                value.highlight_value = channel.highlight_value;
                found->choice.values.push_back(std::move(value));
            }
            for (const auto& capability : channel.capabilities) {
                if (capability.access ==
                        showcore::ChannelCapabilityAccess::Protected ||
                    capability.property >= showcore::Property::Count) {
                    continue;
                }
                const auto occurrence_base = channel.owner + "\x1f" +
                    std::string(parameter_id(capability.property)) + "\x1f" +
                    capability.id + "\x1f" +
                    std::to_string(static_cast<unsigned int>(capability.behavior)) +
                    "\x1f" +
                    std::to_string(static_cast<unsigned int>(capability.role));
                const auto occurrence = occurrences[occurrence_base]++;
                const auto key = control_choice_key(
                    channel, capability, occurrence);
                const auto selection = resolve_fixture_channel_capability(
                    *profile,
                    one_based_channel,
                    capability.id,
                    position);
                if (!selection) {
                    continue;
                }

                auto found = std::find_if(
                    accumulated.begin(), accumulated.end(),
                    [&key](const auto& candidate) {
                        return candidate.key == key;
                    });
                if (found == accumulated.end()) {
                    AccumulatedChoice entry;
                    entry.key = key;
                    entry.choice.id = control_choice_id(
                        target_id, channel, capability, occurrence);
                    entry.choice.capability_id = capability.id;
                    entry.choice.name = capability.name;
                    entry.choice.owner = channel.owner;
                    entry.choice.kind =
                        FixtureControlChoiceKind::NamedCapability;
                    entry.choice.property = capability.property;
                    entry.choice.behavior = capability.behavior;
                    entry.choice.access = capability.access;
                    entry.choice.role = capability.role;
                    accumulated.push_back(std::move(entry));
                    found = std::prev(accumulated.end());
                } else {
                    if (found->choice.name != capability.name) {
                        result.warnings.push_back(
                            "Named function " + capability.id +
                            " has different labels across target profiles.");
                    }
                    if (capability.access ==
                        showcore::ChannelCapabilityAccess::SafetyGated) {
                        found->choice.access =
                            showcore::ChannelCapabilityAccess::SafetyGated;
                    }
                }
                FixtureControlChoiceValue value;
                value.fixture_id = std::string(fixture.fixture_id);
                value.profile_id = profile->id;
                value.binding_id = selection.binding_id;
                value.channel = one_based_channel;
                value.property = selection.property;
                value.normalized_value = selection.normalized_value;
                value.semantic_min = selection.semantic_min;
                value.semantic_max = selection.semantic_max;
                value.raw_value = selection.raw_value;
                value.dmx_min = capability.dmx_min;
                value.dmx_max = capability.dmx_max;
                value.encoding = channel.encoding;
                value.default_value = channel.default_value;
                value.blackout_value = channel.blackout_value;
                value.highlight_value = channel.highlight_value;
                found->choice.values.push_back(std::move(value));
            }
        }
    }

    result.choices.reserve(accumulated.size());
    for (auto& entry : accumulated) {
        auto& choice = entry.choice;
        choice.supported_fixture_count = choice.values.size();
        choice.target_fixture_count = result.target_fixture_count;
        if (!choice.values.empty()) {
            choice.shared_normalized_value = choice.values.front().normalized_value;
            choice.shared_value = std::all_of(
                choice.values.begin(), choice.values.end(),
                [&](const auto& value) {
                    return value.property == choice.property &&
                        std::fabs(value.normalized_value -
                                  choice.shared_normalized_value) <= 0.000001F;
                });
        }
        result.choices.push_back(std::move(choice));
    }
    std::sort(
        result.choices.begin(), result.choices.end(),
        [](const auto& first, const auto& second) {
            return std::tuple{
                       parameter_category_rank(first.property),
                       static_cast<std::size_t>(first.property),
                       static_cast<unsigned int>(first.kind),
                       first.owner,
                       first.name,
                       first.id} <
                std::tuple{
                       parameter_category_rank(second.property),
                       static_cast<std::size_t>(second.property),
                       static_cast<unsigned int>(second.kind),
                       second.owner,
                       second.name,
                       second.id};
        });
    return result;
}

}  // namespace emberlights
