#include "emberlights/qlc_fixture_import.hpp"

#include "emberlights/file_identity.hpp"

#include "showcore/fixture.hpp"
#include "showcore/fixture_library.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace emberlights {
namespace {

inline constexpr std::size_t kMaximumQxfBytes = 4U * 1024U * 1024U;
inline constexpr std::size_t kMaximumXmlNodes = 50000U;
inline constexpr std::size_t kMaximumXmlDepth = 64U;
inline constexpr std::size_t kMaximumImportIssues = 512U;

struct XmlAttribute {
    std::string name;
    std::string value;
};

struct XmlNode {
    std::string name;
    std::string text;
    std::vector<XmlAttribute> attributes;
    std::vector<XmlNode> children;
};

enum class XmlTokenKind : std::uint8_t {
    Start,
    End,
    Text,
    Eof
};

struct XmlToken {
    XmlTokenKind kind{XmlTokenKind::Eof};
    std::string name;
    std::string text;
    std::vector<XmlAttribute> attributes;
    bool empty{false};
    std::size_t line{1U};
};

[[nodiscard]] bool is_name_character(char character) noexcept {
    const auto value = static_cast<unsigned char>(character);
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
        (value >= '0' && value <= '9') || character == '_' || character == '-' ||
        character == ':' || character == '.';
}

[[nodiscard]] bool append_utf8(std::uint32_t codepoint, std::string& output) {
    if (codepoint == 0U || codepoint > 0x10FFFFU ||
        (codepoint >= 0xD800U && codepoint <= 0xDFFFU)) {
        return false;
    }
    if (codepoint <= 0x7FU) {
        output.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FFU) {
        output.push_back(static_cast<char>(0xC0U | (codepoint >> 6U)));
        output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    } else if (codepoint <= 0xFFFFU) {
        output.push_back(static_cast<char>(0xE0U | (codepoint >> 12U)));
        output.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    } else {
        output.push_back(static_cast<char>(0xF0U | (codepoint >> 18U)));
        output.push_back(static_cast<char>(0x80U | ((codepoint >> 12U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    }
    return true;
}

[[nodiscard]] bool decode_xml_text(
    std::string_view input,
    std::string& output,
    std::string& error) {
    output.clear();
    output.reserve(input.size());
    for (std::size_t index = 0; index < input.size(); ++index) {
        if (input[index] != '&') {
            output.push_back(input[index]);
            continue;
        }
        const auto semicolon = input.find(';', index + 1U);
        if (semicolon == std::string_view::npos || semicolon - index > 16U) {
            error = "Malformed XML entity.";
            return false;
        }
        const auto entity = input.substr(index + 1U, semicolon - index - 1U);
        if (entity == "amp") {
            output.push_back('&');
        } else if (entity == "lt") {
            output.push_back('<');
        } else if (entity == "gt") {
            output.push_back('>');
        } else if (entity == "quot") {
            output.push_back('"');
        } else if (entity == "apos") {
            output.push_back('\'');
        } else if (!entity.empty() && entity.front() == '#') {
            const bool hexadecimal = entity.size() > 2U &&
                (entity[1] == 'x' || entity[1] == 'X');
            const auto digits = entity.substr(hexadecimal ? 2U : 1U);
            std::uint32_t codepoint = 0U;
            const auto parsed = std::from_chars(
                digits.data(), digits.data() + digits.size(), codepoint,
                hexadecimal ? 16 : 10);
            if (digits.empty() || parsed.ec != std::errc{} ||
                parsed.ptr != digits.data() + digits.size() ||
                !append_utf8(codepoint, output)) {
                error = "Invalid numeric XML entity.";
                return false;
            }
        } else {
            error = "Unsupported XML entity. External entities are never resolved.";
            return false;
        }
        index = semicolon;
    }
    return true;
}

class XmlReader {
public:
    explicit XmlReader(std::string_view input) noexcept : input_(input) {}

    [[nodiscard]] bool next(XmlToken& token, std::string& error) {
        token = {};
        while (position_ < input_.size()) {
            token.line = line_;
            if (input_[position_] != '<') {
                const auto end = input_.find('<', position_);
                const auto limit = end == std::string_view::npos ? input_.size() : end;
                const auto source = input_.substr(position_, limit - position_);
                advance(limit - position_);
                token.kind = XmlTokenKind::Text;
                return decode_xml_text(source, token.text, error);
            }
            if (input_.substr(position_).starts_with("<!--")) {
                const auto end = input_.find("-->", position_ + 4U);
                if (end == std::string_view::npos) {
                    error = "Unterminated XML comment.";
                    return false;
                }
                advance(end + 3U - position_);
                continue;
            }
            if (input_.substr(position_).starts_with("<?")) {
                const auto end = input_.find("?>", position_ + 2U);
                if (end == std::string_view::npos) {
                    error = "Unterminated XML processing instruction.";
                    return false;
                }
                advance(end + 2U - position_);
                continue;
            }
            if (input_.substr(position_).starts_with("<![CDATA[")) {
                const auto start = position_ + 9U;
                const auto end = input_.find("]]>", start);
                if (end == std::string_view::npos) {
                    error = "Unterminated CDATA section.";
                    return false;
                }
                token.kind = XmlTokenKind::Text;
                token.text.assign(input_.substr(start, end - start));
                advance(end + 3U - position_);
                return true;
            }
            if (input_.substr(position_).starts_with("<!")) {
                const auto end = input_.find('>', position_ + 2U);
                if (end == std::string_view::npos) {
                    error = "Unterminated XML declaration.";
                    return false;
                }
                const auto declaration = input_.substr(position_ + 2U, end - position_ - 2U);
                if (!declaration.starts_with("DOCTYPE") || declaration.find('[') != std::string_view::npos ||
                    declaration.find("SYSTEM") != std::string_view::npos ||
                    declaration.find("PUBLIC") != std::string_view::npos) {
                    error = "Only a simple QXF DOCTYPE is allowed; external and internal entities are rejected.";
                    return false;
                }
                advance(end + 1U - position_);
                continue;
            }
            return input_.substr(position_).starts_with("</")
                ? parse_end(token, error)
                : parse_start(token, error);
        }
        token.kind = XmlTokenKind::Eof;
        token.line = line_;
        return true;
    }

private:
    void advance(std::size_t count) noexcept {
        const auto end = std::min(position_ + count, input_.size());
        for (auto index = position_; index < end; ++index) {
            if (input_[index] == '\n') {
                ++line_;
            }
        }
        position_ = end;
    }

    void skip_space() noexcept {
        while (position_ < input_.size()) {
            const auto character = input_[position_];
            if (character != ' ' && character != '\t' && character != '\r' && character != '\n') {
                break;
            }
            advance(1U);
        }
    }

    [[nodiscard]] bool parse_name(std::string& name, std::string& error) {
        const auto start = position_;
        while (position_ < input_.size() && is_name_character(input_[position_])) {
            ++position_;
        }
        if (position_ == start) {
            error = "Expected an XML name.";
            return false;
        }
        name.assign(input_.substr(start, position_ - start));
        return true;
    }

    [[nodiscard]] bool parse_end(XmlToken& token, std::string& error) {
        token.kind = XmlTokenKind::End;
        token.line = line_;
        advance(2U);
        if (!parse_name(token.name, error)) {
            return false;
        }
        skip_space();
        if (position_ >= input_.size() || input_[position_] != '>') {
            error = "Malformed XML end tag.";
            return false;
        }
        advance(1U);
        return true;
    }

    [[nodiscard]] bool parse_start(XmlToken& token, std::string& error) {
        token.kind = XmlTokenKind::Start;
        token.line = line_;
        advance(1U);
        if (!parse_name(token.name, error)) {
            return false;
        }
        while (true) {
            skip_space();
            if (position_ >= input_.size()) {
                error = "Unterminated XML start tag.";
                return false;
            }
            if (input_[position_] == '>') {
                advance(1U);
                return true;
            }
            if (input_[position_] == '/' && position_ + 1U < input_.size() &&
                input_[position_ + 1U] == '>') {
                token.empty = true;
                advance(2U);
                return true;
            }
            if (token.attributes.size() >= 64U) {
                error = "XML element has too many attributes.";
                return false;
            }
            XmlAttribute attribute;
            if (!parse_name(attribute.name, error)) {
                return false;
            }
            skip_space();
            if (position_ >= input_.size() || input_[position_] != '=') {
                error = "XML attribute is missing '='.";
                return false;
            }
            advance(1U);
            skip_space();
            if (position_ >= input_.size() ||
                (input_[position_] != '"' && input_[position_] != '\'')) {
                error = "XML attribute value must be quoted.";
                return false;
            }
            const auto quote = input_[position_];
            advance(1U);
            const auto start = position_;
            const auto end = input_.find(quote, start);
            if (end == std::string_view::npos) {
                error = "Unterminated XML attribute value.";
                return false;
            }
            const auto source = input_.substr(start, end - start);
            if (!decode_xml_text(source, attribute.value, error)) {
                return false;
            }
            advance(end + 1U - position_);
            const auto duplicate = std::find_if(
                token.attributes.begin(), token.attributes.end(),
                [&](const auto& existing) { return existing.name == attribute.name; });
            if (duplicate != token.attributes.end()) {
                error = "Duplicate XML attribute.";
                return false;
            }
            token.attributes.push_back(std::move(attribute));
        }
    }

    std::string_view input_;
    std::size_t position_{0U};
    std::size_t line_{1U};
};

[[nodiscard]] bool whitespace_only(std::string_view text) noexcept {
    return std::all_of(text.begin(), text.end(), [](char character) {
        return character == ' ' || character == '\t' || character == '\r' || character == '\n';
    });
}

[[nodiscard]] bool parse_xml_element(
    XmlReader& reader,
    XmlToken start,
    XmlNode& node,
    std::size_t depth,
    std::size_t& node_count,
    std::string& error) {
    if (depth > kMaximumXmlDepth || ++node_count > kMaximumXmlNodes) {
        error = "QXF XML exceeds the safe structure limit.";
        return false;
    }
    node.name = std::move(start.name);
    node.attributes = std::move(start.attributes);
    if (start.empty) {
        return true;
    }
    while (true) {
        XmlToken token;
        if (!reader.next(token, error)) {
            return false;
        }
        if (token.kind == XmlTokenKind::Text) {
            node.text += token.text;
        } else if (token.kind == XmlTokenKind::Start) {
            node.children.emplace_back();
            if (!parse_xml_element(
                    reader, std::move(token), node.children.back(), depth + 1U,
                    node_count, error)) {
                return false;
            }
        } else if (token.kind == XmlTokenKind::End) {
            if (token.name != node.name) {
                error = "Mismatched XML end tag at line " + std::to_string(token.line) + ".";
                return false;
            }
            return true;
        } else {
            error = "Unexpected end of QXF XML.";
            return false;
        }
    }
}

[[nodiscard]] bool parse_xml(std::string_view input, XmlNode& root, std::string& error) {
    XmlReader reader(input);
    XmlToken token;
    while (true) {
        if (!reader.next(token, error)) {
            return false;
        }
        if (token.kind == XmlTokenKind::Text && whitespace_only(token.text)) {
            continue;
        }
        if (token.kind != XmlTokenKind::Start) {
            error = "QXF does not contain a root XML element.";
            return false;
        }
        break;
    }
    std::size_t node_count = 0U;
    if (!parse_xml_element(reader, std::move(token), root, 1U, node_count, error)) {
        return false;
    }
    while (true) {
        if (!reader.next(token, error)) {
            return false;
        }
        if (token.kind == XmlTokenKind::Eof) {
            return true;
        }
        if (token.kind != XmlTokenKind::Text || !whitespace_only(token.text)) {
            error = "QXF contains data after its root element.";
            return false;
        }
    }
}

[[nodiscard]] std::string_view local_name(std::string_view name) noexcept {
    const auto separator = name.find_last_of(':');
    return separator == std::string_view::npos ? name : name.substr(separator + 1U);
}

[[nodiscard]] bool node_is(const XmlNode& node, std::string_view name) noexcept {
    return local_name(node.name) == name;
}

[[nodiscard]] std::string trim_copy(std::string_view value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return std::string(value.substr(first, last - first + 1U));
}

[[nodiscard]] const XmlNode* first_child(const XmlNode& node, std::string_view name) noexcept {
    const auto child = std::find_if(
        node.children.begin(), node.children.end(),
        [&](const auto& candidate) { return node_is(candidate, name); });
    return child == node.children.end() ? nullptr : &*child;
}

[[nodiscard]] std::string child_text(const XmlNode& node, std::string_view name) {
    const auto* child = first_child(node, name);
    return child == nullptr ? std::string{} : trim_copy(child->text);
}

[[nodiscard]] const std::string* attribute(
    const XmlNode& node,
    std::string_view name) noexcept {
    const auto found = std::find_if(
        node.attributes.begin(), node.attributes.end(),
        [&](const auto& candidate) { return local_name(candidate.name) == name; });
    return found == node.attributes.end() ? nullptr : &found->value;
}

template <typename Value>
[[nodiscard]] bool parse_integer(std::string_view text, Value& value) noexcept {
    if (text.empty()) {
        return false;
    }
    Value parsed{};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
        return false;
    }
    value = parsed;
    return true;
}

[[nodiscard]] char ascii_lower(char character) noexcept {
    return character >= 'A' && character <= 'Z'
        ? static_cast<char>(character + ('a' - 'A'))
        : character;
}

[[nodiscard]] std::string lower_copy(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), ascii_lower);
    return result;
}

[[nodiscard]] bool contains_case_insensitive(
    std::string_view value,
    std::string_view needle) {
    return lower_copy(value).find(lower_copy(needle)) != std::string::npos;
}

[[nodiscard]] bool ends_with_case_insensitive(
    std::string_view value,
    std::string_view suffix) {
    return value.size() >= suffix.size() &&
        std::equal(suffix.rbegin(), suffix.rend(), value.rbegin(),
                   [](char first, char second) { return ascii_lower(first) == ascii_lower(second); });
}

[[nodiscard]] std::string slug_component(std::string_view value, std::size_t maximum) {
    std::string result;
    bool separator = false;
    for (const auto character : value) {
        const auto lower = ascii_lower(character);
        if ((lower >= 'a' && lower <= 'z') || (lower >= '0' && lower <= '9')) {
            if (separator && !result.empty() && result.size() < maximum) {
                result.push_back('-');
            }
            separator = false;
            if (result.size() < maximum) {
                result.push_back(lower);
            }
        } else {
            separator = true;
        }
    }
    if (result.empty()) {
        result = "unknown";
    }
    return result;
}

// Compact deterministic suffixes keep generated stable IDs within the fixed
// profile text limit. They are identifiers only; source evidence uses the full
// SHA-256 digest recorded in source_revision below.
[[nodiscard]] std::uint64_t fnv1a(std::string_view value) noexcept {
    std::uint64_t hash = 1469598103934665603ULL;
    for (const auto character : value) {
        hash ^= static_cast<std::uint8_t>(character);
        hash *= 1099511628211ULL;
    }
    return hash;
}

[[nodiscard]] std::string hex64(std::uint64_t value) {
    constexpr std::array<char, 16> digits{{
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'}};
    std::string result(16U, '0');
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[result.size() - index - 1U] = digits[value & 0x0FU];
        value >>= 4U;
    }
    return result;
}

void add_issue(
    QlcFixtureImportResult& result,
    QlcImportIssueSeverity severity,
    std::string code,
    std::string subject,
    std::string message) {
    if (result.issues.size() < kMaximumImportIssues) {
        result.issues.push_back({
            severity, std::move(code), std::move(subject), std::move(message)});
    } else if (result.issues.size() == kMaximumImportIssues) {
        result.issues.push_back({
            QlcImportIssueSeverity::Warning,
            "import.issueLimit",
            "fixture",
            "Additional importer messages were suppressed."});
    }
}

struct QlcCapability {
    std::uint8_t minimum{0U};
    std::uint8_t maximum{255U};
    std::string preset;
    std::string label;
    bool has_alias{false};
    std::vector<std::string> alias_modes;
};

struct QlcChannel {
    std::string name;
    std::string preset;
    std::string group;
    std::string color;
    std::uint8_t group_byte{0U};
    std::uint8_t default_value{0U};
    bool explicit_default{false};
    std::vector<QlcCapability> capabilities;
};

struct ModeChannel {
    std::uint16_t number{0U};
    std::string channel_name;
    bool acts_on{false};
};

struct PropertyGuess {
    showcore::Property property{showcore::Property::Count};
    bool fine{false};
    bool confident{false};
    bool constant{false};
};

[[nodiscard]] std::optional<showcore::Property> preset_property(std::string preset) {
    if (ends_with_case_insensitive(preset, "Fine")) {
        preset.resize(preset.size() - 4U);
    }
    const auto lower = lower_copy(preset);
    if (lower == "intensitymasterdimmer" || lower == "intensitydimmer") {
        return showcore::Property::Intensity;
    }
    if (lower == "intensityred") return showcore::Property::Red;
    if (lower == "intensitygreen") return showcore::Property::Green;
    if (lower == "intensityblue") return showcore::Property::Blue;
    if (lower == "intensitywhite") return showcore::Property::White;
    if (lower == "intensityamber") return showcore::Property::Amber;
    if (lower == "intensityuv") return showcore::Property::UV;
    if (lower == "intensitycyan") return showcore::Property::Cyan;
    if (lower == "intensitymagenta") return showcore::Property::Magenta;
    if (lower == "intensityyellow") return showcore::Property::Yellow;
    if (lower == "intensitylime") return showcore::Property::Lime;
    if (lower == "intensityindigo") return showcore::Property::Indigo;
    if (lower == "positionpan") return showcore::Property::Pan;
    if (lower == "positiontilt") return showcore::Property::Tilt;
    if (lower.starts_with("speedpantilt") || lower.starts_with("speedpan") ||
        lower.starts_with("speedtilt")) return showcore::Property::PanTiltSpeed;
    if (lower.starts_with("colorwheel") || lower == "colormacro") {
        return showcore::Property::ColorWheel;
    }
    if (lower.starts_with("gobowheel") || lower.starts_with("goboindex")) {
        return showcore::Property::Gobo;
    }
    if (lower.starts_with("shutterstrobe")) return showcore::Property::Strobe;
    if (lower.starts_with("shutteriris")) return showcore::Property::Iris;
    if (lower.starts_with("beamfocus")) return showcore::Property::Focus;
    if (lower.starts_with("beamzoom")) return showcore::Property::Zoom;
    if (lower.starts_with("prismrotation")) return showcore::Property::PrismRotation;
    if (lower == "nofunction") return showcore::Property::Count;
    return std::nullopt;
}

[[nodiscard]] bool preset_is_reversed(std::string_view preset) {
    const auto lower = lower_copy(preset);
    return lower.find("fastslow") != std::string::npos ||
        lower.find("farnear") != std::string::npos ||
        lower.find("bigsmall") != std::string::npos ||
        lower.find("maxtomin") != std::string::npos;
}

[[nodiscard]] PropertyGuess guess_property(
    const QlcChannel& channel,
    std::string_view fixture_type) {
    const auto name = lower_copy(channel.name);
    const auto type = lower_copy(fixture_type);
    if (const auto mapped = preset_property(channel.preset); mapped.has_value()) {
        auto property = *mapped;
        if (property == showcore::Property::Intensity) {
            if (name.find("fan") != std::string::npos || type == "fan") {
                property = showcore::Property::Fan;
            } else if (type == "hazer" || name.find("haze") != std::string::npos) {
                property = showcore::Property::Haze;
            } else if (type == "smoke" || name.find("fog") != std::string::npos ||
                       name.find("smoke") != std::string::npos) {
                property = showcore::Property::Fog;
            } else if (type == "laser") {
                property = showcore::Property::Laser;
            }
        } else if (type == "laser" && property == showcore::Property::Strobe) {
            property = showcore::Property::Laser;
        }
        return {
            property,
            ends_with_case_insensitive(channel.preset, "Fine") || channel.group_byte == 1U,
            true,
            property == showcore::Property::Count};
    }

    const auto group = lower_copy(channel.group);
    const auto color = lower_copy(channel.color);
    const bool fine = channel.group_byte == 1U || ends_with_case_insensitive(name, " fine");
    if (group == "pan") return {showcore::Property::Pan, fine, true, false};
    if (group == "tilt") return {showcore::Property::Tilt, fine, true, false};
    if (group == "intensity") {
        if (color == "red") return {showcore::Property::Red, fine, true, false};
        if (color == "green") return {showcore::Property::Green, fine, true, false};
        if (color == "blue") return {showcore::Property::Blue, fine, true, false};
        if (color == "white") return {showcore::Property::White, fine, true, false};
        if (color == "amber") return {showcore::Property::Amber, fine, true, false};
        if (color == "uv") return {showcore::Property::UV, fine, true, false};
        if (color == "cyan") return {showcore::Property::Cyan, fine, true, false};
        if (color == "magenta") return {showcore::Property::Magenta, fine, true, false};
        if (color == "yellow") return {showcore::Property::Yellow, fine, true, false};
        if (color == "lime") return {showcore::Property::Lime, fine, true, false};
        if (color == "indigo") return {showcore::Property::Indigo, fine, true, false};
        // QLC+ commonly groups both the output and blower channel of a hazer
        // under Intensity. Prefer the explicit channel name before the broad
        // fixture type so "Fan Speed" cannot become an armed haze output.
        if (name.find("fan") != std::string::npos || type == "fan") {
            return {showcore::Property::Fan, fine, true, false};
        }
        if (type == "laser") {
            return {showcore::Property::Laser, fine, true, false};
        }
        if (type == "hazer" || name.find("haze") != std::string::npos) {
            return {showcore::Property::Haze, fine, true, false};
        }
        if (type == "smoke" || name.find("fog") != std::string::npos ||
            name.find("smoke") != std::string::npos) {
            return {showcore::Property::Fog, fine, true, false};
        }
        return {showcore::Property::Intensity, fine, true, false};
    }
    if (group == "colour" || group == "color") {
        if (name == "red" || name.starts_with("red ")) return {showcore::Property::Red, fine, true, false};
        if (name == "green" || name.starts_with("green ")) return {showcore::Property::Green, fine, true, false};
        if (name == "blue" || name.starts_with("blue ")) return {showcore::Property::Blue, fine, true, false};
        if (name == "white" || name.starts_with("white ")) return {showcore::Property::White, fine, true, false};
        if (name == "amber" || name.starts_with("amber ")) return {showcore::Property::Amber, fine, true, false};
        if (name == "uv" || name.find("ultraviolet") != std::string::npos) return {showcore::Property::UV, fine, true, false};
        if (name == "cyan" || name.starts_with("cyan ")) return {showcore::Property::Cyan, fine, true, false};
        if (name == "magenta" || name.starts_with("magenta ")) return {showcore::Property::Magenta, fine, true, false};
        if (name == "yellow" || name.starts_with("yellow ")) return {showcore::Property::Yellow, fine, true, false};
        if (name == "lime" || name.starts_with("lime ")) return {showcore::Property::Lime, fine, true, false};
        if (name == "indigo" || name.starts_with("indigo ")) return {showcore::Property::Indigo, fine, true, false};
        return {showcore::Property::ColorWheel, fine, false, false};
    }
    if (group == "gobo") {
        return {
            name.find("rotat") != std::string::npos
                ? showcore::Property::GoboRotation : showcore::Property::Gobo,
            fine, false, false};
    }
    if (group == "prism") {
        return {
            name.find("rotat") != std::string::npos
                ? showcore::Property::PrismRotation : showcore::Property::Prism,
            fine, false, false};
    }
    if (group == "shutter") {
        if (type == "laser") {
            return {showcore::Property::Laser, fine, true, false};
        }
        if (name.find("iris") != std::string::npos) {
            return {showcore::Property::Iris, fine, false, false};
        }
        const bool strobe = std::any_of(
            channel.capabilities.begin(), channel.capabilities.end(),
            [](const auto& capability) {
                return contains_case_insensitive(capability.preset, "strobe") ||
                    contains_case_insensitive(capability.label, "strobe");
            });
        return {strobe ? showcore::Property::Strobe : showcore::Property::Shutter,
                fine, strobe, false};
    }
    if (group == "beam") {
        if (name.find("focus") != std::string::npos) return {showcore::Property::Focus, fine, false, false};
        if (name.find("zoom") != std::string::npos) return {showcore::Property::Zoom, fine, false, false};
        if (name.find("iris") != std::string::npos) return {showcore::Property::Iris, fine, false, false};
        if (name.find("frost") != std::string::npos) return {showcore::Property::Frost, fine, false, false};
    }
    if (group == "speed") {
        return {
            name.find("pan") != std::string::npos || name.find("tilt") != std::string::npos
                ? showcore::Property::PanTiltSpeed : showcore::Property::EffectSpeed,
            fine, false, false};
    }
    if (group == "effect") return {showcore::Property::Effect, fine, false, false};
    if (type == "laser") return {showcore::Property::Laser, fine, false, false};
    return {showcore::Property::Count, fine, false, false};
}

[[nodiscard]] bool parse_qlc_channel(
    const XmlNode& node,
    QlcChannel& channel,
    QlcFixtureImportResult& result) {
    const auto* name = attribute(node, "Name");
    if (name == nullptr || trim_copy(*name).empty()) {
        add_issue(result, QlcImportIssueSeverity::Error, "channel.name", "fixture",
                  "A QLC+ channel is missing its Name attribute.");
        return false;
    }
    channel.name = trim_copy(*name);
    if (const auto* preset = attribute(node, "Preset"); preset != nullptr) {
        channel.preset = trim_copy(*preset);
    }
    if (const auto* value = attribute(node, "Default"); value != nullptr) {
        std::uint16_t parsed = 0U;
        if (!parse_integer(*value, parsed) || parsed > 255U) {
            add_issue(result, QlcImportIssueSeverity::Error, "channel.default", channel.name,
                      "The QLC+ channel default is outside DMX range 0–255.");
            return false;
        }
        channel.default_value = static_cast<std::uint8_t>(parsed);
        channel.explicit_default = true;
    }
    for (const auto& child : node.children) {
        if (node_is(child, "Group")) {
            channel.group = trim_copy(child.text);
            if (const auto* byte = attribute(child, "Byte"); byte != nullptr) {
                std::uint16_t parsed = 0U;
                if (!parse_integer(*byte, parsed) || parsed > 1U) {
                    add_issue(result, QlcImportIssueSeverity::Error, "channel.groupByte", channel.name,
                              "Only QLC+ coarse/fine group bytes 0 and 1 are supported.");
                    return false;
                }
                channel.group_byte = static_cast<std::uint8_t>(parsed);
            }
        } else if (node_is(child, "Colour")) {
            channel.color = trim_copy(child.text);
        } else if (node_is(child, "Capability")) {
            const auto* minimum = attribute(child, "Min");
            const auto* maximum = attribute(child, "Max");
            std::uint16_t parsed_minimum = 0U;
            std::uint16_t parsed_maximum = 0U;
            if (minimum == nullptr || maximum == nullptr ||
                !parse_integer(*minimum, parsed_minimum) ||
                !parse_integer(*maximum, parsed_maximum) ||
                parsed_minimum > 255U || parsed_maximum > 255U ||
                parsed_minimum > parsed_maximum) {
                add_issue(result, QlcImportIssueSeverity::Error, "capability.range", channel.name,
                          "A QLC+ capability has an invalid DMX range.");
                return false;
            }
            QlcCapability capability;
            capability.minimum = static_cast<std::uint8_t>(parsed_minimum);
            capability.maximum = static_cast<std::uint8_t>(parsed_maximum);
            capability.label = trim_copy(child.text);
            if (const auto* preset = attribute(child, "Preset"); preset != nullptr) {
                capability.preset = trim_copy(*preset);
            }
            for (const auto& nested : child.children) {
                if (!node_is(nested, "Alias")) {
                    continue;
                }
                capability.has_alias = true;
                if (const auto* mode = attribute(nested, "Mode"); mode != nullptr) {
                    capability.alias_modes.push_back(*mode);
                }
            }
            channel.capabilities.push_back(std::move(capability));
        }
    }
    return true;
}

[[nodiscard]] bool alias_affects_mode(const QlcChannel& channel, std::string_view mode) {
    for (const auto& capability : channel.capabilities) {
        if (!capability.has_alias) {
            continue;
        }
        if (capability.alias_modes.empty() ||
            std::any_of(capability.alias_modes.begin(), capability.alias_modes.end(),
                        [&](const auto& alias_mode) {
                            return alias_mode == mode || alias_mode == "*";
                        })) {
            return true;
        }
    }
    return false;
}

struct ActiveRange {
    std::uint8_t minimum{1U};
    std::uint8_t maximum{255U};
    std::uint8_t inactive{0U};
    bool reversed{false};
    bool heuristic{false};
    bool found{false};
};

[[nodiscard]] ActiveRange strobe_range(const QlcChannel& channel) {
    ActiveRange range;
    int best_priority = -1;
    std::uint16_t best_width = 0U;
    for (const auto& capability : channel.capabilities) {
        const auto preset = lower_copy(capability.preset);
        const auto label = lower_copy(capability.label);
        int priority = -1;
        if (preset == "strobeslowtofast" || preset == "strobefasttoslow" ||
            preset == "strobefreqrange") {
            priority = 4;
        } else if (preset == "strobefrequency") {
            priority = 3;
        } else if (preset.starts_with("strobe") && preset.find("random") == std::string::npos) {
            priority = 2;
        } else if (label.find("strobe") != std::string::npos &&
                   label.find("random") == std::string::npos) {
            priority = 1;
        }
        const auto width = static_cast<std::uint16_t>(
            capability.maximum - capability.minimum);
        if (priority > best_priority || (priority == best_priority && width > best_width)) {
            best_priority = priority;
            best_width = width;
            range.minimum = capability.minimum;
            range.maximum = capability.maximum;
            range.reversed = preset.find("fasttoslow") != std::string::npos;
            range.heuristic = priority == 1;
            range.found = priority >= 0;
        }
    }
    if (!channel.explicit_default) {
        const auto open = std::find_if(
            channel.capabilities.begin(), channel.capabilities.end(),
            [](const auto& capability) {
                return lower_copy(capability.preset) == "shutteropen";
            });
        if (open != channel.capabilities.end()) {
            range.inactive = open->minimum;
        }
    } else {
        range.inactive = channel.default_value;
    }
    if (!range.found && preset_property(channel.preset) == showcore::Property::Strobe) {
        range.minimum = 1U;
        range.maximum = 255U;
        range.inactive = channel.default_value;
        range.heuristic = true;
        range.found = true;
    }
    return range;
}

[[nodiscard]] ActiveRange activation_range(
    const QlcChannel& channel,
    showcore::Property property) {
    if (property == showcore::Property::Strobe) {
        return strobe_range(channel);
    }
    ActiveRange range;
    range.inactive = channel.default_value;
    if (property == showcore::Property::Fog || property == showcore::Property::Haze ||
        property == showcore::Property::Laser || property == showcore::Property::Spark) {
        range.minimum = 1U;
        range.maximum = 255U;
        if (!channel.capabilities.empty()) {
            const auto active = std::find_if(
                channel.capabilities.begin(), channel.capabilities.end(),
                [](const auto& capability) {
                    const auto label = lower_copy(capability.label);
                    return label.find("off") == std::string::npos &&
                        label.find("closed") == std::string::npos && capability.maximum > 0U;
                });
            if (active != channel.capabilities.end()) {
                range.minimum = std::max<std::uint8_t>(1U, active->minimum);
                range.maximum = active->maximum;
            }
        }
        range.found = true;
    } else if (property == showcore::Property::Prism) {
        const auto active = std::find_if(
            channel.capabilities.begin(), channel.capabilities.end(),
            [](const auto& capability) {
                return lower_copy(capability.preset) == "prismeffecton";
            });
        if (active != channel.capabilities.end()) {
            range.minimum = active->minimum;
            range.maximum = active->maximum;
            range.found = true;
        }
    }
    return range;
}

[[nodiscard]] bool is_continuous(showcore::Property property) noexcept {
    switch (property) {
    case showcore::Property::Intensity:
    case showcore::Property::Red:
    case showcore::Property::Green:
    case showcore::Property::Blue:
    case showcore::Property::White:
    case showcore::Property::Amber:
    case showcore::Property::UV:
    case showcore::Property::Cyan:
    case showcore::Property::Magenta:
    case showcore::Property::Yellow:
    case showcore::Property::Lime:
    case showcore::Property::Indigo:
    case showcore::Property::Pan:
    case showcore::Property::Tilt:
    case showcore::Property::PanRotate:
    case showcore::Property::TiltRotate:
    case showcore::Property::PanTiltSpeed:
    case showcore::Property::Focus:
    case showcore::Property::Zoom:
    case showcore::Property::Iris:
    case showcore::Property::Frost:
    case showcore::Property::AnimationRotation:
    case showcore::Property::EffectSpeed:
    case showcore::Property::Fan:
    case showcore::Property::Custom1:
    case showcore::Property::Custom2:
    case showcore::Property::Custom3:
    case showcore::Property::Custom4:
    case showcore::Property::Custom5:
    case showcore::Property::Custom6:
    case showcore::Property::Custom7:
    case showcore::Property::Custom8:
    case showcore::Property::Custom9:
    case showcore::Property::Custom10:
    case showcore::Property::Custom11:
    case showcore::Property::Custom12:
    case showcore::Property::Custom13:
    case showcore::Property::Custom14:
    case showcore::Property::Custom15:
    case showcore::Property::Custom16:
        return true;
    case showcore::Property::Strobe:
    case showcore::Property::Shutter:
    case showcore::Property::ColorWheel:
    case showcore::Property::Gobo:
    case showcore::Property::GoboRotation:
    case showcore::Property::Prism:
    case showcore::Property::PrismRotation:
    case showcore::Property::Animation:
    case showcore::Property::Effect:
    case showcore::Property::Fog:
    case showcore::Property::Haze:
    case showcore::Property::Laser:
    case showcore::Property::Spark:
    case showcore::Property::Count:
        return false;
    }
    return false;
}

[[nodiscard]] showcore::Property custom_property(std::size_t index) noexcept {
    return static_cast<showcore::Property>(
        static_cast<std::size_t>(showcore::Property::Custom1) + index);
}

[[nodiscard]] std::string stable_profile_id(
    std::string_view manufacturer,
    std::string_view model,
    std::string_view mode) {
    const auto identity = std::string(manufacturer) + "\n" + std::string(model) + "\n" +
        std::string(mode);
    return "qlcplus." + slug_component(manufacturer, 20U) + "." +
        slug_component(model, 20U) + "." + slug_component(mode, 20U) + "." +
        hex64(fnv1a(identity)).substr(0U, 10U);
}

[[nodiscard]] bool parse_mode_channels(
    const XmlNode& mode_node,
    std::vector<ModeChannel>& mode_channels,
    bool& has_heads,
    QlcFixtureImportResult& result,
    std::string_view mode_name) {
    std::unordered_set<std::uint16_t> numbers;
    for (const auto& child : mode_node.children) {
        if (node_is(child, "Head")) {
            has_heads = true;
            continue;
        }
        if (!node_is(child, "Channel")) {
            continue;
        }
        const auto* number_text = attribute(child, "Number");
        std::uint16_t number = 0U;
        if (number_text == nullptr || !parse_integer(*number_text, number) ||
            number >= showcore::kUniverseSlots || !numbers.insert(number).second) {
            add_issue(result, QlcImportIssueSeverity::Error, "mode.channelNumber",
                      std::string(mode_name),
                      "The mode contains a missing, duplicate, or out-of-range channel number.");
            return false;
        }
        ModeChannel reference;
        reference.number = number;
        reference.channel_name = trim_copy(child.text);
        reference.acts_on = attribute(child, "ActsOn") != nullptr;
        if (reference.channel_name.empty()) {
            add_issue(result, QlcImportIssueSeverity::Error, "mode.channelName",
                      std::string(mode_name), "A mode channel does not name a QLC+ channel.");
            return false;
        }
        mode_channels.push_back(std::move(reference));
    }
    std::sort(mode_channels.begin(), mode_channels.end(), [](const auto& first, const auto& second) {
        return first.number < second.number;
    });
    if (mode_channels.empty()) {
        add_issue(result, QlcImportIssueSeverity::Error, "mode.empty", std::string(mode_name),
                  "The QLC+ mode has no channels and was quarantined.");
        return false;
    }
    return true;
}

[[nodiscard]] bool build_mode_profile(
    const XmlNode& mode_node,
    const std::unordered_map<std::string, QlcChannel>& channels,
    QlcFixtureImportResult& result,
    FixtureProfileDefinition& profile) {
    const auto* mode_attribute = attribute(mode_node, "Name");
    const auto mode_name = mode_attribute == nullptr ? std::string{} : trim_copy(*mode_attribute);
    if (mode_name.empty()) {
        add_issue(result, QlcImportIssueSeverity::Error, "mode.name", "fixture",
                  "A QLC+ mode is missing its Name attribute and was quarantined.");
        return false;
    }
    if (mode_name.size() > showcore::kFixtureProfileTextLength) {
        add_issue(result, QlcImportIssueSeverity::Error, "mode.nameLength", mode_name,
                  "The QLC+ mode name exceeds EmberLights' 96-character stable-profile limit.");
        return false;
    }

    std::vector<ModeChannel> references;
    bool has_heads = false;
    if (!parse_mode_channels(mode_node, references, has_heads, result, mode_name)) {
        return false;
    }
    if (std::any_of(references.begin(), references.end(), [](const auto& channel) {
            return channel.acts_on;
        })) {
        add_issue(result, QlcImportIssueSeverity::Error, "mode.actsOn", mode_name,
                  "QLC+ ActsOn channel switching is not representable yet; this mode was quarantined.");
        return false;
    }
    for (const auto& reference : references) {
        const auto found = channels.find(reference.channel_name);
        if (found == channels.end()) {
            add_issue(result, QlcImportIssueSeverity::Error, "mode.missingChannel", mode_name,
                      "The mode references missing channel '" + reference.channel_name + "'.");
            return false;
        }
        if (alias_affects_mode(found->second, mode_name)) {
            add_issue(result, QlcImportIssueSeverity::Error, "mode.switchingAlias", mode_name,
                      "QLC+ switching-channel aliases are not representable yet; this mode was quarantined.");
            return false;
        }
    }

    profile.id = stable_profile_id(result.manufacturer, result.model, mode_name);
    profile.manufacturer = result.manufacturer;
    profile.model = result.model;
    profile.mode = mode_name;
    profile.name = result.manufacturer + " " + result.model + " (" + mode_name + ")";
    if (profile.name.size() > showcore::kFixtureProfileTextLength) {
        auto prefix_length = showcore::kFixtureProfileTextLength - 11U;
        while (prefix_length > 0U &&
               (static_cast<std::uint8_t>(profile.name[prefix_length]) & 0xC0U) == 0x80U) {
            --prefix_length;
        }
        profile.name.resize(prefix_length);
        profile.name += "~" + hex64(fnv1a(mode_name)).substr(0U, 10U);
    }
    profile.source = result.source;
    profile.source_revision = result.source_revision;
    profile.footprint = static_cast<std::uint16_t>(references.back().number + 1U);

    std::size_t custom_count = 0U;
    struct PendingFine {
        std::string source_name;
        showcore::Property property{showcore::Property::Count};
        std::uint16_t offset{0U};
        std::uint8_t default_value{0U};
    };
    std::vector<PendingFine> fine_channels;

    for (const auto& reference : references) {
        const auto& source = channels.at(reference.channel_name);
        auto guess = guess_property(source, result.fixture_type);
        if (guess.fine) {
            fine_channels.push_back({source.name, guess.property, reference.number, source.default_value});
            continue;
        }

        if (guess.constant) {
            profile.channels.push_back({
                showcore::Property::Count,
                reference.number,
                -1,
                showcore::ChannelEncoding::Constant8,
                0U,
                255U,
                source.default_value});
            continue;
        }

        if (guess.property == showcore::Property::Count) {
            if (custom_count < 16U) {
                guess.property = custom_property(custom_count++);
                add_issue(result, QlcImportIssueSeverity::Warning, "channel.custom", mode_name,
                          "Channel '" + source.name +
                              "' has no safe semantic match and was preserved as " +
                              std::string(property_name(guess.property)) + ".");
            } else {
                profile.channels.push_back({
                    showcore::Property::Count,
                    reference.number,
                    -1,
                    showcore::ChannelEncoding::Constant8,
                    0U,
                    255U,
                    source.default_value});
                add_issue(result, QlcImportIssueSeverity::Warning, "channel.customCapacity", mode_name,
                          "Channel '" + source.name +
                              "' exceeded the 16 custom semantic lanes and is held at its safe default.");
                continue;
            }
        } else if (!guess.confident) {
            add_issue(result, QlcImportIssueSeverity::Warning, "channel.approximation", mode_name,
                      "Channel '" + source.name + "' was approximated as " +
                          std::string(property_name(guess.property)) + ". Review against the fixture manual.");
        }

        auto encoding = is_continuous(guess.property)
            ? showcore::ChannelEncoding::Linear8
            : showcore::ChannelEncoding::Discrete8;
        std::uint8_t dmx_min = 0U;
        std::uint8_t dmx_max = 255U;
        if (is_continuous(guess.property) && preset_is_reversed(source.preset)) {
            dmx_min = 255U;
            dmx_max = 0U;
        }
        auto default_value = source.default_value;
        const auto active = activation_range(source, guess.property);
        if (active.found) {
            encoding = showcore::ChannelEncoding::Ranged8;
            dmx_min = active.reversed ? active.maximum : active.minimum;
            dmx_max = active.reversed ? active.minimum : active.maximum;
            default_value = active.inactive;
            if (guess.property == showcore::Property::Strobe) {
                add_issue(result, QlcImportIssueSeverity::Warning, "channel.strobeShutter", mode_name,
                          "Channel '" + source.name +
                              "' uses its regular strobe range with a safe open/inactive value; pulse, random, and other shared shutter functions remain available only through a custom profile review.");
                if (active.heuristic) {
                    add_issue(result, QlcImportIssueSeverity::Warning, "channel.strobeHeuristic", mode_name,
                              "The strobe range for '" + source.name +
                                  "' was inferred from its QLC+ label and should be verified.");
                }
            }
        }
        profile.channels.push_back({
            guess.property,
            reference.number,
            -1,
            encoding,
            dmx_min,
            dmx_max,
            default_value});
    }

    for (const auto& fine : fine_channels) {
        auto base_name = lower_copy(fine.source_name);
        if (ends_with_case_insensitive(base_name, " fine")) {
            base_name.resize(base_name.size() - 5U);
        }
        auto candidate = profile.channels.end();
        for (auto mapping = profile.channels.begin(); mapping != profile.channels.end(); ++mapping) {
            if (mapping->property != fine.property ||
                mapping->encoding != showcore::ChannelEncoding::Linear8) {
                continue;
            }
            const auto source_reference = std::find_if(
                references.begin(), references.end(), [&](const auto& reference) {
                    return reference.number == mapping->coarse_offset;
                });
            if (source_reference == references.end()) {
                continue;
            }
            auto coarse_name = lower_copy(source_reference->channel_name);
            if (coarse_name == base_name) {
                candidate = mapping;
                break;
            }
            if (candidate == profile.channels.end()) {
                candidate = mapping;
            } else {
                candidate = profile.channels.end();
                break;
            }
        }
        if (candidate == profile.channels.end()) {
            profile.channels.push_back({
                showcore::Property::Count,
                fine.offset,
                -1,
                showcore::ChannelEncoding::Constant8,
                0U,
                255U,
                fine.default_value});
            add_issue(result, QlcImportIssueSeverity::Warning, "channel.orphanFine", mode_name,
                      "Fine channel '" + fine.source_name +
                          "' could not be paired and is held at its safe default.");
            continue;
        }
        candidate->encoding = showcore::ChannelEncoding::Linear16;
        candidate->fine_offset = static_cast<std::int16_t>(fine.offset);
        candidate->default_value = static_cast<std::uint16_t>(
            (candidate->default_value << 8U) | fine.default_value);
    }

    if (has_heads) {
        add_issue(result, QlcImportIssueSeverity::Warning, "mode.headsFlattened", mode_name,
                  "QLC+ head/cell topology was flattened into fixture-level channels; verify multi-cell behavior.");
    }

    if (lower_copy(result.fixture_type) == "laser" &&
        std::none_of(profile.channels.begin(), profile.channels.end(), [](const auto& channel) {
            return channel.property == showcore::Property::Laser;
        })) {
        add_issue(result, QlcImportIssueSeverity::Error, "mode.laserGate", mode_name,
                  "The laser mode has no safely representable armed emission gate and was quarantined.");
        return false;
    }

    std::vector<showcore::ChannelMapping> runtime;
    runtime.reserve(profile.channels.size());
    for (const auto& channel : profile.channels) {
        runtime.push_back({
            channel.property,
            channel.coarse_offset,
            channel.fine_offset,
            channel.encoding,
            channel.dmx_min,
            channel.dmx_max,
            channel.default_value});
    }
    const showcore::FixtureProfile candidate{
        profile.name.c_str(), runtime.data(), runtime.size(), profile.footprint};
    const auto validation = showcore::validate_fixture_profile(candidate);
    if (!validation) {
        add_issue(result, QlcImportIssueSeverity::Error, "mode.nativeValidation", mode_name,
                  "The converted mode failed native channel/footprint validation and was quarantined.");
        return false;
    }
    return true;
}

}  // namespace

std::size_t QlcFixtureImportResult::warning_count() const noexcept {
    return static_cast<std::size_t>(std::count_if(
        issues.begin(), issues.end(), [](const auto& issue) {
            return issue.severity == QlcImportIssueSeverity::Warning;
        }));
}

std::size_t QlcFixtureImportResult::error_count() const noexcept {
    return static_cast<std::size_t>(std::count_if(
        issues.begin(), issues.end(), [](const auto& issue) {
            return issue.severity == QlcImportIssueSeverity::Error;
        }));
}

QlcFixtureImportResult import_qlc_fixture(
    std::string_view qxf,
    std::string_view source_identity) {
    QlcFixtureImportResult result;
    const auto subject = source_identity.empty() ? std::string("QXF fixture")
                                                  : std::string(source_identity);
    if (qxf.empty() || qxf.size() > kMaximumQxfBytes) {
        add_issue(result, QlcImportIssueSeverity::Error, "qxf.size", subject,
                  "The QXF file is empty or exceeds the 4 MB Studio import limit.");
        return result;
    }

    if (qxf.size() >= 3U &&
        static_cast<std::uint8_t>(qxf[0]) == 0xEFU &&
        static_cast<std::uint8_t>(qxf[1]) == 0xBBU &&
        static_cast<std::uint8_t>(qxf[2]) == 0xBFU) {
        qxf.remove_prefix(3U);
    }

    XmlNode root;
    std::string xml_error;
    if (!parse_xml(qxf, root, xml_error)) {
        add_issue(result, QlcImportIssueSeverity::Error, "qxf.xml", subject,
                  "The QXF XML is invalid: " + xml_error);
        return result;
    }
    if (!node_is(root, "FixtureDefinition")) {
        add_issue(result, QlcImportIssueSeverity::Error, "qxf.root", subject,
                  "The file is not a QLC+ FixtureDefinition document.");
        return result;
    }

    result.manufacturer = child_text(root, "Manufacturer");
    result.model = child_text(root, "Model");
    result.fixture_type = child_text(root, "Type");
    if (result.manufacturer.empty() || result.model.empty() || result.fixture_type.empty()) {
        add_issue(result, QlcImportIssueSeverity::Error, "qxf.metadata", subject,
                  "Manufacturer, Model, and Type are required QLC+ fixture fields.");
        return result;
    }
    if (result.manufacturer.size() > showcore::kFixtureProfileTextLength ||
        result.model.size() > showcore::kFixtureProfileTextLength) {
        add_issue(result, QlcImportIssueSeverity::Error, "qxf.metadataLength", subject,
                  "Manufacturer or Model exceeds EmberLights' 96-character stable-profile limit.");
        return result;
    }

    std::string creator_name = "QLC+";
    if (const auto* creator = first_child(root, "Creator"); creator != nullptr) {
        const auto parsed_name = child_text(*creator, "Name");
        if (!parsed_name.empty()) creator_name = parsed_name;
    }
    if (lower_copy(creator_name).starts_with("ofl")) {
        result.source = showcore::FixtureProfileSource::OpenFixtureLibrary;
    }
    // Source identity must survive catalog refreshes and same-ID/different-file
    // comparisons. The old 12-hex FNV fragment was collision-prone and could
    // not serve as evidence. Creator metadata still selects OFL provenance;
    // the full source digest now occupies the revision field.
    result.source_revision = std::string(kQlcFixtureAdapterVersion) +
        "#sha256:" + sha256_text(qxf);

    std::unordered_map<std::string, QlcChannel> channels;
    bool duplicate_channel_name = false;
    for (const auto& child : root.children) {
        if (!node_is(child, "Channel")) {
            continue;
        }
        QlcChannel channel;
        if (!parse_qlc_channel(child, channel, result)) {
            continue;
        }
        const auto name = channel.name;
        if (!channels.emplace(name, std::move(channel)).second) {
            duplicate_channel_name = true;
            add_issue(result, QlcImportIssueSeverity::Error, "channel.duplicate", name,
                      "Duplicate QLC+ channel names are ambiguous; the fixture was quarantined.");
        }
    }
    if (channels.empty()) {
        add_issue(result, QlcImportIssueSeverity::Error, "qxf.channels", subject,
                  "The QXF file contains no usable channel definitions.");
        return result;
    }
    if (duplicate_channel_name) {
        return result;
    }

    for (const auto& child : root.children) {
        if (!node_is(child, "Mode")) {
            continue;
        }
        FixtureProfileDefinition profile;
        if (build_mode_profile(child, channels, result, profile)) {
            result.profiles.push_back(std::move(profile));
        }
    }
    if (result.profiles.empty()) {
        add_issue(result, QlcImportIssueSeverity::Error, "qxf.noModes", subject,
                  "No QLC+ mode could be converted safely; the fixture was quarantined.");
    }
    return result;
}

QlcFixtureImportResult load_qlc_fixture(const std::filesystem::path& path) {
    QlcFixtureImportResult result;
    std::error_code filesystem_error;
    const auto size = std::filesystem::file_size(path, filesystem_error);
    if (filesystem_error || size == 0U || size > kMaximumQxfBytes) {
        add_issue(result, QlcImportIssueSeverity::Error, "qxf.open", path.filename().string(),
                  "The QXF file is unavailable, empty, or exceeds the 4 MB import limit.");
        return result;
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        add_issue(result, QlcImportIssueSeverity::Error, "qxf.open", path.filename().string(),
                  "The QXF file could not be opened.");
        return result;
    }
    std::string bytes(static_cast<std::size_t>(size), '\0');
    if (!input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()))) {
        add_issue(result, QlcImportIssueSeverity::Error, "qxf.read", path.filename().string(),
                  "The complete QXF file could not be read.");
        return result;
    }
    return import_qlc_fixture(bytes, path.filename().string());
}

}  // namespace emberlights
