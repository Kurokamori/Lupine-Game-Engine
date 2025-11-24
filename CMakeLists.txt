cmake_minimum_required(VERSION 3.20)

# ============================================================================
# VCPKG Configuration - MUST be set before project()
# ============================================================================

# Set VCPKG target triplet based on platform
if(NOT DEFINED VCPKG_TARGET_TRIPLET)
    if(WIN32)
        set(VCPKG_TARGET_TRIPLET "x64-windows-static-md" CACHE STRING "VCPKG target triplet")
    elseif(APPLE)
        set(VCPKG_TARGET_TRIPLET "x64-osx" CACHE STRING "VCPKG target triplet")
    elseif(UNIX)
        set(VCPKG_TARGET_TRIPLET "x64-linux" CACHE STRING "VCPKG target triplet")
    elseif(EMSCRIPTEN)
        set(VCPKG_TARGET_TRIPLET "wasm32-emscripten" CACHE STRING "VCPKG target triplet")
    else()
        message(WARNING "Unknown platform - VCPKG_TARGET_TRIPLET not set")
    endif()
endif()

# Set VCPKG toolchain file from environment variable
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VCPKG_ROOT})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "Vcpkg toolchain file")
        message(STATUS "Using VCPKG_ROOT from environment: $ENV{VCPKG_ROOT}")
    else()
        message(WARNING "VCPKG_ROOT environment variable not set - vcpkg integration disabled")
    endif()
endif()

# Enable vcpkg manifest mode (automatic dependency installation)
set(VCPKG_MANIFEST_MODE ON CACHE BOOL "Use vcpkg manifest mode")

# ============================================================================
# Project Definition
# ============================================================================

project(LupineEngine
    VERSION 0.1.0
    LANGUAGES CXX
)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDEs
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Platform detection
if(WIN32)
    set(LUPINE_PLATFORM "Windows")
    add_definitions(-DLUPINE_PLATFORM_WINDOWS)
elseif(APPLE)
    set(LUPINE_PLATFORM "macOS")
    add_definitions(-DLUPINE_PLATFORM_MACOS)
elseif(UNIX)
    set(LUPINE_PLATFORM "Linux")
    add_definitions(-DLUPINE_PLATFORM_LINUX)
elseif(EMSCRIPTEN)
    set(LUPINE_PLATFORM "Web")
    add_definitions(-DLUPINE_PLATFORM_WEB)
else()
    set(LUPINE_PLATFORM "Unknown")
    message(WARNING "Unknown platform detected")
endif()

message(STATUS "Building for platform: ${LUPINE_PLATFORM}")

# Force static linking
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
set(CMAKE_FIND_LIBRARY_SUFFIXES .a .lib)

# Runtime linking for MSVC
if(MSVC)
    # Use DYNAMIC runtime for Python modules to match Python's runtime
    # Python is built with /MD (MultiThreadedDLL), so we must use the same
    # Using static runtime (/MT) with Python causes heap corruption and crashes
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    add_compile_options(
        /W4                    # Warning level 4
        /WX-                   # Don't treat warnings as errors (can enable with /WX)
        /permissive-           # Standards conformance
        /MP                    # Multi-processor compilation
    )
    # Link against required Windows libraries for STL intrinsics
    link_libraries(
        $<$<CONFIG:Debug>:vcruntimed.lib>
        $<$<CONFIG:Release>:vcruntime.lib>
    )
else()
    # GCC/Clang flags
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -static-libgcc
        -static-libstdc++
    )
    if(NOT APPLE)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
    endif()
endif()

# Find Python for Python bindings
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
message(STATUS "Python3 found: ${Python3_VERSION}")
message(STATUS "Python3 include dirs: ${Python3_INCLUDE_DIRS}")
message(STATUS "Python3 libraries: ${Python3_LIBRARIES}")

# Set vcpkg_installed location for finding packages
set(VCPKG_INSTALLED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}" CACHE PATH "")

