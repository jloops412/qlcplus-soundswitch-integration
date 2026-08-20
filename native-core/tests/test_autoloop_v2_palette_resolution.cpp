#include "emberlights/autoloop_palette_resolution.hpp"
#include "emberlights/studio_preview.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

[[nodiscard]] emberlights::FixtureProfileDefinition rgb_profile() {
    return {
        "palette.rgb",
        "EmberLights",
        "Semantic RGB",
        "4ch",
        "Semantic RGB 4ch",
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
        }};
}

[[nodiscard]] emberlights::FixtureProfileDefinition white_profile() {
    return {
        "palette.white",
        "EmberLights",
        "Semantic White",
        "2ch",
        "Semantic White 2ch",
        showcore::FixtureProfileSource::Local,
        "1",
        2U,
        {
            {showcore::Property::Intensity, 0U, -1,
             showcore::ChannelEncoding::Linear8, 0U, 255U, 0U},
            {showcore::Property::White, 1U, -1,
             showcore::ChannelEncoding::Linear8, 0U, 255U, 0U},
        }};
}

[[nodiscard]] emberlights::FixtureProfileDefinition wheel_profile() {
    return {
        "palette.wheel",
        "EmberLights",
        "Raw Wheel",
        "1ch",
        "Raw Wheel 1ch",
        showcore::FixtureProfileSource::Local,
        "1",
        1U,
        {{showcore::Property::ColorWheel, 0U, -1,
          showcore::ChannelEncoding::Discrete8, 17U, 231U, 99U}}};
}

[[nodiscard]] emberlights::StudioColorPaletteAsset palette(
    std::string palette_id,
    std::string swatch_id = "swatch.cerulean") {
    emberlights::StudioColorPaletteAsset result;
    result.id = std::move(palette_id);
    result.name = "Project Palette";
    emberlights::StudioColorSwatch swatch;
    swatch.id = std::move(swatch_id);
    swatch.name = "Cerulean Display Name";
    swatch.color.rgb = {0.2F, 0.4F, 0.8F};
    swatch.color.intensity = 0.75F;
    result.swatches.push_back(std::move(swatch));
    return result;
}

[[nodiscard]] emberlights::ProjectDocument make_project() {
    emberlights::ProjectDocument project;
    project.id = "palette-resolution-project";
    project.name = "Palette Resolution Project";
    project.connections.artnet_enabled = true;
    project.connections.artnet_destination = "203.0.113.20";
    project.connections.sacn_enabled = true;
    project.connections.sacn_destination = "203.0.113.21";
    project.connections.dmx_usb_pro_ports[0] = "COM81";
    project.fixture_profiles.push_back(rgb_profile());
    project.fixtures.push_back({
        "fixture.rgb",
        "RGB Fixture",
        "palette.rgb",
        1U,
        1U,
        {"wash"}});
    project.groups.push_back({
        "group.colors", "Color Fixtures", {"fixture.rgb"}});
    project.color_palettes.push_back(palette("palette.primary"));
    return project;
}

[[nodiscard]] emberlights::AutoloopSourceDocument make_source(
    std::string reference_id = "swatch.cerulean") {
    emberlights::AutoloopSourceDocument source;
    source.assets.push_back({
        "palette.asset",
        "Palette Loop",
        "Original semantic palette resolution test content.",
        {"original", "palette"},
        "test",
        0.5F,
        "palette.program",
        "palette.launch",
        "palette.provenance",
        1U});
    source.placements.push_back({
        "palette.placement", 0U, 0U, "palette.asset", {}});

    emberlights::AutoloopProgramDefinition program;
    program.id = "palette.program";
    program.length_ticks = 4 * emberlights::kMusicalTicksPerQuarter;
    program.targets.push_back({
        "palette.target.master",
        emberlights::AutoloopTargetKind::Master,
        {},
        {}});
    program.lanes.push_back({
        "palette.lane.master", "palette.target.master", 0U});
    emberlights::AutoloopEventDefinition event;
    event.id = "palette.event.color";
    event.lane_id = "palette.lane.master";
    event.kind = emberlights::AutoloopEventKind::Palette;
    event.start_tick = 0;
    event.end_tick = program.length_ticks;
    event.reference_id = std::move(reference_id);
    program.events.push_back(std::move(event));
    source.programs.push_back(std::move(program));

    emberlights::AutoloopLaunchProfileDefinition launch;
    launch.id = "palette.launch";
    launch.repeat = showcore::AutoloopRepeat::Infinite;
    source.launch_profiles.push_back(std::move(launch));
    emberlights::AutoloopProvenanceDefinition provenance;
    provenance.id = "palette.provenance";
    provenance.origin = emberlights::AutoloopProvenanceOrigin::Native;
    provenance.producer_id = "emberlights.palette-resolution.test";
    provenance.producer_version = "1";
    provenance.source_object_key = "palette.asset";
    provenance.evidence_status = "synthetic";
    source.provenance.push_back(std::move(provenance));
    emberlights::normalize_autoloop_source(source);
    return source;
}

