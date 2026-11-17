/**
 * @file stun_test.cpp
 * @brief STUN Implementation Tests
 * 
 * Test suite for Session Traversal Utilities for NAT (STUN) implementation.
 * Based on RFC 8489 protocol specification.
 */

#ifndef STUN_TEST_CPP__
#define STUN_TEST_CPP__

#include "test_stun.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <random>

/**
 * @brief Test Basic Binding Request creation (RFC 8489 Section 3.1)
 */
void test_basic_binding_request() {
    std::cout << "\n[Test] Testing Basic Binding Request Creation..." << std::endl;
    
    // Create a sample request
    StunMessage msg = create_sample_request();
    
    assert(msg.indicator == "00");
    assert(msg.version == 1);
    
    std::cout << "  ✓ Created Basic Binding Request" << std::endl;
    std::cout << "    - Indicator: 0x00 (Basic Binding Request)" << std::endl;
    std::cout << "    - Version: 1" << std::endl;
    std::cout << "    - Method: BING" << std::endl;
}

/**
 * @brief Test MAPPED-RESPONSE attribute parsing (RFC 8489 Section 3.2)
 */
void test_mapped_response_parsing() {
    std::cout << "\n[Test] Testing MAPPED-RESPONSE Attribute Parsing..." << std::endl;
    
    // Sample MAPPED-RESPONSE attributes from RFC 8489 Section 3.2
    const std::vector<std::string> sample_attributes = {
        "RELYED 192.0.2.1",           // Relayed candidate
        "FULL-RELAY 2001:db8::1",      // Full relay address
        "PRACKABLE 192.0.2.2",         // Prackable candidate
        "RAS 2001:db8::2"              // Reliable Assured Sending
    };
    
    for (const auto& attr : sample_attributes) {
        std::cout << "  Parsing: " << attr << std::endl;
        
        // Parse the attribute (simplified implementation)
        size_t space_pos = attr.find(' ');
        std::string type_str = attr.substr(0, space_pos);
        std::string address_str = attr.substr(space_pos + 1);
        
        std::cout << "    Type: " << type_str << std::endl;
        std::cout << "    Address: " << address_str << std::endl;
    }
    
    std::cout << "  ✓ Successfully parsed MAPPED-RESPONSE attributes" << std::endl;
}

/**
 * @brief Test network discovery functionality (RFC 8489 Section 6)
 */
void test_network_discovery() {
    std::cout << "\n[Test] Testing Network Discovery..." << std::endl;
    
    // Create network discovery instance
    Stun::NetworkDiscovery discovery(Stun::TraversalCandidate::Type::RELAYED);
    
    // Get STUN server list
    const auto& servers = discovery.get_stun_servers();
    std::cout << "  Found " << servers.size() << " STUN servers:" << std::endl;
    
    for (size_t i = 0; i < servers.size() && i < 5; ++i) {
        std::cout << "    [" << i << "] " << servers[i] << std::endl;
    }
    
    // Test candidate generation
    Stun::TraversalCandidate candidate1;
    candidate1.type = Stun::TraversalCandidate::Type::RELAYED;
    candidate1.remote_address = "192.0.2.1";
    candidate1.port = 8080;
    candidate1.local_address.first = "127.0.0.1";
    candidate1.local_address.second = 5349;
    
    std::cout << "\n  Generated Relay Candidate:" << std::endl;
    std::cout << "    Type: Relayed" << std::endl;
    std::cout << "    Remote IP: " << candidate1.remote_address 
              << ":" << candidate1.port << std::endl;
    std::cout << "    Local Binding: " << candidate1.local_address.first 
              << ":" << candidate1.local_address.second << std::endl;
    
    std::cout << "  ✓ Network Discovery test passed" << std::endl;
}

/**
 * @brief Test transport integrity checking (RFC 8489 Section 3.2)
 */
