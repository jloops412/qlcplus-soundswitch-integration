#pragma once

#include "emberlights/ember_action_registry_adapter.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace emberlights {

inline constexpr std::size_t kEmberActionExecutionMaximumNodes = 64U;
inline constexpr std::size_t kEmberActionExecutionMaximumDispatches = 32U;
inline constexpr std::size_t kEmberActionExecutionMaximumDepth = 8U;
inline constexpr std::size_t kEmberActionExecutionMaximumArguments = 32U;
inline constexpr std::size_t kEmberActionExecutionMaximumResults = 64U;
inline constexpr std::size_t kEmberActionExecutionMaximumExpressionOperations = 1024U;
inline constexpr std::size_t kEmberActionExecutionMaximumExpressionStackDepth = 64U;
inline constexpr std::size_t kEmberActionExecutionMaximumTraceEntries = 192U;
inline constexpr std::size_t kEmberActionExecutionMaximumTextBytes = 256U;
inline constexpr std::uint32_t kEmberActionExecutableIrCompilerGeneration = 3U;

enum class EmberActionEntryPoint : std::uint8_t {
    OnPress,
    OnRelease,
    OnValue,
    OnEncoderStep,
    OnLongPress,
    OnDoublePress,
    Count
};

enum class EmberActionRuntimeValueKind : std::uint8_t {
    Boolean,
    Integer,
    Number,
    Enum,
    String,
    StableId,
    SemanticRole
};

// A non-owning invocation value. String-like payloads remain owned by the
// caller for the duration of execute_ember_action(). The compiled IR owns all
// literal payloads. No path, URL, device, or storage semantics are implied.
struct EmberActionRuntimeValue {
    EmberActionRuntimeValueKind kind{EmberActionRuntimeValueKind::Boolean};
    bool boolean_value{false};
    std::int64_t integer_value{0};
    double number_value{0.0};
    std::string_view text{};

    [[nodiscard]] static constexpr EmberActionRuntimeValue boolean(bool value) noexcept {
        EmberActionRuntimeValue result;
        result.kind = EmberActionRuntimeValueKind::Boolean;
        result.boolean_value = value;
        return result;
    }

    [[nodiscard]] static constexpr EmberActionRuntimeValue integer(
        std::int64_t value) noexcept {
        EmberActionRuntimeValue result;
        result.kind = EmberActionRuntimeValueKind::Integer;
        result.integer_value = value;
        result.number_value = static_cast<double>(value);
        return result;
    }

    [[nodiscard]] static constexpr EmberActionRuntimeValue number(double value) noexcept {
        EmberActionRuntimeValue result;
        result.kind = EmberActionRuntimeValueKind::Number;
        result.number_value = value;
        return result;
    }

    [[nodiscard]] static constexpr EmberActionRuntimeValue text_value(
        EmberActionRuntimeValueKind kind_value,
        std::string_view value) noexcept {
        EmberActionRuntimeValue result;
        result.kind = kind_value;
        result.text = value;
        return result;
    }
};

struct EmberActionParameterSlot {
    std::string name;
    EmberActionValueContract value;
};

enum class EmberActionIrValueSource : std::uint8_t {
    Literal,
    Parameter
};

struct EmberActionIrValue {
    EmberActionIrValueSource source{EmberActionIrValueSource::Literal};
    EmberActionRuntimeValueKind kind{EmberActionRuntimeValueKind::Boolean};
    bool boolean_value{false};
    std::int64_t integer_value{0};
    double number_value{0.0};
    std::string text;
    std::uint16_t parameter_index{0U};
};

struct EmberActionIrArgument {
    std::string name;
    EmberActionValueContract expected;
    EmberActionIrValue value;
};

enum class EmberActionIrNodeKind : std::uint8_t {
    Sequence,
    InvokeCommand,
    If,
    Switch,
    OnResult,
    Return
};

enum class EmberActionIrPredicateOperation : std::uint8_t {
    PushValue,
    Equal,
    NotEqual,
    Less,
    LessOrEqual,
    Greater,
    GreaterOrEqual,
    And,
    Or,
    Not
};

