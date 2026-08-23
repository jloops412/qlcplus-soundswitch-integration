set(module_name "soundswitch")

project(${module_name})

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

add_library(${module_name} SHARED)

target_sources(${module_name} PRIVATE
    ../interfaces/qlcioplugin.cpp ../interfaces/qlcioplugin.h
    soundswitchdevice.cpp soundswitchdevice.h
    soundswitchmidiinput.cpp soundswitchmidiinput.h
    soundswitchplugin.cpp soundswitchplugin.h
    soundswitchprotocol.cpp soundswitchprotocol.h
)

target_include_directories(${module_name} PRIVATE
    ../interfaces
)

target_compile_features(${module_name} PRIVATE cxx_std_17)

target_link_libraries(${module_name} PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    setupapi
    winusb
    ole32
    winmm
)

install(TARGETS ${module_name}
    LIBRARY DESTINATION ${INSTALLROOT}/${PLUGINDIR}
    RUNTIME DESTINATION ${INSTALLROOT}/${PLUGINDIR}
)

add_executable(soundswitch_protocol_tests EXCLUDE_FROM_ALL
    soundswitchprotocol.cpp soundswitchprotocol.h
    tests/soundswitchprotocoltest.cpp
)
target_compile_features(soundswitch_protocol_tests PRIVATE cxx_std_17)

add_executable(ss_smoke EXCLUDE_FROM_ALL
    ../interfaces/qlcioplugin.cpp
    ../interfaces/qlcioplugin.h
    tests/soundswitchpluginsmoketest.cpp
)
set_target_properties(ss_smoke PROPERTIES OUTPUT_NAME soundswitch_plugin_smoke_tests)
target_include_directories(ss_smoke PRIVATE
    ../interfaces
)
target_link_libraries(ss_smoke PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC 16 diagnoses a Qt 6.8 header SFINAE pattern that predates this
    # project. Keep project warnings fatal without turning that external
    # compatibility warning into a smoke-test build failure.
    target_compile_options(ss_smoke PRIVATE
        -Wno-error=sfinae-incomplete
    )
endif()
