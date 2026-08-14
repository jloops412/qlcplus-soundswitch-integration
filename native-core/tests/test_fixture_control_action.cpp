#include "emberlights/fixture_control_action.hpp"
#include "emberlights/project_io.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

namespace {

int g_failures = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            std::cerr << "CHECK failed at " << __FILE__ << ':' << __LINE__     \
                      << ": " #condition << '\n';                              \
            ++g_failures;                                                       \
        }                                                                       \
    } while (false)

[[nodiscard]] emberlights::ChannelCapabilityDefinition capability(
    std::string id,
    std::string name,
    showcore::Property property,
    std::uint8_t minimum,
    std::uint8_t maximum,
    std::uint8_t preferred,
    showcore::ChannelCapabilityBehavior behavior =
        showcore::ChannelCapabilityBehavior::Slot,
    showcore::ChannelCapabilityAccess access =
        showcore::ChannelCapabilityAccess::Selectable) {
    emberlights::ChannelCapabilityDefinition value;
    value.id = std::move(id);
    value.name = std::move(name);
    value.property = property;
    value.dmx_min = minimum;
    value.dmx_max = maximum;
    value.preferred_value = preferred;
    value.behavior = behavior;
    value.access = access;
    return value;
}

[[nodiscard]] emberlights::FixtureProfileDefinition make_profile(
    std::string id,
    bool alternate_ranges) {
    emberlights::FixtureProfileDefinition profile;
    profile.id = std::move(id);
    profile.manufacturer = "Test";
    profile.model = "Action fixture";
    profile.mode = alternate_ranges ? "Alternate" : "Primary";
    profile.name = profile.model + " " + profile.mode;
    profile.source = showcore::FixtureProfileSource::Local;
    profile.source_revision = "1";
    profile.footprint = 3U;

    emberlights::ChannelDefinition wheel;
    wheel.property = showcore::Property::Count;
    wheel.coarse_offset = 0U;
    wheel.encoding = showcore::ChannelEncoding::Discrete8;
    wheel.owner = "fixture";
    wheel.capabilities.push_back(capability(
        "red", "Red", showcore::Property::ColorWheel,
        0U, 9U, 5U));
    wheel.capabilities.push_back(capability(
        "blue", "Blue", showcore::Property::ColorWheel,
        alternate_ranges ? 20U : 10U,
        alternate_ranges ? 39U : 19U,
        alternate_ranges ? 30U : 15U));

    emberlights::ChannelDefinition shutter;
    shutter.property = showcore::Property::Count;
    shutter.coarse_offset = 1U;
    shutter.encoding = showcore::ChannelEncoding::Discrete8;
    shutter.owner = "fixture";
    shutter.capabilities.push_back(capability(
        "open", "Open", showcore::Property::Shutter,
        alternate_ranges ? 40U : 0U,
        alternate_ranges ? 69U : 29U,
        alternate_ranges ? 55U : 15U));
    shutter.capabilities.push_back(capability(
        "strobe-slow-fast", "Strobe slow to fast",
        showcore::Property::Strobe,
        alternate_ranges ? 70U : 30U,
        alternate_ranges ? 170U : 130U,
        alternate_ranges ? 120U : 80U,
        showcore::ChannelCapabilityBehavior::Continuous,
        showcore::ChannelCapabilityAccess::SafetyGated));
    shutter.capabilities.push_back(capability(
        "factory-reset", "Factory reset", showcore::Property::Custom1,
        220U, 239U, 230U,
        showcore::ChannelCapabilityBehavior::Slot,
        showcore::ChannelCapabilityAccess::Protected));

    emberlights::ChannelDefinition intensity;
    intensity.property = showcore::Property::Intensity;
    intensity.coarse_offset = 2U;
    intensity.encoding = showcore::ChannelEncoding::Linear8;
    profile.channels = {
        std::move(wheel), std::move(shutter), std::move(intensity)};
    return profile;
}

