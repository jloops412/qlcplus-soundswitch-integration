#include "emberlights/compiler.hpp"
#include "emberlights/fixture_controller_binding.hpp"
#include "emberlights/project_io.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

namespace {

int failures = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            std::cerr << "CHECK failed at " << __FILE__ << ':' << __LINE__     \
                      << ": " #condition << '\n';                              \
            ++failures;                                                        \
        }                                                                       \
    } while (false)

[[nodiscard]] emberlights::ChannelCapabilityDefinition capability(
    std::string id,
    std::string name,
    showcore::Property property,
    std::uint8_t dmx_min,
    std::uint8_t dmx_max,
    std::uint8_t preferred,
    showcore::ChannelCapabilityBehavior behavior =
        showcore::ChannelCapabilityBehavior::Slot,
    showcore::ChannelCapabilityAccess access =
        showcore::ChannelCapabilityAccess::Selectable) {
    emberlights::ChannelCapabilityDefinition value;
    value.id = std::move(id);
    value.name = std::move(name);
    value.property = property;
    value.dmx_min = dmx_min;
    value.dmx_max = dmx_max;
    value.preferred_value = preferred;
    value.behavior = behavior;
    value.access = access;
    return value;
}

[[nodiscard]] emberlights::FixtureProfileDefinition make_profile(
    bool three_colors) {
    emberlights::FixtureProfileDefinition profile;
    profile.id = three_colors ? "local.controller.three" : "local.controller.two";
    profile.manufacturer = "Test";
    profile.model = "Controller fixture";
    profile.mode = three_colors ? "Three colors" : "Two colors";
    profile.name = profile.model + " " + profile.mode;
    profile.source = showcore::FixtureProfileSource::Local;
    profile.source_revision = "1";
    profile.footprint = 4U;

    emberlights::ChannelDefinition wheel;
    wheel.property = showcore::Property::Count;
    wheel.coarse_offset = 0U;
    wheel.encoding = showcore::ChannelEncoding::Discrete8;
    wheel.owner = "fixture";
    wheel.capabilities.push_back(capability(
        "red", "Red", showcore::Property::ColorWheel, 0U, 9U, 5U));
    if (three_colors) {
        wheel.capabilities.push_back(capability(
            "green", "Green", showcore::Property::ColorWheel,
            10U, 19U, 15U));
    }
    wheel.capabilities.push_back(capability(
        "blue", "Blue", showcore::Property::ColorWheel,
        three_colors ? 20U : 10U, three_colors ? 29U : 19U,
        three_colors ? 25U : 15U));

    emberlights::ChannelDefinition shutter;
    shutter.property = showcore::Property::Count;
    shutter.coarse_offset = 1U;
    shutter.encoding = showcore::ChannelEncoding::Discrete8;
    shutter.owner = "fixture";
    shutter.capabilities.push_back(capability(
        "open", "Open", showcore::Property::Shutter,
        0U, three_colors ? 29U : 19U,
        three_colors ? 15U : 10U));
    shutter.capabilities.push_back(capability(
        "strobe-slow-fast", "Strobe slow to fast",
        showcore::Property::Strobe,
        three_colors ? 30U : 20U, three_colors ? 130U : 120U,
        three_colors ? 80U : 70U,
        showcore::ChannelCapabilityBehavior::Continuous,
        showcore::ChannelCapabilityAccess::SafetyGated));
    shutter.capabilities.push_back(capability(
        "factory-reset", "Factory reset", showcore::Property::Custom1,
        200U, 220U, 210U,
        showcore::ChannelCapabilityBehavior::Slot,
        showcore::ChannelCapabilityAccess::Protected));
    emberlights::ChannelDefinition intensity;
    intensity.property = showcore::Property::Intensity;
    intensity.coarse_offset = 2U;
    intensity.fine_offset = 3;
    intensity.encoding = showcore::ChannelEncoding::Linear16;
    intensity.highlight_value = 65535U;
    profile.channels = {
        std::move(wheel), std::move(shutter), std::move(intensity)};
    return profile;
}

