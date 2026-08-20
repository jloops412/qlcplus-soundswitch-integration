#include "emberlights/ember_action_compiler.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace emberlights {
namespace {

using JsonValue = EmberActionJsonValue;
using JsonObject = EmberActionJsonValue::Object;
using JsonArray = EmberActionJsonValue::Array;
using StringSet = std::set<std::string, EmberActionUtf8ByteLess>;

[[nodiscard]] const JsonObject* object_at(
    const JsonObject& object,
    std::string_view key) noexcept {
    const auto* value = ember_action_object_find(object, key);
    return value == nullptr ? nullptr : value->as_object();
}

[[nodiscard]] const JsonArray* array_at(
    const JsonObject& object,
    std::string_view key) noexcept {
    const auto* value = ember_action_object_find(object, key);
    return value == nullptr ? nullptr : value->as_array();
}

[[nodiscard]] const std::string* string_at(
    const JsonObject& object,
    std::string_view key) noexcept {
    const auto* value = ember_action_object_find(object, key);
    return value == nullptr ? nullptr : value->as_string();
}

[[nodiscard]] const EmberActionJsonNumber* number_at(
    const JsonObject& object,
    std::string_view key) noexcept {
    const auto* value = ember_action_object_find(object, key);
    return value == nullptr ? nullptr : value->as_number();
}

[[nodiscard]] bool integral(double value) noexcept {
    return std::isfinite(value) && std::floor(value) == value;
}

[[nodiscard]] std::optional<std::size_t> size_at(
    const JsonObject& object,
    std::string_view key) noexcept {
    const auto* number = number_at(object, key);
    if (number == nullptr || !integral(number->value) || number->value < 0.0 ||
        number->value > static_cast<double>(std::numeric_limits<std::size_t>::max())) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(number->value);
}

[[nodiscard]] EmberActionValueKind value_kind(std::string_view name) noexcept {
    if (name == "boolean") return EmberActionValueKind::Boolean;
    if (name == "integer") return EmberActionValueKind::Integer;
    if (name == "number") return EmberActionValueKind::Number;
    if (name == "enum") return EmberActionValueKind::Enum;
    if (name == "string") return EmberActionValueKind::String;
    if (name == "stableId") return EmberActionValueKind::StableId;
    if (name == "semanticRole") return EmberActionValueKind::SemanticRole;
    if (name == "color") return EmberActionValueKind::Color;
    if (name == "duration") return EmberActionValueKind::Duration;
    if (name == "object") return EmberActionValueKind::Object;
    if (name == "list") return EmberActionValueKind::List;
    if (name == "result") return EmberActionValueKind::Result;
    if (name == "void") return EmberActionValueKind::Void;
    return EmberActionValueKind::Unknown;
}

[[nodiscard]] EmberActionValueContract value_contract(const JsonObject& object) {
    EmberActionValueContract result;
    if (const auto* type = string_at(object, "type")) result.kind = value_kind(*type);
    if (const auto* unit = string_at(object, "unit")) result.unit = *unit;
    if (const auto* minimum = number_at(object, "minimum")) result.minimum = minimum->value;
    if (const auto* maximum = number_at(object, "maximum")) result.maximum = maximum->value;
    if (const auto* target = string_at(object, "targetKind")) result.target_kind = *target;
    if (const auto* schema = string_at(object, "schemaRef")) result.schema_ref = *schema;
    if (const auto count = size_at(object, "maxItems")) result.maximum_items = *count;
    if (const auto count = size_at(object, "maxLength")) result.maximum_string_bytes = *count;
    return result;
}

[[nodiscard]] EmberActionValueContract literal_contract(const JsonValue& value) {
    EmberActionValueContract result;
    if (value.is_null()) {
        result.kind = EmberActionValueKind::Null;
    } else if (value.as_boolean() != nullptr) {
        result.kind = EmberActionValueKind::Boolean;
    } else if (const auto* number = value.as_number()) {
        result.kind = integral(number->value)
            ? EmberActionValueKind::Integer
            : EmberActionValueKind::Number;
        result.minimum = number->value;
        result.maximum = number->value;
    } else if (const auto* text = value.as_string()) {
        result.kind = EmberActionValueKind::String;
        result.maximum_string_bytes = text->size();
    } else if (const auto* array = value.as_array()) {
        result.kind = EmberActionValueKind::List;
        result.maximum_items = array->size();
    } else if (value.as_object() != nullptr) {
        result.kind = EmberActionValueKind::Object;
    }
    return result;
}

[[nodiscard]] bool compatible_type_and_unit(
    const EmberActionValueContract& actual,
    const EmberActionValueContract& expected) noexcept {
    const auto kind_matches = actual.kind == expected.kind ||
        (actual.kind == EmberActionValueKind::Integer &&
         expected.kind == EmberActionValueKind::Number) ||
        actual.kind == EmberActionValueKind::Unknown;
    return kind_matches &&
        (actual.unit.empty() || expected.unit.empty() || actual.unit == expected.unit) &&
        (actual.target_kind.empty() || expected.target_kind.empty() ||
         actual.target_kind == expected.target_kind) &&
        (actual.schema_ref.empty() || expected.schema_ref.empty() ||
         actual.schema_ref == expected.schema_ref);
}

[[nodiscard]] bool range_within(
    const EmberActionValueContract& actual,
    const EmberActionValueContract& expected) noexcept {
    if (expected.minimum.has_value() &&
        (!actual.minimum.has_value() || *actual.minimum < *expected.minimum)) {
        return false;
    }
    if (expected.maximum.has_value() &&
        (!actual.maximum.has_value() || *actual.maximum > *expected.maximum)) {
        return false;
    }
    return true;
}

[[nodiscard]] EmberActionValueContract result_contract() {
    EmberActionValueContract result;
    result.kind = EmberActionValueKind::Result;
    return result;
}

[[nodiscard]] bool stable_id(std::string_view id) noexcept {
    if (id.size() < 3U || id.size() > 128U ||
        !std::isalnum(static_cast<unsigned char>(id.front()))) {
        return false;
    }
    return std::all_of(id.begin(), id.end(), [](char character) {
        const auto byte = static_cast<unsigned char>(character);
        return std::isalnum(byte) != 0 || character == '.' || character == '_' ||
            character == ':' || character == '-';
    });
}

[[nodiscard]] bool portable_path(std::string_view path) noexcept {
    const auto drive = path.size() >= 2U &&
        std::isalpha(static_cast<unsigned char>(path[0])) != 0 && path[1] == ':';
    return !path.empty() && path.front() != '/' && path.front() != '\\' && !drive &&
        path.find("../") == std::string_view::npos &&
        path.find("..\\") == std::string_view::npos &&
        path.find('\\') == std::string_view::npos;
}

[[nodiscard]] EmberActionRealtimeClass stronger(
    EmberActionRealtimeClass first,
    EmberActionRealtimeClass second) noexcept {
    return static_cast<int>(first) >= static_cast<int>(second) ? first : second;
}

class Validator {
public:
    Validator(
        const EmberActionRegistryView& registry,
        EmberActionPlatformLimits limits,
        std::shared_ptr<JsonValue> root,
        EmberActionCanonicalSource canonical)
        : registry_(registry), limits_(limits), effective_(limits),
          root_(std::move(root)), canonical_(std::move(canonical)) {}

