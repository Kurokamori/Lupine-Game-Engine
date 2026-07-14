# Lupine Core Library

# Collect unified .lsh shader sources as dependencies for regeneration
file(GLOB LSH_SHADER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/shaders/Unified/*.lsh")

# Generate shaders header from unified .lsh shader files via transpiler
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/include/lupine/rendering/GeneratedShaders.hpp"
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/shaders/compile_shaders.py"
        --use-unified --lsh-dir "${CMAKE_CURRENT_SOURCE_DIR}/shaders/Unified"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/compile_shaders.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/lupine_shader_transpiler.py"
        ${LSH_SHADER_SOURCES}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
    COMMENT "Generating shader header from unified .lsh sources..."
    VERBATIM
)

# Create a target for the generated shader header
add_custom_target(generate_shaders
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/lupine/rendering/GeneratedShaders.hpp"
)

# Collect all source files using GLOB
# CONFIGURE_DEPENDS re-globs at build time so newly added source files are
# picked up without a manual reconfigure.
file(GLOB_RECURSE CORE_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/lupine/core/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/lupine/core/*.h"
)

file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

# For Emscripten, exclude desktop-only rendering backends (OpenGL, Vulkan, DirectX)
# WebGL backend will be used instead
if(EMSCRIPTEN)
    # Exclude OpenGL backend (uses desktop GL functions not in WebGL)
    list(FILTER CORE_SOURCES EXCLUDE REGEX ".*/backends/opengl/.*")
    # Exclude Vulkan backend (not available in browsers)
    list(FILTER CORE_SOURCES EXCLUDE REGEX ".*/backends/vulkan/.*")
    # Exclude DirectX backends (Windows only)
    list(FILTER CORE_SOURCES EXCLUDE REGEX ".*/backends/directx11/.*")
    list(FILTER CORE_SOURCES EXCLUDE REGEX ".*/backends/directx12/.*")
    # Exclude Metal backend (Apple only)
    list(FILTER CORE_SOURCES EXCLUDE REGEX ".*/backends/metal/.*")
    # Exclude platform-specific debug renderers
    list(FILTER CORE_SOURCES EXCLUDE REGEX ".*/debug/DebugRendererDX.*")
    list(FILTER CORE_SOURCES EXCLUDE REGEX ".*/debug/DebugRendererVulkan.*")
    list(FILTER CORE_SOURCES EXCLUDE REGEX ".*/debug/DebugRendererOpenGL.*")
    list(FILTER CORE_SOURCES EXCLUDE REGEX ".*/debug/DebugRendererMetal.*")
    message(STATUS "Emscripten: Excluding desktop rendering backends, using WebGL only")
endif()

# Networking: pick the platform-specific WebSocket transport implementation.
# The browser build uses emscripten/websocket.h; the native build uses the
# ixwebsocket-backed implementation. Each excludes the other's source file.
if(EMSCRIPTEN)
    list(FILTER CORE_SOURCES EXCLUDE REGEX ".*WebSocketTransport_Native.*")
else()
    list(FILTER CORE_SOURCES EXCLUDE REGEX ".*WebSocketTransport_Emscripten.*")
endif()

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
        # Native extension C ABI header (lupine/lupine_extension_interface.h),
        # consumed by ExtensionManager. Always available regardless of whether the
        # extension SDK build option is enabled.
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/extension/include>
)

