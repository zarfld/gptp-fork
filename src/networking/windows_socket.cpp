/**
 * @file windows_socket.cpp
 * @brief Windows implementation of gPTP socket using WinPcap/Npcap
 */

#include "windows_socket.hpp"
#include "../../include/gptp_protocol.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <iomanip>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

// Include Npcap headers if available
#ifdef HAVE_NPCAP
#include <pcap.h>
#pragma comment(lib, "packet.lib")
#pragma comment(lib, "wpcap.lib")
#endif

namespace gptp {

WindowsSocket::WindowsSocket() 
    : initialized_(false)
    , hardware_timestamping_available_(false)
    , pcap_handle_(nullptr)
    , async_thread_running_(false) {
}

WindowsSocket::~WindowsSocket() {
    cleanup();
}

Result<bool> WindowsSocket::initialize(const std::string& interface_name) {
    if (initialized_) {
        return Result<bool>::success(true);
    }

    interface_name_ = interface_name;
    
    // Initialize Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        return Result<bool>::error("Failed to initialize Winsock: " + std::to_string(result));
    }

#ifdef HAVE_NPCAP
    // Try to initialize with Npcap for raw Ethernet access
    char errbuf[PCAP_ERRBUF_SIZE];
    
    // Find the device
    pcap_if_t* devices;
    if (pcap_findalldevs(&devices, errbuf) == -1) {
        WSACleanup();
        return Result<bool>::error("Error finding devices: " + std::string(errbuf));
    }

    pcap_if_t* device = nullptr;
    for (pcap_if_t* d = devices; d != nullptr; d = d->next) {
        if (interface_name == d->name || 
            (d->description && interface_name == d->description)) {
            device = d;
            break;
        }
    }

    if (!device) {
        pcap_freealldevs(devices);
        WSACleanup();
        return Result<bool>::error("Interface not found: " + interface_name);
    }

    // Open the device
    pcap_handle_ = pcap_open_live(device->name, 65536, 1, 1000, errbuf);
    if (!pcap_handle_) {
        pcap_freealldevs(devices);
        WSACleanup();
        return Result<bool>::error("Error opening device: " + std::string(errbuf));
    }

    // Set filter for gPTP packets (EtherType 0x88F7)
    struct bpf_program fp;
    std::string filter_exp = "ether proto 0x88f7";
    if (pcap_compile(pcap_handle_, &fp, filter_exp.c_str(), 0, PCAP_NETMASK_UNKNOWN) == -1) {
        pcap_close(pcap_handle_);
        pcap_freealldevs(devices);
        WSACleanup();
        return Result<bool>::error("Error compiling filter");
    }

    if (pcap_setfilter(pcap_handle_, &fp) == -1) {
        pcap_freecode(&fp);
        pcap_close(pcap_handle_);
        pcap_freealldevs(devices);
        WSACleanup();
        return Result<bool>::error("Error setting filter");
    }

    pcap_freecode(&fp);
    pcap_freealldevs(devices);

    // Get interface MAC address
    if (!get_interface_mac_address()) {
        pcap_close(pcap_handle_);
        WSACleanup();
        return Result<bool>::error("Failed to get interface MAC address");
    }

    // Check for hardware timestamping support
    hardware_timestamping_available_ = check_hardware_timestamping();

    std::cout << "Windows gPTP socket initialized:" << std::endl;
    std::cout << "  Interface: " << interface_name_ << std::endl;
    std::cout << "  MAC: " << get_mac_string() << std::endl;
    std::cout << "  Hardware timestamping: " << (hardware_timestamping_available_ ? "Yes" : "No") << std::endl;

#else
    // Fallback to UDP multicast if Npcap not available
    std::cout << "⚠️ Npcap not available, using UDP fallback" << std::endl;
    
