#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace showcore {

enum class OutputBackendKind : std::uint8_t {
    ArtNet,
    Sacn,
    DmxUsbPro,
    SoundSwitchMicro,
    SoundSwitchControlOne,
    WolfmixDmxInputBridge,
    Count
};

enum class OutputImplementationStage : std::uint8_t {
    Planned,
    IsolatedExperiment,
    BridgeOnly,
    Implemented
};

enum class OutputEvidenceStage : std::uint8_t {
    None,
    ContractTested,
    LoopbackVerified,
    HostAccepted,
    PhysicalVerified,
    GigQualified
};

enum class OutputCapability : std::uint32_t {
    None = 0U,
    Dmx512Frames = 1U << 0U,
    DirectHostOutput = 1U << 1U,
    NetworkTransport = 1U << 2U,
    SerialTransport = 1U << 3U,
    UsbTransport = 1U << 4U,
    HotReconnect = 1U << 5U,
    SafeBlackout = 1U << 6U,
    MultipleUniverses = 1U << 7U,
    ExternalDmxInput = 1U << 8U,
    ControllerSurface = 1U << 9U,
    RequiresIsolatedBroker = 1U << 10U
};

[[nodiscard]] constexpr OutputCapability operator|(
    OutputCapability left,
    OutputCapability right) noexcept {
    return static_cast<OutputCapability>(
        static_cast<std::uint32_t>(left) |
        static_cast<std::uint32_t>(right));
}

[[nodiscard]] constexpr bool has_output_capability(
    OutputCapability capabilities,
    OutputCapability capability) noexcept {
    return (static_cast<std::uint32_t>(capabilities) &
            static_cast<std::uint32_t>(capability)) != 0U;
}

struct OutputBackendDescriptor {
    OutputBackendKind kind{OutputBackendKind::ArtNet};
    const char* name{"Art-Net"};
    OutputImplementationStage implementation{OutputImplementationStage::Planned};
    OutputEvidenceStage evidence{OutputEvidenceStage::None};
    OutputCapability capabilities{OutputCapability::None};
    std::uint8_t hardware_max_universes{0U};
    std::uint8_t emberlights_supported_universes{0U};
    enum class ConfigurationPolicy : std::uint8_t {
        Unavailable,
        Normal,
        ExperimentalOptIn
    } configuration_policy{ConfigurationPolicy::Unavailable};
};

inline constexpr std::array<OutputBackendDescriptor,
                            static_cast<std::size_t>(OutputBackendKind::Count)>
    kOutputBackendDescriptors{{
        {OutputBackendKind::ArtNet,
         "Art-Net",
         OutputImplementationStage::Implemented,
         OutputEvidenceStage::LoopbackVerified,
         OutputCapability::Dmx512Frames | OutputCapability::DirectHostOutput |
             OutputCapability::NetworkTransport | OutputCapability::SafeBlackout |
             OutputCapability::MultipleUniverses,
         0U,
         2U,
         OutputBackendDescriptor::ConfigurationPolicy::Normal},
        {OutputBackendKind::Sacn,
         "sACN",
         OutputImplementationStage::Implemented,
         OutputEvidenceStage::LoopbackVerified,
         OutputCapability::Dmx512Frames | OutputCapability::DirectHostOutput |
             OutputCapability::NetworkTransport | OutputCapability::SafeBlackout |
             OutputCapability::MultipleUniverses,
         0U,
         2U,
         OutputBackendDescriptor::ConfigurationPolicy::Normal},
        {OutputBackendKind::DmxUsbPro,
         "DMX USB Pro",
         OutputImplementationStage::Implemented,
         OutputEvidenceStage::ContractTested,
         OutputCapability::Dmx512Frames | OutputCapability::DirectHostOutput |
             OutputCapability::SerialTransport | OutputCapability::HotReconnect |
             OutputCapability::SafeBlackout,
         1U,
         1U,
         OutputBackendDescriptor::ConfigurationPolicy::Normal},
        {OutputBackendKind::SoundSwitchMicro,
         "SoundSwitch Micro",
         OutputImplementationStage::Implemented,
         OutputEvidenceStage::HostAccepted,
         OutputCapability::Dmx512Frames | OutputCapability::DirectHostOutput |
             OutputCapability::UsbTransport | OutputCapability::HotReconnect |
             OutputCapability::SafeBlackout,
         1U,
         1U,
         OutputBackendDescriptor::ConfigurationPolicy::Normal},
        {OutputBackendKind::SoundSwitchControlOne,
         "SoundSwitch Control One DMX",
         OutputImplementationStage::Implemented,
         OutputEvidenceStage::ContractTested,
         OutputCapability::Dmx512Frames | OutputCapability::DirectHostOutput |
             OutputCapability::UsbTransport | OutputCapability::HotReconnect |
             OutputCapability::SafeBlackout |
             OutputCapability::ControllerSurface |
             OutputCapability::MultipleUniverses,
         2U,
         2U,
         OutputBackendDescriptor::ConfigurationPolicy::ExperimentalOptIn},
        {OutputBackendKind::WolfmixDmxInputBridge,
         "WOLFmix DMX-input bridge",
         OutputImplementationStage::BridgeOnly,
         OutputEvidenceStage::None,
         OutputCapability::Dmx512Frames | OutputCapability::ExternalDmxInput |
             OutputCapability::ControllerSurface |
             OutputCapability::MultipleUniverses,
         4U,
         0U,
         OutputBackendDescriptor::ConfigurationPolicy::Unavailable},
    }};

