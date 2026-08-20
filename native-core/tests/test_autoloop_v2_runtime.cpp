#include "emberlights/autoloop_source.hpp"
#include "emberlights/project.hpp"
#include "showcore/autoloop_director.hpp"

#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <new>
#include <string_view>
#include <type_traits>
#include <utility>

static_assert(std::is_trivially_copyable_v<showcore::AutoloopDirectorStatus>);
static_assert(sizeof(showcore::AutoloopDirectorStatus) < 2048U);

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

[[nodiscard]] bool approximately(float first, float second) noexcept {
    return std::abs(first - second) < 0.00001F;
}

[[nodiscard]] emberlights::ProjectDocument make_runtime_project() {
    auto project = emberlights::make_starter_project();
    project.id = "autoloop-v2-runtime";
    project.name = "Autoloop V2 runtime";
    project.autoloops.clear();
    project.autoloops.push_back({
        "auto-a", "Auto A", 0U, 0U, 1.0F, showcore::AutoloopRepeat::Once,
        {{0.0F, "look-a", showcore::AutoloopTransition::Cut}}});
    project.autoloops.push_back({
        "auto-b", "Auto B", 0U, 1U, 1.0F, showcore::AutoloopRepeat::Once,
        {{0.0F, "look-b", showcore::AutoloopTransition::Cut}}});
    project.autoloops.push_back({
        "bank-one", "Bank One", 1U, 0U, 1.0F,
        showcore::AutoloopRepeat::Infinite,
        {{0.0F, "look-bank-one", showcore::AutoloopTransition::Cut}}});
    project.autoloops.push_back({
        "manual-red", "Manual Red", 2U, 0U, 1.0F,
        showcore::AutoloopRepeat::Once,
        {{0.0F, "look-manual", showcore::AutoloopTransition::Cut}}});
    project.autoloops.push_back({
        "scripted", "Scripted", 3U, 0U, 1.0F,
        showcore::AutoloopRepeat::Infinite,
        {{0.0F, "look-scripted", showcore::AutoloopTransition::Cut}}});
    project.autoloops.push_back({
        "track-blue", "Track Blue", 4U, 0U, 1.0F,
        showcore::AutoloopRepeat::TrackDuration,
        {{0.0F, "look-track", showcore::AutoloopTransition::Cut}}});
    return project;
}

struct RuntimePackageFixture {
    std::array<std::uint16_t, 1U> fixture_ids{{0U}};
    std::array<showcore::LookAssignment, 1U> look_a{{{
        0U, showcore::Property::Intensity,
        showcore::PropertyValue::set(0.10F)}}};
    std::array<showcore::LookAssignment, 1U> look_b{{{
        0U, showcore::Property::Intensity,
        showcore::PropertyValue::set(0.20F)}}};
    std::array<showcore::LookAssignment, 1U> look_bank_one{{{
        0U, showcore::Property::Intensity,
        showcore::PropertyValue::set(0.70F)}}};
    std::array<showcore::LookAssignment, 1U> look_manual{{{
        0U, showcore::Property::Red,
        showcore::PropertyValue::set(1.0F)}}};
    std::array<showcore::LookAssignment, 1U> look_scripted{{{
        0U, showcore::Property::Intensity,
        showcore::PropertyValue::set(0.55F)}}};
    std::array<showcore::LookAssignment, 1U> look_track{{{
        0U, showcore::Property::Blue,
        showcore::PropertyValue::set(1.0F)}}};
    std::array<showcore::AutoloopTargetBinding, 1U> targets{};
    std::array<showcore::AutoloopReferenceBinding, 6U> references{};
    showcore::AutoloopCompileResult compiled{};

    RuntimePackageFixture() {
        targets[0] = {
            showcore::CompiledAutoloopTargetKind::Master,
            {},
            fixture_ids,
            showcore::autoloop_property_mask(showcore::Property::Intensity) |
                showcore::autoloop_property_mask(showcore::Property::Red) |
                showcore::autoloop_property_mask(showcore::Property::Blue)};
        references[0] = make_reference("look-a", look_a);
        references[1] = make_reference("look-b", look_b);
        references[2] = make_reference("look-bank-one", look_bank_one);
        references[3] = make_reference("look-manual", look_manual);
        references[4] = make_reference("look-scripted", look_scripted);
        references[5] = make_reference("look-track", look_track);

        const auto source = emberlights::adapt_format1_autoloops(
            make_runtime_project());
        compiled = showcore::compile_autoloop_programs(
            source, {targets, references});
    }

