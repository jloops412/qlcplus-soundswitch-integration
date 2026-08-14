cmake_minimum_required(VERSION 3.20)

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(dj_lighting_showcore LANGUAGES CXX)

include(GNUInstallDirs)

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

# Parse the frozen migration schemas during every configure so malformed JSON
# cannot reach a test or package build on either Linux or Windows.
foreach(EMBERLIGHTS_MIGRATION_SCHEMA IN ITEMS
        soundswitch-corpus-manifest-v1.schema.json
        soundswitch-migration-report-v1.schema.json)
    file(READ
        "${CMAKE_CURRENT_SOURCE_DIR}/../spec/migration/${EMBERLIGHTS_MIGRATION_SCHEMA}"
        EMBERLIGHTS_MIGRATION_SCHEMA_JSON)
    string(JSON EMBERLIGHTS_MIGRATION_SCHEMA_DIALECT
        ERROR_VARIABLE EMBERLIGHTS_MIGRATION_SCHEMA_ERROR
        GET "${EMBERLIGHTS_MIGRATION_SCHEMA_JSON}" "$schema")
    if(NOT EMBERLIGHTS_MIGRATION_SCHEMA_ERROR STREQUAL "NOTFOUND")
        message(FATAL_ERROR
            "Invalid migration schema ${EMBERLIGHTS_MIGRATION_SCHEMA}: "
            "${EMBERLIGHTS_MIGRATION_SCHEMA_ERROR}")
    endif()
endforeach()

add_library(showcore STATIC
    src/layer_resolver.cpp
    src/fixture.cpp
    src/fixture_capabilities.cpp
    src/fixture_control_action.cpp
    src/fixture_control_surface.cpp
    src/fixture_controller_binding.cpp
    src/fixture_function_component.cpp
    src/fixture_library.cpp
    src/fixture_parameter_catalog.cpp
    src/fixture_profile_editor.cpp
    src/fixture_profile_upgrade.cpp
    src/dmx_usb_pro.cpp
    src/soundswitch_micro.cpp
    src/soundswitch_control_one.cpp
    src/output_backend.cpp
    src/artnet.cpp
    src/sacn.cpp
    src/os2l.cpp
    src/os2l_server.cpp
    src/sync_manager.cpp
    src/midi.cpp
    src/winmm_midi.cpp
    src/look.cpp
    src/autoloop.cpp
    src/autoloop_source.cpp
    src/autoloop_persistence.cpp
    src/autoloop_program.cpp
    src/autoloop_authoring.cpp
    src/autoloop_fixture_controls.cpp
    src/autoloop_content_pack.cpp
    src/autoloop_autoscript.cpp
    src/autoloop_autoscript_studio.cpp
    src/autoloop_autoscript_workflow.cpp
    src/autoloop_palette_resolution.cpp
    src/autoloop_director.cpp
    src/autoloop_runtime.cpp
    src/engine.cpp
    src/project.cpp
    src/hardware_qualification.cpp
    src/raw_hardware_test.cpp
    src/raw_hardware_test_operator.cpp
    src/connection_layout.cpp
    src/connection_coordinator.cpp
    src/project_edit_history.cpp
    src/project_io.cpp
    src/file_identity.cpp
    src/ember_action_json.cpp
    src/ember_action_compiler.cpp
    src/ember_action_registry_adapter.cpp
    src/ember_action_executor.cpp
    src/audio_assets.cpp
    src/qlc_fixture_import.cpp
    src/ofl_fixture_catalog.cpp
    src/soundswitch_import.cpp
    src/soundswitch_source_binding.cpp
    src/soundswitch_v1.cpp
    src/soundswitch_migration_ir.cpp
    src/migration_portability_review.cpp
    src/studio_document.cpp
    src/static_look_authoring.cpp
    src/static_look_preview.cpp
    src/static_look_physical_preview.cpp
    src/studio_color.cpp
    src/studio_preview.cpp
    src/compiler.cpp
    src/ui_command.cpp
    src/ui_state.cpp
    src/ui_authoring.cpp
    src/ui_visual.cpp
    src/live_view_model.cpp
    src/runner.cpp
    src/runner_frame_inspector.cpp
    src/runner_raw_hardware_parity.cpp)

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
    target_link_libraries(showcore PUBLIC
        ws2_32 winmm setupapi winusb winhttp advapi32 dnsapi ole32 uuid)
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
if(MINGW)
    target_link_options(emberlights_migrate PRIVATE -municode)
