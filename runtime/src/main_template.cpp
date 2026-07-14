/**
 * Lupine Engine - Export Template Runtime Entry Point
 *
 * This is the entry point for exported games. It loads game data from
 * a .pck file that is either:
 * 1. Adjacent to the executable (same directory, same name with .pck extension)
 * 2. Embedded at the end of the executable
 * 3. Specified via --pack command line argument
 * 4. (Web) Preloaded via Emscripten virtual filesystem
 */

#include "lupine/runtime/RuntimeApp.hpp"
#include "lupine/platform/PackFile.hpp"
#include "lupine/platform/FileSystem.hpp"
#include "lupine/platform/Path.hpp"
#include "lupine/core/Project.hpp"
#include "lupine/logger/Logger.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <algorithm>
#include <fstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

namespace {

// Magic marker for embedded pack data at end of executable
constexpr uint32_t EMBEDDED_PACK_MARKER = 0x4B434C50;  // "PLCK" in little-endian
constexpr size_t EMBEDDED_PACK_TRAILER_SIZE = 12;      // marker(4) + offset(8)

std::string getExecutablePath() {
#ifdef __EMSCRIPTEN__
    // On web, we don't have a real executable path
    // Return a virtual path that points to our preloaded data
    return "/game";
#elif defined(_WIN32)
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    return std::string(path);
#elif defined(__APPLE__)
    char path[PATH_MAX];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        return std::string(path);
    }
    return "";
#else
    char path[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
    if (count != -1) {
        return std::string(path, count);
    }
    return "";
#endif
}

std::string getExecutableDirectory() {
    std::string exePath = getExecutablePath();
    return lupine::platform::Path::GetDirectory(exePath);
}

std::string findPackFile(const std::string& explicitPath = "") {
#ifdef __EMSCRIPTEN__
    // On web, pack file is preloaded to virtual filesystem
    // Check standard locations in the virtual FS
    const char* webPackPaths[] = {
        "/game.pck",
        "/data.pck",
        "/assets/game.pck",
        "/assets/data.pck"
    };

    for (const char* path : webPackPaths) {
        if (lupine::platform::FileSystem::Exists(path)) {
            return path;
        }
    }

    // If no pack file, we'll rely on preloaded individual files
    return "";
#else
    // 1. Check explicit path from command line
    if (!explicitPath.empty()) {
        if (lupine::platform::FileSystem::Exists(explicitPath)) {
            return explicitPath;
        }
        std::cerr << "Specified pack file not found: " << explicitPath << std::endl;
        return "";
    }

    // 2. Check for adjacent .pck file with same name as executable
    std::string exePath = getExecutablePath();
    std::string exeDir = lupine::platform::Path::GetDirectory(exePath);
    std::string exeName = lupine::platform::Path::GetFilename(exePath);

    // Remove extension from exe name
    size_t dotPos = exeName.rfind('.');
    if (dotPos != std::string::npos) {
        exeName = exeName.substr(0, dotPos);
    }

    std::string adjacentPck = lupine::platform::Path::Join(exeDir, exeName + ".pck");
    if (lupine::platform::FileSystem::Exists(adjacentPck)) {
        return adjacentPck;
    }

    // 3. Check for game.pck in same directory
    std::string gamePck = lupine::platform::Path::Join(exeDir, "game.pck");
    if (lupine::platform::FileSystem::Exists(gamePck)) {
        return gamePck;
    }

    // 4. Check for data.pck in same directory
    std::string dataPck = lupine::platform::Path::Join(exeDir, "data.pck");
    if (lupine::platform::FileSystem::Exists(dataPck)) {
        return dataPck;
    }

    return "";
#endif
}

bool checkEmbeddedPack(const std::string& exePath, uint64_t& packOffset, uint64_t& packSize) {
    std::ifstream file(exePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return false;
    }

    std::streamsize fileSize = file.tellg();
    if (fileSize < static_cast<std::streamsize>(EMBEDDED_PACK_TRAILER_SIZE)) {
        return false;
    }

    // Read trailer from end of file
    file.seekg(-static_cast<std::streamoff>(EMBEDDED_PACK_TRAILER_SIZE), std::ios::end);

    uint32_t marker;
    file.read(reinterpret_cast<char*>(&marker), sizeof(uint32_t));

    if (marker != EMBEDDED_PACK_MARKER) {
        return false;
    }

    uint64_t offset;
    file.read(reinterpret_cast<char*>(&offset), sizeof(uint64_t));

    if (offset >= static_cast<uint64_t>(fileSize)) {
        return false;
    }

    packOffset = offset;
    packSize = static_cast<uint64_t>(fileSize) - offset - EMBEDDED_PACK_TRAILER_SIZE;

    return true;
}

struct GameConfig {
    std::string title = "Lupine Game";
    int windowWidth = 1280;
    int windowHeight = 720;
    bool fullscreen = false;
    bool vsync = true;
    bool resizable = true;
    bool borderless = false;
    int targetFps = 0;
    float physicsTickRate = 60.0f;
    std::string mainScene;
    lupine::GraphicsBackend backend = lupine::GraphicsBackend::OpenGL;

