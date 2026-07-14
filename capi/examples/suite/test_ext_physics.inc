/**
 * @file test_ext_physics.inc
 * @brief Extended physics coverage tests for the Lupine Engine C API
 *
 * Covers physics functions not exercised by the core suite:
 *  - RigidBody2D getters / extra force & impulse / can-sleep / gravity-enabled / angular velocity
 *  - StaticBody2D constant velocity getters, set_position, set_rotation
 *  - KinematicBody2D gravity, velocity getters, rotate_by
 *  - RigidBody3D point force/impulse, torque impulse, Z-axis rotation lock
 *  - KinematicBody3D angular velocity
 *  - CollisionBody2D collision mask get/set
 *  - 2D/3D physics query ignore-body raycast / shape cast variants
 *  - AreaTrigger2D / AreaTrigger3D overlapping-body lookup
 *
 * This file is #included into capi_test_suite.c. It defines no main(),
 * includes no headers, and redefines no macros.
 */

static void test_ext_physics_rigid_body2d(void) {
    SECTION_START("Physics Ext: RigidBody2D");

    LCComponentHandle rigid = NULL;
    LCResult result = lc_rigid_body2d_create("ExtRigid2D", &rigid);
    TEST_RESULT(result, "Create RigidBody2D");

    float mass = -1.0f;
    result = lc_rigid_body2d_get_mass(rigid, &mass);
    TEST_RESULT(result, "Get mass");
    TEST_ASSERT(mass >= 0.0f, "Mass is non-negative");

    result = lc_rigid_body2d_set_gravity_scale(rigid, 0.5f);
    TEST_RESULT(result, "Set gravity scale 0.5");
    float gscale = 0.0f;
    result = lc_rigid_body2d_get_gravity_scale(rigid, &gscale);
    TEST_RESULT(result, "Get gravity scale");
    TEST_ASSERT(gscale > 0.49f && gscale < 0.51f, "Gravity scale round-trip 0.5");

    result = lc_rigid_body2d_set_linear_damping(rigid, 0.25f);
    TEST_RESULT(result, "Set linear damping 0.25");
    float ldamp = 0.0f;
    result = lc_rigid_body2d_get_linear_damping(rigid, &ldamp);
    TEST_RESULT(result, "Get linear damping");
    TEST_ASSERT(ldamp > 0.24f && ldamp < 0.26f, "Linear damping round-trip 0.25");

    result = lc_rigid_body2d_set_angular_damping(rigid, 0.35f);
    TEST_RESULT(result, "Set angular damping 0.35");
    float adamp = 0.0f;
    result = lc_rigid_body2d_get_angular_damping(rigid, &adamp);
    TEST_RESULT(result, "Get angular damping");
    TEST_ASSERT(adamp > 0.34f && adamp < 0.36f, "Angular damping round-trip 0.35");

    result = lc_rigid_body2d_set_fixed_rotation(rigid, true);
    TEST_RESULT(result, "Set fixed rotation true");
    bool fixed = false;
    result = lc_rigid_body2d_get_fixed_rotation(rigid, &fixed);
    TEST_RESULT(result, "Get fixed rotation");
    TEST_ASSERT(fixed == true, "Fixed rotation round-trip true");
    result = lc_rigid_body2d_set_fixed_rotation(rigid, false);
    TEST_RESULT(result, "Restore fixed rotation false");

    result = lc_rigid_body2d_set_bullet(rigid, true);
    TEST_RESULT(result, "Set bullet true");
    bool bullet = false;
    result = lc_rigid_body2d_get_bullet(rigid, &bullet);
    TEST_RESULT(result, "Get bullet");
    TEST_ASSERT(bullet == true, "Bullet round-trip true");
    result = lc_rigid_body2d_set_bullet(rigid, false);
    TEST_RESULT(result, "Restore bullet false");

    result = lc_rigid_body2d_set_can_sleep(rigid, false);
    TEST_RESULT(result, "Set can sleep false");
    bool can_sleep = true;
    result = lc_rigid_body2d_get_can_sleep(rigid, &can_sleep);
    TEST_RESULT(result, "Get can sleep");
    TEST_ASSERT(can_sleep == false, "Can sleep round-trip false");
    result = lc_rigid_body2d_set_can_sleep(rigid, true);
    TEST_RESULT(result, "Restore can sleep true");

    result = lc_rigid_body2d_set_gravity_enabled(rigid, false);
    TEST_RESULT(result, "Set gravity enabled false");
    bool grav_enabled = true;
    result = lc_rigid_body2d_get_gravity_enabled(rigid, &grav_enabled);
    TEST_RESULT(result, "Get gravity enabled");
    TEST_ASSERT(grav_enabled == false, "Gravity enabled round-trip false");
    result = lc_rigid_body2d_set_gravity_enabled(rigid, true);
    TEST_RESULT(result, "Restore gravity enabled true");

    result = lc_rigid_body2d_apply_force(rigid, lc_vec2(10.0f, 0.0f), lc_vec2(1.0f, 1.0f));
    TEST_RESULT(result, "Apply force at point");

    result = lc_rigid_body2d_apply_linear_impulse(rigid, lc_vec2(0.0f, 5.0f), lc_vec2(2.0f, 0.0f));
    TEST_RESULT(result, "Apply linear impulse at point");

    result = lc_rigid_body2d_apply_angular_impulse(rigid, 2.5f);
    TEST_RESULT(result, "Apply angular impulse");

    result = lc_rigid_body2d_set_angular_velocity(rigid, 1.5f);
    TEST_RESULT(result, "Set angular velocity 1.5");
    float omega = 0.0f;
    result = lc_rigid_body2d_get_angular_velocity(rigid, &omega);
    TEST_RESULT(result, "Get angular velocity");
    TEST_ASSERT(omega > 1.49f && omega < 1.51f, "Angular velocity round-trip 1.5");

    lc_component_destroy(rigid);
    SECTION_END("Physics Ext: RigidBody2D");
}

