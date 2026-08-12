#include "emberlights/connection_coordinator.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>
#include <type_traits>

namespace {

int failures = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            std::cerr << "CHECK failed at " << __FILE__ << ':' << __LINE__    \
                      << ": " #condition << '\n';                            \
            ++failures;                                                         \
        }                                                                       \
    } while (false)

using emberlights::ConnectionApplyFailureBoundary;
using emberlights::ConnectionApplyResult;
using emberlights::ConnectionCoordinator;
using emberlights::ConnectionEndpoint;
using emberlights::ConnectionRuntimeState;
using emberlights::ConnectionSettings;

static_assert(!std::is_copy_constructible_v<ConnectionCoordinator>);
static_assert(!std::is_move_constructible_v<ConnectionCoordinator>);

[[nodiscard]] bool includes(
    emberlights::ConnectionEndpointMask mask,
    ConnectionEndpoint endpoint) {
    return (mask & emberlights::connection_endpoint_bit(endpoint)) != 0U;
}

[[nodiscard]] ConnectionSettings changed_settings() {
    ConnectionSettings settings;
    settings.os2l_enabled = false;
    settings.os2l_bind = "192.0.2.10";
    settings.os2l_port = 10996U;
    settings.artnet_enabled = true;
    settings.artnet_destination = "192.0.2.20";
    settings.artnet_base = 8U;
    settings.sacn_enabled = true;
    settings.sacn_destination = "multicast";
    settings.sacn_universe_base = 100U;
    settings.dmx_usb_pro_ports = {"COM3", "COM4"};
    settings.soundswitch_micro_universe = 2U;
    settings.soundswitch_control_one_experimental = true;
    settings.midi_input_index = 3;
    settings.midi_output_index = 4;
    settings.frame_rate = 39U;
    settings.manual_bpm = 126.5;
    return settings;
}

void test_initial_snapshot_is_truthful() {
    ConnectionSettings initial;
    initial.os2l_port = 10001U;
    ConnectionCoordinator coordinator(initial, ConnectionRuntimeState::Running);
    const auto snapshot = coordinator.snapshot();
    CHECK(snapshot.generation == 1U);
    CHECK(snapshot.desired_settings == initial);
    CHECK(snapshot.saved_settings == initial);
    CHECK(snapshot.active_settings == initial);
    CHECK(snapshot.desired_to_saved == 0U);
    CHECK(snapshot.saved_to_active == 0U);
    CHECK(snapshot.last_result == ConnectionApplyResult::None);
    CHECK(!snapshot.apply_pending);

    ConnectionCoordinator stopped(initial, ConnectionRuntimeState::Stopped);
    const auto stopped_snapshot = stopped.snapshot();
    CHECK(!stopped_snapshot.active_settings.has_value());
    CHECK(includes(stopped_snapshot.saved_to_active, ConnectionEndpoint::Os2l));
    CHECK(includes(stopped_snapshot.saved_to_active, ConnectionEndpoint::Timing));
}

void test_endpoint_diff_is_exact() {
    ConnectionSettings initial;
    const auto changed = changed_settings();
    const auto mask = emberlights::diff_connection_endpoints(initial, changed);
    for (std::uint8_t value = 0U;
         value < static_cast<std::uint8_t>(ConnectionEndpoint::Count);
         ++value) {
        CHECK(includes(mask, static_cast<ConnectionEndpoint>(value)));
    }
    CHECK(emberlights::diff_connection_endpoints(changed, changed) == 0U);

    auto one = initial;
    one.dmx_usb_pro_ports[1U] = "COM9";
    const auto one_mask = emberlights::diff_connection_endpoints(initial, one);
    CHECK(one_mask == emberlights::connection_endpoint_bit(
        ConnectionEndpoint::DmxUsbProUniverse2));

    one = initial;
    one.soundswitch_micro_universe = 1U;
    CHECK(emberlights::diff_connection_endpoints(initial, one) ==
        emberlights::connection_endpoint_bit(ConnectionEndpoint::SoundSwitchMicro));
}