    template <std::size_t Size>
    [[nodiscard]] static showcore::AutoloopReferenceBinding make_reference(
        std::string_view id,
        const std::array<showcore::LookAssignment, Size>& assignments) {
        return {
            showcore::CompiledAutoloopReferenceKind::LegacyLook,
            id,
            showcore::CompiledAutoloopTargetKind::Master,
            {},
            assignments,
            showcore::CompiledAutoloopGeneratorKind::None,
            1U};
    }

    [[nodiscard]] const showcore::CompiledAutoloopPackage* package() const {
        return compiled.package.get();
    }
};

[[nodiscard]] showcore::AutoloopTransportState transport_at(
    std::int64_t tick) noexcept {
    showcore::AutoloopTransportState transport;
    transport.musical_tick = tick;
    transport.phase_available = true;
    transport.running = true;
    transport.autonomous_eligible = true;
    return transport;
}

[[nodiscard]] showcore::AutoloopLaunchRequest launch_request(
    showcore::AutoloopAddress address,
    std::uint64_t generation,
    showcore::AutoloopRepeat repeat,
    showcore::CompiledAutoloopPlaybackMode mode) noexcept {
    return {address, generation, true, repeat, mode};
}

void check_resolved(
    const showcore::LayerStack& layers,
    showcore::Property property,
    showcore::LayerId source,
    float value) {
    const auto resolved = layers.resolve(0U, property);
    CHECK(resolved.owned);
    CHECK(resolved.source == source);
    CHECK(approximately(resolved.value, value));
}

void test_autonomous_bank_boundary_and_direct_launch(
    const RuntimePackageFixture& fixture) {
    showcore::AutoloopDirector director({
        showcore::AutoloopSelectionPolicy::Sequential, 0xA11CEU});
    showcore::LayerStack layers;
    CHECK(director.activate_package(fixture.package(), 7U, layers) ==
          showcore::AutoloopDirectorResult::PackageActivated);
    CHECK(director.tick(transport_at(0), layers) ==
          showcore::AutoloopDirectorResult::AutonomousStarted);
    CHECK((director.status().autonomous.selection.address ==
           showcore::AutoloopAddress{0U, 0U}));
    check_resolved(
        layers, showcore::Property::Intensity,
        showcore::LayerId::Autonomous, 0.10F);

    CHECK(director.request_exclusive_bank(1U, 7U) ==
          showcore::AutoloopDirectorResult::BankMaskPending);
    CHECK(director.status().has_pending_bank_mask);
    CHECK(director.status().pending_bank_mask == (std::uint64_t{1U} << 1U));
    CHECK(director.status().queued.valid);
    CHECK((director.status().queued.address ==
           showcore::AutoloopAddress{1U, 0U}));
    CHECK(director.tick(transport_at(959), layers) ==
          showcore::AutoloopDirectorResult::BankMaskPending);
    CHECK((director.status().autonomous.selection.address ==
           showcore::AutoloopAddress{0U, 0U}));

    CHECK(director.tick(transport_at(960), layers) ==
          showcore::AutoloopDirectorResult::AutonomousAdvanced);
    CHECK(director.status().active_bank_mask == (std::uint64_t{1U} << 1U));
    CHECK(!director.status().has_pending_bank_mask);
    CHECK((director.status().autonomous.selection.address ==
           showcore::AutoloopAddress{1U, 0U}));
    check_resolved(
        layers, showcore::Property::Intensity,
        showcore::LayerId::Autonomous, 0.70F);

    const auto manual = launch_request(
        {2U, 0U}, 7U, showcore::AutoloopRepeat::Once,
        showcore::CompiledAutoloopPlaybackMode::Overlay);
    CHECK(director.launch_manual(manual, transport_at(960), layers) ==
          showcore::AutoloopDirectorResult::ManualStarted);
    CHECK((director.status().manual.selection.address ==
           showcore::AutoloopAddress{2U, 0U}));
    check_resolved(
        layers, showcore::Property::Red,
        showcore::LayerId::ManualAutoloop, 1.0F);

    CHECK(director.set_bank_enabled(0U, true, 7U) ==
          showcore::AutoloopDirectorResult::BankMaskPending);
    CHECK(director.status().pending_bank_mask ==
          ((std::uint64_t{1U} << 0U) | (std::uint64_t{1U} << 1U)));
    CHECK(director.request_all_banks(7U) ==
          showcore::AutoloopDirectorResult::BankMaskPending);
    CHECK(director.tick(transport_at(1920), layers) ==
          showcore::AutoloopDirectorResult::ManualCompleted);
    CHECK(director.status().active_bank_mask == ~std::uint64_t{0U});
    CHECK((director.status().autonomous.selection.address ==
           showcore::AutoloopAddress{2U, 0U}));

    CHECK(director.request_bank_mask(0U, 7U) ==
          showcore::AutoloopDirectorResult::BankMaskPending);
    CHECK(director.tick(transport_at(2880), layers) ==
          showcore::AutoloopDirectorResult::NoEligiblePlacement);
    CHECK(!director.status().autonomous.active);
    CHECK(!layers.resolve(0U, showcore::Property::Intensity).owned);
    CHECK(!layers.resolve(0U, showcore::Property::Red).owned);
    CHECK(director.request_exclusive_bank(0U, 7U) ==
          showcore::AutoloopDirectorResult::BankMaskApplied);
    CHECK(director.tick(transport_at(2881), layers) ==
          showcore::AutoloopDirectorResult::AutonomousStarted);
}

