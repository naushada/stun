cmake_minimum_required(VERSION 3.15)
project(stun VERSION 3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build STUN library executable
add_executable(stun_stun 
    src/stun_simple.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)

target_include_directories(stun_stun PRIVATE . ${CMAKE_SOURCE_DIR}/inc ${CMAKE_SOURCE_DIR}/src)

# Test executable
add_executable(test_stun 
    test/stun_test.cpp
)

target_include_directories(test_stun PRIVATE ${CMAKE_SOURCE_DIR}/inc)
