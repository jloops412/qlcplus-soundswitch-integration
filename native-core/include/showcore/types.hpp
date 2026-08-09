#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace showcore {

inline constexpr std::size_t kUniverseSlots = 512;
inline constexpr std::size_t kV1UniverseCount = 2;
inline constexpr std::size_t kMaxFixtures = 256;
inline constexpr std::size_t kMaxGroupFixtures = kMaxFixtures;

enum class Property : std::uint8_t {
    Intensity,
    Red,
    Green,
    Blue,
    White,
    Amber,
    UV,
    Cyan,
    Magenta,
    Yellow,
    Lime,
    Indigo,
    Pan,
    Tilt,
    PanRotate,
    TiltRotate,
    PanTiltSpeed,
    Strobe,
    Shutter,
    ColorWheel,
    Gobo,
    GoboRotation,
    Prism,
    PrismRotation,
    Focus,
    Zoom,
    Iris,
    Frost,
    Animation,
    AnimationRotation,
    Effect,
    EffectSpeed,
    Fan,
    Fog,
    Haze,
    Laser,
    Spark,
    Custom1,
    Custom2,
    Custom3,
    Custom4,
    Custom5,
    Custom6,
    Custom7,
    Custom8,
    Custom9,
    Custom10,
    Custom11,
    Custom12,
    Custom13,
    Custom14,
    Custom15,
    Custom16,
    Count
};

inline constexpr std::size_t kPropertyCount = static_cast<std::size_t>(Property::Count);

enum class LayerId : std::uint8_t {
    Idle,
    Autonomous,
    TrackScript,
    ManualAutoloop,
    EventMoment,
    ManualOverride,
    Emergency,
    Safety,
    Count
};

inline constexpr std::size_t kLayerCount = static_cast<std::size_t>(LayerId::Count);

enum class ValueMode : std::uint8_t {
    Release,
    Set,
    ForceZero
};

struct PropertyValue {
    ValueMode mode{ValueMode::Release};
    float value{0.0F};

    [[nodiscard]] static constexpr PropertyValue release() noexcept {
        return {ValueMode::Release, 0.0F};
    }

    [[nodiscard]] static constexpr PropertyValue set(float new_value) noexcept {
        return {ValueMode::Set, new_value};
    }

    [[nodiscard]] static constexpr PropertyValue force_zero() noexcept {
        return {ValueMode::ForceZero, 0.0F};
    }
};

struct ResolvedProperty {
    bool owned{false};
    float value{0.0F};
    ValueMode mode{ValueMode::Release};
    LayerId source{LayerId::Idle};
};

struct SemanticColor {
    float red{0.0F};
    float green{0.0F};
    float blue{0.0F};
    float white{0.0F};
    float amber{0.0F};
    float uv{0.0F};
};

using DmxUniverse = std::array<std::uint8_t, kUniverseSlots>;

struct DmxFrames {
    std::array<DmxUniverse, kV1UniverseCount> universes{};

    void clear() noexcept {
        for (auto& universe : universes) {
            universe.fill(0U);
        }
    }
};

struct FixtureGroup {
    std::array<std::uint16_t, kMaxGroupFixtures> fixture_ids{};
    std::size_t count{0};

    [[nodiscard]] bool add(std::uint16_t fixture_id) noexcept {
        if (count >= fixture_ids.size()) {
            return false;
        }
        fixture_ids[count++] = fixture_id;
        return true;
    }
};

}  // namespace showcore
