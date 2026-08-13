#include "emberlights/connection_coordinator.hpp"

#include <limits>
#include <utility>

namespace emberlights {

ConnectionTransaction::ConnectionTransaction(
    const ConnectionCoordinator* owner,
    ConnectionGeneration base_generation,
    std::uint64_t transaction_id,
    ConnectionSettings desired_settings)
    : owner_(owner),
      base_generation_(base_generation),
      transaction_id_(transaction_id),
      desired_settings_(std::move(desired_settings)) {}

ConnectionCoordinator::ConnectionCoordinator(
    ConnectionSettings initial_settings,
    ConnectionRuntimeState runtime_state)
    : desired_settings_(initial_settings),
      saved_settings_(initial_settings) {
    if (runtime_state == ConnectionRuntimeState::Running) {
        active_settings_ = std::move(initial_settings);
    }
}

ConnectionCoordinatorSnapshot ConnectionCoordinator::snapshot() const {
    return {
        desired_settings_,
        saved_settings_,
        active_settings_,
        generation_,
        diff_connection_endpoints(desired_settings_, saved_settings_),
        diff_runtime_connection_endpoints(saved_settings_, active_settings_),
        last_affected_endpoints_,
        last_result_,
        apply_transaction_id_ != 0U};
}

ConnectionPrepareOutcome ConnectionCoordinator::prepare(
    ConnectionGeneration expected_generation,
    ConnectionSettings desired_settings) {
    if (expected_generation != generation_) {
        return {ConnectionApplyResult::StaleGeneration, generation_, std::nullopt};
    }
    // A durable saved state must reach an explicit applied/failed boundary
    // before another proposal can begin. Otherwise a later validation/save
    // failure could orphan the older saved-but-inactive runtime transition.
    if (apply_transaction_id_ != 0U) {
        return {ConnectionApplyResult::InvalidTransition, generation_, std::nullopt};
    }
    // Reserve one generation for the save outcome, one for an observed
    // runtime-stopped boundary, and one for the final apply outcome. This
    // prevents a saved transaction from becoming impossible to resolve at the
    // numeric boundary.
    if (!can_advance_generation(3U) ||
        next_transaction_id_ == std::numeric_limits<std::uint64_t>::max()) {
        return {ConnectionApplyResult::GenerationLimit, generation_, std::nullopt};
    }

    const auto transaction_id = next_transaction_id_++;
    return {
        ConnectionApplyResult::None,
        generation_,
        ConnectionTransaction{
            this,
            generation_,
            transaction_id,
            std::move(desired_settings)}};
}

ConnectionMutationOutcome ConnectionCoordinator::reject_validation(
    const ConnectionTransaction& transaction) {
    if (!transaction_is_current(transaction)) {
        return reject_transition(
            transaction.owner_ == this
                ? ConnectionApplyResult::StaleGeneration
                : ConnectionApplyResult::InvalidTransition);
    }
    desired_settings_ = transaction.desired_settings_;
    clear_apply_ticket();
    return commit_result(
        ConnectionApplyResult::ValidationRejected,
        diff_connection_endpoints(desired_settings_, saved_settings_));
}

ConnectionMutationOutcome ConnectionCoordinator::acknowledge_save_failed(
    const ConnectionTransaction& transaction) {
    if (!transaction_is_current(transaction)) {
        return reject_transition(
            transaction.owner_ == this
                ? ConnectionApplyResult::StaleGeneration
                : ConnectionApplyResult::InvalidTransition);
    }
    desired_settings_ = transaction.desired_settings_;
    clear_apply_ticket();
    return commit_result(
        ConnectionApplyResult::SaveFailed,
        diff_connection_endpoints(desired_settings_, saved_settings_));
}

ConnectionMutationOutcome ConnectionCoordinator::acknowledge_saved(
    const ConnectionTransaction& transaction) {
    if (!transaction_is_current(transaction)) {
        return reject_transition(
            transaction.owner_ == this
                ? ConnectionApplyResult::StaleGeneration
                : ConnectionApplyResult::InvalidTransition);
    }

    desired_settings_ = transaction.desired_settings_;
    saved_settings_ = desired_settings_;
    const auto affected = active_settings_.has_value()
        ? diff_runtime_connection_endpoints(saved_settings_, active_settings_)
        : ConnectionEndpointMask{0U};
    const auto result = !active_settings_.has_value() || affected == 0U
        ? ConnectionApplyResult::SavedNoRuntimeChange
        : ConnectionApplyResult::SavedRestartRequired;
    // When every changed field belongs to a disabled endpoint, the live graph
    // is already semantically equivalent to the saved settings. Adopt the
    // inert metadata without forcing an unnecessary restart.
    if (active_settings_.has_value() && affected == 0U) {
        active_settings_ = saved_settings_;
    }
    clear_apply_ticket();
    const auto outcome = commit_result(result, affected);
    if (result == ConnectionApplyResult::SavedRestartRequired) {
        apply_transaction_id_ = transaction.transaction_id_;
        apply_generation_ = generation_;
        apply_previous_active_settings_ = active_settings_;
        apply_runtime_stopped_ = false;
    }
    return outcome;
}

ConnectionMutationOutcome
ConnectionCoordinator::acknowledge_apply_runtime_stopped(
    const ConnectionTransaction& transaction) {
    if (!apply_ticket_is_current(transaction)) {
        return reject_transition(
            transaction.owner_ == this
                ? ConnectionApplyResult::StaleGeneration
                : ConnectionApplyResult::InvalidTransition);
    }
    if (apply_runtime_stopped_ || !active_settings_.has_value()) {
        return reject_transition(ConnectionApplyResult::InvalidTransition);
    }
    const auto affected = diff_runtime_connection_endpoints(
        *active_settings_, std::nullopt);
    active_settings_.reset();
    const auto outcome =
        commit_result(ConnectionApplyResult::ApplyRuntimeStopped, affected);
    apply_generation_ = generation_;
    apply_runtime_stopped_ = true;
    return outcome;
}

ConnectionMutationOutcome ConnectionCoordinator::acknowledge_applied(
    const ConnectionTransaction& transaction) {
    if (!apply_ticket_is_current(transaction)) {
        return reject_transition(
            transaction.owner_ == this
                ? ConnectionApplyResult::StaleGeneration
                : ConnectionApplyResult::InvalidTransition);
    }
    if (!apply_runtime_stopped_ || active_settings_.has_value()) {
        return reject_transition(ConnectionApplyResult::InvalidTransition);
    }
    const auto affected =
        diff_runtime_connection_endpoints(saved_settings_, active_settings_);
    active_settings_ = saved_settings_;
    clear_apply_ticket();
    return commit_result(ConnectionApplyResult::SavedAndApplied, affected);
}

ConnectionMutationOutcome ConnectionCoordinator::acknowledge_apply_failed(
    const ConnectionTransaction& transaction,
    ConnectionApplyFailureBoundary failure_boundary) {
    if (!apply_ticket_is_current(transaction)) {
        return reject_transition(
            transaction.owner_ == this
                ? ConnectionApplyResult::StaleGeneration
                : ConnectionApplyResult::InvalidTransition);
    }
    if (failure_boundary != ConnectionApplyFailureBoundary::RuntimeStopped &&
        failure_boundary !=
            ConnectionApplyFailureBoundary::PreviousRuntimeRestored) {
        return reject_transition(ConnectionApplyResult::InvalidTransition);
    }
    if (!apply_runtime_stopped_ || active_settings_.has_value()) {
        return reject_transition(ConnectionApplyResult::InvalidTransition);
    }
    if (failure_boundary == ConnectionApplyFailureBoundary::RuntimeStopped) {
        active_settings_.reset();
    } else {
        active_settings_ = apply_previous_active_settings_;
    }
    const auto affected =
        diff_runtime_connection_endpoints(saved_settings_, active_settings_);
    clear_apply_ticket();
    return commit_result(ConnectionApplyResult::SavedApplyFailed, affected);
}

ConnectionMutationOutcome ConnectionCoordinator::acknowledge_runtime_started(
    ConnectionGeneration expected_generation) {
    if (expected_generation != generation_) {
        return reject_transition(ConnectionApplyResult::StaleGeneration);
    }
    if (active_settings_.has_value() || apply_transaction_id_ != 0U) {
        return reject_transition(ConnectionApplyResult::InvalidTransition);
    }
    if (!can_advance_generation()) {
        return reject_transition(ConnectionApplyResult::GenerationLimit);
    }
    const auto affected =
        diff_runtime_connection_endpoints(saved_settings_, std::nullopt);
    active_settings_ = saved_settings_;
    return commit_result(ConnectionApplyResult::RuntimeStarted, affected);
}

ConnectionMutationOutcome ConnectionCoordinator::acknowledge_runtime_stopped(
    ConnectionGeneration expected_generation) {
    if (expected_generation != generation_) {
        return reject_transition(ConnectionApplyResult::StaleGeneration);
    }
    if (!active_settings_.has_value() || apply_transaction_id_ != 0U) {
        return reject_transition(ConnectionApplyResult::InvalidTransition);
    }
    if (!can_advance_generation()) {
        return reject_transition(ConnectionApplyResult::GenerationLimit);
    }
    const auto affected = diff_runtime_connection_endpoints(
        *active_settings_, std::nullopt);
    active_settings_.reset();
    return commit_result(ConnectionApplyResult::RuntimeStopped, affected);
}

ConnectionMutationOutcome ConnectionCoordinator::replace_document_settings(
    ConnectionGeneration expected_generation,
    ConnectionSettings settings,
    ConnectionRuntimeState runtime_state) {
    if (expected_generation != generation_) {
        return reject_transition(ConnectionApplyResult::StaleGeneration);
    }
    if (runtime_state != ConnectionRuntimeState::Stopped) {
        return reject_transition(ConnectionApplyResult::InvalidTransition);
    }
    if (!can_advance_generation()) {
        return reject_transition(ConnectionApplyResult::GenerationLimit);
    }
    desired_settings_ = settings;
    saved_settings_ = settings;
    active_settings_.reset();
    clear_apply_ticket();
    return commit_result(ConnectionApplyResult::SavedNoRuntimeChange, 0U);
}

bool ConnectionCoordinator::transaction_is_current(
    const ConnectionTransaction& transaction) const noexcept {
    return transaction.owner_ == this &&
        transaction.base_generation_ == generation_ &&
        transaction.transaction_id_ != 0U;
}

bool ConnectionCoordinator::apply_ticket_is_current(
    const ConnectionTransaction& transaction) const noexcept {
    return transaction.owner_ == this &&
        transaction.transaction_id_ != 0U &&
        transaction.transaction_id_ == apply_transaction_id_ &&
        apply_generation_ == generation_ &&
        transaction.desired_settings_ == saved_settings_;
}

bool ConnectionCoordinator::can_advance_generation(
    ConnectionGeneration count) const noexcept {
    return count <=
        std::numeric_limits<ConnectionGeneration>::max() - generation_;
}

void ConnectionCoordinator::advance_generation() noexcept {
    ++generation_;
}

void ConnectionCoordinator::clear_apply_ticket() noexcept {
    apply_transaction_id_ = 0U;
    apply_generation_ = 0U;
    apply_previous_active_settings_.reset();
    apply_runtime_stopped_ = false;
}

ConnectionMutationOutcome ConnectionCoordinator::reject_transition(
    ConnectionApplyResult result) const noexcept {
    return {result, generation_, 0U};
}

ConnectionMutationOutcome ConnectionCoordinator::commit_result(
    ConnectionApplyResult result,
    ConnectionEndpointMask affected_endpoints) {
    if (!can_advance_generation()) {
        return reject_transition(ConnectionApplyResult::GenerationLimit);
    }
    advance_generation();
    last_result_ = result;
    last_affected_endpoints_ = affected_endpoints;
    return {result, generation_, affected_endpoints};
}

ConnectionEndpointMask diff_connection_endpoints(
    const ConnectionSettings& left,
    const ConnectionSettings& right) noexcept {
    ConnectionEndpointMask result = 0U;
    const auto changed = [&result](ConnectionEndpoint endpoint, bool differs) {
        if (differs) {
            result = static_cast<ConnectionEndpointMask>(
                result | connection_endpoint_bit(endpoint));
        }
    };

    changed(
        ConnectionEndpoint::Os2l,
        left.os2l_enabled != right.os2l_enabled ||
            left.os2l_bind != right.os2l_bind ||
            left.os2l_port != right.os2l_port);
    changed(
        ConnectionEndpoint::ArtNet,
        left.artnet_enabled != right.artnet_enabled ||
            left.artnet_destination != right.artnet_destination ||
            left.artnet_base != right.artnet_base);
    changed(
        ConnectionEndpoint::Sacn,
        left.sacn_enabled != right.sacn_enabled ||
            left.sacn_destination != right.sacn_destination ||
            left.sacn_universe_base != right.sacn_universe_base);
    changed(
        ConnectionEndpoint::DmxUsbProUniverse1,
        left.dmx_usb_pro_ports[0U] != right.dmx_usb_pro_ports[0U]);
    changed(
        ConnectionEndpoint::DmxUsbProUniverse2,
        left.dmx_usb_pro_ports[1U] != right.dmx_usb_pro_ports[1U]);
    changed(
        ConnectionEndpoint::SoundSwitchMicro,
        left.soundswitch_micro_universe != right.soundswitch_micro_universe ||
            left.soundswitch_micro_framing != right.soundswitch_micro_framing);
    changed(
        ConnectionEndpoint::SoundSwitchControlOne,
        left.soundswitch_control_one_experimental !=
            right.soundswitch_control_one_experimental);
    changed(
        ConnectionEndpoint::MidiInput,
        left.midi_input_index != right.midi_input_index);
    changed(
        ConnectionEndpoint::MidiOutput,
        left.midi_output_index != right.midi_output_index);
    changed(
        ConnectionEndpoint::Timing,
        left.frame_rate != right.frame_rate ||
            left.manual_bpm != right.manual_bpm);
    return result;
}

ConnectionEndpointMask diff_runtime_connection_endpoints(
    const ConnectionSettings& target_settings,
    const std::optional<ConnectionSettings>& active_settings) noexcept {
    ConnectionEndpointMask result = 0U;
    const auto changed = [&result](ConnectionEndpoint endpoint, bool differs) {
        if (differs) {
            result = static_cast<ConnectionEndpointMask>(
                result | connection_endpoint_bit(endpoint));
        }
    };

    if (!active_settings.has_value()) {
        changed(ConnectionEndpoint::Os2l, target_settings.os2l_enabled);
        changed(ConnectionEndpoint::ArtNet, target_settings.artnet_enabled);
        changed(ConnectionEndpoint::Sacn, target_settings.sacn_enabled);
        changed(
            ConnectionEndpoint::DmxUsbProUniverse1,
            !target_settings.dmx_usb_pro_ports[0U].empty());
        changed(
            ConnectionEndpoint::DmxUsbProUniverse2,
            !target_settings.dmx_usb_pro_ports[1U].empty());
        changed(
            ConnectionEndpoint::SoundSwitchMicro,
            target_settings.soundswitch_micro_universe != 0U);
        changed(
            ConnectionEndpoint::SoundSwitchControlOne,
            target_settings.soundswitch_control_one_experimental);
        changed(
            ConnectionEndpoint::MidiInput,
            target_settings.midi_input_index >= 0);
        changed(
            ConnectionEndpoint::MidiOutput,
            target_settings.midi_output_index >= 0);
        // Timing is a Runner service even when every external endpoint is off.
        changed(ConnectionEndpoint::Timing, true);
        return result;
    }

    const auto& active = *active_settings;
    const auto enabled_change = [](bool target_enabled,
                                   bool active_enabled,
                                   bool configuration_differs) {
        return target_enabled != active_enabled ||
            (target_enabled && active_enabled && configuration_differs);
    };

    changed(
        ConnectionEndpoint::Os2l,
        enabled_change(
            target_settings.os2l_enabled,
            active.os2l_enabled,
            target_settings.os2l_bind != active.os2l_bind ||
                target_settings.os2l_port != active.os2l_port));
    changed(
        ConnectionEndpoint::ArtNet,
        enabled_change(
            target_settings.artnet_enabled,
            active.artnet_enabled,
            target_settings.artnet_destination != active.artnet_destination ||
                target_settings.artnet_base != active.artnet_base));
    changed(
        ConnectionEndpoint::Sacn,
        enabled_change(
            target_settings.sacn_enabled,
            active.sacn_enabled,
            target_settings.sacn_destination != active.sacn_destination ||
                target_settings.sacn_universe_base !=
                    active.sacn_universe_base));

    const auto target_usb_1 = !target_settings.dmx_usb_pro_ports[0U].empty();
    const auto active_usb_1 = !active.dmx_usb_pro_ports[0U].empty();
    changed(
        ConnectionEndpoint::DmxUsbProUniverse1,
        enabled_change(
            target_usb_1,
            active_usb_1,
            target_settings.dmx_usb_pro_ports[0U] !=
                active.dmx_usb_pro_ports[0U]));
    const auto target_usb_2 = !target_settings.dmx_usb_pro_ports[1U].empty();
    const auto active_usb_2 = !active.dmx_usb_pro_ports[1U].empty();
    changed(
        ConnectionEndpoint::DmxUsbProUniverse2,
        enabled_change(
            target_usb_2,
            active_usb_2,
            target_settings.dmx_usb_pro_ports[1U] !=
                active.dmx_usb_pro_ports[1U]));

    const auto target_micro =
        target_settings.soundswitch_micro_universe != 0U;
    const auto active_micro = active.soundswitch_micro_universe != 0U;
    changed(
        ConnectionEndpoint::SoundSwitchMicro,
        enabled_change(
            target_micro,
            active_micro,
            target_settings.soundswitch_micro_universe !=
                    active.soundswitch_micro_universe ||
                target_settings.soundswitch_micro_framing !=
                    active.soundswitch_micro_framing));
    changed(
        ConnectionEndpoint::SoundSwitchControlOne,
        target_settings.soundswitch_control_one_experimental !=
            active.soundswitch_control_one_experimental);

    const auto target_midi_input = target_settings.midi_input_index >= 0;
    const auto active_midi_input = active.midi_input_index >= 0;
    changed(
        ConnectionEndpoint::MidiInput,
        enabled_change(
            target_midi_input,
            active_midi_input,
            target_settings.midi_input_index != active.midi_input_index));
    const auto target_midi_output = target_settings.midi_output_index >= 0;
    const auto active_midi_output = active.midi_output_index >= 0;
    changed(
        ConnectionEndpoint::MidiOutput,
        enabled_change(
            target_midi_output,
            active_midi_output,
            target_settings.midi_output_index != active.midi_output_index));
    changed(
        ConnectionEndpoint::Timing,
        target_settings.frame_rate != active.frame_rate ||
            target_settings.manual_bpm != active.manual_bpm);
    return result;
}

const char* connection_endpoint_name(ConnectionEndpoint endpoint) noexcept {
    switch (endpoint) {
    case ConnectionEndpoint::Os2l: return "OS2L";
    case ConnectionEndpoint::ArtNet: return "Art-Net";
    case ConnectionEndpoint::Sacn: return "sACN";
    case ConnectionEndpoint::DmxUsbProUniverse1: return "DMX USB Pro universe 1";
    case ConnectionEndpoint::DmxUsbProUniverse2: return "DMX USB Pro universe 2";
    case ConnectionEndpoint::SoundSwitchMicro: return "SoundSwitch Micro";
    case ConnectionEndpoint::SoundSwitchControlOne: return "SoundSwitch Control One";
    case ConnectionEndpoint::MidiInput: return "MIDI input";
    case ConnectionEndpoint::MidiOutput: return "MIDI output";
    case ConnectionEndpoint::Timing: return "timing";
    case ConnectionEndpoint::Count: return "unknown";
    }
    return "unknown";
}

const char* connection_apply_result_name(ConnectionApplyResult result) noexcept {
    switch (result) {
    case ConnectionApplyResult::None: return "None";
    case ConnectionApplyResult::ValidationRejected: return "ValidationRejected";
    case ConnectionApplyResult::SaveFailed: return "SaveFailed";
    case ConnectionApplyResult::SavedNoRuntimeChange: return "SavedNoRuntimeChange";
    case ConnectionApplyResult::SavedAndApplied: return "SavedAndApplied";
    case ConnectionApplyResult::SavedRestartRequired: return "SavedRestartRequired";
    case ConnectionApplyResult::SavedApplyFailed: return "SavedApplyFailed";
    case ConnectionApplyResult::ApplyRuntimeStopped: return "ApplyRuntimeStopped";
    case ConnectionApplyResult::RuntimeStarted: return "RuntimeStarted";
    case ConnectionApplyResult::RuntimeStopped: return "RuntimeStopped";
    case ConnectionApplyResult::StaleGeneration: return "StaleGeneration";
    case ConnectionApplyResult::InvalidTransition: return "InvalidTransition";
    case ConnectionApplyResult::GenerationLimit: return "GenerationLimit";
    }
    return "InvalidTransition";
}

}  // namespace emberlights
