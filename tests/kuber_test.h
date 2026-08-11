// kuber_test.h — a ~90-line test harness with no dependencies.
//
// Why hand-rolled instead of GoogleTest/Catch2/doctest:
//   * Day 1's goal is a green build in hours. Every dependency is a new way to
//     fail, and doctest in particular trips -Wpedantic -Werror unless included
//     as a SYSTEM header.
//   * Writing it teaches __FILE__/__LINE__, the # stringification operator, and
//     what a test framework actually is underneath.
//
// The macro NAMES (CHECK, CHECK_EQ, REQUIRE) are deliberately doctest's. If we
// outgrow this, dropping in doctest.h is a near-mechanical swap: the only real
// change is turning `void foo()` + RUN_TEST(foo) into TEST_CASE("foo").
//
// What this fixes versus the assert()-based tests it replaces:
//   1. assert() is compiled OUT when NDEBUG is defined. A Release build of the
//      old tests printed "All tests passed" while checking nothing at all.
//   2. The old main() returned 0 unconditionally, so ctest could never see a
//      failure. TEST_SUMMARY() returns a real exit code.

#pragma once

#include <iostream>
#include <ostream>
#include <string_view>

namespace trading::test {

inline int g_checks = 0;
inline int g_failures = 0;
inline int g_currentTestFailures = 0;

// Same C++20 `requires` machinery as the EventBus concepts, used here to ask
// "can this type be printed?" so CHECK_EQ can show actual values for ints and
// strings without failing to compile on types that have no operator<<.
template <typename T>
concept Streamable = requires(std::ostream& os, const T& value) { os << value; };

template <typename T>
void printValue(std::ostream& os, const T& value) {
    if constexpr (Streamable<T>) {
        os << value;
    } else {
        os << "<not streamable>";
    }
}

inline void recordFailure(const char* file, int line, std::string_view expression) {
    ++g_failures;
    ++g_currentTestFailures;
    std::cerr << "  FAIL " << file << ':' << line << "  " << expression << '\n';
}

}  // namespace trading::test

// `#expr` is the stringification operator: the preprocessor turns the token
// sequence into a string literal, so a failure can print the source text.
#define CHECK(expr)                                                       \
    do {                                                                  \
        ++::trading::test::g_checks;                                      \
        if (!(expr)) {                                                    \
            ::trading::test::recordFailure(__FILE__, __LINE__, #expr);    \
        }                                                                 \
    } while (false)

// Like CHECK but prints both operands, which is the whole reason to prefer it.
#define CHECK_EQ(a, b)                                                    \
    do {                                                                  \
        ++::trading::test::g_checks;                                      \
        const auto& kuberLhs_ = (a);                                      \
        const auto& kuberRhs_ = (b);                                      \
        if (!(kuberLhs_ == kuberRhs_)) {                                  \
            ++::trading::test::g_failures;                                \
            ++::trading::test::g_currentTestFailures;                     \
            std::cerr << "  FAIL " << __FILE__ << ':' << __LINE__         \
                      << "  " #a " == " #b "  (";                         \
            ::trading::test::printValue(std::cerr, kuberLhs_);            \
            std::cerr << " != ";                                          \
            ::trading::test::printValue(std::cerr, kuberRhs_);            \
            std::cerr << ")\n";                                           \
        }                                                                 \
    } while (false)

// Abandons the rest of the test on failure. Use when everything after would be
// meaningless or unsafe — e.g. a null handle you are about to dereference.
#define REQUIRE(expr)                                                     \
    do {                                                                  \
        ++::trading::test::g_checks;                                      \
        if (!(expr)) {                                                    \
            ::trading::test::recordFailure(__FILE__, __LINE__, #expr);    \
            return;                                                       \
        }                                                                 \
    } while (false)

#define RUN_TEST(fn)                                                      \
    do {                                                                  \
        ::trading::test::g_currentTestFailures = 0;                       \
        std::cout << "[ RUN  ] " #fn "\n";                                \
        fn();                                                             \
        if (::trading::test::g_currentTestFailures == 0) {                \
            std::cout << "[  OK  ] " #fn "\n";                            \
        } else {                                                          \
            std::cout << "[ FAIL ] " #fn " ("                             \
                      << ::trading::test::g_currentTestFailures           \
                      << " check(s) failed)\n";                           \
        }                                                                 \
    } while (false)

// Use as: int main() { RUN_TEST(a); RUN_TEST(b); return TEST_SUMMARY(); }
// The exit code is what makes ctest meaningful.
#define TEST_SUMMARY()                                                    \
    ((std::cout << "\n"                                                   \
                << ::trading::test::g_checks << " check(s), "             \
                << ::trading::test::g_failures << " failure(s)\n"),       \
     ::trading::test::g_failures == 0 ? 0 : 1)
