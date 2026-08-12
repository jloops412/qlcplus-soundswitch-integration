#include "emberlights/autoloop_source.hpp"
#include "emberlights/project.hpp"
#include "emberlights/project_io.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
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

[[nodiscard]] bool has_issue(
    const emberlights::AutoloopSourceValidation& validation,
    std::string_view code) {
    return std::any_of(
        validation.issues.begin(), validation.issues.end(),
        [code](const auto& issue) { return issue.code == code; });
}

[[nodiscard]] bool replace_record_field(
    std::string& serialized,
    std::string_view record_kind,
    std::size_t field_index,
    std::string_view replacement) {
    const auto record = serialized.find(std::string(record_kind) + '\t');
    if (record == std::string::npos) {
        return false;
    }
    auto field = record;
    for (std::size_t index = 0U; index < field_index; ++index) {
        field = serialized.find('\t', field);
        if (field == std::string::npos) {
            return false;
        }
        ++field;
    }
    const auto end = serialized.find_first_of("\t\n", field);
    if (end == std::string::npos) {
        return false;
    }
    serialized.replace(field, end - field, replacement);
    return true;
}

[[nodiscard]] emberlights::ProjectDocument make_format1_project() {
    auto project = emberlights::make_starter_project();
    project.id = "autoloop-v2-adapter-test";
    project.name = "Autoloop V2 adapter test";
    project.autoloops.push_back({
        "legacy-alpha",
        "Legacy\tAlpha\n%",
        7U,
        3U,
        4.0F,
        showcore::AutoloopRepeat::TrackDuration,
        {
            {0.0F, "look-red", showcore::AutoloopTransition::Cut},
            {1.5F, "look-blue", showcore::AutoloopTransition::Linear},
            {3.25F, "look-white", showcore::AutoloopTransition::Cut},
        }});
    project.autoloops.push_back({
        "legacy-beta",
        "Legacy Beta",
        2U,
        9U,
        2.0F,
        showcore::AutoloopRepeat::Once,
        {
            {0.0F, "look-low", showcore::AutoloopTransition::Cut},
            {1.0F, "look-high", showcore::AutoloopTransition::Linear},
        }});
    return project;
}

void test_format1_tick_adapter() {
    emberlights::MusicalTick tick = 0;
    CHECK(emberlights::format1_beat_to_musical_tick(0.0F, tick));
    CHECK(tick == 0);
    CHECK(emberlights::format1_beat_to_musical_tick(1.25F, tick));
    CHECK(tick == 1200);
    CHECK(emberlights::format1_beat_to_musical_tick(-0.5F, tick));
    CHECK(tick == -480);
    CHECK(!emberlights::format1_beat_to_musical_tick(
        std::numeric_limits<float>::quiet_NaN(), tick));
    CHECK(!emberlights::format1_beat_to_musical_tick(
        std::numeric_limits<float>::infinity(), tick));

    const auto positive_boundary =
        static_cast<float>(0x1p63L / 960.0L);
    CHECK(!emberlights::format1_beat_to_musical_tick(
        positive_boundary, tick));
    CHECK(emberlights::format1_beat_to_musical_tick(
        std::nextafter(positive_boundary, 0.0F), tick));
    CHECK(tick > 0);

    const auto negative_boundary =
        static_cast<float>(-0x1p63L / 960.0L);
    CHECK(!emberlights::format1_beat_to_musical_tick(
        negative_boundary, tick));
    CHECK(emberlights::format1_beat_to_musical_tick(
        std::nextafter(negative_boundary, 0.0F), tick));
    CHECK(tick < 0);
    CHECK(!emberlights::format1_beat_to_musical_tick(
        std::numeric_limits<float>::max(), tick));
    CHECK(!emberlights::format1_beat_to_musical_tick(
        std::numeric_limits<float>::lowest(), tick));
}

