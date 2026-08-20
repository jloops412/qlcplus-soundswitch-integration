#include "emberlights/os2l_service.hpp"

#include <algorithm>
#include <chrono>
#include <limits>
#include <thread>

namespace emberlights {
namespace {

using SteadyClock = std::chrono::steady_clock;

[[nodiscard]] std::uint64_t monotonic_ms() noexcept {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            SteadyClock::now().time_since_epoch()).count());
}

void add_stats(
    showcore::Os2lServerStats& target,
    const showcore::Os2lServerStats& source) noexcept {
    target.connections += source.connections;
    target.disconnects += source.disconnects;
    target.bytes_received += source.bytes_received;
    target.messages += source.messages;
    target.decode_errors += source.decode_errors;
    target.client_errors += source.client_errors;
    target.bytes_sent += source.bytes_sent;
    target.feedback_messages += source.feedback_messages;
    target.feedback_errors += source.feedback_errors;
}

[[nodiscard]] showcore::Os2lServerStats combined_stats(
    showcore::Os2lServerStats completed,
    const showcore::Os2lServerStats& current) noexcept {
    add_stats(completed, current);
    return completed;
}

template <std::size_t Capacity>
void assign_fixed(
    showcore::FixedText<Capacity>& target,
    std::string_view source) noexcept {
    target.length = std::min(source.size(), target.bytes.size());
    std::copy_n(source.begin(), target.length, target.bytes.begin());
    if (target.length < target.bytes.size()) {
        target.bytes[target.length] = '\0';
    }
}

}  // namespace

Os2lService::~Os2lService() noexcept {
    stop();
}

bool Os2lService::configure(
    bool enabled,
    std::string_view bind_address,
    std::uint16_t port) noexcept {
    if (bind_address.empty() || bind_address.size() >= Config{}.bind.size()) {
        return false;
    }

    Config next;
    next.enabled = enabled;
    next.bind_length = bind_address.size();
    next.port = port;
    std::copy(bind_address.begin(), bind_address.end(), next.bind.begin());

    {
        const std::lock_guard<std::mutex> lock(config_mutex_);
        const auto unchanged = desired_config_.enabled == next.enabled &&
            desired_config_.bind_length == next.bind_length &&
            desired_config_.port == next.port &&
            std::equal(
                desired_config_.bind.begin(),
                desired_config_.bind.begin() +
                    static_cast<std::ptrdiff_t>(desired_config_.bind_length),
                next.bind.begin());
        if (!unchanged) {
            desired_config_ = next;
            config_revision_.fetch_add(1U, std::memory_order_release);
        }
    }

    {
        const std::lock_guard<std::mutex> lock(status_mutex_);
        status_.enabled = enabled;
        assign_fixed(status_.configured_bind, bind_address);
        status_.configured_port = port;
    }

    if (!thread_.joinable()) {
        stop_requested_.store(false, std::memory_order_release);
        try {
            thread_ = std::thread(&Os2lService::run, this);
        } catch (...) {
            const std::lock_guard<std::mutex> lock(status_mutex_);
            status_.running = false;
            status_.listener = showcore::Os2lServerState::Fault;
            return false;
        }
    }
    return true;
}

void Os2lService::stop() noexcept {
    stop_requested_.store(true, std::memory_order_release);
    if (thread_.joinable()) {
        thread_.join();
    }
    consumer_enabled_.store(false, std::memory_order_release);
    const std::lock_guard<std::mutex> lock(status_mutex_);
    status_.running = false;
    status_.listener = showcore::Os2lServerState::Closed;
    status_.discovery = showcore::Os2lDiscoveryState::Unavailable;
    status_.client_connected = false;
    status_.bound_port = 0U;
    status_.blackout_feedback_synchronized = false;
}

Os2lServiceStatus Os2lService::status() const noexcept {
    const std::lock_guard<std::mutex> lock(status_mutex_);
    return status_;
}

void Os2lService::publish_blackout(bool active) noexcept {
    authoritative_blackout_.store(active, std::memory_order_release);
}

std::uint64_t Os2lService::attach_consumer() noexcept {
    consumer_enabled_.store(false, std::memory_order_release);
    auto generation = consumer_generation_.fetch_add(
                          1U, std::memory_order_acq_rel) +
        1U;
    if (generation == 0U) {
        consumer_generation_.store(1U, std::memory_order_release);
        generation = 1U;
    }
    Os2lServiceEvent stale;
    std::uint64_t discarded = 0U;
    while (events_.try_pop(stale)) {
        ++discarded;
    }
    if (discarded != 0U) {
        discarded_while_detached_.fetch_add(
            discarded, std::memory_order_relaxed);
    }
    consumer_enabled_.store(true, std::memory_order_release);
    return generation;
}

void Os2lService::detach_consumer(std::uint64_t generation) noexcept {
    if (generation != 0U &&
        consumer_generation_.load(std::memory_order_acquire) == generation) {
        consumer_enabled_.store(false, std::memory_order_release);
    }
}

