/* Extended asset C API tests (capi/include/asset/lc_asset.h,
 * capi/include/components/lc_multimesh.h, capi/include/ui/lc_image2d.h).
 *
 * Covers public functions not exercised by test_asset_system() / test_multimesh():
 *   common asset lifecycle (lc_asset_release / get_path / get_name with NULL guards),
 *   image load-from-file / load-from-memory / mip data,
 *   image2d texture path management,
 *   font load + info/glyph/has_glyph/atlas/kerning/measure/release (graceful on missing file),
 *   model load + info/mesh/material/bone/animation accessors/release (graceful on missing file),
 *   multimesh mesh-path + material-override round-trip and instance transform/color/custom-data round-trip.
 *
 * Load functions point at nonexistent paths: the suite must not depend on real asset
 * files, so those calls are asserted to fail gracefully (no crash) rather than succeed. */

static void test_ext_assets_common(void) {
    SECTION_START("Ext Assets: Common Asset Lifecycle");

    const uint8_t pixels[16] = {
        255, 0, 0, 255,   0, 255, 0, 255,
        0, 0, 255, 255,   255, 255, 0, 255
    };

    LCImageAssetHandle image = NULL;
    LCResult result = lc_image_load_from_raw(pixels, 2, 2, 4, false, LC_IMAGE_COLOR_SPACE_SRGB, &image);
    TEST_RESULT(result, "Create 2x2 RGBA image asset for lifecycle tests");
    TEST_ASSERT(image != NULL, "Image handle is not NULL");

    if (image != NULL) {
        LCAssetHandle asset = (LCAssetHandle)image;

        char path[256];
        path[0] = '\0';
        result = lc_asset_get_path(asset, path, sizeof(path));
        TEST_RESULT(result, "lc_asset_get_path() succeeds");

        char name[256];
        name[0] = '\0';
        result = lc_asset_get_name(asset, name, sizeof(name));
        TEST_RESULT(result, "lc_asset_get_name() succeeds");

        result = lc_asset_get_path(NULL, path, sizeof(path));
        TEST_ASSERT(result != LC_SUCCESS, "lc_asset_get_path(NULL asset) fails gracefully");

        result = lc_asset_get_path(asset, NULL, sizeof(path));
        TEST_ASSERT(result != LC_SUCCESS, "lc_asset_get_path(NULL buffer) fails gracefully");

        result = lc_asset_get_name(NULL, name, sizeof(name));
        TEST_ASSERT(result != LC_SUCCESS, "lc_asset_get_name(NULL asset) fails gracefully");

        int refCount = 0;
        result = lc_asset_get_ref_count(asset, &refCount);
        TEST_RESULT(result, "Get reference count before add_ref");

        result = lc_asset_add_ref(asset);
        TEST_RESULT(result, "lc_asset_add_ref()");

        result = lc_asset_release(asset);
        TEST_RESULT(result, "lc_asset_release() drops the added reference");

        result = lc_asset_release(NULL);
        TEST_ASSERT(result != LC_SUCCESS, "lc_asset_release(NULL) fails gracefully");

        result = lc_image_release(image);
        TEST_RESULT(result, "Release base image reference");
    }

    SECTION_END("Ext Assets: Common Asset Lifecycle");
}

