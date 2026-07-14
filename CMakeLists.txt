cmake_minimum_required(VERSION 3.20)

# ============================================================================
# VCPKG Configuration - MUST be set before project()
# ============================================================================

# Set VCPKG target triplet based on platform
if(NOT DEFINED VCPKG_TARGET_TRIPLET)
    if(WIN32)
        set(VCPKG_TARGET_TRIPLET "x64-windows-static-md" CACHE STRING "VCPKG target triplet")

    elseif(APPLE)
        # Prefer an explicit CMAKE_OSX_ARCHITECTURES if set by the user or preset
        if(DEFINED CMAKE_OSX_ARCHITECTURES AND NOT CMAKE_OSX_ARCHITECTURES STREQUAL "")
            # CMAKE_OSX_ARCHITECTURES can be "arm64", "x86_64", or "arm64;x86_64"
            string(FIND "${CMAKE_OSX_ARCHITECTURES}" "arm64" _has_arm)
            string(FIND "${CMAKE_OSX_ARCHITECTURES}" "x86_64" _has_x86)
            if(_has_arm GREATER -1 AND _has_x86 GREATER -1)
                # Universal requested — default to arm64 (users can override)
                set(chosen_arch "arm64")
            elseif(_has_arm GREATER -1)
                set(chosen_arch "arm64")
            elseif(_has_x86 GREATER -1)
                set(chosen_arch "x86_64")
            endif()
        endif()

        # If not decided yet, prefer CMAKE_HOST_SYSTEM_PROCESSOR (reliable for build host)
        if(NOT DEFINED chosen_arch OR chosen_arch STREQUAL "")
            if(DEFINED CMAKE_HOST_SYSTEM_PROCESSOR AND NOT CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "")
                if(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "arm64" OR CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "aarch64")
                    set(chosen_arch "arm64")
                elseif(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "x86_64" OR CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "AMD64")
                    set(chosen_arch "x86_64")
                endif()
            endif()
        endif()

        # Fallback: check environment vars (useful for CI or Rosetta shells)
        if(NOT DEFINED chosen_arch OR chosen_arch STREQUAL "")
            if(DEFINED ENV{PROCESSOR_ARCHITECTURE})
                if("$ENV{PROCESSOR_ARCHITECTURE}" STREQUAL "ARM64")
                    set(chosen_arch "arm64")
                elseif("$ENV{PROCESSOR_ARCHITECTURE}" STREQUAL "x86")
                    set(chosen_arch "x86_64")
                endif()
            endif()
        endif()

        # Final fallback: run uname -m
        if(NOT DEFINED chosen_arch OR chosen_arch STREQUAL "")
            execute_process(COMMAND uname -m
                OUTPUT_VARIABLE _uname_m OUTPUT_STRIP_TRAILING_WHITESPACE)
            if(_uname_m STREQUAL "arm64" OR _uname_m STREQUAL "aarch64")
                set(chosen_arch "arm64")
            elseif(_uname_m STREQUAL "x86_64")
                set(chosen_arch "x86_64")
            endif()
        endif()

        # Decide triplet
        if(chosen_arch STREQUAL "arm64")
            set(VCPKG_TARGET_TRIPLET "arm64-osx" CACHE STRING "VCPKG target triplet")
            # optional: ensure CMake is configured for arm64 builds unless user overrides
            if(NOT DEFINED CMAKE_OSX_ARCHITECTURES OR CMAKE_OSX_ARCHITECTURES STREQUAL "")
                set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "OSX architectures" FORCE)
            endif()
        elseif(chosen_arch STREQUAL "x86_64")
            set(VCPKG_TARGET_TRIPLET "x64-osx" CACHE STRING "VCPKG target triplet")
            if(NOT DEFINED CMAKE_OSX_ARCHITECTURES OR CMAKE_OSX_ARCHITECTURES STREQUAL "")
                set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "OSX architectures" FORCE)
            endif()
        else()
            # If we couldn't detect, default to arm64 (safer for modern macs) and warn
            set(VCPKG_TARGET_TRIPLET "arm64-osx" CACHE STRING "VCPKG target triplet")
            message(WARNING "Could not reliably detect macOS CPU architecture; defaulting to arm64. Users can override with -DVCPKG_TARGET_TRIPLET or set CMAKE_OSX_ARCHITECTURES.")
        endif()

        message(STATUS "Auto-detected macOS arch: ${chosen_arch}; VCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET}")

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
    LANGUAGES CXX C
)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDEs
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Platform detection
# NOTE: EMSCRIPTEN must be checked before UNIX because Emscripten also defines UNIX
if(EMSCRIPTEN)
    set(LUPINE_PLATFORM "Web")
    add_definitions(-DLUPINE_PLATFORM_WEB)
    add_definitions(-DLUPINE_WEBGL)