    [[nodiscard]] EmberActionCompileResult run() {
        EmberActionCompileResult result;
        root_object_ = root_->as_object();
        if (root_object_ == nullptr) {
            add("EA_SCHEMA_ROOT_OBJECT", "/", "The action root must be an object.");
        } else {
            validate_root();
        }
        if (diagnostics_.empty()) {
            validate_limits();
            collect_parameters();
            collect_requirements();
            collect_nodes_and_locals();
            validate_nodes();
            validate_entry_points();
            validate_action_dependencies();
            validate_content_hash();
        }
        if (diagnostics_.empty()) {
            auto prepared = std::make_shared<EmberActionPreparedSource>();
            prepared->source = std::shared_ptr<const JsonValue>(root_);
            prepared->normalized_json = canonical_.normalized_json;
            prepared->content_hash = canonical_.content_hash;
            prepared->dependencies.commands.assign(commands_.begin(), commands_.end());
            prepared->dependencies.states.assign(states_.begin(), states_.end());
            prepared->dependencies.capabilities.assign(capabilities_.begin(), capabilities_.end());
            prepared->dependencies.actions.assign(actions_.begin(), actions_.end());
            prepared->dependencies.registry_digest = std::string(registry_.registry_digest());
            prepared->node_count = nodes_->size();
            prepared->maximum_branch_depth = maximum_depth_;
            prepared->maximum_command_invocations = maximum_commands_;
            prepared->realtime_class = realtime_class_;
            result.prepared = std::move(prepared);
        }
        result.diagnostics = std::move(diagnostics_);
        return result;
    }

private:
    void validate_root() {
        static const StringSet allowed{
            "author", "compatibility", "contentHash", "description", "entryPoints",
            "feedback", "id", "label", "limits", "localization", "nodes",
            "parameters", "provenance", "requires", "schemaVersion", "source",
            "surfaceState", "version"};
        for (const auto& [key, ignored] : *root_object_) {
            static_cast<void>(ignored);
            if (!allowed.contains(key)) {
                add("EA_SCHEMA_UNKNOWN_PROPERTY", "/" + key,
                    "The root contains an unknown property.");
            }
        }
        for (const auto required : {
                 "schemaVersion", "id", "version", "label", "compatibility",
                 "parameters", "requires", "entryPoints", "nodes", "feedback"}) {
            if (ember_action_object_find(*root_object_, required) == nullptr) {
                add("EA_SCHEMA_REQUIRED", "/" + std::string(required),
                    "A required action property is missing.");
            }
        }
        const auto* schema = number_at(*root_object_, "schemaVersion");
        if (schema != nullptr && schema->value != 1.0) {
            add("EA_SCHEMA_FUTURE_VERSION", "/schemaVersion",
                "Only schemaVersion 1 is accepted by this compiler generation.");
        }
        const auto* id = string_at(*root_object_, "id");
        if (id != nullptr && !stable_id(*id)) {
            add("EA_SCHEMA_STABLE_ID", "/id", "The action ID is not canonical.");
        }
        if (canonical_.normalized_json.size() > limits_.maximum_normalized_bytes) {
            add("EA_LIMIT_NORMALIZED_BYTES", "/",
                "The canonical action exceeds the platform byte limit.");
        }
        validate_source_map();
    }

