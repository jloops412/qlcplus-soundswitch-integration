#pragma once

#include "emberlights/project.hpp"

#include <array>
#include <cstddef>
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

}  // namespace emberlights
