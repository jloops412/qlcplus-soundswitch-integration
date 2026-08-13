#pragma once

#include "emberlights/fixture_profile_ids.hpp"
#include "emberlights/project.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace emberlights {

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

// Manual-backed built-ins are immutable project snapshots. This helper makes
// both reviewed IR-4 modes available to an older or imported project without
// replacing a user-owned profile that happens to occupy a canonical ID.
enum class Ir4ProfileAvailabilityError : std::uint8_t {
    None,
    CanonicalDefinitionInvalid,
    ConflictingSixChannelId,
    ConflictingTenChannelId,
    ProfileCapacity
};

struct Ir4ProfileAvailabilityResult {
    Ir4ProfileAvailabilityError error{Ir4ProfileAvailabilityError::None};
    bool six_channel_added{false};
    bool ten_channel_added{false};
    std::string message;

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == Ir4ProfileAvailabilityError::None;
    }
};

enum class FixtureProfileWhiteAmberCorrectionError : std::uint8_t {
    None,
    MissingWhite,
    MissingAmber,
    AmbiguousWhite,
    AmbiguousAmber,
    InvalidProfile,
    NotUserOwned
};

// Channels are one-based for display. A zero fine_channel means that the
// mapping has no fine channel.
struct FixtureChannelMappingSummary {
    std::size_t source_index{0U};
    std::uint16_t coarse_channel{0U};
    std::uint16_t fine_channel{0U};
    showcore::Property property{showcore::Property::Count};
    showcore::ChannelEncoding encoding{showcore::ChannelEncoding::Linear8};
    std::uint8_t dmx_min{0U};
    std::uint8_t dmx_max{255U};
    std::uint16_t default_value{0U};
    std::uint16_t blackout_value{0U};
    std::uint16_t highlight_value{255U};
    std::string owner;
    std::size_t capability_count{0U};
    std::string line;
};

// Structured fields support UI badges/actions; text is a complete readable
// summary suitable for the current native profile editor or an exported
// diagnostic report.
struct FixtureProfileMappingSummary {
    bool profile_valid{false};
    showcore::ProfileError profile_error{showcore::ProfileError::None};
    std::size_t profile_error_mapping_index{0U};
    std::size_t white_mapping_count{0U};
    std::size_t amber_mapping_count{0U};
    std::uint16_t white_channel{0U};
    std::uint16_t amber_channel{0U};
    FixtureProfileWhiteAmberCorrectionError correction_error{
        FixtureProfileWhiteAmberCorrectionError::InvalidProfile};
    std::string validation_message;
    std::vector<FixtureChannelMappingSummary> channels;
    std::string text;

    [[nodiscard]] bool can_correct_white_amber() const noexcept {
        return correction_error == FixtureProfileWhiteAmberCorrectionError::None;
    }
};

struct FixtureProfileWhiteAmberCorrectionResult {
    bool applied{false};
    FixtureProfileWhiteAmberCorrectionError error{
        FixtureProfileWhiteAmberCorrectionError::InvalidProfile};
    std::uint16_t white_channel_before{0U};
    std::uint16_t amber_channel_before{0U};
    std::uint16_t white_channel_after{0U};
    std::uint16_t amber_channel_after{0U};
    std::string before_behavior_fingerprint;
    std::string after_behavior_fingerprint;
    std::string message;
};

// Project-level correction is deliberately a reviewed transaction rather
// than a profile-editor convenience. Auto updates a Local profile in place;
// immutable/imported sources are preserved and forked into a corrected Local
// snapshot. CreateLocalCopy is also available for operators who want to retain
// an existing Local revision unchanged.
enum class FixtureProfileWhiteAmberProjectCorrectionMode : std::uint8_t {
    Auto,
    CreateLocalCopy,
    UpdateLocalInPlace
};

enum class FixtureProfileWhiteAmberProjectCorrectionError : std::uint8_t {
    None,
    InvalidProject,
    SourceProfileMissing,
    SourceProfileAmbiguous,
    SourceProfileReadOnly,
    ProfileCapacity,
    ReplacementIdUnavailable,
    CorrectionUnavailable,
    StalePlan,
    CandidateValidationFailed,
    CandidateCompilationFailed
};

struct FixtureProfileWhiteAmberRebindTarget {
    std::string fixture_id;
    std::string fixture_name;
    std::uint8_t universe{0U};
    std::uint16_t address{0U};
};