    void validate_source_map() {
        const auto* source = object_at(*root_object_, "source");
        const auto* source_map = source == nullptr ? nullptr : object_at(*source, "sourceMap");
        const auto* sources = source_map == nullptr ? nullptr : array_at(*source_map, "sources");
        if (sources != nullptr) {
            for (std::size_t index = 0U; index < sources->size(); ++index) {
                const auto* item = (*sources)[index].as_object();
                const auto* path = item == nullptr ? nullptr : string_at(*item, "path");
                if (path != nullptr && !portable_path(*path)) {
                    add("EA_SOURCE_PATH_PORTABLE",
                        "/source/sourceMap/sources/" + std::to_string(index) + "/path",
                        "Portable source maps reject absolute and traversal paths.");
                }
            }
        }
        const auto* ranges = source_map == nullptr ? nullptr : object_at(*source_map, "ranges");
        if (ranges != nullptr) {
            for (const auto& [id, value] : *ranges) {
                const auto* range = value.as_object();
                const auto start = range == nullptr ? std::nullopt : size_at(*range, "start");
                const auto end = range == nullptr ? std::nullopt : size_at(*range, "end");
                if (start.has_value() && end.has_value() && *end < *start) {
                    add("EA_SOURCE_RANGE_ORDER", "/source/sourceMap/ranges/" + id,
                        "A source range end cannot precede its start.");
                }
            }
        }
    }

    void validate_limits() {
        const auto* declared = object_at(*root_object_, "limits");
        if (declared == nullptr) return;
        lower(*declared, "normalizedBytes", effective_.maximum_normalized_bytes);
        lower(*declared, "nodes", effective_.maximum_nodes);
        lower(*declared, "branchDepth", effective_.maximum_branch_depth);
        lower(*declared, "referencedActions", effective_.maximum_referenced_actions);
        lower(*declared, "actionCallDepth", effective_.maximum_action_call_depth);
        lower(*declared, "stateReads", effective_.maximum_state_reads);
        lower(*declared, "commandInvocations", effective_.maximum_command_invocations);
        lower(*declared, "parameters", effective_.maximum_parameters);
        lower(*declared, "feedbackOutputs", effective_.maximum_feedback_outputs);
        lower(*declared, "surfaceStateValues", effective_.maximum_surface_state_values);
        lower(*declared, "parallelChildren", effective_.maximum_parallel_children);
        lower(*declared, "expressionOperations", effective_.maximum_expression_operations);
        lower(*declared, "expressionStackDepth", effective_.maximum_expression_stack_depth);
        if (canonical_.normalized_json.size() > effective_.maximum_normalized_bytes) {
            add("EA_LIMIT_DECLARED_NORMALIZED_BYTES", "/limits/normalizedBytes",
                "The canonical action exceeds its declared lower byte limit.");
        }
    }

    void lower(const JsonObject& object, std::string_view key, std::size_t& value) {
        if (const auto declared = size_at(object, key)) value = std::min(value, *declared);
    }

    void collect_parameters() {
        const auto* parameters = object_at(*root_object_, "parameters");
        if (parameters == nullptr) return;
        if (parameters->size() > effective_.maximum_parameters) {
            add("EA_LIMIT_PARAMETERS", "/parameters", "Too many action parameters.");
        }
        for (const auto& [name, value] : *parameters) {
            const auto* definition = value.as_object();
            const auto* type = definition == nullptr ? nullptr : object_at(*definition, "valueType");
            if (type == nullptr) {
                add("EA_TYPE_PARAMETER_DEFINITION", "/parameters/" + name,
                    "A parameter requires a valueType.");
            } else {
                parameters_.emplace(name, value_contract(*type));
            }
        }
    }

    void collect_requirements() {
        const auto* requirements = object_at(*root_object_, "requires");
        if (requirements == nullptr) return;
        collect_ids(*requirements, "commands", declared_commands_);
        collect_ids(*requirements, "states", declared_states_);
        collect_ids(*requirements, "capabilities", declared_capabilities_);
        for (const auto& id : declared_commands_) {
            if (registry_.find_command(id) == nullptr) {
                add("EA_REGISTRY_COMMAND_MISSING", "/requires/commands",
                    "A declared command is absent from the injected registry.");
            }
        }
        for (const auto& id : declared_states_) {
            if (registry_.find_state(id) == nullptr) {
                add("EA_REGISTRY_STATE_MISSING", "/requires/states",
                    "A declared state is absent from the injected registry.");
            }
        }
        for (const auto& id : declared_capabilities_) {
            if (registry_.find_capability(id) == nullptr) {
                add("EA_REGISTRY_CAPABILITY_MISSING", "/requires/capabilities",
                    "A declared capability is absent from the injected registry.");
            }
        }
        const auto* dependencies = array_at(*requirements, "actions");
        if (dependencies != nullptr) {
            for (const auto& item : *dependencies) {
                const auto* object = item.as_object();
                const auto* id = object == nullptr ? nullptr : string_at(*object, "id");
                const auto* range = object == nullptr ? nullptr : string_at(*object, "versionRange");
                if (id != nullptr && range != nullptr) {
                    declared_actions_.insert(*id);
                    action_ranges_[*id] = *range;
                    if (registry_.find_action(*id, *range) == nullptr) {
                        add("EA_REGISTRY_ACTION_MISSING", "/requires/actions",
                            "A declared action is absent from the injected registry.");
                    }
                }
            }
        }
    }

    void collect_ids(const JsonObject& object, std::string_view key, StringSet& output) {
        const auto* values = array_at(object, key);
        if (values == nullptr) return;
        for (const auto& value : *values) {
            if (const auto* text = value.as_string()) output.insert(*text);
        }
    }

