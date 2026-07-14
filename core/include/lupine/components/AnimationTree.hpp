#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/animation/AnimationData.hpp"
#include "lupine/animation/AnimationStateMachine.hpp"
#include "lupine/animation/AnimationPose.hpp"
#include "lupine/animation/ClipSampler.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <unordered_map>

namespace lupine {
namespace components {

class AnimationPlayer;

/**
 * AnimationTree
 *
 * Drives a parameter-controlled animation graph (state machines + blend trees,
 * Unity Animator parity). References an .animgraph resource and a sibling
 * AnimationPlayer whose clips it samples. Each frame it ticks every layer's state
 * machine, evaluates the active state's blend tree into a weighted pose, composites
 * the layers (Override / Additive with masks), and writes the result to the scene.
 *
 * Parameters (float/int/bool/trigger) are set from scripts/editor and persist with
 * the component. While active, the linked AnimationPlayer yields pose control.
 */
class AnimationTree : public core::Component {
public:
    AnimationTree();
    explicit AnimationTree(const std::string& name);
    ~AnimationTree() override;

    std::string GetTypeName() const override { return "AnimationTree"; }
    void DefineProperties() override;
    void DefineSignals() override;

    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& json) override;

    void OnAwake() override;
    void OnUpdate(float deltaTime) override;
    void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;

    nlohmann::json CallMethod(const std::string& method, const nlohmann::json& args) override;

    // ===== Parameters =====
    void SetFloat(const std::string& name, float value);
    void SetInt(const std::string& name, int value);
    void SetBool(const std::string& name, bool value);
    void SetTrigger(const std::string& name);
    float GetFloat(const std::string& name);
    int GetInt(const std::string& name);
    bool GetBool(const std::string& name);

    // ===== State machine control =====
    void Travel(const std::string& state, int layer = 0, float duration = 0.2f);
    std::string GetCurrentState(int layer = 0);

    // ===== Properties =====
    std::string GetGraphPath() const;
    void SetGraphPath(const std::string& path);
    std::string GetAnimationPlayerPath() const;
    void SetAnimationPlayerPath(const std::string& path);
    bool GetActive() const;
    void SetActive(bool active);
    std::string GetRootNodePath() const;
    void SetRootNodePath(const std::string& path);

    // Re-load the graph and rebuild layer runtimes (editor live edit).
    void ReloadGraph();

    const animation::AnimationGraph& GetGraph() const { return m_Graph; }
    animation::AnimationParameters& GetParameters() { return m_Params; }

private:
    void EnsureInit();
    void LoadGraph();
    AnimationPlayer* ResolvePlayer();
    const animation::AnimationClip* ResolveClip(const std::string& clip, const std::string& path);
    core::Node* ResolveRoot() const;
    nlohmann::json ReadCurrent(const std::string& node, const std::string& nodeUuid,
                               const std::string& channel,
                               animation::TrackValueType type) const;
    void CompositeLayer(animation::TargetValueMap& result, const animation::TargetValueMap& layerMap,
                        animation::LayerRuntime& runtime, const animation::AnimationLayer& layerData,
                        const animation::BlendContext& ctx, float weight) const;
    void FireMethodCalls(const std::vector<animation::MethodCall>& calls) const;

    animation::AnimationGraph m_Graph;
    animation::AnimationParameters m_Params;
    std::vector<animation::LayerRuntime> m_Layers;
    std::unordered_map<std::string, animation::AnimationClip> m_PathClips;

    AnimationPlayer* m_Player = nullptr;
    bool m_PlayerResolved = false;
    bool m_Initialized = false;
};

} // namespace components
} // namespace lupine
