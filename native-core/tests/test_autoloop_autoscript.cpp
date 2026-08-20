#include "emberlights/autoloop_autoscript.hpp"

#include "showcore/autoloop_program.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

int failures = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            std::cerr << "CHECK failed at " << __FILE__ << ':' << __LINE__     \
                      << ": " #condition << '\n';                              \
            ++failures;                                                        \
        }                                                                       \
    } while (false)

template <typename Collection>
[[nodiscard]] auto find_by_id(Collection& collection, std::string_view id) {
    return std::find_if(
        collection.begin(), collection.end(),
        [id](const auto& value) { return value.id == id; });
}

[[nodiscard]] emberlights::AutoloopAssetBundle make_bundle(
    std::string prefix,
    std::string name) {
    emberlights::AutoloopAssetBundle bundle;
    bundle.asset.id = prefix + ".asset";
    bundle.asset.name = std::move(name);
    bundle.asset.description = "Unrelated user-authored semantic content.";
    bundle.asset.tags = {"test", "user"};
    bundle.asset.style = "user";
    bundle.asset.energy = 0.4F;
    bundle.asset.program_id = prefix + ".program";
    bundle.asset.launch_profile_id = prefix + ".launch";
    bundle.asset.provenance_id = prefix + ".provenance";

    bundle.program.id = bundle.asset.program_id;
    bundle.program.length_ticks = 4 * emberlights::kMusicalTicksPerQuarter;
    bundle.program.targets.push_back({
        prefix + ".target", emberlights::AutoloopTargetKind::Master, {},
        {showcore::Property::Intensity}});
    bundle.program.lanes.push_back({
        prefix + ".lane", prefix + ".target", 0U});
    emberlights::AutoloopEventDefinition event;
    event.id = prefix + ".event";
    event.lane_id = prefix + ".lane";
    event.kind = emberlights::AutoloopEventKind::PropertyBlock;
    event.start_tick = 0;
    event.end_tick = bundle.program.length_ticks;
    event.property = showcore::Property::Intensity;
    event.value = showcore::PropertyValue::set(0.4F);
    bundle.program.events.push_back(std::move(event));

    bundle.launch_profile.id = bundle.asset.launch_profile_id;
    bundle.launch_profile.repeat = showcore::AutoloopRepeat::Infinite;
    bundle.provenance.id = bundle.asset.provenance_id;
    bundle.provenance.origin =
        emberlights::AutoloopProvenanceOrigin::Native;
    bundle.provenance.producer_id = "emberlights.test";
    bundle.provenance.producer_version = "1";
    bundle.provenance.source_object_key = bundle.asset.id;
    bundle.provenance.evidence_status = "synthetic";
    return bundle;
}

[[nodiscard]] emberlights::AutoloopSourceDocument source_from_bundle(
    const emberlights::AutoloopAssetBundle& bundle) {
    emberlights::AutoloopSourceDocument source;
    source.assets.push_back(bundle.asset);
    source.programs.push_back(bundle.program);
    source.launch_profiles.push_back(bundle.launch_profile);
    source.provenance.push_back(bundle.provenance);
    emberlights::normalize_autoloop_source(source);
    return source;
}

