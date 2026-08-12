#include "emberlights/project.hpp"
#include "emberlights/studio_color.hpp"
#include "emberlights/studio_preview.hpp"

#include <algorithm>
#include <cmath>
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

[[nodiscard]] bool near(float first, float second, float tolerance = 0.002F) {
    return std::fabs(first - second) <= tolerance;
}

[[nodiscard]] const emberlights::FixtureProfileDefinition* profile(
    const emberlights::ProjectDocument& project,
    std::string_view id) {
    const auto found = std::find_if(
        project.fixture_profiles.begin(), project.fixture_profiles.end(),
        [&](const auto& candidate) { return candidate.id == id; });
    return found == project.fixture_profiles.end() ? nullptr : &*found;
}

[[nodiscard]] const emberlights::StudioColorPropertyAssignment* assignment(
    const emberlights::StudioColorRealization& realization,
    showcore::Property property) {
    const auto found = std::find_if(
        realization.assignments.begin(), realization.assignments.end(),
        [&](const auto& candidate) { return candidate.property == property; });
    return found == realization.assignments.end() ? nullptr : &*found;
}

emberlights::ProjectDocument make_authoring_project() {
    auto project = emberlights::make_starter_project();
    project.id = "studio-authoring-test";
    project.name = "Studio Authoring Test";
    project.fixtures = {
        {"wash-left", "Wash Left", "builtin.generic.rgbwauvd-7ch", 1U, 1U, {"wash"}},
        {"wash-right", "Wash Right", "builtin.generic.rgbwauvd-7ch", 1U, 8U, {"wash"}}};
    project.groups = {{"all-washes", "All Washes", {"wash-left", "wash-right"}}};

    emberlights::StudioColor red;
    red.rgb = {1.0F, 0.0F, 0.0F};
    red.intensity = 0.5F;
    emberlights::LookDefinition red_look{"look-red", "Red", 1000U, {}};
    CHECK(emberlights::apply_studio_color_to_look(
        project, "all-washes", red, red_look));

    emberlights::StudioColor blue;
    blue.rgb = {0.0F, 0.0F, 1.0F};
    blue.intensity = 0.5F;
    emberlights::LookDefinition blue_look{"look-blue", "Blue", 0U, {}};
    CHECK(emberlights::apply_studio_color_to_look(
        project, "all-washes", blue, blue_look));

    project.looks = {red_look, blue_look};
    emberlights::AutoloopDefinition loop;
    loop.id = "loop-red-blue";
    loop.name = "Red Blue Fade";
    loop.bank = 0U;
    loop.slot = 0U;
    loop.length_beats = 4.0F;
    loop.repeat = showcore::AutoloopRepeat::Infinite;
    loop.steps = {
        {0.0F, "look-red", showcore::AutoloopTransition::Linear},
        {2.0F, "look-blue", showcore::AutoloopTransition::Linear}};
    project.autoloops = {loop};
    return project;
}

void test_picker_color_spaces() {
    const auto red = emberlights::studio_rgb_from_hsv({0.0F, 1.0F, 1.0F});
    CHECK(near(red.red, 1.0F));
    CHECK(near(red.green, 0.0F));
    CHECK(near(red.blue, 0.0F));

    const auto cyan = emberlights::studio_rgb_from_hsv({180.0F, 1.0F, 1.0F});
    const auto cyan_hsl = emberlights::studio_hsl_from_rgb(cyan);
    CHECK(near(cyan_hsl.hue_degrees, 180.0F));
    CHECK(near(cyan_hsl.saturation, 1.0F));
    CHECK(near(cyan_hsl.lightness, 0.5F));
    const auto cyan_round_trip = emberlights::studio_rgb_from_hsl(cyan_hsl);
    CHECK(near(cyan_round_trip.red, cyan.red));
    CHECK(near(cyan_round_trip.green, cyan.green));
    CHECK(near(cyan_round_trip.blue, cyan.blue));

    const auto cmy = emberlights::studio_cmy_from_rgb({0.25F, 0.5F, 0.75F});
    CHECK(near(cmy.cyan, 0.75F));
    CHECK(near(cmy.magenta, 0.5F));
    CHECK(near(cmy.yellow, 0.25F));
    const auto cmy_round_trip = emberlights::studio_rgb_from_cmy(cmy);
    CHECK(near(cmy_round_trip.red, 0.25F));
    CHECK(near(cmy_round_trip.green, 0.5F));
    CHECK(near(cmy_round_trip.blue, 0.75F));

    emberlights::StudioRgbColor brand;
    CHECK(emberlights::parse_studio_hex_color("#D7B185", brand));
    CHECK(emberlights::format_studio_hex_color(brand) == "#D7B185");
    emberlights::StudioRgbColor short_hex;
    CHECK(emberlights::parse_studio_hex_color("#0AF", short_hex));
    CHECK(emberlights::format_studio_hex_color(short_hex) == "#00AAFF");
    CHECK(!emberlights::parse_studio_hex_color("#GG0000", short_hex));

    const auto warm = emberlights::studio_rgb_from_temperature(2200.0F);
    const auto cool = emberlights::studio_rgb_from_temperature(10000.0F);
    CHECK(warm.red > warm.blue);
    CHECK(cool.blue > warm.blue);
    const auto green_tint = emberlights::studio_rgb_from_temperature(6500.0F, -1.0F);
    const auto magenta_tint = emberlights::studio_rgb_from_temperature(6500.0F, 1.0F);
    CHECK(green_tint.red < magenta_tint.red);
    CHECK(green_tint.blue > magenta_tint.blue);
}

