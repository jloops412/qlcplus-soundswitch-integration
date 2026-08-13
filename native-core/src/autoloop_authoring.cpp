#include "emberlights/autoloop_authoring.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace emberlights {
namespace {

template <typename Collection>
[[nodiscard]] auto find_by_id(Collection& collection, std::string_view id) {
    return std::find_if(
        collection.begin(), collection.end(),
        [id](const auto& value) { return value.id == id; });
}

[[nodiscard]] bool has_capacity_issue(
    const AutoloopSourceValidation& validation) {
    return std::any_of(
        validation.issues.begin(), validation.issues.end(),
        [](const auto& issue) {
            return issue.code.starts_with("autoloop.authoring.capacity");
        });
}

void sort_unique(std::vector<std::string>& values) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
}

}  // namespace

AutoloopAuthoringService::AutoloopAuthoringService()
    : AutoloopAuthoringService(AutoloopSourceDocument{}, 1U) {}

AutoloopAuthoringService::AutoloopAuthoringService(
    AutoloopSourceDocument source,
    StudioDocumentGeneration generation)
    : source_(std::move(source)), generation_(generation) {
    if (generation_ == 0U) {
        throw std::invalid_argument(
            "An Autoloop authoring generation must be non-zero.");
    }
    normalize_autoloop_source(source_);
    const auto validation = validate_candidate(source_);
    if (!validation.ok()) {
        throw std::invalid_argument(
            "AutoloopAuthoringService requires a valid source document.");
    }
    serialized_ = serialize_autoloop_source(source_);
    if (serialized_.empty()) {
        throw std::invalid_argument(
            "AutoloopAuthoringService could not canonicalize source.");
    }
}

AutoloopAuthoringSnapshot AutoloopAuthoringService::snapshot() const {
    return {
        source_,
        generation_,
        undo_source_.has_value(),
        redo_source_.has_value(),
        autoloop_source_digest(source_)};
}

AutoloopAuthoringOutcome AutoloopAuthoringService::outcome(
    AutoloopAuthoringResult result,
    StudioDocumentGeneration expected_generation,
    std::string message) const {
    return {
        result,
        expected_generation,
        generation_,
        validate_candidate(source_),
        {},
        {},
        std::nullopt,
        std::move(message)};
}

AutoloopSourceValidation AutoloopAuthoringService::validate_candidate(
    const AutoloopSourceDocument& candidate) const {
    auto validation = validate_autoloop_source(candidate);
    const auto add_capacity = [&](std::string code, std::string subject,
                                  std::string message) {
        validation.issues.push_back({
            AutoloopSourceIssueSeverity::Error,
            std::move(code),
            std::move(subject),
            std::move(message)});
    };
    if (candidate.assets.size() > kMaximumAutoloopAuthoringAssets) {
        add_capacity(
            "autoloop.authoring.capacity.assets", "assets",
            "Autoloop assets exceed the bounded authoring capacity.");
    }
    if (candidate.placements.size() > showcore::kMaxAutoloops) {
        add_capacity(
            "autoloop.authoring.capacity.placements", "placements",
            "Autoloop placements exceed the 64 by 32 address capacity.");
    }
    if (candidate.programs.size() > kMaximumAutoloopAuthoringPrograms) {
        add_capacity(
            "autoloop.authoring.capacity.programs", "programs",
            "Autoloop programs exceed the bounded authoring capacity.");
    }
    if (candidate.launch_profiles.size() >
        kMaximumAutoloopAuthoringLaunchProfiles) {
        add_capacity(
            "autoloop.authoring.capacity.launchProfiles", "launchProfiles",
            "Autoloop launch profiles exceed the bounded authoring capacity.");
    }
    if (candidate.provenance.size() >
        kMaximumAutoloopAuthoringProvenanceRecords) {
        add_capacity(
            "autoloop.authoring.capacity.provenance", "provenance",
            "Autoloop provenance records exceed the bounded authoring capacity.");
    }
    return validation;
}

bool AutoloopAuthoringService::can_advance_generation() const noexcept {
    return generation_ !=
        std::numeric_limits<StudioDocumentGeneration>::max();
}

void AutoloopAuthoringService::advance_generation() noexcept {
    ++generation_;
}

