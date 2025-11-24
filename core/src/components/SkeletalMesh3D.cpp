#include "lupine/components/SkeletalMesh3D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/Mesh.hpp"
#include "lupine/asset/ImageAsset.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/platform/FileSystem.hpp"
#include "lupine/rendering/Rendering.hpp"
#include <algorithm>

namespace lupine {
namespace components {

using namespace asset;

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
    LOG_INFO(LogCategory::Core, "SkeletalMesh3D::SkeletalMesh3D() constructor called");
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

    LOG_INFO(LogCategory::Core, "SkeletalMesh3D::DefineProperties() called");

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

    LOG_INFO(LogCategory::Core, "SkeletalMesh3D::OnAwake() called, owner={}, enabled={}",
             (void*)GetOwner(), IsEnabled());

    std::string modelPath = GetModelPath();
    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Model path = '{}'", modelPath);

    if (!modelPath.empty()) {
        LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Attempting to load model...");
        if (!LoadModel(modelPath)) {
            LOG_WARN(LogCategory::Core, "SkeletalMesh3D: Failed to load model in OnAwake: {}", modelPath);
        } else {
            LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Model loaded successfully, meshes need upload={}", m_MeshesNeedUpload);
        }
    } else {
        LOG_WARN(LogCategory::Core, "SkeletalMesh3D: Model path is empty in OnAwake!");
    }
}

void SkeletalMesh3D::OnReady() {
    Component::OnReady();

    LOG_INFO(LogCategory::Core, "SkeletalMesh3D::OnReady() called, enabled={}, model valid={}",
             IsEnabled(), m_ModelAsset.IsValid());

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
        LOG_WARN(LogCategory::Core, "SkeletalMesh3D: Cannot load model - empty filepath");
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
        LOG_ERROR(LogCategory::Core, "SkeletalMesh3D: Failed to load model from: {}", filepath);
        m_ModelAsset.Reset();
        return false;
    }

