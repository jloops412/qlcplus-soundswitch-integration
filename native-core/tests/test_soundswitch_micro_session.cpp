#include "emberlights/hardware_qualification.hpp"
#include "emberlights/project.hpp"
#include "showcore/soundswitch_micro.hpp"

#include <algorithm>
#include <array>
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
    const showcore::SoundSwitchMicroSessionConfig defaults;
    CHECK(showcore::valid_soundswitch_micro_session_config(defaults));
    CHECK(defaults.transfer_timeout == std::chrono::milliseconds(500));
    CHECK(defaults.settling_interval == std::chrono::milliseconds(200));
    CHECK(defaults.frame_interval == std::chrono::milliseconds(25));
    CHECK(defaults.warmup_blackout_frames == 50U);
    CHECK(defaults.close_blackout_frames == 3U);

    auto invalid = defaults;
    invalid.transfer_timeout = std::chrono::milliseconds(0);
    CHECK(!showcore::valid_soundswitch_micro_session_config(invalid));
    invalid = defaults;
    invalid.frame_interval = std::chrono::milliseconds(0);
    CHECK(!showcore::valid_soundswitch_micro_session_config(invalid));
    invalid = defaults;
    invalid.warmup_blackout_frames = 401U;
    CHECK(!showcore::valid_soundswitch_micro_session_config(invalid));

    CHECK(showcore::kSoundSwitchMicroOpenSequence.front() ==
          showcore::SoundSwitchMicroLifecycleState::Detecting);
    CHECK(showcore::kSoundSwitchMicroOpenSequence.back() ==
          showcore::SoundSwitchMicroLifecycleState::Streaming);
    CHECK(std::string_view(showcore::soundswitch_micro_lifecycle_name(
              showcore::SoundSwitchMicroLifecycleState::WarmingUp)) == "warming-up");
    CHECK(std::string_view(showcore::soundswitch_micro_lifecycle_name(
              showcore::SoundSwitchMicroLifecycleState::Fault)) == "fault");

    showcore::DmxUniverse universe{};
    universe[0] = 0x11U;
    universe[511] = 0xFEU;
    const auto packet = showcore::build_soundswitch_micro_packet(
        universe, showcore::SoundSwitchMicroFraming::NativeJls1);
    CHECK(packet.length == 522U);
    CHECK(packet.bytes[10] == 0x11U);
    CHECK(packet.bytes[521] == 0xFEU);

    const auto project = emberlights::make_starter_project();
    const auto ir4_profile = std::find_if(
        project.fixture_profiles.begin(),
        project.fixture_profiles.end(),
        [](const auto& profile) {
            return profile.id == emberlights::kBothLightingIr4SixChannelProfileId;
        });
    CHECK(ir4_profile != project.fixture_profiles.end());
    if (ir4_profile != project.fixture_profiles.end()) {
        CHECK(ir4_profile->manufacturer == "Both Lighting");
        CHECK(ir4_profile->model == "BO-IR4 LED Mini Spotlight");
        CHECK(ir4_profile->mode == "6 Channel (manual-matched; CH6 Purple/UV)");
        CHECK(ir4_profile->footprint == 6U);
        CHECK(ir4_profile->channels.size() == 6U);
        constexpr std::array properties{
            showcore::Property::Red,
            showcore::Property::Green,
            showcore::Property::Blue,
            showcore::Property::White,
            showcore::Property::Amber,
            showcore::Property::UV};
        for (std::size_t index = 0U;
             index < properties.size() && index < ir4_profile->channels.size(); ++index) {
            CHECK(ir4_profile->channels[index].property == properties[index]);
            CHECK(ir4_profile->channels[index].coarse_offset == index);
        }
    }

    const auto qualification = emberlights::build_ir4_6ch_red_qualification();
    CHECK(qualification.exact());
    CHECK(qualification.validation.ok());
    CHECK(qualification.raw_reference[0] == 255U);
    CHECK(qualification.runner_rendered[0] == 255U);
    for (std::size_t index = 1U; index < showcore::kUniverseSlots; ++index) {
        CHECK(qualification.raw_reference[index] == 0U);
        CHECK(qualification.runner_rendered[index] == 0U);
    }
    CHECK(qualification.raw_packet.length == 522U);
    CHECK(qualification.runner_packet.length == 522U);
    CHECK(qualification.frame_comparison.differing_slots == 0U);
    CHECK(qualification.packet_comparison.differing_bytes == 0U);

    auto mismatch = qualification.runner_rendered;
    mismatch[4] = 1U;
    const auto comparison = emberlights::compare_dmx_frames(
        qualification.raw_reference, mismatch);
    CHECK(!comparison.exact());
    CHECK(comparison.differing_slots == 1U);
    CHECK(comparison.first_differing_channel == 5U);
    CHECK(comparison.expected == 0U);
    CHECK(comparison.actual == 1U);

    emberlights::MicroPhysicalQualificationEvidence physical{};
    CHECK(emberlights::evaluate_micro_physical_qualification(physical) ==
          emberlights::MicroPhysicalQualificationResult::SoftwareFrameMismatch);
    physical.software_frame_match = true;
    CHECK(emberlights::evaluate_micro_physical_qualification(physical) ==
          emberlights::MicroPhysicalQualificationResult::InitialOpenFailed);
    physical.initial_open_succeeded = true;
    physical.raw_writes_succeeded = true;
    physical.raw_visible_red_and_blackout = true;
    physical.repeat_open_succeeded = true;
    physical.runner_writes_succeeded = true;
    physical.runner_visible_match_and_blackout = true;
    physical.bounded_blackouts_succeeded = true;
    CHECK(emberlights::evaluate_micro_physical_qualification(physical) ==
          emberlights::MicroPhysicalQualificationResult::DisconnectNotObserved);
    physical.disconnect_observed = true;
    physical.reconnect_detected = true;
    physical.reconnect_open_succeeded = true;
    physical.reconnect_writes_succeeded = true;
    physical.reconnect_visible_red_and_blackout = true;
    CHECK(emberlights::evaluate_micro_physical_qualification(physical) ==
          emberlights::MicroPhysicalQualificationResult::ReconnectBlackoutFailed);
    physical.reconnect_blackout_succeeded = true;
    CHECK(emberlights::evaluate_micro_physical_qualification(physical) ==
          emberlights::MicroPhysicalQualificationResult::Passed);
    CHECK(std::string_view(emberlights::micro_physical_qualification_result_name(
              emberlights::MicroPhysicalQualificationResult::Passed)) == "passed");

    if (failures == 0) {
        std::cout << "SoundSwitch Micro session tests passed\n";
        return 0;
    }
    return 1;
}
