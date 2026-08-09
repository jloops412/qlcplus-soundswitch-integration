#include "showcore/midi.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace showcore {
namespace {

[[nodiscard]] bool type_matches(MidiMessageType mapped, MidiMessageType received) noexcept {
    return mapped == received || (mapped == MidiMessageType::NoteOn && received == MidiMessageType::NoteOff);
}

[[nodiscard]] float absolute_input(const MidiMessage& message, MidiInputMode mode) noexcept {
    if (mode == MidiInputMode::Absolute14) {
        return std::clamp(static_cast<float>(message.value) / 16383.0F, 0.0F, 1.0F);
    }
    return std::clamp(static_cast<float>(message.value) / 127.0F, 0.0F, 1.0F);
}

[[nodiscard]] float relative_input(std::uint16_t raw) noexcept {
    const auto value = static_cast<std::uint8_t>(raw & 0x7FU);
    if (value == 0U) {
        return 0.0F;
    }
    if (value <= 63U) {
        return static_cast<float>(value) / 63.0F;
    }
    const auto signed_value = static_cast<std::int16_t>(value) - 128;
    return std::clamp(static_cast<float>(signed_value) / 63.0F, -1.0F, 0.0F);
}

}  // namespace

bool decode_short_midi(
    std::uint32_t logical_device_id,
    std::uint32_t packed,
    std::uint64_t timestamp_ms,
    MidiMessage& message) noexcept {
    const auto status = static_cast<std::uint8_t>(packed & 0xFFU);
    const auto data1 = static_cast<std::uint8_t>((packed >> 8U) & 0x7FU);
    const auto data2 = static_cast<std::uint8_t>((packed >> 16U) & 0x7FU);
    const auto message_class = static_cast<std::uint8_t>(status & 0xF0U);
    message = {};
    message.device_id = logical_device_id;
    message.channel = static_cast<std::uint8_t>(status & 0x0FU);
    message.timestamp_ms = timestamp_ms;

    switch (message_class) {
    case 0x80U:
        message.type = MidiMessageType::NoteOff;
        message.number = data1;
        message.value = data2;
        return true;
    case 0x90U:
        message.type = MidiMessageType::NoteOn;
        message.number = data1;
        message.value = data2;
        return true;
    case 0xB0U:
        message.type = MidiMessageType::ControlChange;
        message.number = data1;
        message.value = data2;
        return true;
    case 0xE0U:
        message.type = MidiMessageType::PitchBend;
        message.number = 0;
        message.value = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(data1) |
            (static_cast<std::uint16_t>(data2) << 7U));
        return true;
    default:
        message = {};
        return false;
    }
}

MidiShortPacket encode_short_midi(const MidiMessage& message) noexcept {
    if (message.channel > 15U) {
        return {};
    }

    std::uint8_t status = 0;
    std::uint8_t data1 = 0;
    std::uint8_t data2 = 0;
    switch (message.type) {
    case MidiMessageType::NoteOn:
        if (message.number > 127U || message.value > 127U) {
            return {};
        }
        status = 0x90U;
        data1 = message.number;
        data2 = static_cast<std::uint8_t>(message.value);
        break;
    case MidiMessageType::NoteOff:
        if (message.number > 127U || message.value > 127U) {
            return {};
        }
        status = 0x80U;
        data1 = message.number;
        data2 = static_cast<std::uint8_t>(message.value);
        break;
    case MidiMessageType::ControlChange:
        if (message.number > 127U || message.value > 127U) {
            return {};
        }
        status = 0xB0U;
        data1 = message.number;
        data2 = static_cast<std::uint8_t>(message.value);
        break;
    case MidiMessageType::PitchBend:
        if (message.value > 16383U) {
            return {};
        }
        status = 0xE0U;
        data1 = static_cast<std::uint8_t>(message.value & 0x7FU);
        data2 = static_cast<std::uint8_t>((message.value >> 7U) & 0x7FU);
        break;
    }

    return {
        static_cast<std::uint32_t>(status | message.channel) |
            (static_cast<std::uint32_t>(data1) << 8U) |
            (static_cast<std::uint32_t>(data2) << 16U),
        true};
}

void MidiMappingEngine::clear() noexcept {
    count_ = 0;
}

bool MidiMappingEngine::add(const MidiMapping& mapping) noexcept {
    if (count_ >= mappings_.size()) {
        return false;
    }
    mappings_[count_++] = mapping;
    return true;
}

bool MidiMappingEngine::set_takeover_target(std::size_t mapping_index, float target) noexcept {
    if (mapping_index >= count_) {
        return false;
    }
    auto& mapping = mappings_[mapping_index];
    mapping.current_target = std::clamp(target, 0.0F, 1.0F);
    mapping.takeover_engaged = !mapping.soft_takeover;
    mapping.has_previous = false;
    return true;
}

std::size_t MidiMappingEngine::process(
    const MidiMessage& message,
    std::array<MidiActionEvent, kMaxMidiActionsPerMessage>& events) noexcept {
    std::size_t event_count = 0;

    for (std::size_t index = 0; index < count_ && event_count < events.size(); ++index) {
        auto& mapping = mappings_[index];
        if (mapping.device_id != kAnyMidiDevice && mapping.device_id != message.device_id) {
            continue;
        }
        if (!type_matches(mapping.message_type, message.type)) {
            continue;
        }
        if (mapping.channel != kAnyMidiChannel && mapping.channel != message.channel) {
            continue;
        }
        if (mapping.number != message.number) {
            continue;
        }

        const bool relative = mapping.input_mode == MidiInputMode::RelativeTwosComplement;
        float value = relative ? relative_input(message.value) : absolute_input(message, mapping.input_mode);
        bool active = message.type != MidiMessageType::NoteOff &&
            !(message.type == MidiMessageType::NoteOn && message.value == 0U);

        if (!relative) {
            if (mapping.inverted) {
                value = 1.0F - value;
            }
            const auto curve = mapping.curve > 0.0F ? mapping.curve : 1.0F;
            value = std::pow(std::clamp(value, 0.0F, 1.0F), curve);
            value = mapping.output_min + value * (mapping.output_max - mapping.output_min);
            value = std::clamp(value, 0.0F, 1.0F);

            if (mapping.soft_takeover && !mapping.takeover_engaged && active) {
                const auto difference = value - mapping.current_target;
                const bool within = std::abs(difference) <= mapping.takeover_tolerance;
                const bool crossed = mapping.has_previous &&
                    (mapping.previous_input - mapping.current_target) * difference <= 0.0F;
                mapping.previous_input = value;
                mapping.has_previous = true;
                if (!within && !crossed) {
                    continue;
                }
                mapping.takeover_engaged = true;
            }
        }

        events[event_count++] = {
            mapping.action,
            mapping.behavior,
            active,
            relative,
            value,
            index};
    }

    return event_count;
}

}  // namespace showcore