void test_manual_repeat_track_and_transport(
    const RuntimePackageFixture& fixture) {
    showcore::AutoloopDirector director;
    showcore::LayerStack layers;
    CHECK(director.activate_package(fixture.package(), 8U, layers) ==
          showcore::AutoloopDirectorResult::PackageActivated);
    CHECK(director.tick(transport_at(0), layers) ==
          showcore::AutoloopDirectorResult::AutonomousStarted);

    auto once = launch_request(
        {2U, 0U}, 8U, showcore::AutoloopRepeat::Once,
        showcore::CompiledAutoloopPlaybackMode::Overlay);
    CHECK(director.launch_manual(once, transport_at(0), layers) ==
          showcore::AutoloopDirectorResult::ManualStarted);
    CHECK(director.tick(transport_at(959), layers) ==
          showcore::AutoloopDirectorResult::ManualStarted);
    CHECK(director.status().manual.active);
    CHECK(director.tick(transport_at(960), layers) ==
          showcore::AutoloopDirectorResult::ManualCompleted);
    CHECK(!director.status().manual.active);
    check_resolved(
        layers, showcore::Property::Intensity,
        showcore::LayerId::Autonomous, 0.20F);

    auto infinite = launch_request(
        {2U, 0U}, 8U, showcore::AutoloopRepeat::Infinite,
        showcore::CompiledAutoloopPlaybackMode::Overlay);
    CHECK(director.launch_manual(infinite, transport_at(960), layers) ==
          showcore::AutoloopDirectorResult::ManualStarted);
    CHECK(director.tick(transport_at(3840), layers) ==
          showcore::AutoloopDirectorResult::AutonomousAdvanced);
    CHECK(director.status().manual.active);
    CHECK(director.status().manual.completed_cycles == 3U);

    auto held = transport_at(9000);
    held.phase_available = false;
    CHECK(director.tick(held, layers) ==
          showcore::AutoloopDirectorResult::AutonomousAdvanced);
    CHECK(director.status().manual.completed_cycles == 3U);
    held.phase_available = true;
    held.running = false;
    CHECK(director.tick(held, layers) ==
          showcore::AutoloopDirectorResult::AutonomousAdvanced);
    CHECK(director.status().manual.completed_cycles == 3U);

    auto discontinuity = transport_at(-480);
    discontinuity.discontinuity = true;
    CHECK(director.tick(discontinuity, layers) ==
          showcore::AutoloopDirectorResult::TransportRebased);
    CHECK(director.status().manual.active);
    CHECK(director.status().manual.phase_tick == 0);
    CHECK(director.status().manual.completed_cycles == 0U);

    auto track_request = launch_request(
        {4U, 0U}, 8U, showcore::AutoloopRepeat::TrackDuration,
        showcore::CompiledAutoloopPlaybackMode::Overlay);
    auto track = transport_at(0);
    track.autonomous_eligible = false;
    CHECK(director.launch_manual(track_request, track, layers) ==
          showcore::AutoloopDirectorResult::TrackBoundaryUnavailable);
    CHECK(director.status().manual.active);

    track.track_boundary_available = true;
    track.track_active = true;
    track.track_epoch = 11U;
    CHECK(director.launch_manual(track_request, track, layers) ==
          showcore::AutoloopDirectorResult::ManualStarted);
    auto same_track = track;
    same_track.musical_tick = 5000;
    CHECK(director.tick(same_track, layers) !=
          showcore::AutoloopDirectorResult::EvaluationFailed);
    CHECK(director.status().manual.active);
    CHECK(director.status().manual.completed_cycles == 5U);

    auto next_track = same_track;
    next_track.track_epoch = 12U;
    CHECK(director.tick(next_track, layers) ==
          showcore::AutoloopDirectorResult::TrackBoundaryReleased);
    CHECK(!director.status().manual.active);

    track.track_epoch = 13U;
    track.musical_tick = 6000;
    CHECK(director.launch_manual(track_request, track, layers) ==
          showcore::AutoloopDirectorResult::ManualStarted);
    track.track_active = false;
    CHECK(director.tick(track, layers) ==
          showcore::AutoloopDirectorResult::TrackBoundaryReleased);
    CHECK(!director.status().manual.active);
}