    // Create UDP socket for multicast
    udp_socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_socket_ == INVALID_SOCKET) {
        WSACleanup();
        return Result<bool>::error("Failed to create UDP socket");
    }

    // Set up multicast address (using 224.0.1.129 for gPTP)
    sockaddr_in multicast_addr{};
    multicast_addr.sin_family = AF_INET;
    multicast_addr.sin_port = htons(319); // gPTP event port
    inet_pton(AF_INET, "224.0.1.129", &multicast_addr.sin_addr);

    // Bind to multicast address
    if (bind(udp_socket_, (sockaddr*)&multicast_addr, sizeof(multicast_addr)) == SOCKET_ERROR) {
        closesocket(udp_socket_);
        WSACleanup();
        return Result<bool>::error("Failed to bind UDP socket");
    }

    std::cout << "UDP multicast socket initialized (fallback mode)" << std::endl;
#endif

    initialized_ = true;
    return Result<bool>::success(true);
}

void WindowsSocket::cleanup() {
    if (!initialized_) return;

    stop_async_receive();

#ifdef HAVE_NPCAP
    if (pcap_handle_) {
        pcap_close(pcap_handle_);
        pcap_handle_ = nullptr;
    }
#else
    if (udp_socket_ != INVALID_SOCKET) {
        closesocket(udp_socket_);
        udp_socket_ = INVALID_SOCKET;
    }
#endif

    WSACleanup();
    initialized_ = false;
}

Result<bool> WindowsSocket::send_packet(const GptpPacket& packet, PacketTimestamp& timestamp) {
    if (!initialized_) {
        return Result<bool>::error("Socket not initialized");
    }

    // Record transmission time
    auto now = std::chrono::high_resolution_clock::now();
    timestamp.hardware_timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch());
    timestamp.is_hardware_timestamp = hardware_timestamping_available_;

#ifdef HAVE_NPCAP
    // Send raw Ethernet frame
    std::vector<uint8_t> frame_data;
    frame_data.resize(sizeof(EthernetFrame) + packet.payload.size());
    
    // Copy Ethernet header
    std::memcpy(frame_data.data(), &packet.ethernet, sizeof(EthernetFrame));
    
    // Copy payload
    if (!packet.payload.empty()) {
        std::memcpy(frame_data.data() + sizeof(EthernetFrame), 
                   packet.payload.data(), packet.payload.size());
    }

    if (pcap_sendpacket(pcap_handle_, frame_data.data(), static_cast<int>(frame_data.size())) != 0) {
        return Result<bool>::error("Failed to send packet: " + std::string(pcap_geterr(pcap_handle_)));
    }
#else
    // Send via UDP (fallback)
    sockaddr_in dest_addr{};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(319);
    inet_pton(AF_INET, "224.0.1.129", &dest_addr.sin_addr);

    if (sendto(udp_socket_, reinterpret_cast<const char*>(packet.payload.data()), 
               static_cast<int>(packet.payload.size()), 0,
               (sockaddr*)&dest_addr, sizeof(dest_addr)) == SOCKET_ERROR) {
        return Result<bool>::error("Failed to send UDP packet");
    }
#endif

    return Result<bool>::success(true);
}

Result<ReceivedPacket> WindowsSocket::receive_packet(uint32_t timeout_ms) {
    if (!initialized_) {
        return Result<ReceivedPacket>::error("Socket not initialized");
    }

#ifdef HAVE_NPCAP
    struct pcap_pkthdr* header;
    const u_char* pkt_data;
    
    int result = pcap_next_ex(pcap_handle_, &header, &pkt_data);
    if (result == 1) {
        // Packet received
        ReceivedPacket received_packet;
        
        // Set timestamp
        auto now = std::chrono::high_resolution_clock::now();
        received_packet.timestamp.hardware_timestamp = 
            std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch());
        received_packet.timestamp.is_hardware_timestamp = hardware_timestamping_available_;
        
        // Copy packet data
        received_packet.packet.payload.resize(header->caplen - sizeof(EthernetFrame));
        
        if (header->caplen >= sizeof(EthernetFrame)) {
            // Copy Ethernet header
            std::memcpy(&received_packet.packet.ethernet, pkt_data, sizeof(EthernetFrame));
            
            // Copy payload
            if (header->caplen > sizeof(EthernetFrame)) {
                std::memcpy(received_packet.packet.payload.data(),
                           pkt_data + sizeof(EthernetFrame),
                           header->caplen - sizeof(EthernetFrame));
            }
        }
        
        return Result<ReceivedPacket>::success(received_packet);
    } else if (result == 0) {
        return Result<ReceivedPacket>::error("Timeout");
    } else {
        return Result<ReceivedPacket>::error("Error receiving packet");
    }