void test_disabled_endpoint_metadata_is_not_a_runtime_change() {
    ConnectionSettings initial;
    initial.os2l_enabled = false;
    auto desired = initial;
    desired.os2l_bind = "192.0.2.44";
    desired.os2l_port = 10044U;
    desired.artnet_destination = "192.0.2.45";
    desired.artnet_base = 42U;
    desired.sacn_destination = "multicast";
    desired.sacn_universe_base = 142U;

    const auto structural =
        emberlights::diff_connection_endpoints(initial, desired);
    CHECK(includes(structural, ConnectionEndpoint::Os2l));
    CHECK(includes(structural, ConnectionEndpoint::ArtNet));
    CHECK(includes(structural, ConnectionEndpoint::Sacn));
    CHECK(emberlights::diff_runtime_connection_endpoints(desired, initial) == 0U);

    ConnectionCoordinator coordinator(initial, ConnectionRuntimeState::Running);
    auto prepared = coordinator.prepare(coordinator.generation(), desired);
    CHECK(prepared);
    const auto saved = coordinator.acknowledge_saved(*prepared.transaction);
    CHECK(saved.result == ConnectionApplyResult::SavedNoRuntimeChange);
    CHECK(saved.affected_endpoints == 0U);
    const auto snapshot = coordinator.snapshot();
    CHECK(snapshot.active_settings == desired);
    CHECK(snapshot.saved_to_active == 0U);
    CHECK(!snapshot.apply_pending);

    desired.artnet_enabled = true;
    prepared = coordinator.prepare(coordinator.generation(), desired);
    CHECK(prepared);
    const auto enabled = coordinator.acknowledge_saved(*prepared.transaction);
    CHECK(enabled.result == ConnectionApplyResult::SavedRestartRequired);
    CHECK(enabled.affected_endpoints ==
        emberlights::connection_endpoint_bit(ConnectionEndpoint::ArtNet));
}

void test_save_without_runtime_change() {
    ConnectionSettings initial;
    ConnectionCoordinator coordinator(initial, ConnectionRuntimeState::Running);
    auto prepared = coordinator.prepare(coordinator.generation(), initial);
    CHECK(prepared);
    const auto outcome = coordinator.acknowledge_saved(*prepared.transaction);
    CHECK(outcome);
    CHECK(outcome.result == ConnectionApplyResult::SavedNoRuntimeChange);
    CHECK(outcome.affected_endpoints == 0U);
    const auto snapshot = coordinator.snapshot();
    CHECK(!snapshot.apply_pending);
    CHECK(snapshot.saved_to_active == 0U);
}

void test_saved_restart_then_applied() {
    ConnectionSettings initial;
    ConnectionCoordinator coordinator(initial, ConnectionRuntimeState::Running);
    const auto desired = changed_settings();
    auto prepared = coordinator.prepare(coordinator.generation(), desired);
    CHECK(prepared);

    const auto saved = coordinator.acknowledge_saved(*prepared.transaction);
    CHECK(saved.result == ConnectionApplyResult::SavedRestartRequired);
    auto snapshot = coordinator.snapshot();
    CHECK(snapshot.desired_settings == desired);
    CHECK(snapshot.saved_settings == desired);
    CHECK(snapshot.active_settings == initial);
    CHECK(snapshot.desired_to_saved == 0U);
    CHECK(snapshot.saved_to_active == saved.affected_endpoints);
    CHECK(snapshot.saved_to_active != 0U);
    CHECK(snapshot.apply_pending);

    CHECK(coordinator.acknowledge_applied(*prepared.transaction).result ==
        ConnectionApplyResult::InvalidTransition);
    const auto runtime_stopped =
        coordinator.acknowledge_apply_runtime_stopped(*prepared.transaction);
    CHECK(runtime_stopped.result == ConnectionApplyResult::ApplyRuntimeStopped);
    CHECK(runtime_stopped);
    snapshot = coordinator.snapshot();
    CHECK(!snapshot.active_settings.has_value());
    CHECK(snapshot.apply_pending);
    CHECK(snapshot.saved_to_active != 0U);

    const auto applied = coordinator.acknowledge_applied(*prepared.transaction);
    CHECK(applied.result == ConnectionApplyResult::SavedAndApplied);
    CHECK(applied.affected_endpoints ==
        emberlights::diff_runtime_connection_endpoints(desired, std::nullopt));
    snapshot = coordinator.snapshot();
    CHECK(snapshot.active_settings == desired);
    CHECK(snapshot.saved_to_active == 0U);
    CHECK(!snapshot.apply_pending);

    const auto duplicate = coordinator.acknowledge_applied(*prepared.transaction);
    CHECK(duplicate.result == ConnectionApplyResult::StaleGeneration);
    CHECK(coordinator.snapshot().active_settings == desired);
}

