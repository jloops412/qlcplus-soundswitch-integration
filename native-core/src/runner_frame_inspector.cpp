#include "emberlights/runner_frame_inspector.hpp"

#include "emberlights/file_identity.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <locale>
#include <sstream>
#include <string>
#include <string_view>

namespace emberlights {
namespace {

using DiagnosticList = std::vector<RunnerFrameDiagnostic>;

void add_diagnostic(
    DiagnosticList& diagnostics,
    bool& truncated,
    RunnerFrameDiagnosticSeverity severity,
    RunnerFrameDiagnosticCode code,
    std::string message,
    std::uint8_t universe = 0U,
    std::uint16_t channel = 0U) {
    if (diagnostics.size() >= kRunnerFrameInspectionMaximumDiagnostics) {
        truncated = true;
        return;
    }
    diagnostics.push_back({
        severity,
        code,
        universe,
        channel,
        std::move(message)});
}

[[nodiscard]] std::string bounded_text(
    std::string_view source,
    bool& truncated) {
    if (source.size() <= kRunnerFrameInspectionMaximumTextField) {
        return std::string(source);
    }
    truncated = true;
    return std::string(source.substr(0U, kRunnerFrameInspectionMaximumTextField));
}

[[nodiscard]] const FixtureProfileDefinition* find_profile(
    const ProjectDocument& project,
    std::string_view profile_id) noexcept {
    const auto found = std::find_if(
        project.fixture_profiles.begin(),
        project.fixture_profiles.end(),
        [profile_id](const auto& profile) { return profile.id == profile_id; });
    return found == project.fixture_profiles.end() ? nullptr : &*found;
}

[[nodiscard]] std::string_view diagnostic_severity_name(
    RunnerFrameDiagnosticSeverity severity) noexcept {
    switch (severity) {
    case RunnerFrameDiagnosticSeverity::Information: return "information";
    case RunnerFrameDiagnosticSeverity::Warning: return "warning";
    case RunnerFrameDiagnosticSeverity::Error: return "error";
    }
    return "invalid";
}

[[nodiscard]] std::string_view layer_name(showcore::LayerId layer) noexcept {
    switch (layer) {
    case showcore::LayerId::Idle: return "Idle";
    case showcore::LayerId::Autonomous: return "Autonomous";
    case showcore::LayerId::TrackScript: return "TrackScript";
    case showcore::LayerId::ManualAutoloop: return "ManualAutoloop";
    case showcore::LayerId::EventMoment: return "EventMoment";
    case showcore::LayerId::ManualOverride: return "ManualOverride";
    case showcore::LayerId::Emergency: return "Emergency";
    case showcore::LayerId::Safety: return "Safety";
    case showcore::LayerId::Count: return "None";
    }
    return "Invalid";
}

[[nodiscard]] std::string_view value_mode_name(
    showcore::ValueMode mode) noexcept {
    switch (mode) {
    case showcore::ValueMode::Release: return "Release";
    case showcore::ValueMode::Set: return "Set";
    case showcore::ValueMode::ForceZero: return "ForceZero";
    }
    return "Invalid";
}

[[nodiscard]] std::string_view property_text(
    showcore::Property property) noexcept {
    constexpr std::array<std::string_view, showcore::kPropertyCount> names{{
        "intensity", "red", "green", "blue", "white", "amber", "uv",
        "cyan", "magenta", "yellow", "lime", "indigo", "pan", "tilt",
        "panRotate", "tiltRotate", "panTiltSpeed", "strobe", "shutter",
        "colorWheel", "gobo", "goboRotation", "prism", "prismRotation",
        "focus", "zoom", "iris", "frost", "animation",
        "animationRotation", "effect", "effectSpeed", "fan", "fog",
        "haze", "laser", "spark", "custom1", "custom2", "custom3",
        "custom4", "custom5", "custom6", "custom7", "custom8", "custom9",
        "custom10", "custom11", "custom12", "custom13", "custom14",
        "custom15", "custom16"}};
    const auto index = static_cast<std::size_t>(property);
    return index < names.size() ? names[index] : "constant";
}

[[nodiscard]] std::string_view encoding_name(
    showcore::ChannelEncoding encoding) noexcept {
    switch (encoding) {
    case showcore::ChannelEncoding::Linear8: return "linear8";
    case showcore::ChannelEncoding::Linear16: return "linear16";
    case showcore::ChannelEncoding::Discrete8: return "discrete8";
    case showcore::ChannelEncoding::Ranged8: return "ranged8";
    case showcore::ChannelEncoding::Constant8: return "constant8";
    }
    return "invalid";
}

[[nodiscard]] std::string_view renderer_origin_name(
    showcore::RenderValueOrigin origin) noexcept {
    switch (origin) {
    case showcore::RenderValueOrigin::None: return "None";
    case showcore::RenderValueOrigin::Default: return "Default";
    case showcore::RenderValueOrigin::Constant: return "Constant";
    case showcore::RenderValueOrigin::Property: return "Property";
    case showcore::RenderValueOrigin::Capability: return "Capability";
    case showcore::RenderValueOrigin::Conflict: return "Conflict";
    case showcore::RenderValueOrigin::Safety: return "Safety";
    }
    return "Invalid";
}

[[nodiscard]] std::string_view profile_source_name(
    showcore::FixtureProfileSource source) noexcept {
    switch (source) {
    case showcore::FixtureProfileSource::Unknown: return "Unknown";
    case showcore::FixtureProfileSource::BuiltIn: return "BuiltIn";
    case showcore::FixtureProfileSource::OpenFixtureLibrary:
        return "OpenFixtureLibrary";
    case showcore::FixtureProfileSource::QlcPlus: return "QlcPlus";
    case showcore::FixtureProfileSource::Local: return "Local";
    case showcore::FixtureProfileSource::Migrated: return "Migrated";
    }
    return "Invalid";
}

[[nodiscard]] std::string_view backend_name(
    showcore::OutputBackendKind kind) noexcept {
    return showcore::output_backend_descriptor(kind).name;
}

[[nodiscard]] std::string escaped(std::string_view value) {
    constexpr std::string_view hexadecimal = "0123456789abcdef";
    std::string result;
    result.reserve(value.size());
    for (const auto character : value) {
        const auto byte = static_cast<unsigned char>(character);
        switch (character) {
        case '\\': result.append("\\\\"); break;
        case '\t': result.append("\\t"); break;
        case '\r': result.append("\\r"); break;
        case '\n': result.append("\\n"); break;
        default:
            if (byte < 0x20U || byte == 0x7FU) {
                result.append("\\x");
                result.push_back(hexadecimal[byte >> 4U]);
                result.push_back(hexadecimal[byte & 0x0FU]);
            } else {
                result.push_back(character);
            }
            break;
        }
    }
    return result;
}

[[nodiscard]] std::size_t bounded_row_limit(
    std::size_t requested,
    DiagnosticList& diagnostics,
    bool& diagnostics_truncated) {
    if (requested <= kRunnerFrameInspectionMaximumRows) {
        return requested;
    }
    add_diagnostic(
        diagnostics,
        diagnostics_truncated,
        RunnerFrameDiagnosticSeverity::Warning,
        RunnerFrameDiagnosticCode::RowLimitClamped,
        "The requested row limit exceeded the bounded inspector maximum and was clamped.");
    return kRunnerFrameInspectionMaximumRows;
}

struct SnapshotFreshness {
    RunnerFrameInspectionStatus status{RunnerFrameInspectionStatus::NoSnapshot};
    bool stale{false};
    std::uint64_t age_ms{0U};
};

[[nodiscard]] SnapshotFreshness assess_snapshot(
    const RunnerOutputSnapshot* snapshot,
    const RunnerFrameInspectionOptions& options,
    DiagnosticList& diagnostics,
    bool& diagnostics_truncated) {
    if (snapshot == nullptr) {
        add_diagnostic(
            diagnostics,
            diagnostics_truncated,
            RunnerFrameDiagnosticSeverity::Warning,
            RunnerFrameDiagnosticCode::NoSnapshot,
            "No routed Runner frame snapshot is available yet.");
        return {};
    }

    SnapshotFreshness result;
    result.status = RunnerFrameInspectionStatus::Current;
    if (snapshot->generation == 0U) {
        add_diagnostic(
            diagnostics,
            diagnostics_truncated,
            RunnerFrameDiagnosticSeverity::Error,
            RunnerFrameDiagnosticCode::SnapshotGenerationInvalid,
            "The routed snapshot has generation zero and cannot identify an active package.");
    }
    if (options.inspected_at_ms < snapshot->rendered_at_ms) {
        result.status = RunnerFrameInspectionStatus::InvalidTimestamp;
        add_diagnostic(
            diagnostics,
            diagnostics_truncated,
            RunnerFrameDiagnosticSeverity::Error,
            RunnerFrameDiagnosticCode::SnapshotTimestampAfterInspection,
            "The snapshot render timestamp is later than the supplied inspection timestamp.");
        return result;
    }
    result.age_ms = options.inspected_at_ms - snapshot->rendered_at_ms;
    if (result.age_ms > options.stale_after_ms) {
        result.status = RunnerFrameInspectionStatus::Stale;
        result.stale = true;
        add_diagnostic(
            diagnostics,
            diagnostics_truncated,
            RunnerFrameDiagnosticSeverity::Warning,
            RunnerFrameDiagnosticCode::StaleSnapshot,
            "The latest routed Runner frame is older than the requested freshness boundary.");
    }
    return result;
}

void add_channel_diagnostic(
    DiagnosticList* diagnostics,
    bool* diagnostics_truncated,
    RunnerFrameDiagnosticSeverity severity,
    RunnerFrameDiagnosticCode code,
    std::string message,
    std::uint8_t universe,
    std::uint16_t channel) {
    if (diagnostics == nullptr || diagnostics_truncated == nullptr) {
        return;
    }
    add_diagnostic(
        *diagnostics,
        *diagnostics_truncated,
        severity,
        code,
        std::move(message),
        universe,
        channel);
}

[[nodiscard]] RunnerFrameChannelInspection inspect_channel(
    const ProjectDocument& project,
    const RunnerOutputSnapshot& snapshot,
    std::size_t universe_index,
    std::size_t slot,
    DiagnosticList* diagnostics,
    bool* diagnostics_truncated) {
    RunnerFrameChannelInspection row;
    row.universe = static_cast<std::uint8_t>(universe_index + 1U);
    row.channel = static_cast<std::uint16_t>(slot + 1U);
    row.pre_blackout_value =
        snapshot.pre_blackout_frames.universes[universe_index][slot];
    row.routed_value = snapshot.routed_frames.universes[universe_index][slot];
    row.changed_by_global_blackout = snapshot.blackout_applied &&
        row.pre_blackout_value != row.routed_value;

    const auto& attribution = snapshot.attribution.universes[universe_index][slot];
    row.runtime_fixture_id = attribution.fixture_id;
    row.mapping_index = attribution.mapping_index;
    row.capability_index = attribution.capability_index;
    row.fine_channel = attribution.fine_channel;
    row.rendered_property = attribution.property;
    row.encoding = attribution.encoding;
    row.renderer_origin = attribution.origin;
    row.winning_layer = attribution.winning_layer;
    row.value_mode = attribution.value_mode;
    row.renderer_attribution_present =
        attribution.origin != showcore::RenderValueOrigin::None ||
        attribution.fixture_id != showcore::kInvalidRenderAttributionIndex ||
        attribution.mapping_index != showcore::kInvalidRenderAttributionIndex;

    if (!row.renderer_attribution_present) {
        if (row.pre_blackout_value != 0U || row.routed_value != 0U) {
            add_channel_diagnostic(
                diagnostics,
                diagnostics_truncated,
                RunnerFrameDiagnosticSeverity::Warning,
                RunnerFrameDiagnosticCode::UnattributedNonzeroChannel,
                "A nonzero frame channel has no renderer attribution.",
                row.universe,
                row.channel);
        }
        return row;
    }

    if (attribution.fixture_id >= project.fixtures.size()) {
        add_channel_diagnostic(
            diagnostics,
            diagnostics_truncated,
            RunnerFrameDiagnosticSeverity::Error,
            RunnerFrameDiagnosticCode::AttributionFixtureMissing,
            "The renderer fixture index is not present in this project snapshot.",
            row.universe,
            row.channel);
        return row;
    }

    const auto& fixture = project.fixtures[attribution.fixture_id];
    row.fixture_id = bounded_text(fixture.id, row.metadata_truncated);
    row.fixture_name = bounded_text(fixture.name, row.metadata_truncated);
    row.fixture_universe = fixture.universe;
    row.fixture_address = fixture.address;
    row.profile_id = bounded_text(fixture.profile_id, row.metadata_truncated);

    const auto* profile = find_profile(project, fixture.profile_id);
    if (profile == nullptr) {
        add_channel_diagnostic(
            diagnostics,
            diagnostics_truncated,
            RunnerFrameDiagnosticSeverity::Error,
            RunnerFrameDiagnosticCode::AttributionProfileMissing,
            "The attributed fixture's profile is absent from this project snapshot.",
            row.universe,
            row.channel);
        return row;
    }
    row.profile_name = bounded_text(profile->name, row.metadata_truncated);
    row.profile_manufacturer =
        bounded_text(profile->manufacturer, row.metadata_truncated);
    row.profile_model = bounded_text(profile->model, row.metadata_truncated);
    row.profile_mode = bounded_text(profile->mode, row.metadata_truncated);
    row.profile_revision =
        bounded_text(profile->source_revision, row.metadata_truncated);
    row.profile_source = profile->source;

    if (attribution.mapping_index >= profile->channels.size()) {
        add_channel_diagnostic(
            diagnostics,
            diagnostics_truncated,
            RunnerFrameDiagnosticSeverity::Error,
            RunnerFrameDiagnosticCode::AttributionMappingMissing,
            "The renderer mapping index is absent from the attributed profile revision.",
            row.universe,
            row.channel);
        return row;
    }

    const auto& mapping = profile->channels[attribution.mapping_index];
    row.mapping_owner = bounded_text(mapping.owner, row.metadata_truncated);
    row.mapping_property = mapping.property;
    row.encoding = mapping.encoding;
    const auto mapped_offset = attribution.fine_channel
        ? mapping.fine_offset
        : static_cast<std::int16_t>(mapping.coarse_offset);
    if (mapped_offset >= 0) {
        const auto absolute = static_cast<std::uint32_t>(fixture.address) +
            static_cast<std::uint32_t>(mapped_offset);
        if (absolute <= showcore::kUniverseSlots) {
            row.mapped_channel = static_cast<std::uint16_t>(absolute);
        }
    }

    if (attribution.capability_index !=
        showcore::kInvalidRenderAttributionIndex) {
        if (attribution.capability_index >= mapping.capabilities.size()) {
            add_channel_diagnostic(
                diagnostics,
                diagnostics_truncated,
                RunnerFrameDiagnosticSeverity::Error,
                RunnerFrameDiagnosticCode::AttributionCapabilityMissing,
                "The renderer capability index is absent from the attributed profile mapping.",
                row.universe,
                row.channel);
            return row;
        }
        const auto& capability =
            mapping.capabilities[attribution.capability_index];
        row.capability_id = bounded_text(capability.id, row.metadata_truncated);
        row.capability_name = bounded_text(capability.name, row.metadata_truncated);
    }

    row.renderer_attribution_resolved = true;
    if (fixture.universe != row.universe || row.mapped_channel != row.channel) {
        row.renderer_attribution_resolved = false;
        add_channel_diagnostic(
            diagnostics,
            diagnostics_truncated,
            RunnerFrameDiagnosticSeverity::Error,
            RunnerFrameDiagnosticCode::AttributionPatchMismatch,
            "The attributed project universe/address/mapping does not own this frame channel.",
            row.universe,
            row.channel);
    }
    if (row.metadata_truncated) {
        add_channel_diagnostic(
            diagnostics,
            diagnostics_truncated,
            RunnerFrameDiagnosticSeverity::Warning,
            RunnerFrameDiagnosticCode::MetadataTruncated,
            "Project metadata exceeded the inspector's bounded text field limit.",
            row.universe,
            row.channel);
    }
    return row;
}

void summarize_universe(
    const showcore::DmxUniverse& universe,
    std::size_t& nonzero,
    std::uint16_t& first,
    std::uint16_t& last) noexcept {
    nonzero = 0U;
    first = 0U;
    last = 0U;
    for (std::size_t slot = 0U; slot < universe.size(); ++slot) {
        if (universe[slot] == 0U) {
            continue;
        }
        ++nonzero;
        const auto channel = static_cast<std::uint16_t>(slot + 1U);
        if (first == 0U) {
            first = channel;
        }
        last = channel;
    }
}

[[nodiscard]] RunnerFrameDifferenceKind difference_kind(
    std::uint8_t expected,
    std::uint8_t actual) noexcept {
    if (expected != 0U && actual == 0U) {
        return RunnerFrameDifferenceKind::MissingExpectedValue;
    }
    if (expected == 0U && actual != 0U) {
        return RunnerFrameDifferenceKind::UnexpectedActualValue;
    }
    return RunnerFrameDifferenceKind::ValueMismatch;
}

[[nodiscard]] RunnerFrameDifferenceCause difference_cause(
    const RunnerFrameChannelInspection& context,
    bool routed_stage) noexcept {
    if (routed_stage && context.changed_by_global_blackout) {
        return RunnerFrameDifferenceCause::GlobalBlackout;
    }
    if (context.renderer_attribution_present &&
        !context.renderer_attribution_resolved) {
        return RunnerFrameDifferenceCause::InvalidAttribution;
    }
    switch (context.renderer_origin) {
    case showcore::RenderValueOrigin::Safety:
        return RunnerFrameDifferenceCause::Safety;
    case showcore::RenderValueOrigin::Conflict:
        return RunnerFrameDifferenceCause::RendererConflict;
    case showcore::RenderValueOrigin::Default:
        return RunnerFrameDifferenceCause::ProfileDefault;
    case showcore::RenderValueOrigin::Constant:
        return RunnerFrameDifferenceCause::ProfileConstant;
    case showcore::RenderValueOrigin::Property:
        return RunnerFrameDifferenceCause::PropertyWinner;
    case showcore::RenderValueOrigin::Capability:
        return RunnerFrameDifferenceCause::CapabilityWinner;
    case showcore::RenderValueOrigin::None:
        return RunnerFrameDifferenceCause::Unattributed;
    }
    return RunnerFrameDifferenceCause::InvalidAttribution;
}

[[nodiscard]] RunnerFrameStageComparison compare_stage(
    const ProjectDocument& project,
    const RunnerOutputSnapshot& snapshot,
    std::size_t universe_index,
    const showcore::DmxUniverse& reference,
    bool routed_stage,
    std::size_t row_limit) {
    RunnerFrameStageComparison result;
    const auto& actual = routed_stage
        ? snapshot.routed_frames.universes[universe_index]
        : snapshot.pre_blackout_frames.universes[universe_index];
    result.actual_sha256 = runner_dmx_universe_sha256(actual);
    result.differences.reserve(std::min(row_limit, actual.size()));
    for (std::size_t slot = 0U; slot < actual.size(); ++slot) {
        if (reference[slot] == actual[slot]) {
            continue;
        }
        ++result.differing_channels;
        if (result.differences.size() >= row_limit) {
            continue;
        }
        auto context = inspect_channel(
            project, snapshot, universe_index, slot, nullptr, nullptr);
        result.differences.push_back({
            static_cast<std::uint16_t>(slot + 1U),
            reference[slot],
            actual[slot],
            difference_kind(reference[slot], actual[slot]),
            difference_cause(context, routed_stage),
            std::move(context)});
    }
    result.exact = result.differing_channels == 0U;
    result.rows_truncated = result.differing_channels > result.differences.size();
    return result;
}

void append_channel_context(
    std::ostringstream& output,
    const RunnerFrameChannelInspection& row) {
    output << "\tfixtureId=" << escaped(row.fixture_id)
           << "\tfixtureName=" << escaped(row.fixture_name)
           << "\tfixtureAddress=" << row.fixture_address
           << "\tprofileId=" << escaped(row.profile_id)
           << "\tprofileMode=" << escaped(row.profile_mode)
           << "\tprofileRevision=" << escaped(row.profile_revision)
           << "\tmappingIndex=" << row.mapping_index
           << "\tmappingProperty=" << property_text(row.mapping_property)
           << "\trenderedProperty=" << property_text(row.rendered_property)
           << "\torigin=" << renderer_origin_name(row.renderer_origin)
           << "\tlayer=" << layer_name(row.winning_layer)
           << "\tvalueMode=" << value_mode_name(row.value_mode);
}

}  // namespace

std::string runner_dmx_universe_sha256(
    const showcore::DmxUniverse& universe) {
    return sha256_bytes(universe);
}

std::string runner_dmx_frames_sha256(const showcore::DmxFrames& frames) {
    std::array<std::uint8_t,
               showcore::kV1UniverseCount * showcore::kUniverseSlots> bytes{};
    std::size_t offset = 0U;
    for (const auto& universe : frames.universes) {
        std::copy(
            universe.begin(), universe.end(),
            bytes.begin() + static_cast<std::ptrdiff_t>(offset));
        offset += universe.size();
    }
    return sha256_bytes(bytes);
}

RunnerFrameInspection inspect_runner_frame(
    const ProjectDocument& project,
    const RunnerOutputSnapshot* snapshot,
    const RunnerFrameInspectionOptions& options) {
    RunnerFrameInspection result;
    result.inspected_at_ms = options.inspected_at_ms;
    const auto freshness = assess_snapshot(
        snapshot,
        options,
        result.diagnostics,
        result.diagnostics_truncated);
    result.status = freshness.status;
    result.stale = freshness.stale;
    result.snapshot_age_ms = freshness.age_ms;
    if (snapshot == nullptr) {
        return result;
    }

    result.snapshot_available = true;
    result.generation = snapshot->generation;
    result.sequence = snapshot->sequence;
    result.rendered_at_ms = snapshot->rendered_at_ms;
    result.blackout_applied = snapshot->blackout_applied;
    result.routes = snapshot->routes;
    result.pre_blackout_sha256 =
        runner_dmx_frames_sha256(snapshot->pre_blackout_frames);
    result.routed_sha256 = runner_dmx_frames_sha256(snapshot->routed_frames);

    for (std::size_t universe = 0U;
         universe < showcore::kV1UniverseCount;
         ++universe) {
        result.pre_blackout_universe_sha256[universe] =
            runner_dmx_universe_sha256(
                snapshot->pre_blackout_frames.universes[universe]);
        result.routed_universe_sha256[universe] =
            runner_dmx_universe_sha256(
                snapshot->routed_frames.universes[universe]);
        summarize_universe(
            snapshot->pre_blackout_frames.universes[universe],
            result.pre_blackout_nonzero[universe],
            result.pre_blackout_first_nonzero[universe],
            result.pre_blackout_last_nonzero[universe]);
        summarize_universe(
            snapshot->routed_frames.universes[universe],
            result.routed_nonzero[universe],
            result.routed_first_nonzero[universe],
            result.routed_last_nonzero[universe]);
    }

    const auto row_limit = bounded_row_limit(
        options.max_rows,
        result.diagnostics,
        result.diagnostics_truncated);
    result.rows.reserve(row_limit);
    for (std::size_t universe = 0U;
         universe < showcore::kV1UniverseCount;
         ++universe) {
        for (std::size_t slot = 0U; slot < showcore::kUniverseSlots; ++slot) {
            const auto pre =
                snapshot->pre_blackout_frames.universes[universe][slot];
            const auto routed = snapshot->routed_frames.universes[universe][slot];
            if (pre == 0U && routed == 0U) {
                continue;
            }
            ++result.union_nonzero_channels;
            if (result.rows.size() >= row_limit) {
                continue;
            }
            result.rows.push_back(inspect_channel(
                project,
                *snapshot,
                universe,
                slot,
                &result.diagnostics,
                &result.diagnostics_truncated));
        }
    }
    result.rows_truncated = result.union_nonzero_channels > result.rows.size();
    if (result.rows_truncated) {
        add_diagnostic(
            result.diagnostics,
            result.diagnostics_truncated,
            RunnerFrameDiagnosticSeverity::Information,
            RunnerFrameDiagnosticCode::RowsTruncated,
            "The nonzero channel list was truncated at the requested bounded row limit.");
    }
    return result;
}

RunnerRawReferenceComparison compare_runner_frame_to_raw(
    const ProjectDocument& project,
    const RunnerOutputSnapshot* snapshot,
    std::uint8_t universe,
    const showcore::DmxUniverse& reference,
    const RunnerFrameInspectionOptions& options) {
    RunnerRawReferenceComparison result;
    result.universe = universe;
    result.reference_sha256 = runner_dmx_universe_sha256(reference);
    if (universe == 0U || universe > showcore::kV1UniverseCount) {
        add_diagnostic(
            result.diagnostics,
            result.diagnostics_truncated,
            RunnerFrameDiagnosticSeverity::Error,
            RunnerFrameDiagnosticCode::ReferenceUniverseInvalid,
            "The raw reference universe is outside EmberLights V1 universes 1-2.",
            universe);
        return result;
    }
    result.valid_reference = true;

    const auto freshness = assess_snapshot(
        snapshot,
        options,
        result.diagnostics,
        result.diagnostics_truncated);
    result.stale = freshness.stale;
    if (snapshot == nullptr) {
        return result;
    }
    result.snapshot_available = true;

    const auto row_limit = bounded_row_limit(
        options.max_rows,
        result.diagnostics,
        result.diagnostics_truncated);
    const auto universe_index = static_cast<std::size_t>(universe - 1U);
    result.pre_blackout = compare_stage(
        project, *snapshot, universe_index, reference, false, row_limit);
    result.routed = compare_stage(
        project, *snapshot, universe_index, reference, true, row_limit);
    if (result.pre_blackout.rows_truncated) {
        add_diagnostic(
            result.diagnostics,
            result.diagnostics_truncated,
            RunnerFrameDiagnosticSeverity::Information,
            RunnerFrameDiagnosticCode::DifferenceRowsTruncated,
            "Pre-blackout difference rows were truncated at the bounded row limit.",
            universe);
    }
    if (result.routed.rows_truncated) {
        add_diagnostic(
            result.diagnostics,
            result.diagnostics_truncated,
            RunnerFrameDiagnosticSeverity::Information,
            RunnerFrameDiagnosticCode::DifferenceRowsTruncated,
            "Routed difference rows were truncated at the bounded row limit.",
            universe);
    }
    return result;
}

RunnerRawReferenceComparison compare_runner_frame_to_one_hot(
    const ProjectDocument& project,
    const RunnerOutputSnapshot* snapshot,
    std::uint8_t universe,
    std::uint16_t channel,
    std::uint8_t value,
    const RunnerFrameInspectionOptions& options) {
    if (channel == 0U || channel > showcore::kUniverseSlots) {
        RunnerRawReferenceComparison result;
        result.universe = universe;
        if (universe == 0U || universe > showcore::kV1UniverseCount) {
            add_diagnostic(
                result.diagnostics,
                result.diagnostics_truncated,
                RunnerFrameDiagnosticSeverity::Error,
                RunnerFrameDiagnosticCode::ReferenceUniverseInvalid,
                "The raw reference universe is outside EmberLights V1 universes 1-2.",
                universe);
        }
        add_diagnostic(
            result.diagnostics,
            result.diagnostics_truncated,
            RunnerFrameDiagnosticSeverity::Error,
            RunnerFrameDiagnosticCode::ReferenceChannelInvalid,
            "The one-hot raw reference channel is outside DMX channels 1-512.",
            universe,
            channel);
        return result;
    }
    showcore::DmxUniverse reference{};
    reference[static_cast<std::size_t>(channel - 1U)] = value;
    return compare_runner_frame_to_raw(
        project, snapshot, universe, reference, options);
}

std::string format_runner_frame_inspection(
    const RunnerFrameInspection& inspection) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << "EMBERLIGHTS_RUNNER_FRAME_INSPECTION_V1\n"
           << "STATUS\t" << runner_frame_inspection_status_name(inspection.status)
           << "\tsnapshotAvailable=" << inspection.snapshot_available
           << "\tstale=" << inspection.stale
           << "\tinspectedAtMs=" << inspection.inspected_at_ms
           << "\tsnapshotAgeMs=" << inspection.snapshot_age_ms << '\n';
    if (inspection.snapshot_available) {
        output << "SNAPSHOT\tgeneration=" << inspection.generation
               << "\tsequence=" << static_cast<unsigned int>(inspection.sequence)
               << "\trenderedAtMs=" << inspection.rendered_at_ms
               << "\tblackoutApplied=" << inspection.blackout_applied << '\n'
               << "HASH\tstage=preBlackout\tsha256="
               << inspection.pre_blackout_sha256 << '\n'
               << "HASH\tstage=routed\tsha256="
               << inspection.routed_sha256 << '\n';
        for (std::size_t universe = 0U;
             universe < showcore::kV1UniverseCount;
             ++universe) {
            output << "UNIVERSE\tU" << universe + 1U
                   << "\tpreSha256="
                   << inspection.pre_blackout_universe_sha256[universe]
                   << "\troutedSha256="
                   << inspection.routed_universe_sha256[universe]
                   << "\tpreNonzero="
                   << inspection.pre_blackout_nonzero[universe]
                   << "\tpreFirst="
                   << inspection.pre_blackout_first_nonzero[universe]
                   << "\tpreLast="
                   << inspection.pre_blackout_last_nonzero[universe]
                   << "\troutedNonzero="
                   << inspection.routed_nonzero[universe]
                   << "\troutedFirst="
                   << inspection.routed_first_nonzero[universe]
                   << "\troutedLast="
                   << inspection.routed_last_nonzero[universe] << '\n';
        }
        for (const auto& route : inspection.routes) {
            output << "ROUTE\tbackend=" << backend_name(route.kind)
                   << "\tconfigured=" << route.configured
                   << "\tfirstSourceUniverse="
                   << static_cast<unsigned int>(route.first_source_universe)
                   << "\tsourceUniverseCount="
                   << static_cast<unsigned int>(route.source_universe_count)
                   << "\tattemptedFrames="
                   << static_cast<unsigned int>(route.attempted_frames)
                   << "\tacceptedFrames="
                   << static_cast<unsigned int>(route.accepted_frames)
                   << "\tlastError=" << route.last_error << '\n';
        }
        for (const auto& row : inspection.rows) {
            output << "CHANNEL\tU" << static_cast<unsigned int>(row.universe)
                   << "\tCH" << row.channel
                   << "\tpre=" << static_cast<unsigned int>(row.pre_blackout_value)
                   << "\trouted=" << static_cast<unsigned int>(row.routed_value)
                   << "\tglobalBlackoutChanged="
                   << row.changed_by_global_blackout
                   << "\tattributionResolved="
                   << row.renderer_attribution_resolved;
            append_channel_context(output, row);
            output << "\tprofileName=" << escaped(row.profile_name)
                   << "\tmanufacturer=" << escaped(row.profile_manufacturer)
                   << "\tmodel=" << escaped(row.profile_model)
                   << "\tprofileSource=" << profile_source_name(row.profile_source)
                   << "\tmappedChannel=" << row.mapped_channel
                   << "\tencoding=" << encoding_name(row.encoding)
                   << "\tfine=" << row.fine_channel
                   << "\towner=" << escaped(row.mapping_owner)
                   << "\tcapabilityId=" << escaped(row.capability_id)
                   << "\tcapabilityName=" << escaped(row.capability_name)
                   << '\n';
        }
        output << "BOUNDS\tunionNonzero=" << inspection.union_nonzero_channels
               << "\trows=" << inspection.rows.size()
               << "\trowsTruncated=" << inspection.rows_truncated
               << "\tdiagnosticsTruncated="
               << inspection.diagnostics_truncated << '\n';
    }
    for (const auto& diagnostic : inspection.diagnostics) {
        output << "DIAGNOSTIC\tseverity="
               << diagnostic_severity_name(diagnostic.severity)
               << "\tcode=" << runner_frame_diagnostic_code_name(diagnostic.code)
               << "\tuniverse=" << static_cast<unsigned int>(diagnostic.universe)
               << "\tchannel=" << diagnostic.channel
               << "\tmessage=" << escaped(diagnostic.message) << '\n';
    }
    output << "END\n";
    return output.str();
}

