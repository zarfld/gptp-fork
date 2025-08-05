/**
 * @file gptp_port_manager.cpp
 * @brief Enhanced gPTP Port Manager implementation with BMCA integration
 */

#include "../../include/gptp_port_manager.hpp"
#include "../../include/gptp_clock.hpp"
#include "../../include/gptp_state_machines.hpp"
#include <iostream>

namespace gptp {

GptpPortManager::GptpPortManager(const ClockIdentity& local_clock_id, MessageSender message_sender)
    : local_clock_id_(local_clock_id)
    , message_sender_(std::move(message_sender))
    , announce_interval_(std::chrono::seconds(1))      // 1 second announce interval
    , sync_interval_(std::chrono::milliseconds(125))   // 125ms sync interval  
    , followup_timeout_(std::chrono::milliseconds(100)) // 100ms follow-up timeout
{
    // Create default clock instance
    default_clock_ = std::make_unique<GptpClock>();
}

GptpPortManager::~GptpPortManager() {
    // Destructor implementation needed for std::unique_ptr with incomplete types
}

// ============================================================================
// Port Management
// ============================================================================

bool GptpPortManager::add_port(uint16_t port_id, uint8_t domain_number, GptpClock* clock) {
    if (ports_.find(port_id) != ports_.end()) {
        std::cerr << "Port " << port_id << " already exists" << std::endl;
        return false;
    }
    
    // Use provided clock or default
    GptpClock* port_clock = clock ? clock : default_clock_.get();
    
    // Create new port info
    PortInfo& port_info = ports_[port_id];
    port_info.domain_number = domain_number;
    port_info.current_role = bmca::PortRole::DISABLED;
    
    // Create underlying GptpPort
    port_info.gptp_port = std::make_unique<GptpPort>(port_id, port_clock);
    port_info.gptp_port->initialize();
    
    std::cout << "Added gPTP port " << port_id << " on domain " << static_cast<int>(domain_number) << std::endl;
    return true;
}

void GptpPortManager::remove_port(uint16_t port_id) {
    auto it = ports_.find(port_id);
    if (it != ports_.end()) {
        std::cout << "Removing gPTP port " << port_id << std::endl;
        ports_.erase(it);
    }
}

void GptpPortManager::enable_port(uint16_t port_id) {
    auto it = ports_.find(port_id);
    if (it != ports_.end()) {
        std::cout << "Enabling gPTP port " << port_id << std::endl;
        it->second.gptp_port->enable();
        
        // Set initial role to PASSIVE (will be updated by BMCA)
        handle_role_change(port_id, bmca::PortRole::PASSIVE);
    }
}

void GptpPortManager::disable_port(uint16_t port_id) {
    auto it = ports_.find(port_id);
    if (it != ports_.end()) {
        std::cout << "Disabling gPTP port " << port_id << std::endl;
        it->second.gptp_port->disable();
        handle_role_change(port_id, bmca::PortRole::DISABLED);
    }
}

void GptpPortManager::set_role_change_callback(RoleChangeCallback callback) {
    role_change_callback_ = std::move(callback);
}

// ============================================================================
// Message Processing with BMCA Integration
// ============================================================================

void GptpPortManager::process_announce_message(uint16_t port_id, 
                                              const AnnounceMessage& announce, 
                                              const Timestamp& receipt_time) {
    auto port_it = ports_.find(port_id);
    if (port_it == ports_.end()) {
        return;
    }
    
    PortInfo& port_info = port_it->second;
    uint8_t domain = port_info.domain_number;
    
    std::cout << "Processing announce message on port " << port_id << " domain " << static_cast<int>(domain) << std::endl;
    
    // Get BMCA coordinator for this domain
    auto* bmca = get_bmca_coordinator(domain);
    if (!bmca) {
        std::cerr << "No BMCA coordinator for domain " << static_cast<int>(domain) << std::endl;
        return;
    }
    
    // Process announce with BMCA
    auto current_time = std::chrono::steady_clock::now();
    bmca->process_announce(port_id, announce, current_time);
    
    // Run BMCA to get updated decisions
    auto local_priority = create_local_priority_vector(domain);
    auto decisions = bmca->run_bmca(local_priority);
    
    // Apply BMCA decisions to ports
    for (const auto& decision : decisions) {
        // Note: BmcaDecision doesn't have port_id, so we apply to the current port
        if (decision.recommended_role != port_info.current_role) {
            std::cout << "BMCA role change for port " << port_id << ": " 
                      << static_cast<int>(port_info.current_role) << " -> " 
                      << static_cast<int>(decision.recommended_role) << std::endl;
            handle_role_change(port_id, decision.recommended_role);
        }
    }
    
    // Also pass to underlying state machine
    port_info.gptp_port->process_announce_message(announce);
}

void GptpPortManager::process_sync_message(uint16_t port_id,
                                          const SyncMessage& sync,
                                          const Timestamp& receipt_time) {
    auto port_it = ports_.find(port_id);
    if (port_it == ports_.end()) {
        return;
    }
    
    PortInfo& port_info = port_it->second;
    
    // Only process sync if we're a slave
    if (port_info.current_role != bmca::PortRole::SLAVE) {
        return;
    }
    
    std::cout << "Processing sync message " << sync.header.sequenceId 
              << " on slave port " << port_id << std::endl;
    
    // Store pending sync for follow-up correlation
    auto& pending = port_info.pending_syncs[sync.header.sequenceId];
    pending.sync_message = sync;
    pending.receipt_time = receipt_time;
    pending.timeout = std::chrono::steady_clock::now() + followup_timeout_;
    
    // Also pass to underlying state machine
    port_info.gptp_port->process_sync_message(sync, receipt_time);
}

void GptpPortManager::process_followup_message(uint16_t port_id, const FollowUpMessage& followup) {
    auto port_it = ports_.find(port_id);
    if (port_it == ports_.end()) {
        return;
    }
    
    PortInfo& port_info = port_it->second;
    
    // Only process follow-up if we're a slave
    if (port_info.current_role != bmca::PortRole::SLAVE) {
        return;
    }
    
    std::cout << "Processing follow-up message " << followup.header.sequenceId 
              << " on slave port " << port_id << std::endl;
    
    // Find corresponding sync message
    auto sync_it = port_info.pending_syncs.find(followup.header.sequenceId);
    if (sync_it == port_info.pending_syncs.end()) {
        std::cerr << "No matching sync for follow-up " << followup.header.sequenceId << std::endl;
        return;
    }
    
    const auto& pending = sync_it->second;
    uint8_t domain = port_info.domain_number;
    
    // Get sync manager for this domain
    auto* sync_manager = get_sync_manager(domain);
    if (!sync_manager) {
        std::cerr << "No sync manager for domain " << static_cast<int>(domain) << std::endl;
        return;
    }
    
    // Set this port as slave if not already
    sync_manager->set_slave_port(port_id);
    
    // Process sync/follow-up pair for synchronization
    // TODO: Get actual path delay from LinkDelay state machine
    std::chrono::nanoseconds path_delay(0);
    
    sync_manager->process_sync_followup(port_id, 
                                       pending.sync_message,
                                       pending.receipt_time,
                                       followup,
                                       path_delay);
    
    // Remove processed sync
    port_info.pending_syncs.erase(sync_it);
    
    // Also pass to underlying state machine
    port_info.gptp_port->process_follow_up_message(followup);
}

// ============================================================================
// Periodic Operations
// ============================================================================

void GptpPortManager::run_periodic_tasks(std::chrono::steady_clock::time_point current_time) {
    // Run periodic tasks for each port
    for (auto& port_pair : ports_) {
        uint16_t port_id = port_pair.first;
        PortInfo& port_info = port_pair.second;
        // Update underlying state machine
        auto current_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            current_time.time_since_epoch());
        port_info.gptp_port->tick(current_time_ns);
        
        // Handle master behavior
        if (port_info.current_role == bmca::PortRole::MASTER) {
            // Transmit announce messages
            if (current_time - port_info.last_announce_tx_time >= announce_interval_) {
                transmit_announce_message(port_id);
                port_info.last_announce_tx_time = current_time;
            }
            
            // Transmit sync messages
            if (current_time - port_info.last_sync_tx_time >= sync_interval_) {
                transmit_sync_message(port_id);
                port_info.last_sync_tx_time = current_time;
            }
        }
        
        // Clean up expired pending syncs
        auto& pending_syncs = port_info.pending_syncs;
        for (auto it = pending_syncs.begin(); it != pending_syncs.end();) {
            if (current_time > it->second.timeout) {
                std::cout << "Sync " << it->first << " timed out waiting for follow-up" << std::endl;
                it = pending_syncs.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    // Run BMCA timeout checks for each domain
    for (auto& domain_pair : bmca_coordinators_) {
        uint8_t domain = domain_pair.first;
        auto* bmca = domain_pair.second.get();
        auto timed_out_ports = bmca->check_announce_timeouts(current_time);
        for (uint16_t timed_out_port_id : timed_out_ports) {
            std::cout << "Announce timeout on port " << timed_out_port_id << " domain " << static_cast<int>(domain) << std::endl;
            // Re-run BMCA to handle timeout
            auto local_priority = create_local_priority_vector(domain);
            auto decisions = bmca->run_bmca(local_priority);
            
            // Apply any role changes - Note: BmcaDecision doesn't have port_id
            // so we apply to all ports in this domain
            for (auto& port_pair : ports_) {
                uint16_t check_port_id = port_pair.first;
                PortInfo& check_port_info = port_pair.second;
                if (check_port_info.domain_number == domain) {
                    for (const auto& decision : decisions) {
                        if (decision.recommended_role != check_port_info.current_role) {
                            handle_role_change(check_port_id, decision.recommended_role);
                        }
                    }
                }
            }
        }
    }
}

// ============================================================================
// Status and Monitoring
// ============================================================================

std::map<uint16_t, bmca::PortRole> GptpPortManager::get_port_roles() const {
    std::map<uint16_t, bmca::PortRole> roles;
    for (const auto& port_pair : ports_) {
        roles[port_pair.first] = port_pair.second.current_role;
    }
    return roles;
}

servo::SynchronizationManager::SyncStatus GptpPortManager::get_sync_status(uint16_t port_id) const {
    auto port_it = ports_.find(port_id);
    if (port_it == ports_.end()) {
        return servo::SynchronizationManager::SyncStatus{}; // Default empty status
    }
    
    uint8_t domain = port_it->second.domain_number;
    auto sync_it = sync_managers_.find(domain);
    if (sync_it != sync_managers_.end()) {
        return sync_it->second->get_sync_status();
    }
    
    return servo::SynchronizationManager::SyncStatus{};
}

std::vector<bmca::BmcaDecision> GptpPortManager::get_bmca_decisions() const {
    std::vector<bmca::BmcaDecision> all_decisions;
    
    for (const auto& domain_pair : bmca_coordinators_) {
        uint8_t domain = domain_pair.first;
        auto* bmca = domain_pair.second.get();
        auto local_priority = const_cast<GptpPortManager*>(this)->create_local_priority_vector(domain);
        auto decisions = bmca->run_bmca(local_priority);
        all_decisions.insert(all_decisions.end(), decisions.begin(), decisions.end());
    }
    
    return all_decisions;
}

// ============================================================================
// Internal Methods
// ============================================================================

bmca::BmcaCoordinator* GptpPortManager::get_bmca_coordinator(uint8_t domain_number) {
    auto it = bmca_coordinators_.find(domain_number);
    if (it == bmca_coordinators_.end()) {
        // Create new BMCA coordinator for this domain
        std::cout << "Creating BMCA coordinator for domain " << static_cast<int>(domain_number) << std::endl;
        bmca_coordinators_[domain_number] = std::make_unique<bmca::BmcaCoordinator>(local_clock_id_);
        return bmca_coordinators_[domain_number].get();
    }
    return it->second.get();
}

servo::SynchronizationManager* GptpPortManager::get_sync_manager(uint8_t domain_number) {
    auto it = sync_managers_.find(domain_number);
    if (it == sync_managers_.end()) {
        // Create new sync manager for this domain
        std::cout << "Creating sync manager for domain " << static_cast<int>(domain_number) << std::endl;
        sync_managers_[domain_number] = std::make_unique<servo::SynchronizationManager>();
        return sync_managers_[domain_number].get();
    }
    return it->second.get();
}

void GptpPortManager::handle_role_change(uint16_t port_id, bmca::PortRole new_role) {
    auto port_it = ports_.find(port_id);
    if (port_it == ports_.end()) {
        return;
    }
    
    PortInfo& port_info = port_it->second;
    bmca::PortRole old_role = port_info.current_role;
    
    if (old_role != new_role) {
        std::cout << "Port " << port_id << " role change: " 
                  << static_cast<int>(old_role) << " -> " << static_cast<int>(new_role) << std::endl;
        
        port_info.current_role = new_role;
        
        // Update underlying port state
        update_port_state(port_id, new_role);
        
        // Notify callback
        if (role_change_callback_) {
            role_change_callback_(port_id, old_role, new_role);
        }
    }
}

void GptpPortManager::transmit_announce_message(uint16_t port_id) {
    auto port_it = ports_.find(port_id);
    if (port_it == ports_.end()) {
        return;
    }
    
    uint8_t domain = port_it->second.domain_number;
    
    // Build and transmit announce message
    auto announce = build_announce_message(domain, port_id);
    auto serialized = serialize_message(announce);
    
    std::cout << "Transmitting announce message from port " << port_id 
              << " (sequence " << announce.header.sequenceId << ")" << std::endl;
    
    message_sender_(port_id, serialized);
}

void GptpPortManager::transmit_sync_message(uint16_t port_id) {
    auto port_it = ports_.find(port_id);
    if (port_it == ports_.end()) {
        return;
    }
    
    uint8_t domain = port_it->second.domain_number;
    
    // Build and transmit sync message
    auto sync = build_sync_message(domain, port_id);
    auto serialized = serialize_message(sync);
    
    std::cout << "Transmitting sync message from port " << port_id 
              << " (sequence " << sync.header.sequenceId << ")" << std::endl;
    
    message_sender_(port_id, serialized);
    
    // TODO: Send corresponding follow-up message with precise timestamp
}

AnnounceMessage GptpPortManager::build_announce_message(uint8_t domain_number, uint16_t port_id) {
    AnnounceMessage announce;
    
    // Fill in header
    announce.header.messageType = static_cast<uint8_t>(protocol::MessageType::ANNOUNCE);
    announce.header.versionPTP = 2;
    announce.header.messageLength = sizeof(AnnounceMessage);
    announce.header.domainNumber = domain_number;
    announce.header.flags = 0;
    announce.header.correctionField = 0;
    announce.header.sourcePortIdentity.clockIdentity = local_clock_id_;
    announce.header.sourcePortIdentity.portNumber = port_id;
    announce.header.sequenceId = sequence_manager_.get_next_sequence(port_id, protocol::MessageType::ANNOUNCE);
    announce.header.controlField = 5; // Other
    announce.header.logMessageInterval = 0; // 1 second
    
    // Fill in announce-specific fields
    announce.originTimestamp.set_seconds(0); // Will be set at transmission
    announce.originTimestamp.nanoseconds = 0;
    announce.currentUtcOffset = 37; // Current TAI-UTC offset
    
    // Get proper clock quality and priorities from clock quality manager
    // For now, use IEEE 802.1AS compliant defaults - this should be replaced
    // with actual ClockQualityManager integration
    announce.grandmasterPriority1 = 248; // IEEE 802.1AS default for end station  
    announce.grandmasterPriority2 = 248; // IEEE 802.1AS default for end station
    
    // Pack ClockQuality according to IEEE 802.1AS format
    // clockClass=248 (default), clockAccuracy=UNKNOWN(0xFE), offsetScaledLogVariance=0x436A
    announce.grandmasterClockQuality = (static_cast<uint32_t>(248) << 24) |      // clockClass
                                      (static_cast<uint32_t>(0xFE) << 16) |      // clockAccuracy 
                                      static_cast<uint32_t>(0x436A);             // offsetScaledLogVariance
    
    announce.grandmasterIdentity = local_clock_id_; // We are grandmaster if we're transmitting
    announce.stepsRemoved = 0; // We are the grandmaster
    announce.timeSource = static_cast<uint8_t>(protocol::TimeSource::INTERNAL_OSCILLATOR);
    
    return announce;
}

SyncMessage GptpPortManager::build_sync_message(uint8_t domain_number, uint16_t port_id) {
    SyncMessage sync;
    
    // Fill in header
    sync.header.messageType = static_cast<uint8_t>(protocol::MessageType::SYNC);
    sync.header.versionPTP = 2;
    sync.header.messageLength = sizeof(SyncMessage);
    sync.header.domainNumber = domain_number;
    sync.header.flags = 0x02; // twoStepFlag
    sync.header.correctionField = 0;
    sync.header.sourcePortIdentity.clockIdentity = local_clock_id_;
    sync.header.sourcePortIdentity.portNumber = port_id;
    sync.header.sequenceId = sequence_manager_.get_next_sequence(port_id, protocol::MessageType::SYNC);
    sync.header.controlField = 0; // Sync
    sync.header.logMessageInterval = -3; // 125ms
    
    // Origin timestamp will be set at transmission time
    sync.originTimestamp.set_seconds(0);
    sync.originTimestamp.nanoseconds = 0;
    
    return sync;
}

std::vector<uint8_t> GptpPortManager::serialize_message(const AnnounceMessage& message) {
    // TODO: Implement proper message serialization
    std::vector<uint8_t> data(sizeof(AnnounceMessage));
    std::memcpy(data.data(), &message, sizeof(AnnounceMessage));
    return data;
}

std::vector<uint8_t> GptpPortManager::serialize_message(const SyncMessage& message) {
    // TODO: Implement proper message serialization
    std::vector<uint8_t> data(sizeof(SyncMessage));
    std::memcpy(data.data(), &message, sizeof(SyncMessage));
    return data;
}

std::vector<uint8_t> GptpPortManager::serialize_message(const FollowUpMessage& message) {
    // TODO: Implement proper message serialization
    std::vector<uint8_t> data(sizeof(FollowUpMessage));
    std::memcpy(data.data(), &message, sizeof(FollowUpMessage));
    return data;
}

bmca::PriorityVector GptpPortManager::create_local_priority_vector(uint8_t domain_number) {
    bmca::PriorityVector local_priority;
    
    // Create local clock priority vector
    local_priority.grandmaster_identity = local_clock_id_;
    local_priority.grandmaster_priority1 = 128; // Default priority
    local_priority.grandmaster_clock_quality.clockClass = 248; // Slave-only by default
    local_priority.grandmaster_clock_quality.clockAccuracy = protocol::ClockAccuracy::UNKNOWN;
    local_priority.grandmaster_clock_quality.offsetScaledLogVariance = 0x436A;
    local_priority.grandmaster_priority2 = 128; // Default priority
    local_priority.sender_identity = local_clock_id_;
    local_priority.steps_removed = 0;
    
    return local_priority;
}

void GptpPortManager::update_port_state(uint16_t port_id, bmca::PortRole role) {
    auto port_it = ports_.find(port_id);
    if (port_it == ports_.end()) {
        return;
    }
    
    // Map BMCA role to port state
    PortState new_state;
    switch (role) {
        case bmca::PortRole::MASTER:
            new_state = PortState::MASTER;
            break;
        case bmca::PortRole::SLAVE:
            new_state = PortState::SLAVE;
            break;
        case bmca::PortRole::PASSIVE:
            new_state = PortState::PASSIVE;
            break;
        case bmca::PortRole::DISABLED:
        default:
            new_state = PortState::DISABLED;
            break;
    }
    
    // Update underlying port state
    port_it->second.gptp_port->set_port_state(new_state);
}

} // namespace gptp
