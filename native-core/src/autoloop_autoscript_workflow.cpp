#include "emberlights/autoloop_autoscript_workflow.hpp"

#include <utility>

namespace emberlights {

namespace {

[[nodiscard]] StudioAutoloopAutoscriptWorkflowResult mutation_failure_result(
    StudioMutationResult result) noexcept {
    return result == StudioMutationResult::StaleGeneration
        ? StudioAutoloopAutoscriptWorkflowResult::StaleDocument
        : StudioAutoloopAutoscriptWorkflowResult::CommitRejected;
}

}  // namespace

StudioAutoloopAutoscriptWorkflowOutcome
StudioAutoloopAutoscriptWorkflow::load_document(
    ProjectDocument document,
    StudioDocumentBoundary boundary) {
    clear_proposal();
    const auto mutation = document_.replace_document(
        document_.generation(), std::move(document), boundary);
    auto result = outcome(
        mutation
            ? StudioAutoloopAutoscriptWorkflowResult::Loaded
            : StudioAutoloopAutoscriptWorkflowResult::InvalidDocument,
        mutation.message);
    result.mutation_result = mutation.result;
    return result;
}

StudioAutoloopAutoscriptWorkflowOutcome
StudioAutoloopAutoscriptWorkflow::propose_and_preview(
    AutoloopAutoscriptRequest request,
    const AutoloopAutoscriptCancellationToken* cancellation) {
    clear_proposal();
    const auto document = document_.snapshot();
    proposal_ = propose_studio_autoloop_autoscript(
        document, std::move(request), cancellation);
    if (!proposal_->ready()) {
        return outcome(
            StudioAutoloopAutoscriptWorkflowResult::ProposalRejected,
            std::string(proposal_->message()));
    }

    const auto& generated = proposal_->proposal().generated_placement_ids();
    const auto& addresses = proposal_->proposal().generated_addresses();
    const auto& assets = proposal_->proposal().generated_asset_ids();
    if (generated.empty() || addresses.empty() || assets.empty()) {
        return outcome(
            StudioAutoloopAutoscriptWorkflowResult::ProposalRejected,
            "The ready AutoScript proposal did not contain a reviewable placement.");
    }
    placement_id_ = generated.front();
    address_ = addresses.front();
    asset_id_ = assets.front();

    AutoloopAuthoringSnapshot source;
    source.source = proposal_->proposal().preview_source();
    source.generation = proposal_->document_generation();
    source.source_digest =
        std::string(proposal_->proposal().preview_source_digest());

    StudioPreviewService candidate_preview;
    const auto loaded = candidate_preview.load_autoloop_v2(document, source);
    preview_result_ = loaded.result;
    if (!loaded) {
        preview_ = std::move(candidate_preview);
        return outcome(
            StudioAutoloopAutoscriptWorkflowResult::PreviewRejected,
            loaded.message);
    }
    const auto selected = candidate_preview.preview_autoloop_v2(
        document.generation, source.generation, placement_id_);
    preview_result_ = selected.result;
    preview_ = std::move(candidate_preview);
    if (!selected) {
        return outcome(
            StudioAutoloopAutoscriptWorkflowResult::PreviewRejected,
            selected.message);
    }

    preview_ready_ = true;
    return outcome(
        StudioAutoloopAutoscriptWorkflowResult::ProposalReady,
        "The deterministic proposal compiled through the production V2 path "
        "and is ready for explicit commit. Preview output is disabled.");
}

StudioAutoloopAutoscriptWorkflowOutcome
StudioAutoloopAutoscriptWorkflow::preview_phase(double phase) {
    if (!preview_ready_ || proposal_ == std::nullopt) {
        return outcome(
            StudioAutoloopAutoscriptWorkflowResult::NoProposal,
            "Generate a successful output-disabled preview first.");
    }
    const auto result = preview_.seek_autoloop_v2_phase(
        proposal_->document_generation(),
        proposal_->document_generation(),
        phase);
    preview_result_ = result.result;
    if (!result) {
        return outcome(
            StudioAutoloopAutoscriptWorkflowResult::PreviewRejected,
            result.message);
    }
    return outcome(
        StudioAutoloopAutoscriptWorkflowResult::PreviewApplied,
        "The output-disabled preview moved to the requested loop phase.");
}

StudioAutoloopAutoscriptWorkflowOutcome
StudioAutoloopAutoscriptWorkflow::commit() {
    if (!proposal_.has_value() || !proposal_->ready()) {
        return outcome(
            StudioAutoloopAutoscriptWorkflowResult::NoProposal,
            "Generate a ready AutoScript proposal before committing.");
    }
    if (!preview_ready_ || committed_) {
        return outcome(
            StudioAutoloopAutoscriptWorkflowResult::PreviewRejected,
            committed_
                ? "This proposal has already been committed."
                : "A production-compiler, output-disabled preview must succeed before commit.");
    }
    const auto mutation = apply_studio_autoloop_autoscript_proposal(
        document_, *proposal_);
    auto result = outcome(
        mutation
            ? StudioAutoloopAutoscriptWorkflowResult::Committed
            : mutation_failure_result(mutation.result),
        mutation.message);
    result.mutation_result = mutation.result;
    if (mutation) {
        committed_ = true;
    }
    return result;
}

StudioAutoloopAutoscriptWorkflowOutcome
StudioAutoloopAutoscriptWorkflow::discard() {
    clear_proposal();
    return outcome(
        StudioAutoloopAutoscriptWorkflowResult::Discarded,
        "The AutoScript proposal and preview were discarded; the document was not changed.");
}

StudioAutoloopAutoscriptWorkflowSnapshot
StudioAutoloopAutoscriptWorkflow::snapshot() const {
    StudioAutoloopAutoscriptWorkflowSnapshot result;
    result.document = document_.snapshot();
    result.has_proposal = proposal_.has_value();
    result.preview_ready = preview_ready_;
    result.committed = committed_;
    result.can_commit = proposal_.has_value() && proposal_->ready() &&
        preview_ready_ && !committed_;
    result.preview_result = preview_result_;
    result.placement_id = placement_id_;
    result.asset_id = asset_id_;
    result.address = address_;
    result.preview = preview_.snapshot();
    if (proposal_.has_value()) {
        result.proposal_ready = proposal_->ready();
        result.proposal_result = proposal_->proposal().result();
        result.proposal_digest =
            std::string(proposal_->proposal().proposal_digest());
        result.preview_source_digest =
            std::string(proposal_->proposal().preview_source_digest());
        result.generated_asset_count =
            proposal_->proposal().generated_asset_ids().size();
        result.generated_event_count =
            proposal_->proposal().generated_event_count();
    }
    return result;
}

void StudioAutoloopAutoscriptWorkflow::clear_proposal() noexcept {
    proposal_.reset();
    preview_ = StudioPreviewService{};
    preview_result_ = StudioPreviewResult::NotLoaded;
    placement_id_.clear();
    asset_id_.clear();
    address_.reset();
    preview_ready_ = false;
    committed_ = false;
}

StudioAutoloopAutoscriptWorkflowOutcome
StudioAutoloopAutoscriptWorkflow::outcome(
    StudioAutoloopAutoscriptWorkflowResult result,
    std::string message) const {
    StudioAutoloopAutoscriptWorkflowOutcome value;
    value.result = result;
    value.message = std::move(message);
    value.preview_result = preview_result_;
    if (proposal_.has_value()) {
        value.proposal_result = proposal_->proposal().result();
    }
    return value;
}

const char* studio_autoloop_autoscript_workflow_result_name(
    StudioAutoloopAutoscriptWorkflowResult result) noexcept {
    switch (result) {
    case StudioAutoloopAutoscriptWorkflowResult::Loaded: return "loaded";
    case StudioAutoloopAutoscriptWorkflowResult::ProposalReady:
        return "proposalReady";
    case StudioAutoloopAutoscriptWorkflowResult::PreviewApplied:
        return "previewApplied";
    case StudioAutoloopAutoscriptWorkflowResult::Committed: return "committed";
    case StudioAutoloopAutoscriptWorkflowResult::Discarded: return "discarded";
    case StudioAutoloopAutoscriptWorkflowResult::InvalidDocument:
        return "invalidDocument";
    case StudioAutoloopAutoscriptWorkflowResult::ProposalRejected:
        return "proposalRejected";
    case StudioAutoloopAutoscriptWorkflowResult::PreviewRejected:
        return "previewRejected";
    case StudioAutoloopAutoscriptWorkflowResult::NoProposal: return "noProposal";
    case StudioAutoloopAutoscriptWorkflowResult::StaleDocument:
        return "staleDocument";
    case StudioAutoloopAutoscriptWorkflowResult::CommitRejected:
        return "commitRejected";
    case StudioAutoloopAutoscriptWorkflowResult::InvalidArgument:
        return "invalidArgument";
    }
    return "invalidArgument";
}

}  // namespace emberlights
