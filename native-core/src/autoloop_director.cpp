#include "showcore/autoloop_director.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace showcore {
namespace {

[[nodiscard]] constexpr std::size_t address_index(
    AutoloopAddress address) noexcept {
    return static_cast<std::size_t>(address.bank) * kAutoloopsPerBank +
        static_cast<std::size_t>(address.slot);
}

[[nodiscard]] constexpr AutoloopAddress address_from_index(
    std::size_t index) noexcept {
    return {
        static_cast<std::uint16_t>(index / kAutoloopsPerBank),
        static_cast<std::uint8_t>(index % kAutoloopsPerBank)};
}

[[nodiscard]] constexpr std::uint64_t elapsed_ticks(
    std::int64_t tick,
    std::int64_t start_tick) noexcept {
    if (tick <= start_tick) {
        return 0U;
    }
    // Unsigned subtraction is well-defined and gives the exact non-negative
    // distance even when the signed endpoints straddle zero.
    return static_cast<std::uint64_t>(tick) -
        static_cast<std::uint64_t>(start_tick);
}

[[nodiscard]] constexpr std::uint32_t bounded_cycle_count(
    std::uint64_t elapsed,
    std::uint64_t length) noexcept {
    if (length == 0U) {
        return 0U;
    }
    return static_cast<std::uint32_t>(std::min(
        elapsed / length,
        static_cast<std::uint64_t>(
            std::numeric_limits<std::uint32_t>::max())));
}

[[nodiscard]] constexpr std::int16_t exclusive_bank(
    std::uint64_t mask) noexcept {
    return std::has_single_bit(mask)
        ? static_cast<std::int16_t>(std::countr_zero(mask))
        : static_cast<std::int16_t>(-1);
}

void update_progress(
    AutoloopSessionStatus& session,
    std::uint64_t elapsed,
    std::uint64_t length) noexcept {
    const auto phase = length == 0U ? 0U : elapsed % length;
    session.phase_tick = static_cast<std::int64_t>(phase);
    session.progress = length == 0U
        ? 0.0F
        : static_cast<float>(
              static_cast<long double>(phase) /
              static_cast<long double>(length));
    session.completed_cycles = bounded_cycle_count(elapsed, length);
}

[[nodiscard]] constexpr LayerId layer_for(
    AutoloopDirectorSource source) noexcept {
    switch (source) {
    case AutoloopDirectorSource::Autonomous: return LayerId::Autonomous;
    case AutoloopDirectorSource::Scripted: return LayerId::TrackScript;
    case AutoloopDirectorSource::Manual: return LayerId::ManualAutoloop;
    case AutoloopDirectorSource::None:
    case AutoloopDirectorSource::Count:
        break;
    }
    return LayerId::Count;
}

}  // namespace

AutoloopDirector::AutoloopDirector(AutoloopDirectorConfig config) noexcept {
    if (config.selection_policy >= AutoloopSelectionPolicy::Count) {
        config.selection_policy = AutoloopSelectionPolicy::Sequential;
    }
    status_.selection_policy = config.selection_policy;
    status_.deterministic_seed = config.deterministic_seed;
    status_.active_exclusive_bank = exclusive_bank(status_.active_bank_mask);
}

