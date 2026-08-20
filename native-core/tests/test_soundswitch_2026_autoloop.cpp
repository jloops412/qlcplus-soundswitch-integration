#include "emberlights/autoloop_persistence.hpp"
#include "emberlights/fixture_profile_ids.hpp"
#include "emberlights/project_io.hpp"
#include "emberlights/soundswitch_2026_autoloop.hpp"
#include "emberlights/studio_document.hpp"
#include "emberlights/studio_preview.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
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

void append_u8(std::vector<std::uint8_t>& bytes, std::uint8_t value) {
    bytes.push_back(value);
}

void append_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8U));
    bytes.push_back(static_cast<std::uint8_t>(value >> 16U));
    bytes.push_back(static_cast<std::uint8_t>(value >> 24U));
}

void append_utf16_ascii(
    std::vector<std::uint8_t>& bytes,
    std::string_view value) {
    append_u32(bytes, static_cast<std::uint32_t>(value.size() + 1U));
    for (const auto character : value) {
        append_u8(bytes, static_cast<std::uint8_t>(character));
        append_u8(bytes, 0U);
    }
    append_u8(bytes, 0U);
    append_u8(bytes, 0U);
}

[[nodiscard]] std::vector<std::uint8_t> make_catalog() {
    std::vector<std::uint8_t> bytes;
    append_u32(bytes, 0x5509AAAAU);
    append_u32(bytes, 3U);
    append_u32(bytes, 0U);
    append_u32(bytes, 0U);
    append_u32(bytes, 2U);
    append_u32(bytes, 4U);
    for (const auto bank : {"Medium", "High", "Low", "Ambient"}) {
        append_u32(bytes, 1U);
        append_utf16_ascii(bytes, bank);
    }
    append_u32(bytes, 2U);
    append_u32(bytes, 2U);
    append_u32(bytes, 0U);
    append_u32(bytes, 8U);
    append_u32(bytes, 1U);
    append_utf16_ascii(bytes, "Raw Zero");
    append_u32(bytes, 0U);
    append_u32(bytes, 2U);
    append_u32(bytes, 7U);
    append_u32(bytes, 4U);
    append_u32(bytes, 1U);
    append_utf16_ascii(bytes, "Placed First");
    append_u32(bytes, 0U);
    append_u32(bytes, 2U);
    append_u32(bytes, 7U);
    append_u32(bytes, 0U);
    append_u8(bytes, 1U);
    for (std::size_t bank = 1U; bank < 4U; ++bank) {
        append_u32(bytes, 0U);
        append_u8(bytes, 1U);
    }
    return bytes;
}

void append_target_block(
    std::vector<std::uint8_t>& bytes,
    std::uint32_t target_id) {
    append_u32(bytes, target_id);
    append_u32(bytes, 4U);
    append_u32(bytes, 1U);
    append_u32(bytes, 1U);
    append_u32(bytes, 0U);
    append_u32(bytes, 2U);
    append_u32(bytes, 2U);
    append_u32(bytes, 0U);
    append_u32(bytes, 0U);
    append_u32(bytes, 0x40000000U);
    append_u8(bytes, 0x5AU);
    append_u32(bytes, 2U);
    append_u32(bytes, 1200U);
    append_u32(bytes, 1200U);
    append_u32(bytes, 0xFFFFFFFFU);
    append_u8(bytes, 0xA5U);
    append_u32(bytes, 0x80808080U);
    append_u32(bytes, 1U);
    append_u32(bytes, 1U);
    append_u32(bytes, 1U);
    append_u32(bytes, 2U);
    append_u32(bytes, 1U);
    append_u32(bytes, 0U);
    append_u32(bytes, 1200U);
    append_u32(bytes, 0x7F102030U);
    append_u32(bytes, 0x11223344U);
    append_u32(bytes, 0x80405060U);
    append_u32(bytes, 0x55667788U);
}

[[nodiscard]] std::vector<std::uint8_t> make_timeline(
    bool duplicate_target = false) {
    std::vector<std::uint8_t> bytes;
    append_u32(bytes, 0x5509AAAAU);
    append_u32(bytes, 3U);
    append_u32(bytes, 16U);
    append_u32(bytes, 16U);
    append_target_block(bytes, 42U);
    if (duplicate_target) append_target_block(bytes, 42U);
    return bytes;
}

