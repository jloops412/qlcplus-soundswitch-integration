#include "emberlights/autoloop_runtime.hpp"
#include "emberlights/autoloop_source.hpp"
#include "emberlights/compiler.hpp"
#include "emberlights/project.hpp"
#include "emberlights/runner.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <new>
#include <string_view>
#include <thread>
#include <type_traits>

static_assert(std::is_trivially_copyable_v<emberlights::RunnerAutoloopV2Status>);
static_assert(sizeof(emberlights::RunnerAutoloopV2Status) < 128U);

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

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    if (allocation_probe::enabled) {
        allocation_probe::allocations.fetch_add(1U, std::memory_order_relaxed);
    }
    return std::malloc(size == 0U ? 1U : size);
}

void* operator new[](std::size_t size, const std::nothrow_t& tag) noexcept {
    return ::operator new(size, tag);
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

void operator delete(void* memory, const std::nothrow_t&) noexcept {
    std::free(memory);
}

void operator delete[](void* memory, const std::nothrow_t&) noexcept {
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

[[nodiscard]] bool approximately(float first, float second) noexcept {
    return std::abs(first - second) < 0.00001F;
}

[[nodiscard]] emberlights::ProjectDocument make_project() {
    auto project = emberlights::make_starter_project();
    project.id = "autoloop-v2-runner";
    project.name = "Autoloop V2 Runner Integration";
    project.connections.os2l_enabled = false;
    project.connections.frame_rate = 40U;
    project.fixtures.push_back({
        "fixture-rgbd",
        "RGBD",
        "builtin.generic.rgbd-4ch",
        1U,
        1U,
        {"front"}});

    const auto add_look = [&](std::string_view id,
                              showcore::Property property,
                              float value) {
        project.looks.push_back({
            std::string(id),
            std::string(id),
            0U,
            {{"fixture-rgbd", property,
              showcore::PropertyValue::set(value)}}});
    };
    add_look("look-auto", showcore::Property::Intensity, 0.15F);
    add_look("look-once", showcore::Property::Red, 0.90F);
    add_look("look-replace", showcore::Property::Blue, 0.60F);
    add_look("look-scripted", showcore::Property::Intensity, 0.55F);
    add_look("look-track", showcore::Property::Green, 0.77F);

    project.autoloops.push_back({
        "loop-auto", "Loop Auto", 0U, 0U, 4.0F,
        showcore::AutoloopRepeat::Infinite,
        {{0.0F, "look-auto", showcore::AutoloopTransition::Cut}}});
    project.autoloops.push_back({
        "loop-once", "Loop Once", 1U, 0U, 1.0F,
        showcore::AutoloopRepeat::Once,
        {{0.0F, "look-once", showcore::AutoloopTransition::Cut}}});
    project.autoloops.push_back({
        "loop-replace", "Loop Replace", 1U, 1U, 2.0F,
        showcore::AutoloopRepeat::Infinite,
        {{0.0F, "look-replace", showcore::AutoloopTransition::Cut}}});
    project.autoloops.push_back({
        "loop-scripted", "Loop Scripted", 1U, 2U, 1.0F,
        showcore::AutoloopRepeat::Infinite,
        {{0.0F, "look-scripted", showcore::AutoloopTransition::Cut}}});
    project.track_scripts.push_back({
        "track-static",
        "Track Static",
        {},
        {{0.0F, emberlights::TrackCueAction::TriggerLook, "look-track"}},
        {}});
    return project;
}

[[nodiscard]] emberlights::AutoloopSourceDocument make_source(
    const emberlights::ProjectDocument& project) {
    auto source = emberlights::adapt_format1_autoloops(project);
    for (auto& launch : source.launch_profiles) {
        if (launch.id == "loop-replace.launch") {
            launch.mode = emberlights::AutoloopPlaybackMode::Replace;
        }
    }
    emberlights::normalize_autoloop_source(source);
    return source;
}

[[nodiscard]] showcore::AutoloopTransportState transport_at(
    std::int64_t tick) noexcept {
    showcore::AutoloopTransportState transport;
    transport.musical_tick = tick;
    transport.phase_available = true;
    transport.running = true;
    transport.autonomous_eligible = true;
    return transport;
}

[[nodiscard]] showcore::AutoloopLaunchRequest request(
    showcore::AutoloopAddress address,
    std::uint64_t generation) noexcept {
    return {address, generation};
}

void check_layer(
    const showcore::LayerStack& layers,
    showcore::LayerId layer,
    showcore::Property property,
    showcore::ValueMode mode,
    float value = 0.0F) {
    const auto* buffer = layers.layer(layer);
    CHECK(buffer != nullptr);
    if (buffer == nullptr) {
        return;
    }
    const auto property_value = buffer->get(0U, property);
    CHECK(property_value.mode == mode);
    if (mode == showcore::ValueMode::Set) {
        CHECK(approximately(property_value.value, value));
    }
}

void test_compile_storage_and_track_ownership() {
    const auto project = make_project();
    const auto source = make_source(project);
    auto legacy = emberlights::compile_project(project);
    CHECK(legacy);
    CHECK(legacy.show->autoloop_v2_package() == nullptr);

    auto first = emberlights::compile_project(project, source);
    auto second = emberlights::compile_project(project, source);
    CHECK(first);
    CHECK(second);
    CHECK(first.show->autoloop_v2_package() != nullptr);
    CHECK(first.show->autoloop_v2_package()->digest() ==
          second.show->autoloop_v2_package()->digest());
    CHECK(first.show->autoloop_v2_package()->canonical_bytes().size() ==
          second.show->autoloop_v2_package()->canonical_bytes().size());

    // Exact legacy-player regression: the TrackScript LayerBuffer produced by
    // StaticLookPlayer is identical with no V2 runtime and while the V2
    // adapter advances autonomous content in its private stack.
    const auto* legacy_track_look = first.show->look(4U);
    CHECK(legacy_track_look != nullptr);
    if (legacy_track_look != nullptr) {
        showcore::LayerStack legacy_layers;
        showcore::LayerStack coexistence_layers;
        showcore::StaticLookPlayer legacy_player{
            showcore::LayerId::TrackScript};
        showcore::StaticLookPlayer coexistence_player{
            showcore::LayerId::TrackScript};
        emberlights::AutoloopRuntimeAdapter coexistence;
        CHECK(legacy_player.trigger(
            *legacy_track_look, 0U, 0U, legacy_layers));
        CHECK(coexistence.activate_package(
                  first.show->autoloop_v2_package(),
                  6U,
                  coexistence_layers) ==
              showcore::AutoloopDirectorResult::PackageActivated);
        CHECK(coexistence.claim_legacy_track_script(
            emberlights::AutoloopTrackScriptOwner::LegacyLook,
            coexistence_layers));
        CHECK(coexistence_player.trigger(
            *legacy_track_look, 0U, 0U, coexistence_layers));
        for (std::uint64_t frame = 0U; frame < 32U; ++frame) {
            legacy_player.tick(frame * 25U, legacy_layers);
            static_cast<void>(coexistence.tick(
                transport_at(static_cast<std::int64_t>(frame * 24U)),
                coexistence_layers));
            coexistence_player.tick(frame * 25U, coexistence_layers);
            for (std::size_t property_index = 0U;
                 property_index < showcore::kPropertyCount;
                 ++property_index) {
                const auto property =
                    static_cast<showcore::Property>(property_index);
                const auto baseline = legacy_layers.layer(
                    showcore::LayerId::TrackScript)->get(0U, property);
                const auto integrated = coexistence_layers.layer(
                    showcore::LayerId::TrackScript)->get(0U, property);
                CHECK(baseline.mode == integrated.mode);
                CHECK(baseline.value == integrated.value);
            }
        }
    }

    auto tiny_limits = showcore::AutoloopCompileLimits{};
    tiny_limits.maximum_programs = 0U;
    const auto rejected = emberlights::compile_project(
        project, source, tiny_limits);
    CHECK(!rejected);
    CHECK(rejected.validation.error_count() > 0U);

    emberlights::AutoloopRuntimeAdapter adapter;
    showcore::LayerStack destination;
    const auto* package = first.show->autoloop_v2_package();
    CHECK(adapter.activate_package(package, 7U, destination) ==
          showcore::AutoloopDirectorResult::PackageActivated);
    CHECK(adapter.tick(transport_at(0), destination) ==
          showcore::AutoloopDirectorResult::AutonomousStarted);
    check_layer(
        destination,
        showcore::LayerId::Autonomous,
        showcore::Property::Intensity,
        showcore::ValueMode::Set,
        0.15F);

    destination.set(
        showcore::LayerId::TrackScript,
        0U,
        showcore::Property::Green,
        showcore::PropertyValue::set(0.77F));
    CHECK(adapter.claim_legacy_track_script(
        emberlights::AutoloopTrackScriptOwner::LegacyLook,
        destination));
    destination.set(
        showcore::LayerId::TrackScript,
        0U,
        showcore::Property::Green,
        showcore::PropertyValue::set(0.77F));
    static_cast<void>(adapter.tick(transport_at(100), destination));
    CHECK(adapter.status().track_script_owner ==
          emberlights::AutoloopTrackScriptOwner::LegacyLook);
    CHECK(!adapter.status().track_script_suppressed_by_replace);
    check_layer(
        destination,
        showcore::LayerId::TrackScript,
        showcore::Property::Green,
        showcore::ValueMode::Set,
        0.77F);

    CHECK(adapter.launch_scripted(
              request({1U, 2U}, 7U), transport_at(100), destination) ==
          showcore::AutoloopDirectorResult::ScriptedStarted);
    CHECK(adapter.status().track_script_owner ==
          emberlights::AutoloopTrackScriptOwner::CompiledV2);
    check_layer(
        destination,
        showcore::LayerId::TrackScript,
        showcore::Property::Intensity,
        showcore::ValueMode::Set,
        0.55F);
    check_layer(
        destination,
        showcore::LayerId::TrackScript,
        showcore::Property::Green,
        showcore::ValueMode::Release);
    // Simulate a legacy ClearLook touching the shared public layer. The next
    // pre-composition pass restores the still-owned V2 scripted contribution
    // before rendering.
    destination.clear_layer(showcore::LayerId::TrackScript);
    static_cast<void>(adapter.tick(transport_at(100), destination));
    check_layer(
        destination,
        showcore::LayerId::TrackScript,
        showcore::Property::Intensity,
        showcore::ValueMode::Set,
        0.55F);

    CHECK(adapter.claim_legacy_track_script(
        emberlights::AutoloopTrackScriptOwner::LegacyLook,
        destination));
    destination.set(
        showcore::LayerId::TrackScript,
        0U,
        showcore::Property::Green,
        showcore::PropertyValue::set(0.77F));
    static_cast<void>(adapter.tick(transport_at(200), destination));
    CHECK(!adapter.director_status().scripted.active);
    check_layer(
        destination,
        showcore::LayerId::TrackScript,
        showcore::Property::Green,
        showcore::ValueMode::Set,
        0.77F);
    CHECK(adapter.launch_manual(
              request({1U, 1U}, 7U), transport_at(200), destination) ==
          showcore::AutoloopDirectorResult::ManualStarted);
    CHECK(adapter.status().track_script_owner ==
          emberlights::AutoloopTrackScriptOwner::LegacyLook);
    CHECK(adapter.status().track_script_suppressed_by_replace);
    check_layer(
        destination,
        showcore::LayerId::TrackScript,
        showcore::Property::Green,
        showcore::ValueMode::Release);
    CHECK(adapter.clear_manual(7U, destination) ==
          showcore::AutoloopDirectorResult::ManualCleared);
    CHECK(!adapter.status().track_script_suppressed_by_replace);
    destination.set(
        showcore::LayerId::TrackScript,
        0U,
        showcore::Property::Green,
        showcore::PropertyValue::set(0.77F));
    static_cast<void>(adapter.tick(transport_at(201), destination));
    check_layer(
        destination,
        showcore::LayerId::TrackScript,
        showcore::Property::Green,
        showcore::ValueMode::Set,
        0.77F);

    destination.set(
        showcore::LayerId::EventMoment,
        0U,
        showcore::Property::Red,
        showcore::PropertyValue::set(0.41F));
    destination.set(
        showcore::LayerId::ManualOverride,
        0U,
        showcore::Property::Blue,
        showcore::PropertyValue::set(0.42F));
    destination.set(
        showcore::LayerId::Emergency,
        0U,
        showcore::Property::Green,
        showcore::PropertyValue::set(0.43F));
    destination.set(
        showcore::LayerId::Safety,
        0U,
        showcore::Property::Intensity,
        showcore::PropertyValue::set(0.44F));

    CHECK(adapter.clear_package(destination) ==
          showcore::AutoloopDirectorResult::PackageCleared);
    CHECK(adapter.status().mode ==
          emberlights::AutoloopRuntimeMode::LegacyV1);
    check_layer(
        destination,
        showcore::LayerId::Autonomous,
        showcore::Property::Intensity,
        showcore::ValueMode::Release);
    check_layer(
        destination,
        showcore::LayerId::TrackScript,
        showcore::Property::Green,
        showcore::ValueMode::Set,
        0.77F);
    check_layer(
        destination,
        showcore::LayerId::EventMoment,
        showcore::Property::Red,
        showcore::ValueMode::Set,
        0.41F);
    check_layer(
        destination,
        showcore::LayerId::ManualOverride,
        showcore::Property::Blue,
        showcore::ValueMode::Set,
        0.42F);
    check_layer(
        destination,
        showcore::LayerId::Emergency,
        showcore::Property::Green,
        showcore::ValueMode::Set,
        0.43F);
    check_layer(
        destination,
        showcore::LayerId::Safety,
        showcore::Property::Intensity,
        showcore::ValueMode::Set,
        0.44F);

    CHECK(adapter.activate_package(package, 8U, destination) ==
          showcore::AutoloopDirectorResult::PackageActivated);
    static_cast<void>(adapter.tick(transport_at(0), destination));
    CHECK(adapter.activate_package(package, 7U, destination) ==
          showcore::AutoloopDirectorResult::StaleGeneration);
    CHECK(adapter.status().mode == emberlights::AutoloopRuntimeMode::Fault);
    CHECK(adapter.status().last_result ==
          showcore::AutoloopDirectorResult::StaleGeneration);
    check_layer(
        destination,
        showcore::LayerId::Autonomous,
        showcore::Property::Intensity,
        showcore::ValueMode::Release);
    check_layer(
        destination,
        showcore::LayerId::TrackScript,
        showcore::Property::Green,
        showcore::ValueMode::Release);
}

void test_lifecycle_overlay_replace_and_replay() {
    const auto project = make_project();
    const auto source = make_source(project);
    auto compiled = emberlights::compile_project(project, source);
    CHECK(compiled);
    const auto* package = compiled.show->autoloop_v2_package();

    emberlights::AutoloopRuntimeAdapter adapter;
    showcore::LayerStack destination;
    CHECK(adapter.activate_package(package, 20U, destination) ==
          showcore::AutoloopDirectorResult::PackageActivated);
    static_cast<void>(adapter.tick(transport_at(0), destination));

    CHECK(adapter.launch_manual(
              request({1U, 0U}, 20U), transport_at(0), destination) ==
          showcore::AutoloopDirectorResult::ManualStarted);
    check_layer(
        destination,
        showcore::LayerId::Autonomous,
        showcore::Property::Intensity,
        showcore::ValueMode::Set,
        0.15F);
    check_layer(
        destination,
        showcore::LayerId::ManualAutoloop,
        showcore::Property::Red,
        showcore::ValueMode::Set,
        0.90F);
    CHECK(adapter.tick(transport_at(960), destination) ==
          showcore::AutoloopDirectorResult::ManualCompleted);
    CHECK(!adapter.director_status().manual.active);
    CHECK(adapter.director_status().autonomous.phase_tick == 960);
    check_layer(
        destination,
        showcore::LayerId::Autonomous,
        showcore::Property::Intensity,
        showcore::ValueMode::Set,
        0.15F);

    CHECK(adapter.launch_manual(
              request({1U, 1U}, 20U), transport_at(960), destination) ==
          showcore::AutoloopDirectorResult::ManualStarted);
    CHECK(adapter.director_status().manual.mode ==
          showcore::CompiledAutoloopPlaybackMode::Replace);
    check_layer(
        destination,
        showcore::LayerId::Autonomous,
        showcore::Property::Intensity,
        showcore::ValueMode::Release);
    check_layer(
        destination,
        showcore::LayerId::ManualAutoloop,
        showcore::Property::Blue,
        showcore::ValueMode::Set,
        0.60F);
    CHECK(adapter.clear_manual(20U, destination) ==
          showcore::AutoloopDirectorResult::ManualCleared);
    check_layer(
        destination,
        showcore::LayerId::Autonomous,
        showcore::Property::Intensity,
        showcore::ValueMode::Set,
        0.15F);

    auto once_scripted = request({1U, 2U}, 20U);
    once_scripted.override_repeat_and_mode = true;
    once_scripted.repeat = showcore::AutoloopRepeat::Once;
    once_scripted.mode = showcore::CompiledAutoloopPlaybackMode::Overlay;
    CHECK(adapter.launch_scripted(
              once_scripted, transport_at(1000), destination) ==
          showcore::AutoloopDirectorResult::ScriptedStarted);
    CHECK(adapter.tick(transport_at(1960), destination) ==
          showcore::AutoloopDirectorResult::ScriptedCompleted);
    CHECK(adapter.status().track_script_owner ==
          emberlights::AutoloopTrackScriptOwner::None);
    check_layer(
        destination,
        showcore::LayerId::TrackScript,
        showcore::Property::Intensity,
        showcore::ValueMode::Release);

    const auto replay = [package]() noexcept {
        emberlights::AutoloopRuntimeAdapter runtime;
        showcore::LayerStack layers;
        std::uint64_t hash = 1469598103934665603ULL;
        static_cast<void>(runtime.activate_package(package, 30U, layers));
        for (std::int64_t tick = 0; tick <= 6000; tick += 137) {
            if (tick == 1370) {
                static_cast<void>(runtime.launch_manual(
                    request({1U, 0U}, 30U), transport_at(tick), layers));
            }
            if (tick == 2740) {
                static_cast<void>(runtime.request_exclusive_bank(0U, 30U));
            }
            static_cast<void>(runtime.tick(transport_at(tick), layers));
            const auto& status = runtime.director_status();
            const auto value = layers.resolve(
                0U, showcore::Property::Intensity);
            hash ^= static_cast<std::uint64_t>(status.active_source);
            hash *= 1099511628211ULL;
            hash ^= static_cast<std::uint64_t>(
                status.active_phase_tick);
            hash *= 1099511628211ULL;
            hash ^= static_cast<std::uint64_t>(std::lround(
                value.value * 1000.0F));
            hash *= 1099511628211ULL;
        }
        return hash;
    };
    CHECK(replay() == replay());

    emberlights::AutoloopRuntimeAdapter allocation_runtime;
    showcore::LayerStack allocation_layers;
    static_cast<void>(allocation_runtime.activate_package(
        package, 40U, allocation_layers));
    static_cast<void>(allocation_runtime.tick(
        transport_at(0), allocation_layers));
    allocation_probe::allocations.store(0U, std::memory_order_relaxed);
    allocation_probe::enabled = true;
    for (std::int64_t tick = 1; tick <= 4096; ++tick) {
        static_cast<void>(allocation_runtime.tick(
            transport_at(tick), allocation_layers));
    }
    allocation_probe::enabled = false;
    CHECK(allocation_probe::allocations.load(std::memory_order_relaxed) == 0U);
}

template <typename Predicate>
[[nodiscard]] bool wait_until(Predicate&& predicate) {
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return predicate();
}

void test_runner_v1_parity_v2_swap_and_output_continuity() {
    const auto project = make_project();
    const auto source = make_source(project);

    {
        auto compiled = emberlights::compile_project(project);
        CHECK(compiled);
        emberlights::RunnerService runner;
        CHECK(runner.start(std::move(compiled.show), project));
        CHECK(wait_until([&]() { return runner.status().frames >= 3U; }));
        CHECK(runner.status().autoloop_v2.mode ==
              emberlights::AutoloopRuntimeMode::LegacyV1);
        CHECK(!runner.status().autoloop_v2.package_active);
        CHECK(runner.trigger_track_script(0U));
        CHECK(wait_until([&]() {
            const auto status = runner.status();
            return status.active_track_script == 0 &&
                status.active_track_script_consumed_cues == 1U &&
                status.autoloop_v2.track_script_owner ==
                    emberlights::AutoloopTrackScriptOwner::LegacyLook;
        }));
        const auto before_blackout = runner.status().output_frames;
        runner.set_blackout(true);
        CHECK(wait_until([&]() {
            const auto status = runner.status();
            return status.blackout &&
                status.output_frames > before_blackout;
        }));
        runner.stop();
    }

    {
        auto compiled = emberlights::compile_project(project, source);
        CHECK(compiled);
        emberlights::RunnerService runner;
        CHECK(runner.start(std::move(compiled.show), project));
        CHECK(wait_until([&]() {
            const auto status = runner.status();
            return status.frames >= 3U &&
                status.autoloop_v2.package_active &&
                status.autoloop_v2.package_generation == 1U &&
                status.autoloop_v2.active_source ==
                    showcore::AutoloopDirectorSource::Autonomous;
        }));
        CHECK(runner.trigger_track_script(0U));
        CHECK(wait_until([&]() {
            const auto status = runner.status();
            return status.active_track_script_consumed_cues == 1U &&
                status.autoloop_v2.track_script_owner ==
                    emberlights::AutoloopTrackScriptOwner::LegacyLook;
        }));
        const auto legacy_owner_frames = runner.status().frames;
        CHECK(wait_until([&]() {
            const auto status = runner.status();
            return status.frames > legacy_owner_frames + 3U &&
                status.autoloop_v2.track_script_owner ==
                    emberlights::AutoloopTrackScriptOwner::LegacyLook;
        }));

        CHECK(runner.trigger_autoloop({1U, 0U}));
        CHECK(wait_until([&]() {
            return runner.status().autoloop_v2.active_source ==
                showcore::AutoloopDirectorSource::Manual;
        }));

        const auto output_before_swap = runner.status().output_frames;
        auto replacement = emberlights::compile_project(project, source);
        CHECK(replacement);
        const auto activation = runner.activate(
            std::move(replacement.show), project);
        CHECK(activation);
        CHECK(activation.generation == 2U);
        CHECK(wait_until([&]() {
            const auto status = runner.status();
            return status.state == emberlights::RunnerState::Running &&
                status.autoloop_v2.package_active &&
                status.autoloop_v2.package_generation == 2U &&
                status.output_frames > output_before_swap;
        }));
        runner.stop();
    }
}

}  // namespace

int main() {
    test_compile_storage_and_track_ownership();
    test_lifecycle_overlay_replace_and_replay();
    test_runner_v1_parity_v2_swap_and_output_continuity();
    if (failures != 0) {
        std::cerr << failures << " Autoloop V2 Runner integration checks failed\n";
        return 1;
    }
    std::cout << "Autoloop V2 Runner integration checks passed\n";
    return 0;
}
