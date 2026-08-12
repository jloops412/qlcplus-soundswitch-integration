#pragma once

#include "emberlights/project.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace emberlights {

inline constexpr std::string_view kBothLightingBoIr4SixChannelProfileId =
    "builtin.both-lighting.bo-ir4.6ch.manual-v1";
inline constexpr std::string_view kBothLightingBoIr4TenChannelProfileId =
    "builtin.both-lighting.bo-ir4.10ch.manual-v1";
inline constexpr std::string_view kBothLightingBoIr4ManualRevision =
    "both-lighting-ir4-user-manual-p8-2026-08-11";

enum class KnownFixtureProfileUpgrade : std::uint8_t {
    BothLightingBoIr4StaleTenChannel
};

struct FixtureProfileUpgradeChange {
    KnownFixtureProfileUpgrade upgrade{
        KnownFixtureProfileUpgrade::BothLightingBoIr4StaleTenChannel};
    std::size_t source_profile_index{0U};
    std::string source_profile_id;
    std::string replacement_profile_id;
    std::string before_behavior_fingerprint;
    std::string after_behavior_fingerprint;
    std::vector<std::string> affected_fixture_ids;
};

struct FixtureProfileUpgradePlan {
    std::vector<FixtureProfileUpgradeChange> changes;

    [[nodiscard]] bool empty() const noexcept { return changes.empty(); }
};

struct FixtureProfileUpgradeResult {
    bool applied{false};
    std::string message;
    std::vector<FixtureProfileUpgradeChange> changes;
};

[[nodiscard]] FixtureProfileDefinition make_both_lighting_bo_ir4_6ch_profile();
[[nodiscard]] FixtureProfileDefinition make_both_lighting_bo_ir4_10ch_profile();

// A behavior fingerprint covers the fields that affect compiled DMX output.
// It is diagnostic evidence, not a substitute for the exact signature checks
// used by the upgrade planner.
[[nodiscard]] std::string fixture_profile_behavior_fingerprint(
    const FixtureProfileDefinition& profile);

// Only the exact known-bad, unedited profile emitted by the old staged V1
// converter is eligible. Similar IDs, user-edited profiles, or different
// revisions are deliberately refused.
[[nodiscard]] FixtureProfileUpgradePlan plan_known_fixture_profile_upgrades(
    const ProjectDocument& project);

// Applies a previously reviewed plan to a project copy. The old profile is
// retained, a manual-backed replacement is added, and only exact referencing
// fixture instances are rebound. Patch addresses and authored content are not
// modified.
[[nodiscard]] FixtureProfileUpgradeResult apply_fixture_profile_upgrade_plan(
    ProjectDocument& project,
    const FixtureProfileUpgradePlan& plan);

[[nodiscard]] std::string serialize_fixture_profile_upgrade_report(
    const FixtureProfileUpgradeResult& result,
    std::string_view input_path,
    std::string_view output_path,
    std::string_view output_sha256 = {},
    std::string_view input_sha256 = {});

}  // namespace emberlights
