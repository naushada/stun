/**
 * @file http_parser.h
 * @brief High-Performance HTTP Parser Header Definition
 * 
 * This header defines the high-performance O(n) HTTP request/response parser.
 * Originally based on NGINX's src/http/ngx_http_parse.c by Igor Sysoev.
 * Adapted for STUN server response parsing.
 */

#ifndef HTTP_PARSER_H__
#define HTTP_PARSER_H__

#include <string>
#include <map>
#include <cstdint>

/**
 * @namespace Http
 * @brief High-performance HTTP request/response parser implementation
 * 
 * Designed to parse O(n) time complexity for maximum efficiency in STUN servers.
 */
namespace Http {

/**
 * @struct ParseState
 * @brief Represents the current parsing state
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
    RES_HTT,            // Detected "H TT"
    RES_HTTP,           // Detected "HTTP"
    RES_FIRST_HTTP_MAJOR, // First digit of major version number
    RES_HTTP_MAJOR,     // After first major version digit
    RES_FIRST_HTTP_MINOR,// First digit of minor version number
    RES_HTTP_MINOR,     // After first minor version digit
    RES_FIRST_STATUS_CODE, // First digit of status code (HTTP response)
    RES_STATUS_CODE,    // After first status code digit
    RES_STATUS_START,   // First digit of human-readable status text
    RES_STATUS,         // Status text being parsed
    RES_LINE_ALMOST_DONE, // Detected "HTTP/1.1 " + status_code
    PRE_START_REQ,      // After 'R', detected Request
    START_REQ,          // After 'R' or 'HR', detected Request
    REQ_METHOD,         // After first request method letter
    REQ_SPACES_BEFORE_URL, // Between request method and URL
    REQ_SCHEMA,         // Starting schema part of URL
    REQ_SCHEMA_SLASH,   // Detected "://" in URL
    REQ_SCHEMA_SLASH_SLASH, // Detected ":///"
    REQ_SERVER_START,   // After "http://", server name start
    REQ_SERVER,         // Server name being parsed
    REQ_SERVER_WITH_AT, // Server with @ (userinfo)
    REQ_HOST_START,     // After userinfo or after http://
    REQ_HOST,           // Server host/hostname being parsed
    REQ_HOST_IPV6,      // IPv6 address start
    REQ_HOST_DONE,      // Server name complete
    REQ_PORT,           // Port number after server name
    REQ_PATH,           // URL path
    REQ_QUERY_STRING_START, // Query string start ('?')
    REQ_QUERY_STRING,   // Query string being parsed
    REQ_FRAGMENT_START,  // Fragment start ('#')
    REQ_FRAGMENT,       // Fragment being parsed
    REQ_HTTP_START,     // HTTP/1.1 after URL
    REQ_HTTP_H,         // Detected 'H' in "HTTP"
    REQ_HTTP_HT,        // Detected "HT"
    REQ_HTTP_HTT,       // Detected "H TT"
    REQ_HTTP_HTTP,      // Detected "HTTP"
    REQ_FIRST_HTTP_MAJOR,// First digit of HTTP version major
    REQ_HTTP_MAJOR,     // After first major version digit
    REQ_FIRST_HTTP_MINOR,// First digit of HTTP version minor
    REQ_HTTP_MINOR,     // After first minor version digit
    HEADER_FIELD_START,  // After request line complete
    HEADER_FIELD,       // Header field being parsed
    HEADER_VALUE_START,  // Before header value
    HEADER_VALUE,       // Header value being parsed
    HEADER_VALUE_LWS,   // Looking for LWS (Line Width Separator)
    HEADER_ALMOST_DONE, // Expected CRLF after headers complete
    CHUNK_SIZE_START,   // Chunk size start (chunked encoding)
    CHUNK_SIZE,         // Chunk size value being parsed
    CHUNK_PARAMETERS,   // Chunk parameters
    CHUNK_SIZE_ALMOST_DONE, // After chunk size value
    HEADERS_ALMOST_DONE,// After header line (CRLF expected)
    HEADERS_DONE        // Headers section complete
    
    /* Important: 'HEADERS_DONE' must be the last 'header' state. All states 
     * beyond this must be 'body' states. It is used for overflow checking. 
     */
};

