#include "emberlights/autoloop_autoscript_studio.hpp"

#include "emberlights/project_io.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

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

template <typename Collection>
[[nodiscard]] auto find_by_id(Collection& collection, std::string_view id) {
    return std::find_if(
        collection.begin(), collection.end(),
        [id](const auto& value) { return value.id == id; });
}

[[nodiscard]] emberlights::AutoloopSourceDocument existing_source(
    std::string name = "Existing User Asset") {
    emberlights::AutoloopSourceDocument source;
    emberlights::AutoloopAssetDefinition asset;
    asset.id = "studio.bridge.existing.asset";
    asset.name = std::move(name);
    asset.description = "Unrelated persisted rich source content.";
    asset.tags = {"existing", "user"};
    asset.style = "user";
    asset.energy = 0.4F;
    asset.program_id = "studio.bridge.existing.program";
    asset.launch_profile_id = "studio.bridge.existing.launch";
    asset.provenance_id = "studio.bridge.existing.provenance";
    source.assets.push_back(std::move(asset));
    source.placements.push_back({
        "studio.bridge.existing.placement", 4U, 7U,
        "studio.bridge.existing.asset", "existing-key"});

    emberlights::AutoloopProgramDefinition program;
    program.id = "studio.bridge.existing.program";
    program.length_ticks = 4 * emberlights::kMusicalTicksPerQuarter;
    program.targets.push_back({
        "studio.bridge.existing.target",
        emberlights::AutoloopTargetKind::Master,
        {},
        {showcore::Property::Intensity}});
    program.lanes.push_back({
        "studio.bridge.existing.lane",
        "studio.bridge.existing.target",
        0U});
    emberlights::AutoloopEventDefinition event;
    event.id = "studio.bridge.existing.event";
    event.lane_id = "studio.bridge.existing.lane";
    event.kind = emberlights::AutoloopEventKind::PropertyBlock;
    event.start_tick = 0;
    event.end_tick = program.length_ticks;
    event.property = showcore::Property::Intensity;
    event.value = showcore::PropertyValue::set(0.4F);
    program.events.push_back(std::move(event));
    source.programs.push_back(std::move(program));

    emberlights::AutoloopLaunchProfileDefinition launch;
    launch.id = "studio.bridge.existing.launch";
    launch.repeat = showcore::AutoloopRepeat::Infinite;
    source.launch_profiles.push_back(std::move(launch));

    emberlights::AutoloopProvenanceDefinition provenance;
    provenance.id = "studio.bridge.existing.provenance";
    provenance.origin = emberlights::AutoloopProvenanceOrigin::Native;
    provenance.producer_id = "emberlights.studio-bridge-test";
    provenance.producer_version = "1";
    provenance.source_object_key = "studio.bridge.existing.asset";
    provenance.evidence_status = "synthetic-existing";
    source.provenance.push_back(std::move(provenance));
    emberlights::normalize_autoloop_source(source);
    return source;
}

[[nodiscard]] emberlights::AutoloopAutoscriptRequest request_at(
    showcore::AutoloopAddress address = {0U, 0U}) {
    emberlights::AutoloopAutoscriptRequest request;
    request.track_duration_ticks =
        8 * emberlights::kMusicalTicksPerQuarter;
    request.loop_length_ticks =
        4 * emberlights::kMusicalTicksPerQuarter;
    request.grid_ticks = emberlights::kMusicalTicksPerQuarter;
    request.style = emberlights::AutoloopAutoscriptStyle::Balanced;
    request.complexity = emberlights::AutoloopAutoscriptComplexity::Low;
    request.musical_sections = {
        {0, 8 * emberlights::kMusicalTicksPerQuarter,
         emberlights::AutoloopAutoscriptSectionKind::Chorus, 700U}};
    request.eligible_role_selectors = {"role.washes"};
    request.seed = 0xA5705C21U;
    request.first_placement = address;
    request.content_budget.maximum_generated_assets = 2U;
    request.content_budget.maximum_generated_events = 128U;
    request.content_budget.maximum_candidate_canonical_bytes =
        1024U * 1024U;
    request.operation_budget.maximum_operations = 5000U;
    return request;
}

[[nodiscard]] const emberlights::AutoloopAssetDefinition* asset(
    const emberlights::AutoloopSourceDocument& source,
    std::string_view id) {
    const auto found = find_by_id(source.assets, id);
    return found == source.assets.end() ? nullptr : &*found;
}

[[nodiscard]] const emberlights::AutoloopPlacementDefinition* placement(
    const emberlights::AutoloopSourceDocument& source,
    std::string_view id) {
    const auto found = find_by_id(source.placements, id);
    return found == source.placements.end() ? nullptr : &*found;
}

