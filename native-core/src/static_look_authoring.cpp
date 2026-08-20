#include "emberlights/static_look_authoring.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

namespace emberlights {
namespace {

constexpr std::array<StaticLookSwatch, 10U> kBuiltInSwatches{{
    {"red", "Red", {1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F}},
    {"green", "Green", {0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F}},
    {"blue", "Blue", {0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F}},
    {"white-emitter", "White emitter", {0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F}},
    {"amber-emitter", "Amber emitter", {0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 1.0F}},
    {"uv-emitter", "UV emitter", {0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 1.0F}},
    {"cyan-rgb", "Cyan (RGB)", {0.0F, 1.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F}},
    {"magenta-rgb", "Magenta (RGB)", {1.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F}},
    {"yellow-rgb", "Yellow (RGB)", {1.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F}},
    {"black", "Black", {0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F}}}};

[[nodiscard]] bool normalized(float value) noexcept {
    return std::isfinite(value) && value >= 0.0F && value <= 1.0F;
}

[[nodiscard]] bool valid_color(const StaticLookColor& color) noexcept {
    return normalized(color.red) && normalized(color.green) &&
        normalized(color.blue) && normalized(color.white) &&
        normalized(color.amber) && normalized(color.uv) &&
        normalized(color.intensity);
}

[[nodiscard]] float emitter_value(
    const StaticLookColor& color,
    showcore::Property property) noexcept {
    switch (property) {
    case showcore::Property::Red: return color.red;
    case showcore::Property::Green: return color.green;
    case showcore::Property::Blue: return color.blue;
    case showcore::Property::White: return color.white;
    case showcore::Property::Amber: return color.amber;
    case showcore::Property::UV: return color.uv;
    default: return 0.0F;
    }
}

[[nodiscard]] bool same_value(
    const showcore::PropertyValue& first,
    const showcore::PropertyValue& second) noexcept {
    return first.mode == second.mode &&
        (first.mode != showcore::ValueMode::Set || first.value == second.value);
}

[[nodiscard]] bool upsert_assignment(
    LookDefinition& look,
    std::string_view fixture_id,
    showcore::Property property,
    showcore::PropertyValue value) {
    const auto found = std::find_if(
        look.assignments.begin(),
        look.assignments.end(),
        [fixture_id, property](const auto& assignment) {
            return assignment.fixture_id == fixture_id &&
                assignment.property == property;
        });
    if (found != look.assignments.end()) {
        if (same_value(found->value, value)) {
            return false;
        }
        found->value = value;
        return true;
    }
    look.assignments.push_back({std::string(fixture_id), property, value});
    return true;
}

void sort_assignments(
    LookDefinition& look,
    const ProjectDocument& project) {
    std::unordered_map<std::string_view, std::size_t> fixture_order;
    for (std::size_t index = 0U; index < project.fixtures.size(); ++index) {
        fixture_order.emplace(project.fixtures[index].id, index);
    }
    std::stable_sort(
        look.assignments.begin(),
        look.assignments.end(),
        [&](const auto& first, const auto& second) {
            const auto first_index = fixture_order.find(first.fixture_id);
            const auto second_index = fixture_order.find(second.fixture_id);
            const auto first_order = first_index == fixture_order.end()
                ? project.fixtures.size()
                : first_index->second;
            const auto second_order = second_index == fixture_order.end()
                ? project.fixtures.size()
                : second_index->second;
            if (first_order != second_order) {
                return first_order < second_order;
            }
            if (first.fixture_id != second.fixture_id) {
                return first.fixture_id < second.fixture_id;
            }
            return first.property < second.property;
        });
}

[[nodiscard]] bool valid_property_value(showcore::PropertyValue value) noexcept {
    return value.mode != showcore::ValueMode::Set || normalized(value.value);
}

[[nodiscard]] unsigned int decode_hex_pair(char first, char second, bool& ok) noexcept {
    const auto digit = [](char value) noexcept -> int {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        if (value >= 'A' && value <= 'F') return value - 'A' + 10;
        return -1;
    };
    const auto high = digit(first);
    const auto low = digit(second);
    ok = ok && high >= 0 && low >= 0;
    return high >= 0 && low >= 0
        ? static_cast<unsigned int>(high * 16 + low)
        : 0U;
}

}  // namespace

StaticLookDraft make_static_look_draft(
    StudioDocumentGeneration generation,
    std::string id,
    std::string name) {
    StaticLookDraft draft;
    draft.base_generation = generation;
    draft.look.id = std::move(id);
    draft.look.name = std::move(name);
    draft.look.fade_ms = 750U;
    return draft;
}

std::optional<StaticLookDraft> load_static_look_draft(
    const StudioDocumentSnapshot& snapshot,
    std::size_t look_index) {
    if (look_index >= snapshot.document.looks.size()) {
        return std::nullopt;
    }
    StaticLookDraft draft;
    draft.base_generation = snapshot.generation;
    draft.source_index = look_index;
    draft.look = snapshot.document.looks[look_index];
    return draft;
}

StaticLookDraft duplicate_static_look_draft(
    const StaticLookDraft& source,
    std::string new_id,
    std::string new_name) {
    auto duplicate = source;
    duplicate.source_index.reset();
    duplicate.look.id = std::move(new_id);
    duplicate.look.name = std::move(new_name);
    return duplicate;
}

StaticLookAuthoringOutcome apply_static_look_color(
    StaticLookDraft& draft,
    const ProjectDocument& project,
    std::string_view target_id,
    const StaticLookColor& color,
    StaticLookColorApplyOptions options) {
    StaticLookAuthoringOutcome outcome;
    if (!valid_color(color)) {
        outcome.result = StaticLookAuthoringResult::InvalidValue;
        return outcome;
    }
    const auto target = inspect_fixture_target(project, target_id);
    outcome.warnings = target.warnings;
    if (!target.target_found) {
        outcome.result = StaticLookAuthoringResult::TargetNotFound;
        return outcome;
    }
    if (target.fixtures.empty()) {
        outcome.result = StaticLookAuthoringResult::EmptyTarget;
        return outcome;
    }
    outcome.fixtures_considered = target.fixtures.size();
    bool changed = false;
    for (const auto& fixture : target.fixtures) {
        if (!fixture.complete || !fixture.has_direct_emitters) {
            ++outcome.fixtures_skipped;
            continue;
        }
        bool fixture_changed = false;
        for (const auto property : kDirectEmitterProperties) {
            if (!fixture.properties[static_cast<std::size_t>(property)]) {
                continue;
            }
            fixture_changed = upsert_assignment(
                draft.look,
                fixture.fixture_id,
                property,
                showcore::PropertyValue::set(emitter_value(color, property))) ||
                fixture_changed;
            ++outcome.assignments_written;
        }
        if (options.open_master_intensity && fixture.has_master_intensity) {
            fixture_changed = upsert_assignment(
                draft.look,
                fixture.fixture_id,
                showcore::Property::Intensity,
                showcore::PropertyValue::set(color.intensity)) || fixture_changed;
            ++outcome.assignments_written;
        }
        if (options.force_strobe_off &&
            fixture.properties[static_cast<std::size_t>(showcore::Property::Strobe)]) {
            fixture_changed = upsert_assignment(
                draft.look,
                fixture.fixture_id,
                showcore::Property::Strobe,
                showcore::PropertyValue::force_zero()) || fixture_changed;
            ++outcome.assignments_written;
        }
        if (fixture_changed) {
            ++outcome.fixtures_modified;
        }
        changed = changed || fixture_changed;
    }
    if (outcome.assignments_written == 0U) {
        outcome.result = StaticLookAuthoringResult::Unsupported;
        return outcome;
    }
    sort_assignments(draft.look, project);
    outcome.result = changed
        ? StaticLookAuthoringResult::Applied
        : StaticLookAuthoringResult::NoChange;
    return outcome;
}

StaticLookAuthoringOutcome apply_static_look_property(
    StaticLookDraft& draft,
    const ProjectDocument& project,
    std::string_view target_id,
    showcore::Property property,
    showcore::PropertyValue value) {
    StaticLookAuthoringOutcome outcome;
    if (property >= showcore::Property::Count || !valid_property_value(value)) {
        outcome.result = StaticLookAuthoringResult::InvalidValue;
        return outcome;
    }
    const auto target = inspect_fixture_target(project, target_id);
    outcome.warnings = target.warnings;
    if (!target.target_found) {
        outcome.result = StaticLookAuthoringResult::TargetNotFound;
        return outcome;
    }
    if (target.fixtures.empty()) {
        outcome.result = StaticLookAuthoringResult::EmptyTarget;
        return outcome;
    }
    outcome.fixtures_considered = target.fixtures.size();
    bool changed = false;
    for (const auto& fixture : target.fixtures) {
        if (!fixture.complete ||
            !fixture.properties[static_cast<std::size_t>(property)]) {
            ++outcome.fixtures_skipped;
            continue;
        }
        if (upsert_assignment(draft.look, fixture.fixture_id, property, value)) {
            ++outcome.fixtures_modified;
            changed = true;
        }
        ++outcome.assignments_written;
    }
    if (outcome.assignments_written == 0U) {
        outcome.result = StaticLookAuthoringResult::Unsupported;
        return outcome;
    }
    if (outcome.fixtures_skipped != 0U) {
        outcome.warnings.push_back(
            std::string(property_name(property)) + " was applied to " +
            std::to_string(outcome.assignments_written) + " of " +
            std::to_string(outcome.fixtures_considered) + " fixtures.");
    }
    sort_assignments(draft.look, project);
    outcome.result = changed
        ? StaticLookAuthoringResult::Applied
        : StaticLookAuthoringResult::NoChange;
    return outcome;
}

StaticLookAuthoringOutcome apply_static_look_control_choice(
    StaticLookDraft& draft,
    const ProjectDocument& project,
    std::string_view target_id,
    std::string_view choice_id,
    float position) {
    StaticLookAuthoringOutcome outcome;
    if (!normalized(position) || choice_id.empty()) {
        outcome.result = StaticLookAuthoringResult::InvalidValue;
        return outcome;
    }
    const auto catalog = fixture_control_choices(project, target_id, position);
    outcome.warnings = catalog.warnings;
    if (!catalog.target_found) {
        outcome.result = StaticLookAuthoringResult::TargetNotFound;
        return outcome;
    }
    if (catalog.target_fixture_count == 0U) {
        outcome.result = StaticLookAuthoringResult::EmptyTarget;
        return outcome;
    }
    const auto selected = std::find_if(
        catalog.choices.begin(), catalog.choices.end(),
        [choice_id](const auto& choice) { return choice.id == choice_id; });
    if (selected == catalog.choices.end() || selected->values.empty()) {
        outcome.result = StaticLookAuthoringResult::Unsupported;
        return outcome;
    }

    auto candidate = draft;
    outcome.fixtures_considered = catalog.target_fixture_count;
    outcome.assignments_written = selected->values.size();
    outcome.fixtures_skipped = outcome.fixtures_considered - std::min(
        outcome.fixtures_considered, selected->values.size());
    for (const auto& value : selected->values) {
        if (upsert_assignment(
                candidate.look,
                value.fixture_id,
                value.property,
                showcore::PropertyValue::set(value.normalized_value))) {
            ++outcome.fixtures_modified;
        }
    }
    if (selected->partial()) {
        outcome.warnings.push_back(
            selected->name + " was applied to " +
            std::to_string(selected->supported_fixture_count) + " of " +
            std::to_string(selected->target_fixture_count) + " fixtures.");
    }
    if (catalog.group && !selected->shared_value) {
        outcome.warnings.push_back(
            selected->name +
            " uses profile-specific values. Static Look authoring preserved each exact value; "
            "one-value Live group control is unavailable.");
    }
    if (selected->safety_gated()) {
        outcome.warnings.push_back(
            selected->name + " remains subject to the normal Runner safety gate.");
    }
    sort_assignments(candidate.look, project);
    const auto changed = outcome.fixtures_modified != 0U;
    draft = std::move(candidate);
    outcome.result = changed
        ? StaticLookAuthoringResult::Applied
        : StaticLookAuthoringResult::NoChange;
    return outcome;
}

StaticLookAuthoringOutcome remove_static_look_property(
    StaticLookDraft& draft,
    const ProjectDocument& project,
    std::string_view target_id,
    showcore::Property property) {
    StaticLookAuthoringOutcome outcome;
    if (property >= showcore::Property::Count) {
        outcome.result = StaticLookAuthoringResult::InvalidValue;
        return outcome;
    }
    const auto target = inspect_fixture_target(project, target_id);
    outcome.warnings = target.warnings;
    if (!target.target_found) {
        outcome.result = StaticLookAuthoringResult::TargetNotFound;
        return outcome;
    }
    outcome.fixtures_considered = target.fixtures.size();
    for (const auto& fixture : target.fixtures) {
        const auto before = draft.look.assignments.size();
        std::erase_if(draft.look.assignments, [&](const auto& assignment) {
            return assignment.fixture_id == fixture.fixture_id &&
                assignment.property == property;
        });
        if (draft.look.assignments.size() != before) {
            ++outcome.fixtures_modified;
            outcome.assignments_written += before - draft.look.assignments.size();
        }
    }
    sort_assignments(draft.look, project);
    outcome.result = outcome.assignments_written == 0U
        ? StaticLookAuthoringResult::NoChange
        : StaticLookAuthoringResult::Applied;
    return outcome;
}

StudioMutationOutcome commit_static_look_draft(
    StudioDocumentService& document,
    StaticLookDraft& draft) {
    const auto snapshot = document.snapshot();
    if (draft.base_generation != snapshot.generation) {
        // Let the document authority produce the canonical stale-generation
        // outcome before inspecting indices that may have shifted meanwhile.
        return document.apply_candidate(
            draft.base_generation, snapshot.document);
    }
    auto invalid = [&](std::string message) {
        return StudioMutationOutcome{
            StudioMutationResult::InvalidCandidate,
            snapshot.generation,
            validate_project(snapshot.document),
            std::move(message)};
    };
    if (draft.look.id.empty()) {
        return invalid("A Static Look draft needs a stable ID before commit.");
    }
    auto candidate = snapshot.document;
    if (draft.source_index.has_value()) {
        if (*draft.source_index >= candidate.looks.size() ||
            candidate.looks[*draft.source_index].id != draft.look.id) {
            return invalid("The Static Look source no longer matches this draft.");
        }
        candidate.looks[*draft.source_index] = draft.look;
    } else {
        const auto duplicate = std::any_of(
            candidate.looks.begin(), candidate.looks.end(),
            [&](const auto& look) { return look.id == draft.look.id; });
        if (duplicate) {
            return invalid("A Static Look already uses this stable ID.");
        }
        candidate.looks.push_back(draft.look);
    }
    auto outcome = document.apply_candidate(draft.base_generation, std::move(candidate));
    if (outcome) {
        if (!draft.source_index.has_value()) {
            draft.source_index = snapshot.document.looks.size();
        }
        draft.base_generation = outcome.generation;
    }
    return outcome;
}

StaticLookDependencyReport inspect_static_look_dependencies(
    const ProjectDocument& project,
    std::string_view look_id) {
    StaticLookDependencyReport result;
    for (const auto& loop : project.autoloops) {
        const auto count = static_cast<std::size_t>(std::count_if(
            loop.steps.begin(), loop.steps.end(),
            [look_id](const auto& step) { return step.look_id == look_id; }));
        if (count != 0U) {
            result.autoloop_steps += count;
            result.dependents.push_back("Autoloop: " + loop.name);
        }
    }
    for (const auto& track : project.track_scripts) {
        const auto count = static_cast<std::size_t>(std::count_if(
            track.cues.begin(), track.cues.end(),
            [look_id](const auto& cue) {
                return cue.action == TrackCueAction::TriggerLook &&
                    cue.target_ref == look_id;
            }));
        if (count != 0U) {
            result.track_cues += count;
            result.dependents.push_back("Track script: " + track.name);
        }
    }
    for (const auto& mapping : project.midi_mappings) {
        if (mapping.action.type == showcore::ActionType::TriggerLook &&
            mapping.target_ref == look_id) {
            ++result.midi_bindings;
            result.dependents.push_back("MIDI binding: " + mapping.device_name);
        }
    }
    return result;
}

bool parse_rgb_hex(std::string_view text, StaticLookColor& color) noexcept {
    if (!text.empty() && text.front() == '#') {
        text.remove_prefix(1U);
    }
    if (text.size() != 6U) {
        return false;
    }
    bool ok = true;
    const auto red = decode_hex_pair(text[0], text[1], ok);
    const auto green = decode_hex_pair(text[2], text[3], ok);
    const auto blue = decode_hex_pair(text[4], text[5], ok);
    if (!ok) {
        return false;
    }
    color.red = static_cast<float>(red) / 255.0F;
    color.green = static_cast<float>(green) / 255.0F;
    color.blue = static_cast<float>(blue) / 255.0F;
    return true;
}

std::string format_rgb_hex(const StaticLookColor& color) {
    if (!normalized(color.red) || !normalized(color.green) ||
        !normalized(color.blue)) {
        return {};
    }
    const auto byte = [](float value) {
        return static_cast<unsigned int>(std::lround(value * 255.0F));
    };
    std::ostringstream output;
    output << '#' << std::uppercase << std::hex << std::setfill('0')
           << std::setw(2) << byte(color.red)
           << std::setw(2) << byte(color.green)
           << std::setw(2) << byte(color.blue);
    return output.str();
}

std::span<const StaticLookSwatch> built_in_static_look_swatches() noexcept {
    return kBuiltInSwatches;
}

}  // namespace emberlights
