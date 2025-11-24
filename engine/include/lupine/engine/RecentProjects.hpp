#pragma once

#include "lupine/core/Core.hpp"
#include <string>
#include <vector>
#include <memory>

namespace lupine {
namespace engine {

/**
 * Represents a recent project entry
 */
struct RecentProjectEntry {
    std::string projectPath;      // Full path to the .lupine project file
    std::string projectName;      // Display name of the project
    int64_t lastOpenedTime;      // Unix timestamp of last opened time
    
    RecentProjectEntry() : lastOpenedTime(0) {}
    RecentProjectEntry(const std::string& path, const std::string& name, int64_t time)
        : projectPath(path), projectName(name), lastOpenedTime(time) {}
};

/**
 * Manages the list of recently opened/created projects
 * Handles adding, removing, and reordering projects based on usage
 */
class RecentProjects : public core::ISerializable {
public:
    RecentProjects();
    ~RecentProjects();

    // ISerializable interface
    std::string GetTypeName() const override { return "RecentProjects"; }
    void RegisterProperties() override;
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& json) override;

    /**
     * Adds or updates a project in the recent list
     * If the project already exists, it's moved to the top
     * @param projectPath Full path to the project file
     * @param projectName Display name of the project
     */
    void AddOrUpdateProject(const std::string& projectPath, const std::string& projectName);

    /**
     * Removes a project from the recent list
     * @param projectPath Full path to the project file
     */
    void RemoveProject(const std::string& projectPath);

    /**
     * Clears all recent projects
     */
    void Clear();

    /**
     * Gets the list of recent projects (ordered by most recent first)
     * @return Vector of recent project entries
     */
    const std::vector<RecentProjectEntry>& GetProjects() const { return m_Projects; }

    /**
     * Gets the maximum number of recent projects to keep
     * @return Maximum count
     */
    int GetMaxProjects() const { return m_MaxProjects; }

    /**
     * Sets the maximum number of recent projects to keep
     * @param maxProjects Maximum count
     */
    void SetMaxProjects(int maxProjects);

    /**
     * Loads the recent projects list from the default location
     * @return True if loaded successfully, false otherwise
     */
    bool Load();

    /**
     * Saves the recent projects list to the default location
     * @return True if saved successfully, false otherwise
     */
    bool Save();

    /**
     * Gets the default file path for storing recent projects
     * @return Path to the recent projects file
     */
    static std::string GetDefaultFilePath();

private:
    std::vector<RecentProjectEntry> m_Projects;
    int m_MaxProjects;  // Maximum number of recent projects to keep (default: 10)
    
    void TrimToMaxSize();
};

} // namespace engine
} // namespace lupine