void test_absent_source_first_commit_and_one_undo_redo() {
    emberlights::StudioDocumentService service;
    auto legacy_project = emberlights::make_starter_project();
    legacy_project.fixtures.push_back({
        "studio.bridge.legacy.fixture", "Legacy Fixture",
        "builtin.generic.dimmer-1ch", 1U, 1U, {}});
    legacy_project.looks.push_back({
        "studio.bridge.legacy.look", "Legacy Look", 0U,
        {{"studio.bridge.legacy.fixture", showcore::Property::Intensity,
          showcore::PropertyValue::set(0.5F)}}});
    legacy_project.autoloops.push_back({
        "studio.bridge.legacy.loop", "Legacy Loop", 3U, 2U, 4.0F,
        showcore::AutoloopRepeat::Infinite,
        {{0.0F, "studio.bridge.legacy.look",
          showcore::AutoloopTransition::Cut}}});
    CHECK(service.replace_document(
        service.generation(), std::move(legacy_project),
        emberlights::StudioDocumentBoundary::OpenedDocument));
    const auto initial = service.snapshot();
    const auto initial_bytes =
        emberlights::serialize_project(initial.document);
    const auto legacy_digest = emberlights::autoloop_source_digest(
        emberlights::adapt_format1_autoloops(initial.document));
    CHECK(initial.autoloop_source);
    CHECK(!initial.autoloop_source.stamp.present);

    const auto proposal = emberlights::propose_studio_autoloop_autoscript(
        initial, request_at());
    const emberlights::AutoloopSourceDocument empty_source;
    CHECK(proposal.ready());
    CHECK(proposal.valid_document_source());
    CHECK(proposal.document_generation() == initial.generation);
    CHECK(proposal.source_stamp() == initial.autoloop_source.stamp);
    CHECK(proposal.base_source_digest() ==
          emberlights::autoloop_source_digest(empty_source));
    CHECK(proposal.proposal().preview_source().assets.size() == 1U);
    // Golden v1 identities bind the generator's proposal digest to the
    // bridge's independent recomputation. Any digest-format drift must bump
    // the owning version(s); one-sided drift is rejected before commit.
    CHECK(emberlights::kAutoloopAutoscriptGeneratorVersion == 1U);
    CHECK(emberlights::kStudioAutoloopAutoscriptBridgeVersion == 1U);
    CHECK(proposal.proposal().proposal_digest() ==
          "f88473fae29cbd9b29a92ca999a199003383a7ae28ef21443a970962d4bf9dfb");
    CHECK(proposal.bridge_digest() ==
          "24432dfe08f5184828c00344acfa790aa2e4334e7d21da8f54b663d2fdd942e0");
    CHECK(service.generation() == initial.generation);
    CHECK(emberlights::serialize_project(service.snapshot().document) ==
          initial_bytes);

    const auto committed =
        emberlights::apply_studio_autoloop_autoscript_proposal(
            service, proposal);
    CHECK(committed.result == emberlights::StudioMutationResult::Applied);
    const auto applied = service.snapshot();
    CHECK(applied.autoloop_source);
    CHECK(applied.autoloop_source.stamp.present);
    CHECK(applied.undo_count == 1U);
    CHECK(applied.redo_count == 0U);
    CHECK(emberlights::autoloop_source_digest(
              emberlights::adapt_format1_autoloops(applied.document)) ==
          legacy_digest);
    CHECK(applied.autoloop_source.stamp.source_digest ==
          proposal.proposal().preview_source_digest());
    const auto applied_bytes =
        emberlights::serialize_project(applied.document);

    CHECK(service.undo(applied.generation));
    const auto undone = service.snapshot();
    CHECK(!undone.autoloop_source.stamp.present);
    CHECK(emberlights::serialize_project(undone.document) == initial_bytes);
    CHECK(!undone.can_undo);
    CHECK(undone.can_redo);

    CHECK(service.redo(undone.generation));
    const auto redone = service.snapshot();
    CHECK(redone.autoloop_source.stamp == applied.autoloop_source.stamp);
    CHECK(emberlights::serialize_project(redone.document) == applied_bytes);
    CHECK(redone.can_undo);
    CHECK(!redone.can_redo);
}

