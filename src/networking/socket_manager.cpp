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
#include <thread>
#include <atomic>
#include <chrono>

namespace gptp {

std::unique_ptr<IGptpSocket> GptpSocketManager::create_socket(const std::string& interface_name) {
    if (interface_name.empty()) {
        std::cerr << "❌ Empty interface name provided" << std::endl;
        return nullptr;
    }

    std::cout << "🔄 [MANAGER] Creating gPTP socket for interface: " << interface_name << std::endl;

#ifdef _WIN32
    auto socket = std::make_unique<WindowsSocket>();
    
    // Use timeout wrapper to prevent hanging during socket initialization
    std::cout << "⏱️  [MANAGER] Starting socket initialization with 10-second timeout..." << std::endl;
    
    std::atomic<bool> initialization_complete{false};
    std::atomic<bool> initialization_success{false};
    Result<bool> result = Result<bool>::success(false); // Initialize with default
    
    std::thread init_thread([&]() {
        try {
            result = socket->initialize(interface_name);
            initialization_success = result.is_success();
            initialization_complete = true;
            std::cout << "✅ [MANAGER] Socket initialization thread completed" << std::endl;
        }
        catch (const std::exception& e) {
            std::cout << "❌ [MANAGER] Exception in initialization thread: " << e.what() << std::endl;
            initialization_success = false;
            initialization_complete = true;
        }
        catch (...) {
            std::cout << "❌ [MANAGER] Unknown exception in initialization thread" << std::endl;
            initialization_success = false;
            initialization_complete = true;
        }
    });
    
    // Wait for completion with timeout
    auto start_time = std::chrono::steady_clock::now();
    const auto timeout_duration = std::chrono::seconds(10);
    
    while (!initialization_complete) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (elapsed > timeout_duration) {
            std::cout << "⏰ [MANAGER] Socket initialization timeout after 10 seconds" << std::endl;
            std::cout << "   ⚠️  Terminating problematic initialization and continuing..." << std::endl;
            
            // Note: In production, we should properly terminate the thread
            // For now, we'll detach it to prevent resource issues
            init_thread.detach();
            return nullptr;
        }
    }
    
    init_thread.join();
    
    if (initialization_success) {
        std::cout << "✅ [MANAGER] Windows gPTP socket created successfully" << std::endl;
        return std::move(socket);
    } else {
        std::cerr << "❌ [MANAGER] Failed to initialize Windows socket: " << static_cast<int>(result.error()) << std::endl;
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
