/**
 * @file windows_timestamp_provider.cpp
 * @brief Windows-specific timestamp provider implementation
 */

#include "windows_timestamp_provider.hpp"

#ifdef _WIN32

#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace gptp {

    WindowsTimestampProvider::WindowsTimestampProvider() : initialized_(false) {
    }

    WindowsTimestampProvider::~WindowsTimestampProvider() {
        cleanup();
    }

    Result<bool> WindowsTimestampProvider::initialize() {
        if (initialized_) {
            return Result<bool>(true);
        }

        // Initialize Winsock
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            return Result<bool>(map_windows_error(result));
        }

        initialized_ = true;
        return Result<bool>(true);
    }

    void WindowsTimestampProvider::cleanup() {
        if (initialized_) {
            WSACleanup();
            initialized_ = false;
        }
    }

    Result<TimestampCapabilities> WindowsTimestampProvider::get_timestamp_capabilities(
        const InterfaceName& interface_name) {
        
        if (!initialized_) {
            return Result<TimestampCapabilities>(ErrorCode::INITIALIZATION_FAILED);
        }

        TimestampCapabilities caps;
        
        // Try to detect Intel hardware timestamping capabilities
        if (detect_intel_hardware_timestamping(interface_name, caps)) {
            return Result<TimestampCapabilities>(caps);
        }

        // Fallback to software timestamping
        caps.hardware_timestamping_supported = false;
        caps.software_timestamping_supported = true;
        caps.transmit_timestamping = true;
        caps.receive_timestamping = true;
        
        return Result<TimestampCapabilities>(caps);
    }

    bool WindowsTimestampProvider::detect_intel_hardware_timestamping(
        const InterfaceName& interface_name, TimestampCapabilities& capabilities) {
        
        // Query network adapter for IEEE 1588/802.1AS capabilities
        DWORD buffer_size = 0;
        
        // First call to get required buffer size
        DWORD result = GetAdaptersInfo(nullptr, &buffer_size);
        if (result != ERROR_BUFFER_OVERFLOW) {
            return false;
        }
        
        std::vector<uint8_t> buffer(buffer_size);
        PIP_ADAPTER_INFO adapter_info = reinterpret_cast<PIP_ADAPTER_INFO>(buffer.data());
        
        result = GetAdaptersInfo(adapter_info, &buffer_size);
        if (result != NO_ERROR) {
            return false;
        }
        
        // Search for the specified interface
        PIP_ADAPTER_INFO current_adapter = adapter_info;
        while (current_adapter) {
            std::string adapter_name(current_adapter->AdapterName);
            std::string description(current_adapter->Description);
            
            if (interface_name.find(adapter_name) != std::string::npos ||
                interface_name.find(description) != std::string::npos) {
                
                // Check if this is an Intel adapter with hardware timestamping
                if (is_intel_adapter_with_timestamping(current_adapter)) {
                    capabilities.hardware_timestamping_supported = true;
                    capabilities.software_timestamping_supported = true;
                    capabilities.transmit_timestamping = true;
                    capabilities.receive_timestamping = true;
                    capabilities.tagged_transmit = true;
                    capabilities.all_receive = true;
                    return true;
                }
            }
            
            current_adapter = current_adapter->Next;
        }
        
        return false;
    }

    bool WindowsTimestampProvider::is_intel_adapter_with_timestamping(PIP_ADAPTER_INFO adapter) {
        if (!adapter) {
            return false;
        }
        
        std::string description(adapter->Description);
        std::transform(description.begin(), description.end(), description.begin(), ::tolower);
        
        // Check for Intel Ethernet controllers known to support hardware timestamping
        std::vector<std::string> intel_timestamping_adapters;
        intel_timestamping_adapters.push_back("intel(r) ethernet controller i210");
        intel_timestamping_adapters.push_back("intel(r) ethernet controller i211");
        intel_timestamping_adapters.push_back("intel(r) ethernet controller i219");
        intel_timestamping_adapters.push_back("intel(r) ethernet controller i225");
        intel_timestamping_adapters.push_back("intel(r) ethernet controller i226");
        intel_timestamping_adapters.push_back("intel(r) ethernet connection i350");
        intel_timestamping_adapters.push_back("intel(r) ethernet controller e810");
        intel_timestamping_adapters.push_back("intel(r) ethernet connection x722");
        intel_timestamping_adapters.push_back("intel(r) 82580 gigabit network connection");
        
        for (size_t i = 0; i < intel_timestamping_adapters.size(); ++i) {
            if (description.find(intel_timestamping_adapters[i]) != std::string::npos) {
                return true;
            }
        }
        
        return false;
    }

    Result<std::vector<NetworkInterface>> WindowsTimestampProvider::get_network_interfaces() {
        if (!initialized_) {
            return Result<std::vector<NetworkInterface>>(ErrorCode::INITIALIZATION_FAILED);
        }

        std::vector<NetworkInterface> interfaces;
        
        DWORD buffer_size = 0;
        DWORD result = GetAdaptersInfo(nullptr, &buffer_size);
        
        if (result != ERROR_BUFFER_OVERFLOW) {
            return Result<std::vector<NetworkInterface>>(map_windows_error(result));
        }

        std::vector<uint8_t> buffer(buffer_size);
        PIP_ADAPTER_INFO adapter_info = reinterpret_cast<PIP_ADAPTER_INFO>(buffer.data());
        
        result = GetAdaptersInfo(adapter_info, &buffer_size);
        if (result != NO_ERROR) {
            return Result<std::vector<NetworkInterface>>(map_windows_error(result));
        }

        PIP_ADAPTER_INFO current_adapter = adapter_info;
        while (current_adapter) {
            interfaces.push_back(convert_adapter_info(*current_adapter));
            current_adapter = current_adapter->Next;
        }

        return Result<std::vector<NetworkInterface>>(std::move(interfaces));
    }

    bool WindowsTimestampProvider::is_hardware_timestamping_available() const {
        return initialized_;
    }

    NetworkInterface WindowsTimestampProvider::convert_adapter_info(
        const IP_ADAPTER_INFO& adapter_info) const {
        
        NetworkInterface interface;
        interface.name = std::string(adapter_info.AdapterName);
        
        // Copy MAC address
        if (adapter_info.AddressLength <= interface.mac_address.size()) {
            std::memcpy(interface.mac_address.data(), 
                       adapter_info.Address, 
                       adapter_info.AddressLength);
        }
        
        interface.is_active = (adapter_info.Type != MIB_IF_TYPE_LOOPBACK);
        
        // Get timestamp capabilities for this interface
        auto caps_result = const_cast<WindowsTimestampProvider*>(this)->get_timestamp_capabilities(interface.name);
        if (!caps_result.has_error()) {
            interface.capabilities = caps_result.value();
        }

        return interface;
    }

    Result<NET_LUID> WindowsTimestampProvider::get_interface_luid(
        const InterfaceName& interface_name) {
        
        NET_LUID luid = {0};
        DWORD result = ConvertInterfaceNameToLuidA(interface_name.c_str(), &luid);
        
        if (result != NO_ERROR) {
            return Result<NET_LUID>(map_windows_error(result));
        }
        
        return Result<NET_LUID>(luid);
    }

    TimestampCapabilities WindowsTimestampProvider::convert_timestamp_capabilities(
        const INTERFACE_TIMESTAMP_CAPABILITIES& win_caps) {
        
        TimestampCapabilities caps;
        caps.hardware_timestamping_supported = (win_caps.HardwareClockFrequencyHz > 0);
        caps.software_timestamping_supported = true; // Always supported
        caps.transmit_timestamping = true;
        caps.receive_timestamping = true;
        
        return caps;
    }

    ErrorCode WindowsTimestampProvider::map_windows_error(DWORD error_code) {
        switch (error_code) {
            case NO_ERROR:
                return ErrorCode::SUCCESS;
            case ERROR_NOT_SUPPORTED:
                return ErrorCode::TIMESTAMPING_NOT_SUPPORTED;
            case ERROR_INVALID_PARAMETER:
                return ErrorCode::INVALID_PARAMETER;
            case ERROR_ACCESS_DENIED:
                return ErrorCode::INSUFFICIENT_PRIVILEGES;
            default:
                return ErrorCode::NETWORK_ERROR;
        }
    }

} // namespace gptp

#endif // _WIN32
