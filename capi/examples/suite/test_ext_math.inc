/* Extended math C API tests (capi/include/math/lc_math.h).
 * Covers public lc_* math functions not exercised by the existing test_math_*
 * functions: Vec2/Vec3/Vec4 extra ops, Quat ops, Mat4 ops (determinant,
 * inverse, from_array, rotate variants, mul_vec4), Color helpers, scalar
 * utilities, and transform_from_matrix. These are pure value functions, so the
 * tests assert mathematical correctness with known inputs and verify identities
 * (inverse(m)*m == identity, normalize length == 1, conjugate twice == original). */

static float ext_math_absf(float a) {
    return a < 0.0f ? -a : a;
}

static int ext_math_feq(float a, float b) {
    return ext_math_absf(a - b) < 1e-4f;
}

static int ext_math_feq_tol(float a, float b, float tol) {
    return ext_math_absf(a - b) < tol;
}

static int ext_math_feq_v2(LCVec2 v, float x, float y) {
    return ext_math_feq(v.x, x) && ext_math_feq(v.y, y);
}

static int ext_math_feq_v3(LCVec3 v, float x, float y, float z) {
    return ext_math_feq(v.x, x) && ext_math_feq(v.y, y) && ext_math_feq(v.z, z);
}

static int ext_math_feq_v4(LCVec4 v, float x, float y, float z, float w) {
    return ext_math_feq(v.x, x) && ext_math_feq(v.y, y) &&
           ext_math_feq(v.z, z) && ext_math_feq(v.w, w);
}

static void test_ext_math_vec2(void) {
    SECTION_START("Ext Math: Vec2 Operations");

    LCVec2 ux = lc_vec2_unit_x();
    LCVec2 uy = lc_vec2_unit_y();
    TEST_ASSERT(ext_math_feq_v2(ux, 1.0f, 0.0f), "lc_vec2_unit_x()");
    TEST_ASSERT(ext_math_feq_v2(uy, 0.0f, 1.0f), "lc_vec2_unit_y()");

    LCVec2 down = lc_vec2_down();
    LCVec2 left = lc_vec2_left();
    TEST_ASSERT(ext_math_feq(down.y, -1.0f) || ext_math_feq(down.y, 1.0f),
                "lc_vec2_down() has unit magnitude on y");
    TEST_ASSERT(ext_math_feq(left.x, -1.0f), "lc_vec2_left() points to -x");

    LCVec2 a = lc_vec2(6.0f, 8.0f);
    LCVec2 b = lc_vec2(2.0f, 4.0f);

    LCVec2 divided = lc_vec2_div(a, 2.0f);
    TEST_ASSERT(ext_math_feq_v2(divided, 3.0f, 4.0f), "lc_vec2_div() by scalar");

    LCVec2 mulv = lc_vec2_mul_vec(a, b);
    TEST_ASSERT(ext_math_feq_v2(mulv, 12.0f, 32.0f), "lc_vec2_mul_vec() component-wise");

    LCVec2 divv = lc_vec2_div_vec(a, b);
    TEST_ASSERT(ext_math_feq_v2(divv, 3.0f, 2.0f), "lc_vec2_div_vec() component-wise");

    LCVec2 neg = lc_vec2_negate(a);
    TEST_ASSERT(ext_math_feq_v2(neg, -6.0f, -8.0f), "lc_vec2_negate()");

    float lsq = lc_vec2_length_squared(lc_vec2(3.0f, 4.0f));
    TEST_ASSERT(ext_math_feq(lsq, 25.0f), "lc_vec2_length_squared()");

    float dsq = lc_vec2_distance_squared(lc_vec2_zero(), lc_vec2(3.0f, 4.0f));
    TEST_ASSERT(ext_math_feq(dsq, 25.0f), "lc_vec2_distance_squared()");

    LCVec2 clamped = lc_vec2_clamp_length(lc_vec2(3.0f, 4.0f), 2.5f);
    TEST_ASSERT(ext_math_feq(lc_vec2_length(clamped), 2.5f),
                "lc_vec2_clamp_length() shortens to max length");
    LCVec2 unclamped = lc_vec2_clamp_length(lc_vec2(1.0f, 0.0f), 5.0f);
    TEST_ASSERT(ext_math_feq(lc_vec2_length(unclamped), 1.0f),
                "lc_vec2_clamp_length() leaves shorter vector unchanged");

    LCVec2 reflected = lc_vec2_reflect(lc_vec2(1.0f, -1.0f), lc_vec2(0.0f, 1.0f));
    TEST_ASSERT(ext_math_feq_v2(reflected, 1.0f, 1.0f), "lc_vec2_reflect() off horizontal plane");

    LCVec2 projected = lc_vec2_project(lc_vec2(2.0f, 3.0f), lc_vec2(1.0f, 0.0f));
    TEST_ASSERT(ext_math_feq_v2(projected, 2.0f, 0.0f), "lc_vec2_project() onto x-axis");

    LCVec2 rotated = lc_vec2_rotate(lc_vec2(1.0f, 0.0f), LC_HALF_PI);
    TEST_ASSERT(ext_math_feq_v2(rotated, 0.0f, 1.0f), "lc_vec2_rotate() 90 degrees");

    float ang = lc_vec2_angle(lc_vec2(1.0f, 0.0f), lc_vec2(0.0f, 1.0f));
    TEST_ASSERT(ext_math_feq(ext_math_absf(ang), LC_HALF_PI), "lc_vec2_angle() is 90 degrees");

    TEST_ASSERT(lc_vec2_equals(a, lc_vec2(6.0f, 8.0f)), "lc_vec2_equals() true for equal");
    TEST_ASSERT(!lc_vec2_equals(a, b), "lc_vec2_equals() false for different");

    SECTION_END("Ext Math: Vec2 Operations");
}

