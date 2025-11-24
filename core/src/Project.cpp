#include "lupine/core/Project.hpp"
#include "lupine/platform/Platform.hpp"
#include "lupine/logger/Logger.hpp"
#include <nlohmann/json.hpp>

namespace lupine {
namespace core {

ProjectSettings::ProjectSettings()
    : projectName("New Lupine Project"),
      creatorName(""),
      version("1.0.0"),
      mainScene("res://scenes/main.scene"),
      windowWidth(1280),
      windowHeight(720),
      fullscreen(false),
      vsync(true),
      referenceResolution(1920.0f, 1080.0f),
      scaleMode(ScaleMode::ScaleWithScreenSize),
      viewportScaleMode(ViewportScaleMode::Letterbox),
      textureFiltering(TextureFiltering::Bilinear),
      physicsTickRate(60.0f),
      gravity2D(0.0f, -980.0f),
      gravity3D(0.0f, -9.8f, 0.0f),
      clearColor(0.1f, 0.1f, 0.1f, 1.0f),
      targetFrameRate(60),
      maxUndoSteps(100) {
}

void ProjectSettings::RegisterProperties() {
    RegisterProperty<std::string>("project_name", core::PropertyType::String,
        [this]() { return projectName; },
        [this](const std::string& value) { projectName = value; });

    RegisterProperty<std::string>("creator_name", core::PropertyType::String,
        [this]() { return creatorName; },
        [this](const std::string& value) { creatorName = value; });

    RegisterProperty<std::string>("version", core::PropertyType::String,
        [this]() { return version; },
        [this](const std::string& value) { version = value; });

    RegisterProperty<std::string>("main_scene", core::PropertyType::String,
        [this]() { return mainScene; },
        [this](const std::string& value) { mainScene = value; });

    RegisterProperty<int>("window_width", core::PropertyType::Int,
        [this]() { return windowWidth; },
        [this](const int& value) { windowWidth = value; });

    RegisterProperty<int>("window_height", core::PropertyType::Int,
        [this]() { return windowHeight; },
        [this](const int& value) { windowHeight = value; });

    RegisterProperty<bool>("fullscreen", core::PropertyType::Bool,
        [this]() { return fullscreen; },
        [this](const bool& value) { fullscreen = value; });

    RegisterProperty<bool>("vsync", core::PropertyType::Bool,
        [this]() { return vsync; },
        [this](const bool& value) { vsync = value; });

    RegisterProperty<math::Vec2>("reference_resolution", core::PropertyType::Vec2,
        [this]() { return referenceResolution; },
        [this](const math::Vec2& value) { referenceResolution = value; });

    RegisterProperty<int>("viewport_scale_mode", core::PropertyType::Int,
        [this]() { return static_cast<int>(viewportScaleMode); },
        [this](const int& value) { viewportScaleMode = static_cast<ViewportScaleMode>(value); });

    RegisterProperty<int>("texture_filtering", core::PropertyType::Int,
        [this]() { return static_cast<int>(textureFiltering); },
        [this](const int& value) { textureFiltering = static_cast<TextureFiltering>(value); });

    RegisterProperty<float>("physics_tick_rate", core::PropertyType::Float,
        [this]() { return physicsTickRate; },
        [this](const float& value) { physicsTickRate = value; });

    RegisterProperty<math::Vec2>("gravity_2d", core::PropertyType::Vec2,
        [this]() { return gravity2D; },
        [this](const math::Vec2& value) { gravity2D = value; });

    RegisterProperty<math::Vec3>("gravity_3d", core::PropertyType::Vec3,
        [this]() { return gravity3D; },
        [this](const math::Vec3& value) { gravity3D = value; });

    RegisterProperty<math::Color>("clear_color", core::PropertyType::Color,
        [this]() { return clearColor; },
        [this](const math::Color& value) { clearColor = value; });

    RegisterProperty<int>("target_frame_rate", core::PropertyType::Int,
        [this]() { return targetFrameRate; },
        [this](const int& value) { targetFrameRate = value; });

    RegisterProperty<int>("max_undo_steps", core::PropertyType::Int,
        [this]() { return maxUndoSteps; },
        [this](const int& value) { maxUndoSteps = value; });
}

Project::Project()
    : m_Loaded(false) {
}

Project::Project(const std::string& projectPath)
    : m_ProjectPath(projectPath), m_Loaded(false) {
    Load(projectPath);
}

Project::~Project() {
}

bool Project::Load(const std::string& projectPath) {
    m_ProjectPath = projectPath;

    auto result = platform::FileSystem::ReadFile(projectPath);
    if (!result.success) {

        return false;
    }

    try {
        nlohmann::json json = nlohmann::json::parse(result.data);

        m_Settings.RegisterProperties();

        if (json.contains("project") && json.contains("window")) {

            if (json.contains("project")) {
                const auto& proj = json["project"];
                if (proj.contains("name")) m_Settings.projectName = proj["name"].get<std::string>();
                if (proj.contains("creator")) m_Settings.creatorName = proj["creator"].get<std::string>();
                if (proj.contains("version")) m_Settings.version = proj["version"].get<std::string>();
                if (proj.contains("main_scene")) {
                    std::string mainScene = proj["main_scene"].get<std::string>();

                    if (mainScene.find("res://") != 0 && mainScene.find("://") == std::string::npos) {
                        mainScene = "res://" + mainScene;
                    }
                    m_Settings.mainScene = mainScene;
                }
            }

            if (json.contains("window")) {
                const auto& win = json["window"];
                if (win.contains("width")) m_Settings.windowWidth = win["width"].get<int>();
                if (win.contains("height")) m_Settings.windowHeight = win["height"].get<int>();
                if (win.contains("fullscreen")) m_Settings.fullscreen = win["fullscreen"].get<bool>();
                if (win.contains("vsync")) m_Settings.vsync = win["vsync"].get<bool>();
                if (win.contains("target_fps")) m_Settings.targetFrameRate = win["target_fps"].get<int>();
            }

            if (json.contains("graphics")) {
                const auto& gfx = json["graphics"];
                if (gfx.contains("scale_mode")) {
                    std::string scaleMode = gfx["scale_mode"].get<std::string>();
                    if (scaleMode == "letterbox") m_Settings.viewportScaleMode = ProjectSettings::ViewportScaleMode::Letterbox;
                    else if (scaleMode == "stretch") m_Settings.viewportScaleMode = ProjectSettings::ViewportScaleMode::Stretch;
                    else if (scaleMode == "crop") m_Settings.viewportScaleMode = ProjectSettings::ViewportScaleMode::Crop;
                    else if (scaleMode == "ignore") m_Settings.viewportScaleMode = ProjectSettings::ViewportScaleMode::Ignore;
                }
                if (gfx.contains("texture_filtering")) {
                    std::string filtering = gfx["texture_filtering"].get<std::string>();
                    if (filtering == "nearest") m_Settings.textureFiltering = ProjectSettings::TextureFiltering::NearestNeighbor;
                    else if (filtering == "bilinear") m_Settings.textureFiltering = ProjectSettings::TextureFiltering::Bilinear;
                    else if (filtering == "cubic") m_Settings.textureFiltering = ProjectSettings::TextureFiltering::Cubic;
                }
            }
        } else if (json.contains("settings")) {

            m_Settings.Deserialize(json["settings"]);
        }

        m_Loaded = true;

        return true;
    } catch (const std::exception& e) {

        return false;
    }
}

bool Project::Save() {
    if (m_ProjectPath.empty()) {

        return false;
    }
    return SaveAs(m_ProjectPath);
}

bool Project::SaveAs(const std::string& projectPath) {
    m_ProjectPath = projectPath;

    m_Settings.RegisterProperties();

    nlohmann::json json;
    json["lupine_version"] = "0.1.0";
    json["settings"] = m_Settings.Serialize();

    std::string jsonStr = json.dump(2);
    auto result = platform::FileSystem::WriteFile(projectPath, jsonStr);

    if (!result.success) {

        return false;
    }

    return true;
}

std::string Project::GetProjectDirectory() const {
    if (m_ProjectPath.empty()) return "";
    return platform::Path::GetDirectory(m_ProjectPath);
}

std::string Project::GetScenesDirectory() const {
    return platform::Path::Join(GetProjectDirectory(), "scenes");
}

std::string Project::GetAssetsDirectory() const {
    return platform::Path::Join(GetProjectDirectory(), "assets");
}

std::string Project::GetScriptsDirectory() const {
    return platform::Path::Join(GetProjectDirectory(), "scripts");
}

}
}

