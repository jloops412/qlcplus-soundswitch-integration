#pragma once

#include "emberlights/autoloop_authoring.hpp"

#include "showcore/autoloop.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace emberlights {

inline constexpr std::string_view kAutoloopAutoscriptGeneratorId =
    "emberlights.autoscript.rule-based";
inline constexpr std::uint32_t kAutoloopAutoscriptGeneratorVersion = 1U;
inline constexpr std::size_t kMaximumAutoloopAutoscriptSegments = 32U;
inline constexpr std::size_t kMaximumAutoloopAutoscriptRoleSelectors = 4U;
inline constexpr std::size_t kMaximumAutoloopAutoscriptGeneratedAssets = 32U;
inline constexpr std::size_t kMaximumAutoloopAutoscriptGeneratedEvents = 8192U;
inline constexpr std::size_t kMaximumAutoloopAutoscriptCandidateEvents =
    16384U;
inline constexpr std::size_t kMaximumAutoloopAutoscriptCanonicalBytes =
    16U * 1024U * 1024U;
inline constexpr std::size_t kMaximumAutoloopAutoscriptOperations = 100000U;
inline constexpr MusicalTick kMaximumAutoloopAutoscriptTrackTicks =
    4096 * 4 * kMusicalTicksPerQuarter;

enum class AutoloopAutoscriptStyle : std::uint8_t {
    Subtle,
    Balanced,
    ColorMotion,
    BuildDrop,
    Count
};

enum class AutoloopAutoscriptComplexity : std::uint8_t {
    Minimal,
    Low,
    Medium,
    High,
    Count
};

enum class AutoloopAutoscriptSectionKind : std::uint8_t {
    Intro,
    Verse,
    Chorus,
    Build,
    Drop,
    Break,
    Outro,
    Custom,
    Count
};

struct AutoloopAutoscriptMusicalSection {
    MusicalTick start_tick{0};
    MusicalTick end_tick{0};
    AutoloopAutoscriptSectionKind kind{
        AutoloopAutoscriptSectionKind::Custom};
    std::uint16_t energy_per_mille{500U};
};

struct AutoloopAutoscriptEnergyBand {
    MusicalTick start_tick{0};
    MusicalTick end_tick{0};
    std::uint16_t minimum_energy_per_mille{0U};
    std::uint16_t maximum_energy_per_mille{1000U};
};

struct AutoloopAutoscriptContentBudget {
    std::size_t maximum_generated_assets{16U};
    std::size_t maximum_generated_events{2048U};
    std::size_t maximum_candidate_canonical_bytes{4U * 1024U * 1024U};
};

struct AutoloopAutoscriptOperationBudget {
    std::size_t maximum_operations{20000U};
};

// A request carries musical time only. Sections and energy bands are
// alternatives: exactly one collection must be non-empty. Empty role selectors
// mean the semantic Master target; otherwise each stable string becomes a
// RoleSelector target. No fixture, DMX, file, media, or wall-clock input enters
// this contract.
struct AutoloopAutoscriptRequest {
    MusicalTick track_duration_ticks{0};
    MusicalTick loop_length_ticks{4 * kMusicalTicksPerQuarter};
    MusicalTick grid_ticks{kMusicalTicksPerQuarter};
    AutoloopAutoscriptStyle style{AutoloopAutoscriptStyle::Balanced};
    AutoloopAutoscriptComplexity complexity{
        AutoloopAutoscriptComplexity::Medium};
    std::vector<AutoloopAutoscriptMusicalSection> musical_sections;
    std::vector<AutoloopAutoscriptEnergyBand> energy_bands;
    std::vector<std::string> eligible_role_selectors;
    // Optional distinguishes an explicitly supplied seed (including zero)
    // from an omitted seed. Omitted seeds are rejected rather than synthesized.
    std::optional<std::uint64_t> seed;
    showcore::AutoloopAddress first_placement{};
    AutoloopAutoscriptContentBudget content_budget;
    AutoloopAutoscriptOperationBudget operation_budget;
};

class AutoloopAutoscriptCancellationToken {
public:
    AutoloopAutoscriptCancellationToken() = default;
    AutoloopAutoscriptCancellationToken(
        const AutoloopAutoscriptCancellationToken&) = delete;
    AutoloopAutoscriptCancellationToken& operator=(
        const AutoloopAutoscriptCancellationToken&) = delete;

    void request_cancellation() noexcept {
        requested_.store(true, std::memory_order_release);
    }
    [[nodiscard]] bool cancellation_requested() const noexcept {
        return requested_.load(std::memory_order_acquire);
    }

private:
    std::atomic_bool requested_{false};
};

