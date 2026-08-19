#include "emberlights/autoloop_content_pack.hpp"

#include "emberlights/studio_color_types.hpp"

#include <algorithm>
#include <array>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace emberlights {
namespace {

struct StarterBankDefinition {
    std::string_view name;
    std::string_view style;
    float energy;
    std::uint16_t bars;
};

constexpr std::array<StarterBankDefinition, 4U> kStarterBanks{{
    {"Hearth Glow", "ambient", 0.32F, 8U},
    {"Prism Current", "flowing", 0.52F, 16U},
    {"Cinder Cadence", "rhythmic", 0.72F, 16U},
    {"Aurora Surge", "high-energy", 0.88F, 32U},
}};

constexpr std::array<StudioColor, 8U> kOriginalStarterColors{{
    {{0.96F, 0.16F, 0.04F}, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F},
    {{0.98F, 0.52F, 0.05F}, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F},
    {{0.84F, 0.08F, 0.38F}, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F},
    {{0.45F, 0.10F, 0.92F}, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F},
    {{0.08F, 0.32F, 0.96F}, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F},
    {{0.04F, 0.78F, 0.88F}, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F},
    {{0.08F, 0.88F, 0.38F}, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F},
    {{0.78F, 0.92F, 0.12F}, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F},
}};

[[nodiscard]] std::string padded_index(std::size_t index) {
    std::ostringstream output;
    output << std::setfill('0') << std::setw(3) << index;
    return output.str();
}

[[nodiscard]] std::string display_number(std::size_t index) {
    std::ostringstream output;
    output << std::setfill('0') << std::setw(2) << index + 1U;
    return output.str();
}

[[nodiscard]] std::string property_suffix(showcore::Property property) {
    switch (property) {
    case showcore::Property::Intensity: return "intensity";
    case showcore::Property::Red: return "red";
    case showcore::Property::Green: return "green";
    case showcore::Property::Blue: return "blue";
    default: return "unsupported";
    }
}

[[nodiscard]] float color_value(
    const StudioColor& color,
    showcore::Property property,
    float intensity) noexcept {
    switch (property) {
    case showcore::Property::Intensity: return intensity;
    case showcore::Property::Red: return color.rgb.red;
    case showcore::Property::Green: return color.rgb.green;
    case showcore::Property::Blue: return color.rgb.blue;
    default: return 0.0F;
    }
}

template <typename Collection>
[[nodiscard]] auto find_by_id(Collection& collection, std::string_view id) {
    return std::find_if(
        collection.begin(), collection.end(),
        [id](const auto& value) { return value.id == id; });
}

template <typename Collection, typename Value>
void upsert_by_id(Collection& collection, const Value& value) {
    const auto found = find_by_id(collection, value.id);
    if (found == collection.end()) {
        collection.push_back(value);
    } else {
        *found = value;
    }
}

struct PackBundlePointers {
    const AutoloopAssetDefinition* asset{nullptr};
    const AutoloopProgramDefinition* program{nullptr};
    const AutoloopLaunchProfileDefinition* launch{nullptr};
    const AutoloopProvenanceDefinition* provenance{nullptr};