void test_saved_apply_failure_with_restored_runtime() {
    ConnectionSettings initial;
    ConnectionCoordinator coordinator(initial, ConnectionRuntimeState::Running);
    const auto desired = changed_settings();
    auto prepared = coordinator.prepare(coordinator.generation(), desired);
    CHECK(prepared);
    CHECK(coordinator.acknowledge_saved(*prepared.transaction).result ==
        ConnectionApplyResult::SavedRestartRequired);
    const auto before_invalid_boundary = coordinator.snapshot();
    CHECK(coordinator.acknowledge_apply_failed(
              *prepared.transaction,
              ConnectionApplyFailureBoundary::PreviousRuntimeRestored).result ==
        ConnectionApplyResult::InvalidTransition);
    CHECK(coordinator.acknowledge_apply_failed(
              *prepared.transaction,
              static_cast<ConnectionApplyFailureBoundary>(255U)).result ==
        ConnectionApplyResult::InvalidTransition);
    CHECK(coordinator.snapshot().generation == before_invalid_boundary.generation);
    CHECK(coordinator.snapshot().apply_pending);
    const auto stopped =
        coordinator.acknowledge_apply_runtime_stopped(*prepared.transaction);
    CHECK(stopped.result == ConnectionApplyResult::ApplyRuntimeStopped);
    CHECK(!coordinator.snapshot().active_settings.has_value());
    CHECK(coordinator.snapshot().apply_pending);
    const auto failed = coordinator.acknowledge_apply_failed(
        *prepared.transaction,
        ConnectionApplyFailureBoundary::PreviousRuntimeRestored);
    CHECK(failed.result == ConnectionApplyResult::SavedApplyFailed);
    const auto snapshot = coordinator.snapshot();
    CHECK(snapshot.desired_settings == desired);
    CHECK(snapshot.saved_settings == desired);
    CHECK(snapshot.active_settings == initial);
    CHECK(snapshot.saved_to_active == failed.affected_endpoints);
    CHECK(snapshot.saved_to_active != 0U);
    CHECK(!snapshot.apply_pending);
}

void test_saved_apply_failure_can_report_stopped_runtime() {
    ConnectionSettings initial;
    ConnectionCoordinator coordinator(initial, ConnectionRuntimeState::Running);
    const auto desired = changed_settings();
    auto prepared = coordinator.prepare(coordinator.generation(), desired);
    CHECK(prepared);
    CHECK(coordinator.acknowledge_saved(*prepared.transaction).result ==
        ConnectionApplyResult::SavedRestartRequired);
    CHECK(coordinator.acknowledge_apply_runtime_stopped(*prepared.transaction).result ==
        ConnectionApplyResult::ApplyRuntimeStopped);
    CHECK(!coordinator.snapshot().active_settings.has_value());
    CHECK(coordinator.snapshot().apply_pending);
    CHECK(coordinator.acknowledge_apply_runtime_stopped(*prepared.transaction).result ==
        ConnectionApplyResult::InvalidTransition);
    const auto failed = coordinator.acknowledge_apply_failed(
        *prepared.transaction,
        ConnectionApplyFailureBoundary::RuntimeStopped);
    CHECK(failed.result == ConnectionApplyResult::SavedApplyFailed);
    auto snapshot = coordinator.snapshot();
    CHECK(!snapshot.active_settings.has_value());
    CHECK(snapshot.saved_settings == desired);
    CHECK(snapshot.saved_to_active != 0U);
    CHECK(failed.affected_endpoints == snapshot.saved_to_active);
    CHECK(!snapshot.apply_pending);

    const auto started =
        coordinator.acknowledge_runtime_started(coordinator.generation());
    CHECK(started.result == ConnectionApplyResult::RuntimeStarted);
    snapshot = coordinator.snapshot();
    CHECK(snapshot.active_settings == desired);
    CHECK(snapshot.saved_to_active == 0U);
}

