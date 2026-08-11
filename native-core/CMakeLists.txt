cmake_minimum_required(VERSION 3.20)

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(dj_lighting_showcore LANGUAGES CXX)

set(EMBERLIGHTS_VERSION "0.1.0-dev" CACHE STRING "EmberLights product version")
set(EMBERLIGHTS_COMMIT "unknown" CACHE STRING "EmberLights source commit")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Keep the Windows testing builds self-contained. The installer must not depend
# on the user having a matching Visual C++ redistributable already installed.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

find_package(Threads REQUIRED)

add_library(showcore STATIC
    src/layer_resolver.cpp
    src/fixture.cpp
    src/fixture_library.cpp
    src/dmx_usb_pro.cpp
    src/soundswitch_micro.cpp
    src/artnet.cpp
    src/sacn.cpp
    src/os2l.cpp
    src/os2l_server.cpp
    src/sync_manager.cpp
    src/midi.cpp
    src/winmm_midi.cpp
    src/look.cpp
    src/autoloop.cpp
    src/engine.cpp
    src/project.cpp
    src/project_edit_history.cpp
    src/project_io.cpp
    src/file_identity.cpp
    src/audio_assets.cpp
    src/qlc_fixture_import.cpp
    src/soundswitch_import.cpp
    src/soundswitch_v1.cpp
    src/compiler.cpp
    src/runner.cpp)

target_include_directories(showcore PUBLIC include)
target_link_libraries(showcore PUBLIC Threads::Threads)
target_compile_definitions(showcore PUBLIC
    EMBERLIGHTS_VERSION="${EMBERLIGHTS_VERSION}"
    EMBERLIGHTS_COMMIT="${EMBERLIGHTS_COMMIT}")

if(MSVC)
    # C4324 reports the deliberate cache-line padding in the lock-free SPSC
    # queues. Keep every other level-4 warning fatal.
    target_compile_options(showcore PUBLIC /W4 /WX /permissive- /wd4324)
else()
    target_compile_options(showcore PUBLIC -Wall -Wextra -Wpedantic -Werror)
endif()

if(WIN32)
    target_compile_definitions(showcore PUBLIC NOMINMAX _WIN32_WINNT=0x0A00)
    target_link_libraries(showcore PUBLIC ws2_32 winmm setupapi winusb ole32 uuid)
endif()

add_executable(runner_demo src/demo.cpp)
target_link_libraries(runner_demo PRIVATE showcore)

add_executable(os2l_capture src/os2l_capture.cpp)
target_link_libraries(os2l_capture PRIVATE showcore)

add_executable(midi_capture src/midi_capture.cpp)
target_link_libraries(midi_capture PRIVATE showcore Threads::Threads)

add_executable(runner_lab src/runner_lab.cpp)
target_link_libraries(runner_lab PRIVATE showcore Threads::Threads)

add_executable(emberlights_qualify src/qualification.cpp)
target_link_libraries(emberlights_qualify PRIVATE showcore Threads::Threads)
if(WIN32)
    target_link_libraries(emberlights_qualify PRIVATE psapi)
endif()

add_executable(emberlights_migrate src/soundswitch_migrate.cpp)
target_link_libraries(emberlights_migrate PRIVATE showcore)

add_executable(core_tests tests/test_main.cpp)
target_link_libraries(core_tests PRIVATE showcore)

add_executable(soundswitch_micro_session_tests
    tests/test_soundswitch_micro_session.cpp)
target_link_libraries(soundswitch_micro_session_tests PRIVATE showcore)

add_executable(core_bench bench/bench_main.cpp)
target_link_libraries(core_bench PRIVATE showcore)

add_executable(performance_bench bench/performance_bench.cpp)
target_link_libraries(performance_bench PRIVATE showcore)

if(WIN32)
    set(EMBERLIGHTS_V1_TEMPLATE
        "${CMAKE_CURRENT_BINARY_DIR}/EmberLights-2026-V1-Template.emberlights")
    add_custom_command(
        OUTPUT "${EMBERLIGHTS_V1_TEMPLATE}"
        COMMAND $<TARGET_FILE:emberlights_migrate>
            template-v1 "${EMBERLIGHTS_V1_TEMPLATE}" --force
        DEPENDS emberlights_migrate
        COMMENT "Generating the output-disabled V1 starter project"
        VERBATIM)
    add_custom_target(emberlights_v1_template ALL
        DEPENDS "${EMBERLIGHTS_V1_TEMPLATE}")

    add_executable(EmberLights WIN32
        src/windows_app.cpp
        windows/EmberLights.rc)
    target_link_libraries(EmberLights PRIVATE
        showcore
        comctl32
        comdlg32
        shell32
        advapi32
        ole32
        uuid)
    if(MSVC)
        # The executable already embeds our explicit DPI/compatibility manifest
        # through EmberLights.rc; suppress the linker's second default manifest.
        target_link_options(EmberLights PRIVATE /MANIFEST:NO)
    endif()
    target_compile_definitions(EmberLights PRIVATE UNICODE _UNICODE)
    set_target_properties(EmberLights PROPERTIES OUTPUT_NAME "EmberLights")

    add_executable(soundswitch_micro_probe
        src/soundswitch_micro_probe.cpp)
    target_link_libraries(soundswitch_micro_probe PRIVATE
        showcore
        setupapi
        winusb
        shell32
        ole32
        uuid)
    target_compile_definitions(soundswitch_micro_probe PRIVATE
        UNICODE
        _UNICODE
        NOMINMAX
        _WIN32_WINNT=0x0A00
        EMBERLIGHTS_VERSION="${EMBERLIGHTS_VERSION}")
    if(MSVC)
        target_compile_options(soundswitch_micro_probe PRIVATE /W4 /WX /permissive-)
    endif()

    install(TARGETS EmberLights midi_capture RUNTIME DESTINATION .)
    install(TARGETS emberlights_qualify emberlights_migrate soundswitch_micro_probe
        RUNTIME DESTINATION Tools)
    install(FILES "${EMBERLIGHTS_V1_TEMPLATE}" DESTINATION Templates)
    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/../README.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/../THIRD_PARTY_NOTICES.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/../docs/15_WINDOWS_V1_TESTING.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/../docs/16_QLC_FIXTURE_IMPORT.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/../docs/17_PRODUCTION_RELEASE_GATE.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/../docs/18_SOUNDSWITCH_MIGRATION.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/../docs/MORNING_HARDWARE_TEST.md"
        DESTINATION docs)
endif()

enable_testing()
if(WIN32)
    add_test(NAME windows_gui_startup COMMAND EmberLights --startup-smoke)
    set_tests_properties(windows_gui_startup PROPERTIES TIMEOUT 20)
    add_test(NAME soundswitch_micro_probe_self_test
        COMMAND soundswitch_micro_probe --self-test)
endif()
add_test(NAME core_tests COMMAND core_tests)
add_test(NAME soundswitch_micro_session_tests COMMAND soundswitch_micro_session_tests)
add_test(NAME midi_capture_list COMMAND midi_capture --list)
add_test(
    NAME runner_lab_smoke
    COMMAND runner_lab --os2l-port 0 --duration 1 --manual-bpm 120)
add_test(
    NAME qualification_smoke
    COMMAND emberlights_qualify --duration 2 --network-loopback --report qualification-smoke.json)
set_tests_properties(qualification_smoke PROPERTIES TIMEOUT 20)
add_test(NAME migration_help COMMAND emberlights_migrate --help)
