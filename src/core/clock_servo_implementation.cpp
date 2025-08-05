/**
 * @file clock_servo_implementation.cpp
 * @brief Complete IEEE 802.1AS clock servo mathematics implementation
 * 
 * Implements PI controller and clock synchronization calculations
 * according to IEEE 802.1AS-2021 requirements.
 */

#include "../../include/clock_servo.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>

namespace gptp {
namespace servo {

/**
 * @brief IEEE 802.1AS-2021 compliant clock servo implementation
 */
class ClockServoImplementation : public ClockServo {
private:
    ServoConfig config_;
    std::deque<SyncMeasurement> measurements_;
    std::deque<OffsetResult> offset_history_;
    
    // PI controller state
    double integral_error_;
    double last_offset_ns_;
    bool servo_locked_;
    size_t consecutive_lock_samples_;
    
    // Statistics
    double mean_offset_;
    double variance_offset_;
    double mean_frequency_adjustment_;
    
public:
    explicit ClockServoImplementation(const ServoConfig& config = ServoConfig())
        : config_(config)
        , integral_error_(0.0)
        , last_offset_ns_(0.0)
        , servo_locked_(false)
        , consecutive_lock_samples_(0)
        , mean_offset_(0.0)
        , variance_offset_(0.0)
        , mean_frequency_adjustment_(0.0) {
    }
    
    /**
     * @brief Add synchronization measurement from Sync/Follow_Up messages
     */
    void add_measurement(const SyncMeasurement& measurement) override {
        measurements_.push_back(measurement);
        
        // Limit history size
        if (measurements_.size() > config_.max_samples) {
            measurements_.pop_front();
        }
        
        // Process the measurement
        process_measurement(measurement);
    }
    
    /**
     * @brief Calculate offset from master clock (IEEE 802.1AS-2021 clause 7.4.2)
     */
    OffsetResult calculate_offset(const SyncMeasurement& measurement) override {
        OffsetResult result;
        
        if (measurements_.empty()) {
            result.valid = false;
            return result;
        }
        
        // IEEE 802.1AS offset calculation:
        // offset = (T2 - T1) - path_delay
        // Where:
        //   T1 = master transmission time (corrected by follow-up)
        //   T2 = slave reception time
        //   path_delay = measured path delay from pDelay mechanism
        
        auto t1_ns = timestamp_to_nanoseconds(measurement.master_timestamp) +
                     timestamp_to_nanoseconds(measurement.correction_field);
        auto t2_ns = timestamp_to_nanoseconds(measurement.local_receipt_time);
        
        // Calculate raw offset
        int64_t raw_offset = static_cast<int64_t>(t2_ns - t1_ns) - 
                            measurement.path_delay.count();
        
        result.offset = std::chrono::nanoseconds(raw_offset);
        result.path_delay = measurement.path_delay;
        result.valid = true;
        
        // Calculate confidence based on measurement stability
        result.confidence = calculate_measurement_confidence();
        
        // Apply outlier filtering
        if (is_outlier(result.offset)) {
            result.valid = false;
            std::cout << "Offset measurement rejected as outlier: " 
                      << result.offset.count() << " ns" << std::endl;
        }
        
        return result;
    }
    
    /**
     * @brief Run PI controller to calculate frequency adjustment
     */
    FrequencyResult run_pi_controller(const OffsetResult& offset) override {
        FrequencyResult result;
        
        if (!offset.valid) {
            result.locked = servo_locked_;
            return result;
        }
        
        double offset_ns = static_cast<double>(offset.offset.count());
        
        // PI Controller implementation (IEEE 802.1AS-2021 Annex B)
        
        // Proportional term
        double proportional_term = config_.proportional_gain * offset_ns;
        
        // Integral term (accumulate error over time)
        integral_error_ += offset_ns;
        
        // Apply integral windup protection
        double max_integral = config_.max_frequency_adjustment / config_.integral_gain;
        integral_error_ = std::clamp(integral_error_, -max_integral, max_integral);
        
        double integral_term = config_.integral_gain * integral_error_;
        
        // Total frequency adjustment in ppb
        result.frequency_adjustment = proportional_term + integral_term;
        
        // Apply limits
        result.frequency_adjustment = std::clamp(
            result.frequency_adjustment,
            -config_.max_frequency_adjustment,
            config_.max_frequency_adjustment
        );
        
        // Phase adjustment (for step corrections)
        result.phase_adjustment = 0.0;
        if (std::abs(offset_ns) > config_.max_phase_adjustment) {
            result.phase_adjustment = offset_ns;
            std::cout << "Large offset detected, applying phase step: " 
                      << result.phase_adjustment << " ns" << std::endl;
        }
        
        // Update servo lock status
        update_servo_lock_status(offset_ns, result.frequency_adjustment);
        result.locked = servo_locked_;
        
        // Update statistics
        update_statistics(offset_ns, result.frequency_adjustment);
        
        last_offset_ns_ = offset_ns;
        
        std::cout << "PI Controller - Offset: " << offset_ns 
                  << " ns, Freq adj: " << result.frequency_adjustment 
                  << " ppb, Locked: " << (result.locked ? "Yes" : "No") << std::endl;
        
        return result;
    }
    
