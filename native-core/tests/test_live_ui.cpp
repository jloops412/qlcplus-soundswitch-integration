#include "emberlights/compiler.hpp"
#include "emberlights/live_view_model.hpp"
#include "emberlights/project.hpp"
#include "emberlights/ui_command.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <thread>
#include <utility>

namespace {

int failures = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            std::cerr << "CHECK failed at " << __FILE__ << ':' << __LINE__    \
                      << ": " #condition << '\n';                            \
            ++failures;                                                         \
        }                                                                       \
    } while (false)

class TestHost final : public emberlights::UiAppCommandHost {
public:
    emberlights::UiInvocationResult ui_start_show() noexcept override {
        return emberlights::UiInvocationResult::Accepted;
    }
    emberlights::UiInvocationResult ui_stop_show() noexcept override {
        return emberlights::UiInvocationResult::Accepted;
    }
};

[[nodiscard]] emberlights::ProjectDocument make_live_project() {
    auto project = emberlights::make_starter_project();
    project.id = "live-ui-test";
    project.name = "Live UI Test";
    project.connections.os2l_enabled = false;
    project.connections.artnet_enabled = false;
    project.connections.sacn_enabled = false;
    project.connections.dmx_usb_pro_ports = {};
    project.connections.soundswitch_micro_universe = 0U;
    project.connections.soundswitch_control_one_experimental = false;
    project.connections.midi_input_index = -1;
    project.connections.midi_output_index = -1;
    project.safety.strobe_allowed = false;

    emberlights::FixtureProfileDefinition strobe_profile;
    strobe_profile.id = "profile.strobe";
    strobe_profile.manufacturer = "Test";
    strobe_profile.model = "Strobe";
    strobe_profile.mode = "2 channel";
    strobe_profile.name = "Test Strobe";
    strobe_profile.footprint = 2U;
    strobe_profile.channels.push_back({
        showcore::Property::Intensity,
        0U,
        -1,
        showcore::ChannelEncoding::Linear8,
        0U,
        255U,
        0U});
    strobe_profile.channels.push_back({
        showcore::Property::Strobe,
        1U,
        -1,
        showcore::ChannelEncoding::Ranged8,
        8U,
        255U,
        0U});
    project.fixture_profiles.push_back(std::move(strobe_profile));

    project.fixtures.push_back({
        "fixture.rgb",
        "RGB Fixture",
        "builtin.generic.rgbd-4ch",
        1U,
        1U,
        {"dance"}});
    project.fixtures.push_back({
        "fixture.dimmer",
        "Dimmer Fixture",
        "builtin.generic.dimmer-1ch",
        1U,
        10U,
        {"dance"}});
    project.fixtures.push_back({
        "fixture.strobe",
        "Strobe Fixture",
        "profile.strobe",
        1U,
        20U,
        {"effect"}});
    project.groups.push_back({
        "group.mixed",
        "Mixed Group",
        {"fixture.rgb", "fixture.dimmer"}});

    emberlights::LookDefinition look;
    look.id = "look.blue";
    look.name = "Blue Look";
    look.fade_ms = 0U;
    look.assignments.push_back({
        "fixture.rgb",
        showcore::Property::Blue,
        showcore::PropertyValue::set(1.0F)});
    project.looks.push_back(std::move(look));

    emberlights::AutoloopDefinition loop;
    loop.id = "loop.blue";
    loop.name = "Blue Loop";
    loop.bank = 2U;
    loop.slot = 5U;
    loop.length_beats = 4.0F;
    loop.repeat = showcore::AutoloopRepeat::Infinite;
    loop.steps.push_back({0.0F, "look.blue", showcore::AutoloopTransition::Cut});
    project.autoloops.push_back(std::move(loop));

    emberlights::TrackScriptDefinition script;
    script.id = "track.script";
    script.name = "Track Script";
    script.cues.push_back({
        0.0F,
        emberlights::TrackCueAction::TriggerLook,
        "look.blue"});
    project.track_scripts.push_back(std::move(script));
    return project;
}

template <typename Predicate>
[[nodiscard]] bool wait_until(Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!predicate() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return predicate();
}

[[nodiscard]] emberlights::UiCommandInvocation target_command(
    emberlights::UiCommandId command,
    std::string_view target) {
    emberlights::UiCommandInvocation invocation;
    invocation.command = command;
    invocation.target_id = target;
    return invocation;
}