    // Viewport scaling mode (letterbox, stretch, crop, ignore)
    std::string viewportScaleMode = "letterbox";

    // Splash screen settings
    lupine::core::SplashScreenSettings splashScreenSettings;

    // Verbatim project JSON read from the pack. Parsed into a full
    // core::Project after the runtime initializes so that physics, audio and
    // rendering (clear color) settings are applied just like a disk project.
    std::string projectJson;
};

GameConfig loadConfigFromPack() {
    GameConfig config;

    auto& vfs = lupine::platform::PackFileSystem::Instance();

    // Try to load project.lupine from pack
    std::string projectJson = vfs.readFileAsString("project.lupine");
    if (projectJson.empty()) {
        // Try alternate names
        projectJson = vfs.readFileAsString("game.lupine");
        if (projectJson.empty()) {
            projectJson = vfs.readFileAsString("project.json");
        }
    }

    if (!projectJson.empty()) {
        config.projectJson = projectJson;
        try {
            nlohmann::json j = nlohmann::json::parse(projectJson);

            // Support both nested structure (new format) and flat structure (legacy)
            // New format: { "project": {...}, "window": {...}, "graphics": {...} }
            // Legacy format: { "name": ..., "window_width": ..., ... }

            bool isNestedFormat = j.contains("project") && j.contains("window");

            if (isNestedFormat) {
                // New nested format
                if (j.contains("project")) {
                    const auto& proj = j["project"];
                    if (proj.contains("name")) {
                        config.title = proj["name"].get<std::string>();
                    }
                    if (proj.contains("main_scene")) {
                        config.mainScene = proj["main_scene"].get<std::string>();
                        if (config.mainScene.find("res://") == 0) {
                            config.mainScene = config.mainScene.substr(6);
                        }
                    }
                }
                if (j.contains("window")) {
                    const auto& win = j["window"];
                    if (win.contains("width")) {
                        config.windowWidth = win["width"].get<int>();
                    }
                    if (win.contains("height")) {
                        config.windowHeight = win["height"].get<int>();
                    }
                    if (win.contains("fullscreen")) {
                        config.fullscreen = win["fullscreen"].get<bool>();
                    }
                    if (win.contains("vsync")) {
                        config.vsync = win["vsync"].get<bool>();
                    }
                    if (win.contains("resizable")) {
                        config.resizable = win["resizable"].get<bool>();
                    }
                    if (win.contains("borderless")) {
                        config.borderless = win["borderless"].get<bool>();
                    }
                    if (win.contains("target_fps")) {
                        config.targetFps = win["target_fps"].get<int>();
                    }
                }
                if (j.contains("physics")) {
                    const auto& physics = j["physics"];
                    if (physics.contains("physics_tick_rate")) {
                        config.physicsTickRate = physics["physics_tick_rate"].get<float>();
                    }
                }
                if (j.contains("graphics")) {
                    const auto& gfx = j["graphics"];
                    if (gfx.contains("scale_mode")) {
                        std::string scaleMode = gfx["scale_mode"].get<std::string>();
                        if (scaleMode == "letterbox") {
                            config.viewportScaleMode = "letterbox";
                        } else if (scaleMode == "stretch") {
                            config.viewportScaleMode = "stretch";
                        } else if (scaleMode == "crop") {
                            config.viewportScaleMode = "crop";
                        } else if (scaleMode == "ignore") {
                            config.viewportScaleMode = "ignore";
                        }
                    }
                }
                if (j.contains("export")) {
                    const auto& exp = j["export"];
                    if (exp.contains("graphics_backend")) {
                        std::string backendStr = exp["graphics_backend"].get<std::string>();
                        std::transform(backendStr.begin(), backendStr.end(), backendStr.begin(), ::tolower);
                        if (backendStr == "opengl") {
                            config.backend = lupine::GraphicsBackend::OpenGL;
                        } else if (backendStr == "vulkan") {
                            config.backend = lupine::GraphicsBackend::Vulkan;
                        } else if (backendStr == "dx11" || backendStr == "directx11") {
                            config.backend = lupine::GraphicsBackend::DirectX11;
                        } else if (backendStr == "dx12" || backendStr == "directx12") {
                            config.backend = lupine::GraphicsBackend::DirectX12;
                        } else if (backendStr == "metal") {
                            config.backend = lupine::GraphicsBackend::Metal;
                        }
                    }
                }
            } else {
                // Legacy flat format
                if (j.contains("name")) {
                    config.title = j["name"].get<std::string>();
                }
                if (j.contains("main_scene")) {
                    config.mainScene = j["main_scene"].get<std::string>();
                    if (config.mainScene.find("res://") == 0) {
                        config.mainScene = config.mainScene.substr(6);
                    }
                }
                if (j.contains("window_width")) {
                    config.windowWidth = j["window_width"].get<int>();
                }
                if (j.contains("window_height")) {
                    config.windowHeight = j["window_height"].get<int>();
                }
                if (j.contains("fullscreen")) {
                    config.fullscreen = j["fullscreen"].get<bool>();
                }
                if (j.contains("vsync")) {
                    config.vsync = j["vsync"].get<bool>();
                }
                if (j.contains("resizable")) {
                    config.resizable = j["resizable"].get<bool>();
                }
                if (j.contains("graphics_backend")) {
                    std::string backendStr = j["graphics_backend"].get<std::string>();
                    std::transform(backendStr.begin(), backendStr.end(), backendStr.begin(), ::tolower);
                    if (backendStr == "opengl") {
                        config.backend = lupine::GraphicsBackend::OpenGL;
                    } else if (backendStr == "vulkan") {
                        config.backend = lupine::GraphicsBackend::Vulkan;
                    } else if (backendStr == "dx11" || backendStr == "directx11") {
                        config.backend = lupine::GraphicsBackend::DirectX11;
                    } else if (backendStr == "dx12" || backendStr == "directx12") {
                        config.backend = lupine::GraphicsBackend::DirectX12;
                    } else if (backendStr == "metal") {
                        config.backend = lupine::GraphicsBackend::Metal;
                    }
                }
            }

            // Load splash screen settings
            if (j.contains("splash_screens")) {
                const auto& splash = j["splash_screens"];
                if (splash.contains("enabled")) {
                    config.splashScreenSettings.enabled = splash["enabled"].get<bool>();
                }
                if (splash.contains("allow_skip")) {
                    config.splashScreenSettings.allowSkip = splash["allow_skip"].get<bool>();
                }
                if (splash.contains("entries") && splash["entries"].is_array()) {
                    config.splashScreenSettings.entries.clear();
                    for (const auto& entry : splash["entries"]) {
                        lupine::core::SplashScreenEntry se;
                        if (entry.contains("image_path")) {
                            se.imagePath = entry["image_path"].get<std::string>();
                        }
                        if (entry.contains("fade_in_duration")) {
                            se.fadeInDuration = entry["fade_in_duration"].get<float>();
                        }
                        if (entry.contains("hold_duration")) {
                            se.holdDuration = entry["hold_duration"].get<float>();
                        }
                        if (entry.contains("fade_out_duration")) {
                            se.fadeOutDuration = entry["fade_out_duration"].get<float>();
                        }
                        config.splashScreenSettings.entries.push_back(se);
                    }
                }
            }

            std::cout << "Loaded project config: " << config.title
                      << " (" << config.windowWidth << "x" << config.windowHeight << ")"
                      << " fullscreen=" << config.fullscreen
                      << " scene=" << config.mainScene << std::endl;

        } catch (const std::exception& e) {
            std::cerr << "Failed to parse project file: " << e.what() << std::endl;
        }
    } else {
        std::cerr << "Warning: No project file found in pack" << std::endl;
    }

    return config;
}

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options]\n";
    std::cout << "Options:\n";
    std::cout << "  --help, -h              Show this help message\n";
    std::cout << "  --pack <path>           Load game data from specified .pck file\n";
    std::cout << "  --windowed              Force windowed mode\n";
    std::cout << "  --fullscreen            Force fullscreen mode\n";
    std::cout << "  --renderer <backend>    Graphics backend (opengl, vulkan, dx11, dx12, metal)\n";
}

