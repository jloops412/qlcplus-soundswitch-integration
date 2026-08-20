#pragma once

#include "showcore/autoloop_program.hpp"
#include "showcore/layer_resolver.hpp"

#include <cstdint>

namespace showcore {

enum class AutoloopDirectorSource : std::uint8_t {
    None,
    Autonomous,
    Scripted,
    Manual,
    Count
};

enum class AutoloopSelectionPolicy : std::uint8_t {
    Sequential,
    Count
};

enum class AutoloopQueuedReason : std::uint8_t {
    None,
    BankMaskBoundary,
    Count
};

enum class AutoloopTransitionReason : std::uint8_t {
    None,
    InitialFallback,
    NaturalBoundary,
    PendingBankBoundary,
    DirectScripted,
    DirectManual,
    TrackBoundary,
    TransportDiscontinuity,
    PackageActivation,
    Fault,
    Count
};

enum class AutoloopDirectorFault : std::uint8_t {
    None,
    PackageUnavailable,
    InvalidCompiledProgram,
    EvaluationFailed,
    ExternalFault,
    Count
};

enum class AutoloopDirectorResult : std::uint8_t {
    None,
    PackageActivated,
    PackageCleared,
    BankMaskApplied,
    BankMaskPending,
    AutonomousStarted,
    AutonomousAdvanced,
    AutonomousStopped,
    ScriptedStarted,
    ScriptedCompleted,
    ScriptedCleared,
    ManualStarted,
    ManualCompleted,
    ManualCleared,
    TrackBoundaryReleased,
    TransportRebased,
    NoEligiblePlacement,
    StaleGeneration,
    PackageUnavailable,
    InvalidAddress,
    EmptyPlacement,
    InvalidCompiledProgram,
    UnsupportedLaunchProfile,
    TrackBoundaryUnavailable,
    TrackInactive,
    EvaluationFailed,
    Faulted,
    Count
};

struct AutoloopDirectorConfig {
    AutoloopSelectionPolicy selection_policy{
        AutoloopSelectionPolicy::Sequential};
    std::uint64_t deterministic_seed{0U};
};

// This is normalized Runner input. Source-specific transport parsing and
// prediction happen before this boundary. When phase_available is false, the
// director holds its last musical phase. A discontinuity deterministically
// rebases active launch-relative sessions without guessing its source.
struct AutoloopTransportState {
    std::int64_t musical_tick{0};
    bool phase_available{true};
    bool running{true};
    bool autonomous_eligible{true};
    bool discontinuity{false};
    bool track_boundary_available{false};
    bool track_active{false};
    std::uint64_t track_epoch{0U};
};

struct AutoloopLaunchRequest {
    AutoloopAddress address{};
    std::uint64_t expected_package_generation{0U};
    // The compiled launch profile is authoritative by default. A typed direct
    // launch may override only repeat and Overlay/Replace mode; quantization,
    // phase origin, and return fade remain package contracts.
    bool override_repeat_and_mode{false};
    AutoloopRepeat repeat{AutoloopRepeat::Once};
    CompiledAutoloopPlaybackMode mode{
        CompiledAutoloopPlaybackMode::Overlay};
};

struct AutoloopSelectionStatus {
    bool valid{false};
    AutoloopAddress address{};
    std::uint32_t program_index{kInvalidCompiledAutoloopIndex};
    CompiledAutoloopStableKey asset_key{};
};

struct AutoloopSessionStatus {
    bool active{false};
    AutoloopDirectorSource source{AutoloopDirectorSource::None};
    AutoloopSelectionStatus selection{};
    AutoloopRepeat repeat{AutoloopRepeat::Once};
    CompiledAutoloopPlaybackMode mode{
        CompiledAutoloopPlaybackMode::Overlay};
    CompiledAutoloopLaunchQuantization launch{
        CompiledAutoloopLaunchQuantization::Immediate};
    CompiledAutoloopPhaseOrigin phase_origin{
        CompiledAutoloopPhaseOrigin::Launch};
    std::int64_t start_tick{0};
    std::int64_t phase_tick{0};
    float progress{0.0F};
    std::uint32_t completed_cycles{0U};
    std::uint64_t package_generation{0U};
    std::uint64_t track_epoch{0U};
};

struct AutoloopDirectorStatus {
    bool package_active{false};
    std::uint64_t package_generation{0U};
    AutoloopDirectorFault fault{AutoloopDirectorFault::None};
    AutoloopSelectionPolicy selection_policy{
        AutoloopSelectionPolicy::Sequential};
    std::uint64_t deterministic_seed{0U};
    std::uint64_t active_bank_mask{~std::uint64_t{0U}};
    bool has_pending_bank_mask{false};
    std::uint64_t pending_bank_mask{0U};
    std::int16_t active_exclusive_bank{-1};
    std::int16_t pending_exclusive_bank{-1};
    bool track_boundary_available{false};
    AutoloopSelectionStatus selected{};
    AutoloopSelectionStatus queued{};
    AutoloopSelectionStatus active{};
    AutoloopDirectorSource active_source{AutoloopDirectorSource::None};
    CompiledAutoloopPlaybackMode active_mode{
        CompiledAutoloopPlaybackMode::Overlay};
    AutoloopRepeat active_repeat{AutoloopRepeat::Once};
    std::int64_t active_phase_tick{0};
    float active_progress{0.0F};
    std::uint32_t active_completed_cycles{0U};
    AutoloopQueuedReason queued_reason{AutoloopQueuedReason::None};
    AutoloopTransitionReason transition_reason{
        AutoloopTransitionReason::None};
    AutoloopDirectorResult last_result{AutoloopDirectorResult::None};
    std::uint64_t transition_count{0U};
    AutoloopSessionStatus autonomous{};
    AutoloopSessionStatus scripted{};
    AutoloopSessionStatus manual{};
};

class AutoloopDirector {
public:
    explicit AutoloopDirector(AutoloopDirectorConfig config = {}) noexcept;

