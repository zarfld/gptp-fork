/**
 * @file socket_manager.cpp
 * @brief Implementation of GptpSocketManager for cross-platform socket creation
 */

#include "../../include/gptp_socket.hpp"

#ifdef _WIN32
#include "windows_socket.hpp"
#endif

#ifdef __linux__
#include "linux_socket.hpp"
#endif

#include <iostream>

namespace gptp {

std::unique_ptr<IGptpSocket> GptpSocketManager::create_socket(const std::string& interface_name) {
    if (interface_name.empty()) {
        std::cerr << "❌ Empty interface name provided" << std::endl;
        return nullptr;
    }

    std::cout << "Creating gPTP socket for interface: " << interface_name << std::endl;

#ifdef _WIN32
    auto socket = std::make_unique<WindowsSocket>();
    auto result = socket->initialize(interface_name);
    if (result.is_success()) {
        std::cout << "✅ Windows gPTP socket created successfully" << std::endl;
        return std::move(socket);
    } else {
        std::cerr << "❌ Failed to initialize Windows socket: " << static_cast<int>(result.error()) << std::endl;
        return nullptr;
    }
#elif defined(__linux__)
    auto socket = std::make_unique<LinuxSocket>();
    auto result = socket->initialize(interface_name);
    if (result.is_success()) {
        std::cout << "✅ Linux gPTP socket created successfully" << std::endl;
        return std::move(socket);
    } else {
        std::cerr << "❌ Failed to initialize Linux socket: " << static_cast<int>(result.error()) << std::endl;
        return nullptr;
    }
#else
    std::cerr << "❌ Unsupported platform for gPTP socket" << std::endl;
    return nullptr;
#endif
}

bool GptpSocketManager::is_supported() {
#ifdef _WIN32
    return true; // Windows with Npcap/WinPcap
#elif defined(__linux__)
    return true; // Linux with raw sockets
#else
    return false; // Unsupported platform
#endif
}

std::vector<std::string> GptpSocketManager::get_available_interfaces() {
    std::vector<std::string> interfaces;

#ifdef _WIN32
    // Use Windows API to enumerate network interfaces
    // This is a simplified implementation
    interfaces.push_back("Ethernet");
    interfaces.push_back("Wi-Fi");
    
    std::cout << "📡 Available Windows interfaces (simplified list):" << std::endl;
    for (const auto& iface : interfaces) {
        std::cout << "  - " << iface << std::endl;
    }
#elif defined(__linux__)
    // Use Linux interface enumeration
    // Read from /proc/net/dev or use netlink
    interfaces.push_back("eth0");
    interfaces.push_back("enp0s3"); // VirtualBox
    interfaces.push_back("ens33");  // VMware
    
    std::cout << "📡 Available Linux interfaces (common names):" << std::endl;
    for (const auto& iface : interfaces) {
        std::cout << "  - " << iface << std::endl;
    }
#endif

    return interfaces;
}

} // namespace gptp
