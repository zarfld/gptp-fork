/**
 * @file path_delay_calculator.hpp
 * @brief IEEE 802.1AS-2021 Path Delay Calculation System
 * 
 * Implementation of Chapter 16.4.3 - Path delay measurement between CSN nodes
 * Supports multiple path delay measurement methods:
 * - Standard peer-to-peer delay mechanism (16.4.3.2)
 * - Native CSN path delay measurement (16.4.3.3)
 * - Intrinsic CSN path delay measurement (16.4.3.4)
 */

#ifndef GPTP_PATH_DELAY_CALCULATOR_HPP
#define GPTP_PATH_DELAY_CALCULATOR_HPP

#include "gptp_protocol.hpp"
#include <chrono>
#include <vector>
#include <memory>
#include <functional>
#include <string>
#include <map>
#include <mutex>

namespace gptp {
namespace path_delay {

/**
 * @brief Path delay measurement result
 */
struct PathDelayResult {
    std::chrono::nanoseconds mean_link_delay;
    double neighbor_rate_ratio;
    bool valid;
    std::chrono::steady_clock::time_point measurement_time;
    double confidence;  // 0.0 to 1.0
    
    PathDelayResult() 
        : mean_link_delay(0)
        , neighbor_rate_ratio(1.0)
        , valid(false)
        , confidence(0.0) {}
};

/**
 * @brief Pdelay measurement timestamps
 * IEEE 802.1AS-2021 Figure 11-13
 */
struct PdelayTimestamps {
    Timestamp t1;  // Pdelay_Req transmission time (initiator)
    Timestamp t2;  // Pdelay_Req reception time (responder)
    Timestamp t3;  // Pdelay_Resp transmission time (responder)
    Timestamp t4;  // Pdelay_Resp reception time (initiator)
    
    uint16_t sequence_id;
    bool t2_valid;
    bool t3_valid;
    
    PdelayTimestamps() : sequence_id(0), t2_valid(false), t3_valid(false) {}
};

/**
 * @brief CSN (Clock Synchronization Network) node information
 */
struct CSNNodeInfo {
    ClockIdentity node_identity;
    std::chrono::nanoseconds propagation_delay;
    double rate_ratio;
    bool has_native_delay_measurement;
    bool has_intrinsic_synchronization;
    std::chrono::steady_clock::time_point last_update;
    
    CSNNodeInfo() 
        : propagation_delay(0)
        , rate_ratio(1.0)
        , has_native_delay_measurement(false)
        , has_intrinsic_synchronization(false) {}
};

/**
 * @brief Path delay measurement method
 */
enum class DelayMeasurementMethod {
    PEER_TO_PEER_PROTOCOL,    // Standard IEEE 802.1AS pdelay mechanism
    NATIVE_CSN_MEASUREMENT,   // CSN-specific native method
    INTRINSIC_CSN_SYNC        // CSN with intrinsic synchronization
};

/**
 * @brief Path Delay Calculator Interface
 */
class IPathDelayCalculator {
public:
    virtual ~IPathDelayCalculator() = default;
    
    virtual PathDelayResult calculate_path_delay(const PdelayTimestamps& timestamps) = 0;
    virtual void update_neighbor_rate_ratio(const std::vector<PdelayTimestamps>& measurements) = 0;
    virtual bool is_measurement_valid(const PdelayTimestamps& timestamps) const = 0;
    virtual DelayMeasurementMethod get_method() const = 0;
};

/**
 * @brief Standard Peer-to-Peer Path Delay Calculator
 * IEEE 802.1AS-2021 Chapter 16.4.3.2
 */
class StandardP2PDelayCalculator : public IPathDelayCalculator {
public:
    explicit StandardP2PDelayCalculator(uint8_t domain_number = 0);
    
    PathDelayResult calculate_path_delay(const PdelayTimestamps& timestamps) override;
    void update_neighbor_rate_ratio(const std::vector<PdelayTimestamps>& measurements) override;
    bool is_measurement_valid(const PdelayTimestamps& timestamps) const override;
    DelayMeasurementMethod get_method() const override { return DelayMeasurementMethod::PEER_TO_PEER_PROTOCOL; }
    
