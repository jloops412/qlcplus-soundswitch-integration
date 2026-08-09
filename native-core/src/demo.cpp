#include "showcore/artnet.hpp"
#include "showcore/autoloop.hpp"
#include "showcore/engine.hpp"
#include "showcore/fixture.hpp"
#include "showcore/sync_manager.hpp"
#include "showcore/types.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string_view>

namespace {

constexpr std::array<showcore::ChannelMapping, 5> kRgbDimmerChannels{{
    {showcore::Property::Intensity, 0, -1, showcore::ChannelEncoding::Linear8, 0, 255},
    {showcore::Property::Red, 1, -1, showcore::ChannelEncoding::Linear8, 0, 255},
    {showcore::Property::Green, 2, -1, showcore::ChannelEncoding::Linear8, 0, 255},
    {showcore::Property::Blue, 3, -1, showcore::ChannelEncoding::Linear8, 0, 255},
    {showcore::Property::Strobe, 4, -1, showcore::ChannelEncoding::Linear8, 0, 255}
}};

constexpr showcore::FixtureProfile kRgbDimmer{
    "Reference RGB Dimmer",
    kRgbDimmerChannels.data(),
    kRgbDimmerChannels.size(),
    5};

constexpr std::array<showcore::LookAssignment, 4> kRedAssignments{{
    {0, showcore::Property::Intensity, showcore::PropertyValue::set(0.65F)},
    {0, showcore::Property::Red, showcore::PropertyValue::set(1.0F)},
    {0, showcore::Property::Green, showcore::PropertyValue::set(0.0F)},
    {0, showcore::Property::Blue, showcore::PropertyValue::set(0.0F)}
}};

constexpr std::array<showcore::LookAssignment, 4> kBlueAssignments{{
    {0, showcore::Property::Intensity, showcore::PropertyValue::set(1.0F)},
    {0, showcore::Property::Red, showcore::PropertyValue::set(0.0F)},
    {0, showcore::Property::Green, showcore::PropertyValue::set(0.0F)},
    {0, showcore::Property::Blue, showcore::PropertyValue::set(1.0F)}
}};

constexpr showcore::StaticLook kRedLook{
    "Red",
    kRedAssignments.data(),
    kRedAssignments.size()};

constexpr showcore::StaticLook kBlueLook{
    "Blue",
    kBlueAssignments.data(),
    kBlueAssignments.size()};

}  // namespace

int main(int argc, char** argv) {
    auto engine = std::make_unique<showcore::Engine>();
    const auto patch_result = engine->patch().add({0, 0, 1, &kRgbDimmer});
    if (!patch_result) {
        std::cerr << "Unable to patch reference fixture\n";
        return EXIT_FAILURE;
    }

    showcore::AutoloopPattern pattern;
    pattern.name = "Reference red/blue loop";
    pattern.length_beats = 4.0F;
    static_cast<void>(pattern.add_step(
        {0.0F, &kRedLook, showcore::AutoloopTransition::Linear}));
    static_cast<void>(pattern.add_step(
        {2.0F, &kBlueLook, showcore::AutoloopTransition::Linear}));

    showcore::AutoloopEngine autoloop;
    showcore::ArtNetSender sender;
    const bool send_artnet = argc == 3 && std::string_view(argv[1]) == "--artnet";
    if (send_artnet && !sender.open_ipv4(argv[2])) {
        std::cerr << "Invalid/unavailable Art-Net destination\n";
        return EXIT_FAILURE;
    }

    std::uint8_t sequence = 1;
    for (int half_beat = 0; half_beat < 16; ++half_beat) {
        const auto beat = static_cast<double>(half_beat) / 2.0;
        static_cast<void>(autoloop.apply(
            pattern,
            beat,
            showcore::LayerId::Autonomous,
            engine->layers()));
        engine->tick();

        const auto& frame = engine->frames().universes[0];
        std::cout << "beat=" << std::fixed << std::setprecision(1) << beat
                  << " dim=" << static_cast<int>(frame[0])
                  << " rgb=(" << static_cast<int>(frame[1]) << ','
                  << static_cast<int>(frame[2]) << ','
                  << static_cast<int>(frame[3]) << ")\n";

        if (send_artnet) {
            const auto packet = showcore::build_artdmx(frame, 0, sequence++);
            if (!sender.send(packet)) {
                std::cerr << "Art-Net send failed\n";
                return EXIT_FAILURE;
            }
        }
    }

    return EXIT_SUCCESS;
}