    void collect_nodes_and_locals() {
        nodes_ = object_at(*root_object_, "nodes");
        if (nodes_ == nullptr) return;
        if (nodes_->empty() || nodes_->size() > effective_.maximum_nodes) {
            add("EA_LIMIT_NODES", "/nodes", "The action exceeds its effective node limit.");
        }
        for (const auto& [id, value] : *nodes_) {
            const auto* node = value.as_object();
            const auto* kind = node == nullptr ? nullptr : string_at(*node, "kind");
            if (kind == nullptr) continue;
            kinds_[id] = *kind;
            const auto path = "/nodes/" + id;
            if (*kind == "InvokeCommand") {
                const auto* command_id = string_at(*node, "commandId");
                const auto* command = command_id == nullptr ? nullptr : registry_.find_command(*command_id);
                const auto* name = string_at(*node, "resultAs");
                if (command != nullptr && name != nullptr) define_local(*name, command->result, path);
            } else if (*kind == "ReadState") {
                const auto* state_id = string_at(*node, "stateId");
                const auto* state = state_id == nullptr ? nullptr : registry_.find_state(*state_id);
                const auto* name = string_at(*node, "as");
                if (state != nullptr && name != nullptr) define_local(*name, state->value, path);
            } else if (*kind == "Let") {
                const auto* name = string_at(*node, "name");
                const auto* type = object_at(*node, "valueType");
                if (name != nullptr && type != nullptr) define_local(*name, value_contract(*type), path);
            } else if (*kind == "MapValue") {
                const auto* name = string_at(*node, "as");
                if (name != nullptr) define_local(*name, {}, path);
            } else if (*kind == "InvokeAction") {
                const auto* name = string_at(*node, "resultAs");
                if (name != nullptr) define_local(*name, result_contract(), path);
            }
        }
    }

    void validate_nodes() {
        if (nodes_ == nullptr) return;
        for (const auto& [id, value] : *nodes_) {
            const auto* node = value.as_object();
            const auto* kind = node == nullptr ? nullptr : string_at(*node, "kind");
            if (node == nullptr || kind == nullptr) {
                add("EA_NODE_KIND", "/nodes/" + id, "Every node requires a kind.");
                continue;
            }
            const auto path = "/nodes/" + id;
            if (*kind == "InvokeCommand") validate_command(id, *node, path);
            else if (*kind == "ReadState") validate_state(*node, path);
            else if (*kind == "InvokeAction") validate_action(*node, path);
            else if (*kind == "Let") validate_expression_member(*node, "value", path);
            else if (*kind == "MapValue") validate_map(*node, path);
            else if (*kind == "If") validate_expression_member(*node, "predicate", path);
            else if (*kind == "Switch") validate_expression_member(*node, "value", path);
            else if (*kind == "OnResult") {
                const auto type = validate_expression_member(*node, "result", path);
                if (type.kind != EmberActionValueKind::Result) {
                    add("EA_TYPE_RESULT_REQUIRED", path + "/result",
                        "OnResult requires an invocation-result value.");
                }
            } else if (*kind == "Return") validate_expression_member(*node, "result", path);
            else if (*kind != "Sequence" && *kind != "Parallel") {
                add("EA_NODE_KIND_UNSUPPORTED", path + "/kind", "Unsupported node kind.");
            }
            collect_edges(id, *node, *kind);
        }
        if (state_reads_ > effective_.maximum_state_reads) {
            add("EA_LIMIT_STATE_READS", "/nodes", "Too many registered-state reads.");
        }
        validate_feedback();
    }

    void validate_command(const std::string& id, const JsonObject& node, const std::string& path) {
        const auto* command_id = string_at(node, "commandId");
        const auto* command = command_id == nullptr ? nullptr : registry_.find_command(*command_id);
        if (command == nullptr) {
            add("EA_REGISTRY_COMMAND_MISSING", path + "/commandId",
                "The command is absent from the injected registry.");
            return;
        }
        commands_.insert(*command_id);
        require_declared(*command_id, declared_commands_, "EA_REQUIREMENT_COMMAND", path);
        if (command->lifecycle == EmberActionRegistryLifecycle::Removed ||
            command->lifecycle == EmberActionRegistryLifecycle::DeprecatedIncompatible) {
            add("EA_REGISTRY_COMMAND_INCOMPATIBLE", path + "/commandId",
                "The command requires explicit migration.");
        }
        const auto* arguments = object_at(node, "arguments");
        for (const auto& expected : command->arguments) {
            const auto* expression = arguments == nullptr
                ? nullptr : ember_action_object_find(*arguments, expected.name);
            if (expression == nullptr) {
                if (expected.required) {
                    add("EA_COMMAND_ARGUMENT_REQUIRED", path + "/arguments/" + expected.name,
                        "A required command argument is missing.");
                }
            } else {
                const auto actual = validate_expression(*expression, path, 1U);
                if (!compatible_type_and_unit(actual, expected.value)) {
                    add("EA_COMMAND_ARGUMENT_TYPE", path + "/arguments/" + expected.name,
                        "The command argument type or unit is incompatible.");
                } else if (!range_within(actual, expected.value)) {
                    add("EA_COMMAND_ARGUMENT_RANGE", path + "/arguments/" + expected.name,
                        "The command argument cannot be proven inside the registry range.");
                }
            }
        }
        if (arguments != nullptr) {
            for (const auto& [name, ignored] : *arguments) {
                static_cast<void>(ignored);
                const auto found = std::find_if(
                    command->arguments.begin(),
                    command->arguments.end(),
                    [&](const EmberActionCommandArgumentContract& argument) {
                        return argument.name == name;
                    });
                if (found == command->arguments.end()) {
                    add("EA_COMMAND_ARGUMENT_UNKNOWN", path + "/arguments/" + name,
                        "The command argument is not declared by the injected registry.");
                }
            }
        }
        for (const auto& capability : command->required_capabilities) {
            if (registry_.find_capability(capability) == nullptr) {
                add("EA_REGISTRY_CAPABILITY_MISSING", path,
                    "A command capability is absent from the injected registry.");
            } else {
                capabilities_.insert(capability);
                require_declared(capability, declared_capabilities_, "EA_REQUIREMENT_CAPABILITY", path);
            }
        }
        direct_commands_[id] = 1U;
        realtime_class_ = stronger(realtime_class_, command->realtime_class);
        command_classes_[id] = command->realtime_class;
        parallel_ok_[id] = command->parallel_compatible;
        activate_ok_[id] = command->on_activate_safe;
    }

