#include "windows_timestamp_provider.hpp"

#ifdef _WIN32

#include <sstream>
#include <iomanip>

namespace gptp {

    WindowsTimestampProvider::WindowsTimestampProvider() : initialized_(false) {
    }

    WindowsTimestampProvider::~WindowsTimestampProvider() {
        // Don't call virtual function from destructor - cleanup directly
        if (initialized_) {
            WSACleanup();
            initialized_ = false;
        }
    }

    Result<bool> WindowsTimestampProvider::initialize() {
        if (initialized_) {
            return Result<bool>(true);
        }

        // Initialize Winsock for network operations
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            return Result<bool>(map_windows_error(static_cast<DWORD>(result)));
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

        auto luid_result = WindowsTimestampProvider::get_interface_luid(interface_name);
        if (luid_result.has_error()) {
            return Result<TimestampCapabilities>(luid_result.error());
        }

        NET_LUID interface_luid = luid_result.value();
        INTERFACE_TIMESTAMP_CAPABILITIES timestamp_caps = {0};

        DWORD result = GetInterfaceActiveTimestampCapabilities(&interface_luid, &timestamp_caps);
        if (result != NO_ERROR) {
            return Result<TimestampCapabilities>(WindowsTimestampProvider::map_windows_error(result));
        }

        return Result<TimestampCapabilities>(WindowsTimestampProvider::convert_timestamp_capabilities(timestamp_caps));
    }

    Result<std::vector<NetworkInterface>> WindowsTimestampProvider::get_network_interfaces() {
        if (!initialized_) {
            return Result<std::vector<NetworkInterface>>(ErrorCode::INITIALIZATION_FAILED);
        }

        std::vector<NetworkInterface> interfaces;
        
        // Get adapter info size
        ULONG buffer_size = 0;
        DWORD result = GetAdaptersInfo(nullptr, &buffer_size);
        
        if (result != ERROR_BUFFER_OVERFLOW) {
            return Result<std::vector<NetworkInterface>>(WindowsTimestampProvider::map_windows_error(result));
        }

        // Allocate buffer and get adapter info
        auto buffer = std::make_unique<char[]>(buffer_size);
        PIP_ADAPTER_INFO adapter_info = reinterpret_cast<PIP_ADAPTER_INFO>(buffer.get());
        
        result = GetAdaptersInfo(adapter_info, &buffer_size);
        if (result != NO_ERROR) {
            return Result<std::vector<NetworkInterface>>(WindowsTimestampProvider::map_windows_error(result));
        }

        // Convert adapter info to NetworkInterface objects
        PIP_ADAPTER_INFO current_adapter = adapter_info;
        while (current_adapter != nullptr) {
            interfaces.push_back(convert_adapter_info(*current_adapter));
            current_adapter = current_adapter->Next;
        }

        return Result<std::vector<NetworkInterface>>(interfaces);
    }

    bool WindowsTimestampProvider::is_hardware_timestamping_available() const {
        // This is a simplified check - in a real implementation, you might want to
        // check specific hardware capabilities or driver versions
        return true; // Assume hardware timestamping is available on Windows 10+
    }

    NetworkInterface WindowsTimestampProvider::convert_adapter_info(
        const IP_ADAPTER_INFO& adapter_info) const {
        
        NetworkInterface interface;
        interface.name = adapter_info.AdapterName;
        
        // Convert MAC address to string format
        std::stringstream mac_stream;
        for (UINT i = 0; i < adapter_info.AddressLength; ++i) {
            if (i > 0) {
                mac_stream << "-";
            }
            mac_stream << std::hex << std::setw(2) << std::setfill('0') 
                      << static_cast<int>(adapter_info.Address[i]);
        }
        interface.mac_address = mac_stream.str();
        
        // Check if interface is active
        interface.is_active = (adapter_info.Type != MIB_IF_TYPE_LOOPBACK);
        
        // Get timestamp capabilities for this interface
        auto caps_result = const_cast<WindowsTimestampProvider*>(this)->get_timestamp_capabilities(interface.name);
        if (caps_result.is_success()) {
            interface.capabilities = caps_result.value();
        }
        
        return interface;
    }

    Result<NET_LUID> WindowsTimestampProvider::get_interface_luid(
        const InterfaceName& interface_name) {
        
        NET_LUID luid = {0};
        
        // Try to convert interface name to LUID
        // This is simplified - in practice, you might need to enumerate interfaces
        // to match the name with the appropriate LUID
        
        // For now, return a zero LUID as a placeholder
        // In a real implementation, you would use ConvertInterfaceNameToLuidA
        // or similar functions
        
        return Result<NET_LUID>(luid);
    }

    TimestampCapabilities WindowsTimestampProvider::convert_timestamp_capabilities(
        const INTERFACE_TIMESTAMP_CAPABILITIES& win_caps) {
        
        TimestampCapabilities caps;
        
        // Convert hardware capabilities
        const auto& hw_caps = win_caps.HardwareCapabilities;
        caps.hardware_timestamping_supported = true; // If we got here, hardware is supported
        caps.transmit_timestamping = hw_caps.TaggedTransmit || hw_caps.AllTransmit;
        caps.receive_timestamping = hw_caps.AllReceive;
        caps.tagged_transmit = hw_caps.TaggedTransmit;
        caps.all_transmit = hw_caps.AllTransmit;
        caps.all_receive = hw_caps.AllReceive;
        
        // Convert software capabilities
        const auto& sw_caps = win_caps.SoftwareCapabilities;
        caps.software_timestamping_supported = sw_caps.TaggedTransmit || sw_caps.AllTransmit || sw_caps.AllReceive;
        
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
            case ERROR_NOT_FOUND:
                return ErrorCode::INTERFACE_NOT_FOUND;
            default:
                return ErrorCode::NETWORK_ERROR;
        }
    }

} // namespace gptp

#endif // _WIN32
