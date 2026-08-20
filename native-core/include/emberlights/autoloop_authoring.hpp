#pragma once

#include "emberlights/autoloop_source.hpp"
#include "emberlights/studio_document.hpp"

#include "showcore/autoloop.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace emberlights {

inline constexpr std::size_t kMaximumAutoloopAuthoringAssets =
    showcore::kMaxAutoloops;
inline constexpr std::size_t kMaximumAutoloopAuthoringPrograms =
    showcore::kMaxAutoloops;
inline constexpr std::size_t kMaximumAutoloopAuthoringLaunchProfiles =
    showcore::kMaxAutoloops;
inline constexpr std::size_t kMaximumAutoloopAuthoringProvenanceRecords =
    showcore::kMaxAutoloops;

enum class AutoloopAuthoringResult : std::uint8_t {
    Applied,
    NoChange,
    StaleGeneration,
    ValidationFailed,
    CapacityExceeded,
    InvalidCandidate,
    MissingAsset,
    MissingPlacement,
    OccupiedPlacement,
    DependencyConflict,
    UndoUnavailable,
    RedoUnavailable,
    GenerationExhausted
};

// A content transaction creates these four canonical source records together.
// This is a convenience bundle only; the persisted/editable model remains the
// accepted AutoloopSourceDocument.
struct AutoloopAssetBundle {
    AutoloopAssetDefinition asset;
    AutoloopProgramDefinition program;
    AutoloopLaunchProfileDefinition launch_profile;
    AutoloopProvenanceDefinition provenance;
};

struct AutoloopDependencyReport {
    std::string asset_id;
    std::vector<std::string> placement_ids;
    std::vector<std::string> assets_sharing_program;
    std::vector<std::string> assets_sharing_launch_profile;
    std::vector<std::string> assets_sharing_provenance;
    // Records that become unreferenced and are removed by an explicitly
    // accepted delete transaction.
    std::vector<std::string> orphan_records;

    [[nodiscard]] bool has_placement_dependencies() const noexcept {
        return !placement_ids.empty();
    }
};

struct AutoloopAuthoringSnapshot {
    AutoloopSourceDocument source;
    StudioDocumentGeneration generation{0U};
    bool can_undo{false};
    bool can_redo{false};
    std::string source_digest;
};

struct AutoloopAuthoringOutcome {
    AutoloopAuthoringResult result{AutoloopAuthoringResult::InvalidCandidate};
    StudioDocumentGeneration expected_generation{0U};
    StudioDocumentGeneration generation{0U};
    AutoloopSourceValidation validation;
    AutoloopDependencyReport dependencies;
    std::string stable_id;
    std::optional<showcore::AutoloopAddress> address;
    std::string message;

    [[nodiscard]] explicit operator bool() const noexcept {
        return result == AutoloopAuthoringResult::Applied ||
            result == AutoloopAuthoringResult::NoChange;
    }
};

struct AutoloopNextOpenPlacement {
    bool found{false};
    showcore::AutoloopAddress address{};
};

// Toolkit-neutral editor session over the accepted V2 source document. It is
// intentionally not project persistence authority: a later coordinated
// project-format integration will commit this validated source through the
// StudioDocumentService boundary. This service supplies bounded source edits,
// generation checks, and one-step draft Undo/Redo for that integration.
class AutoloopAuthoringService {
public:
    AutoloopAuthoringService();
    explicit AutoloopAuthoringService(
        AutoloopSourceDocument source,
        StudioDocumentGeneration generation = 1U);

    [[nodiscard]] AutoloopAuthoringSnapshot snapshot() const;
    [[nodiscard]] StudioDocumentGeneration generation() const noexcept {
        return generation_;
    }

    // Applies one complete normalized source candidate as one transaction.
    // Pack/generator proposal services use this same boundary rather than
    // mutating source vectors directly.
    [[nodiscard]] AutoloopAuthoringOutcome apply_candidate(
        StudioDocumentGeneration expected_generation,
        AutoloopSourceDocument candidate);

    [[nodiscard]] AutoloopAuthoringOutcome create_asset(
        StudioDocumentGeneration expected_generation,
        AutoloopAssetBundle bundle);
    [[nodiscard]] AutoloopAuthoringOutcome rename_asset(
        StudioDocumentGeneration expected_generation,
        std::string_view asset_id,
        std::string name);
    [[nodiscard]] AutoloopAuthoringOutcome duplicate_asset(
        StudioDocumentGeneration expected_generation,
        std::string_view source_asset_id,
        std::string new_asset_id,
        std::string new_program_id,
        std::string new_launch_profile_id,
        std::string new_provenance_id,
        std::string new_name);

    [[nodiscard]] AutoloopDependencyReport inspect_dependencies(
        std::string_view asset_id) const;
    // Referenced assets are rejected unless remove_dependent_placements is an
    // explicit true. The returned report always names every removed placement
    // and every shared/orphan record decision.
    [[nodiscard]] AutoloopAuthoringOutcome delete_asset(
        StudioDocumentGeneration expected_generation,
        std::string_view asset_id,
        bool remove_dependent_placements = false);

    [[nodiscard]] AutoloopAuthoringOutcome assign_placement(
        StudioDocumentGeneration expected_generation,
        std::string placement_id,
        showcore::AutoloopAddress address,
        std::string asset_id,
        std::string content_management_key = {});
    [[nodiscard]] AutoloopAuthoringOutcome assign_next_open(
        StudioDocumentGeneration expected_generation,
        std::string placement_id,
        std::string asset_id,
        showcore::AutoloopAddress after = {},
        std::string content_management_key = {});
    [[nodiscard]] AutoloopAuthoringOutcome unassign_placement(
        StudioDocumentGeneration expected_generation,
        std::string_view placement_id);
    [[nodiscard]] AutoloopAuthoringOutcome move_placement(
        StudioDocumentGeneration expected_generation,
        std::string_view placement_id,
        showcore::AutoloopAddress destination);
    [[nodiscard]] AutoloopAuthoringOutcome swap_placements(
        StudioDocumentGeneration expected_generation,
        std::string_view first_placement_id,
        std::string_view second_placement_id);
    [[nodiscard]] AutoloopNextOpenPlacement next_open(
        showcore::AutoloopAddress after = {}) const noexcept;

    [[nodiscard]] AutoloopAuthoringOutcome undo(
        StudioDocumentGeneration expected_generation);
    [[nodiscard]] AutoloopAuthoringOutcome redo(
        StudioDocumentGeneration expected_generation);

private:
    [[nodiscard]] AutoloopAuthoringOutcome outcome(
        AutoloopAuthoringResult result,
        StudioDocumentGeneration expected_generation,
        std::string message = {}) const;
    [[nodiscard]] AutoloopSourceValidation validate_candidate(
        const AutoloopSourceDocument& candidate) const;
    [[nodiscard]] bool can_advance_generation() const noexcept;
    void advance_generation() noexcept;

    AutoloopSourceDocument source_;
    StudioDocumentGeneration generation_{1U};
    std::string serialized_;
    std::optional<AutoloopSourceDocument> undo_source_;
    std::optional<AutoloopSourceDocument> redo_source_;
};

[[nodiscard]] const char* autoloop_authoring_result_name(
    AutoloopAuthoringResult result) noexcept;

}  // namespace emberlights
