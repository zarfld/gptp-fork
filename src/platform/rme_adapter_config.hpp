/**
 * @brief RME Audio Interface Configuration for gPTP Support
 * 
 * This header defines configuration data for RME audio interfaces,
 * particularly for professional audio synchronization requirements.
 * 
 * Note: RME specifications for IEEE 1588/802.1AS support are not publicly
 * documented. This implementation is based on reasonable assumptions for
 * professional audio equipment and may require adjustment based on actual
 * RME hardware capabilities.
 */

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

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
     * @brief Mapping of RME USB Product IDs to profiles
     */
    const std::unordered_map<uint16_t, RmeProfile> RME_USB_PROFILES = {
        {
            RmeUsbIds::MADIFACE_USB,
            {
                .product_name = "MADIface USB",
                .family = RmeProductFamily::MADIFACE_USB,
                .supports_word_clock = true,
                .supports_aes_reference = true,
                .supports_madi_clock = true,
                .has_steadyclock_technology = true,
                .potential_ieee_1588_support = true,  // Unknown - could be possible
                .potential_hardware_timestamping = true, // Professional audio device assumption
                .supports_sample_accurate_sync = true,
                .max_channels = 128, // 64 in/64 out or 128 channels total
                .is_usb_interface = true,
                .is_pcie_interface = false,
                .is_network_interface = true, // Has Ethernet-like interface
                .notes = "Professional MADI interface with advanced clocking. IEEE 1588 support unknown - requires RME specification confirmation."
            }
        },
        {
            RmeUsbIds::FIREFACE_UCX,
            {
                .product_name = "Fireface UCX",
                .family = RmeProductFamily::FIREFACE_UCX,
                .supports_word_clock = true,
                .supports_aes_reference = true,
                .supports_madi_clock = false,
                .has_steadyclock_technology = true,
                .potential_ieee_1588_support = false, // Lower-end interface
                .potential_hardware_timestamping = false,
                .supports_sample_accurate_sync = true,
                .max_channels = 36, // 18 in/18 out
                .is_usb_interface = true,
                .is_pcie_interface = false,
                .is_network_interface = false,
                .notes = "Mid-range interface. Unlikely to support IEEE 1588."
            }
        },
        {
            RmeUsbIds::FIREFACE_UCX_II,
            {
                .product_name = "Fireface UCX II",
                .family = RmeProductFamily::FIREFACE_UCX,
                .supports_word_clock = true,
                .supports_aes_reference = true,
                .supports_madi_clock = false,
                .has_steadyclock_technology = true,
                .potential_ieee_1588_support = false, // Lower-end interface
                .potential_hardware_timestamping = false,
                .supports_sample_accurate_sync = true,
                .max_channels = 40, // 20 in/20 out
                .is_usb_interface = true,
                .is_pcie_interface = false,
                .is_network_interface = false,
                .notes = "Updated mid-range interface. Unlikely to support IEEE 1588."
            }
        }
    };

    /**
     * @brief Get RME profile by USB Product ID
     * @param product_id USB Product ID
     * @return RME profile if found, nullptr otherwise
     */
    inline const RmeProfile* get_rme_profile_by_usb_id(uint16_t product_id) {
        auto it = RME_USB_PROFILES.find(product_id);
        return (it != RME_USB_PROFILES.end()) ? &it->second : nullptr;
    }

    /**
     * @brief Check if an RME device potentially supports gPTP
     * @param profile RME device profile
     * @return true if device might support gPTP (needs verification)
     */
    inline bool rme_potentially_supports_gptp(const RmeProfile& profile) {
        // Conservative approach: only consider devices that:
        // 1. Are network-capable interfaces (like MADIface USB)
        // 2. Have professional clocking capabilities
        // 3. Are high-end professional devices
        
        return profile.potential_ieee_1588_support && 
               profile.is_network_interface &&
               profile.has_steadyclock_technology &&
               profile.max_channels >= 64; // Professional-grade channel count
    }

    /**
     * @brief Get recommendations for RME gPTP implementation
     * @return Vector of implementation recommendations
     */
    inline std::vector<std::string> get_rme_implementation_recommendations() {
        return {
            "Contact RME Audio for official IEEE 1588/802.1AS specifications",
            "Check if RME provides SDK or API for hardware timestamping access",
            "Investigate RME driver interfaces for precision timing capabilities",
            "Test with professional audio applications that require sample-accurate sync",
            "Consider ASIO driver integration for low-latency timing access",
            "Evaluate Word Clock and AES reference timing as alternative sync methods",
            "Check RME TotalMix FX for any network timing configuration options",
            "Review RME SteadyClock technology documentation for timing precision specs"
        };
    }

} // namespace gptp