void test_strict_catalog_decoder_uses_authored_placement() {
    const auto bytes = make_catalog();
    const auto decoded = emberlights::decode_soundswitch_v3_autoloop_catalog(
        bytes, "catalog-artifact");
    CHECK(decoded);
    CHECK(decoded.catalog.database_kind == 2U);
    CHECK(decoded.catalog.bank_names.front() == "Medium");
    CHECK(decoded.catalog.entries.size() == 2U);
    CHECK(decoded.catalog.placements.size() == 2U);
    if (decoded.catalog.placements.size() == 2U) {
        CHECK(decoded.catalog.placements[0].bank_index == 0U);
        CHECK(decoded.catalog.placements[0].slot_index == 0U);
        CHECK(decoded.catalog.placements[0].source_index == 7U);
        CHECK(decoded.catalog.placements[1].source_index == 0U);
        CHECK(decoded.catalog.placements[0].evidence.has_byte_range);
    }

    auto trailing = bytes;
    trailing.push_back(0U);
    CHECK(!emberlights::decode_soundswitch_v3_autoloop_catalog(
        trailing, "catalog-artifact"));

    auto unknown_placement = bytes;
    constexpr std::array<std::uint8_t, 8U> placement_pattern{
        {7U, 0U, 0U, 0U, 0U, 0U, 0U, 0U}};
    const auto placement = std::search(
        unknown_placement.begin(), unknown_placement.end(),
        placement_pattern.begin(), placement_pattern.end());
    CHECK(placement != unknown_placement.end());
    if (placement != unknown_placement.end()) {
        *placement = 9U;
        CHECK(!emberlights::decode_soundswitch_v3_autoloop_catalog(
            unknown_placement, "catalog-artifact"));
    }
}

void test_strict_timeline_decoder_retains_raw_records() {
    const auto bytes = make_timeline();
    const std::array<std::uint32_t, 2U> target_ids{{42U, 99U}};
    const auto decoded = emberlights::decode_soundswitch_v3_autoloop_timeline(
        bytes, target_ids, "timeline-artifact");
    CHECK(decoded);
    CHECK(decoded.targets.size() == 2U);
    if (decoded.targets.size() != 2U) return;
    const auto& target = decoded.targets[0];
    CHECK(target.present);
    CHECK(target.source_target_id == 42U);
    CHECK(target.intensity_records.size() == 2U);
    CHECK(target.color_records.size() == 1U);
    if (target.intensity_records.size() == 2U) {
        CHECK(target.intensity_records[0].value_raw == 0x40000000U);
        CHECK(target.intensity_records[0].trailing_raw == 0x5AU);
        CHECK(target.intensity_records[1].timestamp_a_ms == 1200U);
        CHECK(target.intensity_records[1].normalized_value() == 1.0F);
        CHECK(target.intensity_records[0].evidence.length == 17U);
    }
    if (target.color_records.size() == 1U) {
        const auto& color = target.color_records.front();
        CHECK(color.rgb_start_raw == 0x7F102030U);
        CHECK(color.direct_start_raw == 0x11223344U);
        CHECK(color.rgb_end_raw == 0x80405060U);
        CHECK(color.direct_end_raw == 0x55667788U);
        CHECK(color.evidence.length == 36U);
    }
    CHECK(!decoded.targets[1].present);

    const auto duplicate = make_timeline(true);
    const std::array<std::uint32_t, 1U> one_target{{42U}};
    CHECK(!emberlights::decode_soundswitch_v3_autoloop_timeline(
        duplicate, one_target, "timeline-artifact"));
    const std::array<std::uint32_t, 2U> duplicate_request{{42U, 42U}};
    CHECK(!emberlights::decode_soundswitch_v3_autoloop_timeline(
        bytes, duplicate_request, "timeline-artifact"));
}