static void test_ext_assets_image(void) {
    SECTION_START("Ext Assets: Image Load and Mip Data");

    LCImageAssetHandle missing = NULL;
    LCResult result = lc_image_load("res://__definitely_missing_image__.png", false,
                                    LC_IMAGE_COLOR_SPACE_SRGB, &missing);
    TEST_ASSERT(result != LC_SUCCESS, "lc_image_load() on missing file fails gracefully");
    TEST_ASSERT(missing == NULL, "Missing image handle remains NULL");
    if (missing != NULL) {
        lc_image_release(missing);
    }

    const uint8_t garbage[8] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77 };
    LCImageAssetHandle fromMem = NULL;
    result = lc_image_load_from_memory(garbage, sizeof(garbage), false,
                                       LC_IMAGE_COLOR_SPACE_SRGB, &fromMem);
    TEST_ASSERT(result != LC_SUCCESS, "lc_image_load_from_memory() on garbage fails gracefully");
    if (fromMem != NULL) {
        lc_image_release(fromMem);
    }

    const uint8_t pixels[64] = {
        255, 0, 0, 255,   0, 255, 0, 255,   0, 0, 255, 255,   255, 255, 0, 255,
        255, 0, 255, 255, 0, 255, 255, 255, 128, 128, 128, 255, 64, 64, 64, 255,
        10, 20, 30, 255,  40, 50, 60, 255,  70, 80, 90, 255,  100, 110, 120, 255,
        200, 150, 100, 255, 50, 100, 150, 255, 25, 75, 125, 255, 5, 15, 25, 255
    };

    LCImageAssetHandle mipped = NULL;
    result = lc_image_load_from_raw(pixels, 4, 4, 4, true, LC_IMAGE_COLOR_SPACE_LINEAR, &mipped);
    TEST_RESULT(result, "Create 4x4 RGBA image with mipmaps");

    if (mipped != NULL) {
        LCImageInfo info;
        memset(&info, 0, sizeof(info));
        result = lc_image_get_info(mipped, &info);
        TEST_RESULT(result, "Get info on mipped image");
        TEST_ASSERT(info.width == 4 && info.height == 4, "Mipped image base dimensions are 4x4");

        const uint8_t* mipData = NULL;
        uint32_t mipWidth = 0;
        uint32_t mipHeight = 0;
        result = lc_image_get_mip_data(mipped, 0, &mipData, &mipWidth, &mipHeight);
        TEST_RESULT(result, "lc_image_get_mip_data(level 0)");
        TEST_ASSERT(mipData != NULL && mipWidth == 4 && mipHeight == 4,
                    "Mip level 0 is 4x4 with valid data pointer");

        result = lc_image_get_mip_data(mipped, 0, NULL, &mipWidth, &mipHeight);
        TEST_ASSERT(result != LC_SUCCESS, "lc_image_get_mip_data(NULL data) fails gracefully");

        result = lc_image_get_mip_data(NULL, 0, &mipData, &mipWidth, &mipHeight);
        TEST_ASSERT(result != LC_SUCCESS, "lc_image_get_mip_data(NULL asset) fails gracefully");

        result = lc_image_release(mipped);
        TEST_RESULT(result, "Release mipped image");
    }

    SECTION_END("Ext Assets: Image Load and Mip Data");
}

static void test_ext_assets_image2d(void) {
    SECTION_START("Ext Assets: Image2D Texture Path");

    LCNodeHandle node = NULL;
    LCResult result = lc_node_create(LC_NODE_2D, "ExtImage2DNode", &node);
    TEST_RESULT(result, "Create 2D node for Image2D");

    LCComponentHandle image2d = NULL;
    result = lc_image2d_create(node, &image2d);
    TEST_RESULT(result, "Create Image2D component");

    if (image2d != NULL) {
        result = lc_image2d_set_texture_path(image2d, "res://textures/example.png");
        TEST_RESULT(result, "lc_image2d_set_texture_path()");

        char path[256];
        path[0] = '\0';
        result = lc_image2d_get_texture_path(image2d, path, sizeof(path));
        TEST_RESULT(result, "lc_image2d_get_texture_path()");
        TEST_ASSERT(strcmp(path, "res://textures/example.png") == 0,
                    "Texture path round-trips");

        result = lc_image2d_load_texture(image2d, "res://__missing_texture__.png");
        TEST_ASSERT(result != LC_SUCCESS, "lc_image2d_load_texture() on missing file fails gracefully");

        result = lc_image2d_get_texture_path(NULL, path, sizeof(path));
        TEST_ASSERT(result != LC_SUCCESS, "lc_image2d_get_texture_path(NULL) fails gracefully");
    }

    if (node != NULL) {
        lc_node_destroy(node);
    }

    SECTION_END("Ext Assets: Image2D Texture Path");
}

