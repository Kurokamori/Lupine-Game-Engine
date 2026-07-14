#pragma once

#include "lupine/asset/Asset.hpp"
#include "lupine/animation/AnimationData.hpp"
#include <string>

namespace lupine {
namespace asset {

/**
 * AnimationGraphAsset
 *
 * Loads and saves an animation controller graph from a .animgraph file (JSON):
 * parameters, layers, per-layer state machines and blend trees. Authored by the
 * blend-tree editor; evaluated at runtime by AnimationTree.
 */
class AnimationGraphAsset : public Asset {
public:
    AnimationGraphAsset();
    explicit AnimationGraphAsset(const core::UUID& uuid);
    ~AnimationGraphAsset() override;

    AssetType GetType() const override { return AssetType::AnimationGraph; }

    bool LoadFromFile(const std::string& filepath);
    bool SaveToFile(const std::string& filepath);

    const animation::AnimationGraph& GetGraph() const { return m_Graph; }
    animation::AnimationGraph& GetGraph() { return m_Graph; }
    void SetGraph(const animation::AnimationGraph& graph) { m_Graph = graph; }

private:
    animation::AnimationGraph m_Graph;
};

} // namespace asset
} // namespace lupine
