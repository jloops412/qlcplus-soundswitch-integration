#include "emberlights/ember_action_executor.hpp"
#include "emberlights/ember_action_registry_adapter.hpp"
#include "emberlights/generated/ui_registry.generated.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::atomic_size_t g_allocation_count{0U};

}  // namespace

#if defined(__GNUC__) || defined(__clang__)
#define EMBERLIGHTS_TEST_NOINLINE __attribute__((noinline))
#else
#define EMBERLIGHTS_TEST_NOINLINE
#endif

EMBERLIGHTS_TEST_NOINLINE void* operator new(std::size_t size) {
    g_allocation_count.fetch_add(1U, std::memory_order_relaxed);
    if (void* pointer = std::malloc(size == 0U ? 1U : size)) return pointer;
    throw std::bad_alloc{};
}

EMBERLIGHTS_TEST_NOINLINE void* operator new[](std::size_t size) {
    return ::operator new(size);
}

EMBERLIGHTS_TEST_NOINLINE void operator delete(void* pointer) noexcept {
    std::free(pointer);
}

EMBERLIGHTS_TEST_NOINLINE void operator delete[](void* pointer) noexcept {
    ::operator delete(pointer);
}

EMBERLIGHTS_TEST_NOINLINE void operator delete(void* pointer, std::size_t) noexcept {
    ::operator delete(pointer);
}

EMBERLIGHTS_TEST_NOINLINE void operator delete[](void* pointer, std::size_t) noexcept {
    ::operator delete[](pointer);
}

#undef EMBERLIGHTS_TEST_NOINLINE

namespace {

int g_failures = 0;

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ << " — " #condition "\n"; \
        ++g_failures; \
    } \
} while (false)

[[nodiscard]] bool has_code(
    const std::vector<emberlights::EmberActionDiagnostic>& diagnostics,
    std::string_view code) {
    return std::any_of(diagnostics.begin(), diagnostics.end(), [&](const auto& item) {
        return item.code == code;
    });
}

[[nodiscard]] bool replace_once(
    std::string& source,
    std::string_view needle,
    std::string_view replacement) {
    const auto position = source.find(needle);
    if (position == std::string::npos) return false;
    source.replace(position, needle.size(), replacement);
    return true;
}

[[nodiscard]] std::string two_command_source(std::string_view policy) {
    std::string source = R"action({
  "schemaVersion": 1,
  "id": "com.test.action.executor",
  "version": "1.0.0",
  "label": "Executor test",
  "compatibility": {
    "minimumAppVersion": "0.1.0",
    "commandRegistry": ">=1 <3",
    "stateRegistry": ">=1 <3",
    "capabilityRegistry": ">=1 <3"
  },
  "parameters": {
    "bpm": {
      "valueType": {"type": "number", "minimum": 20, "maximum": 300, "unit": "bpm"},
      "required": true
    }
  },
  "requires": {
    "commands": ["transport.manualBpm.set", "transport.tap"],
    "states": [],
    "capabilities": []
  },
  "entryPoints": {"onValue": "node.sequence"},
  "nodes": {
    "node.sequence": {
      "kind": "Sequence",
      "policy": "__POLICY__",
      "children": ["node.bpm", "node.tap", "node.return"]
    },
    "node.bpm": {
      "kind": "InvokeCommand",
      "commandId": "transport.manualBpm.set",
      "arguments": {"bpm": {"source": "parameter", "path": "bpm"}},
      "resultAs": "bpmResult"
    },
    "node.tap": {
      "kind": "InvokeCommand",
      "commandId": "transport.tap",
      "arguments": {},
      "resultAs": "tapResult"
    },
    "node.return": {
      "kind": "Return",
      "result": {"source": "nodeOutput", "path": "tapResult"}
    }
  },
  "feedback": {}
})action";
    const auto position = source.find("__POLICY__");
    source.replace(position, std::string_view("__POLICY__").size(), policy);
    return source;
}

[[nodiscard]] std::string repeated_dispatch_source(std::size_t count) {
    const auto group_count = (count + 31U) / 32U;
    std::string root_children;
    std::string group_nodes;
    std::size_t remaining = count;
    for (std::size_t group = 0U; group < group_count; ++group) {
        if (!root_children.empty()) root_children.append(", ");
        const auto group_id = "node.group" + std::to_string(group);
        root_children.append("\"").append(group_id).append("\"");
        std::string group_children;
        const auto group_size = std::min<std::size_t>(remaining, 32U);
        for (std::size_t index = 0U; index < group_size; ++index) {
            if (!group_children.empty()) group_children.append(", ");
            group_children.append("\"node.invoke\"");
        }
        if (!group_nodes.empty()) group_nodes.append(",\n");
        group_nodes.append("    \"").append(group_id)
            .append("\": {\"kind\": \"Sequence\", \"policy\": \"continue\", \"children\": [")
            .append(group_children).append("]}");
        remaining -= group_size;
    }
    if (!root_children.empty()) root_children.append(", ");
    root_children.append("\"node.return\"");
    std::string source = R"action({
  "schemaVersion": 1,
  "id": "com.test.action.flood",
  "version": "1.0.0",
  "label": "Bounded flood",
  "compatibility": {
    "minimumAppVersion": "0.1.0",
    "commandRegistry": ">=1 <3",
    "stateRegistry": ">=1 <3",
    "capabilityRegistry": ">=1 <3"
  },
  "parameters": {},
  "requires": {"commands": ["transport.tap"], "states": [], "capabilities": []},
  "entryPoints": {"onPress": "node.sequence"},
  "nodes": {
    "node.sequence": {"kind": "Sequence", "policy": "continue", "children": [__ROOT_CHILDREN__]},
__GROUP_NODES__,
    "node.invoke": {"kind": "InvokeCommand", "commandId": "transport.tap", "arguments": {}, "resultAs": "result"},
    "node.return": {"kind": "Return", "result": {"source": "nodeOutput", "path": "result"}}
  },
  "feedback": {}
})action";
    auto position = source.find("__ROOT_CHILDREN__");
    source.replace(position, std::string_view("__ROOT_CHILDREN__").size(), root_children);
    position = source.find("__GROUP_NODES__");
    source.replace(position, std::string_view("__GROUP_NODES__").size(), group_nodes);
    return source;
}

[[nodiscard]] std::string state_dependency_source() {
    return R"action({
  "schemaVersion": 1,
  "id": "com.test.action.state-dependency",
  "version": "1.0.0",
  "label": "State dependency",
  "compatibility": {
    "minimumAppVersion": "0.1.0",
    "commandRegistry": ">=1 <3",
    "stateRegistry": ">=1 <3",
    "capabilityRegistry": ">=1 <3"
  },
  "parameters": {},
  "requires": {"commands": ["transport.tap"], "states": ["transport.bpm"], "capabilities": []},
  "entryPoints": {"onPress": "node.sequence"},
  "nodes": {
    "node.sequence": {"kind": "Sequence", "children": ["node.read", "node.invoke", "node.return"]},
    "node.read": {"kind": "ReadState", "stateId": "transport.bpm", "as": "bpm"},
    "node.invoke": {"kind": "InvokeCommand", "commandId": "transport.tap", "arguments": {}, "resultAs": "result"},
    "node.return": {"kind": "Return", "result": {"source": "nodeOutput", "path": "result"}}
  },
  "feedback": {}
})action";
}

[[nodiscard]] std::string nested_source() {
    return R"action({
  "schemaVersion": 1,
  "id": "com.test.action.depth",
  "version": "1.0.0",
  "label": "Bounded depth",
  "compatibility": {
    "minimumAppVersion": "0.1.0",
    "commandRegistry": ">=1 <3",
    "stateRegistry": ">=1 <3",
    "capabilityRegistry": ">=1 <3"
  },
  "parameters": {},
  "requires": {"commands": ["transport.tap"], "states": [], "capabilities": []},
  "entryPoints": {"onPress": "node.outer"},
  "nodes": {
    "node.outer": {"kind": "Sequence", "children": ["node.middle", "node.return"]},
    "node.middle": {"kind": "Sequence", "children": ["node.inner"]},
    "node.inner": {"kind": "Sequence", "children": ["node.invoke"]},
    "node.invoke": {"kind": "InvokeCommand", "commandId": "transport.tap", "arguments": {}, "resultAs": "result"},
    "node.return": {"kind": "Return", "result": {"source": "nodeOutput", "path": "result"}}
  },
  "feedback": {}
})action";
}

[[nodiscard]] std::string control_flow_source() {
    return R"action({
  "schemaVersion": 1,
  "id": "com.test.action.control-flow",
  "version": "1.0.0",
  "label": "Bounded control flow",
  "compatibility": {
    "minimumAppVersion": "0.1.0",
    "commandRegistry": ">=1 <3",
    "stateRegistry": ">=1 <3",
    "capabilityRegistry": ">=1 <3"
  },
  "parameters": {
    "enabled": {"valueType": {"type": "boolean"}, "required": true},
    "level": {"valueType": {"type": "integer", "minimum": 0, "maximum": 10}, "required": true},
    "mode": {"valueType": {"type": "enum", "values": ["tap", "bpm", "idle"]}, "required": true}
  },
  "requires": {"commands": ["transport.tap"], "states": [], "capabilities": []},
  "entryPoints": {"onPress": "node.if"},
  "nodes": {
    "node.if": {
      "kind": "If",
      "predicate": {
        "op": "and",
        "args": [
          {"source": "parameter", "path": "enabled"},
          {"op": "greaterOrEqual", "args": [
            {"source": "parameter", "path": "level"},
            {"literal": 5}
          ]}
        ]
      },
      "then": "node.switch",
      "else": "node.disabled"
    },
    "node.switch": {
      "kind": "Switch",
      "value": {"source": "parameter", "path": "mode"},
      "cases": [
        {"match": "tap", "node": "node.tap"},
        {"match": "bpm", "node": "node.bpm"}
      ],
      "default": "node.idle"
    },
    "node.tap": {"kind": "Sequence", "children": ["node.invoke", "node.tapReturn"]},
    "node.invoke": {"kind": "InvokeCommand", "commandId": "transport.tap", "arguments": {}},
    "node.tapReturn": {"kind": "Return", "result": {"literal": "NoChange"}},
    "node.bpm": {"kind": "Return", "result": {"literal": "Unavailable"}},
    "node.idle": {"kind": "Return", "result": {"literal": "Accepted"}},
    "node.disabled": {"kind": "Return", "result": {"literal": "Unsupported"}}
  },
  "feedback": {}
})action";
}

[[nodiscard]] std::string integer_switch_source() {
    return R"action({
  "schemaVersion": 1,
  "id": "com.test.action.integer-switch",
  "version": "1.0.0",
  "label": "Integer switch",
  "compatibility": {
    "minimumAppVersion": "0.1.0",
    "commandRegistry": ">=1 <3",
    "stateRegistry": ">=1 <3",
    "capabilityRegistry": ">=1 <3"
  },
  "parameters": {
    "slot": {"valueType": {"type": "integer", "minimum": 0, "maximum": 3}, "required": true}
  },
  "requires": {"commands": ["transport.tap"], "states": [], "capabilities": []},
  "entryPoints": {"onPress": "node.switch"},
  "nodes": {
    "node.switch": {
      "kind": "Switch",
      "value": {"source": "parameter", "path": "slot"},
      "cases": [
        {"match": 0, "node": "node.zero"},
        {"match": 1, "node": "node.one"}
      ],
      "default": "node.default"
    },
    "node.zero": {"kind": "Return", "result": {"literal": "Accepted"}},
    "node.one": {"kind": "InvokeCommand", "commandId": "transport.tap", "arguments": {}},
    "node.default": {"kind": "Return", "result": {"literal": "Unavailable"}}
  },
  "feedback": {}
})action";
}

