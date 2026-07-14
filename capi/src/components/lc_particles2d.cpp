/**
 * @file lc_particles2d.cpp
 * @brief Implementation of Particles2D C API
 */

#include "components/lc_particles2d.h"
#include "../core/lc_internal.h"

#include <lupine/core/Node.hpp>
#include <lupine/components/Particles2D.hpp>

#include <cstring>

using namespace lupine;
using namespace lupine::core;
using namespace lupine::components;

namespace {

lupine::math::Color ToEngineColor(LCColor color) {
    return lupine::math::Color(color.r, color.g, color.b, color.a);
}

LCColor FromEngineColor(const lupine::math::Color& color) {
    return LCColor{color.r, color.g, color.b, color.a};
}

lupine::math::Vec2 ToEngineVec2(LCVec2 v) {
    return lupine::math::Vec2(v.x, v.y);
}

LCVec2 FromEngineVec2(const lupine::math::Vec2& v) {
    return LCVec2{v.x, v.y};
}

Particles2D* GetParticles2D(LCComponentHandle handle) {
    auto comp = GetComponent(handle);
    if (!comp) return nullptr;
    return dynamic_cast<Particles2D*>(comp.get());
}

} // anonymous namespace

/* ============================================================================
 * Creation
 * ============================================================================ */

