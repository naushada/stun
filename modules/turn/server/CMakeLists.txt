# TURN relay server: response builders + UDP relay engine.
add_library(turn_server src/turn_server.cpp)
target_include_directories(turn_server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)
target_link_libraries(turn_server PUBLIC turn_core framework PRIVATE stun_log)

add_executable(turn-server src/main.cpp)
target_link_libraries(turn-server PRIVATE turn_server stun_log)

add_executable(turn-server-test test/turn_server_test.cpp)
target_link_libraries(turn-server-test PRIVATE turn_server)
add_test(NAME turn-server COMMAND turn-server-test)