void test_apply_failure_reports_exact_remaining_runtime_drift() {
    ConnectionSettings initial;
    auto desired = initial;
    desired.artnet_enabled = true;

    ConnectionCoordinator restored(initial, ConnectionRuntimeState::Running);
    auto restored_transaction =
        restored.prepare(restored.generation(), desired);
    CHECK(restored_transaction);
    const auto restored_saved =
        restored.acknowledge_saved(*restored_transaction.transaction);
    CHECK(restored_saved.affected_endpoints ==
        emberlights::connection_endpoint_bit(ConnectionEndpoint::ArtNet));
    CHECK(restored.acknowledge_apply_runtime_stopped(
              *restored_transaction.transaction).result ==
        ConnectionApplyResult::ApplyRuntimeStopped);
    const auto restored_failed = restored.acknowledge_apply_failed(
        *restored_transaction.transaction,
        ConnectionApplyFailureBoundary::PreviousRuntimeRestored);
    const auto artnet_only =
        emberlights::connection_endpoint_bit(ConnectionEndpoint::ArtNet);
    CHECK(restored_failed.affected_endpoints == artnet_only);
    CHECK(restored.snapshot().saved_to_active == artnet_only);
    CHECK(restored.snapshot().last_affected_endpoints == artnet_only);

    ConnectionCoordinator stopped(initial, ConnectionRuntimeState::Running);
    auto stopped_transaction = stopped.prepare(stopped.generation(), desired);
    CHECK(stopped_transaction);
    CHECK(stopped.acknowledge_saved(*stopped_transaction.transaction).result ==
        ConnectionApplyResult::SavedRestartRequired);
    CHECK(stopped.acknowledge_apply_runtime_stopped(
              *stopped_transaction.transaction).result ==
        ConnectionApplyResult::ApplyRuntimeStopped);
    const auto stopped_failed = stopped.acknowledge_apply_failed(
        *stopped_transaction.transaction,
        ConnectionApplyFailureBoundary::RuntimeStopped);
    const auto expected_stopped_drift =
        emberlights::diff_runtime_connection_endpoints(desired, std::nullopt);
    CHECK(stopped_failed.affected_endpoints == expected_stopped_drift);
    CHECK(stopped.snapshot().saved_to_active == expected_stopped_drift);
    CHECK(stopped.snapshot().last_affected_endpoints == expected_stopped_drift);
    CHECK(includes(expected_stopped_drift, ConnectionEndpoint::ArtNet));
    CHECK(includes(expected_stopped_drift, ConnectionEndpoint::Os2l));
    CHECK(includes(expected_stopped_drift, ConnectionEndpoint::Timing));
}

