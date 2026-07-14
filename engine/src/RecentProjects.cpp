#include "lupine/engine/RecentProjects.hpp"
#include "lupine/platform/Platform.hpp"
#include "lupine/logger/Logger.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>

#if defined(LUPINE_PLATFORM_WINDOWS)
    #ifdef CreateDirectory
        #undef CreateDirectory
    #endif
    #ifdef DeleteFile
        #undef DeleteFile
    #endif
    #ifdef CopyFile
        #undef CopyFile
    #endif
    #ifdef MoveFile
        #undef MoveFile
    #endif
#endif

namespace lupine {
namespace engine {

RecentProjects::RecentProjects()
    : m_MaxProjects(10) {
}

RecentProjects::~RecentProjects() {
}

void RecentProjects::RegisterProperties() {

}

nlohmann::json RecentProjects::Serialize() const {
    nlohmann::json json;
    json["type"] = GetTypeName();
    json["max_projects"] = m_MaxProjects;

    nlohmann::json projectsArray = nlohmann::json::array();
    for (const auto& entry : m_Projects) {
        nlohmann::json entryJson;
        entryJson["path"] = entry.projectPath;
        entryJson["name"] = entry.projectName;
        entryJson["last_opened"] = entry.lastOpenedTime;
        projectsArray.push_back(entryJson);
    }
    json["projects"] = projectsArray;

    return json;
}

void RecentProjects::Deserialize(const nlohmann::json& json) {
    m_Projects.clear();

    if (json.contains("max_projects")) {
        m_MaxProjects = json["max_projects"].get<int>();
    }

    if (json.contains("projects") && json["projects"].is_array()) {
        for (const auto& entryJson : json["projects"]) {
            RecentProjectEntry entry;
            if (entryJson.contains("path")) {
                entry.projectPath = entryJson["path"].get<std::string>();
            }
            if (entryJson.contains("name")) {
                entry.projectName = entryJson["name"].get<std::string>();
            }
            if (entryJson.contains("last_opened")) {
                entry.lastOpenedTime = entryJson["last_opened"].get<int64_t>();
            }

            if (platform::FileSystem::Exists(entry.projectPath)) {
                m_Projects.push_back(entry);
            }
        }
    }
}

void RecentProjects::AddOrUpdateProject(const std::string& projectPath, const std::string& projectName) {

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    auto it = std::find_if(m_Projects.begin(), m_Projects.end(),
        [&projectPath](const RecentProjectEntry& entry) {
            return entry.projectPath == projectPath;
        });

    if (it != m_Projects.end()) {

        it->projectName = projectName;
        it->lastOpenedTime = timestamp;

        if (it != m_Projects.begin()) {
            RecentProjectEntry entry = *it;
            m_Projects.erase(it);
            m_Projects.insert(m_Projects.begin(), entry);
        }
    } else {

        RecentProjectEntry entry(projectPath, projectName, timestamp);
        m_Projects.insert(m_Projects.begin(), entry);
    }

    TrimToMaxSize();
}

void RecentProjects::RemoveProject(const std::string& projectPath) {
    auto it = std::find_if(m_Projects.begin(), m_Projects.end(),
        [&projectPath](const RecentProjectEntry& entry) {
            return entry.projectPath == projectPath;
        });

    if (it != m_Projects.end()) {
        m_Projects.erase(it);
    }
}

void RecentProjects::Clear() {
    m_Projects.clear();
}

void RecentProjects::SetMaxProjects(int maxProjects) {
    if (maxProjects > 0) {
        m_MaxProjects = maxProjects;
        TrimToMaxSize();
    }
}

bool RecentProjects::Load() {
    std::string filePath = GetDefaultFilePath();

    if (!platform::FileSystem::Exists(filePath)) {

        return true;
    }

    auto result = platform::FileSystem::ReadFile(filePath);
    if (!result.success) {

        return false;
    }

    try {
        nlohmann::json json = nlohmann::json::parse(result.data);
        Deserialize(json);

        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(LogCategory::Tools, "RecentProjects: failed to load recent-projects file: {}", e.what());
        return false;
    }
}

bool RecentProjects::Save() {
    std::string filePath = GetDefaultFilePath();

    std::string dirPath = platform::Path::GetDirectory(filePath);
    auto dirResult = platform::FileSystem::CreateDirectory(dirPath, true);
    if (!dirResult.success) {

        return false;
    }

    try {
        nlohmann::json json = Serialize();
        std::string jsonStr = json.dump(2);

        auto result = platform::FileSystem::WriteFile(filePath, jsonStr);
        if (!result.success) {

            return false;
        }

        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(LogCategory::Tools, "RecentProjects: failed to save recent-projects file: {}", e.what());
        return false;
    }
}

std::string RecentProjects::GetDefaultFilePath() {

    auto& vfs = platform::VirtualFileSystem::GetInstance();
    auto result = vfs.ResolvePath("user://editor/recent_projects.json");

    if (result.success) {
        return result.data;
    }

    return "recent_projects.json";
}

void RecentProjects::TrimToMaxSize() {
    if (static_cast<int>(m_Projects.size()) > m_MaxProjects) {
        m_Projects.resize(m_MaxProjects);
    }
}

}
}