    // Configuration
    void set_measurement_interval(std::chrono::nanoseconds interval);
    void set_rate_ratio_calculation_window(size_t N);
    void set_validation_thresholds(std::chrono::nanoseconds max_delay, double max_rate_ratio_change);
    
    // Statistics
    size_t get_measurement_count() const { return measurement_history_.size(); }
    double get_current_neighbor_rate_ratio() const { return current_neighbor_rate_ratio_; }
    std::chrono::nanoseconds get_last_mean_link_delay() const { return last_mean_link_delay_; }

    // Measurement data structure for external access
    struct MeasurementData {
        Timestamp t_rsp3;  // Responder transmission time (from follow-up)
        Timestamp t_req4;  // Initiator reception time
        std::chrono::steady_clock::time_point measurement_time;
        uint16_t sequence_id;
    };

private:
    double calculate_neighbor_rate_ratio(const std::vector<MeasurementData>& measurements, size_t N);
    std::chrono::nanoseconds calculate_mean_link_delay(const PdelayTimestamps& timestamps, double rate_ratio);
    bool validate_timestamps(const PdelayTimestamps& timestamps) const;
    
    uint8_t domain_number_;
    std::chrono::nanoseconds measurement_interval_;
    size_t rate_ratio_window_size_;
    
    // Validation thresholds
    std::chrono::nanoseconds max_path_delay_;
    double max_rate_ratio_change_;
    
    // State
    double current_neighbor_rate_ratio_;
    std::chrono::nanoseconds last_mean_link_delay_;
    std::vector<MeasurementData> measurement_history_;
    std::vector<PdelayTimestamps> timestamp_history_;
};

/**
 * @brief Native CSN Path Delay Calculator
 * IEEE 802.1AS-2021 Chapter 16.4.3.3
 */
class NativeCSNDelayCalculator : public IPathDelayCalculator {
public:
    using NativeDelayProvider = std::function<PathDelayResult()>;
    
    explicit NativeCSNDelayCalculator(NativeDelayProvider provider);
    
    PathDelayResult calculate_path_delay(const PdelayTimestamps& timestamps) override;
    void update_neighbor_rate_ratio(const std::vector<PdelayTimestamps>& measurements) override;
    bool is_measurement_valid(const PdelayTimestamps& timestamps) const override;
    DelayMeasurementMethod get_method() const override { return DelayMeasurementMethod::NATIVE_CSN_MEASUREMENT; }
    
    // CSN MD entity variables (IEEE 802.1AS-2021 16.4.3.3)
    void set_as_capable(bool capable) { as_capable_ = capable; }
    void set_neighbor_rate_ratio(double ratio) { neighbor_rate_ratio_ = ratio; }
    void set_mean_link_delay(std::chrono::nanoseconds delay) { mean_link_delay_ = delay; }
    void set_compute_flags(bool compute_rate_ratio, bool compute_mean_delay);
    void set_measuring_delay(bool measuring) { is_measuring_delay_ = measuring; }
    
    bool get_as_capable() const { return as_capable_; }
    double get_neighbor_rate_ratio() const { return neighbor_rate_ratio_; }
    std::chrono::nanoseconds get_mean_link_delay() const { return mean_link_delay_; }
    bool is_measuring_delay() const { return is_measuring_delay_; }

private:
    NativeDelayProvider native_provider_;
    
    // IEEE 802.1AS-2021 MD entity variables
    bool as_capable_;
    double neighbor_rate_ratio_;
    std::chrono::nanoseconds mean_link_delay_;
    bool compute_neighbor_rate_ratio_;
    bool compute_mean_link_delay_;
    bool is_measuring_delay_;
};

/**
 * @brief Intrinsic CSN Path Delay Calculator
 * IEEE 802.1AS-2021 Chapter 16.4.3.4
 */
class IntrinsicCSNDelayCalculator : public IPathDelayCalculator {
public:
    explicit IntrinsicCSNDelayCalculator();
    
    PathDelayResult calculate_path_delay(const PdelayTimestamps& timestamps) override;
    void update_neighbor_rate_ratio(const std::vector<PdelayTimestamps>& measurements) override;
    bool is_measurement_valid(const PdelayTimestamps& timestamps) const override;
    DelayMeasurementMethod get_method() const override { return DelayMeasurementMethod::INTRINSIC_CSN_SYNC; }
    
