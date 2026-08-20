#include "emberlights/ember_action_executor.hpp"

#include "emberlights/file_identity.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <bitset>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace emberlights {
namespace {

using JsonValue = EmberActionJsonValue;
using JsonObject = EmberActionJsonValue::Object;
using JsonArray = EmberActionJsonValue::Array;

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

void add_diagnostic(
    std::vector<EmberActionDiagnostic>& diagnostics,
    std::string code,
    std::string path,
    std::string message) {
    diagnostics.push_back({std::move(code), std::move(path), std::move(message)});
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
    if (const auto* maximum = number_at(object, "maxItems")) {
        if (maximum->value >= 0.0) {
            result.maximum_items = static_cast<std::size_t>(maximum->value);
        }
    }
    if (const auto* maximum = number_at(object, "maxLength")) {
        if (maximum->value >= 0.0) {
            result.maximum_string_bytes = static_cast<std::size_t>(maximum->value);
        }
    }
    if (const auto* values = array_at(object, "values")) {
        result.enum_values.reserve(values->size());
        for (const auto& value : *values) {
            if (const auto* text = value.as_string()) {
                result.enum_values.push_back(*text);
            }
        }
    }
    return result;
}

[[nodiscard]] std::optional<EmberActionRuntimeValueKind> runtime_kind(
    EmberActionValueKind kind) noexcept {
    switch (kind) {
        case EmberActionValueKind::Boolean:
            return EmberActionRuntimeValueKind::Boolean;
        case EmberActionValueKind::Integer:
            return EmberActionRuntimeValueKind::Integer;
        case EmberActionValueKind::Number:
            return EmberActionRuntimeValueKind::Number;
        case EmberActionValueKind::Enum:
            return EmberActionRuntimeValueKind::Enum;
        case EmberActionValueKind::String:
            return EmberActionRuntimeValueKind::String;
        case EmberActionValueKind::StableId:
            return EmberActionRuntimeValueKind::StableId;
        case EmberActionValueKind::SemanticRole:
            return EmberActionRuntimeValueKind::SemanticRole;
        default:
            return std::nullopt;
    }
}

[[nodiscard]] std::optional<UiInvocationResult> invocation_result(
    std::string_view name) noexcept {
    if (name == "Accepted") return UiInvocationResult::Accepted;
    if (name == "NoChange") return UiInvocationResult::NoChange;
    if (name == "Unavailable") return UiInvocationResult::Unavailable;
    if (name == "InvalidArguments") return UiInvocationResult::InvalidArguments;
    if (name == "NotFound") return UiInvocationResult::NotFound;
    if (name == "ValidationFailed") return UiInvocationResult::ValidationFailed;
    if (name == "QueueFull") return UiInvocationResult::QueueFull;
    if (name == "SafetyRejected") return UiInvocationResult::SafetyRejected;
    if (name == "Unsupported") return UiInvocationResult::Unsupported;
    if (name == "InternalError") return UiInvocationResult::InternalError;
    return std::nullopt;
}

[[nodiscard]] std::optional<UiInvocationResult> executable_on_result_case(
    std::string_view name) noexcept {
    if (name == "Accepted") return UiInvocationResult::Accepted;
    if (name == "NoChange") return UiInvocationResult::NoChange;
    if (name == "Unavailable") return UiInvocationResult::Unavailable;
    if (name == "InvalidArguments") return UiInvocationResult::InvalidArguments;
    if (name == "ValidationFailed") return UiInvocationResult::ValidationFailed;
    if (name == "Unsupported") return UiInvocationResult::Unsupported;
    return std::nullopt;
}

[[nodiscard]] bool reserved_on_result_case(std::string_view name) noexcept {
    return name == "MissingTarget" || name == "Cancelled" ||
        name == "StartedAsync";
}

[[nodiscard]] bool hard_stop_on_result_case(std::string_view name) noexcept {
    return name == "QueueFull" || name == "SafetyRejected" ||
        name == "InternalError";
}

[[nodiscard]] std::optional<EmberActionEntryPoint> entry_point(
    std::string_view name) noexcept {
    if (name == "onPress") return EmberActionEntryPoint::OnPress;
    if (name == "onRelease") return EmberActionEntryPoint::OnRelease;
    if (name == "onValue") return EmberActionEntryPoint::OnValue;
    if (name == "onEncoderStep") return EmberActionEntryPoint::OnEncoderStep;
    if (name == "onLongPress") return EmberActionEntryPoint::OnLongPress;
    if (name == "onDoublePress") return EmberActionEntryPoint::OnDoublePress;
    return std::nullopt;
}

[[nodiscard]] std::optional<EmberActionSequencePolicy> sequence_policy(
    const JsonObject& node) noexcept {
    const auto* policy = string_at(node, "policy");
    if (policy == nullptr || *policy == "stopOnError") {
        return EmberActionSequencePolicy::StopOnError;
    }
    if (*policy == "continue") return EmberActionSequencePolicy::Continue;
    if (*policy == "stopOnRejected") {
        return EmberActionSequencePolicy::StopOnRejected;
    }
    if (*policy == "stopOnAnyNonAccepted") {
        return EmberActionSequencePolicy::StopOnAnyNonAccepted;
    }
    return std::nullopt;
}

template <std::size_t Count>
[[nodiscard]] bool only_fields(
    const JsonObject& object,
    const std::array<std::string_view, Count>& allowed) noexcept {
    return std::all_of(object.begin(), object.end(), [&](const auto& item) {
        return std::find(allowed.begin(), allowed.end(), item.first) != allowed.end();
    });
}

[[nodiscard]] const EmberActionResolvedCommandReference* resolved_command(
    const EmberActionIrFoundation& foundation,
    std::string_view id) noexcept {
    const auto found = std::lower_bound(
        foundation.commands.begin(),
        foundation.commands.end(),
        id,
        [](const EmberActionResolvedCommandReference& command, std::string_view value) {
            return command.id.compare(value) < 0;
        });
    return found != foundation.commands.end() && found->id == id ? &*found : nullptr;
}

[[nodiscard]] bool executable_value_contract(
    const EmberActionValueContract& value) noexcept {
    const auto kind = runtime_kind(value.kind);
    if (!kind.has_value()) return false;
    if (value.maximum_string_bytes > kEmberActionExecutionMaximumTextBytes) {
        return false;
    }
    if (value.kind == EmberActionValueKind::Enum && value.enum_values.empty()) {
        // Schema-referenced enums need a generated compact value table before
        // runtime validation can be fail-closed.
        return false;
    }
    return true;
}

[[nodiscard]] std::uint64_t executable_ir_structural_seal(
    const EmberActionExecutableIr& ir) noexcept;

class ExecutableCompiler {
public:
    ExecutableCompiler(
        std::shared_ptr<const EmberActionIrFoundation> foundation,
        const EmberActionRegistryView& registry)
        : foundation_(std::move(foundation)), registry_(registry) {}

    [[nodiscard]] EmberActionExecutableIrResult run() {
        EmberActionExecutableIrResult result;
        if (!validate_foundation()) {
            result.diagnostics = std::move(diagnostics_);
            return result;
        }
        root_ = foundation_->prepared->source->as_object();
        if (root_ == nullptr) {
            add("EA_EXEC_IR_ROOT", "/", "Prepared Action source has no object root.");
        } else {
            collect_parameters();
            collect_node_slots();
            compile_nodes();
            compile_entries();
            validate_entries();
        }
        if (diagnostics_.empty()) {
            candidate_->execution_digest = make_execution_digest();
            candidate_->structural_seal = executable_ir_structural_seal(*candidate_);
            result.ir = std::shared_ptr<const EmberActionExecutableIr>(
                std::move(candidate_));
        }
        result.diagnostics = std::move(diagnostics_);
        return result;
    }

private:
    struct Metrics {
        std::size_t node_visits{0U};
        std::size_t dispatches{0U};
        std::size_t depth{0U};
        std::size_t expression_operations{0U};
        bool valid{true};
    };

    struct ScalarOperand {
        EmberActionIrValue value;
        EmberActionValueContract contract;
        const JsonValue* literal{nullptr};
    };

    bool validate_foundation() {
        if (foundation_ == nullptr || foundation_->prepared == nullptr ||
            foundation_->prepared->source == nullptr) {
            add("EA_EXEC_IR_FOUNDATION_REQUIRED", "/",
                "An immutable resolved Action IR foundation is required.");
            return false;
        }
        const auto& dependencies = foundation_->prepared->dependencies;
        const auto expected_source_hash =
            "sha256:" + sha256_text(foundation_->prepared->normalized_json);
        if (foundation_->prepared->content_hash != expected_source_hash ||
            foundation_->cache_key.source_hash != expected_source_hash ||
            foundation_->cache_key.cache_digest.empty() ||
            foundation_->cache_key.dependency_digest.empty()) {
            add("EA_EXEC_IR_FOUNDATION_IDENTITY", "/contentHash",
                "The resolved IR foundation identity is incomplete or stale.");
        }
        if (dependencies.registry_digest != registry_.registry_digest() ||
            foundation_->cache_key.registry_digest != registry_.registry_digest()) {
            add("EA_EXEC_IR_REGISTRY_DIGEST", "/requires",
                "The executable IR registry digest does not match the compiler view.");
        }
        if (!dependencies.states.empty()) {
            add("EA_EXEC_IR_STATE_DEPENDENCY_UNSUPPORTED", "/requires/states",
                "Registered-state reads are not executable in this bounded slice.");
        }
        if (!dependencies.capabilities.empty()) {
            add("EA_EXEC_IR_CAPABILITY_DEPENDENCY_UNSUPPORTED", "/requires/capabilities",
                "Capability-dependent execution awaits an injected availability snapshot.");
        }
        if (!dependencies.actions.empty()) {
            add("EA_EXEC_IR_ACTION_DEPENDENCY_UNSUPPORTED", "/requires/actions",
                "Nested Action invocation is not executable in this bounded slice.");
        }
        if (foundation_->prepared->realtime_class !=
            EmberActionRealtimeClass::RunnerCommand) {
            add("EA_EXEC_IR_REALTIME_CLASS_UNSUPPORTED", "/nodes",
                "Only ordinary runnerCommand Actions are executable in this slice.");
        }
        candidate_ = std::make_shared<EmberActionExecutableIr>();
        candidate_->foundation = foundation_;
        return diagnostics_.empty();
    }

    void collect_parameters() {
        const auto* parameters = object_at(*root_, "parameters");
        if (parameters == nullptr) return;
        candidate_->parameters.reserve(parameters->size());
        for (const auto& [name, value] : *parameters) {
            const auto* definition = value.as_object();
            const auto* type = definition == nullptr
                ? nullptr : object_at(*definition, "valueType");
            if (type == nullptr) {
                add("EA_EXEC_IR_PARAMETER_TYPE", "/parameters/" + name,
                    "An executable parameter requires a value contract.");
                continue;
            }
            auto contract = value_contract(*type);
            if (contract.kind == EmberActionValueKind::Enum) {
                const auto* values = array_at(*type, "values");
                if (values == nullptr || values->empty() ||
                    contract.enum_values.size() != values->size()) {
                    add("EA_EXEC_IR_PARAMETER_ENUM_UNSUPPORTED", "/parameters/" + name,
                        "Executable enum parameters require a non-empty inline string table.");
                    continue;
                }
            }
            if (!executable_value_contract(contract)) {
                add("EA_EXEC_IR_PARAMETER_TYPE_UNSUPPORTED", "/parameters/" + name,
                    "The parameter type lacks a bounded runtime representation.");
                continue;
            }
            parameter_indices_[name] = static_cast<std::uint16_t>(
                candidate_->parameters.size());
            candidate_->parameters.push_back({name, std::move(contract)});
        }
    }

    void collect_node_slots() {
        nodes_ = object_at(*root_, "nodes");
        if (nodes_ == nullptr || nodes_->empty() ||
            nodes_->size() > kEmberActionExecutionMaximumNodes) {
            add("EA_EXEC_IR_NODE_COUNT", "/nodes",
                "Executable IR requires one to 64 nodes.");
            return;
        }
        candidate_->nodes.resize(nodes_->size());
        std::size_t index = 0U;
        for (const auto& [id, value] : *nodes_) {
            static_cast<void>(value);
            node_indices_[id] = static_cast<std::uint16_t>(index);
            candidate_->nodes[index].id = id;
            ++index;
        }
        for (const auto& [id, value] : *nodes_) {
            const auto* node = value.as_object();
            const auto* kind = node == nullptr ? nullptr : string_at(*node, "kind");
            if (kind != nullptr && *kind == "InvokeCommand") {
                const auto* name = string_at(*node, "resultAs");
                if (name != nullptr) {
                    if (result_slots_.size() >= kEmberActionExecutionMaximumResults) {
                        add("EA_EXEC_IR_RESULT_SLOTS", "/nodes/" + id + "/resultAs",
                            "Executable result slots exceed the platform limit.");
                    } else {
                        result_slots_[*name] = static_cast<std::uint16_t>(
                            result_slots_.size());
                    }
                }
            }
        }
        candidate_->result_slot_count = result_slots_.size();
    }

