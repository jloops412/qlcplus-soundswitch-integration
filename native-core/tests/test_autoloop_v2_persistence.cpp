#include "emberlights/autoloop_persistence.hpp"
#include "emberlights/file_identity.hpp"
#include "emberlights/project_io.hpp"
#include "emberlights/studio_document.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
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

[[nodiscard]] emberlights::AutoloopSourceDocument make_source(
    std::string name,
    float intensity = 0.625F) {
    emberlights::AutoloopSourceDocument source;
    emberlights::AutoloopAssetDefinition asset;
    asset.id = "persistence.asset";
    asset.name = std::move(name);
    asset.description = "Original persisted-source test content.";
    asset.tags = {"warm", "original"};
    asset.style = "test";
    asset.energy = intensity;
    asset.program_id = "persistence.program";
    asset.launch_profile_id = "persistence.launch";
    asset.provenance_id = "persistence.provenance";
    source.assets.push_back(std::move(asset));
    source.placements.push_back({
        "persistence.placement", 4U, 7U, "persistence.asset", {}});

    emberlights::AutoloopProgramDefinition program;
    program.id = "persistence.program";
    program.length_ticks = 4 * emberlights::kMusicalTicksPerQuarter;
    program.targets.push_back({
        "persistence.target",
        emberlights::AutoloopTargetKind::Master,
        {},
        {showcore::Property::Intensity}});
    program.lanes.push_back({
        "persistence.lane", "persistence.target", 0U});
    emberlights::AutoloopEventDefinition event;
    event.id = "persistence.event";
    event.lane_id = "persistence.lane";
    event.kind = emberlights::AutoloopEventKind::PropertyBlock;
    event.start_tick = 0;
    event.end_tick = program.length_ticks;
    event.property = showcore::Property::Intensity;
    event.value = showcore::PropertyValue::set(intensity);
    program.events.push_back(std::move(event));
    source.programs.push_back(std::move(program));

    emberlights::AutoloopLaunchProfileDefinition launch;
    launch.id = "persistence.launch";
    source.launch_profiles.push_back(std::move(launch));
    emberlights::AutoloopProvenanceDefinition provenance;
    provenance.id = "persistence.provenance";
    provenance.origin = emberlights::AutoloopProvenanceOrigin::Native;
    provenance.producer_id = "emberlights.persistence-test";
    provenance.producer_version = "1";
    provenance.source_object_key = "persistence.asset";
    provenance.evidence_status = "synthetic-original";
    source.provenance.push_back(std::move(provenance));
    emberlights::normalize_autoloop_source(source);
    return source;
}

[[nodiscard]] std::vector<std::string> record_fields(
    std::string_view record) {
    std::vector<std::string> fields;
    std::size_t offset = 0U;
    while (offset <= record.size()) {
        const auto tab = record.find('\t', offset);
        const auto end = tab == std::string_view::npos ? record.size() : tab;
        fields.emplace_back(record.substr(offset, end - offset));
        if (tab == std::string_view::npos) {
            break;
        }
        offset = tab + 1U;
    }
    return fields;
}

[[nodiscard]] std::string join_fields(
    const std::vector<std::string>& fields) {
    std::string record;
    for (std::size_t index = 0U; index < fields.size(); ++index) {
        if (index != 0U) {
            record.push_back('\t');
        }
        record.append(fields[index]);
    }
    return record;
}

[[nodiscard]] std::string replace_record_field(
    std::string_view record,
    std::size_t index,
    std::string value) {
    auto fields = record_fields(record);
    if (index < fields.size()) {
        fields[index] = std::move(value);
    }
    return join_fields(fields);
}

[[nodiscard]] const std::string& persisted_record(
    const emberlights::ProjectDocument& project) {
    const auto inspected =
        emberlights::inspect_persisted_autoloop_source(project);
    CHECK(inspected);
    CHECK(inspected.stamp.present);
    CHECK(inspected.record_index < project.unknown_records.size());
    if (inspected.record_index >= project.unknown_records.size()) {
        static const std::string empty;
        return empty;
    }
    return project.unknown_records[inspected.record_index];
}

