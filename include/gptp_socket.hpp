/**
 * @file gptp_socket.hpp
 * @brief IEEE 802.1AS gPTP soc        // Get best available timestamp
        std::chrono::nanoseconds get_timestamp() const {
            return is_hardware_timestamp ? hardware_timestamp : software_timestamp;
        }handling for raw Ethernet frames
 * 
 * This file provides platform-agnostic socket handling for gPTP messages
 * using raw Ethernet sockets with proper timestamping support.
 */

#pragma once

#include "gptp_protocol.hpp"
#include "gptp_message_parser.hpp"
#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <array>
#include <chrono>

namespace gptp {

    /**
     * @brief Result type for error handling
     */
    template<typename T>
    class Result {
    public:
        static Result success(const T& value) {
            Result result;
            result.success_ = true;
            result.value_ = value;
            return result;
        }

        static Result error(const std::string& error_message) {
            Result result;
            result.success_ = false;
            result.error_message_ = error_message;
            return result;
        }

        bool is_success() const { return success_; }
        bool is_error() const { return !success_; }
        
        const T& value() const { return value_; }
        const std::string& error() const { return error_message_; }

    private:
        bool success_ = false;
        T value_{};
        std::string error_message_;
    };

    /**
     * @brief Timestamp information for received/transmitted packets
     */
    struct PacketTimestamp {
        std::chrono::nanoseconds hardware_timestamp{0};
        std::chrono::nanoseconds software_timestamp{0};
        bool is_hardware_timestamp = false;
        bool software_timestamp_valid = false;
        
        // Get the best available timestamp
        std::chrono::nanoseconds get_best_timestamp() const {
            return is_hardware_timestamp ? hardware_timestamp : software_timestamp;
        }
    };

    /**
     * @brief Received gPTP packet with timing information
     */
    struct ReceivedPacket {
        GptpPacket packet;
        PacketTimestamp timestamp;
        std::string interface_name;
        
        ReceivedPacket() = default;
        ReceivedPacket(GptpPacket pkt, PacketTimestamp ts, std::string iface)
            : packet(std::move(pkt)), timestamp(ts), interface_name(std::move(iface)) {}
    };

    /**
     * @brief Callback function for received gPTP packets
     */
    using PacketCallback = std::function<void(const ReceivedPacket&)>;

    /**
     * @brief gPTP Socket interface for raw Ethernet communication
     */
    class IGptpSocket {
    public:
        virtual ~IGptpSocket() = default;

        /**
         * @brief Initialize the socket for the specified interface
         * @param interface_name Network interface name
         * @return Result indicating success or error
         */
        virtual Result<bool> initialize(const std::string& interface_name) = 0;

        /**
         * @brief Cleanup and close the socket
         */
        virtual void cleanup() = 0;

        /**
         * @brief Send a gPTP packet
         * @param packet Packet to send
         * @param timestamp Output parameter for transmission timestamp
         * @return Result indicating success or error
         */
        virtual Result<bool> send_packet(const GptpPacket& packet, PacketTimestamp& timestamp) = 0;

        /**
         * @brief Receive gPTP packets (blocking)
         * @param timeout_ms Timeout in milliseconds, 0 for no timeout
         * @return Result containing received packet or error
         */
        virtual Result<ReceivedPacket> receive_packet(uint32_t timeout_ms = 0) = 0;

        /**
         * @brief Start asynchronous packet reception
         * @param callback Callback function for received packets
         * @return Result indicating success or error
         */
        virtual Result<bool> start_async_receive(PacketCallback callback) = 0;

        /**
         * @brief Stop asynchronous packet reception
         */
        virtual void stop_async_receive() = 0;

        /**
         * @brief Check if hardware timestamping is available
         * @return true if hardware timestamping is supported
         */
        virtual bool is_hardware_timestamping_available() const = 0;

        /**
         * @brief Get the MAC address of the interface
         * @return MAC address as byte array
         */
        virtual Result<std::array<uint8_t, 6>> get_interface_mac() const = 0;

