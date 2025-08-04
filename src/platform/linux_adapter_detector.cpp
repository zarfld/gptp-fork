#include "linux_adapter_detector.hpp"

#ifdef __linux__

#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <unistd.h>
#include <dirent.h>

namespace gptp {

    // Known Intel Ethernet Controller Device IDs (same as Windows)
    const std::map<std::string, std::string> LinuxAdapterDetector::INTEL_DEVICE_ID_MAP = {
        // I210 Family
        {"1531", "I210"},  // Not programmed/factory default
        {"1533", "I210"},  // WGI210AT/WGI210IT (copper only)
        {"1536", "I210"},  // WGI210IS (fiber, industrial temperature)
        {"1537", "I210"},  // WGI210IS (1000BASE-KX/BX backplane)
        {"1538", "I210"},  // WGI210IS (external SGMII, industrial temperature)
        {"157b", "I210"},  // WGI210AT/WGI210IT (Flash-less copper only)
        {"157c", "I210"},  // WGI210IS/WGI210CS (Flash-less 1000BASE-KX/BX backplane)
        {"15f6", "I210"},  // WGI210CS/WGI210CL (Flash-less SGMII)
        
        // I225 Family
        {"15f2", "I225"},  // I225-LM/V (estimated)
        {"15f3", "I225"},  // I225-IT (estimated)
        
        // I226 Family
        {"125b", "I226"},  // I226-LM/V (estimated)
        {"125c", "I226"},  // I226-IT (estimated)
        
        // Other Intel Ethernet Controllers
        {"10ea", "82577"}, // Intel 82577LM Gigabit Network Connection
        {"10eb", "82577"}, // Intel 82577LC Gigabit Network Connection
        
        // I350 Family - Comprehensive device ID mapping
        {"1521", "I350"},  // Intel I350 Gigabit Network Connection
        {"1522", "I350"},  // Intel I350 Gigabit Fiber Network Connection
        {"1523", "I350"},  // Intel I350 Gigabit Backplane Connection
        {"1524", "I350"},  // Intel I350 Gigabit Connection
        {"1525", "I350"},  // Intel I350 Virtual Function
        {"1526", "I350"},  // Intel I350 Virtual Function
        {"1527", "I350"},  // Intel I350 Virtual Function
        {"1528", "I350"},  // Intel I350 Virtual Function
        
        // I219 Family - Integrated Ethernet Controllers
        {"0dc7", "I219"},  // Intel I219-LM (22) Gigabit Network Connection
        {"15b7", "I219"},  // Intel I219-LM Gigabit Network Connection
        {"15b8", "I219"},  // Intel I219-V2 LM Gigabit Network Connection
        {"15b9", "I219"},  // Intel I219-LM2 Gigabit Network Connection
        {"15bb", "I219"},  // Intel I219-V Gigabit Network Connection
        {"15bc", "I219"},  // Intel I219-LM3 Gigabit Network Connection
        {"15bd", "I219"},  // Intel I219-V2 Gigabit Network Connection
        {"15be", "I219"},  // Intel I219-V3 Gigabit Network Connection
        {"15d6", "I219"},  // Intel I219-LM4 Gigabit Network Connection
        {"15d7", "I219"},  // Intel I219-V4 Gigabit Network Connection
        {"15d8", "I219"},  // Intel I219-V5 Gigabit Network Connection
        {"15e3", "I219"},  // Intel I219-LM6 Gigabit Network Connection
        {"15e7", "I219"},  // Intel I219-LM7 Gigabit Network Connection
        {"15e8", "I219"},  // Intel I219-V7 Gigabit Network Connection
        {"15f4", "I219"},  // Intel I219-LM8 Gigabit Network Connection
        {"15f5", "I219"},  // Intel I219-V8 Gigabit Network Connection
        {"15f6", "I219"},  // Intel I219-LM9 Gigabit Network Connection
        {"15f7", "I219"},  // Intel I219-V9 Gigabit Network Connection
        {"1a1c", "I219"},  // Intel I219-LM10 Gigabit Network Connection
        {"1a1d", "I219"},  // Intel I219-V10 Gigabit Network Connection
        {"1a1e", "I219"},  // Intel I219-LM11 Gigabit Network Connection
        {"1a1f", "I219"},  // Intel I219-V11 Gigabit Network Connection
        
        // E810 Family - High Performance Controllers
        {"1593", "E810"},  // Intel E810-CAM2 100Gb 2-port
        {"1594", "E810"},  // Intel E810-CAM1 100Gb 1-port
        {"1595", "E810"},  // Intel E810-XXVAM2 25Gb 2-port
        {"1596", "E810"},  // Intel E810 Virtual Function
        {"1597", "E810"},  // Intel E810 Virtual Function
        {"1598", "E810"},  // Intel E810 Management Interface
        {"1599", "E810"},  // Intel E810 Backplane Connection
        {"159a", "E810"},  // Intel E810 SFP Connection
        {"159b", "E810"},  // Intel E810 QSFP Connection
        {"1891", "E810"},  // Intel E810 PCIe Virtual Function
        {"1892", "E810"},  // Intel E810 Adaptive Virtual Function
        {"1893", "E810"},  // Intel E810 SR-IOV Virtual Function
    };