    /**
     * @brief Get current servo status
     */
    ServoStatus get_status() const override {
        ServoStatus status;
        
        status.locked = servo_locked_;
        status.offset_ns = last_offset_ns_;
        status.frequency_adjustment_ppb = mean_frequency_adjustment_;
        status.measurement_count = measurements_.size();
        status.confidence = calculate_measurement_confidence();
        
        return status;
    }
    
    /**
     * @brief Reset servo state
     */
    void reset() override {
        measurements_.clear();
        offset_history_.clear();
        integral_error_ = 0.0;
        last_offset_ns_ = 0.0;
        servo_locked_ = false;
        consecutive_lock_samples_ = 0;
        mean_offset_ = 0.0;
        variance_offset_ = 0.0;
        mean_frequency_adjustment_ = 0.0;
        
        std::cout << "Clock servo reset" << std::endl;
    }
    
private:
    /**
     * @brief Process new measurement through servo pipeline
     */
    void process_measurement(const SyncMeasurement& measurement) {
        // Calculate offset
        OffsetResult offset = calculate_offset(measurement);
        
        if (offset.valid) {
            // Store in history
            offset_history_.push_back(offset);
            if (offset_history_.size() > config_.max_samples) {
                offset_history_.pop_front();
            }
            
            // Run PI controller
            FrequencyResult freq_result = run_pi_controller(offset);
            
            // Apply clock adjustments (this would interface with hardware/OS)
            apply_clock_adjustments(freq_result);
        }
    }
    
    /**
     * @brief Convert Timestamp to nanoseconds since epoch
     */
    uint64_t timestamp_to_nanoseconds(const Timestamp& ts) {
        return ts.get_seconds() * 1000000000ULL + ts.nanoseconds;
    }
    
    /**
     * @brief Check if offset measurement is an outlier
     */
    bool is_outlier(const std::chrono::nanoseconds& offset) {
        if (offset_history_.size() < 3) {
            return false; // Not enough data
        }
        
        double offset_ns = static_cast<double>(offset.count());
        return std::abs(offset_ns - mean_offset_) > config_.outlier_threshold;
    }
    
    /**
     * @brief Calculate measurement confidence (0.0 to 1.0)
     */
    double calculate_measurement_confidence() const {
        if (offset_history_.size() < 2) {
            return 0.0;
        }
        
        // Confidence based on measurement stability
        double stability = 1.0 / (1.0 + sqrt(variance_offset_) / 1000000.0); // Convert to ms
        
        // Factor in servo lock status
        double lock_factor = servo_locked_ ? 1.0 : 0.5;
        
        // Factor in measurement count
        double count_factor = std::min(1.0, static_cast<double>(offset_history_.size()) / config_.max_samples);
        
        return stability * lock_factor * count_factor;
    }
    
    /**
     * @brief Update servo lock detection
     */
    void update_servo_lock_status(double offset_ns, double freq_adj_ppb) {
        bool offset_stable = std::abs(offset_ns) < (config_.lock_threshold * 1000.0); // Convert ppb to ns
        bool freq_stable = std::abs(freq_adj_ppb) < config_.lock_threshold;
        
        if (offset_stable && freq_stable) {
            consecutive_lock_samples_++;
            if (consecutive_lock_samples_ >= config_.lock_samples && !servo_locked_) {
                servo_locked_ = true;
                std::cout << "Clock servo achieved lock" << std::endl;
            }
        } else {
            consecutive_lock_samples_ = 0;
            if (servo_locked_) {
                servo_locked_ = false;
                std::cout << "Clock servo lost lock" << std::endl;
            }
        }
    }
    
