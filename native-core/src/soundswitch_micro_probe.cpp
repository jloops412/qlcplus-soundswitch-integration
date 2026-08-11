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
constexpr UCHAR kBulkOutPipe = 0x01U;

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
    bool writes_succeeded{false};
    bool visible_match{false};
    showcore::SoundSwitchMicroFraming framing{
        showcore::SoundSwitchMicroFraming::NativeJls1};
    DWORD error{ERROR_SUCCESS};
    std::uint16_t address{1U};
};

[[nodiscard]] bool write_packet(
    WINUSB_INTERFACE_HANDLE usb,
    const showcore::SoundSwitchMicroPacket& packet,
    DWORD& error) noexcept {
    const BOOL terminate = FALSE;
    if (::WinUsb_SetPipePolicy(
            usb, kBulkOutPipe, SHORT_PACKET_TERMINATE,
            sizeof(terminate), const_cast<BOOL*>(&terminate)) == FALSE) {
        error = ::GetLastError();
        return false;
    }
    ULONG transferred = 0U;
    if (::WinUsb_WritePipe(
            usb, kBulkOutPipe,
            const_cast<PUCHAR>(packet.bytes.data()),
            static_cast<ULONG>(packet.length), &transferred, nullptr) == FALSE) {
        error = ::GetLastError();
        return false;
    }
    if (transferred != packet.length) {
        error = ERROR_WRITE_FAULT;
        return false;
    }
    error = ERROR_SUCCESS;
    return true;
}

