/* ============================================================================
 * Extended Rendering / Scene Component Tests
 *
 * Fragment included into capi_test_suite.c. Covers public functions of the
 * rendering, mesh, 2D-shape, tilemap, environment, scatter, ysort, camera and
 * input headers that are not already exercised by the base suite.
 * ============================================================================ */

static bool ext_float_eq(float a, float b) {
    float d = a - b;
    return d < 0.001f && d > -0.001f;
}

static bool ext_color_eq(LCColor a, LCColor b) {
    return ext_float_eq(a.r, b.r) && ext_float_eq(a.g, b.g) &&
           ext_float_eq(a.b, b.b) && ext_float_eq(a.a, b.a);
}

/* ============================================================================
 * Sprite2D - remaining getters / texture path / flip-v / offset
 * ============================================================================ */

static void test_ext_sprite2d(void) {
    SECTION_START("Rendering: Sprite2D (extended)");

    LCComponentHandle sprite = NULL;
    LCResult result = lc_sprite2d_create("ExtSprite2D", &sprite);
    TEST_RESULT(result, "Create Sprite2D");

    result = lc_sprite2d_load_texture(sprite, "res://textures/test.png");
    printf("    load_texture result: %d\n", (int)result);
    TEST_ASSERT(true, "lc_sprite2d_load_texture() callable");

    result = lc_sprite2d_set_texture_path(sprite, "res://textures/icon.png");
    TEST_RESULT(result, "Set texture path");

    char path[256];
    result = lc_sprite2d_get_texture_path(sprite, path, sizeof(path));
    TEST_RESULT(result, "Get texture path");
    TEST_ASSERT(strcmp(path, "res://textures/icon.png") == 0, "Texture path round-trips");

    result = lc_sprite2d_set_size(sprite, lc_vec2(48.0f, 96.0f));
    TEST_RESULT(result, "Set size");

    LCVec2 size = {0.0f, 0.0f};
    result = lc_sprite2d_get_size(sprite, &size);
    TEST_RESULT(result, "Get size");
    TEST_ASSERT(ext_float_eq(size.x, 48.0f) && ext_float_eq(size.y, 96.0f), "Size round-trips");

    result = lc_sprite2d_set_uv_rect(sprite, lc_vec4(0.1f, 0.2f, 0.3f, 0.4f));
    TEST_RESULT(result, "Set UV rect");

    LCVec4 uv = {0.0f, 0.0f, 0.0f, 0.0f};
    result = lc_sprite2d_get_uv_rect(sprite, &uv);
    TEST_RESULT(result, "Get UV rect");
    TEST_ASSERT(ext_float_eq(uv.x, 0.1f) && ext_float_eq(uv.y, 0.2f) &&
                ext_float_eq(uv.z, 0.3f) && ext_float_eq(uv.w, 0.4f), "UV rect round-trips");

    result = lc_sprite2d_set_alpha_cutoff(sprite, 0.35f);
    TEST_RESULT(result, "Set alpha cutoff");

    float cutoff = 0.0f;
    result = lc_sprite2d_get_alpha_cutoff(sprite, &cutoff);
    TEST_RESULT(result, "Get alpha cutoff");
    TEST_ASSERT(ext_float_eq(cutoff, 0.35f), "Alpha cutoff round-trips");

    result = lc_sprite2d_set_flip_v(sprite, true);
    TEST_RESULT(result, "Set vertical flip");

    bool flip_v = false;
    result = lc_sprite2d_get_flip_v(sprite, &flip_v);
    TEST_RESULT(result, "Get vertical flip");
    TEST_ASSERT(flip_v == true, "Vertical flip is enabled");

    result = lc_sprite2d_set_flip_v(sprite, false);
    TEST_RESULT(result, "Restore vertical flip");
    lc_sprite2d_get_flip_v(sprite, &flip_v);
    TEST_ASSERT(flip_v == false, "Vertical flip restored");

    result = lc_sprite2d_set_centered(sprite, false);
    TEST_RESULT(result, "Set centered false");

    bool centered = true;
    result = lc_sprite2d_get_centered(sprite, &centered);
    TEST_RESULT(result, "Get centered");
    TEST_ASSERT(centered == false, "Centered is false");

    result = lc_sprite2d_set_offset(sprite, lc_vec2(12.0f, -8.0f));
    TEST_RESULT(result, "Set offset");

    LCVec2 offset = {0.0f, 0.0f};
    result = lc_sprite2d_get_offset(sprite, &offset);
    TEST_RESULT(result, "Get offset");
    TEST_ASSERT(ext_float_eq(offset.x, 12.0f) && ext_float_eq(offset.y, -8.0f), "Offset round-trips");

    lc_component_destroy(sprite);

    SECTION_END("Rendering: Sprite2D (extended)");
}

/* ============================================================================
 * Sprite3D - texture/modulate/size/uv/alpha/flip/getters
 * ============================================================================ */

