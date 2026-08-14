#include "emberlights/fixture_function_component.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <tuple>
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
        showcore::ChannelCapabilityAccess::Selectable,
    emberlights::FixtureChannelCapabilityRole role =
        emberlights::FixtureChannelCapabilityRole::Function) {
    emberlights::ChannelCapabilityDefinition result;
    result.id = std::move(id);
    result.name = std::move(name);
    result.property = property;
    result.dmx_min = dmx_min;
    result.dmx_max = dmx_max;
    result.preferred_value = preferred;
    result.behavior = behavior;
    result.access = access;
    result.role = role;
    return result;
}

[[nodiscard]] emberlights::FixtureProfileDefinition make_profile(
    bool three_colors) {
    emberlights::FixtureProfileDefinition profile;
    profile.id = three_colors
        ? "local.component.three"
        : "local.component.two";
    profile.manufacturer = "Test Works";
    profile.model = "Component fixture";
    profile.mode = three_colors ? "Three colors" : "Two colors";
    profile.name = profile.model + " " + profile.mode;
    profile.source = showcore::FixtureProfileSource::Local;
    profile.source_revision = three_colors ? "3" : "2";
    profile.footprint = 4U;

    emberlights::ChannelDefinition wheel;
    wheel.property = showcore::Property::Count;
    wheel.coarse_offset = 0U;
    wheel.encoding = showcore::ChannelEncoding::Discrete8;
    wheel.owner = "wheel";
    wheel.capabilities.push_back(capability(
        "red", "Red", showcore::Property::ColorWheel, 0U, 9U, 5U));
    if (three_colors) {
        wheel.capabilities.push_back(capability(
            "green", "Green", showcore::Property::ColorWheel,
            10U, 19U, 15U));
    }
    wheel.capabilities.push_back(capability(
        "blue", "Blue", showcore::Property::ColorWheel,
        three_colors ? 20U : 10U,
        three_colors ? 29U : 19U,
        three_colors ? 25U : 15U));
    wheel.capabilities.push_back(capability(
        "factory-reset",
        "Factory reset",
        showcore::Property::Custom1,
        200U,
        220U,
        210U,
        showcore::ChannelCapabilityBehavior::Slot,
        showcore::ChannelCapabilityAccess::Protected,
        emberlights::FixtureChannelCapabilityRole::Reset));

    emberlights::ChannelDefinition shutter;
    shutter.property = showcore::Property::Count;
    shutter.coarse_offset = 1U;
    shutter.encoding = showcore::ChannelEncoding::Discrete8;
    shutter.owner = "shutter";
    shutter.capabilities.push_back(capability(
        "open",
        "Open",
        showcore::Property::Shutter,
        0U,
        three_colors ? 29U : 19U,
        three_colors ? 15U : 10U,
        showcore::ChannelCapabilityBehavior::Slot,
        showcore::ChannelCapabilityAccess::Selectable,
        emberlights::FixtureChannelCapabilityRole::Open));
    shutter.capabilities.push_back(capability(
        "strobe-slow-fast",
        "Strobe slow to fast",
        showcore::Property::Strobe,
        three_colors ? 30U : 20U,
        three_colors ? 130U : 120U,
        three_colors ? 80U : 70U,
        showcore::ChannelCapabilityBehavior::Continuous,
        showcore::ChannelCapabilityAccess::SafetyGated));

    emberlights::ChannelDefinition intensity;
    intensity.property = showcore::Property::Intensity;
    intensity.coarse_offset = 2U;
    intensity.fine_offset = 3;
    intensity.encoding = showcore::ChannelEncoding::Linear16;
    intensity.default_value = 0U;
    intensity.blackout_value = 0U;
    intensity.highlight_value = 65535U;
    intensity.owner = "fixture";

    profile.channels = {
        std::move(wheel), std::move(shutter), std::move(intensity)};
    return profile;
}

