#include "lupine/scripting/ScriptAPI.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Component.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/core/SceneManager.hpp"
#include "lupine/input/InputManager.hpp"
#include "lupine/input/InputCodes.hpp"
#include "lupine/audio/AudioManager.hpp"
#include "lupine/asset/ImageAsset.hpp"
#include "lupine/asset/AudioAsset.hpp"
#include "lupine/asset/ModelAsset.hpp"
#include "lupine/logger/Logger.hpp"
#include <random>
#include <chrono>

namespace lupine {
namespace scripting {

static auto s_StartTime = std::chrono::high_resolution_clock::now();
static int s_FrameCount = 0;

ScriptAPI::ScriptAPI() {
}

ScriptAPI::~ScriptAPI() {
}

void ScriptAPI::SetOwner(core::Node* owner) {
    m_Owner = owner;
}

void ScriptAPI::SetGamePaused(bool paused) {

}

bool ScriptAPI::IsGamePaused() const {

    return false;
}

float ScriptAPI::GetDeltaTime() const {
    return m_DeltaTime;
}

bool ScriptAPI::IsActionPressed(const std::string& action) const {
    return input::InputManager::Get().IsActionPressed(action);
}

bool ScriptAPI::IsActionJustPressed(const std::string& action) const {
    return input::InputManager::Get().IsActionJustPressed(action);
}

bool ScriptAPI::IsActionJustReleased(const std::string& action) const {
    return input::InputManager::Get().IsActionJustReleased(action);
}

float ScriptAPI::GetActionStrength(const std::string& action) const {

    return IsActionPressed(action) ? 1.0f : 0.0f;
}

float ScriptAPI::GetAxis(const std::string& axis) const {
    return input::InputManager::Get().GetAxisValue(axis);
}

math::Vec2 ScriptAPI::GetVector(const std::string& negativeX, const std::string& positiveX,
                                const std::string& negativeY, const std::string& positiveY) const {
    float x = 0.0f;
    float y = 0.0f;

    if (IsActionPressed(positiveX)) x += 1.0f;
    if (IsActionPressed(negativeX)) x -= 1.0f;
    if (IsActionPressed(positiveY)) y += 1.0f;
    if (IsActionPressed(negativeY)) y -= 1.0f;

    if (x != 0.0f && y != 0.0f) {
        float length = std::sqrt(x * x + y * y);
        x /= length;
        y /= length;
    }

    return math::Vec2(x, y);
}

bool ScriptAPI::IsKeyPressed(int keyCode) const {
    return input::InputManager::Get().IsKeyPressed(static_cast<input::KeyCode>(keyCode));
}

bool ScriptAPI::IsKeyJustPressed(int keyCode) const {
    return input::InputManager::Get().IsKeyJustPressed(static_cast<input::KeyCode>(keyCode));
}

bool ScriptAPI::IsKeyJustReleased(int keyCode) const {
    return input::InputManager::Get().IsKeyJustReleased(static_cast<input::KeyCode>(keyCode));
}

bool ScriptAPI::IsMouseButtonPressed(int button) const {
    return input::InputManager::Get().IsMouseButtonPressed(static_cast<input::MouseButton>(button));
}

bool ScriptAPI::IsMouseButtonJustPressed(int button) const {
    return input::InputManager::Get().IsMouseButtonJustPressed(static_cast<input::MouseButton>(button));
}

bool ScriptAPI::IsMouseButtonJustReleased(int button) const {
    return input::InputManager::Get().IsMouseButtonJustReleased(static_cast<input::MouseButton>(button));
}

math::Vec2 ScriptAPI::GetMousePosition() const {
    auto pos = input::InputManager::Get().GetMousePosition();
    return math::Vec2(pos.x, pos.y);
}

math::Vec2 ScriptAPI::GetMouseDelta() const {
    auto delta = input::InputManager::Get().GetMouseDelta();
    return math::Vec2(delta.x, delta.y);
}

math::Vec2 ScriptAPI::GetMouseScrollDelta() const {
    auto scroll = input::InputManager::Get().GetMouseScrollDelta();
    return math::Vec2(scroll.x, scroll.y);
}

void ScriptAPI::QueueFree(core::Node* node) {
    if (!node) return;

}

void ScriptAPI::QueueFreeSelf() {
    QueueFree(m_Owner);
}

void ScriptAPI::DestroyNode(core::Node* node) {
    if (!node || !node->GetParent()) return;

    auto parent = node->GetParent();
    auto scene = node->GetScene();

    if (scene) {

        for (auto& child : parent->GetChildren()) {
            if (child.get() == node) {
                parent->RemoveChild(child);
                break;
            }
        }
    }
}

void ScriptAPI::AddChild(std::shared_ptr<core::Node> child) {
    if (!m_Owner || !child) return;
    m_Owner->AddChild(child);
}

void ScriptAPI::AddSibling(std::shared_ptr<core::Node> sibling) {
    if (!m_Owner || !sibling || !m_Owner->GetParent()) return;
    m_Owner->GetParent()->AddChild(sibling);
}

void ScriptAPI::RemoveChild(core::Node* child) {
    if (!m_Owner || !child) return;

    for (auto& c : m_Owner->GetChildren()) {
        if (c.get() == child) {
            m_Owner->RemoveChild(c);
            break;
        }
    }
}

void ScriptAPI::RemoveChild(const std::string& name) {
    if (!m_Owner) return;
    m_Owner->RemoveChild(name);
}

core::Node* ScriptAPI::FindNode(const std::string& path) const {
    if (!m_Owner) return nullptr;

    auto scene = m_Owner->GetScene();
    if (!scene) return nullptr;

    auto node = scene->FindNode(path);
    return node.get();
}

core::Node* ScriptAPI::FindNodeByUUID(const std::string& uuidStr) const {
    if (!m_Owner) return nullptr;

    auto scene = m_Owner->GetScene();
    if (!scene) return nullptr;

    core::UUID uuid = core::UUID::FromString(uuidStr);
    auto node = scene->FindNodeByUUID(uuid);
    return node.get();
}

core::Node* ScriptAPI::GetParent() const {
    if (!m_Owner) return nullptr;
    return m_Owner->GetParent();
}

core::Node* ScriptAPI::GetChild(const std::string& name) const {
    if (!m_Owner) return nullptr;
    auto child = m_Owner->GetChild(name);
    return child.get();
}

core::Node* ScriptAPI::GetChild(int index) const {
    if (!m_Owner) return nullptr;
    auto child = m_Owner->GetChild(static_cast<size_t>(index));
    return child.get();
}

int ScriptAPI::GetChildCount() const {
    if (!m_Owner) return 0;
    return static_cast<int>(m_Owner->GetChildCount());
}

core::Component* ScriptAPI::GetComponent(const std::string& typeName) const {
    if (!m_Owner) return nullptr;
    auto comp = m_Owner->GetComponent(typeName);
    return comp.get();
}

std::vector<core::Component*> ScriptAPI::GetComponents(const std::string& typeName) const {
    std::vector<core::Component*> result;
    if (!m_Owner) return result;

    for (auto& comp : m_Owner->GetComponents()) {
        if (comp->GetTypeName() == typeName) {
            result.push_back(comp.get());
        }
    }

    return result;
}

core::Component* ScriptAPI::GetComponentInChildren(const std::string& typeName) const {
    if (!m_Owner) return nullptr;

    auto comp = GetComponent(typeName);
    if (comp) return comp;

    for (auto& child : m_Owner->GetChildren()) {
        auto childComp = child->GetComponent(typeName);
        if (childComp) return childComp.get();

        ScriptAPI childAPI;
        childAPI.SetOwner(child.get());
        auto found = childAPI.GetComponentInChildren(typeName);
        if (found) return found;
    }

    return nullptr;
}

core::Component* ScriptAPI::GetComponentInParent(const std::string& typeName) const {
    if (!m_Owner) return nullptr;

    auto parent = m_Owner->GetParent();
    while (parent) {
        auto comp = parent->GetComponent(typeName);
        if (comp) return comp.get();
        parent = parent->GetParent();
    }

    return nullptr;
}

void ScriptAPI::AddComponent(std::shared_ptr<core::Component> component) {
    if (!m_Owner || !component) return;
    m_Owner->AddComponent(component);
}

void ScriptAPI::RemoveComponent(core::Component* component) {
    if (!m_Owner || !component) return;

    for (auto& comp : m_Owner->GetComponents()) {
        if (comp.get() == component) {
            m_Owner->RemoveComponent(comp);
            break;
        }
    }
}

void ScriptAPI::ChangeScene(const std::string& scenePath) {
    if (!m_SceneManager) {

        return;
    }
    m_SceneManager->SwitchScene(scenePath);
}

void ScriptAPI::AddScene(const std::string& scenePath) {
    if (!m_SceneManager) {

        return;
    }
    m_SceneManager->AddAutoloadScene(scenePath);
}

void ScriptAPI::RemoveScene(const std::string& sceneName) {
    if (!m_SceneManager) {

        return;
    }
    m_SceneManager->RemoveAutoloadScene(sceneName);
}

core::Scene* ScriptAPI::GetCurrentScene() const {
    if (m_SceneManager) {
        return m_SceneManager->GetCurrentScene();
    }

    if (!m_Owner) return nullptr;
    return m_Owner->GetScene();
}

bool ScriptAPI::LoadImageAsset(const std::string& assetPath) {

    if (m_ImageCache.find(assetPath) != m_ImageCache.end()) {
        return true;
    }

    asset::AssetRef<asset::ImageAsset> image(new asset::ImageAsset());
    bool loaded = image->LoadFromFile(assetPath, true, asset::ImageColorSpace::sRGB);

    if (loaded) {
        m_ImageCache[assetPath] = image;

        return true;
    } else {

        return false;
    }
}

bool ScriptAPI::LoadAudioAsset(const std::string& assetPath, const std::string& loadMode) {

    if (m_AudioCache.find(assetPath) != m_AudioCache.end()) {
        return true;
    }

    asset::AudioLoadMode mode = asset::AudioLoadMode::Preload;
    if (loadMode == "stream" || loadMode == "streaming") {
        mode = asset::AudioLoadMode::Streaming;
    }

    asset::AssetRef<asset::AudioAsset> audio(new asset::AudioAsset());
    bool loaded = audio->LoadFromFile(assetPath, mode);

    if (loaded) {
        m_AudioCache[assetPath] = audio;

        return true;
    } else {

        return false;
    }
}

bool ScriptAPI::LoadModelAsset(const std::string& assetPath) {

    if (m_ModelCache.find(assetPath) != m_ModelCache.end()) {
        return true;
    }

    asset::AssetRef<asset::ModelAsset> model(new asset::ModelAsset());
    bool loaded = model->LoadFromFile(assetPath, true);

    if (loaded) {
        m_ModelCache[assetPath] = model;

        return true;
    } else {

        return false;
    }
}

void ScriptAPI::PreloadAssets(const std::vector<std::string>& assetPaths) {
    for (const auto& path : assetPaths) {

        std::string ext = path.substr(path.find_last_of('.'));
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" ||
            ext == ".tga" || ext == ".hdr") {
            LoadImageAsset(path);
        } else if (ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac") {
            LoadAudioAsset(path, "preload");
        } else if (ext == ".gltf" || ext == ".glb" || ext == ".fbx" || ext == ".obj") {
            LoadModelAsset(path);
        } else {

        }
    }
}

std::string ScriptAPI::PlayAudio(const std::string& audioPath, const std::string& busName,
                                 bool loop, float volume) {

    if (m_AudioCache.find(audioPath) == m_AudioCache.end()) {
        if (!LoadAudioAsset(audioPath, "preload")) {

            return "";
        }
    }

    auto& audioAsset = m_AudioCache[audioPath];

    audio::PlaybackMode mode = loop ? audio::PlaybackMode::Loop : audio::PlaybackMode::OneShot;
    core::UUID sourceUUID = audio::AudioManager::GetInstance().Play(audioAsset, busName, mode, volume);

    return sourceUUID.ToString();
}

std::string ScriptAPI::PlayAudio3D(const std::string& audioPath, const math::Vec3& position,
                                   const std::string& busName, bool loop, float volume) {

    if (m_AudioCache.find(audioPath) == m_AudioCache.end()) {
        if (!LoadAudioAsset(audioPath, "preload")) {

            return "";
        }
    }

    auto& audioAsset = m_AudioCache[audioPath];

    audio::PlaybackMode mode = loop ? audio::PlaybackMode::Loop : audio::PlaybackMode::OneShot;
    core::UUID sourceUUID = audio::AudioManager::GetInstance().Play3D(audioAsset, position, busName, mode, volume);

    return sourceUUID.ToString();
}

void ScriptAPI::StopAudio(const std::string& sourceUUID) {
    core::UUID uuid = core::UUID::FromString(sourceUUID);
    audio::AudioManager::GetInstance().Stop(uuid);
}

void ScriptAPI::PauseAudio(const std::string& sourceUUID) {
    core::UUID uuid = core::UUID::FromString(sourceUUID);
    audio::AudioManager::GetInstance().Pause(uuid);
}

void ScriptAPI::ResumeAudio(const std::string& sourceUUID) {
    core::UUID uuid = core::UUID::FromString(sourceUUID);
    audio::AudioManager::GetInstance().Resume(uuid);
}

void ScriptAPI::SetBusVolume(const std::string& busName, float volume) {
    audio::AudioManager::GetInstance().SetBusVolume(busName, volume);
}

float ScriptAPI::GetBusVolume(const std::string& busName) const {
    return audio::AudioManager::GetInstance().GetBusVolume(busName);
}

void ScriptAPI::SetBusMuted(const std::string& busName, bool muted) {
    audio::AudioManager::GetInstance().SetBusMuted(busName, muted);
}

bool ScriptAPI::IsBusMuted(const std::string& busName) const {
    return audio::AudioManager::GetInstance().IsBusMuted(busName);
}

void ScriptAPI::LogInfo(const std::string& message) {

}

void ScriptAPI::LogWarning(const std::string& message) {

}

void ScriptAPI::LogError(const std::string& message) {

}

void ScriptAPI::LogDebug(const std::string& message) {

}

float ScriptAPI::RandomRange(float min, float max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(min, max);
    return dis(gen);
}

int ScriptAPI::RandomRangeInt(int min, int max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(min, max);
    return dis(gen);
}

float ScriptAPI::GetTime() const {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - s_StartTime);
    return duration.count() / 1000.0f;
}

