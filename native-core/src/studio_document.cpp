#include "emberlights/studio_document.hpp"

#include "emberlights/project_io.hpp"

#include <limits>
#include <utility>

namespace emberlights {

StudioDocumentService::StudioDocumentService()
    : project_(make_starter_project()),
      current_serialized_(serialize_project(project_)),
      durable_serialized_(current_serialized_) {}

StudioDocumentSnapshot StudioDocumentService::snapshot() const {
    return {
        project_,
        generation_,
        dirty_,
        history_.can_undo(),
        history_.can_redo(),
        history_.undo_count(),
        history_.redo_count(),
        inspect_persisted_autoloop_source(project_)};
}

StudioMutationOutcome StudioDocumentService::apply_candidate(
    StudioDocumentGeneration expected_generation,
    ProjectDocument candidate) {
    return apply_candidate_impl(
        expected_generation, std::move(candidate), false);
}

StudioMutationOutcome StudioDocumentService::apply_candidate_impl(
    StudioDocumentGeneration expected_generation,
    ProjectDocument candidate,
    bool allow_autoloop_source_change) {
    if (expected_generation != generation_) {
        return make_outcome(
            StudioMutationResult::StaleGeneration,
            validate_project(project_),
            "The Studio document changed before this edit could be committed.");
    }

    auto validation = validate_project(candidate);
    if (!validation.ok()) {
        return make_outcome(
            StudioMutationResult::ValidationFailed,
            std::move(validation),
            "The candidate project failed validation and was not applied.");
    }
    const auto persisted = inspect_persisted_autoloop_source(candidate);
    if (!persisted) {
        return make_outcome(
            StudioMutationResult::InvalidCandidate,
            std::move(validation),
            "The candidate project contains an invalid persisted Autoloop source: " +
                persisted.message,
            persisted.error);
    }
    if (!allow_autoloop_source_change) {
        const auto active_source = inspect_persisted_autoloop_source(project_);
        if (!active_source) {
            return make_outcome(
                StudioMutationResult::InvalidCandidate,
                std::move(validation),
                "The active project contains an invalid persisted Autoloop source: " +
                    active_source.message,
                active_source.error);
        }
        if (active_source.stamp != persisted.stamp) {
            return make_outcome(
                StudioMutationResult::InvalidCandidate,
                std::move(validation),
                "Rich Autoloop source changes require the generation-and-digest checked transaction.");
        }
    }

    const auto candidate_serialized = serialize_project(candidate);
    if (candidate_serialized == current_serialized_) {
        return make_outcome(
            StudioMutationResult::NoChange,
            std::move(validation),
            "The candidate is identical to the active Studio document.");
    }
    if (!can_advance_generation()) {
        return make_outcome(
            StudioMutationResult::LimitExceeded,
            std::move(validation),
            "The Studio document generation limit has been reached.");
    }

    history_.record_before_change(project_);
    project_ = std::move(candidate);
    current_serialized_ = candidate_serialized;
    advance_generation();
    refresh_dirty_state();
    return make_outcome(
        StudioMutationResult::Applied,
        std::move(validation),
        "The Studio edit was applied as one Undo transaction.");
}

StudioMutationOutcome StudioDocumentService::apply_autoloop_source(
    StudioDocumentGeneration expected_generation,
    const PersistedAutoloopSourceStamp& expected_source,
    AutoloopSourceDocument candidate) {
    if (expected_generation != generation_) {
        return make_outcome(
            StudioMutationResult::StaleGeneration,
            validate_project(project_),
            "The Studio document changed before the Autoloop source could be committed.");
    }

    const auto active_source = inspect_persisted_autoloop_source(project_);
    if (!active_source) {
        return make_outcome(
            StudioMutationResult::InvalidCandidate,
            validate_project(project_),
            "The active project contains an invalid persisted Autoloop source: " +
                active_source.message,
            active_source.error);
    }
    if (active_source.stamp != expected_source) {
        return make_outcome(
            StudioMutationResult::StaleGeneration,
            validate_project(project_),
            "The persisted Autoloop source digest or version changed before commit.");
    }

    auto candidate_project = project_;
    const auto persisted = upsert_persisted_autoloop_source(
        candidate_project, candidate);
    if (!persisted) {
        return make_outcome(
            persisted.error == AutoloopPersistenceError::SourceTooLarge
                ? StudioMutationResult::LimitExceeded
                : StudioMutationResult::InvalidCandidate,
            validate_project(project_),
            "The Autoloop source was not persisted: " + persisted.message,
            persisted.error);
    }
    return apply_candidate_impl(
        expected_generation, std::move(candidate_project), true);
}

StudioMutationOutcome StudioDocumentService::replace_document(
    StudioDocumentGeneration expected_generation,
    ProjectDocument candidate,
    StudioDocumentBoundary boundary) {
    if (expected_generation != generation_) {
        return make_outcome(
            StudioMutationResult::StaleGeneration,
            validate_project(project_),
            "The Studio document changed before the document boundary completed.");
    }

    auto validation = validate_project(candidate);
    if (!validation.ok()) {
        return make_outcome(
            StudioMutationResult::ValidationFailed,
            std::move(validation),
            "The replacement project failed validation and was not opened.");
    }
    const auto persisted = inspect_persisted_autoloop_source(candidate);
    if (!persisted) {
        return make_outcome(
            StudioMutationResult::InvalidCandidate,
            std::move(validation),
            "The replacement project contains an invalid persisted Autoloop source: " +
                persisted.message,
            persisted.error);
    }
    if (!can_advance_generation()) {
        return make_outcome(
            StudioMutationResult::LimitExceeded,
            std::move(validation),
            "The Studio document generation limit has been reached.");
    }

    project_ = std::move(candidate);
    history_.clear();
    current_serialized_ = serialize_project(project_);
    advance_generation();

    if (boundary == StudioDocumentBoundary::NewDocument) {
        durable_serialized_.clear();
        has_durable_baseline_ = false;
    } else {
        durable_serialized_ = current_serialized_;
        has_durable_baseline_ = true;
    }
    refresh_dirty_state();

    const char* message = boundary == StudioDocumentBoundary::NewDocument
        ? "A new unsaved Studio document is active."
        : boundary == StudioDocumentBoundary::OpenedDocument
            ? "The opened project is now the durable Studio baseline."
            : "The restored project is now the durable Studio baseline.";
    return make_outcome(
        StudioMutationResult::Applied,
        std::move(validation),
        message);
}

StudioMutationOutcome StudioDocumentService::undo(
    StudioDocumentGeneration expected_generation) {
    if (expected_generation != generation_) {
        return make_outcome(
            StudioMutationResult::StaleGeneration,
            validate_project(project_),
            "The Studio document changed before Undo could be applied.");
    }
    if (!history_.can_undo()) {
        return make_outcome(
            StudioMutationResult::NoChange,
            validate_project(project_),
            "There is no Studio edit to undo.");
    }
    if (!can_advance_generation()) {
        return make_outcome(
            StudioMutationResult::LimitExceeded,
            validate_project(project_),
            "The Studio document generation limit has been reached.");
    }
    if (!history_.undo(project_)) {
        return make_outcome(
            StudioMutationResult::InternalError,
            validate_project(project_),
            "The Studio Undo history could not restore the prior document.");
    }

    refresh_serialized_state();
    advance_generation();
    refresh_dirty_state();
    return make_outcome(
        StudioMutationResult::Applied,
        validate_project(project_),
        "The previous Studio document state was restored.");
}

StudioMutationOutcome StudioDocumentService::redo(
    StudioDocumentGeneration expected_generation) {
    if (expected_generation != generation_) {
        return make_outcome(
            StudioMutationResult::StaleGeneration,
            validate_project(project_),
            "The Studio document changed before Redo could be applied.");
    }
    if (!history_.can_redo()) {
        return make_outcome(
            StudioMutationResult::NoChange,
            validate_project(project_),
            "There is no Studio edit to redo.");
    }
    if (!can_advance_generation()) {
        return make_outcome(
            StudioMutationResult::LimitExceeded,
            validate_project(project_),
            "The Studio document generation limit has been reached.");
    }
    if (!history_.redo(project_)) {
        return make_outcome(
            StudioMutationResult::InternalError,
            validate_project(project_),
            "The Studio Redo history could not restore the later document.");
    }

    refresh_serialized_state();
    advance_generation();
    refresh_dirty_state();
    return make_outcome(
        StudioMutationResult::Applied,
        validate_project(project_),
        "The later Studio document state was restored.");
}

StudioMutationOutcome StudioDocumentService::acknowledge_saved(
    StudioDocumentGeneration expected_generation) {
    if (expected_generation != generation_) {
        return make_outcome(
            StudioMutationResult::StaleGeneration,
            validate_project(project_),
            "The document changed while the save was completing; it remains unsaved.");
    }
    if (has_durable_baseline_ && durable_serialized_ == current_serialized_) {
        return make_outcome(
            StudioMutationResult::NoChange,
            validate_project(project_),
            "The current Studio document already matches the durable save baseline.");
    }

    durable_serialized_ = current_serialized_;
    has_durable_baseline_ = true;
    refresh_dirty_state();
    return make_outcome(
        StudioMutationResult::Applied,
        validate_project(project_),
        "The durable save baseline now matches this Studio generation.");
}

StudioMutationOutcome StudioDocumentService::make_outcome(
    StudioMutationResult result,
    ProjectValidation validation,
    std::string message,
    AutoloopPersistenceError persistence_error) const {
    return {
        result,
        generation_,
        std::move(validation),
        std::move(message),
        persistence_error};
}

bool StudioDocumentService::can_advance_generation() const noexcept {
    return generation_ != std::numeric_limits<StudioDocumentGeneration>::max();
}

void StudioDocumentService::advance_generation() noexcept {
    ++generation_;
}

void StudioDocumentService::refresh_serialized_state() {
    current_serialized_ = serialize_project(project_);
}

void StudioDocumentService::refresh_dirty_state() noexcept {
    dirty_ = !has_durable_baseline_ || current_serialized_ != durable_serialized_;
}

const char* studio_mutation_result_name(StudioMutationResult result) noexcept {
    switch (result) {
    case StudioMutationResult::Applied: return "applied";
    case StudioMutationResult::NoChange: return "noChange";
    case StudioMutationResult::StaleGeneration: return "staleGeneration";
    case StudioMutationResult::ValidationFailed: return "validationFailed";
    case StudioMutationResult::LimitExceeded: return "limitExceeded";
    case StudioMutationResult::InvalidCandidate: return "invalidCandidate";
    case StudioMutationResult::InternalError: return "internalError";
    }
    return "internalError";
}

}  // namespace emberlights
