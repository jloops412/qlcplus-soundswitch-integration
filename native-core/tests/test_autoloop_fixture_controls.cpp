#include "emberlights/autoloop_fixture_controls.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
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

template <typename Collection>
[[nodiscard]] auto find_by_id(Collection& collection, std::string_view id) {
    return std::find_if(
        collection.begin(), collection.end(),
        [id](const auto& value) { return value.id == id; });
}

[[nodiscard]] emberlights::ChannelCapabilityDefinition capability(
    std::string id,
    std::string name,
    showcore::Property property,
    std::uint8_t minimum,
    std::uint8_t maximum,
    std::uint8_t preferred,
    showcore::ChannelCapabilityAccess access =
        showcore::ChannelCapabilityAccess::Selectable) {
    emberlights::ChannelCapabilityDefinition value;
    value.id = std::move(id);
    value.name = std::move(name);
    value.property = property;
    value.dmx_min = minimum;
    value.dmx_max = maximum;
    value.preferred_value = preferred;
    value.access = access;
    return value;
}

[[nodiscard]] emberlights::ProjectDocument make_project() {
    emberlights::ProjectDocument project;
    project.id = "autoloop-fixture-controls-test";
    project.name = "Autoloop Fixture Controls Test";

    const auto make_profile = [](bool three_wheel_slots) {
        emberlights::FixtureProfileDefinition profile;
        profile.id = three_wheel_slots ? "profile.wheel.three" :
                                        "profile.wheel.two";
        profile.manufacturer = "Synthetic";
        profile.model = "Wheel";
        profile.mode = three_wheel_slots ? "Three" : "Two";
        profile.name = profile.model + " " + profile.mode;
        profile.footprint = 3U;

        emberlights::ChannelDefinition wheel;
        wheel.property = showcore::Property::Count;
        wheel.coarse_offset = 0U;
        wheel.encoding = showcore::ChannelEncoding::Discrete8;
        wheel.capabilities.push_back(capability(
            "red", "Red", showcore::Property::ColorWheel,
            0U, 9U, 5U));
        if (three_wheel_slots) {
            wheel.capabilities.push_back(capability(
                "green", "Green", showcore::Property::ColorWheel,
                10U, 19U, 15U));
            wheel.capabilities.push_back(capability(
                "blue", "Blue", showcore::Property::ColorWheel,
                20U, 29U, 25U));
        } else {
            wheel.capabilities.push_back(capability(
                "blue", "Blue", showcore::Property::ColorWheel,
                10U, 19U, 15U));
        }
        wheel.capabilities.push_back(capability(
            "factory-reset", "Factory reset", showcore::Property::Custom1,
            240U, 249U, 245U,
            showcore::ChannelCapabilityAccess::Protected));
        profile.channels.push_back(std::move(wheel));

        emberlights::ChannelDefinition shutter;
        shutter.property = showcore::Property::Shutter;
        shutter.coarse_offset = 1U;
        shutter.encoding = showcore::ChannelEncoding::Discrete8;
        auto strobe = capability(
            "strobe", "Strobe slow to fast", showcore::Property::Strobe,
            three_wheel_slots ? 20U : 10U,
            three_wheel_slots ? 89U : 99U,
            three_wheel_slots ? 55U : 60U,
            showcore::ChannelCapabilityAccess::SafetyGated);
        strobe.behavior = showcore::ChannelCapabilityBehavior::Continuous;
        shutter.capabilities.push_back(std::move(strobe));
        profile.channels.push_back(std::move(shutter));

        emberlights::ChannelDefinition intensity;
        intensity.property = showcore::Property::Intensity;
        intensity.coarse_offset = 2U;
        intensity.encoding = showcore::ChannelEncoding::Linear8;
        profile.channels.push_back(std::move(intensity));
        return profile;
    };

    project.fixture_profiles.push_back(make_profile(false));
    project.fixture_profiles.push_back(make_profile(true));
    project.fixtures.push_back({
        "fixture-two", "Two slot", "profile.wheel.two", 1U, 1U, {"wash"}});
    project.fixtures.push_back({
        "fixture-three", "Three slot", "profile.wheel.three", 1U, 10U,
        {"wash"}});
    project.groups.push_back({
        "wheel-group", "Wheel group", {"fixture-two", "fixture-three"}});
    project.groups.push_back({"empty-group", "Empty group", {}});
    return project;
}

