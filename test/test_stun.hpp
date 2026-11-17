/**
 * @file test_stun.hpp
 * @brief Test Header File for STUN Implementation
 */

#ifndef TEST_STUN_HPP__
#define TEST_STUN_HPP__

#include "inc/stun.h"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// ============================================================================
// Test Helper Functions
// ============================================================================

/**
 * @brief Print STUN configuration status
 */
inline void print_stun_config() {
    std::cout << "STUN Configuration:" << std::endl;
    std::cout << "  Number of servers: " << NetworkDiscovery::num_stun_servers() 
              << std::endl;
    for (const auto& server : NetworkDiscovery::get_stun_servers()) {
        std::cout << "  " << server << std::endl;
    }
}

/**
 * @brief Generate a sample Basic Binding Request
 */
inline StunMessage create_sample_request() {
    return Stun::create_basic_binding();
}

/**
 * @brief Parse MAPPED-RESPONSE attribute string
 */
inline Stun::TraversalCandidate parse_mapped_response(const std::string& attr) {
    Stun::StunMessage msg;
    msg.add_attribute("MAPPED-RESPONSE", attr);
    
    // Return dummy candidate for testing
    Stun::TraversalCandidate candidate;
    return candidate;
}

/**
 * @brief Format mapped response attribute value
 */
inline std::string format_mapped_response(const Stun::TraversalCandidate& candidate) {
    return Stun::format_mapped_response(candidate);
}

#endif // TEST_STUN_HPP__
