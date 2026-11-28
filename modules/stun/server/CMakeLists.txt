# Server: Binding response + CLI.
add_library(stun_server src/stun_server.cpp)
target_include_directories(stun_server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)
target_link_libraries(stun_server PUBLIC stun_core)

add_executable(stun-server src/main.cpp)
target_link_libraries(stun-server PRIVATE stun_server stun_log framework)

add_executable(stun-server-test test/server_test.cpp)
target_link_libraries(stun-server-test PRIVATE stun_server)
add_test(NAME server COMMAND stun-server-test)
