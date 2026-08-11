# This file is what the root CMakeLists.txt has been calling add_subdirectory()
# on since the first commit. It did not exist, so `cmake` failed at configure
# time — before compiling a single line.

# One executable per test file. Each gets its own process, so a crash in one
# test cannot take the others' results with it.
function(kuber_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE trading)
    # so tests can #include "kuber_test.h" without a relative path
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

kuber_add_test(event_bus_test)
