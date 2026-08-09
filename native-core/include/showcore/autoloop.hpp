#pragma once

#include "showcore/look.hpp"
#include "showcore/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace showcore {

inline constexpr std::size_t kMaxAutoloopSteps = 32;
inline constexpr std::size_t kAutoloopsPerBank = 32;
inline constexpr std::size_t kAutoloopBanksPerControlPage = 4;
inline constexpr std::size_t kMaxAutoloopBanks = 64;
inline constexpr std::size_t kAutoloopControlPageCount =
    kMaxAutoloopBanks / kAutoloopBanksPerControlPage;
inline constexpr std::size_t kMaxAutoloops = kMaxAutoloopBanks * kAutoloopsPerBank;

static_assert(kMaxAutoloopBanks <= 64U);
static_assert(kMaxAutoloopBanks % kAutoloopBanksPerControlPage == 0U);

enum class AutoloopTransition : std::uint8_t {
    Cut,
    Linear
};

enum class AutoloopRepeat : std::uint8_t {
    Once,
    Infinite,
    TrackDuration
};

struct AutoloopStep {
    float at_beat{0.0F};
    const StaticLook* look{nullptr};
    AutoloopTransition transition_to_next{AutoloopTransition::Cut};
};

struct AutoloopPattern {
    const char* name{nullptr};
    float length_beats{4.0F};
    std::array<AutoloopStep, kMaxAutoloopSteps> steps{};
    std::size_t step_count{0};

    [[nodiscard]] bool add_step(AutoloopStep step) noexcept;
};

enum class AutoloopError : std::uint8_t {
    None,
    MissingName,
    InvalidLength,
    MissingSteps,
    MissingLook,
    InvalidLook,
    InvalidStepTime,
    FirstStepNotZero,
    StepsOutOfOrder,
    StepOutsidePattern
};

struct AutoloopResult {
    AutoloopError error{AutoloopError::None};
    std::size_t step_index{0};
    LookError look_error{LookError::None};

    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return error == AutoloopError::None;
    }
};

[[nodiscard]] AutoloopResult validate_autoloop_pattern(
    const AutoloopPattern& pattern) noexcept;

class AutoloopEngine {
public:
    [[nodiscard]] bool apply(
        const AutoloopPattern& pattern,
        double beat_position,
        LayerId layer,
        LayerStack& layers) noexcept;

private:
    friend class AutoloopPlayer;
    [[nodiscard]] bool apply_validated(
        const AutoloopPattern& pattern,
        double beat_position,
        LayerId layer,
        LayerStack& layers) noexcept;
    LayerBuffer current_{};
    LayerBuffer next_{};
    LayerBuffer output_{};
};

struct AutoloopAddress {
    std::uint16_t bank{std::numeric_limits<std::uint16_t>::max()};
    std::uint8_t slot{0xFFU};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return bank < kMaxAutoloopBanks && slot < kAutoloopsPerBank;
    }

    [[nodiscard]] friend constexpr bool operator==(
        const AutoloopAddress& first,
        const AutoloopAddress& second) noexcept = default;
};

class AutoloopCatalog {
public:
    [[nodiscard]] bool set(
        AutoloopAddress address,
        const AutoloopPattern* pattern) noexcept;
    void clear(AutoloopAddress address) noexcept;
    [[nodiscard]] const AutoloopPattern* get(AutoloopAddress address) const noexcept;
    [[nodiscard]] bool swap_slots(AutoloopAddress first, AutoloopAddress second) noexcept;
    [[nodiscard]] bool duplicate(AutoloopAddress source, AutoloopAddress destination) noexcept;

    void select_all_banks() noexcept {
        active_bank_mask_ = std::numeric_limits<std::uint64_t>::max();
    }
    [[nodiscard]] bool select_exclusive_bank(std::uint16_t bank) noexcept;
    [[nodiscard]] bool set_bank_enabled(std::uint16_t bank, bool enabled) noexcept;
    [[nodiscard]] bool bank_enabled(std::uint16_t bank) const noexcept;
    [[nodiscard]] std::uint64_t active_bank_mask() const noexcept {
        return active_bank_mask_;
    }
    [[nodiscard]] AutoloopAddress next_available(AutoloopAddress after = {}) const noexcept;
    [[nodiscard]] AutoloopAddress previous_available(AutoloopAddress before = {}) const noexcept;

private:
    [[nodiscard]] static constexpr std::size_t index(AutoloopAddress address) noexcept {
        return static_cast<std::size_t>(address.bank) * kAutoloopsPerBank + address.slot;
    }

    std::array<const AutoloopPattern*, kMaxAutoloops> slots_{};
    std::uint64_t active_bank_mask_{std::numeric_limits<std::uint64_t>::max()};
};

class AutoloopBankWindow {
public:
    [[nodiscard]] bool select_page(std::uint16_t page) noexcept;
    void next_page() noexcept;
    void previous_page() noexcept;

    [[nodiscard]] std::uint16_t page() const noexcept { return page_; }
    [[nodiscard]] AutoloopAddress address(
        std::uint8_t visible_bank,
        std::uint8_t slot) const noexcept;

private:
    std::uint16_t page_{0};
};

struct AutoloopPlaybackStatus {
    bool active{false};
    AutoloopAddress address{};
    AutoloopRepeat repeat{AutoloopRepeat::Once};
    double elapsed_beats{0.0};
    float progress{0.0F};
    std::uint32_t completed_cycles{0};
    const char* name{nullptr};
};

class AutoloopPlayer {
public:
    explicit AutoloopPlayer(LayerId layer = LayerId::ManualAutoloop) noexcept
        : layer_(layer == LayerId::Count ? LayerId::ManualAutoloop : layer) {}

    [[nodiscard]] bool trigger(
        const AutoloopCatalog& catalog,
        AutoloopAddress address,
        AutoloopRepeat repeat,
        double beat_position,
        bool track_playing,
        LayerStack& layers) noexcept;
    void clear(LayerStack& layers) noexcept;
    void tick(double beat_position, bool track_playing, LayerStack& layers) noexcept;

    [[nodiscard]] const AutoloopPlaybackStatus& status() const noexcept { return status_; }

private:
    LayerId layer_{LayerId::ManualAutoloop};
    const AutoloopPattern* pattern_{nullptr};
    double start_beat_{0.0};
    AutoloopEngine engine_{};
    AutoloopPlaybackStatus status_{};
};

}  // namespace showcore
