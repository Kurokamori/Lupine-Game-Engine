# =============================================================================
# Lupine Engine - Export Templates CMake Configuration
# =============================================================================
#
# This module provides targets for building export templates for various platforms.
# Export templates are pre-compiled runtime executables that can be combined with
# game data (.pck files) to create standalone game distributions.
#
# Supported platforms:
# - Windows x64 (MSVC)
# - Linux x64 (GCC/Clang)
# - macOS ARM64 (Apple Silicon)
# - macOS x64 (Intel)
# - Web (Emscripten/WebAssembly)
#
# Every platform produces TWO templates:
# - A release template, optimized and stripped.
# - A debug template, which keeps symbols and function names so that crash
#   dumps, stack traces and profilers show real names instead of addresses.
#   The editor picks this one when a preset enables "Include debug symbols".
#
# Usage:
#   cmake -DLUPINE_BUILD_EXPORT_TEMPLATES=ON ..
#   cmake --build . --target lupine_export_templates
#
# For web templates on any platform:
#   cmake -DLUPINE_BUILD_EXPORT_TEMPLATES=ON -DLUPINE_BUILD_WEB_TEMPLATE=ON ..
#
# =============================================================================

option(LUPINE_BUILD_EXPORT_TEMPLATES "Build export templates for game distribution" OFF)
option(LUPINE_BUILD_WEB_TEMPLATE "Also build web template via Emscripten (requires emsdk)" OFF)

# Template output directory
set(LUPINE_TEMPLATES_DIR "${CMAKE_BINARY_DIR}/export_templates" CACHE PATH "Export templates output directory")

# Platform identifier
if(WIN32)
    set(LUPINE_TEMPLATE_PLATFORM "windows")
    set(LUPINE_TEMPLATE_ARCH "x64")
    set(LUPINE_TEMPLATE_EXT ".exe")
elseif(APPLE)
    set(LUPINE_TEMPLATE_PLATFORM "macos")
    if(CMAKE_OSX_ARCHITECTURES STREQUAL "arm64" OR CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "arm64")
        set(LUPINE_TEMPLATE_ARCH "arm64")
    else()
        set(LUPINE_TEMPLATE_ARCH "x64")
    endif()
    set(LUPINE_TEMPLATE_EXT "")
elseif(EMSCRIPTEN)
    set(LUPINE_TEMPLATE_PLATFORM "web")
    set(LUPINE_TEMPLATE_ARCH "wasm32")
    set(LUPINE_TEMPLATE_EXT ".html")
else()
    set(LUPINE_TEMPLATE_PLATFORM "linux")
    set(LUPINE_TEMPLATE_ARCH "x64")
    set(LUPINE_TEMPLATE_EXT "")
endif()

# =============================================================================
# Export Template Executable Target
# =============================================================================

# =============================================================================
# Shared template configuration
# =============================================================================
#
# Both the release and the debug template are the same program (main_template.cpp
# linked against lupine_runtime_lib); they differ only in symbol/optimization
# settings and, on Windows, in whether a console is attached. Everything they
# share lives in this function so the two can never drift apart.

