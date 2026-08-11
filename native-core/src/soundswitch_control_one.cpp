#include "showcore/soundswitch_control_one.hpp"

#include <algorithm>
#include <new>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
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

const char* soundswitch_control_one_lifecycle_name(
    SoundSwitchControlOneLifecycleState state) noexcept {
    switch (state) {
    case SoundSwitchControlOneLifecycleState::Disabled: return "disabled";
    case SoundSwitchControlOneLifecycleState::Detecting: return "detecting";
    case SoundSwitchControlOneLifecycleState::Opening: return "opening";
    case SoundSwitchControlOneLifecycleState::Inspecting: return "inspecting";
    case SoundSwitchControlOneLifecycleState::Initializing: return "initializing";
    case SoundSwitchControlOneLifecycleState::WarmingUp: return "warming-up";
    case SoundSwitchControlOneLifecycleState::Streaming: return "streaming";
    case SoundSwitchControlOneLifecycleState::Fault: return "fault";
    case SoundSwitchControlOneLifecycleState::Closing: return "closing";
    }
    return "unknown";
}

bool valid_soundswitch_control_one_session_config(
    const SoundSwitchControlOneSessionConfig& config) noexcept {
    return config.transfer_timeout.count() > 0 &&
        config.transfer_timeout.count() <= 30'000 &&
        config.frame_interval.count() >= 10 &&
        config.frame_interval.count() <= 1'000 &&
        config.warmup_blackout_pairs <= 40U &&
        config.close_blackout_pairs <= 40U;
}

SoundSwitchControlOnePacket build_soundswitch_control_one_packet(
    SoundSwitchControlOnePort port,
    const DmxUniverse& universe) noexcept {
    SoundSwitchControlOnePacket packet;
    packet.length = kSoundSwitchControlOneFrameSize;
    packet.bytes[0] = static_cast<std::uint8_t>('s');
    packet.bytes[1] = static_cast<std::uint8_t>('T');
    packet.bytes[2] = static_cast<std::uint8_t>('R');
    packet.bytes[3] = static_cast<std::uint8_t>('t');
    packet.bytes[4] = 0x01U;
    packet.bytes[5] = 0x00U;
    packet.bytes[6] = 0x02U;
    packet.bytes[7] = 0x02U;
    packet.bytes[8] = static_cast<std::uint8_t>(port);
    packet.bytes[9] = 0x00U;
    std::copy(universe.begin(), universe.end(), packet.bytes.begin() + 10);
    return packet;
}

#ifdef _WIN32
namespace {

[[nodiscard]] std::wstring upper(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t character) {
        return static_cast<wchar_t>(::towupper(character));
    });
    return value;
}

[[nodiscard]] bool target_text(std::wstring_view value) {
    const auto normalized = upper(std::wstring(value));
    return normalized.find(L"VID_15E4") != std::wstring::npos &&
        normalized.find(L"PID_0054") != std::wstring::npos;
}

class DeviceInfoSet {
public:
    explicit DeviceInfoSet(HDEVINFO value) noexcept : value_(value) {}
    ~DeviceInfoSet() noexcept {
        if (value_ != INVALID_HANDLE_VALUE) {
            static_cast<void>(::SetupDiDestroyDeviceInfoList(value_));
        }
    }
    DeviceInfoSet(const DeviceInfoSet&) = delete;
    DeviceInfoSet& operator=(const DeviceInfoSet&) = delete;
    [[nodiscard]] HDEVINFO get() const noexcept { return value_; }
    [[nodiscard]] bool valid() const noexcept {
        return value_ != INVALID_HANDLE_VALUE;
    }

private:
    HDEVINFO value_{INVALID_HANDLE_VALUE};
};

class RegistryKey {
public:
    explicit RegistryKey(HKEY value = nullptr) noexcept : value_(value) {}
    ~RegistryKey() noexcept {
        if (value_ != nullptr && value_ != INVALID_HANDLE_VALUE) {
            static_cast<void>(::RegCloseKey(value_));
        }
    }
    RegistryKey(const RegistryKey&) = delete;
    RegistryKey& operator=(const RegistryKey&) = delete;
    [[nodiscard]] HKEY get() const noexcept { return value_; }
    [[nodiscard]] bool valid() const noexcept {
        return value_ != nullptr && value_ != INVALID_HANDLE_VALUE;
    }

private:
    HKEY value_{nullptr};
};