void test_existing_source_preserved_and_save_reopen() {
    auto project = emberlights::make_starter_project();
    project.unknown_records = {
        "FUTURE_BEFORE\tstable\topaque",
        "FUTURE_AFTER\tstable\tsecond"};
    const auto source = existing_source();
    CHECK(emberlights::upsert_persisted_autoloop_source(project, source));
    const auto baseline_bytes = emberlights::serialize_project(project);

    emberlights::StudioDocumentService service;
    CHECK(service.replace_document(
        service.generation(), project,
        emberlights::StudioDocumentBoundary::OpenedDocument));
    const auto initial = service.snapshot();
    CHECK(initial.undo_count == 0U);
    CHECK(initial.autoloop_source.stamp.present);

    const auto proposal = emberlights::propose_studio_autoloop_autoscript(
        initial, request_at());
    CHECK(proposal.ready());
    CHECK(proposal.base_source_digest() ==
          initial.autoloop_source.stamp.source_digest);
    CHECK(proposal.proposal().preview_source().assets.size() ==
          source.assets.size() + 1U);
    CHECK(asset(
        proposal.proposal().preview_source(),
        "studio.bridge.existing.asset") != nullptr);
    CHECK(placement(
        proposal.proposal().preview_source(),
        "studio.bridge.existing.placement") != nullptr);

    CHECK(emberlights::apply_studio_autoloop_autoscript_proposal(
        service, proposal));
    const auto applied = service.snapshot();
    CHECK(applied.undo_count == 1U);
    CHECK(applied.document.unknown_records[0] ==
          project.unknown_records[0]);
    CHECK(applied.document.unknown_records[1] ==
          project.unknown_records[1]);
    const auto* existing = asset(
        applied.autoloop_source.source,
        "studio.bridge.existing.asset");
    CHECK(existing != nullptr);
    if (existing != nullptr) {
        CHECK(existing->name == "Existing User Asset");
        CHECK(existing->description ==
              "Unrelated persisted rich source content.");
    }

    CHECK(service.undo(applied.generation));
    CHECK(emberlights::serialize_project(service.snapshot().document) ==
          baseline_bytes);
    CHECK(service.redo(service.generation()));
    const auto durable = service.snapshot();
    CHECK(durable.autoloop_source.stamp.source_digest ==
          proposal.proposal().preview_source_digest());

    const auto root = std::filesystem::path(
        "build/autoloop-autoscript-studio-round-trip");
    const auto path = root / "project.emberlights";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    CHECK(emberlights::save_project_atomic(path, durable.document, false));
    CHECK(service.acknowledge_saved(durable.generation));
    CHECK(!service.dirty());

    emberlights::ProjectDocument loaded;
    CHECK(emberlights::load_project(path, loaded, false));
    emberlights::StudioDocumentService reopened;
    CHECK(reopened.replace_document(
        reopened.generation(), std::move(loaded),
        emberlights::StudioDocumentBoundary::OpenedDocument));
    const auto reopened_snapshot = reopened.snapshot();
    CHECK(!reopened_snapshot.dirty);
    CHECK(reopened_snapshot.autoloop_source.stamp ==
          durable.autoloop_source.stamp);
    CHECK(emberlights::serialize_autoloop_source(
              reopened_snapshot.autoloop_source.source) ==
          emberlights::serialize_autoloop_source(
              proposal.proposal().preview_source()));
    CHECK(asset(
        reopened_snapshot.autoloop_source.source,
        "studio.bridge.existing.asset") != nullptr);
    std::filesystem::remove_all(root, ignored);
}