void test_overlay_replace_and_higher_layer_independence(
    const RuntimePackageFixture& fixture) {
    showcore::AutoloopDirector director;
    showcore::LayerStack layers;
    CHECK(director.activate_package(fixture.package(), 9U, layers) ==
          showcore::AutoloopDirectorResult::PackageActivated);
    CHECK(director.tick(transport_at(0), layers) ==
          showcore::AutoloopDirectorResult::AutonomousStarted);

    const auto scripted = launch_request(
        {3U, 0U}, 9U, showcore::AutoloopRepeat::Infinite,
        showcore::CompiledAutoloopPlaybackMode::Overlay);
    CHECK(director.launch_scripted(scripted, transport_at(0), layers) ==
          showcore::AutoloopDirectorResult::ScriptedStarted);
    check_resolved(
        layers, showcore::Property::Intensity,
        showcore::LayerId::TrackScript, 0.55F);

    const auto once_overlay = launch_request(
        {2U, 0U}, 9U, showcore::AutoloopRepeat::Once,
        showcore::CompiledAutoloopPlaybackMode::Overlay);
    CHECK(director.launch_manual(once_overlay, transport_at(0), layers) ==
          showcore::AutoloopDirectorResult::ManualStarted);
    check_resolved(
        layers, showcore::Property::Intensity,
        showcore::LayerId::TrackScript, 0.55F);
    check_resolved(
        layers, showcore::Property::Red,
        showcore::LayerId::ManualAutoloop, 1.0F);
    CHECK(director.tick(transport_at(959), layers) ==
          showcore::AutoloopDirectorResult::ManualStarted);
    CHECK(director.tick(transport_at(960), layers) ==
          showcore::AutoloopDirectorResult::ManualCompleted);
    CHECK(!director.status().manual.active);
    CHECK(director.status().scripted.active);
    check_resolved(
        layers, showcore::Property::Intensity,
        showcore::LayerId::TrackScript, 0.55F);

    const auto overlay = launch_request(
        {2U, 0U}, 9U, showcore::AutoloopRepeat::Infinite,
        showcore::CompiledAutoloopPlaybackMode::Overlay);
    CHECK(director.launch_manual(overlay, transport_at(960), layers) ==
          showcore::AutoloopDirectorResult::ManualStarted);

    const auto replace = launch_request(
        {2U, 0U}, 9U, showcore::AutoloopRepeat::Infinite,
        showcore::CompiledAutoloopPlaybackMode::Replace);
    CHECK(director.launch_manual(replace, transport_at(1000), layers) ==
          showcore::AutoloopDirectorResult::ManualStarted);
    CHECK(director.status().scripted.active);
    CHECK(director.status().autonomous.active);
    CHECK(!layers.resolve(0U, showcore::Property::Intensity).owned);
    check_resolved(
        layers, showcore::Property::Red,
        showcore::LayerId::ManualAutoloop, 1.0F);

    CHECK(director.tick(transport_at(1600), layers) ==
          showcore::AutoloopDirectorResult::ManualStarted);
    CHECK(director.status().scripted.phase_tick == 640);
    CHECK(director.status().autonomous.phase_tick == 640);
    CHECK(director.clear_manual(9U, layers) ==
          showcore::AutoloopDirectorResult::ManualCleared);
    check_resolved(
        layers, showcore::Property::Intensity,
        showcore::LayerId::TrackScript, 0.55F);

    const auto once_replace = launch_request(
        {2U, 0U}, 9U, showcore::AutoloopRepeat::Once,
        showcore::CompiledAutoloopPlaybackMode::Replace);
    CHECK(director.launch_manual(
              once_replace, transport_at(1600), layers) ==
          showcore::AutoloopDirectorResult::ManualStarted);
    CHECK(!layers.resolve(0U, showcore::Property::Intensity).owned);
    CHECK(director.tick(transport_at(2560), layers) ==
          showcore::AutoloopDirectorResult::ManualCompleted);
    CHECK(!director.status().manual.active);
    CHECK(director.status().scripted.active);
    check_resolved(
        layers, showcore::Property::Intensity,
        showcore::LayerId::TrackScript, 0.55F);

    layers.set(
        showcore::LayerId::EventMoment, 0U, showcore::Property::Intensity,
        showcore::PropertyValue::set(0.91F));
    layers.set(
        showcore::LayerId::ManualOverride, 0U, showcore::Property::Red,
        showcore::PropertyValue::set(0.81F));
    layers.set(
        showcore::LayerId::Emergency, 0U, showcore::Property::Blue,
        showcore::PropertyValue::set(0.71F));
    layers.set(
        showcore::LayerId::Safety, 0U, showcore::Property::White,
        showcore::PropertyValue::set(0.61F));
    CHECK(director.tick(transport_at(2600), layers) ==
          showcore::AutoloopDirectorResult::ManualCompleted);
    check_resolved(
        layers, showcore::Property::Intensity,
        showcore::LayerId::EventMoment, 0.91F);
    check_resolved(
        layers, showcore::Property::Red,
        showcore::LayerId::ManualOverride, 0.81F);
    check_resolved(
        layers, showcore::Property::Blue,
        showcore::LayerId::Emergency, 0.71F);
    check_resolved(
        layers, showcore::Property::White,
        showcore::LayerId::Safety, 0.61F);

    CHECK(director.fault(
              showcore::AutoloopDirectorFault::ExternalFault, layers) ==
          showcore::AutoloopDirectorResult::Faulted);
    check_resolved(
        layers, showcore::Property::Intensity,
        showcore::LayerId::EventMoment, 0.91F);
    check_resolved(
        layers, showcore::Property::Red,
        showcore::LayerId::ManualOverride, 0.81F);
    check_resolved(
        layers, showcore::Property::Blue,
        showcore::LayerId::Emergency, 0.71F);
    check_resolved(
        layers, showcore::Property::White,
        showcore::LayerId::Safety, 0.61F);
}

