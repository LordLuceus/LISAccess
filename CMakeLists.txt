cmake_minimum_required(VERSION 3.24)
project(LISAccess LANGUAGES C CXX)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "LISAccess requires Windows and the MSVC toolchain.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "LISAccess requires x86. Configure with -A Win32.")
endif()

add_subdirectory(extern)

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
    src/output/prism_bridge.cpp
)
target_link_libraries(lis_output PRIVATE prism::prism)
add_library(lis_features OBJECT
    src/features/dialogue/dialogue.cpp
    src/features/dialogue/native.cpp
    src/features/dialogue/scripted_owner.cpp
    src/features/camera/facing.cpp
    src/features/interactions/actions.cpp
    src/features/interactions/scanner.cpp
    src/features/menus/diagnostics.cpp
    src/features/menus/menus.cpp
    src/features/prompts/prompts.cpp
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
lis_test(popup_test startup_popup tests/popup_test.cpp)
lis_test(prompts_test prompt_policy tests/prompts_test.cpp)
lis_test(dialogue_test dialogue_policy tests/dialogue_test.cpp)
lis_test(scripted_choices_test scripted_choices tests/scripted_choices_test.cpp)
lis_test(dialogue_lifecycle_test dialogue_lifecycle tests/dialogue_lifecycle_test.cpp src/features/dialogue/dialogue.cpp)
lis_test(navigation_test navigation_geometry tests/navigation_test.cpp)
lis_test(target_list_test scanner_target_list tests/target_list_test.cpp)
lis_test(native_abi_test native_dispatch_abi tests/native_abi_test.cpp)
lis_test(input_commands_test input_commands tests/input_commands_test.cpp)
lis_test(interaction_fields_test interaction_fields tests/interaction_fields_test.cpp)
lis_test(object_registry_test object_registry tests/object_registry_test.cpp
    src/engine/memory.cpp src/engine/object_registry.cpp)
lis_test(navigation_description_test navigation_description tests/navigation_description_test.cpp
    src/features/navigation/description.cpp)
lis_test(speech_test speech_routing tests/speech_test.cpp src/output/speech.cpp
    src/output/prism_bridge.cpp src/platform/text.cpp)
target_include_directories(speech_test PRIVATE "${LIS_PRISM_SOURCE_DIR}/include")
target_compile_definitions(speech_test PRIVATE PRISM_STATIC)
# Manual smoke test: real desktop/RPC/voice availability is host-dependent and
# belongs outside CTest. Default probes without speech; --output emits one line.
add_executable(prism_smoke tests/prism_smoke.cpp src/output/speech.cpp
    src/output/prism_bridge.cpp src/platform/text.cpp)
lis_target(prism_smoke)
target_link_libraries(prism_smoke PRIVATE prism::prism ole32 sapi shell32 uuid user32)
foreach(target lis_output speech_test prism_smoke)
    # Match Prism's full-width feature enum; MSVC otherwise truncates its bit 63.
    target_compile_options(${target} PRIVATE /Zc:enumTypes)
endforeach()
find_program(POWERSHELL_EXECUTABLE NAMES powershell REQUIRED)
add_custom_target(prism_notices ALL
    COMMAND "${POWERSHELL_EXECUTABLE}" -NoProfile -File
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/stage-prism-notices.ps1"
        -PrismSource "${LIS_PRISM_SOURCE_DIR}"
        -OutputDirectory "$<TARGET_FILE_DIR:dinput8>/notices"
    COMMENT "Stage pinned Prism attribution and license notices"
    VERBATIM)
add_dependencies(dinput8 prism_notices)
add_test(NAME prism_notice_validation COMMAND "${POWERSHELL_EXECUTABLE}" -NoProfile -File
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/notices_test.ps1" -PrismSource "${LIS_PRISM_SOURCE_DIR}")
add_test(NAME architecture COMMAND "${POWERSHELL_EXECUTABLE}" -NoProfile -File
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/check-architecture.ps1")
add_test(NAME architecture_gate_regression COMMAND "${POWERSHELL_EXECUTABLE}" -NoProfile -File
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/architecture_test.ps1")
