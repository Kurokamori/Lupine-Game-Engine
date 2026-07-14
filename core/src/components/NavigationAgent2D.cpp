#include "lupine/components/NavigationAgent2D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/navigation/NavigationServer2D.hpp"

#include <algorithm>
#include <cmath>

namespace lupine {
namespace components {

using namespace core;
using math::Vec2;

namespace {

constexpr float kRecomputeTargetEps = 1.0f;
constexpr float kStrayDistance = 64.0f;

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

} // namespace

NavigationAgent2D::NavigationAgent2D()
    : Component("NavigationAgent2D") {}

NavigationAgent2D::NavigationAgent2D(const std::string& name)
    : Component(name) {}

NavigationAgent2D::~NavigationAgent2D() {
    UnregisterAvoidance();
}

void NavigationAgent2D::DefineProperties() {
    DefineProperty(PROPERTY_DEFAULT_GROUP(targetPosition, Vec2, math::Vec2(0.0f, 0.0f), "Navigation"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(maxSpeed, 200.0f, 0.0f, 10000.0f, 1.0f, "Navigation"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(pathDesiredDistance, 16.0f, 0.1f, 1000.0f, 0.5f, "Navigation"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(targetDesiredDistance, 16.0f, 0.1f, 1000.0f, 0.5f, "Navigation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(autoMove, Bool, false, "Navigation"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(radius, 16.0f, 0.0f, 1000.0f, 0.5f, "Avoidance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(avoidanceEnabled, Bool, false, "Avoidance"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(neighborDistance, 200.0f, 0.0f, 10000.0f, 1.0f, "Avoidance"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(maxNeighbors, 10, 0, 100, 1, "Avoidance"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(timeHorizon, 1.0f, 0.0f, 100.0f, 0.1f, "Avoidance"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(timeHorizonObstacle, 0.5f, 0.0f, 100.0f, 0.1f, "Avoidance"));
}

void NavigationAgent2D::DefineSignals() {
    RegisterSignal({"path_changed", {}, "Emitted when a new path is computed."});
    RegisterSignal({"waypoint_reached", {}, "Emitted when the agent reaches an intermediate path point."});
    RegisterSignal({"target_reached", {}, "Emitted once when the agent reaches the target."});
    RegisterSignal({"navigation_finished", {}, "Emitted once when navigation completes (reached or unreachable)."});
    RegisterSignal({"velocity_computed", {}, "Emitted each update with the avoidance-corrected velocity."});
}

float NavigationAgent2D::GetMaxSpeed() const { return GetPropertyValue<float>("maxSpeed"); }
void NavigationAgent2D::SetMaxSpeed(float speed) { SetPropertyValue<float>("maxSpeed", speed); }
float NavigationAgent2D::GetPathDesiredDistance() const { return GetPropertyValue<float>("pathDesiredDistance"); }
float NavigationAgent2D::GetTargetDesiredDistance() const { return GetPropertyValue<float>("targetDesiredDistance"); }
bool NavigationAgent2D::GetAutoMove() const { return GetPropertyValue<bool>("autoMove"); }
void NavigationAgent2D::SetAutoMove(bool autoMove) { SetPropertyValue<bool>("autoMove", autoMove); }

float NavigationAgent2D::GetRadius() const { return GetPropertyValue<float>("radius"); }
void NavigationAgent2D::SetRadius(float radius) { SetPropertyValue<float>("radius", radius); }
bool NavigationAgent2D::GetAvoidanceEnabled() const { return GetPropertyValue<bool>("avoidanceEnabled"); }
void NavigationAgent2D::SetAvoidanceEnabled(bool enabled) { SetPropertyValue<bool>("avoidanceEnabled", enabled); }
float NavigationAgent2D::GetNeighborDistance() const { return GetPropertyValue<float>("neighborDistance"); }
int NavigationAgent2D::GetMaxNeighbors() const { return GetPropertyValue<int>("maxNeighbors"); }
float NavigationAgent2D::GetTimeHorizon() const { return GetPropertyValue<float>("timeHorizon"); }
float NavigationAgent2D::GetTimeHorizonObstacle() const { return GetPropertyValue<float>("timeHorizonObstacle"); }

std::string NavigationAgent2D::AvoidanceId() const {
    return GetUUID().ToString();
}

void NavigationAgent2D::UnregisterAvoidance() {
    if (m_AvoidanceRegistered) {
        navigation::NavigationServer2D::GetInstance().RemoveAvoidanceAgent(AvoidanceId());
        m_AvoidanceRegistered = false;
    }
}

void NavigationAgent2D::OnDestroy() {
    UnregisterAvoidance();
}

void NavigationAgent2D::OnExitTree() {
    UnregisterAvoidance();
}

void NavigationAgent2D::SetTargetPosition(const Vec2& target) {
    SetPropertyValue<Vec2>("targetPosition", target);
    m_HasTarget = true;
    m_PathValid = false;
    m_Finished = false;
    m_FinishEmitted = false;
}

Vec2 NavigationAgent2D::GetTargetPosition() const {
    return GetPropertyValue<Vec2>("targetPosition");
}

void NavigationAgent2D::ResetPath() {
    m_Path.clear();
    m_PathIndex = 0;
    m_PathValid = false;
    m_Finished = false;
    m_FinishEmitted = false;
}

float NavigationAgent2D::DistanceToTarget() const {
    Node2D* owner = dynamic_cast<Node2D*>(GetOwner());
    if (!owner) {
        return 0.0f;
    }
    return (owner->GetGlobalPosition() - GetTargetPosition()).Length();
}

Vec2 NavigationAgent2D::GetNextPathPosition() const {
    Node2D* owner = dynamic_cast<Node2D*>(GetOwner());
    Vec2 ownerPos = owner ? owner->GetGlobalPosition() : Vec2::Zero();
    if (!m_PathValid || m_Path.empty()) {
        return ownerPos;
    }
    size_t idx = std::min(m_PathIndex, m_Path.size() - 1);
    return m_Path[idx];
}

void NavigationAgent2D::RecomputePath(const Vec2& from, const Vec2& to) {
    navigation::NavigationServer2D& server = navigation::NavigationServer2D::GetInstance();
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

Vec2 NavigationAgent2D::ComputeDesiredVelocity(const Vec2& currentPos) const {
    Vec2 toNext = GetNextPathPosition() - currentPos;
    float dist = toNext.Length();
    if (dist < 1e-4f) {
        return Vec2::Zero();
    }
    return toNext / dist * GetMaxSpeed();
}

void NavigationAgent2D::ApplyMovement(float deltaTime, const Vec2& currentPos, const Vec2& velocity) {
    Node2D* owner = dynamic_cast<Node2D*>(GetOwner());
    if (!owner) {
        return;
    }
    Vec2 worldStep = velocity * deltaTime;

    // Do not overshoot the immediate waypoint.
    Vec2 toNext = GetNextPathPosition() - currentPos;
    float toNextLen = toNext.Length();
    if (toNextLen > 1e-4f && worldStep.Length() > toNextLen) {
        worldStep = toNext;
    }

    Vec2 localStep = worldStep;
    Node2D* parent = dynamic_cast<Node2D*>(owner->GetParent());
    if (parent) {
        float prot = parent->GetGlobalRotation();
        Vec2 pscale = parent->GetGlobalScale();
        float cs = std::cos(-prot);
        float sn = std::sin(-prot);
        Vec2 rotated(worldStep.x * cs - worldStep.y * sn,
                     worldStep.x * sn + worldStep.y * cs);
        localStep = Vec2(std::fabs(pscale.x) > 1e-5f ? rotated.x / pscale.x : rotated.x,
                         std::fabs(pscale.y) > 1e-5f ? rotated.y / pscale.y : rotated.y);
    }
    owner->SetPosition(owner->GetPosition() + localStep);
}

void NavigationAgent2D::OnUpdate(float deltaTime) {
    Node2D* owner = dynamic_cast<Node2D*>(GetOwner());
    if (!owner) {
        return;
    }

    Vec2 pos = owner->GetGlobalPosition();
    navigation::NavigationServer2D& server = navigation::NavigationServer2D::GetInstance();
    Vec2 prefVelocity = Vec2::Zero();

    if (m_HasTarget) {
        Vec2 target = GetTargetPosition();

        bool needRecompute = !m_PathValid ||
            server.GetVersion() != m_LastMapVersion ||
            (target - m_LastQueryTarget).Length() > kRecomputeTargetEps ||
            (pos - m_LastQueryStart).Length() > kStrayDistance;
        if (needRecompute) {
            RecomputePath(pos, target);
        }

        float targetDist = (pos - target).Length();
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
                   (pos - m_Path[m_PathIndex]).Length() <= pathDesired) {
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
        m_Velocity = server.ComputeAvoidanceVelocity(AvoidanceId(), step,
            GetNeighborDistance(), GetMaxNeighbors(), GetTimeHorizon(), GetTimeHorizonObstacle());
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

nlohmann::json NavigationAgent2D::CallMethod(const std::string& method,
                                             const nlohmann::json& args) {
    if (method == "set_target_position") {
        SetTargetPosition(Vec2(ArgFloat(args, 0), ArgFloat(args, 1)));
    } else if (method == "get_target_position") {
        Vec2 t = GetTargetPosition();
        return nlohmann::json{{"x", t.x}, {"y", t.y}};
    } else if (method == "get_next_path_position") {
        Vec2 p = GetNextPathPosition();
        return nlohmann::json{{"x", p.x}, {"y", p.y}};
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
        for (const Vec2& p : m_Path) {
            arr.push_back({{"x", p.x}, {"y", p.y}});
        }
        return arr;
    } else if (method == "set_max_speed") {
        SetMaxSpeed(ArgFloat(args, 0, 200.0f));
    } else if (method == "get_max_speed") {
        return GetMaxSpeed();
    } else if (method == "set_auto_move") {
        SetAutoMove(ArgBool(args, 0, false));
    } else if (method == "set_radius") {
        SetRadius(ArgFloat(args, 0, 16.0f));
    } else if (method == "get_radius") {
        return GetRadius();
    } else if (method == "set_avoidance_enabled") {
        SetAvoidanceEnabled(ArgBool(args, 0, false));
    } else if (method == "is_avoidance_enabled") {
        return GetAvoidanceEnabled();
    } else if (method == "get_velocity") {
        return nlohmann::json{{"x", m_Velocity.x}, {"y", m_Velocity.y}};
    } else if (method == "reset") {
        ResetPath();
    }
    return nlohmann::json();
}

} // namespace components
} // namespace lupine
