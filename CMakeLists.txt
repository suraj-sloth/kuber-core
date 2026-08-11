cmake_minimum_required(VERSION 3.20)
project(KuberCore VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Lets clangd and clang-tidy see exactly how each file is compiled.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(KUBER_BUILD_TESTS "Build tests" ON)
option(KUBER_BUILD_BENCH "Build benchmarks" OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ---------------------------------------------------------------------------
# Warnings, as a target rather than a directory property.
#
# The previous build did `target_compile_options(trading PRIVATE -Wall ...)`
# (the library was called `trading` before the rename to KuberCore).
# PRIVATE means "when compiling this library only" — so tests and future apps
# were built with NO warnings at all. An INTERFACE library carries no code; it
# exists purely to propagate flags to everything that links it.
# ---------------------------------------------------------------------------
add_library(kuber_warnings INTERFACE)
target_compile_options(kuber_warnings INTERFACE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -Wconversion        # int64 -> int32 narrowing, which tick/qty math invites
    -Wsign-conversion   # size_t <-> ptrdiff_t mixups
    -Wshadow
    -Wnon-virtual-dtor
    -Wcast-align
    -Woverloaded-virtual
    -Wdouble-promotion
)

# ---------------------------------------------------------------------------
# The library.
#
# Explicit source list, NOT file(GLOB). A glob runs at configure time, so adding
# order_book.cpp later would not trigger a re-configure — you would get an
# "undefined reference" at link time and no hint why.
# ---------------------------------------------------------------------------
add_library(kuber STATIC
    src/core/logging.cpp
)

target_include_directories(kuber PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(kuber PUBLIC cxx_std_20)
target_link_libraries(kuber PUBLIC kuber_warnings)

add_library(KuberCore::kuber ALIAS kuber)

if(KUBER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

install(TARGETS kuber ARCHIVE DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)

# NOTE: the old `docs` target ran `doxygen Doxyfile`. Neither doxygen nor a
# Doxyfile exists here, and hand-written docs/ is the real artifact, so it is
# deliberately gone rather than fixed.
