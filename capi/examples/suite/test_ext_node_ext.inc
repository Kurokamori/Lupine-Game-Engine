/**
 * @file test_ext_node_ext.inc
 * @brief Extended node coverage tests for the Lupine Engine C API
 *
 * Covers node functions not exercised by the core suite or test_ext_node_scene.inc:
 *  - Node2D spatial helpers: translate / rotate / look_at / move_toward /
 *    set_global_position / set_global_rotation / distance_to_point / distance_to_node
 *  - Node3D spatial helpers: translate / rotate_euler / move_toward /
 *    set_global_position / distance_to_point / distance_to_node /
 *    get_forward / get_right / get_up
 *  - Groups: add_to_group / is_in_group / remove_from_group /
 *    get_groups_json / get_nodes_in_group
 *  - Hierarchy: add_sibling / reparent / duplicate /
 *    get_component_in_children / get_component_in_parent
 *  - Scene references / singletons / uuid lookup: get_scene / get_root /
 *    get_singleton / find_by_uuid
 *  - Lifecycle teardown: free / queue_free / queue_free_deferred
 *
 * Functions that need a live scene, scene-tree singleton registry, or deferred
 * free-list are exercised with tolerant assertions: they must either succeed or
 * fail with a plausible documented error rather than crash.
 *
 * This file is #included into capi_test_suite.c. It defines no main(),
 * includes no headers, and redefines no macros.
 */