[[nodiscard]] emberlights::StudioDocumentSnapshot document_snapshot(
    emberlights::ProjectDocument project,
    emberlights::StudioDocumentGeneration generation = 7U) {
    emberlights::StudioDocumentSnapshot snapshot;
    snapshot.document = std::move(project);
    snapshot.generation = generation;
    return snapshot;
}

[[nodiscard]] emberlights::AutoloopAuthoringSnapshot source_snapshot(
    emberlights::AutoloopSourceDocument source,
    emberlights::StudioDocumentGeneration generation = 11U) {
    emberlights::AutoloopAuthoringSnapshot snapshot;
    snapshot.source = std::move(source);
    snapshot.generation = generation;
    snapshot.source_digest =
        emberlights::autoloop_source_digest(snapshot.source);
    return snapshot;
}

[[nodiscard]] bool has_diagnostic(
    std::span<const showcore::AutoloopCompileDiagnostic> diagnostics,
    showcore::AutoloopCompileError error,
    std::string_view code = {}) {
    return std::any_of(
        diagnostics.begin(), diagnostics.end(), [&](const auto& diagnostic) {
            return diagnostic.error == error &&
                (code.empty() || diagnostic.code == code);
        });
}

[[nodiscard]] bool has_diagnostic(
    const emberlights::StudioPreviewOutcome& outcome,
    showcore::AutoloopCompileError error,
    std::string_view code = {}) {
    return has_diagnostic(outcome.autoloop_diagnostics, error, code);
}