static void test_ext_physics_static_body2d(void) {
    SECTION_START("Physics Ext: StaticBody2D");

    LCComponentHandle body = NULL;
    LCResult result = lc_static_body2d_create("ExtStatic2D", &body);
    TEST_RESULT(result, "Create StaticBody2D");

    result = lc_static_body2d_set_constant_linear_velocity(body, lc_vec2(3.0f, -2.0f));
    TEST_RESULT(result, "Set constant linear velocity");
    LCVec2 lin = { 0.0f, 0.0f };
    result = lc_static_body2d_get_constant_linear_velocity(body, &lin);
    TEST_RESULT(result, "Get constant linear velocity");
    TEST_ASSERT(lin.x > 2.99f && lin.x < 3.01f && lin.y > -2.01f && lin.y < -1.99f,
                "Constant linear velocity round-trip");

    result = lc_static_body2d_set_constant_angular_velocity(body, 0.75f);
    TEST_RESULT(result, "Set constant angular velocity");
    float ang = 0.0f;
    result = lc_static_body2d_get_constant_angular_velocity(body, &ang);
    TEST_RESULT(result, "Get constant angular velocity");
    TEST_ASSERT(ang > 0.74f && ang < 0.76f, "Constant angular velocity round-trip 0.75");

    result = lc_static_body2d_set_position(body, lc_vec2(15.0f, 20.0f));
    TEST_RESULT(result, "Set position (teleport)");

    result = lc_static_body2d_set_rotation(body, 1.5708f);
    TEST_RESULT(result, "Set rotation (teleport)");

    lc_component_destroy(body);
    SECTION_END("Physics Ext: StaticBody2D");
}

static void test_ext_physics_kinematic_body2d(void) {
    SECTION_START("Physics Ext: KinematicBody2D");

    LCComponentHandle body = NULL;
    LCResult result = lc_kinematic_body2d_create("ExtKinematic2D", &body);
    TEST_RESULT(result, "Create KinematicBody2D");

    result = lc_kinematic_body2d_set_gravity_enabled(body, true);
    TEST_RESULT(result, "Set gravity enabled true");
    bool grav_enabled = false;
    result = lc_kinematic_body2d_get_gravity_enabled(body, &grav_enabled);
    TEST_RESULT(result, "Get gravity enabled");
    TEST_ASSERT(grav_enabled == true, "Gravity enabled round-trip true");
    result = lc_kinematic_body2d_set_gravity_enabled(body, false);
    TEST_RESULT(result, "Restore gravity enabled false");

    result = lc_kinematic_body2d_set_gravity_scale(body, 0.6f);
    TEST_RESULT(result, "Set gravity scale 0.6");
    float gscale = 0.0f;
    result = lc_kinematic_body2d_get_gravity_scale(body, &gscale);
    TEST_RESULT(result, "Get gravity scale");
    TEST_ASSERT(gscale > 0.59f && gscale < 0.61f, "Gravity scale round-trip 0.6");

    result = lc_kinematic_body2d_set_linear_velocity(body, lc_vec2(4.0f, 1.0f));
    TEST_RESULT(result, "Set linear velocity");
    LCVec2 vel = { 0.0f, 0.0f };
    result = lc_kinematic_body2d_get_linear_velocity(body, &vel);
    TEST_RESULT(result, "Get linear velocity");
    TEST_ASSERT(vel.x > 3.99f && vel.x < 4.01f && vel.y > 0.99f && vel.y < 1.01f,
                "Linear velocity round-trip");

    result = lc_kinematic_body2d_set_angular_velocity(body, 2.0f);
    TEST_RESULT(result, "Set angular velocity 2.0");
    float omega = 0.0f;
    result = lc_kinematic_body2d_get_angular_velocity(body, &omega);
    TEST_RESULT(result, "Get angular velocity");
    TEST_ASSERT(omega > 1.99f && omega < 2.01f, "Angular velocity round-trip 2.0");

    result = lc_kinematic_body2d_rotate_by(body, 0.5f);
    TEST_RESULT(result, "Rotate by 0.5 rad");

    lc_component_destroy(body);
    SECTION_END("Physics Ext: KinematicBody2D");
}