endif()

add_executable(core_tests tests/test_main.cpp)
target_link_libraries(core_tests PRIVATE showcore)

add_executable(soundswitch_micro_session_tests
    tests/test_soundswitch_micro_session.cpp)
target_link_libraries(soundswitch_micro_session_tests PRIVATE showcore)

add_executable(raw_hardware_test_tests
    tests/test_raw_hardware_test.cpp)
target_link_libraries(raw_hardware_test_tests PRIVATE showcore)

add_executable(raw_hardware_test_operator_tests
    tests/test_raw_hardware_test_operator.cpp)
target_link_libraries(raw_hardware_test_operator_tests PRIVATE showcore)

add_executable(connection_coordinator_tests
    tests/test_connection_coordinator.cpp)
target_link_libraries(connection_coordinator_tests PRIVATE showcore)

add_executable(connection_layout_tests
    tests/test_connection_layout.cpp)
target_link_libraries(connection_layout_tests PRIVATE showcore)

add_executable(soundswitch_control_one_tests
    tests/test_soundswitch_control_one.cpp)
target_link_libraries(soundswitch_control_one_tests PRIVATE showcore)

add_executable(ui_command_state_tests
    tests/test_ui_command_state.cpp)
target_link_libraries(ui_command_state_tests PRIVATE showcore)

add_executable(live_ui_tests
    tests/test_live_ui.cpp)
target_link_libraries(live_ui_tests PRIVATE showcore)

add_executable(runner_frame_inspector_tests
    tests/test_runner_frame_inspector.cpp)
target_link_libraries(runner_frame_inspector_tests PRIVATE showcore)

add_executable(runner_raw_hardware_parity_tests
    tests/test_runner_raw_hardware_parity.cpp)
target_link_libraries(runner_raw_hardware_parity_tests PRIVATE showcore)

add_executable(ui_visual_tests
    tests/test_ui_visual.cpp)
target_link_libraries(ui_visual_tests PRIVATE showcore)

add_executable(ui_authoring_tests
    tests/test_ui_authoring.cpp)
target_link_libraries(ui_authoring_tests PRIVATE showcore)

add_executable(studio_foundation_tests
    tests/test_studio_foundation.cpp)
target_link_libraries(studio_foundation_tests PRIVATE showcore)

add_executable(static_look_authoring_tests
    tests/test_static_look_authoring.cpp)
target_link_libraries(static_look_authoring_tests PRIVATE showcore)

add_executable(static_look_physical_preview_tests
    tests/test_static_look_physical_preview.cpp)
target_link_libraries(static_look_physical_preview_tests PRIVATE showcore)

add_executable(ofl_fixture_catalog_tests
    tests/test_ofl_fixture_catalog.cpp)
target_link_libraries(ofl_fixture_catalog_tests PRIVATE showcore)

add_executable(fixture_profile_editor_tests
    tests/test_fixture_profile_editor.cpp)
target_link_libraries(fixture_profile_editor_tests PRIVATE showcore)

add_executable(fixture_controller_binding_tests
    tests/test_fixture_controller_binding.cpp)
target_link_libraries(fixture_controller_binding_tests PRIVATE showcore)

add_executable(fixture_function_component_tests
    tests/test_fixture_function_component.cpp)
target_link_libraries(fixture_function_component_tests PRIVATE showcore)

add_executable(fixture_control_surface_tests
    tests/test_fixture_control_surface.cpp)
target_link_libraries(fixture_control_surface_tests PRIVATE showcore)

add_executable(studio_authoring_tests
    tests/test_studio_authoring.cpp)
target_link_libraries(studio_authoring_tests PRIVATE showcore)

add_executable(ember_action_foundation_tests
    tests/test_ember_action_foundation.cpp)
target_link_libraries(ember_action_foundation_tests PRIVATE showcore)

add_executable(ember_action_executor_tests
    tests/test_ember_action_executor.cpp)
target_link_libraries(ember_action_executor_tests PRIVATE showcore)

add_executable(fixture_control_action_tests
    tests/test_fixture_control_action.cpp)
target_link_libraries(fixture_control_action_tests PRIVATE showcore)

add_executable(autoloop_v2_model_tests
    tests/test_autoloop_v2_model.cpp)