[[nodiscard]] emberlights::AutoloopSourceDocument make_source() {
    emberlights::AutoloopSourceDocument source;
    emberlights::AutoloopProgramDefinition program;
    program.id = "program.main";
    program.length_ticks = 4 * emberlights::kMusicalTicksPerQuarter;
    program.targets.push_back({
        "target.master", emberlights::AutoloopTargetKind::Master, {},
        {showcore::Property::Intensity}});
    program.lanes.push_back({"lane.master", "target.master", 0U});
    emberlights::AutoloopEventDefinition event;
    event.id = "event.intensity";
    event.lane_id = "lane.master";
    event.kind = emberlights::AutoloopEventKind::PropertyBlock;
    event.start_tick = 0;
    event.end_tick = emberlights::kMusicalTicksPerQuarter;
    event.property = showcore::Property::Intensity;
    event.value = showcore::PropertyValue::set(0.5F);
    program.events.push_back(std::move(event));
    source.programs.push_back(std::move(program));
    emberlights::normalize_autoloop_source(source);
    CHECK(emberlights::validate_autoloop_source(source).ok());
    return source;
}

[[nodiscard]] std::string choice_id(
    const emberlights::ProjectDocument& project,
    std::string_view capability_id) {
    const auto catalog = emberlights::fixture_control_choices(
        project, "wheel-group");
    const auto found = std::find_if(
        catalog.choices.begin(), catalog.choices.end(),
        [capability_id](const auto& choice) {
            return choice.capability_id == capability_id;
        });
    return found == catalog.choices.end() ? std::string{} : found->id;
}

[[nodiscard]] emberlights::AutoloopFixtureControlRequest request_for(
    const emberlights::AutoloopAuthoringService& service,
    std::string choice,
    std::string prefix = "gesture.blue") {
    emberlights::AutoloopFixtureControlRequest request;
    request.expected_generation = service.generation();
    request.program_id = "program.main";
    request.target_id = "wheel-group";
    request.choice_id = std::move(choice);
    request.stable_id_prefix = std::move(prefix);
    request.start_tick = emberlights::kMusicalTicksPerQuarter;
    request.end_tick = 2 * emberlights::kMusicalTicksPerQuarter;
    return request;
}

void check_unchanged(
    const emberlights::AutoloopAuthoringService& service,
    const emberlights::AutoloopAuthoringSnapshot& before) {
    const auto after = service.snapshot();
    CHECK(after.generation == before.generation);
    CHECK(after.source_digest == before.source_digest);
    CHECK(emberlights::serialize_autoloop_source(after.source) ==
          emberlights::serialize_autoloop_source(before.source));
}

void test_exact_mixed_profile_plan_and_atomic_apply() {
    const auto project = make_project();
    emberlights::AutoloopAuthoringService service(make_source());
    const auto initial = service.snapshot();
    const auto blue_id = choice_id(project, "blue");
    CHECK(!blue_id.empty());
    const auto request = request_for(service, blue_id);

    const auto proposal = emberlights::plan_autoloop_fixture_control(
        initial, project, request);
    CHECK(proposal);
    CHECK(proposal.result ==
          emberlights::AutoloopFixtureControlResult::Prepared);
    CHECK(proposal.writes.size() == 2U);
    CHECK(proposal.validation.ok());
    CHECK(!proposal.warnings.empty());
    CHECK(std::fabs(proposal.writes[0].normalized_value - 0.75F) < 0.0001F);
    CHECK(std::fabs(
        proposal.writes[1].normalized_value - (2.5F / 3.0F)) < 0.0001F);
    CHECK(proposal.writes[0].normalized_value !=
          proposal.writes[1].normalized_value);

    auto program = find_by_id(proposal.candidate.programs, "program.main");
    CHECK(program != proposal.candidate.programs.end());
    if (program != proposal.candidate.programs.end()) {
        CHECK(program->targets.size() == 3U);
        CHECK(program->lanes.size() == 3U);
        CHECK(program->events.size() == 3U);
        for (const auto& write : proposal.writes) {
            const auto target = find_by_id(program->targets, write.target_id);
            const auto event = find_by_id(program->events, write.event_id);
            CHECK(target != program->targets.end());
            CHECK(event != program->events.end());
            if (target != program->targets.end()) {
                CHECK(target->kind == emberlights::AutoloopTargetKind::Fixture);
                CHECK(target->stable_ref == write.fixture_id);
                CHECK(target->required_properties.size() == 1U);
                CHECK(target->required_properties[0] == write.property);
            }
            if (event != program->events.end()) {
                CHECK(event->kind ==
                      emberlights::AutoloopEventKind::PropertyBlock);
                CHECK(event->start_tick == request.start_tick);
                CHECK(event->end_tick == request.end_tick);
                CHECK(event->property == write.property);
                CHECK(event->value.mode == showcore::ValueMode::Set);
                CHECK(event->value.value == write.normalized_value);
                CHECK(event->reference_id.empty());
            }
        }
    }

    const auto applied = emberlights::apply_autoloop_fixture_control(
        service, project, request);
    CHECK(applied);
    CHECK(applied.result == emberlights::AutoloopFixtureControlResult::Applied);
    CHECK(applied.writes.size() == 2U);
    CHECK(service.generation() == initial.generation + 1U);
    CHECK(service.snapshot().source_digest != initial.source_digest);
    const auto undone = service.undo(service.generation());
    CHECK(undone.result == emberlights::AutoloopAuthoringResult::Applied);
    CHECK(service.snapshot().source_digest == initial.source_digest);
}