std::string format_runner_raw_reference_comparison(
    const RunnerRawReferenceComparison& comparison) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << "EMBERLIGHTS_RAW_RUNNER_FRAME_COMPARISON_V1\n"
           << "REFERENCE\tvalid=" << comparison.valid_reference
           << "\tsnapshotAvailable=" << comparison.snapshot_available
           << "\tstale=" << comparison.stale
           << "\tuniverse=" << static_cast<unsigned int>(comparison.universe)
           << "\tsha256=" << comparison.reference_sha256 << '\n';
    const auto append_stage = [&](std::string_view name,
                                  const RunnerFrameStageComparison& stage) {
        output << "STAGE\t" << name
               << "\texact=" << stage.exact
               << "\tactualSha256=" << stage.actual_sha256
               << "\tdifferingChannels=" << stage.differing_channels
               << "\trows=" << stage.differences.size()
               << "\trowsTruncated=" << stage.rows_truncated << '\n';
        for (const auto& difference : stage.differences) {
            output << "DIFFERENCE\tstage=" << name
                   << "\tU" << static_cast<unsigned int>(comparison.universe)
                   << "\tCH" << difference.channel
                   << "\texpected=" << static_cast<unsigned int>(difference.expected)
                   << "\tactual=" << static_cast<unsigned int>(difference.actual)
                   << "\tkind="
                   << runner_frame_difference_kind_name(difference.kind)
                   << "\tcause="
                   << runner_frame_difference_cause_name(difference.cause);
            append_channel_context(output, difference.context);
            output << '\n';
        }
    };
    if (comparison.snapshot_available && comparison.valid_reference) {
        append_stage("preBlackout", comparison.pre_blackout);
        append_stage("routed", comparison.routed);
    }
    for (const auto& diagnostic : comparison.diagnostics) {
        output << "DIAGNOSTIC\tseverity="
               << diagnostic_severity_name(diagnostic.severity)
               << "\tcode=" << runner_frame_diagnostic_code_name(diagnostic.code)
               << "\tuniverse=" << static_cast<unsigned int>(diagnostic.universe)
               << "\tchannel=" << diagnostic.channel
               << "\tmessage=" << escaped(diagnostic.message) << '\n';
    }
    output << "BOUNDS\tdiagnosticsTruncated="
           << comparison.diagnostics_truncated << '\n'
           << "END\n";
    return output.str();
}

