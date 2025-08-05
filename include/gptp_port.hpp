/**
 * @file gptp_port.hpp
 * @brief Enhanced gPTP Port with BMCA and Clock Servo integration
 */

#pragma once

#include "gptp_protocol.hpp"
#include "bmca.hpp"
#include "clock_servo.hpp"
#include "gptp_state_machines.hpp"
#include <memory>
#include <chrono>
#include <functional>

namespace gptp {

/**
 * @brief Port state enumeration
 */
enum class PortState : uint8_t {
    INITIALIZING = 0,
    FAULTY = 1,
    DISABLED = 2,
    LISTENING = 3,
    PRE_MASTER = 4,
    MASTER = 5,
    PASSIVE = 6,
    UNCALIBRATED = 7,
    SLAVE = 8
};

/**
 * @brief Port status information for monitoring
 */
struct PortStatus {
    uint16_t port_id;
    PortState state;
    bmca::PortRole bmca_role;
    std::string interface_name;
    bool link_up;
    std::chrono::nanoseconds current_offset;
    double frequency_adjustment_ppb;
    std::chrono::steady_clock::time_point last_sync_time;
    std::chrono::steady_clock::time_point last_announce_time;
    uint64_t sync_messages_sent;
    uint64_t sync_messages_received;
    uint64_t announce_messages_sent;
    uint64_t announce_messages_received;
    
    PortStatus() : port_id(0), state(PortState::INITIALIZING), 
                   bmca_role(bmca::PortRole::DISABLED), link_up(false),
                   current_offset(0), frequency_adjustment_ppb(0.0),
                   sync_messages_sent(0), sync_messages_received(0),
                   announce_messages_sent(0), announce_messages_received(0) {}
};

/**
 * @brief Enhanced gPTP Port with BMCA and Clock Servo integration
 * 
 * This class represents a single gPTP port on a network interface,
 * integrating BMCA decision making with clock synchronization.
 */
class GptpPort {
public:
    /**
     * @brief Callback for sending messages
     */
    using MessageSender = std::function<void(const std::vector<uint8_t>&)>;
    
    /**
     * @brief Callback for port state changes
     */
    using StateChangeCallback = std::function<void(uint16_t port_id, PortState old_state, PortState new_state)>;

    /**
     * @brief Constructor
     * @param port_id Port identifier
     * @param domain_number gPTP domain number
     * @param local_clock_id Local clock identity
     * @param message_sender Callback for sending messages
     */
    GptpPort(uint16_t port_id, 
             uint8_t domain_number,
             const ClockIdentity& local_clock_id,
             MessageSender message_sender);

    ~GptpPort() = default;

    // ========================================================================
    // Configuration and Control
    // ========================================================================

    /**
     * @brief Set port state change callback
     */
    void set_state_change_callback(StateChangeCallback callback);

    /**
     * @brief Enable/disable the port
     */
    void set_enabled(bool enabled);

    /**
     * @brief Set interface name for this port
     */
    void set_interface_name(const std::string& interface_name);

    /**
     * @brief Update link status
     */
    void set_link_status(bool link_up);

    // ========================================================================
    // Message Processing
    // ========================================================================

    /**
     * @brief Process received announce message
     * @param announce Received announce message
     * @param receipt_time Time when message was received
     */
    void process_announce_message(const AnnounceMessage& announce, 
                                 const Timestamp& receipt_time);

    /**
     * @brief Process received sync message
     * @param sync Received sync message
     * @param receipt_time Time when message was received
     */
    void process_sync_message(const SyncMessage& sync,
                             const Timestamp& receipt_time);

    /**
     * @brief Process received follow-up message
     * @param followup Received follow-up message
     */
    void process_followup_message(const FollowUpMessage& followup);

    /**
     * @brief Process received pDelay request message
     * @param pdelay_req Received pDelay request
     * @param receipt_time Time when message was received
     */
    void process_pdelay_request(const PDelayReqMessage& pdelay_req,
                               const Timestamp& receipt_time);

    /**
     * @brief Process received pDelay response message
     * @param pdelay_resp Received pDelay response
     * @param receipt_time Time when message was received
     */
    void process_pdelay_response(const PDelayRespMessage& pdelay_resp,
                                const Timestamp& receipt_time);

