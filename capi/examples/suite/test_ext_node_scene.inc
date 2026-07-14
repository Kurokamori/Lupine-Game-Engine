static int g_ext_ns_fire_count = 0;
static char g_ext_ns_last_args[256];

static void ext_ns_signal_callback(const char* args_json, void* user_data) {
    g_ext_ns_fire_count++;
    if (args_json) {
        copy_bounded(g_ext_ns_last_args, sizeof(g_ext_ns_last_args), args_json);
    }
    if (user_data) {
        *(int*)user_data += 1;
    }
}

static void test_ext_nodescene_visibility(void) {
    SECTION_START("NodeScene Ext: Visibility");

    LCNodeHandle parent = NULL;
    LCResult result = lc_node_create(LC_NODE_2D, "VisParent", &parent);
    TEST_RESULT(result, "Create visibility parent node");

    LCNodeHandle child = NULL;
    result = lc_node_create(LC_NODE_2D, "VisChild", &child);
    TEST_RESULT(result, "Create visibility child node");

    result = lc_node_add_child(parent, child);
    TEST_RESULT(result, "Attach child for visibility test");

    TEST_ASSERT(lc_node_is_visible(parent), "Parent visible by default");
    TEST_ASSERT(lc_node_is_visible(child), "Child visible by default");

    result = lc_node_set_visible(child, false);
    TEST_RESULT(result, "Set child not visible");
    TEST_ASSERT(!lc_node_is_visible(child), "Child reports not visible");

    result = lc_node_set_visible(child, true);
    TEST_RESULT(result, "Restore child visible");
    TEST_ASSERT(lc_node_is_visible_in_hierarchy(child), "Child visible in hierarchy when parent visible");

    result = lc_node_set_visible(parent, false);
    TEST_RESULT(result, "Hide parent");
    TEST_ASSERT(!lc_node_is_visible_in_hierarchy(child),
                "Child not visible in hierarchy when parent hidden");

    result = lc_node_set_visible(parent, true);
    TEST_RESULT(result, "Restore parent visible");
    TEST_ASSERT(lc_node_is_visible_in_hierarchy(child),
                "Child visible in hierarchy after parent restored");

    lc_node_destroy(parent);

    SECTION_END("NodeScene Ext: Visibility");
}

static void test_ext_nodescene_child_ordering(void) {
    SECTION_START("NodeScene Ext: Child Ordering");

    LCNodeHandle parent = NULL;
    LCResult result = lc_node_create(LC_NODE_BASE, "OrderParent", &parent);
    TEST_RESULT(result, "Create ordering parent node");

    LCNodeHandle node_a = NULL;
    LCNodeHandle node_b = NULL;
    LCNodeHandle node_c = NULL;
    LCNodeHandle node_d = NULL;
    result = lc_node_create(LC_NODE_BASE, "NodeA", &node_a);
    TEST_RESULT(result, "Create NodeA");
    result = lc_node_create(LC_NODE_BASE, "NodeB", &node_b);
    TEST_RESULT(result, "Create NodeB");
    result = lc_node_create(LC_NODE_BASE, "NodeC", &node_c);
    TEST_RESULT(result, "Create NodeC");
    result = lc_node_create(LC_NODE_BASE, "NodeD", &node_d);
    TEST_RESULT(result, "Create NodeD");

    result = lc_node_add_child(parent, node_a);
    TEST_RESULT(result, "Add NodeA");
    result = lc_node_add_child(parent, node_b);
    TEST_RESULT(result, "Add NodeB");
    result = lc_node_add_child(parent, node_c);
    TEST_RESULT(result, "Add NodeC");

    size_t index = 0;
    result = lc_node_get_child_index(parent, node_b, &index);
    TEST_RESULT(result, "Get child index of NodeB");
    TEST_ASSERT(index == 1, "NodeB is at index 1");

    result = lc_node_get_sibling_index(node_c, &index);
    TEST_RESULT(result, "Get sibling index of NodeC");
    TEST_ASSERT(index == 2, "NodeC sibling index is 2");

    result = lc_node_insert_child(parent, node_d, 1);
    TEST_RESULT(result, "Insert NodeD at index 1");
    size_t count = 0;
    result = lc_node_get_child_count(parent, &count);
    TEST_RESULT(result, "Get child count after insert");
    TEST_ASSERT(count == 4, "Parent has 4 children after insert");
    result = lc_node_get_child_index(parent, node_d, &index);
    TEST_RESULT(result, "Get child index of inserted NodeD");
    TEST_ASSERT(index == 1, "NodeD inserted at index 1");

    LCNodeHandle at_one = NULL;
    result = lc_node_get_child(parent, 1, &at_one);
    TEST_RESULT(result, "Get child at index 1");
    TEST_ASSERT(at_one == node_d, "Child at index 1 is NodeD");

    result = lc_node_move_child(parent, node_d, 3);
    TEST_RESULT(result, "Move NodeD to index 3");
    result = lc_node_get_child_index(parent, node_d, &index);
    TEST_RESULT(result, "Get child index after move");
    TEST_ASSERT(index == 3, "NodeD moved to index 3");

    result = lc_node_set_sibling_index(node_a, 2);
    TEST_RESULT(result, "Set NodeA sibling index to 2");
    result = lc_node_get_sibling_index(node_a, &index);
    TEST_RESULT(result, "Get NodeA sibling index after reorder");
    TEST_ASSERT(index == 2, "NodeA sibling index is 2 after reorder");

    LCNodeHandle children[8];
    size_t got = 0;
    result = lc_node_get_children(parent, children, 8, &got);
    TEST_RESULT(result, "Get all children into array");
    TEST_ASSERT(got == 4, "Retrieved 4 children");

    size_t count_only = 0;
    result = lc_node_get_children(parent, NULL, 0, &count_only);
    TEST_RESULT(result, "Get children count via NULL array");
    TEST_ASSERT(count_only == 4, "Count-only query reports 4 children");

    result = lc_node_remove_child_by_name(parent, "NodeD");
    TEST_RESULT(result, "Remove child by name 'NodeD'");
    result = lc_node_get_child_count(parent, &count);
    TEST_RESULT(result, "Get child count after remove by name");
    TEST_ASSERT(count == 3, "Parent has 3 children after remove by name");

    size_t missing_index = 0;
    result = lc_node_get_child_index(parent, node_d, &missing_index);
    TEST_RESULT(result, "Query index of removed child returns success");
    TEST_ASSERT(missing_index == (size_t)-1, "Removed child reports index (size_t)-1");

    lc_node_destroy(node_d);
    lc_node_destroy(parent);

    SECTION_END("NodeScene Ext: Child Ordering");
}