LC_API LCResult lc_particles2d_create(LCNodeHandle node, LCComponentHandle* outHandle) {
    if (!outHandle) {
        SetError(LC_ERROR_NULL_POINTER, "outHandle is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto nodePtr = GetNode(node);
    if (!nodePtr) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        auto comp = std::make_shared<Particles2D>();
        comp->RegisterProperties();
        nodePtr->AddComponent(comp);
        *outHandle = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to create Particles2D component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Runtime control
 * ============================================================================ */

LC_API LCResult lc_particles2d_restart(LCComponentHandle handle) {
    auto p = GetParticles2D(handle);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    p->Restart();
    return LC_SUCCESS;
}

LC_API LCResult lc_particles2d_emit_burst(LCComponentHandle handle, int32_t count) {
    auto p = GetParticles2D(handle);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    p->EmitBurst(static_cast<int>(count));
    return LC_SUCCESS;
}

LC_API LCResult lc_particles2d_get_alive_count(LCComponentHandle handle, int32_t* outCount) {
    if (!outCount) return LC_ERROR_NULL_POINTER;
    auto p = GetParticles2D(handle);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    *outCount = static_cast<int32_t>(p->GetAliveCount());
    return LC_SUCCESS;
}

/* ============================================================================
 * Property helpers (macros keep the many trivial accessors compact and exact)
 * ============================================================================ */

#define LC_P2D_SET_FLOAT(suffix, Method)                                      \
    LC_API LCResult lc_particles2d_set_##suffix(LCComponentHandle handle, float value) { \
        auto p = GetParticles2D(handle);                                      \
        if (!p) return LC_ERROR_INVALID_HANDLE;                               \
        p->Method(value);                                                     \
        return LC_SUCCESS;                                                    \
    }

#define LC_P2D_GET_FLOAT(suffix, Method)                                      \
    LC_API LCResult lc_particles2d_get_##suffix(LCComponentHandle handle, float* outValue) { \
        if (!outValue) return LC_ERROR_NULL_POINTER;                          \
        auto p = GetParticles2D(handle);                                      \
        if (!p) return LC_ERROR_INVALID_HANDLE;                               \
        *outValue = p->Method();                                             \
        return LC_SUCCESS;                                                    \
    }

#define LC_P2D_FLOAT(suffix, Setter, Getter) \
    LC_P2D_SET_FLOAT(suffix, Setter)         \
    LC_P2D_GET_FLOAT(suffix, Getter)

/* ----- Emission ----- */

LC_API LCResult lc_particles2d_set_emitting(LCComponentHandle handle, bool emitting) {
    auto p = GetParticles2D(handle);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    p->SetEmitting(emitting);
    return LC_SUCCESS;
}
LC_API LCResult lc_particles2d_get_emitting(LCComponentHandle handle, bool* outEmitting) {
    if (!outEmitting) return LC_ERROR_NULL_POINTER;
    auto p = GetParticles2D(handle);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    *outEmitting = p->GetEmitting();
    return LC_SUCCESS;
}

LC_API LCResult lc_particles2d_set_amount(LCComponentHandle handle, int32_t amount) {
    auto p = GetParticles2D(handle);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    p->SetAmount(static_cast<int>(amount));
    return LC_SUCCESS;
}
LC_API LCResult lc_particles2d_get_amount(LCComponentHandle handle, int32_t* outAmount) {
    if (!outAmount) return LC_ERROR_NULL_POINTER;
    auto p = GetParticles2D(handle);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    *outAmount = static_cast<int32_t>(p->GetAmount());
    return LC_SUCCESS;
}

LC_API LCResult lc_particles2d_set_one_shot(LCComponentHandle handle, bool oneShot) {
    auto p = GetParticles2D(handle);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    p->SetOneShot(oneShot);
    return LC_SUCCESS;
}
LC_API LCResult lc_particles2d_get_one_shot(LCComponentHandle handle, bool* outOneShot) {
    if (!outOneShot) return LC_ERROR_NULL_POINTER;
    auto p = GetParticles2D(handle);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    *outOneShot = p->GetOneShot();
    return LC_SUCCESS;
}

LC_P2D_FLOAT(explosiveness, SetExplosiveness, GetExplosiveness)
LC_P2D_FLOAT(speed_scale, SetSpeedScale, GetSpeedScale)
LC_P2D_FLOAT(preprocess, SetPreprocess, GetPreprocess)

LC_API LCResult lc_particles2d_set_local_space(LCComponentHandle handle, bool localSpace) {
    auto p = GetParticles2D(handle);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    p->SetLocalSpace(localSpace);
    return LC_SUCCESS;
}
LC_API LCResult lc_particles2d_get_local_space(LCComponentHandle handle, bool* outLocalSpace) {
    if (!outLocalSpace) return LC_ERROR_NULL_POINTER;
    auto p = GetParticles2D(handle);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    *outLocalSpace = p->GetLocalSpace();
    return LC_SUCCESS;
}

/* ----- Lifetime ----- */

LC_P2D_FLOAT(lifetime, SetLifetime, GetLifetime)
LC_P2D_FLOAT(lifetime_randomness, SetLifetimeRandomness, GetLifetimeRandomness)

/* ----- Emission shape ----- */

LC_API LCResult lc_particles2d_set_emission_shape(LCComponentHandle handle, LCParticles2DEmissionShape shape) {
    auto p = GetParticles2D(handle);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    p->SetEmissionShape(static_cast<Particles2D::EmissionShape>(shape));
    return LC_SUCCESS;
}
LC_API LCResult lc_particles2d_get_emission_shape(LCComponentHandle handle, LCParticles2DEmissionShape* outShape) {
    if (!outShape) return LC_ERROR_NULL_POINTER;
    auto p = GetParticles2D(handle);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    *outShape = static_cast<LCParticles2DEmissionShape>(p->GetEmissionShape());
    return LC_SUCCESS;
}

LC_P2D_FLOAT(emission_radius, SetEmissionRadius, GetEmissionRadius)

LC_API LCResult lc_particles2d_set_emission_extents(LCComponentHandle handle, LCVec2 extents) {
    auto p = GetParticles2D(handle);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    p->SetEmissionExtents(ToEngineVec2(extents));
    return LC_SUCCESS;
}
LC_API LCResult lc_particles2d_get_emission_extents(LCComponentHandle handle, LCVec2* outExtents) {
    if (!outExtents) return LC_ERROR_NULL_POINTER;
    auto p = GetParticles2D(handle);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    *outExtents = FromEngineVec2(p->GetEmissionExtents());
    return LC_SUCCESS;
}

/* ----- Velocity ----- */

LC_API LCResult lc_particles2d_set_direction(LCComponentHandle handle, LCVec2 direction) {
    auto p = GetParticles2D(handle);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    p->SetDirection(ToEngineVec2(direction));
    return LC_SUCCESS;
}
LC_API LCResult lc_particles2d_get_direction(LCComponentHandle handle, LCVec2* outDirection) {
    if (!outDirection) return LC_ERROR_NULL_POINTER;
    auto p = GetParticles2D(handle);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    *outDirection = FromEngineVec2(p->GetDirection());
    return LC_SUCCESS;
}

LC_P2D_FLOAT(spread, SetSpread, GetSpread)
LC_P2D_FLOAT(initial_velocity_min, SetInitialVelocityMin, GetInitialVelocityMin)
LC_P2D_FLOAT(initial_velocity_max, SetInitialVelocityMax, GetInitialVelocityMax)

LC_API LCResult lc_particles2d_set_gravity(LCComponentHandle handle, LCVec2 gravity) {
    auto p = GetParticles2D(handle);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    p->SetGravity(ToEngineVec2(gravity));
    return LC_SUCCESS;
}
LC_API LCResult lc_particles2d_get_gravity(LCComponentHandle handle, LCVec2* outGravity) {
    if (!outGravity) return LC_ERROR_NULL_POINTER;
    auto p = GetParticles2D(handle);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    *outGravity = FromEngineVec2(p->GetGravity());
    return LC_SUCCESS;
}

LC_P2D_FLOAT(linear_damping, SetLinearDamping, GetLinearDamping)

/* ----- Rotation ----- */

LC_P2D_FLOAT(initial_angle_min, SetInitialAngleMin, GetInitialAngleMin)
LC_P2D_FLOAT(initial_angle_max, SetInitialAngleMax, GetInitialAngleMax)
LC_P2D_FLOAT(angular_velocity_min, SetAngularVelocityMin, GetAngularVelocityMin)
LC_P2D_FLOAT(angular_velocity_max, SetAngularVelocityMax, GetAngularVelocityMax)

/* ----- Display ----- */

LC_API LCResult lc_particles2d_set_particle_size(LCComponentHandle handle, LCVec2 size) {
    auto p = GetParticles2D(handle);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    p->SetParticleSize(ToEngineVec2(size));
    return LC_SUCCESS;
}
LC_API LCResult lc_particles2d_get_particle_size(LCComponentHandle handle, LCVec2* outSize) {
    if (!outSize) return LC_ERROR_NULL_POINTER;
    auto p = GetParticles2D(handle);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    *outSize = FromEngineVec2(p->GetParticleSize());
    return LC_SUCCESS;
}

LC_P2D_FLOAT(scale_min, SetScaleMin, GetScaleMin)
LC_P2D_FLOAT(scale_max, SetScaleMax, GetScaleMax)
LC_P2D_FLOAT(scale_end, SetScaleEnd, GetScaleEnd)

LC_API LCResult lc_particles2d_set_texture_path(LCComponentHandle handle, const char* path) {
    auto p = GetParticles2D(handle);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    p->SetTexturePath(path ? std::string(path) : std::string());
    return LC_SUCCESS;
}
LC_API LCResult lc_particles2d_get_texture_path(LCComponentHandle handle, char* outBuffer, uint32_t bufferSize, uint32_t* outLength) {
    if (!outBuffer || bufferSize == 0) return LC_ERROR_NULL_POINTER;
    auto p = GetParticles2D(handle);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    std::string path = p->GetTexturePath();
    CopyStringToBuffer(outBuffer, bufferSize, path.c_str());
    if (outLength) {
        *outLength = static_cast<uint32_t>(path.size());
    }
    return LC_SUCCESS;
}

LC_API LCResult lc_particles2d_set_color_start(LCComponentHandle handle, LCColor color) {
    auto p = GetParticles2D(handle);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    p->SetColorStart(ToEngineColor(color));
    return LC_SUCCESS;
}
LC_API LCResult lc_particles2d_get_color_start(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) return LC_ERROR_NULL_POINTER;
    auto p = GetParticles2D(handle);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    *outColor = FromEngineColor(p->GetColorStart());
    return LC_SUCCESS;
}

