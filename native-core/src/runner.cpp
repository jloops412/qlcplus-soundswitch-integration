#include "emberlights/runner.hpp"

#include "showcore/artnet.hpp"
#include "showcore/dmx_usb_pro.hpp"
#include "showcore/look.hpp"
#include "showcore/os2l_server.hpp"
#include "showcore/sacn.hpp"
#include "showcore/soundswitch_micro.hpp"
#include "showcore/soundswitch_control_one.hpp"
#include "showcore/winmm_midi.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string_view>
#include <thread>
#include <utility>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#endif

namespace emberlights {
namespace {

using SteadyClock = std::chrono::steady_clock;

inline constexpr std::uint64_t kJitterBucketWidthUs = 50U;
inline constexpr std::size_t kJitterBucketCount = 201U;
inline constexpr std::uint64_t kDeadlineMissUs = 5'000U;
inline constexpr std::uint64_t kPackedAutoloopAddressMask = 0x0FFFU;
inline constexpr std::uint64_t kPackedInvalidAutoloopAddress = 0x0FFFU;
inline constexpr std::uint64_t kPackedAutoloopRepeatShift = 12U;
inline constexpr std::uint64_t kPackedAutoloopProgressShift = 14U;
inline constexpr std::uint64_t kPackedAutoloopCycleShift = 24U;
inline constexpr std::size_t kArtNetOutputHealth = 0U;
inline constexpr std::size_t kSacnOutputHealth = 1U;
inline constexpr std::size_t kDmxUsbProUniverseOneHealth = 2U;
inline constexpr std::size_t kDmxUsbProUniverseTwoHealth = 3U;
inline constexpr std::size_t kSoundSwitchMicroOutputHealth = 4U;
inline constexpr std::size_t kSoundSwitchControlOneOutputHealth = 5U;

[[nodiscard]] std::uint16_t nonzero_slot_count(
    const showcore::DmxUniverse& universe) noexcept {
    return static_cast<std::uint16_t>(std::count_if(
        universe.begin(), universe.end(), [](std::uint8_t value) {
            return value != 0U;
        }));
}

[[nodiscard]] std::uint16_t encode_autoloop(showcore::AutoloopAddress address) noexcept {
    return address.valid()
        ? static_cast<std::uint16_t>(
              static_cast<std::uint16_t>(address.bank * showcore::kAutoloopsPerBank) +
              address.slot)
        : 0xFFFFU;
}

[[nodiscard]] showcore::AutoloopAddress decode_autoloop(std::uint16_t encoded) noexcept {
    if (encoded >= showcore::kMaxAutoloops) {
        return {};
    }
    return {
        static_cast<std::uint16_t>(encoded / showcore::kAutoloopsPerBank),
        static_cast<std::uint8_t>(encoded % showcore::kAutoloopsPerBank)};
}

[[nodiscard]] std::uint64_t static_look_feedback_token(
    std::string_view identity) noexcept {
    // FNV-1a provides a deterministic, non-sensitive feedback identity for
    // trusted protocol bindings without retaining raw device/control text.
    std::uint64_t hash = 14695981039346656037ULL;
    for (const auto character : identity) {
        hash ^= static_cast<std::uint8_t>(character);
        hash *= 1099511628211ULL;
    }
    return hash == 0U ? 1U : hash;
}

[[nodiscard]] StaticLookOwnerContext normalized_static_look_owner(
    StaticLookOwnerContext owner,
    std::uint16_t look_index) noexcept {
    if (owner.kind == StaticLookOwnerKind::None) {
        owner.kind = StaticLookOwnerKind::External;
    }
    if (owner.feedback_token == 0U) {
        owner.feedback_token = 0x454C000000000000ULL ^
            (static_cast<std::uint64_t>(owner.kind) << 32U) ^
            (static_cast<std::uint64_t>(look_index) + 1U);
    }
    return owner;
}

[[nodiscard]] std::uint64_t encode_autoloop_playback(
    const showcore::AutoloopPlaybackStatus* status) noexcept {
    if (status == nullptr) {
        return kPackedInvalidAutoloopAddress;
    }
    const auto encoded_address = encode_autoloop(status->address);
    const auto packed_address = encoded_address < showcore::kMaxAutoloops
        ? static_cast<std::uint64_t>(encoded_address)
        : kPackedInvalidAutoloopAddress;
    const auto progress = static_cast<std::uint64_t>(std::lround(
        std::clamp(status->progress, 0.0F, 1.0F) * 1000.0F));
    return packed_address |
        ((static_cast<std::uint64_t>(status->repeat) & 0x03U) <<
         kPackedAutoloopRepeatShift) |
        ((progress & 0x03FFU) << kPackedAutoloopProgressShift) |
        (static_cast<std::uint64_t>(status->completed_cycles) << kPackedAutoloopCycleShift);
}

[[nodiscard]] std::uint64_t encode_autoloop_playback(
    showcore::AutoloopAddress address,
    showcore::AutoloopRepeat repeat,
    float progress_value,
    std::uint32_t completed_cycles) noexcept {
    const auto encoded_address = encode_autoloop(address);
    const auto packed_address = encoded_address < showcore::kMaxAutoloops
        ? static_cast<std::uint64_t>(encoded_address)
        : kPackedInvalidAutoloopAddress;
    const auto progress = static_cast<std::uint64_t>(std::lround(
        std::clamp(progress_value, 0.0F, 1.0F) * 1000.0F));
    return packed_address |
        ((static_cast<std::uint64_t>(repeat) & 0x03U) <<
         kPackedAutoloopRepeatShift) |
        ((progress & 0x03FFU) << kPackedAutoloopProgressShift) |
        (static_cast<std::uint64_t>(completed_cycles) <<
         kPackedAutoloopCycleShift);
}

void update_maximum(
    std::atomic<std::uint64_t>& maximum,
    std::uint64_t candidate) noexcept {
    auto observed = maximum.load(std::memory_order_relaxed);
    while (candidate > observed && !maximum.compare_exchange_weak(
               observed,
               candidate,
               std::memory_order_relaxed,
               std::memory_order_relaxed)) {
    }
}

[[nodiscard]] bool is_hazard(showcore::Property property) noexcept {
    return property == showcore::Property::Fog || property == showcore::Property::Haze ||
        property == showcore::Property::Laser || property == showcore::Property::Spark;
}

[[nodiscard]] AdapterState adapter_state(
    showcore::Os2lDiscoveryState state) noexcept {
    switch (state) {
    case showcore::Os2lDiscoveryState::Unavailable:
        return AdapterState::Disabled;
    case showcore::Os2lDiscoveryState::Starting:
        return AdapterState::Starting;
    case showcore::Os2lDiscoveryState::Advertised:
        return AdapterState::Ready;
    case showcore::Os2lDiscoveryState::Fault:
        return AdapterState::Fault;
    }
    return AdapterState::Fault;
}

constexpr std::array<showcore::Property, 12> kVisualBlackoutProperties{{
    showcore::Property::Intensity,
    showcore::Property::Red,
    showcore::Property::Green,
    showcore::Property::Blue,
    showcore::Property::White,
    showcore::Property::Amber,
    showcore::Property::UV,
    showcore::Property::Cyan,
    showcore::Property::Magenta,
    showcore::Property::Yellow,
    showcore::Property::Lime,
    showcore::Property::Indigo}};

[[nodiscard]] bool beat_to_musical_tick(
    double beat_position,
    std::int64_t& tick) noexcept {
    constexpr long double kLowest = -0x1p63L;
    constexpr long double kHighestExclusive = 0x1p63L;
    if (!std::isfinite(beat_position)) {
        return false;
    }
    const auto scaled = static_cast<long double>(beat_position) *
        static_cast<long double>(kMusicalTicksPerQuarter);
    const auto rounded = std::round(scaled);
    if (!std::isfinite(rounded) || rounded < kLowest ||
        rounded >= kHighestExclusive) {
        return false;
    }
    tick = static_cast<std::int64_t>(rounded);
    return true;
}

[[nodiscard]] showcore::AutoloopTransportState v2_transport(
    const showcore::ClockSnapshot& clock,
    bool track_playing,
    bool discontinuity = false) noexcept {
    showcore::AutoloopTransportState transport;
    transport.phase_available =
        clock.source != showcore::ClockSource::None &&
        beat_to_musical_tick(clock.beat_position, transport.musical_tick);
    transport.running = transport.phase_available;
    transport.autonomous_eligible = true;
    transport.discontinuity = discontinuity;
    // Runner's current transport does not publish a stable track epoch. Keep
    // TrackDuration explicitly degraded instead of guessing a boundary.
    transport.track_boundary_available = false;
    transport.track_active = track_playing;
    transport.track_epoch = 0U;
    return transport;
}

[[nodiscard]] showcore::AutoloopAddress next_v2_address(
    const showcore::CompiledAutoloopPackage& package,
    showcore::AutoloopAddress after,
    std::uint64_t bank_mask,
    bool reverse) noexcept {
    const auto start = after.valid()
        ? static_cast<std::size_t>(after.bank) * showcore::kAutoloopsPerBank +
              after.slot
        : (reverse ? 0U : showcore::kMaxAutoloops - 1U);
    for (std::size_t offset = 1U; offset <= showcore::kMaxAutoloops; ++offset) {
        const auto candidate = reverse
            ? (start + showcore::kMaxAutoloops - offset) %
                  showcore::kMaxAutoloops
            : (start + offset) % showcore::kMaxAutoloops;
        const auto bank = static_cast<std::uint16_t>(
            candidate / showcore::kAutoloopsPerBank);
        const showcore::AutoloopAddress address{
            bank,
            static_cast<std::uint8_t>(
                candidate % showcore::kAutoloopsPerBank)};
        const auto* placement = package.placement(address);
        if ((bank_mask & (std::uint64_t{1U} << bank)) != 0U &&
            placement != nullptr && placement->populated()) {
            return address;
        }
    }
    return {};
}

}  // namespace

struct RunnerService::RuntimeState {
    showcore::SyncManager sync{};
    showcore::StaticLookPlayer static_look{showcore::LayerId::EventMoment};
    showcore::AutoloopPlayer autonomous{showcore::LayerId::Autonomous};
    showcore::StaticLookPlayer scripted_look{showcore::LayerId::TrackScript};
    showcore::AutoloopPlayer scripted_autoloop{showcore::LayerId::TrackScript};
    showcore::AutoloopPlayer manual_autoloop{showcore::LayerId::ManualAutoloop};
    AutoloopRuntimeAdapter autoloop_v2{};
    std::array<bool, showcore::kMaxMidiMappings> mapping_toggles{};
    std::array<StaticLookBindingLease, showcore::kMaxMidiMappings>
        midi_static_look_leases{};
    std::array<StaticLookBindingLease, kMaximumCompiledLooks>
        os2l_static_look_leases{};
    std::array<std::array<float, showcore::kPropertyCount>, showcore::kMaxFixtures>
        manual_values{};
    std::array<std::array<bool, showcore::kPropertyCount>, showcore::kMaxFixtures>
        manual_override_active{};
    std::uint16_t manual_override_count{0};
    std::array<std::uint64_t, 8> tap_intervals{};
    std::size_t tap_interval_count{0};
    std::size_t tap_interval_cursor{0};
    std::uint64_t last_tap_ms{0};
    std::array<std::uint64_t, kJitterBucketCount> jitter_histogram{};
    std::uint64_t jitter_sample_count{0};
    showcore::AutoloopAddress selected_autoloop{};
    RunnerStaticLookActivation static_look_activation{};
    std::int32_t selected_track_script{-1};
    std::size_t next_track_cue{0};
    double track_script_start_beat{0.0};
    double last_track_script_beat{-1.0};
    bool track_playing{true};
    bool applied_work_light{false};
    AutoloopTrackScriptOwner legacy_track_script_owner{
        AutoloopTrackScriptOwner::None};
    bool has_v2_transport_tick{false};
    std::int64_t last_v2_transport_tick{0};
};

struct RunnerService::ActivationState {
    std::unique_ptr<CompiledShow> show;
    std::unique_ptr<RuntimeState> runtime;
    SafetySettings safety{};
    std::string project_name;
    std::uint64_t generation{0};
};

RunnerService::RunnerService() noexcept = default;

RunnerService::~RunnerService() noexcept {
    stop();
}

std::uint64_t RunnerService::monotonic_ms() noexcept {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            SteadyClock::now().time_since_epoch()).count());
}

void RunnerService::publish_static_look_status(
    const RunnerStaticLookActivation& activation) noexcept {
    static_look_status_sequence_.fetch_add(1U, std::memory_order_acq_rel);
    active_look_.store(activation.look_index, std::memory_order_relaxed);
    static_look_package_generation_.store(
        activation.package_generation, std::memory_order_relaxed);
    static_look_activation_generation_.store(
        activation.activation_generation, std::memory_order_relaxed);
    static_look_owner_kind_.store(
        activation.owner_kind, std::memory_order_relaxed);
    static_look_owner_feedback_token_.store(
        activation.owner_feedback_token, std::memory_order_relaxed);
    static_look_behavior_.store(
        activation.behavior, std::memory_order_relaxed);
    static_look_activation_status_.store(
        activation.status, std::memory_order_relaxed);
    static_look_activated_at_ms_.store(
        activation.activated_at_ms, std::memory_order_relaxed);
    const auto progress = static_cast<std::uint16_t>(std::lround(
        std::clamp(activation.transition_progress, 0.0F, 1.0F) * 1000.0F));
    static_look_transition_milli_.store(progress, std::memory_order_relaxed);
    static_look_status_sequence_.fetch_add(1U, std::memory_order_release);
}

