#include "emberlights/autoloop_autoscript_workflow.hpp"

#include "emberlights/compiler.hpp"
#include "emberlights/project_io.hpp"
#include "emberlights/runner.hpp"
#include "emberlights/ui_command.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
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

class TestHost final : public emberlights::UiAppCommandHost {
public:
    emberlights::UiInvocationResult ui_start_show() noexcept override {
        return emberlights::UiInvocationResult::Unsupported;
    }
    emberlights::UiInvocationResult ui_stop_show() noexcept override {
        return emberlights::UiInvocationResult::Unsupported;
    }
};

[[nodiscard]] emberlights::ProjectDocument make_project() {
    auto project = emberlights::make_starter_project();
    project.id = "autoscript-workflow-project";
    project.name = "AutoScript Workflow Project";
    project.connections.os2l_enabled = false;
    project.fixtures.push_back({
        "workflow-rgbd",
        "Workflow RGBD",
        "builtin.generic.rgbd-4ch",
        1U,
        1U,
        {"wash"}});
    return project;
}

[[nodiscard]] emberlights::AutoloopAutoscriptRequest request_at(
    showcore::AutoloopAddress address = {5U, 7U}) {
    emberlights::AutoloopAutoscriptRequest request;
    request.track_duration_ticks =
        16 * emberlights::kMusicalTicksPerQuarter;
    request.loop_length_ticks =
        4 * emberlights::kMusicalTicksPerQuarter;
    request.grid_ticks = emberlights::kMusicalTicksPerQuarter / 2;
    request.style = emberlights::AutoloopAutoscriptStyle::Balanced;
    request.complexity = emberlights::AutoloopAutoscriptComplexity::Low;
    request.musical_sections = {{
        0,
        request.track_duration_ticks,
        emberlights::AutoloopAutoscriptSectionKind::Chorus,
        700U}};
    request.eligible_role_selectors = {"wash"};
    request.seed = 0x4122026U;
    request.first_placement = address;
    return request;
}

template <typename Predicate>
[[nodiscard]] bool wait_until(Predicate&& predicate) {
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return predicate();
}

