#include "lupine/runtime/RuntimeApp.hpp"
#include <iostream>
#include <string>

void printUsage(const char* programName) {
}

int main(int argc, char* argv[]) {

    bool debugging = false;
    std::string projectPath;
    std::string scenePath;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
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
    config.projectPath = projectPath;
    config.scenePath = scenePath;

    if (!app.initialize(config)) {
        return -1;
    }

    app.run();

    app.shutdown();

    return 0;
}
