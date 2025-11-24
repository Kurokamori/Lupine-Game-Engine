#include "lupine/components/CharacterController2D.hpp"
#include "lupine/components/CollisionBody2DComponent.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/SceneManager.hpp"
#include "lupine/physics2d/Physics2DWorld.hpp"
#include "lupine/logger/Logger.hpp"
#include <algorithm>

namespace lupine {
namespace components {

using namespace core;
using namespace math;
using namespace physics2d;

CharacterController2D::CharacterController2D()
    : Component("CharacterController2D")
    , m_KinematicBody(nullptr)
    , m_CollisionShape(nullptr)
    , m_Velocity(Vec2::Zero())
    , m_LastMovement(Vec2::Zero())
    , m_IsOnGround(false)
    , m_IsOnWall(false)
    , m_IsOnCeiling(false)
    , m_GroundNormal(Vec2(0.0f, 1.0f))
    , m_WallNormal(Vec2::Zero())
    , m_TimeSinceGrounded(0.0f)
    , m_Gravity(-980.0f)
    , m_MaxFallSpeed(-1000.0f)
    , m_GroundDetectionDistance(2.0f)
    , m_WallDetectionDistance(2.0f)
    , m_MaxSlopeAngle(45.0f)
    , m_SnapToGround(true)
    , m_MaxBounces(4)
{
}

CharacterController2D::CharacterController2D(const std::string& name)
    : Component(name)
    , m_KinematicBody(nullptr)
    , m_CollisionShape(nullptr)
    , m_Velocity(Vec2::Zero())
    , m_LastMovement(Vec2::Zero())
    , m_IsOnGround(false)
    , m_IsOnWall(false)
    , m_IsOnCeiling(false)
    , m_GroundNormal(Vec2(0.0f, 1.0f))
    , m_WallNormal(Vec2::Zero())
    , m_TimeSinceGrounded(0.0f)
    , m_Gravity(-980.0f)
    , m_MaxFallSpeed(-1000.0f)
    , m_GroundDetectionDistance(2.0f)
    , m_WallDetectionDistance(2.0f)
    , m_MaxSlopeAngle(45.0f)
    , m_SnapToGround(true)
    , m_MaxBounces(4)
{
}

CharacterController2D::~CharacterController2D() {
    ClearKinematicBodyReference();
}

void CharacterController2D::DefineProperties() {
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(gravity, -980.0f, -2000.0f, 0.0f, 10.0f, "Physics"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(maxFallSpeed, -1000.0f, -2000.0f, 0.0f, 10.0f, "Physics"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(groundDetectionDistance, 2.0f, 0.1f, 10.0f, 0.1f, "Detection"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(wallDetectionDistance, 2.0f, 0.1f, 10.0f, 0.1f, "Detection"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(maxSlopeAngle, 45.0f, 0.0f, 89.0f, 1.0f, "Movement"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(snapToGround, Bool, true, "Movement"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(maxBounces, 4, 1, 10, 1, "Movement"));
}

void CharacterController2D::OnAwake() {
    FindKinematicBody();
    FindCollisionShape();
}

void CharacterController2D::OnDestroy() {
    ClearKinematicBodyReference();
}

void CharacterController2D::OnPhysicsProcess(float deltaTime) {
    if (!m_KinematicBody || !m_Owner) return;

    // Sync all property values with member variables
    m_Gravity = GetPropertyValue<float>("gravity");
    m_MaxFallSpeed = GetPropertyValue<float>("maxFallSpeed");
    m_GroundDetectionDistance = GetPropertyValue<float>("groundDetectionDistance");
    m_WallDetectionDistance = GetPropertyValue<float>("wallDetectionDistance");
    m_MaxSlopeAngle = GetPropertyValue<float>("maxSlopeAngle");
    m_SnapToGround = GetPropertyValue<bool>("snapToGround");
    m_MaxBounces = GetPropertyValue<int>("maxBounces");

    if (m_Gravity != 0.0f) {

        if (!m_IsOnGround) {
            m_Velocity.y += m_Gravity * deltaTime;

            if (m_Velocity.y < m_MaxFallSpeed) {
                m_Velocity.y = m_MaxFallSpeed;
            }
        } else {

            if (m_Velocity.y < 0.0f) {
                m_Velocity.y = 0.0f;
            }
        }
    }

    Vec2 actualMovement = MoveAndSlide(m_Velocity, deltaTime);
    m_LastMovement = actualMovement;

    UpdateGroundDetection();
    UpdateWallDetection();

    if (m_IsOnGround) {
        m_TimeSinceGrounded = 0.0f;
    } else {
        m_TimeSinceGrounded += deltaTime;
    }

    static int frameCount = 0;
    frameCount++;
    if (frameCount % 60 == 0) {
        float radius = GetCollisionRadius();

    }
}

Vec2 CharacterController2D::MoveAndSlide(const Vec2& velocity, float deltaTime) {
    if (!m_KinematicBody) return Vec2::Zero();

    Vec2 movement = velocity * deltaTime;
    Vec2 actualMovement = PerformSweptCollision(movement, deltaTime);

    if (m_KinematicBody->GetPhysicsBody()) {
        Vec2 currentPos = m_KinematicBody->GetPhysicsBody()->GetPosition();
        Vec2 newPos = currentPos + actualMovement;
        m_KinematicBody->GetPhysicsBody()->SetPosition(newPos);
    }

    return actualMovement;
}

bool CharacterController2D::MoveAndCollide(const Vec2& velocity, float deltaTime, Vec2& outActualMovement) {
    if (!m_KinematicBody || !m_KinematicBody->GetPhysicsBody()) {
        outActualMovement = Vec2::Zero();
        return false;
    }

    Vec2 movement = velocity * deltaTime;

    auto* sceneManager = SceneManager::GetInstance();
    if (!sceneManager) {
        outActualMovement = movement;
        return false;
    }

    auto* physicsWorld = sceneManager->GetPhysics2DWorld();
    if (!physicsWorld) {
        outActualMovement = movement;
        return false;
    }

    Vec2 currentPos = m_KinematicBody->GetPhysicsBody()->GetPosition();
    float movementLength = movement.Length();

    if (movementLength < 0.001f) {
        outActualMovement = Vec2::Zero();
        return false;
    }

    Vec2 direction = movement / movementLength;

    ShapeCastHit2D hit;
    bool hasCollision = physicsWorld->ShapeCast(currentPos, currentPos + movement, 1.0f, hit);

    if (hasCollision) {

        outActualMovement = direction * (movementLength * hit.fraction);

        Vec2 newPos = currentPos + outActualMovement;
        m_KinematicBody->GetPhysicsBody()->SetPosition(newPos);
        return true;
    }

    outActualMovement = movement;
    Vec2 newPos = currentPos + outActualMovement;
    m_KinematicBody->GetPhysicsBody()->SetPosition(newPos);
    return false;
}

void CharacterController2D::SetVelocity(const Vec2& velocity) {
    m_Velocity = velocity;
}

float CharacterController2D::GetGravity() const {
    return GetPropertyValue<float>("gravity");
}

void CharacterController2D::SetGravity(float gravity) {
    SetPropertyValue("gravity", gravity);
    m_Gravity = gravity;
}

float CharacterController2D::GetMaxFallSpeed() const {
    return GetPropertyValue<float>("maxFallSpeed");
}

void CharacterController2D::SetMaxFallSpeed(float speed) {
    SetPropertyValue("maxFallSpeed", speed);
    m_MaxFallSpeed = speed;
}

float CharacterController2D::GetGroundDetectionDistance() const {
    return GetPropertyValue<float>("groundDetectionDistance");
}

void CharacterController2D::SetGroundDetectionDistance(float distance) {
    SetPropertyValue("groundDetectionDistance", distance);
    m_GroundDetectionDistance = distance;
}

float CharacterController2D::GetWallDetectionDistance() const {
    return GetPropertyValue<float>("wallDetectionDistance");
}

void CharacterController2D::SetWallDetectionDistance(float distance) {
    SetPropertyValue("wallDetectionDistance", distance);
    m_WallDetectionDistance = distance;
}

float CharacterController2D::GetMaxSlopeAngle() const {
    return GetPropertyValue<float>("maxSlopeAngle");
}

void CharacterController2D::SetMaxSlopeAngle(float angle) {
    SetPropertyValue("maxSlopeAngle", angle);
    m_MaxSlopeAngle = angle;
}

bool CharacterController2D::GetSnapToGround() const {
    return GetPropertyValue<bool>("snapToGround");
}

void CharacterController2D::SetSnapToGround(bool snap) {
    SetPropertyValue("snapToGround", snap);
    m_SnapToGround = snap;
}

int CharacterController2D::GetMaxBounces() const {
    return GetPropertyValue<int>("maxBounces");
}

void CharacterController2D::SetMaxBounces(int bounces) {
    SetPropertyValue("maxBounces", bounces);
    m_MaxBounces = bounces;
}

void CharacterController2D::UpdateGroundDetection() {
    if (!m_KinematicBody || !m_KinematicBody->GetPhysicsBody()) return;

    auto* sceneManager = SceneManager::GetInstance();
    if (!sceneManager) return;

    auto* physicsWorld = sceneManager->GetPhysics2DWorld();
    if (!physicsWorld) return;

    Vec2 currentPos = m_KinematicBody->GetPhysicsBody()->GetPosition();
    Vec2 downDirection(0.0f, -1.0f);

    float collisionRadius = GetCollisionRadius();

    float raycastDistance = collisionRadius + m_GroundDetectionDistance;

    const UUID& ignoreBodyId = m_KinematicBody->GetPhysicsBody()->GetId();

    RaycastHit2D hit;
    bool hasGround = physicsWorld->Raycast(currentPos, downDirection, raycastDistance, hit, &ignoreBodyId);

    static int debugFrameCount = 0;
    debugFrameCount++;
    if (debugFrameCount % 60 == 0) {

        Vec2 nodePos = Vec2::Zero();
        if (m_Owner) {
            auto* node2D = dynamic_cast<core::Node2D*>(m_Owner);
            if (node2D) {
                nodePos = node2D->GetPosition();
            }
        }

        if (hasGround) {

        }
    }

    if (hasGround && IsFloor(hit.normal)) {

        float characterBottom = currentPos.y - collisionRadius;
        float distanceToGround = characterBottom - hit.point.y;

        if (std::abs(distanceToGround) <= m_GroundDetectionDistance) {
            m_IsOnGround = true;
            m_GroundNormal = hit.normal;

            if (m_Velocity.y < 0.0f) {
                m_Velocity.y = 0.0f;
            }
        } else {
            m_IsOnGround = false;
            m_GroundNormal = Vec2(0.0f, 1.0f);
        }
    } else {
        m_IsOnGround = false;
        m_GroundNormal = Vec2(0.0f, 1.0f);
    }
}

void CharacterController2D::UpdateWallDetection() {
    if (!m_KinematicBody || !m_KinematicBody->GetPhysicsBody()) return;

    auto* sceneManager = SceneManager::GetInstance();
    if (!sceneManager) return;

    auto* physicsWorld = sceneManager->GetPhysics2DWorld();
    if (!physicsWorld) return;

    Vec2 currentPos = m_KinematicBody->GetPhysicsBody()->GetPosition();

    Vec2 leftDirection(-1.0f, 0.0f);
    Vec2 rightDirection(1.0f, 0.0f);

    const UUID& ignoreBodyId = m_KinematicBody->GetPhysicsBody()->GetId();

    RaycastHit2D leftHit, rightHit;
    bool hasLeftWall = physicsWorld->Raycast(currentPos, leftDirection, m_WallDetectionDistance, leftHit, &ignoreBodyId);
    bool hasRightWall = physicsWorld->Raycast(currentPos, rightDirection, m_WallDetectionDistance, rightHit, &ignoreBodyId);

    if (hasLeftWall && IsWall(leftHit.normal)) {
        m_IsOnWall = true;
        m_WallNormal = leftHit.normal;
    } else if (hasRightWall && IsWall(rightHit.normal)) {
        m_IsOnWall = true;
        m_WallNormal = rightHit.normal;
    } else {
        m_IsOnWall = false;
        m_WallNormal = Vec2::Zero();
    }
}

Vec2 CharacterController2D::PerformSweptCollision(const Vec2& movement, float deltaTime) {
    if (!m_KinematicBody || !m_KinematicBody->GetPhysicsBody()) return Vec2::Zero();

    auto* sceneManager = SceneManager::GetInstance();
    if (!sceneManager) return movement;

    auto* physicsWorld = sceneManager->GetPhysics2DWorld();
    if (!physicsWorld) return movement;

    Vec2 currentPos = m_KinematicBody->GetPhysicsBody()->GetPosition();
    Vec2 remainingMovement = movement;
    Vec2 totalMovement = Vec2::Zero();

    const UUID& ignoreBodyId = m_KinematicBody->GetPhysicsBody()->GetId();

    float collisionRadius = GetCollisionRadius();

    static int debugFrameCount = 0;
    debugFrameCount++;
    bool shouldLog = (debugFrameCount % 60 == 0) && movement.LengthSquared() > 0.0001f;

    if (shouldLog) {

    }

    for (int bounce = 0; bounce < m_MaxBounces && remainingMovement.LengthSquared() > 0.0001f; ++bounce) {
        float movementLength = remainingMovement.Length();
        Vec2 direction = remainingMovement / movementLength;

        ShapeCastHit2D hit;
        bool hasCollision = physicsWorld->ShapeCast(currentPos + totalMovement,
                                                     currentPos + totalMovement + remainingMovement,
                                                     collisionRadius, hit, &ignoreBodyId);

        if (hasCollision) {
            if (shouldLog) {

            }

            float normalLength = hit.normal.Length();
            if (normalLength < 0.001f) {

                if (hit.fraction == 0.0f) {

                    Vec2 depenetrationOffset(0.0f, 0.1f);
                    totalMovement += depenetrationOffset;

                    if (shouldLog) {

                    }
                }

                break;
            }

            if (hit.fraction > 0.0f) {

                Vec2 safeMovement = direction * (movementLength * hit.fraction * 0.99f);
                totalMovement += safeMovement;
            }

            Vec2 slideVelocity = SlideAlongSurface(remainingMovement, hit.normal);
            remainingMovement = slideVelocity;

            if (IsFloor(hit.normal)) {
                m_IsOnGround = true;
                m_GroundNormal = hit.normal;
                if (m_Velocity.y < 0.0f) {
                    m_Velocity.y = 0.0f;
                }
            } else if (IsCeiling(hit.normal)) {
                m_IsOnCeiling = true;
                if (m_Velocity.y > 0.0f) {
                    m_Velocity.y = 0.0f;
                }
            } else if (IsWall(hit.normal)) {
                m_IsOnWall = true;
                m_WallNormal = hit.normal;
            }
        } else {

            totalMovement += remainingMovement;
            break;
        }
    }

    if (shouldLog) {

    }

    return totalMovement;
}

Vec2 CharacterController2D::SlideAlongSurface(const Vec2& velocity, const Vec2& normal) {

    float dot = velocity.Dot(normal);
    return velocity - (normal * dot);
}

bool CharacterController2D::IsFloor(const Vec2& normal) const {

    float angle = std::acos(normal.Dot(Vec2(0.0f, 1.0f))) * (180.0f / 3.14159265f);
    return angle <= m_MaxSlopeAngle;
}

bool CharacterController2D::IsWall(const Vec2& normal) const {

    return !IsFloor(normal) && !IsCeiling(normal);
}

bool CharacterController2D::IsCeiling(const Vec2& normal) const {

    float angle = std::acos(normal.Dot(Vec2(0.0f, -1.0f))) * (180.0f / 3.14159265f);
    return angle <= m_MaxSlopeAngle;
}

void CharacterController2D::FindKinematicBody() {
    if (!m_Owner) return;

    auto kinematicBody = m_Owner->GetComponent<KinematicBody2DComponent>();
    if (kinematicBody) {
        m_KinematicBody = kinematicBody.get();
        return;
    }

    for (auto& child : m_Owner->GetChildren()) {
        kinematicBody = child->GetComponent<KinematicBody2DComponent>();
        if (kinematicBody) {
            m_KinematicBody = kinematicBody.get();
            return;
        }

        for (auto& grandchild : child->GetChildren()) {
            kinematicBody = grandchild->GetComponent<KinematicBody2DComponent>();
            if (kinematicBody) {
                m_KinematicBody = kinematicBody.get();
                return;
            }
        }
    }

}

void CharacterController2D::FindCollisionShape() {
    if (!m_Owner) {
        m_CollisionShape = nullptr;
        return;
    }

    auto collisionShape = m_Owner->GetComponent<CollisionBody2DComponent>();
    if (collisionShape) {

        m_CollisionShape = collisionShape.get();
        return;
    }

    for (auto& child : m_Owner->GetChildren()) {

        collisionShape = child->GetComponent<CollisionBody2DComponent>();
        if (collisionShape) {

            m_CollisionShape = collisionShape.get();
            return;
        }

        for (auto& grandchild : child->GetChildren()) {

            collisionShape = grandchild->GetComponent<CollisionBody2DComponent>();
            if (collisionShape) {

                m_CollisionShape = collisionShape.get();
                return;
            }
        }
    }

    m_CollisionShape = nullptr;
}

float CharacterController2D::GetCollisionRadius() const {
    if (!m_CollisionShape) {
        return 50.0f;
    }

    CollisionShape2DType shapeType = m_CollisionShape->GetShapeType();

    switch (shapeType) {
        case CollisionShape2DType::Circle:
            return m_CollisionShape->GetRadius();

        case CollisionShape2DType::Rectangle: {

            Vec2 size = m_CollisionShape->GetSize();
            return std::min(size.x, size.y) / 2.0f;
        }

        case CollisionShape2DType::Triangle:
        case CollisionShape2DType::Pentagon:
        case CollisionShape2DType::Hexagon:
        case CollisionShape2DType::Polygon: {

            return m_CollisionShape->GetRadius();
        }

        default:
            return 50.0f;
    }
}

void CharacterController2D::ClearKinematicBodyReference() {
    m_KinematicBody = nullptr;
}

}
}

