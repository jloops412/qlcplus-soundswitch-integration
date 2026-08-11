#include "emberlights/compiler.hpp"
#include "emberlights/project.hpp"
#include "emberlights/runner.hpp"
#include "emberlights/version.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <sys/resource.h>
#endif

namespace {

using SteadyClock = std::chrono::steady_clock;

constexpr std::size_t kQualificationFixtureCount = 128U;
constexpr std::uint16_t kQualificationFrameRate = 40U;

struct Options {
    std::uint64_t duration_seconds{60U};
    bool strict{false};
    bool network_loopback{false};
    bool help{false};
    std::filesystem::path report_path;
};

struct CheckResult {
    std::string id;
    bool passed{false};
    std::string observed;
    std::string requirement;
};

[[nodiscard]] std::uint64_t unix_ms() noexcept {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
}

[[nodiscard]] std::uint64_t process_cpu_us() noexcept {
#ifdef _WIN32
    FILETIME created{};
    FILETIME exited{};
    FILETIME kernel{};
    FILETIME user{};
    if (::GetProcessTimes(::GetCurrentProcess(), &created, &exited, &kernel, &user) == FALSE) {
        return 0U;
    }
    ULARGE_INTEGER kernel_time{};
    kernel_time.LowPart = kernel.dwLowDateTime;
    kernel_time.HighPart = kernel.dwHighDateTime;
    ULARGE_INTEGER user_time{};
    user_time.LowPart = user.dwLowDateTime;
    user_time.HighPart = user.dwHighDateTime;
    return (kernel_time.QuadPart + user_time.QuadPart) / 10U;
#elif defined(__linux__) || defined(__APPLE__)
    rusage usage{};
    if (::getrusage(RUSAGE_SELF, &usage) != 0) {
        return 0U;
    }
    const auto user = static_cast<std::uint64_t>(usage.ru_utime.tv_sec) * 1'000'000U +
        static_cast<std::uint64_t>(usage.ru_utime.tv_usec);
    const auto system = static_cast<std::uint64_t>(usage.ru_stime.tv_sec) * 1'000'000U +
        static_cast<std::uint64_t>(usage.ru_stime.tv_usec);
    return user + system;
#else
    return 0U;
#endif
}

[[nodiscard]] std::uint64_t peak_rss_bytes() noexcept {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters{};
    counters.cb = static_cast<DWORD>(sizeof(counters));
    if (::GetProcessMemoryInfo(
            ::GetCurrentProcess(), &counters,
            static_cast<DWORD>(sizeof(counters))) == FALSE) {
        return 0U;
    }
    return static_cast<std::uint64_t>(counters.PeakWorkingSetSize);
#elif defined(__APPLE__)
    rusage usage{};
    return ::getrusage(RUSAGE_SELF, &usage) == 0
        ? static_cast<std::uint64_t>(usage.ru_maxrss)
        : 0U;
#elif defined(__linux__)
    rusage usage{};
    return ::getrusage(RUSAGE_SELF, &usage) == 0
        ? static_cast<std::uint64_t>(usage.ru_maxrss) * 1024U
        : 0U;
#else
    return 0U;
#endif
}

[[nodiscard]] bool parse_u64(std::string_view text, std::uint64_t& value) noexcept {
    if (text.empty()) {
        return false;
    }
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

[[nodiscard]] std::filesystem::path default_report_path() {
    std::filesystem::path directory;
#ifdef _WIN32
    std::array<wchar_t, 32768> local_app_data{};
    const auto length = ::GetEnvironmentVariableW(
        L"LOCALAPPDATA", local_app_data.data(),
        static_cast<DWORD>(local_app_data.size()));
    if (length > 0U && length < static_cast<DWORD>(local_app_data.size())) {
        directory = std::filesystem::path(local_app_data.data()) /
            "EmberLights" / "Qualification";
    }
#endif
    if (directory.empty()) {
        directory = std::filesystem::current_path();
    }
    return directory / ("EmberLights-qualification-" + std::to_string(unix_ms()) + ".json");
}

void print_usage() {
    std::cout
        << "EmberLights qualification tool\n\n"
        << "Usage: emberlights_qualify [--duration SECONDS] [--strict] "
           "[--network-loopback] [--report PATH]\n\n"
        << "  --duration  Run time from 1 through 86400 seconds (default 60).\n"
        << "  --strict    Apply production timing ceilings; use with an eight-hour run.\n"
        << "  --network-loopback  Exercise Art-Net/sACN only on an isolated machine.\n"
        << "  --report    Write the machine-readable JSON report to PATH.\n";
}

[[nodiscard]] bool parse_options(int argc, char** argv, Options& options) {
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument == "--help" || argument == "-h") {
            options.help = true;
            continue;
        }
        if (argument == "--strict") {
            options.strict = true;
            continue;
        }
        if (argument == "--network-loopback") {
            options.network_loopback = true;
            continue;
        }
        if (argument == "--duration") {
            if (++index >= argc ||
                !parse_u64(argv[index], options.duration_seconds) ||
                options.duration_seconds < 1U || options.duration_seconds > 86'400U) {
                std::cerr << "--duration must be a whole number from 1 through 86400.\n";
                return false;
            }
            continue;
        }
        if (argument == "--report") {
            if (++index >= argc || std::string_view(argv[index]).empty()) {
                std::cerr << "--report requires a path.\n";
                return false;
            }
            options.report_path = argv[index];
            continue;
        }
        std::cerr << "Unknown option: " << argument << '\n';
        return false;
    }
    if (options.report_path.empty()) {
        options.report_path = default_report_path();
    }
    return true;
}