set(pybind11_DIR "${VCPKG_INSTALLED_DIR}/share/pybind11" CACHE PATH "")
set(polyclipping_DIR "${VCPKG_INSTALLED_DIR}/share/polyclipping" CACHE PATH "")
set(SDL2_DIR "${VCPKG_INSTALLED_DIR}/share/SDL2" CACHE PATH "")
set(nlohmann_json_DIR "${VCPKG_INSTALLED_DIR}/share/nlohmann_json" CACHE PATH "")
set(glad_DIR "${VCPKG_INSTALLED_DIR}/share/glad" CACHE PATH "")
set(fmt_DIR "${VCPKG_INSTALLED_DIR}/share/fmt" CACHE PATH "")
set(glfw3_DIR "${VCPKG_INSTALLED_DIR}/share/glfw3" CACHE PATH "")
set(glm_DIR "${VCPKG_INSTALLED_DIR}/share/glm" CACHE PATH "")
set(assimp_DIR "${VCPKG_INSTALLED_DIR}/share/assimp" CACHE PATH "")
set(zip_DIR "${VCPKG_INSTALLED_DIR}/share/zip-kuba--" CACHE PATH "")
set(ZLIB_DIR "${VCPKG_INSTALLED_DIR}/share/zlib" CACHE PATH "")
set(ZLIB_LIBRARY "${VCPKG_INSTALLED_DIR}/lib/zlib.lib" CACHE FILEPATH "")
set(ZLIB_INCLUDE_DIR "${VCPKG_INSTALLED_DIR}/include/zlib.h" CACHE PATH "")
set(unofficial-minizip_DIR "${VCPKG_INSTALLED_DIR}/share/unofficial-minizip" CACHE PATH "")
set(poly2tri_DIR "${VCPKG_INSTALLED_DIR}/share/poly2tri" CACHE PATH "")
set(pugixml_DIR "${VCPKG_INSTALLED_DIR}/share/pugixml" CACHE PATH "")
set(spdlog_DIR "${VCPKG_INSTALLED_DIR}/share/spdlog" CACHE PATH "")
set(RapidJSON_DIR "${VCPKG_INSTALLED_DIR}/share/rapidjson" CACHE PATH "")
set(sol2_DIR "${VCPKG_INSTALLED_DIR}/share/sol2" CACHE PATH "")
set(Lua_DIR "${VCPKG_INSTALLED_DIR}/share/lua" CACHE PATH "")

