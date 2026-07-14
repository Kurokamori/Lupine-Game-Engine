/**
 * @file RenderingTests.cpp
 * @brief Headless console tests for the rendering math/data layer.
 *
 * The full render pipeline needs a live GPU device (there is no null backend), so
 * these tests target the parts of the rendering system that are pure CPU logic and
 * exercisable without a window or graphics device:
 *   - RenderCamera view/projection matrix math (Camera2D / Camera3D / CameraCanvas)
 *   - Material and MaterialPropertyBlock property storage / typed access
 *
 * Matrix assertions are written as convention-independent invariants (the camera's
 * own position maps to the view-space origin; the look target projects to the centre
 * of the clip volume; zoom scales clip coordinates) so they hold across the GL/DX
 * backends rather than baking in one depth-range or handedness convention.
 */

#include "lupine/engine/Engine.hpp"
#include "lupine/rendering/RenderCamera.hpp"
#include "lupine/rendering/Material.hpp"
#include "lupine/rendering/TextLayout.hpp"
#include "lupine/rendering/Font.hpp"
#include "TestFramework.hpp"
#include <iostream>
#include <cmath>
#include <vector>

using namespace lupine;

namespace {

bool FloatEq(float a, float b, float tol = 1e-3f) {
    return std::fabs(a - b) < tol;
}

// Project a world point through a combined view-projection matrix and return the
// normalized device coordinate (after perspective divide).
math::Vec3 ToNDC(const Mat4& viewProj, const math::Vec3& world) {
    math::Vec4 clip = viewProj * math::Vec4(world, 1.0f);
    if (FloatEq(clip.w, 0.0f)) {
        return math::Vec3(clip.x, clip.y, clip.z);
    }
    return math::Vec3(clip.x / clip.w, clip.y / clip.w, clip.z / clip.w);
}

bool TestCamera3D() {
    TEST_SECTION("Rendering: Camera3D Matrices");

    Camera3D cam;
    cam.position = math::Vec3(0.0f, 0.0f, 5.0f);
    cam.target = math::Vec3(0.0f, 0.0f, 0.0f);
    cam.up = math::Vec3(0.0f, 1.0f, 0.0f);
    cam.projectionType = ProjectionType::Perspective;

    TEST_ASSERT(cam.getType() == CameraType::Camera3D, "Camera3D reports Camera3D type");

    Mat4 view = cam.getViewMatrix();
    math::Vec3 camInView = view.TransformPoint(cam.position);
    TEST_ASSERT(FloatEq(camInView.x, 0.0f) && FloatEq(camInView.y, 0.0f) && FloatEq(camInView.z, 0.0f),
                "View matrix maps the camera position to the view-space origin");

    math::Vec3 targetInView = view.TransformPoint(cam.target);
    TEST_ASSERT(targetInView.z < 0.0f, "Look target lies down the camera forward (-Z) axis in view space");
    TEST_ASSERT(FloatEq(targetInView.x, 0.0f) && FloatEq(targetInView.y, 0.0f),
                "Look target is centered on the view axis");

    Mat4 proj = cam.getProjectionMatrix(16.0f / 9.0f);
    TEST_ASSERT(!FloatEq(proj.Determinant(), 0.0f), "Perspective projection is invertible");

    Mat4 viewProj = proj * view;
    math::Vec3 centerNdc = ToNDC(viewProj, cam.target);
    TEST_ASSERT(FloatEq(centerNdc.x, 0.0f) && FloatEq(centerNdc.y, 0.0f),
                "The look target projects to the centre of the clip volume");

    cam.projectionType = ProjectionType::Orthographic;
    Mat4 ortho = cam.getProjectionMatrix(16.0f / 9.0f);
    TEST_ASSERT(ortho != proj, "Orthographic projection differs from perspective projection");
    TEST_ASSERT(!FloatEq(ortho.Determinant(), 0.0f), "Orthographic projection is invertible");

    return true;
}

bool TestCamera2D() {
    TEST_SECTION("Rendering: Camera2D Matrices");

    Camera2D cam;
    cam.position = math::Vec2(3.0f, 4.0f);
    cam.zoom = 1.0f;
    cam.orthoSize = 10.0f;

    TEST_ASSERT(cam.getType() == CameraType::Camera2D, "Camera2D reports Camera2D type");

    Mat4 view = cam.getViewMatrix();
    math::Vec3 centerInView = view.TransformPoint(math::Vec3(cam.position.x, cam.position.y, 0.0f));
    TEST_ASSERT(FloatEq(centerInView.x, 0.0f) && FloatEq(centerInView.y, 0.0f),
                "View matrix recentres the camera position to the origin");

    Mat4 proj = cam.getProjectionMatrix(1.0f);
    TEST_ASSERT(!FloatEq(proj.Determinant(), 0.0f), "2D projection is invertible");

    Mat4 viewProj = proj * view;
    math::Vec3 centerNdc = ToNDC(viewProj, math::Vec3(cam.position.x, cam.position.y, 0.0f));
    TEST_ASSERT(FloatEq(centerNdc.x, 0.0f) && FloatEq(centerNdc.y, 0.0f),
                "The camera centre projects to NDC origin");

    // A fixed world offset projects further from centre as zoom increases.
    math::Vec3 offsetWorld(cam.position.x + 2.0f, cam.position.y, 0.0f);
    float ndcAtZoom1 = std::fabs(ToNDC(proj * view, offsetWorld).x);

    cam.zoom = 2.0f;
    Mat4 viewZoomed = cam.getViewMatrix();
    Mat4 projZoomed = cam.getProjectionMatrix(1.0f);
    float ndcAtZoom2 = std::fabs(ToNDC(projZoomed * viewZoomed, offsetWorld).x);
    TEST_ASSERT(ndcAtZoom2 > ndcAtZoom1,
                "Increasing zoom magnifies a fixed world offset in clip space");

    // A positive rotation turns a fixed world offset the same (positive) direction in
    // clip space, i.e. +X rotates toward +Y rather than -Y (the previously-backwards case).
    cam.zoom = 1.0f;
    cam.rotation = math::PI * 0.5f;
    math::Vec3 rotatedNdc = ToNDC(cam.getProjectionMatrix(1.0f) * cam.getViewMatrix(), offsetWorld);
    TEST_ASSERT(rotatedNdc.y > 0.0f && FloatEq(rotatedNdc.x, 0.0f, 1e-2f),
                "Positive rotation turns +X world offset toward +Y");

    return true;
}

bool TestCameraCanvas() {
    TEST_SECTION("Rendering: CameraCanvas Matrices");

    CameraCanvas cam;
    cam.canvasSize = math::Vec2(1920.0f, 1080.0f);

    TEST_ASSERT(cam.getType() == CameraType::CameraCanvas, "CameraCanvas reports CameraCanvas type");

    Mat4 view = cam.getViewMatrix();
    Mat4 proj = cam.getProjectionMatrix(cam.canvasSize.x / cam.canvasSize.y);
    TEST_ASSERT(!FloatEq(proj.Determinant(), 0.0f), "Canvas projection is invertible");

    Mat4 viewProj = proj * view;
    math::Vec3 centerNdc = ToNDC(viewProj, math::Vec3(cam.canvasSize.x * 0.5f, cam.canvasSize.y * 0.5f, 0.0f));
    TEST_ASSERT(FloatEq(centerNdc.x, 0.0f) && FloatEq(centerNdc.y, 0.0f, 1e-2f),
                "The canvas centre maps to NDC origin");

    math::Vec3 cornerNdc = ToNDC(viewProj, math::Vec3(0.0f, 0.0f, 0.0f));
    TEST_ASSERT(FloatEq(std::fabs(cornerNdc.x), 1.0f, 1e-2f) && FloatEq(std::fabs(cornerNdc.y), 1.0f, 1e-2f),
                "A canvas corner maps to the edge of the clip box");

    // Defaults (zero position/rotation, unit zoom) keep an identity view so internal
    // full-screen blits are unaffected.
    math::Vec3 identityPt = cam.getViewMatrix().TransformPoint(math::Vec3(7.0f, -3.0f, 0.0f));
    TEST_ASSERT(FloatEq(identityPt.x, 7.0f) && FloatEq(identityPt.y, -3.0f),
                "Default canvas view is identity");

    // A positive rotation turns the canvas the expected (positive) direction: +90 deg
    // sends +X to +Y, not -Y (the previously-backwards case).
    cam.rotation = math::PI * 0.5f;
    math::Vec3 rotated = cam.getViewMatrix().TransformPoint(math::Vec3(1.0f, 0.0f, 0.0f));
    TEST_ASSERT(FloatEq(rotated.x, 0.0f, 1e-2f) && FloatEq(rotated.y, 1.0f, 1e-2f),
                "Positive canvas rotation sends +X to +Y");

    // Zoom magnifies the canvas about its origin.
    cam.rotation = 0.0f;
    cam.zoom = 2.0f;
    math::Vec3 scaled = cam.getViewMatrix().TransformPoint(math::Vec3(5.0f, 0.0f, 0.0f));
    TEST_ASSERT(FloatEq(scaled.x, 10.0f),
                "Canvas zoom magnifies positions about the origin");

    return true;
}

bool TestMaterialPropertyBlock() {
    TEST_SECTION("Rendering: MaterialPropertyBlock");

    MaterialPropertyBlock block;
    TEST_ASSERT(block.isEmpty(), "A new property block is empty");

    block.setFloat("u_Metallic", 0.75f);
    block.setInt("u_Mode", 3);
    block.setBool("u_Flag", true);
    block.setVec2("u_Tiling", math::Vec2(2.0f, 4.0f));
    block.setVec3("u_Offset", math::Vec3(1.0f, 2.0f, 3.0f));
    block.setVec4("u_Params", math::Vec4(0.1f, 0.2f, 0.3f, 0.4f));
    block.setColor("u_Color", math::Color(1.0f, 0.5f, 0.25f, 1.0f));

    TEST_ASSERT(!block.isEmpty(), "Property block is non-empty after sets");
    TEST_ASSERT(block.hasProperty("u_Metallic"), "hasProperty finds a set float");
    TEST_ASSERT(!block.hasProperty("u_Missing"), "hasProperty is false for an unset name");

    const MaterialPropertyValue* metallic = block.getProperty("u_Metallic");
    TEST_ASSERT(metallic != nullptr && std::holds_alternative<float>(*metallic) &&
                FloatEq(std::get<float>(*metallic), 0.75f), "Float property round-trips");

    const MaterialPropertyValue* mode = block.getProperty("u_Mode");
    TEST_ASSERT(mode != nullptr && std::holds_alternative<int>(*mode) && std::get<int>(*mode) == 3,
                "Int property round-trips");

    const MaterialPropertyValue* flag = block.getProperty("u_Flag");
    TEST_ASSERT(flag != nullptr && std::holds_alternative<bool>(*flag) && std::get<bool>(*flag) == true,
                "Bool property round-trips");

    const MaterialPropertyValue* offset = block.getProperty("u_Offset");
    TEST_ASSERT(offset != nullptr && std::holds_alternative<math::Vec3>(*offset) &&
                FloatEq(std::get<math::Vec3>(*offset).y, 2.0f), "Vec3 property round-trips");

    const MaterialPropertyValue* color = block.getProperty("u_Color");
    TEST_ASSERT(color != nullptr && std::holds_alternative<math::Color>(*color) &&
                FloatEq(std::get<math::Color>(*color).g, 0.5f), "Color property round-trips");

    TEST_ASSERT(block.getProperties().size() == 7, "Property block holds all set properties");

    block.clear();
    TEST_ASSERT(block.isEmpty(), "Property block is empty after clear");

    return true;
}

bool TestMaterial() {
    TEST_SECTION("Rendering: Material");

    Material mat;
    mat.name = "TestMaterial";
    mat.renderLayer = RenderLayer::Transparent;
    mat.isTransparent = true;
    mat.alphaClipThreshold = 0.25f;
    mat.usesLighting = true;
    mat.castsShadows = false;
    mat.properties["u_Color"] = math::Color(0.0f, 1.0f, 0.0f, 1.0f);
    mat.properties["u_Strength"] = 2.0f;

    TEST_ASSERT(mat.name == "TestMaterial", "Material name round-trips");
    TEST_ASSERT(mat.renderLayer == RenderLayer::Transparent, "Material render layer round-trips");
    TEST_ASSERT(mat.isTransparent, "Material transparency flag round-trips");
    TEST_ASSERT(FloatEq(mat.alphaClipThreshold, 0.25f), "Material alpha clip threshold round-trips");
    TEST_ASSERT(mat.usesLighting, "Material lighting flag round-trips");
    TEST_ASSERT(!mat.castsShadows, "Material shadow casting flag round-trips");
    TEST_ASSERT(mat.properties.size() == 2, "Material holds its property overrides");

    const MaterialPropertyValue& strength = mat.properties.at("u_Strength");
    TEST_ASSERT(std::holds_alternative<float>(strength) && FloatEq(std::get<float>(strength), 2.0f),
                "Material float property round-trips");

    return true;
}

// A uniform-metric atlas: every glyph is identical, so all glyphs on a line share
// one quad Y and each line collapses to a single scalar row that can be compared.
// The metrics mirror what FontBaker produces from a real font: whole-font
// ascent/descent, and a NEGATIVE glyph bearing.y (stb's `yoff`, measured downward
// from the baseline to the glyph top).
FontAtlas MakeUniformAtlas() {
    FontAtlas atlas;
    atlas.atlasWidth = 64;
    atlas.atlasHeight = 64;
    atlas.fontSize = 16.0f;
    atlas.lineHeight = 20.0f;
    atlas.ascent = 13.0f;
    atlas.descent = 4.0f;

    const char* kChars = "ABCD ";
    for (const char* c = kChars; *c != '\0'; ++c) {
        Glyph glyph;
        glyph.codepoint = static_cast<uint32_t>(*c);
        glyph.uvMin = Vec2(0.0f, 0.0f);
        glyph.uvMax = Vec2(1.0f, 1.0f);
        glyph.size = Vec2(8.0f, 10.0f);
        glyph.bearing = Vec2(0.0f, -10.0f);
        glyph.advance = 10.0f;
        atlas.glyphs[glyph.codepoint] = glyph;
    }
    return atlas;
}

// The top edge of each distinct row (the largest Y of a quad, since local space is
// Y-up), in the order the rows are emitted.
std::vector<float> RowTopsInEmissionOrder(const TextLayoutResult& layout) {
    std::vector<float> rows;
    for (const TextGlyphQuad& quad : layout.quads) {
        float top = quad.pos[0].y;
        for (int k = 1; k < 4; ++k) {
            top = std::max(top, quad.pos[k].y);
        }
        bool seen = false;
        for (float existing : rows) {
            if (FloatEq(existing, top)) {
                seen = true;
                break;
            }
        }
        if (!seen) {
            rows.push_back(top);
        }
    }
    return rows;
}

bool TestTextLayoutWordWrap() {
    TEST_SECTION("Rendering: Word-wrapped text stacks downward");

    const FontAtlas atlas = MakeUniformAtlas();

    TextLayoutParams params;
    params.fontSize = 16.0f;
    params.wordWrap = true;
    params.multiline = true;
    params.lineSpacing = 1.0f;
    params.hAlign = TextHAlign::Left;
    params.vAlign = TextVAlign::Top;
    params.boxWidth = 35.0f;

    TextLayoutResult layout = TextLayout::Layout("AA BB CC", atlas, params);
    TEST_ASSERT(layout.lineCount == 3, "'AA BB CC' wraps into three lines at boxWidth 35");

    std::vector<float> rows = RowTopsInEmissionOrder(layout);
    TEST_ASSERT(rows.size() == 3, "Three wrapped lines occupy three distinct rows");

    TEST_ASSERT(rows[0] > rows[1] && rows[1] > rows[2],
                "Lines read top-to-bottom: each row sits below the previous on the Y-up canvas");

    TEST_ASSERT(FloatEq(rows[0] - rows[1], layout.lineAdvance) &&
                FloatEq(rows[1] - rows[2], layout.lineAdvance),
                "Consecutive rows are exactly one line advance apart");

    TextLayoutResult longer = TextLayout::Layout("AA BB CC DD", atlas, params);
    TEST_ASSERT(longer.lineCount == 4, "'AA BB CC DD' wraps into four lines");

    std::vector<float> longerRows = RowTopsInEmissionOrder(longer);
    TEST_ASSERT(longerRows.size() == 4, "Four wrapped lines occupy four distinct rows");

    TEST_ASSERT(FloatEq(longerRows[0], rows[0]),
                "The first line holds its baseline as more lines wrap (the block does not drift)");

    return true;
}

// The leftmost X and the topmost Y actually occupied by the emitted glyph quads.
Vec2 BlockTopLeft(const TextLayoutResult& layout) {
    float minX = 0.0f;
    float maxY = 0.0f;
    bool first = true;
    for (const TextGlyphQuad& quad : layout.quads) {
        for (int k = 0; k < 4; ++k) {
            if (first) {
                minX = quad.pos[k].x;
                maxY = quad.pos[k].y;
                first = false;
            } else {
                minX = std::min(minX, quad.pos[k].x);
                maxY = std::max(maxY, quad.pos[k].y);
            }
        }
    }
    return Vec2(minX, maxY);
}

bool TestTextLayoutBoxOrigin() {
    TEST_SECTION("Rendering: Text lays out below the box top, inside the box");

    const FontAtlas atlas = MakeUniformAtlas();
    const float kScale = 1.0f;  // params.fontSize == atlas.fontSize

    TextLayoutParams params;
    params.fontSize = 16.0f;
    params.multiline = false;
    params.hAlign = TextHAlign::Left;
    params.vAlign = TextVAlign::Top;

    TextLayoutResult layout = TextLayout::Layout("AB", atlas, params);

    TEST_ASSERT(FloatEq(layout.ascent, atlas.ascent * kScale),
                "Ascent comes from the font's vertical metrics, not from the glyphs in the string");
    TEST_ASSERT(FloatEq(layout.descent, atlas.descent * kScale),
                "Descent comes from the font's vertical metrics");

    TEST_ASSERT(FloatEq(layout.contentTopY, 0.0f),
                "Top-aligned text starts at the box top (local y = 0)");
    TEST_ASSERT(FloatEq(layout.firstBaselineY, -layout.ascent),
                "The first baseline sits one ascent BELOW the box top (negative Y)");

    // Glyph tops must land below the box top and above the baseline: the whole
    // block hangs down from the origin rather than floating above it.
    const Vec2 topLeft = BlockTopLeft(layout);
    TEST_ASSERT(topLeft.y <= 0.0f,
                "No glyph is emitted above the box top");
    TEST_ASSERT(topLeft.y > layout.firstBaselineY,
                "Glyph tops sit above the first baseline");
    TEST_ASSERT(FloatEq(topLeft.x, 0.0f),
                "Left-aligned text starts at the box left edge");

    // A string whose glyphs are all identical still has the same baseline as one
    // measured from font metrics: the placement must not depend on the characters.
    TextLayoutResult other = TextLayout::Layout("CD", atlas, params);
    TEST_ASSERT(FloatEq(other.firstBaselineY, layout.firstBaselineY),
                "Baseline does not shift with the string's contents");

    return true;
}

bool TestTextLayoutAlignment() {
    TEST_SECTION("Rendering: Horizontal and vertical alignment honor the box");

    const FontAtlas atlas = MakeUniformAtlas();

    TextLayoutParams params;
    params.fontSize = 16.0f;
    params.multiline = false;
    params.boxWidth = 100.0f;
    params.boxHeight = 60.0f;
    params.vAlign = TextVAlign::Top;

    // "AB" is two 10px advances wide, so it leaves 80px of horizontal slack.
    params.hAlign = TextHAlign::Left;
    const Vec2 left = BlockTopLeft(TextLayout::Layout("AB", atlas, params));

    params.hAlign = TextHAlign::Center;
    const Vec2 center = BlockTopLeft(TextLayout::Layout("AB", atlas, params));

    params.hAlign = TextHAlign::Right;
    const Vec2 right = BlockTopLeft(TextLayout::Layout("AB", atlas, params));

    TEST_ASSERT(FloatEq(left.x, 0.0f), "Left alignment pins the text to the box's left edge");
    TEST_ASSERT(FloatEq(center.x, 40.0f), "Center alignment splits the slack evenly");
    TEST_ASSERT(FloatEq(right.x, 80.0f), "Right alignment pushes the text to the box's right edge");

    // Vertical: the box is 60 tall and one line is 20 tall, so 40px of slack.
    params.hAlign = TextHAlign::Left;
    params.vAlign = TextVAlign::Top;
    const TextLayoutResult top = TextLayout::Layout("AB", atlas, params);

    params.vAlign = TextVAlign::Center;
    const TextLayoutResult middle = TextLayout::Layout("AB", atlas, params);

    params.vAlign = TextVAlign::Bottom;
    const TextLayoutResult bottom = TextLayout::Layout("AB", atlas, params);

    TEST_ASSERT(FloatEq(top.contentTopY, 0.0f), "Top alignment pins the block to the box top");
    TEST_ASSERT(FloatEq(middle.contentTopY, -20.0f), "Center alignment splits the vertical slack evenly");
    TEST_ASSERT(FloatEq(bottom.contentTopY, -40.0f), "Bottom alignment pushes the block to the box bottom");

    TEST_ASSERT(top.contentTopY > middle.contentTopY && middle.contentTopY > bottom.contentTopY,
                "Top/Center/Bottom move the text progressively DOWN the box, not up");

    // The bottom-aligned block's last line must still end inside the box.
    TEST_ASSERT(FloatEq(bottom.GetLineBottomY(bottom.lineCount - 1), -params.boxHeight),
                "Bottom-aligned text ends exactly on the box's bottom edge");

    return true;
}

}  // namespace

void RunRenderingTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "RENDERING TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    lupine_test::SetCurrentSuite("Rendering");

    engine::InitializeEngine();

    bool allPassed = true;
    allPassed &= TestCamera3D();
    allPassed &= TestCamera2D();
    allPassed &= TestCameraCanvas();
    allPassed &= TestMaterialPropertyBlock();
    allPassed &= TestMaterial();
    allPassed &= TestTextLayoutWordWrap();
    allPassed &= TestTextLayoutBoxOrigin();
    allPassed &= TestTextLayoutAlignment();

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "ALL RENDERING TESTS PASSED!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}