target_link_libraries(autoloop_v2_model_tests PRIVATE showcore)

add_executable(autoloop_v2_persistence_tests
    tests/test_autoloop_v2_persistence.cpp)
target_link_libraries(autoloop_v2_persistence_tests PRIVATE showcore)

add_executable(autoloop_v2_studio_tests
    tests/test_autoloop_v2_studio.cpp)
target_link_libraries(autoloop_v2_studio_tests PRIVATE showcore)

add_executable(autoloop_fixture_controls_tests
    tests/test_autoloop_fixture_controls.cpp)
target_link_libraries(autoloop_fixture_controls_tests PRIVATE showcore)

add_executable(autoloop_autoscript_tests
    tests/test_autoloop_autoscript.cpp)
target_link_libraries(autoloop_autoscript_tests PRIVATE showcore)

add_executable(autoloop_autoscript_studio_tests
    tests/test_autoloop_autoscript_studio.cpp)
target_link_libraries(autoloop_autoscript_studio_tests PRIVATE showcore)

add_executable(autoloop_autoscript_workflow_tests
    tests/test_autoloop_autoscript_workflow.cpp)
target_link_libraries(autoloop_autoscript_workflow_tests PRIVATE showcore)

add_executable(autoloop_v2_runtime_tests
    tests/test_autoloop_v2_runtime.cpp)
target_link_libraries(autoloop_v2_runtime_tests PRIVATE showcore)

add_executable(autoloop_v2_preview_tests
    tests/test_autoloop_v2_preview.cpp)
target_link_libraries(autoloop_v2_preview_tests PRIVATE showcore)

add_executable(autoloop_v2_palette_resolution_tests
    tests/test_autoloop_v2_palette_resolution.cpp)
target_link_libraries(autoloop_v2_palette_resolution_tests PRIVATE showcore)

add_executable(autoloop_v2_runner_tests
    tests/test_autoloop_v2_runner.cpp)
target_link_libraries(autoloop_v2_runner_tests PRIVATE showcore)

add_executable(migration_portability_review_tests
    tests/test_migration_portability_review.cpp)
target_link_libraries(migration_portability_review_tests PRIVATE showcore)

add_executable(core_bench bench/bench_main.cpp)
target_link_libraries(core_bench PRIVATE showcore)

add_executable(performance_bench bench/performance_bench.cpp)
target_link_libraries(performance_bench PRIVATE showcore)

