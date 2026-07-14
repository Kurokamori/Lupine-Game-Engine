#pragma once

#include "lupine/asset/Asset.hpp"
#include "lupine/animation/AnimationData.hpp"
#include <string>

namespace lupine {
namespace asset {

/**
 * AnimationClipAsset
 *
 * Loads and saves a single keyframe animation clip from a .animclip file (JSON).
 * Authored by the animation editor; sampled at runtime by AnimationPlayer and the
 * blend trees referenced by AnimationTree.
 */
class AnimationClipAsset : public Asset {
public:
    AnimationClipAsset();
    explicit AnimationClipAsset(const core::UUID& uuid);
    ~AnimationClipAsset() override;

    AssetType GetType() const override { return AssetType::KeyframeAnimation; }

    bool LoadFromFile(const std::string& filepath);
    bool SaveToFile(const std::string& filepath);

    const animation::AnimationClip& GetClip() const { return m_Clip; }
    animation::AnimationClip& GetClip() { return m_Clip; }
    void SetClip(const animation::AnimationClip& clip) { m_Clip = clip; }

private:
    animation::AnimationClip m_Clip;
};

} // namespace asset
} // namespace lupine
