/* Node tree utility C API tests:
 *  - lc_node_get_node_or_null / lc_node_find_children /
 *    lc_node_get_first_node_in_group / lc_node_is_ancestor_of.
 *
 * These mirror the scripting tree utilities and operate on explicit node
 * handles. Group lookups require an owning scene; a detached node reports no
 * matches (LC_SUCCESS with a NULL handle). */

static void test_ext_tree_get_node_or_null(void) {
    SECTION_START("Tree: get_node_or_null");

    LCNodeHandle parent = NULL;
    LCResult result = lc_node_create(LC_NODE_BASE, "TreeRoot", &parent);
    TEST_RESULT(result, "Create tree root node");

    LCNodeHandle child = NULL;
    result = lc_node_create(LC_NODE_BASE, "TreeChild", &child);
    TEST_RESULT(result, "Create tree child node");
    result = lc_node_add_child(parent, child);
    TEST_RESULT(result, "Attach tree child");

    LCNodeHandle found = NULL;
    result = lc_node_get_node_or_null(parent, "TreeChild", &found);
    TEST_RESULT(result, "lc_node_get_node_or_null() succeeds for existing path");
    TEST_ASSERT(found != NULL, "lc_node_get_node_or_null() resolves an existing child");

    LCNodeHandle missing = (LCNodeHandle)1;
    result = lc_node_get_node_or_null(parent, "DoesNotExist", &missing);
    TEST_RESULT(result, "lc_node_get_node_or_null() succeeds for a missing path");
    TEST_ASSERT(missing == NULL, "lc_node_get_node_or_null() yields NULL for a missing path");

    TEST_ASSERT(lc_node_get_node_or_null(parent, NULL, &found) == LC_ERROR_NULL_POINTER,
                "lc_node_get_node_or_null(NULL path) rejects null");

    lc_node_destroy(parent);

    SECTION_END("Tree: get_node_or_null");
}

static void test_ext_tree_find_children(void) {
    SECTION_START("Tree: find_children");

    LCNodeHandle parent = NULL;
    LCResult result = lc_node_create(LC_NODE_BASE, "FindRoot", &parent);
    TEST_RESULT(result, "Create find_children root");

    LCNodeHandle child2d = NULL;
    LCNodeHandle child3d = NULL;
    LCNodeHandle grandchild = NULL;
    result = lc_node_create(LC_NODE_2D, "Child2D", &child2d);
    TEST_RESULT(result, "Create Node2D child");
    result = lc_node_create(LC_NODE_3D, "Child3D", &child3d);
    TEST_RESULT(result, "Create Node3D child");
    result = lc_node_create(LC_NODE_2D, "GrandChild2D", &grandchild);
    TEST_RESULT(result, "Create Node2D grandchild");

    lc_node_add_child(parent, child2d);
    lc_node_add_child(parent, child3d);
    lc_node_add_child(child2d, grandchild);

    size_t total = 0;
    result = lc_node_find_children(parent, NULL, true, NULL, 0, &total);
    TEST_RESULT(result, "lc_node_find_children() counts all descendants");
    TEST_ASSERT(total == 3, "lc_node_find_children(any, recursive) finds all 3 descendants");

    size_t directCount = 0;
    result = lc_node_find_children(parent, NULL, false, NULL, 0, &directCount);
    TEST_RESULT(result, "lc_node_find_children() counts direct children");
    TEST_ASSERT(directCount == 2, "lc_node_find_children(any, non-recursive) finds 2 direct children");

    LCNodeHandle matches[8];
    size_t typedCount = 0;
    result = lc_node_find_children(parent, "Node2D", true, matches, 8, &typedCount);
    TEST_RESULT(result, "lc_node_find_children() filters by type");
    TEST_ASSERT(typedCount == 2, "lc_node_find_children(\"Node2D\") finds both Node2D descendants");

    TEST_ASSERT(lc_node_find_children(parent, NULL, true, NULL, 0, NULL) == LC_ERROR_NULL_POINTER,
                "lc_node_find_children(NULL out_count) rejects null");

    lc_node_destroy(parent);

    SECTION_END("Tree: find_children");
}

static void test_ext_tree_first_node_in_group(void) {
    SECTION_START("Tree: get_first_node_in_group");

    LCNodeHandle node = NULL;
    LCResult result = lc_node_create(LC_NODE_BASE, "GroupProbe", &node);
    TEST_RESULT(result, "Create group-probe node");

    LCNodeHandle found = (LCNodeHandle)1;
    result = lc_node_get_first_node_in_group(node, "enemies", &found);
    TEST_RESULT(result, "lc_node_get_first_node_in_group() succeeds for a detached node");
    TEST_ASSERT(found == NULL, "lc_node_get_first_node_in_group() yields NULL with no owning scene");

    TEST_ASSERT(lc_node_get_first_node_in_group(node, NULL, &found) == LC_ERROR_NULL_POINTER,
                "lc_node_get_first_node_in_group(NULL group) rejects null");

    lc_node_destroy(node);

    SECTION_END("Tree: get_first_node_in_group");
}

static void test_ext_tree_is_ancestor_of(void) {
    SECTION_START("Tree: is_ancestor_of");

    LCNodeHandle root = NULL;
    LCNodeHandle child = NULL;
    LCNodeHandle grandchild = NULL;
    LCNodeHandle stranger = NULL;
    lc_node_create(LC_NODE_BASE, "AncRoot", &root);
    lc_node_create(LC_NODE_BASE, "AncChild", &child);
    lc_node_create(LC_NODE_BASE, "AncGrandChild", &grandchild);
    lc_node_create(LC_NODE_BASE, "AncStranger", &stranger);

    lc_node_add_child(root, child);
    lc_node_add_child(child, grandchild);

    TEST_ASSERT(lc_node_is_ancestor_of(root, child) == true, "root is ancestor of direct child");
    TEST_ASSERT(lc_node_is_ancestor_of(root, grandchild) == true, "root is ancestor of grandchild");
    TEST_ASSERT(lc_node_is_ancestor_of(child, root) == false, "child is not ancestor of root");
    TEST_ASSERT(lc_node_is_ancestor_of(root, stranger) == false, "root is not ancestor of unrelated node");
    TEST_ASSERT(lc_node_is_ancestor_of(root, NULL) == false, "lc_node_is_ancestor_of(NULL) returns false");

    lc_node_destroy(root);
    lc_node_destroy(stranger);

    SECTION_END("Tree: is_ancestor_of");
}
