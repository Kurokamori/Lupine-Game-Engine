/* Miscellaneous extended C API coverage (capi_test_suite.c fragment).
 *
 * Fills coverage gaps across several small surfaces that the other suite
 * fragments do not touch:
 *   - ParallaxBackground / ParallaxLayer components (deterministic config
 *     round-trips; tolerant for the camera/scene-driven update + applied/home).
 *   - Active-scene control on the runtime SceneManager (change/reload/current),
 *     which has no manager in this headless harness -> tolerant results.
 *   - One-call scene / prefab instantiation (needs an attach parent / loader).
 *   - Interface queries on components, the interface name table, and the
 *     scene first-by-interface lookup.
 *   - Synchronous + async archetype access (missing files -> graceful failure).
 *   - Table-scoped formatted + full plural localization.
 *   - NavigationRegion2D hole add/clear.
 *   - create_node_child, node call_deferred, and the profiler last-frame JSON.
 *
 * This file is #included into capi_test_suite.c. It defines no main(),
 * includes no headers, and redefines no macros.
 */

static int ext_misc_contains(const char* haystack, const char* needle) {
    return haystack && needle && strstr(haystack, needle) != NULL;
}

static void test_ext_misc_ext_parallax_background(void) {
    SECTION_START("Misc Ext: ParallaxBackground");

    LCNodeHandle node = NULL;
    LCResult result = lc_node_create(LC_NODE_2D, "ExtParallaxBg", &node);
    TEST_RESULT(result, "Create node for ParallaxBackground");

    LCComponentHandle bg = NULL;
    result = lc_parallax_background_create(node, &bg);
    TEST_RESULT(result, "Create ParallaxBackground component");

    result = lc_parallax_background_set_scroll_scale(bg, lc_vec2(0.5f, 0.25f));
    TEST_RESULT(result, "Set scroll scale (0.5,0.25)");
    LCVec2 scale = { 0.0f, 0.0f };
    result = lc_parallax_background_get_scroll_scale(bg, &scale);
    TEST_RESULT(result, "Get scroll scale");
    TEST_ASSERT(scale.x > 0.49f && scale.x < 0.51f && scale.y > 0.24f && scale.y < 0.26f,
                "Scroll scale round-trip (0.5,0.25)");

    result = lc_parallax_background_set_scroll_base_offset(bg, lc_vec2(16.0f, -8.0f));
    TEST_RESULT(result, "Set scroll base offset (16,-8)");
    LCVec2 baseOffset = { 0.0f, 0.0f };
    result = lc_parallax_background_get_scroll_base_offset(bg, &baseOffset);
    TEST_RESULT(result, "Get scroll base offset");
    TEST_ASSERT(baseOffset.x > 15.99f && baseOffset.x < 16.01f &&
                baseOffset.y > -8.01f && baseOffset.y < -7.99f,
                "Scroll base offset round-trip (16,-8)");

    result = lc_parallax_background_set_ignore_camera_scroll(bg, true);
    TEST_RESULT(result, "Set ignore camera scroll true");
    bool ignore = false;
    result = lc_parallax_background_get_ignore_camera_scroll(bg, &ignore);
    TEST_RESULT(result, "Get ignore camera scroll");
    TEST_ASSERT(ignore == true, "Ignore camera scroll round-trip true");

    result = lc_parallax_background_set_scroll_offset(bg, lc_vec2(120.0f, 64.0f));
    TEST_RESULT(result, "Set scroll offset (120,64)");
    LCVec2 scroll = { 0.0f, 0.0f };
    result = lc_parallax_background_get_scroll_offset(bg, &scroll);
    TEST_RESULT(result, "Get scroll offset");
    TEST_ASSERT(scroll.x > 119.99f && scroll.x < 120.01f &&
                scroll.y > 63.99f && scroll.y < 64.01f,
                "Scroll offset round-trip (120,64)");

    /* update_layers needs an attached scene/camera and live layer children to do
     * meaningful work; it must still report a defined result without one. */
    result = lc_parallax_background_update_layers(bg);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_INVALID_HANDLE,
                "update_layers defined result without a scene");

    /* applied scroll is computed during update_layers; with manual scroll and the
     * camera ignored it should reflect the offset, but stay tolerant since the
     * exact value depends on layer/camera state. */
    LCVec2 applied = { -1.0f, -1.0f };
    result = lc_parallax_background_get_applied_scroll(bg, &applied);
    TEST_RESULT(result, "Get applied scroll");

    /* NULL-pointer guards on the out-parameter accessors. */
    result = lc_parallax_background_get_scroll_scale(bg, NULL);
    TEST_ASSERT(result != LC_SUCCESS, "get_scroll_scale(NULL out) fails gracefully");
    result = lc_parallax_background_get_applied_scroll(bg, NULL);
    TEST_ASSERT(result != LC_SUCCESS, "get_applied_scroll(NULL out) fails gracefully");

    lc_component_destroy(bg);
    lc_node_destroy(node);
    SECTION_END("Misc Ext: ParallaxBackground");
}

