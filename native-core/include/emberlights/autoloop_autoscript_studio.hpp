#pragma once

#include "emberlights/autoloop_autoscript.hpp"
#include "emberlights/studio_document.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace emberlights {

inline constexpr std::uint32_t kStudioAutoloopAutoscriptBridgeVersion = 1U;

// Immutable handoff between output-disabled AutoScript proposal work and the
// authoritative Studio document transaction. The complete persistence stamp
// remains paired with the exact document generation used for the proposal.
class StudioAutoloopAutoscriptProposal {
public:
    StudioAutoloopAutoscriptProposal() = default;

    [[nodiscard]] bool ready() const noexcept {
        return valid_document_source_ && proposal_.ready();
    }
    [[nodiscard]] bool valid_document_source() const noexcept {
        return valid_document_source_;
    }
    [[nodiscard]] StudioDocumentGeneration document_generation()
        const noexcept {
        return document_generation_;
    }
    [[nodiscard]] const PersistedAutoloopSourceStamp& source_stamp()
        const noexcept {
        return source_stamp_;
    }
    [[nodiscard]] std::string_view base_source_digest() const noexcept {
        return base_source_digest_;
    }
    [[nodiscard]] std::string_view bridge_digest() const noexcept {
        return bridge_digest_;
    }
    [[nodiscard]] std::string_view message() const noexcept {
        return message_;
    }
    [[nodiscard]] const AutoloopAutoscriptProposal& proposal()
        const noexcept {
        return proposal_;
    }

private:
    friend StudioAutoloopAutoscriptProposal
    propose_studio_autoloop_autoscript(
        const StudioDocumentSnapshot&,
        AutoloopAutoscriptRequest,
        const AutoloopAutoscriptCancellationToken*);
    friend StudioMutationOutcome apply_studio_autoloop_autoscript_proposal(
        StudioDocumentService&,
        const StudioAutoloopAutoscriptProposal&);

    bool valid_document_source_{false};
    StudioDocumentGeneration document_generation_{0U};
    PersistedAutoloopSourceStamp source_stamp_;
    std::string base_source_digest_;
    std::string bridge_digest_;
    std::string message_;
    AutoloopAutoscriptProposal proposal_;
};

// Uses only the validated persisted rich source carried by the Studio
// snapshot. When no record exists, generation starts from the canonical empty
// rich source rather than adapting or mutating format-1 Autoloops.
[[nodiscard]] StudioAutoloopAutoscriptProposal
propose_studio_autoloop_autoscript(
    const StudioDocumentSnapshot& snapshot,
    AutoloopAutoscriptRequest request,
    const AutoloopAutoscriptCancellationToken* cancellation = nullptr);

// Rechecks the immutable proposal, canonical source bytes, document
// generation, and complete persistence stamp before delegating exactly one
// candidate to StudioDocumentService::apply_autoloop_source. Every refusal is
// no-mutation; success is one document Undo transaction.
[[nodiscard]] StudioMutationOutcome apply_studio_autoloop_autoscript_proposal(
    StudioDocumentService& service,
    const StudioAutoloopAutoscriptProposal& proposal);

}  // namespace emberlights
