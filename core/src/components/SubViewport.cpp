#include "lupine/components/SubViewport.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/CameraNodes.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderView.hpp"
#include "lupine/rendering/RenderCamera.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/gfx/GfxTypes.hpp"
#include "lupine/rendering/gfx/GfxDescriptors.hpp"
#include "lupine/rendering/ViewportUtils.hpp"
#include "lupine/input/InputManager.hpp"
#include "lupine/input/InputCodes.hpp"
#include "lupine/logger/Logger.hpp"

#include <algorithm>
#include <cmath>
#include <functional>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

namespace {

CameraType WorldTypeToCameraType(SubViewport::WorldType worldType) {
    switch (worldType) {
        case SubViewport::WorldType::World3D:
            return CameraType::Camera3D;
        case SubViewport::WorldType::ScreenUI:
            return CameraType::CameraCanvas;
        case SubViewport::WorldType::World2D:
        default:
            return CameraType::Camera2D;
    }
}

} // namespace

SubViewport::SubViewport()
    : Component("SubViewport")
{
}

SubViewport::SubViewport(const std::string& name)
    : Component(name)
{
}

SubViewport::~SubViewport() {
    ReleaseRenderTarget();
}

void SubViewport::DefineProperties() {
    // ===== Viewport (off-screen render target) =====
    DefineProperty(PROPERTY_INT_RANGE_GROUP(renderWidth, 512, 1, 8192, 1, "Viewport"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(renderHeight, 512, 1, 8192, 1, "Viewport"));
    DefineProperty(PROPERTY_ENUM_GROUP(worldType, 0, "Viewport", World2D, World3D, ScreenUI));
    DefineProperty(PROPERTY_DEFAULT_GROUP(transparentBackground, Bool, true, "Viewport"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(clearColor, Color, Color(0.0f, 0.0f, 0.0f, 1.0f), "Viewport"));

    // ===== Display (how the rendered texture is drawn on screen) =====
    DefineProperty(PROPERTY_ENUM_GROUP(displayMode, 1, "Display", Disabled, Stretch, KeepAspect));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(displayWidth, 512.0f, 0.0f, 10000.0f, 1.0f, "Display"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(displayHeight, 512.0f, 0.0f, 10000.0f, 1.0f, "Display"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(useUISpace, Bool, true, "Display"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(modulate, Color, Color::White(), "Display"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(flipVertical, Bool, false, "Display"));

    // ===== Input routing =====
    DefineProperty(PROPERTY_ENUM_GROUP(inputMode, 1, "Input", None, Isolated, Shared));
    DefineProperty(PROPERTY_DEFAULT_GROUP(focusOnClick, Bool, true, "Input"));
}

void SubViewport::OnAwake() {
    m_LastRenderedFrame = 0xFFFFFFFFu;
    m_Focused = false;
}

void SubViewport::OnDestroy() {
    ReleaseRenderTarget();
}

// ============================================================================
// GPU resource management
// ============================================================================

void SubViewport::EnsureRenderTarget(IGfxDevice* device, uint32_t width, uint32_t height) {
    if (!device) {
        return;
    }

    m_OwningDevice = device;

    if (m_RenderTarget.isValid() && m_RTWidth == width && m_RTHeight == height) {
        return;
    }

    if (m_RenderTarget.isValid()) {
        device->destroyRenderTarget(m_RenderTarget);
        m_RenderTarget = RenderTargetHandle();
        m_ColorTexture = TextureHandle();
        m_DepthTexture = TextureHandle();
    }

    RenderTargetDesc desc;
    desc.width = width;
    desc.height = height;
    desc.colorFormat = TextureFormat::RGBA8_UNORM;
    desc.depthFormat = TextureFormat::DEPTH24_STENCIL8;
    desc.hasColor = true;
    desc.hasDepth = true;
    desc.sampleCount = 1;

    m_RenderTarget = device->createRenderTarget(desc);
    if (m_RenderTarget.isValid()) {
        m_RTWidth = width;
        m_RTHeight = height;
        m_ColorTexture = device->getRenderTargetColorTexture(m_RenderTarget);
        m_DepthTexture = device->getRenderTargetDepthTexture(m_RenderTarget);
    } else {
        m_RTWidth = 0;
        m_RTHeight = 0;
        LOG_WARN(LogCategory::Render, "[SubViewport] Failed to create render target {}x{}", width, height);
    }
}

void SubViewport::ReleaseRenderTarget() {
    if (m_OwningDevice && m_RenderTarget.isValid()) {
        m_OwningDevice->destroyRenderTarget(m_RenderTarget);
    }
    m_RenderTarget = RenderTargetHandle();
    m_ColorTexture = TextureHandle();
    m_DepthTexture = TextureHandle();
    m_RTWidth = 0;
    m_RTHeight = 0;
}

void SubViewport::prepareGPUResources(IGfxDevice* device) {
    if (!device) {
        return;
    }
    uint32_t w = static_cast<uint32_t>(std::max(1, GetRenderWidth()));
    uint32_t h = static_cast<uint32_t>(std::max(1, GetRenderHeight()));
    EnsureRenderTarget(device, w, h);
}

// ============================================================================
// Sub-scene rendering
// ============================================================================

core::Node* SubViewport::FindCameraNode(CameraType cameraType) const {
    if (!m_Owner) {
        return nullptr;
    }

    core::Node* result = nullptr;

    std::function<void(core::Node*)> visit = [&](core::Node* node) {
        if (result || !node) {
            return;
        }

        // Do not descend into nested SubViewports - they own their own cameras.
        if (node != m_Owner && node->GetComponent<SubViewport>()) {
            return;
        }

        if (node != m_Owner && node->IsActiveInHierarchy()) {
            switch (cameraType) {
                case CameraType::Camera3D: {
                    auto* cam = dynamic_cast<core::Camera3D*>(node);
                    if (cam && cam->IsActive()) {
                        result = node;
                        return;
                    }
                    break;
                }
                case CameraType::Camera2D: {
                    auto* cam = dynamic_cast<core::Camera2D*>(node);
                    if (cam && cam->IsActive()) {
                        result = node;
                        return;
                    }
                    break;
                }
                case CameraType::CameraCanvas: {
                    auto* cam = dynamic_cast<core::CameraUI*>(node);
                    if (cam && cam->IsActive()) {
                        result = node;
                        return;
                    }
                    break;
                }
            }
        }

        for (const auto& child : node->GetChildren()) {
            visit(child.get());
            if (result) {
                return;
            }
        }
    };

    visit(m_Owner);
    return result;
}

std::unique_ptr<RenderCamera> SubViewport::BuildRenderCamera(RenderWorld* world,
                                                            CameraType cameraType,
                                                            float aspectRatio) const {
    (void)aspectRatio;

    GraphicsBackend backend = world->getBackend();
    Color clear = GetTransparentBackground() ? Color(0.0f, 0.0f, 0.0f, 0.0f) : GetClearColor();

    if (cameraType == CameraType::Camera3D) {
        auto camera = std::make_unique<lupine::Camera3D>();
        camera->backend = backend;

        if (auto* camNode = dynamic_cast<core::Camera3D*>(FindCameraNode(CameraType::Camera3D))) {
            Vec3 pos = camNode->GetGlobalPosition();
            Quat rot = camNode->GetGlobalRotation();
            camera->position = pos;
            camera->target = pos + rot * Vec3(0.0f, 0.0f, -1.0f);
            camera->up = rot * Vec3(0.0f, 1.0f, 0.0f);
            camera->projectionType =
                (camNode->GetProjectionType() == core::Camera3D::ProjectionType::Perspective)
                    ? ProjectionType::Perspective
                    : ProjectionType::Orthographic;
            camera->fov = camNode->GetFOV();
            camera->nearPlane = camNode->GetNearPlane();
            camera->farPlane = camNode->GetFarPlane();
            camera->orthoSize = camNode->GetOrthoSize();
        } else {
            // No camera in the subtree - use a sensible default so the view is not blank.
            camera->position = Vec3(0.0f, 2.0f, 5.0f);
            camera->target = Vec3(0.0f, 0.0f, 0.0f);
            camera->up = Vec3(0.0f, 1.0f, 0.0f);
        }

        camera->clearFlags = CameraClearFlags::All;
        camera->clearColor = clear;
        return camera;
    }

    if (cameraType == CameraType::Camera2D) {
        auto camera = std::make_unique<lupine::Camera2D>();
        camera->backend = backend;

        if (auto* camNode = dynamic_cast<core::Camera2D*>(FindCameraNode(CameraType::Camera2D))) {
            camera->position = camNode->GetGlobalPosition();
            camera->rotation = camNode->GetGlobalRotation();
            camera->zoom = camNode->GetZoom();
            camera->orthoSize = camNode->GetOrthoSize();
        } else {
            camera->position = Vec2(0.0f, 0.0f);
            camera->rotation = 0.0f;
            camera->zoom = 1.0f;
            camera->orthoSize = static_cast<float>(std::max(1, GetRenderHeight()));
        }

        camera->clearFlags = CameraClearFlags::All;
        camera->clearColor = clear;
        return camera;
    }

    // CameraCanvas (ScreenUI)
    auto camera = std::make_unique<lupine::CameraCanvas>();
    camera->backend = backend;
    camera->canvasSize = Vec2(static_cast<float>(std::max(1, GetRenderWidth())),
                              static_cast<float>(std::max(1, GetRenderHeight())));

    if (auto* camNode = dynamic_cast<core::CameraUI*>(FindCameraNode(CameraType::CameraCanvas))) {
        camera->canvasSize = camNode->GetCanvasSize();
        camera->origin = camNode->GetOrigin();
        camera->scaleFactor = camNode->GetScaleFactor();
        camera->pixelPerfect = camNode->IsPixelPerfect();
    }

    camera->clearFlags = CameraClearFlags::All;
    camera->clearColor = clear;
    return camera;
}

void SubViewport::RenderEmbedded(RenderWorld* world, core::Scene* scene) {
    if (!world || !scene || !m_Owner) {
        return;
    }

    IGfxDevice* device = world->getDevice();
    if (!device) {
        return;
    }

    // Render at most once per frame, even when multiple views reference this
    // SubViewport (e.g. the runtime's 3D/2D/UI passes share one frame).
    uint32_t frame = world->getFrameNumber();
    if (m_LastRenderedFrame == frame) {
        return;
    }
    m_LastRenderedFrame = frame;

    uint32_t width = static_cast<uint32_t>(std::max(1, GetRenderWidth()));
    uint32_t height = static_cast<uint32_t>(std::max(1, GetRenderHeight()));

    EnsureRenderTarget(device, width, height);
    if (!m_RenderTarget.isValid()) {
        return;
    }

    float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    CameraType cameraType = WorldTypeToCameraType(GetWorldType());

    std::unique_ptr<RenderCamera> renderCamera = BuildRenderCamera(world, cameraType, aspectRatio);
    if (!renderCamera) {
        return;
    }

    RenderViewID viewID = world->createRenderView(std::move(renderCamera));
    if (viewID == 0) {
        return;
    }

    RenderView* view = world->getRenderView(viewID);
    if (!view) {
        world->destroyRenderView(viewID);
        return;
    }

    view->setScene(scene);
    view->setRenderRootNode(m_Owner);
    // Link the subtree's camera node so its stacked CameraEffect components apply to
    // this SubViewport's render (the camera effects run inside world->renderView()).
    view->setSourceCameraNode(FindCameraNode(cameraType));
    view->setRenderTarget(m_RenderTarget);
    view->setDebugRenderingEnabled(false);
    view->setGridRenderingEnabled(false);
    view->setGizmoEnabled(false);
    view->setCameraPreviewEnabled(false);
    view->setCollisionShapesEnabled(false);

    Viewport viewport;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(width);
    viewport.height = static_cast<float>(height);
    view->setViewport(viewport);

    // renderView runs its own embedded SubViewport pre-pass first, so nested
    // SubViewports within this subtree are rendered before this one samples them.
    world->renderView(viewID);

    world->destroyRenderView(viewID);

    // Refresh the sampled color texture (handle is stable, but keep it in sync).
    m_ColorTexture = device->getRenderTargetColorTexture(m_RenderTarget);
}

// ============================================================================
// Display (drawing the rendered texture on screen)
// ============================================================================

void SubViewport::GetDisplayContentRectLogical(math::Vec2& outCenter, math::Vec2& outSize) const {
    Vec2 center(0.0f, 0.0f);
    if (auto* node2D = dynamic_cast<core::Node2D*>(m_Owner)) {
        center = node2D->GetGlobalPosition();
    }

    Vec2 box(std::max(0.0f, GetDisplayWidth()), std::max(0.0f, GetDisplayHeight()));
    Vec2 size = box;

    if (GetDisplayMode() == DisplayMode::KeepAspect && box.x > 0.0f && box.y > 0.0f) {
        float targetAspect = static_cast<float>(std::max(1, GetRenderWidth())) /
                             static_cast<float>(std::max(1, GetRenderHeight()));
        float boxAspect = box.x / box.y;
        if (boxAspect > targetAspect) {
            size.x = box.y * targetAspect;
            size.y = box.y;
        } else {
            size.x = box.x;
            size.y = box.x / targetAspect;
        }
    }

    outCenter = center;
    outSize = size;
}

void SubViewport::buildDrawCommands(RenderContext& ctx) {
    if (!IsEnabled() || !m_Owner) {
        return;
    }

    // Never draw our own display quad into our own off-screen target.
    if (RenderView* view = ctx.getView()) {
        if (view->getRenderRootNode() == m_Owner) {
            return;
        }
    }

    if (GetDisplayMode() == DisplayMode::Disabled) {
        return;
    }

    if (!m_ColorTexture.isValid()) {
        return;
    }

    auto* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (!node2D) {
        return;
    }

    Vec2 center;
    Vec2 size;
    GetDisplayContentRectLogical(center, size);
    if (size.x <= 0.0f || size.y <= 0.0f) {
        return;
    }

    // Off-screen targets on bottom-left-origin backends (OpenGL/WebGL) are stored
    // bottom-up relative to UI sampling, so flip V there. The manual flip toggle
    // composes on top of that.
    bool backendFlip = IsBottomLeftOrigin(ctx.getDevice()->getBackend());
    bool flip = backendFlip != GetFlipVertical();
    Vec2 uvMin = flip ? Vec2(0.0f, 1.0f) : Vec2(0.0f, 0.0f);
    Vec2 uvMax = flip ? Vec2(1.0f, 0.0f) : Vec2(1.0f, 1.0f);

    Color modulate = GetModulate();
    Vec4 cornerRadius(0.0f, 0.0f, 0.0f, 0.0f);
    float rotation = node2D->GetGlobalRotation();
    const int blendMode = 0; // Alpha blend

    if (std::abs(rotation) > 0.0001f) {
        ctx.drawRoundedRect(center, size, cornerRadius, modulate, m_ColorTexture,
                            rotation, uvMin, uvMax, blendMode);
    } else {
        Vec2 topLeft(center.x - size.x * 0.5f, center.y - size.y * 0.5f);
        ctx.drawRoundedRect(topLeft, size, cornerRadius, modulate, m_ColorTexture,
                            uvMin, uvMax, blendMode);
    }
}

AABB SubViewport::getWorldBounds() const {
    auto* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (!node2D) {
        return AABB();
    }

    Vec2 center;
    Vec2 size;
    GetDisplayContentRectLogical(center, size);

    Vec2 half = size * 0.5f;
    return AABB(
        Vec3(center.x - half.x, center.y - half.y, -0.1f),
        Vec3(center.x + half.x, center.y + half.y, 0.1f)
    );
}

RenderLayer SubViewport::getRenderLayer() const {
    // The display quad is an alpha-blended textured rect; render it in an overlay
    // layer so it composes above 2D/UI content rather than behind it.
    return GetUseUISpace() ? RenderLayer::UI : RenderLayer::Transparent;
}

SpatialType SubViewport::getSpatialType() const {
    return GetUseUISpace() ? SpatialType::Canvas : SpatialType::World2D;
}

// ============================================================================
// Input routing
// ============================================================================

void SubViewport::DispatchChildInput(float deltaTime,
                                     const std::vector<std::shared_ptr<core::Node>>& children) {
    InputMode mode = GetInputMode();

    if (mode == InputMode::None) {
        // The subtree receives no input.
        return;
    }

    auto* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (!node2D) {
        // Without a 2D rectangle we cannot remap pointer coordinates. Shared mode
        // still forwards input unchanged; isolated mode has no rectangle to gate on.
        if (mode == InputMode::Shared) {
            for (const auto& child : children) {
                if (child) {
                    child->OnInput(deltaTime);
                }
            }
        }
        return;
    }

    Viewport outerViewport = lupine::GetCurrentViewport();
    Vec2 outerCanvas = lupine::GetLogicalCanvasSize();

    if (outerCanvas.x <= 0.0f || outerCanvas.y <= 0.0f ||
        outerViewport.width <= 0.0f || outerViewport.height <= 0.0f) {
        // No usable coordinate context - forward unchanged.
        for (const auto& child : children) {
            if (child) {
                child->OnInput(deltaTime);
            }
        }
        return;
    }

    // Compute the SubViewport's on-screen rectangle (screen pixels) from its
    // logical display rectangle. Logical space is Y-up; screen space is Y-down.
    Vec2 center;
    Vec2 sizeLogical;
    GetDisplayContentRectLogical(center, sizeLogical);

    float logicalLeft = center.x - sizeLogical.x * 0.5f;
    float logicalTop = center.y + sizeLogical.y * 0.5f;

    Viewport innerViewport;
    innerViewport.x = outerViewport.x + (logicalLeft / outerCanvas.x) * outerViewport.width;
    innerViewport.width = (sizeLogical.x / outerCanvas.x) * outerViewport.width;
    innerViewport.y = outerViewport.y + (1.0f - (logicalTop / outerCanvas.y)) * outerViewport.height;
    innerViewport.height = (sizeLogical.y / outerCanvas.y) * outerViewport.height;
    innerViewport.minDepth = outerViewport.minDepth;
    innerViewport.maxDepth = outerViewport.maxDepth;

    Vec2 innerCanvas(static_cast<float>(std::max(1, GetRenderWidth())),
                     static_cast<float>(std::max(1, GetRenderHeight())));

    input::InputManager& inputMgr = input::InputManager::Get();
    glm::vec2 mouse = inputMgr.GetMousePosition();
    bool pointerOver = (innerViewport.width > 0.0f && innerViewport.height > 0.0f &&
                        mouse.x >= innerViewport.x &&
                        mouse.x <= innerViewport.x + innerViewport.width &&
                        mouse.y >= innerViewport.y &&
                        mouse.y <= innerViewport.y + innerViewport.height);

    if (mode == InputMode::Isolated && GetFocusOnClick()) {
        if (inputMgr.IsMouseButtonJustPressed(input::MouseButton::Left) ||
            inputMgr.IsMouseButtonJustPressed(input::MouseButton::Right) ||
            inputMgr.IsMouseButtonJustPressed(input::MouseButton::Middle)) {
            m_Focused = pointerOver;
        }
    } else {
        m_Focused = false;
    }

    bool dispatch = (mode == InputMode::Shared) || pointerOver ||
                    (mode == InputMode::Isolated && m_Focused);
    if (!dispatch) {
        return;
    }

    // Remap the global coordinate context so child UI hit-tests resolve into the
    // SubViewport's own space, then restore it afterwards.
    lupine::SetCurrentViewport(innerViewport);
    lupine::SetLogicalCanvasSize(innerCanvas);

    for (const auto& child : children) {
        if (child) {
            child->OnInput(deltaTime);
        }
    }

    lupine::SetCurrentViewport(outerViewport);
    lupine::SetLogicalCanvasSize(outerCanvas);
}

// ============================================================================
// Property accessors
// ============================================================================

int SubViewport::GetRenderWidth() const {
    return GetPropertyValue<int>("renderWidth");
}

void SubViewport::SetRenderWidth(int width) {
    SetPropertyValue<int>("renderWidth", std::max(1, width));
}

int SubViewport::GetRenderHeight() const {
    return GetPropertyValue<int>("renderHeight");
}

void SubViewport::SetRenderHeight(int height) {
    SetPropertyValue<int>("renderHeight", std::max(1, height));
}

SubViewport::WorldType SubViewport::GetWorldType() const {
    return static_cast<WorldType>(GetPropertyValue<int>("worldType"));
}

void SubViewport::SetWorldType(WorldType type) {
    SetPropertyValue<int>("worldType", static_cast<int>(type));
}

bool SubViewport::GetTransparentBackground() const {
    return GetPropertyValue<bool>("transparentBackground");
}

void SubViewport::SetTransparentBackground(bool transparent) {
    SetPropertyValue<bool>("transparentBackground", transparent);
}

Color SubViewport::GetClearColor() const {
    return GetPropertyValue<Color>("clearColor");
}

void SubViewport::SetClearColor(const Color& color) {
    SetPropertyValue<Color>("clearColor", color);
}

SubViewport::DisplayMode SubViewport::GetDisplayMode() const {
    return static_cast<DisplayMode>(GetPropertyValue<int>("displayMode"));
}

void SubViewport::SetDisplayMode(DisplayMode mode) {
    SetPropertyValue<int>("displayMode", static_cast<int>(mode));
}

float SubViewport::GetDisplayWidth() const {
    return GetPropertyValue<float>("displayWidth");
}

void SubViewport::SetDisplayWidth(float width) {
    SetPropertyValue<float>("displayWidth", width);
}

float SubViewport::GetDisplayHeight() const {
    return GetPropertyValue<float>("displayHeight");
}

void SubViewport::SetDisplayHeight(float height) {
    SetPropertyValue<float>("displayHeight", height);
}

bool SubViewport::GetUseUISpace() const {
    return GetPropertyValue<bool>("useUISpace");
}

void SubViewport::SetUseUISpace(bool useUISpace) {
    SetPropertyValue<bool>("useUISpace", useUISpace);
}

Color SubViewport::GetModulate() const {
    return GetPropertyValue<Color>("modulate");
}

void SubViewport::SetModulate(const Color& color) {
    SetPropertyValue<Color>("modulate", color);
}

bool SubViewport::GetFlipVertical() const {
    return GetPropertyValue<bool>("flipVertical");
}

void SubViewport::SetFlipVertical(bool flip) {
    SetPropertyValue<bool>("flipVertical", flip);
}

SubViewport::InputMode SubViewport::GetInputMode() const {
    return static_cast<InputMode>(GetPropertyValue<int>("inputMode"));
}

void SubViewport::SetInputMode(InputMode mode) {
    SetPropertyValue<int>("inputMode", static_cast<int>(mode));
}

bool SubViewport::GetFocusOnClick() const {
    return GetPropertyValue<bool>("focusOnClick");
}

void SubViewport::SetFocusOnClick(bool focusOnClick) {
    SetPropertyValue<bool>("focusOnClick", focusOnClick);
}

} // namespace components
} // namespace lupine
