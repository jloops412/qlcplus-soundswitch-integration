#pragma once

#include "emberlights/fixture_parameter_catalog.hpp"
#include "emberlights/fixture_profile_upgrade.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace emberlights {

// Toolkit-neutral profile-authoring primitives. Windows, a future renderer,
// and the skins builder can all consume the same rows and mutations without
// inheriting Win32 control state or the legacy comma-separated editor format.
enum class FixtureProfileTemplateId : std::uint8_t {
    Dimmer1,
    Rgb3,
    Rgbw4,
    Rgba4,
    Rgbwauv6,
    MasterRgbwauv7,
    Rgbwa5,
    MasterRgb4,
    MasterRgbw5,
    PanTilt2
};

struct FixtureProfileTemplateDescriptor {
    FixtureProfileTemplateId id{FixtureProfileTemplateId::Dimmer1};
    std::string stable_id;
    std::string display_name;
    std::string description;
    std::uint16_t footprint{0U};
};

enum class FixtureProfileEditorError : std::uint8_t {
    None,
    InvalidTemplate,
    InvalidFootprint,
    InvalidChannel,
    InvalidDefinition,
    UnsafePreset,
    LastChannel,
    ProfileInvalid
};

struct FixtureProfileEditorMutationResult {
    FixtureProfileEditorError error{FixtureProfileEditorError::None};
    bool changed{false};
    bool replaced{false};
    std::string message;

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == FixtureProfileEditorError::None;
    }
};

struct FixtureProfileEditorRow {
    std::size_t source_index{0U};
    std::uint16_t channel{0U};
    std::uint16_t fine_channel{0U};
    showcore::Property property{showcore::Property::Count};
    showcore::ChannelEncoding encoding{showcore::ChannelEncoding::Linear8};
    std::string property_label;
    std::string encoding_label;
    std::string range_label;
    std::string default_label;
    std::string fine_label;
    std::string owner_label;
    std::string capability_label;
    std::string accessibility_label;
};

// Picker-ready rows for the complete shared semantic catalog. A UI presents
// these labels and passes the typed Property value back to the mutation APIs;
// an operator never has to type or remember a persistence ID.
struct FixtureProfileParameterChoice {
    showcore::Property property{showcore::Property::Count};
    std::string stable_id;
    std::string display_name;
    std::string description;
    std::string category_label;
    std::string control_label;
    std::string safety_label;
    bool direct_assignment_available{false};
    std::string unavailable_reason;
    std::string accessibility_label;
};

enum class FixtureProfileChannelPlacementError : std::uint8_t {
    None,
    SourceReadOnly,
    InvalidProfile,
    InvalidProperty,
    UnsafePreset,
    ProfileFull,
    CandidateInvalid
};

struct FixtureProfileChannelPlacementResult {
    FixtureProfileChannelPlacementError error{
        FixtureProfileChannelPlacementError::None};
    bool changed{false};
    bool filled_gap{false};
    bool grew_footprint{false};
    std::uint16_t channel{0U};
    std::size_t filled_count{0U};
    std::string message;

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == FixtureProfileChannelPlacementError::None;
    }
};

enum class FixtureProfileChannelFunctionSwapError : std::uint8_t {
    None,
    SourceReadOnly,
    InvalidProfile,
    InvalidChannel,
    SameChannel,
    ChannelMissing,
    FineChannelUnsupported,
    CompoundChannelUnsupported,
    UnsafeFunction,
    IncompatibleMappings,
    CandidateInvalid,
    StalePlan
};

struct FixtureProfileChannelFunctionSwapPlan {
    std::string profile_id;
    std::string profile_name;
    std::string source_revision;
    showcore::FixtureProfileSource source{
        showcore::FixtureProfileSource::Local};
    std::string source_behavior_fingerprint;
    std::uint16_t first_channel{0U};
    std::uint16_t second_channel{0U};
    showcore::Property first_property_before{showcore::Property::Count};
    showcore::Property second_property_before{showcore::Property::Count};
    showcore::Property first_property_after{showcore::Property::Count};
    showcore::Property second_property_after{showcore::Property::Count};
    std::string candidate_behavior_fingerprint;
    bool changes_mapping{false};
};

struct FixtureProfileChannelFunctionSwapResult {
    FixtureProfileChannelFunctionSwapError error{
        FixtureProfileChannelFunctionSwapError::InvalidProfile};
    bool changed{false};
    FixtureProfileChannelFunctionSwapPlan plan;
    std::string message;

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == FixtureProfileChannelFunctionSwapError::None;
    }
};

