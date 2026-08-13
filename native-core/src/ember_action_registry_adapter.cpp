#include "emberlights/ember_action_registry_adapter.hpp"

#include "emberlights/file_identity.hpp"
#include "emberlights/generated/ember_action_registry_adapter.generated.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace emberlights {
namespace {

using generated_action_registry::GeneratedValueMetadata;

[[nodiscard]] EmberActionValueKind value_kind(std::string_view type) noexcept {
    if (type == "boolean") return EmberActionValueKind::Boolean;
    if (type == "integer") return EmberActionValueKind::Integer;
    if (type == "number") return EmberActionValueKind::Number;
    if (type == "enum") return EmberActionValueKind::Enum;
    if (type == "string") return EmberActionValueKind::String;
    if (type == "id") return EmberActionValueKind::StableId;
    if (type == "semanticRole") return EmberActionValueKind::SemanticRole;
    if (type == "color") return EmberActionValueKind::Color;
    if (type == "duration") return EmberActionValueKind::Duration;
    if (type == "object") return EmberActionValueKind::Object;
    if (type == "list") return EmberActionValueKind::List;
    if (type == "result") return EmberActionValueKind::Result;
    if (type == "void") return EmberActionValueKind::Void;
    return EmberActionValueKind::Unknown;
}

[[nodiscard]] EmberActionValueContract value_contract(
    const GeneratedValueMetadata& metadata) {
    EmberActionValueContract result;
    result.kind = value_kind(metadata.type);
    result.unit = std::string(metadata.unit);
    if (metadata.has_minimum) result.minimum = metadata.minimum;
    if (metadata.has_maximum) result.maximum = metadata.maximum;
    result.enum_values.reserve(metadata.enum_value_count);
    for (std::size_t index = 0U; index < metadata.enum_value_count; ++index) {
        result.enum_values.emplace_back(metadata.enum_values[index]);
    }
    result.target_kind = std::string(metadata.target_kind);
    result.schema_ref = std::string(metadata.schema_ref);
    result.maximum_items = metadata.maximum_items;
    result.maximum_string_bytes = metadata.maximum_string_bytes;
    return result;
}

[[nodiscard]] EmberActionRealtimeClass realtime_class(
    std::string_view value) noexcept {
    if (value == "studioMutation") return EmberActionRealtimeClass::StudioMutation;
    if (value == "runnerCommand") return EmberActionRealtimeClass::RunnerCommand;
    if (value == "runnerPriority") return EmberActionRealtimeClass::RunnerPriority;
    if (value == "utilityAsync") return EmberActionRealtimeClass::UtilityAsync;
    if (value == "blockingForbiddenLive") {
        return EmberActionRealtimeClass::BlockingForbiddenLive;
    }
    return EmberActionRealtimeClass::ViewLocal;
}

[[nodiscard]] EmberActionRegistryLifecycle lifecycle(
    std::string_view status) noexcept {
    // A deprecated definition is fail-closed until the canonical registry
    // publishes a proven Action-compatible conversion contract. A replacement
    // ID alone is not enough to authorize an automatic rewrite.
    if (status == "deprecated") {
        return EmberActionRegistryLifecycle::DeprecatedIncompatible;
    }
    if (status == "planned") return EmberActionRegistryLifecycle::Removed;
    return EmberActionRegistryLifecycle::Current;
}

[[nodiscard]] bool sorted_unique(const std::vector<std::string>& values) {
    return std::is_sorted(values.begin(), values.end()) &&
        std::adjacent_find(values.begin(), values.end()) == values.end();
}

void add_diagnostic(
    std::vector<EmberActionDiagnostic>& diagnostics,
    std::string code,
    std::string path,
    std::string message) {
    diagnostics.push_back({std::move(code), std::move(path), std::move(message)});
}

void append_field(
    std::string& output,
    std::string_view name,
    std::string_view value) {
    output.append(name);
    output.push_back('=');
    output.append(std::to_string(value.size()));
    output.push_back(':');
    output.append(value);
    output.push_back('\n');
}

void append_dependencies(
    std::string& output,
    std::string_view kind,
    const std::vector<std::string>& values) {
    output.append(kind);
    output.push_back('=');
    output.append(std::to_string(values.size()));
    output.push_back('\n');
    for (const auto& value : values) append_field(output, "id", value);
}

[[nodiscard]] std::string dependency_identity(
    const EmberActionDependencyManifest& dependencies) {
    std::string canonical;
    append_dependencies(canonical, "commands", dependencies.commands);
    append_dependencies(canonical, "states", dependencies.states);
    append_dependencies(canonical, "capabilities", dependencies.capabilities);
    append_dependencies(canonical, "actions", dependencies.actions);
    return "sha256:" + sha256_text(canonical);
}

[[nodiscard]] EmberActionIrCacheKey make_cache_key(
    const EmberActionPreparedSource& prepared) {
    EmberActionIrCacheKey result;
    result.source_hash = prepared.content_hash;
    result.registry_digest = prepared.dependencies.registry_digest;
    result.dependency_digest = dependency_identity(prepared.dependencies);

    std::string canonical;
    append_field(canonical, "format", "ember-action-ir-cache-v1");
    append_field(canonical, "compilerGeneration", std::to_string(result.compiler_generation));
    append_field(canonical, "sourceHash", result.source_hash);
    append_field(canonical, "registryDigest", result.registry_digest);
    append_field(canonical, "dependencyDigest", result.dependency_digest);
    result.cache_digest = "sha256:" + sha256_text(canonical);
    return result;
}

}  // namespace

