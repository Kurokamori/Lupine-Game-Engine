/*
 * Extended coverage fragment: the generic reflection / object-model bridge
 * (capi/include/core/lc_reflection.h) — type registry, property get/set by name,
 * component introspection, CallMethod dispatch, and JSON serialization. This is
 * the surface a foreign-language binding drives the engine through generically.
 */

static void test_ext_reflection_type_registry(void) {
    SECTION_START("Reflection: Type Registry");

    int typeCount = -1;
    LCResult result = lc_type_get_count(&typeCount);
    TEST_RESULT(result, "lc_type_get_count");
    TEST_ASSERT(typeCount >= 0, "Registered type count is non-negative");
    printf("    Registered types: %d\n", typeCount);

    if (typeCount > 0) {
        char name[128] = {0};
        result = lc_type_get_name_at(0, name, (uint32_t)sizeof(name));
        TEST_RESULT(result, "lc_type_get_name_at(0)");
        TEST_ASSERT(name[0] != '\0', "First registered type has a non-empty name");
        TEST_ASSERT(lc_type_is_registered(name), "lc_type_is_registered agrees for enumerated name");
        printf("    First type: %s\n", name);
    }

    TEST_ASSERT(!lc_type_is_registered("__definitely_not_a_real_type__"),
                "Unknown type reports not registered");

    LCComponentHandle bogus = NULL;
    result = lc_type_create_component("__definitely_not_a_real_type__", &bogus);
    TEST_ASSERT(result == LC_ERROR_NOT_FOUND, "Creating an unregistered type fails with NOT_FOUND");

    SECTION_END("Reflection: Type Registry");
}

static int test_ext_reflection_find_component_type(char* out_name, uint32_t max_length) {
    int typeCount = 0;
    if (lc_type_get_count(&typeCount) != LC_SUCCESS) {
        return 0;
    }
    for (int i = 0; i < typeCount; ++i) {
        char name[128] = {0};
        if (lc_type_get_name_at(i, name, (uint32_t)sizeof(name)) != LC_SUCCESS) {
            continue;
        }
        LCComponentHandle component = NULL;
        if (lc_type_create_component(name, &component) == LC_SUCCESS && component != NULL) {
            lc_component_destroy(component);
            copy_bounded(out_name, max_length, name);
            return 1;
        }
    }
    return 0;
}

static void test_ext_reflection_component(void) {
    SECTION_START("Reflection: Component Introspection");

    char typeName[128] = {0};
    int found = test_ext_reflection_find_component_type(typeName, (uint32_t)sizeof(typeName));
    TEST_ASSERT(found, "Found at least one creatable registered component type");
    if (!found) {
        SECTION_END("Reflection: Component Introspection");
        return;
    }
    printf("    Using component type: %s\n", typeName);

    LCComponentHandle component = NULL;
    LCResult result = lc_type_create_component(typeName, &component);
    TEST_RESULT(result, "lc_type_create_component");

    char readType[128] = {0};
    result = lc_component_get_type_name(component, readType, (uint32_t)sizeof(readType));
    TEST_RESULT(result, "lc_component_get_type_name");
    TEST_ASSERT(strcmp(readType, typeName) == 0, "Component type name round-trips");

    bool enabled = lc_component_is_enabled(component);
    result = lc_component_set_enabled(component, !enabled);
    TEST_RESULT(result, "lc_component_set_enabled");
    TEST_ASSERT(lc_component_is_enabled(component) == (!enabled), "Component enabled flag round-trips");
    lc_component_set_enabled(component, enabled);

    int propCount = -1;
    result = lc_component_get_property_count(component, &propCount);
    TEST_RESULT(result, "lc_component_get_property_count");
    TEST_ASSERT(propCount >= 0, "Component property count is non-negative");
    printf("    Reflected property descriptors: %d\n", propCount);

    char* descriptors = NULL;
    result = lc_component_get_property_descriptors_json(component, &descriptors);
    TEST_RESULT(result, "lc_component_get_property_descriptors_json");
    TEST_ASSERT(descriptors != NULL && descriptors[0] == '[', "Descriptors serialize as a JSON array");
    lc_free(descriptors);

    char* serialized = NULL;
    result = lc_component_serialize_json(component, &serialized);
    TEST_RESULT(result, "lc_component_serialize_json");
    TEST_ASSERT(serialized != NULL && strstr(serialized, "\"type\"") != NULL,
                "Component serialization contains a type field");

    if (serialized != NULL) {
        result = lc_component_deserialize_json(component, serialized);
        TEST_RESULT(result, "lc_component_deserialize_json round-trips own serialization");
    }
    lc_free(serialized);

    char* callResult = NULL;
    result = lc_component_call_method_json(component, "__no_such_method__", "[]", &callResult);
    TEST_RESULT(result, "lc_component_call_method_json (unknown method handled)");
    TEST_ASSERT(callResult != NULL, "CallMethod returns an allocated JSON result");
    lc_free(callResult);

    result = lc_component_set_property_json(component, "__no_such_property__", "1");
    TEST_ASSERT(result == LC_ERROR_NOT_FOUND, "Setting an unknown property fails with NOT_FOUND");

    result = lc_component_set_property_json(component, "anything", "{not valid json");
    TEST_ASSERT(result == LC_ERROR_INVALID_PARAMETER, "Invalid JSON value is rejected");

    lc_component_destroy(component);
    SECTION_END("Reflection: Component Introspection");
}

