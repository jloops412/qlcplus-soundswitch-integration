#include "showcore/winmm_midi.hpp"

#include "showcore/spsc_queue.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#endif

namespace showcore {
namespace {

template <std::size_t SourceCapacity, std::size_t DestinationCapacity>
void copy_port_name(
    const char (&source)[SourceCapacity],
    std::array<char, DestinationCapacity>& destination,
    std::size_t& length) noexcept {
    length = 0;
    const auto limit = std::min(SourceCapacity, destination.size());
    while (length < limit && source[length] != '\0') {
        destination[length] = source[length];
        ++length;
    }
}

}  // namespace

#ifdef _WIN32

MidiPortList enumerate_winmm_midi_inputs() noexcept {
    MidiPortList list;
    const auto available = static_cast<std::size_t>(::midiInGetNumDevs());
    list.truncated = available > list.ports.size();
    const auto count = std::min(available, list.ports.size());
    for (std::size_t index = 0; index < count; ++index) {
        MIDIINCAPSA capabilities{};
        if (::midiInGetDevCapsA(
                static_cast<UINT_PTR>(index),
                &capabilities,
                static_cast<UINT>(sizeof(capabilities))) != MMSYSERR_NOERROR) {
            continue;
        }
        auto& port = list.ports[list.count++];
        port.system_index = static_cast<std::uint32_t>(index);
        port.direction = MidiPortDirection::Input;
        port.manufacturer_id = capabilities.wMid;
        port.product_id = capabilities.wPid;
        port.driver_version = capabilities.vDriverVersion;
        copy_port_name(capabilities.szPname, port.name_bytes, port.name_length);
    }
    return list;
}

MidiPortList enumerate_winmm_midi_outputs() noexcept {
    MidiPortList list;
    const auto available = static_cast<std::size_t>(::midiOutGetNumDevs());
    list.truncated = available > list.ports.size();
    const auto count = std::min(available, list.ports.size());
    for (std::size_t index = 0; index < count; ++index) {
        MIDIOUTCAPSA capabilities{};
        if (::midiOutGetDevCapsA(
                static_cast<UINT_PTR>(index),
                &capabilities,
                static_cast<UINT>(sizeof(capabilities))) != MMSYSERR_NOERROR) {
            continue;
        }
        auto& port = list.ports[list.count++];
        port.system_index = static_cast<std::uint32_t>(index);
        port.direction = MidiPortDirection::Output;
        port.manufacturer_id = capabilities.wMid;
        port.product_id = capabilities.wPid;
        port.driver_version = capabilities.vDriverVersion;
        copy_port_name(capabilities.szPname, port.name_bytes, port.name_length);
    }
    return list;
}

struct WinMmMidiInput::Impl {
    struct InputSlot {
        HMIDIIN handle{nullptr};
        std::uint32_t logical_device_id{kAnyMidiDevice};
        SpscQueue<MidiMessage, 257> queue{};
        std::atomic<std::uint64_t> dropped{0};
        std::atomic<bool> active{false};
    };

    static void CALLBACK callback(
        HMIDIIN,
        UINT message,
        DWORD_PTR instance,
        DWORD_PTR parameter1,
        DWORD_PTR) noexcept {
        auto* slot = reinterpret_cast<InputSlot*>(instance);
        if (slot == nullptr || !slot->active.load(std::memory_order_acquire)) {
            return;
        }
        if (message != MIM_DATA && message != MIM_MOREDATA) {
            return;
        }
        MidiMessage decoded{};
        if (!decode_short_midi(
                slot->logical_device_id,
                static_cast<std::uint32_t>(parameter1),
                static_cast<std::uint64_t>(::GetTickCount64()),
                decoded)) {
            return;
        }
        if (!slot->queue.try_push(decoded)) {
            slot->dropped.fetch_add(1, std::memory_order_relaxed);
        }
    }

    std::array<InputSlot, kMaxOpenMidiPorts> slots{};
    std::size_t next_poll{0};
    std::uint32_t last_error{MMSYSERR_NOERROR};

};

struct WinMmMidiOutput::Impl {
    struct OutputSlot {
        HMIDIOUT handle{nullptr};
        std::uint32_t logical_device_id{kAnyMidiDevice};
        bool active{false};
    };

