/**
 * @file clock_quality_manager.cpp
 * @brief Implementation of IEEE 802.1AS Clock Quality and Priority Management
 */

#include "../../include/clock_quality_manager.hpp"
#include <stdexcept>
#include <sstream>
#include <chrono>

namespace gptp {
namespace clock_quality {

// =============================================================================
// ClockQualityManager Implementation
// =============================================================================

ClockQualityManager::ClockQualityManager(const ClockQualityConfig& config)
    : config_(config)
    , last_source_update_(std::chrono::steady_clock::now())
{
    external_source_available_ = config_.has_external_time_source;
    external_source_traceable_ = config_.time_source_traceable;
}

void ClockQualityManager::set_config(const ClockQualityConfig& config) {
    config_ = config;
    external_source_available_ = config_.has_external_time_source;
    external_source_traceable_ = config_.time_source_traceable;
    last_source_update_ = std::chrono::steady_clock::now();
}

ClockQuality ClockQualityManager::calculate_clock_quality() const {
    ClockQuality quality;
    
    quality.clockClass = static_cast<uint8_t>(determine_clock_class());
    quality.clockAccuracy = determine_clock_accuracy();
    quality.offsetScaledLogVariance = calculate_offset_scaled_log_variance();
    
    return quality;
}

ClockClass ClockQualityManager::determine_clock_class() const {
    return calculate_clock_class_internal();
}

ClockClass ClockQualityManager::calculate_clock_class_internal() const {
    // IEEE 802.1AS-2021 clause 8.6.2.2
    
    // If not grandmaster-capable, always return slave-only
    if (!config_.grandmaster_capable) {
        return ClockClass::GPTP_SLAVE_ONLY;
    }
    
    // If in holdover mode without external source
    if (in_holdover_mode_ && !external_source_available_) {
        // Use holdover specifications based on holdover capability
        if (config_.holdover_capability >= std::chrono::hours(24)) {
            return ClockClass::HOLDOVER_SPEC_1;
        } else if (config_.holdover_capability >= std::chrono::hours(1)) {
            return ClockClass::HOLDOVER_SPEC_2;
        } else {
            return ClockClass::GPTP_DEFAULT_GRANDMASTER;
        }
    }
    
    // Determine class based on source type and traceability
    switch (config_.source_type) {
        case ClockSourceType::GPS_DISCIPLINED:
        case ClockSourceType::GNSS_DISCIPLINED:
            if (external_source_available_ && external_source_traceable_) {
                return ClockClass::PRIMARY_GPS;
            }
            break;
            
        case ClockSourceType::RADIO_SYNCHRONIZED:
            if (external_source_available_ && external_source_traceable_) {
                return ClockClass::PRIMARY_RADIO;
            }
            break;
            
        case ClockSourceType::PTP_SYNCHRONIZED:
            if (external_source_available_) {
                return ClockClass::PRIMARY_PTP;
            }
            break;
            
        case ClockSourceType::CESIUM_OSCILLATOR:
        case ClockSourceType::RUBIDIUM_OSCILLATOR:
            // High precision atomic clocks
            return ClockClass::HOLDOVER_SPEC_1;
            
        case ClockSourceType::OVEN_CONTROLLED_CRYSTAL:
            // OCXO can provide good holdover
            return ClockClass::HOLDOVER_SPEC_2;
            
        case ClockSourceType::TEMPERATURE_COMPENSATED_CRYSTAL:
        case ClockSourceType::IEEE802_3_CRYSTAL:
        case ClockSourceType::FREE_RUNNING_CRYSTAL:
        case ClockSourceType::NTP_SYNCHRONIZED:
        case ClockSourceType::MANUAL_INPUT:
        case ClockSourceType::UNKNOWN:
        default:
            // Default to standard gPTP class
            break;
    }
    
    // Default for grandmaster-capable devices
    return ClockClass::GPTP_DEFAULT_GRANDMASTER;
}

protocol::ClockAccuracy ClockQualityManager::determine_clock_accuracy() const {
    return accuracy_from_nanoseconds(config_.estimated_accuracy);
}

protocol::ClockAccuracy ClockQualityManager::accuracy_from_nanoseconds(
    std::chrono::nanoseconds accuracy) const {
    
    // IEEE 802.1AS-2021 Table 7-2
    auto ns = accuracy.count();
    
    if (ns <= 25) return protocol::ClockAccuracy::WITHIN_25_NS;
    if (ns <= 100) return protocol::ClockAccuracy::WITHIN_100_NS;
    if (ns <= 250) return protocol::ClockAccuracy::WITHIN_250_NS;
    if (ns <= 1000) return protocol::ClockAccuracy::WITHIN_1_US;
    if (ns <= 2500) return protocol::ClockAccuracy::WITHIN_2_5_US;
    if (ns <= 10000) return protocol::ClockAccuracy::WITHIN_10_US;
    if (ns <= 25000) return protocol::ClockAccuracy::WITHIN_25_US;
    if (ns <= 100000) return protocol::ClockAccuracy::WITHIN_100_US;
    if (ns <= 250000) return protocol::ClockAccuracy::WITHIN_250_US;
    if (ns <= 1000000) return protocol::ClockAccuracy::WITHIN_1_MS;
    if (ns <= 2500000) return protocol::ClockAccuracy::WITHIN_2_5_MS;
    if (ns <= 10000000) return protocol::ClockAccuracy::WITHIN_10_MS;
    if (ns <= 25000000) return protocol::ClockAccuracy::WITHIN_25_MS;
    if (ns <= 100000000) return protocol::ClockAccuracy::WITHIN_100_MS;
    if (ns <= 250000000) return protocol::ClockAccuracy::WITHIN_250_MS;
    if (ns <= 1000000000) return protocol::ClockAccuracy::WITHIN_1_S;
    if (ns <= 10000000000LL) return protocol::ClockAccuracy::WITHIN_10_S;
    
    return protocol::ClockAccuracy::GREATER_THAN_10_S;
}

uint16_t ClockQualityManager::calculate_offset_scaled_log_variance() const {
    // Use configured value or calculate based on source type
    if (config_.offset_scaled_log_variance != 0) {
        return config_.offset_scaled_log_variance;
    }
    
    return variance_from_source_type(config_.source_type);
}

uint16_t ClockQualityManager::variance_from_source_type(ClockSourceType source) const {
    // IEEE 802.1AS typical values for different oscillator types
    switch (source) {
        case ClockSourceType::CESIUM_OSCILLATOR:
            return 0x1000;  // Excellent stability
            
        case ClockSourceType::RUBIDIUM_OSCILLATOR:
            return 0x2000;  // Very good stability
            
        case ClockSourceType::OVEN_CONTROLLED_CRYSTAL:
            return 0x3000;  // Good stability
            
        case ClockSourceType::TEMPERATURE_COMPENSATED_CRYSTAL:
            return 0x4000;  // Moderate stability
            
        case ClockSourceType::GPS_DISCIPLINED:
        case ClockSourceType::GNSS_DISCIPLINED:
            if (external_source_available_) {
                return 0x2000;  // Very good when locked
            } else {
                return 0x5000;  // Degraded when not locked
            }
            
        case ClockSourceType::IEEE802_3_CRYSTAL:
            return 0x436A;  // IEEE 802.1AS default
            
        case ClockSourceType::FREE_RUNNING_CRYSTAL:
            return 0x6000;  // Poor stability
            
        case ClockSourceType::PTP_SYNCHRONIZED:
        case ClockSourceType::NTP_SYNCHRONIZED:
            return 0x4000;  // Depends on network quality
            
        case ClockSourceType::UNKNOWN:
        case ClockSourceType::MANUAL_INPUT:
        default:
            return 0x8000;  // Unknown/worst case
    }
}

uint8_t ClockQualityManager::get_priority1() const {
    // Management override takes precedence
    if (has_management_priority1_) {
        return management_priority1_;
    }
    
    return config_.priority1;
}

uint8_t ClockQualityManager::get_priority2() const {
    return config_.priority2;
}

bool ClockQualityManager::is_grandmaster_capable() const {
    return config_.grandmaster_capable;
}

void ClockQualityManager::update_time_source_status(bool available, bool traceable) {
    external_source_available_ = available;
    external_source_traceable_ = traceable;
    last_source_update_ = std::chrono::steady_clock::now();
    
    // Exit holdover mode if source becomes available
    if (available) {
        in_holdover_mode_ = false;
    }
}

void ClockQualityManager::update_accuracy_estimate(std::chrono::nanoseconds accuracy) {
    config_.estimated_accuracy = accuracy;
}

void ClockQualityManager::set_holdover_mode(bool in_holdover) {
    in_holdover_mode_ = in_holdover;
}

void ClockQualityManager::set_management_priority1(uint8_t priority1) {
    if (!is_valid_priority1(priority1)) {
        throw std::invalid_argument("Invalid priority1 value: " + std::to_string(priority1));
    }
    
    management_priority1_ = priority1;
    has_management_priority1_ = true;
}

std::string ClockQualityManager::get_clock_source_description() const {
    switch (config_.source_type) {
        case ClockSourceType::GPS_DISCIPLINED:
            return "GPS Disciplined Oscillator";
        case ClockSourceType::GNSS_DISCIPLINED:
            return "GNSS Disciplined Oscillator";
        case ClockSourceType::CESIUM_OSCILLATOR:
            return "Cesium Atomic Clock";
        case ClockSourceType::RUBIDIUM_OSCILLATOR:
            return "Rubidium Atomic Clock";
        case ClockSourceType::OVEN_CONTROLLED_CRYSTAL:
            return "Oven Controlled Crystal Oscillator (OCXO)";
        case ClockSourceType::TEMPERATURE_COMPENSATED_CRYSTAL:
            return "Temperature Compensated Crystal Oscillator (TCXO)";
        case ClockSourceType::IEEE802_3_CRYSTAL:
            return "IEEE 802.3 Compliant Crystal";
        case ClockSourceType::FREE_RUNNING_CRYSTAL:
            return "Free Running Crystal";
        case ClockSourceType::PTP_SYNCHRONIZED:
            return "PTP Synchronized (Boundary Clock)";
        case ClockSourceType::NTP_SYNCHRONIZED:
            return "NTP Synchronized";
        case ClockSourceType::RADIO_SYNCHRONIZED:
            return "Radio Synchronized (e.g., WWVB)";
        case ClockSourceType::MANUAL_INPUT:
            return "Manual Time Input";
        case ClockSourceType::UNKNOWN:
        default:
            return "Unknown Clock Source";
    }
}

protocol::TimeSource ClockQualityManager::get_time_source() const {
    switch (config_.source_type) {
        case ClockSourceType::GPS_DISCIPLINED:
        case ClockSourceType::GNSS_DISCIPLINED:
            return protocol::TimeSource::GPS;
            
        case ClockSourceType::RADIO_SYNCHRONIZED:
            return protocol::TimeSource::TERRESTRIAL_RADIO;
            
        case ClockSourceType::PTP_SYNCHRONIZED:
            return protocol::TimeSource::PTP;
            
        case ClockSourceType::NTP_SYNCHRONIZED:
            return protocol::TimeSource::NTP;
            
        case ClockSourceType::MANUAL_INPUT:
            return protocol::TimeSource::HAND_SET;
            
        case ClockSourceType::CESIUM_OSCILLATOR:
        case ClockSourceType::RUBIDIUM_OSCILLATOR:
            return protocol::TimeSource::ATOMIC_CLOCK;
            
        case ClockSourceType::OVEN_CONTROLLED_CRYSTAL:
        case ClockSourceType::TEMPERATURE_COMPENSATED_CRYSTAL:
        case ClockSourceType::IEEE802_3_CRYSTAL:
        case ClockSourceType::FREE_RUNNING_CRYSTAL:
        case ClockSourceType::UNKNOWN:
        default:
            return protocol::TimeSource::INTERNAL_OSCILLATOR;
    }
}

bool ClockQualityManager::is_valid_priority1(uint8_t priority1) {
    // IEEE 802.1AS-2021 clause 8.6.2.1
    // 0 is reserved for management use
    // 255 is for slave-only clocks
    // 1-254 are valid for grandmaster-capable clocks
    return true; // All values 0-255 are technically valid
}

bool ClockQualityManager::is_valid_priority2(uint8_t priority2) {
    // IEEE 802.1AS-2021 clause 8.6.2.5
    // All values 0-255 are valid
    return true;
}

bool ClockQualityManager::is_valid_clock_class(uint8_t clock_class) {
    // Check against reserved ranges
    ClockClass cc = static_cast<ClockClass>(clock_class);
    
    // Primary reference classes (6-8)
    if (clock_class >= 6 && clock_class <= 8) return true;
    
    // Holdover classes (13-14)
    if (clock_class >= 13 && clock_class <= 14) return true;
    
    // Application specific (128-135)
    if (clock_class >= 128 && clock_class <= 135) return true;
    
    // IEEE 802.1AS specific
    if (clock_class == 248 || clock_class == 255) return true;
    
    // Reserved ranges are not valid for use
    return false;
}

// =============================================================================
// ClockQualityFactory Implementation
// =============================================================================

std::unique_ptr<ClockQualityManager> ClockQualityFactory::create_gps_grandmaster() {
    ClockQualityConfig config;
    config.source_type = ClockSourceType::GPS_DISCIPLINED;
    config.grandmaster_capable = true;
    config.priority1 = static_cast<uint8_t>(Priority1::HIGH_PRIORITY);
    config.priority2 = static_cast<uint8_t>(Priority2::DEFAULT_PRIORITY);
    config.estimated_accuracy = std::chrono::nanoseconds(100); // 100ns GPS accuracy
    config.has_external_time_source = true;
    config.time_source_traceable = true;
    config.holdover_capability = std::chrono::hours(1);
    
    return std::make_unique<ClockQualityManager>(config);
}

std::unique_ptr<ClockQualityManager> ClockQualityFactory::create_ieee802_3_end_station() {
    ClockQualityConfig config;
    config.source_type = ClockSourceType::IEEE802_3_CRYSTAL;
    config.grandmaster_capable = false; // End station - slave only
    config.priority1 = static_cast<uint8_t>(Priority1::SLAVE_ONLY);
    config.priority2 = static_cast<uint8_t>(Priority2::DEFAULT_PRIORITY);
    config.estimated_accuracy = std::chrono::microseconds(100); // 100µs typical
    config.offset_scaled_log_variance = 0x436A; // IEEE 802.1AS default
    
    return std::make_unique<ClockQualityManager>(config);
}

std::unique_ptr<ClockQualityManager> ClockQualityFactory::create_high_precision_oscillator() {
    ClockQualityConfig config;
    config.source_type = ClockSourceType::OVEN_CONTROLLED_CRYSTAL;
    config.grandmaster_capable = true;
    config.priority1 = static_cast<uint8_t>(Priority1::DEFAULT_PRIORITY);
    config.priority2 = static_cast<uint8_t>(Priority2::HIGH_PRIORITY);
    config.estimated_accuracy = std::chrono::nanoseconds(250); // 250ns OCXO
    config.holdover_capability = std::chrono::hours(24);
    
    return std::make_unique<ClockQualityManager>(config);
}

std::unique_ptr<ClockQualityManager> ClockQualityFactory::create_slave_only_clock() {
    ClockQualityConfig config;
    config.source_type = ClockSourceType::FREE_RUNNING_CRYSTAL;
    config.grandmaster_capable = false;
    config.priority1 = static_cast<uint8_t>(Priority1::SLAVE_ONLY);
    config.priority2 = static_cast<uint8_t>(Priority2::LOWEST_PRIORITY);
    config.estimated_accuracy = std::chrono::milliseconds(1); // 1ms poor accuracy
    
    return std::make_unique<ClockQualityManager>(config);
}

std::unique_ptr<ClockQualityManager> ClockQualityFactory::create_boundary_clock() {
    ClockQualityConfig config;
    config.source_type = ClockSourceType::PTP_SYNCHRONIZED;
    config.grandmaster_capable = true;
    config.priority1 = static_cast<uint8_t>(Priority1::DEFAULT_PRIORITY);
    config.priority2 = static_cast<uint8_t>(Priority2::DEFAULT_PRIORITY);
    config.estimated_accuracy = std::chrono::microseconds(1); // 1µs PTP accuracy
    config.has_external_time_source = false; // No external source
    
    return std::make_unique<ClockQualityManager>(config);
}

std::unique_ptr<ClockQualityManager> ClockQualityFactory::create_custom(
    const ClockQualityConfig& config) {
    return std::make_unique<ClockQualityManager>(config);
}

// =============================================================================
// Utility Functions Implementation
// =============================================================================

namespace utils {

std::string clock_class_to_string(ClockClass clock_class) {
    switch (clock_class) {
        case ClockClass::PRIMARY_GPS:
            return "Primary GPS (6)";
        case ClockClass::PRIMARY_RADIO:
            return "Primary Radio (7)";
        case ClockClass::PRIMARY_PTP:
            return "Primary PTP (8)";
        case ClockClass::HOLDOVER_SPEC_1:
            return "Holdover Spec 1 (13)";
        case ClockClass::HOLDOVER_SPEC_2:
            return "Holdover Spec 2 (14)";
        case ClockClass::GPTP_DEFAULT_GRANDMASTER:
            return "gPTP Default Grandmaster (248)";
        case ClockClass::GPTP_SLAVE_ONLY:
            return "gPTP Slave Only (255)";
        default:
            return "Clock Class (" + std::to_string(static_cast<uint8_t>(clock_class)) + ")";
    }
}

std::string clock_accuracy_to_string(protocol::ClockAccuracy accuracy) {
    switch (accuracy) {
        case protocol::ClockAccuracy::WITHIN_25_NS:
            return "±25ns";
        case protocol::ClockAccuracy::WITHIN_100_NS:
            return "±100ns";
        case protocol::ClockAccuracy::WITHIN_250_NS:
            return "±250ns";
        case protocol::ClockAccuracy::WITHIN_1_US:
            return "±1µs";
        case protocol::ClockAccuracy::WITHIN_2_5_US:
            return "±2.5µs";
        case protocol::ClockAccuracy::WITHIN_10_US:
            return "±10µs";
        case protocol::ClockAccuracy::WITHIN_25_US:
            return "±25µs";
        case protocol::ClockAccuracy::WITHIN_100_US:
            return "±100µs";
        case protocol::ClockAccuracy::WITHIN_250_US:
            return "±250µs";
        case protocol::ClockAccuracy::WITHIN_1_MS:
            return "±1ms";
        case protocol::ClockAccuracy::WITHIN_2_5_MS:
            return "±2.5ms";
        case protocol::ClockAccuracy::WITHIN_10_MS:
            return "±10ms";
        case protocol::ClockAccuracy::WITHIN_25_MS:
            return "±25ms";
        case protocol::ClockAccuracy::WITHIN_100_MS:
            return "±100ms";
        case protocol::ClockAccuracy::WITHIN_250_MS:
            return "±250ms";
        case protocol::ClockAccuracy::WITHIN_1_S:
            return "±1s";
        case protocol::ClockAccuracy::WITHIN_10_S:
            return "±10s";
        case protocol::ClockAccuracy::GREATER_THAN_10_S:
            return ">10s";
        case protocol::ClockAccuracy::UNKNOWN:
        default:
            return "Unknown";
    }
}

std::string priority_to_string(uint8_t priority) {
    if (priority == 0) return "Management Reserved (0)";
    if (priority <= 63) return "Highest Priority (" + std::to_string(priority) + ")";
    if (priority <= 127) return "High Priority (" + std::to_string(priority) + ")";
    if (priority <= 191) return "Default Priority (" + std::to_string(priority) + ")";
    if (priority <= 254) return "Low Priority (" + std::to_string(priority) + ")";
    return "Slave Only (255)";
}

int compare_clock_quality(const ClockQuality& a, const ClockQuality& b) {
    // IEEE 802.1AS BMCA comparison order:
    // 1. clockClass (lower is better)
    // 2. clockAccuracy (lower is better)  
    // 3. offsetScaledLogVariance (lower is better)
    
    if (a.clockClass != b.clockClass) {
        return static_cast<int>(a.clockClass) - static_cast<int>(b.clockClass);
    }
    
    if (a.clockAccuracy != b.clockAccuracy) {
        return static_cast<int>(a.clockAccuracy) - static_cast<int>(b.clockAccuracy);
    }
    
    return static_cast<int>(a.offsetScaledLogVariance) - static_cast<int>(b.offsetScaledLogVariance);
}

bool is_better_clock_quality(const ClockQuality& a, const ClockQuality& b) {
    return compare_clock_quality(a, b) < 0;
}

bool validate_clock_quality(const ClockQuality& quality) {
    return ClockQualityManager::is_valid_clock_class(quality.clockClass);
}

uint32_t pack_clock_quality(const ClockQuality& quality) {
    // Pack into network byte order: clockClass(8) | clockAccuracy(8) | offsetScaledLogVariance(16)
    return (static_cast<uint32_t>(quality.clockClass) << 24) |
           (static_cast<uint32_t>(quality.clockAccuracy) << 16) |
           static_cast<uint32_t>(quality.offsetScaledLogVariance);
}

ClockQuality unpack_clock_quality(uint32_t packed) {
    ClockQuality quality;
    quality.clockClass = static_cast<uint8_t>((packed >> 24) & 0xFF);
    quality.clockAccuracy = static_cast<protocol::ClockAccuracy>((packed >> 16) & 0xFF);
    quality.offsetScaledLogVariance = static_cast<uint16_t>(packed & 0xFFFF);
    return quality;
}

} // namespace utils
} // namespace clock_quality  
} // namespace gptp
