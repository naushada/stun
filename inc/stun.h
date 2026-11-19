/**
 * @file stun.h
 * @brief Session Traversal Utilities for NAT (STUN) Implementation - Header File
 * @version 3.0 based on RFC 8489
 */

#ifndef STUN_H__
#define STUN_H__

#include <string>
#include <vector>
#include <optional>
#include <map>
#include <cstdint>
#include <algorithm>
#include <random>

namespace Stun {

/**
 * @struct StunMessage
 * @brief Represents a single STUN message (Request or Response)
 */
struct StunMessage {
    std::string indicator;
    std::map<std::string, std::string> attributes;
    uint8_t version = 1;
    uint8_t transaction_id[12];
    bool integrity_valid = false;
    std::vector<uint8_t> message_body;

    StunMessage();
    ~StunMessage() override {};

    static std::vector<uint8_t> generate_transaction_id();
    void add_attribute(const std::string& name, const std::string& value);
    std::optional<std::string> get_attribute(const std::string& name) const;
};

/**
 * @class TraversalCandidate
 * @brief Represents a candidate binding address from STUN server
 */
class TraversalCandidate {
public:
    enum class Type : uint8_t {
        RELAYED = 0x01,      // Relay candidate (requires TURN)
        FULL = 0x02,         // Full relay address
        PRACKABLE = 0x03,    // Prackable candidate
        RAS = 0x04           // Reliable Assured Sending candidate
    };

    TraversalCandidate() : type(Type::RELAYED) {}
    ~TraversalCandidate() override {};

    Type type = Type::RELAYED;
    std::string remote_address;
    uint16_t port = 0;
    std::pair<std::string, uint16_t> local_address;
    std::optional<NetworkType> network_type;
    uint16_t binding_port = 0;
    int8_t relative_preference = -128;

    TraversalCandidate(const std::string& addr, uint16_t port, 
                       const std::pair<std::string, uint16_t>& local_addr,
                       Type type_ = Type::RELAYED);
};

enum class NetworkType {
    NAT = 0x01,
    PUBLIC = 0x02,
    DIRECT = 0x03
};

/**
 * @namespace Utility
 * @brief STUN message construction and parsing utilities
 */
namespace Utility {
    std::vector<uint8_t> compute_md5(const std::vector<uint8_t>& data);
    std::string format_attribute(const std::string& name, const std::string& value);
    uint8_t generate_indicator();
}

/**
 * @class Transport
 * @brief Handles STUN message transport and integrity checking
 */
class Transport {
public:
    enum class Direction { SEND = 0, RECEIVE = 1 };
    Transport(TraversalCandidate::Type candidate_type = TraversalCandidate::Type::RELAYED);
    ~Transport() override {};
    uint8_t send_message(const StunMessage& msg, const std::string& bind_addr, 
                       const TraversalCandidate::Type preferred_type);
    bool parse_message(const std::string& raw_data);
};

/**
 * @namespace Binding
 * @brief Helper functions for creating binding requests
 */
namespace Binding {
    std::vector<uint8_t> create_basic_binding_request();
    std::vector<uint8_t> create_mapped_response_binding_request(const TraversalCandidate& candidate);
}

/**
 * @namespace Message
 * @brief STUN message generation and handling functions
 */
namespace Message {
    StunMessage create_basic_binding();
    StunMessage create_mapped_response_binding(const TraversalCandidate& candidate);
}

/**
 * @namespace CandidateGeneration
 * @brief STUN candidate generation utilities
 */
namespace CandidateGeneration {
    std::string generate_full_relay_address(const StunMessage& msg, 
                                           const TraversalCandidate& local_candidate);
}

/**
 * @class Session
 * @brief Manages STUN sessions and candidate ordering
 */
class Session {
public:
    using OrderedList = std::vector<TraversalCandidate>;
    ~Session() override {};
    void set_stun_server(const std::string& url);
    OrderedList get_candidates();
    bool process_candidate(const TraversalCandidate& candidate);
};

/**
 * @namespace Reliability
 * @brief Reliability protocols for STUN (RFC 8489 Section 12)
 */
namespace Reliability {
    std::vector<uint8_t> create_prack_request(const StunMessage& request, 
                                            const TraversalCandidate::Type candidate_type);
}

/**
 * @namespace Statistics
 * @brief Track and collect statistics for STUN operations
 */
namespace Statistics {
    void increment_stat(const std::string& stat_name, size_t value);
}

/**
 * @namespace MessageConstruction
 * @brief Helper functions for building STUN messages (RFC 8489)
 */
namespace MessageConstruction {
    std::vector<uint8_t> compute_md5(const std::vector<uint8_t>& data);
}

/**
 * @namespace BindingRequest
 * @brief Build binding request messages (RFC 8489 Section 3)
 */
namespace BindingRequest {
    std::vector<uint8_t> create_basic_binding_request();
}

/**
 * @namespace MappingType
 * @brief STUN mapping type utilities (RFC 8489 Section 3.2)
 */
namespace MappingType {
    std::string format_mapped_response(const TraversalCandidate& candidate);
}

#endif // STUN_H__
