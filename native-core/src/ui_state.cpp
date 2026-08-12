#include "emberlights/ui_state.hpp"

namespace emberlights {

LiveCoreUiState make_live_core_ui_state(const RunnerStatus& status) noexcept {
    LiveCoreUiState result;
    result.generation = status.package_generation;
    result.frames = status.frames;
    result.runner = status.state;
    result.os2l = status.os2l;
    result.os2l_discovery = status.os2l_discovery;
    result.midi_input = status.midi_input;
    result.midi_output = status.midi_output;
    result.artnet = status.artnet;
    result.sacn = status.sacn;
    result.dmx_usb_pro = status.dmx_usb_pro;
    result.soundswitch_micro = status.soundswitch_micro;
    result.output_backends = status.output_backends;
    result.sync = status.sync_state;
    result.clock_source = status.clock_source;
    result.bpm = status.bpm;
    result.beat_position = status.beat_position;
    result.active_look = status.active_look;
    result.active_autoloop = status.active_autoloop;
    result.active_autoloop_repeat = status.active_autoloop_repeat;
    result.active_autoloop_progress = status.active_autoloop_progress;
    result.active_autoloop_completed_cycles = status.active_autoloop_completed_cycles;
    result.active_autoloop_bank_mask = status.active_autoloop_bank_mask;
    result.active_track_script = status.active_track_script;
    result.active_track_script_beat = status.active_track_script_beat;
    result.active_track_script_consumed_cues =
        status.active_track_script_consumed_cues;
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
