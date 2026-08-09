#include "showcore/sacn.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string_view>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace showcore {
namespace {

#ifdef _WIN32
using NativeSocket = SOCKET;
constexpr NativeSocket kInvalidSocket = INVALID_SOCKET;
#else
using NativeSocket = int;
constexpr NativeSocket kInvalidSocket = -1;
#endif

void put_u16_be(std::uint8_t* destination, std::uint16_t value) noexcept {
    destination[0] = static_cast<std::uint8_t>(value >> 8U);
    destination[1] = static_cast<std::uint8_t>(value & 0xFFU);
}

void put_u32_be(std::uint8_t* destination, std::uint32_t value) noexcept {
    destination[0] = static_cast<std::uint8_t>(value >> 24U);
    destination[1] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    destination[2] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    destination[3] = static_cast<std::uint8_t>(value & 0xFFU);
}

void put_flags_length(std::uint8_t* destination, std::size_t length) noexcept {
    put_u16_be(destination, static_cast<std::uint16_t>(0x7000U | (length & 0x0FFFU)));
}

void close_socket(NativeSocket socket) noexcept {
#ifdef _WIN32
    ::closesocket(socket);
#else
    ::close(socket);
#endif
}

}  // namespace

SacnCid make_sacn_cid(std::string_view stable_identity) noexcept {
    std::uint64_t first = 14695981039346656037ULL;
    std::uint64_t second = 1099511628211ULL;
    for (const auto character : stable_identity) {
        const auto byte = static_cast<std::uint8_t>(character);
        first = (first ^ byte) * 1099511628211ULL;
        second = (second ^ static_cast<std::uint8_t>(byte + 0x9DU)) *
            14695981039346656037ULL;
    }
    SacnCid cid{};
    for (std::size_t index = 0; index < 8U; ++index) {
        cid[index] = static_cast<std::uint8_t>(first >> ((7U - index) * 8U));
        cid[index + 8U] = static_cast<std::uint8_t>(second >> ((7U - index) * 8U));
    }
    cid[6] = static_cast<std::uint8_t>((cid[6] & 0x0FU) | 0x40U);
    cid[8] = static_cast<std::uint8_t>((cid[8] & 0x3FU) | 0x80U);
    return cid;
}

SacnDataPacket build_sacn_data_packet(
    const DmxUniverse& universe_data,
    std::uint16_t universe,
    std::uint8_t sequence,
    const SacnCid& cid,
    std::string_view source_name,
    std::uint8_t priority,
    std::uint16_t slot_count) noexcept {
    SacnDataPacket packet;
    if (universe == 0U || universe > 63999U || slot_count == 0U ||
        slot_count > kUniverseSlots || source_name.empty()) {
        return packet;
    }
    packet.length = kSacnHeaderSize + slot_count;
    auto* bytes = packet.bytes.data();
    put_u16_be(bytes + 0, 0x0010U);
    put_u16_be(bytes + 2, 0U);
    constexpr std::array<std::uint8_t, 12> identifier{{
        'A', 'S', 'C', '-', 'E', '1', '.', '1', '7', 0, 0, 0}};
    std::copy(identifier.begin(), identifier.end(), bytes + 4);

    put_flags_length(bytes + 16, packet.length - 16U);
    put_u32_be(bytes + 18, 0x00000004U);
    std::copy(cid.begin(), cid.end(), bytes + 22);

    put_flags_length(bytes + 38, packet.length - 38U);
    put_u32_be(bytes + 40, 0x00000002U);
    const auto source_length = std::min<std::size_t>(source_name.size(), 63U);
    std::copy_n(source_name.begin(), source_length, bytes + 44);
    bytes[108] = priority;
    put_u16_be(bytes + 109, 0U);
    bytes[111] = sequence;
    bytes[112] = 0U;
    put_u16_be(bytes + 113, universe);

    put_flags_length(bytes + 115, packet.length - 115U);
    bytes[117] = 0x02U;
    bytes[118] = 0xA1U;
    put_u16_be(bytes + 119, 0U);
    put_u16_be(bytes + 121, 1U);
    put_u16_be(bytes + 123, static_cast<std::uint16_t>(slot_count + 1U));
    bytes[125] = 0U;
    std::copy_n(universe_data.begin(), slot_count, bytes + kSacnHeaderSize);
    return packet;
}

std::array<char, 16> sacn_multicast_address(std::uint16_t universe) noexcept {
    std::array<char, 16> address{};
    if (universe == 0U || universe > 63999U) {
        return address;
    }
    constexpr std::string_view prefix = "239.255.";
    std::copy(prefix.begin(), prefix.end(), address.begin());
    auto cursor = address.data() + prefix.size();
    const auto end = address.data() + address.size() - 1U;
    const auto high = std::to_chars(cursor, end, static_cast<unsigned int>(universe >> 8U));
    if (high.ec != std::errc{} || high.ptr >= end) {
        return {};
    }
    *high.ptr = '.';
    const auto low = std::to_chars(
        high.ptr + 1U, end, static_cast<unsigned int>(universe & 0xFFU));
    if (low.ec != std::errc{} || low.ptr > end) {
        return {};
    }
    return address;
}

SacnSender::~SacnSender() noexcept {
    close();
}

bool SacnSender::open_ipv4(std::string_view address, std::uint16_t port) noexcept {
    close();
    if (address.empty() || address.size() >= 64U || port == 0U) {
        return false;
    }
#ifdef _WIN32
    WSADATA data{};
    if (::WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        return false;
    }
    winsock_started_ = true;
#endif
    const auto socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket == kInvalidSocket) {
        close();
        return false;
    }
    std::array<char, 64> terminated{};
    std::copy(address.begin(), address.end(), terminated.begin());
    in_addr parsed{};
    if (::inet_pton(AF_INET, terminated.data(), &parsed) != 1) {
        close_socket(socket);
        close();
        return false;
    }
    socket_handle_ = static_cast<std::intptr_t>(socket);
    address_network_order_ = parsed.s_addr;
    port_ = port;
    return true;
}

bool SacnSender::open_multicast(std::uint16_t universe, std::uint16_t port) noexcept {
    const auto address = sacn_multicast_address(universe);
    return address[0] != '\0' && open_ipv4(address.data(), port);
}

void SacnSender::close() noexcept {
    if (socket_handle_ != -1) {
        close_socket(static_cast<NativeSocket>(socket_handle_));
        socket_handle_ = -1;
    }
#ifdef _WIN32
    if (winsock_started_) {
        ::WSACleanup();
        winsock_started_ = false;
    }
#endif
    address_network_order_ = 0;
}

bool SacnSender::is_open() const noexcept {
    return socket_handle_ != -1;
}

bool SacnSender::send(const SacnDataPacket& packet) noexcept {
    if (!is_open() || packet.length == 0U || packet.length > packet.bytes.size()) {
        return false;
    }
    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(port_);
    destination.sin_addr.s_addr = address_network_order_;
#ifdef _WIN32
    const auto sent = ::sendto(
        static_cast<NativeSocket>(socket_handle_),
        reinterpret_cast<const char*>(packet.bytes.data()),
        static_cast<int>(packet.length),
        0,
        reinterpret_cast<const sockaddr*>(&destination),
        static_cast<int>(sizeof(destination)));
#else
    const auto sent = ::sendto(
        static_cast<NativeSocket>(socket_handle_),
        packet.bytes.data(),
        packet.length,
        0,
        reinterpret_cast<const sockaddr*>(&destination),
        sizeof(destination));
#endif
    return sent >= 0 && static_cast<std::size_t>(sent) == packet.length;
}

}  // namespace showcore