    void validate_state(const JsonObject& node, const std::string& path) {
        const auto* id = string_at(node, "stateId");
        const auto* state = id == nullptr ? nullptr : registry_.find_state(*id);
        if (state == nullptr) {
            add("EA_REGISTRY_STATE_MISSING", path + "/stateId",
                "The state is absent from the injected registry.");
        } else {
            states_.insert(*id);
            require_declared(*id, declared_states_, "EA_REQUIREMENT_STATE", path);
            ++state_reads_;
        }
    }

    void validate_action(const JsonObject& node, const std::string& path) {
        const auto* id = string_at(node, "actionId");
        const auto* range = string_at(node, "versionRange");
        const auto* action = id == nullptr || range == nullptr
            ? nullptr : registry_.find_action(*id, *range);
        if (action == nullptr) {
            add("EA_REGISTRY_ACTION_MISSING", path + "/actionId",
                "The action dependency is absent from the injected registry.");
        } else {
            actions_.insert(*id);
            action_ranges_[*id] = *range;
            require_declared(*id, declared_actions_, "EA_REQUIREMENT_ACTION", path);
            direct_commands_[path.substr(7U)] = action->maximum_command_invocations;
            realtime_class_ = stronger(realtime_class_, action->realtime_class);
        }
    }

    void validate_map(const JsonObject& node, const std::string& path) {
        validate_expression_member(node, "input", path);
        const auto* transforms = array_at(node, "transforms");
        if (transforms == nullptr) return;
        for (std::size_t index = 0U; index < transforms->size(); ++index) {
            const auto* transform = (*transforms)[index].as_object();
            const auto* kind = transform == nullptr ? nullptr : string_at(*transform, "kind");
            const auto item_path = path + "/transforms/" + std::to_string(index);
            if (kind == nullptr) continue;
            if (*kind == "clamp" || *kind == "deadZone") {
                const auto* minimum = number_at(*transform, "minimum");
                const auto* maximum = number_at(*transform, "maximum");
                if (minimum == nullptr || maximum == nullptr || minimum->value > maximum->value) {
                    add("EA_TRANSFORM_RANGE", item_path, "The transform range is invalid.");
                }
            } else if (*kind == "scale") {
                const auto* first = number_at(*transform, "sourceMinimum");
                const auto* last = number_at(*transform, "sourceMaximum");
                if (first == nullptr || last == nullptr || first->value == last->value) {
                    add("EA_TRANSFORM_SCALE_SOURCE", item_path,
                        "Scale requires a non-degenerate source range.");
                }
            } else if (*kind == "curve") {
                const auto* curve = string_at(*transform, "curveId");
                if (curve == nullptr || !registry_.supports_curve(*curve)) {
                    add("EA_TRANSFORM_CURVE", item_path, "The curve is not registered.");
                }
            } else if (*kind == "unitConvert") {
                const auto* source = string_at(*transform, "sourceUnit");
                const auto* target = string_at(*transform, "targetUnit");
                if (source == nullptr || target == nullptr ||
                    !registry_.supports_unit_conversion(*source, *target)) {
                    add("EA_TRANSFORM_UNIT", item_path, "The unit conversion is not approved.");
                }
            }
        }
    }

    void collect_edges(const std::string& id, const JsonObject& node, std::string_view kind) {
        if (kind == "Sequence" || kind == "Parallel") {
            group_kinds_[id] = std::string(kind);
            const auto* children = array_at(node, "children");
            if (children != nullptr) {
                if (kind == "Parallel" && children->size() > effective_.maximum_parallel_children) {
                    add("EA_LIMIT_PARALLEL_CHILDREN", "/nodes/" + id,
                        "Parallel child count exceeds the effective limit.");
                }
                for (const auto& child : *children) {
                    if (const auto* target = child.as_string()) graph_[id].push_back(*target);
                }
            }
        } else if (kind == "If") {
            edge(node, id, "then"); edge(node, id, "else");
        } else if (kind == "Switch") {
            const auto* cases = array_at(node, "cases");
            if (cases != nullptr) {
                for (const auto& item : *cases) {
                    const auto* branch = item.as_object();
                    if (branch != nullptr) edge(*branch, id, "node");
                }
            }
            edge(node, id, "default");
        } else if (kind == "Let" || kind == "MapValue") {
            edge(node, id, "next");
        } else if (kind == "OnResult") {
            const auto* cases = object_at(node, "cases");
            if (cases != nullptr) {
                for (const auto& [name, value] : *cases) {
                    static_cast<void>(name);
                    if (const auto* target = value.as_string()) graph_[id].push_back(*target);
                }
            }
        }
    }

    void edge(const JsonObject& object, const std::string& id, std::string_view key) {
        if (const auto* target = string_at(object, key)) graph_[id].push_back(*target);
    }

