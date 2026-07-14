#include "lupine/asset/SpriteAnimationAsset.hpp"
#include "lupine/platform/FileSystem.hpp"
#include "lupine/platform/PackFile.hpp"
#include "lupine/logger/Logger.hpp"
#include <nlohmann/json.hpp>

namespace lupine {
namespace asset {

using namespace core;
using namespace math;

SpriteAnimationAsset::SpriteAnimationAsset()
    : Asset()
    , m_ImagesLoaded(false)
{
}

SpriteAnimationAsset::SpriteAnimationAsset(const UUID& uuid)
    : Asset(uuid)
    , m_ImagesLoaded(false)
{
}

SpriteAnimationAsset::~SpriteAnimationAsset() {
}

bool SpriteAnimationAsset::LoadFromFile(const std::string& filepath) {

    SetPath(filepath);

    std::string fileContents;

    // Check if running from pack file first
    auto& packFS = platform::PackFileSystem::Instance();
    if (packFS.isPackMode() && packFS.exists(filepath)) {
        fileContents = packFS.readFileAsString(filepath);
        if (fileContents.empty()) {
            
            return false;
        }
    } else {
        // Resolve res://, user://, temp:// virtual paths to a physical path. This asset is
        // loaded by file path directly (not via the asset database), so a raw filesystem read
        // would not understand the res:// mount and would fail, leaving AnimatedSprite2D with
        // no frames (a blank sprite). ResolveAssetPath routes res:// through the AssetDatabase,
        // which knows the project root; the VFS mounts res:// to the engine's own resources
        // directory, so resolving through it lands on the wrong root entirely.
        std::string realPath = ResolveAssetPath(filepath);

        platform::FileResult<std::string> result = platform::FileSystem::ReadFile(realPath);
        if (!result.success) {
            LOG_ERROR(LogCategory::Asset, "SpriteAnimationAsset: failed to read {} (resolved: {}): {}",
                      filepath, realPath, result.error);
            return false;
        }
        fileContents = std::move(result.data);
    }

    try {

        nlohmann::json json = nlohmann::json::parse(fileContents);

        if (json.contains("image_paths")) {
            m_ImagePaths = json["image_paths"].get<std::vector<std::string>>();
        }

        if (json.contains("sprite_config")) {
            ParseSpriteConfig(json["sprite_config"]);
        }

        if (json.contains("animations")) {
            for (const auto& animJson : json["animations"]) {
                SpriteAnimationClip clip;
                clip.name = animJson["name"].get<std::string>();
                clip.speed = animJson.value("speed", 10.0f);

                std::string loopStr = animJson.value("loop_behavior", "linear");
                if (loopStr == "none") {
                    clip.loopMode = AnimationLoopMode::None;
                } else if (loopStr == "pingpong") {
                    clip.loopMode = AnimationLoopMode::PingPong;
                } else {
                    clip.loopMode = AnimationLoopMode::Linear;
                }

                std::vector<int> frameIndices = animJson["frames"].get<std::vector<int>>();

                for (int frameIndex : frameIndices) {
                    SpriteFrame frame;
                    frame.spriteIndex = frameIndex;
                    frame.uvRect = Vec4(0, 0, 1, 1);
                    if (!m_ImagePaths.empty()) {
                        frame.sourceImagePath = m_ImagePaths[0];
                    }
                    clip.frames.push_back(frame);
                }

                m_Animations[clip.name] = clip;
            }
        }

        SetLoaded(true);

        return true;

    } catch (const std::exception& e) {
        LOG_ERROR(LogCategory::Asset, "SpriteAnimationAsset: failed to load '{}': {}", filepath, e.what());
        return false;
    }
}

const SpriteAnimationClip* SpriteAnimationAsset::GetAnimation(const std::string& name) const {
    auto it = m_Animations.find(name);
    if (it != m_Animations.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<std::string> SpriteAnimationAsset::GetAnimationNames() const {
    std::vector<std::string> names;
    names.reserve(m_Animations.size());
    for (const auto& pair : m_Animations) {
        names.push_back(pair.first);
    }
    return names;
}

bool SpriteAnimationAsset::HasAnimation(const std::string& name) const {
    return m_Animations.find(name) != m_Animations.end();
}

const std::vector<AssetRef<ImageAsset>>& SpriteAnimationAsset::GetImageAssets() {
    if (!m_ImagesLoaded) {
        LoadImageAssets();
    }
    return m_ImageAssets;
}

void SpriteAnimationAsset::LoadImageAssets() {
    if (m_ImagesLoaded) {
        return;
    }

    m_ImageAssets.clear();
    m_ImageAssets.reserve(m_ImagePaths.size());

    for (const auto& imagePath : m_ImagePaths) {
        auto imageAsset = AssetRef<ImageAsset>(new ImageAsset());
        if (imageAsset->LoadFromFile(imagePath, true, ImageColorSpace::sRGB)) {
            m_ImageAssets.push_back(imageAsset);
        } else {

            m_ImageAssets.push_back(AssetRef<ImageAsset>());
        }
    }

    m_ImagesLoaded = true;

    if (!m_ImageAssets.empty() && m_ImageAssets[0].IsValid()) {
        for (auto& animPair : m_Animations) {
            for (auto& frame : animPair.second.frames) {
                frame.uvRect = CalculateUVRect(frame.spriteIndex, m_ImageAssets[0]);
            }
        }
    }
}

void SpriteAnimationAsset::ParseSpriteConfig(const nlohmann::json& json) {
    m_SpriteConfig.type = json.value("type", "images");

    if (m_SpriteConfig.type == "sheet_grid") {
        m_SpriteConfig.cols = json.value("cols", 8);
        m_SpriteConfig.rows = json.value("rows", 8);
    } else if (m_SpriteConfig.type == "sheet_size") {
        m_SpriteConfig.width = json.value("width", 32);
        m_SpriteConfig.height = json.value("height", 32);
    } else if (m_SpriteConfig.type == "images") {
        m_SpriteConfig.displayCols = json.value("display_cols", 8);
    } else if (m_SpriteConfig.type == "aseprite") {
        m_SpriteConfig.jsonPath = json.value("json_path", "");
        m_SpriteConfig.imagePath = json.value("image_path", "");
    }
}

Vec4 SpriteAnimationAsset::CalculateUVRect(int spriteIndex, const AssetRef<ImageAsset>& image) const {
    if (!image.IsValid()) {
        return Vec4(0, 0, 1, 1);
    }

    int imageWidth = image->GetWidth();
    int imageHeight = image->GetHeight();

    if (imageWidth == 0 || imageHeight == 0) {
        return Vec4(0, 0, 1, 1);
    }

    if (m_SpriteConfig.type == "sheet_grid") {

        int cols = m_SpriteConfig.cols;
        int rows = m_SpriteConfig.rows;

        if (cols <= 0 || rows <= 0) {
            return Vec4(0, 0, 1, 1);
        }

        int row = spriteIndex / cols;
        int col = spriteIndex % cols;

        float spriteWidth = 1.0f / static_cast<float>(cols);
        float spriteHeight = 1.0f / static_cast<float>(rows);

        float u = col * spriteWidth;
        float v = row * spriteHeight;

        return Vec4(u, v, u + spriteWidth, v + spriteHeight);

    } else if (m_SpriteConfig.type == "sheet_size") {

        int spriteWidth = m_SpriteConfig.width;
        int spriteHeight = m_SpriteConfig.height;

        if (spriteWidth <= 0 || spriteHeight <= 0) {
            return Vec4(0, 0, 1, 1);
        }

        int cols = imageWidth / spriteWidth;
        int rows = imageHeight / spriteHeight;

        if (cols <= 0 || rows <= 0) {
            return Vec4(0, 0, 1, 1);
        }

        int row = spriteIndex / cols;
        int col = spriteIndex % cols;

        float u = (col * spriteWidth) / static_cast<float>(imageWidth);
        float v = (row * spriteHeight) / static_cast<float>(imageHeight);
        float uMax = ((col + 1) * spriteWidth) / static_cast<float>(imageWidth);
        float vMax = ((row + 1) * spriteHeight) / static_cast<float>(imageHeight);

        return Vec4(u, v, uMax, vMax);

    } else if (m_SpriteConfig.type == "images") {

        return Vec4(0, 0, 1, 1);
    }

    return Vec4(0, 0, 1, 1);
}

}
}

