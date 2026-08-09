#include "showcore/dmx_usb_pro.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <string_view>
#include <system_error>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace showcore {
namespace {

[[nodiscard]] constexpr bool ascii_equal_fold(char first, char second) noexcept {
    const auto fold = [](char value) constexpr noexcept {
        return value >= 'a' && value <= 'z'
            ? static_cast<char>(value - ('a' - 'A'))
            : value;
    };
    return fold(first) == fold(second);
}

#ifdef _WIN32
void store_port_name(
    std::uint16_t port_number,
    DmxSerialPortInfo& destination) noexcept {
    destination.name_bytes.fill('\0');
    destination.name_bytes[0] = 'C';
    destination.name_bytes[1] = 'O';
    destination.name_bytes[2] = 'M';
    const auto result = std::to_chars(
        destination.name_bytes.data() + 3,
        destination.name_bytes.data() + destination.name_bytes.size(),
        port_number);
    destination.name_length = result.ec == std::errc{}
        ? static_cast<std::size_t>(result.ptr - destination.name_bytes.data())
        : 0U;
}
#endif

}  // namespace

DmxUsbProPacket build_dmx_usb_pro_packet(const DmxUniverse& universe) noexcept {
    DmxUsbProPacket packet;
    packet.bytes[0] = kDmxUsbProStartByte;
    packet.bytes[1] = kDmxUsbProSendDmxLabel;
    packet.bytes[2] = static_cast<std::uint8_t>(kDmxUsbProPayloadSize & 0xFFU);
    packet.bytes[3] = static_cast<std::uint8_t>((kDmxUsbProPayloadSize >> 8U) & 0xFFU);
    packet.bytes[4] = 0U;
    std::copy(universe.begin(), universe.end(), packet.bytes.begin() + 5);
    packet.bytes.back() = kDmxUsbProEndByte;
    return packet;
}

bool parse_windows_com_port(
    std::string_view name,
    std::uint16_t& port_number) noexcept {
    if (name.size() < 4U || name.size() >= kDmxSerialPortNameCapacity ||
        !ascii_equal_fold(name[0], 'C') || !ascii_equal_fold(name[1], 'O') ||
        !ascii_equal_fold(name[2], 'M')) {
        return false;
    }
    std::uint16_t parsed = 0;
    const auto result = std::from_chars(name.data() + 3, name.data() + name.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != name.data() + name.size() ||
        parsed == 0U || parsed > 256U) {
        return false;
    }
    port_number = parsed;
    return true;
}

#ifdef _WIN32

struct DmxUsbProSender::Impl {
    HANDLE handle{INVALID_HANDLE_VALUE};
    std::array<char, kDmxSerialPortNameCapacity> port{};
    std::size_t port_length{0};
    std::uint32_t last_error{ERROR_SUCCESS};
};

DmxSerialPortList enumerate_dmx_serial_ports() noexcept {
    DmxSerialPortList list;
    std::array<char, 1024> target{};
    for (std::uint16_t number = 1U; number <= 256U; ++number) {
        DmxSerialPortInfo candidate;
        store_port_name(number, candidate);
        if (candidate.name_length == 0U ||
            ::QueryDosDeviceA(candidate.name_bytes.data(), target.data(),
                              static_cast<DWORD>(target.size())) == 0U) {
            continue;
        }
        if (list.count >= list.ports.size()) {
            list.truncated = true;
            break;
        }
        list.ports[list.count++] = candidate;
    }
    return list;
}

DmxUsbProSender::DmxUsbProSender() noexcept : impl_(new (std::nothrow) Impl{}) {}

DmxUsbProSender::~DmxUsbProSender() noexcept {
    close();
}

bool DmxUsbProSender::supported() noexcept {
    return true;
}

