#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace emberlights {

inline constexpr std::uint16_t kConnectionLayoutDefaultDpi = 96U;
inline constexpr std::uint16_t kConnectionLayoutMinimumDpi = 48U;
inline constexpr std::uint16_t kConnectionLayoutMaximumDpi = 768U;

struct ConnectionLayoutRect {
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

    friend constexpr bool operator==(
        const ConnectionLayoutRect&,
        const ConnectionLayoutRect&) noexcept = default;
};

[[nodiscard]] constexpr bool connection_layout_intersects(
    const ConnectionLayoutRect& left,
    const ConnectionLayoutRect& right) noexcept {
    return left.has_area() && right.has_area() && left.x < right.right() &&
        right.x < left.right() && left.y < right.bottom() &&
        right.y < left.bottom();
}

[[nodiscard]] constexpr bool connection_layout_contains(
    const ConnectionLayoutRect& outer,
    const ConnectionLayoutRect& inner) noexcept {
    return outer.has_area() && inner.has_area() && inner.x >= outer.x &&
        inner.y >= outer.y && inner.right() <= outer.right() &&
        inner.bottom() <= outer.bottom();
}

// This order intentionally matches the Win32 Connections page construction
// order. It is presentation identity only; connection behavior and outcome
// vocabulary remain owned by ConnectionCoordinator.
enum class ConnectionLayoutItem : std::uint8_t {
    Title,
    ProjectNameLabel,
    ProjectName,
    Os2lEnabled,
    Os2lBindLabel,
    Os2lBind,
    Os2lPortLabel,
    Os2lPort,
    ArtNetEnabled,
    ArtNetDestinationLabel,
    ArtNetDestination,
    ArtNetBaseLabel,
    ArtNetBase,
    SacnEnabled,
    SacnDestinationLabel,
    SacnDestination,
    SacnBaseLabel,
    SacnBase,
    DmxUsbProUniverse1Label,
    DmxUsbProUniverse1,
    DmxUsbProUniverse2Label,
    DmxUsbProUniverse2,
    SoundSwitchMicroUniverseLabel,
    SoundSwitchMicroUniverse,
    SoundSwitchMicroFramingLabel,
    SoundSwitchMicroFraming,
    SoundSwitchControlOneLabel,
    SoundSwitchControlOne,
    FrameRateLabel,
    FrameRate,
    ManualBpmLabel,
    ManualBpm,
    MidiInputLabel,
    MidiInput,
    MidiOutputLabel,
    MidiOutput,
    Refresh,
    CopyVirtualDjSetup,
    SaveAndApply,
    Message,
    Count
};

inline constexpr std::size_t kConnectionLayoutItemCount =
    static_cast<std::size_t>(ConnectionLayoutItem::Count);
inline constexpr std::size_t kConnectionLayoutScrollableItemCount =
    static_cast<std::size_t>(ConnectionLayoutItem::Refresh);

enum class ConnectionKeyboardIntent : std::uint8_t {
    DefaultActivate,
    AltApply,
    AltRefresh
};

enum class ConnectionLayoutAction : std::uint8_t {
    None,
    Refresh,
    SaveAndApply
};

struct ConnectionLayoutInput {
    std::int32_t viewport_width{1};
    std::int32_t viewport_height{1};
    std::uint16_t dpi{kConnectionLayoutDefaultDpi};
    std::int32_t scroll_offset{0};
};

struct ConnectionLayout {
    std::array<ConnectionLayoutRect, kConnectionLayoutItemCount> items{};
    ConnectionLayoutRect viewport{};
    ConnectionLayoutRect scroll_viewport{};
    ConnectionLayoutRect action_bar{};
    std::uint16_t dpi{kConnectionLayoutDefaultDpi};
    std::int32_t content_height{0};
    std::int32_t scroll_offset{0};
    std::int32_t maximum_scroll_offset{0};
    bool two_columns{false};
    bool compact_action_bar{false};

    friend constexpr bool operator==(
        const ConnectionLayout&,
        const ConnectionLayout&) noexcept = default;
};

[[nodiscard]] ConnectionLayout compute_connection_layout(
    ConnectionLayoutInput input) noexcept;

[[nodiscard]] std::int32_t connection_layout_scroll_to_reveal(
    const ConnectionLayout& layout,
    ConnectionLayoutItem item) noexcept;

[[nodiscard]] ConnectionLayoutAction connection_layout_keyboard_action(
    bool connections_page_active,
    ConnectionKeyboardIntent intent) noexcept;

}  // namespace emberlights
