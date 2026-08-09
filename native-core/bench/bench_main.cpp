#include "showcore/engine.hpp"
#include "showcore/fixture.hpp"
#include "showcore/layer_resolver.hpp"
#include "showcore/types.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>

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
    "Benchmark RGB",
    kChannels.data(),
    kChannels.size(),
    4};

}  // namespace

int main() {
    auto engine = std::make_unique<showcore::Engine>();
    constexpr std::uint16_t fixtures_per_universe = 64;
    for (std::uint8_t universe = 0; universe < showcore::kV1UniverseCount; ++universe) {
        for (std::uint16_t index = 0; index < fixtures_per_universe; ++index) {
            const auto fixture_id = static_cast<std::uint16_t>(universe * fixtures_per_universe + index);
            const auto address = static_cast<std::uint16_t>(index * 4U + 1U);
            if (!engine->patch().add({fixture_id, universe, address, &kProfile})) {
                std::cerr << "Benchmark patch failed\n";
                return EXIT_FAILURE;
            }
            engine->layers().set(showcore::LayerId::Autonomous, fixture_id,
                showcore::Property::Intensity, showcore::PropertyValue::set(0.75F));
            engine->layers().set(showcore::LayerId::Autonomous, fixture_id,
                showcore::Property::Red, showcore::PropertyValue::set(0.9F));
            engine->layers().set(showcore::LayerId::Autonomous, fixture_id,
                showcore::Property::Green, showcore::PropertyValue::set(0.4F));
            engine->layers().set(showcore::LayerId::Autonomous, fixture_id,
                showcore::Property::Blue, showcore::PropertyValue::set(0.1F));
        }
    }

    constexpr std::size_t iterations = 1'000'000;
    std::uint64_t checksum = 0;
    const auto start = std::chrono::steady_clock::now();
    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
        engine->tick();
        if ((iteration & 1023U) == 0U) {
            checksum += engine->frames().universes[iteration & 1U][iteration & 255U];
        }
    }
    const auto end = std::chrono::steady_clock::now();
    const auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    const auto ns_per_tick = static_cast<double>(elapsed_ns) / static_cast<double>(iterations);

    std::cout << std::fixed << std::setprecision(2)
              << "iterations=" << iterations << '\n'
              << "fixtures=" << fixtures_per_universe * showcore::kV1UniverseCount << '\n'
              << "engine_bytes=" << sizeof(engine) << '\n'
              << "ns_per_render_tick=" << ns_per_tick << '\n'
              << "max_render_ticks_per_second=" << 1'000'000'000.0 / ns_per_tick << '\n'
              << "checksum=" << checksum << '\n';

#ifndef _WIN32
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        std::cout << "max_rss_kib=" << usage.ru_maxrss << '\n';
    }
#endif

    return EXIT_SUCCESS;
}
