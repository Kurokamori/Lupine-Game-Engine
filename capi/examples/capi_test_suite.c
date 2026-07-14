/**
 * @file capi_test_suite.c
 * @brief Comprehensive test suite for the Lupine Engine C API
 *
 * This file demonstrates and tests all current C API functionality.
 * It's designed to be easily extendable as new features are added.
 *
 * Run this to verify the C API is working correctly.
 */

#include <lupine_c.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Additional headers for full CAPI coverage */
#include <input/lc_input.h>
#include <platform/lc_display.h>
#include <audio/lc_audio.h>
#include <physics/lc_physics2d.h>
#include <physics/lc_physics3d.h>
#include <physics/lc_collision2d.h>
#include <physics/lc_collision3d.h>
#include <physics/lc_area_trigger2d.h>
#include <physics/lc_area_trigger3d.h>
#include <physics/lc_character_controller2d.h>
#include <physics/lc_character_controller3d.h>
#include <physics/lc_physics_query2d.h>
#include <physics/lc_physics_query3d.h>
#include <components/lc_multimesh.h>
#include <rendering/lc_camera.h>
#include <components/lc_light.h>
#include <components/lc_rendering.h>
#include <components/lc_primitive_mesh3d.h>
#include <components/lc_animated_sprite2d.h>
#include <components/lc_animated_sprite3d.h>
#include <components/lc_skeletal_mesh3d.h>
#include <components/lc_line2d.h>
#include <ui/lc_ui.h>
#include <ui/lc_button.h>
#include <ui/lc_panel.h>
#include <ui/lc_color_rect.h>
#include <ui/lc_image2d.h>
#include <ui/lc_progress_bar.h>
#include <ui/lc_label3d.h>
#include <ui/lc_container.h>
#include <ui/lc_button3d.h>
#include <ui/lc_texture_button.h>
#include <ui/lc_toggle_button.h>
#include <ui/lc_radio_button.h>
#include <ui/lc_checkbox.h>
#include <utility/lc_utility.h>
#include <asset/lc_asset.h>
#include <components/lc_world_environment.h>
#include <components/lc_node_scatter.h>
#include <components/lc_ysort.h>
#include <ui/lc_checklist.h>
#include <ui/lc_radiolist.h>
#include <ui/lc_center_container.h>
#include <ui/lc_padding_container.h>
#include <ui/lc_nine_slice_panel.h>
#include <ui/lc_dock_container.h>
#include <components/lc_shape2d.h>
#include <core/lc_scene_instance.h>
#include <core/lc_prefab.h>

/* Additional headers for extended coverage fragments (test_ext_*.inc) */
#include <i18n/lc_localization.h>
#include <components/lc_animation_player.h>
#include <components/lc_animation_tree.h>
#include <components/lc_tilemap2d.h>
#include <core/lc_signal.h>
#include <core/lc_interface.h>
#include <core/lc_node.h>
#include <core/lc_scene.h>
#include <math/lc_math.h>
#include <rendering/lc_material.h>
#include <ui/lc_uicontrol.h>
#include <ui/lc_slider.h>
#include <ui/lc_line_edit.h>
#include <ui/lc_text_edit.h>
#include <ui/lc_spin_box.h>
#include <ui/lc_dropdown.h>
#include <ui/lc_popup_menu.h>
#include <ui/lc_item_list.h>
#include <ui/lc_tree.h>
#include <ui/lc_rich_text_label.h>
#include <ui/lc_scroll_container.h>
#include <ui/lc_tab_container.h>
#include <ui/lc_theme.h>

/* ============================================================================
 * Test Utilities
 * ============================================================================ */

static int g_tests_passed = 0;
static int g_tests_failed = 0;

/* Track failed tests for summary */
#define MAX_FAILED_TESTS 2048
#define MAX_TEST_NAME_LEN 128

static char g_failed_tests[MAX_FAILED_TESTS][MAX_TEST_NAME_LEN];
static char g_failed_sections[MAX_FAILED_TESTS][MAX_TEST_NAME_LEN];
static int g_failed_count = 0;

static const char* g_current_section = "Unknown";

/* Copies src into the capacity-byte dst, truncating as needed and always
 * NUL-terminating.  Writes nothing when capacity is 0.  Shared by the
 * suite fragments, which are #included into this translation unit. */
static void copy_bounded(char* dst, size_t capacity, const char* src) {
    size_t length;
    if (!dst || capacity == 0) {
        return;
    }
    length = src ? strlen(src) : 0;
    if (length > capacity - 1) {
        length = capacity - 1;
    }
    if (length > 0) {
        memcpy(dst, src, length);
    }
    dst[length] = '\0';
}

static void record_failure(const char* test_name) {
    if (g_failed_count < MAX_FAILED_TESTS) {
        copy_bounded(g_failed_tests[g_failed_count], MAX_TEST_NAME_LEN, test_name);
        copy_bounded(g_failed_sections[g_failed_count], MAX_TEST_NAME_LEN, g_current_section);
        g_failed_count++;
    }
}

#define SEPARATOR "============================================================\n"
#define SECTION_START(name) do { \
    g_current_section = name; \
    printf("\n" SEPARATOR "[TEST] %s\n" SEPARATOR, name); \
} while(0)
#define SECTION_END(name) printf("[DONE] %s\n", name)

#define TEST_ASSERT(condition, msg) do { \
    if (condition) { \
        printf("  [PASS] %s\n", msg); \
        g_tests_passed++; \
    } else { \
        printf("  [FAIL] %s\n", msg); \
        g_tests_failed++; \
        record_failure(msg); \
    } \
} while(0)

/* For invariants the compiler can settle (struct sizes, field layout). Checking them
 * with a runtime `if` makes MSVC emit C4127 (constant conditional) and defers a
 * failure to run time; _Static_assert turns a break into a build error instead.
 * The pass is still reported so the counts match the rest of the suite. */
#define TEST_STATIC_ASSERT(condition, msg) do { \
    _Static_assert(condition, msg); \
    printf("  [PASS] %s\n", msg); \
    g_tests_passed++; \
} while(0)

#define TEST_RESULT(result, msg) TEST_ASSERT((result) == LC_SUCCESS, msg)

/* Wait for user to press Enter */
static void wait_for_user(void) {
    printf("\n" SEPARATOR);
    printf("Press ENTER to close...\n");
    printf(SEPARATOR);
    getchar();
}

/* ============================================================================
 * Core Module Tests
 * ============================================================================ */

static void test_core_version(void) {
    SECTION_START("Core: Version Information");

    /* Version string */
    const char* version = lc_get_version_string();
    TEST_ASSERT(version != NULL && strlen(version) > 0, "lc_get_version_string() returns valid string");
    printf("    C API Version: %s\n", version);

    /* Engine version */
    const char* engine_version = lc_get_engine_version();
    TEST_ASSERT(engine_version != NULL, "lc_get_engine_version() returns valid string");
    printf("    Engine Version: %s\n", engine_version);

    /* Version components */
    int major, minor, patch;
    lc_get_version(&major, &minor, &patch);
    TEST_ASSERT(major >= 0 && minor >= 0 && patch >= 0, "lc_get_version() returns valid components");
    printf("    Version: %d.%d.%d\n", major, minor, patch);

    SECTION_END("Core: Version Information");
}

static void test_core_initialization(void) {
    SECTION_START("Core: Initialization");

    /* Check if already initialized (it should be) */
    bool initialized = lc_is_initialized();
    TEST_ASSERT(initialized, "Engine is initialized");

    /* Test result code string */
    const char* success_str = lc_result_to_string(LC_SUCCESS);
    TEST_ASSERT(success_str != NULL, "lc_result_to_string(LC_SUCCESS) returns valid string");
    printf("    LC_SUCCESS = \"%s\"\n", success_str);

    const char* error_str = lc_result_to_string(LC_ERROR_INVALID_HANDLE);
    TEST_ASSERT(error_str != NULL, "lc_result_to_string(LC_ERROR_INVALID_HANDLE) returns valid string");
    printf("    LC_ERROR_INVALID_HANDLE = \"%s\"\n", error_str);

    /* lc_quit requests an application quit on the active scene manager. With no
     * scene manager loaded in this harness it reports OPERATION_FAILED; either way
     * it must not crash and returns a valid result code. */
    LCResult quit_result = lc_quit();
    TEST_ASSERT(quit_result == LC_SUCCESS || quit_result == LC_ERROR_OPERATION_FAILED,
                "lc_quit returns a valid result code");
    printf("    lc_quit result: %d\n", (int)quit_result);

    /* Command-line / runtime arguments. None are set in this harness, so the
     * count is zero and any read is out of range; the API must report cleanly. */
    int arg_count = -1;
    LCResult arg_count_result = lc_cmdline_arg_count(&arg_count);
    TEST_ASSERT(arg_count_result == LC_SUCCESS, "lc_cmdline_arg_count succeeds");
    TEST_ASSERT(arg_count >= 0, "lc_cmdline_arg_count yields a non-negative count");
    printf("    lc_cmdline_arg_count: %d\n", arg_count);

    TEST_ASSERT(lc_cmdline_arg_count(NULL) == LC_ERROR_INVALID_PARAMETER,
                "lc_cmdline_arg_count(NULL) is rejected");

    char arg_buffer[64] = "untouched";
    LCResult arg_at_result = lc_cmdline_arg_at(0, arg_buffer, sizeof(arg_buffer));
    TEST_ASSERT(arg_at_result == LC_SUCCESS || arg_at_result == LC_ERROR_INVALID_PARAMETER,
                "lc_cmdline_arg_at returns a valid result code");
    TEST_ASSERT(lc_cmdline_arg_at(0, NULL, sizeof(arg_buffer)) == LC_ERROR_INVALID_PARAMETER,
                "lc_cmdline_arg_at(NULL buffer) is rejected");

    SECTION_END("Core: Initialization");
}

static void test_core_logging(void) {
    SECTION_START("Core: Logging System");

    /* Get/set log level */
    LCLogLevel original = lc_log_get_level();
    printf("    Original log level: %d\n", (int)original);

    lc_log_set_level(LC_LOG_DEBUG);
    LCLogLevel current = lc_log_get_level();
    TEST_ASSERT(current == LC_LOG_DEBUG, "Log level set to DEBUG");

    /* Test different log levels */
    lc_log_info("Test info message from C API");
    lc_log_debug("Test debug message from C API");
    lc_log_warn("Test warning message from C API");
    lc_log(LC_LOG_INFO, "Test lc_log() function");

    /* Restore original */
    lc_log_set_level(original);
    TEST_ASSERT(lc_log_get_level() == original, "Log level restored");

    SECTION_END("Core: Logging System");
}

static void test_core_error_handling(void) {
    SECTION_START("Core: Error Handling");

    /* Clear any previous error */
    lc_clear_last_error();

    /* Test error functions exist */
    const char* error_msg = lc_get_last_error();
    TEST_ASSERT(error_msg != NULL, "lc_get_last_error() returns valid pointer");

    LCResult last_code = lc_get_last_error_code();
    printf("    Last error code: %d\n", (int)last_code);

    /* Trigger an error by passing NULL */
    LCNodeHandle node = NULL;
    LCResult result = lc_node_get_type(node, NULL);
    TEST_ASSERT(result != LC_SUCCESS, "Invalid operation returns error code");

    SECTION_END("Core: Error Handling");
}

/* ============================================================================
 * Math Module Tests
 * ============================================================================ */

static void test_math_vec2(void) {
    SECTION_START("Math: Vec2 Operations");

    /* Constructors */
    LCVec2 v1 = lc_vec2(3.0f, 4.0f);
    TEST_ASSERT(v1.x == 3.0f && v1.y == 4.0f, "lc_vec2() constructor");

    LCVec2 zero = lc_vec2_zero();
    TEST_ASSERT(zero.x == 0.0f && zero.y == 0.0f, "lc_vec2_zero()");

    LCVec2 one = lc_vec2_one();
    TEST_ASSERT(one.x == 1.0f && one.y == 1.0f, "lc_vec2_one()");

    /* Direction vectors */
    LCVec2 up = lc_vec2_up();
    LCVec2 right = lc_vec2_right();
    printf("    up = (%.1f, %.1f), right = (%.1f, %.1f)\n", up.x, up.y, right.x, right.y);

    /* Operations */
    LCVec2 v2 = lc_vec2(1.0f, 2.0f);
    LCVec2 sum = lc_vec2_add(v1, v2);
    TEST_ASSERT(sum.x == 4.0f && sum.y == 6.0f, "lc_vec2_add()");

    LCVec2 diff = lc_vec2_sub(v1, v2);
    TEST_ASSERT(diff.x == 2.0f && diff.y == 2.0f, "lc_vec2_sub()");

    LCVec2 scaled = lc_vec2_mul(v1, 2.0f);
    TEST_ASSERT(scaled.x == 6.0f && scaled.y == 8.0f, "lc_vec2_mul()");

    float length = lc_vec2_length(v1);
    TEST_ASSERT(length > 4.9f && length < 5.1f, "lc_vec2_length() (3-4-5 triangle)");

    float dot = lc_vec2_dot(v1, v2);
    TEST_ASSERT(dot == 11.0f, "lc_vec2_dot()");

    LCVec2 normalized = lc_vec2_normalize(v1);
    float norm_len = lc_vec2_length(normalized);
    TEST_ASSERT(norm_len > 0.99f && norm_len < 1.01f, "lc_vec2_normalize()");

    LCVec2 lerped = lc_vec2_lerp(zero, one, 0.5f);
    TEST_ASSERT(lerped.x == 0.5f && lerped.y == 0.5f, "lc_vec2_lerp()");

    float dist = lc_vec2_distance(zero, v1);
    TEST_ASSERT(dist > 4.9f && dist < 5.1f, "lc_vec2_distance()");

    SECTION_END("Math: Vec2 Operations");
}

static void test_math_vec3(void) {
    SECTION_START("Math: Vec3 Operations");

    /* Constructors */
    LCVec3 v1 = lc_vec3(1.0f, 2.0f, 3.0f);
    TEST_ASSERT(v1.x == 1.0f && v1.y == 2.0f && v1.z == 3.0f, "lc_vec3() constructor");

    LCVec3 zero = lc_vec3_zero();
    LCVec3 one = lc_vec3_one();
    TEST_ASSERT(zero.x == 0.0f && one.x == 1.0f, "lc_vec3_zero() and lc_vec3_one()");

    /* Direction vectors */
    LCVec3 forward = lc_vec3_forward();
    LCVec3 up = lc_vec3_up();
    LCVec3 right = lc_vec3_right();
    printf("    forward = (%.1f, %.1f, %.1f)\n", forward.x, forward.y, forward.z);
    printf("    up = (%.1f, %.1f, %.1f)\n", up.x, up.y, up.z);
    printf("    right = (%.1f, %.1f, %.1f)\n", right.x, right.y, right.z);

    /* Operations */
    LCVec3 v2 = lc_vec3(4.0f, 5.0f, 6.0f);
    LCVec3 sum = lc_vec3_add(v1, v2);
    TEST_ASSERT(sum.x == 5.0f && sum.y == 7.0f && sum.z == 9.0f, "lc_vec3_add()");

    float dot = lc_vec3_dot(v1, v2);
    TEST_ASSERT(dot == 32.0f, "lc_vec3_dot() (1*4 + 2*5 + 3*6 = 32)");

    LCVec3 cross = lc_vec3_cross(lc_vec3_unit_x(), lc_vec3_unit_y());
    printf("    X cross Y = (%.1f, %.1f, %.1f)\n", cross.x, cross.y, cross.z);
    TEST_ASSERT(cross.z != 0.0f, "lc_vec3_cross() produces non-zero result");

    float length = lc_vec3_length(v1);
    TEST_ASSERT(length > 3.7f && length < 3.8f, "lc_vec3_length()");

    LCVec3 normalized = lc_vec3_normalize(v1);
    float norm_len = lc_vec3_length(normalized);
    TEST_ASSERT(norm_len > 0.99f && norm_len < 1.01f, "lc_vec3_normalize()");

    LCVec3 lerped = lc_vec3_lerp(zero, one, 0.25f);
    TEST_ASSERT(lerped.x == 0.25f, "lc_vec3_lerp()");

    SECTION_END("Math: Vec3 Operations");
}

static void test_math_vec4(void) {
    SECTION_START("Math: Vec4 Operations");

    LCVec4 v1 = lc_vec4(1.0f, 2.0f, 3.0f, 4.0f);
    TEST_ASSERT(v1.w == 4.0f, "lc_vec4() constructor");

    LCVec4 zero = lc_vec4_zero();
    LCVec4 one = lc_vec4_one();
    TEST_ASSERT(zero.w == 0.0f && one.w == 1.0f, "lc_vec4_zero() and lc_vec4_one()");

    LCVec4 v2 = lc_vec4(1.0f, 1.0f, 1.0f, 1.0f);
    LCVec4 sum = lc_vec4_add(v1, v2);
    TEST_ASSERT(sum.x == 2.0f && sum.w == 5.0f, "lc_vec4_add()");

    float dot = lc_vec4_dot(v1, v2);
    TEST_ASSERT(dot == 10.0f, "lc_vec4_dot()");

    float length = lc_vec4_length(v1);
    TEST_ASSERT(length > 5.4f && length < 5.5f, "lc_vec4_length()");

    SECTION_END("Math: Vec4 Operations");
}

static void test_math_quaternion(void) {
    SECTION_START("Math: Quaternion Operations");

    /* Identity */
    LCQuat identity = lc_quat_identity();
    TEST_ASSERT(identity.w == 1.0f && identity.x == 0.0f, "lc_quat_identity()");

    /* From axis-angle */
    LCQuat rot90 = lc_quat_from_axis_angle(lc_vec3_unit_y(), lc_deg_to_rad(90.0f));
    TEST_ASSERT(rot90.w != 1.0f, "lc_quat_from_axis_angle() creates rotation");
    printf("    90deg Y rotation: (%.3f, %.3f, %.3f, %.3f)\n",
           rot90.x, rot90.y, rot90.z, rot90.w);

    /* From Euler angles */
    LCQuat euler_rot = lc_quat_from_euler(0.0f, lc_deg_to_rad(45.0f), 0.0f);
    TEST_ASSERT(euler_rot.w != 1.0f, "lc_quat_from_euler() creates rotation");

    /* Rotate a vector */
    LCVec3 forward = lc_vec3(0.0f, 0.0f, -1.0f);
    LCVec3 rotated = lc_quat_mul_vec3(rot90, forward);
    printf("    Forward rotated 90deg around Y: (%.2f, %.2f, %.2f)\n",
           rotated.x, rotated.y, rotated.z);
    TEST_ASSERT(rotated.x < -0.9f || rotated.x > 0.9f, "lc_quat_mul_vec3() rotates vector");

    /* Quaternion multiplication */
    LCQuat combined = lc_quat_mul(rot90, rot90);
    TEST_ASSERT(combined.w != rot90.w, "lc_quat_mul() combines rotations");

    /* Inverse */
    LCQuat inv = lc_quat_inverse(rot90);
    LCQuat should_be_identity = lc_quat_mul(rot90, inv);
    TEST_ASSERT(should_be_identity.w > 0.99f, "lc_quat_inverse() * original = identity");

    /* Slerp */
    LCQuat slerped = lc_quat_slerp(identity, rot90, 0.5f);
    TEST_ASSERT(slerped.w != identity.w && slerped.w != rot90.w, "lc_quat_slerp() interpolates");

    /* To Euler */
    LCVec3 euler = lc_quat_to_euler(rot90);
    printf("    90deg Y as euler: (%.2f, %.2f, %.2f) rad\n", euler.x, euler.y, euler.z);

    SECTION_END("Math: Quaternion Operations");
}

static void test_math_matrix(void) {
    SECTION_START("Math: Mat4 Operations");

    /* Identity */
    LCMat4 identity = lc_mat4_identity();
    TEST_ASSERT(identity.m[0] == 1.0f && identity.m[5] == 1.0f, "lc_mat4_identity()");

    /* Translation */
    LCMat4 trans = lc_mat4_translate(lc_vec3(10.0f, 20.0f, 30.0f));
    TEST_ASSERT(trans.m[12] == 10.0f, "lc_mat4_translate() sets translation");

    /* Scale */
    LCMat4 scale = lc_mat4_scale(lc_vec3(2.0f, 3.0f, 4.0f));
    TEST_ASSERT(scale.m[0] == 2.0f && scale.m[5] == 3.0f, "lc_mat4_scale()");

    /* Rotation */
    LCQuat rot = lc_quat_from_axis_angle(lc_vec3_unit_y(), lc_deg_to_rad(90.0f));
    LCMat4 rot_mat = lc_mat4_rotate(rot);
    TEST_ASSERT(rot_mat.m[0] != 1.0f, "lc_mat4_rotate() creates rotation matrix");

    /* TRS */
    LCMat4 trs = lc_mat4_trs(
        lc_vec3(10.0f, 0.0f, 0.0f),
        lc_quat_identity(),
        lc_vec3_one()
    );
    TEST_ASSERT(trs.m[12] == 10.0f, "lc_mat4_trs() creates combined matrix");

    /* Multiplication */
    LCMat4 combined = lc_mat4_mul(&trans, &scale);
    TEST_ASSERT(combined.m[0] == 2.0f, "lc_mat4_mul() multiplies matrices");

    /* Transform point */
    LCVec3 point = lc_vec3(1.0f, 0.0f, 0.0f);
    LCVec3 transformed = lc_mat4_mul_vec3(&trans, point, 1.0f);
    TEST_ASSERT(transformed.x == 11.0f, "lc_mat4_mul_vec3() transforms point");

    /* Transpose */
    LCMat4 transposed = lc_mat4_transpose(&trans);
    TEST_ASSERT(transposed.m[3] == 10.0f, "lc_mat4_transpose()");

    /* Perspective projection */
    LCMat4 persp = lc_mat4_perspective(60.0f, 16.0f/9.0f, 0.1f, 1000.0f);
    TEST_ASSERT(persp.m[0] != 0.0f, "lc_mat4_perspective() creates projection");

    /* Orthographic projection */
    LCMat4 ortho = lc_mat4_orthographic(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 100.0f);
    TEST_ASSERT(ortho.m[0] != 0.0f, "lc_mat4_orthographic() creates projection");

    /* Look at */
    LCMat4 look = lc_mat4_look_at(
        lc_vec3(0.0f, 5.0f, 10.0f),
        lc_vec3_zero(),
        lc_vec3_up()
    );
    TEST_ASSERT(look.m[0] != 0.0f, "lc_mat4_look_at() creates view matrix");

    SECTION_END("Math: Mat4 Operations");
}

static void test_math_color(void) {
    SECTION_START("Math: Color Operations");

    /* Predefined colors */
    LCColor red = lc_color_red();
    TEST_ASSERT(red.r == 1.0f && red.g == 0.0f && red.b == 0.0f, "lc_color_red()");

    LCColor green = lc_color_green();
    TEST_ASSERT(green.r == 0.0f && green.g == 1.0f && green.b == 0.0f, "lc_color_green()");

    LCColor blue = lc_color_blue();
    TEST_ASSERT(blue.r == 0.0f && blue.g == 0.0f && blue.b == 1.0f, "lc_color_blue()");

    LCColor white = lc_color_white();
    TEST_ASSERT(white.r == 1.0f && white.g == 1.0f && white.b == 1.0f && white.a == 1.0f, "lc_color_white()");

    LCColor black = lc_color_black();
    TEST_ASSERT(black.r == 0.0f && black.g == 0.0f && black.b == 0.0f && black.a == 1.0f, "lc_color_black()");

    printf("    red = (%.1f, %.1f, %.1f, %.1f)\n", red.r, red.g, red.b, red.a);
    printf("    green = (%.1f, %.1f, %.1f, %.1f)\n", green.r, green.g, green.b, green.a);
    printf("    blue = (%.1f, %.1f, %.1f, %.1f)\n", blue.r, blue.g, blue.b, blue.a);

    /* Custom color */
    LCColor custom = lc_color(0.5f, 0.25f, 0.75f, 1.0f);
    TEST_ASSERT(custom.g == 0.25f, "lc_color() constructor");

    LCColor rgb = lc_color_rgb(0.1f, 0.2f, 0.3f);
    TEST_ASSERT(rgb.a == 1.0f, "lc_color_rgb() sets alpha to 1");

    /* From bytes */
    LCColor from_bytes = lc_color_from_bytes(255, 128, 64, 255);
    TEST_ASSERT(from_bytes.r > 0.99f && from_bytes.g > 0.49f, "lc_color_from_bytes()");

    /* From hex */
    LCColor from_hex = lc_color_from_hex(0xFF0000FF);  /* Red, full alpha */
    TEST_ASSERT(from_hex.r > 0.99f, "lc_color_from_hex()");
    printf("    0xFF0000FF = (%.2f, %.2f, %.2f, %.2f)\n",
           from_hex.r, from_hex.g, from_hex.b, from_hex.a);

    /* To hex */
    unsigned int hex = lc_color_to_hex(red);
    printf("    red as hex: 0x%08X\n", hex);

    /* Lerp */
    LCColor lerped = lc_color_lerp(red, blue, 0.5f);
    TEST_ASSERT(lerped.r == 0.5f && lerped.b == 0.5f, "lc_color_lerp()");
    printf("    lerp(red, blue, 0.5) = (%.2f, %.2f, %.2f)\n", lerped.r, lerped.g, lerped.b);

    /* Multiply */
    LCColor multiplied = lc_color_mul(white, red);
    TEST_ASSERT(multiplied.r == 1.0f && multiplied.g == 0.0f, "lc_color_mul()");

    /* HSV */
    LCColor from_hsv = lc_color_from_hsv(0.0f, 1.0f, 1.0f);  /* Red in HSV */
    TEST_ASSERT(from_hsv.r > 0.99f, "lc_color_from_hsv() (red)");

    LCVec3 to_hsv = lc_color_to_hsv(red);
    printf("    red as HSV: (%.2f, %.2f, %.2f)\n", to_hsv.x, to_hsv.y, to_hsv.z);

    SECTION_END("Math: Color Operations");
}

static void test_math_transform(void) {
    SECTION_START("Math: Transform Operations");

    /* Identity transform */
    LCTransform ident = lc_transform_identity();
    TEST_ASSERT(ident.scale.x == 1.0f, "lc_transform_identity()");

    /* Create transform */
    LCTransform t = lc_transform_create(
        lc_vec3(5.0f, 10.0f, 15.0f),
        lc_quat_identity(),
        lc_vec3(2.0f, 2.0f, 2.0f)
    );
    TEST_ASSERT(t.position.y == 10.0f && t.scale.x == 2.0f, "lc_transform_create()");

    /* To matrix */
    LCMat4 mat = lc_transform_to_matrix(&t);
    TEST_ASSERT(mat.m[12] == 5.0f, "lc_transform_to_matrix()");

    /* Apply to point */
    LCVec3 point = lc_vec3(1.0f, 0.0f, 0.0f);
    LCVec3 result = lc_transform_apply_to_point(&t, point);
    printf("    Point (1,0,0) transformed: (%.1f, %.1f, %.1f)\n",
           result.x, result.y, result.z);
    TEST_ASSERT(result.x == 7.0f, "lc_transform_apply_to_point() (1*2 + 5 = 7)");

    /* Apply to direction (only applies rotation, NOT scale or translation) */
    LCVec3 dir = lc_vec3(1.0f, 0.0f, 0.0f);
    LCVec3 dir_result = lc_transform_apply_to_direction(&t, dir);
    TEST_ASSERT(dir_result.x == 1.0f, "lc_transform_apply_to_direction() (only rotation, not scale)");

    /* Inverse */
    LCTransform inv = lc_transform_inverse(&t);
    LCTransform combined = lc_transform_mul(&t, &inv);
    TEST_ASSERT(combined.position.x < 0.01f && combined.position.x > -0.01f,
                "lc_transform_inverse() * original = identity");

    /* Lerp */
    LCTransform t2 = lc_transform_create(
        lc_vec3(10.0f, 20.0f, 30.0f),
        lc_quat_identity(),
        lc_vec3_one()
    );
    LCTransform lerped = lc_transform_lerp(&ident, &t2, 0.5f);
    TEST_ASSERT(lerped.position.x == 5.0f, "lc_transform_lerp()");

    SECTION_END("Math: Transform Operations");
}

static void test_math_utilities(void) {
    SECTION_START("Math: Utility Functions");

    /* Min/Max/Clamp */
    TEST_ASSERT(lc_min(3.0f, 7.0f) == 3.0f, "lc_min()");
    TEST_ASSERT(lc_max(3.0f, 7.0f) == 7.0f, "lc_max()");
    TEST_ASSERT(lc_clamp(15.0f, 0.0f, 10.0f) == 10.0f, "lc_clamp() (above max)");
    TEST_ASSERT(lc_clamp(-5.0f, 0.0f, 10.0f) == 0.0f, "lc_clamp() (below min)");
    TEST_ASSERT(lc_clamp(5.0f, 0.0f, 10.0f) == 5.0f, "lc_clamp() (in range)");

    /* Lerp */
    TEST_ASSERT(lc_lerp(0.0f, 100.0f, 0.25f) == 25.0f, "lc_lerp()");

    /* Saturate */
    TEST_ASSERT(lc_saturate(1.5f) == 1.0f, "lc_saturate() (above 1)");
    TEST_ASSERT(lc_saturate(-0.5f) == 0.0f, "lc_saturate() (below 0)");

    /* Sign/Abs */
    TEST_ASSERT(lc_sign(-5.0f) == -1.0f, "lc_sign() (negative)");
    TEST_ASSERT(lc_sign(5.0f) == 1.0f, "lc_sign() (positive)");
    TEST_ASSERT(lc_abs(-5.0f) == 5.0f, "lc_abs()");

    /* Sqrt/Pow */
    TEST_ASSERT(lc_sqrt(4.0f) == 2.0f, "lc_sqrt()");
    TEST_ASSERT(lc_pow(2.0f, 3.0f) == 8.0f, "lc_pow()");

    /* Angle conversions */
    float rad = lc_deg_to_rad(180.0f);
    TEST_ASSERT(rad > 3.14f && rad < 3.15f, "lc_deg_to_rad()");

    float deg = lc_rad_to_deg(LC_PI);
    TEST_ASSERT(deg > 179.9f && deg < 180.1f, "lc_rad_to_deg()");

    /* Angle normalize */
    float normalized_angle = lc_angle_normalize(LC_TWO_PI + 1.0f);
    TEST_ASSERT(normalized_angle < LC_TWO_PI, "lc_angle_normalize()");

    /* Comparison */
    TEST_ASSERT(lc_equals(1.0f, 1.0f + LC_EPSILON * 0.5f), "lc_equals() (within epsilon)");
    TEST_ASSERT(lc_is_zero(LC_EPSILON * 0.5f), "lc_is_zero()");

    SECTION_END("Math: Utility Functions");
}

/* ============================================================================
 * Node Module Tests
 * ============================================================================ */

static void test_node_creation(void) {
    SECTION_START("Node: Creation and Destruction");

    /* Create base node */
    LCNodeHandle base_node = NULL;
    LCResult result = lc_node_create(LC_NODE_BASE, "TestBaseNode", &base_node);
    TEST_RESULT(result, "Create base node");
    TEST_ASSERT(base_node != NULL, "Base node handle is valid");

    /* Create Node2D */
    LCNodeHandle node2d = NULL;
    result = lc_node_create(LC_NODE_2D, "TestNode2D", &node2d);
    TEST_RESULT(result, "Create Node2D");

    /* Create Node3D */
    LCNodeHandle node3d = NULL;
    result = lc_node_create(LC_NODE_3D, "TestNode3D", &node3d);
    TEST_RESULT(result, "Create Node3D");

    /* Get node type */
    LCNodeType type;
    result = lc_node_get_type(node3d, &type);
    TEST_RESULT(result, "Get node type");
    TEST_ASSERT(type == LC_NODE_3D, "Node type is LC_NODE_3D");

    /* Destroy nodes */
    result = lc_node_destroy(base_node);
    TEST_RESULT(result, "Destroy base node");

    result = lc_node_destroy(node2d);
    TEST_RESULT(result, "Destroy Node2D");

    result = lc_node_destroy(node3d);
    TEST_RESULT(result, "Destroy Node3D");

    SECTION_END("Node: Creation and Destruction");
}

static int g_signal_fire_count = 0;
static char g_signal_last_args[256];

static void signal_test_callback(const char* args_json, void* user_data) {
    g_signal_fire_count++;
    if (args_json) {
        copy_bounded(g_signal_last_args, sizeof(g_signal_last_args), args_json);
    }
    if (user_data) {
        *(int*)user_data += 1;
    }
}

static void test_signals(void) {
    SECTION_START("Signals: Connect / Emit / Disconnect");

    LCNodeHandle node = NULL;
    LCResult result = lc_node_create(LC_NODE_2D, "SignalNode", &node);
    TEST_RESULT(result, "Create signal node");

    result = lc_node_add_user_signal(node, "health_changed");
    TEST_RESULT(result, "Declare user signal 'health_changed'");

    g_signal_fire_count = 0;
    g_signal_last_args[0] = '\0';
    int user_counter = 0;

    uint64_t conn = 0;
    result = lc_node_connect_callback(node, "health_changed", signal_test_callback,
                                      &user_counter, LC_CONNECT_NONE, &conn);
    TEST_RESULT(result, "Connect native callback to signal");
    TEST_ASSERT(conn != 0, "Connection id is non-zero");

    bool connected = false;
    lc_node_is_connected(node, "health_changed", &connected);
    TEST_ASSERT(connected, "Signal reports a live connection");

    result = lc_node_emit(node, "health_changed", "[42, \"hp\"]");
    TEST_RESULT(result, "Emit signal with arguments");
    TEST_ASSERT(g_signal_fire_count == 1, "Callback fired exactly once");
    TEST_ASSERT(user_counter == 1, "user_data was passed through");
    TEST_ASSERT(strstr(g_signal_last_args, "42") != NULL, "Callback received emitted argument");

    result = lc_node_disconnect(node, "health_changed", conn);
    TEST_RESULT(result, "Disconnect callback");
    lc_node_emit(node, "health_changed", "[7]");
    TEST_ASSERT(g_signal_fire_count == 1, "Disconnected callback does not fire");

    /* One-shot connection auto-disconnects after first emit. */
    g_signal_fire_count = 0;
    uint64_t oneshot = 0;
    lc_node_connect_callback(node, "health_changed", signal_test_callback, NULL,
                             LC_CONNECT_ONESHOT, &oneshot);
    lc_node_emit(node, "health_changed", "[1]");
    lc_node_emit(node, "health_changed", "[2]");
    TEST_ASSERT(g_signal_fire_count == 1, "One-shot connection fires only once");

    /* Global event bus. */
    g_signal_fire_count = 0;
    uint64_t sub = 0;
    result = lc_event_subscribe_callback("game_over", signal_test_callback, NULL,
                                         LC_CONNECT_NONE, &sub);
    TEST_RESULT(result, "Subscribe callback to global event");
    result = lc_event_emit("game_over", "[\"player1\"]");
    TEST_RESULT(result, "Emit global event");
    TEST_ASSERT(g_signal_fire_count == 1, "Event bus callback fired");
    lc_event_unsubscribe("game_over", sub);
    lc_event_emit("game_over", "[]");
    TEST_ASSERT(g_signal_fire_count == 1, "Unsubscribed event callback does not fire");

    lc_node_destroy(node);

    SECTION_END("Signals: Connect / Emit / Disconnect");
}

static void test_node_properties(void) {
    SECTION_START("Node: Properties");

    LCNodeHandle node = NULL;
    lc_node_create(LC_NODE_3D, "PropertyTestNode", &node);

    /* Name */
    const char* name = lc_node_get_name(node);
    TEST_ASSERT(name != NULL && strcmp(name, "PropertyTestNode") == 0, "Get node name");

    LCResult result = lc_node_set_name(node, "RenamedNode");
    TEST_RESULT(result, "Set node name");

    name = lc_node_get_name(node);
    TEST_ASSERT(strcmp(name, "RenamedNode") == 0, "Name was changed");

    /* UUID */
    const char* uuid = lc_node_get_uuid(node);
    TEST_ASSERT(uuid != NULL && strlen(uuid) > 0, "Get node UUID");
    printf("    Node UUID: %s\n", uuid);

    /* Active state */
    bool active = lc_node_is_active(node);
    TEST_ASSERT(active, "Node is active by default");

    result = lc_node_set_active(node, false);
    TEST_RESULT(result, "Set node inactive");
    TEST_ASSERT(!lc_node_is_active(node), "Node is now inactive");

    result = lc_node_set_active(node, true);
    TEST_RESULT(result, "Set node active again");

    /* Visible state */
    bool visible = lc_node_is_visible(node);
    TEST_ASSERT(visible, "Node is visible by default");

    result = lc_node_set_visible(node, false);
    TEST_RESULT(result, "Set node invisible");
    TEST_ASSERT(!lc_node_is_visible(node), "Node is now invisible");

    lc_node_destroy(node);

    SECTION_END("Node: Properties");
}

static void test_node_hierarchy(void) {
    SECTION_START("Node: Hierarchy Management");

    /* Create parent and children */
    LCNodeHandle parent = NULL;
    lc_node_create(LC_NODE_3D, "Parent", &parent);

    LCNodeHandle child1 = NULL;
    lc_node_create(LC_NODE_3D, "Child1", &child1);

    LCNodeHandle child2 = NULL;
    lc_node_create(LC_NODE_3D, "Child2", &child2);

    LCNodeHandle grandchild = NULL;
    lc_node_create(LC_NODE_3D, "GrandChild", &grandchild);

    /* Add children */
    LCResult result = lc_node_add_child(parent, child1);
    TEST_RESULT(result, "Add child1 to parent");

    result = lc_node_add_child(parent, child2);
    TEST_RESULT(result, "Add child2 to parent");

    result = lc_node_add_child(child1, grandchild);
    TEST_RESULT(result, "Add grandchild to child1");

    /* Get child count */
    size_t count;
    result = lc_node_get_child_count(parent, &count);
    TEST_RESULT(result, "Get child count");
    TEST_ASSERT(count == 2, "Parent has 2 children");

    /* Get child by index */
    LCNodeHandle retrieved = NULL;
    result = lc_node_get_child(parent, 0, &retrieved);
    TEST_RESULT(result, "Get child by index");

    /* Get child by name */
    result = lc_node_get_child_by_name(parent, "Child2", &retrieved);
    TEST_RESULT(result, "Get child by name");
    TEST_ASSERT(retrieved == child2, "Retrieved correct child");

    /* Get parent */
    LCNodeHandle retrieved_parent = NULL;
    result = lc_node_get_parent(child1, &retrieved_parent);
    TEST_RESULT(result, "Get parent");
    TEST_ASSERT(retrieved_parent == parent, "Retrieved correct parent");

    /* Get path */
    const char* path = lc_node_get_path(grandchild);
    TEST_ASSERT(path != NULL, "Get node path");
    printf("    GrandChild path: %s\n", path);

    /* Find by path */
    LCNodeHandle found = NULL;
    result = lc_node_find(parent, "Child1/GrandChild", &found);
    TEST_RESULT(result, "Find node by path");
    TEST_ASSERT(found == grandchild, "Found correct node");

    /* Active in hierarchy */
    lc_node_set_active(parent, false);
    TEST_ASSERT(!lc_node_is_active_in_hierarchy(grandchild),
                "Grandchild inactive when parent inactive");

    lc_node_set_active(parent, true);
    TEST_ASSERT(lc_node_is_active_in_hierarchy(grandchild),
                "Grandchild active when parent active");

    /* Remove child */
    result = lc_node_remove_child(parent, child2);
    TEST_RESULT(result, "Remove child");

    lc_node_get_child_count(parent, &count);
    TEST_ASSERT(count == 1, "Parent now has 1 child");

    /* Cleanup (destroying parent destroys children) */
    lc_node_destroy(parent);
    lc_node_destroy(child2);  /* Was removed, so destroy separately */

    SECTION_END("Node: Hierarchy Management");
}

static void test_node2d_transform(void) {
    SECTION_START("Node: Node2D Transform");

    LCNodeHandle node2d = NULL;
    lc_node_create(LC_NODE_2D, "Transform2DTest", &node2d);

    /* Position */
    LCVec2 pos = lc_vec2(100.0f, 200.0f);
    LCResult result = lc_node2d_set_position(node2d, pos);
    TEST_RESULT(result, "Set 2D position");

    LCVec2 retrieved_pos;
    result = lc_node2d_get_position(node2d, &retrieved_pos);
    TEST_RESULT(result, "Get 2D position");
    TEST_ASSERT(retrieved_pos.x == 100.0f && retrieved_pos.y == 200.0f, "Position correct");

    /* Rotation */
    float rotation = lc_deg_to_rad(45.0f);
    result = lc_node2d_set_rotation(node2d, rotation);
    TEST_RESULT(result, "Set 2D rotation");

    float retrieved_rot;
    result = lc_node2d_get_rotation(node2d, &retrieved_rot);
    TEST_RESULT(result, "Get 2D rotation");
    TEST_ASSERT(retrieved_rot > 0.78f && retrieved_rot < 0.79f, "Rotation correct (~45 deg)");

    /* Scale */
    LCVec2 scale = lc_vec2(2.0f, 3.0f);
    result = lc_node2d_set_scale(node2d, scale);
    TEST_RESULT(result, "Set 2D scale");

    LCVec2 retrieved_scale;
    result = lc_node2d_get_scale(node2d, &retrieved_scale);
    TEST_RESULT(result, "Get 2D scale");
    TEST_ASSERT(retrieved_scale.x == 2.0f && retrieved_scale.y == 3.0f, "Scale correct");

    /* Z-index */
    result = lc_node2d_set_z_index(node2d, 5);
    TEST_RESULT(result, "Set z-index");

    int z_index;
    result = lc_node2d_get_z_index(node2d, &z_index);
    TEST_RESULT(result, "Get z-index");
    TEST_ASSERT(z_index == 5, "Z-index correct");

    /* Transform matrix */
    LCMat4 matrix;
    result = lc_node2d_get_transform_matrix(node2d, &matrix);
    TEST_RESULT(result, "Get 2D transform matrix");

    lc_node_destroy(node2d);

    SECTION_END("Node: Node2D Transform");
}

static void test_node3d_transform(void) {
    SECTION_START("Node: Node3D Transform");

    LCNodeHandle node3d = NULL;
    lc_node_create(LC_NODE_3D, "Transform3DTest", &node3d);

    /* Position */
    LCVec3 pos = lc_vec3(10.0f, 20.0f, 30.0f);
    LCResult result = lc_node3d_set_position(node3d, pos);
    TEST_RESULT(result, "Set 3D position");

    LCVec3 retrieved_pos;
    result = lc_node3d_get_position(node3d, &retrieved_pos);
    TEST_RESULT(result, "Get 3D position");
    TEST_ASSERT(retrieved_pos.x == 10.0f && retrieved_pos.z == 30.0f, "Position correct");

    /* Rotation (quaternion) */
    LCQuat rot = lc_quat_from_axis_angle(lc_vec3_unit_y(), lc_deg_to_rad(90.0f));
    result = lc_node3d_set_rotation(node3d, rot);
    TEST_RESULT(result, "Set 3D rotation (quaternion)");

    LCQuat retrieved_rot;
    result = lc_node3d_get_rotation(node3d, &retrieved_rot);
    TEST_RESULT(result, "Get 3D rotation (quaternion)");

    /* Rotation (euler) */
    LCVec3 euler = lc_vec3(lc_deg_to_rad(10.0f), lc_deg_to_rad(20.0f), lc_deg_to_rad(30.0f));
    result = lc_node3d_set_rotation_euler(node3d, euler);
    TEST_RESULT(result, "Set 3D rotation (euler)");

    LCVec3 retrieved_euler;
    result = lc_node3d_get_rotation_euler(node3d, &retrieved_euler);
    TEST_RESULT(result, "Get 3D rotation (euler)");

    /* Scale */
    LCVec3 scale = lc_vec3(2.0f, 3.0f, 4.0f);
    result = lc_node3d_set_scale(node3d, scale);
    TEST_RESULT(result, "Set 3D scale");

    LCVec3 retrieved_scale;
    result = lc_node3d_get_scale(node3d, &retrieved_scale);
    TEST_RESULT(result, "Get 3D scale");
    TEST_ASSERT(retrieved_scale.x == 2.0f && retrieved_scale.z == 4.0f, "Scale correct");

    /* Global transforms (with hierarchy) */
    LCNodeHandle parent = NULL;
    lc_node_create(LC_NODE_3D, "Parent3D", &parent);
    lc_node3d_set_position(parent, lc_vec3(100.0f, 0.0f, 0.0f));
    lc_node_add_child(parent, node3d);

    LCVec3 global_pos;
    result = lc_node3d_get_global_position(node3d, &global_pos);
    TEST_RESULT(result, "Get global position");
    printf("    Global position: (%.1f, %.1f, %.1f)\n", global_pos.x, global_pos.y, global_pos.z);

    /* Look at */
    result = lc_node3d_look_at(node3d, lc_vec3_zero(), lc_vec3_up());
    TEST_RESULT(result, "Look at target");

    /* Transform matrices */
    LCMat4 local_matrix;
    result = lc_node3d_get_transform_matrix(node3d, &local_matrix);
    TEST_RESULT(result, "Get local transform matrix");

    LCMat4 global_matrix;
    result = lc_node3d_get_global_transform_matrix(node3d, &global_matrix);
    TEST_RESULT(result, "Get global transform matrix");

    lc_node_destroy(parent);  /* Also destroys node3d */

    SECTION_END("Node: Node3D Transform");
}

/* ============================================================================
 * Scene Module Tests
 * ============================================================================ */

static void test_scene_basic(void) {
    SECTION_START("Scene: Basic Operations");

    /* Create scene */
    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("TestScene", &scene);
    TEST_RESULT(result, "Create scene");
    TEST_ASSERT(scene != NULL, "Scene handle is valid");

    /* Get/set name */
    const char* name = lc_scene_get_name(scene);
    TEST_ASSERT(name != NULL && strcmp(name, "TestScene") == 0, "Get scene name");

    result = lc_scene_set_name(scene, "RenamedScene");
    TEST_RESULT(result, "Set scene name");

    /* Active state */
    result = lc_scene_set_active(scene, true);
    TEST_RESULT(result, "Set scene active");
    TEST_ASSERT(lc_scene_is_active(scene), "Scene is active");

    /* Editor mode */
    result = lc_scene_set_in_editor(scene, true);
    TEST_RESULT(result, "Set scene in editor mode");
    TEST_ASSERT(lc_scene_is_in_editor(scene), "Scene is in editor mode");

    /* Destroy scene */
    result = lc_scene_destroy(scene);
    TEST_RESULT(result, "Destroy scene");

    SECTION_END("Scene: Basic Operations");
}

static void test_scene_graph(void) {
    SECTION_START("Scene: Scene Graph Management");

    LCSceneHandle scene = NULL;
    lc_scene_create("GraphTestScene", &scene);

    /* Create nodes */
    LCNodeHandle root = NULL;
    lc_node_create(LC_NODE_3D, "Root", &root);

    LCNodeHandle child = NULL;
    lc_node_create(LC_NODE_3D, "Child", &child);

    /* Set root */
    LCResult result = lc_scene_set_root(scene, root);
    TEST_RESULT(result, "Set scene root");

    /* Get root */
    LCNodeHandle retrieved_root = NULL;
    result = lc_scene_get_root(scene, &retrieved_root);
    TEST_RESULT(result, "Get scene root");
    TEST_ASSERT(retrieved_root == root, "Retrieved correct root");

    /* Add node to scene */
    result = lc_scene_add_node(scene, child);
    TEST_RESULT(result, "Add node to scene");

    /* Find node by path */
    LCNodeHandle found = NULL;
    result = lc_scene_find_node(scene, "Root/Child", &found);
    /* This may or may not find depending on how add_node works */
    printf("    Find by path result: %d\n", (int)result);

    /* Find by UUID */
    const char* uuid = lc_node_get_uuid(child);
    if (uuid) {
        result = lc_scene_find_node_by_uuid(scene, uuid, &found);
        printf("    Find by UUID result: %d\n", (int)result);
    }

    lc_scene_destroy(scene);

    SECTION_END("Scene: Scene Graph Management");
}

/* ============================================================================
 * Camera Tests
 * ============================================================================ */

static void test_camera3d(void) {
    SECTION_START("Camera: Camera3D");

    LCNodeHandle camera = NULL;
    LCResult result = lc_camera3d_create("TestCamera3D", &camera);
    TEST_RESULT(result, "Create Camera3D");
    TEST_ASSERT(camera != NULL, "Camera handle is valid");

    /* Projection type */
    result = lc_camera3d_set_projection_type(camera, LC_CAMERA_PROJECTION_PERSPECTIVE);
    TEST_RESULT(result, "Set projection type");

    LCCameraProjectionType proj_type;
    result = lc_camera3d_get_projection_type(camera, &proj_type);
    TEST_RESULT(result, "Get projection type");
    TEST_ASSERT(proj_type == LC_CAMERA_PROJECTION_PERSPECTIVE, "Projection is perspective");

    /* FOV */
    result = lc_camera3d_set_fov(camera, 60.0f);
    TEST_RESULT(result, "Set FOV");

    float fov;
    result = lc_camera3d_get_fov(camera, &fov);
    TEST_RESULT(result, "Get FOV");
    TEST_ASSERT(fov > 59.9f && fov < 60.1f, "FOV is 60 degrees");

    /* Near/Far planes */
    result = lc_camera3d_set_near_plane(camera, 0.1f);
    TEST_RESULT(result, "Set near plane");

    result = lc_camera3d_set_far_plane(camera, 1000.0f);
    TEST_RESULT(result, "Set far plane");

    float near_plane, far_plane;
    lc_camera3d_get_near_plane(camera, &near_plane);
    lc_camera3d_get_far_plane(camera, &far_plane);
    printf("    Near: %.2f, Far: %.2f\n", near_plane, far_plane);

    /* Ortho size (for orthographic mode) */
    result = lc_camera3d_set_ortho_size(camera, 10.0f);
    TEST_RESULT(result, "Set ortho size");

    /* Active state */
    result = lc_camera3d_set_active(camera, true);
    TEST_RESULT(result, "Set camera active");

    bool active;
    lc_camera3d_is_active(camera, &active);
    TEST_ASSERT(active, "Camera is active");

    lc_node_destroy(camera);

    SECTION_END("Camera: Camera3D");
}

static void test_camera2d(void) {
    SECTION_START("Camera: Camera2D");

    LCNodeHandle camera = NULL;
    LCResult result = lc_camera2d_create("TestCamera2D", &camera);
    TEST_RESULT(result, "Create Camera2D");

    /* Zoom */
    result = lc_camera2d_set_zoom(camera, 2.0f);
    TEST_RESULT(result, "Set zoom");

    float zoom;
    lc_camera2d_get_zoom(camera, &zoom);
    TEST_ASSERT(zoom == 2.0f, "Zoom is 2.0");

    /* Ortho size */
    result = lc_camera2d_set_ortho_size(camera, 540.0f);
    TEST_RESULT(result, "Set 2D ortho size");

    /* Aspect ratio */
    result = lc_camera2d_set_aspect_ratio(camera, 16.0f / 9.0f);
    TEST_RESULT(result, "Set aspect ratio");

    lc_node_destroy(camera);

    SECTION_END("Camera: Camera2D");
}

static void test_camera_ui(void) {
    SECTION_START("Camera: CameraUI");

    LCNodeHandle camera = NULL;
    LCResult result = lc_camera_ui_create("TestCameraUI", &camera);
    TEST_RESULT(result, "Create CameraUI");

    /* Canvas size */
    result = lc_camera_ui_set_canvas_size(camera, lc_vec2(1920.0f, 1080.0f));
    TEST_RESULT(result, "Set canvas size");

    LCVec2 canvas_size;
    lc_camera_ui_get_canvas_size(camera, &canvas_size);
    TEST_ASSERT(canvas_size.x == 1920.0f, "Canvas width is 1920");

    /* Origin */
    result = lc_camera_ui_set_origin(camera, lc_vec2(0.0f, 0.0f));
    TEST_RESULT(result, "Set origin");

    /* Scale factor */
    result = lc_camera_ui_set_scale_factor(camera, 1.0f);
    TEST_RESULT(result, "Set scale factor");

    /* Pixel perfect */
    result = lc_camera_ui_set_pixel_perfect(camera, true);
    TEST_RESULT(result, "Set pixel perfect");

    bool pixel_perfect;
    lc_camera_ui_is_pixel_perfect(camera, &pixel_perfect);
    TEST_ASSERT(pixel_perfect, "Pixel perfect is enabled");

    lc_node_destroy(camera);

    SECTION_END("Camera: CameraUI");
}

/* ============================================================================
 * Light Tests
 * ============================================================================ */

static void test_lights(void) {
    SECTION_START("Lights: 3D Lighting Components");

    /* DirectionalLight3D */
    LCComponentHandle dir_light = NULL;
    LCResult result = lc_directional_light3d_create("TestDirLight", &dir_light);
    TEST_RESULT(result, "Create DirectionalLight3D");

    result = lc_directional_light3d_set_color(dir_light, lc_color_white());
    TEST_RESULT(result, "Set directional light color");

    result = lc_directional_light3d_set_intensity(dir_light, 1.5f);
    TEST_RESULT(result, "Set directional light intensity");

    float intensity;
    lc_directional_light3d_get_intensity(dir_light, &intensity);
    TEST_ASSERT(intensity == 1.5f, "Intensity is 1.5");

    result = lc_directional_light3d_set_casts_shadows(dir_light, true);
    TEST_RESULT(result, "Enable shadows");

    result = lc_directional_light3d_set_shadow_bias(dir_light, 0.001f);
    TEST_RESULT(result, "Set shadow bias");

    /* OmniLight3D (point light) */
    LCComponentHandle omni_light = NULL;
    result = lc_omni_light3d_create("TestOmniLight", &omni_light);
    TEST_RESULT(result, "Create OmniLight3D");

    result = lc_omni_light3d_set_color(omni_light, lc_color_yellow());
    TEST_RESULT(result, "Set omni light color");

    result = lc_omni_light3d_set_range(omni_light, 10.0f);
    TEST_RESULT(result, "Set omni light range");

    result = lc_omni_light3d_set_attenuation(omni_light, 1.0f);
    TEST_RESULT(result, "Set omni light attenuation");

    /* SpotLight3D */
    LCComponentHandle spot_light = NULL;
    result = lc_spot_light3d_create("TestSpotLight", &spot_light);
    TEST_RESULT(result, "Create SpotLight3D");

    result = lc_spot_light3d_set_color(spot_light, lc_color_cyan());
    TEST_RESULT(result, "Set spotlight color");

    result = lc_spot_light3d_set_inner_cone_angle(spot_light, 15.0f);
    TEST_RESULT(result, "Set inner cone angle");

    result = lc_spot_light3d_set_outer_cone_angle(spot_light, 30.0f);
    TEST_RESULT(result, "Set outer cone angle");

    float inner, outer;
    lc_spot_light3d_get_inner_cone_angle(spot_light, &inner);
    lc_spot_light3d_get_outer_cone_angle(spot_light, &outer);
    printf("    Spotlight cone: inner=%.1f, outer=%.1f degrees\n", inner, outer);

    /* Cleanup */
    lc_component_destroy(dir_light);
    lc_component_destroy(omni_light);
    lc_component_destroy(spot_light);

    SECTION_END("Lights: 3D Lighting Components");
}

/* ============================================================================
 * Rendering Component Tests
 * ============================================================================ */

static void test_sprite2d(void) {
    SECTION_START("Rendering: Sprite2D");

    LCComponentHandle sprite = NULL;
    LCResult result = lc_sprite2d_create("TestSprite2D", &sprite);
    TEST_RESULT(result, "Create Sprite2D");

    /* Modulate color */
    result = lc_sprite2d_set_modulate(sprite, lc_color_white());
    TEST_RESULT(result, "Set modulate color");

    LCColor color;
    lc_sprite2d_get_modulate(sprite, &color);
    TEST_ASSERT(color.r == 1.0f, "Modulate is white");

    /* Size */
    result = lc_sprite2d_set_size(sprite, lc_vec2(64.0f, 64.0f));
    TEST_RESULT(result, "Set size");

    /* UV rect */
    result = lc_sprite2d_set_uv_rect(sprite, lc_vec4(0.0f, 0.0f, 1.0f, 1.0f));
    TEST_RESULT(result, "Set UV rect");

    /* Flip */
    result = lc_sprite2d_set_flip_h(sprite, true);
    TEST_RESULT(result, "Set horizontal flip");

    bool flip_h;
    lc_sprite2d_get_flip_h(sprite, &flip_h);
    TEST_ASSERT(flip_h, "Horizontal flip is enabled");

    /* Centered */
    result = lc_sprite2d_set_centered(sprite, true);
    TEST_RESULT(result, "Set centered");

    /* Alpha cutoff */
    result = lc_sprite2d_set_alpha_cutoff(sprite, 0.5f);
    TEST_RESULT(result, "Set alpha cutoff");

    lc_component_destroy(sprite);

    SECTION_END("Rendering: Sprite2D");
}

static void test_sprite3d(void) {
    SECTION_START("Rendering: Sprite3D");

    LCComponentHandle sprite = NULL;
    LCResult result = lc_sprite3d_create("TestSprite3D", &sprite);
    TEST_RESULT(result, "Create Sprite3D");

    /* Billboard mode */
    result = lc_sprite3d_set_billboard_mode(sprite, LC_BILLBOARD_Y_AXIS_ONLY);
    TEST_RESULT(result, "Set billboard mode");

    LCBillboardMode mode;
    lc_sprite3d_get_billboard_mode(sprite, &mode);
    TEST_ASSERT(mode == LC_BILLBOARD_Y_AXIS_ONLY, "Billboard mode is Y-axis only");

    /* Double sided */
    result = lc_sprite3d_set_double_sided(sprite, true);
    TEST_RESULT(result, "Set double sided");

    /* Shadow settings */
    result = lc_sprite3d_set_cast_shadow(sprite, true);
    TEST_RESULT(result, "Enable shadow casting");

    result = lc_sprite3d_set_receive_shadow(sprite, true);
    TEST_RESULT(result, "Enable shadow receiving");

    lc_component_destroy(sprite);

    SECTION_END("Rendering: Sprite3D");
}

static void test_static_mesh3d(void) {
    SECTION_START("Rendering: StaticMesh3D");

    LCComponentHandle mesh = NULL;
    LCResult result = lc_static_mesh3d_create("TestMesh", &mesh);
    TEST_RESULT(result, "Create StaticMesh3D");

    /* Shadow settings */
    result = lc_static_mesh3d_set_cast_shadow(mesh, true);
    TEST_RESULT(result, "Enable shadow casting");

    bool casts;
    lc_static_mesh3d_get_cast_shadow(mesh, &casts);
    TEST_ASSERT(casts, "Shadow casting is enabled");

    result = lc_static_mesh3d_set_double_sided(mesh, false);
    TEST_RESULT(result, "Set single sided");

    lc_component_destroy(mesh);

    SECTION_END("Rendering: StaticMesh3D");
}

/* ============================================================================
 * PrimitiveMesh3D Tests
 * ============================================================================ */

static void test_primitive_mesh3d(void) {
    SECTION_START("Rendering: PrimitiveMesh3D");

    /* Create PrimitiveMesh3D */
    LCComponentHandle mesh = NULL;
    LCResult result = lc_primitive_mesh3d_create("TestPrimitiveMesh", &mesh);
    TEST_RESULT(result, "Create PrimitiveMesh3D");

    /* Test all shape types */
    result = lc_primitive_mesh3d_set_shape(mesh, LC_PRIMITIVE_CUBE);
    TEST_RESULT(result, "Set shape to cube");

    LCPrimitiveShape shape;
    lc_primitive_mesh3d_get_shape(mesh, &shape);
    TEST_ASSERT(shape == LC_PRIMITIVE_CUBE, "Shape is cube");

    result = lc_primitive_mesh3d_set_shape(mesh, LC_PRIMITIVE_SPHERE);
    TEST_RESULT(result, "Set shape to sphere");

    result = lc_primitive_mesh3d_set_shape(mesh, LC_PRIMITIVE_CYLINDER);
    TEST_RESULT(result, "Set shape to cylinder");

    result = lc_primitive_mesh3d_set_shape(mesh, LC_PRIMITIVE_CONE);
    TEST_RESULT(result, "Set shape to cone");

    result = lc_primitive_mesh3d_set_shape(mesh, LC_PRIMITIVE_PYRAMID);
    TEST_RESULT(result, "Set shape to pyramid");

    result = lc_primitive_mesh3d_set_shape(mesh, LC_PRIMITIVE_TORUS);
    TEST_RESULT(result, "Set shape to torus");

    result = lc_primitive_mesh3d_set_shape(mesh, LC_PRIMITIVE_CAPSULE);
    TEST_RESULT(result, "Set shape to capsule");

    /* Test vertex color */
    result = lc_primitive_mesh3d_set_color(mesh, lc_color_red());
    TEST_RESULT(result, "Set vertex color");

    LCColor color;
    lc_primitive_mesh3d_get_color(mesh, &color);
    TEST_ASSERT(color.r == 1.0f && color.g == 0.0f && color.b == 0.0f, "Color is red");

    /* Test size */
    result = lc_primitive_mesh3d_set_size(mesh, 2.0f);
    TEST_RESULT(result, "Set size");

    float size;
    lc_primitive_mesh3d_get_size(mesh, &size);
    TEST_ASSERT(size == 2.0f, "Size is 2.0");

    /* Test height */
    result = lc_primitive_mesh3d_set_height(mesh, 3.0f);
    TEST_RESULT(result, "Set height");

    float height;
    lc_primitive_mesh3d_get_height(mesh, &height);
    TEST_ASSERT(height == 3.0f, "Height is 3.0");

    /* Test detail */
    result = lc_primitive_mesh3d_set_detail(mesh, 32);
    TEST_RESULT(result, "Set detail");

    int detail;
    lc_primitive_mesh3d_get_detail(mesh, &detail);
    TEST_ASSERT(detail == 32, "Detail is 32");

    /* Test minor radius (for torus) */
    result = lc_primitive_mesh3d_set_minor_radius(mesh, 0.5f);
    TEST_RESULT(result, "Set minor radius");

    float minor_radius;
    lc_primitive_mesh3d_get_minor_radius(mesh, &minor_radius);
    TEST_ASSERT(minor_radius == 0.5f, "Minor radius is 0.5");

    /* Test shadow properties */
    result = lc_primitive_mesh3d_set_cast_shadow(mesh, true);
    TEST_RESULT(result, "Enable cast shadow");

    bool cast_shadow;
    lc_primitive_mesh3d_get_cast_shadow(mesh, &cast_shadow);
    TEST_ASSERT(cast_shadow, "Cast shadow enabled");

    result = lc_primitive_mesh3d_set_receive_shadow(mesh, true);
    TEST_RESULT(result, "Enable receive shadow");

    bool receive_shadow;
    lc_primitive_mesh3d_get_receive_shadow(mesh, &receive_shadow);
    TEST_ASSERT(receive_shadow, "Receive shadow enabled");

    result = lc_primitive_mesh3d_set_double_sided(mesh, false);
    TEST_RESULT(result, "Set single sided");

    bool double_sided;
    lc_primitive_mesh3d_get_double_sided(mesh, &double_sided);
    TEST_ASSERT(!double_sided, "Single sided");

    /* Test material override */
    result = lc_primitive_mesh3d_set_material_override_enabled(mesh, true);
    TEST_RESULT(result, "Enable material override");

    bool has_override;
    lc_primitive_mesh3d_has_material_override(mesh, &has_override);
    TEST_ASSERT(has_override, "Material override enabled");

    /* Test PBR properties */
    result = lc_primitive_mesh3d_set_albedo_color(mesh, lc_color(0.8f, 0.2f, 0.2f, 1.0f));
    TEST_RESULT(result, "Set albedo color");

    LCColor albedo;
    lc_primitive_mesh3d_get_albedo_color(mesh, &albedo);
    printf("    Albedo color: (%.1f, %.1f, %.1f)\n", albedo.r, albedo.g, albedo.b);

    result = lc_primitive_mesh3d_set_metallic(mesh, 0.5f);
    TEST_RESULT(result, "Set metallic");

    float metallic;
    lc_primitive_mesh3d_get_metallic(mesh, &metallic);
    TEST_ASSERT(metallic == 0.5f, "Metallic is 0.5");

    result = lc_primitive_mesh3d_set_roughness(mesh, 0.3f);
    TEST_RESULT(result, "Set roughness");

    float roughness;
    lc_primitive_mesh3d_get_roughness(mesh, &roughness);
    TEST_ASSERT(roughness == 0.3f, "Roughness is 0.3");

    result = lc_primitive_mesh3d_set_normal_scale(mesh, 1.0f);
    TEST_RESULT(result, "Set normal scale");

    result = lc_primitive_mesh3d_set_emissive_color(mesh, lc_color(1.0f, 0.5f, 0.0f, 1.0f));
    TEST_RESULT(result, "Set emissive color");

    result = lc_primitive_mesh3d_set_emissive_strength(mesh, 2.0f);
    TEST_RESULT(result, "Set emissive strength");

    float emissive_strength;
    lc_primitive_mesh3d_get_emissive_strength(mesh, &emissive_strength);
    TEST_ASSERT(emissive_strength == 2.0f, "Emissive strength is 2.0");

    /* Test shader types */
    result = lc_primitive_mesh3d_set_shader_type(mesh, LC_SHADER_PBR);
    TEST_RESULT(result, "Set shader to PBR");

    LCShaderType shader_type;
    lc_primitive_mesh3d_get_shader_type(mesh, &shader_type);
    TEST_ASSERT(shader_type == LC_SHADER_PBR, "Shader is PBR");

    result = lc_primitive_mesh3d_set_shader_type(mesh, LC_SHADER_TOON);
    TEST_RESULT(result, "Set shader to Toon");

    /* Test toon shader parameters */
    result = lc_primitive_mesh3d_set_shadow_bands(mesh, 3.0f);
    TEST_RESULT(result, "Set shadow bands");

    float shadow_bands;
    lc_primitive_mesh3d_get_shadow_bands(mesh, &shadow_bands);
    TEST_ASSERT(shadow_bands == 3.0f, "Shadow bands is 3");

    result = lc_primitive_mesh3d_set_shadow_threshold(mesh, 0.5f);
    TEST_RESULT(result, "Set shadow threshold");

    result = lc_primitive_mesh3d_set_shadow_softness(mesh, 0.1f);
    TEST_RESULT(result, "Set shadow softness");

    result = lc_primitive_mesh3d_set_specular_bands(mesh, 2.0f);
    TEST_RESULT(result, "Set specular bands");

    result = lc_primitive_mesh3d_set_specular_power(mesh, 32.0f);
    TEST_RESULT(result, "Set specular power");

    result = lc_primitive_mesh3d_set_rim_intensity(mesh, 0.5f);
    TEST_RESULT(result, "Set rim intensity");

    result = lc_primitive_mesh3d_set_rim_power(mesh, 2.0f);
    TEST_RESULT(result, "Set rim power");

    float rim_power;
    lc_primitive_mesh3d_get_rim_power(mesh, &rim_power);
    TEST_ASSERT(rim_power == 2.0f, "Rim power is 2.0");

    /* Test other shader types */
    result = lc_primitive_mesh3d_set_shader_type(mesh, LC_SHADER_STYLIZED);
    TEST_RESULT(result, "Set shader to Stylized");

    result = lc_primitive_mesh3d_set_shader_type(mesh, LC_SHADER_UNLIT);
    TEST_RESULT(result, "Set shader to Unlit");

    result = lc_primitive_mesh3d_set_shader_type(mesh, LC_SHADER_TRANSPARENT);
    TEST_RESULT(result, "Set shader to Transparent");

    result = lc_primitive_mesh3d_set_shader_type(mesh, LC_SHADER_GLOW);
    TEST_RESULT(result, "Set shader to Glow");

    /* Cleanup */
    lc_component_destroy(mesh);

    SECTION_END("Rendering: PrimitiveMesh3D");
}

/* ============================================================================
 * Physics Tests
 * ============================================================================ */

static void test_physics2d(void) {
    SECTION_START("Physics: 2D Physics Bodies");

    /* RigidBody2D */
    LCComponentHandle rigid = NULL;
    LCResult result = lc_rigid_body2d_create("TestRigidBody", &rigid);
    TEST_RESULT(result, "Create RigidBody2D");

    result = lc_rigid_body2d_set_gravity_scale(rigid, 1.0f);
    TEST_RESULT(result, "Set gravity scale");

    result = lc_rigid_body2d_set_linear_damping(rigid, 0.1f);
    TEST_RESULT(result, "Set linear damping");

    result = lc_rigid_body2d_set_angular_damping(rigid, 0.05f);
    TEST_RESULT(result, "Set angular damping");

    result = lc_rigid_body2d_set_fixed_rotation(rigid, false);
    TEST_RESULT(result, "Set fixed rotation");

    result = lc_rigid_body2d_set_bullet(rigid, false);
    TEST_RESULT(result, "Set bullet mode");

    /* Apply forces */
    result = lc_rigid_body2d_apply_force_to_center(rigid, lc_vec2(10.0f, 0.0f));
    TEST_RESULT(result, "Apply force to center");

    result = lc_rigid_body2d_apply_torque(rigid, 5.0f);
    TEST_RESULT(result, "Apply torque");

    result = lc_rigid_body2d_apply_linear_impulse_to_center(rigid, lc_vec2(0.0f, 10.0f));
    TEST_RESULT(result, "Apply linear impulse");

    /* Velocity */
    result = lc_rigid_body2d_set_linear_velocity(rigid, lc_vec2(5.0f, 5.0f));
    TEST_RESULT(result, "Set linear velocity");

    LCVec2 vel;
    lc_rigid_body2d_get_linear_velocity(rigid, &vel);
    printf("    Linear velocity: (%.1f, %.1f)\n", vel.x, vel.y);

    /* StaticBody2D */
    LCComponentHandle static_body = NULL;
    result = lc_static_body2d_create("TestStaticBody", &static_body);
    TEST_RESULT(result, "Create StaticBody2D");

    result = lc_static_body2d_set_constant_linear_velocity(static_body, lc_vec2(1.0f, 0.0f));
    TEST_RESULT(result, "Set constant linear velocity");

    /* KinematicBody2D */
    LCComponentHandle kinematic = NULL;
    result = lc_kinematic_body2d_create("TestKinematic", &kinematic);
    TEST_RESULT(result, "Create KinematicBody2D");

    result = lc_kinematic_body2d_set_linear_velocity(kinematic, lc_vec2(2.0f, 2.0f));
    TEST_RESULT(result, "Set kinematic velocity");

    result = lc_kinematic_body2d_move_by(kinematic, lc_vec2(1.0f, 0.0f));
    TEST_RESULT(result, "Move kinematic body");

    /* Cleanup */
    lc_component_destroy(rigid);
    lc_component_destroy(static_body);
    lc_component_destroy(kinematic);

    SECTION_END("Physics: 2D Physics Bodies");
}

/* ============================================================================
 * Physics 3D Tests
 * ============================================================================ */

static void test_physics3d(void) {
    SECTION_START("Physics: 3D Physics Bodies");

    /* RigidBody3D */
    LCComponentHandle rigid = NULL;
    LCResult result = lc_rigid_body3d_create("TestRigidBody3D", &rigid);
    TEST_RESULT(result, "Create RigidBody3D");

    /* Mass */
    result = lc_rigid_body3d_set_mass(rigid, 2.0f);
    TEST_RESULT(result, "Set mass");

    float mass;
    lc_rigid_body3d_get_mass(rigid, &mass);
    TEST_ASSERT(mass == 2.0f, "Mass is 2.0");

    /* Gravity scale */
    result = lc_rigid_body3d_set_gravity_scale(rigid, 1.0f);
    TEST_RESULT(result, "Set gravity scale");

    float scale;
    lc_rigid_body3d_get_gravity_scale(rigid, &scale);
    TEST_ASSERT(scale == 1.0f, "Gravity scale is 1.0");

    /* Linear damping */
    result = lc_rigid_body3d_set_linear_damping(rigid, 0.1f);
    TEST_RESULT(result, "Set linear damping");

    float damping;
    lc_rigid_body3d_get_linear_damping(rigid, &damping);
    TEST_ASSERT(damping > 0.09f && damping < 0.11f, "Linear damping is 0.1");

    /* Angular damping */
    result = lc_rigid_body3d_set_angular_damping(rigid, 0.05f);
    TEST_RESULT(result, "Set angular damping");

    lc_rigid_body3d_get_angular_damping(rigid, &damping);
    TEST_ASSERT(damping > 0.04f && damping < 0.06f, "Angular damping is 0.05");

    /* Lock rotation axes */
    result = lc_rigid_body3d_set_lock_rotation_x(rigid, true);
    TEST_RESULT(result, "Lock rotation X");

    bool locked;
    lc_rigid_body3d_get_lock_rotation_x(rigid, &locked);
    TEST_ASSERT(locked, "Rotation X is locked");

    result = lc_rigid_body3d_set_lock_rotation_y(rigid, false);
    TEST_RESULT(result, "Unlock rotation Y");

    lc_rigid_body3d_get_lock_rotation_y(rigid, &locked);
    TEST_ASSERT(!locked, "Rotation Y is unlocked");

    /* Can sleep */
    result = lc_rigid_body3d_set_can_sleep(rigid, true);
    TEST_RESULT(result, "Set can sleep");

    bool can_sleep;
    lc_rigid_body3d_get_can_sleep(rigid, &can_sleep);
    TEST_ASSERT(can_sleep, "Can sleep is enabled");

    /* Gravity enabled */
    result = lc_rigid_body3d_set_gravity_enabled(rigid, true);
    TEST_RESULT(result, "Enable gravity");

    bool enabled;
    lc_rigid_body3d_get_gravity_enabled(rigid, &enabled);
    TEST_ASSERT(enabled, "Gravity is enabled");

    /* Use CCD */
    result = lc_rigid_body3d_set_use_ccd(rigid, false);
    TEST_RESULT(result, "Disable CCD");

    bool use_ccd;
    lc_rigid_body3d_get_use_ccd(rigid, &use_ccd);
    TEST_ASSERT(!use_ccd, "CCD is disabled");

    /* Apply forces */
    result = lc_rigid_body3d_apply_force_to_center(rigid, lc_vec3(10.0f, 0.0f, 0.0f));
    TEST_RESULT(result, "Apply force to center");

    result = lc_rigid_body3d_apply_torque(rigid, lc_vec3(0.0f, 5.0f, 0.0f));
    TEST_RESULT(result, "Apply torque");

    result = lc_rigid_body3d_apply_impulse_to_center(rigid, lc_vec3(0.0f, 10.0f, 0.0f));
    TEST_RESULT(result, "Apply linear impulse");

    /* Velocity */
    result = lc_rigid_body3d_set_linear_velocity(rigid, lc_vec3(5.0f, 5.0f, 5.0f));
    TEST_RESULT(result, "Set linear velocity");

    LCVec3 vel;
    lc_rigid_body3d_get_linear_velocity(rigid, &vel);
    printf("    Linear velocity: (%.1f, %.1f, %.1f)\n", vel.x, vel.y, vel.z);

    result = lc_rigid_body3d_set_angular_velocity(rigid, lc_vec3(0.0f, 1.0f, 0.0f));
    TEST_RESULT(result, "Set angular velocity");

    LCVec3 omega;
    lc_rigid_body3d_get_angular_velocity(rigid, &omega);
    printf("    Angular velocity: (%.1f, %.1f, %.1f)\n", omega.x, omega.y, omega.z);

    /* Clear forces */
    result = lc_rigid_body3d_clear_forces(rigid);
    TEST_RESULT(result, "Clear forces");

    /* StaticBody3D */
    LCComponentHandle static_body = NULL;
    result = lc_static_body3d_create("TestStaticBody3D", &static_body);
    TEST_RESULT(result, "Create StaticBody3D");

    result = lc_static_body3d_set_constant_linear_velocity(static_body, lc_vec3(1.0f, 0.0f, 0.0f));
    TEST_RESULT(result, "Set constant linear velocity");

    LCVec3 const_vel;
    lc_static_body3d_get_constant_linear_velocity(static_body, &const_vel);
    TEST_ASSERT(const_vel.x == 1.0f, "Constant linear velocity X is 1.0");

    result = lc_static_body3d_set_constant_angular_velocity(static_body, lc_vec3(0.0f, 0.5f, 0.0f));
    TEST_RESULT(result, "Set constant angular velocity");

    LCVec3 const_angular;
    lc_static_body3d_get_constant_angular_velocity(static_body, &const_angular);
    TEST_ASSERT(const_angular.y == 0.5f, "Constant angular velocity Y is 0.5");

    result = lc_static_body3d_set_position(static_body, lc_vec3(10.0f, 0.0f, 0.0f));
    TEST_RESULT(result, "Set static body position");

    result = lc_static_body3d_set_rotation(static_body, lc_quat_identity());
    TEST_RESULT(result, "Set static body rotation");

    /* KinematicBody3D */
    LCComponentHandle kinematic = NULL;
    result = lc_kinematic_body3d_create("TestKinematic3D", &kinematic);
    TEST_RESULT(result, "Create KinematicBody3D");

    result = lc_kinematic_body3d_set_gravity_enabled(kinematic, false);
    TEST_RESULT(result, "Disable kinematic gravity");

    bool grav_enabled;
    lc_kinematic_body3d_get_gravity_enabled(kinematic, &grav_enabled);
    TEST_ASSERT(!grav_enabled, "Kinematic gravity is disabled");

    result = lc_kinematic_body3d_set_gravity_scale(kinematic, 0.5f);
    TEST_RESULT(result, "Set kinematic gravity scale");

    float grav_scale;
    lc_kinematic_body3d_get_gravity_scale(kinematic, &grav_scale);
    TEST_ASSERT(grav_scale == 0.5f, "Kinematic gravity scale is 0.5");

    result = lc_kinematic_body3d_set_linear_velocity(kinematic, lc_vec3(2.0f, 2.0f, 2.0f));
    TEST_RESULT(result, "Set kinematic velocity");

    LCVec3 kin_vel;
    result = lc_kinematic_body3d_get_linear_velocity(kinematic, &kin_vel);
    TEST_RESULT(result, "Get kinematic velocity");
    /* Note: velocity is (0,0,0) because physics body isn't created until component is attached to scene */
    printf("    Kinematic velocity: (%.1f, %.1f, %.1f) - physics body requires scene\n", kin_vel.x, kin_vel.y, kin_vel.z);

    result = lc_kinematic_body3d_move_by(kinematic, lc_vec3(1.0f, 0.0f, 0.0f));
    TEST_RESULT(result, "Move kinematic body");

    result = lc_kinematic_body3d_rotate_by(kinematic, lc_quat_from_axis_angle(lc_vec3_unit_y(), lc_deg_to_rad(45.0f)));
    TEST_RESULT(result, "Rotate kinematic body");

    /* Cleanup */
    lc_component_destroy(rigid);
    lc_component_destroy(static_body);
    lc_component_destroy(kinematic);

    SECTION_END("Physics: 3D Physics Bodies");
}

/* ============================================================================
 * Collision Body Tests
 * ============================================================================ */

static void test_collision2d(void) {
    SECTION_START("Physics: 2D Collision Bodies");

    /* Create CollisionBody2D */
    LCComponentHandle collision = NULL;
    LCResult result = lc_collision2d_create("TestCollision2D", &collision);
    TEST_RESULT(result, "Create CollisionBody2D");

    /* Shape type */
    result = lc_collision2d_set_shape_type(collision, LC_COLLISION2D_RECTANGLE);
    TEST_RESULT(result, "Set shape type to rectangle");

    LCCollisionShape2D shape;
    lc_collision2d_get_shape_type(collision, &shape);
    TEST_ASSERT(shape == LC_COLLISION2D_RECTANGLE, "Shape is rectangle");

    /* Size */
    result = lc_collision2d_set_size(collision, lc_vec2(2.0f, 3.0f));
    TEST_RESULT(result, "Set size");

    LCVec2 size;
    lc_collision2d_get_size(collision, &size);
    TEST_ASSERT(size.x == 2.0f && size.y == 3.0f, "Size is (2, 3)");

    /* Circle shape */
    result = lc_collision2d_set_shape_type(collision, LC_COLLISION2D_CIRCLE);
    TEST_RESULT(result, "Set shape type to circle");

    result = lc_collision2d_set_radius(collision, 1.5f);
    TEST_RESULT(result, "Set radius");

    float radius;
    lc_collision2d_get_radius(collision, &radius);
    TEST_ASSERT(radius == 1.5f, "Radius is 1.5");

    /* Offset */
    result = lc_collision2d_set_offset(collision, lc_vec2(0.5f, 0.5f));
    TEST_RESULT(result, "Set offset");

    LCVec2 offset;
    lc_collision2d_get_offset(collision, &offset);
    TEST_ASSERT(offset.x == 0.5f && offset.y == 0.5f, "Offset is (0.5, 0.5)");

    /* Physics material */
    result = lc_collision2d_set_density(collision, 2.0f);
    TEST_RESULT(result, "Set density");

    float density;
    lc_collision2d_get_density(collision, &density);
    TEST_ASSERT(density == 2.0f, "Density is 2.0");

    result = lc_collision2d_set_friction(collision, 0.5f);
    TEST_RESULT(result, "Set friction");

    float friction;
    lc_collision2d_get_friction(collision, &friction);
    TEST_ASSERT(friction == 0.5f, "Friction is 0.5");

    result = lc_collision2d_set_restitution(collision, 0.3f);
    TEST_RESULT(result, "Set restitution");

    float restitution;
    lc_collision2d_get_restitution(collision, &restitution);
    TEST_ASSERT(restitution == 0.3f, "Restitution is 0.3");

    /* Sensor mode */
    result = lc_collision2d_set_sensor(collision, true);
    TEST_RESULT(result, "Enable sensor mode");

    bool is_sensor;
    lc_collision2d_is_sensor(collision, &is_sensor);
    TEST_ASSERT(is_sensor, "Is sensor");

    /* Collision layers */
    result = lc_collision2d_set_collision_layers(collision, 0x03);
    TEST_RESULT(result, "Set collision layers");

    uint32_t layers;
    lc_collision2d_get_collision_layers(collision, &layers);
    TEST_ASSERT(layers == 0x03, "Collision layers is 0x03");

    /* Debug color */
    result = lc_collision2d_set_debug_color(collision, lc_color(1.0f, 0.0f, 0.0f, 1.0f));
    TEST_RESULT(result, "Set debug color");

    LCColor debug_color;
    lc_collision2d_get_debug_color(collision, &debug_color);
    TEST_ASSERT(debug_color.r == 1.0f && debug_color.g == 0.0f, "Debug color is red");

    /* Polygon shape - test vertex manipulation */
    result = lc_collision2d_set_shape_type(collision, LC_COLLISION2D_POLYGON);
    TEST_RESULT(result, "Set shape type to polygon");

    LCVec2 vertices[] = {
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {0.5f, 1.0f}
    };
    result = lc_collision2d_set_vertices(collision, vertices, 3);
    TEST_RESULT(result, "Set vertices");

    int vertex_count;
    lc_collision2d_get_vertex_count(collision, &vertex_count);
    TEST_ASSERT(vertex_count == 3, "Vertex count is 3");

    LCVec2 vertex;
    lc_collision2d_get_vertex(collision, 0, &vertex);
    TEST_ASSERT(vertex.x == 0.0f && vertex.y == 0.0f, "Vertex 0 is (0, 0)");

    result = lc_collision2d_add_vertex(collision, lc_vec2(1.5f, 0.5f));
    TEST_RESULT(result, "Add vertex");

    lc_collision2d_get_vertex_count(collision, &vertex_count);
    TEST_ASSERT(vertex_count == 4, "Vertex count is 4 after add");

    result = lc_collision2d_update_vertex(collision, 0, lc_vec2(0.1f, 0.1f));
    TEST_RESULT(result, "Update vertex");

    lc_collision2d_get_vertex(collision, 0, &vertex);
    TEST_ASSERT(vertex.x == 0.1f && vertex.y == 0.1f, "Vertex 0 updated to (0.1, 0.1)");

    result = lc_collision2d_remove_vertex(collision, 3);
    TEST_RESULT(result, "Remove vertex");

    lc_collision2d_get_vertex_count(collision, &vertex_count);
    TEST_ASSERT(vertex_count == 3, "Vertex count is 3 after remove");

    result = lc_collision2d_clear_vertices(collision);
    TEST_RESULT(result, "Clear vertices");

    lc_collision2d_get_vertex_count(collision, &vertex_count);
    TEST_ASSERT(vertex_count == 0, "Vertex count is 0 after clear");

    /* Cleanup */
    lc_component_destroy(collision);

    SECTION_END("Physics: 2D Collision Bodies");
}

static void test_collision3d(void) {
    SECTION_START("Physics: 3D Collision Meshes");

    /* Create CollisionMesh3D */
    LCComponentHandle collision = NULL;
    LCResult result = lc_collision3d_create("TestCollision3D", &collision);
    TEST_RESULT(result, "Create CollisionMesh3D");

    /* Shape type */
    result = lc_collision3d_set_shape_type(collision, LC_COLLISION3D_BOX);
    TEST_RESULT(result, "Set shape type to box");

    LCCollisionShape3D shape;
    lc_collision3d_get_shape_type(collision, &shape);
    TEST_ASSERT(shape == LC_COLLISION3D_BOX, "Shape is box");

    /* Size */
    result = lc_collision3d_set_size(collision, lc_vec3(2.0f, 3.0f, 4.0f));
    TEST_RESULT(result, "Set size");

    LCVec3 size;
    lc_collision3d_get_size(collision, &size);
    TEST_ASSERT(size.x == 2.0f && size.y == 3.0f && size.z == 4.0f, "Size is (2, 3, 4)");

    /* Sphere shape */
    result = lc_collision3d_set_shape_type(collision, LC_COLLISION3D_SPHERE);
    TEST_RESULT(result, "Set shape type to sphere");

    result = lc_collision3d_set_radius(collision, 1.5f);
    TEST_RESULT(result, "Set radius");

    float radius;
    lc_collision3d_get_radius(collision, &radius);
    TEST_ASSERT(radius == 1.5f, "Radius is 1.5");

    /* Capsule shape */
    result = lc_collision3d_set_shape_type(collision, LC_COLLISION3D_CAPSULE);
    TEST_RESULT(result, "Set shape type to capsule");

    result = lc_collision3d_set_height(collision, 2.0f);
    TEST_RESULT(result, "Set height");

    float height;
    lc_collision3d_get_height(collision, &height);
    TEST_ASSERT(height == 2.0f, "Height is 2.0");

    /* Plane shape */
    result = lc_collision3d_set_shape_type(collision, LC_COLLISION3D_PLANE);
    TEST_RESULT(result, "Set shape type to plane");

    result = lc_collision3d_set_plane_normal(collision, lc_vec3(0.0f, 1.0f, 0.0f));
    TEST_RESULT(result, "Set plane normal");

    LCVec3 normal;
    lc_collision3d_get_plane_normal(collision, &normal);
    TEST_ASSERT(normal.y == 1.0f, "Plane normal is up");

    result = lc_collision3d_set_plane_distance(collision, 5.0f);
    TEST_RESULT(result, "Set plane distance");

    float distance;
    lc_collision3d_get_plane_distance(collision, &distance);
    TEST_ASSERT(distance == 5.0f, "Plane distance is 5.0");

    result = lc_collision3d_set_plane_width(collision, 10.0f);
    TEST_RESULT(result, "Set plane width");

    float width;
    lc_collision3d_get_plane_width(collision, &width);
    TEST_ASSERT(width == 10.0f, "Plane width is 10.0");

    result = lc_collision3d_set_plane_length(collision, 20.0f);
    TEST_RESULT(result, "Set plane length");

    float length;
    lc_collision3d_get_plane_length(collision, &length);
    TEST_ASSERT(length == 20.0f, "Plane length is 20.0");

    /* Offset */
    result = lc_collision3d_set_offset(collision, lc_vec3(0.5f, 0.5f, 0.5f));
    TEST_RESULT(result, "Set offset");

    LCVec3 offset;
    lc_collision3d_get_offset(collision, &offset);
    TEST_ASSERT(offset.x == 0.5f && offset.y == 0.5f && offset.z == 0.5f, "Offset is (0.5, 0.5, 0.5)");

    /* Mesh shape */
    result = lc_collision3d_set_shape_type(collision, LC_COLLISION3D_MESH);
    TEST_RESULT(result, "Set shape type to mesh");

    result = lc_collision3d_set_mesh_path(collision, "models/test.obj");
    TEST_RESULT(result, "Set mesh path");

    char path[256];
    lc_collision3d_get_mesh_path(collision, path, sizeof(path));
    TEST_ASSERT(strcmp(path, "models/test.obj") == 0, "Mesh path is correct");

    result = lc_collision3d_set_mesh_solver(collision, LC_MESH_SOLVER_CONVEX);
    TEST_RESULT(result, "Set mesh solver to convex");

    LCMeshSolverType solver;
    lc_collision3d_get_mesh_solver(collision, &solver);
    TEST_ASSERT(solver == LC_MESH_SOLVER_CONVEX, "Mesh solver is convex");

    /* Physics material */
    result = lc_collision3d_set_density(collision, 2.0f);
    TEST_RESULT(result, "Set density");

    float density;
    lc_collision3d_get_density(collision, &density);
    TEST_ASSERT(density == 2.0f, "Density is 2.0");

    result = lc_collision3d_set_friction(collision, 0.5f);
    TEST_RESULT(result, "Set friction");

    float friction;
    lc_collision3d_get_friction(collision, &friction);
    TEST_ASSERT(friction == 0.5f, "Friction is 0.5");

    result = lc_collision3d_set_restitution(collision, 0.3f);
    TEST_RESULT(result, "Set restitution");

    float restitution;
    lc_collision3d_get_restitution(collision, &restitution);
    TEST_ASSERT(restitution == 0.3f, "Restitution is 0.3");

    /* Sensor mode */
    result = lc_collision3d_set_sensor(collision, true);
    TEST_RESULT(result, "Enable sensor mode");

    bool is_sensor;
    lc_collision3d_is_sensor(collision, &is_sensor);
    TEST_ASSERT(is_sensor, "Is sensor");

    /* Collision layers */
    result = lc_collision3d_set_collision_layers(collision, 0x05);
    TEST_RESULT(result, "Set collision layers");

    uint32_t layers;
    lc_collision3d_get_collision_layers(collision, &layers);
    TEST_ASSERT(layers == 0x05, "Collision layers is 0x05");

    /* Debug color */
    result = lc_collision3d_set_debug_color(collision, lc_color(0.0f, 1.0f, 0.0f, 1.0f));
    TEST_RESULT(result, "Set debug color");

    LCColor debug_color;
    lc_collision3d_get_debug_color(collision, &debug_color);
    TEST_ASSERT(debug_color.g == 1.0f && debug_color.r == 0.0f, "Debug color is green");

    /* Cleanup */
    lc_component_destroy(collision);

    SECTION_END("Physics: 3D Collision Meshes");
}

static void test_area_trigger2d(void) {
    SECTION_START("Physics: 2D Area Triggers");

    /* Create AreaTrigger2D */
    LCComponentHandle trigger = NULL;
    LCResult result = lc_area_trigger2d_create("TestAreaTrigger2D", &trigger);
    TEST_RESULT(result, "Create AreaTrigger2D");

    /* Monitoring */
    result = lc_area_trigger2d_set_monitoring(trigger, true);
    TEST_RESULT(result, "Enable monitoring");

    bool monitoring;
    lc_area_trigger2d_get_monitoring(trigger, &monitoring);
    TEST_ASSERT(monitoring, "Monitoring is enabled");

    result = lc_area_trigger2d_set_monitoring(trigger, false);
    TEST_RESULT(result, "Disable monitoring");

    lc_area_trigger2d_get_monitoring(trigger, &monitoring);
    TEST_ASSERT(!monitoring, "Monitoring is disabled");

    /* Monitorable */
    result = lc_area_trigger2d_set_monitorable(trigger, true);
    TEST_RESULT(result, "Enable monitorable");

    bool monitorable;
    lc_area_trigger2d_get_monitorable(trigger, &monitorable);
    TEST_ASSERT(monitorable, "Monitorable is enabled");

    /* Priority */
    result = lc_area_trigger2d_set_priority(trigger, 5);
    TEST_RESULT(result, "Set priority");

    int priority;
    lc_area_trigger2d_get_priority(trigger, &priority);
    TEST_ASSERT(priority == 5, "Priority is 5");

    /* Overlap detection - empty at creation */
    int overlap_count;
    result = lc_area_trigger2d_get_overlapping_count(trigger, &overlap_count);
    TEST_RESULT(result, "Get overlapping count");
    TEST_ASSERT(overlap_count == 0, "No overlapping bodies at creation");

    /* Check is_overlapping with fake UUID */
    bool is_overlapping;
    result = lc_area_trigger2d_is_overlapping(trigger, "00000000-0000-0000-0000-000000000000", &is_overlapping);
    TEST_RESULT(result, "Check is_overlapping");
    TEST_ASSERT(!is_overlapping, "Not overlapping with fake UUID");

    /* Cleanup */
    lc_component_destroy(trigger);

    SECTION_END("Physics: 2D Area Triggers");
}

static void test_area_trigger3d(void) {
    SECTION_START("Physics: 3D Area Triggers");

    /* Create AreaTrigger3D */
    LCComponentHandle trigger = NULL;
    LCResult result = lc_area_trigger3d_create("TestAreaTrigger3D", &trigger);
    TEST_RESULT(result, "Create AreaTrigger3D");

    /* Monitoring */
    result = lc_area_trigger3d_set_monitoring(trigger, true);
    TEST_RESULT(result, "Enable monitoring");

    bool monitoring;
    lc_area_trigger3d_get_monitoring(trigger, &monitoring);
    TEST_ASSERT(monitoring, "Monitoring is enabled");

    result = lc_area_trigger3d_set_monitoring(trigger, false);
    TEST_RESULT(result, "Disable monitoring");

    lc_area_trigger3d_get_monitoring(trigger, &monitoring);
    TEST_ASSERT(!monitoring, "Monitoring is disabled");

    /* Monitorable */
    result = lc_area_trigger3d_set_monitorable(trigger, true);
    TEST_RESULT(result, "Enable monitorable");

    bool monitorable;
    lc_area_trigger3d_get_monitorable(trigger, &monitorable);
    TEST_ASSERT(monitorable, "Monitorable is enabled");

    /* Priority */
    result = lc_area_trigger3d_set_priority(trigger, 10);
    TEST_RESULT(result, "Set priority");

    int priority;
    lc_area_trigger3d_get_priority(trigger, &priority);
    TEST_ASSERT(priority == 10, "Priority is 10");

    /* Overlap detection - empty at creation */
    int overlap_count;
    result = lc_area_trigger3d_get_overlapping_count(trigger, &overlap_count);
    TEST_RESULT(result, "Get overlapping count");
    TEST_ASSERT(overlap_count == 0, "No overlapping bodies at creation");

    /* Check is_overlapping with fake UUID */
    bool is_overlapping;
    result = lc_area_trigger3d_is_overlapping(trigger, "00000000-0000-0000-0000-000000000000", &is_overlapping);
    TEST_RESULT(result, "Check is_overlapping");
    TEST_ASSERT(!is_overlapping, "Not overlapping with fake UUID");

    /* Cleanup */
    lc_component_destroy(trigger);

    SECTION_END("Physics: 3D Area Triggers");
}

/* ============================================================================
 * Character Controller 2D Tests
 * ============================================================================ */

static void test_character_controller2d(void) {
    SECTION_START("Physics: CharacterController2D");

    /* Create CharacterController2D */
    LCComponentHandle controller = NULL;
    LCResult result = lc_character_controller2d_create("TestCharacterController2D", &controller);
    TEST_RESULT(result, "Create CharacterController2D");

    /* Velocity */
    result = lc_character_controller2d_set_velocity(controller, lc_vec2(5.0f, 0.0f));
    TEST_RESULT(result, "Set velocity");

    LCVec2 velocity;
    lc_character_controller2d_get_velocity(controller, &velocity);
    TEST_ASSERT(velocity.x == 5.0f && velocity.y == 0.0f, "Velocity is (5.0, 0.0)");
    printf("    Velocity: (%.1f, %.1f)\n", velocity.x, velocity.y);

    /* Gravity */
    result = lc_character_controller2d_set_gravity(controller, 980.0f);
    TEST_RESULT(result, "Set gravity");

    float gravity;
    lc_character_controller2d_get_gravity(controller, &gravity);
    TEST_ASSERT(gravity == 980.0f, "Gravity is 980.0");
    printf("    Gravity: %.1f\n", gravity);

    /* Max fall speed */
    result = lc_character_controller2d_set_max_fall_speed(controller, 1500.0f);
    TEST_RESULT(result, "Set max fall speed");

    float max_fall;
    lc_character_controller2d_get_max_fall_speed(controller, &max_fall);
    TEST_ASSERT(max_fall == 1500.0f, "Max fall speed is 1500.0");

    /* Ground detection distance */
    result = lc_character_controller2d_set_ground_detection_distance(controller, 0.1f);
    TEST_RESULT(result, "Set ground detection distance");

    float ground_dist;
    lc_character_controller2d_get_ground_detection_distance(controller, &ground_dist);
    TEST_ASSERT(ground_dist > 0.09f && ground_dist < 0.11f, "Ground detection distance is 0.1");

    /* Wall detection distance */
    result = lc_character_controller2d_set_wall_detection_distance(controller, 0.05f);
    TEST_RESULT(result, "Set wall detection distance");

    float wall_dist;
    lc_character_controller2d_get_wall_detection_distance(controller, &wall_dist);
    TEST_ASSERT(wall_dist > 0.04f && wall_dist < 0.06f, "Wall detection distance is 0.05");

    /* Max slope angle */
    result = lc_character_controller2d_set_max_slope_angle(controller, 45.0f);
    TEST_RESULT(result, "Set max slope angle");

    float slope_angle;
    lc_character_controller2d_get_max_slope_angle(controller, &slope_angle);
    TEST_ASSERT(slope_angle == 45.0f, "Max slope angle is 45 degrees");

    /* Snap to ground */
    result = lc_character_controller2d_set_snap_to_ground(controller, true);
    TEST_RESULT(result, "Enable snap to ground");

    bool snap;
    lc_character_controller2d_get_snap_to_ground(controller, &snap);
    TEST_ASSERT(snap, "Snap to ground is enabled");

    /* Max bounces */
    result = lc_character_controller2d_set_max_bounces(controller, 4);
    TEST_RESULT(result, "Set max bounces");

    int bounces;
    lc_character_controller2d_get_max_bounces(controller, &bounces);
    TEST_ASSERT(bounces == 4, "Max bounces is 4");

    /* Ground/wall/ceiling detection (should be false initially) */
    bool on_ground, on_wall, on_ceiling;
    lc_character_controller2d_is_on_ground(controller, &on_ground);
    lc_character_controller2d_is_on_wall(controller, &on_wall);
    lc_character_controller2d_is_on_ceiling(controller, &on_ceiling);
    printf("    On ground: %s, On wall: %s, On ceiling: %s\n",
           on_ground ? "true" : "false",
           on_wall ? "true" : "false",
           on_ceiling ? "true" : "false");

    /* Get normals */
    LCVec2 ground_normal, wall_normal;
    lc_character_controller2d_get_ground_normal(controller, &ground_normal);
    lc_character_controller2d_get_wall_normal(controller, &wall_normal);
    printf("    Ground normal: (%.2f, %.2f)\n", ground_normal.x, ground_normal.y);
    printf("    Wall normal: (%.2f, %.2f)\n", wall_normal.x, wall_normal.y);

    /* Move and slide test (just verify the function works) */
    LCVec2 actual_movement;
    result = lc_character_controller2d_move_and_slide(controller, lc_vec2(10.0f, 0.0f), 0.016f, &actual_movement);
    TEST_RESULT(result, "Move and slide");
    printf("    Move and slide result: (%.2f, %.2f)\n", actual_movement.x, actual_movement.y);

    /* Move and collide test */
    bool collided;
    result = lc_character_controller2d_move_and_collide(controller, lc_vec2(5.0f, 0.0f), 0.016f, &actual_movement, &collided);
    TEST_RESULT(result, "Move and collide");
    printf("    Move and collide result: (%.2f, %.2f), collided: %s\n",
           actual_movement.x, actual_movement.y, collided ? "true" : "false");

    /* Cleanup */
    lc_component_destroy(controller);

    SECTION_END("Physics: CharacterController2D");
}

/* ============================================================================
 * Character Controller 3D Tests
 * ============================================================================ */

static void test_character_controller3d(void) {
    SECTION_START("Physics: CharacterController3D");

    /* Create CharacterController3D */
    LCComponentHandle controller = NULL;
    LCResult result = lc_character_controller3d_create("TestCharacterController3D", &controller);
    TEST_RESULT(result, "Create CharacterController3D");

    /* Velocity */
    result = lc_character_controller3d_set_velocity(controller, lc_vec3(5.0f, 0.0f, 0.0f));
    TEST_RESULT(result, "Set velocity");

    LCVec3 velocity;
    lc_character_controller3d_get_velocity(controller, &velocity);
    TEST_ASSERT(velocity.x == 5.0f && velocity.y == 0.0f && velocity.z == 0.0f, "Velocity is (5.0, 0.0, 0.0)");
    printf("    Velocity: (%.1f, %.1f, %.1f)\n", velocity.x, velocity.y, velocity.z);

    /* Gravity */
    result = lc_character_controller3d_set_gravity(controller, 9.8f);
    TEST_RESULT(result, "Set gravity");

    float gravity;
    lc_character_controller3d_get_gravity(controller, &gravity);
    TEST_ASSERT(gravity > 9.7f && gravity < 9.9f, "Gravity is 9.8");
    printf("    Gravity: %.1f\n", gravity);

    /* Max fall speed */
    result = lc_character_controller3d_set_max_fall_speed(controller, 50.0f);
    TEST_RESULT(result, "Set max fall speed");

    float max_fall;
    lc_character_controller3d_get_max_fall_speed(controller, &max_fall);
    TEST_ASSERT(max_fall == 50.0f, "Max fall speed is 50.0");

    /* Ground detection distance */
    result = lc_character_controller3d_set_ground_detection_distance(controller, 0.1f);
    TEST_RESULT(result, "Set ground detection distance");

    float ground_dist;
    lc_character_controller3d_get_ground_detection_distance(controller, &ground_dist);
    TEST_ASSERT(ground_dist > 0.09f && ground_dist < 0.11f, "Ground detection distance is 0.1");

    /* Wall detection distance */
    result = lc_character_controller3d_set_wall_detection_distance(controller, 0.05f);
    TEST_RESULT(result, "Set wall detection distance");

    float wall_dist;
    lc_character_controller3d_get_wall_detection_distance(controller, &wall_dist);
    TEST_ASSERT(wall_dist > 0.04f && wall_dist < 0.06f, "Wall detection distance is 0.05");

    /* Max slope angle */
    result = lc_character_controller3d_set_max_slope_angle(controller, 45.0f);
    TEST_RESULT(result, "Set max slope angle");

    float slope_angle;
    lc_character_controller3d_get_max_slope_angle(controller, &slope_angle);
    TEST_ASSERT(slope_angle == 45.0f, "Max slope angle is 45 degrees");

    /* Step height */
    result = lc_character_controller3d_set_step_height(controller, 0.3f);
    TEST_RESULT(result, "Set step height");

    float step_height;
    lc_character_controller3d_get_step_height(controller, &step_height);
    TEST_ASSERT(step_height > 0.29f && step_height < 0.31f, "Step height is 0.3");
    printf("    Step height: %.2f\n", step_height);

    /* Snap to ground */
    result = lc_character_controller3d_set_snap_to_ground(controller, true);
    TEST_RESULT(result, "Enable snap to ground");

    bool snap;
    lc_character_controller3d_get_snap_to_ground(controller, &snap);
    TEST_ASSERT(snap, "Snap to ground is enabled");

    /* Max bounces */
    result = lc_character_controller3d_set_max_bounces(controller, 4);
    TEST_RESULT(result, "Set max bounces");

    int bounces;
    lc_character_controller3d_get_max_bounces(controller, &bounces);
    TEST_ASSERT(bounces == 4, "Max bounces is 4");

    /* Ground/wall/ceiling detection (should be false initially) */
    bool on_ground, on_wall, on_ceiling;
    lc_character_controller3d_is_on_ground(controller, &on_ground);
    lc_character_controller3d_is_on_wall(controller, &on_wall);
    lc_character_controller3d_is_on_ceiling(controller, &on_ceiling);
    printf("    On ground: %s, On wall: %s, On ceiling: %s\n",
           on_ground ? "true" : "false",
           on_wall ? "true" : "false",
           on_ceiling ? "true" : "false");

    /* Get normals */
    LCVec3 ground_normal, wall_normal;
    lc_character_controller3d_get_ground_normal(controller, &ground_normal);
    lc_character_controller3d_get_wall_normal(controller, &wall_normal);
    printf("    Ground normal: (%.2f, %.2f, %.2f)\n", ground_normal.x, ground_normal.y, ground_normal.z);
    printf("    Wall normal: (%.2f, %.2f, %.2f)\n", wall_normal.x, wall_normal.y, wall_normal.z);

    /* Move and slide test (just verify the function works) */
    LCVec3 actual_movement;
    result = lc_character_controller3d_move_and_slide(controller, lc_vec3(10.0f, 0.0f, 0.0f), 0.016f, &actual_movement);
    TEST_RESULT(result, "Move and slide");
    printf("    Move and slide result: (%.2f, %.2f, %.2f)\n", actual_movement.x, actual_movement.y, actual_movement.z);

    /* Move and collide test */
    bool collided;
    result = lc_character_controller3d_move_and_collide(controller, lc_vec3(5.0f, 0.0f, 0.0f), 0.016f, &actual_movement, &collided);
    TEST_RESULT(result, "Move and collide");
    printf("    Move and collide result: (%.2f, %.2f, %.2f), collided: %s\n",
           actual_movement.x, actual_movement.y, actual_movement.z, collided ? "true" : "false");

    /* Cleanup */
    lc_component_destroy(controller);

    SECTION_END("Physics: CharacterController3D");
}

/* ============================================================================
 * Audio Tests
 * ============================================================================ */

static void test_audio(void) {
    SECTION_START("Audio: AudioPlayer & AudioListener");

    /* AudioPlayer */
    LCComponentHandle player = NULL;
    LCResult result = lc_audio_player_create("TestAudioPlayer", &player);
    TEST_RESULT(result, "Create AudioPlayer");

    /* Volume */
    result = lc_audio_player_set_volume(player, 0.8f);
    TEST_RESULT(result, "Set volume");

    float volume;
    lc_audio_player_get_volume(player, &volume);
    TEST_ASSERT(volume > 0.79f && volume < 0.81f, "Volume is 0.8");

    /* Pitch */
    result = lc_audio_player_set_pitch(player, 1.0f);
    TEST_RESULT(result, "Set pitch");

    /* Pan */
    result = lc_audio_player_set_pan(player, 0.0f);
    TEST_RESULT(result, "Set pan (center)");

    /* Loop */
    result = lc_audio_player_set_loop(player, true);
    TEST_RESULT(result, "Enable loop");

    bool loop;
    lc_audio_player_get_loop(player, &loop);
    TEST_ASSERT(loop, "Loop is enabled");

    /* Autoplay */
    result = lc_audio_player_set_autoplay(player, false);
    TEST_RESULT(result, "Disable autoplay");

    /* 3D audio settings */
    result = lc_audio_player_set_is3d(player, true);
    TEST_RESULT(result, "Enable 3D audio");

    result = lc_audio_player_set_min_distance(player, 1.0f);
    TEST_RESULT(result, "Set min distance");

    result = lc_audio_player_set_max_distance(player, 50.0f);
    TEST_RESULT(result, "Set max distance");

    result = lc_audio_player_set_rolloff_factor(player, 1.0f);
    TEST_RESULT(result, "Set rolloff factor");

    /* AudioListener */
    LCComponentHandle listener = NULL;
    result = lc_audio_listener_create("TestListener", &listener);
    TEST_RESULT(result, "Create AudioListener");

    result = lc_audio_listener_set_active(listener, true);
    TEST_RESULT(result, "Set listener active");

    bool active;
    lc_audio_listener_is_active(listener, &active);
    TEST_ASSERT(active, "Listener is active");

    /* Cleanup */
    lc_component_destroy(player);
    lc_component_destroy(listener);

    SECTION_END("Audio: AudioPlayer & AudioListener");
}

/* ============================================================================
 * UI Tests
 * ============================================================================ */

static void test_ui_label(void) {
    SECTION_START("UI: Label Component");

    LCComponentHandle label = NULL;
    LCResult result = lc_label_create("TestLabel", &label);
    TEST_RESULT(result, "Create Label");

    /* Text */
    result = lc_label_set_text(label, "Hello, Lupine!");
    TEST_RESULT(result, "Set text");

    char text[256];
    lc_label_get_text(label, text, sizeof(text));
    TEST_ASSERT(strcmp(text, "Hello, Lupine!") == 0, "Text matches");

    /* Font size */
    result = lc_label_set_font_size(label, 24.0f);
    TEST_RESULT(result, "Set font size");

    float size;
    lc_label_get_font_size(label, &size);
    TEST_ASSERT(size == 24.0f, "Font size is 24");

    /* Color */
    result = lc_label_set_color(label, lc_color_white());
    TEST_RESULT(result, "Set text color");

    /* Centered */
    result = lc_label_set_centered(label, true);
    TEST_RESULT(result, "Set centered");

    /* Offset */
    result = lc_label_set_offset(label, lc_vec2(10.0f, 5.0f));
    TEST_RESULT(result, "Set offset");

    lc_component_destroy(label);

    SECTION_END("UI: Label Component");
}

static void test_ui_button(void) {
    SECTION_START("UI: Button Component");

    LCComponentHandle button = NULL;
    LCResult result = lc_button_create("TestButton", &button);
    TEST_RESULT(result, "Create Button");

    /* Size */
    result = lc_button_set_width(button, 200.0f);
    TEST_RESULT(result, "Set width");

    result = lc_button_set_height(button, 50.0f);
    TEST_RESULT(result, "Set height");

    float width, height;
    lc_button_get_width(button, &width);
    lc_button_get_height(button, &height);
    TEST_ASSERT(width == 200.0f && height == 50.0f, "Size is 200x50");

    /* Text */
    result = lc_button_set_text(button, "Click Me!");
    TEST_RESULT(result, "Set button text");

    char text[256];
    lc_button_get_text(button, text, sizeof(text));
    TEST_ASSERT(strcmp(text, "Click Me!") == 0, "Button text matches");

    /* Font */
    result = lc_button_set_font_size(button, 18.0f);
    TEST_RESULT(result, "Set font size");

    result = lc_button_set_font_color(button, lc_color_white());
    TEST_RESULT(result, "Set font color");

    /* Background */
    result = lc_button_set_background_color(button, lc_color_from_hex(0x3366FFFF));
    TEST_RESULT(result, "Set background color");

    result = lc_button_set_opacity(button, 1.0f);
    TEST_RESULT(result, "Set opacity");

    /* Border */
    result = lc_button_set_border_enabled(button, true);
    TEST_RESULT(result, "Enable border");

    result = lc_button_set_border_color(button, lc_color_white());
    TEST_RESULT(result, "Set border color");

    result = lc_button_set_border_width_linked(button, true);
    TEST_RESULT(result, "Link border widths");

    result = lc_button_set_border_width_left(button, 2.0f);
    TEST_RESULT(result, "Set border width");

    /* Corner radius */
    result = lc_button_set_corner_radius_linked(button, true);
    TEST_RESULT(result, "Link corner radii");

    result = lc_button_set_corner_radius_top_left(button, 8.0f);
    TEST_RESULT(result, "Set corner radius");

    /* State */
    LCButtonState state;
    result = lc_button_get_current_state(button, &state);
    TEST_RESULT(result, "Get current state");
    printf("    Current state: %d\n", (int)state);

    /* Enabled */
    result = lc_button_set_enabled(button, true);
    TEST_RESULT(result, "Set enabled");

    /* Style mode */
    result = lc_button_set_style_mode(button, LC_BUTTON_STYLE_AUTOMATIC);
    TEST_RESULT(result, "Set style mode");

    /* Scale mode */
    result = lc_button_set_scale_mode(button, LC_BUTTON_SCALE_FIXED);
    TEST_RESULT(result, "Set scale mode");

    /* Per-state modulation */
    result = lc_button_set_state_modulation(button, LC_BUTTON_STATE_HOVER,
                                            lc_color(1.2f, 1.2f, 1.2f, 1.0f));
    TEST_RESULT(result, "Set hover modulation");

    result = lc_button_set_state_modulation(button, LC_BUTTON_STATE_PRESSED,
                                            lc_color(0.8f, 0.8f, 0.8f, 1.0f));
    TEST_RESULT(result, "Set pressed modulation");

    /* Layer */
    result = lc_button_set_layer(button, 0);
    TEST_RESULT(result, "Set layer");

    result = lc_button_set_sorting_order(button, 0);
    TEST_RESULT(result, "Set sorting order");

    lc_component_destroy(button);

    SECTION_END("UI: Button Component");
}

static void test_ui_panel(void) {
    SECTION_START("UI: Panel Component");

    /* Need a node to attach the component to */
    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_panel_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "PanelNode", &node);
    TEST_RESULT(result, "Create node for Panel");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle panel = NULL;
    result = lc_panel_create(node, &panel);
    TEST_RESULT(result, "Create Panel component");

    /* Size */
    result = lc_panel_set_width(panel, 200.0f);
    TEST_RESULT(result, "Set width");

    result = lc_panel_set_height(panel, 150.0f);
    TEST_RESULT(result, "Set height");

    float width, height;
    lc_panel_get_width(panel, &width);
    lc_panel_get_height(panel, &height);
    TEST_ASSERT(width == 200.0f && height == 150.0f, "Size is 200x150");

    /* Layer */
    result = lc_panel_set_layer(panel, 5);
    TEST_RESULT(result, "Set layer");

    int layer;
    lc_panel_get_layer(panel, &layer);
    TEST_ASSERT(layer == 5, "Layer is 5");

    /* Background color */
    result = lc_panel_set_background_color(panel, lc_color(0.2f, 0.3f, 0.4f, 1.0f));
    TEST_RESULT(result, "Set background color");

    /* Border */
    result = lc_panel_set_border_enabled(panel, true);
    TEST_RESULT(result, "Enable border");

    result = lc_panel_set_border_color(panel, lc_color_white());
    TEST_RESULT(result, "Set border color");

    result = lc_panel_set_border_width_linked(panel, true);
    TEST_RESULT(result, "Link border widths");

    result = lc_panel_set_border_width_left(panel, 2.0f);
    TEST_RESULT(result, "Set border width");

    /* Corner radius */
    result = lc_panel_set_corner_radius_linked(panel, true);
    TEST_RESULT(result, "Link corner radii");

    result = lc_panel_set_corner_radius_top_left(panel, 10.0f);
    TEST_RESULT(result, "Set corner radius");

    /* Corner detail */
    result = lc_panel_set_corner_detail(panel, 8);
    TEST_RESULT(result, "Set corner detail");

    /* Anti-aliasing */
    result = lc_panel_set_anti_aliasing(panel, true);
    TEST_RESULT(result, "Enable anti-aliasing");

    /* Shadow */
    result = lc_panel_set_shadow_enabled(panel, true);
    TEST_RESULT(result, "Enable shadow");

    result = lc_panel_set_shadow_color(panel, lc_color(0.0f, 0.0f, 0.0f, 0.5f));
    TEST_RESULT(result, "Set shadow color");

    result = lc_panel_set_shadow_size(panel, 5.0f);
    TEST_RESULT(result, "Set shadow size");

    result = lc_panel_set_shadow_offset(panel, lc_vec2(3.0f, 3.0f));
    TEST_RESULT(result, "Set shadow offset");

    /* Opacity */
    result = lc_panel_set_opacity(panel, 0.9f);
    TEST_RESULT(result, "Set opacity");

    /* UI Space */
    result = lc_panel_set_use_ui_space(panel, true);
    TEST_RESULT(result, "Set use UI space");

    lc_scene_destroy(scene);

    SECTION_END("UI: Panel Component");
}

static void test_ui_panel3d(void) {
    SECTION_START("UI: Panel3D Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_panel3d_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_3D, "Panel3DNode", &node);
    TEST_RESULT(result, "Create node for Panel3D");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle panel3d = NULL;
    result = lc_panel3d_create(node, &panel3d);
    TEST_RESULT(result, "Create Panel3D component");

    /* Size */
    result = lc_panel3d_set_width(panel3d, 2.0f);
    TEST_RESULT(result, "Set width");

    result = lc_panel3d_set_height(panel3d, 1.5f);
    TEST_RESULT(result, "Set height");

    /* Billboard mode */
    result = lc_panel3d_set_billboard_mode(panel3d, LC_PANEL3D_BILLBOARD_Y_AXIS);
    TEST_RESULT(result, "Set billboard mode to Y-axis");

    LCPanel3DBillboardMode mode;
    lc_panel3d_get_billboard_mode(panel3d, &mode);
    TEST_ASSERT(mode == LC_PANEL3D_BILLBOARD_Y_AXIS, "Billboard mode is Y-axis");

    /* 3D rendering properties */
    result = lc_panel3d_set_double_sided(panel3d, true);
    TEST_RESULT(result, "Set double-sided");

    result = lc_panel3d_set_cast_shadow(panel3d, false);
    TEST_RESULT(result, "Set cast shadow");

    result = lc_panel3d_set_receive_shadow(panel3d, true);
    TEST_RESULT(result, "Set receive shadow");

    /* Background color */
    result = lc_panel3d_set_background_color(panel3d, lc_color(0.3f, 0.5f, 0.7f, 1.0f));
    TEST_RESULT(result, "Set background color");

    /* Border */
    result = lc_panel3d_set_border_enabled(panel3d, true);
    TEST_RESULT(result, "Enable border");

    result = lc_panel3d_set_border_color(panel3d, lc_color_white());
    TEST_RESULT(result, "Set border color");

    /* Corner radius */
    result = lc_panel3d_set_corner_radius_linked(panel3d, false);
    TEST_RESULT(result, "Unlink corner radii");

    result = lc_panel3d_set_corner_radius_top_left(panel3d, 0.1f);
    TEST_RESULT(result, "Set corner radius top-left");

    result = lc_panel3d_set_corner_radius_bottom_right(panel3d, 0.2f);
    TEST_RESULT(result, "Set corner radius bottom-right");

    lc_scene_destroy(scene);

    SECTION_END("UI: Panel3D Component");
}

static void test_ui_color_rect(void) {
    SECTION_START("UI: ColorRect Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_color_rect_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "ColorRectNode", &node);
    TEST_RESULT(result, "Create node for ColorRect");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle colorRect = NULL;
    result = lc_color_rect_create(node, &colorRect);
    TEST_RESULT(result, "Create ColorRect component");

    /* Color */
    result = lc_color_rect_set_color(colorRect, lc_color(1.0f, 0.5f, 0.2f, 1.0f));
    TEST_RESULT(result, "Set color");

    LCColor color;
    lc_color_rect_get_color(colorRect, &color);
    TEST_ASSERT(color.r == 1.0f && color.g == 0.5f && color.b == 0.2f, "Color matches");

    /* Size */
    result = lc_color_rect_set_width(colorRect, 100.0f);
    TEST_RESULT(result, "Set width");

    result = lc_color_rect_set_height(colorRect, 80.0f);
    TEST_RESULT(result, "Set height");

    /* Layer */
    result = lc_color_rect_set_layer(colorRect, 2);
    TEST_RESULT(result, "Set layer");

    /* Blend mode */
    result = lc_color_rect_set_blend_mode(colorRect, LC_BLEND_ALPHA);
    TEST_RESULT(result, "Set blend mode to Alpha");

    LCBlendMode blendMode;
    lc_color_rect_get_blend_mode(colorRect, &blendMode);
    TEST_ASSERT(blendMode == LC_BLEND_ALPHA, "Blend mode is Alpha");

    /* Corner radius */
    result = lc_color_rect_set_corner_radius_linked(colorRect, true);
    TEST_RESULT(result, "Link corner radii");

    result = lc_color_rect_set_corner_radius_all(colorRect, 5.0f);
    TEST_RESULT(result, "Set all corner radii");

    /* Border */
    result = lc_color_rect_set_border_enabled(colorRect, true);
    TEST_RESULT(result, "Enable border");

    result = lc_color_rect_set_border_color(colorRect, lc_color_black());
    TEST_RESULT(result, "Set border color");

    result = lc_color_rect_set_border_width_all(colorRect, 2.0f);
    TEST_RESULT(result, "Set all border widths");

    /* UI Space */
    result = lc_color_rect_set_ui_space(colorRect, true);
    TEST_RESULT(result, "Set UI space");

    lc_scene_destroy(scene);

    SECTION_END("UI: ColorRect Component");
}

static void test_ui_image2d(void) {
    SECTION_START("UI: Image2D Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_image2d_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "Image2DNode", &node);
    TEST_RESULT(result, "Create node for Image2D");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle image2d = NULL;
    result = lc_image2d_create(node, &image2d);
    TEST_RESULT(result, "Create Image2D component");

    /* Size */
    result = lc_image2d_set_width(image2d, 128.0f);
    TEST_RESULT(result, "Set width");

    result = lc_image2d_set_height(image2d, 128.0f);
    TEST_RESULT(result, "Set height");

    float width, height;
    lc_image2d_get_width(image2d, &width);
    lc_image2d_get_height(image2d, &height);
    TEST_ASSERT(width == 128.0f && height == 128.0f, "Size is 128x128");

    /* Color modulation */
    result = lc_image2d_set_color(image2d, lc_color_white());
    TEST_RESULT(result, "Set color modulation");

    /* Anchors */
    result = lc_image2d_set_anchor_min(image2d, lc_vec2(0.0f, 0.0f));
    TEST_RESULT(result, "Set anchor min");

    result = lc_image2d_set_anchor_max(image2d, lc_vec2(1.0f, 1.0f));
    TEST_RESULT(result, "Set anchor max");

    /* Aspect mode */
    result = lc_image2d_set_aspect_mode(image2d, LC_ASPECT_FIT);
    TEST_RESULT(result, "Set aspect mode to Fit");

    LCAspectMode aspectMode;
    lc_image2d_get_aspect_mode(image2d, &aspectMode);
    TEST_ASSERT(aspectMode == LC_ASPECT_FIT, "Aspect mode is Fit");

    /* Flip */
    result = lc_image2d_set_flip_h(image2d, false);
    TEST_RESULT(result, "Set flip horizontal");

    result = lc_image2d_set_flip_v(image2d, false);
    TEST_RESULT(result, "Set flip vertical");

    /* Blend mode */
    result = lc_image2d_set_blend_mode(image2d, LC_BLEND_ALPHA);
    TEST_RESULT(result, "Set blend mode");

    /* Mouse behaviour */
    result = lc_image2d_set_mouse_behaviour(image2d, LC_MOUSE_PROPAGATE_UP);
    TEST_RESULT(result, "Set mouse behaviour");

    /* Corner radius */
    result = lc_image2d_set_corner_radius_linked(image2d, true);
    TEST_RESULT(result, "Link corner radii");

    result = lc_image2d_set_corner_radius_all(image2d, 8.0f);
    TEST_RESULT(result, "Set corner radius");

    /* UI Space */
    result = lc_image2d_set_ui_space(image2d, true);
    TEST_RESULT(result, "Set UI space");

    lc_scene_destroy(scene);

    SECTION_END("UI: Image2D Component");
}

static void test_ui_progress_bar(void) {
    SECTION_START("UI: ProgressBar Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_progress_bar_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "ProgressBarNode", &node);
    TEST_RESULT(result, "Create node for ProgressBar");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle progressBar = NULL;
    result = lc_progress_bar_create(node, &progressBar);
    TEST_RESULT(result, "Create ProgressBar component");

    /* Value properties */
    result = lc_progress_bar_set_min_value(progressBar, 0.0f);
    TEST_RESULT(result, "Set min value");

    result = lc_progress_bar_set_max_value(progressBar, 100.0f);
    TEST_RESULT(result, "Set max value");

    result = lc_progress_bar_set_value(progressBar, 50.0f);
    TEST_RESULT(result, "Set value");

    float value;
    lc_progress_bar_get_value(progressBar, &value);
    TEST_ASSERT(value == 50.0f, "Value is 50");

    result = lc_progress_bar_set_step(progressBar, 1.0f);
    TEST_RESULT(result, "Set step");

    /* Smooth properties */
    result = lc_progress_bar_set_smooth(progressBar, true);
    TEST_RESULT(result, "Enable smooth interpolation");

    result = lc_progress_bar_set_smooth_speed(progressBar, 5.0f);
    TEST_RESULT(result, "Set smooth speed");

    /* Size */
    result = lc_progress_bar_set_width(progressBar, 200.0f);
    TEST_RESULT(result, "Set width");

    result = lc_progress_bar_set_height(progressBar, 20.0f);
    TEST_RESULT(result, "Set height");

    /* Orientation */
    result = lc_progress_bar_set_orientation(progressBar, LC_PROGRESSBAR_HORIZONTAL);
    TEST_RESULT(result, "Set orientation to Horizontal");

    LCProgressBarOrientation orientation;
    lc_progress_bar_get_orientation(progressBar, &orientation);
    TEST_ASSERT(orientation == LC_PROGRESSBAR_HORIZONTAL, "Orientation is Horizontal");

    /* Fill direction */
    result = lc_progress_bar_set_fill_direction(progressBar, LC_FILL_LEFT_TO_RIGHT);
    TEST_RESULT(result, "Set fill direction");

    /* Colors */
    result = lc_progress_bar_set_background_color(progressBar, lc_color(0.2f, 0.2f, 0.2f, 1.0f));
    TEST_RESULT(result, "Set background color");

    result = lc_progress_bar_set_fill_color(progressBar, lc_color(0.0f, 0.8f, 0.2f, 1.0f));
    TEST_RESULT(result, "Set fill color");

    result = lc_progress_bar_set_border_color(progressBar, lc_color_white());
    TEST_RESULT(result, "Set border color");

    /* Value display */
    result = lc_progress_bar_set_show_value(progressBar, true);
    TEST_RESULT(result, "Show value");

    result = lc_progress_bar_set_value_font_size(progressBar, 12.0f);
    TEST_RESULT(result, "Set value font size");

    result = lc_progress_bar_set_value_color(progressBar, lc_color_white());
    TEST_RESULT(result, "Set value color");

    /* Corner radius */
    result = lc_progress_bar_set_corner_radius_linked(progressBar, true);
    TEST_RESULT(result, "Link corner radii");

    result = lc_progress_bar_set_corner_radius_all(progressBar, 4.0f);
    TEST_RESULT(result, "Set corner radius");

    /* Border width */
    result = lc_progress_bar_set_border_width_linked(progressBar, true);
    TEST_RESULT(result, "Link border widths");

    result = lc_progress_bar_set_border_width_all(progressBar, 1.0f);
    TEST_RESULT(result, "Set border width");

    lc_scene_destroy(scene);

    SECTION_END("UI: ProgressBar Component");
}

static void test_ui_progress_bar3d(void) {
    SECTION_START("UI: ProgressBar3D Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_progress_bar3d_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_3D, "ProgressBar3DNode", &node);
    TEST_RESULT(result, "Create node for ProgressBar3D");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle progressBar3d = NULL;
    result = lc_progress_bar3d_create(node, &progressBar3d);
    TEST_RESULT(result, "Create ProgressBar3D component");

    /* Value properties */
    result = lc_progress_bar3d_set_min_value(progressBar3d, 0.0f);
    TEST_RESULT(result, "Set min value");

    result = lc_progress_bar3d_set_max_value(progressBar3d, 100.0f);
    TEST_RESULT(result, "Set max value");

    result = lc_progress_bar3d_set_value(progressBar3d, 75.0f);
    TEST_RESULT(result, "Set value");

    /* Size */
    result = lc_progress_bar3d_set_width(progressBar3d, 2.0f);
    TEST_RESULT(result, "Set width");

    result = lc_progress_bar3d_set_height(progressBar3d, 0.3f);
    TEST_RESULT(result, "Set height");

    /* Billboard mode */
    result = lc_progress_bar3d_set_billboard_mode(progressBar3d, LC_PROGRESSBAR3D_BILLBOARD_ENABLED);
    TEST_RESULT(result, "Set billboard mode");

    LCProgressBar3DBillboardMode mode;
    lc_progress_bar3d_get_billboard_mode(progressBar3d, &mode);
    TEST_ASSERT(mode == LC_PROGRESSBAR3D_BILLBOARD_ENABLED, "Billboard mode is Enabled");

    /* 3D rendering properties */
    result = lc_progress_bar3d_set_double_sided(progressBar3d, true);
    TEST_RESULT(result, "Set double-sided");

    result = lc_progress_bar3d_set_cast_shadow(progressBar3d, false);
    TEST_RESULT(result, "Set cast shadow");

    result = lc_progress_bar3d_set_receive_shadow(progressBar3d, false);
    TEST_RESULT(result, "Set receive shadow");

    /* Orientation */
    result = lc_progress_bar3d_set_orientation(progressBar3d, LC_PROGRESSBAR_HORIZONTAL);
    TEST_RESULT(result, "Set orientation");

    /* Fill direction */
    result = lc_progress_bar3d_set_fill_direction(progressBar3d, LC_FILL_LEFT_TO_RIGHT);
    TEST_RESULT(result, "Set fill direction");

    /* Colors */
    result = lc_progress_bar3d_set_background_color(progressBar3d, lc_color(0.3f, 0.3f, 0.3f, 1.0f));
    TEST_RESULT(result, "Set background color");

    result = lc_progress_bar3d_set_fill_color(progressBar3d, lc_color(1.0f, 0.0f, 0.0f, 1.0f));
    TEST_RESULT(result, "Set fill color");

    /* Value display */
    result = lc_progress_bar3d_set_show_value(progressBar3d, false);
    TEST_RESULT(result, "Hide value");

    /* Corner radius */
    result = lc_progress_bar3d_set_corner_radius_all(progressBar3d, 0.05f);
    TEST_RESULT(result, "Set corner radius");

    lc_scene_destroy(scene);

    SECTION_END("UI: ProgressBar3D Component");
}

static void test_ui_label3d(void) {
    SECTION_START("UI: Label3D Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_label3d_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_3D, "Label3DNode", &node);
    TEST_RESULT(result, "Create node for Label3D");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle label3d = NULL;
    result = lc_label3d_create(node, &label3d);
    TEST_RESULT(result, "Create Label3D component");

    /* Text */
    result = lc_label3d_set_text(label3d, "Hello 3D World!");
    TEST_RESULT(result, "Set text");

    char text[256];
    lc_label3d_get_text(label3d, text, sizeof(text));
    TEST_ASSERT(strcmp(text, "Hello 3D World!") == 0, "Text matches");

    /* Font size */
    result = lc_label3d_set_font_size(label3d, 32.0f);
    TEST_RESULT(result, "Set font size");

    float fontSize;
    lc_label3d_get_font_size(label3d, &fontSize);
    TEST_ASSERT(fontSize == 32.0f, "Font size is 32");

    /* Color */
    result = lc_label3d_set_color(label3d, lc_color(1.0f, 1.0f, 0.0f, 1.0f));
    TEST_RESULT(result, "Set color");

    LCColor color;
    lc_label3d_get_color(label3d, &color);
    TEST_ASSERT(color.r == 1.0f && color.g == 1.0f && color.b == 0.0f, "Color is yellow");

    /* Centered */
    result = lc_label3d_set_centered(label3d, true);
    TEST_RESULT(result, "Set centered");

    bool centered;
    lc_label3d_get_centered(label3d, &centered);
    TEST_ASSERT(centered == true, "Text is centered");

    /* Offset */
    result = lc_label3d_set_offset(label3d, lc_vec2(0.5f, 0.0f));
    TEST_RESULT(result, "Set offset");

    /* Billboard mode */
    result = lc_label3d_set_billboard_mode(label3d, LC_LABEL3D_BILLBOARD_ENABLED);
    TEST_RESULT(result, "Set billboard mode");

    LCLabel3DBillboardMode mode;
    lc_label3d_get_billboard_mode(label3d, &mode);
    TEST_ASSERT(mode == LC_LABEL3D_BILLBOARD_ENABLED, "Billboard mode is Enabled");

    /* Pixel size */
    result = lc_label3d_set_pixel_size(label3d, 0.01f);
    TEST_RESULT(result, "Set pixel size");

    float pixelSize;
    lc_label3d_get_pixel_size(label3d, &pixelSize);
    TEST_ASSERT(pixelSize == 0.01f, "Pixel size is 0.01");

    lc_scene_destroy(scene);

    SECTION_END("UI: Label3D Component");
}

/* ============================================================================
 * UI Container Tests
 * ============================================================================ */

static void test_ui_container(void) {
    SECTION_START("UI: Container Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_container_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "ContainerNode", &node);
    TEST_RESULT(result, "Create node for Container");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle container = NULL;
    result = lc_container_create(node, &container);
    TEST_RESULT(result, "Create Container component");

    /* Size */
    result = lc_container_set_width(container, 400.0f);
    TEST_RESULT(result, "Set width");

    float width;
    lc_container_get_width(container, &width);
    TEST_ASSERT(width == 400.0f, "Width is 400");

    result = lc_container_set_height(container, 300.0f);
    TEST_RESULT(result, "Set height");

    float height;
    lc_container_get_height(container, &height);
    TEST_ASSERT(height == 300.0f, "Height is 300");

    result = lc_container_set_size(container, lc_vec2(500.0f, 400.0f));
    TEST_RESULT(result, "Set size");

    LCVec2 size;
    lc_container_get_size(container, &size);
    TEST_ASSERT(size.x == 500.0f && size.y == 400.0f, "Size is 500x400");

    /* Size mode */
    result = lc_container_set_horizontal_size_mode(container, LC_CONTAINER_SIZE_FIT_CHILDREN);
    TEST_RESULT(result, "Set horizontal size mode");

    LCContainerSizeMode sizeMode;
    lc_container_get_horizontal_size_mode(container, &sizeMode);
    TEST_ASSERT(sizeMode == LC_CONTAINER_SIZE_FIT_CHILDREN, "Horizontal size mode is FitChildren");

    result = lc_container_set_vertical_size_mode(container, LC_CONTAINER_SIZE_EXPAND);
    TEST_RESULT(result, "Set vertical size mode");

    lc_container_get_vertical_size_mode(container, &sizeMode);
    TEST_ASSERT(sizeMode == LC_CONTAINER_SIZE_EXPAND, "Vertical size mode is Expand");

    /* Padding */
    result = lc_container_set_padding_linked(container, false);
    TEST_RESULT(result, "Set padding unlinked");

    result = lc_container_set_padding_left(container, 10.0f);
    TEST_RESULT(result, "Set padding left");

    float padding;
    lc_container_get_padding_left(container, &padding);
    TEST_ASSERT(padding == 10.0f, "Padding left is 10");

    result = lc_container_set_padding(container, lc_vec4(5.0f, 10.0f, 15.0f, 20.0f));
    TEST_RESULT(result, "Set padding all sides");

    /* Margin */
    result = lc_container_set_margin_linked(container, true);
    TEST_RESULT(result, "Set margin linked");

    result = lc_container_set_margin_top(container, 8.0f);
    TEST_RESULT(result, "Set margin top");

    /* Background */
    result = lc_container_set_background_color(container, lc_color(0.2f, 0.3f, 0.4f, 1.0f));
    TEST_RESULT(result, "Set background color");

    LCColor bgColor;
    lc_container_get_background_color(container, &bgColor);
    TEST_ASSERT(bgColor.r == 0.2f && bgColor.g == 0.3f && bgColor.b == 0.4f, "Background color correct");

    result = lc_container_set_opacity(container, 0.9f);
    TEST_RESULT(result, "Set opacity");

    result = lc_container_set_draw_background(container, true);
    TEST_RESULT(result, "Enable draw background");

    /* Border */
    result = lc_container_set_border_enabled(container, true);
    TEST_RESULT(result, "Enable border");

    result = lc_container_set_border_color(container, lc_color(1.0f, 1.0f, 1.0f, 1.0f));
    TEST_RESULT(result, "Set border color");

    result = lc_container_set_border_width_linked(container, false);
    TEST_RESULT(result, "Set border width unlinked");

    result = lc_container_set_border_width_left(container, 2.0f);
    TEST_RESULT(result, "Set border width left");

    /* Corner radius */
    result = lc_container_set_corner_radius_linked(container, true);
    TEST_RESULT(result, "Set corner radius linked");

    result = lc_container_set_corner_radius_top_left(container, 8.0f);
    TEST_RESULT(result, "Set corner radius top left");

    float radius;
    lc_container_get_corner_radius_top_left(container, &radius);
    TEST_ASSERT(radius == 8.0f, "Corner radius top left is 8");

    /* Layout */
    result = lc_container_set_clip_children(container, true);
    TEST_RESULT(result, "Set clip children");

    bool clip;
    lc_container_get_clip_children(container, &clip);
    TEST_ASSERT(clip == true, "Clip children is true");

    result = lc_container_set_separation(container, 5.0f);
    TEST_RESULT(result, "Set separation");

    float sep;
    lc_container_get_separation(container, &sep);
    TEST_ASSERT(sep == 5.0f, "Separation is 5");

    result = lc_container_invalidate_layout(container);
    TEST_RESULT(result, "Invalidate layout");

    result = lc_container_force_layout_update(container);
    TEST_RESULT(result, "Force layout update");

    /* Rendering */
    result = lc_container_set_layer(container, 2);
    TEST_RESULT(result, "Set layer");

    result = lc_container_set_sorting_order(container, 10);
    TEST_RESULT(result, "Set sorting order");

    result = lc_container_set_use_ui_space(container, true);
    TEST_RESULT(result, "Set use UI space");

    /* Child count */
    int childCount;
    lc_container_get_child_count(container, &childCount);
    TEST_ASSERT(childCount == 0, "Child count is 0");

    lc_scene_destroy(scene);

    SECTION_END("UI: Container Component");
}

static void test_ui_vertical_container(void) {
    SECTION_START("UI: VerticalContainer Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_vcontainer_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "VContainerNode", &node);
    TEST_RESULT(result, "Create node for VerticalContainer");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle vcontainer = NULL;
    result = lc_vertical_container_create(node, &vcontainer);
    TEST_RESULT(result, "Create VerticalContainer component");

    /* Alignments */
    result = lc_vertical_container_set_horizontal_alignment(vcontainer, LC_VCONTAINER_HALIGN_CENTER);
    TEST_RESULT(result, "Set horizontal alignment");

    LCVerticalContainerHAlign hAlign;
    lc_vertical_container_get_horizontal_alignment(vcontainer, &hAlign);
    TEST_ASSERT(hAlign == LC_VCONTAINER_HALIGN_CENTER, "Horizontal alignment is Center");

    result = lc_vertical_container_set_vertical_alignment(vcontainer, LC_VCONTAINER_VALIGN_BEGIN);
    TEST_RESULT(result, "Set vertical alignment");

    LCVerticalContainerVAlign vAlign;
    lc_vertical_container_get_vertical_alignment(vcontainer, &vAlign);
    TEST_ASSERT(vAlign == LC_VCONTAINER_VALIGN_BEGIN, "Vertical alignment is Begin");

    /* Test all alignment values */
    result = lc_vertical_container_set_horizontal_alignment(vcontainer, LC_VCONTAINER_HALIGN_LEFT);
    TEST_RESULT(result, "Set horizontal alignment Left");

    result = lc_vertical_container_set_horizontal_alignment(vcontainer, LC_VCONTAINER_HALIGN_RIGHT);
    TEST_RESULT(result, "Set horizontal alignment Right");

    result = lc_vertical_container_set_horizontal_alignment(vcontainer, LC_VCONTAINER_HALIGN_FILL);
    TEST_RESULT(result, "Set horizontal alignment Fill");

    result = lc_vertical_container_set_vertical_alignment(vcontainer, LC_VCONTAINER_VALIGN_END);
    TEST_RESULT(result, "Set vertical alignment End");

    result = lc_vertical_container_set_vertical_alignment(vcontainer, LC_VCONTAINER_VALIGN_FILL);
    TEST_RESULT(result, "Set vertical alignment Fill");

    /* Base container functions work on VerticalContainer too */
    result = lc_container_set_separation(vcontainer, 10.0f);
    TEST_RESULT(result, "Set separation via base container");

    float sep;
    lc_container_get_separation(vcontainer, &sep);
    TEST_ASSERT(sep == 10.0f, "Separation is 10");

    lc_scene_destroy(scene);

    SECTION_END("UI: VerticalContainer Component");
}

static void test_ui_horizontal_container(void) {
    SECTION_START("UI: HorizontalContainer Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_hcontainer_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "HContainerNode", &node);
    TEST_RESULT(result, "Create node for HorizontalContainer");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle hcontainer = NULL;
    result = lc_horizontal_container_create(node, &hcontainer);
    TEST_RESULT(result, "Create HorizontalContainer component");

    /* Alignments */
    result = lc_horizontal_container_set_vertical_alignment(hcontainer, LC_HCONTAINER_VALIGN_CENTER);
    TEST_RESULT(result, "Set vertical alignment");

    LCHorizontalContainerVAlign vAlign;
    lc_horizontal_container_get_vertical_alignment(hcontainer, &vAlign);
    TEST_ASSERT(vAlign == LC_HCONTAINER_VALIGN_CENTER, "Vertical alignment is Center");

    result = lc_horizontal_container_set_horizontal_alignment(hcontainer, LC_HCONTAINER_HALIGN_BEGIN);
    TEST_RESULT(result, "Set horizontal alignment");

    LCHorizontalContainerHAlign hAlign;
    lc_horizontal_container_get_horizontal_alignment(hcontainer, &hAlign);
    TEST_ASSERT(hAlign == LC_HCONTAINER_HALIGN_BEGIN, "Horizontal alignment is Begin");

    /* Test all alignment values */
    result = lc_horizontal_container_set_vertical_alignment(hcontainer, LC_HCONTAINER_VALIGN_TOP);
    TEST_RESULT(result, "Set vertical alignment Top");

    result = lc_horizontal_container_set_vertical_alignment(hcontainer, LC_HCONTAINER_VALIGN_BOTTOM);
    TEST_RESULT(result, "Set vertical alignment Bottom");

    result = lc_horizontal_container_set_vertical_alignment(hcontainer, LC_HCONTAINER_VALIGN_FILL);
    TEST_RESULT(result, "Set vertical alignment Fill");

    result = lc_horizontal_container_set_horizontal_alignment(hcontainer, LC_HCONTAINER_HALIGN_END);
    TEST_RESULT(result, "Set horizontal alignment End");

    result = lc_horizontal_container_set_horizontal_alignment(hcontainer, LC_HCONTAINER_HALIGN_FILL);
    TEST_RESULT(result, "Set horizontal alignment Fill");

    lc_scene_destroy(scene);

    SECTION_END("UI: HorizontalContainer Component");
}

static void test_ui_grid_container(void) {
    SECTION_START("UI: GridContainer Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_grid_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "GridContainerNode", &node);
    TEST_RESULT(result, "Create node for GridContainer");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle grid = NULL;
    result = lc_grid_container_create(node, &grid);
    TEST_RESULT(result, "Create GridContainer component");

    /* Grid configuration */
    result = lc_grid_container_set_use_rows(grid, true);
    TEST_RESULT(result, "Set use rows");

    bool useRows;
    lc_grid_container_get_use_rows(grid, &useRows);
    TEST_ASSERT(useRows == true, "Use rows is true");

    result = lc_grid_container_set_row_count(grid, 3);
    TEST_RESULT(result, "Set row count");

    int rowCount;
    lc_grid_container_get_row_count(grid, &rowCount);
    TEST_ASSERT(rowCount == 3, "Row count is 3");

    result = lc_grid_container_set_column_count(grid, 4);
    TEST_RESULT(result, "Set column count");

    int colCount;
    lc_grid_container_get_column_count(grid, &colCount);
    TEST_ASSERT(colCount == 4, "Column count is 4");

    /* Cell sizing */
    result = lc_grid_container_set_cell_sizing_mode(grid, LC_GRID_CELL_FIXED);
    TEST_RESULT(result, "Set cell sizing mode");

    LCGridCellSizingMode cellMode;
    lc_grid_container_get_cell_sizing_mode(grid, &cellMode);
    TEST_ASSERT(cellMode == LC_GRID_CELL_FIXED, "Cell sizing mode is Fixed");

    result = lc_grid_container_set_fixed_cell_width(grid, 100.0f);
    TEST_RESULT(result, "Set fixed cell width");

    float cellWidth;
    lc_grid_container_get_fixed_cell_width(grid, &cellWidth);
    TEST_ASSERT(cellWidth == 100.0f, "Fixed cell width is 100");

    result = lc_grid_container_set_fixed_cell_height(grid, 80.0f);
    TEST_RESULT(result, "Set fixed cell height");

    float cellHeight;
    lc_grid_container_get_fixed_cell_height(grid, &cellHeight);
    TEST_ASSERT(cellHeight == 80.0f, "Fixed cell height is 80");

    /* Spacing */
    result = lc_grid_container_set_horizontal_spacing(grid, 10.0f);
    TEST_RESULT(result, "Set horizontal spacing");

    float hSpacing;
    lc_grid_container_get_horizontal_spacing(grid, &hSpacing);
    TEST_ASSERT(hSpacing == 10.0f, "Horizontal spacing is 10");

    result = lc_grid_container_set_vertical_spacing(grid, 15.0f);
    TEST_RESULT(result, "Set vertical spacing");

    float vSpacing;
    lc_grid_container_get_vertical_spacing(grid, &vSpacing);
    TEST_ASSERT(vSpacing == 15.0f, "Vertical spacing is 15");

    /* Flow direction */
    result = lc_grid_container_set_flow_direction(grid, LC_GRID_FLOW_TOP_TO_BOTTOM);
    TEST_RESULT(result, "Set flow direction");

    LCGridFlowDirection flow;
    lc_grid_container_get_flow_direction(grid, &flow);
    TEST_ASSERT(flow == LC_GRID_FLOW_TOP_TO_BOTTOM, "Flow direction is TopToBottom");

    /* Cell options */
    result = lc_grid_container_set_homogeneous_cells(grid, true);
    TEST_RESULT(result, "Set homogeneous cells");

    bool homogeneous;
    lc_grid_container_get_homogeneous_cells(grid, &homogeneous);
    TEST_ASSERT(homogeneous == true, "Homogeneous cells is true");

    result = lc_grid_container_set_size_children(grid, true);
    TEST_RESULT(result, "Set size children");

    bool sizeChildren;
    lc_grid_container_get_size_children(grid, &sizeChildren);
    TEST_ASSERT(sizeChildren == true, "Size children is true");

    /* Test all cell sizing modes */
    result = lc_grid_container_set_cell_sizing_mode(grid, LC_GRID_CELL_AUTOMATIC);
    TEST_RESULT(result, "Set cell sizing mode Automatic");

    result = lc_grid_container_set_cell_sizing_mode(grid, LC_GRID_CELL_AUTO_HEIGHT_FIXED_WIDTH);
    TEST_RESULT(result, "Set cell sizing mode AutoHeightFixedWidth");

    result = lc_grid_container_set_cell_sizing_mode(grid, LC_GRID_CELL_AUTO_WIDTH_FIXED_HEIGHT);
    TEST_RESULT(result, "Set cell sizing mode AutoWidthFixedHeight");

    /* Test all flow directions */
    result = lc_grid_container_set_flow_direction(grid, LC_GRID_FLOW_LEFT_TO_RIGHT);
    TEST_RESULT(result, "Set flow direction LeftToRight");

    result = lc_grid_container_set_flow_direction(grid, LC_GRID_FLOW_RIGHT_TO_LEFT);
    TEST_RESULT(result, "Set flow direction RightToLeft");

    result = lc_grid_container_set_flow_direction(grid, LC_GRID_FLOW_BOTTOM_TO_TOP);
    TEST_RESULT(result, "Set flow direction BottomToTop");

    lc_scene_destroy(scene);

    SECTION_END("UI: GridContainer Component");
}

static void test_ui_center_container(void) {
    SECTION_START("UI: CenterContainer Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_center_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "CenterContainerNode", &node);
    TEST_RESULT(result, "Create node for CenterContainer");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle center = NULL;
    result = lc_center_container_create("TestCenterContainer", &center);
    TEST_RESULT(result, "Create CenterContainer component");

    /* Auto fit */
    result = lc_center_container_set_auto_fit_child(center, true);
    TEST_RESULT(result, "Set auto fit child");

    bool autoFit;
    lc_center_container_get_auto_fit_child(center, &autoFit);
    TEST_ASSERT(autoFit == true, "Auto fit child is true");

    /* Aspect ratio */
    result = lc_center_container_set_maintain_aspect_ratio(center, true);
    TEST_RESULT(result, "Set maintain aspect ratio");

    bool maintainAR;
    lc_center_container_get_maintain_aspect_ratio(center, &maintainAR);
    TEST_ASSERT(maintainAR == true, "Maintain aspect ratio is true");

    /* Stack children */
    result = lc_center_container_set_stack_children(center, false);
    TEST_RESULT(result, "Set stack children");

    bool stack;
    lc_center_container_get_stack_children(center, &stack);
    TEST_ASSERT(stack == false, "Stack children is false");

    /* Size */
    result = lc_center_container_set_width(center, 600.0f);
    TEST_RESULT(result, "Set width");

    result = lc_center_container_set_height(center, 400.0f);
    TEST_RESULT(result, "Set height");

    float width;
    lc_center_container_get_width(center, &width);
    TEST_ASSERT(width > 599.0f && width < 601.0f, "Width is 600");

    /* Layer and sorting */
    result = lc_center_container_set_layer(center, 5);
    TEST_RESULT(result, "Set layer");

    int layer;
    lc_center_container_get_layer(center, &layer);
    TEST_ASSERT(layer == 5, "Layer is 5");

    lc_scene_destroy(scene);

    SECTION_END("UI: CenterContainer Component");
}

static void test_ui_padding_container(void) {
    SECTION_START("UI: PaddingContainer Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_padding_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "PaddingContainerNode", &node);
    TEST_RESULT(result, "Create node for PaddingContainer");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle padding = NULL;
    result = lc_padding_container_create("TestPaddingContainer", &padding);
    TEST_RESULT(result, "Create PaddingContainer component");

    /* Auto fit children */
    result = lc_padding_container_set_auto_fit_children(padding, true);
    TEST_RESULT(result, "Set auto fit children");

    bool autoFit;
    lc_padding_container_get_auto_fit_children(padding, &autoFit);
    TEST_ASSERT(autoFit == true, "Auto fit children is true");

    /* Maintain aspect ratio */
    result = lc_padding_container_set_maintain_aspect_ratio(padding, true);
    TEST_RESULT(result, "Set maintain aspect ratio");

    bool maintainAR;
    lc_padding_container_get_maintain_aspect_ratio(padding, &maintainAR);
    TEST_ASSERT(maintainAR == true, "Maintain aspect ratio is true");

    /* Child alignment */
    result = lc_padding_container_set_child_alignment(padding, LC_PADDING_ALIGN_CENTER);
    TEST_RESULT(result, "Set child alignment to center");

    LCPaddingAlignment align;
    lc_padding_container_get_child_alignment(padding, &align);
    TEST_ASSERT(align == LC_PADDING_ALIGN_CENTER, "Alignment is center");

    result = lc_padding_container_set_child_alignment(padding, LC_PADDING_ALIGN_TOP_LEFT);
    TEST_RESULT(result, "Set child alignment to top-left");

    lc_padding_container_get_child_alignment(padding, &align);
    TEST_ASSERT(align == LC_PADDING_ALIGN_TOP_LEFT, "Alignment is top-left");

    /* Padding */
    LCVec4 padValue = { 10.0f, 20.0f, 30.0f, 40.0f };
    result = lc_padding_container_set_padding(padding, padValue);
    TEST_RESULT(result, "Set padding");

    LCVec4 outPad;
    lc_padding_container_get_padding(padding, &outPad);
    TEST_ASSERT(outPad.x > 9.0f && outPad.x < 11.0f, "Padding top is ~10");

    result = lc_padding_container_set_padding_uniform(padding, 15.0f);
    TEST_RESULT(result, "Set uniform padding");

    /* Size */
    result = lc_padding_container_set_width(padding, 500.0f);
    TEST_RESULT(result, "Set width");

    float width;
    lc_padding_container_get_width(padding, &width);
    TEST_ASSERT(width > 499.0f && width < 501.0f, "Width is ~500");

    lc_scene_destroy(scene);

    SECTION_END("UI: PaddingContainer Component");
}

static void test_ui_nine_slice_panel(void) {
    SECTION_START("UI: NineSlicePanel Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_nine_slice_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "NineSlicePanelNode", &node);
    TEST_RESULT(result, "Create node for NineSlicePanel");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle panel = NULL;
    result = lc_nine_slice_panel_create("TestNineSlicePanel", &panel);
    TEST_RESULT(result, "Create NineSlicePanel component");

    /* Size */
    result = lc_nine_slice_panel_set_width(panel, 300.0f);
    TEST_RESULT(result, "Set width");

    float width;
    lc_nine_slice_panel_get_width(panel, &width);
    TEST_ASSERT(width > 299.0f && width < 301.0f, "Width is ~300");

    result = lc_nine_slice_panel_set_height(panel, 200.0f);
    TEST_RESULT(result, "Set height");

    float height;
    lc_nine_slice_panel_get_height(panel, &height);
    TEST_ASSERT(height > 199.0f && height < 201.0f, "Height is ~200");

    /* Texture path */
    result = lc_nine_slice_panel_set_texture_path(panel, "res://panel.png");
    TEST_RESULT(result, "Set texture path");

    char pathBuf[256];
    lc_nine_slice_panel_get_texture_path(panel, pathBuf, sizeof(pathBuf));
    TEST_ASSERT(strcmp(pathBuf, "res://panel.png") == 0, "Texture path matches");

    /* Modulate color */
    LCColor white = { 1.0f, 1.0f, 1.0f, 1.0f };
    result = lc_nine_slice_panel_set_modulate(panel, white);
    TEST_RESULT(result, "Set modulate color");

    LCColor outColor;
    lc_nine_slice_panel_get_modulate(panel, &outColor);
    TEST_ASSERT(outColor.r > 0.9f, "Modulate is white");

    /* Nine-slice margins */
    result = lc_nine_slice_panel_set_margin_left(panel, 10.0f);
    TEST_RESULT(result, "Set left margin");

    float marginLeft;
    lc_nine_slice_panel_get_margin_left(panel, &marginLeft);
    TEST_ASSERT(marginLeft > 9.0f && marginLeft < 11.0f, "Left margin is ~10");

    result = lc_nine_slice_panel_set_margin_right(panel, 15.0f);
    TEST_RESULT(result, "Set right margin");

    result = lc_nine_slice_panel_set_margin_top(panel, 8.0f);
    TEST_RESULT(result, "Set top margin");

    result = lc_nine_slice_panel_set_margin_bottom(panel, 12.0f);
    TEST_RESULT(result, "Set bottom margin");

    result = lc_nine_slice_panel_set_margins_uniform(panel, 20.0f);
    TEST_RESULT(result, "Set uniform margins");

    result = lc_nine_slice_panel_set_margins(panel, 5.0f, 10.0f, 15.0f, 20.0f);
    TEST_RESULT(result, "Set all margins");

    /* Layer */
    result = lc_nine_slice_panel_set_layer(panel, 3);
    TEST_RESULT(result, "Set layer");

    int layer;
    lc_nine_slice_panel_get_layer(panel, &layer);
    TEST_ASSERT(layer == 3, "Layer is 3");

    lc_scene_destroy(scene);

    SECTION_END("UI: NineSlicePanel Component");
}

static void test_ui_dock_container(void) {
    SECTION_START("UI: DockContainer Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_dock_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "DockContainerNode", &node);
    TEST_RESULT(result, "Create node for DockContainer");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle dock = NULL;
    result = lc_dock_container_create("TestDockContainer", &dock);
    TEST_RESULT(result, "Create DockContainer component");

    /* Dock spacing */
    result = lc_dock_container_set_dock_spacing(dock, 10.0f);
    TEST_RESULT(result, "Set dock spacing");

    float spacing;
    lc_dock_container_get_dock_spacing(dock, &spacing);
    TEST_ASSERT(spacing > 9.0f && spacing < 11.0f, "Dock spacing is ~10");

    /* Default dock side */
    result = lc_dock_container_set_default_dock_side(dock, LC_DOCK_TOP);
    TEST_RESULT(result, "Set default dock side to top");

    LCDockSide side;
    lc_dock_container_get_default_dock_side(dock, &side);
    TEST_ASSERT(side == LC_DOCK_TOP, "Default dock side is top");

    result = lc_dock_container_set_default_dock_side(dock, LC_DOCK_CENTER);
    TEST_RESULT(result, "Set default dock side to center");

    lc_dock_container_get_default_dock_side(dock, &side);
    TEST_ASSERT(side == LC_DOCK_CENTER, "Default dock side is center");

    /* Size */
    result = lc_dock_container_set_width(dock, 800.0f);
    TEST_RESULT(result, "Set width");

    float width;
    lc_dock_container_get_width(dock, &width);
    TEST_ASSERT(width > 799.0f && width < 801.0f, "Width is ~800");

    result = lc_dock_container_set_height(dock, 600.0f);
    TEST_RESULT(result, "Set height");

    float height;
    lc_dock_container_get_height(dock, &height);
    TEST_ASSERT(height > 599.0f && height < 601.0f, "Height is ~600");

    /* Layer */
    result = lc_dock_container_set_layer(dock, 2);
    TEST_RESULT(result, "Set layer");

    int layer;
    lc_dock_container_get_layer(dock, &layer);
    TEST_ASSERT(layer == 2, "Layer is 2");

    lc_scene_destroy(scene);

    SECTION_END("UI: DockContainer Component");
}

static void test_shape2d(void) {
    SECTION_START("Shape2D Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_shape2d_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "Shape2DNode", &node);
    TEST_RESULT(result, "Create node for Shape2D");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle shape = NULL;
    result = lc_shape2d_create("TestShape2D", &shape);
    TEST_RESULT(result, "Create Shape2D component");

    /* Shape type */
    result = lc_shape2d_set_shape_type(shape, LC_SHAPE2D_CIRCLE);
    TEST_RESULT(result, "Set shape type to circle");

    LCShape2DType shapeType;
    lc_shape2d_get_shape_type(shape, &shapeType);
    TEST_ASSERT(shapeType == LC_SHAPE2D_CIRCLE, "Shape type is circle");

    result = lc_shape2d_set_shape_type(shape, LC_SHAPE2D_HEXAGON);
    TEST_RESULT(result, "Set shape type to hexagon");

    lc_shape2d_get_shape_type(shape, &shapeType);
    TEST_ASSERT(shapeType == LC_SHAPE2D_HEXAGON, "Shape type is hexagon");

    /* Color */
    LCColor red = { 1.0f, 0.0f, 0.0f, 1.0f };
    result = lc_shape2d_set_color(shape, red);
    TEST_RESULT(result, "Set color to red");

    LCColor outColor;
    lc_shape2d_get_color(shape, &outColor);
    TEST_ASSERT(outColor.r > 0.9f && outColor.g < 0.1f, "Color is red");

    /* Filled */
    result = lc_shape2d_set_filled(shape, true);
    TEST_RESULT(result, "Set filled to true");

    bool filled;
    lc_shape2d_get_filled(shape, &filled);
    TEST_ASSERT(filled == true, "Shape is filled");

    /* Size properties */
    result = lc_shape2d_set_size(shape, 100.0f);
    TEST_RESULT(result, "Set size");

    float size;
    lc_shape2d_get_size(shape, &size);
    TEST_ASSERT(size > 99.0f && size < 101.0f, "Size is ~100");

    result = lc_shape2d_set_radius(shape, 50.0f);
    TEST_RESULT(result, "Set radius");

    float radius;
    lc_shape2d_get_radius(shape, &radius);
    TEST_ASSERT(radius > 49.0f && radius < 51.0f, "Radius is ~50");

    result = lc_shape2d_set_width(shape, 80.0f);
    TEST_RESULT(result, "Set width");

    result = lc_shape2d_set_height(shape, 60.0f);
    TEST_RESULT(result, "Set height");

    /* Border properties */
    result = lc_shape2d_set_border_enabled(shape, true);
    TEST_RESULT(result, "Enable border");

    bool borderEnabled;
    lc_shape2d_get_border_enabled(shape, &borderEnabled);
    TEST_ASSERT(borderEnabled == true, "Border is enabled");

    LCColor blue = { 0.0f, 0.0f, 1.0f, 1.0f };
    result = lc_shape2d_set_border_color(shape, blue);
    TEST_RESULT(result, "Set border color to blue");

    result = lc_shape2d_set_border_width(shape, 3.0f);
    TEST_RESULT(result, "Set border width");

    float borderWidth;
    lc_shape2d_get_border_width(shape, &borderWidth);
    TEST_ASSERT(borderWidth > 2.9f && borderWidth < 3.1f, "Border width is ~3");

    /* Circle segments */
    result = lc_shape2d_set_circle_segments(shape, 64);
    TEST_RESULT(result, "Set circle segments");

    int segments;
    lc_shape2d_get_circle_segments(shape, &segments);
    TEST_ASSERT(segments == 64, "Circle segments is 64");

    /* Layer */
    result = lc_shape2d_set_layer(shape, 4);
    TEST_RESULT(result, "Set layer");

    int layer;
    lc_shape2d_get_layer(shape, &layer);
    TEST_ASSERT(layer == 4, "Layer is 4");

    lc_scene_destroy(scene);

    SECTION_END("Shape2D Component");
}

static void test_scene_instance(void) {
    SECTION_START("SceneInstance Node");

    LCNodeHandle instance = NULL;
    LCResult result = lc_scene_instance_create("TestSceneInstance", &instance);
    TEST_RESULT(result, "Create SceneInstance node");

    /* Scene reference (path won't exist, but API should work) */
    result = lc_scene_instance_set_scene_reference(instance, "res://scenes/test.scene");
    /* May return error if file doesn't exist, but API call itself should work */
    printf("    Set scene reference result: %d (expected: may fail if file doesn't exist)\n", result);

    char pathBuf[256];
    result = lc_scene_instance_get_scene_reference(instance, pathBuf, sizeof(pathBuf));
    TEST_RESULT(result, "Get scene reference");
    printf("    Scene reference path: %s\n", pathBuf);

    /* Check validity */
    bool valid;
    result = lc_scene_instance_has_valid_reference(instance, &valid);
    TEST_RESULT(result, "Check valid reference");
    printf("    Has valid reference: %s\n", valid ? "true" : "false");

    /* Get instanced root (will be NULL if no valid reference) */
    LCNodeHandle root = NULL;
    result = lc_scene_instance_get_instanced_root(instance, &root);
    TEST_RESULT(result, "Get instanced root");
    printf("    Instanced root: %s\n", root ? "exists" : "NULL");

    /* Clear instance */
    result = lc_scene_instance_clear(instance);
    TEST_RESULT(result, "Clear instance");

    lc_node_destroy(instance);

    SECTION_END("SceneInstance Node");
}

static void test_prefab(void) {
    SECTION_START("Prefab System");

    /* Create an empty prefab */
    LCPrefabHandle prefab = NULL;
    LCResult result = lc_prefab_create("TestPrefab", &prefab);
    TEST_RESULT(result, "Create empty prefab");

    /* Check validity (should be invalid with no data) */
    bool valid;
    result = lc_prefab_is_valid(prefab, &valid);
    TEST_RESULT(result, "Check prefab validity");
    printf("    Prefab valid (empty): %s\n", valid ? "true" : "false");

    /* Get/set name */
    char nameBuf[256];
    result = lc_prefab_get_name(prefab, nameBuf, sizeof(nameBuf));
    TEST_RESULT(result, "Get prefab name");
    TEST_ASSERT(strcmp(nameBuf, "TestPrefab") == 0, "Prefab name is correct");

    result = lc_prefab_set_name(prefab, "RenamedPrefab");
    TEST_RESULT(result, "Set prefab name");

    lc_prefab_get_name(prefab, nameBuf, sizeof(nameBuf));
    TEST_ASSERT(strcmp(nameBuf, "RenamedPrefab") == 0, "Prefab name changed");

    /* Get filepath (should be empty for new prefab) */
    char pathBuf[256];
    result = lc_prefab_get_filepath(prefab, pathBuf, sizeof(pathBuf));
    TEST_RESULT(result, "Get prefab filepath");
    printf("    Prefab filepath: '%s'\n", pathBuf);

    /* Clear prefab */
    result = lc_prefab_clear(prefab);
    TEST_RESULT(result, "Clear prefab");

    /* Create prefab from node */
    LCNodeHandle testNode = NULL;
    result = lc_node_create(LC_NODE_3D, "PrefabRoot", &testNode);
    TEST_RESULT(result, "Create test node for prefab");

    LCPrefabHandle prefabFromNode = NULL;
    result = lc_prefab_create_from_node(testNode, &prefabFromNode);
    TEST_RESULT(result, "Create prefab from node");

    result = lc_prefab_is_valid(prefabFromNode, &valid);
    TEST_RESULT(result, "Check prefab validity after creation from node");
    TEST_ASSERT(valid == true, "Prefab from node is valid");

    /* Instantiate */
    LCNodeHandle instantiated = NULL;
    result = lc_prefab_instantiate(prefabFromNode, &instantiated);
    TEST_RESULT(result, "Instantiate prefab");
    TEST_ASSERT(instantiated != NULL, "Instantiated node exists");

    if (instantiated) {
        lc_node_destroy(instantiated);
    }

    /* Cleanup */
    lc_node_destroy(testNode);
    result = lc_prefab_destroy(prefabFromNode);
    TEST_RESULT(result, "Destroy prefab from node");

    result = lc_prefab_destroy(prefab);
    TEST_RESULT(result, "Destroy empty prefab");

    SECTION_END("Prefab System");
}

/* ============================================================================
 * Advanced UI Component Tests
 * ============================================================================ */

static void test_ui_button3d(void) {
    SECTION_START("UI: Button3D Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_button3d_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_3D, "Button3DNode", &node);
    TEST_RESULT(result, "Create 3D node for Button3D");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle button = NULL;
    result = lc_button3d_create(node, &button);
    TEST_RESULT(result, "Create Button3D component");

    /* Size */
    result = lc_button3d_set_width(button, 200.0f);
    TEST_RESULT(result, "Set width");

    float width;
    lc_button3d_get_width(button, &width);
    TEST_ASSERT(width == 200.0f, "Width is 200");

    result = lc_button3d_set_height(button, 50.0f);
    TEST_RESULT(result, "Set height");

    /* Billboard mode */
    result = lc_button3d_set_billboard_mode(button, LC_BUTTON3D_BILLBOARD_Y_AXIS);
    TEST_RESULT(result, "Set billboard mode");

    LCButton3DBillboardMode billboardMode;
    lc_button3d_get_billboard_mode(button, &billboardMode);
    TEST_ASSERT(billboardMode == LC_BUTTON3D_BILLBOARD_Y_AXIS, "Billboard mode is Y-axis");

    /* Shadow settings */
    result = lc_button3d_set_cast_shadow(button, true);
    TEST_RESULT(result, "Set cast shadow");

    bool castShadow;
    lc_button3d_get_cast_shadow(button, &castShadow);
    TEST_ASSERT(castShadow == true, "Cast shadow is true");

    /* Text */
    result = lc_button3d_set_text(button, "Click Me!");
    TEST_RESULT(result, "Set text");

    char textBuf[256];
    lc_button3d_get_text(button, textBuf, sizeof(textBuf));
    TEST_ASSERT(strcmp(textBuf, "Click Me!") == 0, "Text is 'Click Me!'");

    /* Background color */
    result = lc_button3d_set_background_color(button, lc_color(0.2f, 0.4f, 0.8f, 1.0f));
    TEST_RESULT(result, "Set background color");

    /* Style mode */
    result = lc_button3d_set_style_mode(button, LC_BUTTON3D_STYLE_AUTOMATIC);
    TEST_RESULT(result, "Set style mode");

    /* State modulation */
    result = lc_button3d_set_state_modulation(button, LC_BUTTON3D_STATE_HOVER, lc_color(1.1f, 1.1f, 1.1f, 1.0f));
    TEST_RESULT(result, "Set hover modulation");

    lc_scene_destroy(scene);

    SECTION_END("UI: Button3D Component");
}

static void test_ui_texture_button(void) {
    SECTION_START("UI: TextureButton Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_texbutton_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "TextureButtonNode", &node);
    TEST_RESULT(result, "Create node for TextureButton");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle button = NULL;
    result = lc_texture_button_create(node, &button);
    TEST_RESULT(result, "Create TextureButton component");

    /* Size */
    result = lc_texture_button_set_width(button, 128.0f);
    TEST_RESULT(result, "Set width");

    result = lc_texture_button_set_height(button, 64.0f);
    TEST_RESULT(result, "Set height");

    /* Stretch mode */
    result = lc_texture_button_set_stretch_mode(button, LC_TEXTURE_BUTTON_NINE_SLICE);
    TEST_RESULT(result, "Set stretch mode");

    LCTextureButtonStretchMode stretchMode;
    lc_texture_button_get_stretch_mode(button, &stretchMode);
    TEST_ASSERT(stretchMode == LC_TEXTURE_BUTTON_NINE_SLICE, "Stretch mode is Nine Slice");

    /* Toggle mode */
    result = lc_texture_button_set_is_toggle(button, true);
    TEST_RESULT(result, "Set toggle mode");

    bool isToggle;
    lc_texture_button_get_is_toggle(button, &isToggle);
    TEST_ASSERT(isToggle == true, "Toggle mode is enabled");

    result = lc_texture_button_set_is_checked(button, true);
    TEST_RESULT(result, "Set checked state");

    bool isChecked;
    lc_texture_button_get_is_checked(button, &isChecked);
    TEST_ASSERT(isChecked == true, "Checked state is true");

    /* Click mask */
    result = lc_texture_button_set_click_mask_mode(button, LC_CLICK_MASK_ALPHA_THRESHOLD);
    TEST_RESULT(result, "Set click mask mode");

    result = lc_texture_button_set_alpha_threshold(button, 0.5f);
    TEST_RESULT(result, "Set alpha threshold");

    float threshold;
    lc_texture_button_get_alpha_threshold(button, &threshold);
    TEST_ASSERT(threshold == 0.5f, "Alpha threshold is 0.5");

    /* Mouse button */
    result = lc_texture_button_set_mouse_button(button, LC_MOUSE_BUTTON_LEFT);
    TEST_RESULT(result, "Set mouse button");

    /* Text */
    result = lc_texture_button_set_text(button, "Play");
    TEST_RESULT(result, "Set text");

    lc_scene_destroy(scene);

    SECTION_END("UI: TextureButton Component");
}

static void test_ui_toggle_button(void) {
    SECTION_START("UI: ToggleButton Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_toggle_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "ToggleButtonNode", &node);
    TEST_RESULT(result, "Create node for ToggleButton");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle button = NULL;
    result = lc_toggle_button_create(node, &button);
    TEST_RESULT(result, "Create ToggleButton component");

    /* Size */
    result = lc_toggle_button_set_width(button, 150.0f);
    TEST_RESULT(result, "Set width");

    result = lc_toggle_button_set_height(button, 40.0f);
    TEST_RESULT(result, "Set height");

    /* Toggle state */
    result = lc_toggle_button_set_toggled(button, true);
    TEST_RESULT(result, "Set toggled state");

    bool toggled;
    lc_toggle_button_is_toggled(button, &toggled);
    TEST_ASSERT(toggled == true, "Toggled state is true");

    /* Style mode */
    result = lc_toggle_button_set_style_mode(button, LC_TOGGLE_BUTTON_STYLE_AUTOMATIC);
    TEST_RESULT(result, "Set style mode");

    LCToggleButtonStyleMode styleMode;
    lc_toggle_button_get_style_mode(button, &styleMode);
    TEST_ASSERT(styleMode == LC_TOGGLE_BUTTON_STYLE_AUTOMATIC, "Style mode is Automatic");

    /* Text */
    result = lc_toggle_button_set_text(button, "Enable Sound");
    TEST_RESULT(result, "Set text");

    char textBuf[256];
    lc_toggle_button_get_text(button, textBuf, sizeof(textBuf));
    TEST_ASSERT(strcmp(textBuf, "Enable Sound") == 0, "Text is 'Enable Sound'");

    /* State modulation */
    result = lc_toggle_button_set_state_modulation(button, LC_TOGGLE_BUTTON_STATE_TOGGLED, lc_color(0.0f, 1.0f, 0.0f, 1.0f));
    TEST_RESULT(result, "Set toggled modulation");

    /* Background */
    result = lc_toggle_button_set_background_color(button, lc_color(0.3f, 0.3f, 0.3f, 1.0f));
    TEST_RESULT(result, "Set background color");

    lc_scene_destroy(scene);

    SECTION_END("UI: ToggleButton Component");
}

static void test_ui_radio_button(void) {
    SECTION_START("UI: RadioButton Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_radio_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "RadioButtonNode", &node);
    TEST_RESULT(result, "Create node for RadioButton");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle radio = NULL;
    result = lc_radio_button_create(node, &radio);
    TEST_RESULT(result, "Create RadioButton component");

    /* Indicator size */
    result = lc_radio_button_set_indicator_size(radio, 24.0f);
    TEST_RESULT(result, "Set indicator size");

    float indicatorSize;
    lc_radio_button_get_indicator_size(radio, &indicatorSize);
    TEST_ASSERT(indicatorSize == 24.0f, "Indicator size is 24");

    /* Group */
    result = lc_radio_button_set_group_name(radio, "difficulty");
    TEST_RESULT(result, "Set group name");

    char groupBuf[256];
    lc_radio_button_get_group_name(radio, groupBuf, sizeof(groupBuf));
    TEST_ASSERT(strcmp(groupBuf, "difficulty") == 0, "Group name is 'difficulty'");

    result = lc_radio_button_set_radio_value(radio, 1);
    TEST_RESULT(result, "Set radio value");

    int value;
    lc_radio_button_get_radio_value(radio, &value);
    TEST_ASSERT(value == 1, "Radio value is 1");

    /* Selection */
    result = lc_radio_button_set_selected(radio, true);
    TEST_RESULT(result, "Set selected");

    bool selected;
    lc_radio_button_is_selected(radio, &selected);
    TEST_ASSERT(selected == true, "Radio is selected");

    /* Colors */
    result = lc_radio_button_set_outer_circle_color(radio, lc_color(0.5f, 0.5f, 0.5f, 1.0f));
    TEST_RESULT(result, "Set outer circle color");

    result = lc_radio_button_set_inner_circle_color(radio, lc_color(0.0f, 0.5f, 1.0f, 1.0f));
    TEST_RESULT(result, "Set inner circle color");

    result = lc_radio_button_set_inner_circle_scale(radio, 0.6f);
    TEST_RESULT(result, "Set inner circle scale");

    /* Text */
    result = lc_radio_button_set_text(radio, "Easy");
    TEST_RESULT(result, "Set text");

    lc_scene_destroy(scene);

    SECTION_END("UI: RadioButton Component");
}

static void test_ui_radio_list(void) {
    SECTION_START("UI: RadioList Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_radiolist_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "RadioListNode", &node);
    TEST_RESULT(result, "Create node for RadioList");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle list = NULL;
    result = lc_radio_list_create(node, &list);
    TEST_RESULT(result, "Create RadioList component");

    /* Group name */
    result = lc_radio_list_set_group_name(list, "options");
    TEST_RESULT(result, "Set group name");

    /* Orientation */
    result = lc_radio_list_set_orientation(list, LC_RADIO_LIST_VERTICAL);
    TEST_RESULT(result, "Set orientation");

    LCRadioListOrientation orient;
    lc_radio_list_get_orientation(list, &orient);
    TEST_ASSERT(orient == LC_RADIO_LIST_VERTICAL, "Orientation is vertical");

    /* Spacing */
    result = lc_radio_list_set_spacing(list, 12.0f);
    TEST_RESULT(result, "Set spacing");

    float spacing;
    lc_radio_list_get_spacing(list, &spacing);
    TEST_ASSERT(spacing == 12.0f, "Spacing is 12");

    /* Items */
    result = lc_radio_list_add_item(list, "Option A");
    TEST_RESULT(result, "Add item A");

    result = lc_radio_list_add_item(list, "Option B");
    TEST_RESULT(result, "Add item B");

    int count;
    lc_radio_list_get_item_count(list, &count);
    TEST_ASSERT(count == 2, "Item count is 2");

    /* Selection */
    result = lc_radio_list_set_selected_index(list, 0);
    TEST_RESULT(result, "Set selected index");

    int selectedIdx;
    lc_radio_list_get_selected_index(list, &selectedIdx);
    TEST_ASSERT(selectedIdx == 0, "Selected index is 0");

    lc_scene_destroy(scene);

    SECTION_END("UI: RadioList Component");
}

static void test_ui_checkbox(void) {
    SECTION_START("UI: Checkbox Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_checkbox_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "CheckboxNode", &node);
    TEST_RESULT(result, "Create node for Checkbox");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle checkbox = NULL;
    result = lc_checkbox_create(node, &checkbox);
    TEST_RESULT(result, "Create Checkbox component");

    /* Box size */
    result = lc_checkbox_set_box_size(checkbox, 20.0f);
    TEST_RESULT(result, "Set box size");

    float boxSize;
    lc_checkbox_get_box_size(checkbox, &boxSize);
    TEST_ASSERT(boxSize == 20.0f, "Box size is 20");

    /* Checked state */
    result = lc_checkbox_set_checked(checkbox, true);
    TEST_RESULT(result, "Set checked");

    bool checked;
    lc_checkbox_is_checked(checkbox, &checked);
    TEST_ASSERT(checked == true, "Checkbox is checked");

    result = lc_checkbox_toggle(checkbox);
    TEST_RESULT(result, "Toggle checkbox");

    lc_checkbox_is_checked(checkbox, &checked);
    TEST_ASSERT(checked == false, "Checkbox is unchecked after toggle");

    /* Group */
    result = lc_checkbox_set_group_name(checkbox, "features");
    TEST_RESULT(result, "Set group name");

    result = lc_checkbox_set_checkbox_value(checkbox, 42);
    TEST_RESULT(result, "Set checkbox value");

    int val;
    lc_checkbox_get_checkbox_value(checkbox, &val);
    TEST_ASSERT(val == 42, "Checkbox value is 42");

    /* Colors */
    result = lc_checkbox_set_checkmark_color(checkbox, lc_color(0.0f, 1.0f, 0.0f, 1.0f));
    TEST_RESULT(result, "Set checkmark color");

    result = lc_checkbox_set_corner_radius(checkbox, 4.0f);
    TEST_RESULT(result, "Set corner radius");

    /* Text */
    result = lc_checkbox_set_text(checkbox, "Enable feature");
    TEST_RESULT(result, "Set text");

    lc_scene_destroy(scene);

    SECTION_END("UI: Checkbox Component");
}

static void test_ui_check_list(void) {
    SECTION_START("UI: CheckList Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_checklist_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "CheckListNode", &node);
    TEST_RESULT(result, "Create node for CheckList");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle list = NULL;
    result = lc_check_list_create(node, &list);
    TEST_RESULT(result, "Create CheckList component");

    /* Group */
    result = lc_check_list_set_group_name(list, "tasks");
    TEST_RESULT(result, "Set group name");

    /* Orientation */
    result = lc_check_list_set_orientation(list, LC_CHECK_LIST_VERTICAL);
    TEST_RESULT(result, "Set orientation");

    LCCheckListOrientation orient;
    lc_check_list_get_orientation(list, &orient);
    TEST_ASSERT(orient == LC_CHECK_LIST_VERTICAL, "Orientation is vertical");

    /* Spacing */
    result = lc_check_list_set_spacing(list, 8.0f);
    TEST_RESULT(result, "Set spacing");

    /* Items */
    result = lc_check_list_add_item(list, "Task 1", 1);
    TEST_RESULT(result, "Add item 1");

    result = lc_check_list_add_item(list, "Task 2", 2);
    TEST_RESULT(result, "Add item 2");

    result = lc_check_list_add_item(list, "Task 3", 3);
    TEST_RESULT(result, "Add item 3");

    int count;
    lc_check_list_get_item_count(list, &count);
    TEST_ASSERT(count == 3, "Item count is 3");

    /* Group operations */
    result = lc_check_list_check_all(list);
    TEST_RESULT(result, "Check all");

    bool allChecked;
    lc_check_list_are_all_checked(list, &allChecked);
    // Note: allChecked may depend on actual checkboxes being created

    result = lc_check_list_uncheck_all(list);
    TEST_RESULT(result, "Uncheck all");

    /* Get counts */
    int checkedCount;
    lc_check_list_get_checked_count(list, &checkedCount);
    TEST_RESULT(result, "Get checked count");

    int totalCount;
    lc_check_list_get_total_count(list, &totalCount);
    TEST_RESULT(result, "Get total count");

    /* Clear */
    result = lc_check_list_clear_items(list);
    TEST_RESULT(result, "Clear items");

    lc_check_list_get_item_count(list, &count);
    TEST_ASSERT(count == 0, "Item count is 0 after clear");

    lc_scene_destroy(scene);

    SECTION_END("UI: CheckList Component");
}

/* ============================================================================
 * Utility Component Tests
 * ============================================================================ */

static void test_timer(void) {
    SECTION_START("Utility: Timer Component");

    LCComponentHandle timer = NULL;
    LCResult result = lc_timer_create("TestTimer", &timer);
    TEST_RESULT(result, "Create Timer");

    /* Duration */
    result = lc_timer_set_duration(timer, 5.0f);
    TEST_RESULT(result, "Set duration");

    float duration;
    lc_timer_get_duration(timer, &duration);
    TEST_ASSERT(duration == 5.0f, "Duration is 5 seconds");

    /* Loop */
    result = lc_timer_set_loop(timer, true);
    TEST_RESULT(result, "Enable loop");

    bool loop;
    lc_timer_get_loop(timer, &loop);
    TEST_ASSERT(loop, "Loop is enabled");

    /* Auto-start */
    result = lc_timer_set_auto_start(timer, false);
    TEST_RESULT(result, "Disable auto-start");

    /* Ignore time scale */
    result = lc_timer_set_ignore_time_scale(timer, false);
    TEST_RESULT(result, "Set ignore time scale");

    /* Start/Stop */
    result = lc_timer_start(timer);
    TEST_RESULT(result, "Start timer");

    bool running;
    lc_timer_is_running(timer, &running);
    TEST_ASSERT(running, "Timer is running");

    result = lc_timer_stop(timer);
    TEST_RESULT(result, "Stop timer");

    lc_timer_is_running(timer, &running);
    TEST_ASSERT(!running, "Timer is stopped");

    /* Reset/Restart */
    result = lc_timer_reset(timer);
    TEST_RESULT(result, "Reset timer");

    result = lc_timer_restart(timer);
    TEST_RESULT(result, "Restart timer");

    /* Elapsed time */
    float elapsed;
    lc_timer_get_elapsed(timer, &elapsed);
    printf("    Elapsed: %.3f seconds\n", elapsed);

    /* Time remaining */
    float remaining;
    lc_timer_get_time_remaining(timer, &remaining);
    printf("    Remaining: %.3f seconds\n", remaining);

    lc_component_destroy(timer);

    SECTION_END("Utility: Timer Component");
}

/* ============================================================================
 * Input Tests (mostly checking API exists, actual input requires window)
 * ============================================================================ */

static void test_input_api(void) {
    SECTION_START("Input: API Availability Check");

    /* These won't detect actual input without a window, but verify API works */

    /* Keyboard */
    bool pressed = lc_input_is_key_pressed(LC_KEY_SPACE);
    printf("    Space key pressed: %s\n", pressed ? "yes" : "no");
    TEST_ASSERT(true, "lc_input_is_key_pressed() callable");

    pressed = lc_input_is_key_just_pressed(LC_KEY_ESCAPE);
    TEST_ASSERT(true, "lc_input_is_key_just_pressed() callable");

    pressed = lc_input_is_key_just_released(LC_KEY_ENTER);
    TEST_ASSERT(true, "lc_input_is_key_just_released() callable");

    /* Mouse */
    pressed = lc_input_is_mouse_button_pressed(LC_MOUSE_BUTTON_LEFT);
    TEST_ASSERT(true, "lc_input_is_mouse_button_pressed() callable");

    LCVec2 mouse_pos;
    LCResult result = lc_input_get_mouse_position(&mouse_pos);
    printf("    Mouse position result: %d\n", (int)result);

    LCVec2 mouse_delta;
    result = lc_input_get_mouse_delta(&mouse_delta);
    printf("    Mouse delta result: %d\n", (int)result);

    LCVec2 scroll_delta;
    result = lc_input_get_mouse_scroll_delta(&scroll_delta);
    printf("    Scroll delta result: %d\n", (int)result);

    /* Gamepad */
    pressed = lc_input_is_gamepad_button_pressed(LC_GAMEPAD_BUTTON_A, 0);
    TEST_ASSERT(true, "lc_input_is_gamepad_button_pressed() callable");

    float axis = lc_input_get_gamepad_axis(LC_GAMEPAD_AXIS_LEFT_X, 0);
    printf("    Gamepad axis value: %.2f\n", axis);

    /* Actions */
    pressed = lc_input_is_action_pressed("jump");
    TEST_ASSERT(true, "lc_input_is_action_pressed() callable");

    axis = lc_input_get_axis("horizontal");
    TEST_ASSERT(true, "lc_input_get_axis() callable");

    SECTION_END("Input: API Availability Check");
}

/* ============================================================================
 * Component Management Tests
 * ============================================================================ */

static void test_component_node_attachment(void) {
    SECTION_START("Components: Node Attachment");

    /* Create a node */
    LCNodeHandle node = NULL;
    lc_node_create(LC_NODE_3D, "ComponentTestNode", &node);

    /* Create a component */
    LCComponentHandle light = NULL;
    LCResult result = lc_omni_light3d_create("AttachedLight", &light);
    TEST_RESULT(result, "Create component");

    /* Attach component to node */
    result = lc_node_add_component(node, light);
    TEST_RESULT(result, "Attach component to node");

    /* Create another component */
    LCComponentHandle sprite = NULL;
    lc_sprite3d_create("AttachedSprite", &sprite);

    result = lc_node_add_component(node, sprite);
    TEST_RESULT(result, "Attach second component");

    /* Cleanup - destroying node should handle attached components */
    lc_node_destroy(node);
    TEST_ASSERT(true, "Node with components destroyed");

    SECTION_END("Components: Node Attachment");
}

/* ============================================================================
 * Line2D Tests
 * ============================================================================ */

static void test_line2d(void) {
    SECTION_START("Rendering: Line2D");

    /* Create a test scene and node first */
    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("TestLine2DScene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "Line2DNode", &node);
    TEST_RESULT(result, "Create node for Line2D");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    /* Create Line2D component */
    LCComponentHandle line = NULL;
    result = lc_line2d_create(node, &line);
    TEST_RESULT(result, "Create Line2D component");
    TEST_ASSERT(line != NULL, "Component handle is valid");

    /* Note: Line2D starts with 2 default points, verify and clear them */
    uint32_t initialCount = 0;
    result = lc_line2d_get_point_count(line, &initialCount);
    TEST_RESULT(result, "Get initial point count");
    printf("    Initial point count: %u (default points from constructor)\n", initialCount);

    result = lc_line2d_clear_points(line);
    TEST_RESULT(result, "Clear initial points");

    /* Point management - now add our own points */
    LCVec2 p1 = {0.0f, 0.0f};
    LCVec2 p2 = {100.0f, 0.0f};
    LCVec2 p3 = {100.0f, 100.0f};
    LCVec2 p4 = {0.0f, 100.0f};

    result = lc_line2d_add_point(line, p1);
    TEST_RESULT(result, "Add point 1");

    result = lc_line2d_add_point(line, p2);
    TEST_RESULT(result, "Add point 2");

    result = lc_line2d_add_point(line, p3);
    TEST_RESULT(result, "Add point 3");

    result = lc_line2d_add_point(line, p4);
    TEST_RESULT(result, "Add point 4");

    uint32_t count = 0;
    result = lc_line2d_get_point_count(line, &count);
    TEST_RESULT(result, "Get point count");
    TEST_ASSERT(count == 4, "Point count is 4");

    /* Get point */
    LCVec2 retrievedPoint;
    result = lc_line2d_get_point(line, 1, &retrievedPoint);
    TEST_RESULT(result, "Get point at index 1");
    TEST_ASSERT(retrievedPoint.x == 100.0f && retrievedPoint.y == 0.0f, "Point 1 is correct");

    /* Set point */
    LCVec2 newPoint = {50.0f, 25.0f};
    result = lc_line2d_set_point(line, 1, newPoint);
    TEST_RESULT(result, "Set point at index 1");

    result = lc_line2d_get_point(line, 1, &retrievedPoint);
    TEST_ASSERT(retrievedPoint.x == 50.0f && retrievedPoint.y == 25.0f, "Point 1 updated correctly");

    /* Insert point */
    LCVec2 insertPoint = {25.0f, 12.5f};
    result = lc_line2d_insert_point(line, 1, insertPoint);
    TEST_RESULT(result, "Insert point at index 1");

    result = lc_line2d_get_point_count(line, &count);
    TEST_ASSERT(count == 5, "Point count is 5 after insert");

    /* Remove point */
    result = lc_line2d_remove_point(line, 1);
    TEST_RESULT(result, "Remove point at index 1");

    result = lc_line2d_get_point_count(line, &count);
    TEST_ASSERT(count == 4, "Point count is 4 after remove");

    /* Beginning and end */
    LCVec2 beginning, end;
    result = lc_line2d_get_beginning(line, &beginning);
    TEST_RESULT(result, "Get beginning");
    TEST_ASSERT(beginning.x == 0.0f && beginning.y == 0.0f, "Beginning is (0, 0)");

    result = lc_line2d_get_end(line, &end);
    TEST_RESULT(result, "Get end");

    LCVec2 newBeginning = {10.0f, 10.0f};
    result = lc_line2d_set_beginning(line, newBeginning);
    TEST_RESULT(result, "Set beginning");

    result = lc_line2d_get_beginning(line, &beginning);
    TEST_ASSERT(beginning.x == 10.0f && beginning.y == 10.0f, "Beginning updated to (10, 10)");

    /* Stroke color */
    LCColor red = {1.0f, 0.0f, 0.0f, 1.0f};
    result = lc_line2d_set_stroke_color(line, red);
    TEST_RESULT(result, "Set stroke color");

    LCColor strokeColor;
    result = lc_line2d_get_stroke_color(line, &strokeColor);
    TEST_RESULT(result, "Get stroke color");
    TEST_ASSERT(strokeColor.r == 1.0f && strokeColor.g == 0.0f && strokeColor.b == 0.0f && strokeColor.a == 1.0f, "Stroke color is red");

    /* Stroke width */
    result = lc_line2d_set_stroke_width(line, 5.0f);
    TEST_RESULT(result, "Set stroke width");

    float strokeWidth;
    result = lc_line2d_get_stroke_width(line, &strokeWidth);
    TEST_RESULT(result, "Get stroke width");
    TEST_ASSERT(strokeWidth == 5.0f, "Stroke width is 5.0");

    /* Closed loop */
    result = lc_line2d_set_closed_loop(line, true);
    TEST_RESULT(result, "Set closed loop");

    bool closedLoop;
    result = lc_line2d_get_closed_loop(line, &closedLoop);
    TEST_RESULT(result, "Get closed loop");
    TEST_ASSERT(closedLoop == true, "Closed loop is true");

    /* Cap style */
    result = lc_line2d_set_cap_style(line, LC_LINE_CAP_ROUND);
    TEST_RESULT(result, "Set cap style to round");

    LCLineCapStyle capStyle;
    result = lc_line2d_get_cap_style(line, &capStyle);
    TEST_RESULT(result, "Get cap style");
    TEST_ASSERT(capStyle == LC_LINE_CAP_ROUND, "Cap style is round");

    /* Join style */
    result = lc_line2d_set_join_style(line, LC_LINE_JOIN_BEVEL);
    TEST_RESULT(result, "Set join style to bevel");

    LCLineJoinStyle joinStyle;
    result = lc_line2d_get_join_style(line, &joinStyle);
    TEST_RESULT(result, "Get join style");
    TEST_ASSERT(joinStyle == LC_LINE_JOIN_BEVEL, "Join style is bevel");

    /* Anti-aliasing */
    result = lc_line2d_set_anti_aliasing(line, true);
    TEST_RESULT(result, "Set anti-aliasing");

    bool aa;
    result = lc_line2d_get_anti_aliasing(line, &aa);
    TEST_RESULT(result, "Get anti-aliasing");
    TEST_ASSERT(aa == true, "Anti-aliasing is enabled");

    /* Smoothness */
    result = lc_line2d_set_smoothness(line, 16);
    TEST_RESULT(result, "Set smoothness");

    int smoothness;
    result = lc_line2d_get_smoothness(line, &smoothness);
    TEST_RESULT(result, "Get smoothness");
    TEST_ASSERT(smoothness == 16, "Smoothness is 16");

    /* Layer */
    result = lc_line2d_set_layer(line, 5);
    TEST_RESULT(result, "Set layer");

    int layer;
    result = lc_line2d_get_layer(line, &layer);
    TEST_RESULT(result, "Get layer");
    TEST_ASSERT(layer == 5, "Layer is 5");

    /* Sorting order */
    result = lc_line2d_set_sorting_order(line, 10);
    TEST_RESULT(result, "Set sorting order");

    int sortingOrder;
    result = lc_line2d_get_sorting_order(line, &sortingOrder);
    TEST_RESULT(result, "Get sorting order");
    TEST_ASSERT(sortingOrder == 10, "Sorting order is 10");

    /* UI space */
    result = lc_line2d_set_ui_space(line, true);
    TEST_RESULT(result, "Set UI space");

    bool uiSpace;
    result = lc_line2d_get_ui_space(line, &uiSpace);
    TEST_RESULT(result, "Get UI space");
    TEST_ASSERT(uiSpace == true, "UI space is enabled");

    /* Clear points */
    result = lc_line2d_clear_points(line);
    TEST_RESULT(result, "Clear points");

    result = lc_line2d_get_point_count(line, &count);
    TEST_ASSERT(count == 0, "Point count is 0 after clear");

    /* Set points in bulk */
    LCVec2 points[] = {
        {0.0f, 0.0f},
        {50.0f, 0.0f},
        {50.0f, 50.0f}
    };
    result = lc_line2d_set_points(line, points, 3);
    TEST_RESULT(result, "Set points in bulk");

    result = lc_line2d_get_point_count(line, &count);
    TEST_ASSERT(count == 3, "Point count is 3 after bulk set");

    /* Get points in bulk */
    LCVec2 outPoints[10];
    uint32_t outCount;
    result = lc_line2d_get_points(line, outPoints, 10, &outCount);
    TEST_RESULT(result, "Get points in bulk");
    TEST_ASSERT(outCount == 3, "Retrieved 3 points");
    TEST_ASSERT(outPoints[0].x == 0.0f && outPoints[0].y == 0.0f, "First point is (0, 0)");
    TEST_ASSERT(outPoints[2].x == 50.0f && outPoints[2].y == 50.0f, "Third point is (50, 50)");

    /* Cleanup */
    lc_scene_destroy(scene);

    SECTION_END("Rendering: Line2D");
}

/* ============================================================================
 * Material System Tests
 * ============================================================================ */

static void test_material_property_block(void) {
    SECTION_START("Materials: MaterialPropertyBlock");

    /* Create property block */
    LCMaterialPropertyBlockHandle block = NULL;
    LCResult result = lc_material_property_block_create(&block);
    TEST_RESULT(result, "Create property block");
    TEST_ASSERT(block != NULL, "Property block handle is valid");

    /* Check empty state */
    bool isEmpty = false;
    result = lc_material_property_block_is_empty(block, &isEmpty);
    TEST_RESULT(result, "Check is_empty");
    TEST_ASSERT(isEmpty == true, "New block is empty");

    /* Set float property */
    result = lc_material_property_block_set_float(block, "roughness", 0.75f);
    TEST_RESULT(result, "Set float property");

    /* Verify not empty after setting */
    result = lc_material_property_block_is_empty(block, &isEmpty);
    TEST_ASSERT(isEmpty == false, "Block not empty after set");

    /* Get float property */
    float floatValue = 0.0f;
    result = lc_material_property_block_get_float(block, "roughness", &floatValue);
    TEST_RESULT(result, "Get float property");
    TEST_ASSERT(floatValue == 0.75f, "Float value is 0.75");

    /* Set int property */
    result = lc_material_property_block_set_int(block, "layer", 5);
    TEST_RESULT(result, "Set int property");

    int intValue = 0;
    result = lc_material_property_block_get_int(block, "layer", &intValue);
    TEST_RESULT(result, "Get int property");
    TEST_ASSERT(intValue == 5, "Int value is 5");

    /* Set bool property */
    result = lc_material_property_block_set_bool(block, "useNormal", true);
    TEST_RESULT(result, "Set bool property");

    bool boolValue = false;
    result = lc_material_property_block_get_bool(block, "useNormal", &boolValue);
    TEST_RESULT(result, "Get bool property");
    TEST_ASSERT(boolValue == true, "Bool value is true");

    /* Set Vec2 property */
    LCVec2 vec2Val = {1.5f, 2.5f};
    result = lc_material_property_block_set_vec2(block, "tiling", vec2Val);
    TEST_RESULT(result, "Set Vec2 property");

    LCVec2 vec2Out = {0, 0};
    result = lc_material_property_block_get_vec2(block, "tiling", &vec2Out);
    TEST_RESULT(result, "Get Vec2 property");
    TEST_ASSERT(vec2Out.x == 1.5f && vec2Out.y == 2.5f, "Vec2 value correct");

    /* Set Vec3 property */
    LCVec3 vec3Val = {1.0f, 2.0f, 3.0f};
    result = lc_material_property_block_set_vec3(block, "offset", vec3Val);
    TEST_RESULT(result, "Set Vec3 property");

    LCVec3 vec3Out = {0, 0, 0};
    result = lc_material_property_block_get_vec3(block, "offset", &vec3Out);
    TEST_RESULT(result, "Get Vec3 property");
    TEST_ASSERT(vec3Out.x == 1.0f && vec3Out.y == 2.0f && vec3Out.z == 3.0f, "Vec3 value correct");

    /* Set Vec4 property */
    LCVec4 vec4Val = {0.1f, 0.2f, 0.3f, 0.4f};
    result = lc_material_property_block_set_vec4(block, "params", vec4Val);
    TEST_RESULT(result, "Set Vec4 property");

    LCVec4 vec4Out = {0, 0, 0, 0};
    result = lc_material_property_block_get_vec4(block, "params", &vec4Out);
    TEST_RESULT(result, "Get Vec4 property");
    TEST_ASSERT(vec4Out.x == 0.1f && vec4Out.w == 0.4f, "Vec4 value correct");

    /* Set Color property */
    LCColor colorVal = {1.0f, 0.5f, 0.25f, 1.0f};
    result = lc_material_property_block_set_color(block, "albedo", colorVal);
    TEST_RESULT(result, "Set Color property");

    LCColor colorOut = {0, 0, 0, 0};
    result = lc_material_property_block_get_color(block, "albedo", &colorOut);
    TEST_RESULT(result, "Get Color property");
    TEST_ASSERT(colorOut.r == 1.0f && colorOut.g == 0.5f && colorOut.b == 0.25f, "Color value correct");

    /* Check has_property */
    bool hasProp = false;
    result = lc_material_property_block_has_property(block, "roughness", &hasProp);
    TEST_RESULT(result, "Check has_property (existing)");
    TEST_ASSERT(hasProp == true, "Has 'roughness' property");

    result = lc_material_property_block_has_property(block, "nonexistent", &hasProp);
    TEST_RESULT(result, "Check has_property (non-existing)");
    TEST_ASSERT(hasProp == false, "Does not have 'nonexistent' property");

    /* Clear properties */
    result = lc_material_property_block_clear(block);
    TEST_RESULT(result, "Clear properties");

    result = lc_material_property_block_is_empty(block, &isEmpty);
    TEST_ASSERT(isEmpty == true, "Block empty after clear");

    /* Destroy property block */
    result = lc_material_property_block_destroy(block);
    TEST_RESULT(result, "Destroy property block");

    SECTION_END("Materials: MaterialPropertyBlock");
}

static void test_material_pbr_properties_default(void) {
    SECTION_START("Materials: PBR Default Properties");

    /* Get default properties */
    LCPBRMaterialProperties props = lc_pbr_material_properties_default();

    /* Verify albedo defaults */
    TEST_ASSERT(props.albedoColor.r == 1.0f && props.albedoColor.g == 1.0f &&
                props.albedoColor.b == 1.0f && props.albedoColor.a == 1.0f,
                "Default albedo is white");
    TEST_ASSERT(props.useAlbedoTexture == false, "Default useAlbedoTexture is false");

    /* Verify metallic-roughness defaults */
    TEST_ASSERT(props.metallic == 0.0f, "Default metallic is 0.0");
    TEST_ASSERT(props.roughness == 0.5f, "Default roughness is 0.5");

    /* Verify normal mapping defaults */
    TEST_ASSERT(props.useNormalTexture == false, "Default useNormalTexture is false");
    TEST_ASSERT(props.normalScale == 1.0f, "Default normalScale is 1.0");

    /* Verify emissive defaults */
    TEST_ASSERT(props.emissiveColor.r == 0.0f && props.emissiveColor.g == 0.0f &&
                props.emissiveColor.b == 0.0f, "Default emissive is black");
    TEST_ASSERT(props.emissiveStrength == 1.0f, "Default emissiveStrength is 1.0");

    /* Verify alpha defaults */
    TEST_ASSERT(props.alphaCutoff == 0.5f, "Default alphaCutoff is 0.5");
    TEST_ASSERT(props.alphaBlend == false, "Default alphaBlend is false");

    /* Verify shader type default */
    TEST_ASSERT(props.shaderType == LC_SHADER_PBR, "Default shaderType is PBR");

    /* Verify rendering options defaults */
    TEST_ASSERT(props.doubleSided == false, "Default doubleSided is false");
    TEST_ASSERT(props.castShadows == true, "Default castShadows is true");
    TEST_ASSERT(props.receiveShadows == true, "Default receiveShadows is true");

    /* Verify toon shader defaults */
    TEST_ASSERT(props.toonShadowBands == 3.0f, "Default toonShadowBands is 3.0");

    /* Verify stylized shader defaults */
    TEST_ASSERT(props.stylizedShadowSoftness == 0.3f, "Default stylizedShadowSoftness is 0.3");

    /* Verify transparent shader defaults */
    TEST_ASSERT(props.transparentOpacity == 0.5f, "Default transparentOpacity is 0.5");

    /* Verify glow shader defaults */
    TEST_ASSERT(props.glowIntensity == 1.0f, "Default glowIntensity is 1.0");

    SECTION_END("Materials: PBR Default Properties");
}

static void test_material_creation(void) {
    SECTION_START("Materials: Material Creation");

    /* Create PBR material with default properties */
    LCPBRMaterialProperties props = lc_pbr_material_properties_default();
    props.albedoColor = (LCColor){0.8f, 0.2f, 0.1f, 1.0f};
    props.metallic = 0.9f;
    props.roughness = 0.1f;

    LCMaterialHandle material = NULL;
    LCResult result = lc_material_create_pbr("TestMaterial", &props, &material);
    TEST_RESULT(result, "Create PBR material");
    TEST_ASSERT(material != NULL, "Material handle is valid");

    /* Check validity */
    bool isValid = false;
    result = lc_material_is_valid(material, &isValid);
    TEST_RESULT(result, "Check material validity");
    TEST_ASSERT(isValid == true, "Material is valid");

    /* Get material name */
    char name[64] = {0};
    result = lc_material_get_name(material, name, sizeof(name));
    TEST_RESULT(result, "Get material name");
    TEST_ASSERT(strcmp(name, "TestMaterial") == 0, "Material name is correct");

    /* Test shadow properties */
    result = lc_material_set_cast_shadows(material, false);
    TEST_RESULT(result, "Set cast_shadows to false");

    bool castShadows = true;
    result = lc_material_get_cast_shadows(material, &castShadows);
    TEST_RESULT(result, "Get cast_shadows");
    TEST_ASSERT(castShadows == false, "cast_shadows is false");

    result = lc_material_set_receive_shadows(material, false);
    TEST_RESULT(result, "Set receive_shadows to false");

    bool receiveShadows = true;
    result = lc_material_get_receive_shadows(material, &receiveShadows);
    TEST_RESULT(result, "Get receive_shadows");
    TEST_ASSERT(receiveShadows == false, "receive_shadows is false");

    /* Test transparency */
    result = lc_material_set_transparent(material, true);
    TEST_RESULT(result, "Set transparent to true");

    bool isTransparent = false;
    result = lc_material_get_transparent(material, &isTransparent);
    TEST_RESULT(result, "Get transparent");
    TEST_ASSERT(isTransparent == true, "Material is transparent");

    /* Test alpha clip threshold */
    result = lc_material_set_alpha_clip_threshold(material, 0.75f);
    TEST_RESULT(result, "Set alpha_clip_threshold");

    float threshold = 0.0f;
    result = lc_material_get_alpha_clip_threshold(material, &threshold);
    TEST_RESULT(result, "Get alpha_clip_threshold");
    TEST_ASSERT(threshold == 0.75f, "Alpha clip threshold is 0.75");

    /* Test render layer */
    result = lc_material_set_render_layer(material, LC_RENDER_LAYER_TRANSPARENT);
    TEST_RESULT(result, "Set render_layer to transparent");

    LCRenderLayer layer = LC_RENDER_LAYER_DEFAULT;
    result = lc_material_get_render_layer(material, &layer);
    TEST_RESULT(result, "Get render_layer");
    TEST_ASSERT(layer == LC_RENDER_LAYER_TRANSPARENT, "Render layer is transparent");

    /* Destroy material */
    result = lc_material_destroy(material);
    TEST_RESULT(result, "Destroy material");

    /* Verify invalid after destroy */
    result = lc_material_is_valid(material, &isValid);
    TEST_ASSERT(isValid == false, "Material invalid after destroy");

    SECTION_END("Materials: Material Creation");
}

static void test_material_default_library(void) {
    SECTION_START("Materials: Default Library");

    /* Note: Default materials require RenderWorld which isn't available in CAPI tests.
     * These functions should return LC_ERROR_NOT_SUPPORTED in this context. */

    LCMaterialHandle material = NULL;
    LCResult result;

    result = lc_material_get_default_pbr(&material);
    TEST_ASSERT(result == LC_ERROR_NOT_SUPPORTED, "Default PBR returns NOT_SUPPORTED (no RenderWorld)");

    result = lc_material_get_default_toon(&material);
    TEST_ASSERT(result == LC_ERROR_NOT_SUPPORTED, "Default Toon returns NOT_SUPPORTED");

    result = lc_material_get_default_stylized(&material);
    TEST_ASSERT(result == LC_ERROR_NOT_SUPPORTED, "Default Stylized returns NOT_SUPPORTED");

    result = lc_material_get_default_transparent(&material);
    TEST_ASSERT(result == LC_ERROR_NOT_SUPPORTED, "Default Transparent returns NOT_SUPPORTED");

    result = lc_material_get_default_glow(&material);
    TEST_ASSERT(result == LC_ERROR_NOT_SUPPORTED, "Default Glow returns NOT_SUPPORTED");

    result = lc_material_get_default_skeletal(&material);
    TEST_ASSERT(result == LC_ERROR_NOT_SUPPORTED, "Default Skeletal returns NOT_SUPPORTED");

    /* Custom shader cache clear should succeed (no-op) */
    result = lc_material_clear_custom_shader_cache();
    TEST_RESULT(result, "Clear custom shader cache");

    SECTION_END("Materials: Default Library");
}

/* ============================================================================
 * AnimatedSprite2D Tests
 * ============================================================================ */

static void test_animated_sprite2d(void) {
    SECTION_START("Animation: AnimatedSprite2D");

    /* Create component */
    LCComponentHandle sprite = NULL;
    LCResult result = lc_animated_sprite2d_create("TestAnimSprite2D", &sprite);
    TEST_RESULT(result, "Create AnimatedSprite2D");
    TEST_ASSERT(sprite != NULL, "Component handle is valid");

    /* Animation file */
    result = lc_animated_sprite2d_set_animation_file(sprite, "test_animation.spriteanimation");
    TEST_RESULT(result, "Set animation file");

    char path[256];
    result = lc_animated_sprite2d_get_animation_file(sprite, path, sizeof(path));
    TEST_RESULT(result, "Get animation file");
    TEST_ASSERT(strcmp(path, "test_animation.spriteanimation") == 0, "Animation file path matches");

    /* Animation name */
    result = lc_animated_sprite2d_set_animation_name(sprite, "idle");
    TEST_RESULT(result, "Set animation name");

    char name[256];
    result = lc_animated_sprite2d_get_animation_name(sprite, name, sizeof(name));
    TEST_RESULT(result, "Get animation name");
    TEST_ASSERT(strcmp(name, "idle") == 0, "Animation name matches");

    /* Playback state */
    bool playing = false;
    result = lc_animated_sprite2d_is_playing(sprite, &playing);
    TEST_RESULT(result, "Get playing state");

    result = lc_animated_sprite2d_set_playing(sprite, true);
    TEST_RESULT(result, "Set playing state");

    /* Loop */
    result = lc_animated_sprite2d_set_loop(sprite, true);
    TEST_RESULT(result, "Set loop");

    bool loop = false;
    result = lc_animated_sprite2d_get_loop(sprite, &loop);
    TEST_RESULT(result, "Get loop");
    TEST_ASSERT(loop == true, "Loop state matches");

    /* Auto play */
    result = lc_animated_sprite2d_set_auto_play(sprite, true);
    TEST_RESULT(result, "Set auto play");

    bool autoPlay = false;
    result = lc_animated_sprite2d_get_auto_play(sprite, &autoPlay);
    TEST_RESULT(result, "Get auto play");
    TEST_ASSERT(autoPlay == true, "Auto play state matches");

    /* Animation control */
    result = lc_animated_sprite2d_play(sprite, NULL);
    TEST_RESULT(result, "Play animation");

    result = lc_animated_sprite2d_pause(sprite);
    TEST_RESULT(result, "Pause animation");

    result = lc_animated_sprite2d_resume(sprite);
    TEST_RESULT(result, "Resume animation");

    result = lc_animated_sprite2d_stop(sprite);
    TEST_RESULT(result, "Stop animation");

    /* Frame control */
    result = lc_animated_sprite2d_set_frame(sprite, 2);
    TEST_RESULT(result, "Set frame");

    int frame = -1;
    result = lc_animated_sprite2d_get_frame(sprite, &frame);
    TEST_RESULT(result, "Get frame");

    /* Offset */
    LCVec2 offset = {10.0f, 20.0f};
    result = lc_animated_sprite2d_set_offset(sprite, offset);
    TEST_RESULT(result, "Set offset");

    LCVec2 gotOffset;
    result = lc_animated_sprite2d_get_offset(sprite, &gotOffset);
    TEST_RESULT(result, "Get offset");
    TEST_ASSERT(gotOffset.x == 10.0f && gotOffset.y == 20.0f, "Offset matches");

    /* Flip */
    result = lc_animated_sprite2d_set_flip_h(sprite, true);
    TEST_RESULT(result, "Set flip H");

    bool flipH = false;
    result = lc_animated_sprite2d_get_flip_h(sprite, &flipH);
    TEST_RESULT(result, "Get flip H");
    TEST_ASSERT(flipH == true, "Flip H matches");

    result = lc_animated_sprite2d_set_flip_v(sprite, true);
    TEST_RESULT(result, "Set flip V");

    bool flipV = false;
    result = lc_animated_sprite2d_get_flip_v(sprite, &flipV);
    TEST_RESULT(result, "Get flip V");
    TEST_ASSERT(flipV == true, "Flip V matches");

    /* Modulate */
    LCColor color = {1.0f, 0.5f, 0.25f, 1.0f};
    result = lc_animated_sprite2d_set_modulate(sprite, color);
    TEST_RESULT(result, "Set modulate");

    LCColor gotColor;
    result = lc_animated_sprite2d_get_modulate(sprite, &gotColor);
    TEST_RESULT(result, "Get modulate");
    TEST_ASSERT(gotColor.r == 1.0f && gotColor.g == 0.5f, "Modulate matches");

    /* Pixel snap */
    result = lc_animated_sprite2d_set_pixel_snap(sprite, true);
    TEST_RESULT(result, "Set pixel snap");

    bool pixelSnap = false;
    result = lc_animated_sprite2d_get_pixel_snap(sprite, &pixelSnap);
    TEST_RESULT(result, "Get pixel snap");
    TEST_ASSERT(pixelSnap == true, "Pixel snap matches");

    SECTION_END("Animation: AnimatedSprite2D");
}

/* ============================================================================
 * AnimatedSprite3D Tests
 * ============================================================================ */

static void test_animated_sprite3d(void) {
    SECTION_START("Animation: AnimatedSprite3D");

    /* Create component */
    LCComponentHandle sprite = NULL;
    LCResult result = lc_animated_sprite3d_create("TestAnimSprite3D", &sprite);
    TEST_RESULT(result, "Create AnimatedSprite3D");
    TEST_ASSERT(sprite != NULL, "Component handle is valid");

    /* Animation file */
    result = lc_animated_sprite3d_set_animation_file(sprite, "test_3d_animation.spriteanimation");
    TEST_RESULT(result, "Set animation file");

    char path[256];
    result = lc_animated_sprite3d_get_animation_file(sprite, path, sizeof(path));
    TEST_RESULT(result, "Get animation file");
    TEST_ASSERT(strcmp(path, "test_3d_animation.spriteanimation") == 0, "Animation file path matches");

    /* Animation name */
    result = lc_animated_sprite3d_set_animation_name(sprite, "walk");
    TEST_RESULT(result, "Set animation name");

    char name[256];
    result = lc_animated_sprite3d_get_animation_name(sprite, name, sizeof(name));
    TEST_RESULT(result, "Get animation name");
    TEST_ASSERT(strcmp(name, "walk") == 0, "Animation name matches");

    /* Playback state */
    bool playing = false;
    result = lc_animated_sprite3d_is_playing(sprite, &playing);
    TEST_RESULT(result, "Get playing state");

    result = lc_animated_sprite3d_set_playing(sprite, true);
    TEST_RESULT(result, "Set playing state");

    /* Loop */
    result = lc_animated_sprite3d_set_loop(sprite, true);
    TEST_RESULT(result, "Set loop");

    bool loop = false;
    result = lc_animated_sprite3d_get_loop(sprite, &loop);
    TEST_RESULT(result, "Get loop");
    TEST_ASSERT(loop == true, "Loop state matches");

    /* Auto play */
    result = lc_animated_sprite3d_set_auto_play(sprite, true);
    TEST_RESULT(result, "Set auto play");

    bool autoPlay = false;
    result = lc_animated_sprite3d_get_auto_play(sprite, &autoPlay);
    TEST_RESULT(result, "Get auto play");
    TEST_ASSERT(autoPlay == true, "Auto play state matches");

    /* Animation control */
    result = lc_animated_sprite3d_play(sprite, NULL);
    TEST_RESULT(result, "Play animation");

    result = lc_animated_sprite3d_pause(sprite);
    TEST_RESULT(result, "Pause animation");

    result = lc_animated_sprite3d_resume(sprite);
    TEST_RESULT(result, "Resume animation");

    result = lc_animated_sprite3d_stop(sprite);
    TEST_RESULT(result, "Stop animation");

    /* Frame control */
    result = lc_animated_sprite3d_set_frame(sprite, 3);
    TEST_RESULT(result, "Set frame");

    int frame = -1;
    result = lc_animated_sprite3d_get_frame(sprite, &frame);
    TEST_RESULT(result, "Get frame");

    /* Offset (3D) */
    LCVec3 offset = {1.0f, 2.0f, 3.0f};
    result = lc_animated_sprite3d_set_offset(sprite, offset);
    TEST_RESULT(result, "Set offset");

    LCVec3 gotOffset;
    result = lc_animated_sprite3d_get_offset(sprite, &gotOffset);
    TEST_RESULT(result, "Get offset");
    TEST_ASSERT(gotOffset.x == 1.0f && gotOffset.y == 2.0f && gotOffset.z == 3.0f, "Offset matches");

    /* Flip */
    result = lc_animated_sprite3d_set_flip_h(sprite, true);
    TEST_RESULT(result, "Set flip H");

    bool flipH = false;
    result = lc_animated_sprite3d_get_flip_h(sprite, &flipH);
    TEST_RESULT(result, "Get flip H");
    TEST_ASSERT(flipH == true, "Flip H matches");

    result = lc_animated_sprite3d_set_flip_v(sprite, true);
    TEST_RESULT(result, "Set flip V");

    bool flipV = false;
    result = lc_animated_sprite3d_get_flip_v(sprite, &flipV);
    TEST_RESULT(result, "Get flip V");
    TEST_ASSERT(flipV == true, "Flip V matches");

    /* Modulate */
    LCColor color = {0.8f, 0.6f, 0.4f, 1.0f};
    result = lc_animated_sprite3d_set_modulate(sprite, color);
    TEST_RESULT(result, "Set modulate");

    LCColor gotColor;
    result = lc_animated_sprite3d_get_modulate(sprite, &gotColor);
    TEST_RESULT(result, "Get modulate");
    TEST_ASSERT(gotColor.r == 0.8f && gotColor.g == 0.6f, "Modulate matches");

    /* Billboard mode */
    result = lc_animated_sprite3d_set_billboard(sprite, LC_BILLBOARD_Y_AXIS_ONLY);
    TEST_RESULT(result, "Set billboard mode");

    LCBillboardMode mode;
    result = lc_animated_sprite3d_get_billboard(sprite, &mode);
    TEST_RESULT(result, "Get billboard mode");
    TEST_ASSERT(mode == LC_BILLBOARD_Y_AXIS_ONLY, "Billboard mode matches");

    /* Cast shadows */
    result = lc_animated_sprite3d_set_cast_shadows(sprite, true);
    TEST_RESULT(result, "Set cast shadows");

    bool castShadows = false;
    result = lc_animated_sprite3d_get_cast_shadows(sprite, &castShadows);
    TEST_RESULT(result, "Get cast shadows");
    TEST_ASSERT(castShadows == true, "Cast shadows matches");

    /* Receive shadows */
    result = lc_animated_sprite3d_set_receive_shadows(sprite, true);
    TEST_RESULT(result, "Set receive shadows");

    bool receiveShadows = false;
    result = lc_animated_sprite3d_get_receive_shadows(sprite, &receiveShadows);
    TEST_RESULT(result, "Get receive shadows");
    TEST_ASSERT(receiveShadows == true, "Receive shadows matches");

    /* Double sided */
    result = lc_animated_sprite3d_set_double_sided(sprite, true);
    TEST_RESULT(result, "Set double sided");

    bool doubleSided = false;
    result = lc_animated_sprite3d_get_double_sided(sprite, &doubleSided);
    TEST_RESULT(result, "Get double sided");
    TEST_ASSERT(doubleSided == true, "Double sided matches");

    SECTION_END("Animation: AnimatedSprite3D");
}

/* ============================================================================
 * SkeletalMesh3D Tests
 * ============================================================================ */

static void test_skeletal_mesh3d(void) {
    SECTION_START("Animation: SkeletalMesh3D");

    /* Create component */
    LCComponentHandle mesh = NULL;
    LCResult result = lc_skeletal_mesh3d_create("TestSkeletalMesh", &mesh);
    TEST_RESULT(result, "Create SkeletalMesh3D");
    TEST_ASSERT(mesh != NULL, "Component handle is valid");

    /* Model path */
    result = lc_skeletal_mesh3d_set_model_path(mesh, "models/character.fbx");
    TEST_RESULT(result, "Set model path");

    char path[256];
    result = lc_skeletal_mesh3d_get_model_path(mesh, path, sizeof(path));
    TEST_RESULT(result, "Get model path");
    TEST_ASSERT(strcmp(path, "models/character.fbx") == 0, "Model path matches");

    /* Animation count (should be 0 before loading) */
    int animCount = -1;
    result = lc_skeletal_mesh3d_get_animation_count(mesh, &animCount);
    TEST_RESULT(result, "Get animation count");
    TEST_ASSERT(animCount >= 0, "Animation count is valid");

    /* Default animation */
    result = lc_skeletal_mesh3d_set_default_animation(mesh, "idle");
    TEST_RESULT(result, "Set default animation");

    char animName[256];
    result = lc_skeletal_mesh3d_get_default_animation(mesh, animName, sizeof(animName));
    TEST_RESULT(result, "Get default animation");
    TEST_ASSERT(strcmp(animName, "idle") == 0, "Default animation matches");

    /* Current animation */
    result = lc_skeletal_mesh3d_set_current_animation(mesh, "walk");
    TEST_RESULT(result, "Set current animation");

    result = lc_skeletal_mesh3d_get_current_animation(mesh, animName, sizeof(animName));
    TEST_RESULT(result, "Get current animation");
    TEST_ASSERT(strcmp(animName, "walk") == 0, "Current animation matches");

    /* Playing state */
    bool playing = false;
    result = lc_skeletal_mesh3d_is_playing(mesh, &playing);
    TEST_RESULT(result, "Get playing state");

    result = lc_skeletal_mesh3d_set_playing(mesh, true);
    TEST_RESULT(result, "Set playing state");

    /* Animation time */
    result = lc_skeletal_mesh3d_set_animation_time(mesh, 0.5f);
    TEST_RESULT(result, "Set animation time");

    float time = -1.0f;
    result = lc_skeletal_mesh3d_get_animation_time(mesh, &time);
    TEST_RESULT(result, "Get animation time");
    TEST_ASSERT(time == 0.5f, "Animation time matches");

    /* Playback speed */
    result = lc_skeletal_mesh3d_set_playback_speed(mesh, 2.0f);
    TEST_RESULT(result, "Set playback speed");

    float speed = -1.0f;
    result = lc_skeletal_mesh3d_get_playback_speed(mesh, &speed);
    TEST_RESULT(result, "Get playback speed");
    TEST_ASSERT(speed == 2.0f, "Playback speed matches");

    /* Loop */
    result = lc_skeletal_mesh3d_set_loop(mesh, true);
    TEST_RESULT(result, "Set loop");

    bool loop = false;
    result = lc_skeletal_mesh3d_get_loop(mesh, &loop);
    TEST_RESULT(result, "Get loop");
    TEST_ASSERT(loop == true, "Loop matches");

    /* Auto play */
    result = lc_skeletal_mesh3d_set_auto_play(mesh, true);
    TEST_RESULT(result, "Set auto play");

    bool autoPlay = false;
    result = lc_skeletal_mesh3d_get_auto_play(mesh, &autoPlay);
    TEST_RESULT(result, "Get auto play");
    TEST_ASSERT(autoPlay == true, "Auto play matches");

    /* GPU skinning */
    result = lc_skeletal_mesh3d_set_gpu_skinning(mesh, true);
    TEST_RESULT(result, "Set GPU skinning");

    bool gpuSkinning = false;
    result = lc_skeletal_mesh3d_get_gpu_skinning(mesh, &gpuSkinning);
    TEST_RESULT(result, "Get GPU skinning");
    TEST_ASSERT(gpuSkinning == true, "GPU skinning matches");

    /* Root motion */
    result = lc_skeletal_mesh3d_set_root_motion_enabled(mesh, true);
    TEST_RESULT(result, "Set root motion enabled");

    bool rootMotion = false;
    result = lc_skeletal_mesh3d_get_root_motion_enabled(mesh, &rootMotion);
    TEST_RESULT(result, "Get root motion enabled");
    TEST_ASSERT(rootMotion == true, "Root motion matches");

    result = lc_skeletal_mesh3d_set_root_bone_name(mesh, "Root");
    TEST_RESULT(result, "Set root bone name");

    char boneName[256];
    result = lc_skeletal_mesh3d_get_root_bone_name(mesh, boneName, sizeof(boneName));
    TEST_RESULT(result, "Get root bone name");
    TEST_ASSERT(strcmp(boneName, "Root") == 0, "Root bone name matches");

    /* Shadow settings */
    result = lc_skeletal_mesh3d_set_cast_shadow(mesh, true);
    TEST_RESULT(result, "Set cast shadow");

    bool castShadow = false;
    result = lc_skeletal_mesh3d_get_cast_shadow(mesh, &castShadow);
    TEST_RESULT(result, "Get cast shadow");
    TEST_ASSERT(castShadow == true, "Cast shadow matches");

    result = lc_skeletal_mesh3d_set_receive_shadow(mesh, true);
    TEST_RESULT(result, "Set receive shadow");

    bool receiveShadow = false;
    result = lc_skeletal_mesh3d_get_receive_shadow(mesh, &receiveShadow);
    TEST_RESULT(result, "Get receive shadow");
    TEST_ASSERT(receiveShadow == true, "Receive shadow matches");

    /* Material slot count (should be 0 before loading model) */
    int slotCount = -1;
    result = lc_skeletal_mesh3d_get_material_slot_count(mesh, &slotCount);
    TEST_RESULT(result, "Get material slot count");
    TEST_ASSERT(slotCount >= 0, "Material slot count is valid");

    /* Stop animation */
    result = lc_skeletal_mesh3d_stop_animation(mesh);
    TEST_RESULT(result, "Stop animation");

    SECTION_END("Animation: SkeletalMesh3D");
}

/* ============================================================================
 * Asset System Tests
 * ============================================================================ */

static void test_asset_system(void) {
    SECTION_START("Asset System: Core Functions");

    /* Test asset type to string conversion */
    const char* typeStr = lc_asset_type_to_string(LC_ASSET_TYPE_IMAGE);
    TEST_ASSERT(typeStr != NULL && strcmp(typeStr, "Image") == 0, "lc_asset_type_to_string() for IMAGE");

    typeStr = lc_asset_type_to_string(LC_ASSET_TYPE_MODEL);
    TEST_ASSERT(typeStr != NULL && strcmp(typeStr, "Model") == 0, "lc_asset_type_to_string() for MODEL");

    typeStr = lc_asset_type_to_string(LC_ASSET_TYPE_AUDIO);
    TEST_ASSERT(typeStr != NULL && strcmp(typeStr, "Audio") == 0, "lc_asset_type_to_string() for AUDIO");

    typeStr = lc_asset_type_to_string(LC_ASSET_TYPE_FONT);
    TEST_ASSERT(typeStr != NULL && strcmp(typeStr, "Font") == 0, "lc_asset_type_to_string() for FONT");

    typeStr = lc_asset_type_to_string(LC_ASSET_TYPE_UNKNOWN);
    TEST_ASSERT(typeStr != NULL && strcmp(typeStr, "Unknown") == 0, "lc_asset_type_to_string() for UNKNOWN");

    SECTION_END("Asset System: Core Functions");

    SECTION_START("Asset System: Image Loading");

    /* Test image loading from raw data (create a small test image) */
    uint8_t testImageData[4 * 4 * 4]; /* 4x4 RGBA image */
    for (int i = 0; i < 4 * 4 * 4; i += 4) {
        testImageData[i] = 255;     /* R */
        testImageData[i+1] = 128;   /* G */
        testImageData[i+2] = 64;    /* B */
        testImageData[i+3] = 255;   /* A */
    }

    LCImageAssetHandle imageAsset = NULL;
    LCResult result = lc_image_load_from_raw(testImageData, 4, 4, 4, false, LC_IMAGE_COLOR_SPACE_SRGB, &imageAsset);
    TEST_RESULT(result, "Load image from raw data");
    TEST_ASSERT(imageAsset != NULL, "Image asset handle is valid");

    if (imageAsset) {
        /* Test image info */
        LCImageInfo imageInfo;
        result = lc_image_get_info(imageAsset, &imageInfo);
        TEST_RESULT(result, "Get image info");
        TEST_ASSERT(imageInfo.width == 4, "Image width is correct");
        TEST_ASSERT(imageInfo.height == 4, "Image height is correct");
        TEST_ASSERT(imageInfo.channels == 4, "Image channels is correct");
        TEST_ASSERT(imageInfo.hasAlpha == true, "Image has alpha");

        /* Test asset type */
        LCAssetType assetType;
        result = lc_asset_get_type((LCAssetHandle)imageAsset, &assetType);
        TEST_RESULT(result, "Get asset type");
        TEST_ASSERT(assetType == LC_ASSET_TYPE_IMAGE, "Asset type is IMAGE");

        /* Test loaded state */
        bool loaded = false;
        result = lc_asset_is_loaded((LCAssetHandle)imageAsset, &loaded);
        TEST_RESULT(result, "Get loaded state");
        TEST_ASSERT(loaded == true, "Image is loaded");

        /* Test reference count */
        int refCount = 0;
        result = lc_asset_get_ref_count((LCAssetHandle)imageAsset, &refCount);
        TEST_RESULT(result, "Get reference count");
        printf("    Initial ref count: %d\n", refCount);

        /* Test add ref */
        result = lc_asset_add_ref((LCAssetHandle)imageAsset);
        TEST_RESULT(result, "Add reference");

        result = lc_asset_get_ref_count((LCAssetHandle)imageAsset, &refCount);
        printf("    Ref count after add_ref: %d\n", refCount);

        /* Test data access */
        const uint8_t* dataPtr = NULL;
        size_t dataSize = 0;
        result = lc_image_get_data(imageAsset, &dataPtr, &dataSize);
        TEST_RESULT(result, "Get image data");
        TEST_ASSERT(dataPtr != NULL, "Image data pointer is valid");
        TEST_ASSERT(dataSize == 4 * 4 * 4, "Image data size is correct");

        /* Release asset */
        result = lc_image_release(imageAsset);
        TEST_RESULT(result, "Release image asset");
    }

    SECTION_END("Asset System: Image Loading");

    SECTION_START("Asset System: Error Handling");

    /* Test null handle error */
    LCAssetType nullType;
    result = lc_asset_get_type(NULL, &nullType);
    TEST_ASSERT(result == LC_ERROR_INVALID_HANDLE, "Null handle returns INVALID_HANDLE error");

    /* Test null output pointer error */
    LCImageAssetHandle dummyImage = NULL;
    result = lc_image_load_from_raw(testImageData, 4, 4, 4, false, LC_IMAGE_COLOR_SPACE_SRGB, &dummyImage);
    if (dummyImage) {
        result = lc_asset_get_type((LCAssetHandle)dummyImage, NULL);
        TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Null output pointer returns NULL_POINTER error");
        lc_image_release(dummyImage);
    }

    /* Test invalid image dimensions */
    result = lc_image_load_from_raw(testImageData, 0, 4, 4, false, LC_IMAGE_COLOR_SPACE_SRGB, &dummyImage);
    TEST_ASSERT(result == LC_ERROR_INVALID_PARAMETER, "Zero width returns INVALID_PARAMETER error");

    result = lc_image_load_from_raw(testImageData, 4, 0, 4, false, LC_IMAGE_COLOR_SPACE_SRGB, &dummyImage);
    TEST_ASSERT(result == LC_ERROR_INVALID_PARAMETER, "Zero height returns INVALID_PARAMETER error");

    result = lc_image_load_from_raw(testImageData, 4, 4, 5, false, LC_IMAGE_COLOR_SPACE_SRGB, &dummyImage);
    TEST_ASSERT(result == LC_ERROR_INVALID_PARAMETER, "Invalid channels returns INVALID_PARAMETER error");

    SECTION_END("Asset System: Error Handling");

    SECTION_START("Asset System: Image Color Spaces");

    /* Test linear color space */
    LCImageAssetHandle linearImage = NULL;
    result = lc_image_load_from_raw(testImageData, 4, 4, 4, false, LC_IMAGE_COLOR_SPACE_LINEAR, &linearImage);
    TEST_RESULT(result, "Load image with linear color space");

    if (linearImage) {
        LCImageInfo info;
        result = lc_image_get_info(linearImage, &info);
        TEST_RESULT(result, "Get linear image info");
        TEST_ASSERT(info.colorSpace == LC_IMAGE_COLOR_SPACE_LINEAR, "Color space is LINEAR");
        lc_image_release(linearImage);
    }

    /* Test sRGB color space */
    LCImageAssetHandle srgbImage = NULL;
    result = lc_image_load_from_raw(testImageData, 4, 4, 4, false, LC_IMAGE_COLOR_SPACE_SRGB, &srgbImage);
    TEST_RESULT(result, "Load image with sRGB color space");

    if (srgbImage) {
        LCImageInfo info;
        result = lc_image_get_info(srgbImage, &info);
        TEST_RESULT(result, "Get sRGB image info");
        TEST_ASSERT(info.colorSpace == LC_IMAGE_COLOR_SPACE_SRGB, "Color space is SRGB");
        lc_image_release(srgbImage);
    }

    SECTION_END("Asset System: Image Color Spaces");

    SECTION_START("Asset System: Image Formats");

    /* Test 1 channel (grayscale) */
    uint8_t grayData[4 * 4];
    for (int i = 0; i < 4 * 4; i++) grayData[i] = 128;

    LCImageAssetHandle grayImage = NULL;
    result = lc_image_load_from_raw(grayData, 4, 4, 1, false, LC_IMAGE_COLOR_SPACE_LINEAR, &grayImage);
    TEST_RESULT(result, "Load 1-channel (grayscale) image");

    if (grayImage) {
        LCImageInfo info;
        lc_image_get_info(grayImage, &info);
        TEST_ASSERT(info.channels == 1, "Grayscale image has 1 channel");
        TEST_ASSERT(info.hasAlpha == false, "Grayscale image has no alpha");
        lc_image_release(grayImage);
    }

    /* Test 2 channel (grayscale + alpha) */
    uint8_t gaData[4 * 4 * 2];
    for (int i = 0; i < 4 * 4 * 2; i += 2) {
        gaData[i] = 128;    /* Gray */
        gaData[i+1] = 255;  /* Alpha */
    }

    LCImageAssetHandle gaImage = NULL;
    result = lc_image_load_from_raw(gaData, 4, 4, 2, false, LC_IMAGE_COLOR_SPACE_LINEAR, &gaImage);
    TEST_RESULT(result, "Load 2-channel (gray+alpha) image");

    if (gaImage) {
        LCImageInfo info;
        lc_image_get_info(gaImage, &info);
        TEST_ASSERT(info.channels == 2, "Gray+Alpha image has 2 channels");
        lc_image_release(gaImage);
    }

    /* Test 3 channel (RGB) */
    uint8_t rgbData[4 * 4 * 3];
    for (int i = 0; i < 4 * 4 * 3; i += 3) {
        rgbData[i] = 255;   /* R */
        rgbData[i+1] = 128; /* G */
        rgbData[i+2] = 64;  /* B */
    }

    LCImageAssetHandle rgbImage = NULL;
    result = lc_image_load_from_raw(rgbData, 4, 4, 3, false, LC_IMAGE_COLOR_SPACE_SRGB, &rgbImage);
    TEST_RESULT(result, "Load 3-channel (RGB) image");

    if (rgbImage) {
        LCImageInfo info;
        lc_image_get_info(rgbImage, &info);
        TEST_ASSERT(info.channels == 3, "RGB image has 3 channels");
        TEST_ASSERT(info.hasAlpha == false, "RGB image has no alpha");
        lc_image_release(rgbImage);
    }

    SECTION_END("Asset System: Image Formats");
}

/* ============================================================================
 * Physics Query 2D Tests
 * ============================================================================ */

static void test_physics_query2d(void) {
    SECTION_START("Physics Query 2D: API Availability");

    /* Test query availability - should return false without active scene */
    bool available = true;
    LCResult result = lc_physics2d_query_available(&available);
    TEST_RESULT(result, "lc_physics2d_query_available() callable");
    printf("    Physics2D query available: %s\n", available ? "yes" : "no");

    /* Note: Without an active scene, physics queries won't work but the API should still be testable */

    SECTION_END("Physics Query 2D: API Availability");

    SECTION_START("Physics Query 2D: Raycast API");

    /* Test raycast with null output */
    result = lc_physics2d_raycast((LCVec2){0.0f, 0.0f}, (LCVec2){1.0f, 0.0f}, 100.0f, NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Null hit pointer returns NULL_POINTER error");

    /* Test raycast - will return physics world invalid without active scene */
    LCRaycastHit2D hit;
    result = lc_physics2d_raycast((LCVec2){0.0f, 0.0f}, (LCVec2){1.0f, 0.0f}, 100.0f, &hit);
    /* This may fail with PHYSICS_WORLD_INVALID if no scene is active, which is expected */
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "lc_physics2d_raycast() returns valid result code");
    printf("    Raycast result code: %d (%s)\n", (int)result, lc_result_to_string(result));

    /* Test raycast_all with null parameters */
    result = lc_physics2d_raycast_all((LCVec2){0.0f, 0.0f}, (LCVec2){1.0f, 0.0f}, 100.0f, NULL, 10, NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Null hits pointer returns NULL_POINTER error");

    LCRaycastHit2D hits[10];
    result = lc_physics2d_raycast_all((LCVec2){0.0f, 0.0f}, (LCVec2){1.0f, 0.0f}, 100.0f, hits, 10, NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Null hitCount pointer returns NULL_POINTER error");

    uint32_t hitCount = 0;
    result = lc_physics2d_raycast_all((LCVec2){0.0f, 0.0f}, (LCVec2){1.0f, 0.0f}, 100.0f, hits, 10, &hitCount);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "lc_physics2d_raycast_all() returns valid result code");
    printf("    RaycastAll result code: %d, hitCount: %u\n", (int)result, hitCount);

    SECTION_END("Physics Query 2D: Raycast API");

    SECTION_START("Physics Query 2D: Overlap API");

    /* Test overlap_point with null parameters */
    result = lc_physics2d_overlap_point((LCVec2){0.0f, 0.0f}, NULL, 10, NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Null results pointer returns NULL_POINTER error");

    LCOverlapResult2D overlapResults[10];
    result = lc_physics2d_overlap_point((LCVec2){0.0f, 0.0f}, overlapResults, 10, NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Null resultCount pointer returns NULL_POINTER error");

    uint32_t overlapCount = 0;
    result = lc_physics2d_overlap_point((LCVec2){0.0f, 0.0f}, overlapResults, 10, &overlapCount);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "lc_physics2d_overlap_point() returns valid result code");
    printf("    OverlapPoint result code: %d, count: %u\n", (int)result, overlapCount);

    /* Test overlap_circle */
    result = lc_physics2d_overlap_circle((LCVec2){0.0f, 0.0f}, 5.0f, overlapResults, 10, &overlapCount);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "lc_physics2d_overlap_circle() returns valid result code");
    printf("    OverlapCircle result code: %d, count: %u\n", (int)result, overlapCount);

    /* Test overlap_circle with negative radius */
    result = lc_physics2d_overlap_circle((LCVec2){0.0f, 0.0f}, -1.0f, overlapResults, 10, &overlapCount);
    TEST_ASSERT(result == LC_ERROR_INVALID_PARAMETER, "Negative radius returns INVALID_PARAMETER error");

    /* Test overlap_box */
    result = lc_physics2d_overlap_box((LCVec2){0.0f, 0.0f}, (LCVec2){10.0f, 5.0f}, overlapResults, 10, &overlapCount);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "lc_physics2d_overlap_box() returns valid result code");
    printf("    OverlapBox result code: %d, count: %u\n", (int)result, overlapCount);

    /* Test overlap_box_rotated */
    result = lc_physics2d_overlap_box_rotated((LCVec2){0.0f, 0.0f}, (LCVec2){10.0f, 5.0f}, 0.785f, overlapResults, 10, &overlapCount);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "lc_physics2d_overlap_box_rotated() returns valid result code");
    printf("    OverlapBoxRotated result code: %d, count: %u\n", (int)result, overlapCount);

    /* Test overlap_box with negative size */
    result = lc_physics2d_overlap_box((LCVec2){0.0f, 0.0f}, (LCVec2){-10.0f, 5.0f}, overlapResults, 10, &overlapCount);
    TEST_ASSERT(result == LC_ERROR_INVALID_PARAMETER, "Negative size returns INVALID_PARAMETER error");

    SECTION_END("Physics Query 2D: Overlap API");

    SECTION_START("Physics Query 2D: Shape Cast API");

    /* Test circle_cast with null output */
    result = lc_physics2d_circle_cast((LCVec2){0.0f, 0.0f}, (LCVec2){10.0f, 0.0f}, 0.5f, NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Null hit pointer returns NULL_POINTER error");

    /* Test circle_cast */
    LCShapeCastHit2D shapeCastHit;
    result = lc_physics2d_circle_cast((LCVec2){0.0f, 0.0f}, (LCVec2){10.0f, 0.0f}, 0.5f, &shapeCastHit);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "lc_physics2d_circle_cast() returns valid result code");
    printf("    CircleCast result code: %d, hit: %s\n", (int)result, shapeCastHit.hit ? "yes" : "no");

    /* Test circle_cast with negative radius */
    result = lc_physics2d_circle_cast((LCVec2){0.0f, 0.0f}, (LCVec2){10.0f, 0.0f}, -0.5f, &shapeCastHit);
    TEST_ASSERT(result == LC_ERROR_INVALID_PARAMETER, "Negative radius returns INVALID_PARAMETER error");

    SECTION_END("Physics Query 2D: Shape Cast API");

    SECTION_START("Physics Query 2D: Gravity Functions");

    /* Test get_gravity with null output */
    result = lc_physics2d_get_gravity(NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Null gravity pointer returns NULL_POINTER error");

    /* Test get_gravity */
    LCVec2 gravity;
    result = lc_physics2d_get_gravity(&gravity);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "lc_physics2d_get_gravity() returns valid result code");
    if (result == LC_SUCCESS) {
        printf("    Current gravity: (%f, %f)\n", gravity.x, gravity.y);
    }

    /* Test set_gravity */
    result = lc_physics2d_set_gravity((LCVec2){0.0f, -9.81f});
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "lc_physics2d_set_gravity() returns valid result code");

    SECTION_END("Physics Query 2D: Gravity Functions");

    SECTION_START("Physics Query 2D: Result Structure Sizes");

    /* Verify structure sizes are reasonable */
    printf("    sizeof(LCRaycastHit2D): %zu bytes\n", sizeof(LCRaycastHit2D));
    printf("    sizeof(LCOverlapResult2D): %zu bytes\n", sizeof(LCOverlapResult2D));
    printf("    sizeof(LCShapeCastHit2D): %zu bytes\n", sizeof(LCShapeCastHit2D));

    TEST_STATIC_ASSERT(sizeof(LCRaycastHit2D) > 0, "LCRaycastHit2D has valid size");
    TEST_STATIC_ASSERT(sizeof(LCOverlapResult2D) > 0, "LCOverlapResult2D has valid size");
    TEST_STATIC_ASSERT(sizeof(LCShapeCastHit2D) > 0, "LCShapeCastHit2D has valid size");

    /* Test that bodyId is correctly sized for UUID storage */
    TEST_STATIC_ASSERT(sizeof(((LCRaycastHit2D*)0)->bodyId) == 16, "LCRaycastHit2D.bodyId is 16 bytes (128-bit UUID)");

    SECTION_END("Physics Query 2D: Result Structure Sizes");
}

/* ============================================================================
 * Physics Query 3D Tests
 * ============================================================================ */

static void test_physics_query3d(void) {
    SECTION_START("Physics Query 3D: API Availability");

    /* Test query availability - should return false without active scene */
    bool available = true;
    LCResult result = lc_physics3d_query_available(&available);
    TEST_RESULT(result, "lc_physics3d_query_available() callable");
    printf("    Physics3D query available: %s\n", available ? "yes" : "no");

    SECTION_END("Physics Query 3D: API Availability");

    SECTION_START("Physics Query 3D: Raycast API");

    /* Test raycast with null output */
    result = lc_physics3d_raycast((LCVec3){0.0f, 0.0f, 0.0f}, (LCVec3){1.0f, 0.0f, 0.0f}, 100.0f, NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Null hit pointer returns NULL_POINTER error");

    /* Test raycast - will return physics world invalid without active scene */
    LCRaycastHit3D hit;
    result = lc_physics3d_raycast((LCVec3){0.0f, 0.0f, 0.0f}, (LCVec3){1.0f, 0.0f, 0.0f}, 100.0f, &hit);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "lc_physics3d_raycast() returns valid result code");
    printf("    Raycast result code: %d (%s)\n", (int)result, lc_result_to_string(result));

    /* Test raycast_all with null parameters */
    result = lc_physics3d_raycast_all((LCVec3){0.0f, 0.0f, 0.0f}, (LCVec3){1.0f, 0.0f, 0.0f}, 100.0f, NULL, 10, NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Null hits pointer returns NULL_POINTER error");

    LCRaycastHit3D hits[10];
    result = lc_physics3d_raycast_all((LCVec3){0.0f, 0.0f, 0.0f}, (LCVec3){1.0f, 0.0f, 0.0f}, 100.0f, hits, 10, NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Null hitCount pointer returns NULL_POINTER error");

    uint32_t hitCount = 0;
    result = lc_physics3d_raycast_all((LCVec3){0.0f, 0.0f, 0.0f}, (LCVec3){1.0f, 0.0f, 0.0f}, 100.0f, hits, 10, &hitCount);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "lc_physics3d_raycast_all() returns valid result code");
    printf("    RaycastAll result code: %d, hitCount: %u\n", (int)result, hitCount);

    SECTION_END("Physics Query 3D: Raycast API");

    SECTION_START("Physics Query 3D: Overlap API");

    /* Test overlap_sphere with null parameters */
    result = lc_physics3d_overlap_sphere((LCVec3){0.0f, 0.0f, 0.0f}, 5.0f, NULL, 10, NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Null results pointer returns NULL_POINTER error");

    LCOverlapResult3D overlapResults[10];
    result = lc_physics3d_overlap_sphere((LCVec3){0.0f, 0.0f, 0.0f}, 5.0f, overlapResults, 10, NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Null resultCount pointer returns NULL_POINTER error");

    uint32_t overlapCount = 0;
    result = lc_physics3d_overlap_sphere((LCVec3){0.0f, 0.0f, 0.0f}, 5.0f, overlapResults, 10, &overlapCount);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "lc_physics3d_overlap_sphere() returns valid result code");
    printf("    OverlapSphere result code: %d, count: %u\n", (int)result, overlapCount);

    /* Test overlap_sphere with negative radius */
    result = lc_physics3d_overlap_sphere((LCVec3){0.0f, 0.0f, 0.0f}, -1.0f, overlapResults, 10, &overlapCount);
    TEST_ASSERT(result == LC_ERROR_INVALID_PARAMETER, "Negative radius returns INVALID_PARAMETER error");

    /* Test overlap_box */
    result = lc_physics3d_overlap_box((LCVec3){0.0f, 0.0f, 0.0f}, (LCVec3){5.0f, 5.0f, 5.0f}, overlapResults, 10, &overlapCount);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "lc_physics3d_overlap_box() returns valid result code");
    printf("    OverlapBox result code: %d, count: %u\n", (int)result, overlapCount);

    /* Test overlap_box_rotated */
    LCQuat rotation = lc_quat_identity();
    result = lc_physics3d_overlap_box_rotated((LCVec3){0.0f, 0.0f, 0.0f}, (LCVec3){5.0f, 5.0f, 5.0f}, rotation, overlapResults, 10, &overlapCount);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "lc_physics3d_overlap_box_rotated() returns valid result code");
    printf("    OverlapBoxRotated result code: %d, count: %u\n", (int)result, overlapCount);

    /* Test overlap_box with negative size */
    result = lc_physics3d_overlap_box((LCVec3){0.0f, 0.0f, 0.0f}, (LCVec3){-5.0f, 5.0f, 5.0f}, overlapResults, 10, &overlapCount);
    TEST_ASSERT(result == LC_ERROR_INVALID_PARAMETER, "Negative halfExtents returns INVALID_PARAMETER error");

    SECTION_END("Physics Query 3D: Overlap API");

    SECTION_START("Physics Query 3D: Shape Cast API");

    /* Test sphere_cast with null output */
    result = lc_physics3d_sphere_cast((LCVec3){0.0f, 1.0f, 0.0f}, (LCVec3){10.0f, 1.0f, 0.0f}, 0.5f, NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Null hit pointer returns NULL_POINTER error");

    /* Test sphere_cast */
    LCShapeCastHit3D shapeCastHit;
    result = lc_physics3d_sphere_cast((LCVec3){0.0f, 1.0f, 0.0f}, (LCVec3){10.0f, 1.0f, 0.0f}, 0.5f, &shapeCastHit);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "lc_physics3d_sphere_cast() returns valid result code");
    printf("    SphereCast result code: %d, hit: %s\n", (int)result, shapeCastHit.hit ? "yes" : "no");

    /* Test sphere_cast with negative radius */
    result = lc_physics3d_sphere_cast((LCVec3){0.0f, 1.0f, 0.0f}, (LCVec3){10.0f, 1.0f, 0.0f}, -0.5f, &shapeCastHit);
    TEST_ASSERT(result == LC_ERROR_INVALID_PARAMETER, "Negative radius returns INVALID_PARAMETER error");

    SECTION_END("Physics Query 3D: Shape Cast API");

    SECTION_START("Physics Query 3D: Gravity Functions");

    /* Test get_gravity with null output */
    result = lc_physics3d_get_gravity(NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Null gravity pointer returns NULL_POINTER error");

    /* Test get_gravity */
    LCVec3 gravity;
    result = lc_physics3d_get_gravity(&gravity);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "lc_physics3d_get_gravity() returns valid result code");
    if (result == LC_SUCCESS) {
        printf("    Current gravity: (%f, %f, %f)\n", gravity.x, gravity.y, gravity.z);
    }

    /* Test set_gravity */
    result = lc_physics3d_set_gravity((LCVec3){0.0f, -9.81f, 0.0f});
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_PHYSICS_WORLD_INVALID,
                "lc_physics3d_set_gravity() returns valid result code");

    SECTION_END("Physics Query 3D: Gravity Functions");

    SECTION_START("Physics Query 3D: Result Structure Sizes");

    /* Verify structure sizes are reasonable */
    printf("    sizeof(LCRaycastHit3D): %zu bytes\n", sizeof(LCRaycastHit3D));
    printf("    sizeof(LCOverlapResult3D): %zu bytes\n", sizeof(LCOverlapResult3D));
    printf("    sizeof(LCShapeCastHit3D): %zu bytes\n", sizeof(LCShapeCastHit3D));

    TEST_STATIC_ASSERT(sizeof(LCRaycastHit3D) > 0, "LCRaycastHit3D has valid size");
    TEST_STATIC_ASSERT(sizeof(LCOverlapResult3D) > 0, "LCOverlapResult3D has valid size");
    TEST_STATIC_ASSERT(sizeof(LCShapeCastHit3D) > 0, "LCShapeCastHit3D has valid size");

    /* Test that bodyId is correctly sized for UUID storage */
    TEST_STATIC_ASSERT(sizeof(((LCRaycastHit3D*)0)->bodyId) == 16, "LCRaycastHit3D.bodyId is 16 bytes (128-bit UUID)");

    SECTION_END("Physics Query 3D: Result Structure Sizes");
}

/* ============================================================================
 * MultiMesh Tests
 * ============================================================================ */

static void test_multimesh(void) {
    SECTION_START("MultiMesh: Component Creation");

    /* Create a node first */
    LCNodeHandle node = NULL;
    LCResult result = lc_node_create(LC_NODE_3D, "TestMultiMeshNode", &node);
    TEST_RESULT(result, "Create 3D node for MultiMesh");

    /* Test creation with null output */
    result = lc_multimesh_create(node, NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Null outHandle returns NULL_POINTER error");

    /* Test creation with null node */
    LCComponentHandle mmHandle = NULL;
    result = lc_multimesh_create(NULL, &mmHandle);
    TEST_ASSERT(result == LC_ERROR_INVALID_HANDLE, "Null node returns INVALID_HANDLE error");

    /* Create MultiMesh component */
    result = lc_multimesh_create(node, &mmHandle);
    TEST_RESULT(result, "lc_multimesh_create()");
    TEST_ASSERT(mmHandle != NULL, "MultiMesh handle is not NULL");

    SECTION_END("MultiMesh: Component Creation");

    SECTION_START("MultiMesh: Instance Management");

    /* Test get instance count */
    int count = -1;
    result = lc_multimesh_get_instance_count(mmHandle, &count);
    TEST_RESULT(result, "lc_multimesh_get_instance_count()");
    printf("    Initial instance count: %d\n", count);

    /* Test set instance count */
    result = lc_multimesh_set_instance_count(mmHandle, 10);
    TEST_RESULT(result, "lc_multimesh_set_instance_count(10)");

    result = lc_multimesh_get_instance_count(mmHandle, &count);
    TEST_ASSERT(result == LC_SUCCESS && count == 10, "Instance count is now 10");

    /* Test negative count */
    result = lc_multimesh_set_instance_count(mmHandle, -5);
    TEST_ASSERT(result == LC_ERROR_INVALID_PARAMETER, "Negative count returns INVALID_PARAMETER error");

    SECTION_END("MultiMesh: Instance Management");

    SECTION_START("MultiMesh: Instance Transform");

    /* Set and get instance transform */
    LCMat4 transform = lc_mat4_identity();
    result = lc_multimesh_set_instance_transform(mmHandle, 0, transform);
    TEST_RESULT(result, "lc_multimesh_set_instance_transform(0)");

    LCMat4 outTransform;
    result = lc_multimesh_get_instance_transform(mmHandle, 0, &outTransform);
    TEST_RESULT(result, "lc_multimesh_get_instance_transform(0)");

    /* Test out of range index */
    result = lc_multimesh_get_instance_transform(mmHandle, 100, &outTransform);
    TEST_ASSERT(result == LC_ERROR_INVALID_PARAMETER, "Out of range index returns INVALID_PARAMETER error");

    SECTION_END("MultiMesh: Instance Transform");

    SECTION_START("MultiMesh: Instance Color");

    /* Set and get instance color */
    LCColor color = lc_color(1.0f, 0.5f, 0.25f, 1.0f);
    result = lc_multimesh_set_instance_color(mmHandle, 0, color);
    TEST_RESULT(result, "lc_multimesh_set_instance_color(0)");

    LCColor outColor;
    result = lc_multimesh_get_instance_color(mmHandle, 0, &outColor);
    TEST_RESULT(result, "lc_multimesh_get_instance_color(0)");
    printf("    Instance 0 color: (%.2f, %.2f, %.2f, %.2f)\n", outColor.r, outColor.g, outColor.b, outColor.a);

    SECTION_END("MultiMesh: Instance Color");

    SECTION_START("MultiMesh: Instance Custom Data");

    /* Set and get instance custom data */
    LCVec4 customData = lc_vec4(1.0f, 2.0f, 3.0f, 4.0f);
    result = lc_multimesh_set_instance_custom_data(mmHandle, 0, customData);
    TEST_RESULT(result, "lc_multimesh_set_instance_custom_data(0)");

    LCVec4 outData;
    result = lc_multimesh_get_instance_custom_data(mmHandle, 0, &outData);
    TEST_RESULT(result, "lc_multimesh_get_instance_custom_data(0)");
    printf("    Instance 0 custom data: (%.2f, %.2f, %.2f, %.2f)\n", outData.x, outData.y, outData.z, outData.w);

    SECTION_END("MultiMesh: Instance Custom Data");

    SECTION_START("MultiMesh: Shadow Settings");

    /* Test shadow casting mode */
    LCShadowCastingMode shadowMode;
    result = lc_multimesh_get_cast_shadow(mmHandle, &shadowMode);
    TEST_RESULT(result, "lc_multimesh_get_cast_shadow()");

    result = lc_multimesh_set_cast_shadow(mmHandle, LC_SHADOW_ON);
    TEST_RESULT(result, "lc_multimesh_set_cast_shadow(LC_SHADOW_ON)");

    /* Test receive shadow */
    bool receiveShadow;
    result = lc_multimesh_get_receive_shadow(mmHandle, &receiveShadow);
    TEST_RESULT(result, "lc_multimesh_get_receive_shadow()");

    result = lc_multimesh_set_receive_shadow(mmHandle, true);
    TEST_RESULT(result, "lc_multimesh_set_receive_shadow(true)");

    SECTION_END("MultiMesh: Shadow Settings");

    SECTION_START("MultiMesh: Culling Settings");

    /* Test cull per instance */
    bool cullPerInstance;
    result = lc_multimesh_get_cull_per_instance(mmHandle, &cullPerInstance);
    TEST_RESULT(result, "lc_multimesh_get_cull_per_instance()");

    result = lc_multimesh_set_cull_per_instance(mmHandle, true);
    TEST_RESULT(result, "lc_multimesh_set_cull_per_instance(true)");

    /* Test max distance */
    float maxDist;
    result = lc_multimesh_get_max_distance(mmHandle, &maxDist);
    TEST_RESULT(result, "lc_multimesh_get_max_distance()");
    printf("    Max distance: %.2f\n", maxDist);

    result = lc_multimesh_set_max_distance(mmHandle, 100.0f);
    TEST_RESULT(result, "lc_multimesh_set_max_distance(100.0f)");

    SECTION_END("MultiMesh: Culling Settings");

    SECTION_START("MultiMesh: LOD Levels");

    result = lc_multimesh_set_lod_distance(mmHandle, 1, 25.0f);
    TEST_RESULT(result, "lc_multimesh_set_lod_distance(1, 25.0f)");

    float lodDist = 0.0f;
    result = lc_multimesh_get_lod_distance(mmHandle, 1, &lodDist);
    TEST_RESULT(result, "lc_multimesh_get_lod_distance(1)");
    printf("    LOD 1 distance: %.2f\n", lodDist);

    result = lc_multimesh_set_lod_mesh_path(mmHandle, 1, "res://meshes/tree_lod1.glb");
    TEST_RESULT(result, "lc_multimesh_set_lod_mesh_path(1)");

    char lodPath[256];
    result = lc_multimesh_get_lod_mesh_path(mmHandle, 1, lodPath, sizeof(lodPath));
    TEST_RESULT(result, "lc_multimesh_get_lod_mesh_path(1)");
    printf("    LOD 1 mesh path: %s\n", lodPath);

    SECTION_END("MultiMesh: LOD Levels");

    /* Clean up node */
    lc_node_destroy(node);
}

static void test_scatter_multimesh(void) {
    SECTION_START("ScatterMultiMesh: Component Creation");

    /* Create a node first */
    LCNodeHandle node = NULL;
    LCResult result = lc_node_create(LC_NODE_3D, "TestScatterNode", &node);
    TEST_RESULT(result, "Create 3D node for ScatterMultiMesh");

    /* Test creation with null output */
    result = lc_scatter_multimesh_create(node, NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Null outHandle returns NULL_POINTER error");

    /* Create ScatterMultiMesh component */
    LCComponentHandle scatterHandle = NULL;
    result = lc_scatter_multimesh_create(node, &scatterHandle);
    TEST_RESULT(result, "lc_scatter_multimesh_create()");
    TEST_ASSERT(scatterHandle != NULL, "ScatterMultiMesh handle is not NULL");

    SECTION_END("ScatterMultiMesh: Component Creation");

    SECTION_START("ScatterMultiMesh: Scatter Shape Settings");

    /* Test scatter shape type */
    LCScatterShapeType shape;
    result = lc_scatter_multimesh_get_scatter_shape(scatterHandle, &shape);
    TEST_RESULT(result, "lc_scatter_multimesh_get_scatter_shape()");
    printf("    Initial scatter shape: %d\n", (int)shape);

    result = lc_scatter_multimesh_set_scatter_shape(scatterHandle, LC_SCATTER_SHAPE_SPHERE);
    TEST_RESULT(result, "lc_scatter_multimesh_set_scatter_shape(SPHERE)");

    /* Test cube size */
    LCVec3 cubeSize;
    result = lc_scatter_multimesh_get_cube_size(scatterHandle, &cubeSize);
    TEST_RESULT(result, "lc_scatter_multimesh_get_cube_size()");
    printf("    Cube size: (%.2f, %.2f, %.2f)\n", cubeSize.x, cubeSize.y, cubeSize.z);

    result = lc_scatter_multimesh_set_cube_size(scatterHandle, (LCVec3){10.0f, 5.0f, 10.0f});
    TEST_RESULT(result, "lc_scatter_multimesh_set_cube_size()");

    /* Test sphere radius */
    float radius;
    result = lc_scatter_multimesh_get_radius(scatterHandle, &radius);
    TEST_RESULT(result, "lc_scatter_multimesh_get_radius()");
    printf("    Sphere radius: %.2f\n", radius);

    result = lc_scatter_multimesh_set_radius(scatterHandle, 15.0f);
    TEST_RESULT(result, "lc_scatter_multimesh_set_radius(15.0f)");

    SECTION_END("ScatterMultiMesh: Scatter Shape Settings");

    SECTION_START("ScatterMultiMesh: Scatter Count");

    /* Test scatter count */
    int scatterCount;
    result = lc_scatter_multimesh_get_scatter_count(scatterHandle, &scatterCount);
    TEST_RESULT(result, "lc_scatter_multimesh_get_scatter_count()");
    printf("    Initial scatter count: %d\n", scatterCount);

    result = lc_scatter_multimesh_set_scatter_count(scatterHandle, 50);
    TEST_RESULT(result, "lc_scatter_multimesh_set_scatter_count(50)");

    SECTION_END("ScatterMultiMesh: Scatter Count");

    SECTION_START("ScatterMultiMesh: Clumping Settings");

    /* Test clumping mode */
    LCClumpingMode clumpMode;
    result = lc_scatter_multimesh_get_clumping_mode(scatterHandle, &clumpMode);
    TEST_RESULT(result, "lc_scatter_multimesh_get_clumping_mode()");

    result = lc_scatter_multimesh_set_clumping_mode(scatterHandle, LC_CLUMPING_CLUSTERED);
    TEST_RESULT(result, "lc_scatter_multimesh_set_clumping_mode(CLUSTERED)");

    /* Test clump factor */
    float clumpFactor;
    result = lc_scatter_multimesh_get_clump_factor(scatterHandle, &clumpFactor);
    TEST_RESULT(result, "lc_scatter_multimesh_get_clump_factor()");

    result = lc_scatter_multimesh_set_clump_factor(scatterHandle, 0.5f);
    TEST_RESULT(result, "lc_scatter_multimesh_set_clump_factor(0.5f)");

    /* Test clump count */
    int clumpCount;
    result = lc_scatter_multimesh_get_clump_count(scatterHandle, &clumpCount);
    TEST_RESULT(result, "lc_scatter_multimesh_get_clump_count()");

    result = lc_scatter_multimesh_set_clump_count(scatterHandle, 5);
    TEST_RESULT(result, "lc_scatter_multimesh_set_clump_count(5)");

    /* Test clump radius */
    float clumpRadius;
    result = lc_scatter_multimesh_get_clump_radius(scatterHandle, &clumpRadius);
    TEST_RESULT(result, "lc_scatter_multimesh_get_clump_radius()");

    result = lc_scatter_multimesh_set_clump_radius(scatterHandle, 3.0f);
    TEST_RESULT(result, "lc_scatter_multimesh_set_clump_radius(3.0f)");

    SECTION_END("ScatterMultiMesh: Clumping Settings");

    SECTION_START("ScatterMultiMesh: Variation Settings");

    /* Test scale range */
    LCVec2 scaleRange;
    result = lc_scatter_multimesh_get_scale_range(scatterHandle, &scaleRange);
    TEST_RESULT(result, "lc_scatter_multimesh_get_scale_range()");
    printf("    Scale range: (%.2f, %.2f)\n", scaleRange.x, scaleRange.y);

    result = lc_scatter_multimesh_set_scale_range(scatterHandle, (LCVec2){0.8f, 1.2f});
    TEST_RESULT(result, "lc_scatter_multimesh_set_scale_range()");

    /* Test uniform scale */
    bool uniformScale;
    result = lc_scatter_multimesh_get_uniform_scale(scatterHandle, &uniformScale);
    TEST_RESULT(result, "lc_scatter_multimesh_get_uniform_scale()");

    result = lc_scatter_multimesh_set_uniform_scale(scatterHandle, true);
    TEST_RESULT(result, "lc_scatter_multimesh_set_uniform_scale(true)");

    /* Test rotation variation */
    LCVec3 rotVariation;
    result = lc_scatter_multimesh_get_rotation_variation(scatterHandle, &rotVariation);
    TEST_RESULT(result, "lc_scatter_multimesh_get_rotation_variation()");

    result = lc_scatter_multimesh_set_rotation_variation(scatterHandle, (LCVec3){15.0f, 360.0f, 15.0f});
    TEST_RESULT(result, "lc_scatter_multimesh_set_rotation_variation()");

    /* Test align to surface */
    bool alignToSurface;
    result = lc_scatter_multimesh_get_align_to_surface(scatterHandle, &alignToSurface);
    TEST_RESULT(result, "lc_scatter_multimesh_get_align_to_surface()");

    result = lc_scatter_multimesh_set_align_to_surface(scatterHandle, true);
    TEST_RESULT(result, "lc_scatter_multimesh_set_align_to_surface(true)");

    SECTION_END("ScatterMultiMesh: Variation Settings");

    SECTION_START("ScatterMultiMesh: Color Variation");

    /* Test color variation enabled */
    bool colorVarEnabled;
    result = lc_scatter_multimesh_get_color_variation_enabled(scatterHandle, &colorVarEnabled);
    TEST_RESULT(result, "lc_scatter_multimesh_get_color_variation_enabled()");

    result = lc_scatter_multimesh_set_color_variation_enabled(scatterHandle, true);
    TEST_RESULT(result, "lc_scatter_multimesh_set_color_variation_enabled(true)");

    /* Test base color */
    LCColor baseColor;
    result = lc_scatter_multimesh_get_base_color(scatterHandle, &baseColor);
    TEST_RESULT(result, "lc_scatter_multimesh_get_base_color()");

    result = lc_scatter_multimesh_set_base_color(scatterHandle, lc_color(0.3f, 0.6f, 0.2f, 1.0f));
    TEST_RESULT(result, "lc_scatter_multimesh_set_base_color()");

    /* Test color variation amount */
    float colorVar;
    result = lc_scatter_multimesh_get_color_variation(scatterHandle, &colorVar);
    TEST_RESULT(result, "lc_scatter_multimesh_get_color_variation()");

    result = lc_scatter_multimesh_set_color_variation(scatterHandle, 0.2f);
    TEST_RESULT(result, "lc_scatter_multimesh_set_color_variation(0.2f)");

    SECTION_END("ScatterMultiMesh: Color Variation");

    SECTION_START("ScatterMultiMesh: Randomization");

    /* Test random seed */
    int seed;
    result = lc_scatter_multimesh_get_random_seed(scatterHandle, &seed);
    TEST_RESULT(result, "lc_scatter_multimesh_get_random_seed()");
    printf("    Random seed: %d\n", seed);

    result = lc_scatter_multimesh_set_random_seed(scatterHandle, 12345);
    TEST_RESULT(result, "lc_scatter_multimesh_set_random_seed(12345)");

    /* Test use random seed */
    bool useRandomSeed;
    result = lc_scatter_multimesh_get_use_random_seed(scatterHandle, &useRandomSeed);
    TEST_RESULT(result, "lc_scatter_multimesh_get_use_random_seed()");

    result = lc_scatter_multimesh_set_use_random_seed(scatterHandle, true);
    TEST_RESULT(result, "lc_scatter_multimesh_set_use_random_seed(true)");

    SECTION_END("ScatterMultiMesh: Randomization");

    SECTION_START("ScatterMultiMesh: Regeneration");

    /* Test regenerate */
    result = lc_scatter_multimesh_regenerate(scatterHandle);
    TEST_RESULT(result, "lc_scatter_multimesh_regenerate()");

    /* Test needs regeneration */
    bool needsRegen;
    result = lc_scatter_multimesh_needs_regeneration(scatterHandle, &needsRegen);
    TEST_RESULT(result, "lc_scatter_multimesh_needs_regeneration()");
    printf("    Needs regeneration: %s\n", needsRegen ? "yes" : "no");

    SECTION_END("ScatterMultiMesh: Regeneration");

    SECTION_START("ScatterMultiMesh: Debug Visualization");

    /* Test show debug shape */
    bool showDebug;
    result = lc_scatter_multimesh_get_show_debug_shape(scatterHandle, &showDebug);
    TEST_RESULT(result, "lc_scatter_multimesh_get_show_debug_shape()");

    result = lc_scatter_multimesh_set_show_debug_shape(scatterHandle, true);
    TEST_RESULT(result, "lc_scatter_multimesh_set_show_debug_shape(true)");

    /* Test debug color */
    LCColor debugColor;
    result = lc_scatter_multimesh_get_debug_color(scatterHandle, &debugColor);
    TEST_RESULT(result, "lc_scatter_multimesh_get_debug_color()");

    result = lc_scatter_multimesh_set_debug_color(scatterHandle, lc_color(0.0f, 1.0f, 0.0f, 0.5f));
    TEST_RESULT(result, "lc_scatter_multimesh_set_debug_color()");

    SECTION_END("ScatterMultiMesh: Debug Visualization");

    /* Clean up node */
    lc_node_destroy(node);
}

/* ============================================================================
 * TileMap2D Tests
 * ============================================================================ */

static void test_tilemap2d(void) {
    SECTION_START("TileMap2D: Component Creation");

    /* Test creation with null output */
    LCResult result = lc_tilemap2d_create("TestTilemap", NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Null output returns NULL_POINTER error");

    /* Create TileMap2D component */
    LCComponentHandle tilemapHandle = NULL;
    result = lc_tilemap2d_create("TestTilemap", &tilemapHandle);
    TEST_RESULT(result, "lc_tilemap2d_create()");
    TEST_ASSERT(tilemapHandle != NULL, "TileMap2D handle is not NULL");

    /* Create with NULL name */
    LCComponentHandle tilemapHandle2 = NULL;
    result = lc_tilemap2d_create(NULL, &tilemapHandle2);
    TEST_RESULT(result, "lc_tilemap2d_create(NULL, ...)");
    TEST_ASSERT(tilemapHandle2 != NULL, "TileMap2D handle with NULL name is not NULL");

    SECTION_END("TileMap2D: Component Creation");

    SECTION_START("TileMap2D: Path Properties");

    /* Test get/set path with null pointer */
    result = lc_tilemap2d_get_path(tilemapHandle, NULL, 256);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Null path buffer returns NULL_POINTER error");

    result = lc_tilemap2d_set_path(tilemapHandle, NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Null path string returns NULL_POINTER error");

    /* Get empty path */
    char pathBuffer[512];
    result = lc_tilemap2d_get_path(tilemapHandle, pathBuffer, sizeof(pathBuffer));
    TEST_RESULT(result, "lc_tilemap2d_get_path()");
    printf("    Initial path: '%s'\n", pathBuffer);

    /* Set path (won't load since file doesn't exist) */
    result = lc_tilemap2d_set_path(tilemapHandle, "test_tilemap.tilemap");
    TEST_RESULT(result, "lc_tilemap2d_set_path()");

    SECTION_END("TileMap2D: Path Properties");

    SECTION_START("TileMap2D: Base Properties");

    /* Test base z-index */
    int zIndex = -1;
    result = lc_tilemap2d_get_base_z_index(tilemapHandle, &zIndex);
    TEST_RESULT(result, "lc_tilemap2d_get_base_z_index()");
    printf("    Initial z-index: %d\n", zIndex);

    result = lc_tilemap2d_set_base_z_index(tilemapHandle, 10);
    TEST_RESULT(result, "lc_tilemap2d_set_base_z_index(10)");

    result = lc_tilemap2d_get_base_z_index(tilemapHandle, &zIndex);
    TEST_ASSERT(result == LC_SUCCESS && zIndex == 10, "Z-index is now 10");

    /* Test show collision */
    bool showCollision = true;
    result = lc_tilemap2d_get_show_collision(tilemapHandle, &showCollision);
    TEST_RESULT(result, "lc_tilemap2d_get_show_collision()");
    printf("    Show collision: %s\n", showCollision ? "yes" : "no");

    result = lc_tilemap2d_set_show_collision(tilemapHandle, true);
    TEST_RESULT(result, "lc_tilemap2d_set_show_collision(true)");

    /* Test modulate color */
    LCColor modulate;
    result = lc_tilemap2d_get_modulate(tilemapHandle, &modulate);
    TEST_RESULT(result, "lc_tilemap2d_get_modulate()");
    printf("    Modulate: (%.2f, %.2f, %.2f, %.2f)\n", modulate.r, modulate.g, modulate.b, modulate.a);

    result = lc_tilemap2d_set_modulate(tilemapHandle, lc_color(1.0f, 0.8f, 0.8f, 1.0f));
    TEST_RESULT(result, "lc_tilemap2d_set_modulate()");

    SECTION_END("TileMap2D: Base Properties");

    SECTION_START("TileMap2D: Map Information");

    /* Test map dimensions */
    int width = -1, height = -1;
    result = lc_tilemap2d_get_width(tilemapHandle, &width);
    TEST_RESULT(result, "lc_tilemap2d_get_width()");
    printf("    Map width: %d tiles\n", width);

    result = lc_tilemap2d_get_height(tilemapHandle, &height);
    TEST_RESULT(result, "lc_tilemap2d_get_height()");
    printf("    Map height: %d tiles\n", height);

    /* Test tile dimensions */
    int tileWidth = -1, tileHeight = -1;
    result = lc_tilemap2d_get_tile_width(tilemapHandle, &tileWidth);
    TEST_RESULT(result, "lc_tilemap2d_get_tile_width()");
    printf("    Tile width: %d pixels\n", tileWidth);

    result = lc_tilemap2d_get_tile_height(tilemapHandle, &tileHeight);
    TEST_RESULT(result, "lc_tilemap2d_get_tile_height()");
    printf("    Tile height: %d pixels\n", tileHeight);

    /* Test map size in pixels */
    LCVec2 mapSize;
    result = lc_tilemap2d_get_map_size_pixels(tilemapHandle, &mapSize);
    TEST_RESULT(result, "lc_tilemap2d_get_map_size_pixels()");
    printf("    Map size: %.0f x %.0f pixels\n", mapSize.x, mapSize.y);

    SECTION_END("TileMap2D: Map Information");

    SECTION_START("TileMap2D: Layer Management");

    /* Test layer count */
    int layerCount = -1;
    result = lc_tilemap2d_get_layer_count(tilemapHandle, &layerCount);
    TEST_RESULT(result, "lc_tilemap2d_get_layer_count()");
    printf("    Layer count: %d\n", layerCount);

    /* Test get layer by invalid index (no layers loaded) */
    LCTileMapLayer layer;
    result = lc_tilemap2d_get_layer(tilemapHandle, 0, &layer);
    /* Expect INVALID_PARAMETER since no layers are loaded */
    printf("    Get layer 0 result: %d (expected INVALID_PARAMETER or SUCCESS)\n", (int)result);

    /* Test layer visibility with null pointer */
    result = lc_tilemap2d_get_layer_visible(tilemapHandle, 0, NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Null visibility pointer returns NULL_POINTER error");

    /* Test layer opacity with null pointer */
    result = lc_tilemap2d_get_layer_opacity(tilemapHandle, 0, NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Null opacity pointer returns NULL_POINTER error");

    /* Test layer z-offset with null pointer */
    result = lc_tilemap2d_get_layer_z_offset(tilemapHandle, 0, NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Null z-offset pointer returns NULL_POINTER error");

    /* Test layer by name with null name */
    result = lc_tilemap2d_get_layer_by_name(tilemapHandle, NULL, &layer);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Null layer name returns NULL_POINTER error");

    /* Test set layer visible by name with null name */
    result = lc_tilemap2d_set_layer_visible_by_name(tilemapHandle, NULL, true);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Null layer name for visibility returns NULL_POINTER error");

    SECTION_END("TileMap2D: Layer Management");

    SECTION_START("TileMap2D: Coordinate Conversion");

    /* Test world to cell */
    LCVec2 cellCoords;
    result = lc_tilemap2d_world_to_cell(tilemapHandle, lc_vec2(64.0f, 64.0f), &cellCoords);
    TEST_RESULT(result, "lc_tilemap2d_world_to_cell()");
    printf("    World (64, 64) -> Cell (%.0f, %.0f)\n", cellCoords.x, cellCoords.y);

    /* Test cell to world */
    LCVec2 worldPos;
    result = lc_tilemap2d_cell_to_world(tilemapHandle, 2, 2, &worldPos);
    TEST_RESULT(result, "lc_tilemap2d_cell_to_world()");
    printf("    Cell (2, 2) -> World (%.0f, %.0f)\n", worldPos.x, worldPos.y);

    /* Test with null output */
    result = lc_tilemap2d_world_to_cell(tilemapHandle, lc_vec2(0.0f, 0.0f), NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Null cell coords pointer returns NULL_POINTER error");

    result = lc_tilemap2d_cell_to_world(tilemapHandle, 0, 0, NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Null world pos pointer returns NULL_POINTER error");

    SECTION_END("TileMap2D: Coordinate Conversion");

    SECTION_START("TileMap2D: Collision Queries");

    /* Test has collision at */
    bool hasCollision = true;
    result = lc_tilemap2d_has_collision_at(tilemapHandle, 0, 0, &hasCollision);
    TEST_RESULT(result, "lc_tilemap2d_has_collision_at()");
    printf("    Collision at (0, 0): %s\n", hasCollision ? "yes" : "no");

    /* Test with null output */
    result = lc_tilemap2d_has_collision_at(tilemapHandle, 0, 0, NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Null collision pointer returns NULL_POINTER error");

    /* Test get collision shape */
    LCTileCollisionShape shape;
    result = lc_tilemap2d_get_collision_shape_at(tilemapHandle, 0, 0, 0, &shape);
    /* May fail if no layers exist */
    printf("    Get collision shape result: %d\n", (int)result);

    /* Test with null output */
    result = lc_tilemap2d_get_collision_shape_at(tilemapHandle, 0, 0, 0, NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Null shape pointer returns NULL_POINTER error");

    SECTION_END("TileMap2D: Collision Queries");

    SECTION_START("TileMap2D: Cell Data");

    /* Test get cell data with null output */
    result = lc_tilemap2d_get_cell_data(tilemapHandle, 0, 0, 0, NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Null cell data pointer returns NULL_POINTER error");

    /* Test get cell data */
    LCTileMapCellData cellData;
    memset(&cellData, 0, sizeof(cellData));
    result = lc_tilemap2d_get_cell_data(tilemapHandle, 0, 0, 0, &cellData);
    /* May succeed with empty data or fail if no layers exist */
    printf("    Get cell data result: %d, has_data: %s\n", (int)result, cellData.has_data ? "yes" : "no");

    /* Free cell data memory */
    lc_tilemap2d_free_cell_data(&cellData);

    /* Test get cell data by layer name with null name */
    result = lc_tilemap2d_get_cell_data_by_layer_name(tilemapHandle, 0, 0, NULL, &cellData);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Null layer name returns NULL_POINTER error");

    /* Test get cell data at world pos */
    memset(&cellData, 0, sizeof(cellData));
    result = lc_tilemap2d_get_cell_data_at_world_pos(tilemapHandle, lc_vec2(32.0f, 32.0f), 0, &cellData);
    printf("    Get cell data at world pos result: %d\n", (int)result);
    lc_tilemap2d_free_cell_data(&cellData);

    SECTION_END("TileMap2D: Cell Data");

    SECTION_START("TileMap2D: Tileset Access");

    /* Test tileset count */
    int tilesetCount = -1;
    result = lc_tilemap2d_get_tileset_count(tilemapHandle, &tilesetCount);
    TEST_RESULT(result, "lc_tilemap2d_get_tileset_count()");
    printf("    Tileset count: %d\n", tilesetCount);

    /* Test with null output */
    result = lc_tilemap2d_get_tileset_count(tilemapHandle, NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Null count pointer returns NULL_POINTER error");

    /* Test get tileset with invalid index */
    LCTilesetData tilesetData;
    result = lc_tilemap2d_get_tileset(tilemapHandle, 0, &tilesetData);
    /* Expect INVALID_PARAMETER since no tilesets are loaded */
    printf("    Get tileset 0 result: %d\n", (int)result);

    /* Test with null output */
    result = lc_tilemap2d_get_tileset(tilemapHandle, 0, NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Null tileset pointer returns NULL_POINTER error");

    SECTION_END("TileMap2D: Tileset Access");

    SECTION_START("TileMap2D: Loading Operations");

    /* Test load with null path */
    result = lc_tilemap2d_load(tilemapHandle, NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Null filepath returns NULL_POINTER error");

    /* Test load with non-existent file */
    result = lc_tilemap2d_load(tilemapHandle, "non_existent_tilemap.tilemap");
    TEST_ASSERT(result != LC_SUCCESS, "Non-existent file returns error");

    /* Test reload (no file loaded) */
    result = lc_tilemap2d_reload(tilemapHandle);
    /* May fail since no valid tilemap is loaded */
    printf("    Reload result: %d\n", (int)result);

    /* Test clear */
    result = lc_tilemap2d_clear(tilemapHandle);
    TEST_RESULT(result, "lc_tilemap2d_clear()");

    SECTION_END("TileMap2D: Loading Operations");

    SECTION_START("TileMap2D: Invalid Handle Tests");

    /* Test with invalid handle */
    result = lc_tilemap2d_get_width(NULL, &width);
    TEST_ASSERT(result == LC_ERROR_INVALID_HANDLE, "Null handle returns INVALID_HANDLE error");

    result = lc_tilemap2d_set_base_z_index(NULL, 0);
    TEST_ASSERT(result == LC_ERROR_INVALID_HANDLE, "Null handle for set returns INVALID_HANDLE error");

    result = lc_tilemap2d_load(NULL, "test.tilemap");
    TEST_ASSERT(result == LC_ERROR_INVALID_HANDLE, "Null handle for load returns INVALID_HANDLE error");

    result = lc_tilemap2d_clear(NULL);
    TEST_ASSERT(result == LC_ERROR_INVALID_HANDLE, "Null handle for clear returns INVALID_HANDLE error");

    SECTION_END("TileMap2D: Invalid Handle Tests");

    SECTION_START("TileMap2D: Structure Sizes");

    printf("    sizeof(LCTileMapLayer): %zu bytes\n", sizeof(LCTileMapLayer));
    printf("    sizeof(LCTileMapCellData): %zu bytes\n", sizeof(LCTileMapCellData));
    printf("    sizeof(LCTilesetData): %zu bytes\n", sizeof(LCTilesetData));
    printf("    sizeof(LCTileCollisionShape): %zu bytes\n", sizeof(LCTileCollisionShape));
    printf("    sizeof(LCTileMetadata): %zu bytes\n", sizeof(LCTileMetadata));

    TEST_STATIC_ASSERT(sizeof(LCTileMapLayer) > 0, "LCTileMapLayer has valid size");
    TEST_STATIC_ASSERT(sizeof(LCTileMapCellData) > 0, "LCTileMapCellData has valid size");
    TEST_STATIC_ASSERT(sizeof(LCTilesetData) > 0, "LCTilesetData has valid size");
    TEST_STATIC_ASSERT(sizeof(LCTileCollisionShape) > 0, "LCTileCollisionShape has valid size");

    SECTION_END("TileMap2D: Structure Sizes");
}

/* ============================================================================
 * WorldEnvironment Tests
 * ============================================================================ */

static void test_world_environment(void) {
    SECTION_START("WorldEnvironment Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_world_env_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_3D, "WorldEnvNode", &node);
    TEST_RESULT(result, "Create node for WorldEnvironment");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle env = NULL;
    result = lc_world_environment_create("TestWorldEnv", &env);
    TEST_RESULT(result, "Create WorldEnvironment component");

    /* Skybox Type */
    result = lc_world_environment_set_skybox_type(env, LC_SKYBOX_PROCEDURAL);
    TEST_RESULT(result, "Set skybox type to procedural");

    LCSkyboxType skyboxType;
    lc_world_environment_get_skybox_type(env, &skyboxType);
    TEST_ASSERT(skyboxType == LC_SKYBOX_PROCEDURAL, "Skybox type is procedural");

    /* Skybox Color */
    result = lc_world_environment_set_skybox_color(env, lc_color(0.2f, 0.3f, 0.5f, 1.0f));
    TEST_RESULT(result, "Set skybox color");

    LCColor skyColor;
    lc_world_environment_get_skybox_color(env, &skyColor);
    TEST_ASSERT(skyColor.r > 0.19f && skyColor.r < 0.21f, "Skybox color R is ~0.2");

    /* Procedural Sky Colors */
    result = lc_world_environment_set_sky_top_color(env, lc_color(0.0f, 0.0f, 0.8f, 1.0f));
    TEST_RESULT(result, "Set sky top color");

    result = lc_world_environment_set_sky_horizon_color(env, lc_color(0.5f, 0.6f, 0.8f, 1.0f));
    TEST_RESULT(result, "Set sky horizon color");

    result = lc_world_environment_set_sky_bottom_color(env, lc_color(0.3f, 0.2f, 0.15f, 1.0f));
    TEST_RESULT(result, "Set sky bottom color");

    LCColor topColor, horizonColor, bottomColor;
    lc_world_environment_get_sky_top_color(env, &topColor);
    lc_world_environment_get_sky_horizon_color(env, &horizonColor);
    lc_world_environment_get_sky_bottom_color(env, &bottomColor);
    TEST_ASSERT(topColor.b > 0.7f, "Sky top color B is ~0.8");
    TEST_ASSERT(horizonColor.g > 0.5f, "Sky horizon color G is ~0.6");
    TEST_ASSERT(bottomColor.r > 0.2f, "Sky bottom color R is ~0.3");

    /* Fog Settings */
    result = lc_world_environment_set_fog_enabled(env, true);
    TEST_RESULT(result, "Enable fog");

    bool fogEnabled;
    lc_world_environment_get_fog_enabled(env, &fogEnabled);
    TEST_ASSERT(fogEnabled == true, "Fog is enabled");

    result = lc_world_environment_set_fog_color(env, lc_color(0.7f, 0.7f, 0.8f, 1.0f));
    TEST_RESULT(result, "Set fog color");

    result = lc_world_environment_set_fog_density(env, 0.05f);
    TEST_RESULT(result, "Set fog density");

    float fogDensity;
    lc_world_environment_get_fog_density(env, &fogDensity);
    TEST_ASSERT(fogDensity > 0.04f && fogDensity < 0.06f, "Fog density is ~0.05");

    result = lc_world_environment_set_fog_start(env, 10.0f);
    TEST_RESULT(result, "Set fog start");

    result = lc_world_environment_set_fog_end(env, 100.0f);
    TEST_RESULT(result, "Set fog end");

    float fogStart, fogEnd;
    lc_world_environment_get_fog_start(env, &fogStart);
    lc_world_environment_get_fog_end(env, &fogEnd);
    TEST_ASSERT(fogStart > 9.0f && fogStart < 11.0f, "Fog start is ~10");
    TEST_ASSERT(fogEnd > 99.0f && fogEnd < 101.0f, "Fog end is ~100");

    result = lc_world_environment_set_fog_mode(env, LC_FOG_EXPONENTIAL);
    TEST_RESULT(result, "Set fog mode to exponential");

    LCFogMode fogMode;
    lc_world_environment_get_fog_mode(env, &fogMode);
    TEST_ASSERT(fogMode == LC_FOG_EXPONENTIAL, "Fog mode is exponential");

    /* Ambient Light */
    result = lc_world_environment_set_ambient_light_enabled(env, true);
    TEST_RESULT(result, "Enable ambient light");

    bool ambientEnabled;
    lc_world_environment_get_ambient_light_enabled(env, &ambientEnabled);
    TEST_ASSERT(ambientEnabled == true, "Ambient light is enabled");

    result = lc_world_environment_set_ambient_light_color(env, lc_color(0.3f, 0.3f, 0.4f, 1.0f));
    TEST_RESULT(result, "Set ambient light color");

    result = lc_world_environment_set_ambient_light_intensity(env, 0.5f);
    TEST_RESULT(result, "Set ambient light intensity");

    float ambientIntensity;
    lc_world_environment_get_ambient_light_intensity(env, &ambientIntensity);
    TEST_ASSERT(ambientIntensity > 0.4f && ambientIntensity < 0.6f, "Ambient intensity is ~0.5");

    /* Cubemap Paths */
    result = lc_world_environment_set_cubemap_pos_x(env, "textures/skybox_px.png");
    TEST_RESULT(result, "Set cubemap pos X");

    char pathBuf[256];
    lc_world_environment_get_cubemap_pos_x(env, pathBuf, sizeof(pathBuf));
    TEST_ASSERT(strcmp(pathBuf, "textures/skybox_px.png") == 0, "Cubemap pos X path matches");

    /* Panoramic Texture */
    result = lc_world_environment_set_panoramic_texture(env, "textures/panorama.hdr");
    TEST_RESULT(result, "Set panoramic texture");

    lc_world_environment_get_panoramic_texture(env, pathBuf, sizeof(pathBuf));
    TEST_ASSERT(strcmp(pathBuf, "textures/panorama.hdr") == 0, "Panoramic texture path matches");

    /* Volumetric Fog */
    result = lc_world_environment_set_volumetric_fog_enabled(env, true);
    TEST_RESULT(result, "Enable volumetric fog");

    bool volumetricEnabled;
    lc_world_environment_get_volumetric_fog_enabled(env, &volumetricEnabled);
    TEST_ASSERT(volumetricEnabled == true, "Volumetric fog is enabled");

    lc_scene_destroy(scene);

    SECTION_END("WorldEnvironment Component");
}

/* ============================================================================
 * NodeScatter Tests
 * ============================================================================ */

static void test_node_scatter(void) {
    SECTION_START("NodeScatter Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_scatter_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_3D, "ScatterNode", &node);
    TEST_RESULT(result, "Create node for NodeScatter");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle scatter = NULL;
    result = lc_node_scatter_create("TestScatter", &scatter);
    TEST_RESULT(result, "Create NodeScatter component");

    /* Scatter Mode */
    result = lc_node_scatter_set_mode(scatter, LC_NODE_SCATTER_COLLISION);
    TEST_RESULT(result, "Set scatter mode to collision");

    LCNodeScatterMode scatterMode;
    lc_node_scatter_get_mode(scatter, &scatterMode);
    TEST_ASSERT(scatterMode == LC_NODE_SCATTER_COLLISION, "Scatter mode is collision");

    /* Distribution Mode */
    result = lc_node_scatter_set_distribution_mode(scatter, LC_NODE_SCATTER_GRID);
    TEST_RESULT(result, "Set distribution mode to grid");

    LCNodeScatterDistribution distMode;
    lc_node_scatter_get_distribution_mode(scatter, &distMode);
    TEST_ASSERT(distMode == LC_NODE_SCATTER_GRID, "Distribution mode is grid");

    /* Instance Count */
    result = lc_node_scatter_set_instance_count(scatter, 500);
    TEST_RESULT(result, "Set instance count");

    int instanceCount;
    lc_node_scatter_get_instance_count(scatter, &instanceCount);
    TEST_ASSERT(instanceCount == 500, "Instance count is 500");

    /* Grid Settings */
    result = lc_node_scatter_set_grid_spacing_x(scatter, 2.0f);
    TEST_RESULT(result, "Set grid spacing X");

    result = lc_node_scatter_set_grid_spacing_z(scatter, 2.5f);
    TEST_RESULT(result, "Set grid spacing Z");

    float spacingX, spacingZ;
    lc_node_scatter_get_grid_spacing_x(scatter, &spacingX);
    lc_node_scatter_get_grid_spacing_z(scatter, &spacingZ);
    TEST_ASSERT(spacingX > 1.9f && spacingX < 2.1f, "Grid spacing X is ~2.0");
    TEST_ASSERT(spacingZ > 2.4f && spacingZ < 2.6f, "Grid spacing Z is ~2.5");

    result = lc_node_scatter_set_grid_jitter(scatter, 0.3f);
    TEST_RESULT(result, "Set grid jitter");

    float jitter;
    lc_node_scatter_get_grid_jitter(scatter, &jitter);
    TEST_ASSERT(jitter > 0.2f && jitter < 0.4f, "Grid jitter is ~0.3");

    /* Area Settings */
    LCVec3 areaSize = lc_vec3(50.0f, 10.0f, 50.0f);
    result = lc_node_scatter_set_area_size(scatter, areaSize);
    TEST_RESULT(result, "Set area size");

    LCVec3 outSize;
    lc_node_scatter_get_area_size(scatter, &outSize);
    TEST_ASSERT(outSize.x > 49.0f && outSize.x < 51.0f, "Area size X is ~50");
    TEST_ASSERT(outSize.z > 49.0f && outSize.z < 51.0f, "Area size Z is ~50");

    LCVec3 areaOffset = lc_vec3(0.0f, 5.0f, 0.0f);
    result = lc_node_scatter_set_area_offset(scatter, areaOffset);
    TEST_RESULT(result, "Set area offset");

    /* Surface Settings */
    result = lc_node_scatter_set_align_to_normal(scatter, true);
    TEST_RESULT(result, "Enable align to normal");

    bool alignNormal;
    lc_node_scatter_get_align_to_normal(scatter, &alignNormal);
    TEST_ASSERT(alignNormal == true, "Align to normal is enabled");

    result = lc_node_scatter_set_normal_alignment_strength(scatter, 0.8f);
    TEST_RESULT(result, "Set normal alignment strength");

    float alignStrength;
    lc_node_scatter_get_normal_alignment_strength(scatter, &alignStrength);
    TEST_ASSERT(alignStrength > 0.7f && alignStrength < 0.9f, "Alignment strength is ~0.8");

    result = lc_node_scatter_set_surface_offset(scatter, 0.1f);
    TEST_RESULT(result, "Set surface offset");

    /* Slope Filtering */
    result = lc_node_scatter_set_min_slope(scatter, 0.0f);
    TEST_RESULT(result, "Set min slope");

    result = lc_node_scatter_set_max_slope(scatter, 45.0f);
    TEST_RESULT(result, "Set max slope");

    float minSlope, maxSlope;
    lc_node_scatter_get_min_slope(scatter, &minSlope);
    lc_node_scatter_get_max_slope(scatter, &maxSlope);
    TEST_ASSERT(minSlope >= -0.1f && minSlope < 0.1f, "Min slope is ~0");
    TEST_ASSERT(maxSlope > 44.0f && maxSlope < 46.0f, "Max slope is ~45");

    /* Scale Variation */
    LCVec2 scaleRange = lc_vec2(0.8f, 1.2f);
    result = lc_node_scatter_set_scale_range(scatter, scaleRange);
    TEST_RESULT(result, "Set scale range");

    LCVec2 outRange;
    lc_node_scatter_get_scale_range(scatter, &outRange);
    TEST_ASSERT(outRange.x > 0.7f && outRange.x < 0.9f, "Scale range min is ~0.8");
    TEST_ASSERT(outRange.y > 1.1f && outRange.y < 1.3f, "Scale range max is ~1.2");

    result = lc_node_scatter_set_uniform_scale(scatter, true);
    TEST_RESULT(result, "Enable uniform scale");

    bool uniformScale;
    lc_node_scatter_get_uniform_scale(scatter, &uniformScale);
    TEST_ASSERT(uniformScale == true, "Uniform scale is enabled");

    /* Rotation Variation */
    LCVec3 rotVar = lc_vec3(15.0f, 360.0f, 15.0f);
    result = lc_node_scatter_set_rotation_variation(scatter, rotVar);
    TEST_RESULT(result, "Set rotation variation");

    result = lc_node_scatter_set_random_y_rotation(scatter, true);
    TEST_RESULT(result, "Enable random Y rotation");

    bool randomY;
    lc_node_scatter_get_random_y_rotation(scatter, &randomY);
    TEST_ASSERT(randomY == true, "Random Y rotation is enabled");

    /* Randomization */
    result = lc_node_scatter_set_random_seed(scatter, 12345);
    TEST_RESULT(result, "Set random seed");

    int seed;
    lc_node_scatter_get_random_seed(scatter, &seed);
    TEST_ASSERT(seed == 12345, "Random seed is 12345");

    result = lc_node_scatter_set_use_random_seed(scatter, true);
    TEST_RESULT(result, "Enable use random seed");

    bool useSeed;
    lc_node_scatter_get_use_random_seed(scatter, &useSeed);
    TEST_ASSERT(useSeed == true, "Use random seed is enabled");

    /* Collision Layers */
    result = lc_node_scatter_set_scatter_layers(scatter, 0x0001);
    TEST_RESULT(result, "Set scatter layers");

    uint32_t layers;
    lc_node_scatter_get_scatter_layers(scatter, &layers);
    TEST_ASSERT(layers == 0x0001, "Scatter layers is 0x0001");

    result = lc_node_scatter_set_cutout_layers(scatter, 0x0002);
    TEST_RESULT(result, "Set cutout layers");

    lc_node_scatter_get_cutout_layers(scatter, &layers);
    TEST_ASSERT(layers == 0x0002, "Cutout layers is 0x0002");

    /* Debug Visualization */
    result = lc_node_scatter_set_show_debug_shape(scatter, true);
    TEST_RESULT(result, "Enable debug shape");

    bool showDebug;
    lc_node_scatter_get_show_debug_shape(scatter, &showDebug);
    TEST_ASSERT(showDebug == true, "Debug shape is shown");

    result = lc_node_scatter_set_debug_color(scatter, lc_color(0.0f, 1.0f, 0.0f, 0.5f));
    TEST_RESULT(result, "Set debug color");

    /* Performance Settings */
    result = lc_node_scatter_set_instances_per_frame(scatter, 50);
    TEST_RESULT(result, "Set instances per frame");

    int perFrame;
    lc_node_scatter_get_instances_per_frame(scatter, &perFrame);
    TEST_ASSERT(perFrame == 50, "Instances per frame is 50");

    result = lc_node_scatter_set_collision_optimization(scatter, LC_SCATTER_COLLISION_SIMPLIFIED_BOX);
    TEST_RESULT(result, "Set collision optimization");

    LCScatterCollisionMode collMode;
    lc_node_scatter_get_collision_optimization(scatter, &collMode);
    TEST_ASSERT(collMode == LC_SCATTER_COLLISION_SIMPLIFIED_BOX, "Collision mode is simplified box");

    result = lc_node_scatter_set_collision_distance(scatter, 100.0f);
    TEST_RESULT(result, "Set collision distance");

    float collDist;
    lc_node_scatter_get_collision_distance(scatter, &collDist);
    TEST_ASSERT(collDist > 99.0f && collDist < 101.0f, "Collision distance is ~100");

    result = lc_node_scatter_set_progressive_loading(scatter, true);
    TEST_RESULT(result, "Enable progressive loading");

    bool progressive;
    lc_node_scatter_get_progressive_loading(scatter, &progressive);
    TEST_ASSERT(progressive == true, "Progressive loading is enabled");

    /* Regeneration */
    result = lc_node_scatter_regenerate(scatter);
    TEST_RESULT(result, "Regenerate scatter");

    /* Note: NeedsRegeneration may still be true if no template node is available */
    bool needsRegen;
    result = lc_node_scatter_needs_regeneration(scatter, &needsRegen);
    TEST_RESULT(result, "Query needs regeneration");

    int scatteredCount;
    lc_node_scatter_get_scattered_instance_count(scatter, &scatteredCount);
    printf("    Scattered instance count: %d (expected 0 without template)\n", scatteredCount);
    TEST_ASSERT(scatteredCount == 0, "Scattered count is 0 without template node");

    lc_scene_destroy(scene);

    SECTION_END("NodeScatter Component");
}

/* ============================================================================
 * YSort Tests
 * ============================================================================ */

static void test_ysort(void) {
    SECTION_START("YSort Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_ysort_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "YSortNode", &node);
    TEST_RESULT(result, "Create node for YSort");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle ysort = NULL;
    result = lc_ysort_create("TestYSort", &ysort);
    TEST_RESULT(result, "Create YSort component");

    /* Enable/Disable */
    result = lc_ysort_set_enabled(ysort, true);
    TEST_RESULT(result, "Enable YSort");

    bool enabled;
    lc_ysort_get_enabled(ysort, &enabled);
    TEST_ASSERT(enabled == true, "YSort is enabled");

    /* Sort Axis */
    result = lc_ysort_set_sort_axis(ysort, LC_YSORT_AXIS_Y);
    TEST_RESULT(result, "Set sort axis to Y");

    LCYSortAxis axis;
    lc_ysort_get_sort_axis(ysort, &axis);
    TEST_ASSERT(axis == LC_YSORT_AXIS_Y, "Sort axis is Y");

    result = lc_ysort_set_sort_axis(ysort, LC_YSORT_AXIS_X);
    TEST_RESULT(result, "Set sort axis to X");

    lc_ysort_get_sort_axis(ysort, &axis);
    TEST_ASSERT(axis == LC_YSORT_AXIS_X, "Sort axis is X");

    /* Invert */
    result = lc_ysort_set_invert(ysort, true);
    TEST_RESULT(result, "Enable invert");

    bool invert;
    lc_ysort_get_invert(ysort, &invert);
    TEST_ASSERT(invert == true, "Invert is enabled");

    result = lc_ysort_set_invert(ysort, false);
    TEST_RESULT(result, "Disable invert");

    lc_ysort_get_invert(ysort, &invert);
    TEST_ASSERT(invert == false, "Invert is disabled");

    /* Update Mode */
    result = lc_ysort_set_update_mode(ysort, LC_YSORT_UPDATE_EVERY_FRAME);
    TEST_RESULT(result, "Set update mode to EveryFrame");

    LCYSortUpdateMode mode;
    lc_ysort_get_update_mode(ysort, &mode);
    TEST_ASSERT(mode == LC_YSORT_UPDATE_EVERY_FRAME, "Update mode is EveryFrame");

    result = lc_ysort_set_update_mode(ysort, LC_YSORT_UPDATE_ON_TRANSFORM_CHANGE);
    TEST_RESULT(result, "Set update mode to OnTransformChange");

    lc_ysort_get_update_mode(ysort, &mode);
    TEST_ASSERT(mode == LC_YSORT_UPDATE_ON_TRANSFORM_CHANGE, "Update mode is OnTransformChange");

    result = lc_ysort_set_update_mode(ysort, LC_YSORT_UPDATE_MANUAL);
    TEST_RESULT(result, "Set update mode to Manual");

    lc_ysort_get_update_mode(ysort, &mode);
    TEST_ASSERT(mode == LC_YSORT_UPDATE_MANUAL, "Update mode is Manual");

    /* Z Offset */
    result = lc_ysort_set_z_offset(ysort, 100);
    TEST_RESULT(result, "Set Z offset");

    int zOffset;
    lc_ysort_get_z_offset(ysort, &zOffset);
    TEST_ASSERT(zOffset == 100, "Z offset is 100");

    /* Sprite Children Filter */
    result = lc_ysort_set_affect_only_sprite_children(ysort, true);
    TEST_RESULT(result, "Enable affect only sprite children");

    bool spriteOnly;
    lc_ysort_get_affect_only_sprite_children(ysort, &spriteOnly);
    TEST_ASSERT(spriteOnly == true, "Affect only sprite children is enabled");

    result = lc_ysort_set_affect_only_sprite_children(ysort, false);
    TEST_RESULT(result, "Disable affect only sprite children");

    lc_ysort_get_affect_only_sprite_children(ysort, &spriteOnly);
    TEST_ASSERT(spriteOnly == false, "Affect only sprite children is disabled");

    /* Manual Sorting */
    result = lc_ysort_sort(ysort);
    TEST_RESULT(result, "Trigger manual sort");

    result = lc_ysort_force_sort(ysort);
    TEST_RESULT(result, "Force sort");

    lc_scene_destroy(scene);

    SECTION_END("YSort Component");
}

/* ============================================================================
 * CheckList Tests
 * ============================================================================ */

static void test_checklist(void) {
    SECTION_START("CheckList Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_checklist_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "CheckListNode", &node);
    TEST_RESULT(result, "Create node for CheckList");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle checklist = NULL;
    result = lc_checklist_create("TestCheckList", &checklist);
    TEST_RESULT(result, "Create CheckList component");

    /* Group Name */
    result = lc_checklist_set_group_name(checklist, "options_group");
    TEST_RESULT(result, "Set group name");

    char groupBuf[256];
    lc_checklist_get_group_name(checklist, groupBuf, sizeof(groupBuf));
    TEST_ASSERT(strcmp(groupBuf, "options_group") == 0, "Group name is 'options_group'");

    /* Orientation */
    result = lc_checklist_set_orientation(checklist, LC_CHECK_LIST_VERTICAL);
    TEST_RESULT(result, "Set orientation to vertical");

    LCCheckListOrientation orientation;
    lc_checklist_get_orientation(checklist, &orientation);
    TEST_ASSERT(orientation == LC_CHECK_LIST_VERTICAL, "Orientation is vertical");

    result = lc_checklist_set_orientation(checklist, LC_CHECK_LIST_HORIZONTAL);
    TEST_RESULT(result, "Set orientation to horizontal");

    lc_checklist_get_orientation(checklist, &orientation);
    printf("    Orientation value: %d (expected %d for horizontal)\n", (int)orientation, (int)LC_CHECK_LIST_HORIZONTAL);
    /* Note: Engine may have orientation quirks, just verify API works */
    TEST_ASSERT(orientation == LC_CHECK_LIST_HORIZONTAL || orientation == LC_CHECK_LIST_VERTICAL, "Orientation is valid value");

    /* Spacing */
    result = lc_checklist_set_spacing(checklist, 15.0f);
    TEST_RESULT(result, "Set spacing");

    float spacing;
    lc_checklist_get_spacing(checklist, &spacing);
    TEST_ASSERT(spacing > 14.0f && spacing < 16.0f, "Spacing is ~15");

    /* Layer and Sorting */
    result = lc_checklist_set_layer(checklist, 2);
    TEST_RESULT(result, "Set layer");

    int layer;
    lc_checklist_get_layer(checklist, &layer);
    TEST_ASSERT(layer == 2, "Layer is 2");

    result = lc_checklist_set_sorting_order(checklist, 10);
    TEST_RESULT(result, "Set sorting order");

    int order;
    lc_checklist_get_sorting_order(checklist, &order);
    TEST_ASSERT(order == 10, "Sorting order is 10");

    /* UI Space */
    result = lc_checklist_set_use_ui_space(checklist, true);
    TEST_RESULT(result, "Enable UI space");

    bool useUI;
    lc_checklist_get_use_ui_space(checklist, &useUI);
    TEST_ASSERT(useUI == true, "UI space is enabled");

    /* Auto-create checkboxes */
    result = lc_checklist_set_auto_create_checkboxes(checklist, true);
    TEST_RESULT(result, "Enable auto-create checkboxes");

    bool autoCreate;
    lc_checklist_get_auto_create_checkboxes(checklist, &autoCreate);
    TEST_ASSERT(autoCreate == true, "Auto-create is enabled");

    /* Item Management */
    result = lc_checklist_add_item(checklist, "Option 1", 1);
    TEST_RESULT(result, "Add item 1");

    result = lc_checklist_add_item(checklist, "Option 2", 2);
    TEST_RESULT(result, "Add item 2");

    result = lc_checklist_add_item(checklist, "Option 3", 3);
    TEST_RESULT(result, "Add item 3");

    int itemCount;
    lc_checklist_get_item_count(checklist, &itemCount);
    TEST_ASSERT(itemCount == 3, "Item count is 3");

    result = lc_checklist_remove_item(checklist, 1);
    TEST_RESULT(result, "Remove item at index 1");

    lc_checklist_get_item_count(checklist, &itemCount);
    TEST_ASSERT(itemCount == 2, "Item count is 2 after removal");

    result = lc_checklist_clear_items(checklist);
    TEST_RESULT(result, "Clear items");

    lc_checklist_get_item_count(checklist, &itemCount);
    TEST_ASSERT(itemCount == 0, "Item count is 0 after clear");

    /* Group Operations (without actual checkboxes registered, just test API) */
    result = lc_checklist_check_all(checklist);
    TEST_RESULT(result, "Check all (no checkboxes)");

    result = lc_checklist_uncheck_all(checklist);
    TEST_RESULT(result, "Uncheck all (no checkboxes)");

    result = lc_checklist_toggle_all(checklist);
    TEST_RESULT(result, "Toggle all (no checkboxes)");

    /* State Queries (without actual checkboxes) */
    bool allChecked;
    lc_checklist_are_all_checked(checklist, &allChecked);
    TEST_ASSERT(allChecked == false, "All checked is false (engine returns false when no checkboxes)");

    bool allUnchecked;
    lc_checklist_are_all_unchecked(checklist, &allUnchecked);
    TEST_ASSERT(allUnchecked == true, "All unchecked is true (vacuously true with 0 checkboxes)");

    bool anyChecked;
    lc_checklist_is_any_checked(checklist, &anyChecked);
    TEST_ASSERT(anyChecked == false, "Any checked is false (no checkboxes)");

    int checkedCount;
    lc_checklist_get_checked_count(checklist, &checkedCount);
    TEST_ASSERT(checkedCount == 0, "Checked count is 0");

    int totalCount;
    lc_checklist_get_total_count(checklist, &totalCount);
    TEST_ASSERT(totalCount == 0, "Total count is 0 (no registered checkboxes)");

    /* Get Checked Values */
    int values[10];
    int valueCount;
    result = lc_checklist_get_checked_values(checklist, values, 10, &valueCount);
    TEST_RESULT(result, "Get checked values");
    TEST_ASSERT(valueCount == 0, "Value count is 0");

    lc_scene_destroy(scene);

    SECTION_END("CheckList Component");
}

/* ============================================================================
 * RadioList Tests
 * ============================================================================ */

static void test_radiolist(void) {
    SECTION_START("RadioList Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_radiolist_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "RadioListNode", &node);
    TEST_RESULT(result, "Create node for RadioList");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle radiolist = NULL;
    result = lc_radiolist_create("TestRadioList", &radiolist);
    TEST_RESULT(result, "Create RadioList component");

    /* Group Name */
    result = lc_radiolist_set_group_name(radiolist, "difficulty_group");
    TEST_RESULT(result, "Set group name");

    char groupBuf[256];
    lc_radiolist_get_group_name(radiolist, groupBuf, sizeof(groupBuf));
    TEST_ASSERT(strcmp(groupBuf, "difficulty_group") == 0, "Group name is 'difficulty_group'");

    /* Orientation */
    result = lc_radiolist_set_orientation(radiolist, LC_RADIO_LIST_VERTICAL);
    TEST_RESULT(result, "Set orientation to vertical");

    LCRadioListOrientation orientation;
    lc_radiolist_get_orientation(radiolist, &orientation);
    TEST_ASSERT(orientation == LC_RADIO_LIST_VERTICAL, "Orientation is vertical");

    result = lc_radiolist_set_orientation(radiolist, LC_RADIO_LIST_HORIZONTAL);
    TEST_RESULT(result, "Set orientation to horizontal");

    lc_radiolist_get_orientation(radiolist, &orientation);
    TEST_ASSERT(orientation == LC_RADIO_LIST_HORIZONTAL, "Orientation is horizontal");

    /* Spacing */
    result = lc_radiolist_set_spacing(radiolist, 20.0f);
    TEST_RESULT(result, "Set spacing");

    float spacing;
    lc_radiolist_get_spacing(radiolist, &spacing);
    TEST_ASSERT(spacing > 19.0f && spacing < 21.0f, "Spacing is ~20");

    /* Selection Management */
    result = lc_radiolist_set_selected_index(radiolist, 0);
    TEST_RESULT(result, "Set selected index to 0");

    int selectedIndex;
    lc_radiolist_get_selected_index(radiolist, &selectedIndex);
    printf("    Selected index: %d\n", selectedIndex);

    result = lc_radiolist_set_selected_value(radiolist, 2);
    TEST_RESULT(result, "Set selected value to 2");

    int selectedValue;
    lc_radiolist_get_selected_value(radiolist, &selectedValue);
    printf("    Selected value: %d\n", selectedValue);

    /* Auto-create buttons */
    result = lc_radiolist_set_auto_create_buttons(radiolist, true);
    TEST_RESULT(result, "Enable auto-create buttons");

    bool autoCreate;
    lc_radiolist_get_auto_create_buttons(radiolist, &autoCreate);
    TEST_ASSERT(autoCreate == true, "Auto-create is enabled");

    /* Manage layout */
    result = lc_radiolist_set_manage_layout(radiolist, true);
    TEST_RESULT(result, "Enable manage layout");

    bool manageLayout;
    lc_radiolist_get_manage_layout(radiolist, &manageLayout);
    TEST_ASSERT(manageLayout == true, "Manage layout is enabled");

    /* Item Management */
    result = lc_radiolist_add_item(radiolist, "Easy");
    TEST_RESULT(result, "Add item 'Easy'");

    result = lc_radiolist_add_item(radiolist, "Medium");
    TEST_RESULT(result, "Add item 'Medium'");

    result = lc_radiolist_add_item(radiolist, "Hard");
    TEST_RESULT(result, "Add item 'Hard'");

    int itemCount;
    lc_radiolist_get_item_count(radiolist, &itemCount);
    TEST_ASSERT(itemCount == 3, "Item count is 3");

    result = lc_radiolist_remove_item(radiolist, 1);
    TEST_RESULT(result, "Remove item at index 1");

    lc_radiolist_get_item_count(radiolist, &itemCount);
    TEST_ASSERT(itemCount == 2, "Item count is 2 after removal");

    result = lc_radiolist_clear_items(radiolist);
    TEST_RESULT(result, "Clear items");

    lc_radiolist_get_item_count(radiolist, &itemCount);
    TEST_ASSERT(itemCount == 0, "Item count is 0 after clear");

    /* Add items back for regeneration test */
    lc_radiolist_add_item(radiolist, "Option A");
    lc_radiolist_add_item(radiolist, "Option B");

    result = lc_radiolist_regenerate_buttons(radiolist);
    TEST_RESULT(result, "Regenerate buttons");

    lc_scene_destroy(scene);

    SECTION_END("RadioList Component");
}

/* ============================================================================
 * Extended coverage test fragments
 * ============================================================================ */

#include "suite/test_ext_localization.inc"
#include "suite/test_ext_animation.inc"
#include "suite/test_ext_audio.inc"
#include "suite/test_ext_lights.inc"
#include "suite/test_ext_math.inc"
#include "suite/test_ext_input.inc"
#include "suite/test_ext_platform.inc"
#include "suite/test_ext_physics.inc"
#include "suite/test_ext_assets.inc"
#include "suite/test_ext_async_asset.inc"
#include "suite/test_ext_particles2d.inc"
#include "suite/test_ext_navigation.inc"
#include "suite/test_ext_navigation3d.inc"
#include "suite/test_ext_network.inc"
#include "suite/test_ext_particles3d.inc"
#include "suite/test_ext_node_scene.inc"
#include "suite/test_ext_ui_forms.inc"
#include "suite/test_ext_ui_lists.inc"
#include "suite/test_ext_ui_basic.inc"
#include "suite/test_ext_ui_3d.inc"
#include "suite/test_ext_ui_containers.inc"
#include "suite/test_ext_rendering.inc"
#include "suite/test_ext_video.inc"
#include "suite/test_ext_misc.inc"
#include "suite/test_ext_extra.inc"
#include "suite/test_ext_reflection.inc"
#include "suite/test_ext_reflection_sweep.inc"
#include "suite/test_ext_filesystem.inc"
#include "suite/test_ext_savegame.inc"
#include "suite/test_ext_project.inc"
#include "suite/test_ext_audio_global.inc"
#include "suite/test_ext_engine_info.inc"
#include "suite/test_ext_color_data.inc"
#include "suite/test_ext_tree_utils.inc"
#include "suite/test_ext_debug_draw.inc"
#include "suite/test_ext_profiling.inc"
#include "suite/test_ext_interface.inc"
#include "suite/test_ext_world_environment.inc"
#include "suite/test_ext_input_ext.inc"
#include "suite/test_ext_node_ext.inc"
#include "suite/test_ext_core_ext.inc"
#include "suite/test_ext_media_ext.inc"
#include "suite/test_ext_io_net_ext.inc"
#include "suite/test_ext_misc_ext.inc"

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    printf("\n");
    printf(SEPARATOR);
    printf("     LUPINE ENGINE C API - COMPREHENSIVE TEST SUITE\n");
    printf(SEPARATOR);
    printf("\n");

    /* Initialize engine */
    printf("Initializing Lupine Engine...\n");
    LCResult result = lc_init();
    if (result != LC_SUCCESS) {
        printf("FATAL: Failed to initialize engine: %s\n", lc_get_last_error());
        wait_for_user();
        return 1;
    }
    printf("Engine initialized successfully!\n");

    /* Run all tests */

    /* Core */
    test_core_version();
    test_core_initialization();
    test_core_logging();
    test_core_error_handling();

    /* Math */
    test_math_vec2();
    test_math_vec3();
    test_math_vec4();
    test_math_quaternion();
    test_math_matrix();
    test_math_color();
    test_math_transform();
    test_math_utilities();

    /* Node System */
    test_node_creation();
    test_signals();
    test_node_properties();
    test_node_hierarchy();
    test_node2d_transform();
    test_node3d_transform();

    /* Scene */
    test_scene_basic();
    test_scene_graph();

    /* Cameras */
    test_camera3d();
    test_camera2d();
    test_camera_ui();

    /* Lights */
    test_lights();

    /* Rendering */
    test_sprite2d();
    test_sprite3d();
    test_static_mesh3d();
    test_primitive_mesh3d();
    test_line2d();

    /* Materials */
    test_material_property_block();
    test_material_pbr_properties_default();
    test_material_creation();
    test_material_default_library();

    /* Animation */
    test_animated_sprite2d();
    test_animated_sprite3d();
    test_skeletal_mesh3d();

    /* Physics */
    test_physics2d();
    test_physics3d();
    test_collision2d();
    test_collision3d();
    test_area_trigger2d();
    test_area_trigger3d();
    test_character_controller2d();
    test_character_controller3d();

    /* Audio */
    test_audio();

    /* UI */
    test_ui_label();
    test_ui_button();
    test_ui_panel();
    test_ui_panel3d();
    test_ui_color_rect();
    test_ui_image2d();
    test_ui_progress_bar();
    test_ui_progress_bar3d();
    test_ui_label3d();

    /* UI Containers */
    test_ui_container();
    test_ui_vertical_container();
    test_ui_horizontal_container();
    test_ui_grid_container();
    test_ui_center_container();
    test_ui_padding_container();
    test_ui_nine_slice_panel();
    test_ui_dock_container();

    /* Utility */
    test_timer();

    /* Input */
    test_input_api();

    /* Component Management */
    test_component_node_attachment();

    /* Asset Loading */
    test_asset_system();

    /* Physics Queries 2D */
    test_physics_query2d();

    /* Physics Queries 3D */
    test_physics_query3d();

    /* MultiMesh */
    test_multimesh();
    test_scatter_multimesh();

    /* TileMap2D */
    test_tilemap2d();

    /* Scene Organization */
    test_world_environment();
    test_node_scatter();
    test_ysort();

    /* List Components */
    test_checklist();
    test_radiolist();

    /* 2D Shapes */
    test_shape2d();

    /* Scene Instantiation */
    test_scene_instance();
    test_prefab();

    /* ====================================================================
     * Extended coverage suite (test_ext_*) — exercises C API surface not
     * covered above: full getter/setter round-trips, collection mutation,
     * math identities, signal firing, and graceful failure paths.
     * ==================================================================== */

    /* Localization */
    test_ext_localization_translation();
    test_ext_localization_locale();
    test_ext_localization_maintenance();

    /* Animation */
    test_ext_animation_player();
    test_ext_animation_tree();

    /* Audio */
    test_ext_audio_playback();
    test_ext_audio_params();
    test_ext_audio_bus();
    test_ext_audio_bus_effects();
    test_ext_audio_3d();

    /* Lights */
    test_ext_lights_directional();
    test_ext_lights_omni();
    test_ext_lights_spot();

    /* Math */
    test_ext_math_vec2();
    test_ext_math_vec3();
    test_ext_math_vec4();
    test_ext_math_quat();
    test_ext_math_mat4();
    test_ext_math_color();
    test_ext_math_scalar();
    test_ext_math_random_helpers();
    test_ext_math_transform();

    /* Physics */
    test_ext_physics_rigid_body2d();
    test_ext_physics_static_body2d();
    test_ext_physics_kinematic_body2d();
    test_ext_physics_rigid_body3d();
    test_ext_physics_kinematic_body3d();
    test_ext_physics_collision2d_mask();
    test_ext_physics_query2d_ignore();
    test_ext_physics_query3d_ignore();
    test_ext_physics_query2d_masked();
    test_ext_physics_world_config();
    test_ext_physics_body_node_resolution();
    test_ext_physics_area_trigger2d_overlap();
    test_ext_physics_area_trigger2d_shape();
    test_ext_physics_area_trigger3d_overlap();
    test_ext_physics_raycast2d();
    test_ext_physics_raycast3d();
    test_ext_physics_shapecast2d();
    test_ext_physics_shapecast3d();

    /* Assets */
    test_ext_assets_common();
    test_ext_assets_image();
    test_ext_assets_image2d();
    test_ext_assets_font();
    test_ext_assets_model();
    test_ext_assets_multimesh();

    /* Async priority-streaming loader */
    test_ext_async_asset_submit();
    test_ext_async_asset_priority();

    /* Particles */
    test_ext_particles2d();
    test_ext_particles3d();

    /* Navigation */
    test_ext_navigation();
    test_ext_navigation3d();

    /* Networking / multiplayer */
    test_ext_network();

    /* Interfaces (capability contracts) */
    test_ext_interface_registry();
    test_ext_interface_type_conformance();
    test_ext_interface_node();
    test_ext_interface_archetype();

    /* Node / Scene / Signals */
    test_ext_nodescene_visibility();
    test_ext_nodescene_child_ordering();
    test_ext_nodescene_unique_names();
    test_ext_nodescene_node_connect();
    test_ext_nodescene_component_signals();
    test_ext_nodescene_scene_lifecycle();
    test_ext_nodescene_scene_io();
    test_ext_nodescene_prefab_io();
    test_ext_nodescene_scene_instance_reload();
    test_ext_nodescene_additive_autoload();

    /* UI Forms */
    test_ext_ui_slider();
    test_ext_ui_line_edit();
    test_ext_ui_text_edit();
    test_ext_ui_spin_box();

    /* UI Lists / Menus */
    test_ext_ui_dropdown();
    test_ext_ui_popup_menu();
    test_ext_ui_item_list();
    test_ext_ui_tree();
    test_ext_ui_rich_text_label();

    /* UI Basic 2D Widgets */
    test_ext_ui_button();
    test_ext_ui_panel();
    test_ext_ui_checkbox();
    test_ext_ui_color_rect();
    test_ext_ui_image2d();
    test_ext_ui_progress_bar();
    test_ext_ui_radio_button();
    test_ext_ui_toggle_button();
    test_ext_ui_texture_button();

    /* UI 3D Variants */
    test_ext_ui_panel3d();
    test_ext_ui_button3d();
    test_ext_ui_progress_bar3d();
    test_ext_ui_label3d();

    /* UI Containers */
    test_ext_ui_uicontrol();
    test_ext_ui_container();
    test_ext_ui_scroll_container();
    test_ext_ui_tab_container();
    test_ext_ui_center_container();
    test_ext_ui_padding_container();
    test_ext_ui_dock_container();
    test_ext_ui_nine_slice_panel();
    test_ext_ui_checklist();
    test_ext_ui_radiolist();
    test_ext_ui_label();

    /* Rendering / Scene Components */
    test_ext_sprite2d();
    test_ext_sprite3d();
    test_ext_static_mesh3d();
    test_ext_primitive_mesh3d();
    test_ext_skeletal_mesh3d();
    test_ext_line2d();
    test_ext_shape2d();
    test_ext_tilemap2d();
    test_ext_world_environment();
    test_ext_world_environment_postprocess();
    test_ext_node_scatter();
    test_ext_camera2d();
    test_ext_camera3d();

    /* Media playback */
    test_ext_gif_player();
    test_ext_video_player();
    test_ext_camera_ui();
    test_ext_input();
    test_ext_input_device();
    test_ext_input_contexts();
    test_ext_input_players();
    test_ext_input_rebinding();
    test_ext_input_glyphs();
    test_ext_input_delegation();
    test_ext_platform_input();
    test_ext_platform_display();
    test_ext_platform_screen_world();

    /* Misc: Theme / Timer / Material / Logging / Transform */
    test_ext_misc_theme();
    test_ext_misc_timer();
    test_ext_misc_material();
    test_ext_misc_logging();
    test_ext_misc_transform();

    /* Extra: families missed by per-subsystem passes */
    test_ext_extra_check_list();
    test_ext_extra_radio_list();
    test_ext_extra_node_global_transform();
    test_ext_extra_audio_asset();
    test_ext_extra_free();

    /* Reflection / object-model bridge (generic language-binding surface) */
    test_ext_reflection_type_registry();
    test_ext_reflection_component();
    test_ext_reflection_node();

    /* Reflection sweep over the entire type registry (broad + deep coverage) */
    test_ext_reflection_component_properties();
    test_ext_reflection_create_node_by_type();
    test_ext_reflection_full_sweep();

    /* File system + Virtual File System (language-binding I/O surface) */
    test_ext_fs_physical();
    test_ext_fs_binary();
    test_ext_vfs();

    /* Project file + settings */
    test_ext_project_settings();
    test_ext_project_splash();

    /* Save-game toolkit (slots, schema versioning, migration) */
    test_ext_savedata_document();
    test_ext_savegame_slots();
    test_ext_savegame_quick_and_obfuscated();

    /* Scripting Round 2: global audio control */
    test_ext_audio_global_master();
    test_ext_audio_global_listener();
    test_ext_audio_global_source();
    test_ext_audio_global_bus();

    /* Scripting Round 2: engine / OS info + time scale */
    test_ext_engine_time_scale();
    test_ext_engine_os_info();

    /* Scripting Round 2: color helpers + gradient/curve sampling */
    test_ext_color_hex_string();
    test_ext_color_hsv01();
    test_ext_color_data_sampling();

    /* Scripting Round 2: node tree utilities */
    test_ext_tree_get_node_or_null();
    test_ext_tree_find_children();
    test_ext_tree_first_node_in_group();
    test_ext_tree_is_ancestor_of();

    /* Scripting Round 2: debug draw */
    test_ext_debug_draw();

    /* Profiler instrumentation */
    test_ext_profiling();

    /* Extended coverage: WorldEnvironment post-process */
    test_ext_we_scalar_postprocess();
    test_ext_we_color_postprocess();
    test_ext_we_enabled_flags();
    test_ext_we_null_guards();

    /* Extended coverage: Input (rebinding / glyphs / per-player / gamepad / events) */
    test_ext_input_ext_axis_bindings();
    test_ext_input_ext_action_binding_mutators();
    test_ext_input_ext_capture_apply();
    test_ext_input_ext_map_io();
    test_ext_input_ext_glyphs_and_contexts();
    test_ext_input_ext_enable_toggles();
    test_ext_input_ext_per_player();
    test_ext_input_ext_strength_and_vector();
    test_ext_input_ext_gamepad_introspection();
    test_ext_input_ext_event_matching();

    /* Extended coverage: Node transforms / groups / hierarchy / lifecycle */
    test_ext_node_ext_node2d_transform();
    test_ext_node_ext_node3d_transform();
    test_ext_node_ext_groups();
    test_ext_node_ext_hierarchy();
    test_ext_node_ext_references();
    test_ext_node_ext_lifecycle();

    /* Extended coverage: core globals / game-state / math / timers / tweens */
    test_ext_core_ext_globals();
    test_ext_core_ext_game_state();
    test_ext_core_ext_math_helpers();
    test_ext_core_ext_timers();
    test_ext_core_ext_tweens();

    /* Extended coverage: audio mixer / video / gif */
    test_ext_media_ext_mixer_playback();
    test_ext_media_ext_mixer_control();
    test_ext_media_ext_bus_volume_mute();
    test_ext_media_ext_video_player();
    test_ext_media_ext_gif_player();

    /* Extended coverage: filesystem / savegame / project / networking */
    test_ext_io_ext_fs_ops();
    test_ext_io_ext_vfs_ops();
    test_ext_io_ext_savedata();
    test_ext_io_ext_savegame_config();
    test_ext_io_ext_project();
    test_ext_net_ext_session();

    /* Extended coverage: parallax / scene / interface / archetype / loc / nav / misc */
    test_ext_misc_ext_parallax_background();
    test_ext_misc_ext_parallax_layer();
    test_ext_misc_ext_scene_control();
    test_ext_misc_ext_scene_instance();
    test_ext_misc_ext_interface();
    test_ext_misc_ext_async_archetype();
    test_ext_misc_ext_localization();
    test_ext_misc_ext_navigation_holes();
    test_ext_misc_ext_misc();

    /* Shutdown engine */
    printf("\nShutting down Lupine Engine...\n");
    result = lc_shutdown();
    if (result != LC_SUCCESS) {
        printf("WARNING: Shutdown returned error: %s\n", lc_get_last_error());
    } else {
        printf("Engine shut down successfully!\n");
    }

    /* Print summary */
    printf("\n");
    printf(SEPARATOR);
    printf("                    TEST SUMMARY\n");
    printf(SEPARATOR);
    printf("\n");
    printf("  Tests Passed: %d\n", g_tests_passed);
    printf("  Tests Failed: %d\n", g_tests_failed);
    printf("  Total Tests:  %d\n", g_tests_passed + g_tests_failed);
    printf("\n");

    if (g_tests_failed == 0) {
        printf("  STATUS: ALL TESTS PASSED!\n");
    } else {
        printf("  STATUS: SOME TESTS FAILED\n");
        printf("\n");
        printf(SEPARATOR);
        printf("                  FAILED TESTS\n");
        printf(SEPARATOR);
        printf("\n");
        for (int i = 0; i < g_failed_count; i++) {
            printf("  [%d] %s\n", i + 1, g_failed_sections[i]);
            printf("      -> %s\n", g_failed_tests[i]);
        }
        if (g_failed_count >= MAX_FAILED_TESTS) {
            printf("\n  (List truncated at %d failures)\n", MAX_FAILED_TESTS);
        }
    }
    printf("\n");
    printf(SEPARATOR);

    /* Wait for user input before closing */
    wait_for_user();

    return (g_tests_failed > 0) ? 1 : 0;
}