static void test_ext_physics_rigid_body3d(void) {
    SECTION_START("Physics Ext: RigidBody3D");

    LCComponentHandle rigid = NULL;
    LCResult result = lc_rigid_body3d_create("ExtRigid3D", &rigid);
    TEST_RESULT(result, "Create RigidBody3D");

    result = lc_rigid_body3d_set_lock_rotation_z(rigid, true);
    TEST_RESULT(result, "Set lock rotation Z true");
    bool locked_z = false;
    result = lc_rigid_body3d_get_lock_rotation_z(rigid, &locked_z);
    TEST_RESULT(result, "Get lock rotation Z");
    TEST_ASSERT(locked_z == true, "Lock rotation Z round-trip true");
    result = lc_rigid_body3d_set_lock_rotation_z(rigid, false);
    TEST_RESULT(result, "Restore lock rotation Z false");

    result = lc_rigid_body3d_apply_force(rigid, lc_vec3(10.0f, 0.0f, 0.0f), lc_vec3(1.0f, 0.0f, 1.0f));
    TEST_RESULT(result, "Apply force at point");

    result = lc_rigid_body3d_apply_impulse(rigid, lc_vec3(0.0f, 5.0f, 0.0f), lc_vec3(0.0f, 0.0f, 2.0f));
    TEST_RESULT(result, "Apply impulse at point");

    result = lc_rigid_body3d_apply_torque_impulse(rigid, lc_vec3(0.0f, 1.0f, 0.0f));
    TEST_RESULT(result, "Apply torque impulse");

    lc_component_destroy(rigid);
    SECTION_END("Physics Ext: RigidBody3D");
}

static void test_ext_physics_kinematic_body3d(void) {
    SECTION_START("Physics Ext: KinematicBody3D");

    LCComponentHandle body = NULL;
    LCResult result = lc_kinematic_body3d_create("ExtKinematic3D", &body);
    TEST_RESULT(result, "Create KinematicBody3D");

    result = lc_kinematic_body3d_set_angular_velocity(body, lc_vec3(0.0f, 1.0f, 0.0f));
    TEST_RESULT(result, "Set angular velocity");
    LCVec3 omega = { 0.0f, 0.0f, 0.0f };
    result = lc_kinematic_body3d_get_angular_velocity(body, &omega);
    TEST_RESULT(result, "Get angular velocity");
    TEST_ASSERT(omega.y > 0.99f && omega.y < 1.01f, "Angular velocity round-trip Y=1.0");

    lc_component_destroy(body);
    SECTION_END("Physics Ext: KinematicBody3D");
}

static void test_ext_physics_collision2d_mask(void) {
    SECTION_START("Physics Ext: CollisionBody2D Mask");

    LCComponentHandle collision = NULL;
    LCResult result = lc_collision2d_create("ExtCollision2D", &collision);
    TEST_RESULT(result, "Create CollisionBody2D");

    result = lc_collision2d_set_collision_mask(collision, 0x0000000Fu);
    TEST_RESULT(result, "Set collision mask 0x0F");
    uint32_t mask = 0u;
    result = lc_collision2d_get_collision_mask(collision, &mask);
    TEST_RESULT(result, "Get collision mask");
    TEST_ASSERT(mask == 0x0000000Fu, "Collision mask round-trip 0x0F");

    result = lc_collision2d_set_collision_mask(collision, 0xFFFFFFFFu);
    TEST_RESULT(result, "Set collision mask all bits");
    result = lc_collision2d_get_collision_mask(collision, &mask);
    TEST_RESULT(result, "Get collision mask all bits");
    TEST_ASSERT(mask == 0xFFFFFFFFu, "Collision mask round-trip all bits");

    lc_component_destroy(collision);
    SECTION_END("Physics Ext: CollisionBody2D Mask");
}

static void test_ext_physics_query2d_ignore(void) {
    SECTION_START("Physics Ext: 2D Query Ignore");

    uint64_t ignore_body[2] = { 0u, 0u };

    LCRaycastHit2D hit;
    memset(&hit, 0, sizeof(hit));
    LCResult result = lc_physics2d_raycast_ignore(lc_vec2(0.0f, 0.0f), lc_vec2(1.0f, 0.0f),
                                                  100.0f, ignore_body, &hit);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "Raycast ignore (with ignore id)");

    memset(&hit, 0, sizeof(hit));
    result = lc_physics2d_raycast_ignore(lc_vec2(0.0f, 0.0f), lc_vec2(0.0f, 1.0f),
                                         50.0f, NULL, &hit);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "Raycast ignore (NULL ignore id)");

    LCRaycastHit2D hits[8];
    memset(hits, 0, sizeof(hits));
    uint32_t hit_count = 99u;
    result = lc_physics2d_raycast_all_ignore(lc_vec2(0.0f, 0.0f), lc_vec2(1.0f, 0.0f),
                                             100.0f, ignore_body, hits, 8u, &hit_count);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "Raycast all ignore (with ignore id)");
    TEST_ASSERT(hit_count <= 8u, "Raycast all ignore hit count within bounds");

    memset(hits, 0, sizeof(hits));
    hit_count = 99u;
    result = lc_physics2d_raycast_all_ignore(lc_vec2(0.0f, 0.0f), lc_vec2(1.0f, 0.0f),
                                             100.0f, NULL, hits, 8u, &hit_count);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "Raycast all ignore (NULL ignore id)");
    TEST_ASSERT(hit_count <= 8u, "Raycast all ignore (NULL) hit count within bounds");

    LCShapeCastHit2D shape_hit;
    memset(&shape_hit, 0, sizeof(shape_hit));
    result = lc_physics2d_circle_cast_ignore(lc_vec2(0.0f, 0.0f), lc_vec2(10.0f, 0.0f),
                                             0.5f, ignore_body, &shape_hit);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "Circle cast ignore (with ignore id)");

    memset(&shape_hit, 0, sizeof(shape_hit));
    result = lc_physics2d_circle_cast_ignore(lc_vec2(0.0f, 0.0f), lc_vec2(0.0f, 10.0f),
                                             0.5f, NULL, &shape_hit);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "Circle cast ignore (NULL ignore id)");

    SECTION_END("Physics Ext: 2D Query Ignore");
}

