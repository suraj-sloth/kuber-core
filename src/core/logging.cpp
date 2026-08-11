#include "kuber/logging.h"

#include <iostream>
#include <ostream>

namespace kuber {

// ---------------------------------------------------------------------------
// The One Definition Rule, in practice.
//
// logging.h DECLARES these three static members — it says "these exist
// somewhere." A declaration reserves the name; it does not create storage.
// Exactly one translation unit must DEFINE them, which is what these three
// lines do. Zero definitions gives "undefined reference" at link time; two
// gives "multiple definition".
//
// This is also why this file exists at all right now: CMake's add_library()
// needs at least one source file, and this is the first real one.
// ---------------------------------------------------------------------------
std::atomic<Logger::Level> Logger::level_{Logger::Level::Info};
std::ostream* Logger::sink_ = nullptr;
std::mutex Logger::mutex_;

namespace {

constexpr std::string_view levelName(Logger::Level lvl) noexcept {
    switch (lvl) {
        case Logger::Level::Trace:
            return "TRACE";
        case Logger::Level::Debug:
            return "DEBUG";
        case Logger::Level::Info:
            return "INFO ";
        case Logger::Level::Warn:
            return "WARN ";
        case Logger::Level::Error:
            return "ERROR";
        case Logger::Level::Critical:
            return "CRIT ";
        case Logger::Level::Off:
            return "OFF  ";
    }
    // Unreachable for any valid enumerator, but -Werror wants every path to
    // return. Note there is no `default:` — that is deliberate. Without one,
    // -Wswitch turns "you added an enumerator and forgot to handle it" into a
    // compile error. We use that trick again for the market state machine.
    return "?????";
}

}  // namespace

void Logger::init(Level level, std::ostream& sink) {
    const std::lock_guard<std::mutex> guard(mutex_);
    level_.store(level, std::memory_order_relaxed);
    sink_ = &sink;
}

void Logger::shutdown() {
    const std::lock_guard<std::mutex> guard(mutex_);
    if (sink_ != nullptr) {
        sink_->flush();
    }
    sink_ = nullptr;
    level_.store(Level::Off, std::memory_order_relaxed);
}

void Logger::setLevel(Level level) noexcept {
    // relaxed is enough: we only need the read to eventually see the write.
    // No other memory is being published alongside it, so there is nothing to
    // order against. Stage 5 covers what acquire/release would add here.
    level_.store(level, std::memory_order_relaxed);
}

Logger::Level Logger::level() noexcept {
    return level_.load(std::memory_order_relaxed);
}

void Logger::write(Level lvl, std::string_view message) {
    const std::lock_guard<std::mutex> guard(mutex_);
    std::ostream* out = (sink_ != nullptr) ? sink_ : &std::cerr;
    *out << '[' << levelName(lvl) << "] " << message << '\n';
}

}  // namespace kuber
