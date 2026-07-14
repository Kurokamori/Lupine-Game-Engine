#include "lupine/asset/AnimationClipAsset.hpp"
#include "lupine/platform/FileSystem.hpp"
#include "lupine/platform/PackFile.hpp"
#include <nlohmann/json.hpp>

namespace lupine {
namespace asset {

AnimationClipAsset::AnimationClipAsset()
    : Asset() {
}

AnimationClipAsset::AnimationClipAsset(const core::UUID& uuid)
    : Asset(uuid) {
}

AnimationClipAsset::~AnimationClipAsset() {
}

bool AnimationClipAsset::LoadFromFile(const std::string& filepath) {
    SetPath(filepath);

    std::string fileContents;
    auto& packFS = platform::PackFileSystem::Instance();
    if (packFS.isPackMode() && packFS.exists(filepath)) {
        fileContents = packFS.readFileAsString(filepath);
        if (fileContents.empty()) {
            return false;
        }
    } else {
        auto result = platform::FileSystem::ReadFile(filepath);
        if (!result.success) {
            return false;
        }
        fileContents = std::move(result.data);
    }

    try {
        nlohmann::json json = nlohmann::json::parse(fileContents);
        m_Clip = animation::AnimationClip::Deserialize(json);
    } catch (const std::exception&) {
        return false;
    }

    SetName(m_Clip.name);
    SetLoaded(true);
    return true;
}

bool AnimationClipAsset::SaveToFile(const std::string& filepath) {
    SetPath(filepath);
    try {
        nlohmann::json json = m_Clip.Serialize();
        auto result = platform::FileSystem::WriteFile(filepath, json.dump(2));
        return result.success;
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace asset
} // namespace lupine