void test_stopped_save_allows_more_edits_then_start() {
    ConnectionSettings initial;
    ConnectionCoordinator coordinator(initial, ConnectionRuntimeState::Stopped);

    auto first_desired = initial;
    first_desired.artnet_enabled = true;
    auto first = coordinator.prepare(coordinator.generation(), first_desired);
    CHECK(first);
    const auto first_saved = coordinator.acknowledge_saved(*first.transaction);
    CHECK(first_saved.result == ConnectionApplyResult::SavedNoRuntimeChange);
    CHECK(first_saved.affected_endpoints == 0U);
    CHECK(!coordinator.snapshot().active_settings.has_value());
    CHECK(!coordinator.snapshot().apply_pending);

    auto second_desired = first_desired;
    second_desired.artnet_destination = "192.0.2.99";
    auto second = coordinator.prepare(coordinator.generation(), second_desired);
    CHECK(second);
    const auto second_saved = coordinator.acknowledge_saved(*second.transaction);
    CHECK(second_saved.result == ConnectionApplyResult::SavedNoRuntimeChange);
    CHECK(!coordinator.snapshot().apply_pending);

    const auto started =
        coordinator.acknowledge_runtime_started(coordinator.generation());
    CHECK(started.result == ConnectionApplyResult::RuntimeStarted);
    auto snapshot = coordinator.snapshot();
    CHECK(snapshot.active_settings == second_desired);
    CHECK(snapshot.saved_to_active == 0U);

    const auto stopped =
        coordinator.acknowledge_runtime_stopped(coordinator.generation());
    CHECK(stopped.result == ConnectionApplyResult::RuntimeStopped);
    snapshot = coordinator.snapshot();
    CHECK(!snapshot.active_settings.has_value());
    CHECK(snapshot.saved_to_active != 0U);
    CHECK(coordinator.acknowledge_runtime_stopped(coordinator.generation()).result ==
        ConnectionApplyResult::InvalidTransition);
}

void test_pending_apply_blocks_new_prepare() {
    ConnectionSettings initial;
    ConnectionCoordinator coordinator(initial, ConnectionRuntimeState::Running);
    auto first_desired = initial;
    first_desired.artnet_enabled = true;
    auto first = coordinator.prepare(coordinator.generation(), first_desired);
    CHECK(first);
    CHECK(coordinator.acknowledge_saved(*first.transaction).result ==
        ConnectionApplyResult::SavedRestartRequired);
    const auto before = coordinator.snapshot();

    auto second_desired = initial;
    second_desired.sacn_enabled = true;
    const auto blocked = coordinator.prepare(coordinator.generation(), second_desired);
    CHECK(!blocked);
    CHECK(blocked.result == ConnectionApplyResult::InvalidTransition);
    const auto after = coordinator.snapshot();
    CHECK(after.generation == before.generation);
    CHECK(after.desired_settings == before.desired_settings);
    CHECK(after.saved_settings == before.saved_settings);
    CHECK(after.active_settings == before.active_settings);
    CHECK(after.apply_pending);
    CHECK(coordinator.acknowledge_runtime_stopped(coordinator.generation()).result ==
        ConnectionApplyResult::InvalidTransition);
    CHECK(coordinator.snapshot().apply_pending);
    CHECK(coordinator.acknowledge_apply_runtime_stopped(*first.transaction).result ==
        ConnectionApplyResult::ApplyRuntimeStopped);
    CHECK(!coordinator.snapshot().active_settings.has_value());
    CHECK(!coordinator.prepare(coordinator.generation(), second_desired));
    CHECK(coordinator.acknowledge_applied(*first.transaction).result ==
        ConnectionApplyResult::SavedAndApplied);
}

void test_stale_validation_failure_preserves_pending_apply() {
    ConnectionSettings initial;
    ConnectionCoordinator coordinator(initial, ConnectionRuntimeState::Running);
    auto first_desired = initial;
    first_desired.artnet_enabled = true;
    auto stale_desired = initial;
    stale_desired.sacn_enabled = true;
    auto first = coordinator.prepare(coordinator.generation(), first_desired);
    auto stale = coordinator.prepare(coordinator.generation(), stale_desired);
    CHECK(first && stale);
    CHECK(coordinator.acknowledge_saved(*first.transaction).result ==
        ConnectionApplyResult::SavedRestartRequired);
    const auto before = coordinator.snapshot();
    CHECK(coordinator.reject_validation(*stale.transaction).result ==
        ConnectionApplyResult::StaleGeneration);
    const auto after = coordinator.snapshot();
    CHECK(after.generation == before.generation);
    CHECK(after.saved_settings == first_desired);
    CHECK(after.active_settings == initial);
    CHECK(after.apply_pending);
    CHECK(coordinator.acknowledge_apply_runtime_stopped(*first.transaction).result ==
        ConnectionApplyResult::ApplyRuntimeStopped);
    CHECK(coordinator.acknowledge_applied(*first.transaction).result ==
        ConnectionApplyResult::SavedAndApplied);
}