bool Os2lService::try_pop(Os2lServiceEvent& event) noexcept {
    return events_.try_pop(event);
}

void Os2lService::receive_callback(
    const showcore::Os2lEvent& event,
    showcore::Os2lParseError error,
    std::string_view raw,
    void* context) noexcept {
    static_cast<Os2lService*>(context)->receive(event, error, raw);
}

void Os2lService::receive(
    const showcore::Os2lEvent& event,
    showcore::Os2lParseError error,
    std::string_view raw) noexcept {
    const auto now = monotonic_ms();
    {
        const std::lock_guard<std::mutex> lock(status_mutex_);
        status_.last_message_ms = now;
        status_.last_decode_error = error;
        status_.last_inbound.length = std::min(
            raw.size(), status_.last_inbound.bytes.size());
        for (std::size_t index = 0U;
             index < status_.last_inbound.length;
             ++index) {
            const auto character = static_cast<unsigned char>(raw[index]);
            status_.last_inbound.bytes[index] =
                character < 0x20U ? ' ' : static_cast<char>(character);
        }
        if (status_.last_inbound.length < status_.last_inbound.bytes.size()) {
            status_.last_inbound.bytes[status_.last_inbound.length] = '\0';
        }
        if (error == showcore::Os2lParseError::None &&
            event.kind == showcore::Os2lKind::Beat) {
            status_.last_beat_ms = now;
        }
    }
    if (error == showcore::Os2lParseError::None) {
        emit(
            Os2lServiceEventKind::Message,
            event,
            session_epoch_.load(std::memory_order_acquire));
    }
}

void Os2lService::emit(
    Os2lServiceEventKind kind,
    const showcore::Os2lEvent& message,
    std::uint64_t session_epoch) noexcept {
    const auto generation =
        consumer_generation_.load(std::memory_order_acquire);
    if (!consumer_enabled_.load(std::memory_order_acquire) ||
        generation == 0U) {
        discarded_while_detached_.fetch_add(1U, std::memory_order_relaxed);
        return;
    }
    const Os2lServiceEvent event{
        kind,
        message,
        session_epoch,
        generation};
    if (!consumer_enabled_.load(std::memory_order_acquire) ||
        consumer_generation_.load(std::memory_order_acquire) != generation) {
        discarded_while_detached_.fetch_add(1U, std::memory_order_relaxed);
        return;
    }
    if (!events_.try_push(event)) {
        dropped_events_.fetch_add(1U, std::memory_order_relaxed);
    }
}