void test_generation_activation_and_fault_clearing(
    const RuntimePackageFixture& fixture) {
    showcore::AutoloopDirector director;
    showcore::LayerStack layers;
    layers.set(
        showcore::LayerId::EventMoment, 0U, showcore::Property::Red,
        showcore::PropertyValue::set(0.40F));
    CHECK(director.activate_package(fixture.package(), 20U, layers) ==
          showcore::AutoloopDirectorResult::PackageActivated);
    CHECK(director.tick(transport_at(0), layers) ==
          showcore::AutoloopDirectorResult::AutonomousStarted);

    const auto stale = launch_request(
        {2U, 0U}, 19U, showcore::AutoloopRepeat::Infinite,
        showcore::CompiledAutoloopPlaybackMode::Overlay);
    CHECK(director.launch_manual(stale, transport_at(0), layers) ==
          showcore::AutoloopDirectorResult::StaleGeneration);
    CHECK(director.status().autonomous.active);
    CHECK(!director.status().manual.active);

    CHECK(director.activate_package(fixture.package(), 21U, layers) ==
          showcore::AutoloopDirectorResult::PackageActivated);
    CHECK(!director.status().autonomous.active);
    CHECK(!layers.resolve(0U, showcore::Property::Intensity).owned);
    check_resolved(
        layers, showcore::Property::Red,
        showcore::LayerId::EventMoment, 0.40F);
    CHECK(director.activate_package(fixture.package(), 20U, layers) ==
          showcore::AutoloopDirectorResult::StaleGeneration);
    CHECK(director.status().package_generation == 21U);

    CHECK(director.tick(transport_at(0), layers) ==
          showcore::AutoloopDirectorResult::AutonomousStarted);
    CHECK(director.fault(
              showcore::AutoloopDirectorFault::ExternalFault, layers) ==
          showcore::AutoloopDirectorResult::Faulted);
    CHECK(!director.status().package_active);
    CHECK(!layers.resolve(0U, showcore::Property::Intensity).owned);
    CHECK(director.tick(transport_at(100), layers) ==
          showcore::AutoloopDirectorResult::Faulted);

    CHECK(director.activate_package(fixture.package(), 22U, layers) ==
          showcore::AutoloopDirectorResult::PackageActivated);
    CHECK(director.status().fault == showcore::AutoloopDirectorFault::None);
    CHECK(director.activate_package(nullptr, 23U, layers) ==
          showcore::AutoloopDirectorResult::PackageUnavailable);
    CHECK(director.status().fault ==
          showcore::AutoloopDirectorFault::PackageUnavailable);
    CHECK(!layers.resolve(0U, showcore::Property::Intensity).owned);
    CHECK(director.activate_package(fixture.package(), 22U, layers) ==
          showcore::AutoloopDirectorResult::StaleGeneration);
    CHECK(director.clear_package(layers) ==
          showcore::AutoloopDirectorResult::PackageCleared);
    CHECK(director.status().package_generation == 0U);
    CHECK(director.activate_package(fixture.package(), 22U, layers) ==
          showcore::AutoloopDirectorResult::StaleGeneration);
    CHECK(director.activate_package(fixture.package(), 24U, layers) ==
          showcore::AutoloopDirectorResult::PackageActivated);
    check_resolved(
        layers, showcore::Property::Red,
        showcore::LayerId::EventMoment, 0.40F);
}