AutoloopAuthoringOutcome AutoloopAuthoringService::apply_candidate(
    StudioDocumentGeneration expected_generation,
    AutoloopSourceDocument candidate) {
    if (expected_generation != generation_) {
        return outcome(
            AutoloopAuthoringResult::StaleGeneration, expected_generation,
            "The Autoloop source changed after this edit began.");
    }
    normalize_autoloop_source(candidate);
    const auto validation = validate_candidate(candidate);
    if (!validation.ok()) {
        auto rejected = outcome(
            has_capacity_issue(validation)
                ? AutoloopAuthoringResult::CapacityExceeded
                : AutoloopAuthoringResult::ValidationFailed,
            expected_generation,
            "The Autoloop source candidate failed validation.");
        rejected.validation = validation;
        return rejected;
    }
    const auto serialized = serialize_autoloop_source(candidate);
    if (serialized.empty()) {
        return outcome(
            AutoloopAuthoringResult::InvalidCandidate, expected_generation,
            "The Autoloop source candidate could not be canonicalized.");
    }
    if (serialized == serialized_) {
        return outcome(
            AutoloopAuthoringResult::NoChange, expected_generation,
            "The Autoloop source candidate is identical.");
    }
    if (!can_advance_generation()) {
        return outcome(
            AutoloopAuthoringResult::GenerationExhausted,
            expected_generation,
            "The Autoloop authoring generation cannot advance.");
    }
    undo_source_ = source_;
    redo_source_.reset();
    source_ = std::move(candidate);
    serialized_ = serialized;
    advance_generation();
    return outcome(
        AutoloopAuthoringResult::Applied, expected_generation,
        "The Autoloop source candidate was applied as one transaction.");
}

AutoloopAuthoringOutcome AutoloopAuthoringService::create_asset(
    StudioDocumentGeneration expected_generation,
    AutoloopAssetBundle bundle) {
    if (expected_generation != generation_) {
        return outcome(
            AutoloopAuthoringResult::StaleGeneration, expected_generation,
            "The Autoloop source changed after this asset draft began.");
    }
    const auto stable_id = bundle.asset.id;
    auto candidate = source_;
    candidate.assets.push_back(std::move(bundle.asset));
    candidate.programs.push_back(std::move(bundle.program));
    candidate.launch_profiles.push_back(std::move(bundle.launch_profile));
    candidate.provenance.push_back(std::move(bundle.provenance));
    auto result = apply_candidate(expected_generation, std::move(candidate));
    result.stable_id = stable_id;
    return result;
}

AutoloopAuthoringOutcome AutoloopAuthoringService::rename_asset(
    StudioDocumentGeneration expected_generation,
    std::string_view asset_id,
    std::string name) {
    if (expected_generation != generation_) {
        return outcome(
            AutoloopAuthoringResult::StaleGeneration, expected_generation,
            "The Autoloop source changed after this rename began.");
    }
    auto candidate = source_;
    const auto asset = find_by_id(candidate.assets, asset_id);
    if (asset == candidate.assets.end()) {
        auto result = outcome(
            AutoloopAuthoringResult::MissingAsset, expected_generation,
            "The Autoloop asset no longer exists.");
        result.stable_id = std::string(asset_id);
        return result;
    }
    asset->name = std::move(name);
    auto result = apply_candidate(expected_generation, std::move(candidate));
    result.stable_id = std::string(asset_id);
    return result;
}

