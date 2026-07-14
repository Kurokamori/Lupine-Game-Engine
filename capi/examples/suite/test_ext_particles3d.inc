static void test_ext_particles3d(void) {
    SECTION_START("Ext: Particles3D Emitter");

    LCNodeHandle node = NULL;
    LCResult result = lc_node_create(LC_NODE_3D, "ExtParticles3DNode", &node);
    TEST_RESULT(result, "Create 3D node for Particles3D");

    LCComponentHandle particles = NULL;
    result = lc_particles3d_create(node, &particles);
    TEST_RESULT(result, "Create Particles3D component");

    if (particles != NULL) {
        /* Emission */
        result = lc_particles3d_set_emitting(particles, false);
        TEST_RESULT(result, "lc_particles3d_set_emitting()");
        bool emitting = true;
        result = lc_particles3d_get_emitting(particles, &emitting);
        TEST_RESULT(result, "lc_particles3d_get_emitting()");
        TEST_ASSERT(emitting == false, "emitting round-trips");

        result = lc_particles3d_set_amount(particles, 256);
        TEST_RESULT(result, "lc_particles3d_set_amount()");
        int32_t amount = 0;
        result = lc_particles3d_get_amount(particles, &amount);
        TEST_RESULT(result, "lc_particles3d_get_amount()");
        TEST_ASSERT(amount == 256, "amount round-trips");

        result = lc_particles3d_set_one_shot(particles, true);
        TEST_RESULT(result, "lc_particles3d_set_one_shot()");
        bool oneShot = false;
        result = lc_particles3d_get_one_shot(particles, &oneShot);
        TEST_RESULT(result, "lc_particles3d_get_one_shot()");
        TEST_ASSERT(oneShot == true, "one_shot round-trips");

        result = lc_particles3d_set_explosiveness(particles, 0.75f);
        TEST_RESULT(result, "lc_particles3d_set_explosiveness()");
        float explosiveness = 0.0f;
        result = lc_particles3d_get_explosiveness(particles, &explosiveness);
        TEST_RESULT(result, "lc_particles3d_get_explosiveness()");
        TEST_ASSERT(explosiveness > 0.74f && explosiveness < 0.76f, "explosiveness round-trips");

        result = lc_particles3d_set_speed_scale(particles, 1.5f);
        TEST_RESULT(result, "lc_particles3d_set_speed_scale()");
        float speedScale = 0.0f;
        result = lc_particles3d_get_speed_scale(particles, &speedScale);
        TEST_RESULT(result, "lc_particles3d_get_speed_scale()");
        TEST_ASSERT(speedScale > 1.49f && speedScale < 1.51f, "speed_scale round-trips");

        result = lc_particles3d_set_preprocess(particles, 2.0f);
        TEST_RESULT(result, "lc_particles3d_set_preprocess()");
        float preprocess = 0.0f;
        result = lc_particles3d_get_preprocess(particles, &preprocess);
        TEST_RESULT(result, "lc_particles3d_get_preprocess()");
        TEST_ASSERT(preprocess > 1.99f && preprocess < 2.01f, "preprocess round-trips");

        result = lc_particles3d_set_local_space(particles, true);
        TEST_RESULT(result, "lc_particles3d_set_local_space()");
        bool localSpace = false;
        result = lc_particles3d_get_local_space(particles, &localSpace);
        TEST_RESULT(result, "lc_particles3d_get_local_space()");
        TEST_ASSERT(localSpace == true, "local_space round-trips");

        /* Lifetime */
        result = lc_particles3d_set_lifetime(particles, 4.0f);
        TEST_RESULT(result, "lc_particles3d_set_lifetime()");
        float lifetime = 0.0f;
        result = lc_particles3d_get_lifetime(particles, &lifetime);
        TEST_RESULT(result, "lc_particles3d_get_lifetime()");
        TEST_ASSERT(lifetime > 3.99f && lifetime < 4.01f, "lifetime round-trips");

        result = lc_particles3d_set_lifetime_randomness(particles, 0.3f);
        TEST_RESULT(result, "lc_particles3d_set_lifetime_randomness()");
        float lifeRand = 0.0f;
        result = lc_particles3d_get_lifetime_randomness(particles, &lifeRand);
        TEST_RESULT(result, "lc_particles3d_get_lifetime_randomness()");
        TEST_ASSERT(lifeRand > 0.29f && lifeRand < 0.31f, "lifetime_randomness round-trips");

        /* Emission shape */
        result = lc_particles3d_set_emission_shape(particles, LC_PARTICLES3D_SHAPE_SPHERE);
        TEST_RESULT(result, "lc_particles3d_set_emission_shape()");
        LCParticles3DEmissionShape shape = LC_PARTICLES3D_SHAPE_POINT;
        result = lc_particles3d_get_emission_shape(particles, &shape);
        TEST_RESULT(result, "lc_particles3d_get_emission_shape()");
        TEST_ASSERT(shape == LC_PARTICLES3D_SHAPE_SPHERE, "emission_shape round-trips");

        result = lc_particles3d_set_emission_radius(particles, 3.5f);
        TEST_RESULT(result, "lc_particles3d_set_emission_radius()");
        float radius = 0.0f;
        result = lc_particles3d_get_emission_radius(particles, &radius);
        TEST_RESULT(result, "lc_particles3d_get_emission_radius()");
        TEST_ASSERT(radius > 3.49f && radius < 3.51f, "emission_radius round-trips");

        LCVec3 setExtents = lc_vec3(1.0f, 2.0f, 3.0f);
        result = lc_particles3d_set_emission_extents(particles, setExtents);
        TEST_RESULT(result, "lc_particles3d_set_emission_extents()");
        LCVec3 gotExtents = lc_vec3(0.0f, 0.0f, 0.0f);
        result = lc_particles3d_get_emission_extents(particles, &gotExtents);
        TEST_RESULT(result, "lc_particles3d_get_emission_extents()");
        TEST_ASSERT(gotExtents.x == setExtents.x && gotExtents.y == setExtents.y && gotExtents.z == setExtents.z,
                    "emission_extents round-trips");

        /* Velocity */
        LCVec3 setDir = lc_vec3(0.0f, 0.0f, 1.0f);
        result = lc_particles3d_set_direction(particles, setDir);
        TEST_RESULT(result, "lc_particles3d_set_direction()");
        LCVec3 gotDir = lc_vec3(0.0f, 0.0f, 0.0f);
        result = lc_particles3d_get_direction(particles, &gotDir);
        TEST_RESULT(result, "lc_particles3d_get_direction()");
        TEST_ASSERT(gotDir.x == setDir.x && gotDir.y == setDir.y && gotDir.z == setDir.z, "direction round-trips");

        result = lc_particles3d_set_spread(particles, 15.0f);
        TEST_RESULT(result, "lc_particles3d_set_spread()");
        float spread = 0.0f;
        result = lc_particles3d_get_spread(particles, &spread);
        TEST_RESULT(result, "lc_particles3d_get_spread()");
        TEST_ASSERT(spread > 14.9f && spread < 15.1f, "spread round-trips");

        result = lc_particles3d_set_initial_velocity_min(particles, 1.0f);
        TEST_RESULT(result, "lc_particles3d_set_initial_velocity_min()");
        result = lc_particles3d_set_initial_velocity_max(particles, 5.0f);
        TEST_RESULT(result, "lc_particles3d_set_initial_velocity_max()");
        float velMin = 0.0f, velMax = 0.0f;
        result = lc_particles3d_get_initial_velocity_min(particles, &velMin);
        TEST_RESULT(result, "lc_particles3d_get_initial_velocity_min()");
        result = lc_particles3d_get_initial_velocity_max(particles, &velMax);
        TEST_RESULT(result, "lc_particles3d_get_initial_velocity_max()");
        TEST_ASSERT(velMin > 0.99f && velMin < 1.01f, "initial_velocity_min round-trips");
        TEST_ASSERT(velMax > 4.99f && velMax < 5.01f, "initial_velocity_max round-trips");

        LCVec3 setGravity = lc_vec3(0.0f, -19.6f, 0.0f);
        result = lc_particles3d_set_gravity(particles, setGravity);
        TEST_RESULT(result, "lc_particles3d_set_gravity()");
        LCVec3 gotGravity = lc_vec3(0.0f, 0.0f, 0.0f);
        result = lc_particles3d_get_gravity(particles, &gotGravity);
        TEST_RESULT(result, "lc_particles3d_get_gravity()");
        TEST_ASSERT(gotGravity.y == setGravity.y, "gravity round-trips");

        result = lc_particles3d_set_linear_damping(particles, 0.2f);
        TEST_RESULT(result, "lc_particles3d_set_linear_damping()");
        float damping = 0.0f;
        result = lc_particles3d_get_linear_damping(particles, &damping);
        TEST_RESULT(result, "lc_particles3d_get_linear_damping()");
        TEST_ASSERT(damping > 0.19f && damping < 0.21f, "linear_damping round-trips");

        /* Rotation */
        result = lc_particles3d_set_initial_angle_min(particles, -30.0f);
        TEST_RESULT(result, "lc_particles3d_set_initial_angle_min()");
        result = lc_particles3d_set_initial_angle_max(particles, 30.0f);
        TEST_RESULT(result, "lc_particles3d_set_initial_angle_max()");
        result = lc_particles3d_set_angular_velocity_min(particles, -60.0f);
        TEST_RESULT(result, "lc_particles3d_set_angular_velocity_min()");
        result = lc_particles3d_set_angular_velocity_max(particles, 60.0f);
        TEST_RESULT(result, "lc_particles3d_set_angular_velocity_max()");
        float angleMin = 0.0f, angleMax = 0.0f, angVelMin = 0.0f, angVelMax = 0.0f;
        result = lc_particles3d_get_initial_angle_min(particles, &angleMin);
        TEST_RESULT(result, "lc_particles3d_get_initial_angle_min()");
        result = lc_particles3d_get_initial_angle_max(particles, &angleMax);
        TEST_RESULT(result, "lc_particles3d_get_initial_angle_max()");
        result = lc_particles3d_get_angular_velocity_min(particles, &angVelMin);
        TEST_RESULT(result, "lc_particles3d_get_angular_velocity_min()");
        result = lc_particles3d_get_angular_velocity_max(particles, &angVelMax);
        TEST_RESULT(result, "lc_particles3d_get_angular_velocity_max()");
        TEST_ASSERT(angleMin < -29.9f && angleMin > -30.1f, "initial_angle_min round-trips");
        TEST_ASSERT(angleMax > 29.9f && angleMax < 30.1f, "initial_angle_max round-trips");
        TEST_ASSERT(angVelMin < -59.9f && angVelMin > -60.1f, "angular_velocity_min round-trips");
        TEST_ASSERT(angVelMax > 59.9f && angVelMax < 60.1f, "angular_velocity_max round-trips");

        /* Display */
        result = lc_particles3d_set_billboard_mode(particles, LC_PARTICLES3D_BILLBOARD_Y_AXIS);
        TEST_RESULT(result, "lc_particles3d_set_billboard_mode()");
        LCParticles3DBillboardMode billboard = LC_PARTICLES3D_BILLBOARD_DISABLED;
        result = lc_particles3d_get_billboard_mode(particles, &billboard);
        TEST_RESULT(result, "lc_particles3d_get_billboard_mode()");
        TEST_ASSERT(billboard == LC_PARTICLES3D_BILLBOARD_Y_AXIS, "billboard_mode round-trips");

        result = lc_particles3d_set_blend_mode(particles, LC_PARTICLES3D_BLEND_ADDITIVE);
        TEST_RESULT(result, "lc_particles3d_set_blend_mode()");
        LCParticles3DBlendMode blend = LC_PARTICLES3D_BLEND_ALPHA;
        result = lc_particles3d_get_blend_mode(particles, &blend);
        TEST_RESULT(result, "lc_particles3d_get_blend_mode()");
        TEST_ASSERT(blend == LC_PARTICLES3D_BLEND_ADDITIVE, "blend_mode round-trips");

        result = lc_particles3d_set_double_sided(particles, false);
        TEST_RESULT(result, "lc_particles3d_set_double_sided()");
        bool doubleSided = true;
        result = lc_particles3d_get_double_sided(particles, &doubleSided);
        TEST_RESULT(result, "lc_particles3d_get_double_sided()");
        TEST_ASSERT(doubleSided == false, "double_sided round-trips");

        LCVec2 setSize = lc_vec2(0.5f, 0.75f);
        result = lc_particles3d_set_particle_size(particles, setSize);
        TEST_RESULT(result, "lc_particles3d_set_particle_size()");
        LCVec2 gotSize = lc_vec2(0.0f, 0.0f);
        result = lc_particles3d_get_particle_size(particles, &gotSize);
        TEST_RESULT(result, "lc_particles3d_get_particle_size()");
        TEST_ASSERT(gotSize.x == setSize.x && gotSize.y == setSize.y, "particle_size round-trips");

        result = lc_particles3d_set_scale_min(particles, 0.5f);
        TEST_RESULT(result, "lc_particles3d_set_scale_min()");
        result = lc_particles3d_set_scale_max(particles, 2.0f);
        TEST_RESULT(result, "lc_particles3d_set_scale_max()");
        result = lc_particles3d_set_scale_end(particles, 0.0f);
        TEST_RESULT(result, "lc_particles3d_set_scale_end()");
        float scaleMin = 0.0f, scaleMax = 0.0f, scaleEnd = 9.0f;
        result = lc_particles3d_get_scale_min(particles, &scaleMin);
        TEST_RESULT(result, "lc_particles3d_get_scale_min()");
        result = lc_particles3d_get_scale_max(particles, &scaleMax);
        TEST_RESULT(result, "lc_particles3d_get_scale_max()");
        result = lc_particles3d_get_scale_end(particles, &scaleEnd);
        TEST_RESULT(result, "lc_particles3d_get_scale_end()");
        TEST_ASSERT(scaleMin > 0.49f && scaleMin < 0.51f, "scale_min round-trips");
        TEST_ASSERT(scaleMax > 1.99f && scaleMax < 2.01f, "scale_max round-trips");
        TEST_ASSERT(scaleEnd >= 0.0f && scaleEnd < 0.01f, "scale_end round-trips");

        result = lc_particles3d_set_texture_path(particles, "res://textures/smoke.png");
        TEST_RESULT(result, "lc_particles3d_set_texture_path()");
        char texPath[256];
        texPath[0] = '\0';
        uint32_t texLen = 0;
        result = lc_particles3d_get_texture_path(particles, texPath, sizeof(texPath), &texLen);
        TEST_RESULT(result, "lc_particles3d_get_texture_path()");
        TEST_ASSERT(strcmp(texPath, "res://textures/smoke.png") == 0, "texture_path round-trips");

        LCColor setStart = lc_color(1.0f, 0.8f, 0.2f, 1.0f);
        result = lc_particles3d_set_color_start(particles, setStart);
        TEST_RESULT(result, "lc_particles3d_set_color_start()");
        LCColor gotStart = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
        result = lc_particles3d_get_color_start(particles, &gotStart);
        TEST_RESULT(result, "lc_particles3d_get_color_start()");
        TEST_ASSERT(gotStart.r == setStart.r && gotStart.a == setStart.a, "color_start round-trips");

        LCColor setEnd = lc_color(0.2f, 0.0f, 0.0f, 0.0f);
        result = lc_particles3d_set_color_end(particles, setEnd);
        TEST_RESULT(result, "lc_particles3d_set_color_end()");
        LCColor gotEnd = lc_color(1.0f, 1.0f, 1.0f, 1.0f);
        result = lc_particles3d_get_color_end(particles, &gotEnd);
        TEST_RESULT(result, "lc_particles3d_get_color_end()");
        TEST_ASSERT(gotEnd.r == setEnd.r && gotEnd.a == setEnd.a, "color_end round-trips");

        LCColor setMod = lc_color(0.9f, 0.9f, 0.9f, 1.0f);
        result = lc_particles3d_set_modulate(particles, setMod);
        TEST_RESULT(result, "lc_particles3d_set_modulate()");
        LCColor gotMod = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
        result = lc_particles3d_get_modulate(particles, &gotMod);
        TEST_RESULT(result, "lc_particles3d_get_modulate()");
        TEST_ASSERT(gotMod.r == setMod.r, "modulate round-trips");

        result = lc_particles3d_set_particle_shape(particles, LC_PARTICLES3D_PARTICLE_CIRCLE);
        TEST_RESULT(result, "lc_particles3d_set_particle_shape()");
        LCParticles3DParticleShape pshape = LC_PARTICLES3D_PARTICLE_SQUARE;
        result = lc_particles3d_get_particle_shape(particles, &pshape);
        TEST_RESULT(result, "lc_particles3d_get_particle_shape()");
        TEST_ASSERT(pshape == LC_PARTICLES3D_PARTICLE_CIRCLE, "particle_shape round-trips");

        const char* grad3d_json = "{\"stops\":[{\"pos\":0.0,\"color\":{\"r\":1.0,\"g\":1.0,\"b\":0.0,\"a\":1.0}},"
                                  "{\"pos\":1.0,\"color\":{\"r\":1.0,\"g\":0.0,\"b\":0.0,\"a\":0.0}}]}";
        result = lc_particles3d_set_color_gradient(particles, grad3d_json);
        TEST_RESULT(result, "lc_particles3d_set_color_gradient()");
        char grad3d_out[512];
        grad3d_out[0] = '\0';
        uint32_t grad3d_len = 0;
        result = lc_particles3d_get_color_gradient(particles, grad3d_out, sizeof(grad3d_out), &grad3d_len);
        TEST_RESULT(result, "lc_particles3d_get_color_gradient()");
        TEST_ASSERT(grad3d_len > 0 && strstr(grad3d_out, "stops") != NULL, "color_gradient round-trips");

        const char* curve3d_json = "{\"points\":[{\"pos\":0.0,\"value\":1.0},{\"pos\":1.0,\"value\":0.0}]}";
        result = lc_particles3d_set_scale_curve(particles, curve3d_json);
        TEST_RESULT(result, "lc_particles3d_set_scale_curve()");
        char curve3d_out[512];
        curve3d_out[0] = '\0';
        uint32_t curve3d_len = 0;
        result = lc_particles3d_get_scale_curve(particles, curve3d_out, sizeof(curve3d_out), &curve3d_len);
        TEST_RESULT(result, "lc_particles3d_get_scale_curve()");
        TEST_ASSERT(curve3d_len > 0 && strstr(curve3d_out, "points") != NULL, "scale_curve round-trips");

        /* Runtime control */
        result = lc_particles3d_emit_burst(particles, 24);
        TEST_RESULT(result, "lc_particles3d_emit_burst()");
        int32_t aliveCount = -1;
        result = lc_particles3d_get_alive_count(particles, &aliveCount);
        TEST_RESULT(result, "lc_particles3d_get_alive_count()");
        TEST_ASSERT(aliveCount == 24, "emit_burst spawns the requested count");

        result = lc_particles3d_restart(particles);
        TEST_RESULT(result, "lc_particles3d_restart()");
        result = lc_particles3d_get_alive_count(particles, &aliveCount);
        TEST_RESULT(result, "lc_particles3d_get_alive_count() after restart");
        TEST_ASSERT(aliveCount == 0, "restart clears live particles");

        /* Null-handle robustness */
        result = lc_particles3d_get_amount(NULL, &amount);
        TEST_ASSERT(result != LC_SUCCESS, "lc_particles3d_get_amount(NULL) fails gracefully");
        result = lc_particles3d_get_blend_mode(particles, NULL);
        TEST_ASSERT(result != LC_SUCCESS, "lc_particles3d_get_blend_mode(NULL out) fails gracefully");

        result = lc_component_destroy(particles);
        TEST_RESULT(result, "Destroy Particles3D");
    }

    if (node != NULL) {
        lc_node_destroy(node);
    }

    SECTION_END("Ext: Particles3D Emitter");
}
