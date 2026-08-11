#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cfgmgr32.h>
#include <knownfolders.h>
#include <setupapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <usb.h>
#include <initguid.h>
#include <usbiodef.h>
#include <winusb.h>

#include "emberlights/hardware_qualification.hpp"
#include "showcore/soundswitch_micro.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cwctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

constexpr std::wstring_view kVendorId = L"VID_15E4";
constexpr std::wstring_view kProductId = L"PID_0053";

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
    [[nodiscard]] bool valid() const noexcept { return value_ != INVALID_HANDLE_VALUE; }

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

class FileHandle {
public:
    explicit FileHandle(HANDLE value = INVALID_HANDLE_VALUE) noexcept : value_(value) {}
    ~FileHandle() noexcept {
        if (value_ != INVALID_HANDLE_VALUE) {
            static_cast<void>(::CloseHandle(value_));
        }
    }
    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;
    [[nodiscard]] HANDLE get() const noexcept { return value_; }
    [[nodiscard]] bool valid() const noexcept { return value_ != INVALID_HANDLE_VALUE; }

private:
    HANDLE value_{INVALID_HANDLE_VALUE};
};

class WinUsbHandle {
public:
    explicit WinUsbHandle(WINUSB_INTERFACE_HANDLE value = nullptr) noexcept : value_(value) {}
    ~WinUsbHandle() noexcept {
        if (value_ != nullptr) {
            ::WinUsb_Free(value_);
        }
    }
    WinUsbHandle(const WinUsbHandle&) = delete;
    WinUsbHandle& operator=(const WinUsbHandle&) = delete;
    [[nodiscard]] WINUSB_INTERFACE_HANDLE get() const noexcept { return value_; }
    [[nodiscard]] bool valid() const noexcept { return value_ != nullptr; }

private:
    WINUSB_INTERFACE_HANDLE value_{nullptr};
};

[[nodiscard]] std::wstring upper(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t character) {
        return static_cast<wchar_t>(::towupper(character));
    });
    return value;
}

[[nodiscard]] bool is_target_text(std::wstring_view value) {
    const auto normalized = upper(std::wstring(value));
    return normalized.find(kVendorId) != std::wstring::npos &&
           normalized.find(kProductId) != std::wstring::npos;
}

[[nodiscard]] std::wstring win32_message(DWORD error) {
    wchar_t* raw = nullptr;
    const auto size = ::FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error,
        0,
        reinterpret_cast<wchar_t*>(&raw),
        0,
        nullptr);
    std::wstring message = size != 0U && raw != nullptr
        ? std::wstring(raw, size)
        : L"Unknown Windows error";
    if (raw != nullptr) {
        static_cast<void>(::LocalFree(raw));
    }
    while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n')) {
        message.pop_back();
    }
    return message;
}

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