[[nodiscard]] std::string string_switch_source() {
    auto source = integer_switch_source();
    CHECK(replace_once(source,
        "\"id\": \"com.test.action.integer-switch\"",
        "\"id\": \"com.test.action.string-switch\""));
    CHECK(replace_once(source,
        "\"slot\": {\"valueType\": {\"type\": \"integer\", \"minimum\": 0, \"maximum\": 3}, \"required\": true}",
        "\"slot\": {\"valueType\": {\"type\": \"string\", \"maxLength\": 16}, \"required\": true}"));
    CHECK(replace_once(source, "{\"match\": 0", "{\"match\": \"zero\""));
    CHECK(replace_once(source, "{\"match\": 1", "{\"match\": \"one\""));
    return source;
}

[[nodiscard]] std::string branch_dispatch_source(std::size_t count) {
    std::string children;
    for (std::size_t index = 0U; index < count; ++index) {
        if (!children.empty()) children.append(", ");
        children.append("\"node.invoke\"");
    }
    std::string source = R"action({
  "schemaVersion": 1,
  "id": "com.test.action.branch-budget",
  "version": "1.0.0",
  "label": "Branch budget",
  "compatibility": {
    "minimumAppVersion": "0.1.0",
    "commandRegistry": ">=1 <3",
    "stateRegistry": ">=1 <3",
    "capabilityRegistry": ">=1 <3"
  },
  "parameters": {
    "takeFlood": {"valueType": {"type": "boolean"}, "required": true}
  },
  "requires": {"commands": ["transport.tap"], "states": [], "capabilities": []},
  "entryPoints": {"onPress": "node.if"},
  "nodes": {
    "node.if": {
      "kind": "If",
      "predicate": {"source": "parameter", "path": "takeFlood"},
      "then": "node.flood",
      "else": "node.safe"
    },
    "node.flood": {"kind": "Sequence", "policy": "continue", "children": [__CHILDREN__]},
    "node.invoke": {"kind": "InvokeCommand", "commandId": "transport.tap", "arguments": {}},
    "node.safe": {"kind": "Return", "result": {"literal": "Accepted"}}
  },
  "feedback": {}
})action";
    CHECK(replace_once(source, "__CHILDREN__", children));
    return source;
}

[[nodiscard]] std::string branch_depth_source() {
    return R"action({
  "schemaVersion": 1,
  "id": "com.test.action.branch-depth",
  "version": "1.0.0",
  "label": "Branch depth",
  "compatibility": {
    "minimumAppVersion": "0.1.0",
    "commandRegistry": ">=1 <3",
    "stateRegistry": ">=1 <3",
    "capabilityRegistry": ">=1 <3"
  },
  "parameters": {
    "deep": {"valueType": {"type": "boolean"}, "required": true}
  },
  "requires": {"commands": ["transport.tap"], "states": [], "capabilities": []},
  "entryPoints": {"onPress": "node.if"},
  "nodes": {
    "node.if": {"kind": "If", "predicate": {"source": "parameter", "path": "deep"}, "then": "node.one", "else": "node.safe"},
    "node.one": {"kind": "Sequence", "children": ["node.two"]},
    "node.two": {"kind": "Sequence", "children": ["node.three"]},
    "node.three": {"kind": "Sequence", "children": ["node.invoke"]},
    "node.invoke": {"kind": "InvokeCommand", "commandId": "transport.tap", "arguments": {}},
    "node.safe": {"kind": "Return", "result": {"literal": "Accepted"}}
  },
  "feedback": {}
})action";
}

[[nodiscard]] std::string number_predicate_source() {
    auto source = control_flow_source();
    CHECK(replace_once(source,
        "\"level\": {\"valueType\": {\"type\": \"integer\", \"minimum\": 0, \"maximum\": 10}, \"required\": true}",
        "\"level\": {\"valueType\": {\"type\": \"number\", \"minimum\": 0, \"maximum\": 10}, \"required\": true}"));
    CHECK(replace_once(source,
        "\"id\": \"com.test.action.control-flow\"",
        "\"id\": \"com.test.action.number-predicate\""));
    CHECK(replace_once(source, "{\"literal\": 5}", "{\"literal\": 5.5}"));
    return source;
}

[[nodiscard]] std::string on_result_source() {
    return R"action({
  "schemaVersion": 1,
  "id": "com.test.action.on-result",
  "version": "1.0.0",
  "label": "Bounded result flow",
  "compatibility": {
    "minimumAppVersion": "0.1.0",
    "commandRegistry": ">=1 <3",
    "stateRegistry": ">=1 <3",
    "capabilityRegistry": ">=1 <3"
  },
  "parameters": {},
  "requires": {"commands": ["transport.tap"], "states": [], "capabilities": []},
  "entryPoints": {"onPress": "node.root"},
  "nodes": {
    "node.root": {
      "kind": "Sequence",
      "policy": "stopOnError",
      "children": ["node.primary", "node.route"]
    },
    "node.primary": {
      "kind": "InvokeCommand",
      "commandId": "transport.tap",
      "arguments": {},
      "resultAs": "primaryResult"
    },
    "node.route": {
      "kind": "OnResult",
      "result": {"source": "nodeOutput", "path": "primaryResult"},
      "cases": {
        "Unsupported": "node.original",
        "ValidationFailed": "node.original",
        "InvalidArguments": "node.original",
        "Unavailable": "node.fallback",
        "NoChange": "node.original",
        "Accepted": "node.accepted",
        "default": "node.original"
      }
    },
    "node.accepted": {"kind": "Return", "result": {"literal": "Accepted"}},
    "node.original": {
      "kind": "Return",
      "result": {"source": "nodeOutput", "path": "primaryResult"}
    },
    "node.fallback": {
      "kind": "Sequence",
      "policy": "continue",
      "children": ["node.fallback.invoke", "node.original"]
    },
    "node.fallback.invoke": {
      "kind": "InvokeCommand",
      "commandId": "transport.tap",
      "arguments": {}
    }
  },
  "feedback": {}
})action";
}

[[nodiscard]] std::string priority_source() {
    return R"action({
  "schemaVersion": 1,
  "id": "com.test.action.priority",
  "version": "1.0.0",
  "label": "Priority command",
  "compatibility": {
    "minimumAppVersion": "0.1.0",
    "commandRegistry": ">=1 <3",
    "stateRegistry": ">=1 <3",
    "capabilityRegistry": ">=1 <3"
  },
  "parameters": {},
  "requires": {"commands": ["output.blackout.toggle"], "states": [], "capabilities": []},
  "entryPoints": {"onPress": "node.invoke"},
  "nodes": {
    "node.invoke": {"kind": "InvokeCommand", "commandId": "output.blackout.toggle", "arguments": {}}
  },
  "feedback": {}
})action";
}

struct CompileChain {
    std::shared_ptr<const emberlights::EmberActionPreparedSource> prepared;
    std::shared_ptr<const emberlights::EmberActionIrFoundation> foundation;
    std::shared_ptr<const emberlights::EmberActionExecutableIr> executable;
    std::vector<emberlights::EmberActionDiagnostic> diagnostics;
};

[[nodiscard]] CompileChain compile_source(
    std::string_view source,
    const emberlights::GeneratedUiRegistryEmberActionView& registry) {
    CompileChain chain;
    auto prepared = emberlights::prepare_ember_action_source(source, registry);
    if (!prepared.ok()) {
        chain.diagnostics = std::move(prepared.diagnostics);
        return chain;
    }
    chain.prepared = prepared.prepared;
    auto foundation = emberlights::compile_ember_action_ir_foundation(
        prepared.prepared, registry);
    if (!foundation.ok()) {
        chain.diagnostics = std::move(foundation.diagnostics);
        return chain;
    }
    chain.foundation = foundation.ir;
    auto executable = emberlights::compile_ember_action_executable_ir(
        foundation.ir, registry);
    chain.diagnostics = std::move(executable.diagnostics);
    chain.executable = std::move(executable.ir);
    return chain;
}

class RecordingControl final : public emberlights::EmberActionCommandControl {
public:
    [[nodiscard]] std::string_view registry_digest() const noexcept override {
        return digest;
    }

    [[nodiscard]] emberlights::UiInvocationResult invoke(
        const emberlights::EmberActionCommandInvocationView& invocation) noexcept override {
        if (calls < commands.size()) {
            commands[calls] = invocation.command;
            argument_counts[calls] = invocation.arguments.size();
            node_indices[calls] = invocation.node_index;
            execution_digests[calls] = invocation.action_execution_digest;
            if (!invocation.arguments.empty()) {
                first_arguments[calls] = invocation.arguments.front().value;
            }
        }
        const auto result = calls < scripted_count
            ? scripted_results[calls]
            : emberlights::UiInvocationResult::Accepted;
        ++calls;
        if (cancel_after_first != nullptr && calls == 1U) {
            cancel_after_first->store(true, std::memory_order_relaxed);
        }
        return result;
    }

    std::string_view digest{emberlights::kUiRegistryDigest};
    std::array<emberlights::UiInvocationResult, 32> scripted_results{};
    std::size_t scripted_count{0U};
    std::array<emberlights::UiCommandId, 32> commands{};
    std::array<std::size_t, 32> argument_counts{};
    std::array<std::uint16_t, 32> node_indices{};
    std::array<std::string_view, 32> execution_digests{};
    std::array<emberlights::EmberActionRuntimeValue, 32> first_arguments{};
    std::atomic_bool* cancel_after_first{nullptr};
    std::size_t calls{0U};
};

class LifecycleRegistryView final : public emberlights::EmberActionRegistryView {
public:
    explicit LifecycleRegistryView(
        const emberlights::GeneratedUiRegistryEmberActionView& source)
        : source_(source) {
        const auto* command = source_.find_command("transport.tap");
        if (command != nullptr) deprecated_ = *command;
        deprecated_.lifecycle = emberlights::EmberActionRegistryLifecycle::DeprecatedIncompatible;
    }

    [[nodiscard]] std::string_view registry_digest() const noexcept override {
        return source_.registry_digest();
    }
    [[nodiscard]] const emberlights::EmberActionCommandContract* find_command(
        std::string_view id) const noexcept override {
        return id == deprecated_.id ? &deprecated_ : source_.find_command(id);
    }
    [[nodiscard]] const emberlights::EmberActionStateContract* find_state(
        std::string_view id) const noexcept override {
        return source_.find_state(id);
    }
    [[nodiscard]] const emberlights::EmberActionCapabilityContract* find_capability(
        std::string_view id) const noexcept override {
        return source_.find_capability(id);
    }
    [[nodiscard]] const emberlights::EmberActionDependencyContract* find_action(
        std::string_view id,
        std::string_view range) const noexcept override {
        return source_.find_action(id, range);
    }
    [[nodiscard]] const emberlights::EmberActionValueContract* find_context_value(
        std::string_view path) const noexcept override {
        return source_.find_context_value(path);
    }
    [[nodiscard]] bool supports_curve(std::string_view id) const noexcept override {
        return source_.supports_curve(id);
    }
    [[nodiscard]] bool supports_unit_conversion(
        std::string_view source,
        std::string_view target) const noexcept override {
        return source_.supports_unit_conversion(source, target);
    }

private:
    const emberlights::GeneratedUiRegistryEmberActionView& source_;
    emberlights::EmberActionCommandContract deprecated_;
};