elseif(WIN32)
    set(LUPINE_PLATFORM "Windows")
    add_definitions(-DLUPINE_PLATFORM_WINDOWS)
elseif(APPLE)
    set(LUPINE_PLATFORM "macOS")
    add_definitions(-DLUPINE_PLATFORM_MACOS)
elseif(UNIX)
    set(LUPINE_PLATFORM "Linux")
    add_definitions(-DLUPINE_PLATFORM_LINUX)
else()
    set(LUPINE_PLATFORM "Unknown")
    message(WARNING "Unknown platform detected")
endif()

message(STATUS "Building for platform: ${LUPINE_PLATFORM}")

# Force static linking
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
set(CMAKE_FIND_LIBRARY_SUFFIXES .a .lib)

# Emscripten-specific settings
if(EMSCRIPTEN)
    # Emscripten uses its own toolchain, don't apply normal static linking
    message(STATUS "Configuring for Emscripten/WebAssembly build")

    # Emscripten compile flags
    add_compile_options(
        -Wall
        -Wextra
        -fexceptions  # Enable C++ exceptions
    )

    # Common Emscripten link flags for all targets
    add_link_options(
        "-sALLOW_MEMORY_GROWTH=1"
        "-sSTACK_SIZE=1048576"  # 1MB stack
        "-sASYNCIFY=0"  # Disable asyncify (we use proper main loop)
    )

    # Skip Python for web builds (use MicroPython/Lua/mRuby instead)
    set(BUILD_PYTHON_MODULES OFF CACHE BOOL "Disable Python for web" FORCE)

# Runtime linking for MSVC
elseif(MSVC)
    # Use DYNAMIC runtime for Python modules to match Python's runtime
    # Python is built with /MD (MultiThreadedDLL), so we must use the same
    # Using static runtime (/MT) with Python causes heap corruption and crashes
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    add_compile_options(
        /W4                    # Warning level 4
        /WX-                   # Don't treat warnings as errors (can enable with /WX)
        /permissive-           # Standards conformance
        /MP                    # Multi-processor compilation
        /bigobj                # Allow >65279 sections per object (scripting binding TUs exceed the default)
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

# macOS: Create Iconv::Iconv target if not found (uses system iconv)
if(APPLE)
    find_package(Iconv QUIET)
    if(NOT TARGET Iconv::Iconv)
        add_library(Iconv::Iconv INTERFACE IMPORTED)
        set_target_properties(Iconv::Iconv PROPERTIES
            INTERFACE_LINK_LIBRARIES "-liconv"
        )
    endif()
endif()

# Find Python for Python bindings (not needed for Emscripten)
if(NOT EMSCRIPTEN)
    find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
    message(STATUS "Python3 found: ${Python3_VERSION}")
    message(STATUS "Python3 include dirs: ${Python3_INCLUDE_DIRS}")
    message(STATUS "Python3 libraries: ${Python3_LIBRARIES}")
else()
    # For Emscripten, we still need Python interpreter for build scripts
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_FOUND)
        message(STATUS "Python3 interpreter found for build scripts: ${Python3_VERSION}")
    endif()
