#include "emberlights/project_io.hpp"
#include "emberlights/soundswitch_migration_ir.hpp"
#include "emberlights/studio_document.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

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

[[nodiscard]] bool write_file(
    const std::filesystem::path& path,
    std::string_view bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(output);
}

[[nodiscard]] std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

[[nodiscard]] bool contains(
    const std::vector<std::string>& values,
    std::string_view expected) {
    return std::find(values.begin(), values.end(), expected) != values.end();
}

void test_studio_document_service() {
    emberlights::StudioDocumentService service;
    auto initial = service.snapshot();
    CHECK(initial.generation == 1U);
    CHECK(!initial.dirty);
    CHECK(!initial.can_undo);
    CHECK(!initial.can_redo);

    auto first_edit = initial.document;
    first_edit.name = "First Studio revision";
    const auto applied = service.apply_candidate(initial.generation, first_edit);
    CHECK(applied.result == emberlights::StudioMutationResult::Applied);
    CHECK(applied.generation == 2U);
    CHECK(service.dirty());
    CHECK(service.snapshot().undo_count == 1U);

    const auto unchanged = service.apply_candidate(service.generation(), first_edit);
    CHECK(unchanged.result == emberlights::StudioMutationResult::NoChange);
    CHECK(service.generation() == 2U);
    CHECK(service.snapshot().undo_count == 1U);

    auto stale_edit = first_edit;
    stale_edit.name = "Stale edit";
    const auto stale = service.apply_candidate(initial.generation, stale_edit);
    CHECK(stale.result == emberlights::StudioMutationResult::StaleGeneration);
    CHECK(service.snapshot().document.name == "First Studio revision");

    auto invalid = first_edit;
    invalid.id.clear();
    const auto rejected = service.apply_candidate(service.generation(), invalid);
    CHECK(rejected.result == emberlights::StudioMutationResult::ValidationFailed);
    CHECK(service.generation() == 2U);
    CHECK(service.snapshot().document.id == first_edit.id);

    const auto serialized_first = emberlights::serialize_project(service.snapshot().document);
    const auto undone = service.undo(service.generation());
    CHECK(undone.result == emberlights::StudioMutationResult::Applied);
    CHECK(service.snapshot().document.name == initial.document.name);
    const auto redone = service.redo(service.generation());
    CHECK(redone.result == emberlights::StudioMutationResult::Applied);
    CHECK(emberlights::serialize_project(service.snapshot().document) == serialized_first);

    auto opened = service.snapshot().document;
    opened.name = "Opened boundary";
    const auto open_result = service.replace_document(
        service.generation(), opened, emberlights::StudioDocumentBoundary::OpenedDocument);
    CHECK(open_result.result == emberlights::StudioMutationResult::Applied);
    CHECK(!service.dirty());
    CHECK(!service.snapshot().can_undo);
    CHECK(!service.snapshot().can_redo);

    auto fresh = emberlights::make_starter_project();
    fresh.name = "Unsaved project";
    const auto new_result = service.replace_document(
        service.generation(), fresh, emberlights::StudioDocumentBoundary::NewDocument);
    CHECK(new_result.result == emberlights::StudioMutationResult::Applied);
    CHECK(service.dirty());

    const auto root = std::filesystem::path("build/studio-foundation-document");
    const auto invalid_save_target = root / "save-target-directory";
    const auto saved_path = root / "project.emberlights";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root, ignored);
    std::filesystem::create_directories(invalid_save_target, ignored);
    CHECK(!emberlights::save_project_atomic(
        invalid_save_target, service.snapshot().document, false));
    CHECK(service.dirty());

    auto with_unknown = service.snapshot().document;
    with_unknown.unknown_records.push_back(
        "FUTURE_STUDIO_RECORD\tstable-id\topaque-value");
    CHECK(service.apply_candidate(service.generation(), with_unknown));
    const auto save_generation = service.generation();
    CHECK(emberlights::save_project_atomic(
        saved_path, service.snapshot().document, false));
    CHECK(service.acknowledge_saved(save_generation));
    CHECK(!service.dirty());

    emberlights::ProjectDocument reparsed;
    CHECK(emberlights::load_project(saved_path, reparsed, false));
    CHECK(reparsed.unknown_records.size() == 1U);
    if (!reparsed.unknown_records.empty()) {
        CHECK(reparsed.unknown_records.front() ==
            "FUTURE_STUDIO_RECORD\tstable-id\topaque-value");
    }

    auto after_save = service.snapshot().document;
    after_save.name = "Dirty after save";
    CHECK(service.apply_candidate(service.generation(), after_save));
    CHECK(service.dirty());
    const auto stale_save_ack = service.acknowledge_saved(save_generation);
    CHECK(stale_save_ack.result == emberlights::StudioMutationResult::StaleGeneration);
    CHECK(service.dirty());

    auto restored = service.snapshot().document;
    restored.name = "Restored boundary";
    CHECK(service.replace_document(
        service.generation(), restored,
        emberlights::StudioDocumentBoundary::RestoredDocument));
    CHECK(!service.dirty());
    CHECK(!service.snapshot().can_undo);

    for (std::size_t index = 0U;
         index < emberlights::kMaximumProjectUndoEntries + 5U; ++index) {
        auto edit = service.snapshot().document;
        edit.name = "Bounded edit " + std::to_string(index);
        CHECK(service.apply_candidate(service.generation(), std::move(edit)));
    }
    CHECK(service.snapshot().undo_count == emberlights::kMaximumProjectUndoEntries);
    std::filesystem::remove_all(root, ignored);
}