    [[nodiscard]] bool complete() const noexcept {
        return asset != nullptr && program != nullptr && launch != nullptr &&
            provenance != nullptr;
    }
};

[[nodiscard]] PackBundlePointers find_pack_bundle(
    const AutoloopContentPack& pack,
    std::string_view asset_id) {
    PackBundlePointers result;
    const auto asset = find_by_id(pack.source.assets, asset_id);
    if (asset == pack.source.assets.end()) {
        return result;
    }
    result.asset = &*asset;
    const auto program = find_by_id(pack.source.programs, asset->program_id);
    const auto launch = find_by_id(
        pack.source.launch_profiles, asset->launch_profile_id);
    const auto provenance = find_by_id(
        pack.source.provenance, asset->provenance_id);
    if (program != pack.source.programs.end()) result.program = &*program;
    if (launch != pack.source.launch_profiles.end()) result.launch = &*launch;
    if (provenance != pack.source.provenance.end()) {
        result.provenance = &*provenance;
    }
    return result;
}

[[nodiscard]] std::string serialize_bundle(
    const AutoloopAssetDefinition& asset,
    const AutoloopProgramDefinition& program,
    const AutoloopLaunchProfileDefinition& launch,
    const AutoloopProvenanceDefinition& provenance) {
    AutoloopSourceDocument source;
    source.assets.push_back(asset);
    source.programs.push_back(program);
    source.launch_profiles.push_back(launch);
    source.provenance.push_back(provenance);
    return serialize_autoloop_source(source);
}

[[nodiscard]] std::string serialize_bundle(
    const AutoloopSourceDocument& source,
    const AutoloopAssetDefinition& asset) {
    const auto program = find_by_id(source.programs, asset.program_id);
    const auto launch = find_by_id(
        source.launch_profiles, asset.launch_profile_id);
    const auto provenance = find_by_id(
        source.provenance, asset.provenance_id);
    if (program == source.programs.end() ||
        launch == source.launch_profiles.end() ||
        provenance == source.provenance.end()) {
        return {};
    }
    return serialize_bundle(asset, *program, *launch, *provenance);
}

[[nodiscard]] std::string serialize_bundle(
    const PackBundlePointers& bundle) {
    if (!bundle.complete()) {
        return {};
    }
    return serialize_bundle(
        *bundle.asset, *bundle.program, *bundle.launch, *bundle.provenance);
}

[[nodiscard]] bool recognized_pack_asset(
    const AutoloopSourceDocument& source,
    const AutoloopAssetDefinition& asset,
    const AutoloopContentPack& pack) {
    const auto provenance = find_by_id(source.provenance, asset.provenance_id);
    return provenance != source.provenance.end() &&
        provenance->origin == AutoloopProvenanceOrigin::ContentPack &&
        provenance->producer_id == pack.id &&
        provenance->producer_version == pack.semantic_version;
}

[[nodiscard]] bool has_record_collision(
    const AutoloopSourceDocument& source,
    const PackBundlePointers& bundle) {
    return find_by_id(source.programs, bundle.program->id) !=
            source.programs.end() ||
        find_by_id(source.launch_profiles, bundle.launch->id) !=
            source.launch_profiles.end() ||
        find_by_id(source.provenance, bundle.provenance->id) !=
            source.provenance.end();
}

void append_bundle(
    AutoloopSourceDocument& source,
    const PackBundlePointers& bundle) {
    source.assets.push_back(*bundle.asset);
    source.programs.push_back(*bundle.program);
    source.launch_profiles.push_back(*bundle.launch);
    source.provenance.push_back(*bundle.provenance);
}

void reset_bundle(
    AutoloopSourceDocument& source,
    const PackBundlePointers& bundle) {
    upsert_by_id(source.assets, *bundle.asset);
    upsert_by_id(source.programs, *bundle.program);
    upsert_by_id(source.launch_profiles, *bundle.launch);
    upsert_by_id(source.provenance, *bundle.provenance);
}

void sort_unique(std::vector<std::string>& values) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
}

void normalize_plan_lists(AutoloopContentPackPlan& plan) {
    sort_unique(plan.added_asset_ids);
    sort_unique(plan.added_placement_ids);
    sort_unique(plan.reset_asset_ids);
    sort_unique(plan.affected_placement_ids);
    sort_unique(plan.skipped_occupied_placement_ids);
    sort_unique(plan.conflicts);
}

[[nodiscard]] bool candidate_over_capacity(
    const AutoloopSourceDocument& source) noexcept {
    return source.assets.size() > kMaximumAutoloopAuthoringAssets ||
        source.placements.size() > showcore::kMaxAutoloops ||
        source.programs.size() > kMaximumAutoloopAuthoringPrograms ||
        source.launch_profiles.size() >
            kMaximumAutoloopAuthoringLaunchProfiles ||
        source.provenance.size() >
            kMaximumAutoloopAuthoringProvenanceRecords;
}