void hash_u64(std::uint64_t& hash, std::uint64_t value) noexcept {
    constexpr std::uint64_t kPrime = 1099511628211ULL;
    for (std::size_t byte = 0U; byte < sizeof(value); ++byte) {
        hash ^= (value >> (byte * 8U)) & 0xFFU;
        hash *= kPrime;
    }
}

void hash_snapshot(
    std::uint64_t& hash,
    const showcore::AutoloopDirector& director) noexcept {
    const auto& status = director.status();
    hash_u64(hash, status.package_generation);
    hash_u64(hash, status.active_bank_mask);
    hash_u64(hash, status.pending_bank_mask);
    hash_u64(hash, static_cast<std::uint64_t>(status.last_result));
    hash_u64(hash, static_cast<std::uint64_t>(status.active_source));
    hash_u64(hash, status.active.valid ? 1U : 0U);
    hash_u64(hash, status.active.address.bank);
    hash_u64(hash, status.active.address.slot);
    hash_u64(hash, status.active.program_index);
}

[[nodiscard]] std::uint64_t replay_digest(
    const RuntimePackageFixture& fixture) {
    showcore::AutoloopDirector director({
        showcore::AutoloopSelectionPolicy::Sequential, 0xB0A7U});
    showcore::LayerStack layers;
    std::uint64_t hash = 1469598103934665603ULL;
    hash_u64(hash, static_cast<std::uint64_t>(
        director.activate_package(fixture.package(), 44U, layers)));
    hash_u64(hash, static_cast<std::uint64_t>(
        director.tick(transport_at(0), layers)));
    hash_u64(hash, static_cast<std::uint64_t>(
        director.request_exclusive_bank(1U, 44U)));
    hash_u64(hash, static_cast<std::uint64_t>(
        director.tick(transport_at(960), layers)));

    const auto manual = launch_request(
        {2U, 0U}, 44U, showcore::AutoloopRepeat::Once,
        showcore::CompiledAutoloopPlaybackMode::Overlay);
    hash_u64(hash, static_cast<std::uint64_t>(
        director.launch_manual(manual, transport_at(960), layers)));
    hash_u64(hash, static_cast<std::uint64_t>(
        director.tick(transport_at(1440), layers)));

    const auto scripted = launch_request(
        {3U, 0U}, 44U, showcore::AutoloopRepeat::Infinite,
        showcore::CompiledAutoloopPlaybackMode::Overlay);
    hash_u64(hash, static_cast<std::uint64_t>(
        director.launch_scripted(scripted, transport_at(1440), layers)));
    const auto replace = launch_request(
        {2U, 0U}, 44U, showcore::AutoloopRepeat::Infinite,
        showcore::CompiledAutoloopPlaybackMode::Replace);
    hash_u64(hash, static_cast<std::uint64_t>(
        director.launch_manual(replace, transport_at(1440), layers)));
    hash_u64(hash, static_cast<std::uint64_t>(
        director.tick(transport_at(1700), layers)));
    hash_u64(hash, static_cast<std::uint64_t>(
        director.clear_manual(44U, layers)));

    auto discontinuity = transport_at(-100);
    discontinuity.discontinuity = true;
    hash_u64(hash, static_cast<std::uint64_t>(
        director.tick(discontinuity, layers)));
    hash_snapshot(hash, director);
    for (const auto property : {
             showcore::Property::Intensity,
             showcore::Property::Red,
             showcore::Property::Blue}) {
        const auto resolved = layers.resolve(0U, property);
        hash_u64(hash, resolved.owned ? 1U : 0U);
        hash_u64(hash, static_cast<std::uint64_t>(resolved.source));
        hash_u64(hash, std::bit_cast<std::uint32_t>(resolved.value));
    }
    return hash;
}

