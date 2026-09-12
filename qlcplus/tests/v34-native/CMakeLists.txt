cmake_minimum_required(VERSION 3.22)
project(v34_pinned_native_contract LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(QLCPLUS_SOURCE_DIR "" CACHE PATH "Clean checkout of the exact pinned QLC+ source")
set(QLCPLUS_PIN a124abebe0b5ad6077727c561a5a0e1f3730810c)
execute_process(COMMAND git -C "${QLCPLUS_SOURCE_DIR}" rev-parse HEAD
    OUTPUT_VARIABLE source_head OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
if(NOT source_head STREQUAL QLCPLUS_PIN)
    message(FATAL_ERROR "Expected pinned QLC+ ${QLCPLUS_PIN}, got ${source_head}")
endif()
execute_process(COMMAND git -C "${QLCPLUS_SOURCE_DIR}" diff --quiet HEAD -- engine qmlui/virtualconsole hotplugmonitor plugins/interfaces
    RESULT_VARIABLE source_dirty)
if(NOT source_dirty EQUAL 0)
    message(FATAL_ERROR "Pinned engine or VC sources contain tracked modifications")
endif()
find_package(Qt6 6.8.1 EXACT REQUIRED COMPONENTS Core Gui Multimedia Qml Quick Test Widgets)
set(QT_VERSION_MAJOR 6)
set(QT_MAJOR_VERSION 6)
set(qmlui ON)
set(udev ON)
set(INSTALL_ROOT "/")
set(FFTW3_FOUND FALSE)
include(GNUInstallDirs)
include("${QLCPLUS_SOURCE_DIR}/variables.cmake")
# The unmodified upstream engine CMake generates qlcconfig.h below this wrapper's
# source root. Add that generated directory; no upstream C++ is patched/copied.
add_subdirectory("${QLCPLUS_SOURCE_DIR}/hotplugmonitor/src" upstream-hotplug)
add_subdirectory("${QLCPLUS_SOURCE_DIR}/engine/audio/src" upstream-audio)
add_subdirectory("${QLCPLUS_SOURCE_DIR}/engine/src" upstream-engine)
target_include_directories(qlcplusengine PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/engine/src")

get_filename_component(INTEGRATION_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(PLUGIN_SOURCE "${INTEGRATION_ROOT}/qlcplus/plugins/soundswitch")
add_executable(v34_native_contract
    native_contract.cpp
    adapters/tardis.h
    "${QLCPLUS_SOURCE_DIR}/qmlui/virtualconsole/vcwidget.cpp"
    "${QLCPLUS_SOURCE_DIR}/qmlui/virtualconsole/vcwidget.h"
    "${QLCPLUS_SOURCE_DIR}/qmlui/virtualconsole/vcbutton.cpp"
    "${QLCPLUS_SOURCE_DIR}/qmlui/virtualconsole/vcbutton.h"
    "${PLUGIN_SOURCE}/soundswitchmidiinput.cpp"
    "${PLUGIN_SOURCE}/soundswitchmidiinput.h"
    "${PLUGIN_SOURCE}/soundswitchperformance.cpp"
    "${PLUGIN_SOURCE}/soundswitcheffects.cpp"
    "${PLUGIN_SOURCE}/soundswitchintensity.cpp")
target_compile_definitions(v34_native_contract PRIVATE
    SOUNDSWITCH_MIDI_TEST_BACKEND
    PINNED_QLC_SOURCE="${QLCPLUS_PIN}")
target_include_directories(v34_native_contract PRIVATE
    adapters "${QLCPLUS_SOURCE_DIR}/engine/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/src"
    "${QLCPLUS_SOURCE_DIR}/qmlui/virtualconsole"
    "${QLCPLUS_SOURCE_DIR}/qmlui"
    "${QLCPLUS_SOURCE_DIR}/plugins/interfaces"
    "${PLUGIN_SOURCE}")
target_link_libraries(v34_native_contract PRIVATE qlcplusengine Qt6::Quick Qt6::Test)
enable_testing()
add_test(NAME pinned-native-v34 COMMAND v34_native_contract)
set_tests_properties(pinned-native-v34 PROPERTIES
    TIMEOUT 90
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;V34_REPOSITORY=${INTEGRATION_ROOT};V34_UPSTREAM=${QLCPLUS_SOURCE_DIR}")
