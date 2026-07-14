#include "lupine/components/NavigationAgent3D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/navigation/NavigationServer3D.hpp"

#include <algorithm>
#include <cmath>

namespace lupine {
namespace components {

using namespace core;
using math::Vec3;

namespace {

constexpr float kRecomputeTargetEps = 0.1f;
constexpr float kStrayDistance = 4.0f;

float ArgFloat(const nlohmann::json& args, size_t i, float def = 0.0f) {
    if (args.is_array() && i < args.size() && args[i].is_number()) {
        return args[i].get<float>();
    }
    return def;
}

bool ArgBool(const nlohmann::json& args, size_t i, bool def = false) {
    if (args.is_array() && i < args.size() && args[i].is_boolean()) {
        return args[i].get<bool>();
    }
    return def;
}

// Horizontal (XZ) distance: navigation adjacency and waypoint proximity are
// resolved in the ground plane, independent of small elevation differences.
float HorizDist(const Vec3& a, const Vec3& b) {
    float dx = a.x - b.x;
    float dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

} // namespace

NavigationAgent3D::NavigationAgent3D()
    : Component("NavigationAgent3D") {}

NavigationAgent3D::NavigationAgent3D(const std::string& name)
    : Component(name) {}

NavigationAgent3D::~NavigationAgent3D() {
    UnregisterAvoidance();
}

void NavigationAgent3D::DefineProperties() {
    DefineProperty(PROPERTY_DEFAULT_GROUP(targetPosition, Vec3, math::Vec3(0.0f, 0.0f, 0.0f), "Navigation"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(maxSpeed, 3.5f, 0.0f, 1000.0f, 0.1f, "Navigation"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(pathDesiredDistance, 0.5f, 0.01f, 100.0f, 0.05f, "Navigation"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(targetDesiredDistance, 0.5f, 0.01f, 100.0f, 0.05f, "Navigation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(autoMove, Bool, false, "Navigation"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(radius, 0.5f, 0.0f, 100.0f, 0.05f, "Avoidance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(avoidanceEnabled, Bool, false, "Avoidance"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(neighborDistance, 5.0f, 0.0f, 1000.0f, 0.1f, "Avoidance"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(maxNeighbors, 10, 0, 100, 1, "Avoidance"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(timeHorizon, 1.0f, 0.0f, 100.0f, 0.1f, "Avoidance"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(timeHorizonObstacle, 0.5f, 0.0f, 100.0f, 0.1f, "Avoidance"));
}

void NavigationAgent3D::DefineSignals() {
    RegisterSignal({"path_changed", {}, "Emitted when a new path is computed."});
    RegisterSignal({"waypoint_reached", {}, "Emitted when the agent reaches an intermediate path point."});
    RegisterSignal({"target_reached", {}, "Emitted once when the agent reaches the target."});
    RegisterSignal({"navigation_finished", {}, "Emitted once when navigation completes (reached or unreachable)."});
    RegisterSignal({"velocity_computed", {}, "Emitted each update with the avoidance-corrected velocity."});
}

float NavigationAgent3D::GetMaxSpeed() const { return GetPropertyValue<float>("maxSpeed"); }
void NavigationAgent3D::SetMaxSpeed(float speed) { SetPropertyValue<float>("maxSpeed", speed); }
float NavigationAgent3D::GetPathDesiredDistance() const { return GetPropertyValue<float>("pathDesiredDistance"); }
float NavigationAgent3D::GetTargetDesiredDistance() const { return GetPropertyValue<float>("targetDesiredDistance"); }
bool NavigationAgent3D::GetAutoMove() const { return GetPropertyValue<bool>("autoMove"); }
void NavigationAgent3D::SetAutoMove(bool autoMove) { SetPropertyValue<bool>("autoMove", autoMove); }

float NavigationAgent3D::GetRadius() const { return GetPropertyValue<float>("radius"); }
void NavigationAgent3D::SetRadius(float radius) { SetPropertyValue<float>("radius", radius); }
bool NavigationAgent3D::GetAvoidanceEnabled() const { return GetPropertyValue<bool>("avoidanceEnabled"); }
void NavigationAgent3D::SetAvoidanceEnabled(bool enabled) { SetPropertyValue<bool>("avoidanceEnabled", enabled); }
float NavigationAgent3D::GetNeighborDistance() const { return GetPropertyValue<float>("neighborDistance"); }
int NavigationAgent3D::GetMaxNeighbors() const { return GetPropertyValue<int>("maxNeighbors"); }
float NavigationAgent3D::GetTimeHorizon() const { return GetPropertyValue<float>("timeHorizon"); }
float NavigationAgent3D::GetTimeHorizonObstacle() const { return GetPropertyValue<float>("timeHorizonObstacle"); }

std::string NavigationAgent3D::AvoidanceId() const {
    return GetUUID().ToString();
}

void NavigationAgent3D::UnregisterAvoidance() {
    if (m_AvoidanceRegistered) {
        navigation::NavigationServer3D::GetInstance().RemoveAvoidanceAgent(AvoidanceId());
        m_AvoidanceRegistered = false;
    }
}

void NavigationAgent3D::OnDestroy() {
    UnregisterAvoidance();
}

void NavigationAgent3D::OnExitTree() {
    UnregisterAvoidance();
}

void NavigationAgent3D::SetTargetPosition(const Vec3& target) {
    SetPropertyValue<Vec3>("targetPosition", target);
    m_HasTarget = true;
    m_PathValid = false;
    m_Finished = false;
    m_FinishEmitted = false;
}

Vec3 NavigationAgent3D::GetTargetPosition() const {
    return GetPropertyValue<Vec3>("targetPosition");
}

void NavigationAgent3D::ResetPath() {
    m_Path.clear();
    m_PathIndex = 0;
    m_PathValid = false;
    m_Finished = false;
    m_FinishEmitted = false;
}

float NavigationAgent3D::DistanceToTarget() const {
    Node3D* owner = dynamic_cast<Node3D*>(GetOwner());
    if (!owner) {
        return 0.0f;
    }
    return (owner->GetGlobalPosition() - GetTargetPosition()).Length();
}

Vec3 NavigationAgent3D::GetNextPathPosition() const {
    Node3D* owner = dynamic_cast<Node3D*>(GetOwner());
    Vec3 ownerPos = owner ? owner->GetGlobalPosition() : Vec3::Zero();
    if (!m_PathValid || m_Path.empty()) {
        return ownerPos;
    }
    size_t idx = std::min(m_PathIndex, m_Path.size() - 1);
    return m_Path[idx];
}

void NavigationAgent3D::RecomputePath(const Vec3& from, const Vec3& to) {
    navigation::NavigationServer3D& server = navigation::NavigationServer3D::GetInstance();
    m_Path.clear();
    m_PathValid = server.QueryPath(from, to, m_Path, GetRadius());
    m_PathIndex = 0;
    m_LastMapVersion = server.GetVersion();
    m_LastQueryTarget = to;
    m_LastQueryStart = from;
    if (m_PathValid) {
        Emit("path_changed");
    }
}

Vec3 NavigationAgent3D::ComputeDesiredVelocity(const Vec3& currentPos) const {
    Vec3 toNext = GetNextPathPosition() - currentPos;
    float dist = toNext.Length();
    if (dist < 1e-4f) {
        return Vec3::Zero();
    }
    return toNext / dist * GetMaxSpeed();
}

void NavigationAgent3D::ApplyMovement(float deltaTime, const Vec3& currentPos, const Vec3& velocity) {
    Node3D* owner = dynamic_cast<Node3D*>(GetOwner());
    if (!owner) {
        return;
    }
    Vec3 worldStep = velocity * deltaTime;

    Vec3 toNext = GetNextPathPosition() - currentPos;
    float toNextLen = toNext.Length();
    if (toNextLen > 1e-4f && worldStep.Length() > toNextLen) {
        worldStep = toNext;
    }

    Vec3 newWorld = currentPos + worldStep;
    Node3D* parent = dynamic_cast<Node3D*>(owner->GetParent());
    if (parent) {
        math::Mat4 pinv = parent->GetGlobalTransformMatrix().Inverse();
        owner->SetPosition(pinv.TransformPoint(newWorld));
    } else {
        owner->SetPosition(newWorld);
    }
}

void NavigationAgent3D::OnUpdate(float deltaTime) {
    Node3D* owner = dynamic_cast<Node3D*>(GetOwner());
    if (!owner) {
        return;
    }

    Vec3 pos = owner->GetGlobalPosition();
    navigation::NavigationServer3D& server = navigation::NavigationServer3D::GetInstance();
    Vec3 prefVelocity = Vec3::Zero();

    if (m_HasTarget) {
        Vec3 target = GetTargetPosition();

        bool needRecompute = !m_PathValid ||
            server.GetVersion() != m_LastMapVersion ||
            (target - m_LastQueryTarget).Length() > kRecomputeTargetEps ||
            HorizDist(pos, m_LastQueryStart) > kStrayDistance;
        if (needRecompute) {
            RecomputePath(pos, target);
        }

        float targetDist = HorizDist(pos, target);
        if (targetDist <= GetTargetDesiredDistance() || !m_PathValid) {
            m_Finished = true;
            if (!m_FinishEmitted) {
                m_FinishEmitted = true;
                if (m_PathValid && targetDist <= GetTargetDesiredDistance()) {
                    Emit("target_reached");
                }
                Emit("navigation_finished");
            }
        } else {
            m_Finished = false;
            float pathDesired = GetPathDesiredDistance();
            while (m_PathIndex < m_Path.size() &&
                   HorizDist(pos, m_Path[m_PathIndex]) <= pathDesired) {
                ++m_PathIndex;
                if (m_PathIndex < m_Path.size()) {
                    Emit("waypoint_reached");
                }
            }
            if (m_PathIndex >= m_Path.size()) {
                m_PathIndex = m_Path.size() - 1;
            }
            prefVelocity = ComputeDesiredVelocity(pos);
        }
    }

    if (GetAvoidanceEnabled()) {
        float step = (deltaTime > 1e-4f) ? deltaTime : 0.016f;
        server.UpdateAvoidanceAgent(AvoidanceId(), pos, GetRadius(), prefVelocity, GetMaxSpeed());
        m_AvoidanceRegistered = true;
        Vec3 avoid = server.ComputeAvoidanceVelocity(AvoidanceId(), step,
            GetNeighborDistance(), GetMaxNeighbors(), GetTimeHorizon(), GetTimeHorizonObstacle());
        // Keep the path's vertical component; avoidance only steers in XZ.
        m_Velocity = Vec3(avoid.x, prefVelocity.y, avoid.z);
        Emit("velocity_computed");
    } else {
        if (m_AvoidanceRegistered) {
            UnregisterAvoidance();
        }
        m_Velocity = prefVelocity;
    }

    if (GetAutoMove() && m_HasTarget && !m_Finished) {
        ApplyMovement(deltaTime, pos, m_Velocity);
    }
}

nlohmann::json NavigationAgent3D::CallMethod(const std::string& method,
                                             const nlohmann::json& args) {
    if (method == "set_target_position") {
        SetTargetPosition(Vec3(ArgFloat(args, 0), ArgFloat(args, 1), ArgFloat(args, 2)));
    } else if (method == "get_target_position") {
        Vec3 t = GetTargetPosition();
        return nlohmann::json{{"x", t.x}, {"y", t.y}, {"z", t.z}};
    } else if (method == "get_next_path_position") {
        Vec3 p = GetNextPathPosition();
        return nlohmann::json{{"x", p.x}, {"y", p.y}, {"z", p.z}};
    } else if (method == "is_navigation_finished") {
        return IsNavigationFinished();
    } else if (method == "is_target_reachable") {
        return IsTargetReachable();
    } else if (method == "distance_to_target") {
        return DistanceToTarget();
    } else if (method == "get_path_count") {
        return static_cast<int>(m_Path.size());
    } else if (method == "get_current_path") {
        nlohmann::json arr = nlohmann::json::array();
        for (const Vec3& p : m_Path) {
            arr.push_back({{"x", p.x}, {"y", p.y}, {"z", p.z}});
        }
        return arr;
    } else if (method == "set_max_speed") {
        SetMaxSpeed(ArgFloat(args, 0, 3.5f));
    } else if (method == "get_max_speed") {
        return GetMaxSpeed();
    } else if (method == "set_auto_move") {
        SetAutoMove(ArgBool(args, 0, false));
    } else if (method == "set_radius") {
        SetRadius(ArgFloat(args, 0, 0.5f));
    } else if (method == "get_radius") {
        return GetRadius();
    } else if (method == "set_avoidance_enabled") {
        SetAvoidanceEnabled(ArgBool(args, 0, false));
    } else if (method == "is_avoidance_enabled") {
        return GetAvoidanceEnabled();
    } else if (method == "get_velocity") {
        return nlohmann::json{{"x", m_Velocity.x}, {"y", m_Velocity.y}, {"z", m_Velocity.z}};
    } else if (method == "reset") {
        ResetPath();
    }
    return nlohmann::json();
}

} // namespace components
} // namespace lupine
