#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string_view>

#include "vk_sdk/Logging.hpp"

namespace vkc {
    std::string_view convertLogLevelToString(LogLevel level) {
        switch (level) {
            case vkc::LogLevel::TRACE: return "TRACE";
            case vkc::LogLevel::DEBUG: return "DEBUG";
            case vkc::LogLevel::INFO: return "INFO";
            case vkc::LogLevel::WARN: return "WARN";
            case vkc::LogLevel::ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }

    void defaultCallback(LogLevel level, std::string_view message) {
        auto point = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(point);
        auto tm = *std::localtime(&time);\

        std::cout << "[" << std::put_time(&tm, "%FT%T")
                  << "] [" << convertLogLevelToString(level)
                  << "] " << message << std::endl;
    }

    std::mutex CALLBACK_MUTEX;
    LoggingCallback CALLBACK = defaultCallback;

    void installLoggingCallback(LoggingCallback callback) {
        std::lock_guard lock(CALLBACK_MUTEX);
        CALLBACK = callback;
    }

    void log(LogLevel level, std::string_view message) {
        std::lock_guard lock(CALLBACK_MUTEX);
        CALLBACK(level, message);
    }
}