void Os2lService::run() noexcept {
    showcore::Os2lTcpServer server;
    showcore::Os2lServerStats completed_stats{};
    Config active_config{};
    auto applied_revision = std::numeric_limits<std::uint64_t>::max();
    auto next_open_attempt = SteadyClock::now();
    auto next_discovery_recovery = SteadyClock::now();
    bool server_open = false;

    {
        const std::lock_guard<std::mutex> lock(status_mutex_);
        status_.running = true;
    }

    auto record_disconnect = [&]() noexcept {
        const auto epoch = session_epoch_.load(std::memory_order_acquire);
        bool was_connected = false;
        {
            const std::lock_guard<std::mutex> lock(status_mutex_);
            was_connected = status_.client_connected;
            status_.client_connected = false;
            status_.blackout_feedback_synchronized = false;
            if (was_connected) {
                status_.last_disconnect_ms = monotonic_ms();
            }
        }
        if (was_connected) {
            emit(Os2lServiceEventKind::ClientDisconnected, {}, epoch);
        }
    };

    auto close_server = [&]() noexcept {
        record_disconnect();
        if (server_open) {
            server.close();
            add_stats(completed_stats, server.stats());
            server_open = false;
        }
    };

    while (!stop_requested_.load(std::memory_order_acquire)) {
        const auto now = SteadyClock::now();
        const auto revision = config_revision_.load(std::memory_order_acquire);
        if (revision != applied_revision) {
            Config next;
            {
                const std::lock_guard<std::mutex> lock(config_mutex_);
                next = desired_config_;
            }
            close_server();
            active_config = next;
            applied_revision = revision;
            next_open_attempt = now;
            next_discovery_recovery = now + std::chrono::seconds(5);
            {
                const std::lock_guard<std::mutex> lock(status_mutex_);
                status_.enabled = active_config.enabled;
                assign_fixed(
                    status_.configured_bind,
                    std::string_view(
                        active_config.bind.data(), active_config.bind_length));
                status_.configured_port = active_config.port;
                status_.bound_port = 0U;
                status_.listener = showcore::Os2lServerState::Closed;
                status_.discovery = showcore::Os2lDiscoveryState::Unavailable;
                status_.stats = completed_stats;
            }
        }

        if (active_config.enabled && !server_open && now >= next_open_attempt) {
            const auto bind = std::string_view(
                active_config.bind.data(), active_config.bind_length);
            server_open = server.open_ipv4(bind, active_config.port);
            const std::lock_guard<std::mutex> lock(status_mutex_);
            status_.listener = server_open
                ? showcore::Os2lServerState::Listening
                : showcore::Os2lServerState::Fault;
            status_.discovery = server.discovery_state();
            status_.bound_port = server_open ? server.bound_port() : 0U;
            status_.last_socket_error = server.last_error();
            status_.last_discovery_error = server.discovery_last_error();
            if (!server_open) {
                next_open_attempt = now + std::chrono::seconds(2);
            }
        }

        if (server_open) {
            const auto blackout =
                authoritative_blackout_.load(std::memory_order_acquire);
            if (server.state() == showcore::Os2lServerState::ClientConnected) {
                static_cast<void>(server.queue_blackout_feedback(blackout));
            }
            const auto poll = server.poll(&Os2lService::receive_callback, this, 2U);
            if (poll == showcore::Os2lPollResult::ClientConnected) {
                auto epoch = session_epoch_.fetch_add(
                                 1U, std::memory_order_acq_rel) +
                    1U;
                if (epoch == 0U) {
                    session_epoch_.store(1U, std::memory_order_release);
                    epoch = 1U;
                }
                {
                    const std::lock_guard<std::mutex> lock(status_mutex_);
                    status_.client_connected = true;
                    status_.session_epoch = epoch;
                    status_.last_connect_ms = monotonic_ms();
                    status_.last_beat_ms = 0U;
                }
                emit(Os2lServiceEventKind::ClientConnected, {}, epoch);
                static_cast<void>(server.queue_blackout_feedback(blackout));
            } else if (poll == showcore::Os2lPollResult::ClientDisconnected) {
                record_disconnect();
            } else if (poll == showcore::Os2lPollResult::Error) {
                const auto socket_error = server.last_error();
                const auto discovery_error = server.discovery_last_error();
                close_server();
                next_open_attempt = now + std::chrono::seconds(2);
                const std::lock_guard<std::mutex> lock(status_mutex_);
                status_.listener = showcore::Os2lServerState::Fault;
                status_.discovery = showcore::Os2lDiscoveryState::Fault;
                status_.bound_port = 0U;
                status_.last_socket_error = socket_error;
                status_.last_discovery_error = discovery_error;
            }

            if (server_open &&
                server.discovery_state() == showcore::Os2lDiscoveryState::Fault &&
                server.state() != showcore::Os2lServerState::ClientConnected &&
                now >= next_discovery_recovery) {
                // Re-open only while no client is attached. This refreshes a
                // failed DNS-SD registration without disrupting a live client.
                server.close();
                add_stats(completed_stats, server.stats());
                server_open = false;
                next_open_attempt = now;
                next_discovery_recovery = now + std::chrono::seconds(5);
            }

            const auto current_stats = server_open
                ? combined_stats(completed_stats, server.stats())
                : completed_stats;
            const auto synchronized = server_open &&
                server.blackout_feedback_synchronized(blackout);
            const std::lock_guard<std::mutex> lock(status_mutex_);
            if (server_open) {
                status_.listener = server.state();
                status_.discovery = server.discovery_state();
                status_.bound_port = server.bound_port();
                status_.last_socket_error = server.last_error();
                status_.last_discovery_error =
                    server.discovery_last_error();
            } else {
                status_.bound_port = 0U;
            }
            if (synchronized &&
                current_stats.feedback_messages !=
                    status_.stats.feedback_messages) {
                status_.last_feedback_valid = true;
                status_.last_feedback_blackout = blackout;
                status_.last_feedback_ms = monotonic_ms();
            }
            status_.stats = current_stats;
            status_.dropped_events =
                dropped_events_.load(std::memory_order_relaxed);
            status_.discarded_while_detached =
                discarded_while_detached_.load(std::memory_order_relaxed);
            status_.authoritative_blackout = blackout;
            status_.blackout_feedback_synchronized = synchronized;
        } else {
            const std::lock_guard<std::mutex> lock(status_mutex_);
            status_.stats = completed_stats;
            status_.dropped_events =
                dropped_events_.load(std::memory_order_relaxed);
            status_.discarded_while_detached =
                discarded_while_detached_.load(std::memory_order_relaxed);
            status_.authoritative_blackout =
                authoritative_blackout_.load(std::memory_order_relaxed);
            status_.blackout_feedback_synchronized = false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    close_server();
    const std::lock_guard<std::mutex> lock(status_mutex_);
    status_.running = false;
    status_.listener = showcore::Os2lServerState::Closed;
    status_.discovery = showcore::Os2lDiscoveryState::Unavailable;
    status_.bound_port = 0U;
    status_.stats = completed_stats;
    status_.blackout_feedback_synchronized = false;
}

}  // namespace emberlights
