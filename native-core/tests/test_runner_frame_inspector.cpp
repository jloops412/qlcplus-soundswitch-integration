#include "emberlights/compiler.hpp"
#include "emberlights/file_identity.hpp"
#include "emberlights/runner_frame_inspector.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>

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

[[nodiscard]] emberlights::ProjectDocument make_project() {
    emberlights::ProjectDocument project;
    project.id = "frame-inspector-test";
    project.name = "Frame Inspector Test";

    emberlights::FixtureProfileDefinition profile;
    profile.id = "local.inspector.fixture";
    profile.manufacturer = "Test Works";
    profile.model = "Inspector Light";
    profile.mode = "Four channel";
    profile.name = "Inspector Light Four channel";
    profile.source = showcore::FixtureProfileSource::Local;
    profile.source_revision = "7";
    profile.footprint = 4U;

    emberlights::ChannelDefinition red;
    red.property = showcore::Property::Red;
    red.coarse_offset = 0U;
    red.encoding = showcore::ChannelEncoding::Linear8;
    red.owner = "emitter";

    emberlights::ChannelDefinition intensity;
    intensity.property = showcore::Property::Intensity;
    intensity.coarse_offset = 1U;
    intensity.encoding = showcore::ChannelEncoding::Linear8;
    intensity.owner = "master";
    intensity.default_value = 17U;

    emberlights::ChannelDefinition constant;
    constant.property = showcore::Property::Count;
    constant.coarse_offset = 2U;
    constant.encoding = showcore::ChannelEncoding::Constant8;
    constant.owner = "mode";
    constant.default_value = 23U;

    emberlights::ChannelDefinition wheel;
    wheel.property = showcore::Property::Count;
    wheel.coarse_offset = 3U;
    wheel.encoding = showcore::ChannelEncoding::Discrete8;
    wheel.owner = "wheel";
    emberlights::ChannelCapabilityDefinition open;
    open.id = "open";
    open.name = "Open";
    open.property = showcore::Property::Shutter;
    open.dmx_min = 20U;
    open.dmx_max = 40U;
    open.preferred_value = 30U;
    wheel.capabilities.push_back(open);

    profile.channels = {red, intensity, constant, wheel};
    project.fixture_profiles.push_back(std::move(profile));
    project.fixtures.push_back({
        "fixture-one",
        "Inspector Fixture",
        "local.inspector.fixture",
        1U,
        10U,
        {}});
    return project;
}

[[nodiscard]] emberlights::RunnerOutputSnapshot make_snapshot(
    const emberlights::ProjectDocument& project) {
    auto compilation = emberlights::compile_project(project);
    CHECK(static_cast<bool>(compilation));
    emberlights::RunnerOutputSnapshot snapshot;
    if (!compilation || compilation.show == nullptr) {
        return snapshot;
    }
    auto& engine = compilation.show->engine();
    engine.layers().set(
        showcore::LayerId::EventMoment,
        0U,
        showcore::Property::Red,
        showcore::PropertyValue::set(1.0F));
    engine.layers().set(
        showcore::LayerId::ManualOverride,
        0U,
        showcore::Property::Shutter,
        showcore::PropertyValue::set(0.5F));
    engine.tick();
    snapshot.generation = 42U;
    snapshot.sequence = 9U;
    snapshot.rendered_at_ms = 1000U;
    snapshot.pre_blackout_frames = engine.frames();
    snapshot.routed_frames = engine.frames();
    snapshot.attribution = engine.frame_attribution();
    snapshot.routes[0] = {
        showcore::OutputBackendKind::ArtNet,
        1U,
        2U,
        true,
        2U,
        2U,
        0U};
    snapshot.routes[3] = {
        showcore::OutputBackendKind::SoundSwitchMicro,
        1U,
        1U,
        true,
        1U,
        0U,
        995U};
    return snapshot;
}

