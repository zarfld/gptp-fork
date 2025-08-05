/**
 * @file sequence_number_manager.hpp
 * @brief IEEE 802.1AS-2021 Sequence Number Management Implementation
 * 
 * Implementation of Section 10.5.7 - Sequence Number Management
 * 
 * Key requirements:
 * - Each PortSync entity maintains separate sequenceId pools for Announce and Signaling messages
 * - Each message type has independent sequence numbers that increment by 1
 * - UInteger16 rollover handling (0-65535)
 * - Per-port sequence number management
 */

#ifndef GPTP_SEQUENCE_NUMBER_MANAGER_HPP
#define GPTP_SEQUENCE_NUMBER_MANAGER_HPP

#include "gptp_protocol.hpp"
#include <cstdint>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <vector>
#include <string>
#include <cstdio>

namespace gptp {
namespace sequence {

/**
 * @brief Sequence number pool for a specific message type
 * IEEE 802.1AS-2021 Section 10.5.7
 */
class SequenceNumberPool {
public:
    SequenceNumberPool() : current_sequence_(0) {}
    
    /**
     * @brief Get next sequence number for this message type
     * @return Next sequence number (with UInteger16 rollover)
     */
    uint16_t get_next_sequence() {
        uint16_t next = current_sequence_.fetch_add(1, std::memory_order_relaxed);
        return next; // UInteger16 automatically handles rollover at 65536
    }
    
    /**
     * @brief Get current sequence number (last used)
     * @return Current sequence number
     */
    uint16_t get_current_sequence() const {
        return current_sequence_.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Reset sequence number (for testing or initialization)
     * @param value New sequence number value
     */
    void reset_sequence(uint16_t value = 0) {
        current_sequence_.store(value, std::memory_order_relaxed);
    }

private:
    std::atomic<uint16_t> current_sequence_;
};

/**
 * @brief Per-port sequence number management
 * IEEE 802.1AS-2021 Section 10.5.7
 */
class PortSequenceManager {
public:
    PortSequenceManager() = default;
    
    /**
     * @brief Get next sequence number for Announce messages
     * @return Next Announce sequence number
     */
    uint16_t get_next_announce_sequence() {
        return announce_pool_.get_next_sequence();
    }
    
    /**
     * @brief Get next sequence number for Signaling messages
     * @return Next Signaling sequence number
     */
    uint16_t get_next_signaling_sequence() {
        return signaling_pool_.get_next_sequence();
    }
    
    /**
     * @brief Get next sequence number for Sync messages
     * @return Next Sync sequence number
     */
    uint16_t get_next_sync_sequence() {
        return sync_pool_.get_next_sequence();
    }
    
    /**
     * @brief Get next sequence number for Follow_Up messages
     * @return Next Follow_Up sequence number
     */
    uint16_t get_next_followup_sequence() {
        return followup_pool_.get_next_sequence();
    }
    
    /**
     * @brief Get next sequence number for Pdelay_Req messages
     * @return Next Pdelay_Req sequence number
     */
    uint16_t get_next_pdelay_req_sequence() {
        return pdelay_req_pool_.get_next_sequence();
    }
    
    /**
     * @brief Get next sequence number for Pdelay_Resp messages
     * @return Next Pdelay_Resp sequence number
     */
    uint16_t get_next_pdelay_resp_sequence() {
        return pdelay_resp_pool_.get_next_sequence();
    }
    
    /**
     * @brief Get current sequence numbers for all message types
     */
    struct SequenceStatus {
        uint16_t announce_sequence;
        uint16_t signaling_sequence;
        uint16_t sync_sequence;
        uint16_t followup_sequence;
        uint16_t pdelay_req_sequence;
        uint16_t pdelay_resp_sequence;
    };
    
    SequenceStatus get_sequence_status() const {
        return {
            announce_pool_.get_current_sequence(),
            signaling_pool_.get_current_sequence(),
            sync_pool_.get_current_sequence(),
            followup_pool_.get_current_sequence(),
            pdelay_req_pool_.get_current_sequence(),
            pdelay_resp_pool_.get_current_sequence()
        };
    }
    
    /**
     * @brief Reset all sequence numbers (for testing)
     */
    void reset_all_sequences() {
        announce_pool_.reset_sequence();
        signaling_pool_.reset_sequence();
        sync_pool_.reset_sequence();
        followup_pool_.reset_sequence();
        pdelay_req_pool_.reset_sequence();
        pdelay_resp_pool_.reset_sequence();
    }

private:
    SequenceNumberPool announce_pool_;      // IEEE 802.1AS-2021 Section 10.5.7
    SequenceNumberPool signaling_pool_;     // IEEE 802.1AS-2021 Section 10.5.7
    SequenceNumberPool sync_pool_;          // For completeness
    SequenceNumberPool followup_pool_;      // For completeness
    SequenceNumberPool pdelay_req_pool_;    // For Link Delay measurements
    SequenceNumberPool pdelay_resp_pool_;   // For Link Delay measurements
};

/**
 * @brief Global sequence number manager for all ports
 * Thread-safe implementation
 */
class SequenceNumberManager {
public:
    /**
     * @brief Get sequence manager for a specific port
     * @param port_id Port identifier
     * @return Reference to port's sequence manager
     */
    PortSequenceManager& get_port_manager(uint16_t port_id) {
        std::lock_guard<std::mutex> lock(ports_mutex_);
        return port_managers_[port_id];
    }
    
