/**
 * @file simple_path_delay.hpp
 * @brief Simplified IEEE 802.1AS-2021 Path Delay Calculation
 * 
 * Focused implementation of Chapter 16.4.3 requirements:
 * - Equation 16-1: neighborRateRatio calculation
 * - Equation 16-2: meanLinkDelay calculation
 * - CSN path delay measurement variants
 */

#ifndef GPTP_SIMPLE_PATH_DELAY_HPP
#define GPTP_SIMPLE_PATH_DELAY_HPP

#include "gptp_protocol.hpp"
#include <chrono>
#include <vector>

namespace gptp {
namespace path_delay {

/**
 * @brief Simple Path Delay Result
 */
struct SimplePathDelayResult {
    std::chrono::nanoseconds mean_link_delay;
    double neighbor_rate_ratio;
    bool valid;
    
    SimplePathDelayResult() 
        : mean_link_delay(0), neighbor_rate_ratio(1.0), valid(false) {}
};

/**
 * @brief Core IEEE 802.1AS Path Delay Calculator
 * Implements the essential equations from Chapter 16.4.3
 */
class PathDelayCalculator {
public:
    PathDelayCalculator();

    /**
     * @brief Calculate neighborRateRatio using IEEE 802.1AS-2021 Equation 16-1
     * 
     * Formula: (t_rsp3_N - t_rsp3_0) / (t_req4_N - t_req4_0)
     * 
     * @param t_rsp3_measurements Vector of responder transmission times (from follow-up messages)
     * @param t_req4_measurements Vector of initiator reception times  
     * @param N Number of Pdelay_Req message transmission intervals
     * @return Calculated neighbor rate ratio
     */
    double calculate_neighbor_rate_ratio_eq16_1(
        const std::vector<Timestamp>& t_rsp3_measurements,
        const std::vector<Timestamp>& t_req4_measurements,
        size_t N
    );

    /**
     * @brief Calculate meanLinkDelay using IEEE 802.1AS-2021 Equation 16-2
     * 
     * Formula: ((t_req4 - t_req1) * r - (t_rsp3 - t_rsp2)) / 2
     * 
     * @param t_req1 Pdelay_Req transmission time (initiator)
     * @param t_rsp2 Pdelay_Req reception time (responder)
     * @param t_rsp3 Pdelay_Resp transmission time (responder)
     * @param t_req4 Pdelay_Resp reception time (initiator)
     * @param r neighborRateRatio
     * @return Calculated mean link delay
     */
    std::chrono::nanoseconds calculate_mean_link_delay_eq16_2(
        const Timestamp& t_req1,
        const Timestamp& t_rsp2,  
        const Timestamp& t_rsp3,
        const Timestamp& t_req4,
        double r
    );

    /**
     * @brief Complete path delay calculation for standard P2P mechanism
     * IEEE 802.1AS-2021 Section 16.4.3.2
     */
    SimplePathDelayResult calculate_p2p_path_delay(
        const Timestamp& t1,  // Pdelay_Req TX (initiator)
        const Timestamp& t2,  // Pdelay_Req RX (responder)
        const Timestamp& t3,  // Pdelay_Resp TX (responder)
        const Timestamp& t4   // Pdelay_Resp RX (initiator)
    );

    /**
     * @brief Update rate ratio using sliding window of measurements
     */
    void update_rate_ratio(const Timestamp& t_rsp3, const Timestamp& t_req4);

    /**
     * @brief Get current neighbor rate ratio
     */
    double get_neighbor_rate_ratio() const { return current_rate_ratio_; }

    /**
     * @brief Get last calculated mean link delay
     */
    std::chrono::nanoseconds get_mean_link_delay() const { return last_mean_delay_; }

    /**
     * @brief Configure measurement window size for rate ratio calculation
     */
    void set_rate_ratio_window_size(size_t N) { rate_ratio_window_size_ = N; }

private:
    struct RatioMeasurement {
        Timestamp t_rsp3;
        Timestamp t_req4;
        std::chrono::steady_clock::time_point time;
    };

    double current_rate_ratio_;
    std::chrono::nanoseconds last_mean_delay_;
    size_t rate_ratio_window_size_;
    std::vector<RatioMeasurement> ratio_measurements_;
    
