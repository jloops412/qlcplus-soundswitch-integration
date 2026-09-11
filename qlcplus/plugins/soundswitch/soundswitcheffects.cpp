/* QLC+ SoundSwitch integration. Licensed under Apache-2.0. */
#include "soundswitcheffects.h"
#include "soundswitchintensity.h"
#include <algorithm>

void SoundSwitchEffects::setFrame(const QByteArray &frame)
{
    // The private frame must include both complete Focus fixture spans. Always
    // take owned bytes: QLC+ can supply a non-owning view of mutable DMX memory.
    if (frame.size() < 116 || frame.size() > 512)
    {
        clearFrame();
        return;
    }
    m_frame = QByteArray(frame.constData(), frame.size());
}

void SoundSwitchEffects::clearFrame() { m_frame.clear(); }

QByteArray SoundSwitchEffects::compose(const QByteArray &selectedFrame) const
{
    if (m_frame.isEmpty())
        return selectedFrame;
    QByteArray result = selectedFrame;
    if (static_cast<uchar>(m_frame[0]) != 0 && result.size() >= 116)
    {
        // Native ForceLTP position latches also write into this private layer.
        // Colour, shutter, gobo, prism, dimmer and UV remain the selected show.
        for (int base : {80, 98})
            for (int channel : {0, 1, 2, 3, 16})
                result[base + channel] = m_frame[base + channel];
    }
    const unsigned active = static_cast<uchar>(m_frame[1]);
    if (active != 0)
    {
        // Both HTP control channels receive the same native grand-master gain.
        // Their ratio avoids applying that gain twice. A closed gate stays zero.
        const unsigned gate = std::min(255U,
            (static_cast<unsigned>(static_cast<uchar>(m_frame[2])) * 255U + active / 2U) / active);
        SoundSwitchIntensity::Levels levels{{static_cast<std::uint8_t>(gate),255,255,255,255,255}};
        SoundSwitchIntensity::scaleFrame(reinterpret_cast<std::uint8_t *>(result.data()),
                                         static_cast<std::size_t>(result.size()), levels);
    }
    return result;
}
