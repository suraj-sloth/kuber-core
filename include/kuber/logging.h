// logging.h — Control-plane logging.
//
// HOT-PATH RULE: never call this from the matching loop, the order book, or
// anything else on the data plane. Logging allocates, locks, and formats.
// The data plane gets integer counters instead. See CLAUDE.md.
//
// Two things make this cheaper than the version it replaces:
//   1. The level check happens in the MACRO, before the arguments are
//      evaluated or formatted. A disabled LOG_DEBUG costs one predictable
//      branch and does not build a string.
//   2. Formatting is std::format-based, so callers pass values, not a
//      pre-built std::string. The old API forced a heap allocation at every
//      call site just to get the message in.

#pragma once

#include <atomic>
#include <format>
#include <iosfwd>
#include <mutex>
#include <string_view>
#include <utility>

namespace kuber {

class Logger {
public:
    enum class Level : int {
        Trace = 0,
        Debug = 1,
        Info = 2,
        Warn = 3,
        Error = 4,
        Critical = 5,
        Off = 6,  // set as the threshold to silence everything
    };

    // `sink` must outlive every logging call. std::cerr is the usual choice.
    static void init(Level level, std::ostream& sink);
    static void shutdown();

    static void setLevel(Level level) noexcept;
    [[nodiscard]] static Level level() noexcept;

    // Prefer the LOG_* macros — they skip formatting when the level is off.
    template <typename... Args>
    static void log(Level lvl, std::string_view fmt, Args&&... args) {
        write(lvl, std::vformat(fmt, std::make_format_args(args...)));
    }

    static void write(Level lvl, std::string_view message);

private:
    // Declared here, DEFINED in logging.cpp — exactly once, in one translation
    // unit. That is the One Definition Rule. The previous header declared these
    // and never defined them anywhere, so anything touching Logger failed to link.
    static std::atomic<Level> level_;
    static std::ostream* sink_;
    static std::mutex mutex_;
};

}  // namespace kuber

// The `if` lives here, not inside log(), so that arguments to a disabled level
// are never even evaluated. do/while(false) makes the macro behave like a
// single statement, so `if (x) LOG_INFO(...); else ...` parses correctly.
#define KUBER_LOG(lvl, ...)                                 \
    do {                                                    \
        if ((lvl) >= ::kuber::Logger::level()) {           \
            ::kuber::Logger::log((lvl), __VA_ARGS__);      \
        }                                                   \
    } while (false)

#define LOG_TRACE(...) KUBER_LOG(::kuber::Logger::Level::Trace, __VA_ARGS__)
#define LOG_DEBUG(...) KUBER_LOG(::kuber::Logger::Level::Debug, __VA_ARGS__)
#define LOG_INFO(...) KUBER_LOG(::kuber::Logger::Level::Info, __VA_ARGS__)
#define LOG_WARN(...) KUBER_LOG(::kuber::Logger::Level::Warn, __VA_ARGS__)
#define LOG_ERROR(...) KUBER_LOG(::kuber::Logger::Level::Error, __VA_ARGS__)
#define LOG_CRITICAL(...) KUBER_LOG(::kuber::Logger::Level::Critical, __VA_ARGS__)
