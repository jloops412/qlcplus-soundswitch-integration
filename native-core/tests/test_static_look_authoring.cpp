#include "emberlights/compiler.hpp"
#include "emberlights/fixture_capabilities.hpp"
#include "emberlights/fixture_profile_ids.hpp"
#include "emberlights/project_io.hpp"
#include "emberlights/static_look_authoring.hpp"
#include "emberlights/static_look_preview.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
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

[[nodiscard]] bool has_issue(
    const emberlights::ProjectValidation& validation,
    std::string_view code) {
    return std::any_of(
        validation.issues.begin(), validation.issues.end(),
        [code](const auto& issue) { return issue.code == code; });
}

[[nodiscard]] const emberlights::LookAssignmentDefinition* assignment(
    const emberlights::LookDefinition& look,
    std::string_view fixture_id,
    showcore::Property property) {
    const auto found = std::find_if(
        look.assignments.begin(), look.assignments.end(),
        [fixture_id, property](const auto& candidate) {
            return candidate.fixture_id == fixture_id &&
                candidate.property == property;
        });
    return found == look.assignments.end() ? nullptr : &*found;
}

[[nodiscard]] emberlights::ProjectDocument make_ir4_authoring_project() {
    auto project = emberlights::make_starter_project();
    project.id = "static-look-authoring-test";
    project.name = "Static Look Authoring Test";
    project.fixtures.push_back({
        "ir4-6", "IR-4 Six", std::string(emberlights::kBothLightingBoIr4SixChannelProfileId),
        1U, 1U, {"ir4"}});
    project.fixtures.push_back({
        "ir4-10", "IR-4 Ten", std::string(emberlights::kBothLightingBoIr4TenChannelProfileId),
        1U, 20U, {"ir4"}});
    project.groups.push_back({"ir4-pair", "IR-4 Pair", {"ir4-6", "ir4-10"}});
    return project;
}

void test_capability_inventory_and_color_authoring() {
    const auto project = make_ir4_authoring_project();
    CHECK(emberlights::validate_project(project).ok());

    const auto six = emberlights::inspect_fixture_target(project, "ir4-6");
    CHECK(six.target_found);
    CHECK(six.fixtures.size() == 1U);
    CHECK(six.capability(showcore::Property::Red).common());
    CHECK(six.capability(showcore::Property::White).common());
    CHECK(six.capability(showcore::Property::Amber).common());
    CHECK(six.capability(showcore::Property::UV).common());
    CHECK(!six.capability(showcore::Property::Intensity).supported());
    CHECK(!six.capability(showcore::Property::Strobe).supported());

    const auto ten = emberlights::inspect_fixture_target(project, "ir4-10");
    CHECK(ten.capability(showcore::Property::Intensity).common());
    CHECK(ten.capability(showcore::Property::Strobe).common());
    CHECK(!ten.capability(showcore::Property::Custom1).supported());
    CHECK(!ten.capability(showcore::Property::Custom2).supported());

    const auto pair = emberlights::inspect_fixture_target(project, "ir4-pair");
    CHECK(pair.fixtures.size() == 2U);
    CHECK(pair.capability(showcore::Property::Red).common());
    CHECK(pair.capability(showcore::Property::Intensity).partial());
    CHECK(pair.capability(showcore::Property::Intensity).supported_fixture_count == 1U);

    auto draft = emberlights::make_static_look_draft(1U, "look-red", "IR-4 Red");
    const emberlights::StaticLookColor red{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};
    const auto applied = emberlights::apply_static_look_color(
        draft, project, "ir4-pair", red);
    CHECK(applied.result == emberlights::StaticLookAuthoringResult::Applied);
    CHECK(applied.fixtures_considered == 2U);
    CHECK(applied.fixtures_modified == 2U);
    CHECK(draft.look.assignments.size() == 14U);

    for (const auto fixture_id : {std::string_view{"ir4-6"}, std::string_view{"ir4-10"}}) {
        for (const auto property : emberlights::kDirectEmitterProperties) {
            const auto* value = assignment(draft.look, fixture_id, property);
            CHECK(value != nullptr);
            if (value != nullptr) {
                CHECK(value->value.mode == showcore::ValueMode::Set);
                CHECK(value->value.value ==
                    (property == showcore::Property::Red ? 1.0F : 0.0F));
            }
        }
    }
    const auto* master = assignment(
        draft.look, "ir4-10", showcore::Property::Intensity);
    CHECK(master != nullptr && master->value.mode == showcore::ValueMode::Set &&
          master->value.value == 1.0F);
    const auto* strobe = assignment(
        draft.look, "ir4-10", showcore::Property::Strobe);
    CHECK(strobe != nullptr && strobe->value.mode == showcore::ValueMode::ForceZero);

    const auto partial = emberlights::apply_static_look_property(
        draft, project, "ir4-pair", showcore::Property::Intensity,
        showcore::PropertyValue::set(0.5F));
    CHECK(partial);
    CHECK(partial.assignments_written == 1U);
    CHECK(partial.fixtures_skipped == 1U);
    CHECK(assignment(draft.look, "ir4-10", showcore::Property::Intensity)->value.value == 0.5F);
}