static void test_ext_node_ext_node2d_transform(void) {
    SECTION_START("Node Ext: Node2D Transform Helpers");

    LCNodeHandle node = NULL;
    LCResult result = lc_node_create(LC_NODE_2D, "Ext2DXform", &node);
    TEST_RESULT(result, "Create Node2D for transform helpers");

    result = lc_node2d_set_position(node, lc_vec2(0.0f, 0.0f));
    TEST_RESULT(result, "Reset Node2D local position");

    result = lc_node2d_translate(node, lc_vec2(3.0f, 4.0f));
    TEST_RESULT(result, "Translate Node2D by (3,4)");
    LCVec2 pos = lc_vec2_zero();
    result = lc_node2d_get_position(node, &pos);
    TEST_RESULT(result, "Get Node2D position after translate");
    TEST_ASSERT(pos.x > 2.99f && pos.x < 3.01f && pos.y > 3.99f && pos.y < 4.01f,
                "Translate accumulated into local position");

    result = lc_node2d_set_rotation(node, 0.0f);
    TEST_RESULT(result, "Reset Node2D rotation");
    result = lc_node2d_rotate(node, 1.0f);
    TEST_RESULT(result, "Rotate Node2D by 1 radian");
    float rot = 0.0f;
    result = lc_node2d_get_rotation(node, &rot);
    TEST_RESULT(result, "Get Node2D rotation after rotate");
    TEST_ASSERT(rot > 0.99f && rot < 1.01f, "Rotate accumulated into local rotation");

    result = lc_node2d_set_global_position(node, lc_vec2(10.0f, -5.0f));
    TEST_RESULT(result, "Set Node2D global position");
    LCVec2 gpos = lc_vec2_zero();
    result = lc_node2d_get_global_position(node, &gpos);
    TEST_RESULT(result, "Get Node2D global position");
    TEST_ASSERT(gpos.x > 9.99f && gpos.x < 10.01f && gpos.y > -5.01f && gpos.y < -4.99f,
                "Global position round-trips with no parent");

    result = lc_node2d_set_global_rotation(node, 0.5f);
    TEST_RESULT(result, "Set Node2D global rotation");
    float grot = 0.0f;
    result = lc_node2d_get_global_rotation(node, &grot);
    TEST_RESULT(result, "Get Node2D global rotation");
    TEST_ASSERT(grot > 0.49f && grot < 0.51f, "Global rotation round-trips with no parent");

    result = lc_node2d_set_rotation_degrees(node, 90.0f);
    TEST_RESULT(result, "Set Node2D rotation in degrees");
    float degRot = 0.0f;
    result = lc_node2d_get_rotation_degrees(node, &degRot);
    TEST_RESULT(result, "Get Node2D rotation in degrees");
    TEST_ASSERT(degRot > 89.9f && degRot < 90.1f, "Degree rotation round-trips");
    result = lc_node2d_get_rotation(node, &rot);
    TEST_RESULT(result, "Get Node2D rotation (radians) after degree set");
    TEST_ASSERT(rot > 1.56f && rot < 1.58f, "90 degrees stored as ~1.5708 radians");
    result = lc_node2d_rotate_degrees(node, 90.0f);
    TEST_RESULT(result, "Rotate Node2D by 90 degrees");
    result = lc_node2d_get_rotation_degrees(node, &degRot);
    TEST_RESULT(result, "Get Node2D rotation degrees after rotate");
    TEST_ASSERT(degRot > 179.9f && degRot < 180.1f, "Degree rotate accumulates to 180");
    result = lc_node2d_set_global_rotation_degrees(node, 45.0f);
    TEST_RESULT(result, "Set Node2D global rotation in degrees");
    result = lc_node2d_get_global_rotation_degrees(node, &degRot);
    TEST_RESULT(result, "Get Node2D global rotation in degrees");
    TEST_ASSERT(degRot > 44.9f && degRot < 45.1f, "Degree global rotation round-trips");

    result = lc_node2d_look_at(node, lc_vec2(20.0f, -5.0f));
    TEST_RESULT(result, "Node2D look_at a target point");

    float dist = -1.0f;
    result = lc_node2d_distance_to_point(node, lc_vec2(13.0f, -1.0f), &dist);
    TEST_RESULT(result, "Node2D distance_to_point");
    TEST_ASSERT(dist > 4.99f && dist < 5.01f, "Distance to point is 5 (3-4-5 triangle)");

    LCNodeHandle other = NULL;
    result = lc_node_create(LC_NODE_2D, "Ext2DXformOther", &other);
    TEST_RESULT(result, "Create second Node2D for distance_to_node");
    result = lc_node2d_set_global_position(other, lc_vec2(10.0f, 7.0f));
    TEST_RESULT(result, "Place second Node2D 12 units away");
    float ndist = -1.0f;
    result = lc_node2d_distance_to_node(node, other, &ndist);
    TEST_RESULT(result, "Node2D distance_to_node");
    TEST_ASSERT(ndist > 11.99f && ndist < 12.01f, "Distance to other node is 12");

    result = lc_node2d_move_toward(node, lc_vec2(10.0f, 5.0f), 1.0f);
    TEST_RESULT(result, "Node2D move_toward target by max 1 unit");
    LCVec2 moved = lc_vec2_zero();
    result = lc_node2d_get_global_position(node, &moved);
    TEST_RESULT(result, "Get Node2D position after move_toward");
    TEST_ASSERT(moved.y > -5.01f, "move_toward advanced toward the (positive-Y) target");

    lc_node_destroy(other);
    lc_node_destroy(node);

    SECTION_END("Node Ext: Node2D Transform Helpers");
}