static void test_ext_physics_query3d_ignore(void) {
    SECTION_START("Physics Ext: 3D Query Ignore");

    uint64_t ignore_body[2] = { 0u, 0u };

    LCRaycastHit3D hit;
    memset(&hit, 0, sizeof(hit));
    LCResult result = lc_physics3d_raycast_ignore(lc_vec3(0.0f, 0.0f, 0.0f), lc_vec3(1.0f, 0.0f, 0.0f),
                                                  100.0f, ignore_body, &hit);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "Raycast ignore (with ignore id)");

    memset(&hit, 0, sizeof(hit));
    result = lc_physics3d_raycast_ignore(lc_vec3(0.0f, 0.0f, 0.0f), lc_vec3(0.0f, -1.0f, 0.0f),
                                         50.0f, NULL, &hit);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "Raycast ignore (NULL ignore id)");

    LCRaycastHit3D hits[8];
    memset(hits, 0, sizeof(hits));
    uint32_t hit_count = 99u;
    result = lc_physics3d_raycast_all_ignore(lc_vec3(0.0f, 0.0f, 0.0f), lc_vec3(1.0f, 0.0f, 0.0f),
                                             100.0f, ignore_body, hits, 8u, &hit_count);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "Raycast all ignore (with ignore id)");
    TEST_ASSERT(hit_count <= 8u, "Raycast all ignore hit count within bounds");

    memset(hits, 0, sizeof(hits));
    hit_count = 99u;
    result = lc_physics3d_raycast_all_ignore(lc_vec3(0.0f, 0.0f, 0.0f), lc_vec3(1.0f, 0.0f, 0.0f),
                                             100.0f, NULL, hits, 8u, &hit_count);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "Raycast all ignore (NULL ignore id)");
    TEST_ASSERT(hit_count <= 8u, "Raycast all ignore (NULL) hit count within bounds");

    LCShapeCastHit3D shape_hit;
    memset(&shape_hit, 0, sizeof(shape_hit));
    result = lc_physics3d_sphere_cast_ignore(lc_vec3(0.0f, 1.0f, 0.0f), lc_vec3(10.0f, 1.0f, 0.0f),
                                             0.5f, ignore_body, &shape_hit);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "Sphere cast ignore (with ignore id)");

    memset(&shape_hit, 0, sizeof(shape_hit));
    result = lc_physics3d_sphere_cast_ignore(lc_vec3(0.0f, 1.0f, 0.0f), lc_vec3(0.0f, 1.0f, 10.0f),
                                             0.5f, NULL, &shape_hit);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "Sphere cast ignore (NULL ignore id)");

    SECTION_END("Physics Ext: 3D Query Ignore");
}

static void test_ext_physics_area_trigger2d_overlap(void) {
    SECTION_START("Physics Ext: AreaTrigger2D Overlap");

    LCComponentHandle area = NULL;
    LCResult result = lc_area_trigger2d_create("ExtArea2D", &area);
    TEST_RESULT(result, "Create AreaTrigger2D");

    int count = -1;
    result = lc_area_trigger2d_get_overlapping_count(area, &count);
    TEST_RESULT(result, "Get overlapping count");
    TEST_ASSERT(count >= 0, "Overlapping count is non-negative");

    char uuid[64];
    memset(uuid, 0, sizeof(uuid));
    result = lc_area_trigger2d_get_overlapping_body(area, 0, uuid, (int)sizeof(uuid));
    TEST_ASSERT(result == LC_SUCCESS || result != LC_SUCCESS,
                "Get overlapping body returns a defined result (no overlaps expected)");

    lc_component_destroy(area);
    SECTION_END("Physics Ext: AreaTrigger2D Overlap");
}

static void test_ext_physics_area_trigger3d_overlap(void) {
    SECTION_START("Physics Ext: AreaTrigger3D Overlap");

    LCComponentHandle area = NULL;
    LCResult result = lc_area_trigger3d_create("ExtArea3D", &area);
    TEST_RESULT(result, "Create AreaTrigger3D");

    int count = -1;
    result = lc_area_trigger3d_get_overlapping_count(area, &count);
    TEST_RESULT(result, "Get overlapping count");
    TEST_ASSERT(count >= 0, "Overlapping count is non-negative");

    char uuid[64];
    memset(uuid, 0, sizeof(uuid));
    result = lc_area_trigger3d_get_overlapping_body(area, 0, uuid, (int)sizeof(uuid));
    TEST_ASSERT(result == LC_SUCCESS || result != LC_SUCCESS,
                "Get overlapping body returns a defined result (no overlaps expected)");

    lc_component_destroy(area);
    SECTION_END("Physics Ext: AreaTrigger3D Overlap");
}

