#pragma once

#include "lupine/asset/Asset.hpp"
#include "lupine/asset/ImageAsset.hpp"
#include "lupine/math/Math.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <map>

namespace lupine {
namespace asset {

/**
 * Loop behavior for sprite animations
 */
enum class AnimationLoopMode {
    None,       // Play once and stop
    Linear,     // Loop from start to end
    PingPong    // Play forward then backward
};

/**
 * Frame data for sprite animation
 * Contains UV coordinates and source image reference
 */
struct SpriteFrame {
    math::Vec4 uvRect;              // UV rectangle (x, y, width, height) in normalized 0-1 space
    std::string sourceImagePath;    // Path to source image
    int spriteIndex;                // Index in sprite sheet (if applicable)
};

/**
 * Animation clip data
 */
struct SpriteAnimationClip {
    std::string name;
    std::vector<SpriteFrame> frames;
    AnimationLoopMode loopMode;
    float speed;  // Frames per second
    
    SpriteAnimationClip() 
        : loopMode(AnimationLoopMode::Linear)
        , speed(10.0f) 
    {}
};

/**
 * Sprite configuration data
 * Describes how sprites were sliced from source images
 */
struct SpriteConfig {
    std::string type;  // "images", "sheet_grid", "sheet_size", "aseprite"
    
    // For sheet_grid
    int cols = 0;
    int rows = 0;
    
    // For sheet_size
    int width = 0;
    int height = 0;
    
    // For images
    int displayCols = 8;
    
    // For aseprite
    std::string jsonPath;
    std::string imagePath;
};

/**
 * SpriteAnimationAsset
 * 
 * Loads and manages sprite animation data from .spriteanimation files.
 * These files are created by the sprite animator editor tool and contain:
 * - Named animation clips with frame lists
 * - Loop mode and speed per animation
 * - Sprite source configuration (sprite sheet or individual images)
 */
class SpriteAnimationAsset : public Asset {
public:
    SpriteAnimationAsset();
    explicit SpriteAnimationAsset(const core::UUID& uuid);
    virtual ~SpriteAnimationAsset();
    
    AssetType GetType() const override { return AssetType::SpriteAnimation; }
    
    /**
     * Load sprite animation from .spriteanimation file
     */
    bool LoadFromFile(const std::string& filepath);
    
    /**
     * Get animation clip by name
     */
    const SpriteAnimationClip* GetAnimation(const std::string& name) const;
    
    /**
     * Get all animation names
     */
    std::vector<std::string> GetAnimationNames() const;
    
    /**
     * Get sprite configuration
     */
    const SpriteConfig& GetSpriteConfig() const { return m_SpriteConfig; }
    
    /**
     * Get source image paths
     */
    const std::vector<std::string>& GetImagePaths() const { return m_ImagePaths; }
    
    /**
     * Get loaded image assets (lazy loaded)
     */
    const std::vector<AssetRef<ImageAsset>>& GetImageAssets();
    
    /**
     * Check if animation exists
     */
    bool HasAnimation(const std::string& name) const;
    
private:
    std::map<std::string, SpriteAnimationClip> m_Animations;
    std::vector<std::string> m_ImagePaths;
    SpriteConfig m_SpriteConfig;

    // Lazy-loaded image assets
    std::vector<AssetRef<ImageAsset>> m_ImageAssets;
    bool m_ImagesLoaded;

    // Helper methods
    /**
     * Parse sprite config from JSON
     */
    void ParseSpriteConfig(const nlohmann::json& json);

    /**
     * Load image assets if not already loaded
     */
    void LoadImageAssets();
    
    /**
     * Calculate UV rect for a sprite based on config
     */
    math::Vec4 CalculateUVRect(int spriteIndex, const AssetRef<ImageAsset>& image) const;
};

} // namespace asset
} // namespace lupine

