#include "emberlights/fixtures_looks_shell.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <numeric>
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
    auto project = emberlights::make_starter_project();
    project.id = "replacement-shell-test";
    project.name = "Ballroom Rig";

    emberlights::FixtureProfileDefinition profile;
    profile.id = "local.visual.mover";
    profile.manufacturer = "Ember Test";
    profile.model = "Visual Mover";
    profile.mode = "12 channel";
    profile.name = "Ember Test Visual Mover (12 channel)";
    profile.source = showcore::FixtureProfileSource::Local;
    profile.source_revision = "shell-v1";
    profile.footprint = 12U;
    profile.channels = {
        direct_channel(showcore::Property::Intensity, 0U),
        direct_channel(showcore::Property::Red, 1U),
        direct_channel(showcore::Property::Green, 2U),
        direct_channel(showcore::Property::Blue, 3U),
        direct_channel(showcore::Property::White, 4U),
        direct_channel(showcore::Property::Amber, 5U),
        direct_channel(showcore::Property::Pan, 6U),
        direct_channel(showcore::Property::Tilt, 7U),
        direct_channel(showcore::Property::Focus, 9U),
        direct_channel(showcore::Property::Zoom, 10U)};

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
    emberlights::ChannelCapabilityDefinition dots = open;
    dots.id = "dots";
    dots.name = "Dots";
    dots.dmx_min = 32U;
    dots.dmx_max = 63U;
    dots.preferred_value = 48U;
    gobo.capabilities.push_back(dots);
    profile.channels.insert(profile.channels.begin() + 8, std::move(gobo));

    emberlights::ChannelDefinition strobe;
    strobe.property = showcore::Property::Count;
    strobe.coarse_offset = 11U;
    strobe.encoding = showcore::ChannelEncoding::Discrete8;
    strobe.owner = "shutter";
    emberlights::ChannelCapabilityDefinition closed;
    closed.id = "closed";
    closed.name = "Closed";
    closed.property = showcore::Property::Strobe;
    closed.dmx_min = 0U;
    closed.dmx_max = 15U;
    closed.preferred_value = 0U;
    closed.behavior = showcore::ChannelCapabilityBehavior::Slot;
    closed.access = showcore::ChannelCapabilityAccess::Selectable;
    strobe.capabilities.push_back(closed);
    auto open_shutter = closed;
    open_shutter.id = "open";
    open_shutter.name = "Open";
    open_shutter.dmx_min = 16U;
    open_shutter.dmx_max = 31U;
    open_shutter.preferred_value = 16U;
    strobe.capabilities.push_back(std::move(open_shutter));
    emberlights::ChannelCapabilityDefinition slow_fast;
    slow_fast.id = "slow-fast";
    slow_fast.name = "Slow to fast";
    slow_fast.property = showcore::Property::Strobe;
    slow_fast.dmx_min = 32U;
    slow_fast.dmx_max = 127U;
    slow_fast.preferred_value = 80U;
    slow_fast.behavior = showcore::ChannelCapabilityBehavior::Continuous;
    slow_fast.access = showcore::ChannelCapabilityAccess::Selectable;
    strobe.capabilities.push_back(std::move(slow_fast));
    profile.channels.push_back(std::move(strobe));
    project.fixture_profiles.push_back(std::move(profile));

    project.fixtures.push_back({
        "fixture.mover.left", "Mover Left", "local.visual.mover", 1U, 1U,
        {"dance-floor", "movers"}});
    project.fixtures.push_back({
        "fixture.mover.right", "Mover Right", "local.visual.mover", 1U, 21U,
        {"dance-floor", "movers"}});
    project.groups.push_back({
        "group.movers", "Dance Floor Movers",
        {"fixture.mover.left", "fixture.mover.right"}});

    emberlights::LookDefinition look;
    look.id = "look.ceremony";
    look.name = "Ceremony Wash";
    look.fade_ms = 1200U;
    for (const auto fixture : {std::string_view("fixture.mover.left"),
                               std::string_view("fixture.mover.right")}) {
        look.assignments.push_back({
            std::string(fixture), showcore::Property::Intensity,
            showcore::PropertyValue::force_zero()});
        look.assignments.push_back({
            std::string(fixture), showcore::Property::Red,
            showcore::PropertyValue::set(0.5F)});
        look.assignments.push_back({
            std::string(fixture), showcore::Property::Strobe,
            showcore::PropertyValue::set(0.75F)});
    }
    look.assignments.push_back({
        "fixture.mover.left", showcore::Property::Green,
        showcore::PropertyValue::set(0.2F)});
    project.looks.push_back(std::move(look));
    project.looks.push_back({
        "look.dance",
        "Open Dance",
        450U,
        {{"fixture.mover.left", showcore::Property::Intensity,
          showcore::PropertyValue::set(0.8F)}}});
    return project;
}

