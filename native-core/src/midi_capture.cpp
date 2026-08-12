#include "showcore/midi.hpp"
#include "showcore/winmm_midi.hpp"

#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>

namespace {

volatile std::sig_atomic_t g_interrupted = 0;

void handle_signal(int) noexcept {
    g_interrupted = 1;
}

[[nodiscard]] bool parse_unsigned(std::string_view text, std::uint32_t& value) noexcept {
    value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

[[nodiscard]] const char* message_name(showcore::MidiMessageType type) noexcept {
    switch (type) {
    case showcore::MidiMessageType::NoteOn: return "note_on";
    case showcore::MidiMessageType::NoteOff: return "note_off";
    case showcore::MidiMessageType::ControlChange: return "cc";
    case showcore::MidiMessageType::PitchBend: return "pitch_bend";
    }
    return "unknown";
}

[[nodiscard]] std::string json_escape(std::string_view value) {
    std::ostringstream output;
    for (const auto character : value) {
        switch (character) {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (static_cast<unsigned char>(character) < 0x20U) {
                output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                       << static_cast<unsigned int>(
                              static_cast<unsigned char>(character));
            } else {
                output << character;
            }
        }
    }
    return output.str();
}

[[nodiscard]] std::string raw_short_hex(const showcore::MidiMessage& message) {
    const auto encoded = showcore::encode_short_midi(message);
    if (!encoded) {
        return {};
    }
    std::ostringstream output;
    output << std::hex << std::setfill('0')
           << std::setw(2) << (encoded.packed & 0xFFU) << ' '
           << std::setw(2) << ((encoded.packed >> 8U) & 0xFFU) << ' '
           << std::setw(2) << ((encoded.packed >> 16U) & 0xFFU);
    return output.str();
}

void print_json_message(
    std::ostream& output,
    const showcore::MidiMessage& message,
    std::string_view label) {
    output << "{\"event\":\"midi_short\",\"label\":\""
           << json_escape(label)
           << "\",\"device\":" << message.device_id
           << ",\"type\":\"" << message_name(message.type)
           << "\",\"channel\":" << static_cast<unsigned>(message.channel)
           << ",\"number\":" << static_cast<unsigned>(message.number)
           << ",\"value\":" << message.value
           << ",\"timestampMs\":" << message.timestamp_ms
           << ",\"rawHex\":\"" << raw_short_hex(message) << "\"}\n";
}

void print_ports(const showcore::MidiPortList& ports, std::string_view heading) {
    std::cout << heading << " count=" << ports.count;
    if (ports.truncated) {
        std::cout << " truncated=true";
    }
    std::cout << '\n';
    for (std::size_t index = 0; index < ports.count; ++index) {
        const auto& port = ports.ports[index];
        std::cout << "  index=" << port.system_index
                  << " manufacturer=" << port.manufacturer_id
                  << " product=" << port.product_id
                  << " driver=" << port.driver_version
                  << " name=" << port.name() << '\n';
    }
}

void print_usage() {
    std::cout
        << "Usage: midi_capture [--list] [--all | --input INDEX ...] [options]\n"
        << "  --list              List WinMM MIDI input/output ports\n"
        << "  --all               Monitor every enumerated input port\n"
        << "  --input INDEX       Monitor one input; may be repeated\n"
        << "  --duration SECONDS  Stop automatically; 0 runs until Ctrl+C\n"
        << "  --json-lines        Emit stable machine-readable short-MIDI events\n"
        << "  --label TEXT        Tag every captured event with one logical control ID\n"
        << "  --output FILE       Save JSON Lines capture to a new file (implies --json-lines)\n"
        << "  --force             Permit replacing the selected capture output file\n"
        << "\nControl One example:\n"
        << "  midi_capture --input 0 --duration 10 --label pads.autoloop.1 "
           "--output control-one-pad-1.jsonl\n"
        << "Capture one labeled physical control at a time with SoundSwitch closed. "
           "This tool records short MIDI input only; it never probes output feedback or OLED.\n"
        << "  --help              Show this help\n";
}

}  // namespace

int main(int argc, char** argv) {
    std::array<std::uint32_t, showcore::kMaxOpenMidiPorts> requested_inputs{};
    std::size_t requested_count = 0;
    std::uint32_t duration_seconds = 0;
    bool list = argc == 1;
    bool all = false;
    bool json_lines = false;
    bool force = false;
    std::string label;
    std::filesystem::path output_path;

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--help") {
            print_usage();
            return EXIT_SUCCESS;
        }
        if (argument == "--list") {
            list = true;
            continue;
        }
        if (argument == "--all") {
            all = true;
            continue;
        }
        if (argument == "--json-lines") {
            json_lines = true;
            continue;
        }
        if (argument == "--force") {
            force = true;
            continue;
        }
        if ((argument == "--label" || argument == "--output") && index + 1 < argc) {
            if (argument == "--label") {
                label = argv[++index];
                if (label.size() > 160U) {
                    std::cerr << "Capture label is longer than 160 characters\n";
                    return EXIT_FAILURE;
                }
            } else {
                output_path = std::filesystem::path(argv[++index]);
                json_lines = true;
            }
            continue;
        }
        if ((argument == "--input" || argument == "--duration") && index + 1 < argc) {
            std::uint32_t value = 0;
            if (!parse_unsigned(argv[++index], value)) {
                print_usage();
                return EXIT_FAILURE;
            }
            if (argument == "--duration") {
                if (value > 86400U) {
                    print_usage();
                    return EXIT_FAILURE;
                }
                duration_seconds = value;
            } else {
                if (requested_count >= requested_inputs.size()) {
                    std::cerr << "Too many input ports requested\n";
                    return EXIT_FAILURE;
                }
                requested_inputs[requested_count++] = value;
            }
            continue;
        }
        print_usage();
        return EXIT_FAILURE;
    }

    const auto inputs = showcore::enumerate_winmm_midi_inputs();
    const auto outputs = showcore::enumerate_winmm_midi_outputs();
    if (list || all || requested_count == 0U) {
        print_ports(inputs, "MIDI inputs");
        print_ports(outputs, "MIDI outputs");
    }
    if (!showcore::WinMmMidiInput::supported()) {
        std::cout << "WinMM MIDI capture is available in the Windows build\n";
        return requested_count == 0U && !all ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (all) {
        requested_count = 0;
        for (std::size_t index = 0;
             index < inputs.count && requested_count < requested_inputs.size();
             ++index) {
            requested_inputs[requested_count++] = inputs.ports[index].system_index;
        }
    }
    if (requested_count == 0U) {
        return EXIT_SUCCESS;
    }

    std::ofstream capture_file;
    std::ostream* event_output = &std::cout;
    if (!output_path.empty()) {
        std::error_code filesystem_error;
        if (!force && std::filesystem::exists(output_path, filesystem_error)) {
            std::cerr << "Capture output already exists; use --force to replace it\n";
            return EXIT_FAILURE;
        }
        const auto parent = output_path.parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent, filesystem_error);
            if (filesystem_error) {
                std::cerr << "Unable to create capture output folder\n";
                return EXIT_FAILURE;
            }
        }
        capture_file.open(output_path, std::ios::binary | std::ios::trunc);
        if (!capture_file) {
            std::cerr << "Unable to open capture output file\n";
            return EXIT_FAILURE;
        }
        event_output = &capture_file;
    }

    showcore::WinMmMidiInput capture;
    for (std::size_t index = 0; index < requested_count; ++index) {
        const auto logical_device_id = static_cast<std::uint32_t>(index + 1U);
        if (!capture.open(requested_inputs[index], logical_device_id)) {
            std::cerr << "Unable to open MIDI input index=" << requested_inputs[index]
                      << " error=" << capture.last_error() << '\n';
            return EXIT_FAILURE;
        }
        std::cout << "Monitoring MIDI input index=" << requested_inputs[index]
                  << " logical_device=" << logical_device_id << '\n';
    }

    if (json_lines) {
        *event_output << "{\"event\":\"capture_start\",\"format\":"
                      << "\"emberlights-midi-short-capture\",\"formatVersion\":1,"
                      << "\"label\":\"" << json_escape(label)
                      << "\",\"durationSeconds\":" << duration_seconds
                      << ",\"inputSystemIndices\":[";
        for (std::size_t index = 0U; index < requested_count; ++index) {
            *event_output << (index == 0U ? "" : ",") << requested_inputs[index];
        }
        *event_output << "]}\n";
    }

    std::signal(SIGINT, &handle_signal);
    std::signal(SIGTERM, &handle_signal);
    const auto started = std::chrono::steady_clock::now();
    std::uint64_t message_count = 0;
    while (g_interrupted == 0) {
        showcore::MidiMessage message;
        bool received = false;
        while (capture.poll(message)) {
            received = true;
            ++message_count;
            if (json_lines) {
                print_json_message(*event_output, message, label);
            } else {
                std::cout << "midi device=" << message.device_id
                          << " type=" << message_name(message.type)
                          << " channel=" << static_cast<unsigned>(message.channel + 1U)
                          << " number=" << static_cast<unsigned>(message.number)
                          << " value=" << message.value
                          << " timestamp_ms=" << message.timestamp_ms << '\n';
            }
        }
        if (duration_seconds > 0U &&
            std::chrono::steady_clock::now() - started >=
                std::chrono::seconds(duration_seconds)) {
            break;
        }
        if (!received) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    if (json_lines) {
        *event_output << "{\"event\":\"capture_stop\",\"label\":\""
                      << json_escape(label)
                      << "\",\"messages\":" << message_count
                      << ",\"dropped\":" << capture.dropped_messages() << "}\n";
        event_output->flush();
    }
    std::cout << "MIDI capture stopped messages=" << message_count
              << " dropped=" << capture.dropped_messages();
    if (!output_path.empty()) {
        std::cout << " output=" << output_path.string();
    }
    std::cout << '\n';
    capture.close_all();
    return EXIT_SUCCESS;
}
