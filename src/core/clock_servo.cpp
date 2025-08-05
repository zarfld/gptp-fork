/**
 * @file clock_servo.cpp
 * @brief Clock synchronization servo implementation
 */

#include "../../include/clock_servo.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace gptp {
namespace servo {

// ============================================================================
// ClockServo Implementation
// ============================================================================

ClockServo::ClockServo(const ServoConfig& config)
    : config_(config)
    , integral_accumulator_(0.0)
    , previous_offset_(0)
    , current_frequency_adjustment_(0.0)
    , current_phase_adjustment_(0.0)
    , locked_(false)
    , consecutive_good_samples_(0)
    , mean_offset_(0)
    , std_deviation_(0)
    , first_measurement_(true)
{
}

OffsetResult ClockServo::calculate_offset(const SyncMeasurement& measurement) {
    OffsetResult result;
    
    // IEEE 802.1AS-2021 offset calculation
    // offset = T2 - T1 - pathDelay - correctionField
    
    auto t1_ns = utils::timestamp_to_nanoseconds(measurement.master_timestamp);
    auto t2_ns = utils::timestamp_to_nanoseconds(measurement.local_receipt_time);
    auto correction_ns = utils::timestamp_to_nanoseconds(measurement.correction_field);
    
    // Calculate raw offset
    auto raw_offset = t2_ns - t1_ns - measurement.path_delay - correction_ns;
    
    // Apply filtering
    if (filter_offset(raw_offset)) {
        result.offset = raw_offset;
        result.path_delay = measurement.path_delay;
        result.valid = true;
        
        // Calculate confidence based on history consistency
        if (offset_history_.size() >= 3) {
            // Use recent measurements to calculate confidence
            std::vector<double> recent_offsets;
            for (size_t i = std::max(offset_history_.size() - 8, size_t(0)); 
                 i < offset_history_.size(); ++i) {
                recent_offsets.push_back(static_cast<double>(offset_history_[i].count()));
            }
            
            auto stats = utils::calculate_statistics(recent_offsets);
            double variation = stats.second; // Standard deviation
            
            // Lower variation = higher confidence
            result.confidence = std::max(0.0, std::min(1.0, 1.0 - (variation / 1000000.0))); // 1ms reference
        } else {
            result.confidence = 0.5; // Moderate confidence for early measurements
        }
    } else {
        result.valid = false;
        result.confidence = 0.0;
    }
    
    return result;
}

FrequencyResult ClockServo::update_servo(std::chrono::nanoseconds offset,
                                        std::chrono::steady_clock::time_point measurement_time) {
    FrequencyResult result;
    
    if (first_measurement_) {
        // Initialize for first measurement
        previous_offset_ = offset;
        previous_time_ = measurement_time;
        first_measurement_ = false;
        return result; // No adjustment on first measurement
    }
    
    // Calculate time difference
    auto dt = measurement_time - previous_time_;
    double dt_seconds = std::chrono::duration<double>(dt).count();
    
    if (dt_seconds <= 0.0) {
        return result; // Invalid time difference
    }
    
    // PI Controller implementation
    double offset_ns = static_cast<double>(offset.count());
    double previous_offset_ns = static_cast<double>(previous_offset_.count());
    
    // Proportional term (phase error)
    double proportional = config_.proportional_gain * offset_ns;
    
    // Integral term (frequency error)
    integral_accumulator_ += offset_ns * dt_seconds;
    double integral = config_.integral_gain * integral_accumulator_;
    
    // Calculate frequency adjustment (ppb)
    result.frequency_adjustment = (proportional + integral) / 1000.0; // Convert ns to ppb
    
    // Calculate phase adjustment (direct phase correction)
    result.phase_adjustment = proportional;
    
    // Apply limits
    result.frequency_adjustment = std::max(-config_.max_frequency_adjustment,
                                         std::min(config_.max_frequency_adjustment,
                                                result.frequency_adjustment));
    
    result.phase_adjustment = std::max(-config_.max_phase_adjustment,
                                     std::min(config_.max_phase_adjustment,
                                            result.phase_adjustment));
    
    // Update state
    current_frequency_adjustment_ = result.frequency_adjustment;
    current_phase_adjustment_ = result.phase_adjustment;
    previous_offset_ = offset;
    previous_time_ = measurement_time;
    last_update_ = measurement_time;
    
    // Update lock detection
    update_lock_detection();
    result.locked = locked_;
    
    return result;
}

void ClockServo::reset() {
    offset_history_.clear();
    time_history_.clear();
    integral_accumulator_ = 0.0;
    previous_offset_ = std::chrono::nanoseconds(0);
    current_frequency_adjustment_ = 0.0;
    current_phase_adjustment_ = 0.0;
    locked_ = false;
    consecutive_good_samples_ = 0;
    mean_offset_ = std::chrono::nanoseconds(0);
    std_deviation_ = std::chrono::nanoseconds(0);
    first_measurement_ = true;
}

ClockServo::ServoStats ClockServo::get_statistics() const {
    ServoStats stats;
    stats.sample_count = offset_history_.size();
    stats.mean_offset = mean_offset_;
    stats.std_deviation = std_deviation_;
    stats.current_frequency_ppb = current_frequency_adjustment_;
    stats.is_locked = locked_;
    stats.last_update = last_update_;
    return stats;
}

bool ClockServo::filter_offset(std::chrono::nanoseconds offset) {
    // Store measurement
    offset_history_.push_back(offset);
    time_history_.push_back(std::chrono::steady_clock::now());
    
    // Limit history size
    while (offset_history_.size() > config_.max_samples) {
        offset_history_.pop_front();
        time_history_.pop_front();
    }
    
    // For first few measurements, accept all
    if (offset_history_.size() < 3) {
        calculate_statistics();
        return true;
    }
    
    // Calculate median and MAD for outlier detection
    std::vector<double> offset_values;
    for (const auto& o : offset_history_) {
        offset_values.push_back(static_cast<double>(o.count()));
    }
    
    std::sort(offset_values.begin(), offset_values.end());
    double median = offset_values[offset_values.size() / 2];
    
    // Calculate MAD (Median Absolute Deviation)
    std::vector<double> deviations;
    for (double val : offset_values) {
        deviations.push_back(std::abs(val - median));
    }
    std::sort(deviations.begin(), deviations.end());
    double mad = deviations[deviations.size() / 2];
    
    // Check if current measurement is outlier
    double current_offset_ns = static_cast<double>(offset.count());
    bool is_outlier_val = utils::is_outlier(current_offset_ns, median, mad);
    
    if (is_outlier_val && offset_history_.size() > 8) {
        // Remove outlier from history
        offset_history_.pop_back();
        time_history_.pop_back();
        return false;
    }
    
    calculate_statistics();
    return true;
}

void ClockServo::calculate_statistics() {
    if (offset_history_.empty()) {
        mean_offset_ = std::chrono::nanoseconds(0);
        std_deviation_ = std::chrono::nanoseconds(0);
        return;
    }
    
    // Calculate mean
    double sum = 0.0;
    for (const auto& offset : offset_history_) {
        sum += static_cast<double>(offset.count());
    }
    double mean = sum / offset_history_.size();
    mean_offset_ = std::chrono::nanoseconds(static_cast<int64_t>(mean));
    
    // Calculate standard deviation
    if (offset_history_.size() > 1) {
        double variance_sum = 0.0;
        for (const auto& offset : offset_history_) {
            double diff = static_cast<double>(offset.count()) - mean;
            variance_sum += diff * diff;
        }
        double variance = variance_sum / (offset_history_.size() - 1);
        std_deviation_ = std::chrono::nanoseconds(static_cast<int64_t>(std::sqrt(variance)));
    }
}

void ClockServo::update_lock_detection() {
    // Consider servo locked if frequency adjustment is stable and small
    bool current_sample_good = std::abs(current_frequency_adjustment_) < config_.lock_threshold;
    
    if (current_sample_good) {
        consecutive_good_samples_++;
    } else {
        consecutive_good_samples_ = 0;
    }
    
    locked_ = (consecutive_good_samples_ >= config_.lock_samples);
}

// ============================================================================
// SynchronizationManager Implementation
// ============================================================================

SynchronizationManager::SynchronizationManager()
    : current_slave_port_(0)
    , total_adjustments_(0)
{
    current_status_.synchronized = false;
    current_status_.current_offset = std::chrono::nanoseconds(0);
    current_status_.frequency_adjustment_ppb = 0.0;
    current_status_.servo_locked = false;
    current_status_.slave_port_id = 0;
}

void SynchronizationManager::process_sync_followup(uint16_t port_id,
                                                  const SyncMessage& sync_msg,
                                                  const Timestamp& sync_receipt_time,
                                                  const FollowUpMessage& followup_msg,
                                                  std::chrono::nanoseconds path_delay) {
    // Only process if this is our current slave port
    if (port_id != current_slave_port_ || current_slave_port_ == 0) {
        return;
    }
    
    // Create or get servo for this port
    if (port_servos_.find(port_id) == port_servos_.end()) {
        port_servos_[port_id] = std::unique_ptr<ClockServo>(new ClockServo());
    }
    
    auto& servo = port_servos_[port_id];
    
    // Build sync measurement
    SyncMeasurement measurement;
    measurement.master_timestamp = sync_msg.originTimestamp;
    measurement.local_receipt_time = sync_receipt_time;
    // Correction field is in the follow-up header
    measurement.correction_field.set_seconds(0);
    measurement.correction_field.nanoseconds = static_cast<uint32_t>(followup_msg.header.correctionField);
    measurement.path_delay = path_delay;
    measurement.measurement_time = std::chrono::steady_clock::now();
    
    // Calculate offset
    OffsetResult offset_result = servo->calculate_offset(measurement);
    
    if (offset_result.valid) {
        // Update servo
        FrequencyResult freq_result = servo->update_servo(offset_result.offset,
                                                         measurement.measurement_time);
        
        // Update status
        current_status_.synchronized = true;
        current_status_.current_offset = offset_result.offset;
        current_status_.frequency_adjustment_ppb = freq_result.frequency_adjustment;
        current_status_.servo_locked = freq_result.locked;
        current_status_.last_sync_time = measurement.measurement_time;
        current_status_.slave_port_id = port_id;
    }
}

void SynchronizationManager::set_slave_port(uint16_t port_id) {
    if (current_slave_port_ != port_id) {
        current_slave_port_ = port_id;
        
        // Reset synchronization status when changing slave port
        current_status_.synchronized = false;
        current_status_.servo_locked = false;
        
        if (port_id == 0) {
            // No slave port - we're probably master
            current_status_.current_offset = std::chrono::nanoseconds(0);
            current_status_.frequency_adjustment_ppb = 0.0;
        }
    }
}

SynchronizationManager::SyncStatus SynchronizationManager::get_sync_status() const {
    return current_status_;
}

void SynchronizationManager::apply_clock_adjustments() {
    // This would integrate with platform-specific clock adjustment APIs
    // For now, just track that adjustments were requested
    
    if (current_status_.synchronized && current_slave_port_ != 0) {
        auto it = port_servos_.find(current_slave_port_);
        if (it != port_servos_.end()) {
            double freq_adj = it->second->get_frequency_adjustment();
            
            // In a real implementation, this would call:
            // - Windows: SetSystemTimeAdjustment() or similar
            // - Linux: adjtimex() system call
            // - Hardware: Direct register access for hardware timestamping
            
            total_adjustments_++;
            last_adjustment_time_ = std::chrono::steady_clock::now();
            
            // For debugging/logging
            if (std::abs(freq_adj) > 1.0) { // Only log significant adjustments
                // Would log: "Applied frequency adjustment: {freq_adj} ppb"
            }
        }
    }
}

ClockServo::ServoStats* SynchronizationManager::get_servo_stats(uint16_t port_id) {
    auto it = port_servos_.find(port_id);
    if (it != port_servos_.end()) {
        static ClockServo::ServoStats stats; // Static to return pointer
        stats = it->second->get_statistics();
        return &stats;
    }
    return nullptr;
}

// ============================================================================
// Utility Functions Implementation
// ============================================================================

namespace utils {

std::chrono::nanoseconds timestamp_to_nanoseconds(const Timestamp& timestamp) {
    // IEEE 802.1AS timestamp: seconds since epoch + nanoseconds
    uint64_t total_ns = timestamp.get_seconds() * 1000000000ULL + 
                       static_cast<uint64_t>(timestamp.nanoseconds);
    return std::chrono::nanoseconds(static_cast<int64_t>(total_ns));
}

Timestamp nanoseconds_to_timestamp(std::chrono::nanoseconds nanoseconds) {
    Timestamp timestamp;
    uint64_t total_ns = static_cast<uint64_t>(nanoseconds.count());
    uint64_t seconds = total_ns / 1000000000ULL;
    uint32_t ns = static_cast<uint32_t>(total_ns % 1000000000ULL);
    timestamp.set_seconds(seconds);
    timestamp.nanoseconds = ns;
    return timestamp;
}

std::pair<double, double> calculate_statistics(const std::vector<double>& values) {
    if (values.empty()) {
        return {0.0, 0.0};
    }
    
    // Calculate mean
    double sum = std::accumulate(values.begin(), values.end(), 0.0);
    double mean = sum / values.size();
    
    // Calculate standard deviation
    if (values.size() == 1) {
        return {mean, 0.0};
    }
    
    double variance_sum = 0.0;
    for (double value : values) {
        double diff = value - mean;
        variance_sum += diff * diff;
    }
    
    double variance = variance_sum / (values.size() - 1);
    double std_dev = std::sqrt(variance);
    
    return {mean, std_dev};
}

std::vector<double> median_filter(const std::vector<double>& values, size_t window_size) {
    std::vector<double> filtered;
    
    if (values.size() < window_size) {
        return values; // Not enough data to filter
    }
    
    for (size_t i = 0; i <= values.size() - window_size; ++i) {
        std::vector<double> window(values.begin() + i, values.begin() + i + window_size);
        std::sort(window.begin(), window.end());
        filtered.push_back(window[window_size / 2]); // Median
    }
    
    return filtered;
}

bool is_outlier(double value, double median, double mad, double threshold) {
    if (mad == 0.0) {
        return false; // Can't detect outliers if no variation
    }
    
    // Modified Z-score using MAD
    double modified_z_score = 0.6745 * (value - median) / mad;
    return std::abs(modified_z_score) > threshold;
}

} // namespace utils

} // namespace servo
} // namespace gptp