emberlights::SoundSwitchCorpusManifest make_synthetic_manifest(
    const std::filesystem::path& source) {
    std::error_code ignored;
    std::filesystem::remove_all(source, ignored);
    std::filesystem::create_directories(source, ignored);
    CHECK(!ignored);
    CHECK(write_file(source / "Example.ssproj", "PRIVATE_SOURCE_BYTES"));
    CHECK(write_file(source / "SoundSwitchVenues.bin", "venue"));
    CHECK(write_file(source / "SoundSwitchAutoLoops.bin", "loops"));
    const auto inspection = emberlights::inspect_soundswitch_project(source);
    CHECK(inspection.complete());
    return emberlights::build_soundswitch_corpus_manifest(
        inspection, "2.10.synthetic", emberlights::SoundSwitchMigrationScope::ProjectOnly);
}

void test_soundswitch_corpus_manifest() {
    const auto source = std::filesystem::path("build/studio-foundation-source.ssproj");
    auto manifest = make_synthetic_manifest(source);
    CHECK(emberlights::validate_soundswitch_corpus_manifest(manifest));
    CHECK(manifest.artifacts.size() == 3U);
    const auto project_artifact = std::find_if(
        manifest.artifacts.begin(), manifest.artifacts.end(), [](const auto& artifact) {
            return artifact.kind == emberlights::SoundSwitchArtifactKind::ProjectManifest;
        });
    CHECK(project_artifact != manifest.artifacts.end());
    if (project_artifact != manifest.artifacts.end()) {
        CHECK(project_artifact->artifact_id ==
            "ssa1-12791a7a39bce4ecf676c573a8877bab2ccd0d8b8cc76a71e0f6b5ea663d606c");
    }
    CHECK(contains(manifest.missing_dependency_codes,
        "soundswitch.track_map_unavailable"));
    CHECK(contains(manifest.missing_dependency_codes,
        "soundswitch.lighting_files_unavailable"));
    CHECK(contains(manifest.missing_dependency_codes,
        "soundswitch.scripted_audio_unavailable"));
    CHECK(contains(manifest.missing_dependency_codes,
        "soundswitch.dj_library_identity_unavailable"));
    CHECK(!contains(manifest.missing_dependency_codes,
        "soundswitch.project_manifest_missing"));

    const auto first = emberlights::serialize_soundswitch_corpus_manifest(manifest);
    CHECK(!first.empty());
    CHECK(first.find("PRIVATE_SOURCE_BYTES") == std::string::npos);
    CHECK(first.find(std::filesystem::absolute(source).string()) == std::string::npos);
    CHECK(first.find("\"payload\"") == std::string::npos);
    std::reverse(manifest.artifacts.begin(), manifest.artifacts.end());
    std::reverse(
        manifest.missing_dependency_codes.begin(),
        manifest.missing_dependency_codes.end());
    CHECK(emberlights::serialize_soundswitch_corpus_manifest(manifest) == first);

    const auto inspection = emberlights::inspect_soundswitch_project(source);
    const auto again = emberlights::build_soundswitch_corpus_manifest(
        inspection, "2.10.synthetic");
    CHECK(again.bundle_id == manifest.bundle_id);
    CHECK(again.artifacts.front().artifact_id == manifest.artifacts.back().artifact_id);

    const emberlights::SoundSwitchInspection unavailable_inspection;
    const auto unavailable = emberlights::build_soundswitch_corpus_manifest(
        unavailable_inspection, {}, emberlights::SoundSwitchMigrationScope::ProjectOnly, false);
    CHECK(unavailable.artifacts.empty());
    CHECK(contains(unavailable.missing_dependency_codes,
        "authorized_soundswitch_corpus_unavailable"));
    CHECK(contains(unavailable.missing_dependency_codes,
        "soundswitch.source_version_unverified"));
    CHECK(emberlights::validate_soundswitch_corpus_manifest(unavailable));

    auto only_manifest_source = source;
    only_manifest_source += "-missing";
    std::error_code ignored;
    std::filesystem::remove_all(only_manifest_source, ignored);
    std::filesystem::create_directories(only_manifest_source, ignored);
    CHECK(write_file(only_manifest_source / "Example.ssproj", "manifest"));
    const auto missing = emberlights::build_soundswitch_corpus_manifest(
        emberlights::inspect_soundswitch_project(only_manifest_source), "2.10.synthetic");
    CHECK(contains(missing.missing_dependency_codes,
        "soundswitch.venue_database_missing"));
    CHECK(contains(missing.missing_dependency_codes,
        "soundswitch.autoloop_database_missing"));

    const auto oversized = emberlights::inspect_soundswitch_project(
        source, {.maximum_files = 100U, .maximum_file_bytes = 2U,
                 .maximum_total_bytes = 100U});
    CHECK(!oversized.complete());
    CHECK(oversized.error_count() > 0U);

    const auto symlink_source = source.string() + "-symlink";
    std::filesystem::remove(symlink_source, ignored);
    std::filesystem::create_directory_symlink(source, symlink_source, ignored);
    if (!ignored) {
        CHECK(!emberlights::inspect_soundswitch_project(symlink_source).complete());
        std::filesystem::remove(symlink_source, ignored);
    }

    std::filesystem::remove_all(source, ignored);
    std::filesystem::remove_all(only_manifest_source, ignored);
}

