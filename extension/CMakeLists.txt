# ============================================================================
# Native C++ Extension SDK ("LupineExtension") — a GDExtension equivalent.
#
# This subdirectory provides:
#   - the stable C ABI header (include/lupine/lupine_extension_interface.h),
#   - the header-only C++ author SDK (sdk/include/lupine/ext/*.hpp),
#   - the lupine_add_extension() CMake helper (cmake/LupineExtension.cmake),
#   - install rules so the SDK can be consumed out of tree,
#   - bundled example extensions.
#
# The host side of the system (ExtensionManager, NativeComponentWrapper) lives in
# lupine_core so every host that links the core can load extensions.
# ============================================================================

# Brings in the Lupine::Extension interface target and lupine_add_extension().
# The module auto-detects the in-source layout under this directory.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/LupineExtension.cmake)

# Install the public SDK headers and the standalone CMake helper so plugin
# authors can build extensions against an installed engine SDK.
install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)
install(DIRECTORY sdk/include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp"
)
install(FILES cmake/LupineExtension.cmake
    DESTINATION lib/cmake/Lupine
)

# Bundled examples (each is a self-contained extension built with the SDK).
option(LUPINE_BUILD_EXTENSION_EXAMPLES "Build the bundled native extension examples" ON)
if(LUPINE_BUILD_EXTENSION_EXAMPLES)
    add_subdirectory(examples/hello_component)
endif()
