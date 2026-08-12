#include "showcore/os2l_server.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <new>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windns.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace showcore {
namespace {

#ifdef _WIN32
using NativeSocket = SOCKET;
constexpr NativeSocket kInvalidSocket = INVALID_SOCKET;

[[nodiscard]] NativeSocket native_socket(std::intptr_t handle) noexcept {
    return static_cast<NativeSocket>(handle);
}

[[nodiscard]] int network_error() noexcept {
    return ::WSAGetLastError();
}

[[nodiscard]] bool recoverable_error(int error) noexcept {
    return error == WSAEINTR || error == WSAEWOULDBLOCK;
}

void close_native_socket(NativeSocket socket) noexcept {
    if (socket != kInvalidSocket) {
        ::closesocket(socket);
    }
}

struct WindowsDnsSdRegistration {
    std::atomic<std::uint32_t> references{1U};
    std::atomic<Os2lDiscoveryState> state{Os2lDiscoveryState::Starting};
    std::atomic<int> last_error{0};
    std::atomic<bool> closing{false};
    std::atomic<bool> deregistering{false};
    DNS_SERVICE_INSTANCE* instance{nullptr};
    DNS_SERVICE_REGISTER_REQUEST request{};
    DNS_SERVICE_CANCEL cancel{};
};

void retain_registration(WindowsDnsSdRegistration* registration) noexcept {
    registration->references.fetch_add(1U, std::memory_order_relaxed);
}

void release_registration(WindowsDnsSdRegistration* registration) noexcept {
    if (registration->references.fetch_sub(1U, std::memory_order_acq_rel) == 1U) {
        if (registration->instance != nullptr) {
            ::DnsServiceFreeInstance(registration->instance);
        }
        delete registration;
    }
}

void begin_deregister(WindowsDnsSdRegistration* registration) noexcept;

void WINAPI dns_sd_complete(
    DWORD status,
    PVOID context,
    PDNS_SERVICE_INSTANCE instance) noexcept {
    auto* registration = static_cast<WindowsDnsSdRegistration*>(context);
    if (instance != nullptr) {
        ::DnsServiceFreeInstance(instance);
    }

    if (!registration->closing.load(std::memory_order_acquire)) {
        registration->last_error.store(
            status == ERROR_SUCCESS ? 0 : static_cast<int>(status),
            std::memory_order_relaxed);
        registration->state.store(
            status == ERROR_SUCCESS
                ? Os2lDiscoveryState::Advertised
                : Os2lDiscoveryState::Fault,
            std::memory_order_release);
    }

    // close() can race the registration callback. Recheck closing after
    // publishing the result so a late successful registration is immediately
    // withdrawn instead of leaving a stale service behind.
    if (status == ERROR_SUCCESS &&
        registration->closing.load(std::memory_order_acquire)) {
        begin_deregister(registration);
    }
    release_registration(registration);
}

void begin_deregister(WindowsDnsSdRegistration* registration) noexcept {
    bool expected = false;
    if (!registration->deregistering.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel)) {
        return;
    }
    retain_registration(registration);
    const auto result = ::DnsServiceDeRegister(&registration->request, nullptr);
    if (result != DNS_REQUEST_PENDING) {
        registration->last_error.store(static_cast<int>(result), std::memory_order_relaxed);
        release_registration(registration);
    }
}

[[nodiscard]] std::wstring dns_sd_service_name() {
    std::array<wchar_t, 128> hostname{};
    DWORD hostname_size = static_cast<DWORD>(hostname.size());
    std::wstring result = L"EmberLights";
    if (::GetComputerNameW(hostname.data(), &hostname_size) != FALSE && hostname_size > 0U) {
        result += L" @ ";
        result.append(hostname.data(), hostname_size);
    }
    result += L"._os2l._tcp.local";
    return result;
}

[[nodiscard]] std::wstring dns_sd_hostname() {
    std::array<wchar_t, 128> hostname{};
    DWORD hostname_size = static_cast<DWORD>(hostname.size());
    if (::GetComputerNameW(hostname.data(), &hostname_size) == FALSE || hostname_size == 0U) {
        return L"EmberLights.local";
    }
    std::wstring result(hostname.data(), hostname_size);
    result += L".local";
    return result;
}
#else
using NativeSocket = int;
constexpr NativeSocket kInvalidSocket = -1;

[[nodiscard]] NativeSocket native_socket(std::intptr_t handle) noexcept {
    return static_cast<NativeSocket>(handle);
}

[[nodiscard]] int network_error() noexcept {
    return errno;
}

[[nodiscard]] bool recoverable_error(int error) noexcept {
    return error == EINTR || error == EAGAIN || error == EWOULDBLOCK;
}

void close_native_socket(NativeSocket socket) noexcept {
    if (socket != kInvalidSocket) {
        ::close(socket);
    }
}
#endif

}  // namespace

