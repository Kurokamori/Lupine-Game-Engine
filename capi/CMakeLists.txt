cmake_minimum_required(VERSION 3.15)

# C API library
project(lupine_capi VERSION 1.0.0 LANGUAGES C CXX)

# Gather all source files using GLOB_RECURSE
file(GLOB_RECURSE CAPI_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

file(GLOB_RECURSE CAPI_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

# Create shared library
add_library(lupine_capi SHARED
    ${CAPI_SOURCES}
)

# Set C++17 for implementation (wrapping C++ code)
set_target_properties(lupine_capi PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

# Find Python (needed because lupine_core depends on it)
find_package(Python COMPONENTS Development)

# Link against C++ engine modules
target_link_libraries(lupine_capi PRIVATE
    lupine_core
)

# Add Python libraries if found (transitively needed by lupine_core)
if(Python_FOUND)
    target_link_libraries(lupine_capi PRIVATE Python::Python)
endif()

# Public include directory (C headers only)
target_include_directories(lupine_capi PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Private include for accessing C++ headers
target_include_directories(lupine_capi PRIVATE
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/engine/include
    ${CMAKE_SOURCE_DIR}/runtime/include
)

# Export C symbols, hide C++ symbols
if(WIN32)
    target_compile_definitions(lupine_capi PRIVATE LUPINE_CAPI_EXPORT)
    target_compile_definitions(lupine_capi PUBLIC LUPINE_CAPI_DLL)
else()
    set_target_properties(lupine_capi PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        C_VISIBILITY_PRESET default
    )
endif()

# Installation rules
install(TARGETS lupine_capi
    EXPORT LupineTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# Optional: Build examples
option(LUPINE_CAPI_BUILD_EXAMPLES "Build C API examples" ON)

if(LUPINE_CAPI_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
