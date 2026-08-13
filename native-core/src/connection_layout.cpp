#include "emberlights/connection_layout.hpp"

#include <algorithm>
#include <cstdint>

namespace emberlights {
namespace {

[[nodiscard]] constexpr std::size_t item_index(
    ConnectionLayoutItem item) noexcept {
    return static_cast<std::size_t>(item);
}

[[nodiscard]] std::int32_t scaled(
    std::int32_t logical,
    std::uint16_t dpi) noexcept {
    const auto product =
        static_cast<std::int64_t>(logical) * static_cast<std::int64_t>(dpi);
    return static_cast<std::int32_t>(
        (product + kConnectionLayoutDefaultDpi / 2U) /
        kConnectionLayoutDefaultDpi);
}

[[nodiscard]] std::int32_t clamped_scroll_offset(
    std::int32_t requested,
    std::int32_t maximum) noexcept {
    return std::clamp(requested, std::int32_t{0}, std::max(0, maximum));
}

}  // namespace

ConnectionLayout compute_connection_layout(ConnectionLayoutInput input) noexcept {
    ConnectionLayout result;
    result.dpi = std::clamp(
        input.dpi,
        kConnectionLayoutMinimumDpi,
        kConnectionLayoutMaximumDpi);

    const auto viewport_width = std::max(1, input.viewport_width);
    const auto viewport_height = std::max(1, input.viewport_height);
    result.viewport = {0, 0, viewport_width, viewport_height};

    const auto margin = scaled(16, result.dpi);
    const auto gap = scaled(12, result.dpi);
    const auto row_height = scaled(29, result.dpi);
    const auto row_step = scaled(36, result.dpi);
    const auto content_width = std::max(1, viewport_width - margin * 2);

    result.compact_action_bar = content_width < scaled(820, result.dpi);
    const auto preferred_action_height = scaled(
        result.compact_action_bar ? 150 : 104,
        result.dpi);
    const auto minimum_scroll_height = scaled(112, result.dpi);
    const auto action_height = std::clamp(
        preferred_action_height,
        std::int32_t{1},
        std::max(1, viewport_height - minimum_scroll_height));
    const auto action_y = viewport_height - action_height;
    result.scroll_viewport = {0, 0, viewport_width, action_y};
    result.action_bar = {0, action_y, viewport_width, action_height};

    result.two_columns = content_width >= scaled(860, result.dpi);
    const auto column_gap = scaled(28, result.dpi);
    const auto column_width = result.two_columns
        ? std::max(1, (content_width - column_gap) / 2)
        : content_width;

    const auto set_rect = [&](ConnectionLayoutItem item,
                              std::int32_t x,
                              std::int32_t y,
                              std::int32_t width,
                              std::int32_t height) {
        result.items[item_index(item)] = {
            x, y, std::max(1, width), std::max(1, height)};
    };
    const auto place_pair = [&](ConnectionLayoutItem label,
                                ConnectionLayoutItem field,
                                std::int32_t x,
                                std::int32_t y,
                                std::int32_t width) {
        const auto label_width = std::min(
            scaled(168, result.dpi),
            std::max(scaled(90, result.dpi), width / 2));
        const auto field_x = x + label_width + gap;
        set_rect(label, x, y, label_width, row_height);
        set_rect(
            field,
            field_x,
            y,
            std::max(1, x + width - field_x),
            row_height);
    };
    const auto place_full = [&](ConnectionLayoutItem item,
                                std::int32_t x,
                                std::int32_t y,
                                std::int32_t width) {
        set_rect(item, x, y, width, row_height);
    };

    auto content_y = scaled(14, result.dpi);
    set_rect(
        ConnectionLayoutItem::Title,
        margin,
        content_y,
        content_width,
        scaled(40, result.dpi));
    content_y += scaled(52, result.dpi);

    if (result.two_columns) {
        const auto left_x = margin;
        const auto right_x = margin + column_width + column_gap;
        auto left_y = content_y;
        auto right_y = content_y;

        place_pair(
            ConnectionLayoutItem::ProjectNameLabel,
            ConnectionLayoutItem::ProjectName,
            left_x,
            left_y,
            column_width);
        left_y += row_step;
        place_full(
            ConnectionLayoutItem::Os2lEnabled,
            left_x,
            left_y,
            column_width);
        left_y += row_step;
        place_pair(
            ConnectionLayoutItem::Os2lBindLabel,
            ConnectionLayoutItem::Os2lBind,
            left_x,
            left_y,
            column_width);
        left_y += row_step;
        place_pair(
            ConnectionLayoutItem::Os2lPortLabel,
            ConnectionLayoutItem::Os2lPort,
            left_x,
            left_y,
            column_width);
        left_y += row_step + scaled(8, result.dpi);

        place_full(
            ConnectionLayoutItem::ArtNetEnabled,
            left_x,
            left_y,
            column_width);
        left_y += row_step;
        place_pair(
            ConnectionLayoutItem::ArtNetDestinationLabel,
            ConnectionLayoutItem::ArtNetDestination,
            left_x,
            left_y,
            column_width);
        left_y += row_step;
        place_pair(
            ConnectionLayoutItem::ArtNetBaseLabel,
            ConnectionLayoutItem::ArtNetBase,
            left_x,
            left_y,
            column_width);
        left_y += row_step + scaled(8, result.dpi);

        place_full(
            ConnectionLayoutItem::SacnEnabled,
            left_x,
            left_y,
            column_width);
        left_y += row_step;
        place_pair(
            ConnectionLayoutItem::SacnDestinationLabel,
            ConnectionLayoutItem::SacnDestination,
            left_x,
            left_y,
            column_width);
        left_y += row_step;
        place_pair(
            ConnectionLayoutItem::SacnBaseLabel,
            ConnectionLayoutItem::SacnBase,
            left_x,
            left_y,
            column_width);
        left_y += row_step;

        place_pair(
            ConnectionLayoutItem::DmxUsbProUniverse1Label,
            ConnectionLayoutItem::DmxUsbProUniverse1,
            right_x,
            right_y,
            column_width);
        right_y += row_step;
        place_pair(
            ConnectionLayoutItem::DmxUsbProUniverse2Label,
            ConnectionLayoutItem::DmxUsbProUniverse2,
            right_x,
            right_y,
            column_width);
        right_y += row_step + scaled(8, result.dpi);
        place_pair(
            ConnectionLayoutItem::SoundSwitchMicroUniverseLabel,
            ConnectionLayoutItem::SoundSwitchMicroUniverse,
            right_x,
            right_y,
            column_width);
        right_y += row_step;
        place_pair(
            ConnectionLayoutItem::SoundSwitchMicroFramingLabel,
            ConnectionLayoutItem::SoundSwitchMicroFraming,
            right_x,
            right_y,
            column_width);
        right_y += row_step;
        place_pair(
            ConnectionLayoutItem::SoundSwitchControlOneLabel,
            ConnectionLayoutItem::SoundSwitchControlOne,
            right_x,
            right_y,
            column_width);
        right_y += row_step + scaled(8, result.dpi);
        place_pair(
            ConnectionLayoutItem::FrameRateLabel,
            ConnectionLayoutItem::FrameRate,
            right_x,
            right_y,
            column_width);
        right_y += row_step;
        place_pair(
            ConnectionLayoutItem::ManualBpmLabel,
            ConnectionLayoutItem::ManualBpm,
            right_x,
            right_y,
            column_width);
        right_y += row_step + scaled(8, result.dpi);
        place_pair(
            ConnectionLayoutItem::MidiInputLabel,
            ConnectionLayoutItem::MidiInput,
            right_x,
            right_y,
            column_width);
        right_y += row_step;
        place_pair(
            ConnectionLayoutItem::MidiOutputLabel,
            ConnectionLayoutItem::MidiOutput,
            right_x,
            right_y,
            column_width);
        right_y += row_step;

        content_y = std::max(left_y, right_y);
    } else {
        const auto x = margin;
        const auto place_pair_next = [&](ConnectionLayoutItem label,
                                         ConnectionLayoutItem field) {
            place_pair(label, field, x, content_y, content_width);
            content_y += row_step;
        };
        const auto place_full_next = [&](ConnectionLayoutItem item) {
            place_full(item, x, content_y, content_width);
            content_y += row_step;
        };
        const auto section_gap = [&]() {
            content_y += scaled(8, result.dpi);
        };

        place_pair_next(
            ConnectionLayoutItem::ProjectNameLabel,
            ConnectionLayoutItem::ProjectName);
        place_full_next(ConnectionLayoutItem::Os2lEnabled);
        place_pair_next(
            ConnectionLayoutItem::Os2lBindLabel,
            ConnectionLayoutItem::Os2lBind);
        place_pair_next(
            ConnectionLayoutItem::Os2lPortLabel,
            ConnectionLayoutItem::Os2lPort);
        section_gap();
        place_full_next(ConnectionLayoutItem::ArtNetEnabled);
        place_pair_next(
            ConnectionLayoutItem::ArtNetDestinationLabel,
            ConnectionLayoutItem::ArtNetDestination);
        place_pair_next(
            ConnectionLayoutItem::ArtNetBaseLabel,
            ConnectionLayoutItem::ArtNetBase);
        section_gap();
        place_full_next(ConnectionLayoutItem::SacnEnabled);
        place_pair_next(
            ConnectionLayoutItem::SacnDestinationLabel,
            ConnectionLayoutItem::SacnDestination);
        place_pair_next(
            ConnectionLayoutItem::SacnBaseLabel,
            ConnectionLayoutItem::SacnBase);
        section_gap();
        place_pair_next(
            ConnectionLayoutItem::DmxUsbProUniverse1Label,
            ConnectionLayoutItem::DmxUsbProUniverse1);
        place_pair_next(
            ConnectionLayoutItem::DmxUsbProUniverse2Label,
            ConnectionLayoutItem::DmxUsbProUniverse2);
        section_gap();
        place_pair_next(
            ConnectionLayoutItem::SoundSwitchMicroUniverseLabel,
            ConnectionLayoutItem::SoundSwitchMicroUniverse);
        place_pair_next(
            ConnectionLayoutItem::SoundSwitchMicroFramingLabel,
            ConnectionLayoutItem::SoundSwitchMicroFraming);
        place_pair_next(
            ConnectionLayoutItem::SoundSwitchControlOneLabel,
            ConnectionLayoutItem::SoundSwitchControlOne);
        section_gap();
        place_pair_next(
            ConnectionLayoutItem::FrameRateLabel,
            ConnectionLayoutItem::FrameRate);
        place_pair_next(
            ConnectionLayoutItem::ManualBpmLabel,
            ConnectionLayoutItem::ManualBpm);
        section_gap();
        place_pair_next(
            ConnectionLayoutItem::MidiInputLabel,
            ConnectionLayoutItem::MidiInput);
        place_pair_next(
            ConnectionLayoutItem::MidiOutputLabel,
            ConnectionLayoutItem::MidiOutput);
    }

    result.content_height = content_y + margin;
    result.maximum_scroll_offset = std::max(
        0,
        result.content_height - result.scroll_viewport.height);
    result.scroll_offset = clamped_scroll_offset(
        input.scroll_offset,
        result.maximum_scroll_offset);
    for (std::size_t index = 0;
         index < kConnectionLayoutScrollableItemCount;
         ++index) {
        result.items[index].y -= result.scroll_offset;
    }

    const auto action_top = result.action_bar.y + scaled(12, result.dpi);
    if (!result.compact_action_bar) {
        const auto refresh_width = scaled(214, result.dpi);
        const auto copy_width = scaled(210, result.dpi);
        const auto apply_width = scaled(240, result.dpi);
        set_rect(
            ConnectionLayoutItem::Refresh,
            margin,
            action_top,
            refresh_width,
            scaled(34, result.dpi));
        set_rect(
            ConnectionLayoutItem::CopyVirtualDjSetup,
            margin + refresh_width + gap,
            action_top,
            copy_width,
            scaled(34, result.dpi));
        set_rect(
            ConnectionLayoutItem::SaveAndApply,
            viewport_width - margin - apply_width,
            action_top,
            apply_width,
            scaled(34, result.dpi));
        set_rect(
            ConnectionLayoutItem::Message,
            margin,
            action_top + scaled(42, result.dpi),
            content_width,
            std::max(1, result.action_bar.bottom() - margin -
                (action_top + scaled(42, result.dpi))));
    } else {
        const auto first_row_width = std::max(1, content_width - gap);
        const auto refresh_width = std::max(1, first_row_width * 2 / 5);
        const auto apply_width = std::max(1, first_row_width - refresh_width);
        set_rect(
            ConnectionLayoutItem::Refresh,
            margin,
            action_top,
            refresh_width,
            scaled(34, result.dpi));
        set_rect(
            ConnectionLayoutItem::SaveAndApply,
            margin + refresh_width + gap,
            action_top,
            apply_width,
            scaled(34, result.dpi));
        set_rect(
            ConnectionLayoutItem::CopyVirtualDjSetup,
            margin,
            action_top + scaled(42, result.dpi),
            std::min(content_width, scaled(230, result.dpi)),
            scaled(34, result.dpi));
        set_rect(
            ConnectionLayoutItem::Message,
            margin,
            action_top + scaled(84, result.dpi),
            content_width,
            std::max(1, result.action_bar.bottom() - margin -
                (action_top + scaled(84, result.dpi))));
    }

    return result;
}

std::int32_t connection_layout_scroll_to_reveal(
    const ConnectionLayout& layout,
    ConnectionLayoutItem item) noexcept {
    const auto index = item_index(item);
    if (index >= kConnectionLayoutScrollableItemCount ||
        !layout.scroll_viewport.has_area()) {
        return layout.scroll_offset;
    }

    const auto& rectangle = layout.items[index];
    const auto padding = scaled(4, layout.dpi);
    auto requested = layout.scroll_offset;
    if (rectangle.y < layout.scroll_viewport.y + padding) {
        requested += rectangle.y - layout.scroll_viewport.y - padding;
    } else if (rectangle.bottom() >
               layout.scroll_viewport.bottom() - padding) {
        requested += rectangle.bottom() -
            (layout.scroll_viewport.bottom() - padding);
    }
    return clamped_scroll_offset(requested, layout.maximum_scroll_offset);
}

ConnectionLayoutAction connection_layout_keyboard_action(
    bool connections_page_active,
    ConnectionKeyboardIntent intent) noexcept {
    if (!connections_page_active) {
        return ConnectionLayoutAction::None;
    }
    switch (intent) {
    case ConnectionKeyboardIntent::DefaultActivate:
    case ConnectionKeyboardIntent::AltApply:
        return ConnectionLayoutAction::SaveAndApply;
    case ConnectionKeyboardIntent::AltRefresh:
        return ConnectionLayoutAction::Refresh;
    }
    return ConnectionLayoutAction::None;
}

}  // namespace emberlights
