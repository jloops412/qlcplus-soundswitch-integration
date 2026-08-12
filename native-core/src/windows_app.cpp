#include "emberlights/compiler.hpp"
#include "emberlights/audio_assets.hpp"
#include "emberlights/connection_layout.hpp"
#include "emberlights/fixture_capabilities.hpp"
#include "emberlights/project.hpp"
#include "emberlights/project_edit_history.hpp"
#include "emberlights/project_io.hpp"
#include "emberlights/qlc_fixture_import.hpp"
#include "emberlights/live_view_model.hpp"
#include "emberlights/runner.hpp"
#include "emberlights/static_look_authoring.hpp"
#include "emberlights/static_look_preview.hpp"
#include "emberlights/ui_command.hpp"
#include "emberlights/ui_state.hpp"
#include "emberlights/soundswitch_import.hpp"
#include "emberlights/version.hpp"
#include "showcore/dmx_usb_pro.hpp"
#include "showcore/number_chars.hpp"
#include "showcore/winmm_midi.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

constexpr wchar_t kWindowClass[] = L"EmberLightsMainWindow";
constexpr wchar_t kPageClass[] = L"EmberLightsPage";
constexpr wchar_t kInstanceMutex[] =
    L"Local\\EmberLights-5AE71134-902A-4E44-AF80-ADCC47F15DA9";
constexpr ULONG_PTR kOpenProjectCopyData = 0x454D4245U;
constexpr UINT_PTR kStatusTimer = 1U;
constexpr UINT kStatusTimerMs = 250U;
constexpr int kNavigationWidth = 176;
constexpr int kStatusHeight = 26;
constexpr wchar_t kApplicationRegistryKey[] = L"Software\\EmberLights";
constexpr wchar_t kLastProjectRegistryValue[] = L"LastProjectPath";

[[nodiscard]] std::optional<std::filesystem::path> remembered_project_path() {
    HKEY key = nullptr;
    if (::RegOpenKeyExW(
            HKEY_CURRENT_USER, kApplicationRegistryKey, 0U, KEY_QUERY_VALUE, &key) !=
        ERROR_SUCCESS) {
        return std::nullopt;
    }
    DWORD type = 0U;
    DWORD bytes = 0U;
    const auto measured = ::RegQueryValueExW(
        key, kLastProjectRegistryValue, nullptr, &type, nullptr, &bytes);
    if (measured != ERROR_SUCCESS || type != REG_SZ || bytes < sizeof(wchar_t) ||
        bytes > 32768U * sizeof(wchar_t)) {
        ::RegCloseKey(key);
        return std::nullopt;
    }
    std::vector<wchar_t> value(bytes / sizeof(wchar_t), L'\0');
    const auto loaded = ::RegQueryValueExW(
        key,
        kLastProjectRegistryValue,
        nullptr,
        &type,
        reinterpret_cast<BYTE*>(value.data()),
        &bytes);
    ::RegCloseKey(key);
    if (loaded != ERROR_SUCCESS || value.empty() || value.back() != L'\0' ||
        value.front() == L'\0') {
        return std::nullopt;
    }
    return std::filesystem::path(value.data());
}

[[nodiscard]] bool remember_project_path(const std::filesystem::path& path) {
    if (path.empty()) {
        return false;
    }
    HKEY key = nullptr;
    if (::RegCreateKeyExW(
            HKEY_CURRENT_USER,
            kApplicationRegistryKey,
            0U,
            nullptr,
            REG_OPTION_NON_VOLATILE,
            KEY_SET_VALUE,
            nullptr,
            &key,
            nullptr) != ERROR_SUCCESS) {
        return false;
    }
    const auto value = path.wstring();
    const auto bytes = static_cast<DWORD>((value.size() + 1U) * sizeof(wchar_t));
    const auto saved = ::RegSetValueExW(
        key,
        kLastProjectRegistryValue,
        0U,
        REG_SZ,
        reinterpret_cast<const BYTE*>(value.c_str()),
        bytes);
    ::RegCloseKey(key);
    return saved == ERROR_SUCCESS;
}

void forget_remembered_project_path() noexcept {
    HKEY key = nullptr;
    if (::RegOpenKeyExW(
            HKEY_CURRENT_USER, kApplicationRegistryKey, 0U, KEY_SET_VALUE, &key) ==
        ERROR_SUCCESS) {
        static_cast<void>(::RegDeleteValueW(key, kLastProjectRegistryValue));
        ::RegCloseKey(key);
    }
}

enum class Page : std::size_t {
    Live,
    Overrides,
    Profiles,
    Patch,
    Groups,
    Looks,
    Autoloops,
    Tracks,
    Midi,
    Connections,
    Safety,
    Diagnostics,
    Count
};

enum ControlId : int {
    IdFileNew = 100,
    IdFileOpen,
    IdFileSave,
    IdFileSaveAs,
    IdFileRestoreHistory,
    IdFileInspectSoundSwitch,
    IdFileCompareSoundSwitch,
    IdFileBundleSoundSwitch,
    IdFileExit,
    IdEditUndo = 110,
    IdEditRedo,
    IdShowValidate = 120,
    IdShowStartStop,
    IdHelpAbout = 140,

    IdNavLive = 200,
    IdNavOverrides,
    IdNavProfiles,
    IdNavPatch,
    IdNavGroups,
    IdNavLooks,
    IdNavAutoloops,
    IdNavTracks,
    IdNavMidi,
    IdNavConnections,
    IdNavSafety,
    IdNavDiagnostics,

    IdLiveTitle = 1000,
    IdLiveState,
    IdLiveStartStop,
    IdLiveBlackout,
    IdLiveWorkLight,
    IdLiveBpm,
    IdLiveApplyBpm,
    IdLiveTap,
    IdLiveLooks,
    IdLiveTriggerLook,
    IdLiveClearLook,
    IdLiveAutoloops,
    IdLiveTriggerAutoloop,
    IdLivePreviousAutoloop,
    IdLiveNextAutoloop,
    IdLiveClearAutoloop,
    IdLiveTrackLabel,
    IdLiveTracks,
    IdLiveTriggerTrack,
    IdLiveClearTrack,
    IdLiveFogArm,
    IdLiveHazeArm,
    IdLiveLaserArm,
    IdLiveSparkArm,
    IdLiveMetrics,
    IdLiveAutoloopBankPage,
    IdLivePreviousAutoloopBankPage,
    IdLiveNextAutoloopBankPage,
    IdLiveSelectAllAutoloopBanks,
    IdLiveAutoloopBank1,
    IdLiveAutoloopBank1Only,
    IdLiveAutoloopBank2,
    IdLiveAutoloopBank2Only,
    IdLiveAutoloopBank3,
    IdLiveAutoloopBank3Only,
    IdLiveAutoloopBank4,
    IdLiveAutoloopBank4Only,
    IdLiveAutoloopPlayback,

    IdOverridesTitle = 1500,
    IdOverridesFixture,
    IdOverridesProperty,
    IdOverridesValue,
    IdOverridesApply,
    IdOverridesRelease,
    IdOverridesReleaseAll,
    IdOverridesHelp,
    IdOverridesActiveCount,
    IdOverridesMessage,

    IdProfileTitle = 2000,
    IdProfileList,
    IdProfileImportQlc,
    IdProfileNew,
    IdProfileDuplicate,
    IdProfileSave,
    IdProfileDelete,
    IdProfileManufacturer,
    IdProfileModel,
    IdProfileMode,
    IdProfileName,
    IdProfileFootprint,
    IdProfileChannels,
    IdProfileHelp,
    IdProfileMessage,

    IdPatchTitle = 3000,
    IdPatchList,
    IdPatchNew,
    IdPatchSave,
    IdPatchDelete,
    IdPatchName,
    IdPatchProfile,
    IdPatchUniverse,
    IdPatchAddress,
    IdPatchRoles,
    IdPatchMessage,

    IdGroupTitle = 3500,
    IdGroupList,
    IdGroupNew,
    IdGroupDuplicate,
    IdGroupSave,
    IdGroupDelete,
    IdGroupName,
    IdGroupMembers,
    IdGroupHelp,
    IdGroupMessage,

    IdLookTitle = 4000,
    IdLookList,
    IdLookNew,
    IdLookDuplicate,
    IdLookSave,
    IdLookDelete,
    IdLookName,
    IdLookFade,
    IdLookTarget,
    IdLookCapabilities,
    IdLookRgbHex,
    IdLookPickRgb,
    IdLookRed,
    IdLookGreen,
    IdLookBlue,
    IdLookWhite,
    IdLookAmber,
    IdLookUv,
    IdLookIntensity,
    IdLookApplyColor,
    IdLookSwatchRed,
    IdLookSwatchGreen,
    IdLookSwatchBlue,
    IdLookSwatchWhite,
    IdLookSwatchAmber,
    IdLookSwatchUv,
    IdLookSwatchBlack,
    IdLookProperty,
    IdLookOwnership,
    IdLookValue,
    IdLookApplyProperty,
    IdLookRemoveProperty,
    IdLookAssignments,
    IdLookPreview,
    IdLookPreviewText,
    IdLookHelp,
    IdLookMessage,

    IdAutoloopTitle = 5000,
    IdAutoloopList,
    IdAutoloopNew,
    IdAutoloopDuplicate,
    IdAutoloopSave,
    IdAutoloopDelete,
    IdAutoloopNextEmpty,
    IdAutoloopSwapTarget,
    IdAutoloopName,
    IdAutoloopBank,
    IdAutoloopSlot,
    IdAutoloopLength,
    IdAutoloopRepeat,
    IdAutoloopSteps,
    IdAutoloopHelp,
    IdAutoloopMessage,

    IdTrackTitle = 5500,
    IdTrackList,
    IdTrackNew,
    IdTrackDuplicate,
    IdTrackSave,
    IdTrackDelete,
    IdTrackName,
    IdTrackAudioAsset,
    IdTrackAddAudio,
    IdTrackRelinkAudio,
    IdTrackVerifyAudio,
    IdTrackResolveAudioFolder,
    IdTrackAudioKey,
    IdTrackCues,
    IdTrackHelp,
    IdTrackMessage,

    IdMidiTitle = 6000,
    IdMidiList,
    IdMidiAction,
    IdMidiTarget,
    IdMidiProperty,
    IdMidiBehavior,
    IdMidiSoftTakeover,
    IdMidiLearn,
    IdMidiDelete,
    IdMidiMessage,

    IdConnectionsTitle = 7000,
    IdProjectName,
    IdOs2lEnabled,
    IdOs2lBind,
    IdOs2lPort,
    IdArtnetEnabled,
    IdArtnetDestination,
    IdArtnetBase,
    IdSacnEnabled,
    IdSacnDestination,
    IdSacnBase,
    IdDmxUsbProUniverse1,
    IdDmxUsbProUniverse2,
    IdSoundSwitchMicroUniverse,
    IdSoundSwitchMicroFraming,
    IdSoundSwitchControlOneMode,
    IdFrameRate,
    IdManualBpm,
    IdMidiInput,
    IdMidiOutput,
    IdRefreshMidi,
    IdCopyVirtualDjSetup,
    IdConnectionsApply,
    IdConnectionsMessage,

    IdSafetyTitle = 7500,
    IdSafetyFogArm,
    IdSafetyHazeArm,
    IdSafetyLaserArm,
    IdSafetySparkArm,
    IdSafetyStrobeAllowed,
    IdSafetyMaxStrobe,
    IdSafetyMaxIntensity,
    IdSafetyApply,
    IdSafetyMessage,

    IdDiagnosticsTitle = 8000,
    IdDiagnosticsText,
    IdDiagnosticsCopy,
    IdDiagnosticsExport,
    IdDiagnosticsValidate
};

constexpr std::array<int, emberlights::kConnectionLayoutItemCount>
    kConnectionLayoutControlIds{{
        IdConnectionsTitle,
        0,
        IdProjectName,
        IdOs2lEnabled,
        0,
        IdOs2lBind,
        0,
        IdOs2lPort,
        IdArtnetEnabled,
        0,
        IdArtnetDestination,
        0,
        IdArtnetBase,
        IdSacnEnabled,
        0,
        IdSacnDestination,
        0,
        IdSacnBase,
        0,
        IdDmxUsbProUniverse1,
        0,
        IdDmxUsbProUniverse2,
        0,
        IdSoundSwitchMicroUniverse,
        0,
        IdSoundSwitchMicroFraming,
        0,
        IdSoundSwitchControlOneMode,
        0,
        IdFrameRate,
        0,
        IdManualBpm,
        0,
        IdMidiInput,
        0,
        IdMidiOutput,
        IdRefreshMidi,
        IdCopyVirtualDjSetup,
        IdConnectionsApply,
        IdConnectionsMessage,
    }};

[[nodiscard]] HMENU control_menu(int id) noexcept {
    return reinterpret_cast<HMENU>(static_cast<INT_PTR>(id));
}

[[nodiscard]] bool is_authoring_edit_command(int id) noexcept {
    switch (id) {
    case IdProfileImportQlc:
    case IdProfileSave:
    case IdProfileDelete:
    case IdPatchSave:
    case IdPatchDelete:
    case IdGroupSave:
    case IdGroupDelete:
    case IdLookSave:
    case IdLookDelete:
    case IdAutoloopSave:
    case IdAutoloopDelete:
    case IdAutoloopNextEmpty:
    case IdAutoloopSwapTarget:
    case IdTrackSave:
    case IdTrackDelete:
    case IdTrackAddAudio:
    case IdTrackRelinkAudio:
    case IdTrackResolveAudioFolder:
    case IdMidiDelete:
    case IdConnectionsApply:
    case IdSafetyApply:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] std::wstring widen(std::string_view value) {
    if (value.empty()) {
        return {};
    }
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return {};
    }
    const auto source_length = static_cast<int>(value.size());
    const auto length = ::MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), source_length, nullptr, 0);
    if (length <= 0) {
        return {};
    }
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    static_cast<void>(::MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), source_length, result.data(), length));
    return result;
}

[[nodiscard]] std::string narrow(std::wstring_view value) {
    if (value.empty()) {
        return {};
    }
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return {};
    }
    const auto source_length = static_cast<int>(value.size());
    const auto length = ::WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), source_length, nullptr, 0, nullptr, nullptr);
    if (length <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(length), '\0');
    static_cast<void>(::WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value.data(),
        source_length,
        result.data(),
        length,
        nullptr,
        nullptr));
    return result;
}

[[nodiscard]] std::string control_text(HWND control) {
    const auto length = ::GetWindowTextLengthW(control);
    if (length <= 0) {
        return {};
    }
    std::wstring value(static_cast<std::size_t>(length) + 1U, L'\0');
    const auto copied = ::GetWindowTextW(control, value.data(), length + 1);
    value.resize(copied > 0 ? static_cast<std::size_t>(copied) : 0U);
    return narrow(value);
}

void set_control_text(HWND control, std::string_view value) {
    const auto wide = widen(value);
    static_cast<void>(::SetWindowTextW(control, wide.c_str()));
}

[[nodiscard]] std::string trim(std::string_view value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return std::string(value.substr(first, last - first + 1U));
}

[[nodiscard]] std::string normalize_newlines(std::string_view value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (const auto character : value) {
        if (character != '\r') {
            normalized.push_back(character);
        }
    }
    return normalized;
}

template <typename Value>
[[nodiscard]] bool parse_number(std::string_view text, Value& value) noexcept {
    const auto cleaned = trim(text);
    if (cleaned.empty()) {
        return false;
    }
    Value parsed{};
    const auto result = showcore::parse_number_chars(
        cleaned.data(), cleaned.data() + cleaned.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != cleaned.data() + cleaned.size()) {
        return false;
    }
    value = parsed;
    return true;
}

template <typename Value>
[[nodiscard]] std::string number_text(Value value) {
    std::array<char, 64> buffer{};
    auto result = [&]() {
        if constexpr (std::is_floating_point_v<Value>) {
            return std::to_chars(
                buffer.data(),
                buffer.data() + buffer.size(),
                value,
                std::chars_format::general,
                std::numeric_limits<Value>::max_digits10);
        } else {
            return std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
        }
    }();
    return result.ec == std::errc{} ? std::string(buffer.data(), result.ptr) : std::string{};
}

[[nodiscard]] std::vector<std::string> lines(std::string_view value) {
    std::vector<std::string> result;
    std::size_t offset = 0;
    while (offset <= value.size()) {
        const auto end = value.find('\n', offset);
        const auto limit = end == std::string_view::npos ? value.size() : end;
        auto line = trim(value.substr(offset, limit - offset));
        if (!line.empty() && line.front() != '#') {
            result.push_back(std::move(line));
        }
        if (end == std::string_view::npos) {
            break;
        }
        offset = end + 1U;
    }
    return result;
}

[[nodiscard]] std::vector<std::string> split_csv(std::string_view value) {
    std::vector<std::string> fields;
    std::size_t offset = 0;
    while (offset <= value.size()) {
        const auto comma = value.find(',', offset);
        const auto end = comma == std::string_view::npos ? value.size() : comma;
        fields.push_back(trim(value.substr(offset, end - offset)));
        if (comma == std::string_view::npos) {
            break;
        }
        offset = comma + 1U;
    }
    return fields;
}

[[nodiscard]] std::string slugify(std::string_view value) {
    std::string slug;
    bool separator = false;
    for (const auto character : value) {
        const auto unsigned_character = static_cast<unsigned char>(character);
        if ((unsigned_character >= 'A' && unsigned_character <= 'Z') ||
            (unsigned_character >= 'a' && unsigned_character <= 'z') ||
            (unsigned_character >= '0' && unsigned_character <= '9')) {
            if (separator && !slug.empty()) {
                slug.push_back('-');
            }
            separator = false;
            slug.push_back(static_cast<char>(
                unsigned_character >= 'A' && unsigned_character <= 'Z'
                    ? unsigned_character + ('a' - 'A')
                    : unsigned_character));
        } else {
            separator = true;
        }
    }
    if (slug.empty()) {
        slug = "item";
    }
    if (slug.size() > 72U) {
        slug.resize(72U);
    }
    return slug;
}

[[nodiscard]] const wchar_t* runner_state_name(emberlights::RunnerState state) noexcept {
    switch (state) {
    case emberlights::RunnerState::Stopped: return L"Stopped";
    case emberlights::RunnerState::Starting: return L"Starting";
    case emberlights::RunnerState::Running: return L"Running";
    case emberlights::RunnerState::Stopping: return L"Stopping";
    case emberlights::RunnerState::Fault: return L"Fault";
    }
    return L"Unknown";
}

[[nodiscard]] const wchar_t* adapter_state_name(emberlights::AdapterState state) noexcept {
    switch (state) {
    case emberlights::AdapterState::Disabled: return L"Off";
    case emberlights::AdapterState::Starting: return L"Starting";
    case emberlights::AdapterState::Waiting: return L"Waiting";
    case emberlights::AdapterState::Ready: return L"Ready";
    case emberlights::AdapterState::Fault: return L"Fault";
    }
    return L"Unknown";
}

[[nodiscard]] const wchar_t* soundswitch_micro_framing_name(
    showcore::SoundSwitchMicroFraming framing) noexcept {
    switch (framing) {
    case showcore::SoundSwitchMicroFraming::NativeJls1: return L"native JLS1";
    }
    return L"invalid";
}

[[nodiscard]] const wchar_t* soundswitch_micro_state_name(
    emberlights::AdapterState state) noexcept {
    return state == emberlights::AdapterState::Ready ? L"Open" : adapter_state_name(state);
}

[[nodiscard]] const wchar_t* autoloop_repeat_name(showcore::AutoloopRepeat repeat) noexcept {
    switch (repeat) {
    case showcore::AutoloopRepeat::Once: return L"once";
    case showcore::AutoloopRepeat::Infinite: return L"infinite";
    case showcore::AutoloopRepeat::TrackDuration: return L"track duration";
    }
    return L"unknown repeat";
}

[[nodiscard]] std::optional<std::filesystem::path> choose_folder(
    HWND owner,
    const wchar_t* title) {
    IFileOpenDialog* dialog = nullptr;
    const auto created = ::CoCreateInstance(
        CLSID_FileOpenDialog,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&dialog));
    if (FAILED(created) || dialog == nullptr) {
        return std::nullopt;
    }
    DWORD options = 0U;
    auto result = dialog->GetOptions(&options);
    if (SUCCEEDED(result)) {
        result = dialog->SetOptions(
            options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST |
            FOS_DONTADDTORECENT);
    }
    if (SUCCEEDED(result)) {
        result = dialog->SetTitle(title);
    }
    if (SUCCEEDED(result)) {
        result = dialog->Show(owner);
    }
    IShellItem* item = nullptr;
    if (SUCCEEDED(result)) {
        result = dialog->GetResult(&item);
    }
    PWSTR selected = nullptr;
    if (SUCCEEDED(result) && item != nullptr) {
        result = item->GetDisplayName(SIGDN_FILESYSPATH, &selected);
    }
    std::optional<std::filesystem::path> path;
    if (SUCCEEDED(result) && selected != nullptr) {
        path = std::filesystem::path(selected);
    }
    if (selected != nullptr) {
        ::CoTaskMemFree(selected);
    }
    if (item != nullptr) {
        item->Release();
    }
    dialog->Release();
    return path;
}

class Application final : public emberlights::UiAppCommandHost {
public:
    explicit Application(HINSTANCE instance) noexcept : instance_(instance) {}
    ~Application() noexcept;

    int run(
        int show_command,
        const std::optional<std::filesystem::path>& initial_file,
        bool startup_smoke);

private:
    static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    static LRESULT CALLBACK page_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT handle_message(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

    bool register_classes();
    bool create_window(int show_command);
    [[nodiscard]] bool window_tree_ready() const noexcept;
    void create_menu_bar();
    void create_navigation();
    void create_pages();
    HWND create_page(Page page);
    HWND add_control(
        HWND parent,
        const wchar_t* class_name,
        const wchar_t* text,
        DWORD style,
        DWORD extended_style,
        int id);
    HWND add_label(HWND parent, const wchar_t* text, int id);
    HWND add_edit(HWND parent, int id, bool multiline = false, bool read_only = false);
    HWND add_button(HWND parent, const wchar_t* text, int id, DWORD extra_style = 0);
    HWND add_combo(HWND parent, int id);
    HWND add_listbox(HWND parent, int id);
    HWND add_listview(HWND parent, int id);
    void add_listview_column(HWND list, int column, int width, const wchar_t* title);

    void layout();
    void layout_page(Page page, int width, int height);
    void layout_connections();
    void scroll_connections(UINT scroll_code);
    void scroll_connections_wheel(short wheel_delta);
    void reveal_connections_focus();
    void show_page(Page page);
    void update_title();
    void update_edit_menu();
    void reset_authoring_selection();
    void capture_saved_project();
    void record_project_edit(const emberlights::ProjectDocument& before);
    void undo_project_edit();
    void redo_project_edit();
    void mark_dirty();
    void set_status(std::wstring text);
    void set_page_message(Page page, int id, std::string_view message, bool error = false);

    void refresh_all();
    void refresh_live_lists();
    void refresh_live_status();
    void refresh_overrides();
    void refresh_override_properties();
    void refresh_profiles();
    void refresh_patch();
    void refresh_groups();
    void refresh_looks();
    void refresh_look_targets();
    void refresh_look_capabilities();
    void refresh_look_draft_view();
    void refresh_autoloops();
    void refresh_tracks();
    void refresh_track_audio_assets(std::string_view selected_asset_id);
    void refresh_midi();
    void refresh_connections();
    void refresh_safety();
    void refresh_midi_ports();
    void refresh_diagnostics();

    void handle_command(int id, int notification, HWND source);
    void handle_notify(const NMHDR& notification);
    void handle_timer();

    bool maybe_save_changes();
    void new_project();
    void open_project_dialog();
    void restore_project_history_dialog();
    void import_qlc_fixture_dialog();
    void inspect_soundswitch_dialog();
    void compare_soundswitch_dialog();
    void bundle_soundswitch_dialog();
    bool open_project(const std::filesystem::path& path);
    bool save_project(bool save_as);
    void validate_project(bool show_success);
    void start_or_stop_show();
    [[nodiscard]] emberlights::UiInvocationResult ui_start_show() noexcept override;
    [[nodiscard]] emberlights::UiInvocationResult ui_stop_show() noexcept override;
    void apply_fixture_override(bool active);
    void clear_fixture_overrides();

    void select_profile(std::int32_t index);
    void new_profile();
    void duplicate_profile();
    void save_profile();
    void delete_profile();
    void select_fixture(std::int32_t index);
    void new_fixture();
    void save_fixture();
    void delete_fixture();
    void select_group(std::int32_t index);
    void new_group();
    void duplicate_group();
    void save_group();
    void delete_group();
    void select_look(std::int32_t index);
    void new_look();
    void duplicate_look();
    void save_look();
    void delete_look();
    [[nodiscard]] std::string selected_look_target_id() const;
    [[nodiscard]] bool read_static_look_color(
        emberlights::StaticLookColor& color,
        std::string& error_message) const;
    void pick_static_look_rgb();
    void apply_static_look_color();
    void apply_static_look_swatch(std::string_view swatch_id);
    void apply_static_look_property();
    void remove_static_look_property();
    void preview_static_look();
    void select_autoloop(std::int32_t index);
    void new_autoloop();
    void duplicate_autoloop();
    void save_autoloop();
    void delete_autoloop();
    void move_autoloop_to_next_empty();
    void swap_autoloop_into_target_slot();
    [[nodiscard]] bool autoloop_editor_has_unsaved_content() const;
    void select_track(std::int32_t index);
    void new_track();
    void duplicate_track();
    void save_track();
    void delete_track();
    void import_audio_for_track(bool relink);
    void verify_selected_audio_for_track();
    void resolve_audio_assets_for_project();

    void apply_connections();
    void copy_virtualdj_setup();
    void apply_safety();
    void update_midi_targets();
    void begin_midi_learn();
    void finish_midi_learn(const showcore::MidiMessage& message);
    void delete_midi_mapping();

    [[nodiscard]] std::string unique_id(std::string_view prefix, std::string_view name) const;
    [[nodiscard]] std::string diagnostics_text() const;
    [[nodiscard]] bool copy_text_to_clipboard(std::wstring_view text);
    [[nodiscard]] bool copy_diagnostics_to_clipboard();
    [[nodiscard]] bool save_diagnostics_report();
    [[nodiscard]] const emberlights::ProjectDocument& live_project() const noexcept;

    HINSTANCE instance_{nullptr};
    HWND window_{nullptr};
    HWND status_bar_{nullptr};
    HFONT normal_font_{nullptr};
    HFONT title_font_{nullptr};
    HBRUSH background_brush_{nullptr};
    HBRUSH surface_brush_{nullptr};
    HBRUSH field_brush_{nullptr};
    std::array<HWND, static_cast<std::size_t>(Page::Count)> pages_{};
    std::array<HWND, static_cast<std::size_t>(Page::Count)> navigation_{};
    std::array<std::vector<HWND>, static_cast<std::size_t>(Page::Count)> page_controls_{};
    Page active_page_{Page::Live};
    emberlights::ConnectionLayout connections_layout_{};
    std::int32_t connections_scroll_offset_{0};
    int connections_wheel_delta_{0};
    HWND last_connections_focus_{nullptr};

    emberlights::ProjectDocument project_{emberlights::make_starter_project()};
    emberlights::ProjectEditHistory edit_history_{};
    std::string saved_project_serialized_{};
    std::filesystem::path current_path_{};
    bool dirty_{false};
    bool recovery_save_required_{false};
    bool refreshing_{false};
    std::int32_t profile_index_{-1};
    std::int32_t fixture_index_{-1};
    std::int32_t group_index_{-1};
    std::int32_t look_index_{-1};
    std::optional<emberlights::StaticLookDraft> look_draft_{};
    std::int32_t autoloop_index_{-1};
    std::int32_t track_index_{-1};
    std::uint16_t live_autoloop_bank_page_{0U};

    emberlights::RunnerService runner_{};
    emberlights::UiCommandFacade ui_commands_{runner_, *this};
    emberlights::LiveViewModel live_view_model_{};
    std::optional<emberlights::ProjectDocument> active_project_{};
    showcore::WinMmMidiInput learn_input_{};
    bool midi_learning_{false};
    bool learn_uses_runner_{false};
    showcore::MidiPortList midi_inputs_{};
    showcore::MidiPortList midi_outputs_{};
    showcore::DmxSerialPortList dmx_serial_ports_{};
};

Application::~Application() noexcept {
    runner_.stop();
    learn_input_.close_all();
    if (normal_font_ != nullptr) {
        static_cast<void>(::DeleteObject(normal_font_));
    }
    if (title_font_ != nullptr) {
        static_cast<void>(::DeleteObject(title_font_));
    }
    // background_brush_ is registered as the background brush for our window
    // classes. Win32 owns a class brush until class/process teardown; deleting
    // it here would leave the registered classes holding an invalid HBRUSH.
    if (surface_brush_ != nullptr) {
        static_cast<void>(::DeleteObject(surface_brush_));
    }
    if (field_brush_ != nullptr) {
        static_cast<void>(::DeleteObject(field_brush_));
    }
}

bool Application::register_classes() {
    background_brush_ = ::CreateSolidBrush(RGB(13, 17, 23));
    surface_brush_ = ::CreateSolidBrush(RGB(22, 27, 34));
    field_brush_ = ::CreateSolidBrush(RGB(30, 36, 45));
    if (background_brush_ == nullptr || surface_brush_ == nullptr ||
        field_brush_ == nullptr) {
        return false;
    }
    WNDCLASSEXW main_class{};
    main_class.cbSize = sizeof(main_class);
    main_class.style = CS_HREDRAW | CS_VREDRAW;
    main_class.lpfnWndProc = &Application::window_proc;
    main_class.hInstance = instance_;
    main_class.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    main_class.hIcon = ::LoadIconW(nullptr, IDI_APPLICATION);
    main_class.hbrBackground = background_brush_;
    main_class.lpszClassName = kWindowClass;
    if (::RegisterClassExW(&main_class) == 0U) {
        return false;
    }

    WNDCLASSEXW page_class{};
    page_class.cbSize = sizeof(page_class);
    page_class.lpfnWndProc = &Application::page_proc;
    page_class.hInstance = instance_;
    page_class.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    page_class.hbrBackground = background_brush_;
    page_class.lpszClassName = kPageClass;
    return ::RegisterClassExW(&page_class) != 0U;
}

int Application::run(
    int show_command,
    const std::optional<std::filesystem::path>& initial_file,
    bool startup_smoke) {
    saved_project_serialized_ = emberlights::serialize_project(project_);
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES};
    const wchar_t* failed_stage = nullptr;
    DWORD error = ERROR_SUCCESS;
    if (!::InitCommonControlsEx(&controls)) {
        failed_stage = L"loading Windows controls";
        error = ::GetLastError();
    } else if (!register_classes()) {
        failed_stage = L"registering the application window";
        error = ::GetLastError();
    } else if (!create_window(show_command)) {
        failed_stage = L"creating the application window";
        error = ::GetLastError();
    }
    if (failed_stage != nullptr) {
        std::wostringstream message;
        message << L"EmberLights could not initialize while " << failed_stage << L".\n\n"
                << L"Windows error " << error
                << L". Please include this number when reporting the problem.";
        if (!startup_smoke) {
            ::MessageBoxW(
                nullptr,
                message.str().c_str(),
                L"EmberLights startup error",
                MB_OK | MB_ICONERROR);
        }
        return EXIT_FAILURE;
    }
    if (startup_smoke) {
        ::DestroyWindow(window_);
        window_ = nullptr;
        return EXIT_SUCCESS;
    }
    if (initial_file.has_value()) {
        static_cast<void>(open_project(*initial_file));
    } else if (const auto remembered = remembered_project_path(); remembered.has_value()) {
        if (!open_project(*remembered)) {
            forget_remembered_project_path();
            set_status(
                L"The last project is no longer available. Choose another project once; "
                L"EmberLights will remember it.");
        }
    }
    MSG message{};
    std::array<ACCEL, 7> accelerator_definitions{{
        {static_cast<BYTE>(FVIRTKEY | FCONTROL), static_cast<WORD>('N'), IdFileNew},
        {static_cast<BYTE>(FVIRTKEY | FCONTROL), static_cast<WORD>('O'), IdFileOpen},
        {static_cast<BYTE>(FVIRTKEY | FCONTROL), static_cast<WORD>('S'), IdFileSave},
        {static_cast<BYTE>(FVIRTKEY | FCONTROL), static_cast<WORD>('Z'), IdEditUndo},
        {static_cast<BYTE>(FVIRTKEY | FCONTROL), static_cast<WORD>('Y'), IdEditRedo},
        {FVIRTKEY, VK_F5, IdShowStartStop},
        {FVIRTKEY, VK_F8, IdLiveBlackout}}};
    const auto accelerators = ::CreateAcceleratorTableW(
        accelerator_definitions.data(), static_cast<int>(accelerator_definitions.size()));
    ACCEL connections_apply_definition{
        static_cast<BYTE>(FVIRTKEY | FALT),
        static_cast<WORD>('A'),
        IdConnectionsApply};
    const auto connections_accelerator = ::CreateAcceleratorTableW(
        &connections_apply_definition, 1);
    while (::GetMessageW(&message, nullptr, 0, 0) > 0) {
        const auto connections_accelerator_handled =
            connections_accelerator != nullptr &&
            emberlights::connection_layout_keyboard_action(
                active_page_ == Page::Connections,
                emberlights::ConnectionKeyboardIntent::AltApply) ==
                emberlights::ConnectionLayoutAction::SaveAndApply &&
            ::TranslateAcceleratorW(
                window_, connections_accelerator, &message) != 0;
        if (!connections_accelerator_handled &&
            (accelerators == nullptr ||
             ::TranslateAcceleratorW(window_, accelerators, &message) == 0) &&
            !::IsDialogMessageW(window_, &message)) {
            ::TranslateMessage(&message);
            ::DispatchMessageW(&message);
        }
        reveal_connections_focus();
    }
    if (accelerators != nullptr) {
        ::DestroyAcceleratorTable(accelerators);
    }
    if (connections_accelerator != nullptr) {
        ::DestroyAcceleratorTable(connections_accelerator);
    }
    return static_cast<int>(message.wParam);
}