void report_compile_failure(const CompileChain& chain) {
    for (const auto& diagnostic : chain.diagnostics) {
        std::cerr << diagnostic.code << ' ' << diagnostic.path << ' '
                  << diagnostic.message << '\n';
    }
}

void test_deterministic_execution_and_trace(
    const emberlights::GeneratedUiRegistryEmberActionView& registry) {
    const auto first = compile_source(two_command_source("stopOnError"), registry);
    const auto second = compile_source(two_command_source("stopOnError"), registry);
    CHECK(first.executable != nullptr);
    CHECK(second.executable != nullptr);
    if (first.executable == nullptr || second.executable == nullptr) {
        report_compile_failure(first);
        return;
    }
    CHECK(first.executable->execution_digest == second.executable->execution_digest);
    CHECK(first.executable->foundation->cache_key.cache_digest ==
        second.executable->foundation->cache_key.cache_digest);

    RecordingControl first_control;
    first_control.scripted_results[0] = emberlights::UiInvocationResult::Accepted;
    first_control.scripted_results[1] = emberlights::UiInvocationResult::NoChange;
    first_control.scripted_count = 2U;
    const std::array parameters{emberlights::EmberActionRuntimeValue::number(120.0)};
    emberlights::EmberActionExecutionRequest request;
    request.entry_point = emberlights::EmberActionEntryPoint::OnValue;
    request.parameters = parameters;
    const auto first_result = emberlights::execute_ember_action(
        *first.executable, request, first_control);
    CHECK(first_result.status == emberlights::EmberActionExecutionStatus::Completed);
    CHECK(first_result.atomicity ==
        emberlights::EmberActionExecutionAtomicity::NonTransactional);
    CHECK(first_result.result == emberlights::UiInvocationResult::NoChange);
    CHECK(first_result.dispatches == 2U);
    CHECK(first_result.node_visits == 4U);
    CHECK(!first_result.trace_truncated());
    CHECK(first_control.commands[0] == emberlights::UiCommandId::ManualBpmSet);
    CHECK(first_control.commands[1] == emberlights::UiCommandId::TapTempo);
    CHECK(first_control.argument_counts[0] == 1U);
    CHECK(first_control.first_arguments[0].number_value == 120.0);
    CHECK(first_control.execution_digests[0] == first.executable->execution_digest);

    RecordingControl second_control;
    second_control.scripted_results = first_control.scripted_results;
    second_control.scripted_count = 2U;
    const auto second_result = emberlights::execute_ember_action(
        *second.executable, request, second_control);
    CHECK(second_result.status == first_result.status);
    CHECK(second_result.result == first_result.result);
    CHECK(second_result.trace_count == first_result.trace_count);
    CHECK(std::equal(
        first_result.trace.begin(),
        first_result.trace.begin() + static_cast<std::ptrdiff_t>(first_result.trace_count),
        second_result.trace.begin()));
}

void test_sequence_refusal_and_dispatcher_fault(
    const emberlights::GeneratedUiRegistryEmberActionView& registry) {
    const auto stopping = compile_source(two_command_source("stopOnRejected"), registry);
    const auto continuing = compile_source(two_command_source("continue"), registry);
    CHECK(stopping.executable != nullptr);
    CHECK(continuing.executable != nullptr);
    if (stopping.executable == nullptr || continuing.executable == nullptr) return;
    const std::array parameters{emberlights::EmberActionRuntimeValue::number(120.0)};
    emberlights::EmberActionExecutionRequest request;
    request.entry_point = emberlights::EmberActionEntryPoint::OnValue;
    request.parameters = parameters;

    RecordingControl refusal;
    refusal.scripted_results[0] = emberlights::UiInvocationResult::Unavailable;
    refusal.scripted_results[1] = emberlights::UiInvocationResult::Accepted;
    refusal.scripted_count = 2U;
    const auto refused = emberlights::execute_ember_action(
        *stopping.executable, request, refusal);
    CHECK(refused.status == emberlights::EmberActionExecutionStatus::Completed);
    CHECK(refused.result == emberlights::UiInvocationResult::Unavailable);
    CHECK(refused.dispatches == 1U);
    CHECK(refusal.calls == 1U);

    RecordingControl queue_full;
    queue_full.scripted_results[0] = emberlights::UiInvocationResult::QueueFull;
    queue_full.scripted_count = 1U;
    const auto queue_rejected = emberlights::execute_ember_action(
        *stopping.executable, request, queue_full);
    CHECK(queue_rejected.status == emberlights::EmberActionExecutionStatus::Completed);
    CHECK(queue_rejected.result == emberlights::UiInvocationResult::QueueFull);
    CHECK(queue_rejected.dispatches == 1U);

    RecordingControl queue_full_continue;
    queue_full_continue.scripted_results[0] = emberlights::UiInvocationResult::QueueFull;
    queue_full_continue.scripted_count = 1U;
    const auto queue_continue_result = emberlights::execute_ember_action(
        *continuing.executable, request, queue_full_continue);
    CHECK(queue_continue_result.result == emberlights::UiInvocationResult::QueueFull);
    CHECK(queue_continue_result.dispatches == 1U);

    RecordingControl safety_continue;
    safety_continue.scripted_results[0] = emberlights::UiInvocationResult::SafetyRejected;
    safety_continue.scripted_count = 1U;
    const auto safety_continue_result = emberlights::execute_ember_action(
        *continuing.executable, request, safety_continue);
    CHECK(safety_continue_result.result == emberlights::UiInvocationResult::SafetyRejected);
    CHECK(safety_continue_result.dispatches == 1U);

    RecordingControl continued;
    continued.scripted_results = refusal.scripted_results;
    continued.scripted_count = 2U;
    const auto continued_result = emberlights::execute_ember_action(
        *continuing.executable, request, continued);
    CHECK(continued_result.status == emberlights::EmberActionExecutionStatus::Completed);
    CHECK(continued_result.result == emberlights::UiInvocationResult::Unavailable);
    CHECK(continued_result.dispatches == 2U);

    RecordingControl fault;
    fault.scripted_results[0] = emberlights::UiInvocationResult::InternalError;
    fault.scripted_count = 1U;
    const auto faulted = emberlights::execute_ember_action(
        *continuing.executable, request, fault);
    CHECK(faulted.status == emberlights::EmberActionExecutionStatus::DispatcherFault);
    CHECK(faulted.result == emberlights::UiInvocationResult::InternalError);
    CHECK(faulted.dispatches == 1U);
}

void test_cancellation_and_fail_closed_context(
    const emberlights::GeneratedUiRegistryEmberActionView& registry) {
    const auto chain = compile_source(two_command_source("continue"), registry);
    CHECK(chain.executable != nullptr);
    if (chain.executable == nullptr) return;
    const std::array parameters{emberlights::EmberActionRuntimeValue::number(120.0)};
    emberlights::EmberActionExecutionRequest request;
    request.entry_point = emberlights::EmberActionEntryPoint::OnValue;
    request.parameters = parameters;

    std::atomic_bool cancelled{false};
    RecordingControl cancellation;
    cancellation.cancel_after_first = &cancelled;
    request.cancellation.cancelled = &cancelled;
    const auto cancelled_result = emberlights::execute_ember_action(
        *chain.executable, request, cancellation);
    CHECK(cancelled_result.status == emberlights::EmberActionExecutionStatus::Cancelled);
    CHECK(cancelled_result.dispatches == 1U);
    CHECK(cancellation.calls == 1U);
    CHECK(std::any_of(
        cancelled_result.trace.begin(),
        cancelled_result.trace.begin() +
            static_cast<std::ptrdiff_t>(cancelled_result.trace_count),
        [](const auto& entry) {
            return entry.event ==
                emberlights::EmberActionTraceEvent::CancelledBeforeDispatch;
        }));

    request.cancellation.cancelled = nullptr;
    RecordingControl invalid;
    const std::array wrong_type{emberlights::EmberActionRuntimeValue::boolean(true)};
    request.parameters = wrong_type;
    const auto invalid_type = emberlights::execute_ember_action(
        *chain.executable, request, invalid);
    CHECK(invalid_type.status == emberlights::EmberActionExecutionStatus::InvalidContext);
    CHECK(invalid.calls == 0U);

    const std::array out_of_range{emberlights::EmberActionRuntimeValue::number(500.0)};
    request.parameters = out_of_range;
    const auto invalid_range = emberlights::execute_ember_action(
        *chain.executable, request, invalid);
    CHECK(invalid_range.status == emberlights::EmberActionExecutionStatus::InvalidContext);
    CHECK(invalid.calls == 0U);

    request.parameters = parameters;
    request.require_studio_transaction = true;
    const auto transaction = emberlights::execute_ember_action(
        *chain.executable, request, invalid);
    CHECK(transaction.status ==
        emberlights::EmberActionExecutionStatus::TransactionUnsupported);
    CHECK(transaction.atomicity == emberlights::EmberActionExecutionAtomicity::NotExecuted);
    CHECK(invalid.calls == 0U);

    request.require_studio_transaction = false;
    invalid.digest = "stale-registry";
    const auto stale = emberlights::execute_ember_action(
        *chain.executable, request, invalid);
    CHECK(stale.status == emberlights::EmberActionExecutionStatus::StaleRegistry);
    CHECK(invalid.calls == 0U);
}