    void validate_feedback() {
        const auto* feedback = object_at(*root_object_, "feedback");
        if (feedback == nullptr) return;
        if (feedback->size() > effective_.maximum_feedback_outputs) {
            add("EA_LIMIT_FEEDBACK", "/feedback", "Too many feedback outputs.");
        }
        for (const auto& [name, value] : *feedback) {
            validate_expression(value, "/feedback/" + name, 1U);
        }
    }

    EmberActionValueContract validate_expression_member(
        const JsonObject& object,
        std::string_view key,
        const std::string& path) {
        const auto* value = ember_action_object_find(object, key);
        if (value == nullptr) {
            add("EA_EXPRESSION_REQUIRED", path + "/" + std::string(key),
                "A required expression is missing.");
            return {};
        }
        return validate_expression(*value, path + "/" + std::string(key), 1U);
    }

    EmberActionValueContract validate_expression(
        const JsonValue& value,
        const std::string& path,
        std::size_t depth) {
        if (depth > effective_.maximum_expression_stack_depth) {
            add("EA_LIMIT_EXPRESSION_STACK", path, "Expression depth exceeds its limit.");
            return {};
        }
        if (++expression_operations_ > effective_.maximum_expression_operations) {
            add("EA_LIMIT_EXPRESSION_OPERATIONS", path,
                "Expression operations exceed their limit.");
            return {};
        }
        const auto* object = value.as_object();
        if (object == nullptr) {
            add("EA_EXPRESSION_SHAPE", path, "An expression must be an object.");
            return {};
        }
        if (const auto* literal = ember_action_object_find(*object, "literal")) {
            return literal_contract(*literal);
        }
        if (const auto* source = string_at(*object, "source")) {
            const auto* value_path = string_at(*object, "path");
            return value_path == nullptr ? EmberActionValueContract{}
                                         : reference(*source, *value_path, path);
        }
        const auto* operation = string_at(*object, "op");
        const auto* arguments = array_at(*object, "args");
        if (operation == nullptr || arguments == nullptr) {
            add("EA_EXPRESSION_SHAPE", path, "Unknown expression shape.");
            return {};
        }
        if (!valid_arity(*operation, arguments->size())) {
            add("EA_EXPRESSION_ARITY", path, "The operator argument count is invalid.");
        }
        std::vector<EmberActionValueContract> types;
        for (std::size_t index = 0U; index < arguments->size(); ++index) {
            types.push_back(validate_expression((*arguments)[index],
                path + "/args/" + std::to_string(index), depth + 1U));
        }
        if (*operation == "and" || *operation == "or" || *operation == "not" ||
            *operation == "equal" || *operation == "notEqual" ||
            *operation == "less" || *operation == "lessOrEqual" ||
            *operation == "greater" || *operation == "greaterOrEqual" ||
            *operation == "isNull" || *operation == "isAvailable" ||
            *operation == "contains") {
            EmberActionValueContract result;
            result.kind = EmberActionValueKind::Boolean;
            if (types.size() == 2U && (*operation == "equal" || *operation == "notEqual" ||
                *operation == "less" || *operation == "lessOrEqual" ||
                *operation == "greater" || *operation == "greaterOrEqual") &&
                !compatible_type_and_unit(types[0], types[1])) {
                add("EA_EXPRESSION_UNIT", path, "Operands have incompatible types or units.");
            }
            return result;
        }
        if (*operation == "conditional") {
            if (types.size() == 3U &&
                !compatible_type_and_unit(types[1], types[2])) {
                add("EA_EXPRESSION_TYPE", path, "Conditional branches are incompatible.");
            }
            return types.size() >= 2U ? types[1] : EmberActionValueContract{};
        }
        if (types.empty()) return {};
        auto result = types.front();
        // This focused foundation does not yet perform interval arithmetic.
        // Erase inherited bounds so a bounded command cannot accept an
        // arithmetic result whose output range has not been proven.
        result.minimum.reset();
        result.maximum.reset();
        return result;
    }

    EmberActionValueContract reference(
        std::string_view source,
        const std::string& path_value,
        const std::string& path) {
        if (source == "parameter") {
            const auto found = parameters_.find(path_value);
            if (found != parameters_.end()) return found->second;
            add("EA_REFERENCE_PARAMETER", path, "Unknown parameter reference.");
        } else if (source == "local" || source == "nodeOutput") {
            const auto found = locals_.find(path_value);
            if (found != locals_.end()) return found->second;
            add("EA_REFERENCE_LOCAL", path, "Unknown invocation-local reference.");
        } else if (source == "state") {
            const auto* state = registry_.find_state(path_value);
            if (state != nullptr) {
                states_.insert(path_value);
                require_declared(path_value, declared_states_, "EA_REQUIREMENT_STATE", path);
                ++state_reads_;
                return state->value;
            }
            add("EA_REGISTRY_STATE_MISSING", path, "Unknown registered-state reference.");
        } else if (source == "context") {
            const auto* context = registry_.find_context_value(path_value);
            if (context != nullptr) return *context;
            add("EA_REFERENCE_CONTEXT", path, "Unknown binding-context reference.");
        }
        return {};
    }

