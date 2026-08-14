#include "showcore/fixture.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace showcore {

ProfileResult validate_fixture_profile(const FixtureProfile& profile) noexcept {
    if (profile.name == nullptr || profile.name[0] == '\0') {
        return {ProfileError::MissingName, 0, 0};
    }
    if (profile.channels == nullptr || profile.channel_count == 0) {
        return {ProfileError::MissingChannels, 0, 0};
    }
    if (profile.footprint == 0 || profile.footprint > kUniverseSlots) {
        return {ProfileError::InvalidFootprint, 0, 0};
    }

    std::array<std::int16_t, kUniverseSlots> occupied{};
    occupied.fill(-1);

    for (std::size_t index = 0; index < profile.channel_count; ++index) {
        const auto& mapping = profile.channels[index];
        const bool is_constant = mapping.encoding == ChannelEncoding::Constant8;
        if (is_constant) {
            if (mapping.property != Property::Count) {
                return {ProfileError::ConstantHasProperty, index, index};
            }
        } else if (mapping.property == Property::Count &&
                   mapping.capability_count == 0U) {
            return {ProfileError::InvalidProperty, index, index};
        }

        if (mapping.capability_count != 0U && mapping.capabilities == nullptr) {
            return {ProfileError::CapabilityPointerMissing, index, index};
        }
        if (mapping.capability_count != 0U &&
            (is_constant || mapping.encoding == ChannelEncoding::Linear16)) {
            return {ProfileError::CapabilityNotAllowed, index, index};
        }

        if (mapping.coarse_offset >= profile.footprint) {
            return {ProfileError::OffsetOutsideFootprint, index, index};
        }

        if (mapping.encoding == ChannelEncoding::Linear16) {
            if (mapping.fine_offset < 0) {
                return {ProfileError::FineOffsetRequired, index, index};
            }
            if (static_cast<std::size_t>(mapping.fine_offset) >= profile.footprint) {
                return {ProfileError::OffsetOutsideFootprint, index, index};
            }
            if (static_cast<std::uint16_t>(mapping.fine_offset) == mapping.coarse_offset) {
                return {ProfileError::DuplicateOffset, index, index};
            }
        } else {
            if (mapping.fine_offset >= 0) {
                return {ProfileError::FineOffsetNotAllowed, index, index};
            }
            if (mapping.default_value > 255U || mapping.blackout_value > 255U ||
                mapping.highlight_value > 255U) {
                return {ProfileError::DefaultOutOfRange, index, index};
            }
        }

        std::array<std::int16_t, 256U> capability_occupancy{};
        capability_occupancy.fill(-1);
        for (std::size_t capability_index = 0U;
             capability_index < mapping.capability_count;
             ++capability_index) {
            const auto& capability = mapping.capabilities[capability_index];
            if (capability.property >= Property::Count) {
                return {
                    ProfileError::CapabilityInvalidProperty,
                    index,
                    capability_index};
            }
            if (capability.dmx_min > capability.dmx_max ||
                capability.behavior > ChannelCapabilityBehavior::Continuous ||
                capability.access > ChannelCapabilityAccess::Protected) {
                return {
                    ProfileError::CapabilityInvalidRange,
                    index,
                    capability_index};
            }
            if (capability.preferred_value < capability.dmx_min ||
                capability.preferred_value > capability.dmx_max) {
                return {
                    ProfileError::CapabilityPreferredOutOfRange,
                    index,
                    capability_index};
            }
            for (std::size_t value = capability.dmx_min;
                 value <= capability.dmx_max;
                 ++value) {
                if (capability_occupancy[value] >= 0) {
                    return {
                        ProfileError::CapabilityRangeOverlap,
                        index,
                        static_cast<std::size_t>(capability_occupancy[value])};
                }
                capability_occupancy[value] =
                    static_cast<std::int16_t>(capability_index);
            }
        }

        const auto claim_offset = [&](std::size_t offset) noexcept -> ProfileResult {
            const auto owner = occupied[offset];
            if (owner >= 0) {
                return {
                    ProfileError::DuplicateOffset,
                    index,
                    static_cast<std::size_t>(owner)};
            }
            occupied[offset] = static_cast<std::int16_t>(index);
            return {};
        };

        auto claim = claim_offset(mapping.coarse_offset);
        if (!claim) {
            return claim;
        }
        if (mapping.encoding == ChannelEncoding::Linear16) {
            claim = claim_offset(static_cast<std::size_t>(mapping.fine_offset));
            if (!claim) {
                return claim;
            }
        }
    }

    return {};
}

