/**
 * @file lc_particles2d.h
 * @brief Lupine Engine C API - Particles2D Component
 *
 * A CPU-simulated 2D particle emitter.
 * Each live particle is drawn as a textured sprite or a solid colored quad.
 *
 * Features:
 * - Continuous, one-shot, and explosive (burst) emission
 * - Point / Circle / Rectangle emission shapes
 * - Randomised lifetime, velocity, angle, angular velocity, and scale
 * - Directional emission with angular spread, gravity, and linear damping
 * - Color and scale interpolation over each particle's lifetime
 * - Local-space or world-space simulation, speed scaling, and warm-up
 */

#ifndef LUPINE_CAPI_PARTICLES2D_H
#define LUPINE_CAPI_PARTICLES2D_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Emission shape used to randomise the spawn position of new particles.
 */
typedef enum LCParticles2DEmissionShape {
    LC_PARTICLES2D_SHAPE_POINT = 0,      /**< Spawn at the node origin. */
    LC_PARTICLES2D_SHAPE_CIRCLE = 1,     /**< Spawn inside a filled circle. */
    LC_PARTICLES2D_SHAPE_RECTANGLE = 2   /**< Spawn inside a rectangle. */
} LCParticles2DEmissionShape;

/**
 * @brief Default particle shape used when no texture is assigned.
 */
typedef enum LCParticles2DParticleShape {
    LC_PARTICLES2D_PARTICLE_SQUARE = 0,  /**< Solid square. */
    LC_PARTICLES2D_PARTICLE_CIRCLE = 1   /**< Soft-edged circle. */
} LCParticles2DParticleShape;

/* ============================================================================
 * Particles2D Component Functions
 * ============================================================================ */

/**
 * @brief Create a Particles2D component on a node
 * @param node The node to add the component to
 * @param outHandle Output parameter for the component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_particles2d_create(LCNodeHandle node, LCComponentHandle* outHandle);

/* ----- Runtime control ----- */

/** @brief Restart the emitter, clearing all live particles and the cycle. */
LC_API LCResult lc_particles2d_restart(LCComponentHandle handle);

/** @brief Emit a one-off burst of particles immediately. */
LC_API LCResult lc_particles2d_emit_burst(LCComponentHandle handle, int32_t count);

/** @brief Get the number of particles currently alive. */
LC_API LCResult lc_particles2d_get_alive_count(LCComponentHandle handle, int32_t* outCount);

/* ----- Emission ----- */

LC_API LCResult lc_particles2d_set_emitting(LCComponentHandle handle, bool emitting);
LC_API LCResult lc_particles2d_get_emitting(LCComponentHandle handle, bool* outEmitting);

LC_API LCResult lc_particles2d_set_amount(LCComponentHandle handle, int32_t amount);
LC_API LCResult lc_particles2d_get_amount(LCComponentHandle handle, int32_t* outAmount);

LC_API LCResult lc_particles2d_set_one_shot(LCComponentHandle handle, bool oneShot);
LC_API LCResult lc_particles2d_get_one_shot(LCComponentHandle handle, bool* outOneShot);

LC_API LCResult lc_particles2d_set_explosiveness(LCComponentHandle handle, float value);
LC_API LCResult lc_particles2d_get_explosiveness(LCComponentHandle handle, float* outValue);

LC_API LCResult lc_particles2d_set_speed_scale(LCComponentHandle handle, float value);
LC_API LCResult lc_particles2d_get_speed_scale(LCComponentHandle handle, float* outValue);

LC_API LCResult lc_particles2d_set_preprocess(LCComponentHandle handle, float seconds);
LC_API LCResult lc_particles2d_get_preprocess(LCComponentHandle handle, float* outSeconds);

LC_API LCResult lc_particles2d_set_local_space(LCComponentHandle handle, bool localSpace);
LC_API LCResult lc_particles2d_get_local_space(LCComponentHandle handle, bool* outLocalSpace);

/* ----- Lifetime ----- */

LC_API LCResult lc_particles2d_set_lifetime(LCComponentHandle handle, float seconds);
LC_API LCResult lc_particles2d_get_lifetime(LCComponentHandle handle, float* outSeconds);

LC_API LCResult lc_particles2d_set_lifetime_randomness(LCComponentHandle handle, float value);
LC_API LCResult lc_particles2d_get_lifetime_randomness(LCComponentHandle handle, float* outValue);

/* ----- Emission shape ----- */

LC_API LCResult lc_particles2d_set_emission_shape(LCComponentHandle handle, LCParticles2DEmissionShape shape);
LC_API LCResult lc_particles2d_get_emission_shape(LCComponentHandle handle, LCParticles2DEmissionShape* outShape);

LC_API LCResult lc_particles2d_set_emission_radius(LCComponentHandle handle, float radius);
LC_API LCResult lc_particles2d_get_emission_radius(LCComponentHandle handle, float* outRadius);

LC_API LCResult lc_particles2d_set_emission_extents(LCComponentHandle handle, LCVec2 extents);
LC_API LCResult lc_particles2d_get_emission_extents(LCComponentHandle handle, LCVec2* outExtents);

/* ----- Velocity ----- */

