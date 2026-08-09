#include "showcore/autoloop.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace showcore {

namespace {

void load_validated_look(const StaticLook& look, LayerBuffer& output) noexcept {
    output.clear();
    for (std::size_t index = 0; index < look.assignment_count; ++index) {
        const auto& assignment = look.assignments[index];
        output.set(assignment.fixture_id, assignment.property, assignment.value);
    }
}

}  // namespace

bool AutoloopPattern::add_step(AutoloopStep step) noexcept {
    if (step_count >= steps.size() || step.look == nullptr || !std::isfinite(step.at_beat) ||
        step.at_beat < 0.0F) {
        return false;
    }
    if (step_count == 0 && step.at_beat != 0.0F) {
        return false;
    }
    if (step_count > 0 && step.at_beat <= steps[step_count - 1U].at_beat) {
        return false;
    }
    if (std::isfinite(length_beats) && length_beats > 0.0F && step.at_beat >= length_beats) {
        return false;
    }
    steps[step_count++] = step;
    return true;
}

AutoloopResult validate_autoloop_pattern(const AutoloopPattern& pattern) noexcept {
    if (pattern.name == nullptr || pattern.name[0] == '\0') {
        return {AutoloopError::MissingName, 0, LookError::None};
    }
    if (!std::isfinite(pattern.length_beats) || pattern.length_beats <= 0.0F) {
        return {AutoloopError::InvalidLength, 0, LookError::None};
    }
    if (pattern.step_count == 0 || pattern.step_count > pattern.steps.size()) {
        return {AutoloopError::MissingSteps, 0, LookError::None};
    }

    for (std::size_t index = 0; index < pattern.step_count; ++index) {
        const auto& step = pattern.steps[index];
        if (step.look == nullptr) {
            return {AutoloopError::MissingLook, index, LookError::None};
        }
        const auto look_result = validate_static_look(*step.look);
        if (!look_result) {
            return {AutoloopError::InvalidLook, index, look_result.error};
        }
        if (!std::isfinite(step.at_beat) || step.at_beat < 0.0F) {
            return {AutoloopError::InvalidStepTime, index, LookError::None};
        }
        if (index == 0 && step.at_beat != 0.0F) {
            return {AutoloopError::FirstStepNotZero, index, LookError::None};
        }
        if (index > 0 && step.at_beat <= pattern.steps[index - 1U].at_beat) {
            return {AutoloopError::StepsOutOfOrder, index, LookError::None};
        }
        if (step.at_beat >= pattern.length_beats) {
            return {AutoloopError::StepOutsidePattern, index, LookError::None};
        }
    }
    return {};
}

bool AutoloopEngine::apply(
    const AutoloopPattern& pattern,
    double beat_position,
    LayerId layer,
    LayerStack& layers) noexcept {
    if (!validate_autoloop_pattern(pattern)) {
        if (layer != LayerId::Count) {
            layers.clear_layer(layer);
        }
        return false;
    }
    return apply_validated(pattern, beat_position, layer, layers);
}

bool AutoloopEngine::apply_validated(
    const AutoloopPattern& pattern,
    double beat_position,
    LayerId layer,
    LayerStack& layers) noexcept {
    if (!std::isfinite(beat_position) || layer == LayerId::Count || pattern.step_count == 0 ||
        pattern.step_count > pattern.steps.size() || pattern.length_beats <= 0.0F) {
        if (layer != LayerId::Count) {
            layers.clear_layer(layer);
        }
        return false;
    }

    auto phase = std::fmod(beat_position, static_cast<double>(pattern.length_beats));
    if (phase < 0.0) {
        phase += pattern.length_beats;
    }

    std::size_t current_index = 0;
    for (std::size_t index = 1; index < pattern.step_count; ++index) {
        if (static_cast<double>(pattern.steps[index].at_beat) > phase) {
            break;
        }
        current_index = index;
    }

    const auto next_index = (current_index + 1U) % pattern.step_count;
    const auto& current_step = pattern.steps[current_index];
    const auto& next_step = pattern.steps[next_index];
    load_validated_look(*current_step.look, current_);

    if (pattern.step_count == 1 ||
        current_step.transition_to_next == AutoloopTransition::Cut) {
        layers.replace_layer(layer, current_);
        return true;
    }

    load_validated_look(*next_step.look, next_);
    const auto current_beat = static_cast<double>(current_step.at_beat);
    const auto next_beat = next_index == 0
        ? static_cast<double>(pattern.length_beats) + next_step.at_beat
        : static_cast<double>(next_step.at_beat);
    const auto adjusted_phase = phase < current_beat ? phase + pattern.length_beats : phase;
    const auto duration = next_beat - current_beat;
    const auto amount = duration > 0.0
        ? static_cast<float>((adjusted_phase - current_beat) / duration)
        : 0.0F;
    blend_layer_buffers(current_, next_, amount, layer, layers, output_);
    return true;
}

bool AutoloopCatalog::set(
    AutoloopAddress address,
    const AutoloopPattern* pattern) noexcept {
    if (!address.valid() || pattern == nullptr || !validate_autoloop_pattern(*pattern)) {
        return false;
    }
    slots_[index(address)] = pattern;
    return true;
}

void AutoloopCatalog::clear(AutoloopAddress address) noexcept {
    if (address.valid()) {
        slots_[index(address)] = nullptr;
    }
}

const AutoloopPattern* AutoloopCatalog::get(AutoloopAddress address) const noexcept {
    return address.valid() ? slots_[index(address)] : nullptr;
}

bool AutoloopCatalog::swap_slots(AutoloopAddress first, AutoloopAddress second) noexcept {
    if (!first.valid() || !second.valid()) {
        return false;
    }
    std::swap(slots_[index(first)], slots_[index(second)]);
    return true;
}

