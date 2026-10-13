# Source-built, static Prism. Never reuse Hortator's x64 library in this x86 DLL.
include(FetchContent)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Link Prism into the mod" FORCE)
set(PRISM_ENABLE_GDEXTENSION OFF CACHE BOOL "" FORCE)
set(PRISM_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(PRISM_ENABLE_DEMOS OFF CACHE BOOL "" FORCE)
set(PRISM_ENABLE_LINTING OFF CACHE BOOL "" FORCE)
set(PRISM_ENABLE_VCPKG_SPECIFIC_OPTIONS OFF CACHE BOOL "" FORCE)
set(PRISM_ENABLE_LEGACY_BACKENDS OFF CACHE BOOL "" FORCE)
set(PRISM_ENABLE_SHIMS OFF CACHE BOOL "" FORCE)
set(PRISM_BUILD_WINELIBS OFF CACHE BOOL "" FORCE)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "Match mod CRT" FORCE)
# VS-generator builds know the librarian even outside a Developer Command Prompt.
set(LIB_TOOL "${CMAKE_AR}" CACHE FILEPATH "MSVC librarian for Prism import libraries" FORCE)
FetchContent_Declare(prism
    URL https://codeload.github.com/ethindp/prism/zip/c30d865faf9b4c38769cc58c2f33cbac78fba3ef
    URL_HASH SHA256=c5d3f0be06bd34fea0478f1c38ce4de85cd3a21dbd2a1ed869b21c8a0a80ebaf
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)
FetchContent_MakeAvailable(prism)
include("${CMAKE_CURRENT_LIST_DIR}/prism-sapi-literal.cmake")
set(LIS_PRISM_SOURCE_DIR "${prism_SOURCE_DIR}" PARENT_SCOPE)
# MSVC implements the required C features, but CMake does not advertise C23 for
# this compiler. Keep this adjustment scoped to Prism, including its RPC C stub.
# Disable upstream's unconditional LTO to keep iteration/link times bounded.
get_directory_property(prism_targets DIRECTORY "${prism_SOURCE_DIR}" BUILDSYSTEM_TARGETS)
foreach(target IN LISTS prism_targets)
    get_target_property(kind ${target} TYPE)
    if(kind MATCHES "^(OBJECT|STATIC|SHARED|INTERFACE)_LIBRARY$")
        set_target_properties(${target} PROPERTIES C_STANDARD_REQUIRED OFF
            INTERPROCEDURAL_OPTIMIZATION OFF
            INTERPROCEDURAL_OPTIMIZATION_RELEASE OFF
            INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF
            INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO OFF
            INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL OFF)
        if(NOT kind STREQUAL "INTERFACE_LIBRARY")
            target_compile_options(${target} PRIVATE "$<$<COMPILE_LANGUAGE:C>:/std:clatest>")
        endif()
    endif()
endforeach()
# Consumers use prism::prism, not the raw archive: its WHOLE_ARCHIVE wrapper keeps
# the registered backends from being stripped by the linker.