[[nodiscard]] std::string fixture_id(std::size_t index) {
    std::ostringstream text;
    text << "qualification-fixture-" << std::setw(3) << std::setfill('0') << index;
    return text.str();
}

[[nodiscard]] emberlights::ProjectDocument make_qualification_project(bool network_loopback) {
    auto project = emberlights::make_starter_project();
    project.id = "emberlights-production-qualification";
    project.name = "EmberLights Production Qualification";
    project.connections.os2l_enabled = true;
    project.connections.os2l_bind = "127.0.0.1";
    project.connections.os2l_port = 0U;
    project.connections.artnet_enabled = network_loopback;
    project.connections.artnet_destination = "127.0.0.1";
    project.connections.sacn_enabled = network_loopback;
    project.connections.sacn_destination = "127.0.0.1";
    project.connections.dmx_usb_pro_ports = {};
    project.connections.midi_input_index = -1;
    project.connections.midi_output_index = -1;
    project.connections.frame_rate = kQualificationFrameRate;
    project.connections.manual_bpm = 124.0;

    project.fixtures.reserve(kQualificationFixtureCount);
    emberlights::LookDefinition warm;
    warm.id = "qualification-warm";
    warm.name = "Qualification Warm";
    warm.fade_ms = 250U;
    warm.assignments.reserve(kQualificationFixtureCount * 4U);
    emberlights::LookDefinition cool;
    cool.id = "qualification-cool";
    cool.name = "Qualification Cool";
    cool.fade_ms = 250U;
    cool.assignments.reserve(kQualificationFixtureCount * 4U);

    for (std::size_t index = 0; index < kQualificationFixtureCount; ++index) {
        const auto id = fixture_id(index);
        const auto universe_index = index / 64U;
        const auto address_index = index % 64U;
        project.fixtures.push_back({
            id,
            "Qualification Fixture " + std::to_string(index + 1U),
            "builtin.generic.rgbd-4ch",
            static_cast<std::uint8_t>(universe_index + 1U),
            static_cast<std::uint16_t>(address_index * 4U + 1U),
            {"qualification"}});
        for (const auto& assignment : {
                 emberlights::LookAssignmentDefinition{
                     id, showcore::Property::Intensity, showcore::PropertyValue::set(0.70F)},
                 emberlights::LookAssignmentDefinition{
                     id, showcore::Property::Red, showcore::PropertyValue::set(1.0F)},
                 emberlights::LookAssignmentDefinition{
                     id, showcore::Property::Green, showcore::PropertyValue::set(0.30F)},
                 emberlights::LookAssignmentDefinition{
                     id, showcore::Property::Blue, showcore::PropertyValue::set(0.05F)}}) {
            warm.assignments.push_back(assignment);
        }
        for (const auto& assignment : {
                 emberlights::LookAssignmentDefinition{
                     id, showcore::Property::Intensity, showcore::PropertyValue::set(0.80F)},
                 emberlights::LookAssignmentDefinition{
                     id, showcore::Property::Red, showcore::PropertyValue::set(0.05F)},
                 emberlights::LookAssignmentDefinition{
                     id, showcore::Property::Green, showcore::PropertyValue::set(0.25F)},
                 emberlights::LookAssignmentDefinition{
                     id, showcore::Property::Blue, showcore::PropertyValue::set(1.0F)}}) {
            cool.assignments.push_back(assignment);
        }
    }
    project.groups.push_back({"qualification", "Qualification Rig", {}});
    project.groups.back().fixture_ids.reserve(project.fixtures.size());
    for (const auto& fixture : project.fixtures) {
        project.groups.back().fixture_ids.push_back(fixture.id);
    }
    project.looks.push_back(std::move(warm));
    project.looks.push_back(std::move(cool));
    project.autoloops.push_back({
        "qualification-loop",
        "Qualification Warm / Cool",
        0U,
        0U,
        4.0F,
        showcore::AutoloopRepeat::Infinite,
        {{0.0F, "qualification-warm", showcore::AutoloopTransition::Linear},
         {2.0F, "qualification-cool", showcore::AutoloopTransition::Linear}}});
    return project;
}

