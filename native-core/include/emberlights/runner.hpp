#pragma once

#include "emberlights/compiler.hpp"
#include "emberlights/project.hpp"
#include "showcore/midi.hpp"
#include "showcore/os2l.hpp"
#include "showcore/spsc_queue.hpp"
#include "showcore/sync_manager.hpp"
#include "showcore/types.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
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

enum class RunnerCommandType : std::uint8_t {
    TriggerLook,
    ClearLook,
    TriggerAutoloop,
    ClearAutoloop,
    NextAutoloop,
    PreviousAutoloop,
    SetManualBpm,
    TapTempo,
    SetProperty,
    ArmHazard,
    SetTrackPlaying
};

struct RunnerCommand {
    RunnerCommandType type{RunnerCommandType::ClearLook};
    std::uint16_t target{0};
    showcore::Property property{showcore::Property::Intensity};
    float value{0.0F};
    bool active{false};
    std::uint64_t timestamp_ms{0};
};

struct RunnerStatus {
    RunnerState state{RunnerState::Stopped};
    AdapterState os2l{AdapterState::Disabled};
    AdapterState midi_input{AdapterState::Disabled};
    AdapterState midi_output{AdapterState::Disabled};
    AdapterState artnet{AdapterState::Disabled};
    AdapterState sacn{AdapterState::Disabled};
    showcore::SyncState sync_state{showcore::SyncState::Waiting};
    showcore::ClockSource clock_source{showcore::ClockSource::None};
    double bpm{0.0};
    double beat_position{0.0};
    std::int32_t active_look{-1};
    showcore::AutoloopAddress active_autoloop{};
    bool blackout{false};
    bool work_light{false};
    bool fog_armed{false};
    bool haze_armed{false};
    bool laser_armed{false};
    bool spark_armed{false};
    std::uint64_t frames{0};
    std::uint64_t output_frames{0};
    std::uint64_t output_queue_drops{0};
    std::uint64_t output_send_failures{0};
    std::uint64_t os2l_connections{0};
    std::uint64_t os2l_messages{0};
    std::uint64_t os2l_decode_errors{0};
    std::uint64_t dropped_beats{0};
    std::uint64_t midi_messages{0};
    std::uint64_t dropped_midi_actions{0};
    std::uint64_t max_jitter_us{0};
};

struct RunnerMidiMonitorEvent {
    showcore::MidiMessage message{};
};

struct RunnerBeatEvent {
    std::int64_t position{0};
    double bpm{0.0};
    std::uint64_t timestamp_ms{0};
};

struct RunnerOutputFrame {
    showcore::DmxFrames frames{};
    std::uint8_t sequence{0};
};

class RunnerService {
public:
    RunnerService() noexcept;
    ~RunnerService() noexcept;

    RunnerService(const RunnerService&) = delete;
    RunnerService& operator=(const RunnerService&) = delete;

    [[nodiscard]] bool start(
        std::unique_ptr<CompiledShow> show,
        const ProjectDocument& project) noexcept;
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
    [[nodiscard]] bool set_manual_bpm(double bpm) noexcept;
    [[nodiscard]] bool tap_tempo() noexcept;
    [[nodiscard]] bool set_property(
        std::uint16_t fixture_id,
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
    using OutputFrame = RunnerOutputFrame;

    using CommandQueue = showcore::SpscQueue<RunnerCommand, 513>;
    using BeatEvent = RunnerBeatEvent;
    using BeatQueue = showcore::SpscQueue<BeatEvent, 1025>;
    using MidiActionQueue = showcore::SpscQueue<showcore::MidiActionEvent, 1025>;
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

    std::unique_ptr<CompiledShow> show_;
    std::unique_ptr<RuntimeState> runtime_;
    ConnectionSettings connections_{};
    SafetySettings safety_{};
    std::string project_id_;
    std::string project_name_;

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
    std::atomic<showcore::SyncState> sync_state_{showcore::SyncState::Waiting};
    std::atomic<showcore::ClockSource> clock_source_{showcore::ClockSource::None};
    std::atomic<std::uint32_t> bpm_milli_{0};
    std::atomic<std::int64_t> beat_milli_{0};
    std::atomic<std::int32_t> active_look_{-1};
    std::atomic<std::uint16_t> active_autoloop_{0xFFFFU};
    std::atomic<bool> fog_armed_{false};
    std::atomic<bool> haze_armed_{false};
    std::atomic<bool> laser_armed_{false};
    std::atomic<bool> spark_armed_{false};
    std::atomic<std::uint64_t> frames_{0};
    std::atomic<std::uint64_t> output_frames_{0};
    std::atomic<std::uint64_t> output_queue_drops_{0};
    std::atomic<std::uint64_t> output_send_failures_{0};
    std::atomic<std::uint64_t> os2l_connections_{0};
    std::atomic<std::uint64_t> os2l_messages_{0};
    std::atomic<std::uint64_t> os2l_decode_errors_{0};
    std::atomic<std::uint64_t> dropped_beats_{0};
    std::atomic<std::uint64_t> midi_messages_{0};
    std::atomic<std::uint64_t> dropped_midi_actions_{0};
    std::atomic<std::uint64_t> max_jitter_us_{0};
};

}  // namespace emberlights
