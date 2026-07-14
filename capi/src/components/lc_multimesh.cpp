/**
 * @file lc_multimesh.cpp
 * @brief Implementation of Lupine Engine C API - MultiMesh Components
 */

#include "components/lc_multimesh.h"
#include "../core/lc_internal.h"

#include <lupine/components/MultiMeshGeneric.hpp>
#include <lupine/components/ScatterMultiMesh.hpp>
#include <lupine/core/Node.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cstring>
#include <algorithm>

using namespace lupine::components;
using namespace lupine::core;
using namespace lupine::math;

namespace {

void SetMultiMeshError(LCResult code, const char* message) {
    ::SetError(code, message);
}

// Helper to get MultiMeshGeneric from handle
MultiMeshGeneric* GetMultiMesh(LCComponentHandle handle) {
    auto component = GetComponent(handle);
    if (!component) {
        return nullptr;
    }
    return dynamic_cast<MultiMeshGeneric*>(component.get());
}

// Helper to get ScatterMultiMesh from handle
ScatterMultiMesh* GetScatterMultiMesh(LCComponentHandle handle) {
    auto component = GetComponent(handle);
    if (!component) {
        return nullptr;
    }
    return dynamic_cast<ScatterMultiMesh*>(component.get());
}

// Convert engine Mat4 to C API LCMat4
LCMat4 FromEngineMat4(const Mat4& mat) {
    LCMat4 result;
    const float* ptr = glm::value_ptr(mat.GetGLM());
    for (int i = 0; i < 16; ++i) {
        result.m[i] = ptr[i];
    }
    return result;
}

// Convert C API LCMat4 to engine Mat4
Mat4 ToEngineMat4(const LCMat4& mat) {
    glm::mat4 glmMat;
    float* ptr = glm::value_ptr(glmMat);
    for (int i = 0; i < 16; ++i) {
        ptr[i] = mat.m[i];
    }
    return Mat4(glmMat);
}

// Convert engine Color to C API LCColor
LCColor FromEngineColor(const Color& color) {
    return LCColor{color.r, color.g, color.b, color.a};
}

// Convert C API LCColor to engine Color
Color ToEngineColor(const LCColor& color) {
    return Color(color.r, color.g, color.b, color.a);
}

// Convert engine Vec4 to C API LCVec4
LCVec4 FromEngineVec4(const Vec4& vec) {
    return LCVec4{vec.x, vec.y, vec.z, vec.w};
}

// Convert C API LCVec4 to engine Vec4
Vec4 ToEngineVec4(const LCVec4& vec) {
    return Vec4(vec.x, vec.y, vec.z, vec.w);
}

// Convert engine Vec3 to C API LCVec3
LCVec3 FromEngineVec3(const Vec3& vec) {
    return LCVec3{vec.x, vec.y, vec.z};
}

// Convert C API LCVec3 to engine Vec3
Vec3 ToEngineVec3(const LCVec3& vec) {
    return Vec3(vec.x, vec.y, vec.z);
}

// Convert engine Vec2 to C API LCVec2
LCVec2 FromEngineVec2(const Vec2& vec) {
    return LCVec2{vec.x, vec.y};
}

// Convert C API LCVec2 to engine Vec2
Vec2 ToEngineVec2(const LCVec2& vec) {
    return Vec2(vec.x, vec.y);
}

// Convert engine ShadowCastingMode to C API LCShadowCastingMode
LCShadowCastingMode FromEngineShadowMode(ShadowCastingMode mode) {
    switch (mode) {
        case ShadowCastingMode::Off: return LC_SHADOW_OFF;
        case ShadowCastingMode::On: return LC_SHADOW_ON;
        case ShadowCastingMode::OnlyShadows: return LC_SHADOW_ONLY_SHADOWS;
        default: return LC_SHADOW_OFF;
    }
}

// Convert C API LCShadowCastingMode to engine ShadowCastingMode
ShadowCastingMode ToEngineShadowMode(LCShadowCastingMode mode) {
    switch (mode) {
        case LC_SHADOW_OFF: return ShadowCastingMode::Off;
        case LC_SHADOW_ON: return ShadowCastingMode::On;
        case LC_SHADOW_ONLY_SHADOWS: return ShadowCastingMode::OnlyShadows;
        default: return ShadowCastingMode::Off;
    }
}

// Convert engine ScatterShapeType to C API LCScatterShapeType
LCScatterShapeType FromEngineScatterShape(ScatterShapeType shape) {
    switch (shape) {
        case ScatterShapeType::Cube: return LC_SCATTER_SHAPE_CUBE;
        case ScatterShapeType::Sphere: return LC_SCATTER_SHAPE_SPHERE;
        default: return LC_SCATTER_SHAPE_CUBE;
    }
}

// Convert C API LCScatterShapeType to engine ScatterShapeType
ScatterShapeType ToEngineScatterShape(LCScatterShapeType shape) {
    switch (shape) {
        case LC_SCATTER_SHAPE_CUBE: return ScatterShapeType::Cube;
        case LC_SCATTER_SHAPE_SPHERE: return ScatterShapeType::Sphere;
        default: return ScatterShapeType::Cube;
    }
}

// Convert engine ClumpingMode to C API LCClumpingMode
LCClumpingMode FromEngineClumpingMode(ClumpingMode mode) {
    switch (mode) {
        case ClumpingMode::None: return LC_CLUMPING_NONE;
        case ClumpingMode::Clustered: return LC_CLUMPING_CLUSTERED;
        case ClumpingMode::Centered: return LC_CLUMPING_CENTERED;
        default: return LC_CLUMPING_NONE;
    }
}

// Convert C API LCClumpingMode to engine ClumpingMode
ClumpingMode ToEngineClumpingMode(LCClumpingMode mode) {
    switch (mode) {
        case LC_CLUMPING_NONE: return ClumpingMode::None;
        case LC_CLUMPING_CLUSTERED: return ClumpingMode::Clustered;
        case LC_CLUMPING_CENTERED: return ClumpingMode::Centered;
        default: return ClumpingMode::None;
    }
}

} // anonymous namespace