void test_deterministic_replay_and_zero_allocation(
    const RuntimePackageFixture& fixture) {
    const auto first = replay_digest(fixture);
    const auto second = replay_digest(fixture);
    CHECK(first == second);
    CHECK(first == 5748616596907537841ULL);

    showcore::AutoloopDirector director;
    showcore::LayerStack layers;
    CHECK(director.activate_package(fixture.package(), 55U, layers) ==
          showcore::AutoloopDirectorResult::PackageActivated);
    CHECK(director.tick(transport_at(0), layers) ==
          showcore::AutoloopDirectorResult::AutonomousStarted);
    const auto manual = launch_request(
        {2U, 0U}, 55U, showcore::AutoloopRepeat::Infinite,
        showcore::CompiledAutoloopPlaybackMode::Overlay);
    CHECK(director.launch_manual(manual, transport_at(0), layers) ==
          showcore::AutoloopDirectorResult::ManualStarted);

    allocation_probe::allocations.store(0U, std::memory_order_relaxed);
    allocation_probe::enabled = true;
    bool ticks_succeeded = true;
    for (std::int64_t tick = 1; tick <= 20'000; tick += 17) {
        const auto result = director.tick(transport_at(tick), layers);
        ticks_succeeded = result !=
            showcore::AutoloopDirectorResult::EvaluationFailed &&
            result != showcore::AutoloopDirectorResult::Faulted &&
            ticks_succeeded;
    }
    allocation_probe::enabled = false;
    CHECK(ticks_succeeded);
    CHECK(allocation_probe::allocations.load(std::memory_order_relaxed) == 0U);
}

}  // namespace

int main() {
    const RuntimePackageFixture fixture;
    CHECK(fixture.compiled.ok());
    if (fixture.compiled.ok()) {
        test_autonomous_bank_boundary_and_direct_launch(fixture);
        test_manual_repeat_track_and_transport(fixture);
        test_overlay_replace_and_higher_layer_independence(fixture);
        test_generation_activation_and_fault_clearing(fixture);
        test_deterministic_replay_and_zero_allocation(fixture);
    }

    if (failures != 0) {
        std::cerr << failures << " Autoloop V2 runtime test(s) failed\n";
        return 1;
    }
    std::cout << "Autoloop V2 runtime director tests passed\n";
    return 0;
}