void finalize_plan(
    AutoloopContentPackPlan& plan,
    const AutoloopAuthoringSnapshot& snapshot) {
    normalize_plan_lists(plan);
    if (!plan.conflicts.empty()) {
        plan.result = AutoloopContentPackPlanResult::Conflict;
        plan.candidate = snapshot.source;
        plan.validation = validate_autoloop_source(plan.candidate);
        plan.message =
            "Content-pack identity conflicts must be resolved before commit.";
        return;
    }
    if (candidate_over_capacity(plan.candidate)) {
        plan.result = AutoloopContentPackPlanResult::CapacityExceeded;
        plan.candidate = snapshot.source;
        plan.validation = validate_autoloop_source(plan.candidate);
        plan.message =
            "The content-pack proposal exceeds bounded source capacity.";
        return;
    }
    normalize_autoloop_source(plan.candidate);
    plan.validation = validate_autoloop_source(plan.candidate);
    if (!plan.validation.ok()) {
        plan.result = AutoloopContentPackPlanResult::Conflict;
        plan.candidate = snapshot.source;
        plan.message =
            "The content-pack proposal failed canonical source validation.";
        return;
    }
    const auto before = serialize_autoloop_source(snapshot.source);
    const auto after = serialize_autoloop_source(plan.candidate);
    if (before == after) {
        plan.result = AutoloopContentPackPlanResult::NoChange;
        plan.message = "The content-pack proposal makes no source change.";
    } else {
        plan.result = AutoloopContentPackPlanResult::Ready;
        plan.message = "The content-pack proposal is ready for one commit.";
    }
}

}  // namespace

