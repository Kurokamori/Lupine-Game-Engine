cmake_minimum_required(VERSION 3.15)

# C API library
project(lupine_capi VERSION 1.0.0 LANGUAGES C CXX)

# Find required packages
find_package(OpenGL REQUIRED)
find_package(GLEW CONFIG REQUIRED)
find_package(Python3 COMPONENTS Development REQUIRED)

# Gather all source files using GLOB_RECURSE
# CONFIGURE_DEPENDS re-globs at build time so newly added C API files are
# picked up without a manual reconfigure.
file(GLOB_RECURSE CAPI_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

file(GLOB_RECURSE CAPI_HEADERS CONFIGURE_DEPENDS
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

# Link against C++ engine modules and their dependencies
target_link_libraries(lupine_capi PRIVATE
    lupine_core
    GLEW::GLEW
    OpenGL::GL
)

# Platform-specific Python linking
# CAPI needs Python since it wraps Python-using core functionality
if(WIN32)
    # Link against Python using full paths (vcpkg)
    target_link_libraries(lupine_capi PRIVATE
        $<$<CONFIG:Debug>:${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/debug/lib/python312_d.lib>
        $<$<CONFIG:Release>:${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/lib/python312.lib>
    )
else()
    # Link against Python using CMake's found package (macOS/Linux)
    target_link_libraries(lupine_capi PRIVATE
        Python3::Python
    )
    # macOS-specific linking for iconv (required by libintl/gettext)
    if(APPLE)
        # Use system iconv on macOS
        target_link_libraries(lupine_capi PRIVATE "-liconv")
    endif()
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
