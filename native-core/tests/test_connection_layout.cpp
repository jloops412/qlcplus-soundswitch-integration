#include "emberlights/connection_layout.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>

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

using emberlights::ConnectionKeyboardIntent;
using emberlights::ConnectionLayout;
using emberlights::ConnectionLayoutAction;
using emberlights::ConnectionLayoutInput;
using emberlights::ConnectionLayoutItem;

[[nodiscard]] constexpr std::size_t index_of(ConnectionLayoutItem item) noexcept {
    return static_cast<std::size_t>(item);
}

struct DisplayCase {
    std::int32_t width;
    std::int32_t height;
    std::uint16_t dpi;
};

[[nodiscard]] ConnectionLayout layout_for_display(
    const DisplayCase& display,
    std::int32_t scroll_offset = 0) {
    // Conservatively reserve scaled navigation/status/window chrome. The real
    // Win32 client area is at least this large when maximized at these display
    // sizes, so passing this matrix does not rely on title/menu coordinates.
    const auto scale = [dpi = display.dpi](std::int32_t value) {
        return static_cast<std::int32_t>(
            (static_cast<std::int64_t>(value) * dpi + 48) / 96);
    };
    return emberlights::compute_connection_layout({
        display.width - scale(176),
        display.height - scale(76),
        display.dpi,
        scroll_offset});
}

void check_action_bar(const ConnectionLayout& layout) {
    const auto& refresh =
        layout.items[index_of(ConnectionLayoutItem::Refresh)];
    const auto& copy =
        layout.items[index_of(ConnectionLayoutItem::CopyVirtualDjSetup)];
    const auto& apply =
        layout.items[index_of(ConnectionLayoutItem::SaveAndApply)];
    const auto& message =
        layout.items[index_of(ConnectionLayoutItem::Message)];

    CHECK(emberlights::connection_layout_contains(layout.viewport, layout.action_bar));
    CHECK(emberlights::connection_layout_contains(layout.action_bar, refresh));
    CHECK(emberlights::connection_layout_contains(layout.action_bar, copy));
    CHECK(emberlights::connection_layout_contains(layout.action_bar, apply));
    CHECK(emberlights::connection_layout_contains(layout.action_bar, message));
    CHECK(!emberlights::connection_layout_intersects(refresh, copy));
    CHECK(!emberlights::connection_layout_intersects(refresh, apply));
    CHECK(!emberlights::connection_layout_intersects(copy, apply));
    CHECK(!emberlights::connection_layout_intersects(refresh, message));
    CHECK(!emberlights::connection_layout_intersects(copy, message));
    CHECK(!emberlights::connection_layout_intersects(apply, message));
    CHECK(!emberlights::connection_layout_intersects(
        layout.scroll_viewport, layout.action_bar));
}

void check_visible_content(const ConnectionLayout& layout) {
    for (std::size_t left = 0;
         left < emberlights::kConnectionLayoutScrollableItemCount;
         ++left) {
        const auto& left_rect = layout.items[left];
        if (!emberlights::connection_layout_contains(
                layout.scroll_viewport, left_rect)) {
            continue;
        }
        CHECK(!emberlights::connection_layout_intersects(
            left_rect, layout.action_bar));
        for (std::size_t right = left + 1;
             right < emberlights::kConnectionLayoutScrollableItemCount;
             ++right) {
            const auto& right_rect = layout.items[right];
            if (emberlights::connection_layout_contains(
                    layout.scroll_viewport, right_rect)) {
                CHECK(!emberlights::connection_layout_intersects(
                    left_rect, right_rect));
            }
        }
    }
}