[[nodiscard]] emberlights::AutoloopAutoscriptRequest section_request(
    std::uint64_t seed = 0x123456789ABCDEF0ULL) {
    emberlights::AutoloopAutoscriptRequest request;
    request.track_duration_ticks = 24 * emberlights::kMusicalTicksPerQuarter;
    request.loop_length_ticks = 4 * emberlights::kMusicalTicksPerQuarter;
    request.grid_ticks = emberlights::kMusicalTicksPerQuarter;
    request.style = emberlights::AutoloopAutoscriptStyle::ColorMotion;
    request.complexity = emberlights::AutoloopAutoscriptComplexity::Low;
    request.musical_sections = {
        {0, 8 * emberlights::kMusicalTicksPerQuarter,
         emberlights::AutoloopAutoscriptSectionKind::Intro, 300U},
        {8 * emberlights::kMusicalTicksPerQuarter,
         16 * emberlights::kMusicalTicksPerQuarter,
         emberlights::AutoloopAutoscriptSectionKind::Build, 650U},
        {16 * emberlights::kMusicalTicksPerQuarter,
         24 * emberlights::kMusicalTicksPerQuarter,
         emberlights::AutoloopAutoscriptSectionKind::Drop, 900U}};
    request.eligible_role_selectors = {"role.washes", "role.movers"};
    request.seed = seed;
    request.first_placement = {2U, 0U};
    request.content_budget.maximum_generated_assets = 8U;
    request.content_budget.maximum_generated_events = 512U;
    request.content_budget.maximum_candidate_canonical_bytes =
        2U * 1024U * 1024U;
    request.operation_budget.maximum_operations = 10000U;
    return request;
}

[[nodiscard]] emberlights::AutoloopAutoscriptRequest energy_request() {
    emberlights::AutoloopAutoscriptRequest request;
    request.track_duration_ticks = 16 * emberlights::kMusicalTicksPerQuarter;
    request.loop_length_ticks = 8 * emberlights::kMusicalTicksPerQuarter;
    request.grid_ticks = emberlights::kMusicalTicksPerQuarter;
    request.style = emberlights::AutoloopAutoscriptStyle::Balanced;
    request.complexity = emberlights::AutoloopAutoscriptComplexity::Medium;
    request.energy_bands = {
        {0, 8 * emberlights::kMusicalTicksPerQuarter, 200U, 400U},
        {8 * emberlights::kMusicalTicksPerQuarter,
         16 * emberlights::kMusicalTicksPerQuarter, 700U, 950U}};
    request.seed = 7U;
    request.first_placement = {4U, 4U};
    request.content_budget.maximum_generated_assets = 4U;
    request.content_budget.maximum_generated_events = 512U;
    request.content_budget.maximum_candidate_canonical_bytes =
        2U * 1024U * 1024U;
    request.operation_budget.maximum_operations = 10000U;
    return request;
}