/* ============================================================================
 * MultiMeshGeneric Component
 * ============================================================================ */

LC_API LCResult lc_multimesh_create(LCNodeHandle node, LCComponentHandle* outHandle) {
    if (!outHandle) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outHandle is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto nodePtr = GetNode(node);
    if (!nodePtr) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        auto component = std::make_shared<MultiMeshGeneric>();
        component->RegisterProperties(); // Initialize property system before adding to node
        nodePtr->AddComponent(component);
        *outHandle = CreateComponentHandle(component);
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to create MultiMeshGeneric component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_multimesh_load_mesh(LCComponentHandle handle, const char* filepath) {
    if (!filepath) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "filepath is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* multiMesh = GetMultiMesh(handle);
    if (!multiMesh) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid MultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        if (multiMesh->LoadMesh(filepath)) {
            return LC_SUCCESS;
        } else {
            SetMultiMeshError(LC_ERROR_FILE_NOT_FOUND, "Failed to load mesh");
            return LC_ERROR_FILE_NOT_FOUND;
        }

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "LoadMesh failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_multimesh_get_mesh_path(LCComponentHandle handle, char* outPath, uint32_t maxLength) {
    if (!outPath) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outPath is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* multiMesh = GetMultiMesh(handle);
    if (!multiMesh) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid MultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        std::string path = multiMesh->GetMeshPath();
        const size_t copyLen = std::min(path.size(), static_cast<size_t>(maxLength - 1));
        std::memcpy(outPath, path.c_str(), copyLen);
        outPath[copyLen] = '\0';
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "GetMeshPath failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_multimesh_set_mesh_path(LCComponentHandle handle, const char* path) {
    if (!path) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "path is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* multiMesh = GetMultiMesh(handle);
    if (!multiMesh) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid MultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        multiMesh->SetMeshPath(path);
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "SetMeshPath failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ----------------------------------------------------------------------------
 * Material Override
 * ---------------------------------------------------------------------------- */

LC_API LCResult lc_multimesh_get_material_override(LCComponentHandle handle, char* outPath, uint32_t maxLength) {
    if (!outPath) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outPath is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* multiMesh = GetMultiMesh(handle);
    if (!multiMesh) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid MultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        std::string path = multiMesh->GetMaterialOverride();
        const size_t copyLen = std::min(path.size(), static_cast<size_t>(maxLength - 1));
        std::memcpy(outPath, path.c_str(), copyLen);
        outPath[copyLen] = '\0';
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "GetMaterialOverride failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_multimesh_set_material_override(LCComponentHandle handle, const char* path) {
    if (!path) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "path is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* multiMesh = GetMultiMesh(handle);
    if (!multiMesh) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid MultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        multiMesh->SetMaterialOverride(path);
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "SetMaterialOverride failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ----------------------------------------------------------------------------
 * Shadow Settings
 * ---------------------------------------------------------------------------- */

LC_API LCResult lc_multimesh_get_cast_shadow(LCComponentHandle handle, LCShadowCastingMode* outMode) {
    if (!outMode) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outMode is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* multiMesh = GetMultiMesh(handle);
    if (!multiMesh) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid MultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        *outMode = FromEngineShadowMode(multiMesh->GetCastShadow());
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "GetCastShadow failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_multimesh_set_cast_shadow(LCComponentHandle handle, LCShadowCastingMode mode) {
    auto* multiMesh = GetMultiMesh(handle);
    if (!multiMesh) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid MultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        multiMesh->SetCastShadow(ToEngineShadowMode(mode));
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "SetCastShadow failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_multimesh_get_receive_shadow(LCComponentHandle handle, bool* outReceive) {
    if (!outReceive) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outReceive is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* multiMesh = GetMultiMesh(handle);
    if (!multiMesh) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid MultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        *outReceive = multiMesh->GetReceiveShadow();
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "GetReceiveShadow failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_multimesh_set_receive_shadow(LCComponentHandle handle, bool receive) {
    auto* multiMesh = GetMultiMesh(handle);
    if (!multiMesh) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid MultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        multiMesh->SetReceiveShadow(receive);
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "SetReceiveShadow failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ----------------------------------------------------------------------------
 * Instance Management
 * ---------------------------------------------------------------------------- */

LC_API LCResult lc_multimesh_get_instance_count(LCComponentHandle handle, int* outCount) {
    if (!outCount) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outCount is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* multiMesh = GetMultiMesh(handle);
    if (!multiMesh) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid MultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        *outCount = multiMesh->GetInstanceCount();
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "GetInstanceCount failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_multimesh_set_instance_count(LCComponentHandle handle, int count) {
    if (count < 0) {
        SetMultiMeshError(LC_ERROR_INVALID_PARAMETER, "count cannot be negative");
        return LC_ERROR_INVALID_PARAMETER;
    }

    auto* multiMesh = GetMultiMesh(handle);
    if (!multiMesh) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid MultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        multiMesh->SetInstanceCount(count);
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "SetInstanceCount failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_multimesh_get_instance_transform(LCComponentHandle handle, int index, LCMat4* outTransform) {
    if (!outTransform) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outTransform is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* multiMesh = GetMultiMesh(handle);
    if (!multiMesh) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid MultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    if (index < 0 || index >= multiMesh->GetInstanceCount()) {
        SetMultiMeshError(LC_ERROR_INVALID_PARAMETER, "Instance index out of range");
        return LC_ERROR_INVALID_PARAMETER;
    }

    try {
        *outTransform = FromEngineMat4(multiMesh->GetInstanceTransform(index));
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "GetInstanceTransform failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_multimesh_set_instance_transform(LCComponentHandle handle, int index, LCMat4 transform) {
    auto* multiMesh = GetMultiMesh(handle);
    if (!multiMesh) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid MultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    if (index < 0 || index >= multiMesh->GetInstanceCount()) {
        SetMultiMeshError(LC_ERROR_INVALID_PARAMETER, "Instance index out of range");
        return LC_ERROR_INVALID_PARAMETER;
    }

    try {
        multiMesh->SetInstanceTransform(index, ToEngineMat4(transform));
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "SetInstanceTransform failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_multimesh_get_instance_color(LCComponentHandle handle, int index, LCColor* outColor) {
    if (!outColor) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outColor is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* multiMesh = GetMultiMesh(handle);
    if (!multiMesh) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid MultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    if (index < 0 || index >= multiMesh->GetInstanceCount()) {
        SetMultiMeshError(LC_ERROR_INVALID_PARAMETER, "Instance index out of range");
        return LC_ERROR_INVALID_PARAMETER;
    }

    try {
        *outColor = FromEngineColor(multiMesh->GetInstanceColor(index));
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "GetInstanceColor failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_multimesh_set_instance_color(LCComponentHandle handle, int index, LCColor color) {
    auto* multiMesh = GetMultiMesh(handle);
    if (!multiMesh) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid MultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    if (index < 0 || index >= multiMesh->GetInstanceCount()) {
        SetMultiMeshError(LC_ERROR_INVALID_PARAMETER, "Instance index out of range");
        return LC_ERROR_INVALID_PARAMETER;
    }

    try {
        multiMesh->SetInstanceColor(index, ToEngineColor(color));
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "SetInstanceColor failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_multimesh_get_instance_custom_data(LCComponentHandle handle, int index, LCVec4* outData) {
    if (!outData) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outData is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* multiMesh = GetMultiMesh(handle);
    if (!multiMesh) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid MultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    if (index < 0 || index >= multiMesh->GetInstanceCount()) {
        SetMultiMeshError(LC_ERROR_INVALID_PARAMETER, "Instance index out of range");
        return LC_ERROR_INVALID_PARAMETER;
    }

    try {
        *outData = FromEngineVec4(multiMesh->GetInstanceCustomData(index));
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "GetInstanceCustomData failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_multimesh_set_instance_custom_data(LCComponentHandle handle, int index, LCVec4 data) {
    auto* multiMesh = GetMultiMesh(handle);
    if (!multiMesh) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid MultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    if (index < 0 || index >= multiMesh->GetInstanceCount()) {
        SetMultiMeshError(LC_ERROR_INVALID_PARAMETER, "Instance index out of range");
        return LC_ERROR_INVALID_PARAMETER;
    }

    try {
        multiMesh->SetInstanceCustomData(index, ToEngineVec4(data));
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "SetInstanceCustomData failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ----------------------------------------------------------------------------
 * Culling Settings
 * ---------------------------------------------------------------------------- */

LC_API LCResult lc_multimesh_get_cull_per_instance(LCComponentHandle handle, bool* outCull) {
    if (!outCull) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outCull is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* multiMesh = GetMultiMesh(handle);
    if (!multiMesh) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid MultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        *outCull = multiMesh->GetCullPerInstance();
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "GetCullPerInstance failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_multimesh_set_cull_per_instance(LCComponentHandle handle, bool cull) {
    auto* multiMesh = GetMultiMesh(handle);
    if (!multiMesh) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid MultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        multiMesh->SetCullPerInstance(cull);
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "SetCullPerInstance failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_multimesh_get_max_distance(LCComponentHandle handle, float* outDistance) {
    if (!outDistance) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outDistance is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* multiMesh = GetMultiMesh(handle);
    if (!multiMesh) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid MultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        *outDistance = multiMesh->GetMaxDistance();
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "GetMaxDistance failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_multimesh_set_max_distance(LCComponentHandle handle, float distance) {
    auto* multiMesh = GetMultiMesh(handle);
    if (!multiMesh) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid MultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        multiMesh->SetMaxDistance(distance);
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "SetMaxDistance failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_multimesh_get_lod_mesh_path(LCComponentHandle handle, int level, char* outBuffer, size_t bufferSize) {
    if (!outBuffer || bufferSize == 0) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outBuffer is NULL or bufferSize is 0");
        return LC_ERROR_NULL_POINTER;
    }

    if (level < 0 || level >= LC_MULTIMESH_LOD_LEVEL_COUNT) {
        SetMultiMeshError(LC_ERROR_INVALID_PARAMETER, "level out of range");
        return LC_ERROR_INVALID_PARAMETER;
    }

    auto* multiMesh = GetMultiMesh(handle);
    if (!multiMesh) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid MultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        std::string path = multiMesh->GetLodMeshPath(level);
        const size_t copyLen = std::min(path.size(), static_cast<size_t>(bufferSize - 1));
        std::memcpy(outBuffer, path.c_str(), copyLen);
        outBuffer[copyLen] = '\0';
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "GetLodMeshPath failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_multimesh_set_lod_mesh_path(LCComponentHandle handle, int level, const char* path) {
    if (!path) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "path is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    if (level < 0 || level >= LC_MULTIMESH_LOD_LEVEL_COUNT) {
        SetMultiMeshError(LC_ERROR_INVALID_PARAMETER, "level out of range");
        return LC_ERROR_INVALID_PARAMETER;
    }

    auto* multiMesh = GetMultiMesh(handle);
    if (!multiMesh) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid MultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        multiMesh->SetLodMeshPath(level, path);
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "SetLodMeshPath failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_multimesh_get_lod_distance(LCComponentHandle handle, int level, float* outDistance) {
    if (!outDistance) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outDistance is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    if (level < 0 || level >= LC_MULTIMESH_LOD_LEVEL_COUNT) {
        SetMultiMeshError(LC_ERROR_INVALID_PARAMETER, "level out of range");
        return LC_ERROR_INVALID_PARAMETER;
    }

    auto* multiMesh = GetMultiMesh(handle);
    if (!multiMesh) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid MultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        *outDistance = multiMesh->GetLodDistance(level);
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "GetLodDistance failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_multimesh_set_lod_distance(LCComponentHandle handle, int level, float distance) {
    if (level < 0 || level >= LC_MULTIMESH_LOD_LEVEL_COUNT) {
        SetMultiMeshError(LC_ERROR_INVALID_PARAMETER, "level out of range");
        return LC_ERROR_INVALID_PARAMETER;
    }

    auto* multiMesh = GetMultiMesh(handle);
    if (!multiMesh) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid MultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        multiMesh->SetLodDistance(level, distance);
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "SetLodDistance failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_multimesh_get_auto_generate_lods(LCComponentHandle handle, bool* outEnabled) {
    if (!outEnabled) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outEnabled is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    auto* multiMesh = GetMultiMesh(handle);
    if (!multiMesh) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid MultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    try {
        *outEnabled = multiMesh->GetAutoGenerateLods();
        return LC_SUCCESS;
    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "GetAutoGenerateLods failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_multimesh_set_auto_generate_lods(LCComponentHandle handle, bool enable) {
    auto* multiMesh = GetMultiMesh(handle);
    if (!multiMesh) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid MultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    try {
        multiMesh->SetAutoGenerateLods(enable);
        return LC_SUCCESS;
    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "SetAutoGenerateLods failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_multimesh_get_auto_lod_reduction(LCComponentHandle handle, float* outReduction) {
    if (!outReduction) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outReduction is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    auto* multiMesh = GetMultiMesh(handle);
    if (!multiMesh) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid MultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    try {
        *outReduction = multiMesh->GetAutoLodReduction();
        return LC_SUCCESS;
    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "GetAutoLodReduction failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_multimesh_set_auto_lod_reduction(LCComponentHandle handle, float reduction) {
    auto* multiMesh = GetMultiMesh(handle);
    if (!multiMesh) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid MultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    try {
        multiMesh->SetAutoLodReduction(reduction);
        return LC_SUCCESS;
    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "SetAutoLodReduction failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_multimesh_get_base_rotation(LCComponentHandle handle, LCVec3* outRotation) {
    if (!outRotation) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outRotation is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    auto* multiMesh = GetMultiMesh(handle);
    if (!multiMesh) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid MultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    try {
        *outRotation = FromEngineVec3(multiMesh->GetBaseRotation());
        return LC_SUCCESS;
    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "GetBaseRotation failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_multimesh_set_base_rotation(LCComponentHandle handle, LCVec3 rotation) {
    auto* multiMesh = GetMultiMesh(handle);
    if (!multiMesh) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid MultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    try {
        multiMesh->SetBaseRotation(ToEngineVec3(rotation));
        return LC_SUCCESS;
    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "SetBaseRotation failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_multimesh_get_base_scale(LCComponentHandle handle, LCVec3* outScale) {
    if (!outScale) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outScale is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    auto* multiMesh = GetMultiMesh(handle);
    if (!multiMesh) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid MultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    try {
        *outScale = FromEngineVec3(multiMesh->GetBaseScale());
        return LC_SUCCESS;
    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "GetBaseScale failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_multimesh_set_base_scale(LCComponentHandle handle, LCVec3 scale) {
    auto* multiMesh = GetMultiMesh(handle);
    if (!multiMesh) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid MultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    try {
        multiMesh->SetBaseScale(ToEngineVec3(scale));
        return LC_SUCCESS;
    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "SetBaseScale failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * ScatterMultiMesh Component
 * ============================================================================ */

LC_API LCResult lc_scatter_multimesh_create(LCNodeHandle node, LCComponentHandle* outHandle) {
    if (!outHandle) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outHandle is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto nodePtr = GetNode(node);
    if (!nodePtr) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        auto component = std::make_shared<ScatterMultiMesh>();
        component->RegisterProperties(); // Initialize property system before adding to node
        nodePtr->AddComponent(component);
        *outHandle = CreateComponentHandle(component);
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "Failed to create ScatterMultiMesh component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ----------------------------------------------------------------------------
 * Debug Visualization
 * ---------------------------------------------------------------------------- */

LC_API LCResult lc_scatter_multimesh_get_show_debug_shape(LCComponentHandle handle, bool* outShow) {
    if (!outShow) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outShow is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        *outShow = scatter->GetShowDebugShape();
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "GetShowDebugShape failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scatter_multimesh_set_show_debug_shape(LCComponentHandle handle, bool show) {
    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        scatter->SetShowDebugShape(show);
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "SetShowDebugShape failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scatter_multimesh_get_debug_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outColor is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        *outColor = FromEngineColor(scatter->GetDebugColor());
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "GetDebugColor failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scatter_multimesh_set_debug_color(LCComponentHandle handle, LCColor color) {
    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        scatter->SetDebugColor(ToEngineColor(color));
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "SetDebugColor failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ----------------------------------------------------------------------------
 * Scatter Shape Settings
 * ---------------------------------------------------------------------------- */

LC_API LCResult lc_scatter_multimesh_get_scatter_shape(LCComponentHandle handle, LCScatterShapeType* outShape) {
    if (!outShape) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outShape is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        *outShape = FromEngineScatterShape(scatter->GetScatterShape());
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "GetScatterShape failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scatter_multimesh_set_scatter_shape(LCComponentHandle handle, LCScatterShapeType shape) {
    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        scatter->SetScatterShape(ToEngineScatterShape(shape));
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "SetScatterShape failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scatter_multimesh_get_cube_size(LCComponentHandle handle, LCVec3* outSize) {
    if (!outSize) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outSize is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        *outSize = FromEngineVec3(scatter->GetCubeSize());
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "GetCubeSize failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scatter_multimesh_set_cube_size(LCComponentHandle handle, LCVec3 size) {
    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        scatter->SetCubeSize(ToEngineVec3(size));
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "SetCubeSize failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scatter_multimesh_get_radius(LCComponentHandle handle, float* outRadius) {
    if (!outRadius) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outRadius is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        *outRadius = scatter->GetRadius();
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "GetRadius failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scatter_multimesh_set_radius(LCComponentHandle handle, float radius) {
    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        scatter->SetRadius(radius);
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "SetRadius failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ----------------------------------------------------------------------------
 * Scatter Count
 * ---------------------------------------------------------------------------- */

LC_API LCResult lc_scatter_multimesh_get_scatter_count(LCComponentHandle handle, int* outCount) {
    if (!outCount) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outCount is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        *outCount = scatter->GetScatterCount();
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "GetScatterCount failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scatter_multimesh_set_scatter_count(LCComponentHandle handle, int count) {
    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        scatter->SetScatterCount(count);
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "SetScatterCount failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ----------------------------------------------------------------------------
 * Clumping Settings
 * ---------------------------------------------------------------------------- */

LC_API LCResult lc_scatter_multimesh_get_clumping_mode(LCComponentHandle handle, LCClumpingMode* outMode) {
    if (!outMode) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outMode is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        *outMode = FromEngineClumpingMode(scatter->GetClumpingMode());
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "GetClumpingMode failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scatter_multimesh_set_clumping_mode(LCComponentHandle handle, LCClumpingMode mode) {
    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        scatter->SetClumpingMode(ToEngineClumpingMode(mode));
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "SetClumpingMode failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scatter_multimesh_get_clump_factor(LCComponentHandle handle, float* outFactor) {
    if (!outFactor) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outFactor is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        *outFactor = scatter->GetClumpFactor();
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "GetClumpFactor failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scatter_multimesh_set_clump_factor(LCComponentHandle handle, float factor) {
    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        scatter->SetClumpFactor(factor);
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "SetClumpFactor failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scatter_multimesh_get_clump_count(LCComponentHandle handle, int* outCount) {
    if (!outCount) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outCount is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        *outCount = scatter->GetClumpCount();
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "GetClumpCount failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scatter_multimesh_set_clump_count(LCComponentHandle handle, int count) {
    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        scatter->SetClumpCount(count);
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "SetClumpCount failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scatter_multimesh_get_clump_radius(LCComponentHandle handle, float* outRadius) {
    if (!outRadius) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outRadius is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        *outRadius = scatter->GetClumpRadius();
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "GetClumpRadius failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scatter_multimesh_set_clump_radius(LCComponentHandle handle, float radius) {
    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        scatter->SetClumpRadius(radius);
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "SetClumpRadius failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ----------------------------------------------------------------------------
 * Variation Settings
 * ---------------------------------------------------------------------------- */

LC_API LCResult lc_scatter_multimesh_get_scale_range(LCComponentHandle handle, LCVec2* outRange) {
    if (!outRange) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outRange is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        *outRange = FromEngineVec2(scatter->GetScaleRange());
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "GetScaleRange failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scatter_multimesh_set_scale_range(LCComponentHandle handle, LCVec2 range) {
    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        scatter->SetScaleRange(ToEngineVec2(range));
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "SetScaleRange failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scatter_multimesh_get_uniform_scale(LCComponentHandle handle, bool* outUniform) {
    if (!outUniform) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outUniform is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        *outUniform = scatter->GetUniformScale();
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "GetUniformScale failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scatter_multimesh_set_uniform_scale(LCComponentHandle handle, bool uniform) {
    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        scatter->SetUniformScale(uniform);
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "SetUniformScale failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scatter_multimesh_get_rotation_variation(LCComponentHandle handle, LCVec3* outVariation) {
    if (!outVariation) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outVariation is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        *outVariation = FromEngineVec3(scatter->GetRotationVariation());
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "GetRotationVariation failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scatter_multimesh_set_rotation_variation(LCComponentHandle handle, LCVec3 variation) {
    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        scatter->SetRotationVariation(ToEngineVec3(variation));
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "SetRotationVariation failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scatter_multimesh_get_align_to_surface(LCComponentHandle handle, bool* outAlign) {
    if (!outAlign) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outAlign is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        *outAlign = scatter->GetAlignToSurface();
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "GetAlignToSurface failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scatter_multimesh_set_align_to_surface(LCComponentHandle handle, bool align) {
    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        scatter->SetAlignToSurface(align);
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "SetAlignToSurface failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ----------------------------------------------------------------------------
 * Color Variation
 * ---------------------------------------------------------------------------- */

LC_API LCResult lc_scatter_multimesh_get_color_variation_enabled(LCComponentHandle handle, bool* outEnabled) {
    if (!outEnabled) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outEnabled is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        *outEnabled = scatter->GetColorVariationEnabled();
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "GetColorVariationEnabled failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scatter_multimesh_set_color_variation_enabled(LCComponentHandle handle, bool enabled) {
    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        scatter->SetColorVariationEnabled(enabled);
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "SetColorVariationEnabled failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scatter_multimesh_get_base_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outColor is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        *outColor = FromEngineColor(scatter->GetBaseColor());
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "GetBaseColor failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scatter_multimesh_set_base_color(LCComponentHandle handle, LCColor color) {
    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        scatter->SetBaseColor(ToEngineColor(color));
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "SetBaseColor failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scatter_multimesh_get_color_variation(LCComponentHandle handle, float* outVariation) {
    if (!outVariation) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outVariation is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        *outVariation = scatter->GetColorVariation();
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "GetColorVariation failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scatter_multimesh_set_color_variation(LCComponentHandle handle, float variation) {
    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        scatter->SetColorVariation(variation);
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "SetColorVariation failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ----------------------------------------------------------------------------
 * Randomization
 * ---------------------------------------------------------------------------- */

LC_API LCResult lc_scatter_multimesh_get_random_seed(LCComponentHandle handle, int* outSeed) {
    if (!outSeed) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outSeed is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        *outSeed = scatter->GetRandomSeed();
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "GetRandomSeed failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scatter_multimesh_set_random_seed(LCComponentHandle handle, int seed) {
    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        scatter->SetRandomSeed(seed);
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "SetRandomSeed failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scatter_multimesh_get_use_random_seed(LCComponentHandle handle, bool* outUse) {
    if (!outUse) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outUse is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        *outUse = scatter->GetUseRandomSeed();
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "GetUseRandomSeed failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scatter_multimesh_set_use_random_seed(LCComponentHandle handle, bool use) {
    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        scatter->SetUseRandomSeed(use);
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "SetUseRandomSeed failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ----------------------------------------------------------------------------
 * Regeneration
 * ---------------------------------------------------------------------------- */

LC_API LCResult lc_scatter_multimesh_regenerate(LCComponentHandle handle) {
    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        scatter->RegenerateScatter();
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "RegenerateScatter failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_scatter_multimesh_needs_regeneration(LCComponentHandle handle, bool* outNeeds) {
    if (!outNeeds) {
        SetMultiMeshError(LC_ERROR_NULL_POINTER, "outNeeds is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto* scatter = GetScatterMultiMesh(handle);
    if (!scatter) {
        SetMultiMeshError(LC_ERROR_INVALID_HANDLE, "Invalid ScatterMultiMesh handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        *outNeeds = scatter->NeedsRegeneration();
        return LC_SUCCESS;

    } catch (...) {
        SetMultiMeshError(LC_ERROR_INTERNAL_ERROR, "NeedsRegeneration failed");
        return LC_ERROR_INTERNAL_ERROR;
    }
}
