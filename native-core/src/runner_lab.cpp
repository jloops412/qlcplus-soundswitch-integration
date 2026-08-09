#include "showcore/artnet.hpp"
#include "showcore/autoloop.hpp"
#include "showcore/engine.hpp"
#include "showcore/fixture.hpp"
#include "showcore/os2l_server.hpp"
#include "showcore/spsc_queue.hpp"
#include "showcore/sync_manager.hpp"
#include "showcore/types.hpp"

#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string_view>
#include <thread>

namespace {

using SteadyClock = std::chrono::steady_clock;

volatile std::sig_atomic_t g_interrupted = 0;

void handle_signal(int) noexcept {
    g_interrupted = 1;
}

[[nodiscard]] std::uint64_t monotonic_ms() noexcept {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            SteadyClock::now().time_since_epoch()).count());
}

constexpr std::array<showcore::ChannelMapping, 5> kRgbDimmerChannels{{
    {showcore::Property::Intensity, 0, -1, showcore::ChannelEncoding::Linear8, 0, 255},
    {showcore::Property::Red, 1, -1, showcore::ChannelEncoding::Linear8, 0, 255},
    {showcore::Property::Green, 2, -1, showcore::ChannelEncoding::Linear8, 0, 255},
    {showcore::Property::Blue, 3, -1, showcore::ChannelEncoding::Linear8, 0, 255},
    {showcore::Property::Strobe, 4, -1, showcore::ChannelEncoding::Linear8, 0, 255}
}};

constexpr showcore::FixtureProfile kRgbDimmer{
    "Runner Lab RGB Dimmer",
    kRgbDimmerChannels.data(),
    kRgbDimmerChannels.size(),
    5};

constexpr std::array<showcore::LookAssignment, 8> kRedAssignments{{
    {0, showcore::Property::Intensity, showcore::PropertyValue::set(0.65F)},
    {0, showcore::Property::Red, showcore::PropertyValue::set(1.0F)},
    {0, showcore::Property::Green, showcore::PropertyValue::set(0.0F)},
    {0, showcore::Property::Blue, showcore::PropertyValue::set(0.0F)},
    {1, showcore::Property::Intensity, showcore::PropertyValue::set(0.65F)},
    {1, showcore::Property::Red, showcore::PropertyValue::set(1.0F)},
    {1, showcore::Property::Green, showcore::PropertyValue::set(0.0F)},
    {1, showcore::Property::Blue, showcore::PropertyValue::set(0.0F)}
}};

constexpr std::array<showcore::LookAssignment, 8> kBlueAssignments{{
    {0, showcore::Property::Intensity, showcore::PropertyValue::set(1.0F)},
    {0, showcore::Property::Red, showcore::PropertyValue::set(0.0F)},
    {0, showcore::Property::Green, showcore::PropertyValue::set(0.0F)},
    {0, showcore::Property::Blue, showcore::PropertyValue::set(1.0F)},
    {1, showcore::Property::Intensity, showcore::PropertyValue::set(1.0F)},
    {1, showcore::Property::Red, showcore::PropertyValue::set(0.0F)},
    {1, showcore::Property::Green, showcore::PropertyValue::set(0.0F)},
    {1, showcore::Property::Blue, showcore::PropertyValue::set(1.0F)}
}};

constexpr showcore::StaticLook kRedLook{
    "Runner Lab Red",
    kRedAssignments.data(),
    kRedAssignments.size()};

constexpr showcore::StaticLook kBlueLook{
    "Runner Lab Blue",
    kBlueAssignments.data(),
    kBlueAssignments.size()};

struct LabConfig {
    std::string_view os2l_bind{"127.0.0.1"};
    std::uint16_t os2l_port{9996};
    std::string_view artnet_destination{};
    std::uint16_t artnet_base{0};
    std::uint16_t fps{40};
    std::uint32_t duration_seconds{0};
    double manual_bpm{0.0};
};

enum class OutputState : std::uint8_t {
    Starting,
    Ready,
    Failed,
    Stopped
};

struct LabBeatEvent {
    std::int64_t position{0};
    double bpm{0.0};
    std::uint64_t timestamp_ms{0};
};

using BeatQueue = showcore::SpscQueue<LabBeatEvent, 257>;

