#include "lupine/import/GodotSceneImporter.hpp"
#include "lupine/core/Core.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Component.hpp"
#include "lupine/core/CameraNodes.hpp"
#include "lupine/components/ComponentRegistry.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/platform/FileSystem.hpp"
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <sstream>
#include <set>

namespace lupine {
namespace import {

namespace fs = std::filesystem;

// ============================================================================
// BlankScriptGenerator Implementation
// ============================================================================

std::string BlankScriptGenerator::GenerateLuaScript(const std::string& className, const std::vector<std::string>& methods) {
    std::stringstream ss;
    ss << "-- " << className << ".lua\n";
    ss << "-- STUB generated from a Godot GDScript import.\n";
    ss << "-- GDScript logic is NOT translated: the class and the original method names are\n";
    ss << "-- recreated below, but every body is empty. Reimplement each method yourself.\n\n";

    ss << "local " << className << " = {}\n";
    ss << className << ".__index = " << className << "\n\n";

    // Constructor
    ss << "function " << className << ":new()\n";
    ss << "    local instance = setmetatable({}, " << className << ")\n";
    ss << "    return instance\n";
    ss << "end\n\n";

    // Standard lifecycle methods
    ss << "function " << className << ":_ready()\n";
    ss << "    -- Called when the node enters the scene tree\n";
    ss << "end\n\n";

    ss << "function " << className << ":_process(delta)\n";
    ss << "    -- Called every frame\n";
    ss << "end\n\n";

    ss << "function " << className << ":_physics_process(delta)\n";
    ss << "    -- Called every physics frame\n";
    ss << "end\n\n";

    // Custom methods from GDScript (names preserved, bodies not translated)
    for (const auto& method : methods) {
        ss << "function " << className << ":" << method << "()\n";
        ss << "    -- TODO: Reimplement " << method << " (original GDScript body not translated)\n";
        ss << "end\n\n";
    }

    ss << "return " << className << "\n";
    return ss.str();
}

std::string BlankScriptGenerator::GeneratePythonScript(const std::string& className, const std::vector<std::string>& methods) {
    std::stringstream ss;
    ss << "# " << className << ".py\n";
    ss << "# STUB generated from a Godot GDScript import.\n";
    ss << "# GDScript logic is NOT translated: the class and the original method names are\n";
    ss << "# recreated below, but every body is empty. Reimplement each method yourself.\n\n";

    ss << "class " << className << ":\n";
    ss << "    def __init__(self):\n";
    ss << "        pass\n\n";

    ss << "    def _ready(self):\n";
    ss << "        \"\"\"Called when the node enters the scene tree\"\"\"\n";
    ss << "        pass\n\n";

    ss << "    def _process(self, delta):\n";
    ss << "        \"\"\"Called every frame\"\"\"\n";
    ss << "        pass\n\n";

    ss << "    def _physics_process(self, delta):\n";
    ss << "        \"\"\"Called every physics frame\"\"\"\n";
    ss << "        pass\n\n";

    for (const auto& method : methods) {
        ss << "    def " << method << "(self):\n";
        ss << "        \"\"\"TODO: Reimplement " << method << " (original GDScript body not translated)\"\"\"\n";
        ss << "        pass\n\n";
    }

    return ss.str();
}

std::string BlankScriptGenerator::GenerateMrubyScript(const std::string& className, const std::vector<std::string>& methods) {
    std::stringstream ss;
    ss << "# " << className << ".rb\n";
    ss << "# STUB generated from a Godot GDScript import.\n";
    ss << "# GDScript logic is NOT translated: the class and the original method names are\n";
    ss << "# recreated below, but every body is empty. Reimplement each method yourself.\n\n";

    ss << "class " << className << "\n";
    ss << "  def initialize\n";
    ss << "    # Constructor\n";
    ss << "  end\n\n";

    ss << "  def _ready\n";
    ss << "    # Called when the node enters the scene tree\n";
    ss << "  end\n\n";

    ss << "  def _process(delta)\n";
    ss << "    # Called every frame\n";
    ss << "  end\n\n";

    ss << "  def _physics_process(delta)\n";
    ss << "    # Called every physics frame\n";
    ss << "  end\n\n";

    for (const auto& method : methods) {
        ss << "  def " << method << "\n";
        ss << "    # TODO: Reimplement " << method << " (original GDScript body not translated)\n";
        ss << "  end\n\n";
    }

    ss << "end\n";
    return ss.str();
}

std::string BlankScriptGenerator::GetScriptExtension(const std::string& language) {
    std::string lang = language;
    std::transform(lang.begin(), lang.end(), lang.begin(), ::tolower);

    if (lang == "lua") return ".lua";
    if (lang == "python" || lang == "py") return ".py";
    if (lang == "mruby" || lang == "ruby" || lang == "rb") return ".rb";
    return ".lua";  // Default
}

std::vector<std::string> BlankScriptGenerator::GetAvailableLanguages() {
    return {"Lua", "Python", "mruby"};
}

// ============================================================================
// GodotSceneImporter Implementation
// ============================================================================

GodotSceneImporter::GodotSceneImporter() {
    InitializeMappings();
}

GodotSceneImporter::~GodotSceneImporter() {
}

void GodotSceneImporter::InitializeMappings() {
    m_NodeMappings.clear();
    m_MappingIndex.clear();

    // ========== Base Node Types ==========
    m_NodeMappings.push_back({"Node", "Node", {}, nullptr, "Base node - direct mapping"});
    m_NodeMappings.push_back({"Node2D", "Node2D", {}, nullptr, "2D node - direct mapping"});
    m_NodeMappings.push_back({"Node3D", "Node3D", {}, nullptr, "3D node - direct mapping (Godot 4)"});
    m_NodeMappings.push_back({"Spatial", "Node3D", {}, nullptr, "3D node - direct mapping (Godot 3)"});

    // ========== 2D Rendering ==========
    m_NodeMappings.push_back({"Sprite2D", "Node2D", {"Sprite2D"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertSprite2D(gn, ln, GodotScene()); },
        "2D sprite with texture"});
    m_NodeMappings.push_back({"Sprite", "Node2D", {"Sprite2D"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertSprite2D(gn, ln, GodotScene()); },
        "2D sprite (Godot 3)"});
    m_NodeMappings.push_back({"AnimatedSprite2D", "Node2D", {"AnimatedSprite2D"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertAnimatedSprite2D(gn, ln, GodotScene()); },
        "Animated 2D sprite"});
    m_NodeMappings.push_back({"AnimatedSprite", "Node2D", {"AnimatedSprite2D"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertAnimatedSprite2D(gn, ln, GodotScene()); },
        "Animated sprite (Godot 3)"});
    m_NodeMappings.push_back({"TileMap", "Node2D", {"TileMap2D"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertTileMap(gn, ln, GodotScene()); },
        "2D tile map - partial support"});
    m_NodeMappings.push_back({"Line2D", "Node2D", {"Line2D"}, nullptr, "2D line drawing"});
    m_NodeMappings.push_back({"Polygon2D", "Node2D", {"Shape2D"}, nullptr, "2D polygon - maps to Shape2D"});

    // ========== 3D Rendering ==========
    m_NodeMappings.push_back({"Sprite3D", "Node3D", {"Sprite3D"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertSprite3D(gn, ln, GodotScene()); },
        "3D sprite billboard"});
    m_NodeMappings.push_back({"AnimatedSprite3D", "Node3D", {"AnimatedSprite3D"}, nullptr, "3D animated sprite"});
    m_NodeMappings.push_back({"MeshInstance3D", "Node3D", {"StaticMesh3D"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertMeshInstance3D(gn, ln, GodotScene()); },
        "3D mesh instance"});
    m_NodeMappings.push_back({"MeshInstance", "Node3D", {"StaticMesh3D"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertMeshInstance3D(gn, ln, GodotScene()); },
        "3D mesh (Godot 3)"});
    m_NodeMappings.push_back({"CSGBox3D", "Node3D", {"PrimitiveMesh3D"}, nullptr, "CSG box - converts to primitive"});
    m_NodeMappings.push_back({"CSGSphere3D", "Node3D", {"PrimitiveMesh3D"}, nullptr, "CSG sphere - converts to primitive"});
    m_NodeMappings.push_back({"CSGCylinder3D", "Node3D", {"PrimitiveMesh3D"}, nullptr, "CSG cylinder - converts to primitive"});
    m_NodeMappings.push_back({"MultiMeshInstance3D", "Node3D", {"MultiMeshGeneric"}, nullptr, "Multi-mesh instancing"});

    // ========== Cameras ==========
    m_NodeMappings.push_back({"Camera2D", "Camera2D", {},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertCamera2D(gn, ln, GodotScene()); },
        "2D camera"});
    m_NodeMappings.push_back({"Camera3D", "Camera3D", {},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertCamera3D(gn, ln, GodotScene()); },
        "3D camera"});
    m_NodeMappings.push_back({"Camera", "Camera3D", {},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertCamera3D(gn, ln, GodotScene()); },
        "Camera (Godot 3)"});

    // ========== Lights ==========
    m_NodeMappings.push_back({"PointLight2D", "Node2D", {"Light2D"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertLight2D(gn, ln, GodotScene()); },
        "2D point light"});
    m_NodeMappings.push_back({"Light2D", "Node2D", {"Light2D"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertLight2D(gn, ln, GodotScene()); },
        "2D light (Godot 3)"});
    m_NodeMappings.push_back({"DirectionalLight3D", "Node3D", {"DirectionalLight3D"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertDirectionalLight3D(gn, ln, GodotScene()); },
        "Directional light"});
    m_NodeMappings.push_back({"DirectionalLight", "Node3D", {"DirectionalLight3D"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertDirectionalLight3D(gn, ln, GodotScene()); },
        "Directional light (Godot 3)"});
    m_NodeMappings.push_back({"OmniLight3D", "Node3D", {"OmniLight3D"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertOmniLight3D(gn, ln, GodotScene()); },
        "Omni/point light"});
    m_NodeMappings.push_back({"OmniLight", "Node3D", {"OmniLight3D"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertOmniLight3D(gn, ln, GodotScene()); },
        "Omni light (Godot 3)"});
    m_NodeMappings.push_back({"SpotLight3D", "Node3D", {"SpotLight3D"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertSpotLight3D(gn, ln, GodotScene()); },
        "Spotlight"});
    m_NodeMappings.push_back({"SpotLight", "Node3D", {"SpotLight3D"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertSpotLight3D(gn, ln, GodotScene()); },
        "Spotlight (Godot 3)"});

    // ========== Physics 2D ==========
    m_NodeMappings.push_back({"RigidBody2D", "Node2D", {"RigidBody2DComponent"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertRigidBody2D(gn, ln, GodotScene()); },
        "2D rigid body"});
    m_NodeMappings.push_back({"StaticBody2D", "Node2D", {"StaticBody2DComponent"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertStaticBody2D(gn, ln, GodotScene()); },
        "2D static body"});
    m_NodeMappings.push_back({"CharacterBody2D", "Node2D", {"CharacterController2D"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertCharacterBody2D(gn, ln, GodotScene()); },
        "2D character controller"});
    m_NodeMappings.push_back({"KinematicBody2D", "Node2D", {"KinematicBody2DComponent"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertCharacterBody2D(gn, ln, GodotScene()); },
        "2D kinematic body (Godot 3)"});
    m_NodeMappings.push_back({"Area2D", "Node2D", {"AreaTrigger2DComponent"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertArea2D(gn, ln, GodotScene()); },
        "2D area trigger"});
    m_NodeMappings.push_back({"CollisionShape2D", "Node2D", {"CollisionBody2DComponent"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertCollisionShape2D(gn, ln, GodotScene()); },
        "2D collision shape"});
    m_NodeMappings.push_back({"CollisionPolygon2D", "Node2D", {"CollisionBody2DComponent"}, nullptr, "2D collision polygon"});

    // ========== Physics 3D ==========
    m_NodeMappings.push_back({"RigidBody3D", "Node3D", {"RigidBody3DComponent"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertRigidBody3D(gn, ln, GodotScene()); },
        "3D rigid body"});
    m_NodeMappings.push_back({"RigidBody", "Node3D", {"RigidBody3DComponent"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertRigidBody3D(gn, ln, GodotScene()); },
        "3D rigid body (Godot 3)"});
    m_NodeMappings.push_back({"StaticBody3D", "Node3D", {"StaticBody3DComponent"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertStaticBody3D(gn, ln, GodotScene()); },
        "3D static body"});
    m_NodeMappings.push_back({"StaticBody", "Node3D", {"StaticBody3DComponent"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertStaticBody3D(gn, ln, GodotScene()); },
        "3D static body (Godot 3)"});
    m_NodeMappings.push_back({"CharacterBody3D", "Node3D", {"CharacterController3D"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertCharacterBody3D(gn, ln, GodotScene()); },
        "3D character controller"});
    m_NodeMappings.push_back({"KinematicBody", "Node3D", {"KinematicBody3DComponent"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertCharacterBody3D(gn, ln, GodotScene()); },
        "3D kinematic body (Godot 3)"});
    m_NodeMappings.push_back({"Area3D", "Node3D", {"AreaTrigger3DComponent"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertArea3D(gn, ln, GodotScene()); },
        "3D area trigger"});
    m_NodeMappings.push_back({"Area", "Node3D", {"AreaTrigger3DComponent"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertArea3D(gn, ln, GodotScene()); },
        "3D area (Godot 3)"});
    m_NodeMappings.push_back({"CollisionShape3D", "Node3D", {"CollisionMesh3DComponent"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertCollisionShape3D(gn, ln, GodotScene()); },
        "3D collision shape"});
    m_NodeMappings.push_back({"CollisionShape", "Node3D", {"CollisionMesh3DComponent"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertCollisionShape3D(gn, ln, GodotScene()); },
        "3D collision shape (Godot 3)"});

    // ========== UI / Control ==========
    m_NodeMappings.push_back({"Control", "Node2D", {},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertControl(gn, ln, GodotScene()); },
        "Base UI control"});
    m_NodeMappings.push_back({"Label", "Node2D", {"Label"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertLabel(gn, ln, GodotScene()); },
        "Text label"});
    m_NodeMappings.push_back({"RichTextLabel", "Node2D", {"Label"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertLabel(gn, ln, GodotScene()); },
        "Rich text label - basic support"});
    m_NodeMappings.push_back({"Button", "Node2D", {"Button"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertButton(gn, ln, GodotScene()); },
        "Button"});
    m_NodeMappings.push_back({"TextureButton", "Node2D", {"TextureButton"}, nullptr, "Texture button"});
    m_NodeMappings.push_back({"CheckBox", "Node2D", {"Checkbox"}, nullptr, "Checkbox"});
    m_NodeMappings.push_back({"CheckButton", "Node2D", {"ToggleButton"}, nullptr, "Toggle button"});
    m_NodeMappings.push_back({"ColorRect", "Node2D", {"ColorRect"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertColorRect(gn, ln, GodotScene()); },
        "Colored rectangle"});
    m_NodeMappings.push_back({"TextureRect", "Node2D", {"Image2D"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertTextureRect(gn, ln, GodotScene()); },
        "Texture rectangle"});
    m_NodeMappings.push_back({"Panel", "Node2D", {"Panel"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertPanel(gn, ln, GodotScene()); },
        "UI panel"});
    m_NodeMappings.push_back({"PanelContainer", "Node2D", {"Panel"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertPanel(gn, ln, GodotScene()); },
        "Panel container"});
    m_NodeMappings.push_back({"ProgressBar", "Node2D", {"ProgressBar"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertProgressBar(gn, ln, GodotScene()); },
        "Progress bar"});

    // ========== Containers ==========
    m_NodeMappings.push_back({"Container", "Node2D", {"Container"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertContainer(gn, ln, GodotScene()); },
        "Base container"});
    m_NodeMappings.push_back({"HBoxContainer", "Node2D", {"HorizontalContainer"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertContainer(gn, ln, GodotScene()); },
        "Horizontal container"});
    m_NodeMappings.push_back({"VBoxContainer", "Node2D", {"VerticalContainer"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertContainer(gn, ln, GodotScene()); },
        "Vertical container"});
    m_NodeMappings.push_back({"GridContainer", "Node2D", {"GridContainer"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertContainer(gn, ln, GodotScene()); },
        "Grid container"});
    m_NodeMappings.push_back({"CenterContainer", "Node2D", {"CenterContainer"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertContainer(gn, ln, GodotScene()); },
        "Center container"});
    m_NodeMappings.push_back({"MarginContainer", "Node2D", {"PaddingContainer"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertContainer(gn, ln, GodotScene()); },
        "Margin/padding container"});

    // ========== Audio ==========
    m_NodeMappings.push_back({"AudioStreamPlayer", "Node", {"AudioPlayer"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertAudioStreamPlayer(gn, ln, GodotScene()); },
        "Audio player (non-spatial)"});
    m_NodeMappings.push_back({"AudioStreamPlayer2D", "Node2D", {"AudioPlayer"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertAudioStreamPlayer(gn, ln, GodotScene()); },
        "2D positional audio"});
    m_NodeMappings.push_back({"AudioStreamPlayer3D", "Node3D", {"AudioPlayer"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertAudioStreamPlayer(gn, ln, GodotScene()); },
        "3D positional audio"});
    m_NodeMappings.push_back({"AudioListener2D", "Node2D", {"AudioListener"}, nullptr, "2D audio listener"});
    m_NodeMappings.push_back({"AudioListener3D", "Node3D", {"AudioListener"}, nullptr, "3D audio listener"});

    // ========== Utility ==========
    m_NodeMappings.push_back({"Timer", "Node", {"Timer"},
        [this](const GodotNode& gn, std::shared_ptr<core::Node> ln) { ConvertTimer(gn, ln, GodotScene()); },
        "Timer"});
    m_NodeMappings.push_back({"CanvasLayer", "Node2D", {}, nullptr, "Canvas layer - partial support"});
    m_NodeMappings.push_back({"ParallaxBackground", "Node2D", {}, nullptr, "Parallax background - not directly supported"});
    m_NodeMappings.push_back({"ParallaxLayer", "Node2D", {}, nullptr, "Parallax layer - not directly supported"});
    m_NodeMappings.push_back({"WorldEnvironment", "Node3D", {"WorldEnvironment"}, nullptr, "World environment"});
    m_NodeMappings.push_back({"Path2D", "Node2D", {"Path2D"}, nullptr, "2D path"});
    m_NodeMappings.push_back({"Path3D", "Node3D", {}, nullptr, "3D path - not directly supported"});
    m_NodeMappings.push_back({"PathFollow2D", "Node2D", {}, nullptr, "Path follower - not directly supported"});
    m_NodeMappings.push_back({"PathFollow3D", "Node3D", {}, nullptr, "Path follower - not directly supported"});
    m_NodeMappings.push_back({"RemoteTransform2D", "Node2D", {}, nullptr, "Remote transform - not supported"});
    m_NodeMappings.push_back({"RemoteTransform3D", "Node3D", {}, nullptr, "Remote transform - not supported"});
    m_NodeMappings.push_back({"VisibleOnScreenNotifier2D", "Node2D", {}, nullptr, "Visibility notifier - not supported"});
    m_NodeMappings.push_back({"VisibleOnScreenNotifier3D", "Node3D", {}, nullptr, "Visibility notifier - not supported"});

    // ========== Particles ==========
    m_NodeMappings.push_back({"GPUParticles2D", "Node2D", {}, nullptr, "GPU particles 2D - not supported"});
    m_NodeMappings.push_back({"CPUParticles2D", "Node2D", {}, nullptr, "CPU particles 2D - not supported"});
    m_NodeMappings.push_back({"GPUParticles3D", "Node3D", {}, nullptr, "GPU particles 3D - not supported"});
    m_NodeMappings.push_back({"CPUParticles3D", "Node3D", {}, nullptr, "CPU particles 3D - not supported"});
    m_NodeMappings.push_back({"Particles2D", "Node2D", {}, nullptr, "Particles (Godot 3) - not supported"});
    m_NodeMappings.push_back({"Particles", "Node3D", {}, nullptr, "Particles (Godot 3) - not supported"});

    // ========== Navigation ==========
    m_NodeMappings.push_back({"NavigationRegion2D", "Node2D", {}, nullptr, "Navigation region - not supported"});
    m_NodeMappings.push_back({"NavigationRegion3D", "Node3D", {}, nullptr, "Navigation region - not supported"});
    m_NodeMappings.push_back({"NavigationAgent2D", "Node2D", {}, nullptr, "Navigation agent - not supported"});
    m_NodeMappings.push_back({"NavigationAgent3D", "Node3D", {}, nullptr, "Navigation agent - not supported"});

    // Build index for fast lookup
    for (size_t i = 0; i < m_NodeMappings.size(); i++) {
        m_MappingIndex[m_NodeMappings[i].godotType] = i;
    }
}

