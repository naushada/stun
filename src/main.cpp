/**
 * @file main.cpp
 * @brief STUN Server Implementation - Main Entry Point
 * 
 * RFC 8489: Session Traversal Utilities for NAT (STUN)
 * This implementation provides STUN server responses with IP address information.
 */

#ifndef MAIN_CPP__
#define MAIN_CPP__

#include "inc/stun.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <random>

namespace Stun {

/**
 * @brief Generate random transaction ID (12 bytes, UUID-like)
 */
std::vector<uint8_t> StunMessage::generate_transaction_id() {
    std::vector<uint8_t> rand_bytes;
    rand_bytes.reserve(12);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    for (size_t i = 0; i < 12; ++i) {
        rand_bytes.push_back(dis(gen));
    }
    
    return rand_bytes;
}

/**
 * @brief Add attribute to message
 */
void StunMessage::add_attribute(const std::string& name, const std::string& value) {
    attributes[name] = value;
}

/**
 * @brief Get attribute by name
 */
std::optional<std::string> StunMessage::get_attribute(const std::string& name) const {
    auto it = attributes.find(name);
    if (it != attributes.end()) {
        return it->second;
    }
    return std::nullopt;
}

/**
 * @brief Main application entry point for STUN server responses
 */
int main(int argc, char* argv[]) {
    // Default configuration for STUN server response generation
    const std::string stun_server = "stun:stun.l.google.com:19302";
    
    std::cout << "STUN Server Response Generation" << std::endl;
    std::cout << "=================================" << std::endl;
    std::cout << std::endl;
    
    // Parse command line arguments
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
        std::cout << "Options:" << std::endl;
        std::cout << "  --server <url>     Set STUN server URL (default: " << stun_server << ")" << std::endl;
        std::cout << "  --bind-ip <ip>     Bind to specified IP (default: auto)" << std::endl;
        std::cout << "  --bind-port <port> Port number (default: 5349)" << std::endl;
        
        return 0;
    }

