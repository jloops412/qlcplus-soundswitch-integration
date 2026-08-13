#include "emberlights/soundswitch_source_binding.hpp"

#include "emberlights/file_identity.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <vector>

namespace emberlights {
namespace {

[[nodiscard]] std::vector<std::string_view> split_tabs(std::string_view record) {
    std::vector<std::string_view> fields;
    while (true) {
        const auto separator = record.find('\t');
        fields.push_back(record.substr(0U, separator));
        if (separator == std::string_view::npos) {
            return fields;
        }
        record.remove_prefix(separator + 1U);
    }
}

[[nodiscard]] std::string json_escape(std::string_view value) {
    std::ostringstream output;
    for (const auto character : value) {
        switch (character) {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (static_cast<unsigned char>(character) < 0x20U) {
                output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                       << static_cast<unsigned int>(
                              static_cast<unsigned char>(character));
            } else {
                output << character;
            }
        }
    }
    return output.str();
}

[[nodiscard]] std::vector<const SoundSwitchArtifact*> active_artifacts(
    const SoundSwitchInspection& inspection,
    SoundSwitchArtifactKind kind) {
    std::vector<const SoundSwitchArtifact*> matches;
    for (const auto& artifact : inspection.artifacts) {
        if (!artifact.is_backup && artifact.kind == kind) {
            matches.push_back(&artifact);
        }
    }
    return matches;
}

[[nodiscard]] bool has_exact_record(
    const ProjectDocument& project,
    std::string_view record) {
    return std::find(project.unknown_records.begin(), project.unknown_records.end(), record) !=
        project.unknown_records.end();
}

void add_record_once(ProjectDocument& project, std::string record) {
    if (!has_exact_record(project, record)) {
        project.unknown_records.push_back(std::move(record));
    }
}

[[nodiscard]] bool project_outputs_disabled(
    const ProjectDocument& project) noexcept {
    return !project.connections.artnet_enabled &&
        !project.connections.sacn_enabled &&
        project.connections.soundswitch_micro_universe == 0U &&
        !project.connections.soundswitch_control_one_experimental &&
        std::all_of(
            project.connections.dmx_usb_pro_ports.begin(),
            project.connections.dmx_usb_pro_ports.end(),
            [](const auto& port) { return port.empty(); });
}

void add_action_once(SoundSwitchSourceBindingAudit& audit, std::string action) {
    if (std::find(
            audit.review_action_codes.begin(),
            audit.review_action_codes.end(),
            action) == audit.review_action_codes.end()) {
        audit.review_action_codes.push_back(std::move(action));
    }
}

void add_review_area(
    SoundSwitchSourceBindingAudit& audit,
    std::string id,
    std::string label,
    SoundSwitchMigrationAreaState state,
    std::size_t source_items,
    std::size_t project_items,
    std::string detail) {
    audit.review_areas.push_back({
        std::move(id),
        std::move(label),
        state,
        source_items,
        project_items,
        std::move(detail)});
}

[[nodiscard]] SoundSwitchMigrationAreaState unqualified_area_state(
    std::size_t source_items,
    std::size_t project_items,
    bool missing_is_dependency = false) noexcept {
    if (project_items != 0U) {
        return SoundSwitchMigrationAreaState::ProjectDataUnqualified;
    }
    if (source_items != 0U) {
        return SoundSwitchMigrationAreaState::SourceEvidenceOnly;
    }
    return missing_is_dependency
        ? SoundSwitchMigrationAreaState::MissingDependency
        : SoundSwitchMigrationAreaState::NotImported;
}

[[nodiscard]] std::string unqualified_detail(
    SoundSwitchMigrationAreaState state) {
    switch (state) {
    case SoundSwitchMigrationAreaState::ProjectDataUnqualified:
        return "Project data exists, but no supported per-object provenance establishes it as a semantic SoundSwitch import.";
    case SoundSwitchMigrationAreaState::SourceEvidenceOnly:
        return "Source artifacts were inventoried by hash, but no corresponding project objects are semantically qualified.";
    case SoundSwitchMigrationAreaState::MissingDependency:
        return "The source evidence needed to recover this area is unavailable.";
    case SoundSwitchMigrationAreaState::NotImported:
        return "No project objects or supported source evidence were found for this area.";
    case SoundSwitchMigrationAreaState::Approximated:
        break;
    }
    return "This area is an explicitly labeled approximation.";
}

void populate_migration_review(
    const ProjectDocument& project,
    SoundSwitchSourceBindingAudit& audit) {
    const auto validation = validate_project(project);
    audit.project_valid = validation.ok();
    audit.project_validation_error_count = validation.error_count();
    audit.project_validation_warning_count = validation.warning_count();
    audit.outputs_disabled = project_outputs_disabled(project);
    audit.review_areas.clear();
    audit.review_action_codes.clear();

    const bool recognized_pilot = audit.project_claim.valid &&
        audit.project_claim.conversion_strategy == "semantic-v1-safe-patch";
    const auto venue_evidence = audit.available_venue_database_count;
    const auto autoloop_evidence = audit.available_autoloop_database_count +
        audit.available_autoloop_script_count;
    const auto track_evidence = audit.available_track_script_count +
        audit.available_recordable_data_count;

    const auto profiles_state = recognized_pilot && audit.project_profile_count != 0U
        ? SoundSwitchMigrationAreaState::Approximated
        : unqualified_area_state(
              audit.available_fixture_personality_count,
              audit.project_profile_count);
    add_review_area(
        audit,
        "fixtureProfiles",
        "Fixture profiles",
        profiles_state,
        audit.available_fixture_personality_count,
        audit.project_profile_count,
        profiles_state == SoundSwitchMigrationAreaState::Approximated
            ? "The pilot converter supplied built-in/manual-backed profile data; it did not decode source personalities, and format 1 cannot distinguish later local edits."
            : unqualified_detail(profiles_state));

    const auto patch_state = recognized_pilot && audit.project_fixture_count != 0U
        ? SoundSwitchMigrationAreaState::Approximated
        : unqualified_area_state(venue_evidence, audit.project_fixture_count);
    add_review_area(
        audit,
        "fixturePatch",
        "Fixture patch",
        patch_state,
        venue_evidence,
        audit.project_fixture_count,
        patch_state == SoundSwitchMigrationAreaState::Approximated
            ? "Fixture identities and addresses are a safe non-overlapping staging layout, not decoded SoundSwitch patch values."
            : unqualified_detail(patch_state));

    const auto looks_state = recognized_pilot && audit.project_look_count != 0U
        ? SoundSwitchMigrationAreaState::Approximated
        : unqualified_area_state(venue_evidence, audit.project_look_count);
    add_review_area(
        audit,
        "staticLooks",
        "Static Looks",
        looks_state,
        venue_evidence,
        audit.project_look_count,
        looks_state == SoundSwitchMigrationAreaState::Approximated
            ? "The pilot Looks are purpose-built EmberLights content, not decoded source Static Look values."
            : unqualified_detail(looks_state));

    const auto autoloops_state = recognized_pilot && audit.project_autoloop_count != 0U
        ? SoundSwitchMigrationAreaState::Approximated
        : unqualified_area_state(autoloop_evidence, audit.project_autoloop_count);
    add_review_area(
        audit,
        "autoloops",
        "Autoloops",
        autoloops_state,
        autoloop_evidence,
        audit.project_autoloop_count,
        autoloops_state == SoundSwitchMigrationAreaState::Approximated
            ? "Active names were retained when available; patterns were rebuilt from names rather than decoded from SoundSwitch cues."
            : unqualified_detail(autoloops_state));

    const auto tracks_state = unqualified_area_state(
        track_evidence, audit.project_track_script_count, true);
    add_review_area(
        audit,
        "trackScripts",
        "Scripted tracks",
        tracks_state,
        track_evidence,
        audit.project_track_script_count,
        unqualified_detail(tracks_state));

    const auto audio_state = unqualified_area_state(
        audit.available_audio_count, audit.project_audio_asset_count, true);
    add_review_area(
        audit,
        "scriptedAudio",
        "Scripted audio",
        audio_state,
        audit.available_audio_count,
        audit.project_audio_asset_count,
        unqualified_detail(audio_state));

    const auto midi_state = unqualified_area_state(
        0U, audit.project_midi_mapping_count);
    add_review_area(
        audit,
        "midiMappings",
        "MIDI mappings",
        midi_state,
        0U,
        audit.project_midi_mapping_count,
        unqualified_detail(midi_state));

    add_action_once(audit, "migration.keep_source_bundle");
    if (!audit.outputs_disabled) {
        add_action_once(audit, "migration.disable_output_before_review");
    }
    if (!audit.project_valid) {
        add_action_once(audit, "migration.fix_project_validation");
    }
    if (audit.status != SoundSwitchSourceBindingStatus::ExactArtifactHashMatch) {
        add_action_once(audit, "migration.provide_matching_source");
    }
    if (recognized_pilot) {
        add_action_once(audit, "migration.review_fixture_patch");
        add_action_once(audit, "migration.review_static_looks");
        add_action_once(audit, "migration.review_autoloops");
    }
    if (track_evidence != 0U || audit.project_track_script_count == 0U) {
        add_action_once(audit, "migration.review_scripted_tracks");
    }
    if (audit.available_audio_count != 0U || audit.project_audio_asset_count == 0U) {
        add_action_once(audit, "migration.review_scripted_audio");
    }

    if (!audit.outputs_disabled) {
        audit.review_state = SoundSwitchMigrationReviewState::OutputMustBeDisabled;
        audit.review_headline = "One or more DMX outputs are enabled. Disable output before reviewing or repairing this migration candidate.";
    } else if (!audit.project_valid) {
        audit.review_state = SoundSwitchMigrationReviewState::ProjectValidationBlocked;
        audit.review_headline = "The EmberLights project does not currently pass validation; repair it before relying on migration counts.";
    } else if (audit.status !=
               SoundSwitchSourceBindingStatus::ExactArtifactHashMatch) {
        audit.review_state = SoundSwitchMigrationReviewState::SourceEvidenceBlocked;
        audit.review_headline = "Matching source identity is not verified. Project data remains inspectable, but its SoundSwitch origin is not established.";
    } else {
        audit.review_state = SoundSwitchMigrationReviewState::ReadyForManualReview;
        audit.review_headline = "Matching source identity is verified. Review every approximate or missing area; semantic import is still not qualified.";
    }
}

}  // namespace

const char* soundswitch_source_binding_status_name(
    SoundSwitchSourceBindingStatus status) noexcept {
    switch (status) {
    case SoundSwitchSourceBindingStatus::ExactArtifactHashMatch:
        return "exactArtifactHashMatch";
    case SoundSwitchSourceBindingStatus::SourceMismatch: return "sourceMismatch";
    case SoundSwitchSourceBindingStatus::ProjectClaimMissing: return "projectClaimMissing";
    case SoundSwitchSourceBindingStatus::ProjectClaimMalformed: return "projectClaimMalformed";
    case SoundSwitchSourceBindingStatus::SourceInspectionIncomplete:
        return "sourceInspectionIncomplete";
    case SoundSwitchSourceBindingStatus::SourceArtifactsAmbiguous:
        return "sourceArtifactsAmbiguous";
    }
    return "projectClaimMissing";
}

const char* soundswitch_migration_review_state_name(
    SoundSwitchMigrationReviewState state) noexcept {
    switch (state) {
    case SoundSwitchMigrationReviewState::ReadyForManualReview:
        return "readyForManualReview";
    case SoundSwitchMigrationReviewState::SourceEvidenceBlocked:
        return "sourceEvidenceBlocked";
    case SoundSwitchMigrationReviewState::ProjectValidationBlocked:
        return "projectValidationBlocked";
    case SoundSwitchMigrationReviewState::OutputMustBeDisabled:
        return "outputMustBeDisabled";
    }
    return "sourceEvidenceBlocked";
}

const char* soundswitch_migration_area_state_name(
    SoundSwitchMigrationAreaState state) noexcept {
    switch (state) {
    case SoundSwitchMigrationAreaState::Approximated: return "approximated";
    case SoundSwitchMigrationAreaState::SourceEvidenceOnly:
        return "sourceEvidenceOnly";
    case SoundSwitchMigrationAreaState::ProjectDataUnqualified:
        return "projectDataUnqualified";
    case SoundSwitchMigrationAreaState::MissingDependency:
        return "missingDependency";
    case SoundSwitchMigrationAreaState::NotImported: return "notImported";
    }
    return "notImported";
}

SoundSwitchProjectSourceClaim read_soundswitch_project_source_claim(
    const ProjectDocument& project) {
    SoundSwitchProjectSourceClaim claim;
    std::size_t claims = 0U;
    for (const auto& record : project.unknown_records) {
        if (!record.starts_with("SOUNDSWITCH_SOURCE\t")) {
            continue;
        }
        ++claims;
        const auto fields = split_tabs(record);
        if (fields.size() != 6U) {
            continue;
        }
        claim.present = true;
        claim.version = fields[1U];
        claim.manifest_id = fields[2U];
        claim.venue_sha256 = fields[3U];
        claim.autoloops_sha256 = fields[4U];
        claim.conversion_strategy = fields[5U];
        claim.valid = is_sha256_digest(claim.venue_sha256) &&
            is_sha256_digest(claim.autoloops_sha256) &&
            !claim.version.empty() && !claim.conversion_strategy.empty();
    }
    if (claims > 1U) {
        claim.present = true;
        claim.valid = false;
    }
    return claim;
}

SoundSwitchSourceBindingAudit audit_soundswitch_source_binding(
    const ProjectDocument& project,
    const SoundSwitchInspection& available_source) {
    SoundSwitchSourceBindingAudit audit;
    audit.project_claim = read_soundswitch_project_source_claim(project);
    audit.available_source_kind = available_source.source_kind;
    audit.available_inventory_sha256 = available_source.inventory_sha256;
    audit.available_artifact_count = available_source.artifacts.size();
    audit.source_inspection_complete = available_source.complete();
    audit.project_profile_count = project.fixture_profiles.size();
    audit.project_fixture_count = project.fixtures.size();
    audit.project_look_count = project.looks.size();
    audit.project_autoloop_count = project.autoloops.size();
    audit.project_track_script_count = project.track_scripts.size();
    audit.project_audio_asset_count = project.audio_assets.size();
    audit.project_midi_mapping_count = project.midi_mappings.size();
    for (const auto& artifact : available_source.artifacts) {
        audit.available_backup_count += artifact.is_backup ? 1U : 0U;
        audit.available_venue_database_count +=
            !artifact.is_backup && artifact.kind == SoundSwitchArtifactKind::VenueDatabase ? 1U : 0U;
        audit.available_autoloop_database_count +=
            !artifact.is_backup &&
                (artifact.kind == SoundSwitchArtifactKind::AutoloopDatabase ||
                 artifact.kind == SoundSwitchArtifactKind::ExtendedAutoloopDatabase)
            ? 1U : 0U;
        audit.available_track_map_count +=
            !artifact.is_backup && artifact.kind == SoundSwitchArtifactKind::TrackMap ? 1U : 0U;
        audit.available_track_script_count +=
            !artifact.is_backup && artifact.kind == SoundSwitchArtifactKind::TrackScript ? 1U : 0U;
        audit.available_recordable_data_count +=
            !artifact.is_backup && artifact.kind == SoundSwitchArtifactKind::RecordableData ? 1U : 0U;
        audit.available_autoloop_script_count +=
            !artifact.is_backup && artifact.kind == SoundSwitchArtifactKind::AutoloopScript ? 1U : 0U;
        audit.available_fixture_personality_count +=
            !artifact.is_backup && artifact.kind == SoundSwitchArtifactKind::FixturePersonality ? 1U : 0U;
        audit.available_audio_count +=
            !artifact.is_backup && artifact.kind == SoundSwitchArtifactKind::Audio ? 1U : 0U;
        audit.available_unknown_count +=
            artifact.kind == SoundSwitchArtifactKind::Unknown ? 1U : 0U;
    }

    if (!audit.source_inspection_complete) {
        audit.status = SoundSwitchSourceBindingStatus::SourceInspectionIncomplete;
        audit.message = "The available SoundSwitch source did not pass bounded read-only inspection.";
        populate_migration_review(project, audit);
        return audit;
    }
    if (!audit.project_claim.present) {
        audit.status = SoundSwitchSourceBindingStatus::ProjectClaimMissing;
        audit.message = "The EmberLights project contains no SoundSwitch source identity claim.";
        populate_migration_review(project, audit);
        return audit;
    }
    if (!audit.project_claim.valid) {
        audit.status = SoundSwitchSourceBindingStatus::ProjectClaimMalformed;
        audit.message = "The EmberLights project's SoundSwitch source identity claim is malformed or ambiguous.";
        populate_migration_review(project, audit);
        return audit;
    }
    const auto venues = active_artifacts(
        available_source, SoundSwitchArtifactKind::VenueDatabase);
    const auto autoloops = active_artifacts(
        available_source, SoundSwitchArtifactKind::AutoloopDatabase);
    if (venues.size() != 1U || autoloops.size() != 1U) {
        audit.status = SoundSwitchSourceBindingStatus::SourceArtifactsAmbiguous;
        audit.message = "The available source does not contain exactly one active Venue and Autoloop database.";
        populate_migration_review(project, audit);
        return audit;
    }
    audit.available_venue_sha256 = venues.front()->sha256;
    audit.available_autoloops_sha256 = autoloops.front()->sha256;
    audit.exact_artifact_hash_match =
        audit.project_claim.venue_sha256 == audit.available_venue_sha256 &&
        audit.project_claim.autoloops_sha256 == audit.available_autoloops_sha256;
    if (audit.exact_artifact_hash_match) {
        audit.status = SoundSwitchSourceBindingStatus::ExactArtifactHashMatch;
        audit.message = "The available Venue and Autoloop database hashes match the project's source claim. Semantic coverage still requires separate qualification.";
    } else {
        audit.status = SoundSwitchSourceBindingStatus::SourceMismatch;
        audit.message = "The available SoundSwitch source hashes do not match the source claimed by this EmberLights project; semantic import is not qualified.";
    }
    populate_migration_review(project, audit);
    return audit;
}

void record_soundswitch_source_binding_evidence(
    ProjectDocument& project,
    const SoundSwitchSourceBindingAudit& audit,
    std::string_view archive_sha256) {
    add_record_once(
        project,
        "SOUNDSWITCH_AVAILABLE_SOURCE\t" +
            std::string(soundswitch_source_kind_name(audit.available_source_kind)) + "\t" +
            audit.available_inventory_sha256 + "\t" +
            audit.available_venue_sha256 + "\t" +
            audit.available_autoloops_sha256 + "\tpreserved-external");
    add_record_once(
        project,
        "SOUNDSWITCH_SOURCE_BINDING_AUDIT\t" +
            std::string(soundswitch_source_binding_status_name(audit.status)) +
            "\tsemantic-import-unqualified");
    if (!archive_sha256.empty()) {
        add_record_once(
            project,
            "SOUNDSWITCH_ARCHIVE_SHA256\t" + std::string(archive_sha256));
    }
}

std::string serialize_soundswitch_source_binding_audit(
    const SoundSwitchSourceBindingAudit& audit,
    std::string_view project_file,
    std::string_view project_sha256,
    std::string_view source_label,
    std::string_view archive_sha256) {
    std::ostringstream output;
    output << "{\n"
           << "  \"format\": \"" << audit.format << "\",\n"
           << "  \"formatVersion\": " << audit.format_version << ",\n"
           << "  \"status\": \""
           << soundswitch_source_binding_status_name(audit.status) << "\",\n"
           << "  \"message\": \"" << json_escape(audit.message) << "\",\n"
           << "  \"projectFile\": \"" << json_escape(project_file) << "\",\n"
           << "  \"projectSha256\": \"" << json_escape(project_sha256) << "\",\n"
           << "  \"sourceLabel\": \"" << json_escape(source_label) << "\",\n"
           << "  \"sourceArchiveSha256\": \"" << json_escape(archive_sha256) << "\",\n"
           << "  \"sourceInspectionComplete\": "
           << (audit.source_inspection_complete ? "true" : "false") << ",\n"
           << "  \"exactArtifactHashMatch\": "
           << (audit.exact_artifact_hash_match ? "true" : "false") << ",\n"
           << "  \"semanticImportQualified\": false,\n"
           << "  \"projectClaim\": {\n"
           << "    \"present\": " << (audit.project_claim.present ? "true" : "false") << ",\n"
           << "    \"valid\": " << (audit.project_claim.valid ? "true" : "false") << ",\n"
           << "    \"version\": \"" << json_escape(audit.project_claim.version) << "\",\n"
           << "    \"manifestId\": \"" << json_escape(audit.project_claim.manifest_id) << "\",\n"
           << "    \"venueSha256\": \"" << json_escape(audit.project_claim.venue_sha256) << "\",\n"
           << "    \"autoloopsSha256\": \"" << json_escape(audit.project_claim.autoloops_sha256) << "\",\n"
           << "    \"conversionStrategy\": \""
           << json_escape(audit.project_claim.conversion_strategy) << "\"\n"
           << "  },\n"
           << "  \"availableSource\": {\n"
           << "    \"kind\": \""
           << soundswitch_source_kind_name(audit.available_source_kind) << "\",\n"
           << "    \"inventorySha256\": \""
           << json_escape(audit.available_inventory_sha256) << "\",\n"
           << "    \"venueSha256\": \""
           << json_escape(audit.available_venue_sha256) << "\",\n"
           << "    \"autoloopsSha256\": \""
           << json_escape(audit.available_autoloops_sha256) << "\",\n"
           << "    \"artifactCount\": " << audit.available_artifact_count << ",\n"
           << "    \"backupCount\": " << audit.available_backup_count << ",\n"
           << "    \"venueDatabaseCount\": "
           << audit.available_venue_database_count << ",\n"
           << "    \"autoloopDatabaseCount\": "
           << audit.available_autoloop_database_count << ",\n"
           << "    \"trackMapCount\": " << audit.available_track_map_count << ",\n"
           << "    \"trackScriptCount\": " << audit.available_track_script_count << ",\n"
           << "    \"recordableDataCount\": "
           << audit.available_recordable_data_count << ",\n"
           << "    \"autoloopScriptCount\": " << audit.available_autoloop_script_count << ",\n"
           << "    \"fixturePersonalityCount\": "
           << audit.available_fixture_personality_count << ",\n"
           << "    \"audioCount\": " << audit.available_audio_count << ",\n"
           << "    \"unknownCount\": " << audit.available_unknown_count << "\n"
           << "  },\n"
           << "  \"coverage\": {\n"
           << "    \"sourceIdentity\": \""
           << (audit.exact_artifact_hash_match ? "exactArtifactHashMatch" : "mismatch")
           << "\",\n"
           << "    \"opaquePreservation\": \""
           << (audit.source_inspection_complete
                   ? "completeExternalInventory"
                   : "incomplete")
           << "\",\n"
           << "    \"fixtureProfiles\": {\"sourcePersonalities\": "
           << audit.available_fixture_personality_count
           << ", \"projectProfiles\": " << audit.project_profile_count
           << ", \"status\": \"notSourceBound\"},\n"
           << "    \"fixturePatch\": {\"projectFixtures\": "
           << audit.project_fixture_count
           << ", \"status\": \"notSemanticallyDecoded\"},\n"
           << "    \"staticLooks\": {\"projectLooks\": "
           << audit.project_look_count
           << ", \"status\": \"notSemanticallyDecoded\"},\n"
           << "    \"autoloops\": {\"sourceScripts\": "
           << audit.available_autoloop_script_count
           << ", \"projectAutoloops\": " << audit.project_autoloop_count
           << ", \"status\": \"notSemanticallyDecoded\"},\n"
           << "    \"trackScripts\": {\"sourceScripts\": "
           << audit.available_track_script_count
           << ", \"projectTrackScripts\": " << audit.project_track_script_count
           << ", \"status\": \"notSemanticallyDecoded\"},\n"
           << "    \"audio\": {\"sourceMediaPayloads\": "
           << audit.available_audio_count
           << ", \"projectAudioAssets\": " << audit.project_audio_asset_count
           << ", \"status\": \"notSemanticallyDecoded\"},\n"
           << "    \"midiMappings\": {\"projectMappings\": "
           << audit.project_midi_mapping_count
           << ", \"status\": \"notSemanticallyDecoded\"}\n"
           << "  },\n"
           << "  \"review\": {\n"
           << "    \"state\": \""
           << soundswitch_migration_review_state_name(audit.review_state)
           << "\",\n"
           << "    \"headline\": \"" << json_escape(audit.review_headline)
           << "\",\n"
           << "    \"projectValid\": "
           << (audit.project_valid ? "true" : "false") << ",\n"
           << "    \"projectValidationErrors\": "
           << audit.project_validation_error_count << ",\n"
           << "    \"projectValidationWarnings\": "
           << audit.project_validation_warning_count << ",\n"
           << "    \"outputsDisabled\": "
           << (audit.outputs_disabled ? "true" : "false") << ",\n"
           << "    \"sourceIdentityVerified\": "
           << (audit.exact_artifact_hash_match ? "true" : "false") << ",\n"
           << "    \"semanticImportQualified\": false,\n"
           << "    \"areas\": [";
    for (std::size_t index = 0U; index < audit.review_areas.size(); ++index) {
        const auto& area = audit.review_areas[index];
        output << (index == 0U ? "\n" : ",\n")
               << "      {\"id\": \"" << json_escape(area.area_id)
               << "\", \"label\": \"" << json_escape(area.label)
               << "\", \"state\": \""
               << soundswitch_migration_area_state_name(area.state)
               << "\", \"sourceItems\": " << area.source_item_count
               << ", \"projectItems\": " << area.project_item_count
               << ", \"detail\": \"" << json_escape(area.detail) << "\"}";
    }
    if (!audit.review_areas.empty()) output << '\n';
    output << "    ],\n"
           << "    \"actionCodes\": [";
    for (std::size_t index = 0U; index < audit.review_action_codes.size(); ++index) {
        output << (index == 0U ? "" : ", ") << "\""
               << json_escape(audit.review_action_codes[index]) << "\"";
    }
    output << "]\n"
           << "  }\n"
           << "}\n";
    return output.str();
}

}  // namespace emberlights
