/*
  Q Light Controller Plus
  soundswitchplugin.h

  Copyright (c) 2026 QLC+ SoundSwitch Integration contributors

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#ifndef SOUNDSWITCHPLUGIN_H
#define SOUNDSWITCHPLUGIN_H

#include "qlcioplugin.h"
#include "soundswitchintensity.h"
#include "soundswitchpriority.h"

#include <QHash>
#include <QMutex>
#include <QSet>
#include <QTimer>
#include <QVector>

class SoundSwitchDevice;
class SoundSwitchMidiInput;

class SoundSwitchPlugin final : public QLCIOPlugin
{
    Q_OBJECT
    Q_INTERFACES(QLCIOPlugin)
    Q_PLUGIN_METADATA(IID QLCIOPlugin_iid)

public:
    ~SoundSwitchPlugin() override;

    void init() override;
    QString name() const override;
    int capabilities() const override;
    QString pluginInfo() const override;

    bool openOutput(quint32 output, quint32 universe) override;
    void closeOutput(quint32 output, quint32 universe) override;
    QStringList outputs() override;
    QStringList outputsUID() override;
    QString outputInfo(quint32 output) override;
    void writeUniverse(quint32 universe, quint32 output,
                       const QByteArray &data, bool dataChanged) override;
    void sendFeedBack(quint32 universe, quint32 output, quint32 channel,
                      uchar value, const QVariant &params) override;

    bool openInput(quint32 input, quint32 universe) override;
    void closeInput(quint32 input, quint32 universe) override;
    QStringList inputs() override;
    QStringList inputsUID() override;
    QString inputInfo(quint32 input) override;

    void configure() override;
    bool canConfigure() const override;

private slots:
    void rescanDevices();
    void midiValueChanged(quint32 channel, uchar value);

private:
    struct OutputBinding
    {
        enum Kind { Dmx, SurfaceFeedback, PriorityLayer } kind{Dmx};
        SoundSwitchDevice *device{nullptr};
        int port{0};
        QString name;
        QString uid;
    };

    bool bindingAt(quint32 output, OutputBinding &binding) const;
    void rebuildBindingsLocked();

private:
    mutable QMutex m_mutex;
    QHash<QString, SoundSwitchDevice *> m_devices;
    QSet<QString> m_presentDeviceUids;
    QVector<OutputBinding> m_bindings;
    QTimer *m_rescanTimer{nullptr};
    SoundSwitchMidiInput *m_midiInput{nullptr};
    QSet<quint32> m_midiUniverses;
    QSet<quint32> m_feedbackUniverses;
    SoundSwitchPriorityState m_priorityState;
    int m_intensityTarget{0}; // 0=global, 1-4=fixture groups, 5=scripted
    SoundSwitchIntensity::Levels m_intensityLevels{{255, 255, 255,
                                                     255, 255, 255}};
    int m_lastGroupNote{-1};
    qint64 m_lastGroupPressMs{0};
};

#endif
