#include "emberlights/fixture_control_action.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>

namespace emberlights {
namespace {

using JsonValue = EmberActionJsonValue;
using JsonArray = EmberActionJsonValue::Array;
using JsonObject = EmberActionJsonValue::Object;

constexpr float kChoiceValueTolerance = 0.000001F;

[[nodiscard]] JsonValue json_null() {
    JsonValue value;
    value.storage = nullptr;
    return value;
}

[[nodiscard]] JsonValue json_boolean(bool input) {
    JsonValue value;
    value.storage = input;
    return value;
}

[[nodiscard]] JsonValue json_number(double input) {
    JsonValue value;
    value.storage = EmberActionJsonNumber{input};
    return value;
}

[[nodiscard]] JsonValue json_string(std::string input) {
    JsonValue value;
    value.storage = std::move(input);
    return value;
}

[[nodiscard]] JsonValue json_array(JsonArray input) {
    JsonValue value;
    value.storage = std::move(input);
    return value;
}

[[nodiscard]] JsonValue json_object(JsonObject input) {
    JsonValue value;
    value.storage = std::move(input);
    return value;
}

[[nodiscard]] bool same_float(float first, float second) noexcept {
    return std::isfinite(first) && std::isfinite(second) &&
        std::fabs(first - second) <= kChoiceValueTolerance;
}

[[nodiscard]] bool same_choice_value(
    const FixtureControlChoiceValue& first,
    const FixtureControlChoiceValue& second) noexcept {
    return first.fixture_id == second.fixture_id &&
        first.profile_id == second.profile_id &&
        first.binding_id == second.binding_id &&
        first.channel == second.channel &&
        first.property == second.property &&
        same_float(first.normalized_value, second.normalized_value) &&
        first.raw_value == second.raw_value &&
        first.dmx_min == second.dmx_min &&
        first.dmx_max == second.dmx_max;
}

[[nodiscard]] bool same_choice(
    const FixtureControlChoice& first,
    const FixtureControlChoice& second) noexcept {
    return first.id == second.id &&
        first.capability_id == second.capability_id &&
        first.name == second.name &&
        first.owner == second.owner &&
        first.property == second.property &&
        first.behavior == second.behavior &&
        first.access == second.access &&
        first.role == second.role &&
        first.supported_fixture_count == second.supported_fixture_count &&
        first.target_fixture_count == second.target_fixture_count &&
        same_float(
            first.shared_normalized_value,
            second.shared_normalized_value) &&
        first.shared_value == second.shared_value &&
        first.values.size() == second.values.size() &&
        std::equal(
            first.values.begin(), first.values.end(),
            second.values.begin(), same_choice_value);
}

[[nodiscard]] FixtureControlActionPlan fail_plan(
    FixtureControlActionPlan plan,
    FixtureControlActionError error,
    std::string message) {
    plan.error = error;
    plan.message = std::move(message);
    plan.canonical_source.clear();
    plan.content_hash.clear();
    plan.prepared.reset();
    plan.foundation.reset();
    plan.executable.reset();
    return plan;
}

[[nodiscard]] JsonValue choice_identity(
    std::string_view target_id,
    bool group,
    const FixtureControlChoice& choice) {
    JsonArray values;
    values.reserve(choice.values.size());
    for (const auto& value : choice.values) {
        JsonObject item;
        item.emplace("bindingId", json_string(value.binding_id));
        item.emplace("channel", json_number(value.channel));
        item.emplace("dmxMaximum", json_number(value.dmx_max));
        item.emplace("dmxMinimum", json_number(value.dmx_min));
        item.emplace("fixtureId", json_string(value.fixture_id));
        item.emplace("normalizedValue", json_number(value.normalized_value));
        item.emplace("profileId", json_string(value.profile_id));
        item.emplace("rawValue", json_number(value.raw_value));
        values.push_back(json_object(std::move(item)));
    }

    JsonObject identity;
    identity.emplace("behavior", json_number(
        static_cast<unsigned int>(choice.behavior)));
    identity.emplace("choiceId", json_string(choice.id));
    identity.emplace("group", json_boolean(group));
    identity.emplace("plannerVersion", json_number(1.0));
    identity.emplace("property", json_string(
        std::string(property_name(choice.property))));
    identity.emplace("targetId", json_string(std::string(target_id)));
    identity.emplace("values", json_array(std::move(values)));
    return json_object(std::move(identity));
}

[[nodiscard]] JsonValue parameter_reference(std::string name) {
    JsonObject value;
    value.emplace("path", json_string(std::move(name)));
    value.emplace("source", json_string("parameter"));
    return json_object(std::move(value));
}

[[nodiscard]] JsonValue literal_number(double number) {
    JsonObject value;
    value.emplace("literal", json_number(number));
    return json_object(std::move(value));
}

[[nodiscard]] JsonValue target_parameter(
    std::string_view target_id,
    std::string_view target_kind) {
    JsonObject type;
    type.emplace("targetKind", json_string(std::string(target_kind)));
    type.emplace("type", json_string("stableId"));

    JsonObject parameter;
    parameter.emplace("default", json_string(std::string(target_id)));
    parameter.emplace("required", json_boolean(true));
    parameter.emplace("valueType", json_object(std::move(type)));
    return json_object(std::move(parameter));
}

[[nodiscard]] JsonValue property_parameter(std::string_view property_id) {
    JsonArray values;
    values.push_back(json_string(std::string(property_id)));

    JsonObject type;
    type.emplace("schemaRef", json_string("value.fixtureProperty"));
    type.emplace("type", json_string("enum"));
    type.emplace("values", json_array(std::move(values)));

    JsonObject parameter;
    parameter.emplace("default", json_string(std::string(property_id)));
    parameter.emplace("required", json_boolean(true));
    parameter.emplace("valueType", json_object(std::move(type)));
    return json_object(std::move(parameter));
}

[[nodiscard]] JsonValue build_action_source(
    const FixtureControlActionPlan& plan,
    const FixtureControlChoice& choice,
    std::string_view identity_hash,
    std::string_view set_command,
    std::string_view release_command) {
    JsonObject compatibility;
    compatibility.emplace("capabilityRegistry", json_string(">=1 <3"));
    compatibility.emplace("commandRegistry", json_string(">=1 <3"));
    compatibility.emplace("minimumAppVersion", json_string("0.1.0"));
    compatibility.emplace("stateRegistry", json_string(">=1 <3"));

    JsonObject provenance;
    provenance.emplace("kind", json_string("local"));
    provenance.emplace("migrationAdapter", json_null());
    provenance.emplace("migrationStatus", json_string("native"));
    provenance.emplace(
        "sourceHash", json_string(std::string(identity_hash)));
    provenance.emplace(
        "sourceId", json_string("fixture-control:" +
            std::string(identity_hash.substr(7U))));
    provenance.emplace("sourceVersion", json_string("1"));

    JsonObject parameters;
    parameters.emplace(
        plan.target_parameter_name,
        target_parameter(plan.target_id, plan.group ? "fixtureGroup" : "fixture"));
    parameters.emplace(
        plan.property_parameter_name,
        property_parameter(plan.property_id));

    JsonArray commands;
    commands.push_back(json_string(std::string(release_command)));
    commands.push_back(json_string(std::string(set_command)));
    JsonObject requirements;
    requirements.emplace("capabilities", json_array({}));
    requirements.emplace("commands", json_array(std::move(commands)));
    requirements.emplace("states", json_array({}));

    JsonObject set_arguments;
    set_arguments.emplace(
        plan.target_parameter_name,
        parameter_reference(plan.target_parameter_name));
    set_arguments.emplace(
        plan.property_parameter_name,
        parameter_reference(plan.property_parameter_name));
    set_arguments.emplace(
        "value", literal_number(plan.normalized_value));
    JsonObject set_node;
    set_node.emplace("arguments", json_object(std::move(set_arguments)));
    set_node.emplace("commandId", json_string(std::string(set_command)));
    set_node.emplace("kind", json_string("InvokeCommand"));
    set_node.emplace("resultAs", json_string("setResult"));

    JsonObject release_arguments;
    release_arguments.emplace(
        plan.target_parameter_name,
        parameter_reference(plan.target_parameter_name));
    release_arguments.emplace(
        plan.property_parameter_name,
        parameter_reference(plan.property_parameter_name));
    JsonObject release_node;
    release_node.emplace(
        "arguments", json_object(std::move(release_arguments)));
    release_node.emplace(
        "commandId", json_string(std::string(release_command)));
    release_node.emplace("kind", json_string("InvokeCommand"));
    release_node.emplace("resultAs", json_string("releaseResult"));

    JsonObject nodes;
    nodes.emplace("node.release", json_object(std::move(release_node)));
    nodes.emplace("node.set", json_object(std::move(set_node)));

    JsonObject entry_points;
    entry_points.emplace("onRelease", json_string("node.release"));
    entry_points.emplace(
        choice.behavior == showcore::ChannelCapabilityBehavior::Continuous
            ? "onValue" : "onPress",
        json_string("node.set"));

    JsonObject root;
    root.emplace("author", json_string("EmberLights"));
    root.emplace("compatibility", json_object(std::move(compatibility)));
    root.emplace(
        "description",
        json_string("Generated exact Fixture Attribute Live set/release action."));
    root.emplace("entryPoints", json_object(std::move(entry_points)));
    root.emplace("feedback", json_object({}));
    root.emplace("id", json_string(plan.action_id));
    root.emplace(
        "label",
        json_string(choice.name.empty() ? plan.property_id : choice.name));
    root.emplace("nodes", json_object(std::move(nodes)));
    root.emplace("parameters", json_object(std::move(parameters)));
    root.emplace("provenance", json_object(std::move(provenance)));
    root.emplace("requires", json_object(std::move(requirements)));
    root.emplace("schemaVersion", json_number(1.0));
    root.emplace("version", json_string("1.0.0"));
    return json_object(std::move(root));
}

}  // namespace

FixtureControlActionPlan plan_fixture_control_action(
    const ProjectDocument& project,
    std::string_view target_id,
    const FixtureControlChoice& choice,
    const GeneratedUiRegistryEmberActionView& registry,
    FixtureControlActionOptions options) {
    FixtureControlActionPlan plan;
    plan.target_id = std::string(target_id);
    plan.property_parameter_name = "property";
    if (target_id.empty() || choice.id.empty() ||
        choice.property >= showcore::Property::Count ||
        !std::isfinite(options.catalog_position) ||
        options.catalog_position < 0.0F || options.catalog_position > 1.0F) {
        return fail_plan(
            std::move(plan), FixtureControlActionError::InvalidSelection,
            "The Fixture Attribute Action selection is incomplete or outside its bounded position.");
    }
    if (choice.access == showcore::ChannelCapabilityAccess::Protected) {
        return fail_plan(
            std::move(plan), FixtureControlActionError::ProtectedChoice,
            "Protected reset/service/custom functions cannot become Ember Actions.");
    }

    const auto catalog = fixture_control_choices(
        project, target_id, options.catalog_position);
    plan.target_found = catalog.target_found;
    plan.group = catalog.group;
    plan.warnings = catalog.warnings;
    if (!catalog.target_found) {
        return fail_plan(
            std::move(plan), FixtureControlActionError::TargetNotFound,
            "The fixture or group target no longer exists.");
    }
    if (catalog.target_fixture_count == 0U) {
        return fail_plan(
            std::move(plan), FixtureControlActionError::TargetEmpty,
            "The target has no patched fixtures.");
    }

    const auto selected = std::find_if(
        catalog.choices.begin(), catalog.choices.end(),
        [&choice](const auto& candidate) {
            return candidate.id == choice.id;
        });
    if (selected == catalog.choices.end()) {
        return fail_plan(
            std::move(plan), FixtureControlActionError::ChoiceNotFound,
            "The Fixture Attribute is unavailable for this target.");
    }
    if (!same_choice(choice, *selected)) {
        return fail_plan(
            std::move(plan), FixtureControlActionError::StaleChoice,
            "The fixture profile or target changed after this attribute was selected.");
    }
    if (selected->safety_gated() && !options.allow_safety_gated) {
        return fail_plan(
            std::move(plan), FixtureControlActionError::SafetyGateRequired,
            "This function requires explicit safety-gated Action authoring confirmation.");
    }

    const auto valid_semantic_set = selected->common() &&
        selected->shared_value && !selected->values.empty() &&
        selected->values.size() == catalog.target_fixture_count &&
        std::isfinite(selected->shared_normalized_value) &&
        selected->shared_normalized_value >= 0.0F &&
        selected->shared_normalized_value <= 1.0F &&
        std::all_of(
            selected->values.begin(), selected->values.end(),
            [&selected](const auto& value) {
                return !value.fixture_id.empty() && !value.profile_id.empty() &&
                    value.property == selected->property &&
                    same_float(
                        value.normalized_value,
                        selected->shared_normalized_value);
            });
    if (!catalog.group) {
        if (!valid_semantic_set || selected->values.size() != 1U ||
            selected->values.front().fixture_id != target_id) {
            return fail_plan(
                std::move(plan),
                FixtureControlActionError::FixtureNotLiveCompatible,
                "The Fixture Attribute does not resolve to one exact Live semantic value.");
        }
    } else {
        const auto& first_profile = selected->values.front().profile_id;
        const auto homogeneous_profile = std::all_of(
            selected->values.begin(), selected->values.end(),
            [&first_profile](const auto& value) {
                return value.profile_id == first_profile;
            });
        if (!valid_semantic_set || !selected->live_override_compatible() ||
            !homogeneous_profile) {
            return fail_plan(
                std::move(plan),
                FixtureControlActionError::GroupNotLiveCompatible,
                "Only a complete single-profile group with one shared semantic value can become one atomic Live Action.");
        }
    }

    plan.normalized_value = selected->shared_normalized_value;
    plan.property_id = std::string(property_name(selected->property));
    plan.target_parameter_name = catalog.group ? "groupId" : "fixtureId";
    plan.continuous_fixed_position = selected->behavior ==
        showcore::ChannelCapabilityBehavior::Continuous;
    plan.release_on_release = true;
    plan.release_on_deactivate = false;

    const auto identity = canonicalize_ember_action_source(
        choice_identity(target_id, catalog.group, *selected));
    plan.action_id = "com.emberlights.action.fixture-control." +
        identity.content_hash.substr(7U);
    const auto set_command = catalog.group
        ? std::string_view{"group.override.property.set"}
        : std::string_view{"fixture.override.property.set"};
    const auto release_command = catalog.group
        ? std::string_view{"group.override.property.release"}
        : std::string_view{"fixture.override.property.release"};
    const auto source = build_action_source(
        plan, *selected, identity.content_hash, set_command, release_command);
    plan.canonical_source =
        canonicalize_ember_action_source(source).normalized_json;

    auto prepared = prepare_ember_action_source(
        plan.canonical_source, registry);
    if (!prepared.ok()) {
        plan.diagnostics = std::move(prepared.diagnostics);
        return fail_plan(
            std::move(plan), FixtureControlActionError::CompilationFailed,
            "The generated Fixture Attribute source failed Ember Action preparation.");
    }
    plan.prepared = std::move(prepared.prepared);
    plan.content_hash = plan.prepared->content_hash;

    auto foundation = compile_ember_action_ir_foundation(
        plan.prepared, registry);
    if (!foundation.ok()) {
        plan.diagnostics = std::move(foundation.diagnostics);
        return fail_plan(
            std::move(plan), FixtureControlActionError::CompilationFailed,
            "The generated Fixture Attribute source failed native Action resolution.");
    }
    plan.foundation = std::move(foundation.ir);

    auto executable = compile_ember_action_executable_ir(
        plan.foundation, registry);
    if (!executable.ok()) {
        plan.diagnostics = std::move(executable.diagnostics);
        return fail_plan(
            std::move(plan), FixtureControlActionError::CompilationFailed,
            "The generated Fixture Attribute source is outside the executable Action subset.");
    }
    plan.executable = std::move(executable.ir);

    if (selected->safety_gated()) {
        plan.warnings.push_back(
            "Authoring confirmation does not bypass the registered command's core fixture-property safety gate.");
    }
    if (plan.continuous_fixed_position) {
        plan.warnings.push_back(
            "Executable Ember Actions do not yet support MapValue; this continuous range is an exact fixed-position onValue Action.");
    }
    plan.warnings.push_back(
        "The executable Action subset does not support onDeactivate; release remains available through onRelease.");
    plan.error = FixtureControlActionError::None;
    plan.message = plan.continuous_fixed_position
        ? "Prepared one exact fixed-position Live Action with release."
        : "Prepared one exact named-slot Live Action with release.";
    return plan;
}

std::vector<EmberActionRuntimeValue>
fixture_control_action_runtime_parameters(
    const FixtureControlActionPlan& plan) {
    std::vector<EmberActionRuntimeValue> values;
    if (!plan || plan.executable == nullptr) {
        return values;
    }
    values.reserve(plan.executable->parameters.size());
    for (const auto& parameter : plan.executable->parameters) {
        if (parameter.name == plan.target_parameter_name) {
            values.push_back(EmberActionRuntimeValue::text_value(
                EmberActionRuntimeValueKind::StableId, plan.target_id));
        } else if (parameter.name == plan.property_parameter_name) {
            values.push_back(EmberActionRuntimeValue::text_value(
                EmberActionRuntimeValueKind::Enum, plan.property_id));
        } else {
            values.clear();
            return values;
        }
    }
    return values;
}

const char* fixture_control_action_error_name(
    FixtureControlActionError error) noexcept {
    switch (error) {
    case FixtureControlActionError::None: return "none";
    case FixtureControlActionError::InvalidSelection: return "invalidSelection";
    case FixtureControlActionError::TargetNotFound: return "targetNotFound";
    case FixtureControlActionError::TargetEmpty: return "targetEmpty";
    case FixtureControlActionError::ChoiceNotFound: return "choiceNotFound";
    case FixtureControlActionError::StaleChoice: return "staleChoice";
    case FixtureControlActionError::ProtectedChoice: return "protectedChoice";
    case FixtureControlActionError::SafetyGateRequired: return "safetyGateRequired";
    case FixtureControlActionError::FixtureNotLiveCompatible:
        return "fixtureNotLiveCompatible";
    case FixtureControlActionError::GroupNotLiveCompatible:
        return "groupNotLiveCompatible";
    case FixtureControlActionError::CompilationFailed: return "compilationFailed";
    }
    return "unknown";
}

}  // namespace emberlights
