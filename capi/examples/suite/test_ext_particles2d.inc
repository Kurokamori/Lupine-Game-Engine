static void test_ext_particles2d(void) {
    SECTION_START("Ext: Particles2D Emitter");

    LCNodeHandle node = NULL;
    LCResult result = lc_node_create(LC_NODE_2D, "ExtParticles2DNode", &node);
    TEST_RESULT(result, "Create 2D node for Particles2D");

    LCComponentHandle particles = NULL;
    result = lc_particles2d_create(node, &particles);
    TEST_RESULT(result, "Create Particles2D component");

    if (particles != NULL) {
        /* Emission */
        result = lc_particles2d_set_emitting(particles, false);
        TEST_RESULT(result, "lc_particles2d_set_emitting()");
        bool emitting = true;
        result = lc_particles2d_get_emitting(particles, &emitting);
        TEST_RESULT(result, "lc_particles2d_get_emitting()");
        TEST_ASSERT(emitting == false, "emitting round-trips");

        result = lc_particles2d_set_amount(particles, 128);
        TEST_RESULT(result, "lc_particles2d_set_amount()");
        int32_t amount = 0;
        result = lc_particles2d_get_amount(particles, &amount);
        TEST_RESULT(result, "lc_particles2d_get_amount()");
        TEST_ASSERT(amount == 128, "amount round-trips");

        result = lc_particles2d_set_one_shot(particles, true);
        TEST_RESULT(result, "lc_particles2d_set_one_shot()");
        bool oneShot = false;
        result = lc_particles2d_get_one_shot(particles, &oneShot);
        TEST_RESULT(result, "lc_particles2d_get_one_shot()");
        TEST_ASSERT(oneShot == true, "one_shot round-trips");

        result = lc_particles2d_set_explosiveness(particles, 0.5f);
        TEST_RESULT(result, "lc_particles2d_set_explosiveness()");
        float explosiveness = 0.0f;
        result = lc_particles2d_get_explosiveness(particles, &explosiveness);
        TEST_RESULT(result, "lc_particles2d_get_explosiveness()");
        TEST_ASSERT(explosiveness > 0.49f && explosiveness < 0.51f, "explosiveness round-trips");

        result = lc_particles2d_set_speed_scale(particles, 2.0f);
        TEST_RESULT(result, "lc_particles2d_set_speed_scale()");
        float speedScale = 0.0f;
        result = lc_particles2d_get_speed_scale(particles, &speedScale);
        TEST_RESULT(result, "lc_particles2d_get_speed_scale()");
        TEST_ASSERT(speedScale > 1.99f && speedScale < 2.01f, "speed_scale round-trips");

        result = lc_particles2d_set_preprocess(particles, 1.5f);
        TEST_RESULT(result, "lc_particles2d_set_preprocess()");
        float preprocess = 0.0f;
        result = lc_particles2d_get_preprocess(particles, &preprocess);
        TEST_RESULT(result, "lc_particles2d_get_preprocess()");
        TEST_ASSERT(preprocess > 1.49f && preprocess < 1.51f, "preprocess round-trips");

        result = lc_particles2d_set_local_space(particles, true);
        TEST_RESULT(result, "lc_particles2d_set_local_space()");
        bool localSpace = false;
        result = lc_particles2d_get_local_space(particles, &localSpace);
        TEST_RESULT(result, "lc_particles2d_get_local_space()");
        TEST_ASSERT(localSpace == true, "local_space round-trips");

        /* Lifetime */
        result = lc_particles2d_set_lifetime(particles, 3.0f);
        TEST_RESULT(result, "lc_particles2d_set_lifetime()");
        float lifetime = 0.0f;
        result = lc_particles2d_get_lifetime(particles, &lifetime);
        TEST_RESULT(result, "lc_particles2d_get_lifetime()");
        TEST_ASSERT(lifetime > 2.99f && lifetime < 3.01f, "lifetime round-trips");

        result = lc_particles2d_set_lifetime_randomness(particles, 0.25f);
        TEST_RESULT(result, "lc_particles2d_set_lifetime_randomness()");
        float lifeRand = 0.0f;
        result = lc_particles2d_get_lifetime_randomness(particles, &lifeRand);
        TEST_RESULT(result, "lc_particles2d_get_lifetime_randomness()");
        TEST_ASSERT(lifeRand > 0.24f && lifeRand < 0.26f, "lifetime_randomness round-trips");

        /* Emission shape */
        result = lc_particles2d_set_emission_shape(particles, LC_PARTICLES2D_SHAPE_CIRCLE);
        TEST_RESULT(result, "lc_particles2d_set_emission_shape()");
        LCParticles2DEmissionShape shape = LC_PARTICLES2D_SHAPE_POINT;
        result = lc_particles2d_get_emission_shape(particles, &shape);
        TEST_RESULT(result, "lc_particles2d_get_emission_shape()");
        TEST_ASSERT(shape == LC_PARTICLES2D_SHAPE_CIRCLE, "emission_shape round-trips");

        result = lc_particles2d_set_emission_radius(particles, 64.0f);
        TEST_RESULT(result, "lc_particles2d_set_emission_radius()");
        float radius = 0.0f;
        result = lc_particles2d_get_emission_radius(particles, &radius);
        TEST_RESULT(result, "lc_particles2d_get_emission_radius()");
        TEST_ASSERT(radius > 63.9f && radius < 64.1f, "emission_radius round-trips");

        LCVec2 setExtents = lc_vec2(32.0f, 48.0f);
        result = lc_particles2d_set_emission_extents(particles, setExtents);
        TEST_RESULT(result, "lc_particles2d_set_emission_extents()");
        LCVec2 gotExtents = lc_vec2(0.0f, 0.0f);
        result = lc_particles2d_get_emission_extents(particles, &gotExtents);
        TEST_RESULT(result, "lc_particles2d_get_emission_extents()");
        TEST_ASSERT(gotExtents.x == setExtents.x && gotExtents.y == setExtents.y, "emission_extents round-trips");

        /* Velocity */
        LCVec2 setDir = lc_vec2(1.0f, 0.0f);
        result = lc_particles2d_set_direction(particles, setDir);
        TEST_RESULT(result, "lc_particles2d_set_direction()");
        LCVec2 gotDir = lc_vec2(0.0f, 0.0f);
        result = lc_particles2d_get_direction(particles, &gotDir);
        TEST_RESULT(result, "lc_particles2d_get_direction()");
        TEST_ASSERT(gotDir.x == setDir.x && gotDir.y == setDir.y, "direction round-trips");

        result = lc_particles2d_set_spread(particles, 30.0f);
        TEST_RESULT(result, "lc_particles2d_set_spread()");
        float spread = 0.0f;
        result = lc_particles2d_get_spread(particles, &spread);
        TEST_RESULT(result, "lc_particles2d_get_spread()");
        TEST_ASSERT(spread > 29.9f && spread < 30.1f, "spread round-trips");

        result = lc_particles2d_set_initial_velocity_min(particles, 10.0f);
        TEST_RESULT(result, "lc_particles2d_set_initial_velocity_min()");
        result = lc_particles2d_set_initial_velocity_max(particles, 200.0f);
        TEST_RESULT(result, "lc_particles2d_set_initial_velocity_max()");
        float velMin = 0.0f, velMax = 0.0f;
        result = lc_particles2d_get_initial_velocity_min(particles, &velMin);
        TEST_RESULT(result, "lc_particles2d_get_initial_velocity_min()");
        result = lc_particles2d_get_initial_velocity_max(particles, &velMax);
        TEST_RESULT(result, "lc_particles2d_get_initial_velocity_max()");
        TEST_ASSERT(velMin > 9.9f && velMin < 10.1f, "initial_velocity_min round-trips");
        TEST_ASSERT(velMax > 199.9f && velMax < 200.1f, "initial_velocity_max round-trips");

        LCVec2 setGravity = lc_vec2(0.0f, -98.0f);
        result = lc_particles2d_set_gravity(particles, setGravity);
        TEST_RESULT(result, "lc_particles2d_set_gravity()");
        LCVec2 gotGravity = lc_vec2(0.0f, 0.0f);
        result = lc_particles2d_get_gravity(particles, &gotGravity);
        TEST_RESULT(result, "lc_particles2d_get_gravity()");
        TEST_ASSERT(gotGravity.x == setGravity.x && gotGravity.y == setGravity.y, "gravity round-trips");

        result = lc_particles2d_set_linear_damping(particles, 0.5f);
        TEST_RESULT(result, "lc_particles2d_set_linear_damping()");
        float damping = 0.0f;
        result = lc_particles2d_get_linear_damping(particles, &damping);
        TEST_RESULT(result, "lc_particles2d_get_linear_damping()");
        TEST_ASSERT(damping > 0.49f && damping < 0.51f, "linear_damping round-trips");

        /* Rotation */
        result = lc_particles2d_set_initial_angle_min(particles, -45.0f);
        TEST_RESULT(result, "lc_particles2d_set_initial_angle_min()");
        result = lc_particles2d_set_initial_angle_max(particles, 45.0f);
        TEST_RESULT(result, "lc_particles2d_set_initial_angle_max()");
        result = lc_particles2d_set_angular_velocity_min(particles, -90.0f);
        TEST_RESULT(result, "lc_particles2d_set_angular_velocity_min()");
        result = lc_particles2d_set_angular_velocity_max(particles, 90.0f);
        TEST_RESULT(result, "lc_particles2d_set_angular_velocity_max()");
        float angleMin = 0.0f, angleMax = 0.0f, angVelMin = 0.0f, angVelMax = 0.0f;
        result = lc_particles2d_get_initial_angle_min(particles, &angleMin);
        TEST_RESULT(result, "lc_particles2d_get_initial_angle_min()");
        result = lc_particles2d_get_initial_angle_max(particles, &angleMax);
        TEST_RESULT(result, "lc_particles2d_get_initial_angle_max()");
        result = lc_particles2d_get_angular_velocity_min(particles, &angVelMin);
        TEST_RESULT(result, "lc_particles2d_get_angular_velocity_min()");
        result = lc_particles2d_get_angular_velocity_max(particles, &angVelMax);
        TEST_RESULT(result, "lc_particles2d_get_angular_velocity_max()");
        TEST_ASSERT(angleMin < -44.9f && angleMin > -45.1f, "initial_angle_min round-trips");
        TEST_ASSERT(angleMax > 44.9f && angleMax < 45.1f, "initial_angle_max round-trips");
        TEST_ASSERT(angVelMin < -89.9f && angVelMin > -90.1f, "angular_velocity_min round-trips");
        TEST_ASSERT(angVelMax > 89.9f && angVelMax < 90.1f, "angular_velocity_max round-trips");

        /* Display */
        LCVec2 setSize = lc_vec2(16.0f, 24.0f);
        result = lc_particles2d_set_particle_size(particles, setSize);
        TEST_RESULT(result, "lc_particles2d_set_particle_size()");
        LCVec2 gotSize = lc_vec2(0.0f, 0.0f);
        result = lc_particles2d_get_particle_size(particles, &gotSize);
        TEST_RESULT(result, "lc_particles2d_get_particle_size()");
        TEST_ASSERT(gotSize.x == setSize.x && gotSize.y == setSize.y, "particle_size round-trips");

        result = lc_particles2d_set_scale_min(particles, 0.5f);
        TEST_RESULT(result, "lc_particles2d_set_scale_min()");
        result = lc_particles2d_set_scale_max(particles, 1.5f);
        TEST_RESULT(result, "lc_particles2d_set_scale_max()");
        result = lc_particles2d_set_scale_end(particles, 0.0f);
        TEST_RESULT(result, "lc_particles2d_set_scale_end()");
        float scaleMin = 0.0f, scaleMax = 0.0f, scaleEnd = 9.0f;
        result = lc_particles2d_get_scale_min(particles, &scaleMin);
        TEST_RESULT(result, "lc_particles2d_get_scale_min()");
        result = lc_particles2d_get_scale_max(particles, &scaleMax);
        TEST_RESULT(result, "lc_particles2d_get_scale_max()");
        result = lc_particles2d_get_scale_end(particles, &scaleEnd);
        TEST_RESULT(result, "lc_particles2d_get_scale_end()");
        TEST_ASSERT(scaleMin > 0.49f && scaleMin < 0.51f, "scale_min round-trips");
        TEST_ASSERT(scaleMax > 1.49f && scaleMax < 1.51f, "scale_max round-trips");
        TEST_ASSERT(scaleEnd >= 0.0f && scaleEnd < 0.01f, "scale_end round-trips");

        result = lc_particles2d_set_texture_path(particles, "res://textures/spark.png");
        TEST_RESULT(result, "lc_particles2d_set_texture_path()");
        char texPath[256];
        texPath[0] = '\0';
        uint32_t texLen = 0;
        result = lc_particles2d_get_texture_path(particles, texPath, sizeof(texPath), &texLen);
        TEST_RESULT(result, "lc_particles2d_get_texture_path()");
        TEST_ASSERT(strcmp(texPath, "res://textures/spark.png") == 0, "texture_path round-trips");

        LCColor setStart = lc_color(1.0f, 0.5f, 0.25f, 1.0f);
        result = lc_particles2d_set_color_start(particles, setStart);
        TEST_RESULT(result, "lc_particles2d_set_color_start()");
        LCColor gotStart = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
        result = lc_particles2d_get_color_start(particles, &gotStart);
        TEST_RESULT(result, "lc_particles2d_get_color_start()");
        TEST_ASSERT(gotStart.r == setStart.r && gotStart.a == setStart.a, "color_start round-trips");

        LCColor setEnd = lc_color(0.0f, 0.0f, 1.0f, 0.0f);
        result = lc_particles2d_set_color_end(particles, setEnd);
        TEST_RESULT(result, "lc_particles2d_set_color_end()");
        LCColor gotEnd = lc_color(1.0f, 1.0f, 1.0f, 1.0f);
        result = lc_particles2d_get_color_end(particles, &gotEnd);
        TEST_RESULT(result, "lc_particles2d_get_color_end()");
        TEST_ASSERT(gotEnd.b == setEnd.b && gotEnd.a == setEnd.a, "color_end round-trips");

        LCColor setMod = lc_color(0.8f, 0.8f, 0.8f, 1.0f);
        result = lc_particles2d_set_modulate(particles, setMod);
        TEST_RESULT(result, "lc_particles2d_set_modulate()");
        LCColor gotMod = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
        result = lc_particles2d_get_modulate(particles, &gotMod);
        TEST_RESULT(result, "lc_particles2d_get_modulate()");
        TEST_ASSERT(gotMod.r == setMod.r, "modulate round-trips");

        result = lc_particles2d_set_particle_shape(particles, LC_PARTICLES2D_PARTICLE_CIRCLE);
        TEST_RESULT(result, "lc_particles2d_set_particle_shape()");
        LCParticles2DParticleShape pshape = LC_PARTICLES2D_PARTICLE_SQUARE;
        result = lc_particles2d_get_particle_shape(particles, &pshape);
        TEST_RESULT(result, "lc_particles2d_get_particle_shape()");
        TEST_ASSERT(pshape == LC_PARTICLES2D_PARTICLE_CIRCLE, "particle_shape round-trips");

        const char* grad_json = "{\"stops\":[{\"pos\":0.0,\"color\":{\"r\":1.0,\"g\":0.0,\"b\":0.0,\"a\":1.0}},"
                                "{\"pos\":1.0,\"color\":{\"r\":0.0,\"g\":0.0,\"b\":1.0,\"a\":0.0}}]}";
        result = lc_particles2d_set_color_gradient(particles, grad_json);
        TEST_RESULT(result, "lc_particles2d_set_color_gradient()");
        char grad_out[512];
        grad_out[0] = '\0';
        uint32_t grad_len = 0;
        result = lc_particles2d_get_color_gradient(particles, grad_out, sizeof(grad_out), &grad_len);
        TEST_RESULT(result, "lc_particles2d_get_color_gradient()");
        TEST_ASSERT(grad_len > 0 && strstr(grad_out, "stops") != NULL, "color_gradient round-trips");

        const char* curve_json = "{\"points\":[{\"pos\":0.0,\"value\":0.0},{\"pos\":0.5,\"value\":1.0},"
                                 "{\"pos\":1.0,\"value\":0.0}]}";
        result = lc_particles2d_set_scale_curve(particles, curve_json);
        TEST_RESULT(result, "lc_particles2d_set_scale_curve()");
        char curve_out[512];
        curve_out[0] = '\0';
        uint32_t curve_len = 0;
        result = lc_particles2d_get_scale_curve(particles, curve_out, sizeof(curve_out), &curve_len);
        TEST_RESULT(result, "lc_particles2d_get_scale_curve()");
        TEST_ASSERT(curve_len > 0 && strstr(curve_out, "points") != NULL, "scale_curve round-trips");

        /* Runtime control */
        result = lc_particles2d_emit_burst(particles, 16);
        TEST_RESULT(result, "lc_particles2d_emit_burst()");
        int32_t aliveCount = -1;
        result = lc_particles2d_get_alive_count(particles, &aliveCount);
        TEST_RESULT(result, "lc_particles2d_get_alive_count()");
        TEST_ASSERT(aliveCount == 16, "emit_burst spawns the requested count");

        result = lc_particles2d_restart(particles);
        TEST_RESULT(result, "lc_particles2d_restart()");
        result = lc_particles2d_get_alive_count(particles, &aliveCount);
        TEST_RESULT(result, "lc_particles2d_get_alive_count() after restart");
        TEST_ASSERT(aliveCount == 0, "restart clears live particles");

        /* Null-handle robustness */
        result = lc_particles2d_get_amount(NULL, &amount);
        TEST_ASSERT(result != LC_SUCCESS, "lc_particles2d_get_amount(NULL) fails gracefully");
        result = lc_particles2d_get_emitting(particles, NULL);
        TEST_ASSERT(result != LC_SUCCESS, "lc_particles2d_get_emitting(NULL out) fails gracefully");

        result = lc_component_destroy(particles);
        TEST_RESULT(result, "Destroy Particles2D");
    }

    if (node != NULL) {
        lc_node_destroy(node);
    }

    SECTION_END("Ext: Particles2D Emitter");
}