        /**
         * @brief Get interface name
         * @return Interface name
         */
        virtual std::string get_interface_name() const = 0;
    };

    /**
     * @brief gPTP Socket Manager - manages multiple socket instances
     */
    class GptpSocketManager {
    public:
        /**
         * @brief Create a socket for the specified interface
         * @param interface_name Network interface name
         * @return Unique pointer to socket instance or nullptr on error
         */
        static std::unique_ptr<IGptpSocket> create_socket(const std::string& interface_name);

        /**
         * @brief Check if gPTP socket creation is supported on current platform
         * @return true if supported, false otherwise
         */
        static bool is_supported();

        /**
         * @brief Get list of available network interfaces suitable for gPTP
         * @return Vector of interface names
         */
        static std::vector<std::string> get_available_interfaces();

    private:
        GptpSocketManager() = delete;
    };

    /**
     * @brief gPTP Packet Builder - helper class for creating gPTP packets
     */
    class GptpPacketBuilder {
    public:
        /**
         * @brief Create a Sync packet
         * @param source_port_identity Source port identity
         * @param sequence_id Sequence ID
         * @param source_mac Source MAC address
         * @return Created packet
         */
        static GptpPacket create_sync_packet(const PortIdentity& source_port_identity,
                                           uint16_t sequence_id,
                                           const std::array<uint8_t, 6>& source_mac);

        /**
         * @brief Create a Follow_Up packet
         * @param source_port_identity Source port identity
         * @param sequence_id Sequence ID
         * @param precise_origin_timestamp Precise origin timestamp
         * @param source_mac Source MAC address
         * @return Created packet
         */
        static GptpPacket create_followup_packet(const PortIdentity& source_port_identity,
                                                uint16_t sequence_id,
                                                const Timestamp& precise_origin_timestamp,
                                                const std::array<uint8_t, 6>& source_mac);

        /**
         * @brief Create a Pdelay_Req packet
         * @param source_port_identity Source port identity
         * @param sequence_id Sequence ID
         * @param source_mac Source MAC address
         * @return Created packet
         */
        static GptpPacket create_pdelay_req_packet(const PortIdentity& source_port_identity,
                                                  uint16_t sequence_id,
                                                  const std::array<uint8_t, 6>& source_mac);

        /**
         * @brief Create a Pdelay_Resp packet
         * @param source_port_identity Source port identity
         * @param sequence_id Sequence ID
         * @param request_receipt_timestamp When the request was received
         * @param requesting_port_identity Port that sent the request
         * @param source_mac Source MAC address
         * @return Created packet
         */
        static GptpPacket create_pdelay_resp_packet(const PortIdentity& source_port_identity,
                                                   uint16_t sequence_id,
                                                   const Timestamp& request_receipt_timestamp,
                                                   const PortIdentity& requesting_port_identity,
                                                   const std::array<uint8_t, 6>& source_mac);

        /**
         * @brief Create an Announce packet
         * @param source_port_identity Source port identity
         * @param sequence_id Sequence ID
         * @param grandmaster_identity Grandmaster clock identity
         * @param grandmaster_priority1 Grandmaster priority1
         * @param grandmaster_priority2 Grandmaster priority2
         * @param steps_removed Steps removed from grandmaster
         * @param source_mac Source MAC address
         * @return Created packet
         */
        static GptpPacket create_announce_packet(const PortIdentity& source_port_identity,
                                                uint16_t sequence_id,
                                                const ClockIdentity& grandmaster_identity,
                                                uint8_t grandmaster_priority1,
                                                uint8_t grandmaster_priority2,
                                                uint16_t steps_removed,
                                                const std::array<uint8_t, 6>& source_mac);

    private:
        /**
         * @brief Set current timestamp from system clock
         * @param timestamp Output timestamp
         */
        static void set_current_timestamp(Timestamp& timestamp);
    };

} // namespace gptp
