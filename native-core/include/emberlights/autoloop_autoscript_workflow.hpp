#pragma once

#include "emberlights/autoloop_autoscript_studio.hpp"
#include "emberlights/studio_preview.hpp"

#include <cstddef>
#include <optional>
#include <string>

namespace emberlights {

enum class StudioAutoloopAutoscriptWorkflowResult : std::uint8_t {
    Loaded,
    ProposalReady,
    PreviewApplied,
    Committed,
    Discarded,
    InvalidDocument,
    ProposalRejected,
    PreviewRejected,
    NoProposal,
    StaleDocument,
    CommitRejected,
    InvalidArgument
};

struct StudioAutoloopAutoscriptWorkflowOutcome {
    StudioAutoloopAutoscriptWorkflowResult result{
        StudioAutoloopAutoscriptWorkflowResult::InvalidArgument};
    AutoloopAutoscriptProposalResult proposal_result{
        AutoloopAutoscriptProposalResult::InvalidRequest};
    StudioPreviewResult preview_result{StudioPreviewResult::NotLoaded};
    StudioMutationResult mutation_result{StudioMutationResult::InternalError};
    std::string message;

    [[nodiscard]] explicit operator bool() const noexcept {
        return result == StudioAutoloopAutoscriptWorkflowResult::Loaded ||
            result == StudioAutoloopAutoscriptWorkflowResult::ProposalReady ||
            result == StudioAutoloopAutoscriptWorkflowResult::PreviewApplied ||
            result == StudioAutoloopAutoscriptWorkflowResult::Committed ||
            result == StudioAutoloopAutoscriptWorkflowResult::Discarded;
    }
};

struct StudioAutoloopAutoscriptWorkflowSnapshot {
    StudioDocumentSnapshot document;
    bool has_proposal{false};
    bool proposal_ready{false};
    bool preview_ready{false};
    bool can_commit{false};
    bool committed{false};
    AutoloopAutoscriptProposalResult proposal_result{
        AutoloopAutoscriptProposalResult::InvalidRequest};
    StudioPreviewResult preview_result{StudioPreviewResult::NotLoaded};
    std::string proposal_digest;
    std::string preview_source_digest;
    std::string placement_id;
    std::string asset_id;
    std::optional<showcore::AutoloopAddress> address;
    std::size_t generated_asset_count{0U};
    std::size_t generated_event_count{0U};
    StudioPreviewSnapshot preview;
};

// Toolkit-neutral orchestration for the complete Studio AutoScript gesture.
// Proposal and preview never mutate the document. Commit delegates exactly
// once to StudioDocumentService after a successful production-compiler,
// output-disabled preview of the immutable proposal.
class StudioAutoloopAutoscriptWorkflow {
public:
    [[nodiscard]] StudioAutoloopAutoscriptWorkflowOutcome load_document(
        ProjectDocument document,
        StudioDocumentBoundary boundary =
            StudioDocumentBoundary::OpenedDocument);

    [[nodiscard]] StudioAutoloopAutoscriptWorkflowOutcome propose_and_preview(
        AutoloopAutoscriptRequest request,
        const AutoloopAutoscriptCancellationToken* cancellation = nullptr);

    [[nodiscard]] StudioAutoloopAutoscriptWorkflowOutcome preview_phase(
        double phase);

    [[nodiscard]] StudioAutoloopAutoscriptWorkflowOutcome commit();
    [[nodiscard]] StudioAutoloopAutoscriptWorkflowOutcome discard();

    [[nodiscard]] StudioAutoloopAutoscriptWorkflowSnapshot snapshot() const;
    [[nodiscard]] StudioDocumentSnapshot document_snapshot() const {
        return document_.snapshot();
    }

private:
    void clear_proposal() noexcept;
    [[nodiscard]] StudioAutoloopAutoscriptWorkflowOutcome outcome(
        StudioAutoloopAutoscriptWorkflowResult result,
        std::string message = {}) const;

    StudioDocumentService document_;
    StudioPreviewService preview_;
    std::optional<StudioAutoloopAutoscriptProposal> proposal_;
    StudioPreviewResult preview_result_{StudioPreviewResult::NotLoaded};
    std::string placement_id_;
    std::string asset_id_;
    std::optional<showcore::AutoloopAddress> address_;
    bool preview_ready_{false};
    bool committed_{false};
};

[[nodiscard]] const char* studio_autoloop_autoscript_workflow_result_name(
    StudioAutoloopAutoscriptWorkflowResult result) noexcept;

}  // namespace emberlights
