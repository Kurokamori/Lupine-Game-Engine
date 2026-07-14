/* ============================================================================
 * Extended Save-Game Tests: lc_savedata_* + lc_savegame_*
 *
 * Included as a fragment into capi_test_suite.c. No includes, no main().
 * Exercises the SaveData document accessors and the slot manager (save/load,
 * round-trip, slot enumeration/info, copy/rename/delete, quick/auto helpers,
 * format + obfuscation config, and schema-version reporting).
 * ============================================================================ */

static void test_ext_savedata_document(void) {
    SECTION_START("Ext SaveGame: SaveData document");

    LCSaveDataHandle data = NULL;
    LCResult result = lc_savedata_create(&data);
    TEST_RESULT(result, "lc_savedata_create()");
    TEST_ASSERT(data != NULL, "SaveData handle created");

    TEST_RESULT(lc_savedata_set_bool(data, "flags/intro_done", 1), "set_bool()");
    TEST_RESULT(lc_savedata_set_int(data, "player/level", 7), "set_int()");
    TEST_RESULT(lc_savedata_set_double(data, "player/health", 88.5), "set_double()");
    TEST_RESULT(lc_savedata_set_string(data, "player/name", "Lupine"), "set_string()");
    TEST_RESULT(lc_savedata_set_json(data, "inventory", "[\"sword\",\"shield\"]"), "set_json()");

    int b = 0;
    TEST_RESULT(lc_savedata_get_bool(data, "flags/intro_done", 0, &b), "get_bool()");
    TEST_ASSERT(b == 1, "get_bool() round-trips nested key");

    int64_t lvl = 0;
    TEST_RESULT(lc_savedata_get_int(data, "player/level", 0, &lvl), "get_int()");
    TEST_ASSERT(lvl == 7, "get_int() round-trips");

    double hp = 0.0;
    TEST_RESULT(lc_savedata_get_double(data, "player/health", 0.0, &hp), "get_double()");
    TEST_ASSERT(hp > 88.0 && hp < 89.0, "get_double() round-trips");

    char* name = NULL;
    TEST_RESULT(lc_savedata_get_string(data, "player/name", "", &name), "get_string()");
    TEST_ASSERT(name != NULL && strcmp(name, "Lupine") == 0, "get_string() round-trips");
    lc_free(name);

    int64_t missing = 0;
    TEST_RESULT(lc_savedata_get_int(data, "player/missing", 42, &missing), "get_int() default callable");
    TEST_ASSERT(missing == 42, "get_int() returns default for missing key");

    int has = 0;
    TEST_RESULT(lc_savedata_has(data, "inventory", &has), "has()");
    TEST_ASSERT(has == 1, "has() true for present key");

    char* json = NULL;
    TEST_RESULT(lc_savedata_to_json(data, 0, &json), "to_json()");
    TEST_ASSERT(json != NULL && strstr(json, "Lupine") != NULL, "to_json() contains content");
    lc_free(json);

    TEST_RESULT(lc_savedata_erase(data, "inventory"), "erase()");
    has = 1;
    lc_savedata_has(data, "inventory", &has);
    TEST_ASSERT(has == 0, "erase() removes key");

    TEST_RESULT(lc_savedata_destroy(data), "lc_savedata_destroy()");

    SECTION_END("Ext SaveGame: SaveData document");
}

