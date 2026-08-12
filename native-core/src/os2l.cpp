#include "showcore/os2l.hpp"
#include "showcore/number_chars.hpp"

#include <charconv>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace showcore {
namespace {

[[nodiscard]] std::size_t skip_space(std::string_view text, std::size_t index) noexcept {
    while (index < text.size() && std::isspace(static_cast<unsigned char>(text[index])) != 0) {
        ++index;
    }
    return index;
}

[[nodiscard]] bool locate_value(
    std::string_view json,
    std::string_view key,
    std::size_t& value_index) noexcept {
    for (std::size_t index = 0; index < json.size(); ++index) {
        if (json[index] != '"') {
            continue;
        }
        const auto key_start = index + 1;
        if (key_start + key.size() >= json.size() || json.substr(key_start, key.size()) != key) {
            continue;
        }
        const auto quote = key_start + key.size();
        if (json[quote] != '"') {
            continue;
        }
        auto colon = skip_space(json, quote + 1);
        if (colon >= json.size() || json[colon] != ':') {
            continue;
        }
        value_index = skip_space(json, colon + 1);
        return value_index < json.size();
    }
    return false;
}

template <std::size_t Capacity>
[[nodiscard]] Os2lParseError parse_string_at(
    std::string_view json,
    std::size_t index,
    FixedText<Capacity>& output) noexcept {
    output.length = 0;
    if (index >= json.size() || json[index] != '"') {
        return Os2lParseError::Malformed;
    }
    ++index;
    while (index < json.size()) {
        auto character = json[index++];
        if (character == '"') {
            return Os2lParseError::None;
        }
        if (character == '\\') {
            if (index >= json.size()) {
                return Os2lParseError::Malformed;
            }
            const auto escaped = json[index++];
            switch (escaped) {
            case '"': character = '"'; break;
            case '\\': character = '\\'; break;
            case '/': character = '/'; break;
            case 'b': character = '\b'; break;
            case 'f': character = '\f'; break;
            case 'n': character = '\n'; break;
            case 'r': character = '\r'; break;
            case 't': character = '\t'; break;
            default: return Os2lParseError::Malformed;
            }
        }
        if (output.length >= output.bytes.size()) {
            return Os2lParseError::FieldTooLong;
        }
        output.bytes[output.length++] = character;
    }
    return Os2lParseError::Malformed;
}

template <typename Number>
[[nodiscard]] Os2lParseError parse_number_at(
    std::string_view json,
    std::size_t index,
    Number& output) noexcept {
    auto end = index;
    while (end < json.size() && json[end] != ',' && json[end] != '}' &&
           std::isspace(static_cast<unsigned char>(json[end])) == 0) {
        ++end;
    }
    if (end == index) {
        return Os2lParseError::InvalidNumber;
    }
    const auto result = showcore::parse_number_chars(
        json.data() + index, json.data() + end, output);
    if (result.ec != std::errc{} || result.ptr != json.data() + end) {
        return Os2lParseError::InvalidNumber;
    }
    return Os2lParseError::None;
}

[[nodiscard]] Os2lParseError parse_bool_at(
    std::string_view json,
    std::size_t index,
    bool& output) noexcept {
    if (json.substr(index, 4) == "true") {
        output = true;
        return Os2lParseError::None;
    }
    if (json.substr(index, 5) == "false") {
        output = false;
        return Os2lParseError::None;
    }
    return Os2lParseError::InvalidBoolean;
}

template <std::size_t Capacity>
[[nodiscard]] Os2lParseError get_string(
    std::string_view json,
    std::string_view key,
    FixedText<Capacity>& output,
    bool required = true) noexcept {
    std::size_t index = 0;
    if (!locate_value(json, key, index)) {
        output.length = 0;
        return required ? Os2lParseError::MissingField : Os2lParseError::None;
    }
    return parse_string_at(json, index, output);
}

template <typename Number>
[[nodiscard]] Os2lParseError get_number(
    std::string_view json,
    std::string_view key,
    Number& output,
    bool required = true) noexcept {
    std::size_t index = 0;
    if (!locate_value(json, key, index)) {
        return required ? Os2lParseError::MissingField : Os2lParseError::None;
    }
    return parse_number_at(json, index, output);
}

[[nodiscard]] Os2lParseError get_bool(
    std::string_view json,
    std::string_view key,
    bool& output) noexcept {
    std::size_t index = 0;
    if (!locate_value(json, key, index)) {
        return Os2lParseError::MissingField;
    }
    return parse_bool_at(json, index, output);
}

}  // namespace

