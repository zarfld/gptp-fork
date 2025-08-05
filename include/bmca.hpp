/**
 * @file bmca.hpp
 * @brief Best Master Clock Algorithm (BMCA) implementation
 * 
 * Implements IEEE 802.1AS-2021 clause 10.3 BMCA logic for automatic 
 * grandmaster selection and port role determination.
 */

#pragma once

#include "gptp_protocol.hpp"
#include <chrono>
#include <vector>
#include <map>
#include <memory>

namespace gptp {
namespace bmca {

/**
 * @brief Priority Vector for BMCA comparison (IEEE 802.1AS-2021 clause 10.3.4)
 */
struct PriorityVector {
    ClockIdentity grandmaster_identity;
    uint8_t grandmaster_priority1;
    ClockQuality grandmaster_clock_quality;
    uint8_t grandmaster_priority2;
    ClockIdentity sender_identity;
    uint16_t steps_removed;
    
    PriorityVector() = default;
    
    // Construct from announce message
    explicit PriorityVector(const AnnounceMessage& announce);
    
    // Construct for local clock
    PriorityVector(const ClockIdentity& clock_id, 
                   uint8_t priority1, 
                   const ClockQuality& quality,
                   uint8_t priority2);
};

/**
 * @brief BMCA comparison result
 */
enum class BmcaResult : uint8_t {
    A_BETTER_THAN_B = 0,    // A < B (A is better master)
    B_BETTER_THAN_A = 1,    // A > B (B is better master)  
    A_BETTER_BY_TOPOLOGY = 2, // A better due to topology
    B_BETTER_BY_TOPOLOGY = 3, // B better due to topology
    SAME_MASTER = 4,        // Same grandmaster, different path
    ERROR_1 = 5,           // Comparison error
    ERROR_2 = 6            // Comparison error
};

/**
 * @brief Port role recommendation from BMCA
 */
enum class PortRole : uint8_t {
    MASTER = 0,
    SLAVE = 1,
    PASSIVE = 2,
    DISABLED = 3
};

/**
 * @brief Master information for a port
 */
struct MasterInfo {
    PriorityVector priority_vector;
    std::chrono::steady_clock::time_point last_announce_time;
    std::chrono::nanoseconds announce_interval;
    bool valid;
    
    MasterInfo() : valid(false) {}
    
    bool is_announce_timeout(std::chrono::steady_clock::time_point now) const {
        return valid && (now - last_announce_time) > (announce_interval * 3);
    }
};

/**
 * @brief BMCA Engine - implements IEEE 802.1AS BMCA logic
 */
class BmcaEngine {
public:
    BmcaEngine() = default;
    ~BmcaEngine() = default;
    
    /**
     * @brief Compare two priority vectors (IEEE 802.1AS-2021 clause 10.3.4)
     * @param a First priority vector
     * @param b Second priority vector
     * @return Comparison result
     */
    static BmcaResult compare_priority_vectors(const PriorityVector& a, const PriorityVector& b);
    
    /**
     * @brief Compare clock qualities (IEEE 802.1AS-2021 clause 7.6.2.4)
     * @param a First clock quality
     * @param b Second clock quality  
     * @return -1 if a < b, 0 if a == b, 1 if a > b
     */
    static int compare_clock_quality(const ClockQuality& a, const ClockQuality& b);
    
    /**
     * @brief Update master information from announce message
     * @param announce Received announce message
     * @param receipt_time When the announce was received
     * @return Updated master information
     */
    MasterInfo update_master_info(const AnnounceMessage& announce, 
                                 std::chrono::steady_clock::time_point receipt_time);
    
    /**
     * @brief Determine port role based on BMCA decision
     * @param local_clock Local clock priority vector
     * @param master_info Current master information (if any)
     * @return Recommended port role
     */
    PortRole determine_port_role(const PriorityVector& local_clock,
                                const MasterInfo* master_info);
    
