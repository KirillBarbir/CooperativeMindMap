cmake_minimum_required(VERSION 3.20)
project(CoopMindMap LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BOOST_ROOT "C:/gitprojects/boost_1_90_0")

add_library(mind_map_core STATIC
        src/utils/id/uuid.cpp
        src/validation/access_rules.cpp
        src/services/mind_map_service.cpp
        src/services/space_service.cpp
        src/services/node_service.cpp
        src/services/edge_service.cpp
        src/services/comment_service.cpp
        src/services/presence_service.cpp
)
target_include_directories(mind_map_core PUBLIC include "${BOOST_ROOT}")

include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(mind_map_tests
        tests/access_rules_test.cpp
        tests/revision_test.cpp
        tests/mind_map_service_test.cpp
        tests/concurrency_test.cpp
)
target_link_libraries(mind_map_tests PRIVATE mind_map_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(mind_map_tests)
