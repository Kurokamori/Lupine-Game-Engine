
#include "physics/lc_physics3d.h"
#include "../core/lc_internal.h"

#include <lupine/components/RigidBody3DComponent.hpp>
#include <lupine/components/StaticBody3DComponent.hpp>
#include <lupine/components/KinematicBody3DComponent.hpp>

namespace {

void SetPhysics3DError(LCResult code, const char* message) {
    ::SetError(code, message);
}

// Convert C API Vec3 to engine Vec3
lupine::math::Vec3 ToEngineVec3(LCVec3 vec) {
    return lupine::math::Vec3(vec.x, vec.y, vec.z);
}

// Convert engine Vec3 to C API Vec3
LCVec3 FromEngineVec3(const lupine::math::Vec3& vec) {
    return LCVec3{vec.x, vec.y, vec.z};
}

// Convert C API Quat to engine Quat
lupine::math::Quat ToEngineQuat(LCQuat q) {
    return lupine::math::Quat(q.w, q.x, q.y, q.z);
}

// Convert engine Quat to C API Quat
LCQuat FromEngineQuat(const lupine::math::Quat& q) {
    return LCQuat{q.x(), q.y(), q.z(), q.w()};
}

} // anonymous namespace


/* ============================================================================
 * RigidBody3D Functions
 * ============================================================================ */

