#include "lupine/components/CharacterController2D.hpp"
#include "lupine/components/CollisionBody2DComponent.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/SceneManager.hpp"
#include "lupine/physics2d/Physics2DWorld.hpp"
#include "lupine/logger/Logger.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

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

    // Wall/ceiling are transient contact flags recomputed every step; ground is
    // resolved authoritatively below by the swept move and the ground probe.
    m_IsOnWall = false;
    m_IsOnCeiling = false;

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
    const core::UUID& ignoreBodyId = m_KinematicBody->GetPhysicsBody()->GetId();
    uint64_t mask = GetCollisionMaskBits();

    std::vector<Vec2> castPoints;
    float castRadius = 0.0f;
    bool isCircle = false;
    BuildColliderProxy(currentPos, castPoints, castRadius, isCircle);

    ShapeCastHit2D hit;
    bool hasCollision = physicsWorld->ShapeCastConvex(castPoints, castRadius, movement, hit, &ignoreBodyId, mask);

    if (hasCollision) {
        const float kSkin = 1.0f;
        float travel = movementLength * hit.fraction - kSkin;
        if (travel < 0.0f) {
            travel = 0.0f;
        }
        outActualMovement = direction * travel;

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

    auto* physicsBody = m_KinematicBody->GetPhysicsBody();
    Vec2 currentPos = physicsBody->GetPosition();
    Vec2 downDirection(0.0f, -1.0f);

    float halfWidth = 0.0f;
    float halfHeight = 0.0f;
    GetColliderHalfExtents(halfWidth, halfHeight);

    float raycastDistance = halfHeight + m_GroundDetectionDistance;

    const UUID& ignoreBodyId = physicsBody->GetId();
    uint64_t mask = GetCollisionMaskBits();

    RaycastHit2D hit;
    bool hasGround = physicsWorld->Raycast(currentPos, downDirection, raycastDistance, hit, &ignoreBodyId, mask);

    if (hasGround && IsFloor(hit.normal)) {

        float characterBottom = currentPos.y - halfHeight;
        float distanceToGround = characterBottom - hit.point.y;

        const float kPenetrationTolerance = 2.0f;
        if (distanceToGround <= m_GroundDetectionDistance && distanceToGround >= -kPenetrationTolerance) {
            m_IsOnGround = true;
            m_GroundNormal = hit.normal;

            if (m_Velocity.y < 0.0f) {
                m_Velocity.y = 0.0f;
            }

            // Snap the body so its base rests exactly on the ground, eliminating
            // the sub-pixel hover/jitter that the swept skin margin would leave.
            if (m_SnapToGround && distanceToGround > 0.0f && m_Velocity.y <= 0.0f) {
                Vec2 snappedPos = currentPos;
                snappedPos.y -= distanceToGround;
                physicsBody->SetPosition(snappedPos);
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

    float halfWidth = 0.0f;
    float halfHeight = 0.0f;
    GetColliderHalfExtents(halfWidth, halfHeight);
    float probeDistance = halfWidth + m_WallDetectionDistance;

    const UUID& ignoreBodyId = m_KinematicBody->GetPhysicsBody()->GetId();
    uint64_t mask = GetCollisionMaskBits();

    RaycastHit2D leftHit, rightHit;
    bool hasLeftWall = physicsWorld->Raycast(currentPos, leftDirection, probeDistance, leftHit, &ignoreBodyId, mask);
    bool hasRightWall = physicsWorld->Raycast(currentPos, rightDirection, probeDistance, rightHit, &ignoreBodyId, mask);

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
    (void)deltaTime;

    if (!m_KinematicBody || !m_KinematicBody->GetPhysicsBody()) return Vec2::Zero();

    auto* sceneManager = SceneManager::GetInstance();
    if (!sceneManager) return movement;

    auto* physicsWorld = sceneManager->GetPhysics2DWorld();
    if (!physicsWorld) return movement;

    const UUID& ignoreBodyId = m_KinematicBody->GetPhysicsBody()->GetId();
    uint64_t mask = GetCollisionMaskBits();

    const float kSkin = 1.0f;          // Contact margin kept in world units.
    const float kMinMove = 0.0001f;

    Vec2 startPosition = m_KinematicBody->GetPhysicsBody()->GetPosition();
    Vec2 position = startPosition;
    Vec2 remaining = movement;

    // 1) Push the body out of anything it is already overlapping before sweeping.
    //    This prevents the swept cast from starting inside a collider (which
    //    yields a zero contact fraction and a degenerate normal -> sticking).
    {
        std::vector<Vec2> depenPoints;
        float proxyRadius = 0.0f;
        bool isCircle = false;
        BuildColliderProxy(position, depenPoints, proxyRadius, isCircle);

        if (isCircle && !depenPoints.empty()) {
            // Depenetration works on polygons; approximate the circle with an octagon.
            Vec2 center = depenPoints[0];
            depenPoints.clear();
            const int kSegments = 8;
            for (int i = 0; i < kSegments; ++i) {
                float angle = static_cast<float>(i) * (2.0f * 3.14159265f / static_cast<float>(kSegments));
                depenPoints.push_back(center + Vec2(std::cos(angle) * proxyRadius, std::sin(angle) * proxyRadius));
            }
            proxyRadius = 0.0f;
        }

        Vec2 correction;
        if (physicsWorld->ComputeConvexDepenetration(depenPoints, proxyRadius, &ignoreBodyId, mask, correction)) {
            position = position + correction;
        }
    }

    // 2) Swept move with sliding. Each bounce advances up to the first contact
    //    (minus a skin margin) and slides the leftover movement along the surface.
    for (int bounce = 0; bounce < m_MaxBounces; ++bounce) {
        float remainingLength = remaining.Length();
        if (remainingLength < kMinMove) {
            break;
        }

        Vec2 direction = remaining / remainingLength;

        std::vector<Vec2> castPoints;
        float castRadius = 0.0f;
        bool isCircle = false;
        BuildColliderProxy(position, castPoints, castRadius, isCircle);

        ShapeCastHit2D hit;
        bool hasCollision = physicsWorld->ShapeCastConvex(castPoints, castRadius, remaining, hit, &ignoreBodyId, mask);

        if (!hasCollision) {
            position = position + remaining;
            remaining = Vec2::Zero();
            break;
        }

        // Advance to just short of the contact.
        float travel = remainingLength * hit.fraction - kSkin;
        if (travel < 0.0f) {
            travel = 0.0f;
        }
        position = position + direction * travel;

        float normalLength = hit.normal.Length();
        if (normalLength < 0.001f) {
            // Degenerate contact: the pre-pass depenetration normally prevents
            // this, so just stop advancing this step rather than guessing.
            break;
        }
        Vec2 normal = hit.normal / normalLength;

        // Slide whatever movement is left along the contact surface.
        Vec2 leftover = direction * (remainingLength - travel);
        remaining = SlideAlongSurface(leftover, normal);

        if (IsFloor(normal)) {
            m_IsOnGround = true;
            m_GroundNormal = normal;
            if (m_Velocity.y < 0.0f) {
                m_Velocity.y = 0.0f;
            }
        } else if (IsCeiling(normal)) {
            m_IsOnCeiling = true;
            if (m_Velocity.y > 0.0f) {
                m_Velocity.y = 0.0f;
            }
        } else if (IsWall(normal)) {
            m_IsOnWall = true;
            m_WallNormal = normal;

            // Cancel the velocity pushing into the wall so it does not build up
            // and pin the body against the surface across frames.
            float intoWall = m_Velocity.Dot(normal);
            if (intoWall < 0.0f) {
                m_Velocity = m_Velocity - normal * intoWall;
            }
        }
    }

    return position - startPosition;
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

// The swept proxy must describe the same geometry Collider2D bakes into Box2D, which applies
// the owner's global scale at shape-creation time. Radii use the largest absolute axis (Box2D
// has no ellipse), matching Collider2D::RadialScale.
static Vec2 OwnerGlobalScale(const core::Node* owner) {
    const core::Node2D* node2D = dynamic_cast<const core::Node2D*>(owner);
    if (!node2D) {
        return Vec2(1.0f, 1.0f);
    }
    return node2D->GetGlobalScale();
}

static float RadialScaleOf(const Vec2& scale) {
    return std::max(std::abs(scale.x), std::abs(scale.y));
}

void CharacterController2D::BuildColliderProxy(const Vec2& bodyCenter, std::vector<Vec2>& outPoints,
                                               float& outRadius, bool& outIsCircle) const {
    outPoints.clear();
    outRadius = 0.0f;
    outIsCircle = false;

    if (!m_CollisionShape) {
        // No collider configured: fall back to a default box around the body.
        const float kHalf = 50.0f;
        outPoints.push_back(bodyCenter + Vec2(-kHalf, -kHalf));
        outPoints.push_back(bodyCenter + Vec2(kHalf, -kHalf));
        outPoints.push_back(bodyCenter + Vec2(kHalf, kHalf));
        outPoints.push_back(bodyCenter + Vec2(-kHalf, kHalf));
        return;
    }

    const Vec2 scale = OwnerGlobalScale(m_Owner);
    const Vec2 offset = m_CollisionShape->GetOffset();
    Vec2 center = bodyCenter + Vec2(offset.x * scale.x, offset.y * scale.y);
    CollisionShape2DType shapeType = m_CollisionShape->GetShapeType();

    switch (shapeType) {
        case CollisionShape2DType::Circle: {
            outIsCircle = true;
            outRadius = m_CollisionShape->GetRadius() * RadialScaleOf(scale);
            outPoints.push_back(center);
            break;
        }

        case CollisionShape2DType::Rectangle: {
            Vec2 size = m_CollisionShape->GetSize();
            float halfW = std::abs(size.x * scale.x) * 0.5f;
            float halfH = std::abs(size.y * scale.y) * 0.5f;
            outPoints.push_back(center + Vec2(-halfW, -halfH));
            outPoints.push_back(center + Vec2(halfW, -halfH));
            outPoints.push_back(center + Vec2(halfW, halfH));
            outPoints.push_back(center + Vec2(-halfW, halfH));
            break;
        }

        default: {
            const std::vector<Vec2>& verts = m_CollisionShape->GetVertices();
            if (verts.size() >= 3) {
                for (const Vec2& v : verts) {
                    outPoints.push_back(center + Vec2(v.x * scale.x, v.y * scale.y));
                }
            } else {
                // Degenerate polygon: treat as a circle so the body still collides.
                float radius = m_CollisionShape->GetRadius();
                if (radius <= 0.0f) {
                    radius = 50.0f;
                }
                outIsCircle = true;
                outRadius = radius * RadialScaleOf(scale);
                outPoints.push_back(center);
            }
            break;
        }
    }
}

void CharacterController2D::GetColliderHalfExtents(float& outHalfWidth, float& outHalfHeight) const {
    outHalfWidth = 50.0f;
    outHalfHeight = 50.0f;

    if (!m_CollisionShape) {
        return;
    }

    const Vec2 scale = OwnerGlobalScale(m_Owner);
    CollisionShape2DType shapeType = m_CollisionShape->GetShapeType();

    switch (shapeType) {
        case CollisionShape2DType::Circle: {
            float radius = m_CollisionShape->GetRadius() * RadialScaleOf(scale);
            outHalfWidth = radius;
            outHalfHeight = radius;
            break;
        }

        case CollisionShape2DType::Rectangle: {
            Vec2 size = m_CollisionShape->GetSize();
            outHalfWidth = std::abs(size.x * scale.x) * 0.5f;
            outHalfHeight = std::abs(size.y * scale.y) * 0.5f;
            break;
        }

        default: {
            const std::vector<Vec2>& verts = m_CollisionShape->GetVertices();
            if (verts.size() >= 2) {
                float minX = verts[0].x, maxX = verts[0].x;
                float minY = verts[0].y, maxY = verts[0].y;
                for (const Vec2& v : verts) {
                    minX = std::min(minX, v.x);
                    maxX = std::max(maxX, v.x);
                    minY = std::min(minY, v.y);
                    maxY = std::max(maxY, v.y);
                }
                outHalfWidth = std::abs((maxX - minX) * scale.x) * 0.5f;
                outHalfHeight = std::abs((maxY - minY) * scale.y) * 0.5f;
            } else {
                float radius = m_CollisionShape->GetRadius() * RadialScaleOf(scale);
                if (radius > 0.0f) {
                    outHalfWidth = radius;
                    outHalfHeight = radius;
                }
            }
            break;
        }
    }
}

uint64_t CharacterController2D::GetCollisionMaskBits() const {
    if (!m_CollisionShape) {
        return 0xFFFFFFFFFFFFFFFFull;
    }
    return static_cast<uint64_t>(m_CollisionShape->GetCollisionMask());
}

void CharacterController2D::ClearKinematicBodyReference() {
    m_KinematicBody = nullptr;
}

}
}

