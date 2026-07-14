/**
 * @file Sprite2DComponentTests.cpp
 * @brief Headless console tests for the 2D-rendering components.
 *
 * These tests exercise the pure data/state layer of each 2D component: setter/getter
 * round-trips, default values, enum handling and collection operations. GPU/renderer
 * dependent methods (buildDrawCommands, getWorldBounds, draw/upload paths, asset
 * loading from disk) are deliberately avoided so the suite runs without a window,
 * graphics device or texture files.
 */

#include "lupine/engine/Engine.hpp"
#include "lupine/components/Sprite2D.hpp"
#include "lupine/components/AnimatedSprite2D.hpp"
#include "lupine/components/Shape2D.hpp"
#include "lupine/components/Line2D.hpp"
#include "lupine/components/VectorGraphic2D.hpp"
#include "lupine/components/Light2D.hpp"
#include "lupine/components/YSort.hpp"
#include "lupine/components/TileMap2D.hpp"
#include "TestFramework.hpp"
#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <cmath>

using namespace lupine;
using namespace lupine::core;
using namespace lupine::components;

namespace {

bool FloatEq(float a, float b, float tol = 1e-4f) {
    return std::fabs(a - b) < tol;
}

bool ColorEq(const math::Color& c, float r, float g, float b, float a, float tol = 1e-4f) {
    return FloatEq(c.r, r, tol) && FloatEq(c.g, g, tol) && FloatEq(c.b, b, tol) && FloatEq(c.a, a, tol);
}

bool Vec2Eq(const math::Vec2& v, float x, float y, float tol = 1e-4f) {
    return FloatEq(v.x, x, tol) && FloatEq(v.y, y, tol);
}

bool TestSprite2D() {
    TEST_SECTION("Sprite2D: Properties");

    std::shared_ptr<Sprite2D> sprite = std::make_shared<Sprite2D>("TestSprite");
    sprite->RegisterProperties();
    TEST_ASSERT(sprite->GetTypeName() == "Sprite2D", "Type name is Sprite2D");

    sprite->SetTexturePath("res://textures/hero.png");
    TEST_ASSERT(sprite->GetTexturePath() == "res://textures/hero.png", "Texture path round-trips");

    sprite->SetModulate(math::Color(0.25f, 0.5f, 0.75f, 0.5f));
    TEST_ASSERT(ColorEq(sprite->GetModulate(), 0.25f, 0.5f, 0.75f, 0.5f), "Modulate color round-trips");

    sprite->SetSize(math::Vec2(128.0f, 64.0f));
    TEST_ASSERT(Vec2Eq(sprite->GetSize(), 128.0f, 64.0f), "Size round-trips");

    sprite->SetUVRect(math::Vec4(0.1f, 0.2f, 0.3f, 0.4f));
    TEST_ASSERT(FloatEq(sprite->GetUVRect().x, 0.1f) && FloatEq(sprite->GetUVRect().w, 0.4f),
                "UV rect round-trips");

    sprite->SetAlphaCutoff(0.6f);
    TEST_ASSERT(FloatEq(sprite->GetAlphaCutoff(), 0.6f), "Alpha cutoff round-trips");

    sprite->SetFlipH(true);
    sprite->SetFlipV(true);
    TEST_ASSERT(sprite->GetFlipH() && sprite->GetFlipV(), "Flip H/V flags round-trip true");
    sprite->SetFlipH(false);
    sprite->SetFlipV(false);
    TEST_ASSERT(!sprite->GetFlipH() && !sprite->GetFlipV(), "Flip H/V flags round-trip false");

    sprite->SetCentered(false);
    TEST_ASSERT(!sprite->GetCentered(), "Centered flag round-trips false");
    sprite->SetCentered(true);
    TEST_ASSERT(sprite->GetCentered(), "Centered flag round-trips true");

    sprite->SetOffset(math::Vec2(-5.0f, 10.0f));
    TEST_ASSERT(Vec2Eq(sprite->GetOffset(), -5.0f, 10.0f), "Offset round-trips");

    return true;
}

bool TestSprite2DSpriteSheet() {
    TEST_SECTION("Sprite2D: Sprite Sheet");

    std::shared_ptr<Sprite2D> sprite = std::make_shared<Sprite2D>();
    sprite->RegisterProperties();

    sprite->SetSpriteSheetEnabled(true);
    TEST_ASSERT(sprite->GetSpriteSheetEnabled(), "Sprite sheet enabled round-trips");

    sprite->SetSpriteSize(math::Vec2(32.0f, 32.0f));
    TEST_ASSERT(Vec2Eq(sprite->GetSpriteSize(), 32.0f, 32.0f), "Sprite size round-trips");

    sprite->SetHFrames(8);
    sprite->SetVFrames(4);
    TEST_ASSERT(sprite->GetHFrames() == 8, "HFrames round-trips");
    TEST_ASSERT(sprite->GetVFrames() == 4, "VFrames round-trips");

    sprite->SetCurrentFrame(5);
    TEST_ASSERT(sprite->GetCurrentFrame() == 5, "Current frame round-trips");

    math::Vec2 texSize = sprite->GetTextureSize();
    TEST_ASSERT(Vec2Eq(texSize, 0.0f, 0.0f), "Texture size is (0,0) with no texture loaded");

    return true;
}

bool TestAnimatedSprite2D() {
    TEST_SECTION("AnimatedSprite2D: Properties");

    std::shared_ptr<AnimatedSprite2D> anim = std::make_shared<AnimatedSprite2D>();
    anim->RegisterProperties();
    TEST_ASSERT(anim->GetTypeName() == "AnimatedSprite2D", "Type name is AnimatedSprite2D");

    TEST_ASSERT(!anim->IsPlaying(), "New animated sprite is not playing by default");
    TEST_ASSERT(anim->GetCurrentFrame() == 0, "Current frame defaults to 0");

    anim->SetAnimationFilePath("res://anim/walk.spriteanimation");
    TEST_ASSERT(anim->GetAnimationFilePath() == "res://anim/walk.spriteanimation",
                "Animation file path round-trips");

    anim->SetAnimationName("walk");
    TEST_ASSERT(anim->GetAnimationName() == "walk", "Animation name round-trips");

    anim->SetPlaying(true);
    TEST_ASSERT(anim->IsPlaying(), "Playing flag round-trips true");
    anim->SetPlaying(false);
    TEST_ASSERT(!anim->IsPlaying(), "Playing flag round-trips false");

    anim->SetLoop(true);
    TEST_ASSERT(anim->GetLoop(), "Loop flag round-trips");

    anim->SetAutoPlay(true);
    TEST_ASSERT(anim->GetAutoPlay(), "Auto play flag round-trips");

    anim->SetOffset(math::Vec2(7.0f, -3.0f));
    TEST_ASSERT(Vec2Eq(anim->GetOffset(), 7.0f, -3.0f), "Offset round-trips");

    anim->SetFlipH(true);
    anim->SetFlipV(true);
    TEST_ASSERT(anim->GetFlipH() && anim->GetFlipV(), "Flip H/V round-trip");

    anim->SetModulate(math::Color(0.1f, 0.2f, 0.3f, 0.9f));
    TEST_ASSERT(ColorEq(anim->GetModulate(), 0.1f, 0.2f, 0.3f, 0.9f), "Modulate round-trips");

    anim->SetPixelSnap(true);
    TEST_ASSERT(anim->GetPixelSnap(), "Pixel snap round-trips");

    return true;
}

bool TestShape2D() {
    TEST_SECTION("Shape2D: Properties");

    std::shared_ptr<Shape2D> shape = std::make_shared<Shape2D>("TestShape");
    shape->RegisterProperties();
    TEST_ASSERT(shape->GetTypeName() == "Shape2D", "Type name is Shape2D");

    shape->SetShapeType(Shape2D::ShapeType::Hexagon);
    TEST_ASSERT(shape->GetShapeType() == Shape2D::ShapeType::Hexagon, "Shape type round-trips Hexagon");
    shape->SetShapeType(Shape2D::ShapeType::Circle);
    TEST_ASSERT(shape->GetShapeType() == Shape2D::ShapeType::Circle, "Shape type round-trips Circle");

    shape->SetColor(math::Color(0.2f, 0.4f, 0.6f, 0.8f));
    TEST_ASSERT(ColorEq(shape->GetColor(), 0.2f, 0.4f, 0.6f, 0.8f), "Fill color round-trips");

    shape->SetFilled(false);
    TEST_ASSERT(!shape->GetFilled(), "Filled flag round-trips false");
    shape->SetFilled(true);
    TEST_ASSERT(shape->GetFilled(), "Filled flag round-trips true");

    shape->SetSize(50.0f);
    TEST_ASSERT(FloatEq(shape->GetSize(), 50.0f), "Size round-trips");

    shape->SetWidth(80.0f);
    shape->SetHeight(40.0f);
    TEST_ASSERT(FloatEq(shape->GetWidth(), 80.0f), "Width round-trips");
    TEST_ASSERT(FloatEq(shape->GetHeight(), 40.0f), "Height round-trips");

    shape->SetRadius(25.0f);
    TEST_ASSERT(FloatEq(shape->GetRadius(), 25.0f), "Radius round-trips");

    shape->SetBorderEnabled(true);
    TEST_ASSERT(shape->GetBorderEnabled(), "Border enabled round-trips");
    shape->SetBorderColor(math::Color(1.0f, 0.0f, 0.0f, 1.0f));
    TEST_ASSERT(ColorEq(shape->GetBorderColor(), 1.0f, 0.0f, 0.0f, 1.0f), "Border color round-trips");
    shape->SetBorderWidth(3.5f);
    TEST_ASSERT(FloatEq(shape->GetBorderWidth(), 3.5f), "Border width round-trips");

    shape->SetCircleSegments(48);
    TEST_ASSERT(shape->GetCircleSegments() == 48, "Circle segments round-trips");

    shape->SetLayer(3);
    TEST_ASSERT(shape->GetLayer() == 3, "Layer round-trips");
    shape->SetSortingOrder(-2);
    TEST_ASSERT(shape->GetSortingOrder() == -2, "Sorting order round-trips");

    shape->SetUISpace(true);
    TEST_ASSERT(shape->GetUISpace(), "UI space flag round-trips");

    shape->SetShader("res://shaders/glow.lsh");
    TEST_ASSERT(shape->GetShader() == "res://shaders/glow.lsh", "Shader path round-trips");
    shape->SetShaderParameters("{\"intensity\":1.0}");
    TEST_ASSERT(shape->GetShaderParameters() == "{\"intensity\":1.0}", "Shader parameters round-trip");

    return true;
}

bool TestLine2DPoints() {
    TEST_SECTION("Line2D: Point Management");

    std::shared_ptr<Line2D> line = std::make_shared<Line2D>("TestLine");
    line->RegisterProperties();
    TEST_ASSERT(line->GetTypeName() == "Line2D", "Type name is Line2D");
    TEST_ASSERT(line->GetPointCount() == 0, "New line has no points");

    line->AddPoint(math::Vec2(0.0f, 0.0f));
    line->AddPoint(math::Vec2(10.0f, 0.0f));
    line->AddPoint(math::Vec2(10.0f, 10.0f));
    TEST_ASSERT(line->GetPointCount() == 3, "Three points added");
    TEST_ASSERT(Vec2Eq(line->GetPoint(1), 10.0f, 0.0f), "Second point position is correct");

    line->SetPoint(1, math::Vec2(20.0f, 5.0f));
    TEST_ASSERT(Vec2Eq(line->GetPoint(1), 20.0f, 5.0f), "SetPoint updates position");

    line->InsertPoint(1, math::Vec2(2.0f, 2.0f));
    TEST_ASSERT(line->GetPointCount() == 4, "InsertPoint grows the list");
    TEST_ASSERT(Vec2Eq(line->GetPoint(1), 2.0f, 2.0f), "Inserted point is at the index");

    line->RemovePoint(1);
    TEST_ASSERT(line->GetPointCount() == 3, "RemovePoint shrinks the list");

    TEST_ASSERT(Vec2Eq(line->GetBeginning(), 0.0f, 0.0f), "Beginning is the first point");
    line->SetEnd(math::Vec2(99.0f, 99.0f));
    TEST_ASSERT(Vec2Eq(line->GetEnd(), 99.0f, 99.0f), "SetEnd updates the last point");

    std::vector<math::Vec2> pts = { math::Vec2(1.0f, 1.0f), math::Vec2(2.0f, 2.0f) };
    line->SetPoints(pts);
    TEST_ASSERT(line->GetPointCount() == 2, "SetPoints replaces the point list");
    TEST_ASSERT(line->GetPointPositions().size() == 2, "GetPointPositions reflects the points");

    line->ClearPoints();
    TEST_ASSERT(line->GetPointCount() == 0, "ClearPoints empties the list");

    return true;
}

bool TestLine2DBezier() {
    TEST_SECTION("Line2D: Bezier And Stroke");

    std::shared_ptr<Line2D> line = std::make_shared<Line2D>();
    line->RegisterProperties();

    Line2DPoint p0(math::Vec2(0.0f, 0.0f));
    Line2DPoint p1(math::Vec2(50.0f, 0.0f), math::Vec2(-10.0f, 5.0f), math::Vec2(10.0f, -5.0f), true, true);
    line->AddLine2DPoint(p0);
    line->AddLine2DPoint(p1);
    TEST_ASSERT(line->GetPointCount() == 2, "Two Bezier points added");
    TEST_ASSERT(line->GetLine2DPoints().size() == 2, "GetLine2DPoints returns both points");

    TEST_ASSERT(line->GetPointUseBezier(1), "Second point uses Bezier");
    TEST_ASSERT(Vec2Eq(line->GetPointControlIn(1), -10.0f, 5.0f), "Control in handle round-trips");
    TEST_ASSERT(Vec2Eq(line->GetPointControlOut(1), 10.0f, -5.0f), "Control out handle round-trips");

    line->SetPointUseBezier(0, true);
    TEST_ASSERT(line->GetPointUseBezier(0), "SetPointUseBezier enables Bezier on a point");

    line->SetPointControlHandles(0, math::Vec2(3.0f, 4.0f), math::Vec2(-3.0f, -4.0f), false);
    TEST_ASSERT(Vec2Eq(line->GetPointControlIn(0), 3.0f, 4.0f), "SetPointControlHandles updates control in");

    line->SetStrokeColor(math::Color(0.9f, 0.8f, 0.7f, 1.0f));
    TEST_ASSERT(ColorEq(line->GetStrokeColor(), 0.9f, 0.8f, 0.7f, 1.0f), "Stroke color round-trips");

    line->SetStrokeWidth(4.0f);
    TEST_ASSERT(FloatEq(line->GetStrokeWidth(), 4.0f), "Stroke width round-trips");

    line->SetClosedLoop(true);
    TEST_ASSERT(line->GetClosedLoop(), "Closed loop flag round-trips");

    line->SetCapStyle(Line2D::CapStyle::Round);
    TEST_ASSERT(line->GetCapStyle() == Line2D::CapStyle::Round, "Cap style round-trips");
    line->SetJoinStyle(Line2D::JoinStyle::Bevel);
    TEST_ASSERT(line->GetJoinStyle() == Line2D::JoinStyle::Bevel, "Join style round-trips");

    line->SetAntiAliasing(false);
    TEST_ASSERT(!line->GetAntiAliasing(), "Anti-aliasing flag round-trips");
    line->SetSmoothness(16);
    TEST_ASSERT(line->GetSmoothness() == 16, "Smoothness round-trips");

    line->SetLayer(2);
    TEST_ASSERT(line->GetLayer() == 2, "Layer round-trips");
    line->SetSortingOrder(7);
    TEST_ASSERT(line->GetSortingOrder() == 7, "Sorting order round-trips");
    line->SetUISpace(true);
    TEST_ASSERT(line->GetUISpace(), "UI space flag round-trips");

    line->SetShowBezierHandles(true);
    TEST_ASSERT(line->GetShowBezierHandles(), "Show Bezier handles round-trips");
    line->SetSelectedPointIndex(1);
    TEST_ASSERT(line->GetSelectedPointIndex() == 1, "Selected point index round-trips");

    return true;
}

bool TestVectorGraphic2D() {
    TEST_SECTION("VectorGraphic2D: Properties");

    std::shared_ptr<VectorGraphic2D> vec = std::make_shared<VectorGraphic2D>("TestVector");
    vec->RegisterProperties();
    TEST_ASSERT(vec->GetTypeName() == "VectorGraphic2D", "Type name is VectorGraphic2D");
    TEST_ASSERT(!vec->IsLoaded(), "New vector graphic has no document loaded");

    vec->SetSVGPath("res://icons/logo.svg");
    TEST_ASSERT(vec->GetSVGPath() == "res://icons/logo.svg", "SVG path round-trips");

    vec->SetSize(math::Vec2(200.0f, 150.0f));
    TEST_ASSERT(Vec2Eq(vec->GetSize(), 200.0f, 150.0f), "Size round-trips");

    vec->SetUniformScale(2.5f);
    TEST_ASSERT(FloatEq(vec->GetUniformScale(), 2.5f), "Uniform scale round-trips");

    vec->SetPreserveAspectRatio(false);
    TEST_ASSERT(!vec->GetPreserveAspectRatio(), "Preserve aspect ratio round-trips");

    vec->SetTint(math::Color(0.3f, 0.6f, 0.9f, 1.0f));
    TEST_ASSERT(ColorEq(vec->GetTint(), 0.3f, 0.6f, 0.9f, 1.0f), "Tint round-trips");

    vec->SetOpacity(0.5f);
    TEST_ASSERT(FloatEq(vec->GetOpacity(), 0.5f), "Opacity round-trips");

    vec->SetCentered(false);
    TEST_ASSERT(!vec->GetCentered(), "Centered flag round-trips");

    vec->SetOffset(math::Vec2(12.0f, -8.0f));
    TEST_ASSERT(Vec2Eq(vec->GetOffset(), 12.0f, -8.0f), "Offset round-trips");

    vec->SetFlipH(true);
    vec->SetFlipV(true);
    TEST_ASSERT(vec->GetFlipH() && vec->GetFlipV(), "Flip H/V round-trip");

    vec->SetTessellationQuality(64);
    TEST_ASSERT(vec->GetTessellationQuality() == 64, "Tessellation quality round-trips");

    vec->SetUISpace(true);
    TEST_ASSERT(vec->GetUISpace(), "UI space flag round-trips");

    vec->SetLayer(5);
    TEST_ASSERT(vec->GetLayer() == 5, "Layer round-trips");

    vec->Clear();
    TEST_ASSERT(!vec->IsLoaded(), "Clear leaves the component without a document");
    TEST_ASSERT(vec->GetPathCount() == 0, "Path count is zero with no document");

    return true;
}

bool TestVectorGraphic2DInline() {
    TEST_SECTION("VectorGraphic2D: Inline SVG Parsing");

    std::shared_ptr<VectorGraphic2D> vec = std::make_shared<VectorGraphic2D>();
    vec->RegisterProperties();

    const std::string svg =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\" viewBox=\"0 0 100 100\">"
        "<rect x=\"10\" y=\"10\" width=\"80\" height=\"80\" fill=\"#ff0000\"/>"
        "</svg>";

    bool parsed = vec->LoadSVGFromString(svg);
    TEST_ASSERT(parsed, "Inline SVG string parses successfully");
    TEST_ASSERT(vec->IsLoaded(), "Component reports a loaded document after parsing");

    return true;
}

bool TestLight2D() {
    TEST_SECTION("Light2D: Properties");

    std::shared_ptr<Light2D> light = std::make_shared<Light2D>("TestLight");
    light->RegisterProperties();
    TEST_ASSERT(light->GetTypeName() == "Light2D", "Type name is Light2D");

    light->SetColor(math::Color(1.0f, 0.9f, 0.7f, 1.0f));
    TEST_ASSERT(ColorEq(light->GetColor(), 1.0f, 0.9f, 0.7f, 1.0f), "Light color round-trips");

    light->SetRange(256.0f);
    TEST_ASSERT(FloatEq(light->GetRange(), 256.0f), "Range round-trips");

    light->SetIntensity(2.5f);
    TEST_ASSERT(FloatEq(light->GetIntensity(), 2.5f), "Intensity round-trips");

    light->SetFalloff(1.5f);
    TEST_ASSERT(FloatEq(light->GetFalloff(), 1.5f), "Falloff round-trips");

    light->SetBlendMode(Light2DBlendMode::Additive);
    TEST_ASSERT(light->GetBlendMode() == Light2DBlendMode::Additive, "Blend mode round-trips Additive");
    light->SetBlendMode(Light2DBlendMode::Overlay);
    TEST_ASSERT(light->GetBlendMode() == Light2DBlendMode::Overlay, "Blend mode round-trips Overlay");
    light->SetBlendMode(Light2DBlendMode::SoftLight);
    TEST_ASSERT(light->GetBlendMode() == Light2DBlendMode::SoftLight, "Blend mode round-trips SoftLight");

    light->SetLightEnabled(false);
    TEST_ASSERT(!light->GetLightEnabled(), "Light enabled flag round-trips false");
    light->SetLightEnabled(true);
    TEST_ASSERT(light->GetLightEnabled(), "Light enabled flag round-trips true");

    light->SetLayer(4);
    TEST_ASSERT(light->GetLayer() == 4, "Layer round-trips");
    light->SetSortingOrder(-1);
    TEST_ASSERT(light->GetSortingOrder() == -1, "Sorting order round-trips");

    return true;
}

bool TestYSort() {
    TEST_SECTION("YSort: Properties");

    std::shared_ptr<YSort> ysort = std::make_shared<YSort>("TestYSort");
    ysort->RegisterProperties();
    TEST_ASSERT(ysort->GetTypeName() == "YSort", "Type name is YSort");

    ysort->SetEnabled(false);
    TEST_ASSERT(!ysort->GetEnabled(), "Enabled flag round-trips false");
    ysort->SetEnabled(true);
    TEST_ASSERT(ysort->GetEnabled(), "Enabled flag round-trips true");

    ysort->SetSortAxis(YSort::SortAxis::X);
    TEST_ASSERT(ysort->GetSortAxis() == YSort::SortAxis::X, "Sort axis round-trips X");
    ysort->SetSortAxis(YSort::SortAxis::Y);
    TEST_ASSERT(ysort->GetSortAxis() == YSort::SortAxis::Y, "Sort axis round-trips Y");

    ysort->SetInvert(true);
    TEST_ASSERT(ysort->GetInvert(), "Invert flag round-trips");

    ysort->SetUpdateMode(YSort::UpdateMode::Manual);
    TEST_ASSERT(ysort->GetUpdateMode() == YSort::UpdateMode::Manual, "Update mode round-trips Manual");
    ysort->SetUpdateMode(YSort::UpdateMode::OnTransformChange);
    TEST_ASSERT(ysort->GetUpdateMode() == YSort::UpdateMode::OnTransformChange,
                "Update mode round-trips OnTransformChange");
    ysort->SetUpdateMode(YSort::UpdateMode::EveryFrame);
    TEST_ASSERT(ysort->GetUpdateMode() == YSort::UpdateMode::EveryFrame,
                "Update mode round-trips EveryFrame");

    ysort->SetZOffset(100);
    TEST_ASSERT(ysort->GetZOffset() == 100, "Z offset round-trips");

    ysort->SetAffectOnlySpriteChildren(true);
    TEST_ASSERT(ysort->GetAffectOnlySpriteChildren(), "Affect-only-sprite-children flag round-trips");

    return true;
}

bool TestTileMap2D() {
    TEST_SECTION("TileMap2D: Properties And Defaults");

    std::shared_ptr<TileMap2D> map = std::make_shared<TileMap2D>("TestTileMap");
    map->RegisterProperties();
    TEST_ASSERT(map->GetTypeName() == "TileMap2D", "Type name is TileMap2D");

    TEST_ASSERT(map->GetMapWidth() == 32, "Default map width is 32");
    TEST_ASSERT(map->GetMapHeight() == 32, "Default map height is 32");
    TEST_ASSERT(map->GetTileWidth() == 32, "Default tile width is 32");
    TEST_ASSERT(map->GetTileHeight() == 32, "Default tile height is 32");
    TEST_ASSERT(Vec2Eq(map->GetMapSizePixels(), 1024.0f, 1024.0f), "Map size in pixels is width*tile");

    TEST_ASSERT(map->GetLayerCount() == 0, "New tilemap has no layers");
    TEST_ASSERT(map->GetTilesetCount() == 0, "New tilemap has no tilesets");
    TEST_ASSERT(map->GetLayer(0) == nullptr, "GetLayer returns null with no layers");
    TEST_ASSERT(map->GetLayerByName("missing") == nullptr, "GetLayerByName returns null for missing layer");
    TEST_ASSERT(map->GetTileset(0) == nullptr, "GetTileset returns null with no tilesets");

    map->SetTileMapPath("res://maps/level1.tilemap");
    TEST_ASSERT(map->GetTileMapPath() == "res://maps/level1.tilemap", "Tilemap path round-trips");

    map->SetBaseZIndex(10);
    TEST_ASSERT(map->GetBaseZIndex() == 10, "Base Z-index round-trips");

    map->SetShowCollision(true);
    TEST_ASSERT(map->GetShowCollision(), "Show collision flag round-trips");

    map->SetModulate(math::Color(0.5f, 0.5f, 0.5f, 1.0f));
    TEST_ASSERT(ColorEq(map->GetModulate(), 0.5f, 0.5f, 0.5f, 1.0f), "Modulate round-trips");

    TileMapCellData empty = map->GetCellData(0, 0, 0);
    TEST_ASSERT(!empty.hasData, "Cell data on an empty map reports no data");
    TEST_ASSERT(!map->HasCollisionAt(0, 0), "No collision on an empty map");

    std::vector<TileMapCellData> allLayers = map->GetCellDataAllLayers(0, 0);
    TEST_ASSERT(allLayers.empty(), "All-layer cell data is empty with no layers");

    TEST_ASSERT(!map->SetLayerVisible(0, true), "SetLayerVisible fails for out-of-range index");
    TEST_ASSERT(!map->SetLayerOpacity("missing", 0.5f), "SetLayerOpacity fails for missing layer");

    return true;
}

bool TestTileMap2DStructs() {
    TEST_SECTION("TileMap2D: Layer Key Encoding");

    uint64_t key = TileMapLayer::MakeKey(7, -3);
    int x = 0;
    int y = 0;
    TileMapLayer::SplitKey(key, x, y);
    TEST_ASSERT(x == 7 && y == -3, "Layer key round-trips signed coordinates");

    uint64_t key2 = TileMapLayer::MakeKey(-100, 250);
    int x2 = 0;
    int y2 = 0;
    TileMapLayer::SplitKey(key2, x2, y2);
    TEST_ASSERT(x2 == -100 && y2 == 250, "Layer key round-trips a second coordinate pair");

    TileInstance instance;
    TEST_ASSERT(instance.tilesetIndex == -1 && instance.tileIndex == -1,
                "Default tile instance is empty");

    TileMapLayer layer;
    TEST_ASSERT(layer.visible && FloatEq(layer.opacity, 1.0f),
                "Default layer is visible at full opacity");

    return true;
}

} // namespace

void RunSprite2DComponentTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "SPRITE2D COMPONENT TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    lupine_test::SetCurrentSuite("Sprite2DComponents");

    engine::InitializeEngine();

    bool allPassed = true;
    allPassed &= TestSprite2D();
    allPassed &= TestSprite2DSpriteSheet();
    allPassed &= TestAnimatedSprite2D();
    allPassed &= TestShape2D();
    allPassed &= TestLine2DPoints();
    allPassed &= TestLine2DBezier();
    allPassed &= TestVectorGraphic2D();
    allPassed &= TestVectorGraphic2DInline();
    allPassed &= TestLight2D();
    allPassed &= TestYSort();
    allPassed &= TestTileMap2D();
    allPassed &= TestTileMap2DStructs();

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "ALL SPRITE2D COMPONENT TESTS PASSED!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}