void test_stale_save_failure_preserves_pending_apply() {
    ConnectionSettings initial;
    ConnectionCoordinator coordinator(initial, ConnectionRuntimeState::Running);
    auto first_desired = initial;
    first_desired.artnet_enabled = true;
    auto stale_desired = initial;
    stale_desired.sacn_enabled = true;
    auto first = coordinator.prepare(coordinator.generation(), first_desired);
    auto stale = coordinator.prepare(coordinator.generation(), stale_desired);
    CHECK(first && stale);
    CHECK(coordinator.acknowledge_saved(*first.transaction).result ==
        ConnectionApplyResult::SavedRestartRequired);
    const auto before = coordinator.snapshot();
    CHECK(coordinator.acknowledge_save_failed(*stale.transaction).result ==
        ConnectionApplyResult::StaleGeneration);
    const auto after = coordinator.snapshot();
    CHECK(after.generation == before.generation);
    CHECK(after.saved_settings == first_desired);
    CHECK(after.active_settings == initial);
    CHECK(after.apply_pending);
    CHECK(coordinator.acknowledge_apply_runtime_stopped(*first.transaction).result ==
        ConnectionApplyResult::ApplyRuntimeStopped);
    CHECK(coordinator.acknowledge_applied(*first.transaction).result ==
        ConnectionApplyResult::SavedAndApplied);
}

void test_validation_and_save_failures_preserve_durable_truth() {
    ConnectionSettings initial;
    const auto desired = changed_settings();

    ConnectionCoordinator validation(initial, ConnectionRuntimeState::Running);
    auto validation_transaction = validation.prepare(validation.generation(), desired);
    const auto rejected =
        validation.reject_validation(*validation_transaction.transaction);
    CHECK(!rejected);
    CHECK(rejected.result == ConnectionApplyResult::ValidationRejected);
    auto snapshot = validation.snapshot();
    CHECK(snapshot.desired_settings == desired);
    CHECK(snapshot.saved_settings == initial);
    CHECK(snapshot.active_settings == initial);
    CHECK(snapshot.desired_to_saved != 0U);
    CHECK(snapshot.saved_to_active == 0U);

    ConnectionCoordinator save(initial, ConnectionRuntimeState::Running);
    auto save_transaction = save.prepare(save.generation(), desired);
    const auto save_failed =
        save.acknowledge_save_failed(*save_transaction.transaction);
    CHECK(!save_failed);
    CHECK(save_failed.result == ConnectionApplyResult::SaveFailed);
    snapshot = save.snapshot();
    CHECK(snapshot.desired_settings == desired);
    CHECK(snapshot.saved_settings == initial);
    CHECK(snapshot.active_settings == initial);
    CHECK(snapshot.desired_to_saved != 0U);
}

void test_stale_and_foreign_transactions_fail_closed() {
    ConnectionSettings initial;
    auto first_desired = initial;
    first_desired.artnet_enabled = true;
    auto second_desired = initial;
    second_desired.sacn_enabled = true;

    ConnectionCoordinator coordinator(initial, ConnectionRuntimeState::Running);
    auto first = coordinator.prepare(coordinator.generation(), first_desired);
    auto second = coordinator.prepare(coordinator.generation(), second_desired);
    CHECK(first && second);
    CHECK(coordinator.acknowledge_save_failed(*first.transaction).result ==
        ConnectionApplyResult::SaveFailed);
    const auto after_first = coordinator.snapshot();
    CHECK(coordinator.acknowledge_saved(*second.transaction).result ==
        ConnectionApplyResult::StaleGeneration);
    CHECK(coordinator.snapshot().desired_settings == after_first.desired_settings);
    CHECK(coordinator.snapshot().saved_settings == after_first.saved_settings);

    ConnectionCoordinator other(initial, ConnectionRuntimeState::Running);
    auto foreign = other.prepare(other.generation(), second_desired);
    CHECK(coordinator.acknowledge_saved(*foreign.transaction).result ==
        ConnectionApplyResult::InvalidTransition);
    CHECK(coordinator.snapshot().saved_settings == initial);
}