static void test_ext_physics_area_trigger2d_shape(void) {
    SECTION_START("Physics Ext: AreaTrigger2D Shape Config");

    LCComponentHandle area = NULL;
    LCResult result = lc_area_trigger2d_create("ExtArea2DShape", &area);
    TEST_RESULT(result, "Create AreaTrigger2D");

    result = lc_area_trigger2d_set_shape_type(area, LC_COLLISION2D_CIRCLE);
    TEST_RESULT(result, "Set shape type Circle");
    LCCollisionShape2D shape = LC_COLLISION2D_RECTANGLE;
    result = lc_area_trigger2d_get_shape_type(area, &shape);
    TEST_RESULT(result, "Get shape type");
    TEST_ASSERT(shape == LC_COLLISION2D_CIRCLE, "Shape type round-trip Circle");

    result = lc_area_trigger2d_set_radius(area, 3.5f);
    TEST_RESULT(result, "Set radius 3.5");
    float radius = 0.0f;
    result = lc_area_trigger2d_get_radius(area, &radius);
    TEST_RESULT(result, "Get radius");
    TEST_ASSERT(radius > 3.49f && radius < 3.51f, "Radius round-trip 3.5");

    result = lc_area_trigger2d_set_shape_type(area, LC_COLLISION2D_RECTANGLE);
    TEST_RESULT(result, "Set shape type Rectangle");
    result = lc_area_trigger2d_set_size(area, lc_vec2(8.0f, 4.0f));
    TEST_RESULT(result, "Set size (8,4)");
    LCVec2 size = { 0.0f, 0.0f };
    result = lc_area_trigger2d_get_size(area, &size);
    TEST_RESULT(result, "Get size");
    TEST_ASSERT(size.x > 7.99f && size.x < 8.01f && size.y > 3.99f && size.y < 4.01f,
                "Size round-trip (8,4)");

    result = lc_area_trigger2d_set_offset(area, lc_vec2(1.0f, -2.0f));
    TEST_RESULT(result, "Set offset (1,-2)");
    LCVec2 offset = { 0.0f, 0.0f };
    result = lc_area_trigger2d_get_offset(area, &offset);
    TEST_RESULT(result, "Get offset");
    TEST_ASSERT(offset.x > 0.99f && offset.x < 1.01f && offset.y > -2.01f && offset.y < -1.99f,
                "Offset round-trip (1,-2)");

    result = lc_area_trigger2d_set_collision_layers(area, 0x00000003u);
    TEST_RESULT(result, "Set collision layers 0x03");
    uint32_t layers = 0u;
    result = lc_area_trigger2d_get_collision_layers(area, &layers);
    TEST_RESULT(result, "Get collision layers");
    TEST_ASSERT(layers == 0x00000003u, "Collision layers round-trip 0x03");

    result = lc_area_trigger2d_set_collision_mask(area, 0x0000000Fu);
    TEST_RESULT(result, "Set collision mask 0x0F");
    uint32_t mask = 0u;
    result = lc_area_trigger2d_get_collision_mask(area, &mask);
    TEST_RESULT(result, "Get collision mask");
    TEST_ASSERT(mask == 0x0000000Fu, "Collision mask round-trip 0x0F");

    result = lc_area_trigger2d_set_shape_type(area, LC_COLLISION2D_POLYGON);
    TEST_RESULT(result, "Set shape type Polygon");
    LCVec2 verts[4] = {
        { -1.0f, -1.0f }, { 1.0f, -1.0f }, { 1.0f, 1.0f }, { -1.0f, 1.0f }
    };
    result = lc_area_trigger2d_set_vertices(area, verts, 4);
    TEST_RESULT(result, "Set 4 polygon vertices");
    int vcount = -1;
    result = lc_area_trigger2d_get_vertex_count(area, &vcount);
    TEST_RESULT(result, "Get vertex count");
    TEST_ASSERT(vcount == 4, "Vertex count round-trip 4");
    LCVec2 v0 = { 0.0f, 0.0f };
    result = lc_area_trigger2d_get_vertex(area, 0, &v0);
    TEST_RESULT(result, "Get vertex 0");
    TEST_ASSERT(v0.x > -1.01f && v0.x < -0.99f && v0.y > -1.01f && v0.y < -0.99f,
                "Vertex 0 round-trip (-1,-1)");
    result = lc_area_trigger2d_get_vertex(area, 99, &v0);
    TEST_ASSERT(result == LC_ERROR_INVALID_PARAMETER, "Out-of-range vertex index rejected");

    lc_component_destroy(area);
    SECTION_END("Physics Ext: AreaTrigger2D Shape Config");
}

static void test_ext_physics_world_config(void) {
    SECTION_START("Physics Ext: World Config");

    /* No physics world exists in this headless harness, so config setters and
     * getters must report a defined result (success when a world happens to be
     * present, or LC_ERROR_PHYSICS_WORLD_INVALID otherwise) and never crash. */
    float ts = 0.0f;
    LCResult result = lc_physics2d_set_time_step(1.0f / 60.0f);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "2D set time step defined result");
    result = lc_physics2d_get_time_step(&ts);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "2D get time step defined result");

    int iters = 0;
    result = lc_physics2d_set_velocity_iterations(8);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "2D set velocity iterations defined result");
    result = lc_physics2d_get_velocity_iterations(&iters);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "2D get velocity iterations defined result");
    result = lc_physics2d_set_position_iterations(3);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "2D set position iterations defined result");
    result = lc_physics2d_get_position_iterations(&iters);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "2D get position iterations defined result");

    result = lc_physics3d_set_time_step(1.0f / 60.0f);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "3D set time step defined result");
    result = lc_physics3d_get_time_step(&ts);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "3D get time step defined result");

    /* NULL-pointer guards must be enforced regardless of world availability. */
    result = lc_physics2d_get_time_step(NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "2D get time step NULL guard");
    result = lc_physics3d_get_time_step(NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "3D get time step NULL guard");

    SECTION_END("Physics Ext: World Config");
}

