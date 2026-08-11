#include "showcore/soundswitch_micro.hpp"

#include <algorithm>
#include <chrono>
#include <limits>
#include <new>
#include <thread>

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

const char* soundswitch_micro_lifecycle_name(
    SoundSwitchMicroLifecycleState state) noexcept {
    switch (state) {
    case SoundSwitchMicroLifecycleState::Disabled: return "disabled";
    case SoundSwitchMicroLifecycleState::Detecting: return "detecting";
    case SoundSwitchMicroLifecycleState::Opening: return "opening";
    case SoundSwitchMicroLifecycleState::Inspecting: return "inspecting";
    case SoundSwitchMicroLifecycleState::Initializing: return "initializing";
    case SoundSwitchMicroLifecycleState::Settling: return "settling";
    case SoundSwitchMicroLifecycleState::WarmingUp: return "warming-up";
    case SoundSwitchMicroLifecycleState::Streaming: return "streaming";
    case SoundSwitchMicroLifecycleState::Recovering: return "recovering";
    case SoundSwitchMicroLifecycleState::Fault: return "fault";
    case SoundSwitchMicroLifecycleState::Closing: return "closing";
    }
    return "unknown";
}

bool valid_soundswitch_micro_session_config(
    const SoundSwitchMicroSessionConfig& config) noexcept {
    return config.framing == SoundSwitchMicroFraming::NativeJls1 &&
        config.transfer_timeout.count() > 0 &&
        config.transfer_timeout.count() <= 30'000 &&
        config.settling_interval.count() >= 0 &&
        config.settling_interval.count() <= 5'000 &&
        config.frame_interval.count() > 0 &&
        config.frame_interval.count() <= 1'000 &&
        config.warmup_blackout_frames <= 400U &&
        config.close_blackout_frames <= 40U;
}

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

