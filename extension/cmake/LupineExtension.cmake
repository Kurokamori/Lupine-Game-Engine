# LupineExtension.cmake
#
# CMake helper for building a native Lupine extension (a GDExtension-equivalent
# C++ component library) OUT OF TREE, against an installed Lupine SDK.
#
# Usage in a plugin project's CMakeLists.txt:
#
#   cmake_minimum_required(VERSION 3.20)
#   project(my_extension LANGUAGES CXX)
#   # Point CMake at the installed SDK, then:
#   include(/path/to/lib/cmake/Lupine/LupineExtension.cmake)
#   lupine_add_extension(my_extension SOURCES src/MyComponent.cpp)
#
# This produces my_extension.dll / .so / .dylib that links only the header-only
# SDK (no engine binaries). Drop the result next to a *.lupineext manifest in
# your project and the engine loads it at project open.
#
# Requires nlohmann/json to be discoverable (find_package(nlohmann_json)). Most
# toolchains provide it via a package manager; the SDK uses it for JSON marshalling.

if(COMMAND lupine_add_extension)
    return()
endif()

get_filename_component(_LUPINE_EXT_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)

# Resolve the SDK include directory. Two supported layouts:
#  1. Installed:  <prefix>/lib/cmake/Lupine/LupineExtension.cmake  +  <prefix>/include
#  2. In-source:  <repo>/extension/cmake/LupineExtension.cmake     +  <repo>/extension/{include,sdk/include}
set(_LUPINE_EXT_INCLUDE_DIRS "")
if(EXISTS "${_LUPINE_EXT_CMAKE_DIR}/../../../include/lupine/lupine_extension_interface.h")
    get_filename_component(_inc "${_LUPINE_EXT_CMAKE_DIR}/../../../include" ABSOLUTE)
    list(APPEND _LUPINE_EXT_INCLUDE_DIRS "${_inc}")
elseif(EXISTS "${_LUPINE_EXT_CMAKE_DIR}/../include/lupine/lupine_extension_interface.h")
    get_filename_component(_inc1 "${_LUPINE_EXT_CMAKE_DIR}/../include" ABSOLUTE)
    get_filename_component(_inc2 "${_LUPINE_EXT_CMAKE_DIR}/../sdk/include" ABSOLUTE)
    list(APPEND _LUPINE_EXT_INCLUDE_DIRS "${_inc1}" "${_inc2}")
else()
    message(FATAL_ERROR "LupineExtension.cmake: could not locate the SDK headers relative to ${_LUPINE_EXT_CMAKE_DIR}")
endif()

if(NOT TARGET Lupine::Extension)
    add_library(Lupine::Extension INTERFACE IMPORTED)
    set_target_properties(Lupine::Extension PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${_LUPINE_EXT_INCLUDE_DIRS}"
        INTERFACE_COMPILE_FEATURES "cxx_std_17"
    )
    # nlohmann/json is required by the SDK headers.
    find_package(nlohmann_json CONFIG QUIET)
    if(TARGET nlohmann_json::nlohmann_json)
        set_property(TARGET Lupine::Extension APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES nlohmann_json::nlohmann_json)
    else()
        message(STATUS "LupineExtension: nlohmann_json package not found; ensure <nlohmann/json.hpp> is on the include path.")
    endif()
endif()

# lupine_add_extension(<name> SOURCES <a.cpp> <b.cpp> ...)
# Builds a native extension module library named exactly <name>.<dll|so|dylib>.
function(lupine_add_extension name)
    cmake_parse_arguments(LAE "" "" "SOURCES" ${ARGN})
    if(NOT LAE_SOURCES)
        message(FATAL_ERROR "lupine_add_extension(${name}): no SOURCES given")
    endif()

    add_library(${name} MODULE ${LAE_SOURCES})
    target_link_libraries(${name} PRIVATE Lupine::Extension)
    set_target_properties(${name} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        PREFIX ""
    )
    if(APPLE)
        set_target_properties(${name} PROPERTIES SUFFIX ".dylib")
    endif()
    # Hide everything except the exported lupine_extension_* entry points on
    # platforms that support visibility control.
    if(NOT MSVC)
        set_target_properties(${name} PROPERTIES
            CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN ON)
    endif()
endfunction()
