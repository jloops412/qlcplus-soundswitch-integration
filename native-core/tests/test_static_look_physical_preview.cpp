#include "emberlights/compiler.hpp"
#include "emberlights/fixture_profile_ids.hpp"
#include "emberlights/static_look_authoring.hpp"
#include "emberlights/static_look_physical_preview.hpp"
#include "emberlights/static_look_preview.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
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

[[nodiscard]] emberlights::ProjectDocument make_project(bool output = true) {
    auto project = emberlights::make_starter_project();
    project.id = "physical-preview-test";
    project.name = "Physical Preview Test";
    project.connections.os2l_enabled = true;
    project.connections.midi_input_index = 3;
    project.connections.midi_output_index = 4;
    project.connections.artnet_enabled = output;
    project.connections.artnet_destination = "127.0.0.1";
    project.fixtures.push_back({
        "ir4-6",
        "IR-4 Six",
        std::string(emberlights::kBothLightingBoIr4SixChannelProfileId),
        1U,
        1U,
        {"wash"}});
    project.fixtures.push_back({
        "ir4-10",
        "IR-4 Ten",
        std::string(emberlights::kBothLightingBoIr4TenChannelProfileId),
        1U,
        20U,
        {"wash"}});
    project.groups.push_back({
        "pair", "IR-4 Pair", {"ir4-6", "ir4-10"}});
    return project;
}

[[nodiscard]] emberlights::StaticLookDraft make_red_pair(
    const emberlights::ProjectDocument& project) {
    auto draft = emberlights::make_static_look_draft(
        1U, "preview-look", "Preview Red");
    CHECK(emberlights::apply_static_look_color(
        draft,
        project,
        "pair",
        {1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F}));
    return draft;
}

