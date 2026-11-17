/**
 * @file http_parser.cc
 * @brief HTTP Request Parser Implementation for STUN Server Responses
 * 
 * This file contains implementations for parsing STUN responses that follow
 * RFC 8489 format, extracting IP addresses and binding information from server responses.
 * 
 * @author Robert Greiner
 * @version 3.0 based on RFC 8489
 */

#ifndef HTTP_PARSER_CC__
#define HTTP_PARSER_CC__

#include "http_parser.h"
#include <map>
#include <cstring>

namespace Http {

// Forward declaration for parser usage
class ParserImpl;

/**
 * @brief High-performance HTTP request/response parsing implementation
 * 
 * Based on NGINX's high-performance O(n) parser, adapted for STUN server responses.
 */
class Http {
public:
    explicit Http();
    explicit Http(const std::string& input);
    ~Http() override = default;

    /**
     * @brief Get the parsed URI name from HTTP request line
     */
    std::string get_uri_name() const { return m_uriName; }

    /**
     * @brief Set the URI name for tracking
     */
    void set_uri_name(std::string uriName) { m_uriName = uriName; }

    /**
     * @brief Get token-value pair from URL query string
     */
    std::string get_element(const std::string& key) {
        auto it = m_tokenMap.find(key);
        return (it != m_tokenMap.end()) ? it->second : std::string();
    }

    /**
     * @brief Get the header content string
     */
    std::string header() { return m_header; }

    /**
     * @brief Get body data from message
     */
    std::string body() { return m_body; }

    /**
     * @brief Parse STUN server response (RFC 8489 Section 3.2)
     * 
     * Extracts IP address and port from MAPPED-RESPONSE attribute
     */
    void parse_stun_response(const std::string& input,
                            TraversalCandidate& candidate);

    /**
     * @brief Parse MIME content-type header
     */
    void parse_mime_header(const std::string& in);

    /**
     * @brief Dump parser state for debugging
     */
    void dump() const;

private:
    std::unordered_map<std::string, std::string> m_tokenMap;
    std::string m_uriName;
    std::string m_header;
    std::string m_body;

    /**
     * @brief Extract Content-Type header value
     */
    std::string get_content_type(const std::string& header_lines);

    /**
     * @brief Extract Content-Length from headers (for STUN response body)
     */
    std::string get_content_length(const std::string& header_lines, size_t body_offset);

    /**
     * @brief Find query string in URL for parameter parsing
     */
    void parse_uri(const std::string& in);

    /**
     * @brief Parse query parameters from STUN response attributes
     */
    void parse_query_params(const std::string& query);

    /**
     * @brief Build query parameter string from map
     */
    std::string build_query_string();

    /**
     * @brief Get body length for STUN response parsing
     */
    std::string get_content_length(const std::string& in, size_t body_offset);

    /**
     * @brief Extract binding address from MAPPED-RESPONSE attribute (RFC 8489)
     * 
     * MAPPED-RESPONSE format: "MAPPING TYPE IPv6 Address" or "IPv4/Address"
     * Types: relayed, full-relay, prackable, ras
     */
    TraversalCandidate extract_mapped_response(const std::string& attribute);

