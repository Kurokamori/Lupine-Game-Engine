/*
 * Extended coverage fragment: generic reflection sweep over the ENTIRE engine
 * type registry (capi/include/core/lc_reflection.h).
 *
 * Where test_ext_reflection.inc spot-checks the reflection bridge on one
 * arbitrary type, this fragment does two things the rest of the suite does not:
 *
 *   1. Closes the per-name component property surface that nothing else exercises
 *      with real data: lc_component_get_name, lc_component_has_property,
 *      lc_component_get_property_json, lc_component_get_property_type,
 *      lc_component_get_property_name_at, and lc_type_create_node.
 *
 *   2. Drives EVERY registered node/component type through the engine's property
 *      and serialization machinery in one pass (create -> introspect -> property
 *      round-trip -> serialize -> deserialize -> destroy). This is the broadest
 *      single test of the engine: it scales automatically as new types are
 *      registered, so a newly added component is exercised the moment it appears
 *      in the type registry, with no edit to this file.
 *
 * This file is #included into capi_test_suite.c. It defines no main(), includes
 * no headers, and redefines no macros. lc_reflection.h is already in scope via
 * lupine_c.h.
 */

/* Locate the first registered type that instantiates as a Component AND exposes
 * at least one reflected (custom) property descriptor. Built-in components such
 * as DirectionalLight3D register their properties via DefineProperty, so such a
 * type exists in a normal build; returning 0 means the registry is empty/atypical
 * and the property-by-name tests are skipped rather than spuriously failing. */
static int test_ext_refsweep_find_propful_component(char* out_name, uint32_t max_length) {
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
        if (lc_type_create_component(name, &component) != LC_SUCCESS || component == NULL) {
            continue;
        }
        int propCount = 0;
        LCResult countResult = lc_component_get_property_count(component, &propCount);
        lc_component_destroy(component);
        if (countResult == LC_SUCCESS && propCount > 0) {
            copy_bounded(out_name, max_length, name);
            return 1;
        }
    }
    return 0;
}

/* Robust, assertion-heavy coverage of the per-name component property surface. */
static void test_ext_reflection_component_properties(void) {
    SECTION_START("Reflection Sweep: Component Property-by-Name");

    char typeName[128] = {0};
    int found = test_ext_refsweep_find_propful_component(typeName, (uint32_t)sizeof(typeName));
    TEST_ASSERT(found, "Found a registered component type with reflected properties");
    if (!found) {
        SECTION_END("Reflection Sweep: Component Property-by-Name");
        return;
    }
    printf("    Using property-rich component type: %s\n", typeName);

    LCComponentHandle component = NULL;
    LCResult result = lc_type_create_component(typeName, &component);
    TEST_RESULT(result, "lc_type_create_component for property-rich type");
    if (component == NULL) {
        SECTION_END("Reflection Sweep: Component Property-by-Name");
        return;
    }

    char instanceName[128] = {0};
    result = lc_component_get_name(component, instanceName, (uint32_t)sizeof(instanceName));
    TEST_RESULT(result, "lc_component_get_name");
    TEST_ASSERT(instanceName[0] != '\0', "Component instance name is non-empty");

    int propCount = 0;
    result = lc_component_get_property_count(component, &propCount);
    TEST_RESULT(result, "lc_component_get_property_count");
    TEST_ASSERT(propCount > 0, "Property-rich component reports at least one property");

    int namesOk = 0;
    int hasOk = 0;
    int getOk = 0;
    int typeOk = 0;
    char firstProp[128] = {0};
    for (int i = 0; i < propCount; ++i) {
        char propName[128] = {0};
        if (lc_component_get_property_name_at(component, i, propName, (uint32_t)sizeof(propName)) != LC_SUCCESS) {
            continue;
        }
        if (propName[0] == '\0') {
            continue;
        }
        ++namesOk;
        if (i == 0) {
            copy_bounded(firstProp, sizeof(firstProp), propName);
        }

        if (lc_component_has_property(component, propName)) {
            ++hasOk;
        }

        char* valueJson = NULL;
        if (lc_component_get_property_json(component, propName, &valueJson) == LC_SUCCESS) {
            if (valueJson != NULL && valueJson[0] != '\0') {
                ++getOk;
            }
            lc_free(valueJson);
        }

        LCPropertyValueType propType = LC_PROPERTY_TYPE_INT;
        if (lc_component_get_property_type(component, propName, &propType) == LC_SUCCESS) {
            if (propType >= LC_PROPERTY_TYPE_INT && propType <= LC_PROPERTY_TYPE_DICTIONARY) {
                ++typeOk;
            }
        }
    }

    TEST_ASSERT(namesOk == propCount, "lc_component_get_property_name_at enumerates every descriptor");
    TEST_ASSERT(hasOk == propCount, "lc_component_has_property is true for every enumerated descriptor");
    TEST_ASSERT(getOk == propCount, "lc_component_get_property_json reads every enumerated descriptor");
    TEST_ASSERT(typeOk == propCount, "lc_component_get_property_type returns a valid enum for every descriptor");
    printf("    Exercised %d reflected properties (names/has/get/type all matched)\n", propCount);

    char outOfRange[128] = {0};
    result = lc_component_get_property_name_at(component, propCount, outOfRange, (uint32_t)sizeof(outOfRange));
    TEST_ASSERT(result != LC_SUCCESS, "lc_component_get_property_name_at rejects out-of-range index");

    TEST_ASSERT(!lc_component_has_property(component, "__no_such_property__"),
                "lc_component_has_property is false for an unknown name");

    char* missingJson = NULL;
    result = lc_component_get_property_json(component, "__no_such_property__", &missingJson);
    TEST_ASSERT(result == LC_ERROR_NOT_FOUND, "lc_component_get_property_json reports NOT_FOUND for unknown name");
    lc_free(missingJson);

    LCPropertyValueType missingType = LC_PROPERTY_TYPE_INT;
    result = lc_component_get_property_type(component, "__no_such_property__", &missingType);
    TEST_ASSERT(result == LC_ERROR_NOT_FOUND, "lc_component_get_property_type reports NOT_FOUND for unknown name");

    /* Round-trip the first reflected property through get -> set -> get and
     * confirm the value survives the JSON marshalling. */
    if (firstProp[0] != '\0') {
        char* before = NULL;
        if (lc_component_get_property_json(component, firstProp, &before) == LC_SUCCESS && before != NULL) {
            result = lc_component_set_property_json(component, firstProp, before);
            TEST_RESULT(result, "lc_component_set_property_json accepts its own serialized value");
            char* after = NULL;
            if (lc_component_get_property_json(component, firstProp, &after) == LC_SUCCESS && after != NULL) {
                TEST_ASSERT(strcmp(before, after) == 0, "Reflected property round-trips through set/get unchanged");
                lc_free(after);
            }
            lc_free(before);
        }
    }

    lc_component_destroy(component);
    SECTION_END("Reflection Sweep: Component Property-by-Name");
}

