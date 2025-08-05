/**
 * @file clock_servo.hpp
 * @brief Clock synchronization servo implementation
 * 
 * Implements clock synchronization mathematics for IEEE 802.1AS-2021
 * including offset calculation, frequency adjustment, and PI controller.
 */

#pragma once

#include "gptp_protocol.hpp"
#include <chrono>
#include <deque>
#include <vector>
#include <map>
#include <memory>

namespace gptp {
namespace servo {

/**
 * @brief Clock synchronization measurement
 */
struct SyncMeasurement {
    Timestamp master_timestamp;      // T1 from sync message
    Timestamp local_receipt_time;    // T2 local reception time
    Timestamp correction_field;      // From follow_up message
    std::chrono::nanoseconds path_delay; // From pDelay mechanism
    std::chrono::steady_clock::time_point measurement_time;
    
    SyncMeasurement() : path_delay(0) {}
};

/**
 * @brief Clock offset calculation result
 */
struct OffsetResult {
    std::chrono::nanoseconds offset;     // Master-slave offset
    std::chrono::nanoseconds path_delay; // Network path delay
    bool valid;                          // Whether calculation is valid
    double confidence;                   // Confidence in measurement (0-1)
    
    OffsetResult() : offset(0), path_delay(0), valid(false), confidence(0.0) {}
};

/**
 * @brief Frequency adjustment result
 */
struct FrequencyResult {
    double frequency_adjustment; // Parts per billion (ppb) adjustment
    double phase_adjustment;     // Phase adjustment in nanoseconds
    bool locked;                // Whether servo is locked to master
    
    FrequencyResult() : frequency_adjustment(0.0), phase_adjustment(0.0), locked(false) {}
};

/**
 * @brief Clock servo configuration
 */
struct ServoConfig {
    // PI controller gains
    double proportional_gain;    // Kp for phase correction
    double integral_gain;        // Ki for frequency correction
    
    // Filtering parameters
    size_t max_samples;          // Maximum samples to keep for filtering
    double outlier_threshold;    // Threshold for outlier detection (nanoseconds)
    
    // Lock detection
    double lock_threshold;       // Threshold for considering servo locked (ppb)
    size_t lock_samples;         // Number of consecutive samples needed for lock
    
    // Limits
    double max_frequency_adjustment; // Maximum frequency adjustment (ppb)
    double max_phase_adjustment;     // Maximum phase adjustment (nanoseconds)
    
    ServoConfig() {
        // Default gPTP servo parameters
        proportional_gain = 0.7;
        integral_gain = 0.3;
        max_samples = 16;
        outlier_threshold = 1000000.0; // 1ms
        lock_threshold = 10.0;         // 10 ppb
        lock_samples = 8;
        max_frequency_adjustment = 100000.0; // 100 ppm
        max_phase_adjustment = 1000000.0;    // 1ms
    }
};

/**
 * @brief Clock synchronization servo
 * 
 * Implements PI controller for IEEE 802.1AS clock synchronization
 */
class ClockServo {
public:
    explicit ClockServo(const ServoConfig& config = ServoConfig());
    ~ClockServo() = default;
    
    /**
     * @brief Calculate master-slave offset from sync measurement
     * @param measurement Sync measurement data
     * @return Offset calculation result
     */
    OffsetResult calculate_offset(const SyncMeasurement& measurement);
    
    /**
     * @brief Update servo with new offset measurement
     * @param offset Measured offset from master
     * @param measurement_time When the measurement was taken
     * @return Frequency/phase adjustment recommendation
     */
    FrequencyResult update_servo(std::chrono::nanoseconds offset,
                                std::chrono::steady_clock::time_point measurement_time);
    
    /**
     * @brief Reset servo state
     */
    void reset();
    
    /**
     * @brief Check if servo is locked to master
     * @return True if servo is locked
     */
    bool is_locked() const { return locked_; }
    
    /**
     * @brief Get current frequency adjustment
     * @return Current frequency adjustment in ppb
     */
    double get_frequency_adjustment() const { return current_frequency_adjustment_; }
    
    /**
     * @brief Get servo statistics
     * @return Statistics about servo performance
     */
    struct ServoStats {
        size_t sample_count;
        std::chrono::nanoseconds mean_offset;
        std::chrono::nanoseconds std_deviation;
        double current_frequency_ppb;
        bool is_locked;
        std::chrono::steady_clock::time_point last_update;
    };
    
    ServoStats get_statistics() const;
    
