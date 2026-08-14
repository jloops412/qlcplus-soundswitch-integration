#include "emberlights/fixture_profile_ids.hpp"
#include "emberlights/static_look_authoring.hpp"
#include "emberlights/static_look_preview_coordinator.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

namespace {

int failures = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            std::cerr << "CHECK failed at " << __FILE__ << ':' << __LINE__     \
                      << ": " #condition << '\n';                              \
            ++failures;                                                        \
        }                                                                       \
    } while (false)

[[nodiscard]] emberlights::ProjectDocument make_project(bool output) {
    auto project = emberlights::make_starter_project();
    project.id = "preview-coordinator-test";
    project.name = "Preview Coordinator Test";
    project.connections.artnet_enabled = output;
    project.connections.artnet_destination = "127.0.0.1";
    project.fixtures.push_back({
        "fixture.ir4",
        "IR-4",
        std::string(emberlights::kBothLightingBoIr4SixChannelProfileId),
        1U,
        1U,
        {"wash"}});
    project.groups.push_back({
        "group.wash", "Wash", {"fixture.ir4"}});
    return project;
}

[[nodiscard]] emberlights::StaticLookDraft make_draft(
    const emberlights::ProjectDocument& project,
    float red,
    float green) {
    auto draft = emberlights::make_static_look_draft(
        1U, "look.preview", "Preview");
    CHECK(emberlights::apply_static_look_color(
        draft,
        project,
        "group.wash",
        {red, green, 0.0F, 0.0F, 0.0F, 0.0F, 0.8F}));
    return draft;
}

template<typename Predicate>
[[nodiscard]] bool wait_for(Predicate predicate, std::uint32_t timeout_ms = 4'000U) {
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return predicate();
}

void test_simulation_is_asynchronous_and_coalesces_updates() {
    const auto project = make_project(false);
    auto draft = make_draft(project, 1.0F, 0.0F);
    emberlights::RunnerService runner;
    emberlights::StaticLookPreviewCoordinator coordinator(runner, false);

    const auto began_at = std::chrono::steady_clock::now();
    CHECK(coordinator.start(
        project,
        draft,
        "group.wash",
        emberlights::StaticLookPreviewMode::Simulation) ==
        emberlights::StaticLookPreviewRequestResult::Accepted);
    const auto accepted_in = std::chrono::steady_clock::now() - began_at;
    CHECK(accepted_in < std::chrono::milliseconds(100));
    CHECK(wait_for([&] {
        return coordinator.status().state ==
            emberlights::StaticLookPreviewCoordinatorState::Active;
    }));
    const auto first = coordinator.status();
    CHECK(first.mode == emberlights::StaticLookPreviewMode::Simulation);
    CHECK(first.error == "none");
    CHECK(first.selected_fixture_count == 1U);
    CHECK(!first.frame_sha256.empty());
    CHECK(first.output_cap == 0.0F);
    CHECK(runner.status().state == emberlights::RunnerState::Stopped);

    draft = make_draft(project, 0.0F, 1.0F);
    CHECK(coordinator.update(project, draft, "group.wash") ==
        emberlights::StaticLookPreviewRequestResult::Accepted);
    draft = make_draft(project, 0.0F, 0.5F);
    CHECK(coordinator.update(project, draft, "group.wash") ==
        emberlights::StaticLookPreviewRequestResult::Accepted);
    draft = make_draft(project, 0.0F, 0.0F);
    CHECK(coordinator.update(project, draft, "group.wash") ==
        emberlights::StaticLookPreviewRequestResult::Accepted);
    const auto expected = emberlights::preview_static_look_draft(
        project, draft);
    CHECK(expected);
    CHECK(wait_for([&] {
        const auto status = coordinator.status();
        return status.state ==
                emberlights::StaticLookPreviewCoordinatorState::Active &&
            status.frame_sha256 == expected.frame_sha256;
    }));
    const auto updated = coordinator.status();
    CHECK(updated.frame_sha256 != first.frame_sha256);
    CHECK(updated.update_count >= 1U);
    CHECK(updated.update_count <= 3U);

    CHECK(coordinator.stop() ==
        emberlights::StaticLookPreviewRequestResult::Accepted);
    CHECK(wait_for([&] {
        return coordinator.status().state ==
            emberlights::StaticLookPreviewCoordinatorState::Stopped;
    }));
    CHECK(coordinator.status().mode == emberlights::StaticLookPreviewMode::None);
}

void test_physical_preview_requires_explicit_host_and_output() {
    const auto no_output = make_project(false);
    const auto draft = make_draft(no_output, 1.0F, 0.0F);
    emberlights::RunnerService disabled_runner;
    emberlights::StaticLookPreviewCoordinator disabled(disabled_runner, false);
    CHECK(!disabled.physical_available(no_output));
    CHECK(disabled.start(
        no_output,
        draft,
        "group.wash",
        emberlights::StaticLookPreviewMode::Physical) ==
        emberlights::StaticLookPreviewRequestResult::Unavailable);

    emberlights::RunnerService armed_runner;
    emberlights::StaticLookPreviewCoordinator armed(armed_runner, true);
    CHECK(!armed.physical_available(no_output));
    CHECK(armed.start(
        no_output,
        draft,
        "group.wash",
        emberlights::StaticLookPreviewMode::Physical) ==
        emberlights::StaticLookPreviewRequestResult::Unavailable);
}

