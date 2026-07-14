/* ============================================================================
 * Extended File System Tests: FileSystem (physical paths) + VirtualFileSystem
 *
 * Included as a fragment into capi_test_suite.c. No includes, no main().
 * Exercises lc_fs_* and lc_vfs_* across read/write/binary/list/resolve/mount.
 * ============================================================================ */

static void test_ext_fs_physical(void) {
    SECTION_START("Ext FileSystem: physical paths");

    const char* path = "lupine_capi_fs_test.txt";
    const char* text = "Lupine C API file system test\nline two";

    LCResult result = lc_fs_write_text(path, text);
    TEST_RESULT(result, "lc_fs_write_text()");

    bool exists = false;
    result = lc_fs_exists(path, &exists);
    TEST_RESULT(result, "lc_fs_exists() callable");
    TEST_ASSERT(exists, "lc_fs_exists() reports written file present");

    bool is_file = false;
    result = lc_fs_is_file(path, &is_file);
    TEST_RESULT(result, "lc_fs_is_file() callable");
    TEST_ASSERT(is_file, "lc_fs_is_file() true for written file");

    bool is_dir = true;
    result = lc_fs_is_directory(path, &is_dir);
    TEST_RESULT(result, "lc_fs_is_directory() callable");
    TEST_ASSERT(!is_dir, "lc_fs_is_directory() false for a file");

    char* read_back = NULL;
    result = lc_fs_read_text(path, &read_back);
    TEST_RESULT(result, "lc_fs_read_text()");
    TEST_ASSERT(read_back != NULL && strcmp(read_back, text) == 0,
                "lc_fs_read_text() round-trips content");
    lc_free(read_back);

    uint64_t size = 0;
    result = lc_fs_get_file_size(path, &size);
    TEST_RESULT(result, "lc_fs_get_file_size()");
    TEST_ASSERT(size == (uint64_t)strlen(text),
                "lc_fs_get_file_size() matches written length");

    int64_t mtime = 0;
    result = lc_fs_get_modification_time(path, &mtime);
    TEST_RESULT(result, "lc_fs_get_modification_time()");

    result = lc_fs_append_text(path, "\nappended");
    TEST_RESULT(result, "lc_fs_append_text()");
    result = lc_fs_get_file_size(path, &size);
    TEST_ASSERT(result == LC_SUCCESS && size > (uint64_t)strlen(text),
                "append grew the file");

    char* abs_path = NULL;
    result = lc_fs_get_absolute_path(path, &abs_path);
    TEST_RESULT(result, "lc_fs_get_absolute_path()");
    lc_free(abs_path);

    char* cwd = NULL;
    result = lc_fs_get_current_directory(&cwd);
    TEST_RESULT(result, "lc_fs_get_current_directory()");
    lc_free(cwd);

    char* listing = NULL;
    result = lc_fs_list_directory(".", false, &listing);
    TEST_RESULT(result, "lc_fs_list_directory()");
    TEST_ASSERT(listing != NULL && listing[0] == '[',
                "lc_fs_list_directory() returns a JSON array");
    lc_free(listing);

    result = lc_fs_delete_file(path);
    TEST_RESULT(result, "lc_fs_delete_file()");
    exists = true;
    lc_fs_exists(path, &exists);
    TEST_ASSERT(!exists, "file removed after lc_fs_delete_file()");

    result = lc_fs_read_text(NULL, &read_back);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER,
                "lc_fs_read_text(NULL) rejected");

    SECTION_END("Ext FileSystem: physical paths");
}

static void test_ext_fs_binary(void) {
    SECTION_START("Ext FileSystem: binary I/O");

    const char* path = "lupine_capi_fs_test.bin";
    const uint8_t payload[] = {0x00, 0x10, 0x7F, 0xFF, 0x42, 0x00, 0x99};
    const size_t payload_size = sizeof(payload);

    LCResult result = lc_fs_write_binary(path, payload, payload_size);
    TEST_RESULT(result, "lc_fs_write_binary()");

    uint8_t* read_data = NULL;
    size_t read_size = 0;
    result = lc_fs_read_binary(path, &read_data, &read_size);
    TEST_RESULT(result, "lc_fs_read_binary()");
    TEST_ASSERT(read_size == payload_size, "binary read size matches");
    TEST_ASSERT(read_data != NULL && memcmp(read_data, payload, payload_size) == 0,
                "binary read content matches");
    lc_free(read_data);

    result = lc_fs_delete_file(path);
    TEST_RESULT(result, "lc_fs_delete_file() (binary)");

    SECTION_END("Ext FileSystem: binary I/O");
}

