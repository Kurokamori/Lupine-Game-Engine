#include "lupine/navigation/NavMesh3D.hpp"
#include "lupine/navigation/NavMeshBaker.hpp"
#include "lupine/navigation/NavigationServer3D.hpp"
#include "lupine/math/Vec3.hpp"
#include "TestFramework.hpp"

#include <cmath>
#include <iostream>
#include <vector>

using namespace lupine;
using namespace lupine::navigation;

namespace {

// Build a flat NxN grid of quads (each `cell` units) at y=`height` in the XZ
// plane, spanning [0, N*cell] in both axes, as a list of triangles.
std::vector<NavMeshTriangle> MakeFlatGridTriangles(int n, float cell, float height) {
    std::vector<NavMeshTriangle> tris;
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            math::Vec3 v00(i * cell, height, j * cell);
            math::Vec3 v10((i + 1) * cell, height, j * cell);
            math::Vec3 v11((i + 1) * cell, height, (j + 1) * cell);
            math::Vec3 v01(i * cell, height, (j + 1) * cell);
            tris.push_back({v00, v10, v11});
            tris.push_back({v00, v11, v01});
        }
    }
    return tris;
}

NavMesh3D MakeFlatGridMesh(int n, float cell, float height) {
    NavMesh3D mesh;
    for (const NavMeshTriangle& t : MakeFlatGridTriangles(n, cell, height)) {
        mesh.AddTriangle(t.a, t.b, t.c);
    }
    mesh.BuildConnectivity();
    return mesh;
}

bool TestGeom3DHelpers() {
    TEST_SECTION("geom3d Helpers");

    math::Vec3 a(0.0f, 0.0f, 0.0f);
    math::Vec3 b(1.0f, 0.0f, 0.0f);
    math::Vec3 c(1.0f, 0.0f, 1.0f);
    TEST_ASSERT(geom3d::TriArea2XZ(a, b, c) > 0.0f, "CCW-from-above triangle has positive XZ area");
    TEST_ASSERT(geom3d::TriArea2XZ(a, c, b) < 0.0f, "Reversed winding has negative XZ area");

    std::vector<math::Vec3> quad = {
        {0.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 2.0f}, {0.0f, 0.0f, 2.0f}};
    TEST_ASSERT(geom3d::PointInConvexXZ(math::Vec3(1.0f, 0.0f, 1.0f), quad), "Interior point is inside convex");
    TEST_ASSERT(!geom3d::PointInConvexXZ(math::Vec3(3.0f, 0.0f, 1.0f), quad), "Exterior point is outside convex");
    TEST_ASSERT(geom3d::PointInPolygonXZ(math::Vec3(1.0f, 0.0f, 1.0f), quad), "Interior point is inside polygon");
    TEST_ASSERT(!geom3d::PointInPolygonXZ(math::Vec3(-1.0f, 0.0f, 1.0f), quad), "Exterior point is outside polygon");

    std::vector<std::array<int, 3>> tris;
    bool ok = geom3d::TriangulateXZ(quad, tris);
    TEST_ASSERT(ok, "Quad triangulates successfully");
    TEST_ASSERT(tris.size() == 2, "Quad triangulates into exactly 2 triangles");

    return true;
}

bool TestNavMesh3DPathfinding() {
    TEST_SECTION("NavMesh3D Pathfinding");

    NavMesh3D mesh = MakeFlatGridMesh(3, 10.0f, 0.0f);
    TEST_ASSERT(mesh.PolygonCount() == 18, "3x3 grid yields 18 triangles");

    int center = mesh.FindContainingPolygon(math::Vec3(15.0f, 0.0f, 15.0f));
    TEST_ASSERT(center >= 0, "Interior point lands in a polygon");

    float y = -999.0f;
    bool sampled = mesh.SampleHeight(15.0f, 15.0f, y);
    TEST_ASSERT(sampled && std::fabs(y) < 1e-3f, "SampleHeight returns the flat surface elevation");

    math::Vec3 clamped = mesh.GetClosestPoint(math::Vec3(100.0f, 5.0f, 15.0f));
    TEST_ASSERT(clamped.x <= 30.5f, "Exterior point clamps onto the surface");

    std::vector<math::Vec3> path;
    bool found = mesh.FindPath(math::Vec3(5.0f, 0.0f, 5.0f), math::Vec3(25.0f, 0.0f, 25.0f), path);
    TEST_ASSERT(found, "Path across the grid is found");
    TEST_ASSERT(path.size() >= 2, "Path has at least a start and an end");
    TEST_ASSERT(std::fabs(path.front().x - 5.0f) < 0.5f && std::fabs(path.front().z - 5.0f) < 0.5f,
                "Path starts near the requested start");
    TEST_ASSERT(std::fabs(path.back().x - 25.0f) < 0.5f && std::fabs(path.back().z - 25.0f) < 0.5f,
                "Path ends near the requested end");

    // A disconnected mesh: path between separate islands must fail.
    NavMesh3D split;
    split.AddTriangle(math::Vec3(0, 0, 0), math::Vec3(1, 0, 0), math::Vec3(1, 0, 1));
    split.AddTriangle(math::Vec3(50, 0, 50), math::Vec3(51, 0, 50), math::Vec3(51, 0, 51));
    split.BuildConnectivity();
    std::vector<math::Vec3> noPath;
    bool connected = split.FindPath(math::Vec3(0.2f, 0, 0.2f), math::Vec3(50.5f, 0, 50.5f), noPath);
    TEST_ASSERT(!connected, "Disconnected islands report no path");

    return true;
}

