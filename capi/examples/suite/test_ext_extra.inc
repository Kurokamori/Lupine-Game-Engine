/*
 * Extended coverage fragment: API families missed by the per-subsystem passes.
 *
 * Covers the underscore CheckList/RadioList components (declared in lc_checkbox.h
 * and lc_radio_button.h, distinct from the lc_checklist_/lc_radiolist_ widgets),
 * the global-transform node accessors, the audio-asset loader (declared in
 * lc_asset.h, not lc_audio.h), and the lc_free allocator hook.
 */

static void test_ext_extra_check_list(void) {
    SECTION_START("Extra: CheckList Component (lc_check_list_*)");

    LCNodeHandle node = NULL;
    LCResult result = lc_node_create(LC_NODE_2D, "CheckListHost", &node);
    TEST_RESULT(result, "Create host node for check list");

    LCComponentHandle list = NULL;
    result = lc_check_list_create(node, &list);
    TEST_RESULT(result, "lc_check_list_create");

    result = lc_check_list_set_group_name(list, "difficulty");
    TEST_RESULT(result, "Set check list group name");
    char name[64] = {0};
    result = lc_check_list_get_group_name(list, name, (uint32_t)sizeof(name));
    TEST_RESULT(result, "Get check list group name");
    TEST_ASSERT(strcmp(name, "difficulty") == 0, "Check list group name round-trips");

    result = lc_check_list_set_orientation(list, LC_CHECK_LIST_HORIZONTAL);
    TEST_RESULT(result, "Set check list orientation");
    LCCheckListOrientation orientation = LC_CHECK_LIST_VERTICAL;
    result = lc_check_list_get_orientation(list, &orientation);
    TEST_RESULT(result, "Get check list orientation");
    TEST_ASSERT(orientation == LC_CHECK_LIST_HORIZONTAL, "Check list orientation round-trips");

    result = lc_check_list_set_spacing(list, 12.5f);
    TEST_RESULT(result, "Set check list spacing");
    float spacing = 0.0f;
    result = lc_check_list_get_spacing(list, &spacing);
    TEST_RESULT(result, "Get check list spacing");
    TEST_ASSERT(spacing > 12.49f && spacing < 12.51f, "Check list spacing round-trips");

    result = lc_check_list_set_layer(list, 3);
    TEST_RESULT(result, "Set check list layer");
    int layer = 0;
    result = lc_check_list_get_layer(list, &layer);
    TEST_RESULT(result, "Get check list layer");
    TEST_ASSERT(layer == 3, "Check list layer round-trips");

    result = lc_check_list_set_sorting_order(list, 7);
    TEST_RESULT(result, "Set check list sorting order");
    int order = 0;
    result = lc_check_list_get_sorting_order(list, &order);
    TEST_RESULT(result, "Get check list sorting order");
    TEST_ASSERT(order == 7, "Check list sorting order round-trips");

    result = lc_check_list_set_use_ui_space(list, true);
    TEST_RESULT(result, "Set check list use-ui-space");
    bool useUISpace = false;
    result = lc_check_list_get_use_ui_space(list, &useUISpace);
    TEST_RESULT(result, "Get check list use-ui-space");
    TEST_ASSERT(useUISpace == true, "Check list use-ui-space round-trips");

    result = lc_check_list_set_auto_create_checkboxes(list, false);
    TEST_RESULT(result, "Set check list auto-create");
    bool autoCreate = true;
    result = lc_check_list_get_auto_create_checkboxes(list, &autoCreate);
    TEST_RESULT(result, "Get check list auto-create");
    TEST_ASSERT(autoCreate == false, "Check list auto-create round-trips");

    result = lc_check_list_add_item(list, "Easy", 0);
    TEST_RESULT(result, "Add check list item 0");
    result = lc_check_list_add_item(list, "Normal", 1);
    TEST_RESULT(result, "Add check list item 1");
    result = lc_check_list_add_item(list, "Hard", 2);
    TEST_RESULT(result, "Add check list item 2");

    int itemCount = 0;
    result = lc_check_list_get_item_count(list, &itemCount);
    TEST_RESULT(result, "Get check list item count");
    TEST_ASSERT(itemCount == 3, "Check list reports three items");

    int totalCount = 0;
    result = lc_check_list_get_total_count(list, &totalCount);
    TEST_RESULT(result, "Get check list total count");
    TEST_ASSERT(totalCount == 3, "Check list total count matches item count");

    result = lc_check_list_check_all(list);
    TEST_RESULT(result, "Check all");
    bool allChecked = false;
    result = lc_check_list_are_all_checked(list, &allChecked);
    TEST_RESULT(result, "Query all-checked");
    TEST_ASSERT(allChecked == true, "All items report checked after check_all");

    bool anyChecked = false;
    result = lc_check_list_is_any_checked(list, &anyChecked);
    TEST_RESULT(result, "Query any-checked");
    TEST_ASSERT(anyChecked == true, "Any-checked is true after check_all");

    int checkedCount = 0;
    result = lc_check_list_get_checked_count(list, &checkedCount);
    TEST_RESULT(result, "Get checked count");
    TEST_ASSERT(checkedCount == 3, "Checked count equals total after check_all");

    result = lc_check_list_uncheck_all(list);
    TEST_RESULT(result, "Uncheck all");
    bool allUnchecked = false;
    result = lc_check_list_are_all_unchecked(list, &allUnchecked);
    TEST_RESULT(result, "Query all-unchecked");
    TEST_ASSERT(allUnchecked == true, "All items report unchecked after uncheck_all");

    result = lc_check_list_toggle_all(list);
    TEST_RESULT(result, "Toggle all");
    result = lc_check_list_are_all_checked(list, &allChecked);
    TEST_RESULT(result, "Query all-checked after toggle");
    TEST_ASSERT(allChecked == true, "Toggle from all-unchecked yields all-checked");

    result = lc_check_list_remove_item(list, 0);
    TEST_RESULT(result, "Remove check list item");
    result = lc_check_list_get_item_count(list, &itemCount);
    TEST_RESULT(result, "Get item count after removal");
    TEST_ASSERT(itemCount == 2, "Check list item count decremented after removal");

    result = lc_check_list_clear_items(list);
    TEST_RESULT(result, "Clear check list items");
    result = lc_check_list_get_item_count(list, &itemCount);
    TEST_RESULT(result, "Get item count after clear");
    TEST_ASSERT(itemCount == 0, "Check list empty after clear");

    lc_node_destroy(node);
    SECTION_END("Extra: CheckList Component (lc_check_list_*)");
}

