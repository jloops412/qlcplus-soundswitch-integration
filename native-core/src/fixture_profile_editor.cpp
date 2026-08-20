#include "emberlights/fixture_profile_editor.hpp"
#include "emberlights/compiler.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <sstream>
#include <utility>

namespace emberlights {
namespace {

using showcore::ChannelEncoding;
using showcore::Property;

const std::array<FixtureProfileTemplateDescriptor, 10U> kTemplates{{
    {FixtureProfileTemplateId::Dimmer1, "dimmer-1ch", "Dimmer • 1CH",
     "One master intensity channel", 1U},
    {FixtureProfileTemplateId::Rgb3, "rgb-3ch", "RGB • 3CH",
     "Red, Green, Blue", 3U},
    {FixtureProfileTemplateId::Rgbw4, "rgbw-4ch", "RGBW • 4CH",
     "Red, Green, Blue, White", 4U},
    {FixtureProfileTemplateId::Rgba4, "rgba-4ch", "RGBA • 4CH",
     "Red, Green, Blue, Amber", 4U},
    {FixtureProfileTemplateId::Rgbwauv6, "rgbwauv-6ch", "RGBWA+UV • 6CH",
     "Red, Green, Blue, White, Amber, UV", 6U},
    {FixtureProfileTemplateId::MasterRgbwauv7, "master-rgbwauv-7ch",
     "Master + RGBWA+UV • 7CH",
     "Intensity, Red, Green, Blue, White, Amber, UV", 7U},
    {FixtureProfileTemplateId::Rgbwa5, "rgbwa-5ch", "RGBWA • 5CH",
     "Red, Green, Blue, White, Amber", 5U},
    {FixtureProfileTemplateId::MasterRgb4, "master-rgb-4ch",
     "Master + RGB • 4CH", "Intensity, Red, Green, Blue", 4U},
    {FixtureProfileTemplateId::MasterRgbw5, "master-rgbw-5ch",
     "Master + RGBW • 5CH", "Intensity, Red, Green, Blue, White", 5U},
    {FixtureProfileTemplateId::PanTilt2, "pan-tilt-2ch", "Pan + Tilt • 2CH",
     "8-bit Pan, Tilt", 2U},
}};

[[nodiscard]] ChannelDefinition direct_channel(
    Property property,
    std::uint16_t zero_based_offset) noexcept {
    return {
        property,
        zero_based_offset,
        -1,
        ChannelEncoding::Linear8,
        0U,
        255U,
        0U};
}

[[nodiscard]] std::string display_property(Property property) {
    if (property == Property::Count) {
        return "Unused / safe constant";
    }
    const auto* descriptor = fixture_parameter_descriptor(property);
    return descriptor == nullptr
        ? std::string(property_name(property))
        : std::string(descriptor->display_name);
}

[[nodiscard]] std::string display_encoding(ChannelEncoding encoding) {
    switch (encoding) {
    case ChannelEncoding::Linear8: return "Linear 8-bit";
    case ChannelEncoding::Linear16: return "Linear 16-bit";
    case ChannelEncoding::Discrete8: return "Discrete 8-bit";
    case ChannelEncoding::Ranged8: return "Ranged 8-bit";
    case ChannelEncoding::Constant8: return "Safe constant";
    }
    return "Invalid";
}

[[nodiscard]] bool unsafe_without_chart(Property property) noexcept {
    const auto* descriptor = fixture_parameter_descriptor(property);
    return descriptor == nullptr || descriptor->needs_manual_dmx_chart();
}

[[nodiscard]] std::string joined_parameter_rows(
    const std::vector<std::string>& values) {
    std::ostringstream stream;
    for (std::size_t index = 0U; index < values.size(); ++index) {
        if (index != 0U) {
            stream << ", ";
        }
        stream << values[index];
    }
    return stream.str();
}

[[nodiscard]] FixtureProfileDefinition validation_copy(
    const FixtureProfileDefinition& draft) {
    auto candidate = draft;
    if (candidate.name.empty()) {
        candidate.name = "Unsaved fixture profile";
    }
    candidate.source = showcore::FixtureProfileSource::Local;
    return candidate;
}

[[nodiscard]] FixtureProfileEditorMutationResult validate_candidate(
    const FixtureProfileDefinition& candidate,
    bool replaced,
    std::string success_message) {
    const auto summary = summarize_fixture_profile_mapping(
        validation_copy(candidate));
    if (!summary.profile_valid) {
        return {
            FixtureProfileEditorError::ProfileInvalid,
            false,
            replaced,
            summary.validation_message};
    }
    return {
        FixtureProfileEditorError::None,
        true,
        replaced,
        std::move(success_message)};
}

[[nodiscard]] std::vector<Property> properties_for_template(
    FixtureProfileTemplateId id) {
    switch (id) {
    case FixtureProfileTemplateId::Dimmer1:
        return {Property::Intensity};
    case FixtureProfileTemplateId::Rgb3:
        return {Property::Red, Property::Green, Property::Blue};
    case FixtureProfileTemplateId::Rgbw4:
        return {Property::Red, Property::Green, Property::Blue, Property::White};
    case FixtureProfileTemplateId::Rgba4:
        return {Property::Red, Property::Green, Property::Blue, Property::Amber};
    case FixtureProfileTemplateId::Rgbwauv6:
        return {Property::Red, Property::Green, Property::Blue, Property::White,
                Property::Amber, Property::UV};
    case FixtureProfileTemplateId::MasterRgbwauv7:
        return {Property::Intensity, Property::Red, Property::Green, Property::Blue,
                Property::White, Property::Amber, Property::UV};
    case FixtureProfileTemplateId::Rgbwa5:
        return {Property::Red, Property::Green, Property::Blue, Property::White,
                Property::Amber};
    case FixtureProfileTemplateId::MasterRgb4:
        return {Property::Intensity, Property::Red, Property::Green,
                Property::Blue};
    case FixtureProfileTemplateId::MasterRgbw5:
        return {Property::Intensity, Property::Red, Property::Green,
                Property::Blue, Property::White};
    case FixtureProfileTemplateId::PanTilt2:
        return {Property::Pan, Property::Tilt};
    }
    return {};
}

[[nodiscard]] std::array<bool, showcore::kUniverseSlots>
occupied_profile_slots(const FixtureProfileDefinition& profile) noexcept {
    std::array<bool, showcore::kUniverseSlots> occupied{};
    for (const auto& channel : profile.channels) {
        if (channel.coarse_offset < occupied.size()) {
            occupied[channel.coarse_offset] = true;
        }
        if (channel.fine_offset >= 0 &&
            static_cast<std::size_t>(channel.fine_offset) < occupied.size()) {
            occupied[static_cast<std::size_t>(channel.fine_offset)] = true;
        }
    }
    return occupied;
}

[[nodiscard]] const ChannelDefinition* profile_channel_at(
    const FixtureProfileDefinition& profile,
    std::uint16_t one_based_channel) noexcept {
    if (one_based_channel == 0U) {
        return nullptr;
    }
    const auto found = std::find_if(
        profile.channels.begin(), profile.channels.end(),
        [one_based_channel](const auto& channel) {
            return channel.coarse_offset == one_based_channel - 1U;
        });
    return found == profile.channels.end() ? nullptr : &*found;
}

[[nodiscard]] ChannelDefinition* profile_channel_at(
    FixtureProfileDefinition& profile,
    std::uint16_t one_based_channel) noexcept {
    return const_cast<ChannelDefinition*>(profile_channel_at(
        static_cast<const FixtureProfileDefinition&>(profile),
        one_based_channel));
}

[[nodiscard]] bool direct_swap_parameter_is_safe(Property property) noexcept {
    const auto* descriptor = fixture_parameter_descriptor(property);
    return descriptor != nullptr &&
        descriptor->supports(FixtureParameterSurface::Profile) &&
        descriptor->profile_preset ==
            FixtureParameterProfilePreset::DirectLinear &&
        descriptor->safety == FixtureParameterSafety::Normal;
}

[[nodiscard]] bool direct_channel_layouts_are_compatible(
    const ChannelDefinition& first,
    const ChannelDefinition& second) noexcept {
    return first.encoding == second.encoding &&
        first.dmx_min == second.dmx_min &&
        first.dmx_max == second.dmx_max &&
        first.default_value == second.default_value &&
        first.blackout_value == second.blackout_value &&
        first.highlight_value == second.highlight_value &&
        first.owner == second.owner;
}

}  // namespace

std::span<const FixtureProfileTemplateDescriptor>
fixture_profile_templates() noexcept {
    return kTemplates;
}

FixtureProfileEditorMutationResult apply_fixture_profile_template(
    FixtureProfileDefinition& draft,
    FixtureProfileTemplateId template_id) {
    const auto descriptor = std::find_if(
        kTemplates.begin(), kTemplates.end(),
        [template_id](const auto& candidate) {
            return candidate.id == template_id;
        });
    if (descriptor == kTemplates.end()) {
        return {
            FixtureProfileEditorError::InvalidTemplate,
            false,
            false,
            "Choose a supported fixture-profile template."};
    }
    const auto properties = properties_for_template(template_id);
    auto candidate = draft;
    candidate.footprint = descriptor->footprint;
    candidate.channels.clear();
    candidate.channels.reserve(properties.size());
    for (std::size_t index = 0U; index < properties.size(); ++index) {
        candidate.channels.push_back(direct_channel(
            properties[index], static_cast<std::uint16_t>(index)));
    }
    const auto validation = validate_candidate(
        candidate,
        !draft.channels.empty(),
        descriptor->display_name +
            " applied. Confirm the order against the fixture's DMX chart.");
    if (!validation) {
        return validation;
    }
    draft = std::move(candidate);
    return validation;
}

FixtureProfileEditorMutationResult make_safe_fixture_profile_channel(
    Property property,
    std::uint16_t one_based_channel,
    ChannelDefinition& definition) {
    if (one_based_channel == 0U || one_based_channel > showcore::kUniverseSlots) {
        return {
            FixtureProfileEditorError::InvalidChannel,
            false,
            false,
            "Choose a channel from 1 through 512."};
    }
    if (property == Property::Count) {
        definition = {
            Property::Count,
            static_cast<std::uint16_t>(one_based_channel - 1U),
            -1,
            ChannelEncoding::Constant8,
            0U,
            255U,
            0U};
        return {
            FixtureProfileEditorError::None,
            true,
            false,
            "Safe constant defaults applied."};
    }
    if (unsafe_without_chart(property)) {
        const auto* descriptor = fixture_parameter_descriptor(property);
        return {
            FixtureProfileEditorError::UnsafePreset,
            false,
            false,
            display_property(property) +
                " needs the fixture's documented DMX capabilities, active range, and safe default; no generic preset was applied." +
                (descriptor == nullptr
                     ? std::string{}
                     : " Control type: " +
                           std::string(fixture_parameter_control_kind_name(
                               descriptor->control_kind)) + ".")};
    }
    definition = direct_channel(
        property, static_cast<std::uint16_t>(one_based_channel - 1U));
    return {
        FixtureProfileEditorError::None,
        true,
        false,
        display_property(property) + " safe direct-channel defaults applied."};
}

FixtureProfileEditorMutationResult upsert_fixture_profile_channel(
    FixtureProfileDefinition& draft,
    const ChannelDefinition& definition) {
    if (draft.footprint == 0U || draft.footprint > showcore::kUniverseSlots) {
        return {
            FixtureProfileEditorError::InvalidFootprint,
            false,
            false,
            "Enter a footprint from 1 through 512 first."};
    }
    if (definition.coarse_offset >= draft.footprint) {
        return {
            FixtureProfileEditorError::InvalidChannel,
            false,
            false,
            "The channel must be inside the profile footprint."};
    }
    auto candidate = draft;
    const auto existing = std::find_if(
        candidate.channels.begin(), candidate.channels.end(),
        [&](const auto& channel) {
            return channel.coarse_offset == definition.coarse_offset;
        });
    const auto replaced = existing != candidate.channels.end();
    if (replaced) {
        const auto preserved_capabilities = existing->capabilities;
        const auto preserved_owner = existing->owner;
        const auto preserved_blackout = existing->blackout_value;
        const auto preserved_highlight = existing->highlight_value;
        *existing = definition;
        existing->capabilities = preserved_capabilities;
        existing->owner = preserved_owner;
        existing->blackout_value = preserved_blackout;
        existing->highlight_value = preserved_highlight;
        if (!existing->capabilities.empty()) {
            const auto first_property = existing->capabilities.front().property;
            const auto compound = std::any_of(
                existing->capabilities.begin(),
                existing->capabilities.end(),
                [first_property](const auto& capability) {
                    return capability.property != first_property;
                });
            existing->property = compound ? Property::Count : first_property;
            if (compound) {
                existing->encoding = ChannelEncoding::Discrete8;
                existing->fine_offset = -1;
                existing->dmx_min = 0U;
                existing->dmx_max = 255U;
            }
        }
    } else {
        candidate.channels.push_back(definition);
    }
    std::stable_sort(
        candidate.channels.begin(), candidate.channels.end(),
        [](const auto& left, const auto& right) {
            return left.coarse_offset < right.coarse_offset;
        });
    const auto validation = validate_candidate(
        candidate,
        replaced,
        replaced ? "Channel mapping replaced." : "Channel mapping added.");
    if (!validation) {
        return validation;
    }
    draft = std::move(candidate);
    return validation;
}

FixtureProfileEditorMutationResult remove_fixture_profile_channel(
    FixtureProfileDefinition& draft,
    std::uint16_t one_based_channel) {
    if (one_based_channel == 0U || one_based_channel > draft.footprint) {
        return {
            FixtureProfileEditorError::InvalidChannel,
            false,
            false,
            "Choose a mapped channel inside the profile footprint."};
    }
    auto candidate = draft;
    const auto found = std::find_if(
        candidate.channels.begin(), candidate.channels.end(),
        [one_based_channel](const auto& channel) {
            return channel.coarse_offset == one_based_channel - 1U;
        });
    if (found == candidate.channels.end()) {
        return {
            FixtureProfileEditorError::InvalidChannel,
            false,
            false,
            "That channel is not mapped."};
    }
    if (candidate.channels.size() == 1U) {
        return {
            FixtureProfileEditorError::LastChannel,
            false,
            false,
            "A fixture profile needs at least one mapped or safe-constant channel."};
    }
    candidate.channels.erase(found);
    const auto validation = validate_candidate(
        candidate, false, "Channel mapping removed.");
    if (!validation) {
        return validation;
    }
    draft = std::move(candidate);
    return validation;
}

FixtureProfileEditorMutationResult update_fixture_profile_channel_metadata(
    FixtureProfileDefinition& draft,
    std::uint16_t one_based_channel,
    std::string owner,
    std::uint16_t blackout_value,
    std::uint16_t highlight_value) {
    if (owner.empty() || owner.size() > showcore::kFixtureProfileTextLength) {
        return {
            FixtureProfileEditorError::InvalidDefinition,
            false,
            false,
            "Channel owner must be 1–96 characters, such as fixture, head.1, or cell.4."};
    }
    auto candidate = draft;
    const auto channel = std::find_if(
        candidate.channels.begin(),
        candidate.channels.end(),
        [one_based_channel](const auto& value) {
            return one_based_channel != 0U &&
                value.coarse_offset == one_based_channel - 1U;
        });
    if (channel == candidate.channels.end()) {
        return {
            FixtureProfileEditorError::InvalidChannel,
            false,
            false,
            "Choose a mapped channel first."};
    }
    const auto maximum = channel->encoding == ChannelEncoding::Linear16
        ? 65535U
        : 255U;
    if (blackout_value > maximum || highlight_value > maximum) {
        return {
            FixtureProfileEditorError::InvalidDefinition,
            false,
            false,
            channel->encoding == ChannelEncoding::Linear16
                ? "16-bit channel blackout/highlight values must be 0–65535."
                : "8-bit channel blackout/highlight values must be 0–255."};
    }
    const auto changed = channel->owner != owner ||
        channel->blackout_value != blackout_value ||
        channel->highlight_value != highlight_value;
    channel->owner = std::move(owner);
    channel->blackout_value = blackout_value;
    channel->highlight_value = highlight_value;
    const auto validation = validate_candidate(
        candidate,
        true,
        changed ? "Channel ownership and safe values updated."
                : "Channel ownership and safe values are unchanged.");
    if (!validation) {
        return validation;
    }
    draft = std::move(candidate);
    auto result = validation;
    result.changed = changed;
    return result;
}

std::vector<FixtureProfileEditorRow> fixture_profile_editor_rows(
    const FixtureProfileDefinition& profile) {
    std::vector<FixtureProfileEditorRow> rows;
    rows.reserve(profile.channels.size());
    for (std::size_t index = 0U; index < profile.channels.size(); ++index) {
        const auto& definition = profile.channels[index];
        FixtureProfileEditorRow row;
        row.source_index = index;
        row.channel = static_cast<std::uint16_t>(definition.coarse_offset + 1U);
        row.fine_channel = definition.fine_offset < 0
            ? 0U
            : static_cast<std::uint16_t>(definition.fine_offset + 1);
        row.property = definition.property;
        row.encoding = definition.encoding;
        row.property_label = display_property(definition.property);
        row.encoding_label = display_encoding(definition.encoding);
        row.range_label = std::to_string(definition.dmx_min) + "–" +
            std::to_string(definition.dmx_max);
        row.default_label = std::to_string(definition.default_value);
        row.fine_label = row.fine_channel == 0U
            ? "—"
            : "CH" + std::to_string(row.fine_channel);
        row.owner_label = definition.owner;
        row.capability_label = definition.capabilities.empty()
            ? "—"
            : std::to_string(definition.capabilities.size()) + " named";
        std::ostringstream accessible;
        accessible << "Channel " << row.channel << ", " << row.property_label
                   << ", " << row.encoding_label << ", DMX "
                   << row.range_label << ", default " << row.default_label;
        accessible << ", owner " << row.owner_label << ", "
                   << row.capability_label << " capability ranges";
        if (row.fine_channel != 0U) {
            accessible << ", fine channel " << row.fine_channel;
        }
        row.accessibility_label = accessible.str();
        rows.push_back(std::move(row));
    }
    std::stable_sort(
        rows.begin(), rows.end(),
        [](const auto& left, const auto& right) {
            return left.channel < right.channel;
        });
    return rows;
}

std::vector<FixtureProfileParameterChoice>
fixture_profile_parameter_choices() {
    const auto catalog = fixture_parameter_catalog();
    std::vector<FixtureProfileParameterChoice> choices;
    choices.reserve(catalog.size());
    for (const auto& descriptor : catalog) {
        FixtureProfileParameterChoice choice;
        choice.property = descriptor.property;
        choice.stable_id = std::string(descriptor.stable_id);
        choice.display_name = std::string(descriptor.display_name);
        choice.description = std::string(descriptor.description);
        choice.category_label = std::string(
            fixture_parameter_category_name(descriptor.category));
        choice.control_label = std::string(
            fixture_parameter_control_kind_name(descriptor.control_kind));
        choice.safety_label = std::string(
            fixture_parameter_safety_name(descriptor.safety));
        choice.direct_assignment_available =
            descriptor.supports(FixtureParameterSurface::Profile) &&
            descriptor.profile_preset ==
                FixtureParameterProfilePreset::DirectLinear &&
            descriptor.safety == FixtureParameterSafety::Normal;
        if (!descriptor.supports(FixtureParameterSurface::Profile)) {
            choice.unavailable_reason =
                "This parameter is not available in fixture profiles.";
        } else if (descriptor.profile_preset ==
                   FixtureParameterProfilePreset::ManualDmxChart) {
            choice.unavailable_reason =
                "Add this from the fixture DMX chart with named ranges and a safe default.";
        } else if (descriptor.safety != FixtureParameterSafety::Normal) {
            choice.unavailable_reason =
                "This safety-restricted parameter needs explicit reviewed ranges.";
        }
        choice.accessibility_label = choice.display_name + ", " +
            choice.category_label + ", " + choice.control_label + ", " +
            (choice.direct_assignment_available
                 ? std::string("ready for a direct 8-bit channel")
                 : choice.unavailable_reason);
        choices.push_back(std::move(choice));
    }
    return choices;
}

FixtureProfileChannelPlacementResult
assign_next_or_append_fixture_profile_channel(
    FixtureProfileDefinition& draft,
    Property property) {
    FixtureProfileChannelPlacementResult result;
    if (draft.source != showcore::FixtureProfileSource::Local) {
        result.error = FixtureProfileChannelPlacementError::SourceReadOnly;
        result.message =
            "This source snapshot is read-only. Duplicate it as a Local profile before assigning channels.";
        return result;
    }
    const auto* descriptor = fixture_parameter_descriptor(property);
    if (descriptor == nullptr ||
        !descriptor->supports(FixtureParameterSurface::Profile)) {
        result.error = FixtureProfileChannelPlacementError::InvalidProperty;
        result.message = "Choose a parameter from the fixture catalog.";
        return result;
    }
    if (descriptor->profile_preset !=
            FixtureParameterProfilePreset::DirectLinear ||
        descriptor->safety != FixtureParameterSafety::Normal) {
        result.error = FixtureProfileChannelPlacementError::UnsafePreset;
        result.message = std::string(descriptor->display_name) +
            " needs its documented DMX ranges and safe value; it was not guessed or assigned.";
        return result;
    }
    if (draft.footprint > showcore::kUniverseSlots) {
        result.error = FixtureProfileChannelPlacementError::InvalidProfile;
        result.message = "Repair the profile footprint before assigning channels.";
        return result;
    }

    const auto occupied = occupied_profile_slots(draft);
    std::uint16_t channel = 0U;
    for (std::size_t index = 0U; index < draft.footprint; ++index) {
        if (!occupied[index]) {
            channel = static_cast<std::uint16_t>(index + 1U);
            result.filled_gap = true;
            break;
        }
    }
    auto candidate = draft;
    if (channel == 0U) {
        if (draft.footprint >= showcore::kUniverseSlots) {
            result.error = FixtureProfileChannelPlacementError::ProfileFull;
            result.message =
                "All 512 fixture channels are already described; nothing changed.";
            return result;
        }
        candidate.footprint = static_cast<std::uint16_t>(
            static_cast<std::size_t>(draft.footprint) + 1U);
        channel = candidate.footprint;
        result.grew_footprint = true;
    }

    ChannelDefinition definition;
    const auto preset = make_safe_fixture_profile_channel(
        property, channel, definition);
    if (!preset) {
        result.error = preset.error == FixtureProfileEditorError::UnsafePreset
            ? FixtureProfileChannelPlacementError::UnsafePreset
            : FixtureProfileChannelPlacementError::CandidateInvalid;
        result.message = preset.message;
        return result;
    }
    candidate.channels.push_back(std::move(definition));
    std::stable_sort(
        candidate.channels.begin(), candidate.channels.end(),
        [](const auto& left, const auto& right) {
            return left.coarse_offset < right.coarse_offset;
        });
    const auto validation = validate_candidate(
        candidate, false, "Catalog parameter assigned.");
    if (!validation) {
        result.error = FixtureProfileChannelPlacementError::CandidateInvalid;
        result.message = validation.message;
        return result;
    }

    draft = std::move(candidate);
    result.error = FixtureProfileChannelPlacementError::None;
    result.changed = true;
    result.channel = channel;
    result.message = std::string(descriptor->display_name) + " assigned to CH" +
        std::to_string(channel) +
        (result.grew_footprint
             ? "; the profile footprint grew to " +
                   std::to_string(draft.footprint) + "."
             : "; the existing footprint gap was filled.") +
        " Confirm the physical order against the fixture's DMX chart.";
    return result;
}

FixtureProfileChannelPlacementResult
fill_fixture_profile_channel_gaps_with_safe_constants(
    FixtureProfileDefinition& draft) {
    FixtureProfileChannelPlacementResult result;
    if (draft.source != showcore::FixtureProfileSource::Local) {
        result.error = FixtureProfileChannelPlacementError::SourceReadOnly;
        result.message =
            "This source snapshot is read-only. Duplicate it as a Local profile before filling unused slots.";
        return result;
    }
    if (draft.footprint == 0U ||
        draft.footprint > showcore::kUniverseSlots) {
        result.error = FixtureProfileChannelPlacementError::InvalidProfile;
        result.message = "Enter a footprint from 1 through 512 first.";
        return result;
    }

    auto candidate = draft;
    const auto occupied = occupied_profile_slots(candidate);
    for (std::size_t index = 0U; index < candidate.footprint; ++index) {
        if (occupied[index]) {
            continue;
        }
        ChannelDefinition safe_constant;
        const auto preset = make_safe_fixture_profile_channel(
            Property::Count,
            static_cast<std::uint16_t>(index + 1U),
            safe_constant);
        if (!preset) {
            result.error = FixtureProfileChannelPlacementError::CandidateInvalid;
            result.message = preset.message;
            return result;
        }
        candidate.channels.push_back(std::move(safe_constant));
        ++result.filled_count;
    }
    std::stable_sort(
        candidate.channels.begin(), candidate.channels.end(),
        [](const auto& left, const auto& right) {
            return left.coarse_offset < right.coarse_offset;
        });
    const auto validation = validate_candidate(
        candidate, false, "Unused footprint slots made explicit.");
    if (!validation) {
        result.error = FixtureProfileChannelPlacementError::CandidateInvalid;
        result.filled_count = 0U;
        result.message = validation.message;
        return result;
    }

    result.error = FixtureProfileChannelPlacementError::None;
    result.changed = result.filled_count != 0U;
    if (result.changed) {
        draft = std::move(candidate);
        result.message = std::to_string(result.filled_count) +
            " unused footprint slot" +
            (result.filled_count == 1U ? std::string{} : std::string("s")) +
            " now holds an explicit zero-valued safe constant. Confirm unused slots against the fixture's DMX chart.";
    } else {
        result.message =
            "Every physical footprint slot is already described; nothing changed.";
    }
    return result;
}

FixtureProfileChannelFunctionSwapResult
plan_fixture_profile_channel_function_swap(
    const FixtureProfileDefinition& profile,
    std::uint16_t first_channel,
    std::uint16_t second_channel) {
    FixtureProfileChannelFunctionSwapResult result;
    if (profile.source != showcore::FixtureProfileSource::Local) {
        result.error = FixtureProfileChannelFunctionSwapError::SourceReadOnly;
        result.message =
            "This source snapshot is read-only. Duplicate it as a Local profile before correcting channel functions.";
        return result;
    }
    const auto summary = summarize_fixture_profile_mapping(
        validation_copy(profile));
    if (!summary.profile_valid) {
        result.error = FixtureProfileChannelFunctionSwapError::InvalidProfile;
        result.message = summary.validation_message;
        return result;
    }
    if (first_channel == 0U || second_channel == 0U ||
        first_channel > profile.footprint ||
        second_channel > profile.footprint) {
        result.error = FixtureProfileChannelFunctionSwapError::InvalidChannel;
        result.message =
            "Choose two mapped channels inside the profile footprint.";
        return result;
    }
    if (first_channel == second_channel) {
        result.error = FixtureProfileChannelFunctionSwapError::SameChannel;
        result.message = "Choose two different physical channels; nothing changed.";
        return result;
    }
    const auto* first = profile_channel_at(profile, first_channel);
    const auto* second = profile_channel_at(profile, second_channel);
    if (first == nullptr || second == nullptr) {
        result.error = FixtureProfileChannelFunctionSwapError::ChannelMissing;
        result.message =
            "Both selected physical channels must have an existing mapping; nothing changed.";
        return result;
    }
    if (first->fine_offset >= 0 || second->fine_offset >= 0) {
        result.error =
            FixtureProfileChannelFunctionSwapError::FineChannelUnsupported;
        result.message =
            "16-bit coarse/fine functions cannot be re-labelled with a direct-channel swap. Edit their complete mapping from the fixture chart.";
        return result;
    }
    if (!first->capabilities.empty() || !second->capabilities.empty() ||
        first->property == Property::Count ||
        second->property == Property::Count) {
        result.error =
            FixtureProfileChannelFunctionSwapError::CompoundChannelUnsupported;
        result.message =
            "Safe constants and channels with named/compound ranges cannot be swapped. Edit their complete capability rows instead.";
        return result;
    }
    if (first->encoding != ChannelEncoding::Linear8 ||
        second->encoding != ChannelEncoding::Linear8 ||
        !direct_swap_parameter_is_safe(first->property) ||
        !direct_swap_parameter_is_safe(second->property)) {
        result.error = FixtureProfileChannelFunctionSwapError::UnsafeFunction;
        result.message =
            "Only ordinary direct 8-bit functions can be swapped. Chart-dependent or safety-restricted functions stay unchanged.";
        return result;
    }
    if (!direct_channel_layouts_are_compatible(*first, *second)) {
        result.error =
            FixtureProfileChannelFunctionSwapError::IncompatibleMappings;
        result.message =
            "The selected channels have different ranges, defaults, blackout/highlight values, or owners. Rebuild those mappings explicitly instead of swapping labels.";
        return result;
    }

    auto& plan = result.plan;
    plan.profile_id = profile.id;
    plan.profile_name = profile.name;
    plan.source_revision = profile.source_revision;
    plan.source = profile.source;
    plan.source_behavior_fingerprint =
        fixture_profile_behavior_fingerprint(profile);
    plan.first_channel = first_channel;
    plan.second_channel = second_channel;
    plan.first_property_before = first->property;
    plan.second_property_before = second->property;
    plan.first_property_after = second->property;
    plan.second_property_after = first->property;
    plan.changes_mapping = first->property != second->property;

    auto candidate = profile;
    if (plan.changes_mapping) {
        auto* candidate_first = profile_channel_at(candidate, first_channel);
        auto* candidate_second = profile_channel_at(candidate, second_channel);
        std::swap(candidate_first->property, candidate_second->property);
    }
    const auto candidate_summary = summarize_fixture_profile_mapping(
        validation_copy(candidate));
    if (!candidate_summary.profile_valid) {
        result.error = FixtureProfileChannelFunctionSwapError::CandidateInvalid;
        result.plan = {};
        result.message =
            "The corrected profile candidate failed exact validation; nothing changed.";
        return result;
    }
    plan.candidate_behavior_fingerprint =
        fixture_profile_behavior_fingerprint(candidate);
    result.error = FixtureProfileChannelFunctionSwapError::None;
    result.changed = false;
    if (!plan.changes_mapping) {
        result.message = "Both channels already use " +
            display_property(first->property) + "; nothing would change.";
        return result;
    }
    result.message = "Ready to exchange " + display_property(first->property) +
        " on CH" + std::to_string(first_channel) + " with " +
        display_property(second->property) + " on CH" +
        std::to_string(second_channel) +
        ". Physical slots, ranges, owners, and safe values stay fixed; fixture confirmation is still required.";
    return result;
}

FixtureProfileChannelFunctionSwapResult
apply_fixture_profile_channel_function_swap(
    FixtureProfileDefinition& profile,
    const FixtureProfileChannelFunctionSwapPlan& plan) {
    FixtureProfileChannelFunctionSwapResult result;
    result.plan = plan;
    const auto source_matches =
        profile.id == plan.profile_id &&
        profile.name == plan.profile_name &&
        profile.source_revision == plan.source_revision &&
        profile.source == plan.source &&
        profile.source == showcore::FixtureProfileSource::Local &&
        fixture_profile_behavior_fingerprint(profile) ==
            plan.source_behavior_fingerprint;
    if (!source_matches) {
        result.error = FixtureProfileChannelFunctionSwapError::StalePlan;
        result.message =
            "The profile changed after this swap was reviewed; nothing changed.";
        return result;
    }
    const auto current = plan_fixture_profile_channel_function_swap(
        profile, plan.first_channel, plan.second_channel);
    if (!current ||
        current.plan.first_property_before != plan.first_property_before ||
        current.plan.second_property_before != plan.second_property_before ||
        current.plan.first_property_after != plan.first_property_after ||
        current.plan.second_property_after != plan.second_property_after ||
        current.plan.changes_mapping != plan.changes_mapping ||
        current.plan.candidate_behavior_fingerprint !=
            plan.candidate_behavior_fingerprint) {
        result.error = FixtureProfileChannelFunctionSwapError::StalePlan;
        result.message =
            "The reviewed channel swap no longer matches the current profile; nothing changed.";
        return result;
    }
    if (!plan.changes_mapping) {
        result.error = FixtureProfileChannelFunctionSwapError::None;
        result.message = current.message;
        return result;
    }

    auto candidate = profile;
    auto* first = profile_channel_at(candidate, plan.first_channel);
    auto* second = profile_channel_at(candidate, plan.second_channel);
    if (first == nullptr || second == nullptr) {
        result.error = FixtureProfileChannelFunctionSwapError::StalePlan;
        result.message =
            "A reviewed physical channel is no longer mapped; nothing changed.";
        return result;
    }
    std::swap(first->property, second->property);
    const auto summary = summarize_fixture_profile_mapping(
        validation_copy(candidate));
    if (!summary.profile_valid ||
        fixture_profile_behavior_fingerprint(candidate) !=
            plan.candidate_behavior_fingerprint) {
        result.error = FixtureProfileChannelFunctionSwapError::CandidateInvalid;
        result.message =
            "The corrected profile candidate failed exact validation; nothing changed.";
        return result;
    }

    profile = std::move(candidate);
    result.error = FixtureProfileChannelFunctionSwapError::None;
    result.changed = true;
    result.message = display_property(plan.first_property_after) + " is now CH" +
        std::to_string(plan.first_channel) + "; " +
        display_property(plan.second_property_after) + " is now CH" +
        std::to_string(plan.second_channel) +
        ". Physical hardware qualification is still required.";
    return result;
}

std::string make_fixture_channel_capability_id(std::string_view label) {
    std::string id;
    bool separator = false;
    for (const auto character : label) {
        const auto value = static_cast<unsigned char>(character);
        if (std::isalnum(value) != 0) {
            if (separator && !id.empty()) {
                id.push_back('-');
            }
            separator = false;
            id.push_back(static_cast<char>(std::tolower(value)));
        } else {
            separator = true;
        }
    }
    if (id.empty()) {
        id = "capability";
    }
    if (id.size() > showcore::kFixtureProfileTextLength) {
        id.resize(showcore::kFixtureProfileTextLength);
    }
    return id;
}

std::string_view fixture_channel_capability_behavior_name(
    showcore::ChannelCapabilityBehavior behavior) noexcept {
    switch (behavior) {
    case showcore::ChannelCapabilityBehavior::Slot: return "Named slot";
    case showcore::ChannelCapabilityBehavior::Continuous: return "Continuous range";
    }
    return "Invalid";
}

std::string_view fixture_channel_capability_access_name(
    showcore::ChannelCapabilityAccess access) noexcept {
    switch (access) {
    case showcore::ChannelCapabilityAccess::Selectable: return "Selectable";
    case showcore::ChannelCapabilityAccess::SafetyGated: return "Safety gated";
    case showcore::ChannelCapabilityAccess::Protected: return "Protected / unavailable";
    }
    return "Invalid";
}

std::string_view fixture_channel_capability_role_name(
    FixtureChannelCapabilityRole role) noexcept {
    switch (role) {
    case FixtureChannelCapabilityRole::Function: return "Function";
    case FixtureChannelCapabilityRole::Open: return "Open";
    case FixtureChannelCapabilityRole::Closed: return "Closed";
    case FixtureChannelCapabilityRole::Home: return "Home / neutral";
    case FixtureChannelCapabilityRole::Blackout: return "Blackout";
    case FixtureChannelCapabilityRole::Clockwise: return "Clockwise";
    case FixtureChannelCapabilityRole::CounterClockwise: return "Counter-clockwise";
    case FixtureChannelCapabilityRole::Reset: return "Reset";
    case FixtureChannelCapabilityRole::Service: return "Service";
    case FixtureChannelCapabilityRole::Custom: return "Custom";
    }
    return "Invalid";
}

FixtureChannelCapabilityMutationResult upsert_fixture_channel_capability(
    FixtureProfileDefinition& draft,
    std::uint16_t one_based_channel,
    const ChannelCapabilityDefinition& definition) {
    const auto channel = std::find_if(
        draft.channels.begin(),
        draft.channels.end(),
        [one_based_channel](const auto& candidate) {
            return one_based_channel != 0U &&
                candidate.coarse_offset == one_based_channel - 1U;
        });
    if (channel == draft.channels.end() ||
        channel->encoding == ChannelEncoding::Constant8 ||
        channel->encoding == ChannelEncoding::Linear16) {
        return {
            FixtureChannelCapabilityEditorError::InvalidChannel,
            false,
            false,
            "Choose one mapped 8-bit semantic channel before editing named ranges."};
    }
    if (definition.id.empty() || definition.name.empty() ||
        definition.id.size() > showcore::kFixtureProfileTextLength ||
        definition.name.size() > showcore::kFixtureProfileTextLength) {
        return {
            FixtureChannelCapabilityEditorError::InvalidIdentity,
            false,
            false,
            "Capability name and stable ID must be non-empty and at most 96 characters."};
    }
    if (definition.property >= Property::Count) {
        return {
            FixtureChannelCapabilityEditorError::InvalidProperty,
            false,
            false,
            "Choose the semantic parameter this DMX range represents."};
    }
    if (definition.dmx_min > definition.dmx_max) {
        return {
            FixtureChannelCapabilityEditorError::InvalidRange,
            false,
            false,
            "Capability DMX From must not exceed DMX To; use Reverse for opposite direction."};
    }
    if (definition.preferred_value < definition.dmx_min ||
        definition.preferred_value > definition.dmx_max) {
        return {
            FixtureChannelCapabilityEditorError::InvalidPreferredValue,
            false,
            false,
            "The preferred value must sit inside this capability's DMX range."};
    }
    const auto* descriptor = fixture_parameter_descriptor(definition.property);
    const auto protected_role =
        definition.role == FixtureChannelCapabilityRole::Reset ||
        definition.role == FixtureChannelCapabilityRole::Service;
    if (descriptor == nullptr ||
        (descriptor->safety_restricted() &&
         descriptor->safety != FixtureParameterSafety::UnverifiedCustom &&
         definition.access == showcore::ChannelCapabilityAccess::Selectable) ||
        (descriptor->safety == FixtureParameterSafety::UnverifiedCustom &&
         definition.access != showcore::ChannelCapabilityAccess::Protected) ||
        (protected_role &&
         definition.access != showcore::ChannelCapabilityAccess::Protected)) {
        return {
            FixtureChannelCapabilityEditorError::InvalidAccess,
            false,
            false,
            "Safety-restricted ranges must be gated; custom, reset, and service ranges stay Protected until explicitly qualified."};
    }

    auto candidate = draft;
    auto candidate_channel = std::find_if(
        candidate.channels.begin(),
        candidate.channels.end(),
        [one_based_channel](const auto& value) {
            return value.coarse_offset == one_based_channel - 1U;
        });
    const auto existing = std::find_if(
        candidate_channel->capabilities.begin(),
        candidate_channel->capabilities.end(),
        [&](const auto& value) { return value.id == definition.id; });
    const auto replaced = existing != candidate_channel->capabilities.end();
    for (const auto& capability : candidate_channel->capabilities) {
        if (capability.id == definition.id) {
            continue;
        }
        if (!(definition.dmx_max < capability.dmx_min ||
              definition.dmx_min > capability.dmx_max)) {
            return {
                FixtureChannelCapabilityEditorError::DuplicateRange,
                false,
                replaced,
                "Named DMX capability ranges cannot overlap. Adjust From/To or edit the existing row."};
        }
    }
    if (replaced) {
        *existing = definition;
    } else {
        candidate_channel->capabilities.push_back(definition);
    }
    std::stable_sort(
        candidate_channel->capabilities.begin(),
        candidate_channel->capabilities.end(),
        [](const auto& left, const auto& right) {
            if (left.dmx_min != right.dmx_min) {
                return left.dmx_min < right.dmx_min;
            }
            return left.id < right.id;
        });
    const auto first_property = candidate_channel->capabilities.front().property;
    const auto compound = std::any_of(
        candidate_channel->capabilities.begin(),
        candidate_channel->capabilities.end(),
        [first_property](const auto& capability) {
            return capability.property != first_property;
        });
    candidate_channel->property = compound ? Property::Count : first_property;
    if (compound) {
        candidate_channel->encoding = ChannelEncoding::Discrete8;
        candidate_channel->dmx_min = 0U;
        candidate_channel->dmx_max = 255U;
    }
    const auto summary = summarize_fixture_profile_mapping(validation_copy(candidate));
    if (!summary.profile_valid) {
        return {
            FixtureChannelCapabilityEditorError::ProfileInvalid,
            false,
            replaced,
            summary.validation_message};
    }
    draft = std::move(candidate);
    return {
        FixtureChannelCapabilityEditorError::None,
        true,
        replaced,
        replaced ? "Named DMX capability updated."
                 : "Named DMX capability added."};
}

FixtureChannelCapabilityMutationResult remove_fixture_channel_capability(
    FixtureProfileDefinition& draft,
    std::uint16_t one_based_channel,
    std::string_view capability_id) {
    auto candidate = draft;
    const auto channel = std::find_if(
        candidate.channels.begin(),
        candidate.channels.end(),
        [one_based_channel](const auto& value) {
            return one_based_channel != 0U &&
                value.coarse_offset == one_based_channel - 1U;
        });
    if (channel == candidate.channels.end()) {
        return {
            FixtureChannelCapabilityEditorError::InvalidChannel,
            false,
            false,
            "Choose a mapped channel first."};
    }
    const auto capability = std::find_if(
        channel->capabilities.begin(),
        channel->capabilities.end(),
        [capability_id](const auto& value) { return value.id == capability_id; });
    if (capability == channel->capabilities.end()) {
        return {
            FixtureChannelCapabilityEditorError::MissingCapability,
            false,
            false,
            "That named capability is no longer present."};
    }
    channel->capabilities.erase(capability);
    if (!channel->capabilities.empty()) {
        const auto first_property = channel->capabilities.front().property;
        const auto compound = std::any_of(
            channel->capabilities.begin(),
            channel->capabilities.end(),
            [first_property](const auto& value) {
                return value.property != first_property;
            });
        channel->property = compound ? Property::Count : first_property;
    } else if (channel->property == Property::Count) {
        channel->property = Property::Custom1;
    }
    const auto summary = summarize_fixture_profile_mapping(validation_copy(candidate));
    if (!summary.profile_valid) {
        return {
            FixtureChannelCapabilityEditorError::ProfileInvalid,
            false,
            false,
            summary.validation_message};
    }
    draft = std::move(candidate);
    return {
        FixtureChannelCapabilityEditorError::None,
        true,
        false,
        "Named DMX capability removed."};
}

std::vector<FixtureChannelCapabilityRow> fixture_channel_capability_rows(
    const FixtureProfileDefinition& profile,
    std::uint16_t one_based_channel) {
    std::vector<FixtureChannelCapabilityRow> rows;
    const auto channel = std::find_if(
        profile.channels.begin(),
        profile.channels.end(),
        [one_based_channel](const auto& value) {
            return one_based_channel != 0U &&
                value.coarse_offset == one_based_channel - 1U;
        });
    if (channel == profile.channels.end()) {
        return rows;
    }
    rows.reserve(channel->capabilities.size());
    for (std::size_t index = 0U; index < channel->capabilities.size(); ++index) {
        const auto& capability = channel->capabilities[index];
        FixtureChannelCapabilityRow row;
        row.source_index = index;
        row.id = capability.id;
        row.name = capability.name;
        row.parameter_label = display_property(capability.property);
        row.range_label = std::to_string(capability.dmx_min) + "–" +
            std::to_string(capability.dmx_max);
        row.preferred_label = std::to_string(capability.preferred_value);
        row.behavior_label = std::string(
            fixture_channel_capability_behavior_name(capability.behavior));
        row.access_label = std::string(
            fixture_channel_capability_access_name(capability.access));
        row.role_label = std::string(
            fixture_channel_capability_role_name(capability.role));
        row.accessibility_label = "DMX " + row.range_label + ", " + row.name +
            ", " + row.parameter_label + ", preferred " + row.preferred_label +
            ", " + row.behavior_label + ", " + row.access_label;
        rows.push_back(std::move(row));
    }
    return rows;
}

FixtureChannelCapabilitySelection resolve_fixture_channel_capability(
    const FixtureProfileDefinition& profile,
    std::uint16_t one_based_channel,
    std::string_view capability_id,
    float position) {
    FixtureChannelCapabilitySelection result;
    const auto channel = std::find_if(
        profile.channels.begin(),
        profile.channels.end(),
        [one_based_channel](const auto& value) {
            return one_based_channel != 0U &&
                value.coarse_offset == one_based_channel - 1U;
        });
    if (channel == profile.channels.end()) {
        result.error = FixtureChannelCapabilityEditorError::InvalidChannel;
        result.message = "The capability channel is missing.";
        return result;
    }
    const auto selected = std::find_if(
        channel->capabilities.begin(),
        channel->capabilities.end(),
        [capability_id](const auto& value) { return value.id == capability_id; });
    if (selected == channel->capabilities.end()) {
        result.message = "The named capability is missing.";
        return result;
    }
    if (selected->access == showcore::ChannelCapabilityAccess::Protected) {
        result.error = FixtureChannelCapabilityEditorError::ProtectedCapability;
        result.message = "Protected reset/service/custom ranges cannot be bound or activated.";
        return result;
    }
    position = std::isfinite(position) ? std::clamp(position, 0.0F, 1.0F) : 0.5F;
    std::size_t matching_count = 0U;
    std::size_t selected_segment = 0U;
    for (const auto& capability : channel->capabilities) {
        if (capability.property != selected->property ||
            capability.access == showcore::ChannelCapabilityAccess::Protected) {
            continue;
        }
        if (&capability == &*selected) {
            selected_segment = matching_count;
        }
        ++matching_count;
    }
    if (matching_count == 0U) {
        result.message = "The named capability has no selectable runtime range.";
        return result;
    }
    const auto local = selected->behavior == showcore::ChannelCapabilityBehavior::Slot
        ? 0.5F
        : position;
    result.error = FixtureChannelCapabilityEditorError::None;
    result.found = true;
    result.property = selected->property;
    result.semantic_min = static_cast<float>(selected_segment) /
        static_cast<float>(matching_count);
    result.semantic_max = static_cast<float>(selected_segment + 1U) /
        static_cast<float>(matching_count);
    result.normalized_value =
        (static_cast<float>(selected_segment) + local) /
        static_cast<float>(matching_count);
    result.raw_value = selected->behavior == showcore::ChannelCapabilityBehavior::Slot
        ? selected->preferred_value
        : static_cast<std::uint8_t>(std::lround(
              static_cast<float>(selected->dmx_min) +
              (selected->reversed ? 1.0F - position : position) *
                  static_cast<float>(selected->dmx_max - selected->dmx_min)));
    result.binding_id = profile.id + "/ch" +
        std::to_string(one_based_channel) + "/" + selected->id;
    result.message = "Named capability resolved through the shared semantic parameter contract.";
    return result;
}

FixtureProfileAudit audit_fixture_profile(
    const FixtureProfileDefinition& profile) {
    FixtureProfileAudit audit;
    const auto structure = summarize_fixture_profile_mapping(profile);
    audit.structurally_valid = structure.profile_valid;
    std::array<bool, showcore::kUniverseSlots> occupied{};
    std::array<std::size_t, showcore::kPropertyCount> property_uses{};
    std::vector<std::string> manual_rows;
    std::vector<std::string> missing_range_rows;
    std::vector<std::string> safety_rows;
    std::vector<std::string> repeated_rows;

    if (!structure.profile_valid) {
        audit.issues.push_back({
            FixtureProfileAuditSeverity::Error,
            "profile.structure.invalid",
            0U,
            structure.validation_message});
    }

    for (const auto& channel : profile.channels) {
        const auto one_based_channel = static_cast<std::uint16_t>(
            static_cast<std::size_t>(channel.coarse_offset) + 1U);
        if (channel.coarse_offset < occupied.size()) {
            occupied[channel.coarse_offset] = true;
        }
        if (channel.fine_offset >= 0 &&
            static_cast<std::size_t>(channel.fine_offset) < occupied.size()) {
            occupied[static_cast<std::size_t>(channel.fine_offset)] = true;
        }
        if (channel.owner != "fixture") {
            ++audit.owned_cell_or_head_count;
        }
        std::array<bool, showcore::kPropertyCount> capability_properties{};
        std::size_t distinct_capability_properties = 0U;
        for (const auto& capability : channel.capabilities) {
            ++audit.named_capability_count;
            if (capability.access == showcore::ChannelCapabilityAccess::Protected) {
                ++audit.protected_capability_count;
            }
            const auto property_index = static_cast<std::size_t>(capability.property);
            if (property_index < capability_properties.size() &&
                !capability_properties[property_index]) {
                capability_properties[property_index] = true;
                ++distinct_capability_properties;
                ++property_uses[property_index];
            }
        }
        if (distinct_capability_properties > 1U) {
            ++audit.compound_channel_count;
        }
        if (channel.encoding == ChannelEncoding::Constant8 ||
            (channel.property == Property::Count && channel.capabilities.empty())) {
            ++audit.safe_constant_count;
            continue;
        }
        ++audit.semantic_mapping_count;
        if (channel.property == Property::Count) {
            for (std::size_t property_index = 0U;
                 property_index < capability_properties.size();
                 ++property_index) {
                if (!capability_properties[property_index]) {
                    continue;
                }
                const auto property = static_cast<Property>(property_index);
                const auto* descriptor = fixture_parameter_descriptor(property);
                if (descriptor == nullptr) {
                    audit.issues.push_back({
                        FixtureProfileAuditSeverity::Error,
                        "profile.parameter.unknown",
                        one_based_channel,
                        "CH" + std::to_string(one_based_channel) +
                            " has an unknown named capability parameter."});
                    continue;
                }
                const auto row_label = std::string(descriptor->display_name) +
                    " CH" + std::to_string(one_based_channel);
                if (descriptor->needs_manual_dmx_chart()) {
                    ++audit.manual_chart_review_count;
                    manual_rows.push_back(row_label);
                }
                if (descriptor->safety != FixtureParameterSafety::Normal &&
                    descriptor->safety != FixtureParameterSafety::UnverifiedCustom) {
                    ++audit.safety_restricted_count;
                    safety_rows.push_back(row_label);
                }
                if (descriptor->category == FixtureParameterCategory::Custom) {
                    ++audit.custom_mapping_count;
                }
            }
            continue;
        }
        const auto* descriptor = fixture_parameter_descriptor(channel.property);
        if (descriptor == nullptr) {
            audit.issues.push_back({
                FixtureProfileAuditSeverity::Error,
                "profile.parameter.unknown",
                one_based_channel,
                "CH" + std::to_string(one_based_channel) +
                    " uses an unknown semantic parameter."});
            continue;
        }
        if (channel.capabilities.empty()) {
            ++property_uses[static_cast<std::size_t>(channel.property)];
        }
        const auto row_label = std::string(descriptor->display_name) + " CH" +
            std::to_string(one_based_channel);
        if (descriptor->needs_manual_dmx_chart()) {
            ++audit.manual_chart_review_count;
            manual_rows.push_back(row_label);
            if (channel.capabilities.empty()) {
                missing_range_rows.push_back(row_label);
            }
        }
        if (descriptor->safety != FixtureParameterSafety::Normal &&
            descriptor->safety != FixtureParameterSafety::UnverifiedCustom) {
            ++audit.safety_restricted_count;
            safety_rows.push_back(row_label);
        }
        if (descriptor->category == FixtureParameterCategory::Custom) {
            ++audit.custom_mapping_count;
        }
    }

    const auto bounded_footprint = std::min<std::size_t>(
        profile.footprint, occupied.size());
    audit.mapped_slot_count = static_cast<std::size_t>(std::count(
        occupied.begin(), occupied.begin() + bounded_footprint, true));
    audit.unmapped_slot_count = bounded_footprint - audit.mapped_slot_count;
    for (std::size_t index = 0U; index < property_uses.size(); ++index) {
        if (property_uses[index] <= 1U) {
            continue;
        }
        ++audit.repeated_semantic_count;
        const auto* descriptor = fixture_parameter_descriptor(
            static_cast<Property>(index));
        repeated_rows.push_back(
            (descriptor == nullptr ? std::string("Unknown")
                                   : std::string(descriptor->display_name)) +
            " ×" + std::to_string(property_uses[index]));
    }

    if (audit.unmapped_slot_count != 0U) {
        audit.issues.push_back({
            FixtureProfileAuditSeverity::Warning,
            "profile.slots.unmapped",
            0U,
            std::to_string(audit.unmapped_slot_count) +
                " DMX footprint slot(s) are not described. Add semantic rows or explicit safe constants from the fixture chart."});
    }
    if (!manual_rows.empty()) {
        audit.issues.push_back({
            FixtureProfileAuditSeverity::Warning,
            "profile.capabilities.manualReview",
            0U,
            "DMX-chart capability ranges required: " +
                joined_parameter_rows(manual_rows) + "."});
    }
    if (!missing_range_rows.empty()) {
        audit.issues.push_back({
            FixtureProfileAuditSeverity::Warning,
            "profile.capabilities.namedRangesMissing",
            0U,
            "Add named DMX capability ranges from the fixture chart: " +
                joined_parameter_rows(missing_range_rows) + "."});
    }
    if (audit.protected_capability_count != 0U) {
        audit.issues.push_back({
            FixtureProfileAuditSeverity::Info,
            "profile.capabilities.protected",
            0U,
            std::to_string(audit.protected_capability_count) +
                " reset/service/custom range(s) are preserved but unavailable to Looks, Live, MIDI, and skins."});
    }
    if (!safety_rows.empty()) {
        audit.issues.push_back({
            FixtureProfileAuditSeverity::Warning,
            "profile.parameters.safetyRestricted",
            0U,
            "Safety-restricted parameters: " + joined_parameter_rows(safety_rows) +
                ". Runtime arming/caps still apply."});
    }
    if (!repeated_rows.empty()) {
        audit.issues.push_back({
            FixtureProfileAuditSeverity::Info,
            "profile.parameters.repeated",
            0U,
            "Repeated semantic parameters move together in the current profile model: " +
                joined_parameter_rows(repeated_rows) + "."});
    }
    if (audit.custom_mapping_count != 0U) {
        audit.issues.push_back({
            FixtureProfileAuditSeverity::Warning,
            "profile.parameters.customUnverified",
            0U,
            std::to_string(audit.custom_mapping_count) +
                " Custom parameter row(s) remain unclassified and require manual review."});
    }

    audit.structurally_complete = audit.structurally_valid &&
        audit.unmapped_slot_count == 0U;
    std::ostringstream text;
    text << "PARAMETER AUDIT\n"
         << "DMX slots described: " << audit.mapped_slot_count << '/'
         << profile.footprint << " | Semantic rows: "
         << audit.semantic_mapping_count << " | Safe constants: "
         << audit.safe_constant_count << '\n'
         << "DMX-chart ranges: " << audit.manual_chart_review_count
         << " | Safety-restricted: " << audit.safety_restricted_count
         << " | Custom: " << audit.custom_mapping_count << '\n'
         << "Named ranges: " << audit.named_capability_count
         << " | Compound channels: " << audit.compound_channel_count
         << " | Protected: " << audit.protected_capability_count
         << " | Cell/head-owned: " << audit.owned_cell_or_head_count << '\n';
    if (audit.issues.empty()) {
        text << "Structure is complete. Physical mode, source, and emitter behavior still require fixture verification.";
    } else {
        for (const auto& issue : audit.issues) {
            text << (issue.severity == FixtureProfileAuditSeverity::Error
                         ? "ERROR: "
                         : issue.severity == FixtureProfileAuditSeverity::Warning
                             ? "REVIEW: "
                             : "INFO: ")
                 << issue.message << '\n';
        }
    }
    audit.text = text.str();
    return audit;
}

FixtureProfileRebindResult rebind_fixture_profile_instances(
    ProjectDocument& project,
    std::string_view source_profile_id,
    std::string_view replacement_profile_id) {
    FixtureProfileRebindResult result;
    if (source_profile_id == replacement_profile_id) {
        result.error = FixtureProfileRebindError::SameProfile;
        result.message = "Source and replacement profile IDs must be different; nothing changed.";
        return result;
    }
    const auto profile_count = [&](std::string_view id) {
        return static_cast<std::size_t>(std::count_if(
            project.fixture_profiles.begin(), project.fixture_profiles.end(),
            [id](const auto& profile) { return profile.id == id; }));
    };
    if (profile_count(source_profile_id) != 1U) {
        result.error = FixtureProfileRebindError::InvalidSource;
        result.message = "The source fixture profile is missing or ambiguous; nothing changed.";
        return result;
    }
    if (profile_count(replacement_profile_id) != 1U) {
        result.error = FixtureProfileRebindError::InvalidReplacement;
        result.message = "The replacement fixture profile is missing or ambiguous; nothing changed.";
        return result;
    }

    auto candidate = project;
    for (auto& fixture : candidate.fixtures) {
        if (fixture.profile_id != source_profile_id) {
            continue;
        }
        fixture.profile_id = std::string(replacement_profile_id);
        result.fixture_ids.push_back(fixture.id);
    }
    if (result.fixture_ids.empty()) {
        result.message = "No patched fixture uses the source profile; nothing changed.";
        return result;
    }
    const auto validation = validate_project(candidate);
    if (!validation.ok()) {
        result.error = FixtureProfileRebindError::InvalidCandidate;
        result.fixture_ids.clear();
        result.message = "Rebinding would make the fixture patch invalid; nothing changed.";
        return result;
    }
    const auto compilation = compile_project_with_persisted_autoloops(candidate);
    if (!compilation) {
        result.error = FixtureProfileRebindError::CompilationFailed;
        result.fixture_ids.clear();
        result.message = "The rebound project did not compile; nothing changed.";
        return result;
    }
    project = std::move(candidate);
    result.changed = true;
    result.message = std::to_string(result.fixture_ids.size()) +
        " patched fixture" +
        (result.fixture_ids.size() == 1U ? std::string{} : std::string("s")) +
        " now uses the replacement profile.";
    return result;
}

FixtureProfileWhiteAmberAssignmentPlanResult
plan_fixture_profile_white_amber_assignment(
    const ProjectDocument& project,
    std::string_view profile_id,
    std::uint16_t desired_white_channel,
    std::uint16_t desired_amber_channel) {
    FixtureProfileWhiteAmberAssignmentPlanResult result;
    const auto profile = std::find_if(
        project.fixture_profiles.begin(), project.fixture_profiles.end(),
        [profile_id](const auto& candidate) {
            return candidate.id == profile_id;
        });
    if (profile == project.fixture_profiles.end()) {
        result.error = FixtureProfileWhiteAmberAssignmentError::InvalidProfile;
        result.message = "The selected fixture profile no longer exists.";
        return result;
    }
    result.current_mapping = summarize_fixture_profile_mapping(*profile);
    if (!result.current_mapping.profile_valid) {
        result.error = FixtureProfileWhiteAmberAssignmentError::InvalidProfile;
        result.message = result.current_mapping.validation_message;
        return result;
    }
    if (desired_white_channel == 0U || desired_amber_channel == 0U ||
        desired_white_channel == desired_amber_channel ||
        desired_white_channel > profile->footprint ||
        desired_amber_channel > profile->footprint) {
        result.error = FixtureProfileWhiteAmberAssignmentError::InvalidSelection;
        result.message =
            "White and Amber must be two different channels inside the profile footprint.";
        return result;
    }
    if (result.current_mapping.white_mapping_count != 1U ||
        result.current_mapping.amber_mapping_count != 1U) {
        result.error = FixtureProfileWhiteAmberAssignmentError::MappingUnavailable;
        result.message =
            "The profile needs exactly one White mapping and one Amber mapping before calibration.";
        return result;
    }
    const auto current_white = result.current_mapping.white_channel;
    const auto current_amber = result.current_mapping.amber_channel;
    const auto desired_is_current_pair =
        (desired_white_channel == current_white &&
         desired_amber_channel == current_amber) ||
        (desired_white_channel == current_amber &&
         desired_amber_channel == current_white);
    if (!desired_is_current_pair) {
        result.error =
            FixtureProfileWhiteAmberAssignmentError::SelectionIsNotWhiteAmberPair;
        result.message =
            "This calibration only re-labels the profile's existing White/Amber pair (CH" +
            std::to_string(current_white) + " and CH" +
            std::to_string(current_amber) +
            "). Use the channel table to map a different physical slot.";
        return result;
    }
    if (desired_white_channel == current_white &&
        desired_amber_channel == current_amber) {
        result.error = FixtureProfileWhiteAmberAssignmentError::None;
        result.already_assigned = true;
        result.message = "Already assigned: White is CH" +
            std::to_string(current_white) + " and Amber is CH" +
            std::to_string(current_amber) + ". Nothing changed.";
        return result;
    }

    const auto planned = plan_fixture_profile_white_amber_correction(
        project, profile_id);
    if (!planned ||
        planned.plan.white_channel_after != desired_white_channel ||
        planned.plan.amber_channel_after != desired_amber_channel) {
        result.error = FixtureProfileWhiteAmberAssignmentError::CorrectionUnavailable;
        result.message = planned.message.empty()
            ? "The requested White/Amber assignment could not be planned safely."
            : planned.message;
        return result;
    }
    result.error = FixtureProfileWhiteAmberAssignmentError::None;
    result.plan = planned.plan;
    result.message = "Ready to assign White to CH" +
        std::to_string(desired_white_channel) + " and Amber to CH" +
        std::to_string(desired_amber_channel) + ".";
    return result;
}

}  // namespace emberlights