LC_API LCResult lc_particles2d_set_color_end(LCComponentHandle handle, LCColor color) {
    auto p = GetParticles2D(handle);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    p->SetColorEnd(ToEngineColor(color));
    return LC_SUCCESS;
}
LC_API LCResult lc_particles2d_get_color_end(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) return LC_ERROR_NULL_POINTER;
    auto p = GetParticles2D(handle);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    *outColor = FromEngineColor(p->GetColorEnd());
    return LC_SUCCESS;
}

LC_API LCResult lc_particles2d_set_modulate(LCComponentHandle handle, LCColor color) {
    auto p = GetParticles2D(handle);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    p->SetModulate(ToEngineColor(color));
    return LC_SUCCESS;
}
LC_API LCResult lc_particles2d_get_modulate(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) return LC_ERROR_NULL_POINTER;
    auto p = GetParticles2D(handle);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    *outColor = FromEngineColor(p->GetModulate());
    return LC_SUCCESS;
}

LC_API LCResult lc_particles2d_set_particle_shape(LCComponentHandle handle, LCParticles2DParticleShape shape) {
    auto p = GetParticles2D(handle);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    p->SetParticleShape(static_cast<ParticleShape>(shape));
    return LC_SUCCESS;
}
LC_API LCResult lc_particles2d_get_particle_shape(LCComponentHandle handle, LCParticles2DParticleShape* outShape) {
    if (!outShape) return LC_ERROR_NULL_POINTER;
    auto p = GetParticles2D(handle);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    *outShape = static_cast<LCParticles2DParticleShape>(p->GetParticleShape());
    return LC_SUCCESS;
}