enum class FixtureChannelCapabilityEditorError : std::uint8_t {
    None,
    InvalidChannel,
    InvalidIdentity,
    InvalidProperty,
    InvalidRange,
    InvalidPreferredValue,
    InvalidAccess,
    DuplicateRange,
    MissingCapability,
    ProtectedCapability,
    ProfileInvalid
};

struct FixtureChannelCapabilityMutationResult {
    FixtureChannelCapabilityEditorError error{
        FixtureChannelCapabilityEditorError::None};
    bool changed{false};
    bool replaced{false};
    std::string message;

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == FixtureChannelCapabilityEditorError::None;
    }
};

struct FixtureChannelCapabilityRow {
    std::size_t source_index{0U};
    std::string id;
    std::string name;
    std::string parameter_label;
    std::string range_label;
    std::string preferred_label;
    std::string behavior_label;
    std::string access_label;
    std::string role_label;
    std::string accessibility_label;
};

struct FixtureChannelCapabilitySelection {
    FixtureChannelCapabilityEditorError error{
        FixtureChannelCapabilityEditorError::MissingCapability};
    bool found{false};
    showcore::Property property{showcore::Property::Count};
    float normalized_value{0.0F};
    float semantic_min{0.0F};
    float semantic_max{1.0F};
    std::uint8_t raw_value{0U};
    std::string binding_id;
    std::string message;

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == FixtureChannelCapabilityEditorError::None && found;
    }
};

enum class FixtureProfileAuditSeverity : std::uint8_t {
    Info,
    Warning,
    Error
};

struct FixtureProfileAuditIssue {
    FixtureProfileAuditSeverity severity{FixtureProfileAuditSeverity::Info};
    std::string stable_code;
    std::uint16_t channel{0U};
    std::string message;
};

struct FixtureProfileAudit {
    bool structurally_valid{false};
    bool structurally_complete{false};
    std::size_t mapped_slot_count{0U};
    std::size_t unmapped_slot_count{0U};
    std::size_t semantic_mapping_count{0U};
    std::size_t safe_constant_count{0U};
    std::size_t manual_chart_review_count{0U};
    std::size_t safety_restricted_count{0U};
    std::size_t custom_mapping_count{0U};
    std::size_t repeated_semantic_count{0U};
    std::size_t named_capability_count{0U};
    std::size_t protected_capability_count{0U};
    std::size_t compound_channel_count{0U};
    std::size_t owned_cell_or_head_count{0U};
    std::vector<FixtureProfileAuditIssue> issues;
    std::string text;
};

enum class FixtureProfileRebindError : std::uint8_t {
    None,
    InvalidSource,
    InvalidReplacement,
    SameProfile,
    InvalidCandidate,
    CompilationFailed
};

struct FixtureProfileRebindResult {
    FixtureProfileRebindError error{FixtureProfileRebindError::None};
    bool changed{false};
    std::vector<std::string> fixture_ids;
    std::string message;

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == FixtureProfileRebindError::None;
    }
};

[[nodiscard]] std::span<const FixtureProfileTemplateDescriptor>
fixture_profile_templates() noexcept;

[[nodiscard]] FixtureProfileEditorMutationResult apply_fixture_profile_template(
    FixtureProfileDefinition& draft,
    FixtureProfileTemplateId template_id);

// Produces safe defaults for direct emitters and movement channels. Channels
// whose safe range depends on the physical DMX chart are deliberately refused.
[[nodiscard]] FixtureProfileEditorMutationResult
make_safe_fixture_profile_channel(
    showcore::Property property,
    std::uint16_t one_based_channel,
    ChannelDefinition& definition);

[[nodiscard]] FixtureProfileEditorMutationResult upsert_fixture_profile_channel(
    FixtureProfileDefinition& draft,
    const ChannelDefinition& definition);

[[nodiscard]] FixtureProfileEditorMutationResult remove_fixture_profile_channel(
    FixtureProfileDefinition& draft,
    std::uint16_t one_based_channel);

[[nodiscard]] FixtureProfileEditorMutationResult
update_fixture_profile_channel_metadata(
    FixtureProfileDefinition& draft,
    std::uint16_t one_based_channel,
    std::string owner,
    std::uint16_t blackout_value,
    std::uint16_t highlight_value);

[[nodiscard]] std::vector<FixtureProfileEditorRow> fixture_profile_editor_rows(
    const FixtureProfileDefinition& profile);

[[nodiscard]] std::vector<FixtureProfileParameterChoice>
fixture_profile_parameter_choices();

// Assigns a catalog parameter to the lowest unused physical slot. If the
// current footprint is completely described, the footprint grows by one.
// Fine slots count as occupied, and chart-dependent/hazardous functions are
// refused until their ranges are authored explicitly.
[[nodiscard]] FixtureProfileChannelPlacementResult
assign_next_or_append_fixture_profile_channel(
    FixtureProfileDefinition& draft,
    showcore::Property property);