function(lupine_configure_template TARGET_NAME)
    # Link dependencies - export templates use embedded scripting (no external DLLs)
    # MicroPython, Lua, and mRuby are all statically linked
    # NOTE: Export templates do NOT link against CPython - they use MicroPython only
    target_link_libraries(${TARGET_NAME} PRIVATE lupine_runtime_lib)

    # Export templates don't need CPython - they use MicroPython
    # Ignore the embedded #pragma comment(lib, "python312.lib") from Python.h
    if(WIN32)
        if(MSVC)
            target_link_options(${TARGET_NAME} PRIVATE
                /NODEFAULTLIB:python312.lib
                /NODEFAULTLIB:python312_d.lib
            )
        elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            # Clang with lld-link needs -Xlinker prefix for MSVC linker options
            target_link_options(${TARGET_NAME} PRIVATE
                "LINKER:/NODEFAULTLIB:python312.lib"
                "LINKER:/NODEFAULTLIB:python312_d.lib"
            )
        endif()
    endif()
    target_compile_definitions(${TARGET_NAME} PRIVATE LUPINE_NO_CPYTHON)

    # Add MicroPython for Python scripting (embedded, no DLLs needed)
    if(LUPINE_HAS_MICROPYTHON)
        target_link_libraries(${TARGET_NAME} PRIVATE micropython)
        target_compile_definitions(${TARGET_NAME} PRIVATE LUPINE_HAS_MICROPYTHON)
    endif()

    if(EMSCRIPTEN)
        set_target_properties(${TARGET_NAME} PROPERTIES
            OUTPUT_NAME "index"
            SUFFIX ".html"
        )

        # Emscripten-specific link options for itch.io compatible web builds
        target_link_options(${TARGET_NAME} PRIVATE
            # Runtime exports for JavaScript interop
            "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','UTF8ToString','stringToUTF8']"
            "-sEXPORTED_FUNCTIONS=['_main','_malloc','_free']"

            # Memory configuration
            "-sALLOW_MEMORY_GROWTH=1"
            "-sINITIAL_MEMORY=67108864"   # 64MB initial
            "-sMAXIMUM_MEMORY=536870912"  # 512MB max
            "-sSTACK_SIZE=1048576"        # 1MB stack

            # WebAssembly settings
            "-sWASM=1"
            "-sWASM_BIGINT=1"

            # Module configuration - non-modular for easier loading
            "-sMODULARIZE=0"
            "-sINVOKE_RUN=1"
            "-sEXIT_RUNTIME=0"

            # Environment
            "-sENVIRONMENT='web'"

            # WebGL 2.0 support
            "-sUSE_WEBGL2=1"
            "-sFULL_ES3=1"
            "-sMAX_WEBGL_VERSION=2"
            "-sMIN_WEBGL_VERSION=2"

            # File system support for asset loading
            "-sFORCE_FILESYSTEM=1"

            # IndexedDB-backed filesystem. Required for the IDBFS mount that backs
            # user:// (saves, settings) - without it FS.mount(IDBFS, ...) in the shell
            # throws and user data silently degrades to session-only MEMFS.
            "-lidbfs.js"

            # Enable C++ exceptions
            "-fexceptions"
            "-sNO_DISABLE_EXCEPTION_CATCHING"

            # Shell file (HTML template)
            "--shell-file=${CMAKE_SOURCE_DIR}/runtime/web/shell.html"
        )

        # If game assets directory exists, preload it
        if(EXISTS "${CMAKE_SOURCE_DIR}/resources")
            target_link_options(${TARGET_NAME} PRIVATE
                "--preload-file=${CMAKE_SOURCE_DIR}/resources@/"
            )
        endif()

    elseif(WIN32)
        set_target_properties(${TARGET_NAME} PROPERTIES
            OUTPUT_NAME "lupine_template_${LUPINE_TEMPLATE_PLATFORM}_${LUPINE_TEMPLATE_ARCH}${LUPINE_TEMPLATE_SUFFIX}"
        )

    elseif(APPLE)
        set_target_properties(${TARGET_NAME} PROPERTIES
            OUTPUT_NAME "lupine_template_${LUPINE_TEMPLATE_PLATFORM}_${LUPINE_TEMPLATE_ARCH}${LUPINE_TEMPLATE_SUFFIX}"
            MACOSX_BUNDLE FALSE
        )

    else()
        set_target_properties(${TARGET_NAME} PROPERTIES
            OUTPUT_NAME "lupine_template_${LUPINE_TEMPLATE_PLATFORM}_${LUPINE_TEMPLATE_ARCH}${LUPINE_TEMPLATE_SUFFIX}"
        )
    endif()
endfunction()

