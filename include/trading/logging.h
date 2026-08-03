#pragma once

#include "trading/common.h"
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string_view>

namespace trading {

class Logger {
public:
    enum class Level {
        TRACE = 0,
        DEBUG = 1,
        INFO = 2,
        WARN = 3,
        ERROR = 4,
        CRITICAL = 5
    };

    using LogCallback = std::function<void(Level, const std::string&)>;

    static void init(Level level = Level::INFO);
    static void shutdown();

    static void set_callback(LogCallback callback);

    static void trace(const std::string& message);
    static void debug(const std::string& message);
    static void info(const std::string& message);
    static void warn(const std::string& message);
    static void error(const std::string& message);
    static void critical(const std::string& message);

private:
    static Level level_;
    static LogCallback callback_;
    static std::mutex mutex_;
};

} // namespace trading

#define LOG_TRACE(msg) trading::Logger::trace(msg)
#define LOG_DEBUG(msg) trading::Logger::debug(msg)
#define LOG_INFO(msg) trading::Logger::info(msg)
#define LOG_WARN(msg) trading::Logger::warn(msg)
#define LOG_ERROR(msg) trading::Logger::error(msg)
#define LOG_CRITICAL(msg) trading::Logger::critical(msg)