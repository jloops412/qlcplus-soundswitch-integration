/*
  Q Light Controller Plus
  soundswitchdevice.h

  Copyright (c) 2026 QLC+ SoundSwitch Integration contributors

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#ifndef SOUNDSWITCHDEVICE_H
#define SOUNDSWITCHDEVICE_H

#include "soundswitchprotocol.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QThread>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>

struct SoundSwitchDeviceIdentity
{
    SoundSwitchProtocol::DeviceKind kind{SoundSwitchProtocol::DeviceKind::Micro};
    QString instanceId;
    QString interfacePath;
    QString description;
    QString uid;
};

QList<SoundSwitchDeviceIdentity> enumerateSoundSwitchDevices();

class SoundSwitchDevice final : public QThread
{
public:
    explicit SoundSwitchDevice(const SoundSwitchDeviceIdentity &identity,
                               QObject *parent = nullptr);
    ~SoundSwitchDevice() override;

    SoundSwitchProtocol::DeviceKind kind() const;
    QString uid() const;
    QString serial() const;
    QString description() const;
    QString infoText() const;
    int outputCount() const;

    void updateIdentity(const SoundSwitchDeviceIdentity &identity);
    bool openPort(int port);
    void closePort(int port);
    bool hasOpenPorts() const;
    void outputDmx(int port, const QByteArray &data);

protected:
    void run() override;

private:
    enum class State : std::uint8_t
    {
        Disabled,
        Opening,
        WarmingUp,
        Streaming,
        Recovering,
        Fault,
        Closing
    };

    struct Transport;

    bool openTransport();
    void closeTransport(bool blackout);
    bool sendUniverse(std::uint8_t port,
                      const SoundSwitchProtocol::Universe &universe);
    bool writeExact(const std::uint8_t *data, std::size_t size);
    bool anyPortOpenLocked() const;
    static QString stateName(State state);

private:
    mutable std::mutex m_dataMutex;
    mutable std::mutex m_lifecycleMutex;
    SoundSwitchDeviceIdentity m_identity;
    std::array<SoundSwitchProtocol::Universe, 2> m_universes{};
    std::array<bool, 2> m_openPorts{{false, false}};
    std::unique_ptr<Transport> m_transport;
    std::atomic<bool> m_stopRequested{false};
    std::atomic<State> m_state{State::Disabled};
    std::atomic<std::uint32_t> m_lastError{0U};
    std::atomic<std::uint64_t> m_openAttempts{0U};
    std::atomic<std::uint64_t> m_openSuccesses{0U};
    std::atomic<std::uint64_t> m_reconnects{0U};
    std::atomic<std::uint64_t> m_framesAccepted{0U};
    std::atomic<std::uint64_t> m_framesFailed{0U};
};

#endif