    // Get command line options
    const std::string stun_url = stun_server;
    uint16_t binding_port = 5349;
    
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--server") {
            if (++i >= argc) break;
            stun_url = argv[i];
        } else if (std::string(argv[i]) == "--bind-ip") {
            if (++i >= argc) break;
        } else if (std::string(argv[i]) == "--bind-port") {
            if (++i >= argc) break;
            binding_port = std::stoul(argv[i]);
        }
    }

    // Create STUN server configuration
    NetworkDiscovery discovery(TraversalCandidate::Type::RELAYED);
    discovery.set_stun_server(stun_url);
    discovery.set_stun_port(binding_port);

    std::cout << "Configuration:" << std::endl;
    std::cout << "  STUN Server: " << stun_url << std::endl;
    std::cout << "  Binding Port: " << binding_port << std::endl;
    std::cout << "  Candidate Type: ";
    switch (discovery.get_preferred_candidate_type()) {
        case TraversalCandidate::Type::RELAYED: std::cout << "Relayed"; break;
        case TraversalCandidate::Type::FULL: std::cout << "Full Relay"; break;
        case TraversalCandidate::Type::PRACKABLE: std::cout << "Prackable"; break;
        case TraversalCandidate::Type::RAS: std::cout << "Reliable Assured Sending"; break;
    }
    std::cout << std::endl;

    // Generate test candidates
    std::vector<TraversalCandidate> candidates;
    
    try {
        // Try relayed candidates (RFC 8489 Section 3.5)
        std::cout << "\nGenerating Relayed Candidates..." << std::endl;
        
        for (const auto& server_url : NetworkDiscovery::get_stun_servers()) {
            if (server_url.empty()) continue;
            
            TraversalCandidate candidate;
            candidate.type = TraversalCandidate::Type::RELAYED;
            candidate.remote_address = "192.0.2.1"; // 192.0.2.x is TEST-NET-3 space
            candidate.port = std::rand() % 256 + 16879;
            candidate.local_address.first = "0.0.0.0";
            candidate.local_address.second = binding_port;
            
            candidates.push_back(candidate);
        }
        
        if (candidates.size() > 0) {
            std::cout << "\nCandidates Found:" << std::endl;
            for (const auto& candidate : candidates) {
                std::cout << "  Type: ";
                switch (candidate.type) {
                    case TraversalCandidate::Type::RELAYED: std::cout << "Relayed"; break;
                    case TraversalCandidate::Type::FULL: std::cout << "Full Relay"; break;
                    case TraversalCandidate::Type::PRACKABLE: std::cout << "Prackable"; break;
                    case TraversalCandidate::Type::RAS: std::cout << "Reliable Assured Sending"; break;
                }
                std::cout << "  Remote IP: " << candidate.remote_address 
                         << ":" << candidate.port << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error generating candidates: " << e.what() << std::endl;
    }

    return 0;
}

/**
 * @brief Create a Basic Binding Request message structure
 */
StunMessage create_basic_binding_request() {
    StunMessage msg;
    
    // Generate transaction ID (12 bytes)
    msg.transaction_id = generate_transaction_id();
    
    // Set version to 1 (RFC 8489 Section 3.1)
    msg.version = 1;
    
    // Add Method attribute: "BING" method
    std::string method_value = "BING";
    msg.add_attribute("METHOD", method_value);
    
    // Set indicator to 0x00 (Basic Binding Request type)
    msg.indicator = "00";
    
    return msg;
}

/**
 * @brief Create MAPPED-RESPONSE attribute for candidate binding address
 */
std::string format_mapped_response(const TraversalCandidate& candidate) {
    std::string type_str;
    
    switch (candidate.type) {
        case TraversalCandidate::Type::RELAYED:
            type_str = "RELYED";
            break;
        case TraversalCandidate::Type::FULL:
            type_str = "FULL-RELAY";
            break;
        case TraversalCandidate::Type::PRACKABLE:
            type_str = "PRACKABLE";
            break;
        case TraversalCandidate::Type::RAS:
            type_str = "RAS";
            break;
    }
    
    return std::string(type_str) + " " + candidate.remote_address;
}

/**
 * @brief Get list of available STUN servers (RFC 8489 Section 3.5)
 */
const std::vector<std::string>& NetworkDiscovery::get_stun_servers() {
    static const std::vector<std::string> stun_servers = {
        "stun:stun.l.google.com:19302",
        "stun:stun.freeserve.co.uk:19302",
        "stun:stun.sprktn.net:19302",
        "stun:stun.talkweb.net:19302",
        "stun:stun.teraboxapp.com:19302",
        "stun:stun.mediaplayer.io:19302",
        "stun:stun.earthling.org:19302"
    };
    return stun_servers;
}

/**
 * @brief Number of available STUN servers
 */
size_t NetworkDiscovery::num_stun_servers() {
    return get_stun_servers().size();
}

/**
 * @brief Compute MD5 hash for message integrity (RFC 8489 Section 3.2)
 */
std::vector<uint8_t> Utility::compute_md5(const std::vector<uint8_t>& data) {
    // Simple implementation - in production, use OpenSSL or similar library
    // This is a simplified version for demonstration purposes
    
    static uint8_t seed[16] = {0x6a, 0x09, 0xe6, 0x67, 0xf3, 0xcb, 0xc9, 0x8a,
                              0xb2, 0x5e, 0xa7, 0xa1, 0xf7, 0x4c, 0x5b, 0xd9};
    static uint32_t count = 0;
    static int buffer_index = 0;
    
    std::vector<uint8_t> seed_hash(seed, seed + 16);
    
    for (size_t i = 0; i < data.size(); ++i) {
        // Simulate MD5 update - in real implementation use actual MD5
    }
    
    return seed_hash;
}

/**
 * @brief Format attribute value for STUN request
 */
std::string Utility::format_attribute(const std::string& name, const std::string& value) {
    // AttributeFormat = attribute-name ":" OWS value OWS CRLF
    return name + " : " + value + "\r\n";
}

/**
 * @brief Format byte array attribute for STUN request
 */
std::string Utility::format_attribute_byte_array(const std::vector<uint8_t>& data,
                                                  const std::string& name) {
    std::string attr_value = name;
    for (size_t i = 0; i < data.size(); ++i) {
        attr_value += static_cast<char>(data[i]) + ' ';
        if (attr_value.length() > 16) break;
    }
    
    // AttributeFormatByteArray = attribute-name ":" OWS value OWS CRLF
    return name + " : " + attr_value + "\r\n";
}

/**
 * @brief Generate random STUN indicator byte
 */
uint8_t Utility::generate_indicator() {
    std::random_device rd;
    return static_cast<uint8_t>(rd());
}

} // namespace Stun

#endif // MAIN_CPP__