static void test_ext_misc_ext_parallax_layer(void) {
    SECTION_START("Misc Ext: ParallaxLayer");

    LCNodeHandle node = NULL;
    LCResult result = lc_node_create(LC_NODE_2D, "ExtParallaxLayer", &node);
    TEST_RESULT(result, "Create node for ParallaxLayer");

    LCComponentHandle layer = NULL;
    result = lc_parallax_layer_create(node, &layer);
    TEST_RESULT(result, "Create ParallaxLayer component");

    result = lc_parallax_layer_set_motion_scale(layer, lc_vec2(0.3f, 0.7f));
    TEST_RESULT(result, "Set motion scale (0.3,0.7)");
    LCVec2 scale = { 0.0f, 0.0f };
    result = lc_parallax_layer_get_motion_scale(layer, &scale);
    TEST_RESULT(result, "Get motion scale");
    TEST_ASSERT(scale.x > 0.29f && scale.x < 0.31f && scale.y > 0.69f && scale.y < 0.71f,
                "Motion scale round-trip (0.3,0.7)");

    result = lc_parallax_layer_set_motion_offset(layer, lc_vec2(-12.0f, 5.0f));
    TEST_RESULT(result, "Set motion offset (-12,5)");
    LCVec2 offset = { 0.0f, 0.0f };
    result = lc_parallax_layer_get_motion_offset(layer, &offset);
    TEST_RESULT(result, "Get motion offset");
    TEST_ASSERT(offset.x > -12.01f && offset.x < -11.99f && offset.y > 4.99f && offset.y < 5.01f,
                "Motion offset round-trip (-12,5)");

    result = lc_parallax_layer_set_motion_mirroring(layer, lc_vec2(512.0f, 0.0f));
    TEST_RESULT(result, "Set motion mirroring (512,0)");
    LCVec2 mirroring = { 0.0f, 0.0f };
    result = lc_parallax_layer_get_motion_mirroring(layer, &mirroring);
    TEST_RESULT(result, "Get motion mirroring");
    TEST_ASSERT(mirroring.x > 511.99f && mirroring.x < 512.01f &&
                mirroring.y > -0.01f && mirroring.y < 0.01f,
                "Motion mirroring round-trip (512,0)");

    /* capture/home need the layer attached to a positioned Node2D to be exact;
     * the call must still succeed and the getter return a defined result. */
    result = lc_parallax_layer_capture_home_position(layer);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_INVALID_HANDLE,
                "capture_home_position defined result");

    LCVec2 home = { -1.0f, -1.0f };
    result = lc_parallax_layer_get_home_position(layer, &home);
    TEST_RESULT(result, "Get home position");

    result = lc_parallax_layer_get_motion_scale(layer, NULL);
    TEST_ASSERT(result != LC_SUCCESS, "get_motion_scale(NULL out) fails gracefully");
    result = lc_parallax_layer_get_home_position(layer, NULL);
    TEST_ASSERT(result != LC_SUCCESS, "get_home_position(NULL out) fails gracefully");

    result = lc_parallax_layer_apply_scroll(layer, lc_vec2(32.0f, 0.0f));
    TEST_RESULT(result, "Apply scroll displacement");

    lc_component_destroy(layer);
    lc_node_destroy(node);
    SECTION_END("Misc Ext: ParallaxLayer");
}

