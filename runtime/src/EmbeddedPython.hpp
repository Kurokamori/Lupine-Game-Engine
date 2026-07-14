#pragma once

/**
 * Embedded Python Runtime
 *
 * This module handles extracting and loading the embedded Python runtime
 * from within the executable. This allows the game to be distributed as
 * a single executable file with full Python scripting support.
 */

#include <string>

namespace lupine {
namespace runtime {

/**
 * Initialize the embedded Python runtime.
 * On Windows, this extracts the Python DLL from resources if needed.
 * Must be called before any Python/pybind11 operations.
 *
 * @return true if initialization successful
 */
bool InitializeEmbeddedPython();

/**
 * Cleanup the embedded Python runtime.
 * Should be called at application shutdown.
 */
void ShutdownEmbeddedPython();

/**
 * Get the path where embedded Python files are extracted.
 * This is typically a temp directory or adjacent to the executable.
 */
std::string GetEmbeddedPythonPath();

/**
 * Check if we're running from an embedded/exported game
 * (as opposed to the development runtime)
 */
bool IsEmbeddedMode();

} // namespace runtime
} // namespace lupine
