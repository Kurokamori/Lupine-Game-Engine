/**
 * @file lc_line2d.cpp
 * @brief Implementation of Line2D C API
 */

#include "components/lc_line2d.h"
#include "../core/lc_internal.h"

#include <lupine/core/Node.hpp>
#include <lupine/components/Line2D.hpp>

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

Line2D::CapStyle ToEngineCapStyle(LCLineCapStyle style) {
    return static_cast<Line2D::CapStyle>(style);
}

LCLineCapStyle FromEngineCapStyle(Line2D::CapStyle style) {
    return static_cast<LCLineCapStyle>(style);
}

Line2D::JoinStyle ToEngineJoinStyle(LCLineJoinStyle style) {
    return static_cast<Line2D::JoinStyle>(style);
}

LCLineJoinStyle FromEngineJoinStyle(Line2D::JoinStyle style) {
    return static_cast<LCLineJoinStyle>(style);
}

Line2D* GetLine2D(LCComponentHandle handle) {
    auto comp = GetComponent(handle);
    if (!comp) return nullptr;
    return dynamic_cast<Line2D*>(comp.get());
}

} // anonymous namespace

/* ============================================================================
 * Line2D Component Functions
 * ============================================================================ */

LC_API LCResult lc_line2d_create(LCNodeHandle node, LCComponentHandle* outHandle) {
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
        auto comp = std::make_shared<Line2D>();
        comp->RegisterProperties();
        nodePtr->AddComponent(comp);
        *outHandle = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to create Line2D component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ----- Point Management ----- */

LC_API LCResult lc_line2d_get_point_count(LCComponentHandle handle, uint32_t* outCount) {
    if (!outCount) return LC_ERROR_NULL_POINTER;
    auto line = GetLine2D(handle);
    if (!line) return LC_ERROR_INVALID_HANDLE;
    *outCount = static_cast<uint32_t>(line->GetPointCount());
    return LC_SUCCESS;
}

LC_API LCResult lc_line2d_get_point(LCComponentHandle handle, uint32_t index, LCVec2* outPoint) {
    if (!outPoint) return LC_ERROR_NULL_POINTER;
    auto line = GetLine2D(handle);
    if (!line) return LC_ERROR_INVALID_HANDLE;
    if (index >= line->GetPointCount()) return LC_ERROR_INVALID_PARAMETER;
    *outPoint = FromEngineVec2(line->GetPoint(index));
    return LC_SUCCESS;
}

LC_API LCResult lc_line2d_set_point(LCComponentHandle handle, uint32_t index, LCVec2 point) {
    auto line = GetLine2D(handle);
    if (!line) return LC_ERROR_INVALID_HANDLE;
    if (index >= line->GetPointCount()) return LC_ERROR_INVALID_PARAMETER;
    line->SetPoint(index, ToEngineVec2(point));
    return LC_SUCCESS;
}

LC_API LCResult lc_line2d_add_point(LCComponentHandle handle, LCVec2 point) {
    auto line = GetLine2D(handle);
    if (!line) return LC_ERROR_INVALID_HANDLE;
    line->AddPoint(ToEngineVec2(point));
    return LC_SUCCESS;
}

LC_API LCResult lc_line2d_insert_point(LCComponentHandle handle, uint32_t index, LCVec2 point) {
    auto line = GetLine2D(handle);
    if (!line) return LC_ERROR_INVALID_HANDLE;
    if (index > line->GetPointCount()) return LC_ERROR_INVALID_PARAMETER;
    line->InsertPoint(index, ToEngineVec2(point));
    return LC_SUCCESS;
}

LC_API LCResult lc_line2d_remove_point(LCComponentHandle handle, uint32_t index) {
    auto line = GetLine2D(handle);
    if (!line) return LC_ERROR_INVALID_HANDLE;
    if (index >= line->GetPointCount()) return LC_ERROR_INVALID_PARAMETER;
    line->RemovePoint(index);
    return LC_SUCCESS;
}

LC_API LCResult lc_line2d_clear_points(LCComponentHandle handle) {
    auto line = GetLine2D(handle);
    if (!line) return LC_ERROR_INVALID_HANDLE;
    line->ClearPoints();
    return LC_SUCCESS;
}

LC_API LCResult lc_line2d_set_points(LCComponentHandle handle, const LCVec2* points, uint32_t count) {
    if (!points && count > 0) return LC_ERROR_NULL_POINTER;
    auto line = GetLine2D(handle);
    if (!line) return LC_ERROR_INVALID_HANDLE;

    std::vector<lupine::math::Vec2> enginePoints;
    enginePoints.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        enginePoints.push_back(ToEngineVec2(points[i]));
    }
    line->SetPoints(enginePoints);
    return LC_SUCCESS;
}

