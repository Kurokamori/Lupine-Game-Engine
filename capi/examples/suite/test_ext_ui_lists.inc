/* Extended UI list/menu C API tests.
 * Covers the public functions of:
 *   capi/include/ui/lc_dropdown.h
 *   capi/include/ui/lc_popup_menu.h
 *   capi/include/ui/lc_item_list.h
 *   capi/include/ui/lc_tree.h
 *   capi/include/ui/lc_rich_text_label.h
 * Exercises collection mutation (add/get/set/remove/clear/count), selection
 * round-trips, tree depth/expand structure, rich-text/BBCode set/get, and
 * scalar/color/bool property round-trips. */

static void test_ext_ui_dropdown(void) {
    SECTION_START("Ext UI: Dropdown");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_ext_dropdown_scene", &scene);
    TEST_RESULT(result, "Create dropdown scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "ExtDropdownNode", &node);
    TEST_RESULT(result, "Create node for Dropdown");
    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add dropdown node to scene");

    LCComponentHandle dropdown = NULL;
    result = lc_dropdown_create(node, &dropdown);
    TEST_RESULT(result, "Create Dropdown component");

    int count = -1;
    result = lc_dropdown_get_item_count(dropdown, &count);
    TEST_RESULT(result, "Get initial dropdown item count");
    TEST_ASSERT(count == 0, "Dropdown starts with zero items");

    result = lc_dropdown_add_item(dropdown, "Apple");
    TEST_RESULT(result, "Add dropdown item Apple");
    result = lc_dropdown_add_item(dropdown, "Banana");
    TEST_RESULT(result, "Add dropdown item Banana");
    result = lc_dropdown_add_item(dropdown, "Cherry");
    TEST_RESULT(result, "Add dropdown item Cherry");

    result = lc_dropdown_get_item_count(dropdown, &count);
    TEST_RESULT(result, "Get dropdown item count after adds");
    TEST_ASSERT(count == 3, "Dropdown has 3 items");

    char buffer[128];
    int required = 0;
    result = lc_dropdown_get_item(dropdown, 1, buffer, (int)sizeof(buffer), &required);
    TEST_RESULT(result, "Get dropdown item at index 1");
    TEST_ASSERT(strcmp(buffer, "Banana") == 0, "Dropdown item 1 is Banana");

    result = lc_dropdown_set_selected_index(dropdown, 2);
    TEST_RESULT(result, "Set dropdown selected index to 2");
    int selected = -1;
    result = lc_dropdown_get_selected_index(dropdown, &selected);
    TEST_RESULT(result, "Get dropdown selected index");
    TEST_ASSERT(selected == 2, "Dropdown selected index is 2");

    result = lc_dropdown_get_selected_text(dropdown, buffer, (int)sizeof(buffer), &required);
    TEST_RESULT(result, "Get dropdown selected text");
    TEST_ASSERT(strcmp(buffer, "Cherry") == 0, "Dropdown selected text is Cherry");

    result = lc_dropdown_remove_item(dropdown, 0);
    TEST_RESULT(result, "Remove dropdown item 0");
    result = lc_dropdown_get_item_count(dropdown, &count);
    TEST_RESULT(result, "Get dropdown count after remove");
    TEST_ASSERT(count == 2, "Dropdown has 2 items after remove");
    result = lc_dropdown_get_item(dropdown, 0, buffer, (int)sizeof(buffer), &required);
    TEST_RESULT(result, "Get dropdown item 0 after remove");
    TEST_ASSERT(strcmp(buffer, "Banana") == 0, "Dropdown item 0 is now Banana");

    result = lc_dropdown_clear_items(dropdown);
    TEST_RESULT(result, "Clear dropdown items");
    result = lc_dropdown_get_item_count(dropdown, &count);
    TEST_RESULT(result, "Get dropdown count after clear");
    TEST_ASSERT(count == 0, "Dropdown empty after clear");

    result = lc_dropdown_set_placeholder(dropdown, "Select fruit...");
    TEST_RESULT(result, "Set dropdown placeholder");
    result = lc_dropdown_get_placeholder(dropdown, buffer, (int)sizeof(buffer), &required);
    TEST_RESULT(result, "Get dropdown placeholder");
    TEST_ASSERT(strcmp(buffer, "Select fruit...") == 0, "Dropdown placeholder round-trips");

    result = lc_dropdown_set_item_height(dropdown, 28.0f);
    TEST_RESULT(result, "Set dropdown item height");
    float item_height = 0.0f;
    result = lc_dropdown_get_item_height(dropdown, &item_height);
    TEST_RESULT(result, "Get dropdown item height");
    TEST_ASSERT(item_height > 27.99f && item_height < 28.01f, "Dropdown item height round-trips");

    result = lc_dropdown_set_font_path(dropdown, "res://fonts/main.ttf");
    TEST_RESULT(result, "Set dropdown font path");
    result = lc_dropdown_get_font_path(dropdown, buffer, (int)sizeof(buffer), &required);
    TEST_RESULT(result, "Get dropdown font path");
    TEST_ASSERT(strcmp(buffer, "res://fonts/main.ttf") == 0, "Dropdown font path round-trips");

    result = lc_dropdown_set_font_size(dropdown, 18.5f);
    TEST_RESULT(result, "Set dropdown font size");
    float font_size = 0.0f;
    result = lc_dropdown_get_font_size(dropdown, &font_size);
    TEST_RESULT(result, "Get dropdown font size");
    TEST_ASSERT(font_size > 18.49f && font_size < 18.51f, "Dropdown font size round-trips");

    LCColor font_color = lc_color(0.1f, 0.2f, 0.3f, 1.0f);
    result = lc_dropdown_set_font_color(dropdown, font_color);
    TEST_RESULT(result, "Set dropdown font color");
    LCColor got_color = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    result = lc_dropdown_get_font_color(dropdown, &got_color);
    TEST_RESULT(result, "Get dropdown font color");
    TEST_ASSERT(got_color.r == font_color.r, "Dropdown font color.r round-trips");
    TEST_ASSERT(got_color.g == font_color.g, "Dropdown font color.g round-trips");
    TEST_ASSERT(got_color.b == font_color.b, "Dropdown font color.b round-trips");
    TEST_ASSERT(got_color.a == font_color.a, "Dropdown font color.a round-trips");

    LCColor bg_color = lc_color(0.4f, 0.5f, 0.6f, 0.8f);
    result = lc_dropdown_set_background_color(dropdown, bg_color);
    TEST_RESULT(result, "Set dropdown background color");
    result = lc_dropdown_get_background_color(dropdown, &got_color);
    TEST_RESULT(result, "Get dropdown background color");
    TEST_ASSERT(got_color.r == bg_color.r, "Dropdown bg color.r round-trips");
    TEST_ASSERT(got_color.g == bg_color.g, "Dropdown bg color.g round-trips");
    TEST_ASSERT(got_color.b == bg_color.b, "Dropdown bg color.b round-trips");
    TEST_ASSERT(got_color.a == bg_color.a, "Dropdown bg color.a round-trips");

    lc_scene_destroy(scene);

    SECTION_END("Ext UI: Dropdown");
}

static void test_ext_ui_popup_menu(void) {
    SECTION_START("Ext UI: PopupMenu");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_ext_popup_scene", &scene);
    TEST_RESULT(result, "Create popup menu scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "ExtPopupMenuNode", &node);
    TEST_RESULT(result, "Create node for PopupMenu");
    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add popup menu node to scene");

    LCComponentHandle menu = NULL;
    result = lc_popup_menu_create(node, &menu);
    TEST_RESULT(result, "Create PopupMenu component");

    int count = -1;
    result = lc_popup_menu_get_item_count(menu, &count);
    TEST_RESULT(result, "Get initial popup menu item count");
    TEST_ASSERT(count == 0, "PopupMenu starts with zero items");

    result = lc_popup_menu_add_item(menu, "New");
    TEST_RESULT(result, "Add popup item New");
    result = lc_popup_menu_add_item(menu, "Open");
    TEST_RESULT(result, "Add popup item Open");
    result = lc_popup_menu_add_item(menu, "Save");
    TEST_RESULT(result, "Add popup item Save");
    result = lc_popup_menu_add_item(menu, "Quit");
    TEST_RESULT(result, "Add popup item Quit");

    result = lc_popup_menu_get_item_count(menu, &count);
    TEST_RESULT(result, "Get popup item count after adds");
    TEST_ASSERT(count == 4, "PopupMenu has 4 items");

    char buffer[128];
    int required = 0;
    result = lc_popup_menu_get_item(menu, 2, buffer, (int)sizeof(buffer), &required);
    TEST_RESULT(result, "Get popup item at index 2");
    TEST_ASSERT(strcmp(buffer, "Save") == 0, "PopupMenu item 2 is Save");

    result = lc_popup_menu_remove_item(menu, 1);
    TEST_RESULT(result, "Remove popup item 1");
    result = lc_popup_menu_get_item_count(menu, &count);
    TEST_RESULT(result, "Get popup count after remove");
    TEST_ASSERT(count == 3, "PopupMenu has 3 items after remove");
    result = lc_popup_menu_get_item(menu, 1, buffer, (int)sizeof(buffer), &required);
    TEST_RESULT(result, "Get popup item 1 after remove");
    TEST_ASSERT(strcmp(buffer, "Save") == 0, "PopupMenu item 1 is now Save");

    bool open = true;
    result = lc_popup_menu_is_open(menu, &open);
    TEST_RESULT(result, "Query popup is_open default");
    TEST_ASSERT(open == false, "PopupMenu starts closed");

    result = lc_popup_menu_popup(menu);
    TEST_RESULT(result, "Open popup menu");
    result = lc_popup_menu_is_open(menu, &open);
    TEST_RESULT(result, "Query popup is_open after popup");
    TEST_ASSERT(open == true, "PopupMenu is open after popup");

    result = lc_popup_menu_close(menu);
    TEST_RESULT(result, "Close popup menu");
    result = lc_popup_menu_is_open(menu, &open);
    TEST_RESULT(result, "Query popup is_open after close");
    TEST_ASSERT(open == false, "PopupMenu is closed after close");

    result = lc_popup_menu_popup_at(menu, lc_vec2(120.0f, 80.0f));
    TEST_RESULT(result, "Open popup menu at position");
    result = lc_popup_menu_is_open(menu, &open);
    TEST_RESULT(result, "Query popup is_open after popup_at");
    TEST_ASSERT(open == true, "PopupMenu is open after popup_at");
    result = lc_popup_menu_close(menu);
    TEST_RESULT(result, "Close popup menu again");

    result = lc_popup_menu_clear_items(menu);
    TEST_RESULT(result, "Clear popup items");
    result = lc_popup_menu_get_item_count(menu, &count);
    TEST_RESULT(result, "Get popup count after clear");
    TEST_ASSERT(count == 0, "PopupMenu empty after clear");

    result = lc_popup_menu_set_item_height(menu, 24.0f);
    TEST_RESULT(result, "Set popup item height");
    float item_height = 0.0f;
    result = lc_popup_menu_get_item_height(menu, &item_height);
    TEST_RESULT(result, "Get popup item height");
    TEST_ASSERT(item_height > 23.99f && item_height < 24.01f, "PopupMenu item height round-trips");

    result = lc_popup_menu_set_font_path(menu, "res://fonts/menu.ttf");
    TEST_RESULT(result, "Set popup font path");
    result = lc_popup_menu_get_font_path(menu, buffer, (int)sizeof(buffer), &required);
    TEST_RESULT(result, "Get popup font path");
    TEST_ASSERT(strcmp(buffer, "res://fonts/menu.ttf") == 0, "PopupMenu font path round-trips");

    result = lc_popup_menu_set_font_size(menu, 15.0f);
    TEST_RESULT(result, "Set popup font size");
    float font_size = 0.0f;
    result = lc_popup_menu_get_font_size(menu, &font_size);
    TEST_RESULT(result, "Get popup font size");
    TEST_ASSERT(font_size > 14.99f && font_size < 15.01f, "PopupMenu font size round-trips");

    LCColor font_color = lc_color(0.9f, 0.8f, 0.7f, 1.0f);
    result = lc_popup_menu_set_font_color(menu, font_color);
    TEST_RESULT(result, "Set popup font color");
    LCColor got_color = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    result = lc_popup_menu_get_font_color(menu, &got_color);
    TEST_RESULT(result, "Get popup font color");
    TEST_ASSERT(got_color.r == font_color.r, "PopupMenu font color.r round-trips");
    TEST_ASSERT(got_color.g == font_color.g, "PopupMenu font color.g round-trips");
    TEST_ASSERT(got_color.b == font_color.b, "PopupMenu font color.b round-trips");
    TEST_ASSERT(got_color.a == font_color.a, "PopupMenu font color.a round-trips");

    LCColor bg_color = lc_color(0.05f, 0.05f, 0.05f, 0.95f);
    result = lc_popup_menu_set_background_color(menu, bg_color);
    TEST_RESULT(result, "Set popup background color");
    result = lc_popup_menu_get_background_color(menu, &got_color);
    TEST_RESULT(result, "Get popup background color");
    TEST_ASSERT(got_color.r == bg_color.r, "PopupMenu bg color.r round-trips");
    TEST_ASSERT(got_color.g == bg_color.g, "PopupMenu bg color.g round-trips");
    TEST_ASSERT(got_color.b == bg_color.b, "PopupMenu bg color.b round-trips");
    TEST_ASSERT(got_color.a == bg_color.a, "PopupMenu bg color.a round-trips");

    lc_scene_destroy(scene);

    SECTION_END("Ext UI: PopupMenu");
}

static void test_ext_ui_item_list(void) {
    SECTION_START("Ext UI: ItemList");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_ext_item_list_scene", &scene);
    TEST_RESULT(result, "Create item list scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "ExtItemListNode", &node);
    TEST_RESULT(result, "Create node for ItemList");
    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add item list node to scene");

    LCComponentHandle list = NULL;
    result = lc_item_list_create(node, &list);
    TEST_RESULT(result, "Create ItemList component");

    int count = -1;
    result = lc_item_list_get_item_count(list, &count);
    TEST_RESULT(result, "Get initial item list count");
    TEST_ASSERT(count == 0, "ItemList starts with zero items");

    result = lc_item_list_add_item(list, "Sword");
    TEST_RESULT(result, "Add item Sword");
    result = lc_item_list_add_item(list, "Shield");
    TEST_RESULT(result, "Add item Shield");
    result = lc_item_list_add_item(list, "Potion");
    TEST_RESULT(result, "Add item Potion");
    result = lc_item_list_add_item(list, "Bow");
    TEST_RESULT(result, "Add item Bow");

    result = lc_item_list_get_item_count(list, &count);
    TEST_RESULT(result, "Get item list count after adds");
    TEST_ASSERT(count == 4, "ItemList has 4 items");

    char buffer[128];
    int required = 0;
    result = lc_item_list_get_item(list, 0, buffer, (int)sizeof(buffer), &required);
    TEST_RESULT(result, "Get item list item 0");
    TEST_ASSERT(strcmp(buffer, "Sword") == 0, "ItemList item 0 is Sword");
    result = lc_item_list_get_item(list, 3, buffer, (int)sizeof(buffer), &required);
    TEST_RESULT(result, "Get item list item 3");
    TEST_ASSERT(strcmp(buffer, "Bow") == 0, "ItemList item 3 is Bow");

    result = lc_item_list_set_selected_index(list, 2);
    TEST_RESULT(result, "Set item list selected index 2");
    int selected = -1;
    result = lc_item_list_get_selected_index(list, &selected);
    TEST_RESULT(result, "Get item list selected index");
    TEST_ASSERT(selected == 2, "ItemList selected index is 2");

    result = lc_item_list_remove_item(list, 1);
    TEST_RESULT(result, "Remove item list item 1");
    result = lc_item_list_get_item_count(list, &count);
    TEST_RESULT(result, "Get item list count after remove");
    TEST_ASSERT(count == 3, "ItemList has 3 items after remove");
    result = lc_item_list_get_item(list, 1, buffer, (int)sizeof(buffer), &required);
    TEST_RESULT(result, "Get item list item 1 after remove");
    TEST_ASSERT(strcmp(buffer, "Potion") == 0, "ItemList item 1 is now Potion");

    result = lc_item_list_clear_items(list);
    TEST_RESULT(result, "Clear item list");
    result = lc_item_list_get_item_count(list, &count);
    TEST_RESULT(result, "Get item list count after clear");
    TEST_ASSERT(count == 0, "ItemList empty after clear");

    result = lc_item_list_set_item_height(list, 32.0f);
    TEST_RESULT(result, "Set item list item height");
    float item_height = 0.0f;
    result = lc_item_list_get_item_height(list, &item_height);
    TEST_RESULT(result, "Get item list item height");
    TEST_ASSERT(item_height > 31.99f && item_height < 32.01f, "ItemList item height round-trips");

    result = lc_item_list_set_font_path(list, "res://fonts/list.ttf");
    TEST_RESULT(result, "Set item list font path");
    result = lc_item_list_get_font_path(list, buffer, (int)sizeof(buffer), &required);
    TEST_RESULT(result, "Get item list font path");
    TEST_ASSERT(strcmp(buffer, "res://fonts/list.ttf") == 0, "ItemList font path round-trips");

    result = lc_item_list_set_font_size(list, 14.0f);
    TEST_RESULT(result, "Set item list font size");
    float font_size = 0.0f;
    result = lc_item_list_get_font_size(list, &font_size);
    TEST_RESULT(result, "Get item list font size");
    TEST_ASSERT(font_size > 13.99f && font_size < 14.01f, "ItemList font size round-trips");

    LCColor font_color = lc_color(0.3f, 0.6f, 0.9f, 1.0f);
    result = lc_item_list_set_font_color(list, font_color);
    TEST_RESULT(result, "Set item list font color");
    LCColor got_color = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    result = lc_item_list_get_font_color(list, &got_color);
    TEST_RESULT(result, "Get item list font color");
    TEST_ASSERT(got_color.r == font_color.r, "ItemList font color.r round-trips");
    TEST_ASSERT(got_color.g == font_color.g, "ItemList font color.g round-trips");
    TEST_ASSERT(got_color.b == font_color.b, "ItemList font color.b round-trips");
    TEST_ASSERT(got_color.a == font_color.a, "ItemList font color.a round-trips");

    LCColor bg_color = lc_color(0.1f, 0.1f, 0.12f, 1.0f);
    result = lc_item_list_set_background_color(list, bg_color);
    TEST_RESULT(result, "Set item list background color");
    result = lc_item_list_get_background_color(list, &got_color);
    TEST_RESULT(result, "Get item list background color");
    TEST_ASSERT(got_color.r == bg_color.r, "ItemList bg color.r round-trips");
    TEST_ASSERT(got_color.g == bg_color.g, "ItemList bg color.g round-trips");
    TEST_ASSERT(got_color.b == bg_color.b, "ItemList bg color.b round-trips");
    TEST_ASSERT(got_color.a == bg_color.a, "ItemList bg color.a round-trips");

    LCColor sel_color = lc_color(0.8f, 0.4f, 0.2f, 0.7f);
    result = lc_item_list_set_selection_color(list, sel_color);
    TEST_RESULT(result, "Set item list selection color");
    result = lc_item_list_get_selection_color(list, &got_color);
    TEST_RESULT(result, "Get item list selection color");
    TEST_ASSERT(got_color.r == sel_color.r, "ItemList selection color.r round-trips");
    TEST_ASSERT(got_color.g == sel_color.g, "ItemList selection color.g round-trips");
    TEST_ASSERT(got_color.b == sel_color.b, "ItemList selection color.b round-trips");
    TEST_ASSERT(got_color.a == sel_color.a, "ItemList selection color.a round-trips");

    lc_scene_destroy(scene);

    SECTION_END("Ext UI: ItemList");
}

static void test_ext_ui_tree(void) {
    SECTION_START("Ext UI: Tree");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_ext_tree_scene", &scene);
    TEST_RESULT(result, "Create tree scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "ExtTreeNode", &node);
    TEST_RESULT(result, "Create node for Tree");
    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add tree node to scene");

    LCComponentHandle tree = NULL;
    result = lc_tree_create(node, &tree);
    TEST_RESULT(result, "Create Tree component");

    int count = -1;
    result = lc_tree_get_item_count(tree, &count);
    TEST_RESULT(result, "Get initial tree item count");
    TEST_ASSERT(count == 0, "Tree starts with zero items");

    result = lc_tree_add_item(tree, "Root", 0);
    TEST_RESULT(result, "Add tree root item (depth 0)");
    result = lc_tree_add_item(tree, "Child A", 1);
    TEST_RESULT(result, "Add tree child A (depth 1)");
    result = lc_tree_add_item(tree, "Child B", 1);
    TEST_RESULT(result, "Add tree child B (depth 1)");
    result = lc_tree_add_item(tree, "Grandchild", 2);
    TEST_RESULT(result, "Add tree grandchild (depth 2)");

    result = lc_tree_get_item_count(tree, &count);
    TEST_RESULT(result, "Get tree item count after adds");
    TEST_ASSERT(count == 4, "Tree has 4 items");

    char buffer[128];
    int required = 0;
    result = lc_tree_get_item_text(tree, 0, buffer, (int)sizeof(buffer), &required);
    TEST_RESULT(result, "Get tree item 0 text");
    TEST_ASSERT(strcmp(buffer, "Root") == 0, "Tree item 0 is Root");
    result = lc_tree_get_item_text(tree, 3, buffer, (int)sizeof(buffer), &required);
    TEST_RESULT(result, "Get tree item 3 text");
    TEST_ASSERT(strcmp(buffer, "Grandchild") == 0, "Tree item 3 is Grandchild");

    int depth = -1;
    result = lc_tree_get_item_depth(tree, 0, &depth);
    TEST_RESULT(result, "Get tree item 0 depth");
    TEST_ASSERT(depth == 0, "Tree root depth is 0");
    result = lc_tree_get_item_depth(tree, 1, &depth);
    TEST_RESULT(result, "Get tree item 1 depth");
    TEST_ASSERT(depth == 1, "Tree child A depth is 1");
    result = lc_tree_get_item_depth(tree, 3, &depth);
    TEST_RESULT(result, "Get tree item 3 depth");
    TEST_ASSERT(depth == 2, "Tree grandchild depth is 2");

    result = lc_tree_set_selected_index(tree, 2);
    TEST_RESULT(result, "Set tree selected index 2");
    int selected = -1;
    result = lc_tree_get_selected_index(tree, &selected);
    TEST_RESULT(result, "Get tree selected index");
    TEST_ASSERT(selected == 2, "Tree selected index is 2");

    bool expanded = false;
    result = lc_tree_set_expanded(tree, 0, true);
    TEST_RESULT(result, "Expand tree root");
    result = lc_tree_is_expanded(tree, 0, &expanded);
    TEST_RESULT(result, "Query tree root expanded");
    TEST_ASSERT(expanded == true, "Tree root is expanded");

    result = lc_tree_set_expanded(tree, 0, false);
    TEST_RESULT(result, "Collapse tree root");
    result = lc_tree_is_expanded(tree, 0, &expanded);
    TEST_RESULT(result, "Query tree root collapsed");
    TEST_ASSERT(expanded == false, "Tree root is collapsed");

    result = lc_tree_toggle_expand(tree, 0);
    TEST_RESULT(result, "Toggle expand tree root");
    result = lc_tree_is_expanded(tree, 0, &expanded);
    TEST_RESULT(result, "Query tree root after toggle");
    TEST_ASSERT(expanded == true, "Tree root is expanded after toggle");

    result = lc_tree_clear_items(tree);
    TEST_RESULT(result, "Clear tree items");
    result = lc_tree_get_item_count(tree, &count);
    TEST_RESULT(result, "Get tree count after clear");
    TEST_ASSERT(count == 0, "Tree empty after clear");

    result = lc_tree_set_item_height(tree, 22.0f);
    TEST_RESULT(result, "Set tree item height");
    float item_height = 0.0f;
    result = lc_tree_get_item_height(tree, &item_height);
    TEST_RESULT(result, "Get tree item height");
    TEST_ASSERT(item_height > 21.99f && item_height < 22.01f, "Tree item height round-trips");

    result = lc_tree_set_indent_width(tree, 18.0f);
    TEST_RESULT(result, "Set tree indent width");
    float indent_width = 0.0f;
    result = lc_tree_get_indent_width(tree, &indent_width);
    TEST_RESULT(result, "Get tree indent width");
    TEST_ASSERT(indent_width > 17.99f && indent_width < 18.01f, "Tree indent width round-trips");

    result = lc_tree_set_font_path(tree, "res://fonts/tree.ttf");
    TEST_RESULT(result, "Set tree font path");
    result = lc_tree_get_font_path(tree, buffer, (int)sizeof(buffer), &required);
    TEST_RESULT(result, "Get tree font path");
    TEST_ASSERT(strcmp(buffer, "res://fonts/tree.ttf") == 0, "Tree font path round-trips");

    result = lc_tree_set_font_size(tree, 13.0f);
    TEST_RESULT(result, "Set tree font size");
    float font_size = 0.0f;
    result = lc_tree_get_font_size(tree, &font_size);
    TEST_RESULT(result, "Get tree font size");
    TEST_ASSERT(font_size > 12.99f && font_size < 13.01f, "Tree font size round-trips");

    LCColor font_color = lc_color(0.7f, 0.7f, 0.7f, 1.0f);
    result = lc_tree_set_font_color(tree, font_color);
    TEST_RESULT(result, "Set tree font color");
    LCColor got_color = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    result = lc_tree_get_font_color(tree, &got_color);
    TEST_RESULT(result, "Get tree font color");
    TEST_ASSERT(got_color.r == font_color.r, "Tree font color.r round-trips");
    TEST_ASSERT(got_color.g == font_color.g, "Tree font color.g round-trips");
    TEST_ASSERT(got_color.b == font_color.b, "Tree font color.b round-trips");
    TEST_ASSERT(got_color.a == font_color.a, "Tree font color.a round-trips");

    LCColor sel_color = lc_color(0.2f, 0.5f, 0.8f, 0.6f);
    result = lc_tree_set_selection_color(tree, sel_color);
    TEST_RESULT(result, "Set tree selection color");
    result = lc_tree_get_selection_color(tree, &got_color);
    TEST_RESULT(result, "Get tree selection color");
    TEST_ASSERT(got_color.r == sel_color.r, "Tree selection color.r round-trips");
    TEST_ASSERT(got_color.g == sel_color.g, "Tree selection color.g round-trips");
    TEST_ASSERT(got_color.b == sel_color.b, "Tree selection color.b round-trips");
    TEST_ASSERT(got_color.a == sel_color.a, "Tree selection color.a round-trips");

    lc_scene_destroy(scene);

    SECTION_END("Ext UI: Tree");
}

static void test_ext_ui_rich_text_label(void) {
    SECTION_START("Ext UI: RichTextLabel");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_ext_rich_text_scene", &scene);
    TEST_RESULT(result, "Create rich text label scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "ExtRichTextLabelNode", &node);
    TEST_RESULT(result, "Create node for RichTextLabel");
    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add rich text label node to scene");

    LCComponentHandle label = NULL;
    result = lc_rich_text_label_create(node, &label);
    TEST_RESULT(result, "Create RichTextLabel component");

    char buffer[256];
    int required = 0;

    const char* bbcode = "[b]Bold[/b] and [color=#ff0000]red[/color] text";
    result = lc_rich_text_label_set_text(label, bbcode);
    TEST_RESULT(result, "Set rich text BBCode");
    result = lc_rich_text_label_get_text(label, buffer, (int)sizeof(buffer), &required);
    TEST_RESULT(result, "Get rich text BBCode");
    TEST_ASSERT(strcmp(buffer, bbcode) == 0, "RichTextLabel text round-trips");

    result = lc_rich_text_label_set_font_path(label, "res://fonts/rich.ttf");
    TEST_RESULT(result, "Set rich text font path");
    result = lc_rich_text_label_get_font_path(label, buffer, (int)sizeof(buffer), &required);
    TEST_RESULT(result, "Get rich text font path");
    TEST_ASSERT(strcmp(buffer, "res://fonts/rich.ttf") == 0, "RichTextLabel font path round-trips");

    result = lc_rich_text_label_set_font_size(label, 20.0f);
    TEST_RESULT(result, "Set rich text font size");
    float font_size = 0.0f;
    result = lc_rich_text_label_get_font_size(label, &font_size);
    TEST_RESULT(result, "Get rich text font size");
    TEST_ASSERT(font_size > 19.99f && font_size < 20.01f, "RichTextLabel font size round-trips");

    LCColor color = lc_color(0.95f, 0.85f, 0.1f, 1.0f);
    result = lc_rich_text_label_set_color(label, color);
    TEST_RESULT(result, "Set rich text color");
    LCColor got_color = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    result = lc_rich_text_label_get_color(label, &got_color);
    TEST_RESULT(result, "Get rich text color");
    TEST_ASSERT(got_color.r == color.r, "RichTextLabel color.r round-trips");
    TEST_ASSERT(got_color.g == color.g, "RichTextLabel color.g round-trips");
    TEST_ASSERT(got_color.b == color.b, "RichTextLabel color.b round-trips");
    TEST_ASSERT(got_color.a == color.a, "RichTextLabel color.a round-trips");

    bool wrap = false;
    result = lc_rich_text_label_get_word_wrap(label, &wrap);
    TEST_RESULT(result, "Get rich text word wrap default");
    bool default_wrap = wrap;

    result = lc_rich_text_label_set_word_wrap(label, !default_wrap);
    TEST_RESULT(result, "Toggle rich text word wrap");
    result = lc_rich_text_label_get_word_wrap(label, &wrap);
    TEST_RESULT(result, "Get rich text word wrap after toggle");
    TEST_ASSERT(wrap == !default_wrap, "RichTextLabel word wrap toggled");
    result = lc_rich_text_label_set_word_wrap(label, default_wrap);
    TEST_RESULT(result, "Restore rich text word wrap");
    result = lc_rich_text_label_get_word_wrap(label, &wrap);
    TEST_RESULT(result, "Get rich text word wrap after restore");
    TEST_ASSERT(wrap == default_wrap, "RichTextLabel word wrap restored");

    result = lc_rich_text_label_set_line_spacing(label, 4.5f);
    TEST_RESULT(result, "Set rich text line spacing");
    float spacing = 0.0f;
    result = lc_rich_text_label_get_line_spacing(label, &spacing);
    TEST_RESULT(result, "Get rich text line spacing");
    TEST_ASSERT(spacing > 4.49f && spacing < 4.51f, "RichTextLabel line spacing round-trips");

    lc_scene_destroy(scene);

    SECTION_END("Ext UI: RichTextLabel");
}
