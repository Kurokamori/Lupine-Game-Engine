#include "lupine/components/SkeletalMesh3D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/Mesh.hpp"
#include "lupine/rendering/TextureUpload.hpp"
#include "lupine/asset/ImageAsset.hpp"
#include "lupine/asset/AssetDatabase.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/platform/FileSystem.hpp"
#include "lupine/rendering/Rendering.hpp"
#include <algorithm>
#include <cmath>

namespace lupine {
namespace components {

using namespace asset;

/// Shortest clip length that can be advanced through. At or below this a clip is treated as a
/// static pose: its length is not a usable divisor, so wrapping or normalising against it would
/// produce a non-finite time.
static constexpr float kMinAnimationDuration = 1e-6f;

SkeletalMesh3D::SkeletalMesh3D()
    : Component("SkeletalMesh3D")
    , m_CurrentAnimationIndex(-1)
    , m_CurrentTime(0.0f)
    , m_PlaybackSpeed(1.0f)
    , m_Loop(true)
    , m_AutoPlay(false)
    , m_IsPlaying(false)
    , m_GPUSkinning(true)
    , m_RootMotionEnabled(false)
    , m_RootBoneIndex(-1)
    , m_LastRootPosition(0.0f, 0.0f, 0.0f)
    , m_LastRootRotation(0.0f, 0.0f, 0.0f, 1.0f)
    , m_CastShadow(true)
    , m_ReceiveShadow(true)
    , m_ShowSkeletonInEditor(false)
    , m_MeshesNeedUpload(false)
{
}

SkeletalMesh3D::SkeletalMesh3D(const std::string& name)
    : Component(name)
    , m_CurrentAnimationIndex(-1)
    , m_CurrentTime(0.0f)
    , m_PlaybackSpeed(1.0f)
    , m_Loop(true)
    , m_AutoPlay(false)
    , m_IsPlaying(false)
    , m_GPUSkinning(true)
    , m_RootMotionEnabled(false)
    , m_RootBoneIndex(-1)
    , m_LastRootPosition(0.0f, 0.0f, 0.0f)
    , m_LastRootRotation(0.0f, 0.0f, 0.0f, 1.0f)
    , m_CastShadow(true)
    , m_ReceiveShadow(true)
    , m_ShowSkeletonInEditor(false)
    , m_MeshesNeedUpload(false)
{
}

SkeletalMesh3D::~SkeletalMesh3D() {
}

void SkeletalMesh3D::DefineProperties() {
    Component::DefineProperties();

    DefineProperty(PROPERTY_FILE_GROUP(modelPath, std::string(""), "*.fbx,*.gltf,*.glb", "Model"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(defaultAnimation, String, std::string(""), "Animation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(currentAnimation, String, std::string(""), "Animation"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(playbackSpeed, 1.0f, 0.0f, 10.0f, 0.1f, "Animation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(loop, Bool, true, "Animation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(autoPlay, Bool, false, "Animation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(playing, Bool, false, "Animation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(gpuSkinning, Bool, true, "Animation"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(rootMotionEnabled, Bool, false, "Root Motion"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(rootBoneName, String, std::string(""), "Root Motion"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(castShadow, Bool, true, "Rendering"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(receiveShadow, Bool, true, "Rendering"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(showSkeletonInEditor, Bool, false, "Rendering"));
}

void SkeletalMesh3D::OnAwake() {
    Component::OnAwake();

    std::string modelPath = GetModelPath();

    if (!modelPath.empty()) {
        LoadModel(modelPath);
    }
}

void SkeletalMesh3D::OnReady() {
    Component::OnReady();

    if (GetAutoPlay()) {
        std::string defaultAnim = GetDefaultAnimation();
        if (!defaultAnim.empty()) {
            bool loop = GetPropertyValue<bool>("loop");
            PlayAnimation(defaultAnim, loop);
        }
    }
}

void SkeletalMesh3D::OnUpdate(float deltaTime) {
    Component::OnUpdate(deltaTime);

    if (m_IsPlaying) {
        UpdateAnimation(deltaTime);
    }
}

void SkeletalMesh3D::OnRender() {
    Component::OnRender();
}

void SkeletalMesh3D::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
    if (propertyName == "modelPath") {
        // Model path changed - reload the model
        std::string newPath = newValue.get<std::string>();
        if (!newPath.empty() && newPath != m_CurrentModelPath) {
            LoadModel(newPath);
        }
    } else if (propertyName == "currentAnimation") {
        // Current animation changed - play the new animation
        std::string newAnim = newValue.get<std::string>();
        if (!newAnim.empty() && GetPlaying()) {
            PlayAnimation(newAnim, GetLoop());
        }
    } else if (propertyName == "playing") {
        // Playing toggle changed
        bool playing = newValue.get<bool>();
        SetPlaying(playing);
    }
}

bool SkeletalMesh3D::LoadModel(const std::string& filepath) {
    if (filepath.empty()) {
        return false;
    }

    // Reset previous model data
    m_ModelAsset.Reset();
    m_MeshHandles.clear();
    m_MaterialSlots.clear();
    m_BoneTransforms.clear();
    m_BoneLocalTransforms.clear();

    m_ModelAsset = asset::AssetRef<asset::ModelAsset>(new asset::ModelAsset());

    if (!m_ModelAsset->LoadFromFile(filepath)) {
        m_ModelAsset.Reset();
        return false;
    }

    if (!m_ModelAsset->HasSkeleton()) {
        m_ModelAsset.Reset();
        return false;
    }

    m_CurrentModelPath = filepath;
    SetPropertyValue<std::string>("modelPath", filepath);

    CreateMaterialSlotsFromModel();

    if (m_ModelAsset->HasSkeleton()) {
        const auto& skeleton = m_ModelAsset->GetSkeleton();
        m_BoneTransforms.resize(skeleton.bones.size());
        m_BoneLocalTransforms.resize(skeleton.bones.size());

        for (size_t i = 0; i < skeleton.bones.size(); ++i) {
            m_BoneLocalTransforms[i] = skeleton.bones[i].localTransform;
        }

        // Calculate bind pose bone transforms (hierarchical)
        CalculateBindPose();

        if (!m_RootBoneName.empty()) {
            for (size_t i = 0; i < skeleton.bones.size(); ++i) {
                if (skeleton.bones[i].name == m_RootBoneName) {
                    m_RootBoneIndex = static_cast<int>(i);
                    break;
                }
            }
        }
    }

    m_MeshesNeedUpload = true;

    return true;
}

void SkeletalMesh3D::PlayAnimation(const std::string& animationName, bool loop) {
    if (!m_ModelAsset || !m_ModelAsset->HasSkeleton()) {
        return;
    }

    const auto& animations = m_ModelAsset->GetAnimations();

    int animIndex = -1;
    for (size_t i = 0; i < animations.size(); ++i) {
        if (animations[i].name == animationName) {
            animIndex = static_cast<int>(i);
            break;
        }
    }

    if (animIndex < 0) {
        return;
    }

    m_CurrentAnimationName = animationName;
    SetPropertyValue<std::string>("currentAnimation", animationName);
    m_CurrentAnimationIndex = animIndex;
    m_CurrentTime = 0.0f;
    m_Loop = loop;
    SetPropertyValue<bool>("loop", loop);
    m_IsPlaying = true;
}

void SkeletalMesh3D::StopAnimation() {
    m_IsPlaying = false;
    m_CurrentTime = 0.0f;
}

std::vector<std::string> SkeletalMesh3D::GetAnimationNames() const {
    std::vector<std::string> names;

    if (m_ModelAsset && m_ModelAsset->HasSkeleton()) {
        const auto& animations = m_ModelAsset->GetAnimations();
        names.reserve(animations.size());
        for (const auto& anim : animations) {
            names.push_back(anim.name);
        }
    }

    return names;
}

void SkeletalMesh3D::SetAnimationTime(float time) {
    // A non-finite time would survive every comparison below (NaN compares false against
    // everything) and reach the keyframe interpolators intact, so it is rejected outright.
    m_CurrentTime = std::isfinite(time) ? std::max(time, 0.0f) : 0.0f;

    if (m_CurrentAnimationIndex >= 0 && m_ModelAsset && m_ModelAsset->HasSkeleton()) {
        const auto& animations = m_ModelAsset->GetAnimations();
        if (m_CurrentAnimationIndex < static_cast<int>(animations.size())) {
            const auto& anim = animations[m_CurrentAnimationIndex];
            if (anim.duration <= kMinAnimationDuration) {
                m_CurrentTime = 0.0f;
            } else if (m_CurrentTime > anim.duration) {
                m_CurrentTime = anim.duration;
            }
        }
    }
}

std::string SkeletalMesh3D::GetModelPath() const {
    return GetPropertyValue<std::string>("modelPath");
}

void SkeletalMesh3D::SetModelPath(const std::string& path) {
    // Convert to res:// path if possible
    std::string resPath = path;
    if (!path.empty() && !(path.size() >= 6 && path.substr(0, 6) == "res://")) {
        auto& assetDb = AssetDatabase::GetInstance();
        if (assetDb.IsInitialized()) {
            std::string converted = assetDb.ToResourcePath(path);
            if (!converted.empty()) {
                resPath = converted;
            }
        }
    }

    std::string currentPath = GetModelPath();
    if (resPath != currentPath) {
        SetPropertyValue<std::string>("modelPath", resPath);
        LoadModel(resPath);
    }
}

std::string SkeletalMesh3D::GetDefaultAnimation() const {
    return GetPropertyValue<std::string>("defaultAnimation");
}

void SkeletalMesh3D::SetDefaultAnimation(const std::string& animName) {
    SetPropertyValue<std::string>("defaultAnimation", animName);
    m_DefaultAnimation = animName;
}

std::string SkeletalMesh3D::GetCurrentAnimation() const {
    return GetPropertyValue<std::string>("currentAnimation");
}

void SkeletalMesh3D::SetCurrentAnimation(const std::string& animName) {
    std::string currentAnim = GetCurrentAnimation();
    if (animName != currentAnim) {
        SetPropertyValue<std::string>("currentAnimation", animName);
        PlayAnimation(animName, GetLoop());
    }
}

float SkeletalMesh3D::GetPlaybackSpeed() const {
    return GetPropertyValue<float>("playbackSpeed");
}

void SkeletalMesh3D::SetPlaybackSpeed(float speed) {
    SetPropertyValue<float>("playbackSpeed", speed);
    m_PlaybackSpeed = speed;
}

bool SkeletalMesh3D::GetLoop() const {
    return GetPropertyValue<bool>("loop");
}

void SkeletalMesh3D::SetLoop(bool loop) {
    SetPropertyValue<bool>("loop", loop);
    m_Loop = loop;
}

bool SkeletalMesh3D::GetAutoPlay() const {
    return GetPropertyValue<bool>("autoPlay");
}

void SkeletalMesh3D::SetAutoPlay(bool autoPlay) {
    SetPropertyValue<bool>("autoPlay", autoPlay);
    m_AutoPlay = autoPlay;
}

bool SkeletalMesh3D::GetPlaying() const {
    return GetPropertyValue<bool>("playing");
}

void SkeletalMesh3D::SetPlaying(bool playing) {
    SetPropertyValue<bool>("playing", playing);

    if (playing && !m_IsPlaying) {
        // Start playing the current animation
        std::string currentAnim = GetCurrentAnimation();
        if (!currentAnim.empty()) {
            bool loop = GetLoop();
            PlayAnimation(currentAnim, loop);
        }
    } else if (!playing && m_IsPlaying) {
        // Stop playing
        StopAnimation();
    }
}

bool SkeletalMesh3D::GetGPUSkinning() const {
    return GetPropertyValue<bool>("gpuSkinning");
}

void SkeletalMesh3D::SetGPUSkinning(bool gpuSkinning) {
    SetPropertyValue<bool>("gpuSkinning", gpuSkinning);
    m_GPUSkinning = gpuSkinning;
}

bool SkeletalMesh3D::GetRootMotionEnabled() const {
    return GetPropertyValue<bool>("rootMotionEnabled");
}

void SkeletalMesh3D::SetRootMotionEnabled(bool enabled) {
    SetPropertyValue<bool>("rootMotionEnabled", enabled);
    m_RootMotionEnabled = enabled;
}

std::string SkeletalMesh3D::GetRootBoneName() const {
    return GetPropertyValue<std::string>("rootBoneName");
}

void SkeletalMesh3D::SetRootBoneName(const std::string& boneName) {
    SetPropertyValue<std::string>("rootBoneName", boneName);
    m_RootBoneName = boneName;

    if (m_ModelAsset && m_ModelAsset->HasSkeleton()) {
        const auto& skeleton = m_ModelAsset->GetSkeleton();
        m_RootBoneIndex = -1;
        for (size_t i = 0; i < skeleton.bones.size(); ++i) {
            if (skeleton.bones[i].name == boneName) {
                m_RootBoneIndex = static_cast<int>(i);
                break;
            }
        }
    }
}

bool SkeletalMesh3D::GetCastShadow() const {
    return GetPropertyValue<bool>("castShadow");
}

void SkeletalMesh3D::SetCastShadow(bool castShadow) {
    SetPropertyValue<bool>("castShadow", castShadow);
    m_CastShadow = castShadow;
}

bool SkeletalMesh3D::GetReceiveShadow() const {
    return GetPropertyValue<bool>("receiveShadow");
}

void SkeletalMesh3D::SetReceiveShadow(bool receiveShadow) {
    SetPropertyValue<bool>("receiveShadow", receiveShadow);
    m_ReceiveShadow = receiveShadow;
}

bool SkeletalMesh3D::GetShowSkeletonInEditor() const {
    return GetPropertyValue<bool>("showSkeletonInEditor");
}

void SkeletalMesh3D::SetShowSkeletonInEditor(bool show) {
    SetPropertyValue<bool>("showSkeletonInEditor", show);
    m_ShowSkeletonInEditor = show;
}

SkeletalMaterialSlot* SkeletalMesh3D::GetMaterialSlot(uint32_t index) {
    if (index >= m_MaterialSlots.size()) {
        return nullptr;
    }
    return &m_MaterialSlots[index];
}

const SkeletalMaterialSlot* SkeletalMesh3D::GetMaterialSlot(uint32_t index) const {
    if (index >= m_MaterialSlots.size()) {
        return nullptr;
    }
    return &m_MaterialSlots[index];
}

void SkeletalMesh3D::SetMaterialSlotOverrideEnabled(uint32_t slotIndex, bool enabled) {
    if (slotIndex < m_MaterialSlots.size()) {
        m_MaterialSlots[slotIndex].enableOverride = enabled;
    }
}

void SkeletalMesh3D::SetMaterialSlotAlbedoColor(uint32_t slotIndex, const Color& color) {
    if (slotIndex < m_MaterialSlots.size()) {
        m_MaterialSlots[slotIndex].albedoColor = color;
    }
}

void SkeletalMesh3D::SetMaterialSlotAlbedoTexture(uint32_t slotIndex, const std::string& texturePath) {
    if (slotIndex < m_MaterialSlots.size()) {
        m_MaterialSlots[slotIndex].albedoTexturePath = texturePath;
        m_MaterialSlots[slotIndex].texturesNeedUpload = true;
    }
}

void SkeletalMesh3D::UpdateAnimation(float deltaTime) {
    if (!m_ModelAsset || !m_ModelAsset->HasSkeleton()) {
        return;
    }

    if (m_CurrentAnimationIndex < 0) {
        return;
    }

    const auto& animations = m_ModelAsset->GetAnimations();
    if (m_CurrentAnimationIndex >= static_cast<int>(animations.size())) {
        return;
    }

    const auto& anim = animations[m_CurrentAnimationIndex];

    m_CurrentTime += deltaTime * m_PlaybackSpeed;

    if (anim.duration <= kMinAnimationDuration) {
        // A clip with no measurable length (a single-keyframe idle pose, which is what an
        // exporter emits for a static "animation") has nothing to advance through, so it is
        // held at its first keyframe. Wrapping it instead would evaluate fmod(t, 0), which is
        // NaN - and that NaN propagates into every bone transform, then into the skinned
        // vertices and the mesh's world bounds, silently poisoning the frame.
        m_CurrentTime = 0.0f;
        if (!m_Loop) {
            m_IsPlaying = false;
        }
    } else if (m_CurrentTime > anim.duration) {
        if (m_Loop) {
            m_CurrentTime = fmod(m_CurrentTime, anim.duration);
        } else {
            m_CurrentTime = anim.duration;
            m_IsPlaying = false;
        }
    }

    CalculateBoneTransforms();
}

void SkeletalMesh3D::CalculateBoneTransforms() {
    if (!m_ModelAsset || !m_ModelAsset->HasSkeleton()) {
        return;
    }

    if (m_CurrentAnimationIndex < 0) {
        return;
    }

    const auto& animations = m_ModelAsset->GetAnimations();
    if (m_CurrentAnimationIndex >= static_cast<int>(animations.size())) {
        return;
    }

    const auto& anim = animations[m_CurrentAnimationIndex];
    const auto& skeleton = m_ModelAsset->GetSkeleton();

    // Build a map of animation channels for quick lookup
    std::map<std::string, const AnimationChannel*> channelMap;
    for (const auto& channel : anim.channels) {
        channelMap[channel.boneName] = &channel;
    }

    // Update local transforms for each bone based on animation data
    for (size_t i = 0; i < skeleton.bones.size(); ++i) {
        const auto& bone = skeleton.bones[i];

        auto it = channelMap.find(bone.name);
        if (it != channelMap.end()) {
            // Bone has animation data - interpolate keyframes
            const auto& channel = *it->second;

            math::Vec3 position = InterpolatePosition(channel, m_CurrentTime);
            math::Quat rotation = InterpolateRotation(channel, m_CurrentTime);
            math::Vec3 scale = InterpolateScale(channel, m_CurrentTime);

            // Build transform matrix from TRS components
            math::Mat4 translationMat = math::Mat4::Translate(position);
            math::Mat4 rotationMat = rotation.ToMat4();
            math::Mat4 scaleMat = math::Mat4::Scale(scale);

            m_BoneLocalTransforms[i] = translationMat * rotationMat * scaleMat;
        } else {
            // No animation data for this bone - use bind pose
            m_BoneLocalTransforms[i] = bone.localTransform;
        }
    }

    // Calculate hierarchical animated world transforms
    std::vector<math::Mat4> animatedWorldTransforms(skeleton.bones.size());
    for (size_t i = 0; i < skeleton.bones.size(); ++i) {
        const auto& bone = skeleton.bones[i];
        if (bone.parentIndex < 0) {
            animatedWorldTransforms[i] = m_BoneLocalTransforms[i];
        } else {
            animatedWorldTransforms[i] = animatedWorldTransforms[bone.parentIndex] * m_BoneLocalTransforms[i];
        }
    }

    // Apply the skeletal animation formula:
    // finalTransform = animatedWorldTransform * offsetMatrix
    //
    // Note: We do NOT apply globalInverseTransform here because:
    // 1. The offsetMatrix already transforms vertices from mesh space to bone space
    // 2. The animatedWorldTransform brings them back to world space
    // 3. Applying globalInverseTransform would incorrectly rotate/transform the entire mesh
    //    based on the root node's transformation, which is not what we want
    for (size_t i = 0; i < skeleton.bones.size(); ++i) {
        const auto& bone = skeleton.bones[i];
        m_BoneTransforms[i] = animatedWorldTransforms[i] * bone.offsetMatrix;
    }
}

void SkeletalMesh3D::CalculateBindPose() {
    if (!m_ModelAsset || !m_ModelAsset->HasSkeleton()) {
        return;
    }

    const auto& skeleton = m_ModelAsset->GetSkeleton();

    // In bind pose, the bone transforms should be identity matrices
    // This is because the offsetMatrix already contains the inverse bind pose transform
    // and when the bones are in their bind pose positions, the formula:
    // globalInverseTransform * bindPoseWorldTransform * offsetMatrix
    // should equal identity (since offsetMatrix = inverse(bindPoseWorldTransform))
    //
    // However, to be safe and handle edge cases, we calculate it properly:
    std::vector<math::Mat4> bindPoseWorldTransforms(skeleton.bones.size());
    for (size_t i = 0; i < skeleton.bones.size(); ++i) {
        const auto& bone = skeleton.bones[i];
        if (bone.parentIndex < 0) {
            bindPoseWorldTransforms[i] = bone.localTransform;
        } else {
            bindPoseWorldTransforms[i] = bindPoseWorldTransforms[bone.parentIndex] * bone.localTransform;
        }
    }

    // Apply the skeletal animation formula (same as animated version)
    // In bind pose, this should result in identity or near-identity transforms
    for (size_t i = 0; i < skeleton.bones.size(); ++i) {
        const auto& bone = skeleton.bones[i];
        m_BoneTransforms[i] = bindPoseWorldTransforms[i] * bone.offsetMatrix;
    }

}

math::Vec3 SkeletalMesh3D::InterpolatePosition(const AnimationChannel& channel, float time) {
    if (channel.positionKeys.empty()) {
        return math::Vec3(0.0f, 0.0f, 0.0f);
    }

    if (channel.positionKeys.size() == 1) {
        return channel.positionKeys[0].value;
    }

    int keyIndex = FindPositionKeyframe(channel, time);
    int nextKeyIndex = keyIndex + 1;

    if (nextKeyIndex >= static_cast<int>(channel.positionKeys.size())) {
        return channel.positionKeys[keyIndex].value;
    }

    const auto& key = channel.positionKeys[keyIndex];
    const auto& nextKey = channel.positionKeys[nextKeyIndex];

    float deltaTime = nextKey.time - key.time;
    float factor = (time - key.time) / deltaTime;

    return key.value.Lerp(nextKey.value, factor);
}

math::Quat SkeletalMesh3D::InterpolateRotation(const AnimationChannel& channel, float time) {
    if (channel.rotationKeys.empty()) {
        return math::Quat(0.0f, 0.0f, 0.0f, 1.0f);
    }

    if (channel.rotationKeys.size() == 1) {
        return channel.rotationKeys[0].value;
    }

    int keyIndex = FindRotationKeyframe(channel, time);
    int nextKeyIndex = keyIndex + 1;

    if (nextKeyIndex >= static_cast<int>(channel.rotationKeys.size())) {
        return channel.rotationKeys[keyIndex].value;
    }

    const auto& key = channel.rotationKeys[keyIndex];
    const auto& nextKey = channel.rotationKeys[nextKeyIndex];

    float deltaTime = nextKey.time - key.time;
    float factor = (time - key.time) / deltaTime;

    return key.value.Slerp(nextKey.value, factor);
}

math::Vec3 SkeletalMesh3D::InterpolateScale(const AnimationChannel& channel, float time) {
    if (channel.scaleKeys.empty()) {
        return math::Vec3(1.0f, 1.0f, 1.0f);
    }

    if (channel.scaleKeys.size() == 1) {
        return channel.scaleKeys[0].value;
    }

    int keyIndex = FindScaleKeyframe(channel, time);
    int nextKeyIndex = keyIndex + 1;

    if (nextKeyIndex >= static_cast<int>(channel.scaleKeys.size())) {
        return channel.scaleKeys[keyIndex].value;
    }

    const auto& key = channel.scaleKeys[keyIndex];
    const auto& nextKey = channel.scaleKeys[nextKeyIndex];

    float deltaTime = nextKey.time - key.time;
    float factor = (time - key.time) / deltaTime;

    return key.value.Lerp(nextKey.value, factor);
}

int SkeletalMesh3D::FindPositionKeyframe(const AnimationChannel& channel, float time) {
    for (int i = 0; i < static_cast<int>(channel.positionKeys.size()) - 1; ++i) {
        if (time < channel.positionKeys[i + 1].time) {
            return i;
        }
    }
    return static_cast<int>(channel.positionKeys.size()) - 1;
}

int SkeletalMesh3D::FindRotationKeyframe(const AnimationChannel& channel, float time) {
    for (int i = 0; i < static_cast<int>(channel.rotationKeys.size()) - 1; ++i) {
        if (time < channel.rotationKeys[i + 1].time) {
            return i;
        }
    }
    return static_cast<int>(channel.rotationKeys.size()) - 1;
}

int SkeletalMesh3D::FindScaleKeyframe(const AnimationChannel& channel, float time) {
    for (int i = 0; i < static_cast<int>(channel.scaleKeys.size()) - 1; ++i) {
        if (time < channel.scaleKeys[i + 1].time) {
            return i;
        }
    }
    return static_cast<int>(channel.scaleKeys.size()) - 1;
}

void SkeletalMesh3D::CreateMaterialSlotsFromModel() {
    m_MaterialSlots.clear();

    if (!m_ModelAsset.IsValid()) {
        return;
    }

    const auto& materials = m_ModelAsset->GetMaterials();
    std::string modelDir = platform::Path::GetDirectory(m_ModelAsset->GetPath());

    for (uint32_t i = 0; i < materials.size(); ++i) {
        SkeletalMaterialSlot slot;
        slot.name = materials[i].name.empty() ? ("Material_" + std::to_string(i)) : materials[i].name;
        slot.materialIndex = i;
        slot.enableOverride = false;

        slot.albedoColor = Color(materials[i].albedo.x, materials[i].albedo.y, materials[i].albedo.z, materials[i].opacity);
        slot.metallic = materials[i].metallic;
        slot.roughness = materials[i].roughness;
        slot.emissiveColor = Color(materials[i].emissive.x, materials[i].emissive.y, materials[i].emissive.z, 1.0f);

        if (!materials[i].albedoMap.empty()) {
            slot.albedoTexturePath = ResolveTexturePath(materials[i].albedoMap, modelDir);
        }
        if (!materials[i].metallicMap.empty() || !materials[i].roughnessMap.empty()) {

            std::string mrPath = !materials[i].metallicMap.empty() ? materials[i].metallicMap : materials[i].roughnessMap;
            slot.metallicRoughnessTexturePath = ResolveTexturePath(mrPath, modelDir);
        }
        if (!materials[i].normalMap.empty()) {
            slot.normalTexturePath = ResolveTexturePath(materials[i].normalMap, modelDir);
        }
        if (!materials[i].emissiveMap.empty()) {
            slot.emissiveTexturePath = ResolveTexturePath(materials[i].emissiveMap, modelDir);
        }

        m_MaterialSlots.push_back(slot);
    }
}

std::string SkeletalMesh3D::ResolveTexturePath(const std::string& texturePath, const std::string& modelDir) {
    if (texturePath.empty()) {
        return "";
    }

    if (texturePath[0] == '*') {
        return texturePath;
    }

    if (platform::Path::IsAbsolute(texturePath)) {
        return texturePath;
    }

    return platform::Path::Join(modelDir, texturePath);
}

bool SkeletalMesh3D::LoadTexture(const std::string& filepath, asset::AssetRef<asset::ImageAsset>& outAsset) {
    if (filepath.empty()) {
        return false;
    }

    if (filepath[0] == '*' && m_ModelAsset.IsValid()) {
        return LoadEmbeddedTexture(filepath, outAsset);
    }

    outAsset = asset::AssetRef<asset::ImageAsset>(new asset::ImageAsset());

    asset::ImageColorSpace colorSpace = asset::ImageColorSpace::sRGB;
    std::string lowerPath = filepath;
    std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);

    if (lowerPath.find("normal") != std::string::npos ||
        lowerPath.find("metallic") != std::string::npos ||
        lowerPath.find("roughness") != std::string::npos) {
        colorSpace = asset::ImageColorSpace::Linear;
    }

    bool loaded = outAsset->LoadFromFile(filepath, true, colorSpace);

    if (!loaded) {
        outAsset.Reset();
        return false;
    }

    return true;
}

bool SkeletalMesh3D::LoadEmbeddedTexture(const std::string& textureName, asset::AssetRef<asset::ImageAsset>& outAsset) {
    if (!m_ModelAsset.IsValid()) {
        return false;
    }

    const asset::EmbeddedTexture* embeddedTex = m_ModelAsset->GetEmbeddedTexture(textureName);
    if (!embeddedTex) {
        return false;
    }

    outAsset = asset::AssetRef<asset::ImageAsset>(new asset::ImageAsset());

    asset::ImageColorSpace colorSpace = asset::ImageColorSpace::sRGB;
    std::string lowerName = textureName;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

    if (lowerName.find("normal") != std::string::npos ||
        lowerName.find("metallic") != std::string::npos ||
        lowerName.find("roughness") != std::string::npos) {
        colorSpace = asset::ImageColorSpace::Linear;
    }

    bool loaded = false;

    if (embeddedTex->compressed) {

        loaded = outAsset->LoadFromMemory(
            embeddedTex->data.data(),
            embeddedTex->data.size(),
            true,
            colorSpace
        );
    } else {

        loaded = outAsset->LoadFromRawData(
            embeddedTex->data.data(),
            embeddedTex->width,
            embeddedTex->height,
            4,
            true,
            colorSpace
        );
    }

    if (!loaded) {
        outAsset.Reset();
        return false;
    }

    return true;
}

void SkeletalMesh3D::UploadMeshesToGPU(RenderContext& ctx) {
    if (!m_MeshesNeedUpload || !m_ModelAsset.IsValid()) {
        return;
    }

    if (!m_MeshHandles.empty() && ctx.getDevice()) {
        for (auto& handle : m_MeshHandles) {
            if (handle.isValid()) {
                ctx.getDevice()->destroyMesh(handle);
            }
        }
        m_MeshHandles.clear();
    }

    const auto& meshes = m_ModelAsset->GetMeshes();
    m_MeshHandles.reserve(meshes.size());

    for (size_t i = 0; i < meshes.size(); ++i) {
        const auto& assetMesh = meshes[i];

        MeshData meshData;

        meshData.vertices.reserve(assetMesh.vertices.size());
        for (size_t vIdx = 0; vIdx < assetMesh.vertices.size(); ++vIdx) {
            const auto& assetVertex = assetMesh.vertices[vIdx];
            lupine::Vertex vertex;
            vertex.position = assetVertex.position;
            vertex.normal = assetVertex.normal;
            vertex.texCoord = assetVertex.texCoord;
            vertex.tangent = assetVertex.tangent;
            vertex.color = Vec4(1.0f, 1.0f, 1.0f, 1.0f);

            // Clamp bone IDs to >= 0: DX12 root CBVs page-fault on negative indices
            vertex.boneIDs = Vec4(
                static_cast<float>(std::max(0, assetVertex.boneIDs[0])),
                static_cast<float>(std::max(0, assetVertex.boneIDs[1])),
                static_cast<float>(std::max(0, assetVertex.boneIDs[2])),
                static_cast<float>(std::max(0, assetVertex.boneIDs[3]))
            );
            vertex.boneWeights = Vec4(
                assetVertex.boneWeights[0],
                assetVertex.boneWeights[1],
                assetVertex.boneWeights[2],
                assetVertex.boneWeights[3]
            );

            // Debug first vertex of first mesh
            if (i == 0 && vIdx == 0) {
            }

            meshData.vertices.push_back(vertex);
        }

        meshData.indices = assetMesh.indices;

        meshData.bounds = AABB(assetMesh.boundsMin, assetMesh.boundsMax);

        if (ctx.getDevice()) {
            MeshHandle handle = ctx.getDevice()->createMesh(meshData);
            m_MeshHandles.push_back(handle);
        }
    }

    m_MeshesNeedUpload = false;
}

void SkeletalMesh3D::UploadMeshesToGPU(IGfxDevice* device) {
    if (!m_MeshesNeedUpload || !m_ModelAsset.IsValid() || !device) {
        return;
    }

    if (!m_MeshHandles.empty()) {
        for (auto& handle : m_MeshHandles) {
            if (handle.isValid()) {
                device->destroyMesh(handle);
            }
        }
        m_MeshHandles.clear();
    }

    const auto& meshes = m_ModelAsset->GetMeshes();
    m_MeshHandles.reserve(meshes.size());

    for (size_t i = 0; i < meshes.size(); ++i) {
        const auto& assetMesh = meshes[i];

        MeshData meshData;

        meshData.vertices.reserve(assetMesh.vertices.size());
        for (size_t vIdx = 0; vIdx < assetMesh.vertices.size(); ++vIdx) {
            const auto& assetVertex = assetMesh.vertices[vIdx];
            lupine::Vertex vertex;
            vertex.position = assetVertex.position;
            vertex.normal = assetVertex.normal;
            vertex.texCoord = assetVertex.texCoord;
            vertex.tangent = assetVertex.tangent;
            vertex.color = Vec4(1.0f, 1.0f, 1.0f, 1.0f);

            // Clamp bone IDs to >= 0: DX12 root CBVs page-fault on negative indices
            vertex.boneIDs = Vec4(
                static_cast<float>(std::max(0, assetVertex.boneIDs[0])),
                static_cast<float>(std::max(0, assetVertex.boneIDs[1])),
                static_cast<float>(std::max(0, assetVertex.boneIDs[2])),
                static_cast<float>(std::max(0, assetVertex.boneIDs[3]))
            );
            vertex.boneWeights = Vec4(
                assetVertex.boneWeights[0],
                assetVertex.boneWeights[1],
                assetVertex.boneWeights[2],
                assetVertex.boneWeights[3]
            );

            meshData.vertices.push_back(vertex);
        }

        meshData.indices = assetMesh.indices;
        meshData.bounds = AABB(assetMesh.boundsMin, assetMesh.boundsMax);

        MeshHandle handle = device->createMesh(meshData);
        m_MeshHandles.push_back(handle);
    }

    m_MeshesNeedUpload = false;
}

void SkeletalMesh3D::prepareGPUResources(IGfxDevice* device) {
    if (!device) {
        return;
    }

    // Upload meshes to GPU if needed
    if (m_MeshesNeedUpload && m_ModelAsset.IsValid()) {
        UploadMeshesToGPU(device);
    }

    // Pre-load and upload textures for all material slots
    for (auto& slot : m_MaterialSlots) {
        // Albedo texture
        if (!slot.albedoTexturePath.empty() && !slot.albedoTextureHandle.isValid()) {
            if (!slot.albedoTextureAsset.IsValid()) {
                LoadTexture(slot.albedoTexturePath, slot.albedoTextureAsset);
            }
            if (slot.albedoTextureAsset.IsValid() && slot.albedoTextureAsset->IsLoaded()) {
                slot.albedoTextureHandle = lupine::CreateTexture2DFromImage(device, *slot.albedoTextureAsset, TextureFormat::RGBA8_SRGB);
            }
        }

        // Metallic roughness texture
        if (!slot.metallicRoughnessTexturePath.empty() && !slot.metallicRoughnessTextureHandle.isValid()) {
            if (!slot.metallicRoughnessTextureAsset.IsValid()) {
                LoadTexture(slot.metallicRoughnessTexturePath, slot.metallicRoughnessTextureAsset);
            }
            if (slot.metallicRoughnessTextureAsset.IsValid() && slot.metallicRoughnessTextureAsset->IsLoaded()) {
                slot.metallicRoughnessTextureHandle = lupine::CreateTexture2DFromImage(device, *slot.metallicRoughnessTextureAsset, TextureFormat::RGBA8_UNORM);
            }
        }

        // Normal texture
        if (!slot.normalTexturePath.empty() && !slot.normalTextureHandle.isValid()) {
            if (!slot.normalTextureAsset.IsValid()) {
                LoadTexture(slot.normalTexturePath, slot.normalTextureAsset);
            }
            if (slot.normalTextureAsset.IsValid() && slot.normalTextureAsset->IsLoaded()) {
                slot.normalTextureHandle = lupine::CreateTexture2DFromImage(device, *slot.normalTextureAsset, TextureFormat::RGBA8_UNORM);
            }
        }

        // Emissive texture
        if (!slot.emissiveTexturePath.empty() && !slot.emissiveTextureHandle.isValid()) {
            if (!slot.emissiveTextureAsset.IsValid()) {
                LoadTexture(slot.emissiveTexturePath, slot.emissiveTextureAsset);
            }
            if (slot.emissiveTextureAsset.IsValid() && slot.emissiveTextureAsset->IsLoaded()) {
                slot.emissiveTextureHandle = lupine::CreateTexture2DFromImage(device, *slot.emissiveTextureAsset, TextureFormat::RGBA8_SRGB);
            }
        }
    }
}

void SkeletalMesh3D::UploadMaterialSlotTextures(RenderContext& ctx, SkeletalMaterialSlot& slot) {
    if (!slot.texturesNeedUpload) {
        return;
    }

    IGfxDevice* device = ctx.getDevice();
    if (!device) {
        return;
    }

    if (!slot.albedoTexturePath.empty() && !slot.albedoTextureHandle.isValid()) {
        if (!slot.albedoTextureAsset.IsValid()) {

            LoadTexture(slot.albedoTexturePath, slot.albedoTextureAsset);
        }

        if (slot.albedoTextureAsset.IsValid() && slot.albedoTextureAsset->IsLoaded()) {
            const void* data = slot.albedoTextureAsset->GetData();
            size_t dataSize = slot.albedoTextureAsset->GetDataSize();
            uint32_t width = slot.albedoTextureAsset->GetWidth();
            uint32_t height = slot.albedoTextureAsset->GetHeight();
            uint32_t channels = slot.albedoTextureAsset->GetChannels();

            if (!data) {
            } else if (dataSize == 0) {
            } else if (width == 0 || height == 0) {
            } else {
                size_t expectedSize = width * height * channels;
                if (dataSize < expectedSize) {
                } else {
                    slot.albedoTextureHandle = CreateTexture2DFromImage(
                        device, *slot.albedoTextureAsset, TextureFormat::RGBA8_SRGB);
                }
            }
        }
    }

    if (!slot.metallicRoughnessTexturePath.empty() && !slot.metallicRoughnessTextureHandle.isValid()) {
        if (!slot.metallicRoughnessTextureAsset.IsValid()) {
            LoadTexture(slot.metallicRoughnessTexturePath, slot.metallicRoughnessTextureAsset);
        }

        if (slot.metallicRoughnessTextureAsset.IsValid() && slot.metallicRoughnessTextureAsset->IsLoaded()) {
            const void* data = slot.metallicRoughnessTextureAsset->GetData();
            size_t dataSize = slot.metallicRoughnessTextureAsset->GetDataSize();
            uint32_t width = slot.metallicRoughnessTextureAsset->GetWidth();
            uint32_t height = slot.metallicRoughnessTextureAsset->GetHeight();
            uint32_t channels = slot.metallicRoughnessTextureAsset->GetChannels();

            if (!data) {
            } else if (dataSize == 0) {
            } else if (width == 0 || height == 0) {
            } else {
                size_t expectedSize = width * height * channels;
                if (dataSize < expectedSize) {
                } else {
                    slot.metallicRoughnessTextureHandle = CreateTexture2DFromImage(
                        device, *slot.metallicRoughnessTextureAsset, TextureFormat::RGBA8_UNORM);
                }
            }
        }
    }

    if (!slot.normalTexturePath.empty() && !slot.normalTextureHandle.isValid()) {
        if (!slot.normalTextureAsset.IsValid()) {
            LoadTexture(slot.normalTexturePath, slot.normalTextureAsset);
        }

        if (slot.normalTextureAsset.IsValid() && slot.normalTextureAsset->IsLoaded()) {
            const void* data = slot.normalTextureAsset->GetData();
            size_t dataSize = slot.normalTextureAsset->GetDataSize();
            uint32_t width = slot.normalTextureAsset->GetWidth();
            uint32_t height = slot.normalTextureAsset->GetHeight();
            uint32_t channels = slot.normalTextureAsset->GetChannels();

            if (!data) {
            } else if (dataSize == 0) {
            } else if (width == 0 || height == 0) {
            } else {
                size_t expectedSize = width * height * channels;
                if (dataSize < expectedSize) {
                } else {
                    slot.normalTextureHandle = CreateTexture2DFromImage(
                        device, *slot.normalTextureAsset, TextureFormat::RGBA8_UNORM);
                }
            }
        }
    }

    if (!slot.emissiveTexturePath.empty() && !slot.emissiveTextureHandle.isValid()) {
        if (!slot.emissiveTextureAsset.IsValid()) {
            LoadTexture(slot.emissiveTexturePath, slot.emissiveTextureAsset);
        }

        if (slot.emissiveTextureAsset.IsValid() && slot.emissiveTextureAsset->IsLoaded()) {
            const void* data = slot.emissiveTextureAsset->GetData();
            size_t dataSize = slot.emissiveTextureAsset->GetDataSize();
            uint32_t width = slot.emissiveTextureAsset->GetWidth();
            uint32_t height = slot.emissiveTextureAsset->GetHeight();
            uint32_t channels = slot.emissiveTextureAsset->GetChannels();

            if (!data) {
            } else if (dataSize == 0) {
            } else if (width == 0 || height == 0) {
            } else {
                size_t expectedSize = width * height * channels;
                if (dataSize < expectedSize) {
                } else {
                    slot.emissiveTextureHandle = CreateTexture2DFromImage(
                        device, *slot.emissiveTextureAsset, TextureFormat::RGBA8_SRGB);
                }
            }
        }
    }

    slot.texturesNeedUpload = false;
}

void SkeletalMesh3D::buildDrawCommands(RenderContext& ctx) {
    if (!IsEnabled()) {
        return;
    }

    // Check if model path has changed (property edited in editor)
    std::string currentPath = GetModelPath();
    if (currentPath != m_CurrentModelPath) {

        // Clean up old resources
        if (!m_MeshHandles.empty()) {
            IGfxDevice* device = ctx.getDevice();
            if (device) {
                for (const auto& handle : m_MeshHandles) {
                    if (handle.isValid()) {
                        device->destroyMesh(handle);
                    }
                }
                m_MeshHandles.clear();
            }
        }

        m_ModelAsset.Reset();
        m_MaterialSlots.clear();

        // Load new model
        if (!currentPath.empty()) {
            m_ModelAsset = asset::AssetRef<asset::ModelAsset>(new asset::ModelAsset());
            bool loaded = m_ModelAsset->LoadFromFile(currentPath, true);

            if (!loaded) {
                m_ModelAsset.Reset();
            } else {
                m_MeshesNeedUpload = true;
                CreateMaterialSlotsFromModel();

                // Initialize skeleton data
                if (m_ModelAsset->HasSkeleton()) {
                    const auto& skeleton = m_ModelAsset->GetSkeleton();
                    m_BoneTransforms.resize(skeleton.bones.size());
                    m_BoneLocalTransforms.resize(skeleton.bones.size());

                    for (size_t i = 0; i < skeleton.bones.size(); ++i) {
                        m_BoneLocalTransforms[i] = skeleton.bones[i].localTransform;
                    }

                    CalculateBindPose();
                }

            }
        }

        m_CurrentModelPath = currentPath;
    }

    if (!m_ModelAsset.IsValid()) {
        return;
    }

    UploadMeshesToGPU(ctx);

    Node3D* node3D = dynamic_cast<Node3D*>(GetOwner());
    if (!node3D) {
        return;
    }

    Mat4 transform = node3D->GetGlobalTransformMatrix();

    const auto& meshes = m_ModelAsset->GetMeshes();
    const auto& materials = m_ModelAsset->GetMaterials();

    for (size_t meshIdx = 0; meshIdx < m_MeshHandles.size(); ++meshIdx) {
        const MeshHandle& meshHandle = m_MeshHandles[meshIdx];

        if (!meshHandle.isValid()) {
            
            continue;
        }

        const auto& assetMesh = meshes[meshIdx];

        uint32_t materialIndex = assetMesh.materialIndex;

        MaterialPropertyBlock overrides;
        bool useOverride = false;

        SkeletalMaterialSlot* slot = nullptr;
        for (auto& s : m_MaterialSlots) {
            if (s.materialIndex == materialIndex) {
                slot = &s;
                break;
            }
        }

        // Select material based on shader type
        MaterialHandle selectedMaterial;
        ShaderType shaderType = ShaderType::PBR;

        if (slot && slot->enableOverride) {
            shaderType = slot->shaderType;
        }

        // A custom .lsh shader takes precedence: translated at runtime with the skeletal
        // vertex layout; its optional #render_mode drives blend/cull/depth.
        const bool hasLsh = (slot && !slot->customLshShaderPath.empty());
        // Choose material based on shader type using the material registry
        // Handle custom shaders (supports single-file shaders like HLSL)
        if (hasLsh) {
            selectedMaterial = ctx.getOrCreateLshMaterial(
                slot->customLshShaderPath, 3 /*opaque default*/,
                LshMaterialLayout::SkeletalMesh3D);
        } else if (shaderType == ShaderType::Custom && slot &&
            (!slot->customVertShaderPath.empty() || !slot->customFragShaderPath.empty())) {
            selectedMaterial = ctx.getOrCreateCustomMaterial(
                slot->customVertShaderPath,
                slot->customFragShaderPath,
                true  // skeletal
            );
        } else {
            selectedMaterial = ctx.getMaterial(shaderType, true);  // true = skeletal
        }

        if (!selectedMaterial.isValid()) {
            continue;
        }

        if (slot && (slot->enableOverride || hasLsh)) {
            useOverride = true;

            UploadMaterialSlotTextures(ctx, *slot);

            overrides.setColor("u_AlbedoColor", slot->albedoColor);
            overrides.setColor("u_EmissiveColor", slot->emissiveColor);
            overrides.setColor("u_TintColor", Color::White());

            // Set ALL material uniforms - shaders use what they need and ignore the rest
            // This approach supports custom shaders without requiring per-shader-type code paths

            // PBR uniforms: u_MaterialParams1 = metallic, roughness, normalScale, emissiveStrength
            overrides.setVec4("u_MaterialParams1", Vec4(slot->metallic, slot->roughness, slot->normalScale, slot->emissiveStrength));
            // PBR uniforms: u_MaterialParams2 = alphaCutoff, aoStrength, heightScale, unused
            overrides.setVec4("u_MaterialParams2", Vec4(slot->alphaCutoff, 1.0f, 1.0f, 0.0f));

            // Toon uniforms: u_ToonMaterialParams = shadowBands, specularBands, normalScale, emissiveStrength
            overrides.setVec4("u_ToonMaterialParams", Vec4(slot->shadowBands, slot->specularBands, slot->normalScale, slot->emissiveStrength));
            // Toon uniforms: u_ToonMaterialParams2 = alphaCutoff, rimPower, rimIntensity, specularPower
            overrides.setVec4("u_ToonMaterialParams2", Vec4(slot->alphaCutoff, slot->rimPower, slot->rimIntensity, slot->specularPower));
            // Toon uniforms: u_ToonParams = shadowThreshold, shadowSoftness, specularThreshold, specularSoftness
            overrides.setVec4("u_ToonParams", Vec4(slot->shadowThreshold, slot->shadowSoftness, slot->shadowThreshold, slot->shadowSoftness));

            // Stylized uniforms: u_MaterialParams1 = shadowSoftness, specularSoftness, normalScale, emissiveStrength
            if (shaderType == ShaderType::Stylized) {
                overrides.setVec4("u_MaterialParams1", Vec4(slot->stylizedShadowSoftness, slot->stylizedSpecularSoftness, slot->normalScale, slot->emissiveStrength));
                // u_MaterialParams2 = alphaCutoff, rimPower, rimIntensity, specularPower (shared with toon)
                overrides.setVec4("u_MaterialParams2", Vec4(slot->alphaCutoff, slot->rimPower, slot->rimIntensity, slot->specularPower));
                // u_StylizedParams = shadowBrightness, shadowWarmth, specularIntensity, halfLambertPower
                overrides.setVec4("u_StylizedParams", Vec4(slot->stylizedShadowBrightness, slot->stylizedShadowWarmth, slot->stylizedSpecularIntensity, slot->stylizedHalfLambertPower));
            }

            // Apply generic shader parameters from shaderParams map
            // This allows custom shaders to receive their parameters without C++ code changes
            for (const auto& [uniformName, value] : slot->shaderParams) {
                std::visit([&overrides, &uniformName](auto&& arg) {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, float>) {
                        overrides.setFloat(uniformName, arg);
                    } else if constexpr (std::is_same_v<T, int>) {
                        overrides.setInt(uniformName, arg);
                    } else if constexpr (std::is_same_v<T, bool>) {
                        overrides.setBool(uniformName, arg);
                    } else if constexpr (std::is_same_v<T, Vec2>) {
                        overrides.setVec2(uniformName, arg);
                    } else if constexpr (std::is_same_v<T, Vec3>) {
                        overrides.setVec3(uniformName, arg);
                    } else if constexpr (std::is_same_v<T, Vec4>) {
                        overrides.setVec4(uniformName, arg);
                    } else if constexpr (std::is_same_v<T, Color>) {
                        overrides.setColor(uniformName, arg);
                    } else if constexpr (std::is_same_v<T, TextureHandle>) {
                        overrides.setTexture(uniformName, arg);
                    }
                }, value);
            }

            Vec4 textureFlags(
                slot->albedoTextureHandle.isValid() ? 1.0f : 0.0f,
                slot->metallicRoughnessTextureHandle.isValid() ? 1.0f : 0.0f,
                slot->normalTextureHandle.isValid() ? 1.0f : 0.0f,
                slot->emissiveTextureHandle.isValid() ? 1.0f : 0.0f
            );
            overrides.setVec4("u_TextureFlags", textureFlags);

            // Always bind textures (valid or not) to prevent stale texture bleeding
            overrides.setTexture("u_AlbedoTexture", slot->albedoTextureHandle);
            overrides.setTexture("u_MetallicRoughnessTexture", slot->metallicRoughnessTextureHandle);
            overrides.setTexture("u_NormalTexture", slot->normalTextureHandle);
            overrides.setTexture("u_EmissiveTexture", slot->emissiveTextureHandle);

        } else if (materialIndex < materials.size() && slot) {
            const auto& material = materials[materialIndex];

            overrides.setColor("u_AlbedoColor", Color(material.albedo.x, material.albedo.y, material.albedo.z, material.opacity));
            overrides.setColor("u_EmissiveColor", Color(material.emissive.x, material.emissive.y, material.emissive.z, 1.0f));
            overrides.setColor("u_TintColor", Color::White());

            // Set ALL material uniforms - shaders use what they need and ignore the rest
            // This approach supports custom shaders without requiring per-shader-type code paths

            // PBR uniforms: u_MaterialParams1 = metallic, roughness, normalScale, emissiveStrength
            overrides.setVec4("u_MaterialParams1", Vec4(material.metallic, material.roughness, 1.0f, 1.0f));
            // PBR uniforms: u_MaterialParams2 = alphaCutoff, aoStrength, heightScale, unused
            overrides.setVec4("u_MaterialParams2", Vec4(0.5f, 1.0f, 1.0f, 0.0f));

            // Toon uniforms: u_ToonMaterialParams = shadowBands, specularBands, normalScale, emissiveStrength
            overrides.setVec4("u_ToonMaterialParams", Vec4(slot->shadowBands, slot->specularBands, 1.0f, 1.0f));
            // Toon uniforms: u_ToonMaterialParams2 = alphaCutoff, rimPower, rimIntensity, specularPower
            overrides.setVec4("u_ToonMaterialParams2", Vec4(0.5f, slot->rimPower, slot->rimIntensity, slot->specularPower));
            // Toon uniforms: u_ToonParams = shadowThreshold, shadowSoftness, specularThreshold, specularSoftness
            overrides.setVec4("u_ToonParams", Vec4(slot->shadowThreshold, slot->shadowSoftness, slot->shadowThreshold, slot->shadowSoftness));

            // Apply generic shader parameters from shaderParams map
            for (const auto& [uniformName, value] : slot->shaderParams) {
                std::visit([&overrides, &uniformName](auto&& arg) {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, float>) {
                        overrides.setFloat(uniformName, arg);
                    } else if constexpr (std::is_same_v<T, int>) {
                        overrides.setInt(uniformName, arg);
                    } else if constexpr (std::is_same_v<T, bool>) {
                        overrides.setBool(uniformName, arg);
                    } else if constexpr (std::is_same_v<T, Vec2>) {
                        overrides.setVec2(uniformName, arg);
                    } else if constexpr (std::is_same_v<T, Vec3>) {
                        overrides.setVec3(uniformName, arg);
                    } else if constexpr (std::is_same_v<T, Vec4>) {
                        overrides.setVec4(uniformName, arg);
                    } else if constexpr (std::is_same_v<T, Color>) {
                        overrides.setColor(uniformName, arg);
                    } else if constexpr (std::is_same_v<T, TextureHandle>) {
                        overrides.setTexture(uniformName, arg);
                    }
                }, value);
            }

            LoadAndUploadMaterialTextures(ctx, *slot);

            Vec4 textureFlags(
                slot->albedoTextureHandle.isValid() ? 1.0f : 0.0f,
                slot->metallicRoughnessTextureHandle.isValid() ? 1.0f : 0.0f,
                slot->normalTextureHandle.isValid() ? 1.0f : 0.0f,
                slot->emissiveTextureHandle.isValid() ? 1.0f : 0.0f
            );
            overrides.setVec4("u_TextureFlags", textureFlags);

            // Always bind textures (valid or not) to prevent stale texture bleeding
            overrides.setTexture("u_AlbedoTexture", slot->albedoTextureHandle);
            overrides.setTexture("u_MetallicRoughnessTexture", slot->metallicRoughnessTextureHandle);
            overrides.setTexture("u_NormalTexture", slot->normalTextureHandle);
            overrides.setTexture("u_EmissiveTexture", slot->emissiveTextureHandle);
        }

        if (m_GPUSkinning && m_ModelAsset->HasSkeleton()) {
            overrides.setBool("u_UseSkinning", true);

            if (!m_BoneTransforms.empty()) {
                overrides.setMat4Array("u_BoneTransforms", m_BoneTransforms.data(), m_BoneTransforms.size());
            }
        } else {
            overrides.setBool("u_UseSkinning", false);
        }

        // Pass shadow flags to shader
        overrides.setBool("u_ReceiveShadow", m_ReceiveShadow);

        ctx.drawMesh(meshHandle, selectedMaterial, transform, overrides, static_cast<uint32_t>(meshIdx), m_CastShadow, m_ReceiveShadow);
    }
}

void SkeletalMesh3D::LoadAndUploadMaterialTextures(RenderContext& ctx, SkeletalMaterialSlot& slot) {
    if (!slot.albedoTexturePath.empty() && !slot.albedoTextureAsset.IsValid()) {
        LoadTexture(slot.albedoTexturePath, slot.albedoTextureAsset);
    }

    if (!slot.metallicRoughnessTexturePath.empty() && !slot.metallicRoughnessTextureAsset.IsValid()) {
        LoadTexture(slot.metallicRoughnessTexturePath, slot.metallicRoughnessTextureAsset);
    }

    if (!slot.emissiveTexturePath.empty() && !slot.emissiveTextureAsset.IsValid()) {
        LoadTexture(slot.emissiveTexturePath, slot.emissiveTextureAsset);
    }
        
    slot.texturesNeedUpload = true;

    UploadMaterialSlotTextures(ctx, slot);
}

AABB SkeletalMesh3D::getWorldBounds() const {

    AABB localBounds = CalculateCombinedBounds();

    if (!m_Owner) {
        return localBounds;
    }

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) {
        return localBounds;
    }

    Mat4 worldTransform = node3D->GetGlobalTransformMatrix();
    return localBounds.Transform(worldTransform);
}

AABB SkeletalMesh3D::CalculateCombinedBounds() const {
    // Default bounds to use when model data is unavailable or invalid
    const AABB defaultBounds(Vec3(-0.5f, -0.5f, -0.5f), Vec3(0.5f, 0.5f, 0.5f));

    if (!m_ModelAsset.IsValid()) {
        return defaultBounds;
    }

    const auto& meshes = m_ModelAsset->GetMeshes();
    if (meshes.empty()) {
        return defaultBounds;
    }

    // Start with mesh bounds (bind pose)
    AABB combined;
    bool hasValidBounds = false;

    for (size_t i = 0; i < meshes.size(); ++i) {
        Vec3 boundsMin = meshes[i].boundsMin;
        Vec3 boundsMax = meshes[i].boundsMax;

        // Skip meshes with inverted bounds (uninitialized - happens when mesh has no vertices)
        if (boundsMin.x > boundsMax.x || boundsMin.y > boundsMax.y || boundsMin.z > boundsMax.z) {
            continue;
        }

        AABB meshBounds(boundsMin, boundsMax);

        if (!hasValidBounds) {
            combined = meshBounds;
            hasValidBounds = true;
        } else {
            combined = AABB::Merge(combined, meshBounds);
        }
    }

    // If no valid mesh bounds found, return default
    if (!hasValidBounds) {
        return defaultBounds;
    }

    // For skeletal meshes, we need to account for bone transforms
    // The mesh bounds are in bind pose, but the actual rendered mesh is transformed by bones
    // Expand bounds to include all bone positions from current animation pose
    if (m_ModelAsset->HasSkeleton() && !m_BoneTransforms.empty()) {
        const auto& skeleton = m_ModelAsset->GetSkeleton();

        for (size_t i = 0; i < m_BoneTransforms.size() && i < skeleton.bones.size(); ++i) {
            // Extract bone position from the transform matrix (translation component)
            const Mat4& boneTransform = m_BoneTransforms[i];
            Vec3 bonePos(boneTransform[3][0], boneTransform[3][1], boneTransform[3][2]);

            // std::min/std::max propagate a NaN operand rather than rejecting it, so a single
            // non-finite bone would turn these bounds NaN - and a NaN AABB does not merely hide
            // this mesh, it poisons whatever scene-wide culling and shadow fitting consumes it,
            // blanking the entire frame. A bone that cannot be placed simply does not expand.
            if (!std::isfinite(bonePos.x) || !std::isfinite(bonePos.y) || !std::isfinite(bonePos.z)) {
                continue;
            }

            // Expand bounds to include this bone position
            combined.min.x = std::min(combined.min.x, bonePos.x);
            combined.min.y = std::min(combined.min.y, bonePos.y);
            combined.min.z = std::min(combined.min.z, bonePos.z);
            combined.max.x = std::max(combined.max.x, bonePos.x);
            combined.max.y = std::max(combined.max.y, bonePos.y);
            combined.max.z = std::max(combined.max.z, bonePos.z);
        }

        // Add some padding around bone positions to account for mesh geometry around bones
        Vec3 size = combined.GetSize();
        Vec3 padding = size * 0.2f; // 20% padding
        padding.x = std::max(padding.x, 0.1f);
        padding.y = std::max(padding.y, 0.1f);
        padding.z = std::max(padding.z, 0.1f);
        combined.min = combined.min - padding;
        combined.max = combined.max + padding;
    }

    // Check if combined bounds have any size (non-zero extents)
    Vec3 size = combined.GetSize();
    if (size.x <= 0.0f && size.y <= 0.0f && size.z <= 0.0f) {
        // Zero-sized bounds - create a small bounds around the center
        Vec3 center = combined.GetCenter();
        return AABB(center - Vec3(0.5f, 0.5f, 0.5f), center + Vec3(0.5f, 0.5f, 0.5f));
    }

    return combined;
}

RenderLayer SkeletalMesh3D::getRenderLayer() const {
    return RenderLayer::Opaque;
}

bool SkeletalMesh3D::IntersectRay(const math::Ray& ray, float& outDistance) const {
    if (!m_ModelAsset.IsValid()) {

        return IRenderableComponent::IntersectRay(ray, outDistance);
    }

    if (!m_Owner) {
        return false;
    }

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) {
        return false;
    }

