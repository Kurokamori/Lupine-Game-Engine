# Example native extension: hello_component (provides the OrbitMover component).
#
# When built as part of the engine (LUPINE_BUILD_EXTENSION_SDK=ON), the
# lupine_add_extension() helper is already available from the parent
# extension/CMakeLists.txt. When built standalone, include LupineExtension.cmake
# from an installed SDK first (see hello_component.lupineext and the docs).

if(NOT COMMAND lupine_add_extension)
    cmake_minimum_required(VERSION 3.20)
    project(hello_component LANGUAGES CXX)
    # Standalone build: locate the SDK helper relative to this example.
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/LupineExtension.cmake)
endif()

lupine_add_extension(hello_component
    SOURCES
        src/OrbitMover.cpp
)

# Stage the built binary next to the manifest under bin/ so the manifest's
# relative path resolves when this folder is dropped into a project.
add_custom_command(TARGET hello_component POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:hello_component>" "${CMAKE_CURRENT_SOURCE_DIR}/bin/"
    COMMENT "Staging hello_component binary into example bin/"
)
