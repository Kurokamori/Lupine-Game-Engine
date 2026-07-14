/* Color string/HSV helpers + gradient/curve sampling C API tests.
 *
 * These mirror the scripting color & data-sampling globals. They are pure math
 * (no engine state), so the checks verify exact conversions and round-trips. */

static void test_ext_color_hex_string(void) {
    SECTION_START("Color: Hex String");

    LCColor red = lc_color_from_hex_string("#ff0000");
    TEST_ASSERT(red.r >= 0.99f && red.g <= 0.01f && red.b <= 0.01f && red.a >= 0.99f,
                "lc_color_from_hex_string(\"#ff0000\") parses opaque red");

    LCColor noHash = lc_color_from_hex_string("00ff00");
    TEST_ASSERT(noHash.g >= 0.99f && noHash.r <= 0.01f && noHash.b <= 0.01f,
                "lc_color_from_hex_string() accepts a missing leading '#'");

    LCColor shortForm = lc_color_from_hex_string("#f00");
    TEST_ASSERT(shortForm.r >= 0.99f && shortForm.g <= 0.01f && shortForm.b <= 0.01f,
                "lc_color_from_hex_string(\"#f00\") expands 3-digit form");

    LCColor withAlpha = lc_color_from_hex_string("#0000ff80");
    TEST_ASSERT(withAlpha.b >= 0.99f && withAlpha.a > 0.45f && withAlpha.a < 0.55f,
                "lc_color_from_hex_string() parses an 8-digit RGBA form");

    char buffer[16];
    LCResult result = lc_color_to_hex_string(lc_color(1.0f, 0.0f, 0.0f, 1.0f), buffer, sizeof(buffer));
    TEST_RESULT(result, "lc_color_to_hex_string() returns success");
    TEST_ASSERT(strcmp(buffer, "#ff0000ff") == 0, "lc_color_to_hex_string() formats opaque red as #ff0000ff");

    TEST_ASSERT(lc_color_to_hex_string(red, NULL, 16) == LC_ERROR_NULL_POINTER,
                "lc_color_to_hex_string(NULL) rejects null buffer");
    TEST_ASSERT(lc_color_to_hex_string(red, buffer, 4) == LC_ERROR_INVALID_PARAMETER,
                "lc_color_to_hex_string() rejects an undersized buffer");

    SECTION_END("Color: Hex String");
}

static void test_ext_color_hsv01(void) {
    SECTION_START("Color: Normalized HSV");

    LCColor red = lc_color_from_hsv01(0.0f, 1.0f, 1.0f, 1.0f);
    TEST_ASSERT(red.r >= 0.99f && red.g <= 0.01f && red.b <= 0.01f,
                "lc_color_from_hsv01(0,1,1) yields red");

    LCColor white = lc_color_from_hsv01(0.0f, 0.0f, 1.0f, 1.0f);
    TEST_ASSERT(white.r >= 0.99f && white.g >= 0.99f && white.b >= 0.99f,
                "lc_color_from_hsv01() with zero saturation yields white");

    LCColor halfAlpha = lc_color_from_hsv01(0.5f, 1.0f, 1.0f, 0.5f);
    TEST_ASSERT(halfAlpha.a > 0.49f && halfAlpha.a < 0.51f,
                "lc_color_from_hsv01() preserves the alpha argument");

    SECTION_END("Color: Normalized HSV");
}

static void test_ext_color_data_sampling(void) {
    SECTION_START("Color: Gradient / Curve Sampling");

    const char* gradient = "{\"stops\":[{\"t\":0.0,\"color\":[0.0,0.0,0.0,1.0]},{\"t\":1.0,\"color\":[1.0,1.0,1.0,1.0]}]}";
    LCColor mid = lc_sample_gradient(gradient, 0.5f);
    TEST_ASSERT(mid.r >= 0.0f && mid.r <= 1.0f, "lc_sample_gradient() returns an in-range color");

    LCColor empty = lc_sample_gradient(NULL, 0.5f);
    TEST_ASSERT(empty.r >= 0.99f && empty.g >= 0.99f && empty.b >= 0.99f,
                "lc_sample_gradient(NULL) returns white");

    const char* curve = "{\"points\":[{\"t\":0.0,\"v\":0.0},{\"t\":1.0,\"v\":1.0}]}";
    float sampled = lc_sample_curve(curve, 0.5f, 1.0f);
    TEST_ASSERT(sampled >= 0.0f && sampled <= 1.0f, "lc_sample_curve() returns an in-range value");

    float fallback = lc_sample_curve(NULL, 0.5f, 0.25f);
    TEST_ASSERT(fallback > 0.24f && fallback < 0.26f, "lc_sample_curve(NULL) returns the empty default");

    SECTION_END("Color: Gradient / Curve Sampling");
}
