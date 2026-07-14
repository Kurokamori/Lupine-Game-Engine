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
if(EMSCRIPTEN)
    # Emscripten uses interface libraries without :: namespace
    target_link_libraries(lupine_engine_lib
        PUBLIC
            lupine_core
            glm
            nlohmann_json
    )
else()
    target_link_libraries(lupine_engine_lib
        PUBLIC
            lupine_core
            glm::glm
            nlohmann_json::nlohmann_json
    )
endif()

# Link mRuby if available (inherited from core)
if(LUPINE_HAS_MRUBY)
    target_link_libraries(lupine_engine_lib PUBLIC mruby)

    # On Windows, mRuby needs additional system libraries
    if(WIN32)
        target_link_libraries(lupine_engine_lib PUBLIC ws2_32)
    endif()

    message(STATUS "Engine: Linking mRuby support")
endif()

# Conditionally link GLEW if OpenGL is available (not for Emscripten)
# Note: We use GLAD from core, but GLEW can coexist if needed
if(NOT EMSCRIPTEN)
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
endif()

# Graphics APIs are inherited from lupine_core
if(LUPINE_HAS_OPENGL)
    message(STATUS "Engine: OpenGL support inherited from core")
endif()

if(LUPINE_HAS_VULKAN)
    message(STATUS "Engine: Vulkan support inherited from core")
endif()

if(LUPINE_HAS_DIRECTX11)
    target_link_libraries(lupine_engine_lib
        PUBLIC
            d3d11.lib
            dxgi.lib
            d3dcompiler.lib
            dxguid.lib
    )
    message(STATUS "Engine: DirectX 11 support inherited from core")
endif()

if(LUPINE_HAS_DIRECTX12)
    target_link_libraries(lupine_engine_lib
        PUBLIC
            d3d12.lib
            dxgi.lib
            d3dcompiler.lib
            dxguid.lib
    )
    message(STATUS "Engine: DirectX 12 support inherited from core")
endif()

# Set target properties
set_target_properties(lupine_engine_lib PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME "lupine_engine_lib"
    DEBUG_POSTFIX "d"
)

# Create Python module (not for Emscripten - uses MicroPython instead)
if(NOT EMSCRIPTEN)

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

# Link DirectX11 to Python module if available
if(LUPINE_HAS_DIRECTX11)
    target_link_libraries(lupine_engine
        PRIVATE
            d3d11.lib
            dxgi.lib
            d3dcompiler.lib
            dxguid.lib
    )
endif()

# Link DirectX12 to Python module if available
if(LUPINE_HAS_DIRECTX12)
    target_link_libraries(lupine_engine
        PRIVATE
            d3d12.lib
            dxgi.lib
            d3dcompiler.lib
            dxguid.lib
    )
endif()

# Link mRuby to Python module if available
if(LUPINE_HAS_MRUBY)
    target_link_libraries(lupine_engine PRIVATE mruby)

    # On Windows, mRuby needs additional system libraries
    if(WIN32)
        target_link_libraries(lupine_engine PRIVATE ws2_32)
    endif()
endif()

# Python modules MUST use dynamic runtime to match Python's runtime
# The main CMakeLists.txt already sets this globally, so no need to override here

# Define preprocessor macros for Python module
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

# Copy DXC runtime DLLs (dxcompiler.dll, dxil.dll) to editor folder for runtime
# shader compilation.  DX12 shaders are compiled from HLSL source at runtime
# because offline-compiled DXIL bytecode may be incompatible with the target GPU
# driver's DXIL validator version.
if(WIN32)
    set(_DXC_DLL_NAMES dxcompiler.dll dxil.dll)
    foreach(_DXC_DLL ${_DXC_DLL_NAMES})
        # Search vcpkg installed tools and bin directories
        foreach(_VCPKG_SUBDIR tools/directx-dxc bin)
            set(_DXC_PATH "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/${_VCPKG_SUBDIR}/${_DXC_DLL}")
            if(EXISTS "${_DXC_PATH}")
                add_custom_command(TARGET lupine_engine POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_DXC_PATH}"
                        "${CMAKE_SOURCE_DIR}/editor/${_DXC_DLL}"
                    COMMENT "Copying ${_DXC_DLL} to editor folder"
                )
                message(STATUS "  DXC: found ${_DXC_DLL} at ${_DXC_PATH}")
                break()
            endif()
        endforeach()
    endforeach()
endif()

message(STATUS "  Python module will be copied to: ${CMAKE_SOURCE_DIR}/editor/")

endif() # NOT EMSCRIPTEN

message(STATUS "Engine library configured")
message(STATUS "  Headers: ${ENGINE_HEADERS}")
message(STATUS "  Sources: ${ENGINE_SOURCES}")