[[nodiscard]] bool has_diagnostic(
    const std::vector<emberlights::RunnerFrameDiagnostic>& diagnostics,
    emberlights::RunnerFrameDiagnosticCode code) {
    return std::any_of(
        diagnostics.begin(), diagnostics.end(),
        [code](const auto& diagnostic) { return diagnostic.code == code; });
}

void test_no_snapshot_and_timestamp_diagnostics() {
    const auto project = make_project();
    const emberlights::RunnerFrameInspectionOptions options{1500U, 200U, 16U};
    const auto absent =
        emberlights::inspect_runner_frame(project, nullptr, options);
    CHECK(!absent.snapshot_available);
    CHECK(absent.status ==
          emberlights::RunnerFrameInspectionStatus::NoSnapshot);
    CHECK(has_diagnostic(
        absent.diagnostics,
        emberlights::RunnerFrameDiagnosticCode::NoSnapshot));
    CHECK(emberlights::format_runner_frame_inspection(absent) ==
          emberlights::format_runner_frame_inspection(absent));

    auto snapshot = make_snapshot(project);
    const auto future = emberlights::inspect_runner_frame(
        project,
        &snapshot,
        {999U, 200U, 16U});
    CHECK(future.status ==
          emberlights::RunnerFrameInspectionStatus::InvalidTimestamp);
    CHECK(has_diagnostic(
        future.diagnostics,
        emberlights::RunnerFrameDiagnosticCode::SnapshotTimestampAfterInspection));

    const auto stale = emberlights::inspect_runner_frame(
        project,
        &snapshot,
        {1500U, 200U, 16U});
    CHECK(stale.stale);
    CHECK(stale.snapshot_age_ms == 500U);
    CHECK(stale.status == emberlights::RunnerFrameInspectionStatus::Stale);
}

void test_attribution_hashes_routes_and_report_are_deterministic() {
    const auto project = make_project();
    const auto snapshot = make_snapshot(project);
    const auto inspection = emberlights::inspect_runner_frame(
        project,
        &snapshot,
        {1100U, 200U, 16U});

    CHECK(inspection.status ==
          emberlights::RunnerFrameInspectionStatus::Current);
    CHECK(inspection.snapshot_available);
    CHECK(!inspection.stale);
    CHECK(inspection.generation == 42U);
    CHECK(inspection.sequence == 9U);
    CHECK(inspection.pre_blackout_sha256.size() == 64U);
    CHECK(inspection.routed_sha256 == inspection.pre_blackout_sha256);
    CHECK(inspection.pre_blackout_nonzero[0] == 4U);
    CHECK(inspection.pre_blackout_first_nonzero[0] == 10U);
    CHECK(inspection.pre_blackout_last_nonzero[0] == 13U);
    CHECK(inspection.rows.size() == 4U);
    CHECK(!inspection.rows_truncated);

    const auto& red = inspection.rows[0];
    CHECK(red.universe == 1U);
    CHECK(red.channel == 10U);
    CHECK(red.pre_blackout_value == 255U);
    CHECK(red.routed_value == 255U);
    CHECK(red.renderer_attribution_present);
    CHECK(red.renderer_attribution_resolved);
    CHECK(red.fixture_id == "fixture-one");
    CHECK(red.fixture_name == "Inspector Fixture");
    CHECK(red.fixture_address == 10U);
    CHECK(red.profile_id == "local.inspector.fixture");
    CHECK(red.profile_mode == "Four channel");
    CHECK(red.profile_revision == "7");
    CHECK(red.mapped_channel == 10U);
    CHECK(red.mapping_property == showcore::Property::Red);
    CHECK(red.rendered_property == showcore::Property::Red);
    CHECK(red.renderer_origin == showcore::RenderValueOrigin::Property);
    CHECK(red.winning_layer == showcore::LayerId::EventMoment);
    CHECK(red.value_mode == showcore::ValueMode::Set);

    const auto& profile_default = inspection.rows[1];
    CHECK(profile_default.channel == 11U);
    CHECK(profile_default.pre_blackout_value == 17U);
    CHECK(profile_default.renderer_origin == showcore::RenderValueOrigin::Default);
    CHECK(profile_default.winning_layer == showcore::LayerId::Count);

    const auto& profile_constant = inspection.rows[2];
    CHECK(profile_constant.channel == 12U);
    CHECK(profile_constant.pre_blackout_value == 23U);
    CHECK(profile_constant.renderer_origin ==
          showcore::RenderValueOrigin::Constant);

    const auto& capability = inspection.rows[3];
    CHECK(capability.channel == 13U);
    CHECK(capability.pre_blackout_value == 30U);
    CHECK(capability.renderer_origin ==
          showcore::RenderValueOrigin::Capability);
    CHECK(capability.winning_layer == showcore::LayerId::ManualOverride);
    CHECK(capability.capability_id == "open");
    CHECK(capability.capability_name == "Open");

    CHECK(inspection.routes[0].configured);
    CHECK(inspection.routes[0].attempted_frames == 2U);
    CHECK(inspection.routes[0].accepted_frames == 2U);
    CHECK(inspection.routes[3].last_error == 995U);

    const auto first =
        emberlights::format_runner_frame_inspection(inspection);
    const auto second =
        emberlights::format_runner_frame_inspection(inspection);
    CHECK(first == second);
    CHECK(first.find("EMBERLIGHTS_RUNNER_FRAME_INSPECTION_V1") !=
          std::string::npos);
    CHECK(first.find("backend=SoundSwitch Micro") != std::string::npos);
    CHECK(first.find("fixtureId=fixture-one") != std::string::npos);
    CHECK(first.find("origin=Capability") != std::string::npos);
}

