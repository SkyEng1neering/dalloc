cmake_minimum_required(VERSION 3.14)
project(dalloc_tests)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Compiler flags
add_compile_options(-Wall -Wextra -g)

# Sanitizers (optional)
option(USE_SANITIZERS "Enable ASan/UBSan" ON)
if(USE_SANITIZERS AND NOT WIN32)
    add_compile_options(-fsanitize=address,undefined)
    add_link_options(-fsanitize=address,undefined)
endif()

# dalloc source (compiled as C)
add_library(dalloc STATIC ../src/dalloc.c)
target_include_directories(dalloc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../inc
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(dalloc PUBLIC
    DALLOC_TEST_BUILD
    USE_SINGLE_HEAP_MEMORY
    DALLOC_CUSTOM_CONF_FILE="dalloc_test_conf.h"
)

# Test sources
set(TEST_SOURCES
    test_basic.cpp
    test_defrag.cpp
    test_edge_cases.cpp
    test_realloc.cpp
    test_stress.cpp
    test_alignment.cpp
    test_single_heap.cpp
)

# Test executable
add_executable(dalloc_tests ${TEST_SOURCES})
target_link_libraries(dalloc_tests
    dalloc
    GTest::gtest_main
)
target_include_directories(dalloc_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../inc
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Enable CTest integration
include(GoogleTest)
gtest_discover_tests(dalloc_tests)

# ==================== Thread Safety Tests ====================

# Find pthreads
find_package(Threads)

if(Threads_FOUND AND NOT WIN32)
    # dalloc with thread safety enabled (compiled as C)
    add_library(dalloc_threadsafe STATIC ../src/dalloc.c)
    target_include_directories(dalloc_threadsafe PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../inc
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_compile_definitions(dalloc_threadsafe PUBLIC
        DALLOC_TEST_BUILD
        USE_SINGLE_HEAP_MEMORY
        DALLOC_CUSTOM_CONF_FILE="dalloc_pthread_conf.h"
    )
    target_link_libraries(dalloc_threadsafe PUBLIC Threads::Threads)

    # Thread safety test executable
    add_executable(dalloc_thread_tests test_thread_safety.cpp)
    target_link_libraries(dalloc_thread_tests
        dalloc_threadsafe
        GTest::gtest_main
        Threads::Threads
    )
    target_include_directories(dalloc_thread_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../inc
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    gtest_discover_tests(dalloc_thread_tests)
endif()
