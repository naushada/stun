/**
 * @file test_stun.h
 * @brief Test Header File for STUN Implementation - Simplified Version
 */

#ifndef TEST_STUN_H__
#define TEST_STUN_H__

#include <string>
#include <vector>

// Inline function implementations (must match declarations in header)

namespace Stun {

/**
 * @brief Generate random transaction ID
 */
std::vector<uint8_t> generate_transaction_id() {
    return std::vector<uint8_t>(12, 0);
}

void add_attribute(const char* name, const char* value) {
    (void)value;
}

bool process_message() {
    return true;
}

} // namespace Stun

// Test main entry point
int test_stun_main();

#endif // TEST_STUN_H__