#ifdef __EMSCRIPTEN__
// Emscripten main loop callback data
struct EmscriptenLoopData {
    lupine::RuntimeApp* app;
    bool initialized;
};

// Global pointer for Emscripten callback (required for emscripten_set_main_loop_arg)
static EmscriptenLoopData* g_loopData = nullptr;

void emscriptenMainLoop(void* arg) {
    EmscriptenLoopData* data = static_cast<EmscriptenLoopData*>(arg);
    if (!data || !data->app) {
        emscripten_cancel_main_loop();
        return;
    }

    if (!data->app->runFrame()) {
        emscripten_cancel_main_loop();
    }
}

// Update window size and content scale based on canvas and CSS sizes
// This ensures InputManager has the correct dimensions for mouse coordinate transforms
void updateCanvasSize() {
    if (!g_loopData || !g_loopData->app) return;

    // Get CSS display size (what Emscripten touch events report in targetX/Y)
    double cssWidth, cssHeight;
    emscripten_get_element_css_size("#canvas", &cssWidth, &cssHeight);

    // Get actual canvas pixel size (what the game uses and what SDL reports coordinates in)
    int canvasWidth, canvasHeight;
    emscripten_get_canvas_element_size("#canvas", &canvasWidth, &canvasHeight);

    auto* inputManager = g_loopData->app->getInputManager();
    if (inputManager) {
        // SDL on Emscripten reports mouse coordinates in canvas element space
        inputManager->SetWindowSize(canvasWidth, canvasHeight);

        // Content scale is CSS size / canvas size
        // Used by Emscripten touch callbacks to convert CSS coords to canvas coords
        // After resize callback syncs canvas to CSS, this will be 1.0
        float scaleX = (canvasWidth > 0) ? static_cast<float>(cssWidth) / static_cast<float>(canvasWidth) : 1.0f;
        float scaleY = (canvasHeight > 0) ? static_cast<float>(cssHeight) / static_cast<float>(canvasHeight) : 1.0f;
        inputManager->SetContentScale(scaleX, scaleY);
    }
}