void test_document_boundary_invalidates_pending_apply() {
    ConnectionSettings initial;
    ConnectionCoordinator coordinator(initial, ConnectionRuntimeState::Running);
    auto prepared = coordinator.prepare(coordinator.generation(), changed_settings());
    CHECK(coordinator.acknowledge_saved(*prepared.transaction).result ==
        ConnectionApplyResult::SavedRestartRequired);
    CHECK(coordinator.snapshot().apply_pending);

    ConnectionSettings opened;
    opened.os2l_port = 12000U;
    const auto before_running_replacement = coordinator.snapshot();
    const auto refused = coordinator.replace_document_settings(
        coordinator.generation(),
        opened,
        ConnectionRuntimeState::Running);
    CHECK(!refused);
    CHECK(refused.result == ConnectionApplyResult::InvalidTransition);
    auto snapshot = coordinator.snapshot();
    CHECK(snapshot.generation == before_running_replacement.generation);
    CHECK(snapshot.desired_settings == before_running_replacement.desired_settings);
    CHECK(snapshot.saved_settings == before_running_replacement.saved_settings);
    CHECK(snapshot.active_settings == before_running_replacement.active_settings);
    CHECK(snapshot.apply_pending);

    CHECK(coordinator.acknowledge_apply_runtime_stopped(*prepared.transaction).result ==
        ConnectionApplyResult::ApplyRuntimeStopped);
    CHECK(!coordinator.snapshot().active_settings.has_value());
    CHECK(coordinator.snapshot().apply_pending);

    const auto replaced = coordinator.replace_document_settings(
        coordinator.generation(),
        opened,
        ConnectionRuntimeState::Stopped);
    CHECK(replaced.result == ConnectionApplyResult::SavedNoRuntimeChange);
    snapshot = coordinator.snapshot();
    CHECK(snapshot.desired_settings == opened);
    CHECK(snapshot.saved_settings == opened);
    CHECK(!snapshot.active_settings.has_value());
    CHECK(!snapshot.apply_pending);
    CHECK(coordinator.acknowledge_applied(*prepared.transaction).result ==
        ConnectionApplyResult::StaleGeneration);
}

void test_names_are_stable() {
    CHECK(std::string_view(emberlights::connection_endpoint_name(
        ConnectionEndpoint::SoundSwitchMicro)) == "SoundSwitch Micro");
    CHECK(std::string_view(emberlights::connection_apply_result_name(
        ConnectionApplyResult::SavedApplyFailed)) == "SavedApplyFailed");
    CHECK(std::string_view(emberlights::connection_apply_result_name(
        ConnectionApplyResult::ApplyRuntimeStopped)) == "ApplyRuntimeStopped");
    CHECK(std::string_view(emberlights::connection_apply_result_name(
        static_cast<ConnectionApplyResult>(255U))) == "InvalidTransition");
}

}  // namespace

int main() {
    test_initial_snapshot_is_truthful();
    test_endpoint_diff_is_exact();
    test_disabled_endpoint_metadata_is_not_a_runtime_change();
    test_save_without_runtime_change();
    test_saved_restart_then_applied();
    test_saved_apply_failure_with_restored_runtime();
    test_saved_apply_failure_can_report_stopped_runtime();
    test_apply_failure_reports_exact_remaining_runtime_drift();
    test_stopped_save_allows_more_edits_then_start();
    test_pending_apply_blocks_new_prepare();
    test_stale_validation_failure_preserves_pending_apply();
    test_stale_save_failure_preserves_pending_apply();
    test_validation_and_save_failures_preserve_durable_truth();
    test_stale_and_foreign_transactions_fail_closed();
    test_document_boundary_invalidates_pending_apply();
    test_names_are_stable();

    if (failures != 0) {
        std::cerr << failures << " connection coordinator checks failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "connection coordinator tests passed\n";
    return EXIT_SUCCESS;
}
