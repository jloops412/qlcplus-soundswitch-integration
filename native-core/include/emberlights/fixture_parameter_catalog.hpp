#pragma once

#include "showcore/types.hpp"

#include <cstdint>
#include <span>
#include <string_view>

namespace emberlights {

// A renderer-neutral vocabulary shared by fixture profiles, Static Looks,
// Autoloops, Live overrides, MIDI/controller maps, and future skins. Stable
// IDs are persistence/automation contracts; display strings are presentation.
enum class FixtureParameterCategory : std::uint8_t {
    Intensity,
    Color,
    Position,
    Beam,
    Image,
    Effect,
    Atmosphere,
    Custom
};

enum class FixtureParameterControlKind : std::uint8_t {
    Level,
    Position,
    Speed,
    Selector,
    Trigger,
    Custom
};

enum class FixtureParameterProfilePreset : std::uint8_t {
    DirectLinear,
    ManualDmxChart
};

enum class FixtureParameterSafety : std::uint8_t {
    Normal,
    StrobeCapped,
    AtmosphericArmed,
    LaserArmed,
    PyrotechnicArmed,
    UnverifiedCustom
};

enum class FixtureParameterSurface : std::uint8_t {
    Profile = 1U << 0U,
    StaticLook = 1U << 1U,
    Autoloop = 1U << 2U,
    LiveOverride = 1U << 3U,
    Controller = 1U << 4U
};

struct FixtureParameterDescriptor {
    showcore::Property property{showcore::Property::Intensity};
    std::string_view stable_id;
    std::string_view display_name;
    std::string_view description;
    FixtureParameterCategory category{FixtureParameterCategory::Intensity};
    FixtureParameterControlKind control_kind{FixtureParameterControlKind::Level};
    FixtureParameterProfilePreset profile_preset{
        FixtureParameterProfilePreset::DirectLinear};
    FixtureParameterSafety safety{FixtureParameterSafety::Normal};
    std::uint8_t surfaces{0U};
    bool supports_fine_channel{false};

    [[nodiscard]] bool supports(FixtureParameterSurface surface) const noexcept;
    [[nodiscard]] bool needs_manual_dmx_chart() const noexcept {
        return profile_preset == FixtureParameterProfilePreset::ManualDmxChart;
    }
    [[nodiscard]] bool safety_restricted() const noexcept {
        return safety != FixtureParameterSafety::Normal;
    }
};

[[nodiscard]] std::span<const FixtureParameterDescriptor>
fixture_parameter_catalog() noexcept;

[[nodiscard]] const FixtureParameterDescriptor* fixture_parameter_descriptor(
    showcore::Property property) noexcept;

[[nodiscard]] const FixtureParameterDescriptor* fixture_parameter_descriptor(
    std::string_view stable_id) noexcept;

[[nodiscard]] std::string_view fixture_parameter_category_name(
    FixtureParameterCategory category) noexcept;

[[nodiscard]] std::string_view fixture_parameter_control_kind_name(
    FixtureParameterControlKind kind) noexcept;

[[nodiscard]] std::string_view fixture_parameter_safety_name(
    FixtureParameterSafety safety) noexcept;

}  // namespace emberlights