void test_palette_transactions() {
    emberlights::StudioColorPalette palette;
    emberlights::StudioColorSwatch gold{"gold", "Gold", {}};
    gold.color.rgb = {0.843F, 0.694F, 0.522F};
    CHECK(palette.upsert(gold) == emberlights::StudioPaletteMutationResult::Added);
    CHECK(palette.upsert(gold) == emberlights::StudioPaletteMutationResult::NoChange);
    auto updated = gold;
    updated.name = "Love & Light Gold";
    CHECK(palette.upsert(updated) == emberlights::StudioPaletteMutationResult::Updated);
    CHECK(palette.find("gold") != nullptr);

    emberlights::StudioColorSwatch blue{"blue", "Blue", {}};
    blue.color.rgb = {0.0F, 0.0F, 1.0F};
    CHECK(palette.upsert(blue) == emberlights::StudioPaletteMutationResult::Added);
    CHECK(palette.move("blue", 0U) == emberlights::StudioPaletteMutationResult::Moved);
    CHECK(palette.swatches().front().id == "blue");
    CHECK(palette.remove("gold") == emberlights::StudioPaletteMutationResult::Removed);
    CHECK(palette.remove("gold") == emberlights::StudioPaletteMutationResult::Missing);

    emberlights::StudioColorSwatch invalid;
    invalid.id = "invalid";
    invalid.name = "Invalid";
    invalid.color.rgb.red = 2.0F;
    CHECK(palette.upsert(invalid) == emberlights::StudioPaletteMutationResult::Invalid);
}

void test_fixture_aware_realization() {
    const auto project = emberlights::make_starter_project();
    const auto* rgbwauv = profile(project, "builtin.generic.rgbwauvd-7ch");
    CHECK(rgbwauv != nullptr);
    if (rgbwauv == nullptr) {
        return;
    }
    emberlights::StudioColor color;
    color.rgb = {0.2F, 0.4F, 0.8F};
    color.white = 0.1F;
    color.amber = 0.3F;
    color.uv = 0.5F;
    color.intensity = 0.75F;
    const auto realized = emberlights::realize_studio_color(*rgbwauv, color);
    CHECK(realized.status == emberlights::StudioColorRealizationStatus::Exact);
    CHECK(realized.assignments.size() == 7U);
    CHECK(near(assignment(realized, showcore::Property::Intensity)->value.value, 0.75F));
    CHECK(near(assignment(realized, showcore::Property::Blue)->value.value, 0.8F));
    CHECK(near(assignment(realized, showcore::Property::UV)->value.value, 0.5F));

    emberlights::FixtureProfileDefinition cmy;
    cmy.id = "test.cmy";
    cmy.manufacturer = "Test";
    cmy.model = "CMY";
    cmy.mode = "3ch";
    cmy.name = "Test CMY";
    cmy.footprint = 3U;
    cmy.channels = {
        {showcore::Property::Cyan, 0U},
        {showcore::Property::Magenta, 1U},
        {showcore::Property::Yellow, 2U}};
    emberlights::StudioColor cmy_color;
    cmy_color.rgb = {0.2F, 0.4F, 0.8F};
    const auto cmy_realized = emberlights::realize_studio_color(cmy, cmy_color);
    CHECK(cmy_realized.status == emberlights::StudioColorRealizationStatus::Exact);
    CHECK(near(assignment(cmy_realized, showcore::Property::Cyan)->value.value, 0.8F));
    CHECK(near(assignment(cmy_realized, showcore::Property::Magenta)->value.value, 0.6F));
    CHECK(near(assignment(cmy_realized, showcore::Property::Yellow)->value.value, 0.2F));

    emberlights::FixtureProfileDefinition wheel = cmy;
    wheel.id = "test.wheel";
    wheel.model = "Wheel";
    wheel.name = "Test Wheel";
    wheel.footprint = 1U;
    wheel.channels = {{
        showcore::Property::ColorWheel,
        0U,
        -1,
        showcore::ChannelEncoding::Discrete8,
        0U,
        255U,
        0U}};
    const auto unsupported = emberlights::realize_studio_color(wheel, color);
    CHECK(unsupported.status == emberlights::StudioColorRealizationStatus::Unsupported);
    CHECK(!unsupported.usable());

    emberlights::FixtureProfileDefinition white = wheel;
    white.id = "test.white";
    white.model = "White";
    white.name = "Test White";
    white.channels.front().property = showcore::Property::White;
    white.channels.front().encoding = showcore::ChannelEncoding::Linear8;
    const auto white_fallback = emberlights::realize_studio_color(white, color);
    CHECK(white_fallback.status == emberlights::StudioColorRealizationStatus::Degraded);
    CHECK(white_fallback.usable());
    CHECK(assignment(white_fallback, showcore::Property::White) != nullptr);
}

