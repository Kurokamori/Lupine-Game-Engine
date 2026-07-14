static void test_ext_lights_directional(void) {
    SECTION_START("Lights Ext: DirectionalLight3D");

    LCComponentHandle light = NULL;
    LCResult result = lc_directional_light3d_create("ExtDirLight", &light);
    TEST_RESULT(result, "Create DirectionalLight3D");

    LCColor set_color = lc_color(0.25f, 0.5f, 0.75f, 1.0f);
    result = lc_directional_light3d_set_color(light, set_color);
    TEST_RESULT(result, "Set directional light color");

    LCColor got_color = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    result = lc_directional_light3d_get_color(light, &got_color);
    TEST_RESULT(result, "Get directional light color");
    TEST_ASSERT(got_color.r == set_color.r, "Directional color.r round-trips");
    TEST_ASSERT(got_color.g == set_color.g, "Directional color.g round-trips");
    TEST_ASSERT(got_color.b == set_color.b, "Directional color.b round-trips");
    TEST_ASSERT(got_color.a == set_color.a, "Directional color.a round-trips");

    result = lc_directional_light3d_set_intensity(light, 2.75f);
    TEST_RESULT(result, "Set directional light intensity");
    float intensity = 0.0f;
    result = lc_directional_light3d_get_intensity(light, &intensity);
    TEST_RESULT(result, "Get directional light intensity");
    TEST_ASSERT(intensity > 2.74f && intensity < 2.76f, "Directional intensity round-trips");

    bool negative = true;
    result = lc_directional_light3d_is_negative(light, &negative);
    TEST_RESULT(result, "Query directional negative (default)");
    bool default_negative = negative;

    result = lc_directional_light3d_set_negative(light, true);
    TEST_RESULT(result, "Set directional negative true");
    result = lc_directional_light3d_is_negative(light, &negative);
    TEST_RESULT(result, "Get directional negative after set");
    TEST_ASSERT(negative == true, "Directional negative is true");

    result = lc_directional_light3d_set_negative(light, default_negative);
    TEST_RESULT(result, "Restore directional negative");

    bool casts = true;
    result = lc_directional_light3d_casts_shadows(light, &casts);
    TEST_RESULT(result, "Query directional casts_shadows (default)");
    bool default_casts = casts;

    result = lc_directional_light3d_set_casts_shadows(light, true);
    TEST_RESULT(result, "Enable directional shadows");
    result = lc_directional_light3d_casts_shadows(light, &casts);
    TEST_RESULT(result, "Get directional casts_shadows after set");
    TEST_ASSERT(casts == true, "Directional casts_shadows is true");
    result = lc_directional_light3d_set_casts_shadows(light, default_casts);
    TEST_RESULT(result, "Restore directional casts_shadows");

    result = lc_directional_light3d_set_shadow_opacity(light, 0.6f);
    TEST_RESULT(result, "Set directional shadow opacity");
    float opacity = 0.0f;
    result = lc_directional_light3d_get_shadow_opacity(light, &opacity);
    TEST_RESULT(result, "Get directional shadow opacity");
    TEST_ASSERT(opacity > 0.59f && opacity < 0.61f, "Directional shadow opacity round-trips");

    result = lc_directional_light3d_set_shadow_bias(light, 0.0025f);
    TEST_RESULT(result, "Set directional shadow bias");
    float bias = 0.0f;
    result = lc_directional_light3d_get_shadow_bias(light, &bias);
    TEST_RESULT(result, "Get directional shadow bias");
    TEST_ASSERT(bias > 0.0024f && bias < 0.0026f, "Directional shadow bias round-trips");

    result = lc_component_destroy(light);
    TEST_RESULT(result, "Destroy DirectionalLight3D");

    SECTION_END("Lights Ext: DirectionalLight3D");
}