GeneratedUiRegistryEmberActionView::GeneratedUiRegistryEmberActionView() {
    commands_.reserve(generated_action_registry::kCommands.size());
    for (const auto& generated : generated_action_registry::kCommands) {
        if (generated.status == "planned") continue;
        EmberActionCommandContract command;
        command.id = std::string(generated.id);
        command.arguments.reserve(generated.argument_count);
        for (std::size_t index = 0U; index < generated.argument_count; ++index) {
            const auto& argument = generated.arguments[index];
            command.arguments.push_back({
                std::string(argument.name),
                value_contract(argument.value),
                argument.required});
        }
        command.result = value_contract(generated.result);
        command.required_capabilities.reserve(generated.required_capability_count);
        for (std::size_t index = 0U; index < generated.required_capability_count; ++index) {
            command.required_capabilities.emplace_back(generated.required_capabilities[index]);
        }
        command.realtime_class = realtime_class(generated.realtime_class);
        command.lifecycle = lifecycle(generated.status);
        command.replacement_id = std::string(generated.replacement_id);
        command.parallel_compatible = generated.parallel_compatible;
        command.studio_transaction_compatible = generated.studio_transaction_compatible;
        command.on_activate_safe = generated.on_activate_safe;
        commands_.push_back(std::move(command));
    }

    states_.reserve(generated_action_registry::kStates.size());
    for (const auto& generated : generated_action_registry::kStates) {
        if (generated.status == "planned") continue;
        EmberActionStateContract state;
        state.id = std::string(generated.id);
        state.value = value_contract(generated.value);
        state.lifecycle = lifecycle(generated.status);
        state.replacement_id = std::string(generated.replacement_id);
        states_.push_back(std::move(state));
    }
}

std::string_view GeneratedUiRegistryEmberActionView::registry_digest() const noexcept {
    return generated_action_registry::kSourceDigest;
}

const EmberActionCommandContract* GeneratedUiRegistryEmberActionView::find_command(
    std::string_view id) const noexcept {
    const auto found = std::find_if(commands_.begin(), commands_.end(), [&](const auto& command) {
        return command.id == id;
    });
    return found == commands_.end() ? nullptr : &*found;
}

const EmberActionStateContract* GeneratedUiRegistryEmberActionView::find_state(
    std::string_view id) const noexcept {
    const auto found = std::find_if(states_.begin(), states_.end(), [&](const auto& state) {
        return state.id == id;
    });
    return found == states_.end() ? nullptr : &*found;
}

const EmberActionCapabilityContract*
GeneratedUiRegistryEmberActionView::find_capability(std::string_view id) const noexcept {
    static_cast<void>(id);
    return nullptr;
}

const EmberActionDependencyContract* GeneratedUiRegistryEmberActionView::find_action(
    std::string_view id,
    std::string_view version_range) const noexcept {
    static_cast<void>(id);
    static_cast<void>(version_range);
    return nullptr;
}

const EmberActionValueContract* GeneratedUiRegistryEmberActionView::find_context_value(
    std::string_view path) const noexcept {
    static_cast<void>(path);
    return nullptr;
}

