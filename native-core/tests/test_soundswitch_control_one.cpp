#include "showcore/output_backend.hpp"
#include "showcore/soundswitch_control_one.hpp"

#include <chrono>
#include <cstddef>
#include <iostream>
#include <string_view>

namespace {

int failures = 0;

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ \
                  << " - " #condition "\n"; \
        ++failures; \
    } \
} while (false)

}  // namespace

int main() {
    static_assert(showcore::kSoundSwitchVendorId == 0x15E4U);
    static_assert(showcore::kSoundSwitchControlOneProductId == 0x0054U);
    static_assert(showcore::kSoundSwitchControlOneOutputCount == 2U);
    static_assert(showcore::kSoundSwitchControlOneFrameSize == 522U);

    const showcore::SoundSwitchControlOneSessionConfig defaults;
    CHECK(showcore::valid_soundswitch_control_one_session_config(defaults));
    CHECK(defaults.transfer_timeout == std::chrono::milliseconds{500});
    CHECK(defaults.frame_interval == std::chrono::milliseconds{25});
    CHECK(defaults.warmup_blackout_pairs == 2U);
    CHECK(defaults.close_blackout_pairs == 3U);

    auto invalid = defaults;
    invalid.frame_interval = std::chrono::milliseconds{9};
    CHECK(!showcore::valid_soundswitch_control_one_session_config(invalid));
    invalid = defaults;
    invalid.transfer_timeout = std::chrono::milliseconds{0};
    CHECK(!showcore::valid_soundswitch_control_one_session_config(invalid));
    invalid = defaults;
    invalid.warmup_blackout_pairs = 41U;
    CHECK(!showcore::valid_soundswitch_control_one_session_config(invalid));

    CHECK(std::string_view(showcore::soundswitch_control_one_lifecycle_name(
              showcore::SoundSwitchControlOneLifecycleState::Initializing)) ==
          "initializing");
    CHECK(std::string_view(showcore::soundswitch_control_one_lifecycle_name(
              showcore::SoundSwitchControlOneLifecycleState::Fault)) == "fault");

    showcore::DmxUniverse first{};
    first[0] = 0x11U;
    first[511] = 0xFEU;
    const auto first_packet = showcore::build_soundswitch_control_one_packet(
        showcore::SoundSwitchControlOnePort::One, first);
    CHECK(first_packet.length == 522U);
    CHECK(first_packet.bytes[0] == 's');
    CHECK(first_packet.bytes[1] == 'T');
    CHECK(first_packet.bytes[2] == 'R');
    CHECK(first_packet.bytes[3] == 't');
    CHECK(first_packet.bytes[4] == 0x01U);
    CHECK(first_packet.bytes[5] == 0x00U);
    CHECK(first_packet.bytes[6] == 0x02U);
    CHECK(first_packet.bytes[7] == 0x02U);
    CHECK(first_packet.bytes[8] == 0x00U);
    CHECK(first_packet.bytes[9] == 0x00U);
    CHECK(first_packet.bytes[10] == 0x11U);
    CHECK(first_packet.bytes[521] == 0xFEU);

    showcore::DmxUniverse second{};
    second[0] = 0x22U;
    second[511] = 0xABU;
    const auto second_packet = showcore::build_soundswitch_control_one_packet(
        showcore::SoundSwitchControlOnePort::Two, second);
    CHECK(second_packet.bytes[8] == 0x01U);
    CHECK(second_packet.bytes[10] == 0x22U);
    CHECK(second_packet.bytes[521] == 0xABU);
    for (std::size_t index = 0U; index < first.size(); ++index) {
        CHECK(first_packet.bytes[index + 10U] == first[index]);
        CHECK(second_packet.bytes[index + 10U] == second[index]);
    }

    const auto& controls = showcore::kSoundSwitchControlOneInitializationPackets;
    CHECK(controls[0][8] == 0x00U && controls[0][10] == 0x01U);
    CHECK(controls[1][8] == 0x01U && controls[1][10] == 0xFFU &&
          controls[1][11] == 0xFFU);
    CHECK(controls[2][8] == 0x00U && controls[2][10] == 0x01U);
    CHECK(controls[3][8] == 0x01U && controls[3][10] == 0x01U);

    const auto& backend = showcore::output_backend_descriptor(
        showcore::OutputBackendKind::SoundSwitchControlOne);
    CHECK(backend.implementation ==
          showcore::OutputImplementationStage::Implemented);
    CHECK(backend.evidence == showcore::OutputEvidenceStage::ContractTested);
    CHECK(backend.hardware_max_universes == 2U);
    CHECK(backend.emberlights_supported_universes == 2U);
    CHECK(backend.configuration_policy ==
          showcore::OutputBackendDescriptor::ConfigurationPolicy::ExperimentalOptIn);
    CHECK(!showcore::output_backend_is_configurable(backend));
    CHECK(showcore::output_backend_is_configurable(backend, true));
    CHECK(showcore::has_output_capability(
        backend.capabilities, showcore::OutputCapability::HotReconnect));
    CHECK(showcore::has_output_capability(
        backend.capabilities, showcore::OutputCapability::SafeBlackout));
    CHECK(showcore::has_output_capability(
        backend.capabilities, showcore::OutputCapability::MultipleUniverses));

    if (failures == 0) {
        std::cout << "SoundSwitch Control One protocol tests passed\n";
        return 0;
    }
    return 1;
}
