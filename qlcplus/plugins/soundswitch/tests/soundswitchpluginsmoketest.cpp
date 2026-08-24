/*
  Q Light Controller Plus
  soundswitchpluginsmoketest.cpp

  Copyright (c) 2026 EmberLights contributors

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
    if (!priorityValid)
    {
        std::cerr << "priority layer binding is unavailable\n";
        return 6;
    }
    if (!microValid && !controlOneValid && !allowNoHardware)
    {
        std::cerr << "no supported SoundSwitch hardware was enumerated\n";
        return 7;
    }

    if (!plugin->openOutput(static_cast<quint32>(priorityIndex), 2U))
    {
        std::cerr << "priority layer did not open as a virtual output\n";
        return 8;
    }
    plugin->writeUniverse(2U, static_cast<quint32>(priorityIndex),
                          QByteArray(334, '\0'), true);
    plugin->closeOutput(static_cast<quint32>(priorityIndex), 2U);

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