static void test_ext_sprite3d(void) {
    SECTION_START("Rendering: Sprite3D (extended)");

    LCComponentHandle sprite = NULL;
    LCResult result = lc_sprite3d_create("ExtSprite3D", &sprite);
    TEST_RESULT(result, "Create Sprite3D");

    result = lc_sprite3d_load_texture(sprite, "res://textures/test.png");
    printf("    load_texture result: %d\n", (int)result);
    TEST_ASSERT(true, "lc_sprite3d_load_texture() callable");

    result = lc_sprite3d_set_texture_path(sprite, "res://textures/billboard.png");
    TEST_RESULT(result, "Set texture path");

    char path[256];
    result = lc_sprite3d_get_texture_path(sprite, path, sizeof(path));
    TEST_RESULT(result, "Get texture path");
    TEST_ASSERT(strcmp(path, "res://textures/billboard.png") == 0, "Texture path round-trips");

    result = lc_sprite3d_set_modulate(sprite, lc_color(0.25f, 0.5f, 0.75f, 1.0f));
    TEST_RESULT(result, "Set modulate");

    LCColor color = {0.0f, 0.0f, 0.0f, 0.0f};
    result = lc_sprite3d_get_modulate(sprite, &color);
    TEST_RESULT(result, "Get modulate");
    TEST_ASSERT(ext_color_eq(color, lc_color(0.25f, 0.5f, 0.75f, 1.0f)), "Modulate round-trips");

    result = lc_sprite3d_set_size(sprite, lc_vec2(2.0f, 3.0f));
    TEST_RESULT(result, "Set size");

    LCVec2 size = {0.0f, 0.0f};
    result = lc_sprite3d_get_size(sprite, &size);
    TEST_RESULT(result, "Get size");
    TEST_ASSERT(ext_float_eq(size.x, 2.0f) && ext_float_eq(size.y, 3.0f), "Size round-trips");

    result = lc_sprite3d_set_uv_rect(sprite, lc_vec4(0.0f, 0.0f, 0.5f, 0.5f));
    TEST_RESULT(result, "Set UV rect");

    LCVec4 uv = {0.0f, 0.0f, 0.0f, 0.0f};
    result = lc_sprite3d_get_uv_rect(sprite, &uv);
    TEST_RESULT(result, "Get UV rect");
    TEST_ASSERT(ext_float_eq(uv.z, 0.5f) && ext_float_eq(uv.w, 0.5f), "UV rect round-trips");

    result = lc_sprite3d_set_alpha_cutoff(sprite, 0.6f);
    TEST_RESULT(result, "Set alpha cutoff");

    float cutoff = 0.0f;
    result = lc_sprite3d_get_alpha_cutoff(sprite, &cutoff);
    TEST_RESULT(result, "Get alpha cutoff");
    TEST_ASSERT(ext_float_eq(cutoff, 0.6f), "Alpha cutoff round-trips");

    result = lc_sprite3d_set_flip_h(sprite, true);
    TEST_RESULT(result, "Set horizontal flip");

    bool flip_h = false;
    result = lc_sprite3d_get_flip_h(sprite, &flip_h);
    TEST_RESULT(result, "Get horizontal flip");
    TEST_ASSERT(flip_h == true, "Horizontal flip enabled");

    result = lc_sprite3d_set_flip_v(sprite, true);
    TEST_RESULT(result, "Set vertical flip");

    bool flip_v = false;
    result = lc_sprite3d_get_flip_v(sprite, &flip_v);
    TEST_RESULT(result, "Get vertical flip");
    TEST_ASSERT(flip_v == true, "Vertical flip enabled");

    result = lc_sprite3d_set_double_sided(sprite, true);
    TEST_RESULT(result, "Set double sided");

    bool double_sided = false;
    result = lc_sprite3d_get_double_sided(sprite, &double_sided);
    TEST_RESULT(result, "Get double sided");
    TEST_ASSERT(double_sided == true, "Double sided enabled");

    result = lc_sprite3d_set_cast_shadow(sprite, false);
    TEST_RESULT(result, "Set cast shadow false");

    bool cast_shadow = true;
    result = lc_sprite3d_get_cast_shadow(sprite, &cast_shadow);
    TEST_RESULT(result, "Get cast shadow");
    TEST_ASSERT(cast_shadow == false, "Cast shadow disabled");

    result = lc_sprite3d_set_receive_shadow(sprite, false);
    TEST_RESULT(result, "Set receive shadow false");

    bool receive_shadow = true;
    result = lc_sprite3d_get_receive_shadow(sprite, &receive_shadow);
    TEST_RESULT(result, "Get receive shadow");
    TEST_ASSERT(receive_shadow == false, "Receive shadow disabled");

    lc_component_destroy(sprite);

    SECTION_END("Rendering: Sprite3D (extended)");
}

/* ============================================================================
 * StaticMesh3D - model path / receive shadow / double sided getters
 * ============================================================================ */

static void test_ext_static_mesh3d(void) {
    SECTION_START("Rendering: StaticMesh3D (extended)");

    LCComponentHandle mesh = NULL;
    LCResult result = lc_static_mesh3d_create("ExtStaticMesh", &mesh);
    TEST_RESULT(result, "Create StaticMesh3D");

    result = lc_static_mesh3d_load_model(mesh, "res://models/test.gltf");
    printf("    load_model result: %d\n", (int)result);
    TEST_ASSERT(true, "lc_static_mesh3d_load_model() callable");

    result = lc_static_mesh3d_set_model_path(mesh, "res://models/crate.obj");
    TEST_RESULT(result, "Set model path");

    char path[256];
    result = lc_static_mesh3d_get_model_path(mesh, path, sizeof(path));
    TEST_RESULT(result, "Get model path");
    TEST_ASSERT(strcmp(path, "res://models/crate.obj") == 0, "Model path round-trips");

    result = lc_static_mesh3d_set_receive_shadow(mesh, false);
    TEST_RESULT(result, "Set receive shadow false");

    bool receive_shadow = true;
    result = lc_static_mesh3d_get_receive_shadow(mesh, &receive_shadow);
    TEST_RESULT(result, "Get receive shadow");
    TEST_ASSERT(receive_shadow == false, "Receive shadow disabled");

    result = lc_static_mesh3d_set_double_sided(mesh, true);
    TEST_RESULT(result, "Set double sided");

    bool double_sided = false;
    result = lc_static_mesh3d_get_double_sided(mesh, &double_sided);
    TEST_RESULT(result, "Get double sided");
    TEST_ASSERT(double_sided == true, "Double sided enabled");

    lc_component_destroy(mesh);

    SECTION_END("Rendering: StaticMesh3D (extended)");
}

/* ============================================================================
 * PrimitiveMesh3D - remaining material / toon getters
 * ============================================================================ */