    /**
     * @brief Remove port sequence manager
     * @param port_id Port identifier
     */
    void remove_port(uint16_t port_id) {
        std::lock_guard<std::mutex> lock(ports_mutex_);
        port_managers_.erase(port_id);
    }
    
    /**
     * @brief Get sequence number for specific message type and port
     * @param port_id Port identifier
     * @param message_type Message type
     * @return Next sequence number
     */
    uint16_t get_next_sequence(uint16_t port_id, protocol::MessageType message_type) {
        auto& port_manager = get_port_manager(port_id);
        
        switch (message_type) {
            case protocol::MessageType::ANNOUNCE:
                return port_manager.get_next_announce_sequence();
                
            case protocol::MessageType::SIGNALING:
                return port_manager.get_next_signaling_sequence();
                
            case protocol::MessageType::SYNC:
                return port_manager.get_next_sync_sequence();
                
            case protocol::MessageType::FOLLOW_UP:
                return port_manager.get_next_followup_sequence();
                
            case protocol::MessageType::PDELAY_REQ:
                return port_manager.get_next_pdelay_req_sequence();
                
            case protocol::MessageType::PDELAY_RESP:
                return port_manager.get_next_pdelay_resp_sequence();
                
            default:
                // For other message types, use a default pool
                return port_manager.get_next_announce_sequence();
        }
    }
    
    /**
     * @brief Get all active port IDs
     * @return Vector of port IDs
     */
    std::vector<uint16_t> get_active_ports() const {
        std::lock_guard<std::mutex> lock(ports_mutex_);
        std::vector<uint16_t> ports;
        ports.reserve(port_managers_.size());
        
        for (const auto& pair : port_managers_) {
            ports.push_back(pair.first);
        }
        
        return ports;
    }
    
    /**
     * @brief Get sequence status for all ports
     * @return Map of port ID to sequence status
     */
    std::unordered_map<uint16_t, PortSequenceManager::SequenceStatus> get_all_sequence_status() const {
        std::lock_guard<std::mutex> lock(ports_mutex_);
        std::unordered_map<uint16_t, PortSequenceManager::SequenceStatus> status;
        
        for (const auto& pair : port_managers_) {
            status[pair.first] = pair.second.get_sequence_status();
        }
        
        return status;
    }
    
    /**
     * @brief Reset all sequence numbers for all ports (for testing)
     */
    void reset_all_ports() {
        std::lock_guard<std::mutex> lock(ports_mutex_);
        for (auto& pair : port_managers_) {
            pair.second.reset_all_sequences();
        }
    }

private:
    mutable std::mutex ports_mutex_;
    std::unordered_map<uint16_t, PortSequenceManager> port_managers_;
};

/**
 * @brief Sequence number utilities
 */
namespace utils {

/**
 * @brief Check if sequence number indicates rollover
 * @param previous Previous sequence number
 * @param current Current sequence number
 * @return True if rollover occurred
 */
inline bool is_sequence_rollover(uint16_t previous, uint16_t current) {
    // UInteger16 rollover: 65535 -> 0
    return (previous == 0xFFFF && current == 0x0000);
}

/**
 * @brief Calculate sequence number difference with rollover handling
 * @param from Starting sequence number
 * @param to Ending sequence number
 * @return Difference (handling rollover)
 */
inline uint16_t sequence_difference(uint16_t from, uint16_t to) {
    if (to >= from) {
        return to - from;
    } else {
        // Handle rollover case
        return (0xFFFF - from) + to + 1;
    }
}

/**
 * @brief Validate sequence number progression
 * @param expected Expected sequence number
 * @param received Received sequence number
 * @return True if sequence is valid (expected or valid rollover)
 */
inline bool is_valid_sequence_progression(uint16_t expected, uint16_t received) {
    return (received == expected) || 
           (expected == 0xFFFF && received == 0x0000); // Rollover case
}

/**
 * @brief Format sequence number for display
 * @param sequence Sequence number
 * @return Formatted string
 */
inline std::string format_sequence(uint16_t sequence) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%u (0x%04X)", sequence, sequence);
    return std::string(buffer);
}

} // namespace utils

} // namespace sequence
} // namespace gptp

#endif // GPTP_SEQUENCE_NUMBER_MANAGER_HPP