// Handle canvas resize for responsive web games
EM_BOOL emscriptenResizeCallback(int eventType, const EmscriptenUiEvent* uiEvent, void* userData) {
    (void)eventType;
    (void)uiEvent;
    (void)userData;

    // Get CSS display size
    double cssWidth, cssHeight;
    emscripten_get_element_css_size("#canvas", &cssWidth, &cssHeight);

    // Set canvas pixel size to match CSS size for crisp rendering
    emscripten_set_canvas_element_size("#canvas",
        static_cast<int>(cssWidth),
        static_cast<int>(cssHeight));

    // Update InputManager with new canvas size
    updateCanvasSize();

    return EM_TRUE;
}
#endif

} // anonymous namespace

int main(int argc, char* argv[]) {
    // Export templates use embedded scripting (MicroPython, Lua, mRuby)
    // No external Python DLLs needed - everything is statically linked

#ifdef LUPINE_DEBUG_TEMPLATE
    // Built from the debug export template: symbols and function names are
    // preserved, so anything this build prints can be traced back to source.
    // Announcing it up front means a bug report's log says which template
    // produced it, instead of leaving that to be guessed at.
    std::cout << "Lupine Engine - debug template (symbols enabled)" << std::endl;
#endif

    std::string packPath;
    bool forceWindowed = false;
    bool forceFullscreen = false;
    std::string overrideRenderer;

#ifndef __EMSCRIPTEN__
    // Parse command line arguments (not available on web)
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        }
        else if (arg == "--pack" && i + 1 < argc) {
            packPath = argv[++i];
        }
        else if (arg == "--windowed") {
            forceWindowed = true;
        }
        else if (arg == "--fullscreen") {
            forceFullscreen = true;
        }
        else if ((arg == "--renderer" || arg == "-r") && i + 1 < argc) {
            overrideRenderer = argv[++i];
        }
    }
