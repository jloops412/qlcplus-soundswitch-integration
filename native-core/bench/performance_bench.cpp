#include "showcore/autoloop.hpp"
#include "showcore/engine.hpp"
#include "showcore/fixture.hpp"
#include "showcore/look.hpp"
#include "showcore/types.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>

#ifndef _WIN32
#include <sys/resource.h>
#endif

namespace {

constexpr std::array<showcore::ChannelMapping, 4> kChannels{{
    {showcore::Property::Intensity, 0, -1, showcore::ChannelEncoding::Linear8, 0, 255},
    {showcore::Property::Red, 1, -1, showcore::ChannelEncoding::Linear8, 0, 255},
    {showcore::Property::Green, 2, -1, showcore::ChannelEncoding::Linear8, 0, 255},
    {showcore::Property::Blue, 3, -1, showcore::ChannelEncoding::Linear8, 0, 255}
}};

constexpr showcore::FixtureProfile kProfile{
    "Performance Benchmark RGB",
    kChannels.data(),
    kChannels.size(),
    4};

constexpr std::size_t kFixtureCount = 128;
constexpr std::size_t kAssignmentsPerLook = kFixtureCount * 4U;

void populate_assignments(
    std::array<showcore::LookAssignment, kAssignmentsPerLook>& assignments,
    float red,
    float green,
    float blue) {
    std::size_t assignment_index = 0;
    for (std::uint16_t fixture_id = 0; fixture_id < kFixtureCount; ++fixture_id) {
        assignments[assignment_index++] = {
            fixture_id,
            showcore::Property::Intensity,
            showcore::PropertyValue::set(0.8F)};
        assignments[assignment_index++] = {
            fixture_id,
            showcore::Property::Red,
            showcore::PropertyValue::set(red)};
        assignments[assignment_index++] = {
            fixture_id,
            showcore::Property::Green,
            showcore::PropertyValue::set(green)};
        assignments[assignment_index++] = {
            fixture_id,
            showcore::Property::Blue,
            showcore::PropertyValue::set(blue)};
    }
}

}  // namespace

int main() {
    showcore::Engine engine;
    constexpr std::uint16_t fixtures_per_universe = kFixtureCount / showcore::kV1UniverseCount;
    for (std::uint8_t universe = 0; universe < showcore::kV1UniverseCount; ++universe) {
        for (std::uint16_t index = 0; index < fixtures_per_universe; ++index) {
            const auto fixture_id = static_cast<std::uint16_t>(
                universe * fixtures_per_universe + index);
            const auto address = static_cast<std::uint16_t>(index * 4U + 1U);
            if (!engine.patch().add({fixture_id, universe, address, &kProfile})) {
                std::cerr << "Performance benchmark patch failed\n";
                return EXIT_FAILURE;
            }
        }
    }

    std::array<showcore::LookAssignment, kAssignmentsPerLook> warm_assignments{};
    std::array<showcore::LookAssignment, kAssignmentsPerLook> cool_assignments{};
    populate_assignments(warm_assignments, 1.0F, 0.2F, 0.0F);
    populate_assignments(cool_assignments, 0.0F, 0.2F, 1.0F);
    const showcore::StaticLook warm{
        "Warm",
        warm_assignments.data(),
        warm_assignments.size()};
    const showcore::StaticLook cool{
        "Cool",
        cool_assignments.data(),
        cool_assignments.size()};

    showcore::AutoloopPattern pattern;
    pattern.name = "Benchmark loop";
    pattern.length_beats = 4.0F;
    if (!pattern.add_step({0.0F, &warm, showcore::AutoloopTransition::Linear}) ||
        !pattern.add_step({2.0F, &cool, showcore::AutoloopTransition::Linear})) {
        std::cerr << "Performance benchmark Autoloop failed\n";
        return EXIT_FAILURE;
    }
    showcore::AutoloopCatalog catalog;
    if (!catalog.set({0, 0}, &pattern)) {
        std::cerr << "Performance benchmark catalog failed\n";
        return EXIT_FAILURE;
    }

    showcore::AutoloopPlayer autoloop;
    showcore::StaticLookPlayer static_look;
    if (!autoloop.trigger(
            catalog,
            {0, 0},
            showcore::AutoloopRepeat::Infinite,
            0.0,
            true,
            engine.layers()) ||
        !static_look.trigger(warm, 0, 750, engine.layers())) {
        std::cerr << "Performance benchmark playback start failed\n";
        return EXIT_FAILURE;
    }

    constexpr std::size_t iterations = 20'000;
    std::uint64_t checksum = 0;
    const auto start = std::chrono::steady_clock::now();
    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
        const auto beat = static_cast<double>(iteration) / 20.0;
        const auto now_ms = static_cast<std::uint64_t>(iteration * 25U);
        if (iteration > 0 && iteration % 40U == 0U) {
            const auto& next = (iteration / 40U) % 2U == 0U ? warm : cool;
            if (!static_look.trigger(next, now_ms, 750, engine.layers())) {
                std::cerr << "Performance benchmark look transition failed\n";
                return EXIT_FAILURE;
            }
        }
        autoloop.tick(beat, true, engine.layers());
        static_look.tick(now_ms, engine.layers());
        engine.tick();
        checksum += engine.frames().universes[iteration & 1U][iteration & 255U];
    }
    const auto end = std::chrono::steady_clock::now();
    const auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    const auto ns_per_tick = static_cast<double>(elapsed_ns) / static_cast<double>(iterations);

    std::cout << std::fixed << std::setprecision(2)
              << "performance_iterations=" << iterations << '\n'
              << "performance_fixtures=" << kFixtureCount << '\n'
              << "performance_assignments_per_look=" << kAssignmentsPerLook << '\n'
              << "ns_per_full_performance_tick=" << ns_per_tick << '\n'
              << "full_performance_ticks_per_second=" << 1'000'000'000.0 / ns_per_tick << '\n'
              << "estimated_single_core_percent_at_40hz=" << ns_per_tick * 40.0 / 10'000'000.0 << '\n'
              << "performance_checksum=" << checksum << '\n';

#ifndef _WIN32
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        std::cout << "performance_max_rss_kib=" << usage.ru_maxrss << '\n';
    }
#endif

    return EXIT_SUCCESS;
}
