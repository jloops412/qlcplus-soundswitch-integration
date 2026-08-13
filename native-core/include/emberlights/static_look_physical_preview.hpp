#pragma once

#include "emberlights/project.hpp"
#include "emberlights/runner.hpp"
#include "emberlights/static_look_authoring.hpp"

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace emberlights {

inline constexpr std::uint32_t kDefaultStaticLookPhysicalPreviewTimeoutMs = 30'000U;
inline constexpr float kDefaultStaticLookPhysicalPreviewOutputCap = 0.35F;

enum class StaticLookPhysicalPreviewError : std::uint8_t {
    None,
    InvalidConfiguration,
    LiveRunning,
    AlreadyActive,
    NotActive,
    NoOutputConfigured,
    TargetNotFound,
    EmptyTarget,
    EmptyLook,
    UnsafeAssignment,
    UnsafeProfile,
    ValidationFailed,
    CompilationFailed,
    RunnerStartFailed,
    OutputNotReady,
    RunnerActivationFailed,
    LookTriggerFailed,
    TimedOut,
    RunnerFault
};

enum class StaticLookPhysicalPreviewState : std::uint8_t {
    Stopped,
    Starting,
    Active,
    Updating,
    Stopping,
    TimedOut,
    Fault
};

enum class StaticLookPhysicalPreviewStopReason : std::uint8_t {
    None,
    Explicit,
    Timeout,
    RunnerFault,
    RejectedUpdate,
    Destroyed
};

struct StaticLookPhysicalPreviewConfig {
    std::uint32_t timeout_ms{kDefaultStaticLookPhysicalPreviewTimeoutMs};
    std::uint32_t runner_start_timeout_ms{5'000U};
    std::uint32_t activation_timeout_ms{2'000U};
    float output_cap{kDefaultStaticLookPhysicalPreviewOutputCap};
};

struct StaticLookPhysicalPreviewCandidate {
    StaticLookPhysicalPreviewError error{StaticLookPhysicalPreviewError::None};
    ProjectDocument project;
    ProjectValidation validation;
    std::vector<std::string> warnings;
    std::size_t selected_fixture_count{0U};
    std::size_t retained_assignment_count{0U};
    std::size_t stripped_assignment_count{0U};

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == StaticLookPhysicalPreviewError::None;
    }
};

struct StaticLookPhysicalPreviewResult {
    StaticLookPhysicalPreviewError error{StaticLookPhysicalPreviewError::None};
    ProjectValidation validation;
    std::vector<std::string> warnings;
    std::size_t selected_fixture_count{0U};
    std::uint64_t deadline_ms{0U};

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == StaticLookPhysicalPreviewError::None;
    }
};

struct StaticLookPhysicalPreviewStatus {
    StaticLookPhysicalPreviewState state{StaticLookPhysicalPreviewState::Stopped};
    StaticLookPhysicalPreviewError error{StaticLookPhysicalPreviewError::None};
    StaticLookPhysicalPreviewStopReason stop_reason{
        StaticLookPhysicalPreviewStopReason::None};
    bool owns_runner{false};
    std::uint64_t started_at_ms{0U};
    std::uint64_t deadline_ms{0U};
    std::uint64_t remaining_ms{0U};
    std::uint64_t update_count{0U};
    std::size_t selected_fixture_count{0U};
    float output_cap{0.0F};
};

// Produces the only project shape that may enter the physical-preview service.
// The candidate keeps the configured output adapters, disables all inputs and
// automation, zeros every fixture-profile default, retains only the selected
// target's draft assignments, rejects positive hazard/unknown assignments,
// and caps direct emitters plus master intensity.
[[nodiscard]] StaticLookPhysicalPreviewCandidate
build_static_look_physical_preview_candidate(
    const ProjectDocument& project,
    const StaticLookDraft& draft,
    std::string_view target_id,
    StaticLookPhysicalPreviewConfig config = {});

// A bounded hardware-preview lease over the production Runner. This class does
// not create another renderer or output implementation. Begin is accepted only
// when the supplied Runner is stopped. Every owned exit calls Runner::stop(),
// whose output path emits its terminal blackout train. The watchdog independently
// enforces the deadline and runner/output faults even if the UI stops polling.
class StaticLookPhysicalPreviewService {
public:
    explicit StaticLookPhysicalPreviewService(RunnerService& runner);
    ~StaticLookPhysicalPreviewService() noexcept;

    StaticLookPhysicalPreviewService(const StaticLookPhysicalPreviewService&) = delete;
    StaticLookPhysicalPreviewService& operator=(
        const StaticLookPhysicalPreviewService&) = delete;

    [[nodiscard]] StaticLookPhysicalPreviewResult begin(
        const ProjectDocument& project,
        const StaticLookDraft& draft,
        std::string_view target_id,
        StaticLookPhysicalPreviewConfig config = {});

    // Recompiles off the scheduler, briefly blackouts, atomically activates the
    // new immutable package, then reveals it. Updates never extend the original
    // session deadline.
    [[nodiscard]] StaticLookPhysicalPreviewResult update(
        const ProjectDocument& project,
        const StaticLookDraft& draft,
        std::string_view target_id);

    [[nodiscard]] bool stop() noexcept;
    [[nodiscard]] StaticLookPhysicalPreviewStatus status() const noexcept;

    // Deterministic deadline seam for tests and hosts with their own timer. The
    // internal watchdog calls the same path using RunnerService::monotonic_ms().
    [[nodiscard]] bool enforce_deadline(std::uint64_t now_ms) noexcept;

private:
    [[nodiscard]] StaticLookPhysicalPreviewResult activate_candidate(
        StaticLookPhysicalPreviewCandidate candidate,
        bool initial);
    [[nodiscard]] bool wait_for_runner_started(
        std::uint32_t timeout_ms) const noexcept;
    [[nodiscard]] bool wait_for_outputs_ready(
        const ConnectionSettings& connections,
        std::uint32_t timeout_ms) const noexcept;
    [[nodiscard]] bool wait_for_look_activation(
        std::uint64_t package_generation,
        std::uint32_t timeout_ms) const noexcept;
    void stop_owned_locked(
        StaticLookPhysicalPreviewState terminal_state,
        StaticLookPhysicalPreviewError error,
        StaticLookPhysicalPreviewStopReason reason) noexcept;
    void watchdog_loop() noexcept;

    RunnerService& runner_;
    mutable std::mutex mutex_;
    std::condition_variable watchdog_condition_;
    std::thread watchdog_;
    StaticLookPhysicalPreviewStatus status_{};
    StaticLookPhysicalPreviewConfig config_{};
    ConnectionSettings preview_connections_{};
    bool shutting_down_{false};
    std::uint64_t lease_generation_{0U};
};

[[nodiscard]] const char* static_look_physical_preview_error_name(
    StaticLookPhysicalPreviewError error) noexcept;

}  // namespace emberlights
