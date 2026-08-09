#include "showcore/os2l_server.hpp"

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

void print_event(
    const showcore::Os2lEvent& event,
    showcore::Os2lParseError error,
    std::string_view raw,
    void*) noexcept {
    if (error != showcore::Os2lParseError::None) {
        std::cout << "invalid error=" << static_cast<int>(error) << " raw=" << raw << '\n';
        return;
    }

    switch (event.kind) {
    case showcore::Os2lKind::Beat:
        std::cout << "beat pos=" << event.beat.position << " bpm=" << event.beat.bpm
                  << " change=" << event.beat.change;
        if (event.beat.has_strength) {
            std::cout << " strength=" << event.beat.strength;
        }
        std::cout << '\n';
        break;
    case showcore::Os2lKind::Button:
        std::cout << "button name=" << event.button.name.view()
                  << " state=" << (event.button.on ? "on" : "off") << '\n';
        break;
    case showcore::Os2lKind::Command:
        std::cout << "command id=" << event.command.id
                  << " parameter=" << event.command.parameter << '\n';
        break;
    case showcore::Os2lKind::Feedback:
        std::cout << "feedback name=" << event.button.name.view()
                  << " state=" << (event.button.on ? "on" : "off") << '\n';
        break;
    case showcore::Os2lKind::Unknown:
        std::cout << "unknown raw=" << raw << '\n';
        break;
    case showcore::Os2lKind::Invalid:
    default:
        std::cout << "invalid raw=" << raw << '\n';
        break;
    }
}

[[nodiscard]] bool parse_port(std::string_view text, std::uint16_t& port) {
    unsigned value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
        value == 0 || value > 65535) {
        return false;
    }
    port = static_cast<std::uint16_t>(value);
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    std::string_view bind_address = "127.0.0.1";
    std::uint16_t port = 9996;
    bool once = false;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--bind" && index + 1 < argc) {
            bind_address = argv[++index];
        } else if (argument == "--port" && index + 1 < argc) {
            if (!parse_port(argv[++index], port)) {
                std::cerr << "Invalid port\n";
                return EXIT_FAILURE;
            }
        } else if (argument == "--once") {
            once = true;
        } else {
            std::cerr << "Usage: os2l_capture [--bind IPv4] [--port PORT] [--once]\n";
            return EXIT_FAILURE;
        }
    }

    showcore::Os2lTcpServer server;
    if (!server.open_ipv4(bind_address, port)) {
        std::cerr << "Unable to listen on " << bind_address << ':' << port
                  << " error=" << server.last_error() << '\n';
        return EXIT_FAILURE;
    }

    std::cout << "Listening for OS2L on " << bind_address << ':' << server.bound_port() << '\n';
    bool connected_once = false;
    while (true) {
        switch (server.poll(&print_event, nullptr, 1000)) {
        case showcore::Os2lPollResult::ClientConnected:
            connected_once = true;
            std::cout << "OS2L client connected\n";
            break;
        case showcore::Os2lPollResult::ClientDisconnected:
            std::cout << "OS2L client disconnected; listener remains available\n";
            if (once && connected_once) {
                return EXIT_SUCCESS;
            }
            break;
        case showcore::Os2lPollResult::Error:
            std::cerr << "OS2L server fault error=" << server.last_error() << '\n';
            return EXIT_FAILURE;
        case showcore::Os2lPollResult::EventsReceived:
        case showcore::Os2lPollResult::Idle:
            break;
        }
    }
}
