#include "showcore/layer_resolver.hpp"

#include <algorithm>

namespace showcore {

LayerBuffer::LayerBuffer() noexcept {
    clear();
}

void LayerBuffer::clear() noexcept {
    values_.fill(PropertyValue::release());
}

void LayerBuffer::set(
    std::uint16_t fixture_id,
    Property property,
    PropertyValue value) noexcept {
    if (fixture_id >= kMaxFixtures || property == Property::Count) {
        return;
    }
    if (value.mode == ValueMode::Set) {
        value.value = std::clamp(value.value, 0.0F, 1.0F);
    }
    values_[index(fixture_id, property)] = value;
}

PropertyValue LayerBuffer::get(std::uint16_t fixture_id, Property property) const noexcept {
    if (fixture_id >= kMaxFixtures || property == Property::Count) {
        return PropertyValue::release();
    }
    return values_[index(fixture_id, property)];
}

bool LayerBuffer::has_owned_values() const noexcept {
    return std::any_of(values_.begin(), values_.end(), [](const PropertyValue& value) {
        return value.mode != ValueMode::Release;
    });
}

void LayerStack::clear() noexcept {
    for (auto& layer : layers_) {
        layer.clear();
    }
}

void LayerStack::clear_layer(LayerId layer) noexcept {
    if (layer == LayerId::Count) {
        return;
    }
    layers_[static_cast<std::size_t>(layer)].clear();
}

void LayerStack::replace_layer(LayerId layer, const LayerBuffer& values) noexcept {
    if (layer == LayerId::Count) {
        return;
    }
    layers_[static_cast<std::size_t>(layer)] = values;
}

void LayerStack::set(
    LayerId layer,
    std::uint16_t fixture_id,
    Property property,
    PropertyValue value) noexcept {
    if (layer == LayerId::Count) {
        return;
    }
    layers_[static_cast<std::size_t>(layer)].set(fixture_id, property, value);
}

void LayerStack::set_group(
    LayerId layer,
    const FixtureGroup& group,
    Property property,
    PropertyValue value) noexcept {
    for (std::size_t index = 0; index < group.count; ++index) {
        set(layer, group.fixture_ids[index], property, value);
    }
}

ResolvedProperty LayerStack::resolve(std::uint16_t fixture_id, Property property) const noexcept {
    for (std::size_t index = kLayerCount; index > 0; --index) {
        const auto layer_index = index - 1;
        const auto value = layers_[layer_index].get(fixture_id, property);
        if (value.mode == ValueMode::Release) {
            continue;
        }
        return {
            true,
            value.mode == ValueMode::ForceZero ? 0.0F : std::clamp(value.value, 0.0F, 1.0F),
            value.mode,
            static_cast<LayerId>(layer_index)};
    }
    return {};
}

ResolvedProperty LayerStack::resolve_below(
    LayerId layer,
    std::uint16_t fixture_id,
    Property property) const noexcept {
    if (layer == LayerId::Count) {
        return resolve(fixture_id, property);
    }

    for (std::size_t index = static_cast<std::size_t>(layer); index > 0; --index) {
        const auto layer_index = index - 1U;
        const auto value = layers_[layer_index].get(fixture_id, property);
        if (value.mode == ValueMode::Release) {
            continue;
        }
        return {
            true,
            value.mode == ValueMode::ForceZero ? 0.0F : std::clamp(value.value, 0.0F, 1.0F),
            value.mode,
            static_cast<LayerId>(layer_index)};
    }
    return {};
}

ResolvedProperty LayerStack::resolve_safe(
    std::uint16_t fixture_id,
    Property property,
    const SafetyPolicy& policy) const noexcept {
    auto resolved = resolve(fixture_id, property);

    if ((property == Property::Fog && !policy.fog_armed) ||
        (property == Property::Haze && !policy.haze_armed) ||
        (property == Property::Laser && !policy.laser_armed) ||
        (property == Property::Spark && !policy.spark_armed)) {
        return {true, 0.0F, ValueMode::ForceZero, LayerId::Safety};
    }

    if (property == Property::Strobe) {
        if (!policy.strobe_allowed) {
            return {true, 0.0F, ValueMode::ForceZero, LayerId::Safety};
        }
        if (resolved.owned && resolved.value > policy.max_strobe) {
            resolved.value = std::clamp(policy.max_strobe, 0.0F, 1.0F);
            resolved.source = LayerId::Safety;
        }
    }

    if (property == Property::Intensity && resolved.owned && resolved.value > policy.max_intensity) {
        resolved.value = std::clamp(policy.max_intensity, 0.0F, 1.0F);
        resolved.source = LayerId::Safety;
    }

    return resolved;
}

}  // namespace showcore