static void test_ext_nodescene_unique_names(void) {
    SECTION_START("NodeScene Ext: Unique Names");

    LCNodeHandle root = NULL;
    LCResult result = lc_node_create(LC_NODE_BASE, "UniqueRoot", &root);
    TEST_RESULT(result, "Create unique-name scope root");

    LCNodeHandle branch = NULL;
    result = lc_node_create(LC_NODE_BASE, "Branch", &branch);
    TEST_RESULT(result, "Create intermediate branch node");

    LCNodeHandle target = NULL;
    result = lc_node_create(LC_NODE_BASE, "Special", &target);
    TEST_RESULT(result, "Create unique target node");

    result = lc_node_add_child(root, branch);
    TEST_RESULT(result, "Attach branch to root");
    result = lc_node_add_child(branch, target);
    TEST_RESULT(result, "Attach target under branch");

    TEST_ASSERT(!lc_node_is_unique_name_in_owner(target), "Target not unique by default");

    result = lc_node_set_unique_name_in_owner(target, true);
    TEST_RESULT(result, "Mark target as unique name in owner");
    TEST_ASSERT(lc_node_is_unique_name_in_owner(target), "Target reports unique name in owner");

    LCNodeHandle found = NULL;
    result = lc_node_resolve_unique_name(branch, "Special", &found);
    TEST_RESULT(result, "Resolve unique name 'Special'");
    TEST_ASSERT(found == target, "Resolved unique name to the target node");

    LCNodeHandle not_found = (LCNodeHandle)0x1;
    result = lc_node_resolve_unique_name(branch, "DoesNotExist", &not_found);
    TEST_RESULT(result, "Resolve missing unique name returns success");
    TEST_ASSERT(not_found == NULL, "Unresolved unique name yields NULL handle");

    result = lc_node_set_unique_name_in_owner(target, false);
    TEST_RESULT(result, "Clear unique-name flag on target");
    TEST_ASSERT(!lc_node_is_unique_name_in_owner(target), "Target no longer unique");

    lc_node_destroy(root);

    SECTION_END("NodeScene Ext: Unique Names");
}