std::string_view runner_frame_inspection_status_name(
    RunnerFrameInspectionStatus status) noexcept {
    switch (status) {
    case RunnerFrameInspectionStatus::NoSnapshot: return "noSnapshot";
    case RunnerFrameInspectionStatus::Current: return "current";
    case RunnerFrameInspectionStatus::Stale: return "stale";
    case RunnerFrameInspectionStatus::InvalidTimestamp: return "invalidTimestamp";
    }
    return "invalid";
}

std::string_view runner_frame_diagnostic_code_name(
    RunnerFrameDiagnosticCode code) noexcept {
    switch (code) {
    case RunnerFrameDiagnosticCode::NoSnapshot: return "noSnapshot";
    case RunnerFrameDiagnosticCode::StaleSnapshot: return "staleSnapshot";
    case RunnerFrameDiagnosticCode::SnapshotTimestampAfterInspection:
        return "snapshotTimestampAfterInspection";
    case RunnerFrameDiagnosticCode::SnapshotGenerationInvalid:
        return "snapshotGenerationInvalid";
    case RunnerFrameDiagnosticCode::RowLimitClamped: return "rowLimitClamped";
    case RunnerFrameDiagnosticCode::RowsTruncated: return "rowsTruncated";
    case RunnerFrameDiagnosticCode::DiagnosticsTruncated:
        return "diagnosticsTruncated";
    case RunnerFrameDiagnosticCode::MetadataTruncated: return "metadataTruncated";
    case RunnerFrameDiagnosticCode::UnattributedNonzeroChannel:
        return "unattributedNonzeroChannel";
    case RunnerFrameDiagnosticCode::AttributionFixtureMissing:
        return "attributionFixtureMissing";
    case RunnerFrameDiagnosticCode::AttributionProfileMissing:
        return "attributionProfileMissing";
    case RunnerFrameDiagnosticCode::AttributionMappingMissing:
        return "attributionMappingMissing";
    case RunnerFrameDiagnosticCode::AttributionCapabilityMissing:
        return "attributionCapabilityMissing";
    case RunnerFrameDiagnosticCode::AttributionPatchMismatch:
        return "attributionPatchMismatch";
    case RunnerFrameDiagnosticCode::ReferenceUniverseInvalid:
        return "referenceUniverseInvalid";
    case RunnerFrameDiagnosticCode::ReferenceChannelInvalid:
        return "referenceChannelInvalid";
    case RunnerFrameDiagnosticCode::DifferenceRowsTruncated:
        return "differenceRowsTruncated";
    }
    return "invalid";
}

