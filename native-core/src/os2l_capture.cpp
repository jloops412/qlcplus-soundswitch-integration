#include "showcore/os2l_server.hpp"

#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

struct CaptureContext {
    std::uint64_t sequence{0U};
};

[[nodiscard]] std::uint64_t unix_time_ms() noexcept {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
}

void begin_record(CaptureContext& capture, std::string_view event) {
    std::cout << "seq=" << ++capture.sequence
              << " unix_ms=" << unix_time_ms()
              << " event=" << event;
}

void finish_record() {
    std::cout << '\n';
    std::cout.flush();
}

void print_event(
    const showcore::Os2lEvent& event,
    showcore::Os2lParseError error,
    std::string_view raw,
    void* context) noexcept {
    auto& capture = *static_cast<CaptureContext*>(context);
    begin_record(capture, "message");
    if (error != showcore::Os2lParseError::None) {
        std::cout << " kind=invalid parse_error=" << static_cast<int>(error);
        std::cout << " raw_bytes=" << raw.size() << " raw=";
        std::cout.write(raw.data(), static_cast<std::streamsize>(raw.size()));
        finish_record();
        return;
    }

    switch (event.kind) {
    case showcore::Os2lKind::Beat:
        std::cout << " kind=beat pos=" << event.beat.position
                  << " bpm=" << event.beat.bpm
                  << " change=" << event.beat.change;
        if (event.beat.has_strength) {
            std::cout << " strength=" << event.beat.strength;
        }
        break;
    case showcore::Os2lKind::Button:
        std::cout << " kind=button name=" << event.button.name.view()
                  << " state=" << (event.button.on ? "on" : "off");
        break;
    case showcore::Os2lKind::Command:
        std::cout << " kind=command id=" << event.command.id
                  << " parameter=" << event.command.parameter;
        break;
    case showcore::Os2lKind::Feedback:
        std::cout << " kind=feedback name=" << event.button.name.view()
                  << " state=" << (event.button.on ? "on" : "off");
        break;
    case showcore::Os2lKind::Unknown:
        std::cout << " kind=unknown";
        break;
    case showcore::Os2lKind::Invalid:
    default:
        std::cout << " kind=invalid";
        break;
    }
    std::cout << " raw_bytes=" << raw.size() << " raw=";
    std::cout.write(raw.data(), static_cast<std::streamsize>(raw.size()));
    finish_record();
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

    CaptureContext capture;
    showcore::Os2lTcpServer server;
    if (!server.open_ipv4(bind_address, port)) {
        begin_record(capture, "listener_fault");
        std::cout << " bind=" << bind_address << " configured_port=" << port
                  << " error=" << server.last_error();
        finish_record();
        return EXIT_FAILURE;
    }

    begin_record(capture, "listener_ready");
    std::cout << " bind=" << bind_address
              << " configured_port=" << port
              << " bound_port=" << server.bound_port()
              << " discovery_state="
              << static_cast<int>(server.discovery_state());
    finish_record();
    bool connected_once = false;
    while (true) {
        switch (server.poll(&print_event, &capture, 1000)) {
        case showcore::Os2lPollResult::ClientConnected:
            connected_once = true;
            begin_record(capture, "client_connected");
            std::cout << " connection=" << server.stats().connections;
            finish_record();
            break;
        case showcore::Os2lPollResult::ClientDisconnected:
            begin_record(capture, "client_disconnected");
            std::cout << " disconnect=" << server.stats().disconnects
                      << " listener_available=true";
            finish_record();
            if (once && connected_once) {
                return EXIT_SUCCESS;
            }
            break;
        case showcore::Os2lPollResult::Error:
            begin_record(capture, "server_fault");
            std::cout << " error=" << server.last_error();
            finish_record();
            return EXIT_FAILURE;
        case showcore::Os2lPollResult::EventsReceived:
        case showcore::Os2lPollResult::Idle:
            break;
        }
    }
}
