#include "emberlights/ember_action_compiler.hpp"
#include "emberlights/ember_action_json.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
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

}  // namespace

int main() {
    const MockRegistry registry;
    test_bounded_json_reader();
    test_canonical_equivalence_and_registry_injection(registry);
    test_focused_semantic_failures(registry);
    test_command_budget_and_argument_regressions(registry);
    if (g_failures == 0) {
        std::cout << "Ember Action foundation tests passed\n";
    }
    return g_failures == 0 ? 0 : 1;
}
