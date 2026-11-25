cmake_minimum_required(VERSION 3.15)
project(stun VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(STUN_MODULE ${CMAKE_CURRENT_SOURCE_DIR}/modules/stun)

# --- core: shared message format (encode/decode, XOR-MAPPED-ADDRESS) ---
add_library(stun_core ${STUN_MODULE}/core/src/stun.cpp)
target_include_directories(stun_core PUBLIC ${STUN_MODULE}/core/inc)

# --- client: binding request + CLI ---
add_library(stun_client ${STUN_MODULE}/client/src/stun_client.cpp)
target_include_directories(stun_client PUBLIC ${STUN_MODULE}/client/inc)
target_link_libraries(stun_client PUBLIC stun_core)

add_executable(stun-client ${STUN_MODULE}/client/src/main.cpp)
target_link_libraries(stun-client PRIVATE stun_client)

# --- server: binding response + CLI ---
add_library(stun_server ${STUN_MODULE}/server/src/stun_server.cpp)
target_include_directories(stun_server PUBLIC ${STUN_MODULE}/server/inc)
target_link_libraries(stun_server PUBLIC stun_core)

add_executable(stun-server ${STUN_MODULE}/server/src/main.cpp)
target_link_libraries(stun-server PRIVATE stun_server)

# --- tests ---
enable_testing()

add_executable(stun-core-test ${STUN_MODULE}/core/test/core_test.cpp)
target_link_libraries(stun-core-test PRIVATE stun_core)
add_test(NAME core COMMAND stun-core-test)

add_executable(stun-client-test ${STUN_MODULE}/client/test/client_test.cpp)
target_link_libraries(stun-client-test PRIVATE stun_client)
add_test(NAME client COMMAND stun-client-test)

add_executable(stun-server-test ${STUN_MODULE}/server/test/server_test.cpp)
target_link_libraries(stun-server-test PRIVATE stun_server)
add_test(NAME server COMMAND stun-server-test)
