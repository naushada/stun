# Leveled logging library with syslog semantics.
add_library(stun_log STATIC src/log.cpp)
target_include_directories(stun_log PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)

# TLS for the HTTP sink (HTTPS).
find_package(OpenSSL REQUIRED)
target_link_libraries(stun_log PUBLIC OpenSSL::SSL OpenSSL::Crypto)

add_executable(stun-log-test test/log_test.cpp)
target_link_libraries(stun-log-test PRIVATE stun_log)
add_test(NAME log COMMAND stun-log-test)