bool TestNavMesh3DElevation() {
    TEST_SECTION("NavMesh3D Elevation");

    // Two adjacent quads on a ramp: floor at y=0 rising to y=2 across z.
    NavMesh3D mesh;
    mesh.AddTriangle(math::Vec3(0, 0, 0), math::Vec3(10, 0, 0), math::Vec3(10, 0, 10));
    mesh.AddTriangle(math::Vec3(0, 0, 0), math::Vec3(10, 0, 10), math::Vec3(0, 0, 10));
    mesh.AddTriangle(math::Vec3(0, 0, 10), math::Vec3(10, 0, 10), math::Vec3(10, 2, 20));
    mesh.AddTriangle(math::Vec3(0, 0, 10), math::Vec3(10, 2, 20), math::Vec3(0, 2, 20));
    mesh.BuildConnectivity();

    std::vector<math::Vec3> path;
    bool found = mesh.FindPath(math::Vec3(5, 0, 2), math::Vec3(5, 2, 18), path);
    TEST_ASSERT(found, "Path up the ramp is found");
    TEST_ASSERT(path.back().y > 1.0f, "Path end carries the raised elevation");

    return true;
}

bool TestBakerFlatFloor() {
    TEST_SECTION("NavMeshBaker Flat Floor");

    // A single 20x20 floor quad (two triangles) at y=0.
    std::vector<NavMeshTriangle> input;
    input.push_back({math::Vec3(0, 0, 0), math::Vec3(20, 0, 0), math::Vec3(20, 0, 20)});
    input.push_back({math::Vec3(0, 0, 0), math::Vec3(20, 0, 20), math::Vec3(0, 0, 20)});

    NavMeshBakeConfig cfg;
    cfg.cellSize = 0.5f;
    cfg.cellHeight = 0.2f;
    cfg.agentHeight = 2.0f;
    cfg.agentRadius = 0.5f;
    cfg.agentMaxClimb = 0.4f;
    cfg.maxSlopeDegrees = 45.0f;
    cfg.minRegionArea = 1;

    std::vector<NavMeshTriangle> out;
    bool ok = NavMeshBaker::Bake(cfg, input, {}, out);
    TEST_ASSERT(ok, "Flat floor bakes successfully");
    TEST_ASSERT(!out.empty(), "Bake produces navmesh triangles");

    NavMesh3D mesh;
    for (const NavMeshTriangle& t : out) {
        mesh.AddTriangle(t.a, t.b, t.c);
    }
    mesh.BuildConnectivity();

    int interior = mesh.FindContainingPolygon(math::Vec3(10.0f, 0.0f, 10.0f), 1.0f);
    TEST_ASSERT(interior >= 0, "Baked floor covers its interior");

    std::vector<math::Vec3> path;
    bool found = mesh.FindPath(math::Vec3(4, 0, 4), math::Vec3(16, 0, 16), path);
    TEST_ASSERT(found, "Path across the baked floor is found");
    TEST_ASSERT(path.size() >= 2, "Baked-floor path has start and end");

    return true;
}

