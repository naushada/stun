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

# Add STUN module
add_subdirectory(stun_src)
add_subdirectory(stun_lib)

# Installation rules (optional)
install(TARGETS stun_lib DESTINATION lib/stun)
install(FILES inc/stun.h inc/http_parser.h inc/ipvx.h DESTINATION include/stun)
