#include "windows_adapter_detector.hpp"

#ifdef _WIN32

namespace gptp {

    // Known Intel Ethernet Controller Device IDs
    const std::map<std::string, std::string> WindowsAdapterDetector::INTEL_DEVICE_ID_MAP = {
        // I210 Family
        {"1531", "I210"},  // Not programmed/factory default
        {"1533", "I210"},  // WGI210AT/WGI210IT (copper only)
        {"1536", "I210"},  // WGI210IS (fiber, industrial temperature)
        {"1537", "I210"},  // WGI210IS (1000BASE-KX/BX backplane)
        {"1538", "I210"},  // WGI210IS (external SGMII, industrial temperature)
        {"157B", "I210"},  // WGI210AT/WGI210IT (Flash-less copper only)
        {"157C", "I210"},  // WGI210IS/WGI210CS (Flash-less 1000BASE-KX/BX backplane)
        {"15F6", "I210"},  // WGI210CS/WGI210CL (Flash-less SGMII)
        
        // I225 Family (add known device IDs when available)
        {"15F2", "I225"},  // I225-LM/V (estimated)
        {"15F3", "I225"},  // I225-IT (estimated)
        
        // I226 Family (add known device IDs when available)
        {"125B", "I226"},  // I226-LM/V (estimated)
        {"125C", "I226"},  // I226-IT (estimated)
        
        // Other Intel Ethernet Controllers
        {"10EA", "82577"}, // Intel 82577LM Gigabit Network Connection
        {"10EB", "82577"}, // Intel 82577LC Gigabit Network Connection
        {"10EF", "82578"}, // Intel 82578DM Gigabit Network Connection
        {"10F0", "82578"}, // Intel 82578DC Gigabit Network Connection
        {"1502", "82579"}, // Intel 82579LM Gigabit Network Connection
        {"1503", "82579"}, // Intel 82579V Gigabit Network Connection
        {"1521", "I350"},  // Intel I350 Gigabit Network Connection
        {"1522", "I350"},  // Intel I350 Gigabit Fiber Network Connection
        {"1523", "I350"},  // Intel I350 Gigabit Backplane Connection
        {"1524", "I350"},  // Intel I350 Gigabit Connection
        {"1525", "I350"},  // Intel I350 Virtual Function
        {"1526", "I350"},  // Intel I350 Virtual Function
        {"1527", "I350"},  // Intel I350 Virtual Function
        {"1528", "I350"},  // Intel I350 Virtual Function
        
        // Intel I219 Family - Integrated Ethernet Controllers
        {"0DC7", "I219"},  // Intel I219-LM (22) Gigabit Network Connection
        {"15B7", "I219"},  // Intel I219-LM Gigabit Network Connection
        {"15B8", "I219"},  // Intel I219-V2 LM Gigabit Network Connection
        {"15B9", "I219"},  // Intel I219-LM2 Gigabit Network Connection
        {"15BB", "I219"},  // Intel I219-V Gigabit Network Connection
        {"15BC", "I219"},  // Intel I219-LM3 Gigabit Network Connection
        {"15BD", "I219"},  // Intel I219-V2 Gigabit Network Connection
        {"15BE", "I219"},  // Intel I219-V3 Gigabit Network Connection
        {"15D6", "I219"},  // Intel I219-LM4 Gigabit Network Connection
        {"15D7", "I219"},  // Intel I219-V4 Gigabit Network Connection
        {"15D8", "I219"},  // Intel I219-V5 Gigabit Network Connection
        {"15E3", "I219"},  // Intel I219-LM6 Gigabit Network Connection
        {"15E7", "I219"},  // Intel I219-LM7 Gigabit Network Connection
        {"15E8", "I219"},  // Intel I219-V7 Gigabit Network Connection
        {"15F4", "I219"},  // Intel I219-LM8 Gigabit Network Connection
        {"15F5", "I219"},  // Intel I219-V8 Gigabit Network Connection
        {"15F6", "I219"},  // Intel I219-LM9 Gigabit Network Connection
        {"15F7", "I219"},  // Intel I219-V9 Gigabit Network Connection
        {"1A1C", "I219"},  // Intel I219-LM10 Gigabit Network Connection
        {"1A1D", "I219"},  // Intel I219-V10 Gigabit Network Connection
        {"1A1E", "I219"},  // Intel I219-LM11 Gigabit Network Connection
        {"1A1F", "I219"},  // Intel I219-V11 Gigabit Network Connection
        
        // Intel E810 Family - High Performance Controllers
        {"1593", "E810"},  // Intel E810-CAM2 100Gb 2-port
        {"1594", "E810"},  // Intel E810-CAM1 100Gb 1-port
        {"1595", "E810"},  // Intel E810-XXVAM2 25Gb 2-port
        {"1596", "E810"},  // Intel E810 Virtual Function
        {"1597", "E810"},  // Intel E810 Virtual Function
        {"1598", "E810"},  // Intel E810 Management Interface
        {"1599", "E810"},  // Intel E810 Backplane Connection
        {"159A", "E810"},  // Intel E810 SFP Connection
        {"159B", "E810"},  // Intel E810 QSFP Connection
        {"1891", "E810"},  // Intel E810 PCIe Virtual Function
        {"1892", "E810"},  // Intel E810 Adaptive Virtual Function
        {"1893", "E810"},  // Intel E810 SR-IOV Virtual Function
    };

    WindowsAdapterDetector::WindowsAdapterDetector() : initialized_(false) {
    }

    WindowsAdapterDetector::~WindowsAdapterDetector() {
        cleanup();
    }