void test_stale_document_and_source_fail_closed() {
    emberlights::StudioDocumentService document_service;
    const auto document_base = document_service.snapshot();
    const auto document_proposal =
        emberlights::propose_studio_autoloop_autoscript(
            document_base, request_at());
    CHECK(document_proposal.ready());
    auto renamed = document_base.document;
    renamed.name = "Newer unrelated document";
    CHECK(document_service.apply_candidate(
        document_base.generation, std::move(renamed)));
    const auto newer_document = document_service.snapshot();
    const auto newer_document_bytes =
        emberlights::serialize_project(newer_document.document);
    const auto stale_document =
        emberlights::apply_studio_autoloop_autoscript_proposal(
            document_service, document_proposal);
    CHECK(stale_document.result ==
          emberlights::StudioMutationResult::StaleGeneration);
    CHECK(document_service.generation() == newer_document.generation);
    CHECK(emberlights::serialize_project(
              document_service.snapshot().document) ==
          newer_document_bytes);

    auto project = emberlights::make_starter_project();
    auto source = existing_source();
    CHECK(emberlights::upsert_persisted_autoloop_source(project, source));
    emberlights::StudioDocumentService source_service;
    CHECK(source_service.replace_document(
        source_service.generation(), std::move(project),
        emberlights::StudioDocumentBoundary::OpenedDocument));
    const auto source_base = source_service.snapshot();
    const auto source_proposal =
        emberlights::propose_studio_autoloop_autoscript(
            source_base, request_at());
    CHECK(source_proposal.ready());
    source.assets.front().name = "Newer persisted source";
    CHECK(source_service.apply_autoloop_source(
        source_base.generation, source_base.autoloop_source.stamp, source));
    const auto newer_source = source_service.snapshot();
    const auto newer_source_bytes =
        emberlights::serialize_project(newer_source.document);
    const auto stale_source =
        emberlights::apply_studio_autoloop_autoscript_proposal(
            source_service, source_proposal);
    CHECK(stale_source.result ==
          emberlights::StudioMutationResult::StaleGeneration);
    CHECK(source_service.generation() == newer_source.generation);
    CHECK(emberlights::serialize_project(source_service.snapshot().document) ==
          newer_source_bytes);

    auto mismatched_snapshot = newer_source;
    mismatched_snapshot.autoloop_source.source.assets.front().name =
        "Digest mismatch";
    const auto mismatched =
        emberlights::propose_studio_autoloop_autoscript(
            mismatched_snapshot, request_at());
    CHECK(!mismatched.valid_document_source());
    CHECK(!mismatched.ready());
    const auto before_rejected = source_service.snapshot();
    const auto rejected =
        emberlights::apply_studio_autoloop_autoscript_proposal(
            source_service, mismatched);
    CHECK(rejected.result ==
          emberlights::StudioMutationResult::InvalidCandidate);
    CHECK(source_service.generation() == before_rejected.generation);
    CHECK(source_service.snapshot().autoloop_source.stamp ==
          before_rejected.autoloop_source.stamp);

    auto unsupported_stamp_snapshot = newer_source;
    ++unsupported_stamp_snapshot.autoloop_source.stamp.record_version;
    const auto unsupported_stamp =
        emberlights::propose_studio_autoloop_autoscript(
            unsupported_stamp_snapshot, request_at());
    CHECK(!unsupported_stamp.valid_document_source());
    CHECK(!unsupported_stamp.ready());
    CHECK(emberlights::apply_studio_autoloop_autoscript_proposal(
              source_service, unsupported_stamp).result ==
          emberlights::StudioMutationResult::InvalidCandidate);
    CHECK(source_service.generation() == before_rejected.generation);
    CHECK(source_service.snapshot().autoloop_source.stamp ==
          before_rejected.autoloop_source.stamp);
}

void test_failed_proposals_never_mutate_document() {
    auto project = emberlights::make_starter_project();
    const auto source = existing_source();
    CHECK(emberlights::upsert_persisted_autoloop_source(project, source));
    emberlights::StudioDocumentService service;
    CHECK(service.replace_document(
        service.generation(), std::move(project),
        emberlights::StudioDocumentBoundary::OpenedDocument));
    const auto before = service.snapshot();
    const auto before_bytes = emberlights::serialize_project(before.document);

    auto invalid_request = request_at();
    invalid_request.seed.reset();
    const auto invalid = emberlights::propose_studio_autoloop_autoscript(
        before, std::move(invalid_request));
    CHECK(invalid.valid_document_source());
    CHECK(!invalid.ready());
    CHECK(invalid.proposal().result() ==
          emberlights::AutoloopAutoscriptProposalResult::InvalidRequest);
    CHECK(emberlights::apply_studio_autoloop_autoscript_proposal(
              service, invalid).result ==
          emberlights::StudioMutationResult::InvalidCandidate);

    const auto occupied = emberlights::propose_studio_autoloop_autoscript(
        before, request_at({4U, 7U}));
    CHECK(!occupied.ready());
    CHECK(occupied.proposal().result() ==
          emberlights::AutoloopAutoscriptProposalResult::OccupiedPlacement);
    CHECK(emberlights::apply_studio_autoloop_autoscript_proposal(
              service, occupied).result ==
          emberlights::StudioMutationResult::InvalidCandidate);

    const auto after = service.snapshot();
    CHECK(after.generation == before.generation);
    CHECK(after.undo_count == before.undo_count);
    CHECK(after.autoloop_source.stamp == before.autoloop_source.stamp);
    CHECK(emberlights::serialize_project(after.document) == before_bytes);
}

}  // namespace

int main() {
    test_absent_source_first_commit_and_one_undo_redo();
    test_existing_source_preserved_and_save_reopen();
    test_stale_document_and_source_fail_closed();
    test_failed_proposals_never_mutate_document();

    if (failures != 0) {
        std::cerr << failures
                  << " Studio AutoScript bridge test(s) failed\n";
        return 1;
    }
    std::cout << "Studio AutoScript bridge tests passed\n";
    return 0;
}
