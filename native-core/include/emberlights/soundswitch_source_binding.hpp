#pragma once

#include "emberlights/project.hpp"
#include "emberlights/soundswitch_import.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace emberlights {

enum class SoundSwitchSourceBindingStatus : std::uint8_t {
    ExactArtifactHashMatch,
    SourceMismatch,
    ProjectClaimMissing,
    ProjectClaimMalformed,
    SourceInspectionIncomplete,
    SourceArtifactsAmbiguous
};

// This is readiness to inspect and repair a migration candidate in Studio,
// never readiness to enable live output or a claim of semantic import parity.
enum class SoundSwitchMigrationReviewState : std::uint8_t {
    ReadyForManualReview,
    SourceEvidenceBlocked,
    ProjectValidationBlocked,
    OutputMustBeDisabled
};

enum class SoundSwitchMigrationAreaState : std::uint8_t {
    Approximated,
    SourceEvidenceOnly,
    ProjectDataUnqualified,
    MissingDependency,
    NotImported
};

struct SoundSwitchMigrationAreaReview {
    std::string area_id;
    std::string label;
    SoundSwitchMigrationAreaState state{
        SoundSwitchMigrationAreaState::NotImported};
    std::size_t source_item_count{0U};
    std::size_t project_item_count{0U};
    std::string detail;
};

struct SoundSwitchProjectSourceClaim {
    bool present{false};
    bool valid{false};
    std::string version;
    std::string manifest_id;
    std::string venue_sha256;
    std::string autoloops_sha256;
    std::string conversion_strategy;
};

struct SoundSwitchSourceBindingAudit {
    std::string format{"emberlights-soundswitch-source-binding"};
    std::uint32_t format_version{1U};
    SoundSwitchSourceBindingStatus status{
        SoundSwitchSourceBindingStatus::ProjectClaimMissing};
    SoundSwitchProjectSourceClaim project_claim;
    SoundSwitchSourceKind available_source_kind{SoundSwitchSourceKind::Unknown};
    std::string available_inventory_sha256;
    std::string available_venue_sha256;
    std::string available_autoloops_sha256;
    std::size_t available_artifact_count{0U};
    std::size_t available_backup_count{0U};
    std::size_t available_venue_database_count{0U};
    std::size_t available_autoloop_database_count{0U};
    std::size_t available_track_map_count{0U};
    std::size_t available_track_script_count{0U};
    std::size_t available_recordable_data_count{0U};
    std::size_t available_autoloop_script_count{0U};
    std::size_t available_fixture_personality_count{0U};
    std::size_t available_audio_count{0U};
    std::size_t available_unknown_count{0U};
    std::size_t project_profile_count{0U};
    std::size_t project_fixture_count{0U};
    std::size_t project_look_count{0U};
    std::size_t project_autoloop_count{0U};
    std::size_t project_track_script_count{0U};
    std::size_t project_audio_asset_count{0U};
    std::size_t project_midi_mapping_count{0U};
    bool source_inspection_complete{false};
    bool exact_artifact_hash_match{false};
    // A hash match establishes identity only. Semantic import remains a
    // separately qualified claim and is deliberately false in this audit.
    bool semantic_import_qualified{false};
    std::string message;
    SoundSwitchMigrationReviewState review_state{
        SoundSwitchMigrationReviewState::SourceEvidenceBlocked};
    bool project_valid{false};
    std::size_t project_validation_error_count{0U};
    std::size_t project_validation_warning_count{0U};
    bool outputs_disabled{true};
    std::string review_headline;
    std::vector<SoundSwitchMigrationAreaReview> review_areas;
    std::vector<std::string> review_action_codes;
};

[[nodiscard]] SoundSwitchProjectSourceClaim read_soundswitch_project_source_claim(
    const ProjectDocument& project);

[[nodiscard]] SoundSwitchSourceBindingAudit audit_soundswitch_source_binding(
    const ProjectDocument& project,
    const SoundSwitchInspection& available_source);

// Adds portable evidence only (no source paths or payload bytes). Existing
// source claims and authored show content are retained unchanged.
void record_soundswitch_source_binding_evidence(
    ProjectDocument& project,
    const SoundSwitchSourceBindingAudit& audit,
    std::string_view archive_sha256 = {});

[[nodiscard]] std::string serialize_soundswitch_source_binding_audit(
    const SoundSwitchSourceBindingAudit& audit,
    std::string_view project_file,
    std::string_view project_sha256,
    std::string_view source_label,
    std::string_view archive_sha256 = {});

[[nodiscard]] const char* soundswitch_source_binding_status_name(
    SoundSwitchSourceBindingStatus status) noexcept;
[[nodiscard]] const char* soundswitch_migration_review_state_name(
    SoundSwitchMigrationReviewState state) noexcept;
[[nodiscard]] const char* soundswitch_migration_area_state_name(
    SoundSwitchMigrationAreaState state) noexcept;

}  // namespace emberlights
