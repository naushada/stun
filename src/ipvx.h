/**
 * @file ipvx.h
 * @brief IP Version Agnostic Wrapper for Network Address Translation (NAT) Discovery
 * 
 * This header provides version-agnostic access to both IPv4 and IPv6 address
 * handling functionality, making it easier to write network-transparent code.
 */

#ifndef IPVX_H__
#define IPVX_H__

#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace Stun {

/**
 * @struct AddressType
 * @brief Represents different IP address variants (IPv4, IPv6)
 */
enum class AddressType {
    NONE = 0,      // No address information
    V4,            // IPv4 address
    V6,            // IPv6 address
    BOTH           // Both IPv4 and IPv6 available
};

/**
 * @class NetworkAddress
 * @brief Unified network address wrapper supporting both IPv4 and IPv6
 */
class NetworkAddress {
public:
    /**
     * @brief Address type (IPv4, IPv6, or both)
     */
    AddressType type = AddressType::NONE;

    /**
     * @brief IPv4 address as string "192.0.2.x"
     */
    std::string v4_str = "0.0.0.0";

    /**
     * @brief IPv6 address as string "::ffff:192.0.2.1"
     */
    std::string v6_str = "::ffff:0.0.0.0";

    /**
     * @brief Convert IPv4 to raw bytes
     */
    uint8_t v4_bytes[4];

    /**
     * @brief Convert IPv6 to raw bytes (16 bytes)
     */
    uint8_t v6_bytes[16];

    NetworkAddress() = default;
    ~NetworkAddress() override = default;

    /**
     * @brief Construct from IPv4 string
     */
    NetworkAddress(const std::string& v4_str);

    /**
     * @brief Construct from IPv6 string
     */
    NetworkAddress(const std::string& v6_str);

    /**
     * @brief Get address as text representation
     */
    std::string to_string() const;

    /**
     * @brief Check if this is an IPv4 address
     */
    bool is_v4() const { return type == AddressType::V4; }

    /**
     * @brief Check if this is an IPv6 address
     */
    bool is_v6() const { return type == AddressType::V6; }

    /**
     * @brief Get port number
     */
    uint16_t port = 0;

};

/**
 * @namespace Network
 * @brief Utility functions for network address manipulation
 */
namespace Network {

    /**
     * @brief Parse IPv4 address string to raw bytes
     * 
     * Example: "192.0.2.1" -> uint8_t v4_bytes[4] = {192, 0, 2, 1}
     * 
     * @param str Input IP address as string
     * @return Raw byte array of size 4
     */
    std::vector<uint8_t> parse_ipv4(const std::string& str);

    /**
     * @brief Parse IPv6 address string to raw bytes
     * 
     * Supports abbreviated IPv6 notation (colons for compression)
     * 
     * Example: "2001:db8::1" -> 16-byte array with compressed form
     * 
     * @param str Input IPv6 address as string
     * @return Raw byte array of size 16
     */
    std::vector<uint8_t> parse_ipv6(const std::string& str);

    /**
     * @brief Convert raw bytes to IPv4 string
     */
    std::string format_ipv4(const uint8_t* bytes, size_t len = 4);

    /**
     * @brief Convert raw bytes to IPv6 string (full expansion)
     */
    std::string format_ipv6(const uint8_t* bytes, size_t len = 16);

    /**
     * @brief Determine if string represents a valid IPv4 address
     */
    bool is_valid_ipv4(const std::string& str);

    /**
     * @brief Determine if string represents a valid IPv6 address
     */
    bool is_valid_ipv6(const std::string& str);

    /**
     * @brief Check if string contains valid IP address (either v4 or v6)
     */
    bool is_valid_ip(const std::string& str);

} // namespace Network

/**
 * @namespace AddressParsing
 * @brief Functions for parsing and formatting network addresses
 */
namespace AddressParsing {

    /**
     * @brief Convert string to byte array with type detection
     * 
     * Automatically detects whether input is IPv4 or IPv6 format.
     * 
     * @param str Input address as string
     * @return Pair of (address_type, bytes)
     */
    std::pair<NetworkAddress::AddressType, std::vector<uint8_t>>
    string_to_bytes(const std::string& str);

    /**
     * @brief Convert byte array to string with type detection
     */
    std::string bytes_to_string(const uint8_t* bytes, NetworkAddress::AddressType type);

} // namespace AddressParsing

/**
 * @namespace TraversalDiscovery
 * @brief Functions for STUN traversal discovery (RFC 8489 Section 6)
 */
namespace TraversalDiscovery {

    /**
     * @brief Get the number of STUN servers configured for discovery
     * 
     * Returns count of available STUN server endpoints.
     * 
     * @return Number of STUN server addresses
     */
    uint32_t get_num_stun_servers();

    /**
     * @brief Iterate over all configured STUN servers
     */
    const std::vector<std::string>& get_stun_servers();

} // namespace TraversalDiscovery

/**
 * @namespace Utility
 * @brief STUN utility functions for address conversion and manipulation
 */
namespace Utility {

    /**
     * @brief Convert string to IP address with automatic type detection
     * 
     * Attempts IPv4 first, then falls back to IPv6 if v4 fails.
     * 
     * @param str Input address string
     * @return NetworkAddress with parsed data (or empty if invalid)
     */
    std::optional<NetworkAddress> parse_address(const std::string& str);

} // namespace Utility

#endif // IPVX_H__