bool Application::create_window(int show_command) {
    normal_font_ = ::CreateFontW(
        -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    title_font_ = ::CreateFontW(
        -27, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    if (normal_font_ == nullptr || title_font_ == nullptr) {
        return false;
    }
    window_ = ::CreateWindowExW(
        0,
        kWindowClass,
        L"EmberLights",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1280,
        820,
        nullptr,
        nullptr,
        instance_,
        this);
    if (window_ == nullptr) {
        return false;
    }
    create_menu_bar();
    create_navigation();
    create_pages();
    status_bar_ = add_label(window_, L"Ready", 0);
    if (!window_tree_ready()) {
        ::DestroyWindow(window_);
        window_ = nullptr;
        ::SetLastError(ERROR_INVALID_WINDOW_HANDLE);
        return false;
    }
    static_cast<void>(::SetTimer(window_, kStatusTimer, kStatusTimerMs, nullptr));
    refresh_midi_ports();
    refresh_all();
    show_page(Page::Live);
    layout();
    ::ShowWindow(window_, show_command);
    static_cast<void>(::UpdateWindow(window_));
    return true;
}

bool Application::window_tree_ready() const noexcept {
    if (window_ == nullptr || status_bar_ == nullptr) {
        return false;
    }
    if (std::any_of(pages_.begin(), pages_.end(), [](HWND page) { return page == nullptr; }) ||
        std::any_of(
            navigation_.begin(), navigation_.end(), [](HWND navigation) { return navigation == nullptr; })) {
        return false;
    }
    const auto& connection_controls =
        page_controls_[static_cast<std::size_t>(Page::Connections)];
    if (connection_controls.size() != kConnectionLayoutControlIds.size() ||
        !std::equal(
            connection_controls.begin(),
            connection_controls.end(),
            kConnectionLayoutControlIds.begin(),
            [](HWND control, int expected_id) {
                return ::GetDlgCtrlID(control) == expected_id;
            })) {
        return false;
    }
    constexpr std::array<std::pair<Page, int>, 12> critical_controls{{
        {Page::Live, IdLiveStartStop},
        {Page::Overrides, IdOverridesApply},
        {Page::Profiles, IdProfileList},
        {Page::Patch, IdPatchList},
        {Page::Groups, IdGroupList},
        {Page::Looks, IdLookList},
        {Page::Autoloops, IdAutoloopList},
        {Page::Tracks, IdTrackList},
        {Page::Midi, IdMidiList},
        {Page::Connections, IdConnectionsApply},
        {Page::Safety, IdSafetyApply},
        {Page::Diagnostics, IdDiagnosticsText},
    }};
    return std::all_of(
        critical_controls.begin(),
        critical_controls.end(),
        [this](const auto& control) {
            return ::GetDlgItem(
                       pages_[static_cast<std::size_t>(control.first)], control.second) != nullptr;
        });
}

LRESULT CALLBACK Application::window_proc(
    HWND window,
    UINT message,
    WPARAM wparam,
    LPARAM lparam) {
    auto* application = reinterpret_cast<Application*>(
        ::GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* creation = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        application = static_cast<Application*>(creation->lpCreateParams);
        application->window_ = window;
        static_cast<void>(::SetWindowLongPtrW(
            window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(application)));
    }
    return application != nullptr
        ? application->handle_message(window, message, wparam, lparam)
        : ::DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CALLBACK Application::page_proc(
    HWND window,
    UINT message,
    WPARAM wparam,
    LPARAM lparam) {
    auto* application = reinterpret_cast<Application*>(
        ::GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* creation = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        application = static_cast<Application*>(creation->lpCreateParams);
        static_cast<void>(::SetWindowLongPtrW(
            window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(application)));
    }
    if (application != nullptr &&
        window == application->pages_[static_cast<std::size_t>(Page::Connections)]) {
        if (message == WM_VSCROLL) {
            application->scroll_connections(LOWORD(wparam));
            return 0;
        }
        if (message == WM_MOUSEWHEEL) {
            application->scroll_connections_wheel(GET_WHEEL_DELTA_WPARAM(wparam));
            return 0;
        }
        if (message == DM_GETDEFID &&
            emberlights::connection_layout_keyboard_action(
                application->active_page_ == Page::Connections,
                emberlights::ConnectionKeyboardIntent::DefaultActivate) ==
                emberlights::ConnectionLayoutAction::SaveAndApply) {
            return MAKELRESULT(IdConnectionsApply, DC_HASDEFID);
        }
    }
    if (message == WM_COMMAND || message == WM_NOTIFY ||
        message == WM_CTLCOLORSTATIC || message == WM_CTLCOLOREDIT ||
        message == WM_CTLCOLORLISTBOX || message == WM_CTLCOLORBTN) {
        return ::SendMessageW(::GetParent(window), message, wparam, lparam);
    }
    return ::DefWindowProcW(window, message, wparam, lparam);
}

LRESULT Application::handle_message(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_GETMINMAXINFO: {
        auto* limits = reinterpret_cast<MINMAXINFO*>(lparam);
        limits->ptMinTrackSize.x = 1050;
        limits->ptMinTrackSize.y = 700;
        return 0;
    }
    case WM_SIZE:
        last_connections_focus_ = nullptr;
        layout();
        return 0;
    case WM_DPICHANGED: {
        last_connections_focus_ = nullptr;
        const auto* suggested = reinterpret_cast<const RECT*>(lparam);
        if (suggested != nullptr) {
            static_cast<void>(::SetWindowPos(
                window,
                nullptr,
                suggested->left,
                suggested->top,
                suggested->right - suggested->left,
                suggested->bottom - suggested->top,
                SWP_NOACTIVATE | SWP_NOZORDER));
        }
        layout();
        return 0;
    }
    case WM_MOUSEWHEEL:
        if (active_page_ == Page::Connections) {
            scroll_connections_wheel(GET_WHEEL_DELTA_WPARAM(wparam));
            return 0;
        }
        return ::DefWindowProcW(window, message, wparam, lparam);
    case DM_GETDEFID:
        if (emberlights::connection_layout_keyboard_action(
                active_page_ == Page::Connections,
                emberlights::ConnectionKeyboardIntent::DefaultActivate) ==
            emberlights::ConnectionLayoutAction::SaveAndApply) {
            return MAKELRESULT(IdConnectionsApply, DC_HASDEFID);
        }
        return ::DefWindowProcW(window, message, wparam, lparam);
    case WM_COMMAND:
        handle_command(
            LOWORD(wparam),
            HIWORD(wparam),
            reinterpret_cast<HWND>(lparam));
        return 0;
    case WM_NOTIFY:
        if (lparam != 0) {
            handle_notify(*reinterpret_cast<const NMHDR*>(lparam));
        }
        return 0;
    case WM_TIMER:
        if (wparam == kStatusTimer) {
            handle_timer();
        }
        return 0;
    case WM_COPYDATA: {
        const auto* copy = reinterpret_cast<const COPYDATASTRUCT*>(lparam);
        if (copy == nullptr || copy->dwData != kOpenProjectCopyData || copy->lpData == nullptr ||
            copy->cbData < sizeof(wchar_t) ||
            copy->cbData > static_cast<DWORD>(32768U * sizeof(wchar_t)) ||
            copy->cbData % sizeof(wchar_t) != 0U) {
            return FALSE;
        }
        const auto count = static_cast<std::size_t>(copy->cbData / sizeof(wchar_t));
        const auto* path = static_cast<const wchar_t*>(copy->lpData);
        if (path[count - 1U] != L'\0') {
            return FALSE;
        }
        if (maybe_save_changes()) {
            static_cast<void>(open_project(std::filesystem::path(path)));
        }
        return TRUE;
    }
    case WM_CTLCOLORSTATIC: {
        const auto device = reinterpret_cast<HDC>(wparam);
        ::SetBkMode(device, TRANSPARENT);
        ::SetTextColor(device, RGB(235, 240, 247));
        return reinterpret_cast<LRESULT>(background_brush_);
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        const auto device = reinterpret_cast<HDC>(wparam);
        ::SetTextColor(device, RGB(235, 240, 247));
        ::SetBkColor(device, RGB(30, 36, 45));
        return reinterpret_cast<LRESULT>(field_brush_);
    }
    case WM_CTLCOLORBTN: {
        const auto device = reinterpret_cast<HDC>(wparam);
        ::SetTextColor(device, RGB(235, 240, 247));
        ::SetBkColor(device, RGB(22, 27, 34));
        return reinterpret_cast<LRESULT>(surface_brush_);
    }
    case WM_CLOSE:
        if (maybe_save_changes()) {
            runner_.stop();
            ::DestroyWindow(window_);
        }
        return 0;
    case WM_DESTROY:
        ::KillTimer(window_, kStatusTimer);
        ::PostQuitMessage(EXIT_SUCCESS);
        return 0;
    default:
        return ::DefWindowProcW(window, message, wparam, lparam);
    }
}

void Application::create_menu_bar() {
    const auto menu = ::CreateMenu();
    const auto file = ::CreatePopupMenu();
    const auto edit = ::CreatePopupMenu();
    const auto show = ::CreatePopupMenu();
    const auto help = ::CreatePopupMenu();
    static_cast<void>(::AppendMenuW(file, MF_STRING, IdFileNew, L"&New\tCtrl+N"));
    static_cast<void>(::AppendMenuW(file, MF_STRING, IdFileOpen, L"&Open...\tCtrl+O"));
    static_cast<void>(::AppendMenuW(file, MF_SEPARATOR, 0, nullptr));
    static_cast<void>(::AppendMenuW(file, MF_STRING, IdFileSave, L"&Save\tCtrl+S"));
    static_cast<void>(::AppendMenuW(file, MF_STRING, IdFileSaveAs, L"Save &As..."));
    static_cast<void>(::AppendMenuW(
        file, MF_STRING, IdFileRestoreHistory, L"Restore Saved &Version..."));
    static_cast<void>(::AppendMenuW(file, MF_SEPARATOR, 0, nullptr));
    static_cast<void>(::AppendMenuW(
        file, MF_STRING, IdFileInspectSoundSwitch, L"Inspect SoundSwitch &Source..."));
    static_cast<void>(::AppendMenuW(
        file, MF_STRING, IdFileCompareSoundSwitch, L"Compare SoundSwitch &Exports..."));
    static_cast<void>(::AppendMenuW(
        file, MF_STRING, IdFileBundleSoundSwitch, L"Create SoundSwitch Migration &Bundle..."));
    static_cast<void>(::AppendMenuW(file, MF_SEPARATOR, 0, nullptr));
    static_cast<void>(::AppendMenuW(file, MF_STRING, IdFileExit, L"E&xit"));
    static_cast<void>(::AppendMenuW(edit, MF_STRING, IdEditUndo, L"&Undo\tCtrl+Z"));
    static_cast<void>(::AppendMenuW(edit, MF_STRING, IdEditRedo, L"&Redo\tCtrl+Y"));
    static_cast<void>(::AppendMenuW(show, MF_STRING, IdShowValidate, L"&Validate Project"));
    static_cast<void>(::AppendMenuW(show, MF_STRING, IdShowStartStop, L"&Start Show"));
    static_cast<void>(::AppendMenuW(help, MF_STRING, IdHelpAbout, L"&About EmberLights"));
    static_cast<void>(::AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(file), L"&File"));
    static_cast<void>(::AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(edit), L"&Edit"));
    static_cast<void>(::AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(show), L"&Show"));
    static_cast<void>(::AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(help), L"&Help"));
    static_cast<void>(::SetMenu(window_, menu));
    update_edit_menu();
}

HWND Application::add_control(
    HWND parent,
    const wchar_t* class_name,
    const wchar_t* text,
    DWORD style,
    DWORD extended_style,
    int id) {
    const auto control = ::CreateWindowExW(
        extended_style,
        class_name,
        text,
        WS_CHILD | WS_VISIBLE | style,
        0,
        0,
        100,
        24,
        parent,
        control_menu(id),
        instance_,
        nullptr);
    if (control != nullptr) {
        ::SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(normal_font_), TRUE);
        for (std::size_t index = 0; index < pages_.size(); ++index) {
            if (parent == pages_[index]) {
                page_controls_[index].push_back(control);
                break;
            }
        }
    }
    return control;
}

HWND Application::add_label(HWND parent, const wchar_t* text, int id) {
    return add_control(parent, L"STATIC", text, SS_LEFT | SS_CENTERIMAGE, 0, id);
}

HWND Application::add_edit(HWND parent, int id, bool multiline, bool read_only) {
    DWORD style = WS_TABSTOP | ES_AUTOHSCROLL;
    if (multiline) {
        style = WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL;
    }
    if (read_only) {
        style |= ES_READONLY;
    }
    return add_control(parent, L"EDIT", L"", style, WS_EX_CLIENTEDGE, id);
}

HWND Application::add_button(HWND parent, const wchar_t* text, int id, DWORD extra_style) {
    return add_control(parent, L"BUTTON", text, WS_TABSTOP | BS_PUSHBUTTON | extra_style, 0, id);
}

HWND Application::add_combo(HWND parent, int id) {
    return add_control(
        parent, L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 0, id);
}

HWND Application::add_listbox(HWND parent, int id) {
    return add_control(
        parent, L"LISTBOX", L"", WS_TABSTOP | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_VSCROLL,
        WS_EX_CLIENTEDGE, id);
}

