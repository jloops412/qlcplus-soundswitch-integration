#pragma once

#include "showcore/midi.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

namespace showcore {

inline constexpr std::size_t kMaxEnumeratedMidiPorts = 32;
inline constexpr std::size_t kMaxOpenMidiPorts = 16;
inline constexpr std::size_t kMidiPortNameCapacity = 128;

enum class MidiPortDirection : std::uint8_t {
    Input,
    Output
};

struct MidiPortInfo {
    std::uint32_t system_index{0};
    MidiPortDirection direction{MidiPortDirection::Input};
    std::uint16_t manufacturer_id{0};
    std::uint16_t product_id{0};
    std::uint32_t driver_version{0};
    std::array<char, kMidiPortNameCapacity> name_bytes{};
    std::size_t name_length{0};

    [[nodiscard]] std::string_view name() const noexcept {
        return {name_bytes.data(), name_length};
    }
};

struct MidiPortList {
    std::array<MidiPortInfo, kMaxEnumeratedMidiPorts> ports{};
    std::size_t count{0};
    bool truncated{false};
};

[[nodiscard]] MidiPortList enumerate_winmm_midi_inputs() noexcept;
[[nodiscard]] MidiPortList enumerate_winmm_midi_outputs() noexcept;

class WinMmMidiInput {
public:
    WinMmMidiInput() noexcept;
    ~WinMmMidiInput() noexcept;

    WinMmMidiInput(const WinMmMidiInput&) = delete;
    WinMmMidiInput& operator=(const WinMmMidiInput&) = delete;

    [[nodiscard]] static bool supported() noexcept;
    [[nodiscard]] bool open(
        std::uint32_t system_index,
        std::uint32_t logical_device_id) noexcept;
    [[nodiscard]] bool close(std::uint32_t logical_device_id) noexcept;
    void close_all() noexcept;
    [[nodiscard]] bool poll(MidiMessage& message) noexcept;
    // Performs a bounded driver-handle probe. This is lifecycle evidence for
    // owner cleanup/retry, not proof that the physical controller is usable.
    [[nodiscard]] bool connected(std::uint32_t logical_device_id) noexcept;
    [[nodiscard]] std::size_t open_port_count() const noexcept;
    [[nodiscard]] std::uint64_t dropped_messages() const noexcept;
    [[nodiscard]] std::uint32_t last_error() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

class WinMmMidiOutput {
public:
    WinMmMidiOutput() noexcept;
    ~WinMmMidiOutput() noexcept;

    WinMmMidiOutput(const WinMmMidiOutput&) = delete;
    WinMmMidiOutput& operator=(const WinMmMidiOutput&) = delete;

    [[nodiscard]] static bool supported() noexcept;
    [[nodiscard]] bool open(
        std::uint32_t system_index,
        std::uint32_t logical_device_id) noexcept;
    [[nodiscard]] bool close(std::uint32_t logical_device_id) noexcept;
    void close_all() noexcept;
    [[nodiscard]] bool send(
        std::uint32_t logical_device_id,
        const MidiMessage& message) noexcept;
    [[nodiscard]] std::size_t open_port_count() const noexcept;
    [[nodiscard]] std::uint32_t last_error() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace showcore