static void test_ext_savegame_slots(void) {
    SECTION_START("Ext SaveGame: slot manager");

    TEST_RESULT(lc_savegame_set_directory("user://capi_test_saves"), "set_directory()");
    TEST_RESULT(lc_savegame_set_format("json_text"), "set_format()");
    TEST_RESULT(lc_savegame_set_schema_version(1), "set_schema_version()");

    int schema = 0;
    TEST_RESULT(lc_savegame_get_schema_version(&schema), "get_schema_version()");
    TEST_ASSERT(schema == 1, "schema version round-trips");

    LCSaveDataHandle data = NULL;
    lc_savedata_create(&data);
    lc_savedata_set_int(data, "score", 1234);
    lc_savedata_set_string(data, "zone", "forest");

    const char* meta = "{\"title\":\"Slot A\",\"playtime\":120.0}";
    LCResult result = lc_savegame_save("capi_slot_a", data, meta);
    TEST_RESULT(result, "lc_savegame_save()");
    lc_savedata_destroy(data);

    int exists = 0;
    TEST_RESULT(lc_savegame_slot_exists("capi_slot_a", &exists), "slot_exists() callable");
    TEST_ASSERT(exists == 1, "slot_exists() true after save");

    LCSaveDataHandle loaded = NULL;
    result = lc_savegame_load("capi_slot_a", &loaded);
    TEST_RESULT(result, "lc_savegame_load()");
    if (loaded != NULL) {
        int64_t score = 0;
        lc_savedata_get_int(loaded, "score", 0, &score);
        TEST_ASSERT(score == 1234, "loaded data round-trips score");
        char* zone = NULL;
        lc_savedata_get_string(loaded, "zone", "", &zone);
        TEST_ASSERT(zone != NULL && strcmp(zone, "forest") == 0, "loaded data round-trips string");
        lc_free(zone);
        lc_savedata_destroy(loaded);
    }

    char* info = NULL;
    result = lc_savegame_get_slot_info("capi_slot_a", &info);
    TEST_RESULT(result, "get_slot_info()");
    TEST_ASSERT(info != NULL && strstr(info, "Slot A") != NULL, "slot info carries title");
    lc_free(info);

    char* slots = NULL;
    result = lc_savegame_list_slots(&slots);
    TEST_RESULT(result, "list_slots()");
    TEST_ASSERT(slots != NULL && strstr(slots, "capi_slot_a") != NULL, "list_slots() includes saved slot");
    lc_free(slots);

    char* infos = NULL;
    result = lc_savegame_list_slot_infos(&infos);
    TEST_RESULT(result, "list_slot_infos()");
    TEST_ASSERT(infos != NULL, "list_slot_infos() callable");
    lc_free(infos);

    result = lc_savegame_copy_slot("capi_slot_a", "capi_slot_b", 1);
    TEST_RESULT(result, "copy_slot()");
    exists = 0;
    lc_savegame_slot_exists("capi_slot_b", &exists);
    TEST_ASSERT(exists == 1, "copy_slot() creates destination");

    result = lc_savegame_rename_slot("capi_slot_b", "capi_slot_c", 1);
    TEST_RESULT(result, "rename_slot()");
    exists = 1;
    lc_savegame_slot_exists("capi_slot_b", &exists);
    TEST_ASSERT(exists == 0, "rename_slot() removes source");

    TEST_RESULT(lc_savegame_delete_slot("capi_slot_a"), "delete_slot()");
    TEST_RESULT(lc_savegame_delete_slot("capi_slot_c"), "delete_slot() (renamed)");

    SECTION_END("Ext SaveGame: slot manager");
}

static void test_ext_savegame_quick_and_obfuscated(void) {
    SECTION_START("Ext SaveGame: quick/auto + obfuscation");

    lc_savegame_set_directory("user://capi_test_saves");
    TEST_RESULT(lc_savegame_set_quick_slot("capi_quick"), "set_quick_slot()");

    LCSaveDataHandle data = NULL;
    lc_savedata_create(&data);
    lc_savedata_set_int(data, "checkpoint", 3);

    TEST_RESULT(lc_savegame_quick_save(data, NULL), "quick_save()");
    int has = 0;
    TEST_RESULT(lc_savegame_has_quick_save(&has), "has_quick_save()");
    TEST_ASSERT(has == 1, "has_quick_save() true after quick_save");

    LCSaveDataHandle loaded = NULL;
    LCResult result = lc_savegame_quick_load(&loaded);
    TEST_RESULT(result, "quick_load()");
    if (loaded != NULL) {
        int64_t cp = 0;
        lc_savedata_get_int(loaded, "checkpoint", 0, &cp);
        TEST_ASSERT(cp == 3, "quick_load() round-trips");
        lc_savedata_destroy(loaded);
    }

    /* Obfuscated round-trip: save with a key, load with the same key. */
    TEST_RESULT(lc_savegame_set_obfuscation_key("secret-key"), "set_obfuscation_key()");
    result = lc_savegame_save("capi_obf", data, NULL);
    TEST_RESULT(result, "save() with obfuscation");

    LCSaveDataHandle obf = NULL;
    result = lc_savegame_load("capi_obf", &obf);
    TEST_RESULT(result, "load() with obfuscation");
    if (obf != NULL) {
        int64_t cp = 0;
        lc_savedata_get_int(obf, "checkpoint", 0, &cp);
        TEST_ASSERT(cp == 3, "obfuscated save round-trips");
        lc_savedata_destroy(obf);
    }
    TEST_RESULT(lc_savegame_set_obfuscation_key(NULL), "clear obfuscation key");

    lc_savegame_delete_slot("capi_quick");
    lc_savegame_delete_slot("capi_obf");
    lc_savedata_destroy(data);

    SECTION_END("Ext SaveGame: quick/auto + obfuscation");
}
