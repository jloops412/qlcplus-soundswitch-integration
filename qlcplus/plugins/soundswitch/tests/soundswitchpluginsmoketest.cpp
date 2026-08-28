/*
  Q Light Controller Plus
  soundswitchpluginsmoketest.cpp

  Copyright (c) 2026 QLC+ SoundSwitch Integration contributors

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0
*/

#include <iostream>

#include "qlcioplugin.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QPluginLoader>
#include <QStringList>
#include <QVector>

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    if (argc != 2 && argc != 3)
    {
        std::cerr << "usage: soundswitch_plugin_smoke_tests <plugin> "
                     "[--open-check|--allow-no-hardware]\n";
        return 2;
    }
    const bool openCheck = argc == 3 &&
        QString::fromLocal8Bit(argv[2]) == QStringLiteral("--open-check");
    const bool allowNoHardware = argc == 3 &&
        QString::fromLocal8Bit(argv[2]) ==
            QStringLiteral("--allow-no-hardware");
    if (argc == 3 && !openCheck && !allowNoHardware)
    {
        std::cerr << "unknown option\n";
        return 2;
    }

    QPluginLoader loader(QString::fromLocal8Bit(argv[1]));
    QObject *instance = loader.instance();
    if (instance == nullptr)
    {
        std::cerr << loader.errorString().toStdString() << '\n';
        return 3;
    }

    QLCIOPlugin *plugin = qobject_cast<QLCIOPlugin *>(instance);
    if (plugin == nullptr)
    {
        std::cerr << "DLL does not implement QLCIOPlugin\n";
        return 4;
    }

    plugin->init();
    const QStringList names = plugin->outputs();
    const QStringList uids = plugin->outputsUID();
    if (plugin->name() != QStringLiteral("SoundSwitch Hardware") ||
        names.size() != uids.size())
    {
        std::cerr << "plugin identity or output UID contract failed\n";
        return 5;
    }

    const qsizetype microIndex = names.indexOf(
        QStringLiteral("SoundSwitch Micro — DMX"));
    const qsizetype controlOneIndex = names.indexOf(
        QStringLiteral("SoundSwitch Control One — DMX 1"));
    const qsizetype controlOneSecondIndex = names.indexOf(
        QStringLiteral("SoundSwitch Control One — DMX 2"));
    const qsizetype feedbackIndex = names.indexOf(
        QStringLiteral("SoundSwitch Control One — Surface Feedback"));
    const qsizetype priorityIndex = names.indexOf(
        QStringLiteral("SoundSwitch Hardware — Priority Looks Layer"));
    const bool priorityValid = priorityIndex >= 0 &&
        uids.at(priorityIndex) == QStringLiteral("soundswitch:priority-layer");
    const bool microValid = microIndex >= 0 &&
        uids.at(microIndex).startsWith(QStringLiteral("soundswitch:micro:"));
    const bool controlOneValid = controlOneIndex >= 0 &&
        controlOneSecondIndex >= 0 && feedbackIndex >= 0 &&
        uids.at(controlOneIndex).startsWith(QStringLiteral("soundswitch:control-one:")) &&
        uids.at(controlOneSecondIndex).startsWith(QStringLiteral("soundswitch:control-one:")) &&
        uids.at(feedbackIndex) == QStringLiteral("soundswitch:controlone:surface");
    const bool surfaceValid = feedbackIndex >= 0 &&
        uids.at(feedbackIndex) == QStringLiteral("soundswitch:controlone:surface");
    if (!priorityValid)
    {
        std::cerr << "priority layer binding is unavailable\n";
        return 6;
    }
    if (!surfaceValid)
    {
        std::cerr << "hardware-independent surface command line is unavailable\n";
        return 7;
    }
    if (!microValid && !controlOneValid && !allowNoHardware)
    {
        std::cerr << "no supported SoundSwitch hardware was enumerated\n";
        return 8;
    }

    if (!plugin->openOutput(static_cast<quint32>(priorityIndex), 2U))
    {
        std::cerr << "priority layer did not open as a virtual output\n";
        return 9;
    }
    plugin->writeUniverse(2U, static_cast<quint32>(priorityIndex),
                          QByteArray(334, '\0'), true);
    plugin->closeOutput(static_cast<quint32>(priorityIndex), 2U);

    if (!plugin->openOutput(static_cast<quint32>(feedbackIndex), 1U))
    {
        std::cerr << "surface command line requires attached hardware\n";
        return 10;
    }

    if (!plugin->openInput(0U, 1U))
    {
        std::cerr << "logical Control One input did not open without hardware\n";
        return 11;
    }
    QVector<QPair<quint32, uchar>> routedValues;
    QObject::connect(plugin, &QLCIOPlugin::valueChanged,
        [&routedValues](quint32 universe, quint32 input, quint32 channel,
                        uchar value, const QString &) {
            if (universe == 1U && input == 0U)
                routedValues.append(qMakePair(channel, value));
        });

    // Priority ownership and Virtual Console commands share QLC+'s single
    // feedback patch. Ownership channels must be consumed by the plug-in and
    // must not leak back into the logical Control One input.
    plugin->sendFeedBack(1U, static_cast<quint32>(feedbackIndex),
                         600U, 255U, QVariant());
    QCoreApplication::processEvents();
    if (!routedValues.isEmpty())
    {
        std::cerr << "priority ownership leaked into surface input\n";
        return 12;
    }

    plugin->sendFeedBack(1U, static_cast<quint32>(feedbackIndex),
                         811U, 255U, QVariant());
    QCoreApplication::processEvents();
    const QVector<QPair<quint32, uchar>> expectedModePulse{
        qMakePair(510U, static_cast<uchar>(255)),
        qMakePair(510U, static_cast<uchar>(0)),
        qMakePair(502U, static_cast<uchar>(255)),
        qMakePair(502U, static_cast<uchar>(0))
    };
    if (routedValues != expectedModePulse)
    {
        std::cerr << "positive mode command did not emit one page change\n";
        return 13;
    }

    plugin->sendFeedBack(1U, static_cast<quint32>(feedbackIndex),
                         811U, 0U, QVariant());
    QCoreApplication::processEvents();
    if (routedValues != expectedModePulse)
    {
        std::cerr << "trailing UI Scene zero repeated the mode command\n";
        return 14;
    }

    routedValues.clear();
    plugin->sendFeedBack(1U, static_cast<quint32>(feedbackIndex),
                         811U, 255U, QVariant());
    QCoreApplication::processEvents();
    const QVector<QPair<quint32, uchar>> expectedAutoloopPulse{
        qMakePair(510U, static_cast<uchar>(255)),
        qMakePair(510U, static_cast<uchar>(0)),
        qMakePair(60U, static_cast<uchar>(255)),
        qMakePair(60U, static_cast<uchar>(0)),
        qMakePair(32U, static_cast<uchar>(255)),
        qMakePair(32U, static_cast<uchar>(0))
    };
    if (routedValues != expectedAutoloopPulse)
    {
        std::cerr << "second mode command did not return to Autoloops\n";
        return 15;
    }
    plugin->sendFeedBack(1U, static_cast<quint32>(feedbackIndex),
                         811U, 0U, QVariant());
    QCoreApplication::processEvents();
    if (routedValues != expectedAutoloopPulse)
    {
        std::cerr << "trailing mode zero repeated the Autoloops command\n";
        return 16;
    }

    routedValues.clear();
    plugin->sendFeedBack(1U, static_cast<quint32>(feedbackIndex),
                         802U, 255U, QVariant());
    QCoreApplication::processEvents();
    const QVector<QPair<quint32, uchar>> expectedBankPulse{
        qMakePair(34U, static_cast<uchar>(255)),
        qMakePair(34U, static_cast<uchar>(0))
    };
    if (routedValues != expectedBankPulse)
    {
        std::cerr << "Bank 3 UI command did not select native bank page 3\n";
        return 17;
    }
    plugin->sendFeedBack(1U, static_cast<quint32>(feedbackIndex),
                         802U, 0U, QVariant());
    QCoreApplication::processEvents();
    if (routedValues != expectedBankPulse)
    {
        std::cerr << "trailing bank zero repeated the selection command\n";
        return 18;
    }

    routedValues.clear();
    plugin->sendFeedBack(1U, static_cast<quint32>(feedbackIndex),
                         815U, 255U, QVariant());
    QCoreApplication::processEvents();
    const QVector<QPair<quint32, uchar>> expectedSpeedPulse{
        qMakePair(473U, static_cast<uchar>(255)),
        qMakePair(473U, static_cast<uchar>(0))
    };
    if (routedValues != expectedSpeedPulse)
    {
        std::cerr << "2x chase-speed UI command did not select preset 3\n";
        return 19;
    }
    plugin->sendFeedBack(1U, static_cast<quint32>(feedbackIndex),
                         815U, 0U, QVariant());
    QCoreApplication::processEvents();
    if (routedValues != expectedSpeedPulse)
    {
        std::cerr << "trailing speed zero repeated the preset command\n";
        return 20;
    }

    plugin->sendFeedBack(1U, static_cast<quint32>(feedbackIndex),
                         600U, 0U, QVariant());
    plugin->closeInput(0U, 1U);
    plugin->closeOutput(static_cast<quint32>(feedbackIndex), 1U);

    std::cout << "Loaded: " << plugin->name().toStdString() << '\n';
    if (!microValid && !controlOneValid)
    {
        std::cout << "PASS: plug-in ABI and virtual priority output; "
                     "no physical hardware attached\n";
        return 0;
    }
    const qsizetype primaryIndex = microValid ? microIndex : controlOneIndex;
    std::cout << "Output: " << names.at(primaryIndex).toStdString() << '\n';
    std::cout << "UID: " << uids.at(primaryIndex).toStdString() << '\n';
    if (controlOneValid)
        std::cout << "Surface feedback: available\n";
    if (openCheck)
    {
        const bool opened = plugin->openOutput(
            static_cast<quint32>(primaryIndex), 0U);
        std::cout << "Open result: " << (opened ? "OPEN" : "BUSY/FAILED")
                  << '\n';
        std::cout << "Status: "
                  << plugin->outputInfo(static_cast<quint32>(primaryIndex))
                         .toStdString()
                  << '\n';
        if (opened)
            plugin->closeOutput(static_cast<quint32>(primaryIndex), 0U);
        return 0;
    }
    std::cout << "PASS: enumeration only; no output was opened\n";
    return 0;
}
