#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cfgmgr32.h>
#include <knownfolders.h>
#include <setupapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <usb.h>
#include <usbiodef.h>
#include <winusb.h>

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
    return true;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc == 2 && std::wstring_view(argv[1]) == L"--self-test") {
        return self_test() ? 0 : 1;
    }

    std::wcout
        << L"EmberLights SoundSwitch Micro Probe\n"
        << L"====================================\n\n"
        << L"This reads only the SoundSwitch Micro device (VID 15E4, PID 0053).\n"
        << L"It does not capture other USB traffic and will not transmit DMX.\n\n"
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
               << L"\nSaved report to:\n" << path.wstring()
               << L"\n\nThe report will open in Notepad. Upload that file here.\n"
               << L"Press Enter to close this window.";
    static_cast<void>(::ShellExecuteW(
        nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    std::wstring ignored;
    std::getline(std::wcin, ignored);
    return 0;
}