[[nodiscard]] emberlights::ProjectDocument make_project() {
    emberlights::ProjectDocument project;
    project.id = "fixture-function-component-test";
    project.name = "Fixture Function Component Test";
    project.fixture_profiles.push_back(make_profile(false));
    project.fixture_profiles.push_back(make_profile(true));
    project.fixtures.push_back({
        "fixture-two", "Wheel Two", "local.component.two", 1U, 1U, {}});
    project.fixtures.push_back({
        "fixture-three", "Wheel Three", "local.component.three", 1U, 10U, {}});
    project.groups.push_back({
        "group-wheels", "Wheel Group", {"fixture-two", "fixture-three"}});
    project.groups.push_back({"group-empty", "Empty Group", {}});
    return project;
}

[[nodiscard]] const emberlights::FixtureFunctionRow* find_row(
    const emberlights::FixtureFunctionComponentModel& model,
    std::string_view capability_id) noexcept {
    const auto found = std::find_if(
        model.rows.begin(), model.rows.end(),
        [capability_id](const auto& row) {
            return row.capability_id == capability_id;
        });
    return found == model.rows.end() ? nullptr : &*found;
}

[[nodiscard]] const emberlights::FixtureFunctionCategorySummary* find_category(
    const emberlights::FixtureFunctionComponentModel& model,
    emberlights::FixtureParameterCategory category) noexcept {
    const auto found = std::find_if(
        model.categories.begin(), model.categories.end(),
        [category](const auto& summary) {
            return summary.category == category;
        });
    return found == model.categories.end() ? nullptr : &*found;
}

[[nodiscard]] emberlights::FixtureFunctionComponentModel group_model(
    const emberlights::ProjectDocument& project,
    float position = 0.25F) {
    emberlights::FixtureFunctionComponentQuery query;
    query.target_id = "group-wheels";
    query.position = position;
    return emberlights::build_fixture_function_component(project, query);
}

void test_search_filter_order_and_bounds() {
    CHECK(emberlights::kFixtureFunctionComponentVersion == 4U);
    const auto project = make_project();
    const auto model = group_model(project);
    CHECK(model.state == emberlights::FixtureFunctionComponentState::Ready);
    CHECK(model.target_kind == emberlights::FixtureFunctionTargetKind::Group);
    CHECK(model.target_complete);
    CHECK(model.source_choice_count == 6U);
    CHECK(model.matching_choice_count == 6U);
    CHECK(model.rows.size() == 6U);
    CHECK(model.categories.size() == 3U);
    CHECK(std::is_sorted(
        model.rows.begin(), model.rows.end(),
        [](const auto& first, const auto& second) {
            return std::tuple{
                       static_cast<std::size_t>(first.category),
                       static_cast<std::size_t>(first.property),
                       first.owner,
                       first.name,
                       first.choice_id} <
                std::tuple{
                       static_cast<std::size_t>(second.category),
                       static_cast<std::size_t>(second.property),
                       second.owner,
                       second.name,
                       second.choice_id};
        }));

    const auto rebuilt = group_model(project);
    CHECK(rebuilt.rows.size() == model.rows.size());
    for (std::size_t index = 0U;
         index < model.rows.size() && index < rebuilt.rows.size();
         ++index) {
        CHECK(rebuilt.rows[index].choice_id == model.rows[index].choice_id);
    }

    emberlights::FixtureFunctionComponentQuery search;
    search.target_id = "group-wheels";
    search.position = 0.25F;
    search.search = "ShUtTeR open";
    const auto searched =
        emberlights::build_fixture_function_component(project, search);
    CHECK(searched.rows.size() == 1U);
    CHECK(searched.matching_choice_count == 1U);
    CHECK(find_row(searched, "open") != nullptr);

    emberlights::FixtureFunctionComponentQuery color_filter;
    color_filter.target_id = "group-wheels";
    color_filter.category = emberlights::FixtureParameterCategory::Color;
    const auto colors =
        emberlights::build_fixture_function_component(project, color_filter);
    CHECK(colors.rows.size() == 3U);
    CHECK(std::all_of(
        colors.rows.begin(), colors.rows.end(),
        [](const auto& row) {
            return row.category == emberlights::FixtureParameterCategory::Color;
        }));
    const auto* color_summary = find_category(
        colors, emberlights::FixtureParameterCategory::Color);
    const auto* beam_summary = find_category(
        colors, emberlights::FixtureParameterCategory::Beam);
    CHECK(color_summary != nullptr);
    CHECK(beam_summary != nullptr);
    if (color_summary != nullptr && beam_summary != nullptr) {
        CHECK(color_summary->total_count == 3U);
        CHECK(color_summary->visible_count == 3U);
        CHECK(beam_summary->total_count == 2U);
        CHECK(beam_summary->search_match_count == 2U);
        CHECK(beam_summary->visible_count == 0U);
    }

    emberlights::FixtureFunctionComponentQuery bounded;
    bounded.target_id = "group-wheels";
    bounded.row_limit = 2U;
    const auto limited =
        emberlights::build_fixture_function_component(project, bounded);
    CHECK(limited.rows.size() == 2U);
    CHECK(limited.rows_truncated);
    CHECK(limited.matching_choice_count == 6U);

    std::string long_query(
        emberlights::kFixtureFunctionComponentMaximumSearchBytes + 20U, 'x');
    bounded.search = long_query;
    const auto query_limited =
        emberlights::build_fixture_function_component(project, bounded);
    CHECK(query_limited.search_truncated);
    CHECK(query_limited.search_query.size() ==
          emberlights::kFixtureFunctionComponentMaximumSearchBytes);
}

