#pragma once

#include "emberlights/project.hpp"
#include "emberlights/studio_color_types.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace emberlights {

[[nodiscard]] bool valid_studio_color(const StudioColor& color) noexcept;
[[nodiscard]] StudioRgbColor studio_rgb_from_hsv(StudioHsvColor color) noexcept;
[[nodiscard]] StudioHsvColor studio_hsv_from_rgb(StudioRgbColor color) noexcept;
[[nodiscard]] StudioRgbColor studio_rgb_from_hsl(StudioHslColor color) noexcept;
[[nodiscard]] StudioHslColor studio_hsl_from_rgb(StudioRgbColor color) noexcept;
[[nodiscard]] StudioRgbColor studio_rgb_from_cmy(StudioCmyColor color) noexcept;
[[nodiscard]] StudioCmyColor studio_cmy_from_rgb(StudioRgbColor color) noexcept;

// Approximate display preview for a black-body color temperature. Kelvin is
// clamped to 1000..40000 and tint to -1..1. This never claims fixture-calibrated
// photometric output; fixture realization still uses qualified channel meaning.
[[nodiscard]] StudioRgbColor studio_rgb_from_temperature(
    float kelvin,
    float tint = 0.0F) noexcept;

[[nodiscard]] bool parse_studio_hex_color(
    std::string_view text,
    StudioRgbColor& color) noexcept;
[[nodiscard]] std::string format_studio_hex_color(StudioRgbColor color);

enum class StudioPaletteMutationResult : std::uint8_t {
    Added,
    Updated,
    Removed,
    Moved,
    NoChange,
    Missing,
    Invalid,
    Capacity
};

class StudioColorPalette {
public:
    [[nodiscard]] const std::vector<StudioColorSwatch>& swatches() const noexcept {
        return swatches_;
    }
    [[nodiscard]] StudioPaletteMutationResult upsert(StudioColorSwatch swatch);
    [[nodiscard]] StudioPaletteMutationResult remove(std::string_view swatch_id);
    [[nodiscard]] StudioPaletteMutationResult move(
        std::string_view swatch_id,
        std::size_t destination_index);
    [[nodiscard]] const StudioColorSwatch* find(std::string_view swatch_id) const noexcept;

private:
    std::vector<StudioColorSwatch> swatches_;
};

[[nodiscard]] bool valid_studio_palette_asset(
    const StudioColorPaletteAsset& palette) noexcept;
[[nodiscard]] StudioPaletteMutationResult upsert_studio_palette_asset(
    ProjectDocument& project,
    StudioColorPaletteAsset palette);
[[nodiscard]] StudioPaletteMutationResult remove_studio_palette_asset(
    ProjectDocument& project,
    std::string_view palette_id);

enum class StudioColorRealizationStatus : std::uint8_t {
    Exact,
    Degraded,
    Unsupported,
    Invalid
};

struct StudioColorPropertyAssignment {
    showcore::Property property{showcore::Property::Intensity};
    showcore::PropertyValue value{};
};

struct StudioColorRealization {
    StudioColorRealizationStatus status{StudioColorRealizationStatus::Invalid};
    std::vector<StudioColorPropertyAssignment> assignments;
    std::vector<std::string> warnings;

    [[nodiscard]] bool usable() const noexcept {
        return status == StudioColorRealizationStatus::Exact ||
            status == StudioColorRealizationStatus::Degraded;
    }
};

// Maps picker intent only through semantic profile properties. It never infers
// a color wheel slot, raw DMX channel, or capability from a channel name.
[[nodiscard]] StudioColorRealization realize_studio_color(
    const FixtureProfileDefinition& profile,
    const StudioColor& color);

enum class StudioColorApplyResult : std::uint8_t {
    Applied,
    NoChange,
    TargetMissing,
    Unsupported,
    Invalid
};

struct StudioColorApplyOutcome {
    StudioColorApplyResult result{StudioColorApplyResult::Invalid};
    std::size_t fixtures_targeted{0U};
    std::size_t fixtures_realized{0U};
    std::size_t assignments_written{0U};
    std::vector<std::string> warnings;

    [[nodiscard]] explicit operator bool() const noexcept {
        return result == StudioColorApplyResult::Applied ||
            result == StudioColorApplyResult::NoChange;
    }
};

// Applies one picker commit to a fixture or group inside a draft Static Look.
// Existing color and picker-intensity properties for the target fixtures are
// replaced deterministically; unrelated properties remain independent.
[[nodiscard]] StudioColorApplyOutcome apply_studio_color_to_look(
    const ProjectDocument& project,
    std::string_view target_id,
    const StudioColor& color,
    LookDefinition& look);

[[nodiscard]] bool is_studio_color_property(showcore::Property property) noexcept;

}  // namespace emberlights