[[nodiscard]] const emberlights::FixturesLooksControlBinding* control(
    const emberlights::FixturesLooksShellModel& model,
    showcore::Property property) {
    const auto found = std::find_if(
        model.controls.begin(), model.controls.end(),
        [property](const auto& candidate) {
            return candidate.property == property;
        });
    return found == model.controls.end() ? nullptr : &*found;
}

[[nodiscard]] const emberlights::FixturesLooksControlGroup* control_group(
    const emberlights::FixturesLooksShellModel& model,
    std::string_view stable_id) {
    const auto found = std::find_if(
        model.control_groups.begin(), model.control_groups.end(),
        [stable_id](const auto& candidate) {
            return candidate.stable_id == stable_id;
        });
    return found == model.control_groups.end() ? nullptr : &*found;
}

void test_ready_slice_joins_profiles_targets_looks_and_visual_controls() {
    const auto project = make_project();
    emberlights::FixturesLooksShellQuery query;
    query.selected_target_id = "group.movers";
    query.selected_static_look_id = "look.ceremony";
    query.selected_profile_id = "local.visual.mover";
    query.viewport_width = 1366;
    query.viewport_height = 768;
    const auto model = emberlights::build_fixtures_looks_shell_model(
        project, query);

    CHECK(emberlights::kFixturesLooksShellModelVersion == 3U);
    CHECK(emberlights::kFixturesLooksShellSliceId ==
          "studio.fixtures-static-looks");
    CHECK(model.state == emberlights::FixturesLooksShellState::Ready);
    CHECK(model.minimum_viewport_supported);
    CHECK(model.project_name == "Ballroom Rig");
    CHECK(model.selected_target_id == "group.movers");
    CHECK(model.selected_static_look_id == "look.ceremony");
    CHECK(model.selected_profile_id == "local.visual.mover");
    CHECK(model.can_edit);
    CHECK(model.can_preview);
    CHECK(!model.controls.empty());
    CHECK(model.advanced_available);
    CHECK(model.control_surface.hidden_advanced_count == 0U);
    CHECK(!model.control_groups.empty());
    CHECK(model.control_group_count == model.control_groups.size());
    CHECK(std::accumulate(
              model.control_groups.begin(), model.control_groups.end(),
              std::size_t{0U}, [](std::size_t total, const auto& group) {
                  return total + group.binding_count;
              }) == model.controls.size());
    CHECK(model.validation_error_count == 0U);

    const auto profile = std::find_if(
        model.profiles.begin(), model.profiles.end(),
        [](const auto& item) { return item.stable_id == "local.visual.mover"; });
    CHECK(profile != model.profiles.end());
    if (profile != model.profiles.end()) {
        CHECK(profile->source_label == "Local editable");
        CHECK(profile->patched_fixture_count == 2U);
        CHECK(!profile->read_only);
        CHECK(profile->selected);
    }

    const auto intensity = control(model, showcore::Property::Intensity);
    const auto red = control(model, showcore::Property::Red);
    const auto green = control(model, showcore::Property::Green);
    const auto pan = control(model, showcore::Property::Pan);
    const auto gobo = control(model, showcore::Property::Gobo);
    const auto strobe = std::find_if(
        model.controls.begin(), model.controls.end(), [](const auto& item) {
            return item.property == showcore::Property::Strobe &&
                item.accepts_value;
        });
    const auto* gobo_group = control_group(model, "parameter.gobo");
    const auto* strobe_group = control_group(model, "parameter.strobe");
    CHECK(intensity != nullptr);
    CHECK(red != nullptr);
    CHECK(green != nullptr);
    CHECK(pan != nullptr);
    CHECK(gobo != nullptr);
    CHECK(strobe != model.controls.end());
    CHECK(gobo_group != nullptr);
    if (gobo_group != nullptr) {
        CHECK(gobo_group->parameter_id == "gobo");
        CHECK(gobo_group->label == "Gobo");
        CHECK(gobo_group->binding_count == 2U);
        CHECK(gobo_group->profile_function_count == 2U);
        CHECK(gobo_group->selector_binding_count == 2U);
        CHECK(gobo_group->value_binding_count == 0U);
        CHECK(!gobo_group->composite);
        CHECK(std::count_if(
                  model.controls.begin(), model.controls.end(),
                  [gobo_group](const auto& item) {
                      return item.widget_id == gobo_group->stable_id;
                  }) == 2);
    }
    CHECK(strobe_group != nullptr);
    if (strobe_group != nullptr) {
        CHECK(strobe_group->binding_count == 3U);
        CHECK(strobe_group->profile_function_count == 3U);
        CHECK(strobe_group->value_binding_count == 1U);
        CHECK(strobe_group->selector_binding_count == 2U);
        CHECK(std::count_if(
                  model.controls.begin(), model.controls.end(),
                  [strobe_group](const auto& item) {
                      return item.widget_id == strobe_group->stable_id;
                  }) == 3);
    }
    if (intensity != nullptr) {
        CHECK(intensity->parameter_id == "intensity");
        CHECK(intensity->category ==
              emberlights::FixtureParameterCategory::Intensity);
        CHECK(intensity->accepts_value);
        CHECK(intensity->ownership ==
              emberlights::StaticLookOwnershipState::ForceZero);
        CHECK(intensity->ownership_text.find("explicitly off") !=
              std::string::npos);
    }
    if (red != nullptr) {
        CHECK(red->ownership == emberlights::StaticLookOwnershipState::Set);
        CHECK(red->normalized_value == 0.5F);
        CHECK(red->assigned_fixture_count == 2U);
        CHECK(red->target_fixture_count == 2U);
    }
    if (green != nullptr) {
        CHECK(green->ownership == emberlights::StaticLookOwnershipState::Mixed);
        CHECK(green->value_mixed);
    }
    if (pan != nullptr) {
        CHECK(pan->ownership == emberlights::StaticLookOwnershipState::Release);
        CHECK(pan->control_kind == "XY position pad");
    }
    if (gobo != nullptr) {
        CHECK(gobo->control_kind == "visual choice tiles");
        CHECK(gobo->profile_function);
        CHECK(!gobo->accepts_value);
    }
    if (strobe != model.controls.end()) {
        CHECK(strobe->profile_function);
        CHECK(strobe->accepts_value);
        CHECK(strobe->value_matches_choice);
        CHECK(std::fabs(strobe->normalized_value - 0.25F) < 0.0001F);
        query.selected_choice_id = strobe->choice_id;
        const auto diagnostic_model =
            emberlights::build_fixtures_looks_shell_model(project, query);
        CHECK(diagnostic_model.control_diagnostics.size() == 2U);
        if (!diagnostic_model.control_diagnostics.empty()) {
            CHECK(diagnostic_model.control_diagnostics.front().detail.find(
                      "DMX 56") != std::string::npos);
            CHECK(diagnostic_model.control_diagnostics.front().provenance.find(
                      "shell-v1") != std::string::npos);
        }
    }
}

