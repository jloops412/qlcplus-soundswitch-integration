#include "emberlights/fixtures_looks_shell.hpp"
#include "emberlights/static_look_authoring.hpp"

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
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

struct LabState {
    emberlights::ProjectDocument project;
    std::string selected_profile_id{"local.visual.mover"};
    std::string selected_target_id{"group.movers"};
    std::string selected_look_id{"look.ceremony"};
    std::string selected_choice_id;
    std::string profile_search;
    std::string look_search;
    std::string operation_message;
    bool dirty{false};
    bool preview_active{false};
    unsigned int created_look_count{0U};
};

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
    profile.mode = "11 channel";
    profile.name = "Ember Test Visual Mover (11 channel)";
    profile.source = showcore::FixtureProfileSource::Local;
    profile.source_revision = "shell-v1";
    profile.footprint = 11U;
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
    item.title = shared_string(control.property_label);
    item.detail = shared_string(control.availability_text);
    item.active = control.value_matches_choice;
    item.selected = control.selected;
    item.enabled = control.enabled;
    item.safety_restricted = control.safety_restricted;
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
    query.include_advanced = true;
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

[[nodiscard]] emberlights::LookDefinition* selected_look(
    LabState& state) noexcept {
    const auto found = std::find_if(
        state.project.looks.begin(), state.project.looks.end(),
        [&](const auto& look) { return look.id == state.selected_look_id; });
    return found == state.project.looks.end() ? nullptr : &*found;
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

void set_control(
    const FixturesLooksLab& ui,
    const emberlights::FixturesLooksControlBinding* control,
    void (FixturesLooksLab::*set_id)(const slint::SharedString&) const,
    void (FixturesLooksLab::*set_value)(const float&) const,
    void (FixturesLooksLab::*set_ownership)(const slint::SharedString&) const,
    float fallback = 0.0F) {
    (ui.*set_id)(shared_string(
        control == nullptr ? std::string_view{} : control->choice_id));
    const auto value = control_value(control, fallback);
    (ui.*set_value)(value);
    (ui.*set_ownership)(shared_string(ownership_name(control)));
}

void refresh_ui(const FixturesLooksLab& ui, LabState& state) {
    const auto model = emberlights::build_fixtures_looks_shell_model(
        state.project, query_from(state, ui.get_advanced_open()));

    ui.set_project_name(shared_string(model.project_name));
    ui.set_save_state(state.dirty ? "Modified • memory only" : "Lab • memory only");
    ui.set_validation_state(shared_string(model.validation_status));
    ui.set_dj_state("Not connected");
    ui.set_output_state("No output");
    ui.set_workspace_message(shared_string(state.operation_message.empty()
        ? model.message
        : state.operation_message));
    ui.set_preview_status(shared_string(state.preview_active
        ? std::string_view("Preview simulation active • no DMX output")
        : std::string_view(model.preview_status)));
    ui.set_preview_active(state.preview_active);
    ui.set_can_edit(model.can_edit);
    ui.set_can_preview(model.can_preview && !state.preview_active);
    ui.set_live_running(model.live_running);
    ui.set_profile_search(shared_string(state.profile_search));
    ui.set_look_search(shared_string(state.look_search));

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

    set_control(
        ui, control_for(model, showcore::Property::Intensity),
        &FixturesLooksLab::set_intensity_choice_id,
        &FixturesLooksLab::set_intensity_value,
        &FixturesLooksLab::set_intensity_ownership);
    set_control(
        ui, control_for(model, showcore::Property::Red),
        &FixturesLooksLab::set_red_choice_id,
        &FixturesLooksLab::set_red_value,
        &FixturesLooksLab::set_red_ownership);
    set_control(
        ui, control_for(model, showcore::Property::Green),
        &FixturesLooksLab::set_green_choice_id,
        &FixturesLooksLab::set_green_value,
        &FixturesLooksLab::set_green_ownership);
    set_control(
        ui, control_for(model, showcore::Property::Blue),
        &FixturesLooksLab::set_blue_choice_id,
        &FixturesLooksLab::set_blue_value,
        &FixturesLooksLab::set_blue_ownership);
    set_control(
        ui, control_for(model, showcore::Property::White),
        &FixturesLooksLab::set_white_choice_id,
        &FixturesLooksLab::set_white_value,
        &FixturesLooksLab::set_white_ownership);
    set_control(
        ui, control_for(model, showcore::Property::Amber),
        &FixturesLooksLab::set_amber_choice_id,
        &FixturesLooksLab::set_amber_value,
        &FixturesLooksLab::set_amber_ownership);
    set_control(
        ui, control_for(model, showcore::Property::Focus),
        &FixturesLooksLab::set_focus_choice_id,
        &FixturesLooksLab::set_focus_value,
        &FixturesLooksLab::set_focus_ownership,
        0.5F);

    const auto* pan = control_for(model, showcore::Property::Pan);
    const auto* tilt = control_for(model, showcore::Property::Tilt);
    ui.set_pan_choice_id(shared_string(
        pan == nullptr ? std::string_view{} : pan->choice_id));
    ui.set_pan_value(control_value(pan, 0.5F));
    ui.set_tilt_choice_id(shared_string(
        tilt == nullptr ? std::string_view{} : tilt->choice_id));
    ui.set_tilt_value(control_value(tilt, 0.5F));
    ui.set_position_ownership(shared_string(
        pan != nullptr && tilt != nullptr && pan->ownership == tilt->ownership
            ? ownership_name(pan)
            : std::string("mixed")));

    std::vector<ChoiceTileItem> choice_items;
    for (const auto& control : model.controls) {
        if (control.control_kind != "visual choice tiles") {
            continue;
        }
        choice_items.push_back(choice_tile_item(control));
    }
    ui.set_choice_items(
        std::make_shared<slint::VectorModel<ChoiceTileItem>>(
            std::move(choice_items)));
}

template<typename Mutation>
void mutate_selected_look(
    const FixturesLooksLab& ui,
    LabState& state,
    Mutation mutation) {
    auto* look = selected_look(state);
    if (look == nullptr) {
        state.operation_message = "Select a Static Look before editing.";
        refresh_ui(ui, state);
        return;
    }
    emberlights::StaticLookDraft draft;
    draft.look = *look;
    const auto outcome = mutation(draft);
    if (!outcome) {
        state.operation_message = "That control is unavailable for the selected target.";
        refresh_ui(ui, state);
        return;
    }
    if (outcome.result == emberlights::StaticLookAuthoringResult::Applied) {
        *look = std::move(draft.look);
        state.dirty = true;
        state.operation_message = outcome.warnings.empty()
            ? "Static Look updated in the in-memory Slint lab."
            : outcome.warnings.front();
    } else {
        state.operation_message = outcome.warnings.empty()
            ? "The Static Look already has that value."
            : outcome.warnings.front();
    }
    refresh_ui(ui, state);
}

[[nodiscard]] int model_smoke() {
    const auto project = make_lab_project();
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
    const auto has_profile_choice = std::any_of(
        model.controls.begin(), model.controls.end(),
        [](const auto& control) {
            return control.control_kind == "visual choice tiles";
        });
    if (model.state != emberlights::FixturesLooksShellState::Ready ||
        !model.minimum_viewport_supported || !model.can_edit ||
        !has_intensity || !has_position || !has_profile_choice) {
        std::cerr << "Fixtures/Looks Slint lab model smoke failed\n";
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

    auto state = std::make_shared<LabState>();
    state->project = make_lab_project();
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
            state.selected_target_id = string_from(id);
            state.selected_choice_id.clear();
            state.operation_message.clear();
            refresh_ui(component, state);
        });
    });
    ui->on_select_look([with_ui](const slint::SharedString& id) {
        with_ui([&](const auto& component, auto& state) {
            state.selected_look_id = string_from(id);
            state.selected_choice_id.clear();
            state.operation_message.clear();
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
    ui->on_select_choice([with_ui](const slint::SharedString& id) {
        with_ui([&](const auto& component, auto& state) {
            state.selected_choice_id = string_from(id);
            mutate_selected_look(component, state, [&](auto& draft) {
                return emberlights::apply_static_look_control_choice(
                    draft, state.project, state.selected_target_id,
                    state.selected_choice_id);
            });
        });
    });
    ui->on_control_value_changed(
        [with_ui](const slint::SharedString& id, float value) {
            with_ui([&](const auto& component, auto& state) {
                const auto model = emberlights::build_fixtures_looks_shell_model(
                    state.project,
                    query_from(state, component.get_advanced_open()));
                const auto* control = control_for(model, string_from(id));
                if (control == nullptr) {
                    state.operation_message = "The selected control is stale.";
                    refresh_ui(component, state);
                    return;
                }
                mutate_selected_look(component, state, [&](auto& draft) {
                    return emberlights::apply_static_look_property(
                        draft, state.project, state.selected_target_id,
                        control->property, showcore::PropertyValue::set(value));
                });
            });
        });
    ui->on_control_ownership_changed(
        [with_ui](const slint::SharedString& id,
                  const slint::SharedString& ownership) {
            with_ui([&](const auto& component, auto& state) {
                const auto model = emberlights::build_fixtures_looks_shell_model(
                    state.project,
                    query_from(state, component.get_advanced_open()));
                const auto* control = control_for(model, string_from(id));
                if (control == nullptr) {
                    state.operation_message = "The selected control is stale.";
                    refresh_ui(component, state);
                    return;
                }
                const auto mode = string_from(ownership);
                mutate_selected_look(component, state, [&](auto& draft) {
                    if (mode == "release") {
                        return emberlights::remove_static_look_property(
                            draft, state.project, state.selected_target_id,
                            control->property);
                    }
                    const auto value = mode == "forceZero"
                        ? showcore::PropertyValue::force_zero()
                        : showcore::PropertyValue::set(control->normalized_value);
                    return emberlights::apply_static_look_property(
                        draft, state.project, state.selected_target_id,
                        control->property, value);
                });
            });
        });
    ui->on_preview_start([with_ui]() {
        with_ui([](const auto& component, auto& state) {
            state.preview_active = true;
            state.operation_message =
                "Preview is simulated in this lab; no DMX output is opened.";
            refresh_ui(component, state);
        });
    });
    ui->on_preview_stop([with_ui]() {
        with_ui([](const auto& component, auto& state) {
            state.preview_active = false;
            state.operation_message = "Preview simulation stopped.";
            refresh_ui(component, state);
        });
    });
    ui->on_save_look([with_ui]() {
        with_ui([](const auto& component, auto& state) {
            state.dirty = false;
            state.operation_message =
                "Lab state acknowledged in memory; project persistence is not wired yet.";
            refresh_ui(component, state);
        });
    });
    ui->on_create_look([with_ui]() {
        with_ui([](const auto& component, auto& state) {
            ++state.created_look_count;
            auto draft = emberlights::make_static_look_draft(
                0U,
                "look.lab." + std::to_string(state.created_look_count),
                "New Static Look " + std::to_string(state.created_look_count));
            state.selected_look_id = draft.look.id;
            state.project.looks.push_back(std::move(draft.look));
            state.dirty = true;
            state.operation_message = "Created an in-memory Static Look.";
            refresh_ui(component, state);
        });
    });
    ui->on_duplicate_look([with_ui]() {
        with_ui([](const auto& component, auto& state) {
            const auto* source = selected_look(state);
            if (source == nullptr) {
                state.operation_message = "Select a Static Look to duplicate.";
                refresh_ui(component, state);
                return;
            }
            ++state.created_look_count;
            emberlights::StaticLookDraft source_draft;
            source_draft.look = *source;
            auto duplicate = emberlights::duplicate_static_look_draft(
                source_draft,
                "look.lab.copy." + std::to_string(state.created_look_count),
                source->name + " Copy");
            state.selected_look_id = duplicate.look.id;
            state.project.looks.push_back(std::move(duplicate.look));
            state.dirty = true;
            state.operation_message = "Duplicated the Static Look in memory.";
            refresh_ui(component, state);
        });
    });
    ui->on_open_advanced([with_ui]() {
        with_ui([](const auto& component, auto& state) {
            refresh_ui(component, state);
        });
    });

    refresh_ui(*ui, *state);
    ui->run();
    return EXIT_SUCCESS;
}