static LCResult CopyStringOut(const std::string& value, char* outBuffer, uint32_t bufferSize, uint32_t* outLength) {
    if (!outBuffer || bufferSize == 0) return LC_ERROR_NULL_POINTER;
    CopyStringToBuffer(outBuffer, bufferSize, value.c_str());
    if (outLength) {
        *outLength = static_cast<uint32_t>(value.size());
    }
    return LC_SUCCESS;
}

LC_API LCResult lc_particles2d_set_color_gradient(LCComponentHandle handle, const char* json) {
    auto p = GetParticles2D(handle);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    p->SetColorGradientJson(json ? std::string(json) : std::string());
    return LC_SUCCESS;
}
LC_API LCResult lc_particles2d_get_color_gradient(LCComponentHandle handle, char* outBuffer, uint32_t bufferSize, uint32_t* outLength) {
    auto p = GetParticles2D(handle);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    return CopyStringOut(p->GetColorGradientJson(), outBuffer, bufferSize, outLength);
}

LC_API LCResult lc_particles2d_set_scale_curve(LCComponentHandle handle, const char* json) {
    auto p = GetParticles2D(handle);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    p->SetScaleCurveJson(json ? std::string(json) : std::string());
    return LC_SUCCESS;
}
LC_API LCResult lc_particles2d_get_scale_curve(LCComponentHandle handle, char* outBuffer, uint32_t bufferSize, uint32_t* outLength) {
    auto p = GetParticles2D(handle);
    if (!p) return LC_ERROR_INVALID_HANDLE;
    return CopyStringOut(p->GetScaleCurveJson(), outBuffer, bufferSize, outLength);
}

#undef LC_P2D_FLOAT
#undef LC_P2D_GET_FLOAT
#undef LC_P2D_SET_FLOAT