AutoloopDirectorResult AutoloopDirector::activate_package(
    const CompiledAutoloopPackage* package,
    std::uint64_t generation,
    LayerStack& layers) noexcept {
    if (last_activation_generation_ != 0U &&
        generation <= last_activation_generation_) {
        status_.last_result = AutoloopDirectorResult::StaleGeneration;
        return status_.last_result;
    }

    clear_sessions_and_layers(layers);
    if (generation != 0U) {
        last_activation_generation_ = generation;
    }
    package_ = package;
    status_.package_generation = generation;
    status_.active_bank_mask = ~std::uint64_t{0U};
    status_.has_pending_bank_mask = false;
    status_.pending_bank_mask = 0U;
    status_.active_exclusive_bank = -1;
    status_.pending_exclusive_bank = -1;
    status_.fault = AutoloopDirectorFault::None;
    status_.track_boundary_available = false;
    status_.selected = {};
    status_.queued = {};
    status_.active = {};
    status_.queued_reason = AutoloopQueuedReason::None;
    autonomous_cursor_ = {};
    has_effective_tick_ = false;
    last_effective_tick_ = 0;

    if (package == nullptr || generation == 0U) {
        package_ = nullptr;
        status_.package_active = false;
        status_.fault = AutoloopDirectorFault::PackageUnavailable;
        record_transition(
            AutoloopDirectorResult::PackageUnavailable,
            AutoloopTransitionReason::PackageActivation);
        refresh_aggregate_status();
        return status_.last_result;
    }

    status_.package_active = true;
    record_transition(
        AutoloopDirectorResult::PackageActivated,
        AutoloopTransitionReason::PackageActivation);
    refresh_aggregate_status();
    return status_.last_result;
}

AutoloopDirectorResult AutoloopDirector::clear_package(
    LayerStack& layers) noexcept {
    clear_sessions_and_layers(layers);
    package_ = nullptr;
    status_.package_active = false;
    status_.package_generation = 0U;
    status_.fault = AutoloopDirectorFault::None;
    status_.has_pending_bank_mask = false;
    status_.pending_bank_mask = 0U;
    status_.pending_exclusive_bank = -1;
    status_.selected = {};
    status_.queued = {};
    status_.active = {};
    status_.queued_reason = AutoloopQueuedReason::None;
    autonomous_cursor_ = {};
    has_effective_tick_ = false;
    record_transition(
        AutoloopDirectorResult::PackageCleared,
        AutoloopTransitionReason::PackageActivation);
    refresh_aggregate_status();
    return status_.last_result;
}

bool AutoloopDirector::command_generation_matches(
    std::uint64_t generation) noexcept {
    if (package_ == nullptr) {
        status_.last_result = AutoloopDirectorResult::PackageUnavailable;
        return false;
    }
    if (status_.fault != AutoloopDirectorFault::None) {
        status_.last_result = AutoloopDirectorResult::Faulted;
        return false;
    }
    if (generation != status_.package_generation) {
        status_.last_result = AutoloopDirectorResult::StaleGeneration;
        return false;
    }
    return true;
}

AutoloopDirectorResult AutoloopDirector::request_bank_mask(
    std::uint64_t mask,
    std::uint64_t expected_generation) noexcept {
    if (!command_generation_matches(expected_generation)) {
        return status_.last_result;
    }

    if (status_.autonomous.active) {
        status_.has_pending_bank_mask = true;
        status_.pending_bank_mask = mask;
        status_.pending_exclusive_bank = exclusive_bank(mask);
        status_.queued_reason = AutoloopQueuedReason::BankMaskBoundary;
        refresh_queued_selection();
        record_transition(
            AutoloopDirectorResult::BankMaskPending,
            AutoloopTransitionReason::PendingBankBoundary);
        refresh_aggregate_status();
        return status_.last_result;
    }

    status_.active_bank_mask = mask;
    status_.active_exclusive_bank = exclusive_bank(mask);
    status_.has_pending_bank_mask = false;
    status_.pending_bank_mask = 0U;
    status_.pending_exclusive_bank = -1;
    status_.queued = {};
    status_.queued_reason = AutoloopQueuedReason::None;
    record_transition(
        AutoloopDirectorResult::BankMaskApplied,
        AutoloopTransitionReason::PendingBankBoundary);
    refresh_aggregate_status();
    return status_.last_result;
}

AutoloopDirectorResult AutoloopDirector::request_all_banks(
    std::uint64_t expected_generation) noexcept {
    return request_bank_mask(~std::uint64_t{0U}, expected_generation);
}

