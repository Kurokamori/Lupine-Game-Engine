# CopyIfExists.cmake
# Helper script to copy a file only if it exists
#
# Usage:
#   cmake -DSOURCE_FILE="path/to/file" -DDEST_DIR="path/to/dest" -P CopyIfExists.cmake

if(NOT DEFINED SOURCE_FILE)
    message(FATAL_ERROR "SOURCE_FILE not defined")
endif()

if(NOT DEFINED DEST_DIR)
    message(FATAL_ERROR "DEST_DIR not defined")
endif()

if(EXISTS "${SOURCE_FILE}")
    get_filename_component(FILENAME "${SOURCE_FILE}" NAME)
    file(COPY "${SOURCE_FILE}" DESTINATION "${DEST_DIR}")
    message(STATUS "Copied: ${FILENAME}")
else()
    message(STATUS "File does not exist, skipping: ${SOURCE_FILE}")
endif()
