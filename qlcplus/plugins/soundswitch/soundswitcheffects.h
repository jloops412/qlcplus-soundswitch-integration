/* QLC+ SoundSwitch integration. Licensed under Apache-2.0. */
#ifndef SOUNDSWITCHEFFECTS_H
#define SOUNDSWITCHEFFECTS_H
#include <QByteArray>

// QLC+ supplies the complete native effect frame, including ownership markers.
// No clock, Function scheduler, MIDI latch, or second lighting engine lives here.
class SoundSwitchEffects
{
public:
    void setFrame(const QByteArray &frame);
    void clearFrame();
    QByteArray compose(const QByteArray &selectedFrame) const;
private:
    QByteArray m_frame;
};
#endif
