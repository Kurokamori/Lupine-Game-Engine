# C API Examples

# Gather all example source files
file(GLOB EXAMPLE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.c")

# Create an executable for each example
foreach(EXAMPLE_SOURCE ${EXAMPLE_SOURCES})
    get_filename_component(EXAMPLE_NAME ${EXAMPLE_SOURCE} NAME_WE)

    add_executable(${EXAMPLE_NAME} ${EXAMPLE_SOURCE})

    target_link_libraries(${EXAMPLE_NAME} PRIVATE lupine_capi)

    # Set C standard
    set_target_properties(${EXAMPLE_NAME} PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED ON
    )
endforeach()
