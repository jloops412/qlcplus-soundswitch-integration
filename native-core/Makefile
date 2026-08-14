CXX ?= g++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror -MMD -MP -Iinclude
LDFLAGS ?=

ifeq ($(OS),Windows_NT)
THREAD_FLAGS :=
else
THREAD_FLAGS := -pthread
endif

LDFLAGS += $(THREAD_FLAGS)

ifeq ($(OS),Windows_NT)
LDFLAGS += -lws2_32 -lwinmm -lsetupapi -lwinusb -lwinhttp -ladvapi32 -ldnsapi -lole32 -luuid
endif

BUILD := build
SOURCES := \
	src/layer_resolver.cpp \
	src/fixture.cpp \
	src/fixture_capabilities.cpp \
	src/fixture_control_action.cpp \
	src/fixture_controller_binding.cpp \
	src/fixture_function_component.cpp \
	src/fixture_library.cpp \
	src/fixture_parameter_catalog.cpp \
	src/fixture_profile_editor.cpp \
	src/fixture_profile_upgrade.cpp \
	src/dmx_usb_pro.cpp \
	src/soundswitch_micro.cpp \
	src/soundswitch_control_one.cpp \
	src/output_backend.cpp \
	src/artnet.cpp \
	src/sacn.cpp \
	src/os2l.cpp \
	src/os2l_server.cpp \
	src/sync_manager.cpp \
	src/midi.cpp \
	src/winmm_midi.cpp \
	src/look.cpp \
	src/autoloop.cpp \
	src/autoloop_source.cpp \
	src/autoloop_persistence.cpp \
	src/autoloop_program.cpp \
	src/autoloop_authoring.cpp \
	src/autoloop_fixture_controls.cpp \
	src/autoloop_content_pack.cpp \
	src/autoloop_autoscript.cpp \
	src/autoloop_autoscript_studio.cpp \
	src/autoloop_palette_resolution.cpp \
	src/autoloop_director.cpp \
	src/autoloop_runtime.cpp \
	src/engine.cpp \
	src/project.cpp \
	src/hardware_qualification.cpp \
	src/raw_hardware_test.cpp \
	src/raw_hardware_test_operator.cpp \
	src/connection_layout.cpp \
	src/connection_coordinator.cpp \
	src/project_edit_history.cpp \
	src/project_io.cpp \
	src/file_identity.cpp \
	src/ember_action_json.cpp \
	src/ember_action_compiler.cpp \
	src/ember_action_registry_adapter.cpp \
	src/ember_action_executor.cpp \
	src/audio_assets.cpp \
	src/qlc_fixture_import.cpp \
	src/ofl_fixture_catalog.cpp \
	src/soundswitch_import.cpp \
	src/soundswitch_source_binding.cpp \
	src/soundswitch_v1.cpp \
	src/soundswitch_migration_ir.cpp \
	src/migration_portability_review.cpp \
	src/studio_document.cpp \
	src/static_look_authoring.cpp \
	src/static_look_preview.cpp \
	src/static_look_physical_preview.cpp \
	src/studio_color.cpp \
	src/studio_preview.cpp \
	src/compiler.cpp \
	src/ui_command.cpp \
	src/ui_state.cpp \
	src/ui_authoring.cpp \
	src/ui_visual.cpp \
	src/live_view_model.cpp \
	src/runner.cpp \
	src/runner_frame_inspector.cpp \
	src/runner_raw_hardware_parity.cpp
