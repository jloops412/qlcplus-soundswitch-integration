#pragma once

#include "emberlights/fixture_capabilities.hpp"
#include "emberlights/fixture_parameter_catalog.hpp"
#include "emberlights/ui_command.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace emberlights {

// This is a toolkit-neutral snapshot/controller boundary for future native
// EmberSkin controls. The stable type ID is retained for compatibility even
// though v3 presents the complete Fixture Control catalog (direct channels plus
// named compound-channel functions). It never owns command dispatch or live
// state. "Attribute Cue" is intentionally not used here: that term is reserved
// for a future reusable authored cue built from these lower-level controls.
inline constexpr std::string_view kFixtureFunctionComponentType =
    "ember.fixtureFunctionBrowser";
inline constexpr std::uint16_t kFixtureFunctionComponentVersion = 4U;
inline constexpr std::size_t kFixtureFunctionComponentDefaultRowLimit = 128U;
inline constexpr std::size_t kFixtureFunctionComponentMaximumRowLimit = 512U;
inline constexpr std::size_t kFixtureFunctionComponentMaximumSearchBytes = 128U;
inline constexpr std::size_t kFixtureFunctionComponentMaximumWarnings = 64U;

enum class FixtureFunctionTargetKind : std::uint8_t {
    Missing,
    Fixture,
    Group
};

enum class FixtureFunctionComponentState : std::uint8_t {
    Ready,
    Empty,
    Unavailable,
    Degraded
};

enum class FixtureFunctionRowAvailability : std::uint8_t {
    Enabled,
    Unavailable,
    SafetyConfirmationRequired
};

enum class FixtureFunctionReason : std::uint8_t {
    None,
    TargetNotFound,
    TargetEmpty,
    TargetIncomplete,
    NoFunctions,
    NoMatches,
    NoLiveCompatibleFunctions,
    PartialGroupCoverage,
    ProfileValuesDiffer,
    SafetyGateRequired,
    SelectionMissing,
    SelectionStale,
    InconsistentChoice,
    RowLimitReached,
    InvalidAction,
    InvalidSurface
};

struct FixtureFunctionComponentQuery {
    std::string_view target_id;
    FixtureParameterSurface surface{FixtureParameterSurface::LiveOverride};
    float position{0.5F};
    std::string_view search;
    std::optional<FixtureParameterCategory> category;
    std::vector<std::string_view> favorite_choice_ids;
    bool favorites_only{false};
    std::string_view selected_choice_id;
    std::size_t row_limit{kFixtureFunctionComponentDefaultRowLimit};
};

struct FixtureFunctionCoverage {
    std::size_t supported_fixture_count{0U};
    std::size_t target_fixture_count{0U};

    [[nodiscard]] bool exact() const noexcept {
        return target_fixture_count != 0U &&
            supported_fixture_count == target_fixture_count;
    }

    [[nodiscard]] bool partial() const noexcept {
        return supported_fixture_count != 0U && !exact();
    }
};

// Raw bytes are inspection evidence for the selected profile attribute. They
// are never copied into UiCommandInvocation; commands retain semantic property
// and normalized-value arguments only.
struct FixtureFunctionDmxDiagnostic {
    std::string fixture_id;
    std::string fixture_name;
    std::string profile_id;
    std::string profile_name;
    std::string profile_revision;
    std::string binding_id;
    std::uint16_t channel{0U};
    showcore::Property property{showcore::Property::Count};
    float normalized_value{0.0F};
    float semantic_min{0.0F};
    float semantic_max{1.0F};
    std::uint8_t raw_value{0U};
    std::uint8_t dmx_min{0U};
    std::uint8_t dmx_max{0U};
    showcore::ChannelEncoding encoding{showcore::ChannelEncoding::Linear8};
    std::uint16_t fine_channel{0U};
    std::uint8_t raw_fine_value{0U};
    std::uint16_t default_value{0U};
    std::uint16_t blackout_value{0U};
    std::uint16_t highlight_value{255U};
    std::string accessibility_label;
};