AutoloopDirectorResult AutoloopDirector::request_exclusive_bank(
    std::uint16_t bank,
    std::uint64_t expected_generation) noexcept {
    if (bank >= kMaxAutoloopBanks) {
        status_.last_result = AutoloopDirectorResult::InvalidAddress;
        return status_.last_result;
    }
    return request_bank_mask(
        std::uint64_t{1U} << bank, expected_generation);
}

AutoloopDirectorResult AutoloopDirector::set_bank_enabled(
    std::uint16_t bank,
    bool enabled,
    std::uint64_t expected_generation) noexcept {
    if (bank >= kMaxAutoloopBanks) {
        status_.last_result = AutoloopDirectorResult::InvalidAddress;
        return status_.last_result;
    }
    if (!command_generation_matches(expected_generation)) {
        return status_.last_result;
    }
    auto mask = status_.has_pending_bank_mask
        ? status_.pending_bank_mask
        : status_.active_bank_mask;
    const auto bit = std::uint64_t{1U} << bank;
    mask = enabled ? mask | bit : mask & ~bit;
    return request_bank_mask(mask, expected_generation);
}

AutoloopSelectionStatus AutoloopDirector::selection_for(
    AutoloopAddress address) const noexcept {
    if (package_ == nullptr || !address.valid()) {
        return {};
    }
    const auto* placement = package_->placement(address);
    if (placement == nullptr || !placement->populated()) {
        return {};
    }
    const auto* program = package_->program(placement->program_index);
    if (program == nullptr || program->length_ticks <= 0) {
        return {};
    }
    return {
        true,
        address,
        placement->program_index,
        placement->asset_key};
}

AutoloopSelectionStatus AutoloopDirector::next_eligible(
    AutoloopAddress after,
    std::uint64_t bank_mask) const noexcept {
    if (package_ == nullptr || bank_mask == 0U) {
        return {};
    }
    const auto start = after.valid()
        ? address_index(after)
        : kMaxAutoloops - 1U;
    for (std::size_t offset = 1U; offset <= kMaxAutoloops; ++offset) {
        const auto index = (start + offset) % kMaxAutoloops;
        const auto address = address_from_index(index);
        const auto enabled =
            (bank_mask & (std::uint64_t{1U} << address.bank)) != 0U;
        if (!enabled) {
            continue;
        }
        const auto selection = selection_for(address);
        if (selection.valid) {
            return selection;
        }
    }
    return {};
}

std::int64_t AutoloopDirector::effective_tick(
    const AutoloopTransportState& transport) noexcept {
    if (!has_effective_tick_) {
        last_effective_tick_ = transport.musical_tick;
        has_effective_tick_ = true;
        return last_effective_tick_;
    }
    if (transport.phase_available &&
        (transport.running || transport.discontinuity)) {
        last_effective_tick_ = transport.musical_tick;
    }
    return last_effective_tick_;
}

