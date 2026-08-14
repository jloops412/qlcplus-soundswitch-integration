#include "emberlights/fixture_function_component.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace emberlights {
namespace {

constexpr std::size_t kCategoryCount =
    static_cast<std::size_t>(FixtureParameterCategory::Custom) + 1U;
constexpr float kSemanticValueTolerance = 0.000001F;

struct CanonicalTarget {
    FixtureFunctionTargetKind kind{FixtureFunctionTargetKind::Missing};
    std::string_view id;
    std::string_view name;
    bool complete{false};
};

[[nodiscard]] bool same_value(float first, float second) noexcept {
    return std::isfinite(first) && std::isfinite(second) &&
        std::fabs(first - second) <= kSemanticValueTolerance;
}

[[nodiscard]] bool valid_normalized(float value) noexcept {
    return std::isfinite(value) && value >= 0.0F && value <= 1.0F;
}

[[nodiscard]] float normalized_position(float position) noexcept {
    return std::isfinite(position)
        ? std::clamp(position, 0.0F, 1.0F)
        : 0.5F;
}

[[nodiscard]] const FixtureDefinition* find_fixture(
    const ProjectDocument& project,
    std::string_view id) noexcept {
    const auto found = std::find_if(
        project.fixtures.begin(), project.fixtures.end(),
        [id](const auto& fixture) { return fixture.id == id; });
    return found == project.fixtures.end() ? nullptr : &*found;
}

[[nodiscard]] const GroupDefinition* find_group(
    const ProjectDocument& project,
    std::string_view id) noexcept {
    const auto found = std::find_if(
        project.groups.begin(), project.groups.end(),
        [id](const auto& group) { return group.id == id; });
    return found == project.groups.end() ? nullptr : &*found;
}

[[nodiscard]] bool fixture_is_complete(
    const ProjectDocument& project,
    const FixtureDefinition& fixture) noexcept {
    const auto found = std::find_if(
        project.fixtures.begin(), project.fixtures.end(),
        [&fixture](const auto& candidate) { return &candidate == &fixture; });
    if (found == project.fixtures.end() ||
        static_cast<std::size_t>(found - project.fixtures.begin()) >=
            showcore::kMaxFixtures) {
        return false;
    }
    return find_fixture_profile(project, fixture.profile_id) != nullptr;
}

[[nodiscard]] bool group_is_complete(
    const ProjectDocument& project,
    const GroupDefinition& group) noexcept {
    if (group.fixture_ids.empty()) {
        return true;
    }
    return std::all_of(
        group.fixture_ids.begin(), group.fixture_ids.end(),
        [&project](const auto& fixture_id) {
            const auto* fixture = find_fixture(project, fixture_id);
            return fixture != nullptr && fixture_is_complete(project, *fixture);
        });
}

[[nodiscard]] CanonicalTarget resolve_target(
    const ProjectDocument& project,
    std::string_view id) noexcept {
    if (const auto* fixture = find_fixture(project, id); fixture != nullptr) {
        return {
            FixtureFunctionTargetKind::Fixture,
            fixture->id,
            fixture->name,
            fixture_is_complete(project, *fixture)};
    }
    if (const auto* group = find_group(project, id); group != nullptr) {
        return {
            FixtureFunctionTargetKind::Group,
            group->id,
            group->name,
            group_is_complete(project, *group)};
    }
    return {};
}

[[nodiscard]] FixtureParameterCategory choice_category(
    const FixtureControlChoice& choice) noexcept {
    const auto* descriptor = fixture_parameter_descriptor(choice.property);
    return descriptor == nullptr
        ? FixtureParameterCategory::Custom
        : descriptor->category;
}

[[nodiscard]] bool valid_category(
    FixtureParameterCategory category) noexcept {
    return static_cast<std::size_t>(category) < kCategoryCount;
}

[[nodiscard]] std::size_t category_index(
    FixtureParameterCategory category) noexcept {
    return valid_category(category)
        ? static_cast<std::size_t>(category)
        : static_cast<std::size_t>(FixtureParameterCategory::Custom);
}

[[nodiscard]] char ascii_fold(char value) noexcept {
    return value >= 'A' && value <= 'Z'
        ? static_cast<char>(value + ('a' - 'A'))
        : value;
}

[[nodiscard]] bool contains_ascii_folded(
    std::string_view text,
    std::string_view needle) noexcept {
    if (needle.empty()) {
        return true;
    }
    if (needle.size() > text.size()) {
        return false;
    }
    for (std::size_t offset = 0U;
         offset + needle.size() <= text.size();
         ++offset) {
        bool equal = true;
        for (std::size_t index = 0U; index < needle.size(); ++index) {
            if (ascii_fold(text[offset + index]) != ascii_fold(needle[index])) {
                equal = false;
                break;
            }
        }
        if (equal) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::vector<std::string_view> search_tokens(
    std::string_view search) {
    std::vector<std::string_view> tokens;
    std::size_t cursor = 0U;
    while (cursor < search.size()) {
        while (cursor < search.size() &&
               (search[cursor] == ' ' || search[cursor] == '\t' ||
                search[cursor] == '\r' || search[cursor] == '\n')) {
            ++cursor;
        }
        const auto start = cursor;
        while (cursor < search.size() &&
               search[cursor] != ' ' && search[cursor] != '\t' &&
               search[cursor] != '\r' && search[cursor] != '\n') {
            ++cursor;
        }
        if (start != cursor) {
            tokens.push_back(search.substr(start, cursor - start));
        }
    }
    return tokens;
}

[[nodiscard]] bool token_matches_choice(
    const ProjectDocument& project,
    const FixtureControlChoice& choice,
    FixtureParameterCategory category,
    std::string_view token) noexcept {
    const auto* descriptor = fixture_parameter_descriptor(choice.property);
    const std::array<std::string_view, 8U> fields{{
        choice.name,
        choice.capability_id,
        choice.owner,
        property_name(choice.property),
        fixture_parameter_category_name(category),
        descriptor == nullptr ? std::string_view{} : descriptor->stable_id,
        descriptor == nullptr ? std::string_view{} : descriptor->display_name,
        descriptor == nullptr ? std::string_view{} : descriptor->description}};
    if (std::any_of(
            fields.begin(), fields.end(),
            [token](std::string_view field) {
                return contains_ascii_folded(field, token);
            })) {
        return true;
    }
    for (const auto& value : choice.values) {
        const auto* fixture = find_fixture(project, value.fixture_id);
        const auto* profile = find_fixture_profile(project, value.profile_id);
        const std::array<std::string_view, 9U> diagnostic_fields{{
            value.fixture_id,
            fixture == nullptr ? std::string_view{} : fixture->name,
            value.profile_id,
            profile == nullptr ? std::string_view{} : profile->name,
            profile == nullptr ? std::string_view{} : profile->manufacturer,
            profile == nullptr ? std::string_view{} : profile->model,
            profile == nullptr ? std::string_view{} : profile->mode,
            profile == nullptr ? std::string_view{} : profile->source_revision,
            value.binding_id}};
        if (std::any_of(
                diagnostic_fields.begin(), diagnostic_fields.end(),
                [token](std::string_view field) {
                    return contains_ascii_folded(field, token);
                })) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool choice_matches_search(
    const ProjectDocument& project,
    const FixtureControlChoice& choice,
    FixtureParameterCategory category,
    const std::vector<std::string_view>& tokens) noexcept {
    return std::all_of(
        tokens.begin(), tokens.end(),
        [&](std::string_view token) {
            return token_matches_choice(project, choice, category, token);
        });
}

[[nodiscard]] std::string coverage_text(
    const FixtureFunctionCoverage& coverage) {
    if (coverage.target_fixture_count == 1U && coverage.exact()) {
        return "Available for the selected fixture.";
    }
    return "Supported by " +
        std::to_string(coverage.supported_fixture_count) + " of " +
        std::to_string(coverage.target_fixture_count) + " target fixtures.";
}

[[nodiscard]] std::string unavailable_reason_text(
    FixtureFunctionReason reason,
    const FixtureFunctionCoverage& coverage,
    std::string_view safety_label) {
    switch (reason) {
    case FixtureFunctionReason::None:
        return "Available for an exact Live override command.";
    case FixtureFunctionReason::TargetIncomplete:
        return "Unavailable because the target contains a missing fixture or fixture profile.";
    case FixtureFunctionReason::PartialGroupCoverage:
        return "Unavailable because only " +
            std::to_string(coverage.supported_fixture_count) + " of " +
            std::to_string(coverage.target_fixture_count) +
            " fixtures support this function; partial group commands are not allowed.";
    case FixtureFunctionReason::ProfileValuesDiffer:
        return "Unavailable because target profiles require different semantic values; one group override cannot preserve them exactly.";
    case FixtureFunctionReason::SafetyGateRequired:
        return "Safety confirmation required (" + std::string(safety_label) +
            "); this component does not arm or confirm hazardous output.";
    case FixtureFunctionReason::InconsistentChoice:
        return "Unavailable because the profile function did not resolve to one finite normalized value.";
    default:
        return "Unavailable.";
    }
}

[[nodiscard]] bool choice_is_favorite(
    const FixtureFunctionComponentQuery& query,
    std::string_view choice_id) noexcept {
    return std::any_of(
        query.favorite_choice_ids.begin(),
        query.favorite_choice_ids.end(),
        [choice_id](std::string_view favorite_id) {
            return favorite_id == choice_id;
        });
}

[[nodiscard]] bool surface_accepts_profile_specific_values(
    FixtureParameterSurface surface) noexcept {
    return surface == FixtureParameterSurface::StaticLook ||
        surface == FixtureParameterSurface::Autoloop ||
        surface == FixtureParameterSurface::Controller;
}

[[nodiscard]] FixtureFunctionDmxDiagnostic make_diagnostic(
    const ProjectDocument& project,
    const FixtureControlChoiceValue& value) {
    FixtureFunctionDmxDiagnostic diagnostic;
    diagnostic.fixture_id = value.fixture_id;
    diagnostic.profile_id = value.profile_id;
    diagnostic.binding_id = value.binding_id;
    diagnostic.channel = value.channel;
    diagnostic.property = value.property;
    diagnostic.normalized_value = value.normalized_value;
    diagnostic.raw_value = value.raw_value;
    diagnostic.dmx_min = value.dmx_min;
    diagnostic.dmx_max = value.dmx_max;
    diagnostic.encoding = value.encoding;
    diagnostic.fine_channel = value.fine_channel;
    diagnostic.raw_fine_value = value.raw_fine_value;
    diagnostic.default_value = value.default_value;
    diagnostic.blackout_value = value.blackout_value;
    diagnostic.highlight_value = value.highlight_value;
    if (const auto* fixture = find_fixture(project, value.fixture_id);
        fixture != nullptr) {
        diagnostic.fixture_name = fixture->name;
    }
    if (const auto* profile = find_fixture_profile(project, value.profile_id);
        profile != nullptr) {
        diagnostic.profile_name = profile->name;
        diagnostic.profile_revision = profile->source_revision;
    }
    const auto fixture_label = diagnostic.fixture_name.empty()
        ? diagnostic.fixture_id
        : diagnostic.fixture_name;
    const auto profile_label = diagnostic.profile_name.empty()
        ? diagnostic.profile_id
        : diagnostic.profile_name;
    diagnostic.accessibility_label =
        fixture_label + ", profile " + profile_label + ", channel " +
        std::to_string(diagnostic.channel) + ", " +
        std::string(channel_encoding_name(diagnostic.encoding)) + ", DMX " +
        std::to_string(static_cast<unsigned int>(diagnostic.raw_value)) +
        (diagnostic.fine_channel == 0U
             ? std::string{}
             : ", fine channel " +
                   std::to_string(diagnostic.fine_channel) + ", DMX " +
                   std::to_string(static_cast<unsigned int>(
                       diagnostic.raw_fine_value))) +
        ", documented range " +
        std::to_string(static_cast<unsigned int>(diagnostic.dmx_min)) +
        " to " +
        std::to_string(static_cast<unsigned int>(diagnostic.dmx_max)) + ".";
    return diagnostic;
}

[[nodiscard]] bool profile_specific_dmx(
    const std::vector<FixtureFunctionDmxDiagnostic>& diagnostics) noexcept {
    if (diagnostics.size() < 2U) {
        return false;
    }
    const auto& first = diagnostics.front();
    return std::any_of(
        std::next(diagnostics.begin()), diagnostics.end(),
        [&first](const auto& diagnostic) {
            return diagnostic.channel != first.channel ||
                diagnostic.raw_value != first.raw_value ||
                diagnostic.encoding != first.encoding ||
                diagnostic.fine_channel != first.fine_channel ||
                diagnostic.raw_fine_value != first.raw_fine_value ||
                diagnostic.dmx_min != first.dmx_min ||
                diagnostic.dmx_max != first.dmx_max;
        });
}

[[nodiscard]] FixtureFunctionRow make_row(
    const ProjectDocument& project,
    const FixtureControlChoice& choice,
    bool target_complete,
    FixtureParameterSurface surface,
    bool favorite) {
    FixtureFunctionRow row;
    row.choice_id = choice.id;
    row.capability_id = choice.capability_id;
    row.name = choice.name.empty() ? choice.capability_id : choice.name;
    row.owner = choice.owner;
    row.kind = choice.kind;
    row.category = choice_category(choice);
    row.category_label = fixture_parameter_category_name(row.category);
    row.property = choice.property;
    const auto* descriptor = fixture_parameter_descriptor(choice.property);
    row.property_label = descriptor == nullptr
        ? std::string(property_name(choice.property))
        : std::string(descriptor->display_name);
    row.control_kind = descriptor == nullptr
        ? FixtureParameterControlKind::Custom
        : descriptor->control_kind;
    row.control_kind_label = fixture_parameter_control_kind_name(
        row.control_kind);
    row.behavior = choice.behavior;
    row.access = choice.access;
    row.role = choice.role;
    row.coverage = {
        choice.supported_fixture_count,
        choice.target_fixture_count};
    row.normalized_value = choice.shared_normalized_value;
    row.shared_semantic_value = choice.shared_value;
    row.favorite = favorite;
    row.accepts_position = choice.kind ==
            FixtureControlChoiceKind::DirectAttribute ||
        choice.behavior == showcore::ChannelCapabilityBehavior::Continuous;
    row.uses_exact_profile_value = choice.kind ==
            FixtureControlChoiceKind::NamedCapability &&
        choice.behavior == showcore::ChannelCapabilityBehavior::Slot;
    row.safety_restricted = choice.safety_gated() ||
        (descriptor != nullptr && descriptor->safety_restricted());
    row.live_override_compatible = target_complete &&
        choice.live_override_compatible() &&
        choice.values.size() == choice.target_fixture_count;
    row.diagnostics.reserve(choice.values.size());
    for (const auto& value : choice.values) {
        row.diagnostics.push_back(make_diagnostic(project, value));
    }
    row.has_profile_specific_dmx = profile_specific_dmx(row.diagnostics);

    if (surface == FixtureParameterSurface::Profile) {
        row.reason = FixtureFunctionReason::InvalidSurface;
    } else if (!target_complete &&
        !surface_accepts_profile_specific_values(surface)) {
        row.reason = FixtureFunctionReason::TargetIncomplete;
    } else if (surface == FixtureParameterSurface::LiveOverride &&
               (!choice.common() ||
                choice.values.size() != choice.target_fixture_count)) {
        row.reason = FixtureFunctionReason::PartialGroupCoverage;
    } else if (surface == FixtureParameterSurface::LiveOverride &&
               !choice.shared_value) {
        row.reason = FixtureFunctionReason::ProfileValuesDiffer;
    } else if (choice.values.empty() ||
               (surface == FixtureParameterSurface::LiveOverride &&
                !valid_normalized(choice.shared_normalized_value))) {
        row.reason = FixtureFunctionReason::InconsistentChoice;
    } else if (row.safety_restricted &&
               surface != FixtureParameterSurface::StaticLook) {
        row.reason = FixtureFunctionReason::SafetyGateRequired;
    } else {
        row.reason = FixtureFunctionReason::None;
    }

    row.enabled = row.reason == FixtureFunctionReason::None &&
        (surface == FixtureParameterSurface::LiveOverride
             ? row.live_override_compatible
             : surface_accepts_profile_specific_values(surface));
    if (row.enabled) {
        row.availability = FixtureFunctionRowAvailability::Enabled;
    } else if (row.reason == FixtureFunctionReason::SafetyGateRequired) {
        row.availability =
            FixtureFunctionRowAvailability::SafetyConfirmationRequired;
    } else {
        row.availability = FixtureFunctionRowAvailability::Unavailable;
    }
    const auto safety_label = descriptor == nullptr ||
            (choice.safety_gated() &&
             descriptor->safety == FixtureParameterSafety::Normal)
        ? std::string_view{"Profile safety gate"}
        : fixture_parameter_safety_name(descriptor->safety);
    row.reason_text = unavailable_reason_text(
        row.reason, row.coverage, safety_label);
    if (row.enabled && surface == FixtureParameterSurface::StaticLook &&
        row.safety_restricted) {
        row.reason_text =
            "Authorable in a Static Look; Runner arming, caps, and preview blocks still apply.";
    } else if (row.enabled && choice.partial()) {
        row.reason_text =
            "Available for the supported fixtures; unsupported fixtures retain lower content.";
    } else if (row.enabled && !choice.shared_value) {
        row.reason_text =
            "Available with exact per-profile values preserved for each fixture.";
    } else if (row.enabled &&
               surface != FixtureParameterSurface::LiveOverride) {
        row.reason_text =
            "Available on this authoring surface; semantic values resolve through each fixture profile.";
    }
    row.accessibility_label = row.name + ", " + row.property_label +
        (row.kind == FixtureControlChoiceKind::DirectAttribute
             ? " direct fixture channel, "
             : " named DMX function, ") +
        row.category_label + ", " + row.control_kind_label + ".";
    row.accessibility_description = coverage_text(row.coverage) + " " +
        row.reason_text + " " +
        std::to_string(row.diagnostics.size()) +
        " profile DMX diagnostics available.";
    return row;
}

void append_warning(
    FixtureFunctionComponentModel& model,
    std::string warning) {
    if (model.warnings.size() < kFixtureFunctionComponentMaximumWarnings) {
        model.warnings.push_back(std::move(warning));
    } else {
        model.warnings_truncated = true;
    }
}

[[nodiscard]] bool same_choice_snapshot(
    const FixtureFunctionRow& row,
    const FixtureControlChoice& choice) noexcept {
    const auto expected_name = choice.name.empty()
        ? std::string_view{choice.capability_id}
        : std::string_view{choice.name};
    if (row.choice_id != choice.id ||
        row.capability_id != choice.capability_id ||
        row.name != expected_name || row.owner != choice.owner ||
        row.kind != choice.kind ||
        row.property != choice.property || row.behavior != choice.behavior ||
        row.access != choice.access || row.role != choice.role ||
        row.coverage.supported_fixture_count !=
            choice.supported_fixture_count ||
        row.coverage.target_fixture_count != choice.target_fixture_count ||
        row.shared_semantic_value != choice.shared_value ||
        row.diagnostics.size() != choice.values.size() ||
        (choice.shared_value &&
         !same_value(row.normalized_value, choice.shared_normalized_value))) {
        return false;
    }
    for (const auto& value : choice.values) {
        const auto matching_diagnostics = static_cast<std::size_t>(std::count_if(
            row.diagnostics.begin(), row.diagnostics.end(),
            [&value](const auto& diagnostic) {
                return value.fixture_id == diagnostic.fixture_id &&
                    value.profile_id == diagnostic.profile_id &&
                    value.binding_id == diagnostic.binding_id &&
                    value.channel == diagnostic.channel &&
                    value.property == diagnostic.property &&
                    value.raw_value == diagnostic.raw_value &&
                    value.encoding == diagnostic.encoding &&
                    value.fine_channel == diagnostic.fine_channel &&
                    value.raw_fine_value == diagnostic.raw_fine_value &&
                    value.default_value == diagnostic.default_value &&
                    value.blackout_value == diagnostic.blackout_value &&
                    value.highlight_value == diagnostic.highlight_value &&
                    value.dmx_min == diagnostic.dmx_min &&
                    value.dmx_max == diagnostic.dmx_max &&
                    same_value(
                        value.normalized_value,
                        diagnostic.normalized_value);
            }));
        if (matching_diagnostics != 1U) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] FixtureFunctionCommandBuildResult command_failure(
    FixtureFunctionReason reason,
    std::string message) {
    FixtureFunctionCommandBuildResult result;
    result.reason = reason;
    result.message = std::move(message);
    return result;
}

}  // namespace

FixtureFunctionComponentModel build_fixture_function_component(
    const ProjectDocument& project,
    const FixtureFunctionComponentQuery& query) {
    FixtureFunctionComponentModel model;
    model.target_id = std::string(query.target_id);
    model.surface = query.surface;
    model.position = normalized_position(query.position);
    model.category_filter = query.category;
    model.favorites_only = query.favorites_only;
    model.selected_choice_id = std::string(query.selected_choice_id);
    const auto search_size = std::min(
        query.search.size(), kFixtureFunctionComponentMaximumSearchBytes);
    if (search_size != 0U) {
        model.search_query.assign(query.search.data(), search_size);
    }
    model.search_truncated = search_size != query.search.size();

    const auto target = resolve_target(project, query.target_id);
    model.target_kind = target.kind;
    model.target_name = std::string(target.name);
    model.target_complete = target.complete;
    if (target.kind == FixtureFunctionTargetKind::Missing) {
        model.state = FixtureFunctionComponentState::Unavailable;
        model.reason = FixtureFunctionReason::TargetNotFound;
        model.message = "The fixture or group target no longer exists.";
        model.accessibility_label = "Fixture controls unavailable: target missing.";
        return model;
    }

    const auto catalog = fixture_control_choices(
        project, target.id, model.position);
    model.target_id = catalog.target_id;
    model.target_name = catalog.target_name;
    model.target_fixture_count = catalog.target_fixture_count;
    for (const auto& warning : catalog.warnings) {
        append_warning(model, warning);
    }
    if (model.search_truncated) {
        append_warning(
            model,
            "Fixture-control search was limited to the first " +
                std::to_string(kFixtureFunctionComponentMaximumSearchBytes) +
                " bytes.");
    }

    if (!target.complete && catalog.target_fixture_count == 0U) {
        model.state = FixtureFunctionComponentState::Unavailable;
        model.reason = FixtureFunctionReason::TargetIncomplete;
        model.message = "The target is missing a patched fixture profile.";
        model.accessibility_label =
            model.target_name + ", fixture controls unavailable: incomplete target.";
        return model;
    }
    if (catalog.target_fixture_count == 0U) {
        model.state = FixtureFunctionComponentState::Empty;
        model.reason = FixtureFunctionReason::TargetEmpty;
        model.message = "The selected target has no patched fixtures.";
        model.accessibility_label =
            model.target_name + ", no patched fixtures available.";
        return model;
    }

    const auto tokens = search_tokens(model.search_query);
    const auto requested_limit = query.row_limit == 0U ? 1U : query.row_limit;
    const auto row_limit = std::min(
        requested_limit, kFixtureFunctionComponentMaximumRowLimit);
    std::array<std::size_t, kCategoryCount> total_by_category{};
    std::array<std::size_t, kCategoryCount> search_by_category{};
    std::array<std::size_t, kCategoryCount> favorite_by_category{};
    std::array<std::size_t, kCategoryCount> visible_by_category{};
    model.rows.reserve(std::min(row_limit, catalog.choices.size()));

    for (const auto& choice : catalog.choices) {
        // fixture_control_choices already removes these; retain a second
        // boundary so malformed future catalog input cannot expose a range.
        if (choice.access == showcore::ChannelCapabilityAccess::Protected) {
            continue;
        }
        const auto category = choice_category(choice);
        const auto index = category_index(category);
        ++model.source_choice_count;
        ++total_by_category[index];
        const bool search_match = choice_matches_search(
            project, choice, category, tokens);
        const bool favorite = choice_is_favorite(query, choice.id);
        if (favorite) {
            ++model.favorite_choice_count;
            ++favorite_by_category[index];
        }
        if (search_match) {
            ++search_by_category[index];
        }
        const bool category_match =
            !query.category.has_value() ||
            (valid_category(*query.category) && *query.category == category);
        if (!search_match || !category_match ||
            (query.favorites_only && !favorite)) {
            continue;
        }
        ++model.matching_choice_count;
        if (model.rows.size() >= row_limit) {
            model.rows_truncated = true;
            continue;
        }
        model.rows.push_back(make_row(
            project, choice, target.complete, query.surface, favorite));
        if (choice.id == query.selected_choice_id) {
            model.selection_visible = true;
        }
        ++visible_by_category[index];
    }

    for (std::size_t index = 0U; index < kCategoryCount; ++index) {
        if (total_by_category[index] == 0U) {
            continue;
        }
        const auto category = static_cast<FixtureParameterCategory>(index);
        model.categories.push_back({
            category,
            std::string(fixture_parameter_category_name(category)),
            total_by_category[index],
            search_by_category[index],
            favorite_by_category[index],
            visible_by_category[index]});
    }

    if (model.source_choice_count == 0U) {
        model.state = target.complete
            ? FixtureFunctionComponentState::Empty
            : FixtureFunctionComponentState::Unavailable;
        model.reason = target.complete
            ? FixtureFunctionReason::NoFunctions
            : FixtureFunctionReason::TargetIncomplete;
        model.message = target.complete
            ? "No selectable profile-backed fixture controls are defined for this target."
            : "The target is missing a fixture or fixture profile.";
    } else if (model.rows.empty()) {
        model.state = FixtureFunctionComponentState::Empty;
        model.reason = FixtureFunctionReason::NoMatches;
        model.message = query.favorites_only && model.favorite_choice_count == 0U
            ? "No fixture controls have been marked as favorites yet."
            : "No fixture controls match the current search, category, and favorites filters.";
    } else {
        const bool any_enabled = std::any_of(
            model.rows.begin(), model.rows.end(),
            [](const auto& row) { return row.enabled; });
        model.state = any_enabled
            ? FixtureFunctionComponentState::Ready
            : FixtureFunctionComponentState::Degraded;
        if (!any_enabled) {
            model.reason = FixtureFunctionReason::NoLiveCompatibleFunctions;
            model.message = query.surface == FixtureParameterSurface::LiveOverride
                ? "Matching fixture controls are visible for diagnosis but none can form one exact Live override command."
                : "Matching fixture controls are visible for diagnosis but unavailable on this authoring surface.";
        } else if (model.rows_truncated) {
            model.reason = FixtureFunctionReason::RowLimitReached;
            model.message = "Fixture controls are available; refine the search to see results beyond the bounded row limit.";
        } else {
            model.reason = FixtureFunctionReason::None;
            model.message = query.surface == FixtureParameterSurface::LiveOverride
                ? "Exact fixture controls are ready for typed Live override command construction."
                : "Profile-backed fixture controls are ready for this authoring surface.";
        }
    }

    std::ostringstream accessibility;
    accessibility << model.target_name << ", " << model.rows.size()
                  << " fixture control";
    if (model.rows.size() != 1U) {
        accessibility << 's';
    }
    accessibility << " shown, " << model.matching_choice_count
                  << " matching, " << model.target_fixture_count
                  << " target fixture";
    if (model.target_fixture_count != 1U) {
        accessibility << 's';
    }
    accessibility << ". " << model.message;
    model.accessibility_label = accessibility.str();
    return model;
}

FixtureFunctionCommandBuildResult build_fixture_function_invocation(
    const ProjectDocument& project,
    const FixtureFunctionComponentModel& snapshot,
    const FixtureFunctionCommandRequest& request) {
    if (request.action != FixtureFunctionCommandAction::Set &&
        request.action != FixtureFunctionCommandAction::Release) {
        return command_failure(
            FixtureFunctionReason::InvalidAction,
            "The fixture-control command action is invalid.");
    }
    if (snapshot.surface != FixtureParameterSurface::LiveOverride) {
        return command_failure(
            FixtureFunctionReason::InvalidSurface,
            "Only a Live Fixture Control Inspector snapshot can build an override command.");
    }
    if (request.choice_id.empty()) {
        return command_failure(
            FixtureFunctionReason::SelectionMissing,
            "No stable fixture-control choice ID was supplied.");
    }
    const auto matching_rows = static_cast<std::size_t>(std::count_if(
        snapshot.rows.begin(), snapshot.rows.end(),
        [&request](const auto& row) {
            return row.choice_id == request.choice_id;
        }));
    if (matching_rows != 1U) {
        return command_failure(
            FixtureFunctionReason::SelectionMissing,
            "The stable fixture-control choice is not present exactly once in this component snapshot.");
    }
    const auto snapshot_row = std::find_if(
        snapshot.rows.begin(), snapshot.rows.end(),
        [&request](const auto& row) {
            return row.choice_id == request.choice_id;
        });

    const auto target = resolve_target(project, snapshot.target_id);
    if (target.kind == FixtureFunctionTargetKind::Missing) {
        return command_failure(
            FixtureFunctionReason::TargetNotFound,
            "The fixture or group target no longer exists.");
    }
    if (target.kind != snapshot.target_kind || target.id != snapshot.target_id) {
        return command_failure(
            FixtureFunctionReason::SelectionStale,
            "The stable target identity changed after the component snapshot was built.");
    }

    const auto catalog = fixture_control_choices(
        project, target.id, snapshot.position);
    if (catalog.target_fixture_count == 0U) {
        return command_failure(
            target.complete
                ? FixtureFunctionReason::TargetEmpty
                : FixtureFunctionReason::TargetIncomplete,
            target.complete
                ? "The selected target no longer has patched fixtures."
                : "The selected target is missing a patched fixture profile.");
    }
    const auto fresh_choice = std::find_if(
        catalog.choices.begin(), catalog.choices.end(),
        [&request](const auto& choice) {
            return choice.id == request.choice_id;
        });
    if (fresh_choice == catalog.choices.end() ||
        fresh_choice->access == showcore::ChannelCapabilityAccess::Protected) {
        return command_failure(
            FixtureFunctionReason::SelectionStale,
            "The selected fixture control is missing, protected, or no longer supported by this target.");
    }
    if (catalog.target_fixture_count != snapshot.target_fixture_count ||
        target.complete != snapshot.target_complete ||
        !same_choice_snapshot(*snapshot_row, *fresh_choice)) {
        return command_failure(
            FixtureFunctionReason::SelectionStale,
            "The selected fixture control changed after the component snapshot was built; refresh before invoking it.");
    }

    const auto fresh_row = make_row(
        project,
        *fresh_choice,
        target.complete,
        FixtureParameterSurface::LiveOverride,
        snapshot_row->favorite);
    if (!fresh_row.enabled) {
        return command_failure(fresh_row.reason, fresh_row.reason_text);
    }

    UiCommandInvocation invocation;
    invocation.command = target.kind == FixtureFunctionTargetKind::Group
        ? (request.action == FixtureFunctionCommandAction::Set
               ? UiCommandId::GroupOverridePropertySet
               : UiCommandId::GroupOverridePropertyRelease)
        : (request.action == FixtureFunctionCommandAction::Set
               ? UiCommandId::FixtureOverridePropertySet
               : UiCommandId::FixtureOverridePropertyRelease);
    invocation.target_id = target.id;
    invocation.property = fresh_choice->property;
    if (request.action == FixtureFunctionCommandAction::Set) {
        invocation.number_value = fresh_choice->shared_normalized_value;
    }

    FixtureFunctionCommandBuildResult result;
    result.reason = FixtureFunctionReason::None;
    result.message =
        "The exact typed override command was constructed but not dispatched.";
    result.invocation = invocation;
    return result;
}

const char* fixture_function_reason_name(
    FixtureFunctionReason reason) noexcept {
    switch (reason) {
    case FixtureFunctionReason::None: return "None";
    case FixtureFunctionReason::TargetNotFound: return "TargetNotFound";
    case FixtureFunctionReason::TargetEmpty: return "TargetEmpty";
    case FixtureFunctionReason::TargetIncomplete: return "TargetIncomplete";
    case FixtureFunctionReason::NoFunctions: return "NoFunctions";
    case FixtureFunctionReason::NoMatches: return "NoMatches";
    case FixtureFunctionReason::NoLiveCompatibleFunctions:
        return "NoLiveCompatibleFunctions";
    case FixtureFunctionReason::PartialGroupCoverage:
        return "PartialGroupCoverage";
    case FixtureFunctionReason::ProfileValuesDiffer:
        return "ProfileValuesDiffer";
    case FixtureFunctionReason::SafetyGateRequired:
        return "SafetyGateRequired";
    case FixtureFunctionReason::SelectionMissing: return "SelectionMissing";
    case FixtureFunctionReason::SelectionStale: return "SelectionStale";
    case FixtureFunctionReason::InconsistentChoice:
        return "InconsistentChoice";
    case FixtureFunctionReason::RowLimitReached: return "RowLimitReached";
    case FixtureFunctionReason::InvalidAction: return "InvalidAction";
    case FixtureFunctionReason::InvalidSurface: return "InvalidSurface";
    }
    return "Unknown";
}

}  // namespace emberlights