    if (!m_ModelAsset->HasSkeleton()) {
        LOG_ERROR(LogCategory::Core, "SkeletalMesh3D: Model does not have skeleton (use StaticMesh3D instead): {}", filepath);
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

    if (m_ModelAsset->HasSkeleton()) {
        const auto& animations = m_ModelAsset->GetAnimations();
        if (!animations.empty()) {
            for (const auto& anim : animations) {
            }
        }
    }

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
    m_CurrentTime = time;

    if (m_CurrentAnimationIndex >= 0 && m_ModelAsset && m_ModelAsset->HasSkeleton()) {
        const auto& animations = m_ModelAsset->GetAnimations();
        if (m_CurrentAnimationIndex < static_cast<int>(animations.size())) {
            const auto& anim = animations[m_CurrentAnimationIndex];
            if (m_CurrentTime > anim.duration) {
                m_CurrentTime = anim.duration;
            }
        }
    }
}

std::string SkeletalMesh3D::GetModelPath() const {
    return GetPropertyValue<std::string>("modelPath");
}

void SkeletalMesh3D::SetModelPath(const std::string& path) {
    std::string currentPath = GetModelPath();
    if (path != currentPath) {
        SetPropertyValue<std::string>("modelPath", path);
        LoadModel(path);
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

    if (m_CurrentTime > anim.duration) {
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

    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Calculating bind pose for {} bones", skeleton.bones.size());

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

    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Bind pose calculated");
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
    LOG_INFO(LogCategory::Core, "SkeletalMesh3D::UploadMeshesToGPU() called, needsUpload={}", m_MeshesNeedUpload);

    if (!m_MeshesNeedUpload || !m_ModelAsset.IsValid()) {
        return;
    }

    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Starting mesh upload...");

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

    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Processing {} meshes", meshes.size());

    for (size_t i = 0; i < meshes.size(); ++i) {
        const auto& assetMesh = meshes[i];
        LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Processing mesh {}/{} with {} vertices", i+1, meshes.size(), assetMesh.vertices.size());

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

            vertex.boneIDs = Vec4(
                static_cast<float>(assetVertex.boneIDs[0]),
                static_cast<float>(assetVertex.boneIDs[1]),
                static_cast<float>(assetVertex.boneIDs[2]),
                static_cast<float>(assetVertex.boneIDs[3])
            );
            vertex.boneWeights = Vec4(
                assetVertex.boneWeights[0],
                assetVertex.boneWeights[1],
                assetVertex.boneWeights[2],
                assetVertex.boneWeights[3]
            );

            // Debug first vertex of first mesh
            if (i == 0 && vIdx == 0) {
                LOG_INFO(LogCategory::Core, "SkeletalMesh3D: First vertex - pos: ({}, {}, {}), boneIDs: ({}, {}, {}, {}), weights: ({}, {}, {}, {})",
                    vertex.position.x, vertex.position.y, vertex.position.z,
                    assetVertex.boneIDs[0], assetVertex.boneIDs[1], assetVertex.boneIDs[2], assetVertex.boneIDs[3],
                    vertex.boneWeights.x, vertex.boneWeights.y, vertex.boneWeights.z, vertex.boneWeights.w);
            }

            meshData.vertices.push_back(vertex);
        }

        meshData.indices = assetMesh.indices;

        meshData.bounds = AABB(assetMesh.boundsMin, assetMesh.boundsMax);

        LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Creating GPU mesh for mesh {}", i+1);
        if (ctx.getDevice()) {
            MeshHandle handle = ctx.getDevice()->createMesh(meshData);
            m_MeshHandles.push_back(handle);
            LOG_INFO(LogCategory::Core, "SkeletalMesh3D: GPU mesh created, handle valid={}", handle.isValid());
        }
    }

    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: All meshes uploaded successfully");
    m_MeshesNeedUpload = false;
}

void SkeletalMesh3D::UploadMaterialSlotTextures(RenderContext& ctx, SkeletalMaterialSlot& slot) {
    LOG_INFO(LogCategory::Core, "SkeletalMesh3D::UploadMaterialSlotTextures() called for material index {}", slot.materialIndex);

    if (!slot.texturesNeedUpload) {
        LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Textures don't need upload, returning");
        return;
    }

    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Getting device...");
    IGfxDevice* device = ctx.getDevice();
    if (!device) {
        LOG_ERROR(LogCategory::Core, "SkeletalMesh3D: Device is null!");
        return;
    }

    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Uploading albedo texture...");
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
                LOG_ERROR(LogCategory::Core, "SkeletalMesh3D: Albedo texture data is null!");
            } else if (dataSize == 0) {
                LOG_ERROR(LogCategory::Core, "SkeletalMesh3D: Albedo texture data size is 0!");
            } else if (width == 0 || height == 0) {
                LOG_ERROR(LogCategory::Core, "SkeletalMesh3D: Albedo texture has invalid dimensions: {}x{}", width, height);
            } else {
                size_t expectedSize = width * height * channels;
                if (dataSize < expectedSize) {
                    LOG_ERROR(LogCategory::Core, "SkeletalMesh3D: Albedo texture data size mismatch! Expected at least {}, got {}", expectedSize, dataSize);
                } else {
                    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Creating albedo texture on GPU ({}x{}, {} channels, {} bytes), data ptr={}",
                        width, height, channels, dataSize, (void*)data);
                    TextureDesc desc;
                    desc.width = width;
                    desc.height = height;
                    desc.format = TextureFormat::RGBA8_SRGB;
                    desc.usage = TextureUsage::Sampled;
                    desc.initialData = data;
                    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Calling device->createTexture()...");
                    slot.albedoTextureHandle = device->createTexture(desc);
                    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Albedo texture created, handle valid={}", slot.albedoTextureHandle.isValid());
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
                LOG_ERROR(LogCategory::Core, "SkeletalMesh3D: Metallic/Roughness texture data is null!");
            } else if (dataSize == 0) {
                LOG_ERROR(LogCategory::Core, "SkeletalMesh3D: Metallic/Roughness texture data size is 0!");
            } else if (width == 0 || height == 0) {
                LOG_ERROR(LogCategory::Core, "SkeletalMesh3D: Metallic/Roughness texture has invalid dimensions: {}x{}", width, height);
            } else {
                size_t expectedSize = width * height * channels;
                if (dataSize < expectedSize) {
                    LOG_ERROR(LogCategory::Core, "SkeletalMesh3D: Metallic/Roughness texture data size mismatch! Expected at least {}, got {}", expectedSize, dataSize);
                } else {
                    TextureDesc desc;
                    desc.width = width;
                    desc.height = height;
                    desc.format = TextureFormat::RGBA8_UNORM;
                    desc.usage = TextureUsage::Sampled;
                    desc.initialData = data;
                    slot.metallicRoughnessTextureHandle = device->createTexture(desc);
                }
            }
        }
    }

    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Uploading normal texture...");
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
                LOG_ERROR(LogCategory::Core, "SkeletalMesh3D: Normal texture data is null!");
            } else if (dataSize == 0) {
                LOG_ERROR(LogCategory::Core, "SkeletalMesh3D: Normal texture data size is 0!");
            } else if (width == 0 || height == 0) {
                LOG_ERROR(LogCategory::Core, "SkeletalMesh3D: Normal texture has invalid dimensions: {}x{}", width, height);
            } else {
                size_t expectedSize = width * height * channels;
                if (dataSize < expectedSize) {
                    LOG_ERROR(LogCategory::Core, "SkeletalMesh3D: Normal texture data size mismatch! Expected at least {}, got {}", expectedSize, dataSize);
                } else {
                    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Creating normal texture on GPU ({}x{}, {} channels, {} bytes), data ptr={}",
                        width, height, channels, dataSize, (void*)data);
                    TextureDesc desc;
                    desc.width = width;
                    desc.height = height;
                    desc.format = TextureFormat::RGBA8_UNORM;
                    desc.usage = TextureUsage::Sampled;
                    desc.initialData = data;
                    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Calling device->createTexture() for normal...");
                    slot.normalTextureHandle = device->createTexture(desc);
                    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Normal texture created, handle valid={}", slot.normalTextureHandle.isValid());
                }
            }
        }
    }

    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Uploading emissive texture...");
    if (!slot.emissiveTexturePath.empty() && !slot.emissiveTextureHandle.isValid()) {
        LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Emissive texture path: {}", slot.emissiveTexturePath);
        if (!slot.emissiveTextureAsset.IsValid()) {
            LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Emissive texture asset not valid, loading...");
            LoadTexture(slot.emissiveTexturePath, slot.emissiveTextureAsset);
        }

        LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Checking if emissive texture asset is valid and loaded...");
        if (slot.emissiveTextureAsset.IsValid() && slot.emissiveTextureAsset->IsLoaded()) {
            const void* data = slot.emissiveTextureAsset->GetData();
            size_t dataSize = slot.emissiveTextureAsset->GetDataSize();
            uint32_t width = slot.emissiveTextureAsset->GetWidth();
            uint32_t height = slot.emissiveTextureAsset->GetHeight();
            uint32_t channels = slot.emissiveTextureAsset->GetChannels();

            if (!data) {
                LOG_ERROR(LogCategory::Core, "SkeletalMesh3D: Emissive texture data is null!");
            } else if (dataSize == 0) {
                LOG_ERROR(LogCategory::Core, "SkeletalMesh3D: Emissive texture data size is 0!");
            } else if (width == 0 || height == 0) {
                LOG_ERROR(LogCategory::Core, "SkeletalMesh3D: Emissive texture has invalid dimensions: {}x{}", width, height);
            } else {
                size_t expectedSize = width * height * channels;
                if (dataSize < expectedSize) {
                    LOG_ERROR(LogCategory::Core, "SkeletalMesh3D: Emissive texture data size mismatch! Expected at least {}, got {}", expectedSize, dataSize);
                } else {
                    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Creating emissive texture on GPU ({}x{}, {} channels, {} bytes), data ptr={}",
                        width, height, channels, dataSize, (void*)data);
                    TextureDesc desc;
                    desc.width = width;
                    desc.height = height;
                    desc.format = TextureFormat::RGBA8_SRGB;
                    desc.usage = TextureUsage::Sampled;
                    desc.initialData = data;
                    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Calling device->createTexture()...");
                    slot.emissiveTextureHandle = device->createTexture(desc);
                    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Emissive texture created, handle valid={}", slot.emissiveTextureHandle.isValid());
                }
            }
        }
    }

    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Setting texturesNeedUpload to false...");
    slot.texturesNeedUpload = false;
    LOG_INFO(LogCategory::Core, "SkeletalMesh3D::UploadMaterialSlotTextures() completed");
}