void test_generate_preview_commit_save_reopen_and_launch() {
    const auto original = make_project();
    const auto original_bytes = emberlights::serialize_project(original);
    emberlights::StudioAutoloopAutoscriptWorkflow workflow;
    CHECK(workflow.load_document(original));

    const auto proposed = workflow.propose_and_preview(request_at());
    CHECK(proposed.result ==
          emberlights::StudioAutoloopAutoscriptWorkflowResult::ProposalReady);
    auto review = workflow.snapshot();
    CHECK(review.proposal_ready);
    CHECK(review.preview_ready);
    CHECK(review.can_commit);
    CHECK(!review.committed);
    const showcore::AutoloopAddress expected_address{5U, 7U};
    CHECK(review.address == expected_address);
    CHECK(review.generated_asset_count == 1U);
    CHECK(review.generated_event_count > 0U);
    CHECK(review.proposal_digest.size() == 64U);
    CHECK(review.preview_source_digest.size() == 64U);
    CHECK(review.preview.output_disabled);
    CHECK(review.preview.autoloop_format ==
          emberlights::StudioPreviewAutoloopFormat::V2);
    CHECK(review.preview.frame_sha256.size() == 64U);
    CHECK(review.preview.compiled_digest.size() == 64U);
    CHECK(review.preview.fixtures.size() == 1U);
    CHECK(!review.preview.fixtures.empty() &&
          review.preview.fixtures.front().dmx_values.size() == 4U);
    CHECK(emberlights::serialize_project(review.document.document) ==
          original_bytes);

    const auto first_frame = review.preview.frame_sha256;
    CHECK(workflow.preview_phase(0.5));
    review = workflow.snapshot();
    CHECK(review.preview.phase == 0.5);
    CHECK(review.preview.frame_sha256.size() == 64U);
    CHECK(review.preview.frame_sha256 != first_frame);

    const auto committed = workflow.commit();
    CHECK(committed.result ==
          emberlights::StudioAutoloopAutoscriptWorkflowResult::Committed);
    const auto durable = workflow.snapshot();
    CHECK(durable.committed);
    CHECK(!durable.can_commit);
    CHECK(durable.document.dirty);
    CHECK(durable.document.undo_count == 1U);
    CHECK(durable.document.autoloop_source);
    CHECK(durable.document.autoloop_source.stamp.present);
    CHECK(durable.document.autoloop_source.stamp.source_digest ==
          durable.preview_source_digest);
    CHECK(emberlights::serialize_project(durable.document.document) !=
          original_bytes);

    const auto root = std::filesystem::path(
        "build/autoloop-autoscript-workflow-e2e");
    const auto path = root / "project.emberlights";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    CHECK(emberlights::save_project_atomic(
        path, durable.document.document, false));

    emberlights::ProjectDocument reopened;
    CHECK(emberlights::load_project(path, reopened, false));
    const auto persisted =
        emberlights::inspect_persisted_autoloop_source(reopened);
    CHECK(persisted);
    CHECK(persisted.stamp.present);
    CHECK(persisted.stamp.source_digest == durable.preview_source_digest);
    CHECK(persisted.source.placements.size() == 1U);
    CHECK(persisted.source.placements.front().id == durable.placement_id);

    auto compilation =
        emberlights::compile_project_with_persisted_autoloops(reopened);
    CHECK(compilation);
    CHECK(compilation.show != nullptr);
    CHECK(compilation.show->autoloop_v2_package() != nullptr);
    CHECK(compilation.show->autoloop_v2_package()->placement({5U, 7U}) !=
          nullptr);

    emberlights::RunnerService runner;
    CHECK(runner.start(std::move(compilation.show), reopened));
    CHECK(wait_until([&]() {
        const auto status = runner.status();
        return status.frames >= 3U && status.autoloop_v2.package_active;
    }));
    TestHost host;
    emberlights::UiCommandFacade commands(runner, host);
    commands.set_active_project(&reopened);
    emberlights::UiCommandInvocation launch;
    launch.command = emberlights::UiCommandId::AutoloopLaunch;
    launch.target_id = durable.placement_id;
    CHECK(commands.invoke(launch) ==
          emberlights::UiInvocationResult::Accepted);
    CHECK(wait_until([&]() {
        const auto status = runner.status();
        return status.autoloop_v2.active_source ==
                showcore::AutoloopDirectorSource::Manual &&
            status.autoloop_v2.active_address == expected_address;
    }));
    runner.stop();
    std::filesystem::remove_all(root, ignored);
}

void test_discard_and_invalid_persistence_fail_closed() {
    const auto project = make_project();
    emberlights::StudioAutoloopAutoscriptWorkflow workflow;
    CHECK(workflow.load_document(project));
    CHECK(workflow.propose_and_preview(request_at()));
    CHECK(workflow.discard());
    const auto discarded = workflow.snapshot();
    CHECK(!discarded.has_proposal);
    CHECK(!discarded.preview_ready);
    CHECK(emberlights::serialize_project(discarded.document.document) ==
          emberlights::serialize_project(project));

    auto malformed = project;
    malformed.unknown_records.push_back(
        "EMBERLIGHTS_AUTOLOOP_SOURCE_RECORD\tbroken");
    const auto compilation =
        emberlights::compile_project_with_persisted_autoloops(malformed);
    CHECK(!compilation);
    CHECK(compilation.validation.error_count() >= 1U);
}

}  // namespace

int main() {
    test_generate_preview_commit_save_reopen_and_launch();
    test_discard_and_invalid_persistence_fail_closed();
    if (failures != 0) {
        std::cerr << failures
                  << " AutoScript Studio workflow end-to-end check(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "AutoScript Studio workflow end-to-end checks passed\n";
    return EXIT_SUCCESS;
}
