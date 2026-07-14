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
      iconPath(""),
      windowWidth(1280),
      windowHeight(720),
      fullscreen(false),
      vsync(true),
      windowResizable(true),
      windowBorderless(false),
      windowSizeOverride(false),
      windowSizeOverrideWidth(1280),
      windowSizeOverrideHeight(720),
      referenceResolution(1920.0f, 1080.0f),
      scaleMode(ScaleMode::ScaleWithScreenSize),
      viewportScaleMode(ViewportScaleMode::Letterbox),
      textureFiltering(TextureFiltering::Bilinear),
      physicsTickRate(60.0f),
      gravity2D(0.0f, -980.0f),
      gravity3D(0.0f, -9.8f, 0.0f),
      masterVolume(1.0f),
      musicVolume(1.0f),
      sfxVolume(1.0f),
      clearColor(0.1f, 0.1f, 0.1f, 1.0f),
      targetFrameRate(60),
      defaultFont(""),
      defaultTheme(""),
      maxUndoSteps(100),
      networkTransport("enet"),
      networkDefaultPort(7777),
      networkMaxPeers(32),
      networkTickRate(30.0f),
      networkInterpDelayMs(100.0f),
      networkKeyframeIntervalMs(1000.0f),
      networkPingIntervalSeconds(1.0f),
      networkInterestRadius(0.0f),
      networkProtocolVersion(1),
      networkGameId("lupine-game"),
      networkEnableLanDiscovery(false),
      networkDiscoveryPort(7779),
      networkServerName("Lupine Server"),
      networkEnablePrediction(true),
      networkInputRedundancy(3),
      networkAutoReconnect(false),
      networkReconnectAttempts(3),
      networkReconnectDelaySeconds(2.0f),
      networkResumeTimeoutSeconds(0.0f),
      networkMaxMessagesPerSecond(0),
      networkMaxBytesPerSecond(0) {
    collisionLayers2D.reserve(32);
    collisionLayers3D.reserve(32);
    for (int i = 0; i < 32; ++i) {
        collisionLayers2D.push_back("Layer " + std::to_string(i + 1));
        collisionLayers3D.push_back("Layer " + std::to_string(i + 1));
    }
}

void ProjectSettings::RegisterProperties() {
    if (m_PropertiesRegistered) {
        return;
    }
    m_PropertiesRegistered = true;

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

    RegisterProperty<std::string>("icon", core::PropertyType::String,
        [this]() { return iconPath; },
        [this](const std::string& value) { iconPath = value; });

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

    RegisterProperty<bool>("window_resizable", core::PropertyType::Bool,
        [this]() { return windowResizable; },
        [this](const bool& value) { windowResizable = value; });

    RegisterProperty<bool>("window_borderless", core::PropertyType::Bool,
        [this]() { return windowBorderless; },
        [this](const bool& value) { windowBorderless = value; });

    RegisterProperty<bool>("window_size_override", core::PropertyType::Bool,
        [this]() { return windowSizeOverride; },
        [this](const bool& value) { windowSizeOverride = value; });

    RegisterProperty<int>("window_size_override_width", core::PropertyType::Int,
        [this]() { return windowSizeOverrideWidth; },
        [this](const int& value) { windowSizeOverrideWidth = value; });

    RegisterProperty<int>("window_size_override_height", core::PropertyType::Int,
        [this]() { return windowSizeOverrideHeight; },
        [this](const int& value) { windowSizeOverrideHeight = value; });

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

    RegisterProperty<float>("master_volume", core::PropertyType::Float,
        [this]() { return masterVolume; },
        [this](const float& value) { masterVolume = value; });

    RegisterProperty<float>("music_volume", core::PropertyType::Float,
        [this]() { return musicVolume; },
        [this](const float& value) { musicVolume = value; });

    RegisterProperty<float>("sfx_volume", core::PropertyType::Float,
        [this]() { return sfxVolume; },
        [this](const float& value) { sfxVolume = value; });

    RegisterProperty<math::Color>("clear_color", core::PropertyType::Color,
        [this]() { return clearColor; },
        [this](const math::Color& value) { clearColor = value; });

    RegisterProperty<int>("target_frame_rate", core::PropertyType::Int,
        [this]() { return targetFrameRate; },
        [this](const int& value) { targetFrameRate = value; });

    RegisterProperty<int>("max_undo_steps", core::PropertyType::Int,
        [this]() { return maxUndoSteps; },
        [this](const int& value) { maxUndoSteps = value; });

    RegisterProperty<std::string>("default_font", core::PropertyType::String,
        [this]() { return defaultFont; },
        [this](const std::string& value) { defaultFont = value; });

    RegisterProperty<std::string>("default_theme", core::PropertyType::String,
        [this]() { return defaultTheme; },
        [this](const std::string& value) { defaultTheme = value; });
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

    return LoadFromString(result.data);
}