void test_flood_and_hard_budgets(
    const emberlights::GeneratedUiRegistryEmberActionView& registry) {
    const auto flood = compile_source(repeated_dispatch_source(32U), registry);
    CHECK(flood.executable != nullptr);
    if (flood.executable == nullptr) {
        report_compile_failure(flood);
        return;
    }
    emberlights::EmberActionExecutionRequest request;
    request.entry_point = emberlights::EmberActionEntryPoint::OnPress;
    request.limits.maximum_dispatches = 31U;
    RecordingControl control;
    const auto dispatch_budget = emberlights::execute_ember_action(
        *flood.executable, request, control);
    CHECK(dispatch_budget.status ==
        emberlights::EmberActionExecutionStatus::DispatchBudgetExceeded);
    CHECK(dispatch_budget.trace_count == 1U);
    CHECK(dispatch_budget.trace[0].event ==
        emberlights::EmberActionTraceEvent::BudgetRejected);
    CHECK(control.calls == 0U);

    request.limits.maximum_dispatches = 32U;
    request.limits.maximum_node_visits = 34U;
    const auto node_budget = emberlights::execute_ember_action(
        *flood.executable, request, control);
    CHECK(node_budget.status ==
        emberlights::EmberActionExecutionStatus::NodeBudgetExceeded);
    CHECK(control.calls == 0U);

    request.limits.maximum_node_visits = 64U;
    request.limits.maximum_results = 0U;
    const auto result_budget = emberlights::execute_ember_action(
        *flood.executable, request, control);
    CHECK(result_budget.status ==
        emberlights::EmberActionExecutionStatus::ResultBudgetExceeded);
    CHECK(control.calls == 0U);

    request.limits.maximum_results = 64U;
    RecordingControl exact_limit;
    const auto exact_limit_result = emberlights::execute_ember_action(
        *flood.executable, request, exact_limit);
    CHECK(exact_limit_result.status == emberlights::EmberActionExecutionStatus::Completed);
    CHECK(exact_limit_result.dispatches == 32U);
    CHECK(exact_limit.calls == 32U);

    const auto nested = compile_source(nested_source(), registry);
    CHECK(nested.executable != nullptr);
    if (nested.executable != nullptr) {
        request.limits.maximum_results = 64U;
        request.limits.maximum_depth = 3U;
        const auto depth_budget = emberlights::execute_ember_action(
            *nested.executable, request, control);
        CHECK(depth_budget.status ==
            emberlights::EmberActionExecutionStatus::DepthBudgetExceeded);
        CHECK(control.calls == 0U);
    }

    const auto rejected_static = compile_source(repeated_dispatch_source(33U), registry);
    CHECK(rejected_static.executable == nullptr);
    CHECK(has_code(rejected_static.diagnostics, "EA_LIMIT_COMMAND_INVOCATIONS"));
}

void test_trace_truncation(
    const emberlights::GeneratedUiRegistryEmberActionView& registry) {
    const auto chain = compile_source(two_command_source("continue"), registry);
    CHECK(chain.executable != nullptr);
    if (chain.executable == nullptr) return;
    const std::array parameters{emberlights::EmberActionRuntimeValue::number(100.0)};
    emberlights::EmberActionExecutionRequest request;
    request.entry_point = emberlights::EmberActionEntryPoint::OnValue;
    request.parameters = parameters;
    request.limits.maximum_trace_entries = 3U;
    RecordingControl control;
    const auto result = emberlights::execute_ember_action(
        *chain.executable, request, control);
    CHECK(result.status == emberlights::EmberActionExecutionStatus::Completed);
    CHECK(result.trace_count == 3U);
    CHECK(result.trace_dropped > 0U);
    CHECK(result.trace_truncated());
    CHECK(result.trace[0].sequence == 0U);
    CHECK(result.trace[1].sequence == 1U);
    CHECK(result.trace[2].sequence == 2U);
}

void test_if_switch_selection_and_skipped_branch_non_dispatch(
    const emberlights::GeneratedUiRegistryEmberActionView& registry) {
    const auto first = compile_source(control_flow_source(), registry);
    const auto second = compile_source(control_flow_source(), registry);
    CHECK(first.executable != nullptr);
    CHECK(second.executable != nullptr);
    if (first.executable == nullptr || second.executable == nullptr) {
        report_compile_failure(first);
        return;
    }
    CHECK(first.executable->execution_digest == second.executable->execution_digest);
    CHECK(emberlights::kEmberActionExecutableIrCompilerGeneration == 3U);

    emberlights::EmberActionExecutionRequest request;
    request.entry_point = emberlights::EmberActionEntryPoint::OnPress;

    const std::array tap_parameters{
        emberlights::EmberActionRuntimeValue::boolean(true),
        emberlights::EmberActionRuntimeValue::integer(5),
        emberlights::EmberActionRuntimeValue::text_value(
            emberlights::EmberActionRuntimeValueKind::Enum, "tap")};
    request.parameters = tap_parameters;
    RecordingControl tap;
    const auto tapped = emberlights::execute_ember_action(
        *first.executable, request, tap);
    CHECK(tapped.status == emberlights::EmberActionExecutionStatus::Completed);
    CHECK(tapped.result == emberlights::UiInvocationResult::NoChange);
    CHECK(tapped.dispatches == 1U);
    CHECK(tap.calls == 1U);
    CHECK(tapped.expression_operations == 7U);
    CHECK(std::count_if(
        tapped.trace.begin(),
        tapped.trace.begin() + static_cast<std::ptrdiff_t>(tapped.trace_count),
        [](const auto& entry) {
            return entry.event == emberlights::EmberActionTraceEvent::BranchSelected;
        }) == 2);

    RecordingControl deterministic;
    const auto repeated = emberlights::execute_ember_action(
        *second.executable, request, deterministic);
    CHECK(repeated.status == tapped.status);
    CHECK(repeated.result == tapped.result);
    CHECK(repeated.trace_count == tapped.trace_count);
    CHECK(std::equal(
        tapped.trace.begin(),
        tapped.trace.begin() + static_cast<std::ptrdiff_t>(tapped.trace_count),
        repeated.trace.begin()));

    const std::array false_parameters{
        emberlights::EmberActionRuntimeValue::boolean(false),
        emberlights::EmberActionRuntimeValue::integer(10),
        emberlights::EmberActionRuntimeValue::text_value(
            emberlights::EmberActionRuntimeValueKind::Enum, "tap")};
    request.parameters = false_parameters;
    RecordingControl disabled;
    const auto disabled_result = emberlights::execute_ember_action(
        *first.executable, request, disabled);
    CHECK(disabled_result.status == emberlights::EmberActionExecutionStatus::Completed);
    CHECK(disabled_result.result == emberlights::UiInvocationResult::Unsupported);
    CHECK(disabled_result.dispatches == 0U);
    CHECK(disabled.calls == 0U);

    const std::array bpm_parameters{
        emberlights::EmberActionRuntimeValue::boolean(true),
        emberlights::EmberActionRuntimeValue::integer(9),
        emberlights::EmberActionRuntimeValue::text_value(
            emberlights::EmberActionRuntimeValueKind::Enum, "bpm")};
    request.parameters = bpm_parameters;
    RecordingControl bpm;
    const auto bpm_result = emberlights::execute_ember_action(
        *first.executable, request, bpm);
    CHECK(bpm_result.result == emberlights::UiInvocationResult::Unavailable);
    CHECK(bpm_result.dispatches == 0U);
    CHECK(bpm.calls == 0U);

    const std::array idle_parameters{
        emberlights::EmberActionRuntimeValue::boolean(true),
        emberlights::EmberActionRuntimeValue::integer(9),
        emberlights::EmberActionRuntimeValue::text_value(
            emberlights::EmberActionRuntimeValueKind::Enum, "idle")};
    request.parameters = idle_parameters;
    RecordingControl idle;
    const auto idle_result = emberlights::execute_ember_action(
        *first.executable, request, idle);
    CHECK(idle_result.result == emberlights::UiInvocationResult::Accepted);
    CHECK(idle_result.dispatches == 0U);
    CHECK(idle.calls == 0U);
    CHECK(idle_result.expression_operations == 8U);

    const auto integer_switch = compile_source(integer_switch_source(), registry);
    CHECK(integer_switch.executable != nullptr);
    if (integer_switch.executable != nullptr) {
        const std::array zero{emberlights::EmberActionRuntimeValue::integer(0)};
        request.parameters = zero;
        RecordingControl zero_control;
        const auto zero_result = emberlights::execute_ember_action(
            *integer_switch.executable, request, zero_control);
        CHECK(zero_result.result == emberlights::UiInvocationResult::Accepted);
        CHECK(zero_control.calls == 0U);

        const std::array one{emberlights::EmberActionRuntimeValue::integer(1)};
        request.parameters = one;
        RecordingControl one_control;
        const auto one_result = emberlights::execute_ember_action(
            *integer_switch.executable, request, one_control);
        CHECK(one_result.result == emberlights::UiInvocationResult::Accepted);
        CHECK(one_control.calls == 1U);
    }

    const auto string_switch = compile_source(string_switch_source(), registry);
    CHECK(string_switch.executable != nullptr);
    if (string_switch.executable != nullptr) {
        const std::array one{emberlights::EmberActionRuntimeValue::text_value(
            emberlights::EmberActionRuntimeValueKind::String, "one")};
        request.parameters = one;
        RecordingControl one_control;
        const auto one_result = emberlights::execute_ember_action(
            *string_switch.executable, request, one_control);
        CHECK(one_result.result == emberlights::UiInvocationResult::Accepted);
        CHECK(one_control.calls == 1U);

        const std::array other{emberlights::EmberActionRuntimeValue::text_value(
            emberlights::EmberActionRuntimeValueKind::String, "other")};
        request.parameters = other;
        RecordingControl other_control;
        const auto other_result = emberlights::execute_ember_action(
            *string_switch.executable, request, other_control);
        CHECK(other_result.result == emberlights::UiInvocationResult::Unavailable);
        CHECK(other_control.calls == 0U);
    }

    auto literal_comparison_source = control_flow_source();
    CHECK(replace_once(literal_comparison_source,
        "{\"source\": \"parameter\", \"path\": \"enabled\"}",
        "{\"op\": \"equal\", \"args\": [{\"literal\": true}, {\"literal\": true}]}"));
    const auto literal_comparison = compile_source(literal_comparison_source, registry);
    CHECK(literal_comparison.executable != nullptr);
    if (literal_comparison.executable != nullptr) {
        request.parameters = tap_parameters;
        RecordingControl literal_control;
        const auto literal_result = emberlights::execute_ember_action(
            *literal_comparison.executable, request, literal_control);
        CHECK(literal_result.status ==
            emberlights::EmberActionExecutionStatus::Completed);
        CHECK(literal_control.calls == 1U);
    }

    const auto number_predicate = compile_source(number_predicate_source(), registry);
    CHECK(number_predicate.executable != nullptr);
    if (number_predicate.executable != nullptr) {
        const std::array integer_for_number{
            emberlights::EmberActionRuntimeValue::boolean(true),
            emberlights::EmberActionRuntimeValue::integer(6),
            emberlights::EmberActionRuntimeValue::text_value(
                emberlights::EmberActionRuntimeValueKind::Enum, "tap")};
        request.parameters = integer_for_number;
        RecordingControl integer_for_number_control;
        const auto integer_for_number_result = emberlights::execute_ember_action(
            *number_predicate.executable, request, integer_for_number_control);
        CHECK(integer_for_number_result.status ==
            emberlights::EmberActionExecutionStatus::Completed);
        CHECK(integer_for_number_result.dispatches == 1U);
        CHECK(integer_for_number_control.calls == 1U);
    } else {
        report_compile_failure(number_predicate);
    }
}