bool TestBakerCarveVolume() {
    TEST_SECTION("NavMeshBaker Carve Volume");

    std::vector<NavMeshTriangle> input;
    input.push_back({math::Vec3(0, 0, 0), math::Vec3(20, 0, 0), math::Vec3(20, 0, 20)});
    input.push_back({math::Vec3(0, 0, 0), math::Vec3(20, 0, 20), math::Vec3(0, 0, 20)});

    NavMeshBakeConfig cfg;
    cfg.cellSize = 0.5f;
    cfg.cellHeight = 0.2f;
    cfg.agentHeight = 2.0f;
    cfg.agentRadius = 0.5f;
    cfg.agentMaxClimb = 0.4f;
    cfg.maxSlopeDegrees = 45.0f;
    cfg.minRegionArea = 1;

    // Carve a 6x6 hole around the centre (10,10).
    std::vector<NavMeshCarveVolume> carves;
    NavMeshCarveVolume hole;
    hole.box = math::AABB(math::Vec3(7.0f, -2.0f, 7.0f), math::Vec3(13.0f, 2.0f, 13.0f));
    carves.push_back(hole);

    std::vector<NavMeshTriangle> out;
    bool ok = NavMeshBaker::Bake(cfg, input, carves, out);
    TEST_ASSERT(ok, "Carved floor bakes successfully");

    NavMesh3D mesh;
    for (const NavMeshTriangle& t : out) {
        mesh.AddTriangle(t.a, t.b, t.c);
    }
    mesh.BuildConnectivity();

    int carved = mesh.FindContainingPolygon(math::Vec3(10.0f, 0.0f, 10.0f), 1.0f);
    TEST_ASSERT(carved < 0, "Carved centre is no longer navigable");

    int edge = mesh.FindContainingPolygon(math::Vec3(3.0f, 0.0f, 3.0f), 1.0f);
    TEST_ASSERT(edge >= 0, "Floor away from the carve remains navigable");

    // A path from one side to the other must route around the hole.
    std::vector<math::Vec3> path;
    bool found = mesh.FindPath(math::Vec3(3, 0, 10), math::Vec3(17, 0, 10), path);
    TEST_ASSERT(found, "Path around the carved hole connects");

    return true;
}

bool TestNavigationServer3D() {
    TEST_SECTION("NavigationServer3D");

    NavigationServer3D& server = NavigationServer3D::GetInstance();
    server.Clear();

    std::vector<NavMeshTriangle> tris = MakeFlatGridTriangles(3, 10.0f, 0.0f);
    server.UpdateRegion("test_region", tris, true);
    TEST_ASSERT(server.GetRegionCount() == 1, "Region registered with the server");
    TEST_ASSERT(server.GetRegionPolygonCount("test_region") == 18, "Server reports the region polygon count");

    bool navigable = server.IsPointNavigable(math::Vec3(15.0f, 0.0f, 15.0f), 1.0f);
    TEST_ASSERT(navigable, "Interior point is navigable through the server");

    std::vector<math::Vec3> path;
    bool found = server.QueryPath(math::Vec3(5, 0, 5), math::Vec3(25, 0, 25), path);
    TEST_ASSERT(found, "Server path query succeeds");
    TEST_ASSERT(path.size() >= 2, "Server path has start and end");

    float y = -999.0f;
    bool sampled = server.SampleHeight(15.0f, 15.0f, y);
    TEST_ASSERT(sampled && std::fabs(y) < 1e-3f, "Server samples the surface height");

    // Carve volumes are tracked separately and bump their own version.
    uint64_t before = server.GetCarveVersion();
    server.UpdateCarveVolume("vol", math::AABB(math::Vec3(0, 0, 0), math::Vec3(1, 1, 1)), true);
    TEST_ASSERT(server.GetCarveVolumeCount() == 1, "Carve volume registered");
    TEST_ASSERT(server.GetCarveVersion() != before, "Carve version advances on change");
    TEST_ASSERT(server.GetActiveCarveVolumes().size() == 1, "Active carve volumes reported");

    server.RemoveRegion("test_region");
    TEST_ASSERT(server.GetRegionCount() == 0, "Region can be removed");

    server.Clear();
    return true;
}

} // namespace

void RunNavigation3DTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "NAVIGATION 3D TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    lupine_test::SetCurrentSuite("Navigation 3D");
    bool allPassed = true;

    allPassed &= TestGeom3DHelpers();
    allPassed &= TestNavMesh3DPathfinding();
    allPassed &= TestNavMesh3DElevation();
    allPassed &= TestBakerFlatFloor();
    allPassed &= TestBakerCarveVolume();
    allPassed &= TestNavigationServer3D();

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "ALL NAVIGATION 3D TESTS PASSED!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}
