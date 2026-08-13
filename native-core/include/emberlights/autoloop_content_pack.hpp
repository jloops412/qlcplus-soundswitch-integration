#pragma once

#include "emberlights/autoloop_authoring.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace emberlights {

inline constexpr std::string_view kEmberlightsStarterAutoloopPackId =
    "emberlights.starter.autoloops";
inline constexpr std::uint32_t kEmberlightsStarterAutoloopPackVersion = 1U;
inline constexpr std::size_t kEmberlightsStarterAutoloopPlacementCount = 128U;
inline constexpr std::string_view kEmberlightsStarterAutoloopManagementPrefix =
    "emberlights.starter.autoloops/v1";

struct AutoloopContentPack {
    std::string id;
    std::uint32_t version{0U};
    std::string semantic_version;
    std::string management_key_prefix;
    std::string license_notice;
    AutoloopSourceDocument source;
    std::string content_digest;
};

enum class AutoloopContentPackPlanResult : std::uint8_t {
    Ready,
    NoChange,
    InvalidPack,
    Conflict,
    CapacityExceeded
};

enum class AutoloopContentPackPlanKind : std::uint8_t {
    PopulateEmpty,
    ResetManaged
};

// The candidate is retained with its base generation so review and commit are
// separate. Stable-ID lists are exact, sorted, and safe for a future UI/report.
struct AutoloopContentPackPlan {
    AutoloopContentPackPlanResult result{
        AutoloopContentPackPlanResult::InvalidPack};
    AutoloopContentPackPlanKind kind{
        AutoloopContentPackPlanKind::PopulateEmpty};
    StudioDocumentGeneration base_generation{0U};
    std::string pack_id;
    std::string pack_digest;
    AutoloopSourceDocument candidate;
    AutoloopSourceValidation validation;
    std::vector<std::string> added_asset_ids;
    std::vector<std::string> added_placement_ids;
    std::vector<std::string> reset_asset_ids;
    std::vector<std::string> affected_placement_ids;
    std::vector<std::string> skipped_occupied_placement_ids;
    std::vector<std::string> conflicts;
    std::string message;

    [[nodiscard]] bool committable() const noexcept {
        return result == AutoloopContentPackPlanResult::Ready ||
            result == AutoloopContentPackPlanResult::NoChange;
    }
};

// Original, independently authored source generated from fixed EmberLights
// rules. It does not read vendor defaults, private projects, fixture data, the
// filesystem, a network, or a random source.
[[nodiscard]] AutoloopContentPack make_emberlights_starter_autoloop_pack();
[[nodiscard]] bool valid_autoloop_content_pack(
    const AutoloopContentPack& pack);

[[nodiscard]] AutoloopContentPackPlan plan_autoloop_pack_populate(
    const AutoloopAuthoringSnapshot& snapshot,
    const AutoloopContentPack& pack);
[[nodiscard]] AutoloopContentPackPlan plan_autoloop_pack_reset(
    const AutoloopAuthoringSnapshot& snapshot,
    const AutoloopContentPack& pack);
[[nodiscard]] AutoloopAuthoringOutcome apply_autoloop_content_pack_plan(
    AutoloopAuthoringService& service,
    const AutoloopContentPackPlan& plan);

[[nodiscard]] const char* autoloop_content_pack_plan_result_name(
    AutoloopContentPackPlanResult result) noexcept;

}  // namespace emberlights