void test_exact_repeatability_semantic_content_and_compiled_digest() {
    emberlights::AutoloopAuthoringService service;
    const auto initial = service.snapshot();
    const auto request = section_request();
    const auto first = emberlights::propose_autoloop_autoscript(
        initial, request);
    const auto repeated = emberlights::propose_autoloop_autoscript(
        initial, request);

    CHECK(first.ready());
    CHECK(repeated.ready());
    CHECK(first.request_digest().size() == 64U);
    CHECK(first.proposal_digest().size() == 64U);
    CHECK(first.preview_source_digest().size() == 64U);
    CHECK(first.request_digest() == repeated.request_digest());
    CHECK(first.proposal_digest() == repeated.proposal_digest());
    CHECK(first.preview_source_digest() == repeated.preview_source_digest());
    CHECK(first.operations_used() == repeated.operations_used());
    CHECK(emberlights::serialize_autoloop_source(first.preview_source()) ==
          emberlights::serialize_autoloop_source(repeated.preview_source()));
    CHECK(service.snapshot().generation == initial.generation);
    CHECK(service.snapshot().source_digest == initial.source_digest);

    CHECK(first.generated_asset_ids().size() == 3U);
    CHECK(first.generated_placement_ids().size() == 3U);
    CHECK(first.generated_addresses().size() == 3U);
    CHECK((first.generated_addresses()[0] ==
           showcore::AutoloopAddress{2U, 0U}));
    CHECK((first.generated_addresses()[2] ==
           showcore::AutoloopAddress{2U, 2U}));
    CHECK(first.generated_event_count() == 96U);

    for (const auto& program : first.preview_source().programs) {
        CHECK(program.targets.size() == 2U);
        CHECK(program.lanes.size() == 2U);
        CHECK(program.events.size() == 32U);
        for (const auto& target : program.targets) {
            CHECK(target.kind ==
                  emberlights::AutoloopTargetKind::RoleSelector);
            CHECK(target.stable_ref == "role.movers" ||
                  target.stable_ref == "role.washes");
            CHECK(target.required_properties.size() == 4U);
        }
        for (const auto& event : program.events) {
            CHECK(event.kind ==
                  emberlights::AutoloopEventKind::PropertyBlock);
            CHECK(event.reference_id.empty());
            CHECK(event.transition_reference_id.empty());
            CHECK(event.property == showcore::Property::Intensity ||
                  event.property == showcore::Property::Red ||
                  event.property == showcore::Property::Green ||
                  event.property == showcore::Property::Blue);
            CHECK(event.start_tick % request.grid_ticks == 0);
            CHECK(event.end_tick % request.grid_ticks == 0);
        }
    }
    for (const auto& provenance : first.preview_source().provenance) {
        CHECK(provenance.origin ==
              emberlights::AutoloopProvenanceOrigin::Generated);
        CHECK(provenance.producer_id ==
              emberlights::kAutoloopAutoscriptGeneratorId);
        CHECK(provenance.producer_version == "1");
        CHECK(provenance.seed == *request.seed);
        CHECK(provenance.source_bundle_id == first.request_digest());
        CHECK(!provenance.source_artifact_id.empty());
        CHECK(provenance.evidence_status == "generated.deterministic");
    }

    const std::array<std::uint16_t, 1U> mover_ids{{0U}};
    const std::array<std::uint16_t, 1U> wash_ids{{1U}};
    const std::array<showcore::AutoloopTargetBinding, 2U> targets{{
        {showcore::CompiledAutoloopTargetKind::RoleSelector,
         "role.movers", std::span<const std::uint16_t>(mover_ids),
         showcore::all_autoloop_property_mask()},
        {showcore::CompiledAutoloopTargetKind::RoleSelector,
         "role.washes", std::span<const std::uint16_t>(wash_ids),
         showcore::all_autoloop_property_mask()}
    }};
    const showcore::AutoloopCompileEnvironment environment{
        std::span<const showcore::AutoloopTargetBinding>(targets), {}};
    const auto compiled = showcore::compile_autoloop_programs(
        first.preview_source(), environment);
    const auto compiled_repeated = showcore::compile_autoloop_programs(
        repeated.preview_source(), environment);
    CHECK(compiled.ok());
    CHECK(compiled_repeated.ok());
    if (compiled.ok() && compiled_repeated.ok()) {
        CHECK(compiled.package->digest() ==
              compiled_repeated.package->digest());
    }

    auto reordered = request;
    std::reverse(
        reordered.musical_sections.begin(),
        reordered.musical_sections.end());
    std::reverse(
        reordered.eligible_role_selectors.begin(),
        reordered.eligible_role_selectors.end());
    const auto normalized = emberlights::propose_autoloop_autoscript(
        initial, std::move(reordered));
    CHECK(normalized.ready());
    CHECK(normalized.request_digest() == first.request_digest());
    CHECK(normalized.preview_source_digest() ==
          first.preview_source_digest());

    const auto other_seed = emberlights::propose_autoloop_autoscript(
        initial, section_request(0xFEDCBA9876543210ULL));
    CHECK(other_seed.ready());
    CHECK(other_seed.generated_asset_ids() == first.generated_asset_ids());
    CHECK(other_seed.generated_placement_ids() ==
          first.generated_placement_ids());
    CHECK(other_seed.request_digest() != first.request_digest());
    CHECK(other_seed.preview_source_digest() !=
          first.preview_source_digest());
}

