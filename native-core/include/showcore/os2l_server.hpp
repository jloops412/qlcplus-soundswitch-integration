#pragma once

#include "showcore/os2l.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace showcore {

enum class Os2lServerState : std::uint8_t {
    Closed,
    Listening,
    ClientConnected,
    Fault
};

enum class Os2lPollResult : std::uint8_t {
    Idle,
    ClientConnected,
    EventsReceived,
    ClientDisconnected,
    Error
};

// VirtualDJ normally locates OS2L servers through DNS-SD. Direct-IP mode is
// still supported by the TCP listener, but VirtualDJ may defer that connection
// until an OS2L action is sent. Discovery is therefore best-effort and must
// never prevent the direct listener from opening.
enum class Os2lDiscoveryState : std::uint8_t {
    Unavailable,
    Starting,
    Advertised,
    Fault
};

struct Os2lServerStats {
    std::uint64_t connections{0};
    std::uint64_t disconnects{0};
    std::uint64_t bytes_received{0};
    std::uint64_t messages{0};
    std::uint64_t decode_errors{0};
    std::uint64_t client_errors{0};
    std::uint64_t bytes_sent{0};
    std::uint64_t feedback_messages{0};
    std::uint64_t feedback_errors{0};
};

class Os2lTcpServer {
public:
    Os2lTcpServer() noexcept = default;
    ~Os2lTcpServer() noexcept;

    Os2lTcpServer(const Os2lTcpServer&) = delete;
    Os2lTcpServer& operator=(const Os2lTcpServer&) = delete;

    [[nodiscard]] bool open_ipv4(
        std::string_view bind_address = "127.0.0.1",
        std::uint16_t port = 9996) noexcept;
    void close() noexcept;
    [[nodiscard]] Os2lPollResult poll(
        Os2lStreamCallback callback,
        void* context,
        std::uint32_t timeout_ms = 0) noexcept;
    // Queues the authoritative blackout state for the connected OS2L client.
    // Socket writes remain owned by poll(); repeated identical states collapse
    // and a newer state supersedes any not-yet-started follow-up message.
    [[nodiscard]] bool queue_blackout_feedback(bool active) noexcept;
    [[nodiscard]] bool blackout_feedback_synchronized(bool active) const noexcept;

    [[nodiscard]] Os2lServerState state() const noexcept { return state_; }
    [[nodiscard]] std::uint16_t bound_port() const noexcept { return bound_port_; }
    [[nodiscard]] int last_error() const noexcept { return last_error_; }
    [[nodiscard]] Os2lDiscoveryState discovery_state() const noexcept {
        return discovery_state_;
    }
    [[nodiscard]] int discovery_last_error() const noexcept {
        return discovery_last_error_;
    }
    [[nodiscard]] const Os2lServerStats& stats() const noexcept { return stats_; }

private:
    void disconnect_client(bool error) noexcept;
    [[nodiscard]] bool flush_feedback() noexcept;
    void prepare_feedback(bool active) noexcept;
    void start_discovery(std::string_view bind_address) noexcept;
    void refresh_discovery() noexcept;
    void stop_discovery() noexcept;

    std::intptr_t listener_{-1};
    std::intptr_t client_{-1};
    std::uint16_t bound_port_{0};
    int last_error_{0};
    Os2lServerState state_{Os2lServerState::Closed};
    Os2lDiscoveryState discovery_state_{Os2lDiscoveryState::Unavailable};
    int discovery_last_error_{0};
    Os2lServerStats stats_{};
    Os2lStreamDecoder decoder_{};
    std::array<char, 64U> feedback_buffer_{};
    std::size_t feedback_offset_{0U};
    std::size_t feedback_size_{0U};
    bool feedback_state_{false};
    bool follow_up_pending_{false};
    bool follow_up_state_{false};
    bool last_feedback_valid_{false};
    bool last_feedback_state_{false};
#ifdef _WIN32
    bool winsock_started_{false};
    void* discovery_registration_{nullptr};
#endif
};

}  // namespace showcore
