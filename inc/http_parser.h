/**
 * @file http_parser.h
 * @brief High-Performance HTTP Parser for STUN server responses
 */

#ifndef HTTP_PARSER_H__
#define HTTP_PARSER_H__

#include <string>
#include <map>
#include <cstdint>

namespace Http {

/**
 * @struct ParseState
 * @brief Represents the current parsing state during HTTP header parsing
 */
enum class ParseState {
    DEAD = 0,           // Parsing stopped due to error
    PRE_START_REQ_OR_RES, // First two bytes: "H" or "R" + digit
    START_REQ_OR_RES,   // After first byte of HTTP version
    RES_OR_RESP_H,      // Detected "H" (HTTP/1.1) or "R" (Request)
    PRE_START_RES,      // First two bytes are "HR"
    START_RES,          // First two bytes are "RR" (Response)
    RES_H,              // After 'H', detected HTTP version
    RES_HT,             // Detected "HT" (HTTP)
    RES_HTTP,           // Detected "HTTP"
    RES_FIRST_HTTP_MAJOR, // First digit of major version number
    RES_HTTP_MAJOR,     // After first major version digit
    HEADER_FIELD_START,  // After request line complete
    HEADERS_DONE        // Headers section complete
};

/**
 * @struct TraversalCandidate
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
    uint16_t binding_port = 0;
    int8_t relative_preference = -128;

    TraversalCandidate(const std::string& addr, uint16_t port, 
                       const std::pair<std::string, uint16_t>& local_addr,
                       Type type_ = Type::RELAYED);
};

/**
 * @class ParserImpl
 * @brief Implements the high-performance parser with O(n) complexity
 */
class ParserImpl {
public:
    /**
     * @brief Parse HTTP headers from STUN server response
     * 
     * @param input Raw byte stream of STUN response data
     * @param headers Output map to populate with parsed header key-value pairs
     * @return true on success, false on error
     */
    bool parse_http_header(const std::string& input, 
                          std::map<std::string, std::string>& headers);

private:
    enum class State : uint8_t {
        PRE_START_REQ_OR_RES = 0,
        START_REQ_OR_RES,
        RES_OR_RESP_H,
        RES_H,
        RES_HT,
        RES_HTTP,
        HEADERS_DONE
    };

    const State current_state;
};

/**
 * @namespace Http
 * @brief STUN server-specific HTTP parser utilities
 */
namespace Http {

/**
 * @class ParserImpl
 * @brief Specialized parser for STUN server responses
 */
class ParserImpl {
public:
    /**
     * @brief Parse the header section of a STUN response
     * 
     * This function extracts all attributes from the STUN server response,
     * including the MAPPED-RESPONSE attribute containing IP address information.
     * 
     * @param input Raw byte stream from STUN server
     * @param candidate Output TraversalCandidate struct to populate
     * @return true on success
     */
    bool parse_stun_response(const std::string& input,
                             TraversalCandidate& candidate);

private:
    /**
     * @brief Parse request line (METHOD URI HTTP/VERSION)
     */
    void parse_request_line(const std::string& in);

    /**
     * @brief Extract IP address from MAPPED-RESPONSE attribute
     */
    TraversalCandidate extract_mapped_response(const std::string& attribute);

    /**
     * @brief Parse header field name (before first colon)
     */
    void parse_header_field_name(const std::string& in);

    /**
     * @brief Parse header field value (after first colon)
     */
    void parse_header_value(const std::string& in);

    /**
     * @brief Validate MAPPING TYPE string in MAPPED-RESPONSE attribute
     */
    TraversalCandidate::Type validate_mapping_type(const std::string& type_str);
};

} // namespace Http

#endif // HTTP_PARSER_H__
