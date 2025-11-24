#pragma once

#include <memory>
#include <string>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace lupine {

    // Log categories
    enum class LogCategory {
        Core,
        ECS,
        Render,
        Audio,
        Physics,
        Asset,
        Scripting,
        Network,
        Input,
        UI,
        Tools
    };

    // Convert category to string
    const char* LogCategoryToString(LogCategory category);

    class Logger {
    public:
        // Initialize the logger system
        static void Init(const std::string& logFilePath = "lupine.log", bool enableFileLogging = true);

        // Shutdown the logger system
        static void Shutdown();

        // Get the logger instance
        static std::shared_ptr<spdlog::logger>& GetLogger();

        // Enable/disable file logging at runtime
        static void SetFileLogging(bool enabled);

        // Set log level
        static void SetLogLevel(spdlog::level::level_enum level);

        // Flush logs to file
        static void Flush();

    private:
        static std::shared_ptr<spdlog::logger> s_Logger;
        static bool s_FileLoggingEnabled;
        static std::string s_LogFilePath;
    };

} // namespace lupine

// Core logging macros - Always log to console
#define LOG_TRACE(category, ...)    ::lupine::Logger::GetLogger()->trace("[{}] {}", ::lupine::LogCategoryToString(category), fmt::format(__VA_ARGS__))
#define LOG_DEBUG(category, ...)    ::lupine::Logger::GetLogger()->debug("[{}] {}", ::lupine::LogCategoryToString(category), fmt::format(__VA_ARGS__))
#define LOG_INFO(category, ...)     ::lupine::Logger::GetLogger()->info("[{}] {}", ::lupine::LogCategoryToString(category), fmt::format(__VA_ARGS__))
#define LOG_WARN(category, ...)     ::lupine::Logger::GetLogger()->warn("[{}] {}", ::lupine::LogCategoryToString(category), fmt::format(__VA_ARGS__))
#define LOG_ERROR(category, ...)    ::lupine::Logger::GetLogger()->error("[{}] {}", ::lupine::LogCategoryToString(category), fmt::format(__VA_ARGS__))
#define LOG_FATAL(category, ...)    ::lupine::Logger::GetLogger()->critical("[{}] {}", ::lupine::LogCategoryToString(category), fmt::format(__VA_ARGS__))

// Category-specific convenience macros
// Core
#define LOG_CORE_TRACE(...)     LOG_TRACE(::lupine::LogCategory::Core, __VA_ARGS__)
#define LOG_CORE_DEBUG(...)     LOG_DEBUG(::lupine::LogCategory::Core, __VA_ARGS__)
#define LOG_CORE_INFO(...)      LOG_INFO(::lupine::LogCategory::Core, __VA_ARGS__)
#define LOG_CORE_WARN(...)      LOG_WARN(::lupine::LogCategory::Core, __VA_ARGS__)
#define LOG_CORE_ERROR(...)     LOG_ERROR(::lupine::LogCategory::Core, __VA_ARGS__)
#define LOG_CORE_FATAL(...)     LOG_FATAL(::lupine::LogCategory::Core, __VA_ARGS__)

// ECS
#define LOG_ECS_TRACE(...)      LOG_TRACE(::lupine::LogCategory::ECS, __VA_ARGS__)
#define LOG_ECS_DEBUG(...)      LOG_DEBUG(::lupine::LogCategory::ECS, __VA_ARGS__)
#define LOG_ECS_INFO(...)       LOG_INFO(::lupine::LogCategory::ECS, __VA_ARGS__)
#define LOG_ECS_WARN(...)       LOG_WARN(::lupine::LogCategory::ECS, __VA_ARGS__)
#define LOG_ECS_ERROR(...)      LOG_ERROR(::lupine::LogCategory::ECS, __VA_ARGS__)
#define LOG_ECS_FATAL(...)      LOG_FATAL(::lupine::LogCategory::ECS, __VA_ARGS__)

// Render
#define LOG_RENDER_TRACE(...)   LOG_TRACE(::lupine::LogCategory::Render, __VA_ARGS__)
#define LOG_RENDER_DEBUG(...)   LOG_DEBUG(::lupine::LogCategory::Render, __VA_ARGS__)
#define LOG_RENDER_INFO(...)    LOG_INFO(::lupine::LogCategory::Render, __VA_ARGS__)
#define LOG_RENDER_WARN(...)    LOG_WARN(::lupine::LogCategory::Render, __VA_ARGS__)
#define LOG_RENDER_ERROR(...)   LOG_ERROR(::lupine::LogCategory::Render, __VA_ARGS__)
#define LOG_RENDER_FATAL(...)   LOG_FATAL(::lupine::LogCategory::Render, __VA_ARGS__)

// Audio
#define LOG_AUDIO_TRACE(...)    LOG_TRACE(::lupine::LogCategory::Audio, __VA_ARGS__)
#define LOG_AUDIO_DEBUG(...)    LOG_DEBUG(::lupine::LogCategory::Audio, __VA_ARGS__)
#define LOG_AUDIO_INFO(...)     LOG_INFO(::lupine::LogCategory::Audio, __VA_ARGS__)
#define LOG_AUDIO_WARN(...)     LOG_WARN(::lupine::LogCategory::Audio, __VA_ARGS__)
#define LOG_AUDIO_ERROR(...)    LOG_ERROR(::lupine::LogCategory::Audio, __VA_ARGS__)
#define LOG_AUDIO_FATAL(...)    LOG_FATAL(::lupine::LogCategory::Audio, __VA_ARGS__)

