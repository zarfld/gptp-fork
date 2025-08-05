/**
 * @file gptp_message_parser.hpp
 * @brief IEEE 802.1AS gPTP message parsing and validation
 * 
 * This file provides functions for parsing and validating gPTP messages
 * according to IEEE 802.1AS-2021 specification.
 */

#pragma once

#include "gptp_types.hpp"
#include "gptp_protocol.hpp"
#include <vector>
#include <cstring>
#include <cstdio>
#include <string>
#include <stdexcept>

// Cross-platform packed struct attribute
#ifdef _MSC_VER
    #pragma pack(push, 1)
    #define PACKED_MSG
#else
    #define PACKED_MSG __attribute__((packed))
#endif

// Use the optional implementation from gptp_types.hpp

namespace gptp {

    /**
     * @brief Result of message parsing operation
     */
    enum class ParseResult {
        SUCCESS,
        INVALID_LENGTH,
        INVALID_ETHERTYPE,
        INVALID_VERSION,
        INVALID_DOMAIN,
        INVALID_MESSAGE_TYPE,
        CHECKSUM_ERROR,
        UNKNOWN_ERROR
    };

    /**
     * @brief Ethernet frame header for gPTP
     */
    struct EthernetFrame {
        std::array<uint8_t, 6> destination;
        std::array<uint8_t, 6> source;
        uint16_t etherType;
        
        EthernetFrame() : etherType(protocol::GPTP_ETHERTYPE) {
            destination = protocol::GPTP_MULTICAST_MAC;
            std::fill(source.begin(), source.end(), 0);
        }
    } PACKED_MSG;

    /**
     * @brief Complete gPTP packet (Ethernet + gPTP message)
     */
    struct GptpPacket {
        EthernetFrame ethernet;
        std::vector<uint8_t> payload;
        
        GptpPacket() = default;
        
        // Get total packet size
        size_t size() const {
            return sizeof(EthernetFrame) + payload.size();
        }
        
        // Set source MAC address
        void set_source_mac(const std::array<uint8_t, 6>& mac) {
            ethernet.source = mac;
        }
        
        // Get source MAC as string
        std::string get_source_mac_string() const {
            char buffer[18];
            snprintf(buffer, sizeof(buffer), "%02x:%02x:%02x:%02x:%02x:%02x",
                ethernet.source[0], ethernet.source[1], ethernet.source[2],
                ethernet.source[3], ethernet.source[4], ethernet.source[5]);
            return std::string(buffer);
        }
    };

    /**
     * @brief gPTP Message Parser
     */
    class MessageParser {
    public:
        /**
         * @brief Parse raw packet data into gPTP message
         * @param data Raw packet data
         * @param length Length of packet data
         * @return ParseResult indicating success or error type
         */
        static ParseResult parse_packet(const uint8_t* data, size_t length, GptpPacket& packet);
        
        /**
         * @brief Validate gPTP message header
         * @param header Message header to validate
         * @return ParseResult indicating validation result
         */
        static ParseResult validate_header(const GptpMessageHeader& header);
        
        /**
         * @brief Extract message type from packet
         * @param data Raw packet data (must include Ethernet header)
         * @param length Length of packet data
         * @return Message type if valid, nullopt otherwise
         */
        static GPTP_OPTIONAL<protocol::MessageType> get_message_type(const uint8_t* data, size_t length);
        
        /**
         * @brief Parse Sync message
         * @param data Payload data (after Ethernet header)
         * @param length Length of payload
         * @param sync_msg Output sync message
         * @return ParseResult indicating success or error
         */
        static ParseResult parse_sync_message(const uint8_t* data, size_t length, SyncMessage& sync_msg);
        
        /**
         * @brief Parse Follow_Up message
         * @param data Payload data (after Ethernet header)
         * @param length Length of payload
         * @param followup_msg Output follow-up message
         * @return ParseResult indicating success or error
         */
        static ParseResult parse_followup_message(const uint8_t* data, size_t length, FollowUpMessage& followup_msg);
        
        /**
         * @brief Parse Pdelay_Req message
         * @param data Payload data (after Ethernet header)
         * @param length Length of payload
         * @param pdelay_req_msg Output pdelay request message
         * @return ParseResult indicating success or error
         */
        static ParseResult parse_pdelay_req_message(const uint8_t* data, size_t length, PdelayReqMessage& pdelay_req_msg);
        
        /**
         * @brief Parse Pdelay_Resp message
         * @param data Payload data (after Ethernet header)
         * @param length Length of payload
         * @param pdelay_resp_msg Output pdelay response message
         * @return ParseResult indicating success or error
         */
        static ParseResult parse_pdelay_resp_message(const uint8_t* data, size_t length, PdelayRespMessage& pdelay_resp_msg);
        
        /**
         * @brief Parse Announce message
         * @param data Payload data (after Ethernet header)
         * @param length Length of payload
         * @param announce_msg Output announce message
         * @return ParseResult indicating success or error
         */
        static ParseResult parse_announce_message(const uint8_t* data, size_t length, AnnounceMessage& announce_msg);
        
        /**
         * @brief Serialize message to raw bytes
         * @param message gPTP message to serialize
         * @param output Output vector to store serialized data
         * @return true if successful, false otherwise
         */
        template<typename MessageType>
        static bool serialize_message(const MessageType& message, std::vector<uint8_t>& output);
        
        /**
         * @brief Create complete gPTP packet ready for transmission
         * @param message gPTP message
         * @param source_mac Source MAC address
         * @param packet Output packet
         * @return true if successful, false otherwise
         */
        template<typename MessageType>
        static bool create_packet(const MessageType& message, 
                                 const std::array<uint8_t, 6>& source_mac, 
                                 GptpPacket& packet);
        
        /**
         * @brief Convert ParseResult to human-readable string
         * @param result Parse result to convert
         * @return String description of the result
         */
        static const char* parse_result_to_string(ParseResult result);
        
    private:
        /**
         * @brief Validate minimum packet length
         * @param length Actual packet length
         * @param required Required minimum length
         * @return true if length is sufficient
         */
        static bool validate_length(size_t length, size_t required);
        
        /**
         * @brief Convert network byte order to host byte order
         */
        static uint16_t ntohs_safe(uint16_t value);
        static uint32_t ntohl_safe(uint32_t value);
        static uint64_t ntohll_safe(uint64_t value);
        
        /**
         * @brief Convert host byte order to network byte order
         */
        static uint16_t htons_safe(uint16_t value);
        static uint32_t htonl_safe(uint32_t value);
        static uint64_t htonll_safe(uint64_t value);
    };

    // Template implementations
    template<typename MessageType>
    bool MessageParser::serialize_message(const MessageType& message, std::vector<uint8_t>& output) {
        output.resize(sizeof(MessageType));
        std::memcpy(output.data(), &message, sizeof(MessageType));
        
        // Convert to network byte order (this is a simplified implementation)
        // In a full implementation, you'd need to handle endianness properly
        
        return true;
    }

    template<typename MessageType>
    bool MessageParser::create_packet(const MessageType& message, 
                                     const std::array<uint8_t, 6>& source_mac, 
                                     GptpPacket& packet) {
        // Set up Ethernet header
        packet.ethernet.destination = protocol::GPTP_MULTICAST_MAC;
        packet.ethernet.source = source_mac;
        packet.ethernet.etherType = protocol::GPTP_ETHERTYPE;
        
        // Serialize the message
        return serialize_message(message, packet.payload);
    }

} // namespace gptp

#ifdef _MSC_VER
    #pragma pack(pop)
#endif
