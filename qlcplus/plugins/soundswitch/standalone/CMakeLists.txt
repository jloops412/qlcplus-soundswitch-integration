cmake_minimum_required(VERSION 3.22)

project(soundswitch_plugin_standalone LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(QT_VERSION_MAJOR 6)
set(QT_MAJOR_VERSION 6)
set(INSTALLROOT "")
set(PLUGINDIR "Plugins")
if(WIN32)
    # Match the pinned QLC+ root project: plug-ins are named soundswitch.dll,
    # not MinGW's default libsoundswitch.dll.
    set(CMAKE_SHARED_LIBRARY_PREFIX "")
endif()

# CI copies plugins/interfaces from the exact pinned QLC+ source commit next
# to this repository's focused plug-in snapshot before configuring this
# harness. The production QLC+ tree continues to use the plug-in's normal
# CMakeLists.txt through plugins/CMakeLists.txt.
if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../interfaces/qlcioplugin.h")
    message(FATAL_ERROR
        "Copy the pinned QLC+ plugins/interfaces directory to qlcplus/plugins/interfaces first")
endif()

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/.." soundswitch)
