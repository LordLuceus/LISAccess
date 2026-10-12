cmake_minimum_required(VERSION 3.21)
project(LISAccess LANGUAGES CXX)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "LISAccess requires Windows and the MSVC toolchain.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "LISAccess requires x86. Configure with -A Win32.")
endif()

add_library(dinput8 SHARED
    src/proxy.cpp
    src/platform.cpp
    src/runtime.cpp
    src/dinput8.def
)
target_compile_features(dinput8 PRIVATE cxx_std_17)
target_compile_definitions(dinput8 PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(dinput8 PRIVATE /W4 /permissive- /utf-8 /EHsc)
# This is a system-DLL proxy, not a COM client import library. Preserve the
# system export ordinals; making COM exports PRIVATE triggers incompatible advice.
target_link_options(dinput8 PRIVATE /IGNORE:4104)
set_property(TARGET dinput8 PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_link_libraries(dinput8 PRIVATE ole32 sapi shell32 uuid user32 bcrypt)
set_target_properties(dinput8 PROPERTIES PREFIX "" OUTPUT_NAME "dinput8")
enable_testing()
add_executable(narration_test tests/narration_test.cpp)
target_include_directories(narration_test PRIVATE src)
target_compile_features(narration_test PRIVATE cxx_std_17)
target_compile_options(narration_test PRIVATE /W4 /utf-8)
set_property(TARGET narration_test PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_test(NAME narration COMMAND narration_test)