void test_direct_profile_attribute_plan() {
    const auto project = make_project();
    emberlights::AutoloopAuthoringService service(make_source());
    const auto direct_id = choice_id(project, "direct.intensity");
    CHECK(!direct_id.empty());
    auto request = request_for(service, direct_id, "gesture.intensity");
    request.position = 0.75F;

    const auto proposal = emberlights::plan_autoloop_fixture_control(
        service.snapshot(), project, request);
    CHECK(proposal);
    CHECK(proposal.writes.size() == 2U);
    CHECK(std::all_of(
        proposal.writes.begin(), proposal.writes.end(),
        [](const auto& write) {
            return write.property == showcore::Property::Intensity &&
                std::fabs(write.normalized_value - 0.75F) < 0.0001F;
        }));
    const auto applied = emberlights::apply_autoloop_fixture_control(
        service, project, request);
    CHECK(applied);
    CHECK(applied.writes.size() == 2U);
}

void test_fail_closed_requests_do_not_mutate() {
    auto project = make_project();
    const auto blue_id = choice_id(project, "blue");
    const auto strobe_id = choice_id(project, "strobe");
    CHECK(!blue_id.empty());
    CHECK(!strobe_id.empty());
    const auto catalog = emberlights::fixture_control_choices(
        project, "wheel-group");
    CHECK(std::none_of(
        catalog.choices.begin(), catalog.choices.end(),
        [](const auto& choice) {
            return choice.capability_id == "factory-reset";
        }));

    const auto check_failure = [&](
        emberlights::AutoloopFixtureControlRequest request,
        emberlights::AutoloopFixtureControlResult expected) {
        emberlights::AutoloopAuthoringService service(make_source());
        if (request.expected_generation == 0U) {
            request.expected_generation = service.generation();
        }
        const auto before = service.snapshot();
        const auto outcome = emberlights::apply_autoloop_fixture_control(
            service, project, request);
        CHECK(outcome.result == expected);
        CHECK(!outcome);
        check_unchanged(service, before);
    };

    {
        emberlights::AutoloopAuthoringService service(make_source());
        auto request = request_for(service, blue_id);
        request.expected_generation = service.generation() + 1U;
        const auto before = service.snapshot();
        const auto outcome = emberlights::apply_autoloop_fixture_control(
            service, project, request);
        CHECK(outcome.result ==
              emberlights::AutoloopFixtureControlResult::StaleGeneration);
        check_unchanged(service, before);
    }
    {
        emberlights::AutoloopAuthoringService service(make_source());
        auto request = request_for(service, blue_id);
        request.program_id = "program.missing";
        check_failure(
            request,
            emberlights::AutoloopFixtureControlResult::ProgramNotFound);
    }
    {
        emberlights::AutoloopAuthoringService service(make_source());
        auto request = request_for(service, blue_id);
        request.target_id = "target.missing";
        check_failure(
            request,
            emberlights::AutoloopFixtureControlResult::TargetNotFound);
    }
    {
        emberlights::AutoloopAuthoringService service(make_source());
        auto request = request_for(service, "choice.missing");
        check_failure(
            request,
            emberlights::AutoloopFixtureControlResult::ChoiceNotFound);
    }
    {
        emberlights::AutoloopAuthoringService service(make_source());
        auto request = request_for(service, "protected.choice.not.in.catalog");
        check_failure(
            request,
            emberlights::AutoloopFixtureControlResult::ChoiceNotFound);
    }
    {
        emberlights::AutoloopAuthoringService service(make_source());
        auto request = request_for(service, blue_id);
        request.target_id = "empty-group";
        check_failure(
            request,
            emberlights::AutoloopFixtureControlResult::EmptyTarget);
    }
    {
        emberlights::AutoloopAuthoringService service(make_source());
        auto request = request_for(service, strobe_id);
        check_failure(
            request,
            emberlights::AutoloopFixtureControlResult::SafetyGateRequired);
    }
    {
        emberlights::AutoloopAuthoringService service(make_source());
        auto request = request_for(service, blue_id);
        request.maximum_fixture_writes = 1U;
        check_failure(
            request,
            emberlights::AutoloopFixtureControlResult::CapacityExceeded);
    }
    {
        emberlights::AutoloopAuthoringService service(make_source());
        auto request = request_for(service, blue_id);
        request.end_tick = request.start_tick;
        check_failure(
            request,
            emberlights::AutoloopFixtureControlResult::InvalidRequest);
    }
    {
        emberlights::AutoloopAuthoringService service(make_source());
        auto request = request_for(service, blue_id);
        request.stable_id_prefix.assign(
            emberlights::kMaximumAutoloopSourceIdentifierLength, 'x');
        check_failure(
            request,
            emberlights::AutoloopFixtureControlResult::InvalidRequest);
    }

    const auto stale_choice = blue_id;
    for (auto& profile : project.fixture_profiles) {
        profile.channels[0].capabilities.erase(
            std::remove_if(
                profile.channels[0].capabilities.begin(),
                profile.channels[0].capabilities.end(),
                [](const auto& candidate) {
                    return candidate.id == "blue";
                }),
            profile.channels[0].capabilities.end());
    }
    {
        emberlights::AutoloopAuthoringService service(make_source());
        auto request = request_for(service, stale_choice);
        const auto before = service.snapshot();
        const auto outcome = emberlights::apply_autoloop_fixture_control(
            service, project, request);
        CHECK(outcome.result ==
              emberlights::AutoloopFixtureControlResult::ChoiceNotFound);
        check_unchanged(service, before);
    }
}

