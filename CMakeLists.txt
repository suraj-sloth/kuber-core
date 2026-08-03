cmake_minimum_required(VERSION 3.15)
project(TradingSystemsLab VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(BUILD_TESTS "Build tests" ON)
option(BUILD_EXAMPLES "Build examples" OFF)
option(ENABLE_PERF "Enable performance measurements" OFF)

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Include core infrastructure
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Find source files
file(GLOB_RECURSE SRC_FILES "src/*.cpp" "src/*.cc")
file(GLOB_RECURSE HDR_FILES "include/*.h" "include/*.hpp")

# Create library
add_library(trading ${SRC_FILES})

# Target properties
target_compile_features(trading PUBLIC cxx_std_20)
target_compile_options(trading PRIVATE -Wall -Wextra -Wpedantic -Werror)

if(ENABLE_PERF)
    target_compile_options(trading PRIVATE -O3 -march=native -flto)
    target_link_options(trading PRIVATE -flto)
endif()

# Install targets
install(TARGETS trading
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include)

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Documentation
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/docs)
    add_custom_target(docs
        COMMAND doxygen Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generate documentation"
    )
endif()