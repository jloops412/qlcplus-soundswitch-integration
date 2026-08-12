#include "emberlights/studio_preview.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string_view>
#include <utility>

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

[[nodiscard]] emberlights::ProjectDocument make_preview_project() {
    emberlights::ProjectDocument project;
    project.id = "autoloop-v2-preview-project";
    project.name = "Autoloop V2 Preview Project";
    project.connections.artnet_enabled = true;
    project.connections.artnet_destination = "203.0.113.10";
    project.connections.sacn_enabled = true;
    project.connections.sacn_destination = "203.0.113.11";
    project.connections.dmx_usb_pro_ports[0] = "COM17";
    project.connections.soundswitch_micro_universe = 1U;
    project.fixture_profiles.push_back({
        "preview.rgb",
        "EmberLights",
        "Synthetic RGB",
        "4ch",
        "Synthetic RGB 4ch",
        showcore::FixtureProfileSource::Local,
        "1",
        4U,
        {
            {showcore::Property::Intensity, 0U, -1,
             showcore::ChannelEncoding::Linear8, 0U, 255U, 0U},
            {showcore::Property::Red, 1U, -1,
             showcore::ChannelEncoding::Linear8, 0U, 255U, 0U},
            {showcore::Property::Green, 2U, -1,
             showcore::ChannelEncoding::Linear8, 0U, 255U, 0U},
            {showcore::Property::Blue, 3U, -1,
             showcore::ChannelEncoding::Linear8, 0U, 255U, 0U},
        }});
    project.fixtures.push_back({
        "preview-fixture",
        "Preview Fixture",
        "preview.rgb",
        1U,
        1U,
        {"wash"}});
    project.groups.push_back({
        "preview-group", "Preview Group", {"preview-fixture"}});
    return project;
}

[[nodiscard]] emberlights::AutoloopSourceDocument make_preview_source() {
    emberlights::AutoloopSourceDocument source;
    source.assets.push_back({
        "preview.asset",
        "Preview Loop",
        "Original deterministic V2 preview test content.",
        {"original", "test"},
        "preview",
        0.5F,
        "preview.program",
        "preview.launch",
        "preview.provenance",
        1U});
    source.placements.push_back({
        "preview.placement", 0U, 0U, "preview.asset", {}});

    emberlights::AutoloopProgramDefinition program;
    program.id = "preview.program";
    program.length_ticks = 4 * emberlights::kMusicalTicksPerQuarter;
    program.targets.push_back({
        "preview.target.master",
        emberlights::AutoloopTargetKind::Master,
        {},
        {
            showcore::Property::Intensity,
            showcore::Property::Red,
            showcore::Property::Blue,
        }});
    program.lanes.push_back({
        "preview.lane.master", "preview.target.master", 0U});

    emberlights::AutoloopEventDefinition intensity;
    intensity.id = "preview.event.intensity";
    intensity.lane_id = "preview.lane.master";
    intensity.kind = emberlights::AutoloopEventKind::PropertyBlock;
    intensity.start_tick = 0;
    intensity.end_tick = program.length_ticks;
    intensity.property = showcore::Property::Intensity;
    intensity.value = showcore::PropertyValue::set(1.0F);
    program.events.push_back(intensity);

    emberlights::AutoloopEventDefinition red;
    red.id = "preview.event.red";
    red.lane_id = "preview.lane.master";
    red.kind = emberlights::AutoloopEventKind::PropertyBlock;
    red.start_tick = 0;
    red.end_tick = 2 * emberlights::kMusicalTicksPerQuarter;
    red.property = showcore::Property::Red;
    red.value = showcore::PropertyValue::set(1.0F);
    program.events.push_back(red);

    emberlights::AutoloopEventDefinition blue;
    blue.id = "preview.event.blue";
    blue.lane_id = "preview.lane.master";
    blue.kind = emberlights::AutoloopEventKind::PropertyBlock;
    blue.start_tick = 2 * emberlights::kMusicalTicksPerQuarter;
    blue.end_tick = program.length_ticks;
    blue.property = showcore::Property::Blue;
    blue.value = showcore::PropertyValue::set(1.0F);
    program.events.push_back(blue);
    source.programs.push_back(std::move(program));

    emberlights::AutoloopLaunchProfileDefinition launch;
    launch.id = "preview.launch";
    launch.repeat = showcore::AutoloopRepeat::Infinite;
    source.launch_profiles.push_back(std::move(launch));

    emberlights::AutoloopProvenanceDefinition provenance;
    provenance.id = "preview.provenance";
    provenance.origin = emberlights::AutoloopProvenanceOrigin::Native;
    provenance.producer_id = "emberlights.preview.test";
    provenance.producer_version = "1";
    provenance.source_object_key = "preview.asset";
    provenance.evidence_status = "synthetic";
    source.provenance.push_back(std::move(provenance));
    emberlights::normalize_autoloop_source(source);
    return source;
}