// Makes every currently unused footprint slot explicit as a zero-valued safe
// constant. This never overwrites a coarse or fine slot and is a successful
// no-op when the profile is already complete.
[[nodiscard]] FixtureProfileChannelPlacementResult
fill_fixture_profile_channel_gaps_with_safe_constants(
    FixtureProfileDefinition& draft);

// Plans and applies a property-label exchange between two compatible, direct
// Linear8 channels. Physical offsets, owners, ranges, safe values, and every
// unrelated row remain fixed. Fine, compound, chart-dependent, hazardous, or
// physically incompatible rows fail closed. Only Local snapshots are mutable;
// imported/built-in sources must be duplicated at the UI boundary first.
[[nodiscard]] FixtureProfileChannelFunctionSwapResult
plan_fixture_profile_channel_function_swap(
    const FixtureProfileDefinition& profile,
    std::uint16_t first_channel,
    std::uint16_t second_channel);

[[nodiscard]] FixtureProfileChannelFunctionSwapResult
apply_fixture_profile_channel_function_swap(
    FixtureProfileDefinition& profile,
    const FixtureProfileChannelFunctionSwapPlan& plan);

[[nodiscard]] std::string make_fixture_channel_capability_id(
    std::string_view label);

[[nodiscard]] FixtureChannelCapabilityMutationResult
upsert_fixture_channel_capability(
    FixtureProfileDefinition& draft,
    std::uint16_t one_based_channel,
    const ChannelCapabilityDefinition& definition);

[[nodiscard]] FixtureChannelCapabilityMutationResult
remove_fixture_channel_capability(
    FixtureProfileDefinition& draft,
    std::uint16_t one_based_channel,
    std::string_view capability_id);

[[nodiscard]] std::vector<FixtureChannelCapabilityRow>
fixture_channel_capability_rows(
    const FixtureProfileDefinition& profile,
    std::uint16_t one_based_channel);

// Resolves a stable named capability into the ordinary property/value contract
// already used by Static Looks, Live overrides, MIDI, and future skins. Slot
// ranges resolve to their preferred raw value; continuous ranges use position.
[[nodiscard]] FixtureChannelCapabilitySelection
resolve_fixture_channel_capability(
    const FixtureProfileDefinition& profile,
    std::uint16_t one_based_channel,
    std::string_view capability_id,
    float position = 0.5F);

[[nodiscard]] std::string_view fixture_channel_capability_behavior_name(
    showcore::ChannelCapabilityBehavior behavior) noexcept;
[[nodiscard]] std::string_view fixture_channel_capability_access_name(
    showcore::ChannelCapabilityAccess access) noexcept;
[[nodiscard]] std::string_view fixture_channel_capability_role_name(
    FixtureChannelCapabilityRole role) noexcept;

// Reports profile structure and the exact rows that still need a fixture DMX
// chart. This does not claim physical qualification or source trust.
[[nodiscard]] FixtureProfileAudit audit_fixture_profile(
    const FixtureProfileDefinition& profile);

// Atomically moves every current fixture from one saved profile snapshot to
// another, then validates and compiles the candidate before replacing the
// document. Future Studio renderers can use this without inheriting Win32 UI.
[[nodiscard]] FixtureProfileRebindResult rebind_fixture_profile_instances(
    ProjectDocument& project,
    std::string_view source_profile_id,
    std::string_view replacement_profile_id);

enum class FixtureProfileWhiteAmberAssignmentError : std::uint8_t {
    None,
    InvalidSelection,
    InvalidProfile,
    MappingUnavailable,
    SelectionIsNotWhiteAmberPair,
    CorrectionUnavailable
};

struct FixtureProfileWhiteAmberAssignmentPlanResult {
    FixtureProfileWhiteAmberAssignmentError error{
        FixtureProfileWhiteAmberAssignmentError::InvalidProfile};
    bool already_assigned{false};
    FixtureProfileMappingSummary current_mapping;
    FixtureProfileWhiteAmberCorrectionPlan plan;
    std::string message;

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == FixtureProfileWhiteAmberAssignmentError::None;
    }
};

// Unlike the old toggle, this plans an absolute assignment: the requested
// channel numbers describe the desired final state. Asking for the current
// state is a successful no-op and can never reverse a previous correction.
[[nodiscard]] FixtureProfileWhiteAmberAssignmentPlanResult
plan_fixture_profile_white_amber_assignment(
    const ProjectDocument& project,
    std::string_view profile_id,
    std::uint16_t desired_white_channel,
    std::uint16_t desired_amber_channel);

}  // namespace emberlights
