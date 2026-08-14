#pragma once

#include "emberlights/fixture_control_surface.hpp"
#include "emberlights/ui_authoring.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace emberlights {

// Renderer-neutral composition model for the first replacement-shell slice.
// It joins the existing profile, patch, Static Look, and task-facing fixture
// control contracts without introducing toolkit types or a second mutation
// path. Slint, a future accepted renderer, Safe, skins, and test harnesses may
// project this detached snapshot differently while retaining stable IDs.
inline constexpr std::string_view kFixturesLooksShellSliceId =
    "studio.fixtures-static-looks";
inline constexpr std::uint16_t kFixturesLooksShellModelVersion = 1U;
inline constexpr std::int32_t kFixturesLooksMinimumWidth = 1366;
inline constexpr std::int32_t kFixturesLooksMinimumHeight = 768;

enum class FixturesLooksShellState : std::uint8_t {
    Ready,
    EmptyProject,
    NoTarget,
    NoStaticLook,
    SelectionRequired,
    Degraded
};

enum class StaticLookOwnershipState : std::uint8_t {
    Release,
    Set,
    ForceZero,
    Mixed
};

struct FixturesLooksProfileItem {
    std::string stable_id;
    std::string name;
    std::string manufacturer;
    std::string model;
    std::string mode;
    std::string source_label;
    std::string source_revision;
    std::string footprint_label;
    std::size_t patched_fixture_count{0U};
    bool selected{false};
    bool read_only{false};
    std::string accessibility_label;
};

struct FixturesLooksTargetItem {
    std::string stable_id;
    std::string name;
    std::string detail;
    std::size_t fixture_count{0U};
    bool group{false};
    bool selected{false};
    bool complete{false};
    std::string accessibility_label;
};

struct FixturesLooksStaticLookItem {
    std::string stable_id;
    std::string name;
    std::string detail;
    std::uint32_t fade_ms{0U};
    std::size_t assignment_count{0U};
    bool selected{false};
    std::string accessibility_label;
};

// Flattened renderer projection of one task-facing widget binding. widget_id
// keeps grouped controls (for example RGBWA or Pan/Tilt) together. choice_id
// remains the profile-backed stable identity used by authoring, Live,
// Autoloops, mappings, Ember Actions, migration, and future skins.
struct FixturesLooksControlBinding {
    std::string widget_id;
    std::string choice_id;
    std::string section_label;
    std::string widget_label;
    std::string control_kind;
    showcore::Property property{showcore::Property::Count};
    std::string property_label;
    StaticLookOwnershipState ownership{StaticLookOwnershipState::Release};
    float normalized_value{0.0F};
    std::size_t assigned_fixture_count{0U};
    std::size_t target_fixture_count{0U};
    bool value_mixed{false};
    bool value_matches_choice{false};
    bool selected{false};
    bool enabled{false};
    bool safety_restricted{false};
    std::string availability_text;
    std::string ownership_text;
    std::string accessibility_label;
};

struct FixturesLooksShellQuery {
    std::string_view profile_search;
    std::string_view static_look_search;
    std::string_view selected_profile_id;
    std::string_view selected_target_id;
    std::string_view selected_static_look_id;
    std::string_view selected_choice_id;
    bool include_advanced{false};
    bool advanced_open{false};
    bool live_running{false};
    bool read_only{false};
    std::int32_t viewport_width{kFixturesLooksMinimumWidth};
    std::int32_t viewport_height{kFixturesLooksMinimumHeight};
};

struct FixturesLooksShellModel {
    FixturesLooksShellState state{FixturesLooksShellState::EmptyProject};
    UiShellDensity density{UiShellDensity::Compact};
    std::string project_id;
    std::string project_name;
    std::string selected_profile_id;
    std::string selected_target_id;
    std::string selected_static_look_id;
    std::string selected_static_look_name;
    std::vector<FixturesLooksProfileItem> profiles;
    std::vector<FixturesLooksTargetItem> targets;
    std::vector<FixturesLooksStaticLookItem> static_looks;
    std::vector<FixturesLooksControlBinding> controls;
    FixtureControlSurfaceModel control_surface;
    std::size_t profile_total_count{0U};
    std::size_t static_look_total_count{0U};
    std::size_t validation_error_count{0U};
    std::size_t validation_warning_count{0U};
    bool minimum_viewport_supported{false};
    bool read_only{false};
    bool can_edit{false};
    bool can_preview{false};
    bool live_running{false};
    bool advanced_open{false};
    bool advanced_available{false};
    std::string profile_summary;
    std::string static_look_summary;
    std::string preview_status;
    std::string validation_status;
    std::string message;
    std::string accessibility_label;
};

// Empty selection IDs choose the first available item. A non-empty stale ID is
// never silently redirected: the returned model enters SelectionRequired and
// preserves the requested ID for diagnostics.
[[nodiscard]] FixturesLooksShellModel build_fixtures_looks_shell_model(
    const ProjectDocument& project,
    const FixturesLooksShellQuery& query = {});

[[nodiscard]] std::string_view fixtures_looks_shell_state_name(
    FixturesLooksShellState state) noexcept;
[[nodiscard]] std::string_view static_look_ownership_state_name(
    StaticLookOwnershipState state) noexcept;

}  // namespace emberlights