[[nodiscard]] bool stream_packet(
    WINUSB_INTERFACE_HANDLE usb,
    const showcore::SoundSwitchMicroPacket& packet,
    unsigned frames,
    DWORD& error) {
    for (unsigned frame = 0U; frame < frames; ++frame) {
        if (!write_packet(usb, packet, error)) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    return true;
}

[[nodiscard]] ActiveTestResult run_active_test(
    const std::wstring& path,
    std::uint16_t address) {
    ActiveTestResult result;
    result.address = address;
    FileHandle file(::CreateFileW(
        path.c_str(), GENERIC_READ | GENERIC_WRITE,
        0U, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr));
    if (!file.valid()) {
        result.error = ::GetLastError();
        return result;
    }
    WINUSB_INTERFACE_HANDLE raw_usb = nullptr;
    if (::WinUsb_Initialize(file.get(), &raw_usb) == FALSE) {
        result.error = ::GetLastError();
        return result;
    }
    WinUsbHandle usb(raw_usb);
    result.opened = true;

    USB_INTERFACE_DESCRIPTOR descriptor{};
    if (::WinUsb_QueryInterfaceSettings(usb.get(), 0U, &descriptor) == FALSE) {
        result.error = ::GetLastError();
        return result;
    }
    bool expected_pipe = false;
    for (UCHAR index = 0U; index < descriptor.bNumEndpoints; ++index) {
        WINUSB_PIPE_INFORMATION pipe{};
        if (::WinUsb_QueryPipe(usb.get(), 0U, index, &pipe) != FALSE &&
            pipe.PipeId == kBulkOutPipe && pipe.PipeType == UsbdPipeTypeBulk &&
            pipe.MaximumPacketSize == 64U) {
            expected_pipe = true;
        }
    }
    if (!expected_pipe) {
        result.error = ERROR_BAD_DEVICE;
        return result;
    }
    ULONG timeout = 500U;
    if (::WinUsb_SetPipePolicy(
            usb.get(), kBulkOutPipe, PIPE_TRANSFER_TIMEOUT,
            sizeof(timeout), &timeout) == FALSE) {
        result.error = ::GetLastError();
        return result;
    }
    for (const auto& initialization : showcore::kSoundSwitchMicroInitializationPackets) {
        showcore::SoundSwitchMicroPacket packet;
        packet.length = initialization.size();
        std::copy(initialization.begin(), initialization.end(), packet.bytes.begin());
        if (!write_packet(usb.get(), packet, result.error)) {
            return result;
        }
    }

    showcore::DmxUniverse off{};
    showcore::DmxUniverse test{};
    const auto first = static_cast<std::size_t>(address - 1U);
    test[first] = 255U;
    if (first + 1U < test.size()) {
        test[first + 1U] = 255U;
    }

    constexpr std::array framings{
        showcore::SoundSwitchMicroFraming::NativeJls1};
    for (const auto framing : framings) {
        result.framing = framing;
        const auto off_packet = showcore::build_soundswitch_micro_packet(off, framing);
        const auto test_packet = showcore::build_soundswitch_micro_packet(test, framing);
        std::wcout << L"\nTesting " << framing_name(framing) << L".\n"
                   << L"The isolated bench fixture may respond for about three seconds.\n";
        DWORD error = ERROR_SUCCESS;
        if (!stream_packet(usb.get(), off_packet, 8U, error) ||
            !stream_packet(usb.get(), test_packet, 120U, error) ||
            !stream_packet(usb.get(), off_packet, 8U, error)) {
            result.error = error;
            std::wcout << L"USB write failed (" << error << L": "
                       << win32_message(error) << L").\n";
            continue;
        }
        result.writes_succeeded = true;
        std::wcout << L"Did the isolated fixture visibly respond? [y/N]: ";
        std::wstring answer;
        std::getline(std::wcin, answer);
        if (affirmative(answer)) {
            result.visible_match = true;
            result.error = ERROR_SUCCESS;
            break;
        }
    }

    if (result.visible_match) {
        const auto off_packet = showcore::build_soundswitch_micro_packet(off, result.framing);
        DWORD ignored = ERROR_SUCCESS;
        static_cast<void>(stream_packet(usb.get(), off_packet, 3U, ignored));
    }
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
           << L"DMX address: " << result.address << L"\n"
           << L"Device opened: " << (result.opened ? L"yes" : L"no") << L"\n"
           << L"USB writes completed: " << (result.writes_succeeded ? L"yes" : L"no") << L"\n"
           << L"Visible DMX match: " << (result.visible_match ? L"yes" : L"no") << L"\n"
           << L"Selected framing: " << framing_name(result.framing) << L"\n"
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
        << L"EmberLights SoundSwitch Micro Output Test\n"
        << L"====================================\n\n"
        << L"This tool only opens the SoundSwitch Micro device (VID 15E4, PID 0053).\n"
        << L"Descriptor collection is passive. EmberLights itself now owns the normal\n"
        << L"full-universe output path; this probe does not transmit unless launched\n"
        << L"manually with the --active-test argument.\n\n"
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
        << L"3. Connect only one non-hazardous bench fixture and set its DMX address.\n"
        << L"4. Keep movers, fog, lasers, and all other effects disconnected.\n\n"
        << L"Enter the bench fixture DMX address [default 1]: ";
    std::wstring address_text;
    std::getline(std::wcin, address_text);
    std::uint16_t address = 1U;
    if (!parse_dmx_address(address_text, address)) {
        std::wcerr << L"Invalid address. Enter a number from 1 through 512.\n";
        return 4;
    }
    std::wcout << L"Type TEST to authorize the bounded DMX output test: ";
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
    const auto active = run_active_test(*interface_path, address);
    save_active_report(active);
    if (active.visible_match) {
        std::wcout << L"\nSUCCESS: " << framing_name(active.framing)
                   << L" produced visible DMX output.\n"
                   << L"The result is saved on your Desktop as:\n"
                   << active_report_path().wstring() << L"\n"
                   << L"Upload that small text file here so the Micro can be marked physically verified.\n";
    } else if (!active.opened) {
        std::wcerr << L"\nThe Micro could not be opened (" << active.error << L": "
                   << win32_message(active.error) << L"). Close SoundSwitch/EmberLights and retry.\n";
    } else {
        std::wcerr << L"\nThe native JLS1 test produced no visible output. The result report is on your Desktop;\n"
                   << L"upload it here with the fixture model, DMX address, and receiver state.\n";
    }
    std::wcout << L"Press Enter to close.";
    std::wstring ignored;
    std::getline(std::wcin, ignored);
    return active.visible_match ? 0 : 6;
}