void test_blackout_distinction_and_raw_comparison_causes() {
    const auto project = make_project();
    auto snapshot = make_snapshot(project);
    snapshot.blackout_applied = true;
    snapshot.routed_frames.clear();

    const auto inspection = emberlights::inspect_runner_frame(
        project,
        &snapshot,
        {1100U, 200U, 16U});
    CHECK(inspection.blackout_applied);
    CHECK(inspection.pre_blackout_nonzero[0] == 4U);
    CHECK(inspection.routed_nonzero[0] == 0U);
    CHECK(inspection.pre_blackout_sha256 != inspection.routed_sha256);
    CHECK(inspection.rows.size() == 4U);
    CHECK(std::all_of(
        inspection.rows.begin(), inspection.rows.end(),
        [](const auto& row) { return row.changed_by_global_blackout; }));

    const auto exact_pre = emberlights::compare_runner_frame_to_raw(
        project,
        &snapshot,
        1U,
        snapshot.pre_blackout_frames.universes[0],
        {1100U, 200U, 16U});
    CHECK(exact_pre.valid_reference);
    CHECK(exact_pre.exact_pre_blackout());
    CHECK(!exact_pre.exact_routed());
    CHECK(exact_pre.routed.differing_channels == 4U);
    CHECK(std::all_of(
        exact_pre.routed.differences.begin(),
        exact_pre.routed.differences.end(),
        [](const auto& difference) {
            return difference.cause ==
                emberlights::RunnerFrameDifferenceCause::GlobalBlackout;
        }));

    showcore::DmxUniverse one_hot{};
    one_hot[9U] = 255U;
    const auto compared = emberlights::compare_runner_frame_to_raw(
        project, &snapshot, 1U, one_hot, {1100U, 200U, 16U});
    CHECK(compared.pre_blackout.differing_channels == 3U);
    CHECK(compared.pre_blackout.differences[0].channel == 11U);
    CHECK(compared.pre_blackout.differences[0].cause ==
          emberlights::RunnerFrameDifferenceCause::ProfileDefault);
    CHECK(compared.pre_blackout.differences[1].channel == 12U);
    CHECK(compared.pre_blackout.differences[1].cause ==
          emberlights::RunnerFrameDifferenceCause::ProfileConstant);
    CHECK(compared.pre_blackout.differences[2].channel == 13U);
    CHECK(compared.pre_blackout.differences[2].cause ==
          emberlights::RunnerFrameDifferenceCause::CapabilityWinner);

    const auto report =
        emberlights::format_runner_raw_reference_comparison(compared);
    CHECK(report.find("EMBERLIGHTS_RAW_RUNNER_FRAME_COMPARISON_V1") !=
          std::string::npos);
    CHECK(report.find("cause=profileDefault") != std::string::npos);
    CHECK(report.find("cause=profileConstant") != std::string::npos);
    CHECK(report.find("cause=capabilityWinner") != std::string::npos);
}