void test_format1_source_shape_and_identity() {
    const auto project = make_format1_project();
    const auto source = emberlights::adapt_format1_autoloops(project);

    CHECK(emberlights::validate_autoloop_source(source).ok());
    CHECK(source.assets.size() == 2U);
    CHECK(source.placements.size() == 2U);
    CHECK(source.programs.size() == 2U);
    CHECK(source.launch_profiles.size() == 2U);
    CHECK(source.provenance.size() == 2U);

    const auto asset = std::find_if(
        source.assets.begin(), source.assets.end(),
        [](const auto& value) { return value.id == "legacy-alpha"; });
    CHECK(asset != source.assets.end());
    if (asset == source.assets.end()) {
        return;
    }
    CHECK(asset->name == "Legacy\tAlpha\n%");
    CHECK(asset->program_id == "legacy-alpha.program");
    CHECK(asset->launch_profile_id == "legacy-alpha.launch");
    CHECK(asset->provenance_id == "legacy-alpha.provenance");

    const auto placement = std::find_if(
        source.placements.begin(), source.placements.end(),
        [](const auto& value) { return value.asset_id == "legacy-alpha"; });
    CHECK(placement != source.placements.end());
    if (placement != source.placements.end()) {
        CHECK(placement->bank == 7U);
        CHECK(placement->slot == 3U);
    }

    const auto program = std::find_if(
        source.programs.begin(), source.programs.end(),
        [](const auto& value) { return value.id == "legacy-alpha.program"; });
    CHECK(program != source.programs.end());
    if (program != source.programs.end()) {
        CHECK(program->length_ticks == 3840);
        CHECK(program->targets.size() == 1U);
        CHECK(program->targets.front().kind ==
              emberlights::AutoloopTargetKind::Master);
        CHECK(program->lanes.size() == 1U);
        CHECK(program->events.size() == 3U);
        if (program->events.size() == 3U) {
            CHECK(program->events[0].start_tick == 0);
            CHECK(program->events[0].end_tick == 1440);
            CHECK(program->events[0].reference_id == "look-red");
            CHECK(program->events[1].start_tick == 1440);
            CHECK(program->events[1].end_tick == 3120);
            CHECK(program->events[1].legacy_transition ==
                  showcore::AutoloopTransition::Linear);
            CHECK(program->events[2].start_tick == 3120);
            CHECK(program->events[2].end_tick == 3840);
        }
    }

    const auto launch = std::find_if(
        source.launch_profiles.begin(), source.launch_profiles.end(),
        [](const auto& value) { return value.id == "legacy-alpha.launch"; });
    CHECK(launch != source.launch_profiles.end());
    if (launch != source.launch_profiles.end()) {
        CHECK(launch->repeat == showcore::AutoloopRepeat::TrackDuration);
        CHECK(launch->track_boundary_required);
    }

    auto moved_project = project;
    moved_project.autoloops[0].bank = 9U;
    moved_project.autoloops[0].slot = 4U;
    const auto moved = emberlights::adapt_format1_autoloops(moved_project);
    const auto moved_asset = std::find_if(
        moved.assets.begin(), moved.assets.end(),
        [](const auto& value) { return value.id == "legacy-alpha"; });
    CHECK(moved_asset != moved.assets.end());
    if (moved_asset != moved.assets.end()) {
        CHECK(moved_asset->program_id == asset->program_id);
        CHECK(moved_asset->launch_profile_id == asset->launch_profile_id);
        CHECK(moved_asset->provenance_id == asset->provenance_id);
    }
    CHECK(emberlights::autoloop_source_digest(moved) !=
          emberlights::autoloop_source_digest(source));
}

void test_canonical_serialization_and_digest() {
    const auto source = emberlights::adapt_format1_autoloops(
        make_format1_project());
    const auto serialized = emberlights::serialize_autoloop_source(source);
    const auto digest = emberlights::autoloop_source_digest(source);
    CHECK(!serialized.empty());
    CHECK(digest.size() == 64U);
    CHECK(serialized.find("Legacy%09Alpha%0A%25") != std::string::npos);

    emberlights::AutoloopSourceDocument parsed;
    const auto result = emberlights::parse_autoloop_source(serialized, parsed);
    CHECK(static_cast<bool>(result));
    CHECK(emberlights::serialize_autoloop_source(parsed) == serialized);
    CHECK(emberlights::autoloop_source_digest(parsed) == digest);

    auto shuffled = source;
    std::reverse(shuffled.assets.begin(), shuffled.assets.end());
    std::reverse(shuffled.placements.begin(), shuffled.placements.end());
    std::reverse(shuffled.programs.begin(), shuffled.programs.end());
    std::reverse(
        shuffled.launch_profiles.begin(), shuffled.launch_profiles.end());
    std::reverse(shuffled.provenance.begin(), shuffled.provenance.end());
    for (auto& asset : shuffled.assets) {
        std::reverse(asset.tags.begin(), asset.tags.end());
    }
    for (auto& program : shuffled.programs) {
        std::reverse(program.targets.begin(), program.targets.end());
        std::reverse(program.lanes.begin(), program.lanes.end());
        std::reverse(program.events.begin(), program.events.end());
    }
    CHECK(emberlights::serialize_autoloop_source(shuffled) == serialized);
    CHECK(emberlights::autoloop_source_digest(shuffled) == digest);

    const auto unknown = serialized + "FUTURE_RECORD\topaque\n";
    const auto unknown_result =
        emberlights::parse_autoloop_source(unknown, parsed);
    CHECK(unknown_result.error ==
          emberlights::AutoloopSourceIoError::InvalidRecord);

    auto sentinel_property = serialized;
    CHECK(replace_record_field(
        sentinel_property,
        "EVENT",
        7U,
        std::to_string(showcore::kPropertyCount)));
    const auto sentinel_result =
        emberlights::parse_autoloop_source(sentinel_property, parsed);
    CHECK(sentinel_result.error ==
          emberlights::AutoloopSourceIoError::InvalidValue);
}