[[nodiscard]] emberlights::ProjectDocument make_project() {
    auto project = emberlights::make_starter_project();
    project.id = "fixture-control-action-test";
    project.name = "Fixture Control Action Test";
    project.fixture_profiles.push_back(
        make_profile("local.action.primary", false));
    project.fixture_profiles.push_back(
        make_profile("local.action.alternate", true));
    auto partial_profile = make_profile("local.action.partial", false);
    auto& partial_capabilities = partial_profile.channels.front().capabilities;
    partial_capabilities.erase(std::remove_if(
        partial_capabilities.begin(), partial_capabilities.end(),
        [](const auto& value) { return value.id == "blue"; }),
        partial_capabilities.end());
    project.fixture_profiles.push_back(std::move(partial_profile));
    project.fixtures.push_back({
        "fixture-a", "Fixture A", "local.action.primary", 1U, 1U, {}});
    project.fixtures.push_back({
        "fixture-b", "Fixture B", "local.action.primary", 1U, 10U, {}});
    project.fixtures.push_back({
        "fixture-c", "Fixture C", "local.action.alternate", 1U, 20U, {}});
    project.fixtures.push_back({
        "fixture-d", "Fixture D", "local.action.partial", 1U, 30U, {}});
    project.groups.push_back({
        "group-same", "Same profile", {"fixture-a", "fixture-b"}});
    project.groups.push_back({
        "group-mixed", "Mixed profiles", {"fixture-a", "fixture-c"}});
    project.groups.push_back({
        "group-partial", "Partial support", {"fixture-a", "fixture-d"}});
    return project;
}

[[nodiscard]] const emberlights::FixtureControlChoice* find_choice(
    const emberlights::FixtureControlChoiceCatalog& catalog,
    std::string_view capability_id) {
    const auto found = std::find_if(
        catalog.choices.begin(), catalog.choices.end(),
        [capability_id](const auto& choice) {
            return choice.capability_id == capability_id;
        });
    return found == catalog.choices.end() ? nullptr : &*found;
}

struct CapturedArgument {
    std::string_view name;
    emberlights::EmberActionRuntimeValue value;
};

class RecordingControl final : public emberlights::EmberActionCommandControl {
public:
    [[nodiscard]] std::string_view registry_digest() const noexcept override {
        return emberlights::kUiRegistryDigest;
    }

    [[nodiscard]] emberlights::UiInvocationResult invoke(
        const emberlights::EmberActionCommandInvocationView& invocation)
        noexcept override {
        command = invocation.command;
        argument_count = std::min(arguments.size(), invocation.arguments.size());
        for (std::size_t index = 0U; index < argument_count; ++index) {
            arguments[index] = {
                invocation.arguments[index].name,
                invocation.arguments[index].value};
        }
        ++calls;
        return result;
    }

    [[nodiscard]] const emberlights::EmberActionRuntimeValue* argument(
        std::string_view name) const noexcept {
        const auto found = std::find_if(
            arguments.begin(),
            arguments.begin() + static_cast<std::ptrdiff_t>(argument_count),
            [name](const auto& value) { return value.name == name; });
        return found == arguments.begin() +
                static_cast<std::ptrdiff_t>(argument_count)
            ? nullptr : &found->value;
    }

    emberlights::UiInvocationResult result{
        emberlights::UiInvocationResult::Accepted};
    emberlights::UiCommandId command{emberlights::UiCommandId::ShowStart};
    std::array<CapturedArgument, 4U> arguments{};
    std::size_t argument_count{0U};
    std::size_t calls{0U};
};

[[nodiscard]] emberlights::EmberActionExecutionResult execute(
    const emberlights::FixtureControlActionPlan& plan,
    emberlights::EmberActionEntryPoint entry,
    RecordingControl& control) {
    const auto parameters =
        emberlights::fixture_control_action_runtime_parameters(plan);
    emberlights::EmberActionExecutionRequest request;
    request.entry_point = entry;
    request.parameters = parameters;
    return emberlights::execute_ember_action(
        *plan.executable, request, control);
}

