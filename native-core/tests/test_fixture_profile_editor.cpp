#include "emberlights/fixture_profile_editor.hpp"
#include "emberlights/compiler.hpp"
#include "emberlights/project_io.hpp"
#include "showcore/engine.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <string_view>

namespace {

int failures = 0;

void check(bool condition, std::string_view expression, int line) {
    if (condition) {
        return;
    }
    ++failures;
    std::cerr << "FAIL line " << line << ": " << expression << '\n';
}

#define CHECK(expression) check((expression), #expression, __LINE__)

emberlights::ProjectDocument project_with_ir4() {
    auto project = emberlights::make_starter_project();
    emberlights::FixtureDefinition fixture;
    fixture.id = "fixture.ir4";
    fixture.name = "IR-4";
    fixture.profile_id = std::string(
        emberlights::kBothLightingBoIr4SixChannelProfileId);
    fixture.universe = 1U;
    fixture.address = 1U;
    project.fixtures.push_back(fixture);
    return project;
}

std::string profile_snapshot(
    const emberlights::FixtureProfileDefinition& profile) {
    return profile.id + "|" + profile.name + "|" + profile.source_revision +
        "|" + std::to_string(static_cast<unsigned int>(profile.source)) +
        "|" + emberlights::fixture_profile_behavior_fingerprint(profile);
}

const emberlights::ChannelDefinition* channel_at(
    const emberlights::FixtureProfileDefinition& profile,
    std::uint16_t one_based_channel) {
    const auto found = std::find_if(
        profile.channels.begin(), profile.channels.end(),
        [one_based_channel](const auto& channel) {
            return one_based_channel != 0U &&
                channel.coarse_offset == one_based_channel - 1U;
        });
    return found == profile.channels.end() ? nullptr : &*found;
}

void test_templates_and_rows() {
    const auto templates = emberlights::fixture_profile_templates();
    CHECK(templates.size() == 10U);
    CHECK(templates[4].stable_id == "rgbwauv-6ch");
    CHECK(templates[6].stable_id == "rgbwa-5ch");
    CHECK(templates[9].stable_id == "pan-tilt-2ch");

    emberlights::FixtureProfileDefinition draft;
    draft.name = "Draft";
    const auto applied = emberlights::apply_fixture_profile_template(
        draft, emberlights::FixtureProfileTemplateId::Rgbwauv6);
    CHECK(static_cast<bool>(applied));
    CHECK(draft.footprint == 6U);
    CHECK(draft.channels.size() == 6U);
    CHECK(draft.channels[3].property == showcore::Property::White);
    CHECK(draft.channels[4].property == showcore::Property::Amber);

    const auto rows = emberlights::fixture_profile_editor_rows(draft);
    CHECK(rows.size() == 6U);
    CHECK(rows[3].channel == 4U);
    CHECK(rows[3].property_label == "White");
    CHECK(rows[4].accessibility_label.find("Channel 5, Amber") !=
          std::string::npos);

    const auto movement = emberlights::apply_fixture_profile_template(
        draft, emberlights::FixtureProfileTemplateId::PanTilt2);
    CHECK(static_cast<bool>(movement));
    CHECK(draft.footprint == 2U);
    CHECK(draft.channels[0].property == showcore::Property::Pan);
    CHECK(draft.channels[1].property == showcore::Property::Tilt);
}

void test_parameter_catalog_contract() {
    const auto catalog = emberlights::fixture_parameter_catalog();
    CHECK(catalog.size() == showcore::kPropertyCount);
    std::set<std::string_view> stable_ids;
    for (std::size_t index = 0U; index < catalog.size(); ++index) {
        const auto property = static_cast<showcore::Property>(index);
        const auto& descriptor = catalog[index];
        CHECK(descriptor.property == property);
        CHECK(descriptor.stable_id == emberlights::property_name(property));
        CHECK(!descriptor.display_name.empty());
        CHECK(!descriptor.description.empty());
        CHECK(stable_ids.insert(descriptor.stable_id).second);
        CHECK(descriptor.supports(
            emberlights::FixtureParameterSurface::Profile));
        CHECK(descriptor.supports(
            emberlights::FixtureParameterSurface::StaticLook));
        CHECK(descriptor.supports(
            emberlights::FixtureParameterSurface::Autoloop));
        CHECK(descriptor.supports(
            emberlights::FixtureParameterSurface::LiveOverride));
        CHECK(descriptor.supports(
            emberlights::FixtureParameterSurface::Controller));
    }
    const auto* white = emberlights::fixture_parameter_descriptor(
        showcore::Property::White);
    CHECK(white != nullptr);
    CHECK(white->category == emberlights::FixtureParameterCategory::Color);
    CHECK(!white->needs_manual_dmx_chart());
    const auto* strobe = emberlights::fixture_parameter_descriptor("strobe");
    CHECK(strobe != nullptr);
    CHECK(strobe->needs_manual_dmx_chart());
    CHECK(strobe->safety ==
          emberlights::FixtureParameterSafety::StrobeCapped);
    CHECK(emberlights::fixture_parameter_descriptor(
              showcore::Property::Count) == nullptr);
}

void test_picker_choices_and_assign_next() {
    const auto choices = emberlights::fixture_profile_parameter_choices();
    CHECK(choices.size() == showcore::kPropertyCount);
    for (std::size_t index = 0U; index < choices.size(); ++index) {
        CHECK(choices[index].property ==
              static_cast<showcore::Property>(index));
        CHECK(!choices[index].stable_id.empty());
        CHECK(!choices[index].display_name.empty());
        CHECK(!choices[index].accessibility_label.empty());
    }
    const auto& white_choice =
        choices[static_cast<std::size_t>(showcore::Property::White)];
    CHECK(white_choice.direct_assignment_available);
    CHECK(white_choice.stable_id == "white");
    const auto& strobe_choice =
        choices[static_cast<std::size_t>(showcore::Property::Strobe)];
    CHECK(!strobe_choice.direct_assignment_available);
    CHECK(strobe_choice.unavailable_reason.find("fixture DMX chart") !=
          std::string::npos);

    emberlights::FixtureProfileDefinition draft;
    draft.id = "local.assign-next";
    draft.name = "Assign next";
    draft.footprint = 4U;
    draft.channels.push_back({showcore::Property::Blue, 2U});
    draft.channels.push_back({showcore::Property::Red, 0U});

    const auto green =
        emberlights::assign_next_or_append_fixture_profile_channel(
            draft, showcore::Property::Green);
    CHECK(static_cast<bool>(green));
    CHECK(green.changed);
    CHECK(green.filled_gap);
    CHECK(!green.grew_footprint);
    CHECK(green.channel == 2U);
    CHECK(draft.footprint == 4U);
    CHECK(channel_at(draft, 2U) != nullptr);
    CHECK(channel_at(draft, 2U)->property == showcore::Property::Green);

    const auto white =
        emberlights::assign_next_or_append_fixture_profile_channel(
            draft, showcore::Property::White);
    CHECK(static_cast<bool>(white));
    CHECK(white.channel == 4U);
    CHECK(white.filled_gap);
    CHECK(!white.grew_footprint);

    const auto amber =
        emberlights::assign_next_or_append_fixture_profile_channel(
            draft, showcore::Property::Amber);
    CHECK(static_cast<bool>(amber));
    CHECK(amber.channel == 5U);
    CHECK(!amber.filled_gap);
    CHECK(amber.grew_footprint);
    CHECK(draft.footprint == 5U);

    const auto rows = emberlights::fixture_profile_editor_rows(draft);
    CHECK(rows.size() == 5U);
    for (std::size_t index = 0U; index < rows.size(); ++index) {
        CHECK(rows[index].channel == index + 1U);
    }

    const auto before_unsafe = profile_snapshot(draft);
    const auto unsafe =
        emberlights::assign_next_or_append_fixture_profile_channel(
            draft, showcore::Property::Strobe);
    CHECK(!static_cast<bool>(unsafe));
    CHECK(unsafe.error ==
          emberlights::FixtureProfileChannelPlacementError::UnsafePreset);
    CHECK(profile_snapshot(draft) == before_unsafe);

    auto immutable = draft;
    immutable.source = showcore::FixtureProfileSource::BuiltIn;
    const auto immutable_snapshot = profile_snapshot(immutable);
    const auto read_only =
        emberlights::assign_next_or_append_fixture_profile_channel(
            immutable, showcore::Property::UV);
    CHECK(!static_cast<bool>(read_only));
    CHECK(read_only.error ==
          emberlights::FixtureProfileChannelPlacementError::SourceReadOnly);
    CHECK(profile_snapshot(immutable) == immutable_snapshot);
}

void test_fill_gaps_respects_fine_slots_and_is_atomic() {
    emberlights::FixtureProfileDefinition draft;
    draft.id = "local.fill-gaps";
    draft.name = "Fill gaps";
    draft.footprint = 5U;
    draft.channels.push_back({
        showcore::Property::Blue,
        3U,
        -1,
        showcore::ChannelEncoding::Linear8});
    draft.channels.push_back({
        showcore::Property::Pan,
        0U,
        1,
        showcore::ChannelEncoding::Linear16});

    const auto filled =
        emberlights::fill_fixture_profile_channel_gaps_with_safe_constants(
            draft);
    CHECK(static_cast<bool>(filled));
    CHECK(filled.changed);
    CHECK(filled.filled_count == 2U);
    CHECK(draft.channels.size() == 4U);
    CHECK(channel_at(draft, 3U) != nullptr);
    CHECK(channel_at(draft, 3U)->property == showcore::Property::Count);
    CHECK(channel_at(draft, 3U)->encoding ==
          showcore::ChannelEncoding::Constant8);
    CHECK(channel_at(draft, 5U) != nullptr);
    CHECK(channel_at(draft, 5U)->property == showcore::Property::Count);
    CHECK(channel_at(draft, 2U) == nullptr);
    const auto rows = emberlights::fixture_profile_editor_rows(draft);
    CHECK(rows.size() == 4U);
    CHECK(rows[0].channel == 1U);
    CHECK(rows[0].fine_channel == 2U);
    CHECK(rows[1].channel == 3U);
    CHECK(rows[2].channel == 4U);
    CHECK(rows[3].channel == 5U);
    const auto audit = emberlights::audit_fixture_profile(draft);
    CHECK(audit.structurally_valid);
    CHECK(audit.structurally_complete);
    CHECK(audit.mapped_slot_count == 5U);

    const auto complete_snapshot = profile_snapshot(draft);
    const auto no_op =
        emberlights::fill_fixture_profile_channel_gaps_with_safe_constants(
            draft);
    CHECK(static_cast<bool>(no_op));
    CHECK(!no_op.changed);
    CHECK(no_op.filled_count == 0U);
    CHECK(profile_snapshot(draft) == complete_snapshot);

    auto invalid = draft;
    invalid.channels.push_back({showcore::Property::Red, 0U});
    const auto invalid_snapshot = profile_snapshot(invalid);
    const auto rejected =
        emberlights::fill_fixture_profile_channel_gaps_with_safe_constants(
            invalid);
    CHECK(!static_cast<bool>(rejected));
    CHECK(rejected.error ==
          emberlights::FixtureProfileChannelPlacementError::CandidateInvalid);
    CHECK(profile_snapshot(invalid) == invalid_snapshot);
}

void test_structured_channel_mutations() {
    emberlights::FixtureProfileDefinition draft;
    draft.name = "Draft";
    CHECK(static_cast<bool>(emberlights::apply_fixture_profile_template(
        draft, emberlights::FixtureProfileTemplateId::Rgb3)));

    emberlights::ChannelDefinition white;
    const auto preset = emberlights::make_safe_fixture_profile_channel(
        showcore::Property::White, 4U, white);
    CHECK(static_cast<bool>(preset));
    draft.footprint = 4U;
    const auto added = emberlights::upsert_fixture_profile_channel(draft, white);
    CHECK(static_cast<bool>(added));
    CHECK(!added.replaced);
    CHECK(draft.channels.size() == 4U);

    emberlights::ChannelDefinition amber;
    CHECK(static_cast<bool>(emberlights::make_safe_fixture_profile_channel(
        showcore::Property::Amber, 4U, amber)));
    const auto replaced = emberlights::upsert_fixture_profile_channel(draft, amber);
    CHECK(static_cast<bool>(replaced));
    CHECK(replaced.replaced);
    CHECK(draft.channels.back().property == showcore::Property::Amber);

    const auto removed = emberlights::remove_fixture_profile_channel(draft, 4U);
    CHECK(static_cast<bool>(removed));
    CHECK(draft.channels.size() == 3U);

    emberlights::ChannelDefinition strobe;
    const auto unsafe = emberlights::make_safe_fixture_profile_channel(
        showcore::Property::Strobe, 4U, strobe);
    CHECK(!static_cast<bool>(unsafe));
    CHECK(unsafe.error == emberlights::FixtureProfileEditorError::UnsafePreset);
}

void test_profile_parameter_audit() {
    emberlights::FixtureProfileDefinition draft;
    draft.name = "Audit";
    CHECK(static_cast<bool>(emberlights::apply_fixture_profile_template(
        draft, emberlights::FixtureProfileTemplateId::Rgbw4)));
    auto audit = emberlights::audit_fixture_profile(draft);
    CHECK(audit.structurally_valid);
    CHECK(audit.structurally_complete);
    CHECK(audit.mapped_slot_count == 4U);
    CHECK(audit.unmapped_slot_count == 0U);
    CHECK(audit.semantic_mapping_count == 4U);
    CHECK(audit.manual_chart_review_count == 0U);
    CHECK(audit.text.find("PARAMETER AUDIT") != std::string::npos);

    draft.footprint = 5U;
    audit = emberlights::audit_fixture_profile(draft);
    CHECK(audit.structurally_valid);
    CHECK(!audit.structurally_complete);
    CHECK(audit.unmapped_slot_count == 1U);

    emberlights::ChannelDefinition strobe{
        showcore::Property::Strobe,
        4U,
        -1,
        showcore::ChannelEncoding::Ranged8,
        16U,
        255U,
        0U};
    CHECK(static_cast<bool>(
        emberlights::upsert_fixture_profile_channel(draft, strobe)));
    audit = emberlights::audit_fixture_profile(draft);
    CHECK(audit.structurally_complete);
    CHECK(audit.manual_chart_review_count == 1U);
    CHECK(audit.safety_restricted_count == 1U);
    CHECK(audit.text.find("Strobe CH5") != std::string::npos);
}

void test_named_channel_capabilities_and_bindings() {
    emberlights::FixtureProfileDefinition draft;
    draft.id = "local.shutter.test";
    draft.name = "Structured shutter";
    draft.footprint = 1U;
    draft.channels.push_back({
        showcore::Property::Shutter,
        0U,
        -1,
        showcore::ChannelEncoding::Discrete8,
        0U,
        255U,
        5U});

    emberlights::ChannelCapabilityDefinition open;
    open.id = "open";
    open.name = "Open";
    open.property = showcore::Property::Shutter;
    open.dmx_min = 0U;
    open.dmx_max = 9U;
    open.preferred_value = 5U;
    open.role = emberlights::FixtureChannelCapabilityRole::Open;
    CHECK(static_cast<bool>(emberlights::upsert_fixture_channel_capability(
        draft, 1U, open)));

    emberlights::ChannelCapabilityDefinition strobe;
    strobe.id = "strobe-slow-fast";
    strobe.name = "Strobe slow to fast";
    strobe.property = showcore::Property::Strobe;
    strobe.dmx_min = 10U;
    strobe.dmx_max = 199U;
    strobe.preferred_value = 64U;
    strobe.behavior = showcore::ChannelCapabilityBehavior::Continuous;
    strobe.access = showcore::ChannelCapabilityAccess::SafetyGated;
    CHECK(static_cast<bool>(emberlights::upsert_fixture_channel_capability(
        draft, 1U, strobe)));

    CHECK(draft.channels[0].property == showcore::Property::Count);
    CHECK(draft.channels[0].capabilities.size() == 2U);
    CHECK(static_cast<bool>(emberlights::update_fixture_profile_channel_metadata(
        draft, 1U, "head.1", 5U, 199U)));
    CHECK(draft.channels[0].owner == "head.1");
    CHECK(draft.channels[0].blackout_value == 5U);
    CHECK(draft.channels[0].highlight_value == 199U);
    CHECK(emberlights::make_fixture_channel_capability_id(
              "Strobe slow → fast") == "strobe-slow-fast");

    auto overlap = strobe;
    overlap.id = "overlap";
    overlap.name = "Overlap";
    overlap.dmx_min = 190U;
    overlap.dmx_max = 220U;
    CHECK(!static_cast<bool>(emberlights::upsert_fixture_channel_capability(
        draft, 1U, overlap)));

    emberlights::ChannelCapabilityDefinition reset;
    reset.id = "factory-reset";
    reset.name = "Factory reset";
    reset.property = showcore::Property::Custom1;
    reset.dmx_min = 200U;
    reset.dmx_max = 209U;
    reset.preferred_value = 205U;
    reset.access = showcore::ChannelCapabilityAccess::Protected;
    reset.role = emberlights::FixtureChannelCapabilityRole::Reset;
    CHECK(static_cast<bool>(emberlights::upsert_fixture_channel_capability(
        draft, 1U, reset)));

    const auto rows = emberlights::fixture_channel_capability_rows(draft, 1U);
    CHECK(rows.size() == 3U);
    CHECK(rows[1].range_label == "10–199");
    CHECK(rows[2].access_label == "Protected / unavailable");

    const auto open_binding = emberlights::resolve_fixture_channel_capability(
        draft, 1U, "open");
    CHECK(static_cast<bool>(open_binding));
    CHECK(open_binding.property == showcore::Property::Shutter);
    CHECK(open_binding.raw_value == 5U);
    CHECK(open_binding.binding_id == "local.shutter.test/ch1/open");

    const auto strobe_binding = emberlights::resolve_fixture_channel_capability(
        draft, 1U, "strobe-slow-fast", 1.0F);
    CHECK(static_cast<bool>(strobe_binding));
    CHECK(strobe_binding.property == showcore::Property::Strobe);
    CHECK(strobe_binding.raw_value == 199U);
    CHECK(strobe_binding.semantic_min == 0.0F);
    CHECK(strobe_binding.semantic_max == 1.0F);

    const auto protected_binding =
        emberlights::resolve_fixture_channel_capability(
            draft, 1U, "factory-reset");
    CHECK(!static_cast<bool>(protected_binding));
    CHECK(protected_binding.error ==
          emberlights::FixtureChannelCapabilityEditorError::ProtectedCapability);

    const auto audit = emberlights::audit_fixture_profile(draft);
    CHECK(audit.structurally_valid);
    CHECK(audit.named_capability_count == 3U);
    CHECK(audit.protected_capability_count == 1U);
    CHECK(audit.compound_channel_count == 1U);
}

void test_named_channel_capability_rendering() {
    constexpr std::array<showcore::ChannelCapabilityMapping, 3U> capabilities{{
        {showcore::Property::Shutter, 0U, 9U, 5U,
         showcore::ChannelCapabilityBehavior::Slot,
         showcore::ChannelCapabilityAccess::Selectable, false},
        {showcore::Property::Strobe, 10U, 199U, 64U,
         showcore::ChannelCapabilityBehavior::Continuous,
         showcore::ChannelCapabilityAccess::SafetyGated, false},
        {showcore::Property::Custom1, 200U, 209U, 205U,
         showcore::ChannelCapabilityBehavior::Slot,
         showcore::ChannelCapabilityAccess::Protected, false},
    }};
    const std::array<showcore::ChannelMapping, 1U> mappings{{
        {showcore::Property::Count,
         0U,
         -1,
         showcore::ChannelEncoding::Discrete8,
         0U,
         255U,
         5U,
         0U,
         255U,
         capabilities.data(),
         capabilities.size()},
    }};
    const showcore::FixtureProfile profile{
        "Compound shutter", mappings.data(), mappings.size(), 1U};
    CHECK(static_cast<bool>(showcore::validate_fixture_profile(profile)));

    showcore::Engine engine;
    CHECK(static_cast<bool>(engine.patch().add({0U, 0U, 1U, &profile})));
    engine.tick();
    CHECK(engine.frames().universes[0][0] == 5U);

    engine.layers().set(
        showcore::LayerId::ManualOverride,
        0U,
        showcore::Property::Shutter,
        showcore::PropertyValue::set(1.0F));
    engine.tick();
    CHECK(engine.frames().universes[0][0] == 5U);

    engine.layers().clear_layer(showcore::LayerId::ManualOverride);
    engine.layers().set(
        showcore::LayerId::ManualOverride,
        0U,
        showcore::Property::Strobe,
        showcore::PropertyValue::set(1.0F));
    engine.tick();
    CHECK(engine.frames().universes[0][0] == 199U);

    engine.safety().strobe_allowed = false;
    engine.tick();
    CHECK(engine.frames().universes[0][0] == 0U);

    engine.safety().strobe_allowed = true;
    engine.layers().set(
        showcore::LayerId::ManualOverride,
        0U,
        showcore::Property::Shutter,
        showcore::PropertyValue::set(1.0F));
    engine.tick();
    CHECK(engine.frames().universes[0][0] == 0U);
}

void test_named_channel_capability_project_round_trip() {
    auto project = emberlights::make_starter_project();
    emberlights::FixtureProfileDefinition profile;
    profile.id = "local.compound.roundtrip";
    profile.manufacturer = "Test";
    profile.model = "Compound";
    profile.mode = "1CH";
    profile.name = "Compound 1CH";
    profile.footprint = 1U;
    emberlights::ChannelDefinition channel{
        showcore::Property::Shutter,
        0U,
        -1,
        showcore::ChannelEncoding::Discrete8,
        0U,
        255U,
        7U,
        3U,
        200U,
        "head.1"};
    emberlights::ChannelCapabilityDefinition capability;
    capability.id = "open";
    capability.name = "Open";
    capability.property = showcore::Property::Shutter;
    capability.dmx_min = 4U;
    capability.dmx_max = 9U;
    capability.preferred_value = 7U;
    capability.role = emberlights::FixtureChannelCapabilityRole::Open;
    channel.capabilities.push_back(capability);
    profile.channels.push_back(channel);
    project.fixture_profiles.push_back(profile);

    const auto serialized = emberlights::serialize_project(project);
    CHECK(serialized.find("CHANNEL_META_V1") != std::string::npos);
    CHECK(serialized.find("CHANNEL_CAPABILITY_V1") != std::string::npos);
    emberlights::ProjectDocument parsed;
    CHECK(static_cast<bool>(emberlights::parse_project(serialized, parsed)));
    const auto found = std::find_if(
        parsed.fixture_profiles.begin(),
        parsed.fixture_profiles.end(),
        [](const auto& candidate) {
            return candidate.id == "local.compound.roundtrip";
        });
    CHECK(found != parsed.fixture_profiles.end());
    if (found == parsed.fixture_profiles.end()) {
        return;
    }
    CHECK(found->channels.size() == 1U);
    const auto& parsed_channel = found->channels[0];
    CHECK(parsed_channel.blackout_value == 3U);
    CHECK(parsed_channel.highlight_value == 200U);
    CHECK(parsed_channel.owner == "head.1");
    CHECK(parsed_channel.capabilities.size() == 1U);
    CHECK(parsed_channel.capabilities[0].id == "open");
    CHECK(parsed_channel.capabilities[0].role ==
          emberlights::FixtureChannelCapabilityRole::Open);
}

void test_generic_profile_rebind_transaction() {
    auto project = project_with_ir4();
    const auto source_id = project.fixtures.front().profile_id;
    const auto source = std::find_if(
        project.fixture_profiles.begin(), project.fixture_profiles.end(),
        [&](const auto& profile) { return profile.id == source_id; });
    CHECK(source != project.fixture_profiles.end());
    if (source == project.fixture_profiles.end()) {
        return;
    }
    auto replacement = *source;
    replacement.id = "local.ir4.corrected";
    replacement.name = "IR-4 corrected local";
    replacement.source = showcore::FixtureProfileSource::Local;
    replacement.source_revision = "test";
    std::swap(replacement.channels[3].property, replacement.channels[4].property);
    project.fixture_profiles.push_back(replacement);

    const auto rebound = emberlights::rebind_fixture_profile_instances(
        project, source_id, replacement.id);
    CHECK(static_cast<bool>(rebound));
    CHECK(rebound.changed);
    CHECK(rebound.fixture_ids.size() == 1U);
    CHECK(project.fixtures.front().profile_id == replacement.id);

    const auto snapshot = project.fixtures.front().profile_id;
    const auto invalid = emberlights::rebind_fixture_profile_instances(
        project, replacement.id, "missing.profile");
    CHECK(!static_cast<bool>(invalid));
    CHECK(invalid.error ==
          emberlights::FixtureProfileRebindError::InvalidReplacement);
    CHECK(project.fixtures.front().profile_id == snapshot);
}

void test_general_channel_function_swap_ir4_output_semantics() {
    auto profile = emberlights::make_both_lighting_bo_ir4_6ch_profile();
    profile.id = "local.ir4.function-swap";
    profile.name = "IR-4 local function correction";
    profile.source = showcore::FixtureProfileSource::Local;
    profile.source_revision = "operator-draft-1";
    emberlights::ChannelCapabilityDefinition red_level;
    red_level.id = "red-level";
    red_level.name = "Red level";
    red_level.property = showcore::Property::Red;
    red_level.preferred_value = 255U;
    red_level.behavior = showcore::ChannelCapabilityBehavior::Continuous;
    profile.channels[0].capabilities.push_back(red_level);
    const auto* white_before = channel_at(profile, 4U);
    const auto* amber_before = channel_at(profile, 5U);
    CHECK(white_before != nullptr);
    CHECK(amber_before != nullptr);
    if (white_before == nullptr || amber_before == nullptr) {
        return;
    }
    const auto white_owner = white_before->owner;
    const auto amber_owner = amber_before->owner;
    const auto white_default = white_before->default_value;
    const auto amber_default = amber_before->default_value;
    const auto white_blackout = white_before->blackout_value;
    const auto amber_blackout = amber_before->blackout_value;
    const auto white_highlight = white_before->highlight_value;
    const auto amber_highlight = amber_before->highlight_value;
    const auto white_min = white_before->dmx_min;
    const auto amber_min = amber_before->dmx_min;
    const auto white_max = white_before->dmx_max;
    const auto amber_max = amber_before->dmx_max;

    const auto planned =
        emberlights::plan_fixture_profile_channel_function_swap(
            profile, 4U, 5U);
    CHECK(static_cast<bool>(planned));
    CHECK(!planned.changed);
    CHECK(planned.plan.changes_mapping);
    CHECK(planned.plan.first_property_before == showcore::Property::White);
    CHECK(planned.plan.second_property_before == showcore::Property::Amber);
    CHECK(planned.plan.first_property_after == showcore::Property::Amber);
    CHECK(planned.plan.second_property_after == showcore::Property::White);

    const auto applied =
        emberlights::apply_fixture_profile_channel_function_swap(
            profile, planned.plan);
    CHECK(static_cast<bool>(applied));
    CHECK(applied.changed);
    const auto* channel4 = channel_at(profile, 4U);
    const auto* channel5 = channel_at(profile, 5U);
    CHECK(channel4 != nullptr);
    CHECK(channel5 != nullptr);
    if (channel4 == nullptr || channel5 == nullptr) {
        return;
    }
    CHECK(channel4->property == showcore::Property::Amber);
    CHECK(channel5->property == showcore::Property::White);
    CHECK(channel4->coarse_offset == 3U);
    CHECK(channel5->coarse_offset == 4U);
    CHECK(channel4->owner == white_owner);
    CHECK(channel5->owner == amber_owner);
    CHECK(channel4->default_value == white_default);
    CHECK(channel5->default_value == amber_default);
    CHECK(channel4->blackout_value == white_blackout);
    CHECK(channel5->blackout_value == amber_blackout);
    CHECK(channel4->highlight_value == white_highlight);
    CHECK(channel5->highlight_value == amber_highlight);
    CHECK(channel4->dmx_min == white_min);
    CHECK(channel5->dmx_min == amber_min);
    CHECK(channel4->dmx_max == white_max);
    CHECK(channel5->dmx_max == amber_max);
    CHECK(channel4->capabilities.empty());
    CHECK(channel5->capabilities.empty());
    CHECK(profile.channels[0].capabilities.size() == 1U);
    CHECK(profile.channels[0].capabilities[0].id == "red-level");

    auto project = emberlights::make_starter_project();
    project.fixture_profiles.push_back(profile);
    project.fixtures.push_back({
        "fixture.ir4.swap",
        "IR-4 swap semantics",
        profile.id,
        1U,
        1U,
        {}});
    auto compilation = emberlights::compile_project(project);
    CHECK(static_cast<bool>(compilation));
    if (!compilation) {
        return;
    }
    auto& engine = compilation.show->engine();
    engine.layers().set(
        showcore::LayerId::ManualOverride,
        0U,
        showcore::Property::White,
        showcore::PropertyValue::set(1.0F));
    engine.tick();
    CHECK(engine.frames().universes[0][3] == 0U);
    CHECK(engine.frames().universes[0][4] == 255U);

    engine.layers().clear_layer(showcore::LayerId::ManualOverride);
    engine.layers().set(
        showcore::LayerId::ManualOverride,
        0U,
        showcore::Property::Amber,
        showcore::PropertyValue::set(1.0F));
    engine.tick();
    CHECK(engine.frames().universes[0][3] == 255U);
    CHECK(engine.frames().universes[0][4] == 0U);
}

void test_general_channel_function_swap_guards_and_atomicity() {
    emberlights::FixtureProfileDefinition no_op;
    no_op.id = "local.swap.noop";
    no_op.name = "Same direct functions";
    no_op.footprint = 2U;
    no_op.channels.push_back({showcore::Property::Red, 0U});
    no_op.channels.push_back({showcore::Property::Red, 1U});
    const auto no_op_snapshot = profile_snapshot(no_op);
    const auto no_op_plan =
        emberlights::plan_fixture_profile_channel_function_swap(
            no_op, 1U, 2U);
    CHECK(static_cast<bool>(no_op_plan));
    CHECK(!no_op_plan.plan.changes_mapping);
    const auto no_op_apply =
        emberlights::apply_fixture_profile_channel_function_swap(
            no_op, no_op_plan.plan);
    CHECK(static_cast<bool>(no_op_apply));
    CHECK(!no_op_apply.changed);
    CHECK(profile_snapshot(no_op) == no_op_snapshot);

    auto stale = emberlights::make_both_lighting_bo_ir4_6ch_profile();
    stale.id = "local.swap.stale";
    stale.name = "Stale swap";
    stale.source = showcore::FixtureProfileSource::Local;
    const auto stale_plan =
        emberlights::plan_fixture_profile_channel_function_swap(
            stale, 4U, 5U);
    CHECK(static_cast<bool>(stale_plan));
    stale.name = "Renamed after review";
    const auto changed_snapshot = profile_snapshot(stale);
    const auto stale_apply =
        emberlights::apply_fixture_profile_channel_function_swap(
            stale, stale_plan.plan);
    CHECK(!static_cast<bool>(stale_apply));
    CHECK(stale_apply.error ==
          emberlights::FixtureProfileChannelFunctionSwapError::StalePlan);
    CHECK(profile_snapshot(stale) == changed_snapshot);

    auto read_only = emberlights::make_both_lighting_bo_ir4_6ch_profile();
    const auto read_only_snapshot = profile_snapshot(read_only);
    const auto immutable =
        emberlights::plan_fixture_profile_channel_function_swap(
            read_only, 4U, 5U);
    CHECK(!static_cast<bool>(immutable));
    CHECK(immutable.error ==
          emberlights::FixtureProfileChannelFunctionSwapError::SourceReadOnly);
    CHECK(profile_snapshot(read_only) == read_only_snapshot);

    emberlights::FixtureProfileDefinition fine;
    fine.id = "local.swap.fine";
    fine.name = "Fine mapping";
    fine.footprint = 3U;
    fine.channels.push_back({
        showcore::Property::Pan,
        0U,
        1,
        showcore::ChannelEncoding::Linear16});
    fine.channels.push_back({showcore::Property::Tilt, 2U});
    const auto fine_snapshot = profile_snapshot(fine);
    const auto fine_rejected =
        emberlights::plan_fixture_profile_channel_function_swap(
            fine, 1U, 3U);
    CHECK(!static_cast<bool>(fine_rejected));
    CHECK(fine_rejected.error ==
          emberlights::FixtureProfileChannelFunctionSwapError::FineChannelUnsupported);
    CHECK(profile_snapshot(fine) == fine_snapshot);

    emberlights::FixtureProfileDefinition compound;
    compound.id = "local.swap.compound";
    compound.name = "Compound mapping";
    compound.footprint = 2U;
    emberlights::ChannelDefinition compound_red{showcore::Property::Red, 0U};
    emberlights::ChannelCapabilityDefinition red_range;
    red_range.id = "red";
    red_range.name = "Red";
    red_range.property = showcore::Property::Red;
    red_range.preferred_value = 255U;
    compound_red.capabilities.push_back(red_range);
    compound.channels.push_back(compound_red);
    compound.channels.push_back({showcore::Property::Green, 1U});
    const auto compound_snapshot = profile_snapshot(compound);
    const auto compound_rejected =
        emberlights::plan_fixture_profile_channel_function_swap(
            compound, 1U, 2U);
    CHECK(!static_cast<bool>(compound_rejected));
    CHECK(compound_rejected.error ==
          emberlights::FixtureProfileChannelFunctionSwapError::CompoundChannelUnsupported);
    CHECK(profile_snapshot(compound) == compound_snapshot);

    emberlights::FixtureProfileDefinition unsafe;
    unsafe.id = "local.swap.unsafe";
    unsafe.name = "Unsafe mapping";
    unsafe.footprint = 2U;
    unsafe.channels.push_back({showcore::Property::Strobe, 0U});
    unsafe.channels.push_back({showcore::Property::Red, 1U});
    const auto unsafe_snapshot = profile_snapshot(unsafe);
    const auto unsafe_rejected =
        emberlights::plan_fixture_profile_channel_function_swap(
            unsafe, 1U, 2U);
    CHECK(!static_cast<bool>(unsafe_rejected));
    CHECK(unsafe_rejected.error ==
          emberlights::FixtureProfileChannelFunctionSwapError::UnsafeFunction);
    CHECK(profile_snapshot(unsafe) == unsafe_snapshot);

    emberlights::FixtureProfileDefinition incompatible;
    incompatible.id = "local.swap.incompatible";
    incompatible.name = "Different physical owners";
    incompatible.footprint = 2U;
    incompatible.channels.push_back({showcore::Property::White, 0U});
    incompatible.channels.push_back({showcore::Property::Amber, 1U});
    incompatible.channels[1].owner = "cell.2";
    const auto incompatible_snapshot = profile_snapshot(incompatible);
    const auto incompatible_rejected =
        emberlights::plan_fixture_profile_channel_function_swap(
            incompatible, 1U, 2U);
    CHECK(!static_cast<bool>(incompatible_rejected));
    CHECK(incompatible_rejected.error ==
          emberlights::FixtureProfileChannelFunctionSwapError::IncompatibleMappings);
    CHECK(profile_snapshot(incompatible) == incompatible_snapshot);

    const auto same_channel =
        emberlights::plan_fixture_profile_channel_function_swap(
            no_op, 1U, 1U);
    CHECK(!static_cast<bool>(same_channel));
    CHECK(same_channel.error ==
          emberlights::FixtureProfileChannelFunctionSwapError::SameChannel);
}

void test_absolute_white_amber_assignment() {
    auto project = project_with_ir4();
    const auto profile_id = std::string(
        emberlights::kBothLightingBoIr4SixChannelProfileId);

    const auto no_op = emberlights::plan_fixture_profile_white_amber_assignment(
        project, profile_id, 4U, 5U);
    CHECK(static_cast<bool>(no_op));
    CHECK(no_op.already_assigned);

    const auto reversed = emberlights::plan_fixture_profile_white_amber_assignment(
        project, profile_id, 5U, 4U);
    CHECK(static_cast<bool>(reversed));
    CHECK(!reversed.already_assigned);
    CHECK(reversed.plan.creates_local_copy);
    CHECK(reversed.plan.white_channel_after == 5U);
    CHECK(reversed.plan.amber_channel_after == 4U);

    const auto applied = emberlights::apply_fixture_profile_white_amber_correction(
        project, reversed.plan);
    CHECK(applied.applied);
    CHECK(project.fixtures.front().profile_id == applied.plan.replacement_profile_id);

    const auto repeated = emberlights::plan_fixture_profile_white_amber_assignment(
        project, applied.plan.replacement_profile_id, 5U, 4U);
    CHECK(static_cast<bool>(repeated));
    CHECK(repeated.already_assigned);

    const auto invalid_pair = emberlights::plan_fixture_profile_white_amber_assignment(
        project, applied.plan.replacement_profile_id, 3U, 4U);
    CHECK(!static_cast<bool>(invalid_pair));
    CHECK(invalid_pair.error ==
          emberlights::FixtureProfileWhiteAmberAssignmentError::SelectionIsNotWhiteAmberPair);
}

}  // namespace

int main() {
    test_templates_and_rows();
    test_parameter_catalog_contract();
    test_picker_choices_and_assign_next();
    test_fill_gaps_respects_fine_slots_and_is_atomic();
    test_structured_channel_mutations();
    test_profile_parameter_audit();
    test_named_channel_capabilities_and_bindings();
    test_named_channel_capability_rendering();
    test_named_channel_capability_project_round_trip();
    test_generic_profile_rebind_transaction();
    test_general_channel_function_swap_ir4_output_semantics();
    test_general_channel_function_swap_guards_and_atomicity();
    test_absolute_white_amber_assignment();
    if (failures != 0) {
        std::cerr << failures << " fixture profile editor check(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "fixture_profile_editor_tests passed\n";
    return EXIT_SUCCESS;
}
