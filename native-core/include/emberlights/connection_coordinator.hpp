#pragma once

#include "emberlights/project.hpp"

#include <cstdint>
#include <optional>

namespace emberlights {

using ConnectionGeneration = std::uint64_t;
using ConnectionEndpointMask = std::uint16_t;

enum class ConnectionEndpoint : std::uint8_t {
    Os2l,
    ArtNet,
    Sacn,
    DmxUsbProUniverse1,
    DmxUsbProUniverse2,
    SoundSwitchMicro,
    SoundSwitchControlOne,
    MidiInput,
    MidiOutput,
    Timing,
    Count
};

[[nodiscard]] constexpr ConnectionEndpointMask connection_endpoint_bit(
    ConnectionEndpoint endpoint) noexcept {
    return static_cast<ConnectionEndpointMask>(
        ConnectionEndpointMask{1U} << static_cast<std::uint8_t>(endpoint));
}

enum class ConnectionApplyResult : std::uint8_t {
    None,
    ValidationRejected,
    SaveFailed,
    SavedNoRuntimeChange,
    SavedAndApplied,
    SavedRestartRequired,
    SavedApplyFailed,
    ApplyRuntimeStopped,
    RuntimeStarted,
    RuntimeStopped,
    StaleGeneration,
    InvalidTransition,
    GenerationLimit
};

enum class ConnectionRuntimeState : std::uint8_t {
    Stopped,
    Running
};

// A failed restart cannot be represented as though the previous graph were
// still active unless the caller actually restored it. Partial endpoint
// recovery is deliberately outside this bounded whole-graph coordinator.
enum class ConnectionApplyFailureBoundary : std::uint8_t {
    RuntimeStopped,
    PreviousRuntimeRestored
};

struct ConnectionCoordinatorSnapshot {
    ConnectionSettings desired_settings;
    ConnectionSettings saved_settings;
    std::optional<ConnectionSettings> active_settings;
    ConnectionGeneration generation{0U};
    ConnectionEndpointMask desired_to_saved{0U};
    ConnectionEndpointMask saved_to_active{0U};
    // Endpoints relevant to the most recently acknowledged outcome. Runtime
    // transitions report the graph they stopped/started; save/apply failures
    // report the exact durable desired/saved/active drift which remains. This
    // is never an accumulated mask for an entire multi-stage restart.
    ConnectionEndpointMask last_affected_endpoints{0U};
    ConnectionApplyResult last_result{ConnectionApplyResult::None};
    bool apply_pending{false};
};

class ConnectionCoordinator;

// Immutable capability issued by one coordinator for one generation. It binds
// the exact desired settings carried through validation, persistence, and the
// eventual runtime result. Transactions are Studio/UI-side objects and never
// belong on the Runner scheduler path.
class ConnectionTransaction {
public:
    [[nodiscard]] ConnectionGeneration base_generation() const noexcept {
        return base_generation_;
    }
    [[nodiscard]] const ConnectionSettings& desired_settings() const noexcept {
        return desired_settings_;
    }

private:
    friend class ConnectionCoordinator;

    ConnectionTransaction(
        const ConnectionCoordinator* owner,
        ConnectionGeneration base_generation,
        std::uint64_t transaction_id,
        ConnectionSettings desired_settings);

    const ConnectionCoordinator* owner_{nullptr};
    ConnectionGeneration base_generation_{0U};
    std::uint64_t transaction_id_{0U};
    ConnectionSettings desired_settings_;
};

struct ConnectionPrepareOutcome {
    ConnectionApplyResult result{ConnectionApplyResult::InvalidTransition};
    ConnectionGeneration generation{0U};
    std::optional<ConnectionTransaction> transaction;

    [[nodiscard]] explicit operator bool() const noexcept {
        return transaction.has_value();
    }
};

struct ConnectionMutationOutcome {
    ConnectionApplyResult result{ConnectionApplyResult::InvalidTransition};
    ConnectionGeneration generation{0U};
    ConnectionEndpointMask affected_endpoints{0U};

    [[nodiscard]] explicit operator bool() const noexcept {
        return result == ConnectionApplyResult::SavedNoRuntimeChange ||
            result == ConnectionApplyResult::SavedAndApplied ||
            result == ConnectionApplyResult::SavedRestartRequired ||
            result == ConnectionApplyResult::ApplyRuntimeStopped ||
            result == ConnectionApplyResult::RuntimeStarted ||
            result == ConnectionApplyResult::RuntimeStopped;
    }
};

// Owns the truthful desired/saved/active connection snapshots while the
// current Windows shell still uses a bounded Runner restart for changed
// adapter graphs. It does not open adapters, persist files, or touch Runner;
// callers report those independently completed boundaries back here.
class ConnectionCoordinator {
public:
    ConnectionCoordinator(
        ConnectionSettings initial_settings,
        ConnectionRuntimeState runtime_state);

    ConnectionCoordinator(const ConnectionCoordinator&) = delete;
    ConnectionCoordinator& operator=(const ConnectionCoordinator&) = delete;
    ConnectionCoordinator(ConnectionCoordinator&&) = delete;
    ConnectionCoordinator& operator=(ConnectionCoordinator&&) = delete;