const NodeMapping* GodotSceneImporter::GetMapping(const std::string& godotType) const {
    auto it = m_MappingIndex.find(godotType);
    if (it != m_MappingIndex.end()) {
        return &m_NodeMappings[it->second];
    }
    return nullptr;
}

bool GodotSceneImporter::HasMapping(const std::string& godotType) const {
    return m_MappingIndex.find(godotType) != m_MappingIndex.end();
}

std::vector<std::string> GodotSceneImporter::GetSupportedGodotTypes() const {
    std::vector<std::string> types;
    for (const auto& mapping : m_NodeMappings) {
        types.push_back(mapping.godotType);
    }
    return types;
}

std::vector<std::string> GodotSceneImporter::GetUnsupportedGodotTypes(const GodotScene& scene) const {
    std::vector<std::string> unsupported;
    std::set<std::string> seen;

    for (const auto& node : scene.nodes) {
        if (!node.type.empty() && !HasMapping(node.type) && seen.find(node.type) == seen.end()) {
            unsupported.push_back(node.type);
            seen.insert(node.type);
        }
    }

    return unsupported;
}

ImportResult GodotSceneImporter::Import(const std::string& filepath) {
    GodotSceneParser parser;
    auto parsed = parser.ParseFile(filepath);

    if (!parsed) {
        ImportResult result;
        result.success = false;
        result.errors.push_back(parser.GetLastError());
        return result;
    }

    return Import(*parsed);
}