[[nodiscard]] std::vector<std::wstring> read_multi_string(HKEY key, const wchar_t* name) {
    DWORD type = 0U;
    DWORD bytes = 0U;
    if (::RegQueryValueExW(key, name, nullptr, &type, nullptr, &bytes) != ERROR_SUCCESS ||
        (type != REG_MULTI_SZ && type != REG_SZ) || bytes == 0U) {
        return {};
    }
    std::vector<wchar_t> buffer(bytes / sizeof(wchar_t) + 2U, L'\0');
    if (::RegQueryValueExW(
            key, name, nullptr, &type, reinterpret_cast<BYTE*>(buffer.data()),
            &bytes) != ERROR_SUCCESS) {
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

struct TargetDevice {
    std::wstring instance_id;
    std::wstring description;
    std::wstring service;
    std::wstring class_guid;
    std::wstring driver_key;
    std::vector<std::wstring> interface_guids;
};

[[nodiscard]] std::optional<TargetDevice> find_target_device() {
    DeviceInfoSet set(::SetupDiGetClassDevsW(
        nullptr, nullptr, nullptr, DIGCF_PRESENT | DIGCF_ALLCLASSES));
    if (!set.valid()) {
        return std::nullopt;
    }
    for (DWORD index = 0U;; ++index) {
        SP_DEVINFO_DATA device{};
        device.cbSize = sizeof(device);
        if (::SetupDiEnumDeviceInfo(set.get(), index, &device) == FALSE) {
            break;
        }
        const auto hardware_ids = device_property(set.get(), device, SPDRP_HARDWAREID);
        if (!is_target_text(hardware_ids)) {
            continue;
        }
        TargetDevice target;
        std::array<wchar_t, MAX_DEVICE_ID_LEN> instance{};
        if (::SetupDiGetDeviceInstanceIdW(
                set.get(), &device, instance.data(),
                static_cast<DWORD>(instance.size()), nullptr) != FALSE) {
            target.instance_id = instance.data();
        }
        target.description = device_property(set.get(), device, SPDRP_FRIENDLYNAME);
        if (target.description.empty()) {
            target.description = device_property(set.get(), device, SPDRP_DEVICEDESC);
        }
        target.service = device_property(set.get(), device, SPDRP_SERVICE);
        target.class_guid = device_property(set.get(), device, SPDRP_CLASSGUID);
        target.driver_key = device_property(set.get(), device, SPDRP_DRIVER);

        RegistryKey device_key(::SetupDiOpenDevRegKey(
            set.get(), &device, DICS_FLAG_GLOBAL, 0U, DIREG_DEV, KEY_READ));
        if (device_key.valid()) {
            target.interface_guids = read_multi_string(
                device_key.get(), L"DeviceInterfaceGUIDs");
            if (target.interface_guids.empty()) {
                HKEY parameters_raw = nullptr;
                if (::RegOpenKeyExW(
                        device_key.get(), L"Device Parameters", 0U, KEY_READ,
                        &parameters_raw) == ERROR_SUCCESS) {
                    RegistryKey parameters(parameters_raw);
                    target.interface_guids = read_multi_string(
                        parameters.get(), L"DeviceInterfaceGUIDs");
                }
            }
        }
        return target;
    }
    return std::nullopt;
}

[[nodiscard]] std::wstring guid_text(const GUID& guid) {
    std::array<wchar_t, 64> value{};
    const auto length = ::StringFromGUID2(
        guid, value.data(), static_cast<int>(value.size()));
    return length > 0 ? std::wstring(value.data()) : L"{unavailable}";
}

void describe_winusb_path(const std::wstring& path, std::wostringstream& report) {
    report << L"\nInterface path: " << path << L"\n";
    FileHandle file(::CreateFileW(
        path.c_str(), GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr));
    if (!file.valid()) {
        const auto error = ::GetLastError();
        report << L"  Open: FAILED (" << error << L": " << win32_message(error) << L")\n";
        return;
    }
    report << L"  Open: OK (read-only probe; no USB transfers sent)\n";
    WINUSB_INTERFACE_HANDLE raw_usb = nullptr;
    if (::WinUsb_Initialize(file.get(), &raw_usb) == FALSE) {
        const auto error = ::GetLastError();
        report << L"  WinUSB initialize: FAILED (" << error << L": "
               << win32_message(error) << L")\n";
        return;
    }
    WinUsbHandle usb(raw_usb);

    USB_DEVICE_DESCRIPTOR descriptor{};
    ULONG received = 0U;
    if (::WinUsb_GetDescriptor(
            usb.get(), USB_DEVICE_DESCRIPTOR_TYPE, 0U, 0U,
            reinterpret_cast<PUCHAR>(&descriptor), sizeof(descriptor), &received) != FALSE) {
        report << std::hex << std::uppercase;
        report << L"  Descriptor VID: 0x" << descriptor.idVendor
               << L" PID: 0x" << descriptor.idProduct
               << L" USB: 0x" << descriptor.bcdUSB
               << L" Device: 0x" << descriptor.bcdDevice << L"\n";
        report << std::dec;
        report << L"  Configurations: " << static_cast<unsigned>(descriptor.bNumConfigurations)
               << L"  EP0 packet: " << static_cast<unsigned>(descriptor.bMaxPacketSize0) << L"\n";
    } else {
        const auto error = ::GetLastError();
        report << L"  Device descriptor: FAILED (" << error << L": "
               << win32_message(error) << L")\n";
    }

    UCHAR speed = 0U;
    ULONG speed_size = sizeof(speed);
    if (::WinUsb_QueryDeviceInformation(
            usb.get(), DEVICE_SPEED, &speed_size, &speed) != FALSE) {
        report << L"  Speed code: " << static_cast<unsigned>(speed) << L"\n";
    }

    USB_INTERFACE_DESCRIPTOR interface_descriptor{};
    if (::WinUsb_QueryInterfaceSettings(
            usb.get(), 0U, &interface_descriptor) == FALSE) {
        const auto error = ::GetLastError();
        report << L"  Interface query: FAILED (" << error << L": "
               << win32_message(error) << L")\n";
        return;
    }
    report << L"  Interface: " << static_cast<unsigned>(interface_descriptor.bInterfaceNumber)
           << L" class/subclass/protocol "
           << static_cast<unsigned>(interface_descriptor.bInterfaceClass) << L"/"
           << static_cast<unsigned>(interface_descriptor.bInterfaceSubClass) << L"/"
           << static_cast<unsigned>(interface_descriptor.bInterfaceProtocol)
           << L" endpoints " << static_cast<unsigned>(interface_descriptor.bNumEndpoints)
           << L"\n";
    for (UCHAR index = 0U; index < interface_descriptor.bNumEndpoints; ++index) {
        WINUSB_PIPE_INFORMATION pipe{};
        if (::WinUsb_QueryPipe(usb.get(), 0U, index, &pipe) == FALSE) {
            const auto error = ::GetLastError();
            report << L"    Pipe " << static_cast<unsigned>(index)
                   << L": FAILED (" << error << L")\n";
            continue;
        }
        report << std::hex << std::uppercase
               << L"    Pipe " << static_cast<unsigned>(index)
               << L": id=0x" << static_cast<unsigned>(pipe.PipeId)
               << std::dec << L" type=" << static_cast<unsigned>(pipe.PipeType)
               << L" maxPacket=" << pipe.MaximumPacketSize
               << L" interval=" << static_cast<unsigned>(pipe.Interval) << L"\n";
    }
}

[[nodiscard]] bool add_guid_text(
    const std::wstring& value,
    std::vector<GUID>& guids) {
    GUID parsed{};
    if (::CLSIDFromString(value.c_str(), &parsed) != NOERROR) {
        return false;
    }
    const auto duplicate = std::any_of(guids.begin(), guids.end(), [&](const GUID& existing) {
        return ::IsEqualGUID(existing, parsed) != FALSE;
    });
    if (!duplicate) {
        guids.push_back(parsed);
    }
    return true;
}

void enumerate_interface_guid(const GUID& guid, std::wostringstream& report) {
    DeviceInfoSet set(::SetupDiGetClassDevsW(
        &guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));
    if (!set.valid()) {
        report << L"\nInterface class " << guid_text(guid) << L": unavailable\n";
        return;
    }
    bool found = false;
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
        std::vector<BYTE> bytes(required, 0U);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(bytes.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (::SetupDiGetDeviceInterfaceDetailW(
                set.get(), &interface_data, detail, required, nullptr, nullptr) == FALSE ||
            !is_target_text(detail->DevicePath)) {
            continue;
        }
        found = true;
        describe_winusb_path(detail->DevicePath, report);
    }
    if (!found) {
        report << L"\nInterface class " << guid_text(guid)
               << L": no matching present path\n";
    }
}

[[nodiscard]] std::optional<std::wstring> find_target_interface_path(
    const std::vector<GUID>& guids) {
    for (const auto& guid : guids) {
        DeviceInfoSet set(::SetupDiGetClassDevsW(
            &guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));
        if (!set.valid()) {
            continue;
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
            std::vector<BYTE> bytes(required, 0U);
            auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(bytes.data());
            detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
            if (::SetupDiGetDeviceInterfaceDetailW(
                    set.get(), &interface_data, detail, required, nullptr, nullptr) != FALSE &&
                is_target_text(detail->DevicePath)) {
                return std::wstring(detail->DevicePath);
            }
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::vector<GUID> target_interface_guids(const TargetDevice& device) {
    std::vector<GUID> guids;
    for (const auto& value : device.interface_guids) {
        static_cast<void>(add_guid_text(value, guids));
    }
    if (std::none_of(guids.begin(), guids.end(), [](const GUID& value) {
            return ::IsEqualGUID(value, GUID_DEVINTERFACE_USB_DEVICE) != FALSE;
        })) {
        guids.push_back(GUID_DEVINTERFACE_USB_DEVICE);
    }
    return guids;
}

[[nodiscard]] const wchar_t* framing_name(showcore::SoundSwitchMicroFraming framing) noexcept {
    switch (framing) {
    case showcore::SoundSwitchMicroFraming::NativeJls1:
        return L"SoundSwitch native JLS1";
    }
    return L"Unknown";
}

[[nodiscard]] bool parse_dmx_address(std::wstring_view text, std::uint16_t& address) noexcept {
    if (text.empty()) {
        address = 1U;
        return true;
    }
    std::uint32_t value = 0U;
    for (const auto character : text) {
        if (character < L'0' || character > L'9') {
            return false;
        }
        value = value * 10U + static_cast<std::uint32_t>(character - L'0');
        if (value > showcore::kUniverseSlots) {
            return false;
        }
    }
    if (value == 0U) {
        return false;
    }
    address = static_cast<std::uint16_t>(value);
    return true;
}

[[nodiscard]] bool affirmative(std::wstring_view text) noexcept {
    return !text.empty() && (text.front() == L'y' || text.front() == L'Y');
}

struct ActiveTestResult {
    bool opened{false};
    bool software_frame_match{false};
    bool raw_writes_succeeded{false};
    bool raw_visible_red{false};
    bool repeat_open_succeeded{false};
    bool runner_writes_succeeded{false};
    bool runner_visible_match{false};
    bool blackout_succeeded{false};
    bool writes_succeeded{false};
    bool visible_match{false};
    bool disconnect_observed{false};
    bool reconnect_detected{false};
    bool reconnect_open_succeeded{false};
    bool reconnect_writes_succeeded{false};
    bool reconnect_visible_red{false};
    bool reconnect_blackout_succeeded{false};
    showcore::SoundSwitchMicroFraming framing{
        showcore::SoundSwitchMicroFraming::NativeJls1};
    DWORD error{ERROR_SUCCESS};
    std::uint16_t address{1U};
    emberlights::FrameComparison frame_comparison{};
    emberlights::PacketComparison packet_comparison{};
    std::array<std::uint8_t, 6U> raw_channels{};
    std::array<std::uint8_t, 6U> runner_channels{};
    showcore::SoundSwitchMicroSessionStatus session{};

    [[nodiscard]] emberlights::MicroPhysicalQualificationEvidence evidence() const noexcept {
        return {
            software_frame_match,
            opened,
            raw_writes_succeeded,
            raw_visible_red,
            repeat_open_succeeded,
            runner_writes_succeeded,
            runner_visible_match,
            blackout_succeeded,
            disconnect_observed,
            reconnect_detected,
            reconnect_open_succeeded,
            reconnect_writes_succeeded,
            reconnect_visible_red,
            reconnect_blackout_succeeded};
    }
};

[[nodiscard]] bool wait_for_target_absence(std::chrono::seconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    do {
        if (!find_target_device().has_value()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    } while (std::chrono::steady_clock::now() < deadline);
    return false;
}

[[nodiscard]] std::optional<TargetDevice> wait_for_target_presence(
    std::chrono::seconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    do {
        if (auto device = find_target_device(); device.has_value()) {
            return device;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    } while (std::chrono::steady_clock::now() < deadline);
    return std::nullopt;
}

[[nodiscard]] bool stream_universe(
    showcore::SoundSwitchMicroSession& session,
    const showcore::DmxUniverse& universe,
    unsigned frames) {
    for (unsigned frame = 0U; frame < frames; ++frame) {
        if (!session.send(universe)) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    return true;
}

[[nodiscard]] ActiveTestResult run_active_test(const std::wstring& path) {
    ActiveTestResult result;
    if (path.empty()) {
        result.error = ERROR_DEVICE_NOT_CONNECTED;
        return result;
    }

    const auto qualification = emberlights::build_ir4_6ch_red_qualification();
    result.frame_comparison = qualification.frame_comparison;
    result.packet_comparison = qualification.packet_comparison;
    for (std::size_t index = 0U; index < result.raw_channels.size(); ++index) {
        result.raw_channels[index] = qualification.raw_reference[index];
        result.runner_channels[index] = qualification.runner_rendered[index];
    }
    result.software_frame_match = qualification.exact();
    if (!result.software_frame_match) {
        result.error = ERROR_INVALID_DATA;
        return result;
    }

    showcore::SoundSwitchMicroSession session;
    showcore::SoundSwitchMicroSessionConfig config;
    config.framing = showcore::SoundSwitchMicroFraming::NativeJls1;
    if (!session.open(config)) {
        result.error = session.last_error();
        result.session = session.status();
        return result;
    }
    result.opened = true;
    result.framing = config.framing;

    std::wcout
        << L"\nSoftware frame inspector: PASS\n"
        << L"Raw reference and compiled Runner frame are byte-identical.\n"
        << L"IR-4 channels 1-6: 255, 0, 0, 0, 0, 0 (red only).\n\n"
        << L"Stage 1: raw reference through " << framing_name(config.framing) << L".\n"
        << L"The isolated IR-4 should show red for about three seconds.\n";
    const auto raw_pre_blackout = session.send_blackout(8U);
    const auto raw_stream = raw_pre_blackout &&
        stream_universe(session, qualification.raw_reference, 120U);
    const auto raw_post_blackout = session.send_blackout(8U);
    result.raw_writes_succeeded =
        raw_pre_blackout && raw_stream && raw_post_blackout;
    result.blackout_succeeded = raw_pre_blackout && raw_post_blackout;
    result.error = session.last_error();
    result.session = session.status();
    if (!result.raw_writes_succeeded) {
        std::wcout << L"USB write failed (" << result.error << L": "
                   << win32_message(result.error) << L").\n";
    } else {
        std::wcout << L"Did the isolated IR-4 visibly show red, then black out? [y/N]: ";
        std::wstring answer;
        std::getline(std::wcin, answer);
        result.raw_visible_red = affirmative(answer);
    }
    session.close();
    result.session = session.status();

    if (result.raw_writes_succeeded && result.raw_visible_red) {
        std::wcout
            << L"\nStage 2: reopening the Micro and sending the compiled Runner frame.\n";
        result.repeat_open_succeeded = session.open(config);
        if (!result.repeat_open_succeeded) {
            result.error = session.last_error();
            result.session = session.status();
            std::wcout << L"Micro reopen failed (" << result.error << L": "
                       << win32_message(result.error) << L").\n";
        } else {
            const auto runner_pre_blackout = session.send_blackout(8U);
            const auto runner_stream = runner_pre_blackout &&
                stream_universe(session, qualification.runner_rendered, 120U);
            const auto runner_post_blackout = session.send_blackout(8U);
            result.runner_writes_succeeded =
                runner_pre_blackout && runner_stream && runner_post_blackout;
            result.blackout_succeeded = result.blackout_succeeded &&
                runner_pre_blackout && runner_post_blackout;
            result.error = session.last_error();
            result.session = session.status();
            if (!result.runner_writes_succeeded) {
                std::wcout << L"Runner-frame write failed (" << result.error << L": "
                           << win32_message(result.error) << L").\n";
            } else {
                std::wcout
                    << L"Did Stage 2 show the same red, then black out? [y/N]: ";
                std::wstring answer;
                std::getline(std::wcin, answer);
                result.runner_visible_match = affirmative(answer);
            }
        }
        if (result.repeat_open_succeeded && result.runner_writes_succeeded &&
            result.runner_visible_match && result.blackout_succeeded) {
            std::wcout
                << L"\nStage 3: unplug/replug recovery through the same production session lifecycle.\n"
                << L"Unplug the SoundSwitch Micro from USB now, then press Enter.\n";
            std::wstring ignored;
            std::getline(std::wcin, ignored);
            result.disconnect_observed = wait_for_target_absence(std::chrono::seconds(10));
            if (!result.disconnect_observed) {
                std::wcout
                    << L"Windows still reports the Micro present. Recovery output was not attempted.\n";
            }
            session.close();

            if (result.disconnect_observed) {
                std::wcout
                    << L"Disconnect observed. Reconnect the same Micro now.\n"
                    << L"Waiting up to 60 seconds; no project reload is needed...\n";
                const auto reconnected = wait_for_target_presence(std::chrono::seconds(60));
                result.reconnect_detected = reconnected.has_value();
                if (!result.reconnect_detected) {
                    result.error = ERROR_DEVICE_NOT_CONNECTED;
                    std::wcout << L"The Micro did not reappear within 60 seconds.\n";
                } else {
                    const auto reopen_deadline = std::chrono::steady_clock::now() +
                        std::chrono::seconds(10);
                    do {
                        result.reconnect_open_succeeded = session.open(config);
                        if (!result.reconnect_open_succeeded) {
                            result.error = session.last_error();
                            std::this_thread::sleep_for(std::chrono::milliseconds(500));
                        }
                    } while (!result.reconnect_open_succeeded &&
                             std::chrono::steady_clock::now() < reopen_deadline);
                    if (!result.reconnect_open_succeeded) {
                        result.error = session.last_error();
                        result.session = session.status();
                        std::wcout << L"Micro recovery open failed (" << result.error << L": "
                                   << win32_message(result.error) << L").\n";
                    } else {
                        std::wcout
                            << L"Recovery open succeeded. The IR-4 should show red again for "
                            << L"about three seconds, then black out.\n";
                        const auto reconnect_pre_blackout = session.send_blackout(8U);
                        const auto reconnect_stream = reconnect_pre_blackout &&
                            stream_universe(session, qualification.runner_rendered, 120U);
                        const auto reconnect_post_blackout = session.send_blackout(8U);
                        result.reconnect_writes_succeeded = reconnect_pre_blackout &&
                            reconnect_stream && reconnect_post_blackout;
                        result.reconnect_blackout_succeeded = reconnect_pre_blackout &&
                            reconnect_post_blackout;
                        result.error = session.last_error();
                        result.session = session.status();
                        if (!result.reconnect_writes_succeeded) {
                            std::wcout << L"Recovery-frame write failed (" << result.error << L": "
                                       << win32_message(result.error) << L").\n";
                        } else {
                            std::wcout
                                << L"After replug, did the IR-4 show red and then black out? [y/N]: ";
                            std::wstring answer;
                            std::getline(std::wcin, answer);
                            result.reconnect_visible_red = affirmative(answer);
                        }
                    }
                }
            }
        }
        session.close();
        result.session = session.status();
    }
    result.writes_succeeded = result.raw_writes_succeeded &&
        result.repeat_open_succeeded && result.runner_writes_succeeded;
    result.visible_match = result.raw_visible_red && result.runner_visible_match;
    return result;
}

[[nodiscard]] std::filesystem::path report_path();

[[nodiscard]] std::filesystem::path active_report_path() {
    auto path = report_path();
    path.replace_filename(L"SoundSwitch-Micro-active-test.txt");
    return path;
}

void save_active_report(const ActiveTestResult& result) {
    const auto path = active_report_path();
    std::wofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        return;
    }
    output << L"EmberLights SoundSwitch Micro active test\n"
           << L"Version: " << EMBERLIGHTS_VERSION << L"\n"
           << L"Source commit: " << EMBERLIGHTS_COMMIT << L"\n"
           << L"Fixture: Both Lighting BO-IR4 LED Mini Spotlight\n"
           << L"Fixture mode: 6 channel (R,G,B,W,A,UV)\n"
           << L"DMX address: " << result.address << L"\n"
           << L"Software raw/Runner frame match: "
           << (result.software_frame_match ? L"yes" : L"no") << L"\n"
           << L"Differing DMX slots: " << result.frame_comparison.differing_slots << L"\n"
           << L"Differing native packet bytes: "
           << result.packet_comparison.differing_bytes << L"\n"
           << L"Raw CH1-CH6: ";
    for (const auto value : result.raw_channels) {
        output << static_cast<unsigned int>(value) << L" ";
    }
    output << L"\nRunner CH1-CH6: ";
    for (const auto value : result.runner_channels) {
        output << static_cast<unsigned int>(value) << L" ";
    }
    output << L"\n"
           << L"Device opened: " << (result.opened ? L"yes" : L"no") << L"\n"
           << L"Raw writes completed: "
           << (result.raw_writes_succeeded ? L"yes" : L"no") << L"\n"
           << L"Raw visible red/blackout: "
           << (result.raw_visible_red ? L"yes" : L"no") << L"\n"
           << L"Repeat open completed: "
           << (result.repeat_open_succeeded ? L"yes" : L"no") << L"\n"
           << L"Runner writes completed: "
           << (result.runner_writes_succeeded ? L"yes" : L"no") << L"\n"
           << L"Runner visible match/blackout: "
           << (result.runner_visible_match ? L"yes" : L"no") << L"\n"
           << L"All bounded blackouts completed: "
           << (result.blackout_succeeded ? L"yes" : L"no") << L"\n"
           << L"USB writes completed: "
           << (result.writes_succeeded ? L"yes" : L"no") << L"\n"
           << L"Visible raw/Runner match: "
           << (result.visible_match ? L"yes" : L"no") << L"\n"
           << L"Disconnect observed: "
           << (result.disconnect_observed ? L"yes" : L"no") << L"\n"
           << L"Reconnect detected: "
           << (result.reconnect_detected ? L"yes" : L"no") << L"\n"
           << L"Reconnect open completed: "
           << (result.reconnect_open_succeeded ? L"yes" : L"no") << L"\n"
           << L"Reconnect writes completed: "
           << (result.reconnect_writes_succeeded ? L"yes" : L"no") << L"\n"
           << L"Reconnect visible red/blackout: "
           << (result.reconnect_visible_red ? L"yes" : L"no") << L"\n"
           << L"Reconnect bounded blackouts completed: "
           << (result.reconnect_blackout_succeeded ? L"yes" : L"no") << L"\n"
           << L"Overall physical qualification: "
           << emberlights::micro_physical_qualification_result_name(
                  emberlights::evaluate_micro_physical_qualification(result.evidence()))
           << L"\n"
           << L"Selected framing: " << framing_name(result.framing) << L"\n"
           << L"Lifecycle at test end: "
           << showcore::soundswitch_micro_lifecycle_name(result.session.state) << L"\n"
           << L"Configuration: " << static_cast<unsigned int>(result.session.configuration_value)
           << L"  alternate setting: "
           << static_cast<unsigned int>(result.session.alternate_setting) << L"\n"
           << L"Bulk OUT pipe: " << static_cast<unsigned int>(result.session.bulk_out_pipe)
           << L"  max packet: " << result.session.maximum_packet_size << L"\n"
           << L"Warm-up frames: " << result.session.warmup_frames_completed
           << L"  complete: " << (result.session.warmup_complete ? L"yes" : L"no") << L"\n"
           << L"Frames attempted/accepted/failed: " << result.session.frames_attempted
           << L"/" << result.session.frames_accepted
           << L"/" << result.session.frames_failed << L"\n"
           << L"Last Windows error: " << result.error << L"\n";
}

[[nodiscard]] std::filesystem::path report_path() {
    PWSTR desktop_raw = nullptr;
    if (::SHGetKnownFolderPath(FOLDERID_Desktop, KF_FLAG_DEFAULT, nullptr, &desktop_raw) == S_OK &&
        desktop_raw != nullptr) {
        const std::filesystem::path desktop(desktop_raw);
        ::CoTaskMemFree(desktop_raw);
        return desktop / L"SoundSwitch-Micro-report.txt";
    }
    if (desktop_raw != nullptr) {
        ::CoTaskMemFree(desktop_raw);
    }
    return std::filesystem::current_path() / L"SoundSwitch-Micro-report.txt";
}

[[nodiscard]] std::wstring build_report(const TargetDevice& device) {
    SYSTEMTIME now{};
    ::GetLocalTime(&now);
    std::wostringstream report;
    report << L"EmberLights SoundSwitch Micro Probe\n"
           << L"Probe version: " << EMBERLIGHTS_VERSION << L"\n"
           << L"Source commit: " << EMBERLIGHTS_COMMIT << L"\n"
           << L"Captured: " << now.wYear << L"-" << now.wMonth << L"-" << now.wDay
           << L" " << now.wHour << L":" << now.wMinute << L":" << now.wSecond << L" local\n"
           << L"Safety: descriptor-only; no control, bulk, or interrupt transfers sent\n\n"
           << L"Description: " << device.description << L"\n"
           << L"Instance ID: " << device.instance_id << L"\n"
           << L"Service: " << device.service << L"\n"
           << L"Class GUID: " << device.class_guid << L"\n"
           << L"Driver key: " << device.driver_key << L"\n";

    std::vector<GUID> guids;
    report << L"DeviceInterfaceGUIDs:";
    if (device.interface_guids.empty()) {
        report << L" (not published in the device registry)\n";
    } else {
        report << L"\n";
        for (const auto& value : device.interface_guids) {
            report << L"  " << value << L"\n";
            if (!add_guid_text(value, guids)) {
                report << L"    (could not parse as GUID)\n";
            }
        }
    }
    if (std::none_of(guids.begin(), guids.end(), [](const GUID& value) {
            return ::IsEqualGUID(value, GUID_DEVINTERFACE_USB_DEVICE) != FALSE;
        })) {
        guids.push_back(GUID_DEVINTERFACE_USB_DEVICE);
    }
    for (const auto& guid : guids) {
        enumerate_interface_guid(guid, report);
    }
    report << L"\nEnd of report. Upload this text file to the EmberLights conversation.\n";
    return report.str();
}

[[nodiscard]] bool self_test() {
    if (!is_target_text(L"USB\\VID_15E4&PID_0053\\123") ||
        is_target_text(L"USB\\VID_15E4&PID_0054\\123") ||
        is_target_text(L"USB\\VID_0000&PID_0053\\123")) {
        return false;
    }
    std::vector<GUID> guids;
    if (!add_guid_text(L"{A5DCBF10-6530-11D2-901F-00C04FB951ED}", guids) ||
        guids.size() != 1U ||
        ::IsEqualGUID(guids.front(), GUID_DEVINTERFACE_USB_DEVICE) == FALSE) {
        return false;
    }
    std::uint16_t address = 0U;
    if (!parse_dmx_address(L"1", address) || address != 1U ||
        !parse_dmx_address(L"512", address) || address != 512U ||
        !parse_dmx_address(L"", address) || address != 1U ||
        parse_dmx_address(L"0", address) || parse_dmx_address(L"513", address) ||
        parse_dmx_address(L"1x", address)) {
        return false;
    }
    showcore::DmxUniverse universe{};
    universe[0] = 0xAAU;
    const auto packet = showcore::build_soundswitch_micro_packet(
        universe, showcore::SoundSwitchMicroFraming::NativeJls1);
    if (packet.length != 522U || packet.bytes[0] != 's' ||
        packet.bytes[4] != 0x01U || packet.bytes[6] != 0x02U ||
        packet.bytes[8] != 0U || packet.bytes[9] != 0U ||
        packet.bytes[10] != 0xAAU) {
        return false;
    }
    return true;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc == 2 && std::wstring_view(argv[1]) == L"--self-test") {
        return self_test() ? 0 : 1;
    }

    const bool active_test =
        argc == 2 && std::wstring_view(argv[1]) == L"--active-test";

    std::wcout
        << L"EmberLights Hardware Test\n"
        << L"=========================\n\n"
        << L"This tool only opens the SoundSwitch Micro device (VID 15E4, PID 0053).\n"
        << L"Descriptor collection is passive. EmberLights itself now owns the normal\n"
        << L"full-universe output path; this tool does not transmit unless launched\n"
        << L"through the Hardware Test shortcut or with --active-test.\n\n"
        << L"Plug the SoundSwitch Micro dongle into this PC now.\n"
        << L"Waiting up to 60 seconds...\n";

    std::optional<TargetDevice> device;
    for (unsigned attempt = 0U; attempt < 120U && !device.has_value(); ++attempt) {
        device = find_target_device();
        if (!device.has_value()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }
    if (!device.has_value()) {
        std::wcerr << L"\nThe SoundSwitch Micro was not detected. No changes were made.\n"
                   << L"Press Enter to close.";
        std::wstring ignored;
        std::getline(std::wcin, ignored);
        return 2;
    }

    const auto report = build_report(*device);
    const auto path = report_path();
    std::wofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        std::wcerr << L"\nThe device was detected, but the report could not be saved to:\n"
                   << path.wstring() << L"\n\nPress Enter to close.";
        std::wstring ignored;
        std::getline(std::wcin, ignored);
        return 3;
    }
    output << report;
    output.close();

    std::wcout << L"\nDetected: " << device->description
               << L"\nSaved descriptor report to:\n" << path.wstring() << L"\n";
    if (!active_test) {
        static_cast<void>(::ShellExecuteW(
            nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
        return 0;
    }

    std::wcout
        << L"\nACTIVE TEST SETUP\n"
        << L"1. Fully close SoundSwitch and EmberLights.\n"
        << L"2. Connect Micro XLR -> Donner transmitter.\n"
        << L"3. Connect one Both Lighting IR-4 only. Set 6-channel mode and address 001.\n"
        << L"4. Confirm the IR-4 Master setting is Off.\n"
        << L"5. Keep movers, fog, lasers, and all other effects disconnected.\n\n"
        << L"Type TEST to authorize the fixed U1/address-001 red test: ";
    std::wstring confirmation;
    std::getline(std::wcin, confirmation);
    if (upper(confirmation) != L"TEST") {
        std::wcout << L"Active test cancelled; no USB output was sent.\n";
        return 0;
    }

    const auto interface_path = find_target_interface_path(target_interface_guids(*device));
    if (!interface_path.has_value()) {
        std::wcerr << L"The target WinUSB interface path disappeared. Reconnect the Micro and retry.\n";
        return 5;
    }
    const auto active = run_active_test(*interface_path);
    save_active_report(active);
    const auto qualification_result =
        emberlights::evaluate_micro_physical_qualification(active.evidence());
    if (qualification_result == emberlights::MicroPhysicalQualificationResult::Passed) {
        std::wcout << L"\nSUCCESS: " << framing_name(active.framing)
                   << L" produced matching raw and Runner red output, blackout, and "
                   << L"unplug/replug recovery.\n"
                   << L"The result is saved on your Desktop as:\n"
                   << active_report_path().wstring() << L"\n"
                   << L"Upload that small text file here so the Micro can be marked physically verified.\n";
    } else if (!active.software_frame_match) {
        std::wcerr
            << L"\nThe raw and compiled Runner frames did not match. No USB output was sent.\n";
    } else if (!active.opened) {
        std::wcerr << L"\nThe Micro could not be opened (" << active.error << L": "
                   << win32_message(active.error) << L"). Close SoundSwitch/EmberLights and retry.\n";
    } else {
        std::wcerr
            << L"\nThe full native JLS1 raw/Runner qualification did not pass. "
            << L"Exact failing gate: "
            << emberlights::micro_physical_qualification_result_name(qualification_result)
            << L". "
            << L"The result report is on your Desktop;\n"
            << L"upload it here with the receiver state and which stage failed.\n";
    }
    std::wcout << L"Press Enter to close.";
    std::wstring ignored;
    std::getline(std::wcin, ignored);
    return qualification_result == emberlights::MicroPhysicalQualificationResult::Passed
        ? 0
        : 6;
}