emberlights::SoundSwitchMigrationReport make_valid_report(
    const emberlights::SoundSwitchCorpusManifest& manifest) {
    emberlights::SoundSwitchMigrationReport report;
    report.source_bundle_id = manifest.bundle_id;
    report.source_version = manifest.source_version;
    constexpr std::array statuses{
        emberlights::MigrationItemStatus::Exact,
        emberlights::MigrationItemStatus::DeterministicallyTranslated,
        emberlights::MigrationItemStatus::Approximated,
        emberlights::MigrationItemStatus::PreservedOpaque,
        emberlights::MigrationItemStatus::Unsupported,
        emberlights::MigrationItemStatus::Conflicted,
        emberlights::MigrationItemStatus::MissingDependency,
        emberlights::MigrationItemStatus::RejectedUnsafe};
    for (std::size_t index = 0U; index < statuses.size(); ++index) {
        emberlights::MigrationItem item;
        item.item_id = "item-" + std::to_string(index);
        item.item_kind = "syntheticFixture";
        item.status = statuses[index];
        item.source_label = "Display label " + std::to_string(index);
        item.rule_id = "synthetic.rule.v1";
        if (item.status != emberlights::MigrationItemStatus::Conflicted &&
            item.status != emberlights::MigrationItemStatus::MissingDependency &&
            item.status != emberlights::MigrationItemStatus::Unsupported) {
            item.destination_ref = "destination-" + std::to_string(index);
        }
        if (item.status == emberlights::MigrationItemStatus::Approximated) {
            item.warnings.push_back("Synthetic approximation differs from the source.");
        }
        if (item.status == emberlights::MigrationItemStatus::MissingDependency) {
            item.blockers.push_back("soundswitch.scripted_audio_unavailable");
        }
        if (item.status == emberlights::MigrationItemStatus::PreservedOpaque ||
            item.status == emberlights::MigrationItemStatus::Exact) {
            emberlights::MigrationEvidenceRef evidence;
            evidence.artifact_id = manifest.artifacts.front().artifact_id;
            evidence.has_byte_range = item.status == emberlights::MigrationItemStatus::Exact;
            evidence.offset = evidence.has_byte_range ? 4U : 0U;
            evidence.length = evidence.has_byte_range ? 8U : 0U;
            evidence.decoder_id = "synthetic.decoder";
            evidence.decoder_version = "1";
            item.evidence.push_back(std::move(evidence));
        }
        report.items.push_back(std::move(item));
    }
    emberlights::normalize_soundswitch_migration_report(report);
    return report;
}