ImportResult GodotSceneImporter::Import(const GodotScene& scene) {
    ImportResult result;
    result.success = true;

    // Create new Lupine scene
    result.scene = std::make_shared<core::Scene>();

    // Extract scene name from root node or file
    std::string sceneName = "ImportedScene";
    if (scene.rootNode) {
        sceneName = scene.rootNode->name;
    }
    result.scene->SetName(sceneName);

    // Collect assets that need importing
    for (const auto& ext : scene.extResources) {
        result.assets.push_back(CreateAssetEntry(ext, scene));
        result.totalAssets++;
    }

    // Resolve asset paths
    ResolveAssets(result.assets);

    // Count resolved assets
    for (const auto& asset : result.assets) {
        if (asset.status == ImportAsset::Status::Resolved) {
            result.resolvedAssets++;
        } else if (asset.status == ImportAsset::Status::Missing) {
            result.missingAssets++;
        }
    }

    // Collect referenced (instanced) scenes
    std::set<std::string> seenScenes;
    for (const auto& node : scene.nodes) {
        if (!node.instance.empty()) {
            std::string instanceStr = node.instance;
            size_t startQuote = instanceStr.find('"');
            size_t endQuote = instanceStr.rfind('"');
            if (startQuote != std::string::npos && endQuote != std::string::npos && endQuote > startQuote) {
                std::string resourceId = instanceStr.substr(startQuote + 1, endQuote - startQuote - 1);
                for (const auto& ext : scene.extResources) {
                    if (ext.id == resourceId && ext.type == "PackedScene") {
                        if (seenScenes.find(ext.path) == seenScenes.end()) {
                            seenScenes.insert(ext.path);
                            result.referencedScenes.push_back(ext.path);
                        }
                        break;
                    }
                }
            }
        }
    }

    // Convert nodes recursively
    if (scene.rootNode) {
        auto rootNode = ConvertNode(*scene.rootNode, scene, result);
        if (rootNode) {
            result.scene->SetRoot(rootNode);

            // Recursively convert children
            std::function<void(const GodotNode*, std::shared_ptr<core::Node>)> convertChildren;
            convertChildren = [&](const GodotNode* godotParent, std::shared_ptr<core::Node> lupineParent) {
                for (const auto* childPtr : godotParent->children) {
                    auto childNode = ConvertNode(*childPtr, scene, result);
                    if (childNode) {
                        lupineParent->AddChild(childNode);
                        convertChildren(childPtr, childNode);
                    }
                }
            };

            convertChildren(scene.rootNode, rootNode);
        }
    }

    return result;
}

ImportResult GodotSceneImporter::Analyze(const std::string& filepath) {
    GodotSceneParser parser;
    auto parsed = parser.ParseFile(filepath);

    if (!parsed) {
        ImportResult result;
        result.success = false;
        result.errors.push_back(parser.GetLastError());
        return result;
    }

    ImportResult result;
    result.success = true;

    // Collect assets
    for (const auto& ext : parsed->extResources) {
        result.assets.push_back(CreateAssetEntry(ext, *parsed));
        result.totalAssets++;
    }

    ResolveAssets(result.assets);

    // Count assets
    for (const auto& asset : result.assets) {
        if (asset.status == ImportAsset::Status::Resolved) {
            result.resolvedAssets++;
        } else if (asset.status == ImportAsset::Status::Missing) {
            result.missingAssets++;
        }
    }

    // Analyze nodes
    result.totalNodes = static_cast<int>(parsed->nodes.size());

    // Track seen referenced scenes to avoid duplicates
    std::set<std::string> seenScenes;

    for (const auto& node : parsed->nodes) {
        // Check for instanced scenes
        if (!node.instance.empty()) {
            // node.instance is like "ExtResource("1_abcde")" or just an ext resource reference
            // We need to resolve it to a path
            std::string instanceStr = node.instance;

            // Try to find the resource ID from the instance string
            // Format: ExtResource("id") or ExtResource( "id" )
            size_t startQuote = instanceStr.find('"');
            size_t endQuote = instanceStr.rfind('"');
            if (startQuote != std::string::npos && endQuote != std::string::npos && endQuote > startQuote) {
                std::string resourceId = instanceStr.substr(startQuote + 1, endQuote - startQuote - 1);

                // Find the external resource with this ID
                for (const auto& ext : parsed->extResources) {
                    if (ext.id == resourceId && ext.type == "PackedScene") {
                        std::string scenePath = ext.path;
                        if (seenScenes.find(scenePath) == seenScenes.end()) {
                            seenScenes.insert(scenePath);
                            result.referencedScenes.push_back(scenePath);
                        }
                        break;
                    }
                }
            }
        }

        if (node.type.empty()) {
            // Instance or script-only node
            result.partialNodes++;
            continue;
        }

        const NodeMapping* mapping = GetMapping(node.type);
        if (mapping) {
            if (mapping->components.empty() && mapping->customConverter == nullptr) {
                result.partialNodes++;
            } else {
                result.convertedNodes++;
            }
        } else {
            result.skippedNodes++;

            UnmappedNode unmapped;
            unmapped.godotType = node.type;
            unmapped.nodeName = node.name;

            // Build path
            if (node.parent == ".") {
                unmapped.nodePath = node.name;
            } else if (!node.parent.empty()) {
                unmapped.nodePath = node.parent + "/" + node.name;
            } else {
                unmapped.nodePath = node.name;
            }

            // Suggest a base type
            if (node.type.find("2D") != std::string::npos || node.type.find("2d") != std::string::npos) {
                unmapped.suggestedLupineType = "Node2D";
            } else if (node.type.find("3D") != std::string::npos || node.type.find("3d") != std::string::npos) {
                unmapped.suggestedLupineType = "Node3D";
            } else {
                unmapped.suggestedLupineType = "Node";
            }

            unmapped.notes = "No direct equivalent - will create base node";
            result.unmappedNodes.push_back(unmapped);
        }
    }

    // Add parser warnings
    for (const auto& warning : parser.GetWarnings()) {
        result.warnings.push_back(warning);
    }

    return result;
}