void test_mixed_profile_rows_diagnostics_and_accessibility() {
    const auto project = make_project();
    const auto model = group_model(project);
    const auto* green = find_row(model, "green");
    const auto* blue = find_row(model, "blue");
    const auto* open = find_row(model, "open");
    const auto* strobe = find_row(model, "strobe-slow-fast");
    const auto* intensity = find_row(model, "direct.intensity");
    CHECK(green != nullptr);
    CHECK(blue != nullptr);
    CHECK(open != nullptr);
    CHECK(strobe != nullptr);
    CHECK(intensity != nullptr);
    if (green == nullptr || blue == nullptr || open == nullptr ||
        strobe == nullptr) {
        return;
    }

    if (intensity != nullptr) {
        CHECK(intensity->kind ==
              emberlights::FixtureControlChoiceKind::DirectAttribute);
        CHECK(intensity->control_kind ==
              emberlights::FixtureParameterControlKind::Level);
        CHECK(intensity->enabled);
        CHECK(intensity->coverage.exact());
        CHECK(intensity->diagnostics.size() == 2U);
        for (const auto& diagnostic : intensity->diagnostics) {
            CHECK(diagnostic.encoding ==
                  showcore::ChannelEncoding::Linear16);
            CHECK(diagnostic.channel == 3U);
            CHECK(diagnostic.fine_channel == 4U);
            CHECK(diagnostic.raw_value == 64U);
            CHECK(diagnostic.raw_fine_value == 0U);
            CHECK(diagnostic.highlight_value == 65535U);
            CHECK(diagnostic.accessibility_label.find("fine channel 4") !=
                  std::string::npos);
        }
    }

    CHECK(green->coverage.supported_fixture_count == 1U);
    CHECK(green->coverage.target_fixture_count == 2U);
    CHECK(green->coverage.partial());
    CHECK(!green->enabled);
    CHECK(green->reason ==
          emberlights::FixtureFunctionReason::PartialGroupCoverage);
    CHECK(green->diagnostics.size() == 1U);

    CHECK(blue->coverage.exact());
    CHECK(!blue->shared_semantic_value);
    CHECK(!blue->enabled);
    CHECK(blue->reason ==
          emberlights::FixtureFunctionReason::ProfileValuesDiffer);
    CHECK(blue->diagnostics.size() == 2U);
    if (blue->diagnostics.size() == 2U) {
        CHECK(std::fabs(
            blue->diagnostics[0].normalized_value -
            blue->diagnostics[1].normalized_value) > 0.01F);
        CHECK(std::fabs(blue->diagnostics[0].semantic_min - 0.5F) <
              0.0001F);
        CHECK(std::fabs(
            blue->diagnostics[1].semantic_min - (2.0F / 3.0F)) <
              0.0001F);
        CHECK(blue->diagnostics[0].semantic_max == 1.0F);
        CHECK(blue->diagnostics[1].semantic_max == 1.0F);
    }

    CHECK(open->coverage.exact());
    CHECK(open->shared_semantic_value);
    CHECK(open->live_override_compatible);
    CHECK(open->enabled);
    CHECK(open->availability ==
          emberlights::FixtureFunctionRowAvailability::Enabled);
    CHECK(open->diagnostics.size() == 2U);
    CHECK(open->has_profile_specific_dmx);
    if (open->diagnostics.size() == 2U) {
        CHECK(open->diagnostics[0].raw_value == 10U);
        CHECK(open->diagnostics[1].raw_value == 15U);
        CHECK(open->diagnostics[0].profile_revision == "2");
        CHECK(open->diagnostics[1].profile_revision == "3");
        CHECK(open->diagnostics[0].accessibility_label.find("DMX 10") !=
              std::string::npos);
        CHECK(open->diagnostics[1].accessibility_label.find("DMX 15") !=
              std::string::npos);
    }
    CHECK(!open->accessibility_label.empty());
    CHECK(open->accessibility_description.find("2 of 2") !=
          std::string::npos);

    CHECK(!strobe->enabled);
    CHECK(strobe->safety_restricted);
    CHECK(strobe->availability ==
          emberlights::FixtureFunctionRowAvailability::SafetyConfirmationRequired);
    CHECK(strobe->reason ==
          emberlights::FixtureFunctionReason::SafetyGateRequired);
    CHECK(strobe->reason_text.find("Safety confirmation required") !=
          std::string::npos);
}

