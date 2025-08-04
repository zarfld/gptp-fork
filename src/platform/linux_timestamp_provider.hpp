#pragma once

#include "../core/timestamp_provider.hpp"

#ifdef __linux__

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <unistd.h>

namespace gptp {

    /**
     * @brief Linux-specific implementation of timestamp provider
     * 
     * This class implements the ITimestampProvider interface using Linux-specific
     * APIs like netlink sockets, ethtool, and SO_TIMESTAMPING for network interface
     * enumeration and timestamping capabilities detection.
     */
    class LinuxTimestampProvider : public ITimestampProvider {
    public:
        LinuxTimestampProvider();
        ~LinuxTimestampProvider() override;

        // Delete copy constructor and assignment to prevent issues with RAII
        LinuxTimestampProvider(const LinuxTimestampProvider&) = delete;
        LinuxTimestampProvider& operator=(const LinuxTimestampProvider&) = delete;

        // Allow move semantics
        LinuxTimestampProvider(LinuxTimestampProvider&&) noexcept = default;
        LinuxTimestampProvider& operator=(LinuxTimestampProvider&&) noexcept = default;

        Result<TimestampCapabilities> get_timestamp_capabilities(
            const InterfaceName& interface_name) override;

        Result<std::vector<NetworkInterface>> get_network_interfaces() override;

        Result<bool> initialize() override;

        void cleanup() override;

        bool is_hardware_timestamping_available() const override;

    private:
        bool initialized_;
        int socket_fd_;

        /**
         * @brief Convert Linux network interface to our NetworkInterface struct
         * @param ifaddr Linux ifaddrs structure
         * @return NetworkInterface object
         */
        NetworkInterface convert_ifaddr_info(const struct ifaddrs& ifaddr) const;

        /**
         * @brief Get hardware timestamping capabilities using ethtool
         * @param interface_name Name of the network interface
         * @return TimestampCapabilities object
         */
        TimestampCapabilities get_ethtool_timestamping_caps(const InterfaceName& interface_name) const;

        /**
         * @brief Get MAC address for a network interface
         * @param interface_name Name of the network interface
         * @return MAC address as string or empty string on error
         */
        std::string get_interface_mac_address(const InterfaceName& interface_name) const;

        /**
         * @brief Convert Linux error codes to our ErrorCode enum
         * @param errno_value Linux errno value
         * @return Corresponding ErrorCode enum value
         */
        ErrorCode map_linux_error(int errno_value) const;
    };

} // namespace gptp

#endif // __linux__