std::shared_ptr<core::Node> GodotSceneImporter::ConvertNode(const GodotNode& godotNode, const GodotScene& scene, ImportResult& result) {
    result.totalNodes++;

    std::string nodeType = "Node";
    const NodeMapping* mapping = nullptr;

    if (!godotNode.type.empty()) {
        mapping = GetMapping(godotNode.type);
        if (mapping) {
            nodeType = mapping->lupineNodeType;
            result.convertedNodes++;
        } else {
            // No mapping - create base node
            if (godotNode.type.find("2D") != std::string::npos || godotNode.type.find("2d") != std::string::npos) {
                nodeType = "Node2D";
            } else if (godotNode.type.find("3D") != std::string::npos || godotNode.type.find("3d") != std::string::npos) {
                nodeType = "Node3D";
            }
            result.partialNodes++;

            result.warnings.push_back("No mapping for Godot type '" + godotNode.type + "' - created " + nodeType);
        }
    }

    // Create the Lupine node
    std::shared_ptr<core::Node> lupineNode;

    if (nodeType == "Node2D") {
        lupineNode = std::make_shared<core::Node2D>(godotNode.name);
    } else if (nodeType == "Node3D") {
        lupineNode = std::make_shared<core::Node3D>(godotNode.name);
    } else if (nodeType == "Camera2D") {
        lupineNode = std::make_shared<core::Camera2D>(godotNode.name);
    } else if (nodeType == "Camera3D") {
        lupineNode = std::make_shared<core::Camera3D>(godotNode.name);
    } else {
        lupineNode = std::make_shared<core::Node>(godotNode.name);
    }

    // Convert transform properties
    ConvertNodeProperties(godotNode, lupineNode, scene);

    // Add components if mapping exists
    if (mapping) {
        for (const auto& componentType : mapping->components) {
            auto component = CreateComponent(componentType);
            if (component) {
                lupineNode->AddComponent(component);
            }
        }

        // Call custom converter if available
        if (mapping->customConverter) {
            mapping->customConverter(godotNode, lupineNode);
        }
    }

    // Convert component properties
    if (mapping && !mapping->components.empty()) {
        for (auto& component : lupineNode->GetComponents()) {
            ConvertComponentProperties(godotNode, component, scene);
        }
    }

    return lupineNode;
}

void GodotSceneImporter::ConvertNodeProperties(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene&) {
    // Handle 2D transforms
    if (auto node2D = std::dynamic_pointer_cast<core::Node2D>(lupineNode)) {
        // First try direct position property (used by Node2D, Sprite2D, etc.)
        auto [posX, posY] = GetVector2Property(godotNode, "position", 0, 0);

        // For UI Controls (Label, Button, Panel, etc.), Godot uses anchors + offsets
        // Anchors are normalized (0-1) positions relative to the parent
        // Offsets are pixel values from the anchor position
        if (posX == 0 && posY == 0) {
            // Check if this is a Control-based node with offset properties
            double offsetLeft = GetFloatProperty(godotNode, "offset_left", 0.0);
            double offsetTop = GetFloatProperty(godotNode, "offset_top", 0.0);

            // Read anchor values (0-1 normalized relative to parent)
            double anchorLeft = GetFloatProperty(godotNode, "anchor_left", 0.0);
            double anchorTop = GetFloatProperty(godotNode, "anchor_top", 0.0);
            double anchorRight = GetFloatProperty(godotNode, "anchor_right", 0.0);
            double anchorBottom = GetFloatProperty(godotNode, "anchor_bottom", 0.0);

            // Check if this is a "fill parent" control (anchors 0,0,1,1 with zero offsets)
            // These should be positioned at (0,0) relative to parent
            bool isFillParent = (anchorLeft == 0.0 && anchorTop == 0.0 &&
                                 anchorRight == 1.0 && anchorBottom == 1.0);

            // Check if anchors are all the same (single-point anchor)
            // e.g., anchor_left = anchor_right = 0.5 means centered horizontally
            bool isSinglePointAnchor = (anchorLeft == anchorRight && anchorTop == anchorBottom);

            if (isFillParent && offsetLeft == 0 && offsetTop == 0) {
                // Fill parent - position at (0,0)
                posX = 0;
                posY = 0;
            } else if (isSinglePointAnchor) {
                // Single-point anchor (like center-bottom: 0.5, 1.0)
                // Just use offsets directly - Lupine doesn't have anchor system
                // The offset is the position relative to where the anchor would be
                posX = offsetLeft;
                posY = offsetTop;
            } else {
                // Complex anchor setup - use offset_left/top as position
                posX = offsetLeft;
                posY = offsetTop;
            }

            LOG_DEBUG(LogCategory::Asset, "ConvertNodeProperties '{}': anchors=({},{},{},{}), offsets=({},{}), isFill={}, isSinglePoint={}, pos=({},{})",
                      godotNode.name, anchorLeft, anchorTop, anchorRight, anchorBottom,
                      offsetLeft, offsetTop, isFillParent, isSinglePointAnchor, posX, posY);
        }

        node2D->SetPosition(math::Vec2(static_cast<float>(posX), static_cast<float>(posY)));

        double rotation = GetFloatProperty(godotNode, "rotation", 0.0);
        node2D->SetRotation(static_cast<float>(rotation));

        auto [scaleX, scaleY] = GetVector2Property(godotNode, "scale", 1, 1);
        node2D->SetScale(math::Vec2(static_cast<float>(scaleX), static_cast<float>(scaleY)));

        int zIndex = static_cast<int>(GetIntProperty(godotNode, "z_index", 0));
        node2D->SetZIndex(zIndex);
    }

    // Handle 3D transforms
    if (auto node3D = std::dynamic_pointer_cast<core::Node3D>(lupineNode)) {
        auto [posX, posY, posZ] = GetVector3Property(godotNode, "position", 0, 0, 0);
        // Also check "transform" property for Godot 3
        if (posX == 0 && posY == 0 && posZ == 0) {
            auto [tx, ty, tz] = GetVector3Property(godotNode, "translation", 0, 0, 0);
            posX = tx; posY = ty; posZ = tz;
        }
        node3D->SetPosition(math::Vec3(static_cast<float>(posX), static_cast<float>(posY), static_cast<float>(posZ)));

        auto [scaleX, scaleY, scaleZ] = GetVector3Property(godotNode, "scale", 1, 1, 1);
        node3D->SetScale(math::Vec3(static_cast<float>(scaleX), static_cast<float>(scaleY), static_cast<float>(scaleZ)));

        // Rotation is typically stored as euler angles or quaternion
        auto [rotX, rotY, rotZ] = GetVector3Property(godotNode, "rotation", 0, 0, 0);
        // Convert euler angles to quaternion
        math::Quat quat = math::Quat::FromEuler(static_cast<float>(rotX), static_cast<float>(rotY), static_cast<float>(rotZ));
        node3D->SetRotation(quat);
    }

    // Common properties
    bool visible = GetBoolProperty(godotNode, "visible", true);
    lupineNode->SetVisible(visible);
}

std::shared_ptr<core::Component> GodotSceneImporter::CreateComponent(const std::string& typeName) {
    // Use the type registry to create components
    auto& registry = core::TypeRegistry::GetInstance();
    auto serializable = registry.CreateInstance(typeName);
    return std::dynamic_pointer_cast<core::Component>(serializable);
}

void GodotSceneImporter::ConvertComponentProperties(const GodotNode&, std::shared_ptr<core::Component>, const GodotScene&) {
    // Component-specific property conversion is handled by the custom converters
    // This is a fallback for common properties
}

ImportAsset GodotSceneImporter::CreateAssetEntry(const GodotExtResource& resource, const GodotScene&) {
    ImportAsset asset;
    asset.godotPath = resource.path;
    asset.resourceId = resource.id;
    asset.type = DetermineAssetType(resource.type, resource.path);

    // Determine status based on type
    if (resource.type == "Script" || resource.type == "GDScript") {
        asset.status = ImportAsset::Status::Script;
        asset.scriptLanguage = resource.type;
        asset.createBlankScript = m_Config.createBlankScripts;
        asset.blankScriptLanguage = m_Config.defaultScriptLanguage;
    } else {
        asset.status = ImportAsset::Status::Missing;  // Will be resolved later
    }

    // Convert Godot path to Lupine path
    asset.lupinePath = ConvertToLupinePath(resource.path);

    return asset;
}

void GodotSceneImporter::ResolveAssets(std::vector<ImportAsset>& assets) {
    for (auto& asset : assets) {
        if (asset.status == ImportAsset::Status::Script) {
            continue;  // Scripts handled separately
        }

        // Try to resolve the physical path
        std::string resolved = ResolveGodotPath(asset.godotPath, GodotScene());

        if (!resolved.empty() && fs::exists(resolved)) {
            asset.resolvedPath = resolved;
            asset.status = ImportAsset::Status::Resolved;
        } else {
            asset.status = ImportAsset::Status::Missing;
        }
    }
}

std::string GodotSceneImporter::ResolveGodotPath(const std::string& godotPath, const GodotScene&) {
    // Remove res:// prefix
    std::string relativePath = godotPath;
    if (relativePath.find("res://") == 0) {
        relativePath = relativePath.substr(6);
    }

    // Try Godot project root if set
    if (!m_Config.godotProjectRoot.empty()) {
        fs::path fullPath = fs::path(m_Config.godotProjectRoot) / relativePath;
        if (fs::exists(fullPath)) {
            return fullPath.string();
        }
    }

    // Try search paths
    for (const auto& searchPath : m_Config.assetSearchPaths) {
        fs::path fullPath = fs::path(searchPath) / relativePath;
        if (fs::exists(fullPath)) {
            return fullPath.string();
        }
    }

    return "";
}

std::string GodotSceneImporter::ConvertToLupinePath(const std::string& godotPath) {
    // Remove res:// and convert to res:// format for Lupine
    std::string path = godotPath;
    if (path.find("res://") == 0) {
        path = path.substr(6);
    }
    return "res://" + path;
}