void test_deterministic_record_and_unknown_preservation() {
    auto project = emberlights::make_starter_project();
    project.unknown_records = {
        "FUTURE_ALPHA\tstable-id\topaque%20bytes",
        "EMBERLIGHTS_AUTOLOOP_SOURCE_RECORD_FUTURE\tkeep-me",
        "FUTURE_OMEGA\tsecond\tvalue"};
    const auto unrelated = project.unknown_records;
    auto source = make_source("Persisted Alpha");

    const auto first = emberlights::upsert_persisted_autoloop_source(
        project, source);
    CHECK(first);
    CHECK(first.changed);
    CHECK(first.stamp.present);
    CHECK(first.stamp.record_version ==
          emberlights::kPersistedAutoloopSourceRecordVersion);
    CHECK(first.stamp.source_format_version ==
          emberlights::kAutoloopSourceFormatVersion);
    CHECK(first.stamp.source_digest ==
          emberlights::autoloop_source_digest(source));
    CHECK(project.unknown_records.size() == unrelated.size() + 1U);
    CHECK(std::equal(
        unrelated.begin(), unrelated.end(), project.unknown_records.begin()));
    const auto canonical_record = persisted_record(project);

    std::reverse(source.assets.front().tags.begin(),
                 source.assets.front().tags.end());
    const auto equivalent = emberlights::upsert_persisted_autoloop_source(
        project, source);
    CHECK(equivalent);
    CHECK(!equivalent.changed);
    CHECK(persisted_record(project) == canonical_record);

    const auto serialized = emberlights::serialize_project(project);
    emberlights::ProjectDocument reopened;
    CHECK(emberlights::parse_project(serialized, reopened));
    CHECK(reopened.unknown_records == project.unknown_records);
    const auto inspected =
        emberlights::inspect_persisted_autoloop_source(reopened);
    CHECK(inspected);
    CHECK(inspected.stamp == first.stamp);
    CHECK(emberlights::serialize_autoloop_source(inspected.source) ==
          emberlights::serialize_autoloop_source(source));
}

