

#include "lupine/rendering/ViewportUtils.hpp"
#include "lupine/rendering/RenderView.hpp"
#include "lupine/math/Math.hpp"
#include <mutex>

namespace lupine {

static Viewport g_CurrentViewport;
static std::mutex g_ViewportMutex;

static math::Vec2 g_LogicalCanvasSize(1280.0f, 720.0f);
static std::mutex g_CanvasSizeMutex;

static math::Mat4 g_CanvasPickMatrix = math::Mat4::Identity();
static bool g_HasCanvasPickMatrix = false;
static std::mutex g_CanvasPickMutex;

void SetCurrentViewport(const Viewport& viewport) {
    std::lock_guard<std::mutex> lock(g_ViewportMutex);
    g_CurrentViewport = viewport;
}

Viewport GetCurrentViewport() {
    std::lock_guard<std::mutex> lock(g_ViewportMutex);
    return g_CurrentViewport;
}

void SetLogicalCanvasSize(const math::Vec2& size) {
    std::lock_guard<std::mutex> lock(g_CanvasSizeMutex);
    g_LogicalCanvasSize = size;
}

math::Vec2 GetLogicalCanvasSize() {
    std::lock_guard<std::mutex> lock(g_CanvasSizeMutex);
    return g_LogicalCanvasSize;
}

void SetCanvasPickMatrix(const math::Mat4& inverseViewProj) {
    std::lock_guard<std::mutex> lock(g_CanvasPickMutex);
    g_CanvasPickMatrix = inverseViewProj;
    g_HasCanvasPickMatrix = true;
}

void ClearCanvasPickMatrix() {
    std::lock_guard<std::mutex> lock(g_CanvasPickMutex);
    g_HasCanvasPickMatrix = false;
}

bool GetCanvasPickMatrix(math::Mat4& outInverseViewProj) {
    std::lock_guard<std::mutex> lock(g_CanvasPickMutex);
    if (!g_HasCanvasPickMatrix) {
        return false;
    }
    outInverseViewProj = g_CanvasPickMatrix;
    return true;
}

}
