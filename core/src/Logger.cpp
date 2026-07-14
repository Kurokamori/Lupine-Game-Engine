#include "lupine/logger/Logger.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <vector>
#include <fstream>
#include <iostream>

namespace lupine {

    std::shared_ptr<spdlog::logger> Logger::s_Logger = nullptr;
    bool Logger::s_FileLoggingEnabled = true;
    std::string Logger::s_LogFilePath = "lupine.log";

    void Logger::Init(const std::string& logFilePath, bool enableFileLogging) {
        try {
            s_LogFilePath = logFilePath;
            s_FileLoggingEnabled = enableFileLogging;

            if (enableFileLogging) {
                std::ofstream logFile(logFilePath, std::ios::trunc);
                if (logFile.is_open()) {
                    logFile.close();
                } else {
                }
            }

            std::vector<spdlog::sink_ptr> sinks;

            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(spdlog::level::trace);
            console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
            sinks.push_back(console_sink);

            if (enableFileLogging) {
                auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath, false);
                file_sink->set_level(spdlog::level::trace);
                file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
                sinks.push_back(file_sink);
            }

            s_Logger = std::make_shared<spdlog::logger>("LupineLogger", sinks.begin(), sinks.end());
            s_Logger->set_level(spdlog::level::trace);
            s_Logger->flush_on(spdlog::level::err);

            spdlog::register_logger(s_Logger);

            spdlog::set_default_logger(s_Logger);

        } catch (const spdlog::spdlog_ex& ex) {
            // The logger is what failed, so report through stderr directly.
            std::cerr << "[Lupine] Logger initialization failed: " << ex.what() << std::endl;
        }
    }

    void Logger::Shutdown() {
        if (s_Logger) {

            s_Logger->flush();
            spdlog::shutdown();
            s_Logger = nullptr;
        }
    }

    std::shared_ptr<spdlog::logger>& Logger::GetLogger() {

        if (!s_Logger) {
            Init();
        }
        return s_Logger;
    }

    void Logger::SetFileLogging(bool enabled) {
        s_FileLoggingEnabled = enabled;

    }

    void Logger::SetLogLevel(spdlog::level::level_enum level) {
        if (s_Logger) {
            s_Logger->set_level(level);

        }
    }

    void Logger::Flush() {
        if (s_Logger) {
            s_Logger->flush();
        }
    }

    const char* LogCategoryToString(LogCategory category) {
        switch (category) {
            case LogCategory::Core:         return "Core";
            case LogCategory::ECS:          return "ECS";
            case LogCategory::Render:       return "Render";
            case LogCategory::Audio:        return "Audio";
            case LogCategory::Physics:      return "Physics";
            case LogCategory::Asset:        return "Asset";
            case LogCategory::Scripting:    return "Scripting";
            case LogCategory::Network:      return "Network";
            case LogCategory::Input:        return "Input";
            case LogCategory::UI:           return "UI";
            default:                        return "Unknown";
        }
    }

}