void test_live_view_model(const emberlights::ProjectDocument& project) {
    emberlights::LiveViewModel view;
    view.load_project(project);
    CHECK(view.project_id() == project.id);
    CHECK(view.project_name() == project.name);
    CHECK(!view.safety().strobe_allowed);
    CHECK(view.static_looks().size() == 1U);
    CHECK(view.track_scripts().size() == 1U);
    CHECK(view.override_targets().size() == 4U);

    const auto group = std::find_if(
        view.override_targets().begin(), view.override_targets().end(),
        [](const auto& target) { return target.id == "group.mixed"; });
    CHECK(group != view.override_targets().end());
    if (group != view.override_targets().end()) {
        CHECK(group->kind == emberlights::LiveOverrideTargetKind::Group);
        CHECK(group->complete);
        CHECK(group->fixture_count == 2U);
        CHECK(group->supports_any(showcore::Property::Red));
        CHECK(!group->supports_all(showcore::Property::Red));
        CHECK(group->supports_all(showcore::Property::Intensity));
        CHECK(!group->supports_any(showcore::Property::Zoom));
    }

    CHECK(view.select_autoloop_bank(2U));
    CHECK(view.select_autoloop_slot(5U));
    emberlights::RunnerStatus status;
    status.state = emberlights::RunnerState::Running;
    status.package_generation = 42U;
    status.frames = 99U;
    status.active_look = 0;
    status.active_autoloop = {2U, 5U};
    status.active_autoloop_repeat = showcore::AutoloopRepeat::Infinite;
    status.active_autoloop_progress = 0.5F;
    status.active_autoloop_completed_cycles = 3U;
    status.active_autoloop_bank_mask = std::uint64_t{1} << 2U;
    status.active_track_script = 0;
    status.active_track_script_beat = 12.5;
    status.active_track_script_consumed_cues = 1U;
    status.manual_override_count = 2U;
    view.update(status);

    CHECK(view.state().generation == 42U);
    CHECK(view.state().frames == 99U);
    CHECK(view.static_looks()[0].active);
    CHECK(view.track_scripts()[0].active);
    const auto& pad = view.autoloop_pads()[5U];
    CHECK(pad.populated);
    CHECK(pad.selected);
    CHECK(pad.active);
    CHECK(std::abs(pad.progress - 0.5F) < 0.001F);
    CHECK(pad.id == "loop.blue");
    CHECK(view.autoloop_bank_window()[2U].selected);
    CHECK(view.autoloop_bank_window()[2U].contains_active);
    CHECK(view.autoloop_bank_window()[2U].enabled_by_filter);
    CHECK(view.active_content().static_look_id == "look.blue");
    CHECK(view.active_content().autoloop_id == "loop.blue");
    CHECK(view.active_content().track_script_id == "track.script");

    CHECK(view.select_autoloop_page(1U));
    CHECK(view.selected_autoloop_bank() == 6U);
    CHECK(view.autoloop_page() == 1U);
    CHECK((view.autoloop_pads()[5U].address ==
           showcore::AutoloopAddress{6U, 5U}));
    CHECK(!view.autoloop_pads()[5U].active);
    CHECK(view.active_content().autoloop_id == "loop.blue");
    CHECK(!view.select_autoloop_bank(
        static_cast<std::uint16_t>(showcore::kMaxAutoloopBanks)));
    CHECK(!view.select_autoloop_slot(
        static_cast<std::uint8_t>(showcore::kAutoloopsPerBank)));
}