bool AutoloopCatalog::duplicate(
    AutoloopAddress source,
    AutoloopAddress destination) noexcept {
    if (!source.valid() || !destination.valid() || get(source) == nullptr ||
        get(destination) != nullptr) {
        return false;
    }
    slots_[index(destination)] = get(source);
    return true;
}

bool AutoloopCatalog::select_exclusive_bank(std::uint16_t bank) noexcept {
    if (bank >= kMaxAutoloopBanks) {
        return false;
    }
    active_bank_mask_ = std::uint64_t{1} << bank;
    return true;
}

bool AutoloopCatalog::set_bank_enabled(std::uint16_t bank, bool enabled) noexcept {
    if (bank >= kMaxAutoloopBanks) {
        return false;
    }
    const auto bit = std::uint64_t{1} << bank;
    if (enabled) {
        active_bank_mask_ |= bit;
    } else {
        active_bank_mask_ &= ~bit;
    }
    return true;
}

bool AutoloopCatalog::bank_enabled(std::uint16_t bank) const noexcept {
    return bank < kMaxAutoloopBanks &&
        (active_bank_mask_ & (std::uint64_t{1} << bank)) != 0U;
}

AutoloopAddress AutoloopCatalog::next_available(AutoloopAddress after) const noexcept {
    const auto start = after.valid() ? index(after) : kMaxAutoloops - 1U;
    for (std::size_t offset = 1; offset <= kMaxAutoloops; ++offset) {
        const auto candidate = (start + offset) % kMaxAutoloops;
        const auto bank = static_cast<std::uint16_t>(candidate / kAutoloopsPerBank);
        if (bank_enabled(bank) && slots_[candidate] != nullptr) {
            return {
                bank,
                static_cast<std::uint8_t>(candidate % kAutoloopsPerBank)};
        }
    }
    return {};
}

AutoloopAddress AutoloopCatalog::previous_available(AutoloopAddress before) const noexcept {
    const auto start = before.valid() ? index(before) : 0U;
    for (std::size_t offset = 1; offset <= kMaxAutoloops; ++offset) {
        const auto candidate = (start + kMaxAutoloops - offset) % kMaxAutoloops;
        const auto bank = static_cast<std::uint16_t>(candidate / kAutoloopsPerBank);
        if (bank_enabled(bank) && slots_[candidate] != nullptr) {
            return {
                bank,
                static_cast<std::uint8_t>(candidate % kAutoloopsPerBank)};
        }
    }
    return {};
}

bool AutoloopBankWindow::select_page(std::uint16_t page) noexcept {
    if (page >= kAutoloopControlPageCount) {
        return false;
    }
    page_ = page;
    return true;
}

void AutoloopBankWindow::next_page() noexcept {
    page_ = static_cast<std::uint16_t>((page_ + 1U) % kAutoloopControlPageCount);
}

void AutoloopBankWindow::previous_page() noexcept {
    page_ = static_cast<std::uint16_t>(
        (page_ + kAutoloopControlPageCount - 1U) % kAutoloopControlPageCount);
}

AutoloopAddress AutoloopBankWindow::address(
    std::uint8_t visible_bank,
    std::uint8_t slot) const noexcept {
    if (visible_bank >= kAutoloopBanksPerControlPage || slot >= kAutoloopsPerBank) {
        return {};
    }
    return {
        static_cast<std::uint16_t>(
            static_cast<std::size_t>(page_) * kAutoloopBanksPerControlPage + visible_bank),
        slot};
}

bool AutoloopPlayer::trigger(
    const AutoloopCatalog& catalog,
    AutoloopAddress address,
    AutoloopRepeat repeat,
    double beat_position,
    bool track_playing,
    LayerStack& layers) noexcept {
    const auto* pattern = catalog.get(address);
    if (pattern == nullptr || !validate_autoloop_pattern(*pattern) ||
        !std::isfinite(beat_position) ||
        (repeat == AutoloopRepeat::TrackDuration && !track_playing)) {
        return false;
    }

    pattern_ = pattern;
    start_beat_ = beat_position;
    status_ = {true, address, repeat, 0.0, 0.0F, 0U, pattern->name};
    tick(beat_position, track_playing, layers);
    return true;
}

void AutoloopPlayer::clear(LayerStack& layers) noexcept {
    layers.clear_layer(layer_);
    status_.active = false;
    pattern_ = nullptr;
}

void AutoloopPlayer::tick(
    double beat_position,
    bool track_playing,
    LayerStack& layers) noexcept {
    if (!status_.active || pattern_ == nullptr) {
        return;
    }
    if (!std::isfinite(beat_position) ||
        (status_.repeat == AutoloopRepeat::TrackDuration && !track_playing)) {
        clear(layers);
        return;
    }

    const auto elapsed = std::max(0.0, beat_position - start_beat_);
    const auto length = static_cast<double>(pattern_->length_beats);
    status_.elapsed_beats = elapsed;
    const auto cycles = std::floor(elapsed / length);
    status_.completed_cycles = static_cast<std::uint32_t>(std::min(
        cycles,
        static_cast<double>(std::numeric_limits<std::uint32_t>::max())));

    if (status_.repeat == AutoloopRepeat::Once && elapsed >= length) {
        status_.progress = 1.0F;
        layers.clear_layer(layer_);
        status_.active = false;
        pattern_ = nullptr;
        return;
    }

    status_.progress = static_cast<float>(std::fmod(elapsed, length) / length);
    if (!engine_.apply_validated(*pattern_, elapsed, layer_, layers)) {
        clear(layers);
    }
}

}  // namespace showcore
