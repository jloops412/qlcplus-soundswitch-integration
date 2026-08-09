#pragma once

#include "showcore/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace showcore {

inline constexpr std::uint16_t kArtNetPort = 6454;
inline constexpr std::size_t kArtDmxHeaderSize = 18;
inline constexpr std::size_t kMaxArtDmxPacketSize = kArtDmxHeaderSize + kUniverseSlots;

struct ArtDmxPacket {
    std::array<std::uint8_t, kMaxArtDmxPacketSize> bytes{};
    std::size_t length{0};
};

[[nodiscard]] ArtDmxPacket build_artdmx(
    const DmxUniverse& universe,
    std::uint16_t port_address,
    std::uint8_t sequence,
    std::uint16_t slot_count = static_cast<std::uint16_t>(kUniverseSlots)) noexcept;

class ArtNetSender {
public:
    ArtNetSender() noexcept = default;
    ~ArtNetSender() noexcept;

    ArtNetSender(const ArtNetSender&) = delete;
    ArtNetSender& operator=(const ArtNetSender&) = delete;

    [[nodiscard]] bool open_ipv4(std::string_view address, std::uint16_t port = kArtNetPort) noexcept;
    void close() noexcept;
    [[nodiscard]] bool is_open() const noexcept;
    [[nodiscard]] bool send(const ArtDmxPacket& packet) noexcept;

private:
    std::intptr_t socket_handle_{-1};
    std::uint32_t address_network_order_{0};
    std::uint16_t port_{kArtNetPort};
#ifdef _WIN32
    bool winsock_started_{false};
#endif
};

}  // namespace showcore
