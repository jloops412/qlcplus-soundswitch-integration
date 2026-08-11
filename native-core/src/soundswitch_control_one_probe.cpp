#include "showcore/soundswitch_control_one.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {

struct Options {
    bool help{false};
    bool self_test{false};
    bool active_test{false};
    bool acknowledged{false};
    std::uint16_t channel_one{1U};
    std::uint16_t channel_two{1U};
    std::uint32_t hold_ms{3000U};
    std::string report_path{"control-one-dmx-qualification.json"};
};

void usage() {
    std::cout
        << "SoundSwitch Control One DMX qualification\n\n"
        << "Usage:\n"
        << "  soundswitch_control_one_probe --self-test\n"
        << "  soundswitch_control_one_probe --active-test "
           "--acknowledge-live-output [options]\n\n"
        << "Options:\n"
        << "  --channel-one N   DMX channel for output jack 1 (1-512)\n"
        << "  --channel-two N   DMX channel for output jack 2 (1-512)\n"
        << "  --hold-ms N       Hold each test level (500-15000 ms)\n"
        << "  --report PATH     Machine-readable report path\n\n"
        << "Close SoundSwitch first. Disconnect unsafe fixtures, fog, sparks, "
           "lasers, and motion loads. The active test drives one selected "
           "channel on each jack, then blackouts both jacks.\n";
}

[[nodiscard]] bool parse_unsigned(
    std::string_view text,
    std::uint32_t& value) {
    if (text.empty()) {
        return false;
    }
    std::uint64_t parsed = 0U;
    for (const char character : text) {
        if (character < '0' || character > '9') {
            return false;
        }
        parsed = parsed * 10U + static_cast<unsigned>(character - '0');
        if (parsed > 0xFFFFFFFFULL) {
            return false;
        }
    }
    value = static_cast<std::uint32_t>(parsed);
    return true;
}

[[nodiscard]] bool parse_options(int argc, char** argv, Options& options) {
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument == "--help" || argument == "-h") {
            options.help = true;
        } else if (argument == "--self-test") {
            options.self_test = true;
        } else if (argument == "--active-test") {
            options.active_test = true;
        } else if (argument == "--acknowledge-live-output") {
            options.acknowledged = true;
        } else if (argument == "--channel-one" ||
                   argument == "--channel-two" ||
                   argument == "--hold-ms" || argument == "--report") {
            if (index + 1 >= argc) {
                return false;
            }
            const auto value = std::string_view(argv[++index]);
            if (argument == "--report") {
                options.report_path = std::string(value);
                continue;
            }
            std::uint32_t parsed = 0U;
            if (!parse_unsigned(value, parsed)) {
                return false;
            }
            if (argument == "--channel-one") {
                options.channel_one = static_cast<std::uint16_t>(parsed);
            } else if (argument == "--channel-two") {
                options.channel_two = static_cast<std::uint16_t>(parsed);
            } else {
                options.hold_ms = parsed;
            }
        } else {
            return false;
        }
    }
    return options.channel_one >= 1U && options.channel_one <= 512U &&
        options.channel_two >= 1U && options.channel_two <= 512U &&
        options.hold_ms >= 500U && options.hold_ms <= 15'000U;
}

[[nodiscard]] bool protocol_self_test() {
    showcore::DmxUniverse first{};
    showcore::DmxUniverse second{};
    first.front() = 0x11U;
    first.back() = 0xFEU;
    second.front() = 0x22U;
    second.back() = 0xABU;
    const auto first_packet = showcore::build_soundswitch_control_one_packet(
        showcore::SoundSwitchControlOnePort::One, first);
    const auto second_packet = showcore::build_soundswitch_control_one_packet(
        showcore::SoundSwitchControlOnePort::Two, second);
    return first_packet.length == 522U && second_packet.length == 522U &&
        first_packet.bytes[0] == 's' && first_packet.bytes[3] == 't' &&
        first_packet.bytes[8] == 0U && second_packet.bytes[8] == 1U &&
        first_packet.bytes[10] == 0x11U && first_packet.bytes[521] == 0xFEU &&
        second_packet.bytes[10] == 0x22U &&
        second_packet.bytes[521] == 0xABU &&
        showcore::kSoundSwitchControlOneInitializationPackets[3][8] == 1U &&
        showcore::kSoundSwitchControlOneInitializationPackets[3][10] == 1U;
}