[[nodiscard]] std::wstring device_property(
    HDEVINFO set,
    SP_DEVINFO_DATA& device,
    DWORD property) {
    DWORD type = 0U;
    DWORD required = 0U;
    static_cast<void>(::SetupDiGetDeviceRegistryPropertyW(
        set, &device, property, &type, nullptr, 0U, &required));
    if (required == 0U) {
        return {};
    }
    std::vector<BYTE> bytes(required + sizeof(wchar_t), 0U);
    if (::SetupDiGetDeviceRegistryPropertyW(
            set, &device, property, &type, bytes.data(),
            static_cast<DWORD>(bytes.size()), nullptr) == FALSE) {
        return {};
    }
    return std::wstring(reinterpret_cast<const wchar_t*>(bytes.data()));
}

[[nodiscard]] std::vector<std::wstring> read_multi_string(
    HKEY key,
    const wchar_t* name) {
    DWORD type = 0U;
    DWORD bytes = 0U;
    if (::RegQueryValueExW(key, name, nullptr, &type, nullptr, &bytes) !=
            ERROR_SUCCESS ||
        (type != REG_MULTI_SZ && type != REG_SZ) || bytes == 0U) {
        return {};
    }
    std::vector<wchar_t> buffer(bytes / sizeof(wchar_t) + 2U, L'\0');
    if (::RegQueryValueExW(
            key, name, nullptr, &type,
            reinterpret_cast<BYTE*>(buffer.data()), &bytes) != ERROR_SUCCESS) {
        return {};
    }
    std::vector<std::wstring> values;
    for (const wchar_t* cursor = buffer.data(); *cursor != L'\0';) {
        std::wstring value(cursor);
        values.push_back(value);
        cursor += value.size() + 1U;
        if (type == REG_SZ) {
            break;
        }
    }
    return values;
}

[[nodiscard]] std::vector<GUID> find_interface_guids() {
    DeviceInfoSet set(::SetupDiGetClassDevsW(
        nullptr, nullptr, nullptr, DIGCF_PRESENT | DIGCF_ALLCLASSES));
    if (!set.valid()) {
        return {};
    }
    for (DWORD index = 0U;; ++index) {
        SP_DEVINFO_DATA device{};
        device.cbSize = sizeof(device);
        if (::SetupDiEnumDeviceInfo(set.get(), index, &device) == FALSE) {
            break;
        }
        if (!target_text(device_property(set.get(), device, SPDRP_HARDWAREID))) {
            continue;
        }
        RegistryKey device_key(::SetupDiOpenDevRegKey(
            set.get(), &device, DICS_FLAG_GLOBAL, 0U, DIREG_DEV, KEY_READ));
        if (!device_key.valid()) {
            return {};
        }
        auto strings = read_multi_string(device_key.get(), L"DeviceInterfaceGUIDs");
        if (strings.empty()) {
            HKEY parameters_raw = nullptr;
            if (::RegOpenKeyExW(
                    device_key.get(), L"Device Parameters", 0U, KEY_READ,
                    &parameters_raw) == ERROR_SUCCESS) {
                RegistryKey parameters(parameters_raw);
                strings = read_multi_string(
                    parameters.get(), L"DeviceInterfaceGUIDs");
            }
        }
        std::vector<GUID> result;
        for (const auto& text : strings) {
            GUID guid{};
            if (::CLSIDFromString(text.c_str(), &guid) == S_OK) {
                result.push_back(guid);
            }
        }
        return result;
    }
    return {};
}

[[nodiscard]] std::wstring interface_path_for_guid(const GUID& guid) {
    DeviceInfoSet set(::SetupDiGetClassDevsW(
        &guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));
    if (!set.valid()) {
        return {};
    }
    for (DWORD index = 0U;; ++index) {
        SP_DEVICE_INTERFACE_DATA interface_data{};
        interface_data.cbSize = sizeof(interface_data);
        if (::SetupDiEnumDeviceInterfaces(
                set.get(), nullptr, &guid, index, &interface_data) == FALSE) {
            break;
        }
        DWORD required = 0U;
        static_cast<void>(::SetupDiGetDeviceInterfaceDetailW(
            set.get(), &interface_data, nullptr, 0U, &required, nullptr));
        if (required < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W)) {
            continue;
        }
        std::vector<std::uint8_t> bytes(required, 0U);
        auto* detail =
            reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(bytes.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (::SetupDiGetDeviceInterfaceDetailW(
                set.get(), &interface_data, detail, required, nullptr, nullptr) !=
                FALSE &&
            target_text(detail->DevicePath)) {
            return detail->DevicePath;
        }
    }
    return {};
}

