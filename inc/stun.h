/**
 * @file stun.h
 * @brief Session Traversal Utilities for NAT (STUN) Implementation
 * @version 3.0 based on RFC 8489
 * 
 * RFC 8489: STUN: A Protocol for Unified Communications to Signaling,
 *      Interoperability and Network Address Translation Discovery
 */

#ifndef STUN_H__
#define STUN_H__

#include <string>
#include <vector>
#include <optional>

namespace Stun {

/**
 * @struct StunMessage
 * @brief Represents a single STUN message (Request or Response)
 * 
 * STUN messages use simple text-based formats with MD5 hash for integrity.
 */
struct StunMessage {
    enum class MessageType {
        INDICATOR = 0,      // STUN Indicator byte
        METHOD = 1,         // METHOD attribute
        RELATION = 2,       // RELATION attribute
        MAPPED = 3          // MAPPED-RESPONSE attribute
    };

    std::string indicator;   // First byte of message (0x00 or 0xff)
    std::map<std::string, std::string> attributes;
    uint8_t version = 1;     // Message version
    uint8_t transaction_id[12]; // Randomly generated transaction ID
    bool integrity_valid = false;
    std::vector<uint8_t> message_body; // MD5 hash for integrity

    StunMessage();
    ~StunMessage() override = default;

    /**
     * @brief Generate a random transaction ID (12-byte UUID-like)
     * 
     * Returns a random 12-byte array to identify this STUN transaction.
     */
    static std::vector<uint8_t> generate_transaction_id();
    
    /**
     * @brief Add an attribute to the message
     */
    void add_attribute(const std::string& name, const std::string& value);

    /**
     * @brief Get a specific attribute by name
     */
    std::optional<std::string> get_attribute(const std::string& name) const;
};

/**
 * @class TraversalCandidate
 * @brief Represents a candidate binding address from STUN server
 * 
 * This struct represents the remote IP address returned by a STUN server,
 * including potential NAT traversal information.
 */
class TraversalCandidate {
public:
    /**
     * @brief Candidate type enumeration
     */
    enum class Type : uint8_t {
        RELAYED = 0x01,      // Relay candidate (requires TURN)
        FULL = 0x02,         // Full relay address
        PRACKABLE = 0x03,    // Prackable candidate
        RAS = 0x04           // Reliable Assured Sending candidate
    };

    TraversalCandidate() : type(Type::RELAYED) {}
    ~TraversalCandidate() override = default;

    /**
     * @brief Candidate type (relay, full-relay, prackable, ras)
     */
    Type type = Type::RELAYED;
    
    /**
     * @brief IP address (IPv4 or IPv6 as string)
     */
    std::string remote_address;

    /**
     * @brief Port number for the remote binding address
     */
    uint16_t port = 0;

    /**
     * @brief Local bind address and port used with STUN server
     */
    std::pair<std::string, uint16_t> local_address;

    /**
     * @brief Network type (NAT, public, or direct connection)
     */
    enum class NetworkType {
        NAT = 0x01,           // Behind a NAT device
        PUBLIC = 0x02,       // Directly connected to network
        DIRECT = 0x03        // Direct connection (not behind NAT)
    } network_type = NetworkType::NAT;

    /**
     * @brief Port used for this candidate
     */
    uint16_t binding_port = 0;

    /**
     * @brief Relative preference value
     */
    int8_t relative_preference = -128;

    /**
     * @brief Candidate flags bitfield
     */
    uint8_t flags = 0;

    TraversalCandidate(const std::string& addr, uint16_t port, 
                       const std::pair<std::string, uint16_t>& local_addr,
                       Type type_ = Type::RELAYED);
};

/**
 * @class NetworkDiscovery
 * @brief Manages the discovery of network topology via STUN servers
 */
class NetworkDiscovery {
public:
    using CandidateList = std::vector<TraversalCandidate>;

    /**
     * @brief Supported STUN server URLs (RFC 8489 Section 3.5)
     */
    static const std::vector<std::string>& get_stun_servers();

    /**
     * @brief Get the number of configured STUN servers
     */
    static size_t num_stun_servers();

    /**
     * @brief Create a new discovery context for the given network type
     */
    NetworkDiscovery(TraversalCandidate::Type preferred_candidate_type = 
                    TraversalCandidate::Type::RELAYED);

    ~NetworkDiscovery() override = default;

    /**
     * @brief Set the STUN server to use
     */
    void set_stun_server(const std::string& url);

    /**
     * @brief Query all available STUN servers for candidates
     */
    std::vector<TraversalCandidate> query_candidates(
        const std::pair<std::string, uint16_t>& bind_addr,
        const TraversalCandidate::Type preferred_type);

    /**
     * @brief Get the currently discovered candidates
     */
    CandidateList get_discovered_candidates();

    /**
     * @brief Set the port to use for STUN requests
     */
    void set_stun_port(uint16_t port);
};

/**
 * @namespace Utility
 * @brief STUN message construction and parsing utilities
 */
namespace Utility {

    /**
     * @brief Generate MD5 hash for message integrity (RFC 8489 Section 3.2)
     * 
     * Computes the MD5 hash of all bytes preceding this point in the stream.
     * Returns a 16-byte hash.
     */
    std::vector<uint8_t> compute_md5(const std::vector<uint8_t>& data);

    /**
     * @brief Format an attribute as text (RFC 8489 Section 3.2)
     * 
     * AttributeFormat = attribute-name ":" OWS value OWS CRLF
     */
    std::string format_attribute(const std::string& name, 
                                 const std::string& value);