static void test_ext_assets_font(void) {
    SECTION_START("Ext Assets: Font Load and Measure");

    LCFontAssetHandle font = NULL;
    LCResult result = lc_font_load("res://__missing_font__.ttf", 32.0f, 512, 512, &font);
    TEST_ASSERT(result != LC_SUCCESS, "lc_font_load() on missing file fails gracefully");
    TEST_ASSERT(font == NULL, "Missing font handle remains NULL");

    LCFontInfo info;
    memset(&info, 0, sizeof(info));
    result = lc_font_get_info(font, &info);
    TEST_ASSERT(result != LC_SUCCESS, "lc_font_get_info(NULL font) fails gracefully");

    LCGlyph glyph;
    memset(&glyph, 0, sizeof(glyph));
    result = lc_font_get_glyph(font, (uint32_t)'A', &glyph);
    TEST_ASSERT(result != LC_SUCCESS, "lc_font_get_glyph(NULL font) fails gracefully");

    bool hasGlyph = true;
    result = lc_font_has_glyph(font, (uint32_t)'A', &hasGlyph);
    TEST_ASSERT(result != LC_SUCCESS, "lc_font_has_glyph(NULL font) fails gracefully");

    const uint8_t* atlasData = NULL;
    size_t atlasSize = 0;
    result = lc_font_get_atlas_data(font, &atlasData, &atlasSize);
    TEST_ASSERT(result != LC_SUCCESS, "lc_font_get_atlas_data(NULL font) fails gracefully");

    float kerning = -1.0f;
    result = lc_font_get_kerning(font, (uint32_t)'A', (uint32_t)'V', &kerning);
    TEST_ASSERT(result != LC_SUCCESS, "lc_font_get_kerning(NULL font) fails gracefully");

    LCVec2 textSize = { -1.0f, -1.0f };
    result = lc_font_measure_text(font, "Hello", 32.0f, &textSize);
    TEST_ASSERT(result != LC_SUCCESS, "lc_font_measure_text(NULL font) fails gracefully");

    result = lc_font_release(font);
    TEST_ASSERT(result != LC_SUCCESS, "lc_font_release(NULL font) fails gracefully");

    SECTION_END("Ext Assets: Font Load and Measure");
}

static void test_ext_assets_model(void) {
    SECTION_START("Ext Assets: Model Load and Accessors");

    LCModelAssetHandle model = NULL;
    LCResult result = lc_model_load("res://__missing_model__.glb", false, &model);
    TEST_ASSERT(result != LC_SUCCESS, "lc_model_load() on missing file fails gracefully");
    TEST_ASSERT(model == NULL, "Missing model handle remains NULL");

    LCModelInfo modelInfo;
    memset(&modelInfo, 0, sizeof(modelInfo));
    result = lc_model_get_info(model, &modelInfo);
    TEST_ASSERT(result != LC_SUCCESS, "lc_model_get_info(NULL model) fails gracefully");

    LCMeshInfo meshInfo;
    memset(&meshInfo, 0, sizeof(meshInfo));
    result = lc_model_get_mesh_info(model, 0, &meshInfo);
    TEST_ASSERT(result != LC_SUCCESS, "lc_model_get_mesh_info(NULL model) fails gracefully");

    LCVertex vertices[4];
    memset(vertices, 0, sizeof(vertices));
    uint32_t verticesCopied = 0;
    result = lc_model_get_mesh_vertices(model, 0, vertices, 4, &verticesCopied);
    TEST_ASSERT(result != LC_SUCCESS, "lc_model_get_mesh_vertices(NULL model) fails gracefully");

    uint32_t indices[6];
    memset(indices, 0, sizeof(indices));
    uint32_t indicesCopied = 0;
    result = lc_model_get_mesh_indices(model, 0, indices, 6, &indicesCopied);
    TEST_ASSERT(result != LC_SUCCESS, "lc_model_get_mesh_indices(NULL model) fails gracefully");

    LCMaterialInfo materialInfo;
    memset(&materialInfo, 0, sizeof(materialInfo));
    result = lc_model_get_material_info(model, 0, &materialInfo);
    TEST_ASSERT(result != LC_SUCCESS, "lc_model_get_material_info(NULL model) fails gracefully");

    LCBone bone;
    memset(&bone, 0, sizeof(bone));
    result = lc_model_get_bone(model, 0, &bone);
    TEST_ASSERT(result != LC_SUCCESS, "lc_model_get_bone(NULL model) fails gracefully");

    LCAnimationInfo animInfo;
    memset(&animInfo, 0, sizeof(animInfo));
    result = lc_model_get_animation_info(model, 0, &animInfo);
    TEST_ASSERT(result != LC_SUCCESS, "lc_model_get_animation_info(NULL model) fails gracefully");

    result = lc_model_release(model);
    TEST_ASSERT(result != LC_SUCCESS, "lc_model_release(NULL model) fails gracefully");

    SECTION_END("Ext Assets: Model Load and Accessors");
}