    void compile_nodes() {
        if (nodes_ == nullptr) return;
        for (const auto& [id, value] : *nodes_) {
            const auto* node = value.as_object();
            const auto* kind = node == nullptr ? nullptr : string_at(*node, "kind");
            const auto path = "/nodes/" + id;
            if (node == nullptr || kind == nullptr) {
                add("EA_EXEC_IR_NODE_SHAPE", path, "Executable node shape is invalid.");
                continue;
            }
            auto& output = candidate_->nodes[node_indices_.at(id)];
            if (*kind == "Sequence") {
                compile_sequence(*node, path, output);
            } else if (*kind == "InvokeCommand") {
                compile_command(*node, path, output);
            } else if (*kind == "If") {
                compile_if(*node, path, output);
            } else if (*kind == "Switch") {
                compile_switch(*node, path, output);
            } else if (*kind == "OnResult") {
                compile_on_result(*node, path, output);
            } else if (*kind == "Return") {
                compile_return(*node, path, output);
            } else {
                add("EA_EXEC_IR_NODE_UNSUPPORTED", path + "/kind",
                    "This executable slice supports only Sequence, InvokeCommand, If, Switch, OnResult, and Return.");
            }
        }
    }

    void compile_sequence(
        const JsonObject& node,
        const std::string& path,
        EmberActionExecutableNode& output) {
        output.kind = EmberActionIrNodeKind::Sequence;
        constexpr std::array allowed{
            std::string_view{"kind"}, std::string_view{"children"},
            std::string_view{"policy"}, std::string_view{"metadata"}};
        if (!only_fields(node, allowed)) {
            add("EA_EXEC_IR_SEQUENCE_PROPERTY", path,
                "Sequence contains a property outside the executable schema subset.");
        }
        const auto policy = sequence_policy(node);
        if (!policy.has_value()) {
            add("EA_EXEC_IR_SEQUENCE_POLICY", path + "/policy",
                "Sequence policy is not supported by this executable IR.");
        } else {
            output.sequence_policy = *policy;
        }
        const auto* children = array_at(node, "children");
        if (children == nullptr || children->empty() || children->size() > 32U) {
            add("EA_EXEC_IR_SEQUENCE_CHILDREN", path + "/children",
                "An executable Sequence requires one to 32 children.");
            return;
        }
        output.children.reserve(children->size());
        for (const auto& child : *children) {
            const auto* id = child.as_string();
            const auto found = id == nullptr ? node_indices_.end() : node_indices_.find(*id);
            if (found == node_indices_.end()) {
                add("EA_EXEC_IR_NODE_REFERENCE", path + "/children",
                    "A Sequence child is absent from executable IR.");
            } else {
                output.children.push_back(found->second);
            }
        }
    }

    void compile_command(
        const JsonObject& node,
        const std::string& path,
        EmberActionExecutableNode& output) {
        output.kind = EmberActionIrNodeKind::InvokeCommand;
        constexpr std::array allowed{
            std::string_view{"kind"}, std::string_view{"commandId"},
            std::string_view{"arguments"}, std::string_view{"resultAs"},
            std::string_view{"metadata"}};
        if (!only_fields(node, allowed)) {
            add("EA_EXEC_IR_COMMAND_PROPERTY", path,
                "InvokeCommand contains a property outside the executable schema subset.");
        }
        const auto* command_id = string_at(node, "commandId");
        const auto* command = command_id == nullptr
            ? nullptr : registry_.find_command(*command_id);
        const auto* resolved = command_id == nullptr
            ? nullptr : resolved_command(*foundation_, *command_id);
        if (command == nullptr || resolved == nullptr) {
            add("EA_EXEC_IR_COMMAND_UNRESOLVED", path + "/commandId",
                "The executable command lacks one accepted generated native handle.");
            return;
        }
        if (command->lifecycle != EmberActionRegistryLifecycle::Current) {
            add("EA_EXEC_IR_COMMAND_LIFECYCLE", path + "/commandId",
                "Deprecated or removed commands require explicit migration before execution.");
        }
        if (command->realtime_class != EmberActionRealtimeClass::RunnerCommand) {
            add("EA_EXEC_IR_COMMAND_CONTEXT", path + "/commandId",
                "Only ordinary runnerCommand dispatch is approved in this executable slice.");
        }
        if (!command->required_capabilities.empty()) {
            add("EA_EXEC_IR_COMMAND_CAPABILITY", path + "/commandId",
                "Capability-gated commands require an injected availability contract.");
        }
        output.command = resolved->command;

        const auto* arguments = object_at(node, "arguments");
        if (arguments == nullptr || arguments->size() > kEmberActionExecutionMaximumArguments) {
            add("EA_EXEC_IR_ARGUMENT_COUNT", path + "/arguments",
                "InvokeCommand arguments exceed the executable schema bound.");
            return;
        }
        output.arguments.reserve(command->arguments.size());
        for (const auto& expected : command->arguments) {
            const auto* expression = arguments == nullptr
                ? nullptr : ember_action_object_find(*arguments, expected.name);
            if (expression == nullptr) continue;
            if (!executable_value_contract(expected.value)) {
                add("EA_EXEC_IR_ARGUMENT_TYPE_UNSUPPORTED",
                    path + "/arguments/" + expected.name,
                    "The command argument lacks a bounded runtime representation.");
                continue;
            }
            EmberActionIrArgument argument;
            argument.name = expected.name;
            argument.expected = expected.value;
            if (compile_argument(*expression, expected.value,
                    path + "/arguments/" + expected.name, argument.value)) {
                output.arguments.push_back(std::move(argument));
            }
        }
        if (const auto* name = string_at(node, "resultAs")) {
            const auto found = result_slots_.find(*name);
            if (found == result_slots_.end()) {
                add("EA_EXEC_IR_RESULT_SLOT", path + "/resultAs",
                    "The command result slot could not be resolved.");
            } else {
                output.result_slot = found->second;
            }
        }
    }

    bool compile_argument(
        const JsonValue& expression,
        const EmberActionValueContract& expected,
        const std::string& path,
        EmberActionIrValue& output) {
        const auto* object = expression.as_object();
        if (object == nullptr) {
            add("EA_EXEC_IR_EXPRESSION", path,
                "An executable command argument requires one expression object.");
            return false;
        }
        if (const auto* literal = ember_action_object_find(*object, "literal")) {
            if (object->size() != 1U) {
                add("EA_EXEC_IR_EXPRESSION_PROPERTY", path,
                    "A literal expression cannot contain additional properties.");
                return false;
            }
            output.source = EmberActionIrValueSource::Literal;
            return compile_literal(*literal, expected, path, output);
        }
        const auto* source = string_at(*object, "source");
        const auto* value_path = string_at(*object, "path");
        if (source == nullptr || value_path == nullptr || *source != "parameter" ||
            object->size() != 2U) {
            add("EA_EXEC_IR_EXPRESSION_UNSUPPORTED", path,
                "Executable command arguments support only literals and declared parameters.");
            return false;
        }
        const auto found = parameter_indices_.find(*value_path);
        if (found == parameter_indices_.end()) {
            add("EA_EXEC_IR_PARAMETER_REFERENCE", path,
                "The executable parameter reference is unresolved.");
            return false;
        }
        output.source = EmberActionIrValueSource::Parameter;
        output.parameter_index = found->second;
        const auto kind = runtime_kind(candidate_->parameters[found->second].value.kind);
        if (!kind.has_value()) {
            add("EA_EXEC_IR_PARAMETER_TYPE_UNSUPPORTED", path,
                "The referenced parameter lacks a runtime representation.");
            return false;
        }
        output.kind = *kind;
        return true;
    }

    bool compile_literal(
        const JsonValue& literal,
        const EmberActionValueContract& expected,
        const std::string& path,
        EmberActionIrValue& output) {
        const auto expected_kind = runtime_kind(expected.kind);
        if (!expected_kind.has_value()) return false;
        output.kind = *expected_kind;
        if (const auto* value = literal.as_boolean()) {
            if (expected.kind != EmberActionValueKind::Boolean) {
                add("EA_EXEC_IR_LITERAL_TYPE", path, "Boolean literal type is incompatible.");
                return false;
            }
            output.boolean_value = *value;
            return true;
        }
        if (const auto* value = literal.as_number()) {
            if (expected.kind != EmberActionValueKind::Integer &&
                expected.kind != EmberActionValueKind::Number) {
                add("EA_EXEC_IR_LITERAL_TYPE", path, "Numeric literal type is incompatible.");
                return false;
            }
            if (!std::isfinite(value->value)) {
                add("EA_EXEC_IR_LITERAL_FINITE", path, "Numeric literals must be finite.");
                return false;
            }
            if (expected.kind == EmberActionValueKind::Integer) {
                constexpr double minimum_integer = -0x1p63;
                constexpr double maximum_integer_exclusive = 0x1p63;
                if (std::floor(value->value) != value->value ||
                    value->value < minimum_integer ||
                    value->value >= maximum_integer_exclusive) {
                    add("EA_EXEC_IR_LITERAL_INTEGER", path,
                        "Integer literals must be exactly representable in the runtime range.");
                    return false;
                }
                output.integer_value = static_cast<std::int64_t>(value->value);
            }
            if ((expected.minimum.has_value() && value->value < *expected.minimum) ||
                (expected.maximum.has_value() && value->value > *expected.maximum)) {
                add("EA_EXEC_IR_LITERAL_RANGE", path,
                    "Numeric literal is outside the declared runtime range.");
                return false;
            }
            output.number_value = value->value;
            return true;
        }
        if (const auto* value = literal.as_string()) {
            if (expected.kind != EmberActionValueKind::Enum &&
                expected.kind != EmberActionValueKind::String &&
                expected.kind != EmberActionValueKind::StableId &&
                expected.kind != EmberActionValueKind::SemanticRole) {
                add("EA_EXEC_IR_LITERAL_TYPE", path, "String literal type is incompatible.");
                return false;
            }
            const auto maximum = expected.maximum_string_bytes == 0U
                ? kEmberActionExecutionMaximumTextBytes
                : std::min(expected.maximum_string_bytes,
                    kEmberActionExecutionMaximumTextBytes);
            if (value->size() > maximum) {
                add("EA_EXEC_IR_LITERAL_TEXT", path,
                    "String-like literal exceeds the executable text limit.");
                return false;
            }
            if ((expected.kind == EmberActionValueKind::StableId ||
                 expected.kind == EmberActionValueKind::SemanticRole) && value->empty()) {
                add("EA_EXEC_IR_LITERAL_TEXT", path,
                    "Stable identifiers and semantic roles cannot be empty.");
                return false;
            }
            if (expected.kind == EmberActionValueKind::Enum &&
                std::find(expected.enum_values.begin(), expected.enum_values.end(), *value) ==
                    expected.enum_values.end()) {
                add("EA_EXEC_IR_LITERAL_ENUM", path,
                    "Enum literal is absent from the declared inline value table.");
                return false;
            }
            output.text = *value;
            return true;
        }
        add("EA_EXEC_IR_LITERAL_UNSUPPORTED", path,
            "Null, list, and object literals are not executable in this bounded slice.");
        return false;
    }

    [[nodiscard]] static bool numeric_kind(EmberActionValueKind kind) noexcept {
        return kind == EmberActionValueKind::Integer ||
            kind == EmberActionValueKind::Number;
    }

    [[nodiscard]] static bool text_kind(EmberActionValueKind kind) noexcept {
        return kind == EmberActionValueKind::Enum ||
            kind == EmberActionValueKind::String ||
            kind == EmberActionValueKind::StableId ||
            kind == EmberActionValueKind::SemanticRole;
    }