[[nodiscard]] std::wstring find_interface_path() {
    for (const auto& guid : find_interface_guids()) {
        auto path = interface_path_for_guid(guid);
        if (!path.empty()) {
            return path;
        }
    }
    return {};
}

[[nodiscard]] bool write_exact(
    WINUSB_INTERFACE_HANDLE usb,
    const std::uint8_t* bytes,
    std::size_t length,
    std::uint32_t& error) noexcept {
    ULONG transferred = 0U;
    if (::WinUsb_WritePipe(
            usb, kSoundSwitchControlOneBulkOutPipe,
            const_cast<PUCHAR>(bytes), static_cast<ULONG>(length),
            &transferred, nullptr) == FALSE) {
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

struct SoundSwitchControlOneSession::Impl {
    HANDLE file{INVALID_HANDLE_VALUE};
    WINUSB_INTERFACE_HANDLE usb{nullptr};
    SoundSwitchControlOneSessionConfig config{};
    SoundSwitchControlOneSessionStatus status{};
};

SoundSwitchControlOneSession::SoundSwitchControlOneSession() noexcept
    : impl_(new (std::nothrow) Impl{}) {}

SoundSwitchControlOneSession::~SoundSwitchControlOneSession() noexcept {
    close();
}

bool SoundSwitchControlOneSession::supported() noexcept { return true; }

bool SoundSwitchControlOneSession::open(
    const SoundSwitchControlOneSessionConfig& config) noexcept {
    close();
    if (impl_ == nullptr) {
        return false;
    }
    const auto previous = impl_->status;
    impl_->config = config;
    impl_->status = {};
    impl_->status.open_attempts = previous.open_attempts + 1U;
    impl_->status.open_successes = previous.open_successes;
    impl_->status.reconnect_count = previous.reconnect_count +
        (previous.open_successes == 0U ? 0U : 1U);
    impl_->status.state = SoundSwitchControlOneLifecycleState::Detecting;

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
        impl_->status.state = SoundSwitchControlOneLifecycleState::Fault;
        release_handles();
        return false;
    };

    if (!valid_soundswitch_control_one_session_config(config)) {
        return fail(ERROR_INVALID_PARAMETER);
    }
    const auto path = find_interface_path();
    if (path.empty()) {
        return fail(ERROR_DEVICE_NOT_CONNECTED);
    }
    impl_->status.device_present = true;
    impl_->status.state = SoundSwitchControlOneLifecycleState::Opening;
    impl_->file = ::CreateFileW(
        path.c_str(), GENERIC_READ | GENERIC_WRITE, 0U, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
    if (impl_->file == INVALID_HANDLE_VALUE) {
        return fail(::GetLastError());
    }
    if (::WinUsb_Initialize(impl_->file, &impl_->usb) == FALSE) {
        return fail(::GetLastError());
    }
    impl_->status.handle_open = true;
    impl_->status.state = SoundSwitchControlOneLifecycleState::Inspecting;

    USB_DEVICE_DESCRIPTOR device_descriptor{};
    ULONG descriptor_length = 0U;
    if (::WinUsb_GetDescriptor(
            impl_->usb, USB_DEVICE_DESCRIPTOR_TYPE, 0U, 0U,
            reinterpret_cast<PUCHAR>(&device_descriptor),
            static_cast<ULONG>(sizeof(device_descriptor)), &descriptor_length) ==
            FALSE ||
        descriptor_length != sizeof(device_descriptor) ||
        device_descriptor.idVendor != kSoundSwitchVendorId ||
        device_descriptor.idProduct != kSoundSwitchControlOneProductId) {
        const auto error = ::GetLastError();
        return fail(error == ERROR_SUCCESS ? ERROR_BAD_DEVICE : error);
    }

    USB_CONFIGURATION_DESCRIPTOR configuration_descriptor{};
    descriptor_length = 0U;
    if (::WinUsb_GetDescriptor(
            impl_->usb, USB_CONFIGURATION_DESCRIPTOR_TYPE, 0U, 0U,
            reinterpret_cast<PUCHAR>(&configuration_descriptor),
            static_cast<ULONG>(sizeof(configuration_descriptor)),
            &descriptor_length) == FALSE ||
        descriptor_length < sizeof(configuration_descriptor) ||
        configuration_descriptor.bConfigurationValue !=
            kSoundSwitchControlOneConfiguration) {
        const auto error = ::GetLastError();
        return fail(error == ERROR_SUCCESS ? ERROR_BAD_DEVICE : error);
    }
    impl_->status.configuration_value =
        configuration_descriptor.bConfigurationValue;

    USB_INTERFACE_DESCRIPTOR descriptor{};
    if (::WinUsb_QueryInterfaceSettings(impl_->usb, 0U, &descriptor) == FALSE ||
        descriptor.bInterfaceNumber != kSoundSwitchControlOneInterface) {
        const auto error = ::GetLastError();
        return fail(error == ERROR_SUCCESS ? ERROR_BAD_DEVICE : error);
    }
    impl_->status.interface_number = descriptor.bInterfaceNumber;
    UCHAR alternate = 0U;
    if (::WinUsb_GetCurrentAlternateSetting(impl_->usb, &alternate) == FALSE) {
        return fail(::GetLastError());
    }
    if (alternate != 0U &&
        ::WinUsb_SetCurrentAlternateSetting(impl_->usb, 0U) == FALSE) {
        return fail(::GetLastError());
    }
    if (::WinUsb_GetCurrentAlternateSetting(impl_->usb, &alternate) == FALSE ||
        alternate != 0U) {
        const auto error = ::GetLastError();
        return fail(error == ERROR_SUCCESS ? ERROR_BAD_DEVICE : error);
    }
    impl_->status.alternate_setting = alternate;

    bool valid_pipe = false;
    for (UCHAR index = 0U; index < descriptor.bNumEndpoints; ++index) {
        WINUSB_PIPE_INFORMATION pipe{};
        if (::WinUsb_QueryPipe(impl_->usb, 0U, index, &pipe) != FALSE &&
            pipe.PipeId == kSoundSwitchControlOneBulkOutPipe &&
            pipe.PipeType == UsbdPipeTypeBulk &&
            pipe.MaximumPacketSize == kSoundSwitchControlOneBulkPacketSize) {
            valid_pipe = true;
            impl_->status.bulk_out_pipe = pipe.PipeId;
            impl_->status.maximum_packet_size = pipe.MaximumPacketSize;
        }
    }
    if (!valid_pipe ||
        ::WinUsb_ResetPipe(impl_->usb, kSoundSwitchControlOneBulkOutPipe) ==
            FALSE) {
        const auto error = ::GetLastError();
        return fail(error == ERROR_SUCCESS ? ERROR_BAD_DEVICE : error);
    }
    ULONG timeout = static_cast<ULONG>(config.transfer_timeout.count());
    if (::WinUsb_SetPipePolicy(
            impl_->usb, kSoundSwitchControlOneBulkOutPipe,
            PIPE_TRANSFER_TIMEOUT, sizeof(timeout), &timeout) == FALSE) {
        return fail(::GetLastError());
    }
    BOOL terminate = FALSE;
    if (::WinUsb_SetPipePolicy(
            impl_->usb, kSoundSwitchControlOneBulkOutPipe,
            SHORT_PACKET_TERMINATE, sizeof(terminate), &terminate) == FALSE) {
        return fail(::GetLastError());
    }

    impl_->status.state = SoundSwitchControlOneLifecycleState::Initializing;
    for (const auto& packet : kSoundSwitchControlOneInitializationPackets) {
        if (!write_exact(
                impl_->usb, packet.data(), packet.size(),
                impl_->status.last_error)) {
            return fail(impl_->status.last_error);
        }
    }

    impl_->status.state = SoundSwitchControlOneLifecycleState::WarmingUp;
    if (!send_blackout(config.warmup_blackout_pairs, config.frame_interval)) {
        return fail(impl_->status.last_error);
    }
    impl_->status.warmup_complete = true;
    ++impl_->status.open_successes;
    impl_->status.last_error = ERROR_SUCCESS;
    impl_->status.state = SoundSwitchControlOneLifecycleState::Streaming;
    return true;
}

void SoundSwitchControlOneSession::close() noexcept {
    if (impl_ == nullptr) {
        return;
    }
    if (impl_->usb != nullptr) {
        impl_->status.state = SoundSwitchControlOneLifecycleState::Closing;
        static_cast<void>(send_blackout(
            impl_->config.close_blackout_pairs, impl_->config.frame_interval));
        ::WinUsb_Free(impl_->usb);
        impl_->usb = nullptr;
    }
    if (impl_->file != INVALID_HANDLE_VALUE) {
        static_cast<void>(::CloseHandle(impl_->file));
        impl_->file = INVALID_HANDLE_VALUE;
    }
    impl_->status.handle_open = false;
    impl_->status.state = SoundSwitchControlOneLifecycleState::Disabled;
}

bool SoundSwitchControlOneSession::is_open() const noexcept {
    return impl_ != nullptr && impl_->file != INVALID_HANDLE_VALUE &&
        impl_->usb != nullptr;
}

bool SoundSwitchControlOneSession::send(
    SoundSwitchControlOnePort port,
    const DmxUniverse& universe) noexcept {
    if (!is_open() ||
        static_cast<std::uint8_t>(port) >= kSoundSwitchControlOneOutputCount) {
        return false;
    }
    ++impl_->status.frames_attempted;
    const auto packet = build_soundswitch_control_one_packet(port, universe);
    if (!write_exact(
            impl_->usb, packet.bytes.data(), packet.length,
            impl_->status.last_error)) {
        ++impl_->status.frames_failed;
        impl_->status.state = SoundSwitchControlOneLifecycleState::Fault;
        return false;
    }
    ++impl_->status.frames_accepted;
    return true;
}

bool SoundSwitchControlOneSession::send_pair(
    const std::array<DmxUniverse, kSoundSwitchControlOneOutputCount>& universes)
    noexcept {
    return send(SoundSwitchControlOnePort::One, universes[0]) &&
        send(SoundSwitchControlOnePort::Two, universes[1]);
}

bool SoundSwitchControlOneSession::send_blackout(
    std::uint16_t repetitions,
    std::chrono::milliseconds interval) noexcept {
    if (!is_open() || interval.count() < 10) {
        return false;
    }
    const std::array<DmxUniverse, kSoundSwitchControlOneOutputCount> blackout{};
    for (std::uint16_t pair = 0U; pair < repetitions; ++pair) {
        if (!send_pair(blackout)) {
            return false;
        }
        if (pair + 1U < repetitions) {
            std::this_thread::sleep_for(interval);
        }
    }
    return true;
}

std::uint32_t SoundSwitchControlOneSession::last_error() const noexcept {
    return impl_ == nullptr ? ERROR_NOT_ENOUGH_MEMORY : impl_->status.last_error;
}

SoundSwitchControlOneSessionStatus SoundSwitchControlOneSession::status() const
    noexcept {
    return impl_ == nullptr ? SoundSwitchControlOneSessionStatus{} : impl_->status;
}

#else

struct SoundSwitchControlOneSession::Impl {
    SoundSwitchControlOneSessionConfig config{};
    SoundSwitchControlOneSessionStatus status{};
};

SoundSwitchControlOneSession::SoundSwitchControlOneSession() noexcept
    : impl_(new (std::nothrow) Impl{}) {}
SoundSwitchControlOneSession::~SoundSwitchControlOneSession() noexcept = default;
bool SoundSwitchControlOneSession::supported() noexcept { return false; }
bool SoundSwitchControlOneSession::open(
    const SoundSwitchControlOneSessionConfig& config) noexcept {
    if (impl_ != nullptr) {
        impl_->config = config;
        impl_->status = {};
        impl_->status.state = SoundSwitchControlOneLifecycleState::Fault;
    }
    return false;
}
void SoundSwitchControlOneSession::close() noexcept {
    if (impl_ != nullptr) {
        impl_->status.state = SoundSwitchControlOneLifecycleState::Disabled;
    }
}
bool SoundSwitchControlOneSession::is_open() const noexcept { return false; }
bool SoundSwitchControlOneSession::send(
    SoundSwitchControlOnePort,
    const DmxUniverse&) noexcept { return false; }
bool SoundSwitchControlOneSession::send_pair(
    const std::array<DmxUniverse, kSoundSwitchControlOneOutputCount>&) noexcept {
    return false;
}
bool SoundSwitchControlOneSession::send_blackout(
    std::uint16_t,
    std::chrono::milliseconds) noexcept { return false; }
std::uint32_t SoundSwitchControlOneSession::last_error() const noexcept {
    return impl_ == nullptr ? 0U : impl_->status.last_error;
}
SoundSwitchControlOneSessionStatus SoundSwitchControlOneSession::status() const
    noexcept {
    return impl_ == nullptr ? SoundSwitchControlOneSessionStatus{} : impl_->status;
}

#endif

}  // namespace showcore