static void test_ext_reflection_node(void) {
    SECTION_START("Reflection: Node Object Model");

    LCNodeHandle node = NULL;
    LCResult result = lc_node_create(LC_NODE_3D, "ReflectionNode", &node);
    TEST_RESULT(result, "Create node for reflection");

    char typeName[64] = {0};
    result = lc_node_get_type_name(node, typeName, (uint32_t)sizeof(typeName));
    TEST_RESULT(result, "lc_node_get_type_name");
    TEST_ASSERT(strcmp(typeName, "Node3D") == 0, "Node type name is Node3D");

    TEST_ASSERT(lc_node_has_property(node, "name"), "Node exposes built-in 'name' property");

    char* nameJson = NULL;
    result = lc_node_get_property_json(node, "name", &nameJson);
    TEST_RESULT(result, "lc_node_get_property_json('name')");
    TEST_ASSERT(nameJson != NULL && strcmp(nameJson, "\"ReflectionNode\"") == 0,
                "Node name property reads back as JSON string");
    lc_free(nameJson);

    result = lc_node_set_property_json(node, "name", "\"RenamedNode\"");
    TEST_RESULT(result, "lc_node_set_property_json('name')");
    char* renamed = NULL;
    result = lc_node_get_property_json(node, "name", &renamed);
    TEST_RESULT(result, "Re-read node name after set");
    TEST_ASSERT(renamed != NULL && strcmp(renamed, "\"RenamedNode\"") == 0,
                "Node name property round-trips through reflection");
    lc_free(renamed);

    result = lc_node_set_property_json(node, "__no_such_property__", "1");
    TEST_ASSERT(result == LC_ERROR_NOT_FOUND, "Setting unknown node property fails with NOT_FOUND");

    char* serialized = NULL;
    result = lc_node_serialize_json(node, &serialized);
    TEST_RESULT(result, "lc_node_serialize_json");
    TEST_ASSERT(serialized != NULL && strstr(serialized, "\"properties\"") != NULL,
                "Node serialization contains properties");
    if (serialized != NULL) {
        result = lc_node_deserialize_json(node, serialized);
        TEST_RESULT(result, "lc_node_deserialize_json round-trips own serialization");
    }
    lc_free(serialized);

    int componentCount = -1;
    result = lc_node_get_component_count(node, &componentCount);
    TEST_RESULT(result, "lc_node_get_component_count (empty)");
    TEST_ASSERT(componentCount == 0, "New node has no components");

    char typeForAttach[128] = {0};
    if (test_ext_reflection_find_component_type(typeForAttach, (uint32_t)sizeof(typeForAttach))) {
        LCComponentHandle attached = NULL;
        result = lc_node_add_component_by_type(node, typeForAttach, &attached);
        TEST_RESULT(result, "lc_node_add_component_by_type");
        TEST_ASSERT(attached != NULL, "Attached component handle is valid");

        result = lc_node_get_component_count(node, &componentCount);
        TEST_RESULT(result, "lc_node_get_component_count (after add)");
        TEST_ASSERT(componentCount == 1, "Node reports one component after add");

        TEST_ASSERT(lc_node_has_component(node, typeForAttach), "lc_node_has_component finds attached type");

        LCComponentHandle byType = NULL;
        result = lc_node_get_component_by_type(node, typeForAttach, &byType);
        TEST_RESULT(result, "lc_node_get_component_by_type");

        LCComponentHandle byIndex = NULL;
        result = lc_node_get_component_at(node, 0, &byIndex);
        TEST_RESULT(result, "lc_node_get_component_at(0)");

        result = lc_node_remove_component(node, attached);
        TEST_RESULT(result, "lc_node_remove_component");
        result = lc_node_get_component_count(node, &componentCount);
        TEST_RESULT(result, "lc_node_get_component_count (after remove)");
        TEST_ASSERT(componentCount == 0, "Node has no components after remove");
    }

    lc_node_destroy(node);
    SECTION_END("Reflection: Node Object Model");
}