    [[nodiscard]] ConnectionCoordinatorSnapshot snapshot() const;
    [[nodiscard]] ConnectionGeneration generation() const noexcept {
        return generation_;
    }

    [[nodiscard]] ConnectionPrepareOutcome prepare(
        ConnectionGeneration expected_generation,
        ConnectionSettings desired_settings);

    // Validation and save failures update only the desired snapshot. A
    // successful save updates saved state and explicitly reports whether a
    // live runtime restart/apply is still required. When the runtime is
    // stopped, saving never creates an apply ticket; the saved graph becomes
    // the exact next-start configuration.
    [[nodiscard]] ConnectionMutationOutcome reject_validation(
        const ConnectionTransaction& transaction);
    [[nodiscard]] ConnectionMutationOutcome acknowledge_save_failed(
        const ConnectionTransaction& transaction);
    [[nodiscard]] ConnectionMutationOutcome acknowledge_saved(
        const ConnectionTransaction& transaction);

    // These are valid only after acknowledge_saved returned
    // SavedRestartRequired for this exact transaction. A failure boundary
    // must say whether the previous graph was actually restored or no runtime
    // remains active.
    // The current bounded Windows path stops the whole old graph before
    // starting the saved one. This transaction-bound observation keeps the
    // apply ticket alive while making that stopped/starting interval visible.
    [[nodiscard]] ConnectionMutationOutcome acknowledge_apply_runtime_stopped(
        const ConnectionTransaction& transaction);
    [[nodiscard]] ConnectionMutationOutcome acknowledge_applied(
        const ConnectionTransaction& transaction);
    [[nodiscard]] ConnectionMutationOutcome acknowledge_apply_failed(
        const ConnectionTransaction& transaction,
        ConnectionApplyFailureBoundary failure_boundary);

    // Whole-runtime lifecycle observations outside a save/apply transaction.
    // Starting establishes that the exact saved graph is active; stopping
    // records that no graph is active. Both fail closed while an apply ticket
    // owns the restart boundary.
    [[nodiscard]] ConnectionMutationOutcome acknowledge_runtime_started(
        ConnectionGeneration expected_generation);
    [[nodiscard]] ConnectionMutationOutcome acknowledge_runtime_stopped(
        ConnectionGeneration expected_generation);

    // Project open/new/restore boundary. Desired and saved settings can be
    // replaced only after the caller has stopped the runtime and completed
    // its output blackout/close boundary. Active remains absent until a
    // successful start is acknowledged. A running boundary fails closed;
    // callers must use the ordinary save/apply transaction for a live change.
    [[nodiscard]] ConnectionMutationOutcome replace_document_settings(
        ConnectionGeneration expected_generation,
        ConnectionSettings settings,
        ConnectionRuntimeState runtime_state);

private:
    [[nodiscard]] bool transaction_is_current(
        const ConnectionTransaction& transaction) const noexcept;
    [[nodiscard]] bool apply_ticket_is_current(
        const ConnectionTransaction& transaction) const noexcept;
    [[nodiscard]] bool can_advance_generation(
        ConnectionGeneration count = 1U) const noexcept;
    void advance_generation() noexcept;
    void clear_apply_ticket() noexcept;
    [[nodiscard]] ConnectionMutationOutcome reject_transition(
        ConnectionApplyResult result) const noexcept;
    [[nodiscard]] ConnectionMutationOutcome commit_result(
        ConnectionApplyResult result,
        ConnectionEndpointMask affected_endpoints);

    ConnectionSettings desired_settings_;
    ConnectionSettings saved_settings_;
    std::optional<ConnectionSettings> active_settings_;
    ConnectionGeneration generation_{1U};
    std::uint64_t next_transaction_id_{1U};
    std::uint64_t apply_transaction_id_{0U};
    ConnectionGeneration apply_generation_{0U};
    std::optional<ConnectionSettings> apply_previous_active_settings_;
    bool apply_runtime_stopped_{false};
    ConnectionEndpointMask last_affected_endpoints_{0U};
    ConnectionApplyResult last_result_{ConnectionApplyResult::None};
};

[[nodiscard]] ConnectionEndpointMask diff_connection_endpoints(
    const ConnectionSettings& left,
    const ConnectionSettings& right) noexcept;

// Compares only settings which can change an effective live adapter graph.
// Metadata belonging to a disabled endpoint is inert. A missing active value
// means the runtime is stopped; the returned mask identifies what a successful
// start of target_settings would activate.
[[nodiscard]] ConnectionEndpointMask diff_runtime_connection_endpoints(
    const ConnectionSettings& target_settings,
    const std::optional<ConnectionSettings>& active_settings) noexcept;

[[nodiscard]] const char* connection_endpoint_name(
    ConnectionEndpoint endpoint) noexcept;
[[nodiscard]] const char* connection_apply_result_name(
    ConnectionApplyResult result) noexcept;

}  // namespace emberlights
