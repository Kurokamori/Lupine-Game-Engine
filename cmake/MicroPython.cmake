# =============================================================================
# MicroPython CMake Configuration for Lupine Engine
# =============================================================================
#
# This module builds MicroPython as an embeddable static library for use
# in export templates and runtime. This provides Python scripting without
# requiring external DLLs.
#
# On Windows with MSVC, MicroPython is built using Clang to avoid MSVC ICE
# (Internal Compiler Error) issues. If Clang is not available, MicroPython
# is disabled gracefully.
#
# Supports: Windows (via Clang), Linux, macOS (Intel and ARM)
#
# =============================================================================

set(MICROPYTHON_DIR "${CMAKE_SOURCE_DIR}/external/micropython")

# Check if MicroPython submodule exists
if(NOT EXISTS "${MICROPYTHON_DIR}/py/py.mk")
    message(WARNING "MicroPython submodule not found. Run: git submodule update --init external/micropython")
    set(LUPINE_HAS_MICROPYTHON FALSE)
    return()
endif()

message(STATUS "Configuring MicroPython...")

# Find Python for MicroPython's build scripts
find_package(Python3 COMPONENTS Interpreter REQUIRED)

# Set up directories
set(MICROPYTHON_BUILD_DIR "${CMAKE_BINARY_DIR}/micropython_build")
set(MICROPYTHON_GENHDR_DIR "${MICROPYTHON_BUILD_DIR}/genhdr")

# Run our build script to generate all necessary headers
set(MICROPYTHON_BUILD_SCRIPT "${CMAKE_SOURCE_DIR}/cmake/build_micropython_embed.py")