HWND Application::add_listview(HWND parent, int id) {
    const auto list = add_control(
        parent,
        WC_LISTVIEWW,
        L"",
        WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        WS_EX_CLIENTEDGE,
        id);
    if (list != nullptr) {
        ListView_SetExtendedListViewStyle(
            list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
        ListView_SetBkColor(list, RGB(30, 36, 45));
        ListView_SetTextBkColor(list, RGB(30, 36, 45));
        ListView_SetTextColor(list, RGB(235, 240, 247));
    }
    return list;
}

void Application::add_listview_column(
    HWND list,
    int column,
    int width,
    const wchar_t* title) {
    LVCOLUMNW definition{};
    definition.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    definition.cx = width;
    definition.iSubItem = column;
    definition.pszText = const_cast<wchar_t*>(title);
    static_cast<void>(ListView_InsertColumn(list, column, &definition));
}

HWND Application::create_page(Page page) {
    const auto index = static_cast<std::size_t>(page);
    pages_[index] = ::CreateWindowExW(
        WS_EX_CONTROLPARENT,
        kPageClass,
        L"",
        WS_CHILD | WS_CLIPCHILDREN |
            (page == Page::Connections ? WS_VSCROLL : 0U),
        0,
        0,
        100,
        100,
        window_,
        nullptr,
        instance_,
        this);
    return pages_[index];
}

void Application::create_navigation() {
    constexpr std::array<const wchar_t*, static_cast<std::size_t>(Page::Count)> names{{
        L"LIVE • Home", L"LIVE • Overrides",
        L"STUDIO • Profiles", L"STUDIO • Patch", L"STUDIO • Groups",
        L"STUDIO • Static Looks", L"STUDIO • Autoloops", L"STUDIO • Track Scripts",
        L"CONTROL • MIDI", L"SYSTEM • Connections", L"SYSTEM • Safety",
        L"SYSTEM • Diagnostics"}};
    for (std::size_t index = 0; index < navigation_.size(); ++index) {
        navigation_[index] = add_button(
            window_, names[index], IdNavLive + static_cast<int>(index), BS_LEFT);
    }
}

void Application::create_pages() {
    // Page construction is split into sections below to keep each workspace focused.
    for (std::size_t index = 0; index < pages_.size(); ++index) {
        static_cast<void>(create_page(static_cast<Page>(index)));
    }

    auto page = pages_[static_cast<std::size_t>(Page::Live)];
    auto title = add_label(page, L"Live Console", IdLiveTitle);
    ::SendMessageW(title, WM_SETFONT, reinterpret_cast<WPARAM>(title_font_), TRUE);
    add_label(page, L"Stopped", IdLiveState);
    add_button(page, L"Start Show", IdLiveStartStop);
    add_button(page, L"BLACKOUT", IdLiveBlackout);
    add_button(page, L"Work Light", IdLiveWorkLight);
    add_label(page, L"Manual BPM", 0);
    add_edit(page, IdLiveBpm);
    add_button(page, L"Apply", IdLiveApplyBpm);
    add_button(page, L"Tap", IdLiveTap);
    add_label(page, L"Static Looks", 0);
    add_listbox(page, IdLiveLooks);
    add_button(page, L"Launch / Toggle", IdLiveTriggerLook);
    add_button(page, L"Clear Look", IdLiveClearLook);
    add_label(page, L"Autoloops", 0);
    add_listbox(page, IdLiveAutoloops);
    add_button(page, L"Launch", IdLiveTriggerAutoloop);
    add_button(page, L"Previous", IdLivePreviousAutoloop);
    add_button(page, L"Next", IdLiveNextAutoloop);
    add_button(page, L"Clear Loop", IdLiveClearAutoloop);
    add_label(page, L"Track Scripts", IdLiveTrackLabel);
    add_listbox(page, IdLiveTracks);
    add_button(page, L"Start Script", IdLiveTriggerTrack);
    add_button(page, L"Clear Script", IdLiveClearTrack);
    add_button(page, L"Arm Fog", IdLiveFogArm, BS_AUTOCHECKBOX);
    add_button(page, L"Arm Haze", IdLiveHazeArm, BS_AUTOCHECKBOX);
    add_button(page, L"Arm Laser", IdLiveLaserArm, BS_AUTOCHECKBOX);
    add_button(page, L"Arm Sparks", IdLiveSparkArm, BS_AUTOCHECKBOX);
    add_label(page, L"", IdLiveMetrics);
    add_label(page, L"", IdLiveAutoloopBankPage);
    add_button(page, L"Previous Banks", IdLivePreviousAutoloopBankPage);
    add_button(page, L"Next Banks", IdLiveNextAutoloopBankPage);
    add_button(page, L"Use All 64", IdLiveSelectAllAutoloopBanks);
    add_button(page, L"Use B1", IdLiveAutoloopBank1, BS_AUTOCHECKBOX);
    add_button(page, L"Only B1", IdLiveAutoloopBank1Only);
    add_button(page, L"Use B2", IdLiveAutoloopBank2, BS_AUTOCHECKBOX);
    add_button(page, L"Only B2", IdLiveAutoloopBank2Only);
    add_button(page, L"Use B3", IdLiveAutoloopBank3, BS_AUTOCHECKBOX);
    add_button(page, L"Only B3", IdLiveAutoloopBank3Only);
    add_button(page, L"Use B4", IdLiveAutoloopBank4, BS_AUTOCHECKBOX);
    add_button(page, L"Only B4", IdLiveAutoloopBank4Only);
    add_label(page, L"", IdLiveAutoloopPlayback);

    page = pages_[static_cast<std::size_t>(Page::Overrides)];
    title = add_label(page, L"Live Fixture Overrides", IdOverridesTitle);
    ::SendMessageW(title, WM_SETFONT, reinterpret_cast<WPARAM>(title_font_), TRUE);
    add_label(
        page,
        L"Immediate fixture/property controls. Overrides are transient, sit above Looks and Autoloops, "
        L"and remain subject to the Runner's safety limits. They never edit the project.",
        IdOverridesHelp);
    add_label(page, L"Active fixture", 0);
    add_listbox(page, IdOverridesFixture);
    add_label(page, L"Property", 0);
    add_combo(page, IdOverridesProperty);
    add_label(page, L"Value (0–100)", 0);
    add_edit(page, IdOverridesValue);
    add_button(page, L"Apply Override", IdOverridesApply);
    add_button(page, L"Release Property", IdOverridesRelease);
    add_button(page, L"Release All Overrides", IdOverridesReleaseAll);
    add_label(page, L"", IdOverridesActiveCount);
    add_label(page, L"", IdOverridesMessage);

    page = pages_[static_cast<std::size_t>(Page::Profiles)];
    title = add_label(page, L"Fixture Profiles", IdProfileTitle);
    ::SendMessageW(title, WM_SETFONT, reinterpret_cast<WPARAM>(title_font_), TRUE);
    add_listbox(page, IdProfileList);
    add_button(page, L"Import QLC+ Fixture (.qxf)...", IdProfileImportQlc);
    add_button(page, L"New", IdProfileNew);
    add_button(page, L"Duplicate", IdProfileDuplicate);
    add_button(page, L"Save Profile", IdProfileSave);
    add_button(page, L"Delete", IdProfileDelete);
    add_label(page, L"Manufacturer", 0);
    add_edit(page, IdProfileManufacturer);
    add_label(page, L"Model", 0);
    add_edit(page, IdProfileModel);
    add_label(page, L"Mode", 0);
    add_edit(page, IdProfileMode);
    add_label(page, L"Display name", 0);
    add_edit(page, IdProfileName);
    add_label(page, L"DMX footprint", 0);
    add_edit(page, IdProfileFootprint);
    add_label(page, L"Channels", 0);
    add_edit(page, IdProfileChannels, true);
    add_label(
        page,
        L"One channel per line: coarse, property, encoding, fine, min, max, default. "
        L"Encodings: linear8, linear16, discrete8, ranged8, constant8. "
        L"Use ranged8 when zero means the separate safe default.",
        IdProfileHelp);
    add_label(page, L"", IdProfileMessage);

    page = pages_[static_cast<std::size_t>(Page::Patch)];
    title = add_label(page, L"Fixture Patch", IdPatchTitle);
    ::SendMessageW(title, WM_SETFONT, reinterpret_cast<WPARAM>(title_font_), TRUE);
    auto list = add_listview(page, IdPatchList);
    add_listview_column(list, 0, 210, L"Fixture");
    add_listview_column(list, 1, 190, L"Stable ID");
    add_listview_column(list, 2, 280, L"Profile");
    add_listview_column(list, 3, 90, L"Universe");
    add_listview_column(list, 4, 90, L"Address");
    add_listview_column(list, 5, 90, L"Footprint");
    add_button(page, L"New", IdPatchNew);
    add_button(page, L"Save Fixture", IdPatchSave);
    add_button(page, L"Delete", IdPatchDelete);
    add_label(page, L"Fixture name", 0);
    add_edit(page, IdPatchName);
    add_label(page, L"Profile", 0);
    add_combo(page, IdPatchProfile);
    add_label(page, L"Universe", 0);
    add_combo(page, IdPatchUniverse);
    add_label(page, L"Address", 0);
    add_edit(page, IdPatchAddress);
    add_label(page, L"Roles (one per line)", 0);
    add_edit(page, IdPatchRoles, true);
    add_label(page, L"", IdPatchMessage);

    page = pages_[static_cast<std::size_t>(Page::Groups)];
    title = add_label(page, L"Fixture Groups", IdGroupTitle);
    ::SendMessageW(title, WM_SETFONT, reinterpret_cast<WPARAM>(title_font_), TRUE);
    add_listbox(page, IdGroupList);
    add_button(page, L"New", IdGroupNew);
    add_button(page, L"Duplicate", IdGroupDuplicate);
    add_button(page, L"Save Group", IdGroupSave);
    add_button(page, L"Delete", IdGroupDelete);
    add_label(page, L"Name", 0);
    add_edit(page, IdGroupName);
    add_label(page, L"Fixture IDs", 0);
    add_edit(page, IdGroupMembers, true);
    add_label(
        page,
        L"Enter one patched fixture ID per line. Groups can be used as targets while "
        L"authoring Static Looks; EmberLights expands them into deterministic fixture assignments.",
        IdGroupHelp);
    add_label(page, L"", IdGroupMessage);

    page = pages_[static_cast<std::size_t>(Page::Looks)];
    title = add_label(page, L"Static Looks", IdLookTitle);
    ::SendMessageW(title, WM_SETFONT, reinterpret_cast<WPARAM>(title_font_), TRUE);
    add_listbox(page, IdLookList);
    add_button(page, L"New", IdLookNew);
    add_button(page, L"Duplicate", IdLookDuplicate);
    add_button(page, L"Save Look", IdLookSave);
    add_button(page, L"Delete", IdLookDelete);
    add_label(page, L"Name", 0);
    add_edit(page, IdLookName);
    add_label(page, L"Crossfade (ms)", 0);
    add_edit(page, IdLookFade);
    add_label(page, L"Fixture or group target", 0);
    add_combo(page, IdLookTarget);
    add_label(page, L"", IdLookCapabilities);
    add_label(page, L"RGB picker", 0);
    add_edit(page, IdLookRgbHex, false, true);
    add_button(page, L"Choose RGB...", IdLookPickRgb);
    add_label(page, L"R %", 0);
    add_edit(page, IdLookRed);
    add_label(page, L"G %", 0);
    add_edit(page, IdLookGreen);
    add_label(page, L"B %", 0);
    add_edit(page, IdLookBlue);
    add_label(page, L"W %", 0);
    add_edit(page, IdLookWhite);
    add_label(page, L"A %", 0);
    add_edit(page, IdLookAmber);
    add_label(page, L"UV %", 0);
    add_edit(page, IdLookUv);
    add_label(page, L"Master %", 0);
    add_edit(page, IdLookIntensity);
    add_button(page, L"Apply Full Color", IdLookApplyColor, BS_DEFPUSHBUTTON);
    add_button(page, L"Red", IdLookSwatchRed);
    add_button(page, L"Green", IdLookSwatchGreen);
    add_button(page, L"Blue", IdLookSwatchBlue);
    add_button(page, L"White", IdLookSwatchWhite);
    add_button(page, L"Amber", IdLookSwatchAmber);
    add_button(page, L"UV", IdLookSwatchUv);
    add_button(page, L"Black", IdLookSwatchBlack);
    add_label(page, L"Property", 0);
    add_combo(page, IdLookProperty);
    add_label(page, L"Ownership", 0);
    add_combo(page, IdLookOwnership);
    add_label(page, L"Value %", 0);
    add_edit(page, IdLookValue);
    add_button(page, L"Apply Property", IdLookApplyProperty);
    add_button(page, L"Remove", IdLookRemoveProperty);
    add_label(page, L"Authored fixture ownership", 0);
    add_edit(page, IdLookAssignments, true, true);
    add_button(page, L"Build Exact Offline DMX Preview", IdLookPreview);
    add_edit(page, IdLookPreviewText, true, true);
    add_label(
        page,
        L"Full Color owns every supported RGBWAUV emitter, including zero, opens a real "
        L"master, and forces strobe off. The RGB picker changes RGB only; W/A/UV stay "
        L"direct. Preview uses the production renderer but never opens an output adapter.",
        IdLookHelp);
    add_label(page, L"", IdLookMessage);

    page = pages_[static_cast<std::size_t>(Page::Autoloops)];
    title = add_label(page, L"Autoloops", IdAutoloopTitle);
    ::SendMessageW(title, WM_SETFONT, reinterpret_cast<WPARAM>(title_font_), TRUE);
    add_listbox(page, IdAutoloopList);
    add_button(page, L"New", IdAutoloopNew);
    add_button(page, L"Duplicate", IdAutoloopDuplicate);
    add_button(page, L"Save Autoloop", IdAutoloopSave);
    add_button(page, L"Delete", IdAutoloopDelete);
    add_button(page, L"Next Open Slot", IdAutoloopNextEmpty);
    add_button(page, L"Swap Target Slot", IdAutoloopSwapTarget);
    add_label(page, L"Name", 0);
    add_edit(page, IdAutoloopName);
    add_label(page, L"Bank (1–64)", 0);
    add_edit(page, IdAutoloopBank);
    add_label(page, L"Slot (1–32)", 0);
    add_edit(page, IdAutoloopSlot);
    add_label(page, L"Length (beats)", 0);
    add_edit(page, IdAutoloopLength);
    add_label(page, L"Repeat", 0);
    add_combo(page, IdAutoloopRepeat);
    add_label(page, L"Steps", 0);
    add_edit(page, IdAutoloopSteps, true);
    add_label(
        page,
        L"One step per line: beat, look-id, cut|linear. The first step must be beat 0.",
        IdAutoloopHelp);
    add_label(page, L"", IdAutoloopMessage);

    page = pages_[static_cast<std::size_t>(Page::Tracks)];
    title = add_label(page, L"Track Scripts", IdTrackTitle);
    ::SendMessageW(title, WM_SETFONT, reinterpret_cast<WPARAM>(title_font_), TRUE);
    add_listbox(page, IdTrackList);
    add_button(page, L"New", IdTrackNew);
    add_button(page, L"Duplicate", IdTrackDuplicate);
    add_button(page, L"Save Script", IdTrackSave);
    add_button(page, L"Delete", IdTrackDelete);
    add_label(page, L"Name", 0);
    add_edit(page, IdTrackName);
    add_label(page, L"Audio asset (optional)", 0);
    add_combo(page, IdTrackAudioAsset);
    add_button(page, L"Add Audio...", IdTrackAddAudio);
    add_button(page, L"Relink...", IdTrackRelinkAudio);
    add_button(page, L"Verify", IdTrackVerifyAudio);
    add_button(page, L"Find in Folder...", IdTrackResolveAudioFolder);
    add_label(page, L"Legacy migration key (optional)", 0);
    add_edit(page, IdTrackAudioKey);
    add_label(page, L"Beat-addressed cues", 0);
    add_edit(page, IdTrackCues, true);
    add_label(
        page,
        L"One cue per line: beat, triggerLook|clearLook|triggerAutoloop|clearAutoloop, target-id. "
        L"Add Audio records a content identity without copying or modifying your music. Relink accepts only "
        L"the same SHA-256/size. Clear actions leave target-id blank; cues replay safely after a beat seek.",
        IdTrackHelp);
    add_label(page, L"", IdTrackMessage);

    page = pages_[static_cast<std::size_t>(Page::Midi)];
    title = add_label(page, L"MIDI Mapping", IdMidiTitle);
    ::SendMessageW(title, WM_SETFONT, reinterpret_cast<WPARAM>(title_font_), TRUE);
    list = add_listview(page, IdMidiList);
    add_listview_column(list, 0, 250, L"Input");
    add_listview_column(list, 1, 220, L"Action");
    add_listview_column(list, 2, 250, L"Target");
    add_listview_column(list, 3, 120, L"Behavior");
    add_label(page, L"Action", 0);
    add_combo(page, IdMidiAction);
    add_label(page, L"Target", 0);
    add_combo(page, IdMidiTarget);
    add_label(page, L"Property", 0);
    add_combo(page, IdMidiProperty);
    add_label(page, L"Behavior", 0);
    add_combo(page, IdMidiBehavior);
    add_button(page, L"Soft takeover", IdMidiSoftTakeover, BS_AUTOCHECKBOX);
    add_button(page, L"Learn Next MIDI Control", IdMidiLearn);
    add_button(page, L"Delete Mapping", IdMidiDelete);
    add_label(page, L"", IdMidiMessage);

    page = pages_[static_cast<std::size_t>(Page::Connections)];
    title = add_label(page, L"Connections & Output", IdConnectionsTitle);
    ::SendMessageW(title, WM_SETFONT, reinterpret_cast<WPARAM>(title_font_), TRUE);
    add_label(page, L"Project name", 0);
    add_edit(page, IdProjectName);
    add_button(page, L"Enable VirtualDJ / OS2L", IdOs2lEnabled, BS_AUTOCHECKBOX);
    add_label(page, L"Bind address", 0);
    add_edit(page, IdOs2lBind);
    add_label(page, L"Port", 0);
    add_edit(page, IdOs2lPort);
    add_button(page, L"Enable Art-Net", IdArtnetEnabled, BS_AUTOCHECKBOX);
    add_label(page, L"Destination", 0);
    add_edit(page, IdArtnetDestination);
    add_label(page, L"First port-address", 0);
    add_edit(page, IdArtnetBase);
    add_button(page, L"Enable sACN / E1.31", IdSacnEnabled, BS_AUTOCHECKBOX);
    add_label(page, L"Destination (or multicast)", 0);
    add_edit(page, IdSacnDestination);
    add_label(page, L"First universe", 0);
    add_edit(page, IdSacnBase);
    add_label(page, L"USB-DMX Pro — universe 1", 0);
    add_combo(page, IdDmxUsbProUniverse1);
    add_label(page, L"USB-DMX Pro — universe 2", 0);
    add_combo(page, IdDmxUsbProUniverse2);
    add_label(page, L"SoundSwitch Micro (WinUSB)", 0);
    add_combo(page, IdSoundSwitchMicroUniverse);
    add_label(page, L"Micro protocol", 0);
    add_combo(page, IdSoundSwitchMicroFraming);
    add_label(page, L"SoundSwitch Control One DMX", 0);
    add_combo(page, IdSoundSwitchControlOneMode);
    add_label(page, L"Frame rate", 0);
    add_edit(page, IdFrameRate);
    add_label(page, L"Manual fallback BPM", 0);
    add_edit(page, IdManualBpm);
    add_label(page, L"MIDI input", 0);
    add_combo(page, IdMidiInput);
    add_label(page, L"MIDI feedback output", 0);
    add_combo(page, IdMidiOutput);
    add_button(page, L"&Refresh MIDI + USB-DMX", IdRefreshMidi);
    add_button(page, L"Copy VirtualDJ Setup", IdCopyVirtualDjSetup);
    add_button(
        page,
        L"Save && &Apply Connections",
        IdConnectionsApply,
        BS_DEFPUSHBUTTON);
    add_label(
        page,
        L"VirtualDJ: use os2l=Auto and clear os2lDirectIp for automatic discovery. "
        L"Use Copy VirtualDJ Setup for the safe direct-IP fallback.",
        IdConnectionsMessage);

    page = pages_[static_cast<std::size_t>(Page::Safety)];
    title = add_label(page, L"Safety Policy", IdSafetyTitle);
    ::SendMessageW(title, WM_SETFONT, reinterpret_cast<WPARAM>(title_font_), TRUE);
    add_label(
        page,
        L"Hazard gates fail closed in Runner. Changing this policy requires stopping and "
        L"restarting the show so the compiled runtime can apply it.",
        0);
    add_button(page, L"Fog requires explicit arm", IdSafetyFogArm, BS_AUTOCHECKBOX);
    add_button(page, L"Haze requires explicit arm", IdSafetyHazeArm, BS_AUTOCHECKBOX);
    add_button(page, L"Laser requires explicit arm", IdSafetyLaserArm, BS_AUTOCHECKBOX);
    add_button(page, L"Sparks require explicit arm", IdSafetySparkArm, BS_AUTOCHECKBOX);
    add_button(page, L"Allow strobe output", IdSafetyStrobeAllowed, BS_AUTOCHECKBOX);
    add_label(page, L"Maximum strobe (0–1)", 0);
    add_edit(page, IdSafetyMaxStrobe);
    add_label(page, L"Maximum intensity (0–1)", 0);
    add_edit(page, IdSafetyMaxIntensity);
    add_button(page, L"Apply Safety Policy", IdSafetyApply);
    add_label(page, L"", IdSafetyMessage);

    page = pages_[static_cast<std::size_t>(Page::Diagnostics)];
    title = add_label(page, L"Diagnostics & Preflight", IdDiagnosticsTitle);
    ::SendMessageW(title, WM_SETFONT, reinterpret_cast<WPARAM>(title_font_), TRUE);
    add_edit(page, IdDiagnosticsText, true, true);
    add_button(page, L"Copy Diagnostics", IdDiagnosticsCopy);
    add_button(page, L"Save Diagnostics...", IdDiagnosticsExport);
    add_button(page, L"Validate Project", IdDiagnosticsValidate);
}

void Application::layout() {
    if (window_ == nullptr || pages_[0] == nullptr) {
        return;
    }
    RECT client{};
    static_cast<void>(::GetClientRect(window_, &client));
    const auto width = std::max(640L, client.right - client.left);
    const auto height = std::max(480L, client.bottom - client.top);
    const auto content_height = static_cast<int>(height) - kStatusHeight;

    for (std::size_t index = 0; index < navigation_.size(); ++index) {
        ::MoveWindow(
            navigation_[index],
            12,
            18 + static_cast<int>(index) * 48,
            kNavigationWidth - 24,
            38,
            TRUE);
    }
    for (std::size_t index = 0; index < pages_.size(); ++index) {
        ::MoveWindow(
            pages_[index],
            kNavigationWidth,
            0,
            static_cast<int>(width) - kNavigationWidth,
            content_height,
            TRUE);
        layout_page(
            static_cast<Page>(index),
            static_cast<int>(width) - kNavigationWidth,
            content_height);
    }
    ::MoveWindow(
        status_bar_,
        0,
        content_height,
        static_cast<int>(width),
        kStatusHeight,
        TRUE);
}

void Application::layout_page(Page page, int width, int height) {
    auto& controls = page_controls_[static_cast<std::size_t>(page)];
    auto move = [&](std::size_t index, int x, int y, int w, int h) {
        if (index < controls.size()) {
            ::MoveWindow(controls[index], x, y, std::max(1, w), std::max(1, h), TRUE);
        }
    };
    constexpr int margin = 24;
    const auto usable_width = std::max(400, width - margin * 2);
    move(0, margin, 18, usable_width, 40);

    switch (page) {
    case Page::Live: {
        move(1, margin, 64, 220, 28);
        move(2, margin + 230, 62, 130, 32);
        move(3, margin + 374, 62, 150, 32);
        move(4, margin + 536, 62, 130, 32);
        move(5, margin + 680, 62, 100, 28);
        move(6, margin + 782, 62, 80, 28);
        move(7, margin + 870, 62, 70, 28);
        move(8, margin + 948, 62, 70, 28);
        const auto column_gap = 20;
        const auto column_width = (usable_width - column_gap) / 2;
        const auto list_height = std::max(120, height - 450);
        const auto list_bottom = 140 + list_height;
        move(9, margin, 112, column_width, 26);
        move(10, margin, 140, column_width, list_height);
        move(11, margin, list_bottom + 10, 140, 34);
        move(12, margin + 150, list_bottom + 10, 110, 34);
        const auto right = margin + column_width + column_gap;
        move(13, right, 112, 120, 26);
        move(28, right + 126, 112, std::max(160, column_width - 126), 26);
        move(29, right, 140, 90, 28);
        move(30, right + 96, 140, 90, 28);
        move(31, right + 192, 140, 120, 28);
        const auto bank_column_width = std::max(120, column_width / 2);
        constexpr int bank_only_width = 62;
        const auto bank_use_width = std::max(54, bank_column_width - bank_only_width - 6);
        move(32, right, 174, bank_use_width, 27);
        move(33, right + bank_use_width + 6, 174, bank_only_width, 27);
        move(34, right + bank_column_width, 174, bank_use_width, 27);
        move(35, right + bank_column_width + bank_use_width + 6, 174, bank_only_width, 27);
        move(36, right, 204, bank_use_width, 27);
        move(37, right + bank_use_width + 6, 204, bank_only_width, 27);
        move(38, right + bank_column_width, 204, bank_use_width, 27);
        move(39, right + bank_column_width + bank_use_width + 6, 204, bank_only_width, 27);
        const auto autoloop_list_height = std::max(90, height - 548);
        const auto autoloop_list_bottom = 238 + autoloop_list_height;
        const auto live_controls_bottom = std::max(list_bottom, autoloop_list_bottom);
        move(14, right, 238, column_width, autoloop_list_height);
        move(15, right, live_controls_bottom + 10, 90, 34);
        move(16, right + 100, live_controls_bottom + 10, 90, 34);
        move(17, right + 200, live_controls_bottom + 10, 90, 34);
        move(18, right + 300, live_controls_bottom + 10, 110, 34);
        move(19, margin, live_controls_bottom + 54, 180, 26);
        move(20, margin, live_controls_bottom + 80, usable_width, 64);
        move(21, margin, live_controls_bottom + 150, 110, 30);
        move(22, margin + 120, live_controls_bottom + 150, 110, 30);
        move(40, right, live_controls_bottom + 44, column_width, 26);
        move(23, margin, height - 110, 110, 28);
        move(24, margin + 120, height - 110, 110, 28);
        move(25, margin + 240, height - 110, 110, 28);
        move(26, margin + 360, height - 110, 120, 28);
        move(27, margin, height - 76, usable_width, 54);
        break;
    }
    case Page::Overrides: {
        const auto left_width = std::min(360, usable_width / 2);
        const auto x = margin + left_width + 28;
        const auto form_width = usable_width - left_width - 28;
        move(1, margin, 70, usable_width, 44);
        move(2, margin, 128, left_width, 26);
        move(3, margin, 156, left_width, std::max(260, height - 250));
        move(4, x, 128, 160, 26);
        move(5, x, 156, form_width, 27);
        move(6, x, 196, 160, 26);
        move(7, x, 224, 140, 27);
        move(8, x, 268, 150, 32);
        move(9, x + 162, 268, 150, 32);
        move(10, x, 312, 190, 32);
        move(11, x, 364, form_width, 28);
        move(12, x, 400, form_width, 48);
        break;
    }
    case Page::Profiles: {
        const auto left_width = std::min(330, usable_width / 3);
        move(1, margin, 70, left_width, height - 190);
        move(2, margin, height - 108, left_width, 30);
        move(3, margin, height - 68, 70, 30);
        move(4, margin + 78, height - 68, 90, 30);
        move(5, margin + 176, height - 68, 110, 30);
        move(6, margin + 294, height - 68, 76, 30);
        const auto x = margin + left_width + 24;
        const auto form_width = usable_width - left_width - 24;
        constexpr int label_width = 120;
        constexpr int row_height = 34;
        for (std::size_t row = 0; row < 5U; ++row) {
            const auto base = 7U + row * 2U;
            move(base, x, 70 + static_cast<int>(row) * row_height, label_width, 26);
            move(base + 1U, x + label_width, 70 + static_cast<int>(row) * row_height,
                 form_width - label_width, 27);
        }
        move(17, x, 246, label_width, 26);
        move(18, x, 274, form_width, std::max(160, height - 430));
        move(19, x, height - 122, form_width, 52);
        move(20, x, height - 66, form_width, 30);
        break;
    }
    case Page::Patch:
        move(1, margin, 70, usable_width, std::max(210, height - 335));
        move(2, margin, height - 240, 80, 30);
        move(3, margin + 90, height - 240, 110, 30);
        move(4, margin + 210, height - 240, 80, 30);
        move(5, margin, height - 192, 100, 26);
        move(6, margin + 100, height - 192, 230, 27);
        move(7, margin + 350, height - 192, 70, 26);
        move(8, margin + 420, height - 192, std::max(220, usable_width - 750), 200);
        move(9, width - 300, height - 192, 72, 26);
        move(10, width - 228, height - 192, 70, 200);
        move(11, width - 146, height - 192, 62, 26);
        move(12, width - 84, height - 192, 60, 27);
        move(13, margin, height - 150, 150, 26);
        move(14, margin + 150, height - 150, usable_width - 150, 58);
        move(15, margin, height - 78, usable_width, 36);
        break;
    case Page::Groups: {
        const auto left_width = std::min(330, usable_width / 3);
        move(1, margin, 70, left_width, height - 150);
        move(2, margin, height - 68, 70, 30);
        move(3, margin + 78, height - 68, 90, 30);
        move(4, margin + 176, height - 68, 90, 30);
        move(5, margin + 274, height - 68, 70, 30);
        const auto x = margin + left_width + 24;
        const auto form_width = usable_width - left_width - 24;
        move(6, x, 70, 110, 26);
        move(7, x + 110, 70, form_width - 110, 27);
        move(8, x, 110, form_width, 26);
        move(9, x, 140, form_width, std::max(190, height - 305));
        move(10, x, height - 122, form_width, 50);
        move(11, x, height - 66, form_width, 30);
        break;
    }
    case Page::Looks: {
        const auto left_width = std::min(230, usable_width / 3);
        move(1, margin, 70, left_width, std::max(250, height - 200));
        move(2, margin, height - 112, 70, 30);
        move(3, margin + 78, height - 112, 96, 30);
        move(4, margin, height - 74, 102, 30);
        move(5, margin + 110, height - 74, 70, 30);

        const auto x = margin + left_width + 18;
        const auto form_width = usable_width - left_width - 18;
        move(6, x, 70, 48, 26);
        move(7, x + 48, 70, std::max(150, form_width - 286), 27);
        move(8, x + form_width - 226, 70, 136, 26);
        move(9, x + form_width - 90, 70, 90, 27);
        move(10, x, 104, 142, 26);
        move(11, x + 142, 104, form_width - 142, 250);
        move(12, x, 134, form_width, 28);
        move(13, x, 166, 76, 26);
        move(14, x + 76, 166, 86, 27);
        move(15, x + 170, 166, 116, 28);

        const auto emitter_width = std::max(58, form_width / 7);
        for (std::size_t emitter = 0U; emitter < 7U; ++emitter) {
            const auto emitter_x = x + static_cast<int>(emitter) * emitter_width;
            const auto label_index = 16U + emitter * 2U;
            move(label_index, emitter_x, 198, emitter_width - 6, 22);
            move(label_index + 1U, emitter_x, 220, emitter_width - 6, 27);
        }
        move(30, x, 256, 132, 30);
        const auto swatch_width = std::max(52, (form_width - 144) / 4);
        move(31, x + 142, 256, swatch_width, 30);
        move(32, x + 148 + swatch_width, 256, swatch_width, 30);
        move(33, x + 154 + swatch_width * 2, 256, swatch_width, 30);
        move(34, x + 160 + swatch_width * 3, 256, swatch_width, 30);
        move(35, x, 292, 78, 30);
        move(36, x + 84, 292, 62, 30);
        move(37, x + 152, 292, 70, 30);

        move(38, x + 236, 292, 66, 26);
        move(39, x + 302, 292, std::max(105, form_width - 302), 250);
        move(40, x, 330, 76, 26);
        move(41, x + 76, 330, 112, 250);
        move(42, x + 198, 330, 62, 26);
        move(43, x + 260, 330, 62, 27);
        move(44, x + 332, 330, 118, 30);
        move(45, x + 458, 330, std::max(72, form_width - 458), 30);

        const auto pane_gap = 10;
        const auto pane_width = (form_width - pane_gap) / 2;
        move(46, x, 370, pane_width, 26);
        move(47, x, 398, pane_width, std::max(112, height - 540));
        move(48, x + pane_width + pane_gap, 366, pane_width, 30);
        move(49, x + pane_width + pane_gap, 398, pane_width, std::max(112, height - 540));
        move(50, x, height - 132, form_width, 60);
        move(51, x, height - 66, form_width, 30);
        break;
    }
    case Page::Autoloops: {
        const auto left_width = std::min(330, usable_width / 3);
        move(1, margin, 70, left_width, height - 150);
        move(2, margin, height - 68, 70, 30);
        move(3, margin + 78, height - 68, 90, 30);
        move(4, margin + 176, height - 68, 100, 30);
        move(5, margin + 284, height - 68, 70, 30);
        const auto x = margin + left_width + 24;
        const auto form_width = usable_width - left_width - 24;
        move(6, x, 148, 130, 30);
        move(7, x + 140, 148, 140, 30);
        move(8, x, 70, 105, 26);
        move(9, x + 105, 70, form_width - 105, 27);
        move(10, x, 108, 105, 26);
        move(11, x + 105, 108, 70, 27);
        move(12, x + 190, 108, 90, 26);
        move(13, x + 280, 108, 70, 27);
        move(14, x + 365, 108, 100, 26);
        move(15, x + 465, 108, 80, 27);
        move(16, x + 560, 108, 70, 26);
        move(17, x + 630, 108, std::max(120, form_width - 630), 200);
        move(18, x, 188, form_width, 26);
        move(19, x, 218, form_width, std::max(170, height - 385));
        move(20, x, height - 130, form_width, 50);
        move(21, x, height - 72, form_width, 30);
        break;
    }
    case Page::Tracks: {
        const auto left_width = std::min(330, usable_width / 3);
        move(1, margin, 70, left_width, height - 150);
        move(2, margin, height - 68, 70, 30);
        move(3, margin + 78, height - 68, 90, 30);
        move(4, margin + 176, height - 68, 100, 30);
        move(5, margin + 284, height - 68, 70, 30);
        const auto x = margin + left_width + 24;
        const auto form_width = usable_width - left_width - 24;
        move(6, x, 70, 130, 26);
        move(7, x + 130, 70, form_width - 130, 27);
        move(8, x, 108, 130, 26);
        move(9, x + 130, 108, form_width - 130, 200);
        move(10, x, 146, 115, 30);
        move(11, x + 125, 146, 100, 30);
        move(12, x + 235, 146, 100, 30);
        move(13, x + 345, 146, 135, 30);
        move(14, x, 188, 190, 26);
        move(15, x + 190, 188, form_width - 190, 27);
        move(16, x, 228, form_width, 26);
        move(17, x, 258, form_width, std::max(150, height - 425));
        move(18, x, height - 130, form_width, 50);
        move(19, x, height - 72, form_width, 30);
        break;
    }
    case Page::Midi:
        move(1, margin, 70, usable_width, std::max(210, height - 300));
        move(2, margin, height - 205, 80, 26);
        move(3, margin + 80, height - 205, 220, 200);
        move(4, margin + 320, height - 205, 70, 26);
        move(5, margin + 390, height - 205, 250, 200);
        move(6, margin + 660, height - 205, 75, 26);
        move(7, margin + 735, height - 205, 160, 200);
        move(8, margin, height - 165, 80, 26);
        move(9, margin + 80, height - 165, 180, 200);
        move(10, margin + 280, height - 165, 130, 28);
        move(11, margin, height - 115, 190, 32);
        move(12, margin + 204, height - 115, 140, 32);
        move(13, margin, height - 70, usable_width, 32);
        break;
    case Page::Connections: {
        layout_connections();
        break;
    }
    case Page::Safety:
        move(1, margin, 70, usable_width, 48);
        move(2, margin, 132, 260, 30);
        move(3, margin, 170, 260, 30);
        move(4, margin, 208, 260, 30);
        move(5, margin, 246, 260, 30);
        move(6, margin, 294, 260, 30);
        move(7, margin, 342, 190, 27);
        move(8, margin + 190, 342, 100, 27);
        move(9, margin, 382, 190, 27);
        move(10, margin + 190, 382, 100, 27);
        move(11, margin, 438, 170, 32);
        move(12, margin, 486, usable_width, 42);
        break;
    case Page::Diagnostics:
        move(1, margin, 70, usable_width, height - 150);
        move(2, margin, height - 64, 150, 30);
        move(3, margin + 162, height - 64, 160, 30);
        move(4, margin + 334, height - 64, 140, 30);
        break;
    case Page::Count:
        break;
    }
}

void Application::layout_connections() {
    const auto page = pages_[static_cast<std::size_t>(Page::Connections)];
    if (page == nullptr) {
        return;
    }
    RECT client{};
    if (::GetClientRect(page, &client) == FALSE) {
        return;
    }
    const auto reported_dpi = ::GetDpiForWindow(page);
    const auto dpi = static_cast<std::uint16_t>(std::clamp<UINT>(
        reported_dpi == 0U ? emberlights::kConnectionLayoutDefaultDpi : reported_dpi,
        emberlights::kConnectionLayoutMinimumDpi,
        emberlights::kConnectionLayoutMaximumDpi));
    connections_layout_ = emberlights::compute_connection_layout({
        static_cast<std::int32_t>(std::max(1L, client.right - client.left)),
        static_cast<std::int32_t>(std::max(1L, client.bottom - client.top)),
        dpi,
        connections_scroll_offset_});
    connections_scroll_offset_ = connections_layout_.scroll_offset;

    SCROLLINFO scroll{};
    scroll.cbSize = sizeof(scroll);
    scroll.fMask = SIF_PAGE | SIF_POS | SIF_RANGE;
    scroll.nMin = 0;
    scroll.nMax = std::max(0, connections_layout_.content_height - 1);
    scroll.nPage = static_cast<UINT>(
        std::max(1, connections_layout_.scroll_viewport.height));
    scroll.nPos = connections_layout_.scroll_offset;
    static_cast<void>(::SetScrollInfo(page, SB_VERT, &scroll, TRUE));

    RECT update{
        connections_layout_.action_bar.x,
        connections_layout_.action_bar.y,
        connections_layout_.action_bar.right(),
        connections_layout_.action_bar.bottom()};
    static_cast<void>(::InvalidateRect(page, &update, TRUE));

    auto& controls =
        page_controls_[static_cast<std::size_t>(Page::Connections)];
    const auto count = std::min(
        controls.size(), emberlights::kConnectionLayoutItemCount);
    for (std::size_t index = 0; index < count; ++index) {
        const auto control = controls[index];
        const auto& rectangle = connections_layout_.items[index];
        wchar_t class_name[16]{};
        const auto is_combo = ::GetClassNameW(
            control,
            class_name,
            static_cast<int>(sizeof(class_name) / sizeof(class_name[0]))) > 0 &&
            ::lstrcmpiW(class_name, L"ComboBox") == 0;
        const auto control_height = is_combo
            ? std::max(
                  rectangle.height,
                  static_cast<std::int32_t>(
                      (200LL * connections_layout_.dpi + 48LL) / 96LL))
            : rectangle.height;
        ::MoveWindow(
            control,
            rectangle.x,
            rectangle.y,
            rectangle.width,
            control_height,
            TRUE);

        if (index >= emberlights::kConnectionLayoutScrollableItemCount) {
            static_cast<void>(::SetWindowRgn(control, nullptr, TRUE));
            continue;
        }

        const auto left = std::max(
            rectangle.x, connections_layout_.scroll_viewport.x);
        const auto top = std::max(
            rectangle.y, connections_layout_.scroll_viewport.y);
        const auto right = std::min(
            rectangle.right(), connections_layout_.scroll_viewport.right());
        const auto bottom = std::min(
            rectangle.y + control_height,
            connections_layout_.scroll_viewport.bottom());
        const auto has_intersection = left < right && top < bottom;
        const auto region = ::CreateRectRgn(
            has_intersection ? left - rectangle.x : 0,
            has_intersection ? top - rectangle.y : 0,
            has_intersection ? right - rectangle.x : 0,
            has_intersection ? bottom - rectangle.y : 0);
        if (region != nullptr && ::SetWindowRgn(control, region, TRUE) == 0) {
            static_cast<void>(::DeleteObject(region));
        }
    }
}

void Application::scroll_connections(UINT scroll_code) {
    if (connections_layout_.viewport.width <= 0) {
        layout_connections();
    }
    auto requested = static_cast<std::int64_t>(connections_scroll_offset_);
    const auto line = std::max<std::int32_t>(
        1,
        static_cast<std::int32_t>(
            (36LL * connections_layout_.dpi + 48LL) / 96LL));
    const auto page = std::max<std::int32_t>(
        line,
        connections_layout_.scroll_viewport.height - line);
    switch (scroll_code) {
    case SB_TOP: requested = 0; break;
    case SB_BOTTOM: requested = connections_layout_.maximum_scroll_offset; break;
    case SB_LINEUP: requested -= line; break;
    case SB_LINEDOWN: requested += line; break;
    case SB_PAGEUP: requested -= page; break;
    case SB_PAGEDOWN: requested += page; break;
    case SB_THUMBPOSITION:
    case SB_THUMBTRACK: {
        SCROLLINFO scroll{};
        scroll.cbSize = sizeof(scroll);
        scroll.fMask = SIF_TRACKPOS;
        if (::GetScrollInfo(
                pages_[static_cast<std::size_t>(Page::Connections)],
                SB_VERT,
                &scroll) != FALSE) {
            requested = scroll.nTrackPos;
        }
        break;
    }
    case SB_ENDSCROLL:
    default:
        return;
    }
    connections_scroll_offset_ = static_cast<std::int32_t>(std::clamp<std::int64_t>(
        requested,
        0,
        connections_layout_.maximum_scroll_offset));
    layout_connections();
}

void Application::scroll_connections_wheel(short wheel_delta) {
    connections_wheel_delta_ += wheel_delta;
    const auto notches = connections_wheel_delta_ / WHEEL_DELTA;
    connections_wheel_delta_ %= WHEEL_DELTA;
    if (notches == 0) {
        return;
    }
    UINT lines = 3U;
    static_cast<void>(::SystemParametersInfoW(
        SPI_GETWHEELSCROLLLINES, 0U, &lines, 0U));
    const auto line = std::max<std::int32_t>(
        1,
        static_cast<std::int32_t>(
            (36LL * connections_layout_.dpi + 48LL) / 96LL));
    const auto distance = lines == WHEEL_PAGESCROLL
        ? std::max<std::int32_t>(
              line, connections_layout_.scroll_viewport.height - line)
        : line * static_cast<std::int32_t>(std::min(lines, 100U));
    const auto requested = static_cast<std::int64_t>(connections_scroll_offset_) -
        static_cast<std::int64_t>(notches) * distance;
    connections_scroll_offset_ = static_cast<std::int32_t>(std::clamp<std::int64_t>(
        requested,
        0,
        connections_layout_.maximum_scroll_offset));
    layout_connections();
}

void Application::reveal_connections_focus() {
    if (active_page_ != Page::Connections) {
        last_connections_focus_ = nullptr;
        return;
    }
    const auto page = pages_[static_cast<std::size_t>(Page::Connections)];
    auto focused = ::GetFocus();
    while (focused != nullptr && ::GetParent(focused) != page) {
        focused = ::GetParent(focused);
    }
    if (focused == nullptr) {
        last_connections_focus_ = nullptr;
        return;
    }
    if (focused == last_connections_focus_) {
        return;
    }
    last_connections_focus_ = focused;
    const auto& controls =
        page_controls_[static_cast<std::size_t>(Page::Connections)];
    const auto found = std::find(controls.begin(), controls.end(), focused);
    if (found == controls.end()) {
        return;
    }
    const auto index = static_cast<std::size_t>(found - controls.begin());
    if (index >= emberlights::kConnectionLayoutScrollableItemCount) {
        return;
    }
    const auto requested = emberlights::connection_layout_scroll_to_reveal(
        connections_layout_,
        static_cast<emberlights::ConnectionLayoutItem>(index));
    if (requested != connections_scroll_offset_) {
        connections_scroll_offset_ = requested;
        layout_connections();
    }
}

void Application::show_page(Page page) {
    if (active_page_ != page) {
        last_connections_focus_ = nullptr;
    }
    active_page_ = page;
    for (std::size_t index = 0; index < pages_.size(); ++index) {
        ::ShowWindow(pages_[index], index == static_cast<std::size_t>(page) ? SW_SHOW : SW_HIDE);
        ::EnableWindow(navigation_[index], index != static_cast<std::size_t>(page));
    }
    if (page == Page::Diagnostics) {
        refresh_diagnostics();
    }
}

void Application::update_title() {
    std::wstring title = L"EmberLights — ";
    title += widen(project_.name);
    if (!current_path_.empty()) {
        title += L" — ";
        title += current_path_.filename().wstring();
    }
    if (dirty_) {
        title += L" *";
    }
    static_cast<void>(::SetWindowTextW(window_, title.c_str()));
}

void Application::update_edit_menu() {
    const auto menu = ::GetMenu(window_);
    const auto edit = menu == nullptr ? nullptr : ::GetSubMenu(menu, 1);
    if (edit == nullptr) {
        return;
    }
    static_cast<void>(::EnableMenuItem(
        edit,
        IdEditUndo,
        MF_BYCOMMAND | (edit_history_.can_undo() ? MF_ENABLED : MF_GRAYED)));
    static_cast<void>(::EnableMenuItem(
        edit,
        IdEditRedo,
        MF_BYCOMMAND | (edit_history_.can_redo() ? MF_ENABLED : MF_GRAYED)));
    static_cast<void>(::DrawMenuBar(window_));
}

void Application::reset_authoring_selection() {
    profile_index_ = -1;
    fixture_index_ = -1;
    group_index_ = -1;
    look_index_ = -1;
    look_draft_.reset();
    autoloop_index_ = -1;
    track_index_ = -1;
}

void Application::capture_saved_project() {
    saved_project_serialized_ = emberlights::serialize_project(project_);
    recovery_save_required_ = false;
    dirty_ = false;
    update_title();
}

void Application::record_project_edit(const emberlights::ProjectDocument& before) {
    if (emberlights::serialize_project(before) == emberlights::serialize_project(project_)) {
        return;
    }
    edit_history_.record_before_change(before);
    update_edit_menu();
}

void Application::undo_project_edit() {
    if (!edit_history_.undo(project_)) {
        set_status(L"Nothing to undo.");
        return;
    }
    reset_authoring_selection();
    mark_dirty();
    refresh_all();
    update_edit_menu();
    set_status(dirty_ ? L"Undo applied. Save when this revision is ready."
                      : L"Undo applied. Project matches its saved revision.");
}

void Application::redo_project_edit() {
    if (!edit_history_.redo(project_)) {
        set_status(L"Nothing to redo.");
        return;
    }
    reset_authoring_selection();
    mark_dirty();
    refresh_all();
    update_edit_menu();
    set_status(dirty_ ? L"Redo applied. Save when this revision is ready."
                      : L"Redo applied. Project matches its saved revision.");
}

void Application::mark_dirty() {
    if (saved_project_serialized_.empty()) {
        saved_project_serialized_ = emberlights::serialize_project(project_);
    }
    dirty_ = recovery_save_required_ ||
        emberlights::serialize_project(project_) != saved_project_serialized_;
    update_title();
    set_status(dirty_ ? L"Unsaved changes" : L"Project matches its saved revision.");
}

void Application::set_status(std::wstring text) {
    if (status_bar_ != nullptr) {
        static_cast<void>(::SetWindowTextW(status_bar_, text.c_str()));
    }
}

void Application::set_page_message(
    Page page,
    int id,
    std::string_view message,
    bool error_message) {
    const auto control = ::GetDlgItem(pages_[static_cast<std::size_t>(page)], id);
    set_control_text(control, message);
    if (!message.empty()) {
        set_status((error_message ? L"Error: " : L"") + widen(message));
    }
}

namespace {

void listbox_add(HWND list, std::wstring_view text, std::intptr_t data) {
    const auto index = static_cast<int>(::SendMessageW(
        list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.data())));
    if (index >= 0) {
        static_cast<void>(::SendMessageW(list, LB_SETITEMDATA, index, static_cast<LPARAM>(data)));
    }
}

void combo_add(HWND combo, std::wstring_view text, std::intptr_t data) {
    const auto index = static_cast<int>(::SendMessageW(
        combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.data())));
    if (index >= 0) {
        static_cast<void>(::SendMessageW(combo, CB_SETITEMDATA, index, static_cast<LPARAM>(data)));
    }
}

void combo_select_data(HWND combo, std::intptr_t data) {
    const auto count = static_cast<int>(::SendMessageW(combo, CB_GETCOUNT, 0, 0));
    for (int index = 0; index < count; ++index) {
        if (static_cast<std::intptr_t>(::SendMessageW(combo, CB_GETITEMDATA, index, 0)) == data) {
            static_cast<void>(::SendMessageW(combo, CB_SETCURSEL, index, 0));
            return;
        }
    }
    if (count > 0) {
        static_cast<void>(::SendMessageW(combo, CB_SETCURSEL, 0, 0));
    }
}

[[nodiscard]] std::intptr_t combo_selected_data(HWND combo, std::intptr_t fallback = -1) {
    const auto selected = static_cast<int>(::SendMessageW(combo, CB_GETCURSEL, 0, 0));
    return selected >= 0
        ? static_cast<std::intptr_t>(::SendMessageW(combo, CB_GETITEMDATA, selected, 0))
        : fallback;
}

void listview_set_row(
    HWND list,
    int row,
    LPARAM data,
    const std::vector<std::wstring>& columns) {
    if (columns.empty()) {
        return;
    }
    LVITEMW item{};
    item.mask = LVIF_TEXT | LVIF_PARAM;
    item.iItem = row;
    item.iSubItem = 0;
    item.pszText = const_cast<wchar_t*>(columns[0].c_str());
    item.lParam = data;
    const auto inserted = ListView_InsertItem(list, &item);
    if (inserted < 0) {
        return;
    }
    for (std::size_t column = 1; column < columns.size(); ++column) {
        ListView_SetItemText(
            list,
            inserted,
            static_cast<int>(column),
            const_cast<wchar_t*>(columns[column].c_str()));
    }
}

[[nodiscard]] std::int32_t listview_selected_data(HWND list) {
    const auto row = ListView_GetNextItem(list, -1, LVNI_SELECTED);
    if (row < 0) {
        return -1;
    }
    LVITEMW item{};
    item.mask = LVIF_PARAM;
    item.iItem = row;
    return ListView_GetItem(list, &item) != FALSE
        ? static_cast<std::int32_t>(item.lParam)
        : -1;
}

[[nodiscard]] std::string action_name(showcore::ActionType action) {
    switch (action) {
    case showcore::ActionType::None: return "None";
    case showcore::ActionType::SetProperty: return "Set fixture property";
    case showcore::ActionType::Blackout: return "Blackout";
    case showcore::ActionType::TriggerLook: return "Trigger Static Look";
    case showcore::ActionType::TriggerAutoloop: return "Trigger Autoloop";
    case showcore::ActionType::TapTempo: return "Tap tempo";
    case showcore::ActionType::ArmFog: return "Arm fog";
    case showcore::ActionType::ClearLook: return "Clear Static Look";
    case showcore::ActionType::ClearAutoloop: return "Clear Autoloop";
    case showcore::ActionType::NextAutoloop: return "Next Autoloop";
    case showcore::ActionType::PreviousAutoloop: return "Previous Autoloop";
    case showcore::ActionType::WorkLight: return "Work light";
    case showcore::ActionType::ArmHaze: return "Arm haze";
    case showcore::ActionType::ArmLaser: return "Arm laser";
    case showcore::ActionType::ArmSpark: return "Arm sparks";
    case showcore::ActionType::TriggerTrackScript: return "Start Track Script";
    case showcore::ActionType::ClearTrackScript: return "Clear Track Script";
    case showcore::ActionType::ClearManualOverrides: return "Release All Manual Overrides";
    case showcore::ActionType::SetGroupProperty: return "Set group property";
    case showcore::ActionType::SelectAutoloopBank: return "Select Autoloop Bank";
    case showcore::ActionType::SelectAllAutoloopBanks: return "Select All Autoloop Banks";
    case showcore::ActionType::SetAutoloopBankEnabled: return "Set Autoloop Bank Enabled";
    case showcore::ActionType::BlackoutGroup: return "Blackout fixture group";
    case showcore::ActionType::Count: return "Invalid";
    }
    return "Invalid";
}

[[nodiscard]] std::string behavior_name(showcore::MappingBehavior behavior) {
    switch (behavior) {
    case showcore::MappingBehavior::Momentary: return "Momentary";
    case showcore::MappingBehavior::Toggle: return "Toggle";
    case showcore::MappingBehavior::Latch: return "Latch";
    case showcore::MappingBehavior::Continuous: return "Continuous";
    case showcore::MappingBehavior::Relative: return "Relative";
    }
    return "Unknown";
}

[[nodiscard]] std::string midi_message_name(const emberlights::MidiMappingDefinition& mapping) {
    std::ostringstream stream;
    if (!mapping.device_name.empty()) {
        stream << mapping.device_name << " — ";
    }
    switch (mapping.message_type) {
    case showcore::MidiMessageType::NoteOn: stream << "Note "; break;
    case showcore::MidiMessageType::NoteOff: stream << "Note off "; break;
    case showcore::MidiMessageType::ControlChange: stream << "CC "; break;
    case showcore::MidiMessageType::PitchBend: stream << "Pitch "; break;
    }
    stream << static_cast<unsigned int>(mapping.number) << " ch "
           << static_cast<unsigned int>(mapping.channel + 1U);
    return stream.str();
}

[[nodiscard]] std::string profile_channels_text(
    const emberlights::FixtureProfileDefinition& profile) {
    std::ostringstream stream;
    for (const auto& channel : profile.channels) {
        stream << channel.coarse_offset + 1U << ','
               << emberlights::property_name(channel.property) << ','
               << emberlights::channel_encoding_name(channel.encoding) << ','
               << (channel.fine_offset >= 0 ? channel.fine_offset + 1 : 0) << ','
               << static_cast<unsigned int>(channel.dmx_min) << ','
               << static_cast<unsigned int>(channel.dmx_max) << ','
               << channel.default_value << '\n';
    }
    return stream.str();
}

[[nodiscard]] std::string look_assignments_text(const emberlights::LookDefinition& look) {
    std::ostringstream stream;
    for (const auto& assignment : look.assignments) {
        stream << assignment.fixture_id << ','
               << emberlights::property_name(assignment.property) << ',';
        switch (assignment.value.mode) {
        case showcore::ValueMode::Set: stream << assignment.value.value; break;
        case showcore::ValueMode::ForceZero: stream << "off"; break;
        case showcore::ValueMode::Release: stream << "release"; break;
        }
        stream << '\n';
    }
    return stream.str();
}

void set_static_look_color_controls(
    HWND page,
    const emberlights::StaticLookColor& color) {
    constexpr std::array ids{
        IdLookRed,
        IdLookGreen,
        IdLookBlue,
        IdLookWhite,
        IdLookAmber,
        IdLookUv,
        IdLookIntensity};
    const std::array values{
        color.red,
        color.green,
        color.blue,
        color.white,
        color.amber,
        color.uv,
        color.intensity};
    for (std::size_t index = 0U; index < ids.size(); ++index) {
        const auto percent = std::clamp(values[index], 0.0F, 1.0F) * 100.0F;
        std::ostringstream formatted;
        formatted << std::fixed << std::setprecision(3) << percent;
        auto text = formatted.str();
        while (!text.empty() && text.back() == '0') {
            text.pop_back();
        }
        if (!text.empty() && text.back() == '.') {
            text.pop_back();
        }
        set_control_text(::GetDlgItem(page, ids[index]), text);
    }
    set_control_text(
        ::GetDlgItem(page, IdLookRgbHex),
        emberlights::format_rgb_hex(color));
}

[[nodiscard]] std::string static_look_outcome_text(
    std::string_view action,
    const emberlights::StaticLookAuthoringOutcome& outcome) {
    std::ostringstream message;
    switch (outcome.result) {
    case emberlights::StaticLookAuthoringResult::Applied:
        message << action << " updated " << outcome.fixtures_modified << " of "
                << outcome.fixtures_considered << " target fixtures ("
                << outcome.assignments_written << " owned properties).";
        break;
    case emberlights::StaticLookAuthoringResult::NoChange:
        message << action << " already matches this draft.";
        break;
    case emberlights::StaticLookAuthoringResult::TargetNotFound:
        return "Select a patched fixture or fixture group first.";
    case emberlights::StaticLookAuthoringResult::EmptyTarget:
        return "The selected fixture group is empty.";
    case emberlights::StaticLookAuthoringResult::Unsupported:
        return "The selected target does not support that property or direct color.";
    case emberlights::StaticLookAuthoringResult::InvalidValue:
        return "Enter values from 0 through 100 percent.";
    }
    for (const auto& warning : outcome.warnings) {
        message << " Warning: " << warning;
    }
    return message.str();
}

[[nodiscard]] std::string autoloop_steps_text(const emberlights::AutoloopDefinition& loop) {
    std::ostringstream stream;
    for (const auto& step : loop.steps) {
        stream << step.at_beat << ',' << step.look_id << ','
               << (step.transition == showcore::AutoloopTransition::Linear ? "linear" : "cut")
               << '\n';
    }
    return stream.str();
}

[[nodiscard]] std::string track_cues_text(const emberlights::TrackScriptDefinition& track) {
    std::ostringstream stream;
    for (const auto& cue : track.cues) {
        stream << cue.at_beat << ',' << emberlights::track_cue_action_name(cue.action) << ','
               << cue.target_ref << '\n';
    }
    return stream.str();
}

}  // namespace

