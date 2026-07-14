/**
 * @file lc_character_controller2d.cpp
 * @brief Lupine Engine C API - CharacterController2D implementation
 */

#include "physics/lc_character_controller2d.h"
#include "../core/lc_internal.h"

#include <lupine/components/CharacterController2D.hpp>

namespace {

void SetCharacterController2DError(LCResult code, const char* message) {
    ::SetError(code, message);
}

lupine::math::Vec2 ToEngineVec2(LCVec2 vec) {
    return lupine::math::Vec2(vec.x, vec.y);
}

LCVec2 FromEngineVec2(const lupine::math::Vec2& vec) {
    return LCVec2{vec.x, vec.y};
}

} // anonymous namespace

/* ============================================================================
 * CharacterController2D Creation
 * ============================================================================ */

LC_API LCResult lc_character_controller2d_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetCharacterController2DError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        std::string controllerName = name ? name : "";
        auto controller = std::make_shared<lupine::components::CharacterController2D>(controllerName);
        controller->DefineProperties();
        *out_component = CreateComponentHandle(controller);
        return LC_SUCCESS;
    } catch (...) {
        SetCharacterController2DError(LC_ERROR_INTERNAL_ERROR, "Failed to create CharacterController2D");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Movement Functions
 * ============================================================================ */

LC_API LCResult lc_character_controller2d_move_and_slide(
    LCComponentHandle component,
    LCVec2 velocity,
    float delta_time,
    LCVec2* out_actual_movement
) {
    if (!out_actual_movement) {
        SetCharacterController2DError(LC_ERROR_NULL_POINTER, "out_actual_movement is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCharacterController2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto controller = std::dynamic_pointer_cast<lupine::components::CharacterController2D>(comp);
        if (!controller) {
            SetCharacterController2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CharacterController2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        auto result = controller->MoveAndSlide(ToEngineVec2(velocity), delta_time);
        *out_actual_movement = FromEngineVec2(result);
        return LC_SUCCESS;
    } catch (...) {
        SetCharacterController2DError(LC_ERROR_INTERNAL_ERROR, "Failed to move and slide");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_character_controller2d_move_and_collide(
    LCComponentHandle component,
    LCVec2 velocity,
    float delta_time,
    LCVec2* out_actual_movement,
    bool* out_collided
) {
    if (!out_actual_movement || !out_collided) {
        SetCharacterController2DError(LC_ERROR_NULL_POINTER, "Output parameter is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCharacterController2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto controller = std::dynamic_pointer_cast<lupine::components::CharacterController2D>(comp);
        if (!controller) {
            SetCharacterController2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CharacterController2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        lupine::math::Vec2 actualMovement;
        *out_collided = controller->MoveAndCollide(ToEngineVec2(velocity), delta_time, actualMovement);
        *out_actual_movement = FromEngineVec2(actualMovement);
        return LC_SUCCESS;
    } catch (...) {
        SetCharacterController2DError(LC_ERROR_INTERNAL_ERROR, "Failed to move and collide");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_character_controller2d_set_velocity(LCComponentHandle component, LCVec2 velocity) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCharacterController2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto controller = std::dynamic_pointer_cast<lupine::components::CharacterController2D>(comp);
        if (!controller) {
            SetCharacterController2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CharacterController2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        controller->SetVelocity(ToEngineVec2(velocity));
        return LC_SUCCESS;
    } catch (...) {
        SetCharacterController2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set velocity");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_character_controller2d_get_velocity(LCComponentHandle component, LCVec2* out_velocity) {
    if (!out_velocity) {
        SetCharacterController2DError(LC_ERROR_NULL_POINTER, "out_velocity is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCharacterController2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto controller = std::dynamic_pointer_cast<lupine::components::CharacterController2D>(comp);
        if (!controller) {
            SetCharacterController2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CharacterController2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_velocity = FromEngineVec2(controller->GetVelocity());
        return LC_SUCCESS;
    } catch (...) {
        SetCharacterController2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get velocity");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Ground/Wall/Ceiling Detection
 * ============================================================================ */

LC_API LCResult lc_character_controller2d_is_on_ground(LCComponentHandle component, bool* out_on_ground) {
    if (!out_on_ground) {
        SetCharacterController2DError(LC_ERROR_NULL_POINTER, "out_on_ground is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCharacterController2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto controller = std::dynamic_pointer_cast<lupine::components::CharacterController2D>(comp);
        if (!controller) {
            SetCharacterController2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CharacterController2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_on_ground = controller->IsOnGround();
        return LC_SUCCESS;
    } catch (...) {
        SetCharacterController2DError(LC_ERROR_INTERNAL_ERROR, "Failed to check ground state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_character_controller2d_is_on_wall(LCComponentHandle component, bool* out_on_wall) {
    if (!out_on_wall) {
        SetCharacterController2DError(LC_ERROR_NULL_POINTER, "out_on_wall is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCharacterController2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto controller = std::dynamic_pointer_cast<lupine::components::CharacterController2D>(comp);
        if (!controller) {
            SetCharacterController2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CharacterController2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_on_wall = controller->IsOnWall();
        return LC_SUCCESS;
    } catch (...) {
        SetCharacterController2DError(LC_ERROR_INTERNAL_ERROR, "Failed to check wall state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_character_controller2d_is_on_ceiling(LCComponentHandle component, bool* out_on_ceiling) {
    if (!out_on_ceiling) {
        SetCharacterController2DError(LC_ERROR_NULL_POINTER, "out_on_ceiling is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCharacterController2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto controller = std::dynamic_pointer_cast<lupine::components::CharacterController2D>(comp);
        if (!controller) {
            SetCharacterController2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CharacterController2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_on_ceiling = controller->IsOnCeiling();
        return LC_SUCCESS;
    } catch (...) {
        SetCharacterController2DError(LC_ERROR_INTERNAL_ERROR, "Failed to check ceiling state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_character_controller2d_get_ground_normal(LCComponentHandle component, LCVec2* out_normal) {
    if (!out_normal) {
        SetCharacterController2DError(LC_ERROR_NULL_POINTER, "out_normal is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCharacterController2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto controller = std::dynamic_pointer_cast<lupine::components::CharacterController2D>(comp);
        if (!controller) {
            SetCharacterController2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CharacterController2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_normal = FromEngineVec2(controller->GetGroundNormal());
        return LC_SUCCESS;
    } catch (...) {
        SetCharacterController2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get ground normal");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_character_controller2d_get_wall_normal(LCComponentHandle component, LCVec2* out_normal) {
    if (!out_normal) {
        SetCharacterController2DError(LC_ERROR_NULL_POINTER, "out_normal is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCharacterController2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto controller = std::dynamic_pointer_cast<lupine::components::CharacterController2D>(comp);
        if (!controller) {
            SetCharacterController2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CharacterController2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_normal = FromEngineVec2(controller->GetWallNormal());
        return LC_SUCCESS;
    } catch (...) {
        SetCharacterController2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get wall normal");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Configuration Properties
 * ============================================================================ */

LC_API LCResult lc_character_controller2d_get_gravity(LCComponentHandle component, float* out_gravity) {
    if (!out_gravity) {
        SetCharacterController2DError(LC_ERROR_NULL_POINTER, "out_gravity is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCharacterController2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto controller = std::dynamic_pointer_cast<lupine::components::CharacterController2D>(comp);
        if (!controller) {
            SetCharacterController2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CharacterController2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_gravity = controller->GetGravity();
        return LC_SUCCESS;
    } catch (...) {
        SetCharacterController2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get gravity");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_character_controller2d_set_gravity(LCComponentHandle component, float gravity) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCharacterController2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto controller = std::dynamic_pointer_cast<lupine::components::CharacterController2D>(comp);
        if (!controller) {
            SetCharacterController2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CharacterController2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        controller->SetGravity(gravity);
        return LC_SUCCESS;
    } catch (...) {
        SetCharacterController2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set gravity");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_character_controller2d_get_max_fall_speed(LCComponentHandle component, float* out_speed) {
    if (!out_speed) {
        SetCharacterController2DError(LC_ERROR_NULL_POINTER, "out_speed is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCharacterController2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto controller = std::dynamic_pointer_cast<lupine::components::CharacterController2D>(comp);
        if (!controller) {
            SetCharacterController2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CharacterController2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_speed = controller->GetMaxFallSpeed();
        return LC_SUCCESS;
    } catch (...) {
        SetCharacterController2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get max fall speed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_character_controller2d_set_max_fall_speed(LCComponentHandle component, float speed) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCharacterController2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto controller = std::dynamic_pointer_cast<lupine::components::CharacterController2D>(comp);
        if (!controller) {
            SetCharacterController2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CharacterController2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        controller->SetMaxFallSpeed(speed);
        return LC_SUCCESS;
    } catch (...) {
        SetCharacterController2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set max fall speed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_character_controller2d_get_ground_detection_distance(LCComponentHandle component, float* out_distance) {
    if (!out_distance) {
        SetCharacterController2DError(LC_ERROR_NULL_POINTER, "out_distance is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCharacterController2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto controller = std::dynamic_pointer_cast<lupine::components::CharacterController2D>(comp);
        if (!controller) {
            SetCharacterController2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CharacterController2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_distance = controller->GetGroundDetectionDistance();
        return LC_SUCCESS;
    } catch (...) {
        SetCharacterController2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get ground detection distance");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_character_controller2d_set_ground_detection_distance(LCComponentHandle component, float distance) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCharacterController2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto controller = std::dynamic_pointer_cast<lupine::components::CharacterController2D>(comp);
        if (!controller) {
            SetCharacterController2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CharacterController2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        controller->SetGroundDetectionDistance(distance);
        return LC_SUCCESS;
    } catch (...) {
        SetCharacterController2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set ground detection distance");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_character_controller2d_get_wall_detection_distance(LCComponentHandle component, float* out_distance) {
    if (!out_distance) {
        SetCharacterController2DError(LC_ERROR_NULL_POINTER, "out_distance is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCharacterController2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto controller = std::dynamic_pointer_cast<lupine::components::CharacterController2D>(comp);
        if (!controller) {
            SetCharacterController2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CharacterController2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_distance = controller->GetWallDetectionDistance();
        return LC_SUCCESS;
    } catch (...) {
        SetCharacterController2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get wall detection distance");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_character_controller2d_set_wall_detection_distance(LCComponentHandle component, float distance) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCharacterController2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto controller = std::dynamic_pointer_cast<lupine::components::CharacterController2D>(comp);
        if (!controller) {
            SetCharacterController2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CharacterController2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        controller->SetWallDetectionDistance(distance);
        return LC_SUCCESS;
    } catch (...) {
        SetCharacterController2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set wall detection distance");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_character_controller2d_get_max_slope_angle(LCComponentHandle component, float* out_angle) {
    if (!out_angle) {
        SetCharacterController2DError(LC_ERROR_NULL_POINTER, "out_angle is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCharacterController2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto controller = std::dynamic_pointer_cast<lupine::components::CharacterController2D>(comp);
        if (!controller) {
            SetCharacterController2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CharacterController2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_angle = controller->GetMaxSlopeAngle();
        return LC_SUCCESS;
    } catch (...) {
        SetCharacterController2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get max slope angle");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_character_controller2d_set_max_slope_angle(LCComponentHandle component, float angle) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCharacterController2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto controller = std::dynamic_pointer_cast<lupine::components::CharacterController2D>(comp);
        if (!controller) {
            SetCharacterController2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CharacterController2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        controller->SetMaxSlopeAngle(angle);
        return LC_SUCCESS;
    } catch (...) {
        SetCharacterController2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set max slope angle");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_character_controller2d_get_snap_to_ground(LCComponentHandle component, bool* out_snap) {
    if (!out_snap) {
        SetCharacterController2DError(LC_ERROR_NULL_POINTER, "out_snap is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCharacterController2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto controller = std::dynamic_pointer_cast<lupine::components::CharacterController2D>(comp);
        if (!controller) {
            SetCharacterController2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CharacterController2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_snap = controller->GetSnapToGround();
        return LC_SUCCESS;
    } catch (...) {
        SetCharacterController2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get snap to ground");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_character_controller2d_set_snap_to_ground(LCComponentHandle component, bool snap) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCharacterController2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto controller = std::dynamic_pointer_cast<lupine::components::CharacterController2D>(comp);
        if (!controller) {
            SetCharacterController2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CharacterController2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        controller->SetSnapToGround(snap);
        return LC_SUCCESS;
    } catch (...) {
        SetCharacterController2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set snap to ground");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_character_controller2d_get_max_bounces(LCComponentHandle component, int* out_bounces) {
    if (!out_bounces) {
        SetCharacterController2DError(LC_ERROR_NULL_POINTER, "out_bounces is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCharacterController2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto controller = std::dynamic_pointer_cast<lupine::components::CharacterController2D>(comp);
        if (!controller) {
            SetCharacterController2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CharacterController2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        *out_bounces = controller->GetMaxBounces();
        return LC_SUCCESS;
    } catch (...) {
        SetCharacterController2DError(LC_ERROR_INTERNAL_ERROR, "Failed to get max bounces");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_character_controller2d_set_max_bounces(LCComponentHandle component, int bounces) {
    try {
        auto comp = GetComponent(component);
        if (!comp) {
            SetCharacterController2DError(LC_ERROR_INVALID_HANDLE, "Invalid component handle");
            return LC_ERROR_INVALID_HANDLE;
        }

        auto controller = std::dynamic_pointer_cast<lupine::components::CharacterController2D>(comp);
        if (!controller) {
            SetCharacterController2DError(LC_ERROR_COMPONENT_INVALID_TYPE, "Component is not a CharacterController2D");
            return LC_ERROR_COMPONENT_INVALID_TYPE;
        }

        controller->SetMaxBounces(bounces);
        return LC_SUCCESS;
    } catch (...) {
        SetCharacterController2DError(LC_ERROR_INTERNAL_ERROR, "Failed to set max bounces");
        return LC_ERROR_INTERNAL_ERROR;
    }
}