struct EmberActionIrPredicateInstruction {
    EmberActionIrPredicateOperation operation{
        EmberActionIrPredicateOperation::PushValue};
    EmberActionIrValue value;
};

struct EmberActionIrSwitchCase {
    EmberActionIrValue match;
    std::uint16_t target_node{0U};
};

struct EmberActionIrResultCase {
    UiInvocationResult match{UiInvocationResult::Accepted};
    std::uint16_t target_node{0U};
};

enum class EmberActionSequencePolicy : std::uint8_t {
    Continue,
    StopOnRejected,
    StopOnError,
    StopOnAnyNonAccepted
};

enum class EmberActionIrReturnSource : std::uint8_t {
    LiteralResult,
    InvocationResult
};

struct EmberActionIrReturnValue {
    EmberActionIrReturnSource source{EmberActionIrReturnSource::LiteralResult};
    UiInvocationResult literal{UiInvocationResult::Accepted};
    std::uint16_t result_slot{0U};
};

struct EmberActionExecutableNode {
    std::string id;
    EmberActionIrNodeKind kind{EmberActionIrNodeKind::Sequence};
    EmberActionSequencePolicy sequence_policy{EmberActionSequencePolicy::StopOnError};
    std::vector<std::uint16_t> children;
    UiCommandId command{UiCommandId::ShowStart};
    std::vector<EmberActionIrArgument> arguments;
    std::optional<std::uint16_t> result_slot;
    std::vector<EmberActionIrPredicateInstruction> predicate;
    std::uint16_t then_node{0U};
    std::optional<std::uint16_t> else_node;
    EmberActionIrValue switch_value;
    std::vector<EmberActionIrSwitchCase> switch_cases;
    std::optional<std::uint16_t> default_node;
    std::uint16_t on_result_slot{0U};
    std::vector<EmberActionIrResultCase> on_result_cases;
    std::optional<std::uint16_t> on_result_default_node;
    EmberActionIrReturnValue return_value;
};

struct EmberActionExecutableEntry {
    bool present{false};
    std::uint16_t root_node{0U};
    std::size_t maximum_node_visits{0U};
    std::size_t maximum_dispatches{0U};
    std::size_t maximum_depth{0U};
    std::size_t maximum_expression_operations{0U};
};

// Immutable executable subset. It contains only resolved generated command
// handles and bounded data; source parsing and string command lookup are absent
// from execute_ember_action().
struct EmberActionExecutableIr {
    std::shared_ptr<const EmberActionIrFoundation> foundation;
    std::vector<EmberActionParameterSlot> parameters;
    std::vector<EmberActionExecutableNode> nodes;
    std::array<EmberActionExecutableEntry,
        static_cast<std::size_t>(EmberActionEntryPoint::Count)> entry_points{};
    std::size_t result_slot_count{0U};
    std::uint64_t structural_seal{0U};
    std::string execution_digest;
};

struct EmberActionExecutableIrResult {
    std::shared_ptr<const EmberActionExecutableIr> ir;
    std::vector<EmberActionDiagnostic> diagnostics;

    [[nodiscard]] bool ok() const noexcept {
        return ir != nullptr && diagnostics.empty();
    }
};

// Compiles Sequence, InvokeCommand, If, Switch, OnResult, and Return. Command
// arguments, predicates, and selectors may consume only literals or declared
// parameters; OnResult may consume only a prior command result slot. All other
// nodes, expression sources, dependencies, lifecycle states, realtime classes,
// and activation entry points fail closed.
[[nodiscard]] EmberActionExecutableIrResult compile_ember_action_executable_ir(
    std::shared_ptr<const EmberActionIrFoundation> foundation,
    const EmberActionRegistryView& registry);

struct EmberActionCommandArgumentValue {
    std::string_view name;
    EmberActionRuntimeValue value;
};

struct EmberActionCommandInvocationView {
    UiCommandId command{UiCommandId::ShowStart};
    std::string_view action_execution_digest;
    std::uint16_t node_index{0U};
    std::span<const EmberActionCommandArgumentValue> arguments;
};