void Application::refresh_all() {
    refreshing_ = true;
    refresh_profiles();
    refresh_patch();
    refresh_groups();
    refresh_looks();
    refresh_autoloops();
    refresh_tracks();
    refresh_midi();
    refresh_overrides();
    refresh_connections();
    refresh_safety();
    refresh_live_lists();
    refresh_live_status();
    refresh_diagnostics();
    refreshing_ = false;
    update_title();
}

void Application::refresh_live_lists() {
    const auto page = pages_[static_cast<std::size_t>(Page::Live)];
    const auto looks = ::GetDlgItem(page, IdLiveLooks);
    const auto loops = ::GetDlgItem(page, IdLiveAutoloops);
    const auto tracks = ::GetDlgItem(page, IdLiveTracks);
    static_cast<void>(::SendMessageW(looks, LB_RESETCONTENT, 0, 0));
    static_cast<void>(::SendMessageW(loops, LB_RESETCONTENT, 0, 0));
    static_cast<void>(::SendMessageW(tracks, LB_RESETCONTENT, 0, 0));
    const auto& live = live_project();
    for (std::size_t index = 0; index < live.looks.size(); ++index) {
        listbox_add(looks, widen(live.looks[index].name), index);
    }
    for (std::size_t index = 0; index < live.autoloops.size(); ++index) {
        const auto& loop = live.autoloops[index];
        std::ostringstream label;
        label << "B" << loop.bank + 1U << " / S" << static_cast<unsigned int>(loop.slot + 1U)
              << " — " << loop.name;
        listbox_add(loops, widen(label.str()), index);
    }
    for (std::size_t index = 0; index < live.track_scripts.size(); ++index) {
        const auto& track = live.track_scripts[index];
        std::ostringstream label;
        label << track.name;
        const auto asset = std::find_if(
            live.audio_assets.begin(), live.audio_assets.end(), [&](const auto& candidate) {
                return candidate.id == track.audio_asset_id;
            });
        if (asset != live.audio_assets.end()) {
            label << " — " << asset->name;
        } else if (!track.audio_key.empty()) {
            label << " — " << track.audio_key;
        }
        label << " (" << track.cues.size() << " cue" << (track.cues.size() == 1U ? "" : "s")
              << ')';
        listbox_add(tracks, widen(label.str()), index);
    }
    set_control_text(::GetDlgItem(page, IdLiveBpm), number_text(live.connections.manual_bpm));
}

void Application::refresh_live_status() {
    const auto page = pages_[static_cast<std::size_t>(Page::Live)];
    const auto status = runner_.status();
    live_view_model_.update(status);
    std::wstring headline = runner_state_name(status.state);
    headline += L" • Clock ";
    headline += widen(number_text(status.bpm));
    headline += L" BPM";
    static_cast<void>(::SetWindowTextW(::GetDlgItem(page, IdLiveState), headline.c_str()));
    static_cast<void>(::SetWindowTextW(
        ::GetDlgItem(page, IdLiveStartStop),
        status.state == emberlights::RunnerState::Running ? L"Stop Show" : L"Start Show"));
    static_cast<void>(::SetWindowTextW(
        ::GetDlgItem(page, IdLiveBlackout), status.blackout ? L"RELEASE BLACKOUT" : L"BLACKOUT"));
    static_cast<void>(::SetWindowTextW(
        ::GetDlgItem(page, IdLiveWorkLight), status.work_light ? L"Clear Work Light" : L"Work Light"));
    Button_SetCheck(::GetDlgItem(page, IdLiveFogArm), status.fog_armed ? BST_CHECKED : BST_UNCHECKED);
    Button_SetCheck(::GetDlgItem(page, IdLiveHazeArm), status.haze_armed ? BST_CHECKED : BST_UNCHECKED);
    Button_SetCheck(::GetDlgItem(page, IdLiveLaserArm), status.laser_armed ? BST_CHECKED : BST_UNCHECKED);
    Button_SetCheck(::GetDlgItem(page, IdLiveSparkArm), status.spark_armed ? BST_CHECKED : BST_UNCHECKED);

    const auto bank_page_count = static_cast<std::uint16_t>(
        showcore::kAutoloopControlPageCount);
    if (live_autoloop_bank_page_ >= bank_page_count) {
        live_autoloop_bank_page_ = 0U;
    }
    const auto first_bank = static_cast<std::uint16_t>(
        live_autoloop_bank_page_ * showcore::kAutoloopBanksPerControlPage);
    std::wostringstream bank_page;
    bank_page << L"Previous/Next filter: banks " << first_bank + 1U << L"–"
              << first_bank + showcore::kAutoloopBanksPerControlPage << L"  (page "
              << live_autoloop_bank_page_ + 1U << L"/" << bank_page_count << L")";
    static_cast<void>(::SetWindowTextW(
        ::GetDlgItem(page, IdLiveAutoloopBankPage), bank_page.str().c_str()));
    const bool can_change_banks = status.state == emberlights::RunnerState::Running;
    static_cast<void>(::EnableWindow(
        ::GetDlgItem(page, IdLiveSelectAllAutoloopBanks), can_change_banks));
    constexpr std::array<int, showcore::kAutoloopBanksPerControlPage> bank_controls{
        IdLiveAutoloopBank1,
        IdLiveAutoloopBank2,
        IdLiveAutoloopBank3,
        IdLiveAutoloopBank4};
    constexpr std::array<int, showcore::kAutoloopBanksPerControlPage> only_controls{
        IdLiveAutoloopBank1Only,
        IdLiveAutoloopBank2Only,
        IdLiveAutoloopBank3Only,
        IdLiveAutoloopBank4Only};
    for (std::size_t offset = 0U; offset < bank_controls.size(); ++offset) {
        const auto bank = static_cast<std::uint16_t>(first_bank + offset);
        const auto enabled = (status.active_autoloop_bank_mask &
                              (std::uint64_t{1} << bank)) != 0U;
        const auto use_label = L"Use B" + std::to_wstring(bank + 1U);
        const auto only_label = L"Only B" + std::to_wstring(bank + 1U);
        const auto use_control = ::GetDlgItem(page, bank_controls[offset]);
        static_cast<void>(::SetWindowTextW(use_control, use_label.c_str()));
        Button_SetCheck(use_control, enabled ? BST_CHECKED : BST_UNCHECKED);
        static_cast<void>(::EnableWindow(use_control, can_change_banks));
        const auto only_control = ::GetDlgItem(page, only_controls[offset]);
        static_cast<void>(::SetWindowTextW(only_control, only_label.c_str()));
        static_cast<void>(::EnableWindow(only_control, can_change_banks));
    }

    std::wostringstream playback;
    if (!status.active_autoloop.valid()) {
        playback << L"No active Autoloop.";
    } else {
        playback << L"Active B" << status.active_autoloop.bank + 1U << L" / S"
                 << static_cast<unsigned int>(status.active_autoloop.slot + 1U);
        const auto& live = live_project();
        const auto matched_loop = std::find_if(
            live.autoloops.begin(), live.autoloops.end(), [&](const auto& loop) {
                return loop.bank == status.active_autoloop.bank &&
                    loop.slot == status.active_autoloop.slot;
            });
        if (matched_loop != live.autoloops.end()) {
            playback << L" — " << widen(matched_loop->name);
        }
        playback << L"  •  " << std::lround(status.active_autoloop_progress * 100.0F)
                 << L"%  •  " << autoloop_repeat_name(status.active_autoloop_repeat)
                 << L"  •  cycle " << status.active_autoloop_completed_cycles + 1U;
    }
    static_cast<void>(::SetWindowTextW(
        ::GetDlgItem(page, IdLiveAutoloopPlayback), playback.str().c_str()));

    std::wostringstream metrics;
    metrics << L"OS2L: " << adapter_state_name(status.os2l)
            << (status.os2l_listen_port != 0U
                    ? L" on " + widen(live_project().connections.os2l_bind) + L":" +
                        std::to_wstring(status.os2l_listen_port)
                    : std::wstring{})
            << L"    Discovery: " << adapter_state_name(status.os2l_discovery)
            << L"    MIDI: " << adapter_state_name(status.midi_input)
            << L"    Art-Net: " << adapter_state_name(status.artnet)
            << L"    sACN: " << adapter_state_name(status.sacn)
            << L"    USB U1/U2: " << adapter_state_name(status.dmx_usb_pro[0])
            << L"/" << adapter_state_name(status.dmx_usb_pro[1])
            << L"    Micro: " << adapter_state_name(status.soundswitch_micro)
            << L"\r\nFrames: " << status.frames
            << L"    Output failures: " << status.output_send_failures
            << L"    Queue drops: " << status.output_queue_drops
            << L"    Superseded: " << status.output_superseded_frames
            << L"    Max jitter: " << status.max_jitter_us << L" µs";
    if (status.active_track_script >= 0) {
        const auto& live = live_project();
        const auto index = static_cast<std::size_t>(status.active_track_script);
        metrics << L"\r\nTrack script: ";
        if (index < live.track_scripts.size()) {
            const auto& track = live.track_scripts[index];
            metrics << widen(track.name) << L"  •  beat " << std::fixed << std::setprecision(3)
                    << status.active_track_script_beat << L"  •  "
                    << status.active_track_script_consumed_cues << L"/" << track.cues.size()
                    << L" cues";
        } else {
            metrics << L"Unknown";
        }
    }
    static_cast<void>(::SetWindowTextW(::GetDlgItem(page, IdLiveMetrics), metrics.str().c_str()));

    const auto overrides_page = pages_[static_cast<std::size_t>(Page::Overrides)];
    if (overrides_page != nullptr) {
        std::wostringstream override_count;
        override_count << L"Runner manual overrides: " << status.manual_override_count
                       << (status.manual_override_count == 1U ? L" property" : L" properties");
        static_cast<void>(::SetWindowTextW(
            ::GetDlgItem(overrides_page, IdOverridesActiveCount), override_count.str().c_str()));
        const bool can_override = status.state == emberlights::RunnerState::Running;
        static_cast<void>(::EnableWindow(
            ::GetDlgItem(overrides_page, IdOverridesApply), can_override));
        static_cast<void>(::EnableWindow(
            ::GetDlgItem(overrides_page, IdOverridesRelease), can_override));
        static_cast<void>(::EnableWindow(
            ::GetDlgItem(overrides_page, IdOverridesReleaseAll), can_override));
    }
}

void Application::refresh_overrides() {
    const auto page = pages_[static_cast<std::size_t>(Page::Overrides)];
    if (page == nullptr) {
        return;
    }
    const auto fixtures = ::GetDlgItem(page, IdOverridesFixture);
    static_cast<void>(::SendMessageW(fixtures, LB_RESETCONTENT, 0, 0));
    const auto& live = live_project();
    live_view_model_.load_project(live);
    live_view_model_.update(runner_.status());
    const auto& targets = live_view_model_.override_targets();
    for (std::size_t index = 0U; index < targets.size(); ++index) {
        const auto& target = targets[index];
        std::ostringstream label;
        label << (target.kind == emberlights::LiveOverrideTargetKind::Fixture
                      ? "Fixture — "
                      : "Group — ")
              << target.name << " (" << target.fixture_count << " fixture"
              << (target.fixture_count == 1U ? "" : "s") << ')';
        if (!target.complete) {
            label << " — incomplete";
        }
        listbox_add(fixtures, widen(label.str()), static_cast<std::intptr_t>(index));
    }
    if (!targets.empty()) {
        static_cast<void>(::SendMessageW(fixtures, LB_SETCURSEL, 0, 0));
    }
    refresh_override_properties();
    set_control_text(::GetDlgItem(page, IdOverridesValue), "100");
    set_page_message(
        Page::Overrides,
        IdOverridesMessage,
        live.fixtures.empty()
            ? "Patch at least one fixture before using Live Overrides."
            : "Select an active fixture or group, then start the show to apply transient manual overrides.",
        live.fixtures.empty());
}

void Application::refresh_override_properties() {
    const auto page = pages_[static_cast<std::size_t>(Page::Overrides)];
    const auto targets = ::GetDlgItem(page, IdOverridesFixture);
    const auto properties = ::GetDlgItem(page, IdOverridesProperty);
    const auto previous_property = static_cast<showcore::Property>(combo_selected_data(
        properties, static_cast<std::intptr_t>(showcore::Property::Count)));
    static_cast<void>(::SendMessageW(properties, CB_RESETCONTENT, 0, 0));
    const auto selected = static_cast<int>(::SendMessageW(targets, LB_GETCURSEL, 0, 0));
    if (selected < 0) {
        return;
    }
    const auto target_index = static_cast<std::size_t>(::SendMessageW(
        targets, LB_GETITEMDATA, selected, 0));
    if (target_index >= live_view_model_.override_targets().size()) {
        return;
    }
    const auto& target = live_view_model_.override_targets()[target_index];
    std::size_t selected_combo = 0U;
    std::size_t added = 0U;
    for (std::size_t index = 0U; index < showcore::kPropertyCount; ++index) {
        const auto property = static_cast<showcore::Property>(index);
        if (!target.supports_any(property) ||
            property == showcore::Property::Fog ||
            property == showcore::Property::Haze ||
            property == showcore::Property::Laser ||
            property == showcore::Property::Spark ||
            (property == showcore::Property::Strobe &&
             !live_view_model_.safety().strobe_allowed)) {
            continue;
        }
        std::string label{emberlights::property_name(property)};
        if (!target.supports_all(property)) {
            label += " (" + std::to_string(target.support_count(property)) + "/" +
                std::to_string(target.fixture_count) + " fixtures)";
        }
        combo_add(properties, widen(label), static_cast<std::intptr_t>(property));
        if (property == previous_property) {
            selected_combo = added;
        }
        ++added;
    }
    if (added > 0U) {
        static_cast<void>(::SendMessageW(
            properties, CB_SETCURSEL, static_cast<WPARAM>(selected_combo), 0));
    }
}

void Application::refresh_profiles() {
    const auto page = pages_[static_cast<std::size_t>(Page::Profiles)];
    const auto list = ::GetDlgItem(page, IdProfileList);
    static_cast<void>(::SendMessageW(list, LB_RESETCONTENT, 0, 0));
    for (std::size_t index = 0; index < project_.fixture_profiles.size(); ++index) {
        listbox_add(list, widen(project_.fixture_profiles[index].name), index);
    }
    if (profile_index_ >= 0 &&
        static_cast<std::size_t>(profile_index_) < project_.fixture_profiles.size()) {
        static_cast<void>(::SendMessageW(list, LB_SETCURSEL, profile_index_, 0));
        select_profile(profile_index_);
    } else {
        new_profile();
    }
}

void Application::refresh_patch() {
    const auto page = pages_[static_cast<std::size_t>(Page::Patch)];
    const auto list = ::GetDlgItem(page, IdPatchList);
    ListView_DeleteAllItems(list);
    for (std::size_t index = 0; index < project_.fixtures.size(); ++index) {
        const auto& fixture = project_.fixtures[index];
        const auto profile = std::find_if(
            project_.fixture_profiles.begin(),
            project_.fixture_profiles.end(),
            [&](const auto& candidate) { return candidate.id == fixture.profile_id; });
        listview_set_row(
            list,
            static_cast<int>(index),
            static_cast<LPARAM>(index),
            {widen(fixture.name),
             widen(fixture.id),
             profile != project_.fixture_profiles.end() ? widen(profile->name) : L"Missing",
             widen(number_text(fixture.universe)),
             widen(number_text(fixture.address)),
             profile != project_.fixture_profiles.end() ? widen(number_text(profile->footprint)) : L"—"});
    }
    const auto profile_combo = ::GetDlgItem(page, IdPatchProfile);
    static_cast<void>(::SendMessageW(profile_combo, CB_RESETCONTENT, 0, 0));
    for (std::size_t index = 0; index < project_.fixture_profiles.size(); ++index) {
        combo_add(profile_combo, widen(project_.fixture_profiles[index].name),
                  static_cast<std::intptr_t>(index));
    }
    const auto universe = ::GetDlgItem(page, IdPatchUniverse);
    static_cast<void>(::SendMessageW(universe, CB_RESETCONTENT, 0, 0));
    combo_add(universe, L"1", 1);
    combo_add(universe, L"2", 2);
    if (fixture_index_ >= 0 && static_cast<std::size_t>(fixture_index_) < project_.fixtures.size()) {
        select_fixture(fixture_index_);
    } else {
        new_fixture();
    }
}

void Application::refresh_groups() {
    const auto page = pages_[static_cast<std::size_t>(Page::Groups)];
    const auto list = ::GetDlgItem(page, IdGroupList);
    static_cast<void>(::SendMessageW(list, LB_RESETCONTENT, 0, 0));
    for (std::size_t index = 0; index < project_.groups.size(); ++index) {
        const auto& group = project_.groups[index];
        std::ostringstream label;
        label << group.name << " (" << group.fixture_ids.size() << ")";
        listbox_add(list, widen(label.str()), index);
    }
    if (group_index_ >= 0 && static_cast<std::size_t>(group_index_) < project_.groups.size()) {
        static_cast<void>(::SendMessageW(list, LB_SETCURSEL, group_index_, 0));
        select_group(group_index_);
    } else {
        new_group();
    }
}

void Application::refresh_looks() {
    const auto page = pages_[static_cast<std::size_t>(Page::Looks)];
    const auto list = ::GetDlgItem(page, IdLookList);
    static_cast<void>(::SendMessageW(list, LB_RESETCONTENT, 0, 0));
    for (std::size_t index = 0; index < project_.looks.size(); ++index) {
        listbox_add(list, widen(project_.looks[index].name), index);
    }
    const auto ownership = ::GetDlgItem(page, IdLookOwnership);
    const auto previous_ownership = combo_selected_data(
        ownership, static_cast<std::intptr_t>(showcore::ValueMode::Set));
    static_cast<void>(::SendMessageW(ownership, CB_RESETCONTENT, 0, 0));
    combo_add(
        ownership,
        L"SET (own value)",
        static_cast<std::intptr_t>(showcore::ValueMode::Set));
    combo_add(
        ownership,
        L"FORCE_ZERO (hard off)",
        static_cast<std::intptr_t>(showcore::ValueMode::ForceZero));
    combo_add(
        ownership,
        L"RELEASE (lower layers)",
        static_cast<std::intptr_t>(showcore::ValueMode::Release));
    combo_select_data(ownership, previous_ownership);
    ::EnableWindow(
        ::GetDlgItem(page, IdLookValue),
        combo_selected_data(ownership) ==
            static_cast<std::intptr_t>(showcore::ValueMode::Set));
    refresh_look_targets();
    if (look_index_ >= 0 && static_cast<std::size_t>(look_index_) < project_.looks.size()) {
        static_cast<void>(::SendMessageW(list, LB_SETCURSEL, look_index_, 0));
        select_look(look_index_);
    } else {
        new_look();
    }
}

void Application::refresh_look_targets() {
    const auto page = pages_[static_cast<std::size_t>(Page::Looks)];
    const auto combo = ::GetDlgItem(page, IdLookTarget);
    const auto previous_id = selected_look_target_id();
    static_cast<void>(::SendMessageW(combo, CB_RESETCONTENT, 0, 0));
    std::intptr_t preferred = -1;
    for (std::size_t index = 0U; index < project_.fixtures.size(); ++index) {
        const auto& fixture = project_.fixtures[index];
        const auto* profile = emberlights::find_fixture_profile(project_, fixture.profile_id);
        std::ostringstream label;
        label << "Fixture: " << fixture.name;
        if (profile != nullptr) {
            label << " — " << profile->mode;
        }
        combo_add(combo, widen(label.str()), static_cast<std::intptr_t>(index));
        if (fixture.id == previous_id) {
            preferred = static_cast<std::intptr_t>(index);
        }
    }
    for (std::size_t index = 0U; index < project_.groups.size(); ++index) {
        const auto& group = project_.groups[index];
        const auto data = static_cast<std::intptr_t>(project_.fixtures.size() + index);
        std::ostringstream label;
        label << "Group: " << group.name << " (" << group.fixture_ids.size() << ')';
        combo_add(combo, widen(label.str()), data);
        if (group.id == previous_id) {
            preferred = data;
        }
    }
    combo_select_data(combo, preferred);
    refresh_look_capabilities();
}

void Application::refresh_look_capabilities() {
    const auto page = pages_[static_cast<std::size_t>(Page::Looks)];
    const auto target_id = selected_look_target_id();
    const auto capabilities = emberlights::inspect_fixture_target(project_, target_id);
    std::ostringstream summary;
    if (!capabilities.target_found) {
        summary << "Patch a fixture or create a non-empty group before authoring color.";
    } else {
        summary << capabilities.fixtures.size() << " fixture";
        if (capabilities.fixtures.size() != 1U) {
            summary << 's';
        }
        constexpr std::array properties{
            showcore::Property::Intensity,
            showcore::Property::Red,
            showcore::Property::Green,
            showcore::Property::Blue,
            showcore::Property::White,
            showcore::Property::Amber,
            showcore::Property::UV};
        for (const auto property : properties) {
            const auto& support = capabilities.capability(property);
            if (support.supported()) {
                summary << " • " << emberlights::property_name(property) << ' '
                        << support.supported_fixture_count << '/'
                        << support.target_fixture_count;
            }
        }
        if (capabilities.fixtures.size() == 1U) {
            summary << " • " << capabilities.fixtures.front().profile_id;
        }
        if (!capabilities.warnings.empty()) {
            summary << " • WARNING: " << capabilities.warnings.front();
        }
    }
    set_control_text(::GetDlgItem(page, IdLookCapabilities), summary.str());

    const auto properties = ::GetDlgItem(page, IdLookProperty);
    const auto previous_property = combo_selected_data(
        properties, static_cast<std::intptr_t>(showcore::Property::Intensity));
    static_cast<void>(::SendMessageW(properties, CB_RESETCONTENT, 0, 0));
    for (std::size_t index = 0U; index < showcore::kPropertyCount; ++index) {
        const auto property = static_cast<showcore::Property>(index);
        const auto& support = capabilities.capability(property);
        if (!support.supported()) {
            continue;
        }
        std::ostringstream label;
        label << emberlights::property_name(property);
        if (support.partial()) {
            label << " (" << support.supported_fixture_count << '/'
                  << support.target_fixture_count << ')';
        }
        combo_add(properties, widen(label.str()), static_cast<std::intptr_t>(property));
    }
    combo_select_data(properties, previous_property);
}

void Application::refresh_look_draft_view() {
    const auto page = pages_[static_cast<std::size_t>(Page::Looks)];
    set_control_text(
        ::GetDlgItem(page, IdLookAssignments),
        look_draft_.has_value() ? look_assignments_text(look_draft_->look) : "");
    set_control_text(::GetDlgItem(page, IdLookPreviewText), "");
    emberlights::StaticLookColor color;
    const auto target = emberlights::inspect_fixture_target(
        project_, selected_look_target_id());
    if (look_draft_.has_value() && !target.fixtures.empty()) {
        const auto fixture_id = target.fixtures.front().fixture_id;
        const auto value_for = [&](showcore::Property property, float fallback) {
            const auto found = std::find_if(
                look_draft_->look.assignments.begin(),
                look_draft_->look.assignments.end(),
                [&](const auto& assignment) {
                    return assignment.fixture_id == fixture_id &&
                        assignment.property == property;
                });
            if (found == look_draft_->look.assignments.end() ||
                found->value.mode == showcore::ValueMode::Release) {
                return fallback;
            }
            return found->value.mode == showcore::ValueMode::ForceZero
                ? 0.0F
                : found->value.value;
        };
        color.red = value_for(showcore::Property::Red, 0.0F);
        color.green = value_for(showcore::Property::Green, 0.0F);
        color.blue = value_for(showcore::Property::Blue, 0.0F);
        color.white = value_for(showcore::Property::White, 0.0F);
        color.amber = value_for(showcore::Property::Amber, 0.0F);
        color.uv = value_for(showcore::Property::UV, 0.0F);
        color.intensity = value_for(showcore::Property::Intensity, 1.0F);
    }
    set_static_look_color_controls(page, color);
}

void Application::refresh_autoloops() {
    const auto page = pages_[static_cast<std::size_t>(Page::Autoloops)];
    const auto list = ::GetDlgItem(page, IdAutoloopList);
    static_cast<void>(::SendMessageW(list, LB_RESETCONTENT, 0, 0));
    for (std::size_t index = 0; index < project_.autoloops.size(); ++index) {
        const auto& loop = project_.autoloops[index];
        std::ostringstream label;
        label << "B" << loop.bank + 1U << " / S" << static_cast<unsigned int>(loop.slot + 1U)
              << " — " << loop.name;
        listbox_add(list, widen(label.str()), index);
    }
    const auto repeat = ::GetDlgItem(page, IdAutoloopRepeat);
    static_cast<void>(::SendMessageW(repeat, CB_RESETCONTENT, 0, 0));
    combo_add(repeat, L"Once", static_cast<std::intptr_t>(showcore::AutoloopRepeat::Once));
    combo_add(repeat, L"Infinite", static_cast<std::intptr_t>(showcore::AutoloopRepeat::Infinite));
    combo_add(repeat, L"Track duration", static_cast<std::intptr_t>(showcore::AutoloopRepeat::TrackDuration));
    if (autoloop_index_ >= 0 &&
        static_cast<std::size_t>(autoloop_index_) < project_.autoloops.size()) {
        static_cast<void>(::SendMessageW(list, LB_SETCURSEL, autoloop_index_, 0));
        select_autoloop(autoloop_index_);
    } else {
        new_autoloop();
    }
}

void Application::refresh_tracks() {
    const auto page = pages_[static_cast<std::size_t>(Page::Tracks)];
    const auto list = ::GetDlgItem(page, IdTrackList);
    static_cast<void>(::SendMessageW(list, LB_RESETCONTENT, 0, 0));
    for (std::size_t index = 0; index < project_.track_scripts.size(); ++index) {
        const auto& track = project_.track_scripts[index];
        std::ostringstream label;
        label << track.name;
        const auto asset = std::find_if(
            project_.audio_assets.begin(), project_.audio_assets.end(), [&](const auto& candidate) {
                return candidate.id == track.audio_asset_id;
            });
        if (asset != project_.audio_assets.end()) {
            label << " — " << asset->name;
        } else if (!track.audio_key.empty()) {
            label << " — migration key";
        }
        label << " (" << track.cues.size() << " cue"
              << (track.cues.size() == 1U ? "" : "s") << ')';
        listbox_add(list, widen(label.str()), index);
    }
    if (track_index_ >= 0 &&
        static_cast<std::size_t>(track_index_) < project_.track_scripts.size()) {
        static_cast<void>(::SendMessageW(list, LB_SETCURSEL, track_index_, 0));
        select_track(track_index_);
    } else {
        new_track();
    }
}

void Application::refresh_track_audio_assets(std::string_view selected_asset_id) {
    const auto page = pages_[static_cast<std::size_t>(Page::Tracks)];
    const auto combo = ::GetDlgItem(page, IdTrackAudioAsset);
    static_cast<void>(::SendMessageW(combo, CB_RESETCONTENT, 0, 0));
    combo_add(combo, L"No linked audio asset", -1);
    std::intptr_t selected = -1;
    for (std::size_t index = 0; index < project_.audio_assets.size(); ++index) {
        const auto& asset = project_.audio_assets[index];
        std::ostringstream label;
        label << asset.name;
        if (asset.file_name != asset.name) {
            label << " — " << asset.file_name;
        }
        label << " (" << asset.size_bytes << " bytes)";
        combo_add(combo, widen(label.str()), static_cast<std::intptr_t>(index));
        if (asset.id == selected_asset_id) {
            selected = static_cast<std::intptr_t>(index);
        }
    }
    combo_select_data(combo, selected);
}

void Application::refresh_midi() {
    const auto page = pages_[static_cast<std::size_t>(Page::Midi)];
    const auto list = ::GetDlgItem(page, IdMidiList);
    ListView_DeleteAllItems(list);
    for (std::size_t index = 0; index < project_.midi_mappings.size(); ++index) {
        const auto& mapping = project_.midi_mappings[index];
        auto target = mapping.target_ref;
        if (target.empty() && mapping.action.property < showcore::Property::Count) {
            target = std::string(emberlights::property_name(mapping.action.property));
        } else if (target.empty() &&
                   (mapping.action.type == showcore::ActionType::SelectAutoloopBank ||
                    mapping.action.type == showcore::ActionType::SetAutoloopBankEnabled) &&
                   mapping.action.target_id < showcore::kMaxAutoloopBanks) {
            target = "Bank " + std::to_string(mapping.action.target_id + 1U);
        }
        listview_set_row(
            list,
            static_cast<int>(index),
            static_cast<LPARAM>(index),
            {widen(midi_message_name(mapping)),
             widen(action_name(mapping.action.type)),
             widen(target),
             widen(behavior_name(mapping.behavior))});
    }

    const auto action = ::GetDlgItem(page, IdMidiAction);
    static_cast<void>(::SendMessageW(action, CB_RESETCONTENT, 0, 0));
    constexpr std::array<showcore::ActionType, 22> actions{{
        showcore::ActionType::Blackout,
        showcore::ActionType::BlackoutGroup,
        showcore::ActionType::WorkLight,
        showcore::ActionType::TriggerLook,
        showcore::ActionType::ClearLook,
        showcore::ActionType::TriggerAutoloop,
        showcore::ActionType::ClearAutoloop,
        showcore::ActionType::SelectAutoloopBank,
        showcore::ActionType::SelectAllAutoloopBanks,
        showcore::ActionType::SetAutoloopBankEnabled,
        showcore::ActionType::TriggerTrackScript,
        showcore::ActionType::ClearTrackScript,
        showcore::ActionType::ClearManualOverrides,
        showcore::ActionType::NextAutoloop,
        showcore::ActionType::PreviousAutoloop,
        showcore::ActionType::TapTempo,
        showcore::ActionType::SetProperty,
        showcore::ActionType::SetGroupProperty,
        showcore::ActionType::ArmFog,
        showcore::ActionType::ArmHaze,
        showcore::ActionType::ArmLaser,
        showcore::ActionType::ArmSpark}};
    for (const auto value : actions) {
        combo_add(action, widen(action_name(value)), static_cast<std::intptr_t>(value));
    }
    static_cast<void>(::SendMessageW(action, CB_SETCURSEL, 0, 0));

    const auto behavior = ::GetDlgItem(page, IdMidiBehavior);
    static_cast<void>(::SendMessageW(behavior, CB_RESETCONTENT, 0, 0));
    for (const auto value : {
             showcore::MappingBehavior::Momentary,
             showcore::MappingBehavior::Toggle,
             showcore::MappingBehavior::Latch,
             showcore::MappingBehavior::Continuous,
             showcore::MappingBehavior::Relative}) {
        combo_add(behavior, widen(behavior_name(value)), static_cast<std::intptr_t>(value));
    }
    static_cast<void>(::SendMessageW(behavior, CB_SETCURSEL, 0, 0));

    const auto property = ::GetDlgItem(page, IdMidiProperty);
    static_cast<void>(::SendMessageW(property, CB_RESETCONTENT, 0, 0));
    for (std::size_t index = 0; index < showcore::kPropertyCount; ++index) {
        const auto value = static_cast<showcore::Property>(index);
        combo_add(property, widen(emberlights::property_name(value)),
                  static_cast<std::intptr_t>(value));
    }
    static_cast<void>(::SendMessageW(property, CB_SETCURSEL, 0, 0));
    update_midi_targets();
}

void Application::refresh_midi_ports() {
    midi_inputs_ = showcore::enumerate_winmm_midi_inputs();
    midi_outputs_ = showcore::enumerate_winmm_midi_outputs();
    dmx_serial_ports_ = showcore::enumerate_dmx_serial_ports();
    refresh_connections();
}

void Application::refresh_connections() {
    if (pages_[static_cast<std::size_t>(Page::Connections)] == nullptr) {
        return;
    }
    const auto page = pages_[static_cast<std::size_t>(Page::Connections)];
    set_control_text(::GetDlgItem(page, IdProjectName), project_.name);
    Button_SetCheck(::GetDlgItem(page, IdOs2lEnabled),
                    project_.connections.os2l_enabled ? BST_CHECKED : BST_UNCHECKED);
    set_control_text(::GetDlgItem(page, IdOs2lBind), project_.connections.os2l_bind);
    set_control_text(::GetDlgItem(page, IdOs2lPort), number_text(project_.connections.os2l_port));
    Button_SetCheck(::GetDlgItem(page, IdArtnetEnabled),
                    project_.connections.artnet_enabled ? BST_CHECKED : BST_UNCHECKED);
    set_control_text(::GetDlgItem(page, IdArtnetDestination),
                     project_.connections.artnet_destination);
    set_control_text(::GetDlgItem(page, IdArtnetBase), number_text(project_.connections.artnet_base));
    Button_SetCheck(::GetDlgItem(page, IdSacnEnabled),
                    project_.connections.sacn_enabled ? BST_CHECKED : BST_UNCHECKED);
    set_control_text(::GetDlgItem(page, IdSacnDestination), project_.connections.sacn_destination);
    set_control_text(::GetDlgItem(page, IdSacnBase),
                     number_text(project_.connections.sacn_universe_base));

    auto populate_dmx_port = [&](int control_id, const std::string& configured) {
        const auto combo = ::GetDlgItem(page, control_id);
        static_cast<void>(::SendMessageW(combo, CB_RESETCONTENT, 0, 0));
        combo_add(combo, L"Disabled", -1);
        std::intptr_t selected = -1;
        for (std::size_t index = 0; index < dmx_serial_ports_.count; ++index) {
            const auto name = dmx_serial_ports_.ports[index].name();
            combo_add(combo, widen(name), static_cast<std::intptr_t>(index));
            if (name == configured) {
                selected = static_cast<std::intptr_t>(index);
            }
        }
        if (!configured.empty() && selected < 0) {
            combo_add(combo, widen(configured + " (not connected)"), -2);
            selected = -2;
        }
        combo_select_data(combo, selected);
    };
    populate_dmx_port(
        IdDmxUsbProUniverse1, project_.connections.dmx_usb_pro_ports[0]);
    populate_dmx_port(
        IdDmxUsbProUniverse2, project_.connections.dmx_usb_pro_ports[1]);

    const auto micro_universe = ::GetDlgItem(page, IdSoundSwitchMicroUniverse);
    static_cast<void>(::SendMessageW(micro_universe, CB_RESETCONTENT, 0, 0));
    combo_add(micro_universe, L"Disabled", 0);
    combo_add(micro_universe, L"Universe 1", 1);
    combo_add(micro_universe, L"Universe 2", 2);
    combo_select_data(micro_universe, project_.connections.soundswitch_micro_universe);

    const auto micro_framing = ::GetDlgItem(page, IdSoundSwitchMicroFraming);
    static_cast<void>(::SendMessageW(micro_framing, CB_RESETCONTENT, 0, 0));
    combo_add(
        micro_framing,
        L"SoundSwitch native JLS1",
        static_cast<std::intptr_t>(
            showcore::SoundSwitchMicroFraming::NativeJls1));
    combo_select_data(
        micro_framing,
        static_cast<std::intptr_t>(project_.connections.soundswitch_micro_framing));
    const auto control_one_mode = ::GetDlgItem(page, IdSoundSwitchControlOneMode);
    static_cast<void>(::SendMessageW(control_one_mode, CB_RESETCONTENT, 0, 0));
    combo_add(control_one_mode, L"Disabled", 0);
    combo_add(
        control_one_mode,
        L"Experimental (unqualified) — Jack 1 = U1, Jack 2 = U2",
        1);
    combo_select_data(
        control_one_mode,
        project_.connections.soundswitch_control_one_experimental ? 1 : 0);
    set_control_text(::GetDlgItem(page, IdFrameRate), number_text(project_.connections.frame_rate));
    set_control_text(::GetDlgItem(page, IdManualBpm), number_text(project_.connections.manual_bpm));

    const auto input = ::GetDlgItem(page, IdMidiInput);
    const auto output = ::GetDlgItem(page, IdMidiOutput);
    static_cast<void>(::SendMessageW(input, CB_RESETCONTENT, 0, 0));
    static_cast<void>(::SendMessageW(output, CB_RESETCONTENT, 0, 0));
    combo_add(input, L"Disabled", -1);
    combo_add(output, L"Disabled", -1);
    for (std::size_t index = 0; index < midi_inputs_.count; ++index) {
        combo_add(input, widen(midi_inputs_.ports[index].name()),
                  static_cast<std::intptr_t>(midi_inputs_.ports[index].system_index));
    }
    for (std::size_t index = 0; index < midi_outputs_.count; ++index) {
        combo_add(output, widen(midi_outputs_.ports[index].name()),
                  static_cast<std::intptr_t>(midi_outputs_.ports[index].system_index));
    }
    combo_select_data(input, project_.connections.midi_input_index);
    combo_select_data(output, project_.connections.midi_output_index);
}

