#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include "lupine/engine/Engine.hpp"
#include "lupine/core/Project.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/engine/SceneDocument.hpp"
#include "lupine/engine/EditorSession.hpp"
#include "lupine/engine/RecentProjects.hpp"
#include "lupine/engine/EditorBridge.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Component.hpp"
#include "lupine/core/Prefab.hpp"
#include "lupine/core/UUID.hpp"
#include "lupine/core/Command.hpp"
#include "lupine/components/Button.hpp"
#include "lupine/components/AudioPlayer.hpp"
#include "lupine/components/AudioListener.hpp"

#include "lupine/rendering/gfx/GfxTypes.hpp"
#include <nlohmann/json.hpp>

namespace py = pybind11;

PYBIND11_MODULE(lupine_engine, m) {
    m.doc() = "Lupine Engine Python Module";
    m.attr("__version__") = "0.1.0";

    #if PY_VERSION_HEX < 0x03090000
        PyEval_InitThreads();
    #endif

    try {
        lupine::Logger::Init("lupine_engine.log", true);
    } catch (const std::exception& e) {

        PyErr_WarnEx(PyExc_RuntimeWarning, ("Failed to initialize logger: " + std::string(e.what())).c_str(), 1);
    }

    py::class_<lupine::core::UUID>(m, "UUID")
        .def(py::init<>())
        .def(py::init<uint64_t>())
        .def("__str__", &lupine::core::UUID::ToString)
        .def("__repr__", [](const lupine::core::UUID& uuid) {
            return "UUID('" + uuid.ToString() + "')";
        })
        .def("__eq__", &lupine::core::UUID::operator==)
        .def("__ne__", &lupine::core::UUID::operator!=)
        .def("__lt__", &lupine::core::UUID::operator<)
        .def("__hash__", [](const lupine::core::UUID& uuid) {
            return static_cast<size_t>(static_cast<uint64_t>(uuid));
        })
        .def("is_valid", &lupine::core::UUID::IsValid)
        .def("to_string", &lupine::core::UUID::ToString)
        .def_static("from_string", &lupine::core::UUID::FromString);

    py::class_<lupine::core::Command, std::shared_ptr<lupine::core::Command>>(m, "Command")
        .def("execute", &lupine::core::Command::Execute)
        .def("undo", &lupine::core::Command::Undo)
        .def("redo", &lupine::core::Command::Redo)
        .def("get_description", &lupine::core::Command::GetDescription)
        .def("get_type_name", &lupine::core::Command::GetTypeName);

    py::class_<lupine::core::CompositeCommand, lupine::core::Command, std::shared_ptr<lupine::core::CompositeCommand>>(m, "CompositeCommand")
        .def(py::init<const std::string&>(), py::arg("description"))
        .def("add_command", &lupine::core::CompositeCommand::AddCommand, py::arg("command"))
        .def("get_command_count", &lupine::core::CompositeCommand::GetCommandCount);

    py::class_<lupine::engine::RecentProjectEntry>(m, "RecentProjectEntry")
        .def(py::init<>())
        .def(py::init<const std::string&, const std::string&, int64_t>())
        .def_readwrite("project_path", &lupine::engine::RecentProjectEntry::projectPath)
        .def_readwrite("project_name", &lupine::engine::RecentProjectEntry::projectName)
        .def_readwrite("last_opened_time", &lupine::engine::RecentProjectEntry::lastOpenedTime);

    py::class_<lupine::engine::RecentProjects>(m, "RecentProjects")
        .def(py::init<>())
        .def("add_or_update_project", &lupine::engine::RecentProjects::AddOrUpdateProject,
             py::arg("project_path"), py::arg("project_name"))
        .def("remove_project", &lupine::engine::RecentProjects::RemoveProject,
             py::arg("project_path"))
        .def("clear", &lupine::engine::RecentProjects::Clear)
        .def("get_projects", &lupine::engine::RecentProjects::GetProjects,
             py::return_value_policy::reference_internal)
        .def("get_max_projects", &lupine::engine::RecentProjects::GetMaxProjects)
        .def("set_max_projects", &lupine::engine::RecentProjects::SetMaxProjects,
             py::arg("max_projects"))
        .def("load", &lupine::engine::RecentProjects::Load)
        .def("save", &lupine::engine::RecentProjects::Save)
        .def_static("get_default_file_path", &lupine::engine::RecentProjects::GetDefaultFilePath);

    py::class_<lupine::core::ProjectSettings>(m, "ProjectSettings")
        .def(py::init<>())
        .def_readwrite("project_name", &lupine::core::ProjectSettings::projectName)
        .def_readwrite("creator_name", &lupine::core::ProjectSettings::creatorName)
        .def_readwrite("version", &lupine::core::ProjectSettings::version)
        .def_readwrite("main_scene", &lupine::core::ProjectSettings::mainScene)
        .def_readwrite("window_width", &lupine::core::ProjectSettings::windowWidth)
        .def_readwrite("window_height", &lupine::core::ProjectSettings::windowHeight)
        .def_readwrite("fullscreen", &lupine::core::ProjectSettings::fullscreen)
        .def_readwrite("vsync", &lupine::core::ProjectSettings::vsync)
        .def_readwrite("target_frame_rate", &lupine::core::ProjectSettings::targetFrameRate);

    py::class_<lupine::core::Project, std::shared_ptr<lupine::core::Project>>(m, "Project")
        .def(py::init<>())
        .def(py::init<const std::string&>())
        .def("load", &lupine::core::Project::Load, py::arg("project_path"))
        .def("save", &lupine::core::Project::Save)
        .def("save_as", &lupine::core::Project::SaveAs, py::arg("project_path"))
        .def("get_settings",
             static_cast<lupine::core::ProjectSettings& (lupine::core::Project::*)()>(&lupine::core::Project::GetSettings),
             py::return_value_policy::reference_internal)
        .def("get_project_path", &lupine::core::Project::GetProjectPath)
        .def("get_project_directory", &lupine::core::Project::GetProjectDirectory)
        .def("get_scenes_directory", &lupine::core::Project::GetScenesDirectory)
        .def("get_assets_directory", &lupine::core::Project::GetAssetsDirectory)
        .def("get_scripts_directory", &lupine::core::Project::GetScriptsDirectory)
        .def("is_loaded", &lupine::core::Project::IsLoaded);

    py::class_<lupine::core::Scene, std::shared_ptr<lupine::core::Scene>>(m, "Scene")
        .def(py::init<>())
        .def(py::init<const std::string&>())
        .def("load", &lupine::core::Scene::Load, py::arg("filepath"))
        .def("save", static_cast<bool (lupine::core::Scene::*)(const std::string&)>(&lupine::core::Scene::Save),
             py::arg("filepath"))
        .def("save", static_cast<bool (lupine::core::Scene::*)()>(&lupine::core::Scene::Save))
        .def("get_name", &lupine::core::Scene::GetName)
        .def("set_name", &lupine::core::Scene::SetName, py::arg("name"))
        .def("get_file_path", &lupine::core::Scene::GetFilePath)
        .def("get_root", &lupine::core::Scene::GetRoot)
        .def("set_root", &lupine::core::Scene::SetRoot, py::arg("root"))
        .def("find_node_by_uuid", &lupine::core::Scene::FindNodeByUUID, py::arg("uuid"))
        .def("find_node", &lupine::core::Scene::FindNode, py::arg("path"))
        .def("initialize", &lupine::core::Scene::Initialize)
        .def("shutdown", &lupine::core::Scene::Shutdown)
        .def("is_initialized", &lupine::core::Scene::IsInitialized)
        .def("is_active", &lupine::core::Scene::IsActive)
        .def("set_active", &lupine::core::Scene::SetActive, py::arg("active"));

    py::class_<lupine::engine::SceneDocument, std::shared_ptr<lupine::engine::SceneDocument>>(m, "SceneDocument")
        .def(py::init<>())
        .def(py::init<std::shared_ptr<lupine::core::Scene>>())
        .def(py::init<const std::string&>())
        .def("get_id", &lupine::engine::SceneDocument::GetID)
        .def("get_scene", &lupine::engine::SceneDocument::GetScene)
        .def("set_scene", &lupine::engine::SceneDocument::SetScene, py::arg("scene"))
        .def("get_file_path", &lupine::engine::SceneDocument::GetFilePath)
        .def("set_file_path", &lupine::engine::SceneDocument::SetFilePath, py::arg("file_path"))
        .def("get_display_name", &lupine::engine::SceneDocument::GetDisplayName)
        .def("is_dirty", &lupine::engine::SceneDocument::IsDirty)
        .def("set_dirty", &lupine::engine::SceneDocument::SetDirty, py::arg("dirty"))
        .def("mark_dirty", &lupine::engine::SceneDocument::MarkDirty)
        .def_static("create_new", &lupine::engine::SceneDocument::CreateNew,
                   py::arg("scene_name") = "New Scene")
        .def_static("open", &lupine::engine::SceneDocument::Open, py::arg("scene_path"))
        .def("save", &lupine::engine::SceneDocument::Save)
        .def("save_as", &lupine::engine::SceneDocument::SaveAs, py::arg("new_path"))
        .def("close", &lupine::engine::SceneDocument::Close);

    py::class_<lupine::engine::EditorSession>(m, "EditorSession")
        .def(py::init<>())
        .def("initialize", &lupine::engine::EditorSession::Initialize)
        .def("shutdown", &lupine::engine::EditorSession::Shutdown)
        .def("is_initialized", &lupine::engine::EditorSession::IsInitialized)

        .def("create_project", &lupine::engine::EditorSession::CreateProject,
             py::arg("project_path"), py::arg("project_name"))
        .def("open_project", &lupine::engine::EditorSession::OpenProject,
             py::arg("project_path"))
        .def("close_project", &lupine::engine::EditorSession::CloseProject,
             py::arg("save_open_scenes") = true)
        .def("has_open_project", &lupine::engine::EditorSession::HasOpenProject)
        .def("get_project", &lupine::engine::EditorSession::GetProject)
        .def("get_recent_projects",
             static_cast<lupine::engine::RecentProjects& (lupine::engine::EditorSession::*)()>(&lupine::engine::EditorSession::GetRecentProjects),
             py::return_value_policy::reference_internal)

        .def("create_scene", &lupine::engine::EditorSession::CreateScene,
             py::arg("scene_name") = "New Scene")
        .def("open_scene", &lupine::engine::EditorSession::OpenScene,
             py::arg("scene_path"))
        .def("close_scene", &lupine::engine::EditorSession::CloseScene,
             py::arg("scene_id"), py::arg("save_if_dirty") = true)
        .def("close_all_scenes", &lupine::engine::EditorSession::CloseAllScenes,
             py::arg("save_if_dirty") = true)
        .def("save_scene", &lupine::engine::EditorSession::SaveScene,
             py::arg("scene_id"))
        .def("save_scene_as", &lupine::engine::EditorSession::SaveSceneAs,
             py::arg("scene_id"), py::arg("new_path"))
        .def("save_all_scenes", &lupine::engine::EditorSession::SaveAllScenes)
        .def("get_scene_document", &lupine::engine::EditorSession::GetSceneDocument,
             py::arg("scene_id"))
        .def("get_scene_document_by_path", &lupine::engine::EditorSession::GetSceneDocumentByPath,
             py::arg("scene_path"))
        .def("get_open_scenes", &lupine::engine::EditorSession::GetOpenScenes)
        .def("is_scene_open", &lupine::engine::EditorSession::IsSceneOpen,
             py::arg("scene_id"))
        .def("is_scene_open_by_path", &lupine::engine::EditorSession::IsSceneOpenByPath,
             py::arg("scene_path"))

        .def("set_active_scene", &lupine::engine::EditorSession::SetActiveScene,
             py::arg("scene_id"))
        .def("get_active_scene_document", &lupine::engine::EditorSession::GetActiveSceneDocument)
        .def("get_active_scene", &lupine::engine::EditorSession::GetActiveScene)
        .def("has_active_scene", &lupine::engine::EditorSession::HasActiveScene)
        .def("get_active_scene_id", &lupine::engine::EditorSession::GetActiveSceneID)

        .def("get_editor_bridge", [](lupine::engine::EditorSession& self) -> lupine::engine::EditorBridge* {
            return self.GetEditorBridge();
        }, py::return_value_policy::reference);

    py::enum_<lupine::GraphicsBackend>(m, "GraphicsBackend")
        .value("OpenGL", lupine::GraphicsBackend::OpenGL)
        .value("Vulkan", lupine::GraphicsBackend::Vulkan)
        .value("DirectX11", lupine::GraphicsBackend::DirectX11)
        .value("DirectX12", lupine::GraphicsBackend::DirectX12)
        .value("Metal", lupine::GraphicsBackend::Metal)
        .value("WebGL", lupine::GraphicsBackend::WebGL)
        .export_values();

    py::enum_<lupine::engine::ViewMode>(m, "ViewMode")
        .value("View2D", lupine::engine::ViewMode::View2D)
        .value("View3D", lupine::engine::ViewMode::View3D)
        .export_values();

    py::enum_<lupine::FilterMode>(m, "FilterMode")
        .value("Nearest", lupine::FilterMode::Nearest)
        .value("Linear", lupine::FilterMode::Linear)
        .export_values();

    py::enum_<lupine::GizmoType>(m, "GizmoType")
        .value("Translation", lupine::GizmoType::Translation)
        .value("Rotation", lupine::GizmoType::Rotation)
        .value("Scale", lupine::GizmoType::Scale)
        .value("All", lupine::GizmoType::All)
        .export_values();

    py::enum_<lupine::TransformSpace>(m, "TransformSpace")
        .value("Local", lupine::TransformSpace::Local)
        .value("Parent", lupine::TransformSpace::Parent)
        .value("World", lupine::TransformSpace::World)
        .export_values();

    py::class_<lupine::engine::TypeInfo>(m, "TypeInfo")
        .def(py::init<>())
        .def(py::init<const std::string&, const std::string&, bool>())
        .def(py::init<const std::string&, const std::string&, const std::string&, bool>())
        .def_readwrite("type_name", &lupine::engine::TypeInfo::typeName)
        .def_readwrite("category", &lupine::engine::TypeInfo::category)
        .def_readwrite("subcategory", &lupine::engine::TypeInfo::subcategory)
        .def_readwrite("file_path", &lupine::engine::TypeInfo::filePath)
        .def_readwrite("is_built_in", &lupine::engine::TypeInfo::isBuiltIn);

    py::class_<lupine::engine::RenderViewInfo>(m, "RenderViewInfo")
        .def(py::init<>())
        .def_readwrite("view_id", &lupine::engine::RenderViewInfo::viewID)
        .def_readwrite("width", &lupine::engine::RenderViewInfo::width)
        .def_readwrite("height", &lupine::engine::RenderViewInfo::height)
        .def_readwrite("is_valid", &lupine::engine::RenderViewInfo::isValid)
        .def_readwrite("view_mode", &lupine::engine::RenderViewInfo::viewMode)
        .def_readwrite("scene", &lupine::engine::RenderViewInfo::scene);

    py::class_<lupine::engine::Node, std::shared_ptr<lupine::engine::Node>>(m, "Node")
        .def("get_uuid", &lupine::engine::Node::GetUUID)
        .def("get_name", &lupine::engine::Node::GetName)
        .def("set_name", &lupine::engine::Node::SetName)
        .def("get_type_name", &lupine::engine::Node::GetTypeName)
        .def("get_path", &lupine::engine::Node::GetPath)
        .def("is_active", &lupine::engine::Node::IsActive)
        .def("set_active", &lupine::engine::Node::SetActive)
        .def("is_visible", &lupine::engine::Node::IsVisible)
        .def("set_visible", &lupine::engine::Node::SetVisible)
        .def("get_children", &lupine::engine::Node::GetChildren)
        .def("get_child_count", &lupine::engine::Node::GetChildCount)
        .def("get_components", &lupine::engine::Node::GetComponents)
        .def("add_component", &lupine::engine::Node::AddComponent)
        .def("remove_component", &lupine::engine::Node::RemoveComponent)
        .def("get_parent", &lupine::engine::Node::GetParent)
        .def("add_child", &lupine::engine::Node::AddChild)
        .def("remove_child", static_cast<void (lupine::engine::Node::*)(std::shared_ptr<lupine::engine::Node>)>(&lupine::engine::Node::RemoveChild))
        .def("get_child", static_cast<std::shared_ptr<lupine::engine::Node> (lupine::engine::Node::*)(const std::string&) const>(&lupine::engine::Node::GetChild))
        .def("on_ready", &lupine::engine::Node::OnReady, "Initialize the node and all its components");

    py::class_<lupine::engine::Component, std::shared_ptr<lupine::engine::Component>>(m, "Component")
        .def("get_type_name", &lupine::engine::Component::GetTypeName)
        .def("is_enabled", &lupine::engine::Component::IsEnabled)
        .def("set_enabled", &lupine::engine::Component::SetEnabled);

    py::class_<lupine::components::Button, lupine::core::Component, std::shared_ptr<lupine::components::Button>>(m, "Button")
        .def("PreviewTween", &lupine::components::Button::PreviewTween, py::arg("state"),
             "Preview tween for a specific button state (0=Normal, 1=Hover, 2=Pressed, 3=Disabled)");

    py::class_<lupine::components::AudioPlayer, lupine::core::Component, std::shared_ptr<lupine::components::AudioPlayer>>(m, "AudioPlayer")
        .def("play", &lupine::components::AudioPlayer::Play,
             "Play the audio")
        .def("stop", &lupine::components::AudioPlayer::Stop,
             "Stop the audio")
        .def("pause", &lupine::components::AudioPlayer::Pause,
             "Pause the audio")
        .def("resume", &lupine::components::AudioPlayer::Resume,
             "Resume the audio")
        .def("is_playing", &lupine::components::AudioPlayer::IsPlaying,
             "Check if audio is playing")
        .def("set_volume", &lupine::components::AudioPlayer::SetVolume,
             py::arg("volume"),
             "Set volume (0.0 - 1.0)")
        .def("get_volume", &lupine::components::AudioPlayer::GetVolume,
             "Get volume")
        .def("set_pitch", &lupine::components::AudioPlayer::SetPitch,
             py::arg("pitch"),
             "Set pitch (0.5 - 2.0)")
        .def("get_pitch", &lupine::components::AudioPlayer::GetPitch,
             "Get pitch")
        .def("set_loop", &lupine::components::AudioPlayer::SetLoop,
             py::arg("loop"),
             "Set loop mode")
        .def("get_loop", &lupine::components::AudioPlayer::GetLoop,
             "Get loop mode")
        .def("set_bus", &lupine::components::AudioPlayer::SetBus,
             py::arg("bus_name"),
             "Set audio bus")
        .def("get_bus", &lupine::components::AudioPlayer::GetBus,
             "Get audio bus");

    py::class_<lupine::components::AudioListener, lupine::core::Component, std::shared_ptr<lupine::components::AudioListener>>(m, "AudioListener")
        .def("set_active", &lupine::components::AudioListener::SetActive,
             py::arg("active"),
             "Set listener active state")
        .def("is_active", &lupine::components::AudioListener::IsActive,
             "Check if listener is active");

    py::class_<lupine::engine::Prefab, std::shared_ptr<lupine::engine::Prefab>>(m, "Prefab")
        .def(py::init<>())
        .def(py::init<const std::string&>())
        .def("get_name", &lupine::engine::Prefab::GetName)
        .def("set_name", &lupine::engine::Prefab::SetName)
        .def("load", &lupine::engine::Prefab::Load)
        .def("save", static_cast<bool (lupine::engine::Prefab::*)()>(&lupine::engine::Prefab::Save))
        .def("is_valid", &lupine::engine::Prefab::IsValid)
        .def("instantiate", &lupine::engine::Prefab::Instantiate);

    py::class_<lupine::engine::EditorBridge>(m, "EditorBridge")
        .def(py::init<>())

        .def("initialize", &lupine::engine::EditorBridge::Initialize,
             py::arg("backend") = lupine::GraphicsBackend::OpenGL)
        .def("shutdown", &lupine::engine::EditorBridge::Shutdown)
        .def("is_initialized", &lupine::engine::EditorBridge::IsInitialized)

        .def("create_render_view", [](lupine::engine::EditorBridge& self, uintptr_t window_handle, uint32_t width, uint32_t height) {
            return self.CreateRenderView(reinterpret_cast<void*>(window_handle), width, height);
        }, py::arg("window_handle"), py::arg("width"), py::arg("height"))
        .def("destroy_render_view", &lupine::engine::EditorBridge::DestroyRenderView,
             py::arg("view_id"))
        .def("resize_render_view", &lupine::engine::EditorBridge::ResizeRenderView,
             py::arg("view_id"), py::arg("width"), py::arg("height"))
        .def("render_view", &lupine::engine::EditorBridge::RenderView,
             py::arg("view_id"))
        .def("get_render_view_info", &lupine::engine::EditorBridge::GetRenderViewInfo,
             py::arg("view_id"))
        .def("set_view_mode", &lupine::engine::EditorBridge::SetViewMode,
             py::arg("view_id"), py::arg("mode"))
        .def("get_view_mode", &lupine::engine::EditorBridge::GetViewMode,
             py::arg("view_id"))
        .def("set_view_scene", &lupine::engine::EditorBridge::SetViewScene,
             py::arg("view_id"), py::arg("scene"))
        .def("get_view_scene", &lupine::engine::EditorBridge::GetViewScene,
             py::arg("view_id"))
        .def("set_view_clear_color", [](lupine::engine::EditorBridge& self, lupine::RenderViewID viewID,
                                         float r, float g, float b, float a) {
            self.SetViewClearColor(viewID, lupine::math::Color(r, g, b, a));
        }, py::arg("view_id"), py::arg("r"), py::arg("g"), py::arg("b"), py::arg("a"))
        .def("set_view_debug_rendering_enabled", &lupine::engine::EditorBridge::SetViewDebugRenderingEnabled,
             py::arg("view_id"), py::arg("enabled"))
        .def("set_view_grid_rendering_enabled", &lupine::engine::EditorBridge::SetViewGridRenderingEnabled,
             py::arg("view_id"), py::arg("enabled"))
        .def("set_view_camera_preview_enabled", &lupine::engine::EditorBridge::SetViewCameraPreviewEnabled,
             py::arg("view_id"), py::arg("enabled"))
        .def("set_view_collision_shapes_enabled", &lupine::engine::EditorBridge::SetViewCollisionShapesEnabled,
             py::arg("view_id"), py::arg("enabled"))
        .def("set_project_settings", &lupine::engine::EditorBridge::SetProjectSettings,
             py::arg("window_width"), py::arg("window_height"))
        .def("set_texture_filtering", &lupine::engine::EditorBridge::SetTextureFiltering,
             py::arg("min_filter"), py::arg("mag_filter"))

        .def("pan_camera_2d", &lupine::engine::EditorBridge::PanCamera2D,
             py::arg("view_id"), py::arg("delta_x"), py::arg("delta_y"))
        .def("zoom_camera_2d", &lupine::engine::EditorBridge::ZoomCamera2D,
             py::arg("view_id"), py::arg("delta"))
        .def("pan_camera_3d", &lupine::engine::EditorBridge::PanCamera3D,
             py::arg("view_id"), py::arg("delta_x"), py::arg("delta_y"))
        .def("orbit_camera_3d", &lupine::engine::EditorBridge::OrbitCamera3D,
             py::arg("view_id"), py::arg("delta_x"), py::arg("delta_y"))
        .def("zoom_camera_3d", &lupine::engine::EditorBridge::ZoomCamera3D,
             py::arg("view_id"), py::arg("delta"))
        .def("set_respect_floor", &lupine::engine::EditorBridge::SetRespectFloor,
             py::arg("view_id"), py::arg("enabled"))
        .def("get_respect_floor", &lupine::engine::EditorBridge::GetRespectFloor,
             py::arg("view_id"))

        .def("load_scene", &lupine::engine::EditorBridge::LoadScene,
             py::arg("scene_path"))
        .def("set_active_scene", &lupine::engine::EditorBridge::SetActiveScene,
             py::arg("scene"))
        .def("get_active_scene", &lupine::engine::EditorBridge::GetActiveScene)
        .def("save_active_scene", &lupine::engine::EditorBridge::SaveActiveScene)
        .def("is_active_scene_dirty", &lupine::engine::EditorBridge::IsActiveSceneDirty)
        .def("mark_scene_dirty", &lupine::engine::EditorBridge::MarkSceneDirty)

        .def("scan_project_types", &lupine::engine::EditorBridge::ScanProjectTypes,
             py::arg("project_path"))
        .def("get_node_types", &lupine::engine::EditorBridge::GetNodeTypes)
        .def("get_component_types", &lupine::engine::EditorBridge::GetComponentTypes)
        .def("get_prefab_types", &lupine::engine::EditorBridge::GetPrefabTypes)
        .def("load_prefab", &lupine::engine::EditorBridge::LoadPrefab,
             py::arg("prefab_path"))

        .def("get_node", &lupine::engine::EditorBridge::GetNode,
             py::arg("node_uuid"))
        .def("get_node_by_path", &lupine::engine::EditorBridge::GetNodeByPath,
             py::arg("path"))
        .def("get_root_node", &lupine::engine::EditorBridge::GetRootNode)
        .def("create_node", &lupine::engine::EditorBridge::CreateNode,
             py::arg("type_name"), py::arg("node_name") = "")
        .def("add_node", &lupine::engine::EditorBridge::AddNode,
             py::arg("node"), py::arg("parent") = nullptr)
        .def("remove_node", &lupine::engine::EditorBridge::RemoveNode,
             py::arg("node"))
        .def("reparent_node", &lupine::engine::EditorBridge::ReparentNode,
             py::arg("node"), py::arg("new_parent"))
        .def("get_children", &lupine::engine::EditorBridge::GetChildren,
             py::arg("node"))
        .def("create_component", &lupine::engine::EditorBridge::CreateComponent,
             py::arg("type_name"))
        .def("add_component", &lupine::engine::EditorBridge::AddComponent,
             py::arg("node"), py::arg("component"))
        .def("add_component_direct", &lupine::engine::EditorBridge::AddComponentDirect,
             py::arg("node"), py::arg("component"))
        .def("remove_component", &lupine::engine::EditorBridge::RemoveComponent,
             py::arg("node"), py::arg("component"))
        .def("get_components", &lupine::engine::EditorBridge::GetComponents,
             py::arg("node"))

        .def("get_node_property", [](lupine::engine::EditorBridge& self,
                                      std::shared_ptr<lupine::engine::Node> node,
                                      const std::string& propertyName) {
            auto json = self.GetNodeProperty(node, propertyName);
            return json.dump();
        }, py::arg("node"), py::arg("property_name"))
        .def("set_node_property", [](lupine::engine::EditorBridge& self,
                                      std::shared_ptr<lupine::engine::Node> node,
                                      const std::string& propertyName,
                                      const std::string& valueJson) {
            auto json = nlohmann::json::parse(valueJson);
            return self.SetNodeProperty(node, propertyName, json);
        }, py::arg("node"), py::arg("property_name"), py::arg("value_json"))
        .def("get_node_properties", [](lupine::engine::EditorBridge& self,
                                        std::shared_ptr<lupine::engine::Node> node) {
            auto json = self.GetNodeProperties(node);
            return json.dump();
        }, py::arg("node"))
        .def("get_component_property", [](lupine::engine::EditorBridge& self,
                                           std::shared_ptr<lupine::engine::Component> component,
                                           const std::string& propertyName) {
            auto json = self.GetComponentProperty(component, propertyName);
            return json.dump();
        }, py::arg("component"), py::arg("property_name"))
        .def("set_component_property", [](lupine::engine::EditorBridge& self,
                                           std::shared_ptr<lupine::engine::Component> component,
                                           const std::string& propertyName,
                                           const std::string& valueJson) {
            auto json = nlohmann::json::parse(valueJson);
            return self.SetComponentProperty(component, propertyName, json);
        }, py::arg("component"), py::arg("property_name"), py::arg("value_json"))
        .def("set_component_property_direct", [](lupine::engine::EditorBridge& self,
                                                   std::shared_ptr<lupine::engine::Component> component,
                                                   const std::string& propertyName,
                                                   const std::string& valueJson) {
            auto json = nlohmann::json::parse(valueJson);
            return self.SetComponentPropertyDirect(component, propertyName, json);
        }, py::arg("component"), py::arg("property_name"), py::arg("value_json"),
           "Set component property directly without creating a command (for duplication/copy-paste)")
        .def("set_node_property_direct", [](lupine::engine::EditorBridge& self,
                                              std::shared_ptr<lupine::engine::Node> node,
                                              const std::string& propertyName,
                                              const std::string& valueJson) {
            auto json = nlohmann::json::parse(valueJson);
            return self.SetNodePropertyDirect(node, propertyName, json);
        }, py::arg("node"), py::arg("property_name"), py::arg("value_json"),
           "Set node property directly without creating a command (for duplication/copy-paste)")
        .def("deserialize_component_direct", [](lupine::engine::EditorBridge& self,
                                                  std::shared_ptr<lupine::engine::Component> component,
                                                  const std::string& componentDataJson) {
            auto json = nlohmann::json::parse(componentDataJson);
            return self.DeserializeComponentDirect(component, json);
        }, py::arg("component"), py::arg("component_data_json"),
           "Deserialize component data directly without triggering individual property changes (for duplication/copy-paste)")
        .def("get_component_properties", [](lupine::engine::EditorBridge& self,
                                             std::shared_ptr<lupine::engine::Component> component) {
            auto json = self.GetComponentProperties(component);
            return json.dump();
        }, py::arg("component"))

        .def("get_material_slot_count", &lupine::engine::EditorBridge::GetMaterialSlotCount,
             py::arg("component"))
        .def("get_material_slot_properties", [](lupine::engine::EditorBridge& self,
                                                  std::shared_ptr<lupine::engine::Component> component,
                                                  uint32_t slotIndex) {
            auto json = self.GetMaterialSlotProperties(component, slotIndex);
            return json.dump();
        }, py::arg("component"), py::arg("slot_index"))
        .def("set_material_slot_property", [](lupine::engine::EditorBridge& self,
                                                std::shared_ptr<lupine::engine::Component> component,
                                                uint32_t slotIndex,
                                                const std::string& propertyName,
                                                const std::string& valueJson) {
            auto json = nlohmann::json::parse(valueJson);
            return self.SetMaterialSlotProperty(component, slotIndex, propertyName, json);
        }, py::arg("component"), py::arg("slot_index"), py::arg("property_name"), py::arg("value_json"))

        .def("instantiate_prefab", &lupine::engine::EditorBridge::InstantiatePrefab,
             py::arg("prefab"), py::arg("parent") = nullptr)

        .def("pick_node_at_screen_position", &lupine::engine::EditorBridge::PickNodeAtScreenPosition,
             py::arg("view_id"), py::arg("screen_x"), py::arg("screen_y"),
             "Pick a node at the given screen coordinates in the viewport")

        .def("set_selected_node", &lupine::engine::EditorBridge::SetSelectedNode,
             py::arg("view_id"), py::arg("node"),
             "Set the selected node for a render view")
        .def("set_selected_nodes", &lupine::engine::EditorBridge::SetSelectedNodes,
             py::arg("view_id"), py::arg("nodes"),
             "Set multiple selected nodes for a render view")
        .def("get_selected_nodes", &lupine::engine::EditorBridge::GetSelectedNodes,
             py::arg("view_id"),
             "Get all selected nodes for a render view")
        .def("get_selected_node", &lupine::engine::EditorBridge::GetSelectedNode,
             py::arg("view_id"),
             "Get the selected node for a render view")

        .def("set_gizmo_enabled", &lupine::engine::EditorBridge::SetGizmoEnabled,
             py::arg("view_id"), py::arg("enabled"),
             "Enable/disable gizmo rendering for a render view")
        .def("is_gizmo_enabled", &lupine::engine::EditorBridge::IsGizmoEnabled,
             py::arg("view_id"),
             "Check if gizmo rendering is enabled for a render view")

        .def("create_composite_command", &lupine::engine::EditorBridge::CreateCompositeCommand,
             py::arg("description"),
             "Create a composite command for grouping multiple operations")
        .def("create_add_node_command", &lupine::engine::EditorBridge::CreateAddNodeCommand,
             py::arg("node"), py::arg("parent") = nullptr,
             "Create an AddNodeCommand without executing it")
        .def("create_add_component_command", &lupine::engine::EditorBridge::CreateAddComponentCommand,
             py::arg("node"), py::arg("component"),
             "Create an AddComponentCommand without executing it")
        .def("create_set_node_property_command",
             [](lupine::engine::EditorBridge& self, std::shared_ptr<lupine::core::Node> node,
                const std::string& propertyName, const std::string& valueJson) {
                 return self.CreateSetNodePropertyCommand(node, propertyName, nlohmann::json::parse(valueJson));
             },
             py::arg("node"), py::arg("property_name"), py::arg("value_json"),
             "Create a SetNodePropertyCommand without executing it")
        .def("create_set_component_property_command",
             [](lupine::engine::EditorBridge& self, std::shared_ptr<lupine::core::Component> component,
                const std::string& propertyName, const std::string& valueJson) {
                 return self.CreateSetComponentPropertyCommand(component, propertyName, nlohmann::json::parse(valueJson));
             },
             py::arg("component"), py::arg("property_name"), py::arg("value_json"),
             "Create a SetComponentPropertyCommand without executing it")
        .def("execute_command", &lupine::engine::EditorBridge::ExecuteCommand,
             py::arg("command"),
             "Execute a command and add it to the undo history")
        .def("record_command", &lupine::engine::EditorBridge::RecordCommand,
             py::arg("command"),
             "Record a command in the undo history without executing it (for commands already executed)")
        .def("undo", &lupine::engine::EditorBridge::Undo,
             "Undo the last command")
        .def("redo", &lupine::engine::EditorBridge::Redo,
             "Redo the last undone command")
        .def("can_undo", &lupine::engine::EditorBridge::CanUndo,
             "Check if undo is available")
        .def("can_redo", &lupine::engine::EditorBridge::CanRedo,
             "Check if redo is available")
        .def("get_undo_description", &lupine::engine::EditorBridge::GetUndoDescription,
             "Get description of the command that would be undone")
        .def("get_redo_description", &lupine::engine::EditorBridge::GetRedoDescription,
             "Get description of the command that would be redone")
        .def("clear_history", &lupine::engine::EditorBridge::ClearHistory,
             "Clear all undo/redo history")
        .def("set_max_undo_steps", &lupine::engine::EditorBridge::SetMaxUndoSteps,
             py::arg("max_steps"),
             "Set the maximum number of undo steps")
        .def("get_max_undo_steps", &lupine::engine::EditorBridge::GetMaxUndoSteps,
             "Get the maximum number of undo steps")
        .def("set_gizmo_type", &lupine::engine::EditorBridge::SetGizmoType,
             py::arg("view_id"), py::arg("type"),
             "Set the gizmo type for a render view")
        .def("get_gizmo_type", &lupine::engine::EditorBridge::GetGizmoType,
             py::arg("view_id"),
             "Get the gizmo type for a render view")
        .def("set_transform_space", &lupine::engine::EditorBridge::SetTransformSpace,
             py::arg("view_id"), py::arg("space"),
             "Set the transform space for gizmo operations")
        .def("get_transform_space", &lupine::engine::EditorBridge::GetTransformSpace,
             py::arg("view_id"),
             "Get the transform space for gizmo operations")

        .def("test_gizmo_hit", &lupine::engine::EditorBridge::TestGizmoHit,
             py::arg("view_id"), py::arg("screen_x"), py::arg("screen_y"),
             "Test if the gizmo is hit at a screen position")
        .def("begin_gizmo_drag", &lupine::engine::EditorBridge::BeginGizmoDrag,
             py::arg("view_id"), py::arg("screen_x"), py::arg("screen_y"),
             "Start gizmo drag interaction")
        .def("update_gizmo_drag", &lupine::engine::EditorBridge::UpdateGizmoDrag,
             py::arg("view_id"), py::arg("screen_x"), py::arg("screen_y"),
             "Update gizmo drag interaction")
        .def("end_gizmo_drag", &lupine::engine::EditorBridge::EndGizmoDrag,
             py::arg("view_id"),
             "End gizmo drag interaction")
        .def("is_gizmo_dragging", &lupine::engine::EditorBridge::IsGizmoDragging,
             py::arg("view_id"),
             "Check if gizmo is currently being dragged")

        .def("play_audio_file", &lupine::engine::EditorBridge::PlayAudioFile,
             py::arg("file_path"),
             py::arg("bus_name") = "Master",
             py::arg("loop") = false,
             py::arg("volume") = 1.0f,
             "Play an audio file (for editor audio preview)")
        .def("stop_audio", &lupine::engine::EditorBridge::StopAudio,
             py::arg("source_uuid"),
             "Stop a playing audio source by UUID")
        .def("stop_all_audio", &lupine::engine::EditorBridge::StopAllAudio,
             "Stop all playing audio")
        .def("play_scene_audio", &lupine::engine::EditorBridge::PlaySceneAudio,
             py::arg("scene"),
             "Play all AudioPlayer components in a scene")
        .def("stop_scene_audio", &lupine::engine::EditorBridge::StopSceneAudio,
             py::arg("scene"),
             "Stop all AudioPlayer components in a scene")
        .def("set_master_volume", &lupine::engine::EditorBridge::SetMasterVolume,
             py::arg("volume"),
             "Set master audio volume")
        .def("get_master_volume", &lupine::engine::EditorBridge::GetMasterVolume,
             "Get master audio volume")
        .def("set_bus_volume", &lupine::engine::EditorBridge::SetBusVolume,
             py::arg("bus_name"),
             py::arg("volume"),
             "Set audio bus volume")
        .def("get_bus_volume", &lupine::engine::EditorBridge::GetBusVolume,
             py::arg("bus_name"),
             "Get audio bus volume")

        .def("screen_to_world_2d", [](lupine::engine::EditorBridge& self,
                                       int viewId,
                                       float screenX,
                                       float screenY) {
            auto worldPos = self.ScreenToWorld2D(viewId, screenX, screenY);
            py::dict result;
            result["x"] = worldPos.x;
            result["y"] = worldPos.y;
            return result;
        }, py::arg("view_id"), py::arg("screen_x"), py::arg("screen_y"),
           "Convert screen coordinates to world coordinates for 2D view")
        .def("screen_to_world_3d", [](lupine::engine::EditorBridge& self,
                                       int viewId,
                                       float screenX,
                                       float screenY,
                                       float planeY) {
            auto worldPos = self.ScreenToWorld3D(viewId, screenX, screenY, planeY);
            py::dict result;
            result["x"] = worldPos.x;
            result["y"] = worldPos.y;
            result["z"] = worldPos.z;
            return result;
        }, py::arg("view_id"), py::arg("screen_x"), py::arg("screen_y"), py::arg("plane_y") = 0.0f,
           "Cast a ray from screen coordinates into 3D world and intersect with horizontal plane")
        .def("add_collision_vertex", &lupine::engine::EditorBridge::AddCollisionVertex,
             py::arg("component"), py::arg("x"), py::arg("y"),
             "Add a vertex to a CollisionBody2D polygon")
        .def("remove_collision_vertex", &lupine::engine::EditorBridge::RemoveCollisionVertex,
             py::arg("component"), py::arg("index"),
             "Remove a vertex from a CollisionBody2D polygon")
        .def("update_collision_vertex", &lupine::engine::EditorBridge::UpdateCollisionVertex,
             py::arg("component"), py::arg("index"), py::arg("x"), py::arg("y"),
             "Update a vertex position in a CollisionBody2D polygon")
        .def("get_collision_vertices", &lupine::engine::EditorBridge::GetCollisionVertices,
             py::arg("component"),
             "Get all vertices from a CollisionBody2D polygon as JSON string")
        .def("clear_collision_vertices", &lupine::engine::EditorBridge::ClearCollisionVertices,
             py::arg("component"),
             "Clear all vertices from a CollisionBody2D polygon")
        .def("get_node_global_position", [](lupine::engine::EditorBridge& self, std::shared_ptr<lupine::core::Node> node) {
            std::string jsonStr = self.GetNodeGlobalPosition(node);
            py::object json_module = py::module::import("json");
            return json_module.attr("loads")(jsonStr);
        }, py::arg("node"),
           "Get the global position of a node (2D or 3D) as a dict")
        .def("get_node_global_rotation", &lupine::engine::EditorBridge::GetNodeGlobalRotation,
             py::arg("node"),
             "Get the global rotation of a node (2D returns float, 3D returns 0.0 for now)")

        .def("create_voxel_builder", &lupine::engine::EditorBridge::CreateVoxelBuilder,
             "Create a voxel builder instance")
        .def("destroy_voxel_builder", &lupine::engine::EditorBridge::DestroyVoxelBuilder,
             py::arg("builder_id"),
             "Destroy a voxel builder instance")
        .def("voxel_builder_place_voxel", &lupine::engine::EditorBridge::VoxelBuilderPlaceVoxel,
             py::arg("builder_id"), py::arg("x"), py::arg("y"), py::arg("z"),
             py::arg("r"), py::arg("g"), py::arg("b"), py::arg("a"),
             "Place a voxel")
        .def("voxel_builder_erase_voxel", &lupine::engine::EditorBridge::VoxelBuilderEraseVoxel,
             py::arg("builder_id"), py::arg("x"), py::arg("y"), py::arg("z"),
             "Erase a voxel")
        .def("voxel_builder_clear", &lupine::engine::EditorBridge::VoxelBuilderClear,
             py::arg("builder_id"),
             "Clear all voxels")
        .def("voxel_builder_has_voxel", &lupine::engine::EditorBridge::VoxelBuilderHasVoxel,
             py::arg("builder_id"), py::arg("x"), py::arg("y"), py::arg("z"),
             "Check if a voxel exists at the given position")
        .def("voxel_builder_get_voxel_count", &lupine::engine::EditorBridge::VoxelBuilderGetVoxelCount,
             py::arg("builder_id"),
             "Get voxel count")
        .def("voxel_builder_render", &lupine::engine::EditorBridge::VoxelBuilderRender,
             py::arg("builder_id"), py::arg("view_id"),
             "Render voxel builder to view")
        .def("voxel_builder_to_json", &lupine::engine::EditorBridge::VoxelBuilderToJSON,
             py::arg("builder_id"),
             "Export voxel builder to JSON")
        .def("voxel_builder_from_json", &lupine::engine::EditorBridge::VoxelBuilderFromJSON,
             py::arg("builder_id"), py::arg("json"),
             "Load voxel builder from JSON")
        .def("voxel_builder_export_obj", &lupine::engine::EditorBridge::VoxelBuilderExportOBJ,
             py::arg("builder_id"),
             py::arg("merge_faces") = false,
             py::arg("texture_atlas") = false,
             "Export voxel builder to OBJ")
        .def("voxel_builder_export_gltf", &lupine::engine::EditorBridge::VoxelBuilderExportGLTF,
             py::arg("builder_id"),
             py::arg("merge_faces") = false,
             py::arg("texture_atlas") = false,
             "Export voxel builder to glTF")

        .def("debug_draw_line", &lupine::engine::EditorBridge::DebugDrawLine,
             py::arg("view_id"),
             py::arg("x1"), py::arg("y1"), py::arg("z1"),
             py::arg("x2"), py::arg("y2"), py::arg("z2"),
             py::arg("r"), py::arg("g"), py::arg("b"), py::arg("a"),
             "Draw a debug line in the viewport")
        .def("debug_draw_aabb", &lupine::engine::EditorBridge::DebugDrawAABB,
             py::arg("view_id"),
             py::arg("min_x"), py::arg("min_y"), py::arg("min_z"),
             py::arg("max_x"), py::arg("max_y"), py::arg("max_z"),
             py::arg("r"), py::arg("g"), py::arg("b"), py::arg("a"),
             "Draw a debug bounding box (AABB) in the viewport");
}