bool Project::LoadFromString(const std::string& jsonString) {
    m_RawProjectJson = jsonString;

    try {
        nlohmann::json json = nlohmann::json::parse(jsonString);

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
                if (proj.contains("icon")) {
                    std::string icon = proj["icon"].get<std::string>();
                    if (!icon.empty() && icon.find("res://") != 0 && icon.find("://") == std::string::npos) {
                        icon = "res://" + icon;
                    }
                    m_Settings.iconPath = icon;
                }
            }

            if (json.contains("window")) {
                const auto& win = json["window"];
                if (win.contains("width")) m_Settings.windowWidth = win["width"].get<int>();
                if (win.contains("height")) m_Settings.windowHeight = win["height"].get<int>();
                if (win.contains("fullscreen")) m_Settings.fullscreen = win["fullscreen"].get<bool>();
                if (win.contains("vsync")) m_Settings.vsync = win["vsync"].get<bool>();
                if (win.contains("resizable")) m_Settings.windowResizable = win["resizable"].get<bool>();
                if (win.contains("borderless")) m_Settings.windowBorderless = win["borderless"].get<bool>();
                if (win.contains("target_fps")) m_Settings.targetFrameRate = win["target_fps"].get<int>();
                if (win.contains("size_override")) m_Settings.windowSizeOverride = win["size_override"].get<bool>();
                if (win.contains("override_width")) m_Settings.windowSizeOverrideWidth = win["override_width"].get<int>();
                if (win.contains("override_height")) m_Settings.windowSizeOverrideHeight = win["override_height"].get<int>();
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

            // Physics settings
            if (json.contains("physics")) {
                const auto& physics = json["physics"];
                if (physics.contains("physics_tick_rate")) {
                    m_Settings.physicsTickRate = physics["physics_tick_rate"].get<float>();
                }
                if (physics.contains("gravity_2d") && physics["gravity_2d"].is_array() &&
                    physics["gravity_2d"].size() >= 2) {
                    m_Settings.gravity2D.x = physics["gravity_2d"][0].get<float>();
                    m_Settings.gravity2D.y = physics["gravity_2d"][1].get<float>();
                }
                if (physics.contains("gravity_3d") && physics["gravity_3d"].is_array() &&
                    physics["gravity_3d"].size() >= 3) {
                    m_Settings.gravity3D.x = physics["gravity_3d"][0].get<float>();
                    m_Settings.gravity3D.y = physics["gravity_3d"][1].get<float>();
                    m_Settings.gravity3D.z = physics["gravity_3d"][2].get<float>();
                }
                if (physics.contains("collision_layers_2d") && physics["collision_layers_2d"].is_array()) {
                    m_Settings.collisionLayers2D.clear();
                    for (const auto& name : physics["collision_layers_2d"]) {
                        m_Settings.collisionLayers2D.push_back(name.get<std::string>());
                    }
                }
                if (physics.contains("collision_layers_3d") && physics["collision_layers_3d"].is_array()) {
                    m_Settings.collisionLayers3D.clear();
                    for (const auto& name : physics["collision_layers_3d"]) {
                        m_Settings.collisionLayers3D.push_back(name.get<std::string>());
                    }
                }
            }

            // Audio settings
            if (json.contains("audio")) {
                const auto& audio = json["audio"];
                if (audio.contains("master_volume")) m_Settings.masterVolume = audio["master_volume"].get<float>();
                if (audio.contains("music_volume")) m_Settings.musicVolume = audio["music_volume"].get<float>();
                if (audio.contains("sfx_volume")) m_Settings.sfxVolume = audio["sfx_volume"].get<float>();
            }

            // Networking settings (consumed into the NetworkManager default config
            // by SceneManager::ApplyProjectSettings).
            if (json.contains("networking")) {
                const auto& net = json["networking"];
                if (net.contains("transport")) m_Settings.networkTransport = net["transport"].get<std::string>();
                if (net.contains("default_port")) m_Settings.networkDefaultPort = net["default_port"].get<int>();
                if (net.contains("max_peers")) m_Settings.networkMaxPeers = net["max_peers"].get<int>();
                if (net.contains("tick_rate")) m_Settings.networkTickRate = net["tick_rate"].get<float>();
                if (net.contains("interp_delay_ms")) m_Settings.networkInterpDelayMs = net["interp_delay_ms"].get<float>();
                if (net.contains("keyframe_interval_ms")) m_Settings.networkKeyframeIntervalMs = net["keyframe_interval_ms"].get<float>();
                if (net.contains("ping_interval_seconds")) m_Settings.networkPingIntervalSeconds = net["ping_interval_seconds"].get<float>();
                if (net.contains("interest_radius")) m_Settings.networkInterestRadius = net["interest_radius"].get<float>();
                if (net.contains("protocol_version")) m_Settings.networkProtocolVersion = net["protocol_version"].get<int>();
                if (net.contains("game_id")) m_Settings.networkGameId = net["game_id"].get<std::string>();
                if (net.contains("enable_lan_discovery")) m_Settings.networkEnableLanDiscovery = net["enable_lan_discovery"].get<bool>();
                if (net.contains("discovery_port")) m_Settings.networkDiscoveryPort = net["discovery_port"].get<int>();
                if (net.contains("server_name")) m_Settings.networkServerName = net["server_name"].get<std::string>();
                if (net.contains("enable_prediction")) m_Settings.networkEnablePrediction = net["enable_prediction"].get<bool>();
                if (net.contains("input_redundancy")) m_Settings.networkInputRedundancy = net["input_redundancy"].get<int>();
                if (net.contains("auto_reconnect")) m_Settings.networkAutoReconnect = net["auto_reconnect"].get<bool>();
                if (net.contains("reconnect_attempts")) m_Settings.networkReconnectAttempts = net["reconnect_attempts"].get<int>();
                if (net.contains("reconnect_delay_seconds")) m_Settings.networkReconnectDelaySeconds = net["reconnect_delay_seconds"].get<float>();
                if (net.contains("resume_timeout_seconds")) m_Settings.networkResumeTimeoutSeconds = net["resume_timeout_seconds"].get<float>();
                if (net.contains("max_messages_per_second")) m_Settings.networkMaxMessagesPerSecond = net["max_messages_per_second"].get<int>();
                if (net.contains("max_bytes_per_second")) m_Settings.networkMaxBytesPerSecond = net["max_bytes_per_second"].get<int>();
            }

            // Editor settings
            if (json.contains("editor")) {
                const auto& editor = json["editor"];
                if (editor.contains("max_undo_steps")) m_Settings.maxUndoSteps = editor["max_undo_steps"].get<int>();
            }

            // Rendering settings (default font/theme, background clear color)
            if (json.contains("rendering")) {
                const auto& rendering = json["rendering"];
                if (rendering.contains("default_font")) {
                    m_Settings.defaultFont = rendering["default_font"].get<std::string>();
                }
                if (rendering.contains("default_theme")) {
                    m_Settings.defaultTheme = rendering["default_theme"].get<std::string>();
                }
                if (rendering.contains("clear_color") && rendering["clear_color"].is_array() &&
                    rendering["clear_color"].size() >= 4) {
                    m_Settings.clearColor.r = rendering["clear_color"][0].get<float>();
                    m_Settings.clearColor.g = rendering["clear_color"][1].get<float>();
                    m_Settings.clearColor.b = rendering["clear_color"][2].get<float>();
                    m_Settings.clearColor.a = rendering["clear_color"][3].get<float>();
                }
            }
        } else if (json.contains("settings")) {

            m_Settings.Deserialize(json["settings"]);
        }

        // Load splash screen settings (handles both formats)
        if (json.contains("splash_screens")) {
            const auto& splash = json["splash_screens"];
            if (splash.contains("enabled")) {
                m_Settings.splashScreenSettings.enabled = splash["enabled"].get<bool>();
            }
            if (splash.contains("allow_skip")) {
                m_Settings.splashScreenSettings.allowSkip = splash["allow_skip"].get<bool>();
            }
            if (splash.contains("entries") && splash["entries"].is_array()) {
                m_Settings.splashScreenSettings.entries.clear();
                for (const auto& entry : splash["entries"]) {
                    SplashScreenEntry se;
                    if (entry.contains("image_path")) {
                        se.imagePath = entry["image_path"].get<std::string>();
                    }
                    if (entry.contains("fade_in_duration")) {
                        se.fadeInDuration = entry["fade_in_duration"].get<float>();
                    }
                    if (entry.contains("hold_duration")) {
                        se.holdDuration = entry["hold_duration"].get<float>();
                    }
                    if (entry.contains("fade_out_duration")) {
                        se.fadeOutDuration = entry["fade_out_duration"].get<float>();
                    }
                    m_Settings.splashScreenSettings.entries.push_back(se);
                }
            }
        }

        m_Loaded = true;

        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(LogCategory::Core, "Project: failed to parse project JSON: {}", e.what());
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

    // Start from the previously loaded file (if any) so editor-only keys that
    // the C++ side does not model - structure, icon, dates, input_map,
    // localization, editor.save_on_play - survive a round-trip untouched.
    nlohmann::json json = nlohmann::json::object();
    if (!m_RawProjectJson.empty()) {
        try {
            json = nlohmann::json::parse(m_RawProjectJson);
        } catch (const std::exception&) {
            json = nlohmann::json::object();
        }
    }
    if (!json.is_object()) {
        json = nlohmann::json::object();
    }

    // Drop the legacy flat container if it is present; we always write nested.
    json.erase("lupine_version");
    json.erase("settings");

    auto scaleModeToString = [](ProjectSettings::ViewportScaleMode mode) -> std::string {
        switch (mode) {
            case ProjectSettings::ViewportScaleMode::Letterbox: return "letterbox";
            case ProjectSettings::ViewportScaleMode::Stretch:   return "stretch";
            case ProjectSettings::ViewportScaleMode::Crop:      return "crop";
            case ProjectSettings::ViewportScaleMode::Ignore:    return "ignore";
        }
        return "letterbox";
    };
    auto filteringToString = [](ProjectSettings::TextureFiltering filter) -> std::string {
        switch (filter) {
            case ProjectSettings::TextureFiltering::NearestNeighbor: return "nearest";
            case ProjectSettings::TextureFiltering::Bilinear:        return "bilinear";
            case ProjectSettings::TextureFiltering::Cubic:           return "cubic";
        }
        return "bilinear";
    };

    json["project"]["name"] = m_Settings.projectName;
    json["project"]["creator"] = m_Settings.creatorName;
    json["project"]["version"] = m_Settings.version;
    json["project"]["main_scene"] = m_Settings.mainScene;

    json["window"]["width"] = m_Settings.windowWidth;
    json["window"]["height"] = m_Settings.windowHeight;
    json["window"]["fullscreen"] = m_Settings.fullscreen;
    json["window"]["vsync"] = m_Settings.vsync;
    json["window"]["resizable"] = m_Settings.windowResizable;
    json["window"]["borderless"] = m_Settings.windowBorderless;
    json["window"]["target_fps"] = m_Settings.targetFrameRate;
    json["window"]["size_override"] = m_Settings.windowSizeOverride;
    json["window"]["override_width"] = m_Settings.windowSizeOverrideWidth;
    json["window"]["override_height"] = m_Settings.windowSizeOverrideHeight;

    json["graphics"]["scale_mode"] = scaleModeToString(m_Settings.viewportScaleMode);
    json["graphics"]["texture_filtering"] = filteringToString(m_Settings.textureFiltering);

    json["physics"]["physics_tick_rate"] = m_Settings.physicsTickRate;
    json["physics"]["gravity_2d"] = { m_Settings.gravity2D.x, m_Settings.gravity2D.y };
    json["physics"]["gravity_3d"] = { m_Settings.gravity3D.x, m_Settings.gravity3D.y, m_Settings.gravity3D.z };
    json["physics"]["collision_layers_2d"] = m_Settings.collisionLayers2D;
    json["physics"]["collision_layers_3d"] = m_Settings.collisionLayers3D;

    json["audio"]["master_volume"] = m_Settings.masterVolume;
    json["audio"]["music_volume"] = m_Settings.musicVolume;
    json["audio"]["sfx_volume"] = m_Settings.sfxVolume;

    json["rendering"]["default_font"] = m_Settings.defaultFont;
    json["rendering"]["default_theme"] = m_Settings.defaultTheme;
    json["rendering"]["clear_color"] = {
        m_Settings.clearColor.r, m_Settings.clearColor.g,
        m_Settings.clearColor.b, m_Settings.clearColor.a
    };

    json["editor"]["max_undo_steps"] = m_Settings.maxUndoSteps;

    nlohmann::json splashJson;
    splashJson["enabled"] = m_Settings.splashScreenSettings.enabled;
    splashJson["allow_skip"] = m_Settings.splashScreenSettings.allowSkip;
    nlohmann::json entriesJson = nlohmann::json::array();
    for (const auto& entry : m_Settings.splashScreenSettings.entries) {
        nlohmann::json entryJson;
        entryJson["image_path"] = entry.imagePath;
        entryJson["fade_in_duration"] = entry.fadeInDuration;
        entryJson["hold_duration"] = entry.holdDuration;
        entryJson["fade_out_duration"] = entry.fadeOutDuration;
        entriesJson.push_back(entryJson);
    }
    splashJson["entries"] = entriesJson;
    json["splash_screens"] = splashJson;

    std::string jsonStr = json.dump(2);
    auto result = platform::FileSystem::WriteFile(projectPath, jsonStr);

    if (!result.success) {

        return false;
    }

    m_RawProjectJson = jsonStr;
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