find_package(ZLIB REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(Python COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 CONFIG REQUIRED)
find_package(polyclipping CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(box2d CONFIG REQUIRED)
find_package(Bullet CONFIG REQUIRED)
find_package(Lua REQUIRED)
find_package(sol2 CONFIG REQUIRED)

# Add Lua include directories
if(LUA_FOUND)
    message(STATUS "Lua found: ${LUA_LIBRARIES}")
    message(STATUS "Lua include: ${LUA_INCLUDE_DIR}")
endif()

# ============================================================================
# mRuby Configuration
# ============================================================================
# Check if Ruby is available for building mRuby
find_program(RUBY_EXECUTABLE ruby)
find_program(RAKE_EXECUTABLE rake)

if(RUBY_EXECUTABLE AND RAKE_EXECUTABLE)
    message(STATUS "Ruby found: ${RUBY_EXECUTABLE}")
    message(STATUS "Rake found: ${RAKE_EXECUTABLE}")

    set(MRUBY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/mruby")
    set(MRUBY_BUILD_DIR "${MRUBY_ROOT}/build/host")
    set(MRUBY_INCLUDE_DIR "${MRUBY_ROOT}/include")

    # Determine build configuration and library extension based on platform and compiler
    if(WIN32 AND MSVC)
        set(MRUBY_CONFIG "msvc")
        set(MRUBY_LIBRARY "${MRUBY_BUILD_DIR}/lib/libmruby.lib")
    elseif(MINGW)
        set(MRUBY_CONFIG "default")
        set(MRUBY_LIBRARY "${MRUBY_BUILD_DIR}/lib/libmruby.a")
    else()
        set(MRUBY_CONFIG "default")
        set(MRUBY_LIBRARY "${MRUBY_BUILD_DIR}/lib/libmruby.a")
    endif()

    # Check if mRuby directory exists
    if(EXISTS "${MRUBY_ROOT}")
        message(STATUS "mRuby source found at: ${MRUBY_ROOT}")
        message(STATUS "mRuby will be built with '${MRUBY_CONFIG}' configuration")
        message(STATUS "mRuby library will be: ${MRUBY_LIBRARY}")

        # On Windows with MSVC, we need to run rake from a VS Developer environment
        if(WIN32 AND MSVC)
            # Try to find Visual Studio installation
            set(VS_DEVSHELL_SCRIPT "")

            # Common Visual Studio installation paths
            set(VS_POSSIBLE_PATHS
                "C:/Program Files/Microsoft Visual Studio/2022/Community"
                "C:/Program Files/Microsoft Visual Studio/2022/Professional"
                "C:/Program Files/Microsoft Visual Studio/2022/Enterprise"
                "C:/Program Files (x86)/Microsoft Visual Studio/2019/Community"
                "C:/Program Files (x86)/Microsoft Visual Studio/2019/Professional"
                "C:/Program Files (x86)/Microsoft Visual Studio/2019/Enterprise"
            )

            foreach(VS_PATH ${VS_POSSIBLE_PATHS})
                set(DEVSHELL_PATH "${VS_PATH}/Common7/Tools/Launch-VsDevShell.ps1")
                if(EXISTS "${DEVSHELL_PATH}")
                    set(VS_DEVSHELL_SCRIPT "${DEVSHELL_PATH}")
                    break()
                endif()
            endforeach()

            if(VS_DEVSHELL_SCRIPT)
                message(STATUS "Found Visual Studio Developer Shell: ${VS_DEVSHELL_SCRIPT}")
                # Build mRuby using PowerShell with VS Developer environment
                add_custom_command(
                    OUTPUT "${MRUBY_LIBRARY}"
                    COMMAND powershell -ExecutionPolicy Bypass -Command "& '${VS_DEVSHELL_SCRIPT}' -Arch amd64 -SkipAutomaticLocation; cd '${MRUBY_ROOT}'; $env:MRUBY_CONFIG='${MRUBY_CONFIG}'; & '${RAKE_EXECUTABLE}'"
                    WORKING_DIRECTORY "${MRUBY_ROOT}"
                    COMMENT "Building mRuby with ${MRUBY_CONFIG} configuration (MSVC)..."
                    VERBATIM
                )
            else()
                message(WARNING "Visual Studio Developer Shell not found at common locations. mRuby build may fail.")
                message(WARNING "If build fails, please build mRuby manually from a VS Developer Command Prompt:")
                message(WARNING "  cd ${MRUBY_ROOT}")
                message(WARNING "  $env:MRUBY_CONFIG='${MRUBY_CONFIG}'; rake")
                # Fallback to regular rake command (will likely fail without cl.exe in PATH)
                add_custom_command(
                    OUTPUT "${MRUBY_LIBRARY}"
                    COMMAND ${CMAKE_COMMAND} -E env "MRUBY_CONFIG=${MRUBY_CONFIG}" ${RAKE_EXECUTABLE}
                    WORKING_DIRECTORY "${MRUBY_ROOT}"
                    COMMENT "Building mRuby with ${MRUBY_CONFIG} configuration..."
                    VERBATIM
                )
            endif()
        else()
            # On non-Windows or MinGW, just run rake normally
            add_custom_command(
                OUTPUT "${MRUBY_LIBRARY}"
                COMMAND ${CMAKE_COMMAND} -E env "MRUBY_CONFIG=${MRUBY_CONFIG}" ${RAKE_EXECUTABLE}
                WORKING_DIRECTORY "${MRUBY_ROOT}"
                COMMENT "Building mRuby with ${MRUBY_CONFIG} configuration..."
                VERBATIM
            )
        endif()

        add_custom_target(build_mruby ALL DEPENDS "${MRUBY_LIBRARY}")

        # Create an imported library target for mRuby
        add_library(mruby STATIC IMPORTED GLOBAL)
        set_target_properties(mruby PROPERTIES
            IMPORTED_LOCATION "${MRUBY_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${MRUBY_INCLUDE_DIR}"
        )
        add_dependencies(mruby build_mruby)

        set(LUPINE_HAS_MRUBY TRUE)
        add_definitions(-DLUPINE_HAS_MRUBY)

        message(STATUS "mRuby support enabled")
        message(STATUS "  Include: ${MRUBY_INCLUDE_DIR}")
        message(STATUS "  Library: ${MRUBY_LIBRARY}")
    else()
        message(WARNING "mRuby source not found at ${MRUBY_ROOT} - mRuby support disabled")
        set(LUPINE_HAS_MRUBY FALSE)
    endif()
else()
    if(NOT RUBY_EXECUTABLE)
        message(WARNING "Ruby not found - mRuby support disabled. Install Ruby to enable mRuby scripting.")
    endif()
    if(NOT RAKE_EXECUTABLE)
        message(WARNING "Rake not found - mRuby support disabled. Install Rake (gem install rake) to enable mRuby scripting.")
    endif()
    set(LUPINE_HAS_MRUBY FALSE)
endif()

# Conditionally find graphics APIs
# OpenGL support
find_package(OpenGL)
if(OPENGL_FOUND)
    message(STATUS "OpenGL found: ${OPENGL_LIBRARIES}")
    find_package(glad CONFIG)
    if(glad_FOUND)
        message(STATUS "GLAD found - OpenGL support enabled")
        set(LUPINE_HAS_OPENGL TRUE)
        add_definitions(-DLUPINE_HAS_OPENGL)
    else()
        message(WARNING "GLAD not found - OpenGL support disabled")
        set(LUPINE_HAS_OPENGL FALSE)
    endif()
else()
    message(STATUS "OpenGL not found - OpenGL support disabled")
    set(LUPINE_HAS_OPENGL FALSE)
endif()

# Vulkan support
find_package(Vulkan)
if(Vulkan_FOUND)
    message(STATUS "Vulkan found: ${Vulkan_LIBRARIES}")
    message(STATUS "Vulkan version: ${Vulkan_VERSION}")
    message(STATUS "Vulkan include: ${Vulkan_INCLUDE_DIRS}")
    set(LUPINE_HAS_VULKAN TRUE)
    add_definitions(-DLUPINE_HAS_VULKAN)
else()
    message(STATUS "Vulkan not found - Vulkan support disabled")
    set(LUPINE_HAS_VULKAN FALSE)
endif()

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# For multi-config generators (Visual Studio, Xcode)
foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG} CONFIG_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR}/bin/${CONFIG})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR}/lib/${CONFIG})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR}/lib/${CONFIG})
endforeach()

