# Lupine Runtime Library

# Find SDL2
find_package(SDL2 CONFIG REQUIRED)

# Collect all header files using GLOB
file(GLOB_RECURSE RUNTIME_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/lupine/runtime/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/lupine/runtime/*.h"
)

# Collect source files excluding main.cpp and Python bindings
file(GLOB_RECURSE RUNTIME_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

# Remove main.cpp and Python bindings from the source list for the library
list(FILTER RUNTIME_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")
list(FILTER RUNTIME_SOURCES EXCLUDE REGEX ".*Python\\.cpp$")

# Create the runtime static library
add_library(lupine_runtime_lib STATIC
    ${RUNTIME_HEADERS}
    ${RUNTIME_SOURCES}
)

# Set include directories
target_include_directories(lupine_runtime_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Link dependencies
target_link_libraries(lupine_runtime_lib
    PUBLIC
        lupine_core
        glm::glm
        nlohmann_json::nlohmann_json
        SDL2::SDL2
        SDL2::SDL2main
)

# Conditionally link GLEW if OpenGL is available
# Note: We use GLAD from core, but GLEW can coexist if needed
find_package(GLEW CONFIG)
if(GLEW_FOUND)
    target_link_libraries(lupine_runtime_lib
        PUBLIC
            GLEW::GLEW
    )
    message(STATUS "Runtime: Linking GLEW")
else()
    message(STATUS "Runtime: GLEW not found, using GLAD from core")
endif()

# Graphics APIs are inherited from lupine_core
if(LUPINE_HAS_OPENGL)
    message(STATUS "Runtime: OpenGL support inherited from core")
endif()

if(LUPINE_HAS_VULKAN)
    message(STATUS "Runtime: Vulkan support inherited from core")
endif()

# Set target properties
set_target_properties(lupine_runtime_lib PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME "lupine_runtime_lib"
    DEBUG_POSTFIX "d"
)

# Configure icon resource for Windows
set(LUPINE_RUNTIME_ICON_PATH "${CMAKE_SOURCE_DIR}/icon.ico" CACHE FILEPATH "Path to icon file for runtime executable")

# Create standalone runtime executable
if(WIN32)
    # Add resource file for Windows icon
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/resource.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/resource.rc"
        @ONLY
    )

    add_executable(lupine_runtime
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/resource.rc
    )
else()
    add_executable(lupine_runtime
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    )
endif()

# Link the executable to the runtime library
target_link_libraries(lupine_runtime
    PRIVATE
        lupine_runtime_lib
        # Link Python embedding for standalone executable
        pybind11::embed
)

# Set executable properties
set_target_properties(lupine_runtime PROPERTIES
    OUTPUT_NAME "lupine_runtime"
    DEBUG_POSTFIX "d"
)

# Platform-specific settings for executable
if(WIN32)
    # Windows console application
    set_target_properties(lupine_runtime PROPERTIES
        WIN32_EXECUTABLE FALSE
    )
elseif(APPLE)
    # macOS bundle application (can be changed to console)
    set_target_properties(lupine_runtime PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.lupine.runtime"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    )
endif()

# Create Python module
pybind11_add_module(lupine_runtime_py
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RuntimePython.cpp
)

# Link the Python module to the runtime library
target_link_libraries(lupine_runtime_py
    PRIVATE
        lupine_runtime_lib
)


# Define preprocessor macro to indicate this is a Python module
target_compile_definitions(lupine_runtime_py PRIVATE LUPINE_PYTHON_MODULE)

# Set Python module properties
set_target_properties(lupine_runtime_py PROPERTIES
    OUTPUT_NAME "lupine_runtime"
)

# Set output directories for multi-config generators
foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG} CONFIG_UPPER)
    set_target_properties(lupine_runtime_py PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${CMAKE_BINARY_DIR}/python_modules/${CONFIG}"
        RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${CMAKE_BINARY_DIR}/python_modules/${CONFIG}"
    )
endforeach()

# Fallback for single-config generators
set_target_properties(lupine_runtime_py PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/python_modules"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/python_modules"
)

# Copy Python module to editor folder after build
add_custom_command(TARGET lupine_runtime_py POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/editor"
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:lupine_runtime_py>
        "${CMAKE_SOURCE_DIR}/editor/"
    COMMENT "Copying lupine_runtime Python module to editor folder: $<TARGET_FILE:lupine_runtime_py>"
)

message(STATUS "Runtime library configured")
message(STATUS "  Headers: ${RUNTIME_HEADERS}")
message(STATUS "  Sources: ${RUNTIME_SOURCES}")
message(STATUS "  Executable: lupine_runtime")
if(WIN32)
    message(STATUS "  Icon: ${LUPINE_RUNTIME_ICON_PATH}")
    message(STATUS "  To customize the icon, set -DLUPINE_RUNTIME_ICON_PATH=path/to/your/icon.ico")
endif()
message(STATUS "  Python module will be copied to: ${CMAKE_SOURCE_DIR}/editor/")
