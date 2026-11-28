# Framework: shared low-level utilities (byte order, event loop) + libevent.
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBEVENT REQUIRED IMPORTED_TARGET libevent)

add_library(framework src/byte_order.cpp src/event_loop.cpp)
target_include_directories(framework PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)
target_link_libraries(framework PUBLIC PkgConfig::LIBEVENT)
