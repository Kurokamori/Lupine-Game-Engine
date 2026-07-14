/* Interface-types C API tests (capi/include/core/lc_interface.h).
 * Exercises the runtime registry path (no project files are loaded in this
 * headless harness, so interfaces are registered via lc_interface_register_json
 * and type conformance via lc_interface_register_type_conformance), then covers
 * definition queries, member verification, node/component conformance, scene
 * lookup, and archetype queries. JSON results are parsed with simple substring
 * checks to stay dependency-free. */

static int ext_iface_contains(const char* haystack, const char* needle) {
    return haystack && needle && strstr(haystack, needle) != NULL;
}

static void test_ext_interface_registry(void) {
    SECTION_START("Ext Interface: Registry & Definitions");

    /* Register a Damageable interface (one method + one signal) at runtime. */
    const char* def =
        "{\"interface_name\":\"Damageable\",\"description\":\"Can take damage\","
        "\"methods\":[{\"name\":\"take_damage\"}],"
        "\"signals\":[{\"name\":\"died\"}],\"tags\":[\"combat\"]}";
    LCResult result = lc_interface_register_json(def);
    TEST_RESULT(result, "lc_interface_register_json() registers Damageable");

    bool exists = false;
    result = lc_interface_exists("Damageable", &exists);
    TEST_RESULT(result, "lc_interface_exists() call succeeds");
    TEST_ASSERT(exists, "lc_interface_exists() reports Damageable present");

    bool missing = true;
    lc_interface_exists("NoSuchInterface", &missing);
    TEST_ASSERT(!missing, "lc_interface_exists() reports unknown interface absent");

    int count = 0;
    result = lc_interface_get_count(&count);
    TEST_RESULT(result, "lc_interface_get_count() call succeeds");
    TEST_ASSERT(count >= 1, "lc_interface_get_count() counts at least one interface");

    char* def_json = NULL;
    result = lc_interface_get_definition_json("Damageable", &def_json);
    TEST_RESULT(result, "lc_interface_get_definition_json() succeeds");
    TEST_ASSERT(ext_iface_contains(def_json, "Damageable"),
                "definition JSON contains the interface name");
    TEST_ASSERT(ext_iface_contains(def_json, "take_damage"),
                "definition JSON contains the required method");
    if (def_json) lc_free(def_json);

    char* missing_json = NULL;
    result = lc_interface_get_definition_json("NoSuchInterface", &missing_json);
    TEST_ASSERT(result == LC_ERROR_NOT_FOUND,
                "lc_interface_get_definition_json() returns NOT_FOUND for unknown");

    /* Verify member coverage: present method+signal => conforms; missing => not. */
    char* verdict = NULL;
    result = lc_interface_verify_members_json("Damageable",
                                              "[\"take_damage\"]", "[\"died\"]", &verdict);
    TEST_RESULT(result, "lc_interface_verify_members_json() succeeds (full)");
    TEST_ASSERT(ext_iface_contains(verdict, "\"conforms\":true"),
                "verify reports conforms when all members present");
    if (verdict) lc_free(verdict);

    char* verdict2 = NULL;
    lc_interface_verify_members_json("Damageable", "[]", "[]", &verdict2);
    TEST_ASSERT(ext_iface_contains(verdict2, "\"conforms\":false"),
                "verify reports non-conformance when members missing");
    TEST_ASSERT(ext_iface_contains(verdict2, "take_damage"),
                "verify lists the missing method");
    if (verdict2) lc_free(verdict2);

    SECTION_END("Ext Interface: Registry & Definitions");
}

