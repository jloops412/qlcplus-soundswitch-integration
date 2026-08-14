#pragma once

#include "emberlights/autoloop_runtime.hpp"
#include "emberlights/compiler.hpp"
#include "emberlights/project.hpp"
#include "showcore/midi.hpp"
#include "showcore/os2l.hpp"
#include "showcore/output_backend.hpp"
#include "showcore/fixture.hpp"
#include "showcore/spsc_queue.hpp"
#include "showcore/sync_manager.hpp"
#include "showcore/types.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

namespace emberlights {

enum class RunnerState : std::uint8_t {
    Stopped,
    Starting,
    Running,
    Stopping,
    Fault
};

enum class AdapterState : std::uint8_t {
    Disabled,
    Starting,
    Waiting,
    Ready,
    Fault
};

enum class RunnerActivationError : std::uint8_t {
    None,
    NotRunning,
    InvalidShow,
    RestartRequired,
    Busy,
    Timeout
};

struct RunnerActivationResult {
    RunnerActivationError error{RunnerActivationError::None};
    std::uint64_t generation{0};

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == RunnerActivationError::None;
    }
};

struct RunnerAutoloopV2Status {
    AutoloopRuntimeMode mode{AutoloopRuntimeMode::LegacyV1};
    AutoloopTrackScriptOwner track_script_owner{
        AutoloopTrackScriptOwner::None};
    bool track_script_suppressed_by_replace{false};
    bool package_active{false};
    std::uint64_t package_generation{0U};
    showcore::AutoloopDirectorFault fault{
        showcore::AutoloopDirectorFault::None};
    showcore::AutoloopDirectorResult last_result{
        showcore::AutoloopDirectorResult::None};
    showcore::AutoloopDirectorSource active_source{
        showcore::AutoloopDirectorSource::None};
    showcore::CompiledAutoloopPlaybackMode active_mode{
        showcore::CompiledAutoloopPlaybackMode::Overlay};
    showcore::AutoloopRepeat active_repeat{showcore::AutoloopRepeat::Once};
    showcore::AutoloopAddress active_address{};
    float active_progress{0.0F};
    std::uint32_t active_completed_cycles{0U};
    std::uint64_t active_bank_mask{~std::uint64_t{0U}};
    bool has_pending_bank_mask{false};
    std::uint64_t pending_bank_mask{0U};
};

enum class StaticLookOwnerKind : std::uint8_t {
    None,
    Ui,
    Keyboard,
    Midi,
    Controller,
    Moment,
    External,
    Test
};

enum class StaticLookBehavior : std::uint8_t {
    None,
    Latch,
    Hold,
    Explicit
};

enum class StaticLookActivationStatus : std::uint8_t {
    None,
    Activating,
    Active,
    Releasing
};

// This context is supplied by a trusted binding/invocation service. The
// feedback token is an opaque, non-sensitive control identity; it is never a
// project/skin registry argument. Expected generations are zero on begin and
// both are required on release to bind it to the observed activation.
struct StaticLookOwnerContext {
    StaticLookOwnerKind kind{StaticLookOwnerKind::External};
    std::uint64_t feedback_token{0U};
    std::uint64_t expected_package_generation{0U};
    std::uint64_t expected_activation_generation{0U};
    // Adapter/controller connection epoch. This is trusted control-plane
    // identity, not a public registry argument or presentation-facing state.
    // A zero value denotes a local/non-session owner.
    std::uint64_t owner_session_token{0U};
};

// Bounded transport adapters use one lease per binding/target. Repeated begin
// events cannot overwrite an outstanding generation; the matching release
// consumes it, after which a later clean begin may activate again.
struct StaticLookBindingLease {
    std::uint64_t owner_feedback_token{0U};
    std::uint64_t owner_session_token{0U};
    std::uint64_t package_generation{0U};
    std::uint64_t activation_generation{0U};

    [[nodiscard]] constexpr bool outstanding() const noexcept {
        return activation_generation != 0U;
    }

    [[nodiscard]] constexpr bool record_begin(
        std::uint64_t owner_token,
        std::uint64_t package,
        std::uint64_t activation,
        std::uint64_t owner_session = 0U) noexcept {
        if (outstanding() || owner_token == 0U || package == 0U ||
            activation == 0U) {
            return false;
        }
        owner_feedback_token = owner_token;
        owner_session_token = owner_session;
        package_generation = package;
        activation_generation = activation;
        return true;
    }

