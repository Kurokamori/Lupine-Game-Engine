#include "lupine/runtime/RuntimeApp.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options] [-- <game args>]\n";
    std::cout << "Options:\n";
    std::cout << "  --help, -h              Show this help message\n";
    std::cout << "  --debug, -d             Enable debug mode\n";
    std::cout << "  --project <path>        Load project from path\n";
    std::cout << "  --scene <path>          Load specific scene (requires --project)\n";
    std::cout << "  --renderer <backend>    Graphics backend (opengl, vulkan, dx11, dx12, metal)\n";
    std::cout << "                          Default: opengl\n";
    std::cout << "  --user-path <path>      Override the user:// directory (per-instance data)\n";
    std::cout << "  -- <game args>          Everything after -- is passed to the game verbatim\n";
    std::cout << "                          (readable via get_cmdline_args()). Unrecognized\n";
    std::cout << "                          arguments are also forwarded to the game.\n";
}

int main(int argc, char* argv[]) {

    bool debugging = false;
    std::string projectPath;
    std::string scenePath;
    std::string userPath;
    std::vector<std::string> gameArgs;
    lupine::GraphicsBackend backend = lupine::GraphicsBackend::OpenGL;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--") {
            // Everything after the separator is passed to the game verbatim.
            for (int j = i + 1; j < argc; ++j) {
                gameArgs.emplace_back(argv[j]);
            }
            break;
        }
        else if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        }
        else if (arg == "--debug" || arg == "-d") {
            debugging = true;
        }
        else if (arg == "--project" && i + 1 < argc) {
            projectPath = argv[++i];
        }
        else if (arg == "--scene" && i + 1 < argc) {
            scenePath = argv[++i];
        }
        else if (arg == "--user-path" && i + 1 < argc) {
            userPath = argv[++i];
        }
        else if ((arg == "--renderer" || arg == "-r") && i + 1 < argc) {
            std::string backendStr = argv[++i];
            std::transform(backendStr.begin(), backendStr.end(), backendStr.begin(), ::tolower);

            if (backendStr == "opengl") {
                backend = lupine::GraphicsBackend::OpenGL;
            } else if (backendStr == "vulkan") {
                backend = lupine::GraphicsBackend::Vulkan;
            } else if (backendStr == "dx11" || backendStr == "directx11") {
                backend = lupine::GraphicsBackend::DirectX11;
            } else if (backendStr == "dx12" || backendStr == "directx12") {
                backend = lupine::GraphicsBackend::DirectX12;
            } else if (backendStr == "metal") {
                backend = lupine::GraphicsBackend::Metal;
            } else {
                std::cerr << "Unknown renderer backend: " << backendStr << std::endl;
                std::cerr << "Valid options: opengl, vulkan, dx11, dx12, metal" << std::endl;
                return -1;
            }
        }
        else {
            // Unrecognized argument: forward it to the game so scripts can read it.
            gameArgs.emplace_back(arg);
        }
    }

    if (!scenePath.empty() && projectPath.empty()) {
        printUsage(argv[0]);
        return -1;
    }

    lupine::RuntimeApp app;
    lupine::RuntimeApp::Config config;
    config.title = "Lupine Runtime";
    config.windowWidth = 1280;
    config.windowHeight = 720;
    config.debugging = debugging;
    config.vsync = true;
    config.resizable = true;
    config.backend = backend;
    config.projectPath = projectPath;
    config.scenePath = scenePath;
    config.userPath = userPath;
    config.args = gameArgs;

    if (!app.initialize(config)) {
        return -1;
    }

    app.run();

    app.shutdown();

    return 0;
}
