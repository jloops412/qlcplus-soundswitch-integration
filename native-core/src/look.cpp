#include "showcore/look.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace showcore {

namespace {

[[nodiscard]] constexpr std::size_t assignment_key(
    std::uint16_t fixture_id,
    Property property) noexcept {
    return static_cast<std::size_t>(fixture_id) * kPropertyCount +
        static_cast<std::size_t>(property);
}

[[nodiscard]] float effective_value(
    PropertyValue value,
    const ResolvedProperty& lower) noexcept {
    if (value.mode == ValueMode::ForceZero) {
        return 0.0F;
    }
    if (value.mode == ValueMode::Set) {
        return std::clamp(value.value, 0.0F, 1.0F);
    }
    return lower.owned ? std::clamp(lower.value, 0.0F, 1.0F) : 0.0F;
}

}  // namespace

LookResult validate_static_look(const StaticLook& look) noexcept {
    if (look.name == nullptr || look.name[0] == '\0') {
        return {LookError::MissingName, 0, 0};
    }
    if (look.assignments == nullptr || look.assignment_count == 0) {
        return {LookError::MissingAssignments, 0, 0};
    }
    if (look.assignment_count > kMaxLookAssignments) {
        return {LookError::TooManyAssignments, look.assignment_count, 0};
    }

    std::array<std::int32_t, kMaxLookAssignments> owners{};
    owners.fill(-1);

    for (std::size_t index = 0; index < look.assignment_count; ++index) {
        const auto& assignment = look.assignments[index];
        if (assignment.fixture_id >= kMaxFixtures) {
            return {LookError::InvalidFixture, index, index};
        }
        if (assignment.property == Property::Count) {
            return {LookError::InvalidProperty, index, index};
        }
        if (assignment.value.mode == ValueMode::Set &&
            (!std::isfinite(assignment.value.value) || assignment.value.value < 0.0F ||
             assignment.value.value > 1.0F)) {
            return {LookError::InvalidValue, index, index};
        }

        const auto key = assignment_key(assignment.fixture_id, assignment.property);
        if (owners[key] >= 0) {
            return {
                LookError::DuplicateAssignment,
                index,
                static_cast<std::size_t>(owners[key])};
        }
        owners[key] = static_cast<std::int32_t>(index);
    }
    return {};
}

LookResult compile_static_look(const StaticLook& look, LayerBuffer& output) noexcept {
    const auto result = validate_static_look(look);
    if (!result) {
        return result;
    }

    output.clear();
    for (std::size_t index = 0; index < look.assignment_count; ++index) {
        const auto& assignment = look.assignments[index];
        output.set(assignment.fixture_id, assignment.property, assignment.value);
    }
    return {};
}

void blend_layer_buffers(
    const LayerBuffer& from,
    const LayerBuffer& to,
    float amount,
    LayerId layer,
    LayerStack& layers,
    LayerBuffer& output) noexcept {
    output.clear();
    if (layer == LayerId::Count) {
        return;
    }

    const auto clamped = std::clamp(amount, 0.0F, 1.0F);
    layers.clear_layer(layer);

    if (clamped <= 0.0F) {
        output = from;
        layers.replace_layer(layer, output);
        return;
    }
    if (clamped >= 1.0F) {
        output = to;
        layers.replace_layer(layer, output);
        return;
    }

    for (std::uint16_t fixture_id = 0; fixture_id < kMaxFixtures; ++fixture_id) {
        for (std::size_t property_index = 0; property_index < kPropertyCount; ++property_index) {
            const auto property = static_cast<Property>(property_index);
            const auto first = from.get(fixture_id, property);
            const auto second = to.get(fixture_id, property);
            if (first.mode == ValueMode::Release && second.mode == ValueMode::Release) {
                continue;
            }

            const auto lower = layers.resolve_below(layer, fixture_id, property);
            const auto first_value = effective_value(first, lower);
            const auto second_value = effective_value(second, lower);
            output.set(
                fixture_id,
                property,
                PropertyValue::set(first_value + (second_value - first_value) * clamped));
        }
    }
    layers.replace_layer(layer, output);
}

float StaticLookPlayer::progress(std::uint64_t now_ms) const noexcept {
    if (!transitioning_ || transition_duration_ms_ == 0) {
        return 1.0F;
    }
    const auto elapsed = now_ms >= transition_start_ms_ ? now_ms - transition_start_ms_ : 0U;
    return std::clamp(
        static_cast<float>(elapsed) / static_cast<float>(transition_duration_ms_),
        0.0F,
        1.0F);
}

void StaticLookPlayer::begin_transition(
    std::uint64_t now_ms,
    std::uint32_t fade_ms,
    LayerStack& layers) noexcept {
    from_ = current_;
    transition_start_ms_ = now_ms;
    transition_duration_ms_ = fade_ms;
    transitioning_ = fade_ms > 0U;

    if (!transitioning_) {
        current_ = target_;
        layers.replace_layer(layer_, current_);
    }
}

LookResult StaticLookPlayer::trigger(
    const StaticLook& look,
    std::uint64_t now_ms,
    std::uint32_t fade_ms,
    LayerStack& layers) noexcept {
    const auto result = validate_static_look(look);
    if (!result) {
        return result;
    }

    tick(now_ms, layers);
    static_cast<void>(compile_static_look(look, target_));
    active_look_ = &look;
    begin_transition(now_ms, fade_ms, layers);
    return {};
}

void StaticLookPlayer::clear(
    std::uint64_t now_ms,
    std::uint32_t fade_ms,
    LayerStack& layers) noexcept {
    tick(now_ms, layers);
    target_.clear();
    active_look_ = nullptr;
    begin_transition(now_ms, fade_ms, layers);
}

void StaticLookPlayer::tick(std::uint64_t now_ms, LayerStack& layers) noexcept {
    if (!transitioning_) {
        layers.replace_layer(layer_, current_);
        return;
    }

    const auto amount = progress(now_ms);
    blend_layer_buffers(from_, target_, amount, layer_, layers, current_);
    if (amount >= 1.0F) {
        transitioning_ = false;
        current_ = target_;
        layers.replace_layer(layer_, current_);
    }
}

StaticLookStatus StaticLookPlayer::status(std::uint64_t now_ms) const noexcept {
    return {
        active_look_ != nullptr,
        transitioning_,
        transitioning_ ? progress(now_ms) : 1.0F,
        active_look_ == nullptr ? nullptr : active_look_->name};
}

}  // namespace showcore