bool GeneratedUiRegistryEmberActionView::supports_curve(std::string_view id) const noexcept {
    static_cast<void>(id);
    return false;
}

bool GeneratedUiRegistryEmberActionView::supports_unit_conversion(
    std::string_view source,
    std::string_view target) const noexcept {
    return source == target;
}

std::optional<UiCommandId> GeneratedUiRegistryEmberActionView::native_command_id(
    std::string_view id) const noexcept {
    if (find_command(id) == nullptr) return std::nullopt;
    const auto found = std::find_if(
        generated_action_registry::kCommands.begin(),
        generated_action_registry::kCommands.end(),
        [&](const auto& command) { return command.id == id; });
    if (found == generated_action_registry::kCommands.end()) return std::nullopt;
    return found->command;
}

std::optional<std::size_t> GeneratedUiRegistryEmberActionView::native_state_ordinal(
    std::string_view id) const noexcept {
    if (find_state(id) == nullptr) return std::nullopt;
    const auto found = std::find_if(
        generated_action_registry::kStates.begin(),
        generated_action_registry::kStates.end(),
        [&](const auto& state) { return state.id == id; });
    if (found == generated_action_registry::kStates.end()) return std::nullopt;
    return found->native_ordinal;
}

EmberActionIrFoundationResult compile_ember_action_ir_foundation(
    std::shared_ptr<const EmberActionPreparedSource> prepared,
    const GeneratedUiRegistryEmberActionView& registry) {
    EmberActionIrFoundationResult result;
    if (prepared == nullptr) {
        add_diagnostic(result.diagnostics, "EA_IR_PREPARED_REQUIRED", "/",
            "A validated prepared Action source is required.");
        return result;
    }
    const auto expected_source_hash = "sha256:" + sha256_text(prepared->normalized_json);
    if (prepared->content_hash != expected_source_hash) {
        add_diagnostic(result.diagnostics, "EA_IR_SOURCE_HASH_MISMATCH", "/contentHash",
            "Prepared canonical source identity does not match its normalized data.");
    }
    if (prepared->dependencies.registry_digest != registry.registry_digest()) {
        add_diagnostic(result.diagnostics, "EA_IR_REGISTRY_DIGEST_MISMATCH", "/requires",
            "Prepared dependencies were validated against a different registry digest.");
    }
    const auto& dependencies = prepared->dependencies;
    if (!sorted_unique(dependencies.commands) || !sorted_unique(dependencies.states) ||
        !sorted_unique(dependencies.capabilities) || !sorted_unique(dependencies.actions)) {
        add_diagnostic(result.diagnostics, "EA_IR_DEPENDENCY_ORDER", "/requires",
            "Prepared dependency lists must be sorted and unique.");
    }

    auto candidate = std::make_shared<EmberActionIrFoundation>();
    candidate->prepared = prepared;
    candidate->commands.reserve(dependencies.commands.size());
    for (const auto& id : dependencies.commands) {
        const auto native = registry.native_command_id(id);
        if (!native.has_value()) {
            add_diagnostic(result.diagnostics, "EA_IR_COMMAND_UNRESOLVED", "/requires/commands",
                "A prepared command has no accepted generated native handle.");
        } else {
            candidate->commands.push_back({id, *native});
        }
    }
    candidate->states.reserve(dependencies.states.size());
    for (const auto& id : dependencies.states) {
        const auto native = registry.native_state_ordinal(id);
        if (!native.has_value()) {
            add_diagnostic(result.diagnostics, "EA_IR_STATE_UNRESOLVED", "/requires/states",
                "A prepared state has no accepted generated native handle.");
        } else {
            candidate->states.push_back({id, *native});
        }
    }
    if (!dependencies.capabilities.empty()) {
        add_diagnostic(result.diagnostics, "EA_IR_CAPABILITY_UNRESOLVED", "/requires/capabilities",
            "The accepted native capability registry is not available in this foundation.");
    }
    if (!dependencies.actions.empty()) {
        add_diagnostic(result.diagnostics, "EA_IR_ACTION_UNRESOLVED", "/requires/actions",
            "Installed Action dependency resolution is not available in this foundation.");
    }

    if (result.diagnostics.empty()) {
        candidate->cache_key = make_cache_key(*prepared);
        result.ir = std::shared_ptr<const EmberActionIrFoundation>(std::move(candidate));
    }
    return result;
}

}  // namespace emberlights
