#pragma once

#include "emberlights/soundswitch_import.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace emberlights {

inline constexpr std::uint32_t kSoundSwitchCorpusManifestFormatVersion = 1U;
inline constexpr std::uint32_t kSoundSwitchMigrationReportFormatVersion = 1U;
inline constexpr std::string_view kSoundSwitchCorpusManifestFormat =
    "emberlights-soundswitch-corpus-manifest";
inline constexpr std::string_view kSoundSwitchMigrationReportFormat =
    "emberlights-soundswitch-migration-report";
inline constexpr std::size_t kMaximumMigrationArtifacts = 100000U;
inline constexpr std::size_t kMaximumMigrationItems = 100000U;
inline constexpr std::size_t kMaximumMigrationEvidencePerItem = 256U;
inline constexpr std::size_t kMaximumMigrationMessagesPerItem = 256U;

enum class MigrationSourceRole : std::uint8_t {
    Required,
    Conditional,
    Optional
};

enum class MigrationSourceAvailability : std::uint8_t {
    PresentVerified,
    Missing,
    Unreadable,
    RejectedUnsafe
};

enum class SoundSwitchMigrationScope : std::uint8_t {
    ProjectOnly,
    ScriptedTracks
};

struct MigrationSourceArtifact {
    std::string artifact_id;
    std::string relative_path;
    SoundSwitchArtifactKind kind{SoundSwitchArtifactKind::Unknown};
    std::uint64_t size{0U};
    std::string sha256;
    MigrationSourceRole role{MigrationSourceRole::Optional};
    MigrationSourceAvailability availability{
        MigrationSourceAvailability::PresentVerified};
};

struct SoundSwitchCorpusManifest {
    std::uint32_t format_version{kSoundSwitchCorpusManifestFormatVersion};
    std::string bundle_id;
    std::string source_version;
    std::vector<MigrationSourceArtifact> artifacts;
    std::vector<std::string> missing_dependency_codes;
};

enum class MigrationItemStatus : std::uint8_t {
    Exact,
    DeterministicallyTranslated,
    Approximated,
    PreservedOpaque,
    Unsupported,
    Conflicted,
    MissingDependency,
    RejectedUnsafe,
    Count
};

struct MigrationEvidenceRef {
    std::string artifact_id;
    std::uint64_t offset{0U};
    std::uint64_t length{0U};
    bool has_byte_range{false};
    std::string decoder_id;
    std::string decoder_version;
};

struct MigrationItem {
    std::string item_id;
    std::string item_kind;
    MigrationItemStatus status{MigrationItemStatus::Unsupported};
    std::string source_label;
    std::string destination_ref;
    std::string rule_id;
    std::vector<MigrationEvidenceRef> evidence;
    std::vector<std::string> warnings;
    std::vector<std::string> blockers;
};

struct MigrationStatusCounts {
    std::array<std::uint32_t,
        static_cast<std::size_t>(MigrationItemStatus::Count)> by_status{};

    [[nodiscard]] std::uint32_t total() const noexcept;
    [[nodiscard]] std::uint32_t count(MigrationItemStatus status) const noexcept;
};

struct SoundSwitchMigrationReport {
    std::uint32_t format_version{kSoundSwitchMigrationReportFormatVersion};
    std::string source_bundle_id;
    std::string source_version;
    std::vector<MigrationItem> items;
    MigrationStatusCounts aggregate_counts;
};

struct MigrationContractValidation {
    std::vector<std::string> errors;

    [[nodiscard]] bool ok() const noexcept { return errors.empty(); }
    [[nodiscard]] explicit operator bool() const noexcept { return ok(); }
};

// Builds a portable manifest from the existing read-only inspector. Passing
// false for authorized_corpus_available produces the explicit unavailable
// dependency result and never invents artifacts or source semantics.
[[nodiscard]] SoundSwitchCorpusManifest build_soundswitch_corpus_manifest(
    const SoundSwitchInspection& inspection,
    std::string source_version,
    SoundSwitchMigrationScope scope = SoundSwitchMigrationScope::ProjectOnly,
    bool authorized_corpus_available = true);

[[nodiscard]] MigrationContractValidation validate_soundswitch_corpus_manifest(
    const SoundSwitchCorpusManifest& manifest);
[[nodiscard]] MigrationContractValidation validate_soundswitch_migration_report(
    const SoundSwitchMigrationReport& report);

// Normalization is deterministic and content-safe: it sorts records and
// messages, removes duplicate messages, and recomputes aggregate counts.
void normalize_soundswitch_corpus_manifest(SoundSwitchCorpusManifest& manifest);
void normalize_soundswitch_migration_report(SoundSwitchMigrationReport& report);

[[nodiscard]] std::string serialize_soundswitch_corpus_manifest(
    const SoundSwitchCorpusManifest& manifest);
[[nodiscard]] std::string serialize_soundswitch_migration_report(
    const SoundSwitchMigrationReport& report);

[[nodiscard]] const char* migration_source_role_name(
    MigrationSourceRole role) noexcept;
[[nodiscard]] bool parse_migration_source_role(
    std::string_view text,
    MigrationSourceRole& role) noexcept;
[[nodiscard]] const char* migration_source_availability_name(
    MigrationSourceAvailability availability) noexcept;
[[nodiscard]] bool parse_migration_source_availability(
    std::string_view text,
    MigrationSourceAvailability& availability) noexcept;
[[nodiscard]] const char* migration_item_status_name(
    MigrationItemStatus status) noexcept;
[[nodiscard]] bool parse_migration_item_status(
    std::string_view text,
    MigrationItemStatus& status) noexcept;

}  // namespace emberlights