[[nodiscard]] emberlights::StudioDocumentSnapshot document_snapshot(
    emberlights::ProjectDocument project = make_preview_project(),
    emberlights::StudioDocumentGeneration generation = 7U) {
    emberlights::StudioDocumentSnapshot snapshot;
    snapshot.document = std::move(project);
    snapshot.generation = generation;
    return snapshot;
}

[[nodiscard]] emberlights::AutoloopAuthoringSnapshot source_snapshot(
    emberlights::AutoloopSourceDocument source = make_preview_source(),
    emberlights::StudioDocumentGeneration generation = 11U) {
    emberlights::AutoloopAuthoringSnapshot snapshot;
    snapshot.source = std::move(source);
    snapshot.generation = generation;
    snapshot.source_digest =
        emberlights::autoloop_source_digest(snapshot.source);
    return snapshot;
}

[[nodiscard]] bool has_diagnostic(
    const emberlights::StudioPreviewOutcome& outcome,
    showcore::AutoloopCompileError error,
    std::string_view code = {}) {
    return std::any_of(
        outcome.autoloop_diagnostics.begin(),
        outcome.autoloop_diagnostics.end(),
        [&](const auto& diagnostic) {
            return diagnostic.error == error &&
                (code.empty() || diagnostic.code == code);
        });
}

[[nodiscard]] bool owns(
    const emberlights::StudioPreviewSnapshot& snapshot,
    showcore::Property property,
    float value) {
    return std::any_of(
        snapshot.ownership.begin(), snapshot.ownership.end(),
        [&](const auto& ownership) {
            return ownership.fixture_id == "preview-fixture" &&
                ownership.property == property &&
                ownership.value.mode == showcore::ValueMode::Set &&
                std::fabs(ownership.value.value - value) < 0.0001F;
        });
}

