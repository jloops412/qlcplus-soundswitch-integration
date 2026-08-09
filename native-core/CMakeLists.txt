cmake_minimum_required(VERSION 3.20)

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(dj_lighting_showcore LANGUAGES CXX)

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
    src/project_io.cpp
    src/qlc_fixture_import.cpp
    src/compiler.cpp
    src/runner.cpp)

target_include_directories(showcore PUBLIC include)
target_link_libraries(showcore PUBLIC Threads::Threads)

if(MSVC)
    # C4324 reports the deliberate cache-line padding in the lock-free SPSC
    # queues. Keep every other level-4 warning fatal.
    target_compile_options(showcore PUBLIC /W4 /WX /permissive- /wd4324)
else()
    target_compile_options(showcore PUBLIC -Wall -Wextra -Wpedantic -Werror)
endif()

if(WIN32)
    target_compile_definitions(showcore PUBLIC NOMINMAX _WIN32_WINNT=0x0A00)
    target_link_libraries(showcore PUBLIC ws2_32 winmm)
endif()

add_executable(runner_demo src/demo.cpp)
target_link_libraries(runner_demo PRIVATE showcore)

add_executable(os2l_capture src/os2l_capture.cpp)
target_link_libraries(os2l_capture PRIVATE showcore)

add_executable(midi_capture src/midi_capture.cpp)
target_link_libraries(midi_capture PRIVATE showcore Threads::Threads)

add_executable(runner_lab src/runner_lab.cpp)
target_link_libraries(runner_lab PRIVATE showcore Threads::Threads)

add_executable(core_tests tests/test_main.cpp)
target_link_libraries(core_tests PRIVATE showcore)

add_executable(core_bench bench/bench_main.cpp)
target_link_libraries(core_bench PRIVATE showcore)

add_executable(performance_bench bench/performance_bench.cpp)
target_link_libraries(performance_bench PRIVATE showcore)

if(WIN32)
    add_executable(EmberLights WIN32
        src/windows_app.cpp
        windows/EmberLights.rc)
    target_link_libraries(EmberLights PRIVATE
        showcore
        comctl32
        comdlg32
        shell32)
    if(MSVC)
        # The executable already embeds our explicit DPI/compatibility manifest
        # through EmberLights.rc; suppress the linker's second default manifest.
        target_link_options(EmberLights PRIVATE /MANIFEST:NO)
    endif()
    target_compile_definitions(EmberLights PRIVATE UNICODE _UNICODE)
    set_target_properties(EmberLights PROPERTIES OUTPUT_NAME "EmberLights")

    install(TARGETS EmberLights midi_capture RUNTIME DESTINATION .)
    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/../README.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/../THIRD_PARTY_NOTICES.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/../docs/15_WINDOWS_V1_TESTING.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/../docs/16_QLC_FIXTURE_IMPORT.md"
        DESTINATION docs)
endif()

enable_testing()
add_test(NAME core_tests COMMAND core_tests)
add_test(NAME midi_capture_list COMMAND midi_capture --list)
add_test(
    NAME runner_lab_smoke
    COMMAND runner_lab --os2l-port 0 --duration 1 --manual-bpm 120)