    bool compile_scalar_expression(
        const JsonValue& expression,
        const std::string& path,
        ScalarOperand& output) {
        const auto* object = expression.as_object();
        if (object == nullptr) {
            add("EA_EXEC_IR_EXPRESSION", path,
                "A control-flow operand requires one expression object.");
            return false;
        }
        if (const auto* literal = ember_action_object_find(*object, "literal")) {
            if (object->size() != 1U) {
                add("EA_EXEC_IR_EXPRESSION_PROPERTY", path,
                    "A literal expression cannot contain additional properties.");
                return false;
            }
            output.literal = literal;
            if (literal->as_boolean() != nullptr) {
                output.contract.kind = EmberActionValueKind::Boolean;
            } else if (const auto* number = literal->as_number()) {
                if (!std::isfinite(number->value)) {
                    add("EA_EXEC_IR_LITERAL_FINITE", path,
                        "Numeric literals must be finite.");
                    return false;
                }
                output.contract.kind = std::floor(number->value) == number->value
                    ? EmberActionValueKind::Integer
                    : EmberActionValueKind::Number;
                output.contract.minimum = number->value;
                output.contract.maximum = number->value;
            } else if (const auto* text = literal->as_string()) {
                output.contract.kind = EmberActionValueKind::String;
                output.contract.maximum_string_bytes = text->size();
            } else {
                add("EA_EXEC_IR_LITERAL_UNSUPPORTED", path,
                    "Control flow supports only boolean, numeric, and string literals.");
                return false;
            }
            output.value.source = EmberActionIrValueSource::Literal;
            return compile_literal(*literal, output.contract, path, output.value);
        }
        const auto* source = string_at(*object, "source");
        const auto* value_path = string_at(*object, "path");
        if (source == nullptr || value_path == nullptr || *source != "parameter" ||
            object->size() != 2U) {
            add("EA_EXEC_IR_EXPRESSION_UNSUPPORTED", path,
                "Control flow supports only literals and declared parameters.");
            return false;
        }
        const auto found = parameter_indices_.find(*value_path);
        if (found == parameter_indices_.end()) {
            add("EA_EXEC_IR_PARAMETER_REFERENCE", path,
                "The control-flow parameter reference is unresolved.");
            return false;
        }
        output.contract = candidate_->parameters[found->second].value;
        const auto kind = runtime_kind(output.contract.kind);
        if (!kind.has_value()) {
            add("EA_EXEC_IR_PARAMETER_TYPE_UNSUPPORTED", path,
                "The control-flow parameter lacks a runtime representation.");
            return false;
        }
        output.value.source = EmberActionIrValueSource::Parameter;
        output.value.parameter_index = found->second;
        output.value.kind = *kind;
        return true;
    }

    bool recompile_literal_operand(
        ScalarOperand& operand,
        const EmberActionValueContract& expected,
        const std::string& path) {
        if (operand.literal == nullptr) return false;
        EmberActionIrValue value;
        value.source = EmberActionIrValueSource::Literal;
        if (!compile_literal(*operand.literal, expected, path, value)) return false;
        operand.value = std::move(value);
        operand.contract = expected;
        return true;
    }

    bool reconcile_comparison_operands(
        ScalarOperand& first,
        ScalarOperand& second,
        bool ordered,
        const std::string& path) {
        if (first.literal != nullptr && second.literal == nullptr) {
            if (!recompile_literal_operand(first, second.contract, path + "/args/0")) {
                return false;
            }
        } else if (second.literal != nullptr && first.literal == nullptr) {
            if (!recompile_literal_operand(second, first.contract, path + "/args/1")) {
                return false;
            }
        } else if (first.literal != nullptr && second.literal != nullptr &&
                   numeric_kind(first.contract.kind) && numeric_kind(second.contract.kind) &&
                   first.contract.kind != second.contract.kind) {
            EmberActionValueContract numeric;
            numeric.kind = EmberActionValueKind::Number;
            if (!recompile_literal_operand(first, numeric, path + "/args/0") ||
                !recompile_literal_operand(second, numeric, path + "/args/1")) {
                return false;
            }
        }

        const auto same_kind = first.contract.kind == second.contract.kind;
        const auto permitted_kind = ordered
            ? numeric_kind(first.contract.kind)
            : first.contract.kind == EmberActionValueKind::Boolean ||
                numeric_kind(first.contract.kind) || text_kind(first.contract.kind);
        const auto units_match = first.contract.unit.empty() || second.contract.unit.empty() ||
            first.contract.unit == second.contract.unit;
        const auto targets_match = first.contract.target_kind.empty() ||
            second.contract.target_kind.empty() ||
            first.contract.target_kind == second.contract.target_kind;
        if (!same_kind || !permitted_kind || !units_match || !targets_match) {
            add("EA_EXEC_IR_PREDICATE_TYPE", path,
                "Comparison operands require one compatible bounded scalar type and unit.");
            return false;
        }
        return true;
    }

    bool append_predicate_instruction(
        EmberActionExecutableNode& output,
        EmberActionIrPredicateOperation operation,
        const std::string& path,
        EmberActionIrValue value = {}) {
        if (compiled_expression_operations_ >=
            kEmberActionExecutionMaximumExpressionOperations) {
            if (!expression_limit_reported_) {
                add("EA_EXEC_IR_EXPRESSION_LIMIT", path,
                    "Compiled control-flow expressions exceed the platform operation limit.");
                expression_limit_reported_ = true;
            }
            return false;
        }
        ++compiled_expression_operations_;
        output.predicate.push_back({operation, std::move(value)});
        return true;
    }

    bool compile_predicate(
        const JsonValue& expression,
        const std::string& path,
        EmberActionExecutableNode& output,
        std::size_t depth) {
        if (depth > kEmberActionExecutionMaximumExpressionStackDepth) {
            add("EA_EXEC_IR_EXPRESSION_DEPTH", path,
                "Control-flow expression depth exceeds the runtime stack limit.");
            return false;
        }
        const auto* object = expression.as_object();
        if (object == nullptr) {
            add("EA_EXEC_IR_EXPRESSION", path,
                "An If predicate requires one expression object.");
            return false;
        }
        if (ember_action_object_find(*object, "literal") != nullptr ||
            ember_action_object_find(*object, "source") != nullptr) {
            ScalarOperand operand;
            if (!compile_scalar_expression(expression, path, operand)) return false;
            if (operand.contract.kind != EmberActionValueKind::Boolean) {
                add("EA_EXEC_IR_PREDICATE_TYPE", path,
                    "An unwrapped If predicate must be boolean.");
                return false;
            }
            return append_predicate_instruction(output,
                EmberActionIrPredicateOperation::PushValue, path,
                std::move(operand.value));
        }

        constexpr std::array allowed{
            std::string_view{"op"}, std::string_view{"args"}};
        if (!only_fields(*object, allowed)) {
            add("EA_EXEC_IR_PREDICATE_PROPERTY", path,
                "A predicate operator contains an unknown property.");
            return false;
        }
        const auto* operation = string_at(*object, "op");
        const auto* arguments = array_at(*object, "args");
        if (operation == nullptr || arguments == nullptr) {
            add("EA_EXEC_IR_PREDICATE_SHAPE", path,
                "A predicate operator requires op and args.");
            return false;
        }
        if (*operation == "not") {
            if (arguments->size() != 1U) {
                add("EA_EXEC_IR_PREDICATE_ARITY", path,
                    "not requires exactly one predicate operand.");
                return false;
            }
            if (!compile_predicate((*arguments)[0], path + "/args/0", output,
                    depth + 1U)) {
                return false;
            }
            return append_predicate_instruction(
                output, EmberActionIrPredicateOperation::Not, path);
        }
        if (*operation == "and" || *operation == "or") {
            if (arguments->empty() || arguments->size() > 16U) {
                add("EA_EXEC_IR_PREDICATE_ARITY", path,
                    "and/or requires one to 16 predicate operands.");
                return false;
            }
            for (std::size_t index = 0U; index < arguments->size(); ++index) {
                if (!compile_predicate((*arguments)[index],
                        path + "/args/" + std::to_string(index), output,
                        depth + 1U)) {
                    return false;
                }
                if (index != 0U && !append_predicate_instruction(
                        output,
                        *operation == "and" ? EmberActionIrPredicateOperation::And
                                             : EmberActionIrPredicateOperation::Or,
                        path)) {
                    return false;
                }
            }
            return true;
        }

        const auto predicate_operation = [&]()
            -> std::optional<EmberActionIrPredicateOperation> {
            if (*operation == "equal") return EmberActionIrPredicateOperation::Equal;
            if (*operation == "notEqual") return EmberActionIrPredicateOperation::NotEqual;
            if (*operation == "less") return EmberActionIrPredicateOperation::Less;
            if (*operation == "lessOrEqual") {
                return EmberActionIrPredicateOperation::LessOrEqual;
            }
            if (*operation == "greater") return EmberActionIrPredicateOperation::Greater;
            if (*operation == "greaterOrEqual") {
                return EmberActionIrPredicateOperation::GreaterOrEqual;
            }
            return std::nullopt;
        }();
        if (!predicate_operation.has_value()) {
            add("EA_EXEC_IR_PREDICATE_OPERATOR", path + "/op",
                "Only boolean composition and typed comparisons are executable here.");
            return false;
        }
        if (arguments->size() != 2U) {
            add("EA_EXEC_IR_PREDICATE_ARITY", path,
                "A comparison requires exactly two scalar operands.");
            return false;
        }
        ScalarOperand first;
        ScalarOperand second;
        if (!compile_scalar_expression((*arguments)[0], path + "/args/0", first) ||
            !compile_scalar_expression((*arguments)[1], path + "/args/1", second)) {
            return false;
        }
        const auto ordered = *predicate_operation != EmberActionIrPredicateOperation::Equal &&
            *predicate_operation != EmberActionIrPredicateOperation::NotEqual;
        if (!reconcile_comparison_operands(first, second, ordered, path)) return false;
        return append_predicate_instruction(output,
                   EmberActionIrPredicateOperation::PushValue, path,
                   std::move(first.value)) &&
            append_predicate_instruction(output,
                EmberActionIrPredicateOperation::PushValue, path,
                std::move(second.value)) &&
            append_predicate_instruction(output, *predicate_operation, path);
    }

    bool validate_predicate_stack(
        const EmberActionExecutableNode& output,
        const std::string& path) {
        std::size_t stack = 0U;
        std::size_t maximum = 0U;
        for (const auto& instruction : output.predicate) {
            if (instruction.operation == EmberActionIrPredicateOperation::PushValue) {
                ++stack;
                maximum = std::max(maximum, stack);
            } else if (instruction.operation == EmberActionIrPredicateOperation::Not) {
                if (stack < 1U) {
                    add("EA_EXEC_IR_PREDICATE_STACK", path,
                        "The compiled predicate stack is malformed.");
                    return false;
                }
            } else {
                if (stack < 2U) {
                    add("EA_EXEC_IR_PREDICATE_STACK", path,
                        "The compiled predicate stack is malformed.");
                    return false;
                }
                --stack;
            }
        }
        if (stack != 1U || maximum > kEmberActionExecutionMaximumExpressionStackDepth) {
            add("EA_EXEC_IR_PREDICATE_STACK", path,
                "The compiled predicate exceeds or violates the runtime stack contract.");
            return false;
        }
        return true;
    }

    [[nodiscard]] std::optional<std::uint16_t> resolve_node_reference(
        const JsonObject& node,
        std::string_view field,
        const std::string& path,
        bool required) {
        const auto* value = ember_action_object_find(node, field);
        if (value == nullptr) {
            if (required) {
                add("EA_EXEC_IR_NODE_REFERENCE", path + "/" + std::string(field),
                    "A required control-flow branch is missing.");
            }
            return std::nullopt;
        }
        const auto* id = value->as_string();
        const auto found = id == nullptr ? node_indices_.end() : node_indices_.find(*id);
        if (found == node_indices_.end()) {
            add("EA_EXEC_IR_NODE_REFERENCE", path + "/" + std::string(field),
                "A control-flow branch is absent from executable IR.");
            return std::nullopt;
        }
        return found->second;
    }

    void compile_if(
        const JsonObject& node,
        const std::string& path,
        EmberActionExecutableNode& output) {
        output.kind = EmberActionIrNodeKind::If;
        constexpr std::array allowed{
            std::string_view{"kind"}, std::string_view{"predicate"},
            std::string_view{"then"}, std::string_view{"else"},
            std::string_view{"metadata"}};
        if (!only_fields(node, allowed)) {
            add("EA_EXEC_IR_IF_PROPERTY", path,
                "If contains a property outside the executable schema subset.");
        }
        const auto* predicate = ember_action_object_find(node, "predicate");
        if (predicate == nullptr ||
            !compile_predicate(*predicate, path + "/predicate", output, 1U) ||
            !validate_predicate_stack(output, path + "/predicate")) {
            if (predicate == nullptr) {
                add("EA_EXEC_IR_PREDICATE_REQUIRED", path + "/predicate",
                    "If requires one bounded predicate.");
            }
        }
        const auto then_node = resolve_node_reference(node, "then", path, true);
        if (then_node.has_value()) output.then_node = *then_node;
        output.else_node = resolve_node_reference(node, "else", path, false);
    }

    [[nodiscard]] static bool same_switch_match(
        const EmberActionIrValue& first,
        const EmberActionIrValue& second) noexcept {
        if (first.kind != second.kind) return false;
        switch (first.kind) {
            case EmberActionRuntimeValueKind::Boolean:
                return first.boolean_value == second.boolean_value;
            case EmberActionRuntimeValueKind::Integer:
                return first.integer_value == second.integer_value;
            case EmberActionRuntimeValueKind::Number:
                return first.number_value == second.number_value;
            case EmberActionRuntimeValueKind::Enum:
            case EmberActionRuntimeValueKind::String:
            case EmberActionRuntimeValueKind::StableId:
            case EmberActionRuntimeValueKind::SemanticRole:
                return first.text == second.text;
        }
        return false;
    }