struct FixtureProfileWhiteAmberCorrectionPlan {
    std::size_t source_profile_index{0U};
    std::string source_profile_id;
    std::string source_snapshot_fingerprint;
    bool creates_local_copy{false};
    std::string replacement_profile_id;
    std::string replacement_profile_name;
    std::string replacement_source_revision;
    std::uint16_t white_channel_before{0U};
    std::uint16_t amber_channel_before{0U};
    std::uint16_t white_channel_after{0U};
    std::uint16_t amber_channel_after{0U};
    FixtureProfileMappingSummary before_mapping;
    FixtureProfileMappingSummary after_mapping;
    std::string before_behavior_fingerprint;
    std::string after_behavior_fingerprint;
    std::string replacement_snapshot_fingerprint;
    std::vector<FixtureProfileWhiteAmberRebindTarget> affected_fixtures;
};

struct FixtureProfileWhiteAmberCorrectionPlanResult {
    FixtureProfileWhiteAmberProjectCorrectionError error{
        FixtureProfileWhiteAmberProjectCorrectionError::InvalidProject};
    FixtureProfileWhiteAmberCorrectionError profile_error{
        FixtureProfileWhiteAmberCorrectionError::InvalidProfile};
    FixtureProfileWhiteAmberCorrectionPlan plan;
    ProjectValidation validation;
    std::string message;

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == FixtureProfileWhiteAmberProjectCorrectionError::None;
    }
};

struct FixtureProfileWhiteAmberProjectCorrectionResult {
    bool applied{false};
    FixtureProfileWhiteAmberProjectCorrectionError error{
        FixtureProfileWhiteAmberProjectCorrectionError::InvalidProject};
    FixtureProfileWhiteAmberCorrectionError profile_error{
        FixtureProfileWhiteAmberCorrectionError::InvalidProfile};
    FixtureProfileWhiteAmberCorrectionPlan plan;
    ProjectValidation validation;
    std::string message;
};

[[nodiscard]] FixtureProfileDefinition make_both_lighting_bo_ir4_6ch_profile();
[[nodiscard]] FixtureProfileDefinition make_both_lighting_bo_ir4_10ch_profile();

[[nodiscard]] Ir4ProfileAvailabilityResult
ensure_manual_backed_both_lighting_bo_ir4_profiles(ProjectDocument& project);

[[nodiscard]] FixtureProfileMappingSummary summarize_fixture_profile_mapping(
    const FixtureProfileDefinition& profile);

// Only a Local/user-owned profile may be corrected in place. Imported and
// built-in profiles must first be duplicated into a Local profile so their
// provenance snapshot remains immutable. The transaction changes only the
// White and Amber property labels; offsets, encodings, defaults, and every
// unrelated channel remain byte-for-byte equivalent.
[[nodiscard]] FixtureProfileWhiteAmberCorrectionResult
correct_fixture_profile_white_amber(FixtureProfileDefinition& profile);

// Produces the exact before/after mapping and fixture set for operator review.
// Applying the plan rechecks the complete source snapshot and fixture set,
// changes a ProjectDocument copy, validates and compiles that copy, and only
// then replaces the caller's document. A caller can therefore record the
// document as one Undo transaction without any partial profile/patch state.
[[nodiscard]] FixtureProfileWhiteAmberCorrectionPlanResult
plan_fixture_profile_white_amber_correction(
    const ProjectDocument& project,
    std::string_view source_profile_id,
    FixtureProfileWhiteAmberProjectCorrectionMode mode =
        FixtureProfileWhiteAmberProjectCorrectionMode::Auto);

[[nodiscard]] FixtureProfileWhiteAmberProjectCorrectionResult
apply_fixture_profile_white_amber_correction(
    ProjectDocument& project,
    const FixtureProfileWhiteAmberCorrectionPlan& plan);

// Convenience wrapper for non-interactive callers. User-facing surfaces
// should normally show the plan first so the before/after channels and every
// affected fixture are explicit before the atomic apply.
[[nodiscard]] FixtureProfileWhiteAmberProjectCorrectionResult
correct_and_rebind_fixture_profile_white_amber(
    ProjectDocument& project,
    std::string_view source_profile_id,
    FixtureProfileWhiteAmberProjectCorrectionMode mode =
        FixtureProfileWhiteAmberProjectCorrectionMode::Auto);

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