AutoloopDirectorResult AutoloopDirector::launch_session(
    AutoloopSessionStatus& session,
    AutoloopDirectorSource source,
    const AutoloopLaunchRequest& request,
    const AutoloopTransportState& transport,
    LayerStack& layers) noexcept {
    if (!command_generation_matches(request.expected_package_generation)) {
        return status_.last_result;
    }
    if (!request.address.valid()) {
        status_.last_result = AutoloopDirectorResult::InvalidAddress;
        return status_.last_result;
    }
    const auto* placement = package_->placement(request.address);
    if (placement == nullptr || !placement->populated()) {
        status_.last_result = AutoloopDirectorResult::EmptyPlacement;
        return status_.last_result;
    }
    const auto selection = selection_for(request.address);
    if (!selection.valid) {
        status_.last_result = AutoloopDirectorResult::InvalidCompiledProgram;
        return status_.last_result;
    }
    if (placement->launch != CompiledAutoloopLaunchQuantization::Immediate ||
        placement->phase_origin != CompiledAutoloopPhaseOrigin::Launch ||
        placement->return_fade_ticks != 0) {
        status_.last_result =
            AutoloopDirectorResult::UnsupportedLaunchProfile;
        return status_.last_result;
    }

    const auto repeat = request.override_repeat_and_mode
        ? request.repeat
        : placement->repeat;
    const auto mode = request.override_repeat_and_mode
        ? request.mode
        : placement->mode;
    if (repeat > AutoloopRepeat::TrackDuration ||
        mode >= CompiledAutoloopPlaybackMode::Count) {
        status_.last_result =
            AutoloopDirectorResult::UnsupportedLaunchProfile;
        return status_.last_result;
    }
    if (placement->track_boundary_required ||
        repeat == AutoloopRepeat::TrackDuration) {
        if (!transport.track_boundary_available) {
            status_.last_result =
                AutoloopDirectorResult::TrackBoundaryUnavailable;
            return status_.last_result;
        }
        if (!transport.track_active) {
            status_.last_result = AutoloopDirectorResult::TrackInactive;
            return status_.last_result;
        }
    }

    const auto tick_value = effective_tick(transport);
    clear_session(session, layer_for(source), layers);
    session.active = true;
    session.source = source;
    session.selection = selection;
    session.repeat = repeat;
    session.mode = mode;
    session.launch = placement->launch;
    session.phase_origin = placement->phase_origin;
    session.start_tick = tick_value;
    session.phase_tick = 0;
    session.progress = 0.0F;
    session.completed_cycles = 0U;
    session.package_generation = status_.package_generation;
    session.track_epoch = transport.track_epoch;
    status_.track_boundary_available = transport.track_boundary_available;

    const auto result = source == AutoloopDirectorSource::Manual
        ? AutoloopDirectorResult::ManualStarted
        : AutoloopDirectorResult::ScriptedStarted;
    const auto reason = source == AutoloopDirectorSource::Manual
        ? AutoloopTransitionReason::DirectManual
        : AutoloopTransitionReason::DirectScripted;
    record_transition(result, reason);
    if (!render_owned_layers(layers)) {
        return fault(AutoloopDirectorFault::EvaluationFailed, layers);
    }
    refresh_aggregate_status();
    return status_.last_result;
}

AutoloopDirectorResult AutoloopDirector::launch_scripted(
    const AutoloopLaunchRequest& request,
    const AutoloopTransportState& transport,
    LayerStack& layers) noexcept {
    return launch_session(
        status_.scripted,
        AutoloopDirectorSource::Scripted,
        request,
        transport,
        layers);
}

AutoloopDirectorResult AutoloopDirector::clear_scripted(
    std::uint64_t expected_generation,
    LayerStack& layers) noexcept {
    if (!command_generation_matches(expected_generation)) {
        return status_.last_result;
    }
    clear_session(status_.scripted, LayerId::TrackScript, layers);
    record_transition(
        AutoloopDirectorResult::ScriptedCleared,
        AutoloopTransitionReason::DirectScripted);
    if (!render_owned_layers(layers)) {
        return fault(AutoloopDirectorFault::EvaluationFailed, layers);
    }
    refresh_aggregate_status();
    return status_.last_result;
}

AutoloopDirectorResult AutoloopDirector::launch_manual(
    const AutoloopLaunchRequest& request,
    const AutoloopTransportState& transport,
    LayerStack& layers) noexcept {
    return launch_session(
        status_.manual,
        AutoloopDirectorSource::Manual,
        request,
        transport,
        layers);
}

AutoloopDirectorResult AutoloopDirector::clear_manual(
    std::uint64_t expected_generation,
    LayerStack& layers) noexcept {
    if (!command_generation_matches(expected_generation)) {
        return status_.last_result;
    }
    clear_session(status_.manual, LayerId::ManualAutoloop, layers);
    record_transition(
        AutoloopDirectorResult::ManualCleared,
        AutoloopTransitionReason::DirectManual);
    if (!render_owned_layers(layers)) {
        return fault(AutoloopDirectorFault::EvaluationFailed, layers);
    }
    refresh_aggregate_status();
    return status_.last_result;
}