    /**
     * @brief Check if we should be grandmaster
     * @param local_clock Local clock priority vector
     * @param all_masters Master info from all ports
     * @return True if local clock should be grandmaster
     */
    bool should_be_grandmaster(const PriorityVector& local_clock,
                              const std::vector<MasterInfo>& all_masters);
    
    /**
     * @brief Select best master from available options
     * @param candidates All available master candidates
     * @return Best master, or nullptr if none suitable
     */
    MasterInfo* select_best_master(const std::vector<MasterInfo>& candidates);

private:
    /**
     * @brief Compare clock identities (IEEE EUI-64)
     * @param a First clock identity
     * @param b Second clock identity
     * @return -1 if a < b, 0 if a == b, 1 if a > b
     */
    static int compare_clock_identity(const ClockIdentity& a, const ClockIdentity& b);
    
    /**
     * @brief Check if steps removed is within acceptable range
     * @param steps Number of steps removed
     * @return True if acceptable
     */
    static bool is_steps_removed_acceptable(uint16_t steps);
    
    /**
     * @brief Validate priority vector for consistency
     * @param pv Priority vector to validate
     * @return True if valid
     */
    static bool is_priority_vector_valid(const PriorityVector& pv);
};

/**
 * @brief BMCA Decision Information
 */
struct BmcaDecision {
    PortRole recommended_role;
    MasterInfo* selected_master;
    bool role_changed;
    std::chrono::steady_clock::time_point decision_time;
    
    BmcaDecision() : recommended_role(PortRole::DISABLED), selected_master(nullptr), role_changed(false) {}
};

/**
 * @brief Multi-port BMCA Coordinator
 * 
 * Manages BMCA decisions across multiple ports and ensures
 * consistent master selection and role assignment.
 */
class BmcaCoordinator {
public:
    explicit BmcaCoordinator(const ClockIdentity& local_clock_id);
    ~BmcaCoordinator() = default;
    
    /**
     * @brief Process announce message from a port
     * @param port_id Port identifier
     * @param announce Received announce message
     * @param receipt_time When message was received
     */
    void process_announce(uint16_t port_id, 
                         const AnnounceMessage& announce,
                         std::chrono::steady_clock::time_point receipt_time);
    
    /**
     * @brief Run BMCA decision process
     * @param local_priority Local clock priority vector
     * @return BMCA decisions for all ports
     */
    std::vector<BmcaDecision> run_bmca(const PriorityVector& local_priority);
    
    /**
     * @brief Update local clock properties
     * @param priority1 Priority 1 value
     * @param quality Clock quality
     * @param priority2 Priority 2 value
     */
    void update_local_clock(uint8_t priority1, 
                           const ClockQuality& quality,
                           uint8_t priority2);
    
    /**
     * @brief Check for announce timeouts
     * @param current_time Current time
     * @return List of ports with timed out masters
     */
    std::vector<uint16_t> check_announce_timeouts(std::chrono::steady_clock::time_point current_time);
    
    /**
     * @brief Get current master information for a port
     * @param port_id Port identifier
     * @return Master info if available, nullptr otherwise
     */
    const MasterInfo* get_master_info(uint16_t port_id) const;
    
    /**
     * @brief Get current grandmaster information
     * @return Grandmaster priority vector if available, nullptr otherwise
     */
    const PriorityVector* get_grandmaster() const;
    
    /**
     * @brief Check if local clock is currently grandmaster
     * @return True if we are grandmaster
     */
    bool is_local_grandmaster() const;

private:
    ClockIdentity local_clock_id_;
    BmcaEngine engine_;
    
    // Master information per port
    std::map<uint16_t, MasterInfo> port_masters_;
    
    // Current grandmaster selection
    PriorityVector* current_grandmaster_;
    bool local_is_grandmaster_;
    
    // Local clock properties
    uint8_t local_priority1_;
    ClockQuality local_clock_quality_;
    uint8_t local_priority2_;
    
    std::chrono::steady_clock::time_point last_bmca_run_;
};

} // namespace bmca
} // namespace gptp
