#include "emberlights/ember_action_json.hpp"

#include "emberlights/file_identity.hpp"
#include "showcore/number_chars.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>

namespace emberlights {
namespace {

[[nodiscard]] bool valid_utf8(std::string_view text) noexcept {
    std::size_t index = 0U;
    while (index < text.size()) {
        const auto first = static_cast<std::uint8_t>(text[index]);
        if (first <= 0x7FU) {
            ++index;
            continue;
        }
        std::size_t count = 0U;
        std::uint32_t code_point = 0U;
        std::uint32_t minimum = 0U;
        if ((first & 0xE0U) == 0xC0U) {
            count = 2U;
            code_point = first & 0x1FU;
            minimum = 0x80U;
        } else if ((first & 0xF0U) == 0xE0U) {
            count = 3U;
            code_point = first & 0x0FU;
            minimum = 0x800U;
        } else if ((first & 0xF8U) == 0xF0U) {
            count = 4U;
            code_point = first & 0x07U;
            minimum = 0x10000U;
        } else {
            return false;
        }
        if (index + count > text.size()) {
            return false;
        }
        for (std::size_t offset = 1U; offset < count; ++offset) {
            const auto next = static_cast<std::uint8_t>(text[index + offset]);
            if ((next & 0xC0U) != 0x80U) {
                return false;
            }
            code_point = (code_point << 6U) | (next & 0x3FU);
        }
        if (code_point < minimum || code_point > 0x10FFFFU ||
            (code_point >= 0xD800U && code_point <= 0xDFFFU)) {
            return false;
        }
        index += count;
    }
    return true;
}

void append_utf8(std::uint32_t code_point, std::string& output) {
    if (code_point <= 0x7FU) {
        output.push_back(static_cast<char>(code_point));
    } else if (code_point <= 0x7FFU) {
        output.push_back(static_cast<char>(0xC0U | (code_point >> 6U)));
        output.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
    } else if (code_point <= 0xFFFFU) {
        output.push_back(static_cast<char>(0xE0U | (code_point >> 12U)));
        output.push_back(static_cast<char>(0x80U | ((code_point >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
    } else {
        output.push_back(static_cast<char>(0xF0U | (code_point >> 18U)));
        output.push_back(static_cast<char>(0x80U | ((code_point >> 12U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | ((code_point >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
    }
}

[[nodiscard]] int hex_digit(char character) noexcept {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return 10 + character - 'a';
    }
    if (character >= 'A' && character <= 'F') {
        return 10 + character - 'A';
    }
    return -1;
}

class JsonParser {
public:
    JsonParser(std::string_view source, EmberActionJsonReadLimits limits)
        : source_(source), limits_(limits) {}

    [[nodiscard]] EmberActionJsonParseResult parse() {
        EmberActionJsonParseResult result;
        if (source_.size() > limits_.maximum_source_bytes) {
            fail("EA_JSON_SOURCE_BYTES", "The JSON source exceeds the bounded reader byte limit.");
        } else if (source_.empty()) {
            fail("EA_JSON_EMPTY", "The JSON source is empty.");
        } else {
            skip_whitespace();
            auto root = parse_value(1U);
            skip_whitespace();
            if (root.has_value() && position_ != source_.size()) {
                fail("EA_JSON_TRAILING", "Unexpected bytes follow the root JSON value.");
            }
            if (root.has_value() && diagnostics_.empty()) {
                result.value = std::move(*root);
            }
        }
        result.diagnostics = std::move(diagnostics_);
        return result;
    }

private:
    [[nodiscard]] std::optional<EmberActionJsonValue> parse_value(std::size_t depth) {
        if (!diagnostics_.empty()) {
            return std::nullopt;
        }
        if (depth > limits_.maximum_nesting_depth) {
            fail("EA_JSON_DEPTH", "The JSON nesting depth exceeds the bounded reader limit.");
            return std::nullopt;
        }
        if (++value_count_ > limits_.maximum_values) {
            fail("EA_JSON_VALUES", "The JSON value count exceeds the bounded reader limit.");
            return std::nullopt;
        }
        skip_whitespace();
        if (position_ >= source_.size()) {
            fail("EA_JSON_UNEXPECTED_END", "The JSON value ended unexpectedly.");
            return std::nullopt;
        }
        const auto character = source_[position_];
        if (character == '{') {
            return parse_object(depth);
        }
        if (character == '[') {
            return parse_array(depth);
        }
        if (character == '"') {
            auto text = parse_string();
            if (!text.has_value()) {
                return std::nullopt;
            }
            return EmberActionJsonValue{std::move(*text)};
        }
        if (character == 't' && consume_token("true")) {
            return EmberActionJsonValue{true};
        }
        if (character == 'f' && consume_token("false")) {
            return EmberActionJsonValue{false};
        }
        if (character == 'n' && consume_token("null")) {
            return EmberActionJsonValue{nullptr};
        }
        if (character == '-' || (character >= '0' && character <= '9')) {
            return parse_number();
        }
        fail("EA_JSON_TOKEN", "The JSON value starts with an unsupported token.");
        return std::nullopt;
    }

    [[nodiscard]] std::optional<EmberActionJsonValue> parse_object(std::size_t depth) {
        ++position_;
        EmberActionJsonValue::Object object;
        skip_whitespace();
        if (consume('}')) {
            return EmberActionJsonValue{std::move(object)};
        }
        while (diagnostics_.empty()) {
            if (position_ >= source_.size() || source_[position_] != '"') {
                fail("EA_JSON_OBJECT_KEY", "An object member key must be a JSON string.");
                return std::nullopt;
            }
            auto key = parse_string();
            if (!key.has_value()) {
                return std::nullopt;
            }
            if (object.size() >= limits_.maximum_object_properties) {
                fail("EA_JSON_OBJECT_PROPERTIES", "An object exceeds the bounded property limit.");
                return std::nullopt;
            }
            skip_whitespace();
            if (!consume(':')) {
                fail("EA_JSON_OBJECT_COLON", "An object member is missing its colon.");
                return std::nullopt;
            }
            auto value = parse_value(depth + 1U);
            if (!value.has_value()) {
                return std::nullopt;
            }
            const auto inserted = object.emplace(std::move(*key), std::move(*value));
            if (!inserted.second) {
                fail("EA_JSON_DUPLICATE_KEY", "Duplicate JSON object keys are rejected.");
                return std::nullopt;
            }
            skip_whitespace();
            if (consume('}')) {
                return EmberActionJsonValue{std::move(object)};
            }
            if (!consume(',')) {
                fail("EA_JSON_OBJECT_SEPARATOR", "An object member is missing its comma.");
                return std::nullopt;
            }
            skip_whitespace();
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<EmberActionJsonValue> parse_array(std::size_t depth) {
        ++position_;
        EmberActionJsonValue::Array array;
        skip_whitespace();
        if (consume(']')) {
            return EmberActionJsonValue{std::move(array)};
        }
        while (diagnostics_.empty()) {
            if (array.size() >= limits_.maximum_array_elements) {
                fail("EA_JSON_ARRAY_ELEMENTS", "An array exceeds the bounded element limit.");
                return std::nullopt;
            }
            auto value = parse_value(depth + 1U);
            if (!value.has_value()) {
                return std::nullopt;
            }
            array.push_back(std::move(*value));
            skip_whitespace();
            if (consume(']')) {
                return EmberActionJsonValue{std::move(array)};
            }
            if (!consume(',')) {
                fail("EA_JSON_ARRAY_SEPARATOR", "An array element is missing its comma.");
                return std::nullopt;
            }
            skip_whitespace();
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::string> parse_string() {
        if (!consume('"')) {
            return std::nullopt;
        }
        std::string output;
        while (position_ < source_.size()) {
            const auto character = static_cast<unsigned char>(source_[position_++]);
            if (character == '"') {
                if (output.size() > limits_.maximum_string_bytes) {
                    fail("EA_JSON_STRING_BYTES", "A decoded string exceeds the bounded byte limit.");
                    return std::nullopt;
                }
                if (!valid_utf8(output)) {
                    fail("EA_JSON_UTF8", "A JSON string contains invalid UTF-8.");
                    return std::nullopt;
                }
                return output;
            }
            if (character < 0x20U) {
                fail("EA_JSON_STRING_CONTROL", "A JSON string contains an unescaped control character.");
                return std::nullopt;
            }
            if (character != '\\') {
                output.push_back(static_cast<char>(character));
                if (output.size() > limits_.maximum_string_bytes) {
                    fail("EA_JSON_STRING_BYTES", "A decoded string exceeds the bounded byte limit.");
                    return std::nullopt;
                }
                continue;
            }
            if (position_ >= source_.size()) {
                fail("EA_JSON_ESCAPE", "A JSON string ends inside an escape sequence.");
                return std::nullopt;
            }
            const auto escaped = source_[position_++];
            switch (escaped) {
                case '"': output.push_back('"'); break;
                case '\\': output.push_back('\\'); break;
                case '/': output.push_back('/'); break;
                case 'b': output.push_back('\b'); break;
                case 'f': output.push_back('\f'); break;
                case 'n': output.push_back('\n'); break;
                case 'r': output.push_back('\r'); break;
                case 't': output.push_back('\t'); break;
                case 'u': {
                    auto code_point = parse_hex_quad();
                    if (!code_point.has_value()) {
                        return std::nullopt;
                    }
                    if (*code_point >= 0xD800U && *code_point <= 0xDBFFU) {
                        if (position_ + 2U > source_.size() || source_[position_] != '\\' ||
                            source_[position_ + 1U] != 'u') {
                            fail("EA_JSON_SURROGATE", "A high surrogate is missing its low surrogate.");
                            return std::nullopt;
                        }
                        position_ += 2U;
                        auto low = parse_hex_quad();
                        if (!low.has_value() || *low < 0xDC00U || *low > 0xDFFFU) {
                            fail("EA_JSON_SURROGATE", "A high surrogate has an invalid low surrogate.");
                            return std::nullopt;
                        }
                        code_point = 0x10000U + ((*code_point - 0xD800U) << 10U) +
                            (*low - 0xDC00U);
                    } else if (*code_point >= 0xDC00U && *code_point <= 0xDFFFU) {
                        fail("EA_JSON_SURROGATE", "An isolated low surrogate is invalid.");
                        return std::nullopt;
                    }
                    append_utf8(*code_point, output);
                    break;
                }
                default:
                    fail("EA_JSON_ESCAPE", "A JSON string contains an invalid escape sequence.");
                    return std::nullopt;
            }
            if (output.size() > limits_.maximum_string_bytes) {
                fail("EA_JSON_STRING_BYTES", "A decoded string exceeds the bounded byte limit.");
                return std::nullopt;
            }
        }
        fail("EA_JSON_UNTERMINATED_STRING", "A JSON string is not terminated.");
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::uint32_t> parse_hex_quad() {
        if (position_ + 4U > source_.size()) {
            fail("EA_JSON_UNICODE_ESCAPE", "A Unicode escape is incomplete.");
            return std::nullopt;
        }
        std::uint32_t value = 0U;
        for (std::size_t index = 0U; index < 4U; ++index) {
            const auto digit = hex_digit(source_[position_++]);
            if (digit < 0) {
                fail("EA_JSON_UNICODE_ESCAPE", "A Unicode escape contains a non-hex digit.");
                return std::nullopt;
            }
            value = (value << 4U) | static_cast<std::uint32_t>(digit);
        }
        return value;
    }

    [[nodiscard]] std::optional<EmberActionJsonValue> parse_number() {
        const auto start = position_;
        if (source_[position_] == '-') {
            ++position_;
        }
        if (position_ >= source_.size()) {
            fail("EA_JSON_NUMBER", "A JSON number is incomplete.");
            return std::nullopt;
        }
        if (source_[position_] == '0') {
            ++position_;
            if (position_ < source_.size() && source_[position_] >= '0' && source_[position_] <= '9') {
                fail("EA_JSON_NUMBER", "A JSON number contains a leading zero.");
                return std::nullopt;
            }
        } else if (source_[position_] >= '1' && source_[position_] <= '9') {
            while (position_ < source_.size() && source_[position_] >= '0' && source_[position_] <= '9') {
                ++position_;
            }
        } else {
            fail("EA_JSON_NUMBER", "A JSON number has an invalid integer part.");
            return std::nullopt;
        }
        if (position_ < source_.size() && source_[position_] == '.') {
            ++position_;
            const auto fraction_start = position_;
            while (position_ < source_.size() && source_[position_] >= '0' && source_[position_] <= '9') {
                ++position_;
            }
            if (position_ == fraction_start) {
                fail("EA_JSON_NUMBER", "A JSON fraction requires at least one digit.");
                return std::nullopt;
            }
        }
        if (position_ < source_.size() && (source_[position_] == 'e' || source_[position_] == 'E')) {
            ++position_;
            if (position_ < source_.size() && (source_[position_] == '+' || source_[position_] == '-')) {
                ++position_;
            }
            const auto exponent_start = position_;
            while (position_ < source_.size() && source_[position_] >= '0' && source_[position_] <= '9') {
                ++position_;
            }
            if (position_ == exponent_start) {
                fail("EA_JSON_NUMBER", "A JSON exponent requires at least one digit.");
                return std::nullopt;
            }
        }
        double value = 0.0;
        const auto parsed = showcore::parse_number_chars(
            source_.data() + start,
            source_.data() + position_,
            value);
        if (parsed.ec != std::errc{} || parsed.ptr != source_.data() + position_ ||
            !std::isfinite(value)) {
            fail("EA_JSON_NONFINITE", "A JSON number must be finite and representable.");
            return std::nullopt;
        }
        constexpr double kMaximumExactInteger = 9007199254740991.0;
        if (std::floor(value) == value && std::abs(value) > kMaximumExactInteger) {
            fail("EA_JSON_NUMBER_PRECISION",
                "An integer exceeds the exact canonical JSON range.");
            return std::nullopt;
        }
        return EmberActionJsonValue{EmberActionJsonNumber{value}};
    }

    void skip_whitespace() noexcept {
        while (position_ < source_.size()) {
            const auto character = source_[position_];
            if (character != ' ' && character != '\t' && character != '\r' && character != '\n') {
                return;
            }
            ++position_;
        }
    }

    [[nodiscard]] bool consume(char character) noexcept {
        if (position_ < source_.size() && source_[position_] == character) {
            ++position_;
            return true;
        }
        return false;
    }

    [[nodiscard]] bool consume_token(std::string_view token) noexcept {
        if (source_.substr(position_, token.size()) == token) {
            position_ += token.size();
            return true;
        }
        return false;
    }

    void fail(std::string code, std::string message) {
        if (diagnostics_.empty()) {
            diagnostics_.push_back({
                std::move(code),
                "@" + std::to_string(position_),
                std::move(message)});
        }
    }

    std::string_view source_;
    EmberActionJsonReadLimits limits_;
    std::size_t position_{0U};
    std::size_t value_count_{0U};
    std::vector<EmberActionDiagnostic> diagnostics_;
};

enum class CanonicalContext {
    Generic,
    Root,
    Nodes,
    Node
};

void append_json_string(std::string_view text, std::string& output) {
    constexpr std::string_view hex = "0123456789abcdef";
    output.push_back('"');
    for (std::size_t index = 0U; index < text.size(); ++index) {
        auto character = static_cast<unsigned char>(text[index]);
        if (character == '\r') {
            if (index + 1U < text.size() && text[index + 1U] == '\n') {
                ++index;
            }
            character = '\n';
        }
        switch (character) {
            case '"': output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\b': output += "\\b"; break;
            case '\f': output += "\\f"; break;
            case '\n': output += "\\n"; break;
            case '\t': output += "\\t"; break;
            default:
                if (character < 0x20U) {
                    output += "\\u00";
                    output.push_back(hex[character >> 4U]);
                    output.push_back(hex[character & 0x0FU]);
                } else {
                    output.push_back(static_cast<char>(character));
                }
                break;
        }
    }
    output.push_back('"');
}

void append_canonical(
    const EmberActionJsonValue& value,
    std::string& output,
    CanonicalContext context) {
    if (value.is_null()) {
        output += "null";
        return;
    }
    if (const auto* boolean = value.as_boolean()) {
        output += *boolean ? "true" : "false";
        return;
    }
    if (const auto* number = value.as_number()) {
        if (number->value == 0.0) {
            output.push_back('0');
            return;
        }
        char buffer[64]{};
        const auto converted = std::to_chars(
            buffer,
            buffer + sizeof(buffer),
            number->value,
            std::chars_format::general);
        if (converted.ec == std::errc{}) {
            output.append(buffer, converted.ptr);
        }
        return;
    }
    if (const auto* text = value.as_string()) {
        append_json_string(*text, output);
        return;
    }
    if (const auto* array = value.as_array()) {
        output.push_back('[');
        for (std::size_t index = 0U; index < array->size(); ++index) {
            if (index != 0U) {
                output.push_back(',');
            }
            append_canonical((*array)[index], output, CanonicalContext::Generic);
        }
        output.push_back(']');
        return;
    }
    const auto* object = value.as_object();
    output.push_back('{');
    bool first = true;
    for (const auto& [key, child] : *object) {
        if ((context == CanonicalContext::Root && (key == "contentHash" || key == "source")) ||
            (context == CanonicalContext::Node && key == "metadata")) {
            continue;
        }
        if (!first) {
            output.push_back(',');
        }
        first = false;
        append_json_string(key, output);
        output.push_back(':');
        auto child_context = CanonicalContext::Generic;
        if (context == CanonicalContext::Root && key == "nodes") {
            child_context = CanonicalContext::Nodes;
        } else if (context == CanonicalContext::Nodes) {
            child_context = CanonicalContext::Node;
        }
        append_canonical(child, output, child_context);
    }
    output.push_back('}');
}

}  // namespace

bool EmberActionUtf8ByteLess::operator()(
    const std::string& left,
    const std::string& right) const noexcept {
    return std::lexicographical_compare(
        left.begin(), left.end(), right.begin(), right.end(),
        [](char first, char second) {
            return static_cast<unsigned char>(first) < static_cast<unsigned char>(second);
        });
}

const EmberActionJsonValue::Object* EmberActionJsonValue::as_object() const noexcept {
    return std::get_if<Object>(&storage);
}

EmberActionJsonValue::Object* EmberActionJsonValue::as_object() noexcept {
    return std::get_if<Object>(&storage);
}

const EmberActionJsonValue::Array* EmberActionJsonValue::as_array() const noexcept {
    return std::get_if<Array>(&storage);
}

const std::string* EmberActionJsonValue::as_string() const noexcept {
    return std::get_if<std::string>(&storage);
}

const EmberActionJsonNumber* EmberActionJsonValue::as_number() const noexcept {
    return std::get_if<EmberActionJsonNumber>(&storage);
}

const bool* EmberActionJsonValue::as_boolean() const noexcept {
    return std::get_if<bool>(&storage);
}

bool EmberActionJsonValue::is_null() const noexcept {
    return std::holds_alternative<std::nullptr_t>(storage);
}

EmberActionJsonParseResult parse_ember_action_json(
    std::string_view source,
    const EmberActionJsonReadLimits& limits) {
    return JsonParser(source, limits).parse();
}

EmberActionCanonicalSource canonicalize_ember_action_source(
    const EmberActionJsonValue& root) {
    EmberActionCanonicalSource result;
    append_canonical(root, result.normalized_json, CanonicalContext::Root);
    result.content_hash = "sha256:" + sha256_text(result.normalized_json);
    return result;
}

const EmberActionJsonValue* ember_action_object_find(
    const EmberActionJsonValue::Object& object,
    std::string_view key) noexcept {
    const auto iterator = object.find(std::string(key));
    return iterator == object.end() ? nullptr : &iterator->second;
}

}  // namespace emberlights
