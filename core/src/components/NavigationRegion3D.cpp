#include "lupine/components/NavigationRegion3D.hpp"
#include "lupine/components/CollisionMesh3DComponent.hpp"
#include "lupine/components/StaticMesh3D.hpp"
#include "lupine/components/PrimitiveMesh3D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/navigation/NavigationServer3D.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/asset/ModelAsset.hpp"

#include <cmath>
#include <functional>
#include <limits>

namespace lupine {
namespace components {

using namespace core;
using math::Vec3;
using math::Mat4;
using navigation::NavMeshTriangle;

namespace {

constexpr float kPi = 3.14159265358979323846f;

enum class GeometrySource { Children = 0, EntireScene = 1 };

void PushTri(std::vector<Vec3>& tris, const Vec3& a, const Vec3& b, const Vec3& c) {
    tris.push_back(a);
    tris.push_back(b);
    tris.push_back(c);
}

void PushQuad(std::vector<Vec3>& tris, const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d) {
    PushTri(tris, a, b, c);
    PushTri(tris, a, c, d);
}

// All generators emit local-space triangles centred at the origin. Winding is
// outward so the top surface carries a +Y normal and reads as walkable.

void GenBox(std::vector<Vec3>& tris, const Vec3& he) {
    Vec3 p[8] = {
        {-he.x, -he.y, -he.z}, {he.x, -he.y, -he.z}, {he.x, -he.y, he.z}, {-he.x, -he.y, he.z},
        {-he.x, he.y, -he.z}, {he.x, he.y, -he.z}, {he.x, he.y, he.z}, {-he.x, he.y, he.z},
    };
    PushQuad(tris, p[0], p[1], p[2], p[3]); // bottom (-Y)
    PushQuad(tris, p[4], p[7], p[6], p[5]);  // top (+Y)
    PushQuad(tris, p[3], p[2], p[6], p[7]);  // +Z
    PushQuad(tris, p[1], p[0], p[4], p[5]);  // -Z
    PushQuad(tris, p[2], p[1], p[5], p[6]);  // +X
    PushQuad(tris, p[0], p[3], p[7], p[4]);  // -X
}

void GenSphere(std::vector<Vec3>& tris, float radius, int segs, int rings) {
    segs = std::max(4, segs);
    rings = std::max(2, rings);
    auto vert = [&](int ri, int si) -> Vec3 {
        float v = -kPi * 0.5f + kPi * (static_cast<float>(ri) / rings);
        float u = 2.0f * kPi * (static_cast<float>(si) / segs);
        return Vec3(radius * std::cos(v) * std::cos(u),
                    radius * std::sin(v),
                    radius * std::cos(v) * std::sin(u));
    };
    for (int ri = 0; ri < rings; ++ri) {
        for (int si = 0; si < segs; ++si) {
            Vec3 a = vert(ri, si);
            Vec3 b = vert(ri + 1, si);
            Vec3 c = vert(ri + 1, si + 1);
            Vec3 d = vert(ri, si + 1);
            PushQuad(tris, a, b, c, d);
        }
    }
}

void GenHemisphere(std::vector<Vec3>& tris, float radius, float yCenter, bool top, int segs, int rings) {
    segs = std::max(4, segs);
    rings = std::max(1, rings);
    auto vert = [&](int ri, int si) -> Vec3 {
        float frac = static_cast<float>(ri) / rings; // 0 at equator, 1 at pole
        float v = (top ? 1.0f : -1.0f) * (kPi * 0.5f) * frac;
        float u = 2.0f * kPi * (static_cast<float>(si) / segs);
        return Vec3(radius * std::cos(v) * std::cos(u),
                    yCenter + radius * std::sin(v),
                    radius * std::cos(v) * std::sin(u));
    };
    for (int ri = 0; ri < rings; ++ri) {
        for (int si = 0; si < segs; ++si) {
            Vec3 a = vert(ri, si);
            Vec3 b = vert(ri + 1, si);
            Vec3 c = vert(ri + 1, si + 1);
            Vec3 d = vert(ri, si + 1);
            if (top) {
                PushQuad(tris, a, d, c, b);
            } else {
                PushQuad(tris, a, b, c, d);
            }
        }
    }
}

void GenCylinder(std::vector<Vec3>& tris, float radius, float height, int segs) {
    segs = std::max(4, segs);
    float hy = height * 0.5f;
    Vec3 topC(0.0f, hy, 0.0f);
    Vec3 botC(0.0f, -hy, 0.0f);
    for (int si = 0; si < segs; ++si) {
        float u0 = 2.0f * kPi * (static_cast<float>(si) / segs);
        float u1 = 2.0f * kPi * (static_cast<float>(si + 1) / segs);
        Vec3 t0(radius * std::cos(u0), hy, radius * std::sin(u0));
        Vec3 t1(radius * std::cos(u1), hy, radius * std::sin(u1));
        Vec3 b0(radius * std::cos(u0), -hy, radius * std::sin(u0));
        Vec3 b1(radius * std::cos(u1), -hy, radius * std::sin(u1));
        PushQuad(tris, b0, b1, t1, t0);   // side
        PushTri(tris, topC, t0, t1);      // top cap (+Y)
        PushTri(tris, botC, b1, b0);      // bottom cap (-Y)
    }
}

void GenCone(std::vector<Vec3>& tris, float radius, float height, int segs) {
    segs = std::max(4, segs);
    float hy = height * 0.5f;
    Vec3 apex(0.0f, hy, 0.0f);
    Vec3 botC(0.0f, -hy, 0.0f);
    for (int si = 0; si < segs; ++si) {
        float u0 = 2.0f * kPi * (static_cast<float>(si) / segs);
        float u1 = 2.0f * kPi * (static_cast<float>(si + 1) / segs);
        Vec3 b0(radius * std::cos(u0), -hy, radius * std::sin(u0));
        Vec3 b1(radius * std::cos(u1), -hy, radius * std::sin(u1));
        PushTri(tris, apex, b0, b1);    // side
        PushTri(tris, botC, b1, b0);    // base cap (-Y)
    }
}

void GenCapsule(std::vector<Vec3>& tris, float radius, float totalHeight, int segs, int rings) {
    float cyl = std::max(0.0f, totalHeight - 2.0f * radius);
    if (cyl > 0.0f) {
        GenCylinder(tris, radius, cyl, segs);
    }
    float hy = cyl * 0.5f;
    GenHemisphere(tris, radius, hy, true, segs, rings);
    GenHemisphere(tris, radius, -hy, false, segs, rings);
}

void GenPlane(std::vector<Vec3>& tris, const Vec3& normalIn, float width, float length, float distance) {
    Vec3 n = normalIn;
    if (n.LengthSquared() < 1e-8f) {
        n = Vec3::Up();
    }
    n = n.Normalized();
    Vec3 ref = std::fabs(n.y) < 0.99f ? Vec3::Up() : Vec3::Right();
    Vec3 t = n.Cross(ref).Normalized();
    Vec3 b = n.Cross(t).Normalized();
    Vec3 center = n * distance;
    float hw = (width > 0.0f ? width : 100.0f) * 0.5f;
    float hl = (length > 0.0f ? length : 100.0f) * 0.5f;
    Vec3 c00 = center - t * hw - b * hl;
    Vec3 c10 = center + t * hw - b * hl;
    Vec3 c11 = center + t * hw + b * hl;
    Vec3 c01 = center - t * hw + b * hl;
    PushQuad(tris, c00, c10, c11, c01);
}

void GenPyramid(std::vector<Vec3>& tris, float baseSize, float height) {
    float h = baseSize * 0.5f;
    float hy = height * 0.5f;
    Vec3 a(-h, -hy, -h), bb(h, -hy, -h), c(h, -hy, h), d(-h, -hy, h);
    Vec3 apex(0.0f, hy, 0.0f);
    PushQuad(tris, a, bb, c, d);   // base (-Y)
    PushTri(tris, a, apex, bb);
    PushTri(tris, bb, apex, c);
    PushTri(tris, c, apex, d);
    PushTri(tris, d, apex, a);
}

void GenTorus(std::vector<Vec3>& tris, float majorR, float minorR, int segs, int rings) {
    segs = std::max(6, segs);
    rings = std::max(6, rings);
    auto vert = [&](int i, int j) -> Vec3 {
        float u = 2.0f * kPi * (static_cast<float>(i) / segs);
        float v = 2.0f * kPi * (static_cast<float>(j) / rings);
        float r = majorR + minorR * std::cos(v);
        return Vec3(r * std::cos(u), minorR * std::sin(v), r * std::sin(u));
    };
    for (int i = 0; i < segs; ++i) {
        for (int j = 0; j < rings; ++j) {
            Vec3 a = vert(i, j);
            Vec3 b = vert(i + 1, j);
            Vec3 c = vert(i + 1, j + 1);
            Vec3 d = vert(i, j + 1);
            PushQuad(tris, a, b, c, d);
        }
    }
}

void EmitLocalTris(const std::vector<Vec3>& localTris, const Vec3& offset,
                   const Mat4& world, std::vector<NavMeshTriangle>& out) {
    for (size_t i = 0; i + 2 < localTris.size(); i += 3) {
        NavMeshTriangle t;
        t.a = world.TransformPoint(localTris[i] + offset);
        t.b = world.TransformPoint(localTris[i + 1] + offset);
        t.c = world.TransformPoint(localTris[i + 2] + offset);
        out.push_back(t);
    }
}

void EmitModelAsset(const asset::ModelAsset* model, const Mat4& world,
                    std::vector<NavMeshTriangle>& out) {
    if (!model || model->GetMeshCount() == 0) {
        return;
    }
    for (const asset::Mesh& mesh : model->GetMeshes()) {
        const std::vector<asset::Vertex>& verts = mesh.vertices;
        const std::vector<uint32_t>& idx = mesh.indices;
        for (size_t i = 0; i + 2 < idx.size(); i += 3) {
            if (idx[i] >= verts.size() || idx[i + 1] >= verts.size() || idx[i + 2] >= verts.size()) {
                continue;
            }
            NavMeshTriangle t;
            t.a = world.TransformPoint(verts[idx[i]].position);
            t.b = world.TransformPoint(verts[idx[i + 1]].position);
            t.c = world.TransformPoint(verts[idx[i + 2]].position);
            out.push_back(t);
        }
    }
}

void EmitCollisionMesh(const CollisionMesh3DComponent& cm, const Mat4& world,
                       std::vector<NavMeshTriangle>& out) {
    Vec3 offset = cm.GetOffset();
    std::vector<Vec3> local;
    switch (cm.GetShapeType()) {
        case CollisionShape3DType::Box:
            GenBox(local, cm.GetSize() * 0.5f);
            break;
        case CollisionShape3DType::Sphere:
            GenSphere(local, cm.GetRadius(), 16, 8);
            break;
        case CollisionShape3DType::Capsule:
            GenCapsule(local, cm.GetRadius(), cm.GetHeight(), 16, 4);
            break;
        case CollisionShape3DType::Cylinder:
            GenCylinder(local, cm.GetRadius(), cm.GetHeight(), 16);
            break;
        case CollisionShape3DType::Cone:
            GenCone(local, cm.GetRadius(), cm.GetHeight(), 16);
            break;
        case CollisionShape3DType::Plane:
            GenPlane(local, cm.GetPlaneNormal(), cm.GetPlaneWidth(), cm.GetPlaneLength(), cm.GetPlaneDistance());
            break;
        case CollisionShape3DType::Mesh: {
            std::string path = cm.GetMeshPath();
            if (!path.empty()) {
                asset::ModelAsset model;
                if (model.LoadFromFile(path)) {
                    EmitModelAsset(&model, world, out);
                }
            }
            return;
        }
    }
    EmitLocalTris(local, offset, world, out);
}

void EmitPrimitiveMesh(const PrimitiveMesh3D& pm, const Mat4& world,
                       std::vector<NavMeshTriangle>& out) {
    std::vector<Vec3> local;
    int detail = std::max(4, pm.GetDetail());
    float size = pm.GetSize();
    float height = pm.GetHeight();
    switch (pm.GetShape()) {
        case PrimitiveShape::Cube:
            GenBox(local, Vec3(size * 0.5f, size * 0.5f, size * 0.5f));
            break;
        case PrimitiveShape::Sphere:
            GenSphere(local, size * 0.5f, detail, std::max(2, detail / 2));
            break;
        case PrimitiveShape::Cylinder:
            GenCylinder(local, size * 0.5f, height, detail);
            break;
        case PrimitiveShape::Cone:
            GenCone(local, size * 0.5f, height, detail);
            break;
        case PrimitiveShape::Pyramid:
            GenPyramid(local, size, height);
            break;
        case PrimitiveShape::Torus:
            GenTorus(local, size * 0.5f, pm.GetMinorRadius(), detail, std::max(6, detail));
            break;
        case PrimitiveShape::Capsule:
            GenCapsule(local, size * 0.5f, height, detail, std::max(2, detail / 2));
            break;
    }
    EmitLocalTris(local, Vec3::Zero(), world, out);
}

} // namespace

NavigationRegion3D::NavigationRegion3D()
    : Component("NavigationRegion3D") {}

NavigationRegion3D::NavigationRegion3D(const std::string& name)
    : Component(name) {}

NavigationRegion3D::~NavigationRegion3D() {
    Unregister();
}

void NavigationRegion3D::DefineProperties() {
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(cellSize, 0.3f, 0.05f, 10.0f, 0.05f, "Baking"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(cellHeight, 0.2f, 0.05f, 10.0f, 0.05f, "Baking"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(agentHeight, 2.0f, 0.1f, 50.0f, 0.1f, "Baking"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(agentRadius, 0.5f, 0.0f, 50.0f, 0.1f, "Baking"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(agentMaxClimb, 0.4f, 0.0f, 50.0f, 0.05f, "Baking"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(maxSlopeDegrees, 45.0f, 0.0f, 89.0f, 1.0f, "Baking"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(minRegionArea, 8, 0, 100000, 1, "Baking"));
    DefineProperty(PROPERTY_ENUM_GROUP(geometrySource, 0, "Geometry", Children, EntireScene));
    DefineProperty(PROPERTY_DEFAULT_GROUP(includeCollision, Bool, true, "Geometry"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(includeMeshes, Bool, false, "Geometry"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(autoBake, Bool, true, "Geometry"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(useManualBounds, Bool, false, "Bounds"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(boundsCenter, Vec3, Vec3(0.0f, 0.0f, 0.0f), "Bounds"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(boundsExtents, Vec3, Vec3(50.0f, 20.0f, 50.0f), "Bounds"));
}

void NavigationRegion3D::DefineSignals() {
    RegisterSignal({"baked", {}, "Emitted after the navmesh is baked and published."});
}

void NavigationRegion3D::OnReady() {
    m_LastCarveVersion = navigation::NavigationServer3D::GetInstance().GetCarveVersion();
    if (GetPropertyValue<bool>("autoBake")) {
        Bake();
    }
}

void NavigationRegion3D::OnUpdate(float deltaTime) {
    (void)deltaTime;
    uint64_t carveVersion = navigation::NavigationServer3D::GetInstance().GetCarveVersion();
    if (carveVersion != m_LastCarveVersion) {
        m_LastCarveVersion = carveVersion;
        if (GetPropertyValue<bool>("autoBake")) {
            Bake();
        }
    }
}

void NavigationRegion3D::OnDestroy() {
    Unregister();
}

void NavigationRegion3D::OnExitTree() {
    Unregister();
}

void NavigationRegion3D::OnPropertyChanged(const std::string& propertyName,
                                           const nlohmann::json& newValue) {
    (void)newValue;
    static const char* kBakeParams[] = {
        "cellSize", "cellHeight", "agentHeight", "agentRadius", "agentMaxClimb",
        "maxSlopeDegrees", "minRegionArea", "geometrySource", "includeCollision",
        "includeMeshes", "useManualBounds", "boundsCenter", "boundsExtents", "enabled"};
    for (const char* p : kBakeParams) {
        if (propertyName == p) {
            if (GetPropertyValue<bool>("autoBake")) {
                Bake();
            }
            return;
        }
    }
}

bool NavigationRegion3D::GetEnabledRegion() const {
    return IsEnabled();
}

void NavigationRegion3D::SetEnabledRegion(bool enabled) {
    SetEnabled(enabled);
    Bake();
}

float NavigationRegion3D::GetCellSize() const { return GetPropertyValue<float>("cellSize"); }
void NavigationRegion3D::SetCellSize(float value) { SetPropertyValue<float>("cellSize", value); }
float NavigationRegion3D::GetCellHeight() const { return GetPropertyValue<float>("cellHeight"); }
void NavigationRegion3D::SetCellHeight(float value) { SetPropertyValue<float>("cellHeight", value); }
float NavigationRegion3D::GetAgentHeight() const { return GetPropertyValue<float>("agentHeight"); }
void NavigationRegion3D::SetAgentHeight(float value) { SetPropertyValue<float>("agentHeight", value); }
float NavigationRegion3D::GetAgentRadius() const { return GetPropertyValue<float>("agentRadius"); }
void NavigationRegion3D::SetAgentRadius(float value) { SetPropertyValue<float>("agentRadius", value); }
float NavigationRegion3D::GetAgentMaxClimb() const { return GetPropertyValue<float>("agentMaxClimb"); }
void NavigationRegion3D::SetAgentMaxClimb(float value) { SetPropertyValue<float>("agentMaxClimb", value); }
float NavigationRegion3D::GetMaxSlopeDegrees() const { return GetPropertyValue<float>("maxSlopeDegrees"); }
void NavigationRegion3D::SetMaxSlopeDegrees(float value) { SetPropertyValue<float>("maxSlopeDegrees", value); }
int NavigationRegion3D::GetMinRegionArea() const { return GetPropertyValue<int>("minRegionArea"); }
void NavigationRegion3D::SetMinRegionArea(int value) { SetPropertyValue<int>("minRegionArea", value); }
int NavigationRegion3D::GetGeometrySource() const { return GetPropertyValue<int>("geometrySource"); }
void NavigationRegion3D::SetGeometrySource(int value) { SetPropertyValue<int>("geometrySource", value); }
bool NavigationRegion3D::GetIncludeCollision() const { return GetPropertyValue<bool>("includeCollision"); }
void NavigationRegion3D::SetIncludeCollision(bool value) { SetPropertyValue<bool>("includeCollision", value); }
bool NavigationRegion3D::GetIncludeMeshes() const { return GetPropertyValue<bool>("includeMeshes"); }
void NavigationRegion3D::SetIncludeMeshes(bool value) { SetPropertyValue<bool>("includeMeshes", value); }

std::string NavigationRegion3D::RegionId() const {
    return GetUUID().ToString();
}

void NavigationRegion3D::Unregister() {
    if (m_Registered) {
        navigation::NavigationServer3D::GetInstance().RemoveRegion(RegionId());
        m_Registered = false;
    }
}

navigation::NavMeshBakeConfig NavigationRegion3D::BuildConfig() const {
    navigation::NavMeshBakeConfig cfg;
    cfg.cellSize = GetPropertyValue<float>("cellSize");
    cfg.cellHeight = GetPropertyValue<float>("cellHeight");
    cfg.agentHeight = GetPropertyValue<float>("agentHeight");
    cfg.agentRadius = GetPropertyValue<float>("agentRadius");
    cfg.agentMaxClimb = GetPropertyValue<float>("agentMaxClimb");
    cfg.maxSlopeDegrees = GetPropertyValue<float>("maxSlopeDegrees");
    cfg.minRegionArea = GetPropertyValue<int>("minRegionArea");
    if (GetPropertyValue<bool>("useManualBounds")) {
        Vec3 center = GetPropertyValue<Vec3>("boundsCenter");
        Vec3 extents = GetPropertyValue<Vec3>("boundsExtents");
        Node3D* owner = dynamic_cast<Node3D*>(GetOwner());
        if (owner) {
            center += owner->GetGlobalPosition();
        }
        cfg.hasBounds = true;
        cfg.boundsMin = center - extents;
        cfg.boundsMax = center + extents;
    }
    return cfg;
}

void NavigationRegion3D::CollectGeometry(std::vector<NavMeshTriangle>& out) const {
    Node* owner = GetOwner();
    if (!owner) {
        return;
    }
    bool includeCollision = GetPropertyValue<bool>("includeCollision");
    bool includeMeshes = GetPropertyValue<bool>("includeMeshes");
    GeometrySource source = static_cast<GeometrySource>(GetPropertyValue<int>("geometrySource"));

    Node* root = owner;
    if (source == GeometrySource::EntireScene) {
        Scene* scene = owner->GetScene();
        if (scene && scene->GetRoot()) {
            root = scene->GetRoot().get();
        }
    }

    std::function<void(Node*)> visit = [&](Node* node) {
        if (!node) {
            return;
        }
        Node3D* n3 = dynamic_cast<Node3D*>(node);
        if (n3) {
            Mat4 world = n3->GetGlobalTransformMatrix();
            for (const std::shared_ptr<Component>& comp : node->GetComponents()) {
                if (!comp) {
                    continue;
                }
                const std::string type = comp->GetTypeName();
                if (includeCollision && type == "CollisionMesh3DComponent") {
                    if (auto* cm = dynamic_cast<CollisionMesh3DComponent*>(comp.get())) {
                        EmitCollisionMesh(*cm, world, out);
                    }
                } else if (includeMeshes && type == "StaticMesh3D") {
                    if (auto* sm = dynamic_cast<StaticMesh3D*>(comp.get())) {
                        EmitModelAsset(sm->GetModelAsset().Get(), world, out);
                    }
                } else if (includeMeshes && type == "PrimitiveMesh3D") {
                    if (auto* pm = dynamic_cast<PrimitiveMesh3D*>(comp.get())) {
                        EmitPrimitiveMesh(*pm, world, out);
                    }
                }
            }
        }
        for (const std::shared_ptr<Node>& child : node->GetChildren()) {
            visit(child.get());
        }
    };
    visit(root);
}

void NavigationRegion3D::Bake() {
    std::vector<NavMeshTriangle> input;
    CollectGeometry(input);

    navigation::NavMeshBakeConfig cfg = BuildConfig();
    navigation::NavigationServer3D& server = navigation::NavigationServer3D::GetInstance();
    std::vector<navigation::NavMeshCarveVolume> carves = server.GetActiveCarveVolumes();

    m_BakedTriangles.clear();
    navigation::NavMeshBaker::Bake(cfg, input, carves, m_BakedTriangles);

    server.UpdateRegion(RegionId(), m_BakedTriangles, GetEnabledRegion());
    m_Registered = true;
    m_BakedPolygonCount = server.GetRegionPolygonCount(RegionId());
    m_LastCarveVersion = server.GetCarveVersion();

    Emit("baked");
}

nlohmann::json NavigationRegion3D::CallMethod(const std::string& method,
                                              const nlohmann::json& args) {
    auto argAt = [&](size_t i) -> nlohmann::json {
        if (args.is_array() && i < args.size()) {
            return args[i];
        }
        return nlohmann::json();
    };

    if (method == "bake" || method == "rebake") {
        Bake();
    } else if (method == "set_enabled") {
        nlohmann::json a = argAt(0);
        SetEnabledRegion(a.is_boolean() ? a.get<bool>() : true);
    } else if (method == "is_enabled") {
        return GetEnabledRegion();
    } else if (method == "get_polygon_count") {
        return static_cast<int>(m_BakedPolygonCount);
    } else if (method == "get_triangle_count") {
        return static_cast<int>(m_BakedTriangles.size());
    }
    return nlohmann::json();
}

void NavigationRegion3D::buildDrawCommands(RenderContext& ctx) {
    Scene* scene = GetOwner() ? GetOwner()->GetScene() : nullptr;
    if (!scene || !scene->IsInEditor()) {
        return; // editor-only visualization
    }
    if (m_BakedTriangles.empty()) {
        return;
    }
    math::Color edge = GetEnabledRegion()
        ? math::Color(0.25f, 0.85f, 0.95f, 0.8f)
        : math::Color(0.5f, 0.5f, 0.5f, 0.5f);
    for (const NavMeshTriangle& t : m_BakedTriangles) {
        ctx.drawLine(t.a, t.b, edge, 1.5f);
        ctx.drawLine(t.b, t.c, edge, 1.5f);
        ctx.drawLine(t.c, t.a, edge, 1.5f);
    }
}

AABB NavigationRegion3D::getWorldBounds() const {
    if (m_BakedTriangles.empty()) {
        Node3D* owner = dynamic_cast<Node3D*>(GetOwner());
        Vec3 c = owner ? owner->GetGlobalPosition() : Vec3::Zero();
        return AABB(c - Vec3(1.0f), c + Vec3(1.0f));
    }
    Vec3 lo(std::numeric_limits<float>::max());
    Vec3 hi(-std::numeric_limits<float>::max());
    auto acc = [&](const Vec3& v) {
        lo.x = std::min(lo.x, v.x); lo.y = std::min(lo.y, v.y); lo.z = std::min(lo.z, v.z);
        hi.x = std::max(hi.x, v.x); hi.y = std::max(hi.y, v.y); hi.z = std::max(hi.z, v.z);
    };
    for (const NavMeshTriangle& t : m_BakedTriangles) {
        acc(t.a); acc(t.b); acc(t.c);
    }
    return AABB(lo, hi);
}

RenderLayer NavigationRegion3D::getRenderLayer() const {
    return RenderLayer::Transparent;
}

SpatialType NavigationRegion3D::getSpatialType() const {
    return SpatialType::World3D;
}

} // namespace components
} // namespace lupine
