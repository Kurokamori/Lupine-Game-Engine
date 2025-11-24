#pragma once

#include "lupine/core/Core.hpp"
#include "lupine/math/Math.hpp"
#include <string>
#include <memory>

namespace lupine {
namespace core {

/**
 * Project settings - stores metadata about the project
 */
class ProjectSettings : public ISerializable {
public:
    ProjectSettings();

    // ISerializable interface
    std::string GetTypeName() const override { return "ProjectSettings"; }
    void RegisterProperties() override;

    // Project metadata
    std::string projectName;
    std::string creatorName;
    std::string version;
    std::string mainScene;  // Path to the default/main scene
    
    // Display settings
    int windowWidth;
    int windowHeight;
    bool fullscreen;
    bool vsync;
    
    // Scale settings
    math::Vec2 referenceResolution;
    enum class ScaleMode {
        PixelPerfect,
        ScaleWithScreenSize,
        ConstantPhysicalSize
    };
    ScaleMode scaleMode;

    // Graphics settings
    enum class ViewportScaleMode {
        Letterbox,  // Maintain aspect ratio with black bars
        Stretch,    // Stretch to fill window (may distort)
        Crop,       // Maintain aspect ratio, crop edges
        Ignore      // Use window size directly
    };
    ViewportScaleMode viewportScaleMode;

    enum class TextureFiltering {
        NearestNeighbor,  // Pixelated, sharp
        Bilinear,         // Smooth, standard
        Cubic             // High quality, smooth
    };
    TextureFiltering textureFiltering;

    // Physics settings
    float physicsTickRate;  // Hz
    math::Vec2 gravity2D;
    math::Vec3 gravity3D;

    // Rendering settings
    math::Color clearColor;
    int targetFrameRate;  // 0 = unlimited

    // Editor settings
    int maxUndoSteps;  // Maximum number of undo steps to keep in history
};

/**
 * Project class - represents a Lupine Engine project (.lupine file)
 */
class Project {
public:
    Project();
    explicit Project(const std::string& projectPath);
    ~Project();

    // Load/Save project
    bool Load(const std::string& projectPath);
    bool Save();
    bool SaveAs(const std::string& projectPath);

    // Project settings
    ProjectSettings& GetSettings() { return m_Settings; }
    const ProjectSettings& GetSettings() const { return m_Settings; }

    // Project paths
    const std::string& GetProjectPath() const { return m_ProjectPath; }
    std::string GetProjectDirectory() const;
    std::string GetScenesDirectory() const;
    std::string GetAssetsDirectory() const;
    std::string GetScriptsDirectory() const;

    // Project state
    bool IsLoaded() const { return m_Loaded; }

private:
    ProjectSettings m_Settings;
    std::string m_ProjectPath;
    bool m_Loaded;
};

} // namespace core
} // namespace lupine