void test_exact_target_scoped_bindings_and_determinism() {
    const auto project = make_project();
    const auto source = make_source();
    emberlights::AutoloopPaletteCompileEnvironment first(project, source);
    emberlights::AutoloopPaletteCompileEnvironment second(project, source);
    CHECK(first.ok());
    CHECK(!first.degraded());
    CHECK(first.resolutions().size() == 1U);
    if (!first.resolutions().empty()) {
        const auto& resolution = first.resolutions().front();
        CHECK(resolution.status ==
              emberlights::AutoloopPaletteResolutionStatus::Exact);
        CHECK(resolution.reference_id == "swatch.cerulean");
        CHECK(resolution.palette_ids ==
              std::vector<std::string>{"palette.primary"});
        CHECK(resolution.fixture_count == 1U);
        CHECK(resolution.exact_fixture_count == 1U);
        CHECK(resolution.degraded_fixture_count == 0U);
        CHECK(resolution.unsupported_fixture_count == 0U);
    }
    const auto environment = first.environment();
    CHECK(environment.targets.size() == 1U);
    CHECK(environment.references.size() == 1U);
    if (!environment.references.empty()) {
        const auto& binding = environment.references.front();
        CHECK(binding.kind ==
              showcore::CompiledAutoloopReferenceKind::Palette);
        CHECK(binding.stable_id == "swatch.cerulean");
        CHECK(binding.assignments.size() == 4U);
        for (const auto& assignment : binding.assignments) {
            CHECK(assignment.fixture_id == 0U);
            CHECK(assignment.property != showcore::Property::ColorWheel);
            CHECK(assignment.property != showcore::Property::Custom1);
        }
    }

    const auto compiled_first = showcore::compile_autoloop_programs(
        source, first.environment());
    const auto compiled_second = showcore::compile_autoloop_programs(
        source, second.environment());
    CHECK(compiled_first);
    CHECK(compiled_second);
    if (compiled_first && compiled_second) {
        CHECK(compiled_first.package->digest() ==
              compiled_second.package->digest());
    }

    auto reused_source = make_source();
    auto& reused_program = reused_source.programs.front();
    reused_program.events.front().end_tick =
        2 * emberlights::kMusicalTicksPerQuarter;
    auto reused_event = reused_program.events.front();
    reused_event.id = "palette.event.color.second";
    reused_event.start_tick = reused_program.events.front().end_tick;
    reused_event.end_tick = reused_program.length_ticks;
    reused_program.events.push_back(std::move(reused_event));
    emberlights::normalize_autoloop_source(reused_source);
    emberlights::AutoloopPaletteCompileEnvironment reused(
        project, reused_source);
    CHECK(reused.ok());
    CHECK(reused.environment().references.size() == 1U);
    CHECK(reused.resolutions().size() == 1U);
    CHECK(showcore::compile_autoloop_programs(
        reused_source, reused.environment()));

    auto targeted_project = make_project();
    targeted_project.fixture_profiles.push_back(white_profile());
    targeted_project.fixtures.push_back({
        "fixture.white",
        "White Fixture",
        "palette.white",
        1U,
        5U,
        {"wash"}});
    auto targeted_source = make_source();
    auto& program = targeted_source.programs.front();
    program.targets.front().kind = emberlights::AutoloopTargetKind::Fixture;
    program.targets.front().stable_ref = "fixture.rgb";
    program.targets.front().id = "palette.target.rgb";
    program.lanes.front().target_id = "palette.target.rgb";
    program.targets.push_back({
        "palette.target.white",
        emberlights::AutoloopTargetKind::Fixture,
        "fixture.white",
        {}});
    program.lanes.push_back({
        "palette.lane.white", "palette.target.white", 0U});
    auto white_event = program.events.front();
    white_event.id = "palette.event.white";
    white_event.lane_id = "palette.lane.white";
    program.events.push_back(std::move(white_event));
    emberlights::normalize_autoloop_source(targeted_source);

    emberlights::AutoloopPaletteCompileEnvironment targeted(
        targeted_project, targeted_source);
    CHECK(targeted.ok());
    CHECK(targeted.degraded());
    CHECK(targeted.resolutions().size() == 2U);
    CHECK(targeted.environment().references.size() == 2U);
    bool saw_rgb = false;
    bool saw_white = false;
    for (const auto& binding : targeted.environment().references) {
        CHECK(!binding.assignments.empty());
        for (const auto& assignment : binding.assignments) {
            if (binding.target_stable_ref == "fixture.rgb") {
                saw_rgb = true;
                CHECK(assignment.fixture_id == 0U);
            } else if (binding.target_stable_ref == "fixture.white") {
                saw_white = true;
                CHECK(assignment.fixture_id == 1U);
            }
        }
    }
    CHECK(saw_rgb);
    CHECK(saw_white);
    CHECK(showcore::compile_autoloop_programs(
        targeted_source, targeted.environment()));
}

void test_degraded_and_nonuniform_capability_outcomes() {
    auto degraded_project = make_project();
    degraded_project.fixture_profiles = {white_profile()};
    degraded_project.fixtures.front().profile_id = "palette.white";
    degraded_project.fixture_profiles.front().footprint = 2U;
    const auto degraded_source = make_source();
    emberlights::AutoloopPaletteCompileEnvironment degraded(
        degraded_project, degraded_source);
    CHECK(degraded.ok());
    CHECK(degraded.degraded());
    CHECK(degraded.resolutions().size() == 1U);
    if (!degraded.resolutions().empty()) {
        CHECK(degraded.resolutions().front().status ==
              emberlights::AutoloopPaletteResolutionStatus::Degraded);
        CHECK(degraded.resolutions().front().degraded_fixture_count == 1U);
    }

    emberlights::StudioPreviewService preview;
    const auto loaded = preview.load_autoloop_v2(
        document_snapshot(degraded_project),
        source_snapshot(make_source()));
    CHECK(loaded.result == emberlights::StudioPreviewResult::Loaded);
    CHECK(loaded.realization ==
          emberlights::StudioPreviewRealization::Degraded);
    CHECK(preview.preview_autoloop_v2(
        7U, 11U, "palette.placement"));
    CHECK(preview.snapshot().realization ==
          emberlights::StudioPreviewRealization::Degraded);
    CHECK(preview.snapshot().output_disabled);

    auto mixed_project = make_project();
    mixed_project.fixture_profiles.push_back(white_profile());
    mixed_project.fixtures.push_back({
        "fixture.white",
        "White Fixture",
        "palette.white",
        1U,
        5U,
        {"wash"}});
    mixed_project.groups.front().fixture_ids.push_back("fixture.white");
    const auto mixed_source = make_source();
    emberlights::AutoloopPaletteCompileEnvironment mixed(
        mixed_project, mixed_source);
    CHECK(!mixed.ok());
    CHECK(mixed.resolutions().size() == 1U);
    if (!mixed.resolutions().empty()) {
        CHECK(mixed.resolutions().front().status ==
              emberlights::AutoloopPaletteResolutionStatus::Unsupported);
        CHECK(mixed.resolutions().front().fixture_count == 2U);
        CHECK(mixed.resolutions().front().unsupported_fixture_count == 2U);
    }
    CHECK(has_diagnostic(
        mixed.diagnostics(),
        showcore::AutoloopCompileError::MissingCapability,
        "autoloop.palette.capability.unsupported"));
    CHECK(mixed.environment().references.empty());
}