[[nodiscard]] emberlights::AutoloopSourceDocument make_unrelated_source() {
    emberlights::AutoloopSourceDocument source;
    source.assets.push_back({
        "unrelated.asset", "Unrelated Loop", "Preserved test content.",
        {"test"}, "native", 0.25F, "unrelated.program",
        "unrelated.launch", "unrelated.provenance", 1U});
    source.placements.push_back({
        "unrelated.placement", 1U, 1U, "unrelated.asset", {}});
    emberlights::AutoloopProgramDefinition program;
    program.id = "unrelated.program";
    program.length_ticks = emberlights::kMusicalTicksPerQuarter;
    program.targets.push_back({
        "unrelated.target", emberlights::AutoloopTargetKind::Fixture,
        "ir4-1",
        {showcore::Property::Intensity}});
    program.lanes.push_back({
        "unrelated.lane", "unrelated.target", 0U});
    emberlights::AutoloopEventDefinition event;
    event.id = "unrelated.event";
    event.lane_id = "unrelated.lane";
    event.kind = emberlights::AutoloopEventKind::PropertyBlock;
    event.start_tick = 0;
    event.end_tick = program.length_ticks;
    event.property = showcore::Property::Intensity;
    event.value = showcore::PropertyValue::set(0.25F);
    program.events.push_back(std::move(event));
    source.programs.push_back(std::move(program));
    emberlights::AutoloopLaunchProfileDefinition launch;
    launch.id = "unrelated.launch";
    launch.repeat = showcore::AutoloopRepeat::Infinite;
    source.launch_profiles.push_back(std::move(launch));
    emberlights::AutoloopProvenanceDefinition provenance;
    provenance.id = "unrelated.provenance";
    provenance.origin = emberlights::AutoloopProvenanceOrigin::Native;
    provenance.producer_id = "emberlights.test";
    provenance.producer_version = "1";
    provenance.source_object_key = "unrelated.asset";
    provenance.evidence_status = "synthetic";
    source.provenance.push_back(std::move(provenance));
    emberlights::normalize_autoloop_source(source);
    return source;
}

[[nodiscard]] emberlights::ProjectDocument make_current_rig_project() {
    auto project = emberlights::make_starter_project();
    project.id = "soundswitch-2026-red-smooth-test";
    project.name = "SoundSwitch 2026 Red Smooth Test";
    for (std::uint32_t ir4 = 0U; ir4 < 4U; ++ir4) {
        project.fixtures.push_back({
            "ir4-" + std::to_string(ir4 + 1U),
            "IR-4 " + std::to_string(ir4 + 1U),
            std::string(emberlights::kBothLightingBoIr4TenChannelProfileId),
            1U, static_cast<std::uint16_t>(1U + ir4 * 10U),
            {"ir4"}});
    }
    constexpr std::array<std::uint16_t, 4U> tube_bases{{41U, 121U, 201U, 281U}};
    for (std::uint32_t tube = 0U; tube < tube_bases.size(); ++tube) {
        for (std::uint32_t cell = 0U; cell < 16U; ++cell) {
            project.fixtures.push_back({
                "tube-" + std::to_string(tube + 1U) + "-cell-" +
                    std::to_string(cell + 1U),
                "Tube " + std::to_string(tube + 1U) + " Cell " +
                    std::to_string(cell + 1U),
                "builtin.generic.rgb-3ch", 1U,
                static_cast<std::uint16_t>(
                    tube_bases[tube] + cell * 3U),
                {"tube-cell"}});
        }
    }
    CHECK(emberlights::upsert_persisted_autoloop_source(
        project, make_unrelated_source()));
    CHECK(emberlights::validate_project(project).ok());
    return project;
}

template <typename Values>
[[nodiscard]] auto find_id(const Values& values, std::string_view id) {
    return std::find_if(values.begin(), values.end(), [&](const auto& value) {
        return value.id == id;
    });
}

[[nodiscard]] const emberlights::SoundSwitchAutoloopTargetRecords* find_target(
    const emberlights::SoundSwitch2026RedSmoothProposal& proposal,
    std::uint32_t target_id) {
    const auto target = std::find_if(
        proposal.decoded_targets.begin(), proposal.decoded_targets.end(),
        [&](const auto& value) { return value.source_target_id == target_id; });
    return target == proposal.decoded_targets.end() ? nullptr : &*target;
}

void report_proposal_failure(
    const emberlights::SoundSwitch2026RedSmoothProposal& proposal) {
    std::cerr << "proposal failed ("
              << emberlights::soundswitch_2026_red_smooth_error_name(
                     proposal.error)
              << "): " << proposal.message << '\n';
    for (const auto& error : proposal.manifest_validation.errors) {
        std::cerr << "manifest: " << error << '\n';
    }
    for (const auto& issue : proposal.source_validation.issues) {
        std::cerr << "source: " << issue.code << " / " << issue.subject
                  << " / " << issue.message << '\n';
    }
    for (const auto& error : proposal.report_validation.errors) {
        std::cerr << "report: " << error << '\n';
    }
}