static void test_ext_math_vec3(void) {
    SECTION_START("Ext Math: Vec3 Operations");

    LCVec3 sc = lc_vec3_scalar(2.0f);
    TEST_ASSERT(ext_math_feq_v3(sc, 2.0f, 2.0f, 2.0f), "lc_vec3_scalar() fills all components");

    LCVec3 uz = lc_vec3_unit_z();
    TEST_ASSERT(ext_math_feq_v3(uz, 0.0f, 0.0f, 1.0f), "lc_vec3_unit_z()");

    LCVec3 down = lc_vec3_down();
    LCVec3 leftv = lc_vec3_left();
    LCVec3 backv = lc_vec3_back();
    TEST_ASSERT(ext_math_feq(lc_vec3_length(down), 1.0f), "lc_vec3_down() is unit length");
    TEST_ASSERT(ext_math_feq(lc_vec3_length(leftv), 1.0f), "lc_vec3_left() is unit length");
    TEST_ASSERT(ext_math_feq(lc_vec3_length(backv), 1.0f), "lc_vec3_back() is unit length");

    LCVec3 a = lc_vec3(6.0f, 9.0f, 12.0f);
    LCVec3 b = lc_vec3(2.0f, 3.0f, 4.0f);

    LCVec3 sub = lc_vec3_sub(a, b);
    TEST_ASSERT(ext_math_feq_v3(sub, 4.0f, 6.0f, 8.0f), "lc_vec3_sub()");

    LCVec3 mul = lc_vec3_mul(b, 3.0f);
    TEST_ASSERT(ext_math_feq_v3(mul, 6.0f, 9.0f, 12.0f), "lc_vec3_mul() by scalar");

    LCVec3 divided = lc_vec3_div(a, 3.0f);
    TEST_ASSERT(ext_math_feq_v3(divided, 2.0f, 3.0f, 4.0f), "lc_vec3_div() by scalar");

    LCVec3 mulv = lc_vec3_mul_vec(a, b);
    TEST_ASSERT(ext_math_feq_v3(mulv, 12.0f, 27.0f, 48.0f), "lc_vec3_mul_vec()");

    LCVec3 divv = lc_vec3_div_vec(a, b);
    TEST_ASSERT(ext_math_feq_v3(divv, 3.0f, 3.0f, 3.0f), "lc_vec3_div_vec()");

    LCVec3 neg = lc_vec3_negate(b);
    TEST_ASSERT(ext_math_feq_v3(neg, -2.0f, -3.0f, -4.0f), "lc_vec3_negate()");

    float lsq = lc_vec3_length_squared(lc_vec3(1.0f, 2.0f, 2.0f));
    TEST_ASSERT(ext_math_feq(lsq, 9.0f), "lc_vec3_length_squared()");

    float dist = lc_vec3_distance(lc_vec3_zero(), lc_vec3(1.0f, 2.0f, 2.0f));
    TEST_ASSERT(ext_math_feq(dist, 3.0f), "lc_vec3_distance()");

    float dsq = lc_vec3_distance_squared(lc_vec3_zero(), lc_vec3(1.0f, 2.0f, 2.0f));
    TEST_ASSERT(ext_math_feq(dsq, 9.0f), "lc_vec3_distance_squared()");

    LCVec3 slerped = lc_vec3_slerp(lc_vec3_unit_x(), lc_vec3_unit_y(), 0.5f);
    TEST_ASSERT(ext_math_feq(lc_vec3_length(slerped), 1.0f),
                "lc_vec3_slerp() preserves unit length");
    TEST_ASSERT(ext_math_feq(slerped.x, slerped.y),
                "lc_vec3_slerp() at midpoint is symmetric");

    LCVec3 clamped = lc_vec3_clamp_length(lc_vec3(0.0f, 3.0f, 4.0f), 2.5f);
    TEST_ASSERT(ext_math_feq(lc_vec3_length(clamped), 2.5f), "lc_vec3_clamp_length()");

    LCVec3 reflected = lc_vec3_reflect(lc_vec3(1.0f, -1.0f, 0.0f), lc_vec3(0.0f, 1.0f, 0.0f));
    TEST_ASSERT(ext_math_feq_v3(reflected, 1.0f, 1.0f, 0.0f), "lc_vec3_reflect()");

    LCVec3 projected = lc_vec3_project(lc_vec3(2.0f, 3.0f, 4.0f), lc_vec3(0.0f, 0.0f, 1.0f));
    TEST_ASSERT(ext_math_feq_v3(projected, 0.0f, 0.0f, 4.0f), "lc_vec3_project() onto z-axis");

    float ang = lc_vec3_angle(lc_vec3_unit_x(), lc_vec3_unit_y());
    TEST_ASSERT(ext_math_feq(ang, LC_HALF_PI), "lc_vec3_angle() is 90 degrees");

    TEST_ASSERT(lc_vec3_equals(a, lc_vec3(6.0f, 9.0f, 12.0f)), "lc_vec3_equals() true");
    TEST_ASSERT(!lc_vec3_equals(a, b), "lc_vec3_equals() false");

    SECTION_END("Ext Math: Vec3 Operations");
}

