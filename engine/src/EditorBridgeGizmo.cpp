

#include "lupine/engine/EditorBridge.hpp"
#include "lupine/rendering/GizmoUtils.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/EditorCommands.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/RenderView.hpp"
#include "lupine/rendering/RenderCamera.hpp"
#include "lupine/components/Sprite2D.hpp"
#include "lupine/components/Image2D.hpp"
#include "lupine/components/ColorRect.hpp"
#include "lupine/components/Label.hpp"
#include "lupine/components/Label3D.hpp"
#include "lupine/components/Button.hpp"
#include "lupine/components/Button3D.hpp"
#include "lupine/components/Panel.hpp"
#include "lupine/components/Panel3D.hpp"
#include "lupine/components/ProgressBar.hpp"
#include "lupine/components/ProgressBar3D.hpp"
#include "lupine/components/Container.hpp"
#include "lupine/components/RadioButton.hpp"
#include "lupine/components/Checkbox.hpp"
#include "lupine/components/Light2D.hpp"
#include "lupine/components/UIControl.hpp"
#include <algorithm>
#include <cfloat>

namespace lupine {
namespace engine {

using namespace core;
using namespace math;
using namespace components;

// Helper functions to capture component properties for undo/redo of gizmo scaling
static bool CaptureComponentScalingProperties(const std::shared_ptr<Component>& component,
                                               std::string& outTypeName,
                                               nlohmann::json& outProperties) {
    if (auto sprite = std::dynamic_pointer_cast<Sprite2D>(component)) {
        outTypeName = "Sprite2D";
        Vec2 size = sprite->GetSize();
        outProperties = {{"size", {{"x", size.x}, {"y", size.y}}}};
        return true;
    } else if (auto label = std::dynamic_pointer_cast<Label>(component)) {
        outTypeName = "Label";
        outProperties = {{"fontSize", label->GetFontSize()}};
        return true;
    } else if (auto label3D = std::dynamic_pointer_cast<Label3D>(component)) {
        outTypeName = "Label3D";
        outProperties = {{"fontSize", label3D->GetFontSize()}};
        return true;
    } else if (auto button3D = std::dynamic_pointer_cast<Button3D>(component)) {
        outTypeName = "Button3D";
        outProperties = {
            {"width", button3D->GetWidth()},
            {"height", button3D->GetHeight()}
        };
        return true;
    } else if (auto panel3D = std::dynamic_pointer_cast<Panel3D>(component)) {
        outTypeName = "Panel3D";
        outProperties = {
            {"width", panel3D->GetWidth()},
            {"height", panel3D->GetHeight()}
        };
        return true;
    } else if (auto progressBar3D = std::dynamic_pointer_cast<ProgressBar3D>(component)) {
        outTypeName = "ProgressBar3D";
        outProperties = {
            {"width", progressBar3D->GetWidth()},
            {"height", progressBar3D->GetHeight()}
        };
        return true;
    } else if (auto radioButton = std::dynamic_pointer_cast<RadioButton>(component)) {
        outTypeName = "RadioButton";
        outProperties = {
            {"indicatorSize", radioButton->GetIndicatorSize()},
            {"fontSize", radioButton->GetFontSize()}
        };
        return true;
    } else if (auto checkbox = std::dynamic_pointer_cast<Checkbox>(component)) {
        outTypeName = "Checkbox";
        outProperties = {
            {"boxSize", checkbox->GetBoxSize()},
            {"fontSize", checkbox->GetFontSize()}
        };
        return true;
    } else if (auto light2D = std::dynamic_pointer_cast<Light2D>(component)) {
        outTypeName = "Light2D";
        outProperties = {{"range", light2D->GetRange()}};
        return true;
    } else if (auto uiControl = std::dynamic_pointer_cast<UIControl>(component)) {
        // Every UIControl whose OnGizmoScale resizes the control -- which is all of them
        // except the ones scaling something else entirely (Label's fontSize, Checkbox's
        // boxSize, ...), and those are matched by their explicit cases ABOVE this one.
        //
        // Width/height AND the offsets are captured, because which pair actually drives an
        // axis depends on its anchors: UIControl::SetAxisExtent writes width/height on a
        // point-anchored axis and offsetMax on an anchor-stretched one. The gizmo restores
        // these before each incremental scale, so anything OnGizmoScale can write has to be
        // restorable or the drag compounds itself frame over frame.
        outTypeName = "UIControl";
        Vec2 offsetMin = uiControl->GetOffsetMin();
        Vec2 offsetMax = uiControl->GetOffsetMax();
        outProperties = {
            {"width", uiControl->GetWidth()},
            {"height", uiControl->GetHeight()},
            {"offsetMin", {{"x", offsetMin.x}, {"y", offsetMin.y}}},
            {"offsetMax", {{"x", offsetMax.x}, {"y", offsetMax.y}}}
        };
        return true;
    }
    return false;
}

// Restore the captured initial sizing properties of a component. This is the
// inverse of CaptureComponentScalingProperties and is applied before each
// incremental gizmo scale so OnGizmoScale always grows from the drag-start size
// rather than compounding the previous frame's result.
static void ApplyComponentScalingProperties(const std::shared_ptr<Component>& component,
                                             const nlohmann::json& properties) {
    if (auto sprite = std::dynamic_pointer_cast<Sprite2D>(component)) {
        if (properties.contains("size")) {
            sprite->SetSize(Vec2(properties["size"]["x"].get<float>(),
                                 properties["size"]["y"].get<float>()));
        }
    } else if (auto label = std::dynamic_pointer_cast<Label>(component)) {
        if (properties.contains("fontSize")) label->SetFontSize(properties["fontSize"].get<float>());
    } else if (auto label3D = std::dynamic_pointer_cast<Label3D>(component)) {
        if (properties.contains("fontSize")) label3D->SetFontSize(properties["fontSize"].get<float>());
    } else if (auto button3D = std::dynamic_pointer_cast<Button3D>(component)) {
        if (properties.contains("width")) button3D->SetWidth(properties["width"].get<float>());
        if (properties.contains("height")) button3D->SetHeight(properties["height"].get<float>());
    } else if (auto panel3D = std::dynamic_pointer_cast<Panel3D>(component)) {
        if (properties.contains("width")) panel3D->SetWidth(properties["width"].get<float>());
        if (properties.contains("height")) panel3D->SetHeight(properties["height"].get<float>());
    } else if (auto progressBar3D = std::dynamic_pointer_cast<ProgressBar3D>(component)) {
        if (properties.contains("width")) progressBar3D->SetWidth(properties["width"].get<float>());
        if (properties.contains("height")) progressBar3D->SetHeight(properties["height"].get<float>());
    } else if (auto radioButton = std::dynamic_pointer_cast<RadioButton>(component)) {
        if (properties.contains("indicatorSize")) radioButton->SetIndicatorSize(properties["indicatorSize"].get<float>());
        if (properties.contains("fontSize")) radioButton->SetFontSize(properties["fontSize"].get<float>());
    } else if (auto checkbox = std::dynamic_pointer_cast<Checkbox>(component)) {
        if (properties.contains("boxSize")) checkbox->SetBoxSize(properties["boxSize"].get<float>());
        if (properties.contains("fontSize")) checkbox->SetFontSize(properties["fontSize"].get<float>());
    } else if (auto light2D = std::dynamic_pointer_cast<Light2D>(component)) {
        if (properties.contains("range")) light2D->SetRange(properties["range"].get<float>());
    } else if (auto uiControl = std::dynamic_pointer_cast<UIControl>(component)) {
        if (properties.contains("width")) uiControl->SetWidth(properties["width"].get<float>());
        if (properties.contains("height")) uiControl->SetHeight(properties["height"].get<float>());
        if (properties.contains("offsetMin")) {
            uiControl->SetOffsetMin(Vec2(properties["offsetMin"]["x"].get<float>(),
                                         properties["offsetMin"]["y"].get<float>()));
        }
        if (properties.contains("offsetMax")) {
            uiControl->SetOffsetMax(Vec2(properties["offsetMax"]["x"].get<float>(),
                                         properties["offsetMax"]["y"].get<float>()));
        }
    }
}

static Vec3 CalculateGizmoCenterPosition(
    const std::vector<std::shared_ptr<core::Node>>& nodes,
    lupine::Camera2D* camera2D,
    lupine::Camera3D* camera3D,
    float& outGizmoSize,
    bool& outIs2D
) {
    if (nodes.empty()) {
        return Vec3::Zero();
    }

    bool is2DMode = std::dynamic_pointer_cast<Node2D>(nodes[0]) != nullptr;
    outIs2D = is2DMode;

    if (is2DMode && camera2D) {

        Vec2 avgPos2D(0.0f, 0.0f);
        int validNodeCount = 0;

        for (const auto& node : nodes) {
            if (auto node2D = std::dynamic_pointer_cast<Node2D>(node)) {
                Vec2 nodePos = node2D->GetGlobalPosition();

                const auto& components = node->GetComponents();
                for (const auto& component : components) {
                    if (auto renderable = std::dynamic_pointer_cast<IRenderableComponent>(component)) {
                        AABB worldBounds = renderable->getWorldBounds();
                        Vec3 boundsSize = worldBounds.GetSize();
                        if (boundsSize.x > 0.0f || boundsSize.y > 0.0f) {
                            Vec3 center = worldBounds.GetCenter();
                            nodePos = Vec2(center.x, center.y);
                            break;
                        }
                    }
                }

                avgPos2D = avgPos2D + nodePos;
                validNodeCount++;
            }
        }

        if (validNodeCount > 0) {
            avgPos2D = avgPos2D / static_cast<float>(validNodeCount);
            outGizmoSize = 50.0f / camera2D->zoom;
            return Vec3(avgPos2D.x, avgPos2D.y, 0.0f);
        }

    } else if (!is2DMode && camera3D) {

        Vec3 avgPos3D(0.0f, 0.0f, 0.0f);
        int validNodeCount = 0;

        for (const auto& node : nodes) {
            if (auto node3D = std::dynamic_pointer_cast<Node3D>(node)) {
                avgPos3D = avgPos3D + node3D->GetGlobalPosition();
                validNodeCount++;
            }
        }

        if (validNodeCount > 0) {
            avgPos3D = avgPos3D / static_cast<float>(validNodeCount);
            Vec3 cameraPos = camera3D->position;
            float distanceToCamera = (avgPos3D - cameraPos).Length();
            outGizmoSize = distanceToCamera * 0.1f;
            return avgPos3D;
        }
    }

    return Vec3::Zero();
}

// Compute the oriented half-extents of the 2D selection rectangle (in world
// units), mirroring the logic the renderer uses to draw the scale gizmo so hit
// testing and dragging line up exactly with what is on screen.
static Vec2 ComputeGizmoHalfExtents2D(
    const std::vector<std::shared_ptr<core::Node>>& nodes,
    float rotation2D
) {
    Vec2 unionMin(FLT_MAX, FLT_MAX);
    Vec2 unionMax(-FLT_MAX, -FLT_MAX);
    bool hasBounds = false;
    int nodeCount = 0;

    for (const auto& node : nodes) {
        if (!std::dynamic_pointer_cast<Node2D>(node)) {
            continue;
        }
        nodeCount++;

        const auto& components = node->GetComponents();
        for (const auto& component : components) {
            if (auto renderable = std::dynamic_pointer_cast<IRenderableComponent>(component)) {
                AABB worldBounds = renderable->getWorldBounds();
                Vec3 boundsSize = worldBounds.GetSize();
                if (boundsSize.x > 0.0f || boundsSize.y > 0.0f) {
                    unionMin.x = std::min(unionMin.x, worldBounds.min.x);
                    unionMin.y = std::min(unionMin.y, worldBounds.min.y);
                    unionMax.x = std::max(unionMax.x, worldBounds.max.x);
                    unionMax.y = std::max(unionMax.y, worldBounds.max.y);
                    hasBounds = true;
                    break;
                }
            }
        }
    }

    if (!hasBounds) {
        return Vec2(0.0f, 0.0f);
    }

    Vec2 aabbHalf((unionMax.x - unionMin.x) * 0.5f, (unionMax.y - unionMin.y) * 0.5f);

    if (nodeCount == 1 && std::abs(rotation2D) > 0.0001f) {
        return GizmoUtils::SolveOrientedHalfExtents2D(aabbHalf, rotation2D);
    }
    return aabbHalf;
}

bool EditorBridge::TestGizmoHit(RenderViewID viewID, float screenX, float screenY) {
    auto it = m_RenderViews.find(viewID);
    if (it == m_RenderViews.end() || !m_RenderWorld) {
        return false;
    }

    lupine::RenderView* pRenderView = m_RenderWorld->getRenderView(viewID);
    if (!pRenderView || !pRenderView->isGizmoEnabled()) {
        return false;
    }

    auto selectedNodes = pRenderView->getSelectedNodes();
    if (selectedNodes.empty()) {
        auto selectedNode = pRenderView->getSelectedNode();
        if (!selectedNode) {
            return false;
        }
        selectedNodes.push_back(selectedNode);
    }

    lupine::RenderCamera* camera = pRenderView->getCamera();
    if (!camera) {
        return false;
    }

    lupine::Camera2D* camera2D = dynamic_cast<lupine::Camera2D*>(camera);
    lupine::Camera3D* camera3D = dynamic_cast<lupine::Camera3D*>(camera);

    Vec3 gizmoPos;
    float gizmoSize;
    bool is2D = false;

    gizmoPos = CalculateGizmoCenterPosition(selectedNodes, camera2D, camera3D, gizmoSize, is2D);

    if (gizmoPos == Vec3::Zero() && gizmoSize == 0.0f) {
        return false;
    }

    const Viewport& viewport = pRenderView->getViewport();
    float viewportWidth = static_cast<float>(viewport.width);
    float viewportHeight = static_cast<float>(viewport.height);

    float rotation2D = 0.0f;
    Quat rotation3D = Quat::Identity();
    TransformSpace transformSpace = pRenderView->getTransformSpace();

    if (selectedNodes.size() == 1) {
        auto& node = selectedNodes[0];

        if (transformSpace == TransformSpace::Local) {

            if (auto node2D = std::dynamic_pointer_cast<core::Node2D>(node)) {
                rotation2D = node2D->GetGlobalRotation();
            } else if (auto node3D = std::dynamic_pointer_cast<core::Node3D>(node)) {
                rotation3D = node3D->GetGlobalRotation();
            }
        } else if (transformSpace == TransformSpace::Parent) {

            if (auto parent = node->GetParent()) {
                if (auto parent2D = dynamic_cast<core::Node2D*>(parent)) {
                    rotation2D = parent2D->GetGlobalRotation();
                } else if (auto parent3D = dynamic_cast<core::Node3D*>(parent)) {
                    rotation3D = parent3D->GetGlobalRotation();
                }
            }
        }

    }

    GizmoHitResult hitResult;

    if (is2D) {

        float aspectRatio = pRenderView->getAspectRatio();
        Mat4 viewMatrix = camera->getViewMatrix();
        Mat4 projMatrix = camera->getProjectionMatrix(aspectRatio);

        Vec2 halfExtents = ComputeGizmoHalfExtents2D(selectedNodes, rotation2D);

        hitResult = GizmoUtils::TestGizmo2D(
            Vec2(screenX, screenY),
            gizmoPos,
            gizmoSize,
            pRenderView->getGizmoType(),
            viewMatrix,
            projMatrix,
            viewportWidth,
            viewportHeight,
            rotation2D,
            halfExtents
        );
    } else {

        float aspectRatio = pRenderView->getAspectRatio();
        Mat4 viewMatrix = camera->getViewMatrix();
        Mat4 projMatrix = camera->getProjectionMatrix(aspectRatio);

        float ndcX = (2.0f * screenX) / viewportWidth - 1.0f;
        float ndcY = 1.0f - (2.0f * screenY) / viewportHeight;

        Mat4 invViewProj = (projMatrix * viewMatrix).Inverse();

        Vec4 rayClipNear = Vec4(ndcX, ndcY, -1.0f, 1.0f);
        Vec4 rayWorldNear = invViewProj * rayClipNear;

        Vec3 nearPoint = Vec3(rayWorldNear.x / rayWorldNear.w,
                             rayWorldNear.y / rayWorldNear.w,
                             rayWorldNear.z / rayWorldNear.w);

        Vec4 rayClipFar = Vec4(ndcX, ndcY, 1.0f, 1.0f);
        Vec4 rayWorldFar = invViewProj * rayClipFar;

        Vec3 farPoint = Vec3(rayWorldFar.x / rayWorldFar.w,
                            rayWorldFar.y / rayWorldFar.w,
                            rayWorldFar.z / rayWorldFar.w);

        Ray ray;
        ray.origin = nearPoint;
        ray.direction = (farPoint - nearPoint).Normalized();

        hitResult = GizmoUtils::TestGizmo3D(ray, gizmoPos, gizmoSize, pRenderView->getGizmoType(), rotation3D);
    }

    return hitResult.hit;
}

void EditorBridge::BeginGizmoDrag(RenderViewID viewID, float screenX, float screenY) {
    auto it = m_RenderViews.find(viewID);
    if (it == m_RenderViews.end() || !m_RenderWorld) {
        return;
    }

    lupine::RenderView* pRenderView = m_RenderWorld->getRenderView(viewID);
    if (!pRenderView || !pRenderView->isGizmoEnabled()) {
        return;
    }

    auto selectedNodes = pRenderView->getSelectedNodes();
    if (selectedNodes.empty()) {
        auto selectedNode = pRenderView->getSelectedNode();
        if (!selectedNode) {
            return;
        }
        selectedNodes.push_back(selectedNode);
    }

    lupine::RenderCamera* camera = pRenderView->getCamera();
    if (!camera) {
        return;
    }

    lupine::Camera2D* camera2D = dynamic_cast<lupine::Camera2D*>(camera);
    lupine::Camera3D* camera3D = dynamic_cast<lupine::Camera3D*>(camera);

    Vec3 gizmoPos;
    float gizmoSize;
    bool is2D = false;

    gizmoPos = CalculateGizmoCenterPosition(selectedNodes, camera2D, camera3D, gizmoSize, is2D);

    if (gizmoPos == Vec3::Zero() && gizmoSize == 0.0f) {
        return;
    }

    const Viewport& viewport = pRenderView->getViewport();
    float viewportWidth = static_cast<float>(viewport.width);
    float viewportHeight = static_cast<float>(viewport.height);

    float rotation2D = 0.0f;
    Quat rotation3D = Quat::Identity();
    TransformSpace transformSpace = pRenderView->getTransformSpace();

    if (selectedNodes.size() == 1) {
        auto& node = selectedNodes[0];

        if (transformSpace == TransformSpace::Local) {

            if (auto node2D = std::dynamic_pointer_cast<core::Node2D>(node)) {
                rotation2D = node2D->GetGlobalRotation();
            } else if (auto node3D = std::dynamic_pointer_cast<core::Node3D>(node)) {
                rotation3D = node3D->GetGlobalRotation();
            }
        } else if (transformSpace == TransformSpace::Parent) {

            if (auto parent = node->GetParent()) {
                if (auto parent2D = dynamic_cast<core::Node2D*>(parent)) {
                    rotation2D = parent2D->GetGlobalRotation();
                } else if (auto parent3D = dynamic_cast<core::Node3D*>(parent)) {
                    rotation3D = parent3D->GetGlobalRotation();
                }
            }
        }

    }

    GizmoHitResult hitResult;

    Vec2 dragHalfExtents(0.0f, 0.0f);

    if (is2D) {
        float aspectRatio = pRenderView->getAspectRatio();
        Mat4 viewMatrix = camera->getViewMatrix();
        Mat4 projMatrix = camera->getProjectionMatrix(aspectRatio);

        dragHalfExtents = ComputeGizmoHalfExtents2D(selectedNodes, rotation2D);

        hitResult = GizmoUtils::TestGizmo2D(
            Vec2(screenX, screenY),
            gizmoPos,
            gizmoSize,
            pRenderView->getGizmoType(),
            viewMatrix,
            projMatrix,
            viewportWidth,
            viewportHeight,
            rotation2D,
            dragHalfExtents
        );
    } else {
        float aspectRatio = pRenderView->getAspectRatio();
        Mat4 viewMatrix = camera->getViewMatrix();
        Mat4 projMatrix = camera->getProjectionMatrix(aspectRatio);

        float ndcX = (2.0f * screenX) / viewportWidth - 1.0f;
        float ndcY = 1.0f - (2.0f * screenY) / viewportHeight;

        Mat4 invViewProj = (projMatrix * viewMatrix).Inverse();

        Vec4 rayClipNear = Vec4(ndcX, ndcY, -1.0f, 1.0f);
        Vec4 rayWorldNear = invViewProj * rayClipNear;
        Vec3 nearPoint = Vec3(rayWorldNear.x / rayWorldNear.w,
                             rayWorldNear.y / rayWorldNear.w,
                             rayWorldNear.z / rayWorldNear.w);

        Vec4 rayClipFar = Vec4(ndcX, ndcY, 1.0f, 1.0f);
        Vec4 rayWorldFar = invViewProj * rayClipFar;
        Vec3 farPoint = Vec3(rayWorldFar.x / rayWorldFar.w,
                            rayWorldFar.y / rayWorldFar.w,
                            rayWorldFar.z / rayWorldFar.w);

        Ray ray;
        ray.origin = nearPoint;
        ray.direction = (farPoint - nearPoint).Normalized();

        hitResult = GizmoUtils::TestGizmo3D(ray, gizmoPos, gizmoSize, pRenderView->getGizmoType(), rotation3D);
    }

    if (hitResult.hit) {

        pRenderView->setGizmoDragging(true);
        pRenderView->setGizmoDragAxis(hitResult.hitAxis);
        pRenderView->setGizmoDragStartWorldPos(gizmoPos);
        pRenderView->setGizmoDragStartScreenPos(Vec2(screenX, screenY));
        pRenderView->setGizmoDragOperation(hitResult.hitOperation);
        pRenderView->setGizmoScaleHandleDir(hitResult.handleDir);
        pRenderView->setGizmoDragRotation2D(rotation2D);
        pRenderView->setGizmoDragHalfExtents(dragHalfExtents);

        auto& initialTransforms = pRenderView->getGizmoDragInitialTransforms();
        initialTransforms.clear();
        for (const auto& node : selectedNodes) {
            lupine::RenderView::NodeInitialTransform initialTransform;

            if (auto node2D = std::dynamic_pointer_cast<Node2D>(node)) {
                initialTransform.is2D = true;
                initialTransform.position2D = node2D->GetPosition();
                initialTransform.rotation2D = node2D->GetRotation();
                initialTransform.scale2D = node2D->GetScale();
            } else if (auto node3D = std::dynamic_pointer_cast<Node3D>(node)) {
                initialTransform.is2D = false;
                initialTransform.position3D = node3D->GetPosition();
                initialTransform.rotation3D = node3D->GetRotation();
                initialTransform.scale3D = node3D->GetScale();
            }

            // Capture initial component properties for components that override OnGizmoScale
            const auto& components = node->GetComponents();
            for (const auto& component : components) {
                std::string typeName;
                nlohmann::json properties;
                if (CaptureComponentScalingProperties(component, typeName, properties)) {
                    initialTransform.scalingComponent = component;
                    initialTransform.componentTypeName = typeName;
                    initialTransform.componentInitialProperties = properties;
                    break;  // Only track the first component that handles scaling
                }
            }

            // Capture the layout state a MOVE has to edit. An anchored control's position is
            // not its Node2D position -- ResolveLayout recomputes the rect from anchors and
            // offsets and writes it back over the node -- so moving one means moving its
            // offsets, and moving a container-driven one is not possible at all.
            if (std::shared_ptr<UIControl> uiControl = node->GetComponent<UIControl>()) {
                initialTransform.layoutControl = uiControl;
                initialTransform.uiControlContainerDriven = uiControl->IsContainerDriven();
                initialTransform.uiControlDrivesPosition =
                    !uiControl->IsContainerDriven() &&
                    uiControl->GetLayoutMode() == LayoutMode::Anchors;
                initialTransform.initialOffsetMin = uiControl->GetOffsetMin();
                initialTransform.initialOffsetMax = uiControl->GetOffsetMax();
            }

            initialTransforms.push_back(initialTransform);
        }

    }
}

void EditorBridge::UpdateGizmoDrag(RenderViewID viewID, float screenX, float screenY, bool keepAspect) {
    auto it = m_RenderViews.find(viewID);
    if (it == m_RenderViews.end() || !m_RenderWorld) {
        return;
    }

    lupine::RenderView* pRenderView = m_RenderWorld->getRenderView(viewID);
    if (!pRenderView || !pRenderView->isGizmoDragging()) {
        return;
    }

    auto selectedNodes = pRenderView->getSelectedNodes();
    if (selectedNodes.empty()) {
        selectedNodes.push_back(pRenderView->getSelectedNode());
    }

    if (selectedNodes.empty() || !selectedNodes[0]) {
        return;
    }

    lupine::RenderCamera* camera = pRenderView->getCamera();
    if (!camera) {
        return;
    }

    lupine::Camera2D* camera2D = dynamic_cast<lupine::Camera2D*>(camera);
    lupine::Camera3D* camera3D = dynamic_cast<lupine::Camera3D*>(camera);

    Vec2 dragStart = pRenderView->getGizmoDragStartScreenPos();
    Vec2 dragCurrent(screenX, screenY);
    Vec2 dragDelta = dragCurrent - dragStart;

    GizmoAxis axis = pRenderView->getGizmoDragAxis();
    GizmoType operation = pRenderView->getGizmoDragOperation();
    TransformSpace transformSpace = pRenderView->getTransformSpace();

    const auto& initialTransforms = pRenderView->getGizmoDragInitialTransforms();
    if (initialTransforms.size() != selectedNodes.size()) {

        return;
    }

    if (operation == GizmoType::Translation) {

        if (camera2D) {
            Vec2 dragStartWorld = ScreenToWorld2D(viewID, dragStart.x, dragStart.y);
            Vec2 dragCurrentWorld = ScreenToWorld2D(viewID, dragCurrent.x, dragCurrent.y);
            Vec2 worldDelta = dragCurrentWorld - dragStartWorld;

            for (size_t i = 0; i < selectedNodes.size(); ++i) {
                auto& node = selectedNodes[i];
                const auto& initialTransform = initialTransforms[i];

                if (auto node2D = std::dynamic_pointer_cast<Node2D>(node)) {
                    Vec2 delta(0.0f, 0.0f);

                    if (axis == GizmoAxis::X || axis == GizmoAxis::XY) {
                        delta.x = worldDelta.x;
                    }
                    if (axis == GizmoAxis::Y || axis == GizmoAxis::XY) {
                        delta.y = worldDelta.y;
                    }

                    // The free-move (XY) handle drags in raw world space; single-axis
                    // arrows are constrained to the local/parent frame.
                    if (axis == GizmoAxis::XY) {
                        // No reorientation: delta already matches the world drag.
                    } else if (transformSpace == TransformSpace::Local) {

                        float rot = initialTransform.rotation2D;
                        float cosRot = std::cos(rot);
                        float sinRot = std::sin(rot);
                        Vec2 rotatedDelta(
                            delta.x * cosRot - delta.y * sinRot,
                            delta.x * sinRot + delta.y * cosRot
                        );
                        delta = rotatedDelta;
                    } else if (transformSpace == TransformSpace::Parent && node2D->GetParent()) {

                        if (auto parent2D = dynamic_cast<Node2D*>(node2D->GetParent())) {
                            float parentRot = parent2D->GetGlobalRotation();
                            float cosRot = std::cos(parentRot);
                            float sinRot = std::sin(parentRot);
                            Vec2 rotatedDelta(
                                delta.x * cosRot - delta.y * sinRot,
                                delta.x * sinRot + delta.y * cosRot
                            );
                            delta = rotatedDelta;
                        }
                    }

                    // Move whatever ACTUALLY places this node.
                    //
                    // A container-driven control has its rect written by its parent every
                    // layout pass, so the drag is refused outright rather than accepted and
                    // silently discarded. An anchored control is placed by its offsets, not
                    // by its Node2D position -- setting the position moved it for exactly as
                    // long as it took ResolveLayout to run, and it snapped back.
                    std::shared_ptr<UIControl> layoutControl = initialTransform.layoutControl.lock();

                    if (initialTransform.uiControlContainerDriven) {
                        // Nothing to move: the parent container owns this rect.
                    } else if (initialTransform.uiControlDrivesPosition && layoutControl) {
                        // Translate both offsets, which slides the rect without resizing it.
                        // X is screen-aligned with the canvas, but the offsets are
                        // screen-oriented on Y (positive moves an edge DOWN) while the canvas
                        // is Y-up -- so a world +Y drag SUBTRACTS from the Y offsets.
                        const Vec2 offsetMin = initialTransform.initialOffsetMin;
                        const Vec2 offsetMax = initialTransform.initialOffsetMax;
                        layoutControl->SetOffsetMin(Vec2(offsetMin.x + delta.x, offsetMin.y - delta.y));
                        layoutControl->SetOffsetMax(Vec2(offsetMax.x + delta.x, offsetMax.y - delta.y));
                    } else {
                        Vec2 newPos = initialTransform.position2D + delta;
                        node2D->SetPosition(newPos);
                    }
                }
            }

        } else if (camera3D) {

            float sensitivity = 0.01f;
            float movement = (dragDelta.x + dragDelta.y) * sensitivity;

            for (size_t i = 0; i < selectedNodes.size(); ++i) {
                auto& node = selectedNodes[i];
                const auto& initialTransform = initialTransforms[i];

                if (auto node3D = std::dynamic_pointer_cast<Node3D>(node)) {
                    Vec3 delta(0.0f, 0.0f, 0.0f);

                    if (axis == GizmoAxis::X) {
                        delta.x = movement;
                    } else if (axis == GizmoAxis::Y) {
                        delta.y = movement;
                    } else if (axis == GizmoAxis::Z) {
                        delta.z = movement;
                    }

                    if (transformSpace == TransformSpace::Local) {

                        delta = initialTransform.rotation3D * delta;
                    } else if (transformSpace == TransformSpace::Parent && node3D->GetParent()) {

                        if (auto parent3D = dynamic_cast<Node3D*>(node3D->GetParent())) {
                            delta = parent3D->GetGlobalRotation() * delta;
                        }
                    }

                    Vec3 newPos = initialTransform.position3D + delta;
                    node3D->SetPosition(newPos);
                }
            }
        }

    } else if (operation == GizmoType::Rotation) {

        float rotationSensitivity = 0.01f;
        float rotationDelta = (dragDelta.x + dragDelta.y) * rotationSensitivity;

        for (size_t i = 0; i < selectedNodes.size(); ++i) {
            auto& node = selectedNodes[i];
            const auto& initialTransform = initialTransforms[i];

            if (camera2D && std::dynamic_pointer_cast<Node2D>(node)) {
                auto node2D = std::dynamic_pointer_cast<Node2D>(node);

                float newRotation = initialTransform.rotation2D + rotationDelta;
                node2D->SetRotation(newRotation);

            } else if (camera3D && std::dynamic_pointer_cast<Node3D>(node)) {
                auto node3D = std::dynamic_pointer_cast<Node3D>(node);

                Vec3 rotationAxis;
                if (axis == GizmoAxis::X) {
                    rotationAxis = Vec3::UnitX();
                } else if (axis == GizmoAxis::Y) {
                    rotationAxis = Vec3::UnitY();
                } else if (axis == GizmoAxis::Z) {
                    rotationAxis = Vec3::UnitZ();
                } else {
                    rotationAxis = Vec3::UnitY();
                }

                if (transformSpace == TransformSpace::Local) {

                    rotationAxis = initialTransform.rotation3D * rotationAxis;
                } else if (transformSpace == TransformSpace::Parent && node3D->GetParent()) {

                    if (auto parent3D = dynamic_cast<Node3D*>(node3D->GetParent())) {
                        rotationAxis = parent3D->GetGlobalRotation() * rotationAxis;
                    }
                }

                Quat rotationQuat = Quat::FromAxisAngle(rotationAxis, rotationDelta);

                node3D->SetRotation(initialTransform.rotation3D * rotationQuat);
            }
        }

    } else if (operation == GizmoType::Scale) {

        if (camera2D) {
            // Direction-aware rectangle scaling: project the world-space drag onto
            // the grabbed handle's local axes and convert it into a proportional
            // factor relative to the selection's half-extents at drag start.
            Vec2 handleDir = pRenderView->getGizmoScaleHandleDir();
            float dragRot = pRenderView->getGizmoDragRotation2D();
            Vec2 halfExtents = pRenderView->getGizmoDragHalfExtents();

            Vec2 dragStartWorld = ScreenToWorld2D(viewID, dragStart.x, dragStart.y);
            Vec2 dragCurrentWorld = ScreenToWorld2D(viewID, dragCurrent.x, dragCurrent.y);
            Vec2 worldDelta = dragCurrentWorld - dragStartWorld;

            float cosR = std::cos(dragRot);
            float sinR = std::sin(dragRot);
            Vec2 localX(cosR, sinR);
            Vec2 localY(-sinR, cosR);

            float dLocalX = worldDelta.x * localX.x + worldDelta.y * localX.y;
            float dLocalY = worldDelta.x * localY.x + worldDelta.y * localY.y;

            float refX = std::max(std::abs(halfExtents.x), 0.001f);
            float refY = std::max(std::abs(halfExtents.y), 0.001f);

            bool uniform = (axis == GizmoAxis::XYZ);
            bool isCorner = (std::abs(handleDir.x) > 0.0f && std::abs(handleDir.y) > 0.0f);
            float factorX = 0.0f;
            float factorY = 0.0f;

            if (uniform) {
                float f = (dLocalX / refX + dLocalY / refY) * 0.5f;
                factorX = f;
                factorY = f;
            } else {
                factorX = handleDir.x * dLocalX / refX;
                factorY = handleDir.y * dLocalY / refY;
            }

            // Holding Shift on a corner preserves the original aspect ratio by
            // driving both axes from the dominant corner displacement.
            if (keepAspect && isCorner) {
                float f = (std::abs(factorX) >= std::abs(factorY)) ? factorX : factorY;
                factorX = f;
                factorY = f;
            }

            bool applyX = uniform || std::abs(handleDir.x) > 0.0f;
            bool applyY = uniform || std::abs(handleDir.y) > 0.0f;

            for (size_t i = 0; i < selectedNodes.size(); ++i) {
                auto& node = selectedNodes[i];
                const auto& initialTransform = initialTransforms[i];

                auto scalingComponent = initialTransform.scalingComponent.lock();
                if (scalingComponent) {
                    // Reset to drag-start size first so each frame scales from the
                    // initial size rather than compounding (OnGizmoScale is relative).
                    ApplyComponentScalingProperties(scalingComponent, initialTransform.componentInitialProperties);

                    if (uniform) {
                        scalingComponent->OnGizmoScale(factorX, -1, false);
                    } else {
                        if (applyX) scalingComponent->OnGizmoScale(factorX, 0, false);
                        if (applyY) scalingComponent->OnGizmoScale(factorY, 1, false);
                    }
                } else if (auto node2D = std::dynamic_pointer_cast<Node2D>(node)) {
                    Vec2 newScale = initialTransform.scale2D;
                    if (applyX) newScale.x = initialTransform.scale2D.x * std::max(0.01f, 1.0f + factorX);
                    if (applyY) newScale.y = initialTransform.scale2D.y * std::max(0.01f, 1.0f + factorY);
                    node2D->SetScale(newScale);
                }
            }

        } else if (camera3D) {

            float totalScaleDelta = (dragDelta.x + dragDelta.y) * 0.005f;

            int axisIndex = -1;
            if (axis == GizmoAxis::X) axisIndex = 0;
            else if (axis == GizmoAxis::Y) axisIndex = 1;
            else if (axis == GizmoAxis::Z) axisIndex = 2;

            for (size_t i = 0; i < selectedNodes.size(); ++i) {
                auto& node = selectedNodes[i];
                const auto& initialTransform = initialTransforms[i];

                bool componentHandledScale = false;
                const auto& components = node->GetComponents();

                for (const auto& component : components) {
                    if (component && component->OnGizmoScale(totalScaleDelta, axisIndex, true)) {
                        componentHandledScale = true;
                    }
                }

                if (!componentHandledScale) {
                    if (auto node3D = std::dynamic_pointer_cast<Node3D>(node)) {
                        Vec3 newScale = initialTransform.scale3D;

                        if (axis == GizmoAxis::X) {
                            newScale.x += totalScaleDelta;
                        } else if (axis == GizmoAxis::Y) {
                            newScale.y += totalScaleDelta;
                        } else if (axis == GizmoAxis::Z) {
                            newScale.z += totalScaleDelta;
                        }

                        node3D->SetScale(newScale);
                    }
                }
            }
        }
    }
}

void EditorBridge::EndGizmoDrag(RenderViewID viewID) {
    auto it = m_RenderViews.find(viewID);
    if (it == m_RenderViews.end() || !m_RenderWorld) {
        return;
    }

    lupine::RenderView* pRenderView = m_RenderWorld->getRenderView(viewID);
    if (!pRenderView) {
        return;
    }

    if (pRenderView->isGizmoDragging()) {

        auto selectedNodes = pRenderView->getSelectedNodes();
        if (selectedNodes.empty()) {
            auto selectedNode = pRenderView->getSelectedNode();
            if (selectedNode) {
                selectedNodes.push_back(selectedNode);
            }
        }

        const auto& initialTransforms = pRenderView->getGizmoDragInitialTransforms();

        if (selectedNodes.size() == initialTransforms.size()) {
            for (size_t i = 0; i < selectedNodes.size(); ++i) {
                const auto& node = selectedNodes[i];
                const auto& initialTransform = initialTransforms[i];

                if (auto node2D = std::dynamic_pointer_cast<Node2D>(node)) {

                    Vec2 finalPos = node2D->GetPosition();
                    float finalRot = node2D->GetRotation();
                    Vec2 finalScale = node2D->GetScale();

                    if (finalPos != initialTransform.position2D) {

                        nlohmann::json oldValue = {{"x", initialTransform.position2D.x}, {"y", initialTransform.position2D.y}};
                        nlohmann::json newValue = {{"x", finalPos.x}, {"y", finalPos.y}};
                        auto cmd = std::make_shared<core::SetNodePropertyCommand>(node, "position", oldValue, newValue);

                        RecordCommand(cmd);
                    }

                    if (std::abs(finalRot - initialTransform.rotation2D) > 0.0001f) {

                        nlohmann::json oldValue = initialTransform.rotation2D;
                        nlohmann::json newValue = finalRot;
                        auto cmd = std::make_shared<core::SetNodePropertyCommand>(node, "rotation", oldValue, newValue);

                        RecordCommand(cmd);
                    }

                    // Check if a component handled scaling
                    auto scalingComponent = initialTransform.scalingComponent.lock();
                    if (scalingComponent) {
                        // Capture final component properties
                        std::string finalTypeName;
                        nlohmann::json finalProperties;
                        if (CaptureComponentScalingProperties(scalingComponent, finalTypeName, finalProperties)) {
                            // Compare each property and create undo commands for changes
                            const nlohmann::json& initialProps = initialTransform.componentInitialProperties;

                            for (auto propIt = initialProps.begin(); propIt != initialProps.end(); ++propIt) {
                                const std::string& propName = propIt.key();
                                const nlohmann::json& oldValue = propIt.value();

                                if (finalProperties.contains(propName)) {
                                    const nlohmann::json& newValue = finalProperties[propName];

                                    // Check if the property actually changed
                                    if (oldValue != newValue) {
                                        auto cmd = std::make_shared<core::SetComponentPropertyCommand>(
                                            scalingComponent, propName, oldValue, newValue);
                                        RecordCommand(cmd);
                                    }
                                }
                            }
                        }
                    } else if (finalScale != initialTransform.scale2D) {
                        // No component handled scaling, use node scale instead
                        nlohmann::json oldValue = {{"x", initialTransform.scale2D.x}, {"y", initialTransform.scale2D.y}};
                        nlohmann::json newValue = {{"x", finalScale.x}, {"y", finalScale.y}};
                        auto cmd = std::make_shared<core::SetNodePropertyCommand>(node, "scale", oldValue, newValue);

                        RecordCommand(cmd);
                    }
                } else if (auto node3D = std::dynamic_pointer_cast<Node3D>(node)) {

                    Vec3 finalPos = node3D->GetPosition();
                    Quat finalRot = node3D->GetRotation();
                    Vec3 finalScale = node3D->GetScale();

                    if (finalPos != initialTransform.position3D) {

                        nlohmann::json oldValue = {{"x", initialTransform.position3D.x}, {"y", initialTransform.position3D.y}, {"z", initialTransform.position3D.z}};
                        nlohmann::json newValue = {{"x", finalPos.x}, {"y", finalPos.y}, {"z", finalPos.z}};
                        auto cmd = std::make_shared<core::SetNodePropertyCommand>(node, "position", oldValue, newValue);
                        RecordCommand(cmd);
                    }

                    if (finalRot != initialTransform.rotation3D) {

                        nlohmann::json oldValue = {{"x", initialTransform.rotation3D.x()}, {"y", initialTransform.rotation3D.y()}, {"z", initialTransform.rotation3D.z()}, {"w", initialTransform.rotation3D.w()}};
                        nlohmann::json newValue = {{"x", finalRot.x()}, {"y", finalRot.y()}, {"z", finalRot.z()}, {"w", finalRot.w()}};
                        auto cmd = std::make_shared<core::SetNodePropertyCommand>(node, "rotation", oldValue, newValue);
                        RecordCommand(cmd);
                    }

                    // Check if a component handled scaling (for 3D components)
                    auto scalingComponent = initialTransform.scalingComponent.lock();
                    if (scalingComponent) {
                        // Capture final component properties
                        std::string finalTypeName;
                        nlohmann::json finalProperties;
                        if (CaptureComponentScalingProperties(scalingComponent, finalTypeName, finalProperties)) {
                            // Compare each property and create undo commands for changes
                            const nlohmann::json& initialProps = initialTransform.componentInitialProperties;

                            for (auto propIt = initialProps.begin(); propIt != initialProps.end(); ++propIt) {
                                const std::string& propName = propIt.key();
                                const nlohmann::json& oldValue = propIt.value();

                                if (finalProperties.contains(propName)) {
                                    const nlohmann::json& newValue = finalProperties[propName];

                                    // Check if the property actually changed
                                    if (oldValue != newValue) {
                                        auto cmd = std::make_shared<core::SetComponentPropertyCommand>(
                                            scalingComponent, propName, oldValue, newValue);
                                        RecordCommand(cmd);
                                    }
                                }
                            }
                        }
                    } else if (finalScale != initialTransform.scale3D) {
                        // No component handled scaling, use node scale instead
                        nlohmann::json oldValue = {{"x", initialTransform.scale3D.x}, {"y", initialTransform.scale3D.y}, {"z", initialTransform.scale3D.z}};
                        nlohmann::json newValue = {{"x", finalScale.x}, {"y", finalScale.y}, {"z", finalScale.z}};
                        auto cmd = std::make_shared<core::SetNodePropertyCommand>(node, "scale", oldValue, newValue);
                        RecordCommand(cmd);
                    }
                }
            }
        }

        pRenderView->setGizmoDragging(false);
        pRenderView->setGizmoDragAxis(GizmoAxis::None);
    }
}

bool EditorBridge::IsGizmoDragging(RenderViewID viewID) const {
    if (!m_RenderWorld) {
        return false;
    }

    lupine::RenderView* pRenderView = m_RenderWorld->getRenderView(viewID);
    if (!pRenderView) {
        return false;
    }

    return pRenderView->isGizmoDragging();
}

}
}
