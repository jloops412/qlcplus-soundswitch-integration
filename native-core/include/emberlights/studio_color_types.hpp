#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace emberlights {

inline constexpr std::uint32_t kStudioColorPaletteAssetVersion = 1U;
inline constexpr std::size_t kMaximumStudioPaletteAssets = 256U;
inline constexpr std::size_t kMaximumStudioPaletteSwatches = 1024U;
inline constexpr std::size_t kMaximumStudioPaletteSwatchesTotal = 8192U;
inline constexpr std::size_t kMaximumStudioPaletteNameLength = 255U;
inline constexpr std::size_t kMaximumStudioSwatchNameLength = 255U;

struct StudioRgbColor {
    float red{0.0F};
    float green{0.0F};
    float blue{0.0F};

    [[nodiscard]] friend bool operator==(
        const StudioRgbColor&,
        const StudioRgbColor&) = default;
};

struct StudioHsvColor {
    float hue_degrees{0.0F};
    float saturation{0.0F};
    float value{0.0F};
};

struct StudioHslColor {
    float hue_degrees{0.0F};
    float saturation{0.0F};
    float lightness{0.0F};
};

struct StudioCmyColor {
    float cyan{0.0F};
    float magenta{0.0F};
    float yellow{0.0F};
};

// Fixture-independent picker intent. RGB/HSV/HSL/CMY and Kelvin controls edit
// the display RGB triplet; dedicated emitters stay explicit so a user can
// deliberately mix White, Amber, UV, Lime, and Indigo instead of losing them
// through an RGB-only conversion.
struct StudioColor {
    StudioRgbColor rgb{};
    float white{0.0F};
    float amber{0.0F};
    float uv{0.0F};
    float lime{0.0F};
    float indigo{0.0F};
    float intensity{1.0F};

    [[nodiscard]] friend bool operator==(
        const StudioColor&,
        const StudioColor&) = default;
};

struct StudioColorSwatch {
    std::string id;
    std::string name;
    StudioColor color{};

    [[nodiscard]] friend bool operator==(
        const StudioColorSwatch&,
        const StudioColorSwatch&) = default;
};

// Project-owned reusable palette asset. The v1 project records are additive to
// the outer format-1 container so earlier readers preserve them as unknown
// records instead of silently discarding color intent.
struct StudioColorPaletteAsset {
    std::uint32_t asset_version{kStudioColorPaletteAssetVersion};
    std::string id;
    std::string name;
    std::vector<StudioColorSwatch> swatches;

    [[nodiscard]] friend bool operator==(
        const StudioColorPaletteAsset&,
        const StudioColorPaletteAsset&) = default;
};

}  // namespace emberlights
