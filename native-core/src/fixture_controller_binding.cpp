#include "emberlights/fixture_controller_binding.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>

namespace emberlights {
namespace {

constexpr float kSemanticValueTolerance = 0.000001F;

[[nodiscard]] const FixtureControlChoice* find_choice(
    const FixtureControlChoiceCatalog& catalog,
    std::string_view choice_id) noexcept {
    const auto found = std::find_if(
        catalog.choices.begin(), catalog.choices.end(),
        [choice_id](const auto& choice) { return choice.id == choice_id; });
    return found == catalog.choices.end() ? nullptr : &*found;
}

[[nodiscard]] const FixtureControlChoiceValue* find_value(
    const FixtureControlChoice& choice,
    std::string_view fixture_id) noexcept {
    const auto found = std::find_if(
        choice.values.begin(), choice.values.end(),
        [fixture_id](const auto& value) {
            return value.fixture_id == fixture_id;
        });
    return found == choice.values.end() ? nullptr : &*found;
}

[[nodiscard]] bool same_semantic_value(float first, float second) noexcept {
    return std::isfinite(first) && std::isfinite(second) &&
        std::fabs(first - second) <= kSemanticValueTolerance;
}

[[nodiscard]] bool midi_message_types_overlap(
    showcore::MidiMessageType first,
    showcore::MidiMessageType second) noexcept {
    if (first == second) {
        return true;
    }
    return (first == showcore::MidiMessageType::NoteOn &&
            second == showcore::MidiMessageType::NoteOff) ||
        (first == showcore::MidiMessageType::NoteOff &&
         second == showcore::MidiMessageType::NoteOn);
}

[[nodiscard]] bool midi_channels_overlap(
    std::uint8_t first,
    std::uint8_t second) noexcept {
    return first == showcore::kAnyMidiChannel ||
        second == showcore::kAnyMidiChannel || first == second;
}

[[nodiscard]] std::size_t existing_gesture_fanout(
    const ProjectDocument& project,
    const MidiMappingDefinition& prototype) noexcept {
    return static_cast<std::size_t>(std::count_if(
        project.midi_mappings.begin(), project.midi_mappings.end(),
        [&prototype](const auto& mapping) {
            return mapping.number == prototype.number &&
                midi_message_types_overlap(
                    mapping.message_type, prototype.message_type) &&
                midi_channels_overlap(mapping.channel, prototype.channel);
        }));
}

[[nodiscard]] MidiMappingDefinition planned_mapping(
    const MidiMappingDefinition& prototype,
    std::string_view target_ref,
    showcore::ActionType action_type,
    showcore::Property property,
    float output_min,
    float output_max,
    std::string_view binding_id) {
    auto mapping = prototype;
    mapping.target_ref = target_ref;
    mapping.action.type = action_type;
    mapping.action.property = property;
    mapping.action.target_id = 0U;
    mapping.output_min = output_min;
    mapping.output_max = output_max;
    mapping.fixture_control_binding_id = binding_id;
    if (same_semantic_value(output_min, output_max)) {
        // A fixed slot has no pickup trajectory. Leaving soft takeover enabled
        // would make the binding permanently unreachable when the current
        // value differs from the slot value.
        mapping.soft_takeover = false;
    }
    return mapping;
}

[[nodiscard]] FixtureControllerBindingPlan failed_plan(
    FixtureControllerBindingPlan plan,
    FixtureControllerBindingError error,
    std::string message) {
    plan.error = error;
    plan.mappings.clear();
    plan.message = std::move(message);
    return plan;
}

}  // namespace

FixtureControllerBindingPlan plan_fixture_controller_binding(
    const ProjectDocument& project,
    std::string_view target_id,
    std::string_view fixture_control_choice_id,
    const MidiMappingDefinition& prototype,
    FixtureControllerBindingOptions options) {
    FixtureControllerBindingPlan plan;
    const auto middle_catalog = fixture_control_choices(project, target_id, 0.5F);
    plan.target_found = middle_catalog.target_found;
    plan.group = middle_catalog.group;
    plan.warnings = middle_catalog.warnings;
    if (!middle_catalog.target_found) {
        return failed_plan(
            std::move(plan), FixtureControllerBindingError::TargetNotFound,
            "The fixture or group target no longer exists.");
    }
    if (middle_catalog.target_fixture_count == 0U) {
        return failed_plan(
            std::move(plan), FixtureControllerBindingError::TargetEmpty,
            "The selected target has no patched fixtures.");
    }
    if (prototype.input_mode ==
            showcore::MidiInputMode::RelativeTwosComplement ||
        prototype.behavior == showcore::MappingBehavior::Relative) {
        return failed_plan(
            std::move(plan), FixtureControllerBindingError::UnsupportedPrototype,
            "Relative controller input cannot remain inside one named fixture-function range.");
    }

    const auto* middle = find_choice(
        middle_catalog, fixture_control_choice_id);
    if (middle == nullptr) {
        return failed_plan(
            std::move(plan), FixtureControllerBindingError::ChoiceNotFound,
            "The named fixture function is unavailable for this target.");
    }
    if (middle->safety_gated() && !options.allow_safety_gated) {
        return failed_plan(
            std::move(plan), FixtureControllerBindingError::SafetyGateRequired,
            "This named fixture function requires an explicit safety-gated authoring confirmation.");
    }

    const FixtureControlChoice* low = middle;
    const FixtureControlChoice* high = middle;
    FixtureControlChoiceCatalog low_catalog;
    FixtureControlChoiceCatalog high_catalog;
    if (middle->behavior ==
        showcore::ChannelCapabilityBehavior::Continuous) {
        low_catalog = fixture_control_choices(project, target_id, 0.0F);
        high_catalog = fixture_control_choices(project, target_id, 1.0F);
        low = find_choice(low_catalog, fixture_control_choice_id);
        high = find_choice(high_catalog, fixture_control_choice_id);
        if (low == nullptr || high == nullptr ||
            low->property != middle->property ||
            high->property != middle->property ||
            low->access != middle->access || high->access != middle->access) {
            return failed_plan(
                std::move(plan),
                FixtureControllerBindingError::InconsistentChoice,
                "The named fixture function changed while resolving its semantic endpoints.");
        }
    }

    const auto homogeneous_group = plan.group && middle->common() &&
        low->common() && high->common() && low->shared_value &&
        high->shared_value &&
        low->values.size() == middle_catalog.target_fixture_count &&
        high->values.size() == middle_catalog.target_fixture_count;

    if (!plan.group || homogeneous_group) {
        const auto output_min = low->shared_normalized_value;
        const auto output_max = high->shared_normalized_value;
        if (!std::isfinite(output_min) || !std::isfinite(output_max)) {
            return failed_plan(
                std::move(plan),
                FixtureControllerBindingError::InconsistentChoice,
                "The named fixture function produced an invalid semantic range.");
        }
        plan.mappings.push_back(planned_mapping(
            prototype,
            target_id,
            plan.group ? showcore::ActionType::SetGroupProperty
                       : showcore::ActionType::SetProperty,
            middle->property,
            output_min,
            output_max,
            middle->id));
    } else {
        plan.expanded_to_fixtures = true;
        plan.mappings.reserve(middle->values.size());
        for (const auto& value : middle->values) {
            const auto* low_value = find_value(*low, value.fixture_id);
            const auto* high_value = find_value(*high, value.fixture_id);
            if (low_value == nullptr || high_value == nullptr ||
                low_value->property != middle->property ||
                high_value->property != middle->property ||
                !std::isfinite(low_value->normalized_value) ||
                !std::isfinite(high_value->normalized_value)) {
                return failed_plan(
                    std::move(plan),
                    FixtureControllerBindingError::InconsistentChoice,
                    "A mixed-profile group could not resolve the named function for every supported fixture.");
            }
            plan.mappings.push_back(planned_mapping(
                prototype,
                value.fixture_id,
                showcore::ActionType::SetProperty,
                middle->property,
                low_value->normalized_value,
                high_value->normalized_value,
                middle->id));
        }
        if (middle->partial()) {
            plan.warnings.push_back(
                "The named fixture function is unsupported by part of the group; only exact supporting fixtures were planned.");
        }
    }

    if (plan.mappings.empty()) {
        return failed_plan(
            std::move(plan), FixtureControllerBindingError::InconsistentChoice,
            "The named fixture function produced no exact controller mappings.");
    }
    if (project.midi_mappings.size() + plan.mappings.size() >
        showcore::kMaxMidiMappings) {
        return failed_plan(
            std::move(plan),
            FixtureControllerBindingError::ProjectCapacityExceeded,
            "Adding the complete named-function binding would exceed project MIDI mapping capacity.");
    }
    if (existing_gesture_fanout(project, prototype) + plan.mappings.size() >
        showcore::kMaxMidiActionsPerMessage) {
        return failed_plan(
            std::move(plan),
            FixtureControllerBindingError::GestureFanoutExceeded,
            "The complete named-function binding would exceed the bounded actions-per-message limit.");
    }

    plan.message = plan.expanded_to_fixtures
        ? "The mixed-profile group was planned as exact per-fixture semantic mappings."
        : "The named fixture function was planned through one semantic controller mapping.";
    return plan;
}

}  // namespace emberlights
