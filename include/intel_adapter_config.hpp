/**
 * @file intel_adapter_config.hpp
 * @brief Configuration templates for Intel Ethernet controllers
 */

#pragma once

#include <string>
#include <vector>
#include <map>

namespace gptp {

    /**
     * @brief Intel controller-specific configuration profiles
     */
    struct IntelControllerProfile {
        std::string controller_family;
        std::string description;
        
        // Timestamping configuration
        bool hardware_timestamping_enabled = true;
        bool software_timestamping_fallback = true;
        bool tagged_transmit_only = true;
        
        // gPTP timing parameters (in milliseconds)
        int sync_interval_ms = 125;
        int announce_interval_ms = 1000;
        int pdelay_req_interval_ms = 1000;
        
        // Advanced features
        bool tsn_features_enabled = false;
        bool dual_clock_master_support = false;
        bool frame_preemption_support = false;
        bool time_aware_shaper_support = false;
        
        // Performance tuning
        int interrupt_coalescing_us = 0;  // 0 = disabled for low latency
        int rx_ring_size = 1024;
        int tx_ring_size = 1024;
        
        // Error handling
        int max_sync_loss_count = 5;
        int sync_timeout_ms = 5000;
        bool auto_recovery_enabled = true;
    };

    /**
     * @brief Intel Ethernet Controller Configuration Manager
     */
    class IntelControllerConfig {
    public:
        /**
         * @brief Get predefined configuration profile for Intel controller
         * @param controller_family Controller family (I210, I225, I226)
         * @return Configuration profile optimized for the controller
         */
        static IntelControllerProfile get_profile(const std::string& controller_family) {
            auto it = CONTROLLER_PROFILES.find(controller_family);
            if (it != CONTROLLER_PROFILES.end()) {
                return it->second;
            }
            return get_default_profile();
        }

        /**
         * @brief Get list of all supported controller families
         * @return Vector of supported controller family names
         */
        static std::vector<std::string> get_supported_families() {
            std::vector<std::string> families;
            for (const auto& pair : CONTROLLER_PROFILES) {
                families.push_back(pair.first);
            }
            return families;
        }

        /**
         * @brief Check if controller family supports advanced TSN features
         * @param controller_family Controller family name
         * @return true if TSN features are supported
         */
        static bool supports_tsn_features(const std::string& controller_family) {
            auto profile = get_profile(controller_family);
            return profile.tsn_features_enabled;
        }

    private:
        /**
         * @brief Predefined configuration profiles for Intel controllers
         */
        static const std::map<std::string, IntelControllerProfile> CONTROLLER_PROFILES;

        /**
         * @brief Get default configuration profile
         * @return Default profile for unknown controllers
         */
        static IntelControllerProfile get_default_profile() {
            IntelControllerProfile profile;
            profile.controller_family = "Unknown";
            profile.description = "Default configuration for unknown Intel controller";
            profile.hardware_timestamping_enabled = false;
            profile.software_timestamping_fallback = true;
            return profile;
        }
    };