void test_studio_transaction_save_reopen_and_history() {
    emberlights::StudioDocumentService service;
    auto base = emberlights::make_starter_project();
    base.unknown_records = {
        "FUTURE_BEFORE\tstable\topaque",
        "FUTURE_AFTER\tstable\tsecond"};
    const auto format1_digest_before = emberlights::autoloop_source_digest(
        emberlights::adapt_format1_autoloops(base));
    CHECK(service.replace_document(
        service.generation(), base,
        emberlights::StudioDocumentBoundary::OpenedDocument));
    const auto initial = service.snapshot();
    CHECK(initial.autoloop_source);
    CHECK(!initial.autoloop_source.stamp.present);

    auto source = make_source("Studio Alpha");
    const auto applied = service.apply_autoloop_source(
        initial.generation, initial.autoloop_source.stamp, source);
    CHECK(applied.result == emberlights::StudioMutationResult::Applied);
    CHECK(service.dirty());
    auto persisted = service.snapshot();
    CHECK(persisted.autoloop_source);
    CHECK(persisted.autoloop_source.stamp.present);
    CHECK(persisted.undo_count == 1U);
    CHECK(persisted.document.unknown_records[0] == base.unknown_records[0]);
    CHECK(persisted.document.unknown_records[1] == base.unknown_records[1]);
    CHECK(emberlights::autoloop_source_digest(
        emberlights::adapt_format1_autoloops(persisted.document)) ==
        format1_digest_before);

    const auto root =
        std::filesystem::path("build/autoloop-v2-persistence-round-trip");
    const auto path = root / "project.emberlights";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    CHECK(emberlights::save_project_atomic(path, persisted.document, false));
    const auto save_generation = service.generation();
    CHECK(service.acknowledge_saved(save_generation));
    CHECK(!service.dirty());

    emberlights::ProjectDocument loaded;
    CHECK(emberlights::load_project(path, loaded, false));
    CHECK(loaded.unknown_records == persisted.document.unknown_records);
    const auto loaded_source =
        emberlights::inspect_persisted_autoloop_source(loaded);
    CHECK(loaded_source);
    CHECK(loaded_source.stamp == persisted.autoloop_source.stamp);

    emberlights::StudioDocumentService reopened_service;
    CHECK(reopened_service.replace_document(
        reopened_service.generation(), loaded,
        emberlights::StudioDocumentBoundary::OpenedDocument));
    CHECK(!reopened_service.dirty());
    CHECK(reopened_service.snapshot().autoloop_source.stamp ==
          persisted.autoloop_source.stamp);

    auto remove_bypass = reopened_service.snapshot().document;
    remove_bypass.unknown_records.erase(
        remove_bypass.unknown_records.begin() +
        static_cast<std::ptrdiff_t>(
            reopened_service.snapshot().autoloop_source.record_index));
    const auto remove_bypass_result = reopened_service.apply_candidate(
        reopened_service.generation(), std::move(remove_bypass));
    CHECK(remove_bypass_result.result ==
          emberlights::StudioMutationResult::InvalidCandidate);
    CHECK(reopened_service.snapshot().autoloop_source.stamp ==
          persisted.autoloop_source.stamp);

    source.assets.front().name = "Studio Beta";
    const auto previous_stamp =
        reopened_service.snapshot().autoloop_source.stamp;
    CHECK(reopened_service.apply_autoloop_source(
        reopened_service.generation(), previous_stamp, source));
    const auto updated = reopened_service.snapshot();
    CHECK(updated.autoloop_source.stamp.source_digest !=
          previous_stamp.source_digest);
    CHECK(updated.document.unknown_records.size() ==
          loaded.unknown_records.size());
    CHECK(updated.document.unknown_records[0] == loaded.unknown_records[0]);
    CHECK(updated.document.unknown_records[1] == loaded.unknown_records[1]);

    auto bypass = updated.document;
    auto bypass_source = source;
    bypass_source.assets.front().name = "Generic Candidate Bypass";
    CHECK(emberlights::upsert_persisted_autoloop_source(
        bypass, bypass_source));
    const auto bypass_result = reopened_service.apply_candidate(
        reopened_service.generation(), std::move(bypass));
    CHECK(bypass_result.result ==
          emberlights::StudioMutationResult::InvalidCandidate);
    CHECK(reopened_service.snapshot().autoloop_source.stamp ==
          updated.autoloop_source.stamp);

    auto wrong_version_stamp = updated.autoloop_source.stamp;
    ++wrong_version_stamp.record_version;
    const auto version_reuse = reopened_service.apply_autoloop_source(
        reopened_service.generation(), wrong_version_stamp, source);
    CHECK(version_reuse.result ==
          emberlights::StudioMutationResult::StaleGeneration);
    CHECK(reopened_service.snapshot().autoloop_source.stamp ==
          updated.autoloop_source.stamp);

    auto stale_candidate = source;
    stale_candidate.assets.front().name = "Must Not Commit";
    const auto reused_generation = reopened_service.apply_autoloop_source(
        reopened_service.generation(), previous_stamp, stale_candidate);
    CHECK(reused_generation.result ==
          emberlights::StudioMutationResult::StaleGeneration);
    CHECK(reopened_service.snapshot().autoloop_source.stamp ==
          updated.autoloop_source.stamp);

    CHECK(reopened_service.undo(reopened_service.generation()));
    CHECK(reopened_service.snapshot().autoloop_source.stamp == previous_stamp);
    CHECK(reopened_service.redo(reopened_service.generation()));
    CHECK(reopened_service.snapshot().autoloop_source.stamp ==
          updated.autoloop_source.stamp);
    std::filesystem::remove_all(root, ignored);
}