std::string_view runner_frame_difference_kind_name(
    RunnerFrameDifferenceKind kind) noexcept {
    switch (kind) {
    case RunnerFrameDifferenceKind::MissingExpectedValue:
        return "missingExpectedValue";
    case RunnerFrameDifferenceKind::UnexpectedActualValue:
        return "unexpectedActualValue";
    case RunnerFrameDifferenceKind::ValueMismatch: return "valueMismatch";
    }
    return "invalid";
}

std::string_view runner_frame_difference_cause_name(
    RunnerFrameDifferenceCause cause) noexcept {
    switch (cause) {
    case RunnerFrameDifferenceCause::GlobalBlackout: return "globalBlackout";
    case RunnerFrameDifferenceCause::Safety: return "safety";
    case RunnerFrameDifferenceCause::RendererConflict: return "rendererConflict";
    case RunnerFrameDifferenceCause::ProfileDefault: return "profileDefault";
    case RunnerFrameDifferenceCause::ProfileConstant: return "profileConstant";
    case RunnerFrameDifferenceCause::PropertyWinner: return "propertyWinner";
    case RunnerFrameDifferenceCause::CapabilityWinner: return "capabilityWinner";
    case RunnerFrameDifferenceCause::Unattributed: return "unattributed";
    case RunnerFrameDifferenceCause::InvalidAttribution:
        return "invalidAttribution";
    }
    return "invalid";
}

}  // namespace emberlights