void test_control_flow_fail_closed_and_budgets(
    const emberlights::GeneratedUiRegistryEmberActionView& registry) {
    auto unknown_if = control_flow_source();
    CHECK(replace_once(unknown_if,
        "\"then\": \"node.switch\",",
        "\"then\": \"node.switch\", \"invented\": true,"));
    const auto unknown_if_result = compile_source(unknown_if, registry);
    CHECK(unknown_if_result.executable == nullptr);
    CHECK(has_code(unknown_if_result.diagnostics, "EA_EXEC_IR_IF_PROPERTY"));

    auto unknown_switch = control_flow_source();
    CHECK(replace_once(unknown_switch,
        "\"default\": \"node.idle\"",
        "\"default\": \"node.idle\", \"pattern\": \".*\""));
    const auto unknown_switch_result = compile_source(unknown_switch, registry);
    CHECK(unknown_switch_result.executable == nullptr);
    CHECK(has_code(
        unknown_switch_result.diagnostics, "EA_EXEC_IR_SWITCH_PROPERTY"));

    auto unknown_case = control_flow_source();
    CHECK(replace_once(unknown_case,
        "{\"match\": \"tap\", \"node\": \"node.tap\"}",
        "{\"match\": \"tap\", \"node\": \"node.tap\", \"regex\": true}"));
    const auto unknown_case_result = compile_source(unknown_case, registry);
    CHECK(unknown_case_result.executable == nullptr);
    CHECK(has_code(
        unknown_case_result.diagnostics, "EA_EXEC_IR_SWITCH_CASE_PROPERTY"));

    auto wrong_predicate = control_flow_source();
    CHECK(replace_once(wrong_predicate,
        "{\"source\": \"parameter\", \"path\": \"enabled\"}",
        "{\"source\": \"parameter\", \"path\": \"level\"}"));
    const auto wrong_predicate_result = compile_source(wrong_predicate, registry);
    CHECK(wrong_predicate_result.executable == nullptr);
    CHECK(has_code(
        wrong_predicate_result.diagnostics, "EA_EXEC_IR_PREDICATE_TYPE"));

    auto invalid_operator = control_flow_source();
    CHECK(replace_once(invalid_operator, "\"op\": \"greaterOrEqual\"",
        "\"op\": \"contains\""));
    const auto invalid_operator_result = compile_source(invalid_operator, registry);
    CHECK(invalid_operator_result.executable == nullptr);
    CHECK(has_code(
        invalid_operator_result.diagnostics, "EA_EXEC_IR_PREDICATE_OPERATOR"));

    auto invalid_arity = control_flow_source();
    CHECK(replace_once(invalid_arity,
        "{\"source\": \"parameter\", \"path\": \"level\"},\n            {\"literal\": 5}",
        "{\"source\": \"parameter\", \"path\": \"level\"}"));
    const auto invalid_arity_result = compile_source(invalid_arity, registry);
    CHECK(invalid_arity_result.executable == nullptr);
    CHECK(has_code(invalid_arity_result.diagnostics, "EA_EXPRESSION_ARITY") ||
        has_code(
            invalid_arity_result.diagnostics, "EA_EXEC_IR_PREDICATE_ARITY"));

    auto state_predicate = control_flow_source();
    CHECK(replace_once(state_predicate,
        "{\"source\": \"parameter\", \"path\": \"enabled\"}",
        "{\"source\": \"state\", \"path\": \"transport.bpm\"}"));
    const auto state_predicate_result = compile_source(state_predicate, registry);
    CHECK(state_predicate_result.executable == nullptr);
    CHECK(has_code(state_predicate_result.diagnostics, "EA_REQUIREMENT_STATE") ||
        has_code(
            state_predicate_result.diagnostics,
            "EA_EXEC_IR_STATE_DEPENDENCY_UNSUPPORTED"));

    auto wrong_switch_type = control_flow_source();
    CHECK(replace_once(wrong_switch_type,
        "{\"source\": \"parameter\", \"path\": \"mode\"}",
        "{\"source\": \"parameter\", \"path\": \"enabled\"}"));
    const auto wrong_switch_type_result = compile_source(wrong_switch_type, registry);
    CHECK(wrong_switch_type_result.executable == nullptr);
    CHECK(has_code(
        wrong_switch_type_result.diagnostics, "EA_EXEC_IR_SWITCH_TYPE"));

    auto wrong_match = integer_switch_source();
    CHECK(replace_once(wrong_match, "{\"match\": 0", "{\"match\": \"zero\""));
    const auto wrong_match_result = compile_source(wrong_match, registry);
    CHECK(wrong_match_result.executable == nullptr);
    CHECK(has_code(wrong_match_result.diagnostics, "EA_EXEC_IR_LITERAL_TYPE"));

    auto duplicate_match = integer_switch_source();
    CHECK(replace_once(duplicate_match, "{\"match\": 1", "{\"match\": 0"));
    const auto duplicate_match_result = compile_source(duplicate_match, registry);
    CHECK(duplicate_match_result.executable == nullptr);
    CHECK(has_code(
        duplicate_match_result.diagnostics, "EA_EXEC_IR_SWITCH_CASE_DUPLICATE"));

    auto unknown_parameter = control_flow_source();
    CHECK(replace_once(unknown_parameter, "\"path\": \"level\"",
        "\"path\": \"missing\""));
    const auto unknown_parameter_result = compile_source(unknown_parameter, registry);
    CHECK(unknown_parameter_result.executable == nullptr);
    CHECK(has_code(
        unknown_parameter_result.diagnostics, "EA_REFERENCE_PARAMETER"));

    auto integer_max_overflow = integer_switch_source();
    CHECK(replace_once(integer_max_overflow, "{\"match\": 0",
        "{\"match\": 9223372036854775808"));
    const auto integer_max_overflow_result = compile_source(
        integer_max_overflow, registry);
    CHECK(integer_max_overflow_result.executable == nullptr);
    CHECK(has_code(integer_max_overflow_result.diagnostics,
        "EA_JSON_NUMBER_PRECISION") ||
        has_code(integer_max_overflow_result.diagnostics,
            "EA_EXEC_IR_LITERAL_INTEGER"));

    const auto chain = compile_source(control_flow_source(), registry);
    CHECK(chain.executable != nullptr);
    if (chain.executable == nullptr) return;
    const std::array parameters{
        emberlights::EmberActionRuntimeValue::boolean(true),
        emberlights::EmberActionRuntimeValue::integer(5),
        emberlights::EmberActionRuntimeValue::text_value(
            emberlights::EmberActionRuntimeValueKind::Enum, "tap")};
    emberlights::EmberActionExecutionRequest request;
    request.entry_point = emberlights::EmberActionEntryPoint::OnPress;
    request.parameters = parameters;
    request.limits.maximum_expression_operations = 4U;
    RecordingControl expression_control;
    const auto expression_budget = emberlights::execute_ember_action(
        *chain.executable, request, expression_control);
    CHECK(expression_budget.status ==
        emberlights::EmberActionExecutionStatus::ExpressionBudgetExceeded);
    CHECK(expression_budget.dispatches == 0U);
    CHECK(expression_control.calls == 0U);

    request.limits.maximum_expression_operations =
        emberlights::kEmberActionExecutionMaximumExpressionOperations;
    request.limits.maximum_dispatches = 0U;
    RecordingControl dispatch_control;
    const auto dispatch_budget = emberlights::execute_ember_action(
        *chain.executable, request, dispatch_control);
    CHECK(dispatch_budget.status ==
        emberlights::EmberActionExecutionStatus::DispatchBudgetExceeded);
    CHECK(dispatch_budget.dispatches == 0U);
    CHECK(dispatch_control.calls == 0U);

    request.limits.maximum_dispatches =
        emberlights::kEmberActionExecutionMaximumDispatches;
    request.limits.maximum_expression_operations = 6U;
    const std::array idle_parameters{
        emberlights::EmberActionRuntimeValue::boolean(true),
        emberlights::EmberActionRuntimeValue::integer(5),
        emberlights::EmberActionRuntimeValue::text_value(
            emberlights::EmberActionRuntimeValueKind::Enum, "idle")};
    request.parameters = idle_parameters;
    RecordingControl switch_work_control;
    const auto switch_work_budget = emberlights::execute_ember_action(
        *chain.executable, request, switch_work_control);
    CHECK(switch_work_budget.status ==
        emberlights::EmberActionExecutionStatus::ExpressionBudgetExceeded);
    CHECK(switch_work_budget.dispatches == 0U);
    CHECK(switch_work_control.calls == 0U);

    request.parameters = parameters;
    request.limits.maximum_expression_operations =
        emberlights::kEmberActionExecutionMaximumExpressionOperations;
    request.limits.maximum_node_visits = 4U;
    RecordingControl node_control;
    const auto node_budget = emberlights::execute_ember_action(
        *chain.executable, request, node_control);
    CHECK(node_budget.status ==
        emberlights::EmberActionExecutionStatus::NodeBudgetExceeded);
    CHECK(node_control.calls == 0U);

    request.limits.maximum_node_visits =
        emberlights::kEmberActionExecutionMaximumNodes;
    request.limits.maximum_depth = 3U;
    RecordingControl depth_control;
    const auto depth_budget = emberlights::execute_ember_action(
        *chain.executable, request, depth_control);
    CHECK(depth_budget.status ==
        emberlights::EmberActionExecutionStatus::DepthBudgetExceeded);
    CHECK(depth_control.calls == 0U);

    const auto branch_flood = compile_source(branch_dispatch_source(32U), registry);
    CHECK(branch_flood.executable != nullptr);
    if (branch_flood.executable != nullptr) {
        const std::array safe{emberlights::EmberActionRuntimeValue::boolean(false)};
        request.parameters = safe;
        request.limits.maximum_depth =
            emberlights::kEmberActionExecutionMaximumDepth;
        request.limits.maximum_dispatches = 31U;
        RecordingControl safe_control;
        const auto worst_case_dispatch = emberlights::execute_ember_action(
            *branch_flood.executable, request, safe_control);
        CHECK(worst_case_dispatch.status ==
            emberlights::EmberActionExecutionStatus::DispatchBudgetExceeded);
        CHECK(worst_case_dispatch.dispatches == 0U);
        CHECK(safe_control.calls == 0U);
    }
    const auto branch_overflow = compile_source(branch_dispatch_source(33U), registry);
    CHECK(branch_overflow.executable == nullptr);
    CHECK(has_code(
        branch_overflow.diagnostics, "EA_LIMIT_COMMAND_INVOCATIONS"));

    const auto branch_depth = compile_source(branch_depth_source(), registry);
    CHECK(branch_depth.executable != nullptr);
    if (branch_depth.executable != nullptr) {
        const std::array safe{emberlights::EmberActionRuntimeValue::boolean(false)};
        request.parameters = safe;
        request.limits.maximum_dispatches =
            emberlights::kEmberActionExecutionMaximumDispatches;
        request.limits.maximum_depth = 4U;
        RecordingControl safe_control;
        const auto worst_case_depth = emberlights::execute_ember_action(
            *branch_depth.executable, request, safe_control);
        CHECK(worst_case_depth.status ==
            emberlights::EmberActionExecutionStatus::DepthBudgetExceeded);
        CHECK(worst_case_depth.dispatches == 0U);
        CHECK(safe_control.calls == 0U);
    }

    request.parameters = parameters;
    request.limits.maximum_depth =
        emberlights::kEmberActionExecutionMaximumDepth;
    request.limits.maximum_trace_entries = 2U;
    RecordingControl trace_control;
    const auto truncated = emberlights::execute_ember_action(
        *chain.executable, request, trace_control);
    CHECK(truncated.status == emberlights::EmberActionExecutionStatus::Completed);
    CHECK(truncated.trace_count == 2U);
    CHECK(truncated.trace_dropped > 0U);
}