    // The caller owns the immutable package for the entire active generation.
    // Every activation clears all director-owned sessions/layers before the new
    // package can produce output. A null package fails closed.
    [[nodiscard]] AutoloopDirectorResult activate_package(
        const CompiledAutoloopPackage* package,
        std::uint64_t generation,
        LayerStack& layers) noexcept;
    [[nodiscard]] AutoloopDirectorResult clear_package(
        LayerStack& layers) noexcept;

    [[nodiscard]] AutoloopDirectorResult request_bank_mask(
        std::uint64_t mask,
        std::uint64_t expected_generation) noexcept;
    [[nodiscard]] AutoloopDirectorResult request_all_banks(
        std::uint64_t expected_generation) noexcept;
    [[nodiscard]] AutoloopDirectorResult request_exclusive_bank(
        std::uint16_t bank,
        std::uint64_t expected_generation) noexcept;
    [[nodiscard]] AutoloopDirectorResult set_bank_enabled(
        std::uint16_t bank,
        bool enabled,
        std::uint64_t expected_generation) noexcept;

    [[nodiscard]] AutoloopDirectorResult launch_scripted(
        const AutoloopLaunchRequest& request,
        const AutoloopTransportState& transport,
        LayerStack& layers) noexcept;
    [[nodiscard]] AutoloopDirectorResult clear_scripted(
        std::uint64_t expected_generation,
        LayerStack& layers) noexcept;
    [[nodiscard]] AutoloopDirectorResult launch_manual(
        const AutoloopLaunchRequest& request,
        const AutoloopTransportState& transport,
        LayerStack& layers) noexcept;
    [[nodiscard]] AutoloopDirectorResult clear_manual(
        std::uint64_t expected_generation,
        LayerStack& layers) noexcept;

    [[nodiscard]] AutoloopDirectorResult tick(
        const AutoloopTransportState& transport,
        LayerStack& layers) noexcept;
    [[nodiscard]] AutoloopDirectorResult fault(
        AutoloopDirectorFault reason,
        LayerStack& layers) noexcept;

    [[nodiscard]] const AutoloopDirectorStatus& status() const noexcept {
        return status_;
    }

private:
    [[nodiscard]] AutoloopDirectorResult launch_session(
        AutoloopSessionStatus& session,
        AutoloopDirectorSource source,
        const AutoloopLaunchRequest& request,
        const AutoloopTransportState& transport,
        LayerStack& layers) noexcept;
    [[nodiscard]] AutoloopSelectionStatus selection_for(
        AutoloopAddress address) const noexcept;
    [[nodiscard]] AutoloopSelectionStatus next_eligible(
        AutoloopAddress after,
        std::uint64_t bank_mask) const noexcept;
    [[nodiscard]] std::int64_t effective_tick(
        const AutoloopTransportState& transport) noexcept;
    [[nodiscard]] bool update_session(
        AutoloopSessionStatus& session,
        std::int64_t tick,
        const AutoloopTransportState& transport,
        AutoloopDirectorResult completion_result,
        LayerId layer,
        LayerStack& layers) noexcept;
    [[nodiscard]] bool evaluate_session(
        const AutoloopSessionStatus& session,
        LayerBuffer& output) noexcept;
    [[nodiscard]] bool render_owned_layers(LayerStack& layers) noexcept;
    void rebase_sessions(std::int64_t tick) noexcept;
    void clear_sessions_and_layers(LayerStack& layers) noexcept;
    void clear_session(
        AutoloopSessionStatus& session,
        LayerId layer,
        LayerStack& layers) noexcept;
    void apply_pending_bank_mask() noexcept;
    void refresh_queued_selection() noexcept;
    void refresh_aggregate_status() noexcept;
    void record_transition(
        AutoloopDirectorResult result,
        AutoloopTransitionReason reason) noexcept;
    [[nodiscard]] bool command_generation_matches(
        std::uint64_t generation) noexcept;

    const CompiledAutoloopPackage* package_{nullptr};
    AutoloopProgramEvaluator evaluator_{};
    LayerBuffer autonomous_output_{};
    LayerBuffer scripted_output_{};
    LayerBuffer manual_output_{};
    AutoloopDirectorStatus status_{};
    AutoloopAddress autonomous_cursor_{};
    std::uint64_t last_activation_generation_{0U};
    bool has_effective_tick_{false};
    std::int64_t last_effective_tick_{0};
};

[[nodiscard]] const char* autoloop_director_result_name(
    AutoloopDirectorResult result) noexcept;
[[nodiscard]] const char* autoloop_director_fault_name(
    AutoloopDirectorFault fault) noexcept;

}  // namespace showcore
