# Lupine Console Test Application

# Collect all console test source files
file(GLOB CONSOLE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

# Configure icon resource for Windows
set(LUPINE_CONSOLE_ICON_PATH "${CMAKE_SOURCE_DIR}/icon.ico" CACHE FILEPATH "Path to icon file for console executable")

# Create the console executable
if(WIN32)
    # Add resource file for Windows icon
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/resource.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/resource.rc"
        @ONLY
    )

    add_executable(lupine_console
        ${CONSOLE_SOURCES}
        ${CMAKE_CURRENT_BINARY_DIR}/resource.rc
    )
else()
    add_executable(lupine_console
        ${CONSOLE_SOURCES}
    )
endif()

# Link to core and engine libraries
target_link_libraries(lupine_console
    PRIVATE
        lupine_core
        lupine_engine_lib
        # Link Python embedding for standalone executable
        pybind11::embed
)

# Set target properties
set_target_properties(lupine_console PROPERTIES
    OUTPUT_NAME "lupine_console"
    DEBUG_POSTFIX "d"
)

# Platform-specific settings
if(WIN32)
    # Windows console application
    set_target_properties(lupine_console PROPERTIES
        WIN32_EXECUTABLE FALSE
    )
elseif(APPLE)
    # macOS console application
    set_target_properties(lupine_console PROPERTIES
        MACOSX_BUNDLE FALSE
    )
endif()

message(STATUS "Console application configured")
message(STATUS "  Sources: ${CONSOLE_SOURCES}")
if(WIN32)
    message(STATUS "  Icon: ${LUPINE_CONSOLE_ICON_PATH}")
    message(STATUS "  To customize the icon, set -DLUPINE_CONSOLE_ICON_PATH=path/to/your/icon.ico")
endif()