ImportAsset::Type GodotSceneImporter::DetermineAssetType(const std::string& godotType, const std::string& path) {
    // By Godot type
    if (godotType == "Texture2D" || godotType == "Texture" || godotType == "ImageTexture" ||
        godotType == "CompressedTexture2D" || godotType == "AtlasTexture") {
        return ImportAsset::Type::Texture;
    }
    if (godotType == "AudioStream" || godotType == "AudioStreamOGGVorbis" ||
        godotType == "AudioStreamMP3" || godotType == "AudioStreamWAV") {
        return ImportAsset::Type::Audio;
    }
    if (godotType == "PackedScene") {
        return ImportAsset::Type::Scene;
    }
    if (godotType == "Script" || godotType == "GDScript" || godotType == "CSharpScript") {
        return ImportAsset::Type::Script;
    }
    if (godotType == "Mesh" || godotType == "ArrayMesh" || godotType == "PrimitiveMesh") {
        return ImportAsset::Type::Model;
    }
    if (godotType == "Font" || godotType == "FontFile" || godotType == "DynamicFont") {
        return ImportAsset::Type::Font;
    }

    // By extension
    std::string ext = path;
    size_t dotPos = ext.rfind('.');
    if (dotPos != std::string::npos) {
        ext = ext.substr(dotPos);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".webp" || ext == ".bmp" || ext == ".tga") {
            return ImportAsset::Type::Texture;
        }
        if (ext == ".ogg" || ext == ".mp3" || ext == ".wav" || ext == ".flac") {
            return ImportAsset::Type::Audio;
        }
        if (ext == ".tscn" || ext == ".scn") {
            return ImportAsset::Type::Scene;
        }
        if (ext == ".gd" || ext == ".cs") {
            return ImportAsset::Type::Script;
        }
        if (ext == ".glb" || ext == ".gltf" || ext == ".obj" || ext == ".fbx") {
            return ImportAsset::Type::Model;
        }
        if (ext == ".ttf" || ext == ".otf" || ext == ".woff") {
            return ImportAsset::Type::Font;
        }
    }

    return ImportAsset::Type::Unknown;
}

int GodotSceneImporter::CopyAssets(const std::vector<ImportAsset>& assets) {
    int copied = 0;

    for (const auto& asset : assets) {
        if (!asset.isSelected || asset.status != ImportAsset::Status::Resolved) {
            continue;
        }

        // Determine destination path
        std::string destPath = asset.lupinePath;
        if (destPath.find("res://") == 0) {
            destPath = destPath.substr(6);
        }

        fs::path fullDest = fs::path(m_Config.lupineProjectRoot) / destPath;

        try {
            // Create directories
            fs::create_directories(fullDest.parent_path());

            // Copy file
            fs::copy_file(asset.resolvedPath, fullDest, fs::copy_options::overwrite_existing);
            copied++;
        } catch (const std::exception& e) {
            LOG_ERROR(LogCategory::Asset, "Failed to copy asset: {} - {}", asset.resolvedPath, e.what());
        }
    }

    return copied;
}

int GodotSceneImporter::CreateBlankScripts(const std::vector<ImportAsset>& assets, const std::string& language) {
    int created = 0;

    for (const auto& asset : assets) {
        if (asset.status != ImportAsset::Status::Script || !asset.createBlankScript || !asset.isSelected) {
            continue;
        }

        // Extract class name from path
        std::string className = asset.godotPath;
        size_t lastSlash = className.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            className = className.substr(lastSlash + 1);
        }
        size_t dotPos = className.rfind('.');
        if (dotPos != std::string::npos) {
            className = className.substr(0, dotPos);
        }

        // Generate script content
        std::string content;
        std::string lang = asset.blankScriptLanguage.empty() ? language : asset.blankScriptLanguage;
        std::transform(lang.begin(), lang.end(), lang.begin(), ::tolower);

        if (lang == "lua") {
            content = BlankScriptGenerator::GenerateLuaScript(className);
        } else if (lang == "python" || lang == "py") {
            content = BlankScriptGenerator::GeneratePythonScript(className);
        } else if (lang == "mruby" || lang == "ruby" || lang == "rb") {
            content = BlankScriptGenerator::GenerateMrubyScript(className);
        } else {
            content = BlankScriptGenerator::GenerateLuaScript(className);  // Default
        }

        // Determine output path
        std::string outPath = asset.lupinePath;
        if (outPath.find("res://") == 0) {
            outPath = outPath.substr(6);
        }
        // Change extension
        size_t extPos = outPath.rfind('.');
        if (extPos != std::string::npos) {
            outPath = outPath.substr(0, extPos);
        }
        outPath += BlankScriptGenerator::GetScriptExtension(lang);

        fs::path fullPath = fs::path(m_Config.lupineProjectRoot) / outPath;

        try {
            fs::create_directories(fullPath.parent_path());

            std::ofstream file(fullPath);
            if (file.is_open()) {
                file << content;
                file.close();
                created++;
            }
        } catch (const std::exception& e) {
            LOG_ERROR(LogCategory::Asset, "Failed to create blank script: {} - {}", fullPath.string(), e.what());
        }
    }

    return created;
}

// ============================================================================
// Property Access Helpers
// ============================================================================

const GodotValue* GodotSceneImporter::GetProperty(const GodotNode& node, const std::string& name) {
    auto it = node.properties.find(name);
    if (it != node.properties.end()) {
        return &it->second;
    }
    return nullptr;
}

double GodotSceneImporter::GetFloatProperty(const GodotNode& node, const std::string& name, double defaultVal) {
    const GodotValue* val = GetProperty(node, name);
    if (!val) return defaultVal;

    if (val->type == GodotValue::Type::Float) return val->floatValue;
    if (val->type == GodotValue::Type::Int) return static_cast<double>(val->intValue);
    return defaultVal;
}

int64_t GodotSceneImporter::GetIntProperty(const GodotNode& node, const std::string& name, int64_t defaultVal) {
    const GodotValue* val = GetProperty(node, name);
    if (!val) return defaultVal;

    if (val->type == GodotValue::Type::Int) return val->intValue;
    if (val->type == GodotValue::Type::Float) return static_cast<int64_t>(val->floatValue);
    return defaultVal;
}

bool GodotSceneImporter::GetBoolProperty(const GodotNode& node, const std::string& name, bool defaultVal) {
    const GodotValue* val = GetProperty(node, name);
    if (!val) return defaultVal;

    if (val->type == GodotValue::Type::Bool) return val->boolValue;
    return defaultVal;
}

std::string GodotSceneImporter::GetStringProperty(const GodotNode& node, const std::string& name, const std::string& defaultVal) {
    const GodotValue* val = GetProperty(node, name);
    if (!val) return defaultVal;

    if (val->type == GodotValue::Type::String) return val->stringValue;
    return defaultVal;
}

std::pair<double, double> GodotSceneImporter::GetVector2Property(const GodotNode& node, const std::string& name, double defX, double defY) {
    const GodotValue* val = GetProperty(node, name);
    if (!val || val->type != GodotValue::Type::Vector2) {
        return {defX, defY};
    }
    return {val->x, val->y};
}

std::tuple<double, double, double> GodotSceneImporter::GetVector3Property(const GodotNode& node, const std::string& name, double defX, double defY, double defZ) {
    const GodotValue* val = GetProperty(node, name);
    if (!val || val->type != GodotValue::Type::Vector3) {
        return {defX, defY, defZ};
    }
    return {val->x, val->y, val->z};
}

std::tuple<double, double, double, double> GodotSceneImporter::GetColorProperty(const GodotNode& node, const std::string& name, double defR, double defG, double defB, double defA) {
    const GodotValue* val = GetProperty(node, name);
    if (!val || val->type != GodotValue::Type::Color) {
        return {defR, defG, defB, defA};
    }
    return {val->r, val->g, val->b, val->a};
}

std::pair<float, float> GodotSceneImporter::GetControlSize(const GodotNode& node) {
    // First try direct size property (most common for manually sized Controls)
    auto [sizeW, sizeH] = GetVector2Property(node, "size", 0, 0);
    if (sizeW > 0 || sizeH > 0) {
        LOG_DEBUG(LogCategory::Asset, "GetControlSize for '{}': Using size property = ({}, {})",
                  node.name, sizeW, sizeH);
        return {static_cast<float>(sizeW), static_cast<float>(sizeH)};
    }

    // Try custom_minimum_size
    auto [minW, minH] = GetVector2Property(node, "custom_minimum_size", 0, 0);
    if (minW > 0 || minH > 0) {
        LOG_DEBUG(LogCategory::Asset, "GetControlSize for '{}': Using custom_minimum_size = ({}, {})",
                  node.name, minW, minH);
        return {static_cast<float>(minW), static_cast<float>(minH)};
    }

    // Calculate from offset deltas (offset_right - offset_left, offset_bottom - offset_top)
    double offsetLeft = GetFloatProperty(node, "offset_left", 0.0);
    double offsetRight = GetFloatProperty(node, "offset_right", 0.0);
    double offsetTop = GetFloatProperty(node, "offset_top", 0.0);
    double offsetBottom = GetFloatProperty(node, "offset_bottom", 0.0);

    LOG_DEBUG(LogCategory::Asset, "GetControlSize for '{}': offsets L={}, R={}, T={}, B={}",
              node.name, offsetLeft, offsetRight, offsetTop, offsetBottom);

    float width = static_cast<float>(offsetRight - offsetLeft);
    float height = static_cast<float>(offsetBottom - offsetTop);

    LOG_DEBUG(LogCategory::Asset, "GetControlSize for '{}': Calculated size = ({}, {})",
              node.name, width, height);

    // Only use calculated values if they're positive
    if (width <= 0) width = 0;
    if (height <= 0) height = 0;

    return {width, height};
}

// ============================================================================
// Node-Specific Converters
// ============================================================================

