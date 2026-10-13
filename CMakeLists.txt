cmake_minimum_required(VERSION 3.21)
project(LISAccess LANGUAGES CXX)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "LISAccess requires Windows and the MSVC toolchain.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "LISAccess requires x86. Configure with -A Win32.")
endif()

function(lis_target target)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(${target} PRIVATE cxx_std_17)
    target_compile_definitions(${target} PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /EHsc)
    set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endfunction()

# Explicit module source lists: a new file must have an intentional home.
add_library(lis_platform OBJECT
    src/platform/foreground.cpp
    src/platform/log.cpp
    src/platform/text.cpp
)
add_library(lis_engine OBJECT
    src/engine/build_verification.cpp
    src/engine/gfx.cpp
    src/engine/localization.cpp
    src/engine/memory.cpp
    src/engine/object_registry.cpp
    src/engine/reflection.cpp
)
add_library(lis_input OBJECT src/input/devices.cpp)
add_library(lis_output OBJECT
    src/output/audio_cues.cpp
    src/output/speech.cpp
)
add_library(lis_features OBJECT
    src/features/camera/facing.cpp
    src/features/interactions/actions.cpp
    src/features/interactions/scanner.cpp
    src/features/menus/diagnostics.cpp
    src/features/menus/menus.cpp
    src/features/navigation/collision.cpp
    src/features/navigation/description.cpp
    src/features/navigation/navigation.cpp
    src/features/navigation/world_sample.cpp
    src/features/world/discovery.cpp
    src/features/world/player.cpp
    src/features/world/world.cpp
)
foreach(module lis_platform lis_engine lis_input lis_output lis_features)
    lis_target(${module})
endforeach()

add_library(dinput8 SHARED
    src/bootstrap/platform_lifecycle.cpp
    src/bootstrap/proxy.cpp
    src/bootstrap/runtime.cpp
    src/bootstrap/dinput8.def
)
lis_target(dinput8)
# A system-DLL proxy, not a COM import library: preserve system export ordinals.
target_link_options(dinput8 PRIVATE /IGNORE:4104)
target_link_libraries(dinput8 PRIVATE
    lis_platform lis_engine lis_input lis_output lis_features
    ole32 sapi shell32 uuid user32 bcrypt
)
set_target_properties(dinput8 PROPERTIES PREFIX "" OUTPUT_NAME "dinput8")

enable_testing()
function(lis_test target name)
    add_executable(${target} ${ARGN})
    lis_target(${target})
    add_test(NAME ${name} COMMAND ${target})
endfunction()
lis_test(narration_test narration tests/narration_test.cpp)
lis_test(navigation_test navigation_geometry tests/navigation_test.cpp)
lis_test(native_abi_test native_dispatch_abi tests/native_abi_test.cpp)
lis_test(input_commands_test input_commands tests/input_commands_test.cpp)
lis_test(interaction_fields_test interaction_fields tests/interaction_fields_test.cpp)
lis_test(object_registry_test object_registry tests/object_registry_test.cpp
    src/engine/memory.cpp src/engine/object_registry.cpp)
lis_test(navigation_description_test navigation_description tests/navigation_description_test.cpp
    src/features/navigation/description.cpp)
find_program(POWERSHELL_EXECUTABLE NAMES powershell REQUIRED)
add_test(NAME architecture COMMAND "${POWERSHELL_EXECUTABLE}" -NoProfile -File
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/check-architecture.ps1")
add_test(NAME architecture_gate_regression COMMAND "${POWERSHELL_EXECUTABLE}" -NoProfile -File
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/architecture_test.ps1")