bool AutoloopDirector::update_session(
    AutoloopSessionStatus& session,
    std::int64_t tick_value,
    const AutoloopTransportState& transport,
    AutoloopDirectorResult completion_result,
    LayerId layer,
    LayerStack& layers) noexcept {
    if (!session.active) {
        return true;
    }
    const auto* program = package_ == nullptr
        ? nullptr
        : package_->program(session.selection.program_index);
    if (program == nullptr || program->length_ticks <= 0 ||
        session.package_generation != status_.package_generation) {
        return false;
    }

    if (session.repeat == AutoloopRepeat::TrackDuration &&
        (!transport.track_boundary_available || !transport.track_active ||
         transport.track_epoch != session.track_epoch)) {
        clear_session(session, layer, layers);
        record_transition(
            AutoloopDirectorResult::TrackBoundaryReleased,
            AutoloopTransitionReason::TrackBoundary);
        return true;
    }

    const auto length = static_cast<std::uint64_t>(program->length_ticks);
    const auto elapsed = elapsed_ticks(tick_value, session.start_tick);
    if (session.repeat == AutoloopRepeat::Once && elapsed >= length) {
        clear_session(session, layer, layers);
        record_transition(
            completion_result,
            AutoloopTransitionReason::NaturalBoundary);
        return true;
    }
    update_progress(session, elapsed, length);
    return true;
}

bool AutoloopDirector::evaluate_session(
    const AutoloopSessionStatus& session,
    LayerBuffer& output) noexcept {
    output.clear();
    if (!session.active) {
        return true;
    }
    if (package_ == nullptr ||
        session.package_generation != status_.package_generation ||
        !session.selection.valid) {
        return false;
    }
    return evaluator_.evaluate(
        *package_,
        session.selection.program_index,
        session.phase_tick,
        output);
}

bool AutoloopDirector::render_owned_layers(LayerStack& layers) noexcept {
    if (!evaluate_session(status_.autonomous, autonomous_output_) ||
        !evaluate_session(status_.scripted, scripted_output_) ||
        !evaluate_session(status_.manual, manual_output_)) {
        return false;
    }

    const auto manual_replaces = status_.manual.active &&
        status_.manual.mode == CompiledAutoloopPlaybackMode::Replace;
    const auto scripted_replaces = status_.scripted.active &&
        status_.scripted.mode == CompiledAutoloopPlaybackMode::Replace;

    if (manual_replaces || scripted_replaces ||
        !status_.autonomous.active) {
        layers.clear_layer(LayerId::Autonomous);
    } else {
        layers.replace_layer(LayerId::Autonomous, autonomous_output_);
    }

    if (manual_replaces || !status_.scripted.active) {
        layers.clear_layer(LayerId::TrackScript);
    } else {
        layers.replace_layer(LayerId::TrackScript, scripted_output_);
    }

    if (status_.manual.active) {
        layers.replace_layer(LayerId::ManualAutoloop, manual_output_);
    } else {
        layers.clear_layer(LayerId::ManualAutoloop);
    }
    return true;
}

void AutoloopDirector::rebase_sessions(std::int64_t tick_value) noexcept {
    auto rebase = [tick_value](AutoloopSessionStatus& session) {
        if (!session.active) {
            return;
        }
        session.start_tick = tick_value;
        session.phase_tick = 0;
        session.progress = 0.0F;
        session.completed_cycles = 0U;
    };
    rebase(status_.autonomous);
    rebase(status_.scripted);
    rebase(status_.manual);
}

void AutoloopDirector::clear_session(
    AutoloopSessionStatus& session,
    LayerId layer,
    LayerStack& layers) noexcept {
    session = {};
    if (layer != LayerId::Count) {
        layers.clear_layer(layer);
    }
}