void test_exact_output_disabled_transport_and_trace() {
    const auto document = document_snapshot();
    const auto source = source_snapshot();
    emberlights::StudioPreviewService preview;
    const auto loaded = preview.load_autoloop_v2(document, source);
    CHECK(loaded.result == emberlights::StudioPreviewResult::Loaded);
    CHECK(loaded.generation == document.generation);
    CHECK(loaded.source_generation == source.generation);
    CHECK(loaded.source_digest == source.source_digest);
    CHECK(loaded.compiled_digest.size() == 64U);
    CHECK(preview.snapshot().output_disabled);
    CHECK(preview.snapshot().source_generation == source.generation);
    CHECK(preview.snapshot().source_digest == source.source_digest);
    if (!loaded) {
        return;
    }

    const auto selected = preview.preview_autoloop_v2(
        document.generation, source.generation, "preview.placement");
    CHECK(selected.result == emberlights::StudioPreviewResult::Applied);
    CHECK(selected.realization == emberlights::StudioPreviewRealization::Exact);
    CHECK(preview.snapshot().autoloop_format ==
          emberlights::StudioPreviewAutoloopFormat::V2);
    CHECK(preview.snapshot().asset_id == "preview.asset");
    CHECK(preview.snapshot().program_id == "preview.program");
    CHECK(preview.snapshot().placement_id == "preview.placement");
    if (!selected) {
        return;
    }
    CHECK(preview.snapshot().transport_tick == 0);
    CHECK(preview.snapshot().loop_tick == 0);
    CHECK(preview.snapshot().phase == 0.0);
    CHECK(preview.snapshot().frame_sha256.size() == 64U);
    CHECK(preview.snapshot().fixtures.size() == 1U);
    CHECK(preview.snapshot().fixtures.front().dmx_values.size() == 4U);
    if (preview.snapshot().fixtures.front().dmx_values.size() == 4U) {
        CHECK(preview.snapshot().fixtures.front().dmx_values[0] == 255U);
        CHECK(preview.snapshot().fixtures.front().dmx_values[1] == 255U);
        CHECK(preview.snapshot().fixtures.front().dmx_values[2] == 0U);
        CHECK(preview.snapshot().fixtures.front().dmx_values[3] == 0U);
    }
    CHECK(owns(preview.snapshot(), showcore::Property::Intensity, 1.0F));
    CHECK(owns(preview.snapshot(), showcore::Property::Red, 1.0F));
    const auto first_frame = preview.snapshot().frame_sha256;

    CHECK(preview.seek_autoloop_v2(
        7U, 11U, 2 * emberlights::kMusicalTicksPerQuarter));
    CHECK(preview.snapshot().transport_tick == 1920);
    CHECK(preview.snapshot().loop_tick == 1920);
    CHECK(std::fabs(preview.snapshot().phase - 0.5) < 0.000001);
    CHECK(preview.snapshot().fixtures.front().dmx_values[1] == 0U);
    CHECK(preview.snapshot().fixtures.front().dmx_values[3] == 255U);
    CHECK(owns(preview.snapshot(), showcore::Property::Blue, 1.0F));
    const auto second_frame = preview.snapshot().frame_sha256;
    CHECK(second_frame != first_frame);

    CHECK(preview.seek_autoloop_v2(
        7U, 11U, 4 * emberlights::kMusicalTicksPerQuarter));
    CHECK(preview.snapshot().loop_tick == 0);
    CHECK(preview.snapshot().completed_loops == 1U);
    CHECK(preview.snapshot().frame_sha256 == first_frame);

    CHECK(preview.seek_autoloop_v2_phase(7U, 11U, 0.25));
    CHECK(preview.snapshot().transport_tick == 4800);
    CHECK(preview.snapshot().loop_tick == 960);
    CHECK(preview.snapshot().frame_sha256 == first_frame);
    CHECK(preview.seek_autoloop_v2_beat(7U, 11U, 2.0));
    CHECK(preview.snapshot().transport_tick == 1920);
    CHECK(preview.snapshot().frame_sha256 == second_frame);
    CHECK(preview.advance_autoloop_v2(7U, 11U, 1920));
    CHECK(preview.snapshot().transport_tick == 3840);
    CHECK(preview.snapshot().frame_sha256 == first_frame);
    CHECK(preview.restart_autoloop_v2(7U, 11U));
    CHECK(preview.snapshot().transport_tick == 0);
    CHECK(preview.snapshot().frame_sha256 == first_frame);

    const auto before_rejection = preview.snapshot().frame_sha256;
    CHECK(preview.seek_autoloop_v2(6U, 11U, 1).result ==
          emberlights::StudioPreviewResult::StaleGeneration);
    CHECK(preview.seek_autoloop_v2(7U, 10U, 1).result ==
          emberlights::StudioPreviewResult::StaleGeneration);
    CHECK(preview.seek_autoloop_v2(7U, 11U, -1).result ==
          emberlights::StudioPreviewResult::InvalidArgument);
    CHECK(preview.seek_autoloop_v2(
        7U,
        11U,
        emberlights::kMaximumStudioAutoloopPreviewTransportTick + 1).result ==
          emberlights::StudioPreviewResult::InvalidArgument);
    CHECK(preview.seek_autoloop_v2_beat(
        7U, 11U, std::numeric_limits<double>::infinity()).result ==
          emberlights::StudioPreviewResult::InvalidArgument);
    CHECK(preview.seek_autoloop_v2_phase(7U, 11U, 1.0).result ==
          emberlights::StudioPreviewResult::InvalidArgument);
    CHECK(preview.advance_autoloop_v2(7U, 11U, -1).result ==
          emberlights::StudioPreviewResult::InvalidArgument);
    CHECK(preview.snapshot().frame_sha256 == before_rejection);
}

