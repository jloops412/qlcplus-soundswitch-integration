#pragma once

#include "emberlights/soundswitch_source_binding.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace emberlights {

inline constexpr std::string_view kMigrationPortabilityReviewFormat =
    "emberlights-migration-portability-review";
inline constexpr std::uint32_t kMigrationPortabilityReviewFormatVersion = 1U;

enum class MigrationPortabilitySource : std::uint8_t {
    SoundSwitch,
    Wolfmix,
    Count
};

// This is the lifecycle documented by the Autoloops/Studio migration plans.
// It describes evidence readiness only; it is deliberately not an adapter ABI.
enum class MigrationPortabilityStage : std::uint8_t {
    Probe,
    Inventory,
    Decode,
    Reconcile,
    Plan,
    Commit,
    Upgrade,
    Count
};

enum class MigrationPortabilityReadiness : std::uint8_t {
    Ready,
    ReadyForManualReview,
    Blocked,
    EvidenceUnavailable
};

enum class MigrationPortabilityEvidenceTier : std::uint8_t {
    Verified,
    ContractTested,
    Inferred,
    Unresolved
};

struct MigrationPortabilityStageReview {
    MigrationPortabilityStage stage{MigrationPortabilityStage::Probe};
    MigrationPortabilityReadiness readiness{
        MigrationPortabilityReadiness::EvidenceUnavailable};
    MigrationPortabilityEvidenceTier evidence_tier{
        MigrationPortabilityEvidenceTier::Unresolved};
    // Stable, source-specific machine code for the stage result.
    std::string readiness_code;
    // Stable dependency/safety codes only. Human prose is kept separate.
    std::vector<std::string> blocker_codes;
    std::string summary;
};

struct MigrationPortabilitySourceReview {
    MigrationPortabilitySource source{MigrationPortabilitySource::SoundSwitch};
    std::string label;
    // SoundSwitch values reuse the existing source-binding/review taxonomy.
    std::string source_binding_status_code;
    std::string review_state_code;
    bool research_only{false};
    bool source_inspection_available{false};
    // Hash comparisons establish this identity flag only. Digests never enter
    // this portability report and never imply semantic decode fidelity.
    bool artifact_identity_verified{false};
    bool semantic_decoder_qualified{false};
    bool semantic_import_claimed{false};
    bool source_bytes_included{false};
    std::array<MigrationPortabilityStageReview,
        static_cast<std::size_t>(MigrationPortabilityStage::Count)> stages;
};

struct MigrationPortabilityReview {
    std::string format{std::string(kMigrationPortabilityReviewFormat)};
    std::uint32_t format_version{kMigrationPortabilityReviewFormatVersion};
    std::array<MigrationPortabilitySourceReview,
        static_cast<std::size_t>(MigrationPortabilitySource::Count)> sources;
};

// Builds a content-safe diagnostics/UI view from the existing SoundSwitch
// audit. WOLFMIX is intentionally emitted as research/evidence unavailable
// until an authorized, versioned controlled-delta corpus exists.
[[nodiscard]] MigrationPortabilityReview build_migration_portability_review(
    const SoundSwitchSourceBindingAudit& soundswitch_audit);

// Deterministic JSON. It contains no paths, hashes, payload fields, source
// bytes, or UI/manual-derived vendor semantics.
[[nodiscard]] std::string serialize_migration_portability_review(
    const MigrationPortabilityReview& review);

[[nodiscard]] const char* migration_portability_source_name(
    MigrationPortabilitySource source) noexcept;
[[nodiscard]] const char* migration_portability_stage_name(
    MigrationPortabilityStage stage) noexcept;
[[nodiscard]] const char* migration_portability_readiness_name(
    MigrationPortabilityReadiness readiness) noexcept;
[[nodiscard]] const char* migration_portability_evidence_tier_name(
    MigrationPortabilityEvidenceTier tier) noexcept;

}  // namespace emberlights