# Link dependencies
# NOTE: Python library is NOT linked here - only headers for compilation
# Engine and runtime targets that create Python modules link Python directly
if(EMSCRIPTEN)
    # Emscripten: Use ports and interface libraries
    target_link_libraries(lupine_core
        PUBLIC
            spdlog           # Interface library from root CMakeLists
            glm              # Interface library from root CMakeLists
            nlohmann_json    # Interface library from root CMakeLists
    )

    # Emscripten ports for native libraries
    target_compile_options(lupine_core PUBLIC
        "-sUSE_ZLIB=1"
        "-sUSE_SDL=2"
    )
    target_link_options(lupine_core PUBLIC
        "-sUSE_ZLIB=1"
        "-sUSE_SDL=2"
        "-sUSE_BULLET=1"
    )

    # Lua for Emscripten - build from source (no USE_LUA port available)
    include(FetchContent)
    FetchContent_Declare(
        lua_emscripten
        URL "https://www.lua.org/ftp/lua-5.4.7.tar.gz"
        URL_HASH SHA256=9fbf5e28ef86c69858f6d3d34eccc32e911c1a28b4120ff3e84aaa70cfbf1e30
    )
    FetchContent_MakeAvailable(lua_emscripten)

    # Create Lua library from source
    set(LUA_SRC_DIR "${lua_emscripten_SOURCE_DIR}/src")
    file(GLOB LUA_CORE_SOURCES
        "${LUA_SRC_DIR}/lapi.c"
        "${LUA_SRC_DIR}/lcode.c"
        "${LUA_SRC_DIR}/lctype.c"
        "${LUA_SRC_DIR}/ldebug.c"
        "${LUA_SRC_DIR}/ldo.c"
        "${LUA_SRC_DIR}/ldump.c"
        "${LUA_SRC_DIR}/lfunc.c"
        "${LUA_SRC_DIR}/lgc.c"
        "${LUA_SRC_DIR}/llex.c"
        "${LUA_SRC_DIR}/lmem.c"
        "${LUA_SRC_DIR}/lobject.c"
        "${LUA_SRC_DIR}/lopcodes.c"
        "${LUA_SRC_DIR}/lparser.c"
        "${LUA_SRC_DIR}/lstate.c"
        "${LUA_SRC_DIR}/lstring.c"
        "${LUA_SRC_DIR}/ltable.c"
        "${LUA_SRC_DIR}/ltm.c"
        "${LUA_SRC_DIR}/lundump.c"
        "${LUA_SRC_DIR}/lvm.c"
        "${LUA_SRC_DIR}/lzio.c"
        "${LUA_SRC_DIR}/lauxlib.c"
        "${LUA_SRC_DIR}/lbaselib.c"
        "${LUA_SRC_DIR}/lcorolib.c"
        "${LUA_SRC_DIR}/ldblib.c"
        "${LUA_SRC_DIR}/liolib.c"
        "${LUA_SRC_DIR}/lmathlib.c"
        "${LUA_SRC_DIR}/loadlib.c"
        "${LUA_SRC_DIR}/loslib.c"
        "${LUA_SRC_DIR}/lstrlib.c"
        "${LUA_SRC_DIR}/ltablib.c"
        "${LUA_SRC_DIR}/lutf8lib.c"
        "${LUA_SRC_DIR}/linit.c"
    )
    add_library(lua_emscripten_lib STATIC ${LUA_CORE_SOURCES})
    target_include_directories(lua_emscripten_lib PUBLIC "${LUA_SRC_DIR}")
    # For WebAssembly: use 32-bit integers to match wasm32 calling conventions
    target_compile_definitions(lua_emscripten_lib PUBLIC
        LUA_USE_C89
        LUA_32BITS  # Use 32-bit integers and floats for WASM compatibility
    )
    target_link_libraries(lupine_core PUBLIC lua_emscripten_lib)

    # sol2 include directory
    target_include_directories(lupine_core PUBLIC "${SOL2_INCLUDE_DIR}/include")

    # Box2D 3.x for Emscripten - build from source (match vcpkg version 3.1.1)
    set(BOX2D_SAMPLES OFF CACHE BOOL "" FORCE)
    set(BOX2D_UNIT_TESTS OFF CACHE BOOL "" FORCE)
    set(BOX2D_VALIDATE OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        box2d_emscripten
        GIT_REPOSITORY https://github.com/erincatto/box2d.git
        GIT_TAG v3.1.1
    )
    FetchContent_MakeAvailable(box2d_emscripten)
    # Disable -Werror for box2d on Emscripten (has unused variable warnings)
    if(TARGET box2d)
        get_target_property(BOX2D_COMPILE_OPTIONS box2d COMPILE_OPTIONS)
        if(BOX2D_COMPILE_OPTIONS)
            list(REMOVE_ITEM BOX2D_COMPILE_OPTIONS "-Werror")
            set_target_properties(box2d PROPERTIES COMPILE_OPTIONS "${BOX2D_COMPILE_OPTIONS}")
        endif()
        target_compile_options(box2d PRIVATE -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable)
    endif()
    target_link_libraries(lupine_core PUBLIC box2d)

    # Bullet Physics - use Emscripten port
    target_compile_options(lupine_core PUBLIC "-sUSE_BULLET=1")
    # Note: USE_BULLET is already in link options above

    # Assimp for Emscripten - build from source (match vcpkg version 6.0.2)
    set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
    set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)
    set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_DRACO OFF CACHE BOOL "" FORCE)  # Disable draco to simplify build
    # Enable only common formats for web
    set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_COLLADA_IMPORTER ON CACHE BOOL "" FORCE)
    FetchContent_Declare(
        assimp_emscripten
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG v5.4.3
    )
    FetchContent_MakeAvailable(assimp_emscripten)
    target_link_libraries(lupine_core PUBLIC assimp)