[[nodiscard]] std::string json_escape(std::string_view input) {
    std::ostringstream output;
    for (const unsigned char value : input) {
        switch (value) {
        case '\\': output << "\\\\"; break;
        case '"': output << "\\\""; break;
        case '\b': output << "\\b"; break;
        case '\f': output << "\\f"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (value < 0x20U) {
                output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                       << static_cast<unsigned int>(value) << std::dec;
            } else {
                output << static_cast<char>(value);
            }
            break;
        }
    }
    return output.str();
}

[[nodiscard]] std::string platform_name() noexcept {
#ifdef _WIN32
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#elif defined(__linux__)
    return "linux";
#else
    return "unknown";
#endif
}

[[nodiscard]] std::string adapter_name(emberlights::AdapterState state) {
    switch (state) {
    case emberlights::AdapterState::Disabled: return "disabled";
    case emberlights::AdapterState::Starting: return "starting";
    case emberlights::AdapterState::Waiting: return "waiting";
    case emberlights::AdapterState::Ready: return "ready";
    case emberlights::AdapterState::Fault: return "fault";
    }
    return "unknown";
}

[[nodiscard]] bool write_report(
    const Options& options,
    std::uint64_t started_unix_ms,
    std::uint64_t observed_duration_ms,
    std::uint64_t compile_ms,
    std::uint64_t runner_start_ms,
    std::uint64_t cpu_used_us,
    double one_core_percent,
    std::uint64_t peak_rss,
    const emberlights::RunnerStatus& status,
    std::uint64_t command_posts,
    std::uint64_t command_rejections,
    std::uint64_t maximum_progress_gap_ms,
    const std::vector<CheckResult>& checks,
    bool passed) {
    std::error_code error;
    const auto parent = options.report_path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);
        if (error) {
            std::cerr << "Could not create report directory: " << error.message() << '\n';
            return false;
        }
    }
    std::ofstream output(options.report_path, std::ios::binary | std::ios::trunc);
    if (!output) {
        std::cerr << "Could not open qualification report: "
                  << options.report_path.string() << '\n';
        return false;
    }
    output << "{\n"
           << "  \"format\": \"emberlights-qualification-report\",\n"
           << "  \"formatVersion\": 1,\n"
           << "  \"productVersion\": \"" << json_escape(emberlights::kVersion) << "\",\n"
           << "  \"commit\": \"" << json_escape(emberlights::kCommit) << "\",\n"
           << "  \"profile\": \"" << (options.strict ? "production" : "smoke") << "\",\n"
           << "  \"networkLoopback\": "
           << (options.network_loopback ? "true" : "false") << ",\n"
           << "  \"platform\": \"" << platform_name() << "\",\n"
           << "  \"hardwareThreads\": " << std::thread::hardware_concurrency() << ",\n"
           << "  \"startedUnixMs\": " << started_unix_ms << ",\n"
           << "  \"requestedDurationMs\": " << options.duration_seconds * 1000U << ",\n"
           << "  \"observedDurationMs\": " << observed_duration_ms << ",\n"
           << "  \"fixtureCount\": " << kQualificationFixtureCount << ",\n"
           << "  \"universeCount\": " << showcore::kV1UniverseCount << ",\n"
           << "  \"frameRate\": " << kQualificationFrameRate << ",\n"
           << "  \"process\": {\n"
           << "    \"compileMs\": " << compile_ms << ",\n"
           << "    \"runnerStartMs\": " << runner_start_ms << ",\n"
           << "    \"cpuUsedUs\": " << cpu_used_us << ",\n"
           << "    \"oneCorePercent\": " << std::fixed << std::setprecision(3)
           << one_core_percent << ",\n"
           << "    \"peakRssBytes\": " << peak_rss << "\n"
           << "  },\n"
           << "  \"runner\": {\n"
           << "    \"os2lState\": \"" << adapter_name(status.os2l) << "\",\n"
           << "    \"artNetState\": \"" << adapter_name(status.artnet) << "\",\n"
           << "    \"sacnState\": \"" << adapter_name(status.sacn) << "\",\n"
           << "    \"frames\": " << status.frames << ",\n"
           << "    \"outputFrames\": " << status.output_frames << ",\n"
           << "    \"outputQueueDrops\": " << status.output_queue_drops << ",\n"
           << "    \"outputSupersededFrames\": " << status.output_superseded_frames << ",\n"
           << "    \"outputSendFailures\": " << status.output_send_failures << ",\n"
           << "    \"jitterSamples\": " << status.jitter_samples << ",\n"
           << "    \"jitterP99Us\": " << status.jitter_p99_us << ",\n"
           << "    \"maxJitterUs\": " << status.max_jitter_us << ",\n"
           << "    \"deadlineMisses\": " << status.deadline_misses << ",\n"
           << "    \"schedulerResyncs\": " << status.scheduler_resyncs << ",\n"
           << "    \"lastFrameAgeMs\": " << status.last_frame_age_ms << ",\n"
           << "    \"maximumProgressGapMs\": " << maximum_progress_gap_ms << ",\n"
           << "    \"commandPosts\": " << command_posts << ",\n"
           << "    \"commandRejections\": " << command_rejections << "\n"
           << "  },\n"
           << "  \"outputs\": [\n";
    for (std::size_t index = 0U; index < status.output_backends.size(); ++index) {
        const auto& backend = status.output_backends[index];
        const auto& descriptor = showcore::output_backend_descriptor(backend.kind);
        output << "    {\"kind\": \"" << json_escape(descriptor.name)
               << "\", \"state\": \""
               << showcore::output_health_state_name(backend.state)
               << "\", \"configured\": "
               << (backend.configured ? "true" : "false")
               << ", \"firstSourceUniverse\": "
               << static_cast<unsigned int>(backend.first_source_universe)
               << ", \"sourceUniverseCount\": "
               << static_cast<unsigned int>(backend.source_universe_count)
               << ", \"openAttempts\": " << backend.open_attempts
               << ", \"openSuccesses\": " << backend.open_successes
               << ", \"reconnects\": " << backend.reconnects
               << ", \"framesAttempted\": " << backend.frames_attempted
               << ", \"framesAccepted\": " << backend.frames_accepted
               << ", \"framesFailed\": " << backend.frames_failed
               << ", \"lastError\": " << backend.last_error
               << ", \"lastNonzeroSlots\": " << backend.last_nonzero_slots
               << "}" << (index + 1U == status.output_backends.size() ? "\n" : ",\n");
    }
    output << "  ],\n"
           << "  \"checks\": [\n";
    for (std::size_t index = 0; index < checks.size(); ++index) {
        const auto& check = checks[index];
        output << "    {\"id\": \"" << json_escape(check.id)
               << "\", \"passed\": " << (check.passed ? "true" : "false")
               << ", \"observed\": \"" << json_escape(check.observed)
               << "\", \"requirement\": \"" << json_escape(check.requirement) << "\"}"
               << (index + 1U == checks.size() ? "\n" : ",\n");
    }
    output << "  ],\n"
           << "  \"verdict\": \"" << (passed ? "pass" : "fail") << "\"\n"
           << "}\n";
    return output.good();
}