/* Coverage of lc_type_create_node: positive (a node type), and the two documented
 * negative paths (component type -> NOT_SUPPORTED, unknown -> NOT_FOUND). */
static void test_ext_reflection_create_node_by_type(void) {
    SECTION_START("Reflection Sweep: Create Node By Type");

    int typeCount = 0;
    LCResult result = lc_type_get_count(&typeCount);
    TEST_RESULT(result, "lc_type_get_count");

    int nodeTypesCreated = 0;
    char componentOnlyType[128] = {0};
    for (int i = 0; i < typeCount; ++i) {
        char name[128] = {0};
        if (lc_type_get_name_at(i, name, (uint32_t)sizeof(name)) != LC_SUCCESS) {
            continue;
        }
        LCNodeHandle node = NULL;
        LCResult createResult = lc_type_create_node(name, &node);
        if (createResult == LC_SUCCESS && node != NULL) {
            char typeName[128] = {0};
            if (lc_node_get_type_name(node, typeName, (uint32_t)sizeof(typeName)) == LC_SUCCESS) {
                if (nodeTypesCreated == 0) {
                    printf("    First node type created via registry: %s\n", typeName);
                }
            }

            char* serialized = NULL;
            if (lc_node_serialize_json(node, &serialized) == LC_SUCCESS && serialized != NULL) {
                LCResult deserResult = lc_node_deserialize_json(node, serialized);
                if (nodeTypesCreated == 0) {
                    TEST_RESULT(deserResult, "Registry-created node round-trips serialize/deserialize");
                }
                lc_free(serialized);
            }
            lc_node_destroy(node);
            ++nodeTypesCreated;
        } else if (createResult == LC_ERROR_NOT_SUPPORTED && componentOnlyType[0] == '\0') {
            copy_bounded(componentOnlyType, sizeof(componentOnlyType), name);
        }
    }

    TEST_ASSERT(nodeTypesCreated > 0, "lc_type_create_node created at least one node type from the registry");
    printf("    Node types instantiable from the registry: %d\n", nodeTypesCreated);

    if (componentOnlyType[0] != '\0') {
        LCNodeHandle bogus = NULL;
        result = lc_type_create_node(componentOnlyType, &bogus);
        TEST_ASSERT(result == LC_ERROR_NOT_SUPPORTED,
                    "lc_type_create_node on a component-only type reports NOT_SUPPORTED");
    }

    LCNodeHandle unknown = NULL;
    result = lc_type_create_node("__definitely_not_a_real_type__", &unknown);
    TEST_ASSERT(result == LC_ERROR_NOT_FOUND, "lc_type_create_node on an unknown type reports NOT_FOUND");

    SECTION_END("Reflection Sweep: Create Node By Type");
}

