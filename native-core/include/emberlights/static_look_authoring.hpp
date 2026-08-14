#pragma once

#include "emberlights/fixture_capabilities.hpp"
#include "emberlights/studio_document.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace emberlights {

struct StaticLookColor {
    float red{0.0F};
    float green{0.0F};
    float blue{0.0F};
    float white{0.0F};
    float amber{0.0F};
    float uv{0.0F};
    float intensity{1.0F};
};

struct StaticLookSwatch {
    std::string_view id;
    std::string_view name;
    StaticLookColor color;
};

struct StaticLookColorApplyOptions {
    bool open_master_intensity{true};
    bool force_strobe_off{true};
};

enum class StaticLookAuthoringResult : std::uint8_t {
    Applied,
    NoChange,
    TargetNotFound,
    EmptyTarget,
    Unsupported,
    InvalidValue
};

struct StaticLookAuthoringOutcome {
    StaticLookAuthoringResult result{StaticLookAuthoringResult::NoChange};
    std::size_t fixtures_considered{0U};
    std::size_t fixtures_modified{0U};
    std::size_t assignments_written{0U};
    std::size_t fixtures_skipped{0U};
    std::vector<std::string> warnings;

    [[nodiscard]] explicit operator bool() const noexcept {
        return result == StaticLookAuthoringResult::Applied ||
            result == StaticLookAuthoringResult::NoChange;
    }
};

struct StaticLookDraft {
    StudioDocumentGeneration base_generation{0U};
    std::optional<std::size_t> source_index;
    LookDefinition look;
};

struct StaticLookDependencyReport {
    std::size_t autoloop_steps{0U};
    std::size_t track_cues{0U};
    std::size_t midi_bindings{0U};
    std::vector<std::string> dependents;

    [[nodiscard]] bool blocked() const noexcept {
        return autoloop_steps != 0U || track_cues != 0U || midi_bindings != 0U;
    }
};

[[nodiscard]] StaticLookDraft make_static_look_draft(
    StudioDocumentGeneration generation,
    std::string id,
    std::string name = "New Static Look");

[[nodiscard]] std::optional<StaticLookDraft> load_static_look_draft(
    const StudioDocumentSnapshot& snapshot,
    std::size_t look_index);

[[nodiscard]] StaticLookDraft duplicate_static_look_draft(
    const StaticLookDraft& source,
    std::string new_id,
    std::string new_name);

// Full color ownership writes SET values for every supported RGBWAUV emitter,
// including zero. White, Amber, and UV remain direct independent emitters; RGB
// picker values never synthesize them without a future measured calibration.
[[nodiscard]] StaticLookAuthoringOutcome apply_static_look_color(
    StaticLookDraft& draft,
    const ProjectDocument& project,
    std::string_view target_id,
    const StaticLookColor& color,
    StaticLookColorApplyOptions options = {});

[[nodiscard]] StaticLookAuthoringOutcome apply_static_look_property(
    StaticLookDraft& draft,
    const ProjectDocument& project,
    std::string_view target_id,
    showcore::Property property,
    showcore::PropertyValue value);

// Applies one human-readable profile function through its exact per-fixture
// semantic realizations. This is the Static Look counterpart to choosing a
// profile-backed attribute in Live or a future skin/controller picker; no raw DMX value
// enters the authored Look.
[[nodiscard]] StaticLookAuthoringOutcome apply_static_look_control_choice(
    StaticLookDraft& draft,
    const ProjectDocument& project,
    std::string_view target_id,
    std::string_view choice_id,
    float position = 0.5F);

[[nodiscard]] StaticLookAuthoringOutcome remove_static_look_property(
    StaticLookDraft& draft,
    const ProjectDocument& project,
    std::string_view target_id,
    showcore::Property property);

[[nodiscard]] StudioMutationOutcome commit_static_look_draft(
    StudioDocumentService& document,
    StaticLookDraft& draft);

[[nodiscard]] StaticLookDependencyReport inspect_static_look_dependencies(
    const ProjectDocument& project,
    std::string_view look_id);

[[nodiscard]] bool parse_rgb_hex(
    std::string_view text,
    StaticLookColor& color) noexcept;
[[nodiscard]] std::string format_rgb_hex(const StaticLookColor& color);
[[nodiscard]] std::span<const StaticLookSwatch> built_in_static_look_swatches() noexcept;

}  // namespace emberlights