struct FixtureFunctionRow {
    // Stable catalog identity. UI adapters return this ID, never a list index.
    std::string choice_id;
    std::string capability_id;
    std::string name;
    std::string owner;
    FixtureControlChoiceKind kind{FixtureControlChoiceKind::NamedCapability};
    FixtureParameterCategory category{FixtureParameterCategory::Custom};
    std::string category_label;
    showcore::Property property{showcore::Property::Count};
    std::string property_label;
    FixtureParameterControlKind control_kind{FixtureParameterControlKind::Custom};
    std::string control_kind_label;
    showcore::ChannelCapabilityBehavior behavior{
        showcore::ChannelCapabilityBehavior::Slot};
    showcore::ChannelCapabilityAccess access{
        showcore::ChannelCapabilityAccess::Selectable};
    FixtureChannelCapabilityRole role{FixtureChannelCapabilityRole::Function};
    FixtureFunctionCoverage coverage;
    float normalized_value{0.0F};
    bool shared_semantic_value{false};
    bool has_profile_specific_dmx{false};
    bool live_override_compatible{false};
    bool favorite{false};
    bool accepts_position{false};
    bool uses_exact_profile_value{false};
    bool safety_restricted{false};
    bool enabled{false};
    FixtureFunctionRowAvailability availability{
        FixtureFunctionRowAvailability::Unavailable};
    FixtureFunctionReason reason{FixtureFunctionReason::InconsistentChoice};
    std::string reason_text;
    std::vector<FixtureFunctionDmxDiagnostic> diagnostics;
    std::string accessibility_label;
    std::string accessibility_description;
};

struct FixtureFunctionCategorySummary {
    FixtureParameterCategory category{FixtureParameterCategory::Custom};
    std::string label;
    // Total excludes protected ranges. Search matches are counted before the
    // optional category filter; visible rows are after all filters and limits.
    std::size_t total_count{0U};
    std::size_t search_match_count{0U};
    std::size_t favorite_count{0U};
    std::size_t visible_count{0U};
};

struct FixtureFunctionComponentModel {
    FixtureFunctionComponentState state{
        FixtureFunctionComponentState::Unavailable};
    FixtureFunctionTargetKind target_kind{FixtureFunctionTargetKind::Missing};
    FixtureParameterSurface surface{FixtureParameterSurface::LiveOverride};
    std::string target_id;
    std::string target_name;
    std::size_t target_fixture_count{0U};
    float position{0.5F};
    std::string search_query;
    std::optional<FixtureParameterCategory> category_filter;
    std::string selected_choice_id;
    std::size_t source_choice_count{0U};
    std::size_t matching_choice_count{0U};
    std::size_t favorite_choice_count{0U};
    bool target_complete{false};
    bool favorites_only{false};
    bool selection_visible{false};
    bool rows_truncated{false};
    bool search_truncated{false};
    bool warnings_truncated{false};
    FixtureFunctionReason reason{FixtureFunctionReason::None};
    std::string message;
    std::string accessibility_label;
    std::vector<FixtureFunctionCategorySummary> categories;
    std::vector<FixtureFunctionRow> rows;
    std::vector<std::string> warnings;
};

// Builds a detached snapshot from an immutable project document. Search is
// ASCII case-insensitive, whitespace-tokenized, and bounded. Catalog order is
// preserved so filtering never changes stable category/function ordering.
// Availability is evaluated for the requested surface: Live requires one exact
// group value, while Static Looks, Autoloops, and controller mappings can retain
// per-fixture profile values under their own safety rules.
[[nodiscard]] FixtureFunctionComponentModel build_fixture_function_component(
    const ProjectDocument& project,
    const FixtureFunctionComponentQuery& query);

enum class FixtureFunctionCommandAction : std::uint8_t {
    Set,
    Release
};

struct FixtureFunctionCommandRequest {
    std::string_view choice_id;
    FixtureFunctionCommandAction action{FixtureFunctionCommandAction::Set};
};

struct FixtureFunctionCommandBuildResult {
    FixtureFunctionReason reason{FixtureFunctionReason::SelectionMissing};
    std::string message;
    std::optional<UiCommandInvocation> invocation;

    [[nodiscard]] explicit operator bool() const noexcept {
        return reason == FixtureFunctionReason::None && invocation.has_value();
    }
};

// Re-resolves a stable row ID against the current immutable project and
// refuses stale, partial, profile-divergent, safety-gated, or protected input.
// It constructs but never dispatches the existing registry-backed override
// command. The project must outlive the returned invocation because target_id
// is a view into its canonical stable target ID.
[[nodiscard]] FixtureFunctionCommandBuildResult
build_fixture_function_invocation(
    const ProjectDocument& project,
    const FixtureFunctionComponentModel& snapshot,
    const FixtureFunctionCommandRequest& request);

[[nodiscard]] const char* fixture_function_reason_name(
    FixtureFunctionReason reason) noexcept;

}  // namespace emberlights
