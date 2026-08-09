#include "showcore/midi.hpp"
#include "showcore/winmm_midi.hpp"

#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>
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
        << "Usage: midi_capture [--list] [--all | --input INDEX ...] [--duration SECONDS]\n"
        << "  --list              List WinMM MIDI input/output ports\n"
        << "  --all               Monitor every enumerated input port\n"
        << "  --input INDEX       Monitor one input; may be repeated\n"
        << "  --duration SECONDS  Stop automatically; 0 runs until Ctrl+C\n"
        << "  --help              Show this help\n";
}

}  // namespace

int main(int argc, char** argv) {
    std::array<std::uint32_t, showcore::kMaxOpenMidiPorts> requested_inputs{};
    std::size_t requested_count = 0;
    std::uint32_t duration_seconds = 0;
    bool list = argc == 1;
    bool all = false;

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
            std::cout << "midi device=" << message.device_id
                      << " type=" << message_name(message.type)
                      << " channel=" << static_cast<unsigned>(message.channel + 1U)
                      << " number=" << static_cast<unsigned>(message.number)
                      << " value=" << message.value
                      << " timestamp_ms=" << message.timestamp_ms << '\n';
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

    std::cout << "MIDI capture stopped messages=" << message_count
              << " dropped=" << capture.dropped_messages() << '\n';
    capture.close_all();
    return EXIT_SUCCESS;
}
