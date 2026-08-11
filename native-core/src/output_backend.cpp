#include "showcore/output_backend.hpp"

namespace showcore {

const char* output_health_state_name(OutputHealthState state) noexcept {
    switch (state) {
    case OutputHealthState::Disabled: return "disabled";
    case OutputHealthState::Opening: return "opening";
    case OutputHealthState::Ready: return "ready";
    case OutputHealthState::Recovering: return "recovering";
    case OutputHealthState::Fault: return "fault";
    case OutputHealthState::Stopping: return "stopping";
    }
    return "unknown";
}

void AtomicOutputBackendHealth::configure(
    OutputBackendKind kind,
    std::uint8_t first_source_universe,
    std::uint8_t source_universe_count,
    bool configured) noexcept {
    kind_.store(kind, std::memory_order_relaxed);
    first_source_universe_.store(first_source_universe, std::memory_order_relaxed);
    source_universe_count_.store(source_universe_count, std::memory_order_relaxed);
    configured_.store(configured, std::memory_order_relaxed);
    ever_ready_.store(false, std::memory_order_relaxed);
    open_attempts_.store(0U, std::memory_order_relaxed);
    open_successes_.store(0U, std::memory_order_relaxed);
    reconnects_.store(0U, std::memory_order_relaxed);
    frames_attempted_.store(0U, std::memory_order_relaxed);
    frames_accepted_.store(0U, std::memory_order_relaxed);
    frames_failed_.store(0U, std::memory_order_relaxed);
    last_error_.store(0U, std::memory_order_relaxed);
    last_nonzero_slots_.store(0U, std::memory_order_relaxed);
    state_.store(
        configured ? OutputHealthState::Opening : OutputHealthState::Disabled,
        std::memory_order_release);
}

void AtomicOutputBackendHealth::mark_opening() noexcept {
    if (!configured_.load(std::memory_order_relaxed)) {
        state_.store(OutputHealthState::Disabled, std::memory_order_release);
        return;
    }
    open_attempts_.fetch_add(1U, std::memory_order_relaxed);
    state_.store(
        ever_ready_.load(std::memory_order_relaxed)
            ? OutputHealthState::Recovering
            : OutputHealthState::Opening,
        std::memory_order_release);
}

void AtomicOutputBackendHealth::mark_ready() noexcept {
    if (!configured_.load(std::memory_order_relaxed)) {
        return;
    }
    const bool was_ready = ever_ready_.exchange(true, std::memory_order_relaxed);
    if (was_ready && state_.load(std::memory_order_relaxed) ==
                         OutputHealthState::Recovering) {
        reconnects_.fetch_add(1U, std::memory_order_relaxed);
    }
    open_successes_.fetch_add(1U, std::memory_order_relaxed);
    last_error_.store(0U, std::memory_order_relaxed);
    state_.store(OutputHealthState::Ready, std::memory_order_release);
}

void AtomicOutputBackendHealth::mark_fault(std::uint32_t error) noexcept {
    if (error != 0U) {
        last_error_.store(error, std::memory_order_relaxed);
    }
    state_.store(OutputHealthState::Fault, std::memory_order_release);
}

void AtomicOutputBackendHealth::mark_stopping() noexcept {
    if (configured_.load(std::memory_order_relaxed)) {
        state_.store(OutputHealthState::Stopping, std::memory_order_release);
    }
}

void AtomicOutputBackendHealth::mark_disabled() noexcept {
    state_.store(OutputHealthState::Disabled, std::memory_order_release);
}

void AtomicOutputBackendHealth::record_send(
    bool accepted,
    std::uint32_t error,
    std::uint16_t nonzero_slots) noexcept {
    frames_attempted_.fetch_add(1U, std::memory_order_relaxed);
    last_nonzero_slots_.store(nonzero_slots, std::memory_order_relaxed);
    if (accepted) {
        frames_accepted_.fetch_add(1U, std::memory_order_relaxed);
        last_error_.store(0U, std::memory_order_relaxed);
        return;
    }
    frames_failed_.fetch_add(1U, std::memory_order_relaxed);
    if (error != 0U) {
        last_error_.store(error, std::memory_order_relaxed);
    }
    state_.store(OutputHealthState::Fault, std::memory_order_release);
}

OutputBackendHealth AtomicOutputBackendHealth::snapshot() const noexcept {
    OutputBackendHealth result;
    result.state = state_.load(std::memory_order_acquire);
    result.kind = kind_.load(std::memory_order_relaxed);
    result.first_source_universe =
        first_source_universe_.load(std::memory_order_relaxed);
    result.source_universe_count =
        source_universe_count_.load(std::memory_order_relaxed);
    result.configured = configured_.load(std::memory_order_relaxed);
    result.open_attempts = open_attempts_.load(std::memory_order_relaxed);
    result.open_successes = open_successes_.load(std::memory_order_relaxed);
    result.reconnects = reconnects_.load(std::memory_order_relaxed);
    result.frames_attempted = frames_attempted_.load(std::memory_order_relaxed);
    result.frames_accepted = frames_accepted_.load(std::memory_order_relaxed);
    result.frames_failed = frames_failed_.load(std::memory_order_relaxed);
    result.last_error = last_error_.load(std::memory_order_relaxed);
    result.last_nonzero_slots = last_nonzero_slots_.load(std::memory_order_relaxed);
    return result;
}

}  // namespace showcore