    // ========================================================================
    // Periodic Operations
    // ========================================================================

    /**
     * @brief Run periodic tasks (timers, transmissions, etc.)
     * @param current_time Current time
     */
    void run_periodic_tasks(std::chrono::steady_clock::time_point current_time);

    // ========================================================================
    // Status and Monitoring
    // ========================================================================

    /**
     * @brief Get current port status
     */
    PortStatus get_status() const;

    /**
     * @brief Get port ID
     */
    uint16_t get_port_id() const { return port_id_; }

    /**
     * @brief Get current port state
     */
    PortState get_port_state() const { return current_state_; }

    /**
     * @brief Get BMCA role
     */
    bmca::PortRole get_bmca_role() const;

    /**
     * @brief Get synchronization status
     */
    servo::SynchronizationManager::SyncStatus get_sync_status() const;

    /**
     * @brief Get local clock identity
     */
    const ClockIdentity& get_local_clock_identity() const { return local_clock_id_; }

    /**
     * @brief Get message sender callback
     */
    const MessageSender& get_message_sender() const { return message_sender_; }

private:
    // ========================================================================
    // Core Components
    // ========================================================================
    
    uint16_t port_id_;
    uint8_t domain_number_;
    ClockIdentity local_clock_id_;
    MessageSender message_sender_;
    
    // BMCA and synchronization
    std::unique_ptr<bmca::BmcaCoordinator> bmca_coordinator_;
    std::unique_ptr<servo::SynchronizationManager> sync_manager_;
    
    // State management
    PortState current_state_;
    bmca::PortRole current_bmca_role_;
    StateChangeCallback state_change_callback_;
    
    // Configuration
    std::string interface_name_;
    bool enabled_;
    bool link_up_;
    
    // Timing intervals
    std::chrono::milliseconds sync_interval_;          // Default: 125ms
    std::chrono::seconds announce_interval_;           // Default: 1s
    std::chrono::milliseconds pdelay_interval_;        // Default: 1s
    
    // Last transmission times
    std::chrono::steady_clock::time_point last_sync_time_;
    std::chrono::steady_clock::time_point last_announce_time_;
    std::chrono::steady_clock::time_point last_pdelay_time_;
    
    // Message tracking
    uint16_t sync_sequence_id_;
    uint16_t announce_sequence_id_;
    uint16_t pdelay_sequence_id_;
    
    // Statistics
    mutable PortStatus cached_status_;
    
    // Pending sync/follow-up correlation
    struct PendingSync {
        SyncMessage sync_message;
        Timestamp receipt_time;
        std::chrono::steady_clock::time_point timestamp;
    };
    std::map<uint16_t, PendingSync> pending_syncs_;  // Key: sequence ID
    
    // ========================================================================
    // Internal Methods
    // ========================================================================
    
    /**
     * @brief Handle BMCA role change
     */
    void handle_bmca_role_change(bmca::PortRole new_role);
    
    /**
     * @brief Update port state based on BMCA role and other factors
     */
    void update_port_state();
    
    /**
     * @brief Transmit announce message (if master)
     */
    void transmit_announce_message();
    
    /**
     * @brief Transmit sync message (if master)
     */
    void transmit_sync_message();
    
    /**
     * @brief Transmit pDelay request
     */
    void transmit_pdelay_request();
    
    /**
     * @brief Transmit pDelay response
     */
    void transmit_pdelay_response(const PDelayReqMessage& request,
                                 const Timestamp& request_receipt_time);
    
    /**
     * @brief Build announce message for transmission
     */
    AnnounceMessage build_announce_message();
    
    /**
     * @brief Build sync message for transmission
     */
    SyncMessage build_sync_message();
    
    /**
     * @brief Serialize message to bytes for transmission
     */
    std::vector<uint8_t> serialize_message(const GptpMessage& message);
    
    /**
     * @brief Update statistics
     */
    void update_statistics() const;
    
    /**
     * @brief Change port state and notify callback
     */
    void change_state(PortState new_state);
};

} // namespace gptp