[[nodiscard]] emberlights::ProjectDocument make_project() {
    auto project = emberlights::make_starter_project();
    project.id = "fixture-controller-binding-test";
    project.name = "Fixture Controller Binding Test";
    project.fixture_profiles.push_back(make_profile(false));
    project.fixture_profiles.push_back(make_profile(true));
    project.fixtures.push_back({
        "wheel-two", "Wheel Two", "local.controller.two", 1U, 1U, {}});
    project.fixtures.push_back({
        "wheel-three", "Wheel Three", "local.controller.three", 1U, 10U, {}});
    project.groups.push_back({
        "wheel-group", "Wheel Group", {"wheel-two", "wheel-three"}});
    return project;
}

[[nodiscard]] std::string choice_id(
    const emberlights::ProjectDocument& project,
    std::string_view target_id,
    std::string_view capability_id) {
    const auto catalog = emberlights::fixture_control_choices(
        project, target_id);
    const auto found = std::find_if(
        catalog.choices.begin(), catalog.choices.end(),
        [capability_id](const auto& choice) {
            return choice.capability_id == capability_id;
        });
    return found == catalog.choices.end() ? std::string{} : found->id;
}

[[nodiscard]] emberlights::MidiMappingDefinition prototype_mapping() {
    emberlights::MidiMappingDefinition prototype;
    prototype.device_name = "Any learned controller";
    prototype.preferred_input_index = 3;
    prototype.message_type = showcore::MidiMessageType::ControlChange;
    prototype.channel = 4U;
    prototype.number = 72U;
    prototype.input_mode = showcore::MidiInputMode::Absolute7;
    prototype.behavior = showcore::MappingBehavior::Continuous;
    prototype.action.layer = showcore::LayerId::ManualOverride;
    prototype.output_min = 0.2F;
    prototype.output_max = 0.8F;
    prototype.curve = 1.75F;
    prototype.inverted = true;
    prototype.soft_takeover = true;
    prototype.takeover_tolerance = 0.04F;
    return prototype;
}

void test_fixture_slot_and_homogeneous_group() {
    const auto project = make_project();
    CHECK(emberlights::validate_project(project).ok());
    const auto prototype = prototype_mapping();

    const auto fixture_blue = emberlights::plan_fixture_controller_binding(
        project,
        "wheel-two",
        choice_id(project, "wheel-two", "blue"),
        prototype);
    CHECK(fixture_blue);
    CHECK(!fixture_blue.group);
    CHECK(!fixture_blue.expanded_to_fixtures);
    CHECK(fixture_blue.mappings.size() == 1U);
    if (!fixture_blue.mappings.empty()) {
        const auto& mapping = fixture_blue.mappings[0];
        CHECK(mapping.device_name == prototype.device_name);
        CHECK(mapping.preferred_input_index == prototype.preferred_input_index);
        CHECK(mapping.channel == prototype.channel &&
              mapping.number == prototype.number);
        CHECK(mapping.action.type == showcore::ActionType::SetProperty);
        CHECK(mapping.action.property == showcore::Property::ColorWheel);
        CHECK(mapping.target_ref == "wheel-two");
        CHECK(std::fabs(mapping.output_min - 0.75F) < 0.0001F);
        CHECK(mapping.output_min == mapping.output_max);
        CHECK(!mapping.soft_takeover);
        CHECK(mapping.fixture_control_binding_id ==
              choice_id(project, "wheel-two", "blue"));
    }

    const auto group_open = emberlights::plan_fixture_controller_binding(
        project,
        "wheel-group",
        choice_id(project, "wheel-group", "open"),
        prototype);
    CHECK(group_open);
    CHECK(group_open.group);
    CHECK(!group_open.expanded_to_fixtures);
    CHECK(group_open.mappings.size() == 1U);
    if (!group_open.mappings.empty()) {
        const auto& mapping = group_open.mappings[0];
        CHECK(mapping.action.type == showcore::ActionType::SetGroupProperty);
        CHECK(mapping.action.property == showcore::Property::Shutter);
        CHECK(mapping.target_ref == "wheel-group");
        CHECK(mapping.output_min == mapping.output_max);
        CHECK(std::fabs(mapping.output_min - 0.5F) < 0.0001F);
    }


    const auto group_intensity = emberlights::plan_fixture_controller_binding(
        project,
        "wheel-group",
        choice_id(project, "wheel-group", "direct.intensity"),
        prototype);
    CHECK(group_intensity);
    CHECK(group_intensity.group);
    CHECK(!group_intensity.expanded_to_fixtures);
    CHECK(group_intensity.mappings.size() == 1U);
    if (!group_intensity.mappings.empty()) {
        const auto& mapping = group_intensity.mappings.front();
        CHECK(mapping.action.type == showcore::ActionType::SetGroupProperty);
        CHECK(mapping.action.property == showcore::Property::Intensity);
        CHECK(mapping.target_ref == "wheel-group");
        CHECK(std::fabs(mapping.output_min) < 0.0001F);
        CHECK(std::fabs(mapping.output_max - 1.0F) < 0.0001F);
        CHECK(mapping.soft_takeover);
        CHECK(mapping.fixture_control_binding_id.find(
                  "|function:direct|") != std::string::npos);
    }
}