else()
    # Native: Use vcpkg packages
    target_link_libraries(lupine_core
        PUBLIC
            spdlog::spdlog
            glm::glm
            nlohmann_json::nlohmann_json
            box2d::box2d
            ${BULLET_LIBRARIES}
            ${LUA_LIBRARIES}
            sol2::sol2
            assimp::assimp
            ZLIB::ZLIB
    )
endif()

# Find stb (header-only library)
find_path(STB_INCLUDE_DIRS "stb_image.h")
if(STB_INCLUDE_DIRS)
    target_include_directories(lupine_core PUBLIC ${STB_INCLUDE_DIRS})
    message(STATUS "Core: Found stb headers at ${STB_INCLUDE_DIRS}")
else()
    # Fetch stb for Emscripten or if not found
    set(STB_DIR "${CMAKE_SOURCE_DIR}/external/stb")
    if(NOT EXISTS "${STB_DIR}/stb_image.h")
        message(STATUS "Core: Fetching stb headers...")
        include(FetchContent)
        FetchContent_Declare(
            stb
            GIT_REPOSITORY https://github.com/nothings/stb.git
            SOURCE_DIR "${STB_DIR}"
        )
        FetchContent_MakeAvailable(stb)
    endif()
    target_include_directories(lupine_core PUBLIC "${STB_DIR}")
    message(STATUS "Core: Using stb headers at ${STB_DIR}")
endif()

# Find miniaudio (header-only library)
find_path(MINIAUDIO_INCLUDE_DIRS "miniaudio.h")
if(MINIAUDIO_INCLUDE_DIRS)
    target_include_directories(lupine_core PUBLIC ${MINIAUDIO_INCLUDE_DIRS})
    message(STATUS "Core: Found miniaudio headers at ${MINIAUDIO_INCLUDE_DIRS}")
else()
    # Fetch miniaudio for Emscripten or if not found
    set(MINIAUDIO_DIR "${CMAKE_SOURCE_DIR}/external/miniaudio")
    if(NOT EXISTS "${MINIAUDIO_DIR}/miniaudio.h")
        message(STATUS "Core: Fetching miniaudio headers...")
        include(FetchContent)
        FetchContent_Declare(
            miniaudio
            GIT_REPOSITORY https://github.com/mackron/miniaudio.git
            SOURCE_DIR "${MINIAUDIO_DIR}"
        )
        FetchContent_MakeAvailable(miniaudio)
    endif()
    target_include_directories(lupine_core PUBLIC "${MINIAUDIO_DIR}")
    message(STATUS "Core: Using miniaudio headers at ${MINIAUDIO_DIR}")
endif()

# Video playback backend (FFmpeg). Powers the VideoPlayer component's
# VideoDecoder. Deliberately stripped to the four libraries we actually use
# (avformat/avcodec/swscale/swresample, pulling avutil transitively) via the
# vcpkg feature flags in vcpkg.json, so we avoid the full ~100MB FFmpeg build.
# Turn this OFF to compile out video support entirely: VideoDecoder then becomes
# a no-op (Open() returns false) and the GIF backend is unaffected.
option(LUPINE_ENABLE_VIDEO "Enable FFmpeg-backed video playback (VideoPlayer component)" ON)
if(LUPINE_ENABLE_VIDEO AND NOT EMSCRIPTEN)
    find_package(FFMPEG QUIET)
    if(FFMPEG_FOUND)
        target_include_directories(lupine_core PRIVATE ${FFMPEG_INCLUDE_DIRS})
        target_link_directories(lupine_core PRIVATE ${FFMPEG_LIBRARY_DIRS})
        target_link_libraries(lupine_core PRIVATE ${FFMPEG_LIBRARIES})
        target_compile_definitions(lupine_core PRIVATE LUPINE_ENABLE_VIDEO)

        # FFmpeg's static libraries pull in several Windows system libraries
        # (crypto, networking, Media Foundation). Linking them defensively keeps
        # the static link from failing with unresolved externals.
        if(WIN32)
            target_link_libraries(lupine_core PRIVATE
                bcrypt secur32 ws2_32 mfplat mfuuid strmiids ole32 user32)
        endif()

        message(STATUS "Core: FFmpeg video backend enabled (VideoPlayer)")
    else()
        message(WARNING "Core: LUPINE_ENABLE_VIDEO is ON but FFmpeg was not found via find_package(FFMPEG); video playback will be compiled out")
    endif()