int ScriptAPI::GetFrameCount() const {
    return s_FrameCount;
}

float ScriptAPI::Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

float ScriptAPI::Clamp(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

float ScriptAPI::Abs(float value) {
    return std::abs(value);
}

float ScriptAPI::Sign(float value) {
    if (value > 0.0f) return 1.0f;
    if (value < 0.0f) return -1.0f;
    return 0.0f;
}

int ScriptAPI::GetGlobalInt(const std::string& name, int defaultValue) const {
    if (m_SceneManager) {
        return m_SceneManager->GetGlobalInt(name, defaultValue);
    }
    return defaultValue;
}

float ScriptAPI::GetGlobalFloat(const std::string& name, float defaultValue) const {
    if (m_SceneManager) {
        return m_SceneManager->GetGlobalFloat(name, defaultValue);
    }
    return defaultValue;
}

std::string ScriptAPI::GetGlobalString(const std::string& name, const std::string& defaultValue) const {
    if (m_SceneManager) {
        return m_SceneManager->GetGlobalString(name, defaultValue);
    }
    return defaultValue;
}

bool ScriptAPI::GetGlobalBool(const std::string& name, bool defaultValue) const {
    if (m_SceneManager) {
        return m_SceneManager->GetGlobalBool(name, defaultValue);
    }
    return defaultValue;
}

void ScriptAPI::SetGlobalInt(const std::string& name, int value) {
    if (m_SceneManager) {
        m_SceneManager->SetGlobalInt(name, value);
    }
}

void ScriptAPI::SetGlobalFloat(const std::string& name, float value) {
    if (m_SceneManager) {
        m_SceneManager->SetGlobalFloat(name, value);
    }
}

void ScriptAPI::SetGlobalString(const std::string& name, const std::string& value) {
    if (m_SceneManager) {
        m_SceneManager->SetGlobalString(name, value);
    }
}

void ScriptAPI::SetGlobalBool(const std::string& name, bool value) {
    if (m_SceneManager) {
        m_SceneManager->SetGlobalBool(name, value);
    }
}

}
}

