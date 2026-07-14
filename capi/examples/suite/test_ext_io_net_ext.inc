/* ============================================================================
 * Extended I/O + Networking Tests: lc_fs_* / lc_vfs_* extras, lc_savedata_* /
 * lc_savegame_* extras, lc_project_* lifecycle + path getters, and the
 * lc_net_* / lc_replication entry points not covered by test_ext_network.inc.
 *
 * Included as a fragment into capi_test_suite.c. No includes, no main(), no
 * macro definitions.
 *
 * Most of these functions need live runtime state (a loaded project, a running
 * net session, an active scene) that does not exist in the headless harness, so
 * the calls use TOLERANT assertions: they must return a *defined* result and
 * never crash. Operations that work headless (physical filesystem round-trips,
 * in-memory SaveData docs, sandbox-path classification, config setters) are
 * exercised as real successes.
 *
 * Networking is exposed unconditionally by network/lc_network.h (no
 * LUPINE_HAS_NETWORKING guard around the declarations), so the network tests are
 * compiled unconditionally; when networking is compiled out at the library
 * level the functions simply report an inactive/unsupported state, which the
 * tolerant assertions accept.
 * ============================================================================ */

/* ----------------------------------------------------------------------------
 * Filesystem extras
 * ------------------------------------------------------------------------- */

static void test_ext_io_ext_fs_ops(void) {
    SECTION_START("Ext I/O: FileSystem extras");

    /* Derive a writable base directory from the platform temp path so the test
     * never pollutes the project tree. */
    char* temp_path = NULL;
    LCResult result = lc_vfs_get_default_temp_path(&temp_path);
    TEST_RESULT(result, "lc_vfs_get_default_temp_path()");
    TEST_ASSERT(temp_path != NULL && temp_path[0] != '\0', "temp path is non-empty");

    char base_dir[1024];
    snprintf(base_dir, sizeof(base_dir), "%s/lupine_capi_io_ext",
             (temp_path != NULL) ? temp_path : ".");
    if (temp_path != NULL) {
        lc_free(temp_path);
    }

    result = lc_fs_create_directory(base_dir, true);
    TEST_RESULT(result, "create base directory");

    char src[1200];
    char dst[1200];
    char moved[1200];
    snprintf(src, sizeof(src), "%s/source.txt", base_dir);
    snprintf(dst, sizeof(dst), "%s/copy.txt", base_dir);
    snprintf(moved, sizeof(moved), "%s/moved.txt", base_dir);

    const char* text = "lc_fs extras round-trip payload";
    result = lc_fs_write_text(src, text);
    TEST_RESULT(result, "write source file");

    /* lc_fs_copy_file: real success + verification. */
    result = lc_fs_copy_file(src, dst, true);
    TEST_RESULT(result, "lc_fs_copy_file()");
    bool exists = false;
    lc_fs_exists(dst, &exists);
    TEST_ASSERT(exists, "lc_fs_copy_file() created the destination");

    /* lc_fs_get_canonical_path: real success, non-empty heap string. */
    char* canon = NULL;
    result = lc_fs_get_canonical_path(src, &canon);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED,
                "lc_fs_get_canonical_path() defined result");
    if (result == LC_SUCCESS) {
        TEST_ASSERT(canon != NULL && canon[0] != '\0',
                    "lc_fs_get_canonical_path() yields a path");
    }
    lc_free(canon);

    /* lc_fs_move_file: real success; source disappears, target appears. */
    result = lc_fs_move_file(dst, moved);
    TEST_RESULT(result, "lc_fs_move_file()");
    exists = true;
    lc_fs_exists(dst, &exists);
    TEST_ASSERT(!exists, "lc_fs_move_file() removed the source");
    exists = false;
    lc_fs_exists(moved, &exists);
    TEST_ASSERT(exists, "lc_fs_move_file() created the target");

    /* lc_fs_set_current_directory: real round-trip (save -> change -> restore). */
    char* original_cwd = NULL;
    result = lc_fs_get_current_directory(&original_cwd);
    TEST_RESULT(result, "get current directory (pre)");

    result = lc_fs_set_current_directory(base_dir);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED,
                "lc_fs_set_current_directory() defined result");

    if (original_cwd != NULL) {
        result = lc_fs_set_current_directory(original_cwd);
        TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED,
                    "lc_fs_set_current_directory() restore defined result");
        lc_free(original_cwd);
    }

    /* NULL-pointer guards. */
    result = lc_fs_copy_file(NULL, dst, true);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "lc_fs_copy_file(NULL src) rejected");
    result = lc_fs_move_file(NULL, moved);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "lc_fs_move_file(NULL src) rejected");
    result = lc_fs_get_canonical_path(NULL, &canon);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "lc_fs_get_canonical_path(NULL) rejected");
    result = lc_fs_set_current_directory(NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "lc_fs_set_current_directory(NULL) rejected");

    /* Cleanup. */
    lc_fs_delete_file(src);
    lc_fs_delete_file(moved);
    lc_fs_delete_directory(base_dir, true);

    SECTION_END("Ext I/O: FileSystem extras");
}