AutoloopAuthoringOutcome AutoloopAuthoringService::duplicate_asset(
    StudioDocumentGeneration expected_generation,
    std::string_view source_asset_id,
    std::string new_asset_id,
    std::string new_program_id,
    std::string new_launch_profile_id,
    std::string new_provenance_id,
    std::string new_name) {
    if (expected_generation != generation_) {
        return outcome(
            AutoloopAuthoringResult::StaleGeneration, expected_generation,
            "The Autoloop source changed after this duplicate began.");
    }
    const auto asset = find_by_id(source_.assets, source_asset_id);
    if (asset == source_.assets.end()) {
        auto result = outcome(
            AutoloopAuthoringResult::MissingAsset, expected_generation,
            "The Autoloop asset to duplicate does not exist.");
        result.stable_id = std::string(source_asset_id);
        return result;
    }
    const auto program = find_by_id(source_.programs, asset->program_id);
    const auto launch = find_by_id(
        source_.launch_profiles, asset->launch_profile_id);
    const auto provenance = find_by_id(
        source_.provenance, asset->provenance_id);
    if (program == source_.programs.end() ||
        launch == source_.launch_profiles.end() ||
        provenance == source_.provenance.end()) {
        auto result = outcome(
            AutoloopAuthoringResult::InvalidCandidate, expected_generation,
            "The source asset has an incomplete canonical dependency bundle.");
        result.stable_id = std::string(source_asset_id);
        return result;
    }

    AutoloopAssetBundle duplicate;
    duplicate.asset = *asset;
    duplicate.asset.id = std::move(new_asset_id);
    duplicate.asset.name = std::move(new_name);
    duplicate.asset.program_id = std::move(new_program_id);
    duplicate.asset.launch_profile_id = std::move(new_launch_profile_id);
    duplicate.asset.provenance_id = std::move(new_provenance_id);
    duplicate.asset.revision = 1U;
    duplicate.program = *program;
    duplicate.program.id = duplicate.asset.program_id;
    duplicate.launch_profile = *launch;
    duplicate.launch_profile.id = duplicate.asset.launch_profile_id;
    duplicate.provenance = *provenance;
    duplicate.provenance.id = duplicate.asset.provenance_id;
    duplicate.provenance.origin = AutoloopProvenanceOrigin::Native;
    duplicate.provenance.producer_id = "emberlights.autoloop-authoring";
    duplicate.provenance.producer_version = "1";
    duplicate.provenance.seed = 0U;
    duplicate.provenance.source_bundle_id.clear();
    duplicate.provenance.source_artifact_id.clear();
    duplicate.provenance.source_object_key = std::string(source_asset_id);
    duplicate.provenance.evidence_status = "duplicated";
    return create_asset(expected_generation, std::move(duplicate));
}

AutoloopDependencyReport AutoloopAuthoringService::inspect_dependencies(
    std::string_view asset_id) const {
    AutoloopDependencyReport report;
    report.asset_id = std::string(asset_id);
    const auto asset = find_by_id(source_.assets, asset_id);
    if (asset == source_.assets.end()) {
        return report;
    }
    for (const auto& placement : source_.placements) {
        if (placement.asset_id == asset_id) {
            report.placement_ids.push_back(placement.id);
        }
    }
    for (const auto& other : source_.assets) {
        if (other.id == asset_id) {
            continue;
        }
        if (other.program_id == asset->program_id) {
            report.assets_sharing_program.push_back(other.id);
        }
        if (other.launch_profile_id == asset->launch_profile_id) {
            report.assets_sharing_launch_profile.push_back(other.id);
        }
        if (other.provenance_id == asset->provenance_id) {
            report.assets_sharing_provenance.push_back(other.id);
        }
    }
    if (report.assets_sharing_program.empty()) {
        report.orphan_records.push_back(asset->program_id);
    }
    if (report.assets_sharing_launch_profile.empty()) {
        report.orphan_records.push_back(asset->launch_profile_id);
    }
    if (report.assets_sharing_provenance.empty()) {
        report.orphan_records.push_back(asset->provenance_id);
    }
    sort_unique(report.placement_ids);
    sort_unique(report.assets_sharing_program);
    sort_unique(report.assets_sharing_launch_profile);
    sort_unique(report.assets_sharing_provenance);
    sort_unique(report.orphan_records);
    return report;
}