void test_physical_command_returns_before_runner_work_and_stops_to_black() {
    const auto project = make_project(true);
    const auto draft = make_draft(project, 1.0F, 0.0F);
    emberlights::RunnerService runner;
    emberlights::StaticLookPreviewCoordinator coordinator(runner, true);
    emberlights::StaticLookPhysicalPreviewConfig config;
    config.timeout_ms = 1'000U;
    config.runner_start_timeout_ms = 2'000U;
    config.activation_timeout_ms = 1'000U;
    config.output_cap = 0.25F;

    const auto began_at = std::chrono::steady_clock::now();
    CHECK(coordinator.start(
        project,
        draft,
        "group.wash",
        emberlights::StaticLookPreviewMode::Physical,
        config) == emberlights::StaticLookPreviewRequestResult::Accepted);
    CHECK(std::chrono::steady_clock::now() - began_at <
        std::chrono::milliseconds(100));
    CHECK(wait_for([&] {
        return coordinator.status().state ==
            emberlights::StaticLookPreviewCoordinatorState::Active;
    }));
    const auto active = coordinator.status();
    CHECK(active.mode == emberlights::StaticLookPreviewMode::Physical);
    CHECK(active.output_cap == 0.25F);
    CHECK(active.remaining_ms > 0U);
    CHECK(active.selected_fixture_count == 1U);
    CHECK(runner.status().state == emberlights::RunnerState::Running);

    CHECK(coordinator.stop() ==
        emberlights::StaticLookPreviewRequestResult::Accepted);
    CHECK(wait_for([&] {
        return coordinator.status().state ==
            emberlights::StaticLookPreviewCoordinatorState::Stopped;
    }));
    CHECK(runner.status().state == emberlights::RunnerState::Stopped);
    CHECK(runner.status().blackout);
}

void test_physical_timeout_blacks_out_and_can_restart_cleanly() {
    const auto project = make_project(true);
    const auto draft = make_draft(project, 1.0F, 0.0F);
    emberlights::RunnerService runner;
    emberlights::StaticLookPreviewCoordinator coordinator(runner, true);
    emberlights::StaticLookPhysicalPreviewConfig config;
    config.timeout_ms = 1'000U;
    config.runner_start_timeout_ms = 2'000U;
    config.activation_timeout_ms = 1'000U;

    CHECK(coordinator.start(
        project,
        draft,
        "group.wash",
        emberlights::StaticLookPreviewMode::Physical,
        config) == emberlights::StaticLookPreviewRequestResult::Accepted);
    CHECK(wait_for([&] {
        return coordinator.status().state ==
            emberlights::StaticLookPreviewCoordinatorState::Active;
    }));
    CHECK(wait_for([&] {
        return coordinator.status().state ==
            emberlights::StaticLookPreviewCoordinatorState::TimedOut;
    }));
    CHECK(runner.status().state == emberlights::RunnerState::Stopped);
    CHECK(runner.status().blackout);

    config.timeout_ms = 999U;
    CHECK(coordinator.start(
        project,
        draft,
        "group.wash",
        emberlights::StaticLookPreviewMode::Physical,
        config) == emberlights::StaticLookPreviewRequestResult::Accepted);
    CHECK(wait_for([&] {
        const auto status = coordinator.status();
        return status.state ==
                emberlights::StaticLookPreviewCoordinatorState::Fault &&
            status.error == "invalid-configuration";
    }));

    config.timeout_ms = 2'000U;
    CHECK(coordinator.start(
        project,
        draft,
        "group.wash",
        emberlights::StaticLookPreviewMode::Physical,
        config) == emberlights::StaticLookPreviewRequestResult::Accepted);
    const auto restarted = coordinator.status();
    CHECK(restarted.state ==
              emberlights::StaticLookPreviewCoordinatorState::Starting ||
          restarted.state ==
              emberlights::StaticLookPreviewCoordinatorState::Active);
    CHECK(wait_for([&] {
        return coordinator.status().state ==
            emberlights::StaticLookPreviewCoordinatorState::Active;
    }));
    CHECK(coordinator.stop() ==
        emberlights::StaticLookPreviewRequestResult::Accepted);
    CHECK(wait_for([&] {
        return coordinator.status().state ==
            emberlights::StaticLookPreviewCoordinatorState::Stopped;
    }));
}

}  // namespace

int main() {
    test_simulation_is_asynchronous_and_coalesces_updates();
    test_physical_preview_requires_explicit_host_and_output();
    test_physical_command_returns_before_runner_work_and_stops_to_black();
    test_physical_timeout_blacks_out_and_can_restart_cleanly();
    if (failures != 0) {
        std::cerr << failures << " Static Look preview coordinator test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "Static Look preview coordinator tests passed\n";
    return EXIT_SUCCESS;
}