void check_set_arguments(
    const RecordingControl& control,
    std::string_view target_name,
    std::string_view target,
    std::string_view property,
    double expected_value) {
    const auto* target_argument = control.argument(target_name);
    const auto* property_argument = control.argument("property");
    const auto* value_argument = control.argument("value");
    CHECK(target_argument != nullptr);
    CHECK(property_argument != nullptr);
    CHECK(value_argument != nullptr);
    if (target_argument != nullptr) {
        CHECK(target_argument->kind ==
              emberlights::EmberActionRuntimeValueKind::StableId);
        CHECK(target_argument->text == target);
    }
    if (property_argument != nullptr) {
        CHECK(property_argument->kind ==
              emberlights::EmberActionRuntimeValueKind::Enum);
        CHECK(property_argument->text == property);
    }
    if (value_argument != nullptr) {
        CHECK(value_argument->kind ==
              emberlights::EmberActionRuntimeValueKind::Number);
        CHECK(std::fabs(value_argument->number_value - expected_value) < 0.000001);
    }
}

void test_exact_fixture_action_and_determinism() {
    const auto project = make_project();
    const auto before = emberlights::serialize_project(project);
    const auto catalog = emberlights::fixture_control_choices(
        project, "fixture-a");
    const auto* blue = find_choice(catalog, "blue");
    CHECK(blue != nullptr);
    if (blue == nullptr) return;

    const emberlights::GeneratedUiRegistryEmberActionView registry;
    const auto first = emberlights::plan_fixture_control_action(
        project, "fixture-a", *blue, registry);
    const auto second = emberlights::plan_fixture_control_action(
        project, "fixture-a", *blue, registry);
    CHECK(first);
    CHECK(second);
    if (!first || !second) {
        for (const auto& diagnostic : first.diagnostics) {
            std::cerr << diagnostic.code << ' ' << diagnostic.path << ' '
                      << diagnostic.message << '\n';
        }
        return;
    }
    CHECK(!first.group);
    CHECK(!first.continuous_fixed_position);
    CHECK(first.release_on_release);
    CHECK(!first.release_on_deactivate);
    CHECK(std::fabs(first.normalized_value - 0.75F) < 0.000001F);
    CHECK(first.action_id == second.action_id);
    CHECK(first.canonical_source == second.canonical_source);
    CHECK(first.content_hash == second.content_hash);
    CHECK(first.action_id ==
          "com.emberlights.action.fixture-control."
          "a31c570277ab8a85943c843aa9d0c14b033e3f15fefaf3285f2eb6c755ca7775");
    CHECK(first.content_hash ==
          "sha256:5588051ac4c79ad7dc7213789e75584809e6a63178c0b66c81933fed87a61315");
    CHECK(first.foundation->cache_key.cache_digest ==
          "sha256:4ee217d2719a5c91c88c84de53c7e1c5e59e9413986840879f8088b777703777");
    CHECK(first.executable->execution_digest ==
          "sha256:bacc505269213d25017b42d23c91a39df0de269c5f3c83543ebad4ad1e695069");
    CHECK(first.prepared->normalized_json == first.canonical_source);
    CHECK(first.prepared->content_hash == first.content_hash);
    CHECK(first.foundation->cache_key.cache_digest ==
          second.foundation->cache_key.cache_digest);
    CHECK(first.executable->execution_digest ==
          second.executable->execution_digest);
    CHECK(first.action_id.starts_with(
        "com.emberlights.action.fixture-control."));
    CHECK(first.content_hash.starts_with("sha256:"));

    const auto other_catalog = emberlights::fixture_control_choices(
        project, "fixture-b");
    const auto* other_blue = find_choice(other_catalog, "blue");
    CHECK(other_blue != nullptr);
    if (other_blue != nullptr) {
        const auto other = emberlights::plan_fixture_control_action(
            project, "fixture-b", *other_blue, registry);
        CHECK(other);
        CHECK(other.action_id != first.action_id);
        CHECK(other.target_id == "fixture-b");
    }

    RecordingControl set;
    const auto set_result = execute(
        first, emberlights::EmberActionEntryPoint::OnPress, set);
    CHECK(set_result.status ==
          emberlights::EmberActionExecutionStatus::Completed);
    CHECK(set_result.result == emberlights::UiInvocationResult::Accepted);
    CHECK(set.calls == 1U);
    CHECK(set.command ==
          emberlights::UiCommandId::FixtureOverridePropertySet);
    check_set_arguments(
        set, "fixtureId", "fixture-a", "colorWheel", 0.75);

    RecordingControl release;
    const auto release_result = execute(
        first, emberlights::EmberActionEntryPoint::OnRelease, release);
    CHECK(release_result.status ==
          emberlights::EmberActionExecutionStatus::Completed);
    CHECK(release.calls == 1U);
    CHECK(release.command ==
          emberlights::UiCommandId::FixtureOverridePropertyRelease);
    CHECK(release.argument("value") == nullptr);

    const auto* intensity = find_choice(catalog, "direct.intensity");
    CHECK(intensity != nullptr);
    if (intensity != nullptr) {
        CHECK(intensity->kind ==
              emberlights::FixtureControlChoiceKind::DirectAttribute);
        const auto direct = emberlights::plan_fixture_control_action(
            project, "fixture-a", *intensity, registry);
        CHECK(direct);
        CHECK(direct.continuous_fixed_position);
        CHECK(std::fabs(direct.normalized_value - 0.5F) < 0.000001F);
        RecordingControl direct_set;
        const auto direct_result = execute(
            direct, emberlights::EmberActionEntryPoint::OnValue, direct_set);
        CHECK(direct_result.status ==
              emberlights::EmberActionExecutionStatus::Completed);
        CHECK(direct_set.command ==
              emberlights::UiCommandId::FixtureOverridePropertySet);
        check_set_arguments(
            direct_set, "fixtureId", "fixture-a", "intensity", 0.5);
    }
    CHECK(emberlights::serialize_project(project) == before);
}