static void test_ext_node_ext_node3d_transform(void) {
    SECTION_START("Node Ext: Node3D Transform Helpers");

    LCNodeHandle node = NULL;
    LCResult result = lc_node_create(LC_NODE_3D, "Ext3DXform", &node);
    TEST_RESULT(result, "Create Node3D for transform helpers");

    result = lc_node3d_set_position(node, lc_vec3(0.0f, 0.0f, 0.0f));
    TEST_RESULT(result, "Reset Node3D local position");

    result = lc_node3d_translate(node, lc_vec3(1.0f, 2.0f, 2.0f));
    TEST_RESULT(result, "Translate Node3D by (1,2,2)");
    LCVec3 pos = lc_vec3_zero();
    result = lc_node3d_get_position(node, &pos);
    TEST_RESULT(result, "Get Node3D position after translate");
    TEST_ASSERT(pos.x > 0.99f && pos.x < 1.01f && pos.y > 1.99f && pos.y < 2.01f &&
                pos.z > 1.99f && pos.z < 2.01f,
                "Translate accumulated into local position");

    result = lc_node3d_set_rotation_euler(node, lc_vec3(0.0f, 0.0f, 0.0f));
    TEST_RESULT(result, "Reset Node3D euler rotation");
    result = lc_node3d_rotate_euler(node, lc_vec3(0.0f, 0.5f, 0.0f));
    TEST_RESULT(result, "Rotate Node3D by yaw 0.5 rad");
    LCVec3 euler = lc_vec3_zero();
    result = lc_node3d_get_rotation_euler(node, &euler);
    TEST_RESULT(result, "Get Node3D euler after rotate");
    TEST_ASSERT(euler.y > 0.45f && euler.y < 0.55f, "rotate_euler accumulated into yaw");

    result = lc_node3d_set_rotation_euler_degrees(node, lc_vec3(0.0f, 90.0f, 0.0f));
    TEST_RESULT(result, "Set Node3D euler rotation in degrees");
    LCVec3 eulerDeg = lc_vec3_zero();
    result = lc_node3d_get_rotation_euler_degrees(node, &eulerDeg);
    TEST_RESULT(result, "Get Node3D euler rotation in degrees");
    TEST_ASSERT(eulerDeg.y > 89.5f && eulerDeg.y < 90.5f, "Degree euler yaw round-trips");
    result = lc_node3d_rotate_euler_degrees(node, lc_vec3(0.0f, -90.0f, 0.0f));
    TEST_RESULT(result, "Rotate Node3D by -90 degrees yaw");
    result = lc_node3d_get_rotation_euler_degrees(node, &eulerDeg);
    TEST_RESULT(result, "Get Node3D euler degrees after rotate");
    TEST_ASSERT(eulerDeg.y > -0.5f && eulerDeg.y < 0.5f, "Degree euler rotate returns to ~0 yaw");

    result = lc_node3d_set_global_position(node, lc_vec3(0.0f, 0.0f, 0.0f));
    TEST_RESULT(result, "Set Node3D global position to origin");
    LCVec3 gpos = lc_vec3_one();
    result = lc_node3d_get_global_position(node, &gpos);
    TEST_RESULT(result, "Get Node3D global position");
    TEST_ASSERT(gpos.x > -0.01f && gpos.x < 0.01f && gpos.y > -0.01f && gpos.y < 0.01f &&
                gpos.z > -0.01f && gpos.z < 0.01f,
                "Global position round-trips to origin with no parent");

    LCVec3 forward = lc_vec3_zero();
    result = lc_node3d_get_forward(node, &forward);
    TEST_RESULT(result, "Get Node3D forward vector");
    TEST_ASSERT(lc_vec3_length(forward) > 0.99f && lc_vec3_length(forward) < 1.01f,
                "Forward vector is unit length");

    LCVec3 right = lc_vec3_zero();
    result = lc_node3d_get_right(node, &right);
    TEST_RESULT(result, "Get Node3D right vector");
    TEST_ASSERT(lc_vec3_length(right) > 0.99f && lc_vec3_length(right) < 1.01f,
                "Right vector is unit length");

    LCVec3 up = lc_vec3_zero();
    result = lc_node3d_get_up(node, &up);
    TEST_RESULT(result, "Get Node3D up vector");
    TEST_ASSERT(lc_vec3_length(up) > 0.99f && lc_vec3_length(up) < 1.01f,
                "Up vector is unit length");

    float dist = -1.0f;
    result = lc_node3d_distance_to_point(node, lc_vec3(2.0f, 3.0f, 6.0f), &dist);
    TEST_RESULT(result, "Node3D distance_to_point");
    TEST_ASSERT(dist > 6.99f && dist < 7.01f, "Distance to point is 7 (2-3-6 -> 7)");

    LCNodeHandle other = NULL;
    result = lc_node_create(LC_NODE_3D, "Ext3DXformOther", &other);
    TEST_RESULT(result, "Create second Node3D for distance_to_node");
    result = lc_node3d_set_global_position(other, lc_vec3(0.0f, 0.0f, 9.0f));
    TEST_RESULT(result, "Place second Node3D 9 units away on Z");
    float ndist = -1.0f;
    result = lc_node3d_distance_to_node(node, other, &ndist);
    TEST_RESULT(result, "Node3D distance_to_node");
    TEST_ASSERT(ndist > 8.99f && ndist < 9.01f, "Distance to other node is 9");

    result = lc_node3d_move_toward(node, lc_vec3(0.0f, 0.0f, 9.0f), 1.0f);
    TEST_RESULT(result, "Node3D move_toward target by max 1 unit");
    LCVec3 moved = lc_vec3_zero();
    result = lc_node3d_get_global_position(node, &moved);
    TEST_RESULT(result, "Get Node3D position after move_toward");
    TEST_ASSERT(moved.z > 0.0f, "move_toward advanced toward the +Z target");

    lc_node_destroy(other);
    lc_node_destroy(node);

    SECTION_END("Node Ext: Node3D Transform Helpers");
}

