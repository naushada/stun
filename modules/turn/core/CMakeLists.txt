# TURN message codecs + allocation state (no sockets). Links stun_core.
add_library(turn_core src/turn.cpp)
target_include_directories(turn_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)
target_link_libraries(turn_core PUBLIC stun_core PRIVATE framework)

add_executable(turn-core-test test/turn_core_test.cpp)
target_link_libraries(turn-core-test PRIVATE turn_core)
add_test(NAME turn-core COMMAND turn-core-test)
