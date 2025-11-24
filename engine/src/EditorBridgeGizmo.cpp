

#include "lupine/engine/EditorBridge.hpp"
#include "lupine/rendering/GizmoUtils.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/EditorCommands.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/RenderView.hpp"
#include "lupine/rendering/RenderCamera.hpp"
#include <algorithm>

namespace lupine {
namespace engine {

using namespace core;
using namespace math;

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

        hitResult = GizmoUtils::TestGizmo2D(
            Vec2(screenX, screenY),
            gizmoPos,
            gizmoSize,
            pRenderView->getGizmoType(),
            viewMatrix,
            projMatrix,
            viewportWidth,
            viewportHeight,
            rotation2D
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

    if (is2D) {
        float aspectRatio = pRenderView->getAspectRatio();
        Mat4 viewMatrix = camera->getViewMatrix();
        Mat4 projMatrix = camera->getProjectionMatrix(aspectRatio);

        hitResult = GizmoUtils::TestGizmo2D(
            Vec2(screenX, screenY),
            gizmoPos,
            gizmoSize,
            pRenderView->getGizmoType(),
            viewMatrix,
            projMatrix,
            viewportWidth,
            viewportHeight,
            rotation2D
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

            initialTransforms.push_back(initialTransform);
        }

    }
}

void EditorBridge::UpdateGizmoDrag(RenderViewID viewID, float screenX, float screenY) {
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
            Vec2 worldDelta = dragDelta / camera2D->zoom;

            for (size_t i = 0; i < selectedNodes.size(); ++i) {
                auto& node = selectedNodes[i];
                const auto& initialTransform = initialTransforms[i];

                if (auto node2D = std::dynamic_pointer_cast<Node2D>(node)) {
                    Vec2 delta(0.0f, 0.0f);

                    if (axis == GizmoAxis::X) {
                        delta.x = worldDelta.x;
                    } else if (axis == GizmoAxis::Y) {
                        delta.y = -worldDelta.y;
                    }

                    if (transformSpace == TransformSpace::Local) {

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

                    Vec2 newPos = initialTransform.position2D + delta;
                    node2D->SetPosition(newPos);
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

        float scaleSensitivity = camera2D ? 0.0005f : 0.005f;
        float totalScaleDelta = (dragDelta.x + dragDelta.y) * scaleSensitivity;

        for (size_t i = 0; i < selectedNodes.size(); ++i) {
            auto& node = selectedNodes[i];
            const auto& initialTransform = initialTransforms[i];

            bool componentHandledScale = false;
            const auto& components = node->GetComponents();

            int axisIndex = -1;
            if (axis == GizmoAxis::X) axisIndex = 0;
            else if (axis == GizmoAxis::Y) axisIndex = 1;
            else if (axis == GizmoAxis::Z) axisIndex = 2;

            bool is3D = (camera3D != nullptr);

            for (const auto& component : components) {
                if (component && component->OnGizmoScale(totalScaleDelta, axisIndex, is3D)) {
                    componentHandledScale = true;

                }
            }

            if (!componentHandledScale) {
                if (camera2D && std::dynamic_pointer_cast<Node2D>(node)) {
                    auto node2D = std::dynamic_pointer_cast<Node2D>(node);
                    Vec2 newScale = initialTransform.scale2D;

                    if (axis == GizmoAxis::X) {
                        newScale.x += totalScaleDelta;
                    } else if (axis == GizmoAxis::Y) {
                        newScale.y += totalScaleDelta;
                    }

                    node2D->SetScale(newScale);

                } else if (camera3D && std::dynamic_pointer_cast<Node3D>(node)) {
                    auto node3D = std::dynamic_pointer_cast<Node3D>(node);
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

                    if (finalScale != initialTransform.scale2D) {

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

                    if (finalScale != initialTransform.scale3D) {

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
