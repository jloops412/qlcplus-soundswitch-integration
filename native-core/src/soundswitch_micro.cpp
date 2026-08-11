#include "showcore/soundswitch_micro.hpp"

#include "showcore/dmx_usb_pro.hpp"

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
    SoundSwitchMicroFraming framing) noexcept {
    SoundSwitchMicroPacket packet;
    switch (framing) {
    case SoundSwitchMicroFraming::RawDmxWithStartCode:
        packet.length = kSoundSwitchMicroRawFrameSize;
        packet.bytes[0] = 0U;
        std::copy(universe.begin(), universe.end(), packet.bytes.begin() + 1);
        break;
    case SoundSwitchMicroFraming::RawSlotsOnly:
        packet.length = kUniverseSlots;
        packet.terminate_with_short_packet = true;
        std::copy(universe.begin(), universe.end(), packet.bytes.begin());
        break;
    case SoundSwitchMicroFraming::EnttecUsbPro: {
        const auto enttec = build_dmx_usb_pro_packet(universe);
        packet.length = enttec.bytes.size();
        std::copy(enttec.bytes.begin(), enttec.bytes.end(), packet.bytes.begin());
        break;
    }
    }
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
    SoundSwitchMicroFraming framing{SoundSwitchMicroFraming::RawDmxWithStartCode};
    std::uint32_t last_error{0U};
};

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
    BOOL terminate = framing == SoundSwitchMicroFraming::RawSlotsOnly ? TRUE : FALSE;
    if (::WinUsb_SetPipePolicy(
            impl_->usb, kSoundSwitchMicroBulkOutPipe,
            SHORT_PACKET_TERMINATE, sizeof(terminate), &terminate) == FALSE) {
        impl_->last_error = ::GetLastError();
        close();
        return false;
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
    ULONG transferred = 0U;
    if (::WinUsb_WritePipe(
            impl_->usb, kSoundSwitchMicroBulkOutPipe,
            const_cast<PUCHAR>(packet.bytes.data()),
            static_cast<ULONG>(packet.length), &transferred, nullptr) == FALSE) {
        impl_->last_error = ::GetLastError();
        return false;
    }
    if (transferred != packet.length) {
        impl_->last_error = ERROR_WRITE_FAULT;
        return false;
    }
    impl_->last_error = ERROR_SUCCESS;
    return true;
}

std::uint32_t SoundSwitchMicroSender::last_error() const noexcept {
    return impl_ == nullptr ? ERROR_NOT_ENOUGH_MEMORY : impl_->last_error;
}

SoundSwitchMicroFraming SoundSwitchMicroSender::framing() const noexcept {
    return impl_ == nullptr
        ? SoundSwitchMicroFraming::RawDmxWithStartCode
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
    return SoundSwitchMicroFraming::RawDmxWithStartCode;
}

#endif

}  // namespace showcore
