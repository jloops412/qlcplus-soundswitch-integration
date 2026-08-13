#include "emberlights/fixture_profile_editor.hpp"
#include "emberlights/compiler.hpp"

#include <algorithm>
#include <array>
#include <sstream>
#include <utility>

namespace emberlights {
namespace {

using showcore::ChannelEncoding;
using showcore::Property;

const std::array<FixtureProfileTemplateDescriptor, 6U> kTemplates{{
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
    }
    return {};
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
        *existing = definition;
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
        std::ostringstream accessible;
        accessible << "Channel " << row.channel << ", " << row.property_label
                   << ", " << row.encoding_label << ", DMX "
                   << row.range_label << ", default " << row.default_label;
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

FixtureProfileAudit audit_fixture_profile(
    const FixtureProfileDefinition& profile) {
    FixtureProfileAudit audit;
    const auto structure = summarize_fixture_profile_mapping(profile);
    audit.structurally_valid = structure.profile_valid;
    std::array<bool, showcore::kUniverseSlots> occupied{};
    std::array<std::size_t, showcore::kPropertyCount> property_uses{};
    std::vector<std::string> manual_rows;
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
        if (channel.encoding == ChannelEncoding::Constant8 ||
            channel.property == Property::Count) {
            ++audit.safe_constant_count;
            continue;
        }
        ++audit.semantic_mapping_count;
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
        ++property_uses[static_cast<std::size_t>(channel.property)];
        const auto row_label = std::string(descriptor->display_name) + " CH" +
            std::to_string(one_based_channel);
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
         << " | Custom: " << audit.custom_mapping_count << '\n';
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