void GodotSceneImporter::ConvertSprite2D(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene) {
    auto sprite = lupineNode->GetComponent<components::Sprite2D>();
    if (!sprite) return;

    // Ensure properties are initialized before setting values
    sprite->RegisterProperties();

    // Texture
    const GodotValue* texVal = GetProperty(godotNode, "texture");
    if (texVal && texVal->type == GodotValue::Type::ExtResource) {
        const GodotExtResource* res = scene.GetExtResource(texVal->resourceId);
        if (res) {
            sprite->SetTexturePath(ConvertToLupinePath(res->path));
        }
    }

    // Modulate color
    auto [r, g, b, a] = GetColorProperty(godotNode, "modulate", 1, 1, 1, 1);
    sprite->SetModulate(math::Color(static_cast<float>(r), static_cast<float>(g), static_cast<float>(b), static_cast<float>(a)));

    // Centered
    bool centered = GetBoolProperty(godotNode, "centered", true);
    sprite->SetCentered(centered);

    // Offset
    auto [offX, offY] = GetVector2Property(godotNode, "offset", 0, 0);
    sprite->SetOffset(math::Vec2(static_cast<float>(offX), static_cast<float>(offY)));

    // Flip
    sprite->SetFlipH(GetBoolProperty(godotNode, "flip_h", false));
    sprite->SetFlipV(GetBoolProperty(godotNode, "flip_v", false));

    // Scale (Godot scale -> Lupine size multiplier)
    // Note: Node2D scale is already handled in ConvertNodeProperties,
    // but if the sprite has explicit size it would be here

    // Region (sprite sheet) - convert to UV rect
    bool regionEnabled = GetBoolProperty(godotNode, "region_enabled", false);
    if (regionEnabled) {
        sprite->SetSpriteSheetEnabled(true);
        // Region rect in Godot: Rect2(x, y, width, height) in pixels
        // In Lupine, we use UV coordinates (normalized 0-1)
        // This would require texture dimensions to calculate properly
    }

    // Hframes and vframes for sprite sheet
    int hframes = static_cast<int>(GetIntProperty(godotNode, "hframes", 1));
    int vframes = static_cast<int>(GetIntProperty(godotNode, "vframes", 1));
    if (hframes > 1 || vframes > 1) {
        sprite->SetSpriteSheetEnabled(true);
        // Lupine calculates sprite size from total frames
        // The actual sprite size would need to be computed based on texture dimensions
        // For now, set current frame
        int frame = static_cast<int>(GetIntProperty(godotNode, "frame", 0));
        sprite->SetCurrentFrame(frame);
    }
}

void GodotSceneImporter::ConvertSprite3D(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene) {
    auto sprite = lupineNode->GetComponent<components::Sprite3D>();
    if (!sprite) return;

    sprite->RegisterProperties();

    // Texture
    const GodotValue* texVal = GetProperty(godotNode, "texture");
    if (texVal && texVal->type == GodotValue::Type::ExtResource) {
        const GodotExtResource* res = scene.GetExtResource(texVal->resourceId);
        if (res) {
            sprite->SetTexturePath(ConvertToLupinePath(res->path));
        }
    }

    // Modulate color
    auto [r, g, b, a] = GetColorProperty(godotNode, "modulate", 1, 1, 1, 1);
    sprite->SetModulate(math::Color(static_cast<float>(r), static_cast<float>(g), static_cast<float>(b), static_cast<float>(a)));

    // Flip
    sprite->SetFlipH(GetBoolProperty(godotNode, "flip_h", false));
    sprite->SetFlipV(GetBoolProperty(godotNode, "flip_v", false));

    // Pixel size (determines world scale relative to pixels)
    double pixelSize = GetFloatProperty(godotNode, "pixel_size", 0.01);
    sprite->SetPixelSize(static_cast<float>(pixelSize));

    // Billboard mode: 0=Disabled, 1=Enabled, 2=Y-Billboard
    int billboard = static_cast<int>(GetIntProperty(godotNode, "billboard", 0));
    switch (billboard) {
        case 1:
            sprite->SetBillboardMode(components::BillboardMode::Enabled);
            break;
        case 2:
            sprite->SetBillboardMode(components::BillboardMode::YAxisOnly);
            break;
        default:
            sprite->SetBillboardMode(components::BillboardMode::Disabled);
            break;
    }

    // Double-sided
    bool doubleSided = GetBoolProperty(godotNode, "double_sided", true);
    sprite->SetDoubleSided(doubleSided);

    // Alpha cut mode (for transparency): 0=Disabled, 1=Discard, 2=OpaquePrepass
    int alphaCut = static_cast<int>(GetIntProperty(godotNode, "alpha_cut", 0));
    if (alphaCut > 0) {
        double alphaCutoff = GetFloatProperty(godotNode, "alpha_scissor_threshold", 0.5);
        sprite->SetAlphaCutoff(static_cast<float>(alphaCutoff));
    }
}

void GodotSceneImporter::ConvertAnimatedSprite2D(const GodotNode&, std::shared_ptr<core::Node>, const GodotScene&) {
    // AnimatedSprite2D in Godot uses SpriteFrames resource
    // In Lupine, we use a different animation system
    // For now, just note that this needs manual setup
}

void GodotSceneImporter::ConvertLabel(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene) {
    auto label = lupineNode->GetComponent<components::Label>();
    if (!label) return;

    label->RegisterProperties();

    // Text content
    std::string text = GetStringProperty(godotNode, "text", "");
    label->SetText(text);

    // Modulate color (Godot applies modulate to all rendering)
    auto [r, g, b, a] = GetColorProperty(godotNode, "modulate", 1, 1, 1, 1);
    label->SetColor(math::Color(static_cast<float>(r), static_cast<float>(g), static_cast<float>(b), static_cast<float>(a)));

    // Font size - Godot 4.x uses theme_override_font_sizes/font_size
    double fontSize = GetFloatProperty(godotNode, "theme_override_font_sizes/font_size", 0.0);
    if (fontSize <= 0) {
        // Try older Godot 3.x style or direct property
        fontSize = GetFloatProperty(godotNode, "font_size", 16.0);
    }
    label->SetFontSize(static_cast<float>(fontSize));

    // Font path from theme_override_fonts/font resource
    const GodotValue* fontVal = GetProperty(godotNode, "theme_override_fonts/font");
    if (fontVal && fontVal->type == GodotValue::Type::ExtResource) {
        const GodotExtResource* res = scene.GetExtResource(fontVal->resourceId);
        if (res) {
            label->SetFontPath(ConvertToLupinePath(res->path));
        }
    }

    // Centered/alignment - Godot uses horizontal_alignment
    int hAlign = static_cast<int>(GetIntProperty(godotNode, "horizontal_alignment", 0));
    // 0=Left, 1=Center, 2=Right, 3=Fill
    label->SetCentered(hAlign == 1);

    // Offset
    auto [offX, offY] = GetVector2Property(godotNode, "offset", 0, 0);
    label->SetOffset(math::Vec2(static_cast<float>(offX), static_cast<float>(offY)));

    // Label-specific font color (theme_override_colors/font_color)
    const GodotValue* fontColorVal = GetProperty(godotNode, "theme_override_colors/font_color");
    if (fontColorVal && fontColorVal->type == GodotValue::Type::Color) {
        label->SetColor(math::Color(
            static_cast<float>(fontColorVal->r),
            static_cast<float>(fontColorVal->g),
            static_cast<float>(fontColorVal->b),
            static_cast<float>(fontColorVal->a)));
    }
}

void GodotSceneImporter::ConvertCamera2D(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene) {
    (void)scene; // Unused parameter
    auto camera = std::dynamic_pointer_cast<core::Camera2D>(lupineNode);
    if (!camera) return;

    auto [zoomX, zoomY] = GetVector2Property(godotNode, "zoom", 1, 1);
    camera->SetZoom(static_cast<float>(zoomX));

    bool current = GetBoolProperty(godotNode, "current", false);
    camera->SetActive(current);
}

void GodotSceneImporter::ConvertCamera3D(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene) {
    (void)scene; // Unused parameter
    auto camera = std::dynamic_pointer_cast<core::Camera3D>(lupineNode);
    if (!camera) return;

    double fov = GetFloatProperty(godotNode, "fov", 75.0);
    camera->SetFOV(static_cast<float>(fov));

    double nearVal = GetFloatProperty(godotNode, "near", 0.1);
    double farVal = GetFloatProperty(godotNode, "far", 1000.0);
    camera->SetNearPlane(static_cast<float>(nearVal));
    camera->SetFarPlane(static_cast<float>(farVal));

    bool current = GetBoolProperty(godotNode, "current", false);
    camera->SetActive(current);
}

void GodotSceneImporter::ConvertAudioStreamPlayer(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene) {
    auto audio = lupineNode->GetComponent<components::AudioPlayer>();
    if (!audio) return;

    audio->RegisterProperties();

    // Stream/audio file - set via property system
    const GodotValue* streamVal = GetProperty(godotNode, "stream");
    if (streamVal && streamVal->type == GodotValue::Type::ExtResource) {
        const GodotExtResource* res = scene.GetExtResource(streamVal->resourceId);
        if (res) {
            // Use property system to set the audio asset path
            audio->SetPropertyValue("audioAsset", ConvertToLupinePath(res->path));
        }
    }

    double volume = GetFloatProperty(godotNode, "volume_db", 0.0);
    // Convert dB to linear (approximately)
    float linearVolume = static_cast<float>(std::pow(10.0, volume / 20.0));
    audio->SetVolume(linearVolume);

    bool autoplay = GetBoolProperty(godotNode, "autoplay", false);
    audio->SetAutoplay(autoplay);

    bool loop = GetBoolProperty(godotNode, "loop", false);
    audio->SetLoop(loop);
}

void GodotSceneImporter::ConvertLight2D(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene) {
    (void)scene; // Unused parameter
    auto light = lupineNode->GetComponent<components::Light2D>();
    if (!light) return;

    light->RegisterProperties();

    auto [r, g, b, a] = GetColorProperty(godotNode, "color", 1, 1, 1, 1);
    light->SetColor(math::Color(static_cast<float>(r), static_cast<float>(g), static_cast<float>(b), static_cast<float>(a)));

    double energy = GetFloatProperty(godotNode, "energy", 1.0);
    light->SetIntensity(static_cast<float>(energy));

    // Note: Light2D does not have a SetTexturePath method - Lupine Light2D uses procedural falloff
    // Texture-based lighting would need a different component or Light2D extension
    const GodotValue* texVal = GetProperty(godotNode, "texture");
    if (texVal && texVal->type == GodotValue::Type::ExtResource) {
        // Log that we're ignoring the texture as it's not supported in Lupine Light2D
        LOG_WARN(LogCategory::Asset, "Light2D texture import not supported - using procedural falloff instead");
    }
}