    AABB worldBounds = getWorldBounds();
    return worldBounds.IntersectRay(ray, outDistance);
}

math::OBB SkeletalMesh3D::getOrientedBounds() const {
    AABB localBounds = CalculateCombinedBounds();

    // Ensure we have valid bounds with non-zero extents
    Vec3 localHalfExtents = localBounds.GetExtents();
    if (localHalfExtents.x <= 0.0f && localHalfExtents.y <= 0.0f && localHalfExtents.z <= 0.0f) {
        // Invalid bounds - use default 1x1x1 centered at origin
        localBounds = AABB(Vec3(-0.5f, -0.5f, -0.5f), Vec3(0.5f, 0.5f, 0.5f));
        localHalfExtents = localBounds.GetExtents();
    }

    if (!m_Owner) {
        return math::OBB::FromAABB(localBounds);
    }

    Node3D* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) {
        return math::OBB::FromAABB(localBounds);
    }

    Vec3 worldPos = node3D->GetGlobalPosition();
    Quat worldRot = node3D->GetGlobalRotation();
    Vec3 worldScale = node3D->GetGlobalScale();

    Vec3 localCenter = localBounds.GetCenter();

    Vec3 scaledCenter(
        localCenter.x * worldScale.x,
        localCenter.y * worldScale.y,
        localCenter.z * worldScale.z
    );
    Vec3 worldCenter = worldPos + worldRot * scaledCenter;

