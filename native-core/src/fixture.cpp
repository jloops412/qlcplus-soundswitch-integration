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
        } else if (mapping.property == Property::Count) {
            return {ProfileError::InvalidProperty, index, index};
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
            if (mapping.default_value > 255U) {
                return {ProfileError::DefaultOutOfRange, index, index};
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

}  // namespace

void FixtureRenderer::render(
    const Patch& patch,
    const LayerStack& layers,
    const SafetyPolicy& safety,
    DmxFrames& frames) const noexcept {
    frames.clear();

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
                continue;
            }

            const auto resolved = layers.resolve_safe(fixture.id, mapping.property, safety);

            if (resolved.mode == ValueMode::ForceZero) {
                frames.universes[fixture.universe][coarse_slot] = 0U;
                if (mapping.encoding == ChannelEncoding::Linear16 && mapping.fine_offset >= 0) {
                    const auto fine_slot = base_slot + static_cast<std::size_t>(mapping.fine_offset);
                    if (fine_slot < kUniverseSlots) {
                        frames.universes[fixture.universe][fine_slot] = 0U;
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
                continue;
            }

            if (mapping.encoding == ChannelEncoding::Ranged8 &&
                (!resolved.owned || resolved.value <= 0.0F)) {
                frames.universes[fixture.universe][coarse_slot] =
                    static_cast<std::uint8_t>(mapping.default_value & 0xFFU);
            } else {
                frames.universes[fixture.universe][coarse_slot] = resolved.owned
                    ? encode_8bit(resolved.value, mapping.dmx_min, mapping.dmx_max)
                    : static_cast<std::uint8_t>(mapping.default_value & 0xFFU);
            }
        }
    }
}

}  // namespace showcore