    [[nodiscard]] constexpr StaticLookOwnerContext consume_release(
        StaticLookOwnerKind kind,
        std::uint64_t owner_token,
        std::uint64_t owner_session = 0U) noexcept {
        StaticLookOwnerContext context{
            kind, owner_token, 0U, 0U, owner_session};
        if (outstanding() && owner_feedback_token == owner_token &&
            owner_session_token == owner_session) {
            context.expected_package_generation = package_generation;
            context.expected_activation_generation = activation_generation;
            clear();
        }
        return context;
    }

    constexpr void clear() noexcept {
        owner_feedback_token = 0U;
        owner_session_token = 0U;
        package_generation = 0U;
        activation_generation = 0U;
    }

    [[nodiscard]] constexpr bool belongs_to(
        std::uint64_t owner_session,
        std::uint64_t owner_token = 0U) const noexcept {
        return outstanding() && owner_session != 0U &&
            owner_session_token == owner_session &&
            (owner_token == 0U || owner_feedback_token == owner_token);
    }
};

struct RunnerStaticLookActivation {
    std::int32_t look_index{-1};
    std::uint64_t package_generation{0U};
    std::uint64_t activation_generation{0U};
    StaticLookOwnerKind owner_kind{StaticLookOwnerKind::None};
    std::uint64_t owner_feedback_token{0U};
    StaticLookBehavior behavior{StaticLookBehavior::None};
    StaticLookActivationStatus status{StaticLookActivationStatus::None};
    std::uint64_t activated_at_ms{0U};
    float transition_progress{0.0F};
    // Internal adapter/controller epoch used only to reject stale disconnect
    // and release events after a source reconnects.
    std::uint64_t owner_session_token{0U};
};

enum class RunnerCommandType : std::uint8_t {
    TriggerLook,
    ToggleLook,
    SetLookHeld,
    ClearLook,
    TriggerAutoloop,
    ClearAutoloop,
    NextAutoloop,
    PreviousAutoloop,
    SelectAllAutoloopBanks,
    SelectExclusiveAutoloopBank,
    SetAutoloopBankEnabled,
    TriggerTrackScript,
    ClearTrackScript,
    SetManualBpm,
    TapTempo,
    SetProperty,
    ArmHazard,
    SetTrackPlaying,
    ClearManualOverrides,
    SetGroupProperty,
    StaticLookOwnerLost
};

struct RunnerCommand {
    RunnerCommandType type{RunnerCommandType::ClearLook};
    std::uint16_t target{0};
    showcore::Property property{showcore::Property::Intensity};
    float value{0.0F};
    bool active{false};
    std::array<std::uint64_t, (showcore::kMaxFixtures + 63U) / 64U> fixture_mask{};
    std::uint64_t timestamp_ms{0};
    std::uint64_t generation{0};
    StaticLookOwnerContext static_look_owner{};
    StaticLookBehavior static_look_behavior{StaticLookBehavior::None};
};

inline constexpr std::size_t kRunnerOutputBackendCount = 6U;