void test_determinism_digest_guard_and_last_good_retention() {
    const auto document = document_snapshot();
    const auto source = source_snapshot();
    emberlights::StudioPreviewService first;
    emberlights::StudioPreviewService second;
    CHECK(first.load_autoloop_v2(document, source));
    CHECK(second.load_autoloop_v2(document, source));
    CHECK(first.preview_autoloop_v2(7U, 11U, "preview.placement"));
    CHECK(second.preview_autoloop_v2(7U, 11U, "preview.placement"));
    for (const auto tick : {0, 1, 959, 960, 1919, 1920, 3839, 3840, 7777}) {
        CHECK(first.seek_autoloop_v2(7U, 11U, tick));
        CHECK(second.seek_autoloop_v2(7U, 11U, tick));
        CHECK(first.snapshot().frame_sha256 == second.snapshot().frame_sha256);
        CHECK(first.snapshot().dmx_frames.universes ==
              second.snapshot().dmx_frames.universes);
    }
    CHECK(first.snapshot().compiled_digest == second.snapshot().compiled_digest);
    const auto last_good_frame = first.snapshot().frame_sha256;
    const auto last_good_compiled = first.snapshot().compiled_digest;

    auto bad_digest = source;
    bad_digest.generation = 12U;
    bad_digest.source_digest.front() =
        bad_digest.source_digest.front() == '0' ? '1' : '0';
    const auto rejected = first.load_autoloop_v2(document, bad_digest);
    CHECK(rejected.result == emberlights::StudioPreviewResult::InvalidArgument);
    CHECK(rejected.source_generation == 12U);
    CHECK(has_diagnostic(
        rejected,
        showcore::AutoloopCompileError::InvalidSource,
        "autoloop.preview.sourceDigestMismatch"));
    CHECK(first.snapshot().frame_sha256 == last_good_frame);
    CHECK(first.snapshot().compiled_digest == last_good_compiled);

    auto reused_generation = source;
    reused_generation.source.assets.front().name = "Changed without generation";
    reused_generation.source_digest =
        emberlights::autoloop_source_digest(reused_generation.source);
    const auto stale = first.load_autoloop_v2(document, reused_generation);
    CHECK(stale.result == emberlights::StudioPreviewResult::StaleGeneration);
    CHECK(first.snapshot().frame_sha256 == last_good_frame);

    auto older_source = source;
    older_source.generation = 10U;
    CHECK(first.load_autoloop_v2(document, older_source).result ==
          emberlights::StudioPreviewResult::StaleGeneration);
    auto older_document = document;
    older_document.generation = 6U;
    CHECK(first.load_autoloop_v2(older_document, source).result ==
          emberlights::StudioPreviewResult::StaleGeneration);

    auto invalid_source = source;
    invalid_source.generation = 12U;
    invalid_source.source.programs.front().events.front().end_tick =
        invalid_source.source.programs.front().length_ticks + 1;
    invalid_source.source_digest =
        emberlights::autoloop_source_digest(invalid_source.source);
    const auto invalid = first.load_autoloop_v2(document, invalid_source);
    CHECK(invalid.result ==
          emberlights::StudioPreviewResult::CompilationFailed);
    CHECK(has_diagnostic(
        invalid, showcore::AutoloopCompileError::InvalidSource));
    CHECK(first.snapshot().frame_sha256 == last_good_frame);

    auto zero_source = source;
    zero_source.generation = 0U;
    emberlights::StudioPreviewService unloaded;
    CHECK(unloaded.load_autoloop_v2(document, zero_source).result ==
          emberlights::StudioPreviewResult::InvalidArgument);
    auto zero_document = document;
    zero_document.generation = 0U;
    CHECK(unloaded.load_autoloop_v2(zero_document, source).result ==
          emberlights::StudioPreviewResult::InvalidArgument);
}