void test_exact_fixture_and_group_invocations() {
    const auto project = make_project();
    emberlights::FixtureFunctionComponentQuery fixture_query;
    fixture_query.target_id = "fixture-two";
    const auto fixture =
        emberlights::build_fixture_function_component(project, fixture_query);
    const auto* blue = find_row(fixture, "blue");
    const auto* intensity = find_row(fixture, "direct.intensity");
    CHECK(blue != nullptr);
    CHECK(intensity != nullptr);
    if (blue == nullptr) {
        return;
    }
    CHECK(blue->enabled);

    const auto fixture_set = emberlights::build_fixture_function_invocation(
        project,
        fixture,
        {blue->choice_id, emberlights::FixtureFunctionCommandAction::Set});
    CHECK(fixture_set);
    CHECK(fixture_set.invocation.has_value());
    if (fixture_set.invocation.has_value()) {
        CHECK(fixture_set.invocation->command ==
              emberlights::UiCommandId::FixtureOverridePropertySet);
        CHECK(fixture_set.invocation->target_id == "fixture-two");
        CHECK(fixture_set.invocation->property ==
              showcore::Property::ColorWheel);
        CHECK(std::fabs(
            fixture_set.invocation->number_value - blue->normalized_value) <
              0.000001);
    }

    const auto fixture_release =
        emberlights::build_fixture_function_invocation(
            project,
            fixture,
            {blue->choice_id,
             emberlights::FixtureFunctionCommandAction::Release});
    CHECK(fixture_release);
    if (fixture_release.invocation.has_value()) {
        CHECK(fixture_release.invocation->command ==
              emberlights::UiCommandId::FixtureOverridePropertyRelease);
        CHECK(fixture_release.invocation->target_id == "fixture-two");
        CHECK(fixture_release.invocation->property ==
              showcore::Property::ColorWheel);
        CHECK(fixture_release.invocation->number_value == 0.0);
    }

    if (intensity != nullptr) {
        const auto direct_set =
            emberlights::build_fixture_function_invocation(
                project,
                fixture,
                {intensity->choice_id,
                 emberlights::FixtureFunctionCommandAction::Set});
        CHECK(direct_set);
        if (direct_set.invocation.has_value()) {
            CHECK(direct_set.invocation->property ==
                  showcore::Property::Intensity);
            CHECK(std::fabs(direct_set.invocation->number_value - 0.5F) <
                  0.000001F);
        }
    }

    const auto group = group_model(project);
    const auto* open = find_row(group, "open");
    CHECK(open != nullptr);
    if (open == nullptr) {
        return;
    }
    const auto group_set = emberlights::build_fixture_function_invocation(
        project,
        group,
        {open->choice_id, emberlights::FixtureFunctionCommandAction::Set});
    CHECK(group_set);
    if (group_set.invocation.has_value()) {
        CHECK(group_set.invocation->command ==
              emberlights::UiCommandId::GroupOverridePropertySet);
        CHECK(group_set.invocation->target_id == "group-wheels");
        CHECK(group_set.invocation->property == showcore::Property::Shutter);
        CHECK(std::fabs(
            group_set.invocation->number_value - open->normalized_value) <
              0.000001);
    }
    const auto group_release =
        emberlights::build_fixture_function_invocation(
            project,
            group,
            {open->choice_id,
             emberlights::FixtureFunctionCommandAction::Release});
    CHECK(group_release);
    if (group_release.invocation.has_value()) {
        CHECK(group_release.invocation->command ==
              emberlights::UiCommandId::GroupOverridePropertyRelease);
        CHECK(group_release.invocation->property == showcore::Property::Shutter);
    }
}