LC_API LCResult lc_line2d_get_points(LCComponentHandle handle, LCVec2* outPoints, uint32_t maxCount, uint32_t* outCount) {
    if (!outCount) return LC_ERROR_NULL_POINTER;
    auto line = GetLine2D(handle);
    if (!line) return LC_ERROR_INVALID_HANDLE;

    const auto& points = line->GetPoints();
    uint32_t count = static_cast<uint32_t>(points.size());
    *outCount = count;

    if (outPoints) {
        uint32_t copyCount = (count < maxCount) ? count : maxCount;
        for (uint32_t i = 0; i < copyCount; ++i) {
            outPoints[i] = FromEngineVec2(points[i]);
        }
    }

    return LC_SUCCESS;
}

/* ----- Beginning and End Points ----- */

LC_API LCResult lc_line2d_get_beginning(LCComponentHandle handle, LCVec2* outPoint) {
    if (!outPoint) return LC_ERROR_NULL_POINTER;
    auto line = GetLine2D(handle);
    if (!line) return LC_ERROR_INVALID_HANDLE;
    *outPoint = FromEngineVec2(line->GetBeginning());
    return LC_SUCCESS;
}

LC_API LCResult lc_line2d_set_beginning(LCComponentHandle handle, LCVec2 point) {
    auto line = GetLine2D(handle);
    if (!line) return LC_ERROR_INVALID_HANDLE;
    line->SetBeginning(ToEngineVec2(point));
    return LC_SUCCESS;
}

LC_API LCResult lc_line2d_get_end(LCComponentHandle handle, LCVec2* outPoint) {
    if (!outPoint) return LC_ERROR_NULL_POINTER;
    auto line = GetLine2D(handle);
    if (!line) return LC_ERROR_INVALID_HANDLE;
    *outPoint = FromEngineVec2(line->GetEnd());
    return LC_SUCCESS;
}

LC_API LCResult lc_line2d_set_end(LCComponentHandle handle, LCVec2 point) {
    auto line = GetLine2D(handle);
    if (!line) return LC_ERROR_INVALID_HANDLE;
    line->SetEnd(ToEngineVec2(point));
    return LC_SUCCESS;
}

/* ----- Stroke Properties ----- */

LC_API LCResult lc_line2d_get_stroke_color(LCComponentHandle handle, LCColor* outColor) {
    if (!outColor) return LC_ERROR_NULL_POINTER;
    auto line = GetLine2D(handle);
    if (!line) return LC_ERROR_INVALID_HANDLE;
    *outColor = FromEngineColor(line->GetStrokeColor());
    return LC_SUCCESS;
}

LC_API LCResult lc_line2d_set_stroke_color(LCComponentHandle handle, LCColor color) {
    auto line = GetLine2D(handle);
    if (!line) return LC_ERROR_INVALID_HANDLE;
    line->SetStrokeColor(ToEngineColor(color));
    return LC_SUCCESS;
}

LC_API LCResult lc_line2d_get_stroke_width(LCComponentHandle handle, float* outWidth) {
    if (!outWidth) return LC_ERROR_NULL_POINTER;
    auto line = GetLine2D(handle);
    if (!line) return LC_ERROR_INVALID_HANDLE;
    *outWidth = line->GetStrokeWidth();
    return LC_SUCCESS;
}

LC_API LCResult lc_line2d_set_stroke_width(LCComponentHandle handle, float width) {
    auto line = GetLine2D(handle);
    if (!line) return LC_ERROR_INVALID_HANDLE;
    line->SetStrokeWidth(width);
    return LC_SUCCESS;
}

/* ----- Closed Loop ----- */

LC_API LCResult lc_line2d_get_closed_loop(LCComponentHandle handle, bool* outClosed) {
    if (!outClosed) return LC_ERROR_NULL_POINTER;
    auto line = GetLine2D(handle);
    if (!line) return LC_ERROR_INVALID_HANDLE;
    *outClosed = line->GetClosedLoop();
    return LC_SUCCESS;
}

LC_API LCResult lc_line2d_set_closed_loop(LCComponentHandle handle, bool closed) {
    auto line = GetLine2D(handle);
    if (!line) return LC_ERROR_INVALID_HANDLE;
    line->SetClosedLoop(closed);
    return LC_SUCCESS;
}

/* ----- Cap and Join Styles ----- */

LC_API LCResult lc_line2d_get_cap_style(LCComponentHandle handle, LCLineCapStyle* outStyle) {
    if (!outStyle) return LC_ERROR_NULL_POINTER;
    auto line = GetLine2D(handle);
    if (!line) return LC_ERROR_INVALID_HANDLE;
    *outStyle = FromEngineCapStyle(line->GetCapStyle());
    return LC_SUCCESS;
}

