#pragma once

#include "showcore/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

namespace showcore {

// ENTTEC's published DMX USB Pro application framing. This adapter implements
// only single-universe output label 6; it does not claim Pro Mk2 dual-port APIs.
inline constexpr std::uint8_t kDmxUsbProStartByte = 0x7EU;
inline constexpr std::uint8_t kDmxUsbProSendDmxLabel = 0x06U;
inline constexpr std::uint8_t kDmxUsbProEndByte = 0xE7U;
inline constexpr std::size_t kDmxUsbProPayloadSize = kUniverseSlots + 1U;
inline constexpr std::size_t kDmxUsbProPacketSize = kDmxUsbProPayloadSize + 5U;
inline constexpr std::size_t kMaxEnumeratedDmxSerialPorts = 64U;
inline constexpr std::size_t kDmxSerialPortNameCapacity = 16U;

struct DmxUsbProPacket {
    std::array<std::uint8_t, kDmxUsbProPacketSize> bytes{};
};

[[nodiscard]] DmxUsbProPacket build_dmx_usb_pro_packet(
    const DmxUniverse& universe) noexcept;

[[nodiscard]] bool parse_windows_com_port(
    std::string_view name,
    std::uint16_t& port_number) noexcept;

struct DmxSerialPortInfo {
    std::array<char, kDmxSerialPortNameCapacity> name_bytes{};
    std::size_t name_length{0};

    [[nodiscard]] std::string_view name() const noexcept {
        return {name_bytes.data(), name_length};
    }
};

struct DmxSerialPortList {
    std::array<DmxSerialPortInfo, kMaxEnumeratedDmxSerialPorts> ports{};
    std::size_t count{0};
    bool truncated{false};
};

[[nodiscard]] DmxSerialPortList enumerate_dmx_serial_ports() noexcept;

class DmxUsbProSender {
public:
    DmxUsbProSender() noexcept;
    ~DmxUsbProSender() noexcept;

    DmxUsbProSender(const DmxUsbProSender&) = delete;
    DmxUsbProSender& operator=(const DmxUsbProSender&) = delete;

    [[nodiscard]] static bool supported() noexcept;
    [[nodiscard]] bool open(std::string_view port_name) noexcept;
    void close() noexcept;
    [[nodiscard]] bool is_open() const noexcept;
    [[nodiscard]] bool send(const DmxUniverse& universe) noexcept;
    [[nodiscard]] std::uint32_t last_error() const noexcept;
    [[nodiscard]] std::string_view port_name() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace showcore