void write_report(
    const Options& options,
    bool self_test_passed,
    bool open_passed,
    bool jack_one_writes,
    bool jack_two_writes,
    bool blackout_writes,
    const showcore::SoundSwitchControlOneSessionStatus& status) {
    std::ofstream report(options.report_path, std::ios::binary | std::ios::trunc);
    if (!report) {
        std::cerr << "Could not write report: " << options.report_path << "\n";
        return;
    }
    report
        << "{\n"
        << "  \"schema\": \"emberlights.control-one-dmx-qualification.v1\",\n"
        << "  \"usb\": {\"vid\": \"15E4\", \"pid\": \"0054\", "
           "\"configuration\": 1, \"interface\": 0, "
           "\"bulk_out\": \"01\"},\n"
        << "  \"protocol_self_test\": "
        << (self_test_passed ? "true" : "false") << ",\n"
        << "  \"device_opened\": " << (open_passed ? "true" : "false")
        << ",\n"
        << "  \"jack_one_writes_accepted\": "
        << (jack_one_writes ? "true" : "false") << ",\n"
        << "  \"jack_two_writes_accepted\": "
        << (jack_two_writes ? "true" : "false") << ",\n"
        << "  \"blackout_writes_accepted\": "
        << (blackout_writes ? "true" : "false") << ",\n"
        << "  \"physical_output_verified\": false,\n"
        << "  \"physical_blackout_verified\": false,\n"
        << "  \"operator_note\": \"Host acceptance is not physical DMX "
           "qualification. Record visible/tester observations separately.\",\n"
        << "  \"test_channels\": [" << options.channel_one << ", "
        << options.channel_two << "],\n"
        << "  \"last_error\": " << status.last_error << ",\n"
        << "  \"frames_attempted\": " << status.frames_attempted << ",\n"
        << "  \"frames_accepted\": " << status.frames_accepted << ",\n"
        << "  \"frames_failed\": " << status.frames_failed << "\n"
        << "}\n";
}

}  // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parse_options(argc, argv, options)) {
        usage();
        return 2;
    }
    if (options.help || argc == 1) {
        usage();
        return 0;
    }
    const bool self_test_passed = protocol_self_test();
    if (options.self_test && !options.active_test) {
        std::cout << (self_test_passed
            ? "Control One protocol self-test passed\n"
            : "Control One protocol self-test FAILED\n");
        return self_test_passed ? 0 : 1;
    }
    if (!options.active_test || !options.acknowledged) {
        std::cerr << "Active output requires both --active-test and "
                     "--acknowledge-live-output.\n";
        return 2;
    }

    showcore::SoundSwitchControlOneSession session;
    const bool open_passed = session.open();
    bool jack_one_writes = false;
    bool jack_two_writes = false;
    bool blackout_writes = false;
    if (open_passed) {
        showcore::DmxUniverse jack_one{};
        showcore::DmxUniverse jack_two{};
        jack_one[options.channel_one - 1U] = 255U;
        jack_two[options.channel_two - 1U] = 255U;

        std::cout << "Driving jack 1, channel " << options.channel_one << "...\n";
        jack_one_writes = session.send(
            showcore::SoundSwitchControlOnePort::One, jack_one);
        std::this_thread::sleep_for(std::chrono::milliseconds(options.hold_ms));
        blackout_writes = session.send_blackout(3U);

        std::cout << "Driving jack 2, channel " << options.channel_two << "...\n";
        jack_two_writes = session.send(
            showcore::SoundSwitchControlOnePort::Two, jack_two);
        std::this_thread::sleep_for(std::chrono::milliseconds(options.hold_ms));
        blackout_writes = session.send_blackout(3U) && blackout_writes;
        std::cout << "Both outputs blacked out.\n";
    } else {
        std::cerr << "Control One open failed. Close SoundSwitch and verify its "
                     "USB driver is installed. Windows error: "
                  << session.last_error() << "\n";
    }
    const auto status = session.status();
    write_report(
        options, self_test_passed, open_passed, jack_one_writes,
        jack_two_writes, blackout_writes, status);
    session.close();

    const bool host_passed = self_test_passed && open_passed &&
        jack_one_writes && jack_two_writes && blackout_writes;
    std::cout << "Report: " << options.report_path << "\n"
              << (host_passed
                  ? "Host-side Control One test passed; physical observation "
                    "is still required.\n"
                  : "Control One host-side test FAILED.\n");
    return host_passed ? 0 : 1;
}
