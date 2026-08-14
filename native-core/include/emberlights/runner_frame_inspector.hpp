#pragma once

#include "emberlights/project.hpp"
#include "emberlights/runner.hpp"
#include "showcore/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace emberlights {

inline constexpr std::size_t kRunnerFrameInspectionDefaultRows = 64U;
inline constexpr std::size_t kRunnerFrameInspectionMaximumRows = 256U;
inline constexpr std::size_t kRunnerFrameInspectionMaximumDiagnostics = 64U;
inline constexpr std::size_t kRunnerFrameInspectionMaximumTextField = 255U;

enum class RunnerFrameInspectionStatus : std::uint8_t {
    NoSnapshot,
    Current,
    Stale,
    InvalidTimestamp
};

enum class RunnerFrameDiagnosticSeverity : std::uint8_t {
    Information,
    Warning,
    Error
};

enum class RunnerFrameDiagnosticCode : std::uint8_t {
    NoSnapshot,
    StaleSnapshot,
    SnapshotTimestampAfterInspection,
    SnapshotGenerationInvalid,
    RowLimitClamped,
    RowsTruncated,
    DiagnosticsTruncated,
    MetadataTruncated,
    UnattributedNonzeroChannel,
    AttributionFixtureMissing,
    AttributionProfileMissing,
    AttributionMappingMissing,
    AttributionCapabilityMissing,
    AttributionPatchMismatch,
    ReferenceUniverseInvalid,
    ReferenceChannelInvalid,
    DifferenceRowsTruncated
};

struct RunnerFrameDiagnostic {
    RunnerFrameDiagnosticSeverity severity{
        RunnerFrameDiagnosticSeverity::Information};
    RunnerFrameDiagnosticCode code{RunnerFrameDiagnosticCode::NoSnapshot};
    std::uint8_t universe{0U};
    std::uint16_t channel{0U};
    std::string message;
};

struct RunnerFrameInspectionOptions {
    // Callers pass the same monotonic clock domain used by the Runner. Keeping
    // time explicit makes inspection and its tests deterministic.
    std::uint64_t inspected_at_ms{0U};
    std::uint64_t stale_after_ms{2000U};
    std::size_t max_rows{kRunnerFrameInspectionDefaultRows};
};

struct RunnerFrameChannelInspection {
    std::uint8_t universe{0U};
    std::uint16_t channel{0U};
    std::uint8_t pre_blackout_value{0U};
    std::uint8_t routed_value{0U};
    bool changed_by_global_blackout{false};

    bool renderer_attribution_present{false};
    bool renderer_attribution_resolved{false};
    bool metadata_truncated{false};
    std::uint16_t runtime_fixture_id{
        showcore::kInvalidRenderAttributionIndex};
    std::uint16_t mapping_index{showcore::kInvalidRenderAttributionIndex};
    std::uint16_t capability_index{showcore::kInvalidRenderAttributionIndex};
    bool fine_channel{false};

    std::string fixture_id;
    std::string fixture_name;
    std::uint8_t fixture_universe{0U};
    std::uint16_t fixture_address{0U};
    std::string profile_id;
    std::string profile_name;
    std::string profile_manufacturer;
    std::string profile_model;
    std::string profile_mode;
    std::string profile_revision;
    showcore::FixtureProfileSource profile_source{
        showcore::FixtureProfileSource::Unknown};

    std::uint16_t mapped_channel{0U};
    std::string mapping_owner;
    showcore::Property mapping_property{showcore::Property::Count};
    showcore::Property rendered_property{showcore::Property::Count};
    showcore::ChannelEncoding encoding{showcore::ChannelEncoding::Linear8};
    showcore::RenderValueOrigin renderer_origin{
        showcore::RenderValueOrigin::None};
    showcore::LayerId winning_layer{showcore::LayerId::Count};
    showcore::ValueMode value_mode{showcore::ValueMode::Release};
    std::string capability_id;
    std::string capability_name;
};

struct RunnerFrameInspection {
    RunnerFrameInspectionStatus status{RunnerFrameInspectionStatus::NoSnapshot};
    bool snapshot_available{false};
    bool stale{false};
    bool rows_truncated{false};
    bool diagnostics_truncated{false};
    std::uint64_t inspected_at_ms{0U};
    std::uint64_t snapshot_age_ms{0U};
    std::uint64_t generation{0U};
    std::uint8_t sequence{0U};
    std::uint64_t rendered_at_ms{0U};
    bool blackout_applied{false};