void test_live_commands(emberlights::ProjectDocument project) {
    const auto validation = emberlights::validate_project(project);
    CHECK(validation.ok());
    auto compilation = emberlights::compile_project(project);
    CHECK(compilation);
    if (!compilation) {
        return;
    }

    emberlights::RunnerService runner;
    CHECK(runner.start(std::move(compilation.show), project));
    CHECK(wait_until([&]() {
        return runner.status().state == emberlights::RunnerState::Running;
    }));

    TestHost host;
    emberlights::UiCommandFacade facade(runner, host);
    facade.set_active_project(&project);

    auto invocation = target_command(
        emberlights::UiCommandId::StaticLookActivate, "look.blue");
    CHECK(facade.invoke(invocation) == emberlights::UiInvocationResult::Accepted);
    CHECK(wait_until([&]() { return runner.status().active_look == 0; }));

    invocation.command = emberlights::UiCommandId::StaticLookToggle;
    CHECK(facade.invoke(invocation) == emberlights::UiInvocationResult::Accepted);
    CHECK(wait_until([&]() { return runner.status().active_look < 0; }));
    invocation.command = emberlights::UiCommandId::StaticLookActivate;
    CHECK(facade.invoke(invocation) == emberlights::UiInvocationResult::Accepted);
    CHECK(wait_until([&]() { return runner.status().active_look == 0; }));

    invocation = target_command(
        emberlights::UiCommandId::StaticLookActivate, "look.missing");
    CHECK(facade.invoke(invocation) == emberlights::UiInvocationResult::NotFound);

    invocation = target_command(
        emberlights::UiCommandId::AutoloopLaunch, "loop.blue");
    CHECK(facade.invoke(invocation) == emberlights::UiInvocationResult::Accepted);
    CHECK(wait_until([&]() {
        return runner.status().active_autoloop == showcore::AutoloopAddress{2U, 5U};
    }));

    invocation = {};
    invocation.command = emberlights::UiCommandId::AutoloopBankFilterSelectExclusive;
    invocation.bank = 2U;
    CHECK(facade.invoke(invocation) == emberlights::UiInvocationResult::Accepted);
    CHECK(wait_until([&]() {
        return runner.status().active_autoloop_bank_mask == (std::uint64_t{1} << 2U);
    }));

    invocation = target_command(
        emberlights::UiCommandId::TrackScriptStart, "track.script");
    CHECK(facade.invoke(invocation) == emberlights::UiInvocationResult::Accepted);
    CHECK(wait_until([&]() { return runner.status().active_track_script == 0; }));

    invocation = target_command(
        emberlights::UiCommandId::FixtureOverridePropertySet, "fixture.rgb");
    invocation.property = showcore::Property::Red;
    invocation.number_value = 0.4;
    CHECK(facade.invoke(invocation) == emberlights::UiInvocationResult::Accepted);
    CHECK(wait_until([&]() { return runner.status().manual_override_count == 1U; }));

    invocation.command = emberlights::UiCommandId::FixtureOverridePropertyRelease;
    CHECK(facade.invoke(invocation) == emberlights::UiInvocationResult::Accepted);
    CHECK(wait_until([&]() { return runner.status().manual_override_count == 0U; }));

    invocation = target_command(
        emberlights::UiCommandId::GroupOverridePropertySet, "group.mixed");
    invocation.property = showcore::Property::Red;
    invocation.number_value = 0.75;
    CHECK(facade.invoke(invocation) == emberlights::UiInvocationResult::Accepted);
    CHECK(wait_until([&]() { return runner.status().manual_override_count == 1U; }));

    invocation.command = emberlights::UiCommandId::GroupOverridePropertyRelease;
    CHECK(facade.invoke(invocation) == emberlights::UiInvocationResult::Accepted);
    CHECK(wait_until([&]() { return runner.status().manual_override_count == 0U; }));

    invocation = target_command(
        emberlights::UiCommandId::FixtureOverridePropertySet, "fixture.dimmer");
    invocation.property = showcore::Property::Red;
    invocation.number_value = 1.0;
    CHECK(facade.invoke(invocation) == emberlights::UiInvocationResult::Unsupported);
    invocation.property = showcore::Property::Intensity;
    invocation.number_value = 1.5;
    CHECK(facade.invoke(invocation) ==
          emberlights::UiInvocationResult::InvalidArguments);

    invocation = target_command(
        emberlights::UiCommandId::FixtureOverridePropertySet, "fixture.strobe");
    invocation.property = showcore::Property::Strobe;
    invocation.number_value = 0.5;
    CHECK(facade.invoke(invocation) ==
          emberlights::UiInvocationResult::SafetyRejected);

    const auto frames_before_clear = runner.status().frames;
    CHECK(facade.invoke({emberlights::UiCommandId::AutoloopClear}) ==
          emberlights::UiInvocationResult::Accepted);
    CHECK(wait_until([&]() { return runner.status().frames > frames_before_clear; }));
    CHECK(facade.invoke({emberlights::UiCommandId::StaticLookClear}) ==
          emberlights::UiInvocationResult::Accepted);
    CHECK(wait_until([&]() { return runner.status().active_look < 0; }));
    CHECK(facade.invoke({emberlights::UiCommandId::TrackScriptClear}) ==
          emberlights::UiInvocationResult::Accepted);
    CHECK(wait_until([&]() { return runner.status().active_track_script < 0; }));

    runner.stop();
    invocation = target_command(
        emberlights::UiCommandId::StaticLookActivate, "look.blue");
    CHECK(facade.invoke(invocation) == emberlights::UiInvocationResult::Unavailable);
}

}  // namespace

int main() {
    const auto project = make_live_project();
    test_live_view_model(project);
    test_live_commands(project);

    CHECK(std::string_view(emberlights::ui_invocation_result_name(
              emberlights::UiInvocationResult::NotFound)) == "notFound");
    CHECK(emberlights::find_ui_command("staticLook.toggle") != nullptr);
    CHECK(emberlights::find_ui_command("autoloop.launch") != nullptr);
    CHECK(emberlights::find_ui_command("group.override.property.release") != nullptr);

    if (failures != 0) {
        return EXIT_FAILURE;
    }
    std::cout << "Live UI facade/view-model tests passed\n";
    return EXIT_SUCCESS;
}
