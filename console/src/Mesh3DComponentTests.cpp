/**
 * @file Mesh3DComponentTests.cpp
 * @brief Headless console tests for the 3D rendering / mesh components.
 *
 * These tests exercise the pure data/configuration layer of the 3D renderable
 * components (Sprite3D, AnimatedSprite3D, PrimitiveMesh3D, StaticMesh3D,
 * SkeletalMesh3D, MultiMeshGeneric and the scatter family). No GPU device,
 * window, model file or texture upload is ever required: every assertion drives
 * setter/getter round-trips, enum handling, default values, boundary clamping
 * and instance-collection management.
 *
 * All getters/setters route through the ComponentProperty registry, which is
 * only populated by DefineProperties(); the registry is therefore initialised
 * explicitly per component (the component constructors do not call it).
 */

#include "lupine/engine/Engine.hpp"
#include "lupine/components/Sprite3D.hpp"
#include "lupine/components/AnimatedSprite3D.hpp"
#include "lupine/components/PrimitiveMesh3D.hpp"
#include "lupine/components/StaticMesh3D.hpp"
#include "lupine/components/SkeletalMesh3D.hpp"
#include "lupine/components/MultiMeshGeneric.hpp"
#include "lupine/components/ScatterMultiMesh.hpp"
#include "lupine/components/CollisionScatterMultiMesh.hpp"
#include "lupine/components/NodeScatter.hpp"
#include "TestFramework.hpp"
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <variant>
#include <cmath>

using namespace lupine;
using namespace lupine::core;
using namespace lupine::components;

