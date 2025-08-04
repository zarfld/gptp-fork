#pragma once

#ifdef _WIN32

#ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0A00 // Windows 10
#endif

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <map>
#include <vector>
#include <string>

#include "../../include/gptp_types.hpp"

#pragma comment(lib, "setupapi.lib")

namespace gptp {

    /**
     * @brief Intel Ethernet adapter specific detection and capabilities
     */
    struct IntelAdapterInfo {
        std::string device_name;
        std::string pci_device_id;
        std::string pci_vendor_id;
        std::string driver_version;
        std::string hardware_revision;
        bool supports_hardware_timestamping = false;
        bool supports_ieee_1588 = false;
        bool supports_802_1as = false;
        bool is_intel_controller = false;
        std::string controller_family; // I210, I225, I226, etc.
    };

    /**
     * @brief Enhanced Windows adapter detector for Intel Ethernet controllers
     */
    class WindowsAdapterDetector {
    public:
        WindowsAdapterDetector();
        ~WindowsAdapterDetector();

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
         * @return Vector of IntelAdapterInfo structures
         */
        Result<std::vector<IntelAdapterInfo>> detect_intel_adapters();

        /**
         * @brief Get detailed adapter information for a specific interface
         * @param interface_name Network interface name
         * @return IntelAdapterInfo structure with detailed information
         */
        Result<IntelAdapterInfo> get_adapter_info(const InterfaceName& interface_name);

        /**
         * @brief Check if adapter supports specific timestamping features
         * @param adapter_info Adapter information structure
         * @return Enhanced TimestampCapabilities with Intel-specific features
         */
        TimestampCapabilities get_intel_timestamp_capabilities(const IntelAdapterInfo& adapter_info);

    private:
        bool initialized_;
        
        /**
         * @brief Map of known Intel device IDs to controller families
         */
        static const std::map<std::string, std::string> INTEL_DEVICE_ID_MAP;

        /**
         * @brief Get PCI device information from registry
         * @param device_instance Device instance ID
         * @return PCI device and vendor IDs
         */
        std::pair<std::string, std::string> get_pci_ids_from_registry(const std::string& device_instance);

        /**
         * @brief Query driver version from registry
         * @param device_instance Device instance ID
         * @return Driver version string
         */
        std::string get_driver_version(const std::string& device_instance);

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
         * @brief Get network interface name from adapter GUID
         * @param adapter_guid Adapter GUID
         * @return Network interface name
         */
        std::string get_interface_name_from_guid(const std::string& adapter_guid);
    };

} // namespace gptp

#endif // _WIN32
