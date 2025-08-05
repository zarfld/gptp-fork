/**
 * @file clock_quality_manager.hpp
 * @brief IEEE 802.1AS Clock Quality and Priority Management
 * 
 * Implements proper Clock Quality, Priority1, and Priority2 management
 * according to IEEE 802.1AS-2021 specifications.
 */

#pragma once

#include "gptp_protocol.hpp"
#include <chrono>
#include <string>
#include <memory>

namespace gptp {
namespace clock_quality {

/**
 * @brief Clock Class enumeration (IEEE 802.1AS-2021 clause 8.6.2.2)
 * 
 * The clockClass denotes the traceability of the synchronized time distributed
 * by a ClockMaster when it is the Grandmaster PTP Instance.
 */
enum class ClockClass : uint8_t {
    // Primary reference time sources (0-5)
    PRIMARY_GPS = 6,                    // GPS receiver synchronized
    PRIMARY_RADIO = 7,                  // Radio synchronized
    PRIMARY_PTP = 8,                    // PTP synchronized
    
    // Holdover specifications (9-127)
    HOLDOVER_SPEC_1 = 13,              // Within holdover specification 1
    HOLDOVER_SPEC_2 = 14,              // Within holdover specification 2
    
    // Application specific (128-135)
    APPLICATION_SPECIFIC_MIN = 128,
    APPLICATION_SPECIFIC_MAX = 135,
    
    // IEEE 802.1AS specific classes (136-199)
    GPTP_DEFAULT_GRANDMASTER = 248,     // Default gPTP grandmaster-capable
    GPTP_SLAVE_ONLY = 255,             // Slave-only clock (not grandmaster-capable)
    
    // Reserved ranges
    RESERVED_MIN_1 = 9,
    RESERVED_MAX_1 = 12,
    RESERVED_MIN_2 = 15,
    RESERVED_MAX_2 = 127,
    RESERVED_MIN_3 = 136,
    RESERVED_MAX_3 = 199,
    RESERVED_MIN_4 = 200,
    RESERVED_MAX_4 = 247,
    RESERVED_MIN_5 = 249,
    RESERVED_MAX_5 = 254
};

/**
 * @brief Priority1 values (IEEE 802.1AS-2021 clause 8.6.2.1)
 */
enum class Priority1 : uint8_t {
    MANAGEMENT_RESERVED = 0,            // Reserved for management use only
    HIGHEST_PRIORITY = 1,               // Highest user priority
    HIGH_PRIORITY = 64,                 // High priority
    DEFAULT_PRIORITY = 128,             // Default priority
    LOW_PRIORITY = 192,                 // Low priority
    GRANDMASTER_CAPABLE_MAX = 254,      // Maximum for grandmaster-capable
    SLAVE_ONLY = 255                    // Slave-only (not grandmaster-capable)
};

/**
 * @brief Priority2 values (IEEE 802.1AS-2021 clause 8.6.2.5)
 */
enum class Priority2 : uint8_t {
    HIGHEST_PRIORITY = 0,               // Highest priority
    HIGH_PRIORITY = 64,                 // High priority  
    DEFAULT_PRIORITY = 128,             // Default priority
    LOW_PRIORITY = 192,                 // Low priority
    LOWEST_PRIORITY = 255               // Lowest priority
};

/**
 * @brief Clock source type for determining clock quality
 */
enum class ClockSourceType {
    UNKNOWN,                            // Unknown/unspecified
    FREE_RUNNING_CRYSTAL,               // Free-running crystal oscillator
    IEEE802_3_CRYSTAL,                  // IEEE 802.3 compliant crystal
    TEMPERATURE_COMPENSATED_CRYSTAL,    // TCXO
    OVEN_CONTROLLED_CRYSTAL,            // OCXO
    RUBIDIUM_OSCILLATOR,               // Rubidium atomic clock
    CESIUM_OSCILLATOR,                 // Cesium atomic clock
    GPS_DISCIPLINED,                   // GPS disciplined oscillator
    GNSS_DISCIPLINED,                  // GNSS disciplined oscillator
    NTP_SYNCHRONIZED,                  // NTP synchronized
    PTP_SYNCHRONIZED,                  // PTP synchronized (boundary clock)
    RADIO_SYNCHRONIZED,                // Radio synchronized (e.g., WWVB)
    MANUAL_INPUT                       // Manually set time
};

/**
 * @brief Clock quality configuration
 */
struct ClockQualityConfig {
    ClockSourceType source_type = ClockSourceType::IEEE802_3_CRYSTAL;
    bool grandmaster_capable = false;
    uint8_t priority1 = static_cast<uint8_t>(Priority1::DEFAULT_PRIORITY);
    uint8_t priority2 = static_cast<uint8_t>(Priority2::DEFAULT_PRIORITY);
    
