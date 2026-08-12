#include "emberlights/autoloop_authoring.hpp"
#include "emberlights/autoloop_content_pack.hpp"

#include "showcore/autoloop_program.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
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
    std::string name,
    float intensity = 0.5F) {
    emberlights::AutoloopAssetBundle bundle;
    bundle.asset.id = prefix + ".asset";
    bundle.asset.name = std::move(name);
    bundle.asset.description = "Original authoring test content.";
    bundle.asset.tags = {"original", "test"};
    bundle.asset.style = "test";
    bundle.asset.energy = intensity;
    bundle.asset.program_id = prefix + ".program";
    bundle.asset.launch_profile_id = prefix + ".launch";
    bundle.asset.provenance_id = prefix + ".provenance";
    bundle.asset.revision = 1U;

    bundle.program.id = bundle.asset.program_id;
    bundle.program.length_ticks = 4 * emberlights::kMusicalTicksPerQuarter;
    bundle.program.targets.push_back({
        prefix + ".target.master",
        emberlights::AutoloopTargetKind::Master,
        {},
        {showcore::Property::Intensity}});
    bundle.program.lanes.push_back({
        prefix + ".lane.master",
        prefix + ".target.master",
        0U});
    emberlights::AutoloopEventDefinition event;
    event.id = prefix + ".event.intensity";
    event.lane_id = prefix + ".lane.master";
    event.kind = emberlights::AutoloopEventKind::PropertyBlock;
    event.start_tick = 0;
    event.end_tick = bundle.program.length_ticks;
    event.property = showcore::Property::Intensity;
    event.value = showcore::PropertyValue::set(intensity);
    bundle.program.events.push_back(std::move(event));

    bundle.launch_profile.id = bundle.asset.launch_profile_id;
    bundle.launch_profile.repeat = showcore::AutoloopRepeat::Infinite;
    bundle.launch_profile.launch =
        emberlights::AutoloopLaunchQuantization::Immediate;
    bundle.launch_profile.phase_origin =
        emberlights::AutoloopPhaseOrigin::Launch;
    bundle.launch_profile.mode = emberlights::AutoloopPlaybackMode::Overlay;
    bundle.provenance.id = bundle.asset.provenance_id;
    bundle.provenance.origin = emberlights::AutoloopProvenanceOrigin::Native;
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

[[nodiscard]] const emberlights::AutoloopPlacementDefinition* placement(
    const emberlights::AutoloopSourceDocument& source,
    std::string_view id) {
    const auto found = find_by_id(source.placements, id);
    return found == source.placements.end() ? nullptr : &*found;
}

[[nodiscard]] const emberlights::AutoloopAssetDefinition* asset(
    const emberlights::AutoloopSourceDocument& source,
    std::string_view id) {
    const auto found = find_by_id(source.assets, id);
    return found == source.assets.end() ? nullptr : &*found;
}

void test_generation_validation_and_one_step_history() {
    emberlights::AutoloopAuthoringService service;
    const auto initial = service.snapshot();
    CHECK(initial.generation == 1U);
    CHECK(initial.source_digest.size() == 64U);
    CHECK(emberlights::validate_autoloop_source(initial.source).ok());
    CHECK(!initial.can_undo);
    CHECK(!initial.can_redo);

    auto created = service.create_asset(
        initial.generation, make_bundle("authoring.alpha", "Alpha"));
    CHECK(created.result == emberlights::AutoloopAuthoringResult::Applied);
    CHECK(created.generation == 2U);
    CHECK(created.stable_id == "authoring.alpha.asset");

    const auto after_create = service.snapshot();
    CHECK(after_create.source.assets.size() == 1U);
    const auto serialized =
        emberlights::serialize_autoloop_source(after_create.source);
    emberlights::AutoloopSourceDocument parsed;
    CHECK(emberlights::parse_autoloop_source(serialized, parsed));
    CHECK(emberlights::serialize_autoloop_source(parsed) == serialized);

    const auto stale_digest = after_create.source_digest;
    const auto stale = service.rename_asset(
        initial.generation, "authoring.alpha.asset", "Stale");
    CHECK(stale.result ==
          emberlights::AutoloopAuthoringResult::StaleGeneration);
    CHECK(service.snapshot().source_digest == stale_digest);

    const auto renamed = service.rename_asset(
        service.generation(), "authoring.alpha.asset", "Alpha Renamed");
    CHECK(renamed.result == emberlights::AutoloopAuthoringResult::Applied);
    CHECK(renamed.generation == 3U);
    CHECK(asset(service.snapshot().source, "authoring.alpha.asset")->name ==
          "Alpha Renamed");

    const auto no_change = service.rename_asset(
        service.generation(), "authoring.alpha.asset", "Alpha Renamed");
    CHECK(no_change.result ==
          emberlights::AutoloopAuthoringResult::NoChange);
    CHECK(no_change.generation == 3U);
    CHECK(service.snapshot().can_undo);

    const auto undone = service.undo(service.generation());
    CHECK(undone.result == emberlights::AutoloopAuthoringResult::Applied);
    CHECK(undone.generation == 4U);
    CHECK(asset(service.snapshot().source, "authoring.alpha.asset")->name ==
          "Alpha");
    CHECK(!service.snapshot().can_undo);
    CHECK(service.snapshot().can_redo);
    CHECK(service.undo(service.generation()).result ==
          emberlights::AutoloopAuthoringResult::UndoUnavailable);

    const auto redone = service.redo(service.generation());
    CHECK(redone.result == emberlights::AutoloopAuthoringResult::Applied);
    CHECK(redone.generation == 5U);
    CHECK(asset(service.snapshot().source, "authoring.alpha.asset")->name ==
          "Alpha Renamed");
    CHECK(service.snapshot().can_undo);
    CHECK(!service.snapshot().can_redo);

    auto invalid = service.snapshot().source;
    invalid.assets.front().program_id = "missing.program";
    const auto before_invalid = service.snapshot().source_digest;
    const auto rejected = service.apply_candidate(
        service.generation(), std::move(invalid));
    CHECK(rejected.result ==
          emberlights::AutoloopAuthoringResult::ValidationFailed);
    CHECK(!rejected.validation.ok());
    CHECK(service.snapshot().source_digest == before_invalid);
}

void test_content_identity_duplicate_and_exact_delete_dependencies() {
    emberlights::AutoloopAuthoringService service;
    CHECK(service.create_asset(
        service.generation(), make_bundle("identity.alpha", "Alpha")));
    const auto duplicated = service.duplicate_asset(
        service.generation(),
        "identity.alpha.asset",
        "identity.beta.asset",
        "identity.beta.program",
        "identity.beta.launch",
        "identity.beta.provenance",
        "Beta Copy");
    CHECK(duplicated.result ==
          emberlights::AutoloopAuthoringResult::Applied);
    auto snapshot = service.snapshot();
    CHECK(snapshot.source.assets.size() == 2U);
    CHECK(snapshot.source.programs.size() == 2U);
    CHECK(asset(snapshot.source, "identity.beta.asset")->program_id ==
          "identity.beta.program");

    CHECK(service.assign_placement(
        service.generation(), "identity.place.1", {0U, 0U},
        "identity.alpha.asset"));
    CHECK(service.assign_placement(
        service.generation(), "identity.place.2", {1U, 3U},
        "identity.alpha.asset"));
    const auto dependencies =
        service.inspect_dependencies("identity.alpha.asset");
    CHECK(dependencies.placement_ids ==
          std::vector<std::string>({"identity.place.1", "identity.place.2"}));
    CHECK(dependencies.assets_sharing_program.empty());
    CHECK(dependencies.orphan_records.size() == 3U);

    const auto before_delete = service.snapshot().source_digest;
    const auto blocked = service.delete_asset(
        service.generation(), "identity.alpha.asset");
    CHECK(blocked.result ==
          emberlights::AutoloopAuthoringResult::DependencyConflict);
    CHECK(blocked.dependencies.placement_ids == dependencies.placement_ids);
    CHECK(service.snapshot().source_digest == before_delete);

    const auto removed = service.delete_asset(
        service.generation(), "identity.alpha.asset", true);
    CHECK(removed.result == emberlights::AutoloopAuthoringResult::Applied);
    CHECK(removed.dependencies.placement_ids == dependencies.placement_ids);
    snapshot = service.snapshot();
    CHECK(snapshot.source.assets.size() == 1U);
    CHECK(snapshot.source.programs.size() == 1U);
    CHECK(snapshot.source.launch_profiles.size() == 1U);
    CHECK(snapshot.source.provenance.size() == 1U);
    CHECK(snapshot.source.placements.empty());
    CHECK(asset(snapshot.source, "identity.beta.asset") != nullptr);
}

void test_explicit_placement_operations_do_not_overwrite() {
    emberlights::AutoloopAuthoringService service;
    CHECK(service.create_asset(
        service.generation(), make_bundle("placement.alpha", "Alpha")));
    CHECK(service.create_asset(
        service.generation(), make_bundle("placement.beta", "Beta")));
    CHECK(service.assign_placement(
        service.generation(), "placement.a", {0U, 0U},
        "placement.alpha.asset"));
    CHECK(service.assign_placement(
        service.generation(), "placement.b", {0U, 1U},
        "placement.beta.asset"));
    // A second placement is not a content duplicate: it shares the same asset.
    CHECK(service.assign_placement(
        service.generation(), "placement.a.again", {0U, 2U},
        "placement.alpha.asset"));

    const auto before_occupied = service.snapshot().source_digest;
    const auto occupied = service.move_placement(
        service.generation(), "placement.a", {0U, 1U});
    CHECK(occupied.result ==
          emberlights::AutoloopAuthoringResult::OccupiedPlacement);
    CHECK(occupied.stable_id == "placement.b");
    CHECK(service.snapshot().source_digest == before_occupied);

    CHECK(service.swap_placements(
        service.generation(), "placement.a", "placement.b"));
    auto snapshot = service.snapshot();
    CHECK(placement(snapshot.source, "placement.a")->bank == 0U);
    CHECK(placement(snapshot.source, "placement.a")->slot == 1U);
    CHECK(placement(snapshot.source, "placement.b")->slot == 0U);

    const auto open = service.next_open({0U, 0U});
    CHECK(open.found);
    CHECK((open.address == showcore::AutoloopAddress{0U, 3U}));
    CHECK(service.move_placement(
        service.generation(), "placement.a.again", {1U, 0U}));
    CHECK(service.unassign_placement(
        service.generation(), "placement.b"));
    const auto assigned = service.assign_next_open(
        service.generation(), "placement.beta.next",
        "placement.beta.asset");
    CHECK(assigned.result == emberlights::AutoloopAuthoringResult::Applied);
    CHECK(assigned.address.has_value());
    CHECK((*assigned.address == showcore::AutoloopAddress{0U, 0U}));
}

void test_full_catalog_capacity_and_next_open_refusal() {
    const auto bundle = make_bundle("capacity.shared", "Shared");
    auto source = source_from_bundle(bundle);
    source.placements.reserve(showcore::kMaxAutoloops);
    for (std::size_t index = 0U; index < showcore::kMaxAutoloops; ++index) {
        source.placements.push_back({
            "capacity.placement." + std::to_string(index),
            static_cast<std::uint16_t>(
                index / showcore::kAutoloopsPerBank),
            static_cast<std::uint8_t>(
                index % showcore::kAutoloopsPerBank),
            bundle.asset.id,
            {}});
    }
    emberlights::AutoloopAuthoringService service(std::move(source));
    CHECK(service.snapshot().source.placements.size() ==
          showcore::kMaxAutoloops);
    CHECK(!service.next_open().found);
    const auto refused = service.assign_next_open(
        service.generation(), "capacity.extra", bundle.asset.id);
    CHECK(refused.result ==
          emberlights::AutoloopAuthoringResult::CapacityExceeded);
}

void test_original_pack_round_trip_and_canonical_compile() {
    const auto first =
        emberlights::make_emberlights_starter_autoloop_pack();
    const auto second =
        emberlights::make_emberlights_starter_autoloop_pack();
    CHECK(emberlights::valid_autoloop_content_pack(first));
    CHECK(first.id == emberlights::kEmberlightsStarterAutoloopPackId);
    CHECK(first.version ==
          emberlights::kEmberlightsStarterAutoloopPackVersion);
    CHECK(first.source.placements.size() ==
          emberlights::kEmberlightsStarterAutoloopPlacementCount);
    CHECK(first.source.assets.size() ==
          emberlights::kEmberlightsStarterAutoloopPlacementCount);
    CHECK(first.content_digest == second.content_digest);
    CHECK(emberlights::serialize_autoloop_source(first.source) ==
          emberlights::serialize_autoloop_source(second.source));
    for (const auto& value : first.source.placements) {
        CHECK(value.bank < 4U);
        CHECK(value.content_management_key.starts_with(
            emberlights::kEmberlightsStarterAutoloopManagementPrefix));
    }

    const auto serialized = emberlights::serialize_autoloop_source(first.source);
    emberlights::AutoloopSourceDocument parsed;
    CHECK(emberlights::parse_autoloop_source(serialized, parsed));
    CHECK(emberlights::serialize_autoloop_source(parsed) == serialized);

    const std::array<std::uint16_t, 1U> fixture_ids{{0U}};
    const std::array<showcore::AutoloopTargetBinding, 1U> targets{{{
        showcore::CompiledAutoloopTargetKind::Master,
        {},
        std::span<const std::uint16_t>(fixture_ids),
        showcore::all_autoloop_property_mask()}}};
    const showcore::AutoloopCompileEnvironment environment{
        std::span<const showcore::AutoloopTargetBinding>(targets), {}};
    const auto compiled = showcore::compile_autoloop_programs(
        first.source, environment);
    CHECK(compiled.ok());
    if (compiled.ok()) {
        CHECK(compiled.package->programs().size() ==
              emberlights::kEmberlightsStarterAutoloopPlacementCount);
        CHECK(compiled.package->placement({0U, 0U}) != nullptr);
        CHECK(compiled.package->placement({3U, 31U}) != nullptr);
        CHECK(compiled.package->digest().size() == 64U);
        const auto repeated = showcore::compile_autoloop_programs(
            second.source, environment);
        CHECK(repeated.ok());
        if (repeated.ok()) {
            CHECK(repeated.package->digest() == compiled.package->digest());
        }
    }

    showcore::AutoloopCompileLimits limits;
    limits.maximum_programs =
        emberlights::kEmberlightsStarterAutoloopPlacementCount - 1U;
    const auto over_limit = showcore::compile_autoloop_programs(
        first.source, environment, limits);
    CHECK(!over_limit.ok());
    CHECK(!over_limit.diagnostics.empty());
    if (!over_limit.diagnostics.empty()) {
        CHECK(over_limit.diagnostics.front().error ==
              showcore::AutoloopCompileError::CapacityExceeded);
        CHECK(over_limit.diagnostics.front().arena ==
              showcore::AutoloopArenaKind::Programs);
    }
}

void test_pack_populate_reset_idempotence_and_user_preservation() {
    const auto pack =
        emberlights::make_emberlights_starter_autoloop_pack();
    emberlights::AutoloopAuthoringService service;
    CHECK(service.create_asset(
        service.generation(), make_bundle("user.original", "User Original")));
    CHECK(service.assign_placement(
        service.generation(), "user.placement", {0U, 0U},
        "user.original.asset"));

    const auto populate = emberlights::plan_autoloop_pack_populate(
        service.snapshot(), pack);
    CHECK(populate.result ==
          emberlights::AutoloopContentPackPlanResult::Ready);
    CHECK(populate.added_asset_ids.size() == 127U);
    CHECK(populate.added_placement_ids.size() == 127U);
    CHECK(populate.skipped_occupied_placement_ids ==
          std::vector<std::string>({"user.placement"}));
    CHECK(emberlights::apply_autoloop_content_pack_plan(service, populate));
    auto populated = service.snapshot();
    CHECK(populated.source.assets.size() == 128U);
    CHECK(populated.source.placements.size() == 128U);
    CHECK(asset(populated.source, "user.original.asset") != nullptr);
    CHECK(placement(populated.source, "user.placement") != nullptr);

    const auto idempotent = emberlights::plan_autoloop_pack_populate(
        populated, pack);
    CHECK(idempotent.result ==
          emberlights::AutoloopContentPackPlanResult::NoChange);
    CHECK(idempotent.added_asset_ids.empty());
    CHECK(idempotent.added_placement_ids.empty());
    const auto unchanged = emberlights::apply_autoloop_content_pack_plan(
        service, idempotent);
    CHECK(unchanged.result ==
          emberlights::AutoloopAuthoringResult::NoChange);
    CHECK(service.snapshot().generation == populated.generation);

    const auto managed = std::find_if(
        populated.source.placements.begin(), populated.source.placements.end(),
        [](const auto& value) {
            return !value.content_management_key.empty();
        });
    CHECK(managed != populated.source.placements.end());
    if (managed == populated.source.placements.end()) {
        return;
    }
    const auto managed_asset_id = managed->asset_id;
    const auto default_asset = asset(pack.source, managed_asset_id);
    CHECK(default_asset != nullptr);
    CHECK(service.rename_asset(
        service.generation(), managed_asset_id, "User Customized Pack Name"));
    CHECK(asset(service.snapshot().source, managed_asset_id)->name ==
          "User Customized Pack Name");

    const auto reset = emberlights::plan_autoloop_pack_reset(
        service.snapshot(), pack);
    CHECK(reset.result == emberlights::AutoloopContentPackPlanResult::Ready);
    CHECK(reset.reset_asset_ids ==
          std::vector<std::string>({managed_asset_id}));
    CHECK(reset.affected_placement_ids ==
          std::vector<std::string>({managed->id}));
    CHECK(reset.conflicts.empty());
    CHECK(emberlights::apply_autoloop_content_pack_plan(service, reset));
    const auto reset_snapshot = service.snapshot();
    CHECK(asset(reset_snapshot.source, managed_asset_id)->name ==
          default_asset->name);
    CHECK(asset(reset_snapshot.source, "user.original.asset")->name ==
          "User Original");
    CHECK(placement(reset_snapshot.source, "user.placement")->bank == 0U);
    CHECK(placement(reset_snapshot.source, "user.placement")->slot == 0U);
    CHECK(reset_snapshot.source.assets.size() == 128U);
    CHECK(reset_snapshot.source.placements.size() == 128U);

    CHECK(service.undo(service.generation()));
    CHECK(asset(service.snapshot().source, managed_asset_id)->name ==
          "User Customized Pack Name");
    CHECK(asset(service.snapshot().source, "user.original.asset") != nullptr);

    const auto stale_plan = emberlights::plan_autoloop_pack_reset(
        service.snapshot(), pack);
    CHECK(service.rename_asset(
        service.generation(), "user.original.asset", "User Renamed"));
    const auto stale_apply =
        emberlights::apply_autoloop_content_pack_plan(service, stale_plan);
    CHECK(stale_apply.result ==
          emberlights::AutoloopAuthoringResult::StaleGeneration);
    CHECK(asset(service.snapshot().source, "user.original.asset")->name ==
          "User Renamed");
}

void test_pack_identity_conflicts_fail_closed() {
    const auto pack =
        emberlights::make_emberlights_starter_autoloop_pack();
    emberlights::AutoloopAuthoringService collision_service;
    CHECK(collision_service.create_asset(
        collision_service.generation(),
        make_bundle("emberlights.starter.v1.000", "Unrelated Collision")));
    const auto collision_before = collision_service.snapshot();
    const auto collision = emberlights::plan_autoloop_pack_populate(
        collision_before, pack);
    CHECK(collision.result ==
          emberlights::AutoloopContentPackPlanResult::Conflict);
    CHECK(!collision.conflicts.empty());
    CHECK(emberlights::serialize_autoloop_source(collision.candidate) ==
          emberlights::serialize_autoloop_source(collision_before.source));
    const auto rejected = emberlights::apply_autoloop_content_pack_plan(
        collision_service, collision);
    CHECK(rejected.result ==
          emberlights::AutoloopAuthoringResult::InvalidCandidate);
    CHECK(collision_service.snapshot().source_digest ==
          collision_before.source_digest);

    emberlights::AutoloopAuthoringService unknown_key_service;
    CHECK(unknown_key_service.create_asset(
        unknown_key_service.generation(),
        make_bundle("user.unknown-key", "Unknown Key")));
    CHECK(unknown_key_service.assign_placement(
        unknown_key_service.generation(), "user.unknown-key.placement",
        {4U, 0U}, "user.unknown-key.asset",
        std::string(emberlights::kEmberlightsStarterAutoloopManagementPrefix) +
            "/unrecognized"));
    const auto unknown_before = unknown_key_service.snapshot();
    const auto unknown = emberlights::plan_autoloop_pack_reset(
        unknown_before, pack);
    CHECK(unknown.result ==
          emberlights::AutoloopContentPackPlanResult::Conflict);
    CHECK(unknown.conflicts == std::vector<std::string>({
        "unrecognized-management-key:" +
        std::string(emberlights::kEmberlightsStarterAutoloopManagementPrefix) +
        "/unrecognized"}));
    CHECK(emberlights::serialize_autoloop_source(unknown.candidate) ==
          emberlights::serialize_autoloop_source(unknown_before.source));
}

}  // namespace

int main() {
    test_generation_validation_and_one_step_history();
    test_content_identity_duplicate_and_exact_delete_dependencies();
    test_explicit_placement_operations_do_not_overwrite();
    test_full_catalog_capacity_and_next_open_refusal();
    test_original_pack_round_trip_and_canonical_compile();
    test_pack_populate_reset_idempotence_and_user_preservation();
    test_pack_identity_conflicts_fail_closed();

    if (failures != 0) {
        std::cerr << failures << " Autoloop V2 Studio test(s) failed\n";
        return 1;
    }
    std::cout << "Autoloop V2 Studio tests passed\n";
    return 0;
}