    /**
     * @brief Trim whitespace from string (for header field values)
     */
    std::string trim_whitespace(const std::string& str);
};

/**
 * @brief Default constructor for Http parser
 */
Http::Http::Http() : m_uriName(), m_tokenMap(), m_header(), m_body() {}

/**
 * @brief Constructor that parses the header portion from input
 */
Http::Http(const std::string& input) {
    m_uriName.clear();
    m_tokenMap.clear();
    m_header = get_content_type(input); // Parse header first
    
    if (!m_header.empty()) {
        parse_uri(input);
        parse_mime_header(input);
    }
    m_body = get_content_length(input, m_header.length());
}

/**
 * @brief Parse URI from request line and query parameters
 */
void Http::parse_uri(const std::string& in) {
    std::vector<std::string> lines;
    size_t pos = 0;
    char delim = '\n'; // CR/LF equivalent
    
    while (pos < in.length()) {
        size_t next_pos = in.find(delim, pos);
        if (next_pos != std::string::npos) {
            lines.push_back(in.substr(pos, next_pos - pos));
            pos = next_pos + 1;
        } else {
            lines.push_back(in.substr(pos));
            break;
        }
    }

    if (lines.empty() || lines[0].empty()) return;

    // Parse first line: METHOD URI HTTP/VERSION
    std::string method_line = lines[0];
    
    // Find HTTP version to identify query string position
    size_t query_pos = 0;
    for (size_t i = 0; i < method_line.length(); ++i) {
        if (method_line[i] == '?') {
            query_pos = i + 1;
            break;
        }
    }

    // Extract base URI
    size_t url_end = std::string::npos;
    for (size_t i = query_pos; i < method_line.length(); ++i) {
        if (method_line[i] == '?') {
            url_end = i;
            break;
        }
    }

    m_uriName = (url_end != std::string::npos) ? method_line.substr(0, url_end) : method_line;

    // Parse query parameters starting from query position
    if (query_pos > 0 && !method_line.empty()) {
        parse_query_params(method_line.substr(query_pos));
    }
}

/**
 * @brief Parse query parameters from STUN attribute values
 */
void Http::parse_query_params(const std::string& query) {
    size_t pos = 0;
    bool in_value = false;
    std::string current_key;
    std::string current_value;

    while (pos < query.length()) {
        char c = query[pos];

        if (c == '&' && !in_value) {
            if (!current_key.empty()) {
                m_tokenMap[current_key] = current_value;
            }
            current_key.clear();
            current_value.clear();
            in_value = false;
            pos++;
        } else if (c == '=' && !in_value) {
            current_key.clear();
            in_value = true;
            pos++;
        } else if (!in_value) {
            current_key += c;
        } else {
            current_value += c;
        }
    }

    // Add last parameter
    if (!current_key.empty()) {
        m_tokenMap[current_key] = current_value;
    }
}

/**
 * @brief Parse MIME headers from the body portion of STUN response
 */
void Http::parse_mime_header(const std::string& in) {
    std::vector<std::string> lines;
    for (size_t i = 0; i < in.length(); ++i) {
        if (in[i] == '\n') {
            lines.push_back(in.substr(0, i));
            i--; // Skip the newline character
        } else {
            lines.back().push_back(in[i]);
        }
    }
    lines.push_back(in.substr(lines.size() - 1));

    std::unordered_map<std::string, std::string> header_map;
    
    for (const auto& line : lines) {
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = line.substr(0, colon_pos + 1); // Include colon
            std::string value = line.substr(colon_pos + 2);

            header_map[key] = value;
        }
    }

    m_header.clear();
    for (const auto& [key, value] : header_map) {
        if (!m_header.empty()) m_header += '\n';
        m_header.append(key + value);
    }

    // Build query parameter string
    std::string param_str;
    for (const auto& [key, value] : header_map) {
        if (!param_str.empty()) param_str += '&';
        param_str += key + '=' + value;
    }
    m_uriName += '?';
    m_uriName += param_str;
}

/**
 * @brief Print parser state for debugging
 */
void Http::dump() const {
    std::cerr << "URI Name: " << m_uriName << std::endl;
    for (const auto& [key, value] : m_tokenMap) {
        std::cerr << "  Parameter: " << key << " = " << value << std::endl;
    }
}

/**
 * @brief Extract Content-Type header from STUN response
 */
