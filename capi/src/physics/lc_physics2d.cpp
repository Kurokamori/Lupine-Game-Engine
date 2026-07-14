
#include "physics/lc_physics2d.h"
#include "../core/lc_internal.h"

#include <lupine/components/RigidBody2DComponent.hpp>
#include <lupine/components/StaticBody2DComponent.hpp>
#include <lupine/components/KinematicBody2DComponent.hpp>

namespace {

void SetPhysicsError(LCResult code, const char* message) {
    ::SetError(code, message);

// Convert C API Vec2 to engine Vec2
}
lupine::math::Vec2 ToEngineVec2(LCVec2 vec) {
    return lupine::math::Vec2(vec.x, vec.y);

// Convert engine Vec2 to C API Vec2
}
LCVec2 FromEngineVec2(const lupine::math::Vec2& vec) {
    return LCVec2{vec.x, vec.y};

}
} // anonymous namespace


/* ============================================================================
 * RigidBody2D Functions
 * ============================================================================ */

LC_API LCResult lc_rigid_body2d_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetPhysicsError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        std::string bodyName = name ? name : "";
        auto body = std::make_shared<lupine::components::RigidBody2DComponent>(bodyName);
        body->DefineProperties();  // Initialize properties before use
        *out_component = CreateComponentHandle(body);
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to create RigidBody2D");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_rigid_body2d_get_mass(LCComponentHandle component, float* out_mass) {
    if (!out_mass) {
        SetPhysicsError(LC_ERROR_NULL_POINTER, "out_mass is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_mass = body->GetMass();
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to get mass");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_rigid_body2d_get_gravity_scale(LCComponentHandle component, float* out_scale) {
    if (!out_scale) {
        SetPhysicsError(LC_ERROR_NULL_POINTER, "out_scale is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_scale = body->GetGravityScale();
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to get gravity scale");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_rigid_body2d_set_gravity_scale(LCComponentHandle component, float scale) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetGravityScale(scale);
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to set gravity scale");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_rigid_body2d_get_linear_damping(LCComponentHandle component, float* out_damping) {
    if (!out_damping) {
        SetPhysicsError(LC_ERROR_NULL_POINTER, "out_damping is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_damping = body->GetLinearDamping();
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to get linear damping");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_rigid_body2d_set_linear_damping(LCComponentHandle component, float damping) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetLinearDamping(damping);
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to set linear damping");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_rigid_body2d_get_angular_damping(LCComponentHandle component, float* out_damping) {
    if (!out_damping) {
        SetPhysicsError(LC_ERROR_NULL_POINTER, "out_damping is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_damping = body->GetAngularDamping();
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to get angular damping");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_rigid_body2d_set_angular_damping(LCComponentHandle component, float damping) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetAngularDamping(damping);
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to set angular damping");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_rigid_body2d_get_fixed_rotation(LCComponentHandle component, bool* out_fixed) {
    if (!out_fixed) {
        SetPhysicsError(LC_ERROR_NULL_POINTER, "out_fixed is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_fixed = body->GetFixedRotation();
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to get fixed rotation");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_rigid_body2d_set_fixed_rotation(LCComponentHandle component, bool fixed) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetFixedRotation(fixed);
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to set fixed rotation");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_rigid_body2d_get_bullet(LCComponentHandle component, bool* out_bullet) {
    if (!out_bullet) {
        SetPhysicsError(LC_ERROR_NULL_POINTER, "out_bullet is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_bullet = body->GetBullet();
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to get bullet mode");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_rigid_body2d_set_bullet(LCComponentHandle component, bool bullet) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetBullet(bullet);
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to set bullet mode");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_rigid_body2d_get_can_sleep(LCComponentHandle component, bool* out_can_sleep) {
    if (!out_can_sleep) {
        SetPhysicsError(LC_ERROR_NULL_POINTER, "out_can_sleep is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_can_sleep = body->GetCanSleep();
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to get can sleep");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_rigid_body2d_set_can_sleep(LCComponentHandle component, bool can_sleep) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetCanSleep(can_sleep);
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to set can sleep");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_rigid_body2d_get_gravity_enabled(LCComponentHandle component, bool* out_enabled) {
    if (!out_enabled) {
        SetPhysicsError(LC_ERROR_NULL_POINTER, "out_enabled is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_enabled = body->GetGravityEnabled();
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to get gravity enabled");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_rigid_body2d_set_gravity_enabled(LCComponentHandle component, bool enabled) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetGravityEnabled(enabled);
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to set gravity enabled");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_rigid_body2d_apply_force(LCComponentHandle component, LCVec2 force, LCVec2 point) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->ApplyForce(ToEngineVec2(force), ToEngineVec2(point));
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to apply force");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_rigid_body2d_apply_force_to_center(LCComponentHandle component, LCVec2 force) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->ApplyForceToCenter(ToEngineVec2(force));
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to apply force to center");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_rigid_body2d_apply_torque(LCComponentHandle component, float torque) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->ApplyTorque(torque);
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to apply torque");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_rigid_body2d_apply_linear_impulse(LCComponentHandle component, LCVec2 impulse, LCVec2 point) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->ApplyLinearImpulse(ToEngineVec2(impulse), ToEngineVec2(point));
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to apply linear impulse");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_rigid_body2d_apply_linear_impulse_to_center(LCComponentHandle component, LCVec2 impulse) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->ApplyLinearImpulseToCenter(ToEngineVec2(impulse));
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to apply linear impulse to center");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_rigid_body2d_apply_angular_impulse(LCComponentHandle component, float impulse) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->ApplyAngularImpulse(impulse);
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to apply angular impulse");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_rigid_body2d_get_linear_velocity(LCComponentHandle component, LCVec2* out_velocity) {
    if (!out_velocity) {
        SetPhysicsError(LC_ERROR_NULL_POINTER, "out_velocity is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_velocity = FromEngineVec2(body->GetLinearVelocity());
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to get linear velocity");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_rigid_body2d_set_linear_velocity(LCComponentHandle component, LCVec2 velocity) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetLinearVelocity(ToEngineVec2(velocity));
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to set linear velocity");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_rigid_body2d_get_angular_velocity(LCComponentHandle component, float* out_omega) {
    if (!out_omega) {
        SetPhysicsError(LC_ERROR_NULL_POINTER, "out_omega is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_omega = body->GetAngularVelocity();
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to get angular velocity");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_rigid_body2d_set_angular_velocity(LCComponentHandle component, float omega) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::RigidBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a RigidBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetAngularVelocity(omega);
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to set angular velocity");
        return LC_ERROR_INTERNAL_ERROR;
    }

/* ============================================================================
 * StaticBody2D Functions
 * ============================================================================ */

}
LC_API LCResult lc_static_body2d_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetPhysicsError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        std::string bodyName = name ? name : "";
        auto body = std::make_shared<lupine::components::StaticBody2DComponent>(bodyName);
        body->DefineProperties();  // Initialize properties before use
        *out_component = CreateComponentHandle(body);
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to create StaticBody2D");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_static_body2d_get_constant_linear_velocity(LCComponentHandle component, LCVec2* out_velocity) {
    if (!out_velocity) {
        SetPhysicsError(LC_ERROR_NULL_POINTER, "out_velocity is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::StaticBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a StaticBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_velocity = FromEngineVec2(body->GetConstantLinearVelocity());
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to get constant linear velocity");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_static_body2d_set_constant_linear_velocity(LCComponentHandle component, LCVec2 velocity) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::StaticBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a StaticBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetConstantLinearVelocity(ToEngineVec2(velocity));
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to set constant linear velocity");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_static_body2d_get_constant_angular_velocity(LCComponentHandle component, float* out_velocity) {
    if (!out_velocity) {
        SetPhysicsError(LC_ERROR_NULL_POINTER, "out_velocity is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::StaticBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a StaticBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_velocity = body->GetConstantAngularVelocity();
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to get constant angular velocity");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_static_body2d_set_constant_angular_velocity(LCComponentHandle component, float velocity) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::StaticBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a StaticBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetConstantAngularVelocity(velocity);
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to set constant angular velocity");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_static_body2d_set_position(LCComponentHandle component, LCVec2 position) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::StaticBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a StaticBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetPosition(ToEngineVec2(position));
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to set position");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_static_body2d_set_rotation(LCComponentHandle component, float angle) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::StaticBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a StaticBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetRotation(angle);
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to set rotation");
        return LC_ERROR_INTERNAL_ERROR;
    }

/* ============================================================================
 * KinematicBody2D Functions
 * ============================================================================ */

}
LC_API LCResult lc_kinematic_body2d_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetPhysicsError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        std::string bodyName = name ? name : "";
        auto body = std::make_shared<lupine::components::KinematicBody2DComponent>(bodyName);
        body->DefineProperties();  // Initialize properties before use
        *out_component = CreateComponentHandle(body);
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to create KinematicBody2D");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_kinematic_body2d_get_gravity_enabled(LCComponentHandle component, bool* out_enabled) {
    if (!out_enabled) {
        SetPhysicsError(LC_ERROR_NULL_POINTER, "out_enabled is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::KinematicBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a KinematicBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_enabled = body->GetGravityEnabled();
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to get gravity enabled");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_kinematic_body2d_set_gravity_enabled(LCComponentHandle component, bool enabled) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::KinematicBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a KinematicBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetGravityEnabled(enabled);
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to set gravity enabled");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_kinematic_body2d_get_gravity_scale(LCComponentHandle component, float* out_scale) {
    if (!out_scale) {
        SetPhysicsError(LC_ERROR_NULL_POINTER, "out_scale is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::KinematicBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a KinematicBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_scale = body->GetGravityScale();
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to get gravity scale");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_kinematic_body2d_set_gravity_scale(LCComponentHandle component, float scale) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::KinematicBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a KinematicBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetGravityScale(scale);
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to set gravity scale");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_kinematic_body2d_get_linear_velocity(LCComponentHandle component, LCVec2* out_velocity) {
    if (!out_velocity) {
        SetPhysicsError(LC_ERROR_NULL_POINTER, "out_velocity is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::KinematicBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a KinematicBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_velocity = FromEngineVec2(body->GetLinearVelocity());
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to get linear velocity");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_kinematic_body2d_set_linear_velocity(LCComponentHandle component, LCVec2 velocity) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::KinematicBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a KinematicBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetLinearVelocity(ToEngineVec2(velocity));
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to set linear velocity");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_kinematic_body2d_get_angular_velocity(LCComponentHandle component, float* out_omega) {
    if (!out_omega) {
        SetPhysicsError(LC_ERROR_NULL_POINTER, "out_omega is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::KinematicBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a KinematicBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_omega = body->GetAngularVelocity();
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to get angular velocity");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_kinematic_body2d_set_angular_velocity(LCComponentHandle component, float omega) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::KinematicBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a KinematicBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->SetAngularVelocity(omega);
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to set angular velocity");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_kinematic_body2d_move_by(LCComponentHandle component, LCVec2 delta) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::KinematicBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a KinematicBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->MoveBy(ToEngineVec2(delta));
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to move by delta");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_kinematic_body2d_rotate_by(LCComponentHandle component, float delta_angle) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetPhysicsError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto body = std::dynamic_pointer_cast<lupine::components::KinematicBody2DComponent>(comp);
        if (!body) {
            SetPhysicsError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a KinematicBody2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        body->RotateBy(delta_angle);
        return LC_SUCCESS;
    } catch (...) {
        SetPhysicsError(LC_ERROR_INTERNAL_ERROR, "Failed to rotate by delta");
        return LC_ERROR_INTERNAL_ERROR;
    }


}