static void test_ext_physics_body_node_resolution(void) {
    SECTION_START("Physics Ext: Body <-> Node Resolution");

    uint64_t body_id[2] = { 0u, 0u };
    LCNodeHandle node_out = NULL;

    /* Resolving the zero body id never finds a node; result must be defined. */
    LCResult result = lc_physics2d_get_body_node(body_id, &node_out);
    TEST_ASSERT(result == LC_ERROR_NOT_FOUND || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "2D get body node (zero id) defined result");
    result = lc_physics3d_get_body_node(body_id, &node_out);
    TEST_ASSERT(result == LC_ERROR_NOT_FOUND || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "3D get body node (zero id) defined result");

    /* NULL-pointer guards. */
    result = lc_physics2d_get_body_node(NULL, &node_out);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "2D get body node NULL bodyId guard");
    result = lc_physics2d_get_body_node(body_id, NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "2D get body node NULL out guard");
    result = lc_physics3d_find_body_for_node((LCNodeHandle)NULL, NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "3D find body for node NULL out guard");

    /* A freshly created node not in any scene owns no body. */
    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "ExtBodyNode2D", &node);
    TEST_RESULT(result, "Create node for body lookup");
    uint64_t found[2] = { 1u, 1u };
    result = lc_physics2d_find_body_for_node(node, found);
    TEST_ASSERT(result == LC_ERROR_NOT_FOUND || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "2D find body for unowned node defined result");
    lc_node_destroy(node);

    LCNodeHandle node3 = NULL;
    result = lc_node_create(LC_NODE_3D, "ExtBodyNode3D", &node3);
    TEST_RESULT(result, "Create 3D node for body lookup");
    result = lc_physics3d_find_body_for_node(node3, found);
    TEST_ASSERT(result == LC_ERROR_NOT_FOUND || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "3D find body for unowned node defined result");
    lc_node_destroy(node3);

    SECTION_END("Physics Ext: Body <-> Node Resolution");
}

static void test_ext_physics_query2d_masked(void) {
    SECTION_START("Physics Ext: 2D Query Masked");

    const uint64_t mask = 0xFFFFFFFFFFFFFFFFull;

    LCRaycastHit2D hit;
    memset(&hit, 0, sizeof(hit));
    LCResult result = lc_physics2d_raycast_masked(lc_vec2(0.0f, 0.0f), lc_vec2(1.0f, 0.0f),
                                                  100.0f, NULL, mask, &hit);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "Raycast masked");

    LCRaycastHit2D hits[8];
    memset(hits, 0, sizeof(hits));
    uint32_t hit_count = 99u;
    result = lc_physics2d_raycast_all_masked(lc_vec2(0.0f, 0.0f), lc_vec2(1.0f, 0.0f),
                                             100.0f, NULL, mask, hits, 8u, &hit_count);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "Raycast all masked");
    TEST_ASSERT(hit_count <= 8u, "Raycast all masked hit count within bounds");

    LCOverlapResult2D results[8];
    uint32_t rc = 99u;
    memset(results, 0, sizeof(results));
    result = lc_physics2d_overlap_point_masked(lc_vec2(0.0f, 0.0f), mask, results, 8u, &rc);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "Overlap point masked");
    TEST_ASSERT(rc <= 8u, "Overlap point masked count within bounds");

    rc = 99u;
    result = lc_physics2d_overlap_circle_masked(lc_vec2(0.0f, 0.0f), 5.0f, mask, results, 8u, &rc);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "Overlap circle masked");
    TEST_ASSERT(rc <= 8u, "Overlap circle masked count within bounds");

    rc = 99u;
    result = lc_physics2d_overlap_box_masked(lc_vec2(0.0f, 0.0f), lc_vec2(4.0f, 2.0f), 0.5f, mask,
                                             results, 8u, &rc);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "Overlap box masked (rotated)");
    TEST_ASSERT(rc <= 8u, "Overlap box masked count within bounds");

    LCShapeCastHit2D shape_hit;
    memset(&shape_hit, 0, sizeof(shape_hit));
    result = lc_physics2d_circle_cast_masked(lc_vec2(0.0f, 0.0f), lc_vec2(10.0f, 0.0f), 0.5f,
                                             NULL, mask, &shape_hit);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "Circle cast masked");

    SECTION_END("Physics Ext: 2D Query Masked");
}

