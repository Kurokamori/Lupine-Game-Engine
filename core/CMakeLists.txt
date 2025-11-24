# Lupine Core Library

# Generate shaders header from shader source files
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/include/lupine/rendering/GeneratedShaders.hpp"
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/shaders/compile_shaders.py"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/compile_shaders.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/OpenGL/standard3_d.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/OpenGL/standard3_d.frag"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/OpenGL/p_b_r.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/OpenGL/p_b_r.frag"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/OpenGL/sprite2_d.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/OpenGL/sprite2_d.frag"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/OpenGL/sprite3_d.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/OpenGL/sprite3_d.frag"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/OpenGL/text.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/OpenGL/text.frag"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/OpenGL/unlit.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/OpenGL/unlit.frag"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/OpenGL/wireframe.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/OpenGL/wireframe.frag"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/OpenGL/line.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/OpenGL/line.frag"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/OpenGL/shadow_map.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/OpenGL/shadow_map.frag"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/OpenGL/shadow_cube.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/OpenGL/shadow_cube.frag"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/OpenGL/rounded_rect.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/OpenGL/rounded_rect.frag"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/OpenGL/rounded_rect_border.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/OpenGL/rounded_rect_border.frag"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
    COMMENT "Generating shader header from OpenGL shader sources..."
    VERBATIM
)

# Create a target for the generated shader header
add_custom_target(generate_shaders
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/lupine/rendering/GeneratedShaders.hpp"
)

# Collect all source files using GLOB
file(GLOB_RECURSE CORE_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/lupine/core/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/lupine/core/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/lupine/components/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/lupine/components/*.h"
)

file(GLOB_RECURSE CORE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

# Create the core library
add_library(lupine_core STATIC
    ${CORE_HEADERS}
    ${CORE_SOURCES}
    "${CMAKE_CURRENT_SOURCE_DIR}/include/lupine/rendering/GeneratedShaders.hpp"
)

# Make sure shaders are generated before building the library
add_dependencies(lupine_core generate_shaders)

# Set include directories
target_include_directories(lupine_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Link dependencies
target_link_libraries(lupine_core
    PUBLIC
        spdlog::spdlog_header_only
        glm::glm
        nlohmann_json::nlohmann_json
        box2d::box2d
        ${BULLET_LIBRARIES}
        ${LUA_LIBRARIES}
        sol2::sol2
        # Use pybind11::pybind11 (header-only) instead of pybind11::embed
        # This provides pybind11 headers without linking Python library
        pybind11::pybind11
        assimp::assimp
)

# Find stb (header-only library)
find_path(STB_INCLUDE_DIRS "stb_image.h")
if(STB_INCLUDE_DIRS)
    target_include_directories(lupine_core PUBLIC ${STB_INCLUDE_DIRS})
    message(STATUS "Core: Found stb headers at ${STB_INCLUDE_DIRS}")
else()
    message(WARNING "Core: stb headers not found, asset loading may not work")
endif()

# Find miniaudio (header-only library)
find_path(MINIAUDIO_INCLUDE_DIRS "miniaudio.h")
if(MINIAUDIO_INCLUDE_DIRS)
    target_include_directories(lupine_core PUBLIC ${MINIAUDIO_INCLUDE_DIRS})
    message(STATUS "Core: Found miniaudio headers at ${MINIAUDIO_INCLUDE_DIRS}")
else()
    message(WARNING "Core: miniaudio headers not found, audio playback will not work")
endif()

# Add Lua include directories
if(LUA_INCLUDE_DIR)
    target_include_directories(lupine_core PUBLIC ${LUA_INCLUDE_DIR})
    message(STATUS "Core: Added Lua include directory: ${LUA_INCLUDE_DIR}")
endif()

# Link mRuby if available
if(LUPINE_HAS_MRUBY)
    target_link_libraries(lupine_core PUBLIC mruby)
    target_compile_definitions(lupine_core PUBLIC LUPINE_HAS_MRUBY)

    # On Windows, mRuby needs additional system libraries
    if(WIN32)
        target_link_libraries(lupine_core PUBLIC ws2_32)  # Winsock2 for socket functions
    endif()

    message(STATUS "Core: Linking mRuby support")
endif()

# Enable GLM experimental features (required for some GLM extensions)
target_compile_definitions(lupine_core PUBLIC GLM_ENABLE_EXPERIMENTAL)

# Conditionally link graphics APIs
if(LUPINE_HAS_OPENGL)
    target_link_libraries(lupine_core
        PUBLIC
            OpenGL::GL
            glad::glad
    )
    message(STATUS "Core: Linking OpenGL support")
endif()

if(LUPINE_HAS_VULKAN)
    target_link_libraries(lupine_core
        PUBLIC
            Vulkan::Vulkan
    )
    target_include_directories(lupine_core
        PUBLIC
            ${Vulkan_INCLUDE_DIRS}
    )
    message(STATUS "Core: Linking Vulkan support")
endif()

# Set target properties
set_target_properties(lupine_core PROPERTIES
    POSITION_INDEPENDENT_CODE ON  # Required for Python modules
    OUTPUT_NAME "lupine_core"
    DEBUG_POSTFIX "d"
)

# Platform-specific settings
if(WIN32)
    target_compile_definitions(lupine_core PUBLIC LUPINE_CORE_WINDOWS)
elseif(APPLE)
    target_compile_definitions(lupine_core PUBLIC LUPINE_CORE_MACOS)
elseif(UNIX)
    target_compile_definitions(lupine_core PUBLIC LUPINE_CORE_LINUX)
endif()

message(STATUS "Core library configured with ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "  Headers: ${CORE_HEADERS}")
message(STATUS "  Sources: ${CORE_SOURCES}")