[[nodiscard]] std::string number(std::uint64_t value) {
    return std::to_string(value);
}

[[nodiscard]] std::string decimal(double value) {
    std::ostringstream output;
    output << std::fixed << std::setprecision(3) << value;
    return output.str();
}

}  // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parse_options(argc, argv, options)) {
        return 2;
    }
    if (options.help) {
        print_usage();
        return 0;
    }

    auto project = make_qualification_project(options.network_loopback);
    const auto validation = emberlights::validate_project(project);
    if (!validation.ok()) {
        std::cerr << "The qualification project did not validate.\n";
        for (const auto& issue : validation.issues) {
            std::cerr << issue.code << ": " << issue.message << '\n';
        }
        return 3;
    }
    const auto compile_started = SteadyClock::now();
    auto compilation = emberlights::compile_project(project);
    const auto compile_ms = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            SteadyClock::now() - compile_started).count());
    if (!compilation) {
        std::cerr << "The qualification project did not compile.\n";
        return 3;
    }

    auto runner = std::make_unique<emberlights::RunnerService>();
    const auto runner_start_started = SteadyClock::now();
    if (!runner->start(std::move(compilation.show), project)) {
        std::cerr << "The qualification Runner did not start.\n";
        return 3;
    }
    const auto running_deadline = SteadyClock::now() + std::chrono::seconds(5);
    while (runner->status().state == emberlights::RunnerState::Starting &&
           SteadyClock::now() < running_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    const auto runner_start_ms = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            SteadyClock::now() - runner_start_started).count());

    const auto started_unix_ms = unix_ms();
    const auto cpu_started_us = process_cpu_us();
    const auto started = SteadyClock::now();
    const auto requested_end = started + std::chrono::seconds(options.duration_seconds);
    auto last_progress = started;
    auto previous_frames = runner->status().frames;
    std::uint64_t maximum_progress_gap_ms = 0U;
    std::uint64_t command_posts = 0U;
    std::uint64_t command_rejections = 0U;
    std::uint64_t previous_phase = static_cast<std::uint64_t>(-1);
    bool remained_running = runner->status().state == emberlights::RunnerState::Running;

    auto record_command = [&](bool accepted) {
        ++command_posts;
        if (!accepted) {
            ++command_rejections;
        }
    };

    while (SteadyClock::now() < requested_end) {
        const auto now = SteadyClock::now();
        const auto elapsed_ms = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now - started).count());
        const auto phase = elapsed_ms / 250U;
        if (phase != previous_phase) {
            previous_phase = phase;
            switch (phase % 8U) {
            case 0U: record_command(runner->trigger_look(0U)); break;
            case 1U: record_command(runner->trigger_autoloop({0U, 0U})); break;
            case 2U:
                record_command(runner->set_property(
                    0U, showcore::Property::Intensity,
                    static_cast<float>((phase % 10U) + 1U) / 10.0F));
                break;
            case 3U: runner->set_blackout(true); break;
            case 4U:
                runner->set_blackout(false);
                runner->set_work_light(true);
                break;
            case 5U:
                runner->set_work_light(false);
                record_command(runner->next_autoloop());
                break;
            case 6U: record_command(runner->set_manual_bpm(120.0 + (phase % 12U))); break;
            case 7U:
                record_command(runner->set_hazard_armed(showcore::Property::Fog, false));
                record_command(runner->clear_look());
                break;
            default: break;
            }
        }

        const auto status = runner->status();
        remained_running = remained_running && status.state == emberlights::RunnerState::Running;
        if (status.frames != previous_frames) {
            const auto gap_ms = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(now - last_progress).count());
            maximum_progress_gap_ms = std::max(maximum_progress_gap_ms, gap_ms);
            last_progress = now;
            previous_frames = status.frames;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    runner->set_blackout(false);
    runner->set_work_light(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const auto final_status = runner->status();
    const auto observed_duration_ms = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(SteadyClock::now() - started).count());
    const auto cpu_finished_us = process_cpu_us();
    const auto cpu_used_us = cpu_finished_us >= cpu_started_us
        ? cpu_finished_us - cpu_started_us
        : 0U;
    const auto one_core_percent = observed_duration_ms == 0U
        ? 0.0
        : static_cast<double>(cpu_used_us) /
            (static_cast<double>(observed_duration_ms) * 1000.0) * 100.0;
    const auto peak_rss = peak_rss_bytes();
    runner->stop();
    const auto stopped = runner->status().state == emberlights::RunnerState::Stopped;

    const auto minimum_frames = options.duration_seconds * kQualificationFrameRate * 9U / 10U;
    const auto jitter_ceiling_us = options.strict ? 5'000U : 100'000U;
    const auto progress_ceiling_ms = options.strict ? 250U : 1'000U;
    const auto cpu_ceiling_percent = options.strict ? 5.0 : 100.0;
    const auto rss_ceiling_bytes = options.strict ? 100U * 1024U * 1024U
                                                  : 512U * 1024U * 1024U;
    // A short hosted/package smoke confirms continuous delivery and records
    // scheduler health, but it is not timing qualification for a real DJ
    // machine. The strict profile is the evidence gate for the 5 ms limit.
    const auto deadline_ratio_passed = !options.strict || final_status.jitter_samples == 0U ||
        final_status.deadline_misses * 100U <= final_status.jitter_samples;

    std::vector<CheckResult> checks;
    checks.push_back({"project.validation", validation.ok(),
                      number(validation.error_count()), "0 validation errors"});
    checks.push_back({"runner.continuity", remained_running,
                      remained_running ? "running" : "state changed",
                      "Runner remains Running for the full test"});
    checks.push_back({"runner.stop", stopped, stopped ? "stopped" : "not stopped",
                      "clean stop reaches Stopped"});
    checks.push_back({"runner.startMs", runner_start_ms <= 4'000U,
                      number(runner_start_ms), "<= 4000"});
    checks.push_back({"process.oneCorePercent",
                      std::isfinite(one_core_percent) &&
                          one_core_percent <= cpu_ceiling_percent,
                      decimal(one_core_percent),
                      "<= " + decimal(cpu_ceiling_percent)});
    checks.push_back({"process.peakRssBytes",
                      peak_rss != 0U && peak_rss <= rss_ceiling_bytes,
                      number(peak_rss), "1 through " + number(rss_ceiling_bytes)});
    checks.push_back({"frames.minimum", final_status.frames >= minimum_frames,
                      number(final_status.frames), ">= " + number(minimum_frames)});
    checks.push_back({"output.queueDrops", final_status.output_queue_drops == 0U,
                      number(final_status.output_queue_drops), "0"});
    checks.push_back({"output.sendFailures", final_status.output_send_failures == 0U,
                      number(final_status.output_send_failures), "0"});
    checks.push_back({"adapter.os2l",
                      final_status.os2l == emberlights::AdapterState::Waiting ||
                          final_status.os2l == emberlights::AdapterState::Ready,
                      adapter_name(final_status.os2l), "waiting or ready"});
    checks.push_back({"adapter.artNet",
                      options.network_loopback
                          ? final_status.artnet == emberlights::AdapterState::Ready
                          : final_status.artnet == emberlights::AdapterState::Disabled,
                      adapter_name(final_status.artnet),
                      options.network_loopback ? "ready" : "disabled"});
    checks.push_back({"adapter.sacn",
                      options.network_loopback
                          ? final_status.sacn == emberlights::AdapterState::Ready
                          : final_status.sacn == emberlights::AdapterState::Disabled,
                      adapter_name(final_status.sacn),
                      options.network_loopback ? "ready" : "disabled"});
    checks.push_back({"commands.rejected", command_rejections == 0U,
                      number(command_rejections), "0"});
    checks.push_back({"scheduler.jitterP99Us",
                      final_status.jitter_p99_us <= jitter_ceiling_us,
                      number(final_status.jitter_p99_us),
                      "<= " + number(jitter_ceiling_us)});
    checks.push_back({"scheduler.deadlineMissRate", deadline_ratio_passed,
                      number(final_status.deadline_misses) + "/" +
                          number(final_status.jitter_samples),
                      options.strict ? "<= 1% over 5 ms"
                                     : "recorded; strict profile requires <= 1% over 5 ms"});
    checks.push_back({"scheduler.resyncs",
                      !options.strict || final_status.scheduler_resyncs == 0U,
                      number(final_status.scheduler_resyncs),
                      options.strict ? "0" : "recorded; strict profile requires 0"});
    checks.push_back({"scheduler.progressGapMs",
                      maximum_progress_gap_ms <= progress_ceiling_ms,
                      number(maximum_progress_gap_ms),
                      "<= " + number(progress_ceiling_ms)});
    checks.push_back({"scheduler.lastFrameAgeMs",
                      final_status.last_frame_age_ms <= progress_ceiling_ms,
                      number(final_status.last_frame_age_ms),
                      "<= " + number(progress_ceiling_ms)});

    const bool passed = std::all_of(
        checks.begin(), checks.end(), [](const auto& check) { return check.passed; });
    if (!write_report(
            options,
            started_unix_ms,
            observed_duration_ms,
            compile_ms,
            runner_start_ms,
            cpu_used_us,
            one_core_percent,
            peak_rss,
            final_status,
            command_posts,
            command_rejections,
            maximum_progress_gap_ms,
            checks,
            passed)) {
        return 3;
    }

    std::cout << "EmberLights qualification " << (passed ? "PASSED" : "FAILED") << '\n'
              << "Frames: " << final_status.frames
              << "  jitter p99: " << final_status.jitter_p99_us << " us"
              << "  max: " << final_status.max_jitter_us << " us\n"
              << "Report: " << options.report_path.string() << '\n';
    return passed ? 0 : 1;
}