static void test_ext_vfs(void) {
    SECTION_START("Ext VirtualFileSystem");

    bool initialized = false;
    LCResult result = lc_vfs_is_initialized(&initialized);
    TEST_RESULT(result, "lc_vfs_is_initialized() callable");

    char* temp_path = NULL;
    result = lc_vfs_get_default_temp_path(&temp_path);
    TEST_RESULT(result, "lc_vfs_get_default_temp_path()");
    TEST_ASSERT(temp_path != NULL && temp_path[0] != '\0',
                "default temp path is non-empty");

    char* user_path = NULL;
    result = lc_vfs_get_default_user_data_path("LupineCapiTest", &user_path);
    TEST_RESULT(result, "lc_vfs_get_default_user_data_path()");
    lc_free(user_path);

    char mount_dir[1024];
    snprintf(mount_dir, sizeof(mount_dir), "%s/lupine_capi_vfs_test", temp_path);
    lc_free(temp_path);

    result = lc_fs_create_directory(mount_dir, true);
    TEST_RESULT(result, "create physical mount directory");

    result = lc_vfs_mount("captest://", mount_dir, false);
    TEST_RESULT(result, "lc_vfs_mount()");

    bool mounted = false;
    result = lc_vfs_is_mounted("captest://", &mounted);
    TEST_RESULT(result, "lc_vfs_is_mounted() callable");
    TEST_ASSERT(mounted, "lc_vfs_is_mounted() true after mount");

    char* mount_path = NULL;
    result = lc_vfs_get_mount_path("captest://", &mount_path);
    TEST_RESULT(result, "lc_vfs_get_mount_path()");
    lc_free(mount_path);

    char* mount_points = NULL;
    result = lc_vfs_get_mount_points(&mount_points);
    TEST_RESULT(result, "lc_vfs_get_mount_points()");
    TEST_ASSERT(mount_points != NULL && strstr(mount_points, "captest://") != NULL,
                "mount points list contains the new mount");
    lc_free(mount_points);

    const char* vpath = "captest://hello.txt";
    const char* vtext = "virtual file contents";
    result = lc_vfs_write_text(vpath, vtext);
    TEST_RESULT(result, "lc_vfs_write_text()");

    bool vexists = false;
    result = lc_vfs_exists(vpath, &vexists);
    TEST_RESULT(result, "lc_vfs_exists() callable");
    TEST_ASSERT(vexists, "lc_vfs_exists() true for written virtual file");

    bool v_is_file = false;
    result = lc_vfs_is_file(vpath, &v_is_file);
    TEST_RESULT(result, "lc_vfs_is_file() callable");
    TEST_ASSERT(v_is_file, "lc_vfs_is_file() true for virtual file");

    char* vread = NULL;
    result = lc_vfs_read_text(vpath, &vread);
    TEST_RESULT(result, "lc_vfs_read_text()");
    TEST_ASSERT(vread != NULL && strcmp(vread, vtext) == 0,
                "lc_vfs_read_text() round-trips content");
    lc_free(vread);

    char* resolved = NULL;
    result = lc_vfs_resolve_path(vpath, &resolved);
    TEST_RESULT(result, "lc_vfs_resolve_path()");
    TEST_ASSERT(resolved != NULL && resolved[0] != '\0',
                "lc_vfs_resolve_path() yields a physical path");
    lc_free(resolved);

    uint64_t vsize = 0;
    result = lc_vfs_get_file_size(vpath, &vsize);
    TEST_RESULT(result, "lc_vfs_get_file_size()");
    TEST_ASSERT(vsize == (uint64_t)strlen(vtext), "virtual file size matches");

    const uint8_t vbytes[] = {0xDE, 0xAD, 0xBE, 0xEF};
    result = lc_vfs_write_binary("captest://blob.bin", vbytes, sizeof(vbytes));
    TEST_RESULT(result, "lc_vfs_write_binary()");
    uint8_t* vblob = NULL;
    size_t vblob_size = 0;
    result = lc_vfs_read_binary("captest://blob.bin", &vblob, &vblob_size);
    TEST_RESULT(result, "lc_vfs_read_binary()");
    TEST_ASSERT(vblob_size == sizeof(vbytes) && vblob != NULL &&
                memcmp(vblob, vbytes, sizeof(vbytes)) == 0,
                "virtual binary round-trips");
    lc_free(vblob);

    char* vlisting = NULL;
    result = lc_vfs_list_directory("captest://", false, &vlisting);
    TEST_RESULT(result, "lc_vfs_list_directory()");
    TEST_ASSERT(vlisting != NULL && vlisting[0] == '[',
                "lc_vfs_list_directory() returns a JSON array");
    lc_free(vlisting);

    result = lc_vfs_copy_file(vpath, "captest://hello_copy.txt", true);
    TEST_RESULT(result, "lc_vfs_copy_file()");

    result = lc_vfs_delete_file(vpath);
    TEST_RESULT(result, "lc_vfs_delete_file()");
    result = lc_vfs_delete_file("captest://hello_copy.txt");
    TEST_RESULT(result, "lc_vfs_delete_file() (copy)");
    lc_vfs_delete_file("captest://blob.bin");

    result = lc_vfs_unmount("captest://");
    TEST_RESULT(result, "lc_vfs_unmount()");
    mounted = true;
    lc_vfs_is_mounted("captest://", &mounted);
    TEST_ASSERT(!mounted, "lc_vfs_is_mounted() false after unmount");

    lc_fs_delete_directory(mount_dir, true);

    result = lc_vfs_write_text(NULL, vtext);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER,
                "lc_vfs_write_text(NULL) rejected");

    SECTION_END("Ext VirtualFileSystem");
}
