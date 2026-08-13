#pragma once

#include "emberlights/autoloop_persistence.hpp"
#include "emberlights/project.hpp"
#include "emberlights/project_edit_history.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace emberlights {

using StudioDocumentGeneration = std::uint64_t;

enum class StudioMutationResult : std::uint8_t {
    Applied,
    NoChange,
    StaleGeneration,
    ValidationFailed,
    LimitExceeded,
    InvalidCandidate,
    InternalError
};

enum class StudioDocumentBoundary : std::uint8_t {
    NewDocument,
    OpenedDocument,
    RestoredDocument
};

struct StudioDocumentSnapshot {
    ProjectDocument document;
    StudioDocumentGeneration generation{0U};
    bool dirty{false};
    bool can_undo{false};
    bool can_redo{false};
    std::size_t undo_count{0U};
    std::size_t redo_count{0U};
    PersistedAutoloopSourceResult autoloop_source;
};

struct StudioMutationOutcome {
    StudioMutationResult result{StudioMutationResult::InternalError};
    StudioDocumentGeneration generation{0U};
    ProjectValidation validation;
    std::string message;
    AutoloopPersistenceError persistence_error{
        AutoloopPersistenceError::None};

    [[nodiscard]] explicit operator bool() const noexcept {
        return result == StudioMutationResult::Applied ||
            result == StudioMutationResult::NoChange;
    }
};

// Studio-only document authority. The service owns editable source state,
// generation checks, bounded Undo/Redo, and durable-save comparison. It has no
// Runner, output, audio-decoding, importer, or UI-toolkit dependency.
class StudioDocumentService {
public:
    StudioDocumentService();

    [[nodiscard]] StudioDocumentSnapshot snapshot() const;
    [[nodiscard]] StudioDocumentGeneration generation() const noexcept {
        return generation_;
    }
    [[nodiscard]] bool dirty() const noexcept { return dirty_; }

    // Applies one complete validated candidate as one Undo transaction. The
    // candidate is rejected if the caller edited an older snapshot.
    [[nodiscard]] StudioMutationOutcome apply_candidate(
        StudioDocumentGeneration expected_generation,
        ProjectDocument candidate);

    // Commits the accepted rich source through this authoritative project
    // document as one Undo transaction. The full persistence stamp is checked
    // with the document generation so a reused numeric generation cannot
    // overwrite a different source digest or version.
    [[nodiscard]] StudioMutationOutcome apply_autoloop_source(
        StudioDocumentGeneration expected_generation,
        const PersistedAutoloopSourceStamp& expected_source,
        AutoloopSourceDocument candidate);

    // New/Open/Restore are explicit document boundaries. They validate and
    // replace the document, clear in-session history, and establish the
    // correct durable baseline without treating navigation as an edit.
    [[nodiscard]] StudioMutationOutcome replace_document(
        StudioDocumentGeneration expected_generation,
        ProjectDocument candidate,
        StudioDocumentBoundary boundary);

    [[nodiscard]] StudioMutationOutcome undo(
        StudioDocumentGeneration expected_generation);
    [[nodiscard]] StudioMutationOutcome redo(
        StudioDocumentGeneration expected_generation);

    // Call only after save_project_atomic has succeeded for this exact
    // generation. A failed/cancelled save deliberately leaves dirty state
    // unchanged.
    [[nodiscard]] StudioMutationOutcome acknowledge_saved(
        StudioDocumentGeneration expected_generation);

private:
    [[nodiscard]] StudioMutationOutcome apply_candidate_impl(
        StudioDocumentGeneration expected_generation,
        ProjectDocument candidate,
        bool allow_autoloop_source_change);
    [[nodiscard]] StudioMutationOutcome make_outcome(
        StudioMutationResult result,
        ProjectValidation validation,
        std::string message,
        AutoloopPersistenceError persistence_error =
            AutoloopPersistenceError::None) const;
    [[nodiscard]] bool can_advance_generation() const noexcept;
    void advance_generation() noexcept;
    void refresh_serialized_state();
    void refresh_dirty_state() noexcept;

    ProjectDocument project_;
    ProjectEditHistory history_;
    StudioDocumentGeneration generation_{1U};
    std::string current_serialized_;
    std::string durable_serialized_;
    bool has_durable_baseline_{true};
    bool dirty_{false};
};

[[nodiscard]] const char* studio_mutation_result_name(
    StudioMutationResult result) noexcept;

}  // namespace emberlights