    void compile_switch(
        const JsonObject& node,
        const std::string& path,
        EmberActionExecutableNode& output) {
        output.kind = EmberActionIrNodeKind::Switch;
        constexpr std::array allowed{
            std::string_view{"kind"}, std::string_view{"value"},
            std::string_view{"cases"}, std::string_view{"default"},
            std::string_view{"metadata"}};
        if (!only_fields(node, allowed)) {
            add("EA_EXEC_IR_SWITCH_PROPERTY", path,
                "Switch contains a property outside the executable schema subset.");
        }
        const auto* selector = ember_action_object_find(node, "value");
        ScalarOperand selector_operand;
        if (selector == nullptr ||
            !compile_scalar_expression(*selector, path + "/value", selector_operand)) {
            if (selector == nullptr) {
                add("EA_EXEC_IR_SWITCH_VALUE_REQUIRED", path + "/value",
                    "Switch requires one bounded selector.");
            }
            return;
        }
        if (selector_operand.contract.kind != EmberActionValueKind::Enum &&
            selector_operand.contract.kind != EmberActionValueKind::Integer &&
            selector_operand.contract.kind != EmberActionValueKind::String) {
            add("EA_EXEC_IR_SWITCH_TYPE", path + "/value",
                "Switch supports only enum, integer, or string selectors.");
        }
        output.switch_value = std::move(selector_operand.value);
        const auto* cases = array_at(node, "cases");
        if (cases == nullptr || cases->empty() || cases->size() > 32U) {
            add("EA_EXEC_IR_SWITCH_CASES", path + "/cases",
                "Switch requires one to 32 bounded constant cases.");
        } else {
            output.switch_cases.reserve(cases->size());
            for (std::size_t index = 0U; index < cases->size(); ++index) {
                const auto case_path = path + "/cases/" + std::to_string(index);
                const auto* item = (*cases)[index].as_object();
                constexpr std::array case_allowed{
                    std::string_view{"match"}, std::string_view{"node"}};
                if (item == nullptr || !only_fields(*item, case_allowed)) {
                    add("EA_EXEC_IR_SWITCH_CASE_PROPERTY", case_path,
                        "A Switch case must contain only match and node.");
                    continue;
                }
                const auto* match = ember_action_object_find(*item, "match");
                const auto target = resolve_node_reference(*item, "node", case_path, true);
                EmberActionIrValue compiled_match;
                compiled_match.source = EmberActionIrValueSource::Literal;
                if (match == nullptr || !target.has_value() ||
                    !compile_literal(*match, selector_operand.contract,
                        case_path + "/match", compiled_match)) {
                    if (match == nullptr) {
                        add("EA_EXEC_IR_SWITCH_MATCH_REQUIRED", case_path + "/match",
                            "A Switch case requires one constant match value.");
                    }
                    continue;
                }
                const auto duplicate = std::any_of(
                    output.switch_cases.begin(), output.switch_cases.end(),
                    [&](const EmberActionIrSwitchCase& existing) {
                        return same_switch_match(existing.match, compiled_match);
                    });
                if (duplicate) {
                    add("EA_EXEC_IR_SWITCH_CASE_DUPLICATE", case_path + "/match",
                        "Switch case constants must be unique after typed normalization.");
                    continue;
                }
                output.switch_cases.push_back({std::move(compiled_match), *target});
            }
        }
        const auto switch_operations = 1U + output.switch_cases.size();
        if (switch_operations >
            kEmberActionExecutionMaximumExpressionOperations -
                std::min(compiled_expression_operations_,
                    kEmberActionExecutionMaximumExpressionOperations)) {
            add("EA_EXEC_IR_EXPRESSION_LIMIT", path + "/cases",
                "Compiled control-flow expressions exceed the platform operation limit.");
        } else {
            compiled_expression_operations_ += switch_operations;
        }
        output.default_node = resolve_node_reference(node, "default", path, false);
    }

    void compile_on_result(
        const JsonObject& node,
        const std::string& path,
        EmberActionExecutableNode& output) {
        output.kind = EmberActionIrNodeKind::OnResult;
        constexpr std::array allowed{
            std::string_view{"kind"}, std::string_view{"result"},
            std::string_view{"cases"}, std::string_view{"metadata"}};
        if (!only_fields(node, allowed)) {
            add("EA_EXEC_IR_ON_RESULT_PROPERTY", path,
                "OnResult contains a property outside the executable schema subset.");
        }

        const auto* expression = ember_action_object_find(node, "result");
        const auto* result_object = expression == nullptr ? nullptr : expression->as_object();
        const auto* source = result_object == nullptr
            ? nullptr : string_at(*result_object, "source");
        const auto* result_path = result_object == nullptr
            ? nullptr : string_at(*result_object, "path");
        const auto slot = result_path == nullptr
            ? result_slots_.end() : result_slots_.find(*result_path);
        if (result_object == nullptr || result_object->size() != 2U ||
            source == nullptr || *source != "nodeOutput" || slot == result_slots_.end()) {
            add("EA_EXEC_IR_ON_RESULT_SOURCE", path + "/result",
                "OnResult may reference only one compiled command result slot.");
        } else {
            output.on_result_slot = slot->second;
        }

        const auto* cases = object_at(node, "cases");
        if (cases == nullptr || cases->empty() || cases->size() > 13U) {
            add("EA_EXEC_IR_ON_RESULT_CASES", path + "/cases",
                "OnResult requires one to 13 schema result branches.");
            return;
        }
        output.on_result_cases.reserve(cases->size());
        for (const auto& [name, target_value] : *cases) {
            const auto case_path = path + "/cases/" + name;
            const auto* target_id = target_value.as_string();
            const auto target = target_id == nullptr
                ? node_indices_.end() : node_indices_.find(*target_id);
            if (target == node_indices_.end()) {
                add("EA_EXEC_IR_NODE_REFERENCE", case_path,
                    "An OnResult branch is absent from executable IR.");
                continue;
            }
            if (name == "default") {
                output.on_result_default_node = target->second;
                continue;
            }
            if (reserved_on_result_case(name)) {
                add("EA_EXEC_IR_ON_RESULT_CASE_RESERVED", case_path,
                    "This schema result has no existing native invocation-result representation.");
                continue;
            }
            if (hard_stop_on_result_case(name)) {
                add("EA_EXEC_IR_ON_RESULT_CASE_HARD_STOP", case_path,
                    "Queue, safety, and dispatcher faults remain hard stops and cannot route fallback.");
                continue;
            }
            const auto match = executable_on_result_case(name);
            if (!match.has_value()) {
                add("EA_EXEC_IR_ON_RESULT_CASE_UNKNOWN", case_path,
                    "This result name is not an executable OnResult case.");
                continue;
            }
            output.on_result_cases.push_back({*match, target->second});
        }
        std::sort(
            output.on_result_cases.begin(), output.on_result_cases.end(),
            [](const EmberActionIrResultCase& first,
               const EmberActionIrResultCase& second) {
                return static_cast<std::uint8_t>(first.match) <
                    static_cast<std::uint8_t>(second.match);
            });
        const auto operations = 1U + output.on_result_cases.size();
        if (operations >
            kEmberActionExecutionMaximumExpressionOperations -
                std::min(compiled_expression_operations_,
                    kEmberActionExecutionMaximumExpressionOperations)) {
            add("EA_EXEC_IR_EXPRESSION_LIMIT", path + "/cases",
                "Compiled result-flow expressions exceed the platform operation limit.");
        } else {
            compiled_expression_operations_ += operations;
        }
    }

    void compile_return(
        const JsonObject& node,
        const std::string& path,
        EmberActionExecutableNode& output) {
        output.kind = EmberActionIrNodeKind::Return;
        constexpr std::array allowed{
            std::string_view{"kind"}, std::string_view{"result"},
            std::string_view{"feedback"}, std::string_view{"metadata"}};
        if (!only_fields(node, allowed)) {
            add("EA_EXEC_IR_RETURN_PROPERTY", path,
                "Return contains a property outside the executable schema subset.");
        }
        if (ember_action_object_find(node, "feedback") != nullptr) {
            add("EA_EXEC_IR_RETURN_FEEDBACK_UNSUPPORTED", path + "/feedback",
                "Return feedback values are not executable in this bounded slice.");
        }
        const auto* expression = ember_action_object_find(node, "result");
        const auto* object = expression == nullptr ? nullptr : expression->as_object();
        if (object == nullptr) {
            add("EA_EXEC_IR_RETURN_EXPRESSION", path + "/result",
                "Return requires a typed invocation-result expression.");
            return;
        }
        if (const auto* literal = ember_action_object_find(*object, "literal")) {
            if (object->size() != 1U) {
                add("EA_EXEC_IR_RETURN_PROPERTY", path + "/result",
                    "A literal Return expression cannot contain additional properties.");
                return;
            }
            const auto* name = literal->as_string();
            const auto result = name == nullptr ? std::nullopt : invocation_result(*name);
            if (!result.has_value()) {
                add("EA_EXEC_IR_RETURN_RESULT", path + "/result",
                    "Return supports only existing native invocation results in this slice.");
            } else {
                output.return_value.source = EmberActionIrReturnSource::LiteralResult;
                output.return_value.literal = *result;
            }
            return;
        }
        const auto* source = string_at(*object, "source");
        const auto* value_path = string_at(*object, "path");
        const auto found = value_path == nullptr
            ? result_slots_.end() : result_slots_.find(*value_path);
        if (source == nullptr || *source != "nodeOutput" || found == result_slots_.end() ||
            object->size() != 2U) {
            add("EA_EXEC_IR_RETURN_SOURCE", path + "/result",
                "Return may reference only one compiled command result slot.");
            return;
        }
        output.return_value.source = EmberActionIrReturnSource::InvocationResult;
        output.return_value.result_slot = found->second;
    }

    void compile_entries() {
        const auto* entries = object_at(*root_, "entryPoints");
        if (entries == nullptr) return;
        for (const auto& [name, value] : *entries) {
            const auto point = entry_point(name);
            if (!point.has_value()) {
                add("EA_EXEC_IR_ENTRY_UNSUPPORTED", "/entryPoints/" + name,
                    "Activation/deactivation and unknown entry points are not executable here.");
                continue;
            }
            const auto* id = value.as_string();
            const auto found = id == nullptr ? node_indices_.end() : node_indices_.find(*id);
            if (found == node_indices_.end()) {
                add("EA_EXEC_IR_ENTRY_REFERENCE", "/entryPoints/" + name,
                    "The executable entry root is unresolved.");
                continue;
            }
            auto& entry = candidate_->entry_points[static_cast<std::size_t>(*point)];
            entry.present = true;
            entry.root_node = found->second;
        }
    }

    void validate_entries() {
        std::array<std::optional<Metrics>, kEmberActionExecutionMaximumNodes> memo{};
        std::array<bool, kEmberActionExecutionMaximumNodes> visiting{};
        bool any = false;
        for (std::size_t index = 0U; index < candidate_->entry_points.size(); ++index) {
            auto& entry = candidate_->entry_points[index];
            if (!entry.present) continue;
            any = true;
            const auto metrics = measure(entry.root_node, memo, visiting);
            if (!metrics.valid) continue;
            entry.maximum_node_visits = metrics.node_visits;
            entry.maximum_dispatches = metrics.dispatches;
            entry.maximum_depth = metrics.depth;
            entry.maximum_expression_operations = metrics.expression_operations;
            const auto path = "/entryPoints/" + std::to_string(index);
            if (metrics.node_visits > kEmberActionExecutionMaximumNodes) {
                add("EA_EXEC_IR_NODE_VISIT_LIMIT", path,
                    "The executable entry exceeds the node-visit budget.");
            }
            if (metrics.dispatches > kEmberActionExecutionMaximumDispatches) {
                add("EA_EXEC_IR_DISPATCH_LIMIT", path,
                    "The executable entry exceeds the dispatch budget.");
            }
            if (metrics.depth > kEmberActionExecutionMaximumDepth) {
                add("EA_EXEC_IR_DEPTH_LIMIT", path,
                    "The executable entry exceeds the runtime depth budget.");
            }
            if (metrics.expression_operations >
                kEmberActionExecutionMaximumExpressionOperations) {
                add("EA_EXEC_IR_EXPRESSION_LIMIT", path,
                    "The executable entry exceeds the expression-operation budget.");
            }
            std::bitset<kEmberActionExecutionMaximumResults> defined;
            std::array<bool, kEmberActionExecutionMaximumNodes> flow_visiting{};
            static_cast<void>(validate_result_flow(
                entry.root_node, defined, flow_visiting, path));
        }
        if (!any) {
            add("EA_EXEC_IR_ENTRY_REQUIRED", "/entryPoints",
                "At least one supported executable entry point is required.");
        }
    }