void test_identifier_ownership_and_half_open_collisions() {
    const auto project = make_project();
    const auto blue_id = choice_id(project, "blue");
    emberlights::AutoloopAuthoringService service(make_source());
    auto first = request_for(service, blue_id, "gesture.first");
    CHECK(emberlights::apply_autoloop_fixture_control(
        service, project, first));

    auto identifier_collision = first;
    identifier_collision.expected_generation = service.generation();
    const auto before_identifier = service.snapshot();
    const auto identifier = emberlights::apply_autoloop_fixture_control(
        service, project, identifier_collision);
    CHECK(identifier.result ==
          emberlights::AutoloopFixtureControlResult::IdentifierCollision);
    check_unchanged(service, before_identifier);

    auto ownership_collision = request_for(
        service, blue_id, "gesture.overlap");
    const auto before_ownership = service.snapshot();
    const auto ownership = emberlights::apply_autoloop_fixture_control(
        service, project, ownership_collision);
    CHECK(ownership.result ==
          emberlights::AutoloopFixtureControlResult::OwnershipConflict);
    check_unchanged(service, before_ownership);

    auto adjacent = request_for(service, blue_id, "gesture.adjacent");
    adjacent.start_tick = first.end_tick;
    adjacent.end_tick = 3 * emberlights::kMusicalTicksPerQuarter;
    const auto adjacent_outcome =
        emberlights::apply_autoloop_fixture_control(
            service, project, adjacent);
    CHECK(adjacent_outcome.result ==
          emberlights::AutoloopFixtureControlResult::Applied);

    auto explicit_priority = request_for(
        service, blue_id, "gesture.priority");
    explicit_priority.lane_priority = 1U;
    const auto priority_outcome =
        emberlights::apply_autoloop_fixture_control(
            service, project, explicit_priority);
    CHECK(priority_outcome.result ==
          emberlights::AutoloopFixtureControlResult::Applied);
}

}  // namespace

int main() {
    test_exact_mixed_profile_plan_and_atomic_apply();
    test_direct_profile_attribute_plan();
    test_fail_closed_requests_do_not_mutate();
    test_identifier_ownership_and_half_open_collisions();
    if (failures != 0) {
        std::cerr << failures << " autoloop fixture-control check(s) failed\n";
        return 1;
    }
    std::cout << "autoloop fixture control tests passed\n";
    return 0;
}