struct RunnerStatus {
    RunnerState state{RunnerState::Stopped};
    AdapterState os2l{AdapterState::Disabled};
    AdapterState os2l_discovery{AdapterState::Disabled};
    AdapterState midi_input{AdapterState::Disabled};
    AdapterState midi_output{AdapterState::Disabled};
    AdapterState artnet{AdapterState::Disabled};
    AdapterState sacn{AdapterState::Disabled};
    std::array<AdapterState, showcore::kV1UniverseCount> dmx_usb_pro{
        AdapterState::Disabled,
        AdapterState::Disabled};
    AdapterState soundswitch_micro{AdapterState::Disabled};
    AdapterState soundswitch_control_one{AdapterState::Disabled};
    std::array<showcore::OutputBackendHealth, kRunnerOutputBackendCount>
        output_backends{};
    showcore::SyncState sync_state{showcore::SyncState::Waiting};
    showcore::ClockSource clock_source{showcore::ClockSource::None};
    double bpm{0.0};
    double beat_position{0.0};
    std::int32_t active_look{-1};
    RunnerStaticLookActivation static_look{};
    showcore::AutoloopAddress active_autoloop{};
    showcore::AutoloopRepeat active_autoloop_repeat{showcore::AutoloopRepeat::Once};
    float active_autoloop_progress{0.0F};
    std::uint32_t active_autoloop_completed_cycles{0};
    std::uint64_t active_autoloop_bank_mask{~std::uint64_t{0}};
    std::int32_t active_track_script{-1};
    double active_track_script_beat{0.0};
    std::uint32_t active_track_script_consumed_cues{0};
    bool blackout{false};
    bool work_light{false};
    bool fog_armed{false};
    bool haze_armed{false};
    bool laser_armed{false};
    bool spark_armed{false};
    std::uint16_t manual_override_count{0};
    std::uint64_t frames{0};
    std::uint64_t output_frames{0};
    std::uint64_t output_queue_drops{0};
    std::uint64_t output_superseded_frames{0};
    std::uint64_t output_send_failures{0};
    std::uint64_t soundswitch_micro_write_frames{0};
    std::uint64_t soundswitch_micro_write_failures{0};
    std::uint32_t soundswitch_micro_last_error{0};
    std::uint16_t soundswitch_micro_last_nonzero_slots{0};
    std::uint64_t os2l_connections{0};
    std::uint64_t os2l_messages{0};
    std::uint64_t os2l_decode_errors{0};
    std::uint16_t os2l_listen_port{0};
    std::int32_t os2l_last_error{0};
    std::int32_t os2l_discovery_last_error{0};
    std::uint64_t dropped_beats{0};
    std::uint64_t dropped_os2l_actions{0};
    std::uint64_t midi_messages{0};
    std::uint64_t dropped_midi_actions{0};
    std::uint64_t uptime_ms{0};
    std::uint64_t last_frame_age_ms{0};
    std::uint64_t jitter_samples{0};
    std::uint64_t jitter_p99_us{0};
    std::uint64_t max_jitter_us{0};
    std::uint64_t deadline_misses{0};
    std::uint64_t scheduler_resyncs{0};
    std::uint64_t package_generation{0};
    std::uint64_t package_activations{0};
    std::uint64_t package_activation_failures{0};
    RunnerAutoloopV2Status autoloop_v2{};
};

struct RunnerMidiMonitorEvent {
    showcore::MidiMessage message{};
};

struct RunnerMidiActionEvent {
    showcore::MidiActionEvent event{};
    std::uint64_t generation{0};
    std::uint64_t owner_session_token{0U};
};

struct RunnerBeatEvent {
    std::int64_t position{0};
    double bpm{0.0};
    std::uint64_t timestamp_ms{0};
};

struct RunnerOs2lButtonEvent {
    showcore::FixedText<96> name{};
    bool on{false};
    std::uint64_t generation{0};
    std::uint64_t owner_session_token{0U};
};

struct RunnerStaticLookOwnerLossEvent {
    StaticLookOwnerKind kind{StaticLookOwnerKind::None};
    std::uint64_t owner_session_token{0U};
    std::uint64_t owner_feedback_token{0U};
    std::uint64_t generation{0U};
};

struct RunnerOutputFrame {
    showcore::DmxFrames pre_blackout_frames{};
    showcore::DmxFrames frames{};
    showcore::DmxFrameAttribution attribution{};
    std::uint64_t rendered_at_ms{0U};
    std::uint8_t sequence{0};
    std::uint64_t generation{0};
    bool blackout_applied{false};
};

inline constexpr std::size_t kRunnerOutputRouteCount =
    kRunnerOutputBackendCount;

// Per-frame route evidence is derived on the output thread from the existing
// backend health counters. Counts are backend frame/send attempts for this
// frame, not cumulative lifetime counters.
struct RunnerOutputRouteResult {
    showcore::OutputBackendKind kind{showcore::OutputBackendKind::ArtNet};
    std::uint8_t first_source_universe{0U};
    std::uint8_t source_universe_count{0U};
    bool configured{false};
    std::uint8_t attempted_frames{0U};
    std::uint8_t accepted_frames{0U};
    std::uint32_t last_error{0U};
};