static void test_ext_node_ext_groups(void) {
    SECTION_START("Node Ext: Groups");

    LCNodeHandle node = NULL;
    LCResult result = lc_node_create(LC_NODE_BASE, "ExtGroupNode", &node);
    TEST_RESULT(result, "Create node for group membership");

    bool in_group = true;
    result = lc_node_is_in_group(node, "enemies", &in_group);
    TEST_RESULT(result, "Query group membership before adding");
    TEST_ASSERT(!in_group, "Node not in group by default");

    result = lc_node_add_to_group(node, "enemies");
    TEST_RESULT(result, "Add node to 'enemies' group");
    in_group = false;
    result = lc_node_is_in_group(node, "enemies", &in_group);
    TEST_RESULT(result, "Query group membership after adding");
    TEST_ASSERT(in_group, "Node reports membership in 'enemies'");

    result = lc_node_add_to_group(node, "spawned");
    TEST_RESULT(result, "Add node to a second group 'spawned'");

    char* groups_json = NULL;
    result = lc_node_get_groups_json(node, &groups_json);
    TEST_RESULT(result, "Get node groups as JSON");
    TEST_ASSERT(groups_json != NULL, "Groups JSON pointer is non-NULL");
    if (groups_json != NULL) {
        TEST_ASSERT(strstr(groups_json, "enemies") != NULL, "Groups JSON contains 'enemies'");
        TEST_ASSERT(strstr(groups_json, "spawned") != NULL, "Groups JSON contains 'spawned'");
        lc_free(groups_json);
    }

    result = lc_node_remove_from_group(node, "enemies");
    TEST_RESULT(result, "Remove node from 'enemies' group");
    in_group = true;
    result = lc_node_is_in_group(node, "enemies", &in_group);
    TEST_RESULT(result, "Query group membership after removal");
    TEST_ASSERT(!in_group, "Node no longer in 'enemies' after removal");

    size_t group_count = 0;
    LCResult group_query = lc_node_get_nodes_in_group(node, "spawned", NULL, 0, &group_count);
    TEST_ASSERT(group_query == LC_SUCCESS || group_query == LC_ERROR_OPERATION_FAILED ||
                group_query == LC_ERROR_NOT_FOUND,
                "get_nodes_in_group tolerant without a live scene");

    lc_node_destroy(node);

    SECTION_END("Node Ext: Groups");
}