if(WIN32)
    set(EMBERLIGHTS_V1_TEMPLATE
        "${CMAKE_CURRENT_BINARY_DIR}/EmberLights-2026-V1-Template.emberlights")
    set(EMBERLIGHTS_PREBUILT_V1_TEMPLATE "" CACHE FILEPATH
        "Existing V1 template for a cross-build that cannot run Windows tools")
    if(EMBERLIGHTS_PREBUILT_V1_TEMPLATE)
        add_custom_command(
            OUTPUT "${EMBERLIGHTS_V1_TEMPLATE}"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${EMBERLIGHTS_PREBUILT_V1_TEMPLATE}"
                "${EMBERLIGHTS_V1_TEMPLATE}"
            DEPENDS "${EMBERLIGHTS_PREBUILT_V1_TEMPLATE}"
            COMMENT "Staging the prebuilt output-disabled V1 starter project"
            VERBATIM)
    else()
        add_custom_command(
            OUTPUT "${EMBERLIGHTS_V1_TEMPLATE}"
            COMMAND $<TARGET_FILE:emberlights_migrate>
                template-v1 "${EMBERLIGHTS_V1_TEMPLATE}" --force
            DEPENDS emberlights_migrate
            COMMENT "Generating the output-disabled V1 starter project"
            VERBATIM)
    endif()
    add_custom_target(emberlights_v1_template ALL
        DEPENDS "${EMBERLIGHTS_V1_TEMPLATE}")

    option(EMBERLIGHTS_SKIP_WINDOWS_RESOURCE
        "Skip the Windows resource file for local cross-builds" OFF)
    set(EMBERLIGHTS_WINDOWS_APP_SOURCES src/windows_app.cpp)
    if(NOT EMBERLIGHTS_SKIP_WINDOWS_RESOURCE)
        list(APPEND EMBERLIGHTS_WINDOWS_APP_SOURCES windows/EmberLights.rc)
    endif()
    add_executable(EmberLights WIN32 ${EMBERLIGHTS_WINDOWS_APP_SOURCES})
    target_link_libraries(EmberLights PRIVATE
        showcore
        comctl32
        comdlg32
        shell32
        advapi32
        dwmapi
        ole32
        uuid)
    if(MSVC)
        # The executable already embeds our explicit DPI/compatibility manifest
        # through EmberLights.rc; suppress the linker's second default manifest.
        target_link_options(EmberLights PRIVATE /MANIFEST:NO)
    elseif(MINGW)
        target_link_options(EmberLights PRIVATE -municode -Wl,--subsystem,windows)
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
    elseif(MINGW)
        target_link_options(soundswitch_micro_probe PRIVATE -municode)
    endif()

    add_executable(soundswitch_control_one_probe
        src/soundswitch_control_one_probe.cpp)
    target_link_libraries(soundswitch_control_one_probe PRIVATE
        showcore
        setupapi
        winusb
        advapi32
        ole32
        uuid)
    target_compile_definitions(soundswitch_control_one_probe PRIVATE
        UNICODE
        _UNICODE
        NOMINMAX
        _WIN32_WINNT=0x0A00
        EMBERLIGHTS_VERSION="${EMBERLIGHTS_VERSION}")
    if(MSVC)
        target_compile_options(soundswitch_control_one_probe PRIVATE
            /W4 /WX /permissive-)
    endif()

    install(TARGETS EmberLights RUNTIME DESTINATION .)
    install(TARGETS midi_capture emberlights_qualify emberlights_migrate soundswitch_micro_probe
        soundswitch_control_one_probe
        RUNTIME DESTINATION Tools)
    install(FILES "${EMBERLIGHTS_V1_TEMPLATE}" DESTINATION Templates)
    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/../installer/assets/EmberLights-IR4-6CH-Editable-Bench.emberlights"
        DESTINATION Templates)
    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/../README.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/../THIRD_PARTY_NOTICES.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/../docs/15_WINDOWS_V1_TESTING.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/../docs/16_QLC_FIXTURE_IMPORT.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/../docs/17_PRODUCTION_RELEASE_GATE.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/../docs/18_SOUNDSWITCH_MIGRATION.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/../docs/20_CONTROL_ONE_DMX_QUALIFICATION.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/../docs/31_LIMITED_BETA_OS2L_AND_INSTALLER_TEST.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/../docs/32_FIXTURE_TRUTH_AND_STATIC_LOOK_BUILDER_CHECKPOINT.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/../docs/33_AUTOSCRIPT_STUDIO_E2E_TEST.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/../docs/MORNING_HARDWARE_TEST.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/../docs/IR4_6CH_RUNNER_FRAME_TEST.md"
        DESTINATION docs)
else()
    # Linux currently exposes the deterministic Runner and authoring/diagnostic
    # command-line tools. Keep the package layout distribution-native so the
    # DEB can be installed, smoke-tested, upgraded, and removed normally.
    install(TARGETS
        runner_lab
        midi_capture
        emberlights_qualify
        emberlights_migrate
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/../README.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/../THIRD_PARTY_NOTICES.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/../docs/17_PRODUCTION_RELEASE_GATE.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/../docs/19_LINUX_INSTALLATION.md"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/emberlights")

    set(CPACK_PACKAGE_NAME "emberlights")
    set(CPACK_PACKAGE_VENDOR "EmberLights")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
        "Deterministic SoundSwitch-first DJ lighting Runner and tools")
    set(CPACK_PACKAGE_VERSION "${EMBERLIGHTS_VERSION}")
    set(CPACK_PACKAGE_CONTACT "EmberLights")
    set(CPACK_PACKAGE_FILE_NAME
        "EmberLights-${EMBERLIGHTS_VERSION}-Linux-${CMAKE_SYSTEM_PROCESSOR}")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
    set(CPACK_GENERATOR "DEB;TGZ")
    set(CPACK_DEBIAN_PACKAGE_NAME "emberlights")
    set(CPACK_DEBIAN_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}.deb")
    set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    include(CPack)
endif()

