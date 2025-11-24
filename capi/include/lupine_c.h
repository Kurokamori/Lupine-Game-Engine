/**
 * @file lupine_c.h
 * @brief Lupine Engine C API - Main umbrella header
 *
 * Include this header to get access to all Lupine Engine C API functionality.
 *
 * @example basic_example.c
 * @code
 * #include <lupine_c.h>
 * #include <stdio.h>
 *
 * int main(void) {
 *     // Initialize engine
 *     LCResult result = lc_init();
 *     if (result != LC_SUCCESS) {
 *         printf("Failed to initialize: %s\n", lc_get_last_error());
 *         return 1;
 *     }
 *
 *     // Use math functions
 *     LCVec3 a = lc_vec3(1.0f, 2.0f, 3.0f);
 *     LCVec3 b = lc_vec3(4.0f, 5.0f, 6.0f);
 *     LCVec3 c = lc_vec3_add(a, b);
 *
 *     printf("Result: %.2f, %.2f, %.2f\n", c.x, c.y, c.z);
 *
 *     // Shutdown engine
 *     lc_shutdown();
 *     return 0;
 * }
 * @endcode
 */

#ifndef LUPINE_C_H
#define LUPINE_C_H

/* Core functionality */
#include "core/lc_core.h"

/* Math types and operations */
#include "math/lc_math.h"

/* Node system and hierarchy */
#include "core/lc_node.h"

/* Scene management */
#include "core/lc_scene.h"

/* Rendering */
#include "rendering/lc_camera.h"

/* Components */
#include "components/lc_light.h"
#include "components/lc_rendering.h"

/* Input */
#include "input/lc_input.h"

/* Audio */
#include "audio/lc_audio.h"

/* Physics */
#include "physics/lc_physics2d.h"

/* Utility */
#include "utility/lc_utility.h"

/* UI */
#include "ui/lc_ui.h"
#include "ui/lc_button.h"

/* Future modules will be included here:
 * #include "rendering/lc_gfx.h"
 * #include "rendering/lc_mesh.h"
 * #include "rendering/lc_material.h"
 * #include "physics/lc_physics2d.h"
 * #include "physics/lc_physics3d.h"
 * #include "input/lc_input.h"
 * #include "audio/lc_audio.h"
 * #include "assets/lc_assets.h"
 * #include "runtime/lc_runtime.h"
 */

#endif /* LUPINE_C_H */