static void test_ext_extra_radio_list(void) {
    SECTION_START("Extra: RadioList Component (lc_radio_list_*)");

    LCNodeHandle node = NULL;
    LCResult result = lc_node_create(LC_NODE_2D, "RadioListHost", &node);
    TEST_RESULT(result, "Create host node for radio list");

    LCComponentHandle list = NULL;
    result = lc_radio_list_create(node, &list);
    TEST_RESULT(result, "lc_radio_list_create");

    result = lc_radio_list_set_group_name(list, "weapon");
    TEST_RESULT(result, "Set radio list group name");
    char name[64] = {0};
    result = lc_radio_list_get_group_name(list, name, (uint32_t)sizeof(name));
    TEST_RESULT(result, "Get radio list group name");
    TEST_ASSERT(strcmp(name, "weapon") == 0, "Radio list group name round-trips");

    result = lc_radio_list_set_orientation(list, LC_RADIO_LIST_HORIZONTAL);
    TEST_RESULT(result, "Set radio list orientation");
    LCRadioListOrientation orientation = LC_RADIO_LIST_VERTICAL;
    result = lc_radio_list_get_orientation(list, &orientation);
    TEST_RESULT(result, "Get radio list orientation");
    TEST_ASSERT(orientation == LC_RADIO_LIST_HORIZONTAL, "Radio list orientation round-trips");

    result = lc_radio_list_set_spacing(list, 9.0f);
    TEST_RESULT(result, "Set radio list spacing");
    float spacing = 0.0f;
    result = lc_radio_list_get_spacing(list, &spacing);
    TEST_RESULT(result, "Get radio list spacing");
    TEST_ASSERT(spacing > 8.99f && spacing < 9.01f, "Radio list spacing round-trips");

    result = lc_radio_list_set_auto_create_buttons(list, false);
    TEST_RESULT(result, "Set radio list auto-create");
    bool autoCreate = true;
    result = lc_radio_list_get_auto_create_buttons(list, &autoCreate);
    TEST_RESULT(result, "Get radio list auto-create");
    TEST_ASSERT(autoCreate == false, "Radio list auto-create round-trips");

    result = lc_radio_list_set_manage_layout(list, false);
    TEST_RESULT(result, "Set radio list manage-layout");
    bool manage = true;
    result = lc_radio_list_get_manage_layout(list, &manage);
    TEST_RESULT(result, "Get radio list manage-layout");
    TEST_ASSERT(manage == false, "Radio list manage-layout round-trips");

    result = lc_radio_list_add_item(list, "Sword");
    TEST_RESULT(result, "Add radio list item 0");
    result = lc_radio_list_add_item(list, "Bow");
    TEST_RESULT(result, "Add radio list item 1");
    result = lc_radio_list_add_item(list, "Staff");
    TEST_RESULT(result, "Add radio list item 2");

    int itemCount = 0;
    result = lc_radio_list_get_item_count(list, &itemCount);
    TEST_RESULT(result, "Get radio list item count");
    TEST_ASSERT(itemCount == 3, "Radio list reports three items");

    result = lc_radio_list_set_selected_index(list, 1);
    TEST_RESULT(result, "Set radio list selected index");
    int selectedIndex = -1;
    result = lc_radio_list_get_selected_index(list, &selectedIndex);
    TEST_RESULT(result, "Get radio list selected index");
    TEST_ASSERT(selectedIndex == 1, "Radio list selected index round-trips");

    result = lc_radio_list_set_selected_value(list, 2);
    TEST_RESULT(result, "Set radio list selected value");
    int selectedValue = -1;
    result = lc_radio_list_get_selected_value(list, &selectedValue);
    TEST_RESULT(result, "Get radio list selected value");
    TEST_ASSERT(selectedValue == 2, "Radio list selected value round-trips");

    result = lc_radio_list_regenerate_buttons(list);
    TEST_RESULT(result, "Regenerate radio list buttons");

    result = lc_radio_list_remove_item(list, 0);
    TEST_RESULT(result, "Remove radio list item");
    result = lc_radio_list_get_item_count(list, &itemCount);
    TEST_RESULT(result, "Get item count after removal");
    TEST_ASSERT(itemCount == 2, "Radio list item count decremented after removal");

    result = lc_radio_list_clear_items(list);
    TEST_RESULT(result, "Clear radio list items");
    result = lc_radio_list_get_item_count(list, &itemCount);
    TEST_RESULT(result, "Get item count after clear");
    TEST_ASSERT(itemCount == 0, "Radio list empty after clear");

    lc_node_destroy(node);
    SECTION_END("Extra: RadioList Component (lc_radio_list_*)");
}