LC_API LCResult lc_line2d_set_cap_style(LCComponentHandle handle, LCLineCapStyle style) {
    auto line = GetLine2D(handle);
    if (!line) return LC_ERROR_INVALID_HANDLE;
    line->SetCapStyle(ToEngineCapStyle(style));
    return LC_SUCCESS;
}

LC_API LCResult lc_line2d_get_join_style(LCComponentHandle handle, LCLineJoinStyle* outStyle) {
    if (!outStyle) return LC_ERROR_NULL_POINTER;
    auto line = GetLine2D(handle);
    if (!line) return LC_ERROR_INVALID_HANDLE;
    *outStyle = FromEngineJoinStyle(line->GetJoinStyle());
    return LC_SUCCESS;
}

LC_API LCResult lc_line2d_set_join_style(LCComponentHandle handle, LCLineJoinStyle style) {
    auto line = GetLine2D(handle);
    if (!line) return LC_ERROR_INVALID_HANDLE;
    line->SetJoinStyle(ToEngineJoinStyle(style));
    return LC_SUCCESS;
}

/* ----- Quality Properties ----- */

LC_API LCResult lc_line2d_get_anti_aliasing(LCComponentHandle handle, bool* outEnabled) {
    if (!outEnabled) return LC_ERROR_NULL_POINTER;
    auto line = GetLine2D(handle);
    if (!line) return LC_ERROR_INVALID_HANDLE;
    *outEnabled = line->GetAntiAliasing();
    return LC_SUCCESS;
}

LC_API LCResult lc_line2d_set_anti_aliasing(LCComponentHandle handle, bool enabled) {
    auto line = GetLine2D(handle);
    if (!line) return LC_ERROR_INVALID_HANDLE;
    line->SetAntiAliasing(enabled);
    return LC_SUCCESS;
}

LC_API LCResult lc_line2d_get_smoothness(LCComponentHandle handle, int* outSmoothness) {
    if (!outSmoothness) return LC_ERROR_NULL_POINTER;
    auto line = GetLine2D(handle);
    if (!line) return LC_ERROR_INVALID_HANDLE;
    *outSmoothness = line->GetSmoothness();
    return LC_SUCCESS;
}

LC_API LCResult lc_line2d_set_smoothness(LCComponentHandle handle, int smoothness) {
    auto line = GetLine2D(handle);
    if (!line) return LC_ERROR_INVALID_HANDLE;
    line->SetSmoothness(smoothness);
    return LC_SUCCESS;
}

/* ----- Rendering Properties ----- */

LC_API LCResult lc_line2d_get_layer(LCComponentHandle handle, int* outLayer) {
    if (!outLayer) return LC_ERROR_NULL_POINTER;
    auto line = GetLine2D(handle);
    if (!line) return LC_ERROR_INVALID_HANDLE;
    *outLayer = line->GetLayer();
    return LC_SUCCESS;
}

LC_API LCResult lc_line2d_set_layer(LCComponentHandle handle, int layer) {
    auto line = GetLine2D(handle);
    if (!line) return LC_ERROR_INVALID_HANDLE;
    line->SetLayer(layer);
    return LC_SUCCESS;
}

LC_API LCResult lc_line2d_get_sorting_order(LCComponentHandle handle, int* outOrder) {
    if (!outOrder) return LC_ERROR_NULL_POINTER;
    auto line = GetLine2D(handle);
    if (!line) return LC_ERROR_INVALID_HANDLE;
    *outOrder = line->GetSortingOrder();
    return LC_SUCCESS;
}

LC_API LCResult lc_line2d_set_sorting_order(LCComponentHandle handle, int order) {
    auto line = GetLine2D(handle);
    if (!line) return LC_ERROR_INVALID_HANDLE;
    line->SetSortingOrder(order);
    return LC_SUCCESS;
}

LC_API LCResult lc_line2d_get_ui_space(LCComponentHandle handle, bool* outUISpace) {
    if (!outUISpace) return LC_ERROR_NULL_POINTER;
    auto line = GetLine2D(handle);
    if (!line) return LC_ERROR_INVALID_HANDLE;
    *outUISpace = line->GetUISpace();
    return LC_SUCCESS;
}

LC_API LCResult lc_line2d_set_ui_space(LCComponentHandle handle, bool uiSpace) {
    auto line = GetLine2D(handle);
    if (!line) return LC_ERROR_INVALID_HANDLE;
    line->SetUISpace(uiSpace);
    return LC_SUCCESS;
}
