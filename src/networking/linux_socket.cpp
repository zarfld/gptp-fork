/**
 * @file linux_socket.cpp
 * @brief Linux implementation of gPTP socket using raw sockets
 */

#include "linux_socket.hpp"
#include "../../include/gptp_protocol.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <algorithm>

#ifdef __linux__
#include <unistd.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>
#include <linux/net_tstamp.h>
#include <arpa/inet.h>

namespace gptp {

LinuxSocket::LinuxSocket() 
    : initialized_(false)
    , hardware_timestamping_available_(false)
    , interface_index_(0)
    , raw_socket_(-1)
    , async_thread_running_(false) {
}

LinuxSocket::~LinuxSocket() {
    cleanup();
}

Result<bool> LinuxSocket::initialize(const std::string& interface_name) {
    if (initialized_) {
        return Result<bool>::success(true);
    }

    interface_name_ = interface_name;
    
    // Create raw socket for Ethernet
    raw_socket_ = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (raw_socket_ < 0) {
        return Result<bool>::error("Failed to create raw socket (requires root privileges)");
    }

    // Get interface information
    if (!get_interface_info()) {
        close(raw_socket_);
        return Result<bool>::error("Failed to get interface information for: " + interface_name);
    }

    // Bind to specific interface
    struct sockaddr_ll socket_address{};
    socket_address.sll_family = AF_PACKET;
    socket_address.sll_protocol = htons(protocol::GPTP_ETHERTYPE);
    socket_address.sll_ifindex = interface_index_;

    if (bind(raw_socket_, (struct sockaddr*)&socket_address, sizeof(socket_address)) < 0) {
        close(raw_socket_);
        return Result<bool>::error("Failed to bind raw socket to interface");
    }

    // Enable hardware timestamping if available
    hardware_timestamping_available_ = check_hardware_timestamping();
    if (hardware_timestamping_available_) {
        enable_timestamping();
    }

    std::cout << "Linux gPTP socket initialized:" << std::endl;
    std::cout << "  Interface: " << interface_name_ << " (index: " << interface_index_ << ")" << std::endl;
    std::cout << "  MAC: " << get_mac_string() << std::endl;
    std::cout << "  Hardware timestamping: " << (hardware_timestamping_available_ ? "Yes" : "No") << std::endl;

    initialized_ = true;
    return Result<bool>::success(true);
}

void LinuxSocket::cleanup() {
    if (!initialized_) return;

    stop_async_receive();

    if (raw_socket_ >= 0) {
        close(raw_socket_);
        raw_socket_ = -1;
    }

    initialized_ = false;
}

Result<bool> LinuxSocket::send_packet(const GptpPacket& packet, PacketTimestamp& timestamp) {
    if (!initialized_) {
        return Result<bool>::error("Socket not initialized");
    }

    // Prepare sockaddr_ll for transmission
    struct sockaddr_ll socket_address{};
    socket_address.sll_family = AF_PACKET;
    socket_address.sll_protocol = htons(protocol::GPTP_ETHERTYPE);
    socket_address.sll_ifindex = interface_index_;
    socket_address.sll_halen = ETH_ALEN;
    std::memcpy(socket_address.sll_addr, packet.ethernet.destination.data(), ETH_ALEN);

    // Prepare frame data
    std::vector<uint8_t> frame_data;
    frame_data.resize(sizeof(EthernetFrame) + packet.payload.size());
    
    // Copy Ethernet header
    std::memcpy(frame_data.data(), &packet.ethernet, sizeof(EthernetFrame));
    
    // Copy payload
    if (!packet.payload.empty()) {
        std::memcpy(frame_data.data() + sizeof(EthernetFrame), 
                   packet.payload.data(), packet.payload.size());
    }

    // Send packet
    ssize_t sent = sendto(raw_socket_, frame_data.data(), frame_data.size(), 0,
                         (struct sockaddr*)&socket_address, sizeof(socket_address));

    // Record transmission time
    auto now = std::chrono::high_resolution_clock::now();
    timestamp.hardware_timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch());
    timestamp.is_hardware_timestamp = hardware_timestamping_available_;

    if (sent < 0) {
        return Result<bool>::error("Failed to send packet: " + std::string(strerror(errno)));
    }

    return Result<bool>::success(true);
}