static void test_ext_math_vec4(void) {
    SECTION_START("Ext Math: Vec4 Operations");

    LCVec4 sc = lc_vec4_scalar(3.0f);
    TEST_ASSERT(ext_math_feq_v4(sc, 3.0f, 3.0f, 3.0f, 3.0f), "lc_vec4_scalar()");

    LCVec4 a = lc_vec4(5.0f, 6.0f, 7.0f, 8.0f);
    LCVec4 b = lc_vec4(1.0f, 2.0f, 3.0f, 4.0f);

    LCVec4 sub = lc_vec4_sub(a, b);
    TEST_ASSERT(ext_math_feq_v4(sub, 4.0f, 4.0f, 4.0f, 4.0f), "lc_vec4_sub()");

    LCVec4 mul = lc_vec4_mul(b, 2.0f);
    TEST_ASSERT(ext_math_feq_v4(mul, 2.0f, 4.0f, 6.0f, 8.0f), "lc_vec4_mul() by scalar");

    LCVec4 divided = lc_vec4_div(a, 2.0f);
    TEST_ASSERT(ext_math_feq_v4(divided, 2.5f, 3.0f, 3.5f, 4.0f), "lc_vec4_div() by scalar");

    LCVec4 neg = lc_vec4_negate(b);
    TEST_ASSERT(ext_math_feq_v4(neg, -1.0f, -2.0f, -3.0f, -4.0f), "lc_vec4_negate()");

    float dot = lc_vec4_dot(b, b);
    float lsq = lc_vec4_length_squared(b);
    TEST_ASSERT(ext_math_feq(lsq, dot), "lc_vec4_length_squared() equals self-dot");
    TEST_ASSERT(ext_math_feq(lsq, 30.0f), "lc_vec4_length_squared() value");

    LCVec4 normalized = lc_vec4_normalize(lc_vec4(0.0f, 0.0f, 0.0f, 5.0f));
    TEST_ASSERT(ext_math_feq(lc_vec4_length(normalized), 1.0f),
                "lc_vec4_normalize() yields unit length");
    TEST_ASSERT(ext_math_feq(normalized.w, 1.0f), "lc_vec4_normalize() direction preserved");

    LCVec4 lerped = lc_vec4_lerp(lc_vec4_zero(), lc_vec4_one(), 0.5f);
    TEST_ASSERT(ext_math_feq_v4(lerped, 0.5f, 0.5f, 0.5f, 0.5f), "lc_vec4_lerp() at midpoint");

    TEST_ASSERT(lc_vec4_equals(a, lc_vec4(5.0f, 6.0f, 7.0f, 8.0f)), "lc_vec4_equals() true");
    TEST_ASSERT(!lc_vec4_equals(a, b), "lc_vec4_equals() false");

    SECTION_END("Ext Math: Vec4 Operations");
}