void Application::refresh_safety() {
    const auto page = pages_[static_cast<std::size_t>(Page::Safety)];
    if (page == nullptr) {
        return;
    }
    Button_SetCheck(::GetDlgItem(page, IdSafetyFogArm),
                    project_.safety.fog_requires_arm ? BST_CHECKED : BST_UNCHECKED);
    Button_SetCheck(::GetDlgItem(page, IdSafetyHazeArm),
                    project_.safety.haze_requires_arm ? BST_CHECKED : BST_UNCHECKED);
    Button_SetCheck(::GetDlgItem(page, IdSafetyLaserArm),
                    project_.safety.laser_requires_arm ? BST_CHECKED : BST_UNCHECKED);
    Button_SetCheck(::GetDlgItem(page, IdSafetySparkArm),
                    project_.safety.spark_requires_arm ? BST_CHECKED : BST_UNCHECKED);
    Button_SetCheck(::GetDlgItem(page, IdSafetyStrobeAllowed),
                    project_.safety.strobe_allowed ? BST_CHECKED : BST_UNCHECKED);
    set_control_text(::GetDlgItem(page, IdSafetyMaxStrobe),
                     number_text(project_.safety.max_strobe));
    set_control_text(::GetDlgItem(page, IdSafetyMaxIntensity),
                     number_text(project_.safety.max_intensity));
}

std::string Application::diagnostics_text() const {
    const auto status = runner_.status();
    const auto validation = emberlights::validate_project(project_);
    std::ostringstream output;
    output << "EmberLights " << emberlights::kVersion
           << " (commit " << emberlights::kCommit << ")\r\n"
           << "V1 preflight and runtime health snapshot\r\n"
           << "Project: " << project_.name << " (" << project_.id << ")\r\n"
           << "Project file: " << (current_path_.empty() ? "Unsaved" : current_path_.string())
           << "\r\nFixtures: " << project_.fixtures.size()
           << "  Profiles: " << project_.fixture_profiles.size()
           << "  Groups: " << project_.groups.size()
           << "  Static Looks: " << project_.looks.size()
           << "  Autoloops: " << project_.autoloops.size()
           << "  Track scripts: " << project_.track_scripts.size()
           << "  Audio assets: " << project_.audio_assets.size()
           << "  MIDI mappings: " << project_.midi_mappings.size() << "\r\n"
           << "Validation: " << validation.error_count() << " error(s), "
           << validation.warning_count() << " warning(s)\r\n\r\n"
           << "Runner: " << narrow(runner_state_name(status.state))
           << "  BPM: " << status.bpm << "  Beat: " << status.beat_position
           << "  Active script: " << status.active_track_script
           << "  Navigation bank mask: 0x" << std::hex
           << status.active_autoloop_bank_mask << std::dec
           << "  Manual overrides: " << status.manual_override_count << "\r\n";
    if (status.active_autoloop.valid()) {
        output << "Active Autoloop: B" << status.active_autoloop.bank + 1U
               << "/S" << static_cast<unsigned int>(status.active_autoloop.slot + 1U)
               << "  Progress: " << std::lround(status.active_autoloop_progress * 100.0F)
               << "%  Repeat: " << narrow(autoloop_repeat_name(status.active_autoloop_repeat))
               << "  Completed cycles: " << status.active_autoloop_completed_cycles << "\r\n";
    } else {
        output << "Active Autoloop: none\r\n";
    }
    output << "OS2L: " << narrow(adapter_state_name(status.os2l))
           << "  Discovery: " << narrow(adapter_state_name(status.os2l_discovery))
           << "  MIDI: " << narrow(adapter_state_name(status.midi_input))
           << "  Art-Net: " << narrow(adapter_state_name(status.artnet))
           << "  sACN: " << narrow(adapter_state_name(status.sacn))
           << "  USB U1: " << narrow(adapter_state_name(status.dmx_usb_pro[0]))
           << "  USB U2: " << narrow(adapter_state_name(status.dmx_usb_pro[1]))
           << "  SoundSwitch Micro: "
           << narrow(soundswitch_micro_state_name(status.soundswitch_micro))
           << "  Control One DMX: "
           << narrow(adapter_state_name(status.soundswitch_control_one)) << "\r\n"
           << "Micro project setting: universe "
           << static_cast<unsigned int>(project_.connections.soundswitch_micro_universe)
           << "  framing: "
           << narrow(soundswitch_micro_framing_name(
                  project_.connections.soundswitch_micro_framing))
           << "  accepted writes: " << status.soundswitch_micro_write_frames
           << "  failed writes: " << status.soundswitch_micro_write_failures
           << "  last WinUSB error: " << status.soundswitch_micro_last_error
           << "  last non-zero slots: "
           << status.soundswitch_micro_last_nonzero_slots << "\r\n"
           << "Control One project setting: "
           << (project_.connections.soundswitch_control_one_experimental
                   ? "EXPERIMENTAL / NOT PHYSICALLY QUALIFIED; jack 1=U1, jack 2=U2"
                   : "disabled")
           << "\r\n"
           << "Output backend health:\r\n";
    for (const auto& backend : status.output_backends) {
        const auto& descriptor = showcore::output_backend_descriptor(backend.kind);
        output << "  " << descriptor.name << " U"
               << static_cast<unsigned int>(backend.first_source_universe);
        if (backend.source_universe_count > 1U) {
            output << "-" << static_cast<unsigned int>(
                backend.first_source_universe + backend.source_universe_count - 1U);
        }
        output << ": " << showcore::output_health_state_name(backend.state)
               << "  configured: " << (backend.configured ? "yes" : "no")
               << "  open: " << backend.open_successes << "/" << backend.open_attempts
               << "  reconnects: " << backend.reconnects
               << "  frames: " << backend.frames_accepted << "/"
               << backend.frames_attempted
               << "  failures: " << backend.frames_failed
               << "  last error: " << backend.last_error
               << "  non-zero slots: " << backend.last_nonzero_slots << "\r\n";
    }
    output << "Frames: " << status.frames << "  Output frames: " << status.output_frames
           << "  Send failures: " << status.output_send_failures
           << "  Queue drops: " << status.output_queue_drops
           << "  Superseded stale frames: " << status.output_superseded_frames << "\r\n"
           << "OS2L connections: " << status.os2l_connections
           << "  messages: " << status.os2l_messages
           << "  decode errors: " << status.os2l_decode_errors
           << "  dropped named actions: " << status.dropped_os2l_actions
           << "  listening: " << project_.connections.os2l_bind << ":"
           << status.os2l_listen_port
           << "  last socket error: " << status.os2l_last_error
           << "  discovery: " << narrow(adapter_state_name(status.os2l_discovery))
           << "  discovery error: " << status.os2l_discovery_last_error << "\r\n"
           << "MIDI messages: " << status.midi_messages
           << "  dropped actions: " << status.dropped_midi_actions
           << "\r\nRunner uptime: " << status.uptime_ms << " ms"
           << "  last frame age: " << status.last_frame_age_ms << " ms\r\n"
           << "Scheduler jitter p99: " << status.jitter_p99_us << " µs"
           << "  max: " << status.max_jitter_us << " µs"
           << "  samples: " << status.jitter_samples << "\r\n"
           << "Deadline misses (>5 ms): " << status.deadline_misses
           << "  scheduler resyncs: " << status.scheduler_resyncs << "\r\n"
           << "Package generation: " << status.package_generation
           << "  activations: " << status.package_activations
           << "  activation failures: " << status.package_activation_failures << "\r\n"
           << "Last-known-good snapshot: "
           << (current_path_.empty()
                   ? "Unavailable (project is unsaved)"
                   : emberlights::project_active_path(current_path_).string())
           << "\r\nSaved-version history: "
           << (current_path_.empty()
                   ? "Unavailable (project is unsaved)"
                   : emberlights::project_history_directory(current_path_).string())
           << " (up to " << emberlights::kMaximumProjectHistoryEntries << ")"
           << "\r\n\r\n";
    for (const auto& issue : validation.issues) {
        output << (issue.severity == emberlights::ProjectIssueSeverity::Error ? "ERROR" : "WARNING")
               << " [" << issue.code << "] " << issue.subject << ": " << issue.message
               << "\r\n";
    }
    return output.str();
}

void Application::refresh_diagnostics() {
    const auto page = pages_[static_cast<std::size_t>(Page::Diagnostics)];
    if (page != nullptr) {
        set_control_text(::GetDlgItem(page, IdDiagnosticsText), diagnostics_text());
    }
}

void Application::handle_notify(const NMHDR& notification) {
    if (notification.idFrom == IdPatchList && notification.code == LVN_ITEMCHANGED && !refreshing_) {
        const auto index = listview_selected_data(notification.hwndFrom);
        if (index >= 0) {
            select_fixture(index);
        }
    }
}

void Application::handle_timer() {
    refresh_live_status();
    if (active_page_ == Page::Diagnostics) {
        refresh_diagnostics();
    }
    if (midi_learning_) {
        showcore::MidiMessage message;
        bool received = false;
        if (learn_uses_runner_) {
            emberlights::RunnerMidiMonitorEvent event;
            if (runner_.poll_midi_monitor(event)) {
                message = event.message;
                received = true;
            }
        } else if (learn_input_.poll(message)) {
            received = true;
        }
        if (received) {
            finish_midi_learn(message);
        }
    }
}

void Application::handle_command(int id, int notification, HWND) {
    if (id >= IdNavLive && id <= IdNavDiagnostics) {
        show_page(static_cast<Page>(id - IdNavLive));
        return;
    }
    if (notification == LBN_SELCHANGE && !refreshing_) {
        if (id == IdProfileList) {
            select_profile(static_cast<std::int32_t>(::SendMessageW(
                ::GetDlgItem(pages_[static_cast<std::size_t>(Page::Profiles)], IdProfileList),
                LB_GETCURSEL, 0, 0)));
        } else if (id == IdGroupList) {
            select_group(static_cast<std::int32_t>(::SendMessageW(
                ::GetDlgItem(pages_[static_cast<std::size_t>(Page::Groups)], IdGroupList),
                LB_GETCURSEL, 0, 0)));
        } else if (id == IdLookList) {
            select_look(static_cast<std::int32_t>(::SendMessageW(
                ::GetDlgItem(pages_[static_cast<std::size_t>(Page::Looks)], IdLookList),
                LB_GETCURSEL, 0, 0)));
        } else if (id == IdAutoloopList) {
            select_autoloop(static_cast<std::int32_t>(::SendMessageW(
                ::GetDlgItem(pages_[static_cast<std::size_t>(Page::Autoloops)], IdAutoloopList),
                LB_GETCURSEL, 0, 0)));
        } else if (id == IdTrackList) {
            select_track(static_cast<std::int32_t>(::SendMessageW(
                ::GetDlgItem(pages_[static_cast<std::size_t>(Page::Tracks)], IdTrackList),
                LB_GETCURSEL, 0, 0)));
        } else if (id == IdOverridesFixture) {
            refresh_override_properties();
        }
        return;
    }
    if (id == IdMidiAction && notification == CBN_SELCHANGE && !refreshing_) {
        update_midi_targets();
        return;
    }
    if (id == IdLookTarget && notification == CBN_SELCHANGE && !refreshing_) {
        refresh_look_capabilities();
        refresh_look_draft_view();
        return;
    }
    if (id == IdLookOwnership && notification == CBN_SELCHANGE && !refreshing_) {
        const auto page = pages_[static_cast<std::size_t>(Page::Looks)];
        ::EnableWindow(
            ::GetDlgItem(page, IdLookValue),
            combo_selected_data(::GetDlgItem(page, IdLookOwnership)) ==
                static_cast<std::intptr_t>(showcore::ValueMode::Set));
        return;
    }

    std::optional<emberlights::ProjectDocument> before_edit;
    if (is_authoring_edit_command(id)) {
        before_edit = project_;
    }

    switch (id) {
    case IdFileNew: new_project(); break;
    case IdFileOpen: open_project_dialog(); break;
    case IdFileSave: static_cast<void>(save_project(false)); break;
    case IdFileSaveAs: static_cast<void>(save_project(true)); break;
    case IdFileRestoreHistory: restore_project_history_dialog(); break;
    case IdEditUndo: undo_project_edit(); break;
    case IdEditRedo: redo_project_edit(); break;
    case IdFileInspectSoundSwitch: inspect_soundswitch_dialog(); break;
    case IdFileCompareSoundSwitch: compare_soundswitch_dialog(); break;
    case IdFileBundleSoundSwitch: bundle_soundswitch_dialog(); break;
    case IdFileExit: static_cast<void>(::SendMessageW(window_, WM_CLOSE, 0, 0)); break;
    case IdShowValidate:
    case IdDiagnosticsValidate: validate_project(true); break;
    case IdShowStartStop:
    case IdLiveStartStop:
        static_cast<void>(ui_commands_.invoke(
            {emberlights::UiCommandId::ShowToggleRunning}));
        break;
    case IdHelpAbout:
    {
        const auto about = widen(
            std::string("EmberLights ") + std::string(emberlights::kVersion) +
            "\n\nOffline-first DJ and event lighting workstation.\n"
            "Windows V1 development build.\n\nCommit: " + std::string(emberlights::kCommit));
        ::MessageBoxW(
            window_,
            about.c_str(),
            L"About EmberLights",
            MB_OK | MB_ICONINFORMATION);
        break;
    }
    case IdOverridesApply: apply_fixture_override(true); break;
    case IdOverridesRelease: apply_fixture_override(false); break;
    case IdOverridesReleaseAll: clear_fixture_overrides(); break;
    case IdLiveBlackout:
        static_cast<void>(ui_commands_.invoke(
            {emberlights::UiCommandId::BlackoutToggle}));
        break;
    case IdLiveWorkLight:
        static_cast<void>(ui_commands_.invoke(
            {emberlights::UiCommandId::WorkLightToggle}));
        break;
    case IdLiveApplyBpm: {
        double bpm = 0.0;
        emberlights::UiCommandInvocation invocation;
        invocation.command = emberlights::UiCommandId::ManualBpmSet;
        invocation.number_value = bpm;
        if (!parse_number(control_text(::GetDlgItem(
                pages_[static_cast<std::size_t>(Page::Live)], IdLiveBpm)), bpm)) {
            set_status(L"Enter a BPM from 20 through 300 while the show is running.");
            break;
        }
        invocation.number_value = bpm;
        if (ui_commands_.invoke(invocation) !=
            emberlights::UiInvocationResult::Accepted) {
            set_status(L"Enter a BPM from 20 through 300 while the show is running.");
        }
        break;
    }
    case IdLiveTap:
        static_cast<void>(ui_commands_.invoke(
            {emberlights::UiCommandId::TapTempo}));
        break;
    case IdLiveTriggerLook: {
        const auto list = ::GetDlgItem(pages_[static_cast<std::size_t>(Page::Live)], IdLiveLooks);
        const auto selected = static_cast<int>(::SendMessageW(list, LB_GETCURSEL, 0, 0));
        if (selected >= 0) {
            const auto index = static_cast<std::size_t>(::SendMessageW(
                list, LB_GETITEMDATA, selected, 0));
            const auto& live = live_project();
            if (index < live.looks.size()) {
                emberlights::UiCommandInvocation invocation;
                invocation.command = emberlights::UiCommandId::StaticLookToggle;
                invocation.target_id = live.looks[index].id;
                static_cast<void>(ui_commands_.invoke(invocation));
            }
        }
        break;
    }
    case IdLiveClearLook:
        static_cast<void>(ui_commands_.invoke(
            {emberlights::UiCommandId::StaticLookClear}));
        break;
    case IdLiveTriggerAutoloop: {
        const auto list = ::GetDlgItem(
            pages_[static_cast<std::size_t>(Page::Live)], IdLiveAutoloops);
        const auto selected = static_cast<int>(::SendMessageW(list, LB_GETCURSEL, 0, 0));
        if (selected >= 0) {
            const auto index = static_cast<std::size_t>(::SendMessageW(
                list, LB_GETITEMDATA, selected, 0));
            const auto& live = live_project();
            if (index < live.autoloops.size()) {
                emberlights::UiCommandInvocation invocation;
                invocation.command = emberlights::UiCommandId::AutoloopLaunch;
                invocation.target_id = live.autoloops[index].id;
                static_cast<void>(ui_commands_.invoke(invocation));
            }
        }
        break;
    }
    case IdLivePreviousAutoloop:
        static_cast<void>(ui_commands_.invoke(
            {emberlights::UiCommandId::AutoloopPrevious}));
        break;
    case IdLiveNextAutoloop:
        static_cast<void>(ui_commands_.invoke(
            {emberlights::UiCommandId::AutoloopNext}));
        break;
    case IdLiveClearAutoloop:
        static_cast<void>(ui_commands_.invoke(
            {emberlights::UiCommandId::AutoloopClear}));
        break;
    case IdLivePreviousAutoloopBankPage:
        if (live_autoloop_bank_page_ == 0U) {
            live_autoloop_bank_page_ = static_cast<std::uint16_t>(
                showcore::kAutoloopControlPageCount - 1U);
        } else {
            --live_autoloop_bank_page_;
        }
        refresh_live_status();
        break;
    case IdLiveNextAutoloopBankPage:
        live_autoloop_bank_page_ = static_cast<std::uint16_t>(
            (live_autoloop_bank_page_ + 1U) % showcore::kAutoloopControlPageCount);
        refresh_live_status();
        break;
    case IdLiveSelectAllAutoloopBanks: {
        const auto result = ui_commands_.invoke(
            {emberlights::UiCommandId::AutoloopBankFilterEnableAll});
        if (result != emberlights::UiInvocationResult::Accepted &&
            result != emberlights::UiInvocationResult::NoChange) {
            set_status(L"Start the show before changing Autoloop navigation banks.");
        }
        break;
    }
    case IdLiveAutoloopBank1:
    case IdLiveAutoloopBank2:
    case IdLiveAutoloopBank3:
    case IdLiveAutoloopBank4: {
        std::uint16_t offset = 0U;
        if (id == IdLiveAutoloopBank2) {
            offset = 1U;
        } else if (id == IdLiveAutoloopBank3) {
            offset = 2U;
        } else if (id == IdLiveAutoloopBank4) {
            offset = 3U;
        }
        const auto bank = static_cast<std::uint16_t>(
            live_autoloop_bank_page_ * showcore::kAutoloopBanksPerControlPage + offset);
        const auto enabled = Button_GetCheck(::GetDlgItem(
            pages_[static_cast<std::size_t>(Page::Live)], id)) == BST_CHECKED;
        emberlights::UiCommandInvocation invocation;
        invocation.command = emberlights::UiCommandId::AutoloopBankFilterSetEnabled;
        invocation.bank = bank;
        invocation.bool_value = enabled;
        const auto result = ui_commands_.invoke(invocation);
        if (result != emberlights::UiInvocationResult::Accepted &&
            result != emberlights::UiInvocationResult::NoChange) {
            set_status(L"Start the show before changing Autoloop navigation banks.");
            refresh_live_status();
        }
        break;
    }
    case IdLiveAutoloopBank1Only:
    case IdLiveAutoloopBank2Only:
    case IdLiveAutoloopBank3Only:
    case IdLiveAutoloopBank4Only: {
        std::uint16_t offset = 0U;
        if (id == IdLiveAutoloopBank2Only) {
            offset = 1U;
        } else if (id == IdLiveAutoloopBank3Only) {
            offset = 2U;
        } else if (id == IdLiveAutoloopBank4Only) {
            offset = 3U;
        }
        const auto bank = static_cast<std::uint16_t>(
            live_autoloop_bank_page_ * showcore::kAutoloopBanksPerControlPage + offset);
        emberlights::UiCommandInvocation invocation;
        invocation.command = emberlights::UiCommandId::AutoloopBankFilterSelectExclusive;
        invocation.bank = bank;
        const auto result = ui_commands_.invoke(invocation);
        if (result != emberlights::UiInvocationResult::Accepted &&
            result != emberlights::UiInvocationResult::NoChange) {
            set_status(L"Start the show before changing Autoloop navigation banks.");
        }
        break;
    }
    case IdLiveTriggerTrack: {
        const auto list = ::GetDlgItem(pages_[static_cast<std::size_t>(Page::Live)], IdLiveTracks);
        const auto selected = static_cast<int>(::SendMessageW(list, LB_GETCURSEL, 0, 0));
        if (selected >= 0) {
            const auto index = static_cast<std::size_t>(::SendMessageW(
                list, LB_GETITEMDATA, selected, 0));
            const auto& live = live_project();
            if (index < live.track_scripts.size()) {
                emberlights::UiCommandInvocation invocation;
                invocation.command = emberlights::UiCommandId::TrackScriptStart;
                invocation.target_id = live.track_scripts[index].id;
                static_cast<void>(ui_commands_.invoke(invocation));
            }
        }
        break;
    }
    case IdLiveClearTrack:
        static_cast<void>(ui_commands_.invoke(
            {emberlights::UiCommandId::TrackScriptClear}));
        break;
    case IdLiveFogArm:
    case IdLiveHazeArm:
    case IdLiveLaserArm:
    case IdLiveSparkArm: {
        emberlights::UiCommandInvocation invocation;
        invocation.command = emberlights::UiCommandId::HazardSetArmed;
        invocation.property = id == IdLiveFogArm ? showcore::Property::Fog
            : id == IdLiveHazeArm ? showcore::Property::Haze
            : id == IdLiveLaserArm ? showcore::Property::Laser
            : showcore::Property::Spark;
        invocation.bool_value = Button_GetCheck(::GetDlgItem(
            pages_[static_cast<std::size_t>(Page::Live)], id)) == BST_CHECKED;
        if (ui_commands_.invoke(invocation) !=
            emberlights::UiInvocationResult::Accepted) {
            refresh_live_status();
        }
        break;
    }
    case IdProfileNew: new_profile(); break;
    case IdProfileImportQlc: import_qlc_fixture_dialog(); break;
    case IdProfileDuplicate: duplicate_profile(); break;
    case IdProfileSave: save_profile(); break;
    case IdProfileDelete: delete_profile(); break;
    case IdPatchNew: new_fixture(); break;
    case IdPatchSave: save_fixture(); break;
    case IdPatchDelete: delete_fixture(); break;
    case IdGroupNew: new_group(); break;
    case IdGroupDuplicate: duplicate_group(); break;
    case IdGroupSave: save_group(); break;
    case IdGroupDelete: delete_group(); break;
    case IdLookNew: new_look(); break;
    case IdLookDuplicate: duplicate_look(); break;
    case IdLookSave: save_look(); break;
    case IdLookDelete: delete_look(); break;
    case IdLookPickRgb: pick_static_look_rgb(); break;
    case IdLookApplyColor: apply_static_look_color(); break;
    case IdLookSwatchRed: apply_static_look_swatch("red"); break;
    case IdLookSwatchGreen: apply_static_look_swatch("green"); break;
    case IdLookSwatchBlue: apply_static_look_swatch("blue"); break;
    case IdLookSwatchWhite: apply_static_look_swatch("white-emitter"); break;
    case IdLookSwatchAmber: apply_static_look_swatch("amber-emitter"); break;
    case IdLookSwatchUv: apply_static_look_swatch("uv-emitter"); break;
    case IdLookSwatchBlack: apply_static_look_swatch("black"); break;
    case IdLookApplyProperty: apply_static_look_property(); break;
    case IdLookRemoveProperty: remove_static_look_property(); break;
    case IdLookPreview: preview_static_look(); break;
    case IdAutoloopNew: new_autoloop(); break;
    case IdAutoloopDuplicate: duplicate_autoloop(); break;
    case IdAutoloopSave: save_autoloop(); break;
    case IdAutoloopDelete: delete_autoloop(); break;
    case IdAutoloopNextEmpty: move_autoloop_to_next_empty(); break;
    case IdAutoloopSwapTarget: swap_autoloop_into_target_slot(); break;
    case IdTrackNew: new_track(); break;
    case IdTrackDuplicate: duplicate_track(); break;
    case IdTrackSave: save_track(); break;
    case IdTrackDelete: delete_track(); break;
    case IdTrackAddAudio: import_audio_for_track(false); break;
    case IdTrackRelinkAudio: import_audio_for_track(true); break;
    case IdTrackVerifyAudio: verify_selected_audio_for_track(); break;
    case IdTrackResolveAudioFolder: resolve_audio_assets_for_project(); break;
    case IdRefreshMidi: refresh_midi_ports(); break;
    case IdCopyVirtualDjSetup: copy_virtualdj_setup(); break;
    case IdConnectionsApply: apply_connections(); break;
    case IdSafetyApply: apply_safety(); break;
    case IdMidiLearn: begin_midi_learn(); break;
    case IdMidiDelete: delete_midi_mapping(); break;
    case IdDiagnosticsCopy:
        if (copy_diagnostics_to_clipboard()) {
            set_status(L"Diagnostics copied to the clipboard.");
        }
        break;
    case IdDiagnosticsExport:
        if (save_diagnostics_report()) {
            set_status(L"Diagnostics report saved. Review it before sharing because it contains the project path.");
        }
        break;
    default:
        break;
    }
    if (before_edit.has_value()) {
        record_project_edit(*before_edit);
    }
}

bool Application::maybe_save_changes() {
    if (!dirty_) {
        return true;
    }
    const auto choice = ::MessageBoxW(
        window_,
        L"Save your changes to this EmberLights project?",
        L"Unsaved changes",
        MB_YESNOCANCEL | MB_ICONQUESTION);
    if (choice == IDCANCEL) {
        return false;
    }
    return choice != IDYES || save_project(false);
}

void Application::new_project() {
    if (!maybe_save_changes()) {
        return;
    }
    runner_.stop();
    ui_commands_.set_active_project(nullptr);
    active_project_.reset();
    project_ = emberlights::make_starter_project();
    current_path_.clear();
    edit_history_.clear();
    capture_saved_project();
    reset_authoring_selection();
    update_edit_menu();
    refresh_all();
    set_status(L"New project created. Add or import fixture profiles, then patch your rig.");
}

