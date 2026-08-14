#include "emberlights/fixtures_looks_shell.hpp"

#include "emberlights/fixture_capabilities.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string_view>

namespace emberlights {
namespace {

struct OwnershipSummary {
    StaticLookOwnershipState ownership{StaticLookOwnershipState::Release};
    float normalized_value{0.0F};
    std::size_t assigned_fixture_count{0U};
    std::size_t target_fixture_count{0U};
    bool value_mixed{false};
};

struct ChoicePositionSummary {
    bool active{false};
    bool mixed{false};
    float position{0.5F};
};

[[nodiscard]] std::string_view profile_source_label(
    showcore::FixtureProfileSource source) noexcept {
    switch (source) {
    case showcore::FixtureProfileSource::BuiltIn: return "Built in";
    case showcore::FixtureProfileSource::OpenFixtureLibrary:
        return "Open Fixture Library";
    case showcore::FixtureProfileSource::QlcPlus: return "QLC+ import";
    case showcore::FixtureProfileSource::Local: return "Local editable";
    case showcore::FixtureProfileSource::Migrated: return "Migrated";
    case showcore::FixtureProfileSource::Unknown: return "Unknown source";
    }
    return "Unknown source";
}

[[nodiscard]] const FixtureProfileDefinition* selected_profile(
    const ProjectDocument& project,
    std::string_view requested,
    const FixtureDefinition* selected_fixture) noexcept {
    if (!requested.empty()) {
        return find_fixture_profile(project, requested);
    }
    if (selected_fixture != nullptr) {
        if (const auto* profile = find_fixture_profile(
                project, selected_fixture->profile_id)) {
            return profile;
        }
    }
    return project.fixture_profiles.empty() ? nullptr
                                            : &project.fixture_profiles.front();
}

[[nodiscard]] const FixtureDefinition* find_fixture(
    const ProjectDocument& project,
    std::string_view id) noexcept {
    const auto found = std::find_if(
        project.fixtures.begin(), project.fixtures.end(),
        [id](const auto& fixture) { return fixture.id == id; });
    return found == project.fixtures.end() ? nullptr : &*found;
}

[[nodiscard]] std::string target_detail(
    const ProjectDocument& project,
    const FixtureDefinition& fixture) {
    std::ostringstream detail;
    detail << "Fixture  •  U" << static_cast<unsigned int>(fixture.universe)
           << ':' << std::setw(3) << std::setfill('0') << fixture.address;
    if (const auto* profile = find_fixture_profile(project, fixture.profile_id)) {
        detail << "  •  " << profile->manufacturer << ' ' << profile->model
               << " / " << profile->mode;
    } else {
        detail << "  •  Profile missing";
    }
    return detail.str();
}

[[nodiscard]] std::vector<UiAuthoringItem> profile_authoring_items(
    const ProjectDocument& project) {
    std::vector<UiAuthoringItem> items;
    items.reserve(project.fixture_profiles.size());
    for (const auto& profile : project.fixture_profiles) {
        items.push_back({
            profile.id,
            profile.name,
            profile.manufacturer + "  •  " + profile.model + "  •  " +
                profile.mode,
            {profile.manufacturer, profile.model, profile.mode,
             std::string(profile_source_label(profile.source)),
             profile.source_revision},
            profile.source != showcore::FixtureProfileSource::Local});
    }
    return items;
}

[[nodiscard]] std::vector<UiAuthoringItem> look_authoring_items(
    const ProjectDocument& project) {
    std::vector<UiAuthoringItem> items;
    items.reserve(project.looks.size());
    for (const auto& look : project.looks) {
        items.push_back({
            look.id,
            look.name,
            std::to_string(look.assignments.size()) + " assignments  •  " +
                std::to_string(look.fade_ms) + " ms fade",
            {std::to_string(look.fade_ms),
             std::to_string(look.assignments.size())},
            false});
    }
    return items;
}

[[nodiscard]] const LookDefinition* resolve_look(
    const ProjectDocument& project,
    std::string_view requested) noexcept {
    if (requested.empty()) {
        return project.looks.empty() ? nullptr : &project.looks.front();
    }
    const auto found = std::find_if(
        project.looks.begin(), project.looks.end(),
        [requested](const auto& look) { return look.id == requested; });
    return found == project.looks.end() ? nullptr : &*found;
}

[[nodiscard]] std::string resolved_target_id(
    const ProjectDocument& project,
    std::string_view requested) {
    if (!requested.empty()) {
        return std::string(requested);
    }
    if (!project.groups.empty()) {
        return project.groups.front().id;
    }
    return project.fixtures.empty() ? std::string{} : project.fixtures.front().id;
}

[[nodiscard]] std::optional<showcore::PropertyValue> assignment_value(
    const LookDefinition* look,
    std::string_view fixture_id,
    showcore::Property property) noexcept {
    if (look == nullptr) {
        return std::nullopt;
    }
    const auto found = std::find_if(
        look->assignments.begin(), look->assignments.end(),
        [fixture_id, property](const auto& assignment) {
            return assignment.fixture_id == fixture_id &&
                assignment.property == property;
        });
    return found == look->assignments.end()
        ? std::nullopt
        : std::optional<showcore::PropertyValue>{found->value};
}

[[nodiscard]] bool same_normalized(float first, float second) noexcept {
    return std::fabs(first - second) <= 0.0001F;
}

[[nodiscard]] StaticLookOwnershipState ownership_state(
    showcore::ValueMode mode) noexcept {
    switch (mode) {
    case showcore::ValueMode::Release:
        return StaticLookOwnershipState::Release;
    case showcore::ValueMode::Set:
        return StaticLookOwnershipState::Set;
    case showcore::ValueMode::ForceZero:
        return StaticLookOwnershipState::ForceZero;
    }
    return StaticLookOwnershipState::Mixed;
}

[[nodiscard]] OwnershipSummary summarize_ownership(
    const LookDefinition* look,
    const FixtureTargetCapabilities& target,
    showcore::Property property,
    float fallback_value) {
    OwnershipSummary result;
    result.normalized_value = fallback_value;
    bool first = true;
    StaticLookOwnershipState first_ownership{
        StaticLookOwnershipState::Release};
    float first_value = fallback_value;
    for (const auto& fixture : target.fixtures) {
        if (property >= showcore::Property::Count ||
            !fixture.properties[static_cast<std::size_t>(property)]) {
            continue;
        }
        ++result.target_fixture_count;
        const auto assignment = assignment_value(look, fixture.fixture_id, property);
        const auto current_ownership = assignment.has_value()
            ? ownership_state(assignment->mode)
            : StaticLookOwnershipState::Release;
        const auto current_value = assignment.has_value() &&
                assignment->mode == showcore::ValueMode::Set
            ? assignment->value
            : fallback_value;
        if (assignment.has_value()) {
            ++result.assigned_fixture_count;
        }
        if (first) {
            first = false;
            first_ownership = current_ownership;
            first_value = current_value;
            continue;
        }
        if (current_ownership != first_ownership) {
            result.ownership = StaticLookOwnershipState::Mixed;
            result.value_mixed = true;
        } else if (current_ownership == StaticLookOwnershipState::Set &&
                   !same_normalized(current_value, first_value)) {
            result.ownership = StaticLookOwnershipState::Mixed;
            result.value_mixed = true;
        }
    }
    if (first) {
        return result;
    }
    if (!result.value_mixed) {
        result.ownership = first_ownership;
        result.normalized_value = first_ownership == StaticLookOwnershipState::Set
            ? first_value
            : fallback_value;
    }
    return result;
}

[[nodiscard]] const FixtureFunctionRow* find_catalog_row(
    const FixtureFunctionComponentModel& catalog,
    std::string_view choice_id) noexcept {
    const auto found = std::find_if(
        catalog.rows.begin(), catalog.rows.end(),
        [choice_id](const auto& row) { return row.choice_id == choice_id; });
    return found == catalog.rows.end() ? nullptr : &*found;
}

[[nodiscard]] bool authored_value_matches_choice(
    const LookDefinition* look,
    const FixtureTargetCapabilities& target,
    const FixtureFunctionRow* row) noexcept {
    if (look == nullptr || row == nullptr || row->property >= showcore::Property::Count) {
        return false;
    }
    std::size_t compared = 0U;
    for (const auto& fixture : target.fixtures) {
        if (!fixture.properties[static_cast<std::size_t>(row->property)]) {
            continue;
        }
        const auto assignment = assignment_value(
            look, fixture.fixture_id, row->property);
        if (!assignment.has_value() || assignment->mode != showcore::ValueMode::Set) {
            return false;
        }
        float expected = row->normalized_value;
        const auto diagnostic = std::find_if(
            row->diagnostics.begin(), row->diagnostics.end(),
            [&](const auto& value) { return value.fixture_id == fixture.fixture_id; });
        if (diagnostic != row->diagnostics.end()) {
            expected = diagnostic->normalized_value;
        }
        if (!same_normalized(assignment->value, expected)) {
            return false;
        }
        ++compared;
    }
    return compared != 0U;
}

[[nodiscard]] ChoicePositionSummary summarize_choice_position(
    const LookDefinition* look,
    const FixtureTargetCapabilities& target,
    const FixtureFunctionRow* row) noexcept {
    ChoicePositionSummary result;
    if (look == nullptr || row == nullptr ||
        row->kind != FixtureControlChoiceKind::NamedCapability ||
        !row->accepts_position) {
        return result;
    }

    bool first = true;
    float first_position = 0.5F;
    for (const auto& fixture : target.fixtures) {
        if (row->property >= showcore::Property::Count ||
            !fixture.properties[static_cast<std::size_t>(row->property)]) {
            continue;
        }
        const auto diagnostic = std::find_if(
            row->diagnostics.begin(), row->diagnostics.end(),
            [&](const auto& value) {
                return value.fixture_id == fixture.fixture_id;
            });
        if (diagnostic == row->diagnostics.end() ||
            diagnostic->semantic_max <= diagnostic->semantic_min) {
            return result;
        }
        const auto assignment = assignment_value(
            look, fixture.fixture_id, row->property);
        if (!assignment.has_value() ||
            assignment->mode != showcore::ValueMode::Set ||
            assignment->value < diagnostic->semantic_min - 0.0001F ||
            assignment->value > diagnostic->semantic_max + 0.0001F) {
            return result;
        }
        const auto position = std::clamp(
            (assignment->value - diagnostic->semantic_min) /
                (diagnostic->semantic_max - diagnostic->semantic_min),
            0.0F, 1.0F);
        if (first) {
            first = false;
            first_position = position;
        } else if (!same_normalized(position, first_position)) {
            result.mixed = true;
        }
    }
    if (first) {
        return result;
    }
    result.active = true;
    result.position = first_position;
    return result;
}

[[nodiscard]] std::string ownership_text(const OwnershipSummary& summary) {
    if (summary.ownership == StaticLookOwnershipState::Mixed) {
        return "Mixed ownership";
    }
    if (summary.ownership == StaticLookOwnershipState::Release) {
        return "Release • lower layer continues";
    }
    if (summary.ownership == StaticLookOwnershipState::ForceZero) {
        return "Force zero • explicitly off";
    }
    std::ostringstream text;
    text << "Set • " << std::lround(summary.normalized_value * 100.0F) << '%';
    return text.str();
}

void append_control_bindings(
    FixturesLooksShellModel& model,
    const LookDefinition* look,
    const FixtureTargetCapabilities& target,
    const FixtureFunctionComponentModel& catalog,
    std::string_view selected_choice_id) {
    for (const auto& section : model.control_surface.sections) {
        for (const auto& widget : section.widgets) {
            std::size_t direct_binding_count = 0U;
            std::size_t profile_function_count = 0U;
            std::optional<showcore::Property> common_property;
            bool composite = false;
            for (const auto& binding : widget.bindings) {
                const auto* row = find_catalog_row(catalog, binding.choice_id);
                if (row != nullptr &&
                    row->kind == FixtureControlChoiceKind::NamedCapability) {
                    ++profile_function_count;
                } else {
                    ++direct_binding_count;
                }
                if (!common_property.has_value()) {
                    common_property = binding.property;
                } else if (*common_property != binding.property) {
                    composite = true;
                }
            }
            const auto group_property = composite || !common_property.has_value()
                ? showcore::Property::Count
                : *common_property;
            model.control_groups.push_back({
                widget.stable_id,
                widget.parameter_id,
                section.label,
                widget.label,
                std::string(fixture_control_widget_kind_name(widget.kind)),
                section.category,
                group_property,
                widget.bindings.size(),
                direct_binding_count,
                profile_function_count,
                widget.value_binding_count,
                widget.choice_binding_count,
                composite,
                widget.enabled && !model.read_only,
                widget.degraded,
                widget.safety_restricted,
                widget.accessibility_label});
            for (const auto& binding : widget.bindings) {
                const auto summary = summarize_ownership(
                    look, target, binding.property, binding.normalized_value);
                const auto* row = find_catalog_row(catalog, binding.choice_id);
                const auto choice_position = summarize_choice_position(
                    look, target, row);
                const auto position_control = row != nullptr &&
                    row->kind == FixtureControlChoiceKind::NamedCapability &&
                    row->accepts_position;
                std::string accessibility = binding.accessibility_label;
                if (!accessibility.empty()) {
                    accessibility += " ";
                }
                accessibility += ownership_text(summary);
                const auto* descriptor = fixture_parameter_descriptor(
                    binding.property);
                model.controls.push_back({
                    widget.stable_id,
                    binding.choice_id,
                    section.label,
                    widget.label,
                    std::string(fixture_control_widget_kind_name(widget.kind)),
                    section.category,
                    descriptor == nullptr
                        ? std::string(property_name(binding.property))
                        : std::string(descriptor->stable_id),
                    binding.property,
                    binding.label,
                    summary.ownership,
                    position_control
                        ? (choice_position.active
                              ? choice_position.position
                              : 0.5F)
                        : summary.normalized_value,
                    summary.assigned_fixture_count,
                    summary.target_fixture_count,
                    summary.value_mixed || choice_position.mixed,
                    position_control
                        ? choice_position.active
                        : authored_value_matches_choice(look, target, row),
                    !selected_choice_id.empty() &&
                        binding.choice_id == selected_choice_id,
                    binding.accepts_value,
                    row != nullptr &&
                        row->kind == FixtureControlChoiceKind::NamedCapability,
                    binding.enabled && !model.read_only,
                    binding.safety_restricted,
                    binding.availability_text,
                    ownership_text(summary),
                    std::move(accessibility)});
            }
        }
    }
}

void append_control_categories(
    FixturesLooksShellModel& model,
    const FixtureFunctionComponentModel& catalog,
    bool include_advanced,
    std::optional<FixtureParameterCategory> selected_category) {
    std::size_t total_count = 0U;
    std::size_t search_match_count = 0U;
    std::size_t visible_count = 0U;
    for (const auto& category : catalog.categories) {
        if (category.category == FixtureParameterCategory::Custom &&
            !include_advanced) {
            continue;
        }
        total_count += category.total_count;
        search_match_count += category.search_match_count;
        visible_count += category.visible_count;
    }

    model.control_total_count = total_count;
    model.control_search_match_count = search_match_count;
    model.control_categories.push_back({
        "all",
        "All",
        total_count,
        search_match_count,
        selected_category.has_value() ? 0U : visible_count,
        !selected_category.has_value(),
        false,
        "All fixture parameters, " + std::to_string(search_match_count) +
            " matching of " + std::to_string(total_count) + "."});

    for (const auto& category : catalog.categories) {
        if (category.category == FixtureParameterCategory::Custom &&
            !include_advanced) {
            continue;
        }
        const auto stable_id = fixture_parameter_category_stable_id(
            category.category);
        const auto selected = selected_category.has_value() &&
            *selected_category == category.category;
        model.control_categories.push_back({
            std::string(stable_id),
            category.label,
            category.total_count,
            category.search_match_count,
            category.visible_count,
            selected,
            category.category == FixtureParameterCategory::Custom,
            category.label + " fixture parameters, " +
                std::to_string(category.search_match_count) + " matching of " +
                std::to_string(category.total_count) + "."});
    }
}

void append_control_diagnostics(
    FixturesLooksShellModel& model,
    const FixtureFunctionComponentModel& catalog,
    std::string_view selected_choice_id) {
    if (selected_choice_id.empty()) {
        return;
    }
    const auto row = std::find_if(
        catalog.rows.begin(), catalog.rows.end(),
        [selected_choice_id](const auto& candidate) {
            return candidate.choice_id == selected_choice_id;
        });
    if (row == catalog.rows.end()) {
        return;
    }
    model.control_diagnostics.reserve(row->diagnostics.size());
    for (const auto& diagnostic : row->diagnostics) {
        const auto fixture_label = diagnostic.fixture_name.empty()
            ? diagnostic.fixture_id
            : diagnostic.fixture_name;
        const auto profile_label = diagnostic.profile_name.empty()
            ? diagnostic.profile_id
            : diagnostic.profile_name;
        auto detail = "Ch " + std::to_string(diagnostic.channel) + " • " +
            std::string(channel_encoding_name(diagnostic.encoding)) +
            " • DMX " +
            std::to_string(static_cast<unsigned int>(diagnostic.raw_value));
        if (diagnostic.fine_channel != 0U) {
            detail += "/" + std::to_string(
                static_cast<unsigned int>(diagnostic.raw_fine_value)) +
                " fine Ch " + std::to_string(diagnostic.fine_channel);
        }
        detail += " • range " +
            std::to_string(static_cast<unsigned int>(diagnostic.dmx_min)) +
            "–" +
            std::to_string(static_cast<unsigned int>(diagnostic.dmx_max));
        const auto provenance = profile_label + " • revision " +
            (diagnostic.profile_revision.empty()
                 ? std::string("unknown")
                 : diagnostic.profile_revision) +
            " • " + diagnostic.binding_id;
        model.control_diagnostics.push_back({
            row->choice_id + "|" + diagnostic.binding_id + "|" +
                diagnostic.fixture_id,
            row->choice_id,
            row->name + " • " + fixture_label,
            std::move(detail),
            provenance,
            true,
            row->safety_restricted || !row->enabled,
            diagnostic.accessibility_label});
    }
}

[[nodiscard]] std::string selection_message(
    bool target_found,
    bool look_found,
    bool profile_found) {
    if (!profile_found) {
        return "The requested fixture profile no longer exists. Select a "
            "current profile or repair the patched fixture profile reference.";
    }
    if (!target_found && !look_found) {
        return "Select a patched fixture or group and a Static Look to edit.";
    }
    if (!target_found) {
        return "Select a patched fixture or group to expose its profile controls.";
    }
    if (!look_found) {
        return "Select or create a Static Look to edit ownership and values.";
    }
    return "The requested fixture/group or Static Look no longer exists.";
}

}  // namespace

FixturesLooksShellModel build_fixtures_looks_shell_model(
    const ProjectDocument& project,
    const FixturesLooksShellQuery& query) {
    FixturesLooksShellModel model;
    model.project_id = project.id;
    model.project_name = project.name.empty() ? "Untitled project" : project.name;
    model.density = select_ui_shell_density(query.viewport_width);
    model.minimum_viewport_supported =
        query.viewport_width >= kFixturesLooksMinimumWidth &&
        query.viewport_height >= kFixturesLooksMinimumHeight;
    model.read_only = query.read_only;
    model.live_running = query.live_running;
    model.advanced_open = query.advanced_open;
    auto selected_control_category = query.control_category;
    if (selected_control_category == FixtureParameterCategory::Custom &&
        !query.include_advanced) {
        selected_control_category.reset();
    }

    const auto validation = validate_project(project);
    model.validation_error_count = validation.error_count();
    model.validation_warning_count = validation.warning_count();
    model.validation_status = validation.ok()
        ? (model.validation_warning_count == 0U
              ? "Validated"
              : std::to_string(model.validation_warning_count) + " warnings")
        : std::to_string(model.validation_error_count) + " blocking errors";

    const auto requested_target_id = resolved_target_id(
        project, query.selected_target_id);
    const auto target = inspect_fixture_target(project, requested_target_id);
    const auto* selected_fixture_value = target.group
        ? nullptr
        : find_fixture(project, requested_target_id);
    const auto* profile = selected_profile(
        project, query.selected_profile_id, selected_fixture_value);
    const auto* look = resolve_look(project, query.selected_static_look_id);

    model.selected_target_id = target.target_found
        ? std::string(target.target_id)
        : requested_target_id;
    model.selected_profile_id = profile != nullptr
        ? profile->id
        : std::string(query.selected_profile_id);
    model.selected_static_look_id = look != nullptr
        ? look->id
        : std::string(query.selected_static_look_id);
    model.selected_static_look_name = look != nullptr ? look->name : std::string{};

    const auto profile_items = profile_authoring_items(project);
    const auto profile_projection = project_authoring_items(
        profile_items, query.profile_search, model.selected_profile_id);
    model.profile_total_count = profile_projection.total_count;
    model.profile_summary = authoring_collection_summary(
        UiAuthoringResourceKind::FixtureProfile, profile_projection);
    model.profiles.reserve(profile_projection.source_indices.size());
    for (const auto index : profile_projection.source_indices) {
        const auto& source = project.fixture_profiles[index];
        const auto patched_count = static_cast<std::size_t>(std::count_if(
            project.fixtures.begin(), project.fixtures.end(),
            [&](const auto& fixture) { return fixture.profile_id == source.id; }));
        std::ostringstream accessibility;
        accessibility << source.name << ", " << profile_source_label(source.source)
                      << ", " << source.footprint << " DMX channels, "
                      << patched_count << " patched fixtures";
        model.profiles.push_back({
            source.id,
            source.name,
            source.manufacturer,
            source.model,
            source.mode,
            std::string(profile_source_label(source.source)),
            source.source_revision,
            std::to_string(source.footprint) + " channels",
            patched_count,
            source.id == model.selected_profile_id,
            source.source != showcore::FixtureProfileSource::Local,
            accessibility.str()});
    }

    model.targets.reserve(project.fixtures.size() + project.groups.size());
    for (const auto& group : project.groups) {
        const auto inspected = inspect_fixture_target(project, group.id);
        const auto complete = std::all_of(
            inspected.fixtures.begin(), inspected.fixtures.end(),
            [](const auto& fixture) { return fixture.complete; });
        const auto detail = std::to_string(group.fixture_ids.size()) +
            " fixtures  •  profile-aware group";
        model.targets.push_back({
            group.id,
            group.name,
            detail,
            group.fixture_ids.size(),
            true,
            group.id == model.selected_target_id,
            complete,
            group.name + ", group, " + detail});
    }
    for (const auto& fixture : project.fixtures) {
        const auto detail = target_detail(project, fixture);
        const auto inspected = inspect_fixture_target(project, fixture.id);
        const auto complete = !inspected.fixtures.empty() &&
            inspected.fixtures.front().complete;
        model.targets.push_back({
            fixture.id,
            fixture.name,
            detail,
            1U,
            false,
            fixture.id == model.selected_target_id,
            complete,
            fixture.name + ", " + detail});
    }

    const auto look_items = look_authoring_items(project);
    const auto look_projection = project_authoring_items(
        look_items, query.static_look_search, model.selected_static_look_id);
    model.static_look_total_count = look_projection.total_count;
    model.static_look_summary = authoring_collection_summary(
        UiAuthoringResourceKind::StaticLook, look_projection);
    model.static_looks.reserve(look_projection.source_indices.size());
    for (const auto index : look_projection.source_indices) {
        const auto& source = project.looks[index];
        const auto detail = std::to_string(source.assignments.size()) +
            " assignments  •  " + std::to_string(source.fade_ms) + " ms fade";
        model.static_looks.push_back({
            source.id,
            source.name,
            detail,
            source.fade_ms,
            source.assignments.size(),
            source.id == model.selected_static_look_id,
            source.name + ", Static Look, " + detail});
    }

    const auto completely_empty = project.fixture_profiles.empty() &&
        project.fixtures.empty() && project.groups.empty() && project.looks.empty();
    if (completely_empty) {
        model.state = FixturesLooksShellState::EmptyProject;
        model.message =
            "No fixture profiles, patched fixtures, groups, or Static Looks yet. "
            "Import an OFL/QLC+ profile or create a local profile to begin.";
    } else if (project.fixtures.empty() && project.groups.empty()) {
        model.state = FixturesLooksShellState::NoTarget;
        model.message =
            "Profiles are available, but nothing is patched. Add a fixture and "
            "choose its universe/address before authoring output.";
    } else if (project.looks.empty()) {
        model.state = FixturesLooksShellState::NoStaticLook;
        model.message =
            "No Static Looks yet. Create a Look, then choose Release, Set, or "
            "Force Zero per fixture property.";
    } else if (!target.target_found || look == nullptr || profile == nullptr) {
        model.state = FixturesLooksShellState::SelectionRequired;
        model.message = selection_message(
            target.target_found, look != nullptr, profile != nullptr);
    } else {
        FixtureFunctionComponentQuery component_query;
        component_query.target_id = target.target_id;
        component_query.surface = FixtureParameterSurface::StaticLook;
        component_query.search = query.control_search;
        component_query.category = selected_control_category;
        component_query.selected_choice_id = query.selected_choice_id;
        component_query.row_limit = kFixtureFunctionComponentMaximumRowLimit;
        auto catalog = build_fixture_function_component(
            project, component_query);
        if (selected_control_category.has_value() &&
            std::none_of(
                catalog.categories.begin(), catalog.categories.end(),
                [selected_control_category](const auto& category) {
                    return category.category == *selected_control_category;
                })) {
            selected_control_category.reset();
            component_query.category.reset();
            catalog = build_fixture_function_component(
                project, component_query);
        }
        model.control_search = catalog.search_query;
        model.control_search_truncated = catalog.search_truncated;
        model.controls_truncated = catalog.rows_truncated;
        model.selected_control_category_id = selected_control_category.has_value()
            ? std::string(fixture_parameter_category_stable_id(
                  *selected_control_category))
            : std::string("all");
        append_control_categories(
            model, catalog, query.include_advanced,
            selected_control_category);
        model.control_surface = build_fixture_control_surface(
            catalog, query.include_advanced);
        model.advanced_available = catalog.source_choice_count != 0U;
        model.state = !model.minimum_viewport_supported ||
                model.control_surface.state == FixtureFunctionComponentState::Degraded ||
                model.validation_error_count != 0U
            ? FixturesLooksShellState::Degraded
            : FixturesLooksShellState::Ready;
        model.message = model.state == FixturesLooksShellState::Ready
            ? "Fixture controls are grouped by operator task. Raw DMX and profile "
              "diagnostics remain in Advanced."
            : (!model.minimum_viewport_supported
                  ? "This slice is qualified from 1366×768. Increase the window "
                    "size to keep primary controls and actions visible."
                  : (model.validation_error_count != 0U
                        ? "Resolve blocking project validation errors before "
                          "preview or activation."
                        : model.control_surface.message));
        append_control_bindings(
            model, look, target, catalog, query.selected_choice_id);
        const auto selected_control = std::find_if(
            model.controls.begin(), model.controls.end(),
            [&](const auto& control) {
                return !query.selected_choice_id.empty() &&
                    control.choice_id == query.selected_choice_id;
            });
        if (selected_control != model.controls.end() &&
            !selected_control->value_mixed) {
            if (selected_control->accepts_value) {
                auto diagnostic_query = component_query;
                diagnostic_query.position = selected_control->normalized_value;
                const auto diagnostic_catalog =
                    build_fixture_function_component(project, diagnostic_query);
                append_control_diagnostics(
                    model, diagnostic_catalog, query.selected_choice_id);
            } else {
                append_control_diagnostics(
                    model, catalog, query.selected_choice_id);
            }
        }
        model.control_visible_count = model.controls.size();
        model.control_group_count = model.control_groups.size();
        std::ostringstream control_summary;
        control_summary << model.control_group_count << " parameter cards • "
                        << model.control_visible_count << " controls shown • "
                        << model.control_search_match_count << " matching • "
                        << model.control_total_count << " profile parameters";
        if (model.controls_truncated) {
            control_summary << " • refine search for remaining controls";
        }
        model.control_summary = control_summary.str();
    }

    model.can_edit = !model.read_only &&
        (model.state == FixturesLooksShellState::Ready ||
         model.state == FixturesLooksShellState::Degraded) &&
        target.target_found && look != nullptr;
    model.can_preview = !model.live_running &&
        model.validation_error_count == 0U && profile != nullptr &&
        target.target_found && look != nullptr &&
        (model.state == FixturesLooksShellState::Ready ||
         model.state == FixturesLooksShellState::Degraded);
    if (model.live_running) {
        model.preview_status =
            "Preview locked while Live is running • stop Live to test this Look";
    } else if (model.validation_error_count != 0U) {
        model.preview_status =
            "Preview locked • resolve blocking validation errors first";
    } else if (model.can_preview) {
        model.preview_status =
            "Preview available • bounded output, Live stopped, release on exit";
    } else {
        model.preview_status =
            "Preview unavailable until a valid target and Static Look are selected";
    }

    std::ostringstream accessibility;
    accessibility << "Fixtures and Static Looks workspace. "
                  << model.profile_total_count << " profiles, "
                  << model.targets.size() << " fixture targets, "
                  << model.static_look_total_count << " Static Looks. "
                  << model.validation_status << ". " << model.preview_status;
    model.accessibility_label = accessibility.str();
    return model;
}

std::string_view fixtures_looks_shell_state_name(
    FixturesLooksShellState state) noexcept {
    switch (state) {
    case FixturesLooksShellState::Ready: return "ready";
    case FixturesLooksShellState::EmptyProject: return "emptyProject";
    case FixturesLooksShellState::NoTarget: return "noTarget";
    case FixturesLooksShellState::NoStaticLook: return "noStaticLook";
    case FixturesLooksShellState::SelectionRequired: return "selectionRequired";
    case FixturesLooksShellState::Degraded: return "degraded";
    }
    return "degraded";
}

std::string_view static_look_ownership_state_name(
    StaticLookOwnershipState state) noexcept {
    switch (state) {
    case StaticLookOwnershipState::Release: return "release";
    case StaticLookOwnershipState::Set: return "set";
    case StaticLookOwnershipState::ForceZero: return "forceZero";
    case StaticLookOwnershipState::Mixed: return "mixed";
    }
    return "mixed";
}

}  // namespace emberlights
