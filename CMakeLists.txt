cmake_minimum_required(VERSION 3.15)
project(stun VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(stun src/stun.cpp)
target_include_directories(stun PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)

add_executable(stun-client src/main.cpp)
target_link_libraries(stun-client PRIVATE stun)

add_executable(stun-test test/stun_test.cpp)
target_link_libraries(stun-test PRIVATE stun)

enable_testing()
add_test(NAME unit COMMAND stun-test)
