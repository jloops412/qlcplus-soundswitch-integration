#pragma once

#include "emberlights/project.hpp"

#include "showcore/types.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace emberlights {

// Shared Studio musical-time contract. Rich Autoloops never introduce a
// floating-point or wall-clock timing authority into authored content.
using MusicalTick = std::int64_t;
inline constexpr MusicalTick kMusicalTicksPerQuarter = 960;
inline constexpr std::uint32_t kAutoloopSourceFormatVersion = 1U;
inline constexpr std::string_view kAutoloopSourceFormat =
    "emberlights-autoloop-source";
inline constexpr std::size_t kMaximumAutoloopSourceIdentifierLength = 255U;
inline constexpr std::size_t kMaximumAutoloopSourceTextLength = 4096U;
inline constexpr std::size_t kMaximumAutoloopCurvePointsPerEvent = 256U;

enum class AutoloopTargetKind : std::uint8_t {
    Master,
    Group,
    Fixture,
    RoleSelector,
    Count
};

enum class AutoloopEventKind : std::uint8_t {
    LegacyLook,
    PropertyBlock,
    PropertyCurve,
    Palette,
    Position,
    Attribute,
    Movement,
    Effect,
    Count
};

enum class AutoloopReferenceKind : std::uint8_t {
    Palette,
    Position,
    Attribute,
    Movement,
    Effect,
    Transition,
    Count
};

enum class AutoloopInterpolation : std::uint8_t {
    Hold,
    Linear,
    SmoothStep,
    Count
};

enum class AutoloopLaunchQuantization : std::uint8_t {
    Immediate,
    NextBeat,
    NextBar,
    NextPhrase,
    Count
};

enum class AutoloopPhaseOrigin : std::uint8_t {
    Launch,
    Track,
    Bar,
    Phrase,
    Count
};

enum class AutoloopPlaybackMode : std::uint8_t {
    Overlay,
    Replace,
    Count
};

enum class AutoloopProvenanceOrigin : std::uint8_t {
    Native,
    ContentPack,
    Generated,
    Migrated,
    Count
};

enum class AutoloopGeneratorKind : std::uint8_t {
    Circle,
    HorizontalScan,
    VerticalScan,
    Oval,
    FigureEight,
    TripleEight,
    Square,
    Sine,
    Triangle,
    Pulse,
    Count
};

struct AutoloopAssetDefinition {
    std::string id;
    std::string name;
    std::string description;
    std::vector<std::string> tags;
    std::string style;
    float energy{0.5F};
    std::string program_id;
    std::string launch_profile_id;
    std::string provenance_id;
    std::uint32_t revision{1U};
};

struct AutoloopPlacementDefinition {
    std::string id;
    std::uint16_t bank{0U};
    std::uint8_t slot{0U};
    std::string asset_id;
    std::string content_management_key;
};

struct AutoloopTargetDefinition {
    std::string id;
    AutoloopTargetKind kind{AutoloopTargetKind::Master};
    // Empty for Master. Otherwise this is a stable group/fixture ID or a
    // semantic role/category selector value, never a UI pointer/expression.
    std::string stable_ref;
    std::vector<showcore::Property> required_properties;
};

struct AutoloopLaneDefinition {
    std::string id;
    std::string target_id;
    // Higher priority wins for the same resolved fixture/property. Equal
    // priority overlap is rejected, so vector order is never precedence.
    std::uint16_t priority{0U};
};

struct AutoloopCurvePointDefinition {
    MusicalTick tick{0};
    showcore::PropertyValue value{};
};

struct AutoloopGeneratorParameters {
    float rate_start{1.0F};
    float rate_end{1.0F};
    float size_start{1.0F};
    float size_end{1.0F};
    float phase{0.0F};
    float spread{0.0F};
    float base_primary{0.5F};
    float base_secondary{0.5F};
    std::uint64_t seed{0U};
};

struct AutoloopEventDefinition {
    std::string id;
    std::string lane_id;
    AutoloopEventKind kind{AutoloopEventKind::PropertyBlock};
    // Ranges are half-open: [start_tick, end_tick).
    MusicalTick start_tick{0};
    MusicalTick end_tick{0};
    showcore::Property property{showcore::Property::Intensity};
    showcore::PropertyValue value{};
    AutoloopInterpolation interpolation{AutoloopInterpolation::Hold};
    std::vector<AutoloopCurvePointDefinition> curve_points;
    // Stable reusable asset/look/generator ID as required by kind.
    std::string reference_id;
    // Optional stable transition/easing profile ID.
    std::string transition_reference_id;
    std::uint32_t payload_version{1U};
    AutoloopGeneratorParameters generator{};
    showcore::AutoloopTransition legacy_transition{
        showcore::AutoloopTransition::Cut};
};

