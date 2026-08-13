#include "emberlights/fixture_parameter_catalog.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

namespace emberlights {
namespace {

using Category = FixtureParameterCategory;
using Control = FixtureParameterControlKind;
using Preset = FixtureParameterProfilePreset;
using Property = showcore::Property;
using Safety = FixtureParameterSafety;
using Surface = FixtureParameterSurface;

constexpr std::uint8_t kAllSurfaces =
    static_cast<std::uint8_t>(Surface::Profile) |
    static_cast<std::uint8_t>(Surface::StaticLook) |
    static_cast<std::uint8_t>(Surface::Autoloop) |
    static_cast<std::uint8_t>(Surface::LiveOverride) |
    static_cast<std::uint8_t>(Surface::Controller);

constexpr FixtureParameterDescriptor descriptor(
    Property property,
    std::string_view stable_id,
    std::string_view display_name,
    std::string_view description,
    Category category,
    Control control,
    Preset preset = Preset::DirectLinear,
    Safety safety = Safety::Normal,
    bool fine = false) noexcept {
    return {
        property,
        stable_id,
        display_name,
        description,
        category,
        control,
        preset,
        safety,
        kAllSurfaces,
        fine};
}

// Keep this table in exact showcore::Property ordinal order. Tests enforce the
// one-to-one contract so a newly added semantic cannot silently disappear from
// profile authoring or controller mapping.
constexpr std::array<FixtureParameterDescriptor, showcore::kPropertyCount>
    kCatalog{{
        descriptor(Property::Intensity, "intensity", "Intensity", "Master brightness or dimmer", Category::Intensity, Control::Level, Preset::DirectLinear, Safety::Normal, true),
        descriptor(Property::Red, "red", "Red", "Red emitter level", Category::Color, Control::Level, Preset::DirectLinear, Safety::Normal, true),
        descriptor(Property::Green, "green", "Green", "Green emitter level", Category::Color, Control::Level, Preset::DirectLinear, Safety::Normal, true),
        descriptor(Property::Blue, "blue", "Blue", "Blue emitter level", Category::Color, Control::Level, Preset::DirectLinear, Safety::Normal, true),
        descriptor(Property::White, "white", "White", "White emitter level", Category::Color, Control::Level, Preset::DirectLinear, Safety::Normal, true),
        descriptor(Property::Amber, "amber", "Amber", "Amber emitter level", Category::Color, Control::Level, Preset::DirectLinear, Safety::Normal, true),
        descriptor(Property::UV, "uv", "UV", "Ultraviolet or purple emitter level", Category::Color, Control::Level, Preset::DirectLinear, Safety::Normal, true),
        descriptor(Property::Cyan, "cyan", "Cyan", "Cyan color-mixing level", Category::Color, Control::Level, Preset::DirectLinear, Safety::Normal, true),
        descriptor(Property::Magenta, "magenta", "Magenta", "Magenta color-mixing level", Category::Color, Control::Level, Preset::DirectLinear, Safety::Normal, true),
        descriptor(Property::Yellow, "yellow", "Yellow", "Yellow color-mixing level", Category::Color, Control::Level, Preset::DirectLinear, Safety::Normal, true),
        descriptor(Property::Lime, "lime", "Lime", "Lime emitter level", Category::Color, Control::Level, Preset::DirectLinear, Safety::Normal, true),
        descriptor(Property::Indigo, "indigo", "Indigo", "Indigo emitter level", Category::Color, Control::Level, Preset::DirectLinear, Safety::Normal, true),
        descriptor(Property::Pan, "pan", "Pan", "Horizontal fixture position", Category::Position, Control::Position, Preset::DirectLinear, Safety::Normal, true),
        descriptor(Property::Tilt, "tilt", "Tilt", "Vertical fixture position", Category::Position, Control::Position, Preset::DirectLinear, Safety::Normal, true),
        descriptor(Property::PanRotate, "panRotate", "Pan rotation", "Continuous pan direction and speed", Category::Position, Control::Speed, Preset::ManualDmxChart),
        descriptor(Property::TiltRotate, "tiltRotate", "Tilt rotation", "Continuous tilt direction and speed", Category::Position, Control::Speed, Preset::ManualDmxChart),
        descriptor(Property::PanTiltSpeed, "panTiltSpeed", "Pan / tilt speed", "Movement timing or speed", Category::Position, Control::Speed, Preset::ManualDmxChart),
        descriptor(Property::Strobe, "strobe", "Strobe", "Strobe rate or pattern", Category::Beam, Control::Speed, Preset::ManualDmxChart, Safety::StrobeCapped),
        descriptor(Property::Shutter, "shutter", "Shutter", "Shutter open, closed, or mode range", Category::Beam, Control::Selector, Preset::ManualDmxChart),
        descriptor(Property::ColorWheel, "colorWheel", "Color wheel", "Indexed color wheel slot or rotation", Category::Color, Control::Selector, Preset::ManualDmxChart),
        descriptor(Property::Gobo, "gobo", "Gobo", "Indexed gobo slot", Category::Image, Control::Selector, Preset::ManualDmxChart),
        descriptor(Property::GoboRotation, "goboRotation", "Gobo rotation", "Gobo direction and speed", Category::Image, Control::Speed, Preset::ManualDmxChart),
        descriptor(Property::Prism, "prism", "Prism", "Prism selection or insertion", Category::Beam, Control::Selector, Preset::ManualDmxChart),
        descriptor(Property::PrismRotation, "prismRotation", "Prism rotation", "Prism direction and speed", Category::Beam, Control::Speed, Preset::ManualDmxChart),
        descriptor(Property::Focus, "focus", "Focus", "Beam focus position", Category::Beam, Control::Position, Preset::DirectLinear, Safety::Normal, true),
        descriptor(Property::Zoom, "zoom", "Zoom", "Beam zoom position", Category::Beam, Control::Position, Preset::DirectLinear, Safety::Normal, true),
        descriptor(Property::Iris, "iris", "Iris", "Beam iris position", Category::Beam, Control::Position, Preset::DirectLinear, Safety::Normal, true),
        descriptor(Property::Frost, "frost", "Frost", "Frost or diffusion level", Category::Beam, Control::Level, Preset::DirectLinear),
        descriptor(Property::Animation, "animation", "Animation wheel", "Animation wheel selection", Category::Image, Control::Selector, Preset::ManualDmxChart),
        descriptor(Property::AnimationRotation, "animationRotation", "Animation rotation", "Animation direction and speed", Category::Image, Control::Speed, Preset::ManualDmxChart),
        descriptor(Property::Effect, "effect", "Effect / macro", "Fixture effect, program, or macro", Category::Effect, Control::Selector, Preset::ManualDmxChart),
        descriptor(Property::EffectSpeed, "effectSpeed", "Effect speed", "Fixture effect or program speed", Category::Effect, Control::Speed, Preset::ManualDmxChart),
        descriptor(Property::Fan, "fan", "Fan", "Fixture fan level or mode", Category::Effect, Control::Level, Preset::ManualDmxChart),
        descriptor(Property::Fog, "fog", "Fog", "Fog output level", Category::Atmosphere, Control::Trigger, Preset::ManualDmxChart, Safety::AtmosphericArmed),
        descriptor(Property::Haze, "haze", "Haze", "Haze output level", Category::Atmosphere, Control::Trigger, Preset::ManualDmxChart, Safety::AtmosphericArmed),
        descriptor(Property::Laser, "laser", "Laser", "Laser output or program", Category::Effect, Control::Trigger, Preset::ManualDmxChart, Safety::LaserArmed),
        descriptor(Property::Spark, "spark", "Spark / pyro", "Spark or pyrotechnic output", Category::Effect, Control::Trigger, Preset::ManualDmxChart, Safety::PyrotechnicArmed),
        descriptor(Property::Custom1, "custom1", "Custom 1", "Unclassified fixture parameter", Category::Custom, Control::Custom, Preset::ManualDmxChart, Safety::UnverifiedCustom),
        descriptor(Property::Custom2, "custom2", "Custom 2", "Unclassified fixture parameter", Category::Custom, Control::Custom, Preset::ManualDmxChart, Safety::UnverifiedCustom),
        descriptor(Property::Custom3, "custom3", "Custom 3", "Unclassified fixture parameter", Category::Custom, Control::Custom, Preset::ManualDmxChart, Safety::UnverifiedCustom),
        descriptor(Property::Custom4, "custom4", "Custom 4", "Unclassified fixture parameter", Category::Custom, Control::Custom, Preset::ManualDmxChart, Safety::UnverifiedCustom),
        descriptor(Property::Custom5, "custom5", "Custom 5", "Unclassified fixture parameter", Category::Custom, Control::Custom, Preset::ManualDmxChart, Safety::UnverifiedCustom),
        descriptor(Property::Custom6, "custom6", "Custom 6", "Unclassified fixture parameter", Category::Custom, Control::Custom, Preset::ManualDmxChart, Safety::UnverifiedCustom),
        descriptor(Property::Custom7, "custom7", "Custom 7", "Unclassified fixture parameter", Category::Custom, Control::Custom, Preset::ManualDmxChart, Safety::UnverifiedCustom),
        descriptor(Property::Custom8, "custom8", "Custom 8", "Unclassified fixture parameter", Category::Custom, Control::Custom, Preset::ManualDmxChart, Safety::UnverifiedCustom),
        descriptor(Property::Custom9, "custom9", "Custom 9", "Unclassified fixture parameter", Category::Custom, Control::Custom, Preset::ManualDmxChart, Safety::UnverifiedCustom),
        descriptor(Property::Custom10, "custom10", "Custom 10", "Unclassified fixture parameter", Category::Custom, Control::Custom, Preset::ManualDmxChart, Safety::UnverifiedCustom),
        descriptor(Property::Custom11, "custom11", "Custom 11", "Unclassified fixture parameter", Category::Custom, Control::Custom, Preset::ManualDmxChart, Safety::UnverifiedCustom),
        descriptor(Property::Custom12, "custom12", "Custom 12", "Unclassified fixture parameter", Category::Custom, Control::Custom, Preset::ManualDmxChart, Safety::UnverifiedCustom),
        descriptor(Property::Custom13, "custom13", "Custom 13", "Unclassified fixture parameter", Category::Custom, Control::Custom, Preset::ManualDmxChart, Safety::UnverifiedCustom),
        descriptor(Property::Custom14, "custom14", "Custom 14", "Unclassified fixture parameter", Category::Custom, Control::Custom, Preset::ManualDmxChart, Safety::UnverifiedCustom),
        descriptor(Property::Custom15, "custom15", "Custom 15", "Unclassified fixture parameter", Category::Custom, Control::Custom, Preset::ManualDmxChart, Safety::UnverifiedCustom),
        descriptor(Property::Custom16, "custom16", "Custom 16", "Unclassified fixture parameter", Category::Custom, Control::Custom, Preset::ManualDmxChart, Safety::UnverifiedCustom),
    }};

}  // namespace

bool FixtureParameterDescriptor::supports(
    FixtureParameterSurface surface) const noexcept {
    return (surfaces & static_cast<std::uint8_t>(surface)) != 0U;
}

std::span<const FixtureParameterDescriptor> fixture_parameter_catalog() noexcept {
    return kCatalog;
}

const FixtureParameterDescriptor* fixture_parameter_descriptor(
    showcore::Property property) noexcept {
    const auto index = static_cast<std::size_t>(property);
    if (index >= kCatalog.size() || kCatalog[index].property != property) {
        return nullptr;
    }
    return &kCatalog[index];
}

const FixtureParameterDescriptor* fixture_parameter_descriptor(
    std::string_view stable_id) noexcept {
    const auto found = std::find_if(
        kCatalog.begin(), kCatalog.end(),
        [stable_id](const auto& candidate) {
            return candidate.stable_id == stable_id;
        });
    return found == kCatalog.end() ? nullptr : &*found;
}

std::string_view fixture_parameter_category_name(
    FixtureParameterCategory category) noexcept {
    switch (category) {
    case FixtureParameterCategory::Intensity: return "Intensity";
    case FixtureParameterCategory::Color: return "Color";
    case FixtureParameterCategory::Position: return "Position";
    case FixtureParameterCategory::Beam: return "Beam";
    case FixtureParameterCategory::Image: return "Image";
    case FixtureParameterCategory::Effect: return "Effect";
    case FixtureParameterCategory::Atmosphere: return "Atmosphere";
    case FixtureParameterCategory::Custom: return "Custom";
    }
    return "Unknown";
}

std::string_view fixture_parameter_control_kind_name(
    FixtureParameterControlKind kind) noexcept {
    switch (kind) {
    case FixtureParameterControlKind::Level: return "Level";
    case FixtureParameterControlKind::Position: return "Position";
    case FixtureParameterControlKind::Speed: return "Speed";
    case FixtureParameterControlKind::Selector: return "Selector / range";
    case FixtureParameterControlKind::Trigger: return "Trigger / level";
    case FixtureParameterControlKind::Custom: return "Custom";
    }
    return "Unknown";
}

std::string_view fixture_parameter_safety_name(
    FixtureParameterSafety safety) noexcept {
    switch (safety) {
    case FixtureParameterSafety::Normal: return "Normal";
    case FixtureParameterSafety::StrobeCapped: return "Strobe safety cap";
    case FixtureParameterSafety::AtmosphericArmed: return "Atmospheric arm required";
    case FixtureParameterSafety::LaserArmed: return "Laser arm required";
    case FixtureParameterSafety::PyrotechnicArmed: return "Pyrotechnic arm required";
    case FixtureParameterSafety::UnverifiedCustom: return "Unverified custom behavior";
    }
    return "Unknown";
}

}  // namespace emberlights