    Metrics measure(
        std::uint16_t index,
        std::array<std::optional<Metrics>, kEmberActionExecutionMaximumNodes>& memo,
        std::array<bool, kEmberActionExecutionMaximumNodes>& visiting) {
        if (index >= candidate_->nodes.size()) return {0U, 0U, 0U, 0U, false};
        if (memo[index].has_value()) return *memo[index];
        if (visiting[index]) {
            add("EA_EXEC_IR_CYCLE", "/nodes/" + candidate_->nodes[index].id,
                "Executable IR must remain acyclic.");
            return {0U, 0U, 0U, 0U, false};
        }
        visiting[index] = true;
        Metrics result{1U, 0U, 1U, 0U, true};
        const auto& node = candidate_->nodes[index];
        if (node.kind == EmberActionIrNodeKind::InvokeCommand) {
            result.dispatches = 1U;
        } else if (node.kind == EmberActionIrNodeKind::Sequence) {
            for (const auto child : node.children) {
                const auto child_metrics = measure(child, memo, visiting);
                if (!child_metrics.valid) result.valid = false;
                result.node_visits += child_metrics.node_visits;
                result.dispatches += child_metrics.dispatches;
                result.depth = std::max(result.depth, child_metrics.depth + 1U);
                result.expression_operations += child_metrics.expression_operations;
            }
        } else if (node.kind == EmberActionIrNodeKind::If ||
                   node.kind == EmberActionIrNodeKind::Switch ||
                   node.kind == EmberActionIrNodeKind::OnResult) {
            Metrics branch;
            const auto include_branch = [&](std::uint16_t child) {
                const auto child_metrics = measure(child, memo, visiting);
                if (!child_metrics.valid) branch.valid = false;
                branch.node_visits = std::max(
                    branch.node_visits, child_metrics.node_visits);
                branch.dispatches = std::max(
                    branch.dispatches, child_metrics.dispatches);
                branch.depth = std::max(branch.depth, child_metrics.depth);
                branch.expression_operations = std::max(
                    branch.expression_operations,
                    child_metrics.expression_operations);
            };
            if (node.kind == EmberActionIrNodeKind::If) {
                include_branch(node.then_node);
                if (node.else_node.has_value()) include_branch(*node.else_node);
                result.expression_operations = node.predicate.size();
            } else if (node.kind == EmberActionIrNodeKind::Switch) {
                for (const auto& item : node.switch_cases) {
                    include_branch(item.target_node);
                }
                if (node.default_node.has_value()) include_branch(*node.default_node);
                result.expression_operations = 1U + node.switch_cases.size();
            } else {
                for (const auto& item : node.on_result_cases) {
                    include_branch(item.target_node);
                }
                if (node.on_result_default_node.has_value()) {
                    include_branch(*node.on_result_default_node);
                }
                result.expression_operations = 1U + node.on_result_cases.size();
            }
            result.valid = branch.valid;
            result.node_visits += branch.node_visits;
            result.dispatches += branch.dispatches;
            result.depth = std::max(result.depth, branch.depth + 1U);
            result.expression_operations += branch.expression_operations;
        }
        visiting[index] = false;
        memo[index] = result;
        return result;
    }

    bool validate_result_flow(
        std::uint16_t index,
        std::bitset<kEmberActionExecutionMaximumResults>& defined,
        std::array<bool, kEmberActionExecutionMaximumNodes>& visiting,
        const std::string& path) {
        if (index >= candidate_->nodes.size()) return false;
        if (visiting[index]) return false;
        visiting[index] = true;
        const auto& node = candidate_->nodes[index];
        bool returns = false;
        if (node.kind == EmberActionIrNodeKind::InvokeCommand) {
            if (node.result_slot.has_value()) defined.set(*node.result_slot);
        } else if (node.kind == EmberActionIrNodeKind::Return) {
            if (node.return_value.source == EmberActionIrReturnSource::InvocationResult &&
                !defined.test(node.return_value.result_slot)) {
                add("EA_EXEC_IR_RESULT_BEFORE_DEFINITION", path + "/" + node.id,
                    "A command result must be produced before Return reads it.");
            }
            returns = true;
        } else if (node.kind == EmberActionIrNodeKind::Sequence) {
            for (std::size_t child_index = 0U; child_index < node.children.size(); ++child_index) {
                if (returns) {
                    add("EA_EXEC_IR_NODE_AFTER_RETURN", path + "/" + node.id,
                        "Sequence children after an unconditional Return are not executable.");
                    break;
                }
                returns = validate_result_flow(
                    node.children[child_index], defined, visiting, path);
            }
        } else {
            if (node.kind == EmberActionIrNodeKind::OnResult &&
                !defined.test(node.on_result_slot)) {
                add("EA_EXEC_IR_RESULT_BEFORE_DEFINITION", path + "/" + node.id,
                    "A command result must be produced before OnResult reads it.");
            }
            std::bitset<kEmberActionExecutionMaximumResults> continuing_definitions;
            bool has_continuing_path = false;
            bool every_path_returns = true;
            const auto validate_branch = [&](std::optional<std::uint16_t> target) {
                auto branch_definitions = defined;
                const auto branch_returns = target.has_value() && validate_result_flow(
                    *target, branch_definitions, visiting, path);
                every_path_returns = every_path_returns && branch_returns;
                if (!branch_returns) {
                    if (!has_continuing_path) {
                        continuing_definitions = branch_definitions;
                        has_continuing_path = true;
                    } else {
                        continuing_definitions &= branch_definitions;
                    }
                }
            };
            if (node.kind == EmberActionIrNodeKind::If) {
                validate_branch(node.then_node);
                validate_branch(node.else_node);
            } else if (node.kind == EmberActionIrNodeKind::Switch) {
                for (const auto& item : node.switch_cases) {
                    validate_branch(item.target_node);
                }
                validate_branch(node.default_node);
            } else {
                for (const auto& item : node.on_result_cases) {
                    validate_branch(item.target_node);
                }
                validate_branch(node.on_result_default_node);
            }
            if (has_continuing_path) defined = continuing_definitions;
            returns = every_path_returns;
        }
        visiting[index] = false;
        return returns;
    }

    [[nodiscard]] std::string make_execution_digest() const {
        std::string canonical{"ember-action-executable-ir-v3\n"};
        canonical.append("compiler=");
        canonical.append(std::to_string(kEmberActionExecutableIrCompilerGeneration));
        canonical.push_back('\n');
        canonical.append("foundation=");
        canonical.append(foundation_->cache_key.cache_digest);
        canonical.push_back('\n');
        return "sha256:" + sha256_text(canonical);
    }

    void add(std::string code, std::string path, std::string message) {
        add_diagnostic(diagnostics_, std::move(code), std::move(path), std::move(message));
    }

    std::shared_ptr<const EmberActionIrFoundation> foundation_;
    const EmberActionRegistryView& registry_;
    const JsonObject* root_{nullptr};
    const JsonObject* nodes_{nullptr};
    std::shared_ptr<EmberActionExecutableIr> candidate_;
    std::map<std::string, std::uint16_t, EmberActionUtf8ByteLess> parameter_indices_;
    std::map<std::string, std::uint16_t, EmberActionUtf8ByteLess> node_indices_;
    std::map<std::string, std::uint16_t, EmberActionUtf8ByteLess> result_slots_;
    std::size_t compiled_expression_operations_{0U};
    bool expression_limit_reported_{false};
    std::vector<EmberActionDiagnostic> diagnostics_;
};

[[nodiscard]] bool valid_invocation_result(UiInvocationResult result) noexcept {
    return static_cast<std::uint8_t>(result) <=
        static_cast<std::uint8_t>(UiInvocationResult::InternalError);
}

[[nodiscard]] bool valid_on_result_match(UiInvocationResult result) noexcept {
    return result == UiInvocationResult::Accepted ||
        result == UiInvocationResult::NoChange ||
        result == UiInvocationResult::Unavailable ||
        result == UiInvocationResult::InvalidArguments ||
        result == UiInvocationResult::ValidationFailed ||
        result == UiInvocationResult::Unsupported;
}

[[nodiscard]] bool rejected(UiInvocationResult result) noexcept {
    return result == UiInvocationResult::Unavailable ||
        result == UiInvocationResult::InvalidArguments ||
        result == UiInvocationResult::NotFound ||
        result == UiInvocationResult::ValidationFailed ||
        result == UiInvocationResult::QueueFull ||
        result == UiInvocationResult::SafetyRejected ||
        result == UiInvocationResult::Unsupported;
}

[[nodiscard]] bool should_stop(
    EmberActionSequencePolicy policy,
    UiInvocationResult result) noexcept {
    // Queue pressure and safety rejection are never converted into an implicit
    // retry/flood by a permissive Sequence policy or an OnResult branch.
    if (result == UiInvocationResult::QueueFull ||
        result == UiInvocationResult::SafetyRejected) {
        return true;
    }
    switch (policy) {
        case EmberActionSequencePolicy::Continue:
            return false;
        case EmberActionSequencePolicy::StopOnRejected:
            return rejected(result);
        case EmberActionSequencePolicy::StopOnError:
            return result == UiInvocationResult::InternalError;
        case EmberActionSequencePolicy::StopOnAnyNonAccepted:
            return result != UiInvocationResult::Accepted;
    }
    return true;
}

[[nodiscard]] bool value_matches(
    const EmberActionRuntimeValue& value,
    const EmberActionValueContract& expected) noexcept {
    const auto text_matches = [&](EmberActionRuntimeValueKind kind) {
        if (value.kind != kind) return false;
        const auto maximum = expected.maximum_string_bytes == 0U
            ? kEmberActionExecutionMaximumTextBytes
            : std::min(expected.maximum_string_bytes,
                kEmberActionExecutionMaximumTextBytes);
        return value.text.size() <= maximum;
    };
    switch (expected.kind) {
        case EmberActionValueKind::Boolean:
            return value.kind == EmberActionRuntimeValueKind::Boolean;
        case EmberActionValueKind::Integer: {
            if (value.kind != EmberActionRuntimeValueKind::Integer) return false;
            const auto numeric = static_cast<double>(value.integer_value);
            return (!expected.minimum.has_value() || numeric >= *expected.minimum) &&
                (!expected.maximum.has_value() || numeric <= *expected.maximum);
        }
        case EmberActionValueKind::Number: {
            const auto numeric = value.kind == EmberActionRuntimeValueKind::Integer
                ? static_cast<double>(value.integer_value) : value.number_value;
            if (value.kind != EmberActionRuntimeValueKind::Integer &&
                value.kind != EmberActionRuntimeValueKind::Number) return false;
            return std::isfinite(numeric) &&
                (!expected.minimum.has_value() || numeric >= *expected.minimum) &&
                (!expected.maximum.has_value() || numeric <= *expected.maximum);
        }
        case EmberActionValueKind::Enum:
            return text_matches(EmberActionRuntimeValueKind::Enum) &&
                std::find(expected.enum_values.begin(), expected.enum_values.end(), value.text) !=
                    expected.enum_values.end();
        case EmberActionValueKind::String:
            return text_matches(EmberActionRuntimeValueKind::String);
        case EmberActionValueKind::StableId:
            return text_matches(EmberActionRuntimeValueKind::StableId) && !value.text.empty();
        case EmberActionValueKind::SemanticRole:
            return text_matches(EmberActionRuntimeValueKind::SemanticRole) && !value.text.empty();
        default:
            return false;
    }
}

struct ExecutionOutcome {
    bool returned{false};
    std::optional<UiInvocationResult> result;
};

class ExecutionRuntime {
public:
    ExecutionRuntime(
        const EmberActionExecutableIr& ir,
        const EmberActionExecutionRequest& request,
        EmberActionCommandControl& control,
        EmberActionExecutionLimits limits,
        EmberActionExecutionResult& result) noexcept
        : ir_(ir), request_(request), control_(control), limits_(limits), result_(result) {}

    [[nodiscard]] ExecutionOutcome execute(std::uint16_t root) noexcept {
        return execute_node(root, 1U);
    }

    [[nodiscard]] std::optional<UiInvocationResult> sticky_result() const noexcept {
        return sticky_result_;
    }

private:
    [[nodiscard]] ExecutionOutcome execute_node(
        std::uint16_t index,
        std::size_t depth) noexcept {
        if (index >= ir_.nodes.size()) {
            result_.status = EmberActionExecutionStatus::InvalidIr;
            return {};
        }
        if (depth > limits_.maximum_depth || depth > kEmberActionExecutionMaximumDepth) {
            result_.status = EmberActionExecutionStatus::DepthBudgetExceeded;
            trace(index, depth, EmberActionTraceEvent::BudgetRejected, std::nullopt);
            return {};
        }
        if (result_.node_visits >= limits_.maximum_node_visits ||
            result_.node_visits >= kEmberActionExecutionMaximumNodes) {
            result_.status = EmberActionExecutionStatus::NodeBudgetExceeded;
            trace(index, depth, EmberActionTraceEvent::BudgetRejected, std::nullopt);
            return {};
        }
        ++result_.node_visits;
        trace(index, depth, EmberActionTraceEvent::NodeEntered, std::nullopt);
        const auto& node = ir_.nodes[index];
        ExecutionOutcome outcome;
        switch (node.kind) {
            case EmberActionIrNodeKind::Sequence:
                outcome = execute_sequence(node, index, depth);
                break;
            case EmberActionIrNodeKind::InvokeCommand:
                outcome = execute_command(node, index, depth);
                break;
            case EmberActionIrNodeKind::If:
                outcome = execute_if(node, index, depth);
                break;
            case EmberActionIrNodeKind::Switch:
                outcome = execute_switch(node, index, depth);
                break;
            case EmberActionIrNodeKind::OnResult:
                outcome = execute_on_result(node, index, depth);
                break;
            case EmberActionIrNodeKind::Return:
                outcome = execute_return(node);
                break;
        }
        trace(index, depth, EmberActionTraceEvent::NodeExited, outcome.result);
        return outcome;
    }

