#pragma once

#include "emberlights/ember_action_json.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace emberlights {

enum class EmberActionValueKind {
    Unknown,
    Null,
    Boolean,
    Integer,
    Number,
    Enum,
    String,
    StableId,
    SemanticRole,
    Color,
    Duration,
    Object,
    List,
    Result,
    Void
};

struct EmberActionValueContract {
    EmberActionValueKind kind{EmberActionValueKind::Unknown};
    std::string unit;
    std::optional<double> minimum;
    std::optional<double> maximum;
    std::vector<std::string> enum_values;
    std::string target_kind;
    std::string schema_ref;
    std::size_t maximum_items{0U};
    std::size_t maximum_string_bytes{0U};
};

enum class EmberActionRealtimeClass {
    ViewLocal,
    StudioMutation,
    RunnerCommand,
    RunnerPriority,
    UtilityAsync,
    BlockingForbiddenLive
};

enum class EmberActionRegistryLifecycle {
    Current,
    DeprecatedCompatible,
    DeprecatedIncompatible,
    Removed
};

struct EmberActionCommandArgumentContract {
    std::string name;
    EmberActionValueContract value;
    bool required{true};
};

struct EmberActionCommandContract {
    std::string id;
    std::vector<EmberActionCommandArgumentContract> arguments;
    EmberActionValueContract result;
    std::vector<std::string> required_capabilities;
    EmberActionRealtimeClass realtime_class{EmberActionRealtimeClass::ViewLocal};
    EmberActionRegistryLifecycle lifecycle{EmberActionRegistryLifecycle::Current};
    std::string replacement_id;
    bool parallel_compatible{false};
    bool studio_transaction_compatible{false};
    bool on_activate_safe{false};
};

struct EmberActionStateContract {
    std::string id;
    EmberActionValueContract value;
    EmberActionRegistryLifecycle lifecycle{EmberActionRegistryLifecycle::Current};
    std::string replacement_id;
};

struct EmberActionCapabilityContract {
    std::string id;
    EmberActionRegistryLifecycle lifecycle{EmberActionRegistryLifecycle::Current};
    std::string replacement_id;
};

struct EmberActionDependencyContract {
    std::string id;
    std::string version;
    std::vector<std::string> invoked_actions;
    std::size_t maximum_command_invocations{0U};
    EmberActionRealtimeClass realtime_class{EmberActionRealtimeClass::ViewLocal};
    EmberActionRegistryLifecycle lifecycle{EmberActionRegistryLifecycle::Current};
    std::string replacement_id;
};

// Read-only bridge to the canonical registry implementation owned by #64/#31.
// The Action foundation neither owns nor caches canonical command/state IDs.
class EmberActionRegistryView {
public:
    virtual ~EmberActionRegistryView() = default;

    [[nodiscard]] virtual std::string_view registry_digest() const noexcept = 0;
    [[nodiscard]] virtual const EmberActionCommandContract* find_command(
        std::string_view id) const noexcept = 0;
    [[nodiscard]] virtual const EmberActionStateContract* find_state(
        std::string_view id) const noexcept = 0;
    [[nodiscard]] virtual const EmberActionCapabilityContract* find_capability(
        std::string_view id) const noexcept = 0;
    [[nodiscard]] virtual const EmberActionDependencyContract* find_action(
        std::string_view id,
        std::string_view version_range) const noexcept = 0;
    [[nodiscard]] virtual const EmberActionValueContract* find_context_value(
        std::string_view path) const noexcept = 0;
    [[nodiscard]] virtual bool supports_curve(std::string_view id) const noexcept = 0;
    [[nodiscard]] virtual bool supports_unit_conversion(
        std::string_view source,
        std::string_view target) const noexcept = 0;
};

struct EmberActionPlatformLimits {
    std::size_t maximum_source_bytes{64U * 1024U};
    std::size_t maximum_normalized_bytes{32U * 1024U};
    std::size_t maximum_nodes{64U};
    std::size_t maximum_branch_depth{8U};
    std::size_t maximum_referenced_actions{8U};
    std::size_t maximum_action_call_depth{8U};
    std::size_t maximum_state_reads{16U};
    std::size_t maximum_parameters{16U};
    std::size_t maximum_feedback_outputs{16U};
    std::size_t maximum_surface_state_values{16U};
    std::size_t maximum_parallel_children{8U};
    std::size_t maximum_command_invocations{32U};
    std::size_t maximum_expression_operations{1024U};
    std::size_t maximum_expression_stack_depth{64U};
};

struct EmberActionDependencyManifest {
    std::vector<std::string> commands;
    std::vector<std::string> states;
    std::vector<std::string> capabilities;
    std::vector<std::string> actions;
    std::string registry_digest;
};

// This is deliberately non-executable. It is an immutable, deterministic
// compiler-preparation product that must be rebased/adapted to #64 before an
// executable Action IR or production activation path is introduced.
struct EmberActionPreparedSource {
    std::shared_ptr<const EmberActionJsonValue> source;
    std::string normalized_json;
    std::string content_hash;
    EmberActionDependencyManifest dependencies;
    std::size_t node_count{0U};
    std::size_t maximum_branch_depth{0U};
    std::size_t maximum_command_invocations{0U};
    EmberActionRealtimeClass realtime_class{EmberActionRealtimeClass::ViewLocal};
};

struct EmberActionCompileResult {
    std::shared_ptr<const EmberActionPreparedSource> prepared;
    std::vector<EmberActionDiagnostic> diagnostics;

    [[nodiscard]] bool ok() const noexcept {
        return prepared != nullptr && diagnostics.empty();
    }
};

[[nodiscard]] EmberActionCompileResult prepare_ember_action_source(
    std::string_view source,
    const EmberActionRegistryView& registry,
    const EmberActionPlatformLimits& limits = {});

}  // namespace emberlights
