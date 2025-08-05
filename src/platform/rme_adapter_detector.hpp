/**
 * @brief RME Audio Interface Detector for Windows
 * 
 * This module detects RME audio interfaces in the system and evaluates
 * their potential for gPTP/IEEE 1588 support.
 */

#pragma once

#include "../../include/gptp_types.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace gptp {

    /**
     * @brief RME product families that potentially support precision timing
     */
    enum class RmeProductFamily {
        UNKNOWN = 0,
        MADIFACE_USB,       // MADIface USB series
        MADIFACE_XT,        // MADIface XT series
        FIREFACE_USB,       // Fireface USB series
        FIREFACE_UCX,       // Fireface UCX series
        HDSPE_MADI,         // HDSPe MADI PCIe cards
        M_SERIES            // M-Series converters (M-1620 Pro, etc.)
    };

    /**
     * @brief RME audio interface profile for gPTP capabilities
     */
    struct RmeProfile {
        std::string product_name;
        RmeProductFamily family;
        
        // Professional audio timing capabilities (assumed)
        bool supports_word_clock;
        bool supports_aes_reference;
        bool supports_madi_clock;
        bool has_steadyclock_technology;
        
        // Potential IEEE 1588 support (unknown without specs)
        bool potential_ieee_1588_support;
        bool potential_hardware_timestamping;
        
        // Audio-specific synchronization
        bool supports_sample_accurate_sync;
        uint32_t max_channels;
        
        // Connection type
        bool is_usb_interface;
        bool is_pcie_interface;
        bool is_network_interface;
        
        std::string notes;
    };

    /**
     * @brief USB Vendor and Product IDs for RME devices
     */
    namespace RmeUsbIds {
        constexpr uint16_t VENDOR_ID = 0x2A39; // RME USB Vendor ID
        
        // Known RME USB Product IDs (partial list - more may exist)
        constexpr uint16_t MADIFACE_USB = 0x3FF1;     // MADIface USB (detected in system)
        constexpr uint16_t FIREFACE_UC = 0x000C;      // Fireface UC
        constexpr uint16_t FIREFACE_UCX = 0x0018;     // Fireface UCX
        constexpr uint16_t FIREFACE_UCX_II = 0x002F;  // Fireface UCX II
        constexpr uint16_t BABYFACE_PRO = 0x0011;     // Babyface Pro
        constexpr uint16_t BABYFACE_PRO_FS = 0x0023;  // Babyface Pro FS
    }

    /**
     * @brief Information about a detected RME audio interface
     */
    struct RmeAdapterInfo {
        std::string device_name;
        std::string device_description;
        RmeProductFamily family;
        RmeProfile profile;
        
        // USB identification
        uint16_t usb_vendor_id;
        uint16_t usb_product_id;
        std::string usb_device_id;
        
        // Windows-specific
        std::string pnp_device_id;
        std::string friendly_name;
        
        // gPTP evaluation
        bool potentially_supports_gptp;
        std::string gptp_assessment;
    };

    /**
     * @brief RME Audio Interface Detector for Windows
     */
    class RmeAdapterDetector {
    public:
        RmeAdapterDetector() = default;
        ~RmeAdapterDetector() = default;

        /**
         * @brief Initialize the detector
         * @return ErrorCode indicating success or failure
         */
        ErrorCode initialize();

        /**
         * @brief Detect RME audio interfaces in the system
         * @return Result containing vector of RME adapter information
         */
        Result<std::vector<RmeAdapterInfo>> detect_rme_adapters();

        /**
         * @brief Cleanup resources
         */
        void cleanup();

    private:
        /**
         * @brief Parse USB device ID to extract vendor/product IDs
         * @param device_id USB device ID string
         * @param vendor_id Output vendor ID
         * @param product_id Output product ID
         * @return true if parsing successful
         */
        bool parse_usb_device_id(const std::string& device_id, 
                                uint16_t& vendor_id, 
                                uint16_t& product_id);

        /**
         * @brief Get device property as string (Windows-specific)
         * @param dev_info_set Device information set
         * @param dev_info_data Device information data
         * @param property Device property key
         * @return Device property string or empty string if failed
         */
        std::string get_device_property_string(void* dev_info_set,
                                             void* dev_info_data,
                                             uint32_t property);

        /**
         * @brief Evaluate RME device for gPTP capability
         * @param adapter_info RME adapter information to evaluate
         */
        void evaluate_gptp_capability(RmeAdapterInfo& adapter_info);

        /**
         * @brief Get RME profile by USB Product ID
         * @param product_id USB Product ID
         * @return RME profile if found, nullptr otherwise
         */
        const RmeProfile* get_rme_profile_by_usb_id(uint16_t product_id);

        /**
         * @brief Check if an RME device potentially supports gPTP
         * @param profile RME device profile
         * @return true if device might support gPTP (needs verification)
         */
        bool rme_potentially_supports_gptp(const RmeProfile& profile);
    };

    /**
     * @brief Get recommendations for RME gPTP implementation
     * @return Vector of implementation recommendations
     */
    std::vector<std::string> get_rme_implementation_recommendations();

} // namespace gptp
