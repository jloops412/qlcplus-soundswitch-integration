#include "showcore/soundswitch_micro.hpp"

#include <chrono>
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

    if (failures == 0) {
        std::cout << "SoundSwitch Micro session tests passed\n";
        return 0;
    }
    return 1;
}