OBJECTS := $(SOURCES:%.cpp=$(BUILD)/%.o)
DEPS := \
	$(OBJECTS:.o=.d) \
	$(BUILD)/src/demo.d \
	$(BUILD)/src/os2l_capture.d \
	$(BUILD)/src/midi_capture.d \
	$(BUILD)/src/runner_lab.d \
	$(BUILD)/src/qualification.d \
	$(BUILD)/src/soundswitch_migrate.d \
	$(BUILD)/tests/test_main.d \
	$(BUILD)/tests/test_soundswitch_micro_session.d \
	$(BUILD)/tests/test_raw_hardware_test.d \
	$(BUILD)/tests/test_raw_hardware_test_operator.d \
	$(BUILD)/tests/test_connection_layout.d \
	$(BUILD)/tests/test_connection_coordinator.d \
	$(BUILD)/tests/test_soundswitch_control_one.d \
	$(BUILD)/tests/test_ui_command_state.d \
	$(BUILD)/tests/test_live_ui.d \
	$(BUILD)/tests/test_runner_frame_inspector.d \
	$(BUILD)/tests/test_runner_raw_hardware_parity.d \
	$(BUILD)/tests/test_ui_visual.d \
	$(BUILD)/tests/test_ui_authoring.d \
	$(BUILD)/tests/test_studio_foundation.d \
	$(BUILD)/tests/test_static_look_authoring.d \
	$(BUILD)/tests/test_static_look_physical_preview.d \
	$(BUILD)/tests/test_ofl_fixture_catalog.d \
	$(BUILD)/tests/test_fixture_profile_editor.d \
	$(BUILD)/tests/test_fixture_controller_binding.d \
	$(BUILD)/tests/test_fixture_function_component.d \
	$(BUILD)/tests/test_studio_authoring.d \
	$(BUILD)/src/ember_action_json.d \
	$(BUILD)/src/ember_action_compiler.d \
	$(BUILD)/src/ember_action_registry_adapter.d \
	$(BUILD)/src/ember_action_executor.d \
	$(BUILD)/tests/test_ember_action_foundation.d \
	$(BUILD)/tests/test_ember_action_executor.d \
	$(BUILD)/tests/test_fixture_control_action.d \
	$(BUILD)/tests/test_autoloop_v2_model.d \
	$(BUILD)/tests/test_autoloop_v2_persistence.d \
	$(BUILD)/tests/test_autoloop_v2_studio.d \
	$(BUILD)/tests/test_autoloop_fixture_controls.d \
	$(BUILD)/tests/test_autoloop_autoscript.d \
	$(BUILD)/tests/test_autoloop_autoscript_studio.d \
	$(BUILD)/tests/test_autoloop_v2_runtime.d \
	$(BUILD)/tests/test_autoloop_v2_preview.d \
	$(BUILD)/tests/test_autoloop_v2_palette_resolution.d \
	$(BUILD)/tests/test_autoloop_v2_runner.d \
	$(BUILD)/tests/test_migration_portability_review.d \
	$(BUILD)/bench/bench_main.d \
	$(BUILD)/bench/performance_bench.d

.PHONY: all runner_lab qualification migration test ember-action-test ember-action-registry-generate ember-action-registry-check winmm-syntax dmx-usb-syntax bench smoke clean ui-registry-generate ui-registry-check surface-contract-gate

all: $(BUILD)/runner_demo $(BUILD)/os2l_capture $(BUILD)/midi_capture $(BUILD)/runner_lab $(BUILD)/emberlights_qualify $(BUILD)/emberlights_migrate $(BUILD)/core_tests $(BUILD)/soundswitch_micro_session_tests $(BUILD)/raw_hardware_test_tests $(BUILD)/raw_hardware_test_operator_tests $(BUILD)/connection_layout_tests $(BUILD)/connection_coordinator_tests $(BUILD)/soundswitch_control_one_tests $(BUILD)/ui_command_state_tests $(BUILD)/live_ui_tests $(BUILD)/runner_frame_inspector_tests $(BUILD)/runner_raw_hardware_parity_tests $(BUILD)/ui_visual_tests $(BUILD)/ui_authoring_tests $(BUILD)/studio_foundation_tests $(BUILD)/static_look_authoring_tests $(BUILD)/static_look_physical_preview_tests $(BUILD)/ofl_fixture_catalog_tests $(BUILD)/fixture_profile_editor_tests $(BUILD)/fixture_controller_binding_tests $(BUILD)/fixture_function_component_tests $(BUILD)/studio_authoring_tests $(BUILD)/ember_action_foundation_tests $(BUILD)/ember_action_executor_tests $(BUILD)/fixture_control_action_tests $(BUILD)/autoloop_v2_model_tests $(BUILD)/autoloop_v2_persistence_tests $(BUILD)/autoloop_v2_studio_tests $(BUILD)/autoloop_fixture_controls_tests $(BUILD)/autoloop_autoscript_tests $(BUILD)/autoloop_autoscript_studio_tests $(BUILD)/autoloop_v2_runtime_tests $(BUILD)/autoloop_v2_preview_tests $(BUILD)/autoloop_v2_palette_resolution_tests $(BUILD)/autoloop_v2_runner_tests $(BUILD)/migration_portability_review_tests $(BUILD)/core_bench $(BUILD)/performance_bench

