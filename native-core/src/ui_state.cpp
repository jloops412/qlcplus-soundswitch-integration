#include "emberlights/ui_state.hpp"

namespace emberlights {

LiveCoreUiState make_live_core_ui_state(const RunnerStatus& status) noexcept {
    LiveCoreUiState result;
    result.generation = status.frames;
    result.runner = status.state;
    result.sync = status.sync_state;
    result.bpm = status.bpm;
    result.blackout = status.blackout;
    result.work_light = status.work_light;
    result.fog_armed = status.fog_armed;
    result.haze_armed = status.haze_armed;
    result.laser_armed = status.laser_armed;
    result.spark_armed = status.spark_armed;
    result.active_override_count = status.manual_override_count;
    result.control_one = status.soundswitch_control_one;
    return result;
}

}  // namespace emberlights