    // CSN with intrinsic synchronization treats path delay as residence time
    void set_residence_time(std::chrono::nanoseconds residence_time);
    std::chrono::nanoseconds get_residence_time() const { return residence_time_; }

private:
    std::chrono::nanoseconds residence_time_;
    bool synchronized_csn_time_complies_b1_;  // Complies with requirements in B.1
};

/**
 * @brief Path Delay Manager
 * Manages multiple CSN nodes and their path delay calculations
 */
class PathDelayManager {
public:
    PathDelayManager();
    
    void add_csn_node(const ClockIdentity& node_id, std::unique_ptr<IPathDelayCalculator> calculator);
    void remove_csn_node(const ClockIdentity& node_id);
    
    PathDelayResult calculate_path_delay_to_node(const ClockIdentity& node_id, const PdelayTimestamps& timestamps);
    void update_neighbor_rate_ratios();
    
    // Node management
    std::vector<ClockIdentity> get_active_nodes() const;
    DelayMeasurementMethod get_node_measurement_method(const ClockIdentity& node_id) const;
    
    // Statistics and monitoring
    void print_path_delay_statistics() const;
    void export_measurement_data(const std::string& filename) const;

private:
    struct NodeCalculator {
        std::unique_ptr<IPathDelayCalculator> calculator;
        CSNNodeInfo node_info;
        std::vector<PathDelayResult> recent_results;
    };
    
    std::map<ClockIdentity, NodeCalculator> node_calculators_;
    mutable std::mutex calculators_mutex_;
    
    void cleanup_old_measurements();
};

/**
 * @brief Path Delay Utilities
 */
namespace utils {

/**
 * @brief Calculate neighbor rate ratio using Equation 16-1
 * @param measurements Vector of measurement data indexed from 0 to N
 * @param N Number of Pdelay_Req message transmission intervals
 * @return Calculated neighbor rate ratio
 */
double calculate_neighbor_rate_ratio_equation_16_1(
    const std::vector<StandardP2PDelayCalculator::MeasurementData>& measurements, 
    size_t N
);

/**
 * @brief Calculate mean link delay using Equation 16-2
 * @param timestamps Current pdelay measurement timestamps
 * @param neighbor_rate_ratio Current neighbor rate ratio
 * @return Calculated mean link delay
 */
std::chrono::nanoseconds calculate_mean_link_delay_equation_16_2(
    const PdelayTimestamps& timestamps,
    double neighbor_rate_ratio
);

/**
 * @brief Validate path delay measurement against IEEE 802.1AS requirements
 */
struct ValidationResult {
    bool valid;
    std::string error_message;
    double confidence;
};

ValidationResult validate_path_delay_measurement(
    const PathDelayResult& result,
    std::chrono::nanoseconds max_expected_delay,
    double min_confidence = 0.7
);

/**
 * @brief Path delay filtering for noise reduction
 */
class PathDelayFilter {
public:
    explicit PathDelayFilter(size_t window_size = 8);
    
    std::chrono::nanoseconds filter(std::chrono::nanoseconds new_delay);
    void reset();
    
private:
    size_t window_size_;
    std::vector<std::chrono::nanoseconds> delay_window_;
    size_t current_index_;
    bool window_full_;
};

} // namespace utils

/**
 * @brief Path Delay Factory
 * Creates appropriate path delay calculators for different scenarios
 */
class PathDelayFactory {
public:
    static std::unique_ptr<IPathDelayCalculator> create_standard_p2p_calculator(uint8_t domain_number = 0);
    
    static std::unique_ptr<IPathDelayCalculator> create_native_csn_calculator(
        NativeCSNDelayCalculator::NativeDelayProvider provider
    );
    
    static std::unique_ptr<IPathDelayCalculator> create_intrinsic_csn_calculator();
    
    // Common configuration presets
    static std::unique_ptr<IPathDelayCalculator> create_automotive_calculator();
    static std::unique_ptr<IPathDelayCalculator> create_industrial_calculator();
    static std::unique_ptr<IPathDelayCalculator> create_high_precision_calculator();
};

} // namespace path_delay
} // namespace gptp

#endif // GPTP_PATH_DELAY_CALCULATOR_HPP