static void test_ext_nodescene_node_connect(void) {
    SECTION_START("NodeScene Ext: Node-to-Node Connect");

    LCNodeHandle emitter = NULL;
    LCResult result = lc_node_create(LC_NODE_BASE, "ConnectEmitter", &emitter);
    TEST_RESULT(result, "Create emitter node");

    LCNodeHandle receiver = NULL;
    result = lc_node_create(LC_NODE_BASE, "ConnectReceiver", &receiver);
    TEST_RESULT(result, "Create receiver node");

    result = lc_node_add_user_signal(emitter, "pinged");
    TEST_RESULT(result, "Declare user signal 'pinged'");

    uint64_t conn = 0;
    result = lc_node_connect(emitter, "pinged", receiver, "on_pinged", LC_CONNECT_NONE, &conn);
    TEST_RESULT(result, "Connect signal to receiver method");
    TEST_ASSERT(conn != 0, "Node-to-node connection id is non-zero");

    bool connected = false;
    result = lc_node_is_connected(emitter, "pinged", &connected);
    TEST_RESULT(result, "Query node-to-node connection state");
    TEST_ASSERT(connected, "Emitter reports a live connection");

    result = lc_node_emit(emitter, "pinged", "[1]");
    TEST_RESULT(result, "Emit to method handler without crashing");

    result = lc_node_disconnect(emitter, "pinged", conn);
    TEST_RESULT(result, "Disconnect node-to-node connection");
    connected = true;
    result = lc_node_is_connected(emitter, "pinged", &connected);
    TEST_RESULT(result, "Query connection state after disconnect");
    TEST_ASSERT(!connected, "Emitter reports no connections after disconnect");

    uint64_t sub = 0;
    result = lc_event_subscribe("ext_ns_event", receiver, "on_event", LC_CONNECT_NONE, &sub);
    TEST_RESULT(result, "Subscribe receiver method to global event");
    TEST_ASSERT(sub != 0, "Event subscription id is non-zero");

    result = lc_event_emit("ext_ns_event", "[\"data\"]");
    TEST_RESULT(result, "Emit global event to method subscriber");

    result = lc_event_unsubscribe("ext_ns_event", sub);
    TEST_RESULT(result, "Unsubscribe method handler from global event");

    lc_node_destroy(emitter);
    lc_node_destroy(receiver);

    SECTION_END("NodeScene Ext: Node-to-Node Connect");
}

static void test_ext_nodescene_component_signals(void) {
    SECTION_START("NodeScene Ext: Component Signals");

    LCComponentHandle component = NULL;
    LCResult result = lc_directional_light3d_create("ExtNsSignalLight", &component);
    TEST_RESULT(result, "Create component for signal test");

    result = lc_component_add_user_signal(component, "charged");
    TEST_RESULT(result, "Declare component user signal 'charged'");

    g_ext_ns_fire_count = 0;
    g_ext_ns_last_args[0] = '\0';
    int user_counter = 0;

    uint64_t conn = 0;
    result = lc_component_connect_callback(component, "charged", ext_ns_signal_callback,
                                           &user_counter, LC_CONNECT_NONE, &conn);
    TEST_RESULT(result, "Connect native callback to component signal");
    TEST_ASSERT(conn != 0, "Component connection id is non-zero");

    bool connected = false;
    result = lc_component_is_connected(component, "charged", &connected);
    TEST_RESULT(result, "Query component connection state");
    TEST_ASSERT(connected, "Component reports a live connection");

    result = lc_component_emit(component, "charged", "[99, \"full\"]");
    TEST_RESULT(result, "Emit component signal with arguments");
    TEST_ASSERT(g_ext_ns_fire_count == 1, "Component callback fired exactly once");
    TEST_ASSERT(user_counter == 1, "Component user_data passed through");
    TEST_ASSERT(strstr(g_ext_ns_last_args, "99") != NULL, "Component callback received emitted argument");

    result = lc_component_disconnect(component, "charged", conn);
    TEST_RESULT(result, "Disconnect component callback");
    result = lc_component_emit(component, "charged", "[0]");
    TEST_RESULT(result, "Emit after component disconnect");
    TEST_ASSERT(g_ext_ns_fire_count == 1, "Disconnected component callback does not fire");

    LCNodeHandle receiver = NULL;
    result = lc_node_create(LC_NODE_BASE, "CompSignalReceiver", &receiver);
    TEST_RESULT(result, "Create receiver for component method connect");

    uint64_t method_conn = 0;
    result = lc_component_connect(component, "charged", receiver, "on_charged",
                                  LC_CONNECT_NONE, &method_conn);
    TEST_RESULT(result, "Connect component signal to node method");
    TEST_ASSERT(method_conn != 0, "Component method connection id is non-zero");

    result = lc_component_disconnect(component, "charged", method_conn);
    TEST_RESULT(result, "Disconnect component method connection");

    lc_node_destroy(receiver);
    result = lc_component_destroy(component);
    TEST_RESULT(result, "Destroy signal test component");

    SECTION_END("NodeScene Ext: Component Signals");
}