void test_preview_commit_is_one_transaction_and_undo_is_exact() {
    auto user = make_bundle("user.keep", "Keep Me");
    auto source = source_from_bundle(user);
    source.placements.push_back({
        "user.keep.placement", 20U, 0U, user.asset.id, {}});
    emberlights::AutoloopAuthoringService service(std::move(source), 11U);
    const auto before = service.snapshot();
    const auto proposal = emberlights::propose_autoloop_autoscript(
        before, energy_request());
    CHECK(proposal.ready());
    CHECK(service.snapshot().generation == before.generation);
    CHECK(service.snapshot().source_digest == before.source_digest);
    CHECK(proposal.preview_source().assets.size() ==
          before.source.assets.size() + 2U);
    CHECK(proposal.preview_source().placements.size() ==
          before.source.placements.size() + 2U);
    const auto kept_asset = find_by_id(
        proposal.preview_source().assets, user.asset.id);
    const auto kept_placement = find_by_id(
        proposal.preview_source().placements, "user.keep.placement");
    CHECK(kept_asset != proposal.preview_source().assets.end());
    CHECK(kept_placement != proposal.preview_source().placements.end());
    if (kept_asset != proposal.preview_source().assets.end()) {
        CHECK(kept_asset->name == "Keep Me");
        CHECK(kept_asset->program_id == user.asset.program_id);
    }
    if (kept_placement != proposal.preview_source().placements.end()) {
        CHECK(kept_placement->bank == 20U);
        CHECK(kept_placement->slot == 0U);
        CHECK(kept_placement->asset_id == user.asset.id);
    }

    const auto committed =
        emberlights::apply_autoloop_autoscript_proposal(service, proposal);
    CHECK(committed.result ==
          emberlights::AutoloopAuthoringResult::Applied);
    CHECK(committed.generation == before.generation + 1U);
    const auto after = service.snapshot();
    CHECK(after.source_digest == proposal.preview_source_digest());
    CHECK(after.can_undo);
    CHECK(!after.can_redo);

    const auto replacement = emberlights::propose_autoloop_autoscript(
        after, [&]() {
            auto changed_seed = energy_request();
            changed_seed.seed = 99U;
            return changed_seed;
        }());
    CHECK(replacement.result() ==
          emberlights::AutoloopAutoscriptProposalResult::StableIdConflict);
    CHECK(service.snapshot().generation == after.generation);
    CHECK(service.snapshot().source_digest == after.source_digest);

    const auto undone = service.undo(after.generation);
    CHECK(undone.result == emberlights::AutoloopAuthoringResult::Applied);
    const auto restored = service.snapshot();
    CHECK(restored.source_digest == before.source_digest);
    CHECK(emberlights::serialize_autoloop_source(restored.source) ==
          emberlights::serialize_autoloop_source(before.source));

    const auto stale_after_undo =
        emberlights::apply_autoloop_autoscript_proposal(service, proposal);
    CHECK(stale_after_undo.result ==
          emberlights::AutoloopAuthoringResult::StaleGeneration);
    CHECK(service.snapshot().source_digest == before.source_digest);
}

void test_occupied_capacity_and_stable_id_conflicts_fail_closed() {
    auto request = section_request();
    auto user = make_bundle("occupied.user", "Occupied");
    auto source = source_from_bundle(user);
    source.placements.push_back({
        "occupied.user.placement", 2U, 1U, user.asset.id, {}});
    emberlights::AutoloopAuthoringService occupied_service(std::move(source));
    const auto occupied_before = occupied_service.snapshot();
    const auto occupied = emberlights::propose_autoloop_autoscript(
        occupied_before, request);
    CHECK(occupied.result() ==
          emberlights::AutoloopAutoscriptProposalResult::OccupiedPlacement);
    CHECK(!occupied.ready());
    CHECK(occupied.proposal_digest().size() == 64U);
    CHECK(occupied_service.snapshot().source_digest ==
          occupied_before.source_digest);
    CHECK(emberlights::apply_autoloop_autoscript_proposal(
              occupied_service, occupied).result ==
          emberlights::AutoloopAuthoringResult::InvalidCandidate);
    CHECK(occupied_service.snapshot().source_digest ==
          occupied_before.source_digest);

    emberlights::AutoloopAuthoringService empty;
    const auto empty_before = empty.snapshot();
    auto beyond = request;
    beyond.first_placement = {63U, 31U};
    const auto capacity = emberlights::propose_autoloop_autoscript(
        empty_before, std::move(beyond));
    CHECK(capacity.result() ==
          emberlights::AutoloopAutoscriptProposalResult::CapacityExceeded);
    CHECK(empty.snapshot().source_digest == empty_before.source_digest);

    const auto baseline = emberlights::propose_autoloop_autoscript(
        empty_before, request);
    CHECK(baseline.ready());
    if (!baseline.ready()) {
        return;
    }
    auto collision_bundle = make_bundle("collision.user", "Collision");
    collision_bundle.asset.id = baseline.generated_asset_ids().front();
    auto collision_source = source_from_bundle(collision_bundle);
    emberlights::AutoloopAuthoringService collision_service(
        std::move(collision_source));
    const auto collision_before = collision_service.snapshot();
    const auto collision = emberlights::propose_autoloop_autoscript(
        collision_before, request);
    CHECK(collision.result() ==
          emberlights::AutoloopAutoscriptProposalResult::StableIdConflict);
    CHECK(collision_service.snapshot().source_digest ==
          collision_before.source_digest);
}