static void test_ext_lights_omni(void) {
    SECTION_START("Lights Ext: OmniLight3D");

    LCComponentHandle light = NULL;
    LCResult result = lc_omni_light3d_create("ExtOmniLight", &light);
    TEST_RESULT(result, "Create OmniLight3D");

    LCColor set_color = lc_color(0.9f, 0.1f, 0.4f, 0.8f);
    result = lc_omni_light3d_set_color(light, set_color);
    TEST_RESULT(result, "Set omni light color");
    LCColor got_color = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    result = lc_omni_light3d_get_color(light, &got_color);
    TEST_RESULT(result, "Get omni light color");
    TEST_ASSERT(got_color.r == set_color.r, "Omni color.r round-trips");
    TEST_ASSERT(got_color.g == set_color.g, "Omni color.g round-trips");
    TEST_ASSERT(got_color.b == set_color.b, "Omni color.b round-trips");
    TEST_ASSERT(got_color.a == set_color.a, "Omni color.a round-trips");

    result = lc_omni_light3d_set_intensity(light, 3.5f);
    TEST_RESULT(result, "Set omni light intensity");
    float intensity = 0.0f;
    result = lc_omni_light3d_get_intensity(light, &intensity);
    TEST_RESULT(result, "Get omni light intensity");
    TEST_ASSERT(intensity > 3.49f && intensity < 3.51f, "Omni intensity round-trips");

    result = lc_omni_light3d_set_range(light, 12.5f);
    TEST_RESULT(result, "Set omni light range");
    float range = 0.0f;
    result = lc_omni_light3d_get_range(light, &range);
    TEST_RESULT(result, "Get omni light range");
    TEST_ASSERT(range > 12.49f && range < 12.51f, "Omni range round-trips");

    result = lc_omni_light3d_set_attenuation(light, 2.0f);
    TEST_RESULT(result, "Set omni light attenuation");
    float attenuation = 0.0f;
    result = lc_omni_light3d_get_attenuation(light, &attenuation);
    TEST_RESULT(result, "Get omni light attenuation");
    TEST_ASSERT(attenuation > 1.99f && attenuation < 2.01f, "Omni attenuation round-trips");

    bool negative = true;
    result = lc_omni_light3d_is_negative(light, &negative);
    TEST_RESULT(result, "Query omni negative (default)");
    bool default_negative = negative;
    result = lc_omni_light3d_set_negative(light, true);
    TEST_RESULT(result, "Set omni negative true");
    result = lc_omni_light3d_is_negative(light, &negative);
    TEST_RESULT(result, "Get omni negative after set");
    TEST_ASSERT(negative == true, "Omni negative is true");
    result = lc_omni_light3d_set_negative(light, default_negative);
    TEST_RESULT(result, "Restore omni negative");

    bool casts = true;
    result = lc_omni_light3d_casts_shadows(light, &casts);
    TEST_RESULT(result, "Query omni casts_shadows (default)");
    bool default_casts = casts;
    result = lc_omni_light3d_set_casts_shadows(light, true);
    TEST_RESULT(result, "Enable omni shadows");
    result = lc_omni_light3d_casts_shadows(light, &casts);
    TEST_RESULT(result, "Get omni casts_shadows after set");
    TEST_ASSERT(casts == true, "Omni casts_shadows is true");
    result = lc_omni_light3d_set_casts_shadows(light, default_casts);
    TEST_RESULT(result, "Restore omni casts_shadows");

    result = lc_component_destroy(light);
    TEST_RESULT(result, "Destroy OmniLight3D");

    SECTION_END("Lights Ext: OmniLight3D");
}