    bool validate_timestamps(const Timestamp& t1, const Timestamp& t2, 
                           const Timestamp& t3, const Timestamp& t4) const;
};

/**
 * @brief CSN MD Entity Variables
 * IEEE 802.1AS-2021 Section 16.4.3.3
 */
struct CSNMDEntity {
    bool asCapable;                    // 10.2.5.1
    double neighborRateRatio;          // 10.2.5.7  
    std::chrono::nanoseconds meanLinkDelay; // 10.2.5.8
    bool computeNeighborRateRatio;     // 10.2.5.10
    bool computeMeanLinkDelay;         // 10.2.5.11
    bool isMeasuringDelay;            // 11.2.13.6
    uint8_t domainNumber;             // 8.1
    
    CSNMDEntity() 
        : asCapable(false)
        , neighborRateRatio(1.0)
        , meanLinkDelay(0)
        , computeNeighborRateRatio(true)
        , computeMeanLinkDelay(true)
        , isMeasuringDelay(false)
        , domainNumber(0) {}
};

/**
 * @brief Native CSN Path Delay Handler
 * IEEE 802.1AS-2021 Section 16.4.3.3
 */
class NativeCSNPathDelay {
public:
    explicit NativeCSNPathDelay(CSNMDEntity& md_entity);

    /**
     * @brief Set path delay from native CSN measurement
     */
    void set_native_path_delay(std::chrono::nanoseconds delay, double rate_ratio);

    /**
     * @brief Configure CSN MD entity for native measurement
     */
    void configure_for_native_measurement();

    /**
     * @brief Get current path delay result
     */
    SimplePathDelayResult get_path_delay_result() const;

private:
    CSNMDEntity& md_entity_;
};

/**
 * @brief Intrinsic CSN Path Delay Handler  
 * IEEE 802.1AS-2021 Section 16.4.3.4
 */
class IntrinsicCSNPathDelay {
public:
    IntrinsicCSNPathDelay();

    /**
     * @brief Set residence time for intrinsic CSN
     * Path delay is treated as part of residence time
     */
    void set_residence_time(std::chrono::nanoseconds residence_time);

    /**
     * @brief Indicate CSN time compliance with B.1 requirements
     */
    void set_b1_compliance(bool compliant) { b1_compliant_ = compliant; }

    /**
     * @brief Get path delay (always zero for intrinsic CSN)
     */
    SimplePathDelayResult get_path_delay_result() const;

private:
    std::chrono::nanoseconds residence_time_;
    bool b1_compliant_;
};

/**
 * @brief Utility Functions for IEEE 802.1AS Equations
 */
namespace equations {

/**
 * @brief Direct implementation of Equation 16-1
 */
inline double neighbor_rate_ratio_eq16_1(
    const Timestamp& t_rsp3_N, const Timestamp& t_rsp3_0,
    const Timestamp& t_req4_N, const Timestamp& t_req4_0) {
    
    auto numerator = t_rsp3_N.to_nanoseconds() - t_rsp3_0.to_nanoseconds();
    auto denominator = t_req4_N.to_nanoseconds() - t_req4_0.to_nanoseconds();
    
    if (denominator.count() == 0) return 1.0;
    
    return static_cast<double>(numerator.count()) / static_cast<double>(denominator.count());
}

/**
 * @brief Direct implementation of Equation 16-2
 */
inline std::chrono::nanoseconds mean_link_delay_eq16_2(
    const Timestamp& t_req1, const Timestamp& t_rsp2,
    const Timestamp& t_rsp3, const Timestamp& t_req4,
    double r) {
    
    auto initiator_turnaround = t_req4.to_nanoseconds() - t_req1.to_nanoseconds();
    auto responder_residence = t_rsp3.to_nanoseconds() - t_rsp2.to_nanoseconds();
    
    // Apply rate ratio correction
    auto corrected_turnaround = std::chrono::nanoseconds(
        static_cast<int64_t>(initiator_turnaround.count() * r)
    );
    
    auto mean_delay = (corrected_turnaround - responder_residence) / 2;
    return std::max(std::chrono::nanoseconds::zero(), mean_delay);
}

/**
 * @brief Validate 200 ppm rate ratio constraint
 * IEEE 802.1AS Note in Section 16.4.3.2
 */
inline bool validate_rate_ratio(double rate_ratio) {
    return (rate_ratio >= 0.9998) && (rate_ratio <= 1.0002);
}

/**
 * @brief Calculate frequency offset from rate ratio  
 * 20 ps difference example from IEEE 802.1AS Note
 */
inline double frequency_offset_ppm(double rate_ratio) {
    return (rate_ratio - 1.0) * 1000000.0;  // Convert to ppm
}

} // namespace equations

} // namespace path_delay
} // namespace gptp

#endif // GPTP_SIMPLE_PATH_DELAY_HPP
