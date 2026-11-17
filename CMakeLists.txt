cmake_minimum_required(VERSION 3.15)
project(stun VERSION 3.0 LANGUAGES CXX)

# Set C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Add source files (actual path structure)
add_executable(stun_stun ../src/main.cpp 
              ../src/stun.cpp
              ../src/http_parser.cc
              ../inc/stun.h)

target_include_directories(stun_stun PRIVATE . ../inc)

add_executable(test_stun ../test/stun_test.cpp 
                 ../test/test_stun.hpp
                 ../src/main.cpp
                 ../inc/stun.h
                 ../inc/ipvx.h
                 ../inc/http_parser.h)

target_include_directories(test_stun PRIVATE . ../inc)

# Installation rules (optional)
install(TARGETS stun_stun RUNTIME DESTINATION bin/stun)
install(TARGETS test_stun RUNTIME DESTINATION bin/test)
