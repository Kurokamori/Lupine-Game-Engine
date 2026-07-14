static void test_ext_navigation(void) {
    SECTION_START("Ext: Navigation 2D");

    LCComponentHandle region = NULL;
    LCResult result = lc_navigation_region2d_create("ExtNavRegion", &region);
    TEST_RESULT(result, "Create NavigationRegion2D component");

    if (region != NULL) {
        /* A 200x200 square walkable area. Winding is normalized at bake time. */
        LCVec2 outline[4];
        outline[0] = lc_vec2(0.0f, 0.0f);
        outline[1] = lc_vec2(200.0f, 0.0f);
        outline[2] = lc_vec2(200.0f, 200.0f);
        outline[3] = lc_vec2(0.0f, 200.0f);
        result = lc_navigation_region2d_set_outline(region, outline, 4);
        TEST_RESULT(result, "lc_navigation_region2d_set_outline()");

        int polyCount = 0;
        result = lc_navigation_region2d_get_polygon_count(region, &polyCount);
        TEST_RESULT(result, "lc_navigation_region2d_get_polygon_count()");
        TEST_ASSERT(polyCount >= 2, "square outline triangulates into >= 2 polygons");

        result = lc_navigation_region2d_set_enabled(region, true);
        TEST_RESULT(result, "lc_navigation_region2d_set_enabled()");
        bool enabled = false;
        result = lc_navigation_region2d_is_enabled(region, &enabled);
        TEST_RESULT(result, "lc_navigation_region2d_is_enabled()");
        TEST_ASSERT(enabled == true, "enabled round-trips");

        int regionCount = 0;
        result = lc_navigation_server2d_get_region_count(&regionCount);
        TEST_RESULT(result, "lc_navigation_server2d_get_region_count()");
        TEST_ASSERT(regionCount >= 1, "region registered with the server");

        /* Point queries. */
        bool navigable = false;
        result = lc_navigation_server2d_is_point_navigable(lc_vec2(100.0f, 100.0f), &navigable);
        TEST_RESULT(result, "lc_navigation_server2d_is_point_navigable()");
        TEST_ASSERT(navigable == true, "interior point is navigable");

        LCVec2 closest = lc_vec2(0.0f, 0.0f);
        result = lc_navigation_server2d_get_closest_point(lc_vec2(500.0f, 100.0f), &closest);
        TEST_RESULT(result, "lc_navigation_server2d_get_closest_point()");
        TEST_ASSERT(closest.x <= 200.5f, "exterior point clamps onto the surface");

        /* Path query between two interior points. */
        LCVec2 pathPts[32];
        int pathCount = 0;
        result = lc_navigation_server2d_query_path(lc_vec2(20.0f, 20.0f),
                                                   lc_vec2(180.0f, 180.0f),
                                                   pathPts, 32, &pathCount);
        TEST_RESULT(result, "lc_navigation_server2d_query_path()");
        TEST_ASSERT(pathCount >= 2, "path has at least start and end");

        /* Agent. */
        LCComponentHandle agent = NULL;
        result = lc_navigation_agent2d_create("ExtNavAgent", &agent);
        TEST_RESULT(result, "Create NavigationAgent2D component");

        if (agent != NULL) {
            result = lc_navigation_agent2d_set_max_speed(agent, 150.0f);
            TEST_RESULT(result, "lc_navigation_agent2d_set_max_speed()");
            float speed = 0.0f;
            result = lc_navigation_agent2d_get_max_speed(agent, &speed);
            TEST_RESULT(result, "lc_navigation_agent2d_get_max_speed()");
            TEST_ASSERT(speed > 149.9f && speed < 150.1f, "max_speed round-trips");

            LCVec2 target = lc_vec2(150.0f, 150.0f);
            result = lc_navigation_agent2d_set_target_position(agent, target);
            TEST_RESULT(result, "lc_navigation_agent2d_set_target_position()");
            LCVec2 gotTarget = lc_vec2(0.0f, 0.0f);
            result = lc_navigation_agent2d_get_target_position(agent, &gotTarget);
            TEST_RESULT(result, "lc_navigation_agent2d_get_target_position()");
            TEST_ASSERT(gotTarget.x == target.x && gotTarget.y == target.y, "target round-trips");

            LCVec2 nextPos = lc_vec2(0.0f, 0.0f);
            result = lc_navigation_agent2d_get_next_path_position(agent, &nextPos);
            TEST_RESULT(result, "lc_navigation_agent2d_get_next_path_position()");

            bool finished = true;
            result = lc_navigation_agent2d_is_navigation_finished(agent, &finished);
            TEST_RESULT(result, "lc_navigation_agent2d_is_navigation_finished()");

            bool reachable = false;
            result = lc_navigation_agent2d_is_target_reachable(agent, &reachable);
            TEST_RESULT(result, "lc_navigation_agent2d_is_target_reachable()");

            float dist = -1.0f;
            result = lc_navigation_agent2d_distance_to_target(agent, &dist);
            TEST_RESULT(result, "lc_navigation_agent2d_distance_to_target()");

            int agentPathCount = -1;
            result = lc_navigation_agent2d_get_path_count(agent, &agentPathCount);
            TEST_RESULT(result, "lc_navigation_agent2d_get_path_count()");

            result = lc_navigation_agent2d_set_auto_move(agent, true);
            TEST_RESULT(result, "lc_navigation_agent2d_set_auto_move()");

            /* Local avoidance (ORCA) tuning. */
            result = lc_navigation_agent2d_set_radius(agent, 12.0f);
            TEST_RESULT(result, "lc_navigation_agent2d_set_radius()");
            float agentRadius = 0.0f;
            result = lc_navigation_agent2d_get_radius(agent, &agentRadius);
            TEST_RESULT(result, "lc_navigation_agent2d_get_radius()");
            TEST_ASSERT(agentRadius > 11.9f && agentRadius < 12.1f, "agent radius round-trips");

            result = lc_navigation_agent2d_set_avoidance_enabled(agent, true);
            TEST_RESULT(result, "lc_navigation_agent2d_set_avoidance_enabled()");
            bool avoidance = false;
            result = lc_navigation_agent2d_is_avoidance_enabled(agent, &avoidance);
            TEST_RESULT(result, "lc_navigation_agent2d_is_avoidance_enabled()");
            TEST_ASSERT(avoidance == true, "avoidance_enabled round-trips");

            LCVec2 velocity = lc_vec2(-1.0f, -1.0f);
            result = lc_navigation_agent2d_get_velocity(agent, &velocity);
            TEST_RESULT(result, "lc_navigation_agent2d_get_velocity()");

            result = lc_navigation_agent2d_reset(agent);
            TEST_RESULT(result, "lc_navigation_agent2d_reset()");

            /* Null-handle robustness. */
            result = lc_navigation_agent2d_get_max_speed(NULL, &speed);
            TEST_ASSERT(result != LC_SUCCESS, "lc_navigation_agent2d_get_max_speed(NULL) fails gracefully");
            result = lc_navigation_agent2d_is_navigation_finished(agent, NULL);
            TEST_ASSERT(result != LC_SUCCESS, "lc_navigation_agent2d_is_navigation_finished(NULL out) fails gracefully");

            result = lc_component_destroy(agent);
            TEST_RESULT(result, "Destroy NavigationAgent2D");
        }

        /* Static carving obstacle: cut a hole out of the centre of the region. */
        LCComponentHandle obstacle = NULL;
        result = lc_navigation_obstacle2d_create("ExtNavObstacle", &obstacle);
        TEST_RESULT(result, "Create NavigationObstacle2D component");

        if (obstacle != NULL) {
            result = lc_navigation_obstacle2d_set_radius(obstacle, 24.0f);
            TEST_RESULT(result, "lc_navigation_obstacle2d_set_radius()");
            float obRadius = 0.0f;
            result = lc_navigation_obstacle2d_get_radius(obstacle, &obRadius);
            TEST_RESULT(result, "lc_navigation_obstacle2d_get_radius()");
            TEST_ASSERT(obRadius > 23.9f && obRadius < 24.1f, "obstacle radius round-trips");

            result = lc_navigation_obstacle2d_set_avoidance_enabled(obstacle, true);
            TEST_RESULT(result, "lc_navigation_obstacle2d_set_avoidance_enabled()");

            LCVec2 hole[4];
            hole[0] = lc_vec2(80.0f, 80.0f);
            hole[1] = lc_vec2(120.0f, 80.0f);
            hole[2] = lc_vec2(120.0f, 120.0f);
            hole[3] = lc_vec2(80.0f, 120.0f);
            result = lc_navigation_obstacle2d_set_vertices(obstacle, hole, 4);
            TEST_RESULT(result, "lc_navigation_obstacle2d_set_vertices()");
            result = lc_navigation_obstacle2d_set_carve(obstacle, true);
            TEST_RESULT(result, "lc_navigation_obstacle2d_set_carve()");

            int obstacleCount = 0;
            result = lc_navigation_server2d_get_obstacle_count(&obstacleCount);
            TEST_RESULT(result, "lc_navigation_server2d_get_obstacle_count()");
            TEST_ASSERT(obstacleCount >= 1, "carve obstacle registered with the server");

            /* Re-bake the region so its polygon count reflects the carve. */
            result = lc_navigation_region2d_bake(region);
            TEST_RESULT(result, "Re-bake region after carve");
            int carvedCount = 0;
            result = lc_navigation_region2d_get_polygon_count(region, &carvedCount);
            TEST_RESULT(result, "polygon count after carve");
            TEST_ASSERT(carvedCount > polyCount, "carving subdivides the region into more polygons");

            /* The carved centre is no longer navigable. */
            bool centerNavigable = true;
            result = lc_navigation_server2d_is_point_navigable(lc_vec2(100.0f, 100.0f), &centerNavigable);
            TEST_RESULT(result, "is_point_navigable() at carved centre");
            TEST_ASSERT(centerNavigable == false, "carved centre is not navigable");

            /* A path across the carved area must still connect (routing around). */
            LCVec2 aroundPts[32];
            int aroundCount = 0;
            result = lc_navigation_server2d_query_path(lc_vec2(20.0f, 100.0f),
                                                       lc_vec2(180.0f, 100.0f),
                                                       aroundPts, 32, &aroundCount);
            TEST_RESULT(result, "query_path() around carved obstacle");
            TEST_ASSERT(aroundCount >= 2, "path around obstacle connects");

            result = lc_navigation_obstacle2d_bake(obstacle);
            TEST_RESULT(result, "lc_navigation_obstacle2d_bake()");

            result = lc_component_destroy(obstacle);
            TEST_RESULT(result, "Destroy NavigationObstacle2D");
        }

        result = lc_navigation_region2d_get_polygon_count(NULL, &polyCount);
        TEST_ASSERT(result != LC_SUCCESS, "lc_navigation_region2d_get_polygon_count(NULL) fails gracefully");

        result = lc_component_destroy(region);
        TEST_RESULT(result, "Destroy NavigationRegion2D");
    }

    SECTION_END("Ext: Navigation 2D");
}