    void validate_entry_points() {
        const auto* entries = object_at(*root_object_, "entryPoints");
        if (entries == nullptr || nodes_ == nullptr) return;
        std::vector<std::pair<std::string, std::string>> roots;
        for (const auto& [event, value] : *entries) {
            const auto* id = value.as_string();
            if (id == nullptr || !nodes_->contains(*id)) {
                add("EA_GRAPH_MISSING_REFERENCE", "/entryPoints/" + event,
                    "The entry point references a missing node.");
                graph_valid_ = false;
                continue;
            }
            roots.emplace_back(event, *id);
        }

        StringSet visiting;
        StringSet complete;
        for (const auto& [event, id] : roots) {
            static_cast<void>(event);
            detect_graph_errors(id, visiting, complete);
        }

        std::map<std::string, std::size_t, EmberActionUtf8ByteLess> depth_memo;
        std::map<std::string, std::size_t, EmberActionUtf8ByteLess> cost_memo;
        for (const auto& [event, id] : roots) {
            StringSet entry_reachable;
            if (graph_valid_) {
                collect_reachable(id, entry_reachable);
                reachable_.insert(entry_reachable.begin(), entry_reachable.end());
                const auto depth = longest_path(id, depth_memo);
                maximum_depth_ = std::max(maximum_depth_, depth);
                if (depth > effective_.maximum_branch_depth) {
                    add("EA_LIMIT_BRANCH_DEPTH", "/entryPoints/" + event,
                        "Graph depth exceeds the effective limit.");
                }
                const auto commands = execution_cost(id, cost_memo);
                maximum_commands_ = std::max(maximum_commands_, commands);
                if (commands > effective_.maximum_command_invocations) {
                    add("EA_LIMIT_COMMAND_INVOCATIONS", "/entryPoints/" + event,
                        "The entry point exceeds its command-submission limit.");
                }
            }
            if (event == "onActivate") {
                for (const auto& node : entry_reachable) {
                    if (direct_commands_.contains(node) &&
                        (!activate_ok_.contains(node) || !activate_ok_.at(node))) {
                        add("EA_POLICY_ON_ACTIVATE_COMMAND", "/entryPoints/onActivate",
                            "Command side effects are rejected from onActivate by default.");
                        break;
                    }
                }
            }
        }
        for (const auto& [id, value] : *nodes_) {
            static_cast<void>(value);
            if (!reachable_.contains(id)) {
                add("EA_GRAPH_UNREACHABLE_NODE", "/nodes/" + id,
                    "Every node must be reachable from an entry point.");
            }
        }
        validate_groups();
    }

    void detect_graph_errors(
        const std::string& id,
        StringSet& visiting,
        StringSet& complete) {
        if (nodes_ == nullptr || !nodes_->contains(id)) {
            add("EA_GRAPH_MISSING_REFERENCE", "/nodes/" + id,
                "A graph edge references a missing node.");
            graph_valid_ = false;
            return;
        }
        if (visiting.contains(id)) {
            add("EA_GRAPH_CYCLE", "/nodes/" + id, "The node graph must be acyclic.");
            graph_valid_ = false;
            return;
        }
        if (complete.contains(id)) return;
        visiting.insert(id);
        if (const auto found = graph_.find(id); found != graph_.end()) {
            for (const auto& child : found->second) {
                detect_graph_errors(child, visiting, complete);
            }
        }
        visiting.erase(id);
        complete.insert(id);
    }

    void collect_reachable(const std::string& id, StringSet& reachable) const {
        if (!reachable.insert(id).second) return;
        if (const auto found = graph_.find(id); found != graph_.end()) {
            for (const auto& child : found->second) collect_reachable(child, reachable);
        }
    }

    std::size_t longest_path(
        const std::string& id,
        std::map<std::string, std::size_t, EmberActionUtf8ByteLess>& memo) const {
        if (const auto cached = memo.find(id); cached != memo.end()) return cached->second;
        std::size_t child_depth = 0U;
        if (const auto found = graph_.find(id); found != graph_.end()) {
            for (const auto& child : found->second) {
                child_depth = std::max(child_depth, longest_path(child, memo));
            }
        }
        const auto result = child_depth + 1U;
        memo[id] = result;
        return result;
    }

    std::size_t execution_cost(
        const std::string& id,
        std::map<std::string, std::size_t, EmberActionUtf8ByteLess>& memo) const {
        if (const auto cached = memo.find(id); cached != memo.end()) return cached->second;
        const auto ceiling = effective_.maximum_command_invocations + 1U;
        auto result = direct_commands_.contains(id)
            ? std::min(direct_commands_.at(id), ceiling)
            : 0U;
        const auto found = graph_.find(id);
        if (found != graph_.end()) {
            const auto kind = kinds_.contains(id) ? kinds_.at(id) : std::string{};
            const auto one_branch = kind == "If" || kind == "Switch" || kind == "OnResult";
            std::size_t children = 0U;
            for (const auto& child : found->second) {
                const auto cost = execution_cost(child, memo);
                if (one_branch) {
                    children = std::max(children, cost);
                } else {
                    children = std::min(ceiling, children + std::min(cost, ceiling - children));
                }
            }
            result = std::min(ceiling, result + std::min(children, ceiling - result));
        }
        memo[id] = result;
        return result;
    }

    void validate_groups() {
        for (const auto& [id, kind] : group_kinds_) {
            const auto found = graph_.find(id);
            if (found == graph_.end()) continue;
            for (const auto& child : found->second) {
                if (command_classes_.contains(child) &&
                    command_classes_.at(child) == EmberActionRealtimeClass::RunnerPriority) {
                    add("EA_POLICY_PRIORITY_COMPOSITION", "/nodes/" + id,
                        "Priority commands cannot be wrapped by generic groups.");
                }
                if (kind == "Parallel" &&
                    (!parallel_ok_.contains(child) || !parallel_ok_.at(child))) {
                    add("EA_POLICY_PARALLEL_COMMAND", "/nodes/" + id,
                        "Parallel children require explicit registry approval.");
                }
            }
        }
    }