AutoloopAuthoringOutcome AutoloopAuthoringService::delete_asset(
    StudioDocumentGeneration expected_generation,
    std::string_view asset_id,
    bool remove_dependent_placements) {
    if (expected_generation != generation_) {
        return outcome(
            AutoloopAuthoringResult::StaleGeneration, expected_generation,
            "The Autoloop source changed after this delete began.");
    }
    const auto asset = find_by_id(source_.assets, asset_id);
    if (asset == source_.assets.end()) {
        auto result = outcome(
            AutoloopAuthoringResult::MissingAsset, expected_generation,
            "The Autoloop asset to delete does not exist.");
        result.stable_id = std::string(asset_id);
        return result;
    }
    const auto dependencies = inspect_dependencies(asset_id);
    if (dependencies.has_placement_dependencies() &&
        !remove_dependent_placements) {
        auto result = outcome(
            AutoloopAuthoringResult::DependencyConflict, expected_generation,
            "Deleting this asset requires explicit removal of its placements.");
        result.stable_id = std::string(asset_id);
        result.dependencies = dependencies;
        return result;
    }

    const auto program_id = asset->program_id;
    const auto launch_id = asset->launch_profile_id;
    const auto provenance_id = asset->provenance_id;
    auto candidate = source_;
    if (remove_dependent_placements) {
        std::erase_if(candidate.placements, [&](const auto& placement) {
            return placement.asset_id == asset_id;
        });
    }
    std::erase_if(candidate.assets, [&](const auto& value) {
        return value.id == asset_id;
    });
    if (dependencies.assets_sharing_program.empty()) {
        std::erase_if(candidate.programs, [&](const auto& value) {
            return value.id == program_id;
        });
    }
    if (dependencies.assets_sharing_launch_profile.empty()) {
        std::erase_if(candidate.launch_profiles, [&](const auto& value) {
            return value.id == launch_id;
        });
    }
    if (dependencies.assets_sharing_provenance.empty()) {
        std::erase_if(candidate.provenance, [&](const auto& value) {
            return value.id == provenance_id;
        });
    }
    auto result = apply_candidate(expected_generation, std::move(candidate));
    result.stable_id = std::string(asset_id);
    result.dependencies = dependencies;
    return result;
}

AutoloopAuthoringOutcome AutoloopAuthoringService::assign_placement(
    StudioDocumentGeneration expected_generation,
    std::string placement_id,
    showcore::AutoloopAddress address,
    std::string asset_id,
    std::string content_management_key) {
    if (expected_generation != generation_) {
        return outcome(
            AutoloopAuthoringResult::StaleGeneration, expected_generation,
            "The Autoloop source changed after this placement began.");
    }
    if (!address.valid()) {
        return outcome(
            AutoloopAuthoringResult::InvalidCandidate, expected_generation,
            "The placement address is outside the 64 by 32 catalog.");
    }
    if (find_by_id(source_.assets, asset_id) == source_.assets.end()) {
        auto result = outcome(
            AutoloopAuthoringResult::MissingAsset, expected_generation,
            "The placement asset does not exist.");
        result.stable_id = asset_id;
        result.address = address;
        return result;
    }
    if (find_by_id(source_.placements, placement_id) !=
        source_.placements.end()) {
        auto result = outcome(
            AutoloopAuthoringResult::InvalidCandidate, expected_generation,
            "A placement already uses this stable ID.");
        result.stable_id = placement_id;
        result.address = address;
        return result;
    }
    const auto occupied = std::find_if(
        source_.placements.begin(), source_.placements.end(),
        [address](const auto& placement) {
            return placement.bank == address.bank &&
                placement.slot == address.slot;
        });
    if (occupied != source_.placements.end()) {
        auto result = outcome(
            AutoloopAuthoringResult::OccupiedPlacement, expected_generation,
            "The destination is occupied; use an explicit swap.");
        result.stable_id = occupied->id;
        result.address = address;
        return result;
    }
    if (source_.placements.size() >= showcore::kMaxAutoloops) {
        auto result = outcome(
            AutoloopAuthoringResult::CapacityExceeded, expected_generation,
            "The 64 by 32 placement catalog is full.");
        result.address = address;
        return result;
    }
    auto candidate = source_;
    candidate.placements.push_back({
        placement_id, address.bank, address.slot, asset_id,
        std::move(content_management_key)});
    auto result = apply_candidate(expected_generation, std::move(candidate));
    result.stable_id = std::move(placement_id);
    result.address = address;
    return result;
}

AutoloopAuthoringOutcome AutoloopAuthoringService::assign_next_open(
    StudioDocumentGeneration expected_generation,
    std::string placement_id,
    std::string asset_id,
    showcore::AutoloopAddress after,
    std::string content_management_key) {
    if (expected_generation != generation_) {
        return outcome(
            AutoloopAuthoringResult::StaleGeneration, expected_generation,
            "The Autoloop source changed after this placement began.");
    }
    const auto open = next_open(after);
    if (!open.found) {
        return outcome(
            AutoloopAuthoringResult::CapacityExceeded, expected_generation,
            "No open Autoloop placement remains.");
    }
    return assign_placement(
        expected_generation, std::move(placement_id), open.address,
        std::move(asset_id), std::move(content_management_key));
}

