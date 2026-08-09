#include "emberlights/runner.hpp"

#include "showcore/artnet.hpp"
#include "showcore/dmx_usb_pro.hpp"
#include "showcore/look.hpp"
#include "showcore/os2l_server.hpp"
#include "showcore/sacn.hpp"
#include "showcore/winmm_midi.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
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

}  // namespace

struct RunnerService::RuntimeState {
    showcore::SyncManager sync{};
    showcore::StaticLookPlayer static_look{showcore::LayerId::EventMoment};
    showcore::AutoloopPlayer autonomous{showcore::LayerId::Autonomous};
    showcore::AutoloopPlayer manual_autoloop{showcore::LayerId::ManualAutoloop};
    std::array<bool, showcore::kMaxMidiMappings> mapping_toggles{};
    std::array<std::array<float, showcore::kPropertyCount>, showcore::kMaxFixtures>
        manual_values{};
    std::array<std::uint64_t, 8> tap_intervals{};
    std::size_t tap_interval_count{0};
    std::size_t tap_interval_cursor{0};
    std::uint64_t last_tap_ms{0};
    std::array<std::uint64_t, kJitterBucketCount> jitter_histogram{};
    std::uint64_t jitter_sample_count{0};
    showcore::AutoloopAddress selected_autoloop{};
    std::int32_t selected_look{-1};
    bool track_playing{true};
    bool applied_work_light{false};
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

bool RunnerService::start(
    std::unique_ptr<CompiledShow> show,
    const ProjectDocument& project) noexcept {
    if (show == nullptr || state_.load(std::memory_order_acquire) != RunnerState::Stopped) {
        return false;
    }
    state_.store(RunnerState::Starting, std::memory_order_release);
    try {
        connections_ = project.connections;
        safety_ = project.safety;
        project_id_ = project.id;
        project_name_ = project.name.empty() ? "EmberLights" : project.name;
        runtime_ = std::make_unique<RuntimeState>();
        show_ = std::move(show);
    } catch (...) {
        show_.reset();
        runtime_.reset();
        state_.store(RunnerState::Stopped, std::memory_order_release);
        return false;
    }

    commands_.reset();
    beats_.reset();
    midi_actions_.reset();
    midi_monitor_.reset();
    output_queue_.reset();
    stop_requested_.store(false, std::memory_order_release);
    blackout_requested_.store(false, std::memory_order_release);
    work_light_requested_.store(false, std::memory_order_release);
    sync_state_.store(showcore::SyncState::Waiting, std::memory_order_relaxed);
    clock_source_.store(showcore::ClockSource::None, std::memory_order_relaxed);
    bpm_milli_.store(0, std::memory_order_relaxed);
    beat_milli_.store(0, std::memory_order_relaxed);
    active_look_.store(-1, std::memory_order_relaxed);
    active_autoloop_.store(0xFFFFU, std::memory_order_relaxed);
    fog_armed_.store(false, std::memory_order_relaxed);
    haze_armed_.store(false, std::memory_order_relaxed);
    laser_armed_.store(false, std::memory_order_relaxed);
    spark_armed_.store(false, std::memory_order_relaxed);
    frames_.store(0, std::memory_order_relaxed);
    output_frames_.store(0, std::memory_order_relaxed);
    output_queue_drops_.store(0, std::memory_order_relaxed);
    output_superseded_frames_.store(0, std::memory_order_relaxed);
    output_send_failures_.store(0, std::memory_order_relaxed);
    os2l_connections_.store(0, std::memory_order_relaxed);
    os2l_messages_.store(0, std::memory_order_relaxed);
    os2l_decode_errors_.store(0, std::memory_order_relaxed);
    dropped_beats_.store(0, std::memory_order_relaxed);
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
    os2l_state_.store(
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
        show_.reset();
        runtime_.reset();
        state_.store(RunnerState::Stopped, std::memory_order_release);
        return false;
    }
    return true;
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
    show_.reset();
    runtime_.reset();
    state_.store(RunnerState::Stopped, std::memory_order_release);
}

RunnerStatus RunnerService::status() const noexcept {
    RunnerStatus snapshot;
    snapshot.state = state_.load(std::memory_order_acquire);
    snapshot.os2l = os2l_state_.load(std::memory_order_relaxed);
    snapshot.midi_input = midi_input_state_.load(std::memory_order_relaxed);
    snapshot.midi_output = midi_output_state_.load(std::memory_order_relaxed);
    snapshot.artnet = artnet_state_.load(std::memory_order_relaxed);
    snapshot.sacn = sacn_state_.load(std::memory_order_relaxed);
    for (std::size_t universe = 0; universe < snapshot.dmx_usb_pro.size(); ++universe) {
        snapshot.dmx_usb_pro[universe] =
            dmx_usb_pro_state_[universe].load(std::memory_order_relaxed);
    }
    snapshot.sync_state = sync_state_.load(std::memory_order_relaxed);
    snapshot.clock_source = clock_source_.load(std::memory_order_relaxed);
    snapshot.bpm = static_cast<double>(bpm_milli_.load(std::memory_order_relaxed)) / 1000.0;
    snapshot.beat_position =
        static_cast<double>(beat_milli_.load(std::memory_order_relaxed)) / 1000.0;
    snapshot.active_look = active_look_.load(std::memory_order_relaxed);
    snapshot.active_autoloop = decode_autoloop(
        active_autoloop_.load(std::memory_order_relaxed));
    snapshot.blackout = blackout_requested_.load(std::memory_order_relaxed);
    snapshot.work_light = work_light_requested_.load(std::memory_order_relaxed);
    snapshot.fog_armed = fog_armed_.load(std::memory_order_relaxed);
    snapshot.haze_armed = haze_armed_.load(std::memory_order_relaxed);
    snapshot.laser_armed = laser_armed_.load(std::memory_order_relaxed);
    snapshot.spark_armed = spark_armed_.load(std::memory_order_relaxed);
    snapshot.frames = frames_.load(std::memory_order_relaxed);
    snapshot.output_frames = output_frames_.load(std::memory_order_relaxed);
    snapshot.output_queue_drops = output_queue_drops_.load(std::memory_order_relaxed);
    snapshot.output_superseded_frames =
        output_superseded_frames_.load(std::memory_order_relaxed);
    snapshot.output_send_failures = output_send_failures_.load(std::memory_order_relaxed);
    snapshot.os2l_connections = os2l_connections_.load(std::memory_order_relaxed);
    snapshot.os2l_messages = os2l_messages_.load(std::memory_order_relaxed);
    snapshot.os2l_decode_errors = os2l_decode_errors_.load(std::memory_order_relaxed);
    snapshot.dropped_beats = dropped_beats_.load(std::memory_order_relaxed);
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
    return snapshot;
}

bool RunnerService::post(const RunnerCommand& command) noexcept {
    return state_.load(std::memory_order_acquire) == RunnerState::Running &&
        commands_.try_push(command);
}

void RunnerService::set_blackout(bool active) noexcept {
    blackout_requested_.store(active, std::memory_order_release);
}

void RunnerService::set_work_light(bool active) noexcept {
    work_light_requested_.store(active, std::memory_order_release);
}

bool RunnerService::trigger_look(std::uint16_t index) noexcept {
    return post({RunnerCommandType::TriggerLook, index});
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
        if (event.button.name.view() == "blackout") {
            service.set_blackout(event.button.on);
        } else if (event.button.name.view() == "worklight" ||
                   event.button.name.view() == "white") {
            service.set_work_light(event.button.on);
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
    auto next_os2l_retry = SteadyClock::now();
    auto next_midi_retry = SteadyClock::now();

    while (!stop_requested_.load(std::memory_order_acquire)) {
        const auto now = SteadyClock::now();
        if (connections_.os2l_enabled && !os2l_open && now >= next_os2l_retry) {
            os2l_state_.store(AdapterState::Starting, std::memory_order_relaxed);
            os2l_open = os2l.open_ipv4(connections_.os2l_bind, connections_.os2l_port);
            os2l_state_.store(
                os2l_open ? AdapterState::Waiting : AdapterState::Fault,
                std::memory_order_relaxed);
            next_os2l_retry = now + std::chrono::seconds(2);
        }
        if (os2l_open) {
            const auto poll = os2l.poll(&RunnerService::os2l_callback, this, 2);
            if (poll == showcore::Os2lPollResult::Error) {
                os2l.close();
                os2l_open = false;
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
            os2l_connections_.store(stats.connections, std::memory_order_relaxed);
            os2l_messages_.store(stats.messages, std::memory_order_relaxed);
            os2l_decode_errors_.store(stats.decode_errors, std::memory_order_relaxed);
        }

        if (now >= next_midi_retry) {
            if (connections_.midi_input_index >= 0 && !midi_input_open) {
                midi_input_state_.store(AdapterState::Starting, std::memory_order_relaxed);
                midi_input_open = midi_input.open(
                    static_cast<std::uint32_t>(connections_.midi_input_index), 1U);
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

        if (midi_input_open) {
            showcore::MidiMessage message;
            while (midi_input.poll(message)) {
                midi_messages_.fetch_add(1, std::memory_order_relaxed);
                static_cast<void>(midi_monitor_.try_push({message}));
                std::array<showcore::MidiActionEvent, showcore::kMaxMidiActionsPerMessage> actions{};
                const auto count = show_->midi_mappings().process(message, actions);
                for (std::size_t index = 0; index < count; ++index) {
                    if (!midi_actions_.try_push(actions[index])) {
                        dropped_midi_actions_.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    os2l.close();
    midi_input.close_all();
    midi_output.close_all();
}

void RunnerService::run_scheduler() noexcept {
#ifdef _WIN32
    static_cast<void>(::timeBeginPeriod(1U));
#endif
    auto& runtime = *runtime_;
    auto& engine = show_->engine();
    engine.safety().strobe_allowed = safety_.strobe_allowed;
    engine.safety().max_strobe = safety_.max_strobe;
    engine.safety().max_intensity = safety_.max_intensity;
    engine.safety().fog_armed = !safety_.fog_requires_arm;
    engine.safety().haze_armed = !safety_.haze_requires_arm;
    engine.safety().laser_armed = !safety_.laser_requires_arm;
    engine.safety().spark_armed = !safety_.spark_requires_arm;
    fog_armed_.store(engine.safety().fog_armed, std::memory_order_relaxed);
    haze_armed_.store(engine.safety().haze_armed, std::memory_order_relaxed);
    laser_armed_.store(engine.safety().laser_armed, std::memory_order_relaxed);
    spark_armed_.store(engine.safety().spark_armed, std::memory_order_relaxed);
    runtime.sync.set_manual_bpm(connections_.manual_bpm, monotonic_ms());

    auto default_address = show_->autoloops().next_available();
    if (default_address.valid()) {
        static_cast<void>(runtime.autonomous.trigger(
            show_->autoloops(),
            default_address,
            showcore::AutoloopRepeat::Infinite,
            0.0,
            true,
            engine.layers()));
    }

    const auto frame_period = std::chrono::microseconds(1'000'000 / connections_.frame_rate);
    auto next_frame = SteadyClock::now();
    std::uint8_t sequence = 1U;
    state_.store(RunnerState::Running, std::memory_order_release);

    auto trigger_loop = [&](showcore::AutoloopAddress address, double beat_position) noexcept {
        if (!address.valid()) {
            return;
        }
        if (runtime.manual_autoloop.trigger(
                show_->autoloops(),
                address,
                show_->autoloop_repeat(address),
                beat_position,
                runtime.track_playing,
                engine.layers())) {
            runtime.selected_autoloop = address;
        }
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
                for (std::size_t index = 0; index < runtime.tap_interval_count; ++index) {
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
                            std::uint64_t now_ms) noexcept {
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

        const auto& action = event.action;
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
                current = event.value;
            }
            engine.layers().set(
                showcore::LayerId::ManualOverride,
                action.target_id,
                action.property,
                active ? showcore::PropertyValue::set(current)
                       : showcore::PropertyValue::release());
            break;
        }
        case showcore::ActionType::Blackout:
            set_blackout(active);
            break;
        case showcore::ActionType::TriggerLook:
            if (active) {
                if (const auto* look = show_->look(action.target_id); look != nullptr &&
                    runtime.static_look.trigger(
                        *look, now_ms, show_->look_fade_ms(action.target_id), engine.layers())) {
                    runtime.selected_look = action.target_id;
                }
            } else {
                runtime.static_look.clear(now_ms, 100U, engine.layers());
                runtime.selected_look = -1;
            }
            break;
        case showcore::ActionType::TriggerAutoloop:
            if (active) {
                trigger_loop(decode_autoloop(action.target_id), beat_position);
            } else {
                runtime.manual_autoloop.clear(engine.layers());
                runtime.selected_autoloop = {};
            }
            break;
        case showcore::ActionType::TapTempo: {
            apply_tap(now_ms);
            break;
        }
        case showcore::ActionType::ArmFog:
            arm_hazard(showcore::Property::Fog, active);
            break;
        case showcore::ActionType::ClearLook:
            if (active) {
                runtime.static_look.clear(now_ms, 100U, engine.layers());
                runtime.selected_look = -1;
            }
            break;
        case showcore::ActionType::ClearAutoloop:
            if (active) {
                runtime.manual_autoloop.clear(engine.layers());
                runtime.selected_autoloop = {};
            }
            break;
        case showcore::ActionType::NextAutoloop:
            if (active) {
                trigger_loop(show_->autoloops().next_available(runtime.selected_autoloop), beat_position);
            }
            break;
        case showcore::ActionType::PreviousAutoloop:
            if (active) {
                trigger_loop(show_->autoloops().previous_available(runtime.selected_autoloop), beat_position);
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

    while (!stop_requested_.load(std::memory_order_acquire)) {
        std::this_thread::sleep_until(next_frame);
        const auto now = SteadyClock::now();
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
        const auto now_ms = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count());

        BeatEvent beat;
        while (beats_.try_pop(beat)) {
            runtime.sync.on_os2l_beat(beat.position, beat.bpm, beat.timestamp_ms);
        }
        auto clock = runtime.sync.tick(now_ms);

        RunnerCommand command;
        while (commands_.try_pop(command)) {
            switch (command.type) {
            case RunnerCommandType::TriggerLook:
                if (const auto* look = show_->look(command.target); look != nullptr &&
                    runtime.static_look.trigger(
                        *look, now_ms, show_->look_fade_ms(command.target), engine.layers())) {
                    runtime.selected_look = command.target;
                }
                break;
            case RunnerCommandType::ClearLook:
                runtime.static_look.clear(now_ms, 100U, engine.layers());
                runtime.selected_look = -1;
                break;
            case RunnerCommandType::TriggerAutoloop:
                trigger_loop(decode_autoloop(command.target), clock.beat_position);
                break;
            case RunnerCommandType::ClearAutoloop:
                runtime.manual_autoloop.clear(engine.layers());
                runtime.selected_autoloop = {};
                break;
            case RunnerCommandType::NextAutoloop:
                trigger_loop(
                    show_->autoloops().next_available(runtime.selected_autoloop),
                    clock.beat_position);
                break;
            case RunnerCommandType::PreviousAutoloop:
                trigger_loop(
                    show_->autoloops().previous_available(runtime.selected_autoloop),
                    clock.beat_position);
                break;
            case RunnerCommandType::SetManualBpm:
                runtime.sync.set_manual_bpm(command.value, command.timestamp_ms);
                break;
            case RunnerCommandType::TapTempo:
                apply_tap(command.timestamp_ms);
                break;
            case RunnerCommandType::SetProperty:
                if (command.target < show_->fixture_count()) {
                    runtime.manual_values[command.target]
                        [static_cast<std::size_t>(command.property)] = command.value;
                    engine.layers().set(
                        showcore::LayerId::ManualOverride,
                        command.target,
                        command.property,
                        command.active ? showcore::PropertyValue::set(command.value)
                                       : showcore::PropertyValue::release());
                }
                break;
            case RunnerCommandType::ArmHazard:
                arm_hazard(command.property, command.active);
                break;
            case RunnerCommandType::SetTrackPlaying:
                runtime.track_playing = command.active;
                break;
            }
        }

        showcore::MidiActionEvent midi_action;
        while (midi_actions_.try_pop(midi_action)) {
            apply_action(midi_action, clock.beat_position, now_ms);
        }

        const bool work_light = work_light_requested_.load(std::memory_order_acquire);
        if (work_light != runtime.applied_work_light) {
            engine.layers().clear_layer(showcore::LayerId::Emergency);
            if (work_light) {
                for (std::uint16_t fixture = 0; fixture < show_->fixture_count(); ++fixture) {
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
        runtime.autonomous.tick(clock.beat_position, runtime.track_playing, engine.layers());
        runtime.manual_autoloop.tick(
            clock.beat_position, runtime.track_playing, engine.layers());
        runtime.static_look.tick(now_ms, engine.layers());
        engine.tick();

        OutputFrame output;
        output.frames = engine.frames();
        if (blackout_requested_.load(std::memory_order_acquire)) {
            output.frames.clear();
        }
        output.sequence = sequence;
        if (!output_queue_.try_push(output)) {
            output_queue_drops_.fetch_add(1, std::memory_order_relaxed);
        }
        ++sequence;
        if (sequence == 0U) {
            sequence = 1U;
        }

        const auto& manual_status = runtime.manual_autoloop.status();
        const auto& autonomous_status = runtime.autonomous.status();
        active_autoloop_.store(
            encode_autoloop(manual_status.active ? manual_status.address
                                                 : autonomous_status.address),
            std::memory_order_relaxed);
        active_look_.store(runtime.selected_look, std::memory_order_relaxed);
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
#ifdef _WIN32
    static_cast<void>(::timeEndPeriod(1U));
#endif
}

void RunnerService::run_output() noexcept {
    showcore::ArtNetSender artnet;
    std::array<showcore::SacnSender, showcore::kV1UniverseCount> sacn;
    std::array<showcore::DmxUsbProSender, showcore::kV1UniverseCount> dmx_usb_pro;
    const auto cid = showcore::make_sacn_cid(project_id_);

    auto open_missing_outputs = [&]() noexcept {
        bool artnet_ready = !connections_.artnet_enabled;
        bool sacn_ready = !connections_.sacn_enabled;
        bool usb_ready = true;
        if (connections_.artnet_enabled) {
            if (!artnet.is_open()) {
                artnet_state_.store(AdapterState::Starting, std::memory_order_relaxed);
                static_cast<void>(artnet.open_ipv4(connections_.artnet_destination));
            }
            artnet_ready = artnet.is_open();
            artnet_state_.store(
                artnet_ready ? AdapterState::Ready : AdapterState::Fault,
                std::memory_order_relaxed);
        }
        if (connections_.sacn_enabled) {
            sacn_state_.store(AdapterState::Starting, std::memory_order_relaxed);
            sacn_ready = true;
            for (std::uint16_t universe = 0; universe < showcore::kV1UniverseCount; ++universe) {
                const auto output_universe = static_cast<std::uint16_t>(
                    connections_.sacn_universe_base + universe);
                if (!sacn[universe].is_open()) {
                    static_cast<void>(connections_.sacn_destination == "multicast"
                        ? sacn[universe].open_multicast(output_universe)
                        : sacn[universe].open_ipv4(connections_.sacn_destination));
                }
                sacn_ready = sacn_ready && sacn[universe].is_open();
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
                dmx_usb_pro_state_[universe].store(
                    AdapterState::Starting, std::memory_order_relaxed);
                static_cast<void>(dmx_usb_pro[universe].open(port));
            }
            const bool opened = dmx_usb_pro[universe].is_open();
            usb_ready = usb_ready && opened;
            dmx_usb_pro_state_[universe].store(
                opened ? AdapterState::Ready : AdapterState::Fault,
                std::memory_order_relaxed);
        }
        return artnet_ready && sacn_ready && usb_ready;
    };

    bool outputs_ready = open_missing_outputs();
    auto next_retry = SteadyClock::now() + std::chrono::seconds(2);
    OutputFrame frame;
    while (!stop_requested_.load(std::memory_order_acquire) || !output_queue_.empty()) {
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

        bool artnet_success = true;
        bool sacn_success = true;
        bool usb_success = true;
        for (std::uint16_t universe = 0; universe < showcore::kV1UniverseCount; ++universe) {
            if (connections_.artnet_enabled && artnet.is_open()) {
                const auto packet = showcore::build_artdmx(
                    frame.frames.universes[universe],
                    static_cast<std::uint16_t>(connections_.artnet_base + universe),
                    frame.sequence);
                artnet_success = artnet.send(packet) && artnet_success;
            }
            if (connections_.sacn_enabled && sacn[universe].is_open()) {
                const auto packet = showcore::build_sacn_data_packet(
                    frame.frames.universes[universe],
                    static_cast<std::uint16_t>(connections_.sacn_universe_base + universe),
                    frame.sequence,
                    cid,
                    project_name_);
                sacn_success = sacn[universe].send(packet) && sacn_success;
            }
            if (!connections_.dmx_usb_pro_ports[universe].empty() &&
                dmx_usb_pro[universe].is_open() &&
                !dmx_usb_pro[universe].send(frame.frames.universes[universe])) {
                usb_success = false;
                dmx_usb_pro[universe].close();
                dmx_usb_pro_state_[universe].store(
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
                artnet_state_.store(AdapterState::Fault, std::memory_order_relaxed);
            }
            if (!sacn_success && connections_.sacn_enabled) {
                for (auto& sender : sacn) {
                    sender.close();
                }
                sacn_state_.store(AdapterState::Fault, std::memory_order_relaxed);
            }
        }
        output_frames_.fetch_add(1, std::memory_order_relaxed);
    }

    showcore::DmxFrames zero_frames;
    zero_frames.clear();
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
                    project_name_)));
            }
            if (!connections_.dmx_usb_pro_ports[universe].empty() &&
                dmx_usb_pro[universe].is_open()) {
                static_cast<void>(dmx_usb_pro[universe].send(
                    zero_frames.universes[universe]));
            }
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
}

}  // namespace emberlights