Os2lTcpServer::~Os2lTcpServer() noexcept {
    close();
}

bool Os2lTcpServer::open_ipv4(
    std::string_view bind_address,
    std::uint16_t port) noexcept {
    close();
    stats_ = {};
    last_error_ = 0;
    if (bind_address.empty() || bind_address.size() >= 64U) {
        state_ = Os2lServerState::Fault;
        return false;
    }

#ifdef _WIN32
    WSADATA winsock{};
    if (::WSAStartup(MAKEWORD(2, 2), &winsock) != 0) {
        last_error_ = network_error();
        state_ = Os2lServerState::Fault;
        return false;
    }
    winsock_started_ = true;
#endif

    const auto listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == kInvalidSocket) {
        last_error_ = network_error();
        state_ = Os2lServerState::Fault;
        close();
        state_ = Os2lServerState::Fault;
        return false;
    }
    listener_ = static_cast<std::intptr_t>(listener);

    int reuse = 1;
#ifdef _WIN32
    static_cast<void>(::setsockopt(
        listener,
        SOL_SOCKET,
        SO_REUSEADDR,
        reinterpret_cast<const char*>(&reuse),
        static_cast<int>(sizeof(reuse))));
#else
    static_cast<void>(::setsockopt(
        listener,
        SOL_SOCKET,
        SO_REUSEADDR,
        &reuse,
        sizeof(reuse)));
#endif

    std::array<char, 64> address_buffer{};
    std::copy(bind_address.begin(), bind_address.end(), address_buffer.begin());
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (::inet_pton(AF_INET, address_buffer.data(), &address.sin_addr) != 1 ||
        ::bind(
            listener,
            reinterpret_cast<const sockaddr*>(&address),
            static_cast<int>(sizeof(address))) != 0 ||
        ::listen(listener, 2) != 0) {
        last_error_ = network_error();
        close();
        state_ = Os2lServerState::Fault;
        return false;
    }

    sockaddr_in actual{};
#ifdef _WIN32
    int actual_size = sizeof(actual);
#else
    socklen_t actual_size = sizeof(actual);
#endif
    if (::getsockname(
            listener,
            reinterpret_cast<sockaddr*>(&actual),
            &actual_size) != 0) {
        last_error_ = network_error();
        close();
        state_ = Os2lServerState::Fault;
        return false;
    }

    bound_port_ = ntohs(actual.sin_port);
    state_ = Os2lServerState::Listening;
    start_discovery(bind_address);
    return true;
}

void Os2lTcpServer::start_discovery(std::string_view bind_address) noexcept {
    discovery_state_ = Os2lDiscoveryState::Unavailable;
    discovery_last_error_ = 0;
#ifdef _WIN32
    auto* registration = new (std::nothrow) WindowsDnsSdRegistration();
    if (registration == nullptr) {
        discovery_state_ = Os2lDiscoveryState::Fault;
        discovery_last_error_ = ERROR_NOT_ENOUGH_MEMORY;
        return;
    }

    std::array<char, 64> address_buffer{};
    std::copy(bind_address.begin(), bind_address.end(), address_buffer.begin());
    IP4_ADDRESS ipv4{};
    const auto has_specific_ipv4 = bind_address != "0.0.0.0" &&
        ::inet_pton(AF_INET, address_buffer.data(), &ipv4) == 1;
    const auto service_name = dns_sd_service_name();
    const auto hostname = dns_sd_hostname();
    registration->instance = ::DnsServiceConstructInstance(
        service_name.c_str(),
        hostname.c_str(),
        has_specific_ipv4 ? &ipv4 : nullptr,
        nullptr,
        bound_port_,
        0U,
        0U,
        0U,
        nullptr,
        nullptr);
    if (registration->instance == nullptr) {
        discovery_state_ = Os2lDiscoveryState::Fault;
        discovery_last_error_ = static_cast<int>(::GetLastError());
        release_registration(registration);
        return;
    }

    registration->request.Version = DNS_QUERY_REQUEST_VERSION1;
    registration->request.InterfaceIndex = 0U;
    registration->request.pServiceInstance = registration->instance;
    registration->request.pRegisterCompletionCallback = &dns_sd_complete;
    registration->request.pQueryContext = registration;
    registration->request.hCredentials = nullptr;
    registration->request.unicastEnabled = FALSE;

    discovery_registration_ = registration;
    discovery_state_ = Os2lDiscoveryState::Starting;
    retain_registration(registration);
    const auto result = ::DnsServiceRegister(&registration->request, &registration->cancel);
    if (result != DNS_REQUEST_PENDING) {
        registration->last_error.store(static_cast<int>(result), std::memory_order_relaxed);
        registration->state.store(Os2lDiscoveryState::Fault, std::memory_order_release);
        release_registration(registration);
        refresh_discovery();
    }
#else
    static_cast<void>(bind_address);
#endif
}