endif()

# ============================================================================
# Dependency Configuration
# ============================================================================

if(EMSCRIPTEN)
    # For Emscripten, we use Emscripten ports and header-only libraries
    message(STATUS "Configuring dependencies for Emscripten...")

    # Emscripten ports - these are fetched and built automatically
    # We add them as compile/link options later in core/CMakeLists.txt
    set(EMSCRIPTEN_USE_PORTS ON)

    # Header-only libraries - download if not present
    # GLM
    set(GLM_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/glm")
    if(NOT EXISTS "${GLM_INCLUDE_DIR}/glm/glm.hpp")
        message(STATUS "Fetching GLM...")
        include(FetchContent)
        FetchContent_Declare(
            glm
            GIT_REPOSITORY https://github.com/g-truc/glm.git
            GIT_TAG 1.0.1
            SOURCE_DIR "${GLM_INCLUDE_DIR}"
        )
        FetchContent_MakeAvailable(glm)
    endif()

    # nlohmann_json - header only
    set(JSON_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/json")
    if(NOT EXISTS "${JSON_INCLUDE_DIR}/nlohmann/json.hpp")
        message(STATUS "Fetching nlohmann_json...")
        include(FetchContent)
        FetchContent_Declare(
            nlohmann_json
            GIT_REPOSITORY https://github.com/nlohmann/json.git
            GIT_TAG v3.11.3
            SOURCE_DIR "${JSON_INCLUDE_DIR}"
        )
        FetchContent_MakeAvailable(nlohmann_json)
    endif()

    # sol2 - header only (Lua binding) - match vcpkg version 3.5.0
    set(SOL2_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/sol2")
    if(NOT EXISTS "${SOL2_INCLUDE_DIR}/sol/sol.hpp")
        message(STATUS "Fetching sol2...")
        include(FetchContent)
        FetchContent_Declare(
            sol2
            GIT_REPOSITORY https://github.com/ThePhD/sol2.git
            GIT_TAG v3.5.0
            SOURCE_DIR "${SOL2_INCLUDE_DIR}"
        )
        FetchContent_MakeAvailable(sol2)
    endif()

    # Create interface targets for Emscripten (only if not created by FetchContent)
    if(NOT TARGET glm)
        add_library(glm INTERFACE)
        target_include_directories(glm INTERFACE "${GLM_INCLUDE_DIR}")
    endif()

    if(NOT TARGET nlohmann_json)
        add_library(nlohmann_json INTERFACE)
        target_include_directories(nlohmann_json INTERFACE "${JSON_INCLUDE_DIR}/include")
    endif()

    # spdlog - fetch headers for Emscripten (header-only mode)
    if(NOT TARGET spdlog)
        FetchContent_Declare(
            spdlog
            GIT_REPOSITORY https://github.com/gabime/spdlog.git
            GIT_TAG v1.12.0
        )
        FetchContent_MakeAvailable(spdlog)
        # spdlog creates its own target, but ensure header-only mode
        target_compile_definitions(spdlog INTERFACE SPDLOG_HEADER_ONLY)
    endif()
    # Use console output for Emscripten instead of file logging

    # Lua is built via Emscripten port
    # Box2D and Bullet need special handling

    message(STATUS "Emscripten dependencies configured")

else()
    # Native build - use vcpkg
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

    # Force release Python library for pybind11 modules
    # Most users don't have the debug Python DLL (python3XX_d.dll) installed
    if(WIN32 AND (MSVC OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
        # Get Python version for library naming (e.g., "312" for Python 3.12)
        set(PYTHON_VERSION_NO_DOT "${Python_VERSION_MAJOR}${Python_VERSION_MINOR}")

        # vcpkg puts release libs in lib/, debug libs in debug/lib/
        set(PYTHON_RELEASE_LIB "${VCPKG_INSTALLED_DIR}/lib/python${PYTHON_VERSION_NO_DOT}.lib")

        if(EXISTS "${PYTHON_RELEASE_LIB}")
            message(STATUS "Forcing release Python library: ${PYTHON_RELEASE_LIB}")

            # Modify Python::Python imported target to use release library for all configurations
            # This ensures pybind11 always links against python3XX.lib, not python3XX_d.lib
            if(TARGET Python::Python)
                # Get the release library path and use it for debug too
                set_target_properties(Python::Python PROPERTIES
                    IMPORTED_LOCATION "${PYTHON_RELEASE_LIB}"
                    IMPORTED_LOCATION_DEBUG "${PYTHON_RELEASE_LIB}"
                    IMPORTED_LOCATION_RELEASE "${PYTHON_RELEASE_LIB}"
                    IMPORTED_LOCATION_RELWITHDEBINFO "${PYTHON_RELEASE_LIB}"
                    IMPORTED_LOCATION_MINSIZEREL "${PYTHON_RELEASE_LIB}"
                )
                message(STATUS "Python::Python target modified to always use release library")
            endif()

            # Also modify Python3::Python if it exists (some CMake versions use this)
            if(TARGET Python3::Python)
                set_target_properties(Python3::Python PROPERTIES
                    IMPORTED_LOCATION "${PYTHON_RELEASE_LIB}"
                    IMPORTED_LOCATION_DEBUG "${PYTHON_RELEASE_LIB}"
                    IMPORTED_LOCATION_RELEASE "${PYTHON_RELEASE_LIB}"
                    IMPORTED_LOCATION_RELWITHDEBINFO "${PYTHON_RELEASE_LIB}"
                    IMPORTED_LOCATION_MINSIZEREL "${PYTHON_RELEASE_LIB}"
                )
            endif()

            # Create interface target for explicit release Python linking
            add_library(Python3_Release INTERFACE IMPORTED)
            set_target_properties(Python3_Release PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${Python_INCLUDE_DIRS}"
                INTERFACE_LINK_LIBRARIES "${PYTHON_RELEASE_LIB}"
            )
        else()
            message(WARNING "Release Python library not found at ${PYTHON_RELEASE_LIB}")
            message(WARNING "pybind11 modules may require python${PYTHON_VERSION_NO_DOT}_d.dll")
        endif()
    endif()

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
    set(MRUBY_INCLUDE_DIR "${MRUBY_ROOT}/include")

    # Determine build configuration based on platform
    if(EMSCRIPTEN)
        # Emscripten cross-compilation uses Lupine's custom config with HAL support
        set(MRUBY_CONFIG "lupine-emscripten")
        set(MRUBY_BUILD_DIR "${MRUBY_ROOT}/build/lupine-emscripten")
        set(MRUBY_LIBRARY "${MRUBY_BUILD_DIR}/lib/libmruby.a")
    elseif(WIN32 AND (MSVC OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
        # Use MSVC config for both MSVC and Clang on Windows (MSVC-compatible ABI)
        set(MRUBY_CONFIG "msvc")
        set(MRUBY_BUILD_DIR "${MRUBY_ROOT}/build/host")
        set(MRUBY_LIBRARY "${MRUBY_BUILD_DIR}/lib/libmruby.lib")
    elseif(MINGW)
        set(MRUBY_CONFIG "default")
        set(MRUBY_BUILD_DIR "${MRUBY_ROOT}/build/host")
        set(MRUBY_LIBRARY "${MRUBY_BUILD_DIR}/lib/libmruby.a")
    else()
        set(MRUBY_CONFIG "default")
        set(MRUBY_BUILD_DIR "${MRUBY_ROOT}/build/host")
        set(MRUBY_LIBRARY "${MRUBY_BUILD_DIR}/lib/libmruby.a")
    endif()

    # mRuby is a git submodule, so Lupine's custom build_config files (msvc.rb,
    # lupine-emscripten.rb) cannot be committed inside it. Keep the canonical
    # copies under cmake/mruby/build_config/ and drop them into the submodule at
    # configure time whenever they are missing (e.g. after a fresh clone or a
    # `git submodule update` that reset the working tree). The 'default' config
    # ships with mRuby, so it is never overwritten here.
    set(MRUBY_CONFIG_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/mruby/build_config/${MRUBY_CONFIG}.rb")
    set(MRUBY_CONFIG_DEST "${MRUBY_ROOT}/build_config/${MRUBY_CONFIG}.rb")
    if(EXISTS "${MRUBY_CONFIG_SOURCE}" AND NOT EXISTS "${MRUBY_CONFIG_DEST}")
        message(STATUS "Installing Lupine mRuby build config into submodule: ${MRUBY_CONFIG}.rb")
        configure_file("${MRUBY_CONFIG_SOURCE}" "${MRUBY_CONFIG_DEST}" COPYONLY)
    endif()

    # Check if mRuby directory exists
    if(EXISTS "${MRUBY_ROOT}")
        message(STATUS "mRuby source found at: ${MRUBY_ROOT}")
        message(STATUS "mRuby will be built with '${MRUBY_CONFIG}' configuration")
        message(STATUS "mRuby library will be: ${MRUBY_LIBRARY}")

        if(EMSCRIPTEN)
            # For Emscripten, mRuby cross-compilation requires two steps:
            # 1. Build host tools (mrbc) with native compiler - needed to compile Ruby to bytecode
            # 2. Build cross library with emcc - the actual WebAssembly library
            #
            # This is because emmake wraps ALL compiler calls with emscripten,
            # but mrbc must run on the build machine, not in the browser.

            # Find emmake
            find_program(EMMAKE_EXECUTABLE emmake NAMES emmake emmake.bat)
            if(NOT EMMAKE_EXECUTABLE AND DEFINED ENV{EMSDK})
                if(CMAKE_HOST_WIN32)
                    set(EMMAKE_EXECUTABLE "$ENV{EMSDK}/upstream/emscripten/emmake.bat")
                else()
                    set(EMMAKE_EXECUTABLE "$ENV{EMSDK}/upstream/emscripten/emmake")
                endif()
            endif()

            if(EXISTS "${EMMAKE_EXECUTABLE}" OR EMMAKE_EXECUTABLE)
                message(STATUS "Found emmake: ${EMMAKE_EXECUTABLE}")

                # Create a PowerShell/Bash script for the two-step build
                if(CMAKE_HOST_WIN32)
                    set(MRUBY_BUILD_SCRIPT "${CMAKE_BINARY_DIR}/build_mruby_emscripten.ps1")
                    file(WRITE "${MRUBY_BUILD_SCRIPT}" "
# mRuby two-step build for Emscripten (Windows)
$ErrorActionPreference = 'Stop'
Set-Location '${MRUBY_ROOT}'

# Check if library already exists (may have been built by build-web.ps1)
$LibPath = '${MRUBY_LIBRARY}'
if (Test-Path $LibPath) {
    Write-Host 'mRuby library already exists, skipping build.' -ForegroundColor Green
    exit 0
}

Write-Host '=== Step 1: Building mRuby host tools (mrbc) with native compiler ===' -ForegroundColor Cyan
$env:MRUBY_CONFIG = '${MRUBY_CONFIG}'
& '${RAKE_EXECUTABLE}'
if ($LASTEXITCODE -ne 0) {
    Write-Host 'Host build failed!' -ForegroundColor Red
    exit 1
}
Write-Host 'Host build complete.' -ForegroundColor Green

Write-Host '=== Step 2: Building mRuby for WebAssembly with Emscripten ===' -ForegroundColor Cyan
$env:MRUBY_CONFIG = '${MRUBY_CONFIG}'
& '${EMMAKE_EXECUTABLE}' '${RAKE_EXECUTABLE}'
if ($LASTEXITCODE -ne 0) {
    Write-Host 'Cross build failed!' -ForegroundColor Red
    exit 1
}
Write-Host 'mRuby WebAssembly build complete.' -ForegroundColor Green
")
                    add_custom_command(
                        OUTPUT "${MRUBY_LIBRARY}"
                        COMMAND powershell -ExecutionPolicy Bypass -File "${MRUBY_BUILD_SCRIPT}"
                        WORKING_DIRECTORY "${MRUBY_ROOT}"
                        COMMENT "Building mRuby with ${MRUBY_CONFIG} configuration for WebAssembly..."
                        VERBATIM
                    )
                else()
                    # Unix/macOS shell script
                    set(MRUBY_BUILD_SCRIPT "${CMAKE_BINARY_DIR}/build_mruby_emscripten.sh")
                    file(WRITE "${MRUBY_BUILD_SCRIPT}" "#!/bin/bash
set -e
cd '${MRUBY_ROOT}'

# Check if library already exists (may have been built by build-web.sh)
if [ -f '${MRUBY_LIBRARY}' ]; then
    echo 'mRuby library already exists, skipping build.'
    exit 0
fi

echo '=== Step 1: Building mRuby host tools (mrbc) with native compiler ==='
MRUBY_CONFIG='${MRUBY_CONFIG}' '${RAKE_EXECUTABLE}'
echo 'Host build complete.'

echo '=== Step 2: Building mRuby for WebAssembly with Emscripten ==='
MRUBY_CONFIG='${MRUBY_CONFIG}' '${EMMAKE_EXECUTABLE}' '${RAKE_EXECUTABLE}'
echo 'mRuby WebAssembly build complete.'
")
                    # Make the script executable
                    file(CHMOD "${MRUBY_BUILD_SCRIPT}" PERMISSIONS
                        OWNER_READ OWNER_WRITE OWNER_EXECUTE
                        GROUP_READ GROUP_EXECUTE
                        WORLD_READ WORLD_EXECUTE)
                    add_custom_command(
                        OUTPUT "${MRUBY_LIBRARY}"
                        COMMAND bash "${MRUBY_BUILD_SCRIPT}"
                        WORKING_DIRECTORY "${MRUBY_ROOT}"
                        COMMENT "Building mRuby with ${MRUBY_CONFIG} configuration for WebAssembly..."
                        VERBATIM
                    )
                endif()
            else()
                message(FATAL_ERROR "emmake not found - cannot build mRuby for Emscripten!\n"
                    "Ensure Emscripten SDK is properly activated:\n"
                    "  emsdk activate latest\n"
                    "  emsdk_env (or emsdk_env.ps1 on Windows)")
            endif()
        elseif(EXISTS "${MRUBY_LIBRARY}")
            # mRuby library already exists - skip build
            message(STATUS "mRuby library already exists at ${MRUBY_LIBRARY}, skipping build")
            # Create a dummy custom command that just touches the file (no-op)
            add_custom_command(
                OUTPUT "${MRUBY_LIBRARY}"
                COMMAND ${CMAKE_COMMAND} -E echo "mRuby library already built"
                COMMENT "mRuby library already exists"
            )
        elseif(WIN32 AND (MSVC OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
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
if(EMSCRIPTEN)
    # For Emscripten, we only use WebGL (handled in core CMakeLists.txt)
    message(STATUS "WebGL 2.0 support enabled for Emscripten")
    set(LUPINE_HAS_WEBGL TRUE)
    set(LUPINE_HAS_OPENGL FALSE)
    set(LUPINE_HAS_VULKAN FALSE)
    set(LUPINE_HAS_DIRECTX11 FALSE)
    set(LUPINE_HAS_DIRECTX12 FALSE)
    set(LUPINE_HAS_METAL FALSE)
else()
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

    # DirectX 11 support (Windows only)
    if(WIN32)
        message(STATUS "DirectX 11 support enabled (Windows platform detected)")
        set(LUPINE_HAS_DIRECTX11 TRUE)
        add_definitions(-DLUPINE_HAS_DIRECTX11)
    else()
        message(STATUS "DirectX 11 not available on this platform")
        set(LUPINE_HAS_DIRECTX11 FALSE)
    endif()

    # DirectX 12 support (Windows only)
    if(WIN32)
        message(STATUS "DirectX 12 support enabled (Windows platform detected)")
        set(LUPINE_HAS_DIRECTX12 TRUE)
        add_definitions(-DLUPINE_HAS_DIRECTX12)
    else()
        message(STATUS "DirectX 12 not available on this platform")
        set(LUPINE_HAS_DIRECTX12 FALSE)
    endif()

    # Metal support (macOS/iOS only)
    if(APPLE)
        message(STATUS "Metal support enabled (Apple platform detected)")
        set(LUPINE_HAS_METAL TRUE)
        add_definitions(-DLUPINE_HAS_METAL)
    else()
        message(STATUS "Metal not available on this platform")
        set(LUPINE_HAS_METAL FALSE)
    endif()
endif() # NOT EMSCRIPTEN graphics APIs

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
# Skip for Emscripten (uses MicroPython/Lua instead)

if(Python3_FOUND AND NOT EMSCRIPTEN)
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

# Include MicroPython configuration
include(${CMAKE_SOURCE_DIR}/cmake/MicroPython.cmake)

# Include networking configuration (defines LUPINE_HAS_NETWORKING + transport deps)
include(${CMAKE_SOURCE_DIR}/cmake/Networking.cmake)

# Add subdirectories
add_subdirectory(core)
add_subdirectory(engine)
add_subdirectory(runtime)

# Include export templates configuration
include(${CMAKE_SOURCE_DIR}/cmake/ExportTemplates.cmake)

# C API (optional, not for Emscripten - used for Crystal bindings)
option(LUPINE_BUILD_C_API "Build C API wrapper" ON)
if(LUPINE_BUILD_C_API AND NOT EMSCRIPTEN)
    add_subdirectory(capi)
endif()

# Native C++ Extension SDK ("LupineExtension", a GDExtension equivalent). Builds
# the author SDK, install rules and bundled examples. The host-side loader lives
# in lupine_core, so the runtime/console/editor get extension loading regardless
# of this option; this only controls the consume-side SDK + examples.
option(LUPINE_BUILD_EXTENSION_SDK "Build the native C++ extension SDK and examples" ON)
if(LUPINE_BUILD_EXTENSION_SDK AND NOT EMSCRIPTEN)
    add_subdirectory(extension)
endif()

# Runtime C API (optional superset of the C API with window + main loop). Off by
# default because it links SDL2 and the graphics backends; enable for language
# bindings that need to open a window and run the engine, not just edit scene data.
option(LUPINE_BUILD_RUNTIME_C_API "Build the runtime (window + main loop) C API wrapper" OFF)
if(LUPINE_BUILD_RUNTIME_C_API AND NOT EMSCRIPTEN)
    add_subdirectory(runtime_capi)
endif()

# Console test suite (optional, not for Emscripten - interactive desktop test app)
if(NOT EMSCRIPTEN)
    add_subdirectory(console)
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
message(STATUS "  DirectX 11:    ${LUPINE_HAS_DIRECTX11}")
message(STATUS "  DirectX 12:    ${LUPINE_HAS_DIRECTX12}")
message(STATUS "  Metal:         ${LUPINE_HAS_METAL}")
message(STATUS "")
message(STATUS "Scripting:")
message(STATUS "  Lua:           ON")
message(STATUS "  mRuby:         ${LUPINE_HAS_MRUBY}")
message(STATUS "  MicroPython:   ${LUPINE_HAS_MICROPYTHON}")
message(STATUS "")
message(STATUS "Networking:")
message(STATUS "  Enabled:       ${LUPINE_HAS_NETWORKING}")
message(STATUS "")
message(STATUS "C API:")
message(STATUS "  Enabled:       ${LUPINE_BUILD_C_API}")
message(STATUS "=======================================================")
message(STATUS "")