    /**
     * @brief Update running statistics
     */
    void update_statistics(double offset_ns, double freq_adj_ppb) {
        // Running mean and variance calculation (Welford's method)
        size_t n = offset_history_.size();
        if (n == 1) {
            mean_offset_ = offset_ns;
            variance_offset_ = 0.0;
        } else {
            double delta = offset_ns - mean_offset_;
            mean_offset_ += delta / n;
            double delta2 = offset_ns - mean_offset_;
            variance_offset_ += (delta * delta2 - variance_offset_) / n;
        }
        
        // Update frequency adjustment mean
        mean_frequency_adjustment_ = (mean_frequency_adjustment_ * (n - 1) + freq_adj_ppb) / n;
    }
    
    /**
     * @brief Apply clock adjustments to system/hardware
     */
    void apply_clock_adjustments(const FrequencyResult& result) {
        // This would interface with:
        // 1. Hardware timestamping implementation for hardware clock adjustment
        // 2. System clock APIs for software clock adjustment
        // 3. Clock discipline algorithms
        
        if (result.phase_adjustment != 0.0) {
            std::cout << "Applying phase adjustment: " << result.phase_adjustment << " ns" << std::endl;
            // Hardware/system specific phase step
        }
        
        if (result.frequency_adjustment != 0.0) {
            std::cout << "Applying frequency adjustment: " << result.frequency_adjustment << " ppb" << std::endl;
            // Hardware/system specific frequency adjustment
        }
    }
};

/**
 * @brief Synchronization Manager Implementation
 */
class SynchronizationManagerImpl : public SynchronizationManager {
private:
    std::unique_ptr<ClockServoImplementation> servo_;
    SyncStatus current_status_;
    std::chrono::steady_clock::time_point last_sync_time_;
    std::chrono::milliseconds sync_timeout_;
    
public:
    SynchronizationManagerImpl(const ServoConfig& config = ServoConfig())
        : servo_(std::make_unique<ClockServoImplementation>(config))
        , current_status_(SyncStatus::INITIALIZING)
        , sync_timeout_(std::chrono::milliseconds(3000)) { // 3 second timeout
    }
    
    void process_sync_message(const SyncMessage& sync, const Timestamp& receipt_time) override {
        SyncMeasurement measurement;
        measurement.master_timestamp = sync.originTimestamp;
        measurement.local_receipt_time = receipt_time;
        measurement.measurement_time = std::chrono::steady_clock::now();
        
        // Path delay would be provided by LinkDelay state machine
        measurement.path_delay = std::chrono::nanoseconds(0); // TODO: Get from path delay calculator
        
        servo_->add_measurement(measurement);
        
        last_sync_time_ = measurement.measurement_time;
        update_sync_status();
    }
    
    void process_followup_message(const FollowUpMessage& followup) override {
        // Update the last measurement with precise timestamp
        if (!servo_->get_measurements().empty()) {
            auto& last_measurement = const_cast<SyncMeasurement&>(servo_->get_measurements().back());
            last_measurement.correction_field = followup.preciseOriginTimestamp;
        }
    }
    
    void set_path_delay(std::chrono::nanoseconds delay) override {
        // Update path delay for future measurements
        if (!servo_->get_measurements().empty()) {
            auto& last_measurement = const_cast<SyncMeasurement&>(servo_->get_measurements().back());
            last_measurement.path_delay = delay;
        }
        
        std::cout << "Path delay updated: " << delay.count() << " ns" << std::endl;
    }
    
    SyncStatus get_sync_status() const override {
        return current_status_;
    }
    
    ServoStatus get_servo_status() const override {
        return servo_->get_status();
    }
    
    void reset() override {
        servo_->reset();
        current_status_ = SyncStatus::INITIALIZING;
        std::cout << "Synchronization manager reset" << std::endl;
    }
    
private:
    void update_sync_status() {
        auto now = std::chrono::steady_clock::now();
        auto time_since_last_sync = now - last_sync_time_;
        
        if (time_since_last_sync > sync_timeout_) {
            current_status_ = SyncStatus::TIMEOUT;
        } else if (servo_->get_status().locked) {
            current_status_ = SyncStatus::SYNCHRONIZED;
        } else {
            current_status_ = SyncStatus::SYNCHRONIZING;
        }
    }
};

// Factory methods
std::unique_ptr<ClockServo> ClockServo::create(const ServoConfig& config) {
    return std::make_unique<ClockServoImplementation>(config);
}

std::unique_ptr<SynchronizationManager> SynchronizationManager::create(const ServoConfig& config) {
    return std::make_unique<SynchronizationManagerImpl>(config);
}

} // namespace servo
} // namespace gptp
