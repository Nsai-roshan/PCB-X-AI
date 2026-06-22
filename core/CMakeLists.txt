cmake_minimum_required(VERSION 3.20)
project(tracemind_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_Declare(
  eigen
  URL https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

set(EIGEN_BUILD_DOC       OFF CACHE BOOL "" FORCE)
set(EIGEN_BUILD_PKGCONFIG OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING         OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest json eigen)

enable_testing()

add_library(tracemind_model
  src/model/circuit.cpp
  src/placer/placer.cpp
  src/router/router.cpp
  src/irdrop/ir_solver.cpp
  src/thermal/thermal.cpp
  src/engine/engine.cpp
)
target_include_directories(tracemind_model PUBLIC src)
target_link_libraries(tracemind_model PUBLIC
  nlohmann_json::nlohmann_json
  Eigen3::Eigen
)

add_executable(tracemind_engine src/main.cpp)
target_link_libraries(tracemind_engine PRIVATE tracemind_model)

add_executable(circuit_parse_test tests/circuit_parse_test.cpp)
target_link_libraries(circuit_parse_test PRIVATE tracemind_model GTest::gtest_main)

add_executable(placer_test tests/placer_test.cpp)
target_link_libraries(placer_test PRIVATE tracemind_model GTest::gtest_main)

add_executable(router_test tests/router_test.cpp)
target_link_libraries(router_test PRIVATE tracemind_model GTest::gtest_main)

add_executable(engine_test tests/engine_test.cpp)
target_link_libraries(engine_test PRIVATE tracemind_model GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(circuit_parse_test)
gtest_discover_tests(placer_test)
gtest_discover_tests(router_test)
gtest_discover_tests(engine_test)