bool DmxUsbProSender::open(std::string_view port_name) noexcept {
    if (impl_ == nullptr || is_open()) {
        return false;
    }
    std::uint16_t port_number = 0;
    if (!parse_windows_com_port(port_name, port_number)) {
        impl_->last_error = ERROR_INVALID_NAME;
        return false;
    }

    std::array<char, 24> device_path{};
    constexpr std::string_view prefix = "\\\\.\\COM";
    std::copy(prefix.begin(), prefix.end(), device_path.begin());
    const auto number = std::to_chars(
        device_path.data() + prefix.size(),
        device_path.data() + device_path.size() - 1U,
        port_number);
    if (number.ec != std::errc{}) {
        impl_->last_error = ERROR_INVALID_NAME;
        return false;
    }
    *number.ptr = '\0';

    const auto handle = ::CreateFileA(
        device_path.data(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        impl_->last_error = ::GetLastError();
        return false;
    }

    DCB configuration{};
    configuration.DCBlength = static_cast<DWORD>(sizeof(configuration));
    if (::GetCommState(handle, &configuration) == FALSE) {
        impl_->last_error = ::GetLastError();
        ::CloseHandle(handle);
        return false;
    }
    configuration.BaudRate = CBR_115200;
    configuration.ByteSize = 8U;
    configuration.Parity = NOPARITY;
    configuration.StopBits = ONESTOPBIT;
    configuration.fBinary = TRUE;
    configuration.fParity = FALSE;
    configuration.fOutxCtsFlow = FALSE;
    configuration.fOutxDsrFlow = FALSE;
    configuration.fDtrControl = DTR_CONTROL_DISABLE;
    configuration.fDsrSensitivity = FALSE;
    configuration.fTXContinueOnXoff = TRUE;
    configuration.fOutX = FALSE;
    configuration.fInX = FALSE;
    configuration.fErrorChar = FALSE;
    configuration.fNull = FALSE;
    configuration.fRtsControl = RTS_CONTROL_DISABLE;
    configuration.fAbortOnError = FALSE;
    if (::SetCommState(handle, &configuration) == FALSE) {
        impl_->last_error = ::GetLastError();
        ::CloseHandle(handle);
        return false;
    }

    COMMTIMEOUTS timeouts{};
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier = 0U;
    timeouts.ReadTotalTimeoutConstant = 0U;
    timeouts.WriteTotalTimeoutMultiplier = 0U;
    timeouts.WriteTotalTimeoutConstant = 50U;
    if (::SetCommTimeouts(handle, &timeouts) == FALSE ||
        ::SetupComm(handle, 4096U, 4096U) == FALSE) {
        impl_->last_error = ::GetLastError();
        ::CloseHandle(handle);
        return false;
    }
    static_cast<void>(::PurgeComm(handle, PURGE_RXABORT | PURGE_RXCLEAR |
        PURGE_TXABORT | PURGE_TXCLEAR));

    impl_->handle = handle;
    impl_->port.fill('\0');
    impl_->port[0] = 'C';
    impl_->port[1] = 'O';
    impl_->port[2] = 'M';
    const auto stored = std::to_chars(
        impl_->port.data() + 3,
        impl_->port.data() + impl_->port.size(),
        port_number);
    impl_->port_length = stored.ec == std::errc{}
        ? static_cast<std::size_t>(stored.ptr - impl_->port.data())
        : 0U;
    impl_->last_error = ERROR_SUCCESS;
    return true;
}

void DmxUsbProSender::close() noexcept {
    if (impl_ == nullptr || impl_->handle == INVALID_HANDLE_VALUE) {
        return;
    }
    static_cast<void>(::PurgeComm(
        impl_->handle, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR));
    static_cast<void>(::CloseHandle(impl_->handle));
    impl_->handle = INVALID_HANDLE_VALUE;
    impl_->port.fill('\0');
    impl_->port_length = 0U;
}

bool DmxUsbProSender::is_open() const noexcept {
    return impl_ != nullptr && impl_->handle != INVALID_HANDLE_VALUE;
}

bool DmxUsbProSender::send(const DmxUniverse& universe) noexcept {
    if (!is_open()) {
        return false;
    }
    const auto packet = build_dmx_usb_pro_packet(universe);
    DWORD written = 0U;
    if (::WriteFile(
            impl_->handle,
            packet.bytes.data(),
            static_cast<DWORD>(packet.bytes.size()),
            &written,
            nullptr) == FALSE ||
        static_cast<std::size_t>(written) != packet.bytes.size()) {
        const auto error = ::GetLastError();
        impl_->last_error = error == ERROR_SUCCESS ? ERROR_WRITE_FAULT : error;
        return false;
    }
    impl_->last_error = ERROR_SUCCESS;
    return true;
}

std::uint32_t DmxUsbProSender::last_error() const noexcept {
    return impl_ == nullptr ? ERROR_NOT_ENOUGH_MEMORY : impl_->last_error;
}

std::string_view DmxUsbProSender::port_name() const noexcept {
    return impl_ == nullptr
        ? std::string_view{}
        : std::string_view{impl_->port.data(), impl_->port_length};
}

#else

struct DmxUsbProSender::Impl {
    std::uint32_t last_error{0U};
};

DmxSerialPortList enumerate_dmx_serial_ports() noexcept {
    return {};
}

DmxUsbProSender::DmxUsbProSender() noexcept : impl_(new (std::nothrow) Impl{}) {}
DmxUsbProSender::~DmxUsbProSender() noexcept = default;

bool DmxUsbProSender::supported() noexcept { return false; }
bool DmxUsbProSender::open(std::string_view) noexcept { return false; }
void DmxUsbProSender::close() noexcept {}
bool DmxUsbProSender::is_open() const noexcept { return false; }
bool DmxUsbProSender::send(const DmxUniverse&) noexcept { return false; }
std::uint32_t DmxUsbProSender::last_error() const noexcept {
    return impl_ == nullptr ? 1U : impl_->last_error;
}
std::string_view DmxUsbProSender::port_name() const noexcept { return {}; }

#endif

}  // namespace showcore