    /**
     * @brief Configure servo parameters
     * @param config New configuration
     */
    void configure(const ServoConfig& config) { config_ = config; }

private:
    /**
     * @brief Filter offset measurements to remove outliers
     * @param offset New offset measurement
     * @return Filtered offset, or nullopt if outlier
     */
    bool filter_offset(std::chrono::nanoseconds offset);
    
    /**
     * @brief Calculate statistics from recent measurements
     */
    void calculate_statistics();
    
    /**
     * @brief Update lock detection
     */
    void update_lock_detection();
    
    ServoConfig config_;
    
    // Offset measurement history
    std::deque<std::chrono::nanoseconds> offset_history_;
    std::deque<std::chrono::steady_clock::time_point> time_history_;
    
    // PI controller state
    double integral_accumulator_;
    std::chrono::nanoseconds previous_offset_;
    std::chrono::steady_clock::time_point previous_time_;
    
    // Current adjustments
    double current_frequency_adjustment_; // ppb
    double current_phase_adjustment_;     // nanoseconds
    
    // Lock detection
    bool locked_;
    size_t consecutive_good_samples_;
    
    // Statistics
    std::chrono::nanoseconds mean_offset_;
    std::chrono::nanoseconds std_deviation_;
    std::chrono::steady_clock::time_point last_update_;
    bool first_measurement_;
};

/**
 * @brief Clock synchronization manager
 * 
 * Manages clock synchronization for multiple ports and integrates
 * with BMCA for master selection.
 */
class SynchronizationManager {
public:
    SynchronizationManager();
    ~SynchronizationManager() = default;
    
    /**
     * @brief Process sync/follow_up message pair
     * @param port_id Port that received the messages
     * @param sync_msg Sync message
     * @param sync_receipt_time When sync was received
     * @param followup_msg Follow_up message
     * @param path_delay Current path delay for this port
     */
    void process_sync_followup(uint16_t port_id,
                              const SyncMessage& sync_msg,
                              const Timestamp& sync_receipt_time,
                              const FollowUpMessage& followup_msg,
                              std::chrono::nanoseconds path_delay);
    
    /**
     * @brief Set which port is the current slave port
     * @param port_id Port ID, or 0 for no slave port
     */
    void set_slave_port(uint16_t port_id);
    
    /**
     * @brief Get current synchronization status
     * @return Sync status for slave port
     */
    struct SyncStatus {
        bool synchronized;
        std::chrono::nanoseconds current_offset;
        double frequency_adjustment_ppb;
        bool servo_locked;
        std::chrono::steady_clock::time_point last_sync_time;
        uint16_t slave_port_id;
    };
    
    SyncStatus get_sync_status() const;
    
    /**
     * @brief Apply clock adjustments to system
     * This would integrate with platform-specific clock adjustment APIs
     */
    void apply_clock_adjustments();
    
    /**
     * @brief Get servo statistics for a port
     * @param port_id Port identifier
     * @return Servo statistics if available
     */
    ClockServo::ServoStats* get_servo_stats(uint16_t port_id);

private:
    std::map<uint16_t, std::unique_ptr<ClockServo>> port_servos_;
    uint16_t current_slave_port_;
    SyncStatus current_status_;
    
    // Statistics
    std::chrono::steady_clock::time_point last_adjustment_time_;
    size_t total_adjustments_;
};

/**
 * @brief Utility functions for clock synchronization
 */
namespace utils {

/**
 * @brief Convert Timestamp to nanoseconds
 * @param timestamp IEEE 802.1AS timestamp
 * @return Nanoseconds since epoch
 */
std::chrono::nanoseconds timestamp_to_nanoseconds(const Timestamp& timestamp);

/**
 * @brief Convert nanoseconds to Timestamp
 * @param nanoseconds Nanoseconds since epoch
 * @return IEEE 802.1AS timestamp
 */
Timestamp nanoseconds_to_timestamp(std::chrono::nanoseconds nanoseconds);

/**
 * @brief Calculate mean and standard deviation
 * @param values Vector of values
 * @return Pair of (mean, std_deviation)
 */
std::pair<double, double> calculate_statistics(const std::vector<double>& values);

/**
 * @brief Apply median filter to remove outliers
 * @param values Input values
 * @param window_size Window size for median filter
 * @return Filtered values
 */
std::vector<double> median_filter(const std::vector<double>& values, size_t window_size);

/**
 * @brief Check if value is an outlier using modified Z-score
 * @param value Value to check
 * @param median Median of dataset
 * @param mad Median absolute deviation
 * @param threshold Threshold for outlier detection (typically 3.5)
 * @return True if value is outlier
 */
bool is_outlier(double value, double median, double mad, double threshold = 3.5);

} // namespace utils

} // namespace servo
} // namespace gptp