AutoloopContentPack make_emberlights_starter_autoloop_pack() {
    AutoloopContentPack pack;
    pack.id = std::string(kEmberlightsStarterAutoloopPackId);
    pack.version = kEmberlightsStarterAutoloopPackVersion;
    pack.semantic_version = "1.0.0";
    pack.management_key_prefix =
        std::string(kEmberlightsStarterAutoloopManagementPrefix);
    pack.license_notice =
        "Original EmberLights-authored content; distribution license pending.";
    pack.source.assets.reserve(kEmberlightsStarterAutoloopPlacementCount);
    pack.source.placements.reserve(kEmberlightsStarterAutoloopPlacementCount);
    pack.source.programs.reserve(kEmberlightsStarterAutoloopPlacementCount);
    pack.source.launch_profiles.reserve(
        kEmberlightsStarterAutoloopPlacementCount);
    pack.source.provenance.reserve(kEmberlightsStarterAutoloopPlacementCount);

    constexpr std::array<showcore::Property, 4U> properties{{
        showcore::Property::Intensity,
        showcore::Property::Red,
        showcore::Property::Green,
        showcore::Property::Blue,
    }};
    for (std::size_t index = 0U;
         index < kEmberlightsStarterAutoloopPlacementCount; ++index) {
        const auto bank_index = index / showcore::kAutoloopsPerBank;
        const auto slot = index % showcore::kAutoloopsPerBank;
        const auto& bank = kStarterBanks[bank_index];
        const auto suffix = padded_index(index);
        const auto base = "emberlights.starter.v1." + suffix;
        const auto asset_id = base + ".asset";
        const auto program_id = base + ".program";
        const auto launch_id = base + ".launch";
        const auto provenance_id = base + ".provenance";
        const auto target_id = program_id + ".target.master";
        const auto intensity_lane_id = program_id + ".lane.intensity";
        const auto color_lane_id = program_id + ".lane.color";

        AutoloopAssetDefinition asset;
        asset.id = asset_id;
        asset.name = std::string(bank.name) + " " + display_number(slot);
        asset.description =
            "Original EmberLights semantic starter pattern " + suffix + ".";
        asset.tags = {
            "original", "starter", std::string(bank.style)};
        asset.style = std::string(bank.style);
        asset.energy = bank.energy;
        asset.program_id = program_id;
        asset.launch_profile_id = launch_id;
        asset.provenance_id = provenance_id;
        asset.revision = 1U;
        pack.source.assets.push_back(std::move(asset));

        pack.source.placements.push_back({
            base + ".placement",
            static_cast<std::uint16_t>(bank_index),
            static_cast<std::uint8_t>(slot),
            asset_id,
            pack.management_key_prefix + "/placement/" + suffix});

        AutoloopProgramDefinition program;
        program.id = program_id;
        program.length_ticks = static_cast<MusicalTick>(bank.bars) * 4 *
            kMusicalTicksPerQuarter;
        program.time_signature_numerator = 4U;
        program.time_signature_denominator = 4U;
        program.targets.push_back({
            target_id,
            AutoloopTargetKind::Master,
            {},
            {showcore::Property::Intensity,
             showcore::Property::Red,
             showcore::Property::Green,
             showcore::Property::Blue}});
        program.lanes.push_back({intensity_lane_id, target_id, 0U});
        program.lanes.push_back({color_lane_id, target_id, 0U});
        const auto phase_ticks = program.length_ticks / 4;
        for (const auto property : properties) {
            AutoloopEventDefinition event;
            event.id = program_id + ".event." + property_suffix(property);
            event.lane_id = property == showcore::Property::Intensity
                ? intensity_lane_id : color_lane_id;
            event.start_tick = 0;
            event.end_tick = program.length_ticks;
            event.property = property;
            if (property == showcore::Property::Intensity) {
                event.kind = AutoloopEventKind::PropertyBlock;
                event.value = showcore::PropertyValue::set(bank.energy);
                event.interpolation = AutoloopInterpolation::Hold;
            } else {
                event.kind = AutoloopEventKind::PropertyCurve;
                event.value = showcore::PropertyValue::release();
                event.interpolation = AutoloopInterpolation::SmoothStep;
                for (std::size_t phase = 0U; phase < 4U; ++phase) {
                    const auto& color = kOriginalStarterColors[
                        (slot + phase * 3U + bank_index * 2U) %
                        kOriginalStarterColors.size()];
                    event.curve_points.push_back({
                        static_cast<MusicalTick>(phase) * phase_ticks,
                        showcore::PropertyValue::set(
                            color_value(color, property, bank.energy))});
                }
                const auto& first_color = kOriginalStarterColors[
                    (slot + bank_index * 2U) %
                    kOriginalStarterColors.size()];
                event.curve_points.push_back({
                    program.length_ticks,
                    showcore::PropertyValue::set(
                        color_value(first_color, property, bank.energy))});
            }
            program.events.push_back(std::move(event));
        }
        pack.source.programs.push_back(std::move(program));
        pack.source.launch_profiles.push_back({
            launch_id,
            showcore::AutoloopRepeat::Infinite,
            AutoloopLaunchQuantization::Immediate,
            AutoloopPhaseOrigin::Launch,
            AutoloopPlaybackMode::Overlay,
            0,
            false});
        pack.source.provenance.push_back({
            provenance_id,
            AutoloopProvenanceOrigin::ContentPack,
            pack.id,
            pack.semantic_version,
            0U,
            {},
            {},
            asset_id,
            "original"});
    }
    normalize_autoloop_source(pack.source);
    pack.content_digest = autoloop_source_digest(pack.source);
    return pack;
}

bool valid_autoloop_content_pack(const AutoloopContentPack& pack) {
    if (pack.id.empty() || pack.version == 0U ||
        pack.semantic_version.empty() || pack.management_key_prefix.empty() ||
        pack.license_notice.empty() || pack.content_digest.size() != 64U ||
        pack.source.assets.empty() || pack.source.placements.empty() ||
        !validate_autoloop_source(pack.source).ok() ||
        autoloop_source_digest(pack.source) != pack.content_digest) {
        return false;
    }
    std::unordered_set<std::string_view> management_keys;
    const auto key_prefix = pack.management_key_prefix + "/";
    for (const auto& placement : pack.source.placements) {
        if (!placement.content_management_key.starts_with(key_prefix) ||
            !management_keys.insert(placement.content_management_key).second) {
            return false;
        }
    }
    for (const auto& asset : pack.source.assets) {
        const auto provenance = find_by_id(
            pack.source.provenance, asset.provenance_id);
        if (provenance == pack.source.provenance.end() ||
            provenance->origin != AutoloopProvenanceOrigin::ContentPack ||
            provenance->producer_id != pack.id ||
            provenance->producer_version != pack.semantic_version) {
            return false;
        }
    }
    return true;
}