[[nodiscard]] bool wait_running(emberlights::RunnerService& runner) {
    for (std::size_t attempt = 0U; attempt < 1'000U; ++attempt) {
        const auto state = runner.status().state;
        if (state == emberlights::RunnerState::Running) {
            return true;
        }
        if (state == emberlights::RunnerState::Fault ||
            state == emberlights::RunnerState::Stopped) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
}

void test_candidate_isolates_and_caps() {
    auto project = make_project();
    auto draft = make_red_pair(project);

    // Prove that a dangerous nonselected profile default cannot leak into the
    // candidate frame. The candidate must rewrite every released default to 0.
    const auto ten_profile = std::find_if(
        project.fixture_profiles.begin(), project.fixture_profiles.end(),
        [](const auto& profile) {
            return profile.id ==
                emberlights::kBothLightingBoIr4TenChannelProfileId;
        });
    CHECK(ten_profile != project.fixture_profiles.end());
    if (ten_profile != project.fixture_profiles.end()) {
        ten_profile->channels[0].default_value = 255U;
    }

    emberlights::StaticLookPhysicalPreviewConfig config;
    config.output_cap = 0.25F;
    const auto candidate =
        emberlights::build_static_look_physical_preview_candidate(
            project, draft, "ir4-6", config);
    CHECK(candidate);
    CHECK(candidate.selected_fixture_count == 1U);
    CHECK(candidate.retained_assignment_count == 6U);
    CHECK(candidate.stripped_assignment_count == 8U);
    CHECK(!candidate.project.connections.os2l_enabled);
    CHECK(candidate.project.connections.midi_input_index == -1);
    CHECK(candidate.project.connections.midi_output_index == -1);
    CHECK(candidate.project.groups.empty());
    CHECK(candidate.project.autoloops.empty());
    CHECK(candidate.project.track_scripts.empty());
    CHECK(candidate.project.midi_mappings.empty());
    CHECK(!candidate.project.safety.strobe_allowed);
    CHECK(candidate.project.safety.max_strobe == 0.0F);
    CHECK(candidate.project.safety.max_intensity == 0.25F);
    CHECK(candidate.project.looks.size() == 1U);
    if (!candidate.project.looks.empty()) {
        CHECK(candidate.project.looks[0].fade_ms == 0U);
        CHECK(std::all_of(
            candidate.project.looks[0].assignments.begin(),
            candidate.project.looks[0].assignments.end(),
            [](const auto& assignment) {
                return assignment.fixture_id == "ir4-6" &&
                    (assignment.value.mode != showcore::ValueMode::Set ||
                     assignment.value.value <= 0.25F);
            }));
    }
    CHECK(std::all_of(
        candidate.project.fixture_profiles.begin(),
        candidate.project.fixture_profiles.end(),
        [](const auto& profile) {
            return std::all_of(
                profile.channels.begin(), profile.channels.end(),
                [](const auto& channel) { return channel.default_value == 0U; });
        }));

    const auto rendered = emberlights::preview_static_look(
        candidate.project, "preview-look");
    CHECK(rendered);
    CHECK(rendered.frames.universes[0][0] == 64U);
    for (std::size_t slot = 1U; slot < 6U; ++slot) {
        CHECK(rendered.frames.universes[0][slot] == 0U);
    }
    for (std::size_t slot = 19U; slot < 29U; ++slot) {
        CHECK(rendered.frames.universes[0][slot] == 0U);
    }
}

void test_candidate_rejects_unsafe_inputs() {
    auto project = make_project();
    auto draft = make_red_pair(project);
    draft.look.assignments.push_back({
        "ir4-10",
        showcore::Property::Strobe,
        showcore::PropertyValue::set(0.2F)});
    const auto hazard = emberlights::build_static_look_physical_preview_candidate(
        project, draft, "ir4-10");
    CHECK(!hazard);
    CHECK(hazard.error ==
        emberlights::StaticLookPhysicalPreviewError::UnsafeAssignment);

    auto unknown = make_red_pair(project);
    unknown.look.assignments.push_back({
        "ir4-10",
        showcore::Property::Custom1,
        showcore::PropertyValue::set(0.1F)});
    const auto custom = emberlights::build_static_look_physical_preview_candidate(
        project, unknown, "ir4-10");
    CHECK(!custom);
    CHECK(custom.error ==
        emberlights::StaticLookPhysicalPreviewError::UnsafeAssignment);

    auto unsafe_profile = project;
    const auto profile = std::find_if(
        unsafe_profile.fixture_profiles.begin(),
        unsafe_profile.fixture_profiles.end(),
        [](const auto& candidate) {
            return candidate.id ==
                emberlights::kBothLightingBoIr4TenChannelProfileId;
        });
    CHECK(profile != unsafe_profile.fixture_profiles.end());
    if (profile != unsafe_profile.fixture_profiles.end()) {
        profile->channels.back().default_value = 1U;
    }
    const auto constant =
        emberlights::build_static_look_physical_preview_candidate(
            unsafe_profile, make_red_pair(unsafe_profile), "ir4-10");
    CHECK(!constant);
    CHECK(constant.error ==
        emberlights::StaticLookPhysicalPreviewError::UnsafeProfile);

    const auto no_output_project = make_project(false);
    const auto no_output =
        emberlights::build_static_look_physical_preview_candidate(
            no_output_project,
            make_red_pair(no_output_project),
            "ir4-6");
    CHECK(!no_output);
    CHECK(no_output.error ==
        emberlights::StaticLookPhysicalPreviewError::NoOutputConfigured);
}

void test_live_interlock_does_not_steal_runner() {
    auto project = make_project();
    const auto draft = make_red_pair(project);
    project.looks.push_back(draft.look);
    auto compilation = emberlights::compile_project(project);
    CHECK(compilation);
    emberlights::RunnerService runner;
    CHECK(runner.start(std::move(compilation.show), project));
    CHECK(wait_running(runner));
    {
        emberlights::StaticLookPhysicalPreviewService service(runner);
        const auto rejected = service.begin(project, draft, "ir4-6");
        CHECK(!rejected);
        CHECK(rejected.error ==
            emberlights::StaticLookPhysicalPreviewError::LiveRunning);
        CHECK(runner.status().state == emberlights::RunnerState::Running);
    }
    CHECK(runner.status().state == emberlights::RunnerState::Running);
    runner.stop();
}

void test_realtime_update_stop_and_timeout() {
    auto project = make_project();
    auto draft = make_red_pair(project);
    emberlights::RunnerService runner;
    emberlights::StaticLookPhysicalPreviewService service(runner);
    emberlights::StaticLookPhysicalPreviewConfig config;
    config.timeout_ms = 1'000U;
    config.runner_start_timeout_ms = 2'000U;
    config.activation_timeout_ms = 1'000U;
    config.output_cap = 0.25F;

    const auto started = service.begin(project, draft, "ir4-6", config);
    CHECK(started);
    CHECK(started.deadline_ms != 0U);
    CHECK(service.status().state ==
        emberlights::StaticLookPhysicalPreviewState::Active);
    CHECK(runner.status().state == emberlights::RunnerState::Running);

    auto green = emberlights::make_static_look_draft(
        1U, "preview-look", "Preview Green");
    CHECK(emberlights::apply_static_look_color(
        green,
        project,
        "pair",
        {0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F}));
    const auto updated = service.update(project, green, "ir4-6");
    CHECK(updated);
    const auto after_update = service.status();
    CHECK(after_update.state ==
        emberlights::StaticLookPhysicalPreviewState::Active);
    CHECK(after_update.update_count == 1U);
    CHECK(after_update.deadline_ms == started.deadline_ms);

    auto unsafe_update = green;
    unsafe_update.look.assignments.push_back({
        "ir4-6",
        showcore::Property::Custom1,
        showcore::PropertyValue::set(0.1F)});
    const auto rejected = service.update(project, unsafe_update, "ir4-6");
    CHECK(!rejected);
    CHECK(rejected.error ==
        emberlights::StaticLookPhysicalPreviewError::UnsafeAssignment);
    CHECK(service.status().state ==
        emberlights::StaticLookPhysicalPreviewState::Fault);
    CHECK(service.status().stop_reason ==
        emberlights::StaticLookPhysicalPreviewStopReason::RejectedUpdate);
    CHECK(runner.status().state == emberlights::RunnerState::Stopped);

    const auto explicit_session = service.begin(project, draft, "ir4-6", config);
    CHECK(explicit_session);
    CHECK(service.stop());
    CHECK(runner.status().state == emberlights::RunnerState::Stopped);
    CHECK(service.status().stop_reason ==
        emberlights::StaticLookPhysicalPreviewStopReason::Explicit);

    const auto restarted = service.begin(project, draft, "ir4-6", config);
    CHECK(restarted);
    CHECK(service.enforce_deadline(restarted.deadline_ms));
    const auto timed_out = service.status();
    CHECK(timed_out.state ==
        emberlights::StaticLookPhysicalPreviewState::TimedOut);
    CHECK(timed_out.error ==
        emberlights::StaticLookPhysicalPreviewError::TimedOut);
    CHECK(timed_out.stop_reason ==
        emberlights::StaticLookPhysicalPreviewStopReason::Timeout);
    CHECK(!timed_out.owns_runner);
    CHECK(runner.status().state == emberlights::RunnerState::Stopped);
}

void test_destruction_stops_owned_preview() {
    auto project = make_project();
    const auto draft = make_red_pair(project);
    emberlights::RunnerService runner;
    {
        emberlights::StaticLookPhysicalPreviewService service(runner);
        emberlights::StaticLookPhysicalPreviewConfig config;
        config.timeout_ms = 1'000U;
        config.runner_start_timeout_ms = 2'000U;
        config.activation_timeout_ms = 1'000U;
        CHECK(service.begin(project, draft, "ir4-6", config));
        CHECK(runner.status().state == emberlights::RunnerState::Running);
    }
    CHECK(runner.status().state == emberlights::RunnerState::Stopped);
}

}  // namespace

int main() {
    test_candidate_isolates_and_caps();
    test_candidate_rejects_unsafe_inputs();
    test_live_interlock_does_not_steal_runner();
    test_realtime_update_stop_and_timeout();
    test_destruction_stops_owned_preview();
    if (failures != 0) {
        std::cerr << failures << " static-look physical preview test(s) failed\n";
        return 1;
    }
    std::cout << "Static Look physical preview tests passed\n";
    return 0;
}