static void test_ext_misc_ext_scene_control(void) {
    SECTION_START("Misc Ext: Active Scene Control");

    /* No runtime SceneManager is active in the headless harness, so these act on
     * a singleton that may be absent. Each must report a defined result. */
    LCResult result = lc_scene_change("res://__missing_ext_scene.scene");
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED,
                "scene_change defined result without a scene manager");

    result = lc_scene_change(NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "scene_change NULL path guard");

    result = lc_scene_reload();
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED,
                "scene_reload defined result without a scene manager");

    LCSceneHandle current = (LCSceneHandle)0x1;
    result = lc_scene_get_current(&current);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED,
                "scene_get_current defined result without a scene manager");

    result = lc_scene_get_current(NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "scene_get_current NULL out guard");

    char path[256];
    memset(path, 0, sizeof(path));
    result = lc_scene_get_current_path(path, sizeof(path));
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED,
                "scene_get_current_path defined result without a scene manager");

    result = lc_scene_get_current_path(NULL, sizeof(path));
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "scene_get_current_path NULL buffer guard");
    result = lc_scene_get_current_path(path, 0);
    TEST_ASSERT(result == LC_ERROR_INVALID_PARAMETER, "scene_get_current_path zero-size guard");

    SECTION_END("Misc Ext: Active Scene Control");
}

static void test_ext_misc_ext_scene_instance(void) {
    SECTION_START("Misc Ext: One-call Instantiation");

    /* Attach to an explicit parent so the call does not depend on a current
     * scene; the referenced file does not exist so loading is expected to fail
     * gracefully (never crash). */
    LCNodeHandle parent = NULL;
    LCResult result = lc_node_create(LC_NODE_2D, "ExtInstantiateParent", &parent);
    TEST_RESULT(result, "Create attach parent for instantiation");

    LCNodeHandle out_scene = (LCNodeHandle)0x1;
    result = lc_instantiate_scene("res://__missing_ext.scene", parent, &out_scene);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED,
                "instantiate_scene defined result for missing file");

    result = lc_instantiate_scene(NULL, parent, &out_scene);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "instantiate_scene NULL path guard");
    result = lc_instantiate_scene("res://__missing_ext.scene", parent, NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "instantiate_scene NULL out guard");

    LCNodeHandle out_prefab = (LCNodeHandle)0x1;
    result = lc_instantiate_prefab("res://__missing_ext.prefab", parent, &out_prefab);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED,
                "instantiate_prefab defined result for missing file");

    result = lc_instantiate_prefab(NULL, parent, &out_prefab);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "instantiate_prefab NULL path guard");
    result = lc_instantiate_prefab("res://__missing_ext.prefab", parent, NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "instantiate_prefab NULL out guard");

    lc_node_destroy(parent);
    SECTION_END("Misc Ext: One-call Instantiation");
}

static void test_ext_misc_ext_interface(void) {
    SECTION_START("Misc Ext: Interface Queries");

    LCComponentHandle component = NULL;
    LCResult result = lc_directional_light3d_create("ExtIfaceLight", &component);
    TEST_RESULT(result, "Create component for interface queries");

    /* A built-in component declares no script interfaces; the list should be an
     * empty JSON array and implements should be false. */
    char* ifaces = NULL;
    result = lc_component_get_interfaces_json(component, &ifaces);
    TEST_RESULT(result, "lc_component_get_interfaces_json() succeeds");
    TEST_ASSERT(ifaces != NULL, "component interfaces JSON is non-null");
    if (ifaces) lc_free(ifaces);

    bool implements = true;
    result = lc_component_implements_interface(component, "Damageable", &implements);
    TEST_RESULT(result, "lc_component_implements_interface() succeeds");
    TEST_ASSERT(implements == false, "built-in component implements no script interface");

    result = lc_component_implements_interface(component, "Damageable", NULL);
    TEST_ASSERT(result != LC_SUCCESS, "implements_interface(NULL out) fails gracefully");

    /* Interface name table: enumerate whatever the registry holds. */
    int count = -1;
    result = lc_interface_get_count(&count);
    TEST_RESULT(result, "lc_interface_get_count() succeeds");
    TEST_ASSERT(count >= 0, "interface count is non-negative");

    char name[128];
    memset(name, 0, sizeof(name));
    result = lc_interface_get_name_at(0, name, (uint32_t)sizeof(name));
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_INVALID_PARAMETER ||
                result == LC_ERROR_NOT_FOUND,
                "interface_get_name_at(0) defined result");

    result = lc_interface_get_name_at(99999, name, (uint32_t)sizeof(name));
    TEST_ASSERT(result != LC_SUCCESS, "interface_get_name_at(out-of-range) fails gracefully");

    /* First-by-interface lookup with no active scene: defined result, NULL node. */
    LCNodeHandle found = (LCNodeHandle)0x1;
    result = lc_scene_get_first_node_by_interface("Damageable", &found);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_NOT_FOUND,
                "scene_get_first_node_by_interface defined result without a scene");

    lc_component_destroy(component);
    SECTION_END("Misc Ext: Interface Queries");
}

