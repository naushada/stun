// byte_order.h — big-endian (network byte order) encode/decode helpers shared
// across the STUN/TURN modules. Dependency-free.

#pragma once

#include <cstdint>
#include <vector>

namespace framework {

// Append a 16-bit value in big-endian order.
void put_u16(std::vector<uint8_t>& out, uint16_t v);

// Append a 32-bit value in big-endian order.
void put_u32(std::vector<uint8_t>& out, uint32_t v);

// Read a big-endian 16-bit value from a buffer (at least 2 readable bytes).
uint16_t get_u16(const uint8_t* p);

// Read a big-endian 32-bit value from a buffer (at least 4 readable bytes).
uint32_t get_u32(const uint8_t* p);

}  // namespace framework
