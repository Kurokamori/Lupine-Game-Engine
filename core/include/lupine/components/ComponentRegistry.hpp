// Header to easily include all components into any file. Used by core, components, and EditorBridge

// 2D Components
#include "lupine/components/Sprite2D.hpp"
#include "lupine/components/AnimatedSprite2D.hpp"
#include "lupine/components/TileMap2D.hpp"
#include "lupine/components/YSort.hpp"
#include "lupine/components/ParallaxBackground.hpp"
#include "lupine/components/ParallaxLayer.hpp"
#include "lupine/components/Shape2D.hpp"
#include "lupine/components/Line2D.hpp"
#include "lupine/components/Light2D.hpp"
#include "lupine/components/LightOccluder2D.hpp"
#include "lupine/components/VectorGraphic2D.hpp"
#include "lupine/components/Particles2D.hpp"
#include "lupine/components/GifPlayer.hpp"
#include "lupine/components/VideoPlayer.hpp"

// 3D Components
#include "lupine/components/Sprite3D.hpp"
#include "lupine/components/Particles3D.hpp"
#include "lupine/components/AnimatedSprite3D.hpp"
#include "lupine/components/PrimitiveMesh3D.hpp"
#include "lupine/components/StaticMesh3D.hpp"
#include "lupine/components/SkeletalMesh3D.hpp"

// ...3D Lighting Components
#include "lupine/components/DirectionalLight3D.hpp"
#include "lupine/components/OmniLight3D.hpp"
#include "lupine/components/SpotLight3D.hpp"
#include "lupine/components/WorldEnvironment.hpp"

// ...3D UI Components
#include "lupine/components/Label3D.hpp"
#include "lupine/components/Button3D.hpp"
#include "lupine/components/ProgressBar3D.hpp"
#include "lupine/components/Panel3D.hpp"

// ...3D Utility Components
#include "lupine/components/MultiMeshGeneric.hpp"
#include "lupine/components/ScatterMultiMesh.hpp"
#include "lupine/components/CollisionScatterMultiMesh.hpp"
#include "lupine/components/NodeScatter.hpp"

// UI Components
#include "lupine/components/Label.hpp"
#include "lupine/components/ColorRect.hpp"
#include "lupine/components/Image2D.hpp"
#include "lupine/components/Panel.hpp"
#include "lupine/components/NineSlicePanel.hpp"
#include "lupine/components/Button.hpp"
#include "lupine/components/TextureButton.hpp"
#include "lupine/components/ToggleButton.hpp"
#include "lupine/components/TextureToggleButton.hpp"
#include "lupine/components/ProgressBar.hpp"
#include "lupine/components/RadioButton.hpp"
#include "lupine/components/RadioList.hpp"
#include "lupine/components/Slider.hpp"
#include "lupine/components/LineEdit.hpp"
#include "lupine/components/SpinBox.hpp"
#include "lupine/components/TextEdit.hpp"
#include "lupine/components/ItemList.hpp"
#include "lupine/components/Dropdown.hpp"
#include "lupine/components/PopupMenu.hpp"
#include "lupine/components/RichTextLabel.hpp"
#include "lupine/components/Tree.hpp"

// ...Containers
#include "lupine/components/Container.hpp"
#include "lupine/components/PaddingContainer.hpp"
#include "lupine/components/CenterContainer.hpp"
#include "lupine/components/HorizontalContainer.hpp"
#include "lupine/components/VerticalContainer.hpp"
#include "lupine/components/GridContainer.hpp"
#include "lupine/components/DockContainer.hpp"
#include "lupine/components/Stack.hpp"
#include "lupine/components/Wrap.hpp"
#include "lupine/components/SplitContainer.hpp"
#include "lupine/components/AspectRatioContainer.hpp"
#include "lupine/components/Spacer.hpp"
#include "lupine/components/LayoutSlot.hpp"
#include "lupine/components/ScrollContainer.hpp"
#include "lupine/components/TabContainer.hpp"

// Physics Components

// ...2D Physics Components
#include "lupine/components/RigidBody2DComponent.hpp"
#include "lupine/components/StaticBody2DComponent.hpp"
#include "lupine/components/KinematicBody2DComponent.hpp"
#include "lupine/components/AreaTrigger2DComponent.hpp"
#include "lupine/components/CollisionBody2DComponent.hpp"
#include "lupine/components/CharacterController2D.hpp"
#include "lupine/components/RayCast2D.hpp"
#include "lupine/components/ShapeCast2D.hpp"

// ...3D Physics Components
#include "lupine/components/RigidBody3DComponent.hpp"
#include "lupine/components/StaticBody3DComponent.hpp"
#include "lupine/components/KinematicBody3DComponent.hpp"
#include "lupine/components/AreaTrigger3DComponent.hpp"
#include "lupine/components/CollisionMesh3DComponent.hpp"
#include "lupine/components/CharacterController3D.hpp"
#include "lupine/components/RayCast3D.hpp"
#include "lupine/components/ShapeCast3D.hpp"

// Navigation Components
#include "lupine/components/NavigationRegion2D.hpp"
#include "lupine/components/NavigationAgent2D.hpp"
#include "lupine/components/NavigationObstacle2D.hpp"
#include "lupine/components/NavigationRegion3D.hpp"
#include "lupine/components/NavigationAgent3D.hpp"
#include "lupine/components/NavigationObstacle3D.hpp"

// Networking Components
#include "lupine/components/NetworkObject.hpp"
#include "lupine/components/NetworkSynchronizer.hpp"
#include "lupine/components/NetworkTransform2D.hpp"
#include "lupine/components/NetworkTransform3D.hpp"
#include "lupine/components/NetworkSpawner.hpp"
#include "lupine/components/NetworkController.hpp"
#include "lupine/components/NetworkAnimator.hpp"
#include "lupine/components/NetworkRigidBody2D.hpp"
#include "lupine/components/NetworkRigidBody3D.hpp"

// Utility and Miscelaneous Components
#include "lupine/components/Timer.hpp"
#include "lupine/components/AnimationPlayer.hpp"
#include "lupine/components/AnimationTree.hpp"
#include "lupine/components/AudioPlayer.hpp"
#include "lupine/components/AudioListener.hpp"
#include "lupine/components/SubViewport.hpp"
#include "lupine/components/CameraEffect.hpp"
#include "lupine/components/CameraEffects.hpp"
#include "lupine/components/Empty2D.hpp"
#include "lupine/components/Empty3D.hpp"

// Test Components
#include "lupine/components/TestPlatform.hpp"
#include "lupine/components/Test3D.hpp"
#include "lupine/components/TestTopdown.hpp"