void test_static_look_color_authoring() {
    auto project = make_authoring_project();
    emberlights::LookDefinition look{"look-picker", "Picker Look", 400U, {}};
    look.assignments.push_back({
        "wash-left", showcore::Property::Strobe, showcore::PropertyValue::set(0.2F)});
    emberlights::StudioColor gold;
    CHECK(emberlights::parse_studio_hex_color("#D7B185", gold.rgb));
    gold.white = 0.1F;
    gold.amber = 0.3F;
    gold.intensity = 0.8F;

    const auto first = emberlights::apply_studio_color_to_look(
        project, "all-washes", gold, look);
    CHECK(first.result == emberlights::StudioColorApplyResult::Applied);
    CHECK(first.fixtures_targeted == 2U);
    CHECK(first.fixtures_realized == 2U);
    CHECK(first.assignments_written == 14U);
    CHECK(look.assignments.size() == 15U);
    CHECK(look.assignments.front().property == showcore::Property::Strobe);

    const auto unchanged = emberlights::apply_studio_color_to_look(
        project, "all-washes", gold, look);
    CHECK(unchanged.result == emberlights::StudioColorApplyResult::NoChange);
    CHECK(look.assignments.size() == 15U);

    gold.rgb = {0.0F, 1.0F, 0.0F};
    const auto changed = emberlights::apply_studio_color_to_look(
        project, "wash-left", gold, look);
    CHECK(changed.result == emberlights::StudioColorApplyResult::Applied);
    CHECK(changed.fixtures_realized == 1U);
    CHECK(look.assignments.size() == 15U);
    const auto missing = emberlights::apply_studio_color_to_look(
        project, "missing-target", gold, look);
    CHECK(missing.result == emberlights::StudioColorApplyResult::TargetMissing);

    project.fixture_profiles.push_back({
        "test.wheel-only",
        "Test",
        "Wheel",
        "1ch",
        "Test Wheel",
        showcore::FixtureProfileSource::Local,
        "1",
        1U,
        {{showcore::Property::ColorWheel, 0U, -1,
          showcore::ChannelEncoding::Discrete8, 0U, 255U, 0U}}});
    project.fixtures.push_back({
        "wheel-only", "Wheel Only", "test.wheel-only", 1U, 15U, {"effect"}});
    project.groups.push_back({
        "mixed-color-support", "Mixed", {"wash-left", "wheel-only"}});
    look.assignments.push_back({
        "wheel-only", showcore::Property::ColorWheel,
        showcore::PropertyValue::set(0.4F)});
    const auto mixed = emberlights::apply_studio_color_to_look(
        project, "mixed-color-support", gold, look);
    CHECK(mixed.result == emberlights::StudioColorApplyResult::Applied);
    CHECK(mixed.fixtures_realized == 1U);
    const auto wheel_assignment = std::find_if(
        look.assignments.begin(), look.assignments.end(), [](const auto& candidate) {
            return candidate.fixture_id == "wheel-only" &&
                candidate.property == showcore::Property::ColorWheel;
        });
    CHECK(wheel_assignment != look.assignments.end());
}

