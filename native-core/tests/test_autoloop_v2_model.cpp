#include "emberlights/autoloop_source.hpp"
#include "emberlights/project.hpp"
#include "emberlights/project_io.hpp"
#include "showcore/autoloop_program.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace allocation_probe {
std::atomic<std::size_t> allocations{0U};
thread_local bool enabled = false;
}  // namespace allocation_probe

void* operator new(std::size_t size) {
    if (allocation_probe::enabled) {
        allocation_probe::allocations.fetch_add(1U, std::memory_order_relaxed);
    }
    if (auto* memory = std::malloc(size == 0U ? 1U : size)) {
        return memory;
    }
    throw std::bad_alloc{};
}

void* operator new[](std::size_t size) {
    return ::operator new(size);
}

void operator delete(void* memory) noexcept {
    std::free(memory);
}

void operator delete[](void* memory) noexcept {
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept {
    std::free(memory);
}

void operator delete[](void* memory, std::size_t) noexcept {
    std::free(memory);
}

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

[[nodiscard]] emberlights::ProjectDocument make_compiled_legacy_project() {
    auto project = emberlights::make_starter_project();
    project.id = "compiled-autoloop-test";
    project.name = "Compiled Autoloop test";
    project.autoloops.clear();
    project.autoloops.push_back({
        "compiled-loop",
        "Compiled loop",
        7U,
        3U,
        4.0F,
        showcore::AutoloopRepeat::Infinite,
        {
            {0.0F, "look-red", showcore::AutoloopTransition::Linear},
            {2.0F, "look-blue", showcore::AutoloopTransition::Linear},
            {3.0F, "look-white", showcore::AutoloopTransition::Cut},
        }});
    return project;
}

struct LegacyCompileFixture {
    std::array<std::uint16_t, 2U> fixture_ids{{1U, 0U}};
    std::array<showcore::LookAssignment, 2U> red{{
        {1U, showcore::Property::Intensity,
         showcore::PropertyValue::set(0.2F)},
        {0U, showcore::Property::Intensity,
         showcore::PropertyValue::set(0.2F)},
    }};
    std::array<showcore::LookAssignment, 2U> blue{{
        {1U, showcore::Property::Intensity,
         showcore::PropertyValue::set(0.8F)},
        {0U, showcore::Property::Intensity,
         showcore::PropertyValue::set(0.8F)},
    }};
    std::array<showcore::LookAssignment, 2U> white{{
        {1U, showcore::Property::Intensity,
         showcore::PropertyValue::set(1.0F)},
        {0U, showcore::Property::Intensity,
         showcore::PropertyValue::set(1.0F)},
    }};
    std::array<showcore::AutoloopTargetBinding, 1U> targets{};
    std::array<showcore::AutoloopReferenceBinding, 3U> references{};

    LegacyCompileFixture() {
        targets[0] = {
            showcore::CompiledAutoloopTargetKind::Master,
            {},
            fixture_ids,
            showcore::autoloop_property_mask(
                showcore::Property::Intensity)};
        references[0] = {
            showcore::CompiledAutoloopReferenceKind::LegacyLook,
            "look-red",
            showcore::CompiledAutoloopTargetKind::Master,
            {},
            red,
            showcore::CompiledAutoloopGeneratorKind::None,
            1U};
        references[1] = {
            showcore::CompiledAutoloopReferenceKind::LegacyLook,
            "look-blue",
            showcore::CompiledAutoloopTargetKind::Master,
            {},
            blue,
            showcore::CompiledAutoloopGeneratorKind::None,
            1U};
        references[2] = {
            showcore::CompiledAutoloopReferenceKind::LegacyLook,
            "look-white",
            showcore::CompiledAutoloopTargetKind::Master,
            {},
            white,
            showcore::CompiledAutoloopGeneratorKind::None,
            1U};
    }

    [[nodiscard]] showcore::AutoloopCompileEnvironment environment() const {
        return {targets, references};
    }
};

[[nodiscard]] bool has_compile_error(
    const showcore::AutoloopCompileResult& result,
    showcore::AutoloopCompileError error,
    showcore::AutoloopArenaKind arena = showcore::AutoloopArenaKind::None) {
    return std::any_of(
        result.diagnostics.begin(), result.diagnostics.end(),
        [error, arena](const auto& diagnostic) {
            return diagnostic.error == error &&
                (arena == showcore::AutoloopArenaKind::None ||
                 diagnostic.arena == arena);
        });
}

void test_compiled_used_content_and_determinism() {
    auto source = emberlights::adapt_format1_autoloops(
        make_compiled_legacy_project());
    LegacyCompileFixture fixture;
    const auto compiled = showcore::compile_autoloop_programs(
        source, fixture.environment());
    CHECK(compiled.ok());
    if (!compiled) {
        return;
    }
    CHECK(compiled.package->format_version() == 1U);
    CHECK(compiled.package->programs().size() == 1U);
    CHECK(compiled.package->target_spans().size() == 1U);
    CHECK(compiled.package->target_fixture_ids().size() == 2U);
    CHECK(compiled.package->events().size() == 3U);
    CHECK(compiled.package->curve_points().empty());
    CHECK(compiled.package->references().size() == 3U);
    CHECK(compiled.package->reference_assignments().size() == 6U);
    CHECK(compiled.package->canonical_bytes().size() > 0U);
    CHECK(compiled.package->digest().size() == 64U);
    CHECK(compiled.package->arena_bytes() >
          compiled.package->canonical_bytes().size());

    const auto* placement = compiled.package->placement({7U, 3U});
    CHECK(placement != nullptr);
    CHECK(placement != nullptr && placement->populated());
    CHECK(placement != nullptr && placement->program_index == 0U);
    const auto* empty = compiled.package->placement({7U, 4U});
    CHECK(empty != nullptr);
    CHECK(empty != nullptr && !empty->populated());

    auto reordered = source;
    std::reverse(reordered.assets.begin(), reordered.assets.end());
    std::reverse(reordered.placements.begin(), reordered.placements.end());
    std::reverse(reordered.programs.begin(), reordered.programs.end());
    std::reverse(
        reordered.launch_profiles.begin(), reordered.launch_profiles.end());
    std::reverse(reordered.provenance.begin(), reordered.provenance.end());
    for (auto& program : reordered.programs) {
        std::reverse(program.targets.begin(), program.targets.end());
        std::reverse(program.lanes.begin(), program.lanes.end());
        std::reverse(program.events.begin(), program.events.end());
    }
    LegacyCompileFixture reordered_fixture;
    std::reverse(
        reordered_fixture.fixture_ids.begin(),
        reordered_fixture.fixture_ids.end());
    std::reverse(
        reordered_fixture.red.begin(), reordered_fixture.red.end());
    std::reverse(
        reordered_fixture.blue.begin(), reordered_fixture.blue.end());
    std::reverse(
        reordered_fixture.white.begin(), reordered_fixture.white.end());
    const auto repeated = showcore::compile_autoloop_programs(
        reordered, reordered_fixture.environment());
    CHECK(repeated.ok());
    if (repeated) {
        CHECK(repeated.package->canonical_bytes().size() ==
              compiled.package->canonical_bytes().size());
        CHECK(std::equal(
            repeated.package->canonical_bytes().begin(),
            repeated.package->canonical_bytes().end(),
            compiled.package->canonical_bytes().begin()));
        CHECK(repeated.package->digest() == compiled.package->digest());
    }

    const std::string raw_id = "compiled-loop.program";
    CHECK(std::search(
              compiled.package->canonical_bytes().begin(),
              compiled.package->canonical_bytes().end(),
              raw_id.begin(), raw_id.end()) ==
          compiled.package->canonical_bytes().end());

    auto shared = source;
    shared.placements.push_back({
        "compiled-loop.second-placement", 9U, 1U, "compiled-loop", {}});
    const auto shared_result = showcore::compile_autoloop_programs(
        shared, fixture.environment());
    CHECK(shared_result.ok());
    if (shared_result) {
        CHECK(shared_result.package->programs().size() == 1U);
        const auto* first = shared_result.package->placement({7U, 3U});
        const auto* second = shared_result.package->placement({9U, 1U});
        CHECK(first != nullptr && second != nullptr);
        CHECK(first != nullptr && second != nullptr &&
              first->program_index == second->program_index);
    }

    auto with_unused_content = source;
    auto unused_program = with_unused_content.programs.front();
    unused_program.id = "unused.program";
    with_unused_content.programs.push_back(std::move(unused_program));
    auto unused_asset = with_unused_content.assets.front();
    unused_asset.id = "unused.asset";
    unused_asset.program_id = "unused.program";
    with_unused_content.assets.push_back(std::move(unused_asset));
    const auto unused_result = showcore::compile_autoloop_programs(
        with_unused_content, fixture.environment());
    CHECK(unused_result.ok());
    if (unused_result) {
        CHECK(unused_result.package->programs().size() == 1U);
        CHECK(unused_result.package->digest() == compiled.package->digest());
    }
}

void check_compiled_matches_legacy(
    const showcore::LayerBuffer& compiled,
    const showcore::LayerStack& legacy) {
    for (std::uint16_t fixture_id = 0U; fixture_id < 2U; ++fixture_id) {
        for (std::size_t property_index = 0U;
             property_index < showcore::kPropertyCount; ++property_index) {
            const auto property = static_cast<showcore::Property>(
                property_index);
            const auto compiled_value = compiled.get(fixture_id, property);
            const auto legacy_value = legacy.resolve(fixture_id, property);
            if (compiled_value.mode == showcore::ValueMode::Release) {
                CHECK(!legacy_value.owned);
                continue;
            }
            CHECK(legacy_value.owned);
            CHECK(legacy_value.mode == compiled_value.mode);
            CHECK(std::abs(legacy_value.value - compiled_value.value) < 0.00001F);
        }
    }
}

void test_compiled_legacy_equivalence_and_no_allocation() {
    const auto source = emberlights::adapt_format1_autoloops(
        make_compiled_legacy_project());
    LegacyCompileFixture fixture;
    const auto compiled = showcore::compile_autoloop_programs(
        source, fixture.environment());
    CHECK(compiled.ok());
    if (!compiled) {
        return;
    }

    const showcore::StaticLook red{
        "red", fixture.red.data(), fixture.red.size()};
    const showcore::StaticLook blue{
        "blue", fixture.blue.data(), fixture.blue.size()};
    const showcore::StaticLook white{
        "white", fixture.white.data(), fixture.white.size()};
    showcore::AutoloopPattern legacy_pattern;
    legacy_pattern.name = "legacy-equivalence";
    legacy_pattern.length_beats = 4.0F;
    CHECK(legacy_pattern.add_step({
        0.0F, &red, showcore::AutoloopTransition::Linear}));
    CHECK(legacy_pattern.add_step({
        2.0F, &blue, showcore::AutoloopTransition::Linear}));
    CHECK(legacy_pattern.add_step({
        3.0F, &white, showcore::AutoloopTransition::Cut}));

    const auto* placement = compiled.package->placement({7U, 3U});
    CHECK(placement != nullptr && placement->populated());
    if (placement == nullptr || !placement->populated()) {
        return;
    }
    showcore::AutoloopEngine legacy_engine;
    showcore::LayerStack legacy_layers;
    showcore::AutoloopProgramEvaluator evaluator;
    showcore::LayerBuffer compiled_layer;
    constexpr std::array<std::int64_t, 11U> ticks{{
        -480, 0, 480, 960, 1919, 1920, 2400, 2879, 2880, 3840, 8640}};
    for (const auto tick : ticks) {
        const auto beat = static_cast<double>(tick) /
            static_cast<double>(emberlights::kMusicalTicksPerQuarter);
        CHECK(legacy_engine.apply(
            legacy_pattern,
            beat,
            showcore::LayerId::ManualAutoloop,
            legacy_layers));
        CHECK(evaluator.evaluate(
            *compiled.package,
            placement->program_index,
            tick,
            compiled_layer));
        check_compiled_matches_legacy(compiled_layer, legacy_layers);
    }

    allocation_probe::allocations.store(0U, std::memory_order_relaxed);
    allocation_probe::enabled = true;
    bool all_evaluations_succeeded = true;
    for (std::int64_t tick = -10000; tick < 10000; ++tick) {
        all_evaluations_succeeded = evaluator.evaluate(
            *compiled.package,
            placement->program_index,
            tick,
            compiled_layer) && all_evaluations_succeeded;
    }
    allocation_probe::enabled = false;
    CHECK(all_evaluations_succeeded);
    CHECK(allocation_probe::allocations.load(std::memory_order_relaxed) == 0U);
}

[[nodiscard]] emberlights::AutoloopSourceDocument make_rich_curve_source() {
    auto source = emberlights::adapt_format1_autoloops(
        make_compiled_legacy_project());
    auto& program = source.programs.front();
    program.events.resize(2U);
    auto& curve = program.events[0];
    curve.kind = emberlights::AutoloopEventKind::PropertyCurve;
    curve.start_tick = 0;
    curve.end_tick = 1920;
    curve.property = showcore::Property::Intensity;
    curve.value = showcore::PropertyValue::release();
    curve.interpolation = emberlights::AutoloopInterpolation::Linear;
    curve.reference_id.clear();
    curve.curve_points = {
        {0, showcore::PropertyValue::set(0.0F)},
        {1920, showcore::PropertyValue::set(1.0F)}};

    auto& block = program.events[1];
    block.kind = emberlights::AutoloopEventKind::PropertyBlock;
    block.start_tick = 1920;
    block.end_tick = 3840;
    block.property = showcore::Property::Red;
    block.value = showcore::PropertyValue::set(0.25F);
    block.interpolation = emberlights::AutoloopInterpolation::Hold;
    block.reference_id.clear();
    block.curve_points.clear();
    program.targets.front().required_properties = {
        showcore::Property::Intensity,
        showcore::Property::Red};
    return source;
}

void test_rich_evaluator_and_fail_closed_compilation() {
    const auto source = make_rich_curve_source();
    std::array<std::uint16_t, 2U> fixture_ids{{0U, 1U}};
    std::array<showcore::AutoloopTargetBinding, 1U> targets{{{
        showcore::CompiledAutoloopTargetKind::Master,
        {},
        fixture_ids,
        showcore::all_autoloop_property_mask()}}};
    const showcore::AutoloopCompileEnvironment environment{targets, {}};
    const auto compiled = showcore::compile_autoloop_programs(
        source, environment);
    CHECK(compiled.ok());
    if (compiled) {
        CHECK(compiled.package->events().size() == 2U);
        CHECK(compiled.package->curve_points().size() == 2U);
        CHECK(compiled.package->references().empty());
        showcore::AutoloopProgramEvaluator evaluator;
        showcore::LayerBuffer output;
        CHECK(evaluator.evaluate(*compiled.package, 0U, 960, output));
        CHECK(output.get(0U, showcore::Property::Intensity).mode ==
              showcore::ValueMode::Set);
        CHECK(std::abs(
            output.get(0U, showcore::Property::Intensity).value - 0.5F) <
            0.00001F);
        CHECK(evaluator.evaluate(*compiled.package, 0U, 2400, output));
        CHECK(output.get(1U, showcore::Property::Red).mode ==
              showcore::ValueMode::Set);
        CHECK(std::abs(output.get(1U, showcore::Property::Red).value -
                       0.25F) < 0.00001F);
    }

    auto limits = showcore::AutoloopCompileLimits{};
    limits.maximum_events = 1U;
    const auto overflow = showcore::compile_autoloop_programs(
        source, environment, limits);
    CHECK(!overflow.ok());
    CHECK(overflow.package == nullptr);
    CHECK(has_compile_error(
        overflow,
        showcore::AutoloopCompileError::CapacityExceeded,
        showcore::AutoloopArenaKind::Events));

    auto byte_limits = showcore::AutoloopCompileLimits{};
    byte_limits.maximum_canonical_bytes = 1U;
    const auto byte_overflow = showcore::compile_autoloop_programs(
        source, environment, byte_limits);
    CHECK(!byte_overflow.ok());
    CHECK(byte_overflow.package == nullptr);
    CHECK(has_compile_error(
        byte_overflow,
        showcore::AutoloopCompileError::CapacityExceeded,
        showcore::AutoloopArenaKind::CanonicalBytes));

    std::array<showcore::AutoloopTargetBinding, 1U> insufficient{{{
        showcore::CompiledAutoloopTargetKind::Master,
        {},
        fixture_ids,
        showcore::autoloop_property_mask(showcore::Property::Red)}}};
    const auto capability_failure = showcore::compile_autoloop_programs(
        source, {insufficient, {}});
    CHECK(!capability_failure.ok());
    CHECK(capability_failure.package == nullptr);
    CHECK(has_compile_error(
        capability_failure,
        showcore::AutoloopCompileError::MissingCapability));

    LegacyCompileFixture legacy_fixture;
    const auto legacy_source = emberlights::adapt_format1_autoloops(
        make_compiled_legacy_project());
    const auto missing_reference = showcore::compile_autoloop_programs(
        legacy_source, {legacy_fixture.targets, {}});
    CHECK(!missing_reference.ok());
    CHECK(missing_reference.package == nullptr);
    CHECK(has_compile_error(
        missing_reference,
        showcore::AutoloopCompileError::MissingReference,
        showcore::AutoloopArenaKind::References));
}

void test_generator_evaluator_and_version_gates() {
    auto source = emberlights::adapt_format1_autoloops(
        make_compiled_legacy_project());
    auto& program = source.programs.front();
    program.events.resize(1U);
    auto& event = program.events.front();
    event.kind = emberlights::AutoloopEventKind::Effect;
    event.start_tick = 0;
    event.end_tick = program.length_ticks;
    event.property = showcore::Property::Intensity;
    event.value = showcore::PropertyValue::release();
    event.reference_id = "effect.sine";
    event.curve_points.clear();
    event.generator.rate_start = 1.0F;
    event.generator.rate_end = 1.0F;
    event.generator.size_start = 1.0F;
    event.generator.size_end = 1.0F;
    event.generator.phase = 0.0F;
    event.generator.spread = 0.0F;
    event.generator.base_primary = 0.0F;
    event.generator.base_secondary = 1.0F;
    event.generator.seed = 0U;
    program.targets.front().required_properties = {
        showcore::Property::Intensity};

    std::array<std::uint16_t, 1U> fixture_ids{{0U}};
    std::array<showcore::AutoloopTargetBinding, 1U> targets{{{
        showcore::CompiledAutoloopTargetKind::Master,
        {},
        fixture_ids,
        showcore::autoloop_property_mask(
            showcore::Property::Intensity)}}};
    std::array<showcore::AutoloopReferenceBinding, 1U> references{{{
        showcore::CompiledAutoloopReferenceKind::Effect,
        "effect.sine",
        showcore::CompiledAutoloopTargetKind::Master,
        {},
        {},
        showcore::CompiledAutoloopGeneratorKind::Sine,
        1U}}};
    const auto compiled = showcore::compile_autoloop_programs(
        source, {targets, references});
    CHECK(compiled.ok());
    if (compiled) {
        showcore::AutoloopProgramEvaluator evaluator;
        showcore::LayerBuffer output;
        CHECK(evaluator.evaluate(*compiled.package, 0U, 0, output));
        CHECK(std::abs(output.get(
            0U, showcore::Property::Intensity).value - 0.5F) < 0.00001F);
        CHECK(evaluator.evaluate(*compiled.package, 0U, 240, output));
        CHECK(std::abs(output.get(
            0U, showcore::Property::Intensity).value - 1.0F) < 0.00001F);
    }

    auto unknown_event_version = source;
    unknown_event_version.programs.front().events.front().payload_version = 2U;
    const auto rejected_event = showcore::compile_autoloop_programs(
        unknown_event_version, {targets, references});
    CHECK(!rejected_event.ok());
    CHECK(has_compile_error(
        rejected_event,
        showcore::AutoloopCompileError::UnsupportedPayload,
        showcore::AutoloopArenaKind::Events));

    references.front().semantic_version = 2U;
    const auto rejected_reference = showcore::compile_autoloop_programs(
        source, {targets, references});
    CHECK(!rejected_reference.ok());
    CHECK(has_compile_error(
        rejected_reference,
        showcore::AutoloopCompileError::UnsupportedPayload,
        showcore::AutoloopArenaKind::References));
}

}  // namespace

int main() {
    test_format1_tick_adapter();
    test_format1_source_shape_and_identity();
    test_canonical_serialization_and_digest();
    test_validation_diagnostics();
    test_format1_document_is_not_mutated();
    test_compiled_used_content_and_determinism();
    test_compiled_legacy_equivalence_and_no_allocation();
    test_rich_evaluator_and_fail_closed_compilation();
    test_generator_evaluator_and_version_gates();

    if (failures != 0) {
        std::cerr << failures
                  << " Autoloop V2 model/compiled-program test(s) failed\n";
        return 1;
    }
    std::cout << "Autoloop V2 model and compiled-program tests passed\n";
    return 0;
}