void test_homogeneous_group_and_mixed_group_refusal() {
    const auto project = make_project();
    const auto before = emberlights::serialize_project(project);
    const emberlights::GeneratedUiRegistryEmberActionView registry;

    const auto same_catalog = emberlights::fixture_control_choices(
        project, "group-same");
    const auto* open = find_choice(same_catalog, "open");
    CHECK(open != nullptr);
    if (open == nullptr) return;
    const auto same = emberlights::plan_fixture_control_action(
        project, "group-same", *open, registry);
    CHECK(same);
    CHECK(same.group);
    if (same) {
        RecordingControl set;
        const auto result = execute(
            same, emberlights::EmberActionEntryPoint::OnPress, set);
        CHECK(result.status ==
              emberlights::EmberActionExecutionStatus::Completed);
        CHECK(set.calls == 1U);
        CHECK(set.command ==
              emberlights::UiCommandId::GroupOverridePropertySet);
        check_set_arguments(
            set, "groupId", "group-same", "shutter", 0.5);

        RecordingControl release;
        static_cast<void>(execute(
            same, emberlights::EmberActionEntryPoint::OnRelease, release));
        CHECK(release.command ==
              emberlights::UiCommandId::GroupOverridePropertyRelease);
    }

    const auto mixed_catalog = emberlights::fixture_control_choices(
        project, "group-mixed");
    const auto* mixed_open = find_choice(mixed_catalog, "open");
    CHECK(mixed_open != nullptr);
    CHECK(mixed_open != nullptr && mixed_open->live_override_compatible());
    if (mixed_open != nullptr) {
        const auto mixed = emberlights::plan_fixture_control_action(
            project, "group-mixed", *mixed_open, registry);
        CHECK(!mixed);
        CHECK(mixed.error ==
              emberlights::FixtureControlActionError::GroupNotLiveCompatible);
        CHECK(mixed.executable == nullptr);
    }

    const auto partial_catalog = emberlights::fixture_control_choices(
        project, "group-partial");
    const auto* partial_blue = find_choice(partial_catalog, "blue");
    CHECK(partial_blue != nullptr);
    CHECK(partial_blue != nullptr && partial_blue->partial());
    CHECK(partial_blue != nullptr &&
          !partial_blue->live_override_compatible());
    if (partial_blue != nullptr) {
        const auto partial = emberlights::plan_fixture_control_action(
            project, "group-partial", *partial_blue, registry);
        CHECK(!partial);
        CHECK(partial.error ==
              emberlights::FixtureControlActionError::GroupNotLiveCompatible);
        CHECK(partial.executable == nullptr);
    }
    CHECK(emberlights::serialize_project(project) == before);
}

