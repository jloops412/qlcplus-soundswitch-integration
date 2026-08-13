#include "emberlights/ui_visual.hpp"

#include <algorithm>
#include <array>

namespace emberlights {
namespace {

constexpr std::array<UiThemeTokenValue, 27> kEmberDarkTokens{{
    {"color.surface.app", {17U, 20U, 22U, 255U}},
    {"color.surface.chrome", {23U, 27U, 30U, 255U}},
    {"color.surface.panel", {29U, 34U, 38U, 255U}},
    {"color.surface.panelRaised", {37U, 43U, 48U, 255U}},
    {"color.surface.control", {41U, 48U, 54U, 255U}},
    {"color.surface.controlHover", {51U, 60U, 67U, 255U}},
    {"color.surface.input", {23U, 28U, 32U, 255U}},
    {"color.text.primary", {238U, 241U, 243U, 255U}},
    {"color.text.secondary", {183U, 192U, 198U, 255U}},
    {"color.text.muted", {127U, 138U, 146U, 255U}},
    {"color.text.disabled", {89U, 98U, 105U, 255U}},
    {"color.border.subtle", {48U, 56U, 62U, 255U}},
    {"color.border.standard", {65U, 75U, 83U, 255U}},
    {"color.focus.ring", {139U, 200U, 255U, 255U}},
    {"color.selection.fill", {63U, 154U, 219U, 51U}},
    {"color.selection.border", {84U, 174U, 230U, 255U}},
    {"color.brand.primary", {240U, 138U, 60U, 255U}},
    {"color.brand.primaryHover", {255U, 154U, 77U, 255U}},
    {"color.brand.primaryPressed", {207U, 111U, 45U, 255U}},
    {"color.brand.soft", {240U, 138U, 60U, 41U}},
    {"color.status.ok", {73U, 199U, 122U, 255U}},
    {"color.status.info", {79U, 165U, 222U, 255U}},
    {"color.status.warn", {231U, 178U, 74U, 255U}},
    {"color.status.error", {224U, 82U, 82U, 255U}},
    {"color.status.offline", {122U, 133U, 141U, 255U}},
    {"color.safety.blackout", {204U, 48U, 48U, 255U}},
    {"color.safety.blackoutActive", {240U, 68U, 68U, 255U}},
}};

constexpr std::array<UiNavigationVisual, 13> kNavigationVisuals{{
    {"live.home", "Dashboard", UiWorkspaceKind::Live, 0xE80FU},
    {"live.overrides", "Overrides", UiWorkspaceKind::Live, 0xE790U},
    {"studio.profiles", "Fixture Profiles", UiWorkspaceKind::Studio, 0xE781U},
    {"studio.patch", "Fixture Patch", UiWorkspaceKind::Studio, 0xE7C3U},
    {"studio.groups", "Groups", UiWorkspaceKind::Studio, 0xE902U},
    {"studio.staticLooks", "Static Looks", UiWorkspaceKind::Studio, 0xE8FFU},
    {"studio.autoloops", "Autoloops", UiWorkspaceKind::Studio, 0xE8EEU},
    {"studio.autoscript", "AutoScript", UiWorkspaceKind::Studio, 0xE9F5U},
    {"studio.trackScripts", "Track Scripts", UiWorkspaceKind::Studio, 0xE90BU},
    {"studio.mapping", "MIDI Mapping", UiWorkspaceKind::Studio, 0xE772U},
    {"system.connections", "Connections", UiWorkspaceKind::System, 0xE703U},
    {"system.safety", "Safety", UiWorkspaceKind::System, 0xEA18U},
    {"system.diagnostics", "Diagnostics", UiWorkspaceKind::System, 0xE9D9U},
}};

[[nodiscard]] constexpr std::int32_t nonnegative(std::int32_t value) noexcept {
    return std::max<std::int32_t>(0, value);
}

}  // namespace

std::span<const UiThemeTokenValue> ember_dark_theme_tokens() noexcept {
    return kEmberDarkTokens;
}

std::optional<UiColor> ember_dark_theme_color(
    std::string_view token_id) noexcept {
    const auto found = std::find_if(
        kEmberDarkTokens.begin(),
        kEmberDarkTokens.end(),
        [token_id](const auto& token) { return token.id == token_id; });
    return found == kEmberDarkTokens.end()
        ? std::nullopt
        : std::optional<UiColor>{found->value};
}

std::span<const UiNavigationVisual> ui_navigation_visuals() noexcept {
    return kNavigationVisuals;
}

std::optional<UiNavigationVisual> ui_navigation_visual(
    std::string_view stable_target) noexcept {
    const auto found = std::find_if(
        kNavigationVisuals.begin(),
        kNavigationVisuals.end(),
        [stable_target](const auto& visual) {
            return visual.stable_target == stable_target;
        });
    return found == kNavigationVisuals.end()
        ? std::nullopt
        : std::optional<UiNavigationVisual>{*found};
}

UiShellDensity select_ui_shell_density(std::int32_t client_width) noexcept {
    if (client_width < 1366) {
        return UiShellDensity::Compact;
    }
    if (client_width >= 1920) {
        return UiShellDensity::Wide;
    }
    return UiShellDensity::Standard;
}

UiShellLayout compute_ui_shell_layout(
    std::int32_t client_width,
    std::int32_t client_height) noexcept {
    const auto width = std::max<std::int32_t>(640, client_width);
    const auto height = std::max<std::int32_t>(480, client_height);
    const auto density = select_ui_shell_density(width);
    const auto navigation_width = density == UiShellDensity::Compact ? 224
        : density == UiShellDensity::Wide ? 260
        : 244;
    const auto health_height = density == UiShellDensity::Wide ? 72 : 64;
    const auto status_height = density == UiShellDensity::Compact ? 32 : 34;
    const auto page_margin = density == UiShellDensity::Compact ? 20 : 24;
    const auto page_height = nonnegative(height - health_height - status_height);
    return {
        density,
        {0, 0, navigation_width, height - status_height},
        {navigation_width, 0, width - navigation_width, health_height},
        {navigation_width, health_height, width - navigation_width, page_height},
        {0, height - status_height, width, status_height},
        page_margin,
        6,
        density == UiShellDensity::Compact ? 38 : 42};
}

UiColor ui_status_tone_color(UiStatusTone tone) noexcept {
    const auto token = [tone]() -> std::string_view {
        switch (tone) {
        case UiStatusTone::Info: return "color.status.info";
        case UiStatusTone::Good: return "color.status.ok";
        case UiStatusTone::Warning: return "color.status.warn";
        case UiStatusTone::Danger: return "color.status.error";
        case UiStatusTone::Neutral: return "color.status.offline";
        }
        return "color.status.offline";
    }();
    return ember_dark_theme_color(token).value_or(UiColor{122U, 133U, 141U, 255U});
}

}  // namespace emberlights