void test_authorized_current_2026_vertical_slice() {
    const auto* source_root =
        std::getenv("EMBERLIGHTS_SOUNDSWITCH_2026_SOURCE_ROOT");
    if (source_root == nullptr || *source_root == '\0') {
        std::cout << "authorized current-2026 source integration skipped\n";
        return;
    }

    const auto project = make_current_rig_project();
    const auto proposal = emberlights::build_soundswitch_2026_red_smooth_proposal(
        std::filesystem::path(source_root), project);
    if (!proposal) report_proposal_failure(proposal);
    CHECK(proposal);
    if (!proposal) return;

    CHECK(proposal.output_disabled);
    CHECK(proposal.source_digest.size() == 64U);
    CHECK(proposal.source_validation.ok());
    CHECK(proposal.manifest_validation);
    CHECK(proposal.report_validation);
    CHECK(proposal.corpus_manifest.source_version == "2.10.0.3");
    CHECK(proposal.corpus_manifest.artifacts.size() == 5U);
    CHECK(find_id(proposal.source.assets, "a1") != proposal.source.assets.end());
    CHECK(find_id(proposal.source.assets, "unrelated.asset") !=
          proposal.source.assets.end());
    const auto placement = find_id(proposal.source.placements, "x1");
    CHECK(placement != proposal.source.placements.end());
    if (placement != proposal.source.placements.end()) {
        CHECK(placement->bank == 0U);
        CHECK(placement->slot == 0U);
        CHECK(placement->asset_id == "a1");
    }
    const auto program = find_id(proposal.source.programs, "p1");
    CHECK(program != proposal.source.programs.end());
    if (program != proposal.source.programs.end()) {
        CHECK(program->length_ticks ==
              32 * emberlights::kMusicalTicksPerQuarter);
        CHECK(!program->events.empty());
    }

    const auto* uplight_1 = find_target(proposal, 198U);
    const auto* uplight_4 = find_target(proposal, 201U);
    const auto* tube_1_cell_1 = find_target(proposal, 90U);
    const auto* tube_2_cell_1 = find_target(proposal, 107U);
    CHECK(uplight_1 != nullptr && uplight_1->intensity_records.size() == 41U);
    CHECK(uplight_4 != nullptr && uplight_4->intensity_records.size() == 68U);
    CHECK(tube_1_cell_1 != nullptr &&
          tube_1_cell_1->intensity_records.size() == 27U &&
          tube_1_cell_1->color_records.size() == 16U);
    CHECK(tube_2_cell_1 != nullptr &&
          tube_2_cell_1->intensity_records.size() == 27U &&
          tube_2_cell_1->color_records.size() == 16U);
    CHECK(find_target(proposal, 345U) == nullptr);
    const auto missing_color_count = static_cast<std::size_t>(std::count_if(
        proposal.migration_report.items.begin(),
        proposal.migration_report.items.end(),
        [](const auto& item) {
            return item.item_kind == "colorContext" &&
                item.status == emberlights::MigrationItemStatus::MissingDependency &&
                std::find(
                    item.blockers.begin(), item.blockers.end(),
                    "soundswitch.missing_color_source") != item.blockers.end();
        }));
    CHECK(missing_color_count == 4U);

    emberlights::StudioDocumentService service;
    CHECK(service.replace_document(
        service.generation(), project,
        emberlights::StudioDocumentBoundary::OpenedDocument));
    const auto before = service.snapshot();
    emberlights::AutoloopAuthoringSnapshot authoring;
    authoring.source = proposal.source;
    authoring.generation = 1U;
    authoring.source_digest = proposal.source_digest;
    emberlights::StudioPreviewService preview;
    const auto loaded = preview.load_autoloop_v2(before, authoring);
    if (!loaded) {
        std::cerr << "preview load failed: " << loaded.message << '\n';
        for (const auto& diagnostic : loaded.autoloop_diagnostics) {
            std::cerr << "compile: " << diagnostic.code << " / "
                      << diagnostic.subject << " / " << diagnostic.message
                      << '\n';
        }
    }
    CHECK(loaded.result == emberlights::StudioPreviewResult::Loaded);
    if (loaded) {
        const auto selected = preview.preview_autoloop_v2(
            before.generation, authoring.generation, "x1");
        CHECK(selected.result == emberlights::StudioPreviewResult::Applied);
        CHECK(preview.snapshot().output_disabled);
        CHECK(preview.snapshot().placement_id == "x1");
        CHECK(preview.snapshot().frame_sha256.size() == 64U);
    }

    const auto applied = service.apply_autoloop_source(
        before.generation, before.autoloop_source.stamp, proposal.source);
    CHECK(applied.result == emberlights::StudioMutationResult::Applied);
    const auto committed = service.snapshot();
    CHECK(committed.autoloop_source.stamp.source_digest ==
          proposal.source_digest);

    const auto root = std::filesystem::path(
        "build/soundswitch-2026-red-smooth-round-trip");
    const auto path = root / "project.emberlights";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    CHECK(emberlights::save_project_atomic(path, committed.document, false));
    emberlights::ProjectDocument reopened;
    CHECK(emberlights::load_project(path, reopened, false));
    const auto persisted =
        emberlights::inspect_persisted_autoloop_source(reopened);
    CHECK(persisted);
    CHECK(persisted.stamp.source_digest == proposal.source_digest);
    CHECK(find_id(persisted.source.assets, "unrelated.asset") !=
          persisted.source.assets.end());

    emberlights::StudioDocumentService reopened_service;
    CHECK(reopened_service.replace_document(
        reopened_service.generation(), reopened,
        emberlights::StudioDocumentBoundary::OpenedDocument));
    const auto reopened_snapshot = reopened_service.snapshot();
    const auto reimport = emberlights::build_soundswitch_2026_red_smooth_proposal(
        std::filesystem::path(source_root), reopened_snapshot.document);
    if (!reimport) report_proposal_failure(reimport);
    CHECK(reimport);
    if (reimport) {
        CHECK(reimport.source_digest == proposal.source_digest);
        const auto no_change = reopened_service.apply_autoloop_source(
            reopened_snapshot.generation,
            reopened_snapshot.autoloop_source.stamp,
            reimport.source);
        CHECK(no_change.result == emberlights::StudioMutationResult::NoChange);
    }

    const auto product_project =
        emberlights::create_soundswitch_2026_red_smooth_project(
            std::filesystem::path(source_root));
    if (!product_project) {
        std::cerr << "product import failed: " << product_project.message << '\n';
    }
    CHECK(product_project);
    if (product_project) {
        CHECK(product_project.output_disabled);
        CHECK(product_project.project.fixtures.size() == 68U);
        CHECK(product_project.project.groups.size() == 7U);
        CHECK(std::none_of(
            product_project.project.fixtures.begin(),
            product_project.project.fixtures.end(),
            [](const auto& fixture) { return fixture.id.starts_with("uplight-"); }));
        constexpr std::array<std::uint16_t, 4U> ir4_addresses{{
            1U, 11U, 21U, 31U}};
        for (std::size_t index = 0U; index < ir4_addresses.size(); ++index) {
            const auto id = "ir4-" + std::to_string(index + 1U);
            const auto fixture = std::find_if(
                product_project.project.fixtures.begin(),
                product_project.project.fixtures.end(),
                [&](const auto& value) { return value.id == id; });
            CHECK(fixture != product_project.project.fixtures.end());
            if (fixture != product_project.project.fixtures.end()) {
                CHECK(fixture->address == ir4_addresses[index]);
            }
        }
        const auto product_source =
            emberlights::inspect_persisted_autoloop_source(
                product_project.project);
        CHECK(product_source);
        CHECK(product_source.stamp.present);
        CHECK(product_source.source.placements.size() == 1U);
        CHECK(product_source.source.placements.front().bank == 0U);
        CHECK(product_source.source.placements.front().slot == 0U);
    }
}

}  // namespace

int main() {
    test_strict_catalog_decoder_uses_authored_placement();
    test_strict_timeline_decoder_retains_raw_records();
    test_authorized_current_2026_vertical_slice();
    if (failures != 0) {
        std::cerr << failures << " SoundSwitch 2026 Autoloop test(s) failed\n";
        return 1;
    }
    std::cout << "SoundSwitch 2026 Autoloop tests passed\n";
    return 0;
}