// Immutable value-copy boundary for diagnostics. The scheduler publishes only
// a fixed RunnerOutputFrame to its SPSC queue. The output thread adds route
// results and replaces the latest snapshot under a reader-side mutex.
struct RunnerOutputSnapshot {
    std::uint64_t generation{0U};
    std::uint8_t sequence{0U};
    std::uint64_t rendered_at_ms{0U};
    showcore::DmxFrames pre_blackout_frames{};
    showcore::DmxFrames routed_frames{};
    showcore::DmxFrameAttribution attribution{};
    bool blackout_applied{false};
    std::array<RunnerOutputRouteResult, kRunnerOutputRouteCount> routes{};
};

class RunnerService {
public:
    using FixtureMask = std::array<std::uint64_t, (showcore::kMaxFixtures + 63U) / 64U>;

    RunnerService() noexcept;
    ~RunnerService() noexcept;

    RunnerService(const RunnerService&) = delete;
    RunnerService& operator=(const RunnerService&) = delete;

    [[nodiscard]] bool start(
        std::unique_ptr<CompiledShow> show,
        const ProjectDocument& project) noexcept;
    [[nodiscard]] RunnerActivationResult activate(
        std::unique_ptr<CompiledShow> show,
        const ProjectDocument& project,
        std::uint32_t timeout_ms = 2000U) noexcept;
    void stop() noexcept;

    [[nodiscard]] RunnerStatus status() const noexcept;
    [[nodiscard]] bool latest_output_snapshot(
        RunnerOutputSnapshot& snapshot) const noexcept;
    [[nodiscard]] bool post(const RunnerCommand& command) noexcept;
    void set_blackout(bool active) noexcept;
    void set_work_light(bool active) noexcept;
    [[nodiscard]] bool trigger_look(
        std::uint16_t index,
        StaticLookOwnerContext owner = {}) noexcept;
    [[nodiscard]] bool toggle_look(
        std::uint16_t index,
        StaticLookOwnerContext owner = {}) noexcept;
    [[nodiscard]] bool hold_look(
        std::uint16_t index,
        bool active,
        StaticLookOwnerContext owner) noexcept;
    [[nodiscard]] bool notify_static_look_owner_lost(
        StaticLookOwnerKind kind,
        std::uint64_t owner_session_token,
        std::uint64_t owner_feedback_token = 0U) noexcept;
    [[nodiscard]] bool clear_look() noexcept;
    [[nodiscard]] bool trigger_autoloop(showcore::AutoloopAddress address) noexcept;
    [[nodiscard]] bool clear_autoloop() noexcept;
    [[nodiscard]] bool next_autoloop() noexcept;
    [[nodiscard]] bool previous_autoloop() noexcept;
    [[nodiscard]] bool select_all_autoloop_banks() noexcept;
    [[nodiscard]] bool select_exclusive_autoloop_bank(std::uint16_t bank) noexcept;
    [[nodiscard]] bool set_autoloop_bank_enabled(std::uint16_t bank, bool enabled) noexcept;
    [[nodiscard]] bool trigger_track_script(std::uint16_t index) noexcept;
    [[nodiscard]] bool clear_track_script() noexcept;
    [[nodiscard]] bool set_manual_bpm(double bpm) noexcept;
    [[nodiscard]] bool tap_tempo() noexcept;
    [[nodiscard]] bool set_property(
        std::uint16_t fixture_id,
        showcore::Property property,
        float value,
        bool active = true) noexcept;
    [[nodiscard]] bool clear_manual_overrides() noexcept;
    [[nodiscard]] bool set_group_property(
        const showcore::FixtureGroup& fixtures,
        showcore::Property property,
        float value,
        bool active = true) noexcept;
    [[nodiscard]] bool set_hazard_armed(
        showcore::Property property,
        bool armed) noexcept;
    [[nodiscard]] bool poll_midi_monitor(RunnerMidiMonitorEvent& event) noexcept;

    [[nodiscard]] static std::uint64_t monotonic_ms() noexcept;

private:
    struct RuntimeState;
    struct ActivationState;
    using OutputFrame = RunnerOutputFrame;

