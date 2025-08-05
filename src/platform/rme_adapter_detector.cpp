/**
 * @brief RME Audio Interface Detector Implementation
 * 
 * This module demonstrates how to detect RME audio interfaces and assess
 * their potential for gPTP support (without official specifications).
 */

#include "rme_adapter_detector.hpp"
#include "../../include/gptp_types.hpp"
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <cfgmgr32.h>
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")
#endif

namespace gptp {

    // RME USB device profiles (based on reasonable assumptions)
    const std::unordered_map<uint16_t, RmeProfile> RME_USB_PROFILES = {
        {
            RmeUsbIds::MADIFACE_USB, // 0x3FF1
            {
                "MADIface USB",                                 // product_name
                RmeProductFamily::MADIFACE_USB,                 // family
                true,                                           // supports_word_clock
                true,                                           // supports_aes_reference
                true,                                           // supports_madi_clock
                true,                                           // has_steadyclock_technology
                true,                                           // potential_ieee_1588_support
                true,                                           // potential_hardware_timestamping
                true,                                           // supports_sample_accurate_sync
                128,                                            // max_channels
                true,                                           // is_usb_interface
                false,                                          // is_pcie_interface
                true,                                           // is_network_interface
                "Professional MADI interface with Ethernet-like connectivity. IEEE 1588 support needs RME confirmation." // notes
            }
        }
    };

    ErrorCode RmeAdapterDetector::initialize() {
        return ErrorCode::SUCCESS;
    }

    Result<std::vector<RmeAdapterInfo>> RmeAdapterDetector::detect_rme_adapters() {
        std::vector<RmeAdapterInfo> rme_adapters;

#ifdef _WIN32
        // For demonstration, we'll create a mock RME adapter based on our known system
        // In reality, this would use SetupAPI to enumerate USB devices
        
        // We know from our system analysis that there's an RME MADIface USB with MAC 48-0b-b2-d9-6a-d3
        RmeAdapterInfo adapter_info;
        adapter_info.usb_vendor_id = RmeUsbIds::VENDOR_ID; // 0x2A39
        adapter_info.usb_product_id = RmeUsbIds::MADIFACE_USB; // 0x3FF1
        adapter_info.usb_device_id = "USB\\VID_2A39&PID_3FF1&MI_03\\6&178F755F&0&0003";
        adapter_info.device_name = "MADIface USB";
        adapter_info.device_description = "RME MADIface USB Ethernet";
        adapter_info.friendly_name = "RME MADIface USB Ethernet";
        adapter_info.pnp_device_id = "USB Device";
        adapter_info.family = RmeProductFamily::MADIFACE_USB;
        
        // Get profile
        const RmeProfile* profile = get_rme_profile_by_usb_id(RmeUsbIds::MADIFACE_USB);
        if (profile) {
            adapter_info.profile = *profile;
        }
        
        // Evaluate gPTP capability
        evaluate_gptp_capability(adapter_info);
        
        rme_adapters.push_back(adapter_info);
#endif

        return Result<std::vector<RmeAdapterInfo>>(std::move(rme_adapters));
    }

    void RmeAdapterDetector::cleanup() {
        // No cleanup needed for this implementation
    }

    const RmeProfile* RmeAdapterDetector::get_rme_profile_by_usb_id(uint16_t product_id) {
        auto it = RME_USB_PROFILES.find(product_id);
        return (it != RME_USB_PROFILES.end()) ? &it->second : nullptr;
    }

    bool RmeAdapterDetector::rme_potentially_supports_gptp(const RmeProfile& profile) {
        return profile.potential_ieee_1588_support && 
               profile.is_network_interface &&
               profile.has_steadyclock_technology &&
               profile.max_channels >= 64;
    }

    void RmeAdapterDetector::evaluate_gptp_capability(RmeAdapterInfo& adapter_info) {
        if (rme_potentially_supports_gptp(adapter_info.profile)) {
            adapter_info.potentially_supports_gptp = true;
            adapter_info.gptp_assessment = "POTENTIALLY SUPPORTS gPTP - Professional MADI interface with network capabilities and advanced clocking. However, IEEE 1588 support requires official RME specification confirmation.";
        } else {
            adapter_info.potentially_supports_gptp = false;
            adapter_info.gptp_assessment = "gPTP support unknown - Professional interface but specifications not publicly available.";
        }
    }

    bool RmeAdapterDetector::parse_usb_device_id(const std::string& device_id, 
                                                 uint16_t& vendor_id, 
                                                 uint16_t& product_id) {
        size_t vid_pos = device_id.find("VID_");
        if (vid_pos == std::string::npos) return false;

        size_t pid_pos = device_id.find("PID_", vid_pos);
        if (pid_pos == std::string::npos) return false;

        try {
            std::string vid_str = device_id.substr(vid_pos + 4, 4);
            vendor_id = static_cast<uint16_t>(std::stoul(vid_str, nullptr, 16));

            std::string pid_str = device_id.substr(pid_pos + 4, 4);
            product_id = static_cast<uint16_t>(std::stoul(pid_str, nullptr, 16));
            return true;
        } catch (...) {
            return false;
        }
    }

    std::string RmeAdapterDetector::get_device_property_string(void* dev_info_set,
                                                             void* dev_info_data,
                                                             uint32_t property) {
        // Windows-specific implementation would go here
        return "";
    }

    std::vector<std::string> get_rme_implementation_recommendations() {
        return {
            "❗ CRITICAL: Contact RME Audio directly for official IEEE 1588/802.1AS support specifications",
            "📋 Request RME SDK/API documentation for hardware timestamping access",  
            "🔍 Investigate RME driver interfaces for precision timing capabilities",
            "🎵 Test with professional audio applications requiring sample-accurate synchronization",
            "⚡ Consider ASIO driver integration for low-latency timing access",
            "🕐 Evaluate Word Clock and AES reference timing as alternative sync methods",
            "🎛️  Check RME TotalMix FX for network timing configuration options",
            "📊 Review RME SteadyClock technology documentation for timing precision specifications",
            "🌐 Check if RME devices support AVB (Audio Video Bridging) which uses 802.1AS",
            "🔬 Investigate MADI frame timing for potential precision timestamping capabilities"
        };
    }

} // namespace gptp