void test_on_result_selection_hard_stops_and_cancellation(
    const emberlights::GeneratedUiRegistryEmberActionView& registry) {
    const auto first = compile_source(on_result_source(), registry);
    const auto second = compile_source(on_result_source(), registry);
    CHECK(first.executable != nullptr);
    CHECK(second.executable != nullptr);
    if (first.executable == nullptr || second.executable == nullptr) {
        report_compile_failure(first);
        return;
    }
    CHECK(first.executable->execution_digest == second.executable->execution_digest);
    const auto route = std::find_if(
        first.executable->nodes.begin(), first.executable->nodes.end(),
        [](const auto& node) {
            return node.kind == emberlights::EmberActionIrNodeKind::OnResult;
        });
    CHECK(route != first.executable->nodes.end());
    if (route == first.executable->nodes.end()) return;
    const std::array expected_cases{
        emberlights::UiInvocationResult::Accepted,
        emberlights::UiInvocationResult::NoChange,
        emberlights::UiInvocationResult::Unavailable,
        emberlights::UiInvocationResult::InvalidArguments,
        emberlights::UiInvocationResult::ValidationFailed,
        emberlights::UiInvocationResult::Unsupported};
    CHECK(route->on_result_cases.size() == expected_cases.size());
    if (route->on_result_cases.size() == expected_cases.size()) {
        for (std::size_t index = 0U; index < expected_cases.size(); ++index) {
            CHECK(route->on_result_cases[index].match == expected_cases[index]);
        }
    }
    const auto& entry = first.executable->entry_points[
        static_cast<std::size_t>(emberlights::EmberActionEntryPoint::OnPress)];
    CHECK(entry.maximum_dispatches == 2U);
    CHECK(entry.maximum_expression_operations == 7U);

    emberlights::EmberActionExecutionRequest request;
    request.entry_point = emberlights::EmberActionEntryPoint::OnPress;

    RecordingControl accepted_control;
    accepted_control.scripted_results[0] = emberlights::UiInvocationResult::Accepted;
    accepted_control.scripted_count = 1U;
    const auto accepted = emberlights::execute_ember_action(
        *first.executable, request, accepted_control);
    CHECK(accepted.status == emberlights::EmberActionExecutionStatus::Completed);
    CHECK(accepted.result == emberlights::UiInvocationResult::Accepted);
    CHECK(accepted.dispatches == 1U);
    CHECK(accepted.expression_operations == 2U);
    CHECK(accepted_control.calls == 1U);
    const auto accepted_branch = std::find_if(
        accepted.trace.begin(),
        accepted.trace.begin() + static_cast<std::ptrdiff_t>(accepted.trace_count),
        [](const auto& item) {
            return item.event == emberlights::EmberActionTraceEvent::BranchSelected;
        });
    CHECK(accepted_branch !=
        accepted.trace.begin() + static_cast<std::ptrdiff_t>(accepted.trace_count));
    if (accepted_branch !=
        accepted.trace.begin() + static_cast<std::ptrdiff_t>(accepted.trace_count)) {
        CHECK(accepted_branch->has_result);
        CHECK(accepted_branch->result == emberlights::UiInvocationResult::Accepted);
        CHECK(accepted_branch->branch_index == 0U);
    }

    RecordingControl deterministic_control;
    deterministic_control.scripted_results[0] = emberlights::UiInvocationResult::Accepted;
    deterministic_control.scripted_count = 1U;
    const auto deterministic = emberlights::execute_ember_action(
        *second.executable, request, deterministic_control);
    CHECK(deterministic.trace_count == accepted.trace_count);
    CHECK(std::equal(
        accepted.trace.begin(),
        accepted.trace.begin() + static_cast<std::ptrdiff_t>(accepted.trace_count),
        deterministic.trace.begin()));

    RecordingControl no_change_control;
    no_change_control.scripted_results[0] = emberlights::UiInvocationResult::NoChange;
    no_change_control.scripted_count = 1U;
    const auto no_change = emberlights::execute_ember_action(
        *first.executable, request, no_change_control);
    CHECK(no_change.status == emberlights::EmberActionExecutionStatus::Completed);
    CHECK(no_change.result == emberlights::UiInvocationResult::NoChange);
    CHECK(no_change.dispatches == 1U);
    CHECK(no_change.expression_operations == 3U);

    for (const auto result : std::array{
             emberlights::UiInvocationResult::InvalidArguments,
             emberlights::UiInvocationResult::ValidationFailed,
             emberlights::UiInvocationResult::Unsupported}) {
        RecordingControl routed_rejection_control;
        routed_rejection_control.scripted_results[0] = result;
        routed_rejection_control.scripted_count = 1U;
        const auto routed_rejection = emberlights::execute_ember_action(
            *first.executable, request, routed_rejection_control);
        CHECK(routed_rejection.status ==
            emberlights::EmberActionExecutionStatus::Completed);
        CHECK(routed_rejection.result == result);
        CHECK(routed_rejection.dispatches == 1U);
        CHECK(routed_rejection_control.calls == 1U);
    }

    RecordingControl fallback_control;
    fallback_control.scripted_results[0] = emberlights::UiInvocationResult::Unavailable;
    fallback_control.scripted_results[1] = emberlights::UiInvocationResult::Accepted;
    fallback_control.scripted_count = 2U;
    const auto fallback = emberlights::execute_ember_action(
        *first.executable, request, fallback_control);
    CHECK(fallback.status == emberlights::EmberActionExecutionStatus::Completed);
    CHECK(fallback.result == emberlights::UiInvocationResult::Unavailable);
    CHECK(fallback.dispatches == 2U);
    CHECK(fallback.expression_operations == 4U);
    CHECK(fallback_control.calls == 2U);

    RecordingControl default_control;
    default_control.scripted_results[0] = emberlights::UiInvocationResult::NotFound;
    default_control.scripted_count = 1U;
    const auto defaulted = emberlights::execute_ember_action(
        *first.executable, request, default_control);
    CHECK(defaulted.status == emberlights::EmberActionExecutionStatus::Completed);
    CHECK(defaulted.result == emberlights::UiInvocationResult::NotFound);
    CHECK(defaulted.dispatches == 1U);
    CHECK(defaulted.expression_operations == 7U);
    CHECK(default_control.calls == 1U);

    for (const auto result : std::array{
             emberlights::UiInvocationResult::QueueFull,
             emberlights::UiInvocationResult::SafetyRejected}) {
        RecordingControl hard_stop_control;
        hard_stop_control.scripted_results[0] = result;
        hard_stop_control.scripted_count = 1U;
        const auto hard_stop = emberlights::execute_ember_action(
            *first.executable, request, hard_stop_control);
        CHECK(hard_stop.status == emberlights::EmberActionExecutionStatus::Completed);
        CHECK(hard_stop.result == result);
        CHECK(hard_stop.dispatches == 1U);
        CHECK(hard_stop.expression_operations == 0U);
        CHECK(hard_stop_control.calls == 1U);
        CHECK(std::none_of(
            hard_stop.trace.begin(),
            hard_stop.trace.begin() +
                static_cast<std::ptrdiff_t>(hard_stop.trace_count),
            [](const auto& item) {
                return item.event ==
                    emberlights::EmberActionTraceEvent::BranchSelected;
            }));
    }

    RecordingControl fault_control;
    fault_control.scripted_results[0] = emberlights::UiInvocationResult::InternalError;
    fault_control.scripted_count = 1U;
    const auto fault = emberlights::execute_ember_action(
        *first.executable, request, fault_control);
    CHECK(fault.status == emberlights::EmberActionExecutionStatus::DispatcherFault);
    CHECK(fault.result == emberlights::UiInvocationResult::InternalError);
    CHECK(fault.dispatches == 1U);
    CHECK(fault.expression_operations == 0U);
    CHECK(fault_control.calls == 1U);

    std::atomic_bool cancelled{false};
    RecordingControl cancellation_control;
    cancellation_control.scripted_results[0] =
        emberlights::UiInvocationResult::Unavailable;
    cancellation_control.scripted_count = 1U;
    cancellation_control.cancel_after_first = &cancelled;
    request.cancellation.cancelled = &cancelled;
    const auto cancellation = emberlights::execute_ember_action(
        *first.executable, request, cancellation_control);
    CHECK(cancellation.status == emberlights::EmberActionExecutionStatus::Cancelled);
    CHECK(cancellation.dispatches == 1U);
    CHECK(cancellation_control.calls == 1U);
}

