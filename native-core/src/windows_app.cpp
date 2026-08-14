#include "emberlights/compiler.hpp"
#include "emberlights/audio_assets.hpp"
#include "emberlights/autoloop_autoscript_workflow.hpp"
#include "emberlights/autoloop_fixture_controls.hpp"
#include "emberlights/autoloop_persistence.hpp"
#include "emberlights/connection_layout.hpp"
#include "emberlights/fixture_capabilities.hpp"
#include "emberlights/fixture_controller_binding.hpp"
#include "emberlights/fixture_parameter_catalog.hpp"
#include "emberlights/fixture_profile_editor.hpp"
#include "emberlights/fixture_profile_upgrade.hpp"
#include "emberlights/hardware_qualification.hpp"
#include "emberlights/project.hpp"
#include "emberlights/project_edit_history.hpp"
#include "emberlights/project_io.hpp"
#include "emberlights/qlc_fixture_import.hpp"
#include "emberlights/live_view_model.hpp"
#include "emberlights/migration_portability_review.hpp"
#include "emberlights/ofl_fixture_catalog.hpp"
#include "emberlights/runner.hpp"
#include "emberlights/runner_frame_inspector.hpp"
#include "emberlights/runner_raw_hardware_parity.hpp"
#include "emberlights/static_look_authoring.hpp"
#include "emberlights/static_look_physical_preview.hpp"
#include "emberlights/static_look_preview.hpp"
#include "emberlights/studio_document.hpp"
#include "emberlights/studio_preview.hpp"
#include "emberlights/ui_authoring.hpp"
#include "emberlights/ui_command.hpp"
#include "emberlights/ui_state.hpp"
#include "emberlights/ui_visual.hpp"
#include "emberlights/soundswitch_import.hpp"
#include "emberlights/soundswitch_source_binding.hpp"
#include "emberlights/soundswitch_v1.hpp"
#include "emberlights/version.hpp"
#include "showcore/dmx_usb_pro.hpp"
#include "showcore/number_chars.hpp"
#include "showcore/winmm_midi.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwmapi.h>
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
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
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
constexpr UINT kFixtureCatalogSearchCompleteMessage = WM_APP + 17U;
constexpr UINT kFixtureCatalogDownloadCompleteMessage = WM_APP + 18U;
[[nodiscard]] constexpr COLORREF color_ref(emberlights::UiColor color) noexcept {
    return RGB(color.red, color.green, color.blue);
}

[[nodiscard]] COLORREF theme_color(std::string_view id) noexcept {
    return color_ref(
        emberlights::ember_dark_theme_color(id).value_or(
            emberlights::UiColor{255U, 0U, 255U, 255U}));
}

[[nodiscard]] COLORREF theme_blend(
    std::string_view foreground_id,
    std::string_view background_id) noexcept {
    const auto foreground = emberlights::ember_dark_theme_color(foreground_id).value_or(
        emberlights::UiColor{});
    const auto background = emberlights::ember_dark_theme_color(background_id).value_or(
        emberlights::UiColor{});
    const auto alpha = static_cast<std::uint32_t>(foreground.alpha);
    const auto blend = [alpha](std::uint8_t foreground_channel, std::uint8_t background_channel) {
        return static_cast<std::uint8_t>(
            (static_cast<std::uint32_t>(foreground_channel) * alpha +
             static_cast<std::uint32_t>(background_channel) * (255U - alpha) + 127U) /
            255U);
    };
    return RGB(
        blend(foreground.red, background.red),
        blend(foreground.green, background.green),
        blend(foreground.blue, background.blue));
}

const COLORREF kColorApp = theme_color("color.surface.app");
const COLORREF kColorChrome = theme_color("color.surface.chrome");
const COLORREF kColorPanel = theme_color("color.surface.panel");
const COLORREF kColorPanelRaised = theme_color("color.surface.panelRaised");
const COLORREF kColorControl = theme_color("color.surface.control");
const COLORREF kColorControlHover = theme_color("color.surface.controlHover");
const COLORREF kColorInput = theme_color("color.surface.input");
const COLORREF kColorPrimary = theme_color("color.brand.primary");
const COLORREF kColorPrimaryHover = theme_color("color.brand.primaryHover");
const COLORREF kColorPrimaryPressed = theme_color("color.brand.primaryPressed");
const COLORREF kColorText = theme_color("color.text.primary");
const COLORREF kColorSecondaryText = theme_color("color.text.secondary");
const COLORREF kColorMutedText = theme_color("color.text.muted");
const COLORREF kColorDisabledText = theme_color("color.text.disabled");
const COLORREF kColorBorderSubtle = theme_color("color.border.subtle");
const COLORREF kColorBorder = theme_color("color.border.standard");
const COLORREF kColorFocus = theme_color("color.focus.ring");
const COLORREF kColorSelection = theme_color("color.selection.border");
const COLORREF kColorSelectionFill =
    theme_blend("color.selection.fill", "color.surface.input");
const COLORREF kColorBrandSoft =
    theme_blend("color.brand.soft", "color.surface.input");
const COLORREF kColorDanger = theme_color("color.safety.blackout");
const COLORREF kColorDangerHover = theme_color("color.safety.blackoutActive");
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

void enable_modern_window_frame(HWND window) noexcept {
    constexpr DWORD use_immersive_dark_mode = 20U;
    constexpr DWORD legacy_use_immersive_dark_mode = 19U;
    constexpr DWORD window_corner_preference = 33U;
    constexpr DWORD round_corners = 2U;
    const BOOL enabled = TRUE;
    if (FAILED(::DwmSetWindowAttribute(
            window,
            use_immersive_dark_mode,
            &enabled,
            sizeof(enabled)))) {
        static_cast<void>(::DwmSetWindowAttribute(
            window,
            legacy_use_immersive_dark_mode,
            &enabled,
            sizeof(enabled)));
    }
    static_cast<void>(::DwmSetWindowAttribute(
        window,
        window_corner_preference,
        &round_corners,
        sizeof(round_corners)));
}

enum class Page : std::size_t {
    Live,
    Overrides,
    Profiles,
    Patch,
    Groups,
    Looks,
    Autoloops,
    Autoscript,
    Tracks,
    Midi,
    Connections,
    Safety,
    Diagnostics,
    Count
};

enum class Workspace : std::size_t {
    Live,
    Studio,
    System,
    Count
};

[[nodiscard]] constexpr Workspace page_workspace(Page page) noexcept {
    switch (page) {
    case Page::Live:
    case Page::Overrides:
        return Workspace::Live;
    case Page::Profiles:
    case Page::Patch:
    case Page::Groups:
    case Page::Looks:
    case Page::Autoloops:
    case Page::Autoscript:
    case Page::Tracks:
    case Page::Midi:
        return Workspace::Studio;
    case Page::Connections:
    case Page::Safety:
    case Page::Diagnostics:
    case Page::Count:
        return Workspace::System;
    }
    return Workspace::System;
}

static_assert(page_workspace(Page::Live) == Workspace::Live);
static_assert(page_workspace(Page::Overrides) == Workspace::Live);
static_assert(page_workspace(Page::Profiles) == Workspace::Studio);
static_assert(page_workspace(Page::Midi) == Workspace::Studio);
static_assert(page_workspace(Page::Connections) == Workspace::System);
static_assert(page_workspace(Page::Diagnostics) == Workspace::System);

[[nodiscard]] constexpr bool is_authoring_page(Page page) noexcept {
    return page == Page::Profiles || page == Page::Patch ||
        page == Page::Groups || page == Page::Looks ||
        page == Page::Autoloops || page == Page::Tracks;
}

[[nodiscard]] constexpr emberlights::UiAuthoringResourceKind
authoring_resource_kind(Page page) noexcept {
    switch (page) {
    case Page::Profiles:
        return emberlights::UiAuthoringResourceKind::FixtureProfile;
    case Page::Patch:
        return emberlights::UiAuthoringResourceKind::Fixture;
    case Page::Groups:
        return emberlights::UiAuthoringResourceKind::FixtureGroup;
    case Page::Looks:
        return emberlights::UiAuthoringResourceKind::StaticLook;
    case Page::Autoloops:
        return emberlights::UiAuthoringResourceKind::Autoloop;
    case Page::Tracks:
        return emberlights::UiAuthoringResourceKind::TrackScript;
    default:
        return emberlights::UiAuthoringResourceKind::FixtureProfile;
    }
}

enum ControlId : int {
    IdFileNew = 100,
    IdFileOpen,
    IdFileSave,
    IdFileSaveAs,
    IdFileRestoreHistory,
    IdFileImportSoundSwitch,
    IdFileInspectSoundSwitch,
    IdFileReviewSoundSwitch,
    IdFileCompareSoundSwitch,
    IdFileBundleSoundSwitch,
    IdFileExit,
    IdEditUndo = 120,
    IdEditRedo,
    IdShowValidate = 130,
    IdShowStartStop,
    IdHelpAbout = 150,

    IdWorkspaceLive = 180,
    IdWorkspaceStudio,
    IdWorkspaceSystem,

    IdNavLive = 200,
    IdNavOverrides,
    IdNavProfiles,
    IdNavPatch,
    IdNavGroups,
    IdNavLooks,
    IdNavAutoloops,
    IdNavAutoscript,
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
    IdOverridesSlider,
    IdOverridesZero,
    IdOverridesQuarter,
    IdOverridesHalf,
    IdOverridesFull,
    IdOverridesApply,
    IdOverridesRelease,
    IdOverridesReleaseAll,
    IdOverridesHelp,
    IdOverridesActiveCount,
    IdOverridesMessage,
    IdOverridesNamedLabel,
    IdOverridesNamedChoice,
    IdOverridesApplyNamed,

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
    IdProfileEnsureIr4,
    IdProfileMappingChannel,
    IdProfileMappingProperty,
    IdProfileMappingEncoding,
    IdProfileMappingFine,
    IdProfileMappingMinimum,
    IdProfileMappingMaximum,
    IdProfileMappingDefault,
    IdProfileMappingApply,
    IdProfileMappingDelete,
    IdProfileMappingDefaults,
    IdProfileMappingSummary,
    IdProfileHelp,
    IdProfileMessage,
    IdProfileCatalogTitle,
    IdProfileCatalogQuery,
    IdProfileCatalogSearch,
    IdProfileCatalogResults,
    IdProfileCatalogImport,
    IdProfileCatalogStatus,
    IdProfileTemplate,
    IdProfileApplyTemplate,
    IdProfileCapabilitiesOpen,
    IdProfileChannelWorkbench,

    IdCapabilityTitle = 2200,
    IdCapabilityContext,
    IdCapabilityList,
    IdCapabilityName,
    IdCapabilityProperty,
    IdCapabilityFrom,
    IdCapabilityTo,
    IdCapabilityPreferred,
    IdCapabilityBehavior,
    IdCapabilityAccess,
    IdCapabilityRole,
    IdCapabilityReverse,
    IdCapabilityUpsert,
    IdCapabilityRemove,
    IdCapabilityOwner,
    IdCapabilityBlackout,
    IdCapabilityHighlight,
    IdCapabilitySaveMetadata,
    IdCapabilityNew,
    IdCapabilityClose,
    IdCapabilityMessage,
    IdCapabilityNameLabel,
    IdCapabilityPropertyLabel,
    IdCapabilityFromLabel,
    IdCapabilityToLabel,
    IdCapabilityPreferredLabel,
    IdCapabilityBehaviorLabel,
    IdCapabilityAccessLabel,
    IdCapabilityRoleLabel,
    IdCapabilityOwnerLabel,
    IdCapabilityBlackoutLabel,
    IdCapabilityHighlightLabel,

    IdChannelWorkbenchTitle = 2300,
    IdChannelWorkbenchContext,
    IdChannelWorkbenchList,
    IdChannelWorkbenchNextPropertyLabel,
    IdChannelWorkbenchNextProperty,
    IdChannelWorkbenchAddNext,
    IdChannelWorkbenchFillGaps,
    IdChannelWorkbenchSwapFirstLabel,
    IdChannelWorkbenchSwapFirst,
    IdChannelWorkbenchSwapSecondLabel,
    IdChannelWorkbenchSwapSecond,
    IdChannelWorkbenchSwap,
    IdChannelWorkbenchNamedRanges,
    IdChannelWorkbenchDone,
    IdChannelWorkbenchMessage,

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
    IdLookPhysicalPreview,
    IdLookPhysicalStop,
    IdLookPhysicalStatus,
    IdLookNamedLabel,
    IdLookNamedChoice,
    IdLookApplyNamed,

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
    IdAutoloopLookChoice,
    IdAutoloopStepBeat,
    IdAutoloopStepTransition,
    IdAutoloopAddStep,
    IdAutoloopRemoveLastStep,
    IdAutoloopClearSteps,
    IdAutoloopSteps,
    IdAutoloopHelp,
    IdAutoloopMessage,

    IdAutoscriptTitle = 5250,
    IdAutoscriptIntroduction,
    IdAutoscriptStyle,
    IdAutoscriptComplexity,
    IdAutoscriptTrackBars,
    IdAutoscriptLoopBeats,
    IdAutoscriptGrid,
    IdAutoscriptEnergy,
    IdAutoscriptBank,
    IdAutoscriptSlot,
    IdAutoscriptSeed,
    IdAutoscriptRoles,
    IdAutoscriptGenerate,
    IdAutoscriptPreviewStart,
    IdAutoscriptPreviewMiddle,
    IdAutoscriptCommit,
    IdAutoscriptDiscard,
    IdAutoscriptSummary,
    IdAutoscriptHelp,
    IdAutoscriptMessage,
    IdAutoscriptFunctionPlacement,
    IdAutoscriptFunctionTarget,
    IdAutoscriptFunctionChoice,
    IdAutoscriptFunctionStart,
    IdAutoscriptFunctionEnd,
    IdAutoscriptFunctionPosition,
    IdAutoscriptFunctionApply,

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
    IdMidiNamedChoice,

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
    IdDiagnosticsValidate,

    // Shared IDs are intentionally reused on separate authoring page parents.
    // They describe one skin-facing Authoring Workbench adapter rather than
    // six unrelated Win32-only controls.
    IdAuthoringSearch = 9000,
    IdAuthoringCollectionSummary,
    IdAuthoringInspectorHeading,
    IdAuthoringFind,
    IdAuthoringClearFilter
};

[[nodiscard]] constexpr int authoring_collection_control_id(Page page) noexcept {
    switch (page) {
    case Page::Profiles: return IdProfileList;
    case Page::Patch: return IdPatchList;
    case Page::Groups: return IdGroupList;
    case Page::Looks: return IdLookList;
    case Page::Autoloops: return IdAutoloopList;
    case Page::Tracks: return IdTrackList;
    default: return 0;
    }
}

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
    case IdProfileEnsureIr4:
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
    case IdAutoscriptCommit:
    case IdAutoscriptFunctionApply:
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

void set_multiline_control_text_preserving_view(
    HWND control,
    std::string_view value) {
    DWORD selection_start = 0U;
    DWORD selection_end = 0U;
    static_cast<void>(::SendMessageW(
        control,
        EM_GETSEL,
        reinterpret_cast<WPARAM>(&selection_start),
        reinterpret_cast<LPARAM>(&selection_end)));
    const auto first_visible_line = static_cast<int>(
        ::SendMessageW(control, EM_GETFIRSTVISIBLELINE, 0, 0));
    set_control_text(control, value);
    const auto text_length = static_cast<DWORD>(std::max(
        ::GetWindowTextLengthW(control), 0));
    selection_start = std::min(selection_start, text_length);
    selection_end = std::min(selection_end, text_length);
    static_cast<void>(::SendMessageW(
        control,
        EM_SETSEL,
        static_cast<WPARAM>(selection_start),
        static_cast<LPARAM>(selection_end)));
    const auto new_first_visible_line = static_cast<int>(
        ::SendMessageW(control, EM_GETFIRSTVISIBLELINE, 0, 0));
    static_cast<void>(::SendMessageW(
        control,
        EM_LINESCROLL,
        0,
        static_cast<LPARAM>(first_visible_line - new_first_visible_line)));
}

[[nodiscard]] std::string fixture_parameter_label(
    showcore::Property property,
    bool include_authoring_guidance = false) {
    const auto* descriptor = emberlights::fixture_parameter_descriptor(property);
    if (descriptor == nullptr) {
        return std::string(emberlights::property_name(property));
    }
    std::string label;
    label.reserve(descriptor->display_name.size() + 36U);
    label += emberlights::fixture_parameter_category_name(descriptor->category);
    label += " • ";
    label += descriptor->display_name;
    if (include_authoring_guidance && descriptor->needs_manual_dmx_chart()) {
        label += " • DMX chart range";
    }
    if (include_authoring_guidance && descriptor->safety_restricted()) {
        label += " • safety";
    }
    return label;
}

[[nodiscard]] bool live_override_property_visible(
    showcore::Property property,
    const emberlights::SafetySettings& safety) noexcept {
    return property < showcore::Property::Count &&
        property != showcore::Property::Fog &&
        property != showcore::Property::Haze &&
        property != showcore::Property::Laser &&
        property != showcore::Property::Spark &&
        (property != showcore::Property::Strobe || safety.strobe_allowed);
}

[[nodiscard]] std::string fixture_control_choice_label(
    const emberlights::FixtureControlChoice& choice) {
    std::ostringstream label;
    label << fixture_parameter_label(choice.property);
    if (choice.kind ==
        emberlights::FixtureControlChoiceKind::NamedCapability) {
        label << " • " << choice.name;
    } else {
        label << " • profile channel";
    }
    if (choice.values.size() == 1U) {
        const auto& value = choice.values.front();
        label << " • CH" << value.channel;
        if (value.fine_channel != 0U) {
            label << "+CH" << value.fine_channel;
        }
        label << " " << emberlights::channel_encoding_name(value.encoding);
        if (value.encoding == showcore::ChannelEncoding::Linear16) {
            label << " 0–65535";
        } else {
            label << " DMX " << static_cast<unsigned int>(value.dmx_min)
                  << "–" << static_cast<unsigned int>(value.dmx_max);
        }
        if (choice.kind ==
                emberlights::FixtureControlChoiceKind::NamedCapability &&
            choice.behavior == showcore::ChannelCapabilityBehavior::Slot) {
            label << " → " << static_cast<unsigned int>(value.raw_value);
        }
    } else {
        label << " • " << choice.supported_fixture_count << '/'
              << choice.target_fixture_count << " fixtures";
    }
    if (choice.behavior == showcore::ChannelCapabilityBehavior::Continuous) {
        label << " • range position";
    }
    if (choice.safety_gated()) {
        label << " • safety-gated";
    }
    if (!choice.owner.empty() && choice.owner != "fixture") {
        label << " • " << choice.owner;
    }
    if (!choice.live_override_compatible()) {
        label << " • profile-specific";
    }
    return label.str();
}

[[nodiscard]] std::string_view fixture_control_binding_target(
    std::string_view binding_id) noexcept {
    constexpr std::string_view prefix = "target:";
    constexpr std::string_view delimiter = "|owner:";
    if (!binding_id.starts_with(prefix)) {
        return {};
    }
    const auto end = binding_id.find(delimiter, prefix.size());
    if (end == std::string_view::npos || end == prefix.size()) {
        return {};
    }
    return binding_id.substr(prefix.size(), end - prefix.size());
}

[[nodiscard]] std::string fixture_control_binding_label(
    const emberlights::ProjectDocument& project,
    std::string_view binding_id) {
    const auto authored_target = fixture_control_binding_target(binding_id);
    if (authored_target.empty()) {
        return "Fixture attribute";
    }
    const auto catalog = emberlights::fixture_control_choices(
        project, authored_target);
    const auto choice = std::find_if(
        catalog.choices.begin(), catalog.choices.end(),
        [binding_id](const auto& candidate) {
            return candidate.id == binding_id;
        });
    return choice == catalog.choices.end()
        ? "Fixture attribute"
        : choice->name;
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

[[nodiscard]] const wchar_t* sync_state_name(showcore::SyncState state) noexcept {
    switch (state) {
    case showcore::SyncState::Waiting: return L"Waiting";
    case showcore::SyncState::Os2lHealthy: return L"OS2L locked";
    case showcore::SyncState::PredictiveHold: return L"Predictive hold";
    case showcore::SyncState::AudioFallback: return L"Audio fallback";
    case showcore::SyncState::Recovering: return L"Recovering";
    case showcore::SyncState::Manual: return L"Manual clock";
    case showcore::SyncState::SafeUnsynchronized: return L"Unsynchronized";
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
    void create_health_bar();
    void refresh_navigation();
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
    HWND add_trackbar(HWND parent, int id, int minimum, int maximum);
    void add_listview_column(HWND list, int column, int width, const wchar_t* title);

    void layout();
    void layout_page(Page page, int width, int height);
    void layout_connections();
    void scroll_connections(UINT scroll_code);
    void scroll_connections_wheel(short wheel_delta);
    void reveal_connections_focus();
    void show_page(Page page);
    void show_workspace(Workspace workspace);
    void draw_owner_button(const DRAWITEMSTRUCT& item);
    void draw_page_background(HWND page);
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
    void refresh_override_control_choices();
    void refresh_profiles();
    void refresh_fixture_catalog_controls();
    [[nodiscard]] std::string current_profile_editor_snapshot() const;
    void refresh_patch();
    void refresh_groups();
    void refresh_looks();
    void refresh_look_targets();
    void refresh_look_capabilities();
    void refresh_look_control_choices();
    void refresh_look_draft_view();
    void refresh_physical_preview_status();
    void refresh_autoloops();
    void refresh_autoscript();
    void refresh_autoscript_function_choices();
    void refresh_tracks();
    void refresh_track_audio_assets(std::string_view selected_asset_id);
    void refresh_midi();
    void refresh_connections();
    void refresh_safety();
    void refresh_midi_ports();
    void refresh_diagnostics();
    [[nodiscard]] std::vector<emberlights::UiAuthoringItem>
        authoring_items(Page page) const;
    [[nodiscard]] std::int32_t authoring_selected_index(Page page) const noexcept;
    [[nodiscard]] std::string authoring_selected_id(Page page) const;
    [[nodiscard]] std::string authoring_editor_snapshot(Page page) const;
    [[nodiscard]] bool authoring_editor_changed(Page page) const;
    void capture_authoring_editor_baseline(Page page);
    void refresh_authoring_collection(Page page);
    void refresh_authoring_summary(Page page);
    void restore_authoring_collection_selection(Page page);
    [[nodiscard]] bool confirm_authoring_selection_change(
        Page page,
        std::int32_t next_index);
    void focus_authoring_search();
    void clear_authoring_search();

    void handle_command(int id, int notification, HWND source);
    void handle_notify(const NMHDR& notification);
    void handle_horizontal_scroll(UINT scroll_code, HWND source);
    void handle_timer();

    bool maybe_save_changes();
    void new_project();
    void open_project_dialog();
    void restore_project_history_dialog();
    void import_qlc_fixture_dialog();
    void search_fixture_catalog();
    void import_selected_catalog_fixture();
    void complete_fixture_catalog_search();
    void complete_fixture_catalog_download();
    void import_soundswitch_v1_dialog();
    void inspect_soundswitch_dialog();
    void review_soundswitch_migration_dialog();
    void compare_soundswitch_dialog();
    void bundle_soundswitch_dialog();
    bool open_project(const std::filesystem::path& path);
    bool save_project(bool save_as);
    void validate_project(bool show_success);
    void start_or_stop_show();
    [[nodiscard]] emberlights::UiInvocationResult ui_start_show() noexcept override;
    [[nodiscard]] emberlights::UiInvocationResult ui_stop_show() noexcept override;
    void apply_fixture_override(bool active);
    void apply_named_fixture_override();
    void clear_fixture_overrides();

    void select_profile(std::int32_t index);
    void new_profile();
    void duplicate_profile();
    void save_profile();
    void delete_profile();
    void ensure_ir4_profiles();
    void apply_profile_template();
    void apply_profile_mapping_row();
    void apply_profile_mapping_defaults();
    void delete_profile_mapping_row();
    void refresh_profile_mapping_summary();
    void refresh_profile_channel_table();
    void select_profile_channel(std::int32_t source_index);
    void create_profile_channel_workbench();
    void layout_profile_channel_workbench();
    void open_profile_channel_workbench();
    void refresh_profile_channel_workbench();
    void add_next_profile_channel();
    void fill_profile_channel_gaps();
    void swap_profile_channel_functions();
    void close_profile_channel_workbench();
    void create_profile_capability_window();
    void layout_profile_capability_window();
    void open_profile_capability_editor();
    void refresh_profile_capability_editor();
    void select_profile_capability(std::int32_t source_index);
    void new_profile_capability();
    void upsert_profile_capability();
    void remove_profile_capability();
    void save_profile_channel_metadata();
    void close_profile_capability_editor();
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
    void apply_static_look_control_choice();
    void remove_static_look_property();
    void preview_static_look();
    [[nodiscard]] bool read_static_look_preview_draft(
        emberlights::StaticLookDraft& draft,
        std::string& error_message) const;
    void begin_or_update_physical_static_look_preview();
    void update_physical_static_look_preview_if_active();
    void stop_physical_static_look_preview(bool announce);
    void select_autoloop(std::int32_t index);
    void new_autoloop();
    void duplicate_autoloop();
    void save_autoloop();
    void delete_autoloop();
    void move_autoloop_to_next_empty();
    void swap_autoloop_into_target_slot();
    void add_autoloop_step();
    void remove_last_autoloop_step();
    void clear_autoloop_steps();
    [[nodiscard]] bool autoloop_editor_has_unsaved_content() const;
    void generate_autoscript_proposal();
    void preview_autoscript_phase(double phase);
    void commit_autoscript_proposal();
    void discard_autoscript_proposal();
    void apply_autoscript_fixture_function();
    void refresh_autoscript_summary(std::string_view message = {});
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
    void refresh_midi_named_choices();
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
    HWND brand_label_{nullptr};
    HWND skin_label_{nullptr};
    HFONT normal_font_{nullptr};
    HFONT title_font_{nullptr};
    HFONT section_font_{nullptr};
    HFONT caption_font_{nullptr};
    HFONT icon_font_{nullptr};
    HBRUSH background_brush_{nullptr};
    HBRUSH surface_brush_{nullptr};
    HBRUSH panel_brush_{nullptr};
    HBRUSH field_brush_{nullptr};
    std::array<HWND, 6U> health_badges_{};
    std::array<emberlights::UiStatusTone, 6U> health_badge_tones_{};
    std::array<std::wstring, 6U> health_badge_labels_{};
    HWND health_start_stop_{nullptr};
    HWND health_blackout_{nullptr};
    std::array<HWND, static_cast<std::size_t>(Workspace::Count)> workspace_buttons_{};
    std::array<HWND, static_cast<std::size_t>(Page::Count)> pages_{};
    std::array<HWND, static_cast<std::size_t>(Page::Count)> navigation_{};
    std::array<std::vector<HWND>, static_cast<std::size_t>(Page::Count)> page_controls_{};
    std::array<std::string, static_cast<std::size_t>(Page::Count)>
        authoring_editor_baselines_{};
    Page active_page_{Page::Live};
    Workspace active_workspace_{Workspace::Live};
    Page last_live_page_{Page::Live};
    Page last_studio_page_{Page::Profiles};
    Page last_system_page_{Page::Connections};
    emberlights::UiShellLayout shell_layout_{};
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
    std::vector<emberlights::ChannelDefinition> profile_draft_channels_;
    std::optional<std::string> profile_duplicate_source_id_{};
    HWND profile_channel_workbench_{nullptr};
    HWND profile_capability_window_{nullptr};
    std::uint16_t profile_capability_channel_{0U};
    std::string selected_profile_capability_id_;
    std::int32_t fixture_index_{-1};
    std::int32_t group_index_{-1};
    std::int32_t look_index_{-1};
    std::optional<emberlights::StaticLookDraft> look_draft_{};
    std::vector<emberlights::FixtureControlChoice> look_control_choices_;
    std::vector<emberlights::FixtureControlChoice> override_control_choices_;
    std::int32_t autoloop_index_{-1};
    std::int32_t track_index_{-1};
    std::uint16_t live_autoloop_bank_page_{0U};
    struct LiveAutoloopListItem {
        std::string id;
        std::string name;
        showcore::AutoloopAddress address{};
        bool v2{false};
    };
    std::vector<LiveAutoloopListItem> live_autoloop_items_;
    std::int32_t last_painted_active_look_{-2};
    std::int32_t last_painted_active_track_{-2};
    std::optional<showcore::AutoloopAddress> last_painted_active_autoloop_{};
    emberlights::StudioAutoloopAutoscriptWorkflow autoscript_workflow_;
    std::vector<std::string> autoscript_function_placement_ids_;
    std::vector<std::string> autoscript_function_target_ids_;
    std::vector<emberlights::FixtureControlChoice>
        autoscript_function_choices_;
    std::string autoscript_function_preview_summary_;
    std::vector<emberlights::FixtureControlChoice> midi_named_choices_;

    struct PendingFixtureCatalogDownload {
        emberlights::OpenFixtureLibraryDownloadResult result;
        std::string project_snapshot;
        std::string profile_editor_snapshot;
    };
    std::vector<emberlights::OpenFixtureLibraryEntry> fixture_catalog_results_;
    std::optional<emberlights::OpenFixtureLibrarySearchResult>
        pending_fixture_catalog_search_;
    std::optional<PendingFixtureCatalogDownload>
        pending_fixture_catalog_download_;
    std::mutex fixture_catalog_mutex_;
    std::thread fixture_catalog_worker_;
    bool fixture_catalog_busy_{false};

    emberlights::RunnerService runner_{};
    emberlights::StaticLookPhysicalPreviewService physical_preview_{runner_};
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
    static_cast<void>(physical_preview_.stop());
    runner_.stop();
    if (fixture_catalog_worker_.joinable()) {
        fixture_catalog_worker_.join();
    }
    learn_input_.close_all();
    if (normal_font_ != nullptr) {
        static_cast<void>(::DeleteObject(normal_font_));
    }
    if (title_font_ != nullptr) {
        static_cast<void>(::DeleteObject(title_font_));
    }
    if (section_font_ != nullptr) {
        static_cast<void>(::DeleteObject(section_font_));
    }
    if (caption_font_ != nullptr) {
        static_cast<void>(::DeleteObject(caption_font_));
    }
    if (icon_font_ != nullptr) {
        static_cast<void>(::DeleteObject(icon_font_));
    }
    // background_brush_ is registered as the background brush for our window
    // classes. Win32 owns a class brush until class/process teardown; deleting
    // it here would leave the registered classes holding an invalid HBRUSH.
    if (surface_brush_ != nullptr) {
        static_cast<void>(::DeleteObject(surface_brush_));
    }
    if (panel_brush_ != nullptr) {
        static_cast<void>(::DeleteObject(panel_brush_));
    }
    if (field_brush_ != nullptr) {
        static_cast<void>(::DeleteObject(field_brush_));
    }
}

bool Application::register_classes() {
    background_brush_ = ::CreateSolidBrush(kColorApp);
    surface_brush_ = ::CreateSolidBrush(kColorChrome);
    panel_brush_ = ::CreateSolidBrush(kColorPanel);
    field_brush_ = ::CreateSolidBrush(kColorInput);
    if (background_brush_ == nullptr || surface_brush_ == nullptr ||
        panel_brush_ == nullptr || field_brush_ == nullptr) {
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
    INITCOMMONCONTROLSEX controls{
        sizeof(controls),
        ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES};
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
    std::array<ACCEL, 9> accelerator_definitions{{
        {static_cast<BYTE>(FVIRTKEY | FCONTROL), static_cast<WORD>('N'), IdFileNew},
        {static_cast<BYTE>(FVIRTKEY | FCONTROL), static_cast<WORD>('O'), IdFileOpen},
        {static_cast<BYTE>(FVIRTKEY | FCONTROL), static_cast<WORD>('S'), IdFileSave},
        {static_cast<BYTE>(FVIRTKEY | FCONTROL), static_cast<WORD>('F'), IdAuthoringFind},
        {static_cast<BYTE>(FVIRTKEY | FCONTROL), static_cast<WORD>('Z'), IdEditUndo},
        {static_cast<BYTE>(FVIRTKEY | FCONTROL), static_cast<WORD>('Y'), IdEditRedo},
        {FVIRTKEY, VK_ESCAPE, IdAuthoringClearFilter},
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
        const auto capability_dialog_handled =
            profile_capability_window_ != nullptr &&
            ::IsWindowVisible(profile_capability_window_) != FALSE &&
            ::IsDialogMessageW(profile_capability_window_, &message) != FALSE;
        const auto channel_workbench_dialog_handled =
            profile_channel_workbench_ != nullptr &&
            ::IsWindowVisible(profile_channel_workbench_) != FALSE &&
            ::IsDialogMessageW(profile_channel_workbench_, &message) != FALSE;
        if (!connections_accelerator_handled && !capability_dialog_handled &&
            !channel_workbench_dialog_handled &&
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
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable Text");
    title_font_ = ::CreateFontW(
        -26, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable Display");
    section_font_ = ::CreateFontW(
        -18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable Text");
    caption_font_ = ::CreateFontW(
        -12, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable Text");
    icon_font_ = ::CreateFontW(
        -20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, SYMBOL_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe Fluent Icons");
    if (icon_font_ != nullptr) {
        const auto device = ::GetDC(nullptr);
        if (device != nullptr) {
            const auto previous = ::SelectObject(device, icon_font_);
            std::array<wchar_t, LF_FACESIZE> selected_face{};
            const auto selected_length = ::GetTextFaceW(
                device,
                static_cast<int>(selected_face.size()),
                selected_face.data());
            if (previous != nullptr) {
                static_cast<void>(::SelectObject(device, previous));
            }
            static_cast<void>(::ReleaseDC(nullptr, device));
            if (selected_length <= 0 ||
                ::lstrcmpiW(selected_face.data(), L"Segoe Fluent Icons") != 0) {
                static_cast<void>(::DeleteObject(icon_font_));
                icon_font_ = ::CreateFontW(
                    -20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, SYMBOL_CHARSET,
                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                    DEFAULT_PITCH | FF_DONTCARE, L"Segoe MDL2 Assets");
            }
        }
    }
    if (normal_font_ == nullptr || title_font_ == nullptr ||
        section_font_ == nullptr || caption_font_ == nullptr ||
        icon_font_ == nullptr) {
        return false;
    }
    window_ = ::CreateWindowExW(
        0,
        kWindowClass,
        L"EmberLights",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1440,
        900,
        nullptr,
        nullptr,
        instance_,
        this);
    if (window_ == nullptr) {
        return false;
    }
    enable_modern_window_frame(window_);
    create_menu_bar();
    create_navigation();
    create_health_bar();
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
    if (window_ == nullptr || status_bar_ == nullptr || brand_label_ == nullptr ||
        skin_label_ == nullptr || health_start_stop_ == nullptr ||
        health_blackout_ == nullptr ||
        std::any_of(
            health_badges_.begin(),
            health_badges_.end(),
            [](HWND badge) { return badge == nullptr; }) ||
        std::any_of(
            workspace_buttons_.begin(),
            workspace_buttons_.end(),
            [](HWND button) { return button == nullptr; })) {
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
    constexpr std::array<std::pair<Page, int>, 16> critical_controls{{
        {Page::Live, IdLiveStartStop},
        {Page::Overrides, IdOverridesApply},
        {Page::Profiles, IdProfileList},
        {Page::Profiles, IdProfileChannelWorkbench},
        {Page::Patch, IdPatchList},
        {Page::Groups, IdGroupList},
        {Page::Looks, IdLookList},
        {Page::Autoloops, IdAutoloopList},
        {Page::Autoscript, IdAutoscriptGenerate},
        {Page::Autoscript, IdAutoscriptFunctionApply},
        {Page::Tracks, IdTrackList},
        {Page::Midi, IdMidiList},
        {Page::Midi, IdMidiNamedChoice},
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
    if (application != nullptr &&
        window == application->profile_capability_window_) {
        if (message == WM_SIZE) {
            application->layout_profile_capability_window();
            return 0;
        }
        if (message == WM_CLOSE) {
            application->close_profile_capability_editor();
            return 0;
        }
        if (message == WM_GETMINMAXINFO) {
            auto* limits = reinterpret_cast<MINMAXINFO*>(lparam);
            limits->ptMinTrackSize.x = 860;
            limits->ptMinTrackSize.y = 620;
            return 0;
        }
    }
    if (application != nullptr &&
        window == application->profile_channel_workbench_) {
        if (message == WM_SIZE) {
            application->layout_profile_channel_workbench();
            return 0;
        }
        if (message == WM_CLOSE) {
            application->close_profile_channel_workbench();
            return 0;
        }
        if (message == WM_GETMINMAXINFO) {
            auto* limits = reinterpret_cast<MINMAXINFO*>(lparam);
            limits->ptMinTrackSize.x = 920;
            limits->ptMinTrackSize.y = 600;
            return 0;
        }
    }
    if (application != nullptr && message == WM_ERASEBKGND) {
        return TRUE;
    }
    if (application != nullptr && message == WM_PAINT) {
        application->draw_page_background(window);
        return 0;
    }
    if (message == WM_COMMAND || message == WM_NOTIFY || message == WM_HSCROLL ||
        message == WM_DRAWITEM ||
        message == WM_CTLCOLORSTATIC || message == WM_CTLCOLOREDIT ||
        message == WM_CTLCOLORLISTBOX || message == WM_CTLCOLORBTN) {
        return ::SendMessageW(
            application != nullptr &&
                    (window == application->profile_capability_window_ ||
                     window == application->profile_channel_workbench_)
                ? application->window_
                : ::GetParent(window),
            message,
            wparam,
            lparam);
    }
    return ::DefWindowProcW(window, message, wparam, lparam);
}

LRESULT Application::handle_message(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case kFixtureCatalogSearchCompleteMessage:
        complete_fixture_catalog_search();
        return 0;
    case kFixtureCatalogDownloadCompleteMessage:
        complete_fixture_catalog_download();
        return 0;
    case WM_GETMINMAXINFO: {
        auto* limits = reinterpret_cast<MINMAXINFO*>(lparam);
        limits->ptMinTrackSize.x = 1200;
        limits->ptMinTrackSize.y = 820;
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
    case WM_HSCROLL:
        handle_horizontal_scroll(
            LOWORD(wparam), reinterpret_cast<HWND>(lparam));
        return 0;
    case WM_DRAWITEM:
        if (lparam != 0) {
            draw_owner_button(*reinterpret_cast<const DRAWITEMSTRUCT*>(lparam));
            return TRUE;
        }
        return FALSE;
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
        const auto control = reinterpret_cast<HWND>(lparam);
        ::SetTextColor(device, control == skin_label_ ? kColorSecondaryText : kColorText);
        if (control == brand_label_ || control == skin_label_ || control == status_bar_) {
            return reinterpret_cast<LRESULT>(surface_brush_);
        }
        for (std::size_t index = 0U; index < pages_.size(); ++index) {
            if (::GetParent(control) != pages_[index]) {
                continue;
            }
            const auto title = !page_controls_[index].empty() &&
                page_controls_[index].front() == control;
            return reinterpret_cast<LRESULT>(
                title ? background_brush_ : panel_brush_);
        }
        return reinterpret_cast<LRESULT>(background_brush_);
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        const auto device = reinterpret_cast<HDC>(wparam);
        ::SetTextColor(device, kColorText);
        ::SetBkColor(device, kColorInput);
        return reinterpret_cast<LRESULT>(field_brush_);
    }
    case WM_CTLCOLORBTN: {
        const auto device = reinterpret_cast<HDC>(wparam);
        ::SetTextColor(device, kColorText);
        ::SetBkColor(device, kColorChrome);
        return reinterpret_cast<LRESULT>(surface_brush_);
    }
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        const auto device = ::BeginPaint(window, &paint);
        RECT client{};
        static_cast<void>(::GetClientRect(window, &client));
        static_cast<void>(::FillRect(device, &client, background_brush_));
        const auto shell = emberlights::compute_ui_shell_layout(
            client.right - client.left, client.bottom - client.top);
        const RECT navigation_panel{
            shell.navigation.x,
            shell.navigation.y,
            shell.navigation.right(),
            shell.navigation.bottom()};
        static_cast<void>(::FillRect(device, &navigation_panel, surface_brush_));
        const RECT health_panel{
            shell.health_bar.x,
            shell.health_bar.y,
            shell.health_bar.right(),
            shell.health_bar.bottom()};
        static_cast<void>(::FillRect(device, &health_panel, surface_brush_));
        RECT accent{
            shell.navigation.right() - 2,
            0,
            shell.navigation.right(),
            shell.navigation.bottom()};
        const auto accent_brush = ::CreateSolidBrush(kColorPrimary);
        if (accent_brush != nullptr) {
            static_cast<void>(::FillRect(device, &accent, accent_brush));
            static_cast<void>(::DeleteObject(accent_brush));
        }
        RECT health_separator{
            shell.health_bar.x,
            shell.health_bar.bottom() - 1,
            shell.health_bar.right(),
            shell.health_bar.bottom()};
        const auto separator_brush = ::CreateSolidBrush(kColorBorderSubtle);
        if (separator_brush != nullptr) {
            static_cast<void>(::FillRect(device, &health_separator, separator_brush));
            static_cast<void>(::DeleteObject(separator_brush));
        }
        ::EndPaint(window, &paint);
        return 0;
    }
    case WM_CLOSE:
        if (maybe_save_changes()) {
            static_cast<void>(physical_preview_.stop());
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
        file, MF_STRING, IdFileImportSoundSwitch, L"Import SoundSwitch Project (&V1 Preview)..."));
    static_cast<void>(::AppendMenuW(
        file, MF_STRING, IdFileInspectSoundSwitch, L"Inspect SoundSwitch &Source..."));
    static_cast<void>(::AppendMenuW(
        file, MF_STRING, IdFileReviewSoundSwitch, L"Review Current SoundSwitch &Migration..."));
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
        if (::lstrcmpiW(class_name, L"EDIT") == 0) {
            static_cast<void>(::SendMessageW(
                control, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(8, 8)));
        }
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
    return add_control(parent, L"EDIT", L"", style | WS_BORDER, 0, id);
}

HWND Application::add_button(HWND parent, const wchar_t* text, int id, DWORD extra_style) {
    constexpr DWORD button_type_mask = 0x0FU;
    const auto button_type = extra_style & button_type_mask;
    const auto checkable = button_type == BS_AUTOCHECKBOX || button_type == BS_CHECKBOX;
    const auto style = checkable
        ? extra_style
        : ((extra_style & ~button_type_mask) | BS_OWNERDRAW);
    return add_control(parent, L"BUTTON", text, WS_TABSTOP | style, 0, id);
}

HWND Application::add_combo(HWND parent, int id) {
    return add_control(
        parent, L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 0, id);
}

HWND Application::add_listbox(HWND parent, int id) {
    const auto list = add_control(
        parent,
        L"LISTBOX",
        L"",
        WS_TABSTOP | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWFIXED |
            LBS_HASSTRINGS | WS_VSCROLL | WS_BORDER,
        0, id);
    if (list != nullptr) {
        static_cast<void>(::SendMessageW(list, LB_SETITEMHEIGHT, 0, 42));
    }
    return list;
}

HWND Application::add_listview(HWND parent, int id) {
    const auto list = add_control(
        parent,
        WC_LISTVIEWW,
        L"",
        WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        0,
        id);
    if (list != nullptr) {
        ListView_SetExtendedListViewStyle(
            list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        ListView_SetBkColor(list, kColorInput);
        ListView_SetTextBkColor(list, kColorInput);
        ListView_SetTextColor(list, kColorText);
    }
    return list;
}

HWND Application::add_trackbar(
    HWND parent,
    int id,
    int minimum,
    int maximum) {
    const auto trackbar = add_control(
        parent,
        TRACKBAR_CLASSW,
        L"",
        WS_TABSTOP | TBS_HORZ | TBS_AUTOTICKS | TBS_TOOLTIPS,
        0,
        id);
    if (trackbar != nullptr) {
        static_cast<void>(::SendMessageW(
            trackbar, TBM_SETRANGE, TRUE, MAKELPARAM(minimum, maximum)));
        static_cast<void>(::SendMessageW(trackbar, TBM_SETTICFREQ, 25, 0));
    }
    return trackbar;
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
    brand_label_ = add_label(window_, L"EMBERLIGHTS", 0);
    ::SendMessageW(brand_label_, WM_SETFONT, reinterpret_cast<WPARAM>(title_font_), TRUE);
    skin_label_ = add_label(window_, L"DEFAULT 2.2 • EMBER DARK", 0);
    constexpr std::array<const wchar_t*, static_cast<std::size_t>(Workspace::Count)>
        workspace_names{{L"LIVE", L"STUDIO", L"SETUP"}};
    for (std::size_t index = 0; index < workspace_buttons_.size(); ++index) {
        workspace_buttons_[index] = add_button(
            window_, workspace_names[index], IdWorkspaceLive + static_cast<int>(index));
    }
    const auto visuals = emberlights::ui_navigation_visuals();
    for (std::size_t index = 0; index < navigation_.size(); ++index) {
        const auto name = index < visuals.size()
            ? widen(visuals[index].accessible_label)
            : std::wstring{L"Unknown"};
        navigation_[index] = add_button(
            window_, name.c_str(), IdNavLive + static_cast<int>(index), BS_LEFT);
    }
}

void Application::create_health_bar() {
    constexpr std::array<const wchar_t*, 6U> initial{{
        L"PROJECT\nLoading",
        L"RUNNER\nStopped",
        L"SYNC\nWaiting",
        L"OUTPUTS\nOff",
        L"OVERRIDES\nNone",
        L"SAFETY\nDisarmed"}};
    for (std::size_t index = 0U; index < health_badges_.size(); ++index) {
        health_badges_[index] = add_control(
            window_, L"STATIC", initial[index], SS_OWNERDRAW, 0, 0);
        health_badge_tones_[index] = emberlights::UiStatusTone::Neutral;
        health_badge_labels_[index] = initial[index];
    }
    health_start_stop_ = add_button(window_, L"Start Show", IdShowStartStop);
    health_blackout_ = add_button(window_, L"BLACKOUT", IdLiveBlackout);
}

void Application::refresh_navigation() {
    int visible_index = 0;
    const auto navigation_width = shell_layout_.navigation.width > 0
        ? shell_layout_.navigation.width
        : 244;
    for (std::size_t index = 0; index < navigation_.size(); ++index) {
        const auto visible = page_workspace(static_cast<Page>(index)) == active_workspace_;
        ::ShowWindow(navigation_[index], visible ? SW_SHOW : SW_HIDE);
        if (visible) {
            ::MoveWindow(
                navigation_[index],
                14,
                138 + visible_index * 44,
                navigation_width - 28,
                38,
                TRUE);
            ++visible_index;
        }
        static_cast<void>(::InvalidateRect(navigation_[index], nullptr, TRUE));
    }
    for (const auto button : workspace_buttons_) {
        static_cast<void>(::InvalidateRect(button, nullptr, TRUE));
    }
}

void Application::draw_owner_button(const DRAWITEMSTRUCT& item) {
    if (item.hwndItem == nullptr || item.hDC == nullptr) {
        return;
    }

    if (item.CtlType == ODT_STATIC) {
        const auto found = std::find(
            health_badges_.begin(), health_badges_.end(), item.hwndItem);
        if (found == health_badges_.end()) {
            return;
        }
        const auto index = static_cast<std::size_t>(found - health_badges_.begin());
        const auto tone = color_ref(
            emberlights::ui_status_tone_color(health_badge_tones_[index]));
        static_cast<void>(::FillRect(item.hDC, &item.rcItem, surface_brush_));
        auto rectangle = item.rcItem;
        static_cast<void>(::InflateRect(&rectangle, -1, -1));
        const auto brush = ::CreateSolidBrush(kColorPanelRaised);
        const auto pen = ::CreatePen(PS_SOLID, 1, tone);
        const auto previous_brush = brush == nullptr
            ? nullptr
            : ::SelectObject(item.hDC, brush);
        const auto previous_pen = pen == nullptr
            ? nullptr
            : ::SelectObject(item.hDC, pen);
        static_cast<void>(::RoundRect(
            item.hDC,
            rectangle.left,
            rectangle.top,
            rectangle.right,
            rectangle.bottom,
            10,
            10));
        if (previous_brush != nullptr) {
            static_cast<void>(::SelectObject(item.hDC, previous_brush));
        }
        if (previous_pen != nullptr) {
            static_cast<void>(::SelectObject(item.hDC, previous_pen));
        }
        if (brush != nullptr) {
            static_cast<void>(::DeleteObject(brush));
        }
        if (pen != nullptr) {
            static_cast<void>(::DeleteObject(pen));
        }
        RECT accent{
            rectangle.left + 1,
            rectangle.top + 5,
            rectangle.left + 4,
            rectangle.bottom - 5};
        const auto accent_brush = ::CreateSolidBrush(tone);
        if (accent_brush != nullptr) {
            static_cast<void>(::FillRect(item.hDC, &accent, accent_brush));
            static_cast<void>(::DeleteObject(accent_brush));
        }

        std::array<wchar_t, 256> caption{};
        static_cast<void>(::GetWindowTextW(
            item.hwndItem, caption.data(), static_cast<int>(caption.size())));
        const std::wstring_view text{caption.data()};
        const auto newline = text.find(L'\n');
        const auto heading = text.substr(0U, newline);
        const auto value = newline == std::wstring_view::npos
            ? std::wstring_view{}
            : text.substr(newline + 1U);
        ::SetBkMode(item.hDC, TRANSPARENT);
        RECT heading_rectangle{
            rectangle.left + 12,
            rectangle.top + 5,
            rectangle.right - 7,
            rectangle.top + 19};
        const auto previous_font = ::SelectObject(item.hDC, caption_font_);
        ::SetTextColor(item.hDC, tone);
        static_cast<void>(::DrawTextW(
            item.hDC,
            heading.data(),
            static_cast<int>(heading.size()),
            &heading_rectangle,
            DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX));
        static_cast<void>(::SelectObject(item.hDC, normal_font_));
        ::SetTextColor(item.hDC, kColorText);
        RECT value_rectangle{
            rectangle.left + 12,
            rectangle.top + 19,
            rectangle.right - 7,
            rectangle.bottom - 3};
        static_cast<void>(::DrawTextW(
            item.hDC,
            value.data(),
            static_cast<int>(value.size()),
            &value_rectangle,
            DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX));
        if (previous_font != nullptr) {
            static_cast<void>(::SelectObject(item.hDC, previous_font));
        }
        return;
    }

    if (item.CtlType == ODT_LISTBOX) {
        const auto selected = (item.itemState & ODS_SELECTED) != 0U;
        const auto disabled = (item.itemState & ODS_DISABLED) != 0U;
        const auto id = ::GetDlgCtrlID(item.hwndItem);
        bool active = false;
        if (item.itemID != static_cast<UINT>(-1) &&
            (id == IdLiveLooks || id == IdLiveTracks || id == IdLiveAutoloops)) {
            const auto data = static_cast<std::size_t>(item.itemData);
            const auto status = runner_.status();
            if (id == IdLiveLooks && status.active_look >= 0) {
                active = data == static_cast<std::size_t>(status.active_look);
            } else if (id == IdLiveTracks && status.active_track_script >= 0) {
                active = data == static_cast<std::size_t>(status.active_track_script);
            } else if (id == IdLiveAutoloops && data < live_autoloop_items_.size()) {
                active = live_autoloop_items_[data].address == status.active_autoloop;
            }
        }
        const auto fill = active ? kColorBrandSoft
            : selected ? kColorSelectionFill
            : kColorInput;
        const auto brush = ::CreateSolidBrush(fill);
        if (brush != nullptr) {
            static_cast<void>(::FillRect(item.hDC, &item.rcItem, brush));
            static_cast<void>(::DeleteObject(brush));
        }
        if (active || selected) {
            RECT accent{item.rcItem.left, item.rcItem.top, item.rcItem.left + 4, item.rcItem.bottom};
            const auto accent_brush = ::CreateSolidBrush(
                active ? kColorPrimary : kColorSelection);
            if (accent_brush != nullptr) {
                static_cast<void>(::FillRect(item.hDC, &accent, accent_brush));
                static_cast<void>(::DeleteObject(accent_brush));
            }
        }
        if (item.itemID != static_cast<UINT>(-1)) {
            const auto length = static_cast<int>(::SendMessageW(
                item.hwndItem, LB_GETTEXTLEN, item.itemID, 0));
            if (length >= 0) {
                std::wstring caption(static_cast<std::size_t>(length) + 1U, L'\0');
                const auto copied = static_cast<int>(::SendMessageW(
                    item.hwndItem,
                    LB_GETTEXT,
                    item.itemID,
                    reinterpret_cast<LPARAM>(caption.data())));
                caption.resize(copied > 0 ? static_cast<std::size_t>(copied) : 0U);
                const auto previous_font = ::SelectObject(item.hDC, normal_font_);
                ::SetBkMode(item.hDC, TRANSPARENT);
                ::SetTextColor(
                    item.hDC,
                    disabled ? kColorDisabledText
                    : active ? kColorPrimaryHover
                    : kColorText);
                auto text_rectangle = item.rcItem;
                text_rectangle.left += active ? 13 : 11;
                text_rectangle.right -= 8;
                static_cast<void>(::DrawTextW(
                    item.hDC,
                    caption.c_str(),
                    -1,
                    &text_rectangle,
                    DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX));
                if (previous_font != nullptr) {
                    static_cast<void>(::SelectObject(item.hDC, previous_font));
                }
            }
        }
        const auto separator_pen = ::CreatePen(PS_SOLID, 1, kColorBorderSubtle);
        if (separator_pen != nullptr) {
            const auto previous_pen = ::SelectObject(item.hDC, separator_pen);
            static_cast<void>(::MoveToEx(
                item.hDC, item.rcItem.left + 8, item.rcItem.bottom - 1, nullptr));
            static_cast<void>(::LineTo(
                item.hDC, item.rcItem.right - 8, item.rcItem.bottom - 1));
            static_cast<void>(::SelectObject(item.hDC, previous_pen));
            static_cast<void>(::DeleteObject(separator_pen));
        }
        if ((item.itemState & ODS_FOCUS) != 0U) {
            auto focus = item.rcItem;
            static_cast<void>(::InflateRect(&focus, -3, -3));
            const auto focus_pen = ::CreatePen(PS_SOLID, 1, kColorFocus);
            const auto previous_brush = ::SelectObject(
                item.hDC, ::GetStockObject(HOLLOW_BRUSH));
            const auto previous_pen = focus_pen == nullptr
                ? nullptr
                : ::SelectObject(item.hDC, focus_pen);
            static_cast<void>(::Rectangle(
                item.hDC, focus.left, focus.top, focus.right, focus.bottom));
            if (previous_pen != nullptr) {
                static_cast<void>(::SelectObject(item.hDC, previous_pen));
            }
            static_cast<void>(::SelectObject(item.hDC, previous_brush));
            if (focus_pen != nullptr) {
                static_cast<void>(::DeleteObject(focus_pen));
            }
        }
        return;
    }

    if (item.CtlType != ODT_BUTTON) {
        return;
    }
    const auto id = ::GetDlgCtrlID(item.hwndItem);
    const auto workspace_button = id >= IdWorkspaceLive && id <= IdWorkspaceSystem;
    const auto navigation_button = id >= IdNavLive && id <= IdNavDiagnostics;
    const auto workspace_selected = workspace_button &&
        static_cast<std::size_t>(id - IdWorkspaceLive) ==
            static_cast<std::size_t>(active_workspace_);
    const auto navigation_selected = navigation_button &&
        static_cast<std::size_t>(id - IdNavLive) == static_cast<std::size_t>(active_page_);
    const auto pressed = (item.itemState & ODS_SELECTED) != 0U;
    const auto hot = (item.itemState & ODS_HOTLIGHT) != 0U;
    const auto disabled = (item.itemState & ODS_DISABLED) != 0U;

    const auto emergency = id == IdLiveBlackout;
    const auto danger = id == IdOverridesReleaseAll ||
        id == IdProfileDelete || id == IdPatchDelete || id == IdGroupDelete ||
        id == IdLookDelete || id == IdAutoloopDelete || id == IdTrackDelete ||
        id == IdMidiDelete || id == IdLookPhysicalStop ||
        id == IdCapabilityRemove;
    const auto primary = id == IdShowStartStop || id == IdLiveStartStop ||
        id == IdLiveTriggerLook ||
        id == IdLiveTriggerAutoloop || id == IdLiveTriggerTrack ||
        id == IdOverridesApply || id == IdProfileSave || id == IdPatchSave ||
        id == IdGroupSave || id == IdLookSave || id == IdLookApplyColor ||
        id == IdLookPhysicalPreview || id == IdProfileCatalogSearch ||
        id == IdProfileCatalogImport || id == IdCapabilityUpsert ||
        id == IdAutoloopSave || id == IdAutoscriptGenerate ||
        id == IdAutoscriptCommit || id == IdAutoscriptFunctionApply ||
        id == IdTrackSave ||
        id == IdConnectionsApply || id == IdSafetyApply;

    COLORREF fill = kColorControl;
    COLORREF border = kColorBorder;
    COLORREF text = kColorText;
    if (workspace_selected) {
        fill = pressed ? kColorPrimaryPressed : kColorPrimary;
        border = kColorPrimaryHover;
        text = kColorApp;
    } else if (navigation_selected) {
        fill = kColorPanelRaised;
        border = kColorPanelRaised;
        text = kColorPrimaryHover;
    } else if (navigation_button) {
        fill = pressed || hot ? kColorControl : kColorChrome;
        border = fill;
    } else if (emergency) {
        fill = pressed || hot ? kColorDangerHover : kColorDanger;
        border = kColorDangerHover;
    } else if (danger) {
        fill = pressed || hot ? kColorDanger : kColorControl;
        border = kColorDanger;
        text = pressed || hot ? kColorText : kColorDangerHover;
    } else if (primary) {
        fill = pressed ? kColorPrimaryPressed
            : hot ? kColorPrimaryHover
            : kColorPrimary;
        border = kColorPrimaryHover;
        text = kColorApp;
    } else if (pressed || hot) {
        fill = kColorControlHover;
        border = kColorBorder;
    }

    switch (id) {
    case IdLookSwatchRed: fill = RGB(202, 55, 63); break;
    case IdLookSwatchGreen: fill = RGB(60, 169, 103); text = kColorApp; break;
    case IdLookSwatchBlue: fill = RGB(58, 116, 210); break;
    case IdLookSwatchWhite: fill = RGB(238, 238, 225); text = kColorApp; break;
    case IdLookSwatchAmber: fill = RGB(227, 158, 43); text = kColorApp; break;
    case IdLookSwatchUv: fill = RGB(112, 70, 190); break;
    case IdLookSwatchBlack: fill = RGB(8, 9, 10); border = RGB(92, 100, 106); break;
    default: break;
    }
    if (disabled) {
        fill = kColorPanel;
        border = kColorBorderSubtle;
        text = kColorDisabledText;
    }

    const auto parent = ::GetParent(item.hwndItem);
    const auto canvas = parent == window_ ? kColorChrome : kColorPanel;
    const auto canvas_brush = ::CreateSolidBrush(canvas);
    if (canvas_brush != nullptr) {
        static_cast<void>(::FillRect(item.hDC, &item.rcItem, canvas_brush));
        static_cast<void>(::DeleteObject(canvas_brush));
    }
    auto rectangle = item.rcItem;
    static_cast<void>(::InflateRect(&rectangle, -1, -1));
    const auto brush = ::CreateSolidBrush(fill);
    const auto pen = ::CreatePen(PS_SOLID, 1, border);
    const auto previous_brush = brush == nullptr
        ? nullptr
        : ::SelectObject(item.hDC, brush);
    const auto previous_pen = pen == nullptr
        ? nullptr
        : ::SelectObject(item.hDC, pen);
    static_cast<void>(::RoundRect(
        item.hDC,
        rectangle.left,
        rectangle.top,
        rectangle.right,
        rectangle.bottom,
        12,
        12));
    if (previous_brush != nullptr) {
        static_cast<void>(::SelectObject(item.hDC, previous_brush));
    }
    if (previous_pen != nullptr) {
        static_cast<void>(::SelectObject(item.hDC, previous_pen));
    }
    if (brush != nullptr) {
        static_cast<void>(::DeleteObject(brush));
    }
    if (pen != nullptr) {
        static_cast<void>(::DeleteObject(pen));
    }
    if (navigation_selected) {
        RECT selection{
            rectangle.left + 1,
            rectangle.top + 7,
            rectangle.left + 4,
            rectangle.bottom - 7};
        const auto selection_brush = ::CreateSolidBrush(kColorPrimary);
        if (selection_brush != nullptr) {
            static_cast<void>(::FillRect(item.hDC, &selection, selection_brush));
            static_cast<void>(::DeleteObject(selection_brush));
        }
    }

    std::array<wchar_t, 256> caption{};
    static_cast<void>(::GetWindowTextW(
        item.hwndItem, caption.data(), static_cast<int>(caption.size())));
    const auto previous_font = ::SelectObject(item.hDC, normal_font_);
    ::SetBkMode(item.hDC, TRANSPARENT);
    ::SetTextColor(item.hDC, text);
    RECT text_rectangle = item.rcItem;
    static_cast<void>(::InflateRect(&text_rectangle, -12, 0));
    auto flags = DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS;
    if (navigation_button) {
        const auto index = static_cast<std::size_t>(id - IdNavLive);
        const auto visuals = emberlights::ui_navigation_visuals();
        if (index < visuals.size()) {
            const wchar_t glyph[]{
                static_cast<wchar_t>(visuals[index].fluent_glyph),
                L'\0'};
            RECT icon_rectangle{
                item.rcItem.left + 10,
                item.rcItem.top,
                item.rcItem.left + 38,
                item.rcItem.bottom};
            static_cast<void>(::SelectObject(item.hDC, icon_font_));
            ::SetTextColor(
                item.hDC, navigation_selected ? kColorPrimaryHover : kColorMutedText);
            static_cast<void>(::DrawTextW(
                item.hDC,
                glyph,
                1,
                &icon_rectangle,
                DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX));
            static_cast<void>(::SelectObject(item.hDC, normal_font_));
            ::SetTextColor(item.hDC, text);
            text_rectangle.left += 30;
        }
        flags |= DT_LEFT;
    } else {
        flags |= DT_CENTER;
    }
    flags |= DT_NOPREFIX;
    static_cast<void>(::DrawTextW(
        item.hDC, caption.data(), -1, &text_rectangle, flags));
    if (previous_font != nullptr) {
        static_cast<void>(::SelectObject(item.hDC, previous_font));
    }
    if ((item.itemState & ODS_FOCUS) != 0U) {
        RECT focus = item.rcItem;
        static_cast<void>(::InflateRect(&focus, -4, -4));
        const auto focus_pen = ::CreatePen(PS_SOLID, 2, kColorFocus);
        const auto previous_focus_brush = ::SelectObject(
            item.hDC, ::GetStockObject(HOLLOW_BRUSH));
        const auto previous_focus_pen = focus_pen == nullptr
            ? nullptr
            : ::SelectObject(item.hDC, focus_pen);
        static_cast<void>(::RoundRect(
            item.hDC,
            focus.left,
            focus.top,
            focus.right,
            focus.bottom,
            8,
            8));
        if (previous_focus_pen != nullptr) {
            static_cast<void>(::SelectObject(item.hDC, previous_focus_pen));
        }
        static_cast<void>(::SelectObject(item.hDC, previous_focus_brush));
        if (focus_pen != nullptr) {
            static_cast<void>(::DeleteObject(focus_pen));
        }
    }
}

void Application::draw_page_background(HWND page) {
    PAINTSTRUCT paint{};
    const auto device = ::BeginPaint(page, &paint);
    if (device == nullptr) {
        return;
    }
    RECT client{};
    static_cast<void>(::GetClientRect(page, &client));
    static_cast<void>(::FillRect(device, &client, background_brush_));
    const auto draw_panel = [&](const emberlights::UiRectangle& panel) {
        if (!panel.has_area()) {
            return;
        }
        RECT card{panel.x, panel.y, panel.right(), panel.bottom()};
        const auto brush = ::CreateSolidBrush(kColorPanel);
        const auto pen = ::CreatePen(PS_SOLID, 1, kColorBorderSubtle);
        const auto previous_brush = brush == nullptr
            ? nullptr
            : ::SelectObject(device, brush);
        const auto previous_pen = pen == nullptr
            ? nullptr
            : ::SelectObject(device, pen);
        static_cast<void>(::RoundRect(
            device, card.left, card.top, card.right, card.bottom, 14, 14));
        if (previous_brush != nullptr) {
            static_cast<void>(::SelectObject(device, previous_brush));
        }
        if (previous_pen != nullptr) {
            static_cast<void>(::SelectObject(device, previous_pen));
        }
        if (brush != nullptr) {
            static_cast<void>(::DeleteObject(brush));
        }
        if (pen != nullptr) {
            static_cast<void>(::DeleteObject(pen));
        }
    };
    const auto page_iterator = std::find(pages_.begin(), pages_.end(), page);
    if (page_iterator != pages_.end()) {
        const auto page_kind = static_cast<Page>(page_iterator - pages_.begin());
        if (is_authoring_page(page_kind)) {
            const auto layout = emberlights::compute_authoring_workbench_layout(
                client.right - client.left,
                client.bottom - client.top,
                shell_layout_.density,
                page_kind == Page::Patch
                    ? emberlights::UiAuthoringCollectionEmphasis::Wide
                    : emberlights::UiAuthoringCollectionEmphasis::Standard);
            draw_panel(layout.library_panel);
            draw_panel(layout.inspector_panel);
            RECT title_accent{24, 54, 92, 57};
            const auto accent_brush = ::CreateSolidBrush(kColorPrimary);
            if (accent_brush != nullptr) {
                static_cast<void>(::FillRect(device, &title_accent, accent_brush));
                static_cast<void>(::DeleteObject(accent_brush));
            }
            ::EndPaint(page, &paint);
            return;
        }
    }
    if (client.right > 32 && client.bottom > 76) {
        RECT card{12, 58, client.right - 12, client.bottom - 10};
        const auto brush = ::CreateSolidBrush(kColorPanel);
        const auto pen = ::CreatePen(PS_SOLID, 1, kColorBorderSubtle);
        const auto previous_brush = brush == nullptr
            ? nullptr
            : ::SelectObject(device, brush);
        const auto previous_pen = pen == nullptr
            ? nullptr
            : ::SelectObject(device, pen);
        static_cast<void>(::RoundRect(
            device, card.left, card.top, card.right, card.bottom, 14, 14));
        if (previous_brush != nullptr) {
            static_cast<void>(::SelectObject(device, previous_brush));
        }
        if (previous_pen != nullptr) {
            static_cast<void>(::SelectObject(device, previous_pen));
        }
        if (brush != nullptr) {
            static_cast<void>(::DeleteObject(brush));
        }
        if (pen != nullptr) {
            static_cast<void>(::DeleteObject(pen));
        }
        RECT title_accent{24, 54, 76, 57};
        const auto accent_brush = ::CreateSolidBrush(kColorPrimary);
        if (accent_brush != nullptr) {
            static_cast<void>(::FillRect(device, &title_accent, accent_brush));
            static_cast<void>(::DeleteObject(accent_brush));
        }
    }
    ::EndPaint(page, &paint);
}

void Application::create_pages() {
    // Page construction is split into sections below to keep each workspace focused.
    for (std::size_t index = 0; index < pages_.size(); ++index) {
        static_cast<void>(create_page(static_cast<Page>(index)));
    }

    auto page = pages_[static_cast<std::size_t>(Page::Live)];
    auto title = add_label(page, L"LIVE • Performance Console", IdLiveTitle);
    ::SendMessageW(title, WM_SETFONT, reinterpret_cast<WPARAM>(title_font_), TRUE);
    add_label(page, L"Stopped", IdLiveState);
    add_button(page, L"Start Show", IdLiveStartStop);
    add_button(page, L"BLACKOUT", IdLiveBlackout);
    add_button(page, L"Work Light", IdLiveWorkLight);
    add_label(page, L"Manual BPM", 0);
    add_edit(page, IdLiveBpm);
    add_button(page, L"Apply", IdLiveApplyBpm);
    add_button(page, L"Tap", IdLiveTap);
    auto section = add_label(page, L"Static Looks", 0);
    ::SendMessageW(section, WM_SETFONT, reinterpret_cast<WPARAM>(section_font_), TRUE);
    add_listbox(page, IdLiveLooks);
    add_button(page, L"Launch / Toggle", IdLiveTriggerLook);
    add_button(page, L"Clear Look", IdLiveClearLook);
    section = add_label(page, L"Autoloops", 0);
    ::SendMessageW(section, WM_SETFONT, reinterpret_cast<WPARAM>(section_font_), TRUE);
    add_listbox(page, IdLiveAutoloops);
    add_button(page, L"Launch", IdLiveTriggerAutoloop);
    add_button(page, L"Previous", IdLivePreviousAutoloop);
    add_button(page, L"Next", IdLiveNextAutoloop);
    add_button(page, L"Clear Loop", IdLiveClearAutoloop);
    section = add_label(page, L"Track Scripts", IdLiveTrackLabel);
    ::SendMessageW(section, WM_SETFONT, reinterpret_cast<WPARAM>(section_font_), TRUE);
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
    title = add_label(page, L"LIVE • Fixture Overrides", IdOverridesTitle);
    ::SendMessageW(title, WM_SETFONT, reinterpret_cast<WPARAM>(title_font_), TRUE);
    add_label(
        page,
        L"Immediate fixture-attribute controls. Overrides are transient, sit above Looks and Autoloops, "
        L"and remain subject to the Runner's safety limits. They never edit the project.",
        IdOverridesHelp);
    add_label(page, L"Active fixture or group", 0);
    add_listbox(page, IdOverridesFixture);
    add_label(page, L"Advanced semantic fallback", 0);
    add_combo(page, IdOverridesProperty);
    add_label(page, L"Level / range position (0–100)", 0);
    add_edit(page, IdOverridesValue);
    add_button(page, L"Apply Override", IdOverridesApply);
    add_button(page, L"Release Attribute", IdOverridesRelease);
    add_button(page, L"Release All Overrides", IdOverridesReleaseAll);
    add_label(page, L"", IdOverridesActiveCount);
    add_label(page, L"", IdOverridesMessage);
    add_trackbar(page, IdOverridesSlider, 0, 100);
    add_button(page, L"0%", IdOverridesZero);
    add_button(page, L"25%", IdOverridesQuarter);
    add_button(page, L"50%", IdOverridesHalf);
    add_button(page, L"100%", IdOverridesFull);
    add_label(page, L"Fixture Attribute • from active profile", IdOverridesNamedLabel);
    add_combo(page, IdOverridesNamedChoice);
    add_button(page, L"Apply Fixture Attribute", IdOverridesApplyNamed);

    page = pages_[static_cast<std::size_t>(Page::Profiles)];
    title = add_label(page, L"STUDIO • Fixture Profiles", IdProfileTitle);
    ::SendMessageW(title, WM_SETFONT, reinterpret_cast<WPARAM>(title_font_), TRUE);
    add_listbox(page, IdProfileList);
    add_button(page, L"Import Fixture File (.qxf)...", IdProfileImportQlc);
    add_button(page, L"New", IdProfileNew);
    add_button(page, L"Duplicate to Edit", IdProfileDuplicate);
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
    add_label(page, L"Channel map • select a row to edit", 0);
    auto profile_channels = add_listview(page, IdProfileChannels);
    add_listview_column(profile_channels, 0, 54, L"CH");
    add_listview_column(profile_channels, 1, 132, L"Attribute / function");
    add_listview_column(profile_channels, 2, 112, L"Encoding");
    add_listview_column(profile_channels, 3, 76, L"DMX range");
    add_listview_column(profile_channels, 4, 64, L"Default");
    add_listview_column(profile_channels, 5, 60, L"Fine");
    add_listview_column(profile_channels, 6, 86, L"Owner");
    add_listview_column(profile_channels, 7, 84, L"Ranges");
    add_button(page, L"Restore Verified IR-4 6CH + 10CH", IdProfileEnsureIr4);
    add_label(page, L"Channel", 0);
    add_edit(page, IdProfileMappingChannel);
    add_label(page, L"Semantic attribute", 0);
    add_combo(page, IdProfileMappingProperty);
    add_label(page, L"Encoding", 0);
    add_combo(page, IdProfileMappingEncoding);
    add_label(page, L"Fine", 0);
    add_edit(page, IdProfileMappingFine);
    add_label(page, L"Min", 0);
    add_edit(page, IdProfileMappingMinimum);
    add_label(page, L"Max", 0);
    add_edit(page, IdProfileMappingMaximum);
    add_label(page, L"Default", 0);
    add_edit(page, IdProfileMappingDefault);
    add_button(page, L"Add / Replace Channel", IdProfileMappingApply);
    add_button(page, L"Apply Safe Defaults", IdProfileMappingDefaults);
    add_edit(page, IdProfileMappingSummary, true, true);
    add_label(
        page,
        L"Select a row, choose a semantic parameter, then use safe defaults or enter the exact DMX-chart range. "
        L"For shutter/strobe, gobos, programs, resets, or multi-function channels, open Named DMX ranges—no parameter IDs to type. "
        L"White and Amber are ordinary Color parameters; map each to the channel observed on the fixture. Imported snapshots stay read-only until duplicated.",
        IdProfileHelp);
    add_label(page, L"", IdProfileMessage);
    add_button(page, L"Remove Channel", IdProfileMappingDelete);
    add_label(
        page,
        L"SEARCH OFFICIAL OPEN FIXTURE LIBRARY",
        IdProfileCatalogTitle);
    add_edit(page, IdProfileCatalogQuery);
    add_button(page, L"Search", IdProfileCatalogSearch);
    add_listbox(page, IdProfileCatalogResults);
    add_button(page, L"Download + Import Selected", IdProfileCatalogImport);
    add_label(
        page,
        L"Official catalog snapshots stay read-only and unreviewed until you verify the DMX chart and hardware.",
        IdProfileCatalogStatus);
    add_label(page, L"Quick template", 0);
    add_combo(page, IdProfileTemplate);
    add_button(page, L"Replace Channel Map", IdProfileApplyTemplate);
    add_button(page, L"Named DMX ranges…", IdProfileCapabilitiesOpen);
    add_button(
        page,
        L"Open Channel Map Workbench…",
        IdProfileChannelWorkbench);

    page = pages_[static_cast<std::size_t>(Page::Patch)];
    title = add_label(page, L"STUDIO • Fixture Patch", IdPatchTitle);
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
    title = add_label(page, L"STUDIO • Fixture Groups", IdGroupTitle);
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
    title = add_label(page, L"STUDIO • Static Looks", IdLookTitle);
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
    add_label(page, L"Advanced semantic fallback", 0);
    add_combo(page, IdLookProperty);
    add_label(page, L"Ownership", 0);
    add_combo(page, IdLookOwnership);
    add_label(page, L"Level / range position %", 0);
    add_edit(page, IdLookValue);
    add_button(page, L"Apply Attribute", IdLookApplyProperty);
    add_button(page, L"Remove from Look", IdLookRemoveProperty);
    add_label(page, L"Included fixture attributes", 0);
    add_edit(page, IdLookAssignments, true, true);
    add_button(page, L"Build Exact Offline DMX Preview", IdLookPreview);
    add_edit(page, IdLookPreviewText, true, true);
    add_label(
        page,
        L"A target not listed is not included and keeps following lower content. An included "
        L"target set to Hard Off stays dark. Full Color owns every supported RGBWAUV emitter, "
        L"including zero, opens a real master, and forces strobe off. W/A/UV remain direct.",
        IdLookHelp);
    add_label(page, L"", IdLookMessage);
    add_button(
        page,
        L"Preview Selected Target on Fixtures",
        IdLookPhysicalPreview);
    add_button(page, L"STOP PREVIEW", IdLookPhysicalStop);
    add_label(
        page,
        L"PHYSICAL PREVIEW OFF • Live must be stopped",
        IdLookPhysicalStatus);
    add_label(page, L"Fixture Attribute • profile-backed", IdLookNamedLabel);
    add_combo(page, IdLookNamedChoice);
    add_button(page, L"Use Fixture Attribute", IdLookApplyNamed);

    page = pages_[static_cast<std::size_t>(Page::Autoloops)];
    title = add_label(page, L"STUDIO • Autoloops", IdAutoloopTitle);
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
    add_label(page, L"Quick step builder", 0);
    add_combo(page, IdAutoloopLookChoice);
    add_label(page, L"Beat", 0);
    add_edit(page, IdAutoloopStepBeat);
    add_combo(page, IdAutoloopStepTransition);
    add_button(page, L"Add Step", IdAutoloopAddStep);
    add_label(page, L"Steps", 0);
    add_edit(page, IdAutoloopSteps, true);
    add_label(
        page,
        L"Choose a saved Static Look, set its beat, and add it. Advanced: edit rows as "
        L"beat, look-id, cut|linear. The first step must be beat 0.",
        IdAutoloopHelp);
    add_label(page, L"", IdAutoloopMessage);
    add_button(page, L"Remove Last Step", IdAutoloopRemoveLastStep);
    add_button(page, L"Clear Draft Steps", IdAutoloopClearSteps);

    page = pages_[static_cast<std::size_t>(Page::Autoscript)];
    title = add_label(page, L"STUDIO • AutoScript", IdAutoscriptTitle);
    ::SendMessageW(title, WM_SETFONT, reinterpret_cast<WPARAM>(title_font_), TRUE);
    add_label(
        page,
        L"Generate one deterministic V2 Autoloop from musical intent. EmberLights first "
        L"compiles an immutable proposal through the production renderer with every output "
        L"adapter disabled; nothing enters the project until you explicitly commit it.",
        IdAutoscriptIntroduction);
    add_label(page, L"Style", 0);
    add_combo(page, IdAutoscriptStyle);
    add_label(page, L"Complexity", 0);
    add_combo(page, IdAutoscriptComplexity);
    add_label(page, L"Track bars", 0);
    add_edit(page, IdAutoscriptTrackBars);
    add_label(page, L"Loop beats", 0);
    add_edit(page, IdAutoscriptLoopBeats);
    add_label(page, L"Grid", 0);
    add_combo(page, IdAutoscriptGrid);
    add_label(page, L"Energy %", 0);
    add_edit(page, IdAutoscriptEnergy);
    add_label(page, L"First bank", 0);
    add_edit(page, IdAutoscriptBank);
    add_label(page, L"First slot", 0);
    add_edit(page, IdAutoscriptSlot);
    add_label(page, L"Seed", 0);
    add_edit(page, IdAutoscriptSeed);
    add_label(page, L"Fixture roles (comma-separated; blank = all)", 0);
    add_edit(page, IdAutoscriptRoles);
    add_button(page, L"Generate + Offline Preview", IdAutoscriptGenerate, BS_DEFPUSHBUTTON);
    add_button(page, L"Preview Start", IdAutoscriptPreviewStart);
    add_button(page, L"Preview Middle", IdAutoscriptPreviewMiddle);
    add_button(page, L"Commit to Project", IdAutoscriptCommit);
    add_button(page, L"Discard", IdAutoscriptDiscard);
    add_edit(page, IdAutoscriptSummary, true, true);
    add_label(
        page,
        L"Use a stable seed to reproduce the same content. Commit is one Undo transaction. "
        L"Save and reopen normally; Live will list persisted V2 placements by bank and slot.",
        IdAutoscriptHelp);
    add_label(page, L"", IdAutoscriptMessage);
    add_label(page, L"Edit a committed V2 loop with an exact fixture-profile function", 0);
    add_combo(page, IdAutoscriptFunctionPlacement);
    add_label(page, L"Target", 0);
    add_combo(page, IdAutoscriptFunctionTarget);
    add_label(page, L"Fixture Attribute", 0);
    add_combo(page, IdAutoscriptFunctionChoice);
    add_label(page, L"Start beat", 0);
    add_edit(page, IdAutoscriptFunctionStart);
    add_label(page, L"End beat", 0);
    add_edit(page, IdAutoscriptFunctionEnd);
    add_label(page, L"Range %", 0);
    add_edit(page, IdAutoscriptFunctionPosition);
    add_button(
        page,
        L"Preview + Add Function",
        IdAutoscriptFunctionApply);

    page = pages_[static_cast<std::size_t>(Page::Tracks)];
    title = add_label(page, L"STUDIO • Track Scripts", IdTrackTitle);
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
    add_label(page, L"Advanced semantic attribute", 0);
    add_combo(page, IdMidiProperty);
    add_label(page, L"Behavior", 0);
    add_combo(page, IdMidiBehavior);
    add_button(page, L"Soft takeover", IdMidiSoftTakeover, BS_AUTOCHECKBOX);
    add_button(page, L"Learn Next MIDI Control", IdMidiLearn);
    add_button(page, L"Delete Mapping", IdMidiDelete);
    add_label(page, L"", IdMidiMessage);
    add_label(page, L"Fixture Attribute • profile-backed (recommended)", 0);
    add_combo(page, IdMidiNamedChoice);

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

    for (const auto authoring_page : {
             Page::Profiles,
             Page::Patch,
             Page::Groups,
             Page::Looks,
             Page::Autoloops,
             Page::Tracks}) {
        const auto authoring_parent =
            pages_[static_cast<std::size_t>(authoring_page)];
        const auto descriptor = emberlights::authoring_resource_descriptor(
            authoring_resource_kind(authoring_page));
        const auto search = add_edit(authoring_parent, IdAuthoringSearch);
        static_cast<void>(::SendMessageW(
            search,
            EM_SETLIMITTEXT,
            emberlights::kUiAuthoringMaximumQueryBytes,
            0));
        const auto hint = widen(descriptor.search_hint);
        static_cast<void>(::SendMessageW(
            search,
            EM_SETCUEBANNER,
            TRUE,
            reinterpret_cast<LPARAM>(hint.c_str())));
        const auto summary = add_label(
            authoring_parent,
            L"Search by name, metadata, or stable ID",
            IdAuthoringCollectionSummary);
        ::SendMessageW(
            summary,
            WM_SETFONT,
            reinterpret_cast<WPARAM>(caption_font_),
            TRUE);
        const auto inspector = add_label(
            authoring_parent,
            L"INSPECTOR • New draft",
            IdAuthoringInspectorHeading);
        ::SendMessageW(
            inspector,
            WM_SETFONT,
            reinterpret_cast<WPARAM>(section_font_),
            TRUE);
    }
}

void Application::layout() {
    if (window_ == nullptr || pages_[0] == nullptr) {
        return;
    }
    RECT client{};
    static_cast<void>(::GetClientRect(window_, &client));
    const auto width = std::max(640L, client.right - client.left);
    const auto height = std::max(480L, client.bottom - client.top);
    shell_layout_ = emberlights::compute_ui_shell_layout(
        static_cast<std::int32_t>(width), static_cast<std::int32_t>(height));
    const auto navigation_width = shell_layout_.navigation.width;

    ::MoveWindow(brand_label_, 16, 12, navigation_width - 32, 36, TRUE);
    ::MoveWindow(skin_label_, 16, 46, navigation_width - 32, 22, TRUE);
    const auto workspace_width = (navigation_width - 36) / 3;
    for (std::size_t index = 0; index < workspace_buttons_.size(); ++index) {
        ::MoveWindow(
            workspace_buttons_[index],
            14 + static_cast<int>(index) * (workspace_width + 4),
            82,
            workspace_width,
            36,
            TRUE);
    }
    refresh_navigation();

    const auto& health = shell_layout_.health_bar;
    constexpr int health_gap = 6;
    constexpr int health_padding = 12;
    const auto action_height = std::max(36, health.height - 18);
    const auto action_y = health.y + (health.height - action_height) / 2;
    const auto start_width = shell_layout_.density == emberlights::UiShellDensity::Compact
        ? 100
        : 112;
    const auto blackout_width = shell_layout_.density == emberlights::UiShellDensity::Compact
        ? 120
        : 132;
    const auto blackout_x = health.right() - health_padding - blackout_width;
    const auto start_x = blackout_x - health_gap - start_width;
    const auto badges_x = health.x + health_padding;
    const auto badges_width = std::max(
        360, start_x - health_gap - badges_x);
    const auto project_width = std::clamp(badges_width / 5, 126, 196);
    const auto other_width = std::max(
        72,
        (badges_width - project_width -
         health_gap * static_cast<int>(health_badges_.size() - 1U)) /
            static_cast<int>(health_badges_.size() - 1U));
    auto badge_x = badges_x;
    for (std::size_t index = 0U; index < health_badges_.size(); ++index) {
        const auto badge_width = index == 0U ? project_width : other_width;
        ::MoveWindow(
            health_badges_[index],
            badge_x,
            action_y,
            badge_width,
            action_height,
            TRUE);
        badge_x += badge_width + health_gap;
    }
    ::MoveWindow(
        health_start_stop_, start_x, action_y, start_width, action_height, TRUE);
    ::MoveWindow(
        health_blackout_,
        blackout_x,
        action_y,
        blackout_width,
        action_height,
        TRUE);

    for (std::size_t index = 0; index < pages_.size(); ++index) {
        ::MoveWindow(
            pages_[index],
            shell_layout_.page.x,
            shell_layout_.page.y,
            shell_layout_.page.width,
            shell_layout_.page.height,
            TRUE);
        layout_page(
            static_cast<Page>(index),
            shell_layout_.page.width,
            shell_layout_.page.height);
        for (const auto control : page_controls_[index]) {
            std::array<wchar_t, 32> class_name{};
            if (::GetClassNameW(
                    control,
                    class_name.data(),
                    static_cast<int>(class_name.size())) > 0 &&
                ::lstrcmpiW(class_name.data(), L"LISTBOX") == 0) {
                static_cast<void>(::SendMessageW(
                    control,
                    LB_SETITEMHEIGHT,
                    0,
                    shell_layout_.list_row_height));
            }
        }
    }
    ::MoveWindow(
        status_bar_,
        shell_layout_.status_bar.x + 14,
        shell_layout_.status_bar.y,
        shell_layout_.status_bar.width - 28,
        shell_layout_.status_bar.height,
        TRUE);
    static_cast<void>(::InvalidateRect(window_, nullptr, TRUE));
}

void Application::layout_page(Page page, int width, int height) {
    auto& controls = page_controls_[static_cast<std::size_t>(page)];
    auto move = [&](std::size_t index, int x, int y, int w, int h) {
        if (index < controls.size()) {
            ::MoveWindow(controls[index], x, y, std::max(1, w), std::max(1, h), TRUE);
        }
    };
    const auto move_control = [&](int id, const emberlights::UiRectangle& area) {
        const auto control = ::GetDlgItem(
            pages_[static_cast<std::size_t>(page)], id);
        if (control != nullptr && area.has_area()) {
            ::MoveWindow(
                control,
                area.x,
                area.y,
                area.width,
                area.height,
                TRUE);
        }
    };
    constexpr int margin = 24;
    const auto usable_width = std::max(400, width - margin * 2);
    move(0, margin, 18, usable_width, 40);
    std::optional<emberlights::UiAuthoringWorkbenchLayout> authoring_layout;
    if (is_authoring_page(page)) {
        authoring_layout = emberlights::compute_authoring_workbench_layout(
            width,
            height,
            shell_layout_.density,
            page == Page::Patch
                ? emberlights::UiAuthoringCollectionEmphasis::Wide
                : emberlights::UiAuthoringCollectionEmphasis::Standard);
        move_control(IdAuthoringSearch, authoring_layout->search);
        move_control(
            IdAuthoringCollectionSummary,
            authoring_layout->collection_summary);
        move_control(
            IdAuthoringInspectorHeading,
            authoring_layout->inspector_heading);
    }

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
        move(18, x, 196, form_width, 26);
        move(19, x, 224, std::max(120, form_width - 160), 250);
        move(20, x + std::max(128, form_width - 152), 222, 152, 32);
        move(6, x, 266, 220, 26);
        move(7, x, 294, 80, 27);
        move(13, x + 90, 290, std::max(140, form_width - 90), 34);
        constexpr int quick_width = 68;
        move(14, x, 334, quick_width, 30);
        move(15, x + quick_width + 8, 334, quick_width, 30);
        move(16, x + (quick_width + 8) * 2, 334, quick_width, 30);
        move(17, x + (quick_width + 8) * 3, 334, quick_width, 30);
        move(8, x, 376, 150, 32);
        move(9, x + 162, 376, 150, 32);
        move(10, x, 418, 190, 32);
        move(11, x, 460, form_width, 28);
        move(12, x, 494, form_width, 58);
        break;
    }
    case Page::Profiles: {
        const auto& workbench = *authoring_layout;
        const auto& collection = workbench.collection;
        const auto profile_list_height = std::max(
            104,
            std::min(collection.height * 34 / 100, collection.height - 238));
        move(1, collection.x, collection.y, collection.width, profile_list_height);
        auto catalog_y = collection.y + profile_list_height + 8;
        move(2, collection.x, catalog_y, collection.width, 32);
        catalog_y += 38;
        move(40, collection.x, catalog_y, collection.width, 22);
        catalog_y += 24;
        move(41, collection.x, catalog_y, std::max(100, collection.width - 96), 28);
        move(42, collection.right() - 88, catalog_y, 88, 28);
        catalog_y += 34;
        const auto catalog_status_height = 36;
        const auto catalog_import_height = 32;
        const auto catalog_results_height = std::max(
            58,
            collection.bottom() - catalog_y - catalog_import_height -
                catalog_status_height - 12);
        move(43, collection.x, catalog_y, collection.width, catalog_results_height);
        catalog_y += catalog_results_height + 6;
        move(44, collection.x, catalog_y, collection.width, catalog_import_height);
        move(45, collection.x, catalog_y + catalog_import_height + 4,
             collection.width, catalog_status_height);
        const auto library_half = (workbench.library_actions.width - 8) / 2;
        move(3, workbench.library_actions.x, workbench.library_actions.y,
             library_half, 34);
        move(4, workbench.library_actions.x + library_half + 8,
             workbench.library_actions.y, library_half, 34);

        const auto inspector_action_width = std::min(
            170, (workbench.inspector_actions.width - 8) / 2);
        move(5, workbench.inspector_actions.x, workbench.inspector_actions.y,
             inspector_action_width, 34);
        move(6, workbench.inspector_actions.right() - inspector_action_width,
             workbench.inspector_actions.y, inspector_action_width, 34);

        const auto& content = workbench.inspector_content;
        const auto x = content.x;
        const auto form_width = content.width;
        const auto top = content.y;
        constexpr int label_width = 120;
        constexpr int row_height = 32;
        for (std::size_t row = 0; row < 5U; ++row) {
            const auto base = 7U + row * 2U;
            move(base, x, top + static_cast<int>(row) * row_height, label_width, 26);
            move(base + 1U, x + label_width,
                 top + static_cast<int>(row) * row_height,
                 form_width - label_width, 27);
        }
        const auto toolbar_y = top + row_height * 5 + 4;
        const auto toolbar_width = std::max(92, (form_width - 18) / 4);
        move(19, x, toolbar_y, toolbar_width, 32);
        move(46, x + toolbar_width + 6, toolbar_y + 4, 84, 24);
        move(47, x + toolbar_width + 90, toolbar_y,
             std::max(80, form_width - toolbar_width * 2 - 108), 200);
        move(48, x + form_width - toolbar_width, toolbar_y, toolbar_width, 32);

        const auto table_heading_y = toolbar_y + 40;
        move(17, x, table_heading_y, std::max(140, form_width - 230), 24);
        move(50, x + std::max(148, form_width - 220), table_heading_y - 4,
             std::min(220, form_width), 32);
        const auto table_y = table_heading_y + 26;
        const auto mapping_block_height = 164;
        const auto table_height = std::max(
            74, content.bottom() - table_y - mapping_block_height);
        move(18, x, table_y, form_width, table_height);

        const auto mapping_y = table_y + table_height + 6;
        move(20, x, mapping_y + 2, 62, 24);
        move(21, x + 62, mapping_y, 68, 27);
        move(22, x + 142, mapping_y + 2, 72, 24);
        move(23, x + 214, mapping_y, std::max(130, form_width - 214), 200);

        move(24, x, mapping_y + 34, 72, 24);
        move(25, x + 72, mapping_y + 32,
             std::min(170, std::max(112, form_width / 3)), 200);
        const auto fine_x = x + std::min(252, std::max(194, form_width / 3 + 82));
        move(26, fine_x, mapping_y + 34, 42, 24);
        move(27, fine_x + 42, mapping_y + 32, 68, 27);

        const auto compact_field = 58;
        move(28, x, mapping_y + 68, 34, 24);
        move(29, x + 34, mapping_y + 66, compact_field, 27);
        move(30, x + 102, mapping_y + 68, 36, 24);
        move(31, x + 138, mapping_y + 66, compact_field, 27);
        move(32, x + 206, mapping_y + 68, 58, 24);
        move(33, x + 264, mapping_y + 66, compact_field + 12, 27);
        const auto mapping_action_width = std::max(104, (form_width - 24) / 4);
        move(34, x, mapping_y + 100, mapping_action_width, 32);
        move(35, x + mapping_action_width + 8, mapping_y + 100,
             mapping_action_width, 32);
        move(49, x + (mapping_action_width + 8) * 2, mapping_y + 100,
             mapping_action_width, 32);
        move(39, x + (mapping_action_width + 8) * 3, mapping_y + 100,
             mapping_action_width, 32);
        move(36, x, mapping_y + 136, form_width,
             std::max(24, content.bottom() - mapping_y - 136));
        ::ShowWindow(controls[37], SW_HIDE);
        move(38, x, workbench.inspector_actions.y - 30, form_width, 24);
        break;
    }
    case Page::Patch: {
        const auto& workbench = *authoring_layout;
        move(1,
             workbench.collection.x,
             workbench.collection.y,
             workbench.collection.width,
             workbench.collection.height);
        move(2,
             workbench.library_actions.x,
             workbench.library_actions.y,
             std::min(150, workbench.library_actions.width),
             34);
        const auto action_width = std::min(
            170, (workbench.inspector_actions.width - 8) / 2);
        move(3,
             workbench.inspector_actions.x,
             workbench.inspector_actions.y,
             action_width,
             34);
        move(4,
             workbench.inspector_actions.right() - action_width,
             workbench.inspector_actions.y,
             action_width,
             34);
        const auto& content = workbench.inspector_content;
        constexpr int label_width = 106;
        move(5, content.x, content.y, label_width, 26);
        move(6, content.x + label_width, content.y,
             content.width - label_width, 27);
        move(7, content.x, content.y + 42, label_width, 26);
        move(8, content.x + label_width, content.y + 42,
             content.width - label_width, 200);
        const auto half = (content.width - 16) / 2;
        move(9, content.x, content.y + 86, 72, 26);
        move(10, content.x + 72, content.y + 84,
             std::max(60, half - 72), 200);
        move(11, content.x + half + 16, content.y + 86, 62, 26);
        move(12, content.x + half + 78, content.y + 84,
             std::max(60, half - 62), 27);
        move(13, content.x, content.y + 128, content.width, 26);
        move(14, content.x, content.y + 156, content.width,
             std::max(120, content.height - 202));
        move(15, content.x, content.bottom() - 36, content.width, 32);
        break;
    }
    case Page::Groups: {
        const auto& workbench = *authoring_layout;
        move(1,
             workbench.collection.x,
             workbench.collection.y,
             workbench.collection.width,
             workbench.collection.height);
        const auto library_half = (workbench.library_actions.width - 8) / 2;
        move(2, workbench.library_actions.x, workbench.library_actions.y,
             library_half, 34);
        move(3, workbench.library_actions.x + library_half + 8,
             workbench.library_actions.y, library_half, 34);
        const auto action_width = std::min(
            170, (workbench.inspector_actions.width - 8) / 2);
        move(4, workbench.inspector_actions.x, workbench.inspector_actions.y,
             action_width, 34);
        move(5, workbench.inspector_actions.right() - action_width,
             workbench.inspector_actions.y, action_width, 34);
        const auto& content = workbench.inspector_content;
        move(6, content.x, content.y, 110, 26);
        move(7, content.x + 110, content.y, content.width - 110, 27);
        move(8, content.x, content.y + 42, content.width, 26);
        move(9, content.x, content.y + 72, content.width,
             std::max(190, content.height - 180));
        move(10, content.x, content.bottom() - 94, content.width, 50);
        move(11, content.x, content.bottom() - 38, content.width, 30);
        break;
    }
    case Page::Looks: {
        const auto& workbench = *authoring_layout;
        move(1,
             workbench.collection.x,
             workbench.collection.y,
             workbench.collection.width,
             workbench.collection.height);
        const auto library_half = (workbench.library_actions.width - 8) / 2;
        move(2, workbench.library_actions.x, workbench.library_actions.y,
             library_half, 34);
        move(3, workbench.library_actions.x + library_half + 8,
             workbench.library_actions.y, library_half, 34);
        const auto action_width = std::min(
            170, (workbench.inspector_actions.width - 8) / 2);
        move(4, workbench.inspector_actions.x, workbench.inspector_actions.y,
             action_width, 34);
        move(5, workbench.inspector_actions.right() - action_width,
             workbench.inspector_actions.y, action_width, 34);

        const auto& content = workbench.inspector_content;
        const auto x = content.x;
        const auto form_width = content.width;
        const auto top = content.y;
        move(6, x, top, 48, 26);
        move(7, x + 48, top, std::max(150, form_width - 286), 27);
        move(8, x + form_width - 226, top, 136, 26);
        move(9, x + form_width - 90, top, 90, 27);
        move(10, x, top + 34, 142, 26);
        move(11, x + 142, top + 34, form_width - 142, 250);
        move(12, x, top + 64, form_width, 24);
        move(13, x, top + 92, 68, 26);
        move(14, x + 68, top + 92, 86, 27);
        move(15, x + 162, top + 92, 116, 28);

        const auto emitter_width = std::max(1, form_width / 7);
        for (std::size_t emitter = 0U; emitter < 7U; ++emitter) {
            const auto emitter_x = x + static_cast<int>(emitter) * emitter_width;
            const auto label_index = 16U + emitter * 2U;
            move(label_index, emitter_x, top + 126, emitter_width - 6, 20);
            move(label_index + 1U, emitter_x, top + 147, emitter_width - 6, 27);
        }
        constexpr int apply_color_width = 116;
        constexpr int color_gap = 5;
        const auto swatch_width = std::max(
            1, (form_width - apply_color_width - color_gap * 7) / 7);
        move(30, x, top + 180, apply_color_width, 30);
        for (std::size_t swatch = 0U; swatch < 7U; ++swatch) {
            move(
                31U + swatch,
                x + apply_color_width + color_gap +
                    static_cast<int>(swatch) * (swatch_width + color_gap),
                top + 180,
                swatch_width,
                30);
        }

        constexpr int named_label_width = 180;
        constexpr int named_action_width = 140;
        move(55, x, top + 216, named_label_width, 26);
        move(56, x + named_label_width, top + 216,
             std::max(1, form_width - named_label_width - named_action_width - 8),
             250);
        move(57, x + form_width - named_action_width, top + 214,
             named_action_width, 32);

        const auto property_combo_width = std::clamp(form_width / 4, 112, 176);
        move(38, x, top + 252, 62, 26);
        move(39, x + 62, top + 252, property_combo_width, 250);
        move(40, x + 70 + property_combo_width, top + 252, 78, 26);
        move(41, x + 148 + property_combo_width, top + 252,
             std::max(1, form_width - property_combo_width - 148), 250);
        move(42, x, top + 286, 164, 26);
        move(43, x + 164, top + 286, 62, 27);
        const auto property_action_width = std::max(
            96, (form_width - 242) / 2);
        move(44, x + 234, top + 284, property_action_width, 30);
        move(45, x + 242 + property_action_width, top + 284,
             std::max(1, form_width - property_action_width - 242), 30);

        const auto pane_gap = 10;
        const auto pane_width = (form_width - pane_gap) / 2;
        move(46, x, top + 322, pane_width, 26);
        const auto pane_height = std::max(
            34, content.bottom() - (top + 350) - 106);
        move(47, x, top + 350, pane_width, pane_height);
        move(48, x + pane_width + pane_gap, top + 318, pane_width, 30);
        move(49, x + pane_width + pane_gap, top + 350, pane_width,
             pane_height);
        const auto preview_width = std::max(1, (form_width - 8) * 2 / 3);
        move(52, x, content.bottom() - 98, preview_width, 32);
        move(53, x + preview_width + 8, content.bottom() - 98,
             std::max(1, form_width - preview_width - 8), 32);
        move(54, x, content.bottom() - 62, form_width, 26);
        ::ShowWindow(controls[50], SW_HIDE);
        move(51, x, content.bottom() - 28, form_width, 24);
        break;
    }
    case Page::Autoloops: {
        const auto& workbench = *authoring_layout;
        move(1,
             workbench.collection.x,
             workbench.collection.y,
             workbench.collection.width,
             workbench.collection.height);
        const auto library_half = (workbench.library_actions.width - 8) / 2;
        move(2, workbench.library_actions.x, workbench.library_actions.y,
             library_half, 34);
        move(3, workbench.library_actions.x + library_half + 8,
             workbench.library_actions.y, library_half, 34);
        const auto action_width = std::min(
            170, (workbench.inspector_actions.width - 8) / 2);
        move(4, workbench.inspector_actions.x, workbench.inspector_actions.y,
             action_width, 34);
        move(5, workbench.inspector_actions.right() - action_width,
             workbench.inspector_actions.y, action_width, 34);
        const auto& content = workbench.inspector_content;
        const auto x = content.x;
        const auto form_width = content.width;
        const auto top = content.y;
        move(8, x, top, 105, 26);
        move(9, x + 105, top, form_width - 105, 27);
        const auto field_gap = 8;
        const auto field_width = std::max(1, (form_width - field_gap * 3) / 4);
        for (std::size_t column = 0U; column < 4U; ++column) {
            const auto field_x = x + static_cast<int>(column) *
                (field_width + field_gap);
            const auto base = 10U + column * 2U;
            move(base, field_x, top + 38, field_width, 22);
            move(base + 1U, field_x, top + 60, field_width, 200);
        }
        move(6, x, top + 96, 130, 30);
        move(7, x + 140, top + 96, 140, 30);
        move(18, x, top + 138, form_width, 24);
        const auto builder_action_width = std::min(112, form_width / 5);
        const auto builder_beat_width = std::min(68, form_width / 8);
        const auto builder_transition_width = std::min(126, form_width / 5);
        const auto builder_look_width = std::max(
            1,
            form_width - builder_action_width - builder_beat_width -
                builder_transition_width - 70);
        move(19, x, top + 164, builder_look_width, 200);
        move(20, x + builder_look_width + 8, top + 166, 40, 24);
        move(21, x + builder_look_width + 48, top + 164,
             builder_beat_width, 27);
        move(22, x + builder_look_width + builder_beat_width + 56,
             top + 164, builder_transition_width, 200);
        move(23, x + form_width - builder_action_width, top + 162,
             builder_action_width, 32);
        move(24, x, top + 204, form_width, 26);
        move(28, x + std::max(0, form_width - 316), top + 200, 150, 30);
        move(29, x + std::max(0, form_width - 158), top + 200, 158, 30);
        move(25, x, top + 230, form_width,
             std::max(90, content.height - 358));
        move(26, x, content.bottom() - 88, form_width, 48);
        move(27, x, content.bottom() - 34, form_width, 28);
        break;
    }
    case Page::Autoscript: {
        const auto field_gap = 16;
        const auto field_width = std::max(150, (usable_width - field_gap) / 2);
        move(1, margin, 64, usable_width, 52);
        move(2, margin, 126, 90, 26);
        move(3, margin + 90, 126, field_width - 90, 200);
        move(4, margin + field_width + field_gap, 126, 100, 26);
        move(5, margin + field_width + field_gap + 100, 126,
             field_width - 100, 200);

        const auto quarter = std::max(145, (usable_width - field_gap * 3) / 4);
        for (std::size_t column = 0U; column < 4U; ++column) {
            const auto x = margin + static_cast<int>(column) * (quarter + field_gap);
            const auto base = 6U + column * 2U;
            move(base, x, 168, quarter, 24);
            move(base + 1U, x, 192, quarter, 200);
        }
        move(14, margin, 232, 90, 24);
        move(15, margin + 90, 232, 80, 27);
        move(16, margin + 186, 232, 80, 24);
        move(17, margin + 266, 232, 80, 27);
        move(18, margin + 362, 232, 60, 24);
        move(19, margin + 422, 232, 190, 27);
        move(20, margin + 630, 232, 280, 24);
        move(21, margin + 910, 232, std::max(100, usable_width - 910), 27);

        move(22, margin, 274, 220, 34);
        move(23, margin + 230, 274, 120, 34);
        move(24, margin + 360, 274, 130, 34);
        move(25, margin + 500, 274, 150, 34);
        move(26, margin + 660, 274, 90, 34);
        const auto summary_height = std::max(70, height - 620);
        const auto function_y = 322 + summary_height + 10;
        move(27, margin, 322, usable_width, summary_height);
        const auto function_column = std::max(150, (usable_width - field_gap * 2) / 3);
        move(30, margin, function_y, function_column, 24);
        move(31, margin, function_y + 26, function_column, 220);
        move(32, margin + function_column + field_gap, function_y + 2,
             function_column, 24);
        move(33, margin + function_column + field_gap, function_y + 26,
             function_column, 220);
        move(34, margin + (function_column + field_gap) * 2, function_y + 2,
             function_column, 24);
        move(35, margin + (function_column + field_gap) * 2, function_y + 26,
             function_column, 220);
        const auto compact_width = std::max(76, (usable_width - 310) / 3);
        move(36, margin, function_y + 64, compact_width, 24);
        move(37, margin, function_y + 88, compact_width, 27);
        move(38, margin + compact_width + field_gap, function_y + 64,
             compact_width, 24);
        move(39, margin + compact_width + field_gap, function_y + 88,
             compact_width, 27);
        move(40, margin + (compact_width + field_gap) * 2, function_y + 64,
             compact_width, 24);
        move(41, margin + (compact_width + field_gap) * 2, function_y + 88,
             compact_width, 27);
        move(42, margin + usable_width - 270, function_y + 82, 270, 34);
        move(28, margin, height - 148, usable_width, 64);
        move(29, margin, height - 76, usable_width, 34);
        break;
    }
    case Page::Tracks: {
        const auto& workbench = *authoring_layout;
        move(1,
             workbench.collection.x,
             workbench.collection.y,
             workbench.collection.width,
             workbench.collection.height);
        const auto library_half = (workbench.library_actions.width - 8) / 2;
        move(2, workbench.library_actions.x, workbench.library_actions.y,
             library_half, 34);
        move(3, workbench.library_actions.x + library_half + 8,
             workbench.library_actions.y, library_half, 34);
        const auto action_width = std::min(
            170, (workbench.inspector_actions.width - 8) / 2);
        move(4, workbench.inspector_actions.x, workbench.inspector_actions.y,
             action_width, 34);
        move(5, workbench.inspector_actions.right() - action_width,
             workbench.inspector_actions.y, action_width, 34);
        const auto& content = workbench.inspector_content;
        const auto x = content.x;
        const auto form_width = content.width;
        const auto top = content.y;
        move(6, x, top, 130, 26);
        move(7, x + 130, top, form_width - 130, 27);
        move(8, x, top + 38, 130, 26);
        move(9, x + 130, top + 38, form_width - 130, 200);
        move(10, x, top + 76, 115, 30);
        move(11, x + 125, top + 76, 100, 30);
        move(12, x + 235, top + 76, 100, 30);
        move(13, x + 345, top + 76, 135, 30);
        move(14, x, top + 118, 190, 26);
        move(15, x + 190, top + 118, form_width - 190, 27);
        move(16, x, top + 158, form_width, 26);
        move(17, x, top + 188, form_width,
             std::max(150, content.height - 302));
        move(18, x, content.bottom() - 88, form_width, 48);
        move(19, x, content.bottom() - 34, form_width, 28);
        break;
    }
    case Page::Midi: {
        const auto column_gap = 16;
        const auto column_width = (usable_width - column_gap) / 2;
        const auto first_row = height - 232;
        const auto second_row = height - 192;
        move(1, margin, 70, usable_width, std::max(190, height - 350));
        move(2, margin, first_row, 80, 26);
        move(3, margin + 80, first_row, column_width - 80, 200);
        move(4, margin + column_width + column_gap, first_row, 70, 26);
        move(5, margin + column_width + column_gap + 70, first_row,
             column_width - 70, 200);
        move(6, margin, second_row, 80, 26);
        move(7, margin + 80, second_row, column_width - 80, 200);
        move(14, margin + column_width + column_gap, second_row, 190, 26);
        move(15, margin + column_width + column_gap + 190, second_row,
             column_width - 190, 200);
        move(8, margin, height - 150, 80, 26);
        move(9, margin + 80, height - 150, 180, 200);
        move(10, margin + 280, height - 150, 130, 28);
        move(11, margin, height - 108, 190, 32);
        move(12, margin + 204, height - 108, 140, 32);
        move(13, margin, height - 66, usable_width, 32);
        break;
    }
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
    if (page != Page::Profiles && profile_capability_window_ != nullptr) {
        ::ShowWindow(profile_capability_window_, SW_HIDE);
    }
    if (page != Page::Profiles && profile_channel_workbench_ != nullptr) {
        ::ShowWindow(profile_channel_workbench_, SW_HIDE);
    }
    if (active_page_ == Page::Looks && page != Page::Looks &&
        physical_preview_.status().owns_runner) {
        stop_physical_static_look_preview(false);
        set_status(
            L"Studio hardware preview stopped because you left Static Looks; zero frames were sent.");
    }
    if (active_page_ != page) {
        last_connections_focus_ = nullptr;
    }
    active_page_ = page;
    active_workspace_ = page_workspace(page);
    switch (active_workspace_) {
    case Workspace::Live: last_live_page_ = page; break;
    case Workspace::Studio: last_studio_page_ = page; break;
    case Workspace::System: last_system_page_ = page; break;
    case Workspace::Count: break;
    }
    for (std::size_t index = 0; index < pages_.size(); ++index) {
        ::ShowWindow(pages_[index], index == static_cast<std::size_t>(page) ? SW_SHOW : SW_HIDE);
    }
    refresh_navigation();
    if (page == Page::Diagnostics) {
        refresh_diagnostics();
    }
}

void Application::show_workspace(Workspace workspace) {
    switch (workspace) {
    case Workspace::Live: show_page(last_live_page_); break;
    case Workspace::Studio: show_page(last_studio_page_); break;
    case Workspace::System: show_page(last_system_page_); break;
    case Workspace::Count: break;
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
    profile_duplicate_source_id_.reset();
    fixture_index_ = -1;
    group_index_ = -1;
    look_index_ = -1;
    look_draft_.reset();
    autoloop_index_ = -1;
    track_index_ = -1;
    static_cast<void>(autoscript_workflow_.discard());
    autoscript_function_preview_summary_.clear();
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

void listbox_select_data(HWND list, std::intptr_t data) {
    const auto count = static_cast<int>(::SendMessageW(list, LB_GETCOUNT, 0, 0));
    for (int row = 0; row < count; ++row) {
        if (static_cast<std::intptr_t>(
                ::SendMessageW(list, LB_GETITEMDATA, row, 0)) == data) {
            static_cast<void>(::SendMessageW(list, LB_SETCURSEL, row, 0));
            return;
        }
    }
    static_cast<void>(::SendMessageW(list, LB_SETCURSEL, -1, 0));
}

[[nodiscard]] std::int32_t listbox_selected_data(HWND list) {
    const auto row = static_cast<int>(::SendMessageW(list, LB_GETCURSEL, 0, 0));
    return row >= 0
        ? static_cast<std::int32_t>(
              ::SendMessageW(list, LB_GETITEMDATA, row, 0))
        : -1;
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

void listview_select_data(HWND list, std::int32_t data) {
    const auto count = ListView_GetItemCount(list);
    for (int row = 0; row < count; ++row) {
        LVITEMW item{};
        item.mask = LVIF_PARAM;
        item.iItem = row;
        if (ListView_GetItem(list, &item) != FALSE &&
            static_cast<std::int32_t>(item.lParam) == data) {
            ListView_SetItemState(
                list,
                row,
                LVIS_SELECTED | LVIS_FOCUSED,
                LVIS_SELECTED | LVIS_FOCUSED);
            static_cast<void>(ListView_EnsureVisible(list, row, FALSE));
            return;
        }
    }
    ListView_SetItemState(list, -1, 0U, LVIS_SELECTED | LVIS_FOCUSED);
}

[[nodiscard]] std::string action_name(showcore::ActionType action) {
    switch (action) {
    case showcore::ActionType::None: return "None";
    case showcore::ActionType::SetProperty: return "Set fixture attribute";
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
    case showcore::ActionType::SetGroupProperty: return "Set group attribute";
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
               << channel.default_value << ','
               << channel.blackout_value << ','
               << channel.highlight_value << ','
               << channel.owner.size() << ':' << channel.owner << ','
               << channel.capabilities.size() << '\n';
        for (const auto& capability : channel.capabilities) {
            stream << "  " << capability.id.size() << ':' << capability.id << ','
                   << capability.name.size() << ':' << capability.name << ','
                   << emberlights::property_name(capability.property) << ','
                   << static_cast<unsigned int>(capability.dmx_min) << ','
                   << static_cast<unsigned int>(capability.dmx_max) << ','
                   << static_cast<unsigned int>(capability.preferred_value) << ','
                   << static_cast<unsigned int>(capability.behavior) << ','
                   << static_cast<unsigned int>(capability.access) << ','
                   << static_cast<unsigned int>(capability.role) << ','
                   << (capability.reversed ? 1 : 0) << '\n';
        }
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
                << outcome.assignments_written << " owned attributes).";
        break;
    case emberlights::StaticLookAuthoringResult::NoChange:
        message << action << " already matches this draft.";
        break;
    case emberlights::StaticLookAuthoringResult::TargetNotFound:
        return "Select a patched fixture or fixture group first.";
    case emberlights::StaticLookAuthoringResult::EmptyTarget:
        return "The selected fixture group is empty.";
    case emberlights::StaticLookAuthoringResult::Unsupported:
        return "The selected target does not support that fixture attribute or direct color.";
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
    refresh_autoscript();
    refresh_tracks();
    refresh_midi();
    refresh_overrides();
    refresh_connections();
    refresh_safety();
    refresh_live_lists();
    refresh_live_status();
    refresh_physical_preview_status();
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
    live_autoloop_items_.clear();
    const auto& live = live_project();
    for (std::size_t index = 0; index < live.looks.size(); ++index) {
        listbox_add(looks, widen(live.looks[index].name), index);
    }
    const auto persisted = emberlights::inspect_persisted_autoloop_source(live);
    if (persisted && persisted.stamp.present) {
        live_autoloop_items_.reserve(persisted.source.placements.size());
        for (const auto& placement : persisted.source.placements) {
            const auto asset = std::find_if(
                persisted.source.assets.begin(),
                persisted.source.assets.end(),
                [&](const auto& candidate) {
                    return candidate.id == placement.asset_id;
                });
            live_autoloop_items_.push_back({
                placement.id,
                asset == persisted.source.assets.end()
                    ? placement.asset_id
                    : asset->name,
                {placement.bank, placement.slot},
                true});
        }
        std::sort(
            live_autoloop_items_.begin(),
            live_autoloop_items_.end(),
            [](const auto& first, const auto& second) {
                return std::pair(first.address.bank, first.address.slot) <
                    std::pair(second.address.bank, second.address.slot);
            });
    } else if (persisted) {
        live_autoloop_items_.reserve(live.autoloops.size());
        for (const auto& loop : live.autoloops) {
            live_autoloop_items_.push_back({
                loop.id,
                loop.name,
                {loop.bank, loop.slot},
                false});
        }
    }
    for (std::size_t index = 0U; index < live_autoloop_items_.size(); ++index) {
        const auto& loop = live_autoloop_items_[index];
        std::ostringstream label;
        label << "B" << loop.address.bank + 1U << " / S"
              << static_cast<unsigned int>(loop.address.slot + 1U)
              << (loop.v2 ? " — V2 — " : " — ") << loop.name;
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
    const auto preview = physical_preview_.status();
    live_view_model_.update(status);
    std::wstring headline;
    if (preview.owns_runner) {
        headline = L"STUDIO HARDWARE PREVIEW • ";
        headline += std::to_wstring(
            static_cast<unsigned long long>((preview.remaining_ms + 999U) / 1000U));
        headline += L"s • ";
        headline += std::to_wstring(
            static_cast<unsigned int>(std::lround(preview.output_cap * 100.0F)));
        headline += L"% CAP";
    } else {
        headline = runner_state_name(status.state);
        headline += L" • Clock ";
        headline += widen(number_text(status.bpm));
        headline += L" BPM";
    }
    static_cast<void>(::SetWindowTextW(::GetDlgItem(page, IdLiveState), headline.c_str()));
    static_cast<void>(::SetWindowTextW(
        ::GetDlgItem(page, IdLiveStartStop),
        preview.owns_runner
            ? L"Stop Studio Preview"
            : status.state == emberlights::RunnerState::Running
                ? L"Stop Show"
                : L"Start Show"));
    static_cast<void>(::SetWindowTextW(
        health_start_stop_,
        preview.owns_runner
            ? L"Stop Preview"
            : status.state == emberlights::RunnerState::Running
                ? L"Stop Show"
                : L"Start Show"));
    if (window_ != nullptr && ::GetMenu(window_) != nullptr) {
        static_cast<void>(::ModifyMenuW(
            ::GetSubMenu(::GetMenu(window_), 2),
            IdShowStartStop,
            MF_BYCOMMAND | MF_STRING,
            IdShowStartStop,
            preview.owns_runner
                ? L"&Stop Studio Preview"
                : status.state == emberlights::RunnerState::Running
                    ? L"&Stop Show"
                    : L"&Start Show"));
    }
    static_cast<void>(::SetWindowTextW(
        ::GetDlgItem(page, IdLiveBlackout), status.blackout ? L"RELEASE BLACKOUT" : L"BLACKOUT"));
    static_cast<void>(::SetWindowTextW(
        health_blackout_, status.blackout ? L"RELEASE BLACKOUT" : L"BLACKOUT"));
    static_cast<void>(::SetWindowTextW(
        ::GetDlgItem(page, IdLiveWorkLight), status.work_light ? L"Clear Work Light" : L"Work Light"));
    Button_SetCheck(::GetDlgItem(page, IdLiveFogArm), status.fog_armed ? BST_CHECKED : BST_UNCHECKED);
    Button_SetCheck(::GetDlgItem(page, IdLiveHazeArm), status.haze_armed ? BST_CHECKED : BST_UNCHECKED);
    Button_SetCheck(::GetDlgItem(page, IdLiveLaserArm), status.laser_armed ? BST_CHECKED : BST_UNCHECKED);
    Button_SetCheck(::GetDlgItem(page, IdLiveSparkArm), status.spark_armed ? BST_CHECKED : BST_UNCHECKED);

    const auto set_health = [this](
                                std::size_t index,
                                std::wstring heading,
                                std::wstring value,
                                emberlights::UiStatusTone tone) {
        if (index >= health_badges_.size()) {
            return;
        }
        const auto label = std::move(heading) + L"\n" + std::move(value);
        if (health_badge_tones_[index] == tone &&
            health_badge_labels_[index] == label) {
            return;
        }
        health_badge_tones_[index] = tone;
        health_badge_labels_[index] = label;
        static_cast<void>(::SetWindowTextW(health_badges_[index], label.c_str()));
        static_cast<void>(::InvalidateRect(health_badges_[index], nullptr, TRUE));
    };

    auto project_value = widen(project_.name);
    if (project_value.empty()) {
        project_value = L"Untitled project";
    }
    if (dirty_) {
        project_value += L" • Unsaved";
    }
    set_health(
        0U,
        L"PROJECT",
        std::move(project_value),
        dirty_ ? emberlights::UiStatusTone::Warning
               : emberlights::UiStatusTone::Good);

    auto runner_value = preview.owns_runner
        ? std::wstring{L"Studio preview"}
        : std::wstring{runner_state_name(status.state)};
    if (!preview.owns_runner && status.state == emberlights::RunnerState::Running) {
        if (status.active_autoloop.valid()) {
            runner_value += L" • Loop B" +
                std::to_wstring(status.active_autoloop.bank + 1U) + L"/S" +
                std::to_wstring(status.active_autoloop.slot + 1U);
        } else if (status.active_look >= 0) {
            runner_value += L" • Look active";
        }
    }
    const auto runner_tone = preview.owns_runner
        ? emberlights::UiStatusTone::Info
        : status.state == emberlights::RunnerState::Running
            ? emberlights::UiStatusTone::Good
            : status.state == emberlights::RunnerState::Fault
                ? emberlights::UiStatusTone::Danger
                : status.state == emberlights::RunnerState::Starting ||
                      status.state == emberlights::RunnerState::Stopping
                    ? emberlights::UiStatusTone::Warning
                    : emberlights::UiStatusTone::Neutral;
    set_health(1U, L"RUNNER", std::move(runner_value), runner_tone);

    auto sync_value = std::wstring{sync_state_name(status.sync_state)};
    if (status.bpm > 0.0) {
        sync_value += L" • " + widen(number_text(status.bpm)) + L" BPM";
    }
    const auto sync_tone = status.sync_state == showcore::SyncState::Os2lHealthy
        ? emberlights::UiStatusTone::Good
        : status.sync_state == showcore::SyncState::Manual
            ? emberlights::UiStatusTone::Info
            : status.sync_state == showcore::SyncState::SafeUnsynchronized
                ? emberlights::UiStatusTone::Danger
                : status.sync_state == showcore::SyncState::Waiting
                    ? emberlights::UiStatusTone::Neutral
                    : emberlights::UiStatusTone::Warning;
    set_health(2U, L"SYNC", std::move(sync_value), sync_tone);

    const std::array output_states{
        status.artnet,
        status.sacn,
        status.dmx_usb_pro[0],
        status.dmx_usb_pro[1],
        status.soundswitch_micro,
        status.soundswitch_control_one};
    const auto has_output_state = [&output_states](emberlights::AdapterState state) {
        return std::find(output_states.begin(), output_states.end(), state) !=
            output_states.end();
    };
    const auto output_fault = has_output_state(emberlights::AdapterState::Fault);
    const auto output_ready = has_output_state(emberlights::AdapterState::Ready);
    const auto output_pending =
        has_output_state(emberlights::AdapterState::Starting) ||
        has_output_state(emberlights::AdapterState::Waiting);
    set_health(
        3U,
        L"OUTPUTS",
        output_fault ? L"Fault — open Setup"
            : output_ready ? L"At least one ready"
            : output_pending ? L"Connecting"
            : L"All disabled",
        output_fault ? emberlights::UiStatusTone::Danger
            : output_ready ? emberlights::UiStatusTone::Good
            : output_pending ? emberlights::UiStatusTone::Warning
            : emberlights::UiStatusTone::Neutral);

    set_health(
        4U,
        L"OVERRIDES",
        status.manual_override_count == 0U
            ? L"None active"
            : std::to_wstring(status.manual_override_count) + L" active",
        status.manual_override_count == 0U
            ? emberlights::UiStatusTone::Neutral
            : emberlights::UiStatusTone::Info);

    const auto hazard_armed =
        status.fog_armed || status.haze_armed || status.laser_armed || status.spark_armed;
    set_health(
        5U,
        L"SAFETY",
        status.blackout ? L"BLACKOUT ACTIVE"
            : hazard_armed ? L"Hazard armed"
            : status.work_light ? L"Work light active"
            : L"Hazards disarmed",
        status.blackout ? emberlights::UiStatusTone::Danger
            : hazard_armed ? emberlights::UiStatusTone::Warning
            : status.work_light ? emberlights::UiStatusTone::Info
            : emberlights::UiStatusTone::Good);
    static_cast<void>(::InvalidateRect(health_start_stop_, nullptr, TRUE));
    static_cast<void>(::InvalidateRect(health_blackout_, nullptr, TRUE));
    const auto active_autoloop = status.active_autoloop.valid()
        ? std::optional<showcore::AutoloopAddress>{status.active_autoloop}
        : std::nullopt;
    const auto live_list_state_changed =
        last_painted_active_look_ != status.active_look ||
        last_painted_active_track_ != status.active_track_script ||
        last_painted_active_autoloop_ != active_autoloop;
    last_painted_active_look_ = status.active_look;
    last_painted_active_track_ = status.active_track_script;
    last_painted_active_autoloop_ = active_autoloop;
    if (active_page_ == Page::Live && live_list_state_changed) {
        static_cast<void>(::InvalidateRect(::GetDlgItem(page, IdLiveLooks), nullptr, FALSE));
        static_cast<void>(::InvalidateRect(::GetDlgItem(page, IdLiveAutoloops), nullptr, FALSE));
        static_cast<void>(::InvalidateRect(::GetDlgItem(page, IdLiveTracks), nullptr, FALSE));
    }

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
    const bool can_change_banks =
        status.state == emberlights::RunnerState::Running && !preview.owns_runner;
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
                       << (status.manual_override_count == 1U ? L" attribute" : L" attributes");
        static_cast<void>(::SetWindowTextW(
            ::GetDlgItem(overrides_page, IdOverridesActiveCount), override_count.str().c_str()));
        const bool can_override =
            status.state == emberlights::RunnerState::Running && !preview.owns_runner;
        static_cast<void>(::EnableWindow(
            ::GetDlgItem(overrides_page, IdOverridesApply), can_override));
        static_cast<void>(::EnableWindow(
            ::GetDlgItem(overrides_page, IdOverridesRelease), can_override));
        static_cast<void>(::EnableWindow(
            ::GetDlgItem(overrides_page, IdOverridesReleaseAll), can_override));
        constexpr std::array<int, 5> value_controls{
            IdOverridesSlider,
            IdOverridesZero,
            IdOverridesQuarter,
            IdOverridesHalf,
            IdOverridesFull};
        for (const auto control : value_controls) {
            static_cast<void>(::EnableWindow(
                ::GetDlgItem(overrides_page, control),
                preview.owns_runner ? FALSE : TRUE));
        }
    }
}

void Application::refresh_physical_preview_status() {
    const auto page = pages_[static_cast<std::size_t>(Page::Looks)];
    if (page == nullptr) {
        return;
    }
    const auto status = physical_preview_.status();
    std::ostringstream text;
    if (status.owns_runner) {
        text << "PHYSICAL PREVIEW ACTIVE • "
             << static_cast<unsigned int>(
                    std::lround(status.output_cap * 100.0F))
             << "% CAP • " << (status.remaining_ms + 999U) / 1000U
             << "s • " << status.selected_fixture_count << " fixture"
             << (status.selected_fixture_count == 1U ? "" : "s")
             << " • realtime updates " << status.update_count;
    } else if (status.state ==
               emberlights::StaticLookPhysicalPreviewState::TimedOut) {
        text << "PREVIEW TIMED OUT • output blacked out and terminal zero frames sent";
    } else if (status.state ==
               emberlights::StaticLookPhysicalPreviewState::Fault) {
        text << "PREVIEW STOPPED SAFELY • "
             << emberlights::static_look_physical_preview_error_name(status.error)
             << " • output blacked out";
    } else {
        text << "PHYSICAL PREVIEW OFF • Live must be stopped • 35% cap • 30 second limit";
    }
    set_control_text(::GetDlgItem(page, IdLookPhysicalStatus), text.str());
    static_cast<void>(::SetWindowTextW(
        ::GetDlgItem(page, IdLookPhysicalPreview),
        status.owns_runner
            ? L"Update Preview Now"
            : L"Preview Selected Target on Fixtures"));
    static_cast<void>(::EnableWindow(
        ::GetDlgItem(page, IdLookPhysicalPreview),
        status.owns_runner ||
                runner_.status().state == emberlights::RunnerState::Stopped
            ? TRUE
            : FALSE));
    static_cast<void>(::EnableWindow(
        ::GetDlgItem(page, IdLookPhysicalStop),
        status.owns_runner ? TRUE : FALSE));
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
    static_cast<void>(::SendMessageW(
        ::GetDlgItem(page, IdOverridesSlider), TBM_SETPOS, TRUE, 100));
    set_page_message(
        Page::Overrides,
        IdOverridesMessage,
        live.fixtures.empty()
            ? "Patch at least one fixture before using Live Overrides."
            : (override_control_choices_.empty()
                   ? "Select a target and advanced semantic attribute. Use the value presets or drag the slider; the slider applies when released."
                   : "Choose one profile-backed Fixture Attribute for direct intensity/color/position/beam control or exact shutter/wheel/effect behavior. The 0–100 control sets continuous range position; advanced semantic fallback remains available above."),
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
        refresh_override_control_choices();
        return;
    }
    const auto target_index = static_cast<std::size_t>(::SendMessageW(
        targets, LB_GETITEMDATA, selected, 0));
    if (target_index >= live_view_model_.override_targets().size()) {
        refresh_override_control_choices();
        return;
    }
    const auto& target = live_view_model_.override_targets()[target_index];
    std::size_t selected_combo = 0U;
    std::size_t added = 0U;
    for (std::size_t index = 0U; index < showcore::kPropertyCount; ++index) {
        const auto property = static_cast<showcore::Property>(index);
        if (!target.supports_any(property) ||
            !live_override_property_visible(
                property, live_view_model_.safety())) {
            continue;
        }
        std::string label = fixture_parameter_label(property);
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
    refresh_override_control_choices();
}

void Application::refresh_override_control_choices() {
    const auto page = pages_[static_cast<std::size_t>(Page::Overrides)];
    const auto combo = ::GetDlgItem(page, IdOverridesNamedChoice);
    std::string previous_id;
    const auto previous = combo_selected_data(combo, -1);
    if (previous >= 0 &&
        static_cast<std::size_t>(previous) < override_control_choices_.size()) {
        previous_id = override_control_choices_[static_cast<std::size_t>(previous)].id;
    }
    override_control_choices_.clear();
    static_cast<void>(::SendMessageW(combo, CB_RESETCONTENT, 0, 0));
    combo_add(combo, L"Choose a profile-backed Fixture Attribute…", -1);

    const auto targets = ::GetDlgItem(page, IdOverridesFixture);
    const auto selected = static_cast<int>(::SendMessageW(
        targets, LB_GETCURSEL, 0, 0));
    if (selected >= 0) {
        const auto target_index = static_cast<std::size_t>(::SendMessageW(
            targets, LB_GETITEMDATA, selected, 0));
        if (target_index < live_view_model_.override_targets().size()) {
            const auto& target = live_view_model_.override_targets()[target_index];
            const auto catalog = emberlights::fixture_control_choices(
                live_project(), target.id);
            for (const auto& choice : catalog.choices) {
                if (!choice.live_override_compatible() ||
                    !live_override_property_visible(
                        choice.property, live_view_model_.safety())) {
                    continue;
                }
                const auto data = static_cast<std::intptr_t>(
                    override_control_choices_.size());
                combo_add(
                    combo,
                    widen(fixture_control_choice_label(choice)),
                    data);
                override_control_choices_.push_back(choice);
            }
        }
    }
    auto selected_data = static_cast<std::intptr_t>(-1);
    if (!previous_id.empty()) {
        const auto found = std::find_if(
            override_control_choices_.begin(), override_control_choices_.end(),
            [&previous_id](const auto& choice) {
                return choice.id == previous_id;
            });
        if (found != override_control_choices_.end()) {
            selected_data = static_cast<std::intptr_t>(
                found - override_control_choices_.begin());
        }
    }
    combo_select_data(combo, selected_data);
    static_cast<void>(::EnableWindow(
        ::GetDlgItem(page, IdOverridesApplyNamed),
        override_control_choices_.empty() ? FALSE : TRUE));
}

void Application::refresh_profiles() {
    const auto page = pages_[static_cast<std::size_t>(Page::Profiles)];
    const auto profile_template = ::GetDlgItem(page, IdProfileTemplate);
    const auto previous_template = combo_selected_data(
        profile_template,
        static_cast<std::intptr_t>(
            emberlights::FixtureProfileTemplateId::Rgbwauv6));
    static_cast<void>(::SendMessageW(profile_template, CB_RESETCONTENT, 0, 0));
    for (const auto& descriptor : emberlights::fixture_profile_templates()) {
        combo_add(
            profile_template,
            widen(descriptor.display_name),
            static_cast<std::intptr_t>(descriptor.id));
    }
    combo_select_data(profile_template, previous_template);

    const auto property = ::GetDlgItem(page, IdProfileMappingProperty);
    const auto previous_property = combo_selected_data(
        property, static_cast<std::intptr_t>(showcore::Property::Intensity));
    static_cast<void>(::SendMessageW(property, CB_RESETCONTENT, 0, 0));
    for (const auto& descriptor : emberlights::fixture_parameter_catalog()) {
        combo_add(
            property,
            widen(fixture_parameter_label(descriptor.property, true)),
            static_cast<std::intptr_t>(descriptor.property));
    }
    combo_add(
        property,
        L"unused / safe constant",
        static_cast<std::intptr_t>(showcore::Property::Count));
    combo_select_data(property, previous_property);

    const auto encoding = ::GetDlgItem(page, IdProfileMappingEncoding);
    const auto previous_encoding = combo_selected_data(
        encoding, static_cast<std::intptr_t>(showcore::ChannelEncoding::Linear8));
    static_cast<void>(::SendMessageW(encoding, CB_RESETCONTENT, 0, 0));
    combo_add(
        encoding,
        L"Linear 8-bit",
        static_cast<std::intptr_t>(showcore::ChannelEncoding::Linear8));
    combo_add(
        encoding,
        L"Linear 16-bit",
        static_cast<std::intptr_t>(showcore::ChannelEncoding::Linear16));
    combo_add(
        encoding,
        L"Discrete 8-bit",
        static_cast<std::intptr_t>(showcore::ChannelEncoding::Discrete8));
    combo_add(
        encoding,
        L"Ranged 8-bit",
        static_cast<std::intptr_t>(showcore::ChannelEncoding::Ranged8));
    combo_add(
        encoding,
        L"Safe constant",
        static_cast<std::intptr_t>(showcore::ChannelEncoding::Constant8));
    combo_select_data(encoding, previous_encoding);

    refresh_authoring_collection(Page::Profiles);
    if (profile_index_ >= 0 &&
        static_cast<std::size_t>(profile_index_) < project_.fixture_profiles.size()) {
        select_profile(profile_index_);
    } else {
        new_profile();
    }
    refresh_fixture_catalog_controls();
}

void Application::refresh_fixture_catalog_controls() {
    const auto page = pages_[static_cast<std::size_t>(Page::Profiles)];
    if (page == nullptr) {
        return;
    }
    const auto results = ::GetDlgItem(page, IdProfileCatalogResults);
    const auto selected = results == nullptr
        ? LB_ERR
        : static_cast<int>(::SendMessageW(results, LB_GETCURSEL, 0, 0));
    static_cast<void>(::EnableWindow(
        ::GetDlgItem(page, IdProfileCatalogQuery),
        fixture_catalog_busy_ ? FALSE : TRUE));
    static_cast<void>(::EnableWindow(
        ::GetDlgItem(page, IdProfileCatalogSearch),
        fixture_catalog_busy_ ? FALSE : TRUE));
    static_cast<void>(::EnableWindow(
        ::GetDlgItem(page, IdProfileCatalogImport),
        !fixture_catalog_busy_ && selected != LB_ERR ? TRUE : FALSE));
}

std::string Application::current_profile_editor_snapshot() const {
    const auto page = pages_[static_cast<std::size_t>(Page::Profiles)];
    constexpr std::array ids{
        IdProfileManufacturer,
        IdProfileModel,
        IdProfileMode,
        IdProfileName,
        IdProfileFootprint};
    std::ostringstream snapshot;
    snapshot << profile_index_ << ';';
    for (const auto id : ids) {
        const auto value = normalize_newlines(control_text(::GetDlgItem(page, id)));
        snapshot << value.size() << ':' << value << ';';
    }
    emberlights::FixtureProfileDefinition draft;
    draft.channels = profile_draft_channels_;
    const auto channels = normalize_newlines(profile_channels_text(draft));
    snapshot << channels.size() << ':' << channels << ';';
    return snapshot.str();
}

void Application::refresh_profile_channel_table() {
    const auto page = pages_[static_cast<std::size_t>(Page::Profiles)];
    const auto list = ::GetDlgItem(page, IdProfileChannels);
    if (list == nullptr) {
        return;
    }
    ::EnableWindow(
        ::GetDlgItem(page, IdProfileCapabilitiesOpen),
        profile_draft_channels_.empty() ? FALSE : TRUE);
    std::uint16_t selected_channel = 0U;
    static_cast<void>(parse_number(
        control_text(::GetDlgItem(page, IdProfileMappingChannel)),
        selected_channel));
    ListView_DeleteAllItems(list);
    emberlights::FixtureProfileDefinition draft;
    draft.channels = profile_draft_channels_;
    const auto rows = emberlights::fixture_profile_editor_rows(draft);
    for (std::size_t index = 0U; index < rows.size(); ++index) {
        const auto& row = rows[index];
        listview_set_row(
            list,
            static_cast<int>(index),
            static_cast<LPARAM>(row.source_index),
            {widen(number_text(row.channel)),
             widen(row.property_label),
             widen(row.encoding_label),
             widen(row.range_label),
             widen(row.default_label),
             widen(row.fine_label),
             widen(row.owner_label),
             widen(row.capability_label)});
        if (row.channel == selected_channel) {
            ListView_SetItemState(
                list,
                static_cast<int>(index),
                LVIS_SELECTED | LVIS_FOCUSED,
                LVIS_SELECTED | LVIS_FOCUSED);
        }
    }
    if (profile_channel_workbench_ != nullptr &&
        ::IsWindowVisible(profile_channel_workbench_) != FALSE) {
        refresh_profile_channel_workbench();
    }
}

void Application::select_profile_channel(std::int32_t source_index) {
    if (source_index < 0 ||
        static_cast<std::size_t>(source_index) >= profile_draft_channels_.size()) {
        return;
    }
    const auto page = pages_[static_cast<std::size_t>(Page::Profiles)];
    const auto& channel =
        profile_draft_channels_[static_cast<std::size_t>(source_index)];
    set_control_text(
        ::GetDlgItem(page, IdProfileMappingChannel),
        number_text(channel.coarse_offset + 1U));
    combo_select_data(
        ::GetDlgItem(page, IdProfileMappingProperty),
        static_cast<std::intptr_t>(
            channel.property == showcore::Property::Count &&
                    !channel.capabilities.empty()
                ? channel.capabilities.front().property
                : channel.property));
    combo_select_data(
        ::GetDlgItem(page, IdProfileMappingEncoding),
        static_cast<std::intptr_t>(channel.encoding));
    set_control_text(
        ::GetDlgItem(page, IdProfileMappingFine),
        channel.fine_offset < 0 ? "0" : number_text(channel.fine_offset + 1));
    set_control_text(
        ::GetDlgItem(page, IdProfileMappingMinimum),
        number_text(channel.dmx_min));
    set_control_text(
        ::GetDlgItem(page, IdProfileMappingMaximum),
        number_text(channel.dmx_max));
    set_control_text(
        ::GetDlgItem(page, IdProfileMappingDefault),
        number_text(channel.default_value));
    if (profile_capability_window_ != nullptr &&
        ::IsWindowVisible(profile_capability_window_) != FALSE) {
        profile_capability_channel_ =
            static_cast<std::uint16_t>(channel.coarse_offset + 1U);
        selected_profile_capability_id_.clear();
        refresh_profile_capability_editor();
    }
}

void Application::create_profile_channel_workbench() {
    if (profile_channel_workbench_ != nullptr) {
        return;
    }
    profile_channel_workbench_ = ::CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_CONTROLPARENT,
        kPageClass,
        L"EmberLights — Fixture channel map workbench",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME |
            WS_CLIPCHILDREN,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1080,
        720,
        window_,
        nullptr,
        instance_,
        this);
    if (profile_channel_workbench_ == nullptr) {
        return;
    }
    enable_modern_window_frame(profile_channel_workbench_);
    auto title = add_label(
        profile_channel_workbench_,
        L"FIXTURE CHANNEL MAP WORKBENCH",
        IdChannelWorkbenchTitle);
    ::SendMessageW(title, WM_SETFONT, reinterpret_cast<WPARAM>(title_font_), TRUE);
    add_label(profile_channel_workbench_, L"", IdChannelWorkbenchContext);
    auto list = add_listview(profile_channel_workbench_, IdChannelWorkbenchList);
    add_listview_column(list, 0, 58, L"CH");
    add_listview_column(list, 1, 170, L"Function");
    add_listview_column(list, 2, 118, L"Encoding");
    add_listview_column(list, 3, 90, L"DMX");
    add_listview_column(list, 4, 78, L"Default");
    add_listview_column(list, 5, 64, L"Fine");
    add_listview_column(list, 6, 110, L"Owner");
    add_listview_column(list, 7, 100, L"Ranges");

    add_label(
        profile_channel_workbench_,
        L"Add a direct parameter at the next open channel",
        IdChannelWorkbenchNextPropertyLabel);
    add_combo(profile_channel_workbench_, IdChannelWorkbenchNextProperty);
    add_button(
        profile_channel_workbench_,
        L"Add at Next Channel",
        IdChannelWorkbenchAddNext);
    add_button(
        profile_channel_workbench_,
        L"Fill Unused Slots Safely",
        IdChannelWorkbenchFillGaps);

    add_label(
        profile_channel_workbench_,
        L"First physical channel",
        IdChannelWorkbenchSwapFirstLabel);
    add_combo(profile_channel_workbench_, IdChannelWorkbenchSwapFirst);
    add_label(
        profile_channel_workbench_,
        L"Second physical channel",
        IdChannelWorkbenchSwapSecondLabel);
    add_combo(profile_channel_workbench_, IdChannelWorkbenchSwapSecond);
    add_button(
        profile_channel_workbench_,
        L"Exchange Channel Functions…",
        IdChannelWorkbenchSwap);

    add_button(
        profile_channel_workbench_,
        L"Named DMX Ranges…",
        IdChannelWorkbenchNamedRanges);
    add_button(
        profile_channel_workbench_,
        L"Duplicate to Edit",
        IdProfileDuplicate);
    add_button(
        profile_channel_workbench_,
        L"Save Profile",
        IdProfileSave);
    add_button(
        profile_channel_workbench_,
        L"Done",
        IdChannelWorkbenchDone);
    add_label(profile_channel_workbench_, L"", IdChannelWorkbenchMessage);
    layout_profile_channel_workbench();
}

void Application::layout_profile_channel_workbench() {
    if (profile_channel_workbench_ == nullptr) {
        return;
    }
    RECT client{};
    static_cast<void>(::GetClientRect(profile_channel_workbench_, &client));
    const auto width = std::max(1L, client.right - client.left);
    const auto height = std::max(1L, client.bottom - client.top);
    constexpr int margin = 18;
    const auto usable = static_cast<int>(width) - margin * 2;
    const auto move = [&](int id, int x, int y, int control_width, int control_height) {
        const auto control = ::GetDlgItem(profile_channel_workbench_, id);
        if (control != nullptr) {
            ::MoveWindow(control, x, y, control_width, control_height, TRUE);
        }
    };
    move(IdChannelWorkbenchTitle, margin, 12, usable, 34);
    move(IdChannelWorkbenchContext, margin, 46, usable, 46);
    const auto list_height = std::max(220, static_cast<int>(height) - 372);
    move(IdChannelWorkbenchList, margin, 96, usable, list_height);
    const auto form_y = 106 + list_height;

    const auto add_label_width = std::min(300, std::max(210, usable / 3));
    move(IdChannelWorkbenchNextPropertyLabel, margin, form_y + 2,
         add_label_width, 26);
    move(IdChannelWorkbenchNextProperty, margin + add_label_width, form_y,
         std::max(180, usable - add_label_width - 370), 240);
    move(IdChannelWorkbenchAddNext, margin + usable - 360, form_y - 2,
         170, 32);
    move(IdChannelWorkbenchFillGaps, margin + usable - 180, form_y - 2,
         180, 32);

    const auto swap_combo_width = std::max(150, (usable - 520) / 2);
    move(IdChannelWorkbenchSwapFirstLabel, margin, form_y + 44, 142, 26);
    move(IdChannelWorkbenchSwapFirst, margin + 142, form_y + 42,
         swap_combo_width, 220);
    const auto second_label_x = margin + 152 + swap_combo_width;
    move(IdChannelWorkbenchSwapSecondLabel, second_label_x, form_y + 44,
         154, 26);
    move(IdChannelWorkbenchSwapSecond, second_label_x + 154, form_y + 42,
         swap_combo_width, 220);
    move(IdChannelWorkbenchSwap, margin + usable - 210, form_y + 40,
         210, 34);

    const auto button_y = form_y + 86;
    const auto button_width = std::max(120, (usable - 30) / 4);
    move(IdChannelWorkbenchNamedRanges, margin, button_y, button_width, 32);
    move(IdProfileDuplicate, margin + button_width + 10, button_y,
         button_width, 32);
    move(IdProfileSave, margin + (button_width + 10) * 2, button_y,
         button_width, 32);
    move(IdChannelWorkbenchDone, margin + (button_width + 10) * 3, button_y,
         button_width, 32);
    move(IdChannelWorkbenchMessage, margin, button_y + 40, usable,
         std::max(28, static_cast<int>(height) - button_y - 52));
}

void Application::open_profile_channel_workbench() {
    create_profile_channel_workbench();
    if (profile_channel_workbench_ == nullptr) {
        set_page_message(
            Page::Profiles,
            IdProfileMessage,
            "The Fixture Channel Map Workbench could not be opened.",
            true);
        return;
    }
    set_control_text(
        ::GetDlgItem(profile_channel_workbench_, IdChannelWorkbenchMessage),
        "Choose ordinary parameters without typing IDs. Use Named DMX Ranges for shutter, strobe, gobos, programs, reset/service, or compound channels.");
    static_cast<void>(::SendMessageW(
        ::GetDlgItem(profile_channel_workbench_, IdChannelWorkbenchSwapFirst),
        CB_SETCURSEL,
        static_cast<WPARAM>(-1),
        0));
    static_cast<void>(::SendMessageW(
        ::GetDlgItem(profile_channel_workbench_, IdChannelWorkbenchSwapSecond),
        CB_SETCURSEL,
        static_cast<WPARAM>(-1),
        0));
    refresh_profile_channel_workbench();
    ::ShowWindow(profile_channel_workbench_, SW_SHOW);
    static_cast<void>(::SetForegroundWindow(profile_channel_workbench_));
}

void Application::refresh_profile_channel_workbench() {
    if (profile_channel_workbench_ == nullptr) {
        return;
    }
    const auto page = pages_[static_cast<std::size_t>(Page::Profiles)];
    const auto selected_profile_valid = profile_index_ >= 0 &&
        static_cast<std::size_t>(profile_index_) < project_.fixture_profiles.size();
    const auto source = selected_profile_valid
        ? project_.fixture_profiles[static_cast<std::size_t>(profile_index_)].source
        : showcore::FixtureProfileSource::Local;
    const auto editable = source == showcore::FixtureProfileSource::Local;
    std::uint16_t footprint = 0U;
    const auto footprint_valid = parse_number(
        control_text(::GetDlgItem(page, IdProfileFootprint)), footprint) &&
        footprint != 0U && footprint <= showcore::kUniverseSlots;
    std::uint16_t selected_channel = 0U;
    static_cast<void>(parse_number(
        control_text(::GetDlgItem(page, IdProfileMappingChannel)),
        selected_channel));

    emberlights::FixtureProfileDefinition draft;
    draft.name = trim(control_text(::GetDlgItem(page, IdProfileName)));
    draft.mode = trim(control_text(::GetDlgItem(page, IdProfileMode)));
    draft.footprint = footprint_valid ? footprint : 0U;
    draft.channels = profile_draft_channels_;
    draft.source = source;
    if (selected_profile_valid) {
        const auto& saved =
            project_.fixture_profiles[static_cast<std::size_t>(profile_index_)];
        draft.id = saved.id;
        draft.source_revision = saved.source_revision;
    }

    std::ostringstream context;
    context << (draft.name.empty() ? "Unsaved local fixture profile" : draft.name)
            << "  •  " << (footprint_valid ? std::to_string(footprint) : "invalid")
            << "CH  •  ";
    if (source == showcore::FixtureProfileSource::Local) {
        context << "LOCAL DRAFT — changes remain staged until Save Profile";
    } else if (source == showcore::FixtureProfileSource::BuiltIn) {
        context << "VERIFIED BUILT-IN — read-only; use Duplicate to Edit";
    } else {
        context << "IMPORTED SNAPSHOT — read-only; duplicate and verify against the fixture manual";
    }
    if (runner_.status().state == emberlights::RunnerState::Running) {
        context << "  •  LIVE snapshot is unchanged until Stop Show / Start Show";
    }
    set_control_text(
        ::GetDlgItem(profile_channel_workbench_, IdChannelWorkbenchContext),
        context.str());

    const auto list =
        ::GetDlgItem(profile_channel_workbench_, IdChannelWorkbenchList);
    ListView_DeleteAllItems(list);
    const auto rows = emberlights::fixture_profile_editor_rows(draft);
    for (std::size_t index = 0U; index < rows.size(); ++index) {
        const auto& row = rows[index];
        listview_set_row(
            list,
            static_cast<int>(index),
            static_cast<LPARAM>(row.source_index),
            {widen(number_text(row.channel)),
             widen(row.property_label),
             widen(row.encoding_label),
             widen(row.range_label),
             widen(row.default_label),
             widen(row.fine_label),
             widen(row.owner_label),
             widen(row.capability_label)});
        if (row.channel == selected_channel) {
            ListView_SetItemState(
                list,
                static_cast<int>(index),
                LVIS_SELECTED | LVIS_FOCUSED,
                LVIS_SELECTED | LVIS_FOCUSED);
        }
    }

    const auto next =
        ::GetDlgItem(profile_channel_workbench_, IdChannelWorkbenchNextProperty);
    const auto previous_property = combo_selected_data(
        next, static_cast<std::intptr_t>(showcore::Property::Intensity));
    static_cast<void>(::SendMessageW(next, CB_RESETCONTENT, 0, 0));
    for (const auto& choice : emberlights::fixture_profile_parameter_choices()) {
        if (!choice.direct_assignment_available) {
            continue;
        }
        combo_add(
            next,
            widen(choice.category_label + "  •  " + choice.display_name),
            static_cast<std::intptr_t>(choice.property));
    }
    combo_select_data(next, previous_property);

    const auto first =
        ::GetDlgItem(profile_channel_workbench_, IdChannelWorkbenchSwapFirst);
    const auto second =
        ::GetDlgItem(profile_channel_workbench_, IdChannelWorkbenchSwapSecond);
    auto previous_first = combo_selected_data(first, -1);
    auto previous_second = combo_selected_data(second, -1);
    static_cast<void>(::SendMessageW(first, CB_RESETCONTENT, 0, 0));
    static_cast<void>(::SendMessageW(second, CB_RESETCONTENT, 0, 0));
    std::vector<std::uint16_t> swappable_channels;
    for (const auto& row : rows) {
        const auto& channel = profile_draft_channels_[row.source_index];
        const auto* descriptor =
            emberlights::fixture_parameter_descriptor(channel.property);
        const auto swappable = channel.fine_offset < 0 &&
            channel.capabilities.empty() &&
            channel.encoding == showcore::ChannelEncoding::Linear8 &&
            descriptor != nullptr &&
            descriptor->profile_preset ==
                emberlights::FixtureParameterProfilePreset::DirectLinear &&
            descriptor->safety == emberlights::FixtureParameterSafety::Normal;
        if (!swappable) {
            continue;
        }
        const auto label = L"CH" + widen(number_text(row.channel)) + L"  •  " +
            widen(row.property_label);
        combo_add(first, label, row.channel);
        combo_add(second, label, row.channel);
        swappable_channels.push_back(row.channel);
        if (previous_first < 0 &&
            channel.property == showcore::Property::White) {
            previous_first = row.channel;
        }
        if (previous_second < 0 &&
            channel.property == showcore::Property::Amber) {
            previous_second = row.channel;
        }
    }
    const auto channel_is_swappable = [&](std::intptr_t channel) {
        return channel >= 0 && std::find(
            swappable_channels.begin(),
            swappable_channels.end(),
            static_cast<std::uint16_t>(channel)) != swappable_channels.end();
    };
    if (!channel_is_swappable(previous_first) &&
        !swappable_channels.empty()) {
        previous_first = swappable_channels.front();
    }
    if (!channel_is_swappable(previous_second) ||
        previous_second == previous_first) {
        const auto distinct = std::find_if(
            swappable_channels.begin(),
            swappable_channels.end(),
            [&](std::uint16_t channel) {
                return static_cast<std::intptr_t>(channel) != previous_first;
            });
        previous_second = distinct == swappable_channels.end()
            ? previous_first
            : static_cast<std::intptr_t>(*distinct);
    }
    combo_select_data(first, previous_first);
    combo_select_data(second, previous_second);

    const auto can_edit = editable && footprint_valid;
    for (const auto id : {
             IdChannelWorkbenchNextProperty,
             IdChannelWorkbenchAddNext,
             IdChannelWorkbenchFillGaps}) {
        ::EnableWindow(
            ::GetDlgItem(profile_channel_workbench_, id),
            can_edit ? TRUE : FALSE);
    }
    for (const auto id : {
             IdChannelWorkbenchSwapFirst,
             IdChannelWorkbenchSwapSecond,
             IdChannelWorkbenchSwap}) {
        ::EnableWindow(
            ::GetDlgItem(profile_channel_workbench_, id),
            can_edit && swappable_channels.size() >= 2U ? TRUE : FALSE);
    }
    ::EnableWindow(
        ::GetDlgItem(profile_channel_workbench_, IdChannelWorkbenchNamedRanges),
        rows.empty() ? FALSE : TRUE);
    ::EnableWindow(
        ::GetDlgItem(profile_channel_workbench_, IdProfileDuplicate),
        selected_profile_valid ? TRUE : FALSE);
    ::EnableWindow(
        ::GetDlgItem(profile_channel_workbench_, IdProfileSave),
        can_edit ? TRUE : FALSE);
}

void Application::add_next_profile_channel() {
    if (profile_channel_workbench_ == nullptr) {
        return;
    }
    const auto page = pages_[static_cast<std::size_t>(Page::Profiles)];
    std::uint16_t footprint = 0U;
    if (!parse_number(
            control_text(::GetDlgItem(page, IdProfileFootprint)), footprint) ||
        footprint == 0U || footprint > showcore::kUniverseSlots) {
        set_control_text(
            ::GetDlgItem(profile_channel_workbench_, IdChannelWorkbenchMessage),
            "Enter a fixture footprint from 1 through 512 first.");
        return;
    }
    emberlights::FixtureProfileDefinition draft;
    draft.name = trim(control_text(::GetDlgItem(page, IdProfileName)));
    draft.footprint = footprint;
    draft.channels = profile_draft_channels_;
    draft.source = profile_index_ >= 0 &&
            static_cast<std::size_t>(profile_index_) < project_.fixture_profiles.size()
        ? project_.fixture_profiles[static_cast<std::size_t>(profile_index_)].source
        : showcore::FixtureProfileSource::Local;
    const auto property = static_cast<showcore::Property>(combo_selected_data(
        ::GetDlgItem(profile_channel_workbench_, IdChannelWorkbenchNextProperty),
        static_cast<std::intptr_t>(showcore::Property::Count)));
    const auto result = emberlights::assign_next_or_append_fixture_profile_channel(
        draft, property);
    if (!result) {
        set_control_text(
            ::GetDlgItem(profile_channel_workbench_, IdChannelWorkbenchMessage),
            result.message);
        return;
    }
    profile_draft_channels_ = std::move(draft.channels);
    set_control_text(
        ::GetDlgItem(page, IdProfileFootprint), number_text(draft.footprint));
    set_control_text(
        ::GetDlgItem(page, IdProfileMappingChannel), number_text(result.channel));
    refresh_profile_channel_table();
    refresh_profile_mapping_summary();
    set_control_text(
        ::GetDlgItem(profile_channel_workbench_, IdChannelWorkbenchMessage),
        result.message + " Save Profile to persist the draft.");
    set_page_message(
        Page::Profiles,
        IdProfileMessage,
        result.message + " Save Profile when the channel order matches the fixture manual.");
}

void Application::fill_profile_channel_gaps() {
    if (profile_channel_workbench_ == nullptr) {
        return;
    }
    const auto page = pages_[static_cast<std::size_t>(Page::Profiles)];
    std::uint16_t footprint = 0U;
    static_cast<void>(parse_number(
        control_text(::GetDlgItem(page, IdProfileFootprint)), footprint));
    emberlights::FixtureProfileDefinition draft;
    draft.name = trim(control_text(::GetDlgItem(page, IdProfileName)));
    draft.footprint = footprint;
    draft.channels = profile_draft_channels_;
    draft.source = profile_index_ >= 0 &&
            static_cast<std::size_t>(profile_index_) < project_.fixture_profiles.size()
        ? project_.fixture_profiles[static_cast<std::size_t>(profile_index_)].source
        : showcore::FixtureProfileSource::Local;
    const auto result =
        emberlights::fill_fixture_profile_channel_gaps_with_safe_constants(draft);
    if (result.changed) {
        profile_draft_channels_ = std::move(draft.channels);
        refresh_profile_channel_table();
        refresh_profile_mapping_summary();
    }
    set_control_text(
        ::GetDlgItem(profile_channel_workbench_, IdChannelWorkbenchMessage),
        result.message + (result.changed ? " Save Profile to persist the draft." : ""));
    if (!result) {
        set_page_message(Page::Profiles, IdProfileMessage, result.message, true);
    }
}

void Application::swap_profile_channel_functions() {
    if (profile_channel_workbench_ == nullptr) {
        return;
    }
    const auto first_channel = static_cast<std::uint16_t>(combo_selected_data(
        ::GetDlgItem(profile_channel_workbench_, IdChannelWorkbenchSwapFirst), 0));
    const auto second_channel = static_cast<std::uint16_t>(combo_selected_data(
        ::GetDlgItem(profile_channel_workbench_, IdChannelWorkbenchSwapSecond), 0));
    const auto page = pages_[static_cast<std::size_t>(Page::Profiles)];
    std::uint16_t footprint = 0U;
    static_cast<void>(parse_number(
        control_text(::GetDlgItem(page, IdProfileFootprint)), footprint));
    emberlights::FixtureProfileDefinition draft;
    draft.name = trim(control_text(::GetDlgItem(page, IdProfileName)));
    draft.mode = trim(control_text(::GetDlgItem(page, IdProfileMode)));
    draft.footprint = footprint;
    draft.channels = profile_draft_channels_;
    draft.source = showcore::FixtureProfileSource::Local;
    draft.source_revision = "emberlights-local-draft-v1";
    if (profile_index_ >= 0 &&
        static_cast<std::size_t>(profile_index_) < project_.fixture_profiles.size()) {
        const auto& saved =
            project_.fixture_profiles[static_cast<std::size_t>(profile_index_)];
        draft.id = saved.id;
        draft.source = saved.source;
        draft.source_revision = saved.source_revision;
    }
    const auto planned = emberlights::plan_fixture_profile_channel_function_swap(
        draft, first_channel, second_channel);
    if (!planned) {
        set_control_text(
            ::GetDlgItem(profile_channel_workbench_, IdChannelWorkbenchMessage),
            planned.message);
        return;
    }
    if (!planned.plan.changes_mapping) {
        set_control_text(
            ::GetDlgItem(profile_channel_workbench_, IdChannelWorkbenchMessage),
            planned.message);
        return;
    }
    const auto question = widen(
        planned.message +
        "\n\nThis changes the Local draft only. Save Profile afterward, rebind the patched fixture if prompted, then verify at low intensity against the physical fixture.");
    if (::MessageBoxW(
            profile_channel_workbench_,
            question.c_str(),
            L"Confirm physical channel-function exchange",
            MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) {
        return;
    }
    const auto applied = emberlights::apply_fixture_profile_channel_function_swap(
        draft, planned.plan);
    if (!applied) {
        set_control_text(
            ::GetDlgItem(profile_channel_workbench_, IdChannelWorkbenchMessage),
            applied.message);
        return;
    }
    profile_draft_channels_ = std::move(draft.channels);
    set_control_text(
        ::GetDlgItem(page, IdProfileMappingChannel),
        number_text(first_channel));
    refresh_profile_channel_table();
    refresh_profile_mapping_summary();
    set_control_text(
        ::GetDlgItem(profile_channel_workbench_, IdChannelWorkbenchMessage),
        applied.message +
            " Save Profile, accept the Patch rebind when correcting a duplicate, and verify the physical output.");
    set_page_message(
        Page::Profiles,
        IdProfileMessage,
        applied.message + " Save Profile to persist this exact mapping.");
}

void Application::close_profile_channel_workbench() {
    if (profile_channel_workbench_ != nullptr) {
        ::ShowWindow(profile_channel_workbench_, SW_HIDE);
    }
    if (window_ != nullptr) {
        static_cast<void>(::SetForegroundWindow(window_));
    }
}

void Application::create_profile_capability_window() {
    if (profile_capability_window_ != nullptr) {
        return;
    }
    profile_capability_window_ = ::CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_CONTROLPARENT,
        kPageClass,
        L"EmberLights — Named DMX ranges",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME |
            WS_CLIPCHILDREN,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        980,
        700,
        window_,
        nullptr,
        instance_,
        this);
    if (profile_capability_window_ == nullptr) {
        return;
    }
    enable_modern_window_frame(profile_capability_window_);
    auto title = add_label(
        profile_capability_window_,
        L"NAMED DMX CAPABILITIES",
        IdCapabilityTitle);
    ::SendMessageW(title, WM_SETFONT, reinterpret_cast<WPARAM>(title_font_), TRUE);
    add_label(profile_capability_window_, L"", IdCapabilityContext);
    auto list = add_listview(profile_capability_window_, IdCapabilityList);
    add_listview_column(list, 0, 82, L"DMX");
    add_listview_column(list, 1, 190, L"Name");
    add_listview_column(list, 2, 140, L"Parameter");
    add_listview_column(list, 3, 126, L"Behavior");
    add_listview_column(list, 4, 150, L"Access");
    add_listview_column(list, 5, 120, L"Role");

    add_label(profile_capability_window_, L"Name", IdCapabilityNameLabel);
    add_edit(profile_capability_window_, IdCapabilityName);
    add_label(profile_capability_window_, L"Parameter", IdCapabilityPropertyLabel);
    const auto property = add_combo(profile_capability_window_, IdCapabilityProperty);
    for (const auto& descriptor : emberlights::fixture_parameter_catalog()) {
        combo_add(
            property,
            widen(std::string(descriptor.display_name) + "  •  " +
                  std::string(emberlights::fixture_parameter_category_name(
                      descriptor.category))),
            static_cast<std::intptr_t>(descriptor.property));
    }
    add_label(profile_capability_window_, L"DMX From", IdCapabilityFromLabel);
    add_edit(profile_capability_window_, IdCapabilityFrom);
    add_label(profile_capability_window_, L"DMX To", IdCapabilityToLabel);
    add_edit(profile_capability_window_, IdCapabilityTo);
    add_label(profile_capability_window_, L"Preferred", IdCapabilityPreferredLabel);
    add_edit(profile_capability_window_, IdCapabilityPreferred);
    add_label(profile_capability_window_, L"Behavior", IdCapabilityBehaviorLabel);
    const auto behavior = add_combo(profile_capability_window_, IdCapabilityBehavior);
    for (const auto value : {
             showcore::ChannelCapabilityBehavior::Slot,
             showcore::ChannelCapabilityBehavior::Continuous}) {
        combo_add(
            behavior,
            widen(emberlights::fixture_channel_capability_behavior_name(value)),
            static_cast<std::intptr_t>(value));
    }
    add_label(profile_capability_window_, L"Access", IdCapabilityAccessLabel);
    const auto access = add_combo(profile_capability_window_, IdCapabilityAccess);
    for (const auto value : {
             showcore::ChannelCapabilityAccess::Selectable,
             showcore::ChannelCapabilityAccess::SafetyGated,
             showcore::ChannelCapabilityAccess::Protected}) {
        combo_add(
            access,
            widen(emberlights::fixture_channel_capability_access_name(value)),
            static_cast<std::intptr_t>(value));
    }
    add_label(profile_capability_window_, L"Role", IdCapabilityRoleLabel);
    const auto role = add_combo(profile_capability_window_, IdCapabilityRole);
    for (std::size_t index = 0U;
         index <= static_cast<std::size_t>(
             emberlights::FixtureChannelCapabilityRole::Custom);
         ++index) {
        const auto value = static_cast<emberlights::FixtureChannelCapabilityRole>(index);
        combo_add(
            role,
            widen(emberlights::fixture_channel_capability_role_name(value)),
            static_cast<std::intptr_t>(value));
    }
    add_button(
        profile_capability_window_,
        L"Reverse direction",
        IdCapabilityReverse,
        BS_AUTOCHECKBOX);

    add_label(profile_capability_window_, L"Owner", IdCapabilityOwnerLabel);
    add_edit(profile_capability_window_, IdCapabilityOwner);
    add_label(profile_capability_window_, L"Blackout", IdCapabilityBlackoutLabel);
    add_edit(profile_capability_window_, IdCapabilityBlackout);
    add_label(profile_capability_window_, L"Highlight", IdCapabilityHighlightLabel);
    add_edit(profile_capability_window_, IdCapabilityHighlight);
    add_button(
        profile_capability_window_,
        L"Save channel metadata",
        IdCapabilitySaveMetadata);
    add_button(profile_capability_window_, L"New range", IdCapabilityNew);
    add_button(
        profile_capability_window_,
        L"Add / Update range",
        IdCapabilityUpsert);
    add_button(profile_capability_window_, L"Remove selected", IdCapabilityRemove);
    add_button(profile_capability_window_, L"Done", IdCapabilityClose);
    add_label(profile_capability_window_, L"", IdCapabilityMessage);
    layout_profile_capability_window();
}

void Application::layout_profile_capability_window() {
    if (profile_capability_window_ == nullptr) {
        return;
    }
    RECT client{};
    static_cast<void>(::GetClientRect(profile_capability_window_, &client));
    const auto width = std::max(1L, client.right - client.left);
    const auto height = std::max(1L, client.bottom - client.top);
    constexpr int margin = 18;
    const auto usable = static_cast<int>(width) - margin * 2;
    const auto move = [&](int id, int x, int y, int control_width, int control_height) {
        const auto control = ::GetDlgItem(profile_capability_window_, id);
        if (control != nullptr) {
            ::MoveWindow(control, x, y, control_width, control_height, TRUE);
        }
    };
    move(IdCapabilityTitle, margin, 12, usable, 34);
    move(IdCapabilityContext, margin, 48, usable, 28);
    const auto list_height = std::max(170, static_cast<int>(height) - 424);
    move(IdCapabilityList, margin, 78, usable, list_height);
    const auto form_y = 88 + list_height;
    const auto right_half = margin + usable / 2;

    move(IdCapabilityNameLabel, margin, form_y, 52, 24);
    move(IdCapabilityName, margin + 52, form_y - 2, usable / 2 - 68, 27);
    move(IdCapabilityPropertyLabel, right_half, form_y, 76, 24);
    move(IdCapabilityProperty, right_half + 76, form_y - 2,
         usable - usable / 2 - 76, 200);

    move(IdCapabilityFromLabel, margin, form_y + 38, 72, 24);
    move(IdCapabilityFrom, margin + 72, form_y + 36, 68, 27);
    move(IdCapabilityToLabel, margin + 152, form_y + 38, 58, 24);
    move(IdCapabilityTo, margin + 210, form_y + 36, 68, 27);
    move(IdCapabilityPreferredLabel, margin + 290, form_y + 38, 72, 24);
    move(IdCapabilityPreferred, margin + 362, form_y + 36, 72, 27);
    move(IdCapabilityBehaviorLabel, right_half, form_y + 38, 70, 24);
    move(IdCapabilityBehavior, right_half + 70, form_y + 36,
         usable - usable / 2 - 70, 200);

    move(IdCapabilityAccessLabel, margin, form_y + 76, 52, 24);
    move(IdCapabilityAccess, margin + 52, form_y + 74, 190, 200);
    move(IdCapabilityRoleLabel, margin + 258, form_y + 76, 42, 24);
    move(IdCapabilityRole, margin + 300, form_y + 74, 185, 200);
    move(IdCapabilityReverse, right_half, form_y + 74, 180, 28);

    move(IdCapabilityOwnerLabel, margin, form_y + 116, 52, 24);
    move(IdCapabilityOwner, margin + 52, form_y + 114, 190, 27);
    move(IdCapabilityBlackoutLabel, margin + 258, form_y + 116, 64, 24);
    move(IdCapabilityBlackout, margin + 322, form_y + 114, 68, 27);
    move(IdCapabilityHighlightLabel, margin + 404, form_y + 116, 66, 24);
    move(IdCapabilityHighlight, margin + 470, form_y + 114, 68, 27);
    move(IdCapabilitySaveMetadata, right_half + 96, form_y + 112,
         std::max(160, usable - usable / 2 - 96), 32);

    const auto button_width = std::max(132, (usable - 30) / 4);
    move(IdCapabilityNew, margin, form_y + 156, button_width, 32);
    move(IdCapabilityUpsert, margin + button_width + 10, form_y + 156,
         button_width, 32);
    move(IdCapabilityRemove, margin + (button_width + 10) * 2, form_y + 156,
         button_width, 32);
    move(IdCapabilityClose, margin + (button_width + 10) * 3, form_y + 156,
         button_width, 32);
    move(IdCapabilityMessage, margin, form_y + 194, usable,
         std::max(28, static_cast<int>(height) - form_y - 204));
}

void Application::open_profile_capability_editor() {
    const auto page = pages_[static_cast<std::size_t>(Page::Profiles)];
    std::uint16_t channel = 0U;
    if (!parse_number(
            control_text(::GetDlgItem(page, IdProfileMappingChannel)), channel) ||
        channel == 0U ||
        std::none_of(
            profile_draft_channels_.begin(),
            profile_draft_channels_.end(),
            [channel](const auto& definition) {
                return definition.coarse_offset == channel - 1U;
            })) {
        set_page_message(
            Page::Profiles,
            IdProfileMessage,
            "Select a mapped channel row before opening Named DMX ranges.",
            true);
        return;
    }
    create_profile_capability_window();
    if (profile_capability_window_ == nullptr) {
        set_page_message(
            Page::Profiles,
            IdProfileMessage,
            "The Named DMX ranges editor could not be opened.",
            true);
        return;
    }
    if (profile_capability_channel_ != channel) {
        selected_profile_capability_id_.clear();
    }
    profile_capability_channel_ = channel;
    refresh_profile_capability_editor();
    ::ShowWindow(profile_capability_window_, SW_SHOW);
    static_cast<void>(::SetForegroundWindow(profile_capability_window_));
}

void Application::refresh_profile_capability_editor() {
    if (profile_capability_window_ == nullptr) {
        return;
    }
    const auto channel = std::find_if(
        profile_draft_channels_.begin(),
        profile_draft_channels_.end(),
        [&](const auto& definition) {
            return profile_capability_channel_ != 0U &&
                definition.coarse_offset == profile_capability_channel_ - 1U;
        });
    const auto list = ::GetDlgItem(profile_capability_window_, IdCapabilityList);
    ListView_DeleteAllItems(list);
    if (channel == profile_draft_channels_.end()) {
        set_control_text(
            ::GetDlgItem(profile_capability_window_, IdCapabilityContext),
            "Select a mapped profile channel.");
        for (const auto id : {
                 IdCapabilityName, IdCapabilityProperty, IdCapabilityFrom,
                 IdCapabilityTo, IdCapabilityPreferred, IdCapabilityBehavior,
                 IdCapabilityAccess, IdCapabilityRole, IdCapabilityReverse,
                 IdCapabilityUpsert, IdCapabilityRemove, IdCapabilityOwner,
                 IdCapabilityBlackout, IdCapabilityHighlight,
                 IdCapabilitySaveMetadata, IdCapabilityNew}) {
            ::EnableWindow(::GetDlgItem(profile_capability_window_, id), FALSE);
        }
        return;
    }

    emberlights::FixtureProfileDefinition draft;
    draft.channels = profile_draft_channels_;
    const auto rows = emberlights::fixture_channel_capability_rows(
        draft, profile_capability_channel_);
    std::int32_t selected_source = -1;
    for (std::size_t index = 0U; index < rows.size(); ++index) {
        const auto& row = rows[index];
        listview_set_row(
            list,
            static_cast<int>(index),
            static_cast<LPARAM>(row.source_index),
            {widen(row.range_label),
             widen(row.name),
             widen(row.parameter_label),
             widen(row.behavior_label),
             widen(row.access_label),
             widen(row.role_label)});
        if (row.id == selected_profile_capability_id_) {
            selected_source = static_cast<std::int32_t>(row.source_index);
            ListView_SetItemState(
                list,
                static_cast<int>(index),
                LVIS_SELECTED | LVIS_FOCUSED,
                LVIS_SELECTED | LVIS_FOCUSED);
        }
    }
    std::ostringstream context;
    context << "CH" << profile_capability_channel_ << "  •  "
            << channel->owner << "  •  " << rows.size()
            << " named range" << (rows.size() == 1U ? "" : "s")
            << "  •  controller IDs are generated automatically";
    set_control_text(
        ::GetDlgItem(profile_capability_window_, IdCapabilityContext),
        context.str());
    set_control_text(
        ::GetDlgItem(profile_capability_window_, IdCapabilityOwner),
        channel->owner);
    set_control_text(
        ::GetDlgItem(profile_capability_window_, IdCapabilityBlackout),
        number_text(channel->blackout_value));
    set_control_text(
        ::GetDlgItem(profile_capability_window_, IdCapabilityHighlight),
        number_text(channel->highlight_value));

    const auto editable = profile_index_ < 0 ||
        (static_cast<std::size_t>(profile_index_) < project_.fixture_profiles.size() &&
         project_.fixture_profiles[static_cast<std::size_t>(profile_index_)].source ==
             showcore::FixtureProfileSource::Local);
    const auto supports_ranges =
        channel->encoding != showcore::ChannelEncoding::Constant8 &&
        channel->encoding != showcore::ChannelEncoding::Linear16;
    for (const auto id : {
             IdCapabilityName, IdCapabilityProperty, IdCapabilityFrom,
             IdCapabilityTo, IdCapabilityPreferred, IdCapabilityBehavior,
             IdCapabilityAccess, IdCapabilityRole, IdCapabilityReverse,
             IdCapabilityOwner, IdCapabilityBlackout, IdCapabilityHighlight}) {
        ::EnableWindow(
            ::GetDlgItem(profile_capability_window_, id),
            editable ? TRUE : FALSE);
    }
    ::EnableWindow(
        ::GetDlgItem(profile_capability_window_, IdCapabilityNew),
        editable && supports_ranges ? TRUE : FALSE);
    ::EnableWindow(
        ::GetDlgItem(profile_capability_window_, IdCapabilityUpsert),
        editable && supports_ranges ? TRUE : FALSE);
    ::EnableWindow(
        ::GetDlgItem(profile_capability_window_, IdCapabilityRemove),
        editable && supports_ranges && selected_source >= 0 ? TRUE : FALSE);
    ::EnableWindow(
        ::GetDlgItem(profile_capability_window_, IdCapabilitySaveMetadata),
        editable ? TRUE : FALSE);
    if (selected_source >= 0) {
        select_profile_capability(selected_source);
    } else {
        new_profile_capability();
    }
    if (!editable) {
        set_control_text(
            ::GetDlgItem(profile_capability_window_, IdCapabilityMessage),
            "Read-only snapshot. Use Duplicate to Edit in Fixture Profiles before changing ranges.");
    } else if (!supports_ranges) {
        set_control_text(
            ::GetDlgItem(profile_capability_window_, IdCapabilityMessage),
            "Named ranges apply to mapped 8-bit channels. This channel can still use owner, blackout, and highlight metadata.");
    }
}

void Application::select_profile_capability(std::int32_t source_index) {
    const auto channel = std::find_if(
        profile_draft_channels_.begin(),
        profile_draft_channels_.end(),
        [&](const auto& definition) {
            return profile_capability_channel_ != 0U &&
                definition.coarse_offset == profile_capability_channel_ - 1U;
        });
    if (channel == profile_draft_channels_.end() || source_index < 0 ||
        static_cast<std::size_t>(source_index) >= channel->capabilities.size()) {
        return;
    }
    const auto& capability =
        channel->capabilities[static_cast<std::size_t>(source_index)];
    selected_profile_capability_id_ = capability.id;
    set_control_text(
        ::GetDlgItem(profile_capability_window_, IdCapabilityName),
        capability.name);
    combo_select_data(
        ::GetDlgItem(profile_capability_window_, IdCapabilityProperty),
        static_cast<std::intptr_t>(capability.property));
    set_control_text(
        ::GetDlgItem(profile_capability_window_, IdCapabilityFrom),
        number_text(capability.dmx_min));
    set_control_text(
        ::GetDlgItem(profile_capability_window_, IdCapabilityTo),
        number_text(capability.dmx_max));
    set_control_text(
        ::GetDlgItem(profile_capability_window_, IdCapabilityPreferred),
        number_text(capability.preferred_value));
    combo_select_data(
        ::GetDlgItem(profile_capability_window_, IdCapabilityBehavior),
        static_cast<std::intptr_t>(capability.behavior));
    combo_select_data(
        ::GetDlgItem(profile_capability_window_, IdCapabilityAccess),
        static_cast<std::intptr_t>(capability.access));
    combo_select_data(
        ::GetDlgItem(profile_capability_window_, IdCapabilityRole),
        static_cast<std::intptr_t>(capability.role));
    static_cast<void>(::SendMessageW(
        ::GetDlgItem(profile_capability_window_, IdCapabilityReverse),
        BM_SETCHECK,
        capability.reversed ? BST_CHECKED : BST_UNCHECKED,
        0));
    set_control_text(
        ::GetDlgItem(profile_capability_window_, IdCapabilityMessage),
        "Editing " + capability.id +
            ". The stable controller binding path remains automatic.");
}

void Application::new_profile_capability() {
    if (profile_capability_window_ == nullptr) {
        return;
    }
    selected_profile_capability_id_.clear();
    const auto channel = std::find_if(
        profile_draft_channels_.begin(),
        profile_draft_channels_.end(),
        [&](const auto& definition) {
            return profile_capability_channel_ != 0U &&
                definition.coarse_offset == profile_capability_channel_ - 1U;
        });
    if (channel == profile_draft_channels_.end()) {
        return;
    }
    std::array<bool, 256U> occupied{};
    for (const auto& capability : channel->capabilities) {
        for (std::size_t value = capability.dmx_min;
             value <= capability.dmx_max;
             ++value) {
            occupied[value] = true;
        }
    }
    std::size_t minimum = 0U;
    while (minimum < occupied.size() && occupied[minimum]) {
        ++minimum;
    }
    std::size_t maximum = minimum;
    while (maximum + 1U < occupied.size() && !occupied[maximum + 1U]) {
        ++maximum;
    }
    if (minimum >= occupied.size()) {
        minimum = 0U;
        maximum = 0U;
    }
    auto property = channel->property < showcore::Property::Count
        ? channel->property
        : showcore::Property::Shutter;
    const auto* descriptor = emberlights::fixture_parameter_descriptor(property);
    set_control_text(
        ::GetDlgItem(profile_capability_window_, IdCapabilityName),
        "Function " + std::to_string(channel->capabilities.size() + 1U));
    combo_select_data(
        ::GetDlgItem(profile_capability_window_, IdCapabilityProperty),
        static_cast<std::intptr_t>(property));
    set_control_text(
        ::GetDlgItem(profile_capability_window_, IdCapabilityFrom),
        number_text(minimum));
    set_control_text(
        ::GetDlgItem(profile_capability_window_, IdCapabilityTo),
        number_text(maximum));
    set_control_text(
        ::GetDlgItem(profile_capability_window_, IdCapabilityPreferred),
        number_text(minimum + (maximum - minimum) / 2U));
    const auto continuous = descriptor != nullptr &&
        (descriptor->control_kind ==
             emberlights::FixtureParameterControlKind::Level ||
         descriptor->control_kind ==
             emberlights::FixtureParameterControlKind::Position ||
         descriptor->control_kind ==
             emberlights::FixtureParameterControlKind::Speed);
    combo_select_data(
        ::GetDlgItem(profile_capability_window_, IdCapabilityBehavior),
        static_cast<std::intptr_t>(
            continuous ? showcore::ChannelCapabilityBehavior::Continuous
                       : showcore::ChannelCapabilityBehavior::Slot));
    auto access = showcore::ChannelCapabilityAccess::Selectable;
    if (descriptor != nullptr && descriptor->safety_restricted()) {
        access = descriptor->safety ==
                emberlights::FixtureParameterSafety::UnverifiedCustom
            ? showcore::ChannelCapabilityAccess::Protected
            : showcore::ChannelCapabilityAccess::SafetyGated;
    }
    combo_select_data(
        ::GetDlgItem(profile_capability_window_, IdCapabilityAccess),
        static_cast<std::intptr_t>(access));
    combo_select_data(
        ::GetDlgItem(profile_capability_window_, IdCapabilityRole),
        static_cast<std::intptr_t>(
            emberlights::FixtureChannelCapabilityRole::Function));
    static_cast<void>(::SendMessageW(
        ::GetDlgItem(profile_capability_window_, IdCapabilityReverse),
        BM_SETCHECK,
        BST_UNCHECKED,
        0));
    ::EnableWindow(
        ::GetDlgItem(profile_capability_window_, IdCapabilityRemove),
        FALSE);
    set_control_text(
        ::GetDlgItem(profile_capability_window_, IdCapabilityMessage),
        minimum == 0U && maximum == 0U && !channel->capabilities.empty()
            ? "All DMX values are already covered. Edit or remove an existing range."
            : "A free DMX span was proposed. Set the name, semantic parameter, exact range, and preferred value from the fixture manual.");
}

void Application::upsert_profile_capability() {
    if (profile_capability_window_ == nullptr) {
        return;
    }
    const auto editable = profile_index_ < 0 ||
        (static_cast<std::size_t>(profile_index_) < project_.fixture_profiles.size() &&
         project_.fixture_profiles[static_cast<std::size_t>(profile_index_)].source ==
             showcore::FixtureProfileSource::Local);
    if (!editable) {
        return;
    }
    const auto name = trim(control_text(
        ::GetDlgItem(profile_capability_window_, IdCapabilityName)));
    std::uint16_t minimum = 0U;
    std::uint16_t maximum = 0U;
    std::uint16_t preferred = 0U;
    if (name.empty() ||
        !parse_number(
            control_text(::GetDlgItem(profile_capability_window_, IdCapabilityFrom)),
            minimum) ||
        !parse_number(
            control_text(::GetDlgItem(profile_capability_window_, IdCapabilityTo)),
            maximum) ||
        !parse_number(
            control_text(::GetDlgItem(profile_capability_window_, IdCapabilityPreferred)),
            preferred) ||
        minimum > 255U || maximum > 255U || preferred > 255U) {
        set_control_text(
            ::GetDlgItem(profile_capability_window_, IdCapabilityMessage),
            "Enter a name plus DMX From, To, and Preferred values from 0 through 255.");
        return;
    }
    emberlights::ChannelCapabilityDefinition definition;
    definition.id = selected_profile_capability_id_.empty()
        ? emberlights::make_fixture_channel_capability_id(name)
        : selected_profile_capability_id_;
    definition.name = name;
    definition.property = static_cast<showcore::Property>(combo_selected_data(
        ::GetDlgItem(profile_capability_window_, IdCapabilityProperty),
        static_cast<std::intptr_t>(showcore::Property::Count)));
    definition.dmx_min = static_cast<std::uint8_t>(minimum);
    definition.dmx_max = static_cast<std::uint8_t>(maximum);
    definition.preferred_value = static_cast<std::uint8_t>(preferred);
    definition.behavior = static_cast<showcore::ChannelCapabilityBehavior>(
        combo_selected_data(
            ::GetDlgItem(profile_capability_window_, IdCapabilityBehavior),
            static_cast<std::intptr_t>(
                showcore::ChannelCapabilityBehavior::Slot)));
    definition.access = static_cast<showcore::ChannelCapabilityAccess>(
        combo_selected_data(
            ::GetDlgItem(profile_capability_window_, IdCapabilityAccess),
            static_cast<std::intptr_t>(
                showcore::ChannelCapabilityAccess::Selectable)));
    definition.role = static_cast<emberlights::FixtureChannelCapabilityRole>(
        combo_selected_data(
            ::GetDlgItem(profile_capability_window_, IdCapabilityRole),
            static_cast<std::intptr_t>(
                emberlights::FixtureChannelCapabilityRole::Function)));
    definition.reversed = ::SendMessageW(
        ::GetDlgItem(profile_capability_window_, IdCapabilityReverse),
        BM_GETCHECK,
        0,
        0) == BST_CHECKED;

    std::uint16_t footprint = 0U;
    static_cast<void>(parse_number(
        control_text(::GetDlgItem(
            pages_[static_cast<std::size_t>(Page::Profiles)],
            IdProfileFootprint)),
        footprint));
    emberlights::FixtureProfileDefinition draft;
    draft.name = "Unsaved local fixture profile";
    draft.footprint = footprint;
    draft.channels = profile_draft_channels_;
    const auto result = emberlights::upsert_fixture_channel_capability(
        draft, profile_capability_channel_, definition);
    if (!result) {
        set_control_text(
            ::GetDlgItem(profile_capability_window_, IdCapabilityMessage),
            result.message);
        return;
    }
    profile_draft_channels_ = std::move(draft.channels);
    selected_profile_capability_id_ = definition.id;
    refresh_profile_channel_table();
    refresh_profile_mapping_summary();
    refresh_profile_capability_editor();
    set_control_text(
        ::GetDlgItem(profile_capability_window_, IdCapabilityMessage),
        result.message +
            " Save Profile to persist it; controller/static-look surfaces use the generated semantic path.");
    set_page_message(
        Page::Profiles,
        IdProfileMessage,
        result.message + " Save Profile when the channel map is complete.");
}

void Application::remove_profile_capability() {
    const auto editable = profile_index_ < 0 ||
        (static_cast<std::size_t>(profile_index_) < project_.fixture_profiles.size() &&
         project_.fixture_profiles[static_cast<std::size_t>(profile_index_)].source ==
             showcore::FixtureProfileSource::Local);
    if (!editable || selected_profile_capability_id_.empty()) {
        return;
    }
    if (::MessageBoxW(
            profile_capability_window_,
            L"Remove this named DMX range from the draft profile?",
            L"Remove named range",
            MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) {
        return;
    }
    std::uint16_t footprint = 0U;
    static_cast<void>(parse_number(
        control_text(::GetDlgItem(
            pages_[static_cast<std::size_t>(Page::Profiles)],
            IdProfileFootprint)),
        footprint));
    emberlights::FixtureProfileDefinition draft;
    draft.name = "Unsaved local fixture profile";
    draft.footprint = footprint;
    draft.channels = profile_draft_channels_;
    const auto result = emberlights::remove_fixture_channel_capability(
        draft,
        profile_capability_channel_,
        selected_profile_capability_id_);
    if (!result) {
        set_control_text(
            ::GetDlgItem(profile_capability_window_, IdCapabilityMessage),
            result.message);
        return;
    }
    profile_draft_channels_ = std::move(draft.channels);
    selected_profile_capability_id_.clear();
    refresh_profile_channel_table();
    refresh_profile_mapping_summary();
    refresh_profile_capability_editor();
    set_control_text(
        ::GetDlgItem(profile_capability_window_, IdCapabilityMessage),
        result.message + " Save Profile to persist the draft.");
}

void Application::save_profile_channel_metadata() {
    const auto editable = profile_index_ < 0 ||
        (static_cast<std::size_t>(profile_index_) < project_.fixture_profiles.size() &&
         project_.fixture_profiles[static_cast<std::size_t>(profile_index_)].source ==
             showcore::FixtureProfileSource::Local);
    if (!editable) {
        return;
    }
    const auto owner = trim(control_text(
        ::GetDlgItem(profile_capability_window_, IdCapabilityOwner)));
    std::uint16_t blackout = 0U;
    std::uint16_t highlight = 0U;
    if (!parse_number(
            control_text(::GetDlgItem(
                profile_capability_window_, IdCapabilityBlackout)),
            blackout) ||
        !parse_number(
            control_text(::GetDlgItem(
                profile_capability_window_, IdCapabilityHighlight)),
            highlight)) {
        set_control_text(
            ::GetDlgItem(profile_capability_window_, IdCapabilityMessage),
            "Enter numeric blackout and highlight values that fit the channel encoding.");
        return;
    }
    std::uint16_t footprint = 0U;
    static_cast<void>(parse_number(
        control_text(::GetDlgItem(
            pages_[static_cast<std::size_t>(Page::Profiles)],
            IdProfileFootprint)),
        footprint));
    emberlights::FixtureProfileDefinition draft;
    draft.name = "Unsaved local fixture profile";
    draft.footprint = footprint;
    draft.channels = profile_draft_channels_;
    const auto result = emberlights::update_fixture_profile_channel_metadata(
        draft,
        profile_capability_channel_,
        owner,
        blackout,
        highlight);
    if (!result) {
        set_control_text(
            ::GetDlgItem(profile_capability_window_, IdCapabilityMessage),
            result.message);
        return;
    }
    profile_draft_channels_ = std::move(draft.channels);
    refresh_profile_channel_table();
    refresh_profile_mapping_summary();
    refresh_profile_capability_editor();
    set_control_text(
        ::GetDlgItem(profile_capability_window_, IdCapabilityMessage),
        result.message + " Save Profile to persist the draft.");
}

void Application::close_profile_capability_editor() {
    if (profile_capability_window_ != nullptr) {
        ::ShowWindow(profile_capability_window_, SW_HIDE);
    }
    if (window_ != nullptr) {
        static_cast<void>(::SetForegroundWindow(window_));
    }
}

void Application::refresh_patch() {
    const auto page = pages_[static_cast<std::size_t>(Page::Patch)];
    const auto profile_combo = ::GetDlgItem(page, IdPatchProfile);
    static_cast<void>(::SendMessageW(profile_combo, CB_RESETCONTENT, 0, 0));
    for (std::size_t index = 0; index < project_.fixture_profiles.size(); ++index) {
        const auto& definition = project_.fixture_profiles[index];
        const auto mapping = emberlights::summarize_fixture_profile_mapping(definition);
        std::ostringstream label;
        label << definition.name << "  •  " << definition.footprint << "CH";
        if (mapping.white_mapping_count == 1U && mapping.amber_mapping_count == 1U) {
            label << "  •  W:CH" << mapping.white_channel
                  << " A:CH" << mapping.amber_channel;
        }
        combo_add(
            profile_combo,
            widen(label.str()),
            static_cast<std::intptr_t>(index));
    }
    const auto universe = ::GetDlgItem(page, IdPatchUniverse);
    static_cast<void>(::SendMessageW(universe, CB_RESETCONTENT, 0, 0));
    combo_add(universe, L"1", 1);
    combo_add(universe, L"2", 2);
    refresh_authoring_collection(Page::Patch);
    if (fixture_index_ >= 0 && static_cast<std::size_t>(fixture_index_) < project_.fixtures.size()) {
        select_fixture(fixture_index_);
    } else {
        new_fixture();
    }
}

void Application::refresh_groups() {
    refresh_authoring_collection(Page::Groups);
    if (group_index_ >= 0 && static_cast<std::size_t>(group_index_) < project_.groups.size()) {
        select_group(group_index_);
    } else {
        new_group();
    }
}

void Application::refresh_looks() {
    const auto page = pages_[static_cast<std::size_t>(Page::Looks)];
    const auto ownership = ::GetDlgItem(page, IdLookOwnership);
    const auto previous_ownership = combo_selected_data(
        ownership, static_cast<std::intptr_t>(showcore::ValueMode::Set));
    static_cast<void>(::SendMessageW(ownership, CB_RESETCONTENT, 0, 0));
    combo_add(
        ownership,
        L"Set value (included)",
        static_cast<std::intptr_t>(showcore::ValueMode::Set));
    combo_add(
        ownership,
        L"Hard off (included at zero)",
        static_cast<std::intptr_t>(showcore::ValueMode::ForceZero));
    combo_add(
        ownership,
        L"Follow lower content (release)",
        static_cast<std::intptr_t>(showcore::ValueMode::Release));
    combo_select_data(ownership, previous_ownership);
    ::EnableWindow(
        ::GetDlgItem(page, IdLookValue),
        combo_selected_data(ownership) ==
            static_cast<std::intptr_t>(showcore::ValueMode::Set));
    refresh_look_targets();
    refresh_authoring_collection(Page::Looks);
    if (look_index_ >= 0 && static_cast<std::size_t>(look_index_) < project_.looks.size()) {
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
    if (preferred < 0 && (!project_.fixtures.empty() || !project_.groups.empty())) {
        preferred = 0;
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
                const auto* descriptor =
                    emberlights::fixture_parameter_descriptor(property);
                summary << " • "
                        << (descriptor == nullptr
                                ? std::string(emberlights::property_name(property))
                                : std::string(descriptor->display_name))
                        << ' '
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
        label << fixture_parameter_label(property);
        if (support.partial()) {
            label << " (" << support.supported_fixture_count << '/'
                  << support.target_fixture_count << ')';
        }
        combo_add(properties, widen(label.str()), static_cast<std::intptr_t>(property));
    }
    combo_select_data(properties, previous_property);
    refresh_look_control_choices();
}

void Application::refresh_look_control_choices() {
    const auto page = pages_[static_cast<std::size_t>(Page::Looks)];
    const auto combo = ::GetDlgItem(page, IdLookNamedChoice);
    std::string previous_id;
    const auto previous = combo_selected_data(combo, -1);
    if (previous >= 0 &&
        static_cast<std::size_t>(previous) < look_control_choices_.size()) {
        previous_id = look_control_choices_[static_cast<std::size_t>(previous)].id;
    }
    look_control_choices_.clear();
    static_cast<void>(::SendMessageW(combo, CB_RESETCONTENT, 0, 0));
    combo_add(combo, L"Choose a profile-backed Fixture Attribute…", -1);
    const auto catalog = emberlights::fixture_control_choices(
        project_, selected_look_target_id());
    for (const auto& choice : catalog.choices) {
        const auto data = static_cast<std::intptr_t>(
            look_control_choices_.size());
        combo_add(
            combo,
            widen(fixture_control_choice_label(choice)),
            data);
        look_control_choices_.push_back(choice);
    }
    auto selected_data = static_cast<std::intptr_t>(-1);
    if (!previous_id.empty()) {
        const auto found = std::find_if(
            look_control_choices_.begin(), look_control_choices_.end(),
            [&previous_id](const auto& choice) {
                return choice.id == previous_id;
            });
        if (found != look_control_choices_.end()) {
            selected_data = static_cast<std::intptr_t>(
                found - look_control_choices_.begin());
        }
    }
    combo_select_data(combo, selected_data);
    static_cast<void>(::EnableWindow(
        ::GetDlgItem(page, IdLookApplyNamed),
        look_control_choices_.empty() ? FALSE : TRUE));
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
    const auto repeat = ::GetDlgItem(page, IdAutoloopRepeat);
    static_cast<void>(::SendMessageW(repeat, CB_RESETCONTENT, 0, 0));
    combo_add(repeat, L"Once", static_cast<std::intptr_t>(showcore::AutoloopRepeat::Once));
    combo_add(repeat, L"Infinite", static_cast<std::intptr_t>(showcore::AutoloopRepeat::Infinite));
    combo_add(repeat, L"Track duration", static_cast<std::intptr_t>(showcore::AutoloopRepeat::TrackDuration));
    const auto look_choice = ::GetDlgItem(page, IdAutoloopLookChoice);
    const auto previous_look = combo_selected_data(look_choice, 0);
    static_cast<void>(::SendMessageW(look_choice, CB_RESETCONTENT, 0, 0));
    for (std::size_t index = 0; index < project_.looks.size(); ++index) {
        const auto& look = project_.looks[index];
        combo_add(
            look_choice,
            widen(look.name + "  •  " + look.id),
            static_cast<std::intptr_t>(index));
    }
    if (!project_.looks.empty()) {
        combo_select_data(
            look_choice,
            previous_look >= 0 &&
                    static_cast<std::size_t>(previous_look) < project_.looks.size()
                ? previous_look
                : 0);
    }
    const auto transition = ::GetDlgItem(page, IdAutoloopStepTransition);
    const auto previous_transition = combo_selected_data(
        transition,
        static_cast<std::intptr_t>(showcore::AutoloopTransition::Cut));
    static_cast<void>(::SendMessageW(transition, CB_RESETCONTENT, 0, 0));
    combo_add(
        transition,
        L"Cut",
        static_cast<std::intptr_t>(showcore::AutoloopTransition::Cut));
    combo_add(
        transition,
        L"Linear fade",
        static_cast<std::intptr_t>(showcore::AutoloopTransition::Linear));
    combo_select_data(transition, previous_transition);
    refresh_authoring_collection(Page::Autoloops);
    if (autoloop_index_ >= 0 &&
        static_cast<std::size_t>(autoloop_index_) < project_.autoloops.size()) {
        select_autoloop(autoloop_index_);
    } else {
        new_autoloop();
    }
}

void Application::refresh_autoscript() {
    const auto page = pages_[static_cast<std::size_t>(Page::Autoscript)];
    const auto style = ::GetDlgItem(page, IdAutoscriptStyle);
    static_cast<void>(::SendMessageW(style, CB_RESETCONTENT, 0, 0));
    combo_add(style, L"Subtle", static_cast<std::intptr_t>(
        emberlights::AutoloopAutoscriptStyle::Subtle));
    combo_add(style, L"Balanced", static_cast<std::intptr_t>(
        emberlights::AutoloopAutoscriptStyle::Balanced));
    combo_add(style, L"Color Motion", static_cast<std::intptr_t>(
        emberlights::AutoloopAutoscriptStyle::ColorMotion));
    combo_add(style, L"Build / Drop", static_cast<std::intptr_t>(
        emberlights::AutoloopAutoscriptStyle::BuildDrop));
    combo_select_data(style, static_cast<std::intptr_t>(
        emberlights::AutoloopAutoscriptStyle::Balanced));

    const auto complexity = ::GetDlgItem(page, IdAutoscriptComplexity);
    static_cast<void>(::SendMessageW(complexity, CB_RESETCONTENT, 0, 0));
    combo_add(complexity, L"Minimal", static_cast<std::intptr_t>(
        emberlights::AutoloopAutoscriptComplexity::Minimal));
    combo_add(complexity, L"Low", static_cast<std::intptr_t>(
        emberlights::AutoloopAutoscriptComplexity::Low));
    combo_add(complexity, L"Medium", static_cast<std::intptr_t>(
        emberlights::AutoloopAutoscriptComplexity::Medium));
    combo_add(complexity, L"High", static_cast<std::intptr_t>(
        emberlights::AutoloopAutoscriptComplexity::High));
    combo_select_data(complexity, static_cast<std::intptr_t>(
        emberlights::AutoloopAutoscriptComplexity::Medium));

    const auto grid = ::GetDlgItem(page, IdAutoscriptGrid);
    static_cast<void>(::SendMessageW(grid, CB_RESETCONTENT, 0, 0));
    combo_add(grid, L"1 beat", emberlights::kMusicalTicksPerQuarter);
    combo_add(grid, L"1/2 beat", emberlights::kMusicalTicksPerQuarter / 2);
    combo_add(grid, L"1/4 beat", emberlights::kMusicalTicksPerQuarter / 4);
    combo_select_data(grid, emberlights::kMusicalTicksPerQuarter / 2);

    set_control_text(::GetDlgItem(page, IdAutoscriptTrackBars), "16");
    set_control_text(::GetDlgItem(page, IdAutoscriptLoopBeats), "4");
    set_control_text(::GetDlgItem(page, IdAutoscriptEnergy), "70");
    set_control_text(::GetDlgItem(page, IdAutoscriptSeed), "4122026");
    set_control_text(::GetDlgItem(page, IdAutoscriptRoles), "");

    auto source = emberlights::AutoloopSourceDocument{};
    const auto persisted = emberlights::inspect_persisted_autoloop_source(project_);
    if (persisted && persisted.stamp.present) {
        source = persisted.source;
    }
    const emberlights::AutoloopAuthoringService authoring(std::move(source));
    const auto next = authoring.next_open();
    set_control_text(
        ::GetDlgItem(page, IdAutoscriptBank),
        next.found ? number_text(next.address.bank + 1U) : "");
    set_control_text(
        ::GetDlgItem(page, IdAutoscriptSlot),
        next.found ? number_text(static_cast<unsigned int>(next.address.slot) + 1U)
                   : "");

    const auto placement = ::GetDlgItem(page, IdAutoscriptFunctionPlacement);
    std::string previous_placement;
    const auto previous_placement_index = combo_selected_data(placement, -1);
    if (previous_placement_index >= 0 &&
        static_cast<std::size_t>(previous_placement_index) <
            autoscript_function_placement_ids_.size()) {
        previous_placement = autoscript_function_placement_ids_[
            static_cast<std::size_t>(previous_placement_index)];
    }
    autoscript_function_placement_ids_.clear();
    static_cast<void>(::SendMessageW(placement, CB_RESETCONTENT, 0, 0));
    if (persisted && persisted.stamp.present) {
        for (const auto& item : persisted.source.placements) {
            const auto asset = std::find_if(
                persisted.source.assets.begin(),
                persisted.source.assets.end(),
                [&](const auto& candidate) {
                    return candidate.id == item.asset_id;
                });
            if (asset == persisted.source.assets.end()) {
                continue;
            }
            std::ostringstream label;
            label << "B" << item.bank + 1U << " / S"
                  << static_cast<unsigned int>(item.slot + 1U)
                  << " — " << asset->name;
            combo_add(
                placement,
                widen(label.str()),
                static_cast<std::intptr_t>(
                    autoscript_function_placement_ids_.size()));
            autoscript_function_placement_ids_.push_back(item.id);
        }
    }
    auto placement_selection = static_cast<std::intptr_t>(
        autoscript_function_placement_ids_.empty() ? -1 : 0);
    const auto retained_placement = std::find(
        autoscript_function_placement_ids_.begin(),
        autoscript_function_placement_ids_.end(),
        previous_placement);
    if (retained_placement != autoscript_function_placement_ids_.end()) {
        placement_selection = static_cast<std::intptr_t>(std::distance(
            autoscript_function_placement_ids_.begin(), retained_placement));
    }
    combo_select_data(placement, placement_selection);

    const auto target = ::GetDlgItem(page, IdAutoscriptFunctionTarget);
    std::string previous_target;
    const auto previous_target_index = combo_selected_data(target, -1);
    if (previous_target_index >= 0 &&
        static_cast<std::size_t>(previous_target_index) <
            autoscript_function_target_ids_.size()) {
        previous_target = autoscript_function_target_ids_[
            static_cast<std::size_t>(previous_target_index)];
    }
    autoscript_function_target_ids_.clear();
    static_cast<void>(::SendMessageW(target, CB_RESETCONTENT, 0, 0));
    for (const auto& fixture : project_.fixtures) {
        combo_add(
            target,
            widen("Fixture • " + fixture.name),
            static_cast<std::intptr_t>(autoscript_function_target_ids_.size()));
        autoscript_function_target_ids_.push_back(fixture.id);
    }
    for (const auto& group : project_.groups) {
        combo_add(
            target,
            widen("Group • " + group.name),
            static_cast<std::intptr_t>(autoscript_function_target_ids_.size()));
        autoscript_function_target_ids_.push_back(group.id);
    }
    auto target_selection = static_cast<std::intptr_t>(
        autoscript_function_target_ids_.empty() ? -1 : 0);
    const auto retained_target = std::find(
        autoscript_function_target_ids_.begin(),
        autoscript_function_target_ids_.end(),
        previous_target);
    if (retained_target != autoscript_function_target_ids_.end()) {
        target_selection = static_cast<std::intptr_t>(std::distance(
            autoscript_function_target_ids_.begin(), retained_target));
    }
    combo_select_data(target, target_selection);
    if (trim(control_text(::GetDlgItem(page, IdAutoscriptFunctionStart))).empty()) {
        set_control_text(::GetDlgItem(page, IdAutoscriptFunctionStart), "0");
    }
    if (trim(control_text(::GetDlgItem(page, IdAutoscriptFunctionEnd))).empty()) {
        set_control_text(::GetDlgItem(page, IdAutoscriptFunctionEnd), "1");
    }
    if (trim(control_text(::GetDlgItem(page, IdAutoscriptFunctionPosition))).empty()) {
        set_control_text(::GetDlgItem(page, IdAutoscriptFunctionPosition), "50");
    }
    refresh_autoscript_function_choices();
    refresh_autoscript_summary(
        persisted
            ? "Set musical intent, then generate an immutable offline proposal."
            : persisted.message);
}

void Application::refresh_autoscript_function_choices() {
    const auto page = pages_[static_cast<std::size_t>(Page::Autoscript)];
    const auto combo = ::GetDlgItem(page, IdAutoscriptFunctionChoice);
    std::string previous_id;
    const auto previous = combo_selected_data(combo, -1);
    if (previous >= 0 &&
        static_cast<std::size_t>(previous) <
            autoscript_function_choices_.size()) {
        previous_id = autoscript_function_choices_[
            static_cast<std::size_t>(previous)].id;
    }
    autoscript_function_choices_.clear();
    static_cast<void>(::SendMessageW(combo, CB_RESETCONTENT, 0, 0));
    const auto target_index = combo_selected_data(
        ::GetDlgItem(page, IdAutoscriptFunctionTarget), -1);
    if (target_index >= 0 &&
        static_cast<std::size_t>(target_index) <
            autoscript_function_target_ids_.size()) {
        const auto catalog = emberlights::fixture_control_choices(
            project_,
            autoscript_function_target_ids_[static_cast<std::size_t>(target_index)]);
        for (const auto& choice : catalog.choices) {
            if (choice.safety_gated()) {
                continue;
            }
            combo_add(
                combo,
                widen(fixture_control_choice_label(choice)),
                static_cast<std::intptr_t>(autoscript_function_choices_.size()));
            autoscript_function_choices_.push_back(choice);
        }
    }
    auto selected = static_cast<std::intptr_t>(
        autoscript_function_choices_.empty() ? -1 : 0);
    const auto retained = std::find_if(
        autoscript_function_choices_.begin(),
        autoscript_function_choices_.end(),
        [&](const auto& choice) { return choice.id == previous_id; });
    if (retained != autoscript_function_choices_.end()) {
        selected = static_cast<std::intptr_t>(std::distance(
            autoscript_function_choices_.begin(), retained));
    }
    combo_select_data(combo, selected);
    static_cast<void>(::EnableWindow(
        ::GetDlgItem(page, IdAutoscriptFunctionApply),
        selected >= 0 && !autoscript_function_placement_ids_.empty()
            ? TRUE
            : FALSE));
}

void Application::refresh_autoscript_summary(std::string_view message) {
    const auto page = pages_[static_cast<std::size_t>(Page::Autoscript)];
    const auto workflow = autoscript_workflow_.snapshot();
    const bool same_document = emberlights::serialize_project(
        workflow.document.document) == emberlights::serialize_project(project_);
    const bool can_preview = workflow.preview_ready && !workflow.committed &&
        same_document;
    static_cast<void>(::EnableWindow(
        ::GetDlgItem(page, IdAutoscriptPreviewStart), can_preview));
    static_cast<void>(::EnableWindow(
        ::GetDlgItem(page, IdAutoscriptPreviewMiddle), can_preview));
    static_cast<void>(::EnableWindow(
        ::GetDlgItem(page, IdAutoscriptCommit),
        workflow.can_commit && same_document));
    static_cast<void>(::EnableWindow(
        ::GetDlgItem(page, IdAutoscriptDiscard), workflow.has_proposal));

    std::ostringstream summary;
    const auto persisted = emberlights::inspect_persisted_autoloop_source(project_);
    if (!persisted) {
        summary << "CURRENT V2 CATALOG: INVALID\r\n"
                << persisted.message << "\r\n\r\n";
    } else if (!persisted.stamp.present) {
        summary << "CURRENT V2 CATALOG: not created yet (format-1 content is unchanged)\r\n\r\n";
    } else {
        summary << "CURRENT V2 CATALOG: " << persisted.source.assets.size()
                << " assets, " << persisted.source.placements.size()
                << " placements\r\nSource SHA-256: "
                << persisted.stamp.source_digest << "\r\n\r\n";
    }

    if (!workflow.has_proposal) {
        summary << "No pending proposal. Generate + Offline Preview does not edit the project.";
    } else {
        summary << "PROPOSAL: "
                << emberlights::autoloop_autoscript_proposal_result_name(
                       workflow.proposal_result)
                << "\r\nProposal SHA-256: " << workflow.proposal_digest
                << "\r\nCandidate source SHA-256: "
                << workflow.preview_source_digest
                << "\r\nGenerated: " << workflow.generated_asset_count
                << " asset, " << workflow.generated_event_count << " events";
        if (workflow.address.has_value()) {
            summary << "\r\nPlacement: B" << workflow.address->bank + 1U
                    << " / S"
                    << static_cast<unsigned int>(workflow.address->slot + 1U)
                    << "  " << workflow.placement_id;
        }
        summary << "\r\n\r\nPREVIEW: "
                << emberlights::studio_preview_result_name(
                       workflow.preview_result)
                << "\r\nOutput adapters: "
                << (workflow.preview.output_disabled ? "DISABLED" : "unexpectedly enabled")
                << "\r\nCompiled SHA-256: "
                << workflow.preview.compiled_digest
                << "\r\nFrame SHA-256: " << workflow.preview.frame_sha256
                << "\r\nPhase: " << workflow.preview.phase
                << "  Beat: " << workflow.preview.beat_position;
        for (const auto& fixture : workflow.preview.fixtures) {
            summary << "\r\n  " << fixture.fixture_name << " — U"
                    << static_cast<unsigned int>(fixture.universe) << " A"
                    << fixture.address << " — DMX";
            for (const auto value : fixture.dmx_values) {
                summary << ' ' << static_cast<unsigned int>(value);
            }
        }
        if (workflow.committed) {
            summary << "\r\n\r\nCOMMITTED as one Undo transaction. Save the project, "
                       "then Start Show to launch this placement from Live.";
        } else if (!same_document) {
            summary << "\r\n\r\nSTALE: the project changed after this proposal. "
                       "Discard it and generate again.";
        }
    }
    if (!autoscript_function_preview_summary_.empty()) {
        summary << "\r\n\r\nLAST FIXTURE ATTRIBUTE PREVIEW\r\n"
                << autoscript_function_preview_summary_;
    }
    set_control_text(::GetDlgItem(page, IdAutoscriptSummary), summary.str());
    if (!message.empty()) {
        set_page_message(
            Page::Autoscript,
            IdAutoscriptMessage,
            message,
            !persisted || message.find("failed") != std::string_view::npos ||
                message.find("invalid") != std::string_view::npos ||
                message.find("rejected") != std::string_view::npos);
    }
}

void Application::refresh_tracks() {
    refresh_authoring_collection(Page::Tracks);
    if (track_index_ >= 0 &&
        static_cast<std::size_t>(track_index_) < project_.track_scripts.size()) {
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
        const auto resolve_name = [&](const auto& collection) {
            const auto found = std::find_if(
                collection.begin(), collection.end(),
                [&](const auto& candidate) {
                    return candidate.id == mapping.target_ref;
                });
            return found == collection.end() ? std::string{} : found->name;
        };
        if (!target.empty()) {
            auto resolved = resolve_name(project_.fixtures);
            if (resolved.empty()) {
                resolved = resolve_name(project_.groups);
            }
            if (resolved.empty()) {
                resolved = resolve_name(project_.looks);
            }
            if (resolved.empty()) {
                resolved = resolve_name(project_.autoloops);
            }
            if (resolved.empty()) {
                resolved = resolve_name(project_.track_scripts);
            }
            if (!resolved.empty()) {
                target = std::move(resolved);
            }
        }
        if (target.empty() && mapping.action.property < showcore::Property::Count) {
            target = std::string(emberlights::property_name(mapping.action.property));
        } else if (target.empty() &&
                   (mapping.action.type == showcore::ActionType::SelectAutoloopBank ||
                    mapping.action.type == showcore::ActionType::SetAutoloopBankEnabled) &&
                   mapping.action.target_id < showcore::kMaxAutoloopBanks) {
            target = "Bank " + std::to_string(mapping.action.target_id + 1U);
        }
        if (!mapping.fixture_control_binding_id.empty()) {
            target += " • " + fixture_control_binding_label(
                project_, mapping.fixture_control_binding_id);
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
    for (const auto& descriptor : emberlights::fixture_parameter_catalog()) {
        combo_add(
            property,
            widen(fixture_parameter_label(descriptor.property)),
            static_cast<std::intptr_t>(descriptor.property));
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
    const auto compilation =
        emberlights::compile_project_with_persisted_autoloops(project_);
    const auto& validation = compilation.validation;
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
    emberlights::RunnerOutputSnapshot frame_snapshot;
    const auto has_frame_snapshot = runner_.latest_output_snapshot(frame_snapshot);
    emberlights::RunnerFrameInspectionOptions frame_options;
    frame_options.inspected_at_ms = emberlights::RunnerService::monotonic_ms();
    const auto frame_inspection = emberlights::inspect_runner_frame(
        project_,
        has_frame_snapshot ? &frame_snapshot : nullptr,
        frame_options);
    output << emberlights::format_runner_frame_inspection(frame_inspection)
           << "\r\n";
    if (has_frame_snapshot && status.active_look >= 0 &&
        static_cast<std::size_t>(status.active_look) < project_.looks.size()) {
        const auto& active_look =
            project_.looks[static_cast<std::size_t>(status.active_look)];
        for (std::size_t index = 0U;
             index < emberlights::kIr4SixChannelSafeLookCount;
             ++index) {
            const auto look = static_cast<emberlights::Ir4SixChannelSafeLook>(index);
            if (active_look.id != emberlights::ir4_6ch_safe_look_id(look)) {
                continue;
            }
            const auto comparison = emberlights::compare_runner_frame_to_raw(
                project_,
                &frame_snapshot,
                1U,
                emberlights::ir4_6ch_safe_look_expected_frame(look),
                frame_options);
            output << "IR-4 manual reference for active look "
                   << active_look.name << ":\r\n"
                   << emberlights::format_runner_raw_reference_comparison(comparison)
                   << "\r\n";
            const auto parity =
                emberlights::bind_runner_frame_to_raw_hardware_attempt(
                    project_,
                    &frame_snapshot,
                    1U,
                    emberlights::ir4_6ch_safe_look_expected_frame(look),
                    frame_options);
            output << emberlights::format_runner_raw_hardware_parity_report(parity)
                   << "\r\n";
            break;
        }
    }
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
        set_multiline_control_text_preserving_view(
            ::GetDlgItem(page, IdDiagnosticsText), diagnostics_text());
    }
}

std::vector<emberlights::UiAuthoringItem> Application::authoring_items(
    Page page) const {
    std::vector<emberlights::UiAuthoringItem> items;
    switch (page) {
    case Page::Profiles:
        items.reserve(project_.fixture_profiles.size());
        for (const auto& profile : project_.fixture_profiles) {
            std::ostringstream secondary;
            secondary << profile.manufacturer << " • " << profile.model;
            if (!profile.mode.empty()) {
                secondary << " • " << profile.mode;
            }
            secondary << " • " << profile.footprint << "CH";
            std::vector<std::string> terms{
                profile.manufacturer,
                profile.model,
                profile.mode,
                profile.source_revision};
            for (const auto& channel : profile.channels) {
                terms.push_back(std::string{
                    fixture_parameter_label(channel.property, false)});
                terms.push_back(channel.owner);
                for (const auto& capability : channel.capabilities) {
                    terms.push_back(capability.name);
                    terms.push_back(capability.id);
                }
            }
            items.push_back({
                profile.id,
                profile.name,
                secondary.str(),
                std::move(terms),
                profile.source != showcore::FixtureProfileSource::Local});
        }
        break;
    case Page::Patch:
        items.reserve(project_.fixtures.size());
        for (const auto& fixture : project_.fixtures) {
            const auto profile = std::find_if(
                project_.fixture_profiles.begin(),
                project_.fixture_profiles.end(),
                [&](const auto& candidate) {
                    return candidate.id == fixture.profile_id;
                });
            std::ostringstream secondary;
            secondary << (profile != project_.fixture_profiles.end()
                              ? profile->name
                              : "Missing profile")
                      << " • U" << static_cast<unsigned int>(fixture.universe)
                      << ":" << fixture.address;
            auto terms = fixture.roles;
            terms.push_back(fixture.profile_id);
            terms.push_back(number_text(fixture.address));
            terms.push_back(number_text(fixture.universe));
            items.push_back({
                fixture.id,
                fixture.name,
                secondary.str(),
                std::move(terms),
                false});
        }
        break;
    case Page::Groups:
        items.reserve(project_.groups.size());
        for (const auto& group : project_.groups) {
            std::ostringstream secondary;
            secondary << group.fixture_ids.size() << " fixture"
                      << (group.fixture_ids.size() == 1U ? "" : "s");
            items.push_back({
                group.id,
                group.name,
                secondary.str(),
                group.fixture_ids,
                false});
        }
        break;
    case Page::Looks:
        items.reserve(project_.looks.size());
        for (const auto& look : project_.looks) {
            std::ostringstream secondary;
            secondary << look.assignments.size() << " assignment"
                      << (look.assignments.size() == 1U ? "" : "s")
                      << " • " << look.fade_ms << "ms";
            std::vector<std::string> terms;
            terms.reserve(look.assignments.size() * 2U + 1U);
            terms.push_back(number_text(look.fade_ms));
            for (const auto& assignment : look.assignments) {
                terms.push_back(assignment.fixture_id);
                terms.push_back(std::string{
                    fixture_parameter_label(assignment.property, false)});
            }
            items.push_back({
                look.id,
                look.name,
                secondary.str(),
                std::move(terms),
                false});
        }
        break;
    case Page::Autoloops:
        items.reserve(project_.autoloops.size());
        for (const auto& loop : project_.autoloops) {
            std::ostringstream secondary;
            secondary << "B" << loop.bank + 1U << " / S"
                      << static_cast<unsigned int>(loop.slot + 1U) << " • "
                      << loop.length_beats << " beats";
            std::vector<std::string> terms{
                number_text(loop.bank + 1U),
                number_text(static_cast<unsigned int>(loop.slot + 1U)),
                number_text(loop.length_beats)};
            for (const auto& step : loop.steps) {
                terms.push_back(step.look_id);
            }
            items.push_back({
                loop.id,
                loop.name,
                secondary.str(),
                std::move(terms),
                false});
        }
        break;
    case Page::Tracks:
        items.reserve(project_.track_scripts.size());
        for (const auto& track : project_.track_scripts) {
            const auto asset = std::find_if(
                project_.audio_assets.begin(),
                project_.audio_assets.end(),
                [&](const auto& candidate) {
                    return candidate.id == track.audio_asset_id;
                });
            std::ostringstream secondary;
            secondary << track.cues.size() << " cue"
                      << (track.cues.size() == 1U ? "" : "s");
            if (asset != project_.audio_assets.end()) {
                secondary << " • " << asset->name;
            }
            std::vector<std::string> terms{
                track.audio_asset_id,
                track.audio_key};
            if (asset != project_.audio_assets.end()) {
                terms.push_back(asset->name);
                terms.push_back(asset->file_name);
            }
            for (const auto& cue : track.cues) {
                terms.push_back(cue.target_ref);
                terms.push_back(number_text(cue.at_beat));
            }
            items.push_back({
                track.id,
                track.name,
                secondary.str(),
                std::move(terms),
                false});
        }
        break;
    default:
        break;
    }
    return items;
}

std::int32_t Application::authoring_selected_index(Page page) const noexcept {
    switch (page) {
    case Page::Profiles: return profile_index_;
    case Page::Patch: return fixture_index_;
    case Page::Groups: return group_index_;
    case Page::Looks: return look_index_;
    case Page::Autoloops: return autoloop_index_;
    case Page::Tracks: return track_index_;
    default: return -1;
    }
}

std::string Application::authoring_selected_id(Page page) const {
    const auto index = authoring_selected_index(page);
    if (index < 0) {
        return {};
    }
    const auto source_index = static_cast<std::size_t>(index);
    switch (page) {
    case Page::Profiles:
        return source_index < project_.fixture_profiles.size()
            ? project_.fixture_profiles[source_index].id
            : std::string{};
    case Page::Patch:
        return source_index < project_.fixtures.size()
            ? project_.fixtures[source_index].id
            : std::string{};
    case Page::Groups:
        return source_index < project_.groups.size()
            ? project_.groups[source_index].id
            : std::string{};
    case Page::Looks:
        return source_index < project_.looks.size()
            ? project_.looks[source_index].id
            : std::string{};
    case Page::Autoloops:
        return source_index < project_.autoloops.size()
            ? project_.autoloops[source_index].id
            : std::string{};
    case Page::Tracks:
        return source_index < project_.track_scripts.size()
            ? project_.track_scripts[source_index].id
            : std::string{};
    default:
        return {};
    }
}

std::string Application::authoring_editor_snapshot(Page page) const {
    if (!is_authoring_page(page)) {
        return {};
    }
    if (page == Page::Profiles) {
        return current_profile_editor_snapshot();
    }
    const auto parent = pages_[static_cast<std::size_t>(page)];
    std::ostringstream snapshot;
    snapshot << authoring_selected_index(page) << ';';
    const auto append_text = [&](int id) {
        const auto value = normalize_newlines(
            control_text(::GetDlgItem(parent, id)));
        snapshot << value.size() << ':' << value << ';';
    };
    const auto append_combo = [&](int id) {
        snapshot << combo_selected_data(::GetDlgItem(parent, id), -1) << ';';
    };
    switch (page) {
    case Page::Patch:
        append_text(IdPatchName);
        append_combo(IdPatchProfile);
        append_combo(IdPatchUniverse);
        append_text(IdPatchAddress);
        append_text(IdPatchRoles);
        break;
    case Page::Groups:
        append_text(IdGroupName);
        append_text(IdGroupMembers);
        break;
    case Page::Looks:
        for (const auto id : {
                 IdLookName,
                 IdLookFade,
                 IdLookRgbHex,
                 IdLookRed,
                 IdLookGreen,
                 IdLookBlue,
                 IdLookWhite,
                 IdLookAmber,
                 IdLookUv,
                 IdLookIntensity,
                 IdLookValue,
                 IdLookAssignments}) {
            append_text(id);
        }
        append_combo(IdLookTarget);
        append_combo(IdLookProperty);
        append_combo(IdLookOwnership);
        append_combo(IdLookNamedChoice);
        break;
    case Page::Autoloops:
        for (const auto id : {
                 IdAutoloopName,
                 IdAutoloopBank,
                 IdAutoloopSlot,
                 IdAutoloopLength,
                 IdAutoloopStepBeat,
                 IdAutoloopSteps}) {
            append_text(id);
        }
        append_combo(IdAutoloopRepeat);
        append_combo(IdAutoloopLookChoice);
        append_combo(IdAutoloopStepTransition);
        break;
    case Page::Tracks:
        append_text(IdTrackName);
        append_combo(IdTrackAudioAsset);
        append_text(IdTrackAudioKey);
        append_text(IdTrackCues);
        break;
    default:
        break;
    }
    return snapshot.str();
}

bool Application::authoring_editor_changed(Page page) const {
    if (!is_authoring_page(page)) {
        return false;
    }
    const auto& baseline =
        authoring_editor_baselines_[static_cast<std::size_t>(page)];
    return !baseline.empty() && authoring_editor_snapshot(page) != baseline;
}

void Application::capture_authoring_editor_baseline(Page page) {
    if (is_authoring_page(page)) {
        authoring_editor_baselines_[static_cast<std::size_t>(page)] =
            authoring_editor_snapshot(page);
        refresh_authoring_summary(page);
    }
}

void Application::refresh_authoring_collection(Page page) {
    if (!is_authoring_page(page)) {
        return;
    }
    const auto parent = pages_[static_cast<std::size_t>(page)];
    if (parent == nullptr) {
        return;
    }
    const auto items = authoring_items(page);
    const auto query = control_text(::GetDlgItem(parent, IdAuthoringSearch));
    const auto projection = emberlights::project_authoring_items(
        items, query, authoring_selected_id(page));
    const auto previous_refreshing = refreshing_;
    refreshing_ = true;
    const auto collection = ::GetDlgItem(
        parent, authoring_collection_control_id(page));
    if (page == Page::Patch) {
        ListView_DeleteAllItems(collection);
        for (std::size_t row = 0U; row < projection.source_indices.size(); ++row) {
            const auto source_index = projection.source_indices[row];
            const auto& fixture = project_.fixtures[source_index];
            const auto profile = std::find_if(
                project_.fixture_profiles.begin(),
                project_.fixture_profiles.end(),
                [&](const auto& candidate) {
                    return candidate.id == fixture.profile_id;
                });
            listview_set_row(
                collection,
                static_cast<int>(row),
                static_cast<LPARAM>(source_index),
                {widen(fixture.name),
                 widen(fixture.id),
                 profile != project_.fixture_profiles.end()
                     ? widen(profile->name)
                     : L"Missing",
                 widen(number_text(fixture.universe)),
                 widen(number_text(fixture.address)),
                 profile != project_.fixture_profiles.end()
                     ? widen(number_text(profile->footprint))
                     : L"—"});
        }
        listview_select_data(collection, authoring_selected_index(page));
    } else {
        static_cast<void>(::SendMessageW(collection, LB_RESETCONTENT, 0, 0));
        for (const auto source_index : projection.source_indices) {
            const auto& item = items[source_index];
            const auto label = item.secondary_text.empty()
                ? item.primary_text
                : item.primary_text + "  •  " + item.secondary_text;
            listbox_add(
                collection,
                widen(label),
                static_cast<std::intptr_t>(source_index));
        }
        listbox_select_data(collection, authoring_selected_index(page));
    }
    set_control_text(
        ::GetDlgItem(parent, IdAuthoringCollectionSummary),
        emberlights::authoring_collection_summary(
            authoring_resource_kind(page), projection));
    refreshing_ = previous_refreshing;
    refresh_authoring_summary(page);
}

void Application::refresh_authoring_summary(Page page) {
    if (!is_authoring_page(page)) {
        return;
    }
    const auto parent = pages_[static_cast<std::size_t>(page)];
    if (parent == nullptr) {
        return;
    }
    const auto items = authoring_items(page);
    const auto query = control_text(::GetDlgItem(parent, IdAuthoringSearch));
    const auto selected_id = authoring_selected_id(page);
    const auto projection = emberlights::project_authoring_items(
        items, query, selected_id);
    set_control_text(
        ::GetDlgItem(parent, IdAuthoringCollectionSummary),
        emberlights::authoring_collection_summary(
            authoring_resource_kind(page), projection));

    emberlights::UiAuthoringInspectorStatus status;
    status.kind = authoring_resource_kind(page);
    status.draft_changed = authoring_editor_changed(page);
    const auto selected_index = authoring_selected_index(page);
    if (selected_index >= 0 &&
        static_cast<std::size_t>(selected_index) < items.size()) {
        const auto& item = items[static_cast<std::size_t>(selected_index)];
        status.mode = item.read_only
            ? emberlights::UiAuthoringInspectorMode::ReadOnly
            : emberlights::UiAuthoringInspectorMode::Editing;
        status.primary_text = item.primary_text;
        status.stable_id = item.stable_id;
    } else {
        status.mode = emberlights::UiAuthoringInspectorMode::Creating;
    }
    set_control_text(
        ::GetDlgItem(parent, IdAuthoringInspectorHeading),
        emberlights::authoring_inspector_heading(status));
}

void Application::restore_authoring_collection_selection(Page page) {
    if (!is_authoring_page(page)) {
        return;
    }
    const auto collection = ::GetDlgItem(
        pages_[static_cast<std::size_t>(page)],
        authoring_collection_control_id(page));
    const auto previous_refreshing = refreshing_;
    refreshing_ = true;
    if (page == Page::Patch) {
        listview_select_data(collection, authoring_selected_index(page));
    } else {
        listbox_select_data(collection, authoring_selected_index(page));
    }
    refreshing_ = previous_refreshing;
}

bool Application::confirm_authoring_selection_change(
    Page page,
    std::int32_t next_index) {
    if (!is_authoring_page(page) ||
        next_index == authoring_selected_index(page) ||
        !authoring_editor_changed(page)) {
        return true;
    }
    const auto descriptor = emberlights::authoring_resource_descriptor(
        authoring_resource_kind(page));
    const auto message = widen(
        std::string{"This "} + std::string{descriptor.singular} +
        " has unsaved Inspector edits.\n\nDiscard those edits and open another " +
        std::string{descriptor.singular} + "?\n\nChoose No, then use Save first, to keep them.");
    if (::MessageBoxW(
            window_,
            message.c_str(),
            L"Unsaved Inspector edits",
            MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES) {
        return true;
    }
    restore_authoring_collection_selection(page);
    return false;
}

void Application::focus_authoring_search() {
    if (!is_authoring_page(active_page_)) {
        set_status(L"Ctrl+F searches the active Studio authoring library.");
        return;
    }
    const auto search = ::GetDlgItem(
        pages_[static_cast<std::size_t>(active_page_)], IdAuthoringSearch);
    if (search != nullptr) {
        ::SetFocus(search);
        static_cast<void>(::SendMessageW(search, EM_SETSEL, 0, -1));
        set_status(L"Search the current Studio library. Press Esc to clear the filter.");
    }
}

void Application::clear_authoring_search() {
    if (!is_authoring_page(active_page_)) {
        return;
    }
    const auto search = ::GetDlgItem(
        pages_[static_cast<std::size_t>(active_page_)], IdAuthoringSearch);
    if (search != nullptr && !control_text(search).empty()) {
        set_control_text(search, "");
        refresh_authoring_collection(active_page_);
        set_status(L"Studio library filter cleared.");
    }
}

void Application::handle_notify(const NMHDR& notification) {
    if (notification.idFrom == IdPatchList && notification.code == LVN_ITEMCHANGED && !refreshing_) {
        const auto index = listview_selected_data(notification.hwndFrom);
        if (index >= 0 &&
            confirm_authoring_selection_change(Page::Patch, index)) {
            select_fixture(index);
        }
    } else if (notification.idFrom == IdProfileChannels &&
               notification.code == LVN_ITEMCHANGED && !refreshing_) {
        const auto index = listview_selected_data(notification.hwndFrom);
        if (index >= 0) {
            select_profile_channel(index);
        }
    } else if (notification.idFrom == IdChannelWorkbenchList &&
               notification.code == LVN_ITEMCHANGED && !refreshing_) {
        const auto index = listview_selected_data(notification.hwndFrom);
        if (index >= 0) {
            select_profile_channel(index);
        }
    } else if (notification.idFrom == IdCapabilityList &&
               notification.code == LVN_ITEMCHANGED && !refreshing_) {
        const auto index = listview_selected_data(notification.hwndFrom);
        if (index >= 0) {
            select_profile_capability(index);
            const auto editable = profile_index_ < 0 ||
                (static_cast<std::size_t>(profile_index_) <
                     project_.fixture_profiles.size() &&
                 project_.fixture_profiles[
                     static_cast<std::size_t>(profile_index_)].source ==
                     showcore::FixtureProfileSource::Local);
            ::EnableWindow(
                ::GetDlgItem(profile_capability_window_, IdCapabilityRemove),
                editable ? TRUE : FALSE);
        }
    }
}

void Application::handle_horizontal_scroll(UINT scroll_code, HWND source) {
    const auto page = pages_[static_cast<std::size_t>(Page::Overrides)];
    if (source == nullptr || source != ::GetDlgItem(page, IdOverridesSlider)) {
        return;
    }
    const auto value = static_cast<int>(::SendMessageW(source, TBM_GETPOS, 0, 0));
    set_control_text(::GetDlgItem(page, IdOverridesValue), number_text(value));
    if (scroll_code == TB_ENDTRACK || scroll_code == TB_THUMBPOSITION) {
        if (runner_.status().state == emberlights::RunnerState::Running &&
            !physical_preview_.status().owns_runner) {
            const auto named = combo_selected_data(
                ::GetDlgItem(page, IdOverridesNamedChoice), -1);
            if (named >= 0 &&
                static_cast<std::size_t>(named) <
                    override_control_choices_.size()) {
                apply_named_fixture_override();
            } else {
                apply_fixture_override(true);
            }
        } else {
            set_page_message(
                Page::Overrides,
                IdOverridesMessage,
                "Value selected. Start the show before applying a Live Override.");
        }
    }
}

void Application::handle_timer() {
    refresh_live_status();
    refresh_physical_preview_status();
    if (is_authoring_page(active_page_)) {
        refresh_authoring_summary(active_page_);
    }
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

void Application::handle_command(int id, int notification, HWND source) {
    if (id == IdAuthoringFind) {
        focus_authoring_search();
        return;
    }
    if (id == IdAuthoringClearFilter) {
        clear_authoring_search();
        return;
    }
    if (id >= IdWorkspaceLive && id <= IdWorkspaceSystem) {
        show_workspace(static_cast<Workspace>(id - IdWorkspaceLive));
        return;
    }
    if (id >= IdNavLive && id <= IdNavDiagnostics) {
        show_page(static_cast<Page>(id - IdNavLive));
        return;
    }
    if (notification == LBN_DBLCLK && !refreshing_) {
        if (id == IdLiveLooks) {
            handle_command(IdLiveTriggerLook, BN_CLICKED, nullptr);
            return;
        }
        if (id == IdLiveAutoloops) {
            handle_command(IdLiveTriggerAutoloop, BN_CLICKED, nullptr);
            return;
        }
        if (id == IdLiveTracks) {
            handle_command(IdLiveTriggerTrack, BN_CLICKED, nullptr);
            return;
        }
        if (id == IdProfileCatalogResults) {
            handle_command(IdProfileCatalogImport, BN_CLICKED, nullptr);
            return;
        }
    }
    if (notification == LBN_SELCHANGE && !refreshing_) {
        if (id == IdProfileList) {
            const auto index = listbox_selected_data(source);
            if (index >= 0 &&
                confirm_authoring_selection_change(Page::Profiles, index)) {
                select_profile(index);
            }
        } else if (id == IdGroupList) {
            const auto index = listbox_selected_data(source);
            if (index >= 0 &&
                confirm_authoring_selection_change(Page::Groups, index)) {
                select_group(index);
            }
        } else if (id == IdLookList) {
            const auto index = listbox_selected_data(source);
            if (index >= 0 &&
                confirm_authoring_selection_change(Page::Looks, index)) {
                select_look(index);
            }
        } else if (id == IdAutoloopList) {
            const auto index = listbox_selected_data(source);
            if (index >= 0 &&
                confirm_authoring_selection_change(Page::Autoloops, index)) {
                select_autoloop(index);
            }
        } else if (id == IdTrackList) {
            const auto index = listbox_selected_data(source);
            if (index >= 0 &&
                confirm_authoring_selection_change(Page::Tracks, index)) {
                select_track(index);
            }
        } else if (id == IdOverridesFixture) {
            refresh_override_properties();
        } else if (id == IdProfileCatalogResults) {
            refresh_fixture_catalog_controls();
        }
        return;
    }
    if (id == IdAuthoringSearch && notification == EN_CHANGE &&
        !refreshing_ && source != nullptr) {
        const auto page_window = ::GetParent(source);
        const auto page = std::find(pages_.begin(), pages_.end(), page_window);
        if (page != pages_.end()) {
            const auto page_kind = static_cast<Page>(
                std::distance(pages_.begin(), page));
            if (is_authoring_page(page_kind)) {
                refresh_authoring_collection(page_kind);
            }
        }
        return;
    }
    if (id == IdMidiAction && notification == CBN_SELCHANGE && !refreshing_) {
        update_midi_targets();
        return;
    }
    if (id == IdMidiTarget && notification == CBN_SELCHANGE && !refreshing_) {
        refresh_midi_named_choices();
        return;
    }
    if (id == IdAutoscriptFunctionTarget &&
        notification == CBN_SELCHANGE && !refreshing_) {
        refresh_autoscript_function_choices();
        return;
    }
    if (id == IdLookTarget && notification == CBN_SELCHANGE && !refreshing_) {
        refresh_look_capabilities();
        refresh_look_draft_view();
        update_physical_static_look_preview_if_active();
        return;
    }
    if (id == IdOverridesNamedChoice && notification == CBN_SELCHANGE &&
        !refreshing_) {
        const auto page = pages_[static_cast<std::size_t>(Page::Overrides)];
        const auto selected = combo_selected_data(
            ::GetDlgItem(page, IdOverridesNamedChoice), -1);
        if (selected >= 0 &&
            static_cast<std::size_t>(selected) <
                override_control_choices_.size()) {
            const auto& choice = override_control_choices_[
                static_cast<std::size_t>(selected)];
            combo_select_data(
                ::GetDlgItem(page, IdOverridesProperty),
                static_cast<std::intptr_t>(choice.property));
            set_page_message(
                Page::Overrides,
                IdOverridesMessage,
                choice.kind == emberlights::FixtureControlChoiceKind::DirectAttribute
                    ? "Direct profile attribute selected. The 0–100 control spans the complete semantic channel; the profile owns channel order, encoding, and DMX realization."
                    : (choice.behavior == showcore::ChannelCapabilityBehavior::Continuous
                           ? "Named capability range selected. The 0–100 control chooses a position inside its documented DMX range."
                           : "Named capability slot selected. Apply Fixture Attribute uses the profile's exact preferred DMX value; the percentage control is ignored."));
        }
        return;
    }
    if (id == IdLookNamedChoice && notification == CBN_SELCHANGE &&
        !refreshing_) {
        const auto page = pages_[static_cast<std::size_t>(Page::Looks)];
        const auto selected = combo_selected_data(
            ::GetDlgItem(page, IdLookNamedChoice), -1);
        if (selected >= 0 &&
            static_cast<std::size_t>(selected) < look_control_choices_.size()) {
            const auto& choice = look_control_choices_[
                static_cast<std::size_t>(selected)];
            combo_select_data(
                ::GetDlgItem(page, IdLookProperty),
                static_cast<std::intptr_t>(choice.property));
            set_page_message(
                Page::Looks,
                IdLookMessage,
                choice.kind == emberlights::FixtureControlChoiceKind::DirectAttribute
                    ? "Direct profile attribute selected. Level spans the complete semantic channel and preserves the profile's exact DMX realization."
                    : (choice.behavior == showcore::ChannelCapabilityBehavior::Continuous
                           ? "Named capability range selected. Level / range position chooses 0–100 inside the documented range."
                           : "Named capability slot selected. Use Fixture Attribute writes the exact profile-backed selection; the percentage field is ignored."));
        }
        return;
    }
    if (id == IdMidiNamedChoice && notification == CBN_SELCHANGE &&
        !refreshing_) {
        const auto page = pages_[static_cast<std::size_t>(Page::Midi)];
        const auto selected = combo_selected_data(
            ::GetDlgItem(page, IdMidiNamedChoice), -1);
        if (selected >= 0 &&
            static_cast<std::size_t>(selected) < midi_named_choices_.size()) {
            const auto& choice =
                midi_named_choices_[static_cast<std::size_t>(selected)];
            combo_select_data(
                ::GetDlgItem(page, IdMidiProperty),
                static_cast<std::intptr_t>(choice.property));
            combo_select_data(
                ::GetDlgItem(page, IdMidiBehavior),
                static_cast<std::intptr_t>(
                    choice.behavior ==
                            showcore::ChannelCapabilityBehavior::Continuous
                        ? showcore::MappingBehavior::Continuous
                        : showcore::MappingBehavior::Momentary));
            set_page_message(
                Page::Midi,
                IdMidiMessage,
                choice.kind == emberlights::FixtureControlChoiceKind::DirectAttribute
                    ? "Direct profile attribute selected. MIDI Learn preserves 0–100 semantic endpoints, scaling/curve settings, soft takeover, and stable profile identity."
                    : (choice.behavior ==
                               showcore::ChannelCapabilityBehavior::Continuous
                           ? "Named profile range selected. MIDI Learn preserves the exact normalized endpoints and stable capability identity."
                           : "Named profile slot selected. MIDI Learn binds the exact profile-backed function value, not a guessed raw channel."));
        }
        return;
    }
    if (id == IdOverridesProperty && notification == CBN_SELCHANGE &&
        !refreshing_) {
        combo_select_data(
            ::GetDlgItem(
                pages_[static_cast<std::size_t>(Page::Overrides)],
                IdOverridesNamedChoice),
            -1);
        return;
    }
    if (id == IdLookProperty && notification == CBN_SELCHANGE && !refreshing_) {
        combo_select_data(
            ::GetDlgItem(
                pages_[static_cast<std::size_t>(Page::Looks)],
                IdLookNamedChoice),
            -1);
        return;
    }
    if (id == IdMidiProperty && notification == CBN_SELCHANGE && !refreshing_) {
        combo_select_data(
            ::GetDlgItem(
                pages_[static_cast<std::size_t>(Page::Midi)],
                IdMidiNamedChoice),
            -1);
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
    if ((id == IdProfileFootprint || id == IdProfileName) &&
        notification == EN_KILLFOCUS && !refreshing_) {
        refresh_profile_mapping_summary();
        return;
    }
    if (id == IdOverridesValue && notification == EN_CHANGE && !refreshing_) {
        float value = 0.0F;
        if (parse_number(
                control_text(::GetDlgItem(
                    pages_[static_cast<std::size_t>(Page::Overrides)],
                    IdOverridesValue)),
                value) &&
            value >= 0.0F && value <= 100.0F) {
            static_cast<void>(::SendMessageW(
                ::GetDlgItem(
                    pages_[static_cast<std::size_t>(Page::Overrides)],
                    IdOverridesSlider),
                TBM_SETPOS,
                TRUE,
                static_cast<LPARAM>(std::lround(value))));
        }
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
    case IdFileImportSoundSwitch: import_soundswitch_v1_dialog(); break;
    case IdFileInspectSoundSwitch: inspect_soundswitch_dialog(); break;
    case IdFileReviewSoundSwitch: review_soundswitch_migration_dialog(); break;
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
            "Default 2.2 operator preview with a searchable Studio authoring workbench.\n"
            "Unsigned Windows testing build.\n\nCommit: " + std::string(emberlights::kCommit));
        ::MessageBoxW(
            window_,
            about.c_str(),
            L"About EmberLights",
            MB_OK | MB_ICONINFORMATION);
        break;
    }
    case IdOverridesApply: apply_fixture_override(true); break;
    case IdOverridesApplyNamed: apply_named_fixture_override(); break;
    case IdOverridesRelease: apply_fixture_override(false); break;
    case IdOverridesReleaseAll: clear_fixture_overrides(); break;
    case IdOverridesZero:
    case IdOverridesQuarter:
    case IdOverridesHalf:
    case IdOverridesFull: {
        const auto value = id == IdOverridesZero ? 0
            : id == IdOverridesQuarter ? 25
            : id == IdOverridesHalf ? 50
            : 100;
        const auto page = pages_[static_cast<std::size_t>(Page::Overrides)];
        set_control_text(::GetDlgItem(page, IdOverridesValue), number_text(value));
        static_cast<void>(::SendMessageW(
            ::GetDlgItem(page, IdOverridesSlider), TBM_SETPOS, TRUE, value));
        if (runner_.status().state == emberlights::RunnerState::Running &&
            !physical_preview_.status().owns_runner) {
            const auto named = combo_selected_data(
                ::GetDlgItem(page, IdOverridesNamedChoice), -1);
            if (named >= 0 &&
                static_cast<std::size_t>(named) <
                    override_control_choices_.size()) {
                apply_named_fixture_override();
            } else {
                apply_fixture_override(true);
            }
        } else {
            set_page_message(
                Page::Overrides,
                IdOverridesMessage,
                "Value selected. Start the show before applying a Live Override.");
        }
        break;
    }
    case IdProfileEnsureIr4: ensure_ir4_profiles(); break;
    case IdProfileApplyTemplate: apply_profile_template(); break;
    case IdProfileCatalogSearch: search_fixture_catalog(); break;
    case IdProfileCatalogImport: import_selected_catalog_fixture(); break;
    case IdProfileMappingApply: apply_profile_mapping_row(); break;
    case IdProfileMappingDefaults: apply_profile_mapping_defaults(); break;
    case IdProfileMappingDelete: delete_profile_mapping_row(); break;
    case IdProfileCapabilitiesOpen: open_profile_capability_editor(); break;
    case IdCapabilityNew: new_profile_capability(); break;
    case IdCapabilityUpsert: upsert_profile_capability(); break;
    case IdCapabilityRemove: remove_profile_capability(); break;
    case IdCapabilitySaveMetadata: save_profile_channel_metadata(); break;
    case IdCapabilityClose: close_profile_capability_editor(); break;
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
            if (index < live_autoloop_items_.size()) {
                emberlights::UiCommandInvocation invocation;
                invocation.command = emberlights::UiCommandId::AutoloopLaunch;
                invocation.target_id = live_autoloop_items_[index].id;
                invocation.autoloop_address =
                    live_autoloop_items_[index].address;
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
    case IdProfileNew:
        if (confirm_authoring_selection_change(Page::Profiles, -1)) {
            new_profile();
        }
        break;
    case IdProfileImportQlc: import_qlc_fixture_dialog(); break;
    case IdProfileDuplicate: {
        const auto reopen_workbench = source != nullptr &&
            ::GetParent(source) == profile_channel_workbench_;
        duplicate_profile();
        if (reopen_workbench) {
            open_profile_channel_workbench();
        }
        break;
    }
    case IdProfileSave: {
        const auto from_workbench = source != nullptr &&
            ::GetParent(source) == profile_channel_workbench_;
        save_profile();
        if (from_workbench && profile_channel_workbench_ != nullptr &&
            ::IsWindowVisible(profile_channel_workbench_) != FALSE) {
            set_control_text(
                ::GetDlgItem(
                    profile_channel_workbench_, IdChannelWorkbenchMessage),
                control_text(::GetDlgItem(
                    pages_[static_cast<std::size_t>(Page::Profiles)],
                    IdProfileMessage)));
        }
        break;
    }
    case IdProfileDelete: delete_profile(); break;
    case IdProfileChannelWorkbench: open_profile_channel_workbench(); break;
    case IdChannelWorkbenchAddNext: add_next_profile_channel(); break;
    case IdChannelWorkbenchFillGaps: fill_profile_channel_gaps(); break;
    case IdChannelWorkbenchSwap: swap_profile_channel_functions(); break;
    case IdChannelWorkbenchNamedRanges:
        open_profile_capability_editor();
        if (profile_capability_window_ == nullptr ||
            ::IsWindowVisible(profile_capability_window_) == FALSE) {
            set_control_text(
                ::GetDlgItem(
                    profile_channel_workbench_, IdChannelWorkbenchMessage),
                control_text(::GetDlgItem(
                    pages_[static_cast<std::size_t>(Page::Profiles)],
                    IdProfileMessage)));
        }
        break;
    case IdChannelWorkbenchDone: close_profile_channel_workbench(); break;
    case IdPatchNew:
        if (confirm_authoring_selection_change(Page::Patch, -1)) {
            new_fixture();
        }
        break;
    case IdPatchSave: save_fixture(); break;
    case IdPatchDelete: delete_fixture(); break;
    case IdGroupNew:
        if (confirm_authoring_selection_change(Page::Groups, -1)) {
            new_group();
        }
        break;
    case IdGroupDuplicate: duplicate_group(); break;
    case IdGroupSave: save_group(); break;
    case IdGroupDelete: delete_group(); break;
    case IdLookNew:
        if (confirm_authoring_selection_change(Page::Looks, -1)) {
            new_look();
        }
        break;
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
    case IdLookApplyNamed: apply_static_look_control_choice(); break;
    case IdLookRemoveProperty: remove_static_look_property(); break;
    case IdLookPreview: preview_static_look(); break;
    case IdLookPhysicalPreview: begin_or_update_physical_static_look_preview(); break;
    case IdLookPhysicalStop: stop_physical_static_look_preview(true); break;
    case IdAutoloopNew:
        if (confirm_authoring_selection_change(Page::Autoloops, -1)) {
            new_autoloop();
        }
        break;
    case IdAutoloopDuplicate: duplicate_autoloop(); break;
    case IdAutoloopSave: save_autoloop(); break;
    case IdAutoloopDelete: delete_autoloop(); break;
    case IdAutoloopNextEmpty: move_autoloop_to_next_empty(); break;
    case IdAutoloopSwapTarget: swap_autoloop_into_target_slot(); break;
    case IdAutoloopAddStep: add_autoloop_step(); break;
    case IdAutoloopRemoveLastStep: remove_last_autoloop_step(); break;
    case IdAutoloopClearSteps: clear_autoloop_steps(); break;
    case IdAutoscriptGenerate: generate_autoscript_proposal(); break;
    case IdAutoscriptPreviewStart: preview_autoscript_phase(0.0); break;
    case IdAutoscriptPreviewMiddle: preview_autoscript_phase(0.5); break;
    case IdAutoscriptCommit: commit_autoscript_proposal(); break;
    case IdAutoscriptDiscard: discard_autoscript_proposal(); break;
    case IdAutoscriptFunctionApply: apply_autoscript_fixture_function(); break;
    case IdTrackNew:
        if (confirm_authoring_selection_change(Page::Tracks, -1)) {
            new_track();
        }
        break;
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
    stop_physical_static_look_preview(false);
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

    stop_physical_static_look_preview(false);
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

void Application::search_fixture_catalog() {
    if (fixture_catalog_busy_) {
        return;
    }
    const auto page = pages_[static_cast<std::size_t>(Page::Profiles)];
    const auto query = trim(control_text(
        ::GetDlgItem(page, IdProfileCatalogQuery)));
    if (query.empty()) {
        set_page_message(
            Page::Profiles,
            IdProfileMessage,
            "Enter a manufacturer or fixture model to search the official Open Fixture Library.",
            true);
        return;
    }
    if (fixture_catalog_worker_.joinable()) {
        fixture_catalog_worker_.join();
    }
    fixture_catalog_busy_ = true;
    set_control_text(
        ::GetDlgItem(page, IdProfileCatalogStatus),
        "SEARCHING OFFICIAL CATALOG... This stays outside Live and the DMX scheduler.");
    refresh_fixture_catalog_controls();
    set_status(L"Searching the official Open Fixture Library...");
    const auto destination = window_;
    fixture_catalog_worker_ = std::thread([this, destination, query] {
        emberlights::OpenFixtureLibrarySearchResult result;
        try {
            result = emberlights::search_open_fixture_library(query);
        } catch (...) {
            result.query = query;
            result.issues.push_back({
                emberlights::FixtureCatalogIssueSeverity::Error,
                "ofl.searchUnexpected",
                "The fixture catalog search stopped after an unexpected local error."});
        }
        {
            std::lock_guard lock(fixture_catalog_mutex_);
            pending_fixture_catalog_search_ = std::move(result);
        }
        static_cast<void>(::PostMessageW(
            destination, kFixtureCatalogSearchCompleteMessage, 0, 0));
    });
}

void Application::complete_fixture_catalog_search() {
    if (fixture_catalog_worker_.joinable()) {
        fixture_catalog_worker_.join();
    }
    std::optional<emberlights::OpenFixtureLibrarySearchResult> completed;
    {
        std::lock_guard lock(fixture_catalog_mutex_);
        completed = std::move(pending_fixture_catalog_search_);
        pending_fixture_catalog_search_.reset();
    }
    fixture_catalog_busy_ = false;
    const auto page = pages_[static_cast<std::size_t>(Page::Profiles)];
    const auto list = ::GetDlgItem(page, IdProfileCatalogResults);
    static_cast<void>(::SendMessageW(list, LB_RESETCONTENT, 0, 0));
    fixture_catalog_results_.clear();
    if (!completed.has_value()) {
        set_control_text(
            ::GetDlgItem(page, IdProfileCatalogStatus),
            "CATALOG SEARCH FAILED • no project data changed");
        refresh_fixture_catalog_controls();
        return;
    }
    fixture_catalog_results_ = std::move(completed->entries);
    for (std::size_t index = 0U; index < fixture_catalog_results_.size(); ++index) {
        const auto& entry = fixture_catalog_results_[index];
        listbox_add(
            list,
            widen(entry.display_name + "  •  " + entry.key),
            static_cast<std::intptr_t>(index));
    }
    if (!fixture_catalog_results_.empty()) {
        static_cast<void>(::SendMessageW(list, LB_SETCURSEL, 0, 0));
    }
    std::string status;
    bool error = false;
    if (!completed->issues.empty()) {
        status = completed->issues.front().message;
        error = completed->issues.front().severity ==
            emberlights::FixtureCatalogIssueSeverity::Error;
    } else if (fixture_catalog_results_.empty()) {
        status = "No exact catalog matches. Try the manufacturer and model from the fixture label or manual.";
    } else {
        status = "Found " + number_text(fixture_catalog_results_.size()) +
            " official result(s). Select the exact fixture, then Download + Import Selected.";
    }
    set_control_text(::GetDlgItem(page, IdProfileCatalogStatus), status);
    set_page_message(Page::Profiles, IdProfileMessage, status, error);
    set_status(error
        ? L"Open Fixture Library search failed safely; no project data changed."
        : L"Official fixture catalog search complete.");
    refresh_fixture_catalog_controls();
}

void Application::import_selected_catalog_fixture() {
    if (fixture_catalog_busy_) {
        return;
    }
    const auto page = pages_[static_cast<std::size_t>(Page::Profiles)];
    const auto list = ::GetDlgItem(page, IdProfileCatalogResults);
    const auto selected = static_cast<int>(::SendMessageW(list, LB_GETCURSEL, 0, 0));
    if (selected == LB_ERR) {
        set_page_message(
            Page::Profiles,
            IdProfileMessage,
            "Select an exact fixture from the official catalog results first.",
            true);
        return;
    }
    const auto index = static_cast<std::size_t>(::SendMessageW(
        list, LB_GETITEMDATA, selected, 0));
    if (index >= fixture_catalog_results_.size()) {
        return;
    }
    bool unsaved_profile_draft = false;
    emberlights::FixtureProfileDefinition editor_channel_draft;
    editor_channel_draft.channels = profile_draft_channels_;
    const auto editor_channels = normalize_newlines(
        profile_channels_text(editor_channel_draft));
    if (profile_index_ >= 0 &&
        static_cast<std::size_t>(profile_index_) < project_.fixture_profiles.size()) {
        const auto& source =
            project_.fixture_profiles[static_cast<std::size_t>(profile_index_)];
        unsaved_profile_draft =
            trim(control_text(::GetDlgItem(page, IdProfileManufacturer))) !=
                source.manufacturer ||
            trim(control_text(::GetDlgItem(page, IdProfileModel))) != source.model ||
            trim(control_text(::GetDlgItem(page, IdProfileMode))) != source.mode ||
            trim(control_text(::GetDlgItem(page, IdProfileName))) != source.name ||
            trim(control_text(::GetDlgItem(page, IdProfileFootprint))) !=
                number_text(source.footprint) ||
            editor_channels !=
                normalize_newlines(profile_channels_text(source));
    } else {
        emberlights::FixtureProfileDefinition default_draft;
        static_cast<void>(emberlights::apply_fixture_profile_template(
            default_draft,
            emberlights::FixtureProfileTemplateId::Rgb3));
        unsaved_profile_draft =
            !trim(control_text(::GetDlgItem(page, IdProfileManufacturer))).empty() ||
            !trim(control_text(::GetDlgItem(page, IdProfileModel))).empty() ||
            !trim(control_text(::GetDlgItem(page, IdProfileMode))).empty() ||
            !trim(control_text(::GetDlgItem(page, IdProfileName))).empty() ||
            trim(control_text(::GetDlgItem(page, IdProfileFootprint))) != "3" ||
            editor_channels != normalize_newlines(
                profile_channels_text(default_draft));
    }
    if (unsaved_profile_draft) {
        set_page_message(
            Page::Profiles,
            IdProfileMessage,
            "Save or discard the current profile draft before importing a catalog fixture.",
            true);
        return;
    }
    if (fixture_catalog_worker_.joinable()) {
        fixture_catalog_worker_.join();
    }
    const auto entry = fixture_catalog_results_[index];
    const auto project_snapshot = emberlights::serialize_project(project_);
    const auto profile_editor_snapshot = current_profile_editor_snapshot();
    fixture_catalog_busy_ = true;
    set_control_text(
        ::GetDlgItem(page, IdProfileCatalogStatus),
        "DOWNLOADING + CONVERTING... Exact source evidence will be retained.");
    refresh_fixture_catalog_controls();
    set_status(L"Downloading the selected official fixture snapshot...");
    const auto destination = window_;
    fixture_catalog_worker_ = std::thread(
        [this, destination, entry, project_snapshot, profile_editor_snapshot] {
            PendingFixtureCatalogDownload completed;
            completed.project_snapshot = project_snapshot;
            completed.profile_editor_snapshot = profile_editor_snapshot;
            try {
                completed.result =
                    emberlights::download_open_fixture_library_fixture(entry);
            } catch (...) {
                completed.result.entry = entry;
                completed.result.issues.push_back({
                    emberlights::FixtureCatalogIssueSeverity::Error,
                    "ofl.downloadUnexpected",
                    "The fixture download stopped after an unexpected local error."});
            }
            {
                std::lock_guard lock(fixture_catalog_mutex_);
                pending_fixture_catalog_download_ = std::move(completed);
            }
            static_cast<void>(::PostMessageW(
                destination, kFixtureCatalogDownloadCompleteMessage, 0, 0));
        });
}

void Application::complete_fixture_catalog_download() {
    if (fixture_catalog_worker_.joinable()) {
        fixture_catalog_worker_.join();
    }
    std::optional<PendingFixtureCatalogDownload> completed;
    {
        std::lock_guard lock(fixture_catalog_mutex_);
        completed = std::move(pending_fixture_catalog_download_);
        pending_fixture_catalog_download_.reset();
    }
    fixture_catalog_busy_ = false;
    const auto page = pages_[static_cast<std::size_t>(Page::Profiles)];
    if (!completed.has_value()) {
        set_control_text(
            ::GetDlgItem(page, IdProfileCatalogStatus),
            "CATALOG IMPORT FAILED • no project data changed");
        refresh_fixture_catalog_controls();
        return;
    }
    if (completed->project_snapshot != emberlights::serialize_project(project_)) {
        const std::string message =
            "The project changed while the fixture downloaded, so EmberLights discarded it. Select the result and import again.";
        set_control_text(::GetDlgItem(page, IdProfileCatalogStatus), message);
        set_page_message(Page::Profiles, IdProfileMessage, message, true);
        set_status(L"Catalog result discarded because the project changed; nothing was imported.");
        refresh_fixture_catalog_controls();
        return;
    }
    if (completed->profile_editor_snapshot != current_profile_editor_snapshot()) {
        const std::string message =
            "The profile draft changed while the fixture downloaded, so EmberLights discarded the import and kept your editor work. Import again after saving or discarding the draft.";
        set_control_text(::GetDlgItem(page, IdProfileCatalogStatus), message);
        set_page_message(Page::Profiles, IdProfileMessage, message, true);
        set_status(L"Catalog result discarded so newer profile-editor work was not overwritten.");
        refresh_fixture_catalog_controls();
        return;
    }

    auto& downloaded = completed->result;
    const auto before = project_;
    auto candidate = project_;
    std::size_t added = 0U;
    std::size_t duplicates = 0U;
    std::size_t rejected = 0U;
    std::int32_t first_added = -1;
    for (std::size_t index = 0U; index < downloaded.imported.profiles.size(); ++index) {
        const auto& profile = downloaded.imported.profiles[index];
        const auto duplicate = std::find_if(
            candidate.fixture_profiles.begin(),
            candidate.fixture_profiles.end(),
            [&](const auto& existing) { return existing.id == profile.id; });
        if (duplicate != candidate.fixture_profiles.end()) {
            ++duplicates;
            continue;
        }
        candidate.fixture_profiles.push_back(profile);
        const bool has_evidence = index < downloaded.project_evidence_records.size();
        if (has_evidence) {
            candidate.unknown_records.push_back(
                downloaded.project_evidence_records[index]);
        }
        const auto validation = emberlights::validate_project(candidate);
        if (!validation.ok()) {
            candidate.fixture_profiles.pop_back();
            if (has_evidence) {
                candidate.unknown_records.pop_back();
            }
            ++rejected;
            continue;
        }
        if (first_added < 0) {
            first_added = static_cast<std::int32_t>(
                candidate.fixture_profiles.size() - 1U);
        }
        ++added;
    }
    if (added > 0U) {
        project_ = std::move(candidate);
        mark_dirty();
        refresh_profiles();
        refresh_patch();
        const auto profiles = ::GetDlgItem(page, IdProfileList);
        static_cast<void>(::SendMessageW(
            profiles, LB_SETCURSEL, first_added, 0));
        select_profile(first_added);
        record_project_edit(before);
    }

    std::ostringstream report;
    report << "Official Open Fixture Library import\r\n\r\n"
           << "Fixture: " << downloaded.entry.display_name << "\r\n"
           << "Converted modes: " << downloaded.imported.profiles.size() << "\r\n"
           << "Added to project: " << added << "\r\n"
           << "Already present: " << duplicates << "\r\n"
           << "Rejected by validation: " << rejected << "\r\n"
           << "Importer warnings: " << downloaded.imported.warning_count() << "\r\n"
           << "Quarantined/errors: " << downloaded.imported.error_count() << "\r\n";
    if (!downloaded.source_content_sha256.empty()) {
        report << "Exact QXF SHA-256: " << downloaded.source_content_sha256 << "\r\n";
    }
    if (!downloaded.issues.empty()) {
        report << "\r\nReview:\r\n";
        const auto shown = std::min<std::size_t>(downloaded.issues.size(), 5U);
        for (std::size_t index = 0U; index < shown; ++index) {
            report << downloaded.issues[index].message << "\r\n";
        }
    }
    report << "\r\nImported snapshots are read-only and unreviewed. Verify the exact mode, official DMX chart, and physical fixture before Live use.";
    const auto has_catalog_error = std::any_of(
        downloaded.issues.begin(), downloaded.issues.end(), [](const auto& issue) {
            return issue.severity ==
                emberlights::FixtureCatalogIssueSeverity::Error;
        });
    const auto report_wide = widen(report.str());
    ::MessageBoxW(
        window_,
        report_wide.c_str(),
        L"Official fixture catalog import",
        MB_OK | ((has_catalog_error || added == 0U)
                     ? MB_ICONWARNING
                     : MB_ICONINFORMATION));
    const auto status = added > 0U
        ? "Imported " + number_text(added) +
            " unreviewed mode snapshot(s). Patch one, then verify the physical channel map."
        : "No fixture modes were added. Review the import report; no project data changed.";
    set_control_text(::GetDlgItem(page, IdProfileCatalogStatus), status);
    set_page_message(Page::Profiles, IdProfileMessage, status, added == 0U);
    set_status(added > 0U
        ? L"Official fixture snapshot imported with source evidence; physical review is still required."
        : L"Official fixture import added nothing; the project was left unchanged.");
    refresh_fixture_catalog_controls();
}

void Application::import_soundswitch_v1_dialog() {
    if (!maybe_save_changes()) {
        return;
    }
    const auto source = choose_folder(
        window_,
        L"Select an extracted SoundSwitch 2.10.x project directory");
    if (!source.has_value()) {
        return;
    }
    set_status(
        L"Building a conservative, output-disabled SoundSwitch migration candidate...");
    static_cast<void>(::UpdateWindow(window_));
    const auto migration = emberlights::create_soundswitch_v1_project(*source);
    if (!migration) {
        const auto message = widen(migration.message);
        ::MessageBoxW(
            window_,
            message.c_str(),
            L"SoundSwitch V1 preview could not import this source",
            MB_OK | MB_ICONWARNING);
        set_status(
            L"SoundSwitch source was left unchanged. This preview only accepts its qualified 2.10.x color-rig shape.");
        return;
    }

    std::wostringstream review;
    review << widen(migration.message) << L"\n\n"
           << L"OUTPUTS: disabled\n"
           << L"Profiles: " << migration.project.fixture_profiles.size()
           << L"    Fixtures: " << migration.project.fixtures.size()
           << L"    Static Looks: " << migration.project.looks.size()
           << L"    Autoloops: " << migration.project.autoloops.size() << L"\n\n"
           << L"Important review limits:\n";
    for (const auto& warning : migration.warnings) {
        review << L"• " << widen(warning) << L"\n";
    }
    review << L"\nCreate this as a separate EmberLights project now?";
    if (::MessageBoxW(
            window_,
            review.str().c_str(),
            L"Review SoundSwitch V1 migration candidate",
            MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) {
        set_status(L"SoundSwitch migration candidate was not saved; the source was unchanged.");
        return;
    }

    std::array<wchar_t, 32768> selected{};
    const auto suggested = widen(
        slugify(migration.project.name) + std::string(emberlights::kProjectExtension));
    std::copy_n(
        suggested.c_str(),
        std::min(suggested.size(), selected.size() - 1U),
        selected.begin());
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window_;
    dialog.lpstrFilter =
        L"EmberLights Projects (*.emberlights)\0*.emberlights\0All Files\0*.*\0";
    dialog.lpstrFile = selected.data();
    dialog.nMaxFile = static_cast<DWORD>(selected.size());
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    dialog.lpstrDefExt = L"emberlights";
    if (::GetSaveFileNameW(&dialog) == FALSE) {
        set_status(L"SoundSwitch migration candidate was not saved; the source was unchanged.");
        return;
    }
    const std::filesystem::path destination(selected.data());
    const auto saved =
        emberlights::save_project_atomic(destination, migration.project, false);
    if (!saved) {
        const auto message = widen(saved.message);
        ::MessageBoxW(
            window_, message.c_str(), L"Could not save migration candidate",
            MB_OK | MB_ICONERROR);
        return;
    }

    stop_physical_static_look_preview(false);
    runner_.stop();
    ui_commands_.set_active_project(nullptr);
    active_project_.reset();
    project_ = migration.project;
    current_path_ = destination;
    static_cast<void>(remember_project_path(current_path_));
    edit_history_.clear();
    capture_saved_project();
    reset_authoring_selection();
    update_edit_menu();
    refresh_all();
    show_page(Page::Profiles);
    ::MessageBoxW(
        window_,
        L"The separate, output-disabled migration project is open. Start in Fixture Profiles "
        L"and Fixture Patch, verify physical modes/addresses, then review Static Looks and "
        L"Autoloops. Nothing is qualified for live output merely because it imported.",
        L"SoundSwitch migration candidate created",
        MB_OK | MB_ICONINFORMATION);
    set_status(
        L"Migration candidate open: verify profiles and patch first, then use File > Review Current SoundSwitch Migration.");
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

void Application::review_soundswitch_migration_dialog() {
    const auto source = choose_folder(
        window_,
        L"Select the exact SoundSwitch source used for this EmberLights migration");
    if (!source.has_value()) {
        return;
    }
    set_status(
        L"Auditing source identity, project validity, disabled outputs, and migrated areas...");
    static_cast<void>(::UpdateWindow(window_));
    const auto inspection = emberlights::inspect_soundswitch_project(*source);
    const auto audit = emberlights::audit_soundswitch_source_binding(project_, inspection);
    const auto portability =
        emberlights::build_migration_portability_review(audit);

    std::wostringstream review;
    review << L"SOUNDSWITCH MIGRATION REVIEW\n\n"
           << widen(audit.review_headline) << L"\n"
           << L"Review state: "
           << widen(emberlights::soundswitch_migration_review_state_name(
                  audit.review_state))
           << L"\nSource identity: "
           << widen(emberlights::soundswitch_source_binding_status_name(audit.status))
           << L"\nProject validation: " << (audit.project_valid ? L"PASS" : L"BLOCKED")
           << L"\nPhysical outputs: " << (audit.outputs_disabled ? L"DISABLED" : L"ENABLED — BLOCKED")
           << L"\nSemantic import qualified: NO (source hashes prove identity only)\n\n";
    for (const auto& area : audit.review_areas) {
        review << widen(area.label) << L" — "
               << widen(emberlights::soundswitch_migration_area_state_name(area.state))
               << L"\n  Source: " << area.source_item_count
               << L"    Project: " << area.project_item_count
               << L"\n  " << widen(area.detail) << L"\n\n";
    }
    if (!audit.review_action_codes.empty()) {
        review << L"NEXT ACTIONS\n";
        for (const auto& action : audit.review_action_codes) {
            review << L"• " << widen(action) << L"\n";
        }
    }
    review << L"\nPORTABILITY PIPELINE\n"
           << L"Probe → Inventory → Decode → Reconcile → Plan → Commit → Upgrade\n";
    for (const auto& source_review : portability.sources) {
        review << L"\n" << widen(source_review.label)
               << (source_review.research_only ? L" — RESEARCH ONLY" : L"")
               << L"\n  Artifact identity: "
               << (source_review.artifact_identity_verified ? L"verified" : L"not verified")
               << L"  Semantic decoder: "
               << (source_review.semantic_decoder_qualified ? L"qualified" : L"not qualified")
               << L"  Import claim: "
               << (source_review.semantic_import_claimed ? L"yes" : L"no")
               << L"\n";
        std::vector<std::string> blockers;
        for (const auto& stage : source_review.stages) {
            review << L"  "
                   << widen(emberlights::migration_portability_stage_name(stage.stage))
                   << L": "
                   << widen(emberlights::migration_portability_readiness_name(
                          stage.readiness))
                   << L" / "
                   << widen(emberlights::migration_portability_evidence_tier_name(
                          stage.evidence_tier))
                   << L"\n";
            for (const auto& blocker : stage.blocker_codes) {
                if (std::find(blockers.begin(), blockers.end(), blocker) ==
                    blockers.end()) {
                    blockers.push_back(blocker);
                }
            }
        }
        if (!blockers.empty()) {
            review << L"  Blockers:";
            const auto shown = std::min<std::size_t>(blockers.size(), 8U);
            for (std::size_t index = 0U; index < shown; ++index) {
                review << L"\n    • " << widen(blockers[index]);
            }
            if (blockers.size() > shown) {
                review << L"\n    • … " << blockers.size() - shown
                       << L" more";
            }
            review << L"\n";
        }
    }
    const auto ready = audit.review_state ==
        emberlights::SoundSwitchMigrationReviewState::ReadyForManualReview;
    ::MessageBoxW(
        window_,
        review.str().c_str(),
        L"Migration portability review",
        MB_OK | (ready ? MB_ICONINFORMATION : MB_ICONWARNING));
    set_status(
        ready
            ? L"Source identity and safety gates pass; each approximated area still needs manual/physical review."
            : L"Migration review found a source, validation, or output-safety blocker. No source file was changed.");
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
    stop_physical_static_look_preview(false);
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
    if (physical_preview_.status().owns_runner) {
        stop_physical_static_look_preview(false);
        set_status(
            L"Studio hardware preview stopped before saving; explicit zero frames were sent.");
    }
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
        auto compilation =
            emberlights::compile_project_with_persisted_autoloops(project_);
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
    const auto compilation =
        emberlights::compile_project_with_persisted_autoloops(project_);
    const auto& validation = compilation.validation;
    refresh_diagnostics();
    if (validation.ok()) {
        if (show_success) {
            ::MessageBoxW(
                window_,
                L"Project validation passed. Fixture profiles, patch, Static Looks, persisted "
                L"V2 Autoloops, and current limits compile through the production path.",
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
    if (physical_preview_.status().owns_runner) {
        stop_physical_static_look_preview(false);
        set_status(
            L"Studio hardware preview stopped. Click Start Show again when you are ready to enter Live.");
        return;
    }
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
    auto compilation =
        emberlights::compile_project_with_persisted_autoloops(run_project);
    bool recovered_activation = false;
    if (!compilation) {
        emberlights::ProjectDocument last_active;
        const auto loaded = emberlights::load_project(
            emberlights::project_active_path(current_path_), last_active, true);
        auto last_active_compilation = loaded
            ? emberlights::compile_project_with_persisted_autoloops(last_active)
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
              << ". The target may not support this attribute, the show may be stopped, "
                 "or a safety gate may be active.";
        set_page_message(Page::Overrides, IdOverridesMessage, error.str(), true);
        return;
    }
    std::ostringstream message;
    message << (active ? "Override queued: " : "Attribute release queued: ")
            << target_name << " — " << fixture_parameter_label(property);
    if (active) {
        message << " at " << percentage << "%";
    }
    message << ". Runner safety limits still apply.";
    set_page_message(Page::Overrides, IdOverridesMessage, message.str());
}

void Application::apply_named_fixture_override() {
    const auto page = pages_[static_cast<std::size_t>(Page::Overrides)];
    const auto selected_choice = combo_selected_data(
        ::GetDlgItem(page, IdOverridesNamedChoice), -1);
    if (selected_choice < 0 ||
        static_cast<std::size_t>(selected_choice) >=
            override_control_choices_.size()) {
        set_page_message(
            Page::Overrides,
            IdOverridesMessage,
            "Choose a profile-backed Fixture Attribute first.",
            true);
        return;
    }
    const auto fixtures = ::GetDlgItem(page, IdOverridesFixture);
    const auto selected_target = static_cast<int>(::SendMessageW(
        fixtures, LB_GETCURSEL, 0, 0));
    if (selected_target < 0) {
        set_page_message(
            Page::Overrides, IdOverridesMessage,
            "Select an active fixture or group first.", true);
        return;
    }
    const auto target_index = static_cast<std::size_t>(::SendMessageW(
        fixtures, LB_GETITEMDATA, selected_target, 0));
    if (target_index >= live_view_model_.override_targets().size()) {
        set_page_message(
            Page::Overrides, IdOverridesMessage,
            "The selected target is no longer in the active Live package.", true);
        return;
    }
    const auto& cached = override_control_choices_[
        static_cast<std::size_t>(selected_choice)];
    float position = 0.5F;
    if (cached.behavior == showcore::ChannelCapabilityBehavior::Continuous) {
        float percentage = 0.0F;
        if (!parse_number(
                control_text(::GetDlgItem(page, IdOverridesValue)), percentage) ||
            !std::isfinite(percentage) || percentage < 0.0F ||
            percentage > 100.0F) {
            set_page_message(
                Page::Overrides, IdOverridesMessage,
                "A continuous named range needs a position from 0 through 100.",
                true);
            return;
        }
        position = percentage / 100.0F;
    }

    const auto& target = live_view_model_.override_targets()[target_index];
    const auto catalog = emberlights::fixture_control_choices(
        live_project(), target.id, position);
    const auto resolved = std::find_if(
        catalog.choices.begin(), catalog.choices.end(),
        [&](const auto& choice) { return choice.id == cached.id; });
    if (resolved == catalog.choices.end() ||
        !resolved->live_override_compatible() ||
        !live_override_property_visible(
            resolved->property, live_view_model_.safety())) {
        set_page_message(
            Page::Overrides, IdOverridesMessage,
            "That Fixture Attribute cannot be represented by one safe Live group value. "
            "Use a fixture target or author it in a Static Look.", true);
        return;
    }

    combo_select_data(
        ::GetDlgItem(page, IdOverridesProperty),
        static_cast<std::intptr_t>(resolved->property));
    emberlights::UiCommandInvocation invocation;
    invocation.command =
        target.kind == emberlights::LiveOverrideTargetKind::Fixture
        ? emberlights::UiCommandId::FixtureOverridePropertySet
        : emberlights::UiCommandId::GroupOverridePropertySet;
    invocation.target_id = target.id;
    invocation.property = resolved->property;
    invocation.number_value = resolved->shared_normalized_value;
    const auto result = ui_commands_.invoke(invocation);
    if (result != emberlights::UiInvocationResult::Accepted &&
        result != emberlights::UiInvocationResult::NoChange) {
        set_page_message(
            Page::Overrides,
            IdOverridesMessage,
            "Fixture Attribute rejected: " +
                std::string(emberlights::ui_invocation_result_name(result)) +
                ". Runner availability and all normal safety gates still apply.",
            true);
        return;
    }
    std::ostringstream message;
    message << "Named override queued: " << target.name << " — "
            << fixture_parameter_label(resolved->property) << " • "
            << resolved->name;
    if (resolved->behavior ==
        showcore::ChannelCapabilityBehavior::Continuous) {
        message << " at " << static_cast<unsigned int>(
            std::lround(position * 100.0F)) << "% of its documented range";
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

}  // namespace

void Application::select_profile(std::int32_t index) {
    if (profile_capability_window_ != nullptr) {
        ::ShowWindow(profile_capability_window_, SW_HIDE);
    }
    if (profile_channel_workbench_ != nullptr) {
        ::ShowWindow(profile_channel_workbench_, SW_HIDE);
    }
    selected_profile_capability_id_.clear();
    if (index < 0 || static_cast<std::size_t>(index) >= project_.fixture_profiles.size()) {
        new_profile();
        return;
    }
    profile_index_ = index;
    profile_duplicate_source_id_.reset();
    const auto page = pages_[static_cast<std::size_t>(Page::Profiles)];
    const auto& profile = project_.fixture_profiles[static_cast<std::size_t>(index)];
    set_control_text(::GetDlgItem(page, IdProfileManufacturer), profile.manufacturer);
    set_control_text(::GetDlgItem(page, IdProfileModel), profile.model);
    set_control_text(::GetDlgItem(page, IdProfileMode), profile.mode);
    set_control_text(::GetDlgItem(page, IdProfileName), profile.name);
    set_control_text(::GetDlgItem(page, IdProfileFootprint), number_text(profile.footprint));
    profile_draft_channels_ = profile.channels;
    refresh_profile_channel_table();
    const bool editable = profile.source == showcore::FixtureProfileSource::Local;
    for (const auto id : {
             IdProfileManufacturer, IdProfileModel, IdProfileMode, IdProfileName,
             IdProfileFootprint, IdProfileSave,
             IdProfileMappingChannel, IdProfileMappingProperty,
             IdProfileMappingEncoding, IdProfileMappingFine,
             IdProfileMappingMinimum, IdProfileMappingMaximum,
             IdProfileMappingDefault, IdProfileMappingApply,
             IdProfileMappingDefaults, IdProfileMappingDelete, IdProfileTemplate,
             IdProfileApplyTemplate}) {
        ::EnableWindow(::GetDlgItem(page, id), editable ? TRUE : FALSE);
    }
    ::EnableWindow(::GetDlgItem(page, IdProfileChannels), TRUE);
    ::EnableWindow(
        ::GetDlgItem(page, IdProfileCapabilitiesOpen),
        profile_draft_channels_.empty() ? FALSE : TRUE);
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
    refresh_profile_mapping_summary();
    restore_authoring_collection_selection(Page::Profiles);
    capture_authoring_editor_baseline(Page::Profiles);
}

void Application::new_profile() {
    if (profile_capability_window_ != nullptr) {
        ::ShowWindow(profile_capability_window_, SW_HIDE);
    }
    if (profile_channel_workbench_ != nullptr) {
        ::ShowWindow(profile_channel_workbench_, SW_HIDE);
    }
    selected_profile_capability_id_.clear();
    profile_index_ = -1;
    profile_duplicate_source_id_.reset();
    const auto page = pages_[static_cast<std::size_t>(Page::Profiles)];
    set_control_text(::GetDlgItem(page, IdProfileManufacturer), "");
    set_control_text(::GetDlgItem(page, IdProfileModel), "");
    set_control_text(::GetDlgItem(page, IdProfileMode), "");
    set_control_text(::GetDlgItem(page, IdProfileName), "");
    emberlights::FixtureProfileDefinition default_draft;
    static_cast<void>(emberlights::apply_fixture_profile_template(
        default_draft,
        emberlights::FixtureProfileTemplateId::Rgb3));
    profile_draft_channels_ = std::move(default_draft.channels);
    set_control_text(::GetDlgItem(page, IdProfileFootprint), "3");
    combo_select_data(
        ::GetDlgItem(page, IdProfileTemplate),
        static_cast<std::intptr_t>(emberlights::FixtureProfileTemplateId::Rgb3));
    refresh_profile_channel_table();
    set_control_text(::GetDlgItem(page, IdProfileMappingChannel), "1");
    set_control_text(::GetDlgItem(page, IdProfileMappingFine), "0");
    set_control_text(::GetDlgItem(page, IdProfileMappingMinimum), "0");
    set_control_text(::GetDlgItem(page, IdProfileMappingMaximum), "255");
    set_control_text(::GetDlgItem(page, IdProfileMappingDefault), "0");
    combo_select_data(
        ::GetDlgItem(page, IdProfileMappingProperty),
        static_cast<std::intptr_t>(showcore::Property::Intensity));
    combo_select_data(
        ::GetDlgItem(page, IdProfileMappingEncoding),
        static_cast<std::intptr_t>(showcore::ChannelEncoding::Linear8));
    for (const auto id : {
             IdProfileManufacturer, IdProfileModel, IdProfileMode, IdProfileName,
             IdProfileFootprint, IdProfileSave,
             IdProfileMappingChannel, IdProfileMappingProperty,
             IdProfileMappingEncoding, IdProfileMappingFine,
             IdProfileMappingMinimum, IdProfileMappingMaximum,
             IdProfileMappingDefault, IdProfileMappingApply,
             IdProfileMappingDefaults, IdProfileMappingDelete, IdProfileTemplate,
             IdProfileApplyTemplate}) {
        ::EnableWindow(::GetDlgItem(page, id), TRUE);
    }
    ::EnableWindow(::GetDlgItem(page, IdProfileChannels), TRUE);
    ::EnableWindow(
        ::GetDlgItem(page, IdProfileCapabilitiesOpen),
        profile_draft_channels_.empty() ? FALSE : TRUE);
    ::EnableWindow(::GetDlgItem(page, IdProfileDelete), FALSE);
    set_page_message(Page::Profiles, IdProfileMessage,
                     "Create a local profile from the fixture's official DMX chart.");
    refresh_profile_mapping_summary();
    restore_authoring_collection_selection(Page::Profiles);
    capture_authoring_editor_baseline(Page::Profiles);
}

void Application::duplicate_profile() {
    if (profile_index_ < 0 ||
        static_cast<std::size_t>(profile_index_) >= project_.fixture_profiles.size()) {
        return;
    }
    const auto source = project_.fixture_profiles[static_cast<std::size_t>(profile_index_)];
    new_profile();
    profile_duplicate_source_id_ = source.id;
    const auto page = pages_[static_cast<std::size_t>(Page::Profiles)];
    set_control_text(::GetDlgItem(page, IdProfileManufacturer), source.manufacturer);
    set_control_text(::GetDlgItem(page, IdProfileModel), source.model);
    set_control_text(::GetDlgItem(page, IdProfileMode), source.mode + " Custom");
    set_control_text(::GetDlgItem(page, IdProfileName), source.name + " Custom");
    set_control_text(::GetDlgItem(page, IdProfileFootprint), number_text(source.footprint));
    profile_draft_channels_ = source.channels;
    refresh_profile_channel_table();
    const auto affected = static_cast<std::size_t>(std::count_if(
        project_.fixtures.begin(), project_.fixtures.end(),
        [&](const auto& fixture) {
            return fixture.profile_id == source.id;
        }));
    set_page_message(
        Page::Profiles,
        IdProfileMessage,
        "Duplicated into a new local profile. Review the rows and Save Profile; EmberLights will offer to rebind " +
            std::to_string(affected) + " patched fixture" +
            (affected == 1U ? std::string{} : std::string("s")) +
            " to this editable copy.");
    refresh_profile_mapping_summary();
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
    profile.channels = profile_draft_channels_;
    const auto mapping = emberlights::summarize_fixture_profile_mapping(profile);
    if (!mapping.profile_valid) {
        set_page_message(
            Page::Profiles,
            IdProfileMessage,
            mapping.validation_message,
            true);
        return;
    }
    profile.source = showcore::FixtureProfileSource::Local;
    profile.source_revision = "emberlights-local-v1";
    profile.id = profile_index_ >= 0
        ? project_.fixture_profiles[static_cast<std::size_t>(profile_index_)].id
        : unique_id("profile", profile.manufacturer + " " + profile.model + " " + profile.mode);
    auto candidate = project_;
    std::size_t rebound_fixture_count = 0U;
    bool rebind_duplicate = false;
    if (profile_index_ < 0 && profile_duplicate_source_id_.has_value()) {
        const auto affected = static_cast<std::size_t>(std::count_if(
            candidate.fixtures.begin(), candidate.fixtures.end(),
            [&](const auto& fixture) {
                return fixture.profile_id == *profile_duplicate_source_id_;
            }));
        if (affected != 0U) {
            std::wostringstream question;
            question << L"Save this editable Local profile and switch "
                     << affected << L" patched fixture"
                     << (affected == 1U ? L"" : L"s")
                     << L" from the source profile to this copy?\n\n"
                        L"Yes is recommended when you duplicated the profile to correct its channel map. "
                        L"No saves the profile without changing Patch. Cancel returns to the editor.";
            const auto choice = ::MessageBoxW(
                window_,
                question.str().c_str(),
                L"Save and rebind fixture profile",
                MB_YESNOCANCEL | MB_ICONQUESTION | MB_DEFBUTTON1);
            if (choice == IDCANCEL) {
                return;
            }
            rebind_duplicate = choice == IDYES;
        }
    }
    if (profile_index_ >= 0) {
        candidate.fixture_profiles[static_cast<std::size_t>(profile_index_)] = profile;
    } else {
        candidate.fixture_profiles.push_back(profile);
    }
    if (rebind_duplicate) {
        const auto rebound = emberlights::rebind_fixture_profile_instances(
            candidate, *profile_duplicate_source_id_, profile.id);
        if (!rebound) {
            set_page_message(
                Page::Profiles, IdProfileMessage, rebound.message, true);
            return;
        }
        rebound_fixture_count = rebound.fixture_ids.size();
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
    profile_duplicate_source_id_.reset();
    mark_dirty();
    refresh_profiles();
    refresh_patch();
    auto saved_message = rebound_fixture_count == 0U
        ? std::string("Fixture profile saved. Patch was unchanged.")
        : "Fixture profile saved and " +
              std::to_string(rebound_fixture_count) + " patched fixture" +
              (rebound_fixture_count == 1U ? std::string{} : std::string("s")) +
              " now uses this editable profile.";
    if (runner_.status().state == emberlights::RunnerState::Running) {
        saved_message +=
            " The running Live package is intentionally unchanged; Stop Show, then Start Show to activate the saved profile/patch revision.";
    }
    set_page_message(
        Page::Profiles,
        IdProfileMessage,
        saved_message);
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

void Application::ensure_ir4_profiles() {
    auto candidate = project_;
    const auto result =
        emberlights::ensure_manual_backed_both_lighting_bo_ir4_profiles(candidate);
    if (!result) {
        set_page_message(Page::Profiles, IdProfileMessage, result.message, true);
        return;
    }
    if (!result.six_channel_added && !result.ten_channel_added) {
        set_page_message(Page::Profiles, IdProfileMessage, result.message);
        return;
    }
    const auto validation = emberlights::validate_project(candidate);
    if (!validation.ok()) {
        set_page_message(
            Page::Profiles,
            IdProfileMessage,
            "The verified profiles could not be added safely: " +
                first_validation_error(validation),
            true);
        return;
    }
    project_ = std::move(candidate);
    mark_dirty();
    refresh_profiles();
    refresh_patch();
    set_page_message(
        Page::Profiles,
        IdProfileMessage,
        result.message +
            " Use Fixture Patch to assign the physical 6CH or 10CH mode; save the project after testing.");
}

void Application::apply_profile_template() {
    const auto page = pages_[static_cast<std::size_t>(Page::Profiles)];
    const auto selected = static_cast<emberlights::FixtureProfileTemplateId>(
        combo_selected_data(
            ::GetDlgItem(page, IdProfileTemplate),
            static_cast<std::intptr_t>(
                emberlights::FixtureProfileTemplateId::Rgbwauv6)));
    if (!profile_draft_channels_.empty() &&
        ::MessageBoxW(
            window_,
            L"Replace the complete draft channel map with this safe template?\n\n"
            L"Manufacturer, model, mode, and display name stay unchanged. You still need to compare the order with the fixture's DMX chart.",
            L"Replace fixture channel map",
            MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) {
        return;
    }
    emberlights::FixtureProfileDefinition draft;
    draft.name = trim(control_text(::GetDlgItem(page, IdProfileName)));
    draft.channels = profile_draft_channels_;
    const auto result = emberlights::apply_fixture_profile_template(
        draft, selected);
    if (!result) {
        set_page_message(
            Page::Profiles, IdProfileMessage, result.message, true);
        return;
    }
    profile_draft_channels_ = std::move(draft.channels);
    set_control_text(
        ::GetDlgItem(page, IdProfileFootprint), number_text(draft.footprint));
    set_control_text(::GetDlgItem(page, IdProfileMappingChannel), "1");
    refresh_profile_channel_table();
    select_profile_channel(0);
    refresh_profile_mapping_summary();
    set_page_message(
        Page::Profiles,
        IdProfileMessage,
        result.message + " Review each row, then Save Profile.");
}

void Application::refresh_profile_mapping_summary() {
    const auto page = pages_[static_cast<std::size_t>(Page::Profiles)];
    emberlights::FixtureProfileDefinition profile;
    profile.name = trim(control_text(::GetDlgItem(page, IdProfileName)));
    profile.mode = trim(control_text(::GetDlgItem(page, IdProfileMode)));
    if (profile.name.empty()) {
        profile.name = "Unsaved local fixture profile";
    }
    profile.source = profile_index_ >= 0 &&
            static_cast<std::size_t>(profile_index_) < project_.fixture_profiles.size()
        ? project_.fixture_profiles[static_cast<std::size_t>(profile_index_)].source
        : showcore::FixtureProfileSource::Local;
    if (!parse_number(
            control_text(::GetDlgItem(page, IdProfileFootprint)),
            profile.footprint) ||
        profile.footprint == 0U || profile.footprint > showcore::kUniverseSlots) {
        set_control_text(
            ::GetDlgItem(page, IdProfileMappingSummary),
            "MAPPING NEEDS ATTENTION\r\nEnter a DMX footprint from 1 through 512.");
        return;
    }
    profile.channels = profile_draft_channels_;
    const auto summary = emberlights::summarize_fixture_profile_mapping(profile);
    const auto audit = emberlights::audit_fixture_profile(profile);
    std::ostringstream usage;
    usage << summary.text << '\n' << audit.text << "\nPATCH USAGE\n";
    std::string_view usage_profile_id;
    bool duplicated_source = false;
    if (profile_index_ >= 0 &&
        static_cast<std::size_t>(profile_index_) < project_.fixture_profiles.size()) {
        usage_profile_id =
            project_.fixture_profiles[static_cast<std::size_t>(profile_index_)].id;
    } else if (profile_duplicate_source_id_.has_value()) {
        usage_profile_id = *profile_duplicate_source_id_;
        duplicated_source = true;
    }
    std::size_t usage_count = 0U;
    for (const auto& fixture : project_.fixtures) {
        if (fixture.profile_id != usage_profile_id) {
            continue;
        }
        ++usage_count;
        if (usage_count <= 6U) {
            usage << fixture.name << " • U" << static_cast<unsigned int>(fixture.universe)
                  << " @ " << fixture.address << '\n';
        }
    }
    if (usage_profile_id.empty()) {
        usage << "Unsaved profile; no patched fixture uses it yet.";
    } else if (usage_count == 0U) {
        usage << "No patched fixture currently uses this profile.";
    } else {
        if (usage_count > 6U) {
            usage << "...and " << usage_count - 6U << " more\n";
        }
        usage << usage_count << " patched fixture"
              << (usage_count == 1U ? "" : "s")
              << (duplicated_source
                      ? " use the source; Save Profile will offer to rebind them."
                      : " use this exact profile ID.");
    }
    set_control_text(
        ::GetDlgItem(page, IdProfileMappingSummary),
        usage.str());
}

void Application::apply_profile_mapping_defaults() {
    const auto page = pages_[static_cast<std::size_t>(Page::Profiles)];
    std::uint16_t footprint = 0U;
    std::uint16_t channel = 0U;
    if (!parse_number(
            control_text(::GetDlgItem(page, IdProfileFootprint)), footprint) ||
        footprint == 0U || footprint > showcore::kUniverseSlots ||
        !parse_number(
            control_text(::GetDlgItem(page, IdProfileMappingChannel)), channel) ||
        channel == 0U || channel > footprint) {
        set_page_message(
            Page::Profiles,
            IdProfileMessage,
            "Choose a channel inside the profile footprint first.",
            true);
        return;
    }
    const auto property = static_cast<showcore::Property>(combo_selected_data(
        ::GetDlgItem(page, IdProfileMappingProperty),
        static_cast<std::intptr_t>(showcore::Property::Count)));
    emberlights::ChannelDefinition definition;
    const auto defaults = emberlights::make_safe_fixture_profile_channel(
        property, channel, definition);
    if (!defaults) {
        set_page_message(
            Page::Profiles, IdProfileMessage, defaults.message, true);
        return;
    }

    emberlights::FixtureProfileDefinition draft;
    draft.name = "Unsaved local fixture profile";
    draft.footprint = footprint;
    draft.channels = profile_draft_channels_;
    const auto applied = emberlights::upsert_fixture_profile_channel(
        draft, definition);
    if (!applied) {
        set_page_message(
            Page::Profiles, IdProfileMessage, applied.message, true);
        return;
    }
    profile_draft_channels_ = std::move(draft.channels);
    combo_select_data(
        ::GetDlgItem(page, IdProfileMappingEncoding),
        static_cast<std::intptr_t>(definition.encoding));
    set_control_text(::GetDlgItem(page, IdProfileMappingFine), "0");
    set_control_text(
        ::GetDlgItem(page, IdProfileMappingMinimum),
        number_text(definition.dmx_min));
    set_control_text(
        ::GetDlgItem(page, IdProfileMappingMaximum),
        number_text(definition.dmx_max));
    set_control_text(
        ::GetDlgItem(page, IdProfileMappingDefault),
        number_text(definition.default_value));
    refresh_profile_channel_table();
    refresh_profile_mapping_summary();
    set_page_message(
        Page::Profiles,
        IdProfileMessage,
        defaults.message + " " + applied.message +
            " Save Profile when the complete channel table matches the fixture's DMX chart.");
}

void Application::apply_profile_mapping_row() {
    const auto page = pages_[static_cast<std::size_t>(Page::Profiles)];
    std::uint16_t footprint = 0U;
    std::uint16_t coarse = 0U;
    std::uint16_t fine = 0U;
    std::uint16_t dmx_min = 0U;
    std::uint16_t dmx_max = 0U;
    std::uint16_t default_value = 0U;
    if (!parse_number(control_text(::GetDlgItem(page, IdProfileFootprint)), footprint) ||
        footprint == 0U || footprint > showcore::kUniverseSlots ||
        !parse_number(control_text(::GetDlgItem(page, IdProfileMappingChannel)), coarse) ||
        coarse == 0U || coarse > footprint ||
        !parse_number(control_text(::GetDlgItem(page, IdProfileMappingFine)), fine) ||
        fine > footprint ||
        !parse_number(control_text(::GetDlgItem(page, IdProfileMappingMinimum)), dmx_min) ||
        dmx_min > 255U ||
        !parse_number(control_text(::GetDlgItem(page, IdProfileMappingMaximum)), dmx_max) ||
        dmx_max > 255U || dmx_min > dmx_max ||
        !parse_number(
            control_text(::GetDlgItem(page, IdProfileMappingDefault)),
            default_value)) {
        set_page_message(
            Page::Profiles,
            IdProfileMessage,
            "Mapping assistant needs a channel inside the footprint, optional fine channel (0 = none), DMX min/max 0–255, and a non-negative default.",
            true);
        return;
    }
    auto property = static_cast<showcore::Property>(combo_selected_data(
        ::GetDlgItem(page, IdProfileMappingProperty),
        static_cast<std::intptr_t>(showcore::Property::Count)));
    const auto encoding = static_cast<showcore::ChannelEncoding>(combo_selected_data(
        ::GetDlgItem(page, IdProfileMappingEncoding),
        static_cast<std::intptr_t>(showcore::ChannelEncoding::Linear8)));
    if (encoding == showcore::ChannelEncoding::Constant8) {
        property = showcore::Property::Count;
        combo_select_data(
            ::GetDlgItem(page, IdProfileMappingProperty),
            static_cast<std::intptr_t>(property));
    } else if (property == showcore::Property::Count) {
        set_page_message(
            Page::Profiles,
            IdProfileMessage,
            "Unused / safe constant requires the Safe constant encoding.",
            true);
        return;
    }
    if ((encoding == showcore::ChannelEncoding::Linear16 &&
         (fine == 0U || fine == coarse)) ||
        (encoding != showcore::ChannelEncoding::Linear16 && fine != 0U)) {
        set_page_message(
            Page::Profiles,
            IdProfileMessage,
            encoding == showcore::ChannelEncoding::Linear16
                ? "Linear 16-bit needs a different fine channel inside the footprint."
                : "Only Linear 16-bit uses a fine channel; enter 0 for this encoding.",
            true);
        return;
    }

    emberlights::ChannelDefinition definition{
        property,
        static_cast<std::uint16_t>(coarse - 1U),
        fine == 0U ? static_cast<std::int16_t>(-1)
                   : static_cast<std::int16_t>(fine - 1U),
        encoding,
        static_cast<std::uint8_t>(dmx_min),
        static_cast<std::uint8_t>(dmx_max),
        default_value};
    emberlights::FixtureProfileDefinition draft;
    draft.name = "Unsaved local fixture profile";
    draft.footprint = footprint;
    draft.channels = profile_draft_channels_;
    const auto result = emberlights::upsert_fixture_profile_channel(
        draft, definition);
    if (!result) {
        set_page_message(
            Page::Profiles, IdProfileMessage, result.message, true);
        return;
    }
    profile_draft_channels_ = std::move(draft.channels);
    refresh_profile_channel_table();
    refresh_profile_mapping_summary();
    set_page_message(
        Page::Profiles,
        IdProfileMessage,
        result.message + " Review the table, then Save Profile.");
}

void Application::delete_profile_mapping_row() {
    const auto page = pages_[static_cast<std::size_t>(Page::Profiles)];
    std::uint16_t footprint = 0U;
    std::uint16_t coarse = 0U;
    if (!parse_number(
            control_text(::GetDlgItem(page, IdProfileFootprint)), footprint) ||
        footprint == 0U || footprint > showcore::kUniverseSlots ||
        !parse_number(
            control_text(::GetDlgItem(page, IdProfileMappingChannel)), coarse) ||
        coarse == 0U || coarse > footprint) {
        set_page_message(
            Page::Profiles,
            IdProfileMessage,
            "Enter the DMX channel number you want to remove.",
            true);
        return;
    }
    emberlights::FixtureProfileDefinition draft;
    draft.name = "Unsaved local fixture profile";
    draft.footprint = footprint;
    draft.channels = profile_draft_channels_;
    const auto result = emberlights::remove_fixture_profile_channel(
        draft, coarse);
    if (!result) {
        set_page_message(
            Page::Profiles, IdProfileMessage, result.message, true);
        return;
    }
    profile_draft_channels_ = std::move(draft.channels);
    refresh_profile_channel_table();
    refresh_profile_mapping_summary();
    set_page_message(
        Page::Profiles,
        IdProfileMessage,
        result.message + " Review the table, then Save Profile.");
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
    restore_authoring_collection_selection(Page::Patch);
    capture_authoring_editor_baseline(Page::Patch);
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
    restore_authoring_collection_selection(Page::Patch);
    capture_authoring_editor_baseline(Page::Patch);
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
    restore_authoring_collection_selection(Page::Groups);
    capture_authoring_editor_baseline(Page::Groups);
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
    restore_authoring_collection_selection(Page::Groups);
    capture_authoring_editor_baseline(Page::Groups);
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
    if (!look.assignments.empty()) {
        const auto fixture = std::find_if(
            project_.fixtures.begin(), project_.fixtures.end(), [&](const auto& candidate) {
                return candidate.id == look.assignments.front().fixture_id;
            });
        if (fixture != project_.fixtures.end()) {
            combo_select_data(
                ::GetDlgItem(page, IdLookTarget),
                static_cast<std::intptr_t>(
                    std::distance(project_.fixtures.begin(), fixture)));
            refresh_look_capabilities();
        }
    }
    refresh_look_draft_view();
    ::EnableWindow(::GetDlgItem(page, IdLookDelete), TRUE);
    set_page_message(Page::Looks, IdLookMessage, "Editing Static Look " + look.id + ".");
    update_physical_static_look_preview_if_active();
    restore_authoring_collection_selection(Page::Looks);
    capture_authoring_editor_baseline(Page::Looks);
}

void Application::new_look() {
    if (physical_preview_.status().owns_runner) {
        stop_physical_static_look_preview(false);
    }
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
                     "Choose a target, then apply a full color or explicit attribute ownership.");
    restore_authoring_collection_selection(Page::Looks);
    capture_authoring_editor_baseline(Page::Looks);
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
    update_physical_static_look_preview_if_active();
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
            "Apply a full color or an attribute before saving this Static Look.",
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
    stop_physical_static_look_preview(false);
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
    if (outcome) {
        update_physical_static_look_preview_if_active();
    }
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
            "The selected target has no supported attributes.", true);
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
        static_look_outcome_text("Attribute ownership", outcome),
        !outcome);
    if (outcome) {
        update_physical_static_look_preview_if_active();
    }
}

void Application::apply_static_look_control_choice() {
    if (!look_draft_.has_value()) {
        new_look();
    }
    const auto page = pages_[static_cast<std::size_t>(Page::Looks)];
    const auto selected = combo_selected_data(
        ::GetDlgItem(page, IdLookNamedChoice), -1);
    if (selected < 0 ||
        static_cast<std::size_t>(selected) >= look_control_choices_.size()) {
        set_page_message(
            Page::Looks, IdLookMessage,
            "Choose a profile-backed Fixture Attribute first.",
            true);
        return;
    }
    const auto& choice = look_control_choices_[static_cast<std::size_t>(selected)];
    float position = 0.5F;
    if (choice.behavior == showcore::ChannelCapabilityBehavior::Continuous) {
        float percentage = 0.0F;
        if (!parse_number(
                control_text(::GetDlgItem(page, IdLookValue)), percentage) ||
            !std::isfinite(percentage) || percentage < 0.0F ||
            percentage > 100.0F) {
            set_page_message(
                Page::Looks, IdLookMessage,
                "A continuous named range needs a position from 0 through 100.",
                true);
            return;
        }
        position = percentage / 100.0F;
    }
    const auto outcome = emberlights::apply_static_look_control_choice(
        *look_draft_,
        project_,
        selected_look_target_id(),
        choice.id,
        position);
    combo_select_data(
        ::GetDlgItem(page, IdLookProperty),
        static_cast<std::intptr_t>(choice.property));
    refresh_look_draft_view();
    set_page_message(
        Page::Looks,
        IdLookMessage,
        static_look_outcome_text("Fixture Attribute • " + choice.name, outcome),
        !outcome);
    if (outcome) {
        update_physical_static_look_preview_if_active();
    }
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
        static_look_outcome_text("Attribute removal", outcome),
        !outcome);
    if (outcome) {
        update_physical_static_look_preview_if_active();
    }
}

bool Application::read_static_look_preview_draft(
    emberlights::StaticLookDraft& draft,
    std::string& error_message) const {
    const auto page = pages_[static_cast<std::size_t>(Page::Looks)];
    if (!look_draft_.has_value() || look_draft_->look.assignments.empty()) {
        error_message =
            "Apply at least one color or attribute before starting a preview.";
        return false;
    }
    draft = *look_draft_;
    draft.look.name = trim(control_text(::GetDlgItem(page, IdLookName)));
    if (draft.look.name.empty()) {
        draft.look.name = "Unsaved Static Look Preview";
    }
    if (!parse_number(control_text(::GetDlgItem(page, IdLookFade)), draft.look.fade_ms) ||
        draft.look.fade_ms > emberlights::kMaximumStaticLookFadeMs) {
        error_message = "Preview needs a crossfade from 0 through 30000 ms.";
        return false;
    }
    if (draft.look.id.empty()) {
        draft.look.id = unique_id("preview-look", draft.look.name);
        draft.source_index.reset();
    }
    return true;
}

void Application::preview_static_look() {
    const auto page = pages_[static_cast<std::size_t>(Page::Looks)];
    emberlights::StaticLookDraft draft;
    std::string error_message;
    if (!read_static_look_preview_draft(draft, error_message)) {
        set_page_message(
            Page::Looks, IdLookMessage, error_message, true);
        return;
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

void Application::begin_or_update_physical_static_look_preview() {
    if (physical_preview_.status().owns_runner) {
        update_physical_static_look_preview_if_active();
        if (physical_preview_.status().owns_runner) {
            set_page_message(
                Page::Looks,
                IdLookMessage,
                "Physical preview updated. Changes remain capped and the original 30-second deadline did not move.");
        }
        return;
    }
    if (runner_.status().state != emberlights::RunnerState::Stopped) {
        set_page_message(
            Page::Looks,
            IdLookMessage,
            "Stop Live before previewing a Static Look on physical fixtures.",
            true);
        return;
    }
    emberlights::StaticLookDraft draft;
    std::string error_message;
    if (!read_static_look_preview_draft(draft, error_message)) {
        set_page_message(Page::Looks, IdLookMessage, error_message, true);
        return;
    }
    const auto target_id = selected_look_target_id();
    if (target_id.empty()) {
        set_page_message(
            Page::Looks,
            IdLookMessage,
            "Select a patched fixture or non-empty group to preview.",
            true);
        return;
    }
    const auto confirmation =
        L"Start a bounded hardware preview for the selected target?\n\n"
        L"• Normal Live must remain stopped\n"
        L"• Only the selected fixture/group is retained\n"
        L"• Output is capped at 35% for 30 seconds\n"
        L"• Strobe, fog, haze, laser, sparks, and unknown positive output are blocked\n"
        L"• STOP PREVIEW sends the Runner's terminal blackout frames\n\n"
        L"This is a visual authoring preview, not fixture qualification.";
    if (::MessageBoxW(
            window_,
            confirmation,
            L"Preview Static Look on fixtures",
            MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) {
        return;
    }
    active_project_.reset();
    ui_commands_.set_active_project(nullptr);
    const auto result = physical_preview_.begin(project_, draft, target_id);
    if (!result) {
        std::string detail = result.warnings.empty()
            ? std::string("Physical preview was refused: ") +
                emberlights::static_look_physical_preview_error_name(result.error) + "."
            : result.warnings.front();
        if (!result.validation.ok() && !result.validation.issues.empty()) {
            detail += " " + first_validation_error(result.validation);
        }
        set_page_message(Page::Looks, IdLookMessage, detail, true);
        refresh_physical_preview_status();
        refresh_live_status();
        return;
    }
    static_cast<void>(::ModifyMenuW(
        ::GetSubMenu(::GetMenu(window_), 2),
        IdShowStartStop,
        MF_BYCOMMAND | MF_STRING,
        IdShowStartStop,
        L"&Stop Studio Preview"));
    refresh_physical_preview_status();
    refresh_live_status();
    set_page_message(
        Page::Looks,
        IdLookMessage,
        "Physical preview active. Apply colors/properties to update fixtures in realtime; unlisted fixtures remain black in this isolated session.");
    set_status(
        L"Studio hardware preview active: 35% cap, 30-second limit, Live content disabled.");
}

void Application::update_physical_static_look_preview_if_active() {
    if (!physical_preview_.status().owns_runner) {
        return;
    }
    emberlights::StaticLookDraft draft;
    std::string error_message;
    if (!read_static_look_preview_draft(draft, error_message)) {
        stop_physical_static_look_preview(false);
        set_page_message(Page::Looks, IdLookMessage, error_message, true);
        return;
    }
    const auto target_id = selected_look_target_id();
    if (target_id.empty()) {
        stop_physical_static_look_preview(false);
        set_page_message(
            Page::Looks,
            IdLookMessage,
            "Physical preview stopped because no valid target is selected.",
            true);
        return;
    }
    const auto result = physical_preview_.update(project_, draft, target_id);
    if (!result) {
        const auto detail = result.warnings.empty()
            ? std::string("Physical preview stopped safely: ") +
                emberlights::static_look_physical_preview_error_name(result.error) + "."
            : result.warnings.front();
        set_page_message(Page::Looks, IdLookMessage, detail, true);
        static_cast<void>(::ModifyMenuW(
            ::GetSubMenu(::GetMenu(window_), 2),
            IdShowStartStop,
            MF_BYCOMMAND | MF_STRING,
            IdShowStartStop,
            L"&Start Show"));
    }
    refresh_physical_preview_status();
    refresh_live_status();
}

void Application::stop_physical_static_look_preview(bool announce) {
    const auto stopped = physical_preview_.stop();
    if (!stopped) {
        refresh_physical_preview_status();
        return;
    }
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
    refresh_physical_preview_status();
    if (announce) {
        set_page_message(
            Page::Looks,
            IdLookMessage,
            "Physical preview stopped. EmberLights sent explicit terminal blackout frames.");
        set_status(
            L"Studio hardware preview stopped; explicit zero frames were sent to active outputs.");
    }
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
    restore_authoring_collection_selection(Page::Autoloops);
    capture_authoring_editor_baseline(Page::Autoloops);
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
    set_control_text(::GetDlgItem(page, IdAutoloopStepBeat), "0");
    set_control_text(::GetDlgItem(page, IdAutoloopSteps), "");
    ::EnableWindow(::GetDlgItem(page, IdAutoloopDelete), FALSE);
    ::EnableWindow(::GetDlgItem(page, IdAutoloopNextEmpty), FALSE);
    ::EnableWindow(::GetDlgItem(page, IdAutoloopSwapTarget), FALSE);
    set_page_message(
        Page::Autoloops,
        IdAutoloopMessage,
        found && !project_.looks.empty()
            ? "The first open bank/slot was selected. Use Quick step builder to add saved Looks."
            : found
                ? "The first open bank/slot was selected. Create a Static Look, then add steps here."
              : "The compiled 2,048-slot Autoloop library is full.",
        !found);
    restore_authoring_collection_selection(Page::Autoloops);
    capture_authoring_editor_baseline(Page::Autoloops);
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

void Application::add_autoloop_step() {
    const auto page = pages_[static_cast<std::size_t>(Page::Autoloops)];
    const auto selected = combo_selected_data(
        ::GetDlgItem(page, IdAutoloopLookChoice), -1);
    if (selected < 0 || static_cast<std::size_t>(selected) >= project_.looks.size()) {
        set_page_message(
            Page::Autoloops,
            IdAutoloopMessage,
            "Save at least one Static Look before building an Autoloop.",
            true);
        return;
    }
    float beat = 0.0F;
    if (!parse_number(
            control_text(::GetDlgItem(page, IdAutoloopStepBeat)), beat) ||
        !std::isfinite(beat) || beat < 0.0F) {
        set_page_message(
            Page::Autoloops,
            IdAutoloopMessage,
            "Quick step beat must be zero or a positive musical beat.",
            true);
        return;
    }
    std::vector<emberlights::AutoloopStepDefinition> steps;
    const auto existing = trim(control_text(::GetDlgItem(page, IdAutoloopSteps)));
    std::string parse_error;
    if (!existing.empty() && !parse_autoloop_rows(existing, steps, parse_error)) {
        set_page_message(Page::Autoloops, IdAutoloopMessage, parse_error, true);
        return;
    }
    const auto transition = static_cast<showcore::AutoloopTransition>(
        combo_selected_data(
            ::GetDlgItem(page, IdAutoloopStepTransition),
            static_cast<std::intptr_t>(showcore::AutoloopTransition::Cut)));
    steps.push_back({beat, project_.looks[static_cast<std::size_t>(selected)].id, transition});
    std::stable_sort(
        steps.begin(),
        steps.end(),
        [](const auto& left, const auto& right) {
            return left.at_beat < right.at_beat;
        });
    emberlights::AutoloopDefinition draft;
    draft.steps = std::move(steps);
    set_control_text(::GetDlgItem(page, IdAutoloopSteps), autoloop_steps_text(draft));
    set_page_message(
        Page::Autoloops,
        IdAutoloopMessage,
        "Step added and ordered by beat. Save Autoloop when the sequence is ready.");
}

void Application::remove_last_autoloop_step() {
    const auto page = pages_[static_cast<std::size_t>(Page::Autoloops)];
    std::vector<emberlights::AutoloopStepDefinition> steps;
    std::string parse_error;
    const auto existing = trim(control_text(::GetDlgItem(page, IdAutoloopSteps)));
    if (existing.empty()) {
        set_page_message(
            Page::Autoloops,
            IdAutoloopMessage,
            "The draft has no steps to remove.");
        return;
    }
    if (!parse_autoloop_rows(existing, steps, parse_error)) {
        set_page_message(Page::Autoloops, IdAutoloopMessage, parse_error, true);
        return;
    }
    steps.pop_back();
    emberlights::AutoloopDefinition draft;
    draft.steps = std::move(steps);
    set_control_text(
        ::GetDlgItem(page, IdAutoloopSteps), autoloop_steps_text(draft));
    set_page_message(
        Page::Autoloops,
        IdAutoloopMessage,
        "Last musical step removed from the draft. Save Autoloop when ready.");
}

void Application::clear_autoloop_steps() {
    const auto page = pages_[static_cast<std::size_t>(Page::Autoloops)];
    if (trim(control_text(::GetDlgItem(page, IdAutoloopSteps))).empty()) {
        set_page_message(
            Page::Autoloops,
            IdAutoloopMessage,
            "The draft already has no steps.");
        return;
    }
    if (::MessageBoxW(
            window_,
            L"Clear every step from this Autoloop draft? The saved Autoloop is unchanged until you press Save Autoloop.",
            L"Clear Autoloop draft steps",
            MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) {
        return;
    }
    set_control_text(::GetDlgItem(page, IdAutoloopSteps), "");
    set_page_message(
        Page::Autoloops,
        IdAutoloopMessage,
        "Draft steps cleared. Add a beat-zero Static Look before saving.");
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

void Application::generate_autoscript_proposal() {
    const auto page = pages_[static_cast<std::size_t>(Page::Autoscript)];
    std::uint32_t track_bars = 0U;
    double loop_beats = 0.0;
    std::uint16_t energy_percent = 0U;
    std::uint16_t bank = 0U;
    std::uint16_t slot = 0U;
    std::uint64_t seed = 0U;
    if (!parse_number(
            control_text(::GetDlgItem(page, IdAutoscriptTrackBars)),
            track_bars) ||
        track_bars == 0U || track_bars > 4096U ||
        !parse_number(
            control_text(::GetDlgItem(page, IdAutoscriptLoopBeats)),
            loop_beats) ||
        !std::isfinite(loop_beats) || loop_beats <= 0.0 ||
        !parse_number(
            control_text(::GetDlgItem(page, IdAutoscriptEnergy)),
            energy_percent) ||
        energy_percent > 100U ||
        !parse_number(
            control_text(::GetDlgItem(page, IdAutoscriptBank)), bank) ||
        bank == 0U || bank > showcore::kMaxAutoloopBanks ||
        !parse_number(
            control_text(::GetDlgItem(page, IdAutoscriptSlot)), slot) ||
        slot == 0U || slot > showcore::kAutoloopsPerBank ||
        !parse_number(
            control_text(::GetDlgItem(page, IdAutoscriptSeed)), seed)) {
        refresh_autoscript_summary(
            "Enter 1–4096 track bars, positive loop beats, energy 0–100, "
            "bank 1–64, slot 1–32, and an unsigned whole-number seed.");
        return;
    }

    const auto loop_ticks_floating =
        loop_beats * static_cast<double>(emberlights::kMusicalTicksPerQuarter);
    const auto loop_ticks = static_cast<emberlights::MusicalTick>(
        std::llround(loop_ticks_floating));
    if (loop_ticks <= 0 ||
        std::fabs(loop_ticks_floating - static_cast<double>(loop_ticks)) >
            0.000001) {
        refresh_autoscript_summary(
            "Loop beats must resolve exactly on the 960-PPQ musical grid.");
        return;
    }
    const auto track_ticks = static_cast<emberlights::MusicalTick>(track_bars) *
        4 * emberlights::kMusicalTicksPerQuarter;
    const auto grid_ticks = static_cast<emberlights::MusicalTick>(
        combo_selected_data(
            ::GetDlgItem(page, IdAutoscriptGrid),
            emberlights::kMusicalTicksPerQuarter));

    std::vector<std::string> roles;
    const auto role_text = control_text(::GetDlgItem(page, IdAutoscriptRoles));
    std::size_t role_start = 0U;
    while (role_start <= role_text.size()) {
        const auto comma = role_text.find(',', role_start);
        const auto role = trim(role_text.substr(
            role_start,
            comma == std::string::npos
                ? std::string::npos
                : comma - role_start));
        if (!role.empty()) {
            roles.push_back(role);
        }
        if (comma == std::string::npos) {
            break;
        }
        role_start = comma + 1U;
    }
    std::sort(roles.begin(), roles.end());
    roles.erase(std::unique(roles.begin(), roles.end()), roles.end());
    if (roles.size() > emberlights::kMaximumAutoloopAutoscriptRoleSelectors) {
        refresh_autoscript_summary(
            "Use no more than four comma-separated fixture roles.");
        return;
    }

    emberlights::AutoloopAutoscriptRequest request;
    request.track_duration_ticks = track_ticks;
    request.loop_length_ticks = loop_ticks;
    request.grid_ticks = grid_ticks;
    request.style = static_cast<emberlights::AutoloopAutoscriptStyle>(
        combo_selected_data(
            ::GetDlgItem(page, IdAutoscriptStyle),
            static_cast<std::intptr_t>(
                emberlights::AutoloopAutoscriptStyle::Balanced)));
    request.complexity =
        static_cast<emberlights::AutoloopAutoscriptComplexity>(
            combo_selected_data(
                ::GetDlgItem(page, IdAutoscriptComplexity),
                static_cast<std::intptr_t>(
                    emberlights::AutoloopAutoscriptComplexity::Medium)));
    auto section = emberlights::AutoloopAutoscriptSectionKind::Verse;
    if (request.style == emberlights::AutoloopAutoscriptStyle::Subtle) {
        section = emberlights::AutoloopAutoscriptSectionKind::Intro;
    } else if (
        request.style == emberlights::AutoloopAutoscriptStyle::ColorMotion) {
        section = emberlights::AutoloopAutoscriptSectionKind::Chorus;
    } else if (
        request.style == emberlights::AutoloopAutoscriptStyle::BuildDrop) {
        section = emberlights::AutoloopAutoscriptSectionKind::Drop;
    }
    request.musical_sections.push_back({
        0,
        track_ticks,
        section,
        static_cast<std::uint16_t>(energy_percent * 10U)});
    request.eligible_role_selectors = std::move(roles);
    request.seed = seed;
    request.first_placement = {
        static_cast<std::uint16_t>(bank - 1U),
        static_cast<std::uint8_t>(slot - 1U)};

    const auto loaded = autoscript_workflow_.load_document(project_);
    if (!loaded) {
        refresh_autoscript_summary(
            loaded.message.empty()
                ? "The current project is not a valid AutoScript document."
                : loaded.message);
        return;
    }
    const auto proposed = autoscript_workflow_.propose_and_preview(
        std::move(request));
    refresh_autoscript_summary(
        proposed.message.empty()
            ? emberlights::studio_autoloop_autoscript_workflow_result_name(
                  proposed.result)
            : proposed.message);
}

void Application::preview_autoscript_phase(double phase) {
    const auto previewed = autoscript_workflow_.preview_phase(phase);
    refresh_autoscript_summary(
        previewed.message.empty()
            ? emberlights::studio_autoloop_autoscript_workflow_result_name(
                  previewed.result)
            : previewed.message);
}

void Application::commit_autoscript_proposal() {
    const auto base = autoscript_workflow_.document_snapshot();
    if (emberlights::serialize_project(base.document) !=
        emberlights::serialize_project(project_)) {
        refresh_autoscript_summary(
            "The project changed after this proposal. Discard it and generate again.");
        return;
    }
    const auto committed = autoscript_workflow_.commit();
    if (!committed) {
        refresh_autoscript_summary(
            committed.message.empty()
                ? emberlights::studio_autoloop_autoscript_workflow_result_name(
                      committed.result)
                : committed.message);
        return;
    }

    project_ = autoscript_workflow_.document_snapshot().document;
    mark_dirty();
    refresh_live_lists();
    refresh_diagnostics();
    refresh_autoscript();
    refresh_autoscript_summary(
        "AutoScript committed as one Undo transaction. Save the project, then "
        "Start Show to launch the V2 placement from Live.");
}

void Application::discard_autoscript_proposal() {
    const auto discarded = autoscript_workflow_.discard();
    refresh_autoscript();
    refresh_autoscript_summary(discarded.message);
}

void Application::apply_autoscript_fixture_function() {
    const auto page = pages_[static_cast<std::size_t>(Page::Autoscript)];
    const auto placement_index = combo_selected_data(
        ::GetDlgItem(page, IdAutoscriptFunctionPlacement), -1);
    const auto target_index = combo_selected_data(
        ::GetDlgItem(page, IdAutoscriptFunctionTarget), -1);
    const auto choice_index = combo_selected_data(
        ::GetDlgItem(page, IdAutoscriptFunctionChoice), -1);
    if (placement_index < 0 ||
        static_cast<std::size_t>(placement_index) >=
            autoscript_function_placement_ids_.size() ||
        target_index < 0 ||
        static_cast<std::size_t>(target_index) >=
            autoscript_function_target_ids_.size() ||
        choice_index < 0 ||
        static_cast<std::size_t>(choice_index) >=
            autoscript_function_choices_.size()) {
        refresh_autoscript_summary(
            "Select a committed V2 placement, fixture/group, and Fixture Attribute first.");
        return;
    }

    double start_beat = 0.0;
    double end_beat = 0.0;
    std::uint16_t position_percent = 0U;
    if (!parse_number(
            control_text(::GetDlgItem(page, IdAutoscriptFunctionStart)),
            start_beat) ||
        !parse_number(
            control_text(::GetDlgItem(page, IdAutoscriptFunctionEnd)),
            end_beat) ||
        !parse_number(
            control_text(::GetDlgItem(page, IdAutoscriptFunctionPosition)),
            position_percent) ||
        !std::isfinite(start_beat) || !std::isfinite(end_beat) ||
        start_beat < 0.0 || end_beat <= start_beat ||
        position_percent > 100U) {
        refresh_autoscript_summary(
            "Use a non-negative start beat, a larger end beat, and range position 0–100.");
        return;
    }

    const auto persisted = emberlights::inspect_persisted_autoloop_source(project_);
    if (!persisted || !persisted.stamp.present) {
        refresh_autoscript_summary(
            "Create and commit a V2 Autoloop before adding a Fixture Attribute.");
        return;
    }
    const auto& placement_id = autoscript_function_placement_ids_[
        static_cast<std::size_t>(placement_index)];
    const auto placement = std::find_if(
        persisted.source.placements.begin(), persisted.source.placements.end(),
        [&](const auto& candidate) { return candidate.id == placement_id; });
    if (placement == persisted.source.placements.end()) {
        refresh_autoscript_summary(
            "The selected V2 placement changed. Refresh the page and select it again.");
        return;
    }
    const auto asset = std::find_if(
        persisted.source.assets.begin(), persisted.source.assets.end(),
        [&](const auto& candidate) {
            return candidate.id == placement->asset_id;
        });
    if (asset == persisted.source.assets.end()) {
        refresh_autoscript_summary(
            "The selected placement has no valid V2 asset.");
        return;
    }
    const auto program = std::find_if(
        persisted.source.programs.begin(), persisted.source.programs.end(),
        [&](const auto& candidate) {
            return candidate.id == asset->program_id;
        });
    if (program == persisted.source.programs.end()) {
        refresh_autoscript_summary(
            "The selected placement has no valid V2 program.");
        return;
    }

    const auto exact_tick = [&](double beat)
        -> std::optional<emberlights::MusicalTick> {
        constexpr auto ticks_per_beat =
            static_cast<double>(emberlights::kMusicalTicksPerQuarter);
        const auto maximum_beat = static_cast<double>(program->length_ticks) /
            ticks_per_beat;
        if (!std::isfinite(beat) || beat < 0.0 || beat > maximum_beat) {
            return std::nullopt;
        }
        const auto floating = beat * ticks_per_beat;
        const auto rounded = static_cast<emberlights::MusicalTick>(
            std::llround(floating));
        if (std::fabs(floating - static_cast<double>(rounded)) > 0.000001) {
            return std::nullopt;
        }
        return rounded;
    };
    const auto start_tick = exact_tick(start_beat);
    const auto end_tick = exact_tick(end_beat);
    if (!start_tick.has_value() || !end_tick.has_value() ||
        *end_tick <= *start_tick || *end_tick > program->length_ticks) {
        refresh_autoscript_summary(
            "Start/end must land exactly on the 960-PPQ grid and stay inside this loop.");
        return;
    }

    emberlights::AutoloopAuthoringService authoring(persisted.source);
    emberlights::AutoloopFixtureControlRequest request;
    request.expected_generation = authoring.generation();
    request.program_id = program->id;
    request.target_id = autoscript_function_target_ids_[
        static_cast<std::size_t>(target_index)];
    request.choice_id = autoscript_function_choices_[
        static_cast<std::size_t>(choice_index)].id;
    request.start_tick = *start_tick;
    request.end_tick = *end_tick;
    request.position = static_cast<float>(position_percent) / 100.0F;

    emberlights::AutoloopFixtureControlProposal proposal;
    bool prepared = false;
    const auto first_suffix = program->events.size() + 1U;
    for (std::size_t attempt = 0U; attempt < 100000U; ++attempt) {
        request.stable_id_prefix =
            "fixture-function." + std::to_string(first_suffix + attempt);
        proposal = emberlights::plan_autoloop_fixture_control(
            authoring.snapshot(), project_, request);
        if (proposal) {
            prepared = true;
            break;
        }
        if (proposal.result !=
            emberlights::AutoloopFixtureControlResult::IdentifierCollision) {
            break;
        }
    }
    if (!prepared) {
        refresh_autoscript_summary(
            proposal.message.empty()
                ? std::string("Fixture Attribute rejected: ") +
                    emberlights::autoloop_fixture_control_result_name(
                        proposal.result)
                : proposal.message);
        return;
    }

    const auto applied = emberlights::apply_autoloop_fixture_control(
        authoring, project_, request);
    if (!applied) {
        refresh_autoscript_summary(
            applied.message.empty()
                ? std::string("Fixture Attribute rejected: ") +
                    emberlights::autoloop_fixture_control_result_name(
                        applied.result)
                : applied.message);
        return;
    }

    emberlights::StudioDocumentService document;
    const auto loaded_document = document.replace_document(
        document.generation(), project_,
        emberlights::StudioDocumentBoundary::OpenedDocument);
    if (!loaded_document) {
        refresh_autoscript_summary(
            "The current project could not enter the Studio preview transaction: " +
            loaded_document.message);
        return;
    }
    const auto document_snapshot = document.snapshot();
    if (!document_snapshot.autoloop_source ||
        !document_snapshot.autoloop_source.stamp.present) {
        refresh_autoscript_summary(
            "The Studio preview transaction could not verify the persisted V2 source.");
        return;
    }
    const auto source_snapshot = authoring.snapshot();
    emberlights::StudioPreviewService preview;
    const auto preview_loaded = preview.load_autoloop_v2(
        document_snapshot, source_snapshot);
    if (!preview_loaded) {
        refresh_autoscript_summary(
            "Production-compiler preview rejected the edit: " +
            preview_loaded.message);
        return;
    }
    const auto preview_selected = preview.preview_autoloop_v2(
        document_snapshot.generation, source_snapshot.generation,
        placement_id);
    if (!preview_selected) {
        refresh_autoscript_summary(
            "The edited placement could not be selected for preview: " +
            preview_selected.message);
        return;
    }
    const auto preview_seek = preview.seek_autoloop_v2(
        document_snapshot.generation, source_snapshot.generation,
        *start_tick);
    const auto preview_snapshot = preview.snapshot();
    if (!preview_seek || !preview_snapshot.output_disabled) {
        refresh_autoscript_summary(
            preview_seek.message.empty()
                ? "The output-disabled preview safety contract did not pass."
                : preview_seek.message);
        return;
    }

    const auto committed = document.apply_autoloop_source(
        document_snapshot.generation,
        document_snapshot.autoloop_source.stamp,
        source_snapshot.source);
    if (!committed) {
        refresh_autoscript_summary(
            "The generation/digest-checked Studio commit was rejected: " +
            committed.message);
        return;
    }

    std::ostringstream preview_summary;
    preview_summary << "Output adapters: DISABLED\r\n"
                    << "Placement: " << placement_id << "  Beat: "
                    << preview_snapshot.beat_position << "\r\n"
                    << "Function writes: " << applied.writes.size()
                    << "  Frame SHA-256: " << preview_snapshot.frame_sha256;
    std::vector<const emberlights::StudioPreviewFixtureSnapshot*>
        previewed_fixtures;
    for (const auto& write : applied.writes) {
        const auto fixture = std::find_if(
            preview_snapshot.fixtures.begin(),
            preview_snapshot.fixtures.end(),
            [&](const auto& candidate) {
                return candidate.fixture_id == write.fixture_id;
            });
        if (fixture != preview_snapshot.fixtures.end() &&
            std::none_of(
                previewed_fixtures.begin(), previewed_fixtures.end(),
                [&](const auto* candidate) {
                    return candidate->fixture_id == fixture->fixture_id;
                })) {
            previewed_fixtures.push_back(&*fixture);
        }
    }
    const auto shown_fixture_count = std::min<std::size_t>(
        previewed_fixtures.size(), 8U);
    for (std::size_t index = 0U; index < shown_fixture_count; ++index) {
        const auto& fixture = *previewed_fixtures[index];
        preview_summary << "\r\n  " << fixture.fixture_name << " — U"
                        << static_cast<unsigned int>(fixture.universe) << " A"
                        << fixture.address << " — DMX";
        for (const auto value : fixture.dmx_values) {
            preview_summary << ' ' << static_cast<unsigned int>(value);
        }
    }
    if (previewed_fixtures.size() > shown_fixture_count) {
        preview_summary << "\r\n  … "
                        << previewed_fixtures.size() - shown_fixture_count
                        << " more fixtures";
    }
    for (const auto& warning : applied.warnings) {
        preview_summary << "\r\n  Warning: " << warning;
    }
    autoscript_function_preview_summary_ = preview_summary.str();

    project_ = document.snapshot().document;
    static_cast<void>(autoscript_workflow_.discard());
    mark_dirty();
    refresh_live_lists();
    refresh_diagnostics();
    refresh_autoscript();
    std::ostringstream message;
    message << "Added " << applied.writes.size()
            << " exact profile-backed Fixture Attribute"
            << (applied.writes.size() == 1U ? "" : "s")
            << " after an output-disabled production preview. Save the project to keep this V2 source transaction.";
    refresh_autoscript_summary(message.str());
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
    restore_authoring_collection_selection(Page::Tracks);
    capture_authoring_editor_baseline(Page::Tracks);
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
    restore_authoring_collection_selection(Page::Tracks);
    capture_authoring_editor_baseline(Page::Tracks);
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
    refresh_midi_named_choices();
}

void Application::refresh_midi_named_choices() {
    const auto page = pages_[static_cast<std::size_t>(Page::Midi)];
    const auto combo = ::GetDlgItem(page, IdMidiNamedChoice);
    std::string previous_id;
    const auto previous = combo_selected_data(combo, -1);
    if (previous >= 0 &&
        static_cast<std::size_t>(previous) < midi_named_choices_.size()) {
        previous_id = midi_named_choices_[static_cast<std::size_t>(previous)].id;
    }
    midi_named_choices_.clear();
    static_cast<void>(::SendMessageW(combo, CB_RESETCONTENT, 0, 0));
    combo_add(combo, L"Use the advanced semantic attribute", -1);

    const auto action = static_cast<showcore::ActionType>(combo_selected_data(
        ::GetDlgItem(page, IdMidiAction),
        static_cast<std::intptr_t>(showcore::ActionType::Blackout)));
    const auto target_index = combo_selected_data(
        ::GetDlgItem(page, IdMidiTarget), -1);
    std::string_view target_id;
    if (action == showcore::ActionType::SetProperty && target_index >= 0 &&
        static_cast<std::size_t>(target_index) < project_.fixtures.size()) {
        target_id = project_.fixtures[static_cast<std::size_t>(target_index)].id;
    } else if (action == showcore::ActionType::SetGroupProperty &&
               target_index >= 0 &&
               static_cast<std::size_t>(target_index) < project_.groups.size()) {
        target_id = project_.groups[static_cast<std::size_t>(target_index)].id;
    }
    if (!target_id.empty()) {
        const auto catalog = emberlights::fixture_control_choices(project_, target_id);
        for (const auto& choice : catalog.choices) {
            if (choice.safety_gated()) {
                continue;
            }
            combo_add(
                combo,
                widen(fixture_control_choice_label(choice)),
                static_cast<std::intptr_t>(midi_named_choices_.size()));
            midi_named_choices_.push_back(choice);
        }
    }
    auto selected = static_cast<std::intptr_t>(-1);
    const auto retained = std::find_if(
        midi_named_choices_.begin(), midi_named_choices_.end(),
        [&](const auto& choice) { return choice.id == previous_id; });
    if (retained != midi_named_choices_.end()) {
        selected = static_cast<std::intptr_t>(
            std::distance(midi_named_choices_.begin(), retained));
    }
    combo_select_data(combo, selected);
    static_cast<void>(::EnableWindow(
        combo,
        target_id.empty() || midi_named_choices_.empty() ? FALSE : TRUE));
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
    const auto finish_learning = [&]() {
        midi_learning_ = false;
        learn_input_.close_all();
        static_cast<void>(::SetWindowTextW(
            ::GetDlgItem(page, IdMidiLearn), L"Learn Next MIDI Control"));
    };
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

    const auto named_index = combo_selected_data(
        ::GetDlgItem(page, IdMidiNamedChoice), -1);
    std::size_t added_mapping_count = 1U;
    bool expanded_named_group = false;
    std::vector<std::string> binding_warnings;
    auto candidate = project_;
    if (named_index >= 0 &&
        static_cast<std::size_t>(named_index) < midi_named_choices_.size()) {
        const auto plan = emberlights::plan_fixture_controller_binding(
            project_, mapping.target_ref,
            midi_named_choices_[static_cast<std::size_t>(named_index)].id,
            mapping);
        if (!plan) {
            finish_learning();
            set_page_message(
                Page::Midi,
                IdMidiMessage,
                plan.message.empty()
                    ? "The profile-backed Fixture Attribute binding was rejected."
                    : plan.message,
                true);
            return;
        }
        added_mapping_count = plan.mappings.size();
        expanded_named_group = plan.expanded_to_fixtures;
        binding_warnings = plan.warnings;
        candidate.midi_mappings.insert(
            candidate.midi_mappings.end(),
            plan.mappings.begin(), plan.mappings.end());
    } else {
        candidate.midi_mappings.push_back(std::move(mapping));
    }

    const auto compilation =
        emberlights::compile_project_with_persisted_autoloops(candidate);
    if (!compilation) {
        finish_learning();
        set_page_message(
            Page::Midi,
            IdMidiMessage,
            "The complete controller map did not compile and was not saved: " +
                first_validation_error(compilation.validation),
            true);
        return;
    }
    project_ = std::move(candidate);
    finish_learning();
    mark_dirty();
    record_project_edit(before);
    refresh_midi();
    std::ostringstream learned;
    learned << (added_mapping_count == 1U ? "Mapping" : "Mappings")
            << " learned and saved to the project";
    if (expanded_named_group) {
        learned << " as " << added_mapping_count
                << " exact per-fixture mappings for the mixed-profile group";
    }
    if (!binding_warnings.empty()) {
        learned << ". " << binding_warnings.front();
    }
    learned << (runner_.status().state == emberlights::RunnerState::Running
            ? ". Save the project to atomically activate the newly compiled map."
            : ".");
    set_page_message(
        Page::Midi,
        IdMidiMessage,
        learned.str());
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