void test_cancellation_invalid_inputs_and_hard_budgets_fail_closed() {
    emberlights::AutoloopAuthoringService service;
    const auto before = service.snapshot();

    auto explicit_zero = energy_request();
    explicit_zero.seed = 0U;
    const auto zero_seed = emberlights::propose_autoloop_autoscript(
        before, std::move(explicit_zero));
    CHECK(zero_seed.ready());
    CHECK(std::any_of(
        zero_seed.diagnostics().begin(), zero_seed.diagnostics().end(),
        [](const auto& diagnostic) {
            return diagnostic.code == "autoscript.target.master";
        }));

    auto missing_seed = section_request();
    missing_seed.seed.reset();
    const auto no_seed = emberlights::propose_autoloop_autoscript(
        before, std::move(missing_seed));
    CHECK(no_seed.result() ==
          emberlights::AutoloopAutoscriptProposalResult::InvalidRequest);

    auto ambiguous = section_request();
    ambiguous.energy_bands.push_back({
        0, ambiguous.track_duration_ticks, 0U, 1000U});
    const auto both = emberlights::propose_autoloop_autoscript(
        before, std::move(ambiguous));
    CHECK(both.result() ==
          emberlights::AutoloopAutoscriptProposalResult::InvalidRequest);

    emberlights::AutoloopAutoscriptCancellationToken cancellation;
    cancellation.request_cancellation();
    const auto cancelled = emberlights::propose_autoloop_autoscript(
        before, section_request(), &cancellation);
    CHECK(cancelled.result() ==
          emberlights::AutoloopAutoscriptProposalResult::Cancelled);
    CHECK(cancelled.proposal_digest().size() == 64U);

    auto operations = section_request();
    operations.operation_budget.maximum_operations = 1U;
    const auto operation_limited =
        emberlights::propose_autoloop_autoscript(
            before, std::move(operations));
    CHECK(operation_limited.result() ==
          emberlights::AutoloopAutoscriptProposalResult::
              OperationBudgetExceeded);
    CHECK(operation_limited.operations_used() == 1U);

    const auto complete = emberlights::propose_autoloop_autoscript(
        before, section_request());
    CHECK(complete.ready());
    CHECK(complete.operations_used() > 1U);
    auto publication_budget = section_request();
    publication_budget.operation_budget.maximum_operations =
        complete.operations_used() - 1U;
    const auto publication_limited =
        emberlights::propose_autoloop_autoscript(
            before, std::move(publication_budget));
    CHECK(publication_limited.result() ==
          emberlights::AutoloopAutoscriptProposalResult::
              OperationBudgetExceeded);
    CHECK(publication_limited.operations_used() ==
          complete.operations_used() - 1U);

    auto content = section_request();
    content.content_budget.maximum_generated_assets = 2U;
    const auto content_limited = emberlights::propose_autoloop_autoscript(
        before, std::move(content));
    CHECK(content_limited.result() ==
          emberlights::AutoloopAutoscriptProposalResult::
              ContentBudgetExceeded);

    auto bytes = section_request();
    bytes.content_budget.maximum_candidate_canonical_bytes = 64U;
    const auto byte_limited = emberlights::propose_autoloop_autoscript(
        before, std::move(bytes));
    CHECK(byte_limited.result() ==
          emberlights::AutoloopAutoscriptProposalResult::
              ContentBudgetExceeded);

    auto invalid_hard_cap = section_request();
    invalid_hard_cap.operation_budget.maximum_operations =
        emberlights::kMaximumAutoloopAutoscriptOperations + 1U;
    const auto invalid_budget = emberlights::propose_autoloop_autoscript(
        before, std::move(invalid_hard_cap));
    CHECK(invalid_budget.result() ==
          emberlights::AutoloopAutoscriptProposalResult::InvalidRequest);

    auto overlapping = section_request();
    overlapping.musical_sections[1].start_tick =
        overlapping.musical_sections[0].end_tick - 1;
    const auto invalid_timeline = emberlights::propose_autoloop_autoscript(
        before, std::move(overlapping));
    CHECK(invalid_timeline.result() ==
          emberlights::AutoloopAutoscriptProposalResult::InvalidRequest);

    auto invalid_base = before;
    invalid_base.source_digest.assign(64U, '0');
    const auto rejected_base = emberlights::propose_autoloop_autoscript(
        invalid_base, energy_request());
    CHECK(rejected_base.result() ==
          emberlights::AutoloopAutoscriptProposalResult::InvalidBaseSource);

    CHECK(service.snapshot().generation == before.generation);
    CHECK(service.snapshot().source_digest == before.source_digest);
}

