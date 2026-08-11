#include "showcore/soundswitch_micro.hpp"

#include <algorithm>
#include <new>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setupapi.h>
#include <usb.h>
#include <winusb.h>

#include <array>
#include <cwctype>
#include <string>
#include <string_view>
#include <vector>
#endif

namespace showcore {

SoundSwitchMicroPacket build_soundswitch_micro_packet(
    const DmxUniverse& universe,
    SoundSwitchMicroFraming) noexcept {
    SoundSwitchMicroPacket packet;
    packet.length = kSoundSwitchMicroMaximumFrameSize;
    packet.bytes[0] = static_cast<std::uint8_t>('s');
    packet.bytes[1] = static_cast<std::uint8_t>('T');
    packet.bytes[2] = static_cast<std::uint8_t>('R');
    packet.bytes[3] = static_cast<std::uint8_t>('t');
    packet.bytes[4] = 0x01U;
    packet.bytes[5] = 0x00U;
    packet.bytes[6] = 0x02U;
    packet.bytes[7] = 0x02U;
    packet.bytes[8] = 0x00U;
    packet.bytes[9] = 0x00U;
    std::copy(universe.begin(), universe.end(), packet.bytes.begin() + 10);
    return packet;
}

#ifdef _WIN32
namespace {

inline constexpr GUID kSoundSwitchMicroInterfaceGuid{
    0xD1AC763B, 0x3888, 0x46C8,
    {0xAB, 0x2F, 0x99, 0xC0, 0x60, 0xEE, 0x05, 0x99}};
inline constexpr UCHAR kSoundSwitchMicroBulkOutPipe = 0x01U;

[[nodiscard]] bool target_path(std::wstring_view path) {
    std::wstring normalized(path);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](wchar_t value) {
        return static_cast<wchar_t>(::towupper(value));
    });
    return normalized.find(L"VID_15E4") != std::wstring::npos &&
        normalized.find(L"PID_0053") != std::wstring::npos;
}

[[nodiscard]] std::wstring find_interface_path() {
    const auto set = ::SetupDiGetClassDevsW(
        &kSoundSwitchMicroInterfaceGuid, nullptr, nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) {
        return {};
    }
    std::wstring result;
    for (DWORD index = 0U;; ++index) {
        SP_DEVICE_INTERFACE_DATA interface_data{};
        interface_data.cbSize = sizeof(interface_data);
        if (::SetupDiEnumDeviceInterfaces(
                set, nullptr, &kSoundSwitchMicroInterfaceGuid,
                index, &interface_data) == FALSE) {
            break;
        }
        DWORD required = 0U;
        static_cast<void>(::SetupDiGetDeviceInterfaceDetailW(
            set, &interface_data, nullptr, 0U, &required, nullptr));
        if (required < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W)) {
            continue;
        }
        std::vector<std::uint8_t> bytes(required, 0U);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(bytes.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (::SetupDiGetDeviceInterfaceDetailW(
                set, &interface_data, detail, required, nullptr, nullptr) != FALSE &&
            target_path(detail->DevicePath)) {
            result = detail->DevicePath;
            break;
        }
    }
    static_cast<void>(::SetupDiDestroyDeviceInfoList(set));
    return result;
}

}  // namespace

struct SoundSwitchMicroSender::Impl {
    HANDLE file{INVALID_HANDLE_VALUE};
    WINUSB_INTERFACE_HANDLE usb{nullptr};
    SoundSwitchMicroFraming framing{SoundSwitchMicroFraming::NativeJls1};
    std::uint32_t last_error{0U};
};

namespace {

[[nodiscard]] bool write_exact(
    WINUSB_INTERFACE_HANDLE usb,
    const std::uint8_t* bytes,
    std::size_t length,
    std::uint32_t& error) noexcept {
    ULONG transferred = 0U;
    if (::WinUsb_WritePipe(
            usb,
            kSoundSwitchMicroBulkOutPipe,
            const_cast<PUCHAR>(bytes),
            static_cast<ULONG>(length),
            &transferred,
            nullptr) == FALSE) {
        error = ::GetLastError();
        return false;
    }
    if (transferred != static_cast<ULONG>(length)) {
        error = ERROR_WRITE_FAULT;
        return false;
    }
    error = ERROR_SUCCESS;
    return true;
}

}  // namespace

SoundSwitchMicroSender::SoundSwitchMicroSender() noexcept
    : impl_(new (std::nothrow) Impl{}) {}

SoundSwitchMicroSender::~SoundSwitchMicroSender() noexcept {
    close();
}

bool SoundSwitchMicroSender::supported() noexcept {
    return true;
}