message(STATUS "Generating MicroPython headers...")
execute_process(
    COMMAND ${Python3_EXECUTABLE} "${MICROPYTHON_BUILD_SCRIPT}" "${MICROPYTHON_BUILD_DIR}" "${Python3_EXECUTABLE}"
    RESULT_VARIABLE MICROPYTHON_BUILD_RESULT
    OUTPUT_VARIABLE MICROPYTHON_BUILD_OUTPUT
    ERROR_VARIABLE MICROPYTHON_BUILD_ERROR
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(NOT MICROPYTHON_BUILD_RESULT EQUAL 0)
    message(WARNING "Failed to generate MicroPython headers:\n${MICROPYTHON_BUILD_OUTPUT}\n${MICROPYTHON_BUILD_ERROR}")
    set(LUPINE_HAS_MICROPYTHON FALSE)
    return()
endif()

message(STATUS "${MICROPYTHON_BUILD_OUTPUT}")

# Define MicroPython source files
set(MICROPYTHON_SOURCES
    # Core VM files
    ${MICROPYTHON_DIR}/py/mpstate.c
    ${MICROPYTHON_DIR}/py/nlr.c
    ${MICROPYTHON_DIR}/py/nlrsetjmp.c
    ${MICROPYTHON_DIR}/py/malloc.c
    ${MICROPYTHON_DIR}/py/gc.c
    ${MICROPYTHON_DIR}/py/qstr.c
    ${MICROPYTHON_DIR}/py/vstr.c
    ${MICROPYTHON_DIR}/py/mpprint.c
    ${MICROPYTHON_DIR}/py/unicode.c
    ${MICROPYTHON_DIR}/py/reader.c
    ${MICROPYTHON_DIR}/py/lexer.c
    ${MICROPYTHON_DIR}/py/parse.c
    ${MICROPYTHON_DIR}/py/scope.c
    ${MICROPYTHON_DIR}/py/compile.c
    ${MICROPYTHON_DIR}/py/emitcommon.c
    ${MICROPYTHON_DIR}/py/emitbc.c
    ${MICROPYTHON_DIR}/py/asmbase.c
    ${MICROPYTHON_DIR}/py/formatfloat.c
    ${MICROPYTHON_DIR}/py/parsenumbase.c
    ${MICROPYTHON_DIR}/py/parsenum.c
    ${MICROPYTHON_DIR}/py/emitglue.c
    ${MICROPYTHON_DIR}/py/runtime.c
    ${MICROPYTHON_DIR}/py/runtime_utils.c
    ${MICROPYTHON_DIR}/py/nativeglue.c
    ${MICROPYTHON_DIR}/py/pairheap.c
    ${MICROPYTHON_DIR}/py/ringbuf.c
    ${MICROPYTHON_DIR}/py/cstack.c
    ${MICROPYTHON_DIR}/py/stackctrl.c
    ${MICROPYTHON_DIR}/py/argcheck.c
    ${MICROPYTHON_DIR}/py/warning.c
    ${MICROPYTHON_DIR}/py/map.c
    ${MICROPYTHON_DIR}/py/obj.c
    ${MICROPYTHON_DIR}/py/objarray.c
    ${MICROPYTHON_DIR}/py/objattrtuple.c
    ${MICROPYTHON_DIR}/py/objbool.c
    ${MICROPYTHON_DIR}/py/objboundmeth.c
    ${MICROPYTHON_DIR}/py/objcell.c
    ${MICROPYTHON_DIR}/py/objclosure.c
    ${MICROPYTHON_DIR}/py/objdict.c
    ${MICROPYTHON_DIR}/py/objenumerate.c
    ${MICROPYTHON_DIR}/py/objexcept.c
    ${MICROPYTHON_DIR}/py/objfilter.c
    ${MICROPYTHON_DIR}/py/objfloat.c
    ${MICROPYTHON_DIR}/py/objfun.c
    ${MICROPYTHON_DIR}/py/objgenerator.c
    ${MICROPYTHON_DIR}/py/objgetitemiter.c
    ${MICROPYTHON_DIR}/py/objint.c
    ${MICROPYTHON_DIR}/py/objint_longlong.c
    ${MICROPYTHON_DIR}/py/objlist.c
    ${MICROPYTHON_DIR}/py/objmap.c
    ${MICROPYTHON_DIR}/py/objmodule.c
    ${MICROPYTHON_DIR}/py/objnone.c
    ${MICROPYTHON_DIR}/py/objobject.c
    ${MICROPYTHON_DIR}/py/objpolyiter.c
    ${MICROPYTHON_DIR}/py/objproperty.c
    ${MICROPYTHON_DIR}/py/objrange.c
    ${MICROPYTHON_DIR}/py/objreversed.c
    ${MICROPYTHON_DIR}/py/objset.c
    ${MICROPYTHON_DIR}/py/objsingleton.c
    ${MICROPYTHON_DIR}/py/objslice.c
    ${MICROPYTHON_DIR}/py/objstr.c
    ${MICROPYTHON_DIR}/py/objstringio.c
    ${MICROPYTHON_DIR}/py/objtuple.c
    ${MICROPYTHON_DIR}/py/objtype.c
    ${MICROPYTHON_DIR}/py/objzip.c
    ${MICROPYTHON_DIR}/py/opmethods.c
    ${MICROPYTHON_DIR}/py/sequence.c
    ${MICROPYTHON_DIR}/py/stream.c
    ${MICROPYTHON_DIR}/py/binary.c
    ${MICROPYTHON_DIR}/py/builtinimport.c
    ${MICROPYTHON_DIR}/py/builtinevex.c
    ${MICROPYTHON_DIR}/py/builtinhelp.c
    # Only include modbuiltins, others require proper module registration
    ${MICROPYTHON_DIR}/py/modbuiltins.c
    ${MICROPYTHON_DIR}/py/vm.c
    ${MICROPYTHON_DIR}/py/bc.c
    ${MICROPYTHON_DIR}/py/showbc.c
    ${MICROPYTHON_DIR}/py/repl.c
    ${MICROPYTHON_DIR}/py/smallint.c
    ${MICROPYTHON_DIR}/py/frozenmod.c
    ${MICROPYTHON_DIR}/py/scheduler.c
    # Embed port sources
    ${MICROPYTHON_DIR}/ports/embed/port/embed_util.c
    # Shared runtime
    ${MICROPYTHON_DIR}/shared/runtime/gchelper_generic.c
)

# =============================================================================
# Windows MSVC: Build MicroPython with Clang
# =============================================================================
if(WIN32 AND MSVC)
    message(STATUS "MSVC detected - looking for Clang to build MicroPython...")

    # Try to find Clang
    find_program(CLANG_EXECUTABLE
        NAMES clang clang.exe
        HINTS
            "C:/Program Files/LLVM/bin"
            "C:/Program Files (x86)/LLVM/bin"
            "$ENV{LLVM_PATH}/bin"
            "$ENV{ProgramFiles}/LLVM/bin"
        DOC "Clang compiler for building MicroPython"
    )

    # Also find llvm-ar for creating static libraries
    find_program(LLVM_AR_EXECUTABLE
        NAMES llvm-ar llvm-ar.exe
        HINTS
            "C:/Program Files/LLVM/bin"
            "C:/Program Files (x86)/LLVM/bin"
            "$ENV{LLVM_PATH}/bin"
            "$ENV{ProgramFiles}/LLVM/bin"
        DOC "LLVM archiver for creating static libraries"
    )

    if(NOT CLANG_EXECUTABLE)
        message(WARNING "")
        message(WARNING "=============================================================")
        message(WARNING " MicroPython DISABLED - Clang not found")
        message(WARNING "=============================================================")
        message(WARNING " MSVC has Internal Compiler Errors (ICE) with MicroPython.")
        message(WARNING " To enable MicroPython, install LLVM/Clang:")
        message(WARNING "   https://releases.llvm.org/download.html")
        message(WARNING " Or via winget: winget install LLVM.LLVM")
        message(WARNING "=============================================================")
        message(WARNING "")
        set(LUPINE_HAS_MICROPYTHON FALSE CACHE BOOL "MicroPython support disabled" FORCE)
        return()
    endif()

    message(STATUS "Found Clang: ${CLANG_EXECUTABLE}")

    # Check Clang version
    execute_process(
        COMMAND "${CLANG_EXECUTABLE}" --version
        OUTPUT_VARIABLE CLANG_VERSION_OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    string(REGEX MATCH "clang version ([0-9]+\\.[0-9]+\\.[0-9]+)" CLANG_VERSION_MATCH "${CLANG_VERSION_OUTPUT}")
    if(CLANG_VERSION_MATCH)
        message(STATUS "Clang version: ${CMAKE_MATCH_1}")
    endif()

    # Set up include directories for Clang compilation
    set(MICROPYTHON_INCLUDE_DIRS
        "${MICROPYTHON_BUILD_DIR}"
        "${MICROPYTHON_GENHDR_DIR}"
        "${MICROPYTHON_DIR}"
        "${MICROPYTHON_DIR}/ports/embed"
        "${MICROPYTHON_DIR}/ports/embed/port"
        "${MICROPYTHON_DIR}/py"
        "${MICROPYTHON_DIR}/shared/runtime"
    )

    # Build include flags string
    set(MICROPYTHON_INCLUDE_FLAGS "")
    foreach(INC_DIR ${MICROPYTHON_INCLUDE_DIRS})
        list(APPEND MICROPYTHON_INCLUDE_FLAGS "-I${INC_DIR}")
    endforeach()

    # Clang compile flags for MicroPython
    # Note: -fPIC is not used on Windows (unsupported by MSVC target)
    set(MICROPYTHON_CLANG_FLAGS
        -c
        -O2
        -std=c99
        -DMICROPY_INCLUDED
        -D_CRT_SECURE_NO_WARNINGS
        -DMP_ENDIANNESS_LITTLE=1
        # Warning suppressions
        -Wno-unused-parameter
        -Wno-sign-compare
        -Wno-missing-field-initializers
        -Wno-unused-function
        -Wno-implicit-fallthrough
        -Wno-unused-variable
        -Wno-unused-but-set-variable
        -Wno-incompatible-pointer-types
        # Target MSVC ABI for compatibility
        --target=x86_64-pc-windows-msvc
    )

    # Create output directory for object files
    set(MICROPYTHON_OBJ_DIR "${CMAKE_BINARY_DIR}/micropython_obj")
    file(MAKE_DIRECTORY "${MICROPYTHON_OBJ_DIR}")

    # Generate custom commands to compile each source file with Clang
    set(MICROPYTHON_OBJECTS "")
    foreach(SOURCE_FILE ${MICROPYTHON_SOURCES})
        get_filename_component(SOURCE_NAME "${SOURCE_FILE}" NAME_WE)
        get_filename_component(SOURCE_DIR "${SOURCE_FILE}" DIRECTORY)

        # Create unique object file name (include parent dir to avoid collisions)
        get_filename_component(PARENT_DIR "${SOURCE_DIR}" NAME)
        set(OBJ_FILE "${MICROPYTHON_OBJ_DIR}/${PARENT_DIR}_${SOURCE_NAME}.obj")

        add_custom_command(
            OUTPUT "${OBJ_FILE}"
            COMMAND "${CLANG_EXECUTABLE}"
                ${MICROPYTHON_CLANG_FLAGS}
                ${MICROPYTHON_INCLUDE_FLAGS}
                -o "${OBJ_FILE}"
                "${SOURCE_FILE}"
            DEPENDS "${SOURCE_FILE}"
            COMMENT "Clang: Compiling ${SOURCE_NAME}.c"
            VERBATIM
        )

        list(APPEND MICROPYTHON_OBJECTS "${OBJ_FILE}")
    endforeach()

    # Create the static library from object files
    set(MICROPYTHON_LIB "${CMAKE_BINARY_DIR}/lib/micropython.lib")

    if(LLVM_AR_EXECUTABLE)
        # Use llvm-ar if available
        add_custom_command(
            OUTPUT "${MICROPYTHON_LIB}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/lib"
            COMMAND "${LLVM_AR_EXECUTABLE}" rcs "${MICROPYTHON_LIB}" ${MICROPYTHON_OBJECTS}
            DEPENDS ${MICROPYTHON_OBJECTS}
            COMMENT "Creating static library micropython.lib with llvm-ar"
            VERBATIM
        )
    else()
        # Fall back to MSVC lib.exe (should work with Clang objects targeting MSVC ABI)
        add_custom_command(
            OUTPUT "${MICROPYTHON_LIB}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/lib"
            COMMAND lib.exe /OUT:"${MICROPYTHON_LIB}" ${MICROPYTHON_OBJECTS}
            DEPENDS ${MICROPYTHON_OBJECTS}
            COMMENT "Creating static library micropython.lib with lib.exe"
            VERBATIM
        )
    endif()

    # Create a custom target that builds the library
    add_custom_target(build_micropython ALL DEPENDS "${MICROPYTHON_LIB}")

    # Create an imported library target
    add_library(micropython STATIC IMPORTED GLOBAL)
    set_target_properties(micropython PROPERTIES
        IMPORTED_LOCATION "${MICROPYTHON_LIB}"
    )
    add_dependencies(micropython build_micropython)

    # Create an interface library for include directories and definitions
    add_library(micropython_interface INTERFACE)
    target_include_directories(micropython_interface INTERFACE
        "${MICROPYTHON_BUILD_DIR}"
        "${MICROPYTHON_GENHDR_DIR}"
        "${MICROPYTHON_DIR}"
        "${MICROPYTHON_DIR}/ports/embed"
        "${MICROPYTHON_DIR}/ports/embed/port"
    )
    # No frozen modules, so no MICROPY_QSTR_EXTRA_POOL needed

    # Link the interface to the main target
    target_link_libraries(micropython INTERFACE micropython_interface)

    message(STATUS "MicroPython will be built with Clang (MSVC mode)")

# =============================================================================
# Emscripten: Build MicroPython for WebAssembly
# =============================================================================
elseif(EMSCRIPTEN)
    message(STATUS "Emscripten detected - building MicroPython for WebAssembly...")

    # Create the MicroPython static library using Emscripten compiler
    add_library(micropython STATIC ${MICROPYTHON_SOURCES})

    # Include directories - order matters for proper header resolution
    target_include_directories(micropython
        PUBLIC
            "${MICROPYTHON_BUILD_DIR}"
            "${MICROPYTHON_GENHDR_DIR}"
            "${MICROPYTHON_DIR}"
            "${MICROPYTHON_DIR}/ports/embed"
            "${MICROPYTHON_DIR}/ports/embed/port"
        PRIVATE
            "${MICROPYTHON_DIR}/py"
            "${MICROPYTHON_DIR}/shared/runtime"
    )

    # Compile definitions
    target_compile_definitions(micropython
        PRIVATE
            MICROPY_INCLUDED
            MP_ENDIANNESS_LITTLE=1
    )

    # Emscripten-specific warning suppressions
    target_compile_options(micropython PRIVATE
        -Wno-unused-parameter
        -Wno-sign-compare
        -Wno-missing-field-initializers
        -Wno-unused-function
        -Wno-implicit-fallthrough
        -Wno-unused-variable
        -Wno-unused-but-set-variable
    )

    set_target_properties(micropython PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        C_STANDARD 99
    )

    message(STATUS "MicroPython will be built with Emscripten (emcc)")

# =============================================================================
# Non-MSVC platforms: Build normally with CMake
# =============================================================================
else()
    # Create the MicroPython static library using native compiler
    add_library(micropython STATIC ${MICROPYTHON_SOURCES})

    # Include directories - order matters for proper header resolution
    target_include_directories(micropython
        PUBLIC
            "${MICROPYTHON_BUILD_DIR}"
            "${MICROPYTHON_GENHDR_DIR}"
            "${MICROPYTHON_DIR}"
            "${MICROPYTHON_DIR}/ports/embed"
            "${MICROPYTHON_DIR}/ports/embed/port"
        PRIVATE
            "${MICROPYTHON_DIR}/py"
            "${MICROPYTHON_DIR}/shared/runtime"
    )

    # Compile definitions
    # No frozen modules, so no MICROPY_QSTR_EXTRA_POOL needed
    target_compile_definitions(micropython
        PRIVATE
            MICROPY_INCLUDED
    )

    # Platform-specific settings
    if(WIN32)
        target_compile_definitions(micropython PRIVATE
            _CRT_SECURE_NO_WARNINGS
            MP_ENDIANNESS_LITTLE=1
        )
    endif()

    # GCC/Clang settings
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(micropython PRIVATE
            -Wno-unused-parameter
            -Wno-sign-compare
            -Wno-missing-field-initializers
            -Wno-unused-function
            -Wno-cast-function-type
            -Wno-implicit-fallthrough
            -Wno-unused-variable
            -Wno-unused-but-set-variable
        )

        # Apple Clang specific
        if(APPLE)
            target_compile_options(micropython PRIVATE
                -Wno-shorten-64-to-32
            )
        endif()
    endif()

    set_target_properties(micropython PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        C_STANDARD 99
    )

    message(STATUS "MicroPython will be built with native compiler")
endif()

# =============================================================================
# Common configuration
# =============================================================================
set(LUPINE_HAS_MICROPYTHON TRUE CACHE BOOL "MicroPython support enabled" FORCE)
add_definitions(-DLUPINE_HAS_MICROPYTHON)

message(STATUS "MicroPython configured successfully")
message(STATUS "  Source dir: ${MICROPYTHON_DIR}")
message(STATUS "  Build dir: ${MICROPYTHON_BUILD_DIR}")