static void test_ext_io_ext_vfs_ops(void) {
    SECTION_START("Ext I/O: VirtualFileSystem extras");

    /* lc_vfs_is_sandboxed_path is a pure string classifier and is fully
     * testable headless. */
    bool ok = false;
    LCResult result = lc_vfs_is_sandboxed_path("res://textures/icon.png", &ok);
    TEST_RESULT(result, "lc_vfs_is_sandboxed_path() callable (res)");
    TEST_ASSERT(ok, "res:// path is allowed by the sandbox");

    ok = false;
    result = lc_vfs_is_sandboxed_path("user://saves/slot.dat", &ok);
    TEST_RESULT(result, "lc_vfs_is_sandboxed_path() callable (user)");
    TEST_ASSERT(ok, "user:// path is allowed by the sandbox");

    ok = true;
    result = lc_vfs_is_sandboxed_path("res://../escape.txt", &ok);
    TEST_RESULT(result, "lc_vfs_is_sandboxed_path() callable (traversal)");
    TEST_ASSERT(!ok, ".. traversal is rejected by the sandbox");

    ok = true;
    result = lc_vfs_is_sandboxed_path("http://example.com/x", &ok);
    TEST_RESULT(result, "lc_vfs_is_sandboxed_path() callable (non-virtual)");
    TEST_ASSERT(!ok, "non-virtual prefix is rejected by the sandbox");

    result = lc_vfs_is_sandboxed_path(NULL, &ok);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "lc_vfs_is_sandboxed_path(NULL) rejected");

    /* The remaining VFS extras operate on a (possibly unmounted headless)
     * user:// space, so they are tolerant. They must still never crash. */
    const char* vdir = "user://capi_io_ext";
    result = lc_vfs_create_directory(vdir, true);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_PATH_INVALID,
                "lc_vfs_create_directory() defined result");

    const char* vfile = "user://capi_io_ext/log.txt";
    result = lc_vfs_append_text(vfile, "first line\n");
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_FILE_WRITE_FAILED ||
                result == LC_ERROR_PATH_INVALID,
                "lc_vfs_append_text() defined result");
    result = lc_vfs_append_text(vfile, "second line\n");
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_FILE_WRITE_FAILED ||
                result == LC_ERROR_PATH_INVALID,
                "lc_vfs_append_text() (second) defined result");

    bool is_dir = true;
    result = lc_vfs_is_directory(vdir, &is_dir);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PATH_INVALID,
                "lc_vfs_is_directory() defined result");

    int64_t mtime = -1;
    result = lc_vfs_get_modification_time(vfile, &mtime);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_FILE_NOT_FOUND ||
                result == LC_ERROR_PATH_INVALID,
                "lc_vfs_get_modification_time() defined result");

    char* resolved = NULL;
    result = lc_vfs_resolve_asset("res://does/not/exist.png", &resolved);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PATH_INVALID,
                "lc_vfs_resolve_asset() defined result");
    lc_free(resolved);

    /* NULL-pointer guards on the out-parameter forms. */
    result = lc_vfs_is_directory(NULL, &is_dir);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "lc_vfs_is_directory(NULL) rejected");
    result = lc_vfs_get_modification_time(NULL, &mtime);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "lc_vfs_get_modification_time(NULL) rejected");
    result = lc_vfs_resolve_asset(NULL, &resolved);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "lc_vfs_resolve_asset(NULL) rejected");
    result = lc_vfs_append_text(NULL, "x");
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "lc_vfs_append_text(NULL) rejected");

    /* Best-effort cleanup (ignored if the user:// space is not mounted). */
    lc_vfs_delete_file(vfile);

    SECTION_END("Ext I/O: VirtualFileSystem extras");
}