#else
    (void)argc;
    (void)argv;
#endif

    auto& vfs = lupine::platform::PackFileSystem::Instance();

#ifdef __EMSCRIPTEN__
    // On web, enable filesystem fallback for preloaded files
    vfs.setFilesystemFallback(true);
    vfs.setBasePath("/");

    std::cout << "Lupine Engine Web Build" << std::endl;
    std::cout << "Checking for preloaded game data..." << std::endl;
#else
    // Disable filesystem fallback for exported games
    vfs.setFilesystemFallback(false);
#endif

    // Try to find and mount pack file
    std::string exePath = getExecutablePath();
    bool packLoaded = false;

#ifndef __EMSCRIPTEN__
    uint64_t embeddedOffset, embeddedSize;

    // Storage for embedded pack data - must outlive the PackFileSystem usage
    // since mountPackFromMemory only stores a pointer to the data
    std::vector<uint8_t> embeddedPackData;

    // First check for embedded pack
    if (checkEmbeddedPack(exePath, embeddedOffset, embeddedSize)) {
        // Read embedded pack data
        std::ifstream file(exePath, std::ios::binary);
        if (file.is_open()) {
            file.seekg(embeddedOffset, std::ios::beg);
            embeddedPackData.resize(embeddedSize);
            file.read(reinterpret_cast<char*>(embeddedPackData.data()), embeddedSize);

            if (vfs.mountPackFromMemory(embeddedPackData.data(), embeddedPackData.size(), "embedded", 100)) {
                packLoaded = true;
                std::cout << "Loaded embedded game data" << std::endl;
            }
        }
    }
#endif

    // If no embedded pack, try external pack file
    if (!packLoaded) {
        std::string foundPack = findPackFile(packPath);
        if (!foundPack.empty()) {
            if (vfs.mountPack(foundPack, 100)) {
                packLoaded = true;
                std::cout << "Loaded game data from: " << foundPack << std::endl;
            }
        }
    }

#ifdef __EMSCRIPTEN__
    // On web, if no pack file, check for preloaded project file directly
    if (!packLoaded) {
        // Check if we have preloaded files (project.lupine or similar)
        if (lupine::platform::FileSystem::Exists("/project.lupine") ||
            lupine::platform::FileSystem::Exists("/game.lupine")) {
            std::cout << "Using preloaded project files" << std::endl;
            packLoaded = true;  // We'll load directly from preloaded FS
        }
    }