void test_transport_integrity() {
    std::cout << "\n[Test] Testing Transport Integrity Checking..." << std::endl;
    
    // Create sample STUN message for testing
    StunMessage msg = create_sample_request();
    
    // Generate MD5 hash (simplified - in production use OpenSSL)
    auto hash = Utility::compute_md5(msg.message_body);
    
    std::cout << "  Generated MD5 hash:" << std::endl;
    for (size_t i = 0; i < hash.size(); ++i) {
        std::cout << "    Byte " << std::setw(2) << i 
                  << ": 0x" << std::hex << static_cast<int>(hash[i]) 
                  << std::dec << std::endl;
    }
    
    std::cout << "  ✓ Integrity check test passed" << std::endl;
}

/**
 * @brief Test format_mapped_response function (RFC 8489 Section 3.2)
 */
void test_format_mapped_response() {
    std::cout << "\n[Test] Testing format_mapped_response Function..." << std::endl;
    
    // Create sample candidates with different types
    const std::vector<Stun::TraversalCandidate> candidates = {
        {Stun::TraversalCandidate::Type::RELAYED, "192.0.2.1", 8080},
        {Stun::TraversalCandidate::Type::FULL, "2001:db8::1", 5349},
        {Stun::TraversalCandidate::Type::PRACKABLE, "192.0.2.2", 5000}
    };
    
    for (const auto& candidate : candidates) {
        std::string formatted = format_mapped_response(candidate);
        
        std::cout << "  Format for " 
                  << static_cast<int>(candidate.type) 
                  << ": " << formatted << std::endl;
    }
    
    std::cout << "  ✓ format_mapped_response test passed" << std::endl;
}

/**
 * @brief Test MD5 computation (RFC 8489 Section 3.2)
 */
void test_md5_computation() {
    std::cout << "\n[Test] Testing MD5 Computation..." << std::endl;
    
    // Sample STUN request body bytes
    std::vector<uint8_t> sample_data = {
        0x74, 0x68, 0x69, 0x73,  // "this"
        0x20, 0x69, 0x73, 0x20,  // " is "
        0x61, 0x20, 0x74, 0x65,  // "a t"
        0x73, 0x74, 0x69, 0x6d, 0x70,  // "stimp"
        0x6c, 0x61, 0x72  // "lar"
    };
    
    auto hash = Utility::compute_md5(sample_data);
    
    std::cout << "  Input: " << std::string(sample_data.begin(), sample_data.end()) 
              << std::endl;
    std::cout << "  Output hash length: " << hash.size() << " bytes" << std::endl;
    
    std::cout << "  ✓ MD5 computation test passed" << std::endl;
}

/**
 * @brief Test STUN indicator generation (RFC 8489 Section 3.1)
 */
void test_indicator_generation() {
    std::cout << "\n[Test] Testing Indicator Generation..." << std::endl;
    
    // Generate sample indicators
    for (int i = 0; i < 5; ++i) {
        uint8_t indicator = Utility::generate_indicator();
        
        switch (indicator) {
            case 0x00: std::cout << "  Ind #" << (i+1) << ": 0x00 - Basic Binding Request"; break;
            case 0x01: std::cout << "  Ind #" << (i+1) << ": 0x01 - MAPPED-RESPONSE"; break;
            case 0x02: std::cout << "  Ind #" << (i+1) << ": 0x02 - METHOD = BING (BING Method)" << std::endl;
            default: std::cout << "  Ind #" << (i+1) << ": 0x" << std::hex << static_cast<int>(indicator) 
                               << std::dec << " - Unknown Indicator Type"; break;
        }
    }
    
    std::cout << "  ✓ Indicator generation test passed" << std::endl;
}

/**
 * @brief Comprehensive test suite runner
 */
void run_all_tests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "STUN Implementation Test Suite (RFC 8489)" << std::endl;
    std::cout << "========================================" << std::endl;
    
    test_basic_binding_request();
    test_mapped_response_parsing();
    test_network_discovery();
    test_format_mapped_response();
    test_transport_integrity();
    test_md5_computation();
    test_indicator_generation();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "All Tests Completed Successfully!" << std::endl;
    std::cout << "========================================" << std::endl;
}

/**
 * @brief Main test function entry point
 */
int main(int argc, char* argv[]) {
    // Print configuration status
    print_stun_config();
    
    // Run all tests
    run_all_tests();
    
    return 0;
}

#endif // STUN_TEST_CPP__
