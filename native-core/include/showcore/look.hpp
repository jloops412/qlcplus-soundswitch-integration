#pragma once

#include "showcore/layer_resolver.hpp"
#include "showcore/types.hpp"

#include <cstddef>
#include <cstdint>

namespace showcore {

inline constexpr std::size_t kMaxLookAssignments = kMaxFixtures * kPropertyCount;

struct LookAssignment {
    std::uint16_t fixture_id{0};
    Property property{Property::Intensity};
    PropertyValue value{};
};

struct StaticLook {
    const char* name{nullptr};
    const LookAssignment* assignments{nullptr};
    std::size_t assignment_count{0};
};

enum class LookError : std::uint8_t {
    None,
    MissingName,
    MissingAssignments,
    TooManyAssignments,
    InvalidFixture,
    InvalidProperty,
    InvalidValue,
    DuplicateAssignment
};

struct LookResult {
    LookError error{LookError::None};
    std::size_t assignment_index{0};
    std::size_t conflicting_assignment_index{0};

    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return error == LookError::None;
    }
};

[[nodiscard]] LookResult validate_static_look(const StaticLook& look) noexcept;
[[nodiscard]] LookResult compile_static_look(
    const StaticLook& look,
    LayerBuffer& output) noexcept;

void blend_layer_buffers(
    const LayerBuffer& from,
    const LayerBuffer& to,
    float amount,
    LayerId layer,
    LayerStack& layers,
    LayerBuffer& output) noexcept;

struct StaticLookStatus {
    bool active{false};
    bool transitioning{false};
    float transition_progress{0.0F};
    const char* name{nullptr};
};

class StaticLookPlayer {
public:
    explicit StaticLookPlayer(LayerId layer = LayerId::EventMoment) noexcept
        : layer_(layer == LayerId::Count ? LayerId::EventMoment : layer) {}

    [[nodiscard]] LookResult trigger(
        const StaticLook& look,
        std::uint64_t now_ms,
        std::uint32_t fade_ms,
        LayerStack& layers) noexcept;
    void clear(
        std::uint64_t now_ms,
        std::uint32_t fade_ms,
        LayerStack& layers) noexcept;
    void tick(std::uint64_t now_ms, LayerStack& layers) noexcept;

    [[nodiscard]] StaticLookStatus status(std::uint64_t now_ms) const noexcept;

private:
    [[nodiscard]] float progress(std::uint64_t now_ms) const noexcept;
    void begin_transition(
        std::uint64_t now_ms,
        std::uint32_t fade_ms,
        LayerStack& layers) noexcept;

    LayerId layer_{LayerId::EventMoment};
    LayerBuffer from_{};
    LayerBuffer target_{};
    LayerBuffer current_{};
    std::uint64_t transition_start_ms_{0};
    std::uint32_t transition_duration_ms_{0};
    bool transitioning_{false};
    const StaticLook* active_look_{nullptr};
};

}  // namespace showcore