struct SharedState {
    BeatQueue beat_queue{};
    std::atomic<bool> stop{false};
    std::atomic<bool> blackout{false};
    std::atomic<std::uint64_t> dropped_beats{0};
    std::atomic<std::uint64_t> blackout_events{0};
    std::atomic<std::uint64_t> ignored_events{0};
    std::atomic<std::uint64_t> frames{0};
    std::atomic<std::uint64_t> send_failures{0};
    std::atomic<std::uint64_t> max_jitter_us{0};
    std::atomic<std::uint32_t> bpm_milli{0};
    std::atomic<std::int64_t> beat_milli{0};
    std::atomic<showcore::SyncState> sync_state{showcore::SyncState::Waiting};
    std::atomic<showcore::ClockSource> clock_source{showcore::ClockSource::None};
    std::atomic<OutputState> output_state{OutputState::Starting};
};

[[nodiscard]] bool parse_unsigned(std::string_view text, std::uint32_t& value) noexcept {
    value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

[[nodiscard]] bool parse_double(std::string_view text, double& value) noexcept {
    value = 0.0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

void print_usage() {
    std::cout
        << "Usage: runner_lab [options]\n"
        << "  --os2l-bind IPv4      Bind address (default 127.0.0.1)\n"
        << "  --os2l-port PORT      TCP port; 0 selects a test port (default 9996)\n"
        << "  --artnet IPv4         Send both universes by Art-Net; omit for dry-run\n"
        << "  --artnet-base PORT    First Art-Net port-address (default 0)\n"
        << "  --fps HZ              Render rate from 10 through 60 (default 40)\n"
        << "  --manual-bpm BPM      Manual fallback from 20 through 300\n"
        << "  --duration SECONDS    Stop automatically; 0 runs until Ctrl+C\n"
        << "  --help                Show this help\n";
}

[[nodiscard]] bool parse_arguments(int argc, char** argv, LabConfig& config, bool& help) {
    help = false;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--help") {
            help = true;
            return true;
        }
        if (index + 1 >= argc) {
            return false;
        }
        const std::string_view value = argv[++index];
        if (argument == "--os2l-bind") {
            config.os2l_bind = value;
        } else if (argument == "--artnet") {
            config.artnet_destination = value;
        } else if (argument == "--os2l-port") {
            std::uint32_t parsed = 0;
            if (!parse_unsigned(value, parsed) || parsed > 65535U) {
                return false;
            }
            config.os2l_port = static_cast<std::uint16_t>(parsed);
        } else if (argument == "--artnet-base") {
            std::uint32_t parsed = 0;
            if (!parse_unsigned(value, parsed) || parsed > 32766U) {
                return false;
            }
            config.artnet_base = static_cast<std::uint16_t>(parsed);
        } else if (argument == "--fps") {
            std::uint32_t parsed = 0;
            if (!parse_unsigned(value, parsed) || parsed < 10U || parsed > 60U) {
                return false;
            }
            config.fps = static_cast<std::uint16_t>(parsed);
        } else if (argument == "--duration") {
            if (!parse_unsigned(value, config.duration_seconds) ||
                config.duration_seconds > 86400U) {
                return false;
            }
        } else if (argument == "--manual-bpm") {
            if (!parse_double(value, config.manual_bpm) ||
                config.manual_bpm < 20.0 || config.manual_bpm > 300.0) {
                return false;
            }
        } else {
            return false;
        }
    }
    return true;
}

