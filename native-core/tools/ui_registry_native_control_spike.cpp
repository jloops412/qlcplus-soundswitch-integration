// Native-first control candidate for the SKIN2-001 source-format decision.
// It intentionally demonstrates the extractor code required to turn C++
// definitions back into tooling JSON; it is not linked into EmberLights.

#include <array>
#include <cstdint>
#include <iostream>
#include <string_view>

namespace {

struct Definition {
    std::uint8_t ordinal;
    std::string_view id;
};

constexpr std::array<Definition, 5> kCommands{{
    {0, "show.start"},
    {3, "output.blackout.set"},
    {14, "staticLook.hold"},
    {16, "autoloop.launch"},
    {27, "group.override.property.set"},
}};

constexpr std::array<std::string_view, 5> kStates{{
    "runner.state",
    "output.blackout",
    "staticLook.active.id",
    "autoloop.active.progress",
    "output.micro.status",
}};

}  // namespace

int main() {
    std::cout << "{\"commands\":[";
    for (std::size_t index = 0; index < kCommands.size(); ++index) {
        if (index != 0U) {
            std::cout << ',';
        }
        std::cout << "{\"id\":\"" << kCommands[index].id
                  << "\",\"nativeOrdinal\":"
                  << static_cast<unsigned int>(kCommands[index].ordinal) << '}';
    }
    std::cout << "],\"states\":[";
    for (std::size_t index = 0; index < kStates.size(); ++index) {
        if (index != 0U) {
            std::cout << ',';
        }
        std::cout << "{\"id\":\"" << kStates[index] << "\"}";
    }
    std::cout << "]}\n";
    return 0;
}
