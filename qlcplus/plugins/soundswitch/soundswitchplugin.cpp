/*
  Q Light Controller Plus
  soundswitchplugin.cpp

  Copyright (c) 2026 EmberLights contributors

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

#include "soundswitchplugin.h"

#include "soundswitchdevice.h"
#include "soundswitchmidiinput.h"

#include <QMutexLocker>
#include <QDateTime>

#include <algorithm>

namespace
{
// QLC stores DMX channels as zero-based indexes. The current performance rig
// has four IR-4 master channels at addresses 1/11/21/31 and four consecutive
// 40-channel BO-TUBE192 fixtures at addresses 175/215/255/295.
constexpr std::array<int, 4> kIr4MasterChannels{{0, 10, 20, 30}};
constexpr int kTubeFirstChannel = 174;
constexpr int kTubeChannelCount = 4 * 40;
constexpr int kTubeEndChannel = kTubeFirstChannel + kTubeChannelCount;
constexpr quint32 kPriorityLookChannelBase = 600;
constexpr quint32 kPriorityLookChannelCount = 32;
}

SoundSwitchPlugin::~SoundSwitchPlugin()
{
    if (m_rescanTimer != nullptr)
        m_rescanTimer->stop();

    QVector<SoundSwitchDevice *> devices;
    {
        QMutexLocker lock(&m_mutex);
        devices.reserve(m_devices.size());
        for (SoundSwitchDevice *device : std::as_const(m_devices))
            devices.append(device);
        m_bindings.clear();
        m_devices.clear();
        m_presentDeviceUids.clear();
    }
    qDeleteAll(devices);
}

void SoundSwitchPlugin::init()
{
    m_midiInput = new SoundSwitchMidiInput(this);
    connect(m_midiInput, &SoundSwitchMidiInput::valueChanged,
            this, &SoundSwitchPlugin::midiValueChanged);
    rescanDevices();
    m_rescanTimer = new QTimer(this);
    m_rescanTimer->setInterval(2000);
    connect(m_rescanTimer, &QTimer::timeout,
            this, &SoundSwitchPlugin::rescanDevices);
    m_rescanTimer->start();
}

QString SoundSwitchPlugin::name() const
{
    return QStringLiteral("SoundSwitch Hardware");
}

int SoundSwitchPlugin::capabilities() const
{
    return QLCIOPlugin::Output | QLCIOPlugin::Input | QLCIOPlugin::Feedback;
}

QString SoundSwitchPlugin::pluginInfo() const
{
    return QStringLiteral(
        "<HTML><HEAD><TITLE>SoundSwitch Hardware</TITLE></HEAD><BODY>"
        "<H3>SoundSwitch Hardware</H3>"
        "<P>Native QLC+ DMX output for SoundSwitch Micro and Control One. "
        "Control One exposes its two physical DMX ports as separate output "
        "lines and its performance surface as one native QLC+ input. Shifted "
        "controls are translated to distinct logical channels inside the "
        "plugin. A private Priority Looks line lets a QLC Scene or Chaser own "
        "the complete output frame while the base Autoloop continues. USB "
        "output is isolated on a worker thread and disconnected "
        "outputs are retried automatically. The Control One MIDI endpoint is "
        "also monitored and reopened after a disconnect or USB replug without "
        "restarting QLC+.</P></BODY></HTML>");
}

bool SoundSwitchPlugin::bindingAt(quint32 output,
                                  OutputBinding &binding) const
{
    QMutexLocker lock(&m_mutex);
    if (output >= static_cast<quint32>(m_bindings.size()))
        return false;
    binding = m_bindings.at(static_cast<qsizetype>(output));
    return binding.kind == OutputBinding::SurfaceFeedback ||
           binding.kind == OutputBinding::PriorityLayer ||
           binding.device != nullptr;
}

bool SoundSwitchPlugin::openOutput(quint32 output, quint32 universe)
{
    OutputBinding binding;
    if (!bindingAt(output, binding))
        return false;

    if (binding.kind == OutputBinding::PriorityLayer)
    {
        // This virtual output is a private full-frame buffer. It never opens
        // a USB port; the normal DMX bindings select this frame only while a
        // Priority Look owns the live output.
    }
    else if (binding.kind == OutputBinding::SurfaceFeedback)
    {
        if (m_midiInput == nullptr)
            return false;

        // Surface feedback is also the Virtual Console command return path.
        // Keep that logical line open even when Control One is unplugged so
        // mouse controls continue to work and the MIDI output can reconnect
        // later without forcing QLC+ to repatch the universe.
        m_midiInput->ensureFeedbackConnected();
        {
            QMutexLocker lock(&m_mutex);
            m_feedbackUniverses.insert(universe);
        }
    }
    else if (binding.device == nullptr || !binding.device->openPort(binding.port))
        return false;

    addToMap(universe, output, Output);
    return true;
}

void SoundSwitchPlugin::closeOutput(quint32 output, quint32 universe)
{
    OutputBinding binding;
    if (bindingAt(output, binding))
    {
        if (binding.kind == OutputBinding::PriorityLayer)
        {
            // The same virtual line can be patched as the overlay DMX output
            // and as its feedback control path. Keep its buffered state until
            // the workspace is replaced or the plug-in is unloaded.
        }
        else if (binding.kind == OutputBinding::SurfaceFeedback)
        {
            bool closeFeedback = false;
            {
                QMutexLocker lock(&m_mutex);
                m_feedbackUniverses.remove(universe);
                closeFeedback = m_feedbackUniverses.isEmpty();
            }
            if (closeFeedback && m_midiInput != nullptr)
                m_midiInput->closeFeedback();
        }
        else if (binding.device != nullptr)
            binding.device->closePort(binding.port);
    }
    removeFromMap(universe, output, Output);
}

QStringList SoundSwitchPlugin::outputs()
{
    QMutexLocker lock(&m_mutex);
    QStringList result;
    result.reserve(m_bindings.size());
    for (const OutputBinding &binding : std::as_const(m_bindings))
        result.append(binding.name);
    return result;
}

QStringList SoundSwitchPlugin::outputsUID()
{
    QMutexLocker lock(&m_mutex);
    QStringList result;
    result.reserve(m_bindings.size());
    for (const OutputBinding &binding : std::as_const(m_bindings))
        result.append(binding.uid);
    return result;
}

QString SoundSwitchPlugin::outputInfo(quint32 output)
{
    if (output == QLCIOPlugin::invalidLine())
        return pluginInfo();

    OutputBinding binding;
    if (!bindingAt(output, binding))
        return QStringLiteral("<HTML><BODY><P>Output unavailable.</P>"
                              "</BODY></HTML>");

    if (binding.kind == OutputBinding::PriorityLayer)
        return QStringLiteral(
            "<HTML><BODY><H3>%1</H3><P>Internal full-frame Priority Looks "
            "layer. Patch a private QLC+ universe here; no additional "
            "hardware or program is required.</P></BODY></HTML>")
            .arg(binding.name.toHtmlEscaped());
    if (binding.kind == OutputBinding::SurfaceFeedback)
        return QStringLiteral("<HTML><BODY><H3>%1</H3><P>Control One MIDI "
                              "LED feedback output.</P></BODY></HTML>")
            .arg(binding.name.toHtmlEscaped());
    return QStringLiteral("<HTML><BODY><H3>%1</H3>%2</BODY></HTML>")
        .arg(binding.name.toHtmlEscaped(), binding.device->infoText());
}

void SoundSwitchPlugin::writeUniverse(quint32 universe, quint32 output,
                                      const QByteArray &data,
                                      bool dataChanged)
{
    Q_UNUSED(universe)
    Q_UNUSED(dataChanged)

    OutputBinding binding;
    if (!bindingAt(output, binding))
        return;

    if (binding.kind == OutputBinding::PriorityLayer)
    {
        QMutexLocker lock(&m_mutex);
        m_priorityLayerFrame = data;
        return;
    }

    if (binding.kind == OutputBinding::SurfaceFeedback || binding.device == nullptr)
        return;

    QByteArray outputData = data;
    {
        QMutexLocker lock(&m_mutex);

        // Autoloops keep advancing on Universe 1. A Scene or Chaser on the
        // private Priority Looks universe replaces the complete physical
        // frame only while its Toggle button is active. Releasing it reveals
        // the base universe at its current step, with no restart or handoff.
        if (universe == 0 && !m_activePriorityLooks.isEmpty() &&
            !m_priorityLayerFrame.isEmpty())
        {
            outputData = m_priorityLayerFrame;
        }

        // Scale only the IR-4 master channels. BO-TUBE192's 40-channel mode is
        // eight RGBWY emitter zones with no independent master, so Group 3
        // scales all 160 channels across the four tubes. Groups 2 and 4 remain
        // remembered expansion targets and intentionally modify no DMX yet.
        if (universe == 0)
        {
            const int global = m_intensityLevels[0];
            const int irGroup = m_intensityLevels[1];
            const int tubeGroup = m_intensityLevels[3];
            const int irScale = (global * irGroup + 127) / 255;
            const int tubeScale = (global * tubeGroup + 127) / 255;

            for (int channel : kIr4MasterChannels)
            {
                if (channel >= outputData.size())
                    continue;
                const int source = static_cast<uchar>(outputData.at(channel));
                outputData[channel] = static_cast<char>((source * irScale + 127) / 255);
            }
            for (int channel = kTubeFirstChannel;
                 channel < kTubeEndChannel && channel < outputData.size();
                 ++channel)
            {
                const int source = static_cast<uchar>(outputData.at(channel));
                outputData[channel] = static_cast<char>((source * tubeScale + 127) / 255);
            }
        }

    }

    binding.device->outputDmx(binding.port, outputData);
}

void SoundSwitchPlugin::sendFeedBack(quint32 universe, quint32 output,
                                     quint32 channel, uchar value,
                                     const QVariant &params)
{
    Q_UNUSED(universe)
    Q_UNUSED(params)

    OutputBinding binding;
    if (!bindingAt(output, binding))
        return;

    const quint32 logicalChannel = channel & 0xffffU;
    const bool priorityLookControl =
        logicalChannel >= kPriorityLookChannelBase &&
        logicalChannel < kPriorityLookChannelBase + kPriorityLookChannelCount;

    // QLC+ supports one feedback patch per universe. The performance surface
    // and Priority Look ownership controls intentionally share that one line,
    // while the separate PriorityLayer output continues to buffer the overlay
    // DMX frame. Accept the ownership channels on either binding so older
    // workspaces remain compatible.
    if (priorityLookControl &&
        (binding.kind == OutputBinding::SurfaceFeedback ||
         binding.kind == OutputBinding::PriorityLayer))
    {
        QMutexLocker lock(&m_mutex);
        if (value != 0)
            m_activePriorityLooks.insert(logicalChannel);
        else
            m_activePriorityLooks.remove(logicalChannel);
        return;
    }

    if (binding.kind == OutputBinding::PriorityLayer)
        return;

    if (binding.kind == OutputBinding::SurfaceFeedback && m_midiInput != nullptr)
        m_midiInput->applyFeedback(channel, value);
}

bool SoundSwitchPlugin::openInput(quint32 input, quint32 universe)
{
    if (input != 0 || m_midiInput == nullptr)
        return false;

    {
        QMutexLocker lock(&m_mutex);
        m_midiUniverses.insert(universe);
    }
    addToMap(universe, input, Input);
    // Keep the logical QLC input open even if the USB endpoint is temporarily
    // absent. rescanDevices() will retry the native WinMM handle every two
    // seconds, so an unplug/replug never requires an application restart.
    m_midiInput->ensureConnected();
    return true;
}

void SoundSwitchPlugin::closeInput(quint32 input, quint32 universe)
{
    if (input != 0)
        return;

    bool closeDevice = false;
    {
        QMutexLocker lock(&m_mutex);
        m_midiUniverses.remove(universe);
        closeDevice = m_midiUniverses.isEmpty();
    }
    removeFromMap(universe, input, Input);
    if (closeDevice && m_midiInput != nullptr)
        m_midiInput->close();
}

QStringList SoundSwitchPlugin::inputs()
{
    return {QStringLiteral("SoundSwitch Control One — Performance")};
}

QStringList SoundSwitchPlugin::inputsUID()
{
    return {QStringLiteral("soundswitch:controlone:midi")};
}

QString SoundSwitchPlugin::inputInfo(quint32 input)
{
    if (input == QLCIOPlugin::invalidLine())
        return pluginInfo();
    if (input != 0)
        return QStringLiteral("<HTML><BODY><P>Input unavailable.</P></BODY></HTML>");

    const QString state = m_midiInput != nullptr && m_midiInput->isOpen()
        ? QStringLiteral("connected")
        : (SoundSwitchMidiInput::isPresent()
            ? QStringLiteral("available; reconnect pending")
            : QStringLiteral("not connected; retrying automatically"));
    return QStringLiteral(
        "<HTML><BODY><H3>SoundSwitch Control One — Performance</H3>"
        "<P>MIDI interface is %1. Base notes use logical channels 0–127; "
        "Shifted notes use 128–255; controllers use 256–383 and shifted "
        "controllers use 384–511. Priority Look pads use synthetic channels "
        "600–631.</P></BODY></HTML>").arg(state);
}

void SoundSwitchPlugin::midiValueChanged(quint32 channel, uchar value)
{
    QVector<quint32> universes;
    QVector<QPair<quint32, uchar>> routedValues;
    routedValues.append(qMakePair(channel, value));
    {
        QMutexLocker lock(&m_mutex);
        universes = m_midiUniverses.values().toVector();

        int nextTarget = -1;
        if (channel == 61 && value != 0)
            nextTarget = 0;
        else if (channel == 189 && value != 0)
            nextTarget = 5;
        else if ((channel == 62 || channel == 63) && value != 0)
        {
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            const bool doublePress = m_lastGroupNote == static_cast<int>(channel) &&
                                     now - m_lastGroupPressMs <= 400;
            nextTarget = channel == 62 ? (doublePress ? 2 : 1)
                                       : (doublePress ? 4 : 3);
            m_lastGroupNote = doublePress ? -1 : static_cast<int>(channel);
            m_lastGroupPressMs = doublePress ? 0 : now;
        }

        if (nextTarget >= 0 && nextTarget != m_intensityTarget)
        {
            routedValues.append(qMakePair(
                static_cast<quint32>(503 + m_intensityTarget), static_cast<uchar>(0)));
            m_intensityTarget = nextTarget;
            routedValues.append(qMakePair(
                static_cast<quint32>(503 + m_intensityTarget), static_cast<uchar>(255)));
            // Refresh the visible intensity page with the value remembered
            // for the newly selected target. Scaling remains in writeUniverse().
            routedValues.append(qMakePair(
                static_cast<quint32>(511),
                m_intensityLevels[static_cast<std::size_t>(m_intensityTarget)]));
            m_midiInput->setIntensityTarget(m_intensityTarget);
        }

        if (channel == 511)
            m_intensityLevels[static_cast<std::size_t>(m_intensityTarget)] = value;
    }
    for (quint32 universe : std::as_const(universes))
    {
        for (const auto &routedValue : std::as_const(routedValues))
            emit valueChanged(universe, 0, routedValue.first, routedValue.second);
    }
}

void SoundSwitchPlugin::configure()
{
    rescanDevices();
}

bool SoundSwitchPlugin::canConfigure() const
{
    return true;
}

void SoundSwitchPlugin::rebuildBindingsLocked()
{
    // QLC+ keeps the numeric output line when reconnecting a patch. Never
    // reorder or remove an output that has already been advertised during
    // this plugin instance, otherwise hot-plugging another device can make a
    // patched universe reconnect to the wrong physical interface. Sort only
    // devices that have not been advertised yet and append their lines.
    QSet<QString> boundDeviceUids;
    for (const OutputBinding &binding : std::as_const(m_bindings))
    {
        if (binding.kind == OutputBinding::Dmx && binding.device != nullptr)
            boundDeviceUids.insert(binding.device->uid());
    }

    QVector<SoundSwitchDevice *> newDevices;
    newDevices.reserve(m_devices.size());
    for (SoundSwitchDevice *device : std::as_const(m_devices))
    {
        if (!boundDeviceUids.contains(device->uid()))
            newDevices.append(device);
    }

    std::sort(newDevices.begin(), newDevices.end(),
              [](const SoundSwitchDevice *left,
                 const SoundSwitchDevice *right) {
        if (left->kind() != right->kind())
            return left->kind() < right->kind();
        return left->uid() < right->uid();
    });

    for (SoundSwitchDevice *device : std::as_const(newDevices))
    {
        for (int port = 0; port < device->outputCount(); ++port)
        {
            OutputBinding binding;
            binding.kind = OutputBinding::Dmx;
            binding.device = device;
            binding.port = port;
            binding.uid = QStringLiteral("%1:dmx:%2")
                .arg(device->uid()).arg(port + 1);
            m_bindings.append(binding);
        }
    }

    const bool hasSurfaceBinding = std::any_of(
        m_bindings.cbegin(), m_bindings.cend(),
        [](const OutputBinding &binding) {
            return binding.kind == OutputBinding::SurfaceFeedback;
        });
    if (!hasSurfaceBinding)
    {
        OutputBinding binding;
        binding.kind = OutputBinding::SurfaceFeedback;
        binding.name = QStringLiteral("SoundSwitch Control One — Surface Feedback");
        binding.uid = QStringLiteral("soundswitch:controlone:surface");
        m_bindings.append(binding);
    }

    const bool hasPriorityBinding = std::any_of(
        m_bindings.cbegin(), m_bindings.cend(),
        [](const OutputBinding &binding) {
            return binding.kind == OutputBinding::PriorityLayer;
        });
    if (!hasPriorityBinding)
    {
        OutputBinding binding;
        binding.kind = OutputBinding::PriorityLayer;
        binding.name = QStringLiteral("SoundSwitch Hardware — Priority Looks Layer");
        binding.uid = QStringLiteral("soundswitch:priority-layer");
        m_bindings.append(binding);
    }

    int microCount = 0;
    int controlOneCount = 0;
    for (const SoundSwitchDevice *device : std::as_const(m_devices))
    {
        if (device->kind() == SoundSwitchProtocol::DeviceKind::Micro)
            ++microCount;
        else
            ++controlOneCount;
    }

    for (OutputBinding &binding : m_bindings)
    {
        if (binding.kind != OutputBinding::Dmx)
            continue;
        SoundSwitchDevice *device = binding.device;
        const bool micro =
            device->kind() == SoundSwitchProtocol::DeviceKind::Micro;
        const int duplicateCount = micro ? microCount : controlOneCount;
        QString deviceName = micro
            ? QStringLiteral("SoundSwitch Micro")
            : QStringLiteral("SoundSwitch Control One");
        if (duplicateCount > 1)
            deviceName += QStringLiteral(" (%1)").arg(device->serial());

        binding.name = micro
            ? deviceName + QStringLiteral(" — DMX")
            : deviceName + QStringLiteral(" — DMX %1")
                .arg(binding.port + 1);
    }
}

void SoundSwitchPlugin::rescanDevices()
{
    bool midiRequired = false;
    bool feedbackRequired = false;
    {
        QMutexLocker lock(&m_mutex);
        midiRequired = !m_midiUniverses.isEmpty();
        feedbackRequired = !m_feedbackUniverses.isEmpty();
    }
    if (midiRequired && m_midiInput != nullptr)
        m_midiInput->ensureConnected();
    if (feedbackRequired && m_midiInput != nullptr)
        m_midiInput->ensureFeedbackConnected();

    const QList<SoundSwitchDeviceIdentity> found =
        enumerateSoundSwitchDevices();

    QStringList previousUids;
    QStringList currentUids;
    {
        QMutexLocker lock(&m_mutex);
        for (const OutputBinding &binding : std::as_const(m_bindings))
            previousUids.append(binding.uid);

        QSet<QString> present;
        for (const SoundSwitchDeviceIdentity &identity : found)
        {
            present.insert(identity.uid);
            SoundSwitchDevice *device = m_devices.value(identity.uid, nullptr);
            if (device == nullptr)
            {
                device = new SoundSwitchDevice(identity, this);
                m_devices.insert(identity.uid, device);
            }
            else
                device->updateIdentity(identity);
        }
        m_presentDeviceUids = present;
        rebuildBindingsLocked();

        for (const OutputBinding &binding : std::as_const(m_bindings))
            currentUids.append(binding.uid);
    }

    if (previousUids != currentUids)
        emit configurationChanged();
}
