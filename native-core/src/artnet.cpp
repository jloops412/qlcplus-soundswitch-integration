#include "showcore/artnet.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace showcore {

ArtDmxPacket build_artdmx(
    const DmxUniverse& universe,
    std::uint16_t port_address,
    std::uint8_t sequence,
    std::uint16_t slot_count) noexcept {
    ArtDmxPacket packet{};
    slot_count = std::clamp<std::uint16_t>(slot_count, 2U, static_cast<std::uint16_t>(kUniverseSlots));
    if ((slot_count & 1U) != 0U) {
        ++slot_count;
    }

    constexpr std::array<std::uint8_t, 8> identifier{'A', 'r', 't', '-', 'N', 'e', 't', 0U};
    std::copy(identifier.begin(), identifier.end(), packet.bytes.begin());
    packet.bytes[8] = 0x00U;
    packet.bytes[9] = 0x50U;
    packet.bytes[10] = 0x00U;
    packet.bytes[11] = 14U;
    packet.bytes[12] = sequence;
    packet.bytes[13] = 0U;
    packet.bytes[14] = static_cast<std::uint8_t>(port_address & 0xFFU);
    packet.bytes[15] = static_cast<std::uint8_t>((port_address >> 8U) & 0x7FU);
    packet.bytes[16] = static_cast<std::uint8_t>((slot_count >> 8U) & 0xFFU);
    packet.bytes[17] = static_cast<std::uint8_t>(slot_count & 0xFFU);
    std::copy_n(universe.begin(), slot_count, packet.bytes.begin() + kArtDmxHeaderSize);
    packet.length = kArtDmxHeaderSize + slot_count;
    return packet;
}

ArtNetSender::~ArtNetSender() noexcept {
    close();
}

bool ArtNetSender::open_ipv4(std::string_view address, std::uint16_t port) noexcept {
    close();
    if (address.empty() || address.size() >= 64U) {
        return false;
    }

    std::array<char, 64> address_buffer{};
    std::copy(address.begin(), address.end(), address_buffer.begin());

#ifdef _WIN32
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        return false;
    }
    winsock_started_ = true;
    const auto socket_value = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_value == INVALID_SOCKET) {
        close();
        return false;
    }
    socket_handle_ = static_cast<std::intptr_t>(socket_value);
#else
    const auto socket_value = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_value < 0) {
        return false;
    }
    socket_handle_ = static_cast<std::intptr_t>(socket_value);
#endif

    in_addr parsed{};
    if (::inet_pton(AF_INET, address_buffer.data(), &parsed) != 1) {
        close();
        return false;
    }
    address_network_order_ = parsed.s_addr;
    port_ = port;
    return true;
}

void ArtNetSender::close() noexcept {
    if (socket_handle_ >= 0) {
#ifdef _WIN32
        ::closesocket(static_cast<SOCKET>(socket_handle_));
#else
        ::close(static_cast<int>(socket_handle_));
#endif
    }
    socket_handle_ = -1;
    address_network_order_ = 0;
#ifdef _WIN32
    if (winsock_started_) {
        ::WSACleanup();
        winsock_started_ = false;
    }
#endif
}

bool ArtNetSender::is_open() const noexcept {
    return socket_handle_ >= 0;
}

bool ArtNetSender::send(const ArtDmxPacket& packet) noexcept {
    if (!is_open() || packet.length == 0 || packet.length > packet.bytes.size()) {
        return false;
    }

    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(port_);
    destination.sin_addr.s_addr = address_network_order_;

#ifdef _WIN32
    const auto sent = ::sendto(
        static_cast<SOCKET>(socket_handle_),
        reinterpret_cast<const char*>(packet.bytes.data()),
        static_cast<int>(packet.length),
        0,
        reinterpret_cast<const sockaddr*>(&destination),
        static_cast<int>(sizeof(destination)));
    return sent == static_cast<int>(packet.length);
#else
    const auto sent = ::sendto(
        static_cast<int>(socket_handle_),
        packet.bytes.data(),
        packet.length,
        0,
        reinterpret_cast<const sockaddr*>(&destination),
        sizeof(destination));
    return sent >= 0 && static_cast<std::size_t>(sent) == packet.length;
#endif
}

}  // namespace showcore
