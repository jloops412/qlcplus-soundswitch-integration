/*
  Q Light Controller Plus
  soundswitchdevice.cpp

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

#include "soundswitchdevice.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <setupapi.h>
#include <usb.h>
#include <winusb.h>

#include <QDebug>
#include <QStringList>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <vector>

namespace
{

constexpr GUID MicroInterfaceGuid{
    0xD1AC763B, 0x3888, 0x46C8,
    {0xAB, 0x2F, 0x99, 0xC0, 0x60, 0xEE, 0x05, 0x99}};

class DeviceInfoSet
{
public:
    explicit DeviceInfoSet(HDEVINFO value) : m_value(value) { }
    ~DeviceInfoSet()
    {
        if (m_value != INVALID_HANDLE_VALUE)
            SetupDiDestroyDeviceInfoList(m_value);
    }

    DeviceInfoSet(const DeviceInfoSet &) = delete;
    DeviceInfoSet &operator=(const DeviceInfoSet &) = delete;

    bool valid() const { return m_value != INVALID_HANDLE_VALUE; }
    HDEVINFO get() const { return m_value; }

private:
    HDEVINFO m_value{INVALID_HANDLE_VALUE};
};

class RegistryKey
{
public:
    explicit RegistryKey(HKEY value) : m_value(value) { }
    ~RegistryKey()
    {
        if (m_value != nullptr && m_value != INVALID_HANDLE_VALUE)
            RegCloseKey(m_value);
    }

    RegistryKey(const RegistryKey &) = delete;
    RegistryKey &operator=(const RegistryKey &) = delete;

    bool valid() const
    {
        return m_value != nullptr && m_value != INVALID_HANDLE_VALUE;
    }
    HKEY get() const { return m_value; }

private:
    HKEY m_value{nullptr};
};

QString deviceProperty(HDEVINFO set, SP_DEVINFO_DATA &device, DWORD property)
{
    DWORD type = 0U;
    DWORD required = 0U;
    SetupDiGetDeviceRegistryPropertyW(set, &device, property, &type, nullptr,
                                      0U, &required);
    if (required == 0U)
        return QString();

    std::vector<BYTE> bytes(required + sizeof(wchar_t), 0U);
    if (SetupDiGetDeviceRegistryPropertyW(set, &device, property, &type,
                                          bytes.data(),
                                          static_cast<DWORD>(bytes.size()),
                                          nullptr) == FALSE)
        return QString();

    return QString::fromWCharArray(
        reinterpret_cast<const wchar_t *>(bytes.data()));
}

QString deviceInstanceId(HDEVINFO set, SP_DEVINFO_DATA &device)
{
    DWORD required = 0U;
    SetupDiGetDeviceInstanceIdW(set, &device, nullptr, 0U, &required);
    if (required == 0U)
        return QString();

    std::vector<wchar_t> text(required + 1U, L'\0');
    if (SetupDiGetDeviceInstanceIdW(set, &device, text.data(),
                                    static_cast<DWORD>(text.size()),
                                    nullptr) == FALSE)
        return QString();
    return QString::fromWCharArray(text.data());
}

QStringList registryMultiString(HKEY key, const wchar_t *name)
{
    DWORD type = 0U;
    DWORD byteCount = 0U;
    if (RegQueryValueExW(key, name, nullptr, &type, nullptr, &byteCount) !=
            ERROR_SUCCESS ||
        (type != REG_MULTI_SZ && type != REG_SZ) || byteCount == 0U)
        return QStringList();

    std::vector<wchar_t> buffer(byteCount / sizeof(wchar_t) + 2U, L'\0');
    if (RegQueryValueExW(key, name, nullptr, &type,
                        reinterpret_cast<BYTE *>(buffer.data()),
                        &byteCount) != ERROR_SUCCESS)
        return QStringList();

    QStringList result;
    for (const wchar_t *cursor = buffer.data(); *cursor != L'\0';)
    {
        const QString value = QString::fromWCharArray(cursor);
        result.append(value);
        cursor += value.size() + 1;
        if (type == REG_SZ)
            break;
    }
    return result;
}

QString interfacePathForGuid(const GUID &guid, const QString &serial,
                             const QString &productToken)
{
    DeviceInfoSet set(SetupDiGetClassDevsW(
        &guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));
    if (!set.valid())
        return QString();

    for (DWORD index = 0U;; ++index)
    {
        SP_DEVICE_INTERFACE_DATA interfaceData{};
        interfaceData.cbSize = sizeof(interfaceData);
        if (SetupDiEnumDeviceInterfaces(set.get(), nullptr, &guid, index,
                                        &interfaceData) == FALSE)
            break;

        DWORD required = 0U;
        SetupDiGetDeviceInterfaceDetailW(set.get(), &interfaceData, nullptr,
                                         0U, &required, nullptr);
        if (required < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W))
            continue;

        std::vector<BYTE> bytes(required, 0U);
        auto *detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W *>(
            bytes.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (SetupDiGetDeviceInterfaceDetailW(set.get(), &interfaceData, detail,
                                              required, nullptr, nullptr) ==
            FALSE)
            continue;

        const QString path = QString::fromWCharArray(detail->DevicePath);
        if (path.contains(productToken, Qt::CaseInsensitive) &&
            (serial.isEmpty() || path.contains(serial, Qt::CaseInsensitive)))
            return path;
    }
    return QString();
}

QList<GUID> interfaceGuids(HDEVINFO set, SP_DEVINFO_DATA &device)
{
    RegistryKey key(SetupDiOpenDevRegKey(set, &device, DICS_FLAG_GLOBAL, 0U,
                                         DIREG_DEV, KEY_READ));
    if (!key.valid())
        return QList<GUID>();

    QStringList values = registryMultiString(key.get(), L"DeviceInterfaceGUIDs");
    if (values.isEmpty())
    {
        HKEY rawParameters = nullptr;
        if (RegOpenKeyExW(key.get(), L"Device Parameters", 0U, KEY_READ,
                          &rawParameters) == ERROR_SUCCESS)
        {
            RegistryKey parameters(rawParameters);
            values = registryMultiString(parameters.get(),
                                         L"DeviceInterfaceGUIDs");
        }
    }

    QList<GUID> result;
    for (const QString &value : values)
    {
        GUID guid{};
        std::wstring text = value.toStdWString();
        if (CLSIDFromString(text.data(), &guid) == S_OK)
            result.append(guid);
    }
    return result;
}

QString kindKey(SoundSwitchProtocol::DeviceKind kind)
{
    return kind == SoundSwitchProtocol::DeviceKind::Micro
        ? QStringLiteral("micro") : QStringLiteral("control-one");
}

QList<SoundSwitchDeviceIdentity> enumerateProduct(
    std::uint16_t productId, SoundSwitchProtocol::DeviceKind kind)
{
    QList<SoundSwitchDeviceIdentity> result;
    DeviceInfoSet set(SetupDiGetClassDevsW(
        nullptr, nullptr, nullptr, DIGCF_PRESENT | DIGCF_ALLCLASSES));
    if (!set.valid())
        return result;

    const QString productToken = QStringLiteral("VID_15E4&PID_%1")
        .arg(productId, 4, 16, QLatin1Char('0')).toUpper();

    for (DWORD index = 0U;; ++index)
    {
        SP_DEVINFO_DATA device{};
        device.cbSize = sizeof(device);
        if (SetupDiEnumDeviceInfo(set.get(), index, &device) == FALSE)
            break;

        const QString hardwareId = deviceProperty(set.get(), device,
                                                   SPDRP_HARDWAREID);
        if (!hardwareId.contains(productToken, Qt::CaseInsensitive))
            continue;

        const QString instanceId = deviceInstanceId(set.get(), device);
        const QString serial = instanceId.section(QLatin1Char('\\'), -1);
        QList<GUID> guids = interfaceGuids(set.get(), device);
        if (guids.isEmpty() && kind == SoundSwitchProtocol::DeviceKind::Micro)
            guids.append(MicroInterfaceGuid);

        QString path;
        for (const GUID &guid : guids)
        {
            path = interfacePathForGuid(guid, serial, productToken);
            if (!path.isEmpty())
                break;
        }
        if (path.isEmpty())
            continue;

        SoundSwitchDeviceIdentity identity;
        identity.kind = kind;
        identity.instanceId = instanceId;
        identity.interfacePath = path;
        identity.description = deviceProperty(set.get(), device,
                                              SPDRP_FRIENDLYNAME);
        if (identity.description.isEmpty())
            identity.description = deviceProperty(set.get(), device,
                                                  SPDRP_DEVICEDESC);
        identity.uid = QStringLiteral("soundswitch:%1:%2")
            .arg(kindKey(kind), instanceId.toLower());

        const bool duplicate = std::any_of(
            result.cbegin(), result.cend(), [&identity](const auto &entry) {
                return entry.uid == identity.uid;
            });
        if (!duplicate)
            result.append(identity);
    }
    return result;
}

} // namespace

QList<SoundSwitchDeviceIdentity> enumerateSoundSwitchDevices()
{
    QList<SoundSwitchDeviceIdentity> result = enumerateProduct(
        SoundSwitchProtocol::MicroProductId,
        SoundSwitchProtocol::DeviceKind::Micro);
    result.append(enumerateProduct(
        SoundSwitchProtocol::ControlOneProductId,
        SoundSwitchProtocol::DeviceKind::ControlOne));
    std::sort(result.begin(), result.end(), [](const auto &left,
                                               const auto &right) {
        if (left.kind != right.kind)
            return left.kind < right.kind;
        return left.uid < right.uid;
    });
    return result;
}

struct SoundSwitchDevice::Transport
{
    HANDLE file{INVALID_HANDLE_VALUE};
    WINUSB_INTERFACE_HANDLE usb{nullptr};
};

SoundSwitchDevice::SoundSwitchDevice(
    const SoundSwitchDeviceIdentity &identity, QObject *parent)
    : QThread(parent)
    , m_identity(identity)
    , m_transport(std::make_unique<Transport>())
{
}

SoundSwitchDevice::~SoundSwitchDevice()
{
    std::lock_guard<std::mutex> lifecycleLock(m_lifecycleMutex);
    {
        std::lock_guard<std::mutex> dataLock(m_dataMutex);
        m_openPorts = {{false, false}};
    }
    m_stopRequested.store(true);
    wait();
    closeTransport(true);
}

SoundSwitchProtocol::DeviceKind SoundSwitchDevice::kind() const
{
    std::lock_guard<std::mutex> lock(m_dataMutex);
    return m_identity.kind;
}

QString SoundSwitchDevice::uid() const
{
    std::lock_guard<std::mutex> lock(m_dataMutex);
    return m_identity.uid;
}

QString SoundSwitchDevice::serial() const
{
    std::lock_guard<std::mutex> lock(m_dataMutex);
    return m_identity.instanceId.section(QLatin1Char('\\'), -1);
}

QString SoundSwitchDevice::description() const
{
    std::lock_guard<std::mutex> lock(m_dataMutex);
    return m_identity.description;
}

int SoundSwitchDevice::outputCount() const
{
    return kind() == SoundSwitchProtocol::DeviceKind::Micro ? 1 : 2;
}

QString SoundSwitchDevice::stateName(State state)
{
    switch (state)
    {
    case State::Disabled: return QStringLiteral("Disabled");
    case State::Opening: return QStringLiteral("Opening");
    case State::WarmingUp: return QStringLiteral("Warming up");
    case State::Streaming: return QStringLiteral("Streaming");
    case State::Recovering: return QStringLiteral("Recovering");
    case State::Fault: return QStringLiteral("Fault");
    case State::Closing: return QStringLiteral("Closing");
    }
    return QStringLiteral("Unknown");
}

QString SoundSwitchDevice::infoText() const
{
    return QStringLiteral(
        "<P><B>Device:</B> %1<BR><B>Serial:</B> %2<BR>"
        "<B>Status:</B> %3<BR><B>WinUSB error:</B> %4<BR>"
        "<B>Accepted frames:</B> %5<BR><B>Failed frames:</B> %6<BR>"
        "<B>Reconnects:</B> %7</P>")
        .arg(description().toHtmlEscaped(), serial().toHtmlEscaped(),
             stateName(m_state.load()))
        .arg(m_lastError.load())
        .arg(m_framesAccepted.load())
        .arg(m_framesFailed.load())
        .arg(m_reconnects.load());
}

void SoundSwitchDevice::updateIdentity(
    const SoundSwitchDeviceIdentity &identity)
{
    std::lock_guard<std::mutex> lock(m_dataMutex);
    if (identity.uid == m_identity.uid)
        m_identity = identity;
}

bool SoundSwitchDevice::anyPortOpenLocked() const
{
    return std::any_of(m_openPorts.cbegin(), m_openPorts.cend(),
                       [](bool open) { return open; });
}

bool SoundSwitchDevice::hasOpenPorts() const
{
    std::lock_guard<std::mutex> lock(m_dataMutex);
    return anyPortOpenLocked();
}

bool SoundSwitchDevice::openPort(int port)
{
    if (port < 0 || port >= outputCount())
        return false;

    std::lock_guard<std::mutex> lifecycleLock(m_lifecycleMutex);
    bool firstPort = false;
    {
        std::lock_guard<std::mutex> dataLock(m_dataMutex);
        if (m_openPorts[static_cast<std::size_t>(port)])
            return true;
        firstPort = !anyPortOpenLocked();
        m_openPorts[static_cast<std::size_t>(port)] = true;
    }

    if (firstPort && !openTransport())
    {
        std::lock_guard<std::mutex> dataLock(m_dataMutex);
        m_openPorts[static_cast<std::size_t>(port)] = false;
        return false;
    }

    if (firstPort)
    {
        m_stopRequested.store(false);
        // A 40 Hz DMX writer must never contend with latency-sensitive DJ
        // audio threads. Normal priority is ample for the 25 ms refresh rate.
        start(QThread::NormalPriority);
    }
    return true;
}

void SoundSwitchDevice::closePort(int port)
{
    if (port < 0 || port >= outputCount())
        return;

    std::lock_guard<std::mutex> lifecycleLock(m_lifecycleMutex);
    bool lastPort = false;
    {
        std::lock_guard<std::mutex> dataLock(m_dataMutex);
        m_openPorts[static_cast<std::size_t>(port)] = false;
        m_universes[static_cast<std::size_t>(port)].fill(0U);
        lastPort = !anyPortOpenLocked();
    }
    if (!lastPort)
        return;

    m_stopRequested.store(true);
    wait();
    closeTransport(true);
}

void SoundSwitchDevice::outputDmx(int port, const QByteArray &data)
{
    if (port < 0 || port >= outputCount())
        return;

    SoundSwitchProtocol::Universe normalized{};
    const std::size_t copyLength = std::min<std::size_t>(
        static_cast<std::size_t>(std::max<qsizetype>(0, data.size())),
        normalized.size());
    if (copyLength > 0U)
        std::memcpy(normalized.data(), data.constData(), copyLength);

    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_universes[static_cast<std::size_t>(port)] = normalized;
}

bool SoundSwitchDevice::writeExact(const std::uint8_t *data,
                                   std::size_t size)
{
    if (!m_transport || m_transport->usb == nullptr)
        return false;

    ULONG transferred = 0U;
    if (WinUsb_WritePipe(m_transport->usb, SoundSwitchProtocol::BulkOutPipe,
                         const_cast<PUCHAR>(data), static_cast<ULONG>(size),
                         &transferred, nullptr) == FALSE)
    {
        m_lastError.store(GetLastError());
        m_framesFailed.fetch_add(1U);
        return false;
    }
    if (transferred != static_cast<ULONG>(size))
    {
        m_lastError.store(ERROR_WRITE_FAULT);
        m_framesFailed.fetch_add(1U);
        return false;
    }
    m_lastError.store(ERROR_SUCCESS);
    m_framesAccepted.fetch_add(1U);
    return true;
}

bool SoundSwitchDevice::sendUniverse(
    std::uint8_t port, const SoundSwitchProtocol::Universe &universe)
{
    const auto packet = SoundSwitchProtocol::buildDmxPacket(port, universe);
    return writeExact(packet.data(), packet.size());
}

bool SoundSwitchDevice::openTransport()
{
    if (!m_transport)
        return false;

    closeTransport(false);
    m_state.store(State::Opening);
    m_openAttempts.fetch_add(1U);

    SoundSwitchDeviceIdentity identity;
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        identity = m_identity;
    }

    const std::wstring path = identity.interfacePath.toStdWString();
    m_transport->file = CreateFileW(
        path.c_str(), GENERIC_READ | GENERIC_WRITE, 0U, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
    if (m_transport->file == INVALID_HANDLE_VALUE)
    {
        m_lastError.store(GetLastError());
        m_state.store(State::Fault);
        return false;
    }
    if (WinUsb_Initialize(m_transport->file, &m_transport->usb) == FALSE)
    {
        m_lastError.store(GetLastError());
        closeTransport(false);
        m_state.store(State::Fault);
        return false;
    }

    auto fail = [this](DWORD error) {
        m_lastError.store(error == ERROR_SUCCESS ? ERROR_BAD_DEVICE : error);
        closeTransport(false);
        m_state.store(State::Fault);
        return false;
    };

    USB_DEVICE_DESCRIPTOR deviceDescriptor{};
    ULONG descriptorLength = 0U;
    if (WinUsb_GetDescriptor(
            m_transport->usb, USB_DEVICE_DESCRIPTOR_TYPE, 0U, 0U,
            reinterpret_cast<PUCHAR>(&deviceDescriptor),
            static_cast<ULONG>(sizeof(deviceDescriptor)), &descriptorLength) ==
            FALSE ||
        descriptorLength != sizeof(deviceDescriptor) ||
        deviceDescriptor.idVendor != SoundSwitchProtocol::VendorId ||
        deviceDescriptor.idProduct !=
            (identity.kind == SoundSwitchProtocol::DeviceKind::Micro
                 ? SoundSwitchProtocol::MicroProductId
                 : SoundSwitchProtocol::ControlOneProductId))
        return fail(GetLastError());

    USB_CONFIGURATION_DESCRIPTOR configurationDescriptor{};
    descriptorLength = 0U;
    if (WinUsb_GetDescriptor(
            m_transport->usb, USB_CONFIGURATION_DESCRIPTOR_TYPE, 0U, 0U,
            reinterpret_cast<PUCHAR>(&configurationDescriptor),
            static_cast<ULONG>(sizeof(configurationDescriptor)),
            &descriptorLength) == FALSE ||
        descriptorLength < sizeof(configurationDescriptor) ||
        configurationDescriptor.bConfigurationValue !=
            SoundSwitchProtocol::ConfigurationValue)
        return fail(GetLastError());

    USB_INTERFACE_DESCRIPTOR interfaceDescriptor{};
    if (WinUsb_QueryInterfaceSettings(m_transport->usb, 0U,
                                      &interfaceDescriptor) == FALSE ||
        interfaceDescriptor.bInterfaceNumber !=
            SoundSwitchProtocol::InterfaceNumber)
        return fail(GetLastError());

    UCHAR alternate = 0U;
    if (WinUsb_GetCurrentAlternateSetting(m_transport->usb, &alternate) ==
        FALSE)
        return fail(GetLastError());
    if (alternate != SoundSwitchProtocol::AlternateSetting &&
        WinUsb_SetCurrentAlternateSetting(
            m_transport->usb, SoundSwitchProtocol::AlternateSetting) == FALSE)
        return fail(GetLastError());
    if (WinUsb_GetCurrentAlternateSetting(m_transport->usb, &alternate) ==
            FALSE ||
        alternate != SoundSwitchProtocol::AlternateSetting)
        return fail(GetLastError());

    bool foundPipe = false;
    for (UCHAR index = 0U; index < interfaceDescriptor.bNumEndpoints; ++index)
    {
        WINUSB_PIPE_INFORMATION pipe{};
        if (WinUsb_QueryPipe(m_transport->usb, 0U, index, &pipe) != FALSE &&
            pipe.PipeId == SoundSwitchProtocol::BulkOutPipe &&
            pipe.PipeType == UsbdPipeTypeBulk &&
            pipe.MaximumPacketSize == SoundSwitchProtocol::BulkPacketSize)
            foundPipe = true;
    }
    if (!foundPipe ||
        WinUsb_ResetPipe(m_transport->usb,
                         SoundSwitchProtocol::BulkOutPipe) == FALSE)
        return fail(GetLastError());

    ULONG timeout = 500U;
    if (WinUsb_SetPipePolicy(m_transport->usb,
                             SoundSwitchProtocol::BulkOutPipe,
                             PIPE_TRANSFER_TIMEOUT, sizeof(timeout),
                             &timeout) == FALSE)
        return fail(GetLastError());
    BOOL terminate = FALSE;
    if (WinUsb_SetPipePolicy(m_transport->usb,
                             SoundSwitchProtocol::BulkOutPipe,
                             SHORT_PACKET_TERMINATE, sizeof(terminate),
                             &terminate) == FALSE)
        return fail(GetLastError());
    BOOL rawIo = FALSE;
    if (WinUsb_SetPipePolicy(m_transport->usb,
                             SoundSwitchProtocol::BulkOutPipe, RAW_IO,
                             sizeof(rawIo), &rawIo) == FALSE)
        return fail(GetLastError());

    bool initialized = true;
    if (identity.kind == SoundSwitchProtocol::DeviceKind::Micro)
    {
        for (const auto &packet :
             SoundSwitchProtocol::MicroInitializationPackets)
            initialized = initialized && writeExact(packet.data(), packet.size());
        if (initialized)
            QThread::msleep(200U);
    }
    else
    {
        for (const auto &packet :
             SoundSwitchProtocol::ControlOneInitializationPackets)
            initialized = initialized && writeExact(packet.data(), packet.size());
    }
    if (!initialized)
        return fail(m_lastError.load());

    if (m_openSuccesses.fetch_add(1U) > 0U)
        m_reconnects.fetch_add(1U);
    m_lastError.store(ERROR_SUCCESS);
    m_state.store(State::WarmingUp);
    return true;
}

void SoundSwitchDevice::closeTransport(bool blackout)
{
    if (!m_transport)
        return;

    if (m_transport->usb != nullptr && blackout)
    {
        m_state.store(State::Closing);
        const SoundSwitchProtocol::Universe zero{};
        for (int repetition = 0; repetition < 3; ++repetition)
        {
            if (kind() == SoundSwitchProtocol::DeviceKind::Micro)
                sendUniverse(0U, zero);
            else
            {
                sendUniverse(0U, zero);
                sendUniverse(1U, zero);
            }
            if (repetition < 2)
                QThread::msleep(25U);
        }
    }

    if (m_transport->usb != nullptr)
    {
        WinUsb_Free(m_transport->usb);
        m_transport->usb = nullptr;
    }
    if (m_transport->file != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_transport->file);
        m_transport->file = INVALID_HANDLE_VALUE;
    }
    if (blackout)
        m_state.store(State::Disabled);
}

void SoundSwitchDevice::run()
{
    const auto frameInterval = std::chrono::milliseconds(25);
    int warmupFrames = kind() == SoundSwitchProtocol::DeviceKind::Micro ? 50 : 2;

    while (!m_stopRequested.load())
    {
        const auto frameStart = std::chrono::steady_clock::now();
        if (!m_transport || m_transport->usb == nullptr)
        {
            m_state.store(State::Recovering);
            for (int waitStep = 0;
                 waitStep < 10 && !m_stopRequested.load(); ++waitStep)
                QThread::msleep(100U);
            if (m_stopRequested.load())
                break;
            if (!openTransport())
                continue;
            warmupFrames = kind() == SoundSwitchProtocol::DeviceKind::Micro
                ? 50 : 2;
        }

        std::array<SoundSwitchProtocol::Universe, 2> frames{};
        std::array<bool, 2> openPorts{};
        {
            std::lock_guard<std::mutex> lock(m_dataMutex);
            frames = m_universes;
            openPorts = m_openPorts;
        }

        bool success = true;
        const SoundSwitchProtocol::Universe zero{};
        if (warmupFrames > 0)
        {
            m_state.store(State::WarmingUp);
            success = sendUniverse(0U, zero);
            if (kind() == SoundSwitchProtocol::DeviceKind::ControlOne)
                success = success && sendUniverse(1U, zero);
            --warmupFrames;
        }
        else if (kind() == SoundSwitchProtocol::DeviceKind::Micro)
        {
            m_state.store(State::Streaming);
            success = sendUniverse(0U, openPorts[0] ? frames[0] : zero);
        }
        else
        {
            m_state.store(State::Streaming);
            success = sendUniverse(0U, openPorts[0] ? frames[0] : zero) &&
                      sendUniverse(1U, openPorts[1] ? frames[1] : zero);
        }

        if (!success)
        {
            m_state.store(State::Fault);
            closeTransport(false);
            continue;
        }

        const auto elapsed = std::chrono::steady_clock::now() - frameStart;
        if (elapsed < frameInterval)
        {
            const auto remaining = std::chrono::duration_cast<
                std::chrono::milliseconds>(frameInterval - elapsed);
            QThread::msleep(static_cast<unsigned long>(remaining.count()));
        }
    }
}
