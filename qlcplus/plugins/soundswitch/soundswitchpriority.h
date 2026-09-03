/*
  Q Light Controller Plus
  soundswitchpriority.h

  Licensed under the Apache License, Version 2.0.
*/

#ifndef SOUNDSWITCHPRIORITY_H
#define SOUNDSWITCHPRIORITY_H

#include <QByteArray>
#include <QSet>
#include <QtGlobal>

class SoundSwitchPriorityState
{
public:
    void updateLook(quint32 logicalChannel, uchar value);
    void setFrame(const QByteArray &frame);
    QByteArray compose(const QByteArray &baseFrame) const;

    bool active() const;
    bool contains(quint32 logicalChannel) const;
    int activeCount() const;
    bool hasFrame() const;

private:
    QSet<quint32> m_activeLooks;
    QByteArray m_frame;
};

#endif