#endif

    if (!packLoaded) {
        std::cerr << "Error: No game data found!\n";
        std::cerr << "Expected one of:\n";
#ifdef __EMSCRIPTEN__
        std::cerr << "  - Preloaded game.pck or data.pck\n";
        std::cerr << "  - Preloaded project.lupine file\n";
#else
        std::cerr << "  - Embedded pack data in executable\n";
        std::cerr << "  - Adjacent .pck file with same name as executable\n";
        std::cerr << "  - game.pck or data.pck in executable directory\n";
        std::cerr << "  - Pack file specified with --pack argument\n";
#endif
        return -1;
    }

#ifndef __EMSCRIPTEN__
    // Set base path to executable directory for any filesystem fallback
    vfs.setBasePath(getExecutableDirectory());
#endif

    // Load game configuration from pack
    // Made static for Emscripten where main() returns before the game loop completes
    static GameConfig gameConfig = loadConfigFromPack();

    // Apply command line overrides
    if (forceWindowed) {
        gameConfig.fullscreen = false;
    }
    if (forceFullscreen) {
        gameConfig.fullscreen = true;
    }

#ifdef __EMSCRIPTEN__
    // On web, always use WebGL backend
    gameConfig.backend = lupine::GraphicsBackend::WebGL;
    // Web builds should not be fullscreen by default (user can click fullscreen button)
    gameConfig.fullscreen = false;
#else
    if (!overrideRenderer.empty()) {
        std::string backendStr = overrideRenderer;
        std::transform(backendStr.begin(), backendStr.end(), backendStr.begin(), ::tolower);

        if (backendStr == "opengl") {
            gameConfig.backend = lupine::GraphicsBackend::OpenGL;
        } else if (backendStr == "vulkan") {
            gameConfig.backend = lupine::GraphicsBackend::Vulkan;
        } else if (backendStr == "dx11" || backendStr == "directx11") {
            gameConfig.backend = lupine::GraphicsBackend::DirectX11;
        } else if (backendStr == "dx12" || backendStr == "directx12") {
            gameConfig.backend = lupine::GraphicsBackend::DirectX12;
        } else if (backendStr == "metal") {
            gameConfig.backend = lupine::GraphicsBackend::Metal;
        } else {
            std::cerr << "Unknown renderer backend: " << backendStr << std::endl;
            return -1;
        }
    }