void test_validation_diagnostics() {
    auto duplicate_tag = emberlights::adapt_format1_autoloops(
        make_format1_project());
    duplicate_tag.assets.front().tags.push_back(
        duplicate_tag.assets.front().tags.front());
    const auto tag_validation =
        emberlights::validate_autoloop_source(duplicate_tag);
    CHECK(!tag_validation.ok());
    CHECK(has_issue(tag_validation, "autoloop.asset.tag"));
    CHECK(emberlights::serialize_autoloop_source(duplicate_tag).empty());

    auto conflict = emberlights::adapt_format1_autoloops(
        make_format1_project());
    auto& program = conflict.programs.front();
    auto event = program.events.front();
    event.id += ".conflict";
    program.events.push_back(std::move(event));
    const auto conflict_validation =
        emberlights::validate_autoloop_source(conflict);
    CHECK(!conflict_validation.ok());
    CHECK(has_issue(
        conflict_validation, "autoloop.event.ownershipConflict"));

    auto invalid_repeat = emberlights::adapt_format1_autoloops(
        make_format1_project());
    invalid_repeat.launch_profiles.front().repeat =
        static_cast<showcore::AutoloopRepeat>(0xFFU);
    const auto repeat_validation =
        emberlights::validate_autoloop_source(invalid_repeat);
    CHECK(!repeat_validation.ok());
    CHECK(has_issue(repeat_validation, "autoloop.launch.value"));
    CHECK(emberlights::serialize_autoloop_source(invalid_repeat).empty());

    auto invalid_value_mode = emberlights::adapt_format1_autoloops(
        make_format1_project());
    invalid_value_mode.programs.front().events.front().value.mode =
        static_cast<showcore::ValueMode>(0xFFU);
    const auto value_validation =
        emberlights::validate_autoloop_source(invalid_value_mode);
    CHECK(!value_validation.ok());
    CHECK(has_issue(value_validation, "autoloop.event.value"));
    CHECK(emberlights::serialize_autoloop_source(invalid_value_mode).empty());

    auto invalid_transition = emberlights::adapt_format1_autoloops(
        make_format1_project());
    invalid_transition.programs.front().events.front().legacy_transition =
        static_cast<showcore::AutoloopTransition>(0xFFU);
    const auto transition_validation =
        emberlights::validate_autoloop_source(invalid_transition);
    CHECK(!transition_validation.ok());
    CHECK(has_issue(
        transition_validation, "autoloop.event.legacyTransition"));
    CHECK(emberlights::serialize_autoloop_source(invalid_transition).empty());
}

void test_format1_document_is_not_mutated() {
    auto project = make_format1_project();
    project.unknown_records.push_back(
        "FUTURE_AUTOLOOP\topaque-format-1-payload");
    const auto before = emberlights::serialize_project(project);
    const auto unknown_before = project.unknown_records;

    const auto source = emberlights::adapt_format1_autoloops(project);
    CHECK(!source.assets.empty());
    CHECK(project.unknown_records == unknown_before);
    CHECK(emberlights::serialize_project(project) == before);
}

}  // namespace

int main() {
    test_format1_tick_adapter();
    test_format1_source_shape_and_identity();
    test_canonical_serialization_and_digest();
    test_validation_diagnostics();
    test_format1_document_is_not_mutated();

    if (failures != 0) {
        std::cerr << failures << " Autoloop V2 model test(s) failed\n";
        return 1;
    }
    std::cout << "Autoloop V2 source model tests passed\n";
    return 0;
}
