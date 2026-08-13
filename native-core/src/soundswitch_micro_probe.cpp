#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <conio.h>
#include <cfgmgr32.h>
#include <knownfolders.h>
#include <setupapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <usb.h>
#include <initguid.h>
#include <usbiodef.h>
#include <winusb.h>

#include "emberlights/raw_hardware_test_operator.hpp"
#include "showcore/soundswitch_micro.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
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

std::atomic_bool cancellation_requested{false};
std::atomic_bool operator_session_active{false};
std::atomic_bool operator_terminal_complete{true};

BOOL WINAPI console_control_handler(DWORD control) noexcept {
    switch (control) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        cancellation_requested.store(true, std::memory_order_relaxed);
        if ((control == CTRL_CLOSE_EVENT || control == CTRL_LOGOFF_EVENT ||
             control == CTRL_SHUTDOWN_EVENT) &&
            operator_session_active.load(std::memory_order_acquire)) {
            // Windows invokes this handler on a helper thread. Give the main
            // coordinator a bounded window to perform terminal blackout,
            // close, and audit append before the console is torn down.
            for (unsigned attempt = 0U;
                 attempt < 200U &&
                 !operator_terminal_complete.load(std::memory_order_acquire);
                 ++attempt) {
                ::Sleep(20U);
            }
        }
        return TRUE;
    default:
        return FALSE;
    }
}

class ConsoleControlRegistration {
public:
    ConsoleControlRegistration() noexcept
        : installed_(::SetConsoleCtrlHandler(console_control_handler, TRUE) != FALSE) {}
    ~ConsoleControlRegistration() noexcept {
        if (installed_) {
            static_cast<void>(::SetConsoleCtrlHandler(
                console_control_handler, FALSE));
        }
    }
    ConsoleControlRegistration(const ConsoleControlRegistration&) = delete;
    ConsoleControlRegistration& operator=(const ConsoleControlRegistration&) = delete;

private:
    bool installed_{false};
};

[[nodiscard]] std::optional<std::string> utf8(std::wstring_view value) {
    if (value.empty()) {
        return std::string{};
    }
    const auto size = ::WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return std::nullopt;
    }
    std::string result(static_cast<std::size_t>(size), '\0');
    if (::WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
            static_cast<int>(value.size()), result.data(), size,
            nullptr, nullptr) != size) {
        return std::nullopt;
    }
    return result;
}

[[nodiscard]] std::optional<std::wstring> wide(std::string_view value) {
    if (value.empty()) {
        return std::wstring{};
    }
    const auto size = ::MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) {
        return std::nullopt;
    }
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    if (::MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
            static_cast<int>(value.size()), result.data(), size) != size) {
        return std::nullopt;
    }
    return result;
}

[[nodiscard]] std::string utc_now() {
    SYSTEMTIME now{};
    ::GetSystemTime(&now);
    std::array<char, 21U> value{};
    const auto length = std::snprintf(
        value.data(), value.size(), "%04u-%02u-%02uT%02u:%02u:%02uZ",
        static_cast<unsigned int>(now.wYear),
        static_cast<unsigned int>(now.wMonth),
        static_cast<unsigned int>(now.wDay),
        static_cast<unsigned int>(now.wHour),
        static_cast<unsigned int>(now.wMinute),
        static_cast<unsigned int>(now.wSecond));
    return length == 20 ? std::string(value.data(), 20U) : std::string{};
}

enum class ObservationCommandKind : std::uint8_t {
    Invalid,
    Pass,
    Fail,
    Spill,
    Cancel
};

struct ObservationCommand {
    ObservationCommandKind kind{ObservationCommandKind::Invalid};
    std::string text;
};

[[nodiscard]] ObservationCommand parse_observation_command(
    std::wstring_view line) {
    const auto separator = line.find(L' ');
    const auto command = upper(std::wstring(line.substr(0U, separator)));
    if (separator == std::wstring_view::npos || separator + 1U >= line.size()) {
        return {};
    }
    const auto converted = utf8(line.substr(separator + 1U));
    if (!converted.has_value() || converted->empty() || converted->size() > 2048U) {
        return {};
    }
    ObservationCommand result;
    result.text = *converted;
    if (command == L"PASS") {
        result.kind = ObservationCommandKind::Pass;
    } else if (command == L"FAIL") {
        result.kind = ObservationCommandKind::Fail;
    } else if (command == L"SPILL") {
        result.kind = ObservationCommandKind::Spill;
    } else if (command == L"CANCEL") {
        result.kind = ObservationCommandKind::Cancel;
    }
    return result;
}

