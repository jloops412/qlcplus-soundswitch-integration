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
    result.static_look = status.static_look;
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

const char* static_look_owner_kind_name(StaticLookOwnerKind kind) noexcept {
    switch (kind) {
    case StaticLookOwnerKind::None: return "none";
    case StaticLookOwnerKind::Ui: return "ui";
    case StaticLookOwnerKind::Keyboard: return "keyboard";
    case StaticLookOwnerKind::Midi: return "midi";
    case StaticLookOwnerKind::Controller: return "controller";
    case StaticLookOwnerKind::Moment: return "moment";
    case StaticLookOwnerKind::External: return "external";
    case StaticLookOwnerKind::Test: return "test";
    }
    return "none";
}

const char* static_look_behavior_name(StaticLookBehavior behavior) noexcept {
    switch (behavior) {
    case StaticLookBehavior::None: return "none";
    case StaticLookBehavior::Latch: return "latch";
    case StaticLookBehavior::Hold: return "hold";
    case StaticLookBehavior::Explicit: return "explicit";
    }
    return "none";
}

const char* static_look_activation_status_name(
    StaticLookActivationStatus status) noexcept {
    switch (status) {
    case StaticLookActivationStatus::None: return "none";
    case StaticLookActivationStatus::Activating: return "activating";
    case StaticLookActivationStatus::Active: return "active";
    case StaticLookActivationStatus::Releasing: return "releasing";
    }
    return "none";
}

}  // namespace emberlights