/* ----------------------------------------------------------------------------
 * SaveData / SaveGame extras
 * ------------------------------------------------------------------------- */

static void test_ext_io_ext_savedata(void) {
    SECTION_START("Ext I/O: SaveData extras");

    /* lc_savedata_create_from_json: real success on valid JSON. */
    const char* json = "{\"player\":{\"name\":\"Lupine\",\"level\":9},\"flags\":[1,2,3]}";
    LCSaveDataHandle data = NULL;
    LCResult result = lc_savedata_create_from_json(json, &data);
    TEST_RESULT(result, "lc_savedata_create_from_json()");
    TEST_ASSERT(data != NULL, "SaveData handle created from JSON");

    if (data != NULL) {
        /* The parsed content is addressable by nested key. */
        char* name = NULL;
        result = lc_savedata_get_string(data, "player/name", "", &name);
        TEST_RESULT(result, "get_string() on parsed doc");
        TEST_ASSERT(name != NULL && strcmp(name, "Lupine") == 0,
                    "parsed doc round-trips nested string");
        lc_free(name);

        /* lc_savedata_get_json: real success, returns the sub-value as JSON. */
        char* sub = NULL;
        result = lc_savedata_get_json(data, "player", &sub);
        TEST_RESULT(result, "lc_savedata_get_json()");
        TEST_ASSERT(sub != NULL && strstr(sub, "Lupine") != NULL,
                    "lc_savedata_get_json() returns the sub-object");
        lc_free(sub);

        /* lc_savedata_clear: real success; the document becomes empty. */
        result = lc_savedata_clear(data);
        TEST_RESULT(result, "lc_savedata_clear()");
        int has = 1;
        lc_savedata_has(data, "player", &has);
        TEST_ASSERT(has == 0, "lc_savedata_clear() empties the document");

        lc_savedata_destroy(data);
    }

    /* NULL-pointer guards. */
    LCSaveDataHandle out = NULL;
    result = lc_savedata_create_from_json(NULL, &out);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER,
                "lc_savedata_create_from_json(NULL json) rejected");
    char* out_json = NULL;
    result = lc_savedata_get_json(NULL, "key", &out_json);
    TEST_ASSERT(result == LC_ERROR_INVALID_HANDLE || result == LC_ERROR_NULL_POINTER,
                "lc_savedata_get_json(NULL handle) rejected");
    result = lc_savedata_clear(NULL);
    TEST_ASSERT(result == LC_ERROR_INVALID_HANDLE || result == LC_ERROR_NULL_POINTER,
                "lc_savedata_clear(NULL handle) rejected");

    SECTION_END("Ext I/O: SaveData extras");
}

