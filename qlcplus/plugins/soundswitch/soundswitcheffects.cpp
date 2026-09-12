/* QLC+ SoundSwitch integration. Licensed under Apache-2.0. */
#include "soundswitcheffects.h"
#include "soundswitchintensity.h"
#include <algorithm>

namespace
{
// U5/U6 mirror the complete 334-channel physical rig. A native HTP channel
// after that rig marks ownership. U4 retains its V32 MOVE/STROBE layout and
// appends separate native WHITE/UV/BLACK ownership channels.
constexpr int ColorActive = 334;
constexpr int WhiteActive = 334;
constexpr int UVActive = 335;
constexpr int BlackActive = 336;

unsigned valueAt(const QByteArray &frame, int channel)
{
    return static_cast<uchar>(frame[channel]);
}

void setColorFrame(QByteArray &destination, const QByteArray &source)
{
    if (source.size() <= ColorActive || source.size() > 512)
    {
        destination.clear();
        return;
    }
    destination = QByteArray(source.constData(), source.size());
}

void recolorGroup(QByteArray &result, const QByteArray &color, int first, int count)
{
    // Never write part of an emitter group supplied by a truncated base frame.
    if (first + count > result.size())
        return;
    unsigned brightness = 0;
    unsigned colorPeak = 0;
    for (int channel = first; channel < first + count; ++channel)
    {
        brightness = std::max(brightness, valueAt(result, channel));
        colorPeak = std::max(colorPeak, valueAt(color, channel));
    }
    // There is no hue to recover from an all-zero template, including one
    // rounded to zero by a very low native grand master. Preserve the show.
    if (colorPeak == 0)
        return;
    for (int channel = first; channel < first + count; ++channel)
        result[channel] = static_cast<char>(
            (valueAt(color, channel) * brightness + colorPeak / 2U) / colorPeak);
}

void scaleIntensity(QByteArray &frame, unsigned gain)
{
    SoundSwitchIntensity::Levels levels{{static_cast<std::uint8_t>(gain),255,255,255,255,255}};
    SoundSwitchIntensity::scaleFrame(reinterpret_cast<std::uint8_t *>(frame.data()),
                                    static_cast<std::size_t>(frame.size()), levels);
}
}

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

void SoundSwitchEffects::setColorLatchFrame(const QByteArray &frame)
{
    setColorFrame(m_colorLatchFrame, frame);
}

void SoundSwitchEffects::setColorHoldFrame(const QByteArray &frame)
{
    setColorFrame(m_colorHoldFrame, frame);
}

void SoundSwitchEffects::clearColorLatchFrame() { m_colorLatchFrame.clear(); }
void SoundSwitchEffects::clearColorHoldFrame() { m_colorHoldFrame.clear(); }

QByteArray SoundSwitchEffects::compose(const QByteArray &selectedFrame) const
{
    QByteArray result = selectedFrame;
    const bool haveEffects = !m_frame.isEmpty();
    const bool havePerformance = m_frame.size() > BlackActive;
    const bool performanceColor = havePerformance &&
        (valueAt(m_frame, WhiteActive) != 0 || valueAt(m_frame, UVActive) != 0);
    const QByteArray *color = nullptr;
    if (!performanceColor)
    {
        if (!m_colorHoldFrame.isEmpty() && valueAt(m_colorHoldFrame, ColorActive) != 0)
            color = &m_colorHoldFrame;
        else if (!m_colorLatchFrame.isEmpty() && valueAt(m_colorLatchFrame, ColorActive) != 0)
            color = &m_colorLatchFrame;
    }
    if (color != nullptr)
    {
        // Each IR-4's six RGBWAUV emitters, each Wash RGBAWUV zone and each
        // tube RGBWA cell retain their own selected-show peak. In particular,
        // a dark cell stays dark and Full Color retains its per-cell palette.
        for (int fixture = 0; fixture < 4; ++fixture)
            recolorGroup(result, *color, fixture * 10 + 1, 6);
        for (int zone = 0; zone < 6; ++zone)
            recolorGroup(result, *color, 44 + zone * 6, 6);
        for (int cell = 0; cell < 32; ++cell)
            recolorGroup(result, *color, 174 + cell * 5, 5);
        // Native Focus fixture channels keep mechanical wheel slots discrete
        // and outside grand-master scaling. Dimmers, aim and optics stay put.
        for (int wheel : {84, 102})
            if (wheel < result.size())
                result[wheel] = (*color)[wheel];
    }
    if (haveEffects && valueAt(m_frame, 0) != 0 && result.size() >= 116)
    {
        // Native ForceLTP position latches also write into this private layer.
        // Colour, shutter, gobo, prism, dimmer and UV remain the selected show.
        for (int base : {80, 98})
            for (int channel : {0, 1, 2, 3, 16})
                result[base + channel] = m_frame[base + channel];
    }
    const unsigned active = haveEffects ? valueAt(m_frame, 1) : 0;
    if (active != 0)
    {
        // Both HTP control channels receive the same native grand-master gain.
        // Their ratio avoids applying that gain twice. A closed gate stays zero.
        const unsigned gate = std::min(255U,
            (valueAt(m_frame, 2) * 255U + active / 2U) / active);
        scaleIntensity(result, gate);
    }
    // BLACK is an intensity-only final gate. Later WHITE, UV, color or MOVE
    // input cannot relight the rig while either native BLACK owner is active.
    if (havePerformance && valueAt(m_frame, BlackActive) != 0)
        scaleIntensity(result, 0);
    return result;
}
