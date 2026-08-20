#include "emberlights/autoloop_autoscript_studio.hpp"

#include "emberlights/file_identity.hpp"

#include <array>
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace emberlights {
namespace {

template <typename Integer>
void append_integer(std::string& output, Integer value) {
    std::array<char, 32U> buffer{};
    const auto converted = std::to_chars(
        buffer.data(), buffer.data() + buffer.size(), value);
    if (converted.ec == std::errc{}) {
        output.append(buffer.data(), converted.ptr);
    }
    output.push_back(';');
}

void append_text(std::string& output, std::string_view value) {
    append_integer(output, value.size());
    output.append(value);
    output.push_back(';');
}

[[nodiscard]] std::string autoscript_proposal_digest(
    const AutoloopAutoscriptProposal& proposal) {
    std::string canonical;
    canonical.reserve(1024U);
    append_text(canonical, "emberlights.autoscript.proposal");
    append_integer(canonical, kAutoloopAutoscriptGeneratorVersion);
    append_integer(
        canonical, static_cast<std::uint8_t>(proposal.result()));
    append_integer(canonical, proposal.base_generation());
    append_text(canonical, proposal.base_source_digest());
    append_text(canonical, proposal.request_digest());
    append_text(canonical, proposal.preview_source_digest());
    append_integer(canonical, proposal.generated_event_count());
    append_integer(canonical, proposal.operations_used());
    append_integer(canonical, proposal.diagnostics().size());
    for (const auto& diagnostic : proposal.diagnostics()) {
        append_integer(
            canonical, static_cast<std::uint8_t>(diagnostic.severity));
        append_text(canonical, diagnostic.code);
        append_text(canonical, diagnostic.subject);
        append_text(canonical, diagnostic.message);
    }
    return sha256_text(canonical);
}

[[nodiscard]] std::string studio_bridge_digest(
    StudioDocumentGeneration document_generation,
    const PersistedAutoloopSourceStamp& stamp,
    std::string_view base_source_digest,
    const AutoloopAutoscriptProposal& proposal) {
    std::string canonical;
    canonical.reserve(2048U);
    append_text(canonical, "emberlights.autoscript.studio-bridge");
    append_integer(canonical, kStudioAutoloopAutoscriptBridgeVersion);
    append_integer(canonical, document_generation);
    append_integer(canonical, stamp.present ? 1U : 0U);
    append_integer(canonical, stamp.record_version);
    append_integer(canonical, stamp.source_format_version);
    append_text(canonical, stamp.source_digest);
    append_text(canonical, base_source_digest);
    append_integer(
        canonical, static_cast<std::uint8_t>(proposal.result()));
    append_text(canonical, proposal.request_digest());
    append_text(canonical, proposal.proposal_digest());
    append_text(canonical, proposal.preview_source_digest());
    append_integer(canonical, proposal.generated_event_count());
    append_integer(canonical, proposal.operations_used());
    append_integer(canonical, proposal.generated_asset_ids().size());
    for (const auto& id : proposal.generated_asset_ids()) {
        append_text(canonical, id);
    }
    append_integer(canonical, proposal.generated_placement_ids().size());
    for (const auto& id : proposal.generated_placement_ids()) {
        append_text(canonical, id);
    }
    append_integer(canonical, proposal.generated_addresses().size());
    for (const auto& address : proposal.generated_addresses()) {
        append_integer(canonical, address.bank);
        append_integer(canonical, address.slot);
    }
    append_integer(canonical, proposal.diagnostics().size());
    for (const auto& diagnostic : proposal.diagnostics()) {
        append_integer(
            canonical, static_cast<std::uint8_t>(diagnostic.severity));
        append_text(canonical, diagnostic.code);
        append_text(canonical, diagnostic.subject);
        append_text(canonical, diagnostic.message);
    }
    return sha256_text(canonical);
}

[[nodiscard]] StudioMutationOutcome failed_commit(
    const StudioDocumentSnapshot& current,
    StudioMutationResult result,
    std::string message) {
    StudioMutationOutcome outcome;
    outcome.result = result;
    outcome.generation = current.generation;
    outcome.validation = validate_project(current.document);
    outcome.message = std::move(message);
    outcome.persistence_error = current.autoloop_source.error;
    return outcome;
}

[[nodiscard]] bool valid_persisted_source_snapshot(
    const StudioDocumentSnapshot& snapshot,
    AutoloopSourceDocument& source,
    std::string& digest,
    std::string& message) {
    if (!snapshot.autoloop_source) {
        message =
            "The Studio snapshot contains an invalid persisted Autoloop source: " +
            snapshot.autoloop_source.message;
        return false;
    }

    const auto& stamp = snapshot.autoloop_source.stamp;
    if (!stamp.present) {
        if (stamp != PersistedAutoloopSourceStamp{}) {
            message =
                "The absent persisted Autoloop source has a non-empty version or digest stamp.";
            return false;
        }
        source = AutoloopSourceDocument{};
    } else {
        if (stamp.record_version != kPersistedAutoloopSourceRecordVersion ||
            stamp.source_format_version != kAutoloopSourceFormatVersion ||
            !is_sha256_digest(stamp.source_digest)) {
            message =
                "The persisted Autoloop source stamp is unsupported or malformed.";
            return false;
        }
        source = snapshot.autoloop_source.source;
    }

    const auto validation = validate_autoloop_source(source);
    digest = autoloop_source_digest(source);
    if (!validation.ok() || !is_sha256_digest(digest)) {
        message =
            "The Studio snapshot's effective Autoloop source is invalid or non-canonical.";
        return false;
    }
    if (stamp.present && digest != stamp.source_digest) {
        message =
            "The Studio snapshot's persisted Autoloop source digest does not match its full stamp.";
        return false;
    }
    return true;
}

[[nodiscard]] bool valid_ready_proposal(
    const StudioAutoloopAutoscriptProposal& bridge) {
    const auto& proposal = bridge.proposal();
    if (!bridge.valid_document_source() || !proposal.ready() ||
        proposal.base_generation() != bridge.document_generation() ||
        proposal.base_source_digest() != bridge.base_source_digest() ||
        !is_sha256_digest(proposal.request_digest()) ||
        !is_sha256_digest(proposal.proposal_digest()) ||
        !is_sha256_digest(proposal.preview_source_digest()) ||
        autoscript_proposal_digest(proposal) != proposal.proposal_digest()) {
        return false;
    }

    const auto serialized = serialize_autoloop_source(
        proposal.preview_source());
    if (serialized.empty() || sha256_text(serialized) !=
            proposal.preview_source_digest() ||
        !validate_autoloop_source(proposal.preview_source()).ok()) {
        return false;
    }

    return studio_bridge_digest(
               bridge.document_generation(), bridge.source_stamp(),
               bridge.base_source_digest(), proposal) ==
        bridge.bridge_digest();
}

}  // namespace

StudioAutoloopAutoscriptProposal propose_studio_autoloop_autoscript(
    const StudioDocumentSnapshot& snapshot,
    AutoloopAutoscriptRequest request,
    const AutoloopAutoscriptCancellationToken* cancellation) {
    StudioAutoloopAutoscriptProposal bridge;
    bridge.document_generation_ = snapshot.generation;
    bridge.source_stamp_ = snapshot.autoloop_source.stamp;

    AutoloopSourceDocument base_source;
    if (!valid_persisted_source_snapshot(
            snapshot, base_source, bridge.base_source_digest_,
            bridge.message_)) {
        return bridge;
    }

    bridge.valid_document_source_ = true;
    AutoloopAuthoringSnapshot authoring_snapshot;
    authoring_snapshot.source = std::move(base_source);
    authoring_snapshot.generation = snapshot.generation;
    authoring_snapshot.source_digest = bridge.base_source_digest_;
    bridge.proposal_ = propose_autoloop_autoscript(
        authoring_snapshot, std::move(request), cancellation);
    bridge.bridge_digest_ = studio_bridge_digest(
        bridge.document_generation_, bridge.source_stamp_,
        bridge.base_source_digest_, bridge.proposal_);
    bridge.message_ = bridge.proposal_.ready()
        ? "The Studio AutoScript proposal is ready for review and explicit commit."
        : "The Studio AutoScript proposal was not ready; the document remains unchanged.";
    return bridge;
}

StudioMutationOutcome apply_studio_autoloop_autoscript_proposal(
    StudioDocumentService& service,
    const StudioAutoloopAutoscriptProposal& bridge) {
    const auto current = service.snapshot();
    if (!bridge.ready()) {
        return failed_commit(
            current, StudioMutationResult::InvalidCandidate,
            "Only a ready Studio AutoScript proposal can be committed.");
    }
    if (current.generation != bridge.document_generation_ ||
        current.autoloop_source.stamp != bridge.source_stamp_) {
        return failed_commit(
            current, StudioMutationResult::StaleGeneration,
            "The Studio document generation or persisted Autoloop source stamp changed after proposal preview.");
    }
    if (!current.autoloop_source) {
        return failed_commit(
            current, StudioMutationResult::InvalidCandidate,
            "The active Studio document contains an invalid persisted Autoloop source.");
    }

    AutoloopSourceDocument current_source;
    std::string current_digest;
    std::string source_message;
    if (!valid_persisted_source_snapshot(
            current, current_source, current_digest, source_message)) {
        return failed_commit(
            current, StudioMutationResult::InvalidCandidate,
            std::move(source_message));
    }
    if (current_digest != bridge.base_source_digest_) {
        return failed_commit(
            current, StudioMutationResult::StaleGeneration,
            "The active persisted Autoloop source no longer matches the proposal base.");
    }
    if (!valid_ready_proposal(bridge)) {
        return failed_commit(
            current, StudioMutationResult::InvalidCandidate,
            "The Studio AutoScript proposal failed its immutable proposal or source digest check.");
    }

    return service.apply_autoloop_source(
        bridge.document_generation_, bridge.source_stamp_,
        bridge.proposal_.preview_source());
}

}  // namespace emberlights