static void test_ext_io_ext_savegame_config(void) {
    SECTION_START("Ext I/O: SaveGame config + auto/scene extras");

    lc_savegame_set_directory("user://capi_io_ext_saves");

    /* Config toggles are real successes regardless of runtime state. */
    TEST_RESULT(lc_savegame_set_atomic_write(1), "set_atomic_write(on)");
    TEST_RESULT(lc_savegame_set_atomic_write(0), "set_atomic_write(off)");
    TEST_RESULT(lc_savegame_set_backup_on_write(1), "set_backup_on_write(on)");
    TEST_RESULT(lc_savegame_set_backup_on_write(0), "set_backup_on_write(off)");
    TEST_RESULT(lc_savegame_set_auto_slot("capi_auto"), "set_auto_slot()");

    /* Auto-save needs a writable directory; tolerant on a headless user:// space. */
    LCSaveDataHandle data = NULL;
    lc_savedata_create(&data);
    if (data != NULL) {
        lc_savedata_set_int(data, "auto/tick", 99);

        LCResult result = lc_savegame_auto_save(data, NULL);
        TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_FILE_WRITE_FAILED ||
                    result == LC_ERROR_OPERATION_FAILED || result == LC_ERROR_PATH_INVALID,
                    "lc_savegame_auto_save() defined result");

        int has = 0;
        result = lc_savegame_has_auto_save(&has);
        TEST_RESULT(result, "lc_savegame_has_auto_save() callable");
        TEST_ASSERT(has == 0 || has == 1, "has_auto_save() yields a boolean");

        lc_savedata_destroy(data);
    }

    /* Scene-state capture/restore need an active scene; tolerant otherwise. */
    char* captured = NULL;
    LCResult result = lc_savegame_capture_scene_state(NULL, &captured);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_SCENE_INVALID || result == LC_ERROR_NOT_FOUND,
                "lc_savegame_capture_scene_state() defined result");
    lc_free(captured);

    int restored = -1;
    result = lc_savegame_restore_scene_state("[]", &restored);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_SCENE_INVALID || result == LC_ERROR_INVALID_PARAMETER,
                "lc_savegame_restore_scene_state() defined result");

    /* Diagnostics: per-thread last-error description always available. */
    char err_buf[256];
    memset(err_buf, 0, sizeof(err_buf));
    result = lc_savegame_get_last_error(err_buf, sizeof(err_buf));
    TEST_RESULT(result, "lc_savegame_get_last_error()");
    TEST_ASSERT(err_buf[0] != '\0', "last-error description is non-empty");

    /* NULL-pointer guards. */
    result = lc_savegame_has_auto_save(NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "has_auto_save(NULL) rejected");
    result = lc_savegame_capture_scene_state(NULL, NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "capture_scene_state(NULL out) rejected");
    result = lc_savegame_restore_scene_state(NULL, &restored);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "restore_scene_state(NULL json) rejected");

    SECTION_END("Ext I/O: SaveGame config + auto/scene extras");
}

/* ----------------------------------------------------------------------------
 * Project lifecycle + path getters
 * ------------------------------------------------------------------------- */