void test_one_hot_validation_bounds_and_attribution_fail_closed() {
    const auto project = make_project();
    auto snapshot = make_snapshot(project);

    const auto invalid_universe = emberlights::compare_runner_frame_to_one_hot(
        project, &snapshot, 3U, 1U, 255U, {1100U, 200U, 16U});
    CHECK(!invalid_universe.valid_reference);
    CHECK(has_diagnostic(
        invalid_universe.diagnostics,
        emberlights::RunnerFrameDiagnosticCode::ReferenceUniverseInvalid));

    const auto invalid_channel = emberlights::compare_runner_frame_to_one_hot(
        project, &snapshot, 1U, 0U, 255U, {1100U, 200U, 16U});
    CHECK(!invalid_channel.valid_reference);
    CHECK(has_diagnostic(
        invalid_channel.diagnostics,
        emberlights::RunnerFrameDiagnosticCode::ReferenceChannelInvalid));

    const auto zero_value = emberlights::compare_runner_frame_to_one_hot(
        project, &snapshot, 1U, 10U, 0U, {1100U, 200U, 16U});
    CHECK(zero_value.valid_reference);
    CHECK(zero_value.reference_sha256 ==
          emberlights::runner_dmx_universe_sha256(showcore::DmxUniverse{}));

    const auto bounded = emberlights::inspect_runner_frame(
        project,
        &snapshot,
        {1100U, 200U, 2U});
    CHECK(bounded.union_nonzero_channels == 4U);
    CHECK(bounded.rows.size() == 2U);
    CHECK(bounded.rows_truncated);
    CHECK(has_diagnostic(
        bounded.diagnostics,
        emberlights::RunnerFrameDiagnosticCode::RowsTruncated));

    snapshot.attribution.universes[0][9U].fixture_id = 99U;
    const auto invalid_attribution = emberlights::inspect_runner_frame(
        project,
        &snapshot,
        {1100U, 200U, 16U});
    CHECK(!invalid_attribution.rows.front().renderer_attribution_resolved);
    CHECK(has_diagnostic(
        invalid_attribution.diagnostics,
        emberlights::RunnerFrameDiagnosticCode::AttributionFixtureMissing));

    auto missing = make_snapshot(project);
    missing.attribution.universes[0][9U] = {};
    const auto unattributed = emberlights::inspect_runner_frame(
        project,
        &missing,
        {1100U, 200U, 16U});
    CHECK(has_diagnostic(
        unattributed.diagnostics,
        emberlights::RunnerFrameDiagnosticCode::UnattributedNonzeroChannel));
}

void test_known_sha256_vectors() {
    const showcore::DmxUniverse blackout{};
    CHECK(emberlights::runner_dmx_universe_sha256(blackout) ==
          "076a27c79e5ace2a3d47f9dd2e83e4ff6ea8872b3c2218f66c92b89b55f36560");

    showcore::DmxUniverse one_hot{};
    one_hot[0U] = 255U;
    CHECK(emberlights::runner_dmx_universe_sha256(one_hot) ==
          emberlights::sha256_bytes(one_hot));
}

}  // namespace

int main() {
    test_no_snapshot_and_timestamp_diagnostics();
    test_attribution_hashes_routes_and_report_are_deterministic();
    test_blackout_distinction_and_raw_comparison_causes();
    test_one_hot_validation_bounds_and_attribution_fail_closed();
    test_known_sha256_vectors();

    if (failures != 0) {
        std::cerr << failures << " runner frame inspector test(s) failed\n";
        return 1;
    }
    std::cout << "runner frame inspector tests passed\n";
    return 0;
}
