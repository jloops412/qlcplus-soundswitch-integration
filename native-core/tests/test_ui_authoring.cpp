#include "emberlights/ui_authoring.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            std::cerr << "CHECK failed at " << __FILE__ << ':' << __LINE__    \
                      << ": " #condition << '\n';                            \
            ++failures;                                                         \
        }                                                                       \
    } while (false)

using emberlights::UiAuthoringCollectionEmphasis;
using emberlights::UiAuthoringInspectorMode;
using emberlights::UiAuthoringInspectorStatus;
using emberlights::UiAuthoringItem;
using emberlights::UiAuthoringResourceKind;
using emberlights::UiShellDensity;

[[nodiscard]] std::vector<UiAuthoringItem> example_items() {
    return {
        {"profile.bo-ir4-6ch", "BO-IR4 6CH", "Both Lighting • 6 channels",
         {"rgbwauv", "verified built-in"}, true},
        {"profile.wash-hex", "Wash FX Hex", "Chauvet DJ • 12 channels",
         {"wash", "local"}, false},
        {"profile.cafe", "Café Wash", "Éclairage • 8 channels",
         {"amber", "local"}, false},
    };
}

void test_descriptors_are_stable() {
    const auto profile = emberlights::authoring_resource_descriptor(
        UiAuthoringResourceKind::FixtureProfile);
    const auto loop = emberlights::authoring_resource_descriptor(
        UiAuthoringResourceKind::Autoloop);
    CHECK(profile.singular == "profile");
    CHECK(profile.create_label == "New Profile");
    CHECK(profile.search_hint.find("manufacturer") != std::string_view::npos);
    CHECK(loop.plural == "Autoloops");
}

void test_filtering_and_identity() {
    const auto items = example_items();
    auto result = emberlights::project_authoring_items(
        items, "  BOTH   rgbWAUv ", "profile.bo-ir4-6ch");
    CHECK(result.normalized_query == "both rgbwauv");
    CHECK(result.source_indices.size() == 1U);
    CHECK(result.source_indices[0] == 0U);
    CHECK(result.selected_source_index == 0U);
    CHECK(result.selected_visible_row == 0U);
    CHECK(!result.selection_hidden());

    result = emberlights::project_authoring_items(
        items, "chauvet 12", "profile.bo-ir4-6ch");
    CHECK(result.source_indices.size() == 1U);
    CHECK(result.source_indices[0] == 1U);
    CHECK(result.selected_source_index == 0U);
    CHECK(!result.selected_visible_row.has_value());
    CHECK(result.selection_hidden());

    result = emberlights::project_authoring_items(items, "profile.wash-hex");
    CHECK(result.source_indices.size() == 1U);
    CHECK(result.source_indices[0] == 1U);

    result = emberlights::project_authoring_items(items, "Café");
    CHECK(result.source_indices.size() == 1U);
    CHECK(result.source_indices[0] == 2U);
}

void test_empty_and_overlong_queries() {
    const auto items = example_items();
    auto result = emberlights::project_authoring_items(items, "   \t\r\n");
    CHECK(!result.query_active());
    CHECK(result.source_indices.size() == items.size());

    const std::string overlong(
        emberlights::kUiAuthoringMaximumQueryBytes + 1U, 'x');
    result = emberlights::project_authoring_items(items, overlong);
    CHECK(result.query_too_long);
    CHECK(result.source_indices.empty());
    CHECK(emberlights::authoring_collection_summary(
              UiAuthoringResourceKind::FixtureProfile, result)
              .find("limited") != std::string::npos);
}

