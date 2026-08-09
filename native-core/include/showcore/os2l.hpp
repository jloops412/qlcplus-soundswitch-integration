#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace showcore {

template <std::size_t Capacity>
struct FixedText {
    std::array<char, Capacity> bytes{};
    std::size_t length{0};

    [[nodiscard]] std::string_view view() const noexcept {
        return {bytes.data(), length};
    }
};

enum class Os2lKind : std::uint8_t {
    Invalid,
    Beat,
    Button,
    Command,
    Feedback,
    Unknown
};

enum class Os2lParseError : std::uint8_t {
    None,
    Empty,
    Oversized,
    Malformed,
    MissingEvent,
    MissingField,
    FieldTooLong,
    InvalidNumber,
    InvalidBoolean
};

struct Os2lBeat {
    bool change{false};
    std::int64_t position{0};
    double bpm{0.0};
    double strength{0.0};
    bool has_strength{false};
};

struct Os2lButton {
    FixedText<96> name{};
    FixedText<64> page{};
    bool on{false};
};

struct Os2lCommand {
    std::int32_t id{0};
    double parameter{0.0};
};

struct Os2lEvent {
    Os2lKind kind{Os2lKind::Invalid};
    Os2lBeat beat{};
    Os2lButton button{};
    Os2lCommand command{};
};

[[nodiscard]] Os2lParseError parse_os2l(std::string_view json, Os2lEvent& event) noexcept;

using Os2lStreamCallback = void (*)(
    const Os2lEvent& event,
    Os2lParseError error,
    std::string_view raw_message,
    void* context) noexcept;

struct Os2lStreamResult {
    std::size_t messages{0};
    std::size_t errors{0};
};

class Os2lStreamDecoder {
public:
    void reset() noexcept;
    [[nodiscard]] Os2lStreamResult feed(
        std::string_view bytes,
        Os2lStreamCallback callback,
        void* context) noexcept;

private:
    void reset_message() noexcept;

    std::array<char, 4096> buffer_{};
    std::size_t length_{0};
    std::size_t depth_{0};
    bool started_{false};
    bool in_string_{false};
    bool escaped_{false};
    bool discarding_{false};
};

}  // namespace showcore