    using CommandQueue = showcore::SpscQueue<RunnerCommand, 513>;
    using BeatEvent = RunnerBeatEvent;
    using BeatQueue = showcore::SpscQueue<BeatEvent, 1025>;
    using Os2lButtonQueue = showcore::SpscQueue<RunnerOs2lButtonEvent, 257>;
    using MidiActionQueue = showcore::SpscQueue<RunnerMidiActionEvent, 1025>;
    using MidiMonitorQueue = showcore::SpscQueue<RunnerMidiMonitorEvent, 257>;
    using OwnerLossQueue =
        showcore::SpscQueue<RunnerStaticLookOwnerLossEvent, 65>;
    using OutputQueue = showcore::SpscQueue<OutputFrame, 9>;

    static void os2l_callback(
        const showcore::Os2lEvent& event,
        showcore::Os2lParseError error,
        std::string_view raw,
        void* context) noexcept;
    void run_scheduler() noexcept;
    void run_input() noexcept;
    void run_output() noexcept;
    void publish_static_look_status(
        const RunnerStaticLookActivation& activation) noexcept;

    std::unique_ptr<ActivationState> active_activation_;
    std::unique_ptr<ActivationState> pending_activation_;
    std::atomic<ActivationState*> published_activation_{nullptr};
    std::mutex activation_mutex_{};
    ConnectionSettings connections_{};
    std::string project_id_;

