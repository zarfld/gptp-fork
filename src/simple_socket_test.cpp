/**
 * @file simple_socket_test.cpp
 * @brief Einfacher Socket-Test für gPTP-Pakete über UDP
 */

#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <chrono>
#include <thread>

#pragma comment(lib, "ws2_32.lib")

class SimpleGptpTest {
public:
    SimpleGptpTest() : socket_(INVALID_SOCKET) {}
    
    ~SimpleGptpTest() {
        cleanup();
    }
    
    bool initialize() {
        // Initialize Winsock
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            std::cout << "❌ WSAStartup failed: " << result << std::endl;
            return false;
        }
        
        // Create UDP socket
        socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socket_ == INVALID_SOCKET) {
            std::cout << "❌ Socket creation failed: " << WSAGetLastError() << std::endl;
            WSACleanup();
            return false;
        }
        
        // Enable broadcast
        BOOL bOptVal = TRUE;
        if (setsockopt(socket_, SOL_SOCKET, SO_BROADCAST, (char*)&bOptVal, sizeof(bOptVal)) == SOCKET_ERROR) {
            std::cout << "⚠️  Broadcast enable failed: " << WSAGetLastError() << std::endl;
        }
        
        std::cout << "✅ Simple gPTP test socket initialized" << std::endl;
        return true;
    }
    
    void cleanup() {
        if (socket_ != INVALID_SOCKET) {
            closesocket(socket_);
            socket_ = INVALID_SOCKET;
        }
        WSACleanup();
    }
    
    bool send_test_packet() {
        // gPTP multicast address: 224.0.1.129 (IEEE 1588)
        sockaddr_in dest_addr{};
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(319); // gPTP event port
        inet_pton(AF_INET, "224.0.1.129", &dest_addr.sin_addr);
        
        // Create a simple gPTP-like packet
        std::vector<uint8_t> packet(64, 0);
        packet[0] = 0x02; // Peer_Delay_Req message type
        packet[1] = 0x02; // Version
        packet[2] = 0x00; packet[3] = 0x2C; // Message Length (44)
        packet[4] = 0x00; // Domain
        
        static uint16_t seq_id = 0;
        packet[30] = (seq_id >> 8) & 0xFF;
        packet[31] = seq_id & 0xFF;
        seq_id++;
        
        int bytes_sent = sendto(socket_, 
                               reinterpret_cast<const char*>(packet.data()), 
                               static_cast<int>(packet.size()),
                               0,
                               reinterpret_cast<const sockaddr*>(&dest_addr),
                               sizeof(dest_addr));
        
        if (bytes_sent == SOCKET_ERROR) {
            std::cout << "❌ Send failed: " << WSAGetLastError() << std::endl;
            return false;
        }
        
        std::cout << "📤 Sent " << bytes_sent << " bytes to gPTP multicast address" << std::endl;
        return true;
    }
    
    void run_test() {
        std::cout << "🚀 Starting simple gPTP test..." << std::endl;
        
        for (int i = 0; i < 10; i++) {
            if (send_test_packet()) {
                std::cout << "✅ Test packet " << (i + 1) << " sent successfully" << std::endl;
            } else {
                std::cout << "❌ Test packet " << (i + 1) << " failed" << std::endl;
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        std::cout << "🎯 Test completed!" << std::endl;
    }

private:
    SOCKET socket_;
};

int main() {
    std::cout << "=== Simple gPTP Socket Test ===" << std::endl;
    
    SimpleGptpTest test;
    if (!test.initialize()) {
        std::cout << "❌ Test initialization failed" << std::endl;
        return 1;
    }
    
    test.run_test();
    return 0;
}