static void test_ext_interface_type_conformance(void) {
    SECTION_START("Ext Interface: Native Type Conformance");

    LCResult result = lc_interface_register_type_conformance("HealthComponent", "Damageable");
    TEST_RESULT(result, "lc_interface_register_type_conformance() succeeds");

    bool implements = false;
    result = lc_interface_type_implements("HealthComponent", "Damageable", &implements);
    TEST_RESULT(result, "lc_interface_type_implements() call succeeds");
    TEST_ASSERT(implements, "registered type reports it implements the interface");

    bool other = true;
    lc_interface_type_implements("HealthComponent", "Flammable", &other);
    TEST_ASSERT(!other, "type does not implement an unrelated interface");

    char* types_json = NULL;
    result = lc_interface_get_types_implementing_json("Damageable", &types_json);
    TEST_RESULT(result, "lc_interface_get_types_implementing_json() succeeds");
    TEST_ASSERT(ext_iface_contains(types_json, "HealthComponent"),
                "types-implementing list contains the registered type");
    if (types_json) lc_free(types_json);

    SECTION_END("Ext Interface: Native Type Conformance");
}

static void test_ext_interface_node(void) {
    SECTION_START("Ext Interface: Node Conformance");

    LCNodeHandle node = NULL;
    LCResult result = lc_node_create(LC_NODE_BASE, "InterfaceNode", &node);
    TEST_RESULT(result, "create node for interface tests");

    /* A bare node with no interface-declaring components implements nothing. */
    bool implements = true;
    result = lc_node_implements_interface(node, "Damageable", &implements);
    TEST_RESULT(result, "lc_node_implements_interface() call succeeds");
    TEST_ASSERT(!implements, "bare node does not implement Damageable");

    char* ifaces = NULL;
    result = lc_node_get_interfaces_json(node, &ifaces);
    TEST_RESULT(result, "lc_node_get_interfaces_json() succeeds");
    TEST_ASSERT(ext_iface_contains(ifaces, "[]"),
                "bare node reports an empty interface list");
    if (ifaces) lc_free(ifaces);

    /* Verifying a bare node against a known interface: exists, not conforming. */
    char* verdict = NULL;
    result = lc_node_verify_interface_json(node, "Damageable", &verdict);
    TEST_RESULT(result, "lc_node_verify_interface_json() succeeds");
    TEST_ASSERT(ext_iface_contains(verdict, "\"exists\":true"),
                "node verify reports the interface exists");
    TEST_ASSERT(ext_iface_contains(verdict, "\"conforms\":false"),
                "bare node does not conform to Damageable");
    if (verdict) lc_free(verdict);

    /* Scene lookup with no active scene returns zero, not an error. */
    size_t found = 123;
    result = lc_scene_find_nodes_by_interface("Damageable", NULL, 0, &found);
    TEST_RESULT(result, "lc_scene_find_nodes_by_interface() succeeds with no scene");
    TEST_ASSERT(found == 0, "no nodes found when no scene is active");

    lc_node_destroy(node);

    SECTION_END("Ext Interface: Node Conformance");
}

static void test_ext_interface_archetype(void) {
    SECTION_START("Ext Interface: Archetype Conformance");

    /* No archetypes are loaded in the headless harness; the queries must return
     * cleanly (false / empty) rather than error. */
    bool implements = true;
    LCResult result = lc_archetype_implements_interface("Weapon", "Damageable", &implements);
    TEST_RESULT(result, "lc_archetype_implements_interface() succeeds for unknown class");
    TEST_ASSERT(!implements, "unknown archetype implements nothing");

    char* classes = NULL;
    result = lc_archetype_get_implementing_json("Damageable", &classes);
    TEST_RESULT(result, "lc_archetype_get_implementing_json() succeeds");
    TEST_ASSERT(ext_iface_contains(classes, "[]"),
                "no archetypes implement the interface in a headless run");
    if (classes) lc_free(classes);

    char* ifaces = NULL;
    result = lc_archetype_get_interfaces_json("Weapon", &ifaces);
    TEST_RESULT(result, "lc_archetype_get_interfaces_json() succeeds for unknown class");
    if (ifaces) lc_free(ifaces);

    SECTION_END("Ext Interface: Archetype Conformance");
}