void capture_os2l(
    const showcore::Os2lEvent& event,
    showcore::Os2lParseError error,
    std::string_view,
    void* context) noexcept {
    auto& shared = *static_cast<SharedState*>(context);
    if (error != showcore::Os2lParseError::None) {
        return;
    }
    if (event.kind == showcore::Os2lKind::Beat) {
        const LabBeatEvent beat{event.beat.position, event.beat.bpm, monotonic_ms()};
        if (!shared.beat_queue.try_push(beat)) {
            shared.dropped_beats.fetch_add(1, std::memory_order_relaxed);
        }
        return;
    }
    if (event.kind == showcore::Os2lKind::Button &&
        event.button.name.view() == "blackout") {
        shared.blackout.store(event.button.on, std::memory_order_release);
        shared.blackout_events.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    shared.ignored_events.fetch_add(1, std::memory_order_relaxed);
}

[[nodiscard]] const char* sync_name(showcore::SyncState state) noexcept {
    switch (state) {
    case showcore::SyncState::Waiting: return "waiting";
    case showcore::SyncState::Os2lHealthy: return "os2l";
    case showcore::SyncState::PredictiveHold: return "hold";
    case showcore::SyncState::AudioFallback: return "audio";
    case showcore::SyncState::Recovering: return "recovering";
    case showcore::SyncState::Manual: return "manual";
    case showcore::SyncState::SafeUnsynchronized: return "safe";
    }
    return "unknown";
}

void update_max_jitter(std::atomic<std::uint64_t>& maximum, std::uint64_t candidate) noexcept {
    auto observed = maximum.load(std::memory_order_relaxed);
    while (candidate > observed && !maximum.compare_exchange_weak(
               observed,
               candidate,
               std::memory_order_relaxed,
               std::memory_order_relaxed)) {
    }
}

void run_scheduler(const LabConfig& config, SharedState& shared) noexcept {
    auto engine = std::make_unique<showcore::Engine>();
    if (!engine->patch().add({0, 0, 1, &kRgbDimmer}) ||
        !engine->patch().add({1, 1, 1, &kRgbDimmer})) {
        shared.output_state.store(OutputState::Failed, std::memory_order_release);
        return;
    }

    showcore::AutoloopPattern pattern;
    pattern.name = "Runner Lab Red/Blue";
    pattern.length_beats = 4.0F;
    if (!pattern.add_step({0.0F, &kRedLook, showcore::AutoloopTransition::Linear}) ||
        !pattern.add_step({2.0F, &kBlueLook, showcore::AutoloopTransition::Linear})) {
        shared.output_state.store(OutputState::Failed, std::memory_order_release);
        return;
    }

    showcore::AutoloopCatalog catalog;
    if (!catalog.set({0, 0}, &pattern)) {
        shared.output_state.store(OutputState::Failed, std::memory_order_release);
        return;
    }
    showcore::AutoloopPlayer player;
    if (!player.trigger(
            catalog,
            {0, 0},
            showcore::AutoloopRepeat::Infinite,
            0.0,
            true,
            engine->layers())) {
        shared.output_state.store(OutputState::Failed, std::memory_order_release);
        return;
    }

    showcore::ArtNetSender sender;
    const bool sends_artnet = !config.artnet_destination.empty();
    if (sends_artnet && !sender.open_ipv4(config.artnet_destination)) {
        shared.output_state.store(OutputState::Failed, std::memory_order_release);
        return;
    }

    showcore::SyncManager sync;
    if (config.manual_bpm > 0.0) {
        sync.set_manual_bpm(config.manual_bpm, monotonic_ms());
    }

    shared.output_state.store(OutputState::Ready, std::memory_order_release);
    const auto frame_period = std::chrono::microseconds(1'000'000 / config.fps);
    auto next_frame = SteadyClock::now();
    bool applied_blackout = false;
    std::uint8_t sequence = 1;

    while (!shared.stop.load(std::memory_order_acquire)) {
        std::this_thread::sleep_until(next_frame);
        const auto now = SteadyClock::now();
        if (now > next_frame) {
            const auto jitter = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(now - next_frame).count());
            update_max_jitter(shared.max_jitter_us, jitter);
        }
        const auto now_ms = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count());

        LabBeatEvent beat{};
        while (shared.beat_queue.try_pop(beat)) {
            sync.on_os2l_beat(beat.position, beat.bpm, beat.timestamp_ms);
        }

        const bool requested_blackout = shared.blackout.load(std::memory_order_acquire);
        if (requested_blackout != applied_blackout) {
            if (requested_blackout) {
                engine->layers().set(
                    showcore::LayerId::Emergency,
                    0,
                    showcore::Property::Intensity,
                    showcore::PropertyValue::force_zero());
                engine->layers().set(
                    showcore::LayerId::Emergency,
                    1,
                    showcore::Property::Intensity,
                    showcore::PropertyValue::force_zero());
            } else {
                engine->layers().clear_layer(showcore::LayerId::Emergency);
            }
            applied_blackout = requested_blackout;
        }

        const auto clock = sync.tick(now_ms);
        const auto beat_position = clock.source == showcore::ClockSource::None
            ? 0.0
            : clock.beat_position;
        player.tick(beat_position, true, engine->layers());
        engine->tick();

        if (sends_artnet) {
            for (std::uint16_t universe = 0; universe < showcore::kV1UniverseCount; ++universe) {
                const auto packet = showcore::build_artdmx(
                    engine->frames().universes[universe],
                    static_cast<std::uint16_t>(config.artnet_base + universe),
                    sequence);
                if (!sender.send(packet)) {
                    shared.send_failures.fetch_add(1, std::memory_order_relaxed);
                }
            }
            ++sequence;
            if (sequence == 0U) {
                sequence = 1U;
            }
        }

        shared.frames.fetch_add(1, std::memory_order_relaxed);
        shared.sync_state.store(clock.state, std::memory_order_relaxed);
        shared.clock_source.store(clock.source, std::memory_order_relaxed);
        shared.bpm_milli.store(
            static_cast<std::uint32_t>(clock.bpm > 0.0 ? clock.bpm * 1000.0 : 0.0),
            std::memory_order_relaxed);
        shared.beat_milli.store(
            static_cast<std::int64_t>(clock.beat_position * 1000.0),
            std::memory_order_relaxed);

        next_frame += frame_period;
        if (now > next_frame + frame_period * 4) {
            next_frame = now + frame_period;
        }
    }

    shared.output_state.store(OutputState::Stopped, std::memory_order_release);
}

}  // namespace

