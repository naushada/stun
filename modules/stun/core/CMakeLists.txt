# Shared STUN message format: encode/decode and XOR-MAPPED-ADDRESS.
add_library(stun_core src/stun.cpp src/net.cpp)
target_include_directories(stun_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)
target_link_libraries(stun_core PRIVATE framework)

add_executable(stun-core-test test/core_test.cpp)
target_link_libraries(stun-core-test PRIVATE stun_core)
add_test(NAME core COMMAND stun-core-test)