[[nodiscard]] bool read_console_character(std::wstring& line, bool& complete) {
    complete = false;
    if (_kbhit() == 0) {
        return false;
    }
    const auto character = _getwch();
    if (character == 0 || character == 0xE0) {
        static_cast<void>(_getwch());
        return true;
    }
    if (character == L'\r' || character == L'\n') {
        std::wcout << L"\n";
        complete = true;
        return true;
    }
    if (character == 27) {
        line = L"CANCEL operator pressed Escape";
        std::wcout << L"\n";
        complete = true;
        return true;
    }
    if (character == L'\b') {
        if (!line.empty()) {
            line.pop_back();
            std::wcout << L"\b \b";
        }
        return true;
    }
    if (character >= L' ' && line.size() < 2200U) {
        line.push_back(static_cast<wchar_t>(character));
        std::wcout << static_cast<wchar_t>(character);
    }
    return true;
}

void print_operator_check(
    std::wstring_view context,
    const emberlights::RawHardwareTestOperatorCheck& result) {
    const auto message = wide(result.message).value_or(L"Invalid UTF-8 diagnostic");
    std::wcerr << context << L" ["
               << wide(emberlights::raw_hardware_test_operator_error_name(
                           result.error)).value_or(L"unknown")
               << L"]";
    if (result.line != 0U) {
        std::wcerr << L" line " << result.line;
    }
    std::wcerr << L": " << message << L"\n";
}

void print_plan(const emberlights::PreparedRawHardwareTestOperatorRun& prepared) {
    const auto& binding = prepared.plan.binding;
    std::wcout
        << L"\nEVIDENCE-BOUND RAW HARDWARE TEST v1\n"
        << L"Candidate: " << prepared.manifest.project_path.wstring() << L"\n"
        << L"Candidate file SHA-256: "
        << wide(prepared.candidate_file_sha256).value_or(L"invalid") << L"\n"
        << L"Candidate basis SHA-256: "
        << wide(prepared.plan.candidate_project_sha256).value_or(L"invalid") << L"\n"
        << L"Fixture/unit: " << wide(binding.fixture_id).value_or(L"invalid")
        << L" / " << wide(binding.unit_label).value_or(L"invalid") << L"\n"
        << L"Profile/mode: " << wide(binding.profile_id).value_or(L"invalid")
        << L" / " << wide(binding.mode).value_or(L"invalid") << L"\n"
        << L"Binding: universe " << static_cast<unsigned int>(binding.universe)
        << L", address " << binding.address << L", footprint "
        << prepared.plan.footprint << L", "
        << wide(binding.output_backend).value_or(L"invalid") << L"\n"
        << L"Audit v1: " << prepared.manifest.audit_path.wstring() << L"\n"
        << L"Graduated candidate (success only): "
        << prepared.manifest.graduated_project_path.wstring() << L"\n\n"
        << L"The session will emit only blackout or one-hot frames inside this exact\n"
        << L"fixture footprint. It does not compile or activate Runner, Looks, Autoloops,\n"
        << L"or the rest of the project patch. Required observations:\n";
    for (std::size_t index = 0U; index < prepared.plan.requirements.size(); ++index) {
        const auto& requirement = prepared.plan.requirements[index];
        std::wcout << L"  " << index + 1U << L". "
                   << wide(requirement.id).value_or(L"invalid") << L": "
                   << wide(requirement.expected_behavior).value_or(L"invalid");
        if (requirement.kind ==
            emberlights::FixtureQualificationRequirementKind::OneHot) {
            std::wcout << L" [DMX " << requirement.absolute_channel << L" = "
                       << static_cast<unsigned int>(requirement.value) << L"]";
        }
        std::wcout << L"\n";
    }
}