bool RunnerService::start(
    std::unique_ptr<CompiledShow> show,
    const ProjectDocument& project) noexcept {
    if (show == nullptr || state_.load(std::memory_order_acquire) != RunnerState::Stopped) {
        return false;
    }
    state_.store(RunnerState::Starting, std::memory_order_release);
    try {
        connections_ = project.connections;
        project_id_ = project.id;
        active_activation_ = std::make_unique<ActivationState>();
        active_activation_->show = std::move(show);
        active_activation_->runtime = std::make_unique<RuntimeState>();
        active_activation_->safety = project.safety;
        active_activation_->project_name =
            project.name.empty() ? "EmberLights" : project.name;
        active_activation_->generation = 1U;
        published_activation_.store(active_activation_.get(), std::memory_order_release);
    } catch (...) {
        published_activation_.store(nullptr, std::memory_order_release);
        active_activation_.reset();
        state_.store(RunnerState::Stopped, std::memory_order_release);
        return false;
    }

    commands_.reset();
    beats_.reset();
    os2l_buttons_.reset();
    midi_actions_.reset();
    midi_monitor_.reset();
    owner_losses_.reset();
    output_queue_.reset();
    {
        const std::lock_guard<std::mutex> lock(output_snapshot_mutex_);
        latest_output_snapshot_ = {};
        has_output_snapshot_ = false;
    }
    stop_requested_.store(false, std::memory_order_release);
    blackout_requested_.store(false, std::memory_order_release);
    work_light_requested_.store(false, std::memory_order_release);
    os2l_owner_session_token_.store(0U, std::memory_order_release);
    sync_state_.store(showcore::SyncState::Waiting, std::memory_order_relaxed);
    clock_source_.store(showcore::ClockSource::None, std::memory_order_relaxed);
    bpm_milli_.store(0, std::memory_order_relaxed);
    beat_milli_.store(0, std::memory_order_relaxed);
    publish_static_look_status({});
    active_autoloop_playback_.store(kPackedInvalidAutoloopAddress, std::memory_order_relaxed);
    active_autoloop_bank_mask_.store(~std::uint64_t{0}, std::memory_order_relaxed);
    autoloop_v2_mode_.store(
        AutoloopRuntimeMode::LegacyV1, std::memory_order_relaxed);
    autoloop_v2_track_owner_.store(
        AutoloopTrackScriptOwner::None, std::memory_order_relaxed);
    autoloop_v2_track_suppressed_by_replace_.store(
        false, std::memory_order_relaxed);
    autoloop_v2_package_active_.store(false, std::memory_order_relaxed);
    autoloop_v2_generation_.store(0U, std::memory_order_relaxed);
    autoloop_v2_fault_.store(
        showcore::AutoloopDirectorFault::None, std::memory_order_relaxed);
    autoloop_v2_result_.store(
        showcore::AutoloopDirectorResult::None, std::memory_order_relaxed);
    autoloop_v2_source_.store(
        showcore::AutoloopDirectorSource::None, std::memory_order_relaxed);
    autoloop_v2_playback_mode_.store(
        showcore::CompiledAutoloopPlaybackMode::Overlay,
        std::memory_order_relaxed);
    autoloop_v2_playback_.store(
        kPackedInvalidAutoloopAddress, std::memory_order_relaxed);
    autoloop_v2_active_bank_mask_.store(
        ~std::uint64_t{0U}, std::memory_order_relaxed);
    autoloop_v2_has_pending_bank_mask_.store(
        false, std::memory_order_relaxed);
    autoloop_v2_pending_bank_mask_.store(0U, std::memory_order_relaxed);
    active_track_script_.store(-1, std::memory_order_relaxed);
    active_track_script_beat_milli_.store(0, std::memory_order_relaxed);
    active_track_script_consumed_cues_.store(0U, std::memory_order_relaxed);
    fog_armed_.store(false, std::memory_order_relaxed);
    haze_armed_.store(false, std::memory_order_relaxed);
    laser_armed_.store(false, std::memory_order_relaxed);
    spark_armed_.store(false, std::memory_order_relaxed);
    manual_override_count_.store(0U, std::memory_order_relaxed);
    frames_.store(0, std::memory_order_relaxed);
    output_frames_.store(0, std::memory_order_relaxed);
    output_queue_drops_.store(0, std::memory_order_relaxed);
    output_superseded_frames_.store(0, std::memory_order_relaxed);
    output_send_failures_.store(0, std::memory_order_relaxed);
    soundswitch_micro_write_frames_.store(0, std::memory_order_relaxed);
    soundswitch_micro_write_failures_.store(0, std::memory_order_relaxed);
    soundswitch_micro_last_error_.store(0, std::memory_order_relaxed);
    soundswitch_micro_last_nonzero_slots_.store(0, std::memory_order_relaxed);
    os2l_connections_.store(0, std::memory_order_relaxed);
    os2l_messages_.store(0, std::memory_order_relaxed);
    os2l_decode_errors_.store(0, std::memory_order_relaxed);
    os2l_feedback_messages_.store(0, std::memory_order_relaxed);
    os2l_feedback_errors_.store(0, std::memory_order_relaxed);
    os2l_blackout_feedback_synchronized_.store(false, std::memory_order_relaxed);
    os2l_listen_port_.store(0, std::memory_order_relaxed);
    os2l_last_error_.store(0, std::memory_order_relaxed);
    os2l_discovery_last_error_.store(0, std::memory_order_relaxed);
    dropped_beats_.store(0, std::memory_order_relaxed);
    dropped_os2l_actions_.store(0, std::memory_order_relaxed);
    midi_messages_.store(0, std::memory_order_relaxed);
    dropped_midi_actions_.store(0, std::memory_order_relaxed);
    const auto started_at = monotonic_ms();
    started_at_ms_.store(started_at, std::memory_order_relaxed);
    last_frame_ms_.store(0, std::memory_order_relaxed);
    jitter_samples_.store(0, std::memory_order_relaxed);
    jitter_p99_us_.store(0, std::memory_order_relaxed);
    max_jitter_us_.store(0, std::memory_order_relaxed);
    deadline_misses_.store(0, std::memory_order_relaxed);
    scheduler_resyncs_.store(0, std::memory_order_relaxed);
    scheduler_activation_ack_.store(0, std::memory_order_relaxed);
    input_activation_ack_.store(0, std::memory_order_relaxed);
    output_activation_ack_.store(0, std::memory_order_relaxed);
    package_generation_.store(1U, std::memory_order_relaxed);
    package_activations_.store(0, std::memory_order_relaxed);
    package_activation_failures_.store(0, std::memory_order_relaxed);
    os2l_state_.store(
        connections_.os2l_enabled ? AdapterState::Starting : AdapterState::Disabled,
        std::memory_order_relaxed);
    os2l_discovery_state_.store(
        connections_.os2l_enabled ? AdapterState::Starting : AdapterState::Disabled,
        std::memory_order_relaxed);
    midi_input_state_.store(
        connections_.midi_input_index >= 0 ? AdapterState::Starting : AdapterState::Disabled,
        std::memory_order_relaxed);
    midi_output_state_.store(
        connections_.midi_output_index >= 0 ? AdapterState::Starting : AdapterState::Disabled,
        std::memory_order_relaxed);
    artnet_state_.store(
        connections_.artnet_enabled ? AdapterState::Starting : AdapterState::Disabled,
        std::memory_order_relaxed);
    sacn_state_.store(
        connections_.sacn_enabled ? AdapterState::Starting : AdapterState::Disabled,
        std::memory_order_relaxed);
    for (std::size_t universe = 0; universe < dmx_usb_pro_state_.size(); ++universe) {
        dmx_usb_pro_state_[universe].store(
            connections_.dmx_usb_pro_ports[universe].empty()
                ? AdapterState::Disabled
                : AdapterState::Starting,
            std::memory_order_relaxed);
    }
    soundswitch_micro_state_.store(
        connections_.soundswitch_micro_universe == 0U
            ? AdapterState::Disabled
            : AdapterState::Starting,
        std::memory_order_relaxed);
    soundswitch_control_one_state_.store(
        connections_.soundswitch_control_one_experimental
            ? AdapterState::Starting
            : AdapterState::Disabled,
        std::memory_order_relaxed);
    output_health_[kArtNetOutputHealth].configure(
        showcore::OutputBackendKind::ArtNet, 1U, showcore::kV1UniverseCount,
        connections_.artnet_enabled);
    output_health_[kSacnOutputHealth].configure(
        showcore::OutputBackendKind::Sacn, 1U, showcore::kV1UniverseCount,
        connections_.sacn_enabled);
    output_health_[kDmxUsbProUniverseOneHealth].configure(
        showcore::OutputBackendKind::DmxUsbPro, 1U, 1U,
        !connections_.dmx_usb_pro_ports[0U].empty());
    output_health_[kDmxUsbProUniverseTwoHealth].configure(
        showcore::OutputBackendKind::DmxUsbPro, 2U, 1U,
        !connections_.dmx_usb_pro_ports[1U].empty());
    output_health_[kSoundSwitchMicroOutputHealth].configure(
        showcore::OutputBackendKind::SoundSwitchMicro,
        connections_.soundswitch_micro_universe,
        connections_.soundswitch_micro_universe == 0U ? 0U : 1U,
        connections_.soundswitch_micro_universe != 0U);
    output_health_[kSoundSwitchControlOneOutputHealth].configure(
        showcore::OutputBackendKind::SoundSwitchControlOne,
        connections_.soundswitch_control_one_experimental ? 1U : 0U,
        connections_.soundswitch_control_one_experimental
            ? showcore::kV1UniverseCount
            : 0U,
        connections_.soundswitch_control_one_experimental);

    try {
        output_thread_ = std::thread(&RunnerService::run_output, this);
        input_thread_ = std::thread(&RunnerService::run_input, this);
        scheduler_thread_ = std::thread(&RunnerService::run_scheduler, this);
    } catch (...) {
        stop_requested_.store(true, std::memory_order_release);
        if (scheduler_thread_.joinable()) {
            scheduler_thread_.join();
        }
        if (input_thread_.joinable()) {
            input_thread_.join();
        }
        if (output_thread_.joinable()) {
            output_thread_.join();
        }
        published_activation_.store(nullptr, std::memory_order_release);
        active_activation_.reset();
        state_.store(RunnerState::Stopped, std::memory_order_release);
        return false;
    }
    return true;
}