void test_ir4_exact_offline_frames() {
    auto project = make_ir4_authoring_project();
    auto draft = emberlights::make_static_look_draft(1U, "look-red", "IR-4 Red");
    CHECK(emberlights::apply_static_look_color(
        draft, project, "ir4-pair",
        {1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F}));
    project.looks.push_back(draft.look);
    const auto preview = emberlights::preview_static_look(project, "look-red");
    if (!preview) {
        std::cerr << emberlights::format_static_look_preview(preview);
    }
    CHECK(preview);
    CHECK(preview.frame_sha256.size() == 64U);

    // 6CH at address 1: R, G, B, W, A, UV.
    CHECK(preview.frames.universes[0][0] == 255U);
    for (std::size_t slot = 1U; slot < 6U; ++slot) {
        CHECK(preview.frames.universes[0][slot] == 0U);
    }
    // 10CH at address 20: master, R, G, B, W, A, UV, strobe,
    // quarantined program selector, quarantined color/speed.
    CHECK(preview.frames.universes[0][19] == 255U);
    CHECK(preview.frames.universes[0][20] == 255U);
    for (std::size_t slot = 21U; slot < 29U; ++slot) {
        CHECK(preview.frames.universes[0][slot] == 0U);
    }
    CHECK(preview.channels.size() == 16U);

    // Every direct emitter gets its own exact one-hot test frame.
    for (std::size_t emitter = 0U;
         emitter < emberlights::kDirectEmitterProperties.size(); ++emitter) {
        emberlights::StaticLookColor color;
        switch (emitter) {
        case 0U: color.red = 1.0F; break;
        case 1U: color.green = 1.0F; break;
        case 2U: color.blue = 1.0F; break;
        case 3U: color.white = 1.0F; break;
        case 4U: color.amber = 1.0F; break;
        case 5U: color.uv = 1.0F; break;
        default: break;
        }
        auto one_hot = emberlights::make_static_look_draft(
            1U, "one-hot", "One Hot");
        CHECK(emberlights::apply_static_look_color(
            one_hot, project, "ir4-6", color));
        const auto one_hot_preview =
            emberlights::preview_static_look_draft(project, one_hot);
        CHECK(one_hot_preview);
        for (std::size_t channel = 0U; channel < 6U; ++channel) {
            CHECK(one_hot_preview.frames.universes[0][channel] ==
                (channel == emitter ? 255U : 0U));
        }

        auto ten_channel = emberlights::make_static_look_draft(
            1U, "ten-one-hot", "Ten Channel One Hot");
        CHECK(emberlights::apply_static_look_color(
            ten_channel, project, "ir4-10", color));
        const auto ten_preview =
            emberlights::preview_static_look_draft(project, ten_channel);
        CHECK(ten_preview);
        CHECK(ten_preview.frames.universes[0][19] == 255U);
        for (std::size_t channel = 0U; channel < 6U; ++channel) {
            CHECK(ten_preview.frames.universes[0][20U + channel] ==
                (channel == emitter ? 255U : 0U));
        }
        CHECK(ten_preview.frames.universes[0][26] == 0U);
        CHECK(ten_preview.frames.universes[0][27] == 0U);
        CHECK(ten_preview.frames.universes[0][28] == 0U);
    }
}

void test_master_closed_and_unsupported_validation() {
    auto project = make_ir4_authoring_project();
    project.looks.push_back({
        "closed", "Closed Red", 0U,
        {{"ir4-10", showcore::Property::Red,
          showcore::PropertyValue::set(1.0F)}}});
    const auto validation = emberlights::validate_project(project);
    CHECK(validation.ok());
    CHECK(has_issue(validation, "look.masterClosed"));
    const auto preview = emberlights::preview_static_look(project, "closed");
    if (!preview) {
        std::cerr << emberlights::format_static_look_preview(preview);
    }
    CHECK(preview);
    CHECK(preview.frames.universes[0][19] == 0U);
    CHECK(preview.frames.universes[0][20] == 255U);
    CHECK(!preview.warnings.empty());

    auto unsupported = project;
    unsupported.fixtures.push_back({
        "rgb", "RGB", "builtin.generic.rgb-3ch", 1U, 100U, {}});
    unsupported.looks.push_back({
        "bad", "Unsupported UV", 0U,
        {{"rgb", showcore::Property::UV, showcore::PropertyValue::set(1.0F)}}});
    const auto rejected = emberlights::validate_project(unsupported);
    CHECK(!rejected.ok());
    CHECK(has_issue(rejected, "look.unsupportedProperty"));
    CHECK(!emberlights::compile_project(unsupported));
}