    Result<bool> WindowsAdapterDetector::initialize() {
        if (initialized_) {
            return Result<bool>(true);
        }

        initialized_ = true;
        return Result<bool>(true);
    }

    void WindowsAdapterDetector::cleanup() {
        initialized_ = false;
    }

    Result<std::vector<IntelAdapterInfo>> WindowsAdapterDetector::detect_intel_adapters() {
        if (!initialized_) {
            return Result<std::vector<IntelAdapterInfo>>(ErrorCode::INITIALIZATION_FAILED);
        }

        std::vector<IntelAdapterInfo> adapters;
        
        // Get device information set for network adapters
        HDEVINFO device_info_set = SetupDiGetClassDevs(
            &GUID_DEVCLASS_NET,
            nullptr,
            nullptr,
            DIGCF_PRESENT
        );

        if (device_info_set == INVALID_HANDLE_VALUE) {
            return Result<std::vector<IntelAdapterInfo>>(ErrorCode::NETWORK_ERROR);
        }

        SP_DEVINFO_DATA device_info_data;
        device_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

        for (DWORD device_index = 0; 
             SetupDiEnumDeviceInfo(device_info_set, device_index, &device_info_data); 
             device_index++) {
            
                // Get device instance ID
                char device_instance_id[MAX_PATH];
                if (SetupDiGetDeviceInstanceIdA(
                    device_info_set, 
                    &device_info_data, 
                    device_instance_id, 
                    sizeof(device_instance_id), 
                    nullptr)) {
                    
                    auto pci_ids = get_pci_ids_from_registry(device_instance_id);
                    std::string vendor_id = pci_ids.first;
                    std::string device_id = pci_ids.second;                if (is_intel_device(vendor_id, device_id)) {
                    IntelAdapterInfo adapter_info;
                    adapter_info.pci_vendor_id = vendor_id;
                    adapter_info.pci_device_id = device_id;
                    adapter_info.is_intel_controller = true;
                    adapter_info.controller_family = determine_controller_family(device_id);
                    adapter_info.driver_version = get_driver_version(device_instance_id);
                    
                    // Get device description
                    char device_desc[256];
                    if (SetupDiGetDeviceRegistryPropertyA(
                        device_info_set,
                        &device_info_data,
                        SPDRP_DEVICEDESC,
                        nullptr,
                        reinterpret_cast<PBYTE>(device_desc),
                        sizeof(device_desc),
                        nullptr)) {
                        adapter_info.device_name = device_desc;
                    }

                    // Set capabilities based on controller family
                    if (adapter_info.controller_family == "I210" || 
                        adapter_info.controller_family == "I219" ||
                        adapter_info.controller_family == "I225" || 
                        adapter_info.controller_family == "I226" ||
                        adapter_info.controller_family == "I350" ||
                        adapter_info.controller_family == "E810") {
                        adapter_info.supports_hardware_timestamping = true;
                        adapter_info.supports_ieee_1588 = true;
                        adapter_info.supports_802_1as = true; // All these controllers support 802.1AS
                    }

                    adapters.push_back(adapter_info);
                }
            }
        }

        SetupDiDestroyDeviceInfoList(device_info_set);
        return Result<std::vector<IntelAdapterInfo>>(adapters);
    }

    Result<IntelAdapterInfo> WindowsAdapterDetector::get_adapter_info(const InterfaceName& interface_name) {
        auto adapters_result = detect_intel_adapters();
        if (adapters_result.has_error()) {
            return Result<IntelAdapterInfo>(adapters_result.error());
        }

        const auto& adapters = adapters_result.value();
        
        // For now, return the first adapter that matches
        // In a full implementation, you would match by interface name
        for (const auto& adapter : adapters) {
            // This is simplified - you would need to implement proper interface name matching
            return Result<IntelAdapterInfo>(adapter);
        }

        return Result<IntelAdapterInfo>(ErrorCode::INTERFACE_NOT_FOUND);
    }

    TimestampCapabilities WindowsAdapterDetector::get_intel_timestamp_capabilities(
        const IntelAdapterInfo& adapter_info) {
        
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

    std::pair<std::string, std::string> WindowsAdapterDetector::get_pci_ids_from_registry(
        const std::string& device_instance) {
        
        // Parse PCI device instance ID (format: PCI\VEN_xxxx&DEV_yyyy&...)
        std::string vendor_id, device_id;
        
        size_t ven_pos = device_instance.find("VEN_");
        size_t dev_pos = device_instance.find("DEV_");
        
        if (ven_pos != std::string::npos && dev_pos != std::string::npos) {
            vendor_id = device_instance.substr(ven_pos + 4, 4);
            device_id = device_instance.substr(dev_pos + 4, 4);
        }
        
        return {vendor_id, device_id};
    }

    std::string WindowsAdapterDetector::get_driver_version(const std::string& device_instance) {
        // Simplified implementation - would query registry for actual driver version
        return "Unknown";
    }

    std::string WindowsAdapterDetector::determine_controller_family(const std::string& device_id) {
        auto it = INTEL_DEVICE_ID_MAP.find(device_id);
        if (it != INTEL_DEVICE_ID_MAP.end()) {
            return it->second;
        }
        return "Unknown";
    }

    bool WindowsAdapterDetector::is_intel_device(const std::string& vendor_id, const std::string& device_id) {
        return (vendor_id == "8086" && INTEL_DEVICE_ID_MAP.find(device_id) != INTEL_DEVICE_ID_MAP.end());
    }

    std::string WindowsAdapterDetector::get_interface_name_from_guid(const std::string& adapter_guid) {
        // This would implement GUID to interface name mapping
        return "";
    }

} // namespace gptp

#endif // _WIN32
