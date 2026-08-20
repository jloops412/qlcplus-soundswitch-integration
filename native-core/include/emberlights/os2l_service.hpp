#pragma once

#include "showcore/os2l.hpp"
#include "showcore/os2l_server.hpp"
#include "showcore/spsc_queue.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string_view>
#include <thread>

namespace emberlights {

enum class Os2lServiceEventKind : std::uint8_t {
    ClientConnected,
    ClientDisconnected,
    Message
};

struct Os2lServiceEvent {
    Os2lServiceEventKind kind{Os2lServiceEventKind::Message};
    showcore::Os2lEvent message{};
    std::uint64_t session_epoch{0U};
    std::uint64_t consumer_generation{0U};
};

// Network and beat-traffic facts intentionally remain separate from Runner's
// SyncManager state. A connected client is not proof of current beat sync.
struct Os2lServiceStatus {
    bool running{false};
    bool enabled{false};
    showcore::Os2lServerState listener{showcore::Os2lServerState::Closed};
    showcore::Os2lDiscoveryState discovery{
        showcore::Os2lDiscoveryState::Unavailable};
    bool client_connected{false};
    std::uint64_t session_epoch{0U};
    showcore::FixedText<64U> configured_bind{};
    std::uint16_t configured_port{0U};
    std::uint16_t bound_port{0U};
    std::int32_t last_socket_error{0};
    std::int32_t last_discovery_error{0};
    std::uint64_t last_connect_ms{0U};
    std::uint64_t last_disconnect_ms{0U};
    std::uint64_t last_message_ms{0U};
    std::uint64_t last_beat_ms{0U};
    showcore::Os2lParseError last_decode_error{
        showcore::Os2lParseError::None};
    showcore::FixedText<384U> last_inbound{};
    showcore::Os2lServerStats stats{};
    std::uint64_t dropped_events{0U};
    std::uint64_t discarded_while_detached{0U};
    bool authoritative_blackout{false};
    bool blackout_feedback_synchronized{false};
    bool last_feedback_valid{false};
    bool last_feedback_blackout{false};
    std::uint64_t last_feedback_ms{0U};
};

// Application-owned OS2L transport. Its thread owns listener, discovery,
// accepted-client, parsing, and outbound feedback lifetime. Runner may attach
// as a bounded event consumer, but starting/stopping Runner never starts or
// stops this service.
class Os2lService {
public:
    Os2lService() noexcept = default;
    ~Os2lService() noexcept;

    Os2lService(const Os2lService&) = delete;
    Os2lService& operator=(const Os2lService&) = delete;

    [[nodiscard]] bool configure(
        bool enabled,
        std::string_view bind_address = "127.0.0.1",
        std::uint16_t port = 9996U) noexcept;
    void stop() noexcept;

    [[nodiscard]] Os2lServiceStatus status() const noexcept;
    void publish_blackout(bool active) noexcept;

    // Exactly one active consumer is supported. Generation tags make an event
    // queued by a prior Runner lifetime harmless after a new attach.
    [[nodiscard]] std::uint64_t attach_consumer() noexcept;
    void detach_consumer(std::uint64_t generation) noexcept;
    [[nodiscard]] bool try_pop(Os2lServiceEvent& event) noexcept;

private:
    struct Config {
        bool enabled{false};
        std::array<char, 64U> bind{};
        std::size_t bind_length{0U};
        std::uint16_t port{0U};
    };

    using EventQueue = showcore::SpscQueue<Os2lServiceEvent, 1025U>;

    static void receive_callback(
        const showcore::Os2lEvent& event,
        showcore::Os2lParseError error,
        std::string_view raw,
        void* context) noexcept;
    void receive(
        const showcore::Os2lEvent& event,
        showcore::Os2lParseError error,
        std::string_view raw) noexcept;
    void emit(
        Os2lServiceEventKind kind,
        const showcore::Os2lEvent& message,
        std::uint64_t session_epoch) noexcept;
    void run() noexcept;

    mutable std::mutex config_mutex_{};
    Config desired_config_{};
    std::atomic<std::uint64_t> config_revision_{0U};

    mutable std::mutex status_mutex_{};
    Os2lServiceStatus status_{};

    EventQueue events_{};
    std::atomic<bool> consumer_enabled_{false};
    std::atomic<std::uint64_t> consumer_generation_{0U};
    std::atomic<std::uint64_t> dropped_events_{0U};
    std::atomic<std::uint64_t> discarded_while_detached_{0U};
    std::atomic<std::uint64_t> session_epoch_{0U};
    std::atomic<bool> authoritative_blackout_{false};
    std::atomic<bool> stop_requested_{false};
    std::thread thread_{};
};

}  // namespace emberlights
