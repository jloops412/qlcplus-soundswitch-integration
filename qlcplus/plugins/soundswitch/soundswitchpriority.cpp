/*
  Q Light Controller Plus
  soundswitchpriority.cpp

  Licensed under the Apache License, Version 2.0.
*/

#include "soundswitchpriority.h"

void SoundSwitchPriorityState::updateLook(quint32 logicalChannel, uchar value)
{
    if (value != 0)
    {
        // QLC+ can report a replacement Toggle before releasing the previous
        // one. Clear on every genuinely new owner so an old private frame is
        // never exposed as the new Look. Base output remains visible until
        // QLC+ writes the fresh private frame.
        if (!m_activeLooks.contains(logicalChannel))
            m_frame.clear();
        m_activeLooks.insert(logicalChannel);
    }
    else
    {
        m_activeLooks.remove(logicalChannel);
        if (m_activeLooks.isEmpty())
            m_frame.clear();
    }
}

void SoundSwitchPriorityState::setFrame(const QByteArray &frame)
{
    m_frame = frame;
}

QByteArray SoundSwitchPriorityState::compose(const QByteArray &baseFrame) const
{
    if (!m_activeLooks.isEmpty() && !m_frame.isEmpty())
        return m_frame;
    return baseFrame;
}

bool SoundSwitchPriorityState::active() const
{
    return !m_activeLooks.isEmpty();
}

bool SoundSwitchPriorityState::contains(quint32 logicalChannel) const
{
    return m_activeLooks.contains(logicalChannel);
}

int SoundSwitchPriorityState::activeCount() const
{
    return m_activeLooks.size();
}

bool SoundSwitchPriorityState::hasFrame() const
{
    return !m_frame.isEmpty();
}