// Production adapters implement this only at the approved UI/control service
// boundary. The interface has no state writer, Runner, scheduler, device, file,
// network, timer, or plugin surface.
class EmberActionCommandControl {
public:
    virtual ~EmberActionCommandControl() = default;

    [[nodiscard]] virtual std::string_view registry_digest() const noexcept = 0;
    [[nodiscard]] virtual UiInvocationResult invoke(
        const EmberActionCommandInvocationView& invocation) noexcept = 0;
};

struct EmberActionCancellationToken {
    const std::atomic_bool* cancelled{nullptr};

    [[nodiscard]] bool is_cancelled() const noexcept {
        return cancelled != nullptr && cancelled->load(std::memory_order_relaxed);
    }
};

struct EmberActionExecutionLimits {
    std::size_t maximum_node_visits{kEmberActionExecutionMaximumNodes};
    std::size_t maximum_dispatches{kEmberActionExecutionMaximumDispatches};
    std::size_t maximum_depth{kEmberActionExecutionMaximumDepth};
    std::size_t maximum_expression_operations{
        kEmberActionExecutionMaximumExpressionOperations};
    std::size_t maximum_trace_entries{kEmberActionExecutionMaximumTraceEntries};
    std::size_t maximum_results{kEmberActionExecutionMaximumResults};
};

struct EmberActionExecutionRequest {
    EmberActionEntryPoint entry_point{EmberActionEntryPoint::OnPress};
    std::span<const EmberActionRuntimeValue> parameters;
    EmberActionExecutionLimits limits;
    EmberActionCancellationToken cancellation;
    bool require_studio_transaction{false};
};

enum class EmberActionExecutionStatus : std::uint8_t {
    Completed,
    EntryPointUnavailable,
    InvalidContext,
    StaleRegistry,
    TransactionUnsupported,
    Cancelled,
    NodeBudgetExceeded,
    DispatchBudgetExceeded,
    DepthBudgetExceeded,
    ExpressionBudgetExceeded,
    ResultBudgetExceeded,
    InvalidIr,
    DispatcherFault
};

enum class EmberActionExecutionAtomicity : std::uint8_t {
    NotExecuted,
    NonTransactional
};

enum class EmberActionTraceEvent : std::uint8_t {
    NodeEntered,
    DispatchStarted,
    DispatchCompleted,
    BranchSelected,
    NodeExited,
    CancelledBeforeDispatch,
    BudgetRejected
};

struct EmberActionTraceEntry {
    std::uint32_t sequence{0U};
    std::uint16_t node_index{0U};
    std::uint8_t depth{0U};
    EmberActionTraceEvent event{EmberActionTraceEvent::NodeEntered};
    bool has_result{false};
    UiInvocationResult result{UiInvocationResult::Accepted};
    bool has_branch_target{false};
    std::uint16_t branch_target{0U};
    std::uint16_t branch_index{0U};

    [[nodiscard]] bool operator==(const EmberActionTraceEntry&) const noexcept = default;
};

struct EmberActionExecutionResult {
    EmberActionExecutionStatus status{EmberActionExecutionStatus::InvalidIr};
    EmberActionExecutionAtomicity atomicity{EmberActionExecutionAtomicity::NotExecuted};
    std::optional<UiInvocationResult> result;
    std::size_t node_visits{0U};
    std::size_t dispatches{0U};
    std::size_t expression_operations{0U};
    std::array<EmberActionTraceEntry, kEmberActionExecutionMaximumTraceEntries> trace{};
    std::size_t trace_count{0U};
    std::size_t trace_dropped{0U};

    [[nodiscard]] bool trace_truncated() const noexcept {
        return trace_dropped != 0U;
    }
};

// Repeated execution performs no dynamic allocation. All transient arguments,
// result slots, recursion, and trace storage are statically bounded.
[[nodiscard]] EmberActionExecutionResult execute_ember_action(
    const EmberActionExecutableIr& ir,
    const EmberActionExecutionRequest& request,
    EmberActionCommandControl& command_control) noexcept;

}  // namespace emberlights