void Patch::clear() noexcept {
    count_ = 0;
    for (auto& universe : occupancy_) {
        universe.fill(-1);
    }
}

PatchResult Patch::add(FixtureInstance fixture) noexcept {
    if (count_ >= fixtures_.size()) {
        return {PatchError::Capacity, 0};
    }
    if (fixture.id >= kMaxFixtures) {
        return {PatchError::Capacity, 0};
    }
    for (std::size_t index = 0; index < count_; ++index) {
        if (fixtures_[index].id == fixture.id) {
            return {PatchError::DuplicateFixtureId, fixture.id};
        }
    }
    if (fixture.universe >= kV1UniverseCount) {
        return {PatchError::InvalidUniverse, 0};
    }
    if (fixture.address == 0 || fixture.address > kUniverseSlots) {
        return {PatchError::InvalidAddress, 0};
    }
    if (fixture.profile == nullptr) {
        return {PatchError::MissingProfile, 0};
    }
    const auto profile_result = validate_fixture_profile(*fixture.profile);
    if (!profile_result) {
        return {
            PatchError::InvalidProfile,
            0,
            profile_result.error,
            profile_result.mapping_index};
    }

    const auto first_slot = static_cast<std::size_t>(fixture.address - 1U);
    const auto end_slot = first_slot + fixture.profile->footprint;
    if (end_slot > kUniverseSlots) {
        return {PatchError::AddressOverflow, 0};
    }

    for (std::size_t slot = first_slot; slot < end_slot; ++slot) {
        const auto owner = occupancy_[fixture.universe][slot];
        if (owner >= 0) {
            return {PatchError::AddressOverlap, static_cast<std::uint16_t>(owner)};
        }
    }

    for (std::size_t slot = first_slot; slot < end_slot; ++slot) {
        occupancy_[fixture.universe][slot] = static_cast<std::int16_t>(fixture.id);
    }
    fixtures_[count_++] = fixture;
    return {};
}

void DmxFrameAttribution::clear() noexcept {
    for (auto& universe : universes) {
        universe.fill(ChannelRenderAttribution{});
    }
}

namespace {

[[nodiscard]] std::uint8_t encode_8bit(
    float value,
    std::uint8_t dmx_min,
    std::uint8_t dmx_max) noexcept {
    const auto clamped = std::clamp(value, 0.0F, 1.0F);
    const auto minimum = static_cast<float>(dmx_min);
    const auto span = static_cast<float>(static_cast<int>(dmx_max) - static_cast<int>(dmx_min));
    return static_cast<std::uint8_t>(std::lround(minimum + clamped * span));
}

void record_attribution(
    DmxFrameAttribution& attribution,
    const FixtureInstance& fixture,
    std::size_t mapping_index,
    const ChannelMapping& mapping,
    std::size_t slot,
    Property property,
    const ResolvedProperty& resolved,
    RenderValueOrigin origin,
    std::uint16_t capability_index = kInvalidRenderAttributionIndex,
    bool fine_channel = false) noexcept {
    if (fixture.universe >= kV1UniverseCount || slot >= kUniverseSlots) {
        return;
    }
    attribution.universes[fixture.universe][slot] = {
        fixture.id,
        static_cast<std::uint16_t>(mapping_index),
        capability_index,
        property,
        resolved.mode,
        resolved.owned ? resolved.source : LayerId::Count,
        mapping.encoding,
        origin,
        fine_channel};
}

}  // namespace

