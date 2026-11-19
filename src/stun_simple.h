/**
 * @file stun_simple.h
 * @brief STUN Implementation Header - Simplified Inline Functions
 */

#ifndef STUN_SIMPLE_H__
#define STUN_SIMPLE_H__

#include <string>
#include <map>
#include <cstdint>

namespace Stun {

struct SimpleMessage {
    uint8_t type;
    char content[32];
    uint16_t length;
};

enum class Type : uint8_t {
    RELAYED = 0x01,
    FULL = 0x02,
    PRACKABLE = 0x03,
    RAS = 0x04
};

std::vector<uint8_t> generate_transaction_id();
void add_attribute(const char* name, const char* value);
bool process_message();
SimpleMessage create_basic_binding();
char* format_mapped_response(uint8_t type_val);

} // namespace Stun

#endif // STUN_SIMPLE_H__
