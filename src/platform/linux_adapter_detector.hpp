#pragma once

#include "../core/timestamp_provider.hpp"

#ifdef __linux__

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <linux/pci.h>
#include <map>
#include <vector>
#include <string>

namespace gptp {

    /**
     * @brief Intel Ethernet adapter specific detection and capabilities for Linux
     */
    struct LinuxIntelAdapterInfo {
        std::string device_name;
        std::string pci_device_id;
        std::string pci_vendor_id;
        std::string driver_name;
        std::string driver_version;
        std::string firmware_version;
        std::string bus_info;  // PCI bus information
        bool supports_hardware_timestamping = false;
        bool supports_ieee_1588 = false;
        bool supports_802_1as = false;
        bool is_intel_controller = false;
        std::string controller_family; // I210, I225, I226, etc.
        
        // Linux-specific ethtool capabilities
        bool supports_so_timestamping = false;
        bool supports_raw_hardware_timestamp = false;
        bool supports_filter_none = false;
        bool supports_filter_all = false;
    };

    /**
     * @brief Enhanced Linux adapter detector for Intel Ethernet controllers
     */
    class LinuxAdapterDetector {
    public:
        LinuxAdapterDetector();
        ~LinuxAdapterDetector();

        /**
         * @brief Initialize the adapter detector
         */
        Result<bool> initialize();

        /**
         * @brief Cleanup resources
         */
        void cleanup();

        /**
         * @brief Detect all Intel Ethernet adapters in the system
         * @return Vector of LinuxIntelAdapterInfo structures
         */
        Result<std::vector<LinuxIntelAdapterInfo>> detect_intel_adapters();

        /**
         * @brief Get detailed adapter information for a specific interface
         * @param interface_name Network interface name
         * @return LinuxIntelAdapterInfo structure with detailed information
         */
        Result<LinuxIntelAdapterInfo> get_adapter_info(const InterfaceName& interface_name);

        /**
         * @brief Check if adapter supports specific timestamping features using ethtool
         * @param interface_name Network interface name
         * @return Enhanced TimestampCapabilities with Intel-specific features
         */
        Result<TimestampCapabilities> get_ethtool_timestamp_capabilities(const InterfaceName& interface_name);

        /**
         * @brief Get Intel-specific capabilities based on adapter info
         * @param adapter_info Adapter information structure
         * @return Enhanced TimestampCapabilities with Intel-specific features
         */
        TimestampCapabilities get_intel_timestamp_capabilities(const LinuxIntelAdapterInfo& adapter_info);

    private:
        bool initialized_;
        int socket_fd_;
        
        /**
         * @brief Map of known Intel device IDs to controller families
         */
        static const std::map<std::string, std::string> INTEL_DEVICE_ID_MAP;

        /**
         * @brief Get PCI device information from sysfs
         * @param interface_name Network interface name
         * @return PCI device and vendor IDs, bus info
         */
        Result<std::tuple<std::string, std::string, std::string>> get_pci_info_from_sysfs(
            const InterfaceName& interface_name);

        /**
         * @brief Query driver information using ethtool
         * @param interface_name Network interface name
         * @return Driver name, version, and firmware version
         */
        Result<std::tuple<std::string, std::string, std::string>> get_driver_info_ethtool(
            const InterfaceName& interface_name);

        /**
         * @brief Get timestamping info using ethtool SIOCETHTOOL
         * @param interface_name Network interface name
         * @return Timestamping capabilities structure
         */
        Result<struct ethtool_ts_info> get_ethtool_ts_info(const InterfaceName& interface_name);

        /**
         * @brief Determine controller family from device ID
         * @param device_id PCI device ID
         * @return Controller family name
         */
        std::string determine_controller_family(const std::string& device_id);

        /**
         * @brief Check if device ID belongs to Intel
         * @param vendor_id PCI vendor ID
         * @param device_id PCI device ID
         * @return true if Intel device
         */
        bool is_intel_device(const std::string& vendor_id, const std::string& device_id);

        /**
         * @brief Read string from sysfs file
         * @param file_path Path to sysfs file
         * @return File contents as string
         */
        std::string read_sysfs_string(const std::string& file_path);

        /**
         * @brief Convert ethtool timestamping capabilities to our format
         * @param ts_info ethtool timestamp info structure
         * @return TimestampCapabilities structure
         */
        TimestampCapabilities convert_ethtool_capabilities(const struct ethtool_ts_info& ts_info);
    };

} // namespace gptp

#endif // __linux__
