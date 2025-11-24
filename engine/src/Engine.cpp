#include "lupine/engine/Engine.hpp"
#include "lupine/components/Components.hpp"

namespace lupine {
namespace engine {

using core::Node;
using core::Node2D;
using core::Node3D;
using core::Camera2D;
using core::Camera3D;
using core::CameraUI;
using core::Component;
using core::PythonScriptComponent;
using core::LuaScriptComponent;
using core::Scene;
using core::ProjectSettings;

void InitializeEngine() {

    core::InitializeCore();

    core::RegisterBuiltInTypes();

    components::InitializeComponents();

}

void ShutdownEngine() {

    components::ShutdownComponents();

    core::ShutdownCore();

}

}
}
