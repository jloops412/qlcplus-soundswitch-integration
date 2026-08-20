#pragma once

#include "emberlights/autoloop_source.hpp"
#include "emberlights/project.hpp"
#include "emberlights/soundswitch_migration_ir.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace emberlights {

inline constexpr std::string_view kSoundSwitch2026ArchiveSha256 =
    "2c58ed57965cd12a0702252595d4966ef8caef4a3b024e24bc001e245fcfe11c";
inline constexpr std::string_view kSoundSwitch2026ProjectId =
    "{7CED9022-5F3E-4154-9C40-E5592BE8F145}";
inline constexpr std::string_view kSoundSwitch2026SourceVersion = "2.10.0.3";
inline constexpr std::string_view kSoundSwitch2026RedSmoothName =
    "Red - Smooth Pulse";
inline constexpr std::string_view kSoundSwitch2026RedSmoothSha256 =
    "ca82522401feee11cf8ea8710da8f8328463648da25997c7f77303dc5392f73d";
inline constexpr std::uint32_t kSoundSwitch2026MillisecondsPerBeat = 1200U;

struct SoundSwitchAutoloopCatalogEntry {
    std::uint32_t source_index{0U};
    std::uint32_t bars{0U};
    std::uint32_t bank_index{0U};
    std::string name;
    MigrationEvidenceRef evidence;
};

struct SoundSwitchAutoloopCatalogPlacement {
    std::uint32_t bank_index{0U};
    std::uint32_t slot_index{0U};
    std::uint32_t source_index{0U};
    MigrationEvidenceRef evidence;
};

struct SoundSwitchAutoloopCatalog {
    std::uint32_t database_kind{0U};
    std::vector<std::string> bank_names;
    std::vector<SoundSwitchAutoloopCatalogEntry> entries;
    std::vector<SoundSwitchAutoloopCatalogPlacement> placements;
};

struct SoundSwitchAutoloopCatalogDecode {
    bool success{false};
    std::string message;
    SoundSwitchAutoloopCatalog catalog;

    [[nodiscard]] explicit operator bool() const noexcept { return success; }
};

// Strict SoundSwitch format-3 catalog decoder. It recovers authored bank,
// placement, name, and bar length; it never treats raw entry order as slots.
[[nodiscard]] SoundSwitchAutoloopCatalogDecode
decode_soundswitch_v3_autoloop_catalog(
    std::span<const std::uint8_t> bytes,
    std::string artifact_id);

struct SoundSwitchAutoloopARecord {
    std::uint32_t record_tag{0U};
    std::uint32_t timestamp_a_ms{0U};
    std::uint32_t timestamp_b_ms{0U};
    std::uint32_t value_raw{0U};
    std::uint8_t trailing_raw{0U};
    MigrationEvidenceRef evidence;

    [[nodiscard]] float normalized_value() const noexcept;
};

struct SoundSwitchAutoloopBRecord {
    std::uint32_t record_tag{0U};
    std::uint32_t record_kind{0U};
    std::uint32_t record_version{0U};
    std::uint32_t start_ms{0U};
    std::uint32_t end_ms{0U};
    std::uint32_t rgb_start_raw{0U};
    std::uint32_t direct_start_raw{0U};
    std::uint32_t rgb_end_raw{0U};
    std::uint32_t direct_end_raw{0U};
    MigrationEvidenceRef evidence;
};

struct SoundSwitchAutoloopTargetRecords {
    std::uint32_t source_target_id{0U};
    bool present{false};
    MigrationEvidenceRef header_evidence;
    std::vector<SoundSwitchAutoloopARecord> intensity_records;
    std::vector<SoundSwitchAutoloopBRecord> color_records;
};

struct SoundSwitchAutoloopTimelineDecode {
    bool success{false};
    std::string message;
    std::vector<SoundSwitchAutoloopTargetRecords> targets;

    [[nodiscard]] explicit operator bool() const noexcept { return success; }
};

// Decodes only the caller-named target IDs. Missing blocks are returned as
// present=false; malformed or duplicate blocks fail the whole decode.
[[nodiscard]] SoundSwitchAutoloopTimelineDecode
decode_soundswitch_v3_autoloop_timeline(
    std::span<const std::uint8_t> bytes,
    std::span<const std::uint32_t> expected_target_ids,
    std::string artifact_id);

enum class SoundSwitch2026RedSmoothError : std::uint8_t {
    None,
    MissingSource,
    ReadFailed,
    UnsupportedSource,
    InvalidCatalog,
    InvalidTimeline,
    InvalidDestination,
    MergeConflict,
    InvalidProposal
};

struct SoundSwitch2026DestinationBinding {
    std::uint32_t source_target_id{0U};
    std::string destination_ref;
};

struct SoundSwitch2026RedSmoothOptions {
    // The archive identity is supplied by the read-only source-bundle audit;
    // the five extracted artifacts are independently hashed again here.
    std::string archive_sha256{std::string(kSoundSwitch2026ArchiveSha256)};
    // Empty selects the reviewed current-rig stable references.
    std::vector<SoundSwitch2026DestinationBinding> destination_bindings;
};

struct SoundSwitch2026RedSmoothProposal {
    SoundSwitch2026RedSmoothError error{
        SoundSwitch2026RedSmoothError::InvalidProposal};
    std::string message;
    bool output_disabled{true};
    std::string source_digest;
    SoundSwitchCorpusManifest corpus_manifest;
    SoundSwitchMigrationReport migration_report;
    AutoloopSourceDocument source;
    AutoloopSourceValidation source_validation;
    MigrationContractValidation manifest_validation;
    MigrationContractValidation report_validation;
    std::vector<SoundSwitchAutoloopTargetRecords> decoded_targets;

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == SoundSwitch2026RedSmoothError::None;
    }
};

struct SoundSwitch2026RedSmoothProjectResult {
    bool success{false};
    std::string message;
    bool output_disabled{true};
    ProjectDocument project;
    SoundSwitch2026RedSmoothProposal proposal;

    [[nodiscard]] explicit operator bool() const noexcept { return success; }
};

[[nodiscard]] std::vector<SoundSwitch2026DestinationBinding>
current_soundswitch_2026_destination_bindings();

// Read-only first vertical slice. This function accepts an extracted source
// directory containing exactly the five required current-2026 artifacts. It
// produces a no-output V2 proposal and never mutates either source or project.
[[nodiscard]] SoundSwitch2026RedSmoothProposal
build_soundswitch_2026_red_smooth_proposal(
    const std::filesystem::path& source_root,
    const ProjectDocument& destination_project,
    const SoundSwitch2026RedSmoothOptions& options = {});

// Product-facing first migration slice. It creates a separate current-rig
// project, removes the unrelated starter Autoloops, applies the reviewed
// source proposal through StudioDocumentService, and leaves every physical
// output disabled. The SoundSwitch directory is opened read-only.
[[nodiscard]] SoundSwitch2026RedSmoothProjectResult
create_soundswitch_2026_red_smooth_project(
    const std::filesystem::path& source_root,
    const SoundSwitch2026RedSmoothOptions& options = {});

[[nodiscard]] const char* soundswitch_2026_red_smooth_error_name(
    SoundSwitch2026RedSmoothError error) noexcept;

}  // namespace emberlights
