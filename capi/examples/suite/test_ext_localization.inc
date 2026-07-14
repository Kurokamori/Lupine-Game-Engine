static void test_ext_localization_translation(void) {
    SECTION_START("Localization: Translation Lookup");

    char buffer[512];
    LCResult result;

    buffer[0] = '\0';
    result = lc_localization_tr("test_key_unlikely_to_exist", buffer, sizeof(buffer));
    TEST_RESULT(result, "tr returns success for any key");
    TEST_ASSERT(buffer[0] != '\0', "tr writes a non-empty string (falls back to key)");

    buffer[0] = '\0';
    result = lc_localization_tr_in_table("ui", "test_key_unlikely_to_exist",
                                         buffer, sizeof(buffer));
    TEST_RESULT(result, "tr_in_table returns success");
    TEST_ASSERT(buffer[0] != '\0', "tr_in_table writes a non-empty string");

    const char* arg_names[2] = { "name", "place" };
    const char* arg_values[2] = { "Lupine", "Engine" };
    buffer[0] = '\0';
    result = lc_localization_tr_format("greeting_test_key", arg_names, arg_values, 2,
                                       buffer, sizeof(buffer));
    TEST_RESULT(result, "tr_format returns success");
    TEST_ASSERT(buffer[0] != '\0', "tr_format writes a non-empty string");

    buffer[0] = '\0';
    result = lc_localization_tr_format("greeting_test_key", NULL, NULL, 0,
                                       buffer, sizeof(buffer));
    TEST_RESULT(result, "tr_format with zero args returns success");

    buffer[0] = '\0';
    result = lc_localization_tr_plural("item_count_test_key", 1, buffer, sizeof(buffer));
    TEST_RESULT(result, "tr_plural (count=1) returns success");
    TEST_ASSERT(buffer[0] != '\0', "tr_plural singular writes a non-empty string");

    buffer[0] = '\0';
    result = lc_localization_tr_plural("item_count_test_key", 5, buffer, sizeof(buffer));
    TEST_RESULT(result, "tr_plural (count=5) returns success");
    TEST_ASSERT(buffer[0] != '\0', "tr_plural plural writes a non-empty string");

    int has_key = -1;
    result = lc_localization_has_key("test_key_unlikely_to_exist", &has_key);
    TEST_RESULT(result, "has_key returns success");
    TEST_ASSERT(has_key == 0 || has_key == 1, "has_key writes a 0/1 result");

    SECTION_END("Localization: Translation Lookup");
}

static void test_ext_localization_locale(void) {
    SECTION_START("Localization: Locale Management");

    char active_locale[128];
    char buffer[128];
    LCResult result;

    active_locale[0] = '\0';
    result = lc_localization_get_locale(active_locale, sizeof(active_locale));
    TEST_RESULT(result, "get_locale returns success");
    TEST_ASSERT(active_locale[0] != '\0', "get_locale writes a non-empty locale");

    buffer[0] = '\0';
    result = lc_localization_get_fallback_locale(buffer, sizeof(buffer));
    TEST_RESULT(result, "get_fallback_locale returns success");

    int locale_count = -1;
    result = lc_localization_get_locale_count(&locale_count);
    TEST_RESULT(result, "get_locale_count returns success");
    TEST_ASSERT(locale_count >= 0, "locale count is non-negative");
    printf("    Available locales: %d\n", locale_count);

    for (int index = 0; index < locale_count; index++) {
        buffer[0] = '\0';
        result = lc_localization_get_locale_at(index, buffer, sizeof(buffer));
        TEST_RESULT(result, "get_locale_at returns success");
        TEST_ASSERT(buffer[0] != '\0', "get_locale_at writes a non-empty locale");
        printf("      [%d] %s\n", index, buffer);
    }

    result = lc_localization_set_locale("en");
    TEST_RESULT(result, "set_locale to 'en' returns success");

    buffer[0] = '\0';
    result = lc_localization_get_locale(buffer, sizeof(buffer));
    TEST_RESULT(result, "get_locale after set returns success");
    TEST_ASSERT(strcmp(buffer, "en") == 0, "active locale round-trips to 'en'");

    result = lc_localization_set_locale(active_locale);
    TEST_RESULT(result, "restore original locale");

    SECTION_END("Localization: Locale Management");
}

static void test_ext_localization_maintenance(void) {
    SECTION_START("Localization: Maintenance & State");

    LCResult result;

    result = lc_localization_reload();
    TEST_RESULT(result, "reload returns success");

    int enabled_state = -1;
    result = lc_localization_is_enabled(&enabled_state);
    TEST_RESULT(result, "is_enabled returns success");
    TEST_ASSERT(enabled_state == 0 || enabled_state == 1, "is_enabled writes a 0/1 result");

    int original_pseudo = -1;
    result = lc_localization_is_pseudolocalization(&original_pseudo);
    TEST_RESULT(result, "is_pseudolocalization returns success");
    TEST_ASSERT(original_pseudo == 0 || original_pseudo == 1,
                "is_pseudolocalization writes a 0/1 result");

    result = lc_localization_set_pseudolocalization(1);
    TEST_RESULT(result, "enable pseudolocalization");

    int pseudo_state = -1;
    result = lc_localization_is_pseudolocalization(&pseudo_state);
    TEST_RESULT(result, "query pseudolocalization after enable");
    TEST_ASSERT(pseudo_state == 1, "pseudolocalization is enabled");

    result = lc_localization_set_pseudolocalization(0);
    TEST_RESULT(result, "disable pseudolocalization");

    pseudo_state = -1;
    result = lc_localization_is_pseudolocalization(&pseudo_state);
    TEST_RESULT(result, "query pseudolocalization after disable");
    TEST_ASSERT(pseudo_state == 0, "pseudolocalization is disabled");

    result = lc_localization_set_pseudolocalization(original_pseudo);
    TEST_RESULT(result, "restore original pseudolocalization state");

    SECTION_END("Localization: Maintenance & State");
}