runner_lab: $(BUILD)/runner_lab

qualification: $(BUILD)/emberlights_qualify

migration: $(BUILD)/emberlights_migrate

ui-registry-generate:
	python3 tools/generate_ui_registry.py generate

ui-registry-check:
	python3 tools/generate_ui_registry.py check
	PYTHONDONTWRITEBYTECODE=1 python3 tests/test_ui_registry.py

ember-action-registry-generate:
	python3 tools/generate_ember_action_registry_adapter.py generate

ember-action-registry-check:
	python3 tools/generate_ember_action_registry_adapter.py check

surface-contract-gate: ui-registry-check ember-action-registry-check $(BUILD)/ui_command_state_tests
	./$(BUILD)/ui_command_state_tests

$(BUILD)/runner_demo: $(OBJECTS) $(BUILD)/src/demo.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/os2l_capture: $(OBJECTS) $(BUILD)/src/os2l_capture.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/midi_capture: $(OBJECTS) $(BUILD)/src/midi_capture.o
	$(CXX) $^ $(LDFLAGS) $(THREAD_FLAGS) -o $@

$(BUILD)/src/runner_lab.o: CXXFLAGS += $(THREAD_FLAGS)

$(BUILD)/runner_lab: $(OBJECTS) $(BUILD)/src/runner_lab.o
	$(CXX) $^ $(LDFLAGS) $(THREAD_FLAGS) -o $@

$(BUILD)/src/qualification.o: CXXFLAGS += $(THREAD_FLAGS)

$(BUILD)/emberlights_qualify: $(OBJECTS) $(BUILD)/src/qualification.o
	$(CXX) $^ $(LDFLAGS) $(THREAD_FLAGS) -o $@

