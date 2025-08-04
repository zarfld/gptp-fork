#pragma once

#include "../core/timestamp_provider.hpp"

#ifdef _WIN32

#ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0A00 // Windows 10
#endif

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <Windows.h>
#include <iphlpapi.h>
#include <IPTypes.h>
#include <ipifcons.h>
#include <winapifamily.h>

#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

namespace gptp {

    /**
     * @brief Windows-specific implementation of timestamp provider
     * 
     * This class implements the ITimestampProvider interface using Windows-specific
     * APIs like IP Helper API for network interface enumeration and timestamping
     * capabilities detection.
     */
    class WindowsTimestampProvider : public ITimestampProvider {
    public:
        WindowsTimestampProvider();
        ~WindowsTimestampProvider() override;

        // Delete copy constructor and assignment to prevent issues with RAII
        WindowsTimestampProvider(const WindowsTimestampProvider&) = delete;
        WindowsTimestampProvider& operator=(const WindowsTimestampProvider&) = delete;

        // Allow move semantics
        WindowsTimestampProvider(WindowsTimestampProvider&&) noexcept = default;
        WindowsTimestampProvider& operator=(WindowsTimestampProvider&&) noexcept = default;

        Result<TimestampCapabilities> get_timestamp_capabilities(
            const InterfaceName& interface_name) override;

        Result<std::vector<NetworkInterface>> get_network_interfaces() override;

        Result<bool> initialize() override;

        void cleanup() override;

        bool is_hardware_timestamping_available() const override;

    private:
        bool initialized_;

        /**
         * @brief Convert Windows network interface to our NetworkInterface struct
         * @param adapter_info Windows IP_ADAPTER_INFO structure
         * @return NetworkInterface object
         */
        NetworkInterface convert_adapter_info(const IP_ADAPTER_INFO& adapter_info) const;

        /**
         * @brief Get NET_LUID for a given interface name
         * @param interface_name Name of the network interface
         * @return Result containing NET_LUID or error code
         */
        Result<NET_LUID> get_interface_luid(const InterfaceName& interface_name) const;

        /**
         * @brief Convert Windows timestamp capabilities to our format
         * @param win_caps Windows INTERFACE_TIMESTAMP_CAPABILITIES
         * @return TimestampCapabilities object
         */
        TimestampCapabilities convert_timestamp_capabilities(
            const INTERFACE_TIMESTAMP_CAPABILITIES& win_caps) const;

        /**
         * @brief Get detailed error message for Windows error codes
         * @param error_code Windows error code
         * @return Corresponding ErrorCode enum value
         */
        ErrorCode map_windows_error(DWORD error_code) const;
    };

} // namespace gptp

#endif // _WIN32