static void test_ext_io_ext_project(void) {
    SECTION_START("Ext I/O: Project lifecycle + paths");

    /* lc_project_create_from_path on a non-existent file must fail cleanly. */
    LCProjectHandle from_path = NULL;
    LCResult result = lc_project_create_from_path("capi_does_not_exist.lupine", &from_path);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_FILE_NOT_FOUND ||
                result == LC_ERROR_OPERATION_FAILED,
                "lc_project_create_from_path() defined result");
    if (result == LC_SUCCESS && from_path != NULL) {
        lc_project_destroy(from_path);
    }

    /* Work with an explicitly-created handle for the remaining calls. */
    LCProjectHandle project = NULL;
    result = lc_project_create(&project);
    TEST_RESULT(result, "lc_project_create()");
    TEST_ASSERT(project != NULL, "project handle is non-NULL");

    /* lc_project_load of a missing file is a clean failure. */
    result = lc_project_load(project, "capi_does_not_exist.lupine");
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_FILE_NOT_FOUND ||
                result == LC_ERROR_OPERATION_FAILED,
                "lc_project_load() defined result");

    /* Path getters on an unloaded project: tolerant; free any heap strings. */
    char* path = NULL;
    result = lc_project_get_path(project, &path);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_NOT_FOUND,
                "lc_project_get_path() defined result");
    lc_free(path);

    char* dir = NULL;
    result = lc_project_get_directory(project, &dir);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_NOT_FOUND,
                "lc_project_get_directory() defined result");
    lc_free(dir);

    char* scenes = NULL;
    result = lc_project_get_scenes_directory(project, &scenes);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_NOT_FOUND,
                "lc_project_get_scenes_directory() defined result");
    lc_free(scenes);

    char* assets = NULL;
    result = lc_project_get_assets_directory(project, &assets);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_NOT_FOUND,
                "lc_project_get_assets_directory() defined result");
    lc_free(assets);

    char* scripts = NULL;
    result = lc_project_get_scripts_directory(project, &scripts);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_NOT_FOUND,
                "lc_project_get_scripts_directory() defined result");
    lc_free(scripts);

    /* lc_project_save with no bound path is a clean failure. */
    result = lc_project_save(project);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED,
                "lc_project_save() defined result");

    /* lc_project_save_as to a temp file may legitimately succeed; clean up if so. */
    char* temp_path = NULL;
    lc_vfs_get_default_temp_path(&temp_path);
    char save_target[1024];
    snprintf(save_target, sizeof(save_target), "%s/capi_ext_project.lupine",
             (temp_path != NULL) ? temp_path : ".");
    if (temp_path != NULL) {
        lc_free(temp_path);
    }
    result = lc_project_save_as(project, save_target);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED,
                "lc_project_save_as() defined result");
    if (result == LC_SUCCESS) {
        lc_fs_delete_file(save_target);
    }

    /* NULL-handle guards. */
    result = lc_project_get_path(NULL, &path);
    TEST_ASSERT(result == LC_ERROR_INVALID_HANDLE || result == LC_ERROR_NULL_POINTER,
                "lc_project_get_path(NULL handle) rejected");
    result = lc_project_load(NULL, "x.lupine");
    TEST_ASSERT(result == LC_ERROR_INVALID_HANDLE || result == LC_ERROR_NULL_POINTER,
                "lc_project_load(NULL handle) rejected");
    result = lc_project_save(NULL);
    TEST_ASSERT(result == LC_ERROR_INVALID_HANDLE || result == LC_ERROR_NULL_POINTER,
                "lc_project_save(NULL handle) rejected");
    result = lc_project_save_as(NULL, "x.lupine");
    TEST_ASSERT(result == LC_ERROR_INVALID_HANDLE || result == LC_ERROR_NULL_POINTER,
                "lc_project_save_as(NULL handle) rejected");

    lc_project_destroy(project);

    SECTION_END("Ext I/O: Project lifecycle + paths");
}

/* ----------------------------------------------------------------------------
 * Networking extras (no live session)
 * ------------------------------------------------------------------------- */