void test_on_result_fail_closed_budgets_and_tamper(
    const emberlights::GeneratedUiRegistryEmberActionView& registry) {
    auto before_definition_source = on_result_source();
    CHECK(replace_once(before_definition_source,
        "[\"node.primary\", \"node.route\"]",
        "[\"node.route\", \"node.primary\"]"));
    const auto before_definition = compile_source(before_definition_source, registry);
    CHECK(before_definition.executable == nullptr);
    CHECK(has_code(
        before_definition.diagnostics, "EA_EXEC_IR_RESULT_BEFORE_DEFINITION"));

    auto literal_source = on_result_source();
    CHECK(replace_once(literal_source,
        "{\"source\": \"nodeOutput\", \"path\": \"primaryResult\"}",
        "{\"literal\": \"Accepted\"}"));
    const auto literal = compile_source(literal_source, registry);
    CHECK(literal.executable == nullptr);
    CHECK(has_code(literal.diagnostics, "EA_TYPE_RESULT_REQUIRED") ||
        has_code(literal.diagnostics, "EA_EXEC_IR_ON_RESULT_SOURCE"));

    auto unknown_property_source = on_result_source();
    CHECK(replace_once(unknown_property_source,
        "\"kind\": \"OnResult\",",
        "\"kind\": \"OnResult\", \"retry\": true,"));
    const auto unknown_property = compile_source(unknown_property_source, registry);
    CHECK(unknown_property.executable == nullptr);
    CHECK(has_code(
        unknown_property.diagnostics, "EA_EXEC_IR_ON_RESULT_PROPERTY"));

    for (const auto name : std::array<std::string_view, 3>{
             "MissingTarget", "Cancelled", "StartedAsync"}) {
        auto source = on_result_source();
        CHECK(replace_once(source, "\"Accepted\": \"node.accepted\"",
            std::string{"\""}.append(name).append("\": \"node.accepted\"")));
        const auto reserved = compile_source(source, registry);
        CHECK(reserved.executable == nullptr);
        CHECK(has_code(
            reserved.diagnostics, "EA_EXEC_IR_ON_RESULT_CASE_RESERVED"));
    }

    for (const auto name : std::array<std::string_view, 3>{
             "QueueFull", "SafetyRejected", "InternalError"}) {
        auto source = on_result_source();
        CHECK(replace_once(source, "\"Accepted\": \"node.accepted\"",
            std::string{"\""}.append(name).append("\": \"node.accepted\"")));
        const auto hard_stop = compile_source(source, registry);
        CHECK(hard_stop.executable == nullptr);
        CHECK(has_code(
            hard_stop.diagnostics, "EA_EXEC_IR_ON_RESULT_CASE_HARD_STOP"));
    }

    auto non_schema_result_source = on_result_source();
    CHECK(replace_once(non_schema_result_source,
        "\"Accepted\": \"node.accepted\"",
        "\"NotFound\": \"node.accepted\""));
    const auto non_schema_result = compile_source(non_schema_result_source, registry);
    CHECK(non_schema_result.executable == nullptr);
    CHECK(!non_schema_result.diagnostics.empty());

    auto unknown_target_source = on_result_source();
    CHECK(replace_once(unknown_target_source,
        "\"Accepted\": \"node.accepted\"",
        "\"Accepted\": \"node.missing\""));
    const auto unknown_target = compile_source(unknown_target_source, registry);
    CHECK(unknown_target.executable == nullptr);
    CHECK(!unknown_target.diagnostics.empty());

    const auto chain = compile_source(on_result_source(), registry);
    CHECK(chain.executable != nullptr);
    if (chain.executable == nullptr) return;
    emberlights::EmberActionExecutionRequest request;
    request.entry_point = emberlights::EmberActionEntryPoint::OnPress;

    request.limits.maximum_dispatches = 1U;
    RecordingControl dispatch_control;
    const auto dispatch_budget = emberlights::execute_ember_action(
        *chain.executable, request, dispatch_control);
    CHECK(dispatch_budget.status ==
        emberlights::EmberActionExecutionStatus::DispatchBudgetExceeded);
    CHECK(dispatch_control.calls == 0U);

    request.limits.maximum_dispatches =
        emberlights::kEmberActionExecutionMaximumDispatches;
    request.limits.maximum_expression_operations = 6U;
    RecordingControl expression_control;
    const auto expression_budget = emberlights::execute_ember_action(
        *chain.executable, request, expression_control);
    CHECK(expression_budget.status ==
        emberlights::EmberActionExecutionStatus::ExpressionBudgetExceeded);
    CHECK(expression_control.calls == 0U);

    request.limits.maximum_expression_operations =
        emberlights::kEmberActionExecutionMaximumExpressionOperations;
    request.limits.maximum_node_visits = 5U;
    RecordingControl node_control;
    const auto node_budget = emberlights::execute_ember_action(
        *chain.executable, request, node_control);
    CHECK(node_budget.status ==
        emberlights::EmberActionExecutionStatus::NodeBudgetExceeded);
    CHECK(node_control.calls == 0U);

    request.limits.maximum_node_visits =
        emberlights::kEmberActionExecutionMaximumNodes;
    request.limits.maximum_depth = 3U;
    RecordingControl depth_control;
    const auto depth_budget = emberlights::execute_ember_action(
        *chain.executable, request, depth_control);
    CHECK(depth_budget.status ==
        emberlights::EmberActionExecutionStatus::DepthBudgetExceeded);
    CHECK(depth_control.calls == 0U);

    request.limits.maximum_depth = emberlights::kEmberActionExecutionMaximumDepth;
    request.limits.maximum_trace_entries = 3U;
    RecordingControl trace_control;
    const auto truncated = emberlights::execute_ember_action(
        *chain.executable, request, trace_control);
    CHECK(truncated.status == emberlights::EmberActionExecutionStatus::Completed);
    CHECK(truncated.trace_count == 3U);
    CHECK(truncated.trace_dropped > 0U);

    auto slot_tamper = *chain.executable;
    auto route = std::find_if(
        slot_tamper.nodes.begin(), slot_tamper.nodes.end(),
        [](const auto& node) {
            return node.kind == emberlights::EmberActionIrNodeKind::OnResult;
        });
    CHECK(route != slot_tamper.nodes.end());
    if (route == slot_tamper.nodes.end()) return;
    route->on_result_slot = static_cast<std::uint16_t>(slot_tamper.result_slot_count);
    RecordingControl slot_tamper_control;
    request.limits.maximum_trace_entries =
        emberlights::kEmberActionExecutionMaximumTraceEntries;
    const auto slot_tamper_result = emberlights::execute_ember_action(
        slot_tamper, request, slot_tamper_control);
    CHECK(slot_tamper_result.status == emberlights::EmberActionExecutionStatus::InvalidIr);
    CHECK(slot_tamper_result.dispatches == 0U);
    CHECK(slot_tamper_control.calls == 0U);

    auto match_tamper = *chain.executable;
    auto match_route = std::find_if(
        match_tamper.nodes.begin(), match_tamper.nodes.end(),
        [](const auto& node) {
            return node.kind == emberlights::EmberActionIrNodeKind::OnResult;
        });
    CHECK(match_route != match_tamper.nodes.end());
    if (match_route != match_tamper.nodes.end() &&
        !match_route->on_result_cases.empty()) {
        match_route->on_result_cases.front().match =
            emberlights::UiInvocationResult::QueueFull;
        RecordingControl match_tamper_control;
        const auto match_tamper_result = emberlights::execute_ember_action(
            match_tamper, request, match_tamper_control);
        CHECK(match_tamper_result.status ==
            emberlights::EmberActionExecutionStatus::InvalidIr);
        CHECK(match_tamper_control.calls == 0U);
    }

    auto target_tamper = *chain.executable;
    auto target_route = std::find_if(
        target_tamper.nodes.begin(), target_tamper.nodes.end(),
        [](const auto& node) {
            return node.kind == emberlights::EmberActionIrNodeKind::OnResult;
        });
    CHECK(target_route != target_tamper.nodes.end());
    if (target_route != target_tamper.nodes.end() &&
        target_route->on_result_default_node.has_value()) {
        target_route->on_result_default_node = static_cast<std::uint16_t>(
            target_tamper.nodes.size());
        RecordingControl target_tamper_control;
        const auto target_tamper_result = emberlights::execute_ember_action(
            target_tamper, request, target_tamper_control);
        CHECK(target_tamper_result.status ==
            emberlights::EmberActionExecutionStatus::InvalidIr);
        CHECK(target_tamper_control.calls == 0U);
    }
}

void test_copied_ir_tamper_rejected_before_dispatch(
    const emberlights::GeneratedUiRegistryEmberActionView& registry) {
    const auto chain = compile_source(control_flow_source(), registry);
    CHECK(chain.executable != nullptr);
    if (chain.executable == nullptr) return;
    const std::array parameters{
        emberlights::EmberActionRuntimeValue::boolean(true),
        emberlights::EmberActionRuntimeValue::integer(6),
        emberlights::EmberActionRuntimeValue::text_value(
            emberlights::EmberActionRuntimeValueKind::Enum, "tap")};
    emberlights::EmberActionExecutionRequest request;
    request.entry_point = emberlights::EmberActionEntryPoint::OnPress;
    request.parameters = parameters;

    auto dishonest_dispatch = *chain.executable;
    auto& dishonest_entry = dishonest_dispatch.entry_points[
        static_cast<std::size_t>(emberlights::EmberActionEntryPoint::OnPress)];
    CHECK(dishonest_entry.maximum_dispatches == 1U);
    dishonest_entry.maximum_dispatches = 0U;
    RecordingControl dishonest_dispatch_control;
    const auto dishonest_dispatch_result = emberlights::execute_ember_action(
        dishonest_dispatch, request, dishonest_dispatch_control);
    CHECK(dishonest_dispatch_result.status ==
        emberlights::EmberActionExecutionStatus::InvalidIr);
    CHECK(dishonest_dispatch_result.atomicity ==
        emberlights::EmberActionExecutionAtomicity::NotExecuted);
    CHECK(dishonest_dispatch_control.calls == 0U);

    auto dishonest_depth = *chain.executable;
    auto& dishonest_depth_entry = dishonest_depth.entry_points[
        static_cast<std::size_t>(emberlights::EmberActionEntryPoint::OnPress)];
    CHECK(dishonest_depth_entry.maximum_depth > 1U);
    dishonest_depth_entry.maximum_depth = 1U;
    RecordingControl dishonest_depth_control;
    const auto dishonest_depth_result = emberlights::execute_ember_action(
        dishonest_depth, request, dishonest_depth_control);
    CHECK(dishonest_depth_result.status ==
        emberlights::EmberActionExecutionStatus::InvalidIr);
    CHECK(dishonest_depth_control.calls == 0U);

    auto dishonest_expression = *chain.executable;
    auto& dishonest_expression_entry = dishonest_expression.entry_points[
        static_cast<std::size_t>(emberlights::EmberActionEntryPoint::OnPress)];
    CHECK(dishonest_expression_entry.maximum_expression_operations > 1U);
    dishonest_expression_entry.maximum_expression_operations = 1U;
    RecordingControl dishonest_expression_control;
    const auto dishonest_expression_result = emberlights::execute_ember_action(
        dishonest_expression, request, dishonest_expression_control);
    CHECK(dishonest_expression_result.status ==
        emberlights::EmberActionExecutionStatus::InvalidIr);
    CHECK(dishonest_expression_control.calls == 0U);

    auto cyclic = *chain.executable;
    const auto root = cyclic.entry_points[
        static_cast<std::size_t>(emberlights::EmberActionEntryPoint::OnPress)].root_node;
    cyclic.nodes[root].then_node = root;
    RecordingControl cyclic_control;
    const auto cyclic_result = emberlights::execute_ember_action(
        cyclic, request, cyclic_control);
    CHECK(cyclic_result.status == emberlights::EmberActionExecutionStatus::InvalidIr);
    CHECK(cyclic_result.node_visits == 0U);
    CHECK(cyclic_result.dispatches == 0U);
    CHECK(cyclic_control.calls == 0U);

    auto command_tamper = *chain.executable;
    const auto command_node = std::find_if(
        command_tamper.nodes.begin(), command_tamper.nodes.end(),
        [](const auto& node) {
            return node.kind == emberlights::EmberActionIrNodeKind::InvokeCommand;
        });
    CHECK(command_node != command_tamper.nodes.end());
    if (command_node != command_tamper.nodes.end()) {
        command_node->command = emberlights::UiCommandId::AutoloopNext;
        RecordingControl command_tamper_control;
        const auto command_tamper_result = emberlights::execute_ember_action(
            command_tamper, request, command_tamper_control);
        CHECK(command_tamper_result.status ==
            emberlights::EmberActionExecutionStatus::InvalidIr);
        CHECK(command_tamper_control.calls == 0U);
    }

    auto digest_tamper = *chain.executable;
    digest_tamper.execution_digest = "sha256:tampered-action-identity";
    RecordingControl digest_tamper_control;
    const auto digest_tamper_result = emberlights::execute_ember_action(
        digest_tamper, request, digest_tamper_control);
    CHECK(digest_tamper_result.status ==
        emberlights::EmberActionExecutionStatus::InvalidIr);
    CHECK(digest_tamper_result.dispatches == 0U);
    CHECK(digest_tamper_control.calls == 0U);

    const auto alternate_chain = compile_source(integer_switch_source(), registry);
    CHECK(alternate_chain.foundation != nullptr);
    if (alternate_chain.foundation != nullptr) {
        auto foundation_tamper = *chain.executable;
        foundation_tamper.foundation = alternate_chain.foundation;
        RecordingControl foundation_tamper_control;
        const auto foundation_tamper_result = emberlights::execute_ember_action(
            foundation_tamper, request, foundation_tamper_control);
        CHECK(foundation_tamper_result.status ==
            emberlights::EmberActionExecutionStatus::InvalidIr);
        CHECK(foundation_tamper_result.dispatches == 0U);
        CHECK(foundation_tamper_control.calls == 0U);
    }

    auto literal_tamper = *chain.executable;
    const auto switch_node = std::find_if(
        literal_tamper.nodes.begin(), literal_tamper.nodes.end(),
        [](const auto& node) {
            return node.kind == emberlights::EmberActionIrNodeKind::Switch;
        });
    CHECK(switch_node != literal_tamper.nodes.end());
    if (switch_node != literal_tamper.nodes.end()) {
        CHECK(!switch_node->switch_cases.empty());
        if (!switch_node->switch_cases.empty()) {
            switch_node->switch_cases.front().match.text = "bpm";
            RecordingControl literal_tamper_control;
            const auto literal_tamper_result = emberlights::execute_ember_action(
                literal_tamper, request, literal_tamper_control);
            CHECK(literal_tamper_result.status ==
                emberlights::EmberActionExecutionStatus::InvalidIr);
            CHECK(literal_tamper_control.calls == 0U);
        }
    }

    auto dishonest_switch = *chain.executable;
    const auto switch_index = dishonest_switch.nodes[root].then_node;
    CHECK(dishonest_switch.nodes[switch_index].kind ==
        emberlights::EmberActionIrNodeKind::Switch);
    auto& switch_entry = dishonest_switch.entry_points[
        static_cast<std::size_t>(emberlights::EmberActionEntryPoint::OnPress)];
    CHECK(switch_entry.maximum_expression_operations > 1U);
    switch_entry.maximum_expression_operations -= 1U;
    RecordingControl dishonest_switch_control;
    const auto dishonest_switch_result = emberlights::execute_ember_action(
        dishonest_switch, request, dishonest_switch_control);
    CHECK(dishonest_switch_result.status ==
        emberlights::EmberActionExecutionStatus::InvalidIr);
    CHECK(dishonest_switch_control.calls == 0U);
}