void test_mixed_group_expands_exactly() {
    const auto project = make_project();
    const auto binding_id = choice_id(project, "wheel-group", "blue");
    const auto plan = emberlights::plan_fixture_controller_binding(
        project, "wheel-group", binding_id, prototype_mapping());
    CHECK(plan);
    CHECK(plan.group);
    CHECK(plan.expanded_to_fixtures);
    CHECK(plan.mappings.size() == 2U);
    if (plan.mappings.size() != 2U) {
        return;
    }
    CHECK(plan.mappings[0].target_ref == "wheel-two");
    CHECK(plan.mappings[1].target_ref == "wheel-three");
    CHECK(plan.mappings[0].action.type == showcore::ActionType::SetProperty);
    CHECK(plan.mappings[1].action.type == showcore::ActionType::SetProperty);
    CHECK(std::fabs(plan.mappings[0].output_min - 0.75F) < 0.0001F);
    CHECK(std::fabs(
        plan.mappings[1].output_min - (2.5F / 3.0F)) < 0.0001F);
    CHECK(plan.mappings[0].output_min == plan.mappings[0].output_max);
    CHECK(plan.mappings[1].output_min == plan.mappings[1].output_max);
    CHECK(plan.mappings[0].fixture_control_binding_id == binding_id);
    CHECK(plan.mappings[1].fixture_control_binding_id == binding_id);
}

void test_continuous_endpoints_and_safety_gate() {
    const auto project = make_project();
    const auto strobe = choice_id(
        project, "wheel-group", "strobe-slow-fast");
    const auto rejected = emberlights::plan_fixture_controller_binding(
        project, "wheel-group", strobe, prototype_mapping());
    CHECK(!rejected);
    CHECK(rejected.error ==
          emberlights::FixtureControllerBindingError::SafetyGateRequired);
    CHECK(rejected.mappings.empty());

    emberlights::FixtureControllerBindingOptions confirmed;
    confirmed.allow_safety_gated = true;
    const auto accepted = emberlights::plan_fixture_controller_binding(
        project, "wheel-group", strobe, prototype_mapping(), confirmed);
    CHECK(accepted);
    CHECK(!accepted.expanded_to_fixtures);
    CHECK(accepted.mappings.size() == 1U);
    if (!accepted.mappings.empty()) {
        CHECK(accepted.mappings[0].action.type ==
              showcore::ActionType::SetGroupProperty);
        CHECK(accepted.mappings[0].action.property == showcore::Property::Strobe);
        CHECK(std::fabs(accepted.mappings[0].output_min) < 0.0001F);
        CHECK(std::fabs(accepted.mappings[0].output_max - 1.0F) < 0.0001F);
        CHECK(accepted.mappings[0].soft_takeover);
    }

    CHECK(choice_id(project, "wheel-group", "factory-reset").empty());
    const auto protected_choice = emberlights::plan_fixture_controller_binding(
        project, "wheel-group", "target:protected", prototype_mapping());
    CHECK(!protected_choice);
    CHECK(protected_choice.error ==
          emberlights::FixtureControllerBindingError::ChoiceNotFound);
}