static void test_ext_misc_ext_async_archetype(void) {
    SECTION_START("Misc Ext: Async / Archetype Access");

    /* Invalidate the resolved-archetype caches; a maintenance no-op that must
     * always report a defined result. */
    LCResult result = lc_archetype_invalidate_caches();
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED,
                "archetype_invalidate_caches defined result");

    /* Synchronous load of a missing instance: graceful failure, no crash. */
    char* fields = NULL;
    result = lc_archetype_load_instance("res://__missing_ext.ares", &fields);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_INVALID_PARAMETER,
                "archetype_load_instance defined result for missing file");
    if (fields) lc_free(fields);

    result = lc_archetype_load_instance(NULL, &fields);
    TEST_ASSERT(result == LC_ERROR_INVALID_PARAMETER || result == LC_ERROR_NULL_POINTER,
                "archetype_load_instance NULL path guard");

    /* Submit an async instance load and drive it to completion, then exercise
     * the class-name and instance-path delivery accessors on a failed load. */
    LCAsyncHandle handle = 0;
    result = lc_async_load_archetype_instance("res://__missing_ext.ares",
                                              LC_ASYNC_PRIORITY_NORMAL, &handle);
    TEST_RESULT(result, "Submit async instance load for missing file");

    for (int i = 0; i < 200; ++i) {
        bool complete = false;
        lc_async_is_complete(handle, &complete);
        if (complete) {
            break;
        }
        lc_async_pump();
    }

    char class_name[128];
    memset(class_name, 0, sizeof(class_name));
    result = lc_async_get_class_name(handle, class_name, (uint32_t)sizeof(class_name));
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_NOT_FOUND,
                "async_get_class_name defined result after completion");

    char inst_path[256];
    memset(inst_path, 0, sizeof(inst_path));
    result = lc_async_get_instance_path(handle, inst_path, (uint32_t)sizeof(inst_path));
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_NOT_FOUND,
                "async_get_instance_path defined result on failed load");

    lc_async_forget(handle);
    lc_async_reset();

    SECTION_END("Misc Ext: Async / Archetype Access");
}

static void test_ext_misc_ext_localization(void) {
    SECTION_START("Misc Ext: Localization Format / Plural");

    char buffer[512];
    const char* arg_names[2] = { "name", "place" };
    const char* arg_values[2] = { "Lupine", "Engine" };

    buffer[0] = '\0';
    LCResult result = lc_localization_tr_format_in_table("ui", "greeting_test_key",
                                                         arg_names, arg_values, 2,
                                                         buffer, sizeof(buffer));
    TEST_RESULT(result, "tr_format_in_table returns success");
    TEST_ASSERT(buffer[0] != '\0', "tr_format_in_table writes a non-empty string");

    buffer[0] = '\0';
    result = lc_localization_tr_format_in_table("ui", "greeting_test_key",
                                                NULL, NULL, 0, buffer, sizeof(buffer));
    TEST_RESULT(result, "tr_format_in_table with zero args returns success");

    buffer[0] = '\0';
    result = lc_localization_tr_plural_full("ui", "item_count_test_key", 1,
                                            arg_names, arg_values, 2, buffer, sizeof(buffer));
    TEST_RESULT(result, "tr_plural_full (count=1) returns success");
    TEST_ASSERT(buffer[0] != '\0', "tr_plural_full singular writes a non-empty string");

    buffer[0] = '\0';
    result = lc_localization_tr_plural_full("ui", "item_count_test_key", 5,
                                            NULL, NULL, 0, buffer, sizeof(buffer));
    TEST_RESULT(result, "tr_plural_full (count=5, no args) returns success");
    TEST_ASSERT(buffer[0] != '\0', "tr_plural_full plural writes a non-empty string");

    SECTION_END("Misc Ext: Localization Format / Plural");
}