void test_generation_stale_commit_preserves_newer_unrelated_source() {
    auto user = make_bundle("stale.user", "Before");
    emberlights::AutoloopAuthoringService service(source_from_bundle(user));
    const auto base = service.snapshot();
    const auto proposal = emberlights::propose_autoloop_autoscript(
        base, energy_request());
    CHECK(proposal.ready());

    const auto renamed = service.rename_asset(
        service.generation(), user.asset.id, "Newer Unrelated Name");
    CHECK(renamed.result == emberlights::AutoloopAuthoringResult::Applied);
    const auto newer = service.snapshot();
    const auto stale = emberlights::apply_autoloop_autoscript_proposal(
        service, proposal);
    CHECK(stale.result ==
          emberlights::AutoloopAuthoringResult::StaleGeneration);
    CHECK(service.snapshot().generation == newer.generation);
    CHECK(service.snapshot().source_digest == newer.source_digest);
    const auto final_snapshot = service.snapshot();
    const auto kept = find_by_id(final_snapshot.source.assets, user.asset.id);
    CHECK(kept != final_snapshot.source.assets.end());
    if (kept != final_snapshot.source.assets.end()) {
        CHECK(kept->name == "Newer Unrelated Name");
    }
}

}  // namespace

int main() {
    static_assert(std::is_copy_constructible_v<
                  emberlights::AutoloopAutoscriptProposal>);
    static_assert(std::is_same_v<
                  decltype(std::declval<const
                      emberlights::AutoloopAutoscriptProposal&>()
                               .preview_source()),
                  const emberlights::AutoloopSourceDocument&>);

    test_exact_repeatability_semantic_content_and_compiled_digest();
    test_preview_commit_is_one_transaction_and_undo_is_exact();
    test_occupied_capacity_and_stable_id_conflicts_fail_closed();
    test_cancellation_invalid_inputs_and_hard_budgets_fail_closed();
    test_generation_stale_commit_preserves_newer_unrelated_source();

    if (failures != 0) {
        std::cerr << failures << " AutoScript test(s) failed\n";
        return 1;
    }
    std::cout << "All deterministic Autoloop AutoScript tests passed\n";
    return 0;
}
