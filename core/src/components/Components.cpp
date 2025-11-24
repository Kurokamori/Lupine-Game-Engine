#include "lupine/components/Components.hpp"
#include "lupine/components/Sprite2D.hpp"
#include "lupine/components/Sprite3D.hpp"
#include "lupine/components/AnimatedSprite2D.hpp"
#include "lupine/components/AnimatedSprite3D.hpp"
#include "lupine/components/PrimitiveMesh3D.hpp"
#include "lupine/components/StaticMesh3D.hpp"
#include "lupine/components/SkeletalMesh3D.hpp"
#include "lupine/components/MultiMeshGeneric.hpp"
#include "lupine/components/Label.hpp"
#include "lupine/components/Label3D.hpp"
#include "lupine/components/DirectionalLight3D.hpp"
#include "lupine/components/OmniLight3D.hpp"
#include "lupine/components/SpotLight3D.hpp"
#include "lupine/components/ColorRect.hpp"
#include "lupine/components/Image2D.hpp"
#include "lupine/components/Timer.hpp"
#include "lupine/components/YSort.hpp"
#include "lupine/components/Panel.hpp"
#include "lupine/components/Panel3D.hpp"
#include "lupine/components/Container.hpp"
#include "lupine/components/Button.hpp"
#include "lupine/components/Button3D.hpp"
#include "lupine/components/ProgressBar.hpp"
#include "lupine/components/ProgressBar3D.hpp"
#include "lupine/components/Shape2D.hpp"
#include "lupine/components/Line2D.hpp"
#include "lupine/components/WorldEnvironment.hpp"
#include "lupine/components/AudioPlayer.hpp"
#include "lupine/components/AudioListener.hpp"
#include "lupine/components/RigidBody2DComponent.hpp"
#include "lupine/components/StaticBody2DComponent.hpp"
#include "lupine/components/KinematicBody2DComponent.hpp"
#include "lupine/components/AreaTrigger2DComponent.hpp"
#include "lupine/components/CollisionBody2DComponent.hpp"
#include "lupine/components/RigidBody3DComponent.hpp"
#include "lupine/components/StaticBody3DComponent.hpp"
#include "lupine/components/KinematicBody3DComponent.hpp"
#include "lupine/components/AreaTrigger3DComponent.hpp"
#include "lupine/components/CollisionMesh3DComponent.hpp"
#include "lupine/components/CharacterController2D.hpp"
#include "lupine/components/CharacterController3D.hpp"
#include "lupine/components/TestPlatform.hpp"
#include "lupine/components/Test3D.hpp"
#include "lupine/components/TestTopdown.hpp"
#include "lupine/audio/AudioManager.hpp"
#include "lupine/core/Serialization.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace components {

// Register component types using the macro
REGISTER_COMPONENT_TYPE(Sprite2D)
REGISTER_COMPONENT_TYPE(Sprite3D)
REGISTER_COMPONENT_TYPE(AnimatedSprite2D)
REGISTER_COMPONENT_TYPE(AnimatedSprite3D)
REGISTER_COMPONENT_TYPE(PrimitiveMesh3D)
REGISTER_COMPONENT_TYPE(StaticMesh3D)
REGISTER_COMPONENT_TYPE(SkeletalMesh3D)
REGISTER_COMPONENT_TYPE(MultiMeshGeneric)
REGISTER_COMPONENT_TYPE(Label)
REGISTER_COMPONENT_TYPE(Label3D)
REGISTER_COMPONENT_TYPE(DirectionalLight3D)
REGISTER_COMPONENT_TYPE(OmniLight3D)
REGISTER_COMPONENT_TYPE(SpotLight3D)
REGISTER_COMPONENT_TYPE(ColorRect)
REGISTER_COMPONENT_TYPE(Image2D)
REGISTER_COMPONENT_TYPE(Timer)
REGISTER_COMPONENT_TYPE(YSort)
REGISTER_COMPONENT_TYPE(Panel)
REGISTER_COMPONENT_TYPE(Panel3D)
REGISTER_COMPONENT_TYPE(Container)
REGISTER_COMPONENT_TYPE(Button)
REGISTER_COMPONENT_TYPE(Button3D)
REGISTER_COMPONENT_TYPE(ProgressBar)
REGISTER_COMPONENT_TYPE(ProgressBar3D)
REGISTER_COMPONENT_TYPE(Shape2D)
REGISTER_COMPONENT_TYPE(Line2D)
REGISTER_COMPONENT_TYPE(WorldEnvironment)
REGISTER_COMPONENT_TYPE(AudioPlayer)
REGISTER_COMPONENT_TYPE(AudioListener)
REGISTER_COMPONENT_TYPE(RigidBody2DComponent)
REGISTER_COMPONENT_TYPE(StaticBody2DComponent)
REGISTER_COMPONENT_TYPE(KinematicBody2DComponent)
REGISTER_COMPONENT_TYPE(AreaTrigger2DComponent)
REGISTER_COMPONENT_TYPE(CollisionBody2DComponent)
REGISTER_COMPONENT_TYPE(RigidBody3DComponent)
REGISTER_COMPONENT_TYPE(StaticBody3DComponent)
REGISTER_COMPONENT_TYPE(KinematicBody3DComponent)
REGISTER_COMPONENT_TYPE(AreaTrigger3DComponent)
REGISTER_COMPONENT_TYPE(CollisionMesh3DComponent)
REGISTER_COMPONENT_TYPE(CharacterController2D)
REGISTER_COMPONENT_TYPE(CharacterController3D)
REGISTER_COMPONENT_TYPE(TestPlatform)
REGISTER_COMPONENT_TYPE(Test3D)
REGISTER_COMPONENT_TYPE(TestTopdown)

void InitializeComponents() {

    // Component types are automatically registered via static initializers
    // from the REGISTER_COMPONENT_TYPE macros above

    // Initialize audio manager
    audio::AudioManager::GetInstance().Initialize();

}

void ShutdownComponents() {

    // Shutdown audio manager
    audio::AudioManager::GetInstance().Shutdown();
}

} // namespace components
} // namespace lupine

