#include "../soundswitchprotocol.h"

#include <cstdlib>

int main()
{
    using namespace SoundSwitchProtocol;

    Universe first{};
    first.front() = 0x01U;
    first[127] = 0x7FU;
    first[128] = 0x80U;
    first.back() = 0xFFU;
    const Packet micro = buildDmxPacket(0U, first);

    if (micro.size() != FrameSize || micro[0] != 's' || micro[1] != 'T' ||
        micro[2] != 'R' || micro[3] != 't' || micro[4] != 0x01U ||
        micro[6] != 0x02U || micro[7] != 0x02U || micro[8] != 0U ||
        micro[9] != 0U || micro[10] != 0x01U || micro[137] != 0x7FU ||
        micro[138] != 0x80U || micro[521] != 0xFFU)
        return EXIT_FAILURE;

    Universe second{};
    second.front() = 0x22U;
    second.back() = 0xFEU;
    const Packet controlOnePortTwo = buildDmxPacket(1U, second);
    if (controlOnePortTwo[8] != 1U || controlOnePortTwo[10] != 0x22U ||
        controlOnePortTwo[521] != 0xFEU)
        return EXIT_FAILURE;

    if (ConfigurationValue != 1U || InterfaceNumber != 0U ||
        AlternateSetting != 0U || ControlOneOutputCount != 2U ||
        MicroInitializationPackets[1][10] != 0xFFU ||
        ControlOneInitializationPackets.size() != 4U ||
        ControlOneInitializationPackets[0] != MicroInitializationPackets[0] ||
        ControlOneInitializationPackets[1] != MicroInitializationPackets[1] ||
        ControlOneInitializationPackets[2][8] != 0U ||
        ControlOneInitializationPackets[2][10] != 1U ||
        ControlOneInitializationPackets[3][8] != 1U ||
        ControlOneInitializationPackets[3][10] != 1U)
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
