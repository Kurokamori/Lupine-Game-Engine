static void test_ext_navigation3d(void) {
    SECTION_START("Ext: Navigation 3D");

    /* ----- Region: configuration + bake (no scene geometry attached, so the
     * bake produces an empty navmesh; the API surface is still exercised). --- */
    LCComponentHandle region = NULL;
    LCResult result = lc_navigation_region3d_create("ExtNavRegion3D", &region);
    TEST_RESULT(result, "Create NavigationRegion3D component");

    if (region != NULL) {
        result = lc_navigation_region3d_set_cell_size(region, 0.3f);
        TEST_RESULT(result, "lc_navigation_region3d_set_cell_size()");
        result = lc_navigation_region3d_set_cell_height(region, 0.2f);
        TEST_RESULT(result, "lc_navigation_region3d_set_cell_height()");
        result = lc_navigation_region3d_set_agent_height(region, 2.0f);
        TEST_RESULT(result, "lc_navigation_region3d_set_agent_height()");
        result = lc_navigation_region3d_set_agent_radius(region, 0.5f);
        TEST_RESULT(result, "lc_navigation_region3d_set_agent_radius()");
        result = lc_navigation_region3d_set_agent_max_climb(region, 0.4f);
        TEST_RESULT(result, "lc_navigation_region3d_set_agent_max_climb()");
        result = lc_navigation_region3d_set_max_slope_degrees(region, 45.0f);
        TEST_RESULT(result, "lc_navigation_region3d_set_max_slope_degrees()");
        result = lc_navigation_region3d_set_min_region_area(region, 4);
        TEST_RESULT(result, "lc_navigation_region3d_set_min_region_area()");
        result = lc_navigation_region3d_set_geometry_source(region, 0);
        TEST_RESULT(result, "lc_navigation_region3d_set_geometry_source()");
        result = lc_navigation_region3d_set_include_collision(region, true);
        TEST_RESULT(result, "lc_navigation_region3d_set_include_collision()");
        result = lc_navigation_region3d_set_include_meshes(region, false);
        TEST_RESULT(result, "lc_navigation_region3d_set_include_meshes()");

        result = lc_navigation_region3d_set_enabled(region, true);
        TEST_RESULT(result, "lc_navigation_region3d_set_enabled()");
        bool enabled = false;
        result = lc_navigation_region3d_is_enabled(region, &enabled);
        TEST_RESULT(result, "lc_navigation_region3d_is_enabled()");
        TEST_ASSERT(enabled == true, "region enabled round-trips");

        result = lc_navigation_region3d_bake(region);
        TEST_RESULT(result, "lc_navigation_region3d_bake()");

        int polyCount = -1;
        result = lc_navigation_region3d_get_polygon_count(region, &polyCount);
        TEST_RESULT(result, "lc_navigation_region3d_get_polygon_count()");
        TEST_ASSERT(polyCount >= 0, "polygon count is non-negative");

        int triCount = -1;
        result = lc_navigation_region3d_get_triangle_count(region, &triCount);
        TEST_RESULT(result, "lc_navigation_region3d_get_triangle_count()");
        TEST_ASSERT(triCount >= 0, "triangle count is non-negative");

        int regionCount = 0;
        result = lc_navigation_server3d_get_region_count(&regionCount);
        TEST_RESULT(result, "lc_navigation_server3d_get_region_count()");
        TEST_ASSERT(regionCount >= 1, "region registered with the 3D server");
    }

    /* ----- Agent ----- */
    LCComponentHandle agent = NULL;
    result = lc_navigation_agent3d_create("ExtNavAgent3D", &agent);
    TEST_RESULT(result, "Create NavigationAgent3D component");

    if (agent != NULL) {
        result = lc_navigation_agent3d_set_max_speed(agent, 5.0f);
        TEST_RESULT(result, "lc_navigation_agent3d_set_max_speed()");
        float speed = 0.0f;
        result = lc_navigation_agent3d_get_max_speed(agent, &speed);
        TEST_RESULT(result, "lc_navigation_agent3d_get_max_speed()");
        TEST_ASSERT(speed > 4.9f && speed < 5.1f, "max_speed round-trips");

        LCVec3 target = lc_vec3(10.0f, 0.0f, 10.0f);
        result = lc_navigation_agent3d_set_target_position(agent, target);
        TEST_RESULT(result, "lc_navigation_agent3d_set_target_position()");
        LCVec3 gotTarget = lc_vec3(0.0f, 0.0f, 0.0f);
        result = lc_navigation_agent3d_get_target_position(agent, &gotTarget);
        TEST_RESULT(result, "lc_navigation_agent3d_get_target_position()");
        TEST_ASSERT(gotTarget.x == target.x && gotTarget.y == target.y && gotTarget.z == target.z,
                    "target round-trips");

        LCVec3 nextPos = lc_vec3(0.0f, 0.0f, 0.0f);
        result = lc_navigation_agent3d_get_next_path_position(agent, &nextPos);
        TEST_RESULT(result, "lc_navigation_agent3d_get_next_path_position()");

        bool finished = false;
        result = lc_navigation_agent3d_is_navigation_finished(agent, &finished);
        TEST_RESULT(result, "lc_navigation_agent3d_is_navigation_finished()");

        bool reachable = true;
        result = lc_navigation_agent3d_is_target_reachable(agent, &reachable);
        TEST_RESULT(result, "lc_navigation_agent3d_is_target_reachable()");

        float dist = -1.0f;
        result = lc_navigation_agent3d_distance_to_target(agent, &dist);
        TEST_RESULT(result, "lc_navigation_agent3d_distance_to_target()");

        int agentPathCount = -1;
        result = lc_navigation_agent3d_get_path_count(agent, &agentPathCount);
        TEST_RESULT(result, "lc_navigation_agent3d_get_path_count()");

        result = lc_navigation_agent3d_set_auto_move(agent, true);
        TEST_RESULT(result, "lc_navigation_agent3d_set_auto_move()");

        result = lc_navigation_agent3d_set_radius(agent, 0.6f);
        TEST_RESULT(result, "lc_navigation_agent3d_set_radius()");
        float agentRadius = 0.0f;
        result = lc_navigation_agent3d_get_radius(agent, &agentRadius);
        TEST_RESULT(result, "lc_navigation_agent3d_get_radius()");
        TEST_ASSERT(agentRadius > 0.59f && agentRadius < 0.61f, "agent radius round-trips");

        result = lc_navigation_agent3d_set_avoidance_enabled(agent, true);
        TEST_RESULT(result, "lc_navigation_agent3d_set_avoidance_enabled()");
        bool avoidance = false;
        result = lc_navigation_agent3d_is_avoidance_enabled(agent, &avoidance);
        TEST_RESULT(result, "lc_navigation_agent3d_is_avoidance_enabled()");
        TEST_ASSERT(avoidance == true, "avoidance_enabled round-trips");

        LCVec3 velocity = lc_vec3(-1.0f, -1.0f, -1.0f);
        result = lc_navigation_agent3d_get_velocity(agent, &velocity);
        TEST_RESULT(result, "lc_navigation_agent3d_get_velocity()");

        result = lc_navigation_agent3d_reset(agent);
        TEST_RESULT(result, "lc_navigation_agent3d_reset()");

        result = lc_navigation_agent3d_get_max_speed(NULL, &speed);
        TEST_ASSERT(result != LC_SUCCESS, "lc_navigation_agent3d_get_max_speed(NULL) fails gracefully");

        result = lc_component_destroy(agent);
        TEST_RESULT(result, "Destroy NavigationAgent3D");
    }

    /* ----- Obstacle (dynamic avoidance + static carve volume) ----- */
    LCComponentHandle obstacle = NULL;
    result = lc_navigation_obstacle3d_create("ExtNavObstacle3D", &obstacle);
    TEST_RESULT(result, "Create NavigationObstacle3D component");

    if (obstacle != NULL) {
        result = lc_navigation_obstacle3d_set_radius(obstacle, 1.5f);
        TEST_RESULT(result, "lc_navigation_obstacle3d_set_radius()");
        float obRadius = 0.0f;
        result = lc_navigation_obstacle3d_get_radius(obstacle, &obRadius);
        TEST_RESULT(result, "lc_navigation_obstacle3d_get_radius()");
        TEST_ASSERT(obRadius > 1.49f && obRadius < 1.51f, "obstacle radius round-trips");

        result = lc_navigation_obstacle3d_set_height(obstacle, 3.0f);
        TEST_RESULT(result, "lc_navigation_obstacle3d_set_height()");

        result = lc_navigation_obstacle3d_set_avoidance_enabled(obstacle, true);
        TEST_RESULT(result, "lc_navigation_obstacle3d_set_avoidance_enabled()");

        result = lc_navigation_obstacle3d_set_carve(obstacle, true);
        TEST_RESULT(result, "lc_navigation_obstacle3d_set_carve()");

        result = lc_navigation_obstacle3d_bake(obstacle);
        TEST_RESULT(result, "lc_navigation_obstacle3d_bake()");

        int carveCount = 0;
        result = lc_navigation_server3d_get_carve_volume_count(&carveCount);
        TEST_RESULT(result, "lc_navigation_server3d_get_carve_volume_count()");
        TEST_ASSERT(carveCount >= 1, "carve volume registered with the 3D server");

        result = lc_component_destroy(obstacle);
        TEST_RESULT(result, "Destroy NavigationObstacle3D");
    }

    /* ----- Server queries (mesh may be empty without scene geometry) ----- */
    LCVec3 closest = lc_vec3(0.0f, 0.0f, 0.0f);
    result = lc_navigation_server3d_get_closest_point(lc_vec3(5.0f, 1.0f, 5.0f), &closest);
    TEST_RESULT(result, "lc_navigation_server3d_get_closest_point()");

    bool navigable = true;
    result = lc_navigation_server3d_is_point_navigable(lc_vec3(5.0f, 0.0f, 5.0f), 1.0f, &navigable);
    TEST_RESULT(result, "lc_navigation_server3d_is_point_navigable()");

    float sampledY = 0.0f;
    bool sampleFound = true;
    result = lc_navigation_server3d_sample_height(5.0f, 5.0f, &sampledY, &sampleFound);
    TEST_RESULT(result, "lc_navigation_server3d_sample_height()");

    LCVec3 pathPts[32];
    int pathCount = -1;
    result = lc_navigation_server3d_query_path(lc_vec3(0.0f, 0.0f, 0.0f),
                                               lc_vec3(8.0f, 0.0f, 8.0f),
                                               pathPts, 32, &pathCount);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_NOT_FOUND,
                "lc_navigation_server3d_query_path() returns success or not-found");

    int carveCount2 = -1;
    result = lc_navigation_server3d_get_carve_volume_count(&carveCount2);
    TEST_RESULT(result, "lc_navigation_server3d_get_carve_volume_count() (post-destroy)");

    if (region != NULL) {
        result = lc_navigation_region3d_get_polygon_count(NULL, &pathCount);
        TEST_ASSERT(result != LC_SUCCESS,
                    "lc_navigation_region3d_get_polygon_count(NULL) fails gracefully");
        result = lc_component_destroy(region);
        TEST_RESULT(result, "Destroy NavigationRegion3D");
    }

    SECTION_END("Ext: Navigation 3D");
}
