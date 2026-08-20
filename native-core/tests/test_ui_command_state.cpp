#include "emberlights/ui_command.hpp"
#include "emberlights/ui_state.hpp"

#include <cstdlib>
#include <iostream>
#include <set>
#include <string>

namespace {

int failures = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            std::cerr << "CHECK failed at " << __FILE__ << ':' << __LINE__     \
                      << ": " #condition << '\n';                             \
            ++failures;                                                        \
        }                                                                       \
    } while (false)

class TestHost final : public emberlights::UiAppCommandHost {
public:
    emberlights::UiInvocationResult ui_start_show() noexcept override {
        ++starts;
        return emberlights::UiInvocationResult::Accepted;
    }
    emberlights::UiInvocationResult ui_stop_show() noexcept override {
        ++stops;
        return emberlights::UiInvocationResult::Accepted;
    }
    emberlights::UiInvocationResult ui_start_static_look_preview(
        std::string_view look_id,
        std::string_view target_id,
        emberlights::UiStaticLookPreviewMode mode) noexcept override {
        ++preview_starts;
        preview_look_id = std::string(look_id);
        preview_target_id = std::string(target_id);
        preview_mode = mode;
        return emberlights::UiInvocationResult::Accepted;
    }
    emberlights::UiInvocationResult ui_stop_static_look_preview() noexcept override {
        ++preview_stops;
        return emberlights::UiInvocationResult::Accepted;
    }

    int starts{0};
    int stops{0};
    int preview_starts{0};
    int preview_stops{0};
    std::string preview_look_id;
    std::string preview_target_id;
    emberlights::UiStaticLookPreviewMode preview_mode{
        emberlights::UiStaticLookPreviewMode::None};
};

}  // namespace

int main() {
    std::set<std::string> ids;
    for (const auto& definition : emberlights::kUiCommandDefinitions) {
        CHECK(!definition.id.empty());
        CHECK(ids.insert(std::string(definition.id)).second);
        CHECK(emberlights::find_ui_command(definition.id) == &definition);
    }
    CHECK(emberlights::find_ui_command("missing.command") == nullptr);

    emberlights::RunnerService runner;
    TestHost host;
    emberlights::UiCommandFacade facade(runner, host);

    CHECK(facade.invoke({emberlights::UiCommandId::ShowStart}) ==
          emberlights::UiInvocationResult::Accepted);
    CHECK(host.starts == 1);
    CHECK(facade.invoke({emberlights::UiCommandId::ShowStop}) ==
          emberlights::UiInvocationResult::NoChange);

    emberlights::UiCommandInvocation invalid_bpm;
    invalid_bpm.command = emberlights::UiCommandId::ManualBpmSet;
    invalid_bpm.number_value = 19.0;
    CHECK(facade.invoke(invalid_bpm) ==
          emberlights::UiInvocationResult::InvalidArguments);
    invalid_bpm.number_value = 120.0;
    CHECK(facade.invoke(invalid_bpm) ==
          emberlights::UiInvocationResult::Unavailable);

    CHECK(facade.invoke({emberlights::UiCommandId::WorkLightToggle}) ==
          emberlights::UiInvocationResult::Unavailable);
    CHECK(facade.invoke({emberlights::UiCommandId::BlackoutToggle}) ==
          emberlights::UiInvocationResult::Accepted);
    CHECK(runner.status().blackout);
    emberlights::UiCommandInvocation blackout_set;
    blackout_set.command = emberlights::UiCommandId::BlackoutSet;
    blackout_set.bool_value = true;
    CHECK(facade.invoke(blackout_set) ==
          emberlights::UiInvocationResult::NoChange);

    emberlights::UiCommandInvocation preview;
    preview.command = emberlights::UiCommandId::StaticLookPreviewStart;
    CHECK(facade.invoke(preview) ==
          emberlights::UiInvocationResult::InvalidArguments);
    preview.target_id = "look.preview";
    preview.secondary_target_id = "group.wash";
    preview.static_look_preview_mode =
        emberlights::UiStaticLookPreviewMode::Simulation;
    CHECK(facade.invoke(preview) ==
          emberlights::UiInvocationResult::Accepted);
    CHECK(host.preview_starts == 1);
    CHECK(host.preview_look_id == "look.preview");
    CHECK(host.preview_target_id == "group.wash");
    CHECK(host.preview_mode ==
          emberlights::UiStaticLookPreviewMode::Simulation);
    CHECK(facade.invoke({emberlights::UiCommandId::StaticLookPreviewStop}) ==
          emberlights::UiInvocationResult::Accepted);
    CHECK(host.preview_stops == 1);

    const auto* preview_definition = emberlights::find_ui_command(
        "staticLook.preview.start");
    CHECK(preview_definition != nullptr);
    if (preview_definition != nullptr) {
        CHECK(!preview_definition->midi_bindable);
        CHECK(!preview_definition->keyboard_bindable);
        CHECK(!preview_definition->action_bindable);
    }

    const auto state = emberlights::make_live_core_ui_state(runner.status());
    CHECK(state.runner == emberlights::RunnerState::Stopped);
    CHECK(state.blackout);
    CHECK(state.control_one == emberlights::AdapterState::Disabled);
    CHECK(state.static_look.status ==
          emberlights::StaticLookActivationStatus::None);

    CHECK(std::string_view(emberlights::static_look_owner_kind_name(
              emberlights::StaticLookOwnerKind::Controller)) == "controller");
    CHECK(std::string_view(emberlights::static_look_behavior_name(
              emberlights::StaticLookBehavior::Hold)) == "hold");
    CHECK(std::string_view(emberlights::static_look_activation_status_name(
              emberlights::StaticLookActivationStatus::Releasing)) ==
          "releasing");

    CHECK(std::string_view(emberlights::ui_invocation_result_name(
              emberlights::UiInvocationResult::QueueFull)) == "queueFull");

    if (failures != 0) {
        return EXIT_FAILURE;
    }
    std::cout << "UI command/state facade tests passed\n";
    return EXIT_SUCCESS;
}
