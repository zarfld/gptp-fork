/**
 * @file bmca.cpp
 * @brief Best Master Clock Algorithm (BMCA) implementation
 * 
 * Implements IEEE 802.1AS-2021 clause 10.3 BMCA logic for automatic 
 * grandmaster selection and port role determination.
 */

#include "../../include/bmca.hpp"
#include <cstring>
#include <cmath>
#include <iostream>  // für std::cout Debug-Ausgaben

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

namespace gptp {
namespace bmca {

// ============================================================================
// PriorityVector Implementation
// ============================================================================

PriorityVector::PriorityVector(const AnnounceMessage& announce) {
    grandmaster_identity = announce.grandmasterIdentity;
    grandmaster_priority1 = announce.grandmasterPriority1;
    
    // Unpack clock quality from packed uint32_t
    grandmaster_clock_quality.clockClass = (announce.grandmasterClockQuality >> 24) & 0xFF;
    grandmaster_clock_quality.clockAccuracy = static_cast<protocol::ClockAccuracy>((announce.grandmasterClockQuality >> 16) & 0xFF);
    grandmaster_clock_quality.offsetScaledLogVariance = announce.grandmasterClockQuality & 0xFFFF;
    
    grandmaster_priority2 = announce.grandmasterPriority2;
    sender_identity = announce.header.sourcePortIdentity.clockIdentity;
    steps_removed = ntohs(announce.stepsRemoved);
}

PriorityVector::PriorityVector(const ClockIdentity& clock_id, 
                               uint8_t priority1, 
                               const ClockQuality& quality,
                               uint8_t priority2) 
    : grandmaster_identity(clock_id)
    , grandmaster_priority1(priority1)
    , grandmaster_clock_quality(quality)
    , grandmaster_priority2(priority2)
    , sender_identity(clock_id)
    , steps_removed(0)
{
}

// ============================================================================
// BmcaEngine Implementation
// ============================================================================

BmcaResult BmcaEngine::compare_priority_vectors(const PriorityVector& a, const PriorityVector& b) {
    // IEEE 802.1AS-2021 clause 10.3.4 - Priority vector comparison
    
    // Step 1: Compare grandmaster priority1
    if (a.grandmaster_priority1 < b.grandmaster_priority1) {
        return BmcaResult::A_BETTER_THAN_B;
    }
    if (a.grandmaster_priority1 > b.grandmaster_priority1) {
        return BmcaResult::B_BETTER_THAN_A;
    }
    
    // Step 2: Compare grandmaster clock quality
    int quality_cmp = compare_clock_quality(a.grandmaster_clock_quality, b.grandmaster_clock_quality);
    if (quality_cmp < 0) {
        return BmcaResult::A_BETTER_THAN_B;
    }
    if (quality_cmp > 0) {
        return BmcaResult::B_BETTER_THAN_A;
    }
    
    // Step 3: Compare grandmaster priority2
    if (a.grandmaster_priority2 < b.grandmaster_priority2) {
        return BmcaResult::A_BETTER_THAN_B;
    }
    if (a.grandmaster_priority2 > b.grandmaster_priority2) {
        return BmcaResult::B_BETTER_THAN_A;
    }
    
    // Step 4: Compare grandmaster identity
    int gm_id_cmp = compare_clock_identity(a.grandmaster_identity, b.grandmaster_identity);
    if (gm_id_cmp < 0) {
        return BmcaResult::A_BETTER_THAN_B;
    }
    if (gm_id_cmp > 0) {
        return BmcaResult::B_BETTER_THAN_A;
    }
    
    // Same grandmaster - compare topology
    
    // Step 5: Compare steps removed
    if (a.steps_removed < b.steps_removed) {
        return BmcaResult::A_BETTER_BY_TOPOLOGY;
    }
    if (a.steps_removed > b.steps_removed) {
        return BmcaResult::B_BETTER_BY_TOPOLOGY;
    }
    
    // Step 6: Compare sender identity
    int sender_id_cmp = compare_clock_identity(a.sender_identity, b.sender_identity);
    if (sender_id_cmp < 0) {
        return BmcaResult::A_BETTER_BY_TOPOLOGY;
    }
    if (sender_id_cmp > 0) {
        return BmcaResult::B_BETTER_BY_TOPOLOGY;
    }
    
    // Identical priority vectors
    return BmcaResult::SAME_MASTER;
}

int BmcaEngine::compare_clock_quality(const ClockQuality& a, const ClockQuality& b) {
    // IEEE 802.1AS-2021 clause 7.6.2.4 - Clock quality comparison
    
    // Step 1: Compare clock class
    if (a.clockClass < b.clockClass) {
        return -1;
    }
    if (a.clockClass > b.clockClass) {
        return 1;
    }
    
    // Step 2: Compare clock accuracy
    if (static_cast<uint8_t>(a.clockAccuracy) < static_cast<uint8_t>(b.clockAccuracy)) {
        return -1;
    }
    if (static_cast<uint8_t>(a.clockAccuracy) > static_cast<uint8_t>(b.clockAccuracy)) {
        return 1;
    }
    
    // Step 3: Compare offset scaled log variance
    if (a.offsetScaledLogVariance < b.offsetScaledLogVariance) {
        return -1;
    }
    if (a.offsetScaledLogVariance > b.offsetScaledLogVariance) {
        return 1;
    }
    
    return 0; // Equal
}

int BmcaEngine::compare_clock_identity(const ClockIdentity& a, const ClockIdentity& b) {
    // IEEE EUI-64 comparison - lexicographic byte order
    return std::memcmp(a.id.data(), b.id.data(), sizeof(ClockIdentity::id));
}

MasterInfo BmcaEngine::update_master_info(const AnnounceMessage& announce, 
                                         std::chrono::steady_clock::time_point receipt_time) {
    MasterInfo info;
    info.priority_vector = PriorityVector(announce);
    info.last_announce_time = receipt_time;
    
    // Extract announce interval from message
    int8_t log_interval = announce.header.logMessageInterval;
    auto interval_ms = static_cast<uint32_t>(1000.0 * pow(2.0, log_interval));
    info.announce_interval = std::chrono::milliseconds(interval_ms);
    
    info.valid = is_priority_vector_valid(info.priority_vector);
    
    return info;
}

PortRole BmcaEngine::determine_port_role(const PriorityVector& local_clock,
                                        const MasterInfo* master_info) {
    if (!master_info || !master_info->valid) {
        // No valid master information - become master
        return PortRole::MASTER;
    }
    
    // Check if master has timed out
    auto now = std::chrono::steady_clock::now();
    if (master_info->is_announce_timeout(now)) {
        return PortRole::MASTER;
    }
    
    // Compare with received master
    BmcaResult result = compare_priority_vectors(local_clock, master_info->priority_vector);
    
    switch (result) {
        case BmcaResult::A_BETTER_THAN_B:
        case BmcaResult::A_BETTER_BY_TOPOLOGY:
            return PortRole::MASTER;
            
        case BmcaResult::B_BETTER_THAN_A:
        case BmcaResult::B_BETTER_BY_TOPOLOGY:
            return PortRole::SLAVE;
            
        case BmcaResult::SAME_MASTER:
            // Same master through different path - passive
            return PortRole::PASSIVE;
            
        default:
            return PortRole::DISABLED;
    }
}

bool BmcaEngine::should_be_grandmaster(const PriorityVector& local_clock,
                                      const std::vector<MasterInfo>& all_masters) {
    // Check if local clock is better than all known masters
    for (const auto& master : all_masters) {
        if (!master.valid) continue;
        
        BmcaResult result = compare_priority_vectors(local_clock, master.priority_vector);
        if (result != BmcaResult::A_BETTER_THAN_B && result != BmcaResult::A_BETTER_BY_TOPOLOGY) {
            return false; // Found a better or equal master
        }
    }
    
    return true; // Local clock is best
}

MasterInfo* BmcaEngine::select_best_master(const std::vector<MasterInfo>& candidates) {
    MasterInfo* best = nullptr;
    
    for (const auto& candidate : candidates) {
        if (!candidate.valid) continue;
        
        if (!best) {
            best = const_cast<MasterInfo*>(&candidate);
            continue;
        }
        
        BmcaResult result = compare_priority_vectors(candidate.priority_vector, best->priority_vector);
        if (result == BmcaResult::A_BETTER_THAN_B || result == BmcaResult::A_BETTER_BY_TOPOLOGY) {
            best = const_cast<MasterInfo*>(&candidate);
        }
    }
    
    return best;
}

bool BmcaEngine::is_steps_removed_acceptable(uint16_t steps) {
    // IEEE 802.1AS-2021: Maximum steps removed should be reasonable
    // Typically 255 is used as maximum, but we'll use 16 for robustness
    return steps < 16;
}

bool BmcaEngine::is_priority_vector_valid(const PriorityVector& pv) {
    // Basic sanity checks
    if (!is_steps_removed_acceptable(pv.steps_removed)) {
        return false;
    }
    
    // Check for reserved clock class values
    if (pv.grandmaster_clock_quality.clockClass == 255) {
        return false; // Reserved value
    }
    
    // Priority values should be reasonable
    if (pv.grandmaster_priority1 == 255 && pv.grandmaster_priority2 == 255) {
        // Both max priority might indicate invalid/unconfigured clock
        return false;
    }
    
    return true;
}

// ============================================================================
// BmcaCoordinator Implementation
// ============================================================================

BmcaCoordinator::BmcaCoordinator(const ClockIdentity& local_clock_id) 
    : local_clock_id_(local_clock_id)
    , current_grandmaster_(nullptr)
    , local_is_grandmaster_(false)
    , local_priority1_(248)  // Default gPTP priority
    , local_priority2_(248)  // Default gPTP priority
{
    // Initialize default clock quality
    local_clock_quality_.clockClass = 248;  // gPTP default
    local_clock_quality_.clockAccuracy = protocol::ClockAccuracy::WITHIN_1_MS;
    local_clock_quality_.offsetScaledLogVariance = 0x4000;
}

void BmcaCoordinator::process_announce(uint16_t port_id, 
                                     const AnnounceMessage& announce,
                                     std::chrono::steady_clock::time_point receipt_time) {
    MasterInfo master_info = engine_.update_master_info(announce, receipt_time);
    port_masters_[port_id] = master_info;
}

std::vector<BmcaDecision> BmcaCoordinator::run_bmca(const PriorityVector& local_priority) {
    std::vector<BmcaDecision> decisions;
    last_bmca_run_ = std::chrono::steady_clock::now();
    
    // Collect all valid masters
    std::vector<MasterInfo> all_masters;
    for (const auto& pair : port_masters_) {
        if (pair.second.valid) {
            all_masters.push_back(pair.second);
        }
    }
    
    std::cout << "👑 [BMCA] Running Best Master Clock Algorithm" << std::endl;
    std::cout << "   Local clock priority vector:" << std::endl;
    std::cout << "     Priority1: " << static_cast<int>(local_priority.grandmaster_priority1) << std::endl;
    std::cout << "     Clock Class: " << static_cast<int>(local_priority.grandmaster_clock_quality.clockClass) << std::endl;
    std::cout << "     Priority2: " << static_cast<int>(local_priority.grandmaster_priority2) << std::endl;
    std::cout << "   Active master candidates: " << all_masters.size() << std::endl;
    
    // Determine if we should be grandmaster
    bool should_be_gm = engine_.should_be_grandmaster(local_priority, all_masters);
    bool role_changed = (should_be_gm != local_is_grandmaster_);
    
    std::cout << "   BMCA Decision: " << (should_be_gm ? "GRANDMASTER" : "SLAVE") << std::endl;
    if (role_changed) {
        std::cout << "   ⚡ ROLE CHANGE DETECTED!" << std::endl;
    }
    
    local_is_grandmaster_ = should_be_gm;
    
    if (should_be_gm) {
        // We are grandmaster - all ports should be master
        current_grandmaster_ = new PriorityVector(local_priority);
        
        for (const auto& pair : port_masters_) {
            BmcaDecision decision;
            decision.recommended_role = PortRole::MASTER;
            decision.selected_master = nullptr;
            decision.role_changed = role_changed;
            decision.decision_time = last_bmca_run_;
            decisions.push_back(decision);
        }
    } else {
        // Select best master and determine roles per port
        MasterInfo* best_master = engine_.select_best_master(all_masters);
        
        if (best_master) {
            current_grandmaster_ = new PriorityVector(best_master->priority_vector);
        }
        
        for (const auto& pair : port_masters_) {
            BmcaDecision decision;
            decision.recommended_role = engine_.determine_port_role(local_priority, &pair.second);
            decision.selected_master = (pair.second.valid) ? const_cast<MasterInfo*>(&pair.second) : nullptr;
            decision.role_changed = role_changed;
            decision.decision_time = last_bmca_run_;
            decisions.push_back(decision);
        }
    }
    
    return decisions;
}

void BmcaCoordinator::update_local_clock(uint8_t priority1, 
                                        const ClockQuality& quality,
                                        uint8_t priority2) {
    local_priority1_ = priority1;
    local_clock_quality_ = quality;
    local_priority2_ = priority2;
}

std::vector<uint16_t> BmcaCoordinator::check_announce_timeouts(std::chrono::steady_clock::time_point current_time) {
    std::vector<uint16_t> timed_out_ports;
    
    for (auto& pair : port_masters_) {
        if (pair.second.valid && pair.second.is_announce_timeout(current_time)) {
            pair.second.valid = false;
            timed_out_ports.push_back(pair.first);
        }
    }
    
    return timed_out_ports;
}

const MasterInfo* BmcaCoordinator::get_master_info(uint16_t port_id) const {
    auto it = port_masters_.find(port_id);
    if (it != port_masters_.end() && it->second.valid) {
        return &it->second;
    }
    return nullptr;
}

const PriorityVector* BmcaCoordinator::get_grandmaster() const {
    return current_grandmaster_;
}

bool BmcaCoordinator::is_local_grandmaster() const {
    return local_is_grandmaster_;
}

} // namespace bmca
} // namespace gptp