    std::array<OutputSlot, kMaxOpenMidiPorts> slots{};
    std::uint32_t last_error{MMSYSERR_NOERROR};
};

WinMmMidiInput::WinMmMidiInput() noexcept : impl_(new (std::nothrow) Impl{}) {}
WinMmMidiInput::~WinMmMidiInput() noexcept { close_all(); }

bool WinMmMidiInput::supported() noexcept { return true; }

bool WinMmMidiInput::open(
    std::uint32_t system_index,
    std::uint32_t logical_device_id) noexcept {
    if (impl_ == nullptr || logical_device_id == kAnyMidiDevice) {
        return false;
    }
    for (const auto& slot : impl_->slots) {
        if (slot.active.load(std::memory_order_acquire) &&
            slot.logical_device_id == logical_device_id) {
            return false;
        }
    }
    auto slot = std::find_if(
        impl_->slots.begin(),
        impl_->slots.end(),
        [](const auto& candidate) {
            return !candidate.active.load(std::memory_order_acquire);
        });
    if (slot == impl_->slots.end()) {
        return false;
    }

    slot->logical_device_id = logical_device_id;
    slot->queue.reset();
    slot->dropped.store(0, std::memory_order_relaxed);
    impl_->last_error = ::midiInOpen(
        &slot->handle,
        static_cast<UINT>(system_index),
        reinterpret_cast<DWORD_PTR>(&Impl::callback),
        reinterpret_cast<DWORD_PTR>(std::addressof(*slot)),
        CALLBACK_FUNCTION | MIDI_IO_STATUS);
    if (impl_->last_error != MMSYSERR_NOERROR) {
        slot->handle = nullptr;
        slot->logical_device_id = kAnyMidiDevice;
        return false;
    }

    slot->active.store(true, std::memory_order_release);
    impl_->last_error = ::midiInStart(slot->handle);
    if (impl_->last_error != MMSYSERR_NOERROR) {
        slot->active.store(false, std::memory_order_release);
        static_cast<void>(::midiInClose(slot->handle));
        slot->handle = nullptr;
        slot->logical_device_id = kAnyMidiDevice;
        return false;
    }
    return true;
}

bool WinMmMidiInput::close(std::uint32_t logical_device_id) noexcept {
    if (impl_ == nullptr) {
        return false;
    }
    for (auto& slot : impl_->slots) {
        if (!slot.active.load(std::memory_order_acquire) ||
            slot.logical_device_id != logical_device_id) {
            continue;
        }
        slot.active.store(false, std::memory_order_release);
        static_cast<void>(::midiInStop(slot.handle));
        static_cast<void>(::midiInReset(slot.handle));
        impl_->last_error = ::midiInClose(slot.handle);
        slot.handle = nullptr;
        slot.logical_device_id = kAnyMidiDevice;
        slot.queue.reset();
        return impl_->last_error == MMSYSERR_NOERROR;
    }
    return false;
}

void WinMmMidiInput::close_all() noexcept {
    if (impl_ == nullptr) {
        return;
    }
    for (auto& slot : impl_->slots) {
        if (slot.active.load(std::memory_order_acquire)) {
            static_cast<void>(close(slot.logical_device_id));
        }
    }
}

bool WinMmMidiInput::poll(MidiMessage& message) noexcept {
    if (impl_ == nullptr) {
        return false;
    }
    for (std::size_t offset = 0; offset < impl_->slots.size(); ++offset) {
        const auto index = (impl_->next_poll + offset) % impl_->slots.size();
        auto& slot = impl_->slots[index];
        if (slot.active.load(std::memory_order_acquire) && slot.queue.try_pop(message)) {
            impl_->next_poll = (index + 1U) % impl_->slots.size();
            return true;
        }
    }
    return false;
}

std::size_t WinMmMidiInput::open_port_count() const noexcept {
    if (impl_ == nullptr) {
        return 0;
    }
    return static_cast<std::size_t>(std::count_if(
        impl_->slots.begin(),
        impl_->slots.end(),
        [](const auto& slot) {
            return slot.active.load(std::memory_order_acquire);
        }));
}

std::uint64_t WinMmMidiInput::dropped_messages() const noexcept {
    if (impl_ == nullptr) {
        return 0;
    }
    std::uint64_t total = 0;
    for (const auto& slot : impl_->slots) {
        total += slot.dropped.load(std::memory_order_relaxed);
    }
    return total;
}

std::uint32_t WinMmMidiInput::last_error() const noexcept {
    return impl_ == nullptr
        ? static_cast<std::uint32_t>(MMSYSERR_NOMEM)
        : impl_->last_error;
}

WinMmMidiOutput::WinMmMidiOutput() noexcept : impl_(new (std::nothrow) Impl{}) {}
WinMmMidiOutput::~WinMmMidiOutput() noexcept { close_all(); }

bool WinMmMidiOutput::supported() noexcept { return true; }

bool WinMmMidiOutput::open(
    std::uint32_t system_index,
    std::uint32_t logical_device_id) noexcept {
    if (impl_ == nullptr || logical_device_id == kAnyMidiDevice) {
        return false;
    }
    for (const auto& slot : impl_->slots) {
        if (slot.active && slot.logical_device_id == logical_device_id) {
            return false;
        }
    }
    auto slot = std::find_if(
        impl_->slots.begin(),
        impl_->slots.end(),
        [](const auto& candidate) { return !candidate.active; });
    if (slot == impl_->slots.end()) {
        return false;
    }
    impl_->last_error = ::midiOutOpen(
        &slot->handle,
        static_cast<UINT>(system_index),
        0,
        0,
        CALLBACK_NULL);
    if (impl_->last_error != MMSYSERR_NOERROR) {
        slot->handle = nullptr;
        return false;
    }
    slot->logical_device_id = logical_device_id;
    slot->active = true;
    return true;
}

bool WinMmMidiOutput::close(std::uint32_t logical_device_id) noexcept {
    if (impl_ == nullptr) {
        return false;
    }
    for (auto& slot : impl_->slots) {
        if (!slot.active || slot.logical_device_id != logical_device_id) {
            continue;
        }
        static_cast<void>(::midiOutReset(slot.handle));
        impl_->last_error = ::midiOutClose(slot.handle);
        slot.active = false;
        slot.handle = nullptr;
        slot.logical_device_id = kAnyMidiDevice;
        return impl_->last_error == MMSYSERR_NOERROR;
    }
    return false;
}

void WinMmMidiOutput::close_all() noexcept {
    if (impl_ == nullptr) {
        return;
    }
    for (auto& slot : impl_->slots) {
        if (slot.active) {
            static_cast<void>(close(slot.logical_device_id));
        }
    }
}

bool WinMmMidiOutput::send(
    std::uint32_t logical_device_id,
    const MidiMessage& message) noexcept {
    if (impl_ == nullptr) {
        return false;
    }
    const auto packet = encode_short_midi(message);
    if (!packet) {
        return false;
    }
    for (auto& slot : impl_->slots) {
        if (!slot.active || slot.logical_device_id != logical_device_id) {
            continue;
        }
        impl_->last_error = ::midiOutShortMsg(slot.handle, packet.packed);
        return impl_->last_error == MMSYSERR_NOERROR;
    }
    return false;
}

std::size_t WinMmMidiOutput::open_port_count() const noexcept {
    if (impl_ == nullptr) {
        return 0;
    }
    return static_cast<std::size_t>(std::count_if(
        impl_->slots.begin(),
        impl_->slots.end(),
        [](const auto& slot) { return slot.active; }));
}

std::uint32_t WinMmMidiOutput::last_error() const noexcept {
    return impl_ == nullptr
        ? static_cast<std::uint32_t>(MMSYSERR_NOMEM)
        : impl_->last_error;
}

#else

MidiPortList enumerate_winmm_midi_inputs() noexcept { return {}; }
MidiPortList enumerate_winmm_midi_outputs() noexcept { return {}; }

struct WinMmMidiInput::Impl {
    std::uint32_t last_error{1};
};

struct WinMmMidiOutput::Impl {
    std::uint32_t last_error{1};
};

WinMmMidiInput::WinMmMidiInput() noexcept : impl_(new (std::nothrow) Impl{}) {}
WinMmMidiInput::~WinMmMidiInput() noexcept = default;
bool WinMmMidiInput::supported() noexcept { return false; }
bool WinMmMidiInput::open(std::uint32_t, std::uint32_t) noexcept { return false; }
bool WinMmMidiInput::close(std::uint32_t) noexcept { return false; }
void WinMmMidiInput::close_all() noexcept {}
bool WinMmMidiInput::poll(MidiMessage&) noexcept { return false; }
std::size_t WinMmMidiInput::open_port_count() const noexcept { return 0; }
std::uint64_t WinMmMidiInput::dropped_messages() const noexcept { return 0; }
std::uint32_t WinMmMidiInput::last_error() const noexcept {
    return impl_ == nullptr ? 1U : impl_->last_error;
}

WinMmMidiOutput::WinMmMidiOutput() noexcept : impl_(new (std::nothrow) Impl{}) {}
WinMmMidiOutput::~WinMmMidiOutput() noexcept = default;
bool WinMmMidiOutput::supported() noexcept { return false; }
bool WinMmMidiOutput::open(std::uint32_t, std::uint32_t) noexcept { return false; }
bool WinMmMidiOutput::close(std::uint32_t) noexcept { return false; }
void WinMmMidiOutput::close_all() noexcept {}
bool WinMmMidiOutput::send(std::uint32_t, const MidiMessage&) noexcept { return false; }
std::size_t WinMmMidiOutput::open_port_count() const noexcept { return 0; }
std::uint32_t WinMmMidiOutput::last_error() const noexcept {
    return impl_ == nullptr ? 1U : impl_->last_error;
}

#endif

}  // namespace showcore
