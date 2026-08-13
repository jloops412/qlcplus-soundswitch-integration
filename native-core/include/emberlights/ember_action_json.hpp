#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace emberlights {

struct EmberActionJsonNumber {
    double value{0.0};
};

struct EmberActionUtf8ByteLess {
    [[nodiscard]] bool operator()(
        const std::string& left,
        const std::string& right) const noexcept;
};

struct EmberActionJsonValue {
    using Array = std::vector<EmberActionJsonValue>;
    using Object = std::map<
        std::string,
        EmberActionJsonValue,
        EmberActionUtf8ByteLess>;
    using Storage = std::variant<
        std::nullptr_t,
        bool,
        EmberActionJsonNumber,
        std::string,
        Array,
        Object>;

    Storage storage{nullptr};

    [[nodiscard]] const Object* as_object() const noexcept;
    [[nodiscard]] Object* as_object() noexcept;
    [[nodiscard]] const Array* as_array() const noexcept;
    [[nodiscard]] const std::string* as_string() const noexcept;
    [[nodiscard]] const EmberActionJsonNumber* as_number() const noexcept;
    [[nodiscard]] const bool* as_boolean() const noexcept;
    [[nodiscard]] bool is_null() const noexcept;
};

struct EmberActionDiagnostic {
    std::string code;
    std::string path;
    std::string message;
};

struct EmberActionJsonReadLimits {
    std::size_t maximum_source_bytes{64U * 1024U};
    std::size_t maximum_nesting_depth{32U};
    std::size_t maximum_values{8192U};
    std::size_t maximum_string_bytes{32U * 1024U};
    std::size_t maximum_object_properties{1024U};
    std::size_t maximum_array_elements{2048U};
};

struct EmberActionJsonParseResult {
    std::optional<EmberActionJsonValue> value;
    std::vector<EmberActionDiagnostic> diagnostics;

    [[nodiscard]] bool ok() const noexcept {
        return value.has_value() && diagnostics.empty();
    }
};

// Parses one in-memory JSON source under explicit byte/depth/value/container
// limits. It performs no filesystem, registry, Runner, device, or network work.
[[nodiscard]] EmberActionJsonParseResult parse_ember_action_json(
    std::string_view source,
    const EmberActionJsonReadLimits& limits = {});

struct EmberActionCanonicalSource {
    std::string normalized_json;
    std::string content_hash;
};

// Canonical source identity rules:
// - UTF-8 object keys sort by unsigned byte value;
// - arrays retain order;
// - equivalent finite JSON numbers share one shortest round-trip spelling;
// - CRLF/CR inside strings normalize to LF and JSON escape spelling is erased;
// - authoring-only root `source`, supplied `contentHash`, and node `metadata`
//   do not contribute to runtime source identity.
[[nodiscard]] EmberActionCanonicalSource canonicalize_ember_action_source(
    const EmberActionJsonValue& root);

[[nodiscard]] const EmberActionJsonValue* ember_action_object_find(
    const EmberActionJsonValue::Object& object,
    std::string_view key) noexcept;

}  // namespace emberlights