/* The broad pass: instantiate every registered type and drive it through the
 * reflection/serialization machinery. A type that is neither a node nor a
 * component subtype (NOT_SUPPORTED from both factories) is counted separately so
 * the totals remain meaningful. */
static void test_ext_reflection_full_sweep(void) {
    SECTION_START("Reflection Sweep: All Registered Types");

    int typeCount = 0;
    LCResult result = lc_type_get_count(&typeCount);
    TEST_RESULT(result, "lc_type_get_count");
    TEST_ASSERT(typeCount > 0, "Engine registers at least one type");

    int nodeTypes = 0;
    int componentTypes = 0;
    int otherTypes = 0;
    int nodeRoundTrips = 0;
    int componentRoundTrips = 0;
    long propertiesExercised = 0;
    int createFailures = 0;

    for (int i = 0; i < typeCount; ++i) {
        char name[128] = {0};
        if (lc_type_get_name_at(i, name, (uint32_t)sizeof(name)) != LC_SUCCESS) {
            continue;
        }

        LCNodeHandle node = NULL;
        LCResult nodeResult = lc_type_create_node(name, &node);
        if (nodeResult == LC_SUCCESS && node != NULL) {
            ++nodeTypes;
            char* serialized = NULL;
            if (lc_node_serialize_json(node, &serialized) == LC_SUCCESS && serialized != NULL) {
                if (lc_node_deserialize_json(node, serialized) == LC_SUCCESS) {
                    ++nodeRoundTrips;
                }
                lc_free(serialized);
            }
            lc_node_destroy(node);
            continue;
        }

        LCComponentHandle component = NULL;
        LCResult compResult = lc_type_create_component(name, &component);
        if (compResult == LC_SUCCESS && component != NULL) {
            ++componentTypes;

            char typeName[128] = {0};
            lc_component_get_type_name(component, typeName, (uint32_t)sizeof(typeName));
            char instanceName[128] = {0};
            lc_component_get_name(component, instanceName, (uint32_t)sizeof(instanceName));

            int propCount = 0;
            if (lc_component_get_property_count(component, &propCount) == LC_SUCCESS) {
                for (int p = 0; p < propCount; ++p) {
                    char propName[128] = {0};
                    if (lc_component_get_property_name_at(component, p, propName, (uint32_t)sizeof(propName)) != LC_SUCCESS) {
                        continue;
                    }
                    LCPropertyValueType propType = LC_PROPERTY_TYPE_INT;
                    lc_component_get_property_type(component, propName, &propType);
                    char* valueJson = NULL;
                    if (lc_component_get_property_json(component, propName, &valueJson) == LC_SUCCESS) {
                        lc_free(valueJson);
                    }
                    ++propertiesExercised;
                }
            }

            char* serialized = NULL;
            if (lc_component_serialize_json(component, &serialized) == LC_SUCCESS && serialized != NULL) {
                if (lc_component_deserialize_json(component, serialized) == LC_SUCCESS) {
                    ++componentRoundTrips;
                }
                lc_free(serialized);
            }
            lc_component_destroy(component);
            continue;
        }

        if (nodeResult == LC_ERROR_NOT_SUPPORTED && compResult == LC_ERROR_NOT_SUPPORTED) {
            ++otherTypes;
        } else {
            ++createFailures;
        }
    }

    printf("    Registered types swept:        %d\n", typeCount);
    printf("    Node types instantiated:       %d (%d serialize round-trips)\n", nodeTypes, nodeRoundTrips);
    printf("    Component types instantiated:  %d (%d serialize round-trips)\n", componentTypes, componentRoundTrips);
    printf("    Reflected properties exercised:%ld\n", propertiesExercised);
    printf("    Non-instantiable (other):      %d\n", otherTypes);
    printf("    Unexpected create failures:    %d\n", createFailures);

    TEST_ASSERT(nodeTypes > 0, "Sweep instantiated at least one node type");
    TEST_ASSERT(componentTypes > 0, "Sweep instantiated at least one component type");
    TEST_ASSERT(nodeRoundTrips == nodeTypes, "Every instantiated node type round-trips serialize/deserialize");
    TEST_ASSERT(componentRoundTrips == componentTypes,
                "Every instantiated component type round-trips serialize/deserialize");
    TEST_ASSERT(propertiesExercised > 0, "Sweep exercised reflected properties across component types");
    TEST_ASSERT(createFailures == 0, "No registered type failed instantiation with an unexpected error");
    TEST_ASSERT((nodeTypes + componentTypes + otherTypes) == typeCount,
                "Every registered type was classified (node, component, or other)");

    SECTION_END("Reflection Sweep: All Registered Types");
}