$(BUILD)/emberlights_migrate: $(OBJECTS) $(BUILD)/src/soundswitch_migrate.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/core_tests: $(OBJECTS) $(BUILD)/tests/test_main.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/soundswitch_micro_session_tests: $(OBJECTS) $(BUILD)/tests/test_soundswitch_micro_session.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/raw_hardware_test_tests: $(OBJECTS) $(BUILD)/tests/test_raw_hardware_test.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/raw_hardware_test_operator_tests: $(OBJECTS) $(BUILD)/tests/test_raw_hardware_test_operator.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/connection_coordinator_tests: $(OBJECTS) $(BUILD)/tests/test_connection_coordinator.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/connection_layout_tests: $(OBJECTS) $(BUILD)/tests/test_connection_layout.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/soundswitch_control_one_tests: $(OBJECTS) $(BUILD)/tests/test_soundswitch_control_one.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/ui_command_state_tests: $(OBJECTS) $(BUILD)/tests/test_ui_command_state.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/live_ui_tests: $(OBJECTS) $(BUILD)/tests/test_live_ui.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/runner_frame_inspector_tests: $(OBJECTS) $(BUILD)/tests/test_runner_frame_inspector.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/runner_raw_hardware_parity_tests: $(OBJECTS) $(BUILD)/tests/test_runner_raw_hardware_parity.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/ui_visual_tests: $(OBJECTS) $(BUILD)/tests/test_ui_visual.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/ui_authoring_tests: $(OBJECTS) $(BUILD)/tests/test_ui_authoring.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/studio_foundation_tests: $(OBJECTS) $(BUILD)/tests/test_studio_foundation.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/static_look_authoring_tests: $(OBJECTS) $(BUILD)/tests/test_static_look_authoring.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/static_look_physical_preview_tests: $(OBJECTS) $(BUILD)/tests/test_static_look_physical_preview.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/ofl_fixture_catalog_tests: $(OBJECTS) $(BUILD)/tests/test_ofl_fixture_catalog.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/fixture_profile_editor_tests: $(OBJECTS) $(BUILD)/tests/test_fixture_profile_editor.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/fixture_controller_binding_tests: $(OBJECTS) $(BUILD)/tests/test_fixture_controller_binding.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/fixture_function_component_tests: $(OBJECTS) $(BUILD)/tests/test_fixture_function_component.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/studio_authoring_tests: $(OBJECTS) $(BUILD)/tests/test_studio_authoring.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/ember_action_foundation_tests: $(BUILD)/src/file_identity.o $(BUILD)/src/ember_action_json.o $(BUILD)/src/ember_action_compiler.o $(BUILD)/src/ember_action_registry_adapter.o $(BUILD)/tests/test_ember_action_foundation.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/ember_action_executor_tests: $(BUILD)/src/file_identity.o $(BUILD)/src/ember_action_json.o $(BUILD)/src/ember_action_compiler.o $(BUILD)/src/ember_action_registry_adapter.o $(BUILD)/src/ember_action_executor.o $(BUILD)/tests/test_ember_action_executor.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/fixture_control_action_tests: $(OBJECTS) $(BUILD)/tests/test_fixture_control_action.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/autoloop_v2_model_tests: $(OBJECTS) $(BUILD)/tests/test_autoloop_v2_model.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/autoloop_v2_persistence_tests: $(OBJECTS) $(BUILD)/tests/test_autoloop_v2_persistence.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/autoloop_v2_studio_tests: $(OBJECTS) $(BUILD)/tests/test_autoloop_v2_studio.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/autoloop_fixture_controls_tests: $(OBJECTS) $(BUILD)/tests/test_autoloop_fixture_controls.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/autoloop_autoscript_tests: $(OBJECTS) $(BUILD)/tests/test_autoloop_autoscript.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/autoloop_autoscript_studio_tests: $(OBJECTS) $(BUILD)/tests/test_autoloop_autoscript_studio.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/autoloop_v2_runtime_tests: $(OBJECTS) $(BUILD)/tests/test_autoloop_v2_runtime.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/autoloop_v2_preview_tests: $(OBJECTS) $(BUILD)/tests/test_autoloop_v2_preview.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/autoloop_v2_palette_resolution_tests: $(OBJECTS) $(BUILD)/tests/test_autoloop_v2_palette_resolution.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/autoloop_v2_runner_tests: $(OBJECTS) $(BUILD)/tests/test_autoloop_v2_runner.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/migration_portability_review_tests: $(OBJECTS) $(BUILD)/tests/test_migration_portability_review.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/core_bench: $(OBJECTS) $(BUILD)/bench/bench_main.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/performance_bench: $(OBJECTS) $(BUILD)/bench/performance_bench.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/%.o: %.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: ember-action-registry-check $(BUILD)/core_tests $(BUILD)/soundswitch_micro_session_tests $(BUILD)/raw_hardware_test_tests $(BUILD)/raw_hardware_test_operator_tests $(BUILD)/connection_layout_tests $(BUILD)/connection_coordinator_tests $(BUILD)/soundswitch_control_one_tests $(BUILD)/ui_command_state_tests $(BUILD)/live_ui_tests $(BUILD)/runner_frame_inspector_tests $(BUILD)/runner_raw_hardware_parity_tests $(BUILD)/ui_visual_tests $(BUILD)/ui_authoring_tests $(BUILD)/studio_foundation_tests $(BUILD)/static_look_authoring_tests $(BUILD)/static_look_physical_preview_tests $(BUILD)/ofl_fixture_catalog_tests $(BUILD)/fixture_profile_editor_tests $(BUILD)/fixture_controller_binding_tests $(BUILD)/fixture_function_component_tests $(BUILD)/studio_authoring_tests $(BUILD)/ember_action_foundation_tests $(BUILD)/ember_action_executor_tests $(BUILD)/fixture_control_action_tests $(BUILD)/autoloop_v2_model_tests $(BUILD)/autoloop_v2_persistence_tests $(BUILD)/autoloop_v2_studio_tests $(BUILD)/autoloop_fixture_controls_tests $(BUILD)/autoloop_autoscript_tests $(BUILD)/autoloop_autoscript_studio_tests $(BUILD)/autoloop_v2_runtime_tests $(BUILD)/autoloop_v2_preview_tests $(BUILD)/autoloop_v2_palette_resolution_tests $(BUILD)/autoloop_v2_runner_tests $(BUILD)/migration_portability_review_tests
	./$(BUILD)/core_tests
	./$(BUILD)/soundswitch_micro_session_tests
	./$(BUILD)/raw_hardware_test_tests
	./$(BUILD)/raw_hardware_test_operator_tests
	./$(BUILD)/connection_layout_tests
	./$(BUILD)/connection_coordinator_tests
	./$(BUILD)/soundswitch_control_one_tests
	./$(BUILD)/ui_command_state_tests
	./$(BUILD)/live_ui_tests
	./$(BUILD)/runner_frame_inspector_tests
	./$(BUILD)/runner_raw_hardware_parity_tests
	./$(BUILD)/ui_visual_tests
	./$(BUILD)/ui_authoring_tests
	./$(BUILD)/studio_foundation_tests
	./$(BUILD)/static_look_authoring_tests
	./$(BUILD)/static_look_physical_preview_tests
	./$(BUILD)/ofl_fixture_catalog_tests
	./$(BUILD)/fixture_profile_editor_tests
	./$(BUILD)/fixture_controller_binding_tests
	./$(BUILD)/fixture_function_component_tests
	./$(BUILD)/studio_authoring_tests
	./$(BUILD)/ember_action_foundation_tests
	./$(BUILD)/ember_action_executor_tests
	./$(BUILD)/fixture_control_action_tests
	./$(BUILD)/autoloop_v2_model_tests
	./$(BUILD)/autoloop_v2_persistence_tests
	./$(BUILD)/autoloop_v2_studio_tests
	./$(BUILD)/autoloop_fixture_controls_tests
	./$(BUILD)/autoloop_autoscript_tests
	./$(BUILD)/autoloop_autoscript_studio_tests
	./$(BUILD)/autoloop_v2_runtime_tests
	./$(BUILD)/autoloop_v2_preview_tests
	./$(BUILD)/autoloop_v2_palette_resolution_tests
	./$(BUILD)/autoloop_v2_runner_tests
	./$(BUILD)/migration_portability_review_tests

ember-action-test: ember-action-registry-check $(BUILD)/ember_action_foundation_tests $(BUILD)/ember_action_executor_tests $(BUILD)/fixture_control_action_tests
	./$(BUILD)/ember_action_foundation_tests
	./$(BUILD)/ember_action_executor_tests
	./$(BUILD)/fixture_control_action_tests

winmm-syntax:
	$(CXX) -std=c++20 -Wall -Wextra -Wpedantic -Werror -D_WIN32 \
		-Itests/winmm_stubs -Iinclude -fsyntax-only src/winmm_midi.cpp

dmx-usb-syntax:
	$(CXX) -std=c++20 -Wall -Wextra -Wpedantic -Werror -D_WIN32 \
		-Itests/winmm_stubs -Iinclude -fsyntax-only src/dmx_usb_pro.cpp

bench: $(BUILD)/core_bench $(BUILD)/performance_bench
	./$(BUILD)/core_bench
	./$(BUILD)/performance_bench

smoke: $(BUILD)/runner_lab
	./$(BUILD)/runner_lab --os2l-port 0 --duration 1 --manual-bpm 120

clean:
	rm -rf $(BUILD)

-include $(DEPS)