void test_missing_ambiguous_and_no_inference() {
    const auto project = make_project();
    const auto display_name_source = make_source("Cerulean Display Name");
    emberlights::AutoloopPaletteCompileEnvironment display_name(
        project, display_name_source);
    CHECK(!display_name.ok());
    CHECK(display_name.resolutions().front().status ==
          emberlights::AutoloopPaletteResolutionStatus::Missing);
    CHECK(has_diagnostic(
        display_name.diagnostics(),
        showcore::AutoloopCompileError::MissingReference,
        "autoloop.palette.reference.missing"));

    auto ambiguous_project = make_project();
    ambiguous_project.color_palettes.push_back(
        palette("palette.secondary"));
    const auto source = make_source();
    emberlights::AutoloopPaletteCompileEnvironment ambiguous(
        ambiguous_project, source);
    CHECK(!ambiguous.ok());
    CHECK(ambiguous.resolutions().front().status ==
          emberlights::AutoloopPaletteResolutionStatus::Ambiguous);
    CHECK(ambiguous.resolutions().front().palette_ids ==
          (std::vector<std::string>{
              "palette.primary", "palette.secondary"}));
    CHECK(has_diagnostic(
        ambiguous.diagnostics(),
        showcore::AutoloopCompileError::AmbiguousReference,
        "autoloop.palette.reference.ambiguous"));

    std::reverse(
        ambiguous_project.color_palettes.begin(),
        ambiguous_project.color_palettes.end());
    emberlights::AutoloopPaletteCompileEnvironment reordered(
        ambiguous_project, source);
    CHECK(reordered.resolutions().front().palette_ids ==
          ambiguous.resolutions().front().palette_ids);

    auto wheel_project = make_project();
    wheel_project.fixture_profiles = {wheel_profile()};
    wheel_project.fixtures.front().profile_id = "palette.wheel";
    emberlights::AutoloopPaletteCompileEnvironment wheel(
        wheel_project, source);
    CHECK(!wheel.ok());
    CHECK(wheel.resolutions().front().status ==
          emberlights::AutoloopPaletteResolutionStatus::Unsupported);
    CHECK(wheel.environment().references.empty());
    CHECK(has_diagnostic(
        wheel.diagnostics(),
        showcore::AutoloopCompileError::MissingCapability));
}

