#pragma once

#include "../core/timestamp_provider.hpp"

#ifdef _WIN32
#include "windows_adapter_detector.hpp"
#endif

#ifdef __linux__
#include "linux_adapter_detector.hpp"
#endif

namespace gptp {

    /**
     * @brief Platform-agnostic Intel adapter detection and enhanced capabilities
     */
    class IntelAdapterManager {
    public:
        IntelAdapterManager();
        ~IntelAdapterManager();

        /**
         * @brief Initialize the adapter manager
         */
        Result<bool> initialize();

        /**
         * @brief Cleanup resources
         */
        void cleanup();

        /**
         * @brief Detect all Intel Ethernet adapters with enhanced capabilities
         * @return Vector of NetworkInterface with enhanced Intel-specific information
         */
        Result<std::vector<NetworkInterface>> get_intel_capable_interfaces();

        /**
         * @brief Get enhanced timestamp capabilities for Intel adapters
         * @param interface_name Network interface name
         * @return Enhanced TimestampCapabilities with Intel-specific features
         */
        Result<TimestampCapabilities> get_enhanced_timestamp_capabilities(
            const InterfaceName& interface_name);

        /**
         * @brief Check if interface is an Intel controller with gPTP support
         * @param interface_name Network interface name
         * @return true if Intel controller with gPTP/PTP support
         */
        bool is_intel_gptp_capable(const InterfaceName& interface_name);

        /**
         * @brief Get Intel controller family name
         * @param interface_name Network interface name
         * @return Controller family (I210, I225, I226, etc.)
         */
        std::string get_controller_family(const InterfaceName& interface_name);

        /**
         * @brief Get recommended configuration for Intel controller
         * @param interface_name Network interface name
         * @return Configuration recommendations for optimal gPTP performance
         */
        struct IntelConfigRecommendations {
            bool enable_hardware_timestamping = true;
            bool enable_ptp_hardware_clock = true;
            int recommended_sync_interval_ms = 125;
            int recommended_announce_interval_ms = 1000;
            int recommended_pdelay_interval_ms = 1000;
            bool use_tagged_transmit = true;
            std::string optimal_driver_version;
            std::vector<std::string> required_kernel_modules;
        };

        Result<IntelConfigRecommendations> get_configuration_recommendations(
            const InterfaceName& interface_name);

    private:
        bool initialized_;

#ifdef _WIN32
        std::unique_ptr<WindowsAdapterDetector> windows_detector_;
#endif

#ifdef __linux__
        std::unique_ptr<LinuxAdapterDetector> linux_detector_;
#endif

        /**
         * @brief Convert platform-specific adapter info to NetworkInterface
         */
#ifdef _WIN32
        NetworkInterface convert_to_network_interface(const IntelAdapterInfo& adapter_info);
#endif

#ifdef __linux__
        NetworkInterface convert_to_network_interface(const LinuxIntelAdapterInfo& adapter_info);
#endif

        /**
         * @brief Map controller family to recommended configuration
         * @param controller_family Controller family name
         * @return Configuration recommendations
         */
        IntelConfigRecommendations get_family_specific_recommendations(
            const std::string& controller_family);
    };

} // namespace gptp
