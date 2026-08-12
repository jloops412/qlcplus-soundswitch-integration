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
        audit.available_track_script_count +=
            !artifact.is_backup && artifact.kind == SoundSwitchArtifactKind::TrackScript ? 1U : 0U;
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
        return audit;
    }
    if (!audit.project_claim.present) {
        audit.status = SoundSwitchSourceBindingStatus::ProjectClaimMissing;
        audit.message = "The EmberLights project contains no SoundSwitch source identity claim.";
        return audit;
    }
    if (!audit.project_claim.valid) {
        audit.status = SoundSwitchSourceBindingStatus::ProjectClaimMalformed;
        audit.message = "The EmberLights project's SoundSwitch source identity claim is malformed or ambiguous.";
        return audit;
    }
    const auto venues = active_artifacts(
        available_source, SoundSwitchArtifactKind::VenueDatabase);
    const auto autoloops = active_artifacts(
        available_source, SoundSwitchArtifactKind::AutoloopDatabase);
    if (venues.size() != 1U || autoloops.size() != 1U) {
        audit.status = SoundSwitchSourceBindingStatus::SourceArtifactsAmbiguous;
        audit.message = "The available source does not contain exactly one active Venue and Autoloop database.";
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
           << "    \"trackScriptCount\": " << audit.available_track_script_count << ",\n"
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
           << "  }\n"
           << "}\n";
    return output.str();
}

}  // namespace emberlights
