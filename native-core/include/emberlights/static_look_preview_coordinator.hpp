#pragma once

#include "emberlights/static_look_physical_preview.hpp"
#include "emberlights/static_look_preview.hpp"

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

namespace emberlights {

enum class StaticLookPreviewMode : std::uint8_t {
    None,
    Simulation,
    Physical
};

enum class StaticLookPreviewCoordinatorState : std::uint8_t {
    Stopped,
    Starting,
    Active,
    Updating,
    Stopping,
    TimedOut,
    Fault
};

enum class StaticLookPreviewRequestResult : std::uint8_t {
    Accepted,
    NoChange,
    Unavailable,
    InvalidArguments
};

struct StaticLookPreviewCoordinatorStatus {
    StaticLookPreviewCoordinatorState state{
        StaticLookPreviewCoordinatorState::Stopped};
    StaticLookPreviewMode mode{StaticLookPreviewMode::None};
    std::string error{"none"};
    std::string look_id;
    std::string target_id;
    std::string frame_sha256;
    bool physical_enabled{false};
    std::uint64_t remaining_ms{0U};
    std::uint64_t update_count{0U};
    std::uint64_t sequence{0U};
    std::size_t selected_fixture_count{0U};
    float output_cap{0.0F};
};

// Toolkit-neutral asynchronous command service for Studio preview. Simulation
// compiles/renders off the caller thread and opens no adapter. Physical mode is
// additionally gated by explicit host construction, then delegates all Runner,
// cap, timeout, hazard, and terminal-blackout authority to the existing bounded
// physical-preview service. Draft updates are latest-wins/coalesced.
class StaticLookPreviewCoordinator {
public:
    StaticLookPreviewCoordinator(
        RunnerService& preview_runner,
        bool physical_enabled);
    ~StaticLookPreviewCoordinator() noexcept;

    StaticLookPreviewCoordinator(const StaticLookPreviewCoordinator&) = delete;
    StaticLookPreviewCoordinator& operator=(
        const StaticLookPreviewCoordinator&) = delete;

    [[nodiscard]] StaticLookPreviewRequestResult start(
        const ProjectDocument& project,
        const StaticLookDraft& draft,
        std::string_view target_id,
        StaticLookPreviewMode mode,
        StaticLookPhysicalPreviewConfig config = {});

    [[nodiscard]] StaticLookPreviewRequestResult update(
        const ProjectDocument& project,
        const StaticLookDraft& draft,
        std::string_view target_id);

    [[nodiscard]] StaticLookPreviewRequestResult stop() noexcept;
    [[nodiscard]] StaticLookPreviewCoordinatorStatus status();

    [[nodiscard]] bool physical_enabled() const noexcept {
        return physical_enabled_;
    }
    [[nodiscard]] bool physical_available(
        const ProjectDocument& project) const noexcept;

private:
    struct WorkItem {
        ProjectDocument project;
        StaticLookDraft draft;
        std::string target_id;
        StaticLookPreviewMode mode{StaticLookPreviewMode::None};
        StaticLookPhysicalPreviewConfig config{};
        bool initial{true};
    };

    void worker_loop() noexcept;
    void execute(WorkItem item) noexcept;
    void execute_simulation(WorkItem item);
    void execute_physical(WorkItem item);
    void publish_stopped() noexcept;
    void reconcile_physical_terminal();

    RunnerService& runner_;
    StaticLookPhysicalPreviewService physical_;
    const bool physical_enabled_{false};
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::optional<WorkItem> pending_;
    StaticLookPreviewCoordinatorStatus status_{};
    bool stop_requested_{false};
    bool shutting_down_{false};
    std::thread worker_;
};

[[nodiscard]] const char* static_look_preview_mode_name(
    StaticLookPreviewMode mode) noexcept;
[[nodiscard]] const char* static_look_preview_coordinator_state_name(
    StaticLookPreviewCoordinatorState state) noexcept;

}  // namespace emberlights
