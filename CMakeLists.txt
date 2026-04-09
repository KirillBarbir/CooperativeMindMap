cmake_minimum_required(VERSION 3.20)
project(CoopMindMap LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BOOST_ROOT "C:/gitprojects/boost_1_90_0")
set(BOOST_LIBRARYDIR "${BOOST_ROOT}/stage/lib")
set(Boost_USE_STATIC_LIBS ON)

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
link_directories("${BOOST_LIBRARYDIR}")

add_library(mind_map_network STATIC
        src/network/server.cpp
        src/network/http_session.cpp
        src/network/websocket_session.cpp
)
target_include_directories(mind_map_network PUBLIC include "${BOOST_ROOT}")
target_link_libraries(mind_map_network PUBLIC mind_map_core)
target_compile_definitions(mind_map_network PUBLIC BOOST_JSON_HEADER_ONLY)

include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(mind_map_server src/main.cpp)
target_link_libraries(mind_map_server PRIVATE mind_map_network)

enable_testing()

add_executable(mind_map_tests
        tests/access_rules_test.cpp
        tests/revision_test.cpp
        tests/mind_map_service_test.cpp
        tests/concurrency_test.cpp
        tests/network_test.cpp
)
target_link_libraries(mind_map_tests PRIVATE mind_map_network GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(mind_map_tests)