static void test_ext_assets_multimesh(void) {
    SECTION_START("Ext Assets: MultiMesh Mesh/Material Paths");

    LCNodeHandle node = NULL;
    LCResult result = lc_node_create(LC_NODE_3D, "ExtMultiMeshNode", &node);
    TEST_RESULT(result, "Create 3D node for MultiMesh");

    LCComponentHandle mm = NULL;
    result = lc_multimesh_create(node, &mm);
    TEST_RESULT(result, "Create MultiMeshGeneric component");

    if (mm != NULL) {
        result = lc_multimesh_load_mesh(mm, "res://__missing_mesh__.glb");
        TEST_ASSERT(result != LC_SUCCESS, "lc_multimesh_load_mesh() on missing file fails gracefully");

        lc_multimesh_set_mesh_path(mm, "res://meshes/example.glb");

        char meshPath[256];
        meshPath[0] = '\0';
        result = lc_multimesh_get_mesh_path(mm, meshPath, sizeof(meshPath));
        TEST_RESULT(result, "lc_multimesh_get_mesh_path()");

        result = lc_multimesh_set_material_override(mm, "res://materials/example.material");
        TEST_RESULT(result, "lc_multimesh_set_material_override()");

        char matPath[256];
        matPath[0] = '\0';
        result = lc_multimesh_get_material_override(mm, matPath, sizeof(matPath));
        TEST_RESULT(result, "lc_multimesh_get_material_override()");
        TEST_ASSERT(strcmp(matPath, "res://materials/example.material") == 0,
                    "Material override path round-trips");

        result = lc_multimesh_set_material_override(mm, "");
        TEST_RESULT(result, "Clear material override with empty string");
    }

    SECTION_END("Ext Assets: MultiMesh Mesh/Material Paths");

    SECTION_START("Ext Assets: MultiMesh Instance Round-Trip");

    if (mm != NULL) {
        result = lc_multimesh_set_instance_count(mm, 3);
        TEST_RESULT(result, "Set instance count to 3");

        LCMat4 transform = lc_mat4_translate(lc_vec3(5.0f, -2.5f, 7.25f));
        result = lc_multimesh_set_instance_transform(mm, 1, transform);
        TEST_RESULT(result, "Set instance 1 transform (translation)");

        LCMat4 outTransform;
        memset(&outTransform, 0, sizeof(outTransform));
        result = lc_multimesh_get_instance_transform(mm, 1, &outTransform);
        TEST_RESULT(result, "Get instance 1 transform");
        TEST_ASSERT(outTransform.m[12] > 4.99f && outTransform.m[12] < 5.01f,
                    "Instance transform translation X round-trips");
        TEST_ASSERT(outTransform.m[13] > -2.51f && outTransform.m[13] < -2.49f,
                    "Instance transform translation Y round-trips");
        TEST_ASSERT(outTransform.m[14] > 7.24f && outTransform.m[14] < 7.26f,
                    "Instance transform translation Z round-trips");

        LCColor color = lc_color(0.25f, 0.5f, 0.75f, 1.0f);
        result = lc_multimesh_set_instance_color(mm, 2, color);
        TEST_RESULT(result, "Set instance 2 color");

        LCColor outColor = { 0.0f, 0.0f, 0.0f, 0.0f };
        result = lc_multimesh_get_instance_color(mm, 2, &outColor);
        TEST_RESULT(result, "Get instance 2 color");
        TEST_ASSERT(outColor.r > 0.24f && outColor.r < 0.26f, "Instance color R round-trips");
        TEST_ASSERT(outColor.g > 0.49f && outColor.g < 0.51f, "Instance color G round-trips");
        TEST_ASSERT(outColor.b > 0.74f && outColor.b < 0.76f, "Instance color B round-trips");
        TEST_ASSERT(outColor.a > 0.99f && outColor.a < 1.01f, "Instance color A round-trips");

        LCVec4 customData = lc_vec4(1.5f, 2.5f, 3.5f, 4.5f);
        result = lc_multimesh_set_instance_custom_data(mm, 0, customData);
        TEST_RESULT(result, "Set instance 0 custom data");

        LCVec4 outData = { 0.0f, 0.0f, 0.0f, 0.0f };
        result = lc_multimesh_get_instance_custom_data(mm, 0, &outData);
        TEST_RESULT(result, "Get instance 0 custom data");
        TEST_ASSERT(outData.x > 1.49f && outData.x < 1.51f, "Custom data X round-trips");
        TEST_ASSERT(outData.y > 2.49f && outData.y < 2.51f, "Custom data Y round-trips");
        TEST_ASSERT(outData.z > 3.49f && outData.z < 3.51f, "Custom data Z round-trips");
        TEST_ASSERT(outData.w > 4.49f && outData.w < 4.51f, "Custom data W round-trips");

        LCVec3 baseRot = { 0.0f, 90.0f, 0.0f };
        result = lc_multimesh_set_base_rotation(mm, baseRot);
        TEST_RESULT(result, "lc_multimesh_set_base_rotation()");
        LCVec3 outRot = { 0.0f, 0.0f, 0.0f };
        result = lc_multimesh_get_base_rotation(mm, &outRot);
        TEST_RESULT(result, "lc_multimesh_get_base_rotation()");
        TEST_ASSERT(outRot.y > 89.9f && outRot.y < 90.1f, "Base rotation Y round-trips");

        LCVec3 baseScale = { 2.0f, 2.0f, 2.0f };
        result = lc_multimesh_set_base_scale(mm, baseScale);
        TEST_RESULT(result, "lc_multimesh_set_base_scale()");
        LCVec3 outScale = { 1.0f, 1.0f, 1.0f };
        result = lc_multimesh_get_base_scale(mm, &outScale);
        TEST_RESULT(result, "lc_multimesh_get_base_scale()");
        TEST_ASSERT(outScale.x > 1.99f && outScale.x < 2.01f, "Base scale X round-trips");

        result = lc_multimesh_set_auto_generate_lods(mm, true);
        TEST_RESULT(result, "lc_multimesh_set_auto_generate_lods()");
        bool autoLods = false;
        result = lc_multimesh_get_auto_generate_lods(mm, &autoLods);
        TEST_RESULT(result, "lc_multimesh_get_auto_generate_lods()");
        TEST_ASSERT(autoLods == true, "Auto-generate LODs round-trips");

        result = lc_multimesh_set_auto_lod_reduction(mm, 0.4f);
        TEST_RESULT(result, "lc_multimesh_set_auto_lod_reduction()");
        float reduction = 0.0f;
        result = lc_multimesh_get_auto_lod_reduction(mm, &reduction);
        TEST_RESULT(result, "lc_multimesh_get_auto_lod_reduction()");
        TEST_ASSERT(reduction > 0.39f && reduction < 0.41f, "Auto LOD reduction round-trips");
    }

    if (node != NULL) {
        lc_node_destroy(node);
    }

    SECTION_END("Ext Assets: MultiMesh Instance Round-Trip");
}