void Application::open_project_dialog() {
    if (!maybe_save_changes()) {
        return;
    }
    std::array<wchar_t, 32768> path{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window_;
    dialog.lpstrFilter = L"EmberLights Projects (*.emberlights)\0*.emberlights\0All Files\0*.*\0";
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    dialog.lpstrDefExt = L"emberlights";
    if (::GetOpenFileNameW(&dialog) != FALSE) {
        static_cast<void>(open_project(std::filesystem::path(path.data())));
    }
}

void Application::restore_project_history_dialog() {
    if (current_path_.empty()) {
        ::MessageBoxW(
            window_,
            L"Save this project before using restore points.",
            L"No saved versions yet",
            MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (!maybe_save_changes()) {
        return;
    }

    std::vector<emberlights::ProjectHistoryEntry> entries;
    const auto listed = emberlights::list_project_history(current_path_, entries);
    if (!listed) {
        const auto message = widen(listed.message);
        ::MessageBoxW(window_, message.c_str(), L"Could not read saved versions", MB_OK | MB_ICONERROR);
        return;
    }
    if (entries.empty()) {
        ::MessageBoxW(
            window_,
            L"EmberLights has not created a saved version for this project yet.",
            L"No saved versions yet",
            MB_OK | MB_ICONINFORMATION);
        return;
    }

    std::array<wchar_t, 32768> selected{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window_;
    dialog.lpstrFilter = L"EmberLights Saved Versions (*.emberlights)\0*.emberlights\0";
    dialog.lpstrFile = selected.data();
    dialog.nMaxFile = static_cast<DWORD>(selected.size());
    const auto history_directory = emberlights::project_history_directory(current_path_);
    const auto initial_directory = history_directory.wstring();
    dialog.lpstrInitialDir = initial_directory.c_str();
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_NOCHANGEDIR;
    dialog.lpstrDefExt = L"emberlights";
    if (::GetOpenFileNameW(&dialog) == FALSE) {
        return;
    }
    const std::filesystem::path selected_path(selected.data());
    const auto is_listed = std::any_of(
        entries.begin(), entries.end(), [&](const emberlights::ProjectHistoryEntry& entry) {
            std::error_code filesystem_error;
            return std::filesystem::equivalent(entry.path, selected_path, filesystem_error) &&
                !filesystem_error;
        });
    if (!is_listed) {
        ::MessageBoxW(
            window_,
            L"Select a version from this project's saved-version folder.",
            L"Invalid restore point",
            MB_OK | MB_ICONERROR);
        return;
    }
    emberlights::ProjectDocument preview;
    const auto preview_result = emberlights::load_project(selected_path, preview, false);
    if (!preview_result) {
        const auto message = widen(preview_result.message);
        ::MessageBoxW(
            window_, message.c_str(), L"Invalid restore point", MB_OK | MB_ICONERROR);
        return;
    }
    const auto prompt = L"Restore the saved version \"" + widen(preview.name) + L"\"?\n\n" +
        L"EmberLights will stop live output, retain the current project as a new saved version, "
        L"and replace the primary project file safely.";
    if (::MessageBoxW(
            window_, prompt.c_str(), L"Restore saved version",
            MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) {
        return;
    }

    runner_.stop();
    ui_commands_.set_active_project(nullptr);
    active_project_.reset();
    static_cast<void>(::ModifyMenuW(
        ::GetSubMenu(::GetMenu(window_), 2),
        IdShowStartStop,
        MF_BYCOMMAND | MF_STRING,
        IdShowStartStop,
        L"&Start Show"));
    emberlights::ProjectDocument restored;
    const auto result = emberlights::restore_project_history(current_path_, selected_path, restored);
    if (!result) {
        const auto message = widen(result.message);
        ::MessageBoxW(window_, message.c_str(), L"Could not restore saved version", MB_OK | MB_ICONERROR);
        refresh_live_lists();
        refresh_overrides();
        refresh_live_status();
        return;
    }
    project_ = std::move(restored);
    edit_history_.clear();
    capture_saved_project();
    reset_authoring_selection();
    update_edit_menu();
    refresh_all();
    set_status(widen(result.message));
}

void Application::import_qlc_fixture_dialog() {
    std::array<wchar_t, 32768> path{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window_;
    dialog.lpstrFilter =
        L"QLC+ Fixture Definitions (*.qxf)\0*.qxf\0XML Files (*.xml)\0*.xml\0All Files\0*.*\0";
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    dialog.lpstrDefExt = L"qxf";
    if (::GetOpenFileNameW(&dialog) == FALSE) {
        return;
    }

    const auto imported = emberlights::load_qlc_fixture(std::filesystem::path(path.data()));
    std::size_t added = 0U;
    std::size_t duplicates = 0U;
    std::size_t rejected = 0U;
    std::int32_t first_added = -1;
    auto candidate = project_;
    for (const auto& profile : imported.profiles) {
        const auto duplicate = std::find_if(
            candidate.fixture_profiles.begin(), candidate.fixture_profiles.end(),
            [&](const auto& existing) { return existing.id == profile.id; });
        if (duplicate != candidate.fixture_profiles.end()) {
            ++duplicates;
            continue;
        }
        candidate.fixture_profiles.push_back(profile);
        const auto validation = emberlights::validate_project(candidate);
        if (!validation.ok()) {
            candidate.fixture_profiles.pop_back();
            ++rejected;
            continue;
        }
        if (first_added < 0) {
            first_added = static_cast<std::int32_t>(candidate.fixture_profiles.size() - 1U);
        }
        ++added;
    }

    if (added > 0U) {
        project_ = std::move(candidate);
        mark_dirty();
        refresh_profiles();
        refresh_patch();
        const auto list = ::GetDlgItem(
            pages_[static_cast<std::size_t>(Page::Profiles)], IdProfileList);
        static_cast<void>(::SendMessageW(list, LB_SETCURSEL, first_added, 0));
        select_profile(first_added);
    }

    std::ostringstream report;
    report << "QLC+ fixture import\r\n\r\n"
           << "Converted modes: " << imported.profiles.size() << "\r\n"
           << "Added to this project: " << added << "\r\n"
           << "Already present: " << duplicates << "\r\n"
           << "Rejected by project capacity/validation: " << rejected << "\r\n"
           << "Importer warnings: " << imported.warning_count() << "\r\n"
           << "Quarantined/errors: " << imported.error_count() << "\r\n";
    const auto shown = std::min<std::size_t>(imported.issues.size(), 8U);
    if (shown > 0U) {
        report << "\r\nReview:\r\n";
        for (std::size_t index = 0; index < shown; ++index) {
            const auto& issue = imported.issues[index];
            report << (issue.severity == emberlights::QlcImportIssueSeverity::Error
                           ? "ERROR" : "WARNING")
                   << " [" << issue.code << "] " << issue.subject << ": "
                   << issue.message << "\r\n";
        }
        if (shown < imported.issues.size()) {
            report << "...and " << imported.issues.size() - shown << " more message(s).\r\n";
        }
    }
    if (added > 0U) {
        report << "\r\nImported profiles are read-only. Duplicate one to customize it, "
                  "and verify every approximation against the fixture's official DMX chart.";
    }
    const auto report_wide = widen(report.str());
    ::MessageBoxW(
        window_, report_wide.c_str(), L"QLC+ fixture import",
        MB_OK | ((imported.error_count() > 0U || added == 0U)
                     ? MB_ICONWARNING : MB_ICONINFORMATION));
    if (added > 0U) {
        set_status(runner_.status().state == emberlights::RunnerState::Running
            ? L"QLC+ profiles imported. Save the project to preflight and atomically activate compatible changes."
            : L"QLC+ profiles imported. Patch fixtures, then validate and start the show.");
    }
}

void Application::inspect_soundswitch_dialog() {
    const auto source = choose_folder(
        window_,
        L"Select a SoundSwitch project or extracted application-data directory");
    if (!source.has_value()) {
        return;
    }
    set_status(L"Inspecting SoundSwitch source read-only and hashing every payload...");
    static_cast<void>(::UpdateWindow(window_));
    const auto inspection = emberlights::inspect_soundswitch_project(*source);

    std::array<wchar_t, 32768> selected{};
    constexpr std::wstring_view suggested = L"EmberLights-SoundSwitch-inspection.json";
    std::copy(suggested.begin(), suggested.end(), selected.begin());
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window_;
    dialog.lpstrFilter = L"JSON Reports (*.json)\0*.json\0All Files\0*.*\0";
    dialog.lpstrFile = selected.data();
    dialog.nMaxFile = static_cast<DWORD>(selected.size());
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    dialog.lpstrDefExt = L"json";
    bool saved = false;
    std::string report_error;
    if (::GetSaveFileNameW(&dialog) != FALSE) {
        saved = emberlights::save_soundswitch_inspection_atomic(
            std::filesystem::path(selected.data()), inspection, report_error);
    }

    std::wostringstream summary;
    summary << L"Read-only SoundSwitch inspection\n\n"
            << L"Files: " << inspection.artifacts.size() << L"\n"
            << L"Known payloads: " << inspection.known_artifacts << L"\n"
            << L"Unknown payloads preserved by inventory: "
            << inspection.unknown_artifacts << L"\n"
            << L"Recognized .ssfile headers: " << inspection.recognized_ssfiles << L"\n"
            << L"Errors: " << inspection.error_count()
            << L"    Warnings: " << inspection.warning_count() << L"\n\n";
    if (!report_error.empty()) {
        summary << widen(report_error);
    } else if (saved) {
        summary << L"The JSON evidence report was saved. No source file was changed.";
    } else {
        summary << L"No report was saved. No source file was changed.";
    }
    ::MessageBoxW(
        window_, summary.str().c_str(), L"SoundSwitch inspection",
        MB_OK | (inspection.complete() ? MB_ICONINFORMATION : MB_ICONWARNING));
    set_status(inspection.complete()
        ? L"SoundSwitch source inventory complete. This preserves evidence but does not claim semantic conversion."
        : L"SoundSwitch inspection found blocking issues; review the JSON report.");
}

void Application::compare_soundswitch_dialog() {
    const auto before = choose_folder(
        window_, L"Select the original SoundSwitch export (before one controlled change)");
    if (!before.has_value()) {
        return;
    }
    const auto after = choose_folder(
        window_, L"Select the changed SoundSwitch export (after one controlled change)");
    if (!after.has_value()) {
        return;
    }

    set_status(L"Comparing SoundSwitch exports read-only and locating changed binary ranges...");
    static_cast<void>(::UpdateWindow(window_));
    const auto comparison = emberlights::compare_soundswitch_projects(*before, *after);

    std::array<wchar_t, 32768> selected{};
    constexpr std::wstring_view suggested = L"EmberLights-SoundSwitch-comparison.json";
    std::copy(suggested.begin(), suggested.end(), selected.begin());
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window_;
    dialog.lpstrFilter = L"JSON Reports (*.json)\0*.json\0All Files\0*.*\0";
    dialog.lpstrFile = selected.data();
    dialog.nMaxFile = static_cast<DWORD>(selected.size());
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    dialog.lpstrDefExt = L"json";
    bool saved = false;
    std::string report_error;
    if (::GetSaveFileNameW(&dialog) != FALSE) {
        saved = emberlights::save_soundswitch_comparison_atomic(
            std::filesystem::path(selected.data()), comparison, report_error);
    }

    std::wostringstream summary;
    summary << L"Read-only SoundSwitch export comparison\n\n"
            << L"Compared payload paths: " << comparison.artifacts.size() << L"\n"
            << L"Modified: " << comparison.modified_artifacts << L"\n"
            << L"Added: " << comparison.added_artifacts << L"\n"
            << L"Removed: " << comparison.removed_artifacts << L"\n"
            << L"Unchanged: " << comparison.unchanged_artifacts << L"\n"
            << L"Errors: " << comparison.error_count()
            << L"    Warnings: " << comparison.warning_count() << L"\n\n";
    if (!report_error.empty()) {
        summary << widen(report_error);
    } else if (saved) {
        summary << L"The JSON comparison was saved. It contains hashes and byte ranges, not source payload bytes.";
    } else {
        summary << L"No report was saved. Neither SoundSwitch export was changed.";
    }
    ::MessageBoxW(
        window_, summary.str().c_str(), L"SoundSwitch export comparison",
        MB_OK | (comparison.complete() ? MB_ICONINFORMATION : MB_ICONWARNING));
    set_status(comparison.complete()
        ? L"SoundSwitch comparison complete. The report is ready for safe decoder mapping."
        : L"SoundSwitch comparison found blocking source or read errors; review the report.");
}

void Application::bundle_soundswitch_dialog() {
    const auto source = choose_folder(
        window_,
        L"Select a SoundSwitch project or extracted application-data directory");
    if (!source.has_value()) {
        return;
    }
    const auto parent = choose_folder(
        window_, L"Select where the verified EmberLights migration bundle should be created");
    if (!parent.has_value()) {
        return;
    }
    auto name = source->filename().wstring();
    if (name.empty()) {
        name = L"SoundSwitch-project";
    }
    const auto destination = *parent / (name + L"-EmberLights-migration");
    set_status(L"Copying and SHA-256 verifying the SoundSwitch migration bundle...");
    static_cast<void>(::UpdateWindow(window_));
    const auto result = emberlights::create_soundswitch_source_bundle(
        *source, destination);
    if (!result) {
        const auto message = widen(result.message);
        ::MessageBoxW(
            window_, message.c_str(), L"Migration bundle not created",
            MB_OK | MB_ICONERROR);
        set_status(L"SoundSwitch bundle failed safely; the source was not changed.");
        return;
    }
    const auto message = L"Verified migration bundle created at:\n\n" +
        result.destination.wstring() +
        L"\n\nEvery regular source payload was copied and matched its SHA-256 inventory. "
        L"The SoundSwitch source was not changed.";
    ::MessageBoxW(
        window_, message.c_str(), L"SoundSwitch bundle complete",
        MB_OK | MB_ICONINFORMATION);
    set_status(L"Verified SoundSwitch migration bundle created.");
}

bool Application::open_project(const std::filesystem::path& path) {
    emberlights::ProjectDocument loaded;
    const auto result = emberlights::load_project(path, loaded, true);
    if (!result) {
        const auto message = widen(result.message);
        ::MessageBoxW(window_, message.c_str(), L"Could not open project", MB_OK | MB_ICONERROR);
        return false;
    }
    runner_.stop();
    ui_commands_.set_active_project(nullptr);
    active_project_.reset();
    project_ = std::move(loaded);
    current_path_ = path;
    const auto remembered = remember_project_path(current_path_);
    edit_history_.clear();
    capture_saved_project();
    recovery_save_required_ = result.recovered_from_backup;
    mark_dirty();
    reset_authoring_selection();
    update_edit_menu();
    refresh_all();
    if (result.recovered_from_backup) {
        ::MessageBoxW(
            window_,
            L"The primary file was damaged or incomplete. EmberLights loaded its last-known-good "
            L"backup. Save the project to activate the recovered copy.",
            L"Project recovered",
            MB_OK | MB_ICONWARNING);
    } else {
        set_status(remembered
            ? L"Project opened successfully and will reopen automatically next time."
            : L"Project opened, but Windows did not allow EmberLights to remember it for next launch.");
    }
    return true;
}

bool Application::save_project(bool save_as) {
    auto path = current_path_;
    if (save_as || path.empty()) {
        std::array<wchar_t, 32768> selected{};
        const auto suggested = widen(slugify(project_.name) + std::string(emberlights::kProjectExtension));
        std::copy_n(
            suggested.c_str(),
            std::min(suggested.size(), selected.size() - 1U),
            selected.begin());
        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = window_;
        dialog.lpstrFilter = L"EmberLights Projects (*.emberlights)\0*.emberlights\0All Files\0*.*\0";
        dialog.lpstrFile = selected.data();
        dialog.nMaxFile = static_cast<DWORD>(selected.size());
        dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_EXPLORER;
        dialog.lpstrDefExt = L"emberlights";
        if (::GetSaveFileNameW(&dialog) == FALSE) {
            return false;
        }
        path = std::filesystem::path(selected.data());
    }

    const auto result = emberlights::save_project_atomic(path, project_);
    if (!result) {
        const auto message = widen(result.message);
        ::MessageBoxW(window_, message.c_str(), L"Could not save project", MB_OK | MB_ICONERROR);
        return false;
    }
    current_path_ = std::move(path);
    const auto remembered = remember_project_path(current_path_);
    capture_saved_project();
    const auto runner_state = runner_.status().state;
    if (runner_state == emberlights::RunnerState::Running) {
        auto compilation = emberlights::compile_project(project_);
        if (!compilation) {
            set_status(
                L"Project saved, but preflight failed. The last activated show remains live.");
            refresh_diagnostics();
            return true;
        }
        const auto activation = runner_.activate(std::move(compilation.show), project_);
        if (activation) {
            active_project_ = project_;
            ui_commands_.set_active_project(&*active_project_);
            const auto snapshot = emberlights::save_project_atomic(
                emberlights::project_active_path(current_path_), project_, false);
            refresh_live_lists();
            refresh_overrides();
            refresh_live_status();
            if (snapshot) {
                set_status(
                    L"Project saved and atomically activated. The prior show stayed live until handoff.");
            } else {
                set_status(
                    L"Project activated, but its last-known-good activation snapshot could not be saved.");
            }
            return true;
        }
        if (activation.error == emberlights::RunnerActivationError::RestartRequired) {
            set_status(
                L"Project saved. Connection or identity changes require Stop Show, then Start Show; the prior show remains live.");
            return true;
        }

        runner_.stop();
        ui_commands_.set_active_project(nullptr);
        active_project_.reset();
        refresh_live_lists();
        refresh_overrides();
        refresh_live_status();
        ::MessageBoxW(
            window_,
            L"The live package handoff did not complete safely. EmberLights stopped output and sent "
            L"zero frames. Start the show again after reviewing Diagnostics.",
            L"Activation stopped safely",
            MB_OK | MB_ICONERROR);
        set_status(L"Activation failed closed. Runner stopped; the project file was saved.");
        return true;
    }
    if (!remembered) {
        set_status(
            L"Project saved, but Windows did not allow EmberLights to remember it for next launch.");
    } else {
        set_status(result.message.empty()
            ? L"Project saved with checksum, recovery backup, and saved-version protection."
            : widen(result.message));
    }
    return true;
}

void Application::validate_project(bool show_success) {
    const auto validation = emberlights::validate_project(project_);
    refresh_diagnostics();
    if (validation.ok()) {
        if (show_success) {
            ::MessageBoxW(
                window_,
                L"Project validation passed. Fixture profiles, patch, Static Looks, Autoloops, "
                L"and current limits are internally consistent.",
                L"Preflight passed",
                MB_OK | MB_ICONINFORMATION);
        }
        set_status(L"Project validation passed.");
        return;
    }
    std::wostringstream message;
    message << L"Preflight found " << validation.error_count() << L" error(s).\n\n";
    const auto limit = std::min<std::size_t>(validation.issues.size(), 8U);
    for (std::size_t index = 0; index < limit; ++index) {
        const auto& issue = validation.issues[index];
        if (issue.severity == emberlights::ProjectIssueSeverity::Error) {
            message << L"• " << widen(issue.subject) << L": " << widen(issue.message) << L"\n";
        }
    }
    if (validation.issues.size() > limit) {
        message << L"\nOpen Diagnostics for the complete report.";
    }
    ::MessageBoxW(window_, message.str().c_str(), L"Preflight failed", MB_OK | MB_ICONERROR);
    set_status(L"Project validation failed. Open Diagnostics for details.");
}

void Application::start_or_stop_show() {
    const auto current = runner_.status().state;
    if (current != emberlights::RunnerState::Stopped) {
        runner_.stop();
        ui_commands_.set_active_project(nullptr);
        active_project_.reset();
        static_cast<void>(::ModifyMenuW(
            ::GetSubMenu(::GetMenu(window_), 2),
            IdShowStartStop,
            MF_BYCOMMAND | MF_STRING,
            IdShowStartStop,
            L"&Start Show"));
        refresh_live_status();
        refresh_live_lists();
        refresh_overrides();
        set_status(L"Show stopped. EmberLights sent explicit zero frames to active outputs.");
        return;
    }
    if (current_path_.empty() && !save_project(false)) {
        set_status(L"Save the project before starting so EmberLights can maintain a last-known-good show.");
        return;
    }

    auto run_project = project_;
    auto compilation = emberlights::compile_project(run_project);
    bool recovered_activation = false;
    if (!compilation) {
        emberlights::ProjectDocument last_active;
        const auto loaded = emberlights::load_project(
            emberlights::project_active_path(current_path_), last_active, true);
        auto last_active_compilation = loaded
            ? emberlights::compile_project(last_active)
            : emberlights::CompilationResult{};
        if (!loaded || !last_active_compilation) {
            validate_project(false);
            return;
        }
        const auto choice = ::MessageBoxW(
            window_,
            L"The current project does not pass preflight. Start the last successfully activated "
            L"snapshot instead? Your current project file will not be changed.",
            L"Start last-known-good show",
            MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
        if (choice != IDYES) {
            validate_project(false);
            return;
        }
        run_project = std::move(last_active);
        compilation = std::move(last_active_compilation);
        recovered_activation = true;
    }
    if (!runner_.start(std::move(compilation.show), run_project)) {
        ::MessageBoxW(
            window_,
            L"The Runner could not start. Stop other EmberLights instances and review Connections "
            L"and Diagnostics.",
            L"Runner start failed",
            MB_OK | MB_ICONERROR);
        refresh_live_lists();
        refresh_overrides();
        refresh_live_status();
        return;
    }
    active_project_ = run_project;
    ui_commands_.set_active_project(&*active_project_);
    if (!recovered_activation) {
        const auto snapshot = emberlights::save_project_atomic(
            emberlights::project_active_path(current_path_), run_project, false);
        if (!snapshot) {
            ::MessageBoxW(
                window_,
                L"The show started, but EmberLights could not persist its last-known-good activation snapshot.",
                L"Recovery snapshot warning",
                MB_OK | MB_ICONWARNING);
        }
    }
    static_cast<void>(::ModifyMenuW(
        ::GetSubMenu(::GetMenu(window_), 2),
        IdShowStartStop,
        MF_BYCOMMAND | MF_STRING,
        IdShowStartStop,
        L"&Stop Show"));
    show_page(Page::Live);
    refresh_live_lists();
    refresh_overrides();
    set_status(recovered_activation
        ? L"Runner starting from the last-known-good snapshot; the current project remains unchanged."
        : L"Runner starting. DMX output follows the enabled Connections settings.");
}

emberlights::UiInvocationResult Application::ui_start_show() noexcept {
    if (runner_.status().state != emberlights::RunnerState::Stopped) {
        return emberlights::UiInvocationResult::NoChange;
    }
    start_or_stop_show();
    return runner_.status().state == emberlights::RunnerState::Stopped
        ? emberlights::UiInvocationResult::InternalError
        : emberlights::UiInvocationResult::Accepted;
}

emberlights::UiInvocationResult Application::ui_stop_show() noexcept {
    if (runner_.status().state == emberlights::RunnerState::Stopped) {
        return emberlights::UiInvocationResult::NoChange;
    }
    start_or_stop_show();
    return runner_.status().state == emberlights::RunnerState::Stopped
        ? emberlights::UiInvocationResult::Accepted
        : emberlights::UiInvocationResult::InternalError;
}

void Application::apply_fixture_override(bool active) {
    const auto page = pages_[static_cast<std::size_t>(Page::Overrides)];
    const auto fixtures = ::GetDlgItem(page, IdOverridesFixture);
    const auto selected = static_cast<int>(::SendMessageW(fixtures, LB_GETCURSEL, 0, 0));
    if (selected < 0) {
        set_page_message(Page::Overrides, IdOverridesMessage,
                         "Select an active fixture before changing an override.", true);
        return;
    }
    const auto target_index = static_cast<std::size_t>(::SendMessageW(
        fixtures, LB_GETITEMDATA, selected, 0));
    if (target_index >= live_view_model_.override_targets().size()) {
        set_page_message(Page::Overrides, IdOverridesMessage,
                         "The selected target is not in the active Live view.", true);
        return;
    }
    const auto& target = live_view_model_.override_targets()[target_index];
    const auto property = static_cast<showcore::Property>(combo_selected_data(
        ::GetDlgItem(page, IdOverridesProperty),
        static_cast<std::intptr_t>(showcore::Property::Intensity)));
    float percentage = 0.0F;
    if (active && (!parse_number(
                       control_text(::GetDlgItem(page, IdOverridesValue)), percentage) ||
                   percentage < 0.0F || percentage > 100.0F)) {
        set_page_message(Page::Overrides, IdOverridesMessage,
                         "Enter an override value from 0 through 100.", true);
        return;
    }
    const auto& target_name = target.name;
    emberlights::UiCommandInvocation invocation;
    invocation.property = property;
    invocation.number_value = percentage / 100.0F;
    invocation.command = target.kind == emberlights::LiveOverrideTargetKind::Fixture
        ? (active ? emberlights::UiCommandId::FixtureOverridePropertySet
                  : emberlights::UiCommandId::FixtureOverridePropertyRelease)
        : (active ? emberlights::UiCommandId::GroupOverridePropertySet
                  : emberlights::UiCommandId::GroupOverridePropertyRelease);
    invocation.target_id = target.id;
    const auto result = ui_commands_.invoke(invocation);
    if (result != emberlights::UiInvocationResult::Accepted &&
        result != emberlights::UiInvocationResult::NoChange) {
        std::ostringstream error;
        error << "Override rejected: "
              << emberlights::ui_invocation_result_name(result)
              << ". The target may not support this property, the show may be stopped, "
                 "or a safety gate may be active.";
        set_page_message(Page::Overrides, IdOverridesMessage, error.str(), true);
        return;
    }
    std::ostringstream message;
    message << (active ? "Override queued: " : "Property release queued: ")
            << target_name << " — " << emberlights::property_name(property);
    if (active) {
        message << " at " << percentage << "%";
    }
    message << ". Runner safety limits still apply.";
    set_page_message(Page::Overrides, IdOverridesMessage, message.str());
}

void Application::clear_fixture_overrides() {
    if (ui_commands_.invoke({emberlights::UiCommandId::ReleaseAllOverrides}) !=
        emberlights::UiInvocationResult::Accepted) {
        set_page_message(Page::Overrides, IdOverridesMessage,
                         "Start the show before releasing manual overrides.", true);
        return;
    }
    set_page_message(Page::Overrides, IdOverridesMessage,
                     "All transient manual overrides are queued for release.");
}

const emberlights::ProjectDocument& Application::live_project() const noexcept {
    return active_project_.has_value() ? *active_project_ : project_;
}

std::string Application::unique_id(std::string_view prefix, std::string_view name) const {
    const auto base = std::string(prefix) + "." + slugify(name);
    auto exists = [&](std::string_view candidate) {
        const auto profile = std::any_of(
            project_.fixture_profiles.begin(), project_.fixture_profiles.end(),
            [&](const auto& value) { return value.id == candidate; });
        const auto fixture = std::any_of(
            project_.fixtures.begin(), project_.fixtures.end(),
            [&](const auto& value) { return value.id == candidate; });
        const auto look = std::any_of(
            project_.looks.begin(), project_.looks.end(),
            [&](const auto& value) { return value.id == candidate; });
        const auto loop = std::any_of(
            project_.autoloops.begin(), project_.autoloops.end(),
            [&](const auto& value) { return value.id == candidate; });
        const auto audio = std::any_of(
            project_.audio_assets.begin(), project_.audio_assets.end(),
            [&](const auto& value) { return value.id == candidate; });
        return profile || fixture || look || loop || audio;
    };
    if (!exists(base)) {
        return base;
    }
    for (std::uint32_t suffix = 2; suffix < 100000U; ++suffix) {
        const auto candidate = base + "-" + number_text(suffix);
        if (!exists(candidate)) {
            return candidate;
        }
    }
    return base + "-new";
}

bool Application::copy_text_to_clipboard(std::wstring_view text) {
    if (::OpenClipboard(window_) == FALSE) {
        return false;
    }
    static_cast<void>(::EmptyClipboard());
    const auto bytes = (text.size() + 1U) * sizeof(wchar_t);
    const auto memory = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory == nullptr) {
        ::CloseClipboard();
        return false;
    }
    auto* destination = static_cast<wchar_t*>(::GlobalLock(memory));
    if (destination == nullptr) {
        ::GlobalFree(memory);
        ::CloseClipboard();
        return false;
    }
    std::copy(text.begin(), text.end(), destination);
    destination[text.size()] = L'\0';
    ::GlobalUnlock(memory);
    if (::SetClipboardData(CF_UNICODETEXT, memory) == nullptr) {
        ::GlobalFree(memory);
        ::CloseClipboard();
        return false;
    }
    ::CloseClipboard();
    return true;
}

bool Application::copy_diagnostics_to_clipboard() {
    return copy_text_to_clipboard(widen(diagnostics_text()));
}

void Application::copy_virtualdj_setup() {
    const auto& settings = project_.connections;
    const auto address = settings.os2l_bind == "0.0.0.0"
        ? std::string{"127.0.0.1"}
        : settings.os2l_bind;
    std::wostringstream setup;
    setup << L"EmberLights / VirtualDJ OS2L setup\r\n\r\n"
          << L"Recommended automatic discovery:\r\n"
          << L"os2l=Auto\r\n"
          << L"os2lDirectIp=<blank>\r\n\r\n"
          << L"Direct-IP fallback:\r\n"
          << L"os2l=Yes\r\n"
          << L"os2lDirectIp=" << widen(address) << L":" << settings.os2l_port
          << L"\r\n"
          << L"Keyboard mapper ONINIT:\r\n"
          << L"wait 100ms & os2l_button 'EmberLights Keepalive' off\r\n";
    if (copy_text_to_clipboard(setup.str())) {
        set_status(L"VirtualDJ automatic-discovery and safe direct-IP fallback setup copied.");
        set_page_message(
            Page::Connections,
            IdConnectionsMessage,
            "VirtualDJ setup copied. Try automatic discovery first; the direct-IP ONINIT fallback is a reserved no-op and cannot change blackout or a live look.");
    } else {
        set_page_message(
            Page::Connections,
            IdConnectionsMessage,
            "Windows did not allow EmberLights to copy the VirtualDJ setup.",
            true);
    }
}

bool Application::save_diagnostics_report() {
    std::array<wchar_t, 32768> path{};
    constexpr std::wstring_view suggested = L"EmberLights-Diagnostics.txt";
    std::copy(suggested.begin(), suggested.end(), path.begin());
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window_;
    dialog.lpstrFilter = L"Text Reports (*.txt)\0*.txt\0All Files\0*.*\0";
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    dialog.lpstrDefExt = L"txt";
    if (::GetSaveFileNameW(&dialog) == FALSE) {
        return false;
    }
    std::ofstream output(
        std::filesystem::path(path.data()),
        std::ios::binary | std::ios::trunc);
    if (!output) {
        ::MessageBoxW(
            window_, L"The diagnostics report could not be opened for writing.",
            L"Could not save diagnostics", MB_OK | MB_ICONERROR);
        return false;
    }
    output << diagnostics_text();
    if (!output.good()) {
        ::MessageBoxW(
            window_, L"The diagnostics report could not be written completely.",
            L"Could not save diagnostics", MB_OK | MB_ICONERROR);
        return false;
    }
    return true;
}

namespace {

[[nodiscard]] std::string first_validation_error(
    const emberlights::ProjectValidation& validation) {
    const auto found = std::find_if(
        validation.issues.begin(),
        validation.issues.end(),
        [](const auto& issue) {
            return issue.severity == emberlights::ProjectIssueSeverity::Error;
        });
    return found == validation.issues.end()
        ? std::string("The edited record is not valid.")
        : found->subject + ": " + found->message;
}

[[nodiscard]] bool parse_channel_rows(
    std::string_view text,
    std::uint16_t footprint,
    std::vector<emberlights::ChannelDefinition>& channels,
    std::string& error_message) {
    channels.clear();
    std::size_t row = 0;
    for (const auto& line : lines(text)) {
        ++row;
        const auto fields = split_csv(line);
        if (fields.size() != 7U) {
            error_message = "Channel row " + number_text(row) + " needs exactly 7 comma-separated fields.";
            return false;
        }
        std::uint16_t coarse = 0;
        std::int16_t fine = 0;
        std::uint16_t dmx_min = 0;
        std::uint16_t dmx_max = 0;
        std::uint16_t default_value = 0;
        showcore::Property property{};
        showcore::ChannelEncoding encoding{};
        if (!parse_number(fields[0], coarse) || coarse == 0U || coarse > footprint ||
            !emberlights::parse_property(fields[1], property) ||
            !emberlights::parse_channel_encoding(fields[2], encoding) ||
            !parse_number(fields[3], fine) || fine < 0 || fine > footprint ||
            !parse_number(fields[4], dmx_min) || dmx_min > 255U ||
            !parse_number(fields[5], dmx_max) || dmx_max > 255U ||
            !parse_number(fields[6], default_value)) {
            error_message = "Channel row " + number_text(row) + " contains an invalid value.";
            return false;
        }
        channels.push_back({
            property,
            static_cast<std::uint16_t>(coarse - 1U),
            fine == 0 ? static_cast<std::int16_t>(-1)
                      : static_cast<std::int16_t>(fine - 1),
            encoding,
            static_cast<std::uint8_t>(dmx_min),
            static_cast<std::uint8_t>(dmx_max),
            default_value});
    }
    if (channels.empty()) {
        error_message = "Add at least one DMX channel mapping.";
        return false;
    }
    return true;
}

}  // namespace

void Application::select_profile(std::int32_t index) {
    if (index < 0 || static_cast<std::size_t>(index) >= project_.fixture_profiles.size()) {
        new_profile();
        return;
    }
    profile_index_ = index;
    const auto page = pages_[static_cast<std::size_t>(Page::Profiles)];
    const auto& profile = project_.fixture_profiles[static_cast<std::size_t>(index)];
    set_control_text(::GetDlgItem(page, IdProfileManufacturer), profile.manufacturer);
    set_control_text(::GetDlgItem(page, IdProfileModel), profile.model);
    set_control_text(::GetDlgItem(page, IdProfileMode), profile.mode);
    set_control_text(::GetDlgItem(page, IdProfileName), profile.name);
    set_control_text(::GetDlgItem(page, IdProfileFootprint), number_text(profile.footprint));
    set_control_text(::GetDlgItem(page, IdProfileChannels), profile_channels_text(profile));
    const bool editable = profile.source == showcore::FixtureProfileSource::Local;
    for (const auto id : {
             IdProfileManufacturer, IdProfileModel, IdProfileMode, IdProfileName,
             IdProfileFootprint, IdProfileChannels, IdProfileSave}) {
        ::EnableWindow(::GetDlgItem(page, id), editable ? TRUE : FALSE);
    }
    ::EnableWindow(
        ::GetDlgItem(page, IdProfileDelete),
        profile.source == showcore::FixtureProfileSource::BuiltIn ? FALSE : TRUE);
    ::EnableWindow(::GetDlgItem(page, IdProfileDuplicate), TRUE);
    set_page_message(
        Page::Profiles,
        IdProfileMessage,
        editable ? "Editing a local fixture profile."
                 : (profile.source == showcore::FixtureProfileSource::BuiltIn
                        ? "Built-in profiles are read-only. Duplicate one to customize it."
                        : "Imported profiles are read-only. Duplicate one to customize it; verify importer warnings against the official DMX chart."));
}

void Application::new_profile() {
    profile_index_ = -1;
    const auto page = pages_[static_cast<std::size_t>(Page::Profiles)];
    set_control_text(::GetDlgItem(page, IdProfileManufacturer), "");
    set_control_text(::GetDlgItem(page, IdProfileModel), "");
    set_control_text(::GetDlgItem(page, IdProfileMode), "");
    set_control_text(::GetDlgItem(page, IdProfileName), "");
    set_control_text(::GetDlgItem(page, IdProfileFootprint), "");
    set_control_text(::GetDlgItem(page, IdProfileChannels),
                     "1,intensity,linear8,0,0,255,0\r\n"
                     "2,red,linear8,0,0,255,0\r\n"
                     "3,green,linear8,0,0,255,0\r\n"
                     "4,blue,linear8,0,0,255,0\r\n");
    for (const auto id : {
             IdProfileManufacturer, IdProfileModel, IdProfileMode, IdProfileName,
             IdProfileFootprint, IdProfileChannels, IdProfileSave}) {
        ::EnableWindow(::GetDlgItem(page, id), TRUE);
    }
    ::EnableWindow(::GetDlgItem(page, IdProfileDelete), FALSE);
    set_page_message(Page::Profiles, IdProfileMessage,
                     "Create a local profile from the fixture's official DMX chart.");
}

void Application::duplicate_profile() {
    if (profile_index_ < 0 ||
        static_cast<std::size_t>(profile_index_) >= project_.fixture_profiles.size()) {
        return;
    }
    const auto source = project_.fixture_profiles[static_cast<std::size_t>(profile_index_)];
    new_profile();
    const auto page = pages_[static_cast<std::size_t>(Page::Profiles)];
    set_control_text(::GetDlgItem(page, IdProfileManufacturer), source.manufacturer);
    set_control_text(::GetDlgItem(page, IdProfileModel), source.model);
    set_control_text(::GetDlgItem(page, IdProfileMode), source.mode + " Custom");
    set_control_text(::GetDlgItem(page, IdProfileName), source.name + " Custom");
    set_control_text(::GetDlgItem(page, IdProfileFootprint), number_text(source.footprint));
    set_control_text(::GetDlgItem(page, IdProfileChannels), profile_channels_text(source));
    set_page_message(Page::Profiles, IdProfileMessage,
                     "Duplicated into a new local profile. Review and save it.");
}

void Application::save_profile() {
    const auto page = pages_[static_cast<std::size_t>(Page::Profiles)];
    if (profile_index_ >= 0 &&
        project_.fixture_profiles[static_cast<std::size_t>(profile_index_)].source !=
            showcore::FixtureProfileSource::Local) {
        set_page_message(Page::Profiles, IdProfileMessage,
                         "Built-in profiles cannot be overwritten. Duplicate this profile first.", true);
        return;
    }
    emberlights::FixtureProfileDefinition profile;
    profile.manufacturer = trim(control_text(::GetDlgItem(page, IdProfileManufacturer)));
    profile.model = trim(control_text(::GetDlgItem(page, IdProfileModel)));
    profile.mode = trim(control_text(::GetDlgItem(page, IdProfileMode)));
    profile.name = trim(control_text(::GetDlgItem(page, IdProfileName)));
    if (profile.name.empty()) {
        profile.name = profile.manufacturer + " " + profile.model + " (" + profile.mode + ")";
    }
    if (profile.manufacturer.empty() || profile.model.empty() || profile.mode.empty() ||
        !parse_number(control_text(::GetDlgItem(page, IdProfileFootprint)), profile.footprint) ||
        profile.footprint == 0U || profile.footprint > showcore::kUniverseSlots) {
        set_page_message(Page::Profiles, IdProfileMessage,
                         "Manufacturer, model, mode, and a footprint from 1 through 512 are required.", true);
        return;
    }
    std::string parse_error;
    if (!parse_channel_rows(
            control_text(::GetDlgItem(page, IdProfileChannels)),
            profile.footprint,
            profile.channels,
            parse_error)) {
        set_page_message(Page::Profiles, IdProfileMessage, parse_error, true);
        return;
    }
    profile.source = showcore::FixtureProfileSource::Local;
    profile.source_revision = "emberlights-local-v1";
    profile.id = profile_index_ >= 0
        ? project_.fixture_profiles[static_cast<std::size_t>(profile_index_)].id
        : unique_id("profile", profile.manufacturer + " " + profile.model + " " + profile.mode);
    auto candidate = project_;
    if (profile_index_ >= 0) {
        candidate.fixture_profiles[static_cast<std::size_t>(profile_index_)] = profile;
    } else {
        candidate.fixture_profiles.push_back(profile);
    }
    const auto validation = emberlights::validate_project(candidate);
    if (!validation.ok()) {
        set_page_message(Page::Profiles, IdProfileMessage, first_validation_error(validation), true);
        return;
    }
    project_ = std::move(candidate);
    if (profile_index_ < 0) {
        profile_index_ = static_cast<std::int32_t>(project_.fixture_profiles.size() - 1U);
    }
    mark_dirty();
    refresh_profiles();
    refresh_patch();
    set_page_message(Page::Profiles, IdProfileMessage, "Fixture profile saved.");
}

void Application::delete_profile() {
    if (profile_index_ < 0 ||
        static_cast<std::size_t>(profile_index_) >= project_.fixture_profiles.size()) {
        return;
    }
    const auto& profile = project_.fixture_profiles[static_cast<std::size_t>(profile_index_)];
    if (profile.source == showcore::FixtureProfileSource::BuiltIn) {
        set_page_message(Page::Profiles, IdProfileMessage,
                         "Built-in profiles cannot be deleted.", true);
        return;
    }
    if (std::any_of(
            project_.fixtures.begin(), project_.fixtures.end(),
            [&](const auto& fixture) { return fixture.profile_id == profile.id; })) {
        set_page_message(Page::Profiles, IdProfileMessage,
                         "This profile is used by the patch. Reassign those fixtures first.", true);
        return;
    }
    if (::MessageBoxW(window_, L"Delete this fixture profile from the project?", L"Delete profile",
                      MB_YESNO | MB_ICONWARNING) != IDYES) {
        return;
    }
    project_.fixture_profiles.erase(project_.fixture_profiles.begin() + profile_index_);
    profile_index_ = -1;
    mark_dirty();
    refresh_profiles();
    refresh_patch();
}

void Application::select_fixture(std::int32_t index) {
    if (index < 0 || static_cast<std::size_t>(index) >= project_.fixtures.size()) {
        new_fixture();
        return;
    }
    fixture_index_ = index;
    const auto page = pages_[static_cast<std::size_t>(Page::Patch)];
    const auto& fixture = project_.fixtures[static_cast<std::size_t>(index)];
    set_control_text(::GetDlgItem(page, IdPatchName), fixture.name);
    const auto profile = std::find_if(
        project_.fixture_profiles.begin(), project_.fixture_profiles.end(),
        [&](const auto& candidate) { return candidate.id == fixture.profile_id; });
    combo_select_data(
        ::GetDlgItem(page, IdPatchProfile),
        profile == project_.fixture_profiles.end()
            ? -1
            : static_cast<std::intptr_t>(
                  std::distance(project_.fixture_profiles.begin(), profile)));
    combo_select_data(::GetDlgItem(page, IdPatchUniverse), fixture.universe);
    set_control_text(::GetDlgItem(page, IdPatchAddress), number_text(fixture.address));
    std::ostringstream roles;
    for (const auto& role : fixture.roles) {
        roles << role << '\n';
    }
    set_control_text(::GetDlgItem(page, IdPatchRoles), roles.str());
    ::EnableWindow(::GetDlgItem(page, IdPatchDelete), TRUE);
    set_page_message(Page::Patch, IdPatchMessage, "Editing patched fixture " + fixture.id + ".");
}

void Application::new_fixture() {
    fixture_index_ = -1;
    const auto page = pages_[static_cast<std::size_t>(Page::Patch)];
    set_control_text(::GetDlgItem(page, IdPatchName), "");
    combo_select_data(::GetDlgItem(page, IdPatchProfile), 0);
    std::uint8_t universe = 1;
    std::uint16_t address = 1;
    for (const auto& fixture : project_.fixtures) {
        if (fixture.universe != universe) {
            continue;
        }
        const auto profile = std::find_if(
            project_.fixture_profiles.begin(), project_.fixture_profiles.end(),
            [&](const auto& candidate) { return candidate.id == fixture.profile_id; });
        if (profile != project_.fixture_profiles.end()) {
            address = std::max<std::uint16_t>(
                address,
                static_cast<std::uint16_t>(fixture.address + profile->footprint));
        }
    }
    if (address > showcore::kUniverseSlots) {
        universe = 2;
        address = 1;
    }
    combo_select_data(::GetDlgItem(page, IdPatchUniverse), universe);
    set_control_text(::GetDlgItem(page, IdPatchAddress), number_text(address));
    set_control_text(::GetDlgItem(page, IdPatchRoles), "");
    ::EnableWindow(::GetDlgItem(page, IdPatchDelete), FALSE);
    set_page_message(Page::Patch, IdPatchMessage,
                     "Choose a profile and a non-overlapping universe/address.");
}

void Application::save_fixture() {
    const auto page = pages_[static_cast<std::size_t>(Page::Patch)];
    emberlights::FixtureDefinition fixture;
    fixture.name = trim(control_text(::GetDlgItem(page, IdPatchName)));
    const auto profile_index = combo_selected_data(::GetDlgItem(page, IdPatchProfile));
    const auto universe = combo_selected_data(::GetDlgItem(page, IdPatchUniverse));
    if (fixture.name.empty() || profile_index < 0 ||
        static_cast<std::size_t>(profile_index) >= project_.fixture_profiles.size() ||
        (universe != 1 && universe != 2) ||
        !parse_number(control_text(::GetDlgItem(page, IdPatchAddress)), fixture.address)) {
        set_page_message(Page::Patch, IdPatchMessage,
                         "Fixture name, profile, universe, and valid address are required.", true);
        return;
    }
    fixture.id = fixture_index_ >= 0
        ? project_.fixtures[static_cast<std::size_t>(fixture_index_)].id
        : unique_id("fixture", fixture.name);
    fixture.profile_id = project_.fixture_profiles[static_cast<std::size_t>(profile_index)].id;
    fixture.universe = static_cast<std::uint8_t>(universe);
    fixture.roles = lines(control_text(::GetDlgItem(page, IdPatchRoles)));
    auto candidate = project_;
    if (fixture_index_ >= 0) {
        candidate.fixtures[static_cast<std::size_t>(fixture_index_)] = fixture;
    } else {
        candidate.fixtures.push_back(fixture);
    }
    const auto validation = emberlights::validate_project(candidate);
    if (!validation.ok()) {
        set_page_message(Page::Patch, IdPatchMessage, first_validation_error(validation), true);
        return;
    }
    project_ = std::move(candidate);
    if (fixture_index_ < 0) {
        fixture_index_ = static_cast<std::int32_t>(project_.fixtures.size() - 1U);
    }
    mark_dirty();
    refresh_patch();
    refresh_live_lists();
    set_page_message(Page::Patch, IdPatchMessage, "Fixture patch saved.");
}

void Application::delete_fixture() {
    if (fixture_index_ < 0 || static_cast<std::size_t>(fixture_index_) >= project_.fixtures.size()) {
        return;
    }
    const auto id = project_.fixtures[static_cast<std::size_t>(fixture_index_)].id;
    const bool referenced_by_look = std::any_of(
        project_.looks.begin(), project_.looks.end(), [&](const auto& look) {
            return std::any_of(
                look.assignments.begin(), look.assignments.end(),
                [&](const auto& assignment) { return assignment.fixture_id == id; });
        });
    const bool referenced_by_group = std::any_of(
        project_.groups.begin(), project_.groups.end(), [&](const auto& group) {
            return std::find(group.fixture_ids.begin(), group.fixture_ids.end(), id) !=
                group.fixture_ids.end();
        });
    if (referenced_by_look || referenced_by_group) {
        set_page_message(Page::Patch, IdPatchMessage,
                         "This fixture is referenced by a Static Look or group. Remove those references first.", true);
        return;
    }
    if (::MessageBoxW(window_, L"Delete this fixture from the patch?", L"Delete fixture",
                      MB_YESNO | MB_ICONWARNING) != IDYES) {
        return;
    }
    project_.fixtures.erase(project_.fixtures.begin() + fixture_index_);
    fixture_index_ = -1;
    mark_dirty();
    refresh_patch();
}

void Application::select_group(std::int32_t index) {
    if (index < 0 || static_cast<std::size_t>(index) >= project_.groups.size()) {
        new_group();
        return;
    }
    group_index_ = index;
    const auto page = pages_[static_cast<std::size_t>(Page::Groups)];
    const auto& group = project_.groups[static_cast<std::size_t>(index)];
    set_control_text(::GetDlgItem(page, IdGroupName), group.name);
    std::ostringstream members;
    for (const auto& fixture_id : group.fixture_ids) {
        members << fixture_id << '\n';
    }
    set_control_text(::GetDlgItem(page, IdGroupMembers), members.str());
    ::EnableWindow(::GetDlgItem(page, IdGroupDuplicate), TRUE);
    ::EnableWindow(::GetDlgItem(page, IdGroupDelete), TRUE);
    set_page_message(Page::Groups, IdGroupMessage, "Editing fixture group " + group.id + ".");
}

void Application::new_group() {
    group_index_ = -1;
    const auto page = pages_[static_cast<std::size_t>(Page::Groups)];
    set_control_text(::GetDlgItem(page, IdGroupName), "");
    set_control_text(::GetDlgItem(page, IdGroupMembers), "");
    ::EnableWindow(::GetDlgItem(page, IdGroupDuplicate), FALSE);
    ::EnableWindow(::GetDlgItem(page, IdGroupDelete), FALSE);
    set_page_message(
        Page::Groups,
        IdGroupMessage,
        "Create a reusable group from the stable fixture IDs shown in Patch and Static Looks.");
}

void Application::duplicate_group() {
    if (group_index_ < 0 || static_cast<std::size_t>(group_index_) >= project_.groups.size()) {
        return;
    }
    const auto page = pages_[static_cast<std::size_t>(Page::Groups)];
    set_control_text(
        ::GetDlgItem(page, IdGroupName),
        project_.groups[static_cast<std::size_t>(group_index_)].name + " Copy");
    group_index_ = -1;
    ::EnableWindow(::GetDlgItem(page, IdGroupDuplicate), FALSE);
    ::EnableWindow(::GetDlgItem(page, IdGroupDelete), FALSE);
    set_page_message(Page::Groups, IdGroupMessage, "Edit the copy and choose Save Group.");
}

void Application::save_group() {
    const auto page = pages_[static_cast<std::size_t>(Page::Groups)];
    emberlights::GroupDefinition group;
    group.name = trim(control_text(::GetDlgItem(page, IdGroupName)));
    group.fixture_ids = lines(control_text(::GetDlgItem(page, IdGroupMembers)));
    if (group.name.empty() || group.fixture_ids.empty()) {
        set_page_message(
            Page::Groups,
            IdGroupMessage,
            "A group needs a name and at least one patched fixture ID.",
            true);
        return;
    }
    group.id = group_index_ >= 0
        ? project_.groups[static_cast<std::size_t>(group_index_)].id
        : unique_id("group", group.name);
    auto candidate = project_;
    if (group_index_ >= 0) {
        candidate.groups[static_cast<std::size_t>(group_index_)] = group;
    } else {
        candidate.groups.push_back(group);
    }
    const auto validation = emberlights::validate_project(candidate);
    if (!validation.ok()) {
        set_page_message(Page::Groups, IdGroupMessage, first_validation_error(validation), true);
        return;
    }
    project_ = std::move(candidate);
    if (group_index_ < 0) {
        group_index_ = static_cast<std::int32_t>(project_.groups.size() - 1U);
    }
    mark_dirty();
    refresh_groups();
    set_page_message(Page::Groups, IdGroupMessage, "Fixture group saved.");
}

void Application::delete_group() {
    if (group_index_ < 0 || static_cast<std::size_t>(group_index_) >= project_.groups.size()) {
        return;
    }
    if (::MessageBoxW(
            window_,
            L"Delete this fixture group? Existing Static Looks keep their expanded fixture values.",
            L"Delete group",
            MB_YESNO | MB_ICONWARNING) != IDYES) {
        return;
    }
    project_.groups.erase(project_.groups.begin() + group_index_);
    group_index_ = -1;
    mark_dirty();
    refresh_groups();
}

namespace {

[[nodiscard]] bool parse_autoloop_rows(
    std::string_view text,
    std::vector<emberlights::AutoloopStepDefinition>& steps,
    std::string& error_message) {
    steps.clear();
    std::size_t row = 0;
    for (const auto& line : lines(text)) {
        ++row;
        const auto fields = split_csv(line);
        emberlights::AutoloopStepDefinition step;
        if (fields.size() != 3U || !parse_number(fields[0], step.at_beat) ||
            step.at_beat < 0.0F || fields[1].empty()) {
            error_message = "Step row " + number_text(row) +
                " must be beat, look-id, and cut or linear.";
            return false;
        }
        step.look_id = fields[1];
        if (fields[2] == "cut") {
            step.transition = showcore::AutoloopTransition::Cut;
        } else if (fields[2] == "linear") {
            step.transition = showcore::AutoloopTransition::Linear;
        } else {
            error_message = "Step row " + number_text(row) +
                " transition must be cut or linear.";
            return false;
        }
        steps.push_back(std::move(step));
    }
    if (steps.empty()) {
        error_message = "An Autoloop needs at least one step.";
        return false;
    }
    return true;
}

[[nodiscard]] bool parse_track_cue_rows(
    std::string_view text,
    std::vector<emberlights::TrackCueDefinition>& cues,
    std::string& error_message) {
    cues.clear();
    std::size_t row = 0;
    for (const auto& line : lines(text)) {
        ++row;
        const auto fields = split_csv(line);
        emberlights::TrackCueDefinition cue;
        if (fields.size() != 3U || !parse_number(fields[0], cue.at_beat) ||
            cue.at_beat < 0.0F || !emberlights::parse_track_cue_action(fields[1], cue.action)) {
            error_message = "Cue row " + number_text(row) +
                " must be beat, action, and target-id (blank for clear actions).";
            return false;
        }
        cue.target_ref = fields[2];
        const bool needs_target = cue.action == emberlights::TrackCueAction::TriggerLook ||
            cue.action == emberlights::TrackCueAction::TriggerAutoloop;
        const bool clear_action = cue.action == emberlights::TrackCueAction::ClearLook ||
            cue.action == emberlights::TrackCueAction::ClearAutoloop;
        if ((needs_target && cue.target_ref.empty()) || (clear_action && !cue.target_ref.empty())) {
            error_message = "Cue row " + number_text(row) +
                " needs a target only for triggerLook and triggerAutoloop.";
            return false;
        }
        cues.push_back(std::move(cue));
    }
    return true;
}

}  // namespace

void Application::select_look(std::int32_t index) {
    if (index < 0 || static_cast<std::size_t>(index) >= project_.looks.size()) {
        new_look();
        return;
    }
    look_index_ = index;
    const auto page = pages_[static_cast<std::size_t>(Page::Looks)];
    const auto& look = project_.looks[static_cast<std::size_t>(index)];
    set_control_text(::GetDlgItem(page, IdLookName), look.name);
    set_control_text(::GetDlgItem(page, IdLookFade), number_text(look.fade_ms));
    look_draft_ = emberlights::make_static_look_draft(0U, look.id, look.name);
    look_draft_->source_index = static_cast<std::size_t>(index);
    look_draft_->look = look;
    refresh_look_draft_view();
    ::EnableWindow(::GetDlgItem(page, IdLookDelete), TRUE);
    set_page_message(Page::Looks, IdLookMessage, "Editing Static Look " + look.id + ".");
}

void Application::new_look() {
    look_index_ = -1;
    const auto page = pages_[static_cast<std::size_t>(Page::Looks)];
    set_control_text(::GetDlgItem(page, IdLookName), "");
    set_control_text(::GetDlgItem(page, IdLookFade), "750");
    set_control_text(::GetDlgItem(page, IdLookValue), "100");
    combo_select_data(
        ::GetDlgItem(page, IdLookOwnership),
        static_cast<std::intptr_t>(showcore::ValueMode::Set));
    ::EnableWindow(::GetDlgItem(page, IdLookValue), TRUE);
    look_draft_ = emberlights::make_static_look_draft(0U, "", "New Static Look");
    look_draft_->look.name.clear();
    refresh_look_draft_view();
    ::EnableWindow(::GetDlgItem(page, IdLookDelete), FALSE);
    set_page_message(Page::Looks, IdLookMessage,
                     "Choose a target, then apply a full color or explicit property ownership.");
}

void Application::duplicate_look() {
    if (look_index_ < 0 || static_cast<std::size_t>(look_index_) >= project_.looks.size()) {
        return;
    }
    const auto source = look_draft_.has_value()
        ? *look_draft_
        : emberlights::make_static_look_draft(0U, "", "Static Look");
    const auto source_name = source.look.name;
    look_index_ = -1;
    look_draft_ = emberlights::duplicate_static_look_draft(
        source, "", source_name + " Copy");
    const auto page = pages_[static_cast<std::size_t>(Page::Looks)];
    static_cast<void>(::SendMessageW(::GetDlgItem(page, IdLookList), LB_SETCURSEL, -1, 0));
    set_control_text(::GetDlgItem(page, IdLookName), look_draft_->look.name);
    set_control_text(::GetDlgItem(page, IdLookFade), number_text(look_draft_->look.fade_ms));
    refresh_look_draft_view();
    ::EnableWindow(::GetDlgItem(page, IdLookDelete), FALSE);
    set_page_message(Page::Looks, IdLookMessage,
                     "Duplicated into an unsaved Static Look draft.");
}

void Application::save_look() {
    const auto page = pages_[static_cast<std::size_t>(Page::Looks)];
    if (!look_draft_.has_value()) {
        new_look();
    }
    auto look = look_draft_->look;
    look.name = trim(control_text(::GetDlgItem(page, IdLookName)));
    if (look.name.empty() || look.name.size() > emberlights::kMaximumStaticLookNameLength ||
        !parse_number(control_text(::GetDlgItem(page, IdLookFade)), look.fade_ms) ||
        look.fade_ms > emberlights::kMaximumStaticLookFadeMs) {
        set_page_message(Page::Looks, IdLookMessage,
                         "Name and a crossfade from 0 through 30000 ms are required.", true);
        return;
    }
    if (look.assignments.empty()) {
        set_page_message(
            Page::Looks,
            IdLookMessage,
            "Apply a full color or a property before saving this Static Look.",
            true);
        return;
    }
    look.id = look_index_ >= 0
        ? project_.looks[static_cast<std::size_t>(look_index_)].id
        : unique_id("look", look.name);
    auto candidate = project_;
    if (look_index_ >= 0) {
        candidate.looks[static_cast<std::size_t>(look_index_)] = look;
    } else {
        candidate.looks.push_back(look);
    }
    const auto validation = emberlights::validate_project(candidate);
    if (!validation.ok()) {
        set_page_message(Page::Looks, IdLookMessage, first_validation_error(validation), true);
        return;
    }
    project_ = std::move(candidate);
    if (look_index_ < 0) {
        look_index_ = static_cast<std::int32_t>(project_.looks.size() - 1U);
    }
    look_draft_->look = look;
    look_draft_->look.id = project_.looks[static_cast<std::size_t>(look_index_)].id;
    look_draft_->source_index = static_cast<std::size_t>(look_index_);
    mark_dirty();
    refresh_looks();
    refresh_autoloops();
    refresh_tracks();
    refresh_live_lists();
    set_page_message(Page::Looks, IdLookMessage, "Static Look saved.");
}

void Application::delete_look() {
    if (look_index_ < 0 || static_cast<std::size_t>(look_index_) >= project_.looks.size()) {
        return;
    }
    const auto id = project_.looks[static_cast<std::size_t>(look_index_)].id;
    const auto dependencies = emberlights::inspect_static_look_dependencies(project_, id);
    if (dependencies.blocked()) {
        std::ostringstream message;
        message << "This Static Look is still referenced by "
                << dependencies.autoloop_steps << " Autoloop step(s), "
                << dependencies.track_cues << " track cue(s), and "
                << dependencies.midi_bindings << " MIDI binding(s). Reassign them first.";
        set_page_message(Page::Looks, IdLookMessage, message.str(), true);
        return;
    }
    if (::MessageBoxW(window_, L"Delete this Static Look?", L"Delete Static Look",
                      MB_YESNO | MB_ICONWARNING) != IDYES) {
        return;
    }
    project_.looks.erase(project_.looks.begin() + look_index_);
    look_index_ = -1;
    look_draft_.reset();
    mark_dirty();
    refresh_looks();
    refresh_autoloops();
    refresh_tracks();
    refresh_midi();
    refresh_live_lists();
}

std::string Application::selected_look_target_id() const {
    const auto page = pages_[static_cast<std::size_t>(Page::Looks)];
    const auto data = combo_selected_data(::GetDlgItem(page, IdLookTarget), -1);
    if (data < 0) {
        return {};
    }
    const auto index = static_cast<std::size_t>(data);
    if (index < project_.fixtures.size()) {
        return project_.fixtures[index].id;
    }
    const auto group_index = index - project_.fixtures.size();
    return group_index < project_.groups.size()
        ? project_.groups[group_index].id
        : std::string{};
}

bool Application::read_static_look_color(
    emberlights::StaticLookColor& color,
    std::string& error_message) const {
    const auto page = pages_[static_cast<std::size_t>(Page::Looks)];
    constexpr std::array ids{
        IdLookRed,
        IdLookGreen,
        IdLookBlue,
        IdLookWhite,
        IdLookAmber,
        IdLookUv,
        IdLookIntensity};
    std::array<float, ids.size()> values{};
    for (std::size_t index = 0U; index < ids.size(); ++index) {
        if (!parse_number(control_text(::GetDlgItem(page, ids[index])), values[index]) ||
            !std::isfinite(values[index]) || values[index] < 0.0F ||
            values[index] > 100.0F) {
            error_message = "Every direct emitter and Master value must be from 0 through 100 percent.";
            return false;
        }
        values[index] /= 100.0F;
    }
    color = {values[0], values[1], values[2], values[3], values[4], values[5], values[6]};
    return true;
}

void Application::pick_static_look_rgb() {
    emberlights::StaticLookColor color;
    std::string error_message;
    if (!read_static_look_color(color, error_message)) {
        set_page_message(Page::Looks, IdLookMessage, error_message, true);
        return;
    }
    static std::array<COLORREF, 16U> custom_colors{};
    CHOOSECOLORW chooser{};
    chooser.lStructSize = sizeof(chooser);
    chooser.hwndOwner = window_;
    chooser.rgbResult = RGB(
        static_cast<BYTE>(std::lround(color.red * 255.0F)),
        static_cast<BYTE>(std::lround(color.green * 255.0F)),
        static_cast<BYTE>(std::lround(color.blue * 255.0F)));
    chooser.lpCustColors = custom_colors.data();
    chooser.Flags = CC_FULLOPEN | CC_RGBINIT;
    if (::ChooseColorW(&chooser) == FALSE) {
        return;
    }
    color.red = static_cast<float>(GetRValue(chooser.rgbResult)) / 255.0F;
    color.green = static_cast<float>(GetGValue(chooser.rgbResult)) / 255.0F;
    color.blue = static_cast<float>(GetBValue(chooser.rgbResult)) / 255.0F;
    set_static_look_color_controls(
        pages_[static_cast<std::size_t>(Page::Looks)], color);
    set_page_message(
        Page::Looks,
        IdLookMessage,
        "RGB selected. White, Amber, and UV were left unchanged; choose Apply Full Color to author it.");
}

void Application::apply_static_look_color() {
    if (!look_draft_.has_value()) {
        new_look();
    }
    emberlights::StaticLookColor color;
    std::string error_message;
    if (!read_static_look_color(color, error_message)) {
        set_page_message(Page::Looks, IdLookMessage, error_message, true);
        return;
    }
    const auto outcome = emberlights::apply_static_look_color(
        *look_draft_, project_, selected_look_target_id(), color);
    refresh_look_draft_view();
    set_page_message(
        Page::Looks,
        IdLookMessage,
        static_look_outcome_text("Full color", outcome),
        !outcome);
}

void Application::apply_static_look_swatch(std::string_view swatch_id) {
    const auto swatches = emberlights::built_in_static_look_swatches();
    const auto found = std::find_if(
        swatches.begin(), swatches.end(),
        [swatch_id](const auto& swatch) { return swatch.id == swatch_id; });
    if (found == swatches.end()) {
        return;
    }
    set_static_look_color_controls(
        pages_[static_cast<std::size_t>(Page::Looks)], found->color);
    apply_static_look_color();
}

void Application::apply_static_look_property() {
    if (!look_draft_.has_value()) {
        new_look();
    }
    const auto page = pages_[static_cast<std::size_t>(Page::Looks)];
    const auto property_data = combo_selected_data(::GetDlgItem(page, IdLookProperty), -1);
    if (property_data < 0) {
        set_page_message(
            Page::Looks, IdLookMessage,
            "The selected target has no supported properties.", true);
        return;
    }
    const auto property = static_cast<showcore::Property>(property_data);
    const auto mode = static_cast<showcore::ValueMode>(
        combo_selected_data(
            ::GetDlgItem(page, IdLookOwnership),
            static_cast<std::intptr_t>(showcore::ValueMode::Set)));
    showcore::PropertyValue value;
    if (mode == showcore::ValueMode::Release) {
        value = showcore::PropertyValue::release();
    } else if (mode == showcore::ValueMode::ForceZero) {
        value = showcore::PropertyValue::force_zero();
    } else {
        float percent = 0.0F;
        if (!parse_number(control_text(::GetDlgItem(page, IdLookValue)), percent) ||
            !std::isfinite(percent) || percent < 0.0F || percent > 100.0F) {
            set_page_message(
                Page::Looks, IdLookMessage,
                "SET ownership needs a value from 0 through 100 percent.", true);
            return;
        }
        value = showcore::PropertyValue::set(percent / 100.0F);
    }
    const auto outcome = emberlights::apply_static_look_property(
        *look_draft_, project_, selected_look_target_id(), property, value);
    refresh_look_draft_view();
    set_page_message(
        Page::Looks,
        IdLookMessage,
        static_look_outcome_text("Property ownership", outcome),
        !outcome);
}

void Application::remove_static_look_property() {
    if (!look_draft_.has_value()) {
        return;
    }
    const auto page = pages_[static_cast<std::size_t>(Page::Looks)];
    const auto property_data = combo_selected_data(::GetDlgItem(page, IdLookProperty), -1);
    if (property_data < 0) {
        return;
    }
    const auto outcome = emberlights::remove_static_look_property(
        *look_draft_,
        project_,
        selected_look_target_id(),
        static_cast<showcore::Property>(property_data));
    refresh_look_draft_view();
    set_page_message(
        Page::Looks,
        IdLookMessage,
        static_look_outcome_text("Property removal", outcome),
        !outcome);
}

void Application::preview_static_look() {
    const auto page = pages_[static_cast<std::size_t>(Page::Looks)];
    if (!look_draft_.has_value() || look_draft_->look.assignments.empty()) {
        set_page_message(
            Page::Looks, IdLookMessage,
            "Apply at least one color or property before building a preview.", true);
        return;
    }
    auto draft = *look_draft_;
    draft.look.name = trim(control_text(::GetDlgItem(page, IdLookName)));
    if (draft.look.name.empty()) {
        draft.look.name = "Unsaved Static Look Preview";
    }
    if (!parse_number(control_text(::GetDlgItem(page, IdLookFade)), draft.look.fade_ms) ||
        draft.look.fade_ms > emberlights::kMaximumStaticLookFadeMs) {
        set_page_message(
            Page::Looks, IdLookMessage,
            "Preview needs a crossfade from 0 through 30000 ms.", true);
        return;
    }
    if (draft.look.id.empty()) {
        draft.look.id = unique_id("preview-look", draft.look.name);
        draft.source_index.reset();
    }
    const auto preview = emberlights::preview_static_look_draft(project_, draft);
    set_control_text(
        ::GetDlgItem(page, IdLookPreviewText),
        emberlights::format_static_look_preview(preview));
    set_page_message(
        Page::Looks,
        IdLookMessage,
        preview
            ? "Exact offline DMX frame built. No output adapter was opened."
            : "Offline preview failed; review the trace and validation errors.",
        !preview);
}

void Application::select_autoloop(std::int32_t index) {
    if (index < 0 || static_cast<std::size_t>(index) >= project_.autoloops.size()) {
        new_autoloop();
        return;
    }
    autoloop_index_ = index;
    const auto page = pages_[static_cast<std::size_t>(Page::Autoloops)];
    const auto& loop = project_.autoloops[static_cast<std::size_t>(index)];
    set_control_text(::GetDlgItem(page, IdAutoloopName), loop.name);
    set_control_text(::GetDlgItem(page, IdAutoloopBank), number_text(loop.bank + 1U));
    set_control_text(::GetDlgItem(page, IdAutoloopSlot),
                     number_text(static_cast<unsigned int>(loop.slot + 1U)));
    set_control_text(::GetDlgItem(page, IdAutoloopLength), number_text(loop.length_beats));
    combo_select_data(::GetDlgItem(page, IdAutoloopRepeat),
                      static_cast<std::intptr_t>(loop.repeat));
    set_control_text(::GetDlgItem(page, IdAutoloopSteps), autoloop_steps_text(loop));
    ::EnableWindow(::GetDlgItem(page, IdAutoloopDelete), TRUE);
    ::EnableWindow(::GetDlgItem(page, IdAutoloopNextEmpty), TRUE);
    ::EnableWindow(::GetDlgItem(page, IdAutoloopSwapTarget), TRUE);
    set_page_message(Page::Autoloops, IdAutoloopMessage,
                     "Editing Autoloop " + loop.id + ".");
}

void Application::new_autoloop() {
    autoloop_index_ = -1;
    const auto page = pages_[static_cast<std::size_t>(Page::Autoloops)];
    std::uint16_t bank = 0;
    std::uint8_t slot = 0;
    bool found = false;
    for (std::size_t candidate = 0; candidate < showcore::kMaxAutoloops; ++candidate) {
        bank = static_cast<std::uint16_t>(candidate / showcore::kAutoloopsPerBank);
        slot = static_cast<std::uint8_t>(candidate % showcore::kAutoloopsPerBank);
        if (std::none_of(
                project_.autoloops.begin(), project_.autoloops.end(), [&](const auto& loop) {
                    return loop.bank == bank && loop.slot == slot;
                })) {
            found = true;
            break;
        }
    }
    set_control_text(::GetDlgItem(page, IdAutoloopName), "");
    set_control_text(::GetDlgItem(page, IdAutoloopBank), number_text(bank + 1U));
    set_control_text(::GetDlgItem(page, IdAutoloopSlot),
                     number_text(static_cast<unsigned int>(slot + 1U)));
    set_control_text(::GetDlgItem(page, IdAutoloopLength), "4");
    combo_select_data(::GetDlgItem(page, IdAutoloopRepeat),
                      static_cast<std::intptr_t>(showcore::AutoloopRepeat::Infinite));
    set_control_text(::GetDlgItem(page, IdAutoloopSteps), "0,look-id,cut\r\n");
    ::EnableWindow(::GetDlgItem(page, IdAutoloopDelete), FALSE);
    ::EnableWindow(::GetDlgItem(page, IdAutoloopNextEmpty), FALSE);
    ::EnableWindow(::GetDlgItem(page, IdAutoloopSwapTarget), FALSE);
    set_page_message(
        Page::Autoloops,
        IdAutoloopMessage,
        found ? "The first open bank/slot was selected. Replace look-id with a Static Look ID."
              : "The compiled 2,048-slot Autoloop library is full.",
        !found);
}

void Application::duplicate_autoloop() {
    if (autoloop_index_ < 0 ||
        static_cast<std::size_t>(autoloop_index_) >= project_.autoloops.size()) {
        return;
    }
    const auto source = project_.autoloops[static_cast<std::size_t>(autoloop_index_)];
    new_autoloop();
    const auto page = pages_[static_cast<std::size_t>(Page::Autoloops)];
    set_control_text(::GetDlgItem(page, IdAutoloopName), source.name + " Copy");
    set_control_text(::GetDlgItem(page, IdAutoloopLength), number_text(source.length_beats));
    combo_select_data(::GetDlgItem(page, IdAutoloopRepeat),
                      static_cast<std::intptr_t>(source.repeat));
    set_control_text(::GetDlgItem(page, IdAutoloopSteps), autoloop_steps_text(source));
}

bool Application::autoloop_editor_has_unsaved_content() const {
    if (autoloop_index_ < 0 ||
        static_cast<std::size_t>(autoloop_index_) >= project_.autoloops.size()) {
        return false;
    }
    const auto page = pages_[static_cast<std::size_t>(Page::Autoloops)];
    const auto& loop = project_.autoloops[static_cast<std::size_t>(autoloop_index_)];
    return trim(control_text(::GetDlgItem(page, IdAutoloopName))) != loop.name ||
        trim(control_text(::GetDlgItem(page, IdAutoloopLength))) != number_text(loop.length_beats) ||
        combo_selected_data(
            ::GetDlgItem(page, IdAutoloopRepeat),
            static_cast<std::intptr_t>(showcore::AutoloopRepeat::Infinite)) !=
            static_cast<std::intptr_t>(loop.repeat) ||
        normalize_newlines(control_text(::GetDlgItem(page, IdAutoloopSteps))) !=
            autoloop_steps_text(loop);
}

void Application::move_autoloop_to_next_empty() {
    if (autoloop_index_ < 0 ||
        static_cast<std::size_t>(autoloop_index_) >= project_.autoloops.size()) {
        return;
    }
    if (autoloop_editor_has_unsaved_content()) {
        set_page_message(Page::Autoloops, IdAutoloopMessage,
                         "Save content edits before changing this Autoloop's placement.", true);
        return;
    }
    const auto id = project_.autoloops[static_cast<std::size_t>(autoloop_index_)].id;
    const auto result = emberlights::move_autoloop_to_next_empty_slot(project_, id);
    if (result != emberlights::AutoloopPlacementResult::Moved) {
        set_page_message(
            Page::Autoloops,
            IdAutoloopMessage,
            result == emberlights::AutoloopPlacementResult::LibraryFull
                ? "Every one of the 2,048 Autoloop slots is occupied."
                : "The selected Autoloop could not be moved safely.",
            true);
        return;
    }
    mark_dirty();
    refresh_autoloops();
    refresh_live_lists();
    set_page_message(Page::Autoloops, IdAutoloopMessage,
                     "Autoloop moved to the next open slot. Use Undo to revert it.");
}

void Application::swap_autoloop_into_target_slot() {
    if (autoloop_index_ < 0 ||
        static_cast<std::size_t>(autoloop_index_) >= project_.autoloops.size()) {
        return;
    }
    if (autoloop_editor_has_unsaved_content()) {
        set_page_message(Page::Autoloops, IdAutoloopMessage,
                         "Save content edits before changing this Autoloop's placement.", true);
        return;
    }
    const auto page = pages_[static_cast<std::size_t>(Page::Autoloops)];
    std::uint16_t bank = 0U;
    std::uint16_t slot = 0U;
    if (!parse_number(control_text(::GetDlgItem(page, IdAutoloopBank)), bank) ||
        bank == 0U || bank > showcore::kMaxAutoloopBanks ||
        !parse_number(control_text(::GetDlgItem(page, IdAutoloopSlot)), slot) ||
        slot == 0U || slot > showcore::kAutoloopsPerBank) {
        set_page_message(Page::Autoloops, IdAutoloopMessage,
                         "Enter a target bank 1–64 and slot 1–32 before swapping.", true);
        return;
    }
    const auto id = project_.autoloops[static_cast<std::size_t>(autoloop_index_)].id;
    const auto result = emberlights::move_autoloop(
        project_,
        id,
        static_cast<std::uint16_t>(bank - 1U),
        static_cast<std::uint8_t>(slot - 1U),
        true);
    if (result != emberlights::AutoloopPlacementResult::Moved &&
        result != emberlights::AutoloopPlacementResult::Swapped) {
        set_page_message(Page::Autoloops, IdAutoloopMessage,
                         "The selected Autoloop could not be moved into that slot safely.", true);
        return;
    }
    mark_dirty();
    refresh_autoloops();
    refresh_live_lists();
    set_page_message(
        Page::Autoloops,
        IdAutoloopMessage,
        result == emberlights::AutoloopPlacementResult::Swapped
            ? "Autoloop slots swapped. Use Undo to revert the placement."
            : "Autoloop moved into the requested open slot. Use Undo to revert it.");
}

void Application::save_autoloop() {
    const auto page = pages_[static_cast<std::size_t>(Page::Autoloops)];
    emberlights::AutoloopDefinition loop;
    loop.name = trim(control_text(::GetDlgItem(page, IdAutoloopName)));
    std::uint16_t bank = 0;
    std::uint16_t slot = 0;
    if (loop.name.empty() ||
        !parse_number(control_text(::GetDlgItem(page, IdAutoloopBank)), bank) ||
        bank == 0U || bank > showcore::kMaxAutoloopBanks ||
        !parse_number(control_text(::GetDlgItem(page, IdAutoloopSlot)), slot) ||
        slot == 0U || slot > showcore::kAutoloopsPerBank ||
        !parse_number(control_text(::GetDlgItem(page, IdAutoloopLength)), loop.length_beats) ||
        loop.length_beats <= 0.0F) {
        set_page_message(Page::Autoloops, IdAutoloopMessage,
                         "Name, bank 1–64, slot 1–32, and a positive musical length are required.", true);
        return;
    }
    loop.bank = static_cast<std::uint16_t>(bank - 1U);
    loop.slot = static_cast<std::uint8_t>(slot - 1U);
    loop.repeat = static_cast<showcore::AutoloopRepeat>(combo_selected_data(
        ::GetDlgItem(page, IdAutoloopRepeat),
        static_cast<std::intptr_t>(showcore::AutoloopRepeat::Infinite)));
    std::string parse_error;
    if (!parse_autoloop_rows(
            control_text(::GetDlgItem(page, IdAutoloopSteps)), loop.steps, parse_error)) {
        set_page_message(Page::Autoloops, IdAutoloopMessage, parse_error, true);
        return;
    }
    loop.id = autoloop_index_ >= 0
        ? project_.autoloops[static_cast<std::size_t>(autoloop_index_)].id
        : unique_id("autoloop", loop.name);
    auto candidate = project_;
    if (autoloop_index_ >= 0) {
        candidate.autoloops[static_cast<std::size_t>(autoloop_index_)] = loop;
    } else {
        candidate.autoloops.push_back(loop);
    }
    const auto validation = emberlights::validate_project(candidate);
    if (!validation.ok()) {
        set_page_message(Page::Autoloops, IdAutoloopMessage,
                         first_validation_error(validation), true);
        return;
    }
    project_ = std::move(candidate);
    if (autoloop_index_ < 0) {
        autoloop_index_ = static_cast<std::int32_t>(project_.autoloops.size() - 1U);
    }
    mark_dirty();
    refresh_autoloops();
    refresh_tracks();
    refresh_live_lists();
    set_page_message(Page::Autoloops, IdAutoloopMessage, "Autoloop saved.");
}

void Application::delete_autoloop() {
    if (autoloop_index_ < 0 ||
        static_cast<std::size_t>(autoloop_index_) >= project_.autoloops.size()) {
        return;
    }
    const auto id = project_.autoloops[static_cast<std::size_t>(autoloop_index_)].id;
    if (std::any_of(
            project_.track_scripts.begin(), project_.track_scripts.end(), [&](const auto& track) {
                return std::any_of(
                    track.cues.begin(), track.cues.end(), [&](const auto& cue) {
                        return cue.action == emberlights::TrackCueAction::TriggerAutoloop &&
                            cue.target_ref == id;
                    });
            })) {
        set_page_message(Page::Autoloops, IdAutoloopMessage,
                         "This Autoloop is used by a track script. Reassign those cues first.", true);
        return;
    }
    if (::MessageBoxW(window_, L"Delete this Autoloop?", L"Delete Autoloop",
                      MB_YESNO | MB_ICONWARNING) != IDYES) {
        return;
    }
    project_.autoloops.erase(project_.autoloops.begin() + autoloop_index_);
    autoloop_index_ = -1;
    mark_dirty();
    refresh_autoloops();
    refresh_tracks();
    refresh_live_lists();
}

void Application::select_track(std::int32_t index) {
    if (index < 0 || static_cast<std::size_t>(index) >= project_.track_scripts.size()) {
        new_track();
        return;
    }
    track_index_ = index;
    const auto page = pages_[static_cast<std::size_t>(Page::Tracks)];
    const auto& track = project_.track_scripts[static_cast<std::size_t>(index)];
    set_control_text(::GetDlgItem(page, IdTrackName), track.name);
    refresh_track_audio_assets(track.audio_asset_id);
    set_control_text(::GetDlgItem(page, IdTrackAudioKey), track.audio_key);
    set_control_text(::GetDlgItem(page, IdTrackCues), track_cues_text(track));
    ::EnableWindow(::GetDlgItem(page, IdTrackDuplicate), TRUE);
    ::EnableWindow(::GetDlgItem(page, IdTrackDelete), TRUE);
    set_page_message(Page::Tracks, IdTrackMessage,
                     "Editing track script " + track.id + ".");
}

void Application::new_track() {
    track_index_ = -1;
    const auto page = pages_[static_cast<std::size_t>(Page::Tracks)];
    set_control_text(::GetDlgItem(page, IdTrackName), "");
    refresh_track_audio_assets({});
    set_control_text(::GetDlgItem(page, IdTrackAudioKey), "");
    set_control_text(::GetDlgItem(page, IdTrackCues), "");
    ::EnableWindow(::GetDlgItem(page, IdTrackDuplicate), FALSE);
    ::EnableWindow(::GetDlgItem(page, IdTrackDelete), FALSE);
    set_page_message(Page::Tracks, IdTrackMessage,
                     "Create a portable beat script. Link audio by content identity when it is available.");
}

void Application::duplicate_track() {
    if (track_index_ < 0 ||
        static_cast<std::size_t>(track_index_) >= project_.track_scripts.size()) {
        return;
    }
    const auto source = project_.track_scripts[static_cast<std::size_t>(track_index_)];
    new_track();
    const auto page = pages_[static_cast<std::size_t>(Page::Tracks)];
    set_control_text(::GetDlgItem(page, IdTrackName), source.name + " Copy");
    refresh_track_audio_assets(source.audio_asset_id);
    set_control_text(::GetDlgItem(page, IdTrackAudioKey), source.audio_key);
    set_control_text(::GetDlgItem(page, IdTrackCues), track_cues_text(source));
}

void Application::save_track() {
    const auto page = pages_[static_cast<std::size_t>(Page::Tracks)];
    emberlights::TrackScriptDefinition track;
    track.name = trim(control_text(::GetDlgItem(page, IdTrackName)));
    track.audio_key = trim(control_text(::GetDlgItem(page, IdTrackAudioKey)));
    const auto audio_asset_index = combo_selected_data(
        ::GetDlgItem(page, IdTrackAudioAsset), -1);
    if (audio_asset_index >= 0 &&
        static_cast<std::size_t>(audio_asset_index) < project_.audio_assets.size()) {
        track.audio_asset_id = project_.audio_assets[static_cast<std::size_t>(audio_asset_index)].id;
    }
    if (track.name.empty()) {
        set_page_message(Page::Tracks, IdTrackMessage, "A track-script name is required.", true);
        return;
    }
    std::string parse_error;
    if (!parse_track_cue_rows(
            control_text(::GetDlgItem(page, IdTrackCues)), track.cues, parse_error)) {
        set_page_message(Page::Tracks, IdTrackMessage, parse_error, true);
        return;
    }
    track.id = track_index_ >= 0
        ? project_.track_scripts[static_cast<std::size_t>(track_index_)].id
        : unique_id("track", track.name);
    auto candidate = project_;
    if (track_index_ >= 0) {
        candidate.track_scripts[static_cast<std::size_t>(track_index_)] = track;
    } else {
        candidate.track_scripts.push_back(track);
    }
    const auto validation = emberlights::validate_project(candidate);
    if (!validation.ok()) {
        set_page_message(Page::Tracks, IdTrackMessage, first_validation_error(validation), true);
        return;
    }
    project_ = std::move(candidate);
    if (track_index_ < 0) {
        track_index_ = static_cast<std::int32_t>(project_.track_scripts.size() - 1U);
    }
    mark_dirty();
    refresh_tracks();
    refresh_live_lists();
    set_page_message(Page::Tracks, IdTrackMessage, "Track script saved.");
}

void Application::delete_track() {
    if (track_index_ < 0 ||
        static_cast<std::size_t>(track_index_) >= project_.track_scripts.size()) {
        return;
    }
    if (::MessageBoxW(window_, L"Delete this track script?", L"Delete Track Script",
                      MB_YESNO | MB_ICONWARNING) != IDYES) {
        return;
    }
    project_.track_scripts.erase(project_.track_scripts.begin() + track_index_);
    track_index_ = -1;
    mark_dirty();
    refresh_tracks();
    refresh_live_lists();
}

void Application::import_audio_for_track(bool relink) {
    const auto page = pages_[static_cast<std::size_t>(Page::Tracks)];
    if (track_index_ < 0 ||
        static_cast<std::size_t>(track_index_) >= project_.track_scripts.size()) {
        set_page_message(
            Page::Tracks,
            IdTrackMessage,
            "Save the track script before attaching or relinking its audio.",
            true);
        return;
    }

    const auto selected_asset = combo_selected_data(
        ::GetDlgItem(page, IdTrackAudioAsset), -1);
    if (relink && (selected_asset < 0 ||
                    static_cast<std::size_t>(selected_asset) >= project_.audio_assets.size())) {
        set_page_message(
            Page::Tracks,
            IdTrackMessage,
            "Select an existing audio asset before relinking it.",
            true);
        return;
    }

    std::array<wchar_t, 32768> path{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window_;
    dialog.lpstrFilter =
        L"Audio Files (*.aac;*.aif;*.aiff;*.alac;*.flac;*.m4a;*.mp3;*.mp4;*.ogg;*.opus;*.wav;*.wma)\0"
        L"*.aac;*.aif;*.aiff;*.alac;*.flac;*.m4a;*.mp3;*.mp4;*.ogg;*.opus;*.wav;*.wma\0"
        L"All Files\0*.*\0";
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    if (::GetOpenFileNameW(&dialog) == FALSE) {
        return;
    }

    const std::filesystem::path selected_path(path.data());
    auto candidate = project_;
    std::string selected_id;
    emberlights::AudioAssetFileResult result;
    if (relink) {
        auto& asset = candidate.audio_assets[static_cast<std::size_t>(selected_asset)];
        result = emberlights::relink_audio_asset(asset, selected_path);
        selected_id = asset.id;
    } else {
        emberlights::AudioAssetDefinition imported;
        auto id_name = narrow(selected_path.stem().wstring());
        if (id_name.empty()) {
            id_name = "asset";
        }
        result = emberlights::make_audio_asset(
            selected_path,
            unique_id("audio", id_name),
            imported);
        if (result.available()) {
            const auto duplicate = std::find_if(
                candidate.audio_assets.begin(), candidate.audio_assets.end(),
                [&](const auto& asset) {
                    return asset.size_bytes == imported.size_bytes && asset.sha256 == imported.sha256;
                });
            if (duplicate != candidate.audio_assets.end()) {
                result = emberlights::relink_audio_asset(*duplicate, selected_path);
                selected_id = duplicate->id;
            } else {
                selected_id = imported.id;
                candidate.audio_assets.push_back(std::move(imported));
            }
        }
    }
    if (!result.available()) {
        set_page_message(Page::Tracks, IdTrackMessage, result.message, true);
        return;
    }
    const auto validation = emberlights::validate_project(candidate);
    if (!validation.ok()) {
        set_page_message(Page::Tracks, IdTrackMessage, first_validation_error(validation), true);
        return;
    }
    project_ = std::move(candidate);
    mark_dirty();
    refresh_track_audio_assets(selected_id);
    set_page_message(
        Page::Tracks,
        IdTrackMessage,
        relink
            ? "Audio location updated after SHA-256 and byte-size verification."
            : "Audio identity added. Save the track script to attach this asset.");
}

void Application::verify_selected_audio_for_track() {
    const auto page = pages_[static_cast<std::size_t>(Page::Tracks)];
    const auto selected_asset = combo_selected_data(
        ::GetDlgItem(page, IdTrackAudioAsset), -1);
    if (selected_asset < 0 ||
        static_cast<std::size_t>(selected_asset) >= project_.audio_assets.size()) {
        set_page_message(
            Page::Tracks,
            IdTrackMessage,
            "Select a linked audio asset before verifying it.",
            true);
        return;
    }
    const auto& asset = project_.audio_assets[static_cast<std::size_t>(selected_asset)];
    const auto result = emberlights::verify_audio_asset(asset);
    if (result.available()) {
        set_page_message(
            Page::Tracks,
            IdTrackMessage,
            "Audio identity verified: " + asset.file_name + ".");
        return;
    }
    set_page_message(
        Page::Tracks,
        IdTrackMessage,
        std::string("Audio ") + emberlights::audio_asset_file_status_name(result.status) +
            ": " + result.message,
        true);
}

void Application::resolve_audio_assets_for_project() {
    if (project_.audio_assets.empty()) {
        set_page_message(
            Page::Tracks,
            IdTrackMessage,
            "Add at least one audio asset before searching a music-library folder.",
            true);
        return;
    }
    BROWSEINFOW dialog{};
    dialog.hwndOwner = window_;
    dialog.lpszTitle = L"Choose the music-library folder EmberLights should search";
    dialog.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    const auto selection = ::SHBrowseForFolderW(&dialog);
    if (selection == nullptr) {
        return;
    }
    std::array<wchar_t, MAX_PATH> directory{};
    const auto resolved_path = ::SHGetPathFromIDListW(selection, directory.data()) != FALSE;
    ::CoTaskMemFree(selection);
    if (!resolved_path) {
        set_page_message(
            Page::Tracks,
            IdTrackMessage,
            "EmberLights could not read the selected music-library path.",
            true);
        return;
    }

    set_page_message(Page::Tracks, IdTrackMessage,
                     "Searching supported audio files by recorded size and SHA-256...", false);
    static_cast<void>(::UpdateWindow(window_));
    auto candidate = project_;
    const auto result = emberlights::resolve_audio_assets_in_directory(
        candidate, std::filesystem::path(directory.data()));
    if (!result.message.empty()) {
        set_page_message(Page::Tracks, IdTrackMessage, result.message, true);
        return;
    }
    if (result.updated_assets != 0U) {
        project_ = std::move(candidate);
        mark_dirty();
        const auto selected_track = track_index_ >= 0 &&
            static_cast<std::size_t>(track_index_) < project_.track_scripts.size()
            ? project_.track_scripts[static_cast<std::size_t>(track_index_)].audio_asset_id
            : std::string{};
        refresh_track_audio_assets(selected_track);
    }
    std::ostringstream message;
    message << "Examined " << result.files_examined << " supported file"
            << (result.files_examined == 1U ? "" : "s")
            << "; hashed " << result.hash_candidates << " size match"
            << (result.hash_candidates == 1U ? "" : "es")
            << "; matched " << result.matched_assets << " asset"
            << (result.matched_assets == 1U ? "" : "s") << ".";
    if (result.updated_assets != 0U) {
        message << " Updated " << result.updated_assets << " local path hint"
                << (result.updated_assets == 1U ? "." : "s.");
    }
    if (result.unreadable_files != 0U) {
        message << " Skipped " << result.unreadable_files << " unreadable file"
                << (result.unreadable_files == 1U ? "." : "s.");
    }
    if (result.limit_reached) {
        message << " Scan limit reached; choose a narrower folder for a complete search.";
    }
    set_page_message(Page::Tracks, IdTrackMessage, message.str(), result.limit_reached);
}

void Application::apply_connections() {
    const auto page = pages_[static_cast<std::size_t>(Page::Connections)];
    const auto previous_project = project_;
    const auto previous_connections = project_.connections;
    const auto was_running =
        runner_.status().state != emberlights::RunnerState::Stopped;
    auto updated = project_.connections;
    const auto project_name = trim(control_text(::GetDlgItem(page, IdProjectName)));
    updated.os2l_enabled = Button_GetCheck(::GetDlgItem(page, IdOs2lEnabled)) == BST_CHECKED;
    updated.os2l_bind = trim(control_text(::GetDlgItem(page, IdOs2lBind)));
    updated.artnet_enabled = Button_GetCheck(::GetDlgItem(page, IdArtnetEnabled)) == BST_CHECKED;
    updated.artnet_destination = trim(control_text(::GetDlgItem(page, IdArtnetDestination)));
    updated.sacn_enabled = Button_GetCheck(::GetDlgItem(page, IdSacnEnabled)) == BST_CHECKED;
    updated.sacn_destination = trim(control_text(::GetDlgItem(page, IdSacnDestination)));
    updated.midi_input_index = static_cast<std::int32_t>(
        combo_selected_data(::GetDlgItem(page, IdMidiInput), -1));
    updated.midi_output_index = static_cast<std::int32_t>(
        combo_selected_data(::GetDlgItem(page, IdMidiOutput), -1));
    auto selected_dmx_port = [&](int control_id, const std::string& current) {
        const auto selected = combo_selected_data(::GetDlgItem(page, control_id), -1);
        if (selected >= 0 &&
            static_cast<std::size_t>(selected) < dmx_serial_ports_.count) {
            return std::string(dmx_serial_ports_.ports[static_cast<std::size_t>(selected)].name());
        }
        return selected == -2 ? current : std::string{};
    };
    updated.dmx_usb_pro_ports[0] = selected_dmx_port(
        IdDmxUsbProUniverse1, project_.connections.dmx_usb_pro_ports[0]);
    updated.dmx_usb_pro_ports[1] = selected_dmx_port(
        IdDmxUsbProUniverse2, project_.connections.dmx_usb_pro_ports[1]);
    updated.soundswitch_micro_universe = static_cast<std::uint8_t>(
        combo_selected_data(::GetDlgItem(page, IdSoundSwitchMicroUniverse), 0));
    updated.soundswitch_micro_framing = static_cast<showcore::SoundSwitchMicroFraming>(
        combo_selected_data(
            ::GetDlgItem(page, IdSoundSwitchMicroFraming),
            static_cast<std::intptr_t>(
                showcore::SoundSwitchMicroFraming::NativeJls1)));
    updated.soundswitch_control_one_experimental =
        combo_selected_data(::GetDlgItem(page, IdSoundSwitchControlOneMode), 0) == 1;
    if (updated.soundswitch_control_one_experimental &&
        !previous_connections.soundswitch_control_one_experimental) {
        const auto confirmation = ::MessageBoxW(
            window_,
            L"Control One DMX is experimental and has not yet been physically qualified.\n\n"
            L"Before continuing:\n"
            L"• Close SoundSwitch.\n"
            L"• Disconnect fog, haze, lasers, sparks, motion, and other hazardous devices.\n"
            L"• Use a safe dimmer/test fixture and keep an independent blackout path.\n"
            L"• Jack 1 will carry EmberLights Universe 1; jack 2 will carry Universe 2.\n\n"
            L"Enable the experimental two-jack output?",
            L"Enable experimental Control One DMX",
            MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2);
        if (confirmation != IDOK) {
            combo_select_data(::GetDlgItem(page, IdSoundSwitchControlOneMode), 0);
            set_page_message(
                Page::Connections,
                IdConnectionsMessage,
                "Control One DMX remains disabled; no connection setting was changed.");
            return;
        }
    }
    if (project_name.empty() || updated.os2l_bind.empty() ||
        (updated.artnet_enabled && updated.artnet_destination.empty()) ||
        (updated.sacn_enabled && updated.sacn_destination.empty()) ||
        !parse_number(control_text(::GetDlgItem(page, IdOs2lPort)), updated.os2l_port) ||
        !parse_number(control_text(::GetDlgItem(page, IdArtnetBase)), updated.artnet_base) ||
        updated.artnet_base > 32766U ||
        !parse_number(control_text(::GetDlgItem(page, IdSacnBase)),
                      updated.sacn_universe_base) ||
        updated.sacn_universe_base == 0U || updated.sacn_universe_base > 63998U ||
        !parse_number(control_text(::GetDlgItem(page, IdFrameRate)), updated.frame_rate) ||
        !parse_number(control_text(::GetDlgItem(page, IdManualBpm)), updated.manual_bpm)) {
        set_page_message(
            Page::Connections,
            IdConnectionsMessage,
            "Check the project name, network destinations, ports, universe values, frame rate, and BPM.",
            true);
        return;
    }
    auto candidate = project_;
    candidate.name = project_name;
    candidate.connections = updated;
    const auto validation = emberlights::validate_project(candidate);
    if (!validation.ok()) {
        set_page_message(
            Page::Connections,
            IdConnectionsMessage,
            first_validation_error(validation),
            true);
        return;
    }
    project_ = std::move(candidate);
    mark_dirty();
    refresh_live_lists();
    if (!save_project(false)) {
        project_ = previous_project;
        mark_dirty();
        refresh_connections();
        set_page_message(
            Page::Connections,
            IdConnectionsMessage,
            "Connections were not saved, so EmberLights rolled them back and did not apply them. Choose a project file and press Save & Apply Connections again.",
            true);
        return;
    }
    if (was_running && previous_connections != project_.connections) {
        runner_.stop();
        ui_commands_.set_active_project(nullptr);
        active_project_.reset();
        static_cast<void>(::ModifyMenuW(
            ::GetSubMenu(::GetMenu(window_), 2),
            IdShowStartStop,
            MF_BYCOMMAND | MF_STRING,
            IdShowStartStop,
            L"&Start Show"));
        start_or_stop_show();
        const auto restart_deadline = ::GetTickCount64() + 2000U;
        auto restart_state = runner_.status().state;
        while (restart_state == emberlights::RunnerState::Starting &&
               ::GetTickCount64() < restart_deadline) {
            ::Sleep(10U);
            restart_state = runner_.status().state;
        }
        if (restart_state != emberlights::RunnerState::Running) {
            set_page_message(
                Page::Connections,
                IdConnectionsMessage,
                "Connections were saved, but Runner could not restart. Review Diagnostics before enabling output.",
                true);
            return;
        }
        set_status(
            L"Connections saved to the project and Runner restarted with the new output settings.");
        set_page_message(
            Page::Connections,
            IdConnectionsMessage,
            "Saved to project and applied. Runner restarted with a zero-frame handoff.");
    } else if (was_running) {
        set_status(L"Connections saved. Runner was already using these settings.");
        set_page_message(
            Page::Connections,
            IdConnectionsMessage,
            "Saved to project. Runner was already using these settings. VirtualDJ should use os2l=Auto with os2lDirectIp blank for automatic discovery.");
    } else {
        set_status(L"Connections saved to the project; they will open when the show starts.");
        set_page_message(
            Page::Connections,
            IdConnectionsMessage,
            "Saved to project. Connections open with Start Show. VirtualDJ should use os2l=Auto with os2lDirectIp blank for automatic discovery.");
    }
}

void Application::apply_safety() {
    const auto page = pages_[static_cast<std::size_t>(Page::Safety)];
    auto updated = project_.safety;
    updated.fog_requires_arm =
        Button_GetCheck(::GetDlgItem(page, IdSafetyFogArm)) == BST_CHECKED;
    updated.haze_requires_arm =
        Button_GetCheck(::GetDlgItem(page, IdSafetyHazeArm)) == BST_CHECKED;
    updated.laser_requires_arm =
        Button_GetCheck(::GetDlgItem(page, IdSafetyLaserArm)) == BST_CHECKED;
    updated.spark_requires_arm =
        Button_GetCheck(::GetDlgItem(page, IdSafetySparkArm)) == BST_CHECKED;
    updated.strobe_allowed =
        Button_GetCheck(::GetDlgItem(page, IdSafetyStrobeAllowed)) == BST_CHECKED;
    if (!parse_number(
            control_text(::GetDlgItem(page, IdSafetyMaxStrobe)), updated.max_strobe) ||
        !parse_number(
            control_text(::GetDlgItem(page, IdSafetyMaxIntensity)), updated.max_intensity)) {
        set_page_message(
            Page::Safety,
            IdSafetyMessage,
            "Maximum strobe and intensity must be numbers from zero through one.",
            true);
        return;
    }
    auto candidate = project_;
    candidate.safety = updated;
    const auto validation = emberlights::validate_project(candidate);
    if (!validation.ok()) {
        set_page_message(Page::Safety, IdSafetyMessage, first_validation_error(validation), true);
        return;
    }
    project_ = std::move(candidate);
    mark_dirty();
    set_page_message(
        Page::Safety,
        IdSafetyMessage,
        runner_.status().state == emberlights::RunnerState::Running
            ? "Safety policy saved. Save the project to atomically activate it."
            : "Safety policy saved and will apply when the show starts.");
}

void Application::update_midi_targets() {
    const auto page = pages_[static_cast<std::size_t>(Page::Midi)];
    const auto action = static_cast<showcore::ActionType>(combo_selected_data(
        ::GetDlgItem(page, IdMidiAction),
        static_cast<std::intptr_t>(showcore::ActionType::Blackout)));
    const auto target = ::GetDlgItem(page, IdMidiTarget);
    const auto property = ::GetDlgItem(page, IdMidiProperty);
    static_cast<void>(::SendMessageW(target, CB_RESETCONTENT, 0, 0));
    bool needs_target = false;
    bool needs_property = false;
    if (action == showcore::ActionType::TriggerLook) {
        needs_target = true;
        for (std::size_t index = 0; index < project_.looks.size(); ++index) {
            combo_add(target, widen(project_.looks[index].name), static_cast<std::intptr_t>(index));
        }
    } else if (action == showcore::ActionType::TriggerAutoloop) {
        needs_target = true;
        for (std::size_t index = 0; index < project_.autoloops.size(); ++index) {
            const auto& loop = project_.autoloops[index];
            std::ostringstream label;
            label << "B" << loop.bank + 1U << "/S"
                  << static_cast<unsigned int>(loop.slot + 1U) << " — " << loop.name;
            combo_add(target, widen(label.str()), static_cast<std::intptr_t>(index));
        }
    } else if (action == showcore::ActionType::SelectAutoloopBank ||
               action == showcore::ActionType::SetAutoloopBankEnabled) {
        needs_target = true;
        for (std::uint16_t bank = 0U; bank < showcore::kMaxAutoloopBanks; ++bank) {
            combo_add(target, L"Bank " + std::to_wstring(bank + 1U), bank);
        }
    } else if (action == showcore::ActionType::TriggerTrackScript) {
        needs_target = true;
        for (std::size_t index = 0; index < project_.track_scripts.size(); ++index) {
            const auto& track = project_.track_scripts[index];
            combo_add(target, widen(track.name), static_cast<std::intptr_t>(index));
        }
    } else if (action == showcore::ActionType::SetProperty) {
        needs_target = true;
        needs_property = true;
        for (std::size_t index = 0; index < project_.fixtures.size(); ++index) {
            combo_add(target, widen(project_.fixtures[index].name), static_cast<std::intptr_t>(index));
        }
    } else if (action == showcore::ActionType::SetGroupProperty ||
               action == showcore::ActionType::BlackoutGroup) {
        needs_target = true;
        needs_property = action == showcore::ActionType::SetGroupProperty;
        for (std::size_t index = 0; index < project_.groups.size(); ++index) {
            combo_add(target, widen(project_.groups[index].name), static_cast<std::intptr_t>(index));
        }
    } else {
        combo_add(target, L"Not required", -1);
    }
    if (static_cast<int>(::SendMessageW(target, CB_GETCOUNT, 0, 0)) > 0) {
        static_cast<void>(::SendMessageW(target, CB_SETCURSEL, 0, 0));
    }
    ::EnableWindow(target, needs_target ? TRUE : FALSE);
    ::EnableWindow(property, needs_property ? TRUE : FALSE);
}

void Application::begin_midi_learn() {
    const auto page = pages_[static_cast<std::size_t>(Page::Midi)];
    if (midi_learning_) {
        midi_learning_ = false;
        learn_input_.close_all();
        static_cast<void>(::SetWindowTextW(::GetDlgItem(page, IdMidiLearn),
                                           L"Learn Next MIDI Control"));
        set_page_message(Page::Midi, IdMidiMessage, "MIDI Learn cancelled.");
        return;
    }
    const auto action = static_cast<showcore::ActionType>(combo_selected_data(
        ::GetDlgItem(page, IdMidiAction),
        static_cast<std::intptr_t>(showcore::ActionType::Blackout)));
    const bool needs_target = action == showcore::ActionType::TriggerLook ||
        action == showcore::ActionType::TriggerAutoloop ||
        action == showcore::ActionType::TriggerTrackScript ||
        action == showcore::ActionType::SetProperty ||
        action == showcore::ActionType::SetGroupProperty ||
        action == showcore::ActionType::BlackoutGroup ||
        action == showcore::ActionType::SelectAutoloopBank ||
        action == showcore::ActionType::SetAutoloopBankEnabled;
    if (needs_target && combo_selected_data(::GetDlgItem(page, IdMidiTarget), -1) < 0) {
        set_page_message(Page::Midi, IdMidiMessage,
                         "Create and select the required fixture, Static Look, Autoloop, or track script first.", true);
        return;
    }
    const auto runner_status = runner_.status();
    learn_uses_runner_ = runner_status.state == emberlights::RunnerState::Running;
    if (learn_uses_runner_) {
        if (runner_status.midi_input != emberlights::AdapterState::Ready) {
            set_page_message(Page::Midi, IdMidiMessage,
                             "The running show does not have a ready MIDI input. Check Connections.", true);
            return;
        }
        emberlights::RunnerMidiMonitorEvent stale;
        while (runner_.poll_midi_monitor(stale)) {
        }
    } else {
        if (project_.connections.midi_input_index < 0) {
            set_page_message(Page::Midi, IdMidiMessage,
                             "Select and apply a MIDI input under Connections first.", true);
            return;
        }
        learn_input_.close_all();
        if (!learn_input_.open(
                static_cast<std::uint32_t>(project_.connections.midi_input_index), 0xE11EU)) {
            set_page_message(Page::Midi, IdMidiMessage,
                             "The selected MIDI input could not be opened.", true);
            return;
        }
    }
    midi_learning_ = true;
    static_cast<void>(::SetWindowTextW(::GetDlgItem(page, IdMidiLearn), L"Cancel Learn"));
    set_page_message(Page::Midi, IdMidiMessage,
                     "Move or press the desired control now. Note releases are ignored.");
}

void Application::finish_midi_learn(const showcore::MidiMessage& message) {
    if (message.type == showcore::MidiMessageType::NoteOff ||
        (message.type == showcore::MidiMessageType::NoteOn && message.value == 0U)) {
        return;
    }
    const auto before = project_;
    const auto page = pages_[static_cast<std::size_t>(Page::Midi)];
    emberlights::MidiMappingDefinition mapping;
    mapping.preferred_input_index = project_.connections.midi_input_index;
    for (std::size_t index = 0; index < midi_inputs_.count; ++index) {
        if (static_cast<std::int32_t>(midi_inputs_.ports[index].system_index) ==
            mapping.preferred_input_index) {
            mapping.device_name = std::string(midi_inputs_.ports[index].name());
            break;
        }
    }
    mapping.message_type = message.type;
    mapping.channel = message.channel;
    mapping.number = message.number;
    mapping.input_mode = message.type == showcore::MidiMessageType::PitchBend
        ? showcore::MidiInputMode::Absolute14
        : showcore::MidiInputMode::Absolute7;
    mapping.behavior = static_cast<showcore::MappingBehavior>(combo_selected_data(
        ::GetDlgItem(page, IdMidiBehavior),
        static_cast<std::intptr_t>(showcore::MappingBehavior::Momentary)));
    mapping.action.type = static_cast<showcore::ActionType>(combo_selected_data(
        ::GetDlgItem(page, IdMidiAction),
        static_cast<std::intptr_t>(showcore::ActionType::Blackout)));
    mapping.action.layer = showcore::LayerId::ManualOverride;
    mapping.action.property = static_cast<showcore::Property>(combo_selected_data(
        ::GetDlgItem(page, IdMidiProperty),
        static_cast<std::intptr_t>(showcore::Property::Intensity)));
    mapping.soft_takeover = Button_GetCheck(::GetDlgItem(page, IdMidiSoftTakeover)) == BST_CHECKED;
    const auto target = combo_selected_data(::GetDlgItem(page, IdMidiTarget), -1);
    if (mapping.action.type == showcore::ActionType::TriggerLook && target >= 0 &&
        static_cast<std::size_t>(target) < project_.looks.size()) {
        mapping.target_ref = project_.looks[static_cast<std::size_t>(target)].id;
    } else if (mapping.action.type == showcore::ActionType::TriggerAutoloop && target >= 0 &&
               static_cast<std::size_t>(target) < project_.autoloops.size()) {
        mapping.target_ref = project_.autoloops[static_cast<std::size_t>(target)].id;
    } else if (mapping.action.type == showcore::ActionType::TriggerTrackScript && target >= 0 &&
               static_cast<std::size_t>(target) < project_.track_scripts.size()) {
        mapping.target_ref = project_.track_scripts[static_cast<std::size_t>(target)].id;
    } else if (mapping.action.type == showcore::ActionType::SetProperty && target >= 0 &&
               static_cast<std::size_t>(target) < project_.fixtures.size()) {
        mapping.target_ref = project_.fixtures[static_cast<std::size_t>(target)].id;
    } else if ((mapping.action.type == showcore::ActionType::SetGroupProperty ||
                mapping.action.type == showcore::ActionType::BlackoutGroup) && target >= 0 &&
               static_cast<std::size_t>(target) < project_.groups.size()) {
        mapping.target_ref = project_.groups[static_cast<std::size_t>(target)].id;
    } else if (mapping.action.type == showcore::ActionType::SelectAutoloopBank && target >= 0 &&
               target < static_cast<std::intptr_t>(showcore::kMaxAutoloopBanks)) {
        mapping.action.target_id = static_cast<std::uint16_t>(target);
    } else if (mapping.action.type == showcore::ActionType::SetAutoloopBankEnabled && target >= 0 &&
               target < static_cast<std::intptr_t>(showcore::kMaxAutoloopBanks)) {
        mapping.action.target_id = static_cast<std::uint16_t>(target);
    }
    project_.midi_mappings.push_back(std::move(mapping));
    midi_learning_ = false;
    learn_input_.close_all();
    static_cast<void>(::SetWindowTextW(::GetDlgItem(page, IdMidiLearn),
                                       L"Learn Next MIDI Control"));
    mark_dirty();
    record_project_edit(before);
    refresh_midi();
    set_page_message(
        Page::Midi,
        IdMidiMessage,
        runner_.status().state == emberlights::RunnerState::Running
            ? "Mapping saved. Save the project to atomically activate the newly compiled map."
            : "Mapping learned and saved to the project.");
}

void Application::delete_midi_mapping() {
    const auto page = pages_[static_cast<std::size_t>(Page::Midi)];
    const auto selected = listview_selected_data(::GetDlgItem(page, IdMidiList));
    if (selected < 0 || static_cast<std::size_t>(selected) >= project_.midi_mappings.size()) {
        set_page_message(Page::Midi, IdMidiMessage, "Select a mapping to delete.", true);
        return;
    }
    project_.midi_mappings.erase(project_.midi_mappings.begin() + selected);
    mark_dirty();
    refresh_midi();
    set_page_message(Page::Midi, IdMidiMessage, "MIDI mapping deleted.");
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    const auto com_result = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    std::optional<std::filesystem::path> initial_file;
    bool startup_smoke = false;
    int argument_count = 0;
    auto** arguments = ::CommandLineToArgvW(::GetCommandLineW(), &argument_count);
    if (arguments != nullptr) {
        for (int index = 1; index < argument_count; ++index) {
            if (arguments[index] == nullptr || arguments[index][0] == L'\0') {
                continue;
            }
            if (::lstrcmpiW(arguments[index], L"--startup-smoke") == 0) {
                startup_smoke = true;
            } else if (!initial_file.has_value()) {
                initial_file = std::filesystem::path(arguments[index]);
            }
        }
        ::LocalFree(arguments);
    }
    const auto instance_mutex = ::CreateMutexW(nullptr, FALSE, kInstanceMutex);
    if (instance_mutex != nullptr && ::GetLastError() == ERROR_ALREADY_EXISTS) {
        if (const auto existing = ::FindWindowW(kWindowClass, nullptr); existing != nullptr) {
            if (initial_file.has_value()) {
                const auto project_path = initial_file->wstring();
                COPYDATASTRUCT copy{};
                copy.dwData = kOpenProjectCopyData;
                copy.cbData = static_cast<DWORD>(
                    (project_path.size() + 1U) * sizeof(wchar_t));
                copy.lpData = const_cast<wchar_t*>(project_path.c_str());
                DWORD_PTR ignored = 0;
                static_cast<void>(::SendMessageTimeoutW(
                    existing,
                    WM_COPYDATA,
                    0,
                    reinterpret_cast<LPARAM>(&copy),
                    SMTO_ABORTIFHUNG | SMTO_BLOCK,
                    5000U,
                    &ignored));
            }
            ::ShowWindow(existing, SW_RESTORE);
            static_cast<void>(::SetForegroundWindow(existing));
        } else {
            ::MessageBoxW(
                nullptr,
                L"EmberLights is already running in this Windows session.",
                L"EmberLights",
                MB_OK | MB_ICONINFORMATION);
        }
        ::CloseHandle(instance_mutex);
        return EXIT_SUCCESS;
    }
    Application application(instance);
    const auto result = application.run(
        startup_smoke ? SW_HIDE : show_command,
        initial_file,
        startup_smoke);
    if (instance_mutex != nullptr) {
        ::CloseHandle(instance_mutex);
    }
    if (SUCCEEDED(com_result)) {
        ::CoUninitialize();
    }
    return result;
}