    Vec3 scaledHalfExtents(
        localHalfExtents.x * std::abs(worldScale.x),
        localHalfExtents.y * std::abs(worldScale.y),
        localHalfExtents.z * std::abs(worldScale.z)
    );

    return math::OBB(worldCenter, scaledHalfExtents, worldRot);
}

bool SkeletalMesh3D::OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) {
    auto& assetDb = asset::AssetDatabase::GetInstance();
    bool anyChanged = false;

    // Check if this is our model
    std::string currentModelPath = GetModelPath();
    if (!currentModelPath.empty()) {
        std::string resolvedModelPath;
        if (assetDb.IsInitialized()) {
            resolvedModelPath = assetDb.ResolveAsset(currentModelPath);
        }

        bool modelMatches = (currentModelPath == changedPath) ||
                       (!resolvedModelPath.empty() && !resolvedChangedPath.empty() &&
                        resolvedModelPath == resolvedChangedPath);

        if (modelMatches) {
            
            m_MeshHandles.clear();
            m_ModelAsset.Reset();
            m_CurrentModelPath.clear();
            m_MeshesNeedUpload = true;
            m_MaterialSlots.clear();
            return true;
        }
    }

    // Check if this is one of our material slot textures
    for (auto& slot : m_MaterialSlots) {
        auto checkTexture = [&](const std::string& texPath, TextureHandle& handle,
                                asset::AssetRef<asset::ImageAsset>& asset) -> bool {
            if (texPath.empty()) return false;
            std::string resolvedTexPath;
            if (assetDb.IsInitialized()) {
                resolvedTexPath = assetDb.ResolveAsset(texPath);
            }
            bool matches = (texPath == changedPath) ||
                           (!resolvedTexPath.empty() && !resolvedChangedPath.empty() &&
                            resolvedTexPath == resolvedChangedPath);
            if (matches) {
                
                handle = TextureHandle();
                asset.Reset();
                slot.texturesNeedUpload = true;
                return true;
            }
            return false;
        };

        if (checkTexture(slot.albedoTexturePath, slot.albedoTextureHandle, slot.albedoTextureAsset)) anyChanged = true;
        if (checkTexture(slot.metallicRoughnessTexturePath, slot.metallicRoughnessTextureHandle, slot.metallicRoughnessTextureAsset)) anyChanged = true;
        if (checkTexture(slot.normalTexturePath, slot.normalTextureHandle, slot.normalTextureAsset)) anyChanged = true;
        if (checkTexture(slot.emissiveTexturePath, slot.emissiveTextureHandle, slot.emissiveTextureAsset)) anyChanged = true;
    }

    return anyChanged;
}

}
}

