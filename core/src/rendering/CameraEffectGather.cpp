#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/RenderView.hpp"
#include "lupine/rendering/RenderCamera.hpp"
#include "lupine/rendering/CameraEffectStack.hpp"
#include "lupine/components/CameraEffect.hpp"
#include "lupine/core/CameraNodes.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Component.hpp"

#include <memory>
#include <utility>

// Defined in its own translation unit so it can include core/CameraNodes.hpp safely:
// in RenderWorld.cpp the render-layer Camera2D/Camera3D and core::Camera2D/Camera3D
// collide under that file's using-directives. Here we fully qualify, so there is no clash.

namespace lupine {

bool RenderWorld::gatherCameraEffects(RenderView* view, CameraEffectChain& out) {
    if (!view) {
        return false;
    }
    core::Node* node = view->getSourceCameraNode();
    if (!node) {
        return false;
    }

    // Per-camera master toggle + render mode, read from whichever camera type this is.
    bool effectsEnabled = true;
    int renderMode = 0;
    bool perspective = false;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;

    if (auto* cam3d = dynamic_cast<core::Camera3D*>(node)) {
        effectsEnabled = cam3d->GetEffectsEnabled();
        renderMode = cam3d->GetEffectRenderMode();
        perspective = (cam3d->GetProjectionType() == core::Camera3D::ProjectionType::Perspective);
        nearPlane = cam3d->GetNearPlane();
        farPlane = cam3d->GetFarPlane();
    } else if (auto* cam2d = dynamic_cast<core::Camera2D*>(node)) {
        effectsEnabled = cam2d->GetEffectsEnabled();
        renderMode = cam2d->GetEffectRenderMode();
    } else if (auto* camUI = dynamic_cast<core::CameraUI*>(node)) {
        effectsEnabled = camUI->GetEffectsEnabled();
        renderMode = camUI->GetEffectRenderMode();
    } else {
        return false;  // Source node is not a camera.
    }

    if (!effectsEnabled) {
        return false;
    }

    components::CameraEffectContext fxCtx;
    fxCtx.cameraPerspective = perspective;
    fxCtx.cameraNear = nearPlane;
    fxCtx.cameraFar = farPlane;
    bool clearsColor = true;
    if (RenderCamera* cam = view->getCamera()) {
        const float aspect = view->getAspectRatio();
        fxCtx.projection = cam->getProjectionMatrix(aspect);
        fxCtx.invProjection = fxCtx.projection.Inverse();
        clearsColor = (static_cast<uint32_t>(cam->clearFlags) &
                       static_cast<uint32_t>(CameraClearFlags::Color)) != 0u;
    }

    out = CameraEffectChain();
    out.mode = (renderMode == 1) ? CameraEffectRenderMode::Composite : CameraEffectRenderMode::Separate;
    out.flipY = FlipYMode::Auto;
    out.time = static_cast<float>(m_frameNumber) * 0.0166667f;
    out.cameraPerspective = perspective;
    out.cameraNear = nearPlane;
    out.cameraFar = farPlane;
    out.cameraClearsColor = clearsColor;
    out.projection = fxCtx.projection;
    out.invProjection = fxCtx.invProjection;

    // Collect enabled CameraEffect components in attachment (stack) order.
    for (const std::shared_ptr<core::Component>& comp : node->GetComponents()) {
        if (!comp || !comp->IsEnabled()) {
            continue;
        }
        const components::CameraEffect* fx = dynamic_cast<const components::CameraEffect*>(comp.get());
        if (!fx) {
            continue;
        }
        ResolvedCameraEffect resolved;
        if (fx->IsComposable()) {
            resolved.composable = true;
            fx->BuildCompose(resolved.compose);
        } else {
            resolved.composable = false;
            fx->BuildPasses(fxCtx, resolved.passes);
            if (resolved.passes.empty()) {
                continue;
            }
        }
        out.effects.push_back(std::move(resolved));
    }

    return !out.effects.empty();
}

} // namespace lupine