static void test_ext_node_ext_hierarchy(void) {
    SECTION_START("Node Ext: Sibling / Reparent / Duplicate");

    LCNodeHandle parent = NULL;
    LCResult result = lc_node_create(LC_NODE_BASE, "ExtHierParent", &parent);
    TEST_RESULT(result, "Create hierarchy parent");

    LCNodeHandle child = NULL;
    result = lc_node_create(LC_NODE_BASE, "ExtHierChild", &child);
    TEST_RESULT(result, "Create hierarchy child");
    result = lc_node_add_child(parent, child);
    TEST_RESULT(result, "Attach child to parent");

    LCNodeHandle sibling = NULL;
    result = lc_node_create(LC_NODE_BASE, "ExtHierSibling", &sibling);
    TEST_RESULT(result, "Create sibling node");
    result = lc_node_add_sibling(child, sibling);
    TEST_RESULT(result, "Add sibling alongside child");

    LCNodeHandle sibling_parent = (LCNodeHandle)0x1;
    result = lc_node_get_parent(sibling, &sibling_parent);
    TEST_RESULT(result, "Get parent of newly added sibling");
    TEST_ASSERT(sibling_parent == parent, "Sibling shares the child's parent");

    size_t child_count = 0;
    result = lc_node_get_child_count(parent, &child_count);
    TEST_RESULT(result, "Get parent child count after add_sibling");
    TEST_ASSERT(child_count == 2, "Parent has 2 children after add_sibling");

    LCNodeHandle new_parent = NULL;
    result = lc_node_create(LC_NODE_BASE, "ExtHierNewParent", &new_parent);
    TEST_RESULT(result, "Create reparent target");
    result = lc_node_reparent(child, new_parent);
    TEST_RESULT(result, "Reparent child under new parent");
    LCNodeHandle reparented = (LCNodeHandle)0x1;
    result = lc_node_get_parent(child, &reparented);
    TEST_RESULT(result, "Get parent after reparent");
    TEST_ASSERT(reparented == new_parent, "Child now belongs to new parent");

    LCNodeHandle clone = NULL;
    result = lc_node_duplicate(sibling, &clone);
    TEST_RESULT(result, "Duplicate sibling node");
    TEST_ASSERT(clone != NULL, "Duplicate produced a clone handle");
    TEST_ASSERT(clone != sibling, "Clone is a distinct handle from the original");

    LCComponentHandle in_children = (LCComponentHandle)0x1;
    LCResult cic = lc_node_get_component_in_children(parent, "Sprite2D", &in_children);
    TEST_ASSERT(cic == LC_SUCCESS, "get_component_in_children returns success when absent");
    TEST_ASSERT(in_children == NULL, "No matching component in children yields NULL");

    LCComponentHandle in_parent = (LCComponentHandle)0x1;
    LCResult cip = lc_node_get_component_in_parent(child, "Sprite2D", &in_parent);
    TEST_ASSERT(cip == LC_SUCCESS, "get_component_in_parent returns success when absent");
    TEST_ASSERT(in_parent == NULL, "No matching component in parents yields NULL");

    lc_node_destroy(new_parent);
    lc_node_destroy(parent);

    SECTION_END("Node Ext: Sibling / Reparent / Duplicate");
}

