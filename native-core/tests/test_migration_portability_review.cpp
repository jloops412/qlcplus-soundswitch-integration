#include "emberlights/migration_portability_review.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

[[nodiscard]] const emberlights::MigrationPortabilitySourceReview& source(
    const emberlights::MigrationPortabilityReview& review,
    emberlights::MigrationPortabilitySource id) {
    return review.sources[static_cast<std::size_t>(id)];
}

[[nodiscard]] const emberlights::MigrationPortabilityStageReview& stage(
    const emberlights::MigrationPortabilitySourceReview& review,
    emberlights::MigrationPortabilityStage id) {
    return review.stages[static_cast<std::size_t>(id)];
}

[[nodiscard]] bool has_blocker(
    const emberlights::MigrationPortabilityStageReview& review,
    std::string_view code) {
    return std::find(
               review.blocker_codes.begin(),
               review.blocker_codes.end(),
               code) != review.blocker_codes.end();
}

emberlights::SoundSwitchSourceBindingAudit exact_identity_audit() {
    emberlights::SoundSwitchSourceBindingAudit audit;
    audit.status =
        emberlights::SoundSwitchSourceBindingStatus::ExactArtifactHashMatch;
    audit.project_claim.present = true;
    audit.project_claim.valid = true;
    audit.project_claim.version = "2.10.synthetic";
    audit.source_inspection_complete = true;
    audit.available_artifact_count = 8U;
    audit.available_track_map_count = 1U;
    audit.available_track_script_count = 1U;
    audit.available_audio_count = 1U;
    audit.exact_artifact_hash_match = true;
    audit.semantic_import_qualified = false;
    audit.review_state =
        emberlights::SoundSwitchMigrationReviewState::ReadyForManualReview;
    audit.project_valid = true;
    audit.outputs_disabled = true;
    // Sentinels must never leak into the portability report.
    audit.available_inventory_sha256 =
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    audit.available_venue_sha256 =
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
    audit.available_autoloops_sha256 =
        "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc";
    return audit;
}

void test_lifecycle_order_and_identity_only_boundary() {
    const auto audit = exact_identity_audit();
    const auto review =
        emberlights::build_migration_portability_review(audit);
    require(
        review.format == emberlights::kMigrationPortabilityReviewFormat,
        "format name is stable");
    require(
        review.format_version == 1U,
        "format version is stable");

    const auto& soundswitch = source(
        review, emberlights::MigrationPortabilitySource::SoundSwitch);
    for (std::size_t index = 0U; index < soundswitch.stages.size(); ++index) {
        require(
            soundswitch.stages[index].stage ==
                static_cast<emberlights::MigrationPortabilityStage>(index),
            "SoundSwitch lifecycle uses canonical stage order");
    }
    require(soundswitch.artifact_identity_verified, "artifact identity is verified");
    require(
        !soundswitch.semantic_decoder_qualified,
        "hash identity does not qualify semantic decoding");
    require(
        !soundswitch.semantic_import_claimed,
        "hash identity does not claim semantic import");
    require(!soundswitch.source_bytes_included, "source bytes are excluded");

    const auto& probe = stage(
        soundswitch, emberlights::MigrationPortabilityStage::Probe);
    const auto& inventory = stage(
        soundswitch, emberlights::MigrationPortabilityStage::Inventory);
    const auto& decode = stage(
        soundswitch, emberlights::MigrationPortabilityStage::Decode);
    const auto& reconcile = stage(
        soundswitch, emberlights::MigrationPortabilityStage::Reconcile);
    const auto& plan = stage(
        soundswitch, emberlights::MigrationPortabilityStage::Plan);
    const auto& commit = stage(
        soundswitch, emberlights::MigrationPortabilityStage::Commit);
    require(
        probe.readiness == emberlights::MigrationPortabilityReadiness::Ready,
        "completed read-only probe is ready");
    require(
        inventory.readiness == emberlights::MigrationPortabilityReadiness::Ready,
        "content-safe inventory is ready");
    require(
        decode.readiness ==
            emberlights::MigrationPortabilityReadiness::EvidenceUnavailable,
        "semantic decode remains evidence unavailable");
    require(
        has_blocker(decode, "soundswitch.semantic_import_unqualified"),
        "decode names semantic qualification blocker");
    require(
        has_blocker(
            decode, "soundswitch.autoloop_delta_corpus_unavailable"),
        "decode names exact Autoloop delta blocker");
    require(
        reconcile.readiness ==
            emberlights::MigrationPortabilityReadiness::ReadyForManualReview,
        "exact identity only makes reconciliation ready for manual review");
    require(
        plan.readiness == emberlights::MigrationPortabilityReadiness::Blocked,
        "plan is blocked without qualified decode");
    require(
        commit.readiness == emberlights::MigrationPortabilityReadiness::Blocked,
        "commit is blocked without qualified decode");

    const auto json = emberlights::serialize_migration_portability_review(review);
    require(
        json.find("\"artifactIdentityVerified\": true") != std::string::npos,
        "serialized report exposes identity as a boolean only");
    require(
        json.find("\"semanticImportClaimed\": false") != std::string::npos,
        "serialized report refuses semantic claim");
    require(
        json.find(audit.available_inventory_sha256) == std::string::npos,
        "inventory digest is excluded");
    require(
        json.find(audit.available_venue_sha256) == std::string::npos,
        "venue digest is excluded");
    require(
        json.find(audit.available_autoloops_sha256) == std::string::npos,
        "Autoloop digest is excluded");
}