# ============================================================================
# Embedded Python Standard Library Setup (Zip-based)
# ============================================================================
# For standalone C++ executables (runtime/console), we create a python3xx.zip
# file containing the standard library that can be embedded in the executable

if(Python3_FOUND)
    # Get Python stdlib location and version
    execute_process(
        COMMAND ${Python3_EXECUTABLE} -c "import sys; print(sys.prefix)"
        OUTPUT_VARIABLE PYTHON_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    
    execute_process(
        COMMAND ${Python3_EXECUTABLE} -c "import sys; print(f'python{sys.version_info.major}{sys.version_info.minor}')"
        OUTPUT_VARIABLE PYTHON_ZIP_NAME
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    
    set(PYTHON_STDLIB_SOURCE "${PYTHON_PREFIX}/Lib")
    message(STATUS "Python stdlib source: ${PYTHON_STDLIB_SOURCE}")
    message(STATUS "Python zip name: ${PYTHON_ZIP_NAME}.zip")
    
    # Create a Python script to zip the stdlib (excluding __pycache__ and tests)
    file(WRITE "${CMAKE_BINARY_DIR}/create_python_zip.py" "
import zipfile
import os
import sys
from pathlib import Path

stdlib_path = Path(sys.argv[1])
output_zip = sys.argv[2]

with zipfile.ZipFile(output_zip, 'w', zipfile.ZIP_DEFLATED) as zf:
    for root, dirs, files in os.walk(stdlib_path):
        # Skip __pycache__, test directories, and site-packages
        dirs[:] = [d for d in dirs if d not in ['__pycache__', 'test', 'tests', 'site-packages', 'ensurepip']]
        
        for file in files:
            if file.endswith(('.py', '.pyc')):
                file_path = Path(root) / file
                arcname = file_path.relative_to(stdlib_path)
                zf.write(file_path, arcname)

print(f'Created {output_zip}')
")
    
    # Create zip file for each configuration
    foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
        set(PYTHON_ZIP_DEST "${CMAKE_BINARY_DIR}/bin/${CONFIG}/${PYTHON_ZIP_NAME}.zip")
        
        add_custom_command(
            OUTPUT "${PYTHON_ZIP_DEST}"
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_BINARY_DIR}/create_python_zip.py" 
                    "${PYTHON_STDLIB_SOURCE}" "${PYTHON_ZIP_DEST}"
            COMMENT "Creating Python stdlib zip for ${CONFIG}: ${PYTHON_ZIP_NAME}.zip"
            VERBATIM
        )
        
        add_custom_target(python_stdlib_zip_${CONFIG} ALL DEPENDS "${PYTHON_ZIP_DEST}")
    endforeach()
    
    # Fallback for single-config generators
    set(PYTHON_ZIP_DEST "${CMAKE_BINARY_DIR}/bin/${PYTHON_ZIP_NAME}.zip")
    add_custom_command(
        OUTPUT "${PYTHON_ZIP_DEST}"
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_BINARY_DIR}/create_python_zip.py" 
                "${PYTHON_STDLIB_SOURCE}" "${PYTHON_ZIP_DEST}"
        COMMENT "Creating Python stdlib zip: ${PYTHON_ZIP_NAME}.zip"
        VERBATIM
    )
    add_custom_target(python_stdlib_zip ALL DEPENDS "${PYTHON_ZIP_DEST}")
    
    message(STATUS "Python standard library will be packaged as ${PYTHON_ZIP_NAME}.zip")
endif()

# Add subdirectories
add_subdirectory(core)
add_subdirectory(engine)
add_subdirectory(runtime)

# C API (optional)
option(LUPINE_BUILD_C_API "Build C API wrapper" ON)
if(LUPINE_BUILD_C_API)
    add_subdirectory(capi)
endif()

# Print build summary
message(STATUS "")
message(STATUS "=======================================================")
message(STATUS "            Lupine Engine Build Configuration         ")
message(STATUS "=======================================================")
message(STATUS "Platform:        ${LUPINE_PLATFORM}")
message(STATUS "Build Type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard:    ${CMAKE_CXX_STANDARD}")
message(STATUS "Static Linking:  ON")
message(STATUS "")
message(STATUS "vcpkg:")
message(STATUS "  Manifest Mode: ${VCPKG_MANIFEST_MODE}")
message(STATUS "  Triplet:       ${VCPKG_TARGET_TRIPLET}")
message(STATUS "  Toolchain:     ${CMAKE_TOOLCHAIN_FILE}")
message(STATUS "")
message(STATUS "Python:")
message(STATUS "  Version:       ${Python3_VERSION}")
message(STATUS "  Include Dirs:  ${Python3_INCLUDE_DIRS}")
message(STATUS "")
message(STATUS "Graphics APIs:")
message(STATUS "  OpenGL:        ${LUPINE_HAS_OPENGL}")
message(STATUS "  Vulkan:        ${LUPINE_HAS_VULKAN}")
message(STATUS "")
message(STATUS "Scripting:")
message(STATUS "  Lua:           ON")
message(STATUS "  mRuby:         ${LUPINE_HAS_MRUBY}")
message(STATUS "")
message(STATUS "C API:")
message(STATUS "  Enabled:       ${LUPINE_BUILD_C_API}")
message(STATUS "=======================================================")
message(STATUS "")
