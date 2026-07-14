/**
 * @file lc_debug_draw.h
 * @brief Lupine Engine C API - Immediate-mode debug drawing
 *
 * These functions enqueue debug primitives (lines, rays, boxes, spheres,
 * circles and text) into the engine's debug draw queue. Each primitive accepts
 * an optional duration: 0 draws for a single frame, a positive value keeps the
 * primitive alive for that many seconds.
 */

#ifndef LUPINE_CAPI_DEBUG_DRAW_H
#define LUPINE_CAPI_DEBUG_DRAW_H

#include "../core/lc_core.h"
#include "../math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Draw a 3D line segment
 * @param start Start point
 * @param end End point
 * @param color Line color
 * @param duration Seconds to keep the primitive alive (0 = one frame)
 * @threadsafety Thread-safe
 */
LC_API void lc_debug_draw_line(LCVec3 start, LCVec3 end, LCColor color, float duration);

/**
 * @brief Draw a 2D line segment (z = 0)
 * @param start Start point
 * @param end End point
 * @param color Line color
 * @param duration Seconds to keep the primitive alive (0 = one frame)
 * @threadsafety Thread-safe
 */
LC_API void lc_debug_draw_line_2d(LCVec2 start, LCVec2 end, LCColor color, float duration);

/**
 * @brief Draw a 3D ray from an origin along a direction
 * @param origin Ray origin
 * @param direction Ray direction (and length)
 * @param color Ray color
 * @param duration Seconds to keep the primitive alive (0 = one frame)
 * @threadsafety Thread-safe
 */
LC_API void lc_debug_draw_ray(LCVec3 origin, LCVec3 direction, LCColor color, float duration);

/**
 * @brief Draw an axis-aligned box
 * @param center Box center
 * @param size Box full extents (width, height, depth)
 * @param color Box color
 * @param duration Seconds to keep the primitive alive (0 = one frame)
 * @threadsafety Thread-safe
 */
LC_API void lc_debug_draw_box(LCVec3 center, LCVec3 size, LCColor color, float duration);

/**
 * @brief Draw a wireframe sphere
 * @param center Sphere center
 * @param radius Sphere radius
 * @param color Sphere color
 * @param duration Seconds to keep the primitive alive (0 = one frame)
 * @threadsafety Thread-safe
 */
LC_API void lc_debug_draw_sphere(LCVec3 center, float radius, LCColor color, float duration);

/**
 * @brief Draw a circle in a plane defined by a normal
 * @param center Circle center
 * @param normal Plane normal
 * @param radius Circle radius
 * @param color Circle color
 * @param duration Seconds to keep the primitive alive (0 = one frame)
 * @threadsafety Thread-safe
 */
LC_API void lc_debug_draw_circle(LCVec3 center, LCVec3 normal, float radius, LCColor color, float duration);

/**
 * @brief Draw debug text at a 3D position
 * @param position Text anchor position
 * @param text Text to draw
 * @param color Text color
 * @param duration Seconds to keep the primitive alive (0 = one frame)
 * @threadsafety Thread-safe
 */
LC_API void lc_debug_draw_text(LCVec3 position, const char* text, LCColor color, float duration);

/**
 * @brief Draw debug text at a 2D position (z = 0)
 * @param position Text anchor position
 * @param text Text to draw
 * @param color Text color
 * @param duration Seconds to keep the primitive alive (0 = one frame)
 * @threadsafety Thread-safe
 */
LC_API void lc_debug_draw_text_2d(LCVec2 position, const char* text, LCColor color, float duration);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_DEBUG_DRAW_H */