void test_bad_target_choice_and_stale_selection() {
    const auto project = make_project();
    const emberlights::GeneratedUiRegistryEmberActionView registry;
    const auto catalog = emberlights::fixture_control_choices(
        project, "fixture-a");
    const auto* blue = find_choice(catalog, "blue");
    CHECK(blue != nullptr);
    if (blue == nullptr) return;

    const auto missing_target = emberlights::plan_fixture_control_action(
        project, "missing", *blue, registry);
    CHECK(!missing_target);
    CHECK(missing_target.error ==
          emberlights::FixtureControlActionError::TargetNotFound);

    auto missing_choice = *blue;
    missing_choice.id = "target:missing-choice";
    const auto missing = emberlights::plan_fixture_control_action(
        project, "fixture-a", missing_choice, registry);
    CHECK(!missing);
    CHECK(missing.error ==
          emberlights::FixtureControlActionError::ChoiceNotFound);

    auto stale = *blue;
    stale.values.front().normalized_value = 0.25F;
    stale.shared_normalized_value = 0.25F;
    const auto stale_plan = emberlights::plan_fixture_control_action(
        project, "fixture-a", stale, registry);
    CHECK(!stale_plan);
    CHECK(stale_plan.error ==
          emberlights::FixtureControlActionError::StaleChoice);

    auto protected_choice = *blue;
    protected_choice.access =
        showcore::ChannelCapabilityAccess::Protected;
    const auto protected_plan = emberlights::plan_fixture_control_action(
        project, "fixture-a", protected_choice, registry);
    CHECK(!protected_plan);
    CHECK(protected_plan.error ==
          emberlights::FixtureControlActionError::ProtectedChoice);
    CHECK(find_choice(catalog, "factory-reset") == nullptr);
}

void test_continuous_fallback_and_safety_authority() {
    const auto project = make_project();
    const auto catalog = emberlights::fixture_control_choices(
        project, "group-same", 0.25F);
    const auto* strobe = find_choice(catalog, "strobe-slow-fast");
    CHECK(strobe != nullptr);
    if (strobe == nullptr) return;
    const emberlights::GeneratedUiRegistryEmberActionView registry;
    emberlights::FixtureControlActionOptions options;
    options.catalog_position = 0.25F;

    const auto rejected = emberlights::plan_fixture_control_action(
        project, "group-same", *strobe, registry, options);
    CHECK(!rejected);
    CHECK(rejected.error ==
          emberlights::FixtureControlActionError::SafetyGateRequired);

    options.allow_safety_gated = true;
    const auto accepted = emberlights::plan_fixture_control_action(
        project, "group-same", *strobe, registry, options);
    CHECK(accepted);
    if (!accepted) return;
    CHECK(accepted.continuous_fixed_position);
    CHECK(std::fabs(accepted.normalized_value - 0.25F) < 0.000001F);
    CHECK(!accepted.warnings.empty());

    RecordingControl safety;
    safety.result = emberlights::UiInvocationResult::SafetyRejected;
    const auto result = execute(
        accepted, emberlights::EmberActionEntryPoint::OnValue, safety);
    CHECK(result.status ==
          emberlights::EmberActionExecutionStatus::Completed);
    CHECK(result.result == emberlights::UiInvocationResult::SafetyRejected);
    CHECK(safety.calls == 1U);
    CHECK(safety.command ==
          emberlights::UiCommandId::GroupOverridePropertySet);
    check_set_arguments(
        safety, "groupId", "group-same", "strobe", 0.25);

    RecordingControl wrong_entry;
    const auto unavailable = execute(
        accepted, emberlights::EmberActionEntryPoint::OnPress, wrong_entry);
    CHECK(unavailable.status ==
          emberlights::EmberActionExecutionStatus::EntryPointUnavailable);
    CHECK(wrong_entry.calls == 0U);
}

}  // namespace

int main() {
    test_exact_fixture_action_and_determinism();
    test_homogeneous_group_and_mixed_group_refusal();
    test_bad_target_choice_and_stale_selection();
    test_continuous_fallback_and_safety_authority();
    if (g_failures == 0) {
        std::cout << "Fixture-control Ember Action tests passed\n";
    }
    return g_failures == 0 ? 0 : 1;
}
