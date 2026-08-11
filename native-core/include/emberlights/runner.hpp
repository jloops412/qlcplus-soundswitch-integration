#pragma once

#include "emberlights/compiler.hpp"
#include "emberlights/project.hpp"
#include "showcore/midi.hpp"
#include "showcore/os2l.hpp"
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

enum class RunnerCommandType : std::uint8_t {
    TriggerLook,
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
    SetGroupProperty
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
};

struct RunnerStatus {
    RunnerState state{RunnerState::Stopped};
    AdapterState os2l{AdapterState::Disabled};
    AdapterState midi_input{AdapterState::Disabled};
    AdapterState midi_output{AdapterState::Disabled};
    AdapterState artnet{AdapterState::Disabled};
    AdapterState sacn{AdapterState::Disabled};
    std::array<AdapterState, showcore::kV1UniverseCount> dmx_usb_pro{
        AdapterState::Disabled,
        AdapterState::Disabled};
    AdapterState soundswitch_micro{AdapterState::Disabled};
    showcore::SyncState sync_state{showcore::SyncState::Waiting};
    showcore::ClockSource clock_source{showcore::ClockSource::None};
    double bpm{0.0};
    double beat_position{0.0};
    std::int32_t active_look{-1};
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
    std::uint64_t os2l_connections{0};
    std::uint64_t os2l_messages{0};
    std::uint64_t os2l_decode_errors{0};
    std::uint64_t dropped_beats{0};
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
};

struct RunnerMidiMonitorEvent {
    showcore::MidiMessage message{};
};

struct RunnerMidiActionEvent {
    showcore::MidiActionEvent event{};
    std::uint64_t generation{0};
};

struct RunnerBeatEvent {
    std::int64_t position{0};
    double bpm{0.0};
    std::uint64_t timestamp_ms{0};
};

struct RunnerOutputFrame {
    showcore::DmxFrames frames{};
    std::uint8_t sequence{0};
    std::uint64_t generation{0};
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
    [[nodiscard]] bool post(const RunnerCommand& command) noexcept;
    void set_blackout(bool active) noexcept;
    void set_work_light(bool active) noexcept;
    [[nodiscard]] bool trigger_look(std::uint16_t index) noexcept;
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
    using MidiActionQueue = showcore::SpscQueue<RunnerMidiActionEvent, 1025>;
    using MidiMonitorQueue = showcore::SpscQueue<RunnerMidiMonitorEvent, 257>;
    using OutputQueue = showcore::SpscQueue<OutputFrame, 9>;

    static void os2l_callback(
        const showcore::Os2lEvent& event,
        showcore::Os2lParseError error,
        std::string_view raw,
        void* context) noexcept;
    void run_scheduler() noexcept;
    void run_input() noexcept;
    void run_output() noexcept;

    std::unique_ptr<ActivationState> active_activation_;
    std::unique_ptr<ActivationState> pending_activation_;
    std::atomic<ActivationState*> published_activation_{nullptr};
    std::mutex activation_mutex_{};
    ConnectionSettings connections_{};
    std::string project_id_;

    CommandQueue commands_{};
    BeatQueue beats_{};
    MidiActionQueue midi_actions_{};
    MidiMonitorQueue midi_monitor_{};
    OutputQueue output_queue_{};
    std::thread scheduler_thread_{};
    std::thread input_thread_{};
    std::thread output_thread_{};

    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> blackout_requested_{false};
    std::atomic<bool> work_light_requested_{false};
    std::atomic<RunnerState> state_{RunnerState::Stopped};
    std::atomic<AdapterState> os2l_state_{AdapterState::Disabled};
    std::atomic<AdapterState> midi_input_state_{AdapterState::Disabled};
    std::atomic<AdapterState> midi_output_state_{AdapterState::Disabled};
    std::atomic<AdapterState> artnet_state_{AdapterState::Disabled};
    std::atomic<AdapterState> sacn_state_{AdapterState::Disabled};
    std::array<std::atomic<AdapterState>, showcore::kV1UniverseCount>
        dmx_usb_pro_state_{};
    std::atomic<AdapterState> soundswitch_micro_state_{AdapterState::Disabled};
    std::atomic<showcore::SyncState> sync_state_{showcore::SyncState::Waiting};
    std::atomic<showcore::ClockSource> clock_source_{showcore::ClockSource::None};
    std::atomic<std::uint32_t> bpm_milli_{0};
    std::atomic<std::int64_t> beat_milli_{0};
    std::atomic<std::int32_t> active_look_{-1};
    std::atomic<std::uint64_t> active_autoloop_playback_{0x0FFFU};
    std::atomic<std::uint64_t> active_autoloop_bank_mask_{~std::uint64_t{0}};
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
    std::atomic<std::uint64_t> os2l_connections_{0};
    std::atomic<std::uint64_t> os2l_messages_{0};
    std::atomic<std::uint64_t> os2l_decode_errors_{0};
    std::atomic<std::uint64_t> dropped_beats_{0};
    std::atomic<std::uint64_t> midi_messages_{0};
    std::atomic<std::uint64_t> dropped_midi_actions_{0};
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
};

}  // namespace emberlights