LC_API LCResult lc_rigid_body3d_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetPhysics3DError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        std::string bodyName = name ? name : "";
        auto body = std::make_shared<lupine::components::RigidBody3DComponent>(bodyName);
        body->DefineProperties();  // Initialize properties before use
        *out_component = CreateComponentHandle(body);
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to create RigidBody3D");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_rigid_body3d_get_mass(LCComponentHandle component, float* out_mass) {
    if (!out_mass) {
        SetPhysics3DError(LC_ERROR_NULL_POINTER, "out_mass is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_mass = body->GetMass();
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get mass");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_rigid_body3d_set_mass(LCComponentHandle component, float mass) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetMass(mass);
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set mass");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_rigid_body3d_get_gravity_scale(LCComponentHandle component, float* out_scale) {
    if (!out_scale) {
        SetPhysics3DError(LC_ERROR_NULL_POINTER, "out_scale is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_scale = body->GetGravityScale();
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get gravity scale");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_rigid_body3d_set_gravity_scale(LCComponentHandle component, float scale) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetGravityScale(scale);
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set gravity scale");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_rigid_body3d_get_linear_damping(LCComponentHandle component, float* out_damping) {
    if (!out_damping) {
        SetPhysics3DError(LC_ERROR_NULL_POINTER, "out_damping is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_damping = body->GetLinearDamping();
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get linear damping");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_rigid_body3d_set_linear_damping(LCComponentHandle component, float damping) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetLinearDamping(damping);
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set linear damping");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_rigid_body3d_get_angular_damping(LCComponentHandle component, float* out_damping) {
    if (!out_damping) {
        SetPhysics3DError(LC_ERROR_NULL_POINTER, "out_damping is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_damping = body->GetAngularDamping();
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get angular damping");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_rigid_body3d_set_angular_damping(LCComponentHandle component, float damping) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetAngularDamping(damping);
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set angular damping");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_rigid_body3d_get_lock_rotation_x(LCComponentHandle component, bool* out_locked) {
    if (!out_locked) {
        SetPhysics3DError(LC_ERROR_NULL_POINTER, "out_locked is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_locked = body->GetLockRotationX();
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get lock rotation X");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_rigid_body3d_set_lock_rotation_x(LCComponentHandle component, bool locked) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetLockRotationX(locked);
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set lock rotation X");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_rigid_body3d_get_lock_rotation_y(LCComponentHandle component, bool* out_locked) {
    if (!out_locked) {
        SetPhysics3DError(LC_ERROR_NULL_POINTER, "out_locked is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_locked = body->GetLockRotationY();
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get lock rotation Y");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_rigid_body3d_set_lock_rotation_y(LCComponentHandle component, bool locked) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetLockRotationY(locked);
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set lock rotation Y");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_rigid_body3d_get_lock_rotation_z(LCComponentHandle component, bool* out_locked) {
    if (!out_locked) {
        SetPhysics3DError(LC_ERROR_NULL_POINTER, "out_locked is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_locked = body->GetLockRotationZ();
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get lock rotation Z");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_rigid_body3d_set_lock_rotation_z(LCComponentHandle component, bool locked) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetLockRotationZ(locked);
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set lock rotation Z");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_rigid_body3d_get_can_sleep(LCComponentHandle component, bool* out_can_sleep) {
    if (!out_can_sleep) {
        SetPhysics3DError(LC_ERROR_NULL_POINTER, "out_can_sleep is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_can_sleep = body->GetCanSleep();
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get can sleep");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_rigid_body3d_set_can_sleep(LCComponentHandle component, bool can_sleep) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetCanSleep(can_sleep);
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set can sleep");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_rigid_body3d_get_gravity_enabled(LCComponentHandle component, bool* out_enabled) {
    if (!out_enabled) {
        SetPhysics3DError(LC_ERROR_NULL_POINTER, "out_enabled is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_enabled = body->GetGravityEnabled();
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get gravity enabled");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_rigid_body3d_set_gravity_enabled(LCComponentHandle component, bool enabled) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetGravityEnabled(enabled);
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set gravity enabled");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_rigid_body3d_get_use_ccd(LCComponentHandle component, bool* out_use_ccd) {
    if (!out_use_ccd) {
        SetPhysics3DError(LC_ERROR_NULL_POINTER, "out_use_ccd is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_use_ccd = body->GetUseCCD();
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get use CCD");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_rigid_body3d_set_use_ccd(LCComponentHandle component, bool use_ccd) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetUseCCD(use_ccd);
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set use CCD");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_rigid_body3d_apply_force(LCComponentHandle component, LCVec3 force, LCVec3 point) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->ApplyForce(ToEngineVec3(force), ToEngineVec3(point));
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to apply force");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_rigid_body3d_apply_force_to_center(LCComponentHandle component, LCVec3 force) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->ApplyForceToCenter(ToEngineVec3(force));
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to apply force to center");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_rigid_body3d_apply_torque(LCComponentHandle component, LCVec3 torque) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->ApplyTorque(ToEngineVec3(torque));
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to apply torque");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_rigid_body3d_apply_impulse(LCComponentHandle component, LCVec3 impulse, LCVec3 point) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->ApplyImpulse(ToEngineVec3(impulse), ToEngineVec3(point));
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to apply impulse");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_rigid_body3d_apply_impulse_to_center(LCComponentHandle component, LCVec3 impulse) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->ApplyImpulseToCenter(ToEngineVec3(impulse));
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to apply impulse to center");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_rigid_body3d_apply_torque_impulse(LCComponentHandle component, LCVec3 torque) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->ApplyTorqueImpulse(ToEngineVec3(torque));
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to apply torque impulse");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_rigid_body3d_get_linear_velocity(LCComponentHandle component, LCVec3* out_velocity) {
    if (!out_velocity) {
        SetPhysics3DError(LC_ERROR_NULL_POINTER, "out_velocity is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_velocity = FromEngineVec3(body->GetLinearVelocity());
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get linear velocity");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_rigid_body3d_set_linear_velocity(LCComponentHandle component, LCVec3 velocity) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetLinearVelocity(ToEngineVec3(velocity));
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set linear velocity");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_rigid_body3d_get_angular_velocity(LCComponentHandle component, LCVec3* out_omega) {
    if (!out_omega) {
        SetPhysics3DError(LC_ERROR_NULL_POINTER, "out_omega is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_omega = FromEngineVec3(body->GetAngularVelocity());
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get angular velocity");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_rigid_body3d_set_angular_velocity(LCComponentHandle component, LCVec3 omega) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetAngularVelocity(ToEngineVec3(omega));
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set angular velocity");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_rigid_body3d_clear_forces(LCComponentHandle component) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->ClearForces();
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to clear forces");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * StaticBody3D Functions
 * ============================================================================ */

LC_API LCResult lc_static_body3d_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetPhysics3DError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        std::string bodyName = name ? name : "";
        auto body = std::make_shared<lupine::components::StaticBody3DComponent>(bodyName);
        body->DefineProperties();  // Initialize properties before use
        *out_component = CreateComponentHandle(body);
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to create StaticBody3D");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_static_body3d_get_constant_linear_velocity(LCComponentHandle component, LCVec3* out_velocity) {
    if (!out_velocity) {
        SetPhysics3DError(LC_ERROR_NULL_POINTER, "out_velocity is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::StaticBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a StaticBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_velocity = FromEngineVec3(body->GetConstantLinearVelocity());
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get constant linear velocity");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_static_body3d_set_constant_linear_velocity(LCComponentHandle component, LCVec3 velocity) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::StaticBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a StaticBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetConstantLinearVelocity(ToEngineVec3(velocity));
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set constant linear velocity");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_static_body3d_get_constant_angular_velocity(LCComponentHandle component, LCVec3* out_velocity) {
    if (!out_velocity) {
        SetPhysics3DError(LC_ERROR_NULL_POINTER, "out_velocity is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::StaticBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a StaticBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_velocity = FromEngineVec3(body->GetConstantAngularVelocity());
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get constant angular velocity");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_static_body3d_set_constant_angular_velocity(LCComponentHandle component, LCVec3 velocity) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::StaticBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a StaticBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetConstantAngularVelocity(ToEngineVec3(velocity));
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set constant angular velocity");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_static_body3d_set_position(LCComponentHandle component, LCVec3 position) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::StaticBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a StaticBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetPosition(ToEngineVec3(position));
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set position");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_static_body3d_set_rotation(LCComponentHandle component, LCQuat rotation) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::StaticBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a StaticBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetRotation(ToEngineQuat(rotation));
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set rotation");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * KinematicBody3D Functions
 * ============================================================================ */

LC_API LCResult lc_kinematic_body3d_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetPhysics3DError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        std::string bodyName = name ? name : "";
        auto body = std::make_shared<lupine::components::KinematicBody3DComponent>(bodyName);
        body->DefineProperties();  // Initialize properties before use
        *out_component = CreateComponentHandle(body);
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to create KinematicBody3D");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_kinematic_body3d_get_gravity_enabled(LCComponentHandle component, bool* out_enabled) {
    if (!out_enabled) {
        SetPhysics3DError(LC_ERROR_NULL_POINTER, "out_enabled is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::KinematicBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a KinematicBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_enabled = body->GetGravityEnabled();
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get gravity enabled");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_kinematic_body3d_set_gravity_enabled(LCComponentHandle component, bool enabled) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::KinematicBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a KinematicBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetGravityEnabled(enabled);
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set gravity enabled");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_kinematic_body3d_get_gravity_scale(LCComponentHandle component, float* out_scale) {
    if (!out_scale) {
        SetPhysics3DError(LC_ERROR_NULL_POINTER, "out_scale is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::KinematicBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a KinematicBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_scale = body->GetGravityScale();
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get gravity scale");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_kinematic_body3d_set_gravity_scale(LCComponentHandle component, float scale) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::KinematicBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a KinematicBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetGravityScale(scale);
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set gravity scale");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_kinematic_body3d_get_linear_velocity(LCComponentHandle component, LCVec3* out_velocity) {
    if (!out_velocity) {
        SetPhysics3DError(LC_ERROR_NULL_POINTER, "out_velocity is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::KinematicBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a KinematicBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_velocity = FromEngineVec3(body->GetLinearVelocity());
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get linear velocity");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_kinematic_body3d_set_linear_velocity(LCComponentHandle component, LCVec3 velocity) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::KinematicBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a KinematicBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetLinearVelocity(ToEngineVec3(velocity));
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set linear velocity");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_kinematic_body3d_get_angular_velocity(LCComponentHandle component, LCVec3* out_omega) {
    if (!out_omega) {
        SetPhysics3DError(LC_ERROR_NULL_POINTER, "out_omega is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::KinematicBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a KinematicBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_omega = FromEngineVec3(body->GetAngularVelocity());
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to get angular velocity");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_kinematic_body3d_set_angular_velocity(LCComponentHandle component, LCVec3 omega) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::KinematicBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a KinematicBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetAngularVelocity(ToEngineVec3(omega));
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to set angular velocity");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_kinematic_body3d_move_by(LCComponentHandle component, LCVec3 delta) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::KinematicBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a KinematicBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->MoveBy(ToEngineVec3(delta));
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to move by delta");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_kinematic_body3d_rotate_by(LCComponentHandle component, LCQuat delta_rotation) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysics3DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::KinematicBody3DComponent>(comp);
        if (!body) {
            SetPhysics3DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a KinematicBody3D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->RotateBy(ToEngineQuat(delta_rotation));
        return LC_SUCCESS;
    } catch (...) {
        SetPhysics3DError(LC_ERROR_INTERNAL_ERROR, "Failed to rotate by delta");
        return LC_ERROR_INTERNAL_ERROR;
    }
}