void FixtureRenderer::render(
    const Patch& patch,
    const LayerStack& layers,
    const SafetyPolicy& safety,
    DmxFrames& frames,
    DmxFrameAttribution& attribution) const noexcept {
    frames.clear();
    attribution.clear();

    for (std::size_t fixture_index = 0; fixture_index < patch.size(); ++fixture_index) {
        const auto& fixture = patch.at(fixture_index);
        const auto& profile = *fixture.profile;
        const auto base_slot = static_cast<std::size_t>(fixture.address - 1U);

        for (std::size_t channel_index = 0; channel_index < profile.channel_count; ++channel_index) {
            const auto& mapping = profile.channels[channel_index];
            const auto coarse_slot = base_slot + mapping.coarse_offset;
            if (coarse_slot >= kUniverseSlots) {
                continue;
            }

            if (mapping.encoding == ChannelEncoding::Constant8) {
                frames.universes[fixture.universe][coarse_slot] =
                    static_cast<std::uint8_t>(mapping.default_value & 0xFFU);
                record_attribution(
                    attribution,
                    fixture,
                    channel_index,
                    mapping,
                    coarse_slot,
                    Property::Count,
                    {},
                    RenderValueOrigin::Constant);
                continue;
            }

            if (mapping.capability_count != 0U && mapping.capabilities != nullptr) {
                std::array<bool, kPropertyCount> inspected{};
                bool selected = false;
                bool conflict = false;
                Property selected_property{Property::Count};
                ResolvedProperty selected_value{};

                for (std::size_t capability_index = 0U;
                     capability_index < mapping.capability_count;
                     ++capability_index) {
                    const auto& capability = mapping.capabilities[capability_index];
                    if (capability.access == ChannelCapabilityAccess::Protected ||
                        capability.property >= Property::Count) {
                        continue;
                    }
                    const auto property_index =
                        static_cast<std::size_t>(capability.property);
                    if (inspected[property_index]) {
                        continue;
                    }
                    inspected[property_index] = true;
                    const auto raw = layers.resolve(fixture.id, capability.property);
                    if (!raw.owned) {
                        continue;
                    }
                    const auto resolved =
                        layers.resolve_safe(fixture.id, capability.property, safety);
                    const auto source = static_cast<std::size_t>(resolved.source);
                    const auto selected_source =
                        static_cast<std::size_t>(selected_value.source);
                    if (!selected || source > selected_source) {
                        selected = true;
                        conflict = false;
                        selected_property = capability.property;
                        selected_value = resolved;
                    } else if (source == selected_source &&
                               capability.property != selected_property) {
                        // Two semantic owners are trying to drive one physical
                        // channel at the same layer. Fail closed instead of
                        // depending on capability order.
                        conflict = true;
                    }
                }

                if (conflict) {
                    frames.universes[fixture.universe][coarse_slot] =
                        static_cast<std::uint8_t>(mapping.blackout_value & 0xFFU);
                    record_attribution(
                        attribution,
                        fixture,
                        channel_index,
                        mapping,
                        coarse_slot,
                        selected_property,
                        selected_value,
                        RenderValueOrigin::Conflict);
                    continue;
                }
                if (selected && selected_value.mode == ValueMode::ForceZero) {
                    frames.universes[fixture.universe][coarse_slot] =
                        static_cast<std::uint8_t>(mapping.blackout_value & 0xFFU);
                    record_attribution(
                        attribution,
                        fixture,
                        channel_index,
                        mapping,
                        coarse_slot,
                        selected_property,
                        selected_value,
                        selected_value.source == LayerId::Safety
                            ? RenderValueOrigin::Safety
                            : RenderValueOrigin::Capability);
                    continue;
                }
                if (!selected) {
                    frames.universes[fixture.universe][coarse_slot] =
                        static_cast<std::uint8_t>(mapping.default_value & 0xFFU);
                    record_attribution(
                        attribution,
                        fixture,
                        channel_index,
                        mapping,
                        coarse_slot,
                        mapping.property,
                        {},
                        RenderValueOrigin::Default);
                    continue;
                }
                if (selected_property == Property::Strobe &&
                    selected_value.value <= 0.0F) {
                    // Preserve EmberLights' existing semantic contract: zero
                    // strobe means the channel's documented open/inactive
                    // value, while any positive value enters a strobe range.
                    frames.universes[fixture.universe][coarse_slot] =
                        static_cast<std::uint8_t>(mapping.default_value & 0xFFU);
                    record_attribution(
                        attribution,
                        fixture,
                        channel_index,
                        mapping,
                        coarse_slot,
                        selected_property,
                        selected_value,
                        RenderValueOrigin::Default);
                    continue;
                }

                std::size_t matching_count = 0U;
                for (std::size_t capability_index = 0U;
                     capability_index < mapping.capability_count;
                     ++capability_index) {
                    const auto& capability = mapping.capabilities[capability_index];
                    if (capability.property == selected_property &&
                        capability.access != ChannelCapabilityAccess::Protected) {
                        ++matching_count;
                    }
                }
                if (matching_count == 0U) {
                    frames.universes[fixture.universe][coarse_slot] =
                        static_cast<std::uint8_t>(mapping.default_value & 0xFFU);
                    record_attribution(
                        attribution,
                        fixture,
                        channel_index,
                        mapping,
                        coarse_slot,
                        selected_property,
                        selected_value,
                        RenderValueOrigin::Default);
                    continue;
                }

                const auto scaled = std::clamp(selected_value.value, 0.0F, 1.0F) *
                    static_cast<float>(matching_count);
                const auto selected_segment = std::min<std::size_t>(
                    static_cast<std::size_t>(scaled), matching_count - 1U);
                const auto local_value = std::clamp(
                    scaled - static_cast<float>(selected_segment), 0.0F, 1.0F);
                const ChannelCapabilityMapping* selected_capability = nullptr;
                std::uint16_t selected_capability_index =
                    kInvalidRenderAttributionIndex;
                std::size_t current_segment = 0U;
                for (std::size_t capability_index = 0U;
                     capability_index < mapping.capability_count;
                     ++capability_index) {
                    const auto& capability = mapping.capabilities[capability_index];
                    if (capability.property != selected_property ||
                        capability.access == ChannelCapabilityAccess::Protected) {
                        continue;
                    }
                    if (current_segment++ == selected_segment) {
                        selected_capability = &capability;
                        selected_capability_index =
                            static_cast<std::uint16_t>(capability_index);
                        break;
                    }
                }
                if (selected_capability == nullptr) {
                    frames.universes[fixture.universe][coarse_slot] =
                        static_cast<std::uint8_t>(mapping.default_value & 0xFFU);
                    record_attribution(
                        attribution,
                        fixture,
                        channel_index,
                        mapping,
                        coarse_slot,
                        selected_property,
                        selected_value,
                        RenderValueOrigin::Default);
                    continue;
                }
                if (selected_capability->behavior ==
                    ChannelCapabilityBehavior::Slot) {
                    frames.universes[fixture.universe][coarse_slot] =
                        selected_capability->preferred_value;
                } else {
                    frames.universes[fixture.universe][coarse_slot] = encode_8bit(
                        selected_capability->reversed ? 1.0F - local_value : local_value,
                        selected_capability->dmx_min,
                        selected_capability->dmx_max);
                }
                record_attribution(
                    attribution,
                    fixture,
                    channel_index,
                    mapping,
                    coarse_slot,
                    selected_property,
                    selected_value,
                    selected_value.source == LayerId::Safety
                        ? RenderValueOrigin::Safety
                        : RenderValueOrigin::Capability,
                    selected_capability_index);
                continue;
            }

            const auto resolved = layers.resolve_safe(fixture.id, mapping.property, safety);

            if (resolved.mode == ValueMode::ForceZero) {
                frames.universes[fixture.universe][coarse_slot] =
                    mapping.encoding == ChannelEncoding::Linear16
                    ? static_cast<std::uint8_t>((mapping.blackout_value >> 8U) & 0xFFU)
                    : static_cast<std::uint8_t>(mapping.blackout_value & 0xFFU);
                const auto origin = resolved.source == LayerId::Safety
                    ? RenderValueOrigin::Safety
                    : RenderValueOrigin::Property;
                record_attribution(
                    attribution,
                    fixture,
                    channel_index,
                    mapping,
                    coarse_slot,
                    mapping.property,
                    resolved,
                    origin);
                if (mapping.encoding == ChannelEncoding::Linear16 && mapping.fine_offset >= 0) {
                    const auto fine_slot = base_slot + static_cast<std::size_t>(mapping.fine_offset);
                    if (fine_slot < kUniverseSlots) {
                        frames.universes[fixture.universe][fine_slot] =
                            static_cast<std::uint8_t>(mapping.blackout_value & 0xFFU);
                        record_attribution(
                            attribution,
                            fixture,
                            channel_index,
                            mapping,
                            fine_slot,
                            mapping.property,
                            resolved,
                            origin,
                            kInvalidRenderAttributionIndex,
                            true);
                    }
                }
                continue;
            }

            if (mapping.encoding == ChannelEncoding::Linear16) {
                if (mapping.fine_offset < 0) {
                    continue;
                }
                const auto fine_slot = base_slot + static_cast<std::size_t>(mapping.fine_offset);
                if (fine_slot >= kUniverseSlots) {
                    continue;
                }
                const auto encoded = resolved.owned
                    ? static_cast<std::uint16_t>(std::lround(
                        std::clamp(resolved.value, 0.0F, 1.0F) * 65535.0F))
                    : mapping.default_value;
                frames.universes[fixture.universe][coarse_slot] =
                    static_cast<std::uint8_t>((encoded >> 8U) & 0xFFU);
                frames.universes[fixture.universe][fine_slot] =
                    static_cast<std::uint8_t>(encoded & 0xFFU);
                const auto origin = !resolved.owned
                    ? RenderValueOrigin::Default
                    : (resolved.source == LayerId::Safety
                           ? RenderValueOrigin::Safety
                           : RenderValueOrigin::Property);
                record_attribution(
                    attribution,
                    fixture,
                    channel_index,
                    mapping,
                    coarse_slot,
                    mapping.property,
                    resolved,
                    origin);
                record_attribution(
                    attribution,
                    fixture,
                    channel_index,
                    mapping,
                    fine_slot,
                    mapping.property,
                    resolved,
                    origin,
                    kInvalidRenderAttributionIndex,
                    true);
                continue;
            }

            if (mapping.encoding == ChannelEncoding::Ranged8 &&
                (!resolved.owned || resolved.value <= 0.0F)) {
                frames.universes[fixture.universe][coarse_slot] =
                    static_cast<std::uint8_t>(mapping.default_value & 0xFFU);
                record_attribution(
                    attribution,
                    fixture,
                    channel_index,
                    mapping,
                    coarse_slot,
                    mapping.property,
                    resolved,
                    RenderValueOrigin::Default);
            } else {
                frames.universes[fixture.universe][coarse_slot] = resolved.owned
                    ? encode_8bit(resolved.value, mapping.dmx_min, mapping.dmx_max)
                    : static_cast<std::uint8_t>(mapping.default_value & 0xFFU);
                record_attribution(
                    attribution,
                    fixture,
                    channel_index,
                    mapping,
                    coarse_slot,
                    mapping.property,
                    resolved,
                    !resolved.owned
                        ? RenderValueOrigin::Default
                        : (resolved.source == LayerId::Safety
                               ? RenderValueOrigin::Safety
                               : RenderValueOrigin::Property));
            }
        }
    }
}

}  // namespace showcore