int main(int argc, char** argv) {
    LabConfig config;
    bool help = false;
    if (!parse_arguments(argc, argv, config, help)) {
        print_usage();
        return EXIT_FAILURE;
    }
    if (help) {
        print_usage();
        return EXIT_SUCCESS;
    }

    showcore::Os2lTcpServer server;
    if (!server.open_ipv4(config.os2l_bind, config.os2l_port)) {
        std::cerr << "Unable to open OS2L listener on " << config.os2l_bind << ':'
                  << config.os2l_port << " error=" << server.last_error() << '\n';
        return EXIT_FAILURE;
    }

    SharedState shared;
    std::signal(SIGINT, &handle_signal);
    std::signal(SIGTERM, &handle_signal);
    std::thread scheduler(&run_scheduler, std::cref(config), std::ref(shared));

    std::cout << "runner_lab started os2l=" << config.os2l_bind << ':' << server.bound_port()
              << " output=";
    if (config.artnet_destination.empty()) {
        std::cout << "dry-run";
    } else {
        std::cout << "artnet:" << config.artnet_destination
                  << " ports=" << config.artnet_base << ',' << config.artnet_base + 1U;
    }
    std::cout << " fps=" << config.fps << '\n';
    if (config.os2l_bind != "127.0.0.1") {
        std::cout << "warning: OS2L is exposed beyond loopback; use only a trusted event LAN\n";
    }

    const auto started = SteadyClock::now();
    auto next_status = started;
    bool server_fault = false;
    while (g_interrupted == 0 && !shared.stop.load(std::memory_order_acquire)) {
        const auto result = server.poll(&capture_os2l, &shared, 50);
        if (result == showcore::Os2lPollResult::Error) {
            std::cerr << "OS2L server fault error=" << server.last_error() << '\n';
            server_fault = true;
            break;
        }
        if (shared.output_state.load(std::memory_order_acquire) == OutputState::Failed) {
            std::cerr << "Scheduler/output initialization failed\n";
            break;
        }

        const auto now = SteadyClock::now();
        if (config.duration_seconds > 0U &&
            now - started >= std::chrono::seconds(config.duration_seconds)) {
            break;
        }
        if (now >= next_status) {
            std::cout << std::fixed << std::setprecision(2)
                      << "status frames=" << shared.frames.load(std::memory_order_relaxed)
                      << " sync=" << sync_name(shared.sync_state.load(std::memory_order_relaxed))
                      << " bpm=" << shared.bpm_milli.load(std::memory_order_relaxed) / 1000.0
                      << " beat=" << shared.beat_milli.load(std::memory_order_relaxed) / 1000.0
                      << " os2l_connections=" << server.stats().connections
                      << " dropped_beats=" << shared.dropped_beats.load(std::memory_order_relaxed)
                      << " send_failures=" << shared.send_failures.load(std::memory_order_relaxed)
                      << " max_jitter_us=" << shared.max_jitter_us.load(std::memory_order_relaxed)
                      << '\n';
            next_status = now + std::chrono::seconds(1);
        }
    }

    shared.stop.store(true, std::memory_order_release);
    scheduler.join();
    server.close();
    const auto frames = shared.frames.load(std::memory_order_relaxed);
    const bool output_failed = shared.output_state.load(std::memory_order_acquire) ==
        OutputState::Failed;
    std::cout << "runner_lab stopped frames=" << frames
              << " os2l_messages=" << server.stats().messages
              << " decode_errors=" << server.stats().decode_errors
              << " dropped_beats=" << shared.dropped_beats.load(std::memory_order_relaxed)
              << " send_failures=" << shared.send_failures.load(std::memory_order_relaxed)
              << '\n';
    return server_fault || output_failed || frames == 0U ? EXIT_FAILURE : EXIT_SUCCESS;
}