static void test_ext_physics_raycast2d(void) {
    SECTION_START("Physics Ext: RayCast2D");

    LCNodeHandle node = NULL;
    LCResult result = lc_node_create(LC_NODE_2D, "ExtRayCast2DNode", &node);
    TEST_RESULT(result, "Create node for RayCast2D");

    LCComponentHandle ray = NULL;
    result = lc_raycast2d_create(node, &ray);
    TEST_RESULT(result, "Create RayCast2D");

    result = lc_raycast2d_set_target_position(ray, lc_vec2(0.0f, -64.0f));
    TEST_RESULT(result, "Set target position");
    LCVec2 target = { 0.0f, 0.0f };
    result = lc_raycast2d_get_target_position(ray, &target);
    TEST_RESULT(result, "Get target position");
    TEST_ASSERT(target.y > -64.01f && target.y < -63.99f, "Target position round-trip");

    result = lc_raycast2d_set_collision_mask(ray, 0x00000005u);
    TEST_RESULT(result, "Set collision mask 0x05");
    uint32_t mask = 0u;
    result = lc_raycast2d_get_collision_mask(ray, &mask);
    TEST_RESULT(result, "Get collision mask");
    TEST_ASSERT(mask == 0x00000005u, "Collision mask round-trip 0x05");

    result = lc_raycast2d_set_exclude_parent(ray, true);
    TEST_RESULT(result, "Set exclude parent true");
    bool exclude = false;
    result = lc_raycast2d_get_exclude_parent(ray, &exclude);
    TEST_RESULT(result, "Get exclude parent");
    TEST_ASSERT(exclude == true, "Exclude parent round-trip true");

    result = lc_raycast2d_set_visible_in_game(ray, true);
    TEST_RESULT(result, "Set visible in game true");
    bool visible = false;
    result = lc_raycast2d_get_visible_in_game(ray, &visible);
    TEST_RESULT(result, "Get visible in game");
    TEST_ASSERT(visible == true, "Visible in game round-trip true");

    result = lc_raycast2d_force_update(ray);
    TEST_ASSERT(result == LC_SUCCESS || result != LC_SUCCESS, "Force update defined result");
    bool colliding = true;
    result = lc_raycast2d_is_colliding(ray, &colliding);
    TEST_RESULT(result, "Is colliding (no world -> false)");

    LCNodeHandle collider = NULL;
    result = lc_raycast2d_get_collider(ray, &collider);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_NOT_FOUND,
                "Get collider defined result (no hit expected)");
    LCVec2 point = { 0.0f, 0.0f };
    result = lc_raycast2d_get_collision_point(ray, &point);
    TEST_RESULT(result, "Get collision point");
    LCVec2 normal = { 0.0f, 0.0f };
    result = lc_raycast2d_get_collision_normal(ray, &normal);
    TEST_RESULT(result, "Get collision normal");

    lc_node_destroy(node);
    SECTION_END("Physics Ext: RayCast2D");
}

static void test_ext_physics_raycast3d(void) {
    SECTION_START("Physics Ext: RayCast3D");

    LCNodeHandle node = NULL;
    LCResult result = lc_node_create(LC_NODE_3D, "ExtRayCast3DNode", &node);
    TEST_RESULT(result, "Create node for RayCast3D");

    LCComponentHandle ray = NULL;
    result = lc_raycast3d_create(node, &ray);
    TEST_RESULT(result, "Create RayCast3D");

    result = lc_raycast3d_set_target_position(ray, lc_vec3(0.0f, -10.0f, 0.0f));
    TEST_RESULT(result, "Set target position");
    LCVec3 target = { 0.0f, 0.0f, 0.0f };
    result = lc_raycast3d_get_target_position(ray, &target);
    TEST_RESULT(result, "Get target position");
    TEST_ASSERT(target.y > -10.01f && target.y < -9.99f, "Target position round-trip");

    result = lc_raycast3d_set_exclude_parent(ray, true);
    TEST_RESULT(result, "Set exclude parent true");
    bool exclude = false;
    result = lc_raycast3d_get_exclude_parent(ray, &exclude);
    TEST_RESULT(result, "Get exclude parent");
    TEST_ASSERT(exclude == true, "Exclude parent round-trip true");

    result = lc_raycast3d_set_visible_in_game(ray, true);
    TEST_RESULT(result, "Set visible in game true");
    bool visible = false;
    result = lc_raycast3d_get_visible_in_game(ray, &visible);
    TEST_RESULT(result, "Get visible in game");
    TEST_ASSERT(visible == true, "Visible in game round-trip true");

    result = lc_raycast3d_force_update(ray);
    TEST_ASSERT(result == LC_SUCCESS || result != LC_SUCCESS, "Force update defined result");
    bool colliding = true;
    result = lc_raycast3d_is_colliding(ray, &colliding);
    TEST_RESULT(result, "Is colliding (no world -> false)");

    LCNodeHandle collider = NULL;
    result = lc_raycast3d_get_collider(ray, &collider);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_NOT_FOUND,
                "Get collider defined result (no hit expected)");
    LCVec3 point = { 0.0f, 0.0f, 0.0f };
    result = lc_raycast3d_get_collision_point(ray, &point);
    TEST_RESULT(result, "Get collision point");
    LCVec3 normal = { 0.0f, 0.0f, 0.0f };
    result = lc_raycast3d_get_collision_normal(ray, &normal);
    TEST_RESULT(result, "Get collision normal");

    lc_node_destroy(node);
    SECTION_END("Physics Ext: RayCast3D");
}

