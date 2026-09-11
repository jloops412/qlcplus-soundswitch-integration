cmake_minimum_required(VERSION 3.22)
project(soundswitch_portable_tests LANGUAGES CXX)
set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 REQUIRED COMPONENTS Core)
enable_testing()

foreach(test_name IN ITEMS protocol intensity performance priority)
    add_executable(soundswitch_${test_name}_tests
        ../soundswitch${test_name}.cpp
        soundswitch${test_name}test.cpp
    )
    target_link_libraries(soundswitch_${test_name}_tests PRIVATE Qt6::Core)
    add_test(NAME ${test_name} COMMAND soundswitch_${test_name}_tests)
endforeach()

add_executable(soundswitch_midi_tests
    ../soundswitchmidiinput.cpp ../soundswitchmidiinput.h
    ../soundswitchperformance.cpp
    soundswitchmiditest.cpp soundswitchmiditestbackend.h
)
target_compile_definitions(soundswitch_midi_tests PRIVATE SOUNDSWITCH_MIDI_TEST_BACKEND)
target_link_libraries(soundswitch_midi_tests PRIVATE Qt6::Core)
add_test(NAME midi COMMAND soundswitch_midi_tests)

add_executable(soundswitch_effects_tests ../soundswitcheffects.cpp
    ../soundswitchintensity.cpp soundswitcheffectstest.cpp)
target_link_libraries(soundswitch_effects_tests PRIVATE Qt6::Core)
add_test(NAME effects COMMAND soundswitch_effects_tests)