AutoloopContentPackPlan plan_autoloop_pack_populate(
    const AutoloopAuthoringSnapshot& snapshot,
    const AutoloopContentPack& pack) {
    AutoloopContentPackPlan plan;
    plan.kind = AutoloopContentPackPlanKind::PopulateEmpty;
    plan.base_generation = snapshot.generation;
    plan.pack_id = pack.id;
    plan.pack_digest = pack.content_digest;
    plan.candidate = snapshot.source;
    if (!valid_autoloop_content_pack(pack)) {
        plan.result = AutoloopContentPackPlanResult::InvalidPack;
        plan.validation = validate_autoloop_source(snapshot.source);
        plan.message = "The content pack is invalid or its digest changed.";
        return plan;
    }

    for (const auto& pack_placement : pack.source.placements) {
        const auto occupied = std::find_if(
            plan.candidate.placements.begin(), plan.candidate.placements.end(),
            [&](const auto& placement) {
                return placement.bank == pack_placement.bank &&
                    placement.slot == pack_placement.slot;
            });
        if (occupied != plan.candidate.placements.end()) {
            plan.skipped_occupied_placement_ids.push_back(occupied->id);
            continue;
        }
        if (find_by_id(plan.candidate.placements, pack_placement.id) !=
            plan.candidate.placements.end()) {
            plan.conflicts.push_back(
                "placement-id:" + pack_placement.id);
            continue;
        }
        const auto bundle = find_pack_bundle(pack, pack_placement.asset_id);
        if (!bundle.complete()) {
            plan.conflicts.push_back(
                "incomplete-pack-bundle:" + pack_placement.asset_id);
            continue;
        }
        const auto existing_asset = find_by_id(
            plan.candidate.assets, bundle.asset->id);
        if (existing_asset == plan.candidate.assets.end()) {
            if (has_record_collision(plan.candidate, bundle)) {
                plan.conflicts.push_back(
                    "content-id:" + pack_placement.asset_id);
                continue;
            }
            append_bundle(plan.candidate, bundle);
            plan.added_asset_ids.push_back(bundle.asset->id);
        } else if (!recognized_pack_asset(
                       plan.candidate, *existing_asset, pack)) {
            plan.conflicts.push_back(
                "asset-id:" + pack_placement.asset_id);
            continue;
        }
        plan.candidate.placements.push_back(pack_placement);
        plan.added_placement_ids.push_back(pack_placement.id);
        plan.affected_placement_ids.push_back(pack_placement.id);
    }
    finalize_plan(plan, snapshot);
    return plan;
}

