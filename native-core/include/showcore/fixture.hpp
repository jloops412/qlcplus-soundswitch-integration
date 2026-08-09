#pragma once

#include "showcore/layer_resolver.hpp"
#include "showcore/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace showcore {

enum class ChannelEncoding : std::uint8_t {
    Linear8,
    Linear16,
    Discrete8,
    Constant8
};

struct ChannelMapping {
    Property property{Property::Intensity};
    std::uint16_t coarse_offset{0};
    std::int16_t fine_offset{-1};
    ChannelEncoding encoding{ChannelEncoding::Linear8};
    std::uint8_t dmx_min{0};
    std::uint8_t dmx_max{255};
    std::uint16_t default_value{0};
};

struct FixtureProfile {
    const char* name{nullptr};
    const ChannelMapping* channels{nullptr};
    std::size_t channel_count{0};
    std::uint16_t footprint{0};
};

struct FixtureInstance {
    std::uint16_t id{0};
    std::uint8_t universe{0};
    std::uint16_t address{1};
    const FixtureProfile* profile{nullptr};
};

enum class ProfileError : std::uint8_t {
    None,
    MissingName,
    MissingChannels,
    InvalidFootprint,
    InvalidProperty,
    ConstantHasProperty,
    OffsetOutsideFootprint,
    FineOffsetRequired,
    FineOffsetNotAllowed,
    DuplicateOffset,
    DefaultOutOfRange
};

struct ProfileResult {
    ProfileError error{ProfileError::None};
    std::size_t mapping_index{0};
    std::size_t conflicting_mapping_index{0};

    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return error == ProfileError::None;
    }
};

[[nodiscard]] ProfileResult validate_fixture_profile(const FixtureProfile& profile) noexcept;

enum class PatchError : std::uint8_t {
    None,
    Capacity,
    DuplicateFixtureId,
    InvalidUniverse,
    InvalidAddress,
    MissingProfile,
    InvalidProfile,
    InvalidFootprint,
    AddressOverflow,
    AddressOverlap
};

struct PatchResult {
    PatchError error{PatchError::None};
    std::uint16_t conflicting_fixture_id{0};
    ProfileError profile_error{ProfileError::None};
    std::size_t profile_mapping_index{0};

    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return error == PatchError::None;
    }
};

class Patch {
public:
    Patch() noexcept { clear(); }
    void clear() noexcept;
    [[nodiscard]] PatchResult add(FixtureInstance fixture) noexcept;
    [[nodiscard]] std::size_t size() const noexcept { return count_; }
    [[nodiscard]] const FixtureInstance& at(std::size_t index) const noexcept { return fixtures_[index]; }

private:
    std::array<FixtureInstance, kMaxFixtures> fixtures_{};
    std::array<std::array<std::int16_t, kUniverseSlots>, kV1UniverseCount> occupancy_{};
    std::size_t count_{0};
};

class FixtureRenderer {
public:
    void render(
        const Patch& patch,
        const LayerStack& layers,
        const SafetyPolicy& safety,
        DmxFrames& frames) const noexcept;
};

}  // namespace showcore
