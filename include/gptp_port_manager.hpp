/**
 * @file gptp_port_manager.hpp
 * @brief Enhanced gPTP Port Manager with BMCA and Clock Servo integration
 */

#pragma once

#include "gptp_protocol.hpp"
#include "bmca.hpp"
#include "clock_servo.hpp"
#include <memory>
#include <chrono>
#include <functional>
#include <map>

// Forward declaration
namespace gptp {
    class GptpPort;
    class GptpClock;
}

namespace gptp {

/**
 * @brief Enhanced port manager that integrates BMCA with existing state machines
 * 
 * This class wraps the existing GptpPort class and adds BMCA decision making
 * and clock servo integration for real network-based synchronization.
 */
class GptpPortManager {
public:
    /**
     * @brief Callback for sending messages over the network
     */
    using MessageSender = std::function<void(uint16_t port_id, const std::vector<uint8_t>&)>;
    
    /**
     * @brief Callback for port role changes
     */
    using RoleChangeCallback = std::function<void(uint16_t port_id, bmca::PortRole old_role, bmca::PortRole new_role)>;

    /**
     * @brief Constructor
     * @param local_clock_id Local clock identity for BMCA
     * @param message_sender Callback for sending messages
     */
    explicit GptpPortManager(const ClockIdentity& local_clock_id, MessageSender message_sender);

    ~GptpPortManager(); // Implemented in .cpp to handle incomplete types

    // ========================================================================
    // Port Management
    // ========================================================================

    /**
     * @brief Add a gPTP port
     * @param port_id Port identifier
     * @param domain_number gPTP domain number (default 0)
     * @param clock Shared clock instance
     * @return Success status
     */
    bool add_port(uint16_t port_id, uint8_t domain_number = 0, GptpClock* clock = nullptr);

    /**
     * @brief Remove a port
     * @param port_id Port identifier
     */
    void remove_port(uint16_t port_id);

    /**
     * @brief Enable a port
     * @param port_id Port identifier
     */
    void enable_port(uint16_t port_id);

    /**
     * @brief Disable a port
     * @param port_id Port identifier
     */
    void disable_port(uint16_t port_id);

    /**
     * @brief Set role change callback
     */
    void set_role_change_callback(RoleChangeCallback callback);

    // ========================================================================
    // Message Processing with BMCA Integration
    // ========================================================================

    /**
     * @brief Process received announce message with BMCA evaluation
     * @param port_id Port that received the message
     * @param announce Received announce message
     * @param receipt_time Time when message was received
     */
    void process_announce_message(uint16_t port_id, 
                                 const AnnounceMessage& announce, 
                                 const Timestamp& receipt_time);

    /**
     * @brief Process received sync message with servo integration
     * @param port_id Port that received the message
     * @param sync Received sync message
     * @param receipt_time Time when message was received
     */
    void process_sync_message(uint16_t port_id,
                             const SyncMessage& sync,
                             const Timestamp& receipt_time);

    /**
     * @brief Process received follow-up message
     * @param port_id Port that received the message
     * @param followup Received follow-up message
     */
    void process_followup_message(uint16_t port_id, const FollowUpMessage& followup);

    // ========================================================================
    // Periodic Operations
    // ========================================================================

    /**
     * @brief Run periodic tasks for all ports
     * @param current_time Current time
     */
    void run_periodic_tasks(std::chrono::steady_clock::time_point current_time);

    // ========================================================================
    // Status and Monitoring
    // ========================================================================

    /**
     * @brief Get status for all ports
     */
    std::map<uint16_t, bmca::PortRole> get_port_roles() const;

    /**
     * @brief Get synchronization status for a port
     */
    servo::SynchronizationManager::SyncStatus get_sync_status(uint16_t port_id) const;

    /**
     * @brief Get BMCA decisions for all ports
     */
    std::vector<bmca::BmcaDecision> get_bmca_decisions() const;

private:
    // ========================================================================
    // Port Information
    // ========================================================================
    
    struct PortInfo {
        std::unique_ptr<GptpPort> gptp_port;
        uint8_t domain_number;
        bmca::PortRole current_role;
        std::chrono::steady_clock::time_point last_announce_tx_time;
        std::chrono::steady_clock::time_point last_sync_tx_time;
        
        // Pending sync/follow-up correlation
        struct PendingSync {
            SyncMessage sync_message;
            Timestamp receipt_time;
            std::chrono::steady_clock::time_point timeout;
        };
        std::map<uint16_t, PendingSync> pending_syncs;  // Key: sequence ID
        
        PortInfo() : domain_number(0), current_role(bmca::PortRole::DISABLED) {}
    };

    // ========================================================================
    // Core Components
    // ========================================================================
    
    ClockIdentity local_clock_id_;
    MessageSender message_sender_;
    RoleChangeCallback role_change_callback_;
    
    // BMCA coordinator (shared across all ports in domain)
    std::map<uint8_t, std::unique_ptr<bmca::BmcaCoordinator>> bmca_coordinators_;
    
    // Synchronization manager per domain
    std::map<uint8_t, std::unique_ptr<servo::SynchronizationManager>> sync_managers_;
    
    // Port management
    std::map<uint16_t, PortInfo> ports_;
    
    // Default clock instance
    std::unique_ptr<GptpClock> default_clock_;
    
    // Timing configuration
    std::chrono::seconds announce_interval_;
    std::chrono::milliseconds sync_interval_;
    std::chrono::milliseconds followup_timeout_;

    // ========================================================================
    // Internal Methods
    // ========================================================================
    
    /**
     * @brief Get or create BMCA coordinator for domain
     */
    bmca::BmcaCoordinator* get_bmca_coordinator(uint8_t domain_number);
    
    /**
     * @brief Get or create sync manager for domain
     */
    servo::SynchronizationManager* get_sync_manager(uint8_t domain_number);
    
    /**
     * @brief Handle BMCA role change for a port
     */
    void handle_role_change(uint16_t port_id, bmca::PortRole new_role);
    
    /**
     * @brief Transmit announce message from a port
     */
    void transmit_announce_message(uint16_t port_id);
    
    /**
     * @brief Transmit sync message from a port
     */
    void transmit_sync_message(uint16_t port_id);
    
    /**
     * @brief Build announce message for transmission
     */
    AnnounceMessage build_announce_message(uint8_t domain_number, uint16_t port_id);
    
    /**
     * @brief Build sync message for transmission
     */
    SyncMessage build_sync_message(uint8_t domain_number, uint16_t port_id);
    
    /**
     * @brief Serialize message to bytes for network transmission
     */
    std::vector<uint8_t> serialize_message(const AnnounceMessage& message);
    std::vector<uint8_t> serialize_message(const SyncMessage& message);
    std::vector<uint8_t> serialize_message(const FollowUpMessage& message);
    
    /**
     * @brief Create local priority vector for BMCA
     */
    bmca::PriorityVector create_local_priority_vector(uint8_t domain_number);
    
    /**
     * @brief Update port state based on BMCA role
     */
    void update_port_state(uint16_t port_id, bmca::PortRole role);
};

} // namespace gptp