void test_preview_determinism_and_last_good_retention() {
    const auto project = make_project();
    const auto source = make_source();
    const auto document = document_snapshot(project);
    const auto authored = source_snapshot(source);
    emberlights::StudioPreviewService first;
    emberlights::StudioPreviewService second;
    CHECK(first.load_autoloop_v2(document, authored));
    CHECK(second.load_autoloop_v2(document, authored));
    CHECK(first.preview_autoloop_v2(
        7U, 11U, "palette.placement"));
    CHECK(second.preview_autoloop_v2(
        7U, 11U, "palette.placement"));
    CHECK(first.snapshot().compiled_digest ==
          second.snapshot().compiled_digest);
    CHECK(first.snapshot().frame_sha256 == second.snapshot().frame_sha256);
    CHECK(first.snapshot().dmx_frames.universes ==
          second.snapshot().dmx_frames.universes);
    CHECK(first.snapshot().output_disabled);
    CHECK(first.snapshot().fixtures.front().dmx_values.size() == 4U);
    if (first.snapshot().fixtures.front().dmx_values.size() == 4U) {
        CHECK(first.snapshot().fixtures.front().dmx_values[0] == 191U);
        CHECK(first.snapshot().fixtures.front().dmx_values[1] == 51U);
        CHECK(first.snapshot().fixtures.front().dmx_values[2] == 102U);
        CHECK(first.snapshot().fixtures.front().dmx_values[3] == 204U);
    }
    const auto last_good_generation = first.snapshot().generation;
    const auto last_good_source_generation =
        first.snapshot().source_generation;
    const auto last_good_compiled = first.snapshot().compiled_digest;
    const auto last_good_frame = first.snapshot().frame_sha256;

    auto missing_source = make_source("swatch.unknown");
    const auto missing = first.load_autoloop_v2(
        document_snapshot(project, 8U),
        source_snapshot(std::move(missing_source), 12U));
    CHECK(missing.result ==
          emberlights::StudioPreviewResult::CompilationFailed);
    CHECK(missing.realization ==
          emberlights::StudioPreviewRealization::Unsupported);
    CHECK(missing.palette_resolutions.size() == 1U);
    CHECK(missing.palette_resolutions.front().status ==
          emberlights::AutoloopPaletteResolutionStatus::Missing);
    CHECK(has_diagnostic(
        missing,
        showcore::AutoloopCompileError::MissingReference,
        "autoloop.palette.reference.missing"));
    CHECK(first.snapshot().generation == last_good_generation);
    CHECK(first.snapshot().source_generation ==
          last_good_source_generation);
    CHECK(first.snapshot().compiled_digest == last_good_compiled);
    CHECK(first.snapshot().frame_sha256 == last_good_frame);
    CHECK(first.snapshot().output_disabled);

    auto ambiguous_project = project;
    ambiguous_project.color_palettes.push_back(
        palette("palette.secondary"));
    const auto ambiguous = first.load_autoloop_v2(
        document_snapshot(std::move(ambiguous_project), 8U),
        source_snapshot(source, 12U));
    CHECK(ambiguous.result ==
          emberlights::StudioPreviewResult::CompilationFailed);
    CHECK(ambiguous.palette_resolutions.front().status ==
          emberlights::AutoloopPaletteResolutionStatus::Ambiguous);
    CHECK(first.snapshot().compiled_digest == last_good_compiled);
    CHECK(first.snapshot().frame_sha256 == last_good_frame);
}

void test_resolution_capacity_is_bounded() {
    const auto project = make_project();
    const auto source = make_source();
    showcore::AutoloopCompileLimits assignment_limits;
    assignment_limits.maximum_reference_assignments = 3U;
    emberlights::AutoloopPaletteCompileEnvironment assignment_capacity(
        project, source, assignment_limits);
    CHECK(!assignment_capacity.ok());
    CHECK(assignment_capacity.environment().references.empty());
    CHECK(has_diagnostic(
        assignment_capacity.diagnostics(),
        showcore::AutoloopCompileError::CapacityExceeded,
        "autoloop.palette.capacity.assignments"));

    showcore::AutoloopCompileLimits target_limits;
    target_limits.maximum_target_fixture_ids = 0U;
    emberlights::AutoloopPaletteCompileEnvironment target_capacity(
        project, source, target_limits);
    CHECK(!target_capacity.ok());
    CHECK(target_capacity.environment().targets.empty());
    CHECK(has_diagnostic(
        target_capacity.diagnostics(),
        showcore::AutoloopCompileError::CapacityExceeded,
        "autoloop.palette.capacity.targetFixtures"));
}

}  // namespace

int main() {
    test_exact_target_scoped_bindings_and_determinism();
    test_degraded_and_nonuniform_capability_outcomes();
    test_missing_ambiguous_and_no_inference();
    test_preview_determinism_and_last_good_retention();
    test_resolution_capacity_is_bounded();

    if (failures != 0) {
        std::cerr << failures
                  << " Autoloop V2 palette resolution test(s) failed\n";
        return 1;
    }
    std::cout
        << "Autoloop V2 project palette resolution tests passed\n";
    return 0;
}
