/**
 * @file windows_socket.hpp
 * @brief Windows implementation of gPTP socket using WinPcap
 */

#pragma once

#include "../../include/gptp_socket.hpp"
#include <thread>
#include <atomic>

#ifdef _WIN32
#include <winsock2.h>

// Forward declarations for WinPcap types
struct pcap;
typedef struct pcap pcap_t;

namespace gptp {

/**
 * @brief Windows implementation of gPTP socket
 * Uses WinPcap for raw Ethernet access with hardware timestamping support
 */
class WindowsSocket : public IGptpSocket {
public:
    WindowsSocket();
    ~WindowsSocket() override;

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
    
    // WinPcap handle
    pcap_t* pcap_handle_;
    
    // UDP fallback socket (if WinPcap not available)
    SOCKET udp_socket_;
    
    // Async reception
    std::thread async_thread_;
    std::atomic<bool> async_thread_running_;
    PacketCallback packet_callback_;

    // Helper methods
    bool get_interface_mac_address();
    bool check_hardware_timestamping();
    std::string get_mac_string() const;
};

} // namespace gptp

#endif // _WIN32
