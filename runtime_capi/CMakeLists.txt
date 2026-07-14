cmake_minimum_required(VERSION 3.15)

# Runtime C API library
#
# Optional superset of lupine_capi: bundles the full headless C API (every lc_*
# function compiled from ../capi/src) AND the runtime/window/main-loop layer
# (src/lc_runtime.cpp wrapping lupine::RuntimeApp). Produces a single shared
# library so a language binding gets one shared engine core, a window and a main
# loop. Consumers that do not need a window should link lupine_capi instead and
# avoid the SDL2 / graphics-backend dependencies pulled in here.
project(lupine_runtime_capi VERSION 1.0.0 LANGUAGES C CXX)

# Find required packages (same surface as the headless C API)
find_package(OpenGL REQUIRED)
find_package(GLEW CONFIG REQUIRED)
find_package(Python3 COMPONENTS Development REQUIRED)

# Reuse the entire headless C API implementation, then add the runtime wrapper.
file(GLOB_RECURSE CAPI_SHARED_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/capi/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/capi/src/*.c"
)

file(GLOB_RECURSE RUNTIME_CAPI_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

add_library(lupine_runtime_capi SHARED
    ${CAPI_SHARED_SOURCES}
    ${RUNTIME_CAPI_SOURCES}
)

set_target_properties(lupine_runtime_capi PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

# Link the runtime static library (pulls in lupine_core, SDL2 and the graphics
# backends), plus the same GL/Python dependencies the headless C API needs.
target_link_libraries(lupine_runtime_capi PRIVATE
    lupine_runtime_lib
    GLEW::GLEW
    OpenGL::GL
)

if(WIN32)
    target_link_libraries(lupine_runtime_capi PRIVATE
        $<$<CONFIG:Debug>:${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/debug/lib/python312_d.lib>
        $<$<CONFIG:Release>:${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/lib/python312.lib>
    )
else()
    target_link_libraries(lupine_runtime_capi PRIVATE
        Python3::Python
    )
    if(APPLE)
        target_link_libraries(lupine_runtime_capi PRIVATE "-liconv")
    endif()
endif()

# Public headers: the headless C API headers plus this library's runtime header.
target_include_directories(lupine_runtime_capi PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/capi/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Private includes for accessing C++ engine headers and the shared C API internals.
target_include_directories(lupine_runtime_capi PRIVATE
    ${CMAKE_SOURCE_DIR}/capi/src
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/engine/include
    ${CMAKE_SOURCE_DIR}/runtime/include
)

# Export C symbols (same macro the shared capi sources use).
if(WIN32)
    target_compile_definitions(lupine_runtime_capi PRIVATE LUPINE_CAPI_EXPORT)
    target_compile_definitions(lupine_runtime_capi PUBLIC LUPINE_CAPI_DLL)
else()
    set_target_properties(lupine_runtime_capi PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        C_VISIBILITY_PRESET default
    )
endif()

install(TARGETS lupine_runtime_capi
    EXPORT LupineTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)