Result<ReceivedPacket> LinuxSocket::receive_packet(uint32_t timeout_ms) {
    if (!initialized_) {
        return Result<ReceivedPacket>::error("Socket not initialized");
    }

    // Set timeout if specified
    if (timeout_ms > 0) {
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        setsockopt(raw_socket_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }

    // Receive packet
    uint8_t buffer[1518]; // Maximum Ethernet frame size
    struct sockaddr_ll sender_addr;
    socklen_t addr_len = sizeof(sender_addr);

    ssize_t received = recvfrom(raw_socket_, buffer, sizeof(buffer), 0,
                               (struct sockaddr*)&sender_addr, &addr_len);

    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return Result<ReceivedPacket>::error("Timeout");
        }
        return Result<ReceivedPacket>::error("Failed to receive packet: " + std::string(strerror(errno)));
    }

    // Filter for gPTP packets
    if (received < sizeof(EthernetFrame)) {
        return Result<ReceivedPacket>::error("Packet too short");
    }

    EthernetFrame* eth_header = reinterpret_cast<EthernetFrame*>(buffer);
    if (ntohs(eth_header->etherType) != protocol::GPTP_ETHERTYPE) {
        return Result<ReceivedPacket>::error("Not a gPTP packet");
    }

    // Create received packet
    ReceivedPacket received_packet;
    
    // Set timestamp
    auto now = std::chrono::high_resolution_clock::now();
    received_packet.timestamp.hardware_timestamp = 
        std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch());
    received_packet.timestamp.is_hardware_timestamp = hardware_timestamping_available_;
    
    // Copy Ethernet header
    std::memcpy(&received_packet.packet.ethernet, buffer, sizeof(EthernetFrame));
    
    // Copy payload
    size_t payload_size = received - sizeof(EthernetFrame);
    if (payload_size > 0) {
        received_packet.packet.payload.resize(payload_size);
        std::memcpy(received_packet.packet.payload.data(),
                   buffer + sizeof(EthernetFrame), payload_size);
    }

    return Result<ReceivedPacket>::success(received_packet);
}

Result<bool> LinuxSocket::start_async_receive(PacketCallback callback) {
    if (async_thread_running_) {
        return Result<bool>::success(true);
    }

    packet_callback_ = callback;
    async_thread_running_ = true;
    
    async_thread_ = std::thread([this]() {
        while (async_thread_running_) {
            auto result = receive_packet(100); // 100ms timeout
            if (result.is_success()) {
                if (packet_callback_) {
                    packet_callback_(result.value());
                }
            }
            // Continue on timeout or error
        }
    });

    return Result<bool>::success(true);
}

void LinuxSocket::stop_async_receive() {
    if (!async_thread_running_) return;

    async_thread_running_ = false;
    if (async_thread_.joinable()) {
        async_thread_.join();
    }
}

bool LinuxSocket::is_hardware_timestamping_available() const {
    return hardware_timestamping_available_;
}

Result<std::array<uint8_t, 6>> LinuxSocket::get_interface_mac() const {
    return Result<std::array<uint8_t, 6>>::success(mac_address_);
}

std::string LinuxSocket::get_interface_name() const {
    return interface_name_;
}

bool LinuxSocket::get_interface_info() {
    struct ifreq ifr;
    std::strncpy(ifr.ifr_name, interface_name_.c_str(), IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';

    // Get interface index
    if (ioctl(raw_socket_, SIOCGIFINDEX, &ifr) < 0) {
        return false;
    }
    interface_index_ = ifr.ifr_ifindex;

    // Get MAC address
    if (ioctl(raw_socket_, SIOCGIFHWADDR, &ifr) < 0) {
        return false;
    }
    std::memcpy(mac_address_.data(), ifr.ifr_hwaddr.sa_data, 6);

    return true;
}

bool LinuxSocket::check_hardware_timestamping() {
    struct ifreq ifr;
    std::strncpy(ifr.ifr_name, interface_name_.c_str(), IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';

    struct ethtool_ts_info ts_info;
    ts_info.cmd = ETHTOOL_GET_TS_INFO;
    ifr.ifr_data = reinterpret_cast<char*>(&ts_info);

    if (ioctl(raw_socket_, SIOCETHTOOL, &ifr) == 0) {
        // Check if hardware timestamping is supported
        return (ts_info.so_timestamping & SOF_TIMESTAMPING_TX_HARDWARE) &&
               (ts_info.so_timestamping & SOF_TIMESTAMPING_RX_HARDWARE);
    }

    return false; // Assume no hardware timestamping if query fails
}

bool LinuxSocket::enable_timestamping() {
    int timestamping_flags = SOF_TIMESTAMPING_TX_HARDWARE |
                            SOF_TIMESTAMPING_RX_HARDWARE |
                            SOF_TIMESTAMPING_RAW_HARDWARE;

    if (setsockopt(raw_socket_, SOL_SOCKET, SO_TIMESTAMPING,
                   &timestamping_flags, sizeof(timestamping_flags)) < 0) {
        std::cout << "⚠️ Failed to enable hardware timestamping, using software timestamps" << std::endl;
        return false;
    }

    return true;
}

std::string LinuxSocket::get_mac_string() const {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < mac_address_.size(); ++i) {
        if (i > 0) ss << ":";
        ss << std::setw(2) << static_cast<int>(mac_address_[i]);
    }
    return ss.str();
}

} // namespace gptp

#endif // __linux__