struct SoundSwitchMicroSession::Impl {
    HANDLE file{INVALID_HANDLE_VALUE};
    WINUSB_INTERFACE_HANDLE usb{nullptr};
    SoundSwitchMicroSessionConfig config{};
    SoundSwitchMicroSessionStatus status{};
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

SoundSwitchMicroSession::SoundSwitchMicroSession() noexcept
    : impl_(new (std::nothrow) Impl{}) {}

SoundSwitchMicroSession::~SoundSwitchMicroSession() noexcept {
    close();
}

bool SoundSwitchMicroSession::supported() noexcept {
    return true;
}

bool SoundSwitchMicroSession::open(SoundSwitchMicroFraming framing) noexcept {
    SoundSwitchMicroSessionConfig config;
    config.framing = framing;
    return open(config);
}

bool SoundSwitchMicroSession::open(
    const SoundSwitchMicroSessionConfig& config) noexcept {
    close();
    if (impl_ == nullptr) {
        return false;
    }
    const auto previous_status = impl_->status;
    impl_->config = config;
    impl_->status = {};
    impl_->status.initialization_attempts = previous_status.initialization_attempts + 1U;
    impl_->status.initialization_successes = previous_status.initialization_successes;
    impl_->status.initialization_failures = previous_status.initialization_failures;
    impl_->status.reconnect_count = previous_status.reconnect_count +
        (previous_status.initialization_attempts == 0U ? 0U : 1U);
    impl_->status.state = SoundSwitchMicroLifecycleState::Detecting;
    if (!valid_soundswitch_micro_session_config(config)) {
        impl_->status.last_error = ERROR_INVALID_PARAMETER;
        ++impl_->status.initialization_failures;
        impl_->status.state = SoundSwitchMicroLifecycleState::Fault;
        return false;
    }

    auto release_handles = [&]() noexcept {
        if (impl_->usb != nullptr) {
            ::WinUsb_Free(impl_->usb);
            impl_->usb = nullptr;
        }
        if (impl_->file != INVALID_HANDLE_VALUE) {
            static_cast<void>(::CloseHandle(impl_->file));
            impl_->file = INVALID_HANDLE_VALUE;
        }
        impl_->status.handle_open = false;
    };
    auto fail = [&](std::uint32_t error) noexcept {
        impl_->status.last_error = error;
        ++impl_->status.initialization_failures;
        impl_->status.state = SoundSwitchMicroLifecycleState::Fault;
        release_handles();
        return false;
    };

    const auto path = find_interface_path();
    if (path.empty()) {
        return fail(ERROR_DEVICE_NOT_CONNECTED);
    }
    impl_->status.device_present = true;
    impl_->status.state = SoundSwitchMicroLifecycleState::Opening;
    impl_->file = ::CreateFileW(
        path.c_str(), GENERIC_READ | GENERIC_WRITE,
        0U, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
    if (impl_->file == INVALID_HANDLE_VALUE) {
        return fail(::GetLastError());
    }
    if (::WinUsb_Initialize(impl_->file, &impl_->usb) == FALSE) {
        return fail(::GetLastError());
    }
    impl_->status.handle_open = true;
    impl_->status.state = SoundSwitchMicroLifecycleState::Inspecting;

    USB_DEVICE_DESCRIPTOR device_descriptor{};
    ULONG descriptor_length = 0U;
    if (::WinUsb_GetDescriptor(
            impl_->usb, USB_DEVICE_DESCRIPTOR_TYPE, 0U, 0U,
            reinterpret_cast<PUCHAR>(&device_descriptor),
            static_cast<ULONG>(sizeof(device_descriptor)), &descriptor_length) == FALSE ||
        descriptor_length != static_cast<ULONG>(sizeof(device_descriptor)) ||
        device_descriptor.idVendor != 0x15E4U ||
        device_descriptor.idProduct != 0x0053U) {
        const auto error = ::GetLastError();
        return fail(error == ERROR_SUCCESS ? ERROR_BAD_DEVICE : error);
    }

    USB_CONFIGURATION_DESCRIPTOR configuration_descriptor{};
    descriptor_length = 0U;
    if (::WinUsb_GetDescriptor(
            impl_->usb, USB_CONFIGURATION_DESCRIPTOR_TYPE, 0U, 0U,
            reinterpret_cast<PUCHAR>(&configuration_descriptor),
            static_cast<ULONG>(sizeof(configuration_descriptor)), &descriptor_length) == FALSE ||
        descriptor_length < static_cast<ULONG>(sizeof(configuration_descriptor)) ||
        configuration_descriptor.bConfigurationValue != 1U) {
        const auto error = ::GetLastError();
        return fail(error == ERROR_SUCCESS ? ERROR_BAD_DEVICE : error);
    }
    impl_->status.configuration_value = configuration_descriptor.bConfigurationValue;

    USB_INTERFACE_DESCRIPTOR descriptor{};
    if (::WinUsb_QueryInterfaceSettings(impl_->usb, 0U, &descriptor) == FALSE) {
        return fail(::GetLastError());
    }
    impl_->status.interface_number = descriptor.bInterfaceNumber;
    UCHAR alternate_setting = 0U;
    if (::WinUsb_GetCurrentAlternateSetting(impl_->usb, &alternate_setting) == FALSE) {
        return fail(::GetLastError());
    }
    if (alternate_setting != 0U &&
        ::WinUsb_SetCurrentAlternateSetting(impl_->usb, 0U) == FALSE) {
        return fail(::GetLastError());
    }
    if (::WinUsb_GetCurrentAlternateSetting(impl_->usb, &alternate_setting) == FALSE ||
        alternate_setting != 0U) {
        const auto error = ::GetLastError();
        return fail(error == ERROR_SUCCESS ? ERROR_BAD_DEVICE : error);
    }
    impl_->status.alternate_setting = alternate_setting;

    bool valid_pipe = false;
    for (UCHAR index = 0U; index < descriptor.bNumEndpoints; ++index) {
        WINUSB_PIPE_INFORMATION pipe{};
        if (::WinUsb_QueryPipe(impl_->usb, 0U, index, &pipe) != FALSE &&
            pipe.PipeId == kSoundSwitchMicroBulkOutPipe &&
            pipe.PipeType == UsbdPipeTypeBulk && pipe.MaximumPacketSize == 64U) {
            valid_pipe = true;
            impl_->status.bulk_out_pipe = pipe.PipeId;
            impl_->status.maximum_packet_size = pipe.MaximumPacketSize;
        }
    }
    if (!valid_pipe) {
        return fail(ERROR_BAD_DEVICE);
    }
    if (::WinUsb_ResetPipe(impl_->usb, kSoundSwitchMicroBulkOutPipe) == FALSE) {
        return fail(::GetLastError());
    }
    const auto timeout_count = config.transfer_timeout.count();
    ULONG timeout = static_cast<ULONG>(timeout_count);
    if (::WinUsb_SetPipePolicy(
            impl_->usb, kSoundSwitchMicroBulkOutPipe,
            PIPE_TRANSFER_TIMEOUT, sizeof(timeout), &timeout) == FALSE) {
        return fail(::GetLastError());
    }
    BOOL terminate = FALSE;
    if (::WinUsb_SetPipePolicy(
            impl_->usb, kSoundSwitchMicroBulkOutPipe,
            SHORT_PACKET_TERMINATE, sizeof(terminate), &terminate) == FALSE) {
        return fail(::GetLastError());
    }
    BOOL raw_io = FALSE;
    if (::WinUsb_SetPipePolicy(
            impl_->usb, kSoundSwitchMicroBulkOutPipe,
            RAW_IO, sizeof(raw_io), &raw_io) == FALSE) {
        return fail(::GetLastError());
    }

    impl_->status.state = SoundSwitchMicroLifecycleState::Initializing;
    for (const auto& packet : kSoundSwitchMicroInitializationPackets) {
        if (!write_exact(
                impl_->usb, packet.data(), packet.size(), impl_->status.last_error)) {
            return fail(impl_->status.last_error);
        }
    }

    impl_->status.state = SoundSwitchMicroLifecycleState::Settling;
    std::this_thread::sleep_for(config.settling_interval);
    impl_->status.state = SoundSwitchMicroLifecycleState::WarmingUp;
    DmxUniverse blackout{};
    const auto blackout_packet = build_soundswitch_micro_packet(blackout, config.framing);
    for (std::uint16_t frame = 0U; frame < config.warmup_blackout_frames; ++frame) {
        ++impl_->status.frames_attempted;
        if (!write_exact(
                impl_->usb, blackout_packet.bytes.data(), blackout_packet.length,
                impl_->status.last_error)) {
            ++impl_->status.frames_failed;
            return fail(impl_->status.last_error);
        }
        ++impl_->status.frames_accepted;
        ++impl_->status.warmup_frames_completed;
        if (frame + 1U < config.warmup_blackout_frames) {
            std::this_thread::sleep_for(config.frame_interval);
        }
    }
    impl_->status.warmup_complete = true;
    ++impl_->status.initialization_successes;
    impl_->status.last_error = ERROR_SUCCESS;
    impl_->status.state = SoundSwitchMicroLifecycleState::Streaming;
    return true;
}

void SoundSwitchMicroSession::close() noexcept {
    if (impl_ == nullptr) {
        return;
    }
    if (impl_->usb != nullptr) {
        impl_->status.state = SoundSwitchMicroLifecycleState::Closing;
        static_cast<void>(send_blackout(
            impl_->config.close_blackout_frames,
            impl_->config.frame_interval));
    }
    if (impl_->usb != nullptr) {
        ::WinUsb_Free(impl_->usb);
        impl_->usb = nullptr;
    }
    if (impl_->file != INVALID_HANDLE_VALUE) {
        static_cast<void>(::CloseHandle(impl_->file));
        impl_->file = INVALID_HANDLE_VALUE;
    }
    impl_->status.handle_open = false;
    impl_->status.state = SoundSwitchMicroLifecycleState::Disabled;
}

bool SoundSwitchMicroSession::is_open() const noexcept {
    return impl_ != nullptr && impl_->file != INVALID_HANDLE_VALUE && impl_->usb != nullptr;
}

bool SoundSwitchMicroSession::send(const DmxUniverse& universe) noexcept {
    if (!is_open()) {
        return false;
    }
    ++impl_->status.frames_attempted;
    const auto packet = build_soundswitch_micro_packet(universe, impl_->config.framing);
    if (!write_exact(
            impl_->usb, packet.bytes.data(), packet.length,
            impl_->status.last_error)) {
        ++impl_->status.frames_failed;
        impl_->status.state = SoundSwitchMicroLifecycleState::Fault;
        return false;
    }
    ++impl_->status.frames_accepted;
    return true;
}

bool SoundSwitchMicroSession::send_blackout(
    std::uint16_t repetitions,
    std::chrono::milliseconds interval) noexcept {
    if (!is_open() || interval.count() < 0) {
        return false;
    }
    DmxUniverse blackout{};
    for (std::uint16_t frame = 0U; frame < repetitions; ++frame) {
        if (!send(blackout)) {
            return false;
        }
        if (frame + 1U < repetitions) {
            std::this_thread::sleep_for(interval);
        }
    }
    return true;
}

std::uint32_t SoundSwitchMicroSession::last_error() const noexcept {
    return impl_ == nullptr ? ERROR_NOT_ENOUGH_MEMORY : impl_->status.last_error;
}

SoundSwitchMicroFraming SoundSwitchMicroSession::framing() const noexcept {
    return impl_ == nullptr
        ? SoundSwitchMicroFraming::NativeJls1
        : impl_->config.framing;
}

SoundSwitchMicroSessionStatus SoundSwitchMicroSession::status() const noexcept {
    return impl_ == nullptr ? SoundSwitchMicroSessionStatus{} : impl_->status;
}

#else

struct SoundSwitchMicroSession::Impl {
    SoundSwitchMicroSessionConfig config{};
    SoundSwitchMicroSessionStatus status{};
};

SoundSwitchMicroSession::SoundSwitchMicroSession() noexcept
    : impl_(new (std::nothrow) Impl{}) {}
SoundSwitchMicroSession::~SoundSwitchMicroSession() noexcept = default;
bool SoundSwitchMicroSession::supported() noexcept { return false; }
bool SoundSwitchMicroSession::open(SoundSwitchMicroFraming framing) noexcept {
    SoundSwitchMicroSessionConfig config;
    config.framing = framing;
    return open(config);
}
bool SoundSwitchMicroSession::open(const SoundSwitchMicroSessionConfig& config) noexcept {
    if (impl_ != nullptr) {
        impl_->config = config;
        impl_->status = {};
        impl_->status.state = SoundSwitchMicroLifecycleState::Fault;
    }
    return false;
}
void SoundSwitchMicroSession::close() noexcept {
    if (impl_ != nullptr) {
        impl_->status.state = SoundSwitchMicroLifecycleState::Disabled;
    }
}
bool SoundSwitchMicroSession::is_open() const noexcept { return false; }
bool SoundSwitchMicroSession::send(const DmxUniverse&) noexcept { return false; }
bool SoundSwitchMicroSession::send_blackout(
    std::uint16_t,
    std::chrono::milliseconds) noexcept {
    return false;
}
std::uint32_t SoundSwitchMicroSession::last_error() const noexcept {
    return impl_ == nullptr ? 0U : impl_->status.last_error;
}
SoundSwitchMicroFraming SoundSwitchMicroSession::framing() const noexcept {
    return impl_ == nullptr
        ? SoundSwitchMicroFraming::NativeJls1
        : impl_->config.framing;
}
SoundSwitchMicroSessionStatus SoundSwitchMicroSession::status() const noexcept {
    return impl_ == nullptr ? SoundSwitchMicroSessionStatus{} : impl_->status;
}

#endif

}  // namespace showcore