    /**
     * @brief Format an attribute with a specific byte sequence (RFC 8489 Section 3.2)
     */
    std::string format_attribute_byte_array(const std::vector<uint8_t>& data,
                                            const std::string& name);

    /**
     * @brief Generate a random STUN indicator byte
     */
    uint8_t generate_indicator();

} // namespace Utility

/**
 * @class Transport
 * @brief Handles STUN message transport and integrity checking
 */
class Transport {
public:
    enum class Direction { SEND = 0, RECEIVE = 1 };

    /**
     * @brief Create a new transport instance
     */
    Transport(TraversalCandidate::Type candidate_type = 
                 TraversalCandidate::Type::RELAYED);

    ~Transport() override = default;

    /**
     * @brief Send a STUN message to a binding address, set up with integrity
     */
    uint8_t send_message(const StunMessage& msg,
                         const std::string& bind_addr,
                         const TraversalCandidate::Type preferred_type);

    /**
     * @brief Parse and validate incoming STUN response
     */
    bool parse_message(const std::string& raw_data);
};

/**
 * @namespace Binding
 * @brief Helper functions for creating binding requests
 */
namespace Binding {

    /**
     * @brief Create a Basic Binding Request (RFC 8489 Section 3.1)
     * 
     * This request allows the server to learn about the local bind address.
     */
    std::vector<uint8_t> create_basic_binding_request();

    /**
     * @brief Create a MAPPED-RESPONSE Binding Request (RFC 8489 Section 3.2)
     */
    std::vector<uint8_t> create_mapped_response_binding_request(
        const TraversalCandidate& candidate);

} // namespace Binding

/**
 * @namespace Message
 * @brief STUN message generation and handling functions
 */
namespace Message {

    /**
     * @brief Create a Basic Binding Request using the Transport class
     */
    StunMessage create_basic_binding();

    /**
     * @brief Create a MAPPED-RESPONSE Binding Request
     */
    StunMessage create_mapped_response_binding(const TraversalCandidate& candidate);

} // namespace Message

/**
 * @namespace CandidateGeneration
 * @brief STUN candidate generation utilities
 */
namespace CandidateGeneration {

    /**
     * @brief Generate full relay address from server response
     */
    std::string generate_full_relay_address(const StunMessage& msg,
                                            const TraversalCandidate& local_candidate);

} // namespace CandidateGeneration

/**
 * @class Session
 * @brief Manages STUN sessions and candidate ordering
 */
class Session {
public:
    /**
     * @brief Ordered list of discovered candidates (RFC 8489 Section 6)
     */
    using OrderedList = std::vector<TraversalCandidate>;

    ~Session() override = default;

    /**
     * @brief Set the STUN server to use
     */
    void set_stun_server(const std::string& url);

    /**
     * @brief Get all discovered candidates with relative ordering
     */
    OrderedList get_candidates();

    /**
     * @brief Process incoming candidate from STUN response
     */
    bool process_candidate(const TraversalCandidate& candidate);
};

/**
 * @namespace Reliability
 * @brief Reliability protocols for STUN (RFC 8489 Section 12)
 */
namespace Reliability {

    /**
     * @brief Send a Prack Request using the RELATION attribute
     */
    std::vector<uint8_t> create_prack_request(
        const StunMessage& request,
        const TraversalCandidate::Type candidate_type);

} // namespace Reliability

/**
 * @namespace Statistics
 * @brief Track and collect statistics for STUN operations
 */
namespace Statistics {

    /**
     * @brief Increment statistics counter for a specific operation
     */
    void increment_stat(const std::string& stat_name, size_t value);

} // namespace Statistics

/**
 * @namespace MessageConstruction
 * @brief Helper functions for building STUN messages (RFC 8489)
 */
namespace MessageConstruction {

    /**
     * @brief Compute MD5 hash for integrity checking (Section 3.2)
     */
    std::vector<uint8_t> compute_md5(const std::vector<uint8_t>& data);

} // namespace MessageConstruction

/**
 * @namespace BindingRequest
 * @brief Build binding request messages
 */
namespace BindingRequest {

    /**
     * @brief Create Basic Binding Request (RFC 8489 Section 3.1)
     */
    std::vector<uint8_t> create_basic_binding_request();

} // namespace BindingRequest

/**
 * @namespace MappingType
 * @brief STUN mapping type utilities
 */
namespace MappingType {

    /**
     * @brief Format MAPPED-RESPONSE attribute value (RFC 8489 Section 3.2)
     */
    std::string format_mapped_response(const TraversalCandidate& candidate);

} // namespace MappingType

/**
 * @name Candidate Ordering Operations
 * @brief Functionality for ordering discovered candidates
 */
#define DEFINE_ORDERING_OPERATION(name, priority)\
    inline const int name = (priority)

/**
 * @section Naming Convention
 */
enum class Reliability {
    /** 
     * Priority 128: Prackable Candidates (preferred due to reliability)
     */
    PRACKABLE = 128,
    
    /** 
     * Priority 1024: Relayed Addresses (requires TURN)
     */
    RELAYED = 1024,

    /** 
     * Priority 2048: Full Binding addresses
     */
    FULL_BINDING = 2048,

    /** 
     * Priority 4096: Reliable Assured Sending
     */
    RAS = 4096,

    /** 
     * Default priority (when no explicit ordering)
     */
    DEFAULT = 0
};

#endif // STUN_H__
