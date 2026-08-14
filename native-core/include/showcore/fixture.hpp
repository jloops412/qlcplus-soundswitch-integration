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
    // Zero/released uses default_value. Positive semantic values enter the
    // configured DMX range. This safely represents shutter/strobe and other
    // channels whose inactive value sits outside their active range.
    Ranged8,
    Constant8
};

// Named capability ranges let one physical 8-bit DMX channel expose several
// semantic functions (for example shutter open plus a bounded strobe range, or
// indexed gobo slots) without turning raw DMX values into UI/controller APIs.
// Names and stable IDs remain in the Studio project; Runner needs only this
// compact immutable realization.
enum class ChannelCapabilityBehavior : std::uint8_t {
    Slot,
    Continuous
};

enum class ChannelCapabilityAccess : std::uint8_t {
    Selectable,
    SafetyGated,
    Protected
};

struct ChannelCapabilityMapping {
    Property property{Property::Intensity};
    std::uint8_t dmx_min{0};
    std::uint8_t dmx_max{255};
    std::uint8_t preferred_value{0};
    ChannelCapabilityBehavior behavior{ChannelCapabilityBehavior::Slot};
    ChannelCapabilityAccess access{ChannelCapabilityAccess::Selectable};
    bool reversed{false};
};

struct ChannelMapping {
    Property property{Property::Intensity};
    std::uint16_t coarse_offset{0};
    std::int16_t fine_offset{-1};
    ChannelEncoding encoding{ChannelEncoding::Linear8};
    std::uint8_t dmx_min{0};
    std::uint8_t dmx_max{255};
    std::uint16_t default_value{0};
    std::uint16_t blackout_value{0};
    std::uint16_t highlight_value{255};
    const ChannelCapabilityMapping* capabilities{nullptr};
    std::size_t capability_count{0};

    constexpr ChannelMapping() noexcept = default;
    constexpr ChannelMapping(
        Property new_property,
        std::uint16_t new_coarse_offset,
        std::int16_t new_fine_offset,
        ChannelEncoding new_encoding,
        std::uint8_t new_dmx_min,
        std::uint8_t new_dmx_max,
        std::uint16_t new_default_value = 0U,
        std::uint16_t new_blackout_value = 0U,
        std::uint16_t new_highlight_value = 255U,
        const ChannelCapabilityMapping* new_capabilities = nullptr,
        std::size_t new_capability_count = 0U) noexcept
        : property(new_property),
          coarse_offset(new_coarse_offset),
          fine_offset(new_fine_offset),
          encoding(new_encoding),
          dmx_min(new_dmx_min),
          dmx_max(new_dmx_max),
          default_value(new_default_value),
          blackout_value(new_blackout_value),
          highlight_value(new_highlight_value),
          capabilities(new_capabilities),
          capability_count(new_capability_count) {}
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

// Fixed renderer evidence accompanies a DMX frame without retaining project
// strings or allocating on the scheduler path. A slot with origin None is not
// owned by a fixture mapping. Default and Constant distinguish profile-authored
// values from resolved layer values; Conflict and Safety make fail-closed
// output explicit instead of presenting it as an ordinary property winner.
enum class RenderValueOrigin : std::uint8_t {
    None,
    Default,
    Constant,
    Property,
    Capability,
    Conflict,
    Safety
};

inline constexpr std::uint16_t kInvalidRenderAttributionIndex = 0xFFFFU;

struct ChannelRenderAttribution {
    std::uint16_t fixture_id{kInvalidRenderAttributionIndex};
    std::uint16_t mapping_index{kInvalidRenderAttributionIndex};
    std::uint16_t capability_index{kInvalidRenderAttributionIndex};
    Property property{Property::Count};
    ValueMode value_mode{ValueMode::Release};
    LayerId winning_layer{LayerId::Count};
    ChannelEncoding encoding{ChannelEncoding::Linear8};
    RenderValueOrigin origin{RenderValueOrigin::None};
    bool fine_channel{false};
};

struct DmxFrameAttribution {
    std::array<std::array<ChannelRenderAttribution, kUniverseSlots>,
               kV1UniverseCount>
        universes{};

    void clear() noexcept;
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
    DefaultOutOfRange,
    CapabilityPointerMissing,
    CapabilityNotAllowed,
    CapabilityInvalidProperty,
    CapabilityInvalidRange,
    CapabilityPreferredOutOfRange,
    CapabilityRangeOverlap
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
        DmxFrames& frames,
        DmxFrameAttribution& attribution) const noexcept;
};

}  // namespace showcore