    void validate_action_dependencies() {
        if (actions_.size() > effective_.maximum_referenced_actions) {
            add("EA_LIMIT_REFERENCED_ACTIONS", "/requires/actions",
                "Too many action dependencies.");
        }
        const auto* current = string_at(*root_object_, "id");
        for (const auto& action : actions_) {
            StringSet visiting;
            if (current != nullptr) visiting.insert(*current);
            visit_action(action, 1U, visiting);
        }
    }

    void visit_action(const std::string& id, std::size_t depth, StringSet& visiting) {
        if (depth > effective_.maximum_action_call_depth) {
            add("EA_LIMIT_ACTION_CALL_DEPTH", "/requires/actions",
                "Action call depth exceeds the effective limit.");
            return;
        }
        if (visiting.contains(id)) {
            add("EA_ACTION_CYCLE", "/requires/actions",
                "The action dependency graph must be acyclic.");
            return;
        }
        const auto range = action_ranges_.contains(id) ? action_ranges_.at(id) : "*";
        const auto* action = registry_.find_action(id, range);
        if (action == nullptr) return;
        visiting.insert(id);
        for (const auto& child : action->invoked_actions) visit_action(child, depth + 1U, visiting);
        visiting.erase(id);
    }

    void validate_content_hash() {
        const auto* supplied = string_at(*root_object_, "contentHash");
        if (supplied != nullptr && *supplied != canonical_.content_hash) {
            add("EA_HASH_MISMATCH", "/contentHash",
                "The supplied hash does not match canonical source identity.");
        }
    }

    void define_local(
        const std::string& name,
        const EmberActionValueContract& type,
        const std::string& path) {
        if (!locals_.emplace(name, type).second) {
            add("EA_LOCAL_DUPLICATE", path, "Invocation-local names must be unique.");
        }
    }

    void require_declared(
        const std::string& id,
        const StringSet& declared,
        std::string_view code,
        const std::string& path) {
        if (!declared.contains(id)) {
            add(std::string(code), path, "A referenced dependency is not declared in requires.");
        }
    }

    [[nodiscard]] static bool valid_arity(std::string_view operation, std::size_t count) {
        if (operation == "not" || operation == "isNull" || operation == "isAvailable" ||
            operation == "round") return count == 1U;
        if (operation == "and" || operation == "or" || operation == "format") {
            return count >= 1U && count <= 16U;
        }
        if (operation == "conditional" || operation == "clamp") return count == 3U;
        return count == 2U;
    }

    void add(std::string code, std::string path, std::string message) {
        diagnostics_.push_back({std::move(code), std::move(path), std::move(message)});
    }

    const EmberActionRegistryView& registry_;
    EmberActionPlatformLimits limits_;
    EmberActionPlatformLimits effective_;
    std::shared_ptr<JsonValue> root_;
    EmberActionCanonicalSource canonical_;
    const JsonObject* root_object_{nullptr};
    const JsonObject* nodes_{nullptr};
    std::vector<EmberActionDiagnostic> diagnostics_;
    std::map<std::string, EmberActionValueContract, EmberActionUtf8ByteLess> parameters_;
    std::map<std::string, EmberActionValueContract, EmberActionUtf8ByteLess> locals_;
    std::map<std::string, std::vector<std::string>, EmberActionUtf8ByteLess> graph_;
    std::map<std::string, std::string, EmberActionUtf8ByteLess> kinds_;
    std::map<std::string, std::string, EmberActionUtf8ByteLess> group_kinds_;
    std::map<std::string, std::size_t, EmberActionUtf8ByteLess> direct_commands_;
    std::map<std::string, EmberActionRealtimeClass, EmberActionUtf8ByteLess> command_classes_;
    std::map<std::string, bool, EmberActionUtf8ByteLess> parallel_ok_;
    std::map<std::string, bool, EmberActionUtf8ByteLess> activate_ok_;
    std::map<std::string, std::string, EmberActionUtf8ByteLess> action_ranges_;
    StringSet declared_commands_;
    StringSet declared_states_;
    StringSet declared_capabilities_;
    StringSet declared_actions_;
    StringSet commands_;
    StringSet states_;
    StringSet capabilities_;
    StringSet actions_;
    StringSet reachable_;
    bool graph_valid_{true};
    std::size_t state_reads_{0U};
    std::size_t expression_operations_{0U};
    std::size_t maximum_depth_{0U};
    std::size_t maximum_commands_{0U};
    EmberActionRealtimeClass realtime_class_{EmberActionRealtimeClass::ViewLocal};
};

}  // namespace

EmberActionCompileResult prepare_ember_action_source(
    std::string_view source,
    const EmberActionRegistryView& registry,
    const EmberActionPlatformLimits& limits) {
    EmberActionJsonReadLimits read_limits;
    read_limits.maximum_source_bytes = limits.maximum_source_bytes;
    auto parsed = parse_ember_action_json(source, read_limits);
    if (!parsed.ok()) {
        EmberActionCompileResult result;
        result.diagnostics = std::move(parsed.diagnostics);
        return result;
    }
    auto root = std::make_shared<JsonValue>(std::move(*parsed.value));
    auto canonical = canonicalize_ember_action_source(*root);
    return Validator(registry, limits, std::move(root), std::move(canonical)).run();
}

}  // namespace emberlights
