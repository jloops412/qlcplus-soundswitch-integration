#include "emberlights/autoloop_runtime.hpp"

namespace emberlights {
namespace {

[[nodiscard]] bool legacy_owner(AutoloopTrackScriptOwner owner) noexcept {
    return owner == AutoloopTrackScriptOwner::LegacyLook ||
        owner == AutoloopTrackScriptOwner::LegacyAutoloop;
}

}  // namespace

void AutoloopRuntimeAdapter::set_result(
    showcore::AutoloopDirectorResult result) noexcept {
    status_.last_result = result;
    status_.mode = director_.status().fault ==
            showcore::AutoloopDirectorFault::None
        ? (director_.status().package_active
              ? AutoloopRuntimeMode::CompiledV2
              : AutoloopRuntimeMode::LegacyV1)
        : AutoloopRuntimeMode::Fault;
}

void AutoloopRuntimeAdapter::compose(
    showcore::LayerStack& destination) noexcept {
    const auto copy = [&](showcore::LayerId layer) noexcept {
        if (const auto* values = private_layers_.layer(layer); values != nullptr) {
            destination.replace_layer(layer, *values);
        } else {
            destination.clear_layer(layer);
        }
    };

    copy(showcore::LayerId::Autonomous);
    copy(showcore::LayerId::ManualAutoloop);

    const bool manual_replaces_lower_layers =
        director_.status().manual.active &&
        director_.status().manual.mode ==
            showcore::CompiledAutoloopPlaybackMode::Replace;
    if (status_.track_script_owner ==
        AutoloopTrackScriptOwner::CompiledV2) {
        if (director_.status().scripted.active &&
            director_.status().fault == showcore::AutoloopDirectorFault::None) {
            copy(showcore::LayerId::TrackScript);
        } else {
            status_.track_script_owner = AutoloopTrackScriptOwner::None;
            destination.clear_layer(showcore::LayerId::TrackScript);
        }
    } else if (status_.track_script_owner ==
                   AutoloopTrackScriptOwner::None ||
               manual_replaces_lower_layers) {
        destination.clear_layer(showcore::LayerId::TrackScript);
    }
    status_.track_script_suppressed_by_replace =
        manual_replaces_lower_layers &&
        status_.track_script_owner != AutoloopTrackScriptOwner::None;
    // An overlay leaves a legacy owner untouched while V2 continues lower-
    // layer progression. A V2 Replace session suppresses it without changing
    // ownership, so the legacy player can resume naturally when Replace ends.
}

showcore::AutoloopDirectorResult AutoloopRuntimeAdapter::activate_package(
    const showcore::CompiledAutoloopPackage* package,
    std::uint64_t generation,
    showcore::LayerStack& destination) noexcept {
    status_.track_script_owner = AutoloopTrackScriptOwner::None;
    destination.clear_layer(showcore::LayerId::TrackScript);
    const auto result = director_.activate_package(
        package, generation, private_layers_);
    if (result == showcore::AutoloopDirectorResult::StaleGeneration) {
        static_cast<void>(director_.fault(
            showcore::AutoloopDirectorFault::ExternalFault,
            private_layers_));
    }
    set_result(result);
    compose(destination);
    return result;
}

showcore::AutoloopDirectorResult AutoloopRuntimeAdapter::clear_package(
    showcore::LayerStack& destination) noexcept {
    const auto previous_owner = status_.track_script_owner;
    const auto result = director_.clear_package(private_layers_);
    set_result(result);
    if (previous_owner == AutoloopTrackScriptOwner::CompiledV2) {
        status_.track_script_owner = AutoloopTrackScriptOwner::None;
    }
    compose(destination);
    return result;
}

showcore::AutoloopDirectorResult AutoloopRuntimeAdapter::fault(
    showcore::AutoloopDirectorFault reason,
    showcore::LayerStack& destination) noexcept {
    const auto previous_owner = status_.track_script_owner;
    const auto result = director_.fault(reason, private_layers_);
    set_result(result);
    if (previous_owner == AutoloopTrackScriptOwner::CompiledV2) {
        status_.track_script_owner = AutoloopTrackScriptOwner::None;
    }
    compose(destination);
    return result;
}

showcore::AutoloopDirectorResult AutoloopRuntimeAdapter::tick(
    const showcore::AutoloopTransportState& transport,
    showcore::LayerStack& destination) noexcept {
    const auto result = director_.tick(transport, private_layers_);
    set_result(result);
    compose(destination);
    return result;
}

showcore::AutoloopDirectorResult AutoloopRuntimeAdapter::launch_scripted(
    const showcore::AutoloopLaunchRequest& request,
    const showcore::AutoloopTransportState& transport,
    showcore::LayerStack& destination) noexcept {
    const auto previous_owner = status_.track_script_owner;
    const auto result = director_.launch_scripted(
        request, transport, private_layers_);
    set_result(result);
    if (director_.status().scripted.active &&
        director_.status().fault == showcore::AutoloopDirectorFault::None &&
        result == showcore::AutoloopDirectorResult::ScriptedStarted) {
        status_.track_script_owner = AutoloopTrackScriptOwner::CompiledV2;
        destination.clear_layer(showcore::LayerId::TrackScript);
    } else {
        status_.track_script_owner = previous_owner;
    }
    compose(destination);
    return result;
}

showcore::AutoloopDirectorResult AutoloopRuntimeAdapter::clear_scripted(
    std::uint64_t expected_generation,
    showcore::LayerStack& destination) noexcept {
    const auto result = director_.clear_scripted(
        expected_generation, private_layers_);
    set_result(result);
    if (status_.track_script_owner ==
            AutoloopTrackScriptOwner::CompiledV2 &&
        !director_.status().scripted.active) {
        status_.track_script_owner = AutoloopTrackScriptOwner::None;
    }
    compose(destination);
    return result;
}

showcore::AutoloopDirectorResult AutoloopRuntimeAdapter::launch_manual(
    const showcore::AutoloopLaunchRequest& request,
    const showcore::AutoloopTransportState& transport,
    showcore::LayerStack& destination) noexcept {
    const auto result = director_.launch_manual(
        request, transport, private_layers_);
    set_result(result);
    compose(destination);
    return result;
}

showcore::AutoloopDirectorResult AutoloopRuntimeAdapter::clear_manual(
    std::uint64_t expected_generation,
    showcore::LayerStack& destination) noexcept {
    const auto result = director_.clear_manual(
        expected_generation, private_layers_);
    set_result(result);
    compose(destination);
    return result;
}

showcore::AutoloopDirectorResult AutoloopRuntimeAdapter::request_all_banks(
    std::uint64_t expected_generation) noexcept {
    const auto result = director_.request_all_banks(expected_generation);
    set_result(result);
    return result;
}

showcore::AutoloopDirectorResult
AutoloopRuntimeAdapter::request_exclusive_bank(
    std::uint16_t bank,
    std::uint64_t expected_generation) noexcept {
    const auto result = director_.request_exclusive_bank(
        bank, expected_generation);
    set_result(result);
    return result;
}

showcore::AutoloopDirectorResult AutoloopRuntimeAdapter::set_bank_enabled(
    std::uint16_t bank,
    bool enabled,
    std::uint64_t expected_generation) noexcept {
    const auto result = director_.set_bank_enabled(
        bank, enabled, expected_generation);
    set_result(result);
    return result;
}

bool AutoloopRuntimeAdapter::claim_legacy_track_script(
    AutoloopTrackScriptOwner owner,
    showcore::LayerStack& destination) noexcept {
    if (!legacy_owner(owner)) {
        return false;
    }
    if (director_.status().package_generation != 0U) {
        const auto result = director_.clear_scripted(
            director_.status().package_generation, private_layers_);
        set_result(result);
    }
    private_layers_.clear_layer(showcore::LayerId::TrackScript);
    destination.clear_layer(showcore::LayerId::TrackScript);
    status_.track_script_owner = owner;
    status_.track_script_suppressed_by_replace =
        director_.status().manual.active &&
        director_.status().manual.mode ==
            showcore::CompiledAutoloopPlaybackMode::Replace;
    return true;
}

void AutoloopRuntimeAdapter::release_legacy_track_script(
    AutoloopTrackScriptOwner owner,
    showcore::LayerStack& destination) noexcept {
    if (status_.track_script_owner != owner || !legacy_owner(owner)) {
        return;
    }
    status_.track_script_owner = AutoloopTrackScriptOwner::None;
    status_.track_script_suppressed_by_replace = false;
    destination.clear_layer(showcore::LayerId::TrackScript);
}

}  // namespace emberlights