    // Static configuration profiles
    const std::map<std::string, IntelControllerProfile> IntelControllerConfig::CONTROLLER_PROFILES = {
        {
            "I210",
            {
                .controller_family = "I210",
                .description = "Intel I210 Gigabit Ethernet Controller - Basic PTP support",
                .hardware_timestamping_enabled = true,
                .software_timestamping_fallback = true,
                .tagged_transmit_only = true,
                .sync_interval_ms = 125,          // Conservative for I210
                .announce_interval_ms = 1000,
                .pdelay_req_interval_ms = 1000,
                .tsn_features_enabled = false,    // No TSN support
                .dual_clock_master_support = false,
                .frame_preemption_support = false,
                .time_aware_shaper_support = false,
                .interrupt_coalescing_us = 50,    // Moderate coalescing
                .rx_ring_size = 512,
                .tx_ring_size = 512,
                .max_sync_loss_count = 5,
                .sync_timeout_ms = 5000,
                .auto_recovery_enabled = true,
            }
        },
        {
            "I225",
            {
                .controller_family = "I225",
                .description = "Intel I225 2.5G Ethernet Controller - Full TSN support",
                .hardware_timestamping_enabled = true,
                .software_timestamping_fallback = true,
                .tagged_transmit_only = false,    // Can timestamp all packets
                .sync_interval_ms = 31,           // More aggressive (32 messages/sec)
                .announce_interval_ms = 1000,
                .pdelay_req_interval_ms = 1000,
                .tsn_features_enabled = true,     // Full TSN support
                .dual_clock_master_support = true,
                .frame_preemption_support = true,
                .time_aware_shaper_support = true,
                .interrupt_coalescing_us = 0,     // No coalescing for best latency
                .rx_ring_size = 1024,
                .tx_ring_size = 1024,
                .max_sync_loss_count = 3,
                .sync_timeout_ms = 3000,
                .auto_recovery_enabled = true,
            }
        },
        {
            "I226",
            {
                .controller_family = "I226",
                .description = "Intel I226 2.5G Ethernet Controller - Enhanced TSN + Power Management",
                .hardware_timestamping_enabled = true,
                .software_timestamping_fallback = true,
                .tagged_transmit_only = false,    // Can timestamp all packets
                .sync_interval_ms = 31,           // More aggressive (32 messages/sec)
                .announce_interval_ms = 1000,
                .pdelay_req_interval_ms = 1000,
                .tsn_features_enabled = true,     // Full TSN support
                .dual_clock_master_support = true,
                .frame_preemption_support = true,
                .time_aware_shaper_support = true,
                .interrupt_coalescing_us = 0,     // No coalescing for best latency
                .rx_ring_size = 1024,
                .tx_ring_size = 1024,
                .max_sync_loss_count = 3,
                .sync_timeout_ms = 3000,
                .auto_recovery_enabled = true,
            }
        },
        {
            "I350",
            {
                .controller_family = "I350",
                .description = "Intel I350 Gigabit Ethernet Controller - IEEE 1588 v1/v2 with per-packet timestamping",
                .hardware_timestamping_enabled = true,
                .software_timestamping_fallback = true,
                .tagged_transmit_only = false,    // Per-packet timestamp support
                .sync_interval_ms = 125,          // Standard 8 messages/sec
                .announce_interval_ms = 1000,
                .pdelay_req_interval_ms = 1000,
                .tsn_features_enabled = false,    // Basic gPTP support only
                .dual_clock_master_support = false,
                .frame_preemption_support = false,
                .time_aware_shaper_support = false,
                .interrupt_coalescing_us = 25,    // Moderate coalescing for efficiency
                .rx_ring_size = 512,              // Moderate ring size
                .tx_ring_size = 512,
                .max_sync_loss_count = 5,
                .sync_timeout_ms = 5000,
                .auto_recovery_enabled = true,
            }
        },
        {
            "I219",
            {
                .controller_family = "I219",
                .description = "Intel I219 Integrated Ethernet Controller - IEEE 802.1AS/1588 conformance with PCIe-based timestamping",
                .hardware_timestamping_enabled = true,
                .software_timestamping_fallback = true,
                .tagged_transmit_only = false,    // Hardware timestamping support
                .sync_interval_ms = 125,          // Standard 8 messages/sec
                .announce_interval_ms = 1000,
                .pdelay_req_interval_ms = 1000,
                .tsn_features_enabled = false,    // Basic gPTP support, no advanced TSN
                .dual_clock_master_support = false,
                .frame_preemption_support = false,
                .time_aware_shaper_support = false,
                .interrupt_coalescing_us = 20,    // Low coalescing for good timing accuracy
                .rx_ring_size = 256,              // Smaller rings for integrated controller
                .tx_ring_size = 256,
                .max_sync_loss_count = 3,         // Tighter tolerance for precision
                .sync_timeout_ms = 3000,
                .auto_recovery_enabled = true,
            }
        },
        {
            "E810",
            {
                .controller_family = "E810",
                .description = "Intel E810 High-Performance Controller - Advanced PTP/SyncE/TSN support",
                .hardware_timestamping_enabled = true,
                .software_timestamping_fallback = true,
                .tagged_transmit_only = false,    // Advanced timestamping capabilities
                .sync_interval_ms = 31,           // High precision (32 messages/sec)
                .announce_interval_ms = 1000,
                .pdelay_req_interval_ms = 1000,
                .tsn_features_enabled = true,     // Full TSN and advanced features
                .dual_clock_master_support = true,
                .frame_preemption_support = true,
                .time_aware_shaper_support = true,
                .interrupt_coalescing_us = 0,     // No coalescing for maximum precision
                .rx_ring_size = 2048,             // Large rings for high-speed operation
                .tx_ring_size = 2048,
                .max_sync_loss_count = 2,         // Tight timing requirements
                .sync_timeout_ms = 2000,          // Fast recovery
                .auto_recovery_enabled = true,
            }
        }
    };

    /**
     * @brief Platform-specific configuration recommendations
     */
    struct PlatformConfig {
        // Windows-specific settings
        struct Windows {
            bool use_native_timestamping_api = true;
            bool configure_w32time_service = true;
            int thread_priority = 15;  // THREAD_PRIORITY_TIME_CRITICAL equivalent
            bool disable_interrupt_moderation = true;
        };

        // Linux-specific settings  
        struct Linux {
            std::vector<std::string> required_kernel_modules = {"ptp", "igb"};
            bool configure_interrupt_affinity = true;
            int cpu_affinity_mask = 0x1;  // Pin to CPU 0
            bool disable_power_management = true;
            std::string recommended_kernel_version = "5.4+";
        };
    };

} // namespace gptp