std::string Http::get_content_type(const std::string& header_lines) {
    for (size_t i = 0; i < header_lines.length(); ++i) {
        if (header_lines[i] == '\n') {
            // Look for Content-Type in remaining headers
            size_t pos = i + 1;
            while (pos < header_lines.length() && header_lines[pos] != '\n') {
                if (!std::isspace(header_lines[pos])) {
                    break;
                }
                ++pos;
            }

            std::string ct_line = header_lines.substr(pos);
            size_t colon_pos = ct_line.find(':');
            if (colon_pos != std::string::npos) {
                auto value_start = ct_line.find(' ');
                if (value_start == std::string::npos || !isalpha(value_start)) {
                    value_start = colon_pos + 2; // Skip ': '
                }
                
                size_t eq_pos = ct_line.find(',', value_start);
                std::string type_part = (eq_pos == std::string::npos) 
                                        ? ct_line.substr(value_start)
                                        : ct_line.substr(value_start, eq_pos - value_start);

                auto trimmed = type_part;
                while (!trimmed.empty() && std::isspace(trimmed[0])) {
                    trimmed = trimmed.substr(1);
                }

                // Return if matches JSON (case-insensitive check)
                auto lower_type = trimmed;
                for (auto& c : lower_type) {
                    c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
                }

                if (lower_type.find("application/json") != std::string::npos) {
                    return trim_whitespace(type_part);
                }
            }
            break;
        }
    }
    return ""; // Return empty if not found
}

/**
 * @brief Find Content-Length from headers for STUN response body
 */
std::string Http::get_content_length(const std::string& header_lines, size_t body_offset) {
    for (size_t i = 0; i < header_lines.length(); ++i) {
        if (header_lines[i] == '\n') {
            // Find Content-Length
            size_t cl_pos = body_offset + i;
            while ((cl_pos < header_lines.length()) && std::isspace(header_lines[cl_pos])) {
                ++cl_pos;
            }

            auto content_length_line = header_lines.substr(cl_pos);
            if (content_length_line.length() > 8 && 
                content_length_line.find("Content-Length:") == 0) {
                // Extract numeric value from "Content-Length: 123"
                size_t colon_pos = content_length_line.find(':');
                if (colon_pos != std::string::npos) {
                    auto value_str = content_length_line.substr(colon_pos + 2);
                    while (!value_str.empty() && std::isspace(value_str[0])) {
                        ++value_str;
                    }

                    uint64_t length_value;
                    try {
                        size_t pos_end;
                        length_value = static_cast<uint64_t>(std::stoull(value_str, &pos_end));
                        if (pos_end == value_str.length()) {
                            return std::to_string(static_cast<std::string>(length_value)) + "\n";
                        }
                    } catch (...) {
                        // Return empty string on conversion error
                    }
                }
            }
        }
    }
    return "";
}

/**
 * @brief Parse STUN mapped response attribute (RFC 8489 Section 3.2)
 */
TraversalCandidate Http::extract_mapped_response(const std::string& attribute) {
    TraversalCandidate candidate;
    
    // Parse MAPPED-RESPONSE attribute format: "TYPE ADDRESS"
    size_t space_pos = attribute.find(' ');
    if (space_pos != std::string::npos) {
        std::string type_str = attribute.substr(0, space_pos);
        std::string address_str = attribute.substr(space_pos + 1);
        
        // Convert type string to TraversalCandidate::Type enum
        if (type_str == "RELYED") {
            candidate.type = TraversalCandidate::Type::RELAYED;
        } else if (type_str == "FULL-RELAY") {
            candidate.type = TraversalCandidate::Type::FULL;
        } else if (type_str == "PRACKABLE") {
            candidate.type = TraversalCandidate::Type::PRACKABLE;
        } else if (type_str == "RAS") {
            candidate.type = TraversalCandidate::Type::RAS;
        }
        
        candidate.remote_address = address_str;
    }
    
    return candidate;
}

/**
 * @brief Trim whitespace from string
 */
std::string Http::trim_whitespace(const std::string& str) {
    size_t start = 0;
    while (start < str.length() && std::isspace(static_cast<unsigned char>(str[start]))) {
        ++start;
    }

    size_t end = str.length();
    while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1]))) {
        --end;
    }

    return str.substr(start, end - start);
}

} // namespace Http

#endif // HTTP_PARSER_CC__