static void test_ext_primitive_mesh3d(void) {
    SECTION_START("Rendering: PrimitiveMesh3D (extended)");

    LCComponentHandle mesh = NULL;
    LCResult result = lc_primitive_mesh3d_create("ExtPrimitiveMesh", &mesh);
    TEST_RESULT(result, "Create PrimitiveMesh3D");

    result = lc_primitive_mesh3d_set_normal_scale(mesh, 0.7f);
    TEST_RESULT(result, "Set normal scale");

    float normal_scale = 0.0f;
    result = lc_primitive_mesh3d_get_normal_scale(mesh, &normal_scale);
    TEST_RESULT(result, "Get normal scale");
    TEST_ASSERT(ext_float_eq(normal_scale, 0.7f), "Normal scale round-trips");

    result = lc_primitive_mesh3d_set_emissive_color(mesh, lc_color(0.2f, 0.4f, 0.6f, 1.0f));
    TEST_RESULT(result, "Set emissive color");

    LCColor emissive = {0.0f, 0.0f, 0.0f, 0.0f};
    result = lc_primitive_mesh3d_get_emissive_color(mesh, &emissive);
    TEST_RESULT(result, "Get emissive color");
    TEST_ASSERT(ext_color_eq(emissive, lc_color(0.2f, 0.4f, 0.6f, 1.0f)), "Emissive color round-trips");

    result = lc_primitive_mesh3d_set_shadow_threshold(mesh, 0.45f);
    TEST_RESULT(result, "Set shadow threshold");

    float shadow_threshold = 0.0f;
    result = lc_primitive_mesh3d_get_shadow_threshold(mesh, &shadow_threshold);
    TEST_RESULT(result, "Get shadow threshold");
    TEST_ASSERT(ext_float_eq(shadow_threshold, 0.45f), "Shadow threshold round-trips");

    result = lc_primitive_mesh3d_set_shadow_softness(mesh, 0.25f);
    TEST_RESULT(result, "Set shadow softness");

    float shadow_softness = 0.0f;
    result = lc_primitive_mesh3d_get_shadow_softness(mesh, &shadow_softness);
    TEST_RESULT(result, "Get shadow softness");
    TEST_ASSERT(ext_float_eq(shadow_softness, 0.25f), "Shadow softness round-trips");

    result = lc_primitive_mesh3d_set_specular_bands(mesh, 3.0f);
    TEST_RESULT(result, "Set specular bands");

    float specular_bands = 0.0f;
    result = lc_primitive_mesh3d_get_specular_bands(mesh, &specular_bands);
    TEST_RESULT(result, "Get specular bands");
    TEST_ASSERT(ext_float_eq(specular_bands, 3.0f), "Specular bands round-trips");

    result = lc_primitive_mesh3d_set_specular_power(mesh, 16.0f);
    TEST_RESULT(result, "Set specular power");

    float specular_power = 0.0f;
    result = lc_primitive_mesh3d_get_specular_power(mesh, &specular_power);
    TEST_RESULT(result, "Get specular power");
    TEST_ASSERT(ext_float_eq(specular_power, 16.0f), "Specular power round-trips");

    result = lc_primitive_mesh3d_set_rim_intensity(mesh, 0.65f);
    TEST_RESULT(result, "Set rim intensity");

    float rim_intensity = 0.0f;
    result = lc_primitive_mesh3d_get_rim_intensity(mesh, &rim_intensity);
    TEST_RESULT(result, "Get rim intensity");
    TEST_ASSERT(ext_float_eq(rim_intensity, 0.65f), "Rim intensity round-trips");

    lc_component_destroy(mesh);

    SECTION_END("Rendering: PrimitiveMesh3D (extended)");
}

/* ============================================================================
 * SkeletalMesh3D - load/anim-name/playback/material-slot coverage
 * ============================================================================ */

static void test_ext_skeletal_mesh3d(void) {
    SECTION_START("Rendering: SkeletalMesh3D (extended)");

    LCComponentHandle mesh = NULL;
    LCResult result = lc_skeletal_mesh3d_create("ExtSkeletalMesh", &mesh);
    TEST_RESULT(result, "Create SkeletalMesh3D");

    result = lc_skeletal_mesh3d_load_model(mesh, "res://models/character.fbx");
    printf("    load_model result: %d\n", (int)result);
    TEST_ASSERT(true, "lc_skeletal_mesh3d_load_model() callable");

    char anim_name[128];
    result = lc_skeletal_mesh3d_get_animation_name_at(mesh, 0, anim_name, (int)sizeof(anim_name));
    printf("    get_animation_name_at result: %d\n", (int)result);
    TEST_ASSERT(true, "lc_skeletal_mesh3d_get_animation_name_at() callable");

    result = lc_skeletal_mesh3d_play_animation(mesh, "Idle", true);
    printf("    play_animation result: %d\n", (int)result);
    TEST_ASSERT(true, "lc_skeletal_mesh3d_play_animation() callable");

    /* Material slot accessors (no slots without a real model => callable checks) */
    char slot_name[128];
    result = lc_skeletal_mesh3d_get_material_slot_name(mesh, 0, slot_name, (int)sizeof(slot_name));
    printf("    get_material_slot_name result: %d\n", (int)result);
    TEST_ASSERT(true, "lc_skeletal_mesh3d_get_material_slot_name() callable");

    bool slot_override = false;
    result = lc_skeletal_mesh3d_get_material_slot_override_enabled(mesh, 0, &slot_override);
    TEST_ASSERT(true, "lc_skeletal_mesh3d_get_material_slot_override_enabled() callable");

    result = lc_skeletal_mesh3d_set_material_slot_override_enabled(mesh, 0, true);
    TEST_ASSERT(true, "lc_skeletal_mesh3d_set_material_slot_override_enabled() callable");

    LCColor slot_albedo = {0.0f, 0.0f, 0.0f, 0.0f};
    result = lc_skeletal_mesh3d_get_material_slot_albedo_color(mesh, 0, &slot_albedo);
    TEST_ASSERT(true, "lc_skeletal_mesh3d_get_material_slot_albedo_color() callable");

    result = lc_skeletal_mesh3d_set_material_slot_albedo_color(mesh, 0, lc_color_white());
    TEST_ASSERT(true, "lc_skeletal_mesh3d_set_material_slot_albedo_color() callable");

    result = lc_skeletal_mesh3d_set_material_slot_albedo_texture(mesh, 0, "res://textures/skin.png");
    TEST_ASSERT(true, "lc_skeletal_mesh3d_set_material_slot_albedo_texture() callable");

    float slot_metallic = 0.0f;
    result = lc_skeletal_mesh3d_get_material_slot_metallic(mesh, 0, &slot_metallic);
    TEST_ASSERT(true, "lc_skeletal_mesh3d_get_material_slot_metallic() callable");

    result = lc_skeletal_mesh3d_set_material_slot_metallic(mesh, 0, 0.5f);
    TEST_ASSERT(true, "lc_skeletal_mesh3d_set_material_slot_metallic() callable");

    float slot_roughness = 0.0f;
    result = lc_skeletal_mesh3d_get_material_slot_roughness(mesh, 0, &slot_roughness);
    TEST_ASSERT(true, "lc_skeletal_mesh3d_get_material_slot_roughness() callable");

    result = lc_skeletal_mesh3d_set_material_slot_roughness(mesh, 0, 0.5f);
    TEST_ASSERT(true, "lc_skeletal_mesh3d_set_material_slot_roughness() callable");

    lc_component_destroy(mesh);

    SECTION_END("Rendering: SkeletalMesh3D (extended)");
}