/**
 * @enum HeaderStates
 * @brief States for header parsing
 */
enum class HeaderState {
    H_GENERAL = 0,              // General matching state
    H_GENERAL_AND_QUOTE,        // Quote-aware general matching
    H_GENERAL_AND_QUOTE_ESCAPE, // General with quotes and escapes
    H_MATCHING_CONTENT_LENGTH,   // Matching Content-Length header
    H_MATCHING_TRANSFER_ENCODING, // Matching Transfer-Encoding
    H_MATCHING_UPGRADE,          // Matching Upgrade
    H_CONTENT_LENGTH,            // Inside Content-Length value parsing
    H_TRANSFER_ENCODING,         // Inside Transfer-Encoding parsing
    H_UPGRADE,                   // Inside Upgrade header parsing
    H_MATCHING_TRANSFER_ENCODING_CHUNKED, // Matching chunked encoding
    H_TRANSFER_ENCODING_CHUNKED  // Inside chunked encoding parsing
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
    /**
     * @brief State machine for O(n) header parsing
     */
    enum class State : uint8_t {
        PRE_START_REQ_OR_RES = 0,
        START_REQ_OR_RES,
        RES_OR_RESP_H,
        PRE_START_RES,
        START_RES,
        RES_H,
        RES_HT,
        RES_HTT,
        RES_HTTP,
        RES_FIRST_HTTP_MAJOR,
        RES_HTTP_MAJOR,
        RES_FIRST_HTTP_MINOR,
        RES_HTTP_MINOR,
        RES_FIRST_STATUS_CODE,
        RES_STATUS_CODE,
        RES_STATUS_START,
        RES_STATUS,
        RES_LINE_ALMOST_DONE,
        PRE_START_REQ,
        START_REQ,
        REQ_METHOD,
        REQ_SPACES_BEFORE_URL,
        REQ_SCHEMA,
        REQ_SCHEMA_SLASH,
        REQ_SCHEMA_SLASH_SLASH,
        REQ_SERVER_START,
        REQ_SERVER,
        REQ_SERVER_WITH_AT,
        REQ_HOST_START,
        REQ_HOST,
        REQ_HOST_IPV6,
        REQ_HOST_DONE,
        REQ_PORT,
        REQ_PATH,
        REQ_QUERY_STRING_START,
        REQ_QUERY_STRING,
        REQ_FRAGMENT_START,
        REQ_FRAGMENT,
        REQ_HTTP_START,
        REQ_HTTP_H,
        REQ_HTTP_HT,
        REQ_HTTP_HTT,
        REQ_HTTP_HTTP,
        REQ_FIRST_HTTP_MAJOR,
        REQ_HTTP_MAJOR,
        REQ_FIRST_HTTP_MINOR,
        REQ_HTTP_MINOR,
        REQ_LINE_ALMOST_DONE,
        HEADER_FIELD_START,
        HEADER_FIELD,
        HEADER_VALUE_START,
        HEADER_VALUE,
        HEADER_VALUE_LWS,
        HEADER_ALMOST_DONE,
        CHUNK_SIZE_START,
        CHUNK_SIZE,
        CHUNK_PARAMETERS,
        CHUNK_SIZE_ALMOST_DONE,
        HEADERS_ALMOST_DONE,
        HEADERS_DONE
        
        /* Important: 'HEADERS_DONE' must be the last header state. All states 
         * beyond this must be body states. It is used for overflow checking.
         */
    };

    /**
     * @brief Parse headers with O(n) time complexity
     */
    const State current_state;

public:
    ParserImpl() : current_state(State::PRE_START_REQ_OR_RES) {}
    
    /**
     * @brief Parse complete message
     */
    bool parse(const std::string& input);
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
     * @brief Extract IP address from MAPPED-RESPONSE attribute (RFC 8489 Section 3.2)
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