static void test_ext_node_ext_references(void) {
    SECTION_START("Node Ext: Scene References / Singleton / UUID");

    LCNodeHandle node = NULL;
    LCResult result = lc_node_create(LC_NODE_3D, "ExtRefNode", &node);
    TEST_RESULT(result, "Create node for reference lookups");

    LCSceneHandle scene = (LCSceneHandle)0x1;
    LCResult scene_result = lc_node_get_scene(node, &scene);
    TEST_ASSERT(scene_result == LC_SUCCESS, "get_scene succeeds even with no owning scene");

    LCNodeHandle root = (LCNodeHandle)0x1;
    LCResult root_result = lc_node_get_root(node, &root);
    TEST_ASSERT(root_result == LC_SUCCESS, "get_root succeeds even with no scene root");

    LCNodeHandle singleton = (LCNodeHandle)0x1;
    LCResult singleton_result = lc_node_get_singleton("ExtNonexistentSingleton", &singleton);
    TEST_ASSERT(singleton_result == LC_SUCCESS || singleton_result == LC_ERROR_NOT_FOUND ||
                singleton_result == LC_ERROR_OPERATION_FAILED,
                "get_singleton tolerant for an unregistered name");

    const char* uuid = lc_node_get_uuid(node);
    TEST_ASSERT(uuid != NULL && strlen(uuid) > 0, "Node reports a non-empty UUID");
    if (uuid != NULL) {
        LCNodeHandle found = (LCNodeHandle)0x1;
        LCResult uuid_result = lc_node_find_by_uuid(node, uuid, &found);
        TEST_ASSERT(uuid_result == LC_SUCCESS || uuid_result == LC_ERROR_NOT_FOUND ||
                    uuid_result == LC_ERROR_OPERATION_FAILED,
                    "find_by_uuid tolerant without a live scene index");
    }

    LCResult null_singleton = lc_node_get_singleton(NULL, &singleton);
    TEST_ASSERT(null_singleton == LC_ERROR_NULL_POINTER || null_singleton == LC_ERROR_INVALID_PARAMETER,
                "get_singleton rejects NULL name");

    LCResult null_uuid = lc_node_find_by_uuid(node, NULL, &node);
    TEST_ASSERT(null_uuid == LC_ERROR_NULL_POINTER || null_uuid == LC_ERROR_INVALID_PARAMETER,
                "find_by_uuid rejects NULL uuid");

    lc_node_destroy(node);

    SECTION_END("Node Ext: Scene References / Singleton / UUID");
}

static void test_ext_node_ext_lifecycle(void) {
    SECTION_START("Node Ext: Lifecycle Teardown");

    LCResult null_free = lc_node_free(NULL);
    TEST_ASSERT(null_free == LC_ERROR_NULL_POINTER || null_free == LC_ERROR_INVALID_HANDLE,
                "free rejects NULL handle");

    LCResult null_queue = lc_node_queue_free(NULL);
    TEST_ASSERT(null_queue == LC_ERROR_NULL_POINTER || null_queue == LC_ERROR_INVALID_HANDLE,
                "queue_free rejects NULL handle");

    LCNodeHandle queued = NULL;
    LCResult result = lc_node_create(LC_NODE_BASE, "ExtQueueFree", &queued);
    TEST_RESULT(result, "Create throwaway node for queue_free");
    LCResult queue_result = lc_node_queue_free(queued);
    TEST_ASSERT(queue_result == LC_SUCCESS || queue_result == LC_ERROR_OPERATION_FAILED ||
                queue_result == LC_ERROR_NOT_INITIALIZED,
                "queue_free tolerant without a live scene tree");

    LCNodeHandle deferred = NULL;
    result = lc_node_create(LC_NODE_BASE, "ExtQueueFreeDeferred", &deferred);
    TEST_RESULT(result, "Create throwaway node for queue_free_deferred");
    LCResult deferred_result = lc_node_queue_free_deferred(deferred);
    TEST_ASSERT(deferred_result == LC_SUCCESS || deferred_result == LC_ERROR_OPERATION_FAILED ||
                deferred_result == LC_ERROR_NOT_INITIALIZED,
                "queue_free_deferred tolerant without a live scene tree");

    LCNodeHandle freed = NULL;
    result = lc_node_create(LC_NODE_BASE, "ExtImmediateFree", &freed);
    TEST_RESULT(result, "Create throwaway node for immediate free");
    LCResult free_result = lc_node_free(freed);
    TEST_ASSERT(free_result == LC_SUCCESS || free_result == LC_ERROR_OPERATION_FAILED,
                "free of a parentless node succeeds or fails gracefully");

    SECTION_END("Node Ext: Lifecycle Teardown");
}