void test_fail_closed_prototypes_and_capacity() {
    auto project = make_project();
    auto relative = prototype_mapping();
    relative.input_mode = showcore::MidiInputMode::RelativeTwosComplement;
    relative.behavior = showcore::MappingBehavior::Relative;
    const auto rejected = emberlights::plan_fixture_controller_binding(
        project,
        "wheel-group",
        choice_id(project, "wheel-group", "blue"),
        relative);
    CHECK(!rejected);
    CHECK(rejected.error ==
          emberlights::FixtureControllerBindingError::UnsupportedPrototype);

    project.midi_mappings.resize(showcore::kMaxMidiMappings);
    const auto full = emberlights::plan_fixture_controller_binding(
        project,
        "wheel-group",
        choice_id(project, "wheel-group", "blue"),
        prototype_mapping());
    CHECK(!full);
    CHECK(full.error ==
          emberlights::FixtureControllerBindingError::ProjectCapacityExceeded);
    CHECK(full.mappings.empty());
}

void test_binding_provenance_format_one_round_trip() {
    auto project = make_project();
    const auto plan = emberlights::plan_fixture_controller_binding(
        project,
        "wheel-group",
        choice_id(project, "wheel-group", "blue"),
        prototype_mapping());
    CHECK(plan);
    project.midi_mappings = plan.mappings;

    auto legacy = prototype_mapping();
    legacy.target_ref = "wheel-two";
    legacy.action.type = showcore::ActionType::SetProperty;
    legacy.action.property = showcore::Property::Intensity;
    legacy.fixture_control_binding_id.clear();
    legacy.number = 73U;
    project.midi_mappings.push_back(std::move(legacy));
    CHECK(emberlights::validate_project(project).ok());

    const auto compilation = emberlights::compile_project(project);
    CHECK(compilation);
    if (compilation) {
        showcore::MidiMessage message;
        message.type = showcore::MidiMessageType::ControlChange;
        message.channel = 4U;
        message.number = 72U;
        message.value = 127U;
        std::array<showcore::MidiActionEvent,
                   showcore::kMaxMidiActionsPerMessage> events{};
        const auto count = compilation.show->midi_mappings().process(
            message, events);
        CHECK(count == 2U);
        if (count == 2U) {
            CHECK(events[0].action.type == showcore::ActionType::SetProperty);
            CHECK(events[1].action.type == showcore::ActionType::SetProperty);
            CHECK(events[0].action.target_id == 0U);
            CHECK(events[1].action.target_id == 1U);
            CHECK(std::fabs(events[0].value - 0.75F) < 0.0001F);
            CHECK(std::fabs(events[1].value - (2.5F / 3.0F)) < 0.0001F);
        }
    }

    const auto serialized = emberlights::serialize_project(project);
    CHECK(serialized.starts_with("EMBERLIGHTS_PROJECT\t1\n"));
    emberlights::ProjectDocument parsed;
    CHECK(emberlights::parse_project(serialized, parsed));
    CHECK(parsed.midi_mappings.size() == 3U);
    if (parsed.midi_mappings.size() == 3U) {
        CHECK(parsed.midi_mappings[0].fixture_control_binding_id ==
              plan.mappings[0].fixture_control_binding_id);
        CHECK(parsed.midi_mappings[1].fixture_control_binding_id ==
              plan.mappings[1].fixture_control_binding_id);
        CHECK(parsed.midi_mappings[2].fixture_control_binding_id.empty());
    }
    CHECK(emberlights::serialize_project(parsed) == serialized);

    auto invalid = project;
    invalid.midi_mappings[0].fixture_control_binding_id.assign(
        emberlights::kMaximumFixtureControlBindingIdLength + 1U, 'x');
    CHECK(!emberlights::validate_project(invalid).ok());
}

}  // namespace

int main() {
    test_fixture_slot_and_homogeneous_group();
    test_mixed_group_expands_exactly();
    test_continuous_endpoints_and_safety_gate();
    test_fail_closed_prototypes_and_capacity();
    test_binding_provenance_format_one_round_trip();
    if (failures != 0) {
        std::cerr << failures << " fixture controller binding test(s) failed\n";
        return 1;
    }
    std::cout << "Fixture controller binding tests passed\n";
    return 0;
}