elseif(LUPINE_ENABLE_VIDEO AND EMSCRIPTEN)
    message(STATUS "Core: Video backend not available for Emscripten builds; VideoPlayer degrades to a no-op")
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

# Link MicroPython if available (for embedded Python scripting)
if(LUPINE_HAS_MICROPYTHON)
    target_link_libraries(lupine_core PUBLIC micropython)
    target_compile_definitions(lupine_core PUBLIC LUPINE_HAS_MICROPYTHON)
    # MicroPython macros use C designated initializers which require C++20 on MSVC
    if(MSVC)
        set_source_files_properties(
            ${CMAKE_CURRENT_SOURCE_DIR}/src/scripting/MicroPythonEnvironment.cpp
            PROPERTIES COMPILE_FLAGS "/std:c++20"
        )
    endif()
    message(STATUS "Core: Linking MicroPython support")
endif()

# Link networking transport libraries if enabled. The subsystem still compiles
# and links when this is off (loopback transport + manager are pure C++); the
# ENet/WebSocket transports degrade to no-ops without the define.
if(LUPINE_HAS_NETWORKING)
    target_compile_definitions(lupine_core PUBLIC LUPINE_HAS_NETWORKING)
    if(NOT EMSCRIPTEN)
        if(LUPINE_ENET_TARGET)
            target_link_libraries(lupine_core PUBLIC ${LUPINE_ENET_TARGET})
        endif()
        if(WIN32)
            # ENet needs Winsock2 + multimedia timers on Windows.
            target_link_libraries(lupine_core PUBLIC ws2_32 winmm)
        endif()
        if(LUPINE_HAS_WEBSOCKET AND LUPINE_WS_TARGET)
            target_link_libraries(lupine_core PUBLIC ${LUPINE_WS_TARGET})
            target_compile_definitions(lupine_core PUBLIC LUPINE_HAS_WEBSOCKET)
        endif()
    else()
        # Emscripten: the browser WebSocket API needs the WEBSOCKET link flag.
        target_link_options(lupine_core PUBLIC "-lwebsocket.js")
        target_compile_definitions(lupine_core PUBLIC LUPINE_HAS_WEBSOCKET)
    endif()
    message(STATUS "Core: Linking networking support")
endif()

# Profiler instrumentation. ON by default for tooling/debug builds; a shipping
# build can set -DLUPINE_ENABLE_PROFILER=OFF to compile out every capture site
# (the LUPINE_PROFILE_ZONE macro expands to nothing without the define).
option(LUPINE_ENABLE_PROFILER "Enable the engine profiler instrumentation" ON)
if(LUPINE_ENABLE_PROFILER)
    target_compile_definitions(lupine_core PUBLIC LUPINE_ENABLE_PROFILER)
    message(STATUS "Core: Profiler instrumentation enabled")
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

    # macOS-specific OpenGL support using NSOpenGLContext
    if(APPLE)
        # Enable Objective-C++ for .mm files (if not already enabled by Metal)
        enable_language(OBJCXX)

        # Add macOS OpenGL platform code
        target_sources(lupine_core PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/backends/opengl/GfxDeviceOpenGL_macOS.mm"
        )

        # Set compile options for Objective-C++ file
        set_source_files_properties(
            "${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/backends/opengl/GfxDeviceOpenGL_macOS.mm"
            PROPERTIES
                COMPILE_FLAGS "-fobjc-arc"
        )

        message(STATUS "Core: Added macOS OpenGL support (NSOpenGLContext)")
    endif()