void test_profile_parameter_category_and_search_projection() {
    const auto project = make_project();
    emberlights::FixturesLooksShellQuery query;
    query.selected_target_id = "group.movers";
    query.selected_static_look_id = "look.ceremony";
    query.control_search = "zoom";
    const auto searched = emberlights::build_fixtures_looks_shell_model(
        project, query);
    CHECK(searched.selected_control_category_id == "all");
    CHECK(searched.control_search == "zoom");
    CHECK(searched.control_visible_count == 1U);
    CHECK(searched.controls.size() == 1U);
    CHECK(searched.controls.front().property == showcore::Property::Zoom);
    CHECK(searched.control_total_count > searched.control_visible_count);
    CHECK(!searched.control_categories.empty());
    CHECK(searched.control_categories.front().stable_id == "all");
    CHECK(searched.control_categories.front().selected);

    query.control_search = {};
    query.control_category = emberlights::FixtureParameterCategory::Beam;
    const auto beam = emberlights::build_fixtures_looks_shell_model(
        project, query);
    CHECK(beam.selected_control_category_id == "beam");
    CHECK(beam.control_visible_count == 5U);
    CHECK(std::all_of(
        beam.controls.begin(), beam.controls.end(), [](const auto& item) {
            return item.category ==
                emberlights::FixtureParameterCategory::Beam;
        }));
    const auto selected = std::find_if(
        beam.control_categories.begin(), beam.control_categories.end(),
        [](const auto& item) { return item.selected; });
    CHECK(selected != beam.control_categories.end());
    if (selected != beam.control_categories.end()) {
        CHECK(selected->stable_id == "beam");
        CHECK(selected->total_count == 5U);
    }

    CHECK(emberlights::fixture_parameter_category_stable_id(
              emberlights::FixtureParameterCategory::Atmosphere) ==
          "atmosphere");
    CHECK(emberlights::fixture_parameter_category_from_stable_id("image") ==
          emberlights::FixtureParameterCategory::Image);
    CHECK(!emberlights::fixture_parameter_category_from_stable_id(
               "not-a-category").has_value());
    CHECK(!emberlights::fixture_parameter_category_from_stable_id(
               "all").has_value());
}