#endif

    // Initialize and run the runtime
    static lupine::RuntimeApp app;  // Static for Emscripten callback access
    lupine::RuntimeApp::Config config;
    config.title = gameConfig.title;
    config.windowWidth = gameConfig.windowWidth;
    config.windowHeight = gameConfig.windowHeight;
    config.debugging = false;
    config.vsync = gameConfig.vsync;
    config.resizable = gameConfig.resizable;
    config.fullscreen = gameConfig.fullscreen;
    config.borderless = gameConfig.borderless;
    config.targetFrameRate = gameConfig.targetFps;
    config.physicsTickRate = gameConfig.physicsTickRate;
    config.backend = gameConfig.backend;

    std::cout << "=== Template Config ===" << std::endl;
    std::cout << "  Window: " << gameConfig.windowWidth << "x" << gameConfig.windowHeight << std::endl;
    std::cout << "  Scale Mode: " << gameConfig.viewportScaleMode << std::endl;
    std::cout << "  Main Scene: " << gameConfig.mainScene << std::endl;

    // Set viewport scale mode from game config
    if (gameConfig.viewportScaleMode == "letterbox") {
        config.viewportScaleMode = lupine::core::ProjectSettings::ViewportScaleMode::Letterbox;
    } else if (gameConfig.viewportScaleMode == "stretch") {
        config.viewportScaleMode = lupine::core::ProjectSettings::ViewportScaleMode::Stretch;
    } else if (gameConfig.viewportScaleMode == "crop") {
        config.viewportScaleMode = lupine::core::ProjectSettings::ViewportScaleMode::Crop;
    } else if (gameConfig.viewportScaleMode == "ignore") {
        config.viewportScaleMode = lupine::core::ProjectSettings::ViewportScaleMode::Ignore;
    }

    // For template mode, we don't use projectPath - the VFS handles everything
    // The scene manager will be modified to use VFS

    if (!app.initialize(config)) {
        std::cerr << "Failed to initialize runtime" << std::endl;
        return -1;
    }

    // Give the scene manager a full project parsed from the packed project file
    // so physics (gravity/tick), audio bus volumes and the rendering clear color
    // are applied exactly as in a disk-based project run.
    if (!gameConfig.projectJson.empty()) {
        auto* sceneManager = app.getSceneManager();
        if (sceneManager) {
            auto project = std::make_unique<lupine::core::Project>();
            if (project->LoadFromString(gameConfig.projectJson)) {
                sceneManager->AdoptProject(std::move(project));
                std::cout << "Applied full project settings from pack" << std::endl;
            } else {
                std::cerr << "Failed to parse packed project settings" << std::endl;
            }
        }
    }

    // Load input map from pack file
    // The input map is stored in the pack file as "input_map.json"
    // Note: vfs is already defined earlier in main()
    std::string inputMapJson = vfs.readFileAsString("input_map.json");
    if (!inputMapJson.empty()) {
        auto* inputManager = app.getInputManager();
        if (inputManager) {
            if (inputManager->LoadInputMapFromString(inputMapJson)) {
                std::cout << "Loaded input map from pack file" << std::endl;
            } else {
                std::cerr << "Failed to parse input map from pack file" << std::endl;
            }
        }
    } else {
        std::cout << "No input_map.json found in pack file (using defaults)" << std::endl;
    }

    // Lambda to load the main scene (called after splash screens or immediately)
    // Note: app and gameConfig are static, so they're accessible without capturing
    auto loadMainScene = []() {
        if (!gameConfig.mainScene.empty()) {
            std::cout << "Attempting to load scene: " << gameConfig.mainScene << std::endl;
            if (!app.loadScene(gameConfig.mainScene)) {
                std::cerr << "Failed to load main scene: " << gameConfig.mainScene << std::endl;
                // Continue anyway - might have default scene
            } else {
                std::cout << "Scene loaded successfully" << std::endl;
                // Debug: Check scene contents
                auto* sceneManager = app.getSceneManager();
                if (sceneManager) {
                    auto* scene = sceneManager->GetCurrentScene();
                    if (scene) {
                        auto root = scene->GetRoot();
                        if (root) {
                            std::cout << "Scene root: " << root->GetName() << " with " << root->GetChildren().size() << " children" << std::endl;
                        } else {
                            std::cerr << "Scene has no root node!" << std::endl;
                        }
                    } else {
                        std::cerr << "No current scene in manager!" << std::endl;
                    }
                }
            }
        }
    };

    // Setup splash screens with callback to load main scene when complete
    // The splash screens will be shown during run() and callback fires when done
    if (gameConfig.splashScreenSettings.enabled && !gameConfig.splashScreenSettings.entries.empty()) {
        std::cout << "Setting up splash screens..." << std::endl;
        app.setupManualSplashScreens(
            gameConfig.splashScreenSettings,
            static_cast<float>(gameConfig.windowWidth),
            static_cast<float>(gameConfig.windowHeight),
            loadMainScene
        );
    } else {
        // No splash screens, load main scene immediately
        loadMainScene();
    }

#ifdef __EMSCRIPTEN__
    // Set up Emscripten main loop
    std::cout << "Starting Emscripten main loop..." << std::endl;

    // Register resize callback for responsive canvas
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, emscriptenResizeCallback);

    // Create loop data
    static EmscriptenLoopData loopData;
    loopData.app = &app;
    loopData.initialized = true;
    g_loopData = &loopData;

    // Initialize canvas size for InputManager
    // This ensures correct mouse input handling at startup before any resize events
    updateCanvasSize();

    // Use 0 for fps to use requestAnimationFrame (vsync, power-efficient)
    // Use 1 for simulate_infinite_loop to prevent the function from returning
    emscripten_set_main_loop_arg(emscriptenMainLoop, &loopData, 0, 1);

    // This code is reached only if the main loop is cancelled
    std::cout << "Emscripten main loop ended" << std::endl;
#else
    app.run();
#endif

    return 0;
}