void test_surface_availability_favorites_and_selection() {
    const auto project = make_project();

    emberlights::FixtureFunctionComponentQuery look_query;
    look_query.target_id = "group-wheels";
    look_query.surface = emberlights::FixtureParameterSurface::StaticLook;
    auto look = emberlights::build_fixture_function_component(
        project, look_query);
    const auto* initial_green = find_row(look, "green");
    const auto* initial_open = find_row(look, "open");
    CHECK(initial_green != nullptr);
    CHECK(initial_open != nullptr);
    if (initial_green == nullptr || initial_open == nullptr) {
        return;
    }
    const auto green_id = initial_green->choice_id;
    const auto open_id = initial_open->choice_id;
    look_query.favorite_choice_ids = {green_id, open_id};
    look = emberlights::build_fixture_function_component(project, look_query);
    const auto* green = find_row(look, "green");
    const auto* blue = find_row(look, "blue");
    const auto* strobe = find_row(look, "strobe-slow-fast");
    const auto* open = find_row(look, "open");
    CHECK(green != nullptr);
    CHECK(blue != nullptr);
    CHECK(strobe != nullptr);
    CHECK(open != nullptr);
    if (green != nullptr) {
        CHECK(green->enabled);
        CHECK(green->coverage.partial());
        CHECK(green->favorite);
    }
    if (blue != nullptr) {
        CHECK(blue->enabled);
        CHECK(!blue->shared_semantic_value);
    }
    if (strobe != nullptr) {
        CHECK(strobe->enabled);
        CHECK(strobe->safety_restricted);
        CHECK(strobe->reason_text.find("Runner arming") != std::string::npos);
    }
    if (open != nullptr) {
        CHECK(open->uses_exact_profile_value);
        CHECK(!open->accepts_position);
        CHECK(open->favorite);
    }
    CHECK(look.favorite_choice_count == 2U);

    look_query.favorites_only = true;
    look_query.selected_choice_id = green_id;
    look = emberlights::build_fixture_function_component(project, look_query);
    CHECK(look.rows.size() == 2U);
    CHECK(look.matching_choice_count == 2U);
    CHECK(look.selection_visible);
    CHECK(std::all_of(
        look.rows.begin(), look.rows.end(),
        [](const auto& row) { return row.favorite; }));

    emberlights::FixtureFunctionComponentQuery loop_query;
    loop_query.target_id = "group-wheels";
    loop_query.surface = emberlights::FixtureParameterSurface::Autoloop;
    const auto loop = emberlights::build_fixture_function_component(
        project, loop_query);
    const auto* loop_green = find_row(loop, "green");
    const auto* loop_strobe = find_row(loop, "strobe-slow-fast");
    CHECK(loop_green != nullptr && loop_green->enabled);
    CHECK(loop_strobe != nullptr && !loop_strobe->enabled);
    if (loop_strobe != nullptr) {
        CHECK(loop_strobe->reason ==
              emberlights::FixtureFunctionReason::SafetyGateRequired);
    }

    auto controller_query = loop_query;
    controller_query.surface =
        emberlights::FixtureParameterSurface::Controller;
    const auto controller = emberlights::build_fixture_function_component(
        project, controller_query);
    const auto* controller_green = find_row(controller, "green");
    const auto* controller_strobe =
        find_row(controller, "strobe-slow-fast");
    CHECK(controller_green != nullptr && controller_green->enabled);
    CHECK(controller_strobe != nullptr && !controller_strobe->enabled);

    const auto invalid_surface =
        emberlights::build_fixture_function_invocation(
            project,
            loop,
            {loop_green == nullptr ? std::string_view{} : loop_green->choice_id,
             emberlights::FixtureFunctionCommandAction::Set});
    CHECK(!invalid_surface);
    CHECK(invalid_surface.reason ==
          emberlights::FixtureFunctionReason::InvalidSurface);
}

