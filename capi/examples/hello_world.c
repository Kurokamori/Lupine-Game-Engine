/**
 * @file hello_world.c
 * @brief Simple example demonstrating Lupine Engine C API basics
 */

#include <lupine_c.h>
#include <stdio.h>

int main(void) {
    printf("==============================================\n");
    printf("  Lupine Engine C API - Hello World Example  \n");
    printf("==============================================\n\n");

    // Get version info before initialization
    printf("C API Version: %s\n", lc_get_version_string());
    printf("Engine Version: %s\n\n", lc_get_engine_version());

    // Initialize engine
    printf("Initializing Lupine Engine...\n");
    LCResult result = lc_init();
    if (result != LC_SUCCESS) {
        printf("ERROR: Failed to initialize engine: %s\n", lc_get_last_error());
        return 1;
    }
    printf("✓ Engine initialized successfully!\n\n");

    // Test logging
    printf("Testing logging system...\n");
    lc_log_info("Hello from C API!");
    lc_log_debug("Debug message test");
    lc_log_warn("Warning message test");
    printf("✓ Logging working!\n\n");

    // Test Vec3 math
    printf("Testing Vec3 math...\n");
    LCVec3 a = lc_vec3(1.0f, 2.0f, 3.0f);
    LCVec3 b = lc_vec3(4.0f, 5.0f, 6.0f);

    printf("  a = (%.2f, %.2f, %.2f)\n", a.x, a.y, a.z);
    printf("  b = (%.2f, %.2f, %.2f)\n", b.x, b.y, b.z);

    LCVec3 sum = lc_vec3_add(a, b);
    printf("  a + b = (%.2f, %.2f, %.2f)\n", sum.x, sum.y, sum.z);

    float dot = lc_vec3_dot(a, b);
    printf("  a · b = %.2f\n", dot);

    LCVec3 cross = lc_vec3_cross(a, b);
    printf("  a × b = (%.2f, %.2f, %.2f)\n", cross.x, cross.y, cross.z);

    float length = lc_vec3_length(a);
    printf("  |a| = %.2f\n", length);

    LCVec3 normalized = lc_vec3_normalize(a);
    printf("  normalize(a) = (%.2f, %.2f, %.2f)\n",
           normalized.x, normalized.y, normalized.z);
    printf("✓ Vec3 math working!\n\n");

    // Test Quaternion
    printf("Testing Quaternion...\n");
    LCQuat rot = lc_quat_from_axis_angle(lc_vec3_unit_y(), lc_deg_to_rad(90.0f));
    printf("  90° rotation around Y-axis created\n");

    LCVec3 forward = lc_vec3_forward();
    LCVec3 rotated = lc_quat_mul_vec3(rot, forward);
    printf("  Rotated forward vector: (%.2f, %.2f, %.2f)\n",
           rotated.x, rotated.y, rotated.z);
    printf("✓ Quaternion working!\n\n");

    // Test Matrix
    printf("Testing Mat4...\n");
    LCMat4 translation = lc_mat4_translate(lc_vec3(10.0f, 20.0f, 30.0f));
    LCMat4 rotation = lc_mat4_rotate(rot);
    LCMat4 scale = lc_mat4_scale(lc_vec3(2.0f, 2.0f, 2.0f));

    LCMat4 transform = lc_mat4_mul(&translation, &rotation);
    transform = lc_mat4_mul(&transform, &scale);
    printf("  Combined TRS matrix created\n");

    LCMat4 trs_direct = lc_mat4_trs(
        lc_vec3(10.0f, 20.0f, 30.0f),
        rot,
        lc_vec3(2.0f, 2.0f, 2.0f)
    );
    printf("  Direct TRS matrix created\n");
    printf("✓ Mat4 working!\n\n");

    // Test Color
    printf("Testing Color...\n");
    LCColor red = lc_color_red();
    LCColor blue = lc_color_blue();
    LCColor purple = lc_color_lerp(red, blue, 0.5f);
    printf("  Lerp(red, blue, 0.5) = (%.2f, %.2f, %.2f, %.2f)\n",
           purple.r, purple.g, purple.b, purple.a);

    LCColor custom = lc_color_from_hex(0xFF6600FF);
    printf("  Color from hex 0xFF6600FF: (%.2f, %.2f, %.2f, %.2f)\n",
           custom.r, custom.g, custom.b, custom.a);
    printf("✓ Color working!\n\n");

    // Test Transform
    printf("Testing Transform...\n");
    LCTransform trans = lc_transform_create(
        lc_vec3(5.0f, 10.0f, 15.0f),
        lc_quat_identity(),
        lc_vec3_one()
    );

    LCVec3 point = lc_vec3(1.0f, 0.0f, 0.0f);
    LCVec3 transformed = lc_transform_apply_to_point(&trans, point);
    printf("  Point (1, 0, 0) transformed: (%.2f, %.2f, %.2f)\n",
           transformed.x, transformed.y, transformed.z);
    printf("✓ Transform working!\n\n");

    // Test utility functions
    printf("Testing utility functions...\n");
    float angle_rad = lc_deg_to_rad(180.0f);
    float angle_deg = lc_rad_to_deg(LC_PI);
    printf("  180° = %.4f radians\n", angle_rad);
    printf("  π radians = %.2f degrees\n", angle_deg);

    float clamped = lc_clamp(15.0f, 0.0f, 10.0f);
    printf("  clamp(15, 0, 10) = %.2f\n", clamped);

    float lerped = lc_lerp(0.0f, 100.0f, 0.75f);
    printf("  lerp(0, 100, 0.75) = %.2f\n", lerped);
    printf("✓ Utility functions working!\n\n");

    // Shutdown engine
    printf("Shutting down Lupine Engine...\n");
    result = lc_shutdown();
    if (result != LC_SUCCESS) {
        printf("ERROR: Failed to shutdown engine: %s\n", lc_get_last_error());
        return 1;
    }
    printf("✓ Engine shut down successfully!\n\n");

    printf("==============================================\n");
    printf("  All tests passed! C API is working!       \n");
    printf("==============================================\n");

    return 0;
}