endif()

if(LUPINE_HAS_VULKAN)
    find_package(glslang CONFIG REQUIRED)
    target_link_libraries(lupine_core
        PUBLIC
            Vulkan::Vulkan
            glslang::glslang
            glslang::SPIRV
            glslang::glslang-default-resource-limits
    )
    target_include_directories(lupine_core
        PUBLIC
            ${Vulkan_INCLUDE_DIRS}
    )
    message(STATUS "Core: Linking Vulkan support with glslang")
endif()

if(LUPINE_HAS_DIRECTX11)
    target_link_libraries(lupine_core
        PUBLIC
            d3d11.lib
            dxgi.lib
            d3dcompiler.lib
            dxguid.lib
    )
    target_compile_definitions(lupine_core PUBLIC LUPINE_HAS_DIRECTX11)
    message(STATUS "Core: Linking DirectX 11 support")
endif()

if(LUPINE_HAS_DIRECTX12)
    target_link_libraries(lupine_core
        PUBLIC
            d3d12.lib
            dxgi.lib
            d3dcompiler.lib
            dxguid.lib
    )
    target_compile_definitions(lupine_core PUBLIC LUPINE_HAS_DIRECTX12)
    message(STATUS "Core: Linking DirectX 12 support")
endif()

# WebGL support (Emscripten only)
if(EMSCRIPTEN)
    # Add WebGL source files explicitly
    target_sources(lupine_core PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/backends/webgl/WebGLState.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/backends/webgl/GfxDeviceWebGL.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/backends/webgl/GfxCommandListWebGL.cpp"
    )

    # Emscripten compile/link flags for WebGL 2.0
    target_compile_options(lupine_core PUBLIC
        "-sUSE_WEBGL2=1"
        "-sFULL_ES3=1"
    )
    target_link_options(lupine_core PUBLIC
        "-sUSE_WEBGL2=1"
        "-sFULL_ES3=1"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sMAX_WEBGL_VERSION=2"
        "-sMIN_WEBGL_VERSION=2"
    )
    target_compile_definitions(lupine_core PUBLIC LUPINE_WEBGL)
    message(STATUS "Core: Configuring WebGL 2.0 support for Emscripten")
endif()

# Metal support (macOS/iOS only)
if(LUPINE_HAS_METAL)
    # Enable Objective-C++ for .mm files
    enable_language(OBJCXX)

    # Find and link Metal frameworks
    find_library(METAL_FRAMEWORK Metal)
    find_library(METALKIT_FRAMEWORK MetalKit)
    find_library(QUARTZCORE_FRAMEWORK QuartzCore)
    find_library(APPKIT_FRAMEWORK AppKit)
    find_library(FOUNDATION_FRAMEWORK Foundation)

    if(METAL_FRAMEWORK AND METALKIT_FRAMEWORK AND QUARTZCORE_FRAMEWORK)
        target_link_libraries(lupine_core
            PUBLIC
                ${METAL_FRAMEWORK}
                ${METALKIT_FRAMEWORK}
                ${QUARTZCORE_FRAMEWORK}
                ${APPKIT_FRAMEWORK}
                ${FOUNDATION_FRAMEWORK}
        )
        target_compile_definitions(lupine_core PUBLIC LUPINE_HAS_METAL)
        message(STATUS "Core: Linking Metal support")

        # Add Metal source files explicitly (since GLOB might not pick up .mm files)
        target_sources(lupine_core PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/backends/metal/MetalState.mm"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/backends/metal/GfxDeviceMetal.mm"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/backends/metal/GfxCommandListMetal.mm"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/debug/DebugRendererMetal.mm"
        )

        # Set compile options for Objective-C++ files
        set_source_files_properties(
            "${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/backends/metal/MetalState.mm"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/backends/metal/GfxDeviceMetal.mm"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/backends/metal/GfxCommandListMetal.mm"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/rendering/debug/DebugRendererMetal.mm"
            PROPERTIES
                COMPILE_FLAGS "-fobjc-arc"
        )
    else()
        message(WARNING "Core: Metal frameworks not found - Metal support disabled")
        set(LUPINE_HAS_METAL FALSE PARENT_SCOPE)
    endif()
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
