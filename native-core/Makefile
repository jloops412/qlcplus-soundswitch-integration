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
LDFLAGS += -lws2_32 -lwinmm -lsetupapi -lwinusb -ladvapi32 -lole32 -luuid
endif

BUILD := build
SOURCES := \
	src/layer_resolver.cpp \
	src/fixture.cpp \
	src/fixture_library.cpp \
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
	src/engine.cpp \
	src/project.cpp \
	src/hardware_qualification.cpp \
	src/project_edit_history.cpp \
	src/project_io.cpp \
	src/file_identity.cpp \
	src/audio_assets.cpp \
	src/qlc_fixture_import.cpp \
	src/soundswitch_import.cpp \
	src/soundswitch_v1.cpp \
	src/compiler.cpp \
	src/runner.cpp
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
	$(BUILD)/tests/test_soundswitch_control_one.d \
	$(BUILD)/bench/bench_main.d \
	$(BUILD)/bench/performance_bench.d

.PHONY: all runner_lab qualification migration test winmm-syntax dmx-usb-syntax bench smoke clean

all: $(BUILD)/runner_demo $(BUILD)/os2l_capture $(BUILD)/midi_capture $(BUILD)/runner_lab $(BUILD)/emberlights_qualify $(BUILD)/emberlights_migrate $(BUILD)/core_tests $(BUILD)/soundswitch_micro_session_tests $(BUILD)/soundswitch_control_one_tests $(BUILD)/core_bench $(BUILD)/performance_bench

runner_lab: $(BUILD)/runner_lab

qualification: $(BUILD)/emberlights_qualify

migration: $(BUILD)/emberlights_migrate

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

$(BUILD)/soundswitch_control_one_tests: $(OBJECTS) $(BUILD)/tests/test_soundswitch_control_one.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/core_bench: $(OBJECTS) $(BUILD)/bench/bench_main.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/performance_bench: $(OBJECTS) $(BUILD)/bench/performance_bench.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/%.o: %.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: $(BUILD)/core_tests $(BUILD)/soundswitch_micro_session_tests $(BUILD)/soundswitch_control_one_tests
	./$(BUILD)/core_tests
	./$(BUILD)/soundswitch_micro_session_tests
	./$(BUILD)/soundswitch_control_one_tests

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