    // Clock accuracy estimate (used for clockAccuracy calculation)
    std::chrono::nanoseconds estimated_accuracy{100000}; // 100 microseconds default
    
    // Offset scaled log variance (stability measure)
    uint16_t offset_scaled_log_variance = 0x436A; // IEEE 802.1AS default
    
    // Source-specific parameters
    bool has_external_time_source = false;
    bool time_source_traceable = false;
    std::chrono::seconds holdover_capability{0};
};

/**
 * @brief Clock Quality Manager
 * 
 * Manages clock quality assessment and priority assignment according to
 * IEEE 802.1AS specifications.
 */
class ClockQualityManager {
public:
    explicit ClockQualityManager(const ClockQualityConfig& config = ClockQualityConfig{});
    ~ClockQualityManager() = default;

    // Configuration
    void set_config(const ClockQualityConfig& config);
    const ClockQualityConfig& get_config() const { return config_; }
    
    // Clock quality assessment
    ClockQuality calculate_clock_quality() const;
    ClockClass determine_clock_class() const;
    protocol::ClockAccuracy determine_clock_accuracy() const;
    uint16_t calculate_offset_scaled_log_variance() const;
    
    // Priority management
    uint8_t get_priority1() const;
    uint8_t get_priority2() const;
    bool is_grandmaster_capable() const;
    
    // Dynamic updates
    void update_time_source_status(bool available, bool traceable = false);
    void update_accuracy_estimate(std::chrono::nanoseconds accuracy);
    void set_holdover_mode(bool in_holdover);
    void set_management_priority1(uint8_t priority1);
    
    // Clock source information
    std::string get_clock_source_description() const;
    protocol::TimeSource get_time_source() const;
    
    // Validation
    static bool is_valid_priority1(uint8_t priority1);
    static bool is_valid_priority2(uint8_t priority2);
    static bool is_valid_clock_class(uint8_t clock_class);

private:
    ClockQualityConfig config_;
    bool in_holdover_mode_ = false;
    bool external_source_available_ = false;
    bool external_source_traceable_ = false;
    std::chrono::steady_clock::time_point last_source_update_;
    
    // Optional management override for priority1
    bool has_management_priority1_ = false;
    uint8_t management_priority1_ = 0;
    
    // Internal calculation helpers
    ClockClass calculate_clock_class_internal() const;
    protocol::ClockAccuracy accuracy_from_nanoseconds(std::chrono::nanoseconds accuracy) const;
    uint16_t variance_from_source_type(ClockSourceType source) const;
};

/**
 * @brief Clock Quality Factory
 * 
 * Factory class for creating pre-configured clock quality managers
 * for common use cases.
 */
class ClockQualityFactory {
public:
    // Common configurations
    static std::unique_ptr<ClockQualityManager> create_gps_grandmaster();
    static std::unique_ptr<ClockQualityManager> create_ieee802_3_end_station();
    static std::unique_ptr<ClockQualityManager> create_high_precision_oscillator();
    static std::unique_ptr<ClockQualityManager> create_slave_only_clock();
    static std::unique_ptr<ClockQualityManager> create_boundary_clock();
    
    // Custom configuration
    static std::unique_ptr<ClockQualityManager> create_custom(const ClockQualityConfig& config);
};

/**
 * @brief Clock Quality Utilities
 * 
 * Utility functions for clock quality operations
 */
namespace utils {
    // Conversion functions
    std::string clock_class_to_string(ClockClass clock_class);
    std::string clock_accuracy_to_string(protocol::ClockAccuracy accuracy);
    std::string priority_to_string(uint8_t priority);
    
    // Comparison functions (for BMCA)
    int compare_clock_quality(const ClockQuality& a, const ClockQuality& b);
    bool is_better_clock_quality(const ClockQuality& a, const ClockQuality& b);
    
    // Validation
    bool validate_clock_quality(const ClockQuality& quality);
    
    // Wire format conversion
    uint32_t pack_clock_quality(const ClockQuality& quality);
    ClockQuality unpack_clock_quality(uint32_t packed);
}

} // namespace clock_quality
} // namespace gptp
