#include "emberlights/fixture_control_surface.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <utility>

namespace {

int failures = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            std::cerr << "CHECK failed at " << __FILE__ << ':' << __LINE__     \
                      << ": " #condition << '\n';                              \
            ++failures;                                                         \
        }                                                                       \
    } while (false)

[[nodiscard]] emberlights::ChannelDefinition direct_channel(
    showcore::Property property,
    std::uint16_t offset) {
    emberlights::ChannelDefinition channel;
    channel.property = property;
    channel.coarse_offset = offset;
    channel.encoding = showcore::ChannelEncoding::Linear8;
    channel.highlight_value = 255U;
    channel.owner = "fixture";
    return channel;
}

[[nodiscard]] emberlights::ProjectDocument make_project() {
    emberlights::FixtureProfileDefinition profile;
    profile.id = "local.surface";
    profile.manufacturer = "Ember Test";
    profile.model = "Visual fixture";
    profile.mode = "10 channel";
    profile.name = "Ember Test Visual fixture 10 channel";
    profile.source = showcore::FixtureProfileSource::Local;
    profile.source_revision = "1";
    profile.footprint = 10U;
    profile.channels = {
        direct_channel(showcore::Property::Intensity, 0U),
        direct_channel(showcore::Property::Red, 1U),
        direct_channel(showcore::Property::Green, 2U),
        direct_channel(showcore::Property::Blue, 3U),
        direct_channel(showcore::Property::White, 4U),
        direct_channel(showcore::Property::Amber, 5U),
        direct_channel(showcore::Property::Pan, 6U),
        direct_channel(showcore::Property::Tilt, 7U)};

    emberlights::ChannelDefinition gobo;
    gobo.property = showcore::Property::Count;
    gobo.coarse_offset = 8U;
    gobo.encoding = showcore::ChannelEncoding::Discrete8;
    gobo.owner = "gobo";
    emberlights::ChannelCapabilityDefinition open;
    open.id = "open";
    open.name = "Open";
    open.property = showcore::Property::Gobo;
    open.dmx_min = 0U;
    open.dmx_max = 31U;
    open.preferred_value = 0U;
    open.behavior = showcore::ChannelCapabilityBehavior::Slot;
    open.access = showcore::ChannelCapabilityAccess::Selectable;
    gobo.capabilities.push_back(open);
    auto dots = open;
    dots.id = "dots";
    dots.name = "Dots";
    dots.dmx_min = 32U;
    dots.dmx_max = 63U;
    dots.preferred_value = 48U;
    gobo.capabilities.push_back(std::move(dots));
    profile.channels.push_back(gobo);

    emberlights::ChannelDefinition custom;
    custom.property = showcore::Property::Custom1;
    custom.coarse_offset = 9U;
    custom.encoding = showcore::ChannelEncoding::Linear8;
    custom.owner = "unverified";
    profile.channels.push_back(custom);

    emberlights::ProjectDocument project;
    project.id = "surface-test";
    project.name = "Fixture control surface test";
    project.fixture_profiles.push_back(std::move(profile));
    project.fixtures.push_back({
        "fixture.visual", "Visual fixture", "local.surface", 1U, 1U, {}});
    return project;
}

[[nodiscard]] const emberlights::FixtureControlSurfaceSection* section(
    const emberlights::FixtureControlSurfaceModel& model,
    emberlights::FixtureParameterCategory category) {
    const auto found = std::find_if(
        model.sections.begin(), model.sections.end(),
        [category](const auto& candidate) {
            return candidate.category == category;
        });
    return found == model.sections.end() ? nullptr : &*found;
}

[[nodiscard]] const emberlights::FixtureControlWidget* widget(
    const emberlights::FixtureControlSurfaceSection* value,
    emberlights::FixtureControlWidgetKind kind) {
    if (value == nullptr) {
        return nullptr;
    }
    const auto found = std::find_if(
        value->widgets.begin(), value->widgets.end(),
        [kind](const auto& candidate) { return candidate.kind == kind; });
    return found == value->widgets.end() ? nullptr : &*found;
}