void GodotSceneImporter::ConvertDirectionalLight3D(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene) {
    (void)scene; // Unused parameter
    auto light = lupineNode->GetComponent<components::DirectionalLight3D>();
    if (!light) return;

    light->RegisterProperties();

    auto [r, g, b, a] = GetColorProperty(godotNode, "light_color", 1, 1, 1, 1);
    light->SetColor(math::Color(static_cast<float>(r), static_cast<float>(g), static_cast<float>(b), static_cast<float>(a)));

    double energy = GetFloatProperty(godotNode, "light_energy", 1.0);
    light->SetIntensity(static_cast<float>(energy));

    bool shadows = GetBoolProperty(godotNode, "shadow_enabled", false);
    light->SetCastsShadows(shadows);
}

void GodotSceneImporter::ConvertOmniLight3D(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene) {
    (void)scene; // Unused parameter
    auto light = lupineNode->GetComponent<components::OmniLight3D>();
    if (!light) return;

    light->RegisterProperties();

    auto [r, g, b, a] = GetColorProperty(godotNode, "light_color", 1, 1, 1, 1);
    light->SetColor(math::Color(static_cast<float>(r), static_cast<float>(g), static_cast<float>(b), static_cast<float>(a)));

    double energy = GetFloatProperty(godotNode, "light_energy", 1.0);
    light->SetIntensity(static_cast<float>(energy));

    double range = GetFloatProperty(godotNode, "omni_range", 5.0);
    light->SetRange(static_cast<float>(range));

    bool shadows = GetBoolProperty(godotNode, "shadow_enabled", false);
    light->SetCastsShadows(shadows);
}

void GodotSceneImporter::ConvertSpotLight3D(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene) {
    (void)scene; // Unused parameter
    auto light = lupineNode->GetComponent<components::SpotLight3D>();
    if (!light) return;

    light->RegisterProperties();

    auto [r, g, b, a] = GetColorProperty(godotNode, "light_color", 1, 1, 1, 1);
    light->SetColor(math::Color(static_cast<float>(r), static_cast<float>(g), static_cast<float>(b), static_cast<float>(a)));

    double energy = GetFloatProperty(godotNode, "light_energy", 1.0);
    light->SetIntensity(static_cast<float>(energy));

    double range = GetFloatProperty(godotNode, "spot_range", 5.0);
    light->SetRange(static_cast<float>(range));

    // Godot uses spot_angle for the outer cone angle
    double angle = GetFloatProperty(godotNode, "spot_angle", 45.0);
    light->SetOuterConeAngle(static_cast<float>(angle));

    bool shadows = GetBoolProperty(godotNode, "shadow_enabled", false);
    light->SetCastsShadows(shadows);
}

void GodotSceneImporter::ConvertMeshInstance3D(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene) {
    auto mesh = lupineNode->GetComponent<components::StaticMesh3D>();
    if (!mesh) return;

    mesh->RegisterProperties();

    // Mesh resource
    const GodotValue* meshVal = GetProperty(godotNode, "mesh");
    if (meshVal && meshVal->type == GodotValue::Type::ExtResource) {
        const GodotExtResource* res = scene.GetExtResource(meshVal->resourceId);
        if (res) {
            mesh->SetModelPath(ConvertToLupinePath(res->path));
        }
    }
}

void GodotSceneImporter::ConvertRigidBody2D(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene) {
    (void)scene; // Unused parameter
    auto body = lupineNode->GetComponent<components::RigidBody2DComponent>();
    if (!body) return;

    body->RegisterProperties();

    // Note: RigidBody2DComponent mass is calculated from colliders, not set directly
    // We can log the Godot mass value for reference
    double mass = GetFloatProperty(godotNode, "mass", 1.0);
    (void)mass; // Mass is derived from collider density in Lupine physics

    double gravityScale = GetFloatProperty(godotNode, "gravity_scale", 1.0);
    body->SetGravityScale(static_cast<float>(gravityScale));

    double linearDamp = GetFloatProperty(godotNode, "linear_damp", 0.0);
    body->SetLinearDamping(static_cast<float>(linearDamp));

    double angularDamp = GetFloatProperty(godotNode, "angular_damp", 0.0);
    body->SetAngularDamping(static_cast<float>(angularDamp));
}

void GodotSceneImporter::ConvertStaticBody2D(const GodotNode&, std::shared_ptr<core::Node>, const GodotScene&) {
    // StaticBody2D typically just uses collision shapes from children
}

void GodotSceneImporter::ConvertCharacterBody2D(const GodotNode&, std::shared_ptr<core::Node>, const GodotScene&) {
    // Character controller specific properties
}

void GodotSceneImporter::ConvertArea2D(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene&) {
    auto area = lupineNode->GetComponent<components::AreaTrigger2DComponent>();
    if (!area) return;

    area->RegisterProperties();

    bool monitoring = GetBoolProperty(godotNode, "monitoring", true);
    area->SetMonitoring(monitoring);

    bool monitorable = GetBoolProperty(godotNode, "monitorable", true);
    area->SetMonitorable(monitorable);
}

void GodotSceneImporter::ConvertCollisionShape2D(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene) {
    auto collision = lupineNode->GetComponent<components::CollisionBody2DComponent>();
    if (!collision) return;

    collision->RegisterProperties();

    bool disabled = GetBoolProperty(godotNode, "disabled", false);
    collision->SetEnabled(!disabled);

    // Shape is typically a SubResource
    const GodotValue* shapeVal = GetProperty(godotNode, "shape");
    if (shapeVal && shapeVal->type == GodotValue::Type::SubResource) {
        const GodotSubResource* subRes = scene.GetSubResource(shapeVal->resourceId);
        if (subRes) {
            // Convert shape type using CollisionShape2DType enum
            if (subRes->type == "RectangleShape2D") {
                collision->SetShapeType(components::CollisionShape2DType::Rectangle);
                // Size might be in properties
            } else if (subRes->type == "CircleShape2D") {
                collision->SetShapeType(components::CollisionShape2DType::Circle);
            } else if (subRes->type == "CapsuleShape2D") {
                // Lupine doesn't have Capsule shape - use Polygon as fallback
                collision->SetShapeType(components::CollisionShape2DType::Polygon);
                LOG_WARN(LogCategory::Asset, "CapsuleShape2D converted to Polygon - may need manual adjustment");
            } else if (subRes->type == "ConvexPolygonShape2D" || subRes->type == "ConcavePolygonShape2D") {
                collision->SetShapeType(components::CollisionShape2DType::Polygon);
            }
        }
    }
}

void GodotSceneImporter::ConvertRigidBody3D(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene&) {
    auto body = lupineNode->GetComponent<components::RigidBody3DComponent>();
    if (!body) return;

    body->RegisterProperties();

    double mass = GetFloatProperty(godotNode, "mass", 1.0);
    body->SetMass(static_cast<float>(mass));

    double gravityScale = GetFloatProperty(godotNode, "gravity_scale", 1.0);
    body->SetGravityScale(static_cast<float>(gravityScale));
}

void GodotSceneImporter::ConvertStaticBody3D(const GodotNode&, std::shared_ptr<core::Node>, const GodotScene&) {
    // Similar to 2D
}

void GodotSceneImporter::ConvertCharacterBody3D(const GodotNode&, std::shared_ptr<core::Node>, const GodotScene&) {
    // Character controller 3D
}

void GodotSceneImporter::ConvertArea3D(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene&) {
    auto area = lupineNode->GetComponent<components::AreaTrigger3DComponent>();
    if (!area) return;

    area->RegisterProperties();

    bool monitoring = GetBoolProperty(godotNode, "monitoring", true);
    area->SetMonitoring(monitoring);
}

void GodotSceneImporter::ConvertCollisionShape3D(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene&) {
    auto collision = lupineNode->GetComponent<components::CollisionMesh3DComponent>();
    if (!collision) return;

    collision->RegisterProperties();

    bool disabled = GetBoolProperty(godotNode, "disabled", false);
    collision->SetEnabled(!disabled);
}

void GodotSceneImporter::ConvertTimer(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene) {
    (void)scene; // Unused parameter
    auto timer = lupineNode->GetComponent<components::Timer>();
    if (!timer) return;

    timer->RegisterProperties();

    // Godot's wait_time maps to Lupine's Duration
    double waitTime = GetFloatProperty(godotNode, "wait_time", 1.0);
    timer->SetDuration(static_cast<float>(waitTime));

    // Godot's one_shot is the inverse of Lupine's Loop
    bool oneShot = GetBoolProperty(godotNode, "one_shot", false);
    timer->SetLoop(!oneShot);

    // Godot's autostart maps to Lupine's AutoStart
    bool autostart = GetBoolProperty(godotNode, "autostart", false);
    timer->SetAutoStart(autostart);
}

void GodotSceneImporter::ConvertTileMap(const GodotNode&, std::shared_ptr<core::Node>, const GodotScene&) {
    // TileMap conversion is complex - would need tileset conversion too
    // For now, just create the component
}

void GodotSceneImporter::ConvertColorRect(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene) {
    (void)scene; // Unused parameter
    auto rect = lupineNode->GetComponent<components::ColorRect>();
    if (!rect) return;

    rect->RegisterProperties();

    // Color
    auto [r, g, b, a] = GetColorProperty(godotNode, "color", 1, 1, 1, 1);
    rect->SetColor(math::Color(static_cast<float>(r), static_cast<float>(g), static_cast<float>(b), static_cast<float>(a)));

    // Size from control properties
    auto [width, height] = GetControlSize(godotNode);
    if (width > 0) rect->SetWidth(width);
    if (height > 0) rect->SetHeight(height);
}