bool SoundSwitchMicroSender::open(SoundSwitchMicroFraming framing) noexcept {
    close();
    if (impl_ == nullptr) {
        return false;
    }
    impl_->framing = framing;
    const auto path = find_interface_path();
    if (path.empty()) {
        impl_->last_error = ERROR_DEVICE_NOT_CONNECTED;
        return false;
    }
    impl_->file = ::CreateFileW(
        path.c_str(), GENERIC_READ | GENERIC_WRITE,
        0U, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
    if (impl_->file == INVALID_HANDLE_VALUE) {
        impl_->last_error = ::GetLastError();
        return false;
    }
    if (::WinUsb_Initialize(impl_->file, &impl_->usb) == FALSE) {
        impl_->last_error = ::GetLastError();
        close();
        return false;
    }

    USB_INTERFACE_DESCRIPTOR descriptor{};
    if (::WinUsb_QueryInterfaceSettings(impl_->usb, 0U, &descriptor) == FALSE) {
        impl_->last_error = ::GetLastError();
        close();
        return false;
    }
    bool valid_pipe = false;
    for (UCHAR index = 0U; index < descriptor.bNumEndpoints; ++index) {
        WINUSB_PIPE_INFORMATION pipe{};
        if (::WinUsb_QueryPipe(impl_->usb, 0U, index, &pipe) != FALSE &&
            pipe.PipeId == kSoundSwitchMicroBulkOutPipe &&
            pipe.PipeType == UsbdPipeTypeBulk && pipe.MaximumPacketSize == 64U) {
            valid_pipe = true;
        }
    }
    if (!valid_pipe) {
        impl_->last_error = ERROR_BAD_DEVICE;
        close();
        return false;
    }
    ULONG timeout = 500U;
    if (::WinUsb_SetPipePolicy(
            impl_->usb, kSoundSwitchMicroBulkOutPipe,
            PIPE_TRANSFER_TIMEOUT, sizeof(timeout), &timeout) == FALSE) {
        impl_->last_error = ::GetLastError();
        close();
        return false;
    }
    BOOL terminate = FALSE;
    if (::WinUsb_SetPipePolicy(
            impl_->usb, kSoundSwitchMicroBulkOutPipe,
            SHORT_PACKET_TERMINATE, sizeof(terminate), &terminate) == FALSE) {
        impl_->last_error = ::GetLastError();
        close();
        return false;
    }
    for (const auto& packet : kSoundSwitchMicroInitializationPackets) {
        if (!write_exact(
                impl_->usb, packet.data(), packet.size(), impl_->last_error)) {
            close();
            return false;
        }
    }
    impl_->last_error = ERROR_SUCCESS;
    return true;
}

void SoundSwitchMicroSender::close() noexcept {
    if (impl_ == nullptr) {
        return;
    }
    if (impl_->usb != nullptr) {
        ::WinUsb_Free(impl_->usb);
        impl_->usb = nullptr;
    }
    if (impl_->file != INVALID_HANDLE_VALUE) {
        static_cast<void>(::CloseHandle(impl_->file));
        impl_->file = INVALID_HANDLE_VALUE;
    }
}

bool SoundSwitchMicroSender::is_open() const noexcept {
    return impl_ != nullptr && impl_->file != INVALID_HANDLE_VALUE && impl_->usb != nullptr;
}

bool SoundSwitchMicroSender::send(const DmxUniverse& universe) noexcept {
    if (!is_open()) {
        return false;
    }
    const auto packet = build_soundswitch_micro_packet(universe, impl_->framing);
    return write_exact(
        impl_->usb, packet.bytes.data(), packet.length, impl_->last_error);
}

std::uint32_t SoundSwitchMicroSender::last_error() const noexcept {
    return impl_ == nullptr ? ERROR_NOT_ENOUGH_MEMORY : impl_->last_error;
}

SoundSwitchMicroFraming SoundSwitchMicroSender::framing() const noexcept {
    return impl_ == nullptr
        ? SoundSwitchMicroFraming::NativeJls1
        : impl_->framing;
}

#else

struct SoundSwitchMicroSender::Impl {};

SoundSwitchMicroSender::SoundSwitchMicroSender() noexcept
    : impl_(new (std::nothrow) Impl{}) {}
SoundSwitchMicroSender::~SoundSwitchMicroSender() noexcept = default;
bool SoundSwitchMicroSender::supported() noexcept { return false; }
bool SoundSwitchMicroSender::open(SoundSwitchMicroFraming) noexcept { return false; }
void SoundSwitchMicroSender::close() noexcept {}
bool SoundSwitchMicroSender::is_open() const noexcept { return false; }
bool SoundSwitchMicroSender::send(const DmxUniverse&) noexcept { return false; }
std::uint32_t SoundSwitchMicroSender::last_error() const noexcept { return 0U; }
SoundSwitchMicroFraming SoundSwitchMicroSender::framing() const noexcept {
    return SoundSwitchMicroFraming::NativeJls1;
}

#endif

}  // namespace showcore
