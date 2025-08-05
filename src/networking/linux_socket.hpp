/**
 * @file linux_socket.hpp
 * @brief Linux implementation of gPTP socket using raw sockets
 */

#pragma once

#include "../../include/gptp_socket.hpp"
#include <thread>
#include <atomic>

#ifdef __linux__
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>

namespace gptp {

/**
 * @brief Linux implementation of gPTP socket
 * Uses raw sockets with proper timestamping support
 */
class LinuxSocket : public IGptpSocket {
public:
    LinuxSocket();
    ~LinuxSocket() override;

    // IGptpSocket interface
    Result<bool> initialize(const std::string& interface_name) override;
    void cleanup() override;
    Result<bool> send_packet(const GptpPacket& packet, PacketTimestamp& timestamp) override;
    Result<ReceivedPacket> receive_packet(uint32_t timeout_ms = 0) override;
    Result<bool> start_async_receive(PacketCallback callback) override;
    void stop_async_receive() override;
    bool is_hardware_timestamping_available() const override;
    Result<std::array<uint8_t, 6>> get_interface_mac() const override;
    std::string get_interface_name() const override;

private:
    bool initialized_;
    std::string interface_name_;
    std::array<uint8_t, 6> mac_address_;
    bool hardware_timestamping_available_;
    int interface_index_;
    
    // Raw socket
    int raw_socket_;
    
    // Async reception
    std::thread async_thread_;
    std::atomic<bool> async_thread_running_;
    PacketCallback packet_callback_;

    // Helper methods
    bool get_interface_info();
    bool check_hardware_timestamping();
    bool enable_timestamping();
    std::string get_mac_string() const;
};

} // namespace gptp

#endif // __linux__