void test_unsupported_and_lifecycle_rejection(
    const emberlights::GeneratedUiRegistryEmberActionView& registry) {
    auto unsupported_node_source = control_flow_source();
    CHECK(replace_once(unsupported_node_source,
        "\"kind\": \"If\"", "\"kind\": \"Let\""));
    const auto unsupported = compile_source(unsupported_node_source, registry);
    CHECK(unsupported.executable == nullptr);
    CHECK(has_code(unsupported.diagnostics, "EA_EXPRESSION_REQUIRED") ||
        has_code(unsupported.diagnostics, "EA_EXEC_IR_NODE_UNSUPPORTED"));

    const auto state_dependency = compile_source(state_dependency_source(), registry);
    CHECK(state_dependency.executable == nullptr);
    CHECK(has_code(
        state_dependency.diagnostics,
        "EA_EXEC_IR_STATE_DEPENDENCY_UNSUPPORTED"));

    auto invalid_policy_source = two_command_source("stopOnError");
    CHECK(replace_once(invalid_policy_source, "stopOnError", "inventedPolicy"));
    const auto invalid_policy = compile_source(invalid_policy_source, registry);
    CHECK(invalid_policy.executable == nullptr);
    CHECK(has_code(invalid_policy.diagnostics, "EA_EXEC_IR_SEQUENCE_POLICY"));

    auto ambiguous_expression_source = two_command_source("continue");
    CHECK(replace_once(
        ambiguous_expression_source,
        "{\"source\": \"parameter\", \"path\": \"bpm\"}",
        "{\"literal\": 120, \"source\": \"parameter\", \"path\": \"bpm\"}"));
    const auto ambiguous_expression = compile_source(ambiguous_expression_source, registry);
    CHECK(ambiguous_expression.executable == nullptr);
    CHECK(has_code(
        ambiguous_expression.diagnostics,
        "EA_EXEC_IR_EXPRESSION_PROPERTY"));

    auto reserved_result_source = two_command_source("continue");
    CHECK(replace_once(
        reserved_result_source,
        "{\"source\": \"nodeOutput\", \"path\": \"tapResult\"}",
        "{\"literal\": \"Cancelled\"}"));
    const auto reserved_result = compile_source(reserved_result_source, registry);
    CHECK(reserved_result.executable == nullptr);
    CHECK(has_code(reserved_result.diagnostics, "EA_EXEC_IR_RETURN_RESULT"));

    const auto priority = compile_source(priority_source(), registry);
    CHECK(priority.executable == nullptr);
    CHECK(has_code(priority.diagnostics, "EA_EXEC_IR_REALTIME_CLASS_UNSUPPORTED"));

    const auto prepared = emberlights::prepare_ember_action_source(
        two_command_source("continue"), registry);
    CHECK(prepared.ok());
    if (!prepared.ok()) return;
    const auto foundation = emberlights::compile_ember_action_ir_foundation(
        prepared.prepared, registry);
    CHECK(foundation.ok());
    if (!foundation.ok()) return;
    const LifecycleRegistryView deprecated(registry);
    const auto executable = emberlights::compile_ember_action_executable_ir(
        foundation.ir, deprecated);
    CHECK(!executable.ok());
    CHECK(has_code(executable.diagnostics, "EA_EXEC_IR_COMMAND_LIFECYCLE"));
}

void test_repeated_execution_no_allocations_and_probe(
    const emberlights::GeneratedUiRegistryEmberActionView& registry) {
    const auto chain = compile_source(two_command_source("continue"), registry);
    CHECK(chain.executable != nullptr);
    if (chain.executable == nullptr) return;
    const std::array parameters{emberlights::EmberActionRuntimeValue::number(128.0)};
    emberlights::EmberActionExecutionRequest request;
    request.entry_point = emberlights::EmberActionEntryPoint::OnValue;
    request.parameters = parameters;
    RecordingControl control;

    const auto allocations_before = g_allocation_count.load(std::memory_order_relaxed);
    constexpr std::size_t iterations = 20000U;
    const auto started = std::chrono::steady_clock::now();
    for (std::size_t index = 0U; index < iterations; ++index) {
        const auto result = emberlights::execute_ember_action(
            *chain.executable, request, control);
        if (result.status != emberlights::EmberActionExecutionStatus::Completed) {
            ++g_failures;
            break;
        }
    }
    const auto elapsed = std::chrono::steady_clock::now() - started;
    const auto allocations_after = g_allocation_count.load(std::memory_order_relaxed);
    CHECK(allocations_after == allocations_before);
    CHECK(control.calls == iterations * 2U);
    const auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(
        elapsed).count();
    const auto nanoseconds_per_execution = nanoseconds /
        static_cast<long long>(iterations);
    std::cout << "Ember Action executor probe: " << nanoseconds_per_execution
              << " ns/execution, allocations="
              << (allocations_after - allocations_before) << '\n';
    CHECK(nanoseconds_per_execution < 50000LL);

    const auto branched = compile_source(control_flow_source(), registry);
    CHECK(branched.executable != nullptr);
    if (branched.executable == nullptr) return;
    const std::array branch_parameters{
        emberlights::EmberActionRuntimeValue::boolean(true),
        emberlights::EmberActionRuntimeValue::integer(6),
        emberlights::EmberActionRuntimeValue::text_value(
            emberlights::EmberActionRuntimeValueKind::Enum, "tap")};
    request.entry_point = emberlights::EmberActionEntryPoint::OnPress;
    request.parameters = branch_parameters;
    RecordingControl branch_control;
    const auto branch_allocations_before =
        g_allocation_count.load(std::memory_order_relaxed);
    const auto branch_started = std::chrono::steady_clock::now();
    for (std::size_t index = 0U; index < iterations; ++index) {
        const auto result = emberlights::execute_ember_action(
            *branched.executable, request, branch_control);
        if (result.status != emberlights::EmberActionExecutionStatus::Completed ||
            result.dispatches != 1U) {
            ++g_failures;
            break;
        }
    }
    const auto branch_elapsed = std::chrono::steady_clock::now() - branch_started;
    const auto branch_allocations_after =
        g_allocation_count.load(std::memory_order_relaxed);
    CHECK(branch_allocations_after == branch_allocations_before);
    CHECK(branch_control.calls == iterations);
    const auto branch_nanoseconds =
        std::chrono::duration_cast<std::chrono::nanoseconds>(branch_elapsed).count();
    const auto branch_nanoseconds_per_execution = branch_nanoseconds /
        static_cast<long long>(iterations);
    std::cout << "Ember Action If/Switch probe: "
              << branch_nanoseconds_per_execution << " ns/execution, allocations="
              << (branch_allocations_after - branch_allocations_before) << '\n';
    CHECK(branch_nanoseconds_per_execution < 50000LL);

    const auto result_flow = compile_source(on_result_source(), registry);
    CHECK(result_flow.executable != nullptr);
    if (result_flow.executable == nullptr) return;
    request.parameters = std::span<const emberlights::EmberActionRuntimeValue>{};
    RecordingControl result_flow_control;
    const auto result_flow_allocations_before =
        g_allocation_count.load(std::memory_order_relaxed);
    const auto result_flow_started = std::chrono::steady_clock::now();
    for (std::size_t index = 0U; index < iterations; ++index) {
        const auto result = emberlights::execute_ember_action(
            *result_flow.executable, request, result_flow_control);
        if (result.status != emberlights::EmberActionExecutionStatus::Completed ||
            result.result != emberlights::UiInvocationResult::Accepted ||
            result.dispatches != 1U || result.expression_operations != 2U) {
            ++g_failures;
            break;
        }
    }
    const auto result_flow_elapsed =
        std::chrono::steady_clock::now() - result_flow_started;
    const auto result_flow_allocations_after =
        g_allocation_count.load(std::memory_order_relaxed);
    CHECK(result_flow_allocations_after == result_flow_allocations_before);
    CHECK(result_flow_control.calls == iterations);
    const auto result_flow_nanoseconds =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            result_flow_elapsed).count();
    const auto result_flow_nanoseconds_per_execution = result_flow_nanoseconds /
        static_cast<long long>(iterations);
    std::cout << "Ember Action OnResult probe: "
              << result_flow_nanoseconds_per_execution
              << " ns/execution, allocations="
              << (result_flow_allocations_after -
                  result_flow_allocations_before) << '\n';
    CHECK(result_flow_nanoseconds_per_execution < 50000LL);
}

}  // namespace

int main() {
    const emberlights::GeneratedUiRegistryEmberActionView registry;
    CHECK(registry.registry_digest() == emberlights::kUiRegistryDigest);
    CHECK(emberlights::kUiCommandDefinitions.size() == 31U);
    CHECK(emberlights::kLiveCoreUiStates.size() == 54U);
    test_deterministic_execution_and_trace(registry);
    test_sequence_refusal_and_dispatcher_fault(registry);
    test_cancellation_and_fail_closed_context(registry);
    test_flood_and_hard_budgets(registry);
    test_trace_truncation(registry);
    test_if_switch_selection_and_skipped_branch_non_dispatch(registry);
    test_control_flow_fail_closed_and_budgets(registry);
    test_on_result_selection_hard_stops_and_cancellation(registry);
    test_on_result_fail_closed_budgets_and_tamper(registry);
    test_copied_ir_tamper_rejected_before_dispatch(registry);
    test_unsupported_and_lifecycle_rejection(registry);
    test_repeated_execution_no_allocations_and_probe(registry);
    if (g_failures == 0) {
        std::cout << "Ember Action executor tests passed\n";
    }
    return g_failures == 0 ? 0 : 1;
}
