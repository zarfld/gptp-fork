#pragma once

#include "../../include/gptp_types.hpp"
#include <vector>
#include <memory>

namespace gptp {

    /**
     * @brief Abstract interface for platform-specific timestamping operations
     * 
     * This interface provides platform-agnostic access to network timestamping
     * capabilities, abstracting away the differences between Windows and Linux
     * implementations.
     */
    class ITimestampProvider {
    public:
        virtual ~ITimestampProvider() = default;

        /**
         * @brief Get timestamping capabilities for a specific network interface
         * @param interface_name Name of the network interface
         * @return Result containing TimestampCapabilities or error code
         */
        virtual Result<TimestampCapabilities> get_timestamp_capabilities(
            const InterfaceName& interface_name) = 0;

        /**
         * @brief Get information about all available network interfaces
         * @return Result containing vector of NetworkInterface or error code
         */
        virtual Result<std::vector<NetworkInterface>> get_network_interfaces() = 0;

        /**
         * @brief Initialize the timestamping provider
         * @return Result indicating success or error code
         */
        virtual Result<bool> initialize() = 0;

        /**
         * @brief Cleanup resources used by the timestamping provider
         */
        virtual void cleanup() = 0;

        /**
         * @brief Check if hardware timestamping is available on the system
         * @return true if hardware timestamping is supported
         */
        virtual bool is_hardware_timestamping_available() const = 0;
    };

    /**
     * @brief Factory function to create platform-specific timestamp provider
     * @return Unique pointer to platform-specific implementation
     */
    std::unique_ptr<ITimestampProvider> create_timestamp_provider();

} // namespace gptp
