#pragma once

#include "showcore/autoloop_director.hpp"

#include <cstdint>

namespace emberlights {

enum class AutoloopRuntimeMode : std::uint8_t {
    LegacyV1,
    CompiledV2,
    Fault,
    Count
};

// TrackScript is a single approved renderer layer. The adapter therefore
// makes its producer mutually exclusive instead of letting two players race
// to replace the same LayerBuffer.
enum class AutoloopTrackScriptOwner : std::uint8_t {
    None,
    LegacyLook,
    LegacyAutoloop,
    CompiledV2,
    Count
};

struct AutoloopRuntimeAdapterStatus {
    AutoloopRuntimeMode mode{AutoloopRuntimeMode::LegacyV1};
    AutoloopTrackScriptOwner track_script_owner{
        AutoloopTrackScriptOwner::None};
    bool track_script_suppressed_by_replace{false};
    showcore::AutoloopDirectorResult last_result{
        showcore::AutoloopDirectorResult::None};
};

// Keeps the reusable Director isolated from Runner's legacy players. The
// Director evaluates into a private stack, then this adapter copies only the
// layers it explicitly owns into the renderer stack. All methods are fixed-
// storage and allocation-free.
class AutoloopRuntimeAdapter {
public:
    [[nodiscard]] showcore::AutoloopDirectorResult activate_package(
        const showcore::CompiledAutoloopPackage* package,
        std::uint64_t generation,
        showcore::LayerStack& destination) noexcept;
    [[nodiscard]] showcore::AutoloopDirectorResult clear_package(
        showcore::LayerStack& destination) noexcept;
    [[nodiscard]] showcore::AutoloopDirectorResult fault(
        showcore::AutoloopDirectorFault reason,
        showcore::LayerStack& destination) noexcept;

    [[nodiscard]] showcore::AutoloopDirectorResult tick(
        const showcore::AutoloopTransportState& transport,
        showcore::LayerStack& destination) noexcept;
    [[nodiscard]] showcore::AutoloopDirectorResult launch_scripted(
        const showcore::AutoloopLaunchRequest& request,
        const showcore::AutoloopTransportState& transport,
        showcore::LayerStack& destination) noexcept;
    [[nodiscard]] showcore::AutoloopDirectorResult clear_scripted(
        std::uint64_t expected_generation,
        showcore::LayerStack& destination) noexcept;
    [[nodiscard]] showcore::AutoloopDirectorResult launch_manual(
        const showcore::AutoloopLaunchRequest& request,
        const showcore::AutoloopTransportState& transport,
        showcore::LayerStack& destination) noexcept;
    [[nodiscard]] showcore::AutoloopDirectorResult clear_manual(
        std::uint64_t expected_generation,
        showcore::LayerStack& destination) noexcept;

    [[nodiscard]] showcore::AutoloopDirectorResult request_all_banks(
        std::uint64_t expected_generation) noexcept;
    [[nodiscard]] showcore::AutoloopDirectorResult request_exclusive_bank(
        std::uint16_t bank,
        std::uint64_t expected_generation) noexcept;
    [[nodiscard]] showcore::AutoloopDirectorResult set_bank_enabled(
        std::uint16_t bank,
        bool enabled,
        std::uint64_t expected_generation) noexcept;

    // Claiming a legacy producer first retires any V2 scripted session and
    // clears its public contribution. The legacy player may then populate the
    // layer without a subsequent V2 tick overwriting it.
    [[nodiscard]] bool claim_legacy_track_script(
        AutoloopTrackScriptOwner owner,
        showcore::LayerStack& destination) noexcept;
    void release_legacy_track_script(
        AutoloopTrackScriptOwner owner,
        showcore::LayerStack& destination) noexcept;

    [[nodiscard]] bool package_active() const noexcept {
        return director_.status().package_active;
    }
    [[nodiscard]] const AutoloopRuntimeAdapterStatus& status() const noexcept {
        return status_;
    }
    [[nodiscard]] const showcore::AutoloopDirectorStatus& director_status()
        const noexcept { return director_.status(); }

private:
    void compose(showcore::LayerStack& destination) noexcept;
    void set_result(showcore::AutoloopDirectorResult result) noexcept;

    showcore::AutoloopDirector director_{};
    showcore::LayerStack private_layers_{};
    AutoloopRuntimeAdapterStatus status_{};
};

}  // namespace emberlights