void AutoloopDirector::clear_sessions_and_layers(
    LayerStack& layers) noexcept {
    clear_session(status_.autonomous, LayerId::Autonomous, layers);
    clear_session(status_.scripted, LayerId::TrackScript, layers);
    clear_session(status_.manual, LayerId::ManualAutoloop, layers);
    autonomous_output_.clear();
    scripted_output_.clear();
    manual_output_.clear();
}

void AutoloopDirector::apply_pending_bank_mask() noexcept {
    if (!status_.has_pending_bank_mask) {
        return;
    }
    status_.active_bank_mask = status_.pending_bank_mask;
    status_.active_exclusive_bank = exclusive_bank(status_.active_bank_mask);
    status_.has_pending_bank_mask = false;
    status_.pending_bank_mask = 0U;
    status_.pending_exclusive_bank = -1;
    status_.queued = {};
    status_.queued_reason = AutoloopQueuedReason::None;
}

void AutoloopDirector::refresh_queued_selection() noexcept {
    status_.queued = status_.has_pending_bank_mask
        ? next_eligible(autonomous_cursor_, status_.pending_bank_mask)
        : AutoloopSelectionStatus{};
}

void AutoloopDirector::refresh_aggregate_status() noexcept {
    status_.package_active = package_ != nullptr &&
        status_.fault == AutoloopDirectorFault::None;
    status_.selected = selection_for(autonomous_cursor_);
    status_.active = {};
    status_.active_source = AutoloopDirectorSource::None;
    status_.active_mode = CompiledAutoloopPlaybackMode::Overlay;
    status_.active_repeat = AutoloopRepeat::Once;
    status_.active_phase_tick = 0;
    status_.active_progress = 0.0F;
    status_.active_completed_cycles = 0U;

    const AutoloopSessionStatus* visible = nullptr;
    if (status_.manual.active) {
        visible = &status_.manual;
    } else if (status_.scripted.active) {
        visible = &status_.scripted;
    } else if (status_.autonomous.active) {
        visible = &status_.autonomous;
    }
    if (visible != nullptr) {
        status_.active = visible->selection;
        status_.active_source = visible->source;
        status_.active_mode = visible->mode;
        status_.active_repeat = visible->repeat;
        status_.active_phase_tick = visible->phase_tick;
        status_.active_progress = visible->progress;
        status_.active_completed_cycles = visible->completed_cycles;
    }
}

void AutoloopDirector::record_transition(
    AutoloopDirectorResult result,
    AutoloopTransitionReason reason) noexcept {
    status_.last_result = result;
    status_.transition_reason = reason;
    if (result != AutoloopDirectorResult::None) {
        ++status_.transition_count;
    }
}