void test_partial_safety_and_protected_refusal() {
    const auto project = make_project();
    const auto model = group_model(project);
    CHECK(std::none_of(
        model.rows.begin(), model.rows.end(),
        [](const auto& row) {
            return row.capability_id == "factory-reset" ||
                row.access == showcore::ChannelCapabilityAccess::Protected;
        }));

    const auto* green = find_row(model, "green");
    const auto* blue = find_row(model, "blue");
    const auto* strobe = find_row(model, "strobe-slow-fast");
    CHECK(green != nullptr);
    CHECK(blue != nullptr);
    CHECK(strobe != nullptr);
    if (green == nullptr || blue == nullptr || strobe == nullptr) {
        return;
    }
    const auto partial = emberlights::build_fixture_function_invocation(
        project,
        model,
        {green->choice_id, emberlights::FixtureFunctionCommandAction::Set});
    CHECK(!partial);
    CHECK(!partial.invocation.has_value());
    CHECK(partial.reason ==
          emberlights::FixtureFunctionReason::PartialGroupCoverage);

    const auto divergent = emberlights::build_fixture_function_invocation(
        project,
        model,
        {blue->choice_id, emberlights::FixtureFunctionCommandAction::Set});
    CHECK(!divergent);
    CHECK(divergent.reason ==
          emberlights::FixtureFunctionReason::ProfileValuesDiffer);

    const auto gated = emberlights::build_fixture_function_invocation(
        project,
        model,
        {strobe->choice_id, emberlights::FixtureFunctionCommandAction::Set});
    CHECK(!gated);
    CHECK(!gated.invocation.has_value());
    CHECK(gated.reason ==
          emberlights::FixtureFunctionReason::SafetyGateRequired);

    const auto protected_attempt =
        emberlights::build_fixture_function_invocation(
            project,
            model,
            {"target:protected-factory-reset",
             emberlights::FixtureFunctionCommandAction::Set});
    CHECK(!protected_attempt);
    CHECK(!protected_attempt.invocation.has_value());
    CHECK(protected_attempt.reason ==
          emberlights::FixtureFunctionReason::SelectionMissing);
}