void drive_operator_session(
    emberlights::RawHardwareTestSession& session,
    emberlights::SoundSwitchMicroRawHardwareTestTransport& transport) {
    std::size_t announced_requirement = static_cast<std::size_t>(-1);
    std::wstring input;
    auto next_device_check =
        emberlights::RawHardwareTestSession::TimePoint::clock::now();
    while (session.snapshot().phase ==
           emberlights::RawHardwareTestPhase::AwaitingObservation) {
        const auto snapshot = session.snapshot();
        if (snapshot.current_requirement != announced_requirement) {
            announced_requirement = snapshot.current_requirement;
            input.clear();
            const auto* plan = session.plan();
            if (plan == nullptr ||
                announced_requirement >= plan->requirements.size()) {
                static_cast<void>(session.cancel(
                    "Operator coordinator lost its bounded plan.",
                    emberlights::RawHardwareTestSession::TimePoint::clock::now()));
                break;
            }
            const auto& requirement = plan->requirements[announced_requirement];
            std::wcout
                << L"\nObservation " << announced_requirement + 1U << L"/"
                << plan->requirements.size() << L" after blackout-before and stimulus:\n"
                << L"Expected: "
                << wide(requirement.expected_behavior).value_or(L"invalid") << L"\n"
                << L"Enter PASS <what you observed>, FAIL <what failed>,\n"
                << L"SPILL <what else responded>, or CANCEL <reason>. Escape/Ctrl+C cancels.\n> ";
        }
        const auto now = emberlights::RawHardwareTestSession::TimePoint::clock::now();
        if (cancellation_requested.load(std::memory_order_relaxed)) {
            static_cast<void>(session.cancel(
                "Operator or console requested cancellation.", now));
            break;
        }
        if (now >= next_device_check) {
            next_device_check = now + std::chrono::milliseconds(250);
            if (!find_target_device().has_value()) {
                // Make the production adapter report disconnected to the raw
                // session; that state machine records device loss and owns the
                // terminal blackout/close attempt.
                transport.close();
            }
        }
        const auto polled = session.poll(now);
        if (!polled.ok()) {
            break;
        }
        bool complete = false;
        if (read_console_character(input, complete) && complete) {
            const auto command = parse_observation_command(input);
            input.clear();
            if (command.kind == ObservationCommandKind::Cancel) {
                static_cast<void>(session.cancel(command.text, now));
            } else if (command.kind == ObservationCommandKind::Pass ||
                       command.kind == ObservationCommandKind::Fail ||
                       command.kind == ObservationCommandKind::Spill) {
                const bool passed = command.kind == ObservationCommandKind::Pass;
                const bool no_spill = command.kind != ObservationCommandKind::Spill;
                static_cast<void>(session.submit_observation(
                    {command.text, passed, no_spill}, now));
            } else {
                std::wcout << L"Invalid bounded observation command; try again.\n> ";
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

[[nodiscard]] int run_evidence_bound_active_test(
    const std::filesystem::path& manifest_path) {
    emberlights::RawHardwareTestOperatorManifest manifest;
    const auto loaded = emberlights::load_raw_hardware_test_operator_manifest(
        manifest_path, manifest);
    if (!loaded.ok()) {
        print_operator_check(L"Manifest rejected", loaded);
        return 4;
    }
    emberlights::PreparedRawHardwareTestOperatorRun prepared;
    const auto prepared_result =
        emberlights::prepare_raw_hardware_test_operator_run(manifest, prepared);
    if (!prepared_result.ok()) {
        print_operator_check(L"Qualification plan rejected", prepared_result);
        return 4;
    }
    print_plan(prepared);
    const auto acknowledgement =
        emberlights::raw_hardware_test_operator_acknowledgement(prepared);
    std::wcout
        << L"\nType this exact line to acknowledge active output to only the binding above:\n"
        << wide(acknowledgement).value_or(L"invalid") << L"\n> ";
    std::wstring response;
    std::getline(std::wcin, response);
    const auto response_utf8 = utf8(response);
    if (!response_utf8.has_value() ||
        !emberlights::raw_hardware_test_operator_acknowledged(
            prepared, *response_utf8)) {
        std::wcout << L"Acknowledgement did not match; no output device was opened.\n";
        return 0;
    }

    emberlights::PreparedRawHardwareTestOperatorRun refreshed;
    const auto refreshed_result =
        emberlights::prepare_raw_hardware_test_operator_run(manifest, refreshed);
    if (!refreshed_result.ok() ||
        emberlights::raw_hardware_test_operator_acknowledgement(refreshed) !=
            acknowledgement) {
        if (!refreshed_result.ok()) {
            print_operator_check(
                L"Pre-output revalidation rejected", refreshed_result);
        } else {
            std::wcerr
                << L"The candidate binding changed after acknowledgement; no output device was opened.\n";
        }
        return 4;
    }
    prepared = std::move(refreshed);

    cancellation_requested.store(false, std::memory_order_relaxed);
    operator_terminal_complete.store(false, std::memory_order_release);
    operator_session_active.store(true, std::memory_order_release);
    [[maybe_unused]] ConsoleControlRegistration control_registration;
    emberlights::SoundSwitchMicroRawHardwareTestTransport transport;
    emberlights::RawHardwareTestSession session;
    const auto started_at = utc_now();
    const auto begun = session.begin(
        prepared.plan,
        {prepared.manifest.operator_id, started_at},
        transport,
        emberlights::RawHardwareTestSession::TimePoint::clock::now());
    if (begun.ok()) {
        drive_operator_session(session, transport);
    }

    emberlights::RawHardwareTestOperatorCompletion completion;
    const auto finalized = emberlights::finalize_raw_hardware_test_operator_run(
        prepared, session, utc_now(), completion);
    operator_terminal_complete.store(true, std::memory_order_release);
    operator_session_active.store(false, std::memory_order_release);
    if (!finalized.ok()) {
        print_operator_check(L"Terminal evidence handling failed", finalized);
        if (completion.audit_appended) {
            std::wcerr << L"The sealed attempt remains in the append-only audit, but no project was authorized.\n";
        }
        return 7;
    }
    const auto snapshot = session.snapshot();
    std::wcout
        << L"\nTerminal phase: "
        << wide(emberlights::raw_hardware_test_phase_name(snapshot.phase)).value_or(L"unknown")
        << L"; error: "
        << wide(emberlights::raw_hardware_test_error_name(snapshot.error)).value_or(L"unknown")
        << L"\nAttempt SHA-256: "
        << wide(completion.attempt.content_sha256).value_or(L"invalid")
        << L"\nAppend-only audit v1: "
        << prepared.manifest.audit_path.wstring() << L"\n";
    if (completion.graduated) {
        std::wcout
            << L"SUCCESS: every planned observation passed and the sealed current attempt\n"
            << L"was transactionally embedded in: "
            << prepared.manifest.graduated_project_path.wstring() << L"\n"
            << L"Other fixtures or markers may still keep physical output blocked.\n";
        return 0;
    }
    std::wcerr
        << L"The attempt was audited but did not graduate or authorize a project.\n";
    return 6;
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
    const auto pass = parse_observation_command(L"PASS red only, no spill");
    const auto cancel = parse_observation_command(L"CANCEL operator stop");
    if (pass.kind != ObservationCommandKind::Pass ||
        pass.text != "red only, no spill" ||
        cancel.kind != ObservationCommandKind::Cancel ||
        parse_observation_command(L"PASS").kind !=
            ObservationCommandKind::Invalid ||
        parse_observation_command(L"YES red").kind !=
            ObservationCommandKind::Invalid) {
        return false;
    }
    return true;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc == 2 && std::wstring_view(argv[1]) == L"--self-test") {
        return self_test() ? 0 : 1;
    }
    if (argc == 2 && (std::wstring_view(argv[1]) == L"--help" ||
                      std::wstring_view(argv[1]) == L"-h")) {
        std::wcout
            << L"Usage:\n"
            << L"  soundswitch_micro_probe                 passive descriptor report\n"
            << L"  soundswitch_micro_probe --self-test     non-outputting software check\n"
            << L"  soundswitch_micro_probe --active-test [operator-manifest-v1]\n\n"
            << L"--active-test retains the installed shortcut entrypoint but now runs only\n"
            << L"the evidence-bound one-fixture Raw Hardware Test. It never invokes Runner.\n";
        return 0;
    }

    const bool active_test =
        (argc == 2 || argc == 3) &&
        std::wstring_view(argv[1]) == L"--active-test";
    if ((argc != 1 && !active_test) || argc > 3) {
        std::wcerr << L"Unknown arguments. Use --help. No output was sent.\n";
        return 1;
    }

    std::wcout
        << L"EmberLights Hardware Test\n"
        << L"=========================\n\n"
        << L"This tool only opens the SoundSwitch Micro device (VID 15E4, PID 0053).\n"
        << L"Descriptor collection is passive. EmberLights itself now owns the normal\n"
        << L"full-universe output path; this tool does not transmit unless launched\n"
        << L"with --active-test, a valid one-fixture manifest, and an exact typed\n"
        << L"acknowledgement. Default and --self-test never transmit.\n\n"
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

    std::filesystem::path manifest_path;
    if (argc == 3) {
        manifest_path = argv[2];
    } else {
        std::wcout
            << L"\nACTIVE TEST SETUP\n"
            << L"1. Fully close SoundSwitch and EmberLights.\n"
            << L"2. Isolate exactly the physical fixture/unit named by a reviewed v1 manifest.\n"
            << L"3. Disconnect hazardous and unrelated fixtures/effects.\n"
            << L"4. Confirm mode, address, universe, backend, and every slot criterion.\n\n"
            << L"Operator manifest v1 path (blank cancels without output): ";
        std::wstring entered;
        std::getline(std::wcin, entered);
        if (entered.empty()) {
            std::wcout << L"Active test cancelled; no output device was opened.\n";
            return 0;
        }
        manifest_path = entered;
    }
    const auto result = run_evidence_bound_active_test(manifest_path);
    std::wcout << L"Press Enter to close.";
    std::wstring ignored;
    std::getline(std::wcin, ignored);
    return result;
}