AutoloopDirectorResult AutoloopDirector::tick(
    const AutoloopTransportState& transport,
    LayerStack& layers) noexcept {
    status_.track_boundary_available = transport.track_boundary_available;
    if (package_ == nullptr) {
        clear_sessions_and_layers(layers);
        status_.last_result = AutoloopDirectorResult::PackageUnavailable;
        refresh_aggregate_status();
        return status_.last_result;
    }
    if (status_.fault != AutoloopDirectorFault::None) {
        clear_sessions_and_layers(layers);
        status_.last_result = AutoloopDirectorResult::Faulted;
        refresh_aggregate_status();
        return status_.last_result;
    }

    const auto tick_value = effective_tick(transport);
    if (transport.discontinuity) {
        rebase_sessions(tick_value);
        record_transition(
            AutoloopDirectorResult::TransportRebased,
            AutoloopTransitionReason::TransportDiscontinuity);
    }

    if (!transport.autonomous_eligible && status_.autonomous.active) {
        clear_session(status_.autonomous, LayerId::Autonomous, layers);
        record_transition(
            AutoloopDirectorResult::AutonomousStopped,
            AutoloopTransitionReason::NaturalBoundary);
    }

    if (status_.autonomous.active) {
        const auto* program = package_->program(
            status_.autonomous.selection.program_index);
        if (program == nullptr || program->length_ticks <= 0 ||
            status_.autonomous.package_generation !=
                status_.package_generation) {
            return fault(
                AutoloopDirectorFault::InvalidCompiledProgram, layers);
        }
        const auto length = static_cast<std::uint64_t>(program->length_ticks);
        const auto elapsed = elapsed_ticks(
            tick_value, status_.autonomous.start_tick);
        if (elapsed >= length) {
            const auto had_pending_mask = status_.has_pending_bank_mask;
            apply_pending_bank_mask();
            const auto next = next_eligible(
                autonomous_cursor_, status_.active_bank_mask);
            clear_session(status_.autonomous, LayerId::Autonomous, layers);
            if (!next.valid) {
                autonomous_cursor_ = {};
                record_transition(
                    AutoloopDirectorResult::NoEligiblePlacement,
                    had_pending_mask
                        ? AutoloopTransitionReason::PendingBankBoundary
                        : AutoloopTransitionReason::NaturalBoundary);
            } else {
                autonomous_cursor_ = next.address;
                status_.autonomous.active = true;
                status_.autonomous.source =
                    AutoloopDirectorSource::Autonomous;
                status_.autonomous.selection = next;
                status_.autonomous.repeat = AutoloopRepeat::Once;
                status_.autonomous.mode =
                    CompiledAutoloopPlaybackMode::Overlay;
                status_.autonomous.start_tick = tick_value;
                status_.autonomous.package_generation =
                    status_.package_generation;
                record_transition(
                    AutoloopDirectorResult::AutonomousAdvanced,
                    had_pending_mask
                        ? AutoloopTransitionReason::PendingBankBoundary
                        : AutoloopTransitionReason::NaturalBoundary);
            }
        } else {
            update_progress(status_.autonomous, elapsed, length);
        }
    }

    if (!status_.autonomous.active && transport.autonomous_eligible) {
        apply_pending_bank_mask();
        const auto next = next_eligible(
            autonomous_cursor_, status_.active_bank_mask);
        if (next.valid) {
            autonomous_cursor_ = next.address;
            status_.autonomous.active = true;
            status_.autonomous.source = AutoloopDirectorSource::Autonomous;
            status_.autonomous.selection = next;
            status_.autonomous.repeat = AutoloopRepeat::Once;
            status_.autonomous.mode = CompiledAutoloopPlaybackMode::Overlay;
            status_.autonomous.start_tick = tick_value;
            status_.autonomous.package_generation = status_.package_generation;
            record_transition(
                AutoloopDirectorResult::AutonomousStarted,
                AutoloopTransitionReason::InitialFallback);
        } else if (status_.last_result !=
                   AutoloopDirectorResult::NoEligiblePlacement) {
            autonomous_cursor_ = {};
            record_transition(
                AutoloopDirectorResult::NoEligiblePlacement,
                AutoloopTransitionReason::InitialFallback);
        }
    }

    if (!update_session(
            status_.scripted,
            tick_value,
            transport,
            AutoloopDirectorResult::ScriptedCompleted,
            LayerId::TrackScript,
            layers) ||
        !update_session(
            status_.manual,
            tick_value,
            transport,
            AutoloopDirectorResult::ManualCompleted,
            LayerId::ManualAutoloop,
            layers)) {
        return fault(AutoloopDirectorFault::InvalidCompiledProgram, layers);
    }

    refresh_queued_selection();
    if (!render_owned_layers(layers)) {
        return fault(AutoloopDirectorFault::EvaluationFailed, layers);
    }
    refresh_aggregate_status();
    return status_.last_result;
}

