/**
 * @file gptp_clock.hpp
 * @brief IEEE 802.1AS Clock implementation
 * 
 * This file defines the gPTP clock that manages time synchronization
 * and maintains the local time base.
 */

#pragma once

#include "gptp_protocol.hpp"
#include <chrono>
#include <memory>
#include <vector>

namespace gptp {

    // Forward declaration
    class GptpPort;

    /**
     * @brief IEEE 802.1AS Clock implementation
     * Manages the local time base and synchronization state
     */
    class GptpClock {
    public:
        GptpClock();
        ~GptpClock() = default;
        
        // Clock identity
        const ClockIdentity& get_clock_identity() const { return clock_identity_; }
        void set_clock_identity(const ClockIdentity& identity) { clock_identity_ = identity; }
        
        // Time management
        std::chrono::nanoseconds get_current_time() const;
        void set_current_time(std::chrono::nanoseconds time);
        
        // Clock quality
        const ClockQuality& get_clock_quality() const { return clock_quality_; }
        void set_clock_quality(const ClockQuality& quality) { clock_quality_ = quality; }
        
        // Priority values for BMCA
        uint8_t get_priority1() const { return priority1_; }
        uint8_t get_priority2() const { return priority2_; }
        void set_priority1(uint8_t priority) { priority1_ = priority; }
        void set_priority2(uint8_t priority) { priority2_ = priority; }
        
        // Grandmaster state
        bool is_grandmaster() const { return is_grandmaster_; }
        void set_grandmaster(bool grandmaster) { is_grandmaster_ = grandmaster; }
        
        // Time properties
        int16_t get_current_utc_offset() const { return current_utc_offset_; }
        protocol::TimeSource get_time_source() const { return time_source_; }
        
        // Port management
        void add_port(std::shared_ptr<GptpPort> port);
        void remove_port(uint16_t port_number);
        std::shared_ptr<GptpPort> get_port(uint16_t port_number) const;
        
        // Clock synchronization
        void update_from_master(const Timestamp& master_time, 
                               const Timestamp& local_receipt_time,
                               std::chrono::nanoseconds path_delay);
        
    private:
        ClockIdentity clock_identity_;
        ClockQuality clock_quality_;
        uint8_t priority1_;
        uint8_t priority2_;
        bool is_grandmaster_;
        int16_t current_utc_offset_;
        protocol::TimeSource time_source_;
        
        // Local time base
        std::chrono::steady_clock::time_point startup_time_;
        std::chrono::nanoseconds time_offset_;
        
        // Ports
        std::vector<std::shared_ptr<GptpPort>> ports_;
    };

} // namespace gptp