# -----------------------------------------------------------------------------
# Release-only settings: optimized, no symbols.
# -----------------------------------------------------------------------------
function(lupine_configure_template_release TARGET_NAME)
    if(EMSCRIPTEN)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            target_link_options(${TARGET_NAME} PRIVATE
                "-sASSERTIONS=2"
                "-sSAFE_HEAP=1"
                "-sSTACK_OVERFLOW_CHECK=2"
                "-gsource-map"
            )
        else()
            # Release optimizations
            # Note: --closure=1 disabled due to miniaudio using undeclared 'miniaudio' variable
            # (should use 'window.miniaudio' consistently for Closure Compiler compatibility)
            target_link_options(${TARGET_NAME} PRIVATE
                "-sASSERTIONS=0"
                "-O3"
            )
        endif()
    elseif(WIN32)
        # No console window for the shipping template.
        set_target_properties(${TARGET_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
    endif()
endfunction()

# -----------------------------------------------------------------------------
# Debug-only settings: keep symbols and function names.
#
# The point of the debug template is that a stack trace from an exported game is
# readable. That means the compiler must emit debug info AND the linker must be
# told not to throw it away - a release-configured link strips or never records
# the symbol table even when the objects carry one.
# -----------------------------------------------------------------------------
function(lupine_configure_template_debug TARGET_NAME)
    target_compile_definitions(${TARGET_NAME} PRIVATE LUPINE_DEBUG_TEMPLATE=1)

    if(EMSCRIPTEN)
        # -g2 keeps the WebAssembly "name" section and leaves the generated JS
        # unminified, so browser stack traces show C++ function names instead of
        # wasm-function[1234]. --profiling-funcs asks the toolchain to preserve
        # those names even through optimization, and --emit-symbol-map writes
        # index.wasm.symbols for tools that demangle after the fact.
        #
        # Optimization stays at -O2: a debug template still has to be playable,
        # and -O0 wasm is far too slow to reproduce timing-dependent bugs.
        target_compile_options(${TARGET_NAME} PRIVATE
            "-g2"
            "-fno-omit-frame-pointer"
        )
        target_link_options(${TARGET_NAME} PRIVATE
            "-O2"
            "-g2"
            "--profiling-funcs"
            "--emit-symbol-map"
            "-sASSERTIONS=2"
            "-sSTACK_OVERFLOW_CHECK=2"
        )

    elseif(MSVC)
        # MSVC and clang-cl. /Zi emits a PDB; /DEBUG:FULL makes the linker actually
        # write it. /OPT:REF,ICF undo the size regression /DEBUG otherwise implies
        # (the linker disables both as soon as debug info is requested).
        #
        # No /Oy- here: frame-pointer omission is an x86-only switch, and on x64 the
        # stack walker unwinds from the PE unwind tables rather than a frame chain.
        target_compile_options(${TARGET_NAME} PRIVATE /Zi)
        target_link_options(${TARGET_NAME} PRIVATE
            /DEBUG:FULL
            /OPT:REF
            /OPT:ICF
            /INCREMENTAL:NO
        )
        # Console window: exported debug builds print engine and script logs to stdout.
        set_target_properties(${TARGET_NAME} PROPERTIES WIN32_EXECUTABLE FALSE)

    elseif(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        # Clang with the GNU driver but the MSVC linker: -gcodeview is the only
        # debug format lld-link can turn into a PDB.
        target_compile_options(${TARGET_NAME} PRIVATE -g -gcodeview -fno-omit-frame-pointer)
        target_link_options(${TARGET_NAME} PRIVATE "LINKER:/DEBUG:FULL")
        set_target_properties(${TARGET_NAME} PROPERTIES WIN32_EXECUTABLE FALSE)

    else()
        # Linux / macOS. -rdynamic puts the symbols in the dynamic symbol table,
        # which is what backtrace_symbols() and most crash handlers actually read;
        # without it a release-optimized binary reports bare addresses even though
        # the DWARF is present.
        target_compile_options(${TARGET_NAME} PRIVATE -g -fno-omit-frame-pointer)
        if(APPLE)
            target_link_options(${TARGET_NAME} PRIVATE -g)
        else()
            target_link_options(${TARGET_NAME} PRIVATE -g -rdynamic)
        endif()
    endif()
endfunction()

if(LUPINE_BUILD_EXPORT_TEMPLATES)
    message(STATUS "")
    message(STATUS "=======================================================")
    message(STATUS "         Building Export Templates")
    message(STATUS "=======================================================")
    message(STATUS "Platform: ${LUPINE_TEMPLATE_PLATFORM}")
    message(STATUS "Architecture: ${LUPINE_TEMPLATE_ARCH}")
    message(STATUS "Output: ${LUPINE_TEMPLATES_DIR}")
    message(STATUS "Variants: release + debug (symbols, function names)")
    message(STATUS "=======================================================")
    message(STATUS "")

    # Create output directory
    file(MAKE_DIRECTORY "${LUPINE_TEMPLATES_DIR}")

    # Template sources. Windows additionally compiles the generated resource
    # script so the exporter has an icon resource to overwrite.
    set(LUPINE_TEMPLATE_SOURCES ${CMAKE_SOURCE_DIR}/runtime/src/main_template.cpp)
    if(WIN32)
        list(APPEND LUPINE_TEMPLATE_SOURCES ${CMAKE_BINARY_DIR}/runtime/resource.rc)
    endif()

    if(EXISTS "${CMAKE_SOURCE_DIR}/resources")
        message(STATUS "Web build: Preloading resources from ${CMAKE_SOURCE_DIR}/resources")
    endif()

    # --- Release template ---
    add_executable(lupine_template ${LUPINE_TEMPLATE_SOURCES})
    set(LUPINE_TEMPLATE_SUFFIX "")
    lupine_configure_template(lupine_template)
    lupine_configure_template_release(lupine_template)

    # --- Debug template ---
    add_executable(lupine_template_debug ${LUPINE_TEMPLATE_SOURCES})
    set(LUPINE_TEMPLATE_SUFFIX "_debug")
    lupine_configure_template(lupine_template_debug)
    lupine_configure_template_debug(lupine_template_debug)
    unset(LUPINE_TEMPLATE_SUFFIX)

    if(EMSCRIPTEN)
        # Both web templates are named index.{html,js,wasm} - the shell and the
        # exporter both hardcode that name - so they cannot share an output
        # directory or the second link would clobber the first.
        set_target_properties(lupine_template_debug PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/web_debug"
        )
    endif()

    # =============================================================================
    # Staging into the templates directory
    # =============================================================================

    # Release template -> export_templates/<platform>/
    add_custom_command(TARGET lupine_template POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${LUPINE_TEMPLATES_DIR}/${LUPINE_TEMPLATE_PLATFORM}"
        COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:lupine_template>
            "${LUPINE_TEMPLATES_DIR}/${LUPINE_TEMPLATE_PLATFORM}/"
        COMMENT "Copying export template to ${LUPINE_TEMPLATES_DIR}/${LUPINE_TEMPLATE_PLATFORM}/"
    )

    # Debug template -> export_templates/<platform>/ for desktop, but
    # export_templates/web/debug/ for web, whose files are all named index.*.
    if(EMSCRIPTEN)
        set(LUPINE_DEBUG_TEMPLATE_DEST "${LUPINE_TEMPLATES_DIR}/${LUPINE_TEMPLATE_PLATFORM}/debug")
    else()
        set(LUPINE_DEBUG_TEMPLATE_DEST "${LUPINE_TEMPLATES_DIR}/${LUPINE_TEMPLATE_PLATFORM}")
    endif()

    add_custom_command(TARGET lupine_template_debug POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${LUPINE_DEBUG_TEMPLATE_DEST}"
        COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:lupine_template_debug>
            "${LUPINE_DEBUG_TEMPLATE_DEST}/"
        COMMENT "Copying debug export template to ${LUPINE_DEBUG_TEMPLATE_DEST}/"
    )

    if(MSVC)
        # The PDB is the debug template's whole reason for existing; without it
        # the exported .exe has no way back to function names.
        add_custom_command(TARGET lupine_template_debug POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_PDB_FILE:lupine_template_debug>"
                "${LUPINE_DEBUG_TEMPLATE_DEST}/"
            COMMENT "Copying debug export template PDB"
        )
    endif()

    if(EMSCRIPTEN)
        # The Emscripten target file is only index.html - the program itself lives in the
        # sibling .wasm/.js/.data that emcc emits alongside it. Copying just the target
        # leaves whatever .wasm was already staged in place, so every web export keeps
        # shipping a stale engine no matter how often the template is rebuilt.
        add_custom_command(TARGET lupine_template POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE_DIR:lupine_template>/index.wasm"
                "$<TARGET_FILE_DIR:lupine_template>/index.js"
                "${LUPINE_TEMPLATES_DIR}/${LUPINE_TEMPLATE_PLATFORM}/"
            COMMENT "Copying web template runtime (index.wasm, index.js)"
        )

        add_custom_command(TARGET lupine_template_debug POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE_DIR:lupine_template_debug>/index.wasm"
                "$<TARGET_FILE_DIR:lupine_template_debug>/index.js"
                "${LUPINE_DEBUG_TEMPLATE_DEST}/"
            COMMENT "Copying debug web template runtime (index.wasm, index.js)"
        )

        # --emit-symbol-map writes index.wasm.symbols next to the wasm. It is not a
        # link output CMake knows about, so it is copied only if the link produced one.
        add_custom_command(TARGET lupine_template_debug POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                -DSOURCE_FILE="$<TARGET_FILE_DIR:lupine_template_debug>/index.wasm.symbols"
                -DDEST_DIR="${LUPINE_DEBUG_TEMPLATE_DEST}"
                -P "${CMAKE_SOURCE_DIR}/cmake/CopyIfExists.cmake"
            COMMENT "Copying debug web template symbol map (index.wasm.symbols)"
        )

        # index.data exists only when the --preload-file above produced one, and copying a
        # file that was never emitted fails the build.
        if(EXISTS "${CMAKE_SOURCE_DIR}/resources")
            add_custom_command(TARGET lupine_template POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE_DIR:lupine_template>/index.data"
                    "${LUPINE_TEMPLATES_DIR}/${LUPINE_TEMPLATE_PLATFORM}/"
                COMMENT "Copying web template preload data (index.data)"
            )
            add_custom_command(TARGET lupine_template_debug POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE_DIR:lupine_template_debug>/index.data"
                    "${LUPINE_DEBUG_TEMPLATE_DEST}/"
                COMMENT "Copying debug web template preload data (index.data)"
            )
        endif()
    endif()

    # Create a combined target for all templates
    add_custom_target(lupine_export_templates
        DEPENDS lupine_template lupine_template_debug
        COMMENT "Building all export templates"
    )

    # =============================================================================
    # Template manifest file
    # =============================================================================

    # The web template is emitted as index.html regardless of platform naming, and
    # its debug variant lives in a subdirectory rather than under a suffixed name.
    if(EMSCRIPTEN)
        set(LUPINE_TEMPLATE_BINARY "index.html")
        set(LUPINE_TEMPLATE_DEBUG_BINARY "debug/index.html")
    else()
        set(LUPINE_TEMPLATE_BINARY "lupine_template_${LUPINE_TEMPLATE_PLATFORM}_${LUPINE_TEMPLATE_ARCH}${LUPINE_TEMPLATE_EXT}")
        set(LUPINE_TEMPLATE_DEBUG_BINARY "lupine_template_${LUPINE_TEMPLATE_PLATFORM}_${LUPINE_TEMPLATE_ARCH}_debug${LUPINE_TEMPLATE_EXT}")
    endif()

    # Generate a manifest file with template information
    set(TEMPLATE_MANIFEST_CONTENT "{
  \"version\": \"${PROJECT_VERSION}\",
  \"templates\": {
    \"${LUPINE_TEMPLATE_PLATFORM}_${LUPINE_TEMPLATE_ARCH}\": {
      \"platform\": \"${LUPINE_TEMPLATE_PLATFORM}\",
      \"architecture\": \"${LUPINE_TEMPLATE_ARCH}\",
      \"binary\": \"${LUPINE_TEMPLATE_BINARY}\",
      \"debug_binary\": \"${LUPINE_TEMPLATE_DEBUG_BINARY}\",
      \"graphics_backends\": [")

    # Add supported graphics backends
    set(BACKENDS_LIST "")
    if(LUPINE_HAS_OPENGL)
        list(APPEND BACKENDS_LIST "\"opengl\"")
    endif()
    if(LUPINE_HAS_VULKAN)
        list(APPEND BACKENDS_LIST "\"vulkan\"")
    endif()
    if(LUPINE_HAS_DIRECTX11)
        list(APPEND BACKENDS_LIST "\"dx11\"")
    endif()
    if(LUPINE_HAS_DIRECTX12)
        list(APPEND BACKENDS_LIST "\"dx12\"")
    endif()
    if(LUPINE_HAS_METAL)
        list(APPEND BACKENDS_LIST "\"metal\"")
    endif()
    if(EMSCRIPTEN)
        list(APPEND BACKENDS_LIST "\"webgl\"")
    endif()

    string(JOIN ", " BACKENDS_STRING ${BACKENDS_LIST})
    set(TEMPLATE_MANIFEST_CONTENT "${TEMPLATE_MANIFEST_CONTENT}${BACKENDS_STRING}]
    }
  }
}
")

    file(WRITE "${LUPINE_TEMPLATES_DIR}/templates.json" "${TEMPLATE_MANIFEST_CONTENT}")

endif() # LUPINE_BUILD_EXPORT_TEMPLATES

# =============================================================================
# Helper function to create a platform-specific template build
# =============================================================================

function(lupine_add_template_build PLATFORM ARCH)
    message(STATUS "Configuring template build for ${PLATFORM}-${ARCH}")
    # This function can be extended to support cross-compilation
endfunction()

# =============================================================================
# Web Template Cross-Compilation (from any platform)
# =============================================================================
# When LUPINE_BUILD_WEB_TEMPLATE is ON and we're NOT already building with Emscripten,
# create a custom target that builds the web template using emcmake/emmake.

if(LUPINE_BUILD_EXPORT_TEMPLATES AND LUPINE_BUILD_WEB_TEMPLATE AND NOT EMSCRIPTEN)
    message(STATUS "")
    message(STATUS "=======================================================")
    message(STATUS "         Web Template Cross-Compilation Enabled")
    message(STATUS "=======================================================")

    # Find Emscripten tools
    find_program(EMCMAKE_EXECUTABLE emcmake)
    find_program(EMMAKE_EXECUTABLE emmake)
    find_program(EMCC_EXECUTABLE emcc)

    if(EMCMAKE_EXECUTABLE AND EMCC_EXECUTABLE)
        message(STATUS "Emscripten found: ${EMCC_EXECUTABLE}")

        set(WEB_BUILD_DIR "${CMAKE_BINARY_DIR}/web-build")
        set(WEB_OUTPUT_DIR "${LUPINE_TEMPLATES_DIR}/web")

        # Determine build tool
        find_program(NINJA_EXECUTABLE ninja)
        if(NINJA_EXECUTABLE)
            set(WEB_GENERATOR "Ninja")
            set(WEB_BUILD_CMD "${NINJA_EXECUTABLE}" "lupine_template" "lupine_template_debug")
        else()
            set(WEB_GENERATOR "Unix Makefiles")
            set(WEB_BUILD_CMD "make" "-j" "lupine_template" "lupine_template_debug")
        endif()

        # Create web build directory
        file(MAKE_DIRECTORY "${WEB_BUILD_DIR}")
        file(MAKE_DIRECTORY "${WEB_OUTPUT_DIR}")

        # Custom target to configure web build
        add_custom_target(lupine_web_template_configure
            COMMAND ${EMCMAKE_EXECUTABLE} ${CMAKE_COMMAND}
                -S "${CMAKE_SOURCE_DIR}"
                -B "${WEB_BUILD_DIR}"
                -G "${WEB_GENERATOR}"
                -DCMAKE_BUILD_TYPE=$<IF:$<CONFIG:Debug>,Debug,Release>
                -DLUPINE_BUILD_EXPORT_TEMPLATES=ON
                -DLUPINE_BUILD_WEB_TEMPLATE=OFF
                -DBUILD_PYTHON_MODULES=OFF
            WORKING_DIRECTORY "${WEB_BUILD_DIR}"
            COMMENT "Configuring web export template with Emscripten..."
            VERBATIM
        )

        # Custom target to build web template
        add_custom_target(lupine_web_template_build
            COMMAND ${EMMAKE_EXECUTABLE} ${WEB_BUILD_CMD}
            WORKING_DIRECTORY "${WEB_BUILD_DIR}"
            DEPENDS lupine_web_template_configure
            COMMENT "Building web export template..."
            VERBATIM
        )

        # Custom target to copy web template output. The inner Emscripten build
        # stages the debug variant in bin/web_debug/ so its index.* cannot collide
        # with the release variant's.
        set(WEB_DEBUG_BUILD_DIR "${WEB_BUILD_DIR}/bin/web_debug")
        set(WEB_DEBUG_OUTPUT_DIR "${WEB_OUTPUT_DIR}/debug")

        add_custom_target(lupine_web_template
            COMMAND ${CMAKE_COMMAND} -E make_directory "${WEB_OUTPUT_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${WEB_BUILD_DIR}/bin/index.html"
                "${WEB_OUTPUT_DIR}/"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${WEB_BUILD_DIR}/bin/index.js"
                "${WEB_OUTPUT_DIR}/"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${WEB_BUILD_DIR}/bin/index.wasm"
                "${WEB_OUTPUT_DIR}/"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${WEB_DEBUG_OUTPUT_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${WEB_DEBUG_BUILD_DIR}/index.html"
                "${WEB_DEBUG_OUTPUT_DIR}/"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${WEB_DEBUG_BUILD_DIR}/index.js"
                "${WEB_DEBUG_OUTPUT_DIR}/"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${WEB_DEBUG_BUILD_DIR}/index.wasm"
                "${WEB_DEBUG_OUTPUT_DIR}/"
            DEPENDS lupine_web_template_build
            COMMENT "Copying web templates (release + debug) to ${WEB_OUTPUT_DIR}/"
            VERBATIM
        )

        # Optional per-variant outputs: .data only exists when resources/ was
        # preloaded, .wasm.symbols only when --emit-symbol-map ran.
        add_custom_command(TARGET lupine_web_template POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                -DSOURCE_FILE="${WEB_BUILD_DIR}/bin/index.data"
                -DDEST_DIR="${WEB_OUTPUT_DIR}"
                -P "${CMAKE_SOURCE_DIR}/cmake/CopyIfExists.cmake"
            COMMAND ${CMAKE_COMMAND}
                -DSOURCE_FILE="${WEB_DEBUG_BUILD_DIR}/index.data"
                -DDEST_DIR="${WEB_DEBUG_OUTPUT_DIR}"
                -P "${CMAKE_SOURCE_DIR}/cmake/CopyIfExists.cmake"
            COMMAND ${CMAKE_COMMAND}
                -DSOURCE_FILE="${WEB_DEBUG_BUILD_DIR}/index.wasm.symbols"
                -DDEST_DIR="${WEB_DEBUG_OUTPUT_DIR}"
                -P "${CMAKE_SOURCE_DIR}/cmake/CopyIfExists.cmake"
            COMMENT "Copying optional web template files if they exist..."
        )

        # Add web template to the combined export templates target
        if(TARGET lupine_export_templates)
            add_dependencies(lupine_export_templates lupine_web_template)
        endif()

        message(STATUS "Web template will be output to: ${WEB_OUTPUT_DIR}")
        message(STATUS "=======================================================")
        message(STATUS "")

    else()
        message(WARNING "")
        message(WARNING "=======================================================")
        message(WARNING "  Emscripten not found - Web template disabled")
        message(WARNING "=======================================================")
        message(WARNING "To enable web templates, install Emscripten SDK:")
        message(WARNING "  1. Download from https://emscripten.org")
        message(WARNING "  2. Run: emsdk install latest")
        message(WARNING "  3. Run: emsdk activate latest")
        message(WARNING "  4. Add to PATH or run: emsdk_env")
        message(WARNING "=======================================================")
        message(WARNING "")
    endif()

endif() # LUPINE_BUILD_WEB_TEMPLATE