void SkeletalMesh3D::buildDrawCommands(RenderContext& ctx) {
    LOG_INFO(LogCategory::Core, "SkeletalMesh3D::buildDrawCommands() called, enabled={}", IsEnabled());

    if (!IsEnabled()) {
        return;
    }

    // Check if model path has changed (property edited in editor)
    std::string currentPath = GetModelPath();
    if (currentPath != m_CurrentModelPath) {
        LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Model path changed from '{}' to '{}'", m_CurrentModelPath, currentPath);

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
                LOG_ERROR(LogCategory::Core, "SkeletalMesh3D: Failed to load model: {}", currentPath);
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

                LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Model loaded successfully");
            }
        }

        m_CurrentModelPath = currentPath;
    }

    if (!m_ModelAsset.IsValid()) {
        LOG_WARN(LogCategory::Core, "SkeletalMesh3D: Model asset is invalid");
        return;
    }

    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Uploading meshes to GPU...");
    UploadMeshesToGPU(ctx);
    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Meshes uploaded");

    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Getting owner node...");
    Node3D* node3D = dynamic_cast<Node3D*>(GetOwner());
    if (!node3D) {
        LOG_WARN(LogCategory::Core, "SkeletalMesh3D: Owner is not a Node3D");
        return;
    }

    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Getting transform...");
    Mat4 transform = node3D->GetGlobalTransformMatrix();

    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Getting default skeletal material...");
    MaterialHandle defaultMaterial = ctx.getDefaultSkeletalMaterial();
    if (!defaultMaterial.isValid()) {
        LOG_ERROR(LogCategory::Core, "SkeletalMesh3D: Default skeletal material is invalid!");
        return;
    }
    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Default material is valid");

    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Getting meshes and materials from model...");
    const auto& meshes = m_ModelAsset->GetMeshes();
    const auto& materials = m_ModelAsset->GetMaterials();
    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Model has {} meshes and {} materials", meshes.size(), materials.size());

    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Starting draw command loop...");
    for (size_t meshIdx = 0; meshIdx < m_MeshHandles.size(); ++meshIdx) {
        LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Processing mesh {} for drawing", meshIdx);

        LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Accessing mesh handle at index {} (total handles: {})", meshIdx, m_MeshHandles.size());
        const MeshHandle& meshHandle = m_MeshHandles[meshIdx];
        LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Mesh handle accessed");

        if (!meshHandle.isValid()) {
            LOG_WARN(LogCategory::Core, "SkeletalMesh3D: Mesh handle {} is invalid, skipping", meshIdx);
            continue;
        }

        LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Getting asset mesh {} (total meshes: {})", meshIdx, meshes.size());
        const auto& assetMesh = meshes[meshIdx];
        LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Asset mesh accessed");

        uint32_t materialIndex = assetMesh.materialIndex;
        LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Mesh {} uses material index {}", meshIdx, materialIndex);

        MaterialPropertyBlock overrides;
        bool useOverride = false;

        LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Looking for material slot for material index {}", materialIndex);
        SkeletalMaterialSlot* slot = nullptr;
        for (auto& s : m_MaterialSlots) {
            if (s.materialIndex == materialIndex) {
                slot = &s;
                break;
            }
        }
        LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Material slot found: {}", slot != nullptr);

        if (slot && slot->enableOverride) {
            LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Using override material for mesh {}", meshIdx);

            useOverride = true;

            LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Uploading material slot textures...");
            UploadMaterialSlotTextures(ctx, *slot);
            LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Material slot textures uploaded");

            overrides.setColor("u_AlbedoColor", slot->albedoColor);
            overrides.setVec4("u_MaterialParams1", Vec4(slot->metallic, slot->roughness, slot->normalScale, slot->emissiveStrength));
            overrides.setColor("u_EmissiveColor", slot->emissiveColor);
            overrides.setColor("u_TintColor", Color::White());

            // u_MaterialParams2: alphaCutoff, aoStrength, heightScale, unused
            overrides.setVec4("u_MaterialParams2", Vec4(slot->alphaCutoff, 1.0f, 1.0f, 0.0f));

            Vec4 textureFlags(
                slot->albedoTextureHandle.isValid() ? 1.0f : 0.0f,
                slot->metallicRoughnessTextureHandle.isValid() ? 1.0f : 0.0f,
                slot->normalTextureHandle.isValid() ? 1.0f : 0.0f,
                slot->emissiveTextureHandle.isValid() ? 1.0f : 0.0f
            );
            overrides.setVec4("u_TextureFlags", textureFlags);

            if (slot->albedoTextureHandle.isValid()) {
                overrides.setTexture("u_AlbedoTexture", slot->albedoTextureHandle);
            }
            if (slot->metallicRoughnessTextureHandle.isValid()) {
                overrides.setTexture("u_MetallicRoughnessTexture", slot->metallicRoughnessTextureHandle);
            }
            if (slot->normalTextureHandle.isValid()) {
                overrides.setTexture("u_NormalTexture", slot->normalTextureHandle);
            }
            if (slot->emissiveTextureHandle.isValid()) {
                overrides.setTexture("u_EmissiveTexture", slot->emissiveTextureHandle);
            }

        } else if (materialIndex < materials.size() && slot) {
            LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Using model material for mesh {}", meshIdx);

            LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Getting material at index {}", materialIndex);
            const auto& material = materials[materialIndex];

            LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Setting material properties...");
            overrides.setColor("u_AlbedoColor", Color(material.albedo.x, material.albedo.y, material.albedo.z, material.opacity));
            overrides.setVec4("u_MaterialParams1", Vec4(material.metallic, material.roughness, 1.0f, 1.0f));
            overrides.setColor("u_EmissiveColor", Color(material.emissive.x, material.emissive.y, material.emissive.z, 1.0f));
            overrides.setColor("u_TintColor", Color::White());

            // u_MaterialParams2: alphaCutoff, aoStrength, heightScale, unused
            overrides.setVec4("u_MaterialParams2", Vec4(0.5f, 1.0f, 1.0f, 0.0f));

            LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Loading and uploading material textures...");
            LoadAndUploadMaterialTextures(ctx, *slot);
            LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Material textures loaded");

            Vec4 textureFlags(
                slot->albedoTextureHandle.isValid() ? 1.0f : 0.0f,
                slot->metallicRoughnessTextureHandle.isValid() ? 1.0f : 0.0f,
                slot->normalTextureHandle.isValid() ? 1.0f : 0.0f,
                slot->emissiveTextureHandle.isValid() ? 1.0f : 0.0f
            );
            overrides.setVec4("u_TextureFlags", textureFlags);

            if (slot->albedoTextureHandle.isValid()) {
                overrides.setTexture("u_AlbedoTexture", slot->albedoTextureHandle);
            }
            if (slot->metallicRoughnessTextureHandle.isValid()) {
                overrides.setTexture("u_MetallicRoughnessTexture", slot->metallicRoughnessTextureHandle);
            }
            if (slot->normalTextureHandle.isValid()) {
                overrides.setTexture("u_NormalTexture", slot->normalTextureHandle);
            }
            if (slot->emissiveTextureHandle.isValid()) {
                overrides.setTexture("u_EmissiveTexture", slot->emissiveTextureHandle);
            }
        }

        LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Setting up GPU skinning for mesh {}", meshIdx);
        if (m_GPUSkinning && m_ModelAsset->HasSkeleton()) {
            overrides.setBool("u_UseSkinning", true);

            if (!m_BoneTransforms.empty()) {
                LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Setting bone transforms ({} bones)", m_BoneTransforms.size());
                overrides.setMat4Array("u_BoneTransforms", m_BoneTransforms.data(), m_BoneTransforms.size());
            }
        } else {
            overrides.setBool("u_UseSkinning", false);
        }

        // Pass shadow flags to shader
        overrides.setBool("u_ReceiveShadow", m_ReceiveShadow);

        LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Submitting draw command for mesh {}", meshIdx);
        ctx.drawMesh(meshHandle, defaultMaterial, transform, overrides, meshIdx, m_CastShadow, m_ReceiveShadow);
        LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Draw command submitted for mesh {}", meshIdx);
    }
    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: All draw commands submitted");
}