RunnerActivationResult RunnerService::activate(
    std::unique_ptr<CompiledShow> show,
    const ProjectDocument& project,
    std::uint32_t timeout_ms) noexcept {
    if (show == nullptr) {
        package_activation_failures_.fetch_add(1U, std::memory_order_relaxed);
        return {RunnerActivationError::InvalidShow, package_generation_.load()};
    }
    if (state_.load(std::memory_order_acquire) != RunnerState::Running) {
        package_activation_failures_.fetch_add(1U, std::memory_order_relaxed);
        return {RunnerActivationError::NotRunning, package_generation_.load()};
    }
    if (project.connections != connections_ || project.id != project_id_) {
        package_activation_failures_.fetch_add(1U, std::memory_order_relaxed);
        return {RunnerActivationError::RestartRequired, package_generation_.load()};
    }

    std::unique_lock lock(activation_mutex_, std::try_to_lock);
    if (!lock.owns_lock() || pending_activation_ != nullptr) {
        package_activation_failures_.fetch_add(1U, std::memory_order_relaxed);
        return {RunnerActivationError::Busy, package_generation_.load()};
    }
    try {
        pending_activation_ = std::make_unique<ActivationState>();
        pending_activation_->show = std::move(show);
        pending_activation_->runtime = std::make_unique<RuntimeState>();
        pending_activation_->safety = project.safety;
        pending_activation_->project_name =
            project.name.empty() ? "EmberLights" : project.name;
        pending_activation_->generation =
            package_generation_.load(std::memory_order_relaxed) + 1U;
    } catch (...) {
        pending_activation_.reset();
        package_activation_failures_.fetch_add(1U, std::memory_order_relaxed);
        return {RunnerActivationError::InvalidShow, package_generation_.load()};
    }

    const auto generation = pending_activation_->generation;
    published_activation_.store(pending_activation_.get(), std::memory_order_release);
    lock.unlock();

    const auto deadline = SteadyClock::now() + std::chrono::milliseconds(timeout_ms);
    while (state_.load(std::memory_order_acquire) == RunnerState::Running &&
           SteadyClock::now() < deadline) {
        if (scheduler_activation_ack_.load(std::memory_order_acquire) >= generation &&
            input_activation_ack_.load(std::memory_order_acquire) >= generation &&
            output_activation_ack_.load(std::memory_order_acquire) >= generation) {
            std::unique_ptr<ActivationState> retired;
            {
                std::lock_guard ownership_lock(activation_mutex_);
                retired = std::move(active_activation_);
                active_activation_ = std::move(pending_activation_);
            }
            package_generation_.store(generation, std::memory_order_release);
            package_activations_.fetch_add(1U, std::memory_order_relaxed);
            retired.reset();
            return {RunnerActivationError::None, generation};
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    package_activation_failures_.fetch_add(1U, std::memory_order_relaxed);
    return {RunnerActivationError::Timeout, generation};
}

void RunnerService::stop() noexcept {
    const auto current = state_.load(std::memory_order_acquire);
    if (current == RunnerState::Stopped) {
        return;
    }
    state_.store(RunnerState::Stopping, std::memory_order_release);
    stop_requested_.store(true, std::memory_order_release);
    if (input_thread_.joinable()) {
        input_thread_.join();
    }
    if (scheduler_thread_.joinable()) {
        scheduler_thread_.join();
    }
    if (output_thread_.joinable()) {
        output_thread_.join();
    }
    published_activation_.store(nullptr, std::memory_order_release);
    {
        std::lock_guard lock(activation_mutex_);
        pending_activation_.reset();
        active_activation_.reset();
    }
    manual_override_count_.store(0U, std::memory_order_relaxed);
    publish_static_look_status({});
    state_.store(RunnerState::Stopped, std::memory_order_release);
}

RunnerStatus RunnerService::status() const noexcept {
    RunnerStatus snapshot;
    snapshot.state = state_.load(std::memory_order_acquire);
    snapshot.os2l = os2l_state_.load(std::memory_order_relaxed);
    snapshot.os2l_discovery =
        os2l_discovery_state_.load(std::memory_order_relaxed);
    snapshot.midi_input = midi_input_state_.load(std::memory_order_relaxed);
    snapshot.midi_output = midi_output_state_.load(std::memory_order_relaxed);
    snapshot.artnet = artnet_state_.load(std::memory_order_relaxed);
    snapshot.sacn = sacn_state_.load(std::memory_order_relaxed);
    for (std::size_t universe = 0; universe < snapshot.dmx_usb_pro.size(); ++universe) {
        snapshot.dmx_usb_pro[universe] =
            dmx_usb_pro_state_[universe].load(std::memory_order_relaxed);
    }
    snapshot.soundswitch_micro =
        soundswitch_micro_state_.load(std::memory_order_relaxed);
    snapshot.soundswitch_control_one =
        soundswitch_control_one_state_.load(std::memory_order_relaxed);
    for (std::size_t index = 0U; index < snapshot.output_backends.size(); ++index) {
        snapshot.output_backends[index] = output_health_[index].snapshot();
    }
    snapshot.sync_state = sync_state_.load(std::memory_order_relaxed);
    snapshot.clock_source = clock_source_.load(std::memory_order_relaxed);
    snapshot.bpm = static_cast<double>(bpm_milli_.load(std::memory_order_relaxed)) / 1000.0;
    snapshot.beat_position =
        static_cast<double>(beat_milli_.load(std::memory_order_relaxed)) / 1000.0;
    std::uint32_t static_look_status_spins = 0U;
    for (;;) {
        const auto before =
            static_look_status_sequence_.load(std::memory_order_acquire);
        if ((before & 1U) != 0U) {
            if (++static_look_status_spins == 16U) {
                std::this_thread::yield();
                static_look_status_spins = 0U;
            }
            continue;
        }
        snapshot.static_look.look_index =
            active_look_.load(std::memory_order_relaxed);
        snapshot.static_look.package_generation =
            static_look_package_generation_.load(std::memory_order_relaxed);
        snapshot.static_look.activation_generation =
            static_look_activation_generation_.load(std::memory_order_relaxed);
        snapshot.static_look.owner_kind =
            static_look_owner_kind_.load(std::memory_order_relaxed);
        snapshot.static_look.owner_feedback_token =
            static_look_owner_feedback_token_.load(std::memory_order_relaxed);
        snapshot.static_look.behavior =
            static_look_behavior_.load(std::memory_order_relaxed);
        snapshot.static_look.status =
            static_look_activation_status_.load(std::memory_order_relaxed);
        snapshot.static_look.activated_at_ms =
            static_look_activated_at_ms_.load(std::memory_order_relaxed);
        snapshot.static_look.transition_progress = static_cast<float>(
            static_look_transition_milli_.load(std::memory_order_relaxed)) /
            1000.0F;
        const auto after =
            static_look_status_sequence_.load(std::memory_order_acquire);
        if (before == after) {
            break;
        }
        if (++static_look_status_spins == 16U) {
            std::this_thread::yield();
            static_look_status_spins = 0U;
        }
    }
    snapshot.active_look = snapshot.static_look.look_index;
    const auto active_autoloop_playback =
        active_autoloop_playback_.load(std::memory_order_relaxed);
    const auto encoded_autoloop = static_cast<std::uint16_t>(
        active_autoloop_playback & kPackedAutoloopAddressMask);
    snapshot.active_autoloop = encoded_autoloop < showcore::kMaxAutoloops
        ? decode_autoloop(encoded_autoloop)
        : showcore::AutoloopAddress{};
    snapshot.active_autoloop_repeat = static_cast<showcore::AutoloopRepeat>(
        (active_autoloop_playback >> kPackedAutoloopRepeatShift) & 0x03U);
    snapshot.active_autoloop_progress = static_cast<float>(
        (active_autoloop_playback >> kPackedAutoloopProgressShift) & 0x03FFU) / 1000.0F;
    snapshot.active_autoloop_completed_cycles = static_cast<std::uint32_t>(
        active_autoloop_playback >> kPackedAutoloopCycleShift);
    snapshot.active_autoloop_bank_mask =
        active_autoloop_bank_mask_.load(std::memory_order_relaxed);
    snapshot.active_track_script = active_track_script_.load(std::memory_order_relaxed);
    snapshot.active_track_script_beat = static_cast<double>(
        active_track_script_beat_milli_.load(std::memory_order_relaxed)) / 1000.0;
    snapshot.active_track_script_consumed_cues =
        active_track_script_consumed_cues_.load(std::memory_order_relaxed);
    snapshot.blackout = blackout_requested_.load(std::memory_order_relaxed);
    snapshot.work_light = work_light_requested_.load(std::memory_order_relaxed);
    snapshot.fog_armed = fog_armed_.load(std::memory_order_relaxed);
    snapshot.haze_armed = haze_armed_.load(std::memory_order_relaxed);
    snapshot.laser_armed = laser_armed_.load(std::memory_order_relaxed);
    snapshot.spark_armed = spark_armed_.load(std::memory_order_relaxed);
    snapshot.manual_override_count = manual_override_count_.load(std::memory_order_relaxed);
    snapshot.frames = frames_.load(std::memory_order_relaxed);
    snapshot.output_frames = output_frames_.load(std::memory_order_relaxed);
    snapshot.output_queue_drops = output_queue_drops_.load(std::memory_order_relaxed);
    snapshot.output_superseded_frames =
        output_superseded_frames_.load(std::memory_order_relaxed);
    snapshot.output_send_failures = output_send_failures_.load(std::memory_order_relaxed);
    snapshot.soundswitch_micro_write_frames =
        soundswitch_micro_write_frames_.load(std::memory_order_relaxed);
    snapshot.soundswitch_micro_write_failures =
        soundswitch_micro_write_failures_.load(std::memory_order_relaxed);
    snapshot.soundswitch_micro_last_error =
        soundswitch_micro_last_error_.load(std::memory_order_relaxed);
    snapshot.soundswitch_micro_last_nonzero_slots =
        soundswitch_micro_last_nonzero_slots_.load(std::memory_order_relaxed);
    snapshot.os2l_connections = os2l_connections_.load(std::memory_order_relaxed);
    snapshot.os2l_messages = os2l_messages_.load(std::memory_order_relaxed);
    snapshot.os2l_decode_errors = os2l_decode_errors_.load(std::memory_order_relaxed);
    snapshot.os2l_feedback_messages =
        os2l_feedback_messages_.load(std::memory_order_relaxed);
    snapshot.os2l_feedback_errors =
        os2l_feedback_errors_.load(std::memory_order_relaxed);
    snapshot.os2l_blackout_feedback_synchronized =
        os2l_blackout_feedback_synchronized_.load(std::memory_order_relaxed);
    snapshot.os2l_listen_port = os2l_listen_port_.load(std::memory_order_relaxed);
    snapshot.os2l_last_error = os2l_last_error_.load(std::memory_order_relaxed);
    snapshot.os2l_discovery_last_error =
        os2l_discovery_last_error_.load(std::memory_order_relaxed);
    snapshot.dropped_beats = dropped_beats_.load(std::memory_order_relaxed);
    snapshot.dropped_os2l_actions =
        dropped_os2l_actions_.load(std::memory_order_relaxed);
    snapshot.midi_messages = midi_messages_.load(std::memory_order_relaxed);
    snapshot.dropped_midi_actions = dropped_midi_actions_.load(std::memory_order_relaxed);
    const auto now_ms = monotonic_ms();
    const auto started_at = started_at_ms_.load(std::memory_order_relaxed);
    const auto last_frame = last_frame_ms_.load(std::memory_order_relaxed);
    snapshot.uptime_ms = started_at != 0U && now_ms >= started_at ? now_ms - started_at : 0U;
    snapshot.last_frame_age_ms = last_frame != 0U && now_ms >= last_frame
        ? now_ms - last_frame
        : 0U;
    snapshot.jitter_samples = jitter_samples_.load(std::memory_order_relaxed);
    snapshot.jitter_p99_us = jitter_p99_us_.load(std::memory_order_relaxed);
    snapshot.max_jitter_us = max_jitter_us_.load(std::memory_order_relaxed);
    snapshot.deadline_misses = deadline_misses_.load(std::memory_order_relaxed);
    snapshot.scheduler_resyncs = scheduler_resyncs_.load(std::memory_order_relaxed);
    snapshot.package_generation = package_generation_.load(std::memory_order_relaxed);
    snapshot.package_activations = package_activations_.load(std::memory_order_relaxed);
    snapshot.package_activation_failures =
        package_activation_failures_.load(std::memory_order_relaxed);
    snapshot.autoloop_v2.mode =
        autoloop_v2_mode_.load(std::memory_order_relaxed);
    snapshot.autoloop_v2.track_script_owner =
        autoloop_v2_track_owner_.load(std::memory_order_relaxed);
    snapshot.autoloop_v2.track_script_suppressed_by_replace =
        autoloop_v2_track_suppressed_by_replace_.load(
            std::memory_order_relaxed);
    snapshot.autoloop_v2.package_active =
        autoloop_v2_package_active_.load(std::memory_order_relaxed);
    snapshot.autoloop_v2.package_generation =
        autoloop_v2_generation_.load(std::memory_order_relaxed);
    snapshot.autoloop_v2.fault =
        autoloop_v2_fault_.load(std::memory_order_relaxed);
    snapshot.autoloop_v2.last_result =
        autoloop_v2_result_.load(std::memory_order_relaxed);
    snapshot.autoloop_v2.active_source =
        autoloop_v2_source_.load(std::memory_order_relaxed);
    snapshot.autoloop_v2.active_mode =
        autoloop_v2_playback_mode_.load(std::memory_order_relaxed);
    const auto v2_playback =
        autoloop_v2_playback_.load(std::memory_order_relaxed);
    const auto v2_address = static_cast<std::uint16_t>(
        v2_playback & kPackedAutoloopAddressMask);
    snapshot.autoloop_v2.active_address =
        v2_address < showcore::kMaxAutoloops
        ? decode_autoloop(v2_address)
        : showcore::AutoloopAddress{};
    snapshot.autoloop_v2.active_repeat =
        static_cast<showcore::AutoloopRepeat>(
            (v2_playback >> kPackedAutoloopRepeatShift) & 0x03U);
    snapshot.autoloop_v2.active_progress = static_cast<float>(
        (v2_playback >> kPackedAutoloopProgressShift) & 0x03FFU) /
        1000.0F;
    snapshot.autoloop_v2.active_completed_cycles =
        static_cast<std::uint32_t>(
            v2_playback >> kPackedAutoloopCycleShift);
    snapshot.autoloop_v2.active_bank_mask =
        autoloop_v2_active_bank_mask_.load(std::memory_order_relaxed);
    snapshot.autoloop_v2.has_pending_bank_mask =
        autoloop_v2_has_pending_bank_mask_.load(std::memory_order_relaxed);
    snapshot.autoloop_v2.pending_bank_mask =
        autoloop_v2_pending_bank_mask_.load(std::memory_order_relaxed);
    return snapshot;
}

bool RunnerService::latest_output_snapshot(
    RunnerOutputSnapshot& snapshot) const noexcept {
    const std::lock_guard<std::mutex> lock(output_snapshot_mutex_);
    if (!has_output_snapshot_) {
        return false;
    }
    snapshot = latest_output_snapshot_;
    return true;
}

bool RunnerService::post(const RunnerCommand& command) noexcept {
    if (state_.load(std::memory_order_acquire) != RunnerState::Running) {
        return false;
    }
    auto versioned = command;
    versioned.generation = package_generation_.load(std::memory_order_acquire);
    return commands_.try_push(versioned);
}

void RunnerService::set_blackout(bool active) noexcept {
    blackout_requested_.store(active, std::memory_order_release);
}

void RunnerService::set_work_light(bool active) noexcept {
    work_light_requested_.store(active, std::memory_order_release);
}

bool RunnerService::trigger_look(
    std::uint16_t index,
    StaticLookOwnerContext owner) noexcept {
    RunnerCommand command;
    command.type = RunnerCommandType::TriggerLook;
    command.target = index;
    command.static_look_owner = owner;
    command.static_look_behavior = StaticLookBehavior::Explicit;
    return post(command);
}

bool RunnerService::toggle_look(
    std::uint16_t index,
    StaticLookOwnerContext owner) noexcept {
    RunnerCommand command;
    command.type = RunnerCommandType::ToggleLook;
    command.target = index;
    command.static_look_owner = owner;
    command.static_look_behavior = StaticLookBehavior::Latch;
    return post(command);
}

bool RunnerService::hold_look(
    std::uint16_t index,
    bool active,
    StaticLookOwnerContext owner) noexcept {
    if (owner.kind == StaticLookOwnerKind::None ||
        owner.feedback_token == 0U ||
        (!active && (owner.expected_package_generation == 0U ||
                     owner.expected_activation_generation == 0U))) {
        return false;
    }
    RunnerCommand command;
    command.type = RunnerCommandType::SetLookHeld;
    command.target = index;
    command.active = active;
    command.static_look_owner = owner;
    command.static_look_behavior = StaticLookBehavior::Hold;
    return post(command);
}

bool RunnerService::notify_static_look_owner_lost(
    StaticLookOwnerKind kind,
    std::uint64_t owner_session_token,
    std::uint64_t owner_feedback_token) noexcept {
    if (kind == StaticLookOwnerKind::None || owner_session_token == 0U) {
        return false;
    }
    RunnerCommand command;
    command.type = RunnerCommandType::StaticLookOwnerLost;
    command.static_look_owner.kind = kind;
    command.static_look_owner.feedback_token = owner_feedback_token;
    command.static_look_owner.owner_session_token = owner_session_token;
    return post(command);
}

bool RunnerService::clear_look() noexcept {
    return post({RunnerCommandType::ClearLook});
}

bool RunnerService::trigger_autoloop(showcore::AutoloopAddress address) noexcept {
    return address.valid() && post({RunnerCommandType::TriggerAutoloop, encode_autoloop(address)});
}

bool RunnerService::clear_autoloop() noexcept {
    return post({RunnerCommandType::ClearAutoloop});
}

bool RunnerService::next_autoloop() noexcept {
    return post({RunnerCommandType::NextAutoloop});
}

bool RunnerService::previous_autoloop() noexcept {
    return post({RunnerCommandType::PreviousAutoloop});
}

bool RunnerService::select_all_autoloop_banks() noexcept {
    return post({RunnerCommandType::SelectAllAutoloopBanks});
}

bool RunnerService::select_exclusive_autoloop_bank(std::uint16_t bank) noexcept {
    return bank < showcore::kMaxAutoloopBanks &&
        post({RunnerCommandType::SelectExclusiveAutoloopBank, bank});
}

bool RunnerService::set_autoloop_bank_enabled(std::uint16_t bank, bool enabled) noexcept {
    return bank < showcore::kMaxAutoloopBanks &&
        post({RunnerCommandType::SetAutoloopBankEnabled, bank, {}, 0.0F, enabled});
}

bool RunnerService::trigger_track_script(std::uint16_t index) noexcept {
    return post({RunnerCommandType::TriggerTrackScript, index});
}

bool RunnerService::clear_track_script() noexcept {
    return post({RunnerCommandType::ClearTrackScript});
}

bool RunnerService::set_manual_bpm(double bpm) noexcept {
    if (!std::isfinite(bpm) || bpm < 20.0 || bpm > 300.0) {
        return false;
    }
    RunnerCommand command;
    command.type = RunnerCommandType::SetManualBpm;
    command.value = static_cast<float>(bpm);
    command.timestamp_ms = monotonic_ms();
    return post(command);
}

bool RunnerService::tap_tempo() noexcept {
    RunnerCommand command;
    command.type = RunnerCommandType::TapTempo;
    command.timestamp_ms = monotonic_ms();
    return post(command);
}

bool RunnerService::set_property(
    std::uint16_t fixture_id,
    showcore::Property property,
    float value,
    bool active) noexcept {
    if (property >= showcore::Property::Count || !std::isfinite(value)) {
        return false;
    }
    RunnerCommand command;
    command.type = RunnerCommandType::SetProperty;
    command.target = fixture_id;
    command.property = property;
    command.value = std::clamp(value, 0.0F, 1.0F);
    command.active = active;
    return post(command);
}

bool RunnerService::clear_manual_overrides() noexcept {
    return post({RunnerCommandType::ClearManualOverrides});
}

bool RunnerService::set_group_property(
    const showcore::FixtureGroup& fixtures,
    showcore::Property property,
    float value,
    bool active) noexcept {
    if (fixtures.count == 0U || fixtures.count > fixtures.fixture_ids.size() ||
        property >= showcore::Property::Count || !std::isfinite(value)) {
        return false;
    }
    RunnerCommand command;
    command.type = RunnerCommandType::SetGroupProperty;
    command.property = property;
    command.value = std::clamp(value, 0.0F, 1.0F);
    command.active = active;
    for (std::size_t index = 0U; index < fixtures.count; ++index) {
        const auto fixture = fixtures.fixture_ids[index];
        if (fixture >= showcore::kMaxFixtures) {
            return false;
        }
        command.fixture_mask[fixture / 64U] |= std::uint64_t{1} << (fixture % 64U);
    }
    return post(command);
}

bool RunnerService::set_hazard_armed(
    showcore::Property property,
    bool armed) noexcept {
    if (!is_hazard(property)) {
        return false;
    }
    RunnerCommand command;
    command.type = RunnerCommandType::ArmHazard;
    command.property = property;
    command.active = armed;
    return post(command);
}

bool RunnerService::poll_midi_monitor(RunnerMidiMonitorEvent& event) noexcept {
    return midi_monitor_.try_pop(event);
}

void RunnerService::os2l_callback(
    const showcore::Os2lEvent& event,
    showcore::Os2lParseError error,
    std::string_view,
    void* context) noexcept {
    auto& service = *static_cast<RunnerService*>(context);
    if (error != showcore::Os2lParseError::None) {
        return;
    }
    if (event.kind == showcore::Os2lKind::Beat) {
        const BeatEvent beat{event.beat.position, event.beat.bpm, monotonic_ms()};
        if (!service.beats_.try_push(beat)) {
            service.dropped_beats_.fetch_add(1, std::memory_order_relaxed);
        }
    } else if (event.kind == showcore::Os2lKind::Button) {
        // Reserved no-op used only to wake VirtualDJ's direct-IP OS2L client.
        // It must never change live output state.
        if (event.button.name.view() == "EmberLights Keepalive" ||
            event.button.name.view() == "emberlights.keepalive") {
            return;
        }
        if (event.button.name.view() == "blackout") {
            service.set_blackout(event.button.on);
        } else if (event.button.name.view() == "worklight" ||
                   event.button.name.view() == "white") {
            service.set_work_light(event.button.on);
        } else if (const auto* activation =
                       service.published_activation_.load(std::memory_order_acquire);
                   activation != nullptr) {
            const RunnerOs2lButtonEvent button{
                event.button.name,
                event.button.on,
                activation->generation,
                service.os2l_owner_session_token_.load(
                    std::memory_order_acquire)};
            if (!service.os2l_buttons_.try_push(button)) {
                service.dropped_os2l_actions_.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }
}

void RunnerService::run_input() noexcept {
    showcore::Os2lTcpServer os2l;
    showcore::WinMmMidiInput midi_input;
    showcore::WinMmMidiOutput midi_output;
    bool os2l_open = false;
    bool midi_input_open = false;
    bool midi_output_open = false;
    std::uint64_t os2l_owner_session = 0U;
    std::uint64_t midi_owner_session = 0U;
    auto next_os2l_retry = SteadyClock::now();
    auto next_midi_retry = SteadyClock::now();
    auto next_midi_health_probe = SteadyClock::now();

    auto next_owner_session = [&]() noexcept {
        auto next = input_owner_session_sequence_.fetch_add(
                        1U, std::memory_order_relaxed) +
            1U;
        if (next == 0U) {
            input_owner_session_sequence_.store(1U, std::memory_order_relaxed);
            next = 1U;
        }
        return next;
    };
    auto queue_owner_loss = [&](StaticLookOwnerKind kind,
                                std::uint64_t owner_session,
                                std::uint64_t generation) noexcept {
        if (owner_session == 0U) {
            return;
        }
        if (!owner_losses_.try_push(
                {kind, owner_session, 0U, generation})) {
            if (kind == StaticLookOwnerKind::Midi) {
                dropped_midi_actions_.fetch_add(1U, std::memory_order_relaxed);
            } else {
                dropped_os2l_actions_.fetch_add(1U, std::memory_order_relaxed);
            }
        }
    };

    os2l_owner_session_token_.store(0U, std::memory_order_release);

    while (!stop_requested_.load(std::memory_order_acquire)) {
        auto* activation = published_activation_.load(std::memory_order_acquire);
        if (activation == nullptr) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        input_activation_ack_.store(activation->generation, std::memory_order_release);
        const auto now = SteadyClock::now();
        if (connections_.os2l_enabled && !os2l_open && now >= next_os2l_retry) {
            os2l_state_.store(AdapterState::Starting, std::memory_order_relaxed);
            os2l_open = os2l.open_ipv4(connections_.os2l_bind, connections_.os2l_port);
            os2l_listen_port_.store(
                os2l_open ? os2l.bound_port() : 0U,
                std::memory_order_relaxed);
            os2l_last_error_.store(os2l.last_error(), std::memory_order_relaxed);
            os2l_discovery_state_.store(
                adapter_state(os2l.discovery_state()), std::memory_order_relaxed);
            os2l_discovery_last_error_.store(
                os2l.discovery_last_error(), std::memory_order_relaxed);
            os2l_state_.store(
                os2l_open ? AdapterState::Waiting : AdapterState::Fault,
                std::memory_order_relaxed);
            next_os2l_retry = now + std::chrono::seconds(2);
        }
        if (os2l_open) {
            const auto blackout =
                blackout_requested_.load(std::memory_order_acquire);
            if (os2l.state() == showcore::Os2lServerState::ClientConnected) {
                static_cast<void>(os2l.queue_blackout_feedback(blackout));
            }
            const auto poll = os2l.poll(&RunnerService::os2l_callback, this, 2);
            if (poll == showcore::Os2lPollResult::ClientConnected) {
                os2l_owner_session = next_owner_session();
                os2l_owner_session_token_.store(
                    os2l_owner_session, std::memory_order_release);
                static_cast<void>(os2l.queue_blackout_feedback(
                    blackout_requested_.load(std::memory_order_acquire)));
            } else if (poll == showcore::Os2lPollResult::ClientDisconnected) {
                queue_owner_loss(
                    StaticLookOwnerKind::External,
                    os2l_owner_session,
                    activation->generation);
                os2l_owner_session = 0U;
                os2l_owner_session_token_.store(0U, std::memory_order_release);
            }
            if (poll == showcore::Os2lPollResult::Error) {
                const auto discovery_state = adapter_state(os2l.discovery_state());
                const auto discovery_error = os2l.discovery_last_error();
                const auto socket_error = os2l.last_error();
                queue_owner_loss(
                    StaticLookOwnerKind::External,
                    os2l_owner_session,
                    activation->generation);
                os2l_owner_session = 0U;
                os2l_owner_session_token_.store(0U, std::memory_order_release);
                os2l.close();
                os2l_open = false;
                os2l_blackout_feedback_synchronized_.store(
                    false, std::memory_order_relaxed);
                os2l_listen_port_.store(0U, std::memory_order_relaxed);
                os2l_last_error_.store(socket_error, std::memory_order_relaxed);
                os2l_discovery_state_.store(
                    discovery_state, std::memory_order_relaxed);
                os2l_discovery_last_error_.store(
                    discovery_error, std::memory_order_relaxed);
                os2l_state_.store(AdapterState::Fault, std::memory_order_relaxed);
                next_os2l_retry = now + std::chrono::seconds(2);
            } else {
                os2l_state_.store(
                    os2l.state() == showcore::Os2lServerState::ClientConnected
                        ? AdapterState::Ready
                        : AdapterState::Waiting,
                    std::memory_order_relaxed);
            }
            const auto& stats = os2l.stats();
            if (os2l_open) {
                os2l_discovery_state_.store(
                    adapter_state(os2l.discovery_state()), std::memory_order_relaxed);
                os2l_discovery_last_error_.store(
                    os2l.discovery_last_error(), std::memory_order_relaxed);
            }
            os2l_connections_.store(stats.connections, std::memory_order_relaxed);
            os2l_messages_.store(stats.messages, std::memory_order_relaxed);
            os2l_decode_errors_.store(stats.decode_errors, std::memory_order_relaxed);
            os2l_feedback_messages_.store(
                stats.feedback_messages, std::memory_order_relaxed);
            os2l_feedback_errors_.store(
                stats.feedback_errors, std::memory_order_relaxed);
            os2l_blackout_feedback_synchronized_.store(
                os2l.blackout_feedback_synchronized(
                    blackout_requested_.load(std::memory_order_acquire)),
                std::memory_order_relaxed);
        }

        if (now >= next_midi_retry) {
            if (connections_.midi_input_index >= 0 && !midi_input_open) {
                midi_input_state_.store(AdapterState::Starting, std::memory_order_relaxed);
                midi_input_open = midi_input.open(
                    static_cast<std::uint32_t>(connections_.midi_input_index), 1U);
                if (midi_input_open) {
                    midi_owner_session = next_owner_session();
                    next_midi_health_probe = now + std::chrono::milliseconds(250);
                }
                midi_input_state_.store(
                    midi_input_open ? AdapterState::Ready : AdapterState::Fault,
                    std::memory_order_relaxed);
            }
            if (connections_.midi_output_index >= 0 && !midi_output_open) {
                midi_output_state_.store(AdapterState::Starting, std::memory_order_relaxed);
                midi_output_open = midi_output.open(
                    static_cast<std::uint32_t>(connections_.midi_output_index), 1U);
                midi_output_state_.store(
                    midi_output_open ? AdapterState::Ready : AdapterState::Fault,
                    std::memory_order_relaxed);
            }
            next_midi_retry = now + std::chrono::seconds(2);
        }

        if (midi_input_open && now >= next_midi_health_probe) {
            if (!midi_input.connected(1U)) {
                queue_owner_loss(
                    StaticLookOwnerKind::Midi,
                    midi_owner_session,
                    activation->generation);
                static_cast<void>(midi_input.close(1U));
                midi_input_open = false;
                midi_owner_session = 0U;
                midi_input_state_.store(
                    AdapterState::Fault, std::memory_order_relaxed);
                next_midi_retry = now + std::chrono::seconds(2);
            }
            next_midi_health_probe = now + std::chrono::milliseconds(250);
        }

        if (midi_input_open) {
            showcore::MidiMessage message;
            while (midi_input.poll(message)) {
                midi_messages_.fetch_add(1, std::memory_order_relaxed);
                static_cast<void>(midi_monitor_.try_push({message}));
                std::array<showcore::MidiActionEvent, showcore::kMaxMidiActionsPerMessage> actions{};
                const auto count = activation->show->midi_mappings().process(message, actions);
                for (std::size_t index = 0; index < count; ++index) {
                    if (!midi_actions_.try_push({
                            actions[index],
                            activation->generation,
                            midi_owner_session})) {
                        dropped_midi_actions_.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    os2l.close();
    os2l_blackout_feedback_synchronized_.store(false, std::memory_order_relaxed);
    os2l_owner_session_token_.store(0U, std::memory_order_release);
    os2l_listen_port_.store(0U, std::memory_order_relaxed);
    os2l_discovery_state_.store(AdapterState::Disabled, std::memory_order_relaxed);
    midi_input.close_all();
    midi_output.close_all();
}

void RunnerService::run_scheduler() noexcept {
#ifdef _WIN32
    static_cast<void>(::timeBeginPeriod(1U));
#endif
    const auto frame_period = std::chrono::microseconds(1'000'000 / connections_.frame_rate);
    auto next_frame = SteadyClock::now();
    std::uint8_t sequence = 1U;
    ActivationState* activation = nullptr;

    auto configure_activation = [&](ActivationState* next,
                                    ActivationState* previous,
                                    std::uint64_t now_ms) noexcept {
        auto& runtime = *next->runtime;
        auto& engine = next->show->engine();
        auto& autoloops = next->show->autoloops();
        const auto* v2_package = next->show->autoloop_v2_package();
        const bool v2_enabled = v2_package != nullptr;
        showcore::AutoloopAddress selected_autoloop{};
        std::uint64_t active_bank_mask = ~std::uint64_t{0};
        if (previous != nullptr) {
            const auto& prior = *previous->runtime;
            runtime.sync = prior.sync;
            runtime.tap_intervals = prior.tap_intervals;
            runtime.tap_interval_count = prior.tap_interval_count;
            runtime.tap_interval_cursor = prior.tap_interval_cursor;
            runtime.last_tap_ms = prior.last_tap_ms;
            runtime.jitter_histogram = prior.jitter_histogram;
            runtime.jitter_sample_count = prior.jitter_sample_count;
            runtime.track_playing = prior.track_playing;
            if (!v2_enabled &&
                previous->show->autoloop_v2_package() == nullptr) {
                selected_autoloop = prior.selected_autoloop;
                active_bank_mask =
                    previous->show->autoloops().active_bank_mask();
            }
        } else {
            runtime.sync.set_manual_bpm(connections_.manual_bpm, now_ms);
        }
        for (std::uint16_t bank = 0U; bank < showcore::kMaxAutoloopBanks; ++bank) {
            static_cast<void>(autoloops.set_bank_enabled(
                bank, (active_bank_mask & (std::uint64_t{1} << bank)) != 0U));
        }
        active_autoloop_bank_mask_.store(
            autoloops.active_bank_mask(), std::memory_order_relaxed);
        manual_override_count_.store(0U, std::memory_order_relaxed);

        engine.safety().strobe_allowed = next->safety.strobe_allowed;
        engine.safety().max_strobe = next->safety.max_strobe;
        engine.safety().max_intensity = next->safety.max_intensity;
        engine.safety().fog_armed = !next->safety.fog_requires_arm;
        engine.safety().haze_armed = !next->safety.haze_requires_arm;
        engine.safety().laser_armed = !next->safety.laser_requires_arm;
        engine.safety().spark_armed = !next->safety.spark_requires_arm;
        fog_armed_.store(engine.safety().fog_armed, std::memory_order_relaxed);
        haze_armed_.store(engine.safety().haze_armed, std::memory_order_relaxed);
        laser_armed_.store(engine.safety().laser_armed, std::memory_order_relaxed);
        spark_armed_.store(engine.safety().spark_armed, std::memory_order_relaxed);

        const auto clock = runtime.sync.tick(now_ms);
        if (v2_enabled) {
            static_cast<void>(runtime.autoloop_v2.activate_package(
                v2_package, next->generation, engine.layers()));
        } else {
            const auto default_address =
                next->show->autoloops().next_available();
            if (default_address.valid()) {
                static_cast<void>(runtime.autonomous.trigger(
                    next->show->autoloops(),
                    default_address,
                    showcore::AutoloopRepeat::Infinite,
                    clock.beat_position,
                    runtime.track_playing,
                    engine.layers()));
            }
        }
        if (!v2_enabled && selected_autoloop.valid() &&
            next->show->autoloops().get(selected_autoloop) != nullptr &&
            runtime.manual_autoloop.trigger(
                next->show->autoloops(),
                selected_autoloop,
                next->show->autoloop_repeat(selected_autoloop),
                clock.beat_position,
                runtime.track_playing,
                engine.layers())) {
            runtime.selected_autoloop = selected_autoloop;
        }
        runtime.applied_work_light = false;
        // V0 intentionally does not preserve an activation across package
        // generations. Stable-ID/content compatibility is not yet compiled
        // into the activation contract, so clearing is the only safe policy.
        publish_static_look_status(runtime.static_look_activation);
    };

    activation = published_activation_.load(std::memory_order_acquire);
    if (activation == nullptr) {
        state_.store(RunnerState::Fault, std::memory_order_release);
#ifdef _WIN32
        static_cast<void>(::timeEndPeriod(1U));
#endif
        return;
    }
    configure_activation(activation, nullptr, monotonic_ms());
    scheduler_activation_ack_.store(activation->generation, std::memory_order_release);
    state_.store(RunnerState::Running, std::memory_order_release);

    while (!stop_requested_.load(std::memory_order_acquire)) {
        std::this_thread::sleep_until(next_frame);
        const auto now = SteadyClock::now();
        const auto now_ms = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count());
        if (auto* requested = published_activation_.load(std::memory_order_acquire);
            requested != nullptr && requested != activation) {
            configure_activation(requested, activation, now_ms);
            activation = requested;
            scheduler_activation_ack_.store(
                activation->generation, std::memory_order_release);
        }
        auto& runtime = *activation->runtime;
        auto& engine = activation->show->engine();
        auto* show = activation->show.get();
        const auto* v2_package = show->autoloop_v2_package();
        const bool v2_enabled = v2_package != nullptr;

        std::uint64_t late_us = 0U;
        if (now > next_frame) {
            late_us = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    now - next_frame).count());
            update_maximum(max_jitter_us_, late_us);
        }
        ++runtime.jitter_sample_count;
        const auto jitter_bucket = std::min<std::size_t>(
            static_cast<std::size_t>(late_us / kJitterBucketWidthUs),
            runtime.jitter_histogram.size() - 1U);
        ++runtime.jitter_histogram[jitter_bucket];
        jitter_samples_.store(runtime.jitter_sample_count, std::memory_order_relaxed);
        if (late_us > kDeadlineMissUs) {
            deadline_misses_.fetch_add(1U, std::memory_order_relaxed);
        }
        if (runtime.jitter_sample_count == 1U ||
            (runtime.jitter_sample_count % 16U) == 0U) {
            const auto target_rank = runtime.jitter_sample_count -
                (runtime.jitter_sample_count / 100U);
            std::uint64_t cumulative = 0U;
            std::uint64_t p99_us = 0U;
            for (std::size_t index = 0; index < runtime.jitter_histogram.size(); ++index) {
                cumulative += runtime.jitter_histogram[index];
                if (cumulative >= target_rank) {
                    p99_us = index + 1U == runtime.jitter_histogram.size()
                        ? max_jitter_us_.load(std::memory_order_relaxed)
                        : static_cast<std::uint64_t>(index + 1U) * kJitterBucketWidthUs;
                    break;
                }
            }
            jitter_p99_us_.store(p99_us, std::memory_order_relaxed);
        }
        BeatEvent beat;
        while (beats_.try_pop(beat)) {
            runtime.sync.on_os2l_beat(beat.position, beat.bpm, beat.timestamp_ms);
        }
        auto clock = runtime.sync.tick(now_ms);

        auto trigger_loop = [&](showcore::AutoloopAddress address,
                                double beat_position) noexcept {
            if (!address.valid()) {
                return;
            }
            if (v2_enabled) {
                const showcore::AutoloopLaunchRequest request{
                    address, activation->generation};
                static_cast<void>(runtime.autoloop_v2.launch_manual(
                    request,
                    v2_transport(clock, runtime.track_playing),
                    engine.layers()));
                if (runtime.autoloop_v2.director_status().manual.active) {
                    runtime.selected_autoloop = address;
                }
            } else if (runtime.manual_autoloop.trigger(
                    show->autoloops(),
                    address,
                    show->autoloop_repeat(address),
                    beat_position,
                    runtime.track_playing,
                    engine.layers())) {
                runtime.selected_autoloop = address;
            }
        };

        auto clear_manual_loop = [&]() noexcept {
            if (v2_enabled) {
                static_cast<void>(runtime.autoloop_v2.clear_manual(
                    activation->generation, engine.layers()));
            } else {
                runtime.manual_autoloop.clear(engine.layers());
            }
            runtime.selected_autoloop = {};
        };

        auto adjacent_loop = [&](bool reverse) noexcept {
            if (v2_enabled) {
                return next_v2_address(
                    *v2_package,
                    runtime.selected_autoloop,
                    runtime.autoloop_v2.director_status().active_bank_mask,
                    reverse);
            }
            return reverse
                ? show->autoloops().previous_available(
                      runtime.selected_autoloop)
                : show->autoloops().next_available(
                      runtime.selected_autoloop);
        };

        auto select_all_banks = [&]() noexcept {
            if (v2_enabled) {
                static_cast<void>(runtime.autoloop_v2.request_all_banks(
                    activation->generation));
            } else {
                show->autoloops().select_all_banks();
            }
        };

        auto select_exclusive_bank = [&](std::uint16_t bank) noexcept {
            if (v2_enabled) {
                static_cast<void>(
                    runtime.autoloop_v2.request_exclusive_bank(
                        bank, activation->generation));
            } else {
                static_cast<void>(
                    show->autoloops().select_exclusive_bank(bank));
            }
        };

        auto set_bank_enabled = [&](std::uint16_t bank, bool enabled) noexcept {
            if (v2_enabled) {
                static_cast<void>(runtime.autoloop_v2.set_bank_enabled(
                    bank, enabled, activation->generation));
            } else {
                static_cast<void>(
                    show->autoloops().set_bank_enabled(bank, enabled));
            }
        };

        auto clear_track_script_layers = [&]() noexcept {
            runtime.scripted_look.clear(now_ms, 0U, engine.layers());
            runtime.scripted_autoloop.clear(engine.layers());
            if (v2_enabled) {
                const auto owner =
                    runtime.autoloop_v2.status().track_script_owner;
                static_cast<void>(runtime.autoloop_v2.clear_scripted(
                    activation->generation, engine.layers()));
                runtime.autoloop_v2.release_legacy_track_script(
                    owner, engine.layers());
            }
            runtime.legacy_track_script_owner =
                AutoloopTrackScriptOwner::None;
        };

        auto clear_track_script = [&]() noexcept {
            clear_track_script_layers();
            runtime.selected_track_script = -1;
            runtime.next_track_cue = 0U;
            runtime.track_script_start_beat = 0.0;
            runtime.last_track_script_beat = -1.0;
            active_track_script_beat_milli_.store(0, std::memory_order_relaxed);
            active_track_script_consumed_cues_.store(0U, std::memory_order_relaxed);
        };

        auto apply_track_cue = [&](const CompiledTrackCue& cue,
                                   double beat_position) noexcept {
            switch (cue.action) {
            case TrackCueAction::TriggerLook:
                if (const auto* look = show->look(cue.target); look != nullptr) {
                    // A scripted scene and a scripted Autoloop share one semantic
                    // layer. Triggering either is an intentional handoff, not an
                    // accidental whole-layer overwrite on the scheduler tick.
                    runtime.scripted_autoloop.clear(engine.layers());
                    if (v2_enabled) {
                        static_cast<void>(
                            runtime.autoloop_v2.claim_legacy_track_script(
                                AutoloopTrackScriptOwner::LegacyLook,
                                engine.layers()));
                    }
                    if (runtime.scripted_look.trigger(
                            *look,
                            now_ms,
                            show->look_fade_ms(cue.target),
                            engine.layers())) {
                        runtime.legacy_track_script_owner =
                            AutoloopTrackScriptOwner::LegacyLook;
                    }
                }
                break;
            case TrackCueAction::ClearLook:
                runtime.scripted_look.clear(now_ms, 0U, engine.layers());
                if (v2_enabled) {
                    runtime.autoloop_v2.release_legacy_track_script(
                        AutoloopTrackScriptOwner::LegacyLook,
                        engine.layers());
                }
                if (runtime.legacy_track_script_owner ==
                    AutoloopTrackScriptOwner::LegacyLook) {
                    runtime.legacy_track_script_owner =
                        AutoloopTrackScriptOwner::None;
                }
                break;
            case TrackCueAction::TriggerAutoloop: {
                const auto address = decode_autoloop(cue.target);
                if (address.valid()) {
                    runtime.scripted_look.clear(now_ms, 0U, engine.layers());
                    if (v2_enabled) {
                        runtime.autoloop_v2.release_legacy_track_script(
                            AutoloopTrackScriptOwner::LegacyLook,
                            engine.layers());
                        runtime.scripted_autoloop.clear(engine.layers());
                        const showcore::AutoloopLaunchRequest request{
                            address, activation->generation};
                        static_cast<void>(
                            runtime.autoloop_v2.launch_scripted(
                                request,
                                v2_transport(clock, runtime.track_playing),
                                engine.layers()));
                    } else if (runtime.scripted_autoloop.trigger(
                                   show->autoloops(),
                                   address,
                                   show->autoloop_repeat(address),
                                   beat_position,
                                   runtime.track_playing,
                                   engine.layers())) {
                        runtime.legacy_track_script_owner =
                            AutoloopTrackScriptOwner::LegacyAutoloop;
                    }
                }
                break;
            }
            case TrackCueAction::ClearAutoloop:
                if (v2_enabled) {
                    static_cast<void>(runtime.autoloop_v2.clear_scripted(
                        activation->generation, engine.layers()));
                } else {
                    runtime.scripted_autoloop.clear(engine.layers());
                    if (runtime.legacy_track_script_owner ==
                        AutoloopTrackScriptOwner::LegacyAutoloop) {
                        runtime.legacy_track_script_owner =
                            AutoloopTrackScriptOwner::None;
                    }
                }
                break;
            case TrackCueAction::Count:
                break;
            }
        };

        auto run_track_script = [&](double beat_position) noexcept {
            if (runtime.selected_track_script < 0) {
                return;
            }
            const auto script_index = static_cast<std::size_t>(runtime.selected_track_script);
            const auto* script = show->track_script(script_index);
            if (script == nullptr) {
                clear_track_script();
                return;
            }
            const auto relative_beat = std::max(0.0, beat_position - runtime.track_script_start_beat);
            constexpr double kSeekToleranceBeats = 0.001;
            if (runtime.last_track_script_beat >= 0.0 &&
                relative_beat + kSeekToleranceBeats < runtime.last_track_script_beat) {
                clear_track_script_layers();
                runtime.next_track_cue = 0U;
            }
            while (runtime.next_track_cue < script->cue_count &&
                   static_cast<double>(script->cues[runtime.next_track_cue].at_beat) <=
                       relative_beat + kSeekToleranceBeats) {
                apply_track_cue(script->cues[runtime.next_track_cue], beat_position);
                ++runtime.next_track_cue;
            }
            runtime.last_track_script_beat = relative_beat;
            active_track_script_beat_milli_.store(
                static_cast<std::int64_t>(relative_beat * 1000.0), std::memory_order_relaxed);
            active_track_script_consumed_cues_.store(
                static_cast<std::uint32_t>(runtime.next_track_cue), std::memory_order_relaxed);
        };

        auto trigger_track_script = [&](std::uint16_t index, double beat_position) noexcept {
            if (show->track_script(index) == nullptr) {
                return;
            }
            clear_track_script();
            runtime.selected_track_script = static_cast<std::int32_t>(index);
            runtime.track_script_start_beat = beat_position;
            active_track_script_beat_milli_.store(0, std::memory_order_relaxed);
            active_track_script_consumed_cues_.store(0U, std::memory_order_relaxed);
        };

        auto arm_hazard = [&](showcore::Property property, bool armed) noexcept {
            switch (property) {
            case showcore::Property::Fog:
                engine.safety().fog_armed = armed;
                fog_armed_.store(armed, std::memory_order_relaxed);
                break;
            case showcore::Property::Haze:
                engine.safety().haze_armed = armed;
                haze_armed_.store(armed, std::memory_order_relaxed);
                break;
            case showcore::Property::Laser:
                engine.safety().laser_armed = armed;
                laser_armed_.store(armed, std::memory_order_relaxed);
                break;
            case showcore::Property::Spark:
                engine.safety().spark_armed = armed;
                spark_armed_.store(armed, std::memory_order_relaxed);
                break;
            default:
                break;
            }
        };

        auto set_manual_property = [&](std::uint16_t fixture,
                                       showcore::Property property,
                                       float value,
                                       bool active,
                                       bool force_zero) noexcept {
            if (fixture >= show->fixture_count() || property >= showcore::Property::Count) {
                return;
            }
            const auto property_index = static_cast<std::size_t>(property);
            auto& current = runtime.manual_values[fixture][property_index];
            auto& override_active = runtime.manual_override_active[fixture][property_index];
            if (active) {
                current = std::clamp(value, 0.0F, 1.0F);
                if (!override_active) {
                    override_active = true;
                    ++runtime.manual_override_count;
                }
            } else {
                current = 0.0F;
                if (override_active) {
                    override_active = false;
                    --runtime.manual_override_count;
                }
            }
            engine.layers().set(
                showcore::LayerId::ManualOverride,
                fixture,
                property,
                active ? (force_zero ? showcore::PropertyValue::force_zero()
                                     : showcore::PropertyValue::set(current))
                       : showcore::PropertyValue::release());
        };

        auto set_group_blackout = [&](const showcore::FixtureGroup& group, bool active) noexcept {
            for (std::size_t member = 0U; member < group.count; ++member) {
                for (const auto property : kVisualBlackoutProperties) {
                    set_manual_property(group.fixture_ids[member], property, 0.0F, active, true);
                }
            }
        };

        auto clear_manual_overrides = [&]() noexcept {
            engine.layers().clear_layer(showcore::LayerId::ManualOverride);
            for (auto& fixture_values : runtime.manual_values) {
                fixture_values.fill(0.0F);
            }
            for (auto& fixture_active : runtime.manual_override_active) {
                fixture_active.fill(false);
            }
            runtime.manual_override_count = 0U;
        };

        auto same_static_look_owner = [](
            const RunnerStaticLookActivation& current,
            const StaticLookOwnerContext& owner) noexcept {
            return current.owner_kind == owner.kind &&
                current.owner_feedback_token == owner.feedback_token &&
                current.owner_session_token == owner.owner_session_token;
        };

        auto next_static_look_generation = [&]() noexcept {
            if (static_look_activation_sequence_ ==
                std::numeric_limits<std::uint64_t>::max()) {
                return std::uint64_t{0U};
            }
            ++static_look_activation_sequence_;
            return static_look_activation_sequence_;
        };

        auto activate_static_look = [&](std::uint16_t index,
                                        StaticLookBehavior behavior,
                                        StaticLookOwnerContext owner) noexcept {
            auto& current = runtime.static_look_activation;
            if (behavior == StaticLookBehavior::Hold) {
                if (owner.kind == StaticLookOwnerKind::None ||
                    owner.feedback_token == 0U) {
                    return false;
                }
            } else {
                owner = normalized_static_look_owner(owner, index);
            }
            if (behavior == StaticLookBehavior::Hold &&
                current.look_index == static_cast<std::int32_t>(index) &&
                current.behavior == StaticLookBehavior::Hold &&
                current.status != StaticLookActivationStatus::None &&
                current.status != StaticLookActivationStatus::Releasing &&
                same_static_look_owner(current, owner)) {
                // A duplicate press from one held control retains the exact
                // generation so its matching release remains authoritative.
                return true;
            }
            const auto* look = show->look(index);
            if (look == nullptr) {
                return false;
            }
            if (static_look_activation_sequence_ ==
                std::numeric_limits<std::uint64_t>::max() ||
                !runtime.static_look.trigger(
                    *look, now_ms, show->look_fade_ms(index), engine.layers())) {
                return false;
            }
            const auto generation = next_static_look_generation();
            current.look_index = static_cast<std::int32_t>(index);
            current.package_generation = activation->generation;
            current.activation_generation = generation;
            current.owner_kind = owner.kind;
            current.owner_feedback_token = owner.feedback_token;
            current.owner_session_token = owner.owner_session_token;
            current.behavior = behavior == StaticLookBehavior::None
                ? StaticLookBehavior::Explicit
                : behavior;
            current.status = StaticLookActivationStatus::Activating;
            current.activated_at_ms = now_ms;
            current.transition_progress = 0.0F;
            return true;
        };

        auto clear_static_look = [&]() noexcept {
            auto& current = runtime.static_look_activation;
            if (current.status == StaticLookActivationStatus::None ||
                current.status == StaticLookActivationStatus::Releasing ||
                current.look_index < 0) {
                return false;
            }
            const auto fade_ms = show->look_fade_ms(
                static_cast<std::size_t>(current.look_index));
            runtime.static_look.clear(now_ms, fade_ms, engine.layers());
            current.status = StaticLookActivationStatus::Releasing;
            current.transition_progress = 0.0F;
            return true;
        };

        auto release_static_look = [&](std::uint16_t index,
                                       const StaticLookOwnerContext& owner) noexcept {
            const auto& current = runtime.static_look_activation;
            if (current.look_index != static_cast<std::int32_t>(index) ||
                current.behavior != StaticLookBehavior::Hold ||
                current.status == StaticLookActivationStatus::None ||
                current.status == StaticLookActivationStatus::Releasing ||
                !same_static_look_owner(current, owner) ||
                owner.expected_package_generation == 0U ||
                owner.expected_package_generation != current.package_generation ||
                owner.expected_activation_generation == 0U ||
                owner.expected_activation_generation !=
                    current.activation_generation) {
                return false;
            }
            return clear_static_look();
        };

        auto toggle_static_look = [&](std::uint16_t index,
                                      StaticLookOwnerContext owner) noexcept {
            owner = normalized_static_look_owner(owner, index);
            const auto& current = runtime.static_look_activation;
            const bool same_active_look =
                current.look_index == static_cast<std::int32_t>(index) &&
                current.status != StaticLookActivationStatus::None &&
                current.status != StaticLookActivationStatus::Releasing;
            if (same_active_look &&
                (same_static_look_owner(current, owner) ||
                 current.behavior == StaticLookBehavior::Latch)) {
                static_cast<void>(clear_static_look());
            } else {
                static_cast<void>(activate_static_look(
                    index, StaticLookBehavior::Latch, owner));
            }
        };

        auto find_look_by_name = [&](std::string_view name) noexcept {
            for (std::size_t index = 0U; index < show->look_count(); ++index) {
                const auto* look = show->look(index);
                if (look != nullptr && look->name != nullptr && name == look->name) {
                    return static_cast<std::int32_t>(index);
                }
            }
            return std::int32_t{-1};
        };

        auto find_autoloop_by_name = [&](std::string_view name) noexcept {
            for (std::uint16_t bank = 0U; bank < showcore::kMaxAutoloopBanks; ++bank) {
                for (std::uint8_t slot = 0U; slot < showcore::kAutoloopsPerBank; ++slot) {
                    const showcore::AutoloopAddress address{bank, slot};
                    const auto* pattern = show->autoloops().get(address);
                    if (pattern != nullptr && pattern->name != nullptr && name == pattern->name) {
                        return address;
                    }
                }
            }
            return showcore::AutoloopAddress{};
        };

        auto apply_tap = [&](std::uint64_t timestamp_ms) noexcept {
            if (runtime.last_tap_ms != 0U) {
                const auto interval = timestamp_ms - runtime.last_tap_ms;
                if (interval >= 200U && interval <= 3000U) {
                    runtime.tap_intervals[runtime.tap_interval_cursor] = interval;
                    runtime.tap_interval_cursor =
                        (runtime.tap_interval_cursor + 1U) % runtime.tap_intervals.size();
                    runtime.tap_interval_count = std::min(
                        runtime.tap_interval_count + 1U,
                        runtime.tap_intervals.size());
                    std::uint64_t total = 0;
                    for (std::size_t index = 0;
                         index < runtime.tap_interval_count;
                         ++index) {
                        total += runtime.tap_intervals[index];
                    }
                    const auto average = static_cast<double>(total) /
                        static_cast<double>(runtime.tap_interval_count);
                    runtime.sync.set_manual_bpm(60000.0 / average, timestamp_ms);
                } else {
                    runtime.tap_interval_count = 0;
                    runtime.tap_interval_cursor = 0;
                }
            }
            runtime.last_tap_ms = timestamp_ms;
        };

        auto apply_action = [&](const showcore::MidiActionEvent& event,
                                double beat_position,
                                std::uint64_t owner_session_token) noexcept {
            const auto& action = event.action;
            if (action.type == showcore::ActionType::TriggerLook) {
                const StaticLookOwnerContext owner{
                    StaticLookOwnerKind::Midi,
                    static_cast<std::uint64_t>(event.mapping_index) + 1U,
                    0U,
                    0U,
                    owner_session_token};
                switch (event.behavior) {
                case showcore::MappingBehavior::Toggle:
                    if (event.active) {
                        toggle_static_look(action.target_id, owner);
                    }
                    break;
                case showcore::MappingBehavior::Latch:
                    if (event.active) {
                        static_cast<void>(activate_static_look(
                            action.target_id, StaticLookBehavior::Latch, owner));
                    }
                    break;
                case showcore::MappingBehavior::Momentary:
                case showcore::MappingBehavior::Continuous:
                case showcore::MappingBehavior::Relative:
                    if (event.active) {
                        // A binding with an outstanding begin cannot safely
                        // reinterpret a repeated begin after replacement: a
                        // delayed release has no wire-level generation. Keep
                        // the original stored generation until its release is
                        // consumed; a later clean begin may reactivate.
                        auto& lease = runtime.midi_static_look_leases
                            [event.mapping_index];
                        if (lease.outstanding()) {
                            if (lease.owner_session_token ==
                                owner.owner_session_token) {
                                break;
                            }
                            lease.clear();
                        }
                        if (activate_static_look(
                                action.target_id,
                                StaticLookBehavior::Hold,
                                owner)) {
                            static_cast<void>(lease.record_begin(
                                owner.feedback_token,
                                runtime.static_look_activation
                                    .package_generation,
                                runtime.static_look_activation
                                    .activation_generation,
                                owner.owner_session_token));
                        }
                    } else {
                        auto release_owner =
                            runtime.midi_static_look_leases[event.mapping_index]
                                .consume_release(
                                    owner.kind,
                                    owner.feedback_token,
                                    owner.owner_session_token);
                        static_cast<void>(release_static_look(
                            action.target_id, release_owner));
                    }
                    break;
                }
                return;
            }

            bool active = event.active;
            auto& toggled = runtime.mapping_toggles[event.mapping_index];
            switch (event.behavior) {
            case showcore::MappingBehavior::Toggle:
                if (!event.active) {
                    return;
                }
                toggled = !toggled;
                active = toggled;
                break;
            case showcore::MappingBehavior::Latch:
                if (!event.active) {
                    return;
                }
                toggled = true;
                active = true;
                break;
            case showcore::MappingBehavior::Continuous:
            case showcore::MappingBehavior::Relative:
                active = true;
                break;
            case showcore::MappingBehavior::Momentary:
                break;
            }

            switch (action.type) {
            case showcore::ActionType::SetProperty: {
                if (action.target_id >= showcore::kMaxFixtures ||
                    action.property >= showcore::Property::Count) {
                    break;
                }
                auto& current = runtime.manual_values[action.target_id]
                    [static_cast<std::size_t>(action.property)];
                if (event.relative) {
                    current = std::clamp(current + event.value * 0.05F, 0.0F, 1.0F);
                } else {
                    current = std::clamp(event.value, 0.0F, 1.0F);
                }
                set_manual_property(action.target_id, action.property, current, active, false);
                break;
            }
            case showcore::ActionType::SetGroupProperty: {
                const auto* group = show->group(action.target_id);
                if (group == nullptr || group->count == 0U ||
                    action.property >= showcore::Property::Count) {
                    break;
                }
                for (std::size_t member = 0U; member < group->count; ++member) {
                    const auto fixture = group->fixture_ids[member];
                    auto& current = runtime.manual_values[fixture]
                        [static_cast<std::size_t>(action.property)];
                    if (event.relative) {
                        current = std::clamp(current + event.value * 0.05F, 0.0F, 1.0F);
                    } else {
                        current = std::clamp(event.value, 0.0F, 1.0F);
                    }
                    set_manual_property(fixture, action.property, current, active, false);
                }
                break;
            }
            case showcore::ActionType::BlackoutGroup:
                if (const auto* group = show->group(action.target_id);
                    group != nullptr && group->count > 0U) {
                    set_group_blackout(*group, active);
                }
                break;
            case showcore::ActionType::Blackout:
                set_blackout(active);
                break;
            case showcore::ActionType::TriggerLook:
                break;
            case showcore::ActionType::TriggerAutoloop:
                if (active) {
                    trigger_loop(decode_autoloop(action.target_id), beat_position);
                } else {
                    clear_manual_loop();
                }
                break;
            case showcore::ActionType::TriggerTrackScript:
                if (active) {
                    trigger_track_script(action.target_id, beat_position);
                } else {
                    clear_track_script();
                }
                break;
            case showcore::ActionType::TapTempo:
                apply_tap(now_ms);
                break;
            case showcore::ActionType::ArmFog:
                arm_hazard(showcore::Property::Fog, active);
                break;
            case showcore::ActionType::ClearLook:
                if (active) {
                    static_cast<void>(clear_static_look());
                }
                break;
            case showcore::ActionType::ClearAutoloop:
                if (active) {
                    clear_manual_loop();
                }
                break;
            case showcore::ActionType::ClearTrackScript:
                if (active) {
                    clear_track_script();
                }
                break;
            case showcore::ActionType::ClearManualOverrides:
                if (active) {
                    clear_manual_overrides();
                }
                break;
            case showcore::ActionType::SelectAutoloopBank:
                if (active) {
                    select_exclusive_bank(action.target_id);
                }
                break;
            case showcore::ActionType::SelectAllAutoloopBanks:
                if (active) {
                    select_all_banks();
                }
                break;
            case showcore::ActionType::SetAutoloopBankEnabled:
                set_bank_enabled(action.target_id, active);
                break;
            case showcore::ActionType::NextAutoloop:
                if (active) {
                    trigger_loop(adjacent_loop(false), beat_position);
                }
                break;
            case showcore::ActionType::PreviousAutoloop:
                if (active) {
                    trigger_loop(adjacent_loop(true), beat_position);
                }
                break;
            case showcore::ActionType::WorkLight:
                set_work_light(active);
                break;
            case showcore::ActionType::ArmHaze:
                arm_hazard(showcore::Property::Haze, active);
                break;
            case showcore::ActionType::ArmLaser:
                arm_hazard(showcore::Property::Laser, active);
                break;
            case showcore::ActionType::ArmSpark:
                arm_hazard(showcore::Property::Spark, active);
                break;
            case showcore::ActionType::None:
            case showcore::ActionType::Count:
                break;
            }
        };

        auto handle_static_look_owner_loss = [&]
            (const RunnerStaticLookOwnerLossEvent& loss) noexcept {
            if (loss.kind == StaticLookOwnerKind::None ||
                loss.owner_session_token == 0U) {
                return;
            }
            auto clear_matching_leases = [&](auto& leases) noexcept {
                for (auto& lease : leases) {
                    if (lease.belongs_to(
                            loss.owner_session_token,
                            loss.owner_feedback_token)) {
                        lease.clear();
                    }
                }
            };
            if (loss.kind == StaticLookOwnerKind::Midi) {
                clear_matching_leases(runtime.midi_static_look_leases);
            } else if (loss.kind == StaticLookOwnerKind::External) {
                clear_matching_leases(runtime.os2l_static_look_leases);
            }

            const auto& current = runtime.static_look_activation;
            if (current.owner_kind == loss.kind &&
                current.owner_session_token == loss.owner_session_token &&
                (loss.owner_feedback_token == 0U ||
                 current.owner_feedback_token == loss.owner_feedback_token)) {
                static_cast<void>(clear_static_look());
            }
        };

        RunnerOs2lButtonEvent os2l_button;
        while (os2l_buttons_.try_pop(os2l_button)) {
            if (os2l_button.generation != activation->generation) {
                continue;
            }
            auto target_name = os2l_button.name.view();
            bool look_only = false;
            bool autoloop_only = false;
            constexpr std::string_view kLookPrefix = "Look: ";
            constexpr std::string_view kAutoloopPrefix = "Autoloop: ";
            if (target_name.starts_with(kLookPrefix)) {
                look_only = true;
                target_name.remove_prefix(kLookPrefix.size());
            } else if (target_name.starts_with(kAutoloopPrefix)) {
                autoloop_only = true;
                target_name.remove_prefix(kAutoloopPrefix.size());
            }
            if (target_name.empty()) {
                continue;
            }

            if (!autoloop_only) {
                const auto look_index = find_look_by_name(target_name);
                if (look_index >= 0) {
                    const auto target = static_cast<std::uint16_t>(look_index);
                    const StaticLookOwnerContext owner{
                        StaticLookOwnerKind::External,
                        static_look_feedback_token(target_name),
                        0U,
                        0U,
                        os2l_button.owner_session_token};
                    if (owner.owner_session_token == 0U) {
                        continue;
                    }
                    if (os2l_button.on) {
                        auto& lease =
                            runtime.os2l_static_look_leases[target];
                        if (lease.outstanding()) {
                            if (lease.owner_session_token ==
                                owner.owner_session_token) {
                                continue;
                            }
                            lease.clear();
                        }
                        if (activate_static_look(
                                target, StaticLookBehavior::Hold, owner)) {
                            static_cast<void>(lease.record_begin(
                                owner.feedback_token,
                                runtime.static_look_activation
                                    .package_generation,
                                runtime.static_look_activation
                                    .activation_generation,
                                owner.owner_session_token));
                        }
                    } else {
                        auto release_owner =
                            runtime.os2l_static_look_leases[target]
                                .consume_release(
                                    owner.kind,
                                    owner.feedback_token,
                                    owner.owner_session_token);
                        static_cast<void>(release_static_look(
                            target, release_owner));
                    }
                    continue;
                }
            }
            if (!look_only) {
                const auto address = find_autoloop_by_name(target_name);
                if (address.valid()) {
                    if (os2l_button.on) {
                        trigger_loop(address, clock.beat_position);
                    } else if (runtime.selected_autoloop == address) {
                        clear_manual_loop();
                    }
                }
            }
        }

        RunnerCommand command;
        while (commands_.try_pop(command)) {
            if (command.generation != activation->generation) {
                continue;
            }
            switch (command.type) {
            case RunnerCommandType::TriggerLook:
                static_cast<void>(activate_static_look(
                    command.target,
                    command.static_look_behavior,
                    command.static_look_owner));
                break;
            case RunnerCommandType::ToggleLook:
                toggle_static_look(
                    command.target, command.static_look_owner);
                break;
            case RunnerCommandType::SetLookHeld:
                if (command.active) {
                    static_cast<void>(activate_static_look(
                        command.target,
                        StaticLookBehavior::Hold,
                        command.static_look_owner));
                } else {
                    static_cast<void>(release_static_look(
                        command.target, command.static_look_owner));
                }
                break;
            case RunnerCommandType::ClearLook:
                static_cast<void>(clear_static_look());
                break;
            case RunnerCommandType::TriggerAutoloop:
                trigger_loop(decode_autoloop(command.target), clock.beat_position);
                break;
            case RunnerCommandType::ClearAutoloop:
                clear_manual_loop();
                break;
            case RunnerCommandType::NextAutoloop:
                trigger_loop(adjacent_loop(false), clock.beat_position);
                break;
            case RunnerCommandType::PreviousAutoloop:
                trigger_loop(adjacent_loop(true), clock.beat_position);
                break;
            case RunnerCommandType::SelectAllAutoloopBanks:
                select_all_banks();
                break;
            case RunnerCommandType::SelectExclusiveAutoloopBank:
                select_exclusive_bank(command.target);
                break;
            case RunnerCommandType::SetAutoloopBankEnabled:
                set_bank_enabled(command.target, command.active);
                break;
            case RunnerCommandType::TriggerTrackScript:
                trigger_track_script(command.target, clock.beat_position);
                break;
            case RunnerCommandType::ClearTrackScript:
                clear_track_script();
                break;
            case RunnerCommandType::SetManualBpm:
                runtime.sync.set_manual_bpm(command.value, command.timestamp_ms);
                break;
            case RunnerCommandType::TapTempo:
                apply_tap(command.timestamp_ms);
                break;
            case RunnerCommandType::SetProperty:
                set_manual_property(
                    command.target, command.property, command.value, command.active, false);
                break;
            case RunnerCommandType::ArmHazard:
                arm_hazard(command.property, command.active);
                break;
            case RunnerCommandType::SetTrackPlaying:
                runtime.track_playing = command.active;
                break;
            case RunnerCommandType::ClearManualOverrides:
                clear_manual_overrides();
                break;
            case RunnerCommandType::SetGroupProperty: {
                bool valid_group = false;
                for (std::size_t fixture = 0U;
                     fixture < showcore::kMaxFixtures;
                     ++fixture) {
                    const auto selected = (command.fixture_mask[fixture / 64U] &
                                           (std::uint64_t{1} << (fixture % 64U))) != 0U;
                    if (!selected) {
                        continue;
                    }
                    if (fixture >= show->fixture_count()) {
                        valid_group = false;
                        break;
                    }
                    valid_group = true;
                }
                if (!valid_group) {
                    break;
                }
                for (std::uint16_t fixture = 0U;
                     fixture < show->fixture_count();
                     ++fixture) {
                    if ((command.fixture_mask[fixture / 64U] &
                         (std::uint64_t{1} << (fixture % 64U))) != 0U) {
                        set_manual_property(
                            fixture, command.property, command.value, command.active, false);
                    }
                }
                break;
            }
            case RunnerCommandType::StaticLookOwnerLost:
                handle_static_look_owner_loss({
                    command.static_look_owner.kind,
                    command.static_look_owner.owner_session_token,
                    command.static_look_owner.feedback_token,
                    command.generation});
                break;
            }
        }

        RunnerMidiActionEvent midi_action;
        while (midi_actions_.try_pop(midi_action)) {
            if (midi_action.generation == activation->generation) {
                apply_action(
                    midi_action.event,
                    clock.beat_position,
                    midi_action.owner_session_token);
            }
        }

        // Transport actions and lifecycle notices use independent SPSC queues.
        // Drain owner loss last so a button/message already queued by the old
        // connection cannot reactivate its Look after the disconnect cleanup.
        // A reconnected source has a new session token and is not affected.
        RunnerStaticLookOwnerLossEvent owner_loss;
        while (owner_losses_.try_pop(owner_loss)) {
            if (owner_loss.generation == activation->generation) {
                handle_static_look_owner_loss(owner_loss);
            }
        }

        const bool work_light = work_light_requested_.load(std::memory_order_acquire);
        if (work_light != runtime.applied_work_light) {
            engine.layers().clear_layer(showcore::LayerId::Emergency);
            if (work_light) {
                for (std::uint16_t fixture = 0; fixture < show->fixture_count(); ++fixture) {
                    for (const auto property : {
                             showcore::Property::Intensity,
                             showcore::Property::Red,
                             showcore::Property::Green,
                             showcore::Property::Blue,
                             showcore::Property::White}) {
                        engine.layers().set(
                            showcore::LayerId::Emergency,
                            fixture,
                            property,
                            showcore::PropertyValue::set(1.0F));
                    }
                }
            }
            runtime.applied_work_light = work_light;
        }

        clock = runtime.sync.tick(now_ms);
        if (v2_enabled) {
            auto transport = v2_transport(clock, runtime.track_playing);
            if (transport.phase_available &&
                runtime.has_v2_transport_tick &&
                transport.musical_tick < runtime.last_v2_transport_tick) {
                transport.discontinuity = true;
            }
            // Track cues may hand TrackScript between a legacy look and V2.
            // Apply that handoff first, then reconcile the private V2 stack so
            // a legacy clear can never blank a live V2 contribution for one
            // rendered frame.
            run_track_script(clock.beat_position);
            static_cast<void>(runtime.autoloop_v2.tick(
                transport, engine.layers()));
            if (transport.phase_available) {
                runtime.has_v2_transport_tick = true;
                runtime.last_v2_transport_tick = transport.musical_tick;
            }
            const auto owner =
                runtime.autoloop_v2.status().track_script_owner;
            const auto& v2_status =
                runtime.autoloop_v2.director_status();
            const bool manual_replaces_lower_layers =
                v2_status.manual.active &&
                v2_status.manual.mode ==
                    showcore::CompiledAutoloopPlaybackMode::Replace;
            if (!manual_replaces_lower_layers &&
                owner == AutoloopTrackScriptOwner::LegacyAutoloop) {
                runtime.scripted_autoloop.tick(
                    clock.beat_position,
                    runtime.track_playing,
                    engine.layers());
            } else if (!manual_replaces_lower_layers &&
                       owner == AutoloopTrackScriptOwner::LegacyLook) {
                runtime.scripted_look.tick(now_ms, engine.layers());
            }
        } else {
            runtime.autonomous.tick(
                clock.beat_position, runtime.track_playing, engine.layers());
            run_track_script(clock.beat_position);
            if (runtime.scripted_autoloop.status().active) {
                runtime.scripted_autoloop.tick(
                    clock.beat_position,
                    runtime.track_playing,
                    engine.layers());
            } else {
                runtime.scripted_look.tick(now_ms, engine.layers());
            }
            runtime.manual_autoloop.tick(
                clock.beat_position,
                runtime.track_playing,
                engine.layers());
            if (runtime.legacy_track_script_owner ==
                    AutoloopTrackScriptOwner::LegacyAutoloop &&
                !runtime.scripted_autoloop.status().active) {
                runtime.legacy_track_script_owner =
                    AutoloopTrackScriptOwner::None;
            }
        }
        runtime.static_look.tick(now_ms, engine.layers());
        {
            auto& current = runtime.static_look_activation;
            if (current.status != StaticLookActivationStatus::None) {
                const auto player_status = runtime.static_look.status(now_ms);
                current.transition_progress = player_status.transition_progress;
                if (current.status == StaticLookActivationStatus::Releasing) {
                    if (!player_status.transitioning) {
                        current = {};
                    }
                } else {
                    current.status = player_status.transitioning
                        ? StaticLookActivationStatus::Activating
                        : StaticLookActivationStatus::Active;
                }
            }
        }
        engine.tick();

        OutputFrame output;
        output.pre_blackout_frames = engine.frames();
        output.frames = output.pre_blackout_frames;
        output.attribution = engine.frame_attribution();
        output.rendered_at_ms = now_ms;
        output.blackout_applied =
            blackout_requested_.load(std::memory_order_acquire);
        if (output.blackout_applied) {
            output.frames.clear();
        }
        output.sequence = sequence;
        output.generation = activation->generation;
        if (!output_queue_.try_push(output)) {
            output_queue_drops_.fetch_add(1, std::memory_order_relaxed);
        }
        ++sequence;
        if (sequence == 0U) {
            sequence = 1U;
        }

        if (v2_enabled) {
            const auto& adapter_status = runtime.autoloop_v2.status();
            const auto& director_status =
                runtime.autoloop_v2.director_status();
            const auto v2_playback = encode_autoloop_playback(
                director_status.active.valid
                    ? director_status.active.address
                    : showcore::AutoloopAddress{},
                director_status.active_repeat,
                director_status.active_progress,
                director_status.active_completed_cycles);
            active_autoloop_playback_.store(
                v2_playback, std::memory_order_relaxed);
            active_autoloop_bank_mask_.store(
                director_status.active_bank_mask,
                std::memory_order_relaxed);
            autoloop_v2_mode_.store(
                adapter_status.mode, std::memory_order_relaxed);
            autoloop_v2_track_owner_.store(
                adapter_status.track_script_owner,
                std::memory_order_relaxed);
            autoloop_v2_track_suppressed_by_replace_.store(
                adapter_status.track_script_suppressed_by_replace,
                std::memory_order_relaxed);
            autoloop_v2_package_active_.store(
                director_status.package_active, std::memory_order_relaxed);
            autoloop_v2_generation_.store(
                director_status.package_generation,
                std::memory_order_relaxed);
            autoloop_v2_fault_.store(
                director_status.fault, std::memory_order_relaxed);
            autoloop_v2_result_.store(
                adapter_status.last_result, std::memory_order_relaxed);
            autoloop_v2_source_.store(
                director_status.active_source, std::memory_order_relaxed);
            autoloop_v2_playback_mode_.store(
                director_status.active_mode, std::memory_order_relaxed);
            autoloop_v2_playback_.store(
                v2_playback, std::memory_order_relaxed);
            autoloop_v2_active_bank_mask_.store(
                director_status.active_bank_mask,
                std::memory_order_relaxed);
            autoloop_v2_has_pending_bank_mask_.store(
                director_status.has_pending_bank_mask,
                std::memory_order_relaxed);
            autoloop_v2_pending_bank_mask_.store(
                director_status.pending_bank_mask,
                std::memory_order_relaxed);
        } else {
            const auto& manual_status = runtime.manual_autoloop.status();
            const auto& scripted_status = runtime.scripted_autoloop.status();
            const auto& autonomous_status = runtime.autonomous.status();
            const showcore::AutoloopPlaybackStatus* active_loop =
                manual_status.active
                ? &manual_status
                : (scripted_status.active
                       ? &scripted_status
                       : (autonomous_status.active
                              ? &autonomous_status
                              : nullptr));
            active_autoloop_playback_.store(
                encode_autoloop_playback(active_loop),
                std::memory_order_relaxed);
            active_autoloop_bank_mask_.store(
                show->autoloops().active_bank_mask(),
                std::memory_order_relaxed);
            autoloop_v2_mode_.store(
                AutoloopRuntimeMode::LegacyV1,
                std::memory_order_relaxed);
            autoloop_v2_track_owner_.store(
                runtime.legacy_track_script_owner,
                std::memory_order_relaxed);
            autoloop_v2_track_suppressed_by_replace_.store(
                false, std::memory_order_relaxed);
            autoloop_v2_package_active_.store(
                false, std::memory_order_relaxed);
            autoloop_v2_generation_.store(0U, std::memory_order_relaxed);
            autoloop_v2_fault_.store(
                showcore::AutoloopDirectorFault::None,
                std::memory_order_relaxed);
            autoloop_v2_result_.store(
                showcore::AutoloopDirectorResult::None,
                std::memory_order_relaxed);
            autoloop_v2_source_.store(
                showcore::AutoloopDirectorSource::None,
                std::memory_order_relaxed);
            autoloop_v2_playback_mode_.store(
                showcore::CompiledAutoloopPlaybackMode::Overlay,
                std::memory_order_relaxed);
            autoloop_v2_playback_.store(
                kPackedInvalidAutoloopAddress,
                std::memory_order_relaxed);
            autoloop_v2_active_bank_mask_.store(
                ~std::uint64_t{0U}, std::memory_order_relaxed);
            autoloop_v2_has_pending_bank_mask_.store(
                false, std::memory_order_relaxed);
            autoloop_v2_pending_bank_mask_.store(
                0U, std::memory_order_relaxed);
        }
        manual_override_count_.store(runtime.manual_override_count, std::memory_order_relaxed);
        publish_static_look_status(runtime.static_look_activation);
        active_track_script_.store(runtime.selected_track_script, std::memory_order_relaxed);
        sync_state_.store(clock.state, std::memory_order_relaxed);
        clock_source_.store(clock.source, std::memory_order_relaxed);
        bpm_milli_.store(
            static_cast<std::uint32_t>(std::max(0.0, clock.bpm) * 1000.0),
            std::memory_order_relaxed);
        beat_milli_.store(
            static_cast<std::int64_t>(clock.beat_position * 1000.0),
            std::memory_order_relaxed);
        frames_.fetch_add(1, std::memory_order_relaxed);
        last_frame_ms_.store(now_ms, std::memory_order_relaxed);

        next_frame += frame_period;
        if (now > next_frame + frame_period * 4) {
            scheduler_resyncs_.fetch_add(1U, std::memory_order_relaxed);
            next_frame = now + frame_period;
        }
    }
    if (activation != nullptr &&
        activation->show->autoloop_v2_package() != nullptr) {
        static_cast<void>(activation->runtime->autoloop_v2.clear_package(
            activation->show->engine().layers()));
    }
#ifdef _WIN32
    static_cast<void>(::timeEndPeriod(1U));
#endif
}

void RunnerService::run_output() noexcept {
    showcore::ArtNetSender artnet;
    std::array<showcore::SacnSender, showcore::kV1UniverseCount> sacn;
    std::array<showcore::DmxUsbProSender, showcore::kV1UniverseCount> dmx_usb_pro;
    showcore::SoundSwitchMicroSender soundswitch_micro;
    showcore::SoundSwitchControlOneSession soundswitch_control_one;
    const auto cid = showcore::make_sacn_cid(project_id_);

    auto open_missing_outputs = [&]() noexcept {
        bool artnet_ready = !connections_.artnet_enabled;
        bool sacn_ready = !connections_.sacn_enabled;
        bool usb_ready = true;
        if (connections_.artnet_enabled) {
            if (!artnet.is_open()) {
                output_health_[kArtNetOutputHealth].mark_opening();
                artnet_state_.store(AdapterState::Starting, std::memory_order_relaxed);
                static_cast<void>(artnet.open_ipv4(connections_.artnet_destination));
                if (artnet.is_open()) {
                    output_health_[kArtNetOutputHealth].mark_ready();
                } else {
                    output_health_[kArtNetOutputHealth].mark_fault(0U);
                }
            }
            artnet_ready = artnet.is_open();
            artnet_state_.store(
                artnet_ready ? AdapterState::Ready : AdapterState::Fault,
                std::memory_order_relaxed);
        }
        if (connections_.sacn_enabled) {
            sacn_state_.store(AdapterState::Starting, std::memory_order_relaxed);
            sacn_ready = true;
            bool sacn_open_attempted = false;
            for (std::uint16_t universe = 0; universe < showcore::kV1UniverseCount; ++universe) {
                const auto output_universe = static_cast<std::uint16_t>(
                    connections_.sacn_universe_base + universe);
                if (!sacn[universe].is_open()) {
                    if (!sacn_open_attempted) {
                        output_health_[kSacnOutputHealth].mark_opening();
                        sacn_open_attempted = true;
                    }
                    static_cast<void>(connections_.sacn_destination == "multicast"
                        ? sacn[universe].open_multicast(output_universe)
                        : sacn[universe].open_ipv4(connections_.sacn_destination));
                }
                sacn_ready = sacn_ready && sacn[universe].is_open();
            }
            if (sacn_open_attempted) {
                if (sacn_ready) {
                    output_health_[kSacnOutputHealth].mark_ready();
                } else {
                    output_health_[kSacnOutputHealth].mark_fault(0U);
                }
            }
            sacn_state_.store(
                sacn_ready ? AdapterState::Ready : AdapterState::Fault,
                std::memory_order_relaxed);
        }
        for (std::size_t universe = 0; universe < dmx_usb_pro.size(); ++universe) {
            const auto& port = connections_.dmx_usb_pro_ports[universe];
            if (port.empty()) {
                dmx_usb_pro_state_[universe].store(
                    AdapterState::Disabled, std::memory_order_relaxed);
                continue;
            }
            if (!dmx_usb_pro[universe].is_open()) {
                output_health_[kDmxUsbProUniverseOneHealth + universe].mark_opening();
                dmx_usb_pro_state_[universe].store(
                    AdapterState::Starting, std::memory_order_relaxed);
                static_cast<void>(dmx_usb_pro[universe].open(port));
                if (dmx_usb_pro[universe].is_open()) {
                    output_health_[kDmxUsbProUniverseOneHealth + universe].mark_ready();
                } else {
                    output_health_[kDmxUsbProUniverseOneHealth + universe].mark_fault(
                        dmx_usb_pro[universe].last_error());
                }
            }
            const bool opened = dmx_usb_pro[universe].is_open();
            usb_ready = usb_ready && opened;
            dmx_usb_pro_state_[universe].store(
                opened ? AdapterState::Ready : AdapterState::Fault,
                std::memory_order_relaxed);
        }
        if (connections_.soundswitch_micro_universe == 0U) {
            soundswitch_micro_state_.store(
                AdapterState::Disabled, std::memory_order_relaxed);
        } else {
            if (!soundswitch_micro.is_open()) {
                output_health_[kSoundSwitchMicroOutputHealth].mark_opening();
                soundswitch_micro_state_.store(
                    AdapterState::Starting, std::memory_order_relaxed);
                const auto opened = soundswitch_micro.open(
                    connections_.soundswitch_micro_framing);
                if (!opened) {
                    soundswitch_micro_last_error_.store(
                        soundswitch_micro.last_error(), std::memory_order_relaxed);
                    output_health_[kSoundSwitchMicroOutputHealth].mark_fault(
                        soundswitch_micro.last_error());
                } else {
                    output_health_[kSoundSwitchMicroOutputHealth].mark_ready();
                }
            }
            const bool opened = soundswitch_micro.is_open();
            usb_ready = usb_ready && opened;
            soundswitch_micro_state_.store(
                opened ? AdapterState::Ready : AdapterState::Fault,
                std::memory_order_relaxed);
        }
        if (!connections_.soundswitch_control_one_experimental) {
            soundswitch_control_one_state_.store(
                AdapterState::Disabled, std::memory_order_relaxed);
        } else {
            if (!soundswitch_control_one.is_open()) {
                output_health_[kSoundSwitchControlOneOutputHealth].mark_opening();
                soundswitch_control_one_state_.store(
                    AdapterState::Starting, std::memory_order_relaxed);
                if (!soundswitch_control_one.open()) {
                    output_health_[kSoundSwitchControlOneOutputHealth].mark_fault(
                        soundswitch_control_one.last_error());
                } else {
                    output_health_[kSoundSwitchControlOneOutputHealth].mark_ready();
                }
            }
            const bool opened = soundswitch_control_one.is_open();
            usb_ready = usb_ready && opened;
            soundswitch_control_one_state_.store(
                opened ? AdapterState::Ready : AdapterState::Fault,
                std::memory_order_relaxed);
        }
        return artnet_ready && sacn_ready && usb_ready;
    };

    bool outputs_ready = open_missing_outputs();
    auto next_retry = SteadyClock::now() + std::chrono::seconds(2);
    OutputFrame frame;
    while (!stop_requested_.load(std::memory_order_acquire) || !output_queue_.empty()) {
        auto* activation = published_activation_.load(std::memory_order_acquire);
        if (activation == nullptr) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        output_activation_ack_.store(activation->generation, std::memory_order_release);
        if (!outputs_ready && SteadyClock::now() >= next_retry) {
            outputs_ready = open_missing_outputs();
            next_retry = SteadyClock::now() + std::chrono::seconds(2);
        }
        const auto consumed = output_queue_.try_pop_latest(frame);
        if (consumed == 0U) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        if (consumed > 1U) {
            output_superseded_frames_.fetch_add(consumed - 1U, std::memory_order_relaxed);
        }
        if (frame.generation != activation->generation) {
            output_superseded_frames_.fetch_add(1U, std::memory_order_relaxed);
            continue;
        }

        std::array<showcore::OutputBackendHealth, kRunnerOutputRouteCount>
            route_before{};
        for (std::size_t index = 0U; index < route_before.size(); ++index) {
            route_before[index] = output_health_[index].snapshot();
        }

        bool artnet_success = true;
        bool sacn_success = true;
        bool usb_success = true;
        for (std::uint16_t universe = 0; universe < showcore::kV1UniverseCount; ++universe) {
            if (connections_.artnet_enabled && artnet.is_open()) {
                const auto packet = showcore::build_artdmx(
                    frame.frames.universes[universe],
                    static_cast<std::uint16_t>(connections_.artnet_base + universe),
                    frame.sequence);
                const bool sent = artnet.send(packet);
                output_health_[kArtNetOutputHealth].record_send(
                    sent, 0U, nonzero_slot_count(frame.frames.universes[universe]));
                artnet_success = sent && artnet_success;
            }
            if (connections_.sacn_enabled && sacn[universe].is_open()) {
                const auto packet = showcore::build_sacn_data_packet(
                    frame.frames.universes[universe],
                    static_cast<std::uint16_t>(connections_.sacn_universe_base + universe),
                    frame.sequence,
                    cid,
                    activation->project_name);
                const bool sent = sacn[universe].send(packet);
                output_health_[kSacnOutputHealth].record_send(
                    sent, 0U, nonzero_slot_count(frame.frames.universes[universe]));
                sacn_success = sent && sacn_success;
            }
            if (!connections_.dmx_usb_pro_ports[universe].empty() &&
                dmx_usb_pro[universe].is_open()) {
                const bool sent = dmx_usb_pro[universe].send(
                    frame.frames.universes[universe]);
                const auto error = sent ? 0U : dmx_usb_pro[universe].last_error();
                output_health_[kDmxUsbProUniverseOneHealth + universe].record_send(
                    sent, error, nonzero_slot_count(frame.frames.universes[universe]));
                if (!sent) {
                    usb_success = false;
                    dmx_usb_pro[universe].close();
                    dmx_usb_pro_state_[universe].store(
                        AdapterState::Fault, std::memory_order_relaxed);
                }
            }
        }
        if (connections_.soundswitch_micro_universe != 0U &&
            soundswitch_micro.is_open()) {
            const auto universe = static_cast<std::size_t>(
                connections_.soundswitch_micro_universe - 1U);
            const bool sent = soundswitch_micro.send(frame.frames.universes[universe]);
            const auto nonzero = nonzero_slot_count(frame.frames.universes[universe]);
            output_health_[kSoundSwitchMicroOutputHealth].record_send(
                sent, sent ? 0U : soundswitch_micro.last_error(), nonzero);
            if (!sent) {
                usb_success = false;
                soundswitch_micro_write_failures_.fetch_add(
                    1U, std::memory_order_relaxed);
                soundswitch_micro_last_error_.store(
                    soundswitch_micro.last_error(), std::memory_order_relaxed);
                soundswitch_micro.close();
                soundswitch_micro_state_.store(
                    AdapterState::Fault, std::memory_order_relaxed);
            } else {
                soundswitch_micro_write_frames_.fetch_add(
                    1U, std::memory_order_relaxed);
                soundswitch_micro_last_error_.store(0U, std::memory_order_relaxed);
                soundswitch_micro_last_nonzero_slots_.store(
                    nonzero, std::memory_order_relaxed);
            }
        }
        if (connections_.soundswitch_control_one_experimental &&
            soundswitch_control_one.is_open()) {
            const bool sent = soundswitch_control_one.send_pair(frame.frames.universes);
            const auto nonzero = static_cast<std::uint16_t>(
                nonzero_slot_count(frame.frames.universes[0U]) +
                nonzero_slot_count(frame.frames.universes[1U]));
            output_health_[kSoundSwitchControlOneOutputHealth].record_send(
                sent,
                sent ? 0U : soundswitch_control_one.last_error(),
                nonzero);
            if (!sent) {
                usb_success = false;
                soundswitch_control_one.close();
                soundswitch_control_one_state_.store(
                    AdapterState::Fault, std::memory_order_relaxed);
            }
        }
        const bool success = artnet_success && sacn_success && usb_success;
        if (!success) {
            output_send_failures_.fetch_add(1, std::memory_order_relaxed);
            outputs_ready = false;
            next_retry = SteadyClock::now() + std::chrono::seconds(2);
            if (!artnet_success && connections_.artnet_enabled) {
                artnet.close();
                output_health_[kArtNetOutputHealth].mark_fault(0U);
                artnet_state_.store(AdapterState::Fault, std::memory_order_relaxed);
            }
            if (!sacn_success && connections_.sacn_enabled) {
                for (auto& sender : sacn) {
                    sender.close();
                }
                output_health_[kSacnOutputHealth].mark_fault(0U);
                sacn_state_.store(AdapterState::Fault, std::memory_order_relaxed);
            }
        }
        RunnerOutputSnapshot published;
        published.generation = frame.generation;
        published.sequence = frame.sequence;
        published.rendered_at_ms = frame.rendered_at_ms;
        published.pre_blackout_frames = frame.pre_blackout_frames;
        published.routed_frames = frame.frames;
        published.attribution = frame.attribution;
        published.blackout_applied = frame.blackout_applied;
        for (std::size_t index = 0U; index < published.routes.size(); ++index) {
            const auto route_after = output_health_[index].snapshot();
            const auto attempted_delta =
                route_after.frames_attempted - route_before[index].frames_attempted;
            const auto accepted_delta =
                route_after.frames_accepted - route_before[index].frames_accepted;
            published.routes[index] = {
                route_after.kind,
                route_after.first_source_universe,
                route_after.source_universe_count,
                route_after.configured,
                static_cast<std::uint8_t>(std::min<std::uint64_t>(
                    attempted_delta,
                    std::numeric_limits<std::uint8_t>::max())),
                static_cast<std::uint8_t>(std::min<std::uint64_t>(
                    accepted_delta,
                    std::numeric_limits<std::uint8_t>::max())),
                route_after.last_error};
        }
        {
            const std::lock_guard<std::mutex> lock(output_snapshot_mutex_);
            latest_output_snapshot_ = published;
            has_output_snapshot_ = true;
        }
        output_frames_.fetch_add(1, std::memory_order_relaxed);
    }

    showcore::DmxFrames zero_frames;
    zero_frames.clear();
    for (auto& health : output_health_) {
        health.mark_stopping();
    }
    const auto* final_activation = published_activation_.load(std::memory_order_acquire);
    const std::string_view final_project_name = final_activation == nullptr
        ? std::string_view{"EmberLights"}
        : std::string_view{final_activation->project_name};
    for (std::uint8_t repeat = 0; repeat < 3U; ++repeat) {
        for (std::uint16_t universe = 0; universe < showcore::kV1UniverseCount; ++universe) {
            if (connections_.artnet_enabled && artnet.is_open()) {
                static_cast<void>(artnet.send(showcore::build_artdmx(
                    zero_frames.universes[universe],
                    static_cast<std::uint16_t>(connections_.artnet_base + universe),
                    static_cast<std::uint8_t>(frame.sequence + repeat + 1U))));
            }
            if (connections_.sacn_enabled && sacn[universe].is_open()) {
                static_cast<void>(sacn[universe].send(showcore::build_sacn_data_packet(
                    zero_frames.universes[universe],
                    static_cast<std::uint16_t>(connections_.sacn_universe_base + universe),
                    static_cast<std::uint8_t>(frame.sequence + repeat + 1U),
                    cid,
                    final_project_name)));
            }
            if (!connections_.dmx_usb_pro_ports[universe].empty() &&
                dmx_usb_pro[universe].is_open()) {
                static_cast<void>(dmx_usb_pro[universe].send(
                    zero_frames.universes[universe]));
            }
        }
        if (connections_.soundswitch_micro_universe != 0U &&
            soundswitch_micro.is_open()) {
            const auto universe = static_cast<std::size_t>(
                connections_.soundswitch_micro_universe - 1U);
            static_cast<void>(soundswitch_micro.send(zero_frames.universes[universe]));
        }
        if (connections_.soundswitch_control_one_experimental &&
            soundswitch_control_one.is_open()) {
            static_cast<void>(soundswitch_control_one.send_pair(zero_frames.universes));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    artnet.close();
    for (auto& sender : sacn) {
        sender.close();
    }
    for (auto& sender : dmx_usb_pro) {
        sender.close();
    }
    soundswitch_micro.close();
    soundswitch_control_one.close();
    for (auto& health : output_health_) {
        health.mark_disabled();
    }
}

}  // namespace emberlights