AutoloopContentPackPlan plan_autoloop_pack_reset(
    const AutoloopAuthoringSnapshot& snapshot,
    const AutoloopContentPack& pack) {
    AutoloopContentPackPlan plan;
    plan.kind = AutoloopContentPackPlanKind::ResetManaged;
    plan.base_generation = snapshot.generation;
    plan.pack_id = pack.id;
    plan.pack_digest = pack.content_digest;
    plan.candidate = snapshot.source;
    if (!valid_autoloop_content_pack(pack)) {
        plan.result = AutoloopContentPackPlanResult::InvalidPack;
        plan.validation = validate_autoloop_source(snapshot.source);
        plan.message = "The content pack is invalid or its digest changed.";
        return plan;
    }

    std::unordered_map<std::string_view, const AutoloopPlacementDefinition*>
        defaults_by_key;
    for (const auto& placement : pack.source.placements) {
        defaults_by_key.emplace(
            placement.content_management_key, &placement);
    }
    const auto managed_prefix = pack.management_key_prefix + "/";
    std::unordered_set<std::string> reset_assets;
    for (auto& placement : plan.candidate.placements) {
        if (!placement.content_management_key.starts_with(managed_prefix)) {
            continue;
        }
        const auto expected = defaults_by_key.find(
            placement.content_management_key);
        if (expected == defaults_by_key.end()) {
            plan.conflicts.push_back(
                "unrecognized-management-key:" +
                placement.content_management_key);
            continue;
        }
        const auto bundle = find_pack_bundle(
            pack, expected->second->asset_id);
        if (!bundle.complete()) {
            plan.conflicts.push_back(
                "incomplete-pack-bundle:" + expected->second->asset_id);
            continue;
        }
        const auto existing_asset = find_by_id(
            plan.candidate.assets, bundle.asset->id);
        bool bundle_changed = false;
        if (existing_asset == plan.candidate.assets.end()) {
            if (has_record_collision(plan.candidate, bundle)) {
                plan.conflicts.push_back(
                    "content-id:" + expected->second->asset_id);
                continue;
            }
            append_bundle(plan.candidate, bundle);
            plan.added_asset_ids.push_back(bundle.asset->id);
            bundle_changed = true;
        } else {
            if (!recognized_pack_asset(
                    plan.candidate, *existing_asset, pack) ||
                existing_asset->program_id != bundle.program->id ||
                existing_asset->launch_profile_id != bundle.launch->id ||
                existing_asset->provenance_id != bundle.provenance->id) {
                plan.conflicts.push_back(
                    "asset-id:" + expected->second->asset_id);
                continue;
            }
            const auto current_bundle = serialize_bundle(
                plan.candidate, *existing_asset);
            const auto default_bundle = serialize_bundle(bundle);
            if (current_bundle.empty() || default_bundle.empty()) {
                plan.conflicts.push_back(
                    "incomplete-current-bundle:" +
                    expected->second->asset_id);
                continue;
            }
            bundle_changed = current_bundle != default_bundle;
            if (bundle_changed) {
                reset_bundle(plan.candidate, bundle);
            }
        }
        if (placement.asset_id != expected->second->asset_id) {
            placement.asset_id = expected->second->asset_id;
            plan.affected_placement_ids.push_back(placement.id);
        }
        if (bundle_changed) {
            reset_assets.insert(expected->second->asset_id);
        }
    }
    for (const auto& asset_id : reset_assets) {
        plan.reset_asset_ids.push_back(asset_id);
        for (const auto& placement : snapshot.source.placements) {
            if (placement.asset_id == asset_id) {
                plan.affected_placement_ids.push_back(placement.id);
            }
        }
    }
    finalize_plan(plan, snapshot);
    return plan;
}

AutoloopAuthoringOutcome apply_autoloop_content_pack_plan(
    AutoloopAuthoringService& service,
    const AutoloopContentPackPlan& plan) {
    const auto snapshot = service.snapshot();
    if (plan.base_generation != snapshot.generation) {
        return service.apply_candidate(
            plan.base_generation, plan.candidate);
    }
    if (!plan.committable()) {
        AutoloopAuthoringOutcome result;
        result.result =
            plan.result == AutoloopContentPackPlanResult::CapacityExceeded
            ? AutoloopAuthoringResult::CapacityExceeded
            : AutoloopAuthoringResult::InvalidCandidate;
        result.expected_generation = plan.base_generation;
        result.generation = snapshot.generation;
        result.validation = plan.validation;
        result.stable_id = plan.pack_id;
        result.message = plan.message;
        return result;
    }
    auto result = service.apply_candidate(
        plan.base_generation, plan.candidate);
    result.stable_id = plan.pack_id;
    return result;
}

const char* autoloop_content_pack_plan_result_name(
    AutoloopContentPackPlanResult result) noexcept {
    switch (result) {
    case AutoloopContentPackPlanResult::Ready: return "ready";
    case AutoloopContentPackPlanResult::NoChange: return "noChange";
    case AutoloopContentPackPlanResult::InvalidPack: return "invalidPack";
    case AutoloopContentPackPlanResult::Conflict: return "conflict";
    case AutoloopContentPackPlanResult::CapacityExceeded:
        return "capacityExceeded";
    }
    return "unknown";
}

}  // namespace emberlights
