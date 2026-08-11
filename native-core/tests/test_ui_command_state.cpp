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

    int starts{0};
    int stops{0};
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

    const auto state = emberlights::make_live_core_ui_state(runner.status());
    CHECK(state.runner == emberlights::RunnerState::Stopped);
    CHECK(state.blackout);
    CHECK(state.control_one == emberlights::AdapterState::Disabled);

    CHECK(std::string_view(emberlights::ui_invocation_result_name(
              emberlights::UiInvocationResult::QueueFull)) == "queueFull");

    if (failures != 0) {
        return EXIT_FAILURE;
    }
    std::cout << "UI command/state facade tests passed\n";
    return EXIT_SUCCESS;
}