static void test_ext_lights_spot(void) {
    SECTION_START("Lights Ext: SpotLight3D");

    LCComponentHandle light = NULL;
    LCResult result = lc_spot_light3d_create("ExtSpotLight", &light);
    TEST_RESULT(result, "Create SpotLight3D");

    LCColor set_color = lc_color(0.2f, 0.6f, 0.95f, 0.5f);
    result = lc_spot_light3d_set_color(light, set_color);
    TEST_RESULT(result, "Set spotlight color");
    LCColor got_color = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    result = lc_spot_light3d_get_color(light, &got_color);
    TEST_RESULT(result, "Get spotlight color");
    TEST_ASSERT(got_color.r == set_color.r, "Spot color.r round-trips");
    TEST_ASSERT(got_color.g == set_color.g, "Spot color.g round-trips");
    TEST_ASSERT(got_color.b == set_color.b, "Spot color.b round-trips");
    TEST_ASSERT(got_color.a == set_color.a, "Spot color.a round-trips");

    result = lc_spot_light3d_set_intensity(light, 4.25f);
    TEST_RESULT(result, "Set spotlight intensity");
    float intensity = 0.0f;
    result = lc_spot_light3d_get_intensity(light, &intensity);
    TEST_RESULT(result, "Get spotlight intensity");
    TEST_ASSERT(intensity > 4.24f && intensity < 4.26f, "Spot intensity round-trips");

    result = lc_spot_light3d_set_range(light, 20.0f);
    TEST_RESULT(result, "Set spotlight range");
    float range = 0.0f;
    result = lc_spot_light3d_get_range(light, &range);
    TEST_RESULT(result, "Get spotlight range");
    TEST_ASSERT(range > 19.99f && range < 20.01f, "Spot range round-trips");

    result = lc_spot_light3d_set_attenuation(light, 1.5f);
    TEST_RESULT(result, "Set spotlight attenuation");
    float attenuation = 0.0f;
    result = lc_spot_light3d_get_attenuation(light, &attenuation);
    TEST_RESULT(result, "Get spotlight attenuation");
    TEST_ASSERT(attenuation > 1.49f && attenuation < 1.51f, "Spot attenuation round-trips");

    result = lc_spot_light3d_set_inner_cone_angle(light, 12.0f);
    TEST_RESULT(result, "Set spotlight inner cone angle");
    float inner = 0.0f;
    result = lc_spot_light3d_get_inner_cone_angle(light, &inner);
    TEST_RESULT(result, "Get spotlight inner cone angle");
    TEST_ASSERT(inner > 11.99f && inner < 12.01f, "Spot inner cone round-trips");

    result = lc_spot_light3d_set_outer_cone_angle(light, 35.0f);
    TEST_RESULT(result, "Set spotlight outer cone angle");
    float outer = 0.0f;
    result = lc_spot_light3d_get_outer_cone_angle(light, &outer);
    TEST_RESULT(result, "Get spotlight outer cone angle");
    TEST_ASSERT(outer > 34.99f && outer < 35.01f, "Spot outer cone round-trips");

    bool negative = true;
    result = lc_spot_light3d_is_negative(light, &negative);
    TEST_RESULT(result, "Query spot negative (default)");
    bool default_negative = negative;
    result = lc_spot_light3d_set_negative(light, true);
    TEST_RESULT(result, "Set spot negative true");
    result = lc_spot_light3d_is_negative(light, &negative);
    TEST_RESULT(result, "Get spot negative after set");
    TEST_ASSERT(negative == true, "Spot negative is true");
    result = lc_spot_light3d_set_negative(light, default_negative);
    TEST_RESULT(result, "Restore spot negative");

    bool casts = true;
    result = lc_spot_light3d_casts_shadows(light, &casts);
    TEST_RESULT(result, "Query spot casts_shadows (default)");
    bool default_casts = casts;
    result = lc_spot_light3d_set_casts_shadows(light, true);
    TEST_RESULT(result, "Enable spot shadows");
    result = lc_spot_light3d_casts_shadows(light, &casts);
    TEST_RESULT(result, "Get spot casts_shadows after set");
    TEST_ASSERT(casts == true, "Spot casts_shadows is true");
    result = lc_spot_light3d_set_casts_shadows(light, default_casts);
    TEST_RESULT(result, "Restore spot casts_shadows");

    result = lc_component_destroy(light);
    TEST_RESULT(result, "Destroy SpotLight3D");

    SECTION_END("Lights Ext: SpotLight3D");
}