void test_generic_add_refusal_and_no_record_generation_pairing() {
    emberlights::StudioDocumentService service;
    auto initial = service.snapshot();
    CHECK(!initial.autoloop_source.stamp.present);

    auto add_bypass = initial.document;
    CHECK(emberlights::upsert_persisted_autoloop_source(
        add_bypass, make_source("Generic Add")));
    const auto add_bypass_result = service.apply_candidate(
        initial.generation, std::move(add_bypass));
    CHECK(add_bypass_result.result ==
          emberlights::StudioMutationResult::InvalidCandidate);
    CHECK(service.generation() == initial.generation);
    CHECK(!service.snapshot().autoloop_source.stamp.present);

    auto unrelated_edit = initial.document;
    unrelated_edit.name = "Unrelated generation advance";
    CHECK(service.apply_candidate(
        initial.generation, std::move(unrelated_edit)));
    CHECK(service.generation() != initial.generation);
    CHECK(!service.snapshot().autoloop_source.stamp.present);

    const auto stale_no_record = service.apply_autoloop_source(
        initial.generation,
        initial.autoloop_source.stamp,
        make_source("Stale No Record"));
    CHECK(stale_no_record.result ==
          emberlights::StudioMutationResult::StaleGeneration);
    CHECK(!service.snapshot().autoloop_source.stamp.present);

    const auto current = service.snapshot();
    CHECK(service.apply_autoloop_source(
        current.generation,
        current.autoloop_source.stamp,
        make_source("Current No Record")));
    CHECK(service.snapshot().autoloop_source.stamp.present);
}

void test_invalid_records_fail_closed() {
    auto valid = emberlights::make_starter_project();
    CHECK(emberlights::upsert_persisted_autoloop_source(
        valid, make_source("Valid")));
    const auto record = persisted_record(valid);

    auto duplicate = valid;
    duplicate.unknown_records.push_back(record);
    const auto duplicate_result =
        emberlights::inspect_persisted_autoloop_source(duplicate);
    CHECK(duplicate_result.error ==
          emberlights::AutoloopPersistenceError::DuplicateRecord);
    emberlights::ProjectDocument parsed;
    CHECK(emberlights::parse_project(
              emberlights::serialize_project(duplicate), parsed).error ==
          emberlights::ProjectIoError::InvalidRecord);

    auto malformed = valid;
    malformed.unknown_records.back() = "EMBERLIGHTS_AUTOLOOP_SOURCE_RECORD\t1";
    CHECK(emberlights::inspect_persisted_autoloop_source(malformed).error ==
          emberlights::AutoloopPersistenceError::MalformedRecord);

    auto record_version = valid;
    record_version.unknown_records.back() =
        replace_record_field(record, 1U, "2");
    CHECK(emberlights::inspect_persisted_autoloop_source(record_version).error ==
          emberlights::AutoloopPersistenceError::UnsupportedRecordVersion);

    auto noncanonical_version = valid;
    noncanonical_version.unknown_records.back() =
        replace_record_field(record, 1U, "01");
    CHECK(emberlights::inspect_persisted_autoloop_source(
              noncanonical_version).error ==
          emberlights::AutoloopPersistenceError::MalformedRecord);

    auto source_version = valid;
    source_version.unknown_records.back() =
        replace_record_field(record, 2U, "2");
    CHECK(emberlights::inspect_persisted_autoloop_source(source_version).error ==
          emberlights::AutoloopPersistenceError::UnsupportedSourceVersion);

    auto oversized = valid;
    oversized.unknown_records.back() = replace_record_field(
        record, 3U,
        std::to_string(
            emberlights::kMaximumPersistedAutoloopSourceBytes + 1U));
    CHECK(emberlights::inspect_persisted_autoloop_source(oversized).error ==
          emberlights::AutoloopPersistenceError::SourceTooLarge);

    auto digest_mismatch = valid;
    digest_mismatch.unknown_records.back() =
        replace_record_field(record, 4U, std::string(64U, '0'));
    CHECK(emberlights::inspect_persisted_autoloop_source(digest_mismatch).error ==
          emberlights::AutoloopPersistenceError::DigestMismatch);

    auto encoded_kind = valid;
    encoded_kind.unknown_records.back().replace(0U, 1U, "%45");
    CHECK(emberlights::inspect_persisted_autoloop_source(encoded_kind).error ==
          emberlights::AutoloopPersistenceError::MalformedRecord);

    auto embedded = emberlights::make_starter_project();
    embedded.unknown_records.push_back(
        "FUTURE_KEEP\topaque\n" + record);
    CHECK(emberlights::inspect_persisted_autoloop_source(embedded).error ==
          emberlights::AutoloopPersistenceError::MalformedRecord);

    emberlights::StudioDocumentService service;
    const auto generation = service.generation();
    const auto rejected = service.replace_document(
        generation, digest_mismatch,
        emberlights::StudioDocumentBoundary::OpenedDocument);
    CHECK(rejected.result == emberlights::StudioMutationResult::InvalidCandidate);
    CHECK(rejected.persistence_error ==
          emberlights::AutoloopPersistenceError::DigestMismatch);
    CHECK(service.generation() == generation);

    const auto root =
        std::filesystem::path("build/autoloop-v2-persistence-invalid");
    const auto path = root / "must-not-exist.emberlights";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    const auto save = emberlights::save_project_atomic(
        path, digest_mismatch, false);
    CHECK(save.error == emberlights::ProjectIoError::InvalidRecord);
    CHECK(!std::filesystem::exists(path));
    std::filesystem::remove_all(root, ignored);
}