void test_linked_content_transition_and_capability_fail_closed() {
    for (const auto kind : {
             emberlights::AutoloopEventKind::Palette,
             emberlights::AutoloopEventKind::Position,
             emberlights::AutoloopEventKind::Attribute}) {
        auto source = make_preview_source();
        source.programs.front().events.resize(1U);
        source.programs.front().targets.front().required_properties.clear();
        source.programs.front().events.front().kind = kind;
        source.programs.front().events.front().reference_id =
            "unresolved.semantic.reference";
        emberlights::normalize_autoloop_source(source);
        emberlights::StudioPreviewService preview;
        const auto failed = preview.load_autoloop_v2(
            document_snapshot(), source_snapshot(std::move(source)));
        CHECK(failed.result ==
              emberlights::StudioPreviewResult::CompilationFailed);
        CHECK(failed.realization ==
              emberlights::StudioPreviewRealization::Unsupported);
        CHECK(has_diagnostic(
            failed, showcore::AutoloopCompileError::MissingReference));
        CHECK(preview.snapshot().content_kind ==
              emberlights::StudioPreviewContentKind::None);
        CHECK(preview.snapshot().output_disabled);
    }

    auto transition_source = make_preview_source();
    transition_source.programs.front().events.front().transition_reference_id =
        "unresolved.custom.transition";
    emberlights::normalize_autoloop_source(transition_source);
    emberlights::StudioPreviewService transition_preview;
    const auto transition_failed = transition_preview.load_autoloop_v2(
        document_snapshot(), source_snapshot(std::move(transition_source)));
    CHECK(transition_failed.result ==
          emberlights::StudioPreviewResult::CompilationFailed);
    CHECK(has_diagnostic(
        transition_failed,
        showcore::AutoloopCompileError::UnsupportedPayload,
        "autoloop.compile.transition.unsupported"));

    auto unsupported_project = make_preview_project();
    auto& channels = unsupported_project.fixture_profiles.front().channels;
    channels.erase(std::remove_if(
        channels.begin(), channels.end(), [](const auto& channel) {
            return channel.property == showcore::Property::Blue;
        }), channels.end());
    emberlights::StudioPreviewService capability_preview;
    const auto capability_failed = capability_preview.load_autoloop_v2(
        document_snapshot(std::move(unsupported_project)), source_snapshot());
    CHECK(capability_failed.result ==
          emberlights::StudioPreviewResult::CompilationFailed);
    CHECK(capability_failed.realization ==
          emberlights::StudioPreviewRealization::Unsupported);
    CHECK(has_diagnostic(
        capability_failed, showcore::AutoloopCompileError::MissingCapability));
    CHECK(capability_preview.snapshot().content_kind ==
          emberlights::StudioPreviewContentKind::None);
}

void test_bounded_trace_keeps_recent_exact_frames() {
    emberlights::StudioPreviewService preview;
    CHECK(preview.load_autoloop_v2(
        document_snapshot(), source_snapshot()));
    CHECK(preview.preview_autoloop_v2(
        7U, 11U, "preview.placement"));
    for (std::size_t index = 0U; index < 300U; ++index) {
        CHECK(preview.advance_autoloop_v2(7U, 11U, 1));
    }
    CHECK(preview.snapshot().frame_trace.size() ==
          emberlights::kMaximumStudioAutoloopPreviewTraceEntries);
    CHECK(preview.snapshot().dropped_trace_entries == 45U);
    if (!preview.snapshot().frame_trace.empty()) {
        CHECK(preview.snapshot().frame_trace.front().transport_tick == 45);
        CHECK(preview.snapshot().frame_trace.back().transport_tick == 300);
        CHECK(preview.snapshot().frame_trace.back().frame_sha256 ==
              preview.snapshot().frame_sha256);
    }
}

}  // namespace

int main() {
    test_exact_output_disabled_transport_and_trace();
    test_determinism_digest_guard_and_last_good_retention();
    test_linked_content_transition_and_capability_fail_closed();
    test_bounded_trace_keeps_recent_exact_frames();

    if (failures != 0) {
        std::cerr << failures << " Autoloop V2 preview test(s) failed\n";
        return 1;
    }
    std::cout << "Autoloop V2 output-disabled preview tests passed\n";
    return 0;
}