void test_ownership_round_trip_and_document_commit() {
    auto project = make_ir4_authoring_project();
    emberlights::StudioDocumentService service;
    CHECK(service.replace_document(
        service.generation(), project,
        emberlights::StudioDocumentBoundary::OpenedDocument));
    auto draft = emberlights::make_static_look_draft(
        service.generation(), "ownership", "Ownership");
    CHECK(emberlights::apply_static_look_property(
        draft, project, "ir4-6", showcore::Property::Red,
        showcore::PropertyValue::release()));
    CHECK(emberlights::apply_static_look_property(
        draft, project, "ir4-6", showcore::Property::Green,
        showcore::PropertyValue::set(0.0F)));
    CHECK(emberlights::apply_static_look_property(
        draft, project, "ir4-6", showcore::Property::Blue,
        showcore::PropertyValue::force_zero()));
    const auto committed = emberlights::commit_static_look_draft(service, draft);
    CHECK(committed.result == emberlights::StudioMutationResult::Applied);
    CHECK(service.snapshot().undo_count == 1U);
    CHECK(service.snapshot().document.looks.size() == 1U);

    const auto serialized = emberlights::serialize_project(service.snapshot().document);
    emberlights::ProjectDocument reopened;
    CHECK(emberlights::parse_project(serialized, reopened));
    CHECK(reopened.looks.size() == 1U);
    if (!reopened.looks.empty()) {
        CHECK(assignment(reopened.looks[0], "ir4-6", showcore::Property::Red)->value.mode ==
            showcore::ValueMode::Release);
        CHECK(assignment(reopened.looks[0], "ir4-6", showcore::Property::Green)->value.mode ==
            showcore::ValueMode::Set);
        CHECK(assignment(reopened.looks[0], "ir4-6", showcore::Property::Blue)->value.mode ==
            showcore::ValueMode::ForceZero);
    }
    CHECK(service.undo(service.generation()));
    CHECK(service.snapshot().document.looks.empty());
    CHECK(service.redo(service.generation()));
    CHECK(service.snapshot().document.looks.size() == 1U);

    auto stale = emberlights::make_static_look_draft(
        service.generation(), "stale", "Stale");
    CHECK(emberlights::apply_static_look_property(
        stale, service.snapshot().document, "ir4-6", showcore::Property::Red,
        showcore::PropertyValue::set(1.0F)));
    auto intervening = service.snapshot().document;
    intervening.name = "Changed first";
    CHECK(service.apply_candidate(service.generation(), intervening));
    const auto stale_commit = emberlights::commit_static_look_draft(service, stale);
    CHECK(stale_commit.result == emberlights::StudioMutationResult::StaleGeneration);
}

void test_limits_dependencies_and_hex() {
    auto project = make_ir4_authoring_project();
    for (std::size_t index = 0U; index < emberlights::kMaximumStaticLooks + 1U; ++index) {
        project.looks.push_back({
            "look-" + std::to_string(index),
            "Look " + std::to_string(index),
            0U,
            {{"ir4-6", showcore::Property::Red,
              showcore::PropertyValue::set(1.0F)}}});
    }
    const auto validation = emberlights::validate_project(project);
    CHECK(!validation.ok());
    CHECK(has_issue(validation, "looks.capacity"));

    auto dependencies = make_ir4_authoring_project();
    dependencies.looks.push_back({
        "used", "Used", 0U,
        {{"ir4-6", showcore::Property::Red,
          showcore::PropertyValue::set(1.0F)}}});
    dependencies.autoloops.push_back({
        "loop", "Loop", 0U, 0U, 4.0F, showcore::AutoloopRepeat::Infinite,
        {{0.0F, "used", showcore::AutoloopTransition::Cut}}});
    dependencies.track_scripts.push_back({
        "track", "Track", {}, {{0.0F, emberlights::TrackCueAction::TriggerLook, "used"}}, {}});
    emberlights::MidiMappingDefinition midi;
    midi.device_name = "Controller";
    midi.target_ref = "used";
    midi.action.type = showcore::ActionType::TriggerLook;
    dependencies.midi_mappings.push_back(midi);
    const auto report = emberlights::inspect_static_look_dependencies(
        dependencies, "used");
    CHECK(report.blocked());
    CHECK(report.autoloop_steps == 1U);
    CHECK(report.track_cues == 1U);
    CHECK(report.midi_bindings == 1U);

    emberlights::StaticLookColor color;
    color.white = 0.25F;
    color.amber = 0.5F;
    color.uv = 0.75F;
    CHECK(emberlights::parse_rgb_hex("#1A80FF", color));
    CHECK(std::fabs(color.red - (26.0F / 255.0F)) < 0.0001F);
    CHECK(std::fabs(color.green - (128.0F / 255.0F)) < 0.0001F);
    CHECK(color.blue == 1.0F);
    CHECK(color.white == 0.25F && color.amber == 0.5F && color.uv == 0.75F);
    CHECK(emberlights::format_rgb_hex(color) == "#1A80FF");
    CHECK(!emberlights::parse_rgb_hex("#not-a-color", color));
    CHECK(emberlights::built_in_static_look_swatches().size() >= 9U);
}

}  // namespace

int main() {
    test_capability_inventory_and_color_authoring();
    test_ir4_exact_offline_frames();
    test_master_closed_and_unsupported_validation();
    test_ownership_round_trip_and_document_commit();
    test_limits_dependencies_and_hex();
    if (failures != 0) {
        std::cerr << failures << " Static Look authoring test(s) failed\n";
        return 1;
    }
    std::cout << "Static Look authoring and IR-4 frame tests passed\n";
    return 0;
}