static void test_ext_nodescene_scene_lifecycle(void) {
    SECTION_START("NodeScene Ext: Scene Lifecycle");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("LifecycleScene", &scene);
    TEST_RESULT(result, "Create lifecycle scene");

    LCNodeHandle root = NULL;
    result = lc_node_create(LC_NODE_3D, "LifecycleRoot", &root);
    TEST_RESULT(result, "Create lifecycle root node");
    result = lc_scene_set_root(scene, root);
    TEST_RESULT(result, "Set lifecycle scene root");

    LCNodeHandle extra = NULL;
    result = lc_node_create(LC_NODE_3D, "LifecycleExtra", &extra);
    TEST_RESULT(result, "Create extra node to add/remove");
    result = lc_scene_add_node(scene, extra);
    TEST_RESULT(result, "Add extra node to scene");

    result = lc_scene_initialize(scene);
    TEST_RESULT(result, "Initialize scene");
    TEST_ASSERT(lc_scene_is_initialized(scene), "Scene reports initialized");

    result = lc_scene_update(scene, 0.016f);
    TEST_RESULT(result, "Update scene");
    result = lc_scene_physics_update(scene, 0.016f);
    TEST_RESULT(result, "Physics update scene");
    result = lc_scene_process_input(scene, 0.016f);
    TEST_RESULT(result, "Process input for scene");
    result = lc_scene_render(scene);
    TEST_RESULT(result, "Render scene");

    result = lc_scene_remove_node(scene, extra);
    TEST_RESULT(result, "Remove extra node from scene");

    result = lc_scene_shutdown(scene);
    TEST_RESULT(result, "Shutdown scene");

    lc_node_destroy(extra);
    lc_scene_destroy(scene);

    SECTION_END("NodeScene Ext: Scene Lifecycle");
}

static void test_ext_nodescene_scene_io(void) {
    SECTION_START("NodeScene Ext: Scene Save/Load");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("SaveLoadScene", &scene);
    TEST_RESULT(result, "Create scene for save/load");

    LCNodeHandle root = NULL;
    result = lc_node_create(LC_NODE_3D, "SaveLoadRoot", &root);
    TEST_RESULT(result, "Create root for save/load scene");
    result = lc_scene_set_root(scene, root);
    TEST_RESULT(result, "Set save/load scene root");

    const char* save_path = "capi_ext_nodescene_test.scene";
    LCResult save_result = lc_scene_save(scene, save_path);
    printf("    Scene save result: %d\n", (int)save_result);

    if (save_result == LC_SUCCESS) {
        const char* stored_path = lc_scene_get_filepath(scene);
        TEST_ASSERT(stored_path != NULL && strlen(stored_path) > 0,
                    "Saved scene reports a non-empty filepath");

        result = lc_scene_save_current(scene);
        TEST_RESULT(result, "Save scene to its current filepath");

        LCSceneHandle loaded = NULL;
        result = lc_scene_load(save_path, &loaded);
        TEST_RESULT(result, "Load scene back from file");
        TEST_ASSERT(loaded != NULL, "Loaded scene handle is valid");
        if (loaded) {
            lc_scene_destroy(loaded);
        }
        remove(save_path);
    } else {
        printf("    Filesystem save unavailable in this environment; skipping load-back.\n");
    }

    LCSceneHandle missing = (LCSceneHandle)0x1;
    LCResult load_missing = lc_scene_load("res://nonexistent_ext_ns_scene_xyz.scene", &missing);
    TEST_ASSERT(load_missing != LC_SUCCESS, "Loading a nonexistent scene reports failure");

    lc_scene_destroy(scene);

    SECTION_END("NodeScene Ext: Scene Save/Load");
}

