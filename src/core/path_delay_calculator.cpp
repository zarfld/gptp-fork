/**
 * @file path_delay_calculator.cpp
 * @brief IEEE 802.1AS-2021 Path Delay Calculation Implementation
 */

#include "../../include/path_delay_calculator.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <fstream>
#include <numeric>
#include <vector>

namespace gptp {
namespace path_delay {

// ============================================================================
// StandardP2PDelayCalculator Implementation
// ============================================================================

StandardP2PDelayCalculator::StandardP2PDelayCalculator(uint8_t domain_number)
    : domain_number_(domain_number)
    , measurement_interval_(std::chrono::seconds(1))
    , rate_ratio_window_size_(8)
    , max_path_delay_(std::chrono::milliseconds(100))  // 100ms max reasonable path delay
    , max_rate_ratio_change_(0.001)  // 1000 ppm max change
    , current_neighbor_rate_ratio_(1.0)
    , last_mean_link_delay_(std::chrono::nanoseconds::zero())
{
    measurement_history_.reserve(256);  // Reserve space for measurements
    timestamp_history_.reserve(256);
}

PathDelayResult StandardP2PDelayCalculator::calculate_path_delay(const PdelayTimestamps& timestamps) {
    PathDelayResult result;
    
    if (!is_measurement_valid(timestamps)) {
        return result;  // Invalid measurement
    }
    
    // Calculate mean link delay using Equation 16-2
    result.mean_link_delay = calculate_mean_link_delay(timestamps, current_neighbor_rate_ratio_);
    result.neighbor_rate_ratio = current_neighbor_rate_ratio_;
    result.valid = true;
    result.measurement_time = std::chrono::steady_clock::now();
    
    // Calculate confidence based on measurement consistency
    if (timestamp_history_.size() >= 3) {
        // Use recent measurements to estimate confidence
        std::vector<std::chrono::nanoseconds> recent_delays;
        size_t start_idx = std::max(0, static_cast<int>(timestamp_history_.size()) - 5);
        
        for (size_t i = start_idx; i < timestamp_history_.size(); ++i) {
            auto delay = calculate_mean_link_delay(timestamp_history_[i], current_neighbor_rate_ratio_);
            recent_delays.push_back(delay);
        }
        
        // Calculate standard deviation
        auto mean_delay = std::accumulate(recent_delays.begin(), recent_delays.end(), 
                                        std::chrono::nanoseconds::zero()) / recent_delays.size();
        
        double variance = 0.0;
        for (const auto& delay : recent_delays) {
            auto diff = (delay - mean_delay).count();
            variance += diff * diff;
        }
        variance /= recent_delays.size();
        double std_dev = std::sqrt(variance);
        
        // Confidence based on stability (inverse of coefficient of variation)
        double cv = (mean_delay.count() > 0) ? std_dev / mean_delay.count() : 1.0;
        result.confidence = std::max(0.0, std::min(1.0, 1.0 - cv));
    } else {
        result.confidence = 0.5;  // Default confidence for early measurements
    }
    
    // Store timestamp for history
    timestamp_history_.push_back(timestamps);
    if (timestamp_history_.size() > 64) {
        timestamp_history_.erase(timestamp_history_.begin());
    }
    
    // Store measurement data for rate ratio calculation
    MeasurementData measurement;
    measurement.t_rsp3 = timestamps.t3;
    measurement.t_req4 = timestamps.t4;
    measurement.measurement_time = result.measurement_time;
    measurement.sequence_id = timestamps.sequence_id;
    
    measurement_history_.push_back(measurement);
    if (measurement_history_.size() > 128) {
        measurement_history_.erase(measurement_history_.begin());
    }
    
    last_mean_link_delay_ = result.mean_link_delay;
    return result;
}

void StandardP2PDelayCalculator::update_neighbor_rate_ratio(const std::vector<PdelayTimestamps>& measurements) {
    if (measurements.size() < rate_ratio_window_size_ + 1) {
        return;  // Not enough measurements
    }
    
    // Convert to MeasurementData format
    std::vector<MeasurementData> measurement_data;
    for (const auto& ts : measurements) {
        MeasurementData data;
        data.t_rsp3 = ts.t3;
        data.t_req4 = ts.t4;
        data.sequence_id = ts.sequence_id;
        measurement_data.push_back(data);
    }
    
    // Calculate neighbor rate ratio using Equation 16-1
    double new_rate_ratio = calculate_neighbor_rate_ratio(measurement_data, rate_ratio_window_size_);
    
    // Validate the new rate ratio
    double change = std::abs(new_rate_ratio - current_neighbor_rate_ratio_);
    if (change <= max_rate_ratio_change_) {
        current_neighbor_rate_ratio_ = new_rate_ratio;
    }
}

bool StandardP2PDelayCalculator::is_measurement_valid(const PdelayTimestamps& timestamps) const {
    return validate_timestamps(timestamps);
}

void StandardP2PDelayCalculator::set_measurement_interval(std::chrono::nanoseconds interval) {
    measurement_interval_ = interval;
}

void StandardP2PDelayCalculator::set_rate_ratio_calculation_window(size_t N) {
    rate_ratio_window_size_ = std::max(size_t(2), N);  // Minimum window size of 2
}

void StandardP2PDelayCalculator::set_validation_thresholds(std::chrono::nanoseconds max_delay, double max_rate_ratio_change) {
    max_path_delay_ = max_delay;
    max_rate_ratio_change_ = max_rate_ratio_change;
}

double StandardP2PDelayCalculator::calculate_neighbor_rate_ratio(const std::vector<MeasurementData>& measurements, size_t N) {
    if (measurements.size() < N + 1) {
        return current_neighbor_rate_ratio_;  // Not enough data
    }
    
    // IEEE 802.1AS-2021 Equation 16-1
    // neighborRateRatio = (t_rsp3_N - t_rsp3_0) / (t_req4_N - t_req4_0)
    
    const auto& first = measurements[0];
    const auto& last = measurements[N];
    
    auto t_rsp3_diff = last.t_rsp3.to_nanoseconds() - first.t_rsp3.to_nanoseconds();
    auto t_req4_diff = last.t_req4.to_nanoseconds() - first.t_req4.to_nanoseconds();
    
    if (t_req4_diff.count() == 0) {
        return current_neighbor_rate_ratio_;  // Avoid division by zero
    }
    
    double rate_ratio = static_cast<double>(t_rsp3_diff.count()) / static_cast<double>(t_req4_diff.count());
    
    // Sanity check: rate ratio should be close to 1.0 (within 200 ppm per IEEE 802.1AS)
    if (rate_ratio > 0.9998 && rate_ratio < 1.0002) {
        return rate_ratio;
    }
    
    return current_neighbor_rate_ratio_;  // Invalid rate ratio
}

std::chrono::nanoseconds StandardP2PDelayCalculator::calculate_mean_link_delay(const PdelayTimestamps& timestamps, double rate_ratio) {
    // IEEE 802.1AS-2021 Equation 16-2
    // meanLinkDelay = ((t_req4 - t_req1) * r - (t_rsp3 - t_rsp2)) / 2
    
    auto t_req1 = timestamps.t1.to_nanoseconds();
    auto t_rsp2 = timestamps.t2.to_nanoseconds();
    auto t_rsp3 = timestamps.t3.to_nanoseconds();
    auto t_req4 = timestamps.t4.to_nanoseconds();
    
    auto initiator_turnaround = t_req4 - t_req1;
    auto responder_residence = t_rsp3 - t_rsp2;
    
    // Apply rate ratio correction to initiator measurement
    auto corrected_initiator_time = std::chrono::nanoseconds(
        static_cast<int64_t>(initiator_turnaround.count() * rate_ratio)
    );
    
    auto mean_link_delay = (corrected_initiator_time - responder_residence) / 2;
    
    // Ensure non-negative delay
    return std::max(std::chrono::nanoseconds::zero(), mean_link_delay);
}

bool StandardP2PDelayCalculator::validate_timestamps(const PdelayTimestamps& timestamps) const {
    // Check if all required timestamps are valid
    if (!timestamps.t2_valid || !timestamps.t3_valid) {
        return false;
    }
    
    // Check temporal ordering: t1 < t2 < t3 < t4
    auto t1_ns = timestamps.t1.to_nanoseconds();
    auto t2_ns = timestamps.t2.to_nanoseconds();
    auto t3_ns = timestamps.t3.to_nanoseconds();
    auto t4_ns = timestamps.t4.to_nanoseconds();
    
    if (!(t1_ns < t2_ns && t2_ns < t3_ns && t3_ns < t4_ns)) {
        return false;
    }
    
    // Check reasonable turnaround time
    auto turnaround_time = t4_ns - t1_ns;
    if (turnaround_time > max_path_delay_ * 2) {  // Conservative check
        return false;
    }
    
    return true;
}

// ============================================================================
// NativeCSNDelayCalculator Implementation
// ============================================================================

NativeCSNDelayCalculator::NativeCSNDelayCalculator(NativeDelayProvider provider)
    : native_provider_(provider)
    , as_capable_(true)
    , neighbor_rate_ratio_(1.0)
    , mean_link_delay_(std::chrono::nanoseconds::zero())
    , compute_neighbor_rate_ratio_(false)
    , compute_mean_link_delay_(false)
    , is_measuring_delay_(true)
{
}

PathDelayResult NativeCSNDelayCalculator::calculate_path_delay(const PdelayTimestamps& timestamps) {
    // Use native CSN measurement instead of protocol-based calculation
    return native_provider_();
}

void NativeCSNDelayCalculator::update_neighbor_rate_ratio(const std::vector<PdelayTimestamps>& measurements) {
    // Native CSN provides its own rate ratio, no protocol-based calculation needed
    if (!compute_neighbor_rate_ratio_) {
        return;
    }
    
    // If compute flag is set, could implement CSN-specific rate ratio calculation here
}

bool NativeCSNDelayCalculator::is_measurement_valid(const PdelayTimestamps& timestamps) const {
    // For native CSN, validity depends on the native measurement system
    return as_capable_ && is_measuring_delay_;
}

void NativeCSNDelayCalculator::set_compute_flags(bool compute_rate_ratio, bool compute_mean_delay) {
    compute_neighbor_rate_ratio_ = compute_rate_ratio;
    compute_mean_link_delay_ = compute_mean_delay;
}

// ============================================================================
// IntrinsicCSNDelayCalculator Implementation
// ============================================================================

IntrinsicCSNDelayCalculator::IntrinsicCSNDelayCalculator()
    : residence_time_(std::chrono::nanoseconds::zero())
    , synchronized_csn_time_complies_b1_(true)
{
}

PathDelayResult IntrinsicCSNDelayCalculator::calculate_path_delay(const PdelayTimestamps& timestamps) {
    PathDelayResult result;
    
    if (synchronized_csn_time_complies_b1_) {
        // Path delay is treated as part of residence time
        result.mean_link_delay = residence_time_;
        result.neighbor_rate_ratio = 1.0;  // Perfect synchronization
        result.valid = true;
        result.measurement_time = std::chrono::steady_clock::now();
        result.confidence = 1.0;  // High confidence due to intrinsic sync
    }
    
    return result;
}

void IntrinsicCSNDelayCalculator::update_neighbor_rate_ratio(const std::vector<PdelayTimestamps>& measurements) {
    // No rate ratio calculation needed for intrinsic CSN
}

bool IntrinsicCSNDelayCalculator::is_measurement_valid(const PdelayTimestamps& timestamps) const {
    return synchronized_csn_time_complies_b1_;
}

void IntrinsicCSNDelayCalculator::set_residence_time(std::chrono::nanoseconds residence_time) {
    residence_time_ = residence_time;
}

// ============================================================================
// PathDelayManager Implementation
// ============================================================================

PathDelayManager::PathDelayManager() {
}

void PathDelayManager::add_csn_node(const ClockIdentity& node_id, std::unique_ptr<IPathDelayCalculator> calculator) {
    std::lock_guard<std::mutex> lock(calculators_mutex_);
    
    NodeCalculator node_calc;
    node_calc.calculator = std::move(calculator);
    node_calc.node_info.node_identity = node_id;
    node_calc.node_info.last_update = std::chrono::steady_clock::now();
    
    node_calculators_[node_id] = std::move(node_calc);
}

void PathDelayManager::remove_csn_node(const ClockIdentity& node_id) {
    std::lock_guard<std::mutex> lock(calculators_mutex_);
    node_calculators_.erase(node_id);
}

PathDelayResult PathDelayManager::calculate_path_delay_to_node(const ClockIdentity& node_id, const PdelayTimestamps& timestamps) {
    std::lock_guard<std::mutex> lock(calculators_mutex_);
    
    auto it = node_calculators_.find(node_id);
    if (it == node_calculators_.end()) {
        return PathDelayResult();  // Node not found
    }
    
    auto result = it->second.calculator->calculate_path_delay(timestamps);
    
    if (result.valid) {
        // Update node info
        it->second.node_info.propagation_delay = result.mean_link_delay;
        it->second.node_info.rate_ratio = result.neighbor_rate_ratio;
        it->second.node_info.last_update = result.measurement_time;
        
        // Store recent result
        it->second.recent_results.push_back(result);
        if (it->second.recent_results.size() > 32) {
            it->second.recent_results.erase(it->second.recent_results.begin());
        }
    }
    
    return result;
}

void PathDelayManager::update_neighbor_rate_ratios() {
    std::lock_guard<std::mutex> lock(calculators_mutex_);
    
    for (auto& pair : node_calculators_) {
        // Convert recent results to timestamps for rate ratio calculation
        std::vector<PdelayTimestamps> timestamps;
        // This would need to be populated from stored measurement data
        // For now, we'll just call the update function with empty data
        pair.second.calculator->update_neighbor_rate_ratio(timestamps);
    }
}

std::vector<ClockIdentity> PathDelayManager::get_active_nodes() const {
    std::lock_guard<std::mutex> lock(calculators_mutex_);
    
    std::vector<ClockIdentity> active_nodes;
    auto now = std::chrono::steady_clock::now();
    
    for (const auto& pair : node_calculators_) {
        // Consider node active if updated within last 10 seconds
        if (now - pair.second.node_info.last_update < std::chrono::seconds(10)) {
            active_nodes.push_back(pair.first);
        }
    }
    
    return active_nodes;
}

DelayMeasurementMethod PathDelayManager::get_node_measurement_method(const ClockIdentity& node_id) const {
    std::lock_guard<std::mutex> lock(calculators_mutex_);
    
    auto it = node_calculators_.find(node_id);
    if (it != node_calculators_.end()) {
        return it->second.calculator->get_method();
    }
    
    return DelayMeasurementMethod::PEER_TO_PEER_PROTOCOL;
}

void PathDelayManager::print_path_delay_statistics() const {
    std::lock_guard<std::mutex> lock(calculators_mutex_);
    
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "Path Delay Statistics" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    for (const auto& pair : node_calculators_) {
        const auto& node_info = pair.second.node_info;
        const auto& calculator = pair.second.calculator;
        
        std::cout << "\nNode: ";
        for (int i = 0; i < 8; ++i) {
            std::cout << std::hex << static_cast<int>(pair.first.id[i]);
            if (i < 7) std::cout << ":";
        }
        std::cout << std::dec << std::endl;
        
        std::string method_str;
        switch (calculator->get_method()) {
            case DelayMeasurementMethod::PEER_TO_PEER_PROTOCOL:
                method_str = "Peer-to-Peer Protocol";
                break;
            case DelayMeasurementMethod::NATIVE_CSN_MEASUREMENT:
                method_str = "Native CSN Measurement";
                break;
            case DelayMeasurementMethod::INTRINSIC_CSN_SYNC:
                method_str = "Intrinsic CSN Sync";
                break;
        }
        
        std::cout << "  Method: " << method_str << std::endl;
        std::cout << "  Path Delay: " << node_info.propagation_delay.count() << " ns" << std::endl;
        std::cout << "  Rate Ratio: " << node_info.rate_ratio << std::endl;
        std::cout << "  Recent Measurements: " << pair.second.recent_results.size() << std::endl;
        
        if (!pair.second.recent_results.empty()) {
            auto latest = pair.second.recent_results.back();
            std::cout << "  Latest Confidence: " << (latest.confidence * 100) << "%" << std::endl;
        }
    }
}

void PathDelayManager::cleanup_old_measurements() {
    auto now = std::chrono::steady_clock::now();
    auto cutoff = now - std::chrono::minutes(5);  // Keep 5 minutes of data
    
    for (auto& pair : node_calculators_) {
        auto& results = pair.second.recent_results;
        results.erase(
            std::remove_if(results.begin(), results.end(),
                [cutoff](const PathDelayResult& result) {
                    return result.measurement_time < cutoff;
                }),
            results.end()
        );
    }
}

// ============================================================================
// Utility Functions Implementation
// ============================================================================

namespace utils {

double calculate_neighbor_rate_ratio_equation_16_1(
    const std::vector<StandardP2PDelayCalculator::MeasurementData>& measurements, 
    size_t N) {
    
    if (measurements.size() < N + 1) {
        return 1.0;  // Default rate ratio
    }
    
    const auto& first = measurements[0];
    const auto& last = measurements[N];
    
    auto t_rsp3_diff = last.t_rsp3.to_nanoseconds() - first.t_rsp3.to_nanoseconds();
    auto t_req4_diff = last.t_req4.to_nanoseconds() - first.t_req4.to_nanoseconds();
    
    if (t_req4_diff.count() == 0) {
        return 1.0;
    }
    
    return static_cast<double>(t_rsp3_diff.count()) / static_cast<double>(t_req4_diff.count());
}

std::chrono::nanoseconds calculate_mean_link_delay_equation_16_2(
    const PdelayTimestamps& timestamps,
    double neighbor_rate_ratio) {
    
    auto t_req1 = timestamps.t1.to_nanoseconds();
    auto t_rsp2 = timestamps.t2.to_nanoseconds();
    auto t_rsp3 = timestamps.t3.to_nanoseconds();
    auto t_req4 = timestamps.t4.to_nanoseconds();
    
    // Equation 16-2: ((t_req4 - t_req1) * r - (t_rsp3 - t_rsp2)) / 2
    auto initiator_turnaround = t_req4 - t_req1;
    auto responder_residence = t_rsp3 - t_rsp2;
    
    auto corrected_turnaround = std::chrono::nanoseconds(
        static_cast<int64_t>(initiator_turnaround.count() * neighbor_rate_ratio)
    );
    
    auto mean_link_delay = (corrected_turnaround - responder_residence) / 2;
    
    return std::max(std::chrono::nanoseconds::zero(), mean_link_delay);
}

ValidationResult validate_path_delay_measurement(
    const PathDelayResult& result,
    std::chrono::nanoseconds max_expected_delay,
    double min_confidence) {
    
    ValidationResult validation;
    validation.valid = true;
    validation.confidence = result.confidence;
    
    if (!result.valid) {
        validation.valid = false;
        validation.error_message = "Measurement marked as invalid";
        return validation;
    }
    
    if (result.mean_link_delay > max_expected_delay) {
        validation.valid = false;
        validation.error_message = "Path delay exceeds maximum expected value";
        return validation;
    }
    
    if (result.mean_link_delay < std::chrono::nanoseconds::zero()) {
        validation.valid = false;
        validation.error_message = "Negative path delay";
        return validation;
    }
    
    if (result.confidence < min_confidence) {
        validation.valid = false;
        validation.error_message = "Measurement confidence too low";
        return validation;
    }
    
    if (std::abs(result.neighbor_rate_ratio - 1.0) > 0.0002) {  // 200 ppm
        validation.valid = false;
        validation.error_message = "Neighbor rate ratio outside IEEE 802.1AS limits";
        return validation;
    }
    
    validation.error_message = "Measurement valid";
    return validation;
}

PathDelayFilter::PathDelayFilter(size_t window_size)
    : window_size_(window_size)
    , current_index_(0)
    , window_full_(false)
{
    delay_window_.resize(window_size_);
}

std::chrono::nanoseconds PathDelayFilter::filter(std::chrono::nanoseconds new_delay) {
    delay_window_[current_index_] = new_delay;
    current_index_ = (current_index_ + 1) % window_size_;
    
    if (!window_full_ && current_index_ == 0) {
        window_full_ = true;
    }
    
    // Calculate median filter
    std::vector<std::chrono::nanoseconds> sorted_delays;
    
    if (window_full_) {
        sorted_delays = delay_window_;
    } else {
        sorted_delays.assign(delay_window_.begin(), delay_window_.begin() + current_index_);
    }
    
    std::sort(sorted_delays.begin(), sorted_delays.end());
    
    size_t middle = sorted_delays.size() / 2;
    if (sorted_delays.size() % 2 == 0) {
        return (sorted_delays[middle - 1] + sorted_delays[middle]) / 2;
    } else {
        return sorted_delays[middle];
    }
}

void PathDelayFilter::reset() {
    current_index_ = 0;
    window_full_ = false;
    std::fill(delay_window_.begin(), delay_window_.end(), std::chrono::nanoseconds::zero());
}

} // namespace utils

// ============================================================================
// PathDelayFactory Implementation
// ============================================================================

std::unique_ptr<IPathDelayCalculator> PathDelayFactory::create_standard_p2p_calculator(uint8_t domain_number) {
    return std::make_unique<StandardP2PDelayCalculator>(domain_number);
}

std::unique_ptr<IPathDelayCalculator> PathDelayFactory::create_native_csn_calculator(
    NativeCSNDelayCalculator::NativeDelayProvider provider) {
    return std::make_unique<NativeCSNDelayCalculator>(provider);
}

std::unique_ptr<IPathDelayCalculator> PathDelayFactory::create_intrinsic_csn_calculator() {
    return std::make_unique<IntrinsicCSNDelayCalculator>();
}

std::unique_ptr<IPathDelayCalculator> PathDelayFactory::create_automotive_calculator() {
    auto calculator = std::make_unique<StandardP2PDelayCalculator>(0);
    
    // Automotive-specific configuration
    calculator->set_measurement_interval(std::chrono::seconds(1));
    calculator->set_rate_ratio_calculation_window(8);
    calculator->set_validation_thresholds(
        std::chrono::microseconds(500),  // 500µs max path delay for automotive
        0.0001  // 100 ppm max rate ratio change
    );
    
    return std::move(calculator);
}

std::unique_ptr<IPathDelayCalculator> PathDelayFactory::create_industrial_calculator() {
    auto calculator = std::make_unique<StandardP2PDelayCalculator>(0);
    
    // Industrial-specific configuration
    calculator->set_measurement_interval(std::chrono::seconds(1));
    calculator->set_rate_ratio_calculation_window(16);
    calculator->set_validation_thresholds(
        std::chrono::milliseconds(10),  // 10ms max path delay for industrial
        0.0002  // 200 ppm max rate ratio change
    );
    
    return std::move(calculator);
}

std::unique_ptr<IPathDelayCalculator> PathDelayFactory::create_high_precision_calculator() {
    auto calculator = std::make_unique<StandardP2PDelayCalculator>(0);
    
    // High precision configuration
    calculator->set_measurement_interval(std::chrono::milliseconds(125));  // More frequent measurements
    calculator->set_rate_ratio_calculation_window(32);  // Larger window for better accuracy
    calculator->set_validation_thresholds(
        std::chrono::microseconds(100),  // 100µs max path delay
        0.00005  // 50 ppm max rate ratio change
    );
    
    return std::move(calculator);
}

} // namespace path_delay
} // namespace gptp
