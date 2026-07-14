// macOS-specific OpenGL context management using NSOpenGLContext
// This file provides platform support for OpenGL on macOS, working with both
// SDL2 windows (runtime) and PyQt6 embedded NSView widgets (editor)

#ifdef __APPLE__

#import <Cocoa/Cocoa.h>
#import <OpenGL/OpenGL.h>
#import <OpenGL/gl3.h>
#include "lupine/rendering/backends/opengl/OpenGLPlatform.hpp"
#include "lupine/logger/Logger.hpp"
#include <unordered_map>

namespace lupine {

// Helper to suppress deprecated OpenGL warnings on macOS
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

struct MacOSGLContext {
    NSOpenGLContext* glContext = nil;
    NSOpenGLPixelFormat* pixelFormat = nil;
    NSView* view = nil;
    bool ownsContext = true;
};

static std::unordered_map<void*, MacOSGLContext> s_macContexts;
static NSOpenGLContext* s_currentContext = nil;
static NSOpenGLContext* s_shareContext = nil;

void* platformCreateGLContext(void* windowHandle, bool vsync, bool isolated) {
    @autoreleasepool {
        NSView* view = (__bridge NSView*)windowHandle;
        if (!view) {
            LOG_ERROR(LogCategory::Render, "OpenGL macOS: Invalid window handle (NSView is null)");
            return nullptr;
        }

        // Log view info for debugging
        LOG_INFO(LogCategory::Render, "OpenGL macOS: NSView pointer={}, frame={}x{}, class={}",
                 windowHandle,
                 view.frame.size.width, view.frame.size.height,
                 [NSStringFromClass([view class]) UTF8String]);

        // Create pixel format attributes for OpenGL 4.1 Core Profile (highest on macOS)
        NSOpenGLPixelFormatAttribute attrs[] = {
            NSOpenGLPFADoubleBuffer,
            NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion4_1Core,
            NSOpenGLPFAColorSize, 24,
            NSOpenGLPFAAlphaSize, 8,
            NSOpenGLPFADepthSize, 24,
            NSOpenGLPFAStencilSize, 8,
            NSOpenGLPFAAccelerated,
            NSOpenGLPFANoRecovery,
            0
        };

        NSOpenGLPixelFormat* pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
        if (!pixelFormat) {
            // Fallback to OpenGL 3.2 Core Profile if 4.1 not available
            LOG_WARN(LogCategory::Render, "OpenGL macOS: 4.1 Core Profile not available, falling back to 3.2");
            NSOpenGLPixelFormatAttribute fallbackAttrs[] = {
                NSOpenGLPFADoubleBuffer,
                NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
                NSOpenGLPFAColorSize, 24,
                NSOpenGLPFAAlphaSize, 8,
                NSOpenGLPFADepthSize, 24,
                NSOpenGLPFAStencilSize, 8,
                NSOpenGLPFAAccelerated,
                0
            };
            pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:fallbackAttrs];
        }

        if (!pixelFormat) {
            LOG_ERROR(LogCategory::Render, "OpenGL macOS: Failed to create pixel format");
            return nullptr;
        }

        // Create context, optionally sharing with existing context
        NSOpenGLContext* shareWith = (!isolated && s_shareContext) ? s_shareContext : nil;
        NSOpenGLContext* glContext = [[NSOpenGLContext alloc] initWithFormat:pixelFormat shareContext:shareWith];

        if (!glContext) {
            LOG_ERROR(LogCategory::Render, "OpenGL macOS: Failed to create NSOpenGLContext");
            return nullptr;
        }

        // Set as share context for future contexts if not isolated
        if (!isolated && !s_shareContext) {
            s_shareContext = glContext;
            LOG_INFO(LogCategory::Render, "OpenGL macOS: Set as share context for resource sharing");
        }

        // Attach context to the view
        [glContext setView:view];

        // Make context current to configure it
        [glContext makeCurrentContext];
        s_currentContext = glContext;

        // Configure vsync
        GLint swapInterval = vsync ? 1 : 0;
        [glContext setValues:&swapInterval forParameter:NSOpenGLContextParameterSwapInterval];

        // Store context info
        MacOSGLContext ctxInfo;
        ctxInfo.glContext = glContext;
        ctxInfo.pixelFormat = pixelFormat;
        ctxInfo.view = view;
        ctxInfo.ownsContext = true;
        s_macContexts[windowHandle] = ctxInfo;

        LOG_INFO(LogCategory::Render, "OpenGL macOS: Created NSOpenGLContext successfully (vsync={})", vsync);

        return (__bridge void*)glContext;
    }
}

void platformDestroyGLContext(void* windowHandle, void* contextHandle) {
    @autoreleasepool {
        auto it = s_macContexts.find(windowHandle);
        if (it == s_macContexts.end()) {
            return;
        }

        MacOSGLContext& ctxInfo = it->second;

        // Clear current context if this is active
        if (s_currentContext == ctxInfo.glContext) {
            [NSOpenGLContext clearCurrentContext];
            s_currentContext = nil;
        }

        // Clear share context if this was it
        if (s_shareContext == ctxInfo.glContext) {
            s_shareContext = nil;
        }

        // Release resources
        if (ctxInfo.glContext) {
            [ctxInfo.glContext clearDrawable];
            ctxInfo.glContext = nil;
        }

        ctxInfo.pixelFormat = nil;
        s_macContexts.erase(it);

        LOG_INFO(LogCategory::Render, "OpenGL macOS: Destroyed context for window {}", windowHandle);
    }
}

bool platformMakeContextCurrent(void* windowHandle, void* contextHandle) {
    @autoreleasepool {
        if (!contextHandle) {
            [NSOpenGLContext clearCurrentContext];
            s_currentContext = nil;
            return true;
        }

        NSOpenGLContext* glContext = (__bridge NSOpenGLContext*)contextHandle;

        if (s_currentContext != glContext) {
            [glContext makeCurrentContext];
            s_currentContext = glContext;
        }

        return true;
    }
}

void platformSwapBuffers(void* windowHandle, void* contextHandle) {
    @autoreleasepool {
        if (!contextHandle) {
            return;
        }

        NSOpenGLContext* glContext = (__bridge NSOpenGLContext*)contextHandle;
        [glContext flushBuffer];
    }
}

void platformUpdateContext(void* windowHandle, void* contextHandle) {
    @autoreleasepool {
        auto it = s_macContexts.find(windowHandle);
        if (it == s_macContexts.end()) {
            return;
        }

        NSOpenGLContext* glContext = (__bridge NSOpenGLContext*)contextHandle;
        if (glContext) {
            // Update context when view size changes
            [glContext update];
        }
    }
}

bool platformIsContextCurrent(void* contextHandle) {
    if (!contextHandle) {
        return s_currentContext == nil;
    }
    NSOpenGLContext* glContext = (__bridge NSOpenGLContext*)contextHandle;
    return s_currentContext == glContext;
}

#pragma clang diagnostic pop

} // namespace lupine

#endif // __APPLE__