static void test_ext_nodescene_prefab_io(void) {
    SECTION_START("NodeScene Ext: Prefab Save/Load");

    LCNodeHandle source = NULL;
    LCResult result = lc_node_create(LC_NODE_3D, "PrefabIoRoot", &source);
    TEST_RESULT(result, "Create source node for prefab");

    LCNodeHandle source_child = NULL;
    result = lc_node_create(LC_NODE_3D, "PrefabIoChild", &source_child);
    TEST_RESULT(result, "Create source child for prefab");
    result = lc_node_add_child(source, source_child);
    TEST_RESULT(result, "Attach child to prefab source");

    LCPrefabHandle prefab = NULL;
    result = lc_prefab_create_from_node(source, &prefab);
    TEST_RESULT(result, "Create prefab from node hierarchy");

    const char* prefab_path = "capi_ext_nodescene_test.prefab";
    LCResult save_result = lc_prefab_save(prefab, prefab_path);
    printf("    Prefab save result: %d\n", (int)save_result);

    if (save_result == LC_SUCCESS) {
        LCPrefabHandle loaded = NULL;
        result = lc_prefab_load(prefab_path, &loaded);
        TEST_RESULT(result, "Load prefab back from file");
        if (loaded) {
            bool valid = false;
            result = lc_prefab_is_valid(loaded, &valid);
            TEST_RESULT(result, "Check loaded prefab validity");
            TEST_ASSERT(valid, "Loaded prefab is valid");
            lc_prefab_destroy(loaded);
        }
        remove(prefab_path);
    } else {
        printf("    Filesystem save unavailable in this environment; skipping prefab load-back.\n");
    }

    LCNodeHandle attach_parent = NULL;
    result = lc_node_create(LC_NODE_3D, "PrefabAttachParent", &attach_parent);
    TEST_RESULT(result, "Create parent for instantiate-as-child");

    LCNodeHandle instantiated = NULL;
    result = lc_prefab_instantiate_as_child(prefab, attach_parent, &instantiated);
    TEST_RESULT(result, "Instantiate prefab as child");
    TEST_ASSERT(instantiated != NULL, "Instantiated child node exists");

    size_t child_count = 0;
    result = lc_node_get_child_count(attach_parent, &child_count);
    TEST_RESULT(result, "Get child count of attach parent");
    TEST_ASSERT(child_count >= 1, "Attach parent gained the instantiated child");

    LCPrefabHandle missing = (LCPrefabHandle)0x1;
    LCResult load_missing = lc_prefab_load("res://nonexistent_ext_ns_prefab_xyz.prefab", &missing);
    TEST_ASSERT(load_missing != LC_SUCCESS, "Loading a nonexistent prefab reports failure");

    lc_node_destroy(attach_parent);
    lc_node_destroy(source);
    lc_prefab_destroy(prefab);

    SECTION_END("NodeScene Ext: Prefab Save/Load");
}

static void test_ext_nodescene_scene_instance_reload(void) {
    SECTION_START("NodeScene Ext: SceneInstance Reload");

    LCNodeHandle instance = NULL;
    LCResult result = lc_scene_instance_create("ReloadInstance", &instance);
    TEST_RESULT(result, "Create SceneInstance node for reload");

    LCResult reload_empty = lc_scene_instance_reload_scene(instance);
    printf("    Reload with no reference result: %d\n", (int)reload_empty);

    result = lc_scene_instance_set_scene_reference(instance, "res://nonexistent_ext_ns_ref.scene");
    printf("    Set bogus scene reference result: %d\n", (int)result);

    LCResult reload_bad = lc_scene_instance_reload_scene(instance);
    printf("    Reload with bogus reference result: %d\n", (int)reload_bad);

    bool valid = true;
    result = lc_scene_instance_has_valid_reference(instance, &valid);
    TEST_RESULT(result, "Query valid reference after bogus reload");

    LCNodeHandle root = (LCNodeHandle)0x1;
    result = lc_scene_instance_get_instanced_root(instance, &root);
    TEST_RESULT(result, "Get instanced root after bogus reload (no crash)");

    result = lc_scene_instance_clear(instance);
    TEST_RESULT(result, "Clear scene instance after reload attempts");

    lc_node_destroy(instance);

    SECTION_END("NodeScene Ext: SceneInstance Reload");
}

static void test_ext_nodescene_additive_autoload(void) {
    SECTION_START("NodeScene Ext: Additive Autoload Scenes");

    LCResult null_add = lc_scene_add_autoload(NULL);
    TEST_ASSERT(null_add == LC_ERROR_NULL_POINTER, "add_autoload rejects NULL path");

    LCResult null_remove = lc_scene_remove_autoload(NULL);
    TEST_ASSERT(null_remove == LC_ERROR_NULL_POINTER, "remove_autoload rejects NULL path");

    LCResult add_no_manager = lc_scene_add_autoload("res://nonexistent_ext_ns_overlay.scene");
    printf("    add_autoload with no active scene manager result: %d\n", (int)add_no_manager);
    TEST_ASSERT(add_no_manager != LC_SUCCESS,
                "add_autoload fails gracefully without an active scene manager");

    LCResult remove_no_manager = lc_scene_remove_autoload("res://nonexistent_ext_ns_overlay.scene");
    printf("    remove_autoload with no active scene manager result: %d\n", (int)remove_no_manager);
    TEST_ASSERT(remove_no_manager != LC_SUCCESS,
                "remove_autoload fails gracefully without an active scene manager");

    SECTION_END("NodeScene Ext: Additive Autoload Scenes");
}
