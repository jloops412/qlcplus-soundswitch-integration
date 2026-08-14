#include "emberlights/ember_action_compiler.hpp"
#include "emberlights/ember_action_json.hpp"
#include "emberlights/ember_action_registry_adapter.hpp"
#include "emberlights/generated/ui_registry.generated.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

int g_failures = 0;

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ << " — " #condition "\n"; \
        ++g_failures; \
    } \
} while (false)

[[nodiscard]] std::string read_fixture(std::string_view relative) {
    const auto path = std::filesystem::path("../spec/ui/examples/actions") /
        std::filesystem::path(relative);
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

[[nodiscard]] bool has_code(
    const std::vector<emberlights::EmberActionDiagnostic>& diagnostics,
    std::string_view code) {
    return std::any_of(diagnostics.begin(), diagnostics.end(), [&](const auto& item) {
        return item.code == code;
    });
}

[[nodiscard]] bool replace_first(
    std::string& source,
    std::string_view needle,
    std::string_view replacement) {
    const auto position = source.find(needle);
    if (position == std::string::npos) return false;
    source.replace(position, needle.size(), replacement);
    return true;
}

class MockRegistry final : public emberlights::EmberActionRegistryView {
public:
    MockRegistry() {
        emberlights::EmberActionValueContract normalized;
        normalized.kind = emberlights::EmberActionValueKind::Number;
        normalized.unit = "normalized";
        normalized.minimum = 0.0;
        normalized.maximum = 1.0;

        emberlights::EmberActionValueContract result;
        result.kind = emberlights::EmberActionValueKind::Result;

        emberlights::EmberActionCommandContract command;
        command.id = "com.test.command.set";
        command.arguments.push_back({"value", normalized, true});
        command.result = result;
        command.required_capabilities.push_back("com.test.capability.basic");
        command.realtime_class = emberlights::EmberActionRealtimeClass::RunnerCommand;
        command.parallel_compatible = true;
        commands_.emplace(command.id, std::move(command));

        emberlights::EmberActionValueContract boolean;
        boolean.kind = emberlights::EmberActionValueKind::Boolean;
        emberlights::EmberActionStateContract state;
        state.id = "com.test.state.ready";
        state.value = boolean;
        states_.emplace(state.id, std::move(state));

        emberlights::EmberActionCapabilityContract capability;
        capability.id = "com.test.capability.basic";
        capabilities_.emplace(capability.id, std::move(capability));
    }

    [[nodiscard]] std::string_view registry_digest() const noexcept override {
        return "sha256:mock-registry-generation-1";
    }

    [[nodiscard]] const emberlights::EmberActionCommandContract* find_command(
        std::string_view id) const noexcept override {
        const auto iterator = commands_.find(std::string(id));
        return iterator == commands_.end() ? nullptr : &iterator->second;
    }

    [[nodiscard]] const emberlights::EmberActionStateContract* find_state(
        std::string_view id) const noexcept override {
        const auto iterator = states_.find(std::string(id));
        return iterator == states_.end() ? nullptr : &iterator->second;
    }

    [[nodiscard]] const emberlights::EmberActionCapabilityContract* find_capability(
        std::string_view id) const noexcept override {
        const auto iterator = capabilities_.find(std::string(id));
        return iterator == capabilities_.end() ? nullptr : &iterator->second;
    }

    [[nodiscard]] const emberlights::EmberActionDependencyContract* find_action(
        std::string_view,
        std::string_view) const noexcept override {
        return nullptr;
    }

    [[nodiscard]] const emberlights::EmberActionValueContract* find_context_value(
        std::string_view) const noexcept override {
        return nullptr;
    }

    [[nodiscard]] bool supports_curve(std::string_view id) const noexcept override {
        return id == "linear";
    }

    [[nodiscard]] bool supports_unit_conversion(
        std::string_view source,
        std::string_view target) const noexcept override {
        return (source == "percent" && target == "normalized") || source == target;
    }

private:
    std::map<std::string, emberlights::EmberActionCommandContract> commands_;
    std::map<std::string, emberlights::EmberActionStateContract> states_;
    std::map<std::string, emberlights::EmberActionCapabilityContract> capabilities_;
};

void test_bounded_json_reader() {
    const auto duplicate = emberlights::parse_ember_action_json("{\"a\":1,\"a\":2}");
    CHECK(!duplicate.ok());
    CHECK(has_code(duplicate.diagnostics, "EA_JSON_DUPLICATE_KEY"));

    emberlights::EmberActionJsonReadLimits depth_limit;
    depth_limit.maximum_nesting_depth = 3U;
    const auto deep = emberlights::parse_ember_action_json("{\"a\":[[0]]}", depth_limit);
    CHECK(!deep.ok());
    CHECK(has_code(deep.diagnostics, "EA_JSON_DEPTH"));

    emberlights::EmberActionJsonReadLimits byte_limit;
    byte_limit.maximum_source_bytes = 4U;
    const auto oversized = emberlights::parse_ember_action_json("{\"a\":1}", byte_limit);
    CHECK(!oversized.ok());
    CHECK(has_code(oversized.diagnostics, "EA_JSON_SOURCE_BYTES"));

    const auto nonfinite = emberlights::parse_ember_action_json("1e9999");
    CHECK(!nonfinite.ok());
    CHECK(has_code(nonfinite.diagnostics, "EA_JSON_NONFINITE"));

    const auto imprecise = emberlights::parse_ember_action_json("9007199254740993");
    CHECK(!imprecise.ok());
    CHECK(has_code(imprecise.diagnostics, "EA_JSON_NUMBER_PRECISION"));
}

void test_canonical_equivalence_and_registry_injection(const MockRegistry& registry) {
    const auto visual = emberlights::prepare_ember_action_source(
        read_fixture("positive/canonical-visual.json"), registry);
    const auto expert = emberlights::prepare_ember_action_source(
        read_fixture("positive/canonical-text.json"), registry);
    CHECK(visual.ok());
    CHECK(expert.ok());
    if (!visual.ok()) {
        for (const auto& diagnostic : visual.diagnostics) {
            std::cerr << diagnostic.code << ' ' << diagnostic.path << ' '
                      << diagnostic.message << '\n';
        }
    }
    if (!expert.ok()) {
        for (const auto& diagnostic : expert.diagnostics) {
            std::cerr << diagnostic.code << ' ' << diagnostic.path << ' '
                      << diagnostic.message << '\n';
        }
    }
    if (visual.ok() && expert.ok()) {
        CHECK(visual.prepared->normalized_json == expert.prepared->normalized_json);
        CHECK(visual.prepared->content_hash == expert.prepared->content_hash);
        CHECK(visual.prepared->content_hash ==
            "sha256:d4a1c640aef1fba1ca061ed0e3abe4c2f106d46adc272f5eb09bff0001e3b1c5");
        CHECK(visual.prepared->dependencies.registry_digest ==
            "sha256:mock-registry-generation-1");
        CHECK(visual.prepared->dependencies.commands ==
            std::vector<std::string>{"com.test.command.set"});
        CHECK(visual.prepared->dependencies.states ==
            std::vector<std::string>{"com.test.state.ready"});
        CHECK(visual.prepared->dependencies.capabilities ==
            std::vector<std::string>{"com.test.capability.basic"});
        CHECK(visual.prepared->node_count == 4U);
        CHECK(visual.prepared->maximum_command_invocations == 1U);
    }
}

void test_focused_semantic_failures(const MockRegistry& registry) {
    const auto missing = emberlights::prepare_ember_action_source(
        read_fixture("negative/missing-node-reference.json"), registry);
    CHECK(!missing.ok());
    CHECK(has_code(missing.diagnostics, "EA_GRAPH_MISSING_REFERENCE"));

    const auto cycle = emberlights::prepare_ember_action_source(
        read_fixture("negative/graph-cycle.json"), registry);
    CHECK(!cycle.ok());
    CHECK(has_code(cycle.diagnostics, "EA_GRAPH_CYCLE"));

    const auto hash = emberlights::prepare_ember_action_source(
        read_fixture("negative/hash-mismatch.json"), registry);
    CHECK(!hash.ok());
    CHECK(has_code(hash.diagnostics, "EA_HASH_MISMATCH"));

    auto missing_registry_source = read_fixture("positive/canonical-visual.json");
    const auto command = missing_registry_source.find("com.test.command.set");
    CHECK(command != std::string::npos);
    if (command != std::string::npos) {
        missing_registry_source.replace(command, std::string("com.test.command.set").size(),
            "com.test.command.gone");
    }
    const auto missing_registry = emberlights::prepare_ember_action_source(
        missing_registry_source, registry);
    CHECK(!missing_registry.ok());
    CHECK(has_code(missing_registry.diagnostics, "EA_REGISTRY_COMMAND_MISSING"));

    auto declared_limit_source = read_fixture("positive/canonical-visual.json");
    const auto insertion = declared_limit_source.find("\"schemaVersion\": 1,");
    CHECK(insertion != std::string::npos);
    if (insertion != std::string::npos) {
        declared_limit_source.insert(
            insertion + std::string("\"schemaVersion\": 1,").size(),
            "\n  \"limits\": {\"nodes\": 3},");
    }
    const auto declared_limit = emberlights::prepare_ember_action_source(
        declared_limit_source, registry);
    CHECK(!declared_limit.ok());
    CHECK(has_code(declared_limit.diagnostics, "EA_LIMIT_NODES"));
}

void test_command_budget_and_argument_regressions(const MockRegistry& registry) {
    auto repeated = read_fixture("positive/canonical-visual.json");
    CHECK(replace_first(
        repeated,
        "\"schemaVersion\": 1,",
        "\"schemaVersion\": 1,\n  \"limits\": {\"commandInvocations\": 1},"));
    CHECK(replace_first(
        repeated,
        "[\"node.read\", \"node.invoke\", \"node.return\"]",
        "[\"node.read\", \"node.invoke\", \"node.invoke\", \"node.return\"]"));
    const auto repeated_result = emberlights::prepare_ember_action_source(repeated, registry);
    CHECK(!repeated_result.ok());
    CHECK(has_code(repeated_result.diagnostics, "EA_LIMIT_COMMAND_INVOCATIONS"));

    auto out_of_range = read_fixture("positive/canonical-visual.json");
    CHECK(replace_first(
        out_of_range,
        "\"value\": {\"source\": \"parameter\", \"path\": \"level\"}",
        "\"value\": {\"literal\": 2}"));
    const auto range_result = emberlights::prepare_ember_action_source(out_of_range, registry);
    CHECK(!range_result.ok());
    CHECK(has_code(range_result.diagnostics, "EA_COMMAND_ARGUMENT_RANGE"));

    auto unknown_argument = read_fixture("positive/canonical-visual.json");
    CHECK(replace_first(
        unknown_argument,
        "\"value\": {\"source\": \"parameter\", \"path\": \"level\"}",
        "\"value\": {\"source\": \"parameter\", \"path\": \"level\"}, "
        "\"unexpected\": {\"literal\": 0}"));
    const auto unknown_result = emberlights::prepare_ember_action_source(unknown_argument, registry);
    CHECK(!unknown_result.ok());
    CHECK(has_code(unknown_result.diagnostics, "EA_COMMAND_ARGUMENT_UNKNOWN"));
}

void test_generated_registry_adapter_and_ir_foundation() {
    const emberlights::GeneratedUiRegistryEmberActionView registry;
    CHECK(registry.registry_digest() == emberlights::kUiRegistryDigest);
    CHECK(registry.find_command("com.test.command.missing") == nullptr);
    CHECK(registry.find_state("com.test.state.missing") == nullptr);
    CHECK(registry.find_capability("content.staticLooks") == nullptr);

    for (const auto& definition : emberlights::kUiCommandDefinitions) {
        const auto* command = registry.find_command(definition.id);
        CHECK(command != nullptr);
        const auto native = registry.native_command_id(definition.id);
        CHECK(native.has_value());
        if (native.has_value()) CHECK(*native == definition.command);
    }
    for (std::size_t index = 0U; index < emberlights::kLiveCoreUiStates.size(); ++index) {
        const auto& definition = emberlights::kLiveCoreUiStates[index];
        const auto* state = registry.find_state(definition.id);
        CHECK(state != nullptr);
        const auto ordinal = registry.native_state_ordinal(definition.id);
        CHECK(ordinal.has_value());
        if (ordinal.has_value()) CHECK(*ordinal == index);
    }

    const auto* manual_bpm = registry.find_command("transport.manualBpm.set");
    CHECK(manual_bpm != nullptr);
    if (manual_bpm != nullptr) {
        CHECK(manual_bpm->realtime_class ==
            emberlights::EmberActionRealtimeClass::RunnerCommand);
        CHECK(manual_bpm->arguments.size() == 1U);
        if (manual_bpm->arguments.size() == 1U) {
            const auto& argument = manual_bpm->arguments.front();
            CHECK(argument.name == "bpm");
            CHECK(argument.value.kind == emberlights::EmberActionValueKind::Number);
            CHECK(argument.value.unit == "bpm");
            CHECK(argument.value.minimum == 20.0);
            CHECK(argument.value.maximum == 300.0);
        }
    }
    const auto* fixture_override =
        registry.find_command("fixture.override.property.set");
    CHECK(fixture_override != nullptr);
    if (fixture_override != nullptr) {
        const auto property = std::find_if(
            fixture_override->arguments.begin(),
            fixture_override->arguments.end(),
            [](const auto& argument) { return argument.name == "property"; });
        CHECK(property != fixture_override->arguments.end());
        if (property != fixture_override->arguments.end()) {
            CHECK(property->value.kind ==
                emberlights::EmberActionValueKind::Enum);
            CHECK(property->value.schema_ref == "value.fixtureProperty");
            CHECK(property->value.enum_values.size() == 53U);
            if (property->value.enum_values.size() == 53U) {
                CHECK(property->value.enum_values.front() == "intensity");
                CHECK(property->value.enum_values[4U] == "white");
                CHECK(property->value.enum_values[5U] == "amber");
                CHECK(property->value.enum_values.back() == "custom16");
            }
        }
    }
    const auto* transport_bpm = registry.find_state("transport.bpm");
    CHECK(transport_bpm != nullptr);
    if (transport_bpm != nullptr) {
        CHECK(transport_bpm->value.kind == emberlights::EmberActionValueKind::Number);
        CHECK(transport_bpm->value.unit == "bpm");
        CHECK(transport_bpm->value.minimum == 0.0);
        CHECK(transport_bpm->value.maximum == 300.0);
    }

    constexpr std::string_view source = R"action({
  "schemaVersion": 1,
  "id": "com.test.action.generated-registry",
  "version": "1.0.0",
  "label": "Generated registry BPM",
  "compatibility": {
    "minimumAppVersion": "0.1.0",
    "commandRegistry": ">=1 <2",
    "stateRegistry": ">=1 <2",
    "capabilityRegistry": ">=1 <2"
  },
  "parameters": {
    "bpm": {
      "valueType": {
        "type": "number",
        "minimum": 20,
        "maximum": 300,
        "unit": "bpm"
      },
      "required": true,
      "default": 120
    }
  },
  "requires": {
    "commands": ["transport.manualBpm.set"],
    "states": ["transport.bpm"],
    "capabilities": []
  },
  "entryPoints": {"onValue": "node.sequence"},
  "nodes": {
    "node.sequence": {
      "kind": "Sequence",
      "children": ["node.read", "node.invoke", "node.return"]
    },
    "node.read": {
      "kind": "ReadState",
      "stateId": "transport.bpm",
      "as": "currentBpm"
    },
    "node.invoke": {
      "kind": "InvokeCommand",
      "commandId": "transport.manualBpm.set",
      "arguments": {
        "bpm": {"source": "parameter", "path": "bpm"}
      },
      "resultAs": "result"
    },
    "node.return": {
      "kind": "Return",
      "result": {"source": "nodeOutput", "path": "result"}
    }
  },
  "feedback": {
    "value": {"source": "state", "path": "transport.bpm"}
  }
})action";

    const auto prepared = emberlights::prepare_ember_action_source(source, registry);
    CHECK(prepared.ok());
    if (!prepared.ok()) {
        for (const auto& diagnostic : prepared.diagnostics) {
            std::cerr << diagnostic.code << ' ' << diagnostic.path << ' '
                      << diagnostic.message << '\n';
        }
        return;
    }
    CHECK(prepared.prepared->dependencies.registry_digest == emberlights::kUiRegistryDigest);
    CHECK(prepared.prepared->dependencies.commands ==
        std::vector<std::string>{"transport.manualBpm.set"});
    CHECK(prepared.prepared->dependencies.states ==
        std::vector<std::string>{"transport.bpm"});

    const auto compiled = emberlights::compile_ember_action_ir_foundation(
        prepared.prepared, registry);
    CHECK(compiled.ok());
    if (!compiled.ok()) {
        for (const auto& diagnostic : compiled.diagnostics) {
            std::cerr << diagnostic.code << ' ' << diagnostic.path << ' '
                      << diagnostic.message << '\n';
        }
        return;
    }
    CHECK(compiled.ir->commands.size() == 1U);
    CHECK(compiled.ir->states.size() == 1U);
    if (compiled.ir->commands.size() == 1U) {
        CHECK(compiled.ir->commands.front().command == emberlights::UiCommandId::ManualBpmSet);
    }
    if (compiled.ir->states.size() == 1U) {
        CHECK(compiled.ir->states.front().native_ordinal == 6U);
    }
    CHECK(compiled.ir->cache_key.compiler_generation ==
        emberlights::kEmberActionIrCompilerGeneration);
    CHECK(compiled.ir->cache_key.source_hash == prepared.prepared->content_hash);
    CHECK(compiled.ir->cache_key.registry_digest == emberlights::kUiRegistryDigest);
    CHECK(compiled.ir->cache_key.dependency_digest.starts_with("sha256:"));
    CHECK(compiled.ir->cache_key.cache_digest ==
        "sha256:392d08421a08dcef9d628489d67e154e02c48b2385a258a78220f20c98d833fd");

    const auto repeated = emberlights::compile_ember_action_ir_foundation(
        prepared.prepared, registry);
    CHECK(repeated.ok());
    if (repeated.ok()) {
        CHECK(repeated.ir->cache_key.cache_digest == compiled.ir->cache_key.cache_digest);
        CHECK(repeated.ir->cache_key.dependency_digest ==
            compiled.ir->cache_key.dependency_digest);
    }

    auto tampered_source = std::make_shared<emberlights::EmberActionPreparedSource>(
        *prepared.prepared);
    tampered_source->content_hash = "sha256:0000000000000000000000000000000000000000000000000000000000000000";
    const auto bad_source = emberlights::compile_ember_action_ir_foundation(
        std::shared_ptr<const emberlights::EmberActionPreparedSource>(tampered_source), registry);
    CHECK(!bad_source.ok());
    CHECK(has_code(bad_source.diagnostics, "EA_IR_SOURCE_HASH_MISMATCH"));

    auto wrong_registry = std::make_shared<emberlights::EmberActionPreparedSource>(
        *prepared.prepared);
    wrong_registry->dependencies.registry_digest = "wrong-registry";
    const auto bad_registry = emberlights::compile_ember_action_ir_foundation(
        std::shared_ptr<const emberlights::EmberActionPreparedSource>(wrong_registry), registry);
    CHECK(!bad_registry.ok());
    CHECK(has_code(bad_registry.diagnostics, "EA_IR_REGISTRY_DIGEST_MISMATCH"));

    auto duplicate_dependency = std::make_shared<emberlights::EmberActionPreparedSource>(
        *prepared.prepared);
    duplicate_dependency->dependencies.commands.push_back("transport.manualBpm.set");
    const auto bad_dependencies = emberlights::compile_ember_action_ir_foundation(
        std::shared_ptr<const emberlights::EmberActionPreparedSource>(duplicate_dependency),
        registry);
    CHECK(!bad_dependencies.ok());
    CHECK(has_code(bad_dependencies.diagnostics, "EA_IR_DEPENDENCY_ORDER"));
}

}  // namespace

int main() {
    const MockRegistry registry;
    test_bounded_json_reader();
    test_canonical_equivalence_and_registry_injection(registry);
    test_focused_semantic_failures(registry);
    test_command_budget_and_argument_regressions(registry);
    test_generated_registry_adapter_and_ir_foundation();
    if (g_failures == 0) {
        std::cout << "Ember Action foundation tests passed\n";
    }
    return g_failures == 0 ? 0 : 1;
}
