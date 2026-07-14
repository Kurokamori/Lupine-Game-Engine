#pragma once

// Platform-specific OpenGL context management
// Abstracts WGL (Windows) and NSOpenGLContext (macOS) behind a common interface

namespace lupine {

#ifdef __APPLE__

// macOS platform functions (implemented in GfxDeviceOpenGL_macOS.mm)

/**
 * Create an OpenGL context for the given window/view handle
 * @param windowHandle NSView* (or NSWindow* - will extract contentView)
 * @param vsync Enable vertical sync
 * @param isolated If true, don't share resources with other contexts
 * @return NSOpenGLContext* cast to void*, or nullptr on failure
 */
void* platformCreateGLContext(void* windowHandle, bool vsync, bool isolated);

/**
 * Destroy an OpenGL context
 * @param windowHandle The window handle used during creation
 * @param contextHandle The context handle returned by platformCreateGLContext
 */
void platformDestroyGLContext(void* windowHandle, void* contextHandle);

/**
 * Make a context current for the calling thread
 * @param windowHandle The window handle (for platform-specific lookups)
 * @param contextHandle The context to make current, or nullptr to clear
 * @return true on success
 */
bool platformMakeContextCurrent(void* windowHandle, void* contextHandle);

/**
 * Swap the front and back buffers (present)
 * @param windowHandle The window handle
 * @param contextHandle The context handle
 */
void platformSwapBuffers(void* windowHandle, void* contextHandle);

/**
 * Update the context after view resize
 * @param windowHandle The window handle
 * @param contextHandle The context handle
 */
void platformUpdateContext(void* windowHandle, void* contextHandle);

/**
 * Check if a context is currently active on this thread
 * @param contextHandle The context to check
 * @return true if the context is current
 */
bool platformIsContextCurrent(void* contextHandle);

#endif // __APPLE__

} // namespace lupine
