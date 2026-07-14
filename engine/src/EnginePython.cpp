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
#include "lupine/asset/AssetDatabase.hpp"
#include "lupine/components/Button.hpp"
#include "lupine/components/AudioPlayer.hpp"
#include "lupine/components/AudioListener.hpp"
#include "lupine/import/GodotSceneParser.hpp"
#include "lupine/import/GodotSceneImporter.hpp"

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

    py::enum_<lupine::engine::DocumentKind>(m, "DocumentKind")
        .value("Scene", lupine::engine::DocumentKind::Scene)
        .value("Prefab", lupine::engine::DocumentKind::Prefab);

    py::class_<lupine::engine::SceneDocument, std::shared_ptr<lupine::engine::SceneDocument>>(m, "SceneDocument")
        .def(py::init<>())
        .def(py::init<std::shared_ptr<lupine::core::Scene>>())
        .def(py::init<const std::string&>())
        .def("get_kind", &lupine::engine::SceneDocument::GetKind)
        .def("set_kind", &lupine::engine::SceneDocument::SetKind, py::arg("kind"))
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
        .def_static("open_prefab", &lupine::engine::SceneDocument::OpenPrefab, py::arg("prefab_path"))
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
        .def("open_prefab", &lupine::engine::EditorSession::OpenPrefab,
             py::arg("prefab_path"))
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
        .def("is_unique_name_in_owner", &lupine::engine::Node::IsUniqueNameInOwner)
        .def("set_unique_name_in_owner", &lupine::engine::Node::SetUniqueNameInOwner)
        .def("resolve_unique_name", [](lupine::engine::Node& self, const std::string& name) {
            lupine::core::Node* found = self.ResolveUniqueName(name);
            return found ? found->shared_from_this() : std::shared_ptr<lupine::core::Node>();
        }, py::arg("name"))
        .def("get_children", &lupine::engine::Node::GetChildren)
        .def("get_child_count", &lupine::engine::Node::GetChildCount)
        .def("get_components", &lupine::engine::Node::GetComponents)
        .def("add_component", &lupine::engine::Node::AddComponent)
        .def("remove_component", &lupine::engine::Node::RemoveComponent)
        .def("get_parent", &lupine::engine::Node::GetParentShared)
        .def("add_child", &lupine::engine::Node::AddChild)
        .def("insert_child", &lupine::engine::Node::InsertChild, py::arg("child"), py::arg("index"))
        .def("remove_child", static_cast<void (lupine::engine::Node::*)(std::shared_ptr<lupine::engine::Node>)>(&lupine::engine::Node::RemoveChild))
        .def("get_child", static_cast<std::shared_ptr<lupine::engine::Node> (lupine::engine::Node::*)(const std::string&) const>(&lupine::engine::Node::GetChild))
        .def("move_child", &lupine::engine::Node::MoveChild, py::arg("child"), py::arg("new_index"),
             "Reorder a direct child to a new index (pure reorder, stays in tree)")
        .def("get_child_index", [](lupine::engine::Node& self, std::shared_ptr<lupine::engine::Node> child) {
            return self.GetChildIndex(child.get());
        }, py::arg("child"), "Index of a direct child, or -1 if not a child")
        .def("get_sibling_index", &lupine::engine::Node::GetIndexInParent,
             "This node's index within its parent, or -1 if it has no parent")
        .def("set_sibling_index", [](lupine::engine::Node& self, size_t index) {
            self.SetSiblingIndex(index);
        }, py::arg("index"), "Move this node to a new position among its siblings")
        .def("on_ready", &lupine::engine::Node::OnReady, "Initialize the node and all its components");

    py::class_<lupine::engine::Component, std::shared_ptr<lupine::engine::Component>>(m, "Component")
        .def("get_name", &lupine::engine::Component::GetName)
        .def("get_type_name", &lupine::engine::Component::GetTypeName)
        .def("get_uuid", &lupine::engine::Component::GetUUID)
        .def("is_enabled", &lupine::engine::Component::IsEnabled)
        .def("set_enabled", &lupine::engine::Component::SetEnabled)
        .def("get_type_chain", &lupine::engine::Component::GetTypeChain,
             "Ancestry from most-derived to least-derived, e.g. ['FireSprite','MySprite','Sprite2D']")
        .def("is_instance_of", &lupine::engine::Component::IsInstanceOf, py::arg("type_name"),
             "True if this component is, or derives from, the named type");

    // The UI layout base. Registering it means every UIControl-derived component (Label,
    // Panel, Button, any Container, ...) surfaces these queries in the editor, because
    // pybind returns each component as its most-derived REGISTERED type.
    //
    // The editor needs them to stop presenting inert fields as editable: roughly half the
    // Layout group is dead in any given configuration (offsets do nothing in Position mode;
    // width/height do nothing on an anchor-stretched axis; everything does nothing under a
    // container), and previously nothing said so -- the user edited a number, watched it
    // revert, and got no explanation.
    py::class_<lupine::components::UIControl, lupine::core::Component,
               std::shared_ptr<lupine::components::UIControl>>(m, "UIControl")
        .def("is_container_driven", &lupine::components::UIControl::IsContainerDriven,
             "True if a parent container owns this control's rect, so width/height, anchors "
             "and offsets are all overwritten on the next layout pass")
        .def("is_layout_container", &lupine::components::UIControl::IsLayoutContainer,
             "True if this control arranges its children")
        .def("get_axis_driver", [](lupine::components::UIControl& self, bool vertical) {
            return static_cast<int>(self.GetAxisDriver(vertical));
        }, py::arg("vertical"),
             "Which property actually drives the given axis: 0 = width/height, 1 = offsets "
             "(anchor-stretched), 2 = container-driven (nothing the user edits can stick)")
        .def("get_rect", [](lupine::components::UIControl& self) {
            const lupine::math::Rect r = self.GetRect();
            py::dict out;
            out["x"] = r.position.x;
            out["y"] = r.position.y;
            out["width"] = r.size.x;
            out["height"] = r.size.y;
            out["left"] = r.GetLeftEdge();
            out["right"] = r.GetRightEdge();
            out["top"] = r.GetTopEdge();
            out["bottom"] = r.GetBottomEdge();
            return out;
        }, "The control's resolved global rect (Y-up canvas; 'y' is the BOTTOM edge)")
        .def("get_min_size", [](lupine::components::UIControl& self) {
            const lupine::math::Vec2 v = self.GetMinSize();
            return py::make_tuple(v.x, v.y);
        }, "Effective minimum size: max(customMinSize, content minimum)")
        .def("get_desired_size", [](lupine::components::UIControl& self) {
            const lupine::math::Vec2 v = self.GetDesiredSize();
            return py::make_tuple(v.x, v.y);
        }, "The intrinsic size this control asks a container for");

    py::class_<lupine::components::Button, lupine::components::UIControl,
               std::shared_ptr<lupine::components::Button>>(m, "Button")
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
        .def("save_to", static_cast<bool (lupine::engine::Prefab::*)(const std::string&)>(&lupine::engine::Prefab::Save),
             py::arg("filepath"))
        .def("create_from_node", &lupine::engine::Prefab::CreateFromNode, py::arg("root_node"))
        .def("get_file_path", &lupine::engine::Prefab::GetFilePath)
        .def("is_valid", &lupine::engine::Prefab::IsValid)
        .def("instantiate", &lupine::engine::Prefab::Instantiate)
        .def("instantiate_for_editing", &lupine::engine::Prefab::InstantiateForEditing);

    py::class_<lupine::engine::EditorBridge>(m, "EditorBridge")
        .def(py::init<>())

        .def("initialize", &lupine::engine::EditorBridge::Initialize,
             py::arg("backend") = lupine::GraphicsBackend::OpenGL)
        .def("shutdown", &lupine::engine::EditorBridge::Shutdown)
        .def("is_initialized", &lupine::engine::EditorBridge::IsInitialized)
        .def("get_backend", &lupine::engine::EditorBridge::GetBackend)

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
        .def("set_view_camera_effects_preview", &lupine::engine::EditorBridge::SetViewCameraEffectsPreview,
             py::arg("view_id"), py::arg("enabled"))
        .def("get_view_camera_effects_preview", &lupine::engine::EditorBridge::GetViewCameraEffectsPreview,
             py::arg("view_id"))
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
        .def("set_scene_dirty", &lupine::engine::EditorBridge::SetSceneDirty, py::arg("dirty"))

        .def("scan_project_types", &lupine::engine::EditorBridge::ScanProjectTypes,
             py::arg("project_path"))
        .def("get_node_types", &lupine::engine::EditorBridge::GetNodeTypes)
        .def("get_component_types", &lupine::engine::EditorBridge::GetComponentTypes)
        .def("get_prefab_types", &lupine::engine::EditorBridge::GetPrefabTypes)
        .def("load_prefab", &lupine::engine::EditorBridge::LoadPrefab,
             py::arg("prefab_path"))

        .def("get_archetype_definitions", [](lupine::engine::EditorBridge& self) {
            return self.GetArchetypeDefinitions().dump();
        })
        .def("get_archetype_definition", [](lupine::engine::EditorBridge& self,
                                            const std::string& className) {
            return self.GetArchetypeDefinition(className).dump();
        }, py::arg("class_name"))
        .def("get_archetype_effective_fields", [](lupine::engine::EditorBridge& self,
                                                  const std::string& className) {
            return self.GetArchetypeEffectiveFields(className).dump();
        }, py::arg("class_name"))
        .def("create_archetype_instance", &lupine::engine::EditorBridge::CreateArchetypeInstance,
             py::arg("class_name"), py::arg("path"))
        .def("load_archetype_instance", [](lupine::engine::EditorBridge& self,
                                           const std::string& path) {
            return self.LoadArchetypeInstance(path).dump();
        }, py::arg("path"))
        .def("save_archetype_instance", [](lupine::engine::EditorBridge& self,
                                           const std::string& path, const std::string& valuesJson) {
            return self.SaveArchetypeInstance(path, nlohmann::json::parse(valuesJson));
        }, py::arg("path"), py::arg("values_json"))
        .def("create_archetype_definition", [](lupine::engine::EditorBridge& self,
                                               const std::string& path, const std::string& schemaJson) {
            return self.CreateArchetypeDefinition(path, nlohmann::json::parse(schemaJson));
        }, py::arg("path"), py::arg("schema_json"))
        .def("load_archetype_definition_file", [](lupine::engine::EditorBridge& self,
                                                  const std::string& path) {
            return self.LoadArchetypeDefinitionFile(path).dump();
        }, py::arg("path"))
        .def("rescan_archetypes", &lupine::engine::EditorBridge::RescanArchetypes)

        .def("get_interface_definitions", [](lupine::engine::EditorBridge& self) {
            return self.GetInterfaceDefinitions().dump();
        })
        .def("get_interface_definition", [](lupine::engine::EditorBridge& self,
                                            const std::string& interfaceName) {
            return self.GetInterfaceDefinition(interfaceName).dump();
        }, py::arg("interface_name"))
        .def("create_interface_definition", [](lupine::engine::EditorBridge& self,
                                               const std::string& path, const std::string& schemaJson) {
            return self.CreateInterfaceDefinition(path, nlohmann::json::parse(schemaJson));
        }, py::arg("path"), py::arg("schema_json"))
        .def("load_interface_definition_file", [](lupine::engine::EditorBridge& self,
                                                  const std::string& path) {
            return self.LoadInterfaceDefinitionFile(path).dump();
        }, py::arg("path"))
        .def("rescan_interfaces", &lupine::engine::EditorBridge::RescanInterfaces)
        .def("get_node_interfaces", [](lupine::engine::EditorBridge& self,
                                       std::shared_ptr<lupine::engine::Node> node) {
            return self.GetNodeInterfaces(node).dump();
        }, py::arg("node"))
        .def("verify_node_interface", [](lupine::engine::EditorBridge& self,
                                         std::shared_ptr<lupine::engine::Node> node,
                                         const std::string& interfaceName) {
            return self.VerifyNodeInterface(node, interfaceName).dump();
        }, py::arg("node"), py::arg("interface_name"))
        .def("get_archetype_interfaces", [](lupine::engine::EditorBridge& self,
                                            const std::string& className) {
            return self.GetArchetypeInterfaces(className).dump();
        }, py::arg("class_name"))
        .def("get_interface_implementers", [](lupine::engine::EditorBridge& self,
                                              const std::string& interfaceName) {
            return self.GetInterfaceImplementers(interfaceName).dump();
        }, py::arg("interface_name"))
        .def("find_nodes_by_class", [](lupine::engine::EditorBridge& self,
                                       const std::string& className) {
            return self.FindNodesByClass(className).dump();
        }, py::arg("class_name"))
        .def("find_archetype_instances", [](lupine::engine::EditorBridge& self,
                                            const std::string& className) {
            return self.FindArchetypeInstances(className).dump();
        }, py::arg("class_name"))

        .def("reload_localization", &lupine::engine::EditorBridge::ReloadLocalization)
        .def("reload_theme", &lupine::engine::EditorBridge::ReloadTheme)
        .def("set_preview_locale", &lupine::engine::EditorBridge::SetPreviewLocale,
             py::arg("locale"))
        .def("get_localization_locale", &lupine::engine::EditorBridge::GetLocalizationLocale)
        .def("get_localization_locales", &lupine::engine::EditorBridge::GetLocalizationLocales)

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
        .def("move_node", &lupine::engine::EditorBridge::MoveNode,
             py::arg("node"), py::arg("new_parent"), py::arg("target_index") = -1,
             "Reparent and/or reorder a node to target_index (-1 = append); undoable")
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
        .def("call_component_method", [](lupine::engine::EditorBridge& self,
                                          std::shared_ptr<lupine::engine::Component> component,
                                          const std::string& method,
                                          const std::string& argsJson) {
            auto args = argsJson.empty() ? nlohmann::json::array() : nlohmann::json::parse(argsJson);
            return self.CallComponentMethod(component, method, args).dump();
        }, py::arg("component"), py::arg("method"), py::arg("args_json") = std::string("[]"),
           "Invoke a generic component control method (Component::CallMethod)")
        .def("preview_animation_clip", [](lupine::engine::EditorBridge& self,
                                           std::shared_ptr<lupine::engine::Node> node,
                                           const std::string& clipJson,
                                           float time) {
            auto clip = nlohmann::json::parse(clipJson);
            return self.PreviewAnimationClip(node, clip, time).dump();
        }, py::arg("node"), py::arg("clip_json"), py::arg("time"),
           "Apply an in-memory animation clip pose at a time to a node (preview)")
        .def("capture_animation_pose", [](lupine::engine::EditorBridge& self,
                                           std::shared_ptr<lupine::engine::Node> node,
                                           const std::string& targetsJson) {
            auto targets = nlohmann::json::parse(targetsJson);
            return self.CaptureAnimationPose(node, targets).dump();
        }, py::arg("node"), py::arg("targets_json"),
           "Capture current channel values for later non-destructive restore")
        .def("restore_animation_pose", [](lupine::engine::EditorBridge& self,
                                           std::shared_ptr<lupine::engine::Node> node,
                                           const std::string& capturedJson) {
            auto captured = nlohmann::json::parse(capturedJson);
            self.RestoreAnimationPose(node, captured);
        }, py::arg("node"), py::arg("captured_json"),
           "Restore previously captured channel values to a node")
        .def("get_node_signals", [](lupine::engine::EditorBridge& self,
                                    std::shared_ptr<lupine::engine::Node> node) {
            return self.GetNodeSignals(node).dump();
        }, py::arg("node"),
           "Return all signals on a node (built-in + per-component) as a JSON string")
        .def("get_node_connections", [](lupine::engine::EditorBridge& self,
                                        std::shared_ptr<lupine::engine::Node> node) {
            return self.GetNodeConnections(node).dump();
        }, py::arg("node"),
           "Return the node's declarative signal connections as a JSON string")
        .def("add_connection", &lupine::engine::EditorBridge::AddConnection,
             py::arg("source_node"), py::arg("source_component_uuid"), py::arg("signal"),
             py::arg("target_node"), py::arg("method"), py::arg("flags"),
             "Connect a node/component signal to a handler method on a target node")
        .def("remove_connection", &lupine::engine::EditorBridge::RemoveConnection,
             py::arg("source_node"), py::arg("source_component_uuid"), py::arg("signal"),
             py::arg("target_node"), py::arg("method"),
             "Remove a signal connection")
        .def("reload_script_component", &lupine::engine::EditorBridge::ReloadScriptComponent,
             py::arg("component"),
             "Reload a script component's script file and re-parse its export properties")
        .def("reload_asset", &lupine::engine::EditorBridge::ReloadAsset,
             py::arg("asset_path"),
             "Reload an asset from disk and invalidate all caches that use it. Returns number of affected components.")

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
        .def("set_undo_recording_enabled", &lupine::engine::EditorBridge::SetUndoRecordingEnabled,
             py::arg("enabled"),
             "Enable/disable undo command recording (and scene-dirty marking); used by modal preview tools driving a detached scene")
        .def("is_undo_recording_enabled", &lupine::engine::EditorBridge::IsUndoRecordingEnabled,
             "Check whether undo command recording is currently enabled")
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
             py::arg("keep_aspect") = false,
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
        .def("set_bus_muted", &lupine::engine::EditorBridge::SetBusMuted,
             py::arg("bus_name"), py::arg("muted"), "Mute/unmute an audio bus")
        .def("is_bus_muted", &lupine::engine::EditorBridge::IsBusMuted,
             py::arg("bus_name"), "Query whether an audio bus is muted")
        .def("set_bus_solo", &lupine::engine::EditorBridge::SetBusSolo,
             py::arg("bus_name"), py::arg("solo"), "Solo/unsolo an audio bus")
        .def("is_bus_solo", &lupine::engine::EditorBridge::IsBusSolo,
             py::arg("bus_name"), "Query whether an audio bus is soloed")
        .def("create_audio_bus", &lupine::engine::EditorBridge::CreateAudioBus,
             py::arg("bus_name"), py::arg("parent_bus"), "Create an audio bus")
        .def("destroy_audio_bus", &lupine::engine::EditorBridge::DestroyAudioBus,
             py::arg("bus_name"), "Destroy an audio bus")
        .def("add_bus_effect", &lupine::engine::EditorBridge::AddBusEffect,
             py::arg("bus_name"), py::arg("effect_type"),
             "Append a DSP effect to a bus; returns the new index or -1")
        .def("remove_bus_effect", &lupine::engine::EditorBridge::RemoveBusEffect,
             py::arg("bus_name"), py::arg("index"), "Remove a bus effect")
        .def("move_bus_effect", &lupine::engine::EditorBridge::MoveBusEffect,
             py::arg("bus_name"), py::arg("from_index"), py::arg("to_index"),
             "Reorder a bus effect")
        .def("clear_bus_effects", &lupine::engine::EditorBridge::ClearBusEffects,
             py::arg("bus_name"), "Remove all effects from a bus")
        .def("set_bus_effect_enabled", &lupine::engine::EditorBridge::SetBusEffectEnabled,
             py::arg("bus_name"), py::arg("index"), py::arg("enabled"),
             "Enable/disable a bus effect")
        .def("set_bus_effect_parameter", &lupine::engine::EditorBridge::SetBusEffectParameter,
             py::arg("bus_name"), py::arg("index"), py::arg("parameter"), py::arg("value"),
             "Set a bus effect parameter")
        .def("get_audio_buses_json", &lupine::engine::EditorBridge::GetAudioBusesJson,
             "Get a JSON snapshot of all buses and their effects")
        .def("get_audio_effect_types_json", &lupine::engine::EditorBridge::GetAudioEffectTypesJson,
             "Get the available effect type names as a JSON array")
        .def("get_audio_bus_levels_json", &lupine::engine::EditorBridge::GetAudioBusLevelsJson,
             "Get live VU levels for all buses as JSON {bus: [left, right]}")
        .def("serialize_audio_buses", &lupine::engine::EditorBridge::SerializeAudioBuses,
             "Serialize the mixer state to a JSON string (for persistence)")
        .def("deserialize_audio_buses", &lupine::engine::EditorBridge::DeserializeAudioBuses,
             py::arg("json"), "Restore the mixer state from a JSON string")

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
        .def("screen_to_world_on_plane", [](lupine::engine::EditorBridge& self,
                                             int viewId,
                                             float screenX,
                                             float screenY,
                                             float planeNormalX,
                                             float planeNormalY,
                                             float planeNormalZ,
                                             float planePointX,
                                             float planePointY,
                                             float planePointZ) {
            auto worldPos = self.ScreenToWorldOnPlane(viewId, screenX, screenY,
                                                       planeNormalX, planeNormalY, planeNormalZ,
                                                       planePointX, planePointY, planePointZ);
            py::dict result;
            result["x"] = worldPos.x;
            result["y"] = worldPos.y;
            result["z"] = worldPos.z;
            return result;
        }, py::arg("view_id"), py::arg("screen_x"), py::arg("screen_y"),
           py::arg("plane_normal_x"), py::arg("plane_normal_y"), py::arg("plane_normal_z"),
           py::arg("plane_point_x"), py::arg("plane_point_y"), py::arg("plane_point_z"),
           "Cast a ray from screen coordinates into 3D world and intersect with arbitrary plane")
        .def("get_camera_ray", [](lupine::engine::EditorBridge& self,
                                   int viewId,
                                   float screenX,
                                   float screenY) {
            lupine::math::Vec3 origin, direction;
            bool success = self.GetCameraRay(viewId, screenX, screenY, origin, direction);
            py::dict result;
            result["success"] = success;
            result["origin_x"] = origin.x;
            result["origin_y"] = origin.y;
            result["origin_z"] = origin.z;
            result["direction_x"] = direction.x;
            result["direction_y"] = direction.y;
            result["direction_z"] = direction.z;
            return result;
        }, py::arg("view_id"), py::arg("screen_x"), py::arg("screen_y"),
           "Get camera ray from screen coordinates")
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
        // Line2D point editing
        .def("get_line2d_points", &lupine::engine::EditorBridge::GetLine2DPoints,
             py::arg("component"),
             "Get all Line2D points with Bezier data as JSON string")
        .def("add_line2d_point", &lupine::engine::EditorBridge::AddLine2DPoint,
             py::arg("component"), py::arg("x"), py::arg("y"),
             "Add a point to Line2D")
        .def("add_line2d_point_with_bezier", &lupine::engine::EditorBridge::AddLine2DPointWithBezier,
             py::arg("component"), py::arg("x"), py::arg("y"),
             py::arg("ctrl_in_x"), py::arg("ctrl_in_y"),
             py::arg("ctrl_out_x"), py::arg("ctrl_out_y"),
             py::arg("use_bezier"),
             "Add a point with Bezier controls to Line2D")
        .def("update_line2d_point", &lupine::engine::EditorBridge::UpdateLine2DPoint,
             py::arg("component"), py::arg("index"), py::arg("x"), py::arg("y"),
             "Update a Line2D point position")
        .def("update_line2d_point_bezier", &lupine::engine::EditorBridge::UpdateLine2DPointBezier,
             py::arg("component"), py::arg("index"),
             py::arg("ctrl_in_x"), py::arg("ctrl_in_y"),
             py::arg("ctrl_out_x"), py::arg("ctrl_out_y"),
             py::arg("symmetric"),
             "Update a Line2D point's Bezier control handles")
        .def("set_line2d_point_use_bezier", &lupine::engine::EditorBridge::SetLine2DPointUseBezier,
             py::arg("component"), py::arg("index"), py::arg("use_bezier"),
             "Set whether a Line2D point uses Bezier curves")
        .def("remove_line2d_point", &lupine::engine::EditorBridge::RemoveLine2DPoint,
             py::arg("component"), py::arg("index"),
             "Remove a point from Line2D")
        .def("clear_line2d_points", &lupine::engine::EditorBridge::ClearLine2DPoints,
             py::arg("component"),
             "Clear all points from Line2D")
        .def("set_line2d_show_handles", &lupine::engine::EditorBridge::SetLine2DShowHandles,
             py::arg("component"), py::arg("show"),
             "Set whether to show Bezier handles in editor")
        .def("set_line2d_selected_point", &lupine::engine::EditorBridge::SetLine2DSelectedPoint,
             py::arg("component"), py::arg("index"),
             "Set the selected point index for handle display (-1 for none)")
        .def("hit_test_line2d_control_point", &lupine::engine::EditorBridge::HitTestLine2DControlPoint,
             py::arg("component"), py::arg("world_x"), py::arg("world_y"), py::arg("radius"),
             "Hit test for Line2D control points, returns control point ID or -1")
        .def("move_line2d_control_point", &lupine::engine::EditorBridge::MoveLine2DControlPoint,
             py::arg("component"), py::arg("control_point_id"), py::arg("world_x"), py::arg("world_y"),
             "Move a Line2D control point by its ID")
        // Curve2D point editing (also works for Path2D)
        .def("get_curve2d_points", &lupine::engine::EditorBridge::GetCurve2DPoints,
             py::arg("component"),
             "Get all Curve2D points with Bezier data as JSON string")
        .def("add_curve2d_point", &lupine::engine::EditorBridge::AddCurve2DPoint,
             py::arg("component"), py::arg("x"), py::arg("y"),
             "Add a point to Curve2D")
        .def("add_curve2d_point_with_bezier", &lupine::engine::EditorBridge::AddCurve2DPointWithBezier,
             py::arg("component"), py::arg("x"), py::arg("y"),
             py::arg("ctrl_in_x"), py::arg("ctrl_in_y"),
             py::arg("ctrl_out_x"), py::arg("ctrl_out_y"),
             py::arg("use_bezier"),
             "Add a point with Bezier controls to Curve2D")
        .def("update_curve2d_point", &lupine::engine::EditorBridge::UpdateCurve2DPoint,
             py::arg("component"), py::arg("index"), py::arg("x"), py::arg("y"),
             "Update a Curve2D point position")
        .def("update_curve2d_point_bezier", &lupine::engine::EditorBridge::UpdateCurve2DPointBezier,
             py::arg("component"), py::arg("index"),
             py::arg("ctrl_in_x"), py::arg("ctrl_in_y"),
             py::arg("ctrl_out_x"), py::arg("ctrl_out_y"),
             py::arg("symmetric"),
             "Update a Curve2D point's Bezier control handles")
        .def("set_curve2d_point_use_bezier", &lupine::engine::EditorBridge::SetCurve2DPointUseBezier,
             py::arg("component"), py::arg("index"), py::arg("use_bezier"),
             "Set whether a Curve2D point uses Bezier curves")
        .def("remove_curve2d_point", &lupine::engine::EditorBridge::RemoveCurve2DPoint,
             py::arg("component"), py::arg("index"),
             "Remove a point from Curve2D")
        .def("clear_curve2d_points", &lupine::engine::EditorBridge::ClearCurve2DPoints,
             py::arg("component"),
             "Clear all points from Curve2D")
        .def("set_curve2d_show_handles", &lupine::engine::EditorBridge::SetCurve2DShowHandles,
             py::arg("component"), py::arg("show"),
             "Set whether to show Bezier handles in editor")
        .def("set_curve2d_selected_point", &lupine::engine::EditorBridge::SetCurve2DSelectedPoint,
             py::arg("component"), py::arg("index"),
             "Set the selected point index for handle display (-1 for none)")
        .def("hit_test_curve2d_control_point", &lupine::engine::EditorBridge::HitTestCurve2DControlPoint,
             py::arg("component"), py::arg("world_x"), py::arg("world_y"), py::arg("radius"),
             "Hit test for Curve2D control points, returns control point ID or -1")
        .def("move_curve2d_control_point", &lupine::engine::EditorBridge::MoveCurve2DControlPoint,
             py::arg("component"), py::arg("control_point_id"), py::arg("world_x"), py::arg("world_y"),
             "Move a Curve2D control point by its ID")
        // Curve3D point editing (also works for Path3D)
        .def("get_curve3d_points", &lupine::engine::EditorBridge::GetCurve3DPoints,
             py::arg("component"),
             "Get all Curve3D points with Bezier + tilt data as JSON string")
        .def("add_curve3d_point", &lupine::engine::EditorBridge::AddCurve3DPoint,
             py::arg("component"), py::arg("x"), py::arg("y"), py::arg("z"),
             "Add a point to Curve3D (local space)")
        .def("update_curve3d_point", &lupine::engine::EditorBridge::UpdateCurve3DPoint,
             py::arg("component"), py::arg("index"), py::arg("x"), py::arg("y"), py::arg("z"),
             "Update a Curve3D point position (local space)")
        .def("update_curve3d_point_bezier", &lupine::engine::EditorBridge::UpdateCurve3DPointBezier,
             py::arg("component"), py::arg("index"),
             py::arg("ctrl_in_x"), py::arg("ctrl_in_y"), py::arg("ctrl_in_z"),
             py::arg("ctrl_out_x"), py::arg("ctrl_out_y"), py::arg("ctrl_out_z"),
             py::arg("symmetric"),
             "Update a Curve3D point's Bezier control handles")
        .def("set_curve3d_point_use_bezier", &lupine::engine::EditorBridge::SetCurve3DPointUseBezier,
             py::arg("component"), py::arg("index"), py::arg("use_bezier"),
             "Set whether a Curve3D point uses Bezier curves")
        .def("set_curve3d_point_tilt", &lupine::engine::EditorBridge::SetCurve3DPointTilt,
             py::arg("component"), py::arg("index"), py::arg("tilt"),
             "Set a Curve3D point's tilt (radians)")
        .def("remove_curve3d_point", &lupine::engine::EditorBridge::RemoveCurve3DPoint,
             py::arg("component"), py::arg("index"),
             "Remove a point from Curve3D")
        .def("clear_curve3d_points", &lupine::engine::EditorBridge::ClearCurve3DPoints,
             py::arg("component"),
             "Clear all points from Curve3D")
        .def("set_curve3d_show_handles", &lupine::engine::EditorBridge::SetCurve3DShowHandles,
             py::arg("component"), py::arg("show"),
             "Set whether to show Bezier handles in editor")
        .def("set_curve3d_selected_point", &lupine::engine::EditorBridge::SetCurve3DSelectedPoint,
             py::arg("component"), py::arg("index"),
             "Set the selected point index for handle display (-1 for none)")
        .def("hit_test_curve3d_control_point", &lupine::engine::EditorBridge::HitTestCurve3DControlPoint,
             py::arg("component"), py::arg("view_id"), py::arg("screen_x"), py::arg("screen_y"), py::arg("pixel_radius"),
             "Hit test Curve3D control points against a screen position, returns control point ID or -1")
        .def("move_curve3d_control_point", &lupine::engine::EditorBridge::MoveCurve3DControlPoint,
             py::arg("component"), py::arg("control_point_id"), py::arg("view_id"), py::arg("screen_x"), py::arg("screen_y"),
             "Move a Curve3D control point on the view plane by its ID")
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

        // TileMap 2.5D Builder
        .def("create_tilemap25d_builder", &lupine::engine::EditorBridge::CreateTileMap25DBuilder,
             "Create a TileMap25D builder instance")
        .def("destroy_tilemap25d_builder", &lupine::engine::EditorBridge::DestroyTileMap25DBuilder,
             py::arg("builder_id"),
             "Destroy a TileMap25D builder instance")
        .def("tilemap25d_add_face", &lupine::engine::EditorBridge::TileMap25DAddFace,
             py::arg("builder_id"),
             py::arg("v0x"), py::arg("v0y"), py::arg("v0z"),
             py::arg("v1x"), py::arg("v1y"), py::arg("v1z"),
             py::arg("v2x"), py::arg("v2y"), py::arg("v2z"),
             py::arg("v3x"), py::arg("v3y"), py::arg("v3z"),
             py::arg("uv0u"), py::arg("uv0v"),
             py::arg("uv1u"), py::arg("uv1v"),
             py::arg("uv2u"), py::arg("uv2v"),
             py::arg("uv3u"), py::arg("uv3v"),
             py::arg("tileset_index"), py::arg("tile_index"),
             py::arg("two_sided") = true,
             "Add a face to the TileMap25D")
        .def("tilemap25d_remove_face", &lupine::engine::EditorBridge::TileMap25DRemoveFace,
             py::arg("builder_id"),
             py::arg("face_id"),
             "Remove a face from the TileMap25D")
        .def("tilemap25d_clear", &lupine::engine::EditorBridge::TileMap25DClear,
             py::arg("builder_id"),
             "Clear all faces from the TileMap25D")
        .def("tilemap25d_get_face_count", &lupine::engine::EditorBridge::TileMap25DGetFaceCount,
             py::arg("builder_id"),
             "Get the number of faces in the TileMap25D")
        .def("tilemap25d_set_face_vertex", &lupine::engine::EditorBridge::TileMap25DSetFaceVertex,
             py::arg("builder_id"),
             py::arg("face_id"),
             py::arg("vertex_index"),
             py::arg("x"), py::arg("y"), py::arg("z"),
             "Set a vertex position for a face")
        .def("tilemap25d_render", &lupine::engine::EditorBridge::TileMap25DRender,
             py::arg("builder_id"),
             py::arg("view_id"),
             "Render the TileMap25D to a view")
        .def("tilemap25d_to_json", &lupine::engine::EditorBridge::TileMap25DToJSON,
             py::arg("builder_id"),
             "Save TileMap25D to JSON string")
        .def("tilemap25d_from_json", &lupine::engine::EditorBridge::TileMap25DFromJSON,
             py::arg("builder_id"),
             py::arg("json"),
             "Load TileMap25D from JSON string")
        .def("tilemap25d_export_obj", &lupine::engine::EditorBridge::TileMap25DExportOBJ,
             py::arg("builder_id"),
             py::arg("merge_vertices") = true,
             py::arg("use_texture_atlas") = true,
             "Export TileMap25D to OBJ format")
        .def("tilemap25d_export_mtl", &lupine::engine::EditorBridge::TileMap25DExportMTL,
             py::arg("builder_id"),
             py::arg("use_texture_atlas") = true,
             "Export TileMap25D material to MTL format")
        .def("tilemap25d_load_tileset_texture", &lupine::engine::EditorBridge::TileMap25DLoadTilesetTexture,
             py::arg("builder_id"),
             py::arg("tileset_index"),
             "Load a tileset texture for TileMap25D")
        .def("tilemap25d_set_texture", &lupine::engine::EditorBridge::TileMap25DSetTexture,
             py::arg("builder_id"),
             py::arg("texture_path"),
             "Set texture for TileMap25D from a file path")
        .def("tilemap25d_unregister", &lupine::engine::EditorBridge::TileMap25DUnregister,
             py::arg("builder_id"),
             py::arg("view_id"),
             "Unregister a TileMap25D builder from a render view")

        .def("debug_draw_line", &lupine::engine::EditorBridge::DebugDrawLine,
             py::arg("view_id"),
             py::arg("x1"), py::arg("y1"), py::arg("z1"),
             py::arg("x2"), py::arg("y2"), py::arg("z2"),
             py::arg("r"), py::arg("g"), py::arg("b"), py::arg("a"),
             py::arg("depth_test") = true,
             "Draw a debug line in the viewport")
        .def("debug_draw_aabb", &lupine::engine::EditorBridge::DebugDrawAABB,
             py::arg("view_id"),
             py::arg("min_x"), py::arg("min_y"), py::arg("min_z"),
             py::arg("max_x"), py::arg("max_y"), py::arg("max_z"),
             py::arg("r"), py::arg("g"), py::arg("b"), py::arg("a"),
             "Draw a debug bounding box (AABB) in the viewport");

    // AssetMeta struct for asset metadata
    py::class_<lupine::asset::AssetMeta>(m, "AssetMeta")
        .def(py::init<>())
        .def_readwrite("uuid", &lupine::asset::AssetMeta::uuid)
        .def_readwrite("path", &lupine::asset::AssetMeta::path)
        .def_readwrite("import_type", &lupine::asset::AssetMeta::importType);

    // AssetDatabase singleton for asset management
    // Use a custom holder type with a no-op deleter for singleton pattern
    py::class_<lupine::asset::AssetDatabase, std::unique_ptr<lupine::asset::AssetDatabase, py::nodelete>>(m, "AssetDatabase")
        .def_static("get_instance", []() -> lupine::asset::AssetDatabase& {
                 return lupine::asset::AssetDatabase::GetInstance();
             },
             py::return_value_policy::reference,
             "Get the AssetDatabase singleton instance")
        .def("initialize", &lupine::asset::AssetDatabase::Initialize,
             py::arg("project_root"),
             "Initialize the asset database with project root")
        .def("shutdown", &lupine::asset::AssetDatabase::Shutdown,
             "Shutdown the asset database")
        .def("is_initialized", &lupine::asset::AssetDatabase::IsInitialized,
             "Check if asset database is initialized")
        .def("import_asset", &lupine::asset::AssetDatabase::ImportAsset,
             py::arg("physical_path"),
             "Import an asset file (creates .meta file with UUID)")
        .def("scan_and_import_all", &lupine::asset::AssetDatabase::ScanAndImportAll,
             "Scan project directory and import all assets")
        .def("get_asset_by_path", [](lupine::asset::AssetDatabase& self, const std::string& path) -> py::object {
                 const auto* meta = self.GetAssetByPath(path);
                 if (meta) return py::cast(*meta);
                 return py::none();
             },
             py::arg("path"),
             "Get metadata for an asset by res:// path")
        .def("get_asset_by_uuid", [](lupine::asset::AssetDatabase& self, const lupine::core::UUID& uuid) -> py::object {
                 const auto* meta = self.GetAssetByUUID(uuid);
                 if (meta) return py::cast(*meta);
                 return py::none();
             },
             py::arg("uuid"),
             "Get metadata for an asset by UUID")
        .def("to_resource_path", &lupine::asset::AssetDatabase::ToResourcePath,
             py::arg("physical_path"),
             "Convert physical path to res:// path")
        .def("resolve_asset", &lupine::asset::AssetDatabase::ResolveAsset,
             py::arg("path_or_uuid"),
             "Resolve res:// path or UUID to physical path")
        .def("get_project_root", &lupine::asset::AssetDatabase::GetProjectRoot,
             "Get the project root directory")
        .def("remove_asset", &lupine::asset::AssetDatabase::RemoveAsset,
             py::arg("res_path"),
             "Remove an asset and its meta file from the database")
        .def("export_uuid_mappings", &lupine::asset::AssetDatabase::ExportUUIDMappings,
             "Export all UUID mappings as JSON for pack files")
        .def("needs_reimport", &lupine::asset::AssetDatabase::NeedsReimport,
             py::arg("path"),
             "Check if an asset needs reimporting (file modified since last import)");

    // ========================================================================
    // Godot Scene Import Bindings
    // ========================================================================

    // GodotValue::Type enum
    py::enum_<lupine::import::GodotValue::Type>(m, "GodotValueType")
        .value("Null", lupine::import::GodotValue::Type::Null)
        .value("Bool", lupine::import::GodotValue::Type::Bool)
        .value("Int", lupine::import::GodotValue::Type::Int)
        .value("Float", lupine::import::GodotValue::Type::Float)
        .value("String", lupine::import::GodotValue::Type::String)
        .value("Vector2", lupine::import::GodotValue::Type::Vector2)
        .value("Vector3", lupine::import::GodotValue::Type::Vector3)
        .value("Color", lupine::import::GodotValue::Type::Color)
        .value("Rect2", lupine::import::GodotValue::Type::Rect2)
        .value("Transform2D", lupine::import::GodotValue::Type::Transform2D)
        .value("Transform3D", lupine::import::GodotValue::Type::Transform3D)
        .value("Quat", lupine::import::GodotValue::Type::Quat)
        .value("Array", lupine::import::GodotValue::Type::Array)
        .value("Dictionary", lupine::import::GodotValue::Type::Dictionary)
        .value("ExtResource", lupine::import::GodotValue::Type::ExtResource)
        .value("SubResource", lupine::import::GodotValue::Type::SubResource)
        .value("NodePath", lupine::import::GodotValue::Type::NodePath)
        .export_values();

    // GodotValue struct
    py::class_<lupine::import::GodotValue>(m, "GodotValue")
        .def(py::init<>())
        .def_readwrite("type", &lupine::import::GodotValue::type)
        .def_readwrite("bool_value", &lupine::import::GodotValue::boolValue)
        .def_readwrite("int_value", &lupine::import::GodotValue::intValue)
        .def_readwrite("float_value", &lupine::import::GodotValue::floatValue)
        .def_readwrite("string_value", &lupine::import::GodotValue::stringValue)
        .def_readwrite("x", &lupine::import::GodotValue::x)
        .def_readwrite("y", &lupine::import::GodotValue::y)
        .def_readwrite("z", &lupine::import::GodotValue::z)
        .def_readwrite("w", &lupine::import::GodotValue::w)
        .def_readwrite("r", &lupine::import::GodotValue::r)
        .def_readwrite("g", &lupine::import::GodotValue::g)
        .def_readwrite("b", &lupine::import::GodotValue::b)
        .def_readwrite("a", &lupine::import::GodotValue::a)
        .def_readwrite("resource_id", &lupine::import::GodotValue::resourceId)
        .def_readwrite("array_value", &lupine::import::GodotValue::arrayValue)
        .def_readwrite("dict_value", &lupine::import::GodotValue::dictValue)
        .def("to_json", [](const lupine::import::GodotValue& self) {
            return self.ToJson().dump();
        }, "Convert to JSON string");

    // GodotExtResource struct
    py::class_<lupine::import::GodotExtResource>(m, "GodotExtResource")
        .def(py::init<>())
        .def_readwrite("id", &lupine::import::GodotExtResource::id)
        .def_readwrite("type", &lupine::import::GodotExtResource::type)
        .def_readwrite("path", &lupine::import::GodotExtResource::path)
        .def_readwrite("uid", &lupine::import::GodotExtResource::uid);

    // GodotSubResource struct
    py::class_<lupine::import::GodotSubResource>(m, "GodotSubResource")
        .def(py::init<>())
        .def_readwrite("id", &lupine::import::GodotSubResource::id)
        .def_readwrite("type", &lupine::import::GodotSubResource::type)
        .def_readwrite("properties", &lupine::import::GodotSubResource::properties);

    // GodotNode struct
    py::class_<lupine::import::GodotNode>(m, "GodotNode")
        .def(py::init<>())
        .def_readwrite("name", &lupine::import::GodotNode::name)
        .def_readwrite("type", &lupine::import::GodotNode::type)
        .def_readwrite("parent", &lupine::import::GodotNode::parent)
        .def_readwrite("owner", &lupine::import::GodotNode::owner)
        .def_readwrite("instance", &lupine::import::GodotNode::instance)
        .def_readwrite("properties", &lupine::import::GodotNode::properties)
        .def_readwrite("groups", &lupine::import::GodotNode::groups)
        .def_property_readonly("children", [](const lupine::import::GodotNode& self) {
            std::vector<lupine::import::GodotNode*> children;
            for (auto* child : self.children) {
                children.push_back(child);
            }
            return children;
        });

    // GodotConnection struct
    py::class_<lupine::import::GodotConnection>(m, "GodotConnection")
        .def(py::init<>())
        .def_readwrite("signal", &lupine::import::GodotConnection::signal)
        .def_readwrite("from_node", &lupine::import::GodotConnection::from)
        .def_readwrite("to_node", &lupine::import::GodotConnection::to)
        .def_readwrite("method", &lupine::import::GodotConnection::method)
        .def_readwrite("flags", &lupine::import::GodotConnection::flags);

    // GodotScene struct
    py::class_<lupine::import::GodotScene>(m, "GodotScene")
        .def(py::init<>())
        .def_readwrite("format_version", &lupine::import::GodotScene::formatVersion)
        .def_readwrite("load_steps", &lupine::import::GodotScene::loadSteps)
        .def_readwrite("uid", &lupine::import::GodotScene::uid)
        .def_readwrite("ext_resources", &lupine::import::GodotScene::extResources)
        .def_readwrite("sub_resources", &lupine::import::GodotScene::subResources)
        .def_readwrite("nodes", &lupine::import::GodotScene::nodes)
        .def_readwrite("connections", &lupine::import::GodotScene::connections)
        .def_readwrite("source_path", &lupine::import::GodotScene::sourcePath)
        .def_readwrite("source_directory", &lupine::import::GodotScene::sourceDirectory)
        .def_property_readonly("root_node", [](const lupine::import::GodotScene& self) -> lupine::import::GodotNode* {
            return self.rootNode;
        })
        .def("get_ext_resource", &lupine::import::GodotScene::GetExtResource,
             py::arg("id"), py::return_value_policy::reference)
        .def("get_sub_resource", &lupine::import::GodotScene::GetSubResource,
             py::arg("id"), py::return_value_policy::reference)
        .def("get_node", &lupine::import::GodotScene::GetNode,
             py::arg("path"), py::return_value_policy::reference)
        .def("build_node_hierarchy", &lupine::import::GodotScene::BuildNodeHierarchy);

    // GodotSceneParser class
    py::class_<lupine::import::GodotSceneParser>(m, "GodotSceneParser")
        .def(py::init<>())
        .def("parse_file", &lupine::import::GodotSceneParser::ParseFile,
             py::arg("filepath"),
             "Parse a .tscn file from path")
        .def("parse_string", &lupine::import::GodotSceneParser::ParseString,
             py::arg("content"), py::arg("source_path") = "",
             "Parse .tscn content from string")
        .def("get_last_error", &lupine::import::GodotSceneParser::GetLastError,
             "Get last parse error")
        .def("get_warnings", &lupine::import::GodotSceneParser::GetWarnings,
             "Get parse warnings");

    // ImportAsset::Type enum
    py::enum_<lupine::import::ImportAsset::Type>(m, "ImportAssetType")
        .value("Texture", lupine::import::ImportAsset::Type::Texture)
        .value("Audio", lupine::import::ImportAsset::Type::Audio)
        .value("Model", lupine::import::ImportAsset::Type::Model)
        .value("Font", lupine::import::ImportAsset::Type::Font)
        .value("Script", lupine::import::ImportAsset::Type::Script)
        .value("Scene", lupine::import::ImportAsset::Type::Scene)
        .value("Resource", lupine::import::ImportAsset::Type::Resource)
        .value("Unknown", lupine::import::ImportAsset::Type::Unknown);
    // NOTE: deliberately no .export_values() — it would export these value names
    // (Scene/Font/Script/Model/...) into the module and shadow same-named classes,
    // notably the Scene class (so `le.Scene(...)` became the ImportAssetType.Scene
    // enum value and raised "object is not callable"). Use le.ImportAssetType.X.

    // ImportAsset::Status enum
    py::enum_<lupine::import::ImportAsset::Status>(m, "ImportAssetStatus")
        .value("Resolved", lupine::import::ImportAsset::Status::Resolved)
        .value("Missing", lupine::import::ImportAsset::Status::Missing)
        .value("NeedsMapping", lupine::import::ImportAsset::Status::NeedsMapping)
        .value("Script", lupine::import::ImportAsset::Status::Script)
        .export_values();

    // ImportAsset struct
    py::class_<lupine::import::ImportAsset>(m, "ImportAsset")
        .def(py::init<>())
        .def_readwrite("godot_path", &lupine::import::ImportAsset::godotPath)
        .def_readwrite("resolved_path", &lupine::import::ImportAsset::resolvedPath)
        .def_readwrite("lupine_path", &lupine::import::ImportAsset::lupinePath)
        .def_readwrite("resource_id", &lupine::import::ImportAsset::resourceId)
        .def_readwrite("type", &lupine::import::ImportAsset::type)
        .def_readwrite("status", &lupine::import::ImportAsset::status)
        .def_readwrite("script_language", &lupine::import::ImportAsset::scriptLanguage)
        .def_readwrite("create_blank_script", &lupine::import::ImportAsset::createBlankScript)
        .def_readwrite("blank_script_language", &lupine::import::ImportAsset::blankScriptLanguage)
        .def_readwrite("should_import", &lupine::import::ImportAsset::shouldImport)
        .def_readwrite("is_selected", &lupine::import::ImportAsset::isSelected);

    // UnmappedNode struct
    py::class_<lupine::import::UnmappedNode>(m, "UnmappedNode")
        .def(py::init<>())
        .def_readwrite("godot_type", &lupine::import::UnmappedNode::godotType)
        .def_readwrite("node_name", &lupine::import::UnmappedNode::nodeName)
        .def_readwrite("node_path", &lupine::import::UnmappedNode::nodePath)
        .def_readwrite("suggested_lupine_type", &lupine::import::UnmappedNode::suggestedLupineType)
        .def_readwrite("notes", &lupine::import::UnmappedNode::notes)
        .def_readwrite("has_partial_support", &lupine::import::UnmappedNode::hasPartialSupport);

    // ImportResult struct
    py::class_<lupine::import::ImportResult>(m, "ImportResult")
        .def(py::init<>())
        .def_readwrite("success", &lupine::import::ImportResult::success)
        .def_readwrite("scene", &lupine::import::ImportResult::scene)
        .def_readwrite("assets", &lupine::import::ImportResult::assets)
        .def_readwrite("unmapped_nodes", &lupine::import::ImportResult::unmappedNodes)
        .def_readwrite("warnings", &lupine::import::ImportResult::warnings)
        .def_readwrite("errors", &lupine::import::ImportResult::errors)
        .def_readwrite("referenced_scenes", &lupine::import::ImportResult::referencedScenes)
        .def_readwrite("total_nodes", &lupine::import::ImportResult::totalNodes)
        .def_readwrite("converted_nodes", &lupine::import::ImportResult::convertedNodes)
        .def_readwrite("partial_nodes", &lupine::import::ImportResult::partialNodes)
        .def_readwrite("skipped_nodes", &lupine::import::ImportResult::skippedNodes)
        .def_readwrite("total_assets", &lupine::import::ImportResult::totalAssets)
        .def_readwrite("resolved_assets", &lupine::import::ImportResult::resolvedAssets)
        .def_readwrite("missing_assets", &lupine::import::ImportResult::missingAssets);

    // ImportConfig struct
    py::class_<lupine::import::ImportConfig>(m, "ImportConfig")
        .def(py::init<>())
        .def_readwrite("target_directory", &lupine::import::ImportConfig::targetDirectory)
        .def_readwrite("copy_assets", &lupine::import::ImportConfig::copyAssets)
        .def_readwrite("create_blank_scripts", &lupine::import::ImportConfig::createBlankScripts)
        .def_readwrite("default_script_language", &lupine::import::ImportConfig::defaultScriptLanguage)
        .def_readwrite("preserve_node_names", &lupine::import::ImportConfig::preserveNodeNames)
        .def_readwrite("convert_groups", &lupine::import::ImportConfig::convertGroups)
        .def_readwrite("import_connections", &lupine::import::ImportConfig::importConnections)
        .def_readwrite("godot_project_root", &lupine::import::ImportConfig::godotProjectRoot)
        .def_readwrite("lupine_project_root", &lupine::import::ImportConfig::lupineProjectRoot)
        .def_readwrite("asset_search_paths", &lupine::import::ImportConfig::assetSearchPaths);

    // NodeMapping struct
    py::class_<lupine::import::NodeMapping>(m, "NodeMapping")
        .def(py::init<>())
        .def_readonly("godot_type", &lupine::import::NodeMapping::godotType)
        .def_readonly("lupine_node_type", &lupine::import::NodeMapping::lupineNodeType)
        .def_readonly("components", &lupine::import::NodeMapping::components)
        .def_readonly("notes", &lupine::import::NodeMapping::notes);

    // GodotSceneImporter class
    py::class_<lupine::import::GodotSceneImporter>(m, "GodotSceneImporter")
        .def(py::init<>())
        .def("set_config", &lupine::import::GodotSceneImporter::SetConfig,
             py::arg("config"),
             "Set import configuration")
        .def("get_config", &lupine::import::GodotSceneImporter::GetConfig,
             py::return_value_policy::reference,
             "Get import configuration")
        .def("import_file", static_cast<lupine::import::ImportResult (lupine::import::GodotSceneImporter::*)(const std::string&)>(&lupine::import::GodotSceneImporter::Import),
             py::arg("filepath"),
             "Import a Godot scene file")
        .def("do_import", static_cast<lupine::import::ImportResult (lupine::import::GodotSceneImporter::*)(const std::string&)>(&lupine::import::GodotSceneImporter::Import),
             py::arg("filepath"),
             "Import a Godot scene file (alias for import_file)")
        .def("import_scene", static_cast<lupine::import::ImportResult (lupine::import::GodotSceneImporter::*)(const lupine::import::GodotScene&)>(&lupine::import::GodotSceneImporter::Import),
             py::arg("scene"),
             "Import from already parsed scene")
        .def("analyze", &lupine::import::GodotSceneImporter::Analyze,
             py::arg("filepath"),
             "Analyze a scene without importing (preview)")
        .def("resolve_assets", &lupine::import::GodotSceneImporter::ResolveAssets,
             py::arg("assets"),
             "Resolve asset paths and check availability")
        .def("copy_assets", &lupine::import::GodotSceneImporter::CopyAssets,
             py::arg("assets"),
             "Copy assets to project directory")
        .def("create_blank_scripts", &lupine::import::GodotSceneImporter::CreateBlankScripts,
             py::arg("assets"), py::arg("language"),
             "Create blank script files")
        .def("get_node_mappings", &lupine::import::GodotSceneImporter::GetNodeMappings,
             py::return_value_policy::reference,
             "Get available node mappings")
        .def("get_mapping", &lupine::import::GodotSceneImporter::GetMapping,
             py::arg("godot_type"), py::return_value_policy::reference,
             "Get mapping for a Godot node type")
        .def("has_mapping", &lupine::import::GodotSceneImporter::HasMapping,
             py::arg("godot_type"),
             "Check if a Godot type has a mapping")
        .def("get_supported_godot_types", &lupine::import::GodotSceneImporter::GetSupportedGodotTypes,
             "Get list of all supported Godot node types")
        .def("get_unsupported_godot_types", &lupine::import::GodotSceneImporter::GetUnsupportedGodotTypes,
             py::arg("scene"),
             "Get list of Godot types without mappings");

    // BlankScriptGenerator utility class
    py::class_<lupine::import::BlankScriptGenerator>(m, "BlankScriptGenerator")
        .def_static("generate_lua_script", &lupine::import::BlankScriptGenerator::GenerateLuaScript,
             py::arg("class_name"), py::arg("methods") = std::vector<std::string>{},
             "Generate a blank Lua script")
        .def_static("generate_python_script", &lupine::import::BlankScriptGenerator::GeneratePythonScript,
             py::arg("class_name"), py::arg("methods") = std::vector<std::string>{},
             "Generate a blank Python script")
        .def_static("generate_mruby_script", &lupine::import::BlankScriptGenerator::GenerateMrubyScript,
             py::arg("class_name"), py::arg("methods") = std::vector<std::string>{},
             "Generate a blank mruby script")
        .def_static("get_script_extension", &lupine::import::BlankScriptGenerator::GetScriptExtension,
             py::arg("language"),
             "Get file extension for a script language")
        .def_static("get_available_languages", &lupine::import::BlankScriptGenerator::GetAvailableLanguages,
             "Get available scripting languages");
}
