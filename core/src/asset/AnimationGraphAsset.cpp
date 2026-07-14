#include "lupine/asset/AnimationGraphAsset.hpp"
#include "lupine/platform/FileSystem.hpp"
#include "lupine/platform/PackFile.hpp"
#include <nlohmann/json.hpp>

namespace lupine {
namespace asset {

AnimationGraphAsset::AnimationGraphAsset()
    : Asset() {
}

AnimationGraphAsset::AnimationGraphAsset(const core::UUID& uuid)
    : Asset(uuid) {
}

AnimationGraphAsset::~AnimationGraphAsset() {
}

bool AnimationGraphAsset::LoadFromFile(const std::string& filepath) {
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
        m_Graph = animation::AnimationGraph::Deserialize(json);
    } catch (const std::exception&) {
        return false;
    }

    SetLoaded(true);
    return true;
}

bool AnimationGraphAsset::SaveToFile(const std::string& filepath) {
    SetPath(filepath);
    try {
        nlohmann::json json = m_Graph.Serialize();
        auto result = platform::FileSystem::WriteFile(filepath, json.dump(2));
        return result.success;
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace asset
} // namespace lupine