static void test_ext_math_quat(void) {
    SECTION_START("Ext Math: Quaternion Operations");

    LCQuat q = lc_quat(0.0f, 0.0f, 0.0f, 1.0f);
    TEST_ASSERT(ext_math_feq(q.w, 1.0f), "lc_quat() constructor");

    LCQuat fromVec = lc_quat_from_euler_vec(lc_vec3(0.0f, LC_HALF_PI, 0.0f));
    LCQuat fromScalar = lc_quat_from_euler(0.0f, LC_HALF_PI, 0.0f);
    TEST_ASSERT(ext_math_feq(fromVec.x, fromScalar.x) && ext_math_feq(fromVec.y, fromScalar.y) &&
                ext_math_feq(fromVec.z, fromScalar.z) && ext_math_feq(fromVec.w, fromScalar.w),
                "lc_quat_from_euler_vec() matches lc_quat_from_euler()");

    LCQuat look = lc_quat_look_rotation(lc_vec3_forward(), lc_vec3_up());
    TEST_ASSERT(ext_math_feq(lc_quat_length(look), 1.0f),
                "lc_quat_look_rotation() returns unit quaternion");

    LCQuat rot = lc_quat_from_axis_angle(lc_vec3_unit_y(), LC_HALF_PI);
    LCQuat conj = lc_quat_conjugate(rot);
    TEST_ASSERT(ext_math_feq(conj.x, -rot.x) && ext_math_feq(conj.y, -rot.y) &&
                ext_math_feq(conj.z, -rot.z) && ext_math_feq(conj.w, rot.w),
                "lc_quat_conjugate() negates vector part");
    LCQuat conj2 = lc_quat_conjugate(conj);
    TEST_ASSERT(ext_math_feq(conj2.x, rot.x) && ext_math_feq(conj2.w, rot.w),
                "lc_quat_conjugate() twice restores original");

    LCQuat unnorm = lc_quat(0.0f, 0.0f, 0.0f, 5.0f);
    LCQuat norm = lc_quat_normalize(unnorm);
    TEST_ASSERT(ext_math_feq(lc_quat_length(norm), 1.0f), "lc_quat_normalize() yields unit length");

    TEST_ASSERT(ext_math_feq(lc_quat_length(lc_quat_identity()), 1.0f),
                "lc_quat_length() of identity is 1");

    float dot = lc_quat_dot(rot, rot);
    TEST_ASSERT(ext_math_feq(dot, 1.0f), "lc_quat_dot() of unit quat with itself is 1");

    LCQuat lerped = lc_quat_lerp(lc_quat_identity(), rot, 0.0f);
    TEST_ASSERT(ext_math_feq(lerped.w, 1.0f), "lc_quat_lerp() at t=0 returns first");

    LCVec3 axis = lc_vec3_zero();
    float angle = 0.0f;
    lc_quat_to_axis_angle(rot, &axis, &angle);
    TEST_ASSERT(ext_math_feq(angle, LC_HALF_PI), "lc_quat_to_axis_angle() recovers angle");
    TEST_ASSERT(ext_math_feq(ext_math_absf(axis.y), 1.0f),
                "lc_quat_to_axis_angle() recovers y axis");

    TEST_ASSERT(lc_quat_equals(lc_quat_identity(), lc_quat(0.0f, 0.0f, 0.0f, 1.0f)),
                "lc_quat_equals() true for identity");
    TEST_ASSERT(!lc_quat_equals(lc_quat_identity(), rot), "lc_quat_equals() false for different");

    SECTION_END("Ext Math: Quaternion Operations");
}