enum class AutoloopAutoscriptProposalResult : std::uint8_t {
    Ready,
    InvalidRequest,
    Cancelled,
    OperationBudgetExceeded,
    ContentBudgetExceeded,
    CapacityExceeded,
    OccupiedPlacement,
    StableIdConflict,
    InvalidBaseSource,
    ValidationFailed
};

enum class AutoloopAutoscriptDiagnosticSeverity : std::uint8_t {
    Information,
    Warning,
    Error
};

struct AutoloopAutoscriptDiagnostic {
    AutoloopAutoscriptDiagnosticSeverity severity{
        AutoloopAutoscriptDiagnosticSeverity::Error};
    std::string code;
    std::string subject;
    std::string message;
};

// Immutable, reviewable proposal. preview_source() is the complete candidate
// to validate/compile through StudioPreviewService without mutating the live
// authoring session. Only apply_autoloop_autoscript_proposal can commit it.
class AutoloopAutoscriptProposal {
public:
    AutoloopAutoscriptProposal() = default;

    [[nodiscard]] AutoloopAutoscriptProposalResult result() const noexcept {
        return result_;
    }
    [[nodiscard]] bool ready() const noexcept {
        return result_ == AutoloopAutoscriptProposalResult::Ready;
    }
    [[nodiscard]] StudioDocumentGeneration base_generation() const noexcept {
        return base_generation_;
    }
    [[nodiscard]] std::string_view base_source_digest() const noexcept {
        return base_source_digest_;
    }
    [[nodiscard]] std::string_view request_digest() const noexcept {
        return request_digest_;
    }
    [[nodiscard]] std::string_view proposal_digest() const noexcept {
        return proposal_digest_;
    }
    [[nodiscard]] std::string_view preview_source_digest() const noexcept {
        return preview_source_digest_;
    }
    [[nodiscard]] const AutoloopSourceDocument& preview_source() const noexcept {
        return preview_source_;
    }
    [[nodiscard]] const std::vector<AutoloopAutoscriptDiagnostic>&
    diagnostics() const noexcept {
        return diagnostics_;
    }
    [[nodiscard]] const std::vector<std::string>& generated_asset_ids()
        const noexcept {
        return generated_asset_ids_;
    }
    [[nodiscard]] const std::vector<std::string>& generated_placement_ids()
        const noexcept {
        return generated_placement_ids_;
    }
    [[nodiscard]] const std::vector<showcore::AutoloopAddress>&
    generated_addresses() const noexcept {
        return generated_addresses_;
    }
    [[nodiscard]] std::size_t generated_event_count() const noexcept {
        return generated_event_count_;
    }
    [[nodiscard]] std::size_t operations_used() const noexcept {
        return operations_used_;
    }

private:
    friend AutoloopAutoscriptProposal propose_autoloop_autoscript(
        const AutoloopAuthoringSnapshot&,
        AutoloopAutoscriptRequest,
        const AutoloopAutoscriptCancellationToken*);
    friend AutoloopAuthoringOutcome apply_autoloop_autoscript_proposal(
        AutoloopAuthoringService&,
        const AutoloopAutoscriptProposal&);

    AutoloopAutoscriptProposalResult result_{
        AutoloopAutoscriptProposalResult::InvalidRequest};
    StudioDocumentGeneration base_generation_{0U};
    std::string base_source_digest_;
    std::string request_digest_;
    std::string proposal_digest_;
    std::string preview_source_digest_;
    AutoloopSourceDocument preview_source_;
    std::vector<AutoloopAutoscriptDiagnostic> diagnostics_;
    std::vector<std::string> generated_asset_ids_;
    std::vector<std::string> generated_placement_ids_;
    std::vector<showcore::AutoloopAddress> generated_addresses_;
    std::size_t generated_event_count_{0U};
    std::size_t operations_used_{0U};
};

[[nodiscard]] AutoloopAutoscriptProposal propose_autoloop_autoscript(
    const AutoloopAuthoringSnapshot& snapshot,
    AutoloopAutoscriptRequest request,
    const AutoloopAutoscriptCancellationToken* cancellation = nullptr);

// Rechecks readiness, proposal/source integrity, base generation and base
// digest, then delegates the entire candidate to apply_candidate exactly once.
// Failures never mutate the service; a successful commit is one Undo step.
[[nodiscard]] AutoloopAuthoringOutcome apply_autoloop_autoscript_proposal(
    AutoloopAuthoringService& service,
    const AutoloopAutoscriptProposal& proposal);

[[nodiscard]] const char* autoloop_autoscript_proposal_result_name(
    AutoloopAutoscriptProposalResult result) noexcept;

}  // namespace emberlights