    [[nodiscard]] ExecutionOutcome execute_sequence(
        const EmberActionExecutableNode& node,
        std::uint16_t,
        std::size_t depth) noexcept {
        ExecutionOutcome aggregate;
        for (const auto child : node.children) {
            const auto outcome = execute_node(child, depth + 1U);
            if (result_.status != EmberActionExecutionStatus::Completed) return outcome;
            if (outcome.result.has_value()) aggregate.result = outcome.result;
            if (outcome.returned) return outcome;
            if (outcome.result.has_value() &&
                should_stop(node.sequence_policy, *outcome.result)) {
                return aggregate;
            }
        }
        return aggregate;
    }

    [[nodiscard]] bool consume_expression_operation(
        std::uint16_t index,
        std::size_t depth) noexcept {
        if (result_.expression_operations >= limits_.maximum_expression_operations ||
            result_.expression_operations >=
                kEmberActionExecutionMaximumExpressionOperations) {
            result_.status = EmberActionExecutionStatus::ExpressionBudgetExceeded;
            trace(index, depth, EmberActionTraceEvent::BudgetRejected, std::nullopt);
            return false;
        }
        ++result_.expression_operations;
        return true;
    }

    [[nodiscard]] static std::optional<int> compare_scalar_values(
        const EmberActionRuntimeValue& first,
        const EmberActionRuntimeValue& second,
        bool ordered) noexcept {
        if (first.kind != second.kind) return std::nullopt;
        switch (first.kind) {
            case EmberActionRuntimeValueKind::Boolean:
                if (ordered) return std::nullopt;
                if (first.boolean_value == second.boolean_value) return 0;
                return first.boolean_value ? 1 : -1;
            case EmberActionRuntimeValueKind::Integer:
                if (first.integer_value == second.integer_value) return 0;
                return first.integer_value < second.integer_value ? -1 : 1;
            case EmberActionRuntimeValueKind::Number:
                if (!std::isfinite(first.number_value) ||
                    !std::isfinite(second.number_value)) {
                    return std::nullopt;
                }
                if (first.number_value == second.number_value) return 0;
                return first.number_value < second.number_value ? -1 : 1;
            case EmberActionRuntimeValueKind::Enum:
            case EmberActionRuntimeValueKind::String:
            case EmberActionRuntimeValueKind::StableId:
            case EmberActionRuntimeValueKind::SemanticRole:
                if (ordered) return std::nullopt;
                if (first.text == second.text) return 0;
                return first.text < second.text ? -1 : 1;
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<bool> evaluate_predicate(
        const EmberActionExecutableNode& node,
        std::uint16_t index,
        std::size_t depth) noexcept {
        std::array<EmberActionRuntimeValue,
            kEmberActionExecutionMaximumExpressionStackDepth> stack{};
        std::size_t stack_size = 0U;
        if (node.predicate.empty() ||
            node.predicate.size() > kEmberActionExecutionMaximumExpressionOperations) {
            result_.status = EmberActionExecutionStatus::InvalidIr;
            return std::nullopt;
        }
        for (const auto& instruction : node.predicate) {
            if (!consume_expression_operation(index, depth)) return std::nullopt;
            if (instruction.operation == EmberActionIrPredicateOperation::PushValue) {
                if (stack_size >= stack.size()) {
                    result_.status = EmberActionExecutionStatus::InvalidIr;
                    return std::nullopt;
                }
                const auto value = resolve_value(instruction.value);
                if (!value.has_value()) {
                    result_.status = EmberActionExecutionStatus::InvalidContext;
                    return std::nullopt;
                }
                stack[stack_size++] = *value;
                continue;
            }
            if (instruction.operation == EmberActionIrPredicateOperation::Not) {
                if (stack_size < 1U ||
                    stack[stack_size - 1U].kind !=
                        EmberActionRuntimeValueKind::Boolean) {
                    result_.status = EmberActionExecutionStatus::InvalidIr;
                    return std::nullopt;
                }
                stack[stack_size - 1U].boolean_value =
                    !stack[stack_size - 1U].boolean_value;
                continue;
            }
            if (stack_size < 2U) {
                result_.status = EmberActionExecutionStatus::InvalidIr;
                return std::nullopt;
            }
            const auto second = stack[--stack_size];
            const auto first = stack[--stack_size];
            bool value = false;
            if (instruction.operation == EmberActionIrPredicateOperation::And ||
                instruction.operation == EmberActionIrPredicateOperation::Or) {
                if (first.kind != EmberActionRuntimeValueKind::Boolean ||
                    second.kind != EmberActionRuntimeValueKind::Boolean) {
                    result_.status = EmberActionExecutionStatus::InvalidIr;
                    return std::nullopt;
                }
                value = instruction.operation == EmberActionIrPredicateOperation::And
                    ? first.boolean_value && second.boolean_value
                    : first.boolean_value || second.boolean_value;
            } else {
                const auto ordered =
                    instruction.operation != EmberActionIrPredicateOperation::Equal &&
                    instruction.operation != EmberActionIrPredicateOperation::NotEqual;
                const auto comparison = compare_scalar_values(first, second, ordered);
                if (!comparison.has_value()) {
                    result_.status = EmberActionExecutionStatus::InvalidIr;
                    return std::nullopt;
                }
                switch (instruction.operation) {
                    case EmberActionIrPredicateOperation::Equal:
                        value = *comparison == 0;
                        break;
                    case EmberActionIrPredicateOperation::NotEqual:
                        value = *comparison != 0;
                        break;
                    case EmberActionIrPredicateOperation::Less:
                        value = *comparison < 0;
                        break;
                    case EmberActionIrPredicateOperation::LessOrEqual:
                        value = *comparison <= 0;
                        break;
                    case EmberActionIrPredicateOperation::Greater:
                        value = *comparison > 0;
                        break;
                    case EmberActionIrPredicateOperation::GreaterOrEqual:
                        value = *comparison >= 0;
                        break;
                    case EmberActionIrPredicateOperation::PushValue:
                    case EmberActionIrPredicateOperation::And:
                    case EmberActionIrPredicateOperation::Or:
                    case EmberActionIrPredicateOperation::Not:
                        result_.status = EmberActionExecutionStatus::InvalidIr;
                        return std::nullopt;
                }
            }
            stack[stack_size++] = EmberActionRuntimeValue::boolean(value);
        }
        if (stack_size != 1U ||
            stack[0].kind != EmberActionRuntimeValueKind::Boolean) {
            result_.status = EmberActionExecutionStatus::InvalidIr;
            return std::nullopt;
        }
        return stack[0].boolean_value;
    }

    [[nodiscard]] ExecutionOutcome execute_if(
        const EmberActionExecutableNode& node,
        std::uint16_t index,
        std::size_t depth) noexcept {
        const auto predicate = evaluate_predicate(node, index, depth);
        if (!predicate.has_value()) return {};
        const auto target = *predicate
            ? std::optional<std::uint16_t>{node.then_node}
            : node.else_node;
        trace(index, depth, EmberActionTraceEvent::BranchSelected, std::nullopt,
            target, *predicate ? 0U : 1U);
        return target.has_value() ? execute_node(*target, depth + 1U)
                                  : ExecutionOutcome{};
    }

    [[nodiscard]] ExecutionOutcome execute_switch(
        const EmberActionExecutableNode& node,
        std::uint16_t index,
        std::size_t depth) noexcept {
        if (node.switch_cases.empty() || node.switch_cases.size() > 32U ||
            !consume_expression_operation(index, depth)) {
            if (node.switch_cases.empty() || node.switch_cases.size() > 32U) {
                result_.status = EmberActionExecutionStatus::InvalidIr;
            }
            return {};
        }
        const auto selector = resolve_value(node.switch_value);
        if (!selector.has_value()) {
            result_.status = EmberActionExecutionStatus::InvalidContext;
            return {};
        }
        std::optional<std::uint16_t> target;
        std::size_t branch_index = node.switch_cases.size();
        for (std::size_t case_index = 0U; case_index < node.switch_cases.size();
             ++case_index) {
            if (!consume_expression_operation(index, depth)) return {};
            const auto match = resolve_value(node.switch_cases[case_index].match);
            const auto equal = match.has_value()
                ? compare_scalar_values(*selector, *match, false)
                : std::nullopt;
            if (!equal.has_value()) {
                result_.status = EmberActionExecutionStatus::InvalidIr;
                return {};
            }
            if (*equal == 0) {
                target = node.switch_cases[case_index].target_node;
                branch_index = case_index;
                break;
            }
        }
        if (!target.has_value()) target = node.default_node;
        trace(index, depth, EmberActionTraceEvent::BranchSelected, std::nullopt,
            target, static_cast<std::uint16_t>(branch_index));
        return target.has_value() ? execute_node(*target, depth + 1U)
                                  : ExecutionOutcome{};
    }

    [[nodiscard]] ExecutionOutcome execute_on_result(
        const EmberActionExecutableNode& node,
        std::uint16_t index,
        std::size_t depth) noexcept {
        if (node.on_result_cases.size() > 6U ||
            (!node.on_result_default_node.has_value() &&
             node.on_result_cases.empty()) ||
            !consume_expression_operation(index, depth)) {
            if (node.on_result_cases.size() > 6U ||
                (!node.on_result_default_node.has_value() &&
                 node.on_result_cases.empty())) {
                result_.status = EmberActionExecutionStatus::InvalidIr;
            }
            return {};
        }
        if (node.on_result_slot >= result_values_.size() ||
            !result_present_[node.on_result_slot]) {
            result_.status = EmberActionExecutionStatus::InvalidIr;
            return {};
        }
        const auto source_result = result_values_[node.on_result_slot];
        if (source_result == UiInvocationResult::InternalError) {
            result_.status = EmberActionExecutionStatus::DispatcherFault;
            return {false, source_result};
        }
        if (source_result == UiInvocationResult::QueueFull ||
            source_result == UiInvocationResult::SafetyRejected) {
            return {false, source_result};
        }

        std::optional<std::uint16_t> target;
        std::size_t branch_index = node.on_result_cases.size();
        for (std::size_t case_index = 0U;
             case_index < node.on_result_cases.size(); ++case_index) {
            if (!consume_expression_operation(index, depth)) return {};
            if (node.on_result_cases[case_index].match == source_result) {
                target = node.on_result_cases[case_index].target_node;
                branch_index = case_index;
                break;
            }
        }
        if (!target.has_value()) target = node.on_result_default_node;
        trace(index, depth, EmberActionTraceEvent::BranchSelected, source_result,
            target, static_cast<std::uint16_t>(branch_index));
        return target.has_value() ? execute_node(*target, depth + 1U)
                                  : ExecutionOutcome{false, source_result};
    }

    [[nodiscard]] ExecutionOutcome execute_command(
        const EmberActionExecutableNode& node,
        std::uint16_t index,
        std::size_t depth) noexcept {
        if (request_.cancellation.is_cancelled()) {
            result_.status = EmberActionExecutionStatus::Cancelled;
            trace(index, depth, EmberActionTraceEvent::CancelledBeforeDispatch, std::nullopt);
            return {};
        }
        if (result_.dispatches >= limits_.maximum_dispatches ||
            result_.dispatches >= kEmberActionExecutionMaximumDispatches) {
            result_.status = EmberActionExecutionStatus::DispatchBudgetExceeded;
            trace(index, depth, EmberActionTraceEvent::BudgetRejected, std::nullopt);
            return {};
        }
        if (node.arguments.size() > argument_values_.size()) {
            result_.status = EmberActionExecutionStatus::InvalidIr;
            return {};
        }
        if (static_cast<std::size_t>(node.command) >= kUiCommandDefinitions.size()) {
            result_.status = EmberActionExecutionStatus::InvalidIr;
            return {};
        }
        std::size_t argument_count = 0U;
        for (const auto& argument : node.arguments) {
            const auto value = resolve_value(argument.value);
            if (!value.has_value() || !value_matches(*value, argument.expected)) {
                result_.status = EmberActionExecutionStatus::InvalidContext;
                return {};
            }
            argument_values_[argument_count++] = {argument.name, *value};
        }
        if (request_.cancellation.is_cancelled()) {
            result_.status = EmberActionExecutionStatus::Cancelled;
            trace(index, depth, EmberActionTraceEvent::CancelledBeforeDispatch, std::nullopt);
            return {};
        }
        trace(index, depth, EmberActionTraceEvent::DispatchStarted, std::nullopt);
        ++result_.dispatches;
        const EmberActionCommandInvocationView invocation{
            node.command,
            ir_.execution_digest,
            index,
            std::span<const EmberActionCommandArgumentValue>(
                argument_values_.data(), argument_count)};
        auto invocation_result = control_.invoke(invocation);
        if (!valid_invocation_result(invocation_result)) {
            invocation_result = UiInvocationResult::InternalError;
        }
        trace(index, depth, EmberActionTraceEvent::DispatchCompleted, invocation_result);
        remember_failure(invocation_result);
        if (node.result_slot.has_value()) {
            if (*node.result_slot >= result_values_.size() ||
                *node.result_slot >= limits_.maximum_results) {
                result_.status = EmberActionExecutionStatus::ResultBudgetExceeded;
                return {false, invocation_result};
            }
            result_values_[*node.result_slot] = invocation_result;
            result_present_[*node.result_slot] = true;
        }
        if (invocation_result == UiInvocationResult::InternalError) {
            result_.status = EmberActionExecutionStatus::DispatcherFault;
        }
        return {false, invocation_result};
    }

    [[nodiscard]] ExecutionOutcome execute_return(
        const EmberActionExecutableNode& node) noexcept {
        if (node.return_value.source == EmberActionIrReturnSource::LiteralResult) {
            return {true, node.return_value.literal};
        }
        const auto slot = node.return_value.result_slot;
        if (slot >= result_values_.size() || !result_present_[slot]) {
            result_.status = EmberActionExecutionStatus::InvalidIr;
            return {};
        }
        return {true, result_values_[slot]};
    }

    [[nodiscard]] std::optional<EmberActionRuntimeValue> resolve_value(
        const EmberActionIrValue& value) const noexcept {
        if (value.source == EmberActionIrValueSource::Parameter) {
            if (value.parameter_index >= request_.parameters.size()) return std::nullopt;
            const auto runtime = request_.parameters[value.parameter_index];
            if (value.kind == EmberActionRuntimeValueKind::Number &&
                runtime.kind == EmberActionRuntimeValueKind::Integer) {
                return EmberActionRuntimeValue::number(
                    static_cast<double>(runtime.integer_value));
            }
            return runtime;
        }
        switch (value.kind) {
            case EmberActionRuntimeValueKind::Boolean:
                return EmberActionRuntimeValue::boolean(value.boolean_value);
            case EmberActionRuntimeValueKind::Integer:
                return EmberActionRuntimeValue::integer(value.integer_value);
            case EmberActionRuntimeValueKind::Number:
                return EmberActionRuntimeValue::number(value.number_value);
            case EmberActionRuntimeValueKind::Enum:
            case EmberActionRuntimeValueKind::String:
            case EmberActionRuntimeValueKind::StableId:
            case EmberActionRuntimeValueKind::SemanticRole:
                return EmberActionRuntimeValue::text_value(value.kind, value.text);
        }
        return std::nullopt;
    }

    void remember_failure(UiInvocationResult value) noexcept {
        if (!rejected(value)) return;
        if (!sticky_result_.has_value() || value == UiInvocationResult::SafetyRejected) {
            sticky_result_ = value;
        }
    }

    void trace(
        std::uint16_t node,
        std::size_t depth,
        EmberActionTraceEvent event,
        std::optional<UiInvocationResult> value,
        std::optional<std::uint16_t> branch_target = std::nullopt,
        std::uint16_t branch_index = 0U) noexcept {
        const auto sequence = trace_sequence_++;
        if (result_.trace_count >= limits_.maximum_trace_entries ||
            result_.trace_count >= result_.trace.size()) {
            ++result_.trace_dropped;
            return;
        }
        auto& entry = result_.trace[result_.trace_count++];
        entry.sequence = sequence;
        entry.node_index = node;
        entry.depth = static_cast<std::uint8_t>(std::min<std::size_t>(depth, 255U));
        entry.event = event;
        entry.has_result = value.has_value();
        entry.result = value.value_or(UiInvocationResult::Accepted);
        entry.has_branch_target = branch_target.has_value();
        entry.branch_target = branch_target.value_or(0U);
        entry.branch_index = branch_index;
    }

    const EmberActionExecutableIr& ir_;
    const EmberActionExecutionRequest& request_;
    EmberActionCommandControl& control_;
    EmberActionExecutionLimits limits_;
    EmberActionExecutionResult& result_;
    std::array<EmberActionCommandArgumentValue,
        kEmberActionExecutionMaximumArguments> argument_values_{};
    std::array<UiInvocationResult, kEmberActionExecutionMaximumResults> result_values_{};
    std::array<bool, kEmberActionExecutionMaximumResults> result_present_{};
    std::optional<UiInvocationResult> sticky_result_;
    std::uint32_t trace_sequence_{0U};
};

[[nodiscard]] EmberActionExecutionLimits effective_limits(
    const EmberActionExecutionLimits& requested) noexcept {
    return {
        std::min(requested.maximum_node_visits, kEmberActionExecutionMaximumNodes),
        std::min(requested.maximum_dispatches, kEmberActionExecutionMaximumDispatches),
        std::min(requested.maximum_depth, kEmberActionExecutionMaximumDepth),
        std::min(requested.maximum_expression_operations,
            kEmberActionExecutionMaximumExpressionOperations),
        std::min(requested.maximum_trace_entries, kEmberActionExecutionMaximumTraceEntries),
        std::min(requested.maximum_results, kEmberActionExecutionMaximumResults)};
}

[[nodiscard]] bool valid_ir_value(
    const EmberActionIrValue& value,
    const EmberActionExecutableIr& ir,
    bool literal_required = false) noexcept {
    if (value.source == EmberActionIrValueSource::Parameter) {
        if (literal_required || value.parameter_index >= ir.parameters.size()) return false;
        const auto expected = runtime_kind(ir.parameters[value.parameter_index].value.kind);
        return expected.has_value() && *expected == value.kind;
    }
    switch (value.kind) {
        case EmberActionRuntimeValueKind::Boolean:
        case EmberActionRuntimeValueKind::Integer:
            return true;
        case EmberActionRuntimeValueKind::Number:
            return std::isfinite(value.number_value);
        case EmberActionRuntimeValueKind::Enum:
        case EmberActionRuntimeValueKind::String:
        case EmberActionRuntimeValueKind::StableId:
        case EmberActionRuntimeValueKind::SemanticRole:
            return value.text.size() <= kEmberActionExecutionMaximumTextBytes;
    }
    return false;
}

class IrSealBuilder {
public:
    void byte(std::uint8_t value) noexcept {
        hash_ ^= value;
        hash_ *= 1099511628211ULL;
    }

    template <typename Integer>
    void integer(Integer value) noexcept {
        using Unsigned = std::make_unsigned_t<Integer>;
        static_assert(sizeof(Unsigned) <= sizeof(std::uintmax_t));
        auto bits = static_cast<std::uintmax_t>(static_cast<Unsigned>(value));
        for (std::size_t index = 0U; index < sizeof(Unsigned); ++index) {
            byte(static_cast<std::uint8_t>(bits & 0xffU));
            bits >>= 8U;
        }
    }

    void boolean(bool value) noexcept { byte(value ? 1U : 0U); }

    void text(std::string_view value) noexcept {
        integer(value.size());
        for (const auto character : value) {
            byte(static_cast<std::uint8_t>(character));
        }
    }

    void number(double value) noexcept {
        integer(std::bit_cast<std::uint64_t>(value));
    }

    [[nodiscard]] std::uint64_t finish() const noexcept { return hash_; }

private:
    std::uint64_t hash_{14695981039346656037ULL};
};

void seal_value(IrSealBuilder& seal, const EmberActionIrValue& value) noexcept {
    seal.integer(value.source);
    seal.integer(value.kind);
    seal.boolean(value.boolean_value);
    seal.integer(value.integer_value);
    seal.number(value.number_value);
    seal.text(value.text);
    seal.integer(value.parameter_index);
}

void seal_contract(
    IrSealBuilder& seal,
    const EmberActionValueContract& value) noexcept {
    seal.integer(value.kind);
    seal.text(value.unit);
    seal.boolean(value.minimum.has_value());
    if (value.minimum.has_value()) seal.number(*value.minimum);
    seal.boolean(value.maximum.has_value());
    if (value.maximum.has_value()) seal.number(*value.maximum);
    seal.integer(value.enum_values.size());
    for (const auto& item : value.enum_values) seal.text(item);
    seal.text(value.target_kind);
    seal.text(value.schema_ref);
    seal.integer(value.maximum_items);
    seal.integer(value.maximum_string_bytes);
}

[[nodiscard]] std::uint64_t executable_ir_structural_seal(
    const EmberActionExecutableIr& ir) noexcept {
    IrSealBuilder seal;
    seal.text("ember-action-executable-ir-structural-seal-v2");
    seal.integer(kEmberActionExecutableIrCompilerGeneration);
    seal.boolean(ir.foundation != nullptr);
    if (ir.foundation != nullptr) {
        seal.text(ir.foundation->cache_key.cache_digest);
        seal.text(ir.foundation->cache_key.source_hash);
        seal.text(ir.foundation->cache_key.registry_digest);
        seal.text(ir.foundation->cache_key.dependency_digest);
        seal.boolean(ir.foundation->prepared != nullptr);
        if (ir.foundation->prepared != nullptr) {
            seal.text(ir.foundation->prepared->content_hash);
            seal.text(ir.foundation->prepared->dependencies.registry_digest);
        }
    }
    seal.text(ir.execution_digest);
    seal.integer(ir.parameters.size());
    for (const auto& parameter : ir.parameters) {
        seal.text(parameter.name);
        seal_contract(seal, parameter.value);
    }
    seal.integer(ir.nodes.size());
    for (const auto& node : ir.nodes) {
        seal.text(node.id);
        seal.integer(node.kind);
        seal.integer(node.sequence_policy);
        seal.integer(node.children.size());
        for (const auto child : node.children) seal.integer(child);
        seal.integer(node.command);
        seal.integer(node.arguments.size());
        for (const auto& argument : node.arguments) {
            seal.text(argument.name);
            seal_contract(seal, argument.expected);
            seal_value(seal, argument.value);
        }
        seal.boolean(node.result_slot.has_value());
        if (node.result_slot.has_value()) seal.integer(*node.result_slot);
        seal.integer(node.predicate.size());
        for (const auto& instruction : node.predicate) {
            seal.integer(instruction.operation);
            seal_value(seal, instruction.value);
        }
        seal.integer(node.then_node);
        seal.boolean(node.else_node.has_value());
        if (node.else_node.has_value()) seal.integer(*node.else_node);
        seal_value(seal, node.switch_value);
        seal.integer(node.switch_cases.size());
        for (const auto& item : node.switch_cases) {
            seal_value(seal, item.match);
            seal.integer(item.target_node);
        }
        seal.boolean(node.default_node.has_value());
        if (node.default_node.has_value()) seal.integer(*node.default_node);
        seal.integer(node.on_result_slot);
        seal.integer(node.on_result_cases.size());
        for (const auto& item : node.on_result_cases) {
            seal.integer(item.match);
            seal.integer(item.target_node);
        }
        seal.boolean(node.on_result_default_node.has_value());
        if (node.on_result_default_node.has_value()) {
            seal.integer(*node.on_result_default_node);
        }
        seal.integer(node.return_value.source);
        seal.integer(node.return_value.literal);
        seal.integer(node.return_value.result_slot);
    }
    seal.integer(ir.entry_points.size());
    for (const auto& entry : ir.entry_points) {
        seal.boolean(entry.present);
        seal.integer(entry.root_node);
        seal.integer(entry.maximum_node_visits);
        seal.integer(entry.maximum_dispatches);
        seal.integer(entry.maximum_depth);
        seal.integer(entry.maximum_expression_operations);
    }
    seal.integer(ir.result_slot_count);
    return seal.finish();
}

struct IrShapeMetrics {
    std::size_t node_visits{0U};
    std::size_t dispatches{0U};
    std::size_t depth{0U};
    std::size_t expression_operations{0U};

    [[nodiscard]] bool operator==(const IrShapeMetrics&) const noexcept = default;
};

[[nodiscard]] bool add_bounded(
    std::size_t& value,
    std::size_t increment,
    std::size_t limit) noexcept {
    if (increment > limit - std::min(value, limit)) return false;
    value += increment;
    return true;
}

[[nodiscard]] std::optional<IrShapeMetrics> recompute_ir_metrics(
    const EmberActionExecutableIr& ir,
    std::uint16_t index,
    std::array<std::optional<IrShapeMetrics>,
        kEmberActionExecutionMaximumNodes>& memo,
    std::array<bool, kEmberActionExecutionMaximumNodes>& visiting) noexcept {
    if (index >= ir.nodes.size() || visiting[index]) return std::nullopt;
    if (memo[index].has_value()) return memo[index];
    visiting[index] = true;
    const auto finish = [&](std::optional<IrShapeMetrics> result) {
        visiting[index] = false;
        if (result.has_value()) memo[index] = *result;
        return result;
    };

    IrShapeMetrics result{1U, 0U, 1U, 0U};
    const auto& node = ir.nodes[index];
    if (node.kind == EmberActionIrNodeKind::InvokeCommand) {
        result.dispatches = 1U;
        return finish(result);
    }
    if (node.kind == EmberActionIrNodeKind::Return) return finish(result);

    const auto append_sequence = [&](std::uint16_t child) {
        const auto metrics = recompute_ir_metrics(ir, child, memo, visiting);
        if (!metrics.has_value()) return false;
        if (!add_bounded(result.node_visits, metrics->node_visits,
                kEmberActionExecutionMaximumNodes) ||
            !add_bounded(result.dispatches, metrics->dispatches,
                kEmberActionExecutionMaximumDispatches) ||
            !add_bounded(result.expression_operations,
                metrics->expression_operations,
                kEmberActionExecutionMaximumExpressionOperations)) {
            return false;
        }
        result.depth = std::max(result.depth, metrics->depth + 1U);
        return result.depth <= kEmberActionExecutionMaximumDepth;
    };
    if (node.kind == EmberActionIrNodeKind::Sequence) {
        for (const auto child : node.children) {
            if (!append_sequence(child)) return finish(std::nullopt);
        }
        return finish(result);
    }

    IrShapeMetrics branch;
    const auto include_branch = [&](std::optional<std::uint16_t> child) {
        if (!child.has_value()) return true;
        const auto metrics = recompute_ir_metrics(ir, *child, memo, visiting);
        if (!metrics.has_value()) return false;
        branch.node_visits = std::max(branch.node_visits, metrics->node_visits);
        branch.dispatches = std::max(branch.dispatches, metrics->dispatches);
        branch.depth = std::max(branch.depth, metrics->depth);
        branch.expression_operations = std::max(
            branch.expression_operations, metrics->expression_operations);
        return true;
    };
    if (node.kind == EmberActionIrNodeKind::If) {
        if (!include_branch(node.then_node) || !include_branch(node.else_node)) {
            return finish(std::nullopt);
        }
        result.expression_operations = node.predicate.size();
    } else if (node.kind == EmberActionIrNodeKind::Switch) {
        for (const auto& item : node.switch_cases) {
            if (!include_branch(item.target_node)) return finish(std::nullopt);
        }
        if (!include_branch(node.default_node)) return finish(std::nullopt);
        result.expression_operations = 1U + node.switch_cases.size();
    } else if (node.kind == EmberActionIrNodeKind::OnResult) {
        for (const auto& item : node.on_result_cases) {
            if (!include_branch(item.target_node)) return finish(std::nullopt);
        }
        if (!include_branch(node.on_result_default_node)) {
            return finish(std::nullopt);
        }
        result.expression_operations = 1U + node.on_result_cases.size();
    } else {
        return finish(std::nullopt);
    }
    if (!add_bounded(result.node_visits, branch.node_visits,
            kEmberActionExecutionMaximumNodes) ||
        !add_bounded(result.dispatches, branch.dispatches,
            kEmberActionExecutionMaximumDispatches) ||
        !add_bounded(result.expression_operations, branch.expression_operations,
            kEmberActionExecutionMaximumExpressionOperations)) {
        return finish(std::nullopt);
    }
    result.depth = std::max(result.depth, branch.depth + 1U);
    if (result.depth > kEmberActionExecutionMaximumDepth) {
        return finish(std::nullopt);
    }
    return finish(result);
}

[[nodiscard]] bool valid_executable_ir_shape(
    const EmberActionExecutableIr& ir) noexcept {
    std::size_t expression_operations = 0U;
    for (const auto& node : ir.nodes) {
        switch (node.kind) {
            case EmberActionIrNodeKind::Sequence:
                if (node.children.empty() || node.children.size() > 32U ||
                    std::any_of(node.children.begin(), node.children.end(),
                        [&](std::uint16_t child) { return child >= ir.nodes.size(); })) {
                    return false;
                }
                break;
            case EmberActionIrNodeKind::InvokeCommand:
                if (node.arguments.size() > kEmberActionExecutionMaximumArguments ||
                    (node.result_slot.has_value() &&
                     *node.result_slot >= ir.result_slot_count)) {
                    return false;
                }
                for (const auto& argument : node.arguments) {
                    if (!valid_ir_value(argument.value, ir)) return false;
                }
                break;
            case EmberActionIrNodeKind::If:
                if (node.then_node >= ir.nodes.size() ||
                    (node.else_node.has_value() && *node.else_node >= ir.nodes.size()) ||
                    node.predicate.empty()) {
                    return false;
                }
                if (node.predicate.size() >
                    kEmberActionExecutionMaximumExpressionOperations -
                        std::min(expression_operations,
                            kEmberActionExecutionMaximumExpressionOperations)) {
                    return false;
                }
                expression_operations += node.predicate.size();
                for (const auto& instruction : node.predicate) {
                    if (instruction.operation ==
                            EmberActionIrPredicateOperation::PushValue &&
                        !valid_ir_value(instruction.value, ir)) {
                        return false;
                    }
                }
                break;
            case EmberActionIrNodeKind::Switch:
                if (node.switch_cases.empty() || node.switch_cases.size() > 32U ||
                    !valid_ir_value(node.switch_value, ir) ||
                    (node.switch_value.kind != EmberActionRuntimeValueKind::Enum &&
                     node.switch_value.kind != EmberActionRuntimeValueKind::Integer &&
                     node.switch_value.kind != EmberActionRuntimeValueKind::String) ||
                    node.switch_cases.size() + 1U >
                        kEmberActionExecutionMaximumExpressionOperations -
                            std::min(expression_operations,
                                kEmberActionExecutionMaximumExpressionOperations)) {
                    return false;
                }
                expression_operations += node.switch_cases.size() + 1U;
                for (const auto& item : node.switch_cases) {
                    if (item.target_node >= ir.nodes.size() ||
                        item.match.kind != node.switch_value.kind ||
                        !valid_ir_value(item.match, ir, true)) {
                        return false;
                    }
                }
                if (node.default_node.has_value() &&
                    *node.default_node >= ir.nodes.size()) {
                    return false;
                }
                break;
            case EmberActionIrNodeKind::OnResult: {
                if (node.on_result_slot >= ir.result_slot_count ||
                    node.on_result_cases.size() > 6U ||
                    (node.on_result_cases.empty() &&
                     !node.on_result_default_node.has_value()) ||
                    node.on_result_cases.size() + 1U >
                        kEmberActionExecutionMaximumExpressionOperations -
                            std::min(expression_operations,
                                kEmberActionExecutionMaximumExpressionOperations)) {
                    return false;
                }
                expression_operations += node.on_result_cases.size() + 1U;
                std::optional<std::uint8_t> previous;
                for (const auto& item : node.on_result_cases) {
                    const auto ordinal = static_cast<std::uint8_t>(item.match);
                    if (!valid_on_result_match(item.match) ||
                        item.target_node >= ir.nodes.size() ||
                        (previous.has_value() && ordinal <= *previous)) {
                        return false;
                    }
                    previous = ordinal;
                }
                if (node.on_result_default_node.has_value() &&
                    *node.on_result_default_node >= ir.nodes.size()) {
                    return false;
                }
                break;
            }
            case EmberActionIrNodeKind::Return:
                if (node.return_value.source ==
                        EmberActionIrReturnSource::InvocationResult &&
                    node.return_value.result_slot >= ir.result_slot_count) {
                    return false;
                }
                break;
        }
    }
    std::array<std::optional<IrShapeMetrics>,
        kEmberActionExecutionMaximumNodes> memo{};
    std::array<bool, kEmberActionExecutionMaximumNodes> visiting{};
    for (const auto& entry : ir.entry_points) {
        if (!entry.present) continue;
        if (entry.root_node >= ir.nodes.size() ||
            entry.maximum_node_visits > kEmberActionExecutionMaximumNodes ||
            entry.maximum_dispatches > kEmberActionExecutionMaximumDispatches ||
            entry.maximum_depth > kEmberActionExecutionMaximumDepth ||
            entry.maximum_expression_operations >
                kEmberActionExecutionMaximumExpressionOperations) {
            return false;
        }
        const auto metrics = recompute_ir_metrics(
            ir, entry.root_node, memo, visiting);
        if (!metrics.has_value() ||
            entry.maximum_node_visits != metrics->node_visits ||
            entry.maximum_dispatches != metrics->dispatches ||
            entry.maximum_depth != metrics->depth ||
            entry.maximum_expression_operations !=
                metrics->expression_operations) {
            return false;
        }
    }
    return true;
}

}  // namespace

EmberActionExecutableIrResult compile_ember_action_executable_ir(
    std::shared_ptr<const EmberActionIrFoundation> foundation,
    const EmberActionRegistryView& registry) {
    return ExecutableCompiler(std::move(foundation), registry).run();
}

EmberActionExecutionResult execute_ember_action(
    const EmberActionExecutableIr& ir,
    const EmberActionExecutionRequest& request,
    EmberActionCommandControl& command_control) noexcept {
    EmberActionExecutionResult result;
    if (ir.foundation == nullptr || ir.foundation->prepared == nullptr ||
        ir.nodes.empty() || ir.nodes.size() > kEmberActionExecutionMaximumNodes ||
        ir.parameters.size() > kEmberActionExecutionMaximumArguments ||
        ir.result_slot_count > kEmberActionExecutionMaximumResults ||
        ir.execution_digest.empty() || ir.structural_seal == 0U ||
        ir.structural_seal != executable_ir_structural_seal(ir) ||
        !valid_executable_ir_shape(ir)) {
        result.status = EmberActionExecutionStatus::InvalidIr;
        return result;
    }
    if (request.require_studio_transaction) {
        result.status = EmberActionExecutionStatus::TransactionUnsupported;
        return result;
    }
    if (command_control.registry_digest() !=
        ir.foundation->prepared->dependencies.registry_digest) {
        result.status = EmberActionExecutionStatus::StaleRegistry;
        return result;
    }
    const auto point = static_cast<std::size_t>(request.entry_point);
    if (point >= ir.entry_points.size() || !ir.entry_points[point].present) {
        result.status = EmberActionExecutionStatus::EntryPointUnavailable;
        return result;
    }
    if (request.parameters.size() != ir.parameters.size()) {
        result.status = EmberActionExecutionStatus::InvalidContext;
        return result;
    }
    for (std::size_t index = 0U; index < request.parameters.size(); ++index) {
        if (!value_matches(request.parameters[index], ir.parameters[index].value)) {
            result.status = EmberActionExecutionStatus::InvalidContext;
            return result;
        }
    }
    const auto limits = effective_limits(request.limits);
    const auto& entry = ir.entry_points[point];
    if (entry.root_node >= ir.nodes.size()) {
        result.status = EmberActionExecutionStatus::InvalidIr;
        return result;
    }
    const auto reject_budget = [&](EmberActionExecutionStatus status) {
        result.status = status;
        if (limits.maximum_trace_entries == 0U) {
            result.trace_dropped = 1U;
        } else {
            auto& trace = result.trace[0];
            trace.sequence = 0U;
            trace.node_index = entry.root_node;
            trace.depth = 1U;
            trace.event = EmberActionTraceEvent::BudgetRejected;
            result.trace_count = 1U;
        }
        return result;
    };
    if (entry.maximum_node_visits > limits.maximum_node_visits) {
        return reject_budget(EmberActionExecutionStatus::NodeBudgetExceeded);
    }
    if (entry.maximum_dispatches > limits.maximum_dispatches) {
        return reject_budget(EmberActionExecutionStatus::DispatchBudgetExceeded);
    }
    if (entry.maximum_depth > limits.maximum_depth) {
        return reject_budget(EmberActionExecutionStatus::DepthBudgetExceeded);
    }
    if (entry.maximum_expression_operations >
        limits.maximum_expression_operations) {
        return reject_budget(
            EmberActionExecutionStatus::ExpressionBudgetExceeded);
    }
    if (ir.result_slot_count > limits.maximum_results) {
        return reject_budget(EmberActionExecutionStatus::ResultBudgetExceeded);
    }
    if (request.cancellation.is_cancelled()) {
        result.status = EmberActionExecutionStatus::Cancelled;
        return result;
    }

    result.status = EmberActionExecutionStatus::Completed;
    result.atomicity = EmberActionExecutionAtomicity::NonTransactional;
    ExecutionRuntime runtime(ir, request, command_control, limits, result);
    const auto outcome = runtime.execute(entry.root_node);
    if (result.status == EmberActionExecutionStatus::Completed) {
        result.result = runtime.sticky_result().has_value()
            ? runtime.sticky_result() : outcome.result;
        if (!result.result.has_value()) result.result = UiInvocationResult::Accepted;
    } else if (outcome.result.has_value()) {
        result.result = outcome.result;
    }
    return result;
}

}  // namespace emberlights
