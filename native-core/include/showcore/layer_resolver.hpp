#pragma once

#include "showcore/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace showcore {

class LayerBuffer {
public:
    LayerBuffer() noexcept;

    void clear() noexcept;
    void set(std::uint16_t fixture_id, Property property, PropertyValue value) noexcept;
    [[nodiscard]] PropertyValue get(std::uint16_t fixture_id, Property property) const noexcept;
    [[nodiscard]] bool has_owned_values() const noexcept;

private:
    [[nodiscard]] static constexpr std::size_t index(std::uint16_t fixture_id, Property property) noexcept {
        return static_cast<std::size_t>(fixture_id) * kPropertyCount + static_cast<std::size_t>(property);
    }

    std::array<PropertyValue, kMaxFixtures * kPropertyCount> values_{};
};

struct SafetyPolicy {
    bool fog_armed{false};
    bool haze_armed{false};
    bool laser_armed{false};
    bool spark_armed{false};
    bool strobe_allowed{true};
    float max_strobe{1.0F};
    float max_intensity{1.0F};
};

class LayerStack {
public:
    void clear() noexcept;
    void clear_layer(LayerId layer) noexcept;
    void replace_layer(LayerId layer, const LayerBuffer& values) noexcept;
    void set(LayerId layer, std::uint16_t fixture_id, Property property, PropertyValue value) noexcept;
    void set_group(LayerId layer, const FixtureGroup& group, Property property, PropertyValue value) noexcept;

    [[nodiscard]] ResolvedProperty resolve(std::uint16_t fixture_id, Property property) const noexcept;
    [[nodiscard]] ResolvedProperty resolve_below(
        LayerId layer,
        std::uint16_t fixture_id,
        Property property) const noexcept;
    [[nodiscard]] ResolvedProperty resolve_safe(
        std::uint16_t fixture_id,
        Property property,
        const SafetyPolicy& policy) const noexcept;
    // Read-only access is used by bounded pre-composition adapters. Callers
    // cannot mutate a layer behind the stack's ownership rules.
    [[nodiscard]] const LayerBuffer* layer(LayerId layer) const noexcept {
        return layer == LayerId::Count
            ? nullptr
            : &layers_[static_cast<std::size_t>(layer)];
    }

private:
    std::array<LayerBuffer, kLayerCount> layers_{};
};

}  // namespace showcore
