#pragma once

#include "emberlights/project.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace emberlights {

inline constexpr std::array<showcore::Property, 6U> kDirectEmitterProperties{{
    showcore::Property::Red,
    showcore::Property::Green,
    showcore::Property::Blue,
    showcore::Property::White,
    showcore::Property::Amber,
    showcore::Property::UV}};

struct FixtureCapabilityView {
    std::size_t fixture_index{0U};
    std::string_view fixture_id;
    std::string_view fixture_name;
    std::string_view profile_id;
    std::string_view profile_name;
    std::string_view manufacturer;
    std::string_view model;
    std::string_view mode;
    std::string_view source_revision;
    std::array<bool, showcore::kPropertyCount> properties{};
    bool complete{false};
    bool has_direct_emitters{false};
    bool has_master_intensity{false};
};

struct TargetPropertyCapability {
    showcore::Property property{showcore::Property::Intensity};
    std::size_t supported_fixture_count{0U};
    std::size_t target_fixture_count{0U};

    [[nodiscard]] bool supported() const noexcept {
        return supported_fixture_count != 0U;
    }
    [[nodiscard]] bool common() const noexcept {
        return target_fixture_count != 0U &&
            supported_fixture_count == target_fixture_count;
    }
    [[nodiscard]] bool partial() const noexcept {
        return supported() && !common();
    }
};

struct FixtureTargetCapabilities {
    bool target_found{false};
    bool group{false};
    std::string_view target_id;
    std::string_view target_name;
    std::vector<FixtureCapabilityView> fixtures;
    std::array<TargetPropertyCapability, showcore::kPropertyCount> properties{};
    bool has_direct_emitters{false};
    bool any_master_intensity{false};
    bool all_color_fixtures_have_master_intensity{false};
    std::vector<std::string> warnings;

    [[nodiscard]] const TargetPropertyCapability& capability(
        showcore::Property property) const noexcept;
};

// One exact, profile-backed fixture function resolved into the ordinary
// semantic property/value contract. The raw byte is diagnostic evidence only;
// callers continue to invoke Static Look/Live/controller behavior through the
// semantic property and normalized value.
struct FixtureControlChoiceValue {
    std::string fixture_id;
    std::string profile_id;
    std::string binding_id;
    std::uint16_t channel{0U};
    showcore::Property property{showcore::Property::Count};
    float normalized_value{0.0F};
    std::uint8_t raw_value{0U};
    std::uint8_t dmx_min{0U};
    std::uint8_t dmx_max{0U};
};

// A target-facing named function such as "Shutter open", "Gobo 4", or
// "Strobe slow to fast". Group choices retain one exact realization per
// supporting fixture. A Live group command can use the choice only when every
// supporting profile resolves to the same semantic value; Static Look
// authoring may always write the per-fixture values transactionally.
struct FixtureControlChoice {
    std::string id;
    std::string capability_id;
    std::string name;
    std::string owner;
    showcore::Property property{showcore::Property::Count};
    showcore::ChannelCapabilityBehavior behavior{
        showcore::ChannelCapabilityBehavior::Slot};
    showcore::ChannelCapabilityAccess access{
        showcore::ChannelCapabilityAccess::Selectable};
    FixtureChannelCapabilityRole role{FixtureChannelCapabilityRole::Function};
    std::size_t supported_fixture_count{0U};
    std::size_t target_fixture_count{0U};
    float shared_normalized_value{0.0F};
    bool shared_value{false};
    std::vector<FixtureControlChoiceValue> values;

    [[nodiscard]] bool partial() const noexcept {
        return supported_fixture_count != 0U &&
            supported_fixture_count != target_fixture_count;
    }
    [[nodiscard]] bool common() const noexcept {
        return target_fixture_count != 0U &&
            supported_fixture_count == target_fixture_count;
    }
    [[nodiscard]] bool safety_gated() const noexcept {
        return access == showcore::ChannelCapabilityAccess::SafetyGated;
    }
    [[nodiscard]] bool live_override_compatible() const noexcept {
        return common() && shared_value;
    }
};

struct FixtureControlChoiceCatalog {
    bool target_found{false};
    bool group{false};
    std::string target_id;
    std::string target_name;
    std::size_t target_fixture_count{0U};
    std::vector<FixtureControlChoice> choices;
    std::vector<std::string> warnings;
};

[[nodiscard]] const FixtureProfileDefinition* find_fixture_profile(
    const ProjectDocument& project,
    std::string_view profile_id) noexcept;

[[nodiscard]] bool fixture_profile_supports_property(
    const FixtureProfileDefinition& profile,
    showcore::Property property) noexcept;

[[nodiscard]] FixtureCapabilityView inspect_fixture_capabilities(
    const ProjectDocument& project,
    std::size_t fixture_index) noexcept;

// Resolves either a fixture ID or a group ID. Group support counts make mixed
// capability explicit so an authoring surface can show "2 of 4" instead of
// creating assignments that the renderer silently ignores.
[[nodiscard]] FixtureTargetCapabilities inspect_fixture_target(
    const ProjectDocument& project,
    std::string_view target_id);

// Builds deterministic, toolkit-neutral named choices for one patched fixture
// or group. Continuous capabilities use `position` within their documented
// DMX range; slots ignore it and use the documented preferred byte. Protected
// reset/service/custom ranges never enter the catalog.
[[nodiscard]] FixtureControlChoiceCatalog fixture_control_choices(
    const ProjectDocument& project,
    std::string_view target_id,
    float position = 0.5F);

}  // namespace emberlights
