#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace emberlights {

struct UiColor {
    std::uint8_t red{0U};
    std::uint8_t green{0U};
    std::uint8_t blue{0U};
    std::uint8_t alpha{255U};

    [[nodiscard]] constexpr bool operator==(const UiColor&) const noexcept = default;
};

struct UiThemeTokenValue {
    std::string_view id;
    UiColor value;
};

// Toolkit-neutral Ember Dark token values. These names are the bridge from the
// current Win32 strangler to Default, Reference, Safe, and the accepted future
// toolkit; widgets must not invent alternate meanings for them.
[[nodiscard]] std::span<const UiThemeTokenValue> ember_dark_theme_tokens() noexcept;
[[nodiscard]] std::optional<UiColor> ember_dark_theme_color(
    std::string_view token_id) noexcept;

enum class UiWorkspaceKind : std::uint8_t {
    Live,
    Studio,
    System
};

struct UiNavigationVisual {
    std::string_view stable_target;
    std::string_view accessible_label;
    UiWorkspaceKind workspace{UiWorkspaceKind::Live};
    std::uint32_t fluent_glyph{0U};
};

// Glyph values follow Microsoft's Segoe Fluent Icons/MDL2 mapping. Text remains
// visible, so a missing icon font never removes an accessible navigation name.
[[nodiscard]] std::span<const UiNavigationVisual> ui_navigation_visuals() noexcept;
[[nodiscard]] std::optional<UiNavigationVisual> ui_navigation_visual(
    std::string_view stable_target) noexcept;

enum class UiShellDensity : std::uint8_t {
    Compact,
    Standard,
    Wide
};

struct UiRectangle {
    std::int32_t x{0};
    std::int32_t y{0};
    std::int32_t width{0};
    std::int32_t height{0};

    [[nodiscard]] constexpr std::int32_t right() const noexcept {
        return x + width;
    }

    [[nodiscard]] constexpr std::int32_t bottom() const noexcept {
        return y + height;
    }

    [[nodiscard]] constexpr bool has_area() const noexcept {
        return width > 0 && height > 0;
    }

    [[nodiscard]] constexpr bool operator==(
        const UiRectangle&) const noexcept = default;
};

struct UiShellLayout {
    UiShellDensity density{UiShellDensity::Standard};
    UiRectangle navigation;
    UiRectangle health_bar;
    UiRectangle page;
    UiRectangle status_bar;
    std::int32_t page_margin{24};
    std::int32_t control_radius{6};
    std::int32_t list_row_height{40};

    [[nodiscard]] constexpr bool operator==(
        const UiShellLayout&) const noexcept = default;
};

[[nodiscard]] UiShellDensity select_ui_shell_density(
    std::int32_t client_width) noexcept;
[[nodiscard]] UiShellLayout compute_ui_shell_layout(
    std::int32_t client_width,
    std::int32_t client_height) noexcept;

enum class UiStatusTone : std::uint8_t {
    Neutral,
    Info,
    Good,
    Warning,
    Danger
};

[[nodiscard]] UiColor ui_status_tone_color(UiStatusTone tone) noexcept;

}  // namespace emberlights