void test_user_facing_summaries() {
    const auto items = example_items();
    auto result = emberlights::project_authoring_items(
        items, "chauvet", "profile.bo-ir4-6ch");
    const auto filtered = emberlights::authoring_collection_summary(
        UiAuthoringResourceKind::FixtureProfile, result);
    CHECK(filtered == "1 of 3 profiles shown • selection hidden by filter");

    result = emberlights::project_authoring_items(items, "does-not-exist");
    CHECK(emberlights::authoring_collection_summary(
              UiAuthoringResourceKind::FixtureProfile, result) ==
          "No matching profiles • Esc clears search");

    const std::vector<UiAuthoringItem> empty;
    result = emberlights::project_authoring_items(empty, {});
    CHECK(emberlights::authoring_collection_summary(
              UiAuthoringResourceKind::TrackScript, result) ==
          "No Track Scripts yet • New Script starts a draft");

    CHECK(emberlights::authoring_inspector_heading({
              UiAuthoringResourceKind::StaticLook,
              UiAuthoringInspectorMode::Creating,
              {},
              {},
              false}) == "INSPECTOR • New Look • new draft");
    CHECK(emberlights::authoring_inspector_heading({
              UiAuthoringResourceKind::FixtureProfile,
              UiAuthoringInspectorMode::ReadOnly,
              "BO-IR4 6CH",
              "profile.bo-ir4-6ch",
              false}) == "INSPECTOR • BO-IR4 6CH • read-only source");
    CHECK(emberlights::authoring_inspector_heading({
              UiAuthoringResourceKind::Autoloop,
              UiAuthoringInspectorMode::Editing,
              "Dance Warm",
              "autoloop.dance-warm",
              true}) == "INSPECTOR • Dance Warm • unsaved edits");
}

void check_layout(
    const emberlights::UiAuthoringWorkbenchLayout& layout,
    std::int32_t width,
    std::int32_t height) {
    const emberlights::UiRectangle viewport{0, 0, width, height};
    const auto contains = [](const auto& outer, const auto& inner) {
        return inner.x >= outer.x && inner.y >= outer.y &&
            inner.right() <= outer.right() && inner.bottom() <= outer.bottom();
    };
    CHECK(contains(viewport, layout.library_panel));
    CHECK(contains(viewport, layout.inspector_panel));
    CHECK(contains(layout.library_panel, layout.search));
    CHECK(contains(layout.library_panel, layout.collection_summary));
    CHECK(contains(layout.library_panel, layout.collection));
    CHECK(contains(layout.library_panel, layout.library_actions));
    CHECK(contains(layout.inspector_panel, layout.inspector_heading));
    CHECK(contains(layout.inspector_panel, layout.inspector_content));
    CHECK(contains(layout.inspector_panel, layout.inspector_actions));
    CHECK(layout.library_panel.right() < layout.inspector_panel.x);
    CHECK(layout.search.bottom() <= layout.collection_summary.y);
    CHECK(layout.collection_summary.bottom() <= layout.collection.y);
    CHECK(layout.collection.bottom() <= layout.library_actions.y);
    CHECK(layout.inspector_content.bottom() <= layout.inspector_actions.y);
}

void test_responsive_layout() {
    const auto compact = emberlights::compute_authoring_workbench_layout(
        976, 670, UiShellDensity::Compact);
    check_layout(compact, 976, 670);
    CHECK(compact.library_panel.width >= 260);

    const auto standard = emberlights::compute_authoring_workbench_layout(
        1440, 900, UiShellDensity::Standard);
    check_layout(standard, 1440, 900);
    CHECK(standard == emberlights::compute_authoring_workbench_layout(
                          1440, 900, UiShellDensity::Standard));

    const auto wide = emberlights::compute_authoring_workbench_layout(
        1660,
        980,
        UiShellDensity::Wide,
        UiAuthoringCollectionEmphasis::Wide);
    check_layout(wide, 1660, 980);
    CHECK(wide.library_panel.width > standard.library_panel.width);
    CHECK(wide.collection.has_area());
    CHECK(wide.inspector_content.has_area());
}

}  // namespace

int main() {
    test_descriptors_are_stable();
    test_filtering_and_identity();
    test_empty_and_overlong_queries();
    test_user_facing_summaries();
    test_responsive_layout();
    if (failures != 0) {
        std::cerr << failures << " UI authoring workbench test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "UI authoring workbench tests passed\n";
    return EXIT_SUCCESS;
}