void test_soundswitch_exact_review_blockers() {
    auto audit = exact_identity_audit();
    audit.status = emberlights::SoundSwitchSourceBindingStatus::SourceMismatch;
    audit.exact_artifact_hash_match = false;
    audit.review_state =
        emberlights::SoundSwitchMigrationReviewState::SourceEvidenceBlocked;
    auto review = emberlights::build_migration_portability_review(audit);
    auto& soundswitch = source(
        review, emberlights::MigrationPortabilitySource::SoundSwitch);
    const auto& reconcile = stage(
        soundswitch, emberlights::MigrationPortabilityStage::Reconcile);
    require(
        reconcile.readiness == emberlights::MigrationPortabilityReadiness::Blocked,
        "mismatched source blocks reconciliation");
    require(
        has_blocker(reconcile, "soundswitch.source_binding_mismatch"),
        "mismatch uses stable blocker code");
    require(
        soundswitch.source_binding_status_code == "sourceMismatch",
        "existing SoundSwitch binding status taxonomy is retained");

    audit = exact_identity_audit();
    audit.outputs_disabled = false;
    audit.review_state =
        emberlights::SoundSwitchMigrationReviewState::OutputMustBeDisabled;
    review = emberlights::build_migration_portability_review(audit);
    const auto& unsafe_commit = stage(
        source(review, emberlights::MigrationPortabilitySource::SoundSwitch),
        emberlights::MigrationPortabilityStage::Commit);
    require(
        has_blocker(unsafe_commit, "migration.output_must_be_disabled"),
        "enabled output produces exact safety blocker");

    audit = exact_identity_audit();
    audit.project_valid = false;
    audit.review_state =
        emberlights::SoundSwitchMigrationReviewState::ProjectValidationBlocked;
    review = emberlights::build_migration_portability_review(audit);
    const auto& invalid_commit = stage(
        source(review, emberlights::MigrationPortabilitySource::SoundSwitch),
        emberlights::MigrationPortabilityStage::Commit);
    require(
        has_blocker(
            invalid_commit, "soundswitch.project_validation_failed"),
        "invalid project produces exact validation blocker");
}

void test_wolfmix_is_explicit_research_only() {
    const auto review = emberlights::build_migration_portability_review(
        exact_identity_audit());
    const auto& wolfmix = source(
        review, emberlights::MigrationPortabilitySource::Wolfmix);
    require(wolfmix.research_only, "WOLFMIX is marked research only");
    require(
        !wolfmix.source_inspection_available,
        "WOLFMIX source inspection is not claimed");
    require(
        !wolfmix.artifact_identity_verified,
        "WOLFMIX artifact identity is not claimed");
    require(
        !wolfmix.semantic_decoder_qualified,
        "WOLFMIX semantic decoder is not claimed");
    require(
        !wolfmix.semantic_import_claimed,
        "WOLFMIX import is not claimed");
    require(!wolfmix.source_bytes_included, "WOLFMIX source bytes are absent");
    for (std::size_t index = 0U; index < wolfmix.stages.size(); ++index) {
        const auto& item = wolfmix.stages[index];
        require(
            item.stage ==
                static_cast<emberlights::MigrationPortabilityStage>(index),
            "WOLFMIX lifecycle uses canonical stage order");
        require(
            item.readiness ==
                emberlights::MigrationPortabilityReadiness::EvidenceUnavailable,
            "every WOLFMIX stage is evidence unavailable");
        require(
            item.evidence_tier ==
                emberlights::MigrationPortabilityEvidenceTier::Unresolved,
            "every WOLFMIX stage is unresolved");
        require(
            has_blocker(
                item, "wolfmix.controlled_delta_corpus_unavailable"),
            "every WOLFMIX stage names controlled-delta blocker");
    }

    const auto json = emberlights::serialize_migration_portability_review(review);
    require(
        json.find("wolfmix.controlled_delta_corpus_unavailable") !=
            std::string::npos,
        "serialized WOLFMIX report names evidence blocker");
    require(
        json.find("\"sourceBytesIncluded\": false") != std::string::npos,
        "serialized sources exclude bytes");
    require(
        json.find("Color FX") == std::string::npos &&
            json.find("Move FX") == std::string::npos &&
            json.find("Beam FX") == std::string::npos,
        "report does not turn manual/UI hypotheses into import semantics");
}

void test_stable_names() {
    require(
        std::string_view(emberlights::migration_portability_stage_name(
            emberlights::MigrationPortabilityStage::Probe)) == "probe",
        "probe name is stable");
    require(
        std::string_view(emberlights::migration_portability_stage_name(
            emberlights::MigrationPortabilityStage::Upgrade)) == "upgrade",
        "upgrade name is stable");
    require(
        std::string_view(emberlights::migration_portability_evidence_tier_name(
            emberlights::MigrationPortabilityEvidenceTier::ContractTested)) ==
            "contractTested",
        "contract-tested evidence name is stable");
}

}  // namespace

int main() {
    test_lifecycle_order_and_identity_only_boundary();
    test_soundswitch_exact_review_blockers();
    test_wolfmix_is_explicit_research_only();
    test_stable_names();
    std::cout << "migration_portability_review_tests: PASS\n";
    return 0;
}