    LinuxAdapterDetector::LinuxAdapterDetector() : initialized_(false), socket_fd_(-1) {
    }

    LinuxAdapterDetector::~LinuxAdapterDetector() {
        cleanup();
    }

    Result<bool> LinuxAdapterDetector::initialize() {
        if (initialized_) {
            return Result<bool>(true);
        }

        // Create a socket for ioctl operations
        socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_fd_ < 0) {
            return Result<bool>(ErrorCode::NETWORK_ERROR);
        }

        initialized_ = true;
        return Result<bool>(true);
    }

    void LinuxAdapterDetector::cleanup() {
        if (socket_fd_ >= 0) {
            close(socket_fd_);
            socket_fd_ = -1;
        }
        initialized_ = false;
    }

    Result<std::vector<LinuxIntelAdapterInfo>> LinuxAdapterDetector::detect_intel_adapters() {
        if (!initialized_) {
            return Result<std::vector<LinuxIntelAdapterInfo>>(ErrorCode::INITIALIZATION_FAILED);
        }

        std::vector<LinuxIntelAdapterInfo> adapters;
        
        struct ifaddrs *ifaddr;
        if (getifaddrs(&ifaddr) == -1) {
            return Result<std::vector<LinuxIntelAdapterInfo>>(ErrorCode::NETWORK_ERROR);
        }

        for (struct ifaddrs *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_INET) {
                continue;
            }

            std::string interface_name = ifa->ifa_name;
            
            // Skip loopback interface
            if (interface_name == "lo") {
                continue;
            }

            auto pci_info_result = get_pci_info_from_sysfs(interface_name);
            if (pci_info_result.has_error()) {
                continue; // Skip non-PCI interfaces
            }

            auto [vendor_id, device_id, bus_info] = pci_info_result.value();

            if (is_intel_device(vendor_id, device_id)) {
                LinuxIntelAdapterInfo adapter_info;
                adapter_info.device_name = interface_name;
                adapter_info.pci_vendor_id = vendor_id;
                adapter_info.pci_device_id = device_id;
                adapter_info.bus_info = bus_info;
                adapter_info.is_intel_controller = true;
                adapter_info.controller_family = determine_controller_family(device_id);

                // Get driver information
                auto driver_info_result = get_driver_info_ethtool(interface_name);
                if (driver_info_result.is_success()) {
                    auto [driver_name, driver_version, firmware_version] = driver_info_result.value();
                    adapter_info.driver_name = driver_name;
                    adapter_info.driver_version = driver_version;
                    adapter_info.firmware_version = firmware_version;
                }

                // Set capabilities based on controller family
                if (adapter_info.controller_family == "I210" || 
                    adapter_info.controller_family == "I225" || 
                    adapter_info.controller_family == "I226") {
                    adapter_info.supports_hardware_timestamping = true;
                    adapter_info.supports_ieee_1588 = true;
                    adapter_info.supports_802_1as = (adapter_info.controller_family != "I210");
                }

                // Get ethtool timestamping capabilities
                auto ts_info_result = get_ethtool_ts_info(interface_name);
                if (ts_info_result.is_success()) {
                    const auto& ts_info = ts_info_result.value();
                    adapter_info.supports_so_timestamping = (ts_info.so_timestamping != 0);
                    adapter_info.supports_raw_hardware_timestamp = 
                        (ts_info.so_timestamping & SOF_TIMESTAMPING_RAW_HARDWARE) != 0;
                }

                adapters.push_back(adapter_info);
            }
        }

        freeifaddrs(ifaddr);
        return Result<std::vector<LinuxIntelAdapterInfo>>(adapters);
    }

    Result<LinuxIntelAdapterInfo> LinuxAdapterDetector::get_adapter_info(const InterfaceName& interface_name) {
        if (!initialized_) {
            return Result<LinuxIntelAdapterInfo>(ErrorCode::INITIALIZATION_FAILED);
        }

        auto pci_info_result = get_pci_info_from_sysfs(interface_name);
        if (pci_info_result.has_error()) {
            return Result<LinuxIntelAdapterInfo>(pci_info_result.error());
        }

        auto [vendor_id, device_id, bus_info] = pci_info_result.value();

        if (!is_intel_device(vendor_id, device_id)) {
            return Result<LinuxIntelAdapterInfo>(ErrorCode::INTERFACE_NOT_FOUND);
        }

        LinuxIntelAdapterInfo adapter_info;
        adapter_info.device_name = interface_name;
        adapter_info.pci_vendor_id = vendor_id;
        adapter_info.pci_device_id = device_id;
        adapter_info.bus_info = bus_info;
        adapter_info.is_intel_controller = true;
        adapter_info.controller_family = determine_controller_family(device_id);

        // Get driver information
        auto driver_info_result = get_driver_info_ethtool(interface_name);
        if (driver_info_result.is_success()) {
            auto [driver_name, driver_version, firmware_version] = driver_info_result.value();
            adapter_info.driver_name = driver_name;
            adapter_info.driver_version = driver_version;
            adapter_info.firmware_version = firmware_version;
        }

        return Result<LinuxIntelAdapterInfo>(adapter_info);
    }

    Result<TimestampCapabilities> LinuxAdapterDetector::get_ethtool_timestamp_capabilities(
        const InterfaceName& interface_name) {
        
        auto ts_info_result = get_ethtool_ts_info(interface_name);
        if (ts_info_result.has_error()) {
            return Result<TimestampCapabilities>(ts_info_result.error());
        }

        const auto& ts_info = ts_info_result.value();
        return Result<TimestampCapabilities>(convert_ethtool_capabilities(ts_info));
    }

    TimestampCapabilities LinuxAdapterDetector::get_intel_timestamp_capabilities(
        const LinuxIntelAdapterInfo& adapter_info) {
        
        TimestampCapabilities caps;
        
        if (adapter_info.is_intel_controller) {
            // Intel-specific capability detection
            if (adapter_info.controller_family == "I210") {
                caps.hardware_timestamping_supported = true;
                caps.software_timestamping_supported = true;
                caps.transmit_timestamping = true;
                caps.receive_timestamping = true;
                caps.tagged_transmit = true;
                caps.all_transmit = false;  // I210 typically doesn't support all transmit
                caps.all_receive = true;
            }
            else if (adapter_info.controller_family == "I225" || 
                     adapter_info.controller_family == "I226") {
                caps.hardware_timestamping_supported = true;
                caps.software_timestamping_supported = true;
                caps.transmit_timestamping = true;
                caps.receive_timestamping = true;
                caps.tagged_transmit = true;
                caps.all_transmit = true;   // I225/I226 support all transmit timestamping
                caps.all_receive = true;
            }
        }
        
        return caps;
    }

    Result<std::tuple<std::string, std::string, std::string>> LinuxAdapterDetector::get_pci_info_from_sysfs(
        const InterfaceName& interface_name) {
        
        // Read PCI information from sysfs
        std::string sysfs_path = "/sys/class/net/" + interface_name + "/device/";
        
        std::string vendor_id = read_sysfs_string(sysfs_path + "vendor");
        std::string device_id = read_sysfs_string(sysfs_path + "device");
        std::string bus_info;
        
        // Remove "0x" prefix if present
        if (vendor_id.substr(0, 2) == "0x") {
            vendor_id = vendor_id.substr(2);
        }
        if (device_id.substr(0, 2) == "0x") {
            device_id = device_id.substr(2);
        }

        // Get bus info (PCI slot)
        char real_path[PATH_MAX];
        std::string device_link = "/sys/class/net/" + interface_name + "/device";
        if (realpath(device_link.c_str(), real_path)) {
            std::string full_path = real_path;
            size_t last_slash = full_path.find_last_of('/');
            if (last_slash != std::string::npos) {
                bus_info = full_path.substr(last_slash + 1);
            }
        }

        if (vendor_id.empty() || device_id.empty()) {
            return Result<std::tuple<std::string, std::string, std::string>>(
                ErrorCode::INTERFACE_NOT_FOUND);
        }

        return Result<std::tuple<std::string, std::string, std::string>>(
            std::make_tuple(vendor_id, device_id, bus_info));
    }

    Result<std::tuple<std::string, std::string, std::string>> LinuxAdapterDetector::get_driver_info_ethtool(
        const InterfaceName& interface_name) {
        
        if (socket_fd_ < 0) {
            return Result<std::tuple<std::string, std::string, std::string>>(
                ErrorCode::INITIALIZATION_FAILED);
        }

        struct ethtool_drvinfo drvinfo;
        struct ifreq ifr;

        memset(&drvinfo, 0, sizeof(drvinfo));
        memset(&ifr, 0, sizeof(ifr));

        drvinfo.cmd = ETHTOOL_GDRVINFO;
        strncpy(ifr.ifr_name, interface_name.c_str(), IFNAMSIZ - 1);
        ifr.ifr_data = reinterpret_cast<char*>(&drvinfo);

        if (ioctl(socket_fd_, SIOCETHTOOL, &ifr) < 0) {
            return Result<std::tuple<std::string, std::string, std::string>>(
                ErrorCode::NETWORK_ERROR);
        }

        return Result<std::tuple<std::string, std::string, std::string>>(
            std::make_tuple(
                std::string(drvinfo.driver),
                std::string(drvinfo.version),
                std::string(drvinfo.fw_version)
            ));
    }

    Result<struct ethtool_ts_info> LinuxAdapterDetector::get_ethtool_ts_info(
        const InterfaceName& interface_name) {
        
        if (socket_fd_ < 0) {
            return Result<struct ethtool_ts_info>(ErrorCode::INITIALIZATION_FAILED);
        }

        struct ethtool_ts_info ts_info;
        struct ifreq ifr;

        memset(&ts_info, 0, sizeof(ts_info));
        memset(&ifr, 0, sizeof(ifr));

        ts_info.cmd = ETHTOOL_GET_TS_INFO;
        strncpy(ifr.ifr_name, interface_name.c_str(), IFNAMSIZ - 1);
        ifr.ifr_data = reinterpret_cast<char*>(&ts_info);

        if (ioctl(socket_fd_, SIOCETHTOOL, &ifr) < 0) {
            return Result<struct ethtool_ts_info>(ErrorCode::TIMESTAMPING_NOT_SUPPORTED);
        }

        return Result<struct ethtool_ts_info>(ts_info);
    }

    std::string LinuxAdapterDetector::determine_controller_family(const std::string& device_id) {
        auto it = INTEL_DEVICE_ID_MAP.find(device_id);
        if (it != INTEL_DEVICE_ID_MAP.end()) {
            return it->second;
        }
        return "Unknown";
    }

    bool LinuxAdapterDetector::is_intel_device(const std::string& vendor_id, const std::string& device_id) {
        return (vendor_id == "8086" && INTEL_DEVICE_ID_MAP.find(device_id) != INTEL_DEVICE_ID_MAP.end());
    }

    std::string LinuxAdapterDetector::read_sysfs_string(const std::string& file_path) {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            return "";
        }

        std::string content;
        std::getline(file, content);
        
        // Remove trailing newline
        if (!content.empty() && content.back() == '\n') {
            content.pop_back();
        }
        
        return content;
    }

    TimestampCapabilities LinuxAdapterDetector::convert_ethtool_capabilities(
        const struct ethtool_ts_info& ts_info) {
        
        TimestampCapabilities caps;
        
        // Check SO_TIMESTAMPING flags
        caps.software_timestamping_supported = 
            (ts_info.so_timestamping & SOF_TIMESTAMPING_SOFTWARE) != 0;
        caps.hardware_timestamping_supported = 
            (ts_info.so_timestamping & SOF_TIMESTAMPING_RAW_HARDWARE) != 0;
        
        // Check transmit timestamping
        caps.transmit_timestamping = 
            (ts_info.so_timestamping & SOF_TIMESTAMPING_TX_HARDWARE) != 0 ||
            (ts_info.so_timestamping & SOF_TIMESTAMPING_TX_SOFTWARE) != 0;
        
        // Check receive timestamping
        caps.receive_timestamping = 
            (ts_info.so_timestamping & SOF_TIMESTAMPING_RX_HARDWARE) != 0 ||
            (ts_info.so_timestamping & SOF_TIMESTAMPING_RX_SOFTWARE) != 0;
        
        // For Intel controllers, we can make educated guesses about specific capabilities
        caps.tagged_transmit = caps.transmit_timestamping;
        caps.all_transmit = caps.transmit_timestamping;
        caps.all_receive = caps.receive_timestamping;
        
        return caps;
    }

} // namespace gptp

#endif // __linux__