// Physics
#define LOG_PHYSICS_TRACE(...)  LOG_TRACE(::lupine::LogCategory::Physics, __VA_ARGS__)
#define LOG_PHYSICS_DEBUG(...)  LOG_DEBUG(::lupine::LogCategory::Physics, __VA_ARGS__)
#define LOG_PHYSICS_INFO(...)   LOG_INFO(::lupine::LogCategory::Physics, __VA_ARGS__)
#define LOG_PHYSICS_WARN(...)   LOG_WARN(::lupine::LogCategory::Physics, __VA_ARGS__)
#define LOG_PHYSICS_ERROR(...)  LOG_ERROR(::lupine::LogCategory::Physics, __VA_ARGS__)
#define LOG_PHYSICS_FATAL(...)  LOG_FATAL(::lupine::LogCategory::Physics, __VA_ARGS__)

// Asset Loading
#define LOG_ASSET_TRACE(...)    LOG_TRACE(::lupine::LogCategory::Asset, __VA_ARGS__)
#define LOG_ASSET_DEBUG(...)    LOG_DEBUG(::lupine::LogCategory::Asset, __VA_ARGS__)
#define LOG_ASSET_INFO(...)     LOG_INFO(::lupine::LogCategory::Asset, __VA_ARGS__)
#define LOG_ASSET_WARN(...)     LOG_WARN(::lupine::LogCategory::Asset, __VA_ARGS__)
#define LOG_ASSET_ERROR(...)    LOG_ERROR(::lupine::LogCategory::Asset, __VA_ARGS__)
#define LOG_ASSET_FATAL(...)    LOG_FATAL(::lupine::LogCategory::Asset, __VA_ARGS__)

// Scripting
#define LOG_SCRIPT_TRACE(...)   LOG_TRACE(::lupine::LogCategory::Scripting, __VA_ARGS__)
#define LOG_SCRIPT_DEBUG(...)   LOG_DEBUG(::lupine::LogCategory::Scripting, __VA_ARGS__)
#define LOG_SCRIPT_INFO(...)    LOG_INFO(::lupine::LogCategory::Scripting, __VA_ARGS__)
#define LOG_SCRIPT_WARN(...)    LOG_WARN(::lupine::LogCategory::Scripting, __VA_ARGS__)
#define LOG_SCRIPT_ERROR(...)   LOG_ERROR(::lupine::LogCategory::Scripting, __VA_ARGS__)
#define LOG_SCRIPT_FATAL(...)   LOG_FATAL(::lupine::LogCategory::Scripting, __VA_ARGS__)

// Network
#define LOG_NETWORK_TRACE(...)  LOG_TRACE(::lupine::LogCategory::Network, __VA_ARGS__)
#define LOG_NETWORK_DEBUG(...)  LOG_DEBUG(::lupine::LogCategory::Network, __VA_ARGS__)
#define LOG_NETWORK_INFO(...)   LOG_INFO(::lupine::LogCategory::Network, __VA_ARGS__)
#define LOG_NETWORK_WARN(...)   LOG_WARN(::lupine::LogCategory::Network, __VA_ARGS__)
#define LOG_NETWORK_ERROR(...)  LOG_ERROR(::lupine::LogCategory::Network, __VA_ARGS__)
#define LOG_NETWORK_FATAL(...)  LOG_FATAL(::lupine::LogCategory::Network, __VA_ARGS__)

// Input
#define LOG_INPUT_TRACE(...)    LOG_TRACE(::lupine::LogCategory::Input, __VA_ARGS__)
#define LOG_INPUT_DEBUG(...)    LOG_DEBUG(::lupine::LogCategory::Input, __VA_ARGS__)
#define LOG_INPUT_INFO(...)     LOG_INFO(::lupine::LogCategory::Input, __VA_ARGS__)
#define LOG_INPUT_WARN(...)     LOG_WARN(::lupine::LogCategory::Input, __VA_ARGS__)
#define LOG_INPUT_ERROR(...)    LOG_ERROR(::lupine::LogCategory::Input, __VA_ARGS__)
#define LOG_INPUT_FATAL(...)    LOG_FATAL(::lupine::LogCategory::Input, __VA_ARGS__)

// UI
#define LOG_UI_TRACE(...)       LOG_TRACE(::lupine::LogCategory::UI, __VA_ARGS__)
#define LOG_UI_DEBUG(...)       LOG_DEBUG(::lupine::LogCategory::UI, __VA_ARGS__)
#define LOG_UI_INFO(...)        LOG_INFO(::lupine::LogCategory::UI, __VA_ARGS__)
#define LOG_UI_WARN(...)        LOG_WARN(::lupine::LogCategory::UI, __VA_ARGS__)
#define LOG_UI_ERROR(...)       LOG_ERROR(::lupine::LogCategory::UI, __VA_ARGS__)
#define LOG_UI_FATAL(...)       LOG_FATAL(::lupine::LogCategory::UI, __VA_ARGS__)

// Tools
#define LOG_TOOLS_TRACE(...)    LOG_TRACE(::lupine::LogCategory::Tools, __VA_ARGS__)
#define LOG_TOOLS_DEBUG(...)    LOG_DEBUG(::lupine::LogCategory::Tools, __VA_ARGS__)
#define LOG_TOOLS_INFO(...)     LOG_INFO(::lupine::LogCategory::Tools, __VA_ARGS__)
#define LOG_TOOLS_WARN(...)     LOG_WARN(::lupine::LogCategory::Tools, __VA_ARGS__)
#define LOG_TOOLS_ERROR(...)    LOG_ERROR(::lupine::LogCategory::Tools, __VA_ARGS__)
#define LOG_TOOLS_FATAL(...)    LOG_FATAL(::lupine::LogCategory::Tools, __VA_ARGS__)
