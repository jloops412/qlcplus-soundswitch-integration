#include "emberlights/migration_portability_review.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <utility>

namespace emberlights {
namespace {

using StageReview = MigrationPortabilityStageReview;
using Readiness = MigrationPortabilityReadiness;
using EvidenceTier = MigrationPortabilityEvidenceTier;

[[nodiscard]] constexpr std::size_t stage_index(
    MigrationPortabilityStage stage) noexcept {
    return static_cast<std::size_t>(stage);
}

[[nodiscard]] std::string json_escape(std::string_view value) {
    std::ostringstream output;
    for (const auto character : value) {
        switch (character) {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (static_cast<unsigned char>(character) < 0x20U) {
                output << "\\u" << std::hex << std::setw(4)
                       << std::setfill('0')
                       << static_cast<unsigned int>(
                              static_cast<unsigned char>(character));
            } else {
                output << character;
            }
        }
    }
    return output.str();
}

void add_blocker_once(StageReview& review, std::string blocker) {
    if (std::find(
            review.blocker_codes.begin(),
            review.blocker_codes.end(),
            blocker) == review.blocker_codes.end()) {
        review.blocker_codes.push_back(std::move(blocker));
    }
}

[[nodiscard]] const char* source_binding_status_code(
    SoundSwitchSourceBindingStatus status) noexcept {
    // Keep the exact public strings from soundswitch_source_binding.cpp while
    // leaving this pure view-model module independently linkable.
    switch (status) {
    case SoundSwitchSourceBindingStatus::ExactArtifactHashMatch:
        return "exactArtifactHashMatch";
    case SoundSwitchSourceBindingStatus::SourceMismatch:
        return "sourceMismatch";
    case SoundSwitchSourceBindingStatus::ProjectClaimMissing:
        return "projectClaimMissing";
    case SoundSwitchSourceBindingStatus::ProjectClaimMalformed:
        return "projectClaimMalformed";
    case SoundSwitchSourceBindingStatus::SourceInspectionIncomplete:
        return "sourceInspectionIncomplete";
    case SoundSwitchSourceBindingStatus::SourceArtifactsAmbiguous:
        return "sourceArtifactsAmbiguous";
    }
    return "projectClaimMissing";
}

[[nodiscard]] const char* review_state_code(
    SoundSwitchMigrationReviewState state) noexcept {
    switch (state) {
    case SoundSwitchMigrationReviewState::ReadyForManualReview:
        return "readyForManualReview";
    case SoundSwitchMigrationReviewState::SourceEvidenceBlocked:
        return "sourceEvidenceBlocked";
    case SoundSwitchMigrationReviewState::ProjectValidationBlocked:
        return "projectValidationBlocked";
    case SoundSwitchMigrationReviewState::OutputMustBeDisabled:
        return "outputMustBeDisabled";
    }
    return "sourceEvidenceBlocked";
}

void add_source_binding_blocker(
    StageReview& stage,
    SoundSwitchSourceBindingStatus status) {
    switch (status) {
    case SoundSwitchSourceBindingStatus::ExactArtifactHashMatch:
        break;
    case SoundSwitchSourceBindingStatus::SourceMismatch:
        add_blocker_once(stage, "soundswitch.source_binding_mismatch");
        break;
    case SoundSwitchSourceBindingStatus::ProjectClaimMissing:
        add_blocker_once(stage, "soundswitch.project_claim_missing");
        break;
    case SoundSwitchSourceBindingStatus::ProjectClaimMalformed:
        add_blocker_once(stage, "soundswitch.project_claim_malformed");
        break;
    case SoundSwitchSourceBindingStatus::SourceInspectionIncomplete:
        add_blocker_once(stage, "soundswitch.source_inspection_incomplete");
        break;
    case SoundSwitchSourceBindingStatus::SourceArtifactsAmbiguous:
        add_blocker_once(stage, "soundswitch.source_artifacts_ambiguous");
        break;
    }
}

void add_review_blockers(
    StageReview& stage,
    const SoundSwitchSourceBindingAudit& audit) {
    switch (audit.review_state) {
    case SoundSwitchMigrationReviewState::ReadyForManualReview:
        break;
    case SoundSwitchMigrationReviewState::SourceEvidenceBlocked:
        add_source_binding_blocker(stage, audit.status);
        break;
    case SoundSwitchMigrationReviewState::ProjectValidationBlocked:
        add_blocker_once(stage, "soundswitch.project_validation_failed");
        break;
    case SoundSwitchMigrationReviewState::OutputMustBeDisabled:
        add_blocker_once(stage, "migration.output_must_be_disabled");
        break;
    }
}

[[nodiscard]] StageReview make_stage(
    MigrationPortabilityStage stage,
    Readiness readiness,
    EvidenceTier tier,
    std::string code,
    std::string summary) {
    return {stage, readiness, tier, std::move(code), {}, std::move(summary)};
}

[[nodiscard]] MigrationPortabilitySourceReview build_soundswitch_review(
    const SoundSwitchSourceBindingAudit& audit) {
    MigrationPortabilitySourceReview source;
    source.source = MigrationPortabilitySource::SoundSwitch;
    source.label = "SoundSwitch";
    source.source_binding_status_code = source_binding_status_code(audit.status);
    source.review_state_code = review_state_code(audit.review_state);
    source.source_inspection_available = audit.source_inspection_complete;
    source.artifact_identity_verified = audit.exact_artifact_hash_match;
    source.semantic_decoder_qualified = audit.semantic_import_qualified;
    source.semantic_import_claimed = audit.semantic_import_qualified;
    source.source_bytes_included = false;

    auto& probe = source.stages[stage_index(MigrationPortabilityStage::Probe)];
    if (audit.source_inspection_complete) {
        probe = make_stage(
            MigrationPortabilityStage::Probe,
            Readiness::Ready,
            EvidenceTier::Verified,
            "soundswitch.probe.ready",
            "The bounded read-only source probe completed.");
    } else {
        probe = make_stage(
            MigrationPortabilityStage::Probe,
            Readiness::Blocked,
            EvidenceTier::Unresolved,
            "soundswitch.probe.blocked",
            "The bounded read-only source probe is incomplete.");
        add_blocker_once(probe, "soundswitch.source_inspection_incomplete");
    }

    auto& inventory =
        source.stages[stage_index(MigrationPortabilityStage::Inventory)];
    if (!audit.source_inspection_complete) {
        inventory = make_stage(
            MigrationPortabilityStage::Inventory,
            Readiness::Blocked,
            EvidenceTier::Unresolved,
            "soundswitch.inventory.blocked",
            "A trustworthy artifact inventory requires a complete probe.");
        add_blocker_once(inventory, "soundswitch.source_inspection_incomplete");
    } else if (audit.available_artifact_count == 0U) {
        inventory = make_stage(
            MigrationPortabilityStage::Inventory,
            Readiness::EvidenceUnavailable,
            EvidenceTier::Unresolved,
            "soundswitch.inventory.evidence_unavailable",
            "The completed inspection contains no source artifacts.");
        add_blocker_once(inventory, "soundswitch.inventory_empty");
    } else {
        inventory = make_stage(
            MigrationPortabilityStage::Inventory,
            Readiness::Ready,
            EvidenceTier::Verified,
            "soundswitch.inventory.ready",
            "The content-safe artifact inventory is available.");
    }

    auto& decode = source.stages[stage_index(MigrationPortabilityStage::Decode)];
    if (audit.semantic_import_qualified) {
        decode = make_stage(
            MigrationPortabilityStage::Decode,
            Readiness::Ready,
            EvidenceTier::Verified,
            "soundswitch.decode.qualified",
            "The supplied audit marks semantic decoding as qualified.");
    } else {
        decode = make_stage(
            MigrationPortabilityStage::Decode,
            Readiness::EvidenceUnavailable,
            EvidenceTier::Unresolved,
            "soundswitch.decode.evidence_unavailable",
            "Source identity and inventory do not qualify semantic decoding.");
        add_blocker_once(decode, "soundswitch.semantic_import_unqualified");
        add_blocker_once(
            decode, "soundswitch.autoloop_delta_corpus_unavailable");
        if (!audit.project_claim.valid || audit.project_claim.version.empty()) {
            add_blocker_once(decode, "soundswitch.source_version_unverified");
        }
        if (audit.available_track_map_count == 0U) {
            add_blocker_once(decode, "soundswitch.track_map_unavailable");
        }
        if (audit.available_track_script_count == 0U &&
            audit.available_recordable_data_count == 0U) {
            add_blocker_once(decode, "soundswitch.lighting_files_unavailable");
        }
        if (audit.available_audio_count == 0U) {
            add_blocker_once(decode, "soundswitch.scripted_audio_unavailable");
        }
        // The current source-binding audit has no DJ-library corpus input.
        add_blocker_once(
            decode, "soundswitch.dj_library_identity_unavailable");
        add_source_binding_blocker(decode, audit.status);
    }

    auto& reconcile =
        source.stages[stage_index(MigrationPortabilityStage::Reconcile)];
    if (audit.review_state ==
        SoundSwitchMigrationReviewState::ReadyForManualReview) {
        reconcile = make_stage(
            MigrationPortabilityStage::Reconcile,
            Readiness::ReadyForManualReview,
            EvidenceTier::Verified,
            "soundswitch.reconcile.ready_for_manual_review",
            "Source identity is verified for manual review; semantic coverage remains separately unqualified.");
    } else {
        reconcile = make_stage(
            MigrationPortabilityStage::Reconcile,
            Readiness::Blocked,
            audit.source_inspection_complete
                ? EvidenceTier::Verified
                : EvidenceTier::Unresolved,
            "soundswitch.reconcile.blocked",
            "The current source-binding review blocks reconciliation.");
        add_review_blockers(reconcile, audit);
    }

    auto& plan = source.stages[stage_index(MigrationPortabilityStage::Plan)];
    if (audit.semantic_import_qualified &&
        audit.review_state ==
            SoundSwitchMigrationReviewState::ReadyForManualReview) {
        plan = make_stage(
            MigrationPortabilityStage::Plan,
            Readiness::ReadyForManualReview,
            EvidenceTier::ContractTested,
            "soundswitch.plan.ready_for_manual_review",
            "Qualified decoded items may enter a reviewed destination proposal.");
    } else {
        plan = make_stage(
            MigrationPortabilityStage::Plan,
            Readiness::Blocked,
            EvidenceTier::Unresolved,
            "soundswitch.plan.blocked",
            "A reviewed destination plan requires qualified decoded items.");
        add_blocker_once(plan, "migration.decode_not_ready");
        add_review_blockers(plan, audit);
    }

    auto& commit = source.stages[stage_index(MigrationPortabilityStage::Commit)];
    if (audit.semantic_import_qualified && audit.project_valid &&
        audit.outputs_disabled &&
        audit.review_state ==
            SoundSwitchMigrationReviewState::ReadyForManualReview) {
        commit = make_stage(
            MigrationPortabilityStage::Commit,
            Readiness::ReadyForManualReview,
            EvidenceTier::ContractTested,
            "soundswitch.commit.ready_for_manual_review",
            "A reviewed output-disabled Studio transaction may be prepared.");
    } else {
        commit = make_stage(
            MigrationPortabilityStage::Commit,
            Readiness::Blocked,
            EvidenceTier::Unresolved,
            "soundswitch.commit.blocked",
            "Commit is unavailable until decode, validation, review, and output-safety gates pass.");
        if (!audit.semantic_import_qualified) {
            add_blocker_once(commit, "migration.decode_not_ready");
        }
        if (!audit.project_valid) {
            add_blocker_once(commit, "soundswitch.project_validation_failed");
        }
        if (!audit.outputs_disabled) {
            add_blocker_once(commit, "migration.output_must_be_disabled");
        }
        add_review_blockers(commit, audit);
    }

    auto& upgrade = source.stages[stage_index(MigrationPortabilityStage::Upgrade)];
    upgrade = make_stage(
        MigrationPortabilityStage::Upgrade,
        Readiness::EvidenceUnavailable,
        EvidenceTier::Unresolved,
        "soundswitch.upgrade.evidence_unavailable",
        "No qualified decoder-upgrade/re-import claim is established by the source-binding audit.");
    add_blocker_once(
        upgrade, "soundswitch.decoder_upgrade_evidence_unavailable");

    return source;
}

[[nodiscard]] MigrationPortabilitySourceReview build_wolfmix_review() {
    MigrationPortabilitySourceReview source;
    source.source = MigrationPortabilitySource::Wolfmix;
    source.label = "WOLFMIX";
    source.source_binding_status_code = "researchEvidenceUnavailable";
    source.review_state_code = "sourceEvidenceBlocked";
    source.research_only = true;
    source.source_inspection_available = false;
    source.artifact_identity_verified = false;
    source.semantic_decoder_qualified = false;
    source.semantic_import_claimed = false;
    source.source_bytes_included = false;

    for (std::size_t index = 0U; index < source.stages.size(); ++index) {
        const auto stage = static_cast<MigrationPortabilityStage>(index);
        auto& review = source.stages[index];
        review = make_stage(
            stage,
            Readiness::EvidenceUnavailable,
            EvidenceTier::Unresolved,
            std::string("wolfmix.") + migration_portability_stage_name(stage) +
                ".evidence_unavailable",
            "WOLFMIX remains a research-only compatibility target; no authorized versioned controlled-delta corpus or source decoder is qualified.");
        add_blocker_once(
            review, "wolfmix.controlled_delta_corpus_unavailable");
    }
    return source;
}

void append_string_array(
    std::ostringstream& output,
    const std::vector<std::string>& values) {
    output << '[';
    for (std::size_t index = 0U; index < values.size(); ++index) {
        if (index != 0U) output << ", ";
        output << '"' << json_escape(values[index]) << '"';
    }
    output << ']';
}

}  // namespace

MigrationPortabilityReview build_migration_portability_review(
    const SoundSwitchSourceBindingAudit& soundswitch_audit) {
    MigrationPortabilityReview review;
    review.sources[static_cast<std::size_t>(
        MigrationPortabilitySource::SoundSwitch)] =
        build_soundswitch_review(soundswitch_audit);
    review.sources[static_cast<std::size_t>(MigrationPortabilitySource::Wolfmix)] =
        build_wolfmix_review();
    return review;
}

std::string serialize_migration_portability_review(
    const MigrationPortabilityReview& review) {
    std::ostringstream output;
    output << "{\n"
           << "  \"format\": \"" << json_escape(review.format) << "\",\n"
           << "  \"formatVersion\": " << review.format_version << ",\n"
           << "  \"sources\": [\n";
    for (std::size_t source_index = 0U;
         source_index < review.sources.size(); ++source_index) {
        const auto& source = review.sources[source_index];
        output << "    {\n"
               << "      \"source\": \""
               << migration_portability_source_name(source.source) << "\",\n"
               << "      \"label\": \"" << json_escape(source.label)
               << "\",\n"
               << "      \"sourceBindingStatus\": \""
               << json_escape(source.source_binding_status_code) << "\",\n"
               << "      \"reviewState\": \""
               << json_escape(source.review_state_code) << "\",\n"
               << "      \"researchOnly\": "
               << (source.research_only ? "true" : "false") << ",\n"
               << "      \"sourceInspectionAvailable\": "
               << (source.source_inspection_available ? "true" : "false")
               << ",\n"
               << "      \"artifactIdentityVerified\": "
               << (source.artifact_identity_verified ? "true" : "false")
               << ",\n"
               << "      \"semanticDecoderQualified\": "
               << (source.semantic_decoder_qualified ? "true" : "false")
               << ",\n"
               << "      \"semanticImportClaimed\": "
               << (source.semantic_import_claimed ? "true" : "false")
               << ",\n"
               << "      \"sourceBytesIncluded\": "
               << (source.source_bytes_included ? "true" : "false") << ",\n"
               << "      \"stages\": [\n";
        for (std::size_t stage_index_value = 0U;
             stage_index_value < source.stages.size(); ++stage_index_value) {
            const auto& stage = source.stages[stage_index_value];
            output << "        {\"stage\": \""
                   << migration_portability_stage_name(stage.stage)
                   << "\", \"readiness\": \""
                   << migration_portability_readiness_name(stage.readiness)
                   << "\", \"evidenceTier\": \""
                   << migration_portability_evidence_tier_name(
                          stage.evidence_tier)
                   << "\", \"readinessCode\": \""
                   << json_escape(stage.readiness_code)
                   << "\", \"blockerCodes\": ";
            append_string_array(output, stage.blocker_codes);
            output << ", \"summary\": \"" << json_escape(stage.summary)
                   << "\"}";
            if (stage_index_value + 1U != source.stages.size()) output << ',';
            output << '\n';
        }
        output << "      ]\n"
               << "    }";
        if (source_index + 1U != review.sources.size()) output << ',';
        output << '\n';
    }
    output << "  ]\n}\n";
    return output.str();
}

const char* migration_portability_source_name(
    MigrationPortabilitySource source) noexcept {
    switch (source) {
    case MigrationPortabilitySource::SoundSwitch: return "soundswitch";
    case MigrationPortabilitySource::Wolfmix: return "wolfmix";
    case MigrationPortabilitySource::Count: break;
    }
    return "soundswitch";
}

const char* migration_portability_stage_name(
    MigrationPortabilityStage stage) noexcept {
    switch (stage) {
    case MigrationPortabilityStage::Probe: return "probe";
    case MigrationPortabilityStage::Inventory: return "inventory";
    case MigrationPortabilityStage::Decode: return "decode";
    case MigrationPortabilityStage::Reconcile: return "reconcile";
    case MigrationPortabilityStage::Plan: return "plan";
    case MigrationPortabilityStage::Commit: return "commit";
    case MigrationPortabilityStage::Upgrade: return "upgrade";
    case MigrationPortabilityStage::Count: break;
    }
    return "probe";
}

const char* migration_portability_readiness_name(
    MigrationPortabilityReadiness readiness) noexcept {
    switch (readiness) {
    case MigrationPortabilityReadiness::Ready: return "ready";
    case MigrationPortabilityReadiness::ReadyForManualReview:
        return "readyForManualReview";
    case MigrationPortabilityReadiness::Blocked: return "blocked";
    case MigrationPortabilityReadiness::EvidenceUnavailable:
        return "evidenceUnavailable";
    }
    return "evidenceUnavailable";
}

const char* migration_portability_evidence_tier_name(
    MigrationPortabilityEvidenceTier tier) noexcept {
    switch (tier) {
    case MigrationPortabilityEvidenceTier::Verified: return "verified";
    case MigrationPortabilityEvidenceTier::ContractTested:
        return "contractTested";
    case MigrationPortabilityEvidenceTier::Inferred: return "inferred";
    case MigrationPortabilityEvidenceTier::Unresolved: return "unresolved";
    }
    return "unresolved";
}

}  // namespace emberlights