AutoloopAuthoringOutcome AutoloopAuthoringService::unassign_placement(
    StudioDocumentGeneration expected_generation,
    std::string_view placement_id) {
    if (expected_generation != generation_) {
        return outcome(
            AutoloopAuthoringResult::StaleGeneration, expected_generation,
            "The Autoloop source changed after this unassign began.");
    }
    if (find_by_id(source_.placements, placement_id) ==
        source_.placements.end()) {
        auto result = outcome(
            AutoloopAuthoringResult::MissingPlacement, expected_generation,
            "The placement to unassign does not exist.");
        result.stable_id = std::string(placement_id);
        return result;
    }
    auto candidate = source_;
    std::erase_if(candidate.placements, [&](const auto& placement) {
        return placement.id == placement_id;
    });
    auto result = apply_candidate(expected_generation, std::move(candidate));
    result.stable_id = std::string(placement_id);
    return result;
}

AutoloopAuthoringOutcome AutoloopAuthoringService::move_placement(
    StudioDocumentGeneration expected_generation,
    std::string_view placement_id,
    showcore::AutoloopAddress destination) {
    if (expected_generation != generation_) {
        return outcome(
            AutoloopAuthoringResult::StaleGeneration, expected_generation,
            "The Autoloop source changed after this move began.");
    }
    if (!destination.valid()) {
        return outcome(
            AutoloopAuthoringResult::InvalidCandidate, expected_generation,
            "The move destination is outside the 64 by 32 catalog.");
    }
    auto candidate = source_;
    const auto placement = find_by_id(candidate.placements, placement_id);
    if (placement == candidate.placements.end()) {
        auto result = outcome(
            AutoloopAuthoringResult::MissingPlacement, expected_generation,
            "The placement to move does not exist.");
        result.stable_id = std::string(placement_id);
        return result;
    }
    const auto occupied = std::find_if(
        candidate.placements.begin(), candidate.placements.end(),
        [&](const auto& other) {
            return other.id != placement_id &&
                other.bank == destination.bank &&
                other.slot == destination.slot;
        });
    if (occupied != candidate.placements.end()) {
        auto result = outcome(
            AutoloopAuthoringResult::OccupiedPlacement, expected_generation,
            "The move destination is occupied; use an explicit swap.");
        result.stable_id = occupied->id;
        result.address = destination;
        return result;
    }
    placement->bank = destination.bank;
    placement->slot = destination.slot;
    auto result = apply_candidate(expected_generation, std::move(candidate));
    result.stable_id = std::string(placement_id);
    result.address = destination;
    return result;
}

AutoloopAuthoringOutcome AutoloopAuthoringService::swap_placements(
    StudioDocumentGeneration expected_generation,
    std::string_view first_placement_id,
    std::string_view second_placement_id) {
    if (expected_generation != generation_) {
        return outcome(
            AutoloopAuthoringResult::StaleGeneration, expected_generation,
            "The Autoloop source changed after this swap began.");
    }
    auto candidate = source_;
    const auto first = find_by_id(candidate.placements, first_placement_id);
    const auto second = find_by_id(candidate.placements, second_placement_id);
    if (first == candidate.placements.end() ||
        second == candidate.placements.end()) {
        auto result = outcome(
            AutoloopAuthoringResult::MissingPlacement, expected_generation,
            "Both placements must exist before they can be swapped.");
        result.stable_id = first == candidate.placements.end()
            ? std::string(first_placement_id)
            : std::string(second_placement_id);
        return result;
    }
    std::swap(first->bank, second->bank);
    std::swap(first->slot, second->slot);
    auto result = apply_candidate(expected_generation, std::move(candidate));
    result.stable_id = std::string(first_placement_id);
    return result;
}