static void test_ext_misc_ext_navigation_holes(void) {
    SECTION_START("Misc Ext: NavigationRegion2D Holes");

    LCComponentHandle region = NULL;
    LCResult result = lc_navigation_region2d_create("ExtNavHoleRegion", &region);
    TEST_RESULT(result, "Create NavigationRegion2D component");

    if (region != NULL) {
        LCVec2 outline[4];
        outline[0] = lc_vec2(0.0f, 0.0f);
        outline[1] = lc_vec2(200.0f, 0.0f);
        outline[2] = lc_vec2(200.0f, 200.0f);
        outline[3] = lc_vec2(0.0f, 200.0f);
        result = lc_navigation_region2d_set_outline(region, outline, 4);
        TEST_RESULT(result, "Set region outline");

        LCVec2 hole[4];
        hole[0] = lc_vec2(80.0f, 80.0f);
        hole[1] = lc_vec2(120.0f, 80.0f);
        hole[2] = lc_vec2(120.0f, 120.0f);
        hole[3] = lc_vec2(80.0f, 120.0f);
        result = lc_navigation_region2d_add_hole(region, hole, 4);
        TEST_RESULT(result, "lc_navigation_region2d_add_hole()");

        result = lc_navigation_region2d_bake(region);
        TEST_RESULT(result, "Bake region with hole");

        result = lc_navigation_region2d_clear_holes(region);
        TEST_RESULT(result, "lc_navigation_region2d_clear_holes()");

        result = lc_navigation_region2d_bake(region);
        TEST_RESULT(result, "Bake region after clearing holes");

        result = lc_navigation_region2d_add_hole(NULL, hole, 4);
        TEST_ASSERT(result != LC_SUCCESS, "add_hole(NULL component) fails gracefully");

        lc_component_destroy(region);
    }

    SECTION_END("Misc Ext: NavigationRegion2D Holes");
}

static void test_ext_misc_ext_misc(void) {
    SECTION_START("Misc Ext: create_node_child / call_deferred / profiler");

    /* create_node_child attaches the new node to the given parent, or the runtime
     * scene root when NULL. With an explicit parent and no scene manager it should
     * still resolve a parent; tolerant because the type registry / scene manager
     * may be absent in this harness. */
    LCNodeHandle parent = NULL;
    LCResult result = lc_node_create(LC_NODE_2D, "ExtCreateChildParent", &parent);
    TEST_RESULT(result, "Create parent for create_node_child");

    LCNodeHandle child = (LCNodeHandle)0x1;
    result = lc_create_node_child("Node2D", "ExtCreatedChild", parent, &child);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_NOT_FOUND ||
                result == LC_ERROR_OPERATION_FAILED,
                "create_node_child defined result");

    result = lc_create_node_child(NULL, "X", parent, &child);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "create_node_child NULL type guard");
    result = lc_create_node_child("Node2D", "X", parent, NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "create_node_child NULL out guard");

    /* call_deferred queues a method invocation for the next deferred flush. */
    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_BASE, "ExtDeferredNode", &node);
    TEST_RESULT(result, "Create node for call_deferred");

    result = lc_node_call_deferred(node, "queue_free", "[]");
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_NOT_FOUND,
                "call_deferred defined result");

    result = lc_node_call_deferred(node, "set_value", NULL);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_NOT_FOUND,
                "call_deferred with NULL args defined result");

    /* Profiler last-frame snapshot: returns an (possibly empty) JSON object. */
    char* frame_json = NULL;
    result = lc_profiler_get_last_frame_json(&frame_json);
    TEST_RESULT(result, "lc_profiler_get_last_frame_json() succeeds");
    TEST_ASSERT(frame_json != NULL, "profiler last-frame JSON is non-null");
    TEST_ASSERT(ext_misc_contains(frame_json, "{"), "profiler last-frame JSON is an object");
    if (frame_json) lc_free(frame_json);

    result = lc_profiler_get_last_frame_json(NULL);
    TEST_ASSERT(result != LC_SUCCESS, "profiler_get_last_frame_json(NULL out) fails gracefully");

    lc_node_destroy(node);
    lc_node_destroy(parent);
    SECTION_END("Misc Ext: create_node_child / call_deferred / profiler");
}