LC_API LCResult lc_particles2d_set_direction(LCComponentHandle handle, LCVec2 direction);
LC_API LCResult lc_particles2d_get_direction(LCComponentHandle handle, LCVec2* outDirection);

LC_API LCResult lc_particles2d_set_spread(LCComponentHandle handle, float degrees);
LC_API LCResult lc_particles2d_get_spread(LCComponentHandle handle, float* outDegrees);

LC_API LCResult lc_particles2d_set_initial_velocity_min(LCComponentHandle handle, float value);
LC_API LCResult lc_particles2d_get_initial_velocity_min(LCComponentHandle handle, float* outValue);

LC_API LCResult lc_particles2d_set_initial_velocity_max(LCComponentHandle handle, float value);
LC_API LCResult lc_particles2d_get_initial_velocity_max(LCComponentHandle handle, float* outValue);

LC_API LCResult lc_particles2d_set_gravity(LCComponentHandle handle, LCVec2 gravity);
LC_API LCResult lc_particles2d_get_gravity(LCComponentHandle handle, LCVec2* outGravity);

LC_API LCResult lc_particles2d_set_linear_damping(LCComponentHandle handle, float value);
LC_API LCResult lc_particles2d_get_linear_damping(LCComponentHandle handle, float* outValue);

/* ----- Rotation ----- */

LC_API LCResult lc_particles2d_set_initial_angle_min(LCComponentHandle handle, float value);
LC_API LCResult lc_particles2d_get_initial_angle_min(LCComponentHandle handle, float* outValue);

LC_API LCResult lc_particles2d_set_initial_angle_max(LCComponentHandle handle, float value);
LC_API LCResult lc_particles2d_get_initial_angle_max(LCComponentHandle handle, float* outValue);

LC_API LCResult lc_particles2d_set_angular_velocity_min(LCComponentHandle handle, float value);
LC_API LCResult lc_particles2d_get_angular_velocity_min(LCComponentHandle handle, float* outValue);

LC_API LCResult lc_particles2d_set_angular_velocity_max(LCComponentHandle handle, float value);
LC_API LCResult lc_particles2d_get_angular_velocity_max(LCComponentHandle handle, float* outValue);

/* ----- Display ----- */

LC_API LCResult lc_particles2d_set_particle_size(LCComponentHandle handle, LCVec2 size);
LC_API LCResult lc_particles2d_get_particle_size(LCComponentHandle handle, LCVec2* outSize);

LC_API LCResult lc_particles2d_set_scale_min(LCComponentHandle handle, float value);
LC_API LCResult lc_particles2d_get_scale_min(LCComponentHandle handle, float* outValue);

LC_API LCResult lc_particles2d_set_scale_max(LCComponentHandle handle, float value);
LC_API LCResult lc_particles2d_get_scale_max(LCComponentHandle handle, float* outValue);

LC_API LCResult lc_particles2d_set_scale_end(LCComponentHandle handle, float value);
LC_API LCResult lc_particles2d_get_scale_end(LCComponentHandle handle, float* outValue);

LC_API LCResult lc_particles2d_set_texture_path(LCComponentHandle handle, const char* path);
LC_API LCResult lc_particles2d_get_texture_path(LCComponentHandle handle, char* outBuffer, uint32_t bufferSize, uint32_t* outLength);

LC_API LCResult lc_particles2d_set_color_start(LCComponentHandle handle, LCColor color);
LC_API LCResult lc_particles2d_get_color_start(LCComponentHandle handle, LCColor* outColor);

LC_API LCResult lc_particles2d_set_color_end(LCComponentHandle handle, LCColor color);
LC_API LCResult lc_particles2d_get_color_end(LCComponentHandle handle, LCColor* outColor);

LC_API LCResult lc_particles2d_set_modulate(LCComponentHandle handle, LCColor color);
LC_API LCResult lc_particles2d_get_modulate(LCComponentHandle handle, LCColor* outColor);

LC_API LCResult lc_particles2d_set_particle_shape(LCComponentHandle handle, LCParticles2DParticleShape shape);
LC_API LCResult lc_particles2d_get_particle_shape(LCComponentHandle handle, LCParticles2DParticleShape* outShape);

/**
 * @brief Multi-stop color-over-life ramp, as a JSON string
 *        ({"stops":[{"pos":float,"color":{"r","g","b","a"}},...]}). An empty
 *        string clears the gradient (the emitter falls back to colorStart/End).
 */
LC_API LCResult lc_particles2d_set_color_gradient(LCComponentHandle handle, const char* json);
LC_API LCResult lc_particles2d_get_color_gradient(LCComponentHandle handle, char* outBuffer, uint32_t bufferSize, uint32_t* outLength);

/**
 * @brief Scale-over-life curve, as a JSON string
 *        ({"points":[{"pos":float,"value":float},...]}). An empty string clears
 *        the curve (the emitter falls back to scaleEnd).
 */
LC_API LCResult lc_particles2d_set_scale_curve(LCComponentHandle handle, const char* json);
LC_API LCResult lc_particles2d_get_scale_curve(LCComponentHandle handle, char* outBuffer, uint32_t bufferSize, uint32_t* outLength);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_PARTICLES2D_H */
