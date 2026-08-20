#include "emberlights/static_look_preview_coordinator.hpp"

#include "emberlights/fixture_capabilities.hpp"

#include <utility>

namespace emberlights {
namespace {

[[nodiscard]] StaticLookPreviewCoordinatorState coordinator_state(
    StaticLookPhysicalPreviewState state) noexcept {
    switch (state) {
    case StaticLookPhysicalPreviewState::Stopped:
        return StaticLookPreviewCoordinatorState::Stopped;
    case StaticLookPhysicalPreviewState::Starting:
        return StaticLookPreviewCoordinatorState::Starting;
    case StaticLookPhysicalPreviewState::Active:
        return StaticLookPreviewCoordinatorState::Active;
    case StaticLookPhysicalPreviewState::Updating:
        return StaticLookPreviewCoordinatorState::Updating;
    case StaticLookPhysicalPreviewState::Stopping:
        return StaticLookPreviewCoordinatorState::Stopping;
    case StaticLookPhysicalPreviewState::TimedOut:
        return StaticLookPreviewCoordinatorState::TimedOut;
    case StaticLookPhysicalPreviewState::Fault:
        return StaticLookPreviewCoordinatorState::Fault;
    }
    return StaticLookPreviewCoordinatorState::Fault;
}

[[nodiscard]] const char* simulation_error_name(
    StaticLookPreviewError error) noexcept {
    switch (error) {
    case StaticLookPreviewError::None: return "none";
    case StaticLookPreviewError::LookNotFound: return "look-not-found";
    case StaticLookPreviewError::ValidationFailed:
        return "simulation-validation-failed";
    case StaticLookPreviewError::CompilationFailed:
        return "simulation-compilation-failed";
    case StaticLookPreviewError::LookPlaybackFailed:
        return "simulation-playback-failed";
    }
    return "simulation-failed";
}

[[nodiscard]] bool terminal_physical_state(
    StaticLookPhysicalPreviewState state) noexcept {
    return state == StaticLookPhysicalPreviewState::TimedOut ||
        state == StaticLookPhysicalPreviewState::Fault;
}

}  // namespace

StaticLookPreviewCoordinator::StaticLookPreviewCoordinator(
    RunnerService& preview_runner,
    bool physical_enabled)
    : runner_(preview_runner),
      physical_(preview_runner),
      physical_enabled_(physical_enabled) {
    status_.physical_enabled = physical_enabled_;
    worker_ = std::thread(
        &StaticLookPreviewCoordinator::worker_loop, this);
}

StaticLookPreviewCoordinator::~StaticLookPreviewCoordinator() noexcept {
    {
        std::lock_guard lock(mutex_);
        pending_.reset();
        stop_requested_ = true;
        shutting_down_ = true;
    }
    condition_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

bool StaticLookPreviewCoordinator::physical_available(
    const ProjectDocument& project) const noexcept {
    return physical_enabled_ &&
        static_look_physical_preview_output_configured(project.connections) &&
        runner_.status().state == RunnerState::Stopped;
}

StaticLookPreviewRequestResult StaticLookPreviewCoordinator::start(
    const ProjectDocument& project,
    const StaticLookDraft& draft,
    std::string_view target_id,
    StaticLookPreviewMode mode,
    StaticLookPhysicalPreviewConfig config) {
    reconcile_physical_terminal();
    if (draft.look.id.empty() || target_id.empty() ||
        mode == StaticLookPreviewMode::None) {
        return StaticLookPreviewRequestResult::InvalidArguments;
    }
    if (mode == StaticLookPreviewMode::Physical &&
        !physical_available(project)) {
        return StaticLookPreviewRequestResult::Unavailable;
    }

    std::lock_guard lock(mutex_);
    if (shutting_down_) {
        return StaticLookPreviewRequestResult::Unavailable;
    }
    if (status_.state == StaticLookPreviewCoordinatorState::Starting ||
        status_.state == StaticLookPreviewCoordinatorState::Active ||
        status_.state == StaticLookPreviewCoordinatorState::Updating ||
        status_.state == StaticLookPreviewCoordinatorState::Stopping) {
        return status_.mode == mode && status_.look_id == draft.look.id &&
                status_.target_id == target_id
            ? StaticLookPreviewRequestResult::NoChange
            : StaticLookPreviewRequestResult::Unavailable;
    }

    pending_ = WorkItem{
        project,
        draft,
        std::string(target_id),
        mode,
        config,
        true};
    stop_requested_ = false;
    const auto sequence = status_.sequence + 1U;
    status_ = {};
    status_.state = StaticLookPreviewCoordinatorState::Starting;
    status_.mode = mode;
    status_.look_id = draft.look.id;
    status_.target_id = std::string(target_id);
    status_.physical_enabled = physical_enabled_;
    status_.sequence = sequence;
    condition_.notify_all();
    return StaticLookPreviewRequestResult::Accepted;
}

StaticLookPreviewRequestResult StaticLookPreviewCoordinator::update(
    const ProjectDocument& project,
    const StaticLookDraft& draft,
    std::string_view target_id) {
    reconcile_physical_terminal();
    if (draft.look.id.empty() || target_id.empty()) {
        return StaticLookPreviewRequestResult::InvalidArguments;
    }
    std::lock_guard lock(mutex_);
    if (shutting_down_ || stop_requested_ ||
        (status_.state != StaticLookPreviewCoordinatorState::Starting &&
         status_.state != StaticLookPreviewCoordinatorState::Active &&
         status_.state != StaticLookPreviewCoordinatorState::Updating)) {
        return StaticLookPreviewRequestResult::Unavailable;
    }
    if (status_.look_id != draft.look.id || status_.target_id != target_id ||
        status_.mode == StaticLookPreviewMode::None) {
        return StaticLookPreviewRequestResult::InvalidArguments;
    }
    const bool still_initial = pending_.has_value() && pending_->initial;
    pending_ = WorkItem{
        project,
        draft,
        std::string(target_id),
        status_.mode,
        pending_.has_value() ? pending_->config
                             : StaticLookPhysicalPreviewConfig{},
        still_initial};
    status_.state = still_initial
        ? StaticLookPreviewCoordinatorState::Starting
        : StaticLookPreviewCoordinatorState::Updating;
    ++status_.sequence;
    condition_.notify_all();
    return StaticLookPreviewRequestResult::Accepted;
}

StaticLookPreviewRequestResult StaticLookPreviewCoordinator::stop() noexcept {
    std::lock_guard lock(mutex_);
    if (status_.state == StaticLookPreviewCoordinatorState::Stopped &&
        !pending_.has_value()) {
        return StaticLookPreviewRequestResult::NoChange;
    }
    if (status_.state == StaticLookPreviewCoordinatorState::Stopping ||
        stop_requested_) {
        return StaticLookPreviewRequestResult::NoChange;
    }
    pending_.reset();
    stop_requested_ = true;
    status_.state = StaticLookPreviewCoordinatorState::Stopping;
    ++status_.sequence;
    condition_.notify_all();
    return StaticLookPreviewRequestResult::Accepted;
}

StaticLookPreviewCoordinatorStatus StaticLookPreviewCoordinator::status() {
    reconcile_physical_terminal();
    StaticLookPreviewCoordinatorStatus snapshot;
    {
        std::lock_guard lock(mutex_);
        snapshot = status_;
    }
    if (snapshot.mode == StaticLookPreviewMode::Physical) {
        const auto physical = physical_.status();
        const auto* physical_error = static_look_physical_preview_error_name(
            physical.error);
        const bool stale_previous_lease =
            terminal_physical_state(physical.state) &&
            (snapshot.state == StaticLookPreviewCoordinatorState::Starting ||
             (snapshot.state == StaticLookPreviewCoordinatorState::Fault &&
              snapshot.error != physical_error));
        if (!stale_previous_lease) {
            snapshot.remaining_ms = physical.remaining_ms;
            snapshot.update_count = physical.update_count;
            snapshot.selected_fixture_count = physical.selected_fixture_count;
            snapshot.output_cap = physical.output_cap;
            if (physical.error != StaticLookPhysicalPreviewError::None) {
                snapshot.error = physical_error;
            }
            if (terminal_physical_state(physical.state)) {
                snapshot.state = coordinator_state(physical.state);
            }
        }
    }
    return snapshot;
}

void StaticLookPreviewCoordinator::worker_loop() noexcept {
    for (;;) {
        std::optional<WorkItem> item;
        bool stop = false;
        {
            std::unique_lock lock(mutex_);
            condition_.wait(lock, [&] {
                return shutting_down_ || stop_requested_ || pending_.has_value();
            });
            if (shutting_down_) {
                break;
            }
            if (stop_requested_) {
                stop_requested_ = false;
                pending_.reset();
                stop = true;
            } else {
                item = std::move(pending_);
                pending_.reset();
            }
        }
        if (stop) {
            static_cast<void>(physical_.stop());
            publish_stopped();
        } else if (item.has_value()) {
            execute(std::move(*item));
        }
    }
    static_cast<void>(physical_.stop());
}

void StaticLookPreviewCoordinator::execute(WorkItem item) noexcept {
    try {
        if (item.mode == StaticLookPreviewMode::Simulation) {
            execute_simulation(std::move(item));
        } else if (item.mode == StaticLookPreviewMode::Physical) {
            execute_physical(std::move(item));
        }
    } catch (...) {
        std::lock_guard lock(mutex_);
        if (!stop_requested_ && !shutting_down_) {
            status_.state = StaticLookPreviewCoordinatorState::Fault;
            status_.error = "internal-error";
            ++status_.sequence;
        }
    }
}

void StaticLookPreviewCoordinator::execute_simulation(WorkItem item) {
    const auto preview = preview_static_look_draft(item.project, item.draft);
    const auto target = inspect_fixture_target(item.project, item.target_id);
    std::lock_guard lock(mutex_);
    if (stop_requested_ || shutting_down_) {
        return;
    }
    status_.selected_fixture_count = target.target_found
        ? target.fixtures.size()
        : 0U;
    status_.output_cap = 0.0F;
    status_.remaining_ms = 0U;
    status_.frame_sha256 = preview.frame_sha256;
    if (!preview) {
        status_.state = StaticLookPreviewCoordinatorState::Fault;
        status_.error = simulation_error_name(preview.error);
    } else if (!target.target_found || target.fixtures.empty()) {
        status_.state = StaticLookPreviewCoordinatorState::Fault;
        status_.error = target.target_found ? "empty-target" : "target-not-found";
    } else {
        status_.state = StaticLookPreviewCoordinatorState::Active;
        status_.error = "none";
        if (!item.initial) {
            ++status_.update_count;
        }
    }
    ++status_.sequence;
}

void StaticLookPreviewCoordinator::execute_physical(WorkItem item) {
    const auto result = item.initial
        ? physical_.begin(
              item.project, item.draft, item.target_id, item.config)
        : physical_.update(item.project, item.draft, item.target_id);
    const auto physical = physical_.status();
    const auto current_error =
        result.error != StaticLookPhysicalPreviewError::None
        ? result.error
        : physical.error;
    const bool current_terminal = terminal_physical_state(physical.state) &&
        (result.error == StaticLookPhysicalPreviewError::None ||
         physical.error == result.error);
    const bool use_physical_status = result || current_terminal;
    std::lock_guard lock(mutex_);
    if (stop_requested_ || shutting_down_) {
        return;
    }
    status_.frame_sha256.clear();
    status_.selected_fixture_count =
        use_physical_status && physical.selected_fixture_count != 0U
        ? physical.selected_fixture_count
        : result.selected_fixture_count;
    status_.output_cap = use_physical_status
        ? physical.output_cap
        : 0.0F;
    status_.remaining_ms = use_physical_status
        ? physical.remaining_ms
        : 0U;
    status_.update_count = use_physical_status
        ? physical.update_count
        : 0U;
    status_.error = static_look_physical_preview_error_name(current_error);
    status_.state = result
        ? StaticLookPreviewCoordinatorState::Active
        : current_terminal
            ? coordinator_state(physical.state)
            : StaticLookPreviewCoordinatorState::Fault;
    ++status_.sequence;
}

void StaticLookPreviewCoordinator::publish_stopped() noexcept {
    std::lock_guard lock(mutex_);
    const auto sequence = status_.sequence + 1U;
    status_ = {};
    status_.physical_enabled = physical_enabled_;
    status_.sequence = sequence;
}

void StaticLookPreviewCoordinator::reconcile_physical_terminal() {
    const auto observed = physical_.status();
    if (!terminal_physical_state(observed.state)) {
        return;
    }
    std::lock_guard lock(mutex_);
    if (status_.mode != StaticLookPreviewMode::Physical ||
        status_.state == StaticLookPreviewCoordinatorState::Starting ||
        status_.state == StaticLookPreviewCoordinatorState::TimedOut ||
        status_.state == StaticLookPreviewCoordinatorState::Fault) {
        return;
    }
    // The worker can begin a new lease between the optimistic physical read and
    // this coordinator lock. Re-read while the coordinator state is stable so
    // a terminal snapshot from the previous lease cannot overwrite the new
    // session's Starting/Active state.
    const auto physical = physical_.status();
    if (!terminal_physical_state(physical.state)) {
        return;
    }
    status_.state = coordinator_state(physical.state);
    status_.error = static_look_physical_preview_error_name(physical.error);
    status_.remaining_ms = 0U;
    status_.update_count = physical.update_count;
    status_.selected_fixture_count = physical.selected_fixture_count;
    status_.output_cap = physical.output_cap;
    ++status_.sequence;
}

const char* static_look_preview_mode_name(
    StaticLookPreviewMode mode) noexcept {
    switch (mode) {
    case StaticLookPreviewMode::None: return "none";
    case StaticLookPreviewMode::Simulation: return "simulation";
    case StaticLookPreviewMode::Physical: return "physical";
    }
    return "none";
}

const char* static_look_preview_coordinator_state_name(
    StaticLookPreviewCoordinatorState state) noexcept {
    switch (state) {
    case StaticLookPreviewCoordinatorState::Stopped: return "stopped";
    case StaticLookPreviewCoordinatorState::Starting: return "starting";
    case StaticLookPreviewCoordinatorState::Active: return "active";
    case StaticLookPreviewCoordinatorState::Updating: return "updating";
    case StaticLookPreviewCoordinatorState::Stopping: return "stopping";
    case StaticLookPreviewCoordinatorState::TimedOut: return "timedOut";
    case StaticLookPreviewCoordinatorState::Fault: return "fault";
    }
    return "fault";
}

}  // namespace emberlights