AutoloopDirectorResult AutoloopDirector::fault(
    AutoloopDirectorFault reason,
    LayerStack& layers) noexcept {
    if (reason == AutoloopDirectorFault::None ||
        reason >= AutoloopDirectorFault::Count) {
        reason = AutoloopDirectorFault::ExternalFault;
    }
    clear_sessions_and_layers(layers);
    status_.fault = reason;
    status_.package_active = false;
    status_.has_pending_bank_mask = false;
    status_.pending_bank_mask = 0U;
    status_.pending_exclusive_bank = -1;
    status_.queued = {};
    status_.queued_reason = AutoloopQueuedReason::None;
    autonomous_cursor_ = {};
    record_transition(
        reason == AutoloopDirectorFault::EvaluationFailed
            ? AutoloopDirectorResult::EvaluationFailed
            : AutoloopDirectorResult::Faulted,
        AutoloopTransitionReason::Fault);
    refresh_aggregate_status();
    return status_.last_result;
}

const char* autoloop_director_result_name(
    AutoloopDirectorResult result) noexcept {
    switch (result) {
    case AutoloopDirectorResult::None: return "none";
    case AutoloopDirectorResult::PackageActivated: return "packageActivated";
    case AutoloopDirectorResult::PackageCleared: return "packageCleared";
    case AutoloopDirectorResult::BankMaskApplied: return "bankMaskApplied";
    case AutoloopDirectorResult::BankMaskPending: return "bankMaskPending";
    case AutoloopDirectorResult::AutonomousStarted: return "autonomousStarted";
    case AutoloopDirectorResult::AutonomousAdvanced: return "autonomousAdvanced";
    case AutoloopDirectorResult::AutonomousStopped: return "autonomousStopped";
    case AutoloopDirectorResult::ScriptedStarted: return "scriptedStarted";
    case AutoloopDirectorResult::ScriptedCompleted: return "scriptedCompleted";
    case AutoloopDirectorResult::ScriptedCleared: return "scriptedCleared";
    case AutoloopDirectorResult::ManualStarted: return "manualStarted";
    case AutoloopDirectorResult::ManualCompleted: return "manualCompleted";
    case AutoloopDirectorResult::ManualCleared: return "manualCleared";
    case AutoloopDirectorResult::TrackBoundaryReleased:
        return "trackBoundaryReleased";
    case AutoloopDirectorResult::TransportRebased: return "transportRebased";
    case AutoloopDirectorResult::NoEligiblePlacement:
        return "noEligiblePlacement";
    case AutoloopDirectorResult::StaleGeneration: return "staleGeneration";
    case AutoloopDirectorResult::PackageUnavailable:
        return "packageUnavailable";
    case AutoloopDirectorResult::InvalidAddress: return "invalidAddress";
    case AutoloopDirectorResult::EmptyPlacement: return "emptyPlacement";
    case AutoloopDirectorResult::InvalidCompiledProgram:
        return "invalidCompiledProgram";
    case AutoloopDirectorResult::UnsupportedLaunchProfile:
        return "unsupportedLaunchProfile";
    case AutoloopDirectorResult::TrackBoundaryUnavailable:
        return "trackBoundaryUnavailable";
    case AutoloopDirectorResult::TrackInactive: return "trackInactive";
    case AutoloopDirectorResult::EvaluationFailed: return "evaluationFailed";
    case AutoloopDirectorResult::Faulted: return "faulted";
    case AutoloopDirectorResult::Count:
        break;
    }
    return "invalid";
}

const char* autoloop_director_fault_name(
    AutoloopDirectorFault fault_value) noexcept {
    switch (fault_value) {
    case AutoloopDirectorFault::None: return "none";
    case AutoloopDirectorFault::PackageUnavailable:
        return "packageUnavailable";
    case AutoloopDirectorFault::InvalidCompiledProgram:
        return "invalidCompiledProgram";
    case AutoloopDirectorFault::EvaluationFailed: return "evaluationFailed";
    case AutoloopDirectorFault::ExternalFault: return "externalFault";
    case AutoloopDirectorFault::Count:
        break;
    }
    return "invalid";
}

}  // namespace showcore
