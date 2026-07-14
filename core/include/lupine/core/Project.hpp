#pragma once

#include "lupine/core/Core.hpp"
#include "lupine/math/Math.hpp"
#include <string>
#include <memory>
#include <vector>

namespace lupine {
namespace core {

/**
 * Single splash screen entry configuration
 */
struct SplashScreenEntry {
    std::string imagePath;      // res:// path to splash image
    float fadeInDuration;       // Duration of fade in (seconds)
    float holdDuration;         // Duration to hold at full opacity (seconds)
    float fadeOutDuration;      // Duration of fade out (seconds)

    SplashScreenEntry()
        : imagePath("")
        , fadeInDuration(0.3f)
        , holdDuration(2.0f)
        , fadeOutDuration(0.3f) {}

    SplashScreenEntry(const std::string& path, float fadeIn = 0.3f, float hold = 2.0f, float fadeOut = 0.3f)
        : imagePath(path)
        , fadeInDuration(fadeIn)
        , holdDuration(hold)
        , fadeOutDuration(fadeOut) {}
};

/**
 * Splash screen settings for project
 */
struct SplashScreenSettings {
    bool enabled;               // Whether splash screens are enabled
    bool allowSkip;             // Whether user can skip splash screens with input
    std::vector<SplashScreenEntry> entries;

    SplashScreenSettings()
        : enabled(true)
        , allowSkip(true)
        , entries() {}
};

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
    std::string iconPath;   // res:// path to the project/game icon (window + taskbar)
    
    // Display settings.
    //
    // windowWidth/windowHeight are the DESIGN RESOLUTION: the fixed logical canvas
    // that UI is authored in, that anchor layout resolves against, and that the UI
    // render canvas projects. They are NOT necessarily the pixel size of the OS
    // window. The window is scaled (letterbox/stretch/crop/ignore) to whatever
    // physical size it actually opens at, so the design resolution stays constant.
    int windowWidth;
    int windowHeight;
    bool fullscreen;
    bool vsync;
    bool windowResizable;   // Allow the OS window to be resized by the user
    bool windowBorderless;  // Create the window without a title bar / border

    // Actual window-size override. When windowSizeOverride is true the OS window
    // opens at windowSizeOverrideWidth x windowSizeOverrideHeight instead of the
    // design resolution above, while UI layout/rendering keep using the design
    // resolution. The viewport scale mode then maps the design canvas onto the
    // (different) physical window. When false the window opens at the design
    // resolution (the historical behavior).
    bool windowSizeOverride;
    int windowSizeOverrideWidth;
    int windowSizeOverrideHeight;

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

    // Named collision layers (32 each). Used by editors and gameplay code to
    // resolve human-readable names for the 2D/3D physics layer bitmasks. The
    // runtime preserves these so they round-trip losslessly.
    std::vector<std::string> collisionLayers2D;
    std::vector<std::string> collisionLayers3D;

    // Audio settings (0.0 - 1.0). Applied to the AudioManager master bus and
    // the built-in "Music"/"SFX" buses on project load.
    float masterVolume;
    float musicVolume;
    float sfxVolume;

    // Rendering settings
    math::Color clearColor;
    int targetFrameRate;  // 0 = unlimited

    // Default font path (res:// path to TTF/OTF file, empty = use engine default)
    std::string defaultFont;

    // Default UI theme path (res:// path to a .uitheme; empty = the
    // res://default.uitheme convention, else no theme)
    std::string defaultTheme;

    // Editor settings
    int maxUndoSteps;  // Maximum number of undo steps to keep in history

    // Networking defaults. Consumed into the NetworkManager's default session
    // config on project load (SceneManager::ApplyProjectSettings), so that a
    // game's Network.start_*()/lc_net_* calls inherit the project's transport,
    // tick/keyframe/ping rates, compatibility gate, and LAN discovery options
    // without restating them. Games may still override per-session.
    std::string networkTransport;        // "enet" | "websocket" | "loopback"
    int networkDefaultPort;
    int networkMaxPeers;
    float networkTickRate;               // Hz
    float networkInterpDelayMs;
    float networkKeyframeIntervalMs;
    float networkPingIntervalSeconds;
    float networkInterestRadius;
    int networkProtocolVersion;
    std::string networkGameId;
    bool networkEnableLanDiscovery;
    int networkDiscoveryPort;
    std::string networkServerName;
    bool networkEnablePrediction;        // Client-side prediction master toggle.
    int networkInputRedundancy;          // Input commands repeated per packet.
    bool networkAutoReconnect;           // Client retries on unexpected drop.
    int networkReconnectAttempts;
    float networkReconnectDelaySeconds;
    float networkResumeTimeoutSeconds;   // Server slot-hold for resume (0=off).
    int networkMaxMessagesPerSecond;     // Anti-flood (0=off).
    int networkMaxBytesPerSecond;

    // Splash screen settings
    SplashScreenSettings splashScreenSettings;

private:
    // RegisterProperties() appends to ISerializable::m_Properties and is re-entered on
    // every Load()/LoadFromString(); without this guard a reload duplicates every property.
    bool m_PropertiesRegistered = false;
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

    /**
     * Parse project settings from an in-memory JSON string (e.g. read from a
     * pack file by the export runtime). Uses the exact same schema as Load().
     * The project path is left unset; callers that need it should set it
     * separately. Returns true on success.
     */
    bool LoadFromString(const std::string& jsonString);

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

    // Verbatim text of the last loaded project file. SaveAs() merges C++-owned
    // settings on top of this so editor-only keys (structure, icon, input map,
    // localization, editor preferences) are never dropped on a round-trip.
    std::string m_RawProjectJson;
};

} // namespace core
} // namespace lupine