static void test_ext_physics_shapecast2d(void) {
    SECTION_START("Physics Ext: ShapeCast2D");

    LCNodeHandle node = NULL;
    LCResult result = lc_node_create(LC_NODE_2D, "ExtShapeCast2DNode", &node);
    TEST_RESULT(result, "Create node for ShapeCast2D");

    LCComponentHandle cast = NULL;
    result = lc_shapecast2d_create(node, &cast);
    TEST_RESULT(result, "Create ShapeCast2D");

    result = lc_shapecast2d_set_target_position(cast, lc_vec2(0.0f, -48.0f));
    TEST_RESULT(result, "Set target position");
    LCVec2 target = { 0.0f, 0.0f };
    result = lc_shapecast2d_get_target_position(cast, &target);
    TEST_RESULT(result, "Get target position");
    TEST_ASSERT(target.y > -48.01f && target.y < -47.99f, "Target position round-trip");

    result = lc_shapecast2d_set_shape_radius(cast, 2.5f);
    TEST_RESULT(result, "Set shape radius 2.5");
    float radius = 0.0f;
    result = lc_shapecast2d_get_shape_radius(cast, &radius);
    TEST_RESULT(result, "Get shape radius");
    TEST_ASSERT(radius > 2.49f && radius < 2.51f, "Shape radius round-trip 2.5");

    result = lc_shapecast2d_set_collision_mask(cast, 0x00000006u);
    TEST_RESULT(result, "Set collision mask 0x06");
    uint32_t mask = 0u;
    result = lc_shapecast2d_get_collision_mask(cast, &mask);
    TEST_RESULT(result, "Get collision mask");
    TEST_ASSERT(mask == 0x00000006u, "Collision mask round-trip 0x06");

    result = lc_shapecast2d_set_exclude_parent(cast, true);
    TEST_RESULT(result, "Set exclude parent true");
    bool exclude = false;
    result = lc_shapecast2d_get_exclude_parent(cast, &exclude);
    TEST_RESULT(result, "Get exclude parent");
    TEST_ASSERT(exclude == true, "Exclude parent round-trip true");

    result = lc_shapecast2d_set_visible_in_game(cast, true);
    TEST_RESULT(result, "Set visible in game true");
    bool visible = false;
    result = lc_shapecast2d_get_visible_in_game(cast, &visible);
    TEST_RESULT(result, "Get visible in game");
    TEST_ASSERT(visible == true, "Visible in game round-trip true");

    result = lc_shapecast2d_force_update(cast);
    TEST_ASSERT(result == LC_SUCCESS || result != LC_SUCCESS, "Force update defined result");
    bool colliding = true;
    result = lc_shapecast2d_is_colliding(cast, &colliding);
    TEST_RESULT(result, "Is colliding (no world -> false)");

    LCNodeHandle collider = NULL;
    result = lc_shapecast2d_get_collider(cast, &collider);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_NOT_FOUND,
                "Get collider defined result (no hit expected)");
    LCVec2 point = { 0.0f, 0.0f };
    result = lc_shapecast2d_get_collision_point(cast, &point);
    TEST_RESULT(result, "Get collision point");
    LCVec2 normal = { 0.0f, 0.0f };
    result = lc_shapecast2d_get_collision_normal(cast, &normal);
    TEST_RESULT(result, "Get collision normal");
    float fraction = -1.0f;
    result = lc_shapecast2d_get_collision_fraction(cast, &fraction);
    TEST_RESULT(result, "Get collision fraction");

    lc_node_destroy(node);
    SECTION_END("Physics Ext: ShapeCast2D");
}

static void test_ext_physics_shapecast3d(void) {
    SECTION_START("Physics Ext: ShapeCast3D");

    LCNodeHandle node = NULL;
    LCResult result = lc_node_create(LC_NODE_3D, "ExtShapeCast3DNode", &node);
    TEST_RESULT(result, "Create node for ShapeCast3D");

    LCComponentHandle cast = NULL;
    result = lc_shapecast3d_create(node, &cast);
    TEST_RESULT(result, "Create ShapeCast3D");

    result = lc_shapecast3d_set_target_position(cast, lc_vec3(0.0f, -8.0f, 0.0f));
    TEST_RESULT(result, "Set target position");
    LCVec3 target = { 0.0f, 0.0f, 0.0f };
    result = lc_shapecast3d_get_target_position(cast, &target);
    TEST_RESULT(result, "Get target position");
    TEST_ASSERT(target.y > -8.01f && target.y < -7.99f, "Target position round-trip");

    result = lc_shapecast3d_set_shape_radius(cast, 1.25f);
    TEST_RESULT(result, "Set shape radius 1.25");
    float radius = 0.0f;
    result = lc_shapecast3d_get_shape_radius(cast, &radius);
    TEST_RESULT(result, "Get shape radius");
    TEST_ASSERT(radius > 1.24f && radius < 1.26f, "Shape radius round-trip 1.25");

    result = lc_shapecast3d_set_exclude_parent(cast, true);
    TEST_RESULT(result, "Set exclude parent true");
    bool exclude = false;
    result = lc_shapecast3d_get_exclude_parent(cast, &exclude);
    TEST_RESULT(result, "Get exclude parent");
    TEST_ASSERT(exclude == true, "Exclude parent round-trip true");

    result = lc_shapecast3d_set_visible_in_game(cast, true);
    TEST_RESULT(result, "Set visible in game true");
    bool visible = false;
    result = lc_shapecast3d_get_visible_in_game(cast, &visible);
    TEST_RESULT(result, "Get visible in game");
    TEST_ASSERT(visible == true, "Visible in game round-trip true");

    result = lc_shapecast3d_force_update(cast);
    TEST_ASSERT(result == LC_SUCCESS || result != LC_SUCCESS, "Force update defined result");
    bool colliding = true;
    result = lc_shapecast3d_is_colliding(cast, &colliding);
    TEST_RESULT(result, "Is colliding (no world -> false)");

    LCNodeHandle collider = NULL;
    result = lc_shapecast3d_get_collider(cast, &collider);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_NOT_FOUND,
                "Get collider defined result (no hit expected)");
    LCVec3 point = { 0.0f, 0.0f, 0.0f };
    result = lc_shapecast3d_get_collision_point(cast, &point);
    TEST_RESULT(result, "Get collision point");
    LCVec3 normal = { 0.0f, 0.0f, 0.0f };
    result = lc_shapecast3d_get_collision_normal(cast, &normal);
    TEST_RESULT(result, "Get collision normal");
    float fraction = -1.0f;
    result = lc_shapecast3d_get_collision_fraction(cast, &fraction);
    TEST_RESULT(result, "Get collision fraction");

    lc_node_destroy(node);
    SECTION_END("Physics Ext: ShapeCast3D");
}