namespace {

bool FloatEq(float a, float b, float tol = 1e-4f) {
    return std::fabs(a - b) < tol;
}

bool ColorEq(const math::Color& a, const math::Color& b, float tol = 1e-4f) {
    return FloatEq(a.r, b.r, tol) && FloatEq(a.g, b.g, tol) &&
           FloatEq(a.b, b.b, tol) && FloatEq(a.a, b.a, tol);
}

bool TestSprite3D() {
    TEST_SECTION("Sprite3D: State & Properties");

    std::shared_ptr<Sprite3D> sprite = std::make_shared<Sprite3D>("MySprite");
    sprite->DefineProperties();

    TEST_ASSERT(sprite->GetTypeName() == "Sprite3D", "Sprite3D reports its type name");
    TEST_ASSERT(sprite->GetName() == "MySprite", "Sprite3D keeps its constructed name");

    sprite->SetModulate(math::Color(0.2f, 0.4f, 0.6f, 0.8f));
    TEST_ASSERT(ColorEq(sprite->GetModulate(), math::Color(0.2f, 0.4f, 0.6f, 0.8f)),
                "Sprite3D modulate color round-trips");

    sprite->SetSize(math::Vec2(3.0f, 5.0f));
    TEST_ASSERT(FloatEq(sprite->GetSize().x, 3.0f) && FloatEq(sprite->GetSize().y, 5.0f),
                "Sprite3D size round-trips");

    sprite->SetUVRect(math::Vec4(0.1f, 0.2f, 0.5f, 0.6f));
    TEST_ASSERT(FloatEq(sprite->GetUVRect().x, 0.1f) && FloatEq(sprite->GetUVRect().w, 0.6f),
                "Sprite3D UV rect round-trips");

    sprite->SetAlphaCutoff(0.33f);
    TEST_ASSERT(FloatEq(sprite->GetAlphaCutoff(), 0.33f), "Sprite3D alpha cutoff round-trips");

    sprite->SetFlipH(true);
    sprite->SetFlipV(true);
    TEST_ASSERT(sprite->GetFlipH() && sprite->GetFlipV(), "Sprite3D flip flags set true");
    sprite->SetFlipH(false);
    TEST_ASSERT(!sprite->GetFlipH() && sprite->GetFlipV(), "Sprite3D flipH cleared independently");

    sprite->SetBillboardMode(BillboardMode::Enabled);
    TEST_ASSERT(sprite->GetBillboardMode() == BillboardMode::Enabled, "Sprite3D billboard Enabled");
    sprite->SetBillboardMode(BillboardMode::YAxisOnly);
    TEST_ASSERT(sprite->GetBillboardMode() == BillboardMode::YAxisOnly, "Sprite3D billboard YAxisOnly");
    sprite->SetBillboardMode(BillboardMode::Disabled);
    TEST_ASSERT(sprite->GetBillboardMode() == BillboardMode::Disabled, "Sprite3D billboard Disabled");

    sprite->SetPixelSize(0.02f);
    TEST_ASSERT(FloatEq(sprite->GetPixelSize(), 0.02f), "Sprite3D pixel size round-trips");

    sprite->SetDoubleSided(true);
    TEST_ASSERT(sprite->GetDoubleSided(), "Sprite3D double-sided round-trips");

    sprite->SetCastShadow(false);
    sprite->SetReceiveShadow(false);
    TEST_ASSERT(!sprite->GetCastShadow() && !sprite->GetReceiveShadow(),
                "Sprite3D shadow flags round-trip");

    sprite->SetSpriteSheetEnabled(true);
    TEST_ASSERT(sprite->GetSpriteSheetEnabled(), "Sprite3D sprite sheet flag round-trips");
    sprite->SetSpriteSize(math::Vec2(16.0f, 16.0f));
    TEST_ASSERT(FloatEq(sprite->GetSpriteSize().x, 16.0f), "Sprite3D sprite size round-trips");

    sprite->SetCurrentFrame(7);
    TEST_ASSERT(sprite->GetCurrentFrame() == 0,
                "Sprite3D current frame clamps to 0 without a loaded sheet texture");

    TEST_ASSERT(sprite->GetFrameCount() == 0, "Sprite3D frame count is 0 without a texture");
    TEST_ASSERT(sprite->GetFramesPerRow() == 0 && sprite->GetFramesPerColumn() == 0,
                "Sprite3D frames per row/column are 0 without a texture");

    sprite->SetTexturePath("res://textures/test.png");
    TEST_ASSERT(sprite->GetTexturePath() == "res://textures/test.png",
                "Sprite3D res:// texture path round-trips unchanged");

    TEST_ASSERT(sprite->getSpatialType() == SpatialType::World3D, "Sprite3D spatial type is World3D");
    TEST_ASSERT(!sprite->GetTextureHandle().isValid(), "Sprite3D has no GPU texture handle by default");

    return true;
}

bool TestAnimatedSprite3D() {
    TEST_SECTION("AnimatedSprite3D: State & Properties");

    std::shared_ptr<AnimatedSprite3D> sprite = std::make_shared<AnimatedSprite3D>();
    sprite->DefineProperties();

    TEST_ASSERT(sprite->GetTypeName() == "AnimatedSprite3D", "AnimatedSprite3D reports its type name");
    TEST_ASSERT(sprite->GetCurrentFrame() == 0, "AnimatedSprite3D defaults to frame 0");
    TEST_ASSERT(sprite->getSpatialType() == SpatialType::World3D, "AnimatedSprite3D spatial type is World3D");
    TEST_ASSERT(sprite->getRenderLayer() == RenderLayer::Opaque, "AnimatedSprite3D renders on the Opaque layer");

    sprite->SetAnimationName("walk");
    TEST_ASSERT(sprite->GetAnimationName() == "walk", "AnimatedSprite3D animation name round-trips");

    sprite->SetPlaying(true);
    TEST_ASSERT(sprite->IsPlaying(), "AnimatedSprite3D playing flag set true");
    sprite->SetPlaying(false);
    TEST_ASSERT(!sprite->IsPlaying(), "AnimatedSprite3D playing flag set false");

    sprite->SetLoop(true);
    TEST_ASSERT(sprite->GetLoop(), "AnimatedSprite3D loop flag round-trips");

    sprite->SetAutoPlay(true);
    TEST_ASSERT(sprite->GetAutoPlay(), "AnimatedSprite3D auto-play flag round-trips");

    sprite->SetOffset(math::Vec3(1.0f, 2.0f, 3.0f));
    TEST_ASSERT(FloatEq(sprite->GetOffset().x, 1.0f) && FloatEq(sprite->GetOffset().z, 3.0f),
                "AnimatedSprite3D offset round-trips");

    sprite->SetFlipH(true);
    sprite->SetFlipV(true);
    TEST_ASSERT(sprite->GetFlipH() && sprite->GetFlipV(), "AnimatedSprite3D flip flags round-trip");

    sprite->SetModulate(math::Color(0.5f, 0.25f, 0.75f, 1.0f));
    TEST_ASSERT(ColorEq(sprite->GetModulate(), math::Color(0.5f, 0.25f, 0.75f, 1.0f)),
                "AnimatedSprite3D modulate round-trips");

    sprite->SetBillboard(BillboardMode::YAxisOnly);
    TEST_ASSERT(sprite->GetBillboard() == BillboardMode::YAxisOnly, "AnimatedSprite3D billboard mode round-trips");

    sprite->SetCastShadows(false);
    sprite->SetReceiveShadows(false);
    TEST_ASSERT(!sprite->GetCastShadows() && !sprite->GetReceiveShadows(),
                "AnimatedSprite3D shadow flags round-trip");

    sprite->SetDoubleSided(true);
    TEST_ASSERT(sprite->GetDoubleSided(), "AnimatedSprite3D double-sided round-trips");

    sprite->Stop();
    TEST_ASSERT(!sprite->IsPlaying(), "AnimatedSprite3D Stop() halts playback");

    return true;
}

bool TestPrimitiveMesh3D() {
    TEST_SECTION("PrimitiveMesh3D: Shape & Material");

    std::shared_ptr<PrimitiveMesh3D> mesh = std::make_shared<PrimitiveMesh3D>("Prim");
    mesh->DefineProperties();

    TEST_ASSERT(mesh->GetTypeName() == "PrimitiveMesh3D", "PrimitiveMesh3D reports its type name");
    TEST_ASSERT(mesh->GetShape() == PrimitiveShape::Cube, "PrimitiveMesh3D defaults to Cube shape");

    const PrimitiveShape shapes[] = {
        PrimitiveShape::Cube, PrimitiveShape::Sphere, PrimitiveShape::Cylinder,
        PrimitiveShape::Cone, PrimitiveShape::Pyramid, PrimitiveShape::Torus,
        PrimitiveShape::Capsule
    };
    bool allShapesRoundTrip = true;
    for (PrimitiveShape shape : shapes) {
        mesh->SetShape(shape);
        if (mesh->GetShape() != shape) {
            allShapesRoundTrip = false;
        }
    }
    TEST_ASSERT(allShapesRoundTrip, "PrimitiveMesh3D round-trips every primitive shape enum");

    mesh->SetColor(math::Color(0.1f, 0.2f, 0.3f, 0.4f));
    TEST_ASSERT(ColorEq(mesh->GetColor(), math::Color(0.1f, 0.2f, 0.3f, 0.4f)),
                "PrimitiveMesh3D vertex color round-trips");

    mesh->SetSize(2.5f);
    TEST_ASSERT(FloatEq(mesh->GetSize(), 2.5f), "PrimitiveMesh3D size round-trips");

    mesh->SetHeight(4.0f);
    TEST_ASSERT(FloatEq(mesh->GetHeight(), 4.0f), "PrimitiveMesh3D height round-trips");

    mesh->SetDetail(48);
    TEST_ASSERT(mesh->GetDetail() == 48, "PrimitiveMesh3D detail round-trips");

    mesh->SetMinorRadius(0.35f);
    TEST_ASSERT(FloatEq(mesh->GetMinorRadius(), 0.35f), "PrimitiveMesh3D minor radius round-trips");

    mesh->SetCastShadow(false);
    mesh->SetReceiveShadow(false);
    mesh->SetDoubleSided(true);
    TEST_ASSERT(!mesh->GetCastShadow() && !mesh->GetReceiveShadow() && mesh->GetDoubleSided(),
                "PrimitiveMesh3D rendering flags round-trip");

    mesh->SetMetallic(0.7f);
    mesh->SetRoughness(0.25f);
    TEST_ASSERT(FloatEq(mesh->GetMetallic(), 0.7f) && FloatEq(mesh->GetRoughness(), 0.25f),
                "PrimitiveMesh3D metallic/roughness round-trip");

    mesh->SetAlbedoColor(math::Color(0.9f, 0.8f, 0.7f, 1.0f));
    TEST_ASSERT(ColorEq(mesh->GetAlbedoColor(), math::Color(0.9f, 0.8f, 0.7f, 1.0f)),
                "PrimitiveMesh3D albedo color round-trips");

    mesh->SetEmissiveColor(math::Color(0.0f, 0.5f, 1.0f, 1.0f));
    TEST_ASSERT(ColorEq(mesh->GetEmissiveColor(), math::Color(0.0f, 0.5f, 1.0f, 1.0f)),
                "PrimitiveMesh3D emissive color round-trips");

    mesh->SetEmissiveStrength(3.5f);
    TEST_ASSERT(FloatEq(mesh->GetEmissiveStrength(), 3.5f), "PrimitiveMesh3D emissive strength round-trips");

    mesh->SetNormalScale(1.5f);
    TEST_ASSERT(FloatEq(mesh->GetNormalScale(), 1.5f), "PrimitiveMesh3D normal scale round-trips");

    mesh->SetShaderType(ShaderType::Toon);
    TEST_ASSERT(mesh->GetShaderType() == ShaderType::Toon, "PrimitiveMesh3D shader type Toon round-trips");
    mesh->SetShaderType(ShaderType::Unlit);
    TEST_ASSERT(mesh->GetShaderType() == ShaderType::Unlit, "PrimitiveMesh3D shader type Unlit round-trips");
    mesh->SetShaderType(ShaderType::PBR);
    TEST_ASSERT(mesh->GetShaderType() == ShaderType::PBR, "PrimitiveMesh3D shader type PBR round-trips");

    mesh->SetCustomVertShaderPath("res://shaders/v.lsh");
    mesh->SetCustomFragShaderPath("res://shaders/f.lsh");
    TEST_ASSERT(mesh->GetCustomVertShaderPath() == "res://shaders/v.lsh" &&
                mesh->GetCustomFragShaderPath() == "res://shaders/f.lsh",
                "PrimitiveMesh3D custom shader paths round-trip");

    mesh->SetShadowBands(5.0f);
    mesh->SetShadowThreshold(0.6f);
    mesh->SetShadowSoftness(0.1f);
    mesh->SetSpecularBands(4.0f);
    mesh->SetSpecularPower(64.0f);
    mesh->SetRimIntensity(0.8f);
    mesh->SetRimPower(2.0f);
    TEST_ASSERT(FloatEq(mesh->GetShadowBands(), 5.0f) && FloatEq(mesh->GetShadowThreshold(), 0.6f) &&
                FloatEq(mesh->GetShadowSoftness(), 0.1f) && FloatEq(mesh->GetSpecularBands(), 4.0f) &&
                FloatEq(mesh->GetSpecularPower(), 64.0f) && FloatEq(mesh->GetRimIntensity(), 0.8f) &&
                FloatEq(mesh->GetRimPower(), 2.0f),
                "PrimitiveMesh3D toon shader parameters round-trip");

    mesh->SetMaterialOverrideEnabled(true);
    TEST_ASSERT(mesh->HasMaterialOverride(), "PrimitiveMesh3D material override can be enabled");
    mesh->SetMaterialOverrideEnabled(false);
    TEST_ASSERT(!mesh->HasMaterialOverride(), "PrimitiveMesh3D material override can be disabled");

    mesh->SetShaderParam("u_Glow", MaterialPropertyValue(2.5f));
    TEST_ASSERT(mesh->GetShaderParams().size() == 1, "PrimitiveMesh3D records a generic shader parameter");
    const std::unordered_map<std::string, MaterialPropertyValue>& params = mesh->GetShaderParams();
    bool paramValueOk = params.count("u_Glow") > 0 &&
                        std::holds_alternative<float>(params.at("u_Glow")) &&
                        FloatEq(std::get<float>(params.at("u_Glow")), 2.5f);
    TEST_ASSERT(paramValueOk, "PrimitiveMesh3D shader parameter stores the correct float value");
    mesh->ClearShaderParams();
    TEST_ASSERT(mesh->GetShaderParams().empty(), "PrimitiveMesh3D shader parameters cleared");

    TEST_ASSERT(mesh->getSpatialType() == SpatialType::World3D, "PrimitiveMesh3D spatial type is World3D");

    return true;
}

bool TestStaticMesh3D() {
    TEST_SECTION("StaticMesh3D: State & Material Slots");

    std::shared_ptr<StaticMesh3D> mesh = std::make_shared<StaticMesh3D>("Static");
    mesh->DefineProperties();

    TEST_ASSERT(mesh->GetTypeName() == "StaticMesh3D", "StaticMesh3D reports its type name");
    TEST_ASSERT(mesh->getSpatialType() == SpatialType::World3D, "StaticMesh3D spatial type is World3D");

    mesh->SetModelPath("res://models/crate.glb");
    TEST_ASSERT(mesh->GetModelPath() == "res://models/crate.glb",
                "StaticMesh3D res:// model path round-trips unchanged");

    mesh->SetCastShadow(false);
    mesh->SetReceiveShadow(false);
    mesh->SetDoubleSided(true);
    TEST_ASSERT(!mesh->GetCastShadow() && !mesh->GetReceiveShadow() && mesh->GetDoubleSided(),
                "StaticMesh3D rendering flags round-trip");

    TEST_ASSERT(mesh->GetMaterialSlotCount() == 0, "StaticMesh3D has no material slots without a loaded model");
    TEST_ASSERT(mesh->GetMaterialSlot(0) == nullptr, "StaticMesh3D returns null for out-of-range material slot");

    const StaticMesh3D* constMesh = mesh.get();
    TEST_ASSERT(constMesh->GetMaterialSlot(0) == nullptr,
                "StaticMesh3D const accessor returns null for out-of-range slot");

    TEST_ASSERT(!mesh->GetModelAsset().IsValid(), "StaticMesh3D model asset is invalid without a load");

    return true;
}

bool TestSkeletalMesh3D() {
    TEST_SECTION("SkeletalMesh3D: State & Animation");

    std::shared_ptr<SkeletalMesh3D> mesh = std::make_shared<SkeletalMesh3D>("Skel");
    mesh->DefineProperties();

    TEST_ASSERT(mesh->GetTypeName() == "SkeletalMesh3D", "SkeletalMesh3D reports its type name");
    TEST_ASSERT(mesh->getSpatialType() == SpatialType::World3D, "SkeletalMesh3D spatial type is World3D");
    TEST_ASSERT(!mesh->IsPlaying(), "SkeletalMesh3D is not playing by default");

    mesh->SetModelPath("res://models/char.gltf");
    TEST_ASSERT(mesh->GetModelPath() == "res://models/char.gltf",
                "SkeletalMesh3D res:// model path round-trips unchanged");

    mesh->SetDefaultAnimation("idle");
    TEST_ASSERT(mesh->GetDefaultAnimation() == "idle", "SkeletalMesh3D default animation round-trips");

    mesh->SetCurrentAnimation("run");
    TEST_ASSERT(mesh->GetCurrentAnimation() == "run", "SkeletalMesh3D current animation round-trips");

    mesh->SetPlaybackSpeed(2.0f);
    TEST_ASSERT(FloatEq(mesh->GetPlaybackSpeed(), 2.0f), "SkeletalMesh3D playback speed round-trips");

    mesh->SetLoop(false);
    TEST_ASSERT(!mesh->GetLoop(), "SkeletalMesh3D loop flag round-trips");

    mesh->SetAutoPlay(true);
    TEST_ASSERT(mesh->GetAutoPlay(), "SkeletalMesh3D auto-play flag round-trips");

    mesh->SetGPUSkinning(false);
    TEST_ASSERT(!mesh->GetGPUSkinning(), "SkeletalMesh3D GPU skinning flag round-trips");

    mesh->SetRootMotionEnabled(true);
    TEST_ASSERT(mesh->GetRootMotionEnabled(), "SkeletalMesh3D root motion flag round-trips");

    mesh->SetRootBoneName("Hips");
    TEST_ASSERT(mesh->GetRootBoneName() == "Hips", "SkeletalMesh3D root bone name round-trips");

    mesh->SetCastShadow(false);
    mesh->SetReceiveShadow(false);
    TEST_ASSERT(!mesh->GetCastShadow() && !mesh->GetReceiveShadow(),
                "SkeletalMesh3D shadow flags round-trip");

    mesh->SetShowSkeletonInEditor(true);
    TEST_ASSERT(mesh->GetShowSkeletonInEditor(), "SkeletalMesh3D editor skeleton flag round-trips");

    mesh->SetAnimationTime(1.25f);
    TEST_ASSERT(FloatEq(mesh->GetAnimationTime(), 1.25f), "SkeletalMesh3D animation time round-trips");

    TEST_ASSERT(mesh->GetMaterialSlotCount() == 0, "SkeletalMesh3D has no material slots without a model");
    TEST_ASSERT(mesh->GetMaterialSlot(0) == nullptr, "SkeletalMesh3D returns null for out-of-range slot");
    TEST_ASSERT(mesh->GetAnimationNames().empty(), "SkeletalMesh3D has no animation names without a model");

    return true;
}

bool TestMultiMeshGeneric() {
    TEST_SECTION("MultiMeshGeneric: Instances & Settings");

    std::shared_ptr<MultiMeshGeneric> mm = std::make_shared<MultiMeshGeneric>("MM");
    mm->DefineProperties();

    TEST_ASSERT(mm->GetTypeName() == "MultiMeshGeneric", "MultiMeshGeneric reports its type name");
    TEST_ASSERT(mm->getSpatialType() == SpatialType::World3D, "MultiMeshGeneric spatial type is World3D");
    TEST_ASSERT(mm->GetInstanceCount() == 0, "MultiMeshGeneric starts with zero instances");

    mm->SetMeshPath("res://models/grass.glb");
    TEST_ASSERT(mm->GetMeshPath() == "res://models/grass.glb", "MultiMeshGeneric mesh path round-trips");

    mm->SetMaterialOverride("res://materials/grass.mat");
    TEST_ASSERT(mm->GetMaterialOverride() == "res://materials/grass.mat",
                "MultiMeshGeneric material override path round-trips");

    mm->SetCastShadow(ShadowCastingMode::OnlyShadows);
    TEST_ASSERT(mm->GetCastShadow() == ShadowCastingMode::OnlyShadows,
                "MultiMeshGeneric shadow casting mode OnlyShadows round-trips");
    mm->SetCastShadow(ShadowCastingMode::Off);
    TEST_ASSERT(mm->GetCastShadow() == ShadowCastingMode::Off,
                "MultiMeshGeneric shadow casting mode Off round-trips");
    mm->SetCastShadow(ShadowCastingMode::On);
    TEST_ASSERT(mm->GetCastShadow() == ShadowCastingMode::On,
                "MultiMeshGeneric shadow casting mode On round-trips");

    mm->SetReceiveShadow(false);
    TEST_ASSERT(!mm->GetReceiveShadow(), "MultiMeshGeneric receive-shadow flag round-trips");

    mm->SetInstanceCount(4);
    TEST_ASSERT(mm->GetInstanceCount() == 4, "MultiMeshGeneric instance count round-trips");
    TEST_ASSERT(mm->GetInstances().size() == 4, "MultiMeshGeneric resizes its instance vector");

    math::Mat4 transform = math::Mat4::Translate(math::Vec3(10.0f, 20.0f, 30.0f));
    mm->SetInstanceTransform(1, transform);
    math::Vec4 translation = mm->GetInstanceTransform(1).GetColumn(3);
    TEST_ASSERT(FloatEq(translation.x, 10.0f) && FloatEq(translation.y, 20.0f) && FloatEq(translation.z, 30.0f),
                "MultiMeshGeneric per-instance transform round-trips");

    mm->SetInstanceColor(2, math::Color(0.1f, 0.5f, 0.9f, 1.0f));
    TEST_ASSERT(ColorEq(mm->GetInstanceColor(2), math::Color(0.1f, 0.5f, 0.9f, 1.0f)),
                "MultiMeshGeneric per-instance color round-trips");

    mm->SetInstanceCustomData(3, math::Vec4(1.0f, 2.0f, 3.0f, 4.0f));
    math::Vec4 custom = mm->GetInstanceCustomData(3);
    TEST_ASSERT(FloatEq(custom.x, 1.0f) && FloatEq(custom.w, 4.0f),
                "MultiMeshGeneric per-instance custom data round-trips");

    math::Vec4 oob = mm->GetInstanceCustomData(999);
    TEST_ASSERT(FloatEq(oob.x, 0.0f) && FloatEq(oob.w, 0.0f),
                "MultiMeshGeneric out-of-range custom data getter returns zero vector");

    mm->SetCullPerInstance(true);
    TEST_ASSERT(mm->GetCullPerInstance(), "MultiMeshGeneric per-instance culling flag round-trips");

    mm->SetMaxDistance(150.0f);
    TEST_ASSERT(FloatEq(mm->GetMaxDistance(), 150.0f), "MultiMeshGeneric max distance round-trips");

    mm->SetLodGroup("res://lod/grass_lod");
    TEST_ASSERT(mm->GetLodGroup() == "res://lod/grass_lod", "MultiMeshGeneric LOD group round-trips");

    mm->SetEditableInEditor(true);
    mm->SetPreviewSingleInstance(true);
    mm->SetPreviewInEditor(true);
    TEST_ASSERT(mm->GetEditableInEditor() && mm->GetPreviewSingleInstance() && mm->GetPreviewInEditor(),
                "MultiMeshGeneric editor flags round-trip");

    mm->SetInstanceCount(0);
    TEST_ASSERT(mm->GetInstanceCount() == 0 && mm->GetInstances().empty(),
                "MultiMeshGeneric clears instances when count set to zero");

    return true;
}

bool TestScatterMultiMesh() {
    TEST_SECTION("ScatterMultiMesh: Distribution Settings");

    std::shared_ptr<ScatterMultiMesh> scatter = std::make_shared<ScatterMultiMesh>("Scatter");
    scatter->DefineProperties();

    TEST_ASSERT(scatter->GetTypeName() == "ScatterMultiMesh", "ScatterMultiMesh reports its type name");

    scatter->SetMeshPath("res://models/rock.glb");
    TEST_ASSERT(scatter->GetMeshPath() == "res://models/rock.glb",
                "ScatterMultiMesh inherits mesh path property from MultiMeshGeneric");

    scatter->SetScatterShape(ScatterShapeType::Sphere);
    TEST_ASSERT(scatter->GetScatterShape() == ScatterShapeType::Sphere, "ScatterMultiMesh shape Sphere round-trips");
    scatter->SetScatterShape(ScatterShapeType::Cube);
    TEST_ASSERT(scatter->GetScatterShape() == ScatterShapeType::Cube, "ScatterMultiMesh shape Cube round-trips");

    scatter->SetWidth(20.0f);
    scatter->SetHeight(15.0f);
    scatter->SetDepth(25.0f);
    TEST_ASSERT(FloatEq(scatter->GetWidth(), 20.0f) && FloatEq(scatter->GetHeight(), 15.0f) &&
                FloatEq(scatter->GetDepth(), 25.0f),
                "ScatterMultiMesh cube dimensions round-trip");

    math::Vec3 cubeSize = scatter->GetCubeSize();
    TEST_ASSERT(FloatEq(cubeSize.x, 20.0f) && FloatEq(cubeSize.y, 15.0f) && FloatEq(cubeSize.z, 25.0f),
                "ScatterMultiMesh GetCubeSize aggregates width/height/depth");

    scatter->SetCubeSize(math::Vec3(5.0f, 6.0f, 7.0f));
    TEST_ASSERT(FloatEq(scatter->GetWidth(), 5.0f) && FloatEq(scatter->GetDepth(), 7.0f),
                "ScatterMultiMesh SetCubeSize updates the individual dimensions");

    scatter->SetRadius(12.0f);
    TEST_ASSERT(FloatEq(scatter->GetRadius(), 12.0f), "ScatterMultiMesh sphere radius round-trips");

    scatter->SetScatterCount(250);
    TEST_ASSERT(scatter->GetScatterCount() == 250, "ScatterMultiMesh scatter count round-trips");

    scatter->SetClumpingMode(ClumpingMode::Clustered);
    TEST_ASSERT(scatter->GetClumpingMode() == ClumpingMode::Clustered, "ScatterMultiMesh clumping Clustered round-trips");
    scatter->SetClumpingMode(ClumpingMode::Centered);
    TEST_ASSERT(scatter->GetClumpingMode() == ClumpingMode::Centered, "ScatterMultiMesh clumping Centered round-trips");
    scatter->SetClumpingMode(ClumpingMode::None);
    TEST_ASSERT(scatter->GetClumpingMode() == ClumpingMode::None, "ScatterMultiMesh clumping None round-trips");

    scatter->SetClumpFactor(0.65f);
    scatter->SetClumpCount(8);
    scatter->SetClumpRadius(3.5f);
    TEST_ASSERT(FloatEq(scatter->GetClumpFactor(), 0.65f) && scatter->GetClumpCount() == 8 &&
                FloatEq(scatter->GetClumpRadius(), 3.5f),
                "ScatterMultiMesh clumping parameters round-trip");

    scatter->SetScaleRange(math::Vec2(0.5f, 2.0f));
    math::Vec2 range = scatter->GetScaleRange();
    TEST_ASSERT(FloatEq(range.x, 0.5f) && FloatEq(range.y, 2.0f), "ScatterMultiMesh scale range round-trips");

    scatter->SetUniformScale(false);
    TEST_ASSERT(!scatter->GetUniformScale(), "ScatterMultiMesh uniform scale flag round-trips");

    scatter->SetRotationVariation(math::Vec3(10.0f, 180.0f, 30.0f));
    TEST_ASSERT(FloatEq(scatter->GetRotationVariation().y, 180.0f),
                "ScatterMultiMesh rotation variation round-trips");

    scatter->SetAlignToSurface(true);
    TEST_ASSERT(scatter->GetAlignToSurface(), "ScatterMultiMesh align-to-surface flag round-trips");

    scatter->SetColorVariationEnabled(true);
    TEST_ASSERT(scatter->GetColorVariationEnabled(), "ScatterMultiMesh color variation flag round-trips");
    scatter->SetBaseColor(math::Color(0.3f, 0.6f, 0.2f, 1.0f));
    TEST_ASSERT(ColorEq(scatter->GetBaseColor(), math::Color(0.3f, 0.6f, 0.2f, 1.0f)),
                "ScatterMultiMesh base color round-trips");
    scatter->SetColorVariation(0.4f);
    TEST_ASSERT(FloatEq(scatter->GetColorVariation(), 0.4f), "ScatterMultiMesh color variation amount round-trips");

    scatter->SetRandomSeed(98765);
    TEST_ASSERT(scatter->GetRandomSeed() == 98765, "ScatterMultiMesh random seed round-trips");
    scatter->SetUseRandomSeed(false);
    TEST_ASSERT(!scatter->GetUseRandomSeed(), "ScatterMultiMesh use-random-seed flag round-trips");

    scatter->SetShowDebugShape(false);
    TEST_ASSERT(!scatter->GetShowDebugShape(), "ScatterMultiMesh debug shape flag round-trips");
    scatter->SetDebugColor(math::Color(1.0f, 0.0f, 0.0f, 0.5f));
    TEST_ASSERT(ColorEq(scatter->GetDebugColor(), math::Color(1.0f, 0.0f, 0.0f, 0.5f)),
                "ScatterMultiMesh debug color round-trips");

    return true;
}

bool TestCollisionScatterMultiMesh() {
    TEST_SECTION("CollisionScatterMultiMesh: Collision-Based Scatter");

    std::shared_ptr<CollisionScatterMultiMesh> scatter = std::make_shared<CollisionScatterMultiMesh>("CScatter");
    scatter->DefineProperties();

    TEST_ASSERT(scatter->GetTypeName() == "CollisionScatterMultiMesh",
                "CollisionScatterMultiMesh reports its type name");

    scatter->SetScatterLayers(0x0F);
    TEST_ASSERT(scatter->GetScatterLayers() == 0x0Fu, "CollisionScatterMultiMesh scatter layers round-trip");
    scatter->SetCutoutLayers(0x30);
    TEST_ASSERT(scatter->GetCutoutLayers() == 0x30u, "CollisionScatterMultiMesh cutout layers round-trip");

    scatter->SetDistributionMode(ScatterDistributionMode::Grid);
    TEST_ASSERT(scatter->GetDistributionMode() == ScatterDistributionMode::Grid,
                "CollisionScatterMultiMesh distribution Grid round-trips");
    scatter->SetDistributionMode(ScatterDistributionMode::Outline);
    TEST_ASSERT(scatter->GetDistributionMode() == ScatterDistributionMode::Outline,
                "CollisionScatterMultiMesh distribution Outline round-trips");
    scatter->SetDistributionMode(ScatterDistributionMode::Random);
    TEST_ASSERT(scatter->GetDistributionMode() == ScatterDistributionMode::Random,
                "CollisionScatterMultiMesh distribution Random round-trips");

    scatter->SetPlacementMode(ScatterPlacementMode::Volume);
    TEST_ASSERT(scatter->GetPlacementMode() == ScatterPlacementMode::Volume,
                "CollisionScatterMultiMesh placement Volume round-trips");
    scatter->SetPlacementMode(ScatterPlacementMode::Surface);
    TEST_ASSERT(scatter->GetPlacementMode() == ScatterPlacementMode::Surface,
                "CollisionScatterMultiMesh placement Surface round-trips");

    scatter->SetDensity(2.5f);
    TEST_ASSERT(FloatEq(scatter->GetDensity(), 2.5f), "CollisionScatterMultiMesh density round-trips");
    scatter->SetMaxInstances(5000);
    TEST_ASSERT(scatter->GetMaxInstances() == 5000, "CollisionScatterMultiMesh max instances round-trips");

    scatter->SetGridSpacingX(1.5f);
    scatter->SetGridSpacingZ(2.5f);
    scatter->SetGridJitter(0.3f);
    TEST_ASSERT(FloatEq(scatter->GetGridSpacingX(), 1.5f) && FloatEq(scatter->GetGridSpacingZ(), 2.5f) &&
                FloatEq(scatter->GetGridJitter(), 0.3f),
                "CollisionScatterMultiMesh grid settings round-trip");

    scatter->SetAlignToNormal(true);
    scatter->SetNormalAlignmentStrength(0.75f);
    scatter->SetSurfaceOffset(0.05f);
    TEST_ASSERT(scatter->GetAlignToNormal() && FloatEq(scatter->GetNormalAlignmentStrength(), 0.75f) &&
                FloatEq(scatter->GetSurfaceOffset(), 0.05f),
                "CollisionScatterMultiMesh surface settings round-trip");

    scatter->SetMinSlope(5.0f);
    scatter->SetMaxSlope(45.0f);
    TEST_ASSERT(FloatEq(scatter->GetMinSlope(), 5.0f) && FloatEq(scatter->GetMaxSlope(), 45.0f),
                "CollisionScatterMultiMesh slope filter round-trips");

    scatter->SetScaleRange(math::Vec2(0.7f, 1.3f));
    math::Vec2 range = scatter->GetScaleRange();
    TEST_ASSERT(FloatEq(range.x, 0.7f) && FloatEq(range.y, 1.3f),
                "CollisionScatterMultiMesh scale range round-trips");
    scatter->SetUniformScale(false);
    TEST_ASSERT(!scatter->GetUniformScale(), "CollisionScatterMultiMesh uniform scale flag round-trips");

    scatter->SetRotationVariation(math::Vec3(0.0f, 90.0f, 0.0f));
    TEST_ASSERT(FloatEq(scatter->GetRotationVariation().y, 90.0f),
                "CollisionScatterMultiMesh rotation variation round-trips");
    scatter->SetRandomYRotation(true);
    TEST_ASSERT(scatter->GetRandomYRotation(), "CollisionScatterMultiMesh random-Y rotation flag round-trips");

    scatter->SetColorVariationEnabled(true);
    scatter->SetBaseColor(math::Color(0.2f, 0.2f, 0.8f, 1.0f));
    scatter->SetColorVariation(0.2f);
    TEST_ASSERT(scatter->GetColorVariationEnabled() &&
                ColorEq(scatter->GetBaseColor(), math::Color(0.2f, 0.2f, 0.8f, 1.0f)) &&
                FloatEq(scatter->GetColorVariation(), 0.2f),
                "CollisionScatterMultiMesh color variation settings round-trip");

    scatter->SetRandomSeed(424242);
    scatter->SetUseRandomSeed(false);
    TEST_ASSERT(scatter->GetRandomSeed() == 424242 && !scatter->GetUseRandomSeed(),
                "CollisionScatterMultiMesh randomization settings round-trip");

    scatter->SetScanAreaSize(math::Vec3(50.0f, 20.0f, 50.0f));
    scatter->SetScanAreaOffset(math::Vec3(0.0f, 10.0f, 0.0f));
    TEST_ASSERT(FloatEq(scatter->GetScanAreaSize().x, 50.0f) && FloatEq(scatter->GetScanAreaOffset().y, 10.0f),
                "CollisionScatterMultiMesh scan area round-trips");

    scatter->SetShowDebugShape(false);
    scatter->SetDebugColor(math::Color(0.0f, 1.0f, 1.0f, 0.6f));
    TEST_ASSERT(!scatter->GetShowDebugShape() &&
                ColorEq(scatter->GetDebugColor(), math::Color(0.0f, 1.0f, 1.0f, 0.6f)),
                "CollisionScatterMultiMesh debug settings round-trip");

    return true;
}

bool TestNodeScatter() {
    TEST_SECTION("NodeScatter: Node-Instance Scatter");

    std::shared_ptr<NodeScatter> scatter = std::make_shared<NodeScatter>("NodeScatter");
    scatter->DefineProperties();

    TEST_ASSERT(scatter->GetTypeName() == "NodeScatter", "NodeScatter reports its type name");
    TEST_ASSERT(scatter->GetScatteredInstances().empty(), "NodeScatter has no scattered instances initially");

    scatter->SetScatterMode(NodeScatterMode::Collision);
    TEST_ASSERT(scatter->GetScatterMode() == NodeScatterMode::Collision, "NodeScatter mode Collision round-trips");
    scatter->SetScatterMode(NodeScatterMode::Simple);
    TEST_ASSERT(scatter->GetScatterMode() == NodeScatterMode::Simple, "NodeScatter mode Simple round-trips");

    scatter->SetScatterLayers(0x07);
    scatter->SetCutoutLayers(0x08);
    TEST_ASSERT(scatter->GetScatterLayers() == 0x07u && scatter->GetCutoutLayers() == 0x08u,
                "NodeScatter collision layers round-trip");

    scatter->SetDistributionMode(NodeScatterDistribution::Grid);
    TEST_ASSERT(scatter->GetDistributionMode() == NodeScatterDistribution::Grid,
                "NodeScatter distribution Grid round-trips");
    scatter->SetDistributionMode(NodeScatterDistribution::Random);
    TEST_ASSERT(scatter->GetDistributionMode() == NodeScatterDistribution::Random,
                "NodeScatter distribution Random round-trips");

    scatter->SetInstanceCount(64);
    TEST_ASSERT(scatter->GetInstanceCount() == 64, "NodeScatter instance count round-trips");

    scatter->SetGridSpacingX(2.0f);
    scatter->SetGridSpacingZ(3.0f);
    scatter->SetGridJitter(0.25f);
    TEST_ASSERT(FloatEq(scatter->GetGridSpacingX(), 2.0f) && FloatEq(scatter->GetGridSpacingZ(), 3.0f) &&
                FloatEq(scatter->GetGridJitter(), 0.25f),
                "NodeScatter grid settings round-trip");

    scatter->SetScatterAreaSize(math::Vec3(40.0f, 5.0f, 40.0f));
    scatter->SetScatterAreaOffset(math::Vec3(1.0f, 0.0f, -1.0f));
    TEST_ASSERT(FloatEq(scatter->GetScatterAreaSize().x, 40.0f) &&
                FloatEq(scatter->GetScatterAreaOffset().z, -1.0f),
                "NodeScatter area settings round-trip");

    scatter->SetAlignToNormal(true);
    scatter->SetNormalAlignmentStrength(0.5f);
    scatter->SetSurfaceOffset(0.1f);
    TEST_ASSERT(scatter->GetAlignToNormal() && FloatEq(scatter->GetNormalAlignmentStrength(), 0.5f) &&
                FloatEq(scatter->GetSurfaceOffset(), 0.1f),
                "NodeScatter surface settings round-trip");

    scatter->SetMinSlope(10.0f);
    scatter->SetMaxSlope(60.0f);
    TEST_ASSERT(FloatEq(scatter->GetMinSlope(), 10.0f) && FloatEq(scatter->GetMaxSlope(), 60.0f),
                "NodeScatter slope filter round-trips");

    scatter->SetScaleRange(math::Vec2(0.9f, 1.1f));
    math::Vec2 range = scatter->GetScaleRange();
    TEST_ASSERT(FloatEq(range.x, 0.9f) && FloatEq(range.y, 1.1f), "NodeScatter scale range round-trips");
    scatter->SetUniformScale(true);
    TEST_ASSERT(scatter->GetUniformScale(), "NodeScatter uniform scale flag round-trips");

    scatter->SetRotationVariation(math::Vec3(0.0f, 45.0f, 0.0f));
    TEST_ASSERT(FloatEq(scatter->GetRotationVariation().y, 45.0f), "NodeScatter rotation variation round-trips");
    scatter->SetRandomYRotation(true);
    TEST_ASSERT(scatter->GetRandomYRotation(), "NodeScatter random-Y rotation flag round-trips");

    scatter->SetRandomSeed(1357);
    scatter->SetUseRandomSeed(false);
    TEST_ASSERT(scatter->GetRandomSeed() == 1357 && !scatter->GetUseRandomSeed(),
                "NodeScatter randomization settings round-trip");

    scatter->SetShowDebugShape(false);
    scatter->SetDebugColor(math::Color(0.8f, 0.8f, 0.0f, 0.7f));
    TEST_ASSERT(!scatter->GetShowDebugShape() &&
                ColorEq(scatter->GetDebugColor(), math::Color(0.8f, 0.8f, 0.0f, 0.7f)),
                "NodeScatter debug settings round-trip");

    scatter->SetInstancesPerFrame(16);
    TEST_ASSERT(scatter->GetInstancesPerFrame() == 16, "NodeScatter instances-per-frame round-trips");

    scatter->SetCollisionOptimization(ScatterCollisionMode::SimplifiedBox);
    TEST_ASSERT(scatter->GetCollisionOptimization() == ScatterCollisionMode::SimplifiedBox,
                "NodeScatter collision optimization SimplifiedBox round-trips");
    scatter->SetCollisionOptimization(ScatterCollisionMode::SimplifiedSphere);
    TEST_ASSERT(scatter->GetCollisionOptimization() == ScatterCollisionMode::SimplifiedSphere,
                "NodeScatter collision optimization SimplifiedSphere round-trips");
    scatter->SetCollisionOptimization(ScatterCollisionMode::None);
    TEST_ASSERT(scatter->GetCollisionOptimization() == ScatterCollisionMode::None,
                "NodeScatter collision optimization None round-trips");
    scatter->SetCollisionOptimization(ScatterCollisionMode::Full);
    TEST_ASSERT(scatter->GetCollisionOptimization() == ScatterCollisionMode::Full,
                "NodeScatter collision optimization Full round-trips");

    scatter->SetCollisionDistance(75.0f);
    TEST_ASSERT(FloatEq(scatter->GetCollisionDistance(), 75.0f), "NodeScatter collision distance round-trips");

    scatter->SetProgressiveLoading(true);
    TEST_ASSERT(scatter->GetProgressiveLoading(), "NodeScatter progressive loading flag round-trips");

    return true;
}

} // namespace

void RunMesh3DComponentTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "MESH3D COMPONENT TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    lupine_test::SetCurrentSuite("Mesh3DComponents");

    engine::InitializeEngine();

    bool allPassed = true;
    allPassed &= TestSprite3D();
    allPassed &= TestAnimatedSprite3D();
    allPassed &= TestPrimitiveMesh3D();
    allPassed &= TestStaticMesh3D();
    allPassed &= TestSkeletalMesh3D();
    allPassed &= TestMultiMeshGeneric();
    allPassed &= TestScatterMultiMesh();
    allPassed &= TestCollisionScatterMultiMesh();
    allPassed &= TestNodeScatter();

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "ALL MESH3D COMPONENT TESTS PASSED!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}