    std::string pre_blackout_sha256;
    std::string routed_sha256;
    std::array<std::string, showcore::kV1UniverseCount>
        pre_blackout_universe_sha256{};
    std::array<std::string, showcore::kV1UniverseCount>
        routed_universe_sha256{};
    std::array<std::size_t, showcore::kV1UniverseCount>
        pre_blackout_nonzero{};
    std::array<std::size_t, showcore::kV1UniverseCount> routed_nonzero{};
    std::array<std::uint16_t, showcore::kV1UniverseCount>
        pre_blackout_first_nonzero{};
    std::array<std::uint16_t, showcore::kV1UniverseCount>
        pre_blackout_last_nonzero{};
    std::array<std::uint16_t, showcore::kV1UniverseCount>
        routed_first_nonzero{};
    std::array<std::uint16_t, showcore::kV1UniverseCount>
        routed_last_nonzero{};
    std::size_t union_nonzero_channels{0U};

    std::array<RunnerOutputRouteResult, kRunnerOutputRouteCount> routes{};
    std::vector<RunnerFrameChannelInspection> rows;
    std::vector<RunnerFrameDiagnostic> diagnostics;
};

enum class RunnerFrameDifferenceKind : std::uint8_t {
    MissingExpectedValue,
    UnexpectedActualValue,
    ValueMismatch
};

enum class RunnerFrameDifferenceCause : std::uint8_t {
    GlobalBlackout,
    Safety,
    RendererConflict,
    ProfileDefault,
    ProfileConstant,
    PropertyWinner,
    CapabilityWinner,
    Unattributed,
    InvalidAttribution
};

struct RunnerFrameDifference {
    std::uint16_t channel{0U};
    std::uint8_t expected{0U};
    std::uint8_t actual{0U};
    RunnerFrameDifferenceKind kind{
        RunnerFrameDifferenceKind::ValueMismatch};
    RunnerFrameDifferenceCause cause{RunnerFrameDifferenceCause::Unattributed};
    RunnerFrameChannelInspection context{};
};

struct RunnerFrameStageComparison {
    bool exact{false};
    std::string actual_sha256;
    std::size_t differing_channels{0U};
    bool rows_truncated{false};
    std::vector<RunnerFrameDifference> differences;
};

struct RunnerRawReferenceComparison {
    bool valid_reference{false};
    bool snapshot_available{false};
    bool stale{false};
    std::uint8_t universe{0U};
    std::string reference_sha256;
    RunnerFrameStageComparison pre_blackout{};
    RunnerFrameStageComparison routed{};
    bool diagnostics_truncated{false};
    std::vector<RunnerFrameDiagnostic> diagnostics;

    [[nodiscard]] bool exact_pre_blackout() const noexcept {
        return valid_reference && snapshot_available && pre_blackout.exact;
    }

    [[nodiscard]] bool exact_routed() const noexcept {
        return valid_reference && snapshot_available && routed.exact;
    }
};

// These hashes cover exactly 512 universe bytes or the two universes in U1,
// U2 order. They are directly comparable with the raw Hardware Test frame.
[[nodiscard]] std::string runner_dmx_universe_sha256(
    const showcore::DmxUniverse& universe);
[[nodiscard]] std::string runner_dmx_frames_sha256(
    const showcore::DmxFrames& frames);

[[nodiscard]] RunnerFrameInspection inspect_runner_frame(
    const ProjectDocument& project,
    const RunnerOutputSnapshot* snapshot,
    const RunnerFrameInspectionOptions& options);

[[nodiscard]] RunnerRawReferenceComparison compare_runner_frame_to_raw(
    const ProjectDocument& project,
    const RunnerOutputSnapshot* snapshot,
    std::uint8_t universe,
    const showcore::DmxUniverse& reference,
    const RunnerFrameInspectionOptions& options);

[[nodiscard]] RunnerRawReferenceComparison compare_runner_frame_to_one_hot(
    const ProjectDocument& project,
    const RunnerOutputSnapshot* snapshot,
    std::uint8_t universe,
    std::uint16_t channel,
    std::uint8_t value,
    const RunnerFrameInspectionOptions& options);

[[nodiscard]] std::string format_runner_frame_inspection(
    const RunnerFrameInspection& inspection);
[[nodiscard]] std::string format_runner_raw_reference_comparison(
    const RunnerRawReferenceComparison& comparison);

[[nodiscard]] std::string_view runner_frame_inspection_status_name(
    RunnerFrameInspectionStatus status) noexcept;
[[nodiscard]] std::string_view runner_frame_diagnostic_code_name(
    RunnerFrameDiagnosticCode code) noexcept;
[[nodiscard]] std::string_view runner_frame_difference_kind_name(
    RunnerFrameDifferenceKind kind) noexcept;
[[nodiscard]] std::string_view runner_frame_difference_cause_name(
    RunnerFrameDifferenceCause cause) noexcept;

}  // namespace emberlights