void test_task_facing_groups_hide_raw_diagnostics() {
    const auto project = make_project();
    emberlights::FixtureFunctionComponentQuery query;
    query.target_id = "fixture.visual";
    query.surface = emberlights::FixtureParameterSurface::StaticLook;
    const auto catalog = emberlights::build_fixture_function_component(
        project, query);
    const auto model = emberlights::build_fixture_control_surface(catalog);

    CHECK(emberlights::kFixtureControlSurfaceComponentVersion == 2U);
    CHECK(model.state == emberlights::FixtureFunctionComponentState::Ready);
    CHECK(model.target_id == "fixture.visual");
    CHECK(model.diagnostics_available);
    CHECK(model.hidden_advanced_count == 1U);
    CHECK(model.visible_binding_count == 10U);

    const auto* color = widget(
        section(model, emberlights::FixtureParameterCategory::Color),
        emberlights::FixtureControlWidgetKind::ColorMixer);
    CHECK(color != nullptr);
    if (color != nullptr) {
        CHECK(color->stable_id == "group.color.mixer");
        CHECK(color->parameter_id == "color");
        CHECK(color->bindings.size() == 5U);
        CHECK(color->enabled);
        CHECK(!color->degraded);
        CHECK(color->accessibility_label.find("5 of 5") != std::string::npos);
    }

    const auto* position = widget(
        section(model, emberlights::FixtureParameterCategory::Position),
        emberlights::FixtureControlWidgetKind::XYPad);
    CHECK(position != nullptr);
    if (position != nullptr) {
        CHECK(position->parameter_id == "position");
        CHECK(position->bindings.size() == 2U);
        CHECK(position->bindings[0].choice_id != position->bindings[1].choice_id);
    }

    const auto* intensity = widget(
        section(model, emberlights::FixtureParameterCategory::Intensity),
        emberlights::FixtureControlWidgetKind::LevelFader);
    const auto* gobo = widget(
        section(model, emberlights::FixtureParameterCategory::Image),
        emberlights::FixtureControlWidgetKind::SlotTiles);
    CHECK(intensity != nullptr);
    CHECK(gobo != nullptr);
    if (gobo != nullptr) {
        CHECK(gobo->stable_id == "parameter.gobo");
        CHECK(gobo->parameter_id == "gobo");
        CHECK(gobo->label == "Gobo");
        CHECK(gobo->bindings.size() == 2U);
        CHECK(std::any_of(
            gobo->bindings.begin(), gobo->bindings.end(),
            [](const auto& binding) { return binding.label == "Open"; }));
        CHECK(std::any_of(
            gobo->bindings.begin(), gobo->bindings.end(),
            [](const auto& binding) { return binding.label == "Dots"; }));
        CHECK(gobo->value_binding_count == 0U);
        CHECK(gobo->choice_binding_count == 2U);
    }

    // The ordinary surface keeps stable semantic values and availability but
    // does not expose physical channel or raw DMX fields. Those stay behind
    // the separately requested diagnostic component.
    if (intensity != nullptr && !intensity->bindings.empty()) {
        CHECK(!intensity->bindings.front().choice_id.empty());
        CHECK(intensity->bindings.front().availability_text.find("DMX ") ==
              std::string::npos);
    }
}

void test_advanced_disclosure_and_surface_availability() {
    const auto project = make_project();
    emberlights::FixtureFunctionComponentQuery query;
    query.target_id = "fixture.visual";
    query.surface = emberlights::FixtureParameterSurface::LiveOverride;
    const auto catalog = emberlights::build_fixture_function_component(
        project, query);
    const auto primary = emberlights::build_fixture_control_surface(catalog);
    const auto advanced = emberlights::build_fixture_control_surface(
        catalog, true);
    CHECK(primary.hidden_advanced_count == 1U);
    CHECK(section(primary, emberlights::FixtureParameterCategory::Custom) ==
          nullptr);
    CHECK(advanced.hidden_advanced_count == 0U);
    const auto* custom = widget(
        section(advanced, emberlights::FixtureParameterCategory::Custom),
        emberlights::FixtureControlWidgetKind::CustomControl);
    CHECK(custom != nullptr);
    if (custom != nullptr) {
        CHECK(!custom->enabled);
        CHECK(custom->safety_restricted);
        CHECK(custom->degraded);
    }
    CHECK(advanced.has_degraded_controls);
}

}  // namespace

int main() {
    test_task_facing_groups_hide_raw_diagnostics();
    test_advanced_disclosure_and_surface_availability();
    if (failures != 0) {
        std::cerr << failures << " fixture control surface test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "Fixture control surface tests passed\n";
    return EXIT_SUCCESS;
}
