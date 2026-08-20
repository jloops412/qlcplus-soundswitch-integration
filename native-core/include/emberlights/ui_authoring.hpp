#pragma once

#include "emberlights/ui_visual.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace emberlights {

// Toolkit-neutral contract for the Default Studio library + contextual
// Inspector pattern. The current Win32 host is one adapter; future skins and
// renderers consume the same stable resource identity, query, summary, and
// responsive geometry rules.
enum class UiAuthoringResourceKind : std::uint8_t {
    FixtureProfile,
    Fixture,
    FixtureGroup,
    StaticLook,
    Autoloop,
    TrackScript
};

struct UiAuthoringResourceDescriptor {
    std::string_view singular;
    std::string_view plural;
    std::string_view create_label;
    std::string_view search_hint;
};

[[nodiscard]] UiAuthoringResourceDescriptor authoring_resource_descriptor(
    UiAuthoringResourceKind kind) noexcept;

struct UiAuthoringItem {
    std::string stable_id;
    std::string primary_text;
    std::string secondary_text;
    std::vector<std::string> search_terms;
    bool read_only{false};
};

constexpr std::size_t kUiAuthoringMaximumQueryBytes = 256U;

struct UiAuthoringProjection {
    std::vector<std::size_t> source_indices;
    std::string normalized_query;
    std::size_t total_count{0U};
    std::optional<std::size_t> selected_source_index;
    std::optional<std::size_t> selected_visible_row;
    bool query_too_long{false};

    [[nodiscard]] bool query_active() const noexcept {
        return !normalized_query.empty();
    }

    [[nodiscard]] bool selection_hidden() const noexcept {
        return selected_source_index.has_value() &&
            !selected_visible_row.has_value();
    }
};

// Search is deterministic, ASCII case-insensitive, UTF-8 byte preserving, and
// ANDs whitespace-delimited tokens across stable ID, labels, and supplied
// metadata. Overlong queries fail closed; the UI adapter also enforces the
// same input bound.
[[nodiscard]] UiAuthoringProjection project_authoring_items(
    std::span<const UiAuthoringItem> items,
    std::string_view query,
    std::string_view selected_stable_id = {});

[[nodiscard]] std::string authoring_collection_summary(
    UiAuthoringResourceKind kind,
    const UiAuthoringProjection& projection);

enum class UiAuthoringInspectorMode : std::uint8_t {
    Empty,
    Creating,
    Editing,
    ReadOnly
};

struct UiAuthoringInspectorStatus {
    UiAuthoringResourceKind kind{UiAuthoringResourceKind::FixtureProfile};
    UiAuthoringInspectorMode mode{UiAuthoringInspectorMode::Empty};
    std::string primary_text;
    std::string stable_id;
    bool draft_changed{false};
};

[[nodiscard]] std::string authoring_inspector_heading(
    const UiAuthoringInspectorStatus& status);

enum class UiAuthoringCollectionEmphasis : std::uint8_t {
    Standard,
    Wide
};

struct UiAuthoringWorkbenchLayout {
    UiShellDensity density{UiShellDensity::Standard};
    UiRectangle library_panel;
    UiRectangle search;
    UiRectangle collection_summary;
    UiRectangle collection;
    UiRectangle library_actions;
    UiRectangle inspector_panel;
    UiRectangle inspector_heading;
    UiRectangle inspector_content;
    UiRectangle inspector_actions;

    [[nodiscard]] constexpr bool operator==(
        const UiAuthoringWorkbenchLayout&) const noexcept = default;
};

[[nodiscard]] UiAuthoringWorkbenchLayout compute_authoring_workbench_layout(
    std::int32_t client_width,
    std::int32_t client_height,
    UiShellDensity density,
    UiAuthoringCollectionEmphasis emphasis =
        UiAuthoringCollectionEmphasis::Standard) noexcept;

}  // namespace emberlights