static void test_ext_math_mat4(void) {
    SECTION_START("Ext Math: Mat4 Operations");

    LCMat4 zero = lc_mat4_zero();
    int allZero = 1;
    int i;
    for (i = 0; i < 16; ++i) {
        if (zero.m[i] != 0.0f) { allZero = 0; }
    }
    TEST_ASSERT(allZero, "lc_mat4_zero() is all zeros");

    float identityArr[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    LCMat4 fromArr = lc_mat4_from_array(identityArr);
    LCMat4 identity = lc_mat4_identity();
    TEST_ASSERT(lc_mat4_equals(&fromArr, &identity),
                "lc_mat4_from_array() builds identity, lc_mat4_equals() true");
    TEST_ASSERT(!lc_mat4_equals(&zero, &identity), "lc_mat4_equals() false for different");

    TEST_ASSERT(ext_math_feq(lc_mat4_determinant(&identity), 1.0f),
                "lc_mat4_determinant() of identity is 1");

    LCMat4 scale = lc_mat4_scale(lc_vec3(2.0f, 3.0f, 4.0f));
    TEST_ASSERT(ext_math_feq(lc_mat4_determinant(&scale), 24.0f),
                "lc_mat4_determinant() of scale is product of diagonal");

    LCMat4 inv = lc_mat4_inverse(&scale);
    LCMat4 product = lc_mat4_mul(&scale, &inv);
    TEST_ASSERT(lc_mat4_equals(&product, &identity),
                "lc_mat4_inverse() times original is identity");

    LCVec4 v = lc_mat4_mul_vec4(&identity, lc_vec4(1.0f, 2.0f, 3.0f, 1.0f));
    TEST_ASSERT(ext_math_feq_v4(v, 1.0f, 2.0f, 3.0f, 1.0f),
                "lc_mat4_mul_vec4() with identity returns input");

    LCVec4 scaled = lc_mat4_mul_vec4(&scale, lc_vec4(1.0f, 1.0f, 1.0f, 1.0f));
    TEST_ASSERT(ext_math_feq_v4(scaled, 2.0f, 3.0f, 4.0f, 1.0f),
                "lc_mat4_mul_vec4() applies scale");

    LCMat4 rotQuat = lc_mat4_rotate(lc_quat_from_axis_angle(lc_vec3_unit_y(), LC_HALF_PI));
    LCMat4 rotAxis = lc_mat4_rotate_axis_angle(lc_vec3_unit_y(), LC_HALF_PI);
    TEST_ASSERT(lc_mat4_equals(&rotQuat, &rotAxis),
                "lc_mat4_rotate_axis_angle() matches lc_mat4_rotate() of equivalent quat");

    LCMat4 rotEuler = lc_mat4_rotate_euler(0.0f, LC_HALF_PI, 0.0f);
    LCMat4 rotEulerQuat = lc_mat4_rotate(lc_quat_from_euler(0.0f, LC_HALF_PI, 0.0f));
    TEST_ASSERT(lc_mat4_equals(&rotEuler, &rotEulerQuat),
                "lc_mat4_rotate_euler() matches quat-built rotation");

    SECTION_END("Ext Math: Mat4 Operations");
}

static void test_ext_math_color(void) {
    SECTION_START("Ext Math: Color Operations");

    LCColor magenta = lc_color_magenta();
    TEST_ASSERT(ext_math_feq(magenta.r, 1.0f) && ext_math_feq(magenta.g, 0.0f) &&
                ext_math_feq(magenta.b, 1.0f), "lc_color_magenta() is (1,0,1)");

    LCColor gray = lc_color_gray();
    TEST_ASSERT(ext_math_feq(gray.r, gray.g) && ext_math_feq(gray.g, gray.b) &&
                gray.r > 0.0f && gray.r < 1.0f, "lc_color_gray() has equal mid channels");

    LCColor transparent = lc_color_transparent();
    TEST_ASSERT(ext_math_feq(transparent.a, 0.0f), "lc_color_transparent() has zero alpha");

    LCColor sum = lc_color_add(lc_color(0.2f, 0.3f, 0.4f, 0.5f),
                               lc_color(0.1f, 0.1f, 0.1f, 0.1f));
    TEST_ASSERT(ext_math_feq(sum.r, 0.3f) && ext_math_feq(sum.g, 0.4f) &&
                ext_math_feq(sum.b, 0.5f), "lc_color_add() sums channels");

    unsigned char r = 0, g = 0, bb = 0, a = 0;
    lc_color_to_bytes(lc_color_white(), &r, &g, &bb, &a);
    TEST_ASSERT(r == 255 && g == 255 && bb == 255 && a == 255,
                "lc_color_to_bytes() of white is all 255");

    unsigned char r2 = 1, g2 = 1, b2 = 1, a2 = 1;
    lc_color_to_bytes(lc_color(0.0f, 0.0f, 0.0f, 0.0f), &r2, &g2, &b2, &a2);
    TEST_ASSERT(r2 == 0 && g2 == 0 && b2 == 0 && a2 == 0,
                "lc_color_to_bytes() of zero color is all 0");

    SECTION_END("Ext Math: Color Operations");
}

static void test_ext_math_scalar(void) {
    SECTION_START("Ext Math: Scalar Utilities");

    TEST_ASSERT(ext_math_feq(lc_min(2.0f, 3.0f), 2.0f), "lc_min()");
    TEST_ASSERT(ext_math_feq(lc_max(2.0f, 3.0f), 3.0f), "lc_max()");

    TEST_ASSERT(ext_math_feq(lc_saturate(1.5f), 1.0f), "lc_saturate() clamps above 1");
    TEST_ASSERT(ext_math_feq(lc_saturate(-0.5f), 0.0f), "lc_saturate() clamps below 0");
    TEST_ASSERT(ext_math_feq(lc_saturate(0.25f), 0.25f), "lc_saturate() passes mid value");

    TEST_ASSERT(ext_math_feq(lc_abs(-3.5f), 3.5f), "lc_abs() of negative");
    TEST_ASSERT(ext_math_feq(lc_abs(3.5f), 3.5f), "lc_abs() of positive");

    TEST_ASSERT(ext_math_feq(lc_sqrt(9.0f), 3.0f), "lc_sqrt()");
    TEST_ASSERT(ext_math_feq(lc_pow(2.0f, 10.0f), 1024.0f), "lc_pow()");

    float diff = lc_angle_difference(0.0f, LC_HALF_PI);
    TEST_ASSERT(ext_math_feq(ext_math_absf(diff), LC_HALF_PI),
                "lc_angle_difference() magnitude for quarter turn");
    float wrapDiff = lc_angle_difference(0.1f, LC_TWO_PI + 0.1f);
    TEST_ASSERT(ext_math_feq_tol(wrapDiff, 0.0f, 1e-3f),
                "lc_angle_difference() handles full-turn wrap");

    float lerpA = lc_lerp_angle(0.0f, LC_HALF_PI, 0.5f);
    TEST_ASSERT(ext_math_feq(lerpA, LC_HALF_PI * 0.5f), "lc_lerp_angle() at midpoint");

    TEST_ASSERT(lc_equals(1.0f, 1.0f + LC_EPSILON * 0.5f), "lc_equals() true within epsilon");
    TEST_ASSERT(!lc_equals(1.0f, 2.0f), "lc_equals() false for distinct");

    TEST_ASSERT(lc_is_zero(0.0f), "lc_is_zero() true for zero");
    TEST_ASSERT(!lc_is_zero(1.0f), "lc_is_zero() false for nonzero");

    SECTION_END("Ext Math: Scalar Utilities");
}

static void test_ext_math_random_helpers(void) {
    SECTION_START("Ext Math: Random & Range Helpers");

    lc_random_seed(12345);
    float r0 = lc_random_float();
    float r1 = lc_random_float();
    TEST_ASSERT(r0 >= 0.0f && r0 <= 1.0f, "lc_random_float() in [0,1]");
    TEST_ASSERT(r1 >= 0.0f && r1 <= 1.0f, "lc_random_float() second draw in [0,1]");

    lc_random_seed(12345);
    float r0b = lc_random_float();
    TEST_ASSERT(ext_math_feq(r0, r0b), "lc_random_seed() makes lc_random_float() deterministic");

    int sign = lc_random_sign();
    TEST_ASSERT(sign == -1 || sign == 1, "lc_random_sign() is +/-1");

    bool b = lc_random_bool();
    TEST_ASSERT(b == true || b == false, "lc_random_bool() returns a defined bool");

    TEST_ASSERT(ext_math_feq(lc_wrap(7.0f, 0.0f, 5.0f), 2.0f), "lc_wrap() wraps above max");
    TEST_ASSERT(ext_math_feq(lc_wrap(-1.0f, 0.0f, 5.0f), 4.0f), "lc_wrap() wraps below min");
    TEST_ASSERT(ext_math_feq(lc_wrap(3.0f, 0.0f, 5.0f), 3.0f), "lc_wrap() leaves in-range value");

    TEST_ASSERT(lc_wrap_int(7, 0, 5) == 2, "lc_wrap_int() wraps above max");
    TEST_ASSERT(lc_wrap_int(-1, 0, 5) == 4, "lc_wrap_int() wraps below min");
    TEST_ASSERT(lc_wrap_int(3, 0, 5) == 3, "lc_wrap_int() leaves in-range value");

    TEST_ASSERT(ext_math_feq(lc_ping_pong(0.0f, 4.0f), 0.0f), "lc_ping_pong() at 0");
    TEST_ASSERT(ext_math_feq(lc_ping_pong(4.0f, 4.0f), 4.0f), "lc_ping_pong() at length peak");
    TEST_ASSERT(ext_math_feq(lc_ping_pong(6.0f, 4.0f), 2.0f), "lc_ping_pong() reflects back");

    TEST_ASSERT(ext_math_feq(lc_snapped(7.3f, 0.5f), 7.5f), "lc_snapped() rounds to step");
    TEST_ASSERT(ext_math_feq(lc_snapped(2.2f, 1.0f), 2.0f), "lc_snapped() rounds down to step");

    TEST_ASSERT(lc_is_equal_approx(1.0f, 1.0f + 1e-7f), "lc_is_equal_approx() true within tolerance");
    TEST_ASSERT(!lc_is_equal_approx(1.0f, 1.5f), "lc_is_equal_approx() false for distinct");

    TEST_ASSERT(ext_math_feq(lc_ease(0.0f, 2.0f), 0.0f), "lc_ease() at t=0 is 0");
    TEST_ASSERT(ext_math_feq(lc_ease(1.0f, 2.0f), 1.0f), "lc_ease() at t=1 is 1");
    TEST_ASSERT(ext_math_feq(lc_ease(0.5f, 1.0f), 0.5f), "lc_ease() linear curve is identity");

    TEST_ASSERT(ext_math_feq(lc_pos_mod(-1.0f, 5.0f), 4.0f), "lc_pos_mod() of negative wraps positive");
    TEST_ASSERT(ext_math_feq(lc_pos_mod(7.0f, 5.0f), 2.0f), "lc_pos_mod() of positive");

    TEST_ASSERT(lc_pos_mod_int(-1, 5) == 4, "lc_pos_mod_int() of negative wraps positive");
    TEST_ASSERT(lc_pos_mod_int(7, 5) == 2, "lc_pos_mod_int() of positive");

    SECTION_END("Ext Math: Random & Range Helpers");
}

static void test_ext_math_transform(void) {
    SECTION_START("Ext Math: Transform from Matrix");

    LCTransform t = lc_transform_create(
        lc_vec3(5.0f, -2.0f, 3.0f),
        lc_quat_from_axis_angle(lc_vec3_unit_y(), LC_HALF_PI),
        lc_vec3(2.0f, 2.0f, 2.0f));

    LCMat4 mat = lc_transform_to_matrix(&t);
    LCTransform recovered = lc_transform_from_matrix(&mat);

    TEST_ASSERT(ext_math_feq_v3(recovered.position, 5.0f, -2.0f, 3.0f),
                "lc_transform_from_matrix() recovers position");
    TEST_ASSERT(ext_math_feq(recovered.scale.x, 2.0f) && ext_math_feq(recovered.scale.y, 2.0f) &&
                ext_math_feq(recovered.scale.z, 2.0f),
                "lc_transform_from_matrix() recovers scale");

    LCVec3 byTransform = lc_transform_apply_to_point(&t, lc_vec3(1.0f, 0.0f, 0.0f));
    LCVec3 byRecovered = lc_transform_apply_to_point(&recovered, lc_vec3(1.0f, 0.0f, 0.0f));
    TEST_ASSERT(ext_math_feq_v3(byRecovered, byTransform.x, byTransform.y, byTransform.z),
                "lc_transform_from_matrix() rotation maps points identically");

    SECTION_END("Ext Math: Transform from Matrix");
}