Os2lParseError parse_os2l(std::string_view json, Os2lEvent& event) noexcept {
    event = {};
    if (json.empty()) {
        return Os2lParseError::Empty;
    }
    if (json.size() > 4096U) {
        return Os2lParseError::Oversized;
    }
    const auto first = skip_space(json, 0);
    auto last = json.size();
    while (last > 0 && std::isspace(static_cast<unsigned char>(json[last - 1])) != 0) {
        --last;
    }
    if (first >= last || json[first] != '{' || json[last - 1] != '}') {
        return Os2lParseError::Malformed;
    }

    FixedText<24> event_name{};
    const auto event_error = get_string(json, "evt", event_name);
    if (event_error != Os2lParseError::None) {
        return event_error == Os2lParseError::MissingField
            ? Os2lParseError::MissingEvent
            : event_error;
    }

    if (event_name.view() == "beat") {
        event.kind = Os2lKind::Beat;
        if (const auto error = get_bool(json, "change", event.beat.change);
            error != Os2lParseError::None) {
            return error;
        }
        if (const auto error = get_number(json, "pos", event.beat.position);
            error != Os2lParseError::None) {
            return error;
        }
        if (const auto error = get_number(json, "bpm", event.beat.bpm);
            error != Os2lParseError::None) {
            return error;
        }
        std::size_t strength_index = 0;
        if (locate_value(json, "strength", strength_index)) {
            event.beat.has_strength = true;
            if (const auto error = parse_number_at(json, strength_index, event.beat.strength);
                error != Os2lParseError::None) {
                return error;
            }
        }
        return Os2lParseError::None;
    }

    if (event_name.view() == "btn" || event_name.view() == "feedback") {
        event.kind = event_name.view() == "btn" ? Os2lKind::Button : Os2lKind::Feedback;
        if (const auto error = get_string(json, "name", event.button.name);
            error != Os2lParseError::None) {
            return error;
        }
        if (const auto error = get_string(json, "page", event.button.page, false);
            error != Os2lParseError::None) {
            return error;
        }
        FixedText<8> state{};
        if (const auto error = get_string(json, "state", state);
            error != Os2lParseError::None) {
            return error;
        }
        if (state.view() == "on") {
            event.button.on = true;
        } else if (state.view() == "off") {
            event.button.on = false;
        } else {
            return Os2lParseError::Malformed;
        }
        return Os2lParseError::None;
    }

    if (event_name.view() == "cmd") {
        event.kind = Os2lKind::Command;
        if (const auto error = get_number(json, "id", event.command.id);
            error != Os2lParseError::None) {
            return error;
        }
        if (const auto error = get_number(json, "param", event.command.parameter);
            error != Os2lParseError::None) {
            return error;
        }
        return Os2lParseError::None;
    }

    event.kind = Os2lKind::Unknown;
    return Os2lParseError::None;
}

void Os2lStreamDecoder::reset_message() noexcept {
    length_ = 0;
    depth_ = 0;
    started_ = false;
    in_string_ = false;
    escaped_ = false;
    discarding_ = false;
}

void Os2lStreamDecoder::reset() noexcept {
    reset_message();
}

Os2lStreamResult Os2lStreamDecoder::feed(
    std::string_view bytes,
    Os2lStreamCallback callback,
    void* context) noexcept {
    Os2lStreamResult result{};

    for (const auto character : bytes) {
        if (!started_) {
            if (std::isspace(static_cast<unsigned char>(character)) != 0) {
                continue;
            }
            if (character != '{') {
                ++result.errors;
                continue;
            }
            started_ = true;
            depth_ = 1;
            buffer_[0] = character;
            length_ = 1;
            continue;
        }

        if (!discarding_) {
            if (length_ >= buffer_.size()) {
                discarding_ = true;
                ++result.errors;
            } else {
                buffer_[length_++] = character;
            }
        }

        if (in_string_) {
            if (escaped_) {
                escaped_ = false;
            } else if (character == '\\') {
                escaped_ = true;
            } else if (character == '"') {
                in_string_ = false;
            }
            continue;
        }

        if (character == '"') {
            in_string_ = true;
            continue;
        }
        if (character == '{') {
            ++depth_;
            continue;
        }
        if (character != '}') {
            continue;
        }

        if (depth_ > 0) {
            --depth_;
        }
        if (depth_ != 0) {
            continue;
        }

        if (!discarding_) {
            Os2lEvent event{};
            const std::string_view raw(buffer_.data(), length_);
            const auto error = parse_os2l(raw, event);
            ++result.messages;
            if (error != Os2lParseError::None) {
                ++result.errors;
            }
            if (callback != nullptr) {
                callback(event, error, raw, context);
            }
        }
        reset_message();
    }

    return result;
}

}  // namespace showcore