void test_stale_missing_and_incomplete_selection() {
    const auto project = make_project();
    const auto model = group_model(project);
    const auto* open = find_row(model, "open");
    CHECK(open != nullptr);
    if (open == nullptr) {
        return;
    }
    const auto open_id = open->choice_id;

    auto changed_profile = project;
    changed_profile.fixture_profiles[0].channels[1].capabilities[0]
        .preferred_value = 19U;
    const auto stale = emberlights::build_fixture_function_invocation(
        changed_profile,
        model,
        {open_id, emberlights::FixtureFunctionCommandAction::Set});
    CHECK(!stale);
    CHECK(!stale.invocation.has_value());
    CHECK(stale.reason ==
          emberlights::FixtureFunctionReason::SelectionStale);

    auto missing_target = project;
    missing_target.groups.erase(missing_target.groups.begin());
    const auto missing = emberlights::build_fixture_function_invocation(
        missing_target,
        model,
        {open_id, emberlights::FixtureFunctionCommandAction::Set});
    CHECK(!missing);
    CHECK(missing.reason ==
          emberlights::FixtureFunctionReason::TargetNotFound);

    emberlights::FixtureFunctionComponentQuery filtered_query;
    filtered_query.target_id = "group-wheels";
    filtered_query.search = "green";
    const auto filtered = emberlights::build_fixture_function_component(
        project, filtered_query);
    const auto not_selected = emberlights::build_fixture_function_invocation(
        project,
        filtered,
        {open_id, emberlights::FixtureFunctionCommandAction::Set});
    CHECK(!not_selected);
    CHECK(not_selected.reason ==
          emberlights::FixtureFunctionReason::SelectionMissing);

    const auto invalid_action = emberlights::build_fixture_function_invocation(
        project,
        model,
        {open_id, static_cast<emberlights::FixtureFunctionCommandAction>(255U)});
    CHECK(!invalid_action);
    CHECK(invalid_action.reason ==
          emberlights::FixtureFunctionReason::InvalidAction);

    emberlights::FixtureFunctionComponentQuery missing_query;
    missing_query.target_id = "target-missing";
    const auto missing_model =
        emberlights::build_fixture_function_component(project, missing_query);
    CHECK(missing_model.state ==
          emberlights::FixtureFunctionComponentState::Unavailable);
    CHECK(missing_model.reason ==
          emberlights::FixtureFunctionReason::TargetNotFound);
    CHECK(missing_model.rows.empty());

    emberlights::FixtureFunctionComponentQuery empty_query;
    empty_query.target_id = "group-empty";
    const auto empty_model =
        emberlights::build_fixture_function_component(project, empty_query);
    CHECK(empty_model.state ==
          emberlights::FixtureFunctionComponentState::Empty);
    CHECK(empty_model.reason ==
          emberlights::FixtureFunctionReason::TargetEmpty);

    auto incomplete_project = project;
    incomplete_project.groups[0].fixture_ids.push_back("fixture-missing");
    const auto incomplete = group_model(incomplete_project);
    CHECK(!incomplete.target_complete);
    CHECK(std::none_of(
        incomplete.rows.begin(), incomplete.rows.end(),
        [](const auto& row) { return row.enabled; }));
    const auto* incomplete_open = find_row(incomplete, "open");
    CHECK(incomplete_open != nullptr);
    if (incomplete_open != nullptr) {
        CHECK(incomplete_open->reason ==
              emberlights::FixtureFunctionReason::TargetIncomplete);
        const auto refused = emberlights::build_fixture_function_invocation(
            incomplete_project,
            incomplete,
            {incomplete_open->choice_id,
             emberlights::FixtureFunctionCommandAction::Set});
        CHECK(!refused);
        CHECK(refused.reason ==
              emberlights::FixtureFunctionReason::TargetIncomplete);
    }
}

}  // namespace

int main() {
    test_search_filter_order_and_bounds();
    test_mixed_profile_rows_diagnostics_and_accessibility();
    test_exact_fixture_and_group_invocations();
    test_surface_availability_favorites_and_selection();
    test_partial_safety_and_protected_refusal();
    test_stale_missing_and_incomplete_selection();
    if (failures != 0) {
        std::cerr << failures << " fixture-function component test(s) failed\n";
        return 1;
    }
    std::cout << "fixture-function component tests passed\n";
    return 0;
}
