#include "lupine/components/CharacterController3D.hpp"
#include "lupine/components/CollisionMesh3DComponent.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/SceneManager.hpp"
#include "lupine/physics3d/Physics3DWorld.hpp"
#include "lupine/logger/Logger.hpp"
#include <algorithm>

namespace lupine {
namespace components {

using namespace core;
using namespace math;
using namespace physics3d;

CharacterController3D::CharacterController3D()
    : Component("CharacterController3D")
    , m_KinematicBody(nullptr)
    , m_CollisionShape(nullptr)
    , m_Velocity(Vec3::Zero())
    , m_LastMovement(Vec3::Zero())
    , m_IsOnGround(false)
    , m_IsOnWall(false)
    , m_IsOnCeiling(false)
    , m_GroundNormal(Vec3(0.0f, 1.0f, 0.0f))
    , m_WallNormal(Vec3::Zero())
    , m_TimeSinceGrounded(0.0f)
    , m_Gravity(-9.8f)
    , m_MaxFallSpeed(-50.0f)
    , m_GroundDetectionDistance(0.1f)
    , m_WallDetectionDistance(0.1f)
    , m_MaxSlopeAngle(45.0f)
    , m_StepHeight(0.3f)
    , m_SnapToGround(true)
    , m_MaxBounces(4)
{
}

CharacterController3D::CharacterController3D(const std::string& name)
    : Component(name)
    , m_KinematicBody(nullptr)
    , m_CollisionShape(nullptr)
    , m_Velocity(Vec3::Zero())
    , m_LastMovement(Vec3::Zero())
    , m_IsOnGround(false)
    , m_IsOnWall(false)
    , m_IsOnCeiling(false)
    , m_GroundNormal(Vec3(0.0f, 1.0f, 0.0f))
    , m_WallNormal(Vec3::Zero())
    , m_TimeSinceGrounded(0.0f)
    , m_Gravity(-9.8f)
    , m_MaxFallSpeed(-50.0f)
    , m_GroundDetectionDistance(0.1f)
    , m_WallDetectionDistance(0.1f)
    , m_MaxSlopeAngle(45.0f)
    , m_StepHeight(0.3f)
    , m_SnapToGround(true)
    , m_MaxBounces(4)
{
}

CharacterController3D::~CharacterController3D() {
    ClearKinematicBodyReference();
}

void CharacterController3D::DefineProperties() {
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(gravity, -9.8f, -50.0f, 0.0f, 0.1f, "Physics"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(maxFallSpeed, -50.0f, -100.0f, 0.0f, 1.0f, "Physics"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(groundDetectionDistance, 0.1f, 0.01f, 1.0f, 0.01f, "Detection"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(wallDetectionDistance, 0.1f, 0.01f, 1.0f, 0.01f, "Detection"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(maxSlopeAngle, 45.0f, 0.0f, 89.0f, 1.0f, "Movement"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(stepHeight, 0.3f, 0.0f, 1.0f, 0.05f, "Movement"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(snapToGround, Bool, true, "Movement"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(maxBounces, 4, 1, 10, 1, "Movement"));
}

void CharacterController3D::OnAwake() {
    FindKinematicBody();
    FindCollisionShape();
}

void CharacterController3D::OnDestroy() {
    ClearKinematicBodyReference();
}

void CharacterController3D::OnPhysicsProcess(float deltaTime) {
    if (!m_KinematicBody || !m_Owner) return;

    // Sync all property values with member variables
    m_Gravity = GetPropertyValue<float>("gravity");
    m_MaxFallSpeed = GetPropertyValue<float>("maxFallSpeed");
    m_GroundDetectionDistance = GetPropertyValue<float>("groundDetectionDistance");
    m_WallDetectionDistance = GetPropertyValue<float>("wallDetectionDistance");
    m_MaxSlopeAngle = GetPropertyValue<float>("maxSlopeAngle");
    m_StepHeight = GetPropertyValue<float>("stepHeight");
    m_SnapToGround = GetPropertyValue<bool>("snapToGround");
    m_MaxBounces = GetPropertyValue<int>("maxBounces");

    if (m_Gravity != 0.0f) {

        m_Velocity.y += m_Gravity * deltaTime;

        if (m_Velocity.y < m_MaxFallSpeed) {
            m_Velocity.y = m_MaxFallSpeed;
        }
    }

    Vec3 actualMovement = MoveAndSlide(m_Velocity, deltaTime);
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

Vec3 CharacterController3D::MoveAndSlide(const Vec3& velocity, float deltaTime) {
    if (!m_KinematicBody) return Vec3::Zero();

    Vec3 movement = velocity * deltaTime;
    Vec3 actualMovement = PerformSweptCollision(movement, deltaTime);

    if (m_KinematicBody->GetPhysicsBody()) {
        Vec3 currentPos = m_KinematicBody->GetPhysicsBody()->GetPosition();
        Vec3 newPos = currentPos + actualMovement;
        m_KinematicBody->GetPhysicsBody()->SetPosition(newPos);
    }

    return actualMovement;
}

bool CharacterController3D::MoveAndCollide(const Vec3& velocity, float deltaTime, Vec3& outActualMovement) {
    if (!m_KinematicBody || !m_KinematicBody->GetPhysicsBody()) {
        outActualMovement = Vec3::Zero();
        return false;
    }

    Vec3 movement = velocity * deltaTime;

    auto* sceneManager = SceneManager::GetInstance();
    if (!sceneManager) {
        outActualMovement = movement;
        return false;
    }

    auto* physicsWorld = sceneManager->GetPhysics3DWorld();
    if (!physicsWorld) {
        outActualMovement = movement;
        return false;
    }

    Vec3 currentPos = m_KinematicBody->GetPhysicsBody()->GetPosition();
    float movementLength = movement.Length();

    if (movementLength < 0.001f) {
        outActualMovement = Vec3::Zero();
        return false;
    }

    Vec3 direction = movement / movementLength;

    ShapeCastHit3D hit;
    bool hasCollision = physicsWorld->SphereCast(currentPos, currentPos + movement, 0.5f, hit);

    if (hasCollision) {

        outActualMovement = direction * (movementLength * hit.fraction);

        Vec3 newPos = currentPos + outActualMovement;
        m_KinematicBody->GetPhysicsBody()->SetPosition(newPos);
        return true;
    }

    outActualMovement = movement;
    Vec3 newPos = currentPos + outActualMovement;
    m_KinematicBody->GetPhysicsBody()->SetPosition(newPos);
    return false;
}

void CharacterController3D::SetVelocity(const Vec3& velocity) {
    m_Velocity = velocity;
}

float CharacterController3D::GetGravity() const {
    return GetPropertyValue<float>("gravity");
}

void CharacterController3D::SetGravity(float gravity) {
    SetPropertyValue("gravity", gravity);
    m_Gravity = gravity;
}

float CharacterController3D::GetMaxFallSpeed() const {
    return GetPropertyValue<float>("maxFallSpeed");
}

void CharacterController3D::SetMaxFallSpeed(float speed) {
    SetPropertyValue("maxFallSpeed", speed);
    m_MaxFallSpeed = speed;
}

float CharacterController3D::GetGroundDetectionDistance() const {
    return GetPropertyValue<float>("groundDetectionDistance");
}

void CharacterController3D::SetGroundDetectionDistance(float distance) {
    SetPropertyValue("groundDetectionDistance", distance);
    m_GroundDetectionDistance = distance;
}

float CharacterController3D::GetWallDetectionDistance() const {
    return GetPropertyValue<float>("wallDetectionDistance");
}

void CharacterController3D::SetWallDetectionDistance(float distance) {
    SetPropertyValue("wallDetectionDistance", distance);
    m_WallDetectionDistance = distance;
}

float CharacterController3D::GetMaxSlopeAngle() const {
    return GetPropertyValue<float>("maxSlopeAngle");
}

void CharacterController3D::SetMaxSlopeAngle(float angle) {
    SetPropertyValue("maxSlopeAngle", angle);
    m_MaxSlopeAngle = angle;
}

float CharacterController3D::GetStepHeight() const {
    return GetPropertyValue<float>("stepHeight");
}

void CharacterController3D::SetStepHeight(float height) {
    SetPropertyValue("stepHeight", height);
    m_StepHeight = height;
}

bool CharacterController3D::GetSnapToGround() const {
    return GetPropertyValue<bool>("snapToGround");
}

void CharacterController3D::SetSnapToGround(bool snap) {
    SetPropertyValue("snapToGround", snap);
    m_SnapToGround = snap;
}

int CharacterController3D::GetMaxBounces() const {
    return GetPropertyValue<int>("maxBounces");
}

void CharacterController3D::SetMaxBounces(int bounces) {
    SetPropertyValue("maxBounces", bounces);
    m_MaxBounces = bounces;
}

void CharacterController3D::UpdateGroundDetection() {
    if (!m_KinematicBody || !m_KinematicBody->GetPhysicsBody()) return;

    auto* sceneManager = SceneManager::GetInstance();
    if (!sceneManager) return;

    auto* physicsWorld = sceneManager->GetPhysics3DWorld();
    if (!physicsWorld) return;

    Vec3 currentPos = m_KinematicBody->GetPhysicsBody()->GetPosition();
    Vec3 downDirection(0.0f, -1.0f, 0.0f);

    float collisionRadius = GetCollisionRadius();

    float raycastDistance = collisionRadius + m_GroundDetectionDistance;

    const UUID& ignoreBodyId = m_KinematicBody->GetPhysicsBody()->GetId();

    RaycastHit3D hit;
    bool hasGround = physicsWorld->Raycast(currentPos, downDirection, raycastDistance, hit, &ignoreBodyId);

    static int debugFrameCount = 0;
    debugFrameCount++;
    if (debugFrameCount % 60 == 0) {

        if (hasGround) {

        }
    }

    if (hasGround) {
        m_IsOnGround = IsFloor(hit.normal);
        if (m_IsOnGround) {
            m_GroundNormal = hit.normal;

            if (m_Velocity.y < 0.0f) {
                m_Velocity.y = 0.0f;
            }
        }
    } else {
        m_IsOnGround = false;
        m_GroundNormal = Vec3(0.0f, 1.0f, 0.0f);
    }
}

void CharacterController3D::UpdateWallDetection() {
    if (!m_KinematicBody || !m_KinematicBody->GetPhysicsBody()) return;

    auto* sceneManager = SceneManager::GetInstance();
    if (!sceneManager) return;

    auto* physicsWorld = sceneManager->GetPhysics3DWorld();
    if (!physicsWorld) return;

    Vec3 currentPos = m_KinematicBody->GetPhysicsBody()->GetPosition();

    Vec3 directions[] = {
        Vec3(1.0f, 0.0f, 0.0f),
        Vec3(-1.0f, 0.0f, 0.0f),
        Vec3(0.0f, 0.0f, 1.0f),
        Vec3(0.0f, 0.0f, -1.0f)
    };

    m_IsOnWall = false;
    for (const Vec3& dir : directions) {
        RaycastHit3D hit;
        bool hasWall = physicsWorld->Raycast(currentPos, dir, m_WallDetectionDistance, hit);

        if (hasWall && IsWall(hit.normal)) {
            m_IsOnWall = true;
            m_WallNormal = hit.normal;
            break;
        }
    }

    if (!m_IsOnWall) {
        m_WallNormal = Vec3::Zero();
    }
}

Vec3 CharacterController3D::PerformSweptCollision(const Vec3& movement, float deltaTime) {
    if (!m_KinematicBody || !m_KinematicBody->GetPhysicsBody()) return Vec3::Zero();

    auto* sceneManager = SceneManager::GetInstance();
    if (!sceneManager) return movement;

    auto* physicsWorld = sceneManager->GetPhysics3DWorld();
    if (!physicsWorld) return movement;

    Vec3 currentPos = m_KinematicBody->GetPhysicsBody()->GetPosition();
    Vec3 remainingMovement = movement;
    Vec3 totalMovement = Vec3::Zero();

    const UUID& ignoreBodyId = m_KinematicBody->GetPhysicsBody()->GetId();

    float collisionRadius = GetCollisionRadius();

    for (int bounce = 0; bounce < m_MaxBounces && remainingMovement.LengthSquared() > 0.0001f; ++bounce) {
        float movementLength = remainingMovement.Length();
        Vec3 direction = remainingMovement / movementLength;

        Vec3 horizontalMovement(remainingMovement.x, 0.0f, remainingMovement.z);
        if (horizontalMovement.LengthSquared() > 0.0001f && bounce == 0) {
            Vec3 stepMovement;
            if (TryStepUp(remainingMovement, stepMovement)) {
                return stepMovement;
            }
        }

        ShapeCastHit3D hit;
        bool hasCollision = physicsWorld->SphereCast(currentPos + totalMovement,
                                                      currentPos + totalMovement + remainingMovement,
                                                      collisionRadius, hit, &ignoreBodyId);

        if (hasCollision) {
            if (hit.fraction > 0.0f) {

                Vec3 safeMovement = direction * (movementLength * hit.fraction * 0.99f);
                totalMovement += safeMovement;
            }

            Vec3 slideVelocity = SlideAlongSurface(remainingMovement, hit.normal);
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

    return totalMovement;
}

Vec3 CharacterController3D::SlideAlongSurface(const Vec3& velocity, const Vec3& normal) {

    float dot = velocity.Dot(normal);
    return velocity - (normal * dot);
}

bool CharacterController3D::TryStepUp(const Vec3& movement, Vec3& outMovement) {
    if (!m_KinematicBody || !m_KinematicBody->GetPhysicsBody()) return false;

    auto* sceneManager = SceneManager::GetInstance();
    if (!sceneManager) return false;

    auto* physicsWorld = sceneManager->GetPhysics3DWorld();
    if (!physicsWorld) return false;

    Vec3 currentPos = m_KinematicBody->GetPhysicsBody()->GetPosition();

    const UUID& ignoreBodyId = m_KinematicBody->GetPhysicsBody()->GetId();

    float collisionRadius = GetCollisionRadius();

    Vec3 upMovement(0.0f, m_StepHeight, 0.0f);
    ShapeCastHit3D upHit;
    bool canMoveUp = !physicsWorld->SphereCast(currentPos, currentPos + upMovement, collisionRadius, upHit, &ignoreBodyId);

    if (!canMoveUp) return false;

    Vec3 elevatedPos = currentPos + upMovement;
    Vec3 horizontalMovement(movement.x, 0.0f, movement.z);
    ShapeCastHit3D forwardHit;
    bool canMoveForward = !physicsWorld->SphereCast(elevatedPos, elevatedPos + horizontalMovement, collisionRadius, forwardHit, &ignoreBodyId);

    if (!canMoveForward) return false;

    Vec3 forwardPos = elevatedPos + horizontalMovement;
    Vec3 downMovement(0.0f, -m_StepHeight * 1.5f, 0.0f);
    ShapeCastHit3D downHit;
    bool foundGround = physicsWorld->SphereCast(forwardPos, forwardPos + downMovement, collisionRadius, downHit, &ignoreBodyId);

    if (foundGround) {

        outMovement = upMovement + horizontalMovement + (downMovement * downHit.fraction);
        return true;
    }

    return false;
}

bool CharacterController3D::IsFloor(const Vec3& normal) const {

    float angle = std::acos(normal.Dot(Vec3(0.0f, 1.0f, 0.0f))) * (180.0f / 3.14159265f);
    return angle <= m_MaxSlopeAngle;
}

bool CharacterController3D::IsWall(const Vec3& normal) const {

    return !IsFloor(normal) && !IsCeiling(normal);
}

bool CharacterController3D::IsCeiling(const Vec3& normal) const {

    float angle = std::acos(normal.Dot(Vec3(0.0f, -1.0f, 0.0f))) * (180.0f / 3.14159265f);
    return angle <= m_MaxSlopeAngle;
}

void CharacterController3D::FindKinematicBody() {
    if (!m_Owner) return;

    auto kinematicBody = m_Owner->GetComponent<KinematicBody3DComponent>();
    if (kinematicBody) {
        m_KinematicBody = kinematicBody.get();
        return;
    }

    for (auto& child : m_Owner->GetChildren()) {
        kinematicBody = child->GetComponent<KinematicBody3DComponent>();
        if (kinematicBody) {
            m_KinematicBody = kinematicBody.get();
            return;
        }

        for (auto& grandchild : child->GetChildren()) {
            kinematicBody = grandchild->GetComponent<KinematicBody3DComponent>();
            if (kinematicBody) {
                m_KinematicBody = kinematicBody.get();
                return;
            }
        }
    }

}

void CharacterController3D::FindCollisionShape() {
    if (!m_Owner) {
        m_CollisionShape = nullptr;
        return;
    }

    auto collisionShape = m_Owner->GetComponent<CollisionMesh3DComponent>();
    if (collisionShape) {

        m_CollisionShape = collisionShape.get();
        return;
    }

    for (auto& child : m_Owner->GetChildren()) {

        collisionShape = child->GetComponent<CollisionMesh3DComponent>();
        if (collisionShape) {

            m_CollisionShape = collisionShape.get();
            return;
        }

        for (auto& grandchild : child->GetChildren()) {

            collisionShape = grandchild->GetComponent<CollisionMesh3DComponent>();
            if (collisionShape) {

                m_CollisionShape = collisionShape.get();
                return;
            }
        }
    }

    m_CollisionShape = nullptr;
}

float CharacterController3D::GetCollisionRadius() const {
    if (!m_CollisionShape) {
        return 0.5f;
    }

    CollisionShape3DType shapeType = m_CollisionShape->GetShapeType();

    switch (shapeType) {
        case CollisionShape3DType::Sphere:
            return m_CollisionShape->GetRadius();

        case CollisionShape3DType::Capsule:
            return m_CollisionShape->GetRadius();

        case CollisionShape3DType::Cylinder:
            return m_CollisionShape->GetRadius();

        case CollisionShape3DType::Cone:
            return m_CollisionShape->GetRadius();

        case CollisionShape3DType::Box: {

            Vec3 size = m_CollisionShape->GetSize();
            return std::min({size.x, size.y, size.z}) / 2.0f;
        }

        case CollisionShape3DType::Plane:

            return 0.5f;

        case CollisionShape3DType::Mesh:

            return 0.5f;

        default:
            return 0.5f;
    }
}

void CharacterController3D::ClearKinematicBodyReference() {
    m_KinematicBody = nullptr;
}

}
}