void test_no_output_realtime_preview() {
    const auto project = make_authoring_project();
    CHECK(emberlights::validate_project(project).ok());
    emberlights::StudioDocumentSnapshot document;
    document.document = project;
    document.generation = 7U;

    emberlights::StudioPreviewService preview;
    const auto loaded = preview.load(document);
    CHECK(loaded.result == emberlights::StudioPreviewResult::Loaded);
    CHECK(preview.snapshot().generation == 7U);
    CHECK(preview.snapshot().fixtures.size() == 2U);

    const auto red = preview.preview_look(7U, "look-red", 100U, false);
    CHECK(red.result == emberlights::StudioPreviewResult::Applied);
    CHECK(preview.snapshot().content_kind ==
        emberlights::StudioPreviewContentKind::StaticLook);
    CHECK(preview.snapshot().fixtures.front().dmx_values.size() == 7U);
    if (preview.snapshot().fixtures.front().dmx_values.size() == 7U) {
        CHECK(preview.snapshot().fixtures.front().dmx_values[0] == 128U);
        CHECK(preview.snapshot().fixtures.front().dmx_values[1] == 255U);
        CHECK(preview.snapshot().fixtures.front().dmx_values[2] == 0U);
        CHECK(preview.snapshot().fixtures.front().dmx_values[3] == 0U);
    }
    CHECK(near(preview.snapshot().fixtures.front().display_rgb.red, 0.5F, 0.01F));

    emberlights::StudioColor draft_blue;
    draft_blue.rgb = {0.0F, 0.0F, 1.0F};
    draft_blue.intensity = 0.75F;
    CHECK(preview.preview_draft_color(7U, "wash-left", draft_blue));
    CHECK(preview.snapshot().draft_color_active);
    CHECK(near(preview.snapshot().fixtures.front().display_rgb.blue, 0.75F, 0.01F));
    CHECK(near(preview.snapshot().fixtures.back().display_rgb.red, 0.5F, 0.01F));
    CHECK(preview.clear_draft_color(7U));
    CHECK(!preview.snapshot().draft_color_active);
    CHECK(near(preview.snapshot().fixtures.front().display_rgb.red, 0.5F, 0.01F));

    const auto loop = preview.preview_autoloop(7U, "loop-red-blue", 1.0);
    CHECK(loop.result == emberlights::StudioPreviewResult::Applied);
    CHECK(preview.snapshot().content_kind == emberlights::StudioPreviewContentKind::Autoloop);
    CHECK(near(preview.snapshot().fixtures.front().emitted.rgb.red, 0.5F, 0.01F));
    CHECK(near(preview.snapshot().fixtures.front().emitted.rgb.blue, 0.5F, 0.01F));
    CHECK(preview.tick(7U, 250U, 2.0));
    CHECK(near(preview.snapshot().fixtures.front().emitted.rgb.red, 0.0F, 0.01F));
    CHECK(near(preview.snapshot().fixtures.front().emitted.rgb.blue, 1.0F, 0.01F));

    const auto before_stale = preview.snapshot().dmx_frames;
    const auto stale = preview.preview_look(6U, "look-red", 300U, false);
    CHECK(stale.result == emberlights::StudioPreviewResult::StaleGeneration);
    CHECK(preview.snapshot().dmx_frames.universes == before_stale.universes);

    auto capped_document = document;
    capped_document.generation = 8U;
    capped_document.document.safety.max_intensity = 0.25F;
    CHECK(preview.load(capped_document));
    CHECK(preview.preview_look(8U, "look-red", 300U, false));
    CHECK(preview.snapshot().fixtures.front().dmx_values.front() == 64U);
    CHECK(near(preview.snapshot().fixtures.front().display_rgb.red, 0.25F, 0.01F));

    auto invalid_document = capped_document;
    invalid_document.generation = 9U;
    invalid_document.document.fixtures.front().address = 510U;
    const auto rejected = preview.load(invalid_document);
    CHECK(rejected.result == emberlights::StudioPreviewResult::CompilationFailed);
    CHECK(preview.snapshot().generation == 8U);
    CHECK(preview.clear(8U));
    CHECK(preview.snapshot().content_kind == emberlights::StudioPreviewContentKind::None);
}

}  // namespace

int main() {
    test_picker_color_spaces();
    test_palette_transactions();
    test_fixture_aware_realization();
    test_static_look_color_authoring();
    test_no_output_realtime_preview();

    if (failures != 0) {
        std::cerr << failures << " Studio authoring test(s) failed\n";
        return 1;
    }
    std::cout << "Studio color authoring and preview tests passed\n";
    return 0;
}
