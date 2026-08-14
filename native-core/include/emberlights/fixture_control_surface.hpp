#pragma once

#include "emberlights/fixture_function_component.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace emberlights {

// Presentation contract for a task-facing fixture control surface. It turns
// the exact catalog rows into appropriate visual-control groups without
// copying raw DMX diagnostics into the ordinary editing surface. Renderers may
// lay these widgets out differently, but they must preserve the stable choice
// IDs, availability, safety, and per-surface decisions from the catalog.
inline constexpr std::string_view kFixtureControlSurfaceComponentType =
    "ember.fixtureControlSurface";
inline constexpr std::uint16_t kFixtureControlSurfaceComponentVersion = 1U;

enum class FixtureControlWidgetKind : std::uint8_t {
    LevelFader,
    ColorMixer,
    XYPad,
    PositionFader,
    RateControl,
    SlotTiles,
    SafetyTrigger,
    CustomControl
};

struct FixtureControlWidgetBinding {
    std::string choice_id;
    std::string label;
    showcore::Property property{showcore::Property::Count};
    float normalized_value{0.0F};
    bool accepts_value{false};
    bool enabled{false};
    bool favorite{false};
    bool safety_restricted{false};
    std::string availability_text;
    std::string accessibility_label;
};

struct FixtureControlWidget {
    std::string stable_id;
    std::string label;
    FixtureControlWidgetKind kind{FixtureControlWidgetKind::CustomControl};
    FixtureParameterCategory category{FixtureParameterCategory::Custom};
    std::vector<FixtureControlWidgetBinding> bindings;
    bool enabled{false};
    bool degraded{false};
    bool safety_restricted{false};
    std::string accessibility_label;
};

struct FixtureControlSurfaceSection {
    FixtureParameterCategory category{FixtureParameterCategory::Custom};
    std::string label;
    std::vector<FixtureControlWidget> widgets;
};

struct FixtureControlSurfaceModel {
    FixtureFunctionComponentState state{
        FixtureFunctionComponentState::Unavailable};
    FixtureParameterSurface surface{FixtureParameterSurface::LiveOverride};
    std::string target_id;
    std::string target_name;
    std::string selected_choice_id;
    std::vector<FixtureControlSurfaceSection> sections;
    std::size_t visible_binding_count{0U};
    std::size_t hidden_advanced_count{0U};
    bool diagnostics_available{false};
    bool has_degraded_controls{false};
    std::string message;
    std::string accessibility_label;
};

// Custom/unclassified controls are hidden by default and belong behind an
// explicit Advanced disclosure. Diagnostics remain available through the
// source FixtureFunctionComponentModel but are intentionally absent from the
// ordinary widget bindings returned here.
[[nodiscard]] FixtureControlSurfaceModel build_fixture_control_surface(
    const FixtureFunctionComponentModel& catalog,
    bool include_advanced = false);

[[nodiscard]] std::string_view fixture_control_widget_kind_name(
    FixtureControlWidgetKind kind) noexcept;

}  // namespace emberlights