void test_required_display_matrix() {
    constexpr std::array<DisplayCase, 6> cases{{
        {1366, 768, 96U},
        {1366, 768, 120U},
        {1366, 768, 144U},
        {1366, 768, 192U},
        {1920, 1080, 96U},
        {1920, 1080, 144U},
    }};

    for (const auto& display : cases) {
        auto layout = layout_for_display(display);
        CHECK(layout == layout_for_display(display));
        check_action_bar(layout);
        check_visible_content(layout);
        CHECK(layout.scroll_viewport.has_area());
        CHECK(layout.content_height > 0);
        CHECK(layout.scroll_offset == 0);

        for (std::size_t item = 0;
             item < emberlights::kConnectionLayoutScrollableItemCount;
             ++item) {
            const auto id = static_cast<ConnectionLayoutItem>(item);
            const auto revealed =
                emberlights::connection_layout_scroll_to_reveal(layout, id);
            const auto revealed_layout = layout_for_display(display, revealed);
            const auto& rectangle = revealed_layout.items[item];
            check_action_bar(revealed_layout);
            check_visible_content(revealed_layout);
            CHECK(emberlights::connection_layout_contains(
                revealed_layout.scroll_viewport, rectangle));
            CHECK(!emberlights::connection_layout_intersects(
                rectangle, revealed_layout.action_bar));
        }

        const auto bottom = layout_for_display(display, INT32_MAX);
        CHECK(bottom.scroll_offset == bottom.maximum_scroll_offset);
        check_action_bar(bottom);
        check_visible_content(bottom);
    }
}

void test_visible_controls_never_overlap() {
    constexpr std::array<DisplayCase, 3> adversarial{{
        {1050, 700, 96U},
        {1050, 700, 192U},
        {1366, 640, 192U},
    }};
    for (const auto& display : adversarial) {
        const auto initial = layout_for_display(display);
        constexpr std::array<std::int32_t, 3> requested{{0, 4096, INT32_MAX}};
        for (const auto offset : requested) {
            const auto layout = layout_for_display(display, offset);
            check_action_bar(layout);
            check_visible_content(layout);
            CHECK(layout.scroll_offset <= initial.maximum_scroll_offset);
        }
    }
}

void test_scroll_and_input_fail_closed() {
    const auto invalid = emberlights::compute_connection_layout(
        ConnectionLayoutInput{-100, -100, 0U, -500});
    CHECK(invalid.viewport.width == 1);
    CHECK(invalid.viewport.height == 1);
    CHECK(invalid.dpi == emberlights::kConnectionLayoutMinimumDpi);
    CHECK(invalid.scroll_offset == 0);

    const auto layout = layout_for_display({1366, 768, 192U}, INT32_MAX);
    CHECK(layout.scroll_offset == layout.maximum_scroll_offset);
    CHECK(emberlights::connection_layout_scroll_to_reveal(
              layout, ConnectionLayoutItem::SaveAndApply) ==
        layout.scroll_offset);

    CHECK(emberlights::connection_layout_keyboard_action(
              true, ConnectionKeyboardIntent::DefaultActivate) ==
        ConnectionLayoutAction::SaveAndApply);
    CHECK(emberlights::connection_layout_keyboard_action(
              true, ConnectionKeyboardIntent::AltApply) ==
        ConnectionLayoutAction::SaveAndApply);
    CHECK(emberlights::connection_layout_keyboard_action(
              true, ConnectionKeyboardIntent::AltRefresh) ==
        ConnectionLayoutAction::Refresh);
    CHECK(emberlights::connection_layout_keyboard_action(
              false, ConnectionKeyboardIntent::DefaultActivate) ==
        ConnectionLayoutAction::None);
    CHECK(emberlights::connection_layout_keyboard_action(
              false, ConnectionKeyboardIntent::AltApply) ==
        ConnectionLayoutAction::None);
    CHECK(emberlights::connection_layout_keyboard_action(
              false, ConnectionKeyboardIntent::AltRefresh) ==
        ConnectionLayoutAction::None);
}

}  // namespace

int main() {
    test_required_display_matrix();
    test_visible_controls_never_overlap();
    test_scroll_and_input_fail_closed();

    if (failures != 0) {
        std::cerr << failures << " connection layout checks failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "connection layout tests passed\n";
    return EXIT_SUCCESS;
}
