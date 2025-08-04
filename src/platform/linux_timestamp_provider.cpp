#include "linux_timestamp_provider.hpp"

#ifdef __linux__

#ifndef _GNU_SOURCE
#define _GNU_SOURCE  // Enable GNU extensions for networking
#endif
#include <cstring>
#include <sstream>
#include <iomanip>
#include <errno.h>

namespace gptp {

    LinuxTimestampProvider::LinuxTimestampProvider() : initialized_(false), socket_fd_(-1) {
    }

    LinuxTimestampProvider::~LinuxTimestampProvider() {
        // Don't call virtual function from destructor - cleanup directly
        if (socket_fd_ >= 0) {
            close(socket_fd_);
            socket_fd_ = -1;
        }
        initialized_ = false;
    }

    Result<bool> LinuxTimestampProvider::initialize() {
        if (initialized_) {
            return Result<bool>(true);
        }

        // Create a socket for ioctl operations
        socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_fd_ < 0) {
            return Result<bool>(LinuxTimestampProvider::map_linux_error(errno));
        }

        initialized_ = true;
        return Result<bool>(true);
    }

    void LinuxTimestampProvider::cleanup() {
        if (socket_fd_ >= 0) {
            close(socket_fd_);
            socket_fd_ = -1;
        }
        initialized_ = false;
    }

    Result<TimestampCapabilities> LinuxTimestampProvider::get_timestamp_capabilities(
        const InterfaceName& interface_name) {
        
        if (!initialized_) {
            return Result<TimestampCapabilities>(ErrorCode::INITIALIZATION_FAILED);
        }

        // For now, return basic capabilities based on interface name
        // In a full implementation, this would use ethtool to query actual capabilities
        TimestampCapabilities caps;
        
        // Skip loopback interface
        if (interface_name == "lo") {
            caps.software_timestamping_supported = false;
            caps.hardware_timestamping_supported = false;
            caps.transmit_timestamping = false;
            caps.receive_timestamping = false;
            caps.tagged_transmit = false;
            caps.all_transmit = false;
            caps.all_receive = false;
            return Result<TimestampCapabilities>(caps);
        }
        
        // Most Linux systems support software timestamping
        caps.software_timestamping_supported = true;
        caps.transmit_timestamping = true;
        caps.receive_timestamping = true;
        
        // Hardware timestamping detection would require ethtool queries
        // For now, assume it might be available (this is a simplified implementation)
        caps.hardware_timestamping_supported = false; // Conservative default
        caps.tagged_transmit = false;
        caps.all_transmit = false;
        caps.all_receive = false;
        
        return Result<TimestampCapabilities>(caps);
    }

    Result<std::vector<NetworkInterface>> LinuxTimestampProvider::get_network_interfaces() {
        if (!initialized_) {
            return Result<std::vector<NetworkInterface>>(ErrorCode::INITIALIZATION_FAILED);
        }

        std::vector<NetworkInterface> interfaces;
        
        struct ifaddrs *ifaddr;
        const struct ifaddrs *ifa;
        if (getifaddrs(&ifaddr) == -1) {
            return Result<std::vector<NetworkInterface>>(LinuxTimestampProvider::map_linux_error(errno));
        }

        // Iterate through linked list of interfaces
        for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == nullptr) {
                continue;
            }

            // Only process AF_INET interfaces (IPv4)
            if (ifa->ifa_addr->sa_family == AF_INET) {
                NetworkInterface interface = convert_ifaddr_info(*ifa);
                
                // Skip loopback interfaces
                if (interface.name != "lo" && !interface.name.empty()) {
                    interfaces.push_back(interface);
                }
            }
        }

        freeifaddrs(ifaddr);
        return Result<std::vector<NetworkInterface>>(interfaces);
    }

    bool LinuxTimestampProvider::is_hardware_timestamping_available() const {
        // In a full implementation, this would check for:
        // 1. Kernel support (SO_TIMESTAMPING)
        // 2. Driver support (ethtool queries)
        // 3. Hardware support (NIC capabilities)
        
        // For now, return true as most modern Linux systems support it
        return true;
    }

    NetworkInterface LinuxTimestampProvider::convert_ifaddr_info(const struct ifaddrs& ifaddr) const {
        NetworkInterface interface;
        interface.name = ifaddr.ifa_name;
        
        // Get MAC address
        interface.mac_address = get_interface_mac_address(interface.name);
        
        // Check if interface is up
        interface.is_active = (ifaddr.ifa_flags & IFF_UP) && (ifaddr.ifa_flags & IFF_RUNNING);
        
        // Get timestamp capabilities for this interface
        auto caps_result = const_cast<LinuxTimestampProvider*>(this)->get_timestamp_capabilities(interface.name);
        if (caps_result.is_success()) {
            interface.capabilities = caps_result.value();
        }
        
        return interface;
    }

    std::string LinuxTimestampProvider::get_interface_mac_address(const InterfaceName& interface_name) const {
        if (socket_fd_ < 0) {
            return "";
        }

        struct ifreq ifr;
        std::memset(&ifr, 0, sizeof(ifr));
        
        // Safely copy interface name
        if (interface_name.length() >= IFNAMSIZ) {
            return ""; // Interface name too long
        }
        std::strncpy(ifr.ifr_name, interface_name.c_str(), IFNAMSIZ - 1);
        ifr.ifr_name[IFNAMSIZ - 1] = '\0'; // Ensure null termination

        if (ioctl(socket_fd_, SIOCGIFHWADDR, &ifr) < 0) {
            return "";
        }

        // Convert MAC address to string format
        std::stringstream mac_stream;
        const unsigned char* mac = reinterpret_cast<const unsigned char*>(ifr.ifr_hwaddr.sa_data);
        
        for (int i = 0; i < 6; ++i) {
            if (i > 0) {
                mac_stream << ":";
            }
            mac_stream << std::hex << std::setw(2) << std::setfill('0') 
                      << static_cast<unsigned int>(mac[i]);
        }
        
        return mac_stream.str();
    }

    ErrorCode LinuxTimestampProvider::map_linux_error(int errno_value) {
        switch (errno_value) {
            case 0:
                return ErrorCode::SUCCESS;
            case ENODEV:
            case ENXIO:
                return ErrorCode::INTERFACE_NOT_FOUND;
            case EOPNOTSUPP:
            case ENOSYS:
                return ErrorCode::TIMESTAMPING_NOT_SUPPORTED;
            case EINVAL:
                return ErrorCode::INVALID_PARAMETER;
            case EACCES:
            case EPERM:
                return ErrorCode::INSUFFICIENT_PRIVILEGES;
            default:
                return ErrorCode::NETWORK_ERROR;
        }
    }

} // namespace gptp

#endif // __linux__