void test_payload_digest_is_canonical_not_caller_supplied() {
    auto project = emberlights::make_starter_project();
    auto source = make_source("Canonical");
    const auto original = emberlights::upsert_persisted_autoloop_source(
        project, source);
    CHECK(original);

    auto fields = record_fields(persisted_record(project));
    CHECK(fields.size() == 6U);
    if (fields.size() != 6U) {
        return;
    }
    fields[5].replace(0U, 2U, fields[5].substr(0U, 2U) == "00" ? "01" : "00");
    std::string altered_payload;
    altered_payload.reserve(fields[5].size() / 2U);
    for (std::size_t index = 0U; index + 1U < fields[5].size(); index += 2U) {
        const auto byte_text = std::string_view(fields[5]).substr(index, 2U);
        unsigned int byte = 0U;
        const auto conversion = std::from_chars(
            byte_text.data(), byte_text.data() + byte_text.size(), byte, 16);
        CHECK(conversion.ec == std::errc{});
        altered_payload.push_back(static_cast<char>(byte));
    }
    fields[4] = emberlights::sha256_text(altered_payload);
    auto tampered = project;
    tampered.unknown_records.back() = join_fields(fields);
    const auto inspected =
        emberlights::inspect_persisted_autoloop_source(tampered);
    CHECK(inspected.error == emberlights::AutoloopPersistenceError::InvalidSource ||
          inspected.error ==
              emberlights::AutoloopPersistenceError::NonCanonicalSource);
    CHECK(inspected.stamp.source_digest.empty());
}

}  // namespace

int main() {
    test_deterministic_record_and_unknown_preservation();
    test_studio_transaction_save_reopen_and_history();
    test_generic_add_refusal_and_no_record_generation_pairing();
    test_invalid_records_fail_closed();
    test_payload_digest_is_canonical_not_caller_supplied();

    if (failures != 0) {
        std::cerr << failures << " Autoloop V2 persistence test(s) failed\n";
        return 1;
    }
    std::cout << "Autoloop V2 persistence tests passed\n";
    return 0;
}
