#pragma once

#include "showcore/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace showcore {

inline constexpr std::uint32_t kAnyMidiDevice = 0xFFFFFFFFU;
inline constexpr std::uint8_t kAnyMidiChannel = 0xFFU;
inline constexpr std::size_t kMaxMidiMappings = 256;
inline constexpr std::size_t kMaxMidiActionsPerMessage = 16;

enum class MidiMessageType : std::uint8_t {
    NoteOn,
    NoteOff,
    ControlChange,
    PitchBend
};

struct MidiMessage {
    std::uint32_t device_id{kAnyMidiDevice};
    MidiMessageType type{MidiMessageType::ControlChange};
    std::uint8_t channel{0};
    std::uint8_t number{0};
    std::uint16_t value{0};
    std::uint64_t timestamp_ms{0};
};

struct MidiShortPacket {
    std::uint32_t packed{0};
    bool valid{false};

    [[nodiscard]] explicit constexpr operator bool() const noexcept { return valid; }
};

[[nodiscard]] bool decode_short_midi(
    std::uint32_t logical_device_id,
    std::uint32_t packed,
    std::uint64_t timestamp_ms,
    MidiMessage& message) noexcept;

[[nodiscard]] MidiShortPacket encode_short_midi(const MidiMessage& message) noexcept;

enum class MidiInputMode : std::uint8_t {
    Absolute7,
    Absolute14,
    RelativeTwosComplement
};

enum class MappingBehavior : std::uint8_t {
    Momentary,
    Toggle,
    Latch,
    Continuous,
    Relative
};

enum class ActionType : std::uint8_t {
    None,
    SetProperty,
    Blackout,
    TriggerLook,
    TriggerAutoloop,
    TapTempo,
    ArmFog,
    ClearLook,
    ClearAutoloop,
    NextAutoloop,
    PreviousAutoloop,
    WorkLight,
    ArmHaze,
    ArmLaser,
    ArmSpark,
    Count
};

struct ActionDescriptor {
    ActionType type{ActionType::None};
    LayerId layer{LayerId::ManualOverride};
    Property property{Property::Intensity};
    std::uint16_t target_id{0};
};

struct MidiMapping {
    std::uint32_t device_id{kAnyMidiDevice};
    MidiMessageType message_type{MidiMessageType::ControlChange};
    std::uint8_t channel{kAnyMidiChannel};
    std::uint8_t number{0};
    MidiInputMode input_mode{MidiInputMode::Absolute7};
    MappingBehavior behavior{MappingBehavior::Continuous};
    ActionDescriptor action{};
    float output_min{0.0F};
    float output_max{1.0F};
    float curve{1.0F};
    bool inverted{false};
    bool soft_takeover{false};
    float takeover_tolerance{0.025F};

    bool takeover_engaged{false};
    bool has_previous{false};
    float previous_input{0.0F};
    float current_target{0.0F};
};

struct MidiActionEvent {
    ActionDescriptor action{};
    MappingBehavior behavior{MappingBehavior::Continuous};
    bool active{false};
    bool relative{false};
    float value{0.0F};
    std::size_t mapping_index{0};
};

class MidiMappingEngine {
public:
    void clear() noexcept;
    [[nodiscard]] bool add(const MidiMapping& mapping) noexcept;
    [[nodiscard]] bool set_takeover_target(std::size_t mapping_index, float target) noexcept;
    [[nodiscard]] std::size_t size() const noexcept { return count_; }

    [[nodiscard]] std::size_t process(
        const MidiMessage& message,
        std::array<MidiActionEvent, kMaxMidiActionsPerMessage>& events) noexcept;

private:
    std::array<MidiMapping, kMaxMidiMappings> mappings_{};
    std::size_t count_{0};
};

}  // namespace showcore
