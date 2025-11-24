# Lupine Engine Library

# Collect all header files using GLOB
file(GLOB_RECURSE ENGINE_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/lupine/engine/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/lupine/engine/*.h"
)

# Collect source files excluding Python bindings
file(GLOB_RECURSE ENGINE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

# Remove Python bindings from the source list for the library
list(FILTER ENGINE_SOURCES EXCLUDE REGEX ".*Python\\.cpp$")

# Create the engine static library
add_library(lupine_engine_lib STATIC
    ${ENGINE_HEADERS}
    ${ENGINE_SOURCES}
)

# Set include directories
target_include_directories(lupine_engine_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Link dependencies
target_link_libraries(lupine_engine_lib
    PUBLIC
        lupine_core
        glm::glm
        nlohmann_json::nlohmann_json
)

# Link mRuby if available (inherited from core)
if(LUPINE_HAS_MRUBY)
    target_link_libraries(lupine_engine_lib PUBLIC mruby)

    # On Windows, mRuby needs additional system libraries
    if(WIN32)
        target_link_libraries(lupine_engine_lib PUBLIC ws2_32)
    endif()

    message(STATUS "Engine: Linking mRuby support")
endif()

# Conditionally link GLEW if OpenGL is available
# Note: We use GLAD from core, but GLEW can coexist if needed
find_package(GLEW CONFIG)
if(GLEW_FOUND)
    target_link_libraries(lupine_engine_lib
        PUBLIC
            GLEW::GLEW
    )
    message(STATUS "Engine: Linking GLEW")
else()
    message(STATUS "Engine: GLEW not found, using GLAD from core")
endif()

# Graphics APIs are inherited from lupine_core
if(LUPINE_HAS_OPENGL)
    message(STATUS "Engine: OpenGL support inherited from core")
endif()

if(LUPINE_HAS_VULKAN)
    message(STATUS "Engine: Vulkan support inherited from core")
endif()

# Set target properties
set_target_properties(lupine_engine_lib PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME "lupine_engine_lib"
    DEBUG_POSTFIX "d"
)

# Create Python module
pybind11_add_module(lupine_engine
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EnginePython.cpp
)

# Link the Python module to the engine library
target_link_libraries(lupine_engine
    PRIVATE
        lupine_engine_lib
        # Need to explicitly link Assimp for Python modules due to C++ stdlib compatibility
        assimp::assimp
)

# Link mRuby to Python module if available
if(LUPINE_HAS_MRUBY)
    target_link_libraries(lupine_engine PRIVATE mruby)

    # On Windows, mRuby needs additional system libraries
    if(WIN32)
        target_link_libraries(lupine_engine PRIVATE ws2_32)
    endif()
endif()

# Create minimal test module to debug loading issues
pybind11_add_module(lupine_engine_test
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EnginePythonMinimal.cpp
)

# Python modules MUST use dynamic runtime to match Python's runtime
# The main CMakeLists.txt already sets this globally, so no need to override here

# Define preprocessor macro to indicate this is a Python module
target_compile_definitions(lupine_engine PRIVATE LUPINE_PYTHON_MODULE)

# Set Python module properties
set_target_properties(lupine_engine PROPERTIES
    OUTPUT_NAME "lupine_engine"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/python_modules"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/python_modules"
)

# Copy Python module to editor folder after build
add_custom_command(TARGET lupine_engine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/editor"
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:lupine_engine>
        "${CMAKE_SOURCE_DIR}/editor/"
    COMMENT "Copying lupine_engine Python module to editor folder"
)

# Copy test module to editor folder
add_custom_command(TARGET lupine_engine_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/editor"
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:lupine_engine_test>
        "${CMAKE_SOURCE_DIR}/editor/"
    COMMENT "Copying lupine_engine_test Python module to editor folder"
)

message(STATUS "Engine library configured")
message(STATUS "  Headers: ${ENGINE_HEADERS}")
message(STATUS "  Sources: ${ENGINE_SOURCES}")
message(STATUS "  Python module will be copied to: ${CMAKE_SOURCE_DIR}/editor/")
