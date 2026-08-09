#pragma once

#include "showcore/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace showcore {

inline constexpr std::uint16_t kSacnPort = 5568;
inline constexpr std::size_t kSacnHeaderSize = 126;
inline constexpr std::size_t kMaxSacnPacketSize = kSacnHeaderSize + kUniverseSlots;

using SacnCid = std::array<std::uint8_t, 16>;

struct SacnDataPacket {
    std::array<std::uint8_t, kMaxSacnPacketSize> bytes{};
    std::size_t length{0};
};

[[nodiscard]] SacnCid make_sacn_cid(std::string_view stable_identity) noexcept;
[[nodiscard]] SacnDataPacket build_sacn_data_packet(
    const DmxUniverse& universe_data,
    std::uint16_t universe,
    std::uint8_t sequence,
    const SacnCid& cid,
    std::string_view source_name,
    std::uint8_t priority = 100,
    std::uint16_t slot_count = static_cast<std::uint16_t>(kUniverseSlots)) noexcept;

[[nodiscard]] std::array<char, 16> sacn_multicast_address(
    std::uint16_t universe) noexcept;

class SacnSender {
public:
    SacnSender() noexcept = default;
    ~SacnSender() noexcept;

    SacnSender(const SacnSender&) = delete;
    SacnSender& operator=(const SacnSender&) = delete;

    [[nodiscard]] bool open_ipv4(
        std::string_view address,
        std::uint16_t port = kSacnPort) noexcept;
    [[nodiscard]] bool open_multicast(
        std::uint16_t universe,
        std::uint16_t port = kSacnPort) noexcept;
    void close() noexcept;
    [[nodiscard]] bool is_open() const noexcept;
    [[nodiscard]] bool send(const SacnDataPacket& packet) noexcept;

private:
    std::intptr_t socket_handle_{-1};
    std::uint32_t address_network_order_{0};
    std::uint16_t port_{kSacnPort};
#ifdef _WIN32
    bool winsock_started_{false};
#endif
};

}  // namespace showcore