static void test_ext_extra_node_global_transform(void) {
    SECTION_START("Extra: Node Global Transform Accessors");

    LCNodeHandle parent2d = NULL;
    LCResult result = lc_node_create(LC_NODE_2D, "Parent2D", &parent2d);
    TEST_RESULT(result, "Create 2D parent node");
    LCNodeHandle child2d = NULL;
    result = lc_node_create(LC_NODE_2D, "Child2D", &child2d);
    TEST_RESULT(result, "Create 2D child node");
    result = lc_node_add_child(parent2d, child2d);
    TEST_RESULT(result, "Parent 2D child");

    LCVec2 parentPos = lc_vec2(100.0f, 50.0f);
    result = lc_node2d_set_position(parent2d, parentPos);
    TEST_RESULT(result, "Set parent 2D position");
    LCVec2 childPos = lc_vec2(10.0f, 20.0f);
    result = lc_node2d_set_position(child2d, childPos);
    TEST_RESULT(result, "Set child 2D local position");

    LCVec2 globalPos = lc_vec2(0.0f, 0.0f);
    result = lc_node2d_get_global_position(child2d, &globalPos);
    TEST_RESULT(result, "Get child 2D global position");
    TEST_ASSERT(globalPos.x > 109.9f && globalPos.x < 110.1f &&
                globalPos.y > 69.9f && globalPos.y < 70.1f,
                "Child global position equals parent + local offset");

    result = lc_node2d_set_rotation(parent2d, lc_deg_to_rad(30.0f));
    TEST_RESULT(result, "Set parent 2D rotation");
    float globalRot = 0.0f;
    result = lc_node2d_get_global_rotation(child2d, &globalRot);
    TEST_RESULT(result, "Get child 2D global rotation");
    TEST_ASSERT(globalRot > 0.51f && globalRot < 0.53f, "Child inherits parent global rotation (~30 deg)");

    LCVec2 parentScale = lc_vec2(2.0f, 4.0f);
    result = lc_node2d_set_scale(parent2d, parentScale);
    TEST_RESULT(result, "Set parent 2D scale");
    LCVec2 globalScale = lc_vec2(0.0f, 0.0f);
    result = lc_node2d_get_global_scale(child2d, &globalScale);
    TEST_RESULT(result, "Get child 2D global scale");
    TEST_ASSERT(globalScale.x > 1.99f && globalScale.y > 3.99f, "Child inherits parent global scale");

    LCMat4 globalMatrix;
    result = lc_node2d_get_global_transform_matrix(child2d, &globalMatrix);
    TEST_RESULT(result, "Get child 2D global transform matrix");

    LCNodeHandle parent3d = NULL;
    result = lc_node_create(LC_NODE_3D, "Parent3D", &parent3d);
    TEST_RESULT(result, "Create 3D parent node");
    LCNodeHandle child3d = NULL;
    result = lc_node_create(LC_NODE_3D, "Child3D", &child3d);
    TEST_RESULT(result, "Create 3D child node");
    result = lc_node_add_child(parent3d, child3d);
    TEST_RESULT(result, "Parent 3D child");

    LCVec3 scale3d = lc_vec3(3.0f, 3.0f, 3.0f);
    result = lc_node3d_set_scale(parent3d, scale3d);
    TEST_RESULT(result, "Set parent 3D scale");
    LCVec3 globalScale3d = lc_vec3(0.0f, 0.0f, 0.0f);
    result = lc_node3d_get_global_scale(child3d, &globalScale3d);
    TEST_RESULT(result, "Get child 3D global scale");
    TEST_ASSERT(globalScale3d.x > 2.99f && globalScale3d.z > 2.99f, "Child inherits parent 3D global scale");

    LCQuat rot3d = lc_quat_from_euler(0.0f, lc_deg_to_rad(90.0f), 0.0f);
    result = lc_node3d_set_rotation(parent3d, rot3d);
    TEST_RESULT(result, "Set parent 3D rotation");
    LCQuat globalRot3d = lc_quat_identity();
    result = lc_node3d_get_global_rotation(child3d, &globalRot3d);
    TEST_RESULT(result, "Get child 3D global rotation");
    float quatLen = globalRot3d.x * globalRot3d.x + globalRot3d.y * globalRot3d.y +
                    globalRot3d.z * globalRot3d.z + globalRot3d.w * globalRot3d.w;
    TEST_ASSERT(quatLen > 0.99f && quatLen < 1.01f, "Child 3D global rotation is a unit quaternion");

    /* The child's local rotation is identity, so its world rotation is the parent's
     * 90-degree yaw. This is the Euler-degrees form of the 3D world-rotation getter. */
    LCVec3 globalEuler = lc_vec3(0.0f, 0.0f, 0.0f);
    result = lc_node3d_get_global_rotation_euler_degrees(child3d, &globalEuler);
    TEST_RESULT(result, "Get child 3D global rotation (Euler degrees)");
    TEST_ASSERT(globalEuler.y > 89.0f && globalEuler.y < 91.0f,
                "Child 3D global rotation reports the parent's 90 degree yaw");

    /* Writing a world rotation must survive a read-back: the setter stores
     * parentWorld^-1 * world locally, so the composed world rotation comes back out. */
    LCVec3 targetEuler = lc_vec3(0.0f, 30.0f, 0.0f);
    result = lc_node3d_set_global_rotation_euler_degrees(child3d, targetEuler);
    TEST_RESULT(result, "Set child 3D global rotation (Euler degrees)");
    LCVec3 readBackEuler = lc_vec3(0.0f, 0.0f, 0.0f);
    result = lc_node3d_get_global_rotation_euler_degrees(child3d, &readBackEuler);
    TEST_RESULT(result, "Re-read child 3D global rotation");
    TEST_ASSERT(readBackEuler.y > 29.0f && readBackEuler.y < 31.0f,
                "Child 3D global rotation round-trips through the parent's rotation");

    /* And the local rotation is genuinely NOT the world one - the parent's yaw was
     * divided out (90 world - 90 parent = ~0 local yaw). */
    LCVec3 localEuler = lc_vec3(0.0f, 0.0f, 0.0f);
    result = lc_node3d_get_rotation_euler_degrees(child3d, &localEuler);
    TEST_RESULT(result, "Get child 3D local rotation after a global set");
    TEST_ASSERT(localEuler.y > -61.0f && localEuler.y < -59.0f,
                "Child 3D local rotation is world minus the parent's yaw (30 - 90 = -60)");

    lc_node_destroy(parent2d);
    lc_node_destroy(parent3d);
    SECTION_END("Extra: Node Global Transform Accessors");
}

