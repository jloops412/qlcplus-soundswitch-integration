#include "emberlights/static_look_physical_preview.hpp"

#include "emberlights/compiler.hpp"
#include "emberlights/fixture_capabilities.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>

namespace emberlights {
namespace {

using Clock = std::chrono::steady_clock;

[[nodiscard]] bool valid_config(
    const StaticLookPhysicalPreviewConfig& config) noexcept {
    return config.timeout_ms >= 1'000U && config.timeout_ms <= 60'000U &&
        config.runner_start_timeout_ms >= 250U &&
        config.runner_start_timeout_ms <= 15'000U &&
        config.activation_timeout_ms >= 100U &&
        config.activation_timeout_ms <= 5'000U &&
        std::isfinite(config.output_cap) && config.output_cap > 0.0F &&
        config.output_cap <= 0.5F;
}

[[nodiscard]] bool output_configured(
    const ConnectionSettings& connections) noexcept {
    return connections.artnet_enabled || connections.sacn_enabled ||
        connections.soundswitch_micro_universe != 0U ||
        connections.soundswitch_control_one_experimental ||
        std::any_of(
            connections.dmx_usb_pro_ports.begin(),
            connections.dmx_usb_pro_ports.end(),
            [](const auto& port) { return !port.empty(); });
}

[[nodiscard]] bool direct_output_property(showcore::Property property) noexcept {
    switch (property) {
    case showcore::Property::Intensity:
    case showcore::Property::Red:
    case showcore::Property::Green:
    case showcore::Property::Blue:
    case showcore::Property::White:
    case showcore::Property::Amber:
    case showcore::Property::UV:
    case showcore::Property::Cyan:
    case showcore::Property::Magenta:
    case showcore::Property::Yellow:
    case showcore::Property::Lime:
    case showcore::Property::Indigo:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool hazard_property(showcore::Property property) noexcept {
    return property == showcore::Property::Strobe ||
        property == showcore::Property::Fog ||
        property == showcore::Property::Haze ||
        property == showcore::Property::Laser ||
        property == showcore::Property::Spark;
}

[[nodiscard]] bool unknown_property(showcore::Property property) noexcept {
    return property >= showcore::Property::Custom1 &&
        property <= showcore::Property::Custom16;
}

enum class OutputReadiness : std::uint8_t {
    Waiting,
    Ready,
    Fault
};

void include_adapter(
    bool enabled,
    AdapterState state,
    bool& waiting,
    bool& fault) noexcept {
    if (!enabled) {
        return;
    }
    waiting = waiting || state != AdapterState::Ready;
    fault = fault || state == AdapterState::Fault;
}

[[nodiscard]] OutputReadiness output_readiness(
    const ConnectionSettings& connections,
    const RunnerStatus& status) noexcept {
    bool waiting = false;
    bool fault = false;
    include_adapter(connections.artnet_enabled, status.artnet, waiting, fault);
    include_adapter(connections.sacn_enabled, status.sacn, waiting, fault);
    for (std::size_t universe = 0U;
         universe < connections.dmx_usb_pro_ports.size(); ++universe) {
        include_adapter(
            !connections.dmx_usb_pro_ports[universe].empty(),
            status.dmx_usb_pro[universe],
            waiting,
            fault);
    }
    include_adapter(
        connections.soundswitch_micro_universe != 0U,
        status.soundswitch_micro,
        waiting,
        fault);
    include_adapter(
        connections.soundswitch_control_one_experimental,
        status.soundswitch_control_one,
        waiting,
        fault);
    if (fault) {
        return OutputReadiness::Fault;
    }
    return waiting ? OutputReadiness::Waiting : OutputReadiness::Ready;
}

[[nodiscard]] StaticLookPhysicalPreviewResult result_from_candidate(
    StaticLookPhysicalPreviewCandidate candidate) {
    StaticLookPhysicalPreviewResult result;
    result.error = candidate.error;
    result.validation = std::move(candidate.validation);
    result.warnings = std::move(candidate.warnings);
    result.selected_fixture_count = candidate.selected_fixture_count;
    return result;
}

[[nodiscard]] StaticLookPhysicalPreviewResult error_result(
    StaticLookPhysicalPreviewError error) {
    StaticLookPhysicalPreviewResult result;
    result.error = error;
    return result;
}

}  // namespace

StaticLookPhysicalPreviewCandidate build_static_look_physical_preview_candidate(
    const ProjectDocument& project,
    const StaticLookDraft& draft,
    std::string_view target_id,
    StaticLookPhysicalPreviewConfig config) {
    StaticLookPhysicalPreviewCandidate result;
    if (!valid_config(config)) {
        result.error = StaticLookPhysicalPreviewError::InvalidConfiguration;
        return result;
    }
    if (!output_configured(project.connections)) {
        result.error = StaticLookPhysicalPreviewError::NoOutputConfigured;
        return result;
    }

    const auto target = inspect_fixture_target(project, target_id);
    if (!target.target_found) {
        result.error = StaticLookPhysicalPreviewError::TargetNotFound;
        return result;
    }
    if (target.fixtures.empty()) {
        result.error = StaticLookPhysicalPreviewError::EmptyTarget;
        return result;
    }
    result.selected_fixture_count = target.fixtures.size();

    std::unordered_set<std::string_view> selected;
    selected.reserve(target.fixtures.size());
    for (const auto& fixture : target.fixtures) {
        selected.emplace(fixture.fixture_id);
        const auto* profile = find_fixture_profile(project, fixture.profile_id);
        if (profile == nullptr) {
            result.error = StaticLookPhysicalPreviewError::ValidationFailed;
            result.validation = validate_project(project);
            return result;
        }
        const auto unsafe_constant = std::find_if(
            profile->channels.begin(), profile->channels.end(),
            [](const ChannelDefinition& channel) {
                return channel.encoding == showcore::ChannelEncoding::Constant8 &&
                    channel.default_value != 0U;
            });
        if (unsafe_constant != profile->channels.end()) {
            result.error = StaticLookPhysicalPreviewError::UnsafeProfile;
            result.warnings.push_back(
                std::string(fixture.fixture_name) +
                " has a nonzero constant/unknown profile channel. Physical preview "
                "refuses it until that channel has a qualified neutral value.");
            return result;
        }
    }

    result.project = project;
    auto& candidate = result.project;
    const auto effective_output_cap = std::min(
        project.safety.max_intensity, config.output_cap);
    candidate.name = project.name + " - bounded Static Look preview";
    candidate.connections.os2l_enabled = false;
    candidate.connections.midi_input_index = -1;
    candidate.connections.midi_output_index = -1;
    candidate.safety.fog_requires_arm = true;
    candidate.safety.haze_requires_arm = true;
    candidate.safety.laser_requires_arm = true;
    candidate.safety.spark_requires_arm = true;
    candidate.safety.strobe_allowed = false;
    candidate.safety.max_strobe = 0.0F;
    candidate.safety.max_intensity = effective_output_cap;

    // A released property must render zero during this special session. This
    // also prevents unselected fixtures from inheriting open shutters, macros,
    // or emitters from profile defaults.
    for (auto& profile : candidate.fixture_profiles) {
        for (auto& channel : profile.channels) {
            channel.default_value = 0U;
        }
    }

    candidate.groups.clear();
    candidate.color_palettes.clear();
    candidate.autoloops.clear();
    candidate.audio_assets.clear();
    candidate.track_scripts.clear();
    candidate.midi_mappings.clear();
    candidate.looks.clear();

    auto look = draft.look;
    look.fade_ms = 0U;
    look.assignments.clear();
    look.assignments.reserve(draft.look.assignments.size());
    for (auto assignment : draft.look.assignments) {
        if (!selected.contains(assignment.fixture_id)) {
            ++result.stripped_assignment_count;
            continue;
        }
        if (assignment.property >= showcore::Property::Count) {
            result.error = StaticLookPhysicalPreviewError::UnsafeAssignment;
            result.warnings.push_back(
                "Physical preview rejected an invalid/unknown Static Look property.");
            return result;
        }
        if ((hazard_property(assignment.property) ||
             unknown_property(assignment.property)) &&
            assignment.value.mode == showcore::ValueMode::Set &&
            assignment.value.value > 0.0F) {
            result.error = StaticLookPhysicalPreviewError::UnsafeAssignment;
            result.warnings.push_back(
                "Physical preview rejected positive " +
                std::string(property_name(assignment.property)) +
                " output. Use the Raw Hardware Test for explicit bounded "
                "qualification of unknown or hazardous channels.");
            return result;
        }
        if (hazard_property(assignment.property) ||
            unknown_property(assignment.property)) {
            assignment.value = showcore::PropertyValue::force_zero();
        } else if (direct_output_property(assignment.property) &&
                   assignment.value.mode == showcore::ValueMode::Set) {
            assignment.value.value = std::min(
                assignment.value.value, effective_output_cap);
        }
        look.assignments.push_back(std::move(assignment));
        ++result.retained_assignment_count;
    }
    if (look.assignments.empty()) {
        result.error = StaticLookPhysicalPreviewError::EmptyLook;
        result.warnings.push_back(
            "The draft has no assignments for the selected fixture or group.");
        return result;
    }
    candidate.looks.push_back(std::move(look));
    result.validation = validate_project(candidate);
    if (!result.validation.ok()) {
        result.error = StaticLookPhysicalPreviewError::ValidationFailed;
        return result;
    }

    result.warnings.insert(
        result.warnings.end(), target.warnings.begin(), target.warnings.end());
    result.warnings.push_back(
        "LIVE IS STOPPED: only the selected target is retained; all other "
        "fixture defaults and assignments render black.");
    result.warnings.push_back(
        "Preview output is capped at " +
        std::to_string(static_cast<unsigned int>(
            std::lround(effective_output_cap * 100.0F))) +
        "% and positive strobe/fog/haze/laser/spark/custom output is rejected.");
    result.warnings.push_back(
        "This bounded preview is not fixture or hardware qualification; the "
        "Raw Hardware Test remains the qualification authority.");
    return result;
}

StaticLookPhysicalPreviewService::StaticLookPhysicalPreviewService(
    RunnerService& runner)
    : runner_(runner) {
    watchdog_ = std::thread(
        &StaticLookPhysicalPreviewService::watchdog_loop, this);
}

StaticLookPhysicalPreviewService::~StaticLookPhysicalPreviewService() noexcept {
    {
        std::lock_guard lock(mutex_);
        shutting_down_ = true;
        if (status_.owns_runner) {
            stop_owned_locked(
                StaticLookPhysicalPreviewState::Stopped,
                StaticLookPhysicalPreviewError::None,
                StaticLookPhysicalPreviewStopReason::Destroyed);
        }
    }
    watchdog_condition_.notify_all();
    if (watchdog_.joinable()) {
        watchdog_.join();
    }
}

StaticLookPhysicalPreviewResult StaticLookPhysicalPreviewService::begin(
    const ProjectDocument& project,
    const StaticLookDraft& draft,
    std::string_view target_id,
    StaticLookPhysicalPreviewConfig config) {
    {
        std::lock_guard lock(mutex_);
        if (status_.owns_runner) {
            return error_result(StaticLookPhysicalPreviewError::AlreadyActive);
        }
        if (runner_.status().state != RunnerState::Stopped) {
            return error_result(StaticLookPhysicalPreviewError::LiveRunning);
        }
    }
    auto candidate = build_static_look_physical_preview_candidate(
        project, draft, target_id, config);
    if (!candidate) {
        return result_from_candidate(std::move(candidate));
    }
    {
        std::lock_guard lock(mutex_);
        if (status_.owns_runner) {
            return error_result(StaticLookPhysicalPreviewError::AlreadyActive);
        }
        if (runner_.status().state != RunnerState::Stopped) {
            return error_result(StaticLookPhysicalPreviewError::LiveRunning);
        }
        config_ = config;
    }
    return activate_candidate(std::move(candidate), true);
}

StaticLookPhysicalPreviewResult StaticLookPhysicalPreviewService::update(
    const ProjectDocument& project,
    const StaticLookDraft& draft,
    std::string_view target_id) {
    StaticLookPhysicalPreviewConfig config;
    {
        std::lock_guard lock(mutex_);
        if (!status_.owns_runner) {
            return error_result(StaticLookPhysicalPreviewError::NotActive);
        }
        config = config_;
    }
    auto candidate = build_static_look_physical_preview_candidate(
        project, draft, target_id, config);
    if (!candidate) {
        std::lock_guard lock(mutex_);
        if (status_.owns_runner) {
            stop_owned_locked(
                StaticLookPhysicalPreviewState::Fault,
                candidate.error,
                StaticLookPhysicalPreviewStopReason::RejectedUpdate);
        }
        return result_from_candidate(std::move(candidate));
    }
    return activate_candidate(std::move(candidate), false);
}

StaticLookPhysicalPreviewResult
StaticLookPhysicalPreviewService::activate_candidate(
    StaticLookPhysicalPreviewCandidate candidate,
    bool initial) {
    auto compilation = compile_project(candidate.project);
    if (!compilation || compilation.show == nullptr) {
        candidate.validation = std::move(compilation.validation);
        candidate.error = candidate.validation.ok()
            ? StaticLookPhysicalPreviewError::CompilationFailed
            : StaticLookPhysicalPreviewError::ValidationFailed;
        std::lock_guard lock(mutex_);
        if (!initial && status_.owns_runner) {
            stop_owned_locked(
                StaticLookPhysicalPreviewState::Fault,
                candidate.error,
                StaticLookPhysicalPreviewStopReason::RejectedUpdate);
        }
        return result_from_candidate(std::move(candidate));
    }

    std::unique_lock lock(mutex_);
    if (initial) {
        if (status_.owns_runner) {
            return error_result(StaticLookPhysicalPreviewError::AlreadyActive);
        }
        if (runner_.status().state != RunnerState::Stopped) {
            return error_result(StaticLookPhysicalPreviewError::LiveRunning);
        }
        status_ = {};
        status_.state = StaticLookPhysicalPreviewState::Starting;
        status_.owns_runner = true;
        status_.selected_fixture_count = candidate.selected_fixture_count;
        status_.output_cap = candidate.project.safety.max_intensity;
        preview_connections_ = candidate.project.connections;
        ++lease_generation_;
        if (!runner_.start(std::move(compilation.show), candidate.project)) {
            status_.owns_runner = false;
            status_.state = StaticLookPhysicalPreviewState::Fault;
            status_.error = StaticLookPhysicalPreviewError::RunnerStartFailed;
            return error_result(
                StaticLookPhysicalPreviewError::RunnerStartFailed);
        }
        runner_.set_blackout(true);
        if (!wait_for_runner_started(config_.runner_start_timeout_ms) ||
            !wait_for_outputs_ready(
                candidate.project.connections,
                config_.runner_start_timeout_ms)) {
            const auto readiness = output_readiness(
                candidate.project.connections, runner_.status());
            const auto error = runner_.status().state == RunnerState::Fault
                ? StaticLookPhysicalPreviewError::RunnerFault
                : StaticLookPhysicalPreviewError::OutputNotReady;
            stop_owned_locked(
                StaticLookPhysicalPreviewState::Fault,
                readiness == OutputReadiness::Fault
                    ? StaticLookPhysicalPreviewError::OutputNotReady
                    : error,
                StaticLookPhysicalPreviewStopReason::RunnerFault);
            return error_result(status_.error);
        }
    } else {
        if (!status_.owns_runner ||
            runner_.status().state != RunnerState::Running) {
            return error_result(StaticLookPhysicalPreviewError::NotActive);
        }
        if (RunnerService::monotonic_ms() >= status_.deadline_ms) {
            stop_owned_locked(
                StaticLookPhysicalPreviewState::TimedOut,
                StaticLookPhysicalPreviewError::TimedOut,
                StaticLookPhysicalPreviewStopReason::Timeout);
            return error_result(StaticLookPhysicalPreviewError::TimedOut);
        }
        status_.state = StaticLookPhysicalPreviewState::Updating;
        runner_.set_blackout(true);
        const auto activation = runner_.activate(
            std::move(compilation.show),
            candidate.project,
            config_.activation_timeout_ms);
        if (!activation) {
            stop_owned_locked(
                StaticLookPhysicalPreviewState::Fault,
                StaticLookPhysicalPreviewError::RunnerActivationFailed,
                StaticLookPhysicalPreviewStopReason::RejectedUpdate);
            return error_result(
                StaticLookPhysicalPreviewError::RunnerActivationFailed);
        }
    }

    const auto package_generation = runner_.status().package_generation;
    const StaticLookOwnerContext owner{
        StaticLookOwnerKind::Test,
        lease_generation_ == 0U ? 1U : lease_generation_,
        0U,
        0U};
    if (!runner_.trigger_look(0U, owner) ||
        !wait_for_look_activation(
            package_generation, config_.activation_timeout_ms)) {
        stop_owned_locked(
            StaticLookPhysicalPreviewState::Fault,
            StaticLookPhysicalPreviewError::LookTriggerFailed,
            initial
                ? StaticLookPhysicalPreviewStopReason::RunnerFault
                : StaticLookPhysicalPreviewStopReason::RejectedUpdate);
        return error_result(StaticLookPhysicalPreviewError::LookTriggerFailed);
    }

    runner_.set_blackout(false);
    const auto now = RunnerService::monotonic_ms();
    if (initial) {
        status_.started_at_ms = now;
        status_.deadline_ms = now + config_.timeout_ms;
        status_.update_count = 0U;
    } else {
        ++status_.update_count;
    }
    status_.state = StaticLookPhysicalPreviewState::Active;
    status_.error = StaticLookPhysicalPreviewError::None;
    status_.stop_reason = StaticLookPhysicalPreviewStopReason::None;
    status_.selected_fixture_count = candidate.selected_fixture_count;
    watchdog_condition_.notify_all();

    StaticLookPhysicalPreviewResult result;
    result.validation = std::move(candidate.validation);
    result.warnings = std::move(candidate.warnings);
    result.selected_fixture_count = candidate.selected_fixture_count;
    result.deadline_ms = status_.deadline_ms;
    return result;
}

bool StaticLookPhysicalPreviewService::wait_for_runner_started(
    std::uint32_t timeout_ms) const noexcept {
    const auto deadline = Clock::now() + std::chrono::milliseconds(timeout_ms);
    while (Clock::now() < deadline) {
        const auto state = runner_.status().state;
        if (state == RunnerState::Running) {
            return true;
        }
        if (state == RunnerState::Fault || state == RunnerState::Stopped) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return runner_.status().state == RunnerState::Running;
}

bool StaticLookPhysicalPreviewService::wait_for_outputs_ready(
    const ConnectionSettings& connections,
    std::uint32_t timeout_ms) const noexcept {
    const auto deadline = Clock::now() + std::chrono::milliseconds(timeout_ms);
    while (Clock::now() < deadline) {
        const auto status = runner_.status();
        if (status.state != RunnerState::Running) {
            return false;
        }
        const auto readiness = output_readiness(connections, status);
        if (readiness == OutputReadiness::Ready) {
            return true;
        }
        if (readiness == OutputReadiness::Fault) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return output_readiness(connections, runner_.status()) ==
        OutputReadiness::Ready;
}

bool StaticLookPhysicalPreviewService::wait_for_look_activation(
    std::uint64_t package_generation,
    std::uint32_t timeout_ms) const noexcept {
    const auto deadline = Clock::now() + std::chrono::milliseconds(timeout_ms);
    while (Clock::now() < deadline) {
        const auto status = runner_.status();
        if (status.state != RunnerState::Running) {
            return false;
        }
        if (status.static_look.look_index == 0 &&
            status.static_look.package_generation == package_generation &&
            status.static_look.activation_generation != 0U) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
}

bool StaticLookPhysicalPreviewService::stop() noexcept {
    std::lock_guard lock(mutex_);
    if (!status_.owns_runner) {
        return false;
    }
    stop_owned_locked(
        StaticLookPhysicalPreviewState::Stopped,
        StaticLookPhysicalPreviewError::None,
        StaticLookPhysicalPreviewStopReason::Explicit);
    watchdog_condition_.notify_all();
    return true;
}

StaticLookPhysicalPreviewStatus
StaticLookPhysicalPreviewService::status() const noexcept {
    std::lock_guard lock(mutex_);
    auto snapshot = status_;
    const auto now = RunnerService::monotonic_ms();
    snapshot.remaining_ms = snapshot.owns_runner && snapshot.deadline_ms > now
        ? snapshot.deadline_ms - now
        : 0U;
    return snapshot;
}

bool StaticLookPhysicalPreviewService::enforce_deadline(
    std::uint64_t now_ms) noexcept {
    std::lock_guard lock(mutex_);
    if (!status_.owns_runner || status_.deadline_ms == 0U ||
        now_ms < status_.deadline_ms) {
        return false;
    }
    stop_owned_locked(
        StaticLookPhysicalPreviewState::TimedOut,
        StaticLookPhysicalPreviewError::TimedOut,
        StaticLookPhysicalPreviewStopReason::Timeout);
    watchdog_condition_.notify_all();
    return true;
}

void StaticLookPhysicalPreviewService::stop_owned_locked(
    StaticLookPhysicalPreviewState terminal_state,
    StaticLookPhysicalPreviewError error,
    StaticLookPhysicalPreviewStopReason reason) noexcept {
    if (!status_.owns_runner) {
        status_.state = terminal_state;
        status_.error = error;
        status_.stop_reason = reason;
        return;
    }
    status_.state = StaticLookPhysicalPreviewState::Stopping;
    runner_.set_blackout(true);
    runner_.stop();
    status_.owns_runner = false;
    status_.state = terminal_state;
    status_.error = error;
    status_.stop_reason = reason;
    status_.remaining_ms = 0U;
}

void StaticLookPhysicalPreviewService::watchdog_loop() noexcept {
    std::unique_lock lock(mutex_);
    while (!shutting_down_) {
        if (!status_.owns_runner ||
            status_.state != StaticLookPhysicalPreviewState::Active) {
            watchdog_condition_.wait(lock, [&] {
                return shutting_down_ ||
                    (status_.owns_runner &&
                     status_.state == StaticLookPhysicalPreviewState::Active);
            });
            continue;
        }
        const auto observed_lease = lease_generation_;
        watchdog_condition_.wait_for(
            lock,
            std::chrono::milliseconds(25),
            [&] {
                return shutting_down_ || !status_.owns_runner ||
                    status_.state != StaticLookPhysicalPreviewState::Active ||
                    lease_generation_ != observed_lease;
            });
        if (shutting_down_ || !status_.owns_runner ||
            status_.state != StaticLookPhysicalPreviewState::Active) {
            continue;
        }
        const auto now = RunnerService::monotonic_ms();
        if (now >= status_.deadline_ms) {
            stop_owned_locked(
                StaticLookPhysicalPreviewState::TimedOut,
                StaticLookPhysicalPreviewError::TimedOut,
                StaticLookPhysicalPreviewStopReason::Timeout);
            continue;
        }
        const auto runner_status = runner_.status();
        if (runner_status.state != RunnerState::Running ||
            output_readiness(preview_connections_, runner_status) !=
                OutputReadiness::Ready) {
            stop_owned_locked(
                StaticLookPhysicalPreviewState::Fault,
                StaticLookPhysicalPreviewError::RunnerFault,
                StaticLookPhysicalPreviewStopReason::RunnerFault);
        }
    }
}

const char* static_look_physical_preview_error_name(
    StaticLookPhysicalPreviewError error) noexcept {
    switch (error) {
    case StaticLookPhysicalPreviewError::None: return "none";
    case StaticLookPhysicalPreviewError::InvalidConfiguration: return "invalid-configuration";
    case StaticLookPhysicalPreviewError::LiveRunning: return "live-running";
    case StaticLookPhysicalPreviewError::AlreadyActive: return "already-active";
    case StaticLookPhysicalPreviewError::NotActive: return "not-active";
    case StaticLookPhysicalPreviewError::NoOutputConfigured: return "no-output-configured";
    case StaticLookPhysicalPreviewError::TargetNotFound: return "target-not-found";
    case StaticLookPhysicalPreviewError::EmptyTarget: return "empty-target";
    case StaticLookPhysicalPreviewError::EmptyLook: return "empty-look";
    case StaticLookPhysicalPreviewError::UnsafeAssignment: return "unsafe-assignment";
    case StaticLookPhysicalPreviewError::UnsafeProfile: return "unsafe-profile";
    case StaticLookPhysicalPreviewError::ValidationFailed: return "validation-failed";
    case StaticLookPhysicalPreviewError::CompilationFailed: return "compilation-failed";
    case StaticLookPhysicalPreviewError::RunnerStartFailed: return "runner-start-failed";
    case StaticLookPhysicalPreviewError::OutputNotReady: return "output-not-ready";
    case StaticLookPhysicalPreviewError::RunnerActivationFailed: return "runner-activation-failed";
    case StaticLookPhysicalPreviewError::LookTriggerFailed: return "look-trigger-failed";
    case StaticLookPhysicalPreviewError::TimedOut: return "timed-out";
    case StaticLookPhysicalPreviewError::RunnerFault: return "runner-fault";
    }
    return "unknown";
}

}  // namespace emberlights
