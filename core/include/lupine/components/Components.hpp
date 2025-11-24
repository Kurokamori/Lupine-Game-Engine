#pragma once

/**
 * Lupine Engine - Components Module
 *
 * This module provides built-in component types for the ECS system.
 * Components can be attached to nodes to add functionality.
 */

#include "Sprite2D.hpp"
#include "Sprite3D.hpp"
#include "PrimitiveMesh3D.hpp"
#include "StaticMesh3D.hpp"
#include "SkeletalMesh3D.hpp"
#include "Label.hpp"
#include "DirectionalLight3D.hpp"
#include "OmniLight3D.hpp"
#include "SpotLight3D.hpp"
#include "ColorRect.hpp"
#include "Image2D.hpp"
#include "Timer.hpp"
#include "Panel.hpp"
#include "Button.hpp"
#include "Shape2D.hpp"
#include "Line2D.hpp"
#include "WorldEnvironment.hpp"
#include "AudioPlayer.hpp"
#include "AudioListener.hpp"
#include "RigidBody2DComponent.hpp"
#include "StaticBody2DComponent.hpp"
#include "KinematicBody2DComponent.hpp"
#include "AreaTrigger2DComponent.hpp"
#include "CollisionBody2DComponent.hpp"
#include "RigidBody3DComponent.hpp"
#include "StaticBody3DComponent.hpp"
#include "KinematicBody3DComponent.hpp"
#include "AreaTrigger3DComponent.hpp"
#include "CollisionMesh3DComponent.hpp"

namespace lupine {
namespace components {

/**
 * Initialize the components module
 * Registers all built-in component types
 */
void InitializeComponents();

/**
 * Shutdown the components module
 */
void ShutdownComponents();

} // namespace components
} // namespace lupine