struct AutoloopProgramDefinition {
    std::string id;
    MusicalTick length_ticks{4 * kMusicalTicksPerQuarter};
    std::uint16_t time_signature_numerator{4U};
    std::uint16_t time_signature_denominator{4U};
    std::vector<AutoloopTargetDefinition> targets;
    std::vector<AutoloopLaneDefinition> lanes;
    std::vector<AutoloopEventDefinition> events;
};

struct AutoloopLaunchProfileDefinition {
    std::string id;
    showcore::AutoloopRepeat repeat{showcore::AutoloopRepeat::Infinite};
    AutoloopLaunchQuantization launch{AutoloopLaunchQuantization::Immediate};
    AutoloopPhaseOrigin phase_origin{AutoloopPhaseOrigin::Launch};
    AutoloopPlaybackMode mode{AutoloopPlaybackMode::Overlay};
    MusicalTick return_fade_ticks{0};
    bool track_boundary_required{false};
};

struct AutoloopProvenanceDefinition {
    std::string id;
    AutoloopProvenanceOrigin origin{AutoloopProvenanceOrigin::Native};
    std::string producer_id;
    std::string producer_version;
    std::uint64_t seed{0U};
    std::string source_bundle_id;
    std::string source_artifact_id;
    std::string source_object_key;
    std::string evidence_status;
};

struct AutoloopSourceDocument {
    std::uint32_t format_version{kAutoloopSourceFormatVersion};
    std::vector<AutoloopAssetDefinition> assets;
    std::vector<AutoloopPlacementDefinition> placements;
    std::vector<AutoloopProgramDefinition> programs;
    std::vector<AutoloopLaunchProfileDefinition> launch_profiles;
    std::vector<AutoloopProvenanceDefinition> provenance;
};

enum class AutoloopSourceIssueSeverity : std::uint8_t {
    Warning,
    Error
};

struct AutoloopSourceIssue {
    AutoloopSourceIssueSeverity severity{AutoloopSourceIssueSeverity::Error};
    std::string code;
    std::string subject;
    std::string message;
};

struct AutoloopSourceValidation {
    std::vector<AutoloopSourceIssue> issues;

    [[nodiscard]] bool ok() const noexcept;
    [[nodiscard]] std::size_t error_count() const noexcept;
    [[nodiscard]] std::size_t warning_count() const noexcept;
};

enum class AutoloopSourceIoError : std::uint8_t {
    None,
    InvalidHeader,
    UnsupportedVersion,
    InvalidRecord,
    InvalidValue,
    MissingReference,
    ValidationFailed
};

struct AutoloopSourceIoResult {
    AutoloopSourceIoError error{AutoloopSourceIoError::None};
    std::size_t line{0U};
    std::string message;

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == AutoloopSourceIoError::None;
    }
};

// Deterministic compatibility rounding: finite format-1 beats are multiplied
// by 960 in double precision and rounded to the nearest tick, with exact
// half-ticks away from zero. Invalid/out-of-range values return false.
[[nodiscard]] bool format1_beat_to_musical_tick(
    float beat,
    MusicalTick& tick) noexcept;

// Pure compatibility adapter. It neither mutates the format-1 ProjectDocument
// nor changes its persistence. Rich persistence remains a coordinated later
// format transition.
[[nodiscard]] AutoloopSourceDocument adapt_format1_autoloops(
    const ProjectDocument& project);

[[nodiscard]] AutoloopSourceValidation validate_autoloop_source(
    const AutoloopSourceDocument& source);

// Canonical order is stable-ID based, so semantically equivalent collection
// ordering produces identical bytes and digest.
void normalize_autoloop_source(AutoloopSourceDocument& source);
[[nodiscard]] std::string serialize_autoloop_source(
    const AutoloopSourceDocument& source);
[[nodiscard]] AutoloopSourceIoResult parse_autoloop_source(
    std::string_view serialized,
    AutoloopSourceDocument& source);
[[nodiscard]] std::string autoloop_source_digest(
    const AutoloopSourceDocument& source);

[[nodiscard]] const char* autoloop_target_kind_name(
    AutoloopTargetKind kind) noexcept;
[[nodiscard]] const char* autoloop_event_kind_name(
    AutoloopEventKind kind) noexcept;
[[nodiscard]] const char* autoloop_reference_kind_name(
    AutoloopReferenceKind kind) noexcept;

}  // namespace emberlights