AutoloopNextOpenPlacement AutoloopAuthoringService::next_open(
    showcore::AutoloopAddress after) const noexcept {
    std::array<bool, showcore::kMaxAutoloops> occupied{};
    for (const auto& placement : source_.placements) {
        if (placement.bank < showcore::kMaxAutoloopBanks &&
            placement.slot < showcore::kAutoloopsPerBank) {
            const auto index = static_cast<std::size_t>(placement.bank) *
                showcore::kAutoloopsPerBank + placement.slot;
            occupied[index] = true;
        }
    }
    std::size_t start = 0U;
    if (after.valid()) {
        start = (static_cast<std::size_t>(after.bank) *
            showcore::kAutoloopsPerBank + after.slot + 1U) %
            showcore::kMaxAutoloops;
    }
    for (std::size_t offset = 0U;
         offset < showcore::kMaxAutoloops; ++offset) {
        const auto index = (start + offset) % showcore::kMaxAutoloops;
        if (!occupied[index]) {
            return {
                true,
                {static_cast<std::uint16_t>(
                     index / showcore::kAutoloopsPerBank),
                 static_cast<std::uint8_t>(
                     index % showcore::kAutoloopsPerBank)}};
        }
    }
    return {};
}

AutoloopAuthoringOutcome AutoloopAuthoringService::undo(
    StudioDocumentGeneration expected_generation) {
    if (expected_generation != generation_) {
        return outcome(
            AutoloopAuthoringResult::StaleGeneration, expected_generation,
            "The Autoloop source changed before Undo.");
    }
    if (!undo_source_.has_value()) {
        return outcome(
            AutoloopAuthoringResult::UndoUnavailable, expected_generation,
            "No Autoloop source transaction is available to undo.");
    }
    if (!can_advance_generation()) {
        return outcome(
            AutoloopAuthoringResult::GenerationExhausted,
            expected_generation,
            "The Autoloop authoring generation cannot advance.");
    }
    redo_source_ = source_;
    source_ = std::move(*undo_source_);
    undo_source_.reset();
    serialized_ = serialize_autoloop_source(source_);
    advance_generation();
    return outcome(
        AutoloopAuthoringResult::Applied, expected_generation,
        "The last Autoloop source transaction was undone.");
}

AutoloopAuthoringOutcome AutoloopAuthoringService::redo(
    StudioDocumentGeneration expected_generation) {
    if (expected_generation != generation_) {
        return outcome(
            AutoloopAuthoringResult::StaleGeneration, expected_generation,
            "The Autoloop source changed before Redo.");
    }
    if (!redo_source_.has_value()) {
        return outcome(
            AutoloopAuthoringResult::RedoUnavailable, expected_generation,
            "No Autoloop source transaction is available to redo.");
    }
    if (!can_advance_generation()) {
        return outcome(
            AutoloopAuthoringResult::GenerationExhausted,
            expected_generation,
            "The Autoloop authoring generation cannot advance.");
    }
    undo_source_ = source_;
    source_ = std::move(*redo_source_);
    redo_source_.reset();
    serialized_ = serialize_autoloop_source(source_);
    advance_generation();
    return outcome(
        AutoloopAuthoringResult::Applied, expected_generation,
        "The Autoloop source transaction was redone.");
}

const char* autoloop_authoring_result_name(
    AutoloopAuthoringResult result) noexcept {
    switch (result) {
    case AutoloopAuthoringResult::Applied: return "applied";
    case AutoloopAuthoringResult::NoChange: return "noChange";
    case AutoloopAuthoringResult::StaleGeneration: return "staleGeneration";
    case AutoloopAuthoringResult::ValidationFailed: return "validationFailed";
    case AutoloopAuthoringResult::CapacityExceeded: return "capacityExceeded";
    case AutoloopAuthoringResult::InvalidCandidate: return "invalidCandidate";
    case AutoloopAuthoringResult::MissingAsset: return "missingAsset";
    case AutoloopAuthoringResult::MissingPlacement: return "missingPlacement";
    case AutoloopAuthoringResult::OccupiedPlacement: return "occupiedPlacement";
    case AutoloopAuthoringResult::DependencyConflict: return "dependencyConflict";
    case AutoloopAuthoringResult::UndoUnavailable: return "undoUnavailable";
    case AutoloopAuthoringResult::RedoUnavailable: return "redoUnavailable";
    case AutoloopAuthoringResult::GenerationExhausted: return "generationExhausted";
    }
    return "unknown";
}

}  // namespace emberlights
