#include "emberlights/fixtures_looks_shell.hpp"
#include "emberlights/os2l_service.hpp"
#include "emberlights/project_io.hpp"
#include "emberlights/static_look_authoring.hpp"
#include "emberlights/static_look_preview_coordinator.hpp"
#include "emberlights/studio_document.hpp"
#include "emberlights/ui_command.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#include "fixtures_looks_lab.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#endif

#ifndef EMBERLIGHTS_PRODUCT_SHELL
#define EMBERLIGHTS_PRODUCT_SHELL 0
#endif

#ifndef EMBERLIGHTS_VERSION
#define EMBERLIGHTS_VERSION "0.1.0-dev"
#endif

#ifndef EMBERLIGHTS_COMMIT
#define EMBERLIGHTS_COMMIT "unknown"
#endif

namespace {

constexpr bool kProductShell = EMBERLIGHTS_PRODUCT_SHELL != 0;

class LabPreviewCommandHost;

struct LabState {
    emberlights::StudioDocumentService document;
    std::optional<emberlights::StaticLookDraft> selected_look_draft;
    std::optional<std::filesystem::path> project_path;
    std::string selected_profile_id{"local.visual.mover"};
    std::string selected_target_id{"group.movers"};
    std::string selected_look_id{"look.ceremony"};
    std::string selected_choice_id;
    std::string profile_search;
    std::string look_search;
    std::string control_search;
    std::optional<emberlights::FixtureParameterCategory> control_category;
    std::string preview_status_token;
    std::string os2l_status_token;
    std::string operation_message;
    bool draft_dirty{false};
    unsigned int created_look_count{0U};
    LabPreviewCommandHost* preview_host{nullptr};
    std::unique_ptr<emberlights::Os2lService> os2l_service;
};

void configure_product_os2l(LabState& state) noexcept {
    if (!kProductShell) {
        return;
    }
    if (!state.os2l_service) {
        state.os2l_service = std::make_unique<emberlights::Os2lService>();
    }
    const auto snapshot = state.document.snapshot();
    const auto& settings = snapshot.document.connections;
    state.os2l_service->publish_blackout(true);
    static_cast<void>(state.os2l_service->configure(
        settings.os2l_enabled,
        settings.os2l_bind,
        settings.os2l_port));
}

[[nodiscard]] std::string os2l_state_text(
    const emberlights::Os2lServiceStatus& status) {
    if (!status.enabled) {
        return "Disabled";
    }
    if (status.client_connected) {
        return "Connected";
    }
    switch (status.listener) {
    case showcore::Os2lServerState::Listening:
        return "Listening";
    case showcore::Os2lServerState::Fault:
        return "Needs attention";
    case showcore::Os2lServerState::ClientConnected:
        return "Connected";
    case showcore::Os2lServerState::Closed:
        return status.running ? "Starting" : "Stopped";
    }
    return "Unavailable";
}

[[nodiscard]] std::string os2l_detail_text(
    const emberlights::Os2lServiceStatus& status) {
    if (!status.enabled) {
        return "OS2L is disabled in this project";
    }
    const auto port = status.bound_port != 0U
        ? status.bound_port
        : status.configured_port;
    auto detail = std::string(status.configured_bind.view()) + ":" +
        std::to_string(port);
    if (status.client_connected) {
        detail += " • VirtualDJ session " +
            std::to_string(status.session_epoch);
    } else if (status.listener == showcore::Os2lServerState::Fault) {
        detail += " • socket error " +
            std::to_string(status.last_socket_error);
    } else {
        detail += " • waiting for VirtualDJ";
    }
    return detail;
}

[[nodiscard]] std::string os2l_status_token(
    const emberlights::Os2lServiceStatus& status) {
    return std::to_string(status.running) + ":" +
        std::to_string(status.enabled) + ":" +
        std::to_string(static_cast<unsigned int>(status.listener)) + ":" +
        std::to_string(status.client_connected) + ":" +
        std::to_string(status.session_epoch) + ":" +
        std::to_string(status.bound_port) + ":" +
        std::to_string(status.last_socket_error);
}

[[nodiscard]] emberlights::ChannelDefinition direct_channel(
    showcore::Property property,
    std::uint16_t offset) {
    emberlights::ChannelDefinition channel;
    channel.property = property;
    channel.coarse_offset = offset;
    channel.encoding = showcore::ChannelEncoding::Linear8;
    channel.highlight_value = 255U;
    channel.owner = "fixture";
    return channel;
}

[[nodiscard]] emberlights::ProjectDocument make_lab_project() {
    auto project = emberlights::make_starter_project();
    project.id = "slint-fixtures-looks-lab";
    project.name = "Ballroom Rig";

    emberlights::FixtureProfileDefinition profile;
    profile.id = "local.visual.mover";
    profile.manufacturer = "Ember Test";
    profile.model = "Visual Mover";
    profile.mode = "12 channel";
    profile.name = "Ember Test Visual Mover (12 channel)";
    profile.source = showcore::FixtureProfileSource::Local;
    profile.source_revision = "shell-v1";
    profile.footprint = 12U;
    profile.channels = {
        direct_channel(showcore::Property::Intensity, 0U),
        direct_channel(showcore::Property::Red, 1U),
        direct_channel(showcore::Property::Green, 2U),
        direct_channel(showcore::Property::Blue, 3U),
        direct_channel(showcore::Property::White, 4U),
        direct_channel(showcore::Property::Amber, 5U),
        direct_channel(showcore::Property::Pan, 6U),
        direct_channel(showcore::Property::Tilt, 7U),
        direct_channel(showcore::Property::Focus, 9U),
        direct_channel(showcore::Property::Zoom, 10U)};

    emberlights::ChannelDefinition gobo;
    gobo.property = showcore::Property::Count;
    gobo.coarse_offset = 8U;
    gobo.encoding = showcore::ChannelEncoding::Discrete8;
    gobo.owner = "gobo";
    emberlights::ChannelCapabilityDefinition open;
    open.id = "open";
    open.name = "Open";
    open.property = showcore::Property::Gobo;
    open.dmx_min = 0U;
    open.dmx_max = 31U;
    open.preferred_value = 0U;
    open.behavior = showcore::ChannelCapabilityBehavior::Slot;
    open.access = showcore::ChannelCapabilityAccess::Selectable;
    gobo.capabilities.push_back(open);
    auto dots = open;
    dots.id = "dots";
    dots.name = "Dots";
    dots.dmx_min = 32U;
    dots.dmx_max = 63U;
    dots.preferred_value = 48U;
    gobo.capabilities.push_back(std::move(dots));
    profile.channels.insert(profile.channels.begin() + 8, std::move(gobo));

    emberlights::ChannelDefinition strobe;
    strobe.property = showcore::Property::Count;
    strobe.coarse_offset = 11U;
    strobe.encoding = showcore::ChannelEncoding::Discrete8;
    strobe.owner = "shutter";
    emberlights::ChannelCapabilityDefinition slow_fast;
    slow_fast.id = "slow-fast";
    slow_fast.name = "Slow to fast";
    slow_fast.property = showcore::Property::Strobe;
    slow_fast.dmx_min = 32U;
    slow_fast.dmx_max = 127U;
    slow_fast.preferred_value = 80U;
    slow_fast.behavior = showcore::ChannelCapabilityBehavior::Continuous;
    slow_fast.access = showcore::ChannelCapabilityAccess::Selectable;
    strobe.capabilities.push_back(std::move(slow_fast));
    profile.channels.push_back(std::move(strobe));
    project.fixture_profiles.push_back(std::move(profile));

    project.fixtures.push_back({
        "fixture.mover.left", "Mover Left", "local.visual.mover", 1U, 1U,
        {"dance-floor", "movers"}});
    project.fixtures.push_back({
        "fixture.mover.right", "Mover Right", "local.visual.mover", 1U, 21U,
        {"dance-floor", "movers"}});
    project.groups.push_back({
        "group.movers", "Dance Floor Movers",
        {"fixture.mover.left", "fixture.mover.right"}});

    emberlights::LookDefinition ceremony;
    ceremony.id = "look.ceremony";
    ceremony.name = "Ceremony Wash";
    ceremony.fade_ms = 1200U;
    for (const auto fixture : {std::string_view("fixture.mover.left"),
                               std::string_view("fixture.mover.right")}) {
        ceremony.assignments.push_back({
            std::string(fixture), showcore::Property::Intensity,
            showcore::PropertyValue::set(0.72F)});
        ceremony.assignments.push_back({
            std::string(fixture), showcore::Property::Red,
            showcore::PropertyValue::set(0.92F)});
        ceremony.assignments.push_back({
            std::string(fixture), showcore::Property::Green,
            showcore::PropertyValue::set(0.42F)});
        ceremony.assignments.push_back({
            std::string(fixture), showcore::Property::Blue,
            showcore::PropertyValue::set(0.12F)});
        ceremony.assignments.push_back({
            std::string(fixture), showcore::Property::White,
            showcore::PropertyValue::set(0.16F)});
        ceremony.assignments.push_back({
            std::string(fixture), showcore::Property::Amber,
            showcore::PropertyValue::set(0.28F)});
        ceremony.assignments.push_back({
            std::string(fixture), showcore::Property::Strobe,
            showcore::PropertyValue::set(0.75F)});
        ceremony.assignments.push_back({
            std::string(fixture), showcore::Property::Pan,
            showcore::PropertyValue::set(0.50F)});
        ceremony.assignments.push_back({
            std::string(fixture), showcore::Property::Tilt,
            showcore::PropertyValue::set(0.58F)});
    }
    project.looks.push_back(std::move(ceremony));
    project.looks.push_back({
        "look.dance", "Open Dance", 450U,
        {{"fixture.mover.left", showcore::Property::Intensity,
          showcore::PropertyValue::set(0.85F)},
         {"fixture.mover.right", showcore::Property::Intensity,
          showcore::PropertyValue::set(0.85F)}}});
    return project;
}

[[nodiscard]] std::optional<std::size_t> look_index_for(
    const emberlights::ProjectDocument& project,
    std::string_view look_id) noexcept {
    const auto found = std::find_if(
        project.looks.begin(), project.looks.end(),
        [look_id](const auto& look) { return look.id == look_id; });
    if (found == project.looks.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(
        std::distance(project.looks.begin(), found));
}

[[nodiscard]] emberlights::ProjectDocument project_for_presentation(
    const LabState& state) {
    auto snapshot = state.document.snapshot();
    auto project = std::move(snapshot.document);
    if (!state.selected_look_draft.has_value() ||
        state.selected_look_draft->base_generation != snapshot.generation) {
        return project;
    }

    const auto& draft = *state.selected_look_draft;
    if (draft.source_index.has_value()) {
        if (*draft.source_index < project.looks.size() &&
            project.looks[*draft.source_index].id == draft.look.id) {
            project.looks[*draft.source_index] = draft.look;
        }
    } else {
        project.looks.push_back(draft.look);
    }
    return project;
}

[[nodiscard]] emberlights::UiInvocationResult ui_result(
    emberlights::StaticLookPreviewRequestResult result) noexcept {
    switch (result) {
    case emberlights::StaticLookPreviewRequestResult::Accepted:
        return emberlights::UiInvocationResult::Accepted;
    case emberlights::StaticLookPreviewRequestResult::NoChange:
        return emberlights::UiInvocationResult::NoChange;
    case emberlights::StaticLookPreviewRequestResult::Unavailable:
        return emberlights::UiInvocationResult::Unavailable;
    case emberlights::StaticLookPreviewRequestResult::InvalidArguments:
        return emberlights::UiInvocationResult::InvalidArguments;
    }
    return emberlights::UiInvocationResult::InternalError;
}

class LabPreviewCommandHost final : public emberlights::UiAppCommandHost {
public:
    LabPreviewCommandHost(LabState& state, bool physical_enabled)
        : state_(state),
          coordinator_(runner_, physical_enabled),
          facade_(runner_, *this) {}

    [[nodiscard]] emberlights::UiCommandFacade& facade() noexcept {
        return facade_;
    }

    [[nodiscard]] emberlights::StaticLookPreviewCoordinatorStatus status() {
        return coordinator_.status();
    }

    [[nodiscard]] bool physical_available(
        const emberlights::ProjectDocument& project) const noexcept {
        return coordinator_.physical_available(project);
    }

    void update_selected_draft() noexcept {
        try {
            const auto preview = coordinator_.status();
            if (!state_.selected_look_draft.has_value() ||
                preview.mode == emberlights::StaticLookPreviewMode::None ||
                preview.look_id != state_.selected_look_id ||
                preview.target_id != state_.selected_target_id ||
                (preview.state !=
                     emberlights::StaticLookPreviewCoordinatorState::Starting &&
                 preview.state !=
                     emberlights::StaticLookPreviewCoordinatorState::Active &&
                 preview.state !=
                     emberlights::StaticLookPreviewCoordinatorState::Updating)) {
                return;
            }
            const auto snapshot = state_.document.snapshot();
            const auto result = coordinator_.update(
                snapshot.document,
                *state_.selected_look_draft,
                state_.selected_target_id);
            if (result != emberlights::StaticLookPreviewRequestResult::Accepted &&
                result != emberlights::StaticLookPreviewRequestResult::NoChange) {
                state_.operation_message =
                    "The active preview could not accept this draft update.";
            }
        } catch (...) {
            state_.operation_message =
                "The preview update failed before it reached the preview worker.";
        }
    }

    void stop_for_context_change() noexcept {
        static_cast<void>(facade_.invoke(
            {emberlights::UiCommandId::StaticLookPreviewStop}));
    }

    [[nodiscard]] emberlights::UiInvocationResult ui_start_show()
        noexcept override {
        return emberlights::UiInvocationResult::Unsupported;
    }

    [[nodiscard]] emberlights::UiInvocationResult ui_stop_show()
        noexcept override {
        return emberlights::UiInvocationResult::Unsupported;
    }

    [[nodiscard]] emberlights::UiInvocationResult ui_start_static_look_preview(
        std::string_view look_id,
        std::string_view target_id,
        emberlights::UiStaticLookPreviewMode mode) noexcept override {
        try {
            if (!state_.selected_look_draft.has_value() ||
                state_.selected_look_draft->look.id != look_id ||
                state_.selected_look_id != look_id ||
                state_.selected_target_id != target_id) {
                return emberlights::UiInvocationResult::NotFound;
            }
            const auto preview_mode =
                mode == emberlights::UiStaticLookPreviewMode::Simulation
                ? emberlights::StaticLookPreviewMode::Simulation
                : mode == emberlights::UiStaticLookPreviewMode::Physical
                    ? emberlights::StaticLookPreviewMode::Physical
                    : emberlights::StaticLookPreviewMode::None;
            const auto snapshot = state_.document.snapshot();
            const auto result = coordinator_.start(
                snapshot.document,
                *state_.selected_look_draft,
                target_id,
                preview_mode);
            return ui_result(result);
        } catch (...) {
            return emberlights::UiInvocationResult::InternalError;
        }
    }

    [[nodiscard]] emberlights::UiInvocationResult
    ui_stop_static_look_preview() noexcept override {
        return ui_result(coordinator_.stop());
    }

private:
    LabState& state_;
    emberlights::RunnerService runner_;
    emberlights::StaticLookPreviewCoordinator coordinator_;
    emberlights::UiCommandFacade facade_;
};

[[nodiscard]] bool preview_busy(
    emberlights::StaticLookPreviewCoordinatorState state) noexcept {
    return state == emberlights::StaticLookPreviewCoordinatorState::Starting ||
        state == emberlights::StaticLookPreviewCoordinatorState::Active ||
        state == emberlights::StaticLookPreviewCoordinatorState::Updating ||
        state == emberlights::StaticLookPreviewCoordinatorState::Stopping;
}

[[nodiscard]] std::string preview_status_text(
    const emberlights::StaticLookPreviewCoordinatorStatus& status) {
    if (status.state == emberlights::StaticLookPreviewCoordinatorState::Stopped) {
        return "Preview stopped";
    }
    if (status.state == emberlights::StaticLookPreviewCoordinatorState::TimedOut) {
        return "Preview timed out • fixtures blacked out";
    }
    if (status.state == emberlights::StaticLookPreviewCoordinatorState::Fault) {
        return "Preview fault • " + status.error;
    }
    const auto mode = status.mode == emberlights::StaticLookPreviewMode::Physical
        ? std::string("Fixture preview")
        : std::string("Offline simulation");
    return mode + " • " +
        emberlights::static_look_preview_coordinator_state_name(status.state);
}

[[nodiscard]] std::string preview_detail_text(
    const emberlights::StaticLookPreviewCoordinatorStatus& status,
    bool output_configured) {
    if (status.mode == emberlights::StaticLookPreviewMode::Simulation &&
        preview_busy(status.state)) {
        const auto digest = status.frame_sha256.empty()
            ? std::string("rendering")
            : "frame " + status.frame_sha256.substr(0U, 12U);
        return "OFFLINE • no DMX output • " + digest;
    }
    if (status.mode == emberlights::StaticLookPreviewMode::Physical &&
        preview_busy(status.state)) {
        const auto seconds = (status.remaining_ms + 999U) / 1'000U;
        return "LIVE STOPPED PREVIEW • " +
            std::to_string(static_cast<unsigned int>(
                status.output_cap * 100.0F + 0.5F)) +
            "% max • " + std::to_string(seconds) + "s • " +
            std::to_string(status.selected_fixture_count) + " fixtures";
    }
    if (status.state == emberlights::StaticLookPreviewCoordinatorState::Fault ||
        status.state == emberlights::StaticLookPreviewCoordinatorState::TimedOut) {
        return "Output is stopped and terminal blackout has been requested.";
    }
    if (!status.physical_enabled) {
        return "Simulation uses the same renderer offline • fixture output is locked unless explicitly armed at launch";
    }
    if (!output_configured) {
        return "Simulation ready • fixture output is armed but no output adapter is configured";
    }
    return "Simulation ready • bounded fixture preview: 35% max, 30s, selected target only";
}

[[nodiscard]] std::string preview_status_token(
    const emberlights::StaticLookPreviewCoordinatorStatus& status) {
    return std::string(
               emberlights::static_look_preview_coordinator_state_name(
                   status.state)) + "|" +
        emberlights::static_look_preview_mode_name(status.mode) + "|" +
        std::to_string((status.remaining_ms + 999U) / 1'000U) + "|" +
        std::to_string(status.sequence) + "|" +
        std::to_string(status.update_count) + "|" + status.frame_sha256 + "|" +
        status.error;
}

[[nodiscard]] bool reload_selected_look_draft(LabState& state) {
    const auto snapshot = state.document.snapshot();
    const auto index = look_index_for(snapshot.document, state.selected_look_id);
    if (!index.has_value()) {
        state.selected_look_draft.reset();
        return false;
    }
    state.selected_look_draft = emberlights::load_static_look_draft(
        snapshot, *index);
    state.draft_dirty = false;
    return state.selected_look_draft.has_value();
}

void select_available_look(LabState& state) {
    const auto snapshot = state.document.snapshot();
    if (!look_index_for(snapshot.document, state.selected_look_id).has_value()) {
        state.selected_look_id = snapshot.document.looks.empty()
            ? std::string{}
            : snapshot.document.looks.front().id;
    }
    static_cast<void>(reload_selected_look_draft(state));
}

[[nodiscard]] bool activate_project(
    LabState& state,
    emberlights::ProjectDocument project,
    emberlights::StudioDocumentBoundary boundary,
    std::optional<std::filesystem::path> project_path) {
    const auto outcome = state.document.replace_document(
        state.document.generation(), std::move(project), boundary);
    if (!outcome) {
        state.operation_message = outcome.message;
        return false;
    }
    state.project_path = std::move(project_path);
    const auto snapshot = state.document.snapshot();
    const auto profile_exists = std::any_of(
        snapshot.document.fixture_profiles.begin(),
        snapshot.document.fixture_profiles.end(),
        [&](const auto& profile) { return profile.id == state.selected_profile_id; });
    if (!profile_exists) {
        state.selected_profile_id = snapshot.document.fixture_profiles.empty()
            ? std::string{}
            : snapshot.document.fixture_profiles.front().id;
    }
    const auto fixture_exists = std::any_of(
        snapshot.document.fixtures.begin(),
        snapshot.document.fixtures.end(),
        [&](const auto& fixture) { return fixture.id == state.selected_target_id; });
    const auto group_exists = std::any_of(
        snapshot.document.groups.begin(),
        snapshot.document.groups.end(),
        [&](const auto& group) { return group.id == state.selected_target_id; });
    if (!fixture_exists && !group_exists) {
        state.selected_target_id = !snapshot.document.groups.empty()
            ? snapshot.document.groups.front().id
            : snapshot.document.fixtures.empty()
                ? std::string{}
                : snapshot.document.fixtures.front().id;
    }
    state.selected_choice_id.clear();
    state.draft_dirty = false;
    select_available_look(state);
    state.operation_message = outcome.message;
    return true;
}

[[nodiscard]] std::string path_label(const LabState& state) {
    if (!state.project_path.has_value()) {
        return kProductShell ? "unsaved project" : "unsaved lab document";
    }
    const auto filename = state.project_path->filename().string();
    return filename.empty() ? state.project_path->string() : filename;
}

[[nodiscard]] std::optional<std::filesystem::path> choose_project_path(
    bool save,
    const std::optional<std::filesystem::path>& current_path = std::nullopt) {
#if defined(_WIN32)
    std::array<wchar_t, 32'768> buffer{};
    if (current_path.has_value()) {
        const auto native = current_path->wstring();
        const auto count = std::min(native.size(), buffer.size() - 1U);
        std::copy_n(native.data(), count, buffer.data());
    }
    constexpr wchar_t filter[] =
        L"EmberLights Projects (*.emberlights)\0*.emberlights\0"
        L"All Files (*.*)\0*.*\0\0";
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.lpstrDefExt = L"emberlights";
    dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR |
        (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    const auto accepted = save
        ? GetSaveFileNameW(&dialog)
        : GetOpenFileNameW(&dialog);
    if (accepted == FALSE) {
        return std::nullopt;
    }
    return std::filesystem::path(buffer.data());
#else
    static_cast<void>(save);
    static_cast<void>(current_path);
    return std::nullopt;
#endif
}

[[nodiscard]] bool launch_safe_shell(const LabState& state) {
#if defined(_WIN32)
    std::array<wchar_t, 32'768> executable_path{};
    const auto copied = GetModuleFileNameW(
        nullptr,
        executable_path.data(),
        static_cast<DWORD>(executable_path.size()));
    if (copied == 0U ||
        static_cast<std::size_t>(copied) >= executable_path.size()) {
        return false;
    }
    const auto safe_path = std::filesystem::path(executable_path.data())
        .parent_path() / L"EmberLights-Safe.exe";
    std::wstring arguments;
    if (state.project_path.has_value()) {
        arguments = L"\"" + state.project_path->wstring() + L"\"";
    }
    const auto safe_directory = safe_path.parent_path();
    const auto result = ShellExecuteW(
        nullptr,
        L"open",
        safe_path.c_str(),
        arguments.empty() ? nullptr : arguments.c_str(),
        safe_directory.c_str(),
        SW_SHOWNORMAL);
    return reinterpret_cast<std::intptr_t>(result) > 32;
#else
    static_cast<void>(state);
    return false;
#endif
}

[[nodiscard]] bool open_project(
    LabState& state,
    const std::filesystem::path& path) {
    emberlights::ProjectDocument loaded;
    const auto load_result = emberlights::load_project(path, loaded, true);
    if (!load_result) {
        state.operation_message = "Unable to open the project: " +
            load_result.message;
        return false;
    }
    if (!activate_project(
            state,
            std::move(loaded),
            load_result.recovered_from_backup
                ? emberlights::StudioDocumentBoundary::NewDocument
                : emberlights::StudioDocumentBoundary::OpenedDocument,
            path)) {
        return false;
    }
    state.operation_message = load_result.recovered_from_backup
        ? "Recovered the project backup. Save Project repairs the primary file."
        : "Opened the project as the durable Studio baseline.";
    return true;
}

[[nodiscard]] std::string string_from(const slint::SharedString& value) {
    return std::string(std::string_view(value));
}

[[nodiscard]] slint::SharedString shared_string(std::string_view value) {
    return slint::SharedString(value);
}

[[nodiscard]] ShellListItem shell_list_item(
    std::string_view stable_id,
    std::string_view title,
    std::string_view detail,
    std::string_view badge,
    bool selected,
    bool warning) {
    ShellListItem item;
    item.stable_id = shared_string(stable_id);
    item.title = shared_string(title);
    item.detail = shared_string(detail);
    item.badge = shared_string(badge);
    item.selected = selected;
    item.warning = warning;
    return item;
}

[[nodiscard]] ChoiceTileItem choice_tile_item(
    const emberlights::FixturesLooksControlBinding& control) {
    ChoiceTileItem item;
    item.choice_id = shared_string(control.choice_id);
    item.parameter_id = shared_string(control.parameter_id);
    item.title = shared_string(control.profile_function
        ? std::string_view(control.property_label)
        : (control.widget_label.empty()
              ? std::string_view(control.property_label)
              : std::string_view(control.widget_label)));
    item.detail = shared_string(
        control.property_label + " • " + control.ownership_text + " • " +
        control.availability_text);
    item.ownership = shared_string(
        emberlights::static_look_ownership_state_name(control.ownership));
    item.active = control.value_matches_choice;
    item.selected = control.selected;
    item.enabled = control.enabled;
    item.safety_restricted = control.safety_restricted;
    return item;
}

[[nodiscard]] ControlGroupItem control_group_item(
    const emberlights::FixturesLooksControlCategoryItem& category,
    bool search_active) {
    ControlGroupItem item;
    item.stable_id = shared_string(category.stable_id);
    item.title = shared_string(category.label);
    item.count_label = shared_string(
        std::to_string(search_active
                ? category.search_match_count
                : category.total_count) +
        (search_active ? " match" : " total"));
    item.selected = category.selected;
    item.advanced = category.advanced;
    return item;
}

[[nodiscard]] ParameterControlItem parameter_control_item(
    const emberlights::FixturesLooksControlBinding& control) {
    ParameterControlItem item;
    item.choice_id = shared_string(control.choice_id);
    item.parameter_id = shared_string(control.parameter_id);
    item.section_label = shared_string(control.section_label);
    item.title = shared_string(control.profile_function
        ? std::string_view(control.property_label)
        : (control.widget_label.empty()
              ? std::string_view(control.property_label)
              : std::string_view(control.widget_label)));
    const auto coverage = std::to_string(control.assigned_fixture_count) +
        " of " + std::to_string(control.target_fixture_count) + " owned";
    item.detail = shared_string(
        control.section_label + " • " + control.control_kind + " • " +
        coverage + " • " + control.availability_text +
        (control.safety_restricted ? " • safety-limited" : ""));
    item.kind = shared_string(control.control_kind);
    item.ownership = shared_string(
        emberlights::static_look_ownership_state_name(control.ownership));
    item.value = control.normalized_value;
    item.enabled = control.enabled;
    item.safety_restricted = control.safety_restricted;
    item.mixed = control.value_mixed;
    item.active = control.value_matches_choice;
    item.profile_function = control.profile_function;
    return item;
}

[[nodiscard]] emberlights::FixturesLooksShellQuery query_from(
    const LabState& state,
    bool advanced_open) {
    emberlights::FixturesLooksShellQuery query;
    query.profile_search = state.profile_search;
    query.static_look_search = state.look_search;
    query.selected_profile_id = state.selected_profile_id;
    query.selected_target_id = state.selected_target_id;
    query.selected_static_look_id = state.selected_look_id;
    query.selected_choice_id = state.selected_choice_id;
    query.control_search = state.control_search;
    query.control_category = state.control_category;
    query.include_advanced = advanced_open;
    query.advanced_open = advanced_open;
    query.viewport_width = 1366;
    query.viewport_height = 768;
    return query;
}

[[nodiscard]] const emberlights::FixturesLooksControlBinding* control_for(
    const emberlights::FixturesLooksShellModel& model,
    showcore::Property property) noexcept {
    const auto found = std::find_if(
        model.controls.begin(), model.controls.end(),
        [property](const auto& control) { return control.property == property; });
    return found == model.controls.end() ? nullptr : &*found;
}

[[nodiscard]] const emberlights::FixturesLooksControlBinding* control_for(
    const emberlights::FixturesLooksShellModel& model,
    std::string_view choice_id) noexcept {
    const auto found = std::find_if(
        model.controls.begin(), model.controls.end(),
        [choice_id](const auto& control) { return control.choice_id == choice_id; });
    return found == model.controls.end() ? nullptr : &*found;
}

[[nodiscard]] std::string ownership_name(
    const emberlights::FixturesLooksControlBinding* control) {
    return control == nullptr
        ? "release"
        : std::string(emberlights::static_look_ownership_state_name(
              control->ownership));
}

[[nodiscard]] float control_value(
    const emberlights::FixturesLooksControlBinding* control,
    float fallback = 0.0F) noexcept {
    return control == nullptr ? fallback : control->normalized_value;
}

void refresh_ui(const FixturesLooksLab& ui, LabState& state) {
    const auto snapshot = state.document.snapshot();
    const auto project = project_for_presentation(state);
    const auto model = emberlights::build_fixtures_looks_shell_model(
        project, query_from(state, ui.get_advanced_open()));
    if (model.selected_control_category_id == "all" &&
        state.control_category.has_value()) {
        state.control_category.reset();
    }
    const auto modified = snapshot.dirty || state.draft_dirty;
    const auto save_state = std::string(modified ? "Modified • " : "Saved • ") +
        path_label(state);
    const auto history_state =
        "Generation " + std::to_string(snapshot.generation) + " • " +
        std::to_string(snapshot.undo_count) + " undo • " +
        std::to_string(snapshot.redo_count) + " redo";
    auto preview = emberlights::StaticLookPreviewCoordinatorStatus{};
    if (state.preview_host != nullptr) {
        try {
            preview = state.preview_host->status();
        } catch (...) {
            preview.state = emberlights::StaticLookPreviewCoordinatorState::Fault;
            preview.error = "status-unavailable";
        }
    }
    state.preview_status_token = preview_status_token(preview);
    const auto output_configured =
        emberlights::static_look_physical_preview_output_configured(
            project.connections);
    const auto active_preview = preview_busy(preview.state);
    const auto physical_ready = state.preview_host != nullptr &&
        !active_preview && state.preview_host->physical_available(project);
    const auto output_state =
        preview.mode == emberlights::StaticLookPreviewMode::Physical &&
            active_preview
        ? "Preview " + std::to_string(static_cast<unsigned int>(
              preview.output_cap * 100.0F + 0.5F)) + "%"
        : preview.mode == emberlights::StaticLookPreviewMode::Simulation &&
              active_preview
            ? std::string("Offline simulation")
            : preview.physical_enabled && output_configured
                ? std::string("Preview armed")
                : std::string("No output");
    const auto os2l_status = state.os2l_service
        ? state.os2l_service->status()
        : emberlights::Os2lServiceStatus{};
    state.os2l_status_token = os2l_status_token(os2l_status);

    ui.set_project_name(shared_string(model.project_name));
    ui.set_product_shell(kProductShell);
    ui.set_build_label(shared_string(
        std::string("Beta ") + EMBERLIGHTS_VERSION + " • " +
        std::string(EMBERLIGHTS_COMMIT).substr(
            0U, std::min<std::size_t>(8U, std::string_view(EMBERLIGHTS_COMMIT).size()))));
    ui.set_has_project_path(state.project_path.has_value());
    ui.set_save_state(shared_string(save_state));
    ui.set_validation_state(shared_string(model.validation_status));
    ui.set_output_state(shared_string(output_state));
    ui.set_os2l_state(shared_string(os2l_state_text(os2l_status)));
    ui.set_os2l_detail(shared_string(os2l_detail_text(os2l_status)));
    ui.set_os2l_connected(os2l_status.client_connected);
    ui.set_os2l_fault(
        os2l_status.listener == showcore::Os2lServerState::Fault);
    ui.set_workspace_message(shared_string(state.operation_message.empty()
        ? model.message
        : state.operation_message));
    ui.set_preview_status(shared_string(preview_status_text(preview)));
    ui.set_preview_detail(shared_string(
        preview_detail_text(preview, output_configured)));
    ui.set_preview_state(shared_string(
        emberlights::static_look_preview_coordinator_state_name(
            preview.state)));
    ui.set_preview_mode(shared_string(
        emberlights::static_look_preview_mode_name(preview.mode)));
    ui.set_preview_active(active_preview);
    ui.set_can_simulate_preview(model.can_preview && !active_preview);
    ui.set_can_physical_preview(model.can_preview && physical_ready);
    ui.set_physical_preview_enabled(preview.physical_enabled);
    ui.set_preview_remaining_seconds(static_cast<int>(
        (preview.remaining_ms + 999U) / 1'000U));
    ui.set_preview_output_cap_percent(static_cast<int>(
        preview.output_cap * 100.0F + 0.5F));
    ui.set_preview_fixture_count(static_cast<int>(
        preview.selected_fixture_count));
    ui.set_can_edit(model.can_edit);
    ui.set_can_save_look(state.draft_dirty);
    ui.set_can_save_project(
        state.project_path.has_value() && modified);
    ui.set_can_undo(state.draft_dirty || snapshot.can_undo);
    ui.set_can_redo(!state.draft_dirty && snapshot.can_redo);
    ui.set_advanced_available(model.advanced_available);
    ui.set_history_state(shared_string(history_state));
    ui.set_live_running(model.live_running);
    ui.set_profile_search(shared_string(state.profile_search));
    ui.set_look_search(shared_string(state.look_search));
    ui.set_parameter_search(shared_string(state.control_search));
    ui.set_control_summary(shared_string(model.control_summary));
    ui.set_selected_control_group(shared_string(
        model.selected_control_category_id));
    ui.set_visible_control_count(static_cast<int>(
        model.control_visible_count));

    std::vector<ShellListItem> profile_items;
    profile_items.reserve(model.profiles.size());
    for (const auto& item : model.profiles) {
        const auto detail =
            item.manufacturer + " • " + item.model + " • " + item.mode;
        const auto badge = item.source_label + " • " + item.footprint_label;
        profile_items.push_back(shell_list_item(
            item.stable_id,
            item.name,
            detail,
            badge,
            item.selected,
            item.read_only));
    }
    ui.set_profile_items(
        std::make_shared<slint::VectorModel<ShellListItem>>(
            std::move(profile_items)));

    std::vector<ShellListItem> target_items;
    target_items.reserve(model.targets.size());
    for (const auto& item : model.targets) {
        const auto badge = item.group
            ? std::to_string(item.fixture_count) + " fixtures"
            : std::string("Fixture");
        target_items.push_back(shell_list_item(
            item.stable_id,
            item.name,
            item.detail,
            badge,
            item.selected,
            !item.complete));
    }
    ui.set_target_items(
        std::make_shared<slint::VectorModel<ShellListItem>>(
            std::move(target_items)));

    std::vector<ShellListItem> look_items;
    look_items.reserve(model.static_looks.size());
    for (const auto& item : model.static_looks) {
        const auto badge = std::to_string(item.fade_ms) + " ms fade";
        look_items.push_back(shell_list_item(
            item.stable_id,
            item.name,
            item.detail,
            badge,
            item.selected,
            false));
    }
    ui.set_look_items(
        std::make_shared<slint::VectorModel<ShellListItem>>(
            std::move(look_items)));

    const auto selected_target = std::find_if(
        model.targets.begin(), model.targets.end(),
        [](const auto& item) { return item.selected; });
    ui.set_selected_target_name(shared_string(
        selected_target == model.targets.end()
            ? std::string_view("Select a fixture or group")
            : std::string_view(selected_target->name)));
    ui.set_selected_target_detail(shared_string(
        selected_target == model.targets.end()
            ? std::string_view("No target selected")
            : std::string_view(selected_target->detail)));
    ui.set_selected_look_name(shared_string(
        model.selected_static_look_name.empty()
            ? std::string_view("Select or create a Static Look")
            : std::string_view(model.selected_static_look_name)));
    if (const auto* selected_profile = emberlights::find_fixture_profile(
            project, model.selected_profile_id)) {
        ui.set_advanced_profile_title(shared_string(
            selected_profile->manufacturer + " • " + selected_profile->model +
            " • " + selected_profile->mode));
        ui.set_advanced_profile_detail(shared_string(
            selected_profile->name + " • revision " +
            (selected_profile->source_revision.empty()
                 ? std::string("unknown")
                 : selected_profile->source_revision) + " • " +
            std::to_string(selected_profile->footprint) + " channels"));
    } else {
        ui.set_advanced_profile_title("No fixture profile selected");
        ui.set_advanced_profile_detail("Select or repair a profile to inspect provenance.");
    }
    ui.set_advanced_patch_detail(shared_string(
        selected_target == model.targets.end()
            ? std::string("No patch target selected")
            : selected_target->name + " • " + selected_target->detail));

    std::vector<ShellListItem> diagnostic_items;
    diagnostic_items.reserve(model.control_diagnostics.size());
    for (const auto& diagnostic : model.control_diagnostics) {
        diagnostic_items.push_back(shell_list_item(
            diagnostic.stable_id,
            diagnostic.title,
            diagnostic.detail,
            diagnostic.provenance,
            diagnostic.selected,
            diagnostic.warning));
    }
    ui.set_diagnostic_item_count(static_cast<int>(diagnostic_items.size()));
    ui.set_diagnostic_items(
        std::make_shared<slint::VectorModel<ShellListItem>>(
            std::move(diagnostic_items)));

    std::vector<ControlGroupItem> control_group_items;
    control_group_items.reserve(model.control_categories.size());
    for (const auto& category : model.control_categories) {
        control_group_items.push_back(control_group_item(
            category, !model.control_search.empty()));
    }
    ui.set_control_group_items(
        std::make_shared<slint::VectorModel<ControlGroupItem>>(
            std::move(control_group_items)));

    const auto* pan = control_for(model, showcore::Property::Pan);
    const auto* tilt = control_for(model, showcore::Property::Tilt);
    const auto xy_visible = pan != nullptr && tilt != nullptr &&
        pan->control_kind == "XY position pad" &&
        tilt->control_kind == "XY position pad";
    ui.set_xy_visible(xy_visible);
    ui.set_pan_choice_id(shared_string(
        !xy_visible ? std::string_view{} : std::string_view(pan->choice_id)));
    ui.set_pan_value(control_value(pan, 0.5F));
    ui.set_tilt_choice_id(shared_string(
        !xy_visible ? std::string_view{} : std::string_view(tilt->choice_id)));
    ui.set_tilt_value(control_value(tilt, 0.5F));
    ui.set_position_ownership(shared_string(
        xy_visible && pan->ownership == tilt->ownership
            ? ownership_name(pan)
            : std::string("mixed")));

    std::vector<ParameterControlItem> color_items;
    for (const auto& control : model.controls) {
        if (control.control_kind == "color mixer") {
            color_items.push_back(parameter_control_item(control));
        }
    }
    const auto active_emitter_value = [&model](std::string_view parameter_id) {
        const auto found = std::find_if(
            model.controls.begin(), model.controls.end(),
            [parameter_id](const auto& control) {
                return control.control_kind == "color mixer" &&
                    control.parameter_id == parameter_id &&
                    control.ownership ==
                        emberlights::StaticLookOwnershipState::Set;
            });
        return found == model.controls.end()
            ? 0.0F
            : std::clamp(found->normalized_value, 0.0F, 1.0F);
    };
    const auto red = active_emitter_value("red");
    const auto green = active_emitter_value("green");
    const auto blue = active_emitter_value("blue");
    const auto white = active_emitter_value("white");
    const auto amber = active_emitter_value("amber");
    const auto supports_rgbwa_preview = std::any_of(
        model.controls.begin(), model.controls.end(), [](const auto& control) {
            return control.control_kind == "color mixer" &&
                (control.parameter_id == "red" ||
                 control.parameter_id == "green" ||
                 control.parameter_id == "blue" ||
                 control.parameter_id == "white" ||
                 control.parameter_id == "amber");
        });
    ui.set_color_preview_visible(supports_rgbwa_preview);
    ui.set_color_preview_red(red);
    ui.set_color_preview_green(green);
    ui.set_color_preview_blue(blue);
    ui.set_color_preview_white(white);
    ui.set_color_preview_amber(amber);
    ui.set_color_preview_detail(shared_string(
        "Read-only profile-backed preview • adjust each emitter below"));

    std::vector<ParameterFamilyItem> parameter_families;
    parameter_families.reserve(model.control_groups.size());
    for (const auto& group : model.control_groups) {
        if (group.control_kind == "color mixer" ||
            group.control_kind == "XY position pad") {
            continue;
        }
        std::vector<ParameterControlItem> value_controls;
        std::vector<ChoiceTileItem> choices;
        const emberlights::FixturesLooksControlBinding* ownership_control = nullptr;
        for (const auto& control : model.controls) {
            if (control.widget_id != group.stable_id) {
                continue;
            }
            if (ownership_control == nullptr || control.selected ||
                control.value_matches_choice) {
                ownership_control = &control;
            }
            if (control.accepts_value) {
                value_controls.push_back(parameter_control_item(control));
            } else {
                choices.push_back(choice_tile_item(control));
            }
        }
        if (ownership_control == nullptr) {
            continue;
        }

        ParameterFamilyItem family;
        family.stable_id = shared_string(group.stable_id);
        family.parameter_id = shared_string(group.parameter_id);
        family.section_label = shared_string(group.section_label);
        family.title = shared_string(group.label);
        family.kind = shared_string(group.control_kind);
        family.ownership = shared_string(
            emberlights::static_look_ownership_state_name(
                ownership_control->ownership));
        family.ownership_choice_id = shared_string(
            ownership_control->choice_id);
        family.value_count = static_cast<int>(value_controls.size());
        family.choice_count = static_cast<int>(choices.size());
        family.enabled = group.enabled;
        family.safety_restricted = group.safety_restricted;
        family.mixed = ownership_control->value_mixed;
        const auto profile_functions = std::to_string(
            group.profile_function_count) +
            (group.profile_function_count == 1U
                 ? " profile function"
                 : " profile functions");
        const auto coverage = std::to_string(
            ownership_control->assigned_fixture_count) + " of " +
            std::to_string(ownership_control->target_fixture_count) + " owned";
        family.detail = shared_string(
            profile_functions + " • " + coverage + " • " +
            ownership_control->availability_text +
            (group.degraded ? " • partial availability" : "") +
            (group.safety_restricted ? " • safety-limited" : ""));
        family.value_controls =
            std::make_shared<slint::VectorModel<ParameterControlItem>>(
                std::move(value_controls));
        family.choices =
            std::make_shared<slint::VectorModel<ChoiceTileItem>>(
                std::move(choices));
        parameter_families.push_back(std::move(family));
    }

    ui.set_color_item_count(static_cast<int>(color_items.size()));
    ui.set_parameter_family_count(static_cast<int>(
        parameter_families.size()));
    ui.set_color_items(
        std::make_shared<slint::VectorModel<ParameterControlItem>>(
            std::move(color_items)));
    ui.set_parameter_family_items(
        std::make_shared<slint::VectorModel<ParameterFamilyItem>>(
            std::move(parameter_families)));
}

template<typename Mutation>
void mutate_selected_look(
    const FixturesLooksLab& ui,
    LabState& state,
    Mutation mutation) {
    if (!state.selected_look_draft.has_value() &&
        !reload_selected_look_draft(state)) {
        state.operation_message = "Select a Static Look before editing.";
        refresh_ui(ui, state);
        return;
    }
    const auto snapshot = state.document.snapshot();
    if (state.selected_look_draft->base_generation != snapshot.generation) {
        state.operation_message =
            "The Studio document changed. Reload the Static Look before editing.";
        refresh_ui(ui, state);
        return;
    }
    const auto outcome = mutation(
        *state.selected_look_draft, snapshot.document);
    if (!outcome) {
        state.operation_message = "That control is unavailable for the selected target.";
        refresh_ui(ui, state);
        return;
    }
    if (outcome.result == emberlights::StaticLookAuthoringResult::Applied) {
        state.draft_dirty = true;
        state.operation_message = outcome.warnings.empty()
            ? "Static Look draft updated. Save Look commits one Undo transaction."
            : outcome.warnings.front();
        if (state.preview_host != nullptr) {
            state.preview_host->update_selected_draft();
        }
    } else {
        state.operation_message = outcome.warnings.empty()
            ? "The Static Look already has that value."
            : outcome.warnings.front();
    }
    refresh_ui(ui, state);
}

[[nodiscard]] emberlights::StudioMutationOutcome commit_selected_look(
    LabState& state) {
    const auto snapshot = state.document.snapshot();
    if (!state.selected_look_draft.has_value()) {
        return {
            emberlights::StudioMutationResult::InvalidCandidate,
            snapshot.generation,
            emberlights::validate_project(snapshot.document),
            "Select a Static Look before saving."};
    }
    if (!state.draft_dirty) {
        return {
            emberlights::StudioMutationResult::NoChange,
            snapshot.generation,
            emberlights::validate_project(snapshot.document),
            "The selected Static Look has no uncommitted changes."};
    }

    auto outcome = emberlights::commit_static_look_draft(
        state.document, *state.selected_look_draft);
    if (outcome) {
        state.draft_dirty = false;
        state.selected_look_id = state.selected_look_draft->look.id;
        static_cast<void>(reload_selected_look_draft(state));
    }
    return outcome;
}

void undo_studio_edit(LabState& state) {
    if (state.preview_host != nullptr) {
        state.preview_host->stop_for_context_change();
    }
    if (state.draft_dirty) {
        const auto was_new = state.selected_look_draft.has_value() &&
            !state.selected_look_draft->source_index.has_value();
        state.draft_dirty = false;
        if (was_new) {
            state.selected_look_draft.reset();
            state.selected_look_id.clear();
            select_available_look(state);
        } else {
            static_cast<void>(reload_selected_look_draft(state));
        }
        state.operation_message = "Discarded the uncommitted Static Look draft.";
        return;
    }

    const auto outcome = state.document.undo(state.document.generation());
    state.operation_message = outcome.message;
    if (outcome) {
        select_available_look(state);
    }
}

void redo_studio_edit(LabState& state) {
    if (state.preview_host != nullptr) {
        state.preview_host->stop_for_context_change();
    }
    if (state.draft_dirty) {
        state.operation_message =
            "Save or undo the Static Look draft before using Redo.";
        return;
    }
    const auto outcome = state.document.redo(state.document.generation());
    state.operation_message = outcome.message;
    if (outcome) {
        select_available_look(state);
    }
}

void save_studio_project(LabState& state) {
    if (!state.project_path.has_value()) {
        state.operation_message =
            "This sample has no project path. Launch with --project <file> to test atomic save/history.";
        return;
    }
    if (state.draft_dirty) {
        const auto committed = commit_selected_look(state);
        if (!committed) {
            state.operation_message = committed.message;
            return;
        }
    }

    const auto snapshot = state.document.snapshot();
    const auto saved = emberlights::save_project_atomic(
        *state.project_path, snapshot.document, true);
    if (!saved) {
        state.operation_message = "Project save failed: " + saved.message;
        return;
    }
    const auto acknowledged = state.document.acknowledge_saved(
        snapshot.generation);
    state.operation_message = acknowledged
        ? "Project saved atomically to " + state.project_path->string() + "."
        : acknowledged.message;
}

void save_studio_project_as(LabState& state) {
    const auto selected = choose_project_path(true, state.project_path);
    if (!selected.has_value()) {
        state.operation_message = "Save Project As was canceled.";
        return;
    }
    const auto previous_path = state.project_path;
    state.project_path = *selected;
    save_studio_project(state);
    if (state.document.snapshot().dirty &&
        state.operation_message.rfind("Project save failed:", 0U) == 0U) {
        state.project_path = previous_path;
    }
}

void invoke_preview(
    const FixturesLooksLab& ui,
    LabState& state,
    emberlights::UiStaticLookPreviewMode mode) {
    if (state.preview_host == nullptr) {
        state.operation_message = "The preview command host is unavailable.";
        refresh_ui(ui, state);
        return;
    }
    emberlights::UiCommandInvocation invocation;
    invocation.command = emberlights::UiCommandId::StaticLookPreviewStart;
    invocation.target_id = state.selected_look_id;
    invocation.secondary_target_id = state.selected_target_id;
    invocation.static_look_preview_mode = mode;
    const auto result = state.preview_host->facade().invoke(invocation);
    if (result == emberlights::UiInvocationResult::Accepted) {
        state.operation_message =
            mode == emberlights::UiStaticLookPreviewMode::Physical
            ? "Bounded fixture preview queued. Live stays stopped; Stop always requests terminal blackout."
            : "Offline simulation queued on the preview worker; no output adapter is opened.";
    } else if (result == emberlights::UiInvocationResult::NoChange) {
        state.operation_message = "That preview is already active or starting.";
    } else if (result == emberlights::UiInvocationResult::Unavailable &&
               mode == emberlights::UiStaticLookPreviewMode::Physical) {
        state.operation_message =
            "Fixture preview is locked, has no configured output, or Live is not stopped.";
    } else {
        state.operation_message = "Preview request rejected: " +
            std::string(emberlights::ui_invocation_result_name(result)) + ".";
    }
    refresh_ui(ui, state);
}

void stop_preview(const FixturesLooksLab& ui, LabState& state) {
    if (state.preview_host == nullptr) {
        return;
    }
    const auto result = state.preview_host->facade().invoke(
        {emberlights::UiCommandId::StaticLookPreviewStop});
    state.operation_message = result == emberlights::UiInvocationResult::Accepted
        ? "Blackout and preview stop queued."
        : result == emberlights::UiInvocationResult::NoChange
            ? "Preview is already stopped."
            : "Preview stop rejected: " +
                std::string(emberlights::ui_invocation_result_name(result)) + ".";
    refresh_ui(ui, state);
}

[[nodiscard]] int model_smoke() {
    LabState state;
    if (!activate_project(
            state,
            make_lab_project(),
            emberlights::StudioDocumentBoundary::NewDocument,
            std::nullopt)) {
        std::cerr << "Fixtures/Looks Slint lab document smoke failed\n";
        return EXIT_FAILURE;
    }
    const auto project = project_for_presentation(state);
    emberlights::FixturesLooksShellQuery query;
    query.selected_profile_id = "local.visual.mover";
    query.selected_target_id = "group.movers";
    query.selected_static_look_id = "look.ceremony";
    query.include_advanced = true;
    query.viewport_width = 1366;
    query.viewport_height = 768;
    const auto model = emberlights::build_fixtures_looks_shell_model(
        project, query);
    const auto has_intensity = control_for(
        model, showcore::Property::Intensity) != nullptr;
    const auto has_position =
        control_for(model, showcore::Property::Pan) != nullptr &&
        control_for(model, showcore::Property::Tilt) != nullptr;
    const auto has_profile_driven_zoom = std::any_of(
        model.controls.begin(), model.controls.end(), [](const auto& control) {
            return control.parameter_id == "zoom" && control.accepts_value;
        });
    const auto has_profile_choice = std::any_of(
        model.controls.begin(), model.controls.end(),
        [](const auto& control) {
            return control.control_kind == "visual choice tiles";
        });
    if (model.state != emberlights::FixturesLooksShellState::Ready ||
        !model.minimum_viewport_supported || !model.can_edit ||
        !has_intensity || !has_position || !has_profile_choice ||
        !has_profile_driven_zoom || model.control_categories.size() < 5U) {
        std::cerr << "Fixtures/Looks Slint lab model smoke failed\n";
        return EXIT_FAILURE;
    }
    const auto before = state.document.snapshot();
    const auto edited = emberlights::apply_static_look_property(
        *state.selected_look_draft,
        before.document,
        state.selected_target_id,
        showcore::Property::Intensity,
        showcore::PropertyValue::set(0.5F));
    state.draft_dirty = edited.result ==
        emberlights::StaticLookAuthoringResult::Applied;
    const auto committed = commit_selected_look(state);
    const auto after_commit = state.document.snapshot();
    const auto undone = state.document.undo(after_commit.generation);
    const auto after_undo = state.document.snapshot();
    const auto redone = state.document.redo(after_undo.generation);
    if (!edited || !committed || !after_commit.can_undo || !undone || !redone) {
        std::cerr << "Fixtures/Looks Slint lab history smoke failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "Fixtures/Looks Slint lab model smoke passed\n";
    return EXIT_SUCCESS;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--model-smoke") {
        return model_smoke();
    }

    std::optional<std::filesystem::path> requested_project_path;
    bool allow_physical_preview = false;
    bool startup_smoke = false;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument == "--project" && index + 1 < argc) {
            requested_project_path = std::filesystem::path(argv[++index]);
        } else if (argument == "--allow-physical-preview") {
            allow_physical_preview = true;
        } else if (argument == "--startup-smoke") {
            startup_smoke = true;
        } else if (argument == "--help") {
            std::cout
                << "Usage: "
                << (kProductShell
                        ? "EmberLights"
                        : "EmberLights-Fixtures-Looks-Lab")
                << " [--project <file>] [--allow-physical-preview]"
                   " [--startup-smoke]\n"
                << "A missing --project file starts the sample as a new document; "
                   "Save Project creates it atomically. Physical preview also "
                   "requires --project and a configured output adapter.\n";
            return EXIT_SUCCESS;
        } else if (!argument.empty() && argument.front() != '-' &&
                   !requested_project_path.has_value()) {
            requested_project_path = std::filesystem::path(argument);
        } else {
            std::cerr << "Unknown or incomplete argument: " << argument << '\n';
            return EXIT_FAILURE;
        }
    }
    if (allow_physical_preview && !requested_project_path.has_value()) {
        std::cerr
            << "--allow-physical-preview requires an explicit --project path.\n";
        return EXIT_FAILURE;
    }

    auto state = std::make_shared<LabState>();
    if (requested_project_path.has_value()) {
        std::error_code filesystem_error;
        const auto exists = std::filesystem::exists(
            *requested_project_path, filesystem_error);
        if (filesystem_error) {
            std::cerr << "Unable to inspect the requested project path: "
                      << filesystem_error.message() << '\n';
            return EXIT_FAILURE;
        }
        if (exists) {
            if (!open_project(*state, *requested_project_path)) {
                std::cerr << "Unable to open the requested project: "
                          << state->operation_message
                          << '\n';
                return EXIT_FAILURE;
            }
        } else if (!activate_project(
                       *state,
                       make_lab_project(),
                       emberlights::StudioDocumentBoundary::NewDocument,
                       requested_project_path)) {
            std::cerr << "Unable to create the new lab document: "
                      << state->operation_message << '\n';
            return EXIT_FAILURE;
        } else {
            state->operation_message =
                "New unsaved project. Save Project creates it atomically with restore history.";
        }
    } else if (!activate_project(
                   *state,
                   make_lab_project(),
                   emberlights::StudioDocumentBoundary::NewDocument,
                   std::nullopt)) {
        std::cerr << "Unable to initialize the lab document: "
                  << state->operation_message << '\n';
        return EXIT_FAILURE;
    } else {
        state->operation_message = kProductShell
            ? "Output-disabled demo project. Open a project or use Save Project As to create one."
            : "Output-disabled sample. Launch with --project <file> to test durable save/history.";
    }
    if (allow_physical_preview) {
        state->operation_message +=
            " Bounded fixture preview was explicitly armed for this lab process.";
    }
    configure_product_os2l(*state);
    auto preview_host = std::make_unique<LabPreviewCommandHost>(
        *state, allow_physical_preview);
    state->preview_host = preview_host.get();
    auto ui = FixturesLooksLab::create();
    const slint::ComponentWeakHandle<FixturesLooksLab> weak_ui(ui);

    const auto with_ui = [weak_ui, state](auto callback) {
        if (const auto locked = weak_ui.lock()) {
            callback(**locked, *state);
        }
    };
    ui->on_select_profile([with_ui](const slint::SharedString& id) {
        with_ui([&](const auto& component, auto& state) {
            state.selected_profile_id = string_from(id);
            state.operation_message.clear();
            refresh_ui(component, state);
        });
    });
    ui->on_select_target([with_ui](const slint::SharedString& id) {
        with_ui([&](const auto& component, auto& state) {
            const auto requested = string_from(id);
            if (requested != state.selected_target_id &&
                state.preview_host != nullptr) {
                state.preview_host->stop_for_context_change();
            }
            state.selected_target_id = requested;
            state.selected_choice_id.clear();
            state.operation_message.clear();
            refresh_ui(component, state);
        });
    });
    ui->on_select_look([with_ui](const slint::SharedString& id) {
        with_ui([&](const auto& component, auto& state) {
            const auto requested = string_from(id);
            if (state.draft_dirty && requested != state.selected_look_id) {
                state.operation_message =
                    "Save or undo the current Static Look draft before changing selection.";
                refresh_ui(component, state);
                return;
            }
            if (requested != state.selected_look_id &&
                state.preview_host != nullptr) {
                state.preview_host->stop_for_context_change();
            }
            state.selected_look_id = requested;
            state.selected_choice_id.clear();
            state.operation_message = reload_selected_look_draft(state)
                ? std::string{}
                : std::string("The selected Static Look is no longer available.");
            refresh_ui(component, state);
        });
    });
    ui->on_profile_search_changed([with_ui](const slint::SharedString& value) {
        with_ui([&](const auto& component, auto& state) {
            state.profile_search = string_from(value);
            refresh_ui(component, state);
        });
    });
    ui->on_look_search_changed([with_ui](const slint::SharedString& value) {
        with_ui([&](const auto& component, auto& state) {
            state.look_search = string_from(value);
            refresh_ui(component, state);
        });
    });
    ui->on_parameter_search_changed(
        [with_ui](const slint::SharedString& value) {
            with_ui([&](const auto& component, auto& state) {
                state.control_search = string_from(value);
                refresh_ui(component, state);
            });
        });
    ui->on_select_control_group([with_ui](const slint::SharedString& id) {
        with_ui([&](const auto& component, auto& state) {
            const auto stable_id = string_from(id);
            if (stable_id == "all") {
                state.control_category.reset();
                state.operation_message.clear();
            } else if (const auto category =
                           emberlights::fixture_parameter_category_from_stable_id(
                               stable_id)) {
                state.control_category = *category;
                state.operation_message.clear();
            } else {
                state.operation_message =
                    "That fixture-parameter category is no longer available.";
            }
            refresh_ui(component, state);
        });
    });
    ui->on_select_choice([with_ui](const slint::SharedString& id) {
        with_ui([&](const auto& component, auto& state) {
            state.selected_choice_id = string_from(id);
            mutate_selected_look(component, state, [&](auto& draft, const auto& project) {
                return emberlights::apply_static_look_control_choice(
                    draft, project, state.selected_target_id,
                    state.selected_choice_id);
            });
        });
    });
    ui->on_control_value_changed(
        [with_ui](const slint::SharedString& id, float value) {
            with_ui([&](const auto& component, auto& state) {
                state.selected_choice_id = string_from(id);
                const auto project = project_for_presentation(state);
                const auto model = emberlights::build_fixtures_looks_shell_model(
                    project,
                    query_from(state, component.get_advanced_open()));
                const auto* control = control_for(model, string_from(id));
                if (control == nullptr) {
                    state.operation_message = "The selected control is stale.";
                    refresh_ui(component, state);
                    return;
                }
                mutate_selected_look(component, state, [&](auto& draft, const auto& active_project) {
                    if (control->profile_function) {
                        return emberlights::apply_static_look_control_choice(
                            draft, active_project, state.selected_target_id,
                            control->choice_id, value);
                    }
                    return emberlights::apply_static_look_property(
                        draft, active_project, state.selected_target_id,
                        control->property, showcore::PropertyValue::set(value));
                });
            });
        });
    ui->on_control_ownership_changed(
        [with_ui](const slint::SharedString& id,
                  const slint::SharedString& ownership) {
            with_ui([&](const auto& component, auto& state) {
                state.selected_choice_id = string_from(id);
                const auto project = project_for_presentation(state);
                const auto model = emberlights::build_fixtures_looks_shell_model(
                    project,
                    query_from(state, component.get_advanced_open()));
                const auto* control = control_for(model, string_from(id));
                if (control == nullptr) {
                    state.operation_message = "The selected control is stale.";
                    refresh_ui(component, state);
                    return;
                }
                const auto mode = string_from(ownership);
                mutate_selected_look(component, state, [&](auto& draft, const auto& active_project) {
                    if (mode == "release") {
                        return emberlights::remove_static_look_property(
                            draft, active_project, state.selected_target_id,
                            control->property);
                    }
                    if (mode == "set" && control->profile_function) {
                        if (control->accepts_value) {
                            return emberlights::apply_static_look_control_choice(
                                draft, active_project, state.selected_target_id,
                                control->choice_id, control->normalized_value);
                        }
                        return emberlights::apply_static_look_control_choice(
                            draft, active_project, state.selected_target_id,
                            control->choice_id);
                    }
                    const auto value = mode == "forceZero"
                        ? showcore::PropertyValue::force_zero()
                        : showcore::PropertyValue::set(control->normalized_value);
                    return emberlights::apply_static_look_property(
                        draft, active_project, state.selected_target_id,
                        control->property, value);
                });
            });
        });
    ui->on_preview_simulate([with_ui]() {
        with_ui([](const auto& component, auto& state) {
            invoke_preview(
                component,
                state,
                emberlights::UiStaticLookPreviewMode::Simulation);
        });
    });
    ui->on_preview_physical([with_ui]() {
        with_ui([](const auto& component, auto& state) {
            invoke_preview(
                component,
                state,
                emberlights::UiStaticLookPreviewMode::Physical);
        });
    });
    ui->on_preview_stop([with_ui]() {
        with_ui([](const auto& component, auto& state) {
            stop_preview(component, state);
        });
    });
    ui->on_save_look([with_ui]() {
        with_ui([](const auto& component, auto& state) {
            const auto outcome = commit_selected_look(state);
            state.operation_message = outcome.result ==
                    emberlights::StudioMutationResult::Applied
                ? "Static Look committed as one generation-checked Undo transaction."
                : outcome.message;
            refresh_ui(component, state);
        });
    });
    ui->on_save_project([with_ui]() {
        with_ui([](const auto& component, auto& state) {
            save_studio_project(state);
            refresh_ui(component, state);
        });
    });
    ui->on_save_project_as([with_ui]() {
        with_ui([](const auto& component, auto& state) {
            save_studio_project_as(state);
            refresh_ui(component, state);
        });
    });
    ui->on_new_project([with_ui]() {
        with_ui([](const auto& component, auto& state) {
            const auto snapshot = state.document.snapshot();
            const auto has_unsaved_user_edits = state.draft_dirty ||
                (snapshot.dirty &&
                 (state.project_path.has_value() || snapshot.can_undo));
            if (has_unsaved_user_edits) {
                state.operation_message =
                    "Save or undo the current project edits before creating a new project.";
                refresh_ui(component, state);
                return;
            }
            if (state.preview_host != nullptr) {
                state.preview_host->stop_for_context_change();
            }
            auto project = make_lab_project();
            project.name = "EmberLights Demo Rig";
            if (activate_project(
                    state,
                    std::move(project),
                    emberlights::StudioDocumentBoundary::NewDocument,
                    std::nullopt)) {
                state.operation_message =
                    "New output-disabled demo project. Save Project As creates a durable file.";
                configure_product_os2l(state);
            }
            refresh_ui(component, state);
        });
    });
    ui->on_open_project([with_ui]() {
        with_ui([](const auto& component, auto& state) {
            const auto snapshot = state.document.snapshot();
            const auto has_unsaved_user_edits = state.draft_dirty ||
                (snapshot.dirty &&
                 (state.project_path.has_value() || snapshot.can_undo));
            if (has_unsaved_user_edits) {
                state.operation_message =
                    "Save or undo the current project edits before opening another project.";
                refresh_ui(component, state);
                return;
            }
            const auto selected = choose_project_path(false, state.project_path);
            if (!selected.has_value()) {
                state.operation_message = "Open Project was canceled.";
            } else {
                if (state.preview_host != nullptr) {
                    state.preview_host->stop_for_context_change();
                }
                static_cast<void>(open_project(state, *selected));
                configure_product_os2l(state);
            }
            refresh_ui(component, state);
        });
    });
    ui->on_open_safe([with_ui]() {
        with_ui([](const auto& component, auto& state) {
            const auto snapshot = state.document.snapshot();
            if (state.draft_dirty ||
                (state.project_path.has_value() && snapshot.dirty)) {
                state.operation_message =
                    "Save or undo this project's edits before opening it in Safe / Live.";
                refresh_ui(component, state);
                return;
            }
            if (state.preview_host != nullptr) {
                const auto preview = state.preview_host->status();
                if (preview_busy(preview.state)) {
                    state.preview_host->stop_for_context_change();
                    state.operation_message =
                        "Stopping preview and requesting blackout. Open Safe / Live again when preview is stopped.";
                    refresh_ui(component, state);
                    return;
                }
            }
            if (state.os2l_service) {
                state.os2l_service->stop();
            }
            const auto launched = launch_safe_shell(state);
            state.operation_message = launched
                ? "Handed the project to the Safe / Live compatibility workspace."
                : "Safe / Live could not be opened. Reinstall EmberLights and try again.";
            if (!launched) {
                configure_product_os2l(state);
            } else {
                slint::Timer::single_shot(
                    std::chrono::milliseconds(120),
                    [] { slint::quit_event_loop(); });
            }
            refresh_ui(component, state);
        });
    });
    ui->on_undo([with_ui]() {
        with_ui([](const auto& component, auto& state) {
            undo_studio_edit(state);
            refresh_ui(component, state);
        });
    });
    ui->on_redo([with_ui]() {
        with_ui([](const auto& component, auto& state) {
            redo_studio_edit(state);
            refresh_ui(component, state);
        });
    });
    ui->on_create_look([with_ui]() {
        with_ui([](const auto& component, auto& state) {
            if (state.draft_dirty) {
                state.operation_message =
                    "Save or undo the current Static Look draft before creating another.";
                refresh_ui(component, state);
                return;
            }
            if (state.preview_host != nullptr) {
                state.preview_host->stop_for_context_change();
            }
            const auto snapshot = state.document.snapshot();
            std::string stable_id;
            do {
                ++state.created_look_count;
                stable_id =
                    "look.lab." + std::to_string(state.created_look_count);
            } while (look_index_for(snapshot.document, stable_id).has_value());
            state.selected_look_draft = emberlights::make_static_look_draft(
                snapshot.generation,
                stable_id,
                "New Static Look " + std::to_string(state.created_look_count));
            state.selected_look_id = state.selected_look_draft->look.id;
            state.draft_dirty = true;
            state.operation_message =
                "New Static Look draft ready. Save Look commits it to project history.";
            refresh_ui(component, state);
        });
    });
    ui->on_duplicate_look([with_ui]() {
        with_ui([](const auto& component, auto& state) {
            if (state.draft_dirty) {
                state.operation_message =
                    "Save or undo the current Static Look draft before duplicating.";
                refresh_ui(component, state);
                return;
            }
            if (state.preview_host != nullptr) {
                state.preview_host->stop_for_context_change();
            }
            if (!state.selected_look_draft.has_value() &&
                !reload_selected_look_draft(state)) {
                state.operation_message = "Select a Static Look to duplicate.";
                refresh_ui(component, state);
                return;
            }
            const auto snapshot = state.document.snapshot();
            std::string stable_id;
            do {
                ++state.created_look_count;
                stable_id = "look.lab.copy." +
                    std::to_string(state.created_look_count);
            } while (look_index_for(snapshot.document, stable_id).has_value());
            state.selected_look_draft =
                emberlights::duplicate_static_look_draft(
                    *state.selected_look_draft,
                    stable_id,
                    state.selected_look_draft->look.name + " Copy");
            state.selected_look_id = state.selected_look_draft->look.id;
            state.draft_dirty = true;
            state.operation_message =
                "Duplicated Static Look draft ready. Save Look commits it to history.";
            refresh_ui(component, state);
        });
    });
    ui->on_open_advanced([with_ui]() {
        with_ui([](const auto& component, auto& state) {
            if (!component.get_advanced_open() &&
                state.control_category ==
                    emberlights::FixtureParameterCategory::Custom) {
                state.control_category.reset();
            }
            refresh_ui(component, state);
        });
    });

    slint::Timer preview_status_timer;
    preview_status_timer.start(
        slint::TimerMode::Repeated,
        std::chrono::milliseconds(100),
        [weak_ui, state] {
            if (state->preview_host == nullptr) {
                return;
            }
            try {
                const auto status = state->preview_host->status();
                if (preview_status_token(status) ==
                    state->preview_status_token) {
                    return;
                }
            } catch (...) {
                if (state->preview_status_token.find("status-unavailable") !=
                    std::string::npos) {
                    return;
                }
            }
            if (const auto locked = weak_ui.lock()) {
                refresh_ui(**locked, *state);
            }
        });
    slint::Timer product_status_timer;
    if (kProductShell) {
        product_status_timer.start(
            slint::TimerMode::Repeated,
            std::chrono::milliseconds(500),
            [weak_ui, state] {
                const auto status = state->os2l_service
                    ? state->os2l_service->status()
                    : emberlights::Os2lServiceStatus{};
                if (os2l_status_token(status) == state->os2l_status_token) {
                    return;
                }
                if (const auto locked = weak_ui.lock()) {
                    refresh_ui(**locked, *state);
                }
            });
    }
    refresh_ui(*ui, *state);
    if (startup_smoke) {
        slint::Timer::single_shot(
            std::chrono::milliseconds(600),
            [] { slint::quit_event_loop(); });
    }
    ui->run();
    return EXIT_SUCCESS;
}