/* ============================================================================
 * Line2D - set_end (only remaining accessor)
 * ============================================================================ */

static void test_ext_line2d(void) {
    SECTION_START("Rendering: Line2D (extended)");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("ExtLine2DScene", &scene);
    TEST_RESULT(result, "Create scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "ExtLine2DNode", &node);
    TEST_RESULT(result, "Create node");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle line = NULL;
    result = lc_line2d_create(node, &line);
    TEST_RESULT(result, "Create Line2D");

    result = lc_line2d_add_point(line, lc_vec2(0.0f, 0.0f));
    TEST_RESULT(result, "Add first point");
    result = lc_line2d_add_point(line, lc_vec2(10.0f, 10.0f));
    TEST_RESULT(result, "Add second point");

    result = lc_line2d_set_end(line, lc_vec2(42.0f, 24.0f));
    TEST_RESULT(result, "Set end point");

    LCVec2 end_point = {0.0f, 0.0f};
    result = lc_line2d_get_end(line, &end_point);
    TEST_RESULT(result, "Get end point");
    TEST_ASSERT(ext_float_eq(end_point.x, 42.0f) && ext_float_eq(end_point.y, 24.0f), "End point round-trips");

    lc_scene_destroy(scene);

    SECTION_END("Rendering: Line2D (extended)");
}

/* ============================================================================
 * Shape2D - width/height/border-color/sorting/ui-space/shader getters
 * ============================================================================ */

static void test_ext_shape2d(void) {
    SECTION_START("Rendering: Shape2D (extended)");

    LCComponentHandle shape = NULL;
    LCResult result = lc_shape2d_create("ExtShape2D", &shape);
    TEST_RESULT(result, "Create Shape2D");

    result = lc_shape2d_set_width(shape, 120.0f);
    TEST_RESULT(result, "Set width");

    float width = 0.0f;
    result = lc_shape2d_get_width(shape, &width);
    TEST_RESULT(result, "Get width");
    TEST_ASSERT(ext_float_eq(width, 120.0f), "Width round-trips");

    result = lc_shape2d_set_height(shape, 64.0f);
    TEST_RESULT(result, "Set height");

    float height = 0.0f;
    result = lc_shape2d_get_height(shape, &height);
    TEST_RESULT(result, "Get height");
    TEST_ASSERT(ext_float_eq(height, 64.0f), "Height round-trips");

    result = lc_shape2d_set_border_color(shape, lc_color(0.1f, 0.2f, 0.3f, 1.0f));
    TEST_RESULT(result, "Set border color");

    LCColor border_color = {0.0f, 0.0f, 0.0f, 0.0f};
    result = lc_shape2d_get_border_color(shape, &border_color);
    TEST_RESULT(result, "Get border color");
    TEST_ASSERT(ext_color_eq(border_color, lc_color(0.1f, 0.2f, 0.3f, 1.0f)), "Border color round-trips");

    result = lc_shape2d_set_sorting_order(shape, 7);
    TEST_RESULT(result, "Set sorting order");

    int sorting_order = 0;
    result = lc_shape2d_get_sorting_order(shape, &sorting_order);
    TEST_RESULT(result, "Get sorting order");
    TEST_ASSERT(sorting_order == 7, "Sorting order round-trips");

    result = lc_shape2d_set_ui_space(shape, true);
    TEST_RESULT(result, "Set UI space");

    bool ui_space = false;
    result = lc_shape2d_get_ui_space(shape, &ui_space);
    TEST_RESULT(result, "Get UI space");
    TEST_ASSERT(ui_space == true, "UI space enabled");

    result = lc_shape2d_set_shader(shape, "");
    TEST_RESULT(result, "Set empty shader");

    char shader_path[256];
    result = lc_shape2d_get_shader(shape, shader_path, sizeof(shader_path));
    TEST_RESULT(result, "Get shader path");
    TEST_ASSERT(strcmp(shader_path, "") == 0, "Empty shader round-trips");

    result = lc_shape2d_set_shader_parameters(shape, "{}");
    TEST_RESULT(result, "Set shader parameters");

    char shader_params[256];
    result = lc_shape2d_get_shader_parameters(shape, shader_params, sizeof(shader_params));
    TEST_RESULT(result, "Get shader parameters");
    TEST_ASSERT(strcmp(shader_params, "{}") == 0, "Shader parameters round-trips");

    lc_component_destroy(shape);

    SECTION_END("Rendering: Shape2D (extended)");
}

/* ============================================================================
 * TileMap2D - per-index/by-name layer ops + metadata/free helpers
 * ============================================================================ */

static void test_ext_tilemap2d(void) {
    SECTION_START("Rendering: TileMap2D (extended)");

    LCComponentHandle map = NULL;
    LCResult result = lc_tilemap2d_create("ExtTileMap2D", &map);
    TEST_RESULT(result, "Create TileMap2D");

    /* No tilemap loaded => 0 layers; verify these are callable without crashing */
    result = lc_tilemap2d_set_layer_visible(map, 0, true);
    TEST_ASSERT(true, "lc_tilemap2d_set_layer_visible() callable");

    bool visible = false;
    result = lc_tilemap2d_get_layer_visible_by_name(map, "Ground", &visible);
    TEST_ASSERT(true, "lc_tilemap2d_get_layer_visible_by_name() callable");

    result = lc_tilemap2d_set_layer_opacity(map, 0, 0.5f);
    TEST_ASSERT(true, "lc_tilemap2d_set_layer_opacity() callable");

    result = lc_tilemap2d_set_layer_opacity_by_name(map, "Ground", 0.5f);
    TEST_ASSERT(true, "lc_tilemap2d_set_layer_opacity_by_name() callable");

    float opacity = 0.0f;
    result = lc_tilemap2d_get_layer_opacity_by_name(map, "Ground", &opacity);
    TEST_ASSERT(true, "lc_tilemap2d_get_layer_opacity_by_name() callable");

    result = lc_tilemap2d_set_layer_z_offset(map, 0, 5);
    TEST_ASSERT(true, "lc_tilemap2d_set_layer_z_offset() callable");

    result = lc_tilemap2d_set_layer_z_offset_by_name(map, "Ground", 5);
    TEST_ASSERT(true, "lc_tilemap2d_set_layer_z_offset_by_name() callable");

    int z_offset = 0;
    result = lc_tilemap2d_get_layer_z_offset_by_name(map, "Ground", &z_offset);
    TEST_ASSERT(true, "lc_tilemap2d_get_layer_z_offset_by_name() callable");

    LCTileMetadata metadata;
    memset(&metadata, 0, sizeof(metadata));
    result = lc_tilemap2d_get_tile_metadata(map, 0, 0, &metadata);
    printf("    get_tile_metadata result: %d\n", (int)result);
    TEST_ASSERT(true, "lc_tilemap2d_get_tile_metadata() callable");
    lc_tilemap2d_free_metadata(&metadata);
    TEST_ASSERT(true, "lc_tilemap2d_free_metadata() callable");

    LCTileCollisionShape collision;
    memset(&collision, 0, sizeof(collision));
    result = lc_tilemap2d_get_collision_shape_at(map, 0, 0, 0, &collision);
    printf("    get_collision_shape_at result: %d\n", (int)result);
    lc_tilemap2d_free_collision_shape(&collision);
    TEST_ASSERT(true, "lc_tilemap2d_free_collision_shape() callable");

    lc_component_destroy(map);

    SECTION_END("Rendering: TileMap2D (extended)");
}

/* ============================================================================
 * WorldEnvironment - fog/ambient color getters + remaining cubemap faces
 * ============================================================================ */

static void test_ext_world_environment(void) {
    SECTION_START("Rendering: WorldEnvironment (extended)");

    LCComponentHandle env = NULL;
    LCResult result = lc_world_environment_create("ExtWorldEnv", &env);
    TEST_RESULT(result, "Create WorldEnvironment");

    result = lc_world_environment_set_fog_color(env, lc_color(0.5f, 0.6f, 0.7f, 1.0f));
    TEST_RESULT(result, "Set fog color");

    LCColor fog_color = {0.0f, 0.0f, 0.0f, 0.0f};
    result = lc_world_environment_get_fog_color(env, &fog_color);
    TEST_RESULT(result, "Get fog color");
    TEST_ASSERT(ext_color_eq(fog_color, lc_color(0.5f, 0.6f, 0.7f, 1.0f)), "Fog color round-trips");

    result = lc_world_environment_set_ambient_light_color(env, lc_color(0.3f, 0.3f, 0.4f, 1.0f));
    TEST_RESULT(result, "Set ambient light color");

    LCColor ambient = {0.0f, 0.0f, 0.0f, 0.0f};
    result = lc_world_environment_get_ambient_light_color(env, &ambient);
    TEST_RESULT(result, "Get ambient light color");
    TEST_ASSERT(ext_color_eq(ambient, lc_color(0.3f, 0.3f, 0.4f, 1.0f)), "Ambient color round-trips");

    char path[512];

    result = lc_world_environment_set_cubemap_neg_x(env, "res://sky/nx.png");
    TEST_RESULT(result, "Set cubemap -X");
    result = lc_world_environment_get_cubemap_neg_x(env, path, sizeof(path));
    TEST_RESULT(result, "Get cubemap -X");
    TEST_ASSERT(strcmp(path, "res://sky/nx.png") == 0, "Cubemap -X round-trips");

    result = lc_world_environment_set_cubemap_pos_y(env, "res://sky/py.png");
    TEST_RESULT(result, "Set cubemap +Y");
    result = lc_world_environment_get_cubemap_pos_y(env, path, sizeof(path));
    TEST_RESULT(result, "Get cubemap +Y");
    TEST_ASSERT(strcmp(path, "res://sky/py.png") == 0, "Cubemap +Y round-trips");

    result = lc_world_environment_set_cubemap_neg_y(env, "res://sky/ny.png");
    TEST_RESULT(result, "Set cubemap -Y");
    result = lc_world_environment_get_cubemap_neg_y(env, path, sizeof(path));
    TEST_RESULT(result, "Get cubemap -Y");
    TEST_ASSERT(strcmp(path, "res://sky/ny.png") == 0, "Cubemap -Y round-trips");

    result = lc_world_environment_set_cubemap_pos_z(env, "res://sky/pz.png");
    TEST_RESULT(result, "Set cubemap +Z");
    result = lc_world_environment_get_cubemap_pos_z(env, path, sizeof(path));
    TEST_RESULT(result, "Get cubemap +Z");
    TEST_ASSERT(strcmp(path, "res://sky/pz.png") == 0, "Cubemap +Z round-trips");

    result = lc_world_environment_set_cubemap_neg_z(env, "res://sky/nz.png");
    TEST_RESULT(result, "Set cubemap -Z");
    result = lc_world_environment_get_cubemap_neg_z(env, path, sizeof(path));
    TEST_RESULT(result, "Get cubemap -Z");
    TEST_ASSERT(strcmp(path, "res://sky/nz.png") == 0, "Cubemap -Z round-trips");

    lc_component_destroy(env);

    SECTION_END("Rendering: WorldEnvironment (extended)");
}

/* ============================================================================
 * WorldEnvironment - post-processing stack (bloom/tonemap/grading/SSAO/...)
 * ============================================================================ */

static void test_ext_world_environment_postprocess(void) {
    SECTION_START("Rendering: WorldEnvironment post-processing");

    LCComponentHandle env = NULL;
    LCResult result = lc_world_environment_create("ExtWorldEnvPP", &env);
    TEST_RESULT(result, "Create WorldEnvironment");

    /* Master + tonemap */
    result = lc_world_environment_set_post_processing_enabled(env, true);
    TEST_RESULT(result, "Set post-processing enabled");
    bool pp_enabled = false;
    result = lc_world_environment_get_post_processing_enabled(env, &pp_enabled);
    TEST_RESULT(result, "Get post-processing enabled");
    TEST_ASSERT(pp_enabled == true, "Post-processing enabled round-trips");

    result = lc_world_environment_set_tonemap_mode(env, LC_TONEMAP_ACES);
    TEST_RESULT(result, "Set tonemap mode");
    LCTonemapMode tm = LC_TONEMAP_LINEAR;
    result = lc_world_environment_get_tonemap_mode(env, &tm);
    TEST_RESULT(result, "Get tonemap mode");
    TEST_ASSERT(tm == LC_TONEMAP_ACES, "Tonemap mode round-trips");

    result = lc_world_environment_set_exposure(env, 1.5f);
    TEST_RESULT(result, "Set exposure");
    float exposure = 0.0f;
    result = lc_world_environment_get_exposure(env, &exposure);
    TEST_RESULT(result, "Get exposure");
    TEST_ASSERT(ext_float_eq(exposure, 1.5f), "Exposure round-trips");

    /* Bloom */
    result = lc_world_environment_set_bloom_enabled(env, true);
    TEST_RESULT(result, "Set bloom enabled");
    result = lc_world_environment_set_bloom_intensity(env, 0.8f);
    TEST_RESULT(result, "Set bloom intensity");
    float bloom_intensity = 0.0f;
    result = lc_world_environment_get_bloom_intensity(env, &bloom_intensity);
    TEST_RESULT(result, "Get bloom intensity");
    TEST_ASSERT(ext_float_eq(bloom_intensity, 0.8f), "Bloom intensity round-trips");
    result = lc_world_environment_set_bloom_iterations(env, 5);
    TEST_RESULT(result, "Set bloom iterations");
    int bloom_iters = 0;
    result = lc_world_environment_get_bloom_iterations(env, &bloom_iters);
    TEST_RESULT(result, "Get bloom iterations");
    TEST_ASSERT(bloom_iters == 5, "Bloom iterations round-trips");

    /* SSAO */
    result = lc_world_environment_set_ssao_enabled(env, true);
    TEST_RESULT(result, "Set SSAO enabled");
    result = lc_world_environment_set_ssao_radius(env, 0.75f);
    TEST_RESULT(result, "Set SSAO radius");
    float ssao_radius = 0.0f;
    result = lc_world_environment_get_ssao_radius(env, &ssao_radius);
    TEST_RESULT(result, "Get SSAO radius");
    TEST_ASSERT(ext_float_eq(ssao_radius, 0.75f), "SSAO radius round-trips");
    result = lc_world_environment_set_ssao_samples(env, 32);
    TEST_RESULT(result, "Set SSAO samples");
    int ssao_samples = 0;
    result = lc_world_environment_get_ssao_samples(env, &ssao_samples);
    TEST_RESULT(result, "Get SSAO samples");
    TEST_ASSERT(ssao_samples == 32, "SSAO samples round-trips");

    /* Color grading */
    result = lc_world_environment_set_color_grading_enabled(env, true);
    TEST_RESULT(result, "Set color grading enabled");
    result = lc_world_environment_set_saturation(env, 1.2f);
    TEST_RESULT(result, "Set saturation");
    float saturation = 0.0f;
    result = lc_world_environment_get_saturation(env, &saturation);
    TEST_RESULT(result, "Get saturation");
    TEST_ASSERT(ext_float_eq(saturation, 1.2f), "Saturation round-trips");
    result = lc_world_environment_set_color_filter(env, lc_color(0.9f, 0.8f, 0.7f, 0.5f));
    TEST_RESULT(result, "Set color filter");
    LCColor filter = {0.0f, 0.0f, 0.0f, 0.0f};
    result = lc_world_environment_get_color_filter(env, &filter);
    TEST_RESULT(result, "Get color filter");
    TEST_ASSERT(ext_color_eq(filter, lc_color(0.9f, 0.8f, 0.7f, 0.5f)), "Color filter round-trips");

    /* Vignette */
    result = lc_world_environment_set_vignette_enabled(env, true);
    TEST_RESULT(result, "Set vignette enabled");
    result = lc_world_environment_set_vignette_intensity(env, 0.6f);
    TEST_RESULT(result, "Set vignette intensity");
    float vig = 0.0f;
    result = lc_world_environment_get_vignette_intensity(env, &vig);
    TEST_RESULT(result, "Get vignette intensity");
    TEST_ASSERT(ext_float_eq(vig, 0.6f), "Vignette intensity round-trips");

    /* Chromatic aberration + film grain */
    result = lc_world_environment_set_chromatic_aberration_enabled(env, true);
    TEST_RESULT(result, "Set chromatic aberration enabled");
    result = lc_world_environment_set_film_grain_intensity(env, 0.15f);
    TEST_RESULT(result, "Set film grain intensity");
    float grain = 0.0f;
    result = lc_world_environment_get_film_grain_intensity(env, &grain);
    TEST_RESULT(result, "Get film grain intensity");
    TEST_ASSERT(ext_float_eq(grain, 0.15f), "Film grain intensity round-trips");

    /* Overlay */
    result = lc_world_environment_set_overlay_texture(env, "res://overlays/dirt.png");
    TEST_RESULT(result, "Set overlay texture");
    char ov_path[512];
    result = lc_world_environment_get_overlay_texture(env, ov_path, sizeof(ov_path));
    TEST_RESULT(result, "Get overlay texture");
    TEST_ASSERT(strcmp(ov_path, "res://overlays/dirt.png") == 0, "Overlay texture round-trips");
    result = lc_world_environment_set_overlay_blend_mode(env, LC_OVERLAY_SCREEN);
    TEST_RESULT(result, "Set overlay blend mode");
    LCOverlayBlendMode ovb = LC_OVERLAY_NORMAL;
    result = lc_world_environment_get_overlay_blend_mode(env, &ovb);
    TEST_RESULT(result, "Get overlay blend mode");
    TEST_ASSERT(ovb == LC_OVERLAY_SCREEN, "Overlay blend mode round-trips");

    /* Flip-Y override */
    result = lc_world_environment_set_post_flip_y(env, LC_FLIPY_ON);
    TEST_RESULT(result, "Set post flip-Y");
    LCFlipYMode flipy = LC_FLIPY_AUTO;
    result = lc_world_environment_get_post_flip_y(env, &flipy);
    TEST_RESULT(result, "Get post flip-Y");
    TEST_ASSERT(flipy == LC_FLIPY_ON, "Post flip-Y round-trips");

    lc_component_destroy(env);

    SECTION_END("Rendering: WorldEnvironment post-processing");
}

/* ============================================================================
 * NodeScatter - remaining getters (area offset/surface/rotation/debug color)
 * ============================================================================ */

static void test_ext_node_scatter(void) {
    SECTION_START("Rendering: NodeScatter (extended)");

    LCComponentHandle scatter = NULL;
    LCResult result = lc_node_scatter_create("ExtNodeScatter", &scatter);
    TEST_RESULT(result, "Create NodeScatter");

    result = lc_node_scatter_set_area_offset(scatter, lc_vec3(1.0f, 2.0f, 3.0f));
    TEST_RESULT(result, "Set area offset");

    LCVec3 area_offset = {0.0f, 0.0f, 0.0f};
    result = lc_node_scatter_get_area_offset(scatter, &area_offset);
    TEST_RESULT(result, "Get area offset");
    TEST_ASSERT(ext_float_eq(area_offset.x, 1.0f) && ext_float_eq(area_offset.y, 2.0f) &&
                ext_float_eq(area_offset.z, 3.0f), "Area offset round-trips");

    result = lc_node_scatter_set_surface_offset(scatter, 0.25f);
    TEST_RESULT(result, "Set surface offset");

    float surface_offset = 0.0f;
    result = lc_node_scatter_get_surface_offset(scatter, &surface_offset);
    TEST_RESULT(result, "Get surface offset");
    TEST_ASSERT(ext_float_eq(surface_offset, 0.25f), "Surface offset round-trips");

    result = lc_node_scatter_set_rotation_variation(scatter, lc_vec3(10.0f, 20.0f, 30.0f));
    TEST_RESULT(result, "Set rotation variation");

    LCVec3 rotation_variation = {0.0f, 0.0f, 0.0f};
    result = lc_node_scatter_get_rotation_variation(scatter, &rotation_variation);
    TEST_RESULT(result, "Get rotation variation");
    TEST_ASSERT(ext_float_eq(rotation_variation.x, 10.0f) && ext_float_eq(rotation_variation.y, 20.0f) &&
                ext_float_eq(rotation_variation.z, 30.0f), "Rotation variation round-trips");

    result = lc_node_scatter_set_debug_color(scatter, lc_color(0.9f, 0.1f, 0.5f, 1.0f));
    TEST_RESULT(result, "Set debug color");

    LCColor debug_color = {0.0f, 0.0f, 0.0f, 0.0f};
    result = lc_node_scatter_get_debug_color(scatter, &debug_color);
    TEST_RESULT(result, "Get debug color");
    TEST_ASSERT(ext_color_eq(debug_color, lc_color(0.9f, 0.1f, 0.5f, 1.0f)), "Debug color round-trips");

    lc_component_destroy(scatter);

    SECTION_END("Rendering: NodeScatter (extended)");
}

/* ============================================================================
 * Camera2D - ortho size / aspect ratio getters + active toggle
 * ============================================================================ */

static void test_ext_camera2d(void) {
    SECTION_START("Camera: Camera2D (extended)");

    LCNodeHandle camera = NULL;
    LCResult result = lc_camera2d_create("ExtCamera2D", &camera);
    TEST_RESULT(result, "Create Camera2D");

    result = lc_camera2d_set_ortho_size(camera, 720.0f);
    TEST_RESULT(result, "Set ortho size");

    float ortho_size = 0.0f;
    result = lc_camera2d_get_ortho_size(camera, &ortho_size);
    TEST_RESULT(result, "Get ortho size");
    TEST_ASSERT(ext_float_eq(ortho_size, 720.0f), "Ortho size round-trips");

    result = lc_camera2d_set_aspect_ratio(camera, 1.7777f);
    TEST_RESULT(result, "Set aspect ratio");

    float aspect_ratio = 0.0f;
    result = lc_camera2d_get_aspect_ratio(camera, &aspect_ratio);
    TEST_RESULT(result, "Get aspect ratio");
    TEST_ASSERT(ext_float_eq(aspect_ratio, 1.7777f), "Aspect ratio round-trips");

    result = lc_camera2d_set_active(camera, true);
    TEST_RESULT(result, "Set active");

    bool active = false;
    result = lc_camera2d_is_active(camera, &active);
    TEST_RESULT(result, "Get active");
    TEST_ASSERT(active == true, "Camera2D is active");

    lc_node_destroy(camera);

    SECTION_END("Camera: Camera2D (extended)");
}

/* ============================================================================
 * Camera3D - ortho size getter
 * ============================================================================ */

static void test_ext_camera3d(void) {
    SECTION_START("Camera: Camera3D (extended)");

    LCNodeHandle camera = NULL;
    LCResult result = lc_camera3d_create("ExtCamera3D", &camera);
    TEST_RESULT(result, "Create Camera3D");

    result = lc_camera3d_set_ortho_size(camera, 12.5f);
    TEST_RESULT(result, "Set ortho size");

    float ortho_size = 0.0f;
    result = lc_camera3d_get_ortho_size(camera, &ortho_size);
    TEST_RESULT(result, "Get ortho size");
    TEST_ASSERT(ext_float_eq(ortho_size, 12.5f), "Ortho size round-trips");

    lc_node_destroy(camera);

    SECTION_END("Camera: Camera3D (extended)");
}

/* ============================================================================
 * CameraUI - origin / scale factor / active / position getters
 * ============================================================================ */

static void test_ext_camera_ui(void) {
    SECTION_START("Camera: CameraUI (extended)");

    LCNodeHandle camera = NULL;
    LCResult result = lc_camera_ui_create("ExtCameraUI", &camera);
    TEST_RESULT(result, "Create CameraUI");

    result = lc_camera_ui_set_origin(camera, lc_vec2(0.5f, 0.5f));
    TEST_RESULT(result, "Set origin");

    LCVec2 origin = {0.0f, 0.0f};
    result = lc_camera_ui_get_origin(camera, &origin);
    TEST_RESULT(result, "Get origin");
    TEST_ASSERT(ext_float_eq(origin.x, 0.5f) && ext_float_eq(origin.y, 0.5f), "Origin round-trips");

    result = lc_camera_ui_set_scale_factor(camera, 2.0f);
    TEST_RESULT(result, "Set scale factor");

    float scale_factor = 0.0f;
    result = lc_camera_ui_get_scale_factor(camera, &scale_factor);
    TEST_RESULT(result, "Get scale factor");
    TEST_ASSERT(ext_float_eq(scale_factor, 2.0f), "Scale factor round-trips");

    result = lc_camera_ui_set_position(camera, lc_vec2(100.0f, 200.0f));
    TEST_RESULT(result, "Set position");

    LCVec2 position = {0.0f, 0.0f};
    result = lc_camera_ui_get_position(camera, &position);
    TEST_RESULT(result, "Get position");
    TEST_ASSERT(ext_float_eq(position.x, 100.0f) && ext_float_eq(position.y, 200.0f), "Position round-trips");

    result = lc_camera_ui_set_rotation(camera, 1.5f);
    TEST_RESULT(result, "Set rotation");

    float rotation = 0.0f;
    result = lc_camera_ui_get_rotation(camera, &rotation);
    TEST_RESULT(result, "Get rotation");
    TEST_ASSERT(ext_float_eq(rotation, 1.5f), "Rotation round-trips");

    result = lc_camera_ui_set_zoom(camera, 2.5f);
    TEST_RESULT(result, "Set zoom");

    float ui_zoom = 0.0f;
    result = lc_camera_ui_get_zoom(camera, &ui_zoom);
    TEST_RESULT(result, "Get zoom");
    TEST_ASSERT(ext_float_eq(ui_zoom, 2.5f), "Zoom round-trips");

    result = lc_camera_ui_set_active(camera, true);
    TEST_RESULT(result, "Set active");

    bool active = false;
    result = lc_camera_ui_is_active(camera, &active);
    TEST_RESULT(result, "Get active");
    TEST_ASSERT(active == true, "CameraUI is active");

    lc_node_destroy(camera);

    SECTION_END("Camera: CameraUI (extended)");
}

/* ============================================================================
 * Input - just-pressed/just-released mouse/gamepad/action query helpers
 * ============================================================================ */

static void test_ext_input(void) {
    SECTION_START("Input: API Availability Check (extended)");

    bool state = lc_input_is_mouse_button_just_pressed(LC_MOUSE_BUTTON_LEFT);
    printf("    mouse left just pressed: %s\n", state ? "yes" : "no");
    TEST_ASSERT(state == false, "lc_input_is_mouse_button_just_pressed() returns defined value");

    state = lc_input_is_mouse_button_just_released(LC_MOUSE_BUTTON_RIGHT);
    TEST_ASSERT(state == false, "lc_input_is_mouse_button_just_released() returns defined value");

    state = lc_input_is_gamepad_button_just_pressed(LC_GAMEPAD_BUTTON_A, 0);
    TEST_ASSERT(state == false, "lc_input_is_gamepad_button_just_pressed() returns defined value");

    state = lc_input_is_gamepad_button_just_released(LC_GAMEPAD_BUTTON_B, 0);
    TEST_ASSERT(state == false, "lc_input_is_gamepad_button_just_released() returns defined value");

    state = lc_input_is_action_just_pressed("ext_jump");
    TEST_ASSERT(state == false, "lc_input_is_action_just_pressed() returns defined value");

    state = lc_input_is_action_just_released("ext_jump");
    TEST_ASSERT(state == false, "lc_input_is_action_just_released() returns defined value");

    SECTION_END("Input: API Availability Check (extended)");
}
