#include "emberlights/ui_visual.hpp"

#include <cstdlib>
#include <iostream>
#include <set>
#include <string_view>

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

void test_theme_contract() {
    const auto tokens = emberlights::ember_dark_theme_tokens();
    CHECK(tokens.size() >= 20U);
    std::set<std::string_view> ids;
    for (const auto& token : tokens) {
        CHECK(!token.id.empty());
        CHECK(ids.insert(token.id).second);
        CHECK(token.value.alpha > 0U);
    }
    constexpr emberlights::UiColor app{17U, 20U, 22U, 255U};
    constexpr emberlights::UiColor brand{240U, 138U, 60U, 255U};
    constexpr emberlights::UiColor danger{224U, 82U, 82U, 255U};
    CHECK(emberlights::ember_dark_theme_color("color.surface.app") == app);
    CHECK(emberlights::ember_dark_theme_color("color.brand.primary") == brand);
    CHECK(!emberlights::ember_dark_theme_color("toolkit.win32.brush").has_value());
    CHECK(emberlights::ui_status_tone_color(emberlights::UiStatusTone::Danger) ==
          danger);
}

void test_navigation_contract() {
    const auto visuals = emberlights::ui_navigation_visuals();
    CHECK(visuals.size() == 13U);
    std::set<std::string_view> targets;
    for (const auto& visual : visuals) {
        CHECK(!visual.stable_target.empty());
        CHECK(!visual.accessible_label.empty());
        CHECK(visual.fluent_glyph >= 0xE700U);
        CHECK(targets.insert(visual.stable_target).second);
    }
    const auto looks = emberlights::ui_navigation_visual("studio.staticLooks");
    CHECK(looks.has_value());
    if (looks.has_value()) {
        CHECK(looks->workspace == emberlights::UiWorkspaceKind::Studio);
        CHECK(looks->accessible_label == "Static Looks");
    }
    CHECK(!emberlights::ui_navigation_visual("studio.not-real").has_value());
}

void test_shell_layout() {
    const auto compact = emberlights::compute_ui_shell_layout(1200, 820);
    CHECK(compact.density == emberlights::UiShellDensity::Compact);
    CHECK(compact.navigation.width == 224);
    CHECK(compact.health_bar.y == 0);
    CHECK(compact.page.y == compact.health_bar.bottom());
    CHECK(compact.page.bottom() == compact.status_bar.y);
    CHECK(compact.navigation.bottom() == compact.status_bar.y);
    CHECK(compact.page.has_area());

    const auto standard = emberlights::compute_ui_shell_layout(1440, 900);
    CHECK(standard.density == emberlights::UiShellDensity::Standard);
    CHECK(standard.navigation.width == 244);
    CHECK(standard.list_row_height == 42);
    CHECK(standard == emberlights::compute_ui_shell_layout(1440, 900));

    const auto wide = emberlights::compute_ui_shell_layout(1920, 1080);
    CHECK(wide.density == emberlights::UiShellDensity::Wide);
    CHECK(wide.navigation.width == 260);
    CHECK(wide.health_bar.height == 72);
    CHECK(wide.page.right() == 1920);
    CHECK(wide.status_bar.bottom() == 1080);
}

}  // namespace

int main() {
    test_theme_contract();
    test_navigation_contract();
    test_shell_layout();
    if (failures != 0) {
        std::cerr << failures << " UI visual contract test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "UI visual contract tests passed\n";
    return EXIT_SUCCESS;
}