void test_soundswitch_migration_report() {
    const auto source = std::filesystem::path("build/studio-foundation-report.ssproj");
    const auto manifest = make_synthetic_manifest(source);
    auto report = make_valid_report(manifest);
    CHECK(emberlights::validate_soundswitch_migration_report(report));
    CHECK(report.aggregate_counts.total() == 8U);
    for (std::size_t index = 0U;
         index < static_cast<std::size_t>(emberlights::MigrationItemStatus::Count);
         ++index) {
        const auto status = static_cast<emberlights::MigrationItemStatus>(index);
        const auto name = emberlights::migration_item_status_name(status);
        emberlights::MigrationItemStatus parsed{};
        CHECK(emberlights::parse_migration_item_status(name, parsed));
        CHECK(parsed == status);
        CHECK(report.aggregate_counts.count(status) == 1U);
    }

    const auto first = emberlights::serialize_soundswitch_migration_report(report);
    CHECK(!first.empty());
    CHECK(first.find("\"payload\"") == std::string::npos);
    std::reverse(report.items.begin(), report.items.end());
    CHECK(emberlights::serialize_soundswitch_migration_report(report) == first);

    const auto preserved_id = report.items.front().item_id;
    const auto preserved_status = report.items.front().status;
    report.items.front().source_label = "A completely different display label";
    CHECK(report.items.front().item_id == preserved_id);
    CHECK(report.items.front().status == preserved_status);

    auto invalid_approximation = make_valid_report(manifest);
    auto approximation = std::find_if(
        invalid_approximation.items.begin(), invalid_approximation.items.end(),
        [](const auto& item) {
            return item.status == emberlights::MigrationItemStatus::Approximated;
        });
    approximation->warnings.clear();
    CHECK(!emberlights::validate_soundswitch_migration_report(invalid_approximation));
    CHECK(emberlights::serialize_soundswitch_migration_report(
        invalid_approximation).empty());

    auto invalid_opaque = make_valid_report(manifest);
    auto opaque = std::find_if(invalid_opaque.items.begin(), invalid_opaque.items.end(),
        [](const auto& item) {
            return item.status == emberlights::MigrationItemStatus::PreservedOpaque;
        });
    opaque->evidence.clear();
    emberlights::normalize_soundswitch_migration_report(invalid_opaque);
    CHECK(!emberlights::validate_soundswitch_migration_report(invalid_opaque));

    auto invalid_conflict = make_valid_report(manifest);
    auto conflict = std::find_if(invalid_conflict.items.begin(), invalid_conflict.items.end(),
        [](const auto& item) {
            return item.status == emberlights::MigrationItemStatus::Conflicted;
        });
    conflict->destination_ref = "silent-selection-not-allowed";
    emberlights::normalize_soundswitch_migration_report(invalid_conflict);
    CHECK(!emberlights::validate_soundswitch_migration_report(invalid_conflict));

    auto invalid_missing = make_valid_report(manifest);
    auto missing = std::find_if(invalid_missing.items.begin(), invalid_missing.items.end(),
        [](const auto& item) {
            return item.status == emberlights::MigrationItemStatus::MissingDependency;
        });
    missing->blockers.clear();
    emberlights::normalize_soundswitch_migration_report(invalid_missing);
    CHECK(!emberlights::validate_soundswitch_migration_report(invalid_missing));

    emberlights::MigrationSourceRole role{};
    CHECK(emberlights::parse_migration_source_role("conditional", role));
    CHECK(role == emberlights::MigrationSourceRole::Conditional);
    emberlights::MigrationSourceAvailability availability{};
    CHECK(emberlights::parse_migration_source_availability(
        "presentVerified", availability));
    CHECK(availability == emberlights::MigrationSourceAvailability::PresentVerified);

    std::error_code ignored;
    std::filesystem::remove_all(source, ignored);
}

void test_schema_contract_files() {
    for (const auto& path : {
             std::filesystem::path("../spec/migration/soundswitch-corpus-manifest-v1.schema.json"),
             std::filesystem::path("../spec/migration/soundswitch-migration-report-v1.schema.json")}) {
        const auto schema = read_file(path);
        CHECK(!schema.empty());
        CHECK(schema.find("https://json-schema.org/draft/2020-12/schema") !=
            std::string::npos);
        CHECK(schema.find("\"additionalProperties\": false") != std::string::npos);
        CHECK(schema.find("\"payload\"") == std::string::npos);
    }
}

}  // namespace

int main() {
    test_studio_document_service();
    test_soundswitch_corpus_manifest();
    test_soundswitch_migration_report();
    test_schema_contract_files();

    if (failures != 0) return EXIT_FAILURE;
    std::cout << "Studio foundation tests passed\n";
    return EXIT_SUCCESS;
}