[[nodiscard]] constexpr bool output_backend_is_configurable(
    const OutputBackendDescriptor& descriptor,
    bool allow_experimental = false) noexcept {
    return descriptor.configuration_policy ==
               OutputBackendDescriptor::ConfigurationPolicy::Normal ||
        (allow_experimental && descriptor.configuration_policy ==
             OutputBackendDescriptor::ConfigurationPolicy::ExperimentalOptIn);
}

[[nodiscard]] constexpr const OutputBackendDescriptor& output_backend_descriptor(
    OutputBackendKind kind) noexcept {
    const auto index = static_cast<std::size_t>(kind);
    return index < kOutputBackendDescriptors.size()
        ? kOutputBackendDescriptors[index]
        : kOutputBackendDescriptors[0U];
}

enum class OutputHealthState : std::uint8_t {
    Disabled,
    Opening,
    Ready,
    Recovering,
    Fault,
    Stopping
};

[[nodiscard]] const char* output_health_state_name(OutputHealthState state) noexcept;

struct OutputBackendHealth {
    OutputBackendKind kind{OutputBackendKind::ArtNet};
    OutputHealthState state{OutputHealthState::Disabled};
    std::uint8_t first_source_universe{0U};
    std::uint8_t source_universe_count{0U};
    bool configured{false};
    std::uint64_t open_attempts{0U};
    std::uint64_t open_successes{0U};
    std::uint64_t reconnects{0U};
    std::uint64_t frames_attempted{0U};
    std::uint64_t frames_accepted{0U};
    std::uint64_t frames_failed{0U};
    std::uint32_t last_error{0U};
    std::uint16_t last_nonzero_slots{0U};
};

class AtomicOutputBackendHealth {
public:
    AtomicOutputBackendHealth() noexcept = default;

    AtomicOutputBackendHealth(const AtomicOutputBackendHealth&) = delete;
    AtomicOutputBackendHealth& operator=(const AtomicOutputBackendHealth&) = delete;

    void configure(
        OutputBackendKind kind,
        std::uint8_t first_source_universe,
        std::uint8_t source_universe_count,
        bool configured) noexcept;
    void mark_opening() noexcept;
    void mark_ready() noexcept;
    void mark_fault(std::uint32_t error) noexcept;
    void mark_stopping() noexcept;
    void mark_disabled() noexcept;
    void record_send(
        bool accepted,
        std::uint32_t error,
        std::uint16_t nonzero_slots) noexcept;

    [[nodiscard]] OutputBackendHealth snapshot() const noexcept;

private:
    std::atomic<OutputBackendKind> kind_{OutputBackendKind::ArtNet};
    std::atomic<OutputHealthState> state_{OutputHealthState::Disabled};
    std::atomic<std::uint8_t> first_source_universe_{0U};
    std::atomic<std::uint8_t> source_universe_count_{0U};
    std::atomic<bool> configured_{false};
    std::atomic<bool> ever_ready_{false};
    std::atomic<std::uint64_t> open_attempts_{0U};
    std::atomic<std::uint64_t> open_successes_{0U};
    std::atomic<std::uint64_t> reconnects_{0U};
    std::atomic<std::uint64_t> frames_attempted_{0U};
    std::atomic<std::uint64_t> frames_accepted_{0U};
    std::atomic<std::uint64_t> frames_failed_{0U};
    std::atomic<std::uint32_t> last_error_{0U};
    std::atomic<std::uint16_t> last_nonzero_slots_{0U};
};

}  // namespace showcore