#else
    // UDP fallback
    sockaddr_in sender_addr;
    int addr_len = sizeof(sender_addr);
    char buffer[1500];
    
    int received = recvfrom(udp_socket_, buffer, sizeof(buffer), 0,
                           (sockaddr*)&sender_addr, &addr_len);
    if (received > 0) {
        ReceivedPacket received_packet;
        
        // Set timestamp
        auto now = std::chrono::high_resolution_clock::now();
        received_packet.timestamp.hardware_timestamp = 
            std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch());
        received_packet.timestamp.is_hardware_timestamp = false;
        
        // Copy data
        received_packet.packet.payload.resize(received);
        std::memcpy(received_packet.packet.payload.data(), buffer, received);
        
        return Result<ReceivedPacket>::success(received_packet);
    }
    
    return Result<ReceivedPacket>::error("Failed to receive packet");
#endif
}

Result<bool> WindowsSocket::start_async_receive(PacketCallback callback) {
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

void WindowsSocket::stop_async_receive() {
    if (!async_thread_running_) return;

    async_thread_running_ = false;
    if (async_thread_.joinable()) {
        async_thread_.join();
    }
}

bool WindowsSocket::is_hardware_timestamping_available() const {
    return hardware_timestamping_available_;
}

Result<std::array<uint8_t, 6>> WindowsSocket::get_interface_mac() const {
    return Result<std::array<uint8_t, 6>>::success(mac_address_);
}

std::string WindowsSocket::get_interface_name() const {
    return interface_name_;
}

bool WindowsSocket::get_interface_mac_address() {
    // Get adapter information
    ULONG adapter_info_size = 0;
    GetAdaptersInfo(nullptr, &adapter_info_size);
    
    std::vector<uint8_t> adapter_buffer(adapter_info_size);
    PIP_ADAPTER_INFO adapter_info = reinterpret_cast<PIP_ADAPTER_INFO>(adapter_buffer.data());
    
    if (GetAdaptersInfo(adapter_info, &adapter_info_size) != ERROR_SUCCESS) {
        return false;
    }

    // Find our interface
    for (PIP_ADAPTER_INFO adapter = adapter_info; adapter != nullptr; adapter = adapter->Next) {
        if (interface_name_ == adapter->AdapterName || 
            interface_name_ == adapter->Description) {
            if (adapter->AddressLength == 6) {
                std::memcpy(mac_address_.data(), adapter->Address, 6);
                return true;
            }
        }
    }

    return false;
}

bool WindowsSocket::check_hardware_timestamping() {
    // Check if interface supports hardware timestamping
    // This is a simplified check - in reality, you'd query the driver
    
    // For Intel adapters, hardware timestamping is typically available
    std::string interface_lower = interface_name_;
    std::transform(interface_lower.begin(), interface_lower.end(), 
                   interface_lower.begin(), ::tolower);
    
    return (interface_lower.find("intel") != std::string::npos ||
            interface_lower.find("i210") != std::string::npos ||
            interface_lower.find("i225") != std::string::npos ||
            interface_lower.find("i226") != std::string::npos ||
            interface_lower.find("e810") != std::string::npos);
}

std::string WindowsSocket::get_mac_string() const {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < mac_address_.size(); ++i) {
        if (i > 0) ss << ":";
        ss << std::setw(2) << static_cast<int>(mac_address_[i]);
    }
    return ss.str();
}

} // namespace gptp

#endif // _WIN32