enable_testing()
find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME ui_direction_tests
        COMMAND "${Python3_EXECUTABLE}" tests/test_ui_direction.py)
    set_tests_properties(ui_direction_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
if(WIN32)
    add_test(NAME windows_gui_startup COMMAND EmberLights --startup-smoke)
    set_tests_properties(windows_gui_startup PROPERTIES TIMEOUT 20)
    add_test(NAME soundswitch_micro_probe_self_test
        COMMAND soundswitch_micro_probe --self-test)
endif()
add_test(NAME core_tests COMMAND core_tests)
add_test(NAME soundswitch_micro_session_tests COMMAND soundswitch_micro_session_tests)
add_test(NAME raw_hardware_test_tests COMMAND raw_hardware_test_tests)
add_test(NAME raw_hardware_test_operator_tests COMMAND raw_hardware_test_operator_tests)
add_test(NAME connection_coordinator_tests COMMAND connection_coordinator_tests)
add_test(NAME connection_layout_tests COMMAND connection_layout_tests)
add_test(NAME soundswitch_control_one_tests COMMAND soundswitch_control_one_tests)
add_test(NAME ui_command_state_tests COMMAND ui_command_state_tests)
add_test(NAME live_ui_tests COMMAND live_ui_tests)
add_test(NAME runner_frame_inspector_tests COMMAND runner_frame_inspector_tests)
add_test(NAME runner_raw_hardware_parity_tests
    COMMAND runner_raw_hardware_parity_tests)
add_test(NAME ui_visual_tests COMMAND ui_visual_tests)
add_test(NAME ui_authoring_tests COMMAND ui_authoring_tests)
add_test(NAME studio_foundation_tests COMMAND studio_foundation_tests)
set_tests_properties(studio_foundation_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME static_look_authoring_tests COMMAND static_look_authoring_tests)
add_test(NAME static_look_physical_preview_tests
    COMMAND static_look_physical_preview_tests)
add_test(NAME ofl_fixture_catalog_tests COMMAND ofl_fixture_catalog_tests)
add_test(NAME fixture_profile_editor_tests COMMAND fixture_profile_editor_tests)
add_test(NAME fixture_controller_binding_tests
    COMMAND fixture_controller_binding_tests)
add_test(NAME fixture_function_component_tests
    COMMAND fixture_function_component_tests)
add_test(NAME fixture_control_surface_tests COMMAND fixture_control_surface_tests)
add_test(NAME studio_authoring_tests COMMAND studio_authoring_tests)
add_test(NAME ember_action_foundation_tests COMMAND ember_action_foundation_tests)
set_tests_properties(ember_action_foundation_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME ember_action_executor_tests COMMAND ember_action_executor_tests)
set_tests_properties(ember_action_executor_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME fixture_control_action_tests COMMAND fixture_control_action_tests)
add_test(NAME autoloop_v2_model_tests COMMAND autoloop_v2_model_tests)
add_test(NAME autoloop_v2_persistence_tests COMMAND autoloop_v2_persistence_tests)
add_test(NAME autoloop_v2_studio_tests COMMAND autoloop_v2_studio_tests)
add_test(NAME autoloop_fixture_controls_tests
    COMMAND autoloop_fixture_controls_tests)
add_test(NAME autoloop_autoscript_tests COMMAND autoloop_autoscript_tests)
add_test(NAME autoloop_autoscript_studio_tests
    COMMAND autoloop_autoscript_studio_tests)
add_test(NAME autoloop_autoscript_workflow_tests
    COMMAND autoloop_autoscript_workflow_tests)
add_test(NAME autoloop_v2_runtime_tests COMMAND autoloop_v2_runtime_tests)
add_test(NAME autoloop_v2_preview_tests COMMAND autoloop_v2_preview_tests)
add_test(NAME autoloop_v2_palette_resolution_tests
    COMMAND autoloop_v2_palette_resolution_tests)
add_test(NAME autoloop_v2_runner_tests COMMAND autoloop_v2_runner_tests)
add_test(NAME migration_portability_review_tests
    COMMAND migration_portability_review_tests)
add_test(NAME midi_capture_list COMMAND midi_capture --list)
add_test(
    NAME runner_lab_smoke
    COMMAND runner_lab --os2l-port 0 --duration 1 --manual-bpm 120)
add_test(
    NAME qualification_smoke
    COMMAND emberlights_qualify --duration 2 --network-loopback --report qualification-smoke.json)
set_tests_properties(qualification_smoke PROPERTIES TIMEOUT 20)
add_test(NAME migration_help COMMAND emberlights_migrate --help)