void Os2lTcpServer::refresh_discovery() noexcept {
#ifdef _WIN32
    if (discovery_registration_ != nullptr) {
        const auto* registration =
            static_cast<const WindowsDnsSdRegistration*>(discovery_registration_);
        discovery_state_ = registration->state.load(std::memory_order_acquire);
        discovery_last_error_ = registration->last_error.load(std::memory_order_relaxed);
    }
#endif
}

void Os2lTcpServer::stop_discovery() noexcept {
#ifdef _WIN32
    auto* registration =
        static_cast<WindowsDnsSdRegistration*>(discovery_registration_);
    discovery_registration_ = nullptr;
    if (registration != nullptr) {
        registration->closing.store(true, std::memory_order_release);
        const auto state = registration->state.load(std::memory_order_acquire);
        if (state == Os2lDiscoveryState::Advertised) {
            begin_deregister(registration);
        } else if (state == Os2lDiscoveryState::Starting) {
            static_cast<void>(::DnsServiceRegisterCancel(&registration->cancel));
        }
        release_registration(registration);
    }
#endif
    discovery_state_ = Os2lDiscoveryState::Unavailable;
    discovery_last_error_ = 0;
}

void Os2lTcpServer::disconnect_client(bool error) noexcept {
    if (client_ >= 0) {
        close_native_socket(native_socket(client_));
        client_ = -1;
        ++stats_.disconnects;
        if (error) {
            ++stats_.client_errors;
        }
    }
    decoder_.reset();
    if (listener_ >= 0) {
        state_ = Os2lServerState::Listening;
    }
}

void Os2lTcpServer::close() noexcept {
    stop_discovery();
    disconnect_client(false);
    if (listener_ >= 0) {
        close_native_socket(native_socket(listener_));
        listener_ = -1;
    }
    decoder_.reset();
    bound_port_ = 0;
#ifdef _WIN32
    if (winsock_started_) {
        ::WSACleanup();
        winsock_started_ = false;
    }
#endif
    state_ = Os2lServerState::Closed;
}

Os2lPollResult Os2lTcpServer::poll(
    Os2lStreamCallback callback,
    void* context,
    std::uint32_t timeout_ms) noexcept {
    refresh_discovery();
    if (listener_ < 0 || state_ == Os2lServerState::Closed ||
        state_ == Os2lServerState::Fault) {
        return Os2lPollResult::Error;
    }

    const auto socket = client_ >= 0 ? native_socket(client_) : native_socket(listener_);
    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(socket, &read_set);
    timeval timeout{};
    timeout.tv_sec = static_cast<long>(timeout_ms / 1000U);
    timeout.tv_usec = static_cast<long>((timeout_ms % 1000U) * 1000U);

#ifdef _WIN32
    const auto selected = ::select(0, &read_set, nullptr, nullptr, &timeout);
#else
    const auto selected = ::select(socket + 1, &read_set, nullptr, nullptr, &timeout);
#endif
    if (selected == 0) {
        return Os2lPollResult::Idle;
    }
    if (selected < 0) {
        last_error_ = network_error();
        if (recoverable_error(last_error_)) {
            return Os2lPollResult::Idle;
        }
        state_ = Os2lServerState::Fault;
        return Os2lPollResult::Error;
    }

    if (client_ < 0) {
        const auto accepted = ::accept(native_socket(listener_), nullptr, nullptr);
        if (accepted == kInvalidSocket) {
            last_error_ = network_error();
            if (recoverable_error(last_error_)) {
                return Os2lPollResult::Idle;
            }
            state_ = Os2lServerState::Fault;
            return Os2lPollResult::Error;
        }
        client_ = static_cast<std::intptr_t>(accepted);
        decoder_.reset();
        ++stats_.connections;
        state_ = Os2lServerState::ClientConnected;
        return Os2lPollResult::ClientConnected;
    }

    std::array<char, 2048> bytes{};
#ifdef _WIN32
    const auto received = ::recv(
        native_socket(client_),
        bytes.data(),
        static_cast<int>(bytes.size()),
        0);
#else
    const auto received = ::recv(native_socket(client_), bytes.data(), bytes.size(), 0);
#endif
    if (received == 0) {
        disconnect_client(false);
        return Os2lPollResult::ClientDisconnected;
    }
    if (received < 0) {
        last_error_ = network_error();
        if (recoverable_error(last_error_)) {
            return Os2lPollResult::Idle;
        }
        disconnect_client(true);
        return Os2lPollResult::ClientDisconnected;
    }

    const auto byte_count = static_cast<std::size_t>(received);
    stats_.bytes_received += byte_count;
    const auto result = decoder_.feed(
        std::string_view(bytes.data(), byte_count),
        callback,
        context);
    stats_.messages += result.messages;
    stats_.decode_errors += result.errors;
    return result.messages > 0 || result.errors > 0
        ? Os2lPollResult::EventsReceived
        : Os2lPollResult::Idle;
}

}  // namespace showcore