void GodotSceneImporter::ConvertTextureRect(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene) {
    auto image = lupineNode->GetComponent<components::Image2D>();
    if (!image) return;

    image->RegisterProperties();

    // Texture path
    const GodotValue* texVal = GetProperty(godotNode, "texture");
    if (texVal && texVal->type == GodotValue::Type::ExtResource) {
        const GodotExtResource* res = scene.GetExtResource(texVal->resourceId);
        if (res) {
            image->SetTexturePath(ConvertToLupinePath(res->path));
        }
    }

    // Size from control properties
    auto [width, height] = GetControlSize(godotNode);
    if (width > 0) image->SetWidth(width);
    if (height > 0) image->SetHeight(height);

    // Color/modulate
    auto [r, g, b, a] = GetColorProperty(godotNode, "modulate", 1, 1, 1, 1);
    image->SetColor(math::Color(static_cast<float>(r), static_cast<float>(g), static_cast<float>(b), static_cast<float>(a)));

    // Flip
    image->SetFlipH(GetBoolProperty(godotNode, "flip_h", false));
    image->SetFlipV(GetBoolProperty(godotNode, "flip_v", false));

    // Stretch mode -> aspect mode
    // Godot stretch_mode: 0=Scale, 1=Tile, 2=Keep, 3=KeepCentered, 4=KeepAspect, 5=KeepAspectCentered, 6=KeepAspectCovered
    int stretchMode = static_cast<int>(GetIntProperty(godotNode, "stretch_mode", 0));
    if (stretchMode >= 4) {
        image->SetPreserveAspect(true);
        // Map to Lupine AspectMode
        if (stretchMode == 6) {
            // KeepAspectCovered -> Fill (crops to fill)
            image->SetAspectMode(components::AspectMode::Fill);
        } else {
            // KeepAspect/KeepAspectCentered -> Fit (letterbox)
            image->SetAspectMode(components::AspectMode::Fit);
        }
    } else if (stretchMode == 0) {
        // Scale -> Stretch (ignore aspect ratio)
        image->SetPreserveAspect(false);
        image->SetAspectMode(components::AspectMode::Stretch);
    }
}

void GodotSceneImporter::ConvertButton(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene) {
    auto button = lupineNode->GetComponent<components::Button>();
    if (!button) return;

    button->RegisterProperties();

    // Text
    std::string text = GetStringProperty(godotNode, "text", "");
    button->SetText(text);

    // Size from control properties
    auto [width, height] = GetControlSize(godotNode);
    if (width > 0) button->SetWidth(width);
    if (height > 0) button->SetHeight(height);

    // Font size from theme_override_font_sizes/font_size
    double fontSize = GetFloatProperty(godotNode, "theme_override_font_sizes/font_size", 0.0);
    if (fontSize > 0) {
        button->SetFontSize(static_cast<float>(fontSize));
    }

    // Font path from theme_override_fonts/font
    const GodotValue* fontVal = GetProperty(godotNode, "theme_override_fonts/font");
    if (fontVal && fontVal->type == GodotValue::Type::ExtResource) {
        const GodotExtResource* res = scene.GetExtResource(fontVal->resourceId);
        if (res) {
            button->SetFontPath(ConvertToLupinePath(res->path));
        }
    }

    // Font color from theme_override_colors/font_color
    const GodotValue* fontColorVal = GetProperty(godotNode, "theme_override_colors/font_color");
    if (fontColorVal && fontColorVal->type == GodotValue::Type::Color) {
        button->SetFontColor(math::Color(
            static_cast<float>(fontColorVal->r),
            static_cast<float>(fontColorVal->g),
            static_cast<float>(fontColorVal->b),
            static_cast<float>(fontColorVal->a)));
    }

    // Disabled state
    bool disabled = GetBoolProperty(godotNode, "disabled", false);
    button->SetEnabled(!disabled);

    // Flat button (no background)
    bool flat = GetBoolProperty(godotNode, "flat", false);
    if (flat) {
        button->SetBackgroundColor(math::Color(0, 0, 0, 0));
    }
}

void GodotSceneImporter::ConvertContainer(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene) {
    (void)scene;

    // Try to get a Container component (or any container subclass)
    auto container = lupineNode->GetComponent<components::Container>();
    if (!container) {
        // Try other container types
        auto hContainer = lupineNode->GetComponent<components::HorizontalContainer>();
        if (hContainer) container = hContainer;
        auto vContainer = lupineNode->GetComponent<components::VerticalContainer>();
        if (vContainer) container = vContainer;
        auto gridContainer = lupineNode->GetComponent<components::GridContainer>();
        if (gridContainer) container = gridContainer;
        auto centerContainer = lupineNode->GetComponent<components::CenterContainer>();
        if (centerContainer) container = centerContainer;
        auto paddingContainer = lupineNode->GetComponent<components::PaddingContainer>();
        if (paddingContainer) container = paddingContainer;
    }

    if (!container) return;

    // Ensure properties are initialized before setting values
    container->RegisterProperties();

    // Godot containers don't draw backgrounds by default, but Lupine containers do
    // Turn off background to match Godot behavior
    container->SetDrawBackground(false);

    // Size from control properties
    auto [width, height] = GetControlSize(godotNode);
    if (width > 0) container->SetWidth(width);
    if (height > 0) container->SetHeight(height);
}

void GodotSceneImporter::ConvertPanel(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene) {
    (void)scene;
    auto panel = lupineNode->GetComponent<components::Panel>();
    if (!panel) {
        LOG_WARN(LogCategory::Asset, "ConvertPanel: Panel component not found on node '{}'", godotNode.name);
        return;
    }

    // Ensure properties are initialized before setting values
    panel->RegisterProperties();

    // Size from control properties
    auto [width, height] = GetControlSize(godotNode);
    LOG_DEBUG(LogCategory::Asset, "ConvertPanel '{}': Setting size to ({}, {})", godotNode.name, width, height);
    if (width > 0) panel->SetWidth(width);
    if (height > 0) panel->SetHeight(height);

    // Verify the values were set
    LOG_DEBUG(LogCategory::Asset, "ConvertPanel '{}': After set, panel size is ({}, {})",
              godotNode.name, panel->GetWidth(), panel->GetHeight());

    // Modulate/self_modulate for background color
    auto [r, g, b, a] = GetColorProperty(godotNode, "self_modulate", 1, 1, 1, 1);
    panel->SetBackgroundColor(math::Color(static_cast<float>(r), static_cast<float>(g), static_cast<float>(b), static_cast<float>(a)));
}

void GodotSceneImporter::ConvertControl(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene) {
    (void)godotNode;
    (void)lupineNode;
    (void)scene;
    // Base Control - mainly for layout
}

void GodotSceneImporter::ConvertProgressBar(const GodotNode& godotNode, std::shared_ptr<core::Node> lupineNode, const GodotScene& scene) {
    auto progress = lupineNode->GetComponent<components::ProgressBar>();
    if (!progress) return;

    progress->RegisterProperties();

    // Value range
    double minValue = GetFloatProperty(godotNode, "min_value", 0.0);
    double maxValue = GetFloatProperty(godotNode, "max_value", 100.0);
    double value = GetFloatProperty(godotNode, "value", 0.0);
    double step = GetFloatProperty(godotNode, "step", 1.0);

    progress->SetMinValue(static_cast<float>(minValue));
    progress->SetMaxValue(static_cast<float>(maxValue));
    progress->SetValue(static_cast<float>(value));
    progress->SetStep(static_cast<float>(step));

    // Size from control properties
    auto [width, height] = GetControlSize(godotNode);
    if (width > 0) progress->SetWidth(width);
    if (height > 0) progress->SetHeight(height);

    // Fill color from theme_override_colors/fill_color or fg_color
    const GodotValue* fillColorVal = GetProperty(godotNode, "theme_override_colors/font_color");
    if (fillColorVal && fillColorVal->type == GodotValue::Type::Color) {
        progress->SetFillColor(math::Color(
            static_cast<float>(fillColorVal->r),
            static_cast<float>(fillColorVal->g),
            static_cast<float>(fillColorVal->b),
            static_cast<float>(fillColorVal->a)));
    }

    // Background color from theme_override_colors/background_color
    const GodotValue* bgColorVal = GetProperty(godotNode, "theme_override_colors/background_color");
    if (bgColorVal && bgColorVal->type == GodotValue::Type::Color) {
        progress->SetBackgroundColor(math::Color(
            static_cast<float>(bgColorVal->r),
            static_cast<float>(bgColorVal->g),
            static_cast<float>(bgColorVal->b),
            static_cast<float>(bgColorVal->a)));
    }

    // Show percentage
    bool showPercentage = GetBoolProperty(godotNode, "show_percentage", true);
    progress->SetShowValue(showPercentage);

    // Font for value display
    const GodotValue* fontVal = GetProperty(godotNode, "theme_override_fonts/font");
    if (fontVal && fontVal->type == GodotValue::Type::ExtResource) {
        const GodotExtResource* res = scene.GetExtResource(fontVal->resourceId);
        if (res) {
            progress->SetValueFontPath(ConvertToLupinePath(res->path));
        }
    }

    double valueFontSize = GetFloatProperty(godotNode, "theme_override_font_sizes/font_size", 0.0);
    if (valueFontSize > 0) {
        progress->SetValueFontSize(static_cast<float>(valueFontSize));
    }
}

void GodotSceneImporter::ConvertLineEdit(const GodotNode&, std::shared_ptr<core::Node>, const GodotScene&) {
    // Line edit text input - not directly supported
}

void GodotSceneImporter::ConvertTextEdit(const GodotNode&, std::shared_ptr<core::Node>, const GodotScene&) {
    // Multi-line text edit - not directly supported
}

} // namespace import
} // namespace lupine