void SkeletalMesh3D::LoadAndUploadMaterialTextures(RenderContext& ctx, SkeletalMaterialSlot& slot) {
    LOG_INFO(LogCategory::Core, "SkeletalMesh3D::LoadAndUploadMaterialTextures() called for material index {}", slot.materialIndex);

    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Checking albedo texture...");
    if (!slot.albedoTexturePath.empty() && !slot.albedoTextureAsset.IsValid()) {
        LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Loading albedo texture: {}", slot.albedoTexturePath);
        if (LoadTexture(slot.albedoTexturePath, slot.albedoTextureAsset)) {
            LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Albedo texture loaded successfully");
        } else {
            LOG_ERROR(LogCategory::Core, "SkeletalMesh3D: Failed to load albedo texture: {}", slot.albedoTexturePath);
        }
    }

    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Checking metallic/roughness texture...");
    if (!slot.metallicRoughnessTexturePath.empty() && !slot.metallicRoughnessTextureAsset.IsValid()) {
        LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Loading metallic/roughness texture: {}", slot.metallicRoughnessTexturePath);
        if (LoadTexture(slot.metallicRoughnessTexturePath, slot.metallicRoughnessTextureAsset)) {
            LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Metallic/roughness texture loaded successfully");
        } else {
            LOG_ERROR(LogCategory::Core, "SkeletalMesh3D: Failed to load metallic/roughness texture: {}", slot.metallicRoughnessTexturePath);
        }
    }

    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Checking normal texture...");
    if (!slot.normalTexturePath.empty() && !slot.normalTextureAsset.IsValid()) {
        LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Loading normal texture: {}", slot.normalTexturePath);
        if (LoadTexture(slot.normalTexturePath, slot.normalTextureAsset)) {
            LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Normal texture loaded successfully");
        } else {
            LOG_ERROR(LogCategory::Core, "SkeletalMesh3D: Failed to load normal texture: {}", slot.normalTexturePath);
        }
    }

    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Checking emissive texture...");
    if (!slot.emissiveTexturePath.empty() && !slot.emissiveTextureAsset.IsValid()) {
        LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Loading emissive texture: {}", slot.emissiveTexturePath);
        if (LoadTexture(slot.emissiveTexturePath, slot.emissiveTextureAsset)) {
            LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Emissive texture loaded successfully");
        } else {
            LOG_ERROR(LogCategory::Core, "SkeletalMesh3D: Failed to load emissive texture: {}", slot.emissiveTexturePath);
        }
    }

    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Setting texturesNeedUpload flag...");
    slot.texturesNeedUpload = true;

    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Uploading material slot textures...");
    UploadMaterialSlotTextures(ctx, slot);
    LOG_INFO(LogCategory::Core, "SkeletalMesh3D: Material slot textures uploaded");
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
    if (!m_ModelAsset.IsValid()) {
        return AABB();
    }

    const auto& meshes = m_ModelAsset->GetMeshes();
    if (meshes.empty()) {
        return AABB();
    }

    AABB combined(meshes[0].boundsMin, meshes[0].boundsMax);

    for (size_t i = 1; i < meshes.size(); ++i) {
        AABB meshBounds(meshes[i].boundsMin, meshes[i].boundsMax);
        combined = AABB::Merge(combined, meshBounds);
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
    Vec3 localHalfExtents = localBounds.GetExtents();

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

}
}

