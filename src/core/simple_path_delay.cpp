/**
 * @file simple_path_delay.cpp
 * @brief Simplified IEEE 802.1AS-2021 Path Delay Implementation
 */

#include "../../include/simple_path_delay.hpp"
#include <algorithm>
#include <iostream>

namespace gptp {
namespace path_delay {

// ============================================================================
// PathDelayCalculator Implementation
// ============================================================================

PathDelayCalculator::PathDelayCalculator()
    : current_rate_ratio_(1.0)
    , last_mean_delay_(std::chrono::nanoseconds::zero())
    , rate_ratio_window_size_(8)  // Default window size
{
    ratio_measurements_.reserve(32);
}

double PathDelayCalculator::calculate_neighbor_rate_ratio_eq16_1(
    const std::vector<Timestamp>& t_rsp3_measurements,
    const std::vector<Timestamp>& t_req4_measurements,
    size_t N) {
    
    if (t_rsp3_measurements.size() < N + 1 || t_req4_measurements.size() < N + 1) {
        return current_rate_ratio_;  // Not enough measurements
    }
    
    // IEEE 802.1AS-2021 Equation 16-1
    return equations::neighbor_rate_ratio_eq16_1(
        t_rsp3_measurements[N], t_rsp3_measurements[0],
        t_req4_measurements[N], t_req4_measurements[0]
    );
}

std::chrono::nanoseconds PathDelayCalculator::calculate_mean_link_delay_eq16_2(
    const Timestamp& t_req1,
    const Timestamp& t_rsp2,  
    const Timestamp& t_rsp3,
    const Timestamp& t_req4,
    double r) {
    
    // IEEE 802.1AS-2021 Equation 16-2
    return equations::mean_link_delay_eq16_2(t_req1, t_rsp2, t_rsp3, t_req4, r);
}

SimplePathDelayResult PathDelayCalculator::calculate_p2p_path_delay(
    const Timestamp& t1,
    const Timestamp& t2,
    const Timestamp& t3,
    const Timestamp& t4) {
    
    SimplePathDelayResult result;
    
    if (!validate_timestamps(t1, t2, t3, t4)) {
        return result;  // Invalid timestamps
    }
    
    // Calculate mean link delay using current rate ratio
    result.mean_link_delay = calculate_mean_link_delay_eq16_2(t1, t2, t3, t4, current_rate_ratio_);
    result.neighbor_rate_ratio = current_rate_ratio_;
    result.valid = true;
    
    last_mean_delay_ = result.mean_link_delay;
    return result;
}

void PathDelayCalculator::update_rate_ratio(const Timestamp& t_rsp3, const Timestamp& t_req4) {
    // Store measurement
    RatioMeasurement measurement;
    measurement.t_rsp3 = t_rsp3;
    measurement.t_req4 = t_req4;
    measurement.time = std::chrono::steady_clock::now();
    
    ratio_measurements_.push_back(measurement);
    
    // Keep only recent measurements  
    if (ratio_measurements_.size() > rate_ratio_window_size_ * 2) {
        ratio_measurements_.erase(ratio_measurements_.begin());
    }
    
    // Update rate ratio if we have enough measurements
    if (ratio_measurements_.size() >= rate_ratio_window_size_ + 1) {
        std::vector<Timestamp> t_rsp3_vec, t_req4_vec;
        
        // Extract timestamps from recent measurements
        size_t start_idx = ratio_measurements_.size() - rate_ratio_window_size_ - 1;
        for (size_t i = start_idx; i < ratio_measurements_.size(); ++i) {
            t_rsp3_vec.push_back(ratio_measurements_[i].t_rsp3);
            t_req4_vec.push_back(ratio_measurements_[i].t_req4);
        }
        
        double new_rate_ratio = calculate_neighbor_rate_ratio_eq16_1(
            t_rsp3_vec, t_req4_vec, rate_ratio_window_size_
        );
        
        // Validate rate ratio (IEEE 802.1AS 200 ppm constraint)
        if (equations::validate_rate_ratio(new_rate_ratio)) {
            current_rate_ratio_ = new_rate_ratio;
            
            std::cout << "[PathDelay] Updated neighbor rate ratio: " << current_rate_ratio_ 
                      << " (offset: " << equations::frequency_offset_ppm(current_rate_ratio_) 
                      << " ppm)" << std::endl;
        }
    }
}

bool PathDelayCalculator::validate_timestamps(const Timestamp& t1, const Timestamp& t2, 
                                            const Timestamp& t3, const Timestamp& t4) const {
    // Check temporal ordering: t1 < t2 < t3 < t4
    auto t1_ns = t1.to_nanoseconds();
    auto t2_ns = t2.to_nanoseconds();
    auto t3_ns = t3.to_nanoseconds();
    auto t4_ns = t4.to_nanoseconds();
    
    if (!(t1_ns < t2_ns && t2_ns < t3_ns && t3_ns < t4_ns)) {
        return false;
    }
    
    // Check reasonable turnaround time (< 100ms)
    auto total_turnaround = t4_ns - t1_ns;
    if (total_turnaround > std::chrono::milliseconds(100)) {
        return false;
    }
    
    return true;
}

// ============================================================================
// NativeCSNPathDelay Implementation
// ============================================================================

NativeCSNPathDelay::NativeCSNPathDelay(CSNMDEntity& md_entity)
    : md_entity_(md_entity) {
}

void NativeCSNPathDelay::set_native_path_delay(std::chrono::nanoseconds delay, double rate_ratio) {
    // IEEE 802.1AS-2021 Section 16.4.3.3 - populate MD entity variables
    md_entity_.asCapable = true;
    md_entity_.neighborRateRatio = rate_ratio;
    md_entity_.meanLinkDelay = delay;
    md_entity_.computeNeighborRateRatio = false;  // Native measurement provides this
    md_entity_.computeMeanLinkDelay = false;      // Native measurement provides this
    md_entity_.isMeasuringDelay = true;           // CSN is measuring delay
}

void NativeCSNPathDelay::configure_for_native_measurement() {
    // Configure MD entity for native CSN measurement
    md_entity_.asCapable = true;
    md_entity_.computeNeighborRateRatio = false;
    md_entity_.computeMeanLinkDelay = false;
    md_entity_.isMeasuringDelay = true;
    
    std::cout << "[NativeCSN] Configured for native path delay measurement" << std::endl;
}

SimplePathDelayResult NativeCSNPathDelay::get_path_delay_result() const {
    SimplePathDelayResult result;
    
    if (md_entity_.asCapable) {
        result.mean_link_delay = md_entity_.meanLinkDelay;
        result.neighbor_rate_ratio = md_entity_.neighborRateRatio;
        result.valid = true;
    }
    
    return result;
}

// ============================================================================
// IntrinsicCSNPathDelay Implementation
// ============================================================================

IntrinsicCSNPathDelay::IntrinsicCSNPathDelay()
    : residence_time_(std::chrono::nanoseconds::zero())
    , b1_compliant_(false) {
}

void IntrinsicCSNPathDelay::set_residence_time(std::chrono::nanoseconds residence_time) {
    residence_time_ = residence_time;
    
    std::cout << "[IntrinsicCSN] Set residence time: " << residence_time_.count() 
              << " ns" << std::endl;
}

SimplePathDelayResult IntrinsicCSNPathDelay::get_path_delay_result() const {
    SimplePathDelayResult result;
    
    if (b1_compliant_) {
        // IEEE 802.1AS-2021 Section 16.4.3.4
        // Path delay is treated as part of residence time of distributed system
        result.mean_link_delay = std::chrono::nanoseconds::zero();  // No separate path delay
        result.neighbor_rate_ratio = 1.0;  // Perfect synchronization
        result.valid = true;
        
        std::cout << "[IntrinsicCSN] Path delay integrated into residence time ("
                  << residence_time_.count() << " ns)" << std::endl;
    }
    
    return result;
}

} // namespace path_delay
} // namespace gptp
