# Client: Binding request + CLI.
add_library(stun_client src/stun_client.cpp)
target_include_directories(stun_client PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)
target_link_libraries(stun_client PUBLIC stun_core)

add_executable(stun-client src/main.cpp)
target_link_libraries(stun-client PRIVATE stun_client stun_log)

add_executable(stun-client-test test/client_test.cpp)
target_link_libraries(stun-client-test PRIVATE stun_client)
add_test(NAME client COMMAND stun-client-test)