void test_advanced_disclosure_read_only_and_live_preview_gate() {
    const auto project = make_project();
    emberlights::FixturesLooksShellQuery query;
    query.selected_target_id = "fixture.mover.left";
    query.selected_static_look_id = "look.ceremony";
    query.include_advanced = true;
    query.advanced_open = true;
    query.live_running = true;
    query.read_only = true;
    query.viewport_width = 1920;
    query.viewport_height = 1080;
    const auto model = emberlights::build_fixtures_looks_shell_model(
        project, query);
    CHECK(model.state == emberlights::FixturesLooksShellState::Ready);
    CHECK(model.density == emberlights::UiShellDensity::Wide);
    CHECK(model.advanced_open);
    CHECK(model.control_surface.hidden_advanced_count == 0U);
    CHECK(model.read_only);
    CHECK(!model.can_edit);
    CHECK(!model.can_preview);
    CHECK(model.preview_status.find("Live is running") != std::string::npos);
    CHECK(std::none_of(
        model.controls.begin(), model.controls.end(),
        [](const auto& item) { return item.enabled; }));
}

void test_search_responsive_and_stale_selection_states() {
    const auto project = make_project();
    emberlights::FixturesLooksShellQuery query;
    query.profile_search = "visual shell-v1";
    query.static_look_search = "dance 450";
    query.selected_target_id = "group.movers";
    query.selected_static_look_id = "look.dance";
    query.viewport_width = 1200;
    query.viewport_height = 720;
    const auto compact = emberlights::build_fixtures_looks_shell_model(
        project, query);
    CHECK(compact.state == emberlights::FixturesLooksShellState::Degraded);
    CHECK(!compact.minimum_viewport_supported);
    CHECK(compact.profiles.size() == 1U);
    CHECK(compact.static_looks.size() == 1U);
    CHECK(compact.message.find("1366") != std::string::npos);

    query.viewport_width = 1366;
    query.viewport_height = 768;
    query.selected_target_id = "group.removed";
    const auto stale = emberlights::build_fixtures_looks_shell_model(
        project, query);
    CHECK(stale.state ==
          emberlights::FixturesLooksShellState::SelectionRequired);
    CHECK(stale.selected_target_id == "group.removed");
    CHECK(!stale.can_edit);
    CHECK(!stale.can_preview);

    query.selected_target_id = "group.movers";
    query.selected_static_look_id = "look.removed";
    const auto stale_look = emberlights::build_fixtures_looks_shell_model(
        project, query);
    CHECK(stale_look.state ==
          emberlights::FixturesLooksShellState::SelectionRequired);
    CHECK(stale_look.selected_static_look_id == "look.removed");
    CHECK(!stale_look.can_edit);
    CHECK(!stale_look.can_preview);

    query.selected_static_look_id = "look.dance";
    query.selected_profile_id = "profile.removed";
    const auto stale_profile = emberlights::build_fixtures_looks_shell_model(
        project, query);
    CHECK(stale_profile.state ==
          emberlights::FixturesLooksShellState::SelectionRequired);
    CHECK(stale_profile.selected_profile_id == "profile.removed");
    CHECK(stale_profile.message.find("profile no longer exists") !=
          std::string::npos);
    CHECK(!stale_profile.can_edit);
    CHECK(!stale_profile.can_preview);
}

void test_explicit_empty_states() {
    emberlights::ProjectDocument empty;
    const auto empty_model =
        emberlights::build_fixtures_looks_shell_model(empty);
    CHECK(empty_model.state ==
          emberlights::FixturesLooksShellState::EmptyProject);
    CHECK(empty_model.message.find("OFL/QLC+") != std::string::npos);

    auto no_target = emberlights::make_starter_project();
    no_target.id = "no-target";
    no_target.name = "No target";
    const auto no_target_model =
        emberlights::build_fixtures_looks_shell_model(no_target);
    CHECK(no_target_model.state == emberlights::FixturesLooksShellState::NoTarget);

    auto no_look = make_project();
    no_look.looks.clear();
    const auto no_look_model =
        emberlights::build_fixtures_looks_shell_model(no_look);
    CHECK(no_look_model.state ==
          emberlights::FixturesLooksShellState::NoStaticLook);
}

}  // namespace

int main() {
    test_ready_slice_joins_profiles_targets_looks_and_visual_controls();
    test_profile_parameter_category_and_search_projection();
    test_advanced_disclosure_read_only_and_live_preview_gate();
    test_search_responsive_and_stale_selection_states();
    test_explicit_empty_states();
    if (failures != 0) {
        std::cerr << failures << " fixtures/looks shell test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "Fixtures/Looks replacement-shell model tests passed\n";
    return EXIT_SUCCESS;
}