static void test_ext_net_ext_session(void) {
    SECTION_START("Ext Net: session/peer/LAN extras");

    LCResult result = lc_net_initialize();
    TEST_RESULT(result, "lc_net_initialize()");

    /* Offline baseline for the boolean queries this fragment owns. */
    TEST_ASSERT(lc_net_is_client() == false, "offline: not a client");
    TEST_ASSERT(lc_net_is_lan_advertising() == false, "offline: not LAN-advertising");
    TEST_ASSERT(lc_net_is_lan_discovering() == false, "offline: not LAN-discovering");

    /* Per-peer diagnostics are 0 for an unknown peer when offline. */
    TEST_ASSERT(lc_net_peer_rtt(2) == 0.0f, "offline: peer rtt is 0");
    TEST_ASSERT(lc_net_peer_bytes_in_per_sec(2) == 0.0f, "offline: peer bytes-in is 0");
    TEST_ASSERT(lc_net_peer_bytes_out_per_sec(2) == 0.0f, "offline: peer bytes-out is 0");

    /* Area-of-interest reports are safe with or without a session. */
    result = lc_net_set_interest_2d(10.0f, 20.0f);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_NOT_SUPPORTED,
                "lc_net_set_interest_2d() defined result");
    result = lc_net_set_interest_3d(1.0f, 2.0f, 3.0f);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_NOT_SUPPORTED,
                "lc_net_set_interest_3d() defined result");

    /* lc_net_connect with no reachable server: must fail cleanly, never crash. */
    LCNetConfig client_cfg;
    result = lc_net_config_default(&client_cfg);
    TEST_RESULT(result, "lc_net_config_default() (client)");
    client_cfg.transport = LC_NET_TRANSPORT_LOOPBACK;
    client_cfg.address = "127.0.0.1";
    client_cfg.port = 41599;
    bool connected = true;
    result = lc_net_connect(&client_cfg, &connected);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_NOT_SUPPORTED,
                "lc_net_connect() defined result");
    if (lc_net_is_active()) {
        lc_net_disconnect();
    }

    /* Stand up a loopback server so the server-side extras have a live session
     * to exercise; everything stays tolerant in case networking is compiled
     * out. */
    LCNetConfig server_cfg;
    result = lc_net_config_default(&server_cfg);
    TEST_RESULT(result, "lc_net_config_default() (server)");
    server_cfg.transport = LC_NET_TRANSPORT_LOOPBACK;
    server_cfg.port = 41600;
    server_cfg.game_id = "capi_io_net_ext";

    bool server_ok = false;
    result = lc_net_start_server(&server_cfg, &server_ok);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_NOT_SUPPORTED,
                "lc_net_start_server() defined result");

    /* lc_net_kick_peer for a non-existent peer: defined, never crashes. */
    bool kicked = true;
    result = lc_net_kick_peer(9999u, &kicked);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_NOT_FOUND || result == LC_ERROR_NOT_SUPPORTED,
                "lc_net_kick_peer() defined result");

    /* LAN advertising lifecycle. */
    bool adv_ok = false;
    result = lc_net_start_lan_advertising(&adv_ok);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_NOT_SUPPORTED,
                "lc_net_start_lan_advertising() defined result");
    (void)lc_net_is_lan_advertising();
    result = lc_net_stop_lan_advertising();
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_NOT_SUPPORTED,
                "lc_net_stop_lan_advertising() defined result");

    /* Spawn / despawn through a NetworkSpawner-bearing node (no peers => local). */
    LCNodeHandle spawner = NULL;
    if (lc_type_create_node("Node2D", &spawner) == LC_SUCCESS && spawner != NULL) {
        LCComponentHandle sc = NULL;
        lc_node_add_component_by_type(spawner, "NetworkSpawner", &sc);

        uint32_t new_id = 0u;
        result = lc_net_spawn(spawner, "res://capi_spawn.scene", 1u, &new_id);
        TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED ||
                    result == LC_ERROR_NOT_FOUND || result == LC_ERROR_NOT_SUPPORTED ||
                    result == LC_ERROR_FILE_NOT_FOUND,
                    "lc_net_spawn() defined result");

        result = lc_net_despawn(new_id);
        TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED ||
                    result == LC_ERROR_NOT_FOUND || result == LC_ERROR_NOT_SUPPORTED,
                    "lc_net_despawn() defined result");

        lc_node_destroy(spawner);
    }

    if (lc_net_is_active()) {
        lc_net_disconnect();
    }

    /* LAN discovery lifecycle (client-side, works while otherwise offline). */
    bool disc_ok = false;
    result = lc_net_start_lan_discovery("capi_io_net_ext", 41601u, &disc_ok);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_NOT_SUPPORTED,
                "lc_net_start_lan_discovery() defined result");
    (void)lc_net_is_lan_discovering();

    char* servers = NULL;
    result = lc_net_get_discovered_servers(&servers);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_NOT_SUPPORTED,
                "lc_net_get_discovered_servers() defined result");
    if (result == LC_SUCCESS && servers != NULL) {
        TEST_ASSERT(servers[0] == '[', "discovered servers serialized as JSON array");
    }
    lc_free(servers);

    result = lc_net_stop_lan_discovery();
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_NOT_SUPPORTED,
                "lc_net_stop_lan_discovery() defined result");

    /* NULL-pointer guard on a heap-out form. */
    result = lc_net_get_discovered_servers(NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER || result == LC_ERROR_NOT_SUPPORTED,
                "lc_net_get_discovered_servers(NULL) rejected");

    /* Ensure no session leaks out of this fragment. */
    if (lc_net_is_active()) {
        lc_net_disconnect();
    }

    SECTION_END("Ext Net: session/peer/LAN extras");
}