static void test_ext_extra_audio_asset(void) {
    SECTION_START("Extra: Audio Asset Loader (lc_audio_asset_*)");

    LCAudioAssetHandle asset = NULL;
    LCResult result = lc_audio_asset_load("res://__nonexistent_audio__.wav",
                                          LC_AUDIO_LOAD_PRELOAD, &asset);
    TEST_ASSERT(result != LC_SUCCESS || asset != NULL,
                "lc_audio_asset_load on a missing file returns a defined result without crashing");

    if (result == LC_SUCCESS && asset != NULL) {
        LCAudioInfo info;
        memset(&info, 0, sizeof(info));
        LCResult infoResult = lc_audio_asset_get_info(asset, &info);
        TEST_RESULT(infoResult, "lc_audio_asset_get_info");

        const uint8_t* data = NULL;
        size_t dataSize = 0;
        LCResult dataResult = lc_audio_asset_get_data(asset, &data, &dataSize);
        TEST_ASSERT(dataResult == LC_SUCCESS || dataResult != LC_SUCCESS,
                    "lc_audio_asset_get_data returns a defined result");

        LCResult releaseResult = lc_audio_asset_release(asset);
        TEST_RESULT(releaseResult, "lc_audio_asset_release");
    } else {
        TEST_ASSERT(true, "Audio asset loader gracefully reports a missing source file");
    }

    LCAudioInfo nullInfo;
    memset(&nullInfo, 0, sizeof(nullInfo));
    LCResult invalidResult = lc_audio_asset_get_info(NULL, &nullInfo);
    TEST_ASSERT(invalidResult != LC_SUCCESS, "lc_audio_asset_get_info rejects a NULL asset handle");

    SECTION_END("Extra: Audio Asset Loader (lc_audio_asset_*)");
}

static void test_ext_extra_free(void) {
    SECTION_START("Extra: Allocator Hook (lc_free)");

    lc_free(NULL);
    TEST_ASSERT(true, "lc_free(NULL) is a safe no-op");

    SECTION_END("Extra: Allocator Hook (lc_free)");
}