    CommandQueue commands_{};
    BeatQueue beats_{};
    Os2lButtonQueue os2l_buttons_{};
    MidiActionQueue midi_actions_{};
    MidiMonitorQueue midi_monitor_{};
    OwnerLossQueue owner_losses_{};
    OutputQueue output_queue_{};
    mutable std::mutex output_snapshot_mutex_{};
    RunnerOutputSnapshot latest_output_snapshot_{};
    bool has_output_snapshot_{false};
    std::thread scheduler_thread_{};
    std::thread input_thread_{};
    std::thread output_thread_{};

    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> blackout_requested_{false};
    std::atomic<bool> work_light_requested_{false};
    std::atomic<RunnerState> state_{RunnerState::Stopped};
    std::atomic<AdapterState> os2l_state_{AdapterState::Disabled};
    std::atomic<AdapterState> os2l_discovery_state_{AdapterState::Disabled};
    std::atomic<AdapterState> midi_input_state_{AdapterState::Disabled};
    std::atomic<AdapterState> midi_output_state_{AdapterState::Disabled};
    std::atomic<AdapterState> artnet_state_{AdapterState::Disabled};
    std::atomic<AdapterState> sacn_state_{AdapterState::Disabled};
    std::array<std::atomic<AdapterState>, showcore::kV1UniverseCount>
        dmx_usb_pro_state_{};
    std::atomic<AdapterState> soundswitch_micro_state_{AdapterState::Disabled};
    std::atomic<AdapterState> soundswitch_control_one_state_{AdapterState::Disabled};
    std::array<showcore::AtomicOutputBackendHealth,
               kRunnerOutputBackendCount>
        output_health_{};
    std::atomic<showcore::SyncState> sync_state_{showcore::SyncState::Waiting};
    std::atomic<showcore::ClockSource> clock_source_{showcore::ClockSource::None};
    std::atomic<std::uint32_t> bpm_milli_{0};
    std::atomic<std::int64_t> beat_milli_{0};
    std::atomic<std::uint64_t> static_look_status_sequence_{0U};
    std::atomic<std::int32_t> active_look_{-1};
    std::atomic<std::uint64_t> static_look_package_generation_{0U};
    std::atomic<std::uint64_t> static_look_activation_generation_{0U};
    std::atomic<StaticLookOwnerKind> static_look_owner_kind_{
        StaticLookOwnerKind::None};
    std::atomic<std::uint64_t> static_look_owner_feedback_token_{0U};
    std::atomic<StaticLookBehavior> static_look_behavior_{
        StaticLookBehavior::None};
    std::atomic<StaticLookActivationStatus> static_look_activation_status_{
        StaticLookActivationStatus::None};
    std::atomic<std::uint64_t> static_look_activated_at_ms_{0U};
    std::atomic<std::uint16_t> static_look_transition_milli_{0U};
    std::atomic<std::uint64_t> active_autoloop_playback_{0x0FFFU};
    std::atomic<std::uint64_t> active_autoloop_bank_mask_{~std::uint64_t{0}};
    std::atomic<AutoloopRuntimeMode> autoloop_v2_mode_{
        AutoloopRuntimeMode::LegacyV1};
    std::atomic<AutoloopTrackScriptOwner> autoloop_v2_track_owner_{
        AutoloopTrackScriptOwner::None};
    std::atomic<bool> autoloop_v2_track_suppressed_by_replace_{false};
    std::atomic<bool> autoloop_v2_package_active_{false};
    std::atomic<std::uint64_t> autoloop_v2_generation_{0U};
    std::atomic<showcore::AutoloopDirectorFault> autoloop_v2_fault_{
        showcore::AutoloopDirectorFault::None};
    std::atomic<showcore::AutoloopDirectorResult> autoloop_v2_result_{
        showcore::AutoloopDirectorResult::None};
    std::atomic<showcore::AutoloopDirectorSource> autoloop_v2_source_{
        showcore::AutoloopDirectorSource::None};
    std::atomic<showcore::CompiledAutoloopPlaybackMode> autoloop_v2_playback_mode_{
        showcore::CompiledAutoloopPlaybackMode::Overlay};
    std::atomic<std::uint64_t> autoloop_v2_playback_{0x0FFFU};
    std::atomic<std::uint64_t> autoloop_v2_active_bank_mask_{
        ~std::uint64_t{0U}};
    std::atomic<bool> autoloop_v2_has_pending_bank_mask_{false};
    std::atomic<std::uint64_t> autoloop_v2_pending_bank_mask_{0U};
    std::atomic<std::int32_t> active_track_script_{-1};
    std::atomic<std::int64_t> active_track_script_beat_milli_{0};
    std::atomic<std::uint32_t> active_track_script_consumed_cues_{0};
    std::atomic<bool> fog_armed_{false};
    std::atomic<bool> haze_armed_{false};
    std::atomic<bool> laser_armed_{false};
    std::atomic<bool> spark_armed_{false};
    std::atomic<std::uint16_t> manual_override_count_{0};
    std::atomic<std::uint64_t> frames_{0};
    std::atomic<std::uint64_t> output_frames_{0};
    std::atomic<std::uint64_t> output_queue_drops_{0};
    std::atomic<std::uint64_t> output_superseded_frames_{0};
    std::atomic<std::uint64_t> output_send_failures_{0};
    std::atomic<std::uint64_t> soundswitch_micro_write_frames_{0};
    std::atomic<std::uint64_t> soundswitch_micro_write_failures_{0};
    std::atomic<std::uint32_t> soundswitch_micro_last_error_{0};
    std::atomic<std::uint16_t> soundswitch_micro_last_nonzero_slots_{0};
    std::atomic<std::uint64_t> os2l_connections_{0};
    std::atomic<std::uint64_t> os2l_messages_{0};
    std::atomic<std::uint64_t> os2l_decode_errors_{0};
    std::atomic<std::uint16_t> os2l_listen_port_{0};
    std::atomic<std::int32_t> os2l_last_error_{0};
    std::atomic<std::int32_t> os2l_discovery_last_error_{0};
    std::atomic<std::uint64_t> dropped_beats_{0};
    std::atomic<std::uint64_t> dropped_os2l_actions_{0};
    std::atomic<std::uint64_t> midi_messages_{0};
    std::atomic<std::uint64_t> dropped_midi_actions_{0};
    std::atomic<std::uint64_t> input_owner_session_sequence_{0U};
    std::atomic<std::uint64_t> os2l_owner_session_token_{0U};
    std::atomic<std::uint64_t> started_at_ms_{0};
    std::atomic<std::uint64_t> last_frame_ms_{0};
    std::atomic<std::uint64_t> jitter_samples_{0};
    std::atomic<std::uint64_t> jitter_p99_us_{0};
    std::atomic<std::uint64_t> max_jitter_us_{0};
    std::atomic<std::uint64_t> deadline_misses_{0};
    std::atomic<std::uint64_t> scheduler_resyncs_{0};
    std::atomic<std::uint64_t> scheduler_activation_ack_{0};
    std::atomic<std::uint64_t> input_activation_ack_{0};
    std::atomic<std::uint64_t> output_activation_ack_{0};
    std::atomic<std::uint64_t> package_generation_{0};
    std::atomic<std::uint64_t> package_activations_{0};
    std::atomic<std::uint64_t> package_activation_failures_{0};
    std::uint64_t static_look_activation_sequence_{0U};
};

}  // namespace emberlights
