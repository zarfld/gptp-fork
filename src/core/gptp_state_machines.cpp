/**
 * @file gptp_state_machines.cpp
 * @brief IEEE 802.1AS state machines implementation
 */

#include "../../include/gptp_state_machines.hpp"
#include "../../include/gptp_clock.hpp"
#include "../../include/clock_servo.hpp"
#include <iostream>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

namespace gptp {

    // ============================================================================
    // Base StateMachine Implementation
    // ============================================================================

    void StateMachine::transition_to_state(int new_state) {
        if (new_state != current_state_) {
            std::cout << "[" << name_ << "] State transition: " << current_state_ 
                      << " -> " << new_state << std::endl;
            
            on_state_exit(current_state_);
            int old_state = current_state_;
            current_state_ = new_state;
            on_state_entry(current_state_);
        }
    }

    namespace state_machine {

        // ============================================================================
        // PortSyncStateMachine Implementation
        // ============================================================================

        PortSyncStateMachine::PortSyncStateMachine(GptpPort* port)
            : StateMachine("PortSync")
            , port_(port)
            , sync_receipt_timeout_(std::chrono::milliseconds(1500))  // 1.5 * sync interval
            , last_sync_receipt_time_(std::chrono::nanoseconds::zero())
        {
        }

        void PortSyncStateMachine::initialize() {
            transition_to_state(State::DISCARD);
        }

        void PortSyncStateMachine::tick(std::chrono::nanoseconds current_time) {
            last_tick_time_ = current_time;
            
            switch (current_state_) {
                case State::DISCARD:
                    if (port_state_selection_logic()) {
                        transition_to_state(State::TRANSMIT);
                    }
                    break;
                    
                case State::TRANSMIT:
                    if (!port_state_selection_logic() || 
                        sync_receipt_timeout_time_interval_expired()) {
                        transition_to_state(State::DISCARD);
                    }
                    break;
            }
        }

        void PortSyncStateMachine::process_event(int event_type, const void* event_data) {
            Event event = static_cast<Event>(event_type);
            
            switch (event) {
                case Event::PORT_STATE_CHANGE:
                case Event::MASTER_PORT_CHANGE:
                    // Re-evaluate state based on current conditions
                    if (current_state_ == State::DISCARD && port_state_selection_logic()) {
                        transition_to_state(State::TRANSMIT);
                    } else if (current_state_ == State::TRANSMIT && !port_state_selection_logic()) {
                        transition_to_state(State::DISCARD);
                    }
                    break;
                    
                case Event::SYNC_RECEIPT_TIMEOUT:
                    if (current_state_ == State::TRANSMIT) {
                        transition_to_state(State::DISCARD);
                    }
                    break;
                    
                case Event::ASYMMETRY_MEASUREMENT_MODE_CHANGE:
                    // Handle asymmetry measurement mode changes
                    break;
            }
        }

        bool PortSyncStateMachine::sync_receipt_timeout_time_interval_expired() const {
            if (last_sync_receipt_time_ == std::chrono::nanoseconds::zero()) {
                return false;
            }
            return (last_tick_time_ - last_sync_receipt_time_) > sync_receipt_timeout_;
        }

        bool PortSyncStateMachine::port_state_selection_logic() const {
            // IEEE 802.1AS-2021 clause 10.2.4.2.1
            // Port should transmit sync if it's in SLAVE state and receiving sync
            PortState state = port_->get_port_state();
            return (state == PortState::SLAVE || state == PortState::MASTER);
        }

        void PortSyncStateMachine::on_state_entry(int state) {
            switch (state) {
                case State::DISCARD:
                    std::cout << "[" << name_ << "] Entered DISCARD state" << std::endl;
                    break;
                case State::TRANSMIT:
                    std::cout << "[" << name_ << "] Entered TRANSMIT state" << std::endl;
                    break;
            }
        }

        void PortSyncStateMachine::on_state_exit(int state) {
            // Cleanup when exiting states
        }

        // ============================================================================
        // MDSyncStateMachine Implementation  
        // ============================================================================

        MDSyncStateMachine::MDSyncStateMachine(GptpPort* port)
            : StateMachine("MDSync")
            , port_(port)
            , follow_up_receipt_timeout_(std::chrono::milliseconds(100))
            , last_md_sync_time_(std::chrono::nanoseconds::zero())
            , waiting_for_follow_up_(false)
            , sync_sequence_id_(0)
            , last_sync_sequence_(0)
        {
        }

        void MDSyncStateMachine::initialize() {
            transition_to_state(State::INITIALIZING);
        }

        void MDSyncStateMachine::tick(std::chrono::nanoseconds current_time) {
            last_tick_time_ = current_time;
            
            switch (current_state_) {
                case State::INITIALIZING:
                    if (port_->get_port_state() == PortState::MASTER) {
                        transition_to_state(State::SEND_MD_SYNC);
                    }
                    break;
                    
                case State::SEND_MD_SYNC:
                    // Check if it's time to send another sync
                    if (current_time - last_md_sync_time_ >= std::chrono::milliseconds(125)) {
                        tx_md_sync();
                        last_md_sync_time_ = current_time;
                    }
                    break;
                    
                case State::WAITING_FOR_FOLLOW_UP:
                    // Check for follow-up timeout
                    if (waiting_for_follow_up_ && 
                        current_time - last_md_sync_time_ > follow_up_receipt_timeout_) {
                        std::cout << "[" << name_ << "] Follow-up timeout" << std::endl;
                        waiting_for_follow_up_ = false;
                        transition_to_state(State::SEND_MD_SYNC);
                    }
                    break;
            }
        }

        void MDSyncStateMachine::process_event(int event_type, const void* event_data) {
            Event event = static_cast<Event>(event_type);
            
            switch (event) {
                case Event::PORT_STATE_CHANGE:
                    if (port_->get_port_state() == PortState::MASTER) {
                        transition_to_state(State::SEND_MD_SYNC);
                    } else {
                        transition_to_state(State::INITIALIZING);
                    }
                    break;
                    
                case Event::MD_SYNC_SEND:
                    if (current_state_ == State::SEND_MD_SYNC) {
                        transition_to_state(State::WAITING_FOR_FOLLOW_UP);
                        waiting_for_follow_up_ = true;
                    }
                    break;
                    
                case Event::MD_SYNC_RECEIPT:
                    // Handle sync message receipt
                    break;
                    
                case Event::FOLLOW_UP_RECEIPT:
                    if (current_state_ == State::WAITING_FOR_FOLLOW_UP) {
                        waiting_for_follow_up_ = false;
                        transition_to_state(State::SEND_MD_SYNC);
                    }
                    break;
                    
                case Event::FOLLOW_UP_RECEIPT_TIMEOUT:
                    if (current_state_ == State::WAITING_FOR_FOLLOW_UP) {
                        waiting_for_follow_up_ = false;
                        transition_to_state(State::SEND_MD_SYNC);
                    }
                    break;
            }
        }

        void MDSyncStateMachine::tx_md_sync() {
            std::cout << "[" << name_ << "] Transmitting MD Sync message" << std::endl;
            
            // Create basic sync message structure
            SyncMessage sync_msg;
            sync_msg.header.messageType = static_cast<uint8_t>(protocol::MessageType::SYNC);
            sync_msg.header.transportSpecific = 1; // IEEE 802.1AS
            sync_msg.header.versionPTP = 2;
            sync_msg.header.messageLength = sizeof(SyncMessage);
            sync_msg.header.domainNumber = 0; // gPTP domain 0
            sync_msg.header.flags = 0x0008; // twoStep flag set
            sync_msg.header.correctionField = 0;
            sync_msg.header.sequenceId = ++sync_sequence_id_;
            sync_msg.header.controlField = 0x00; // Sync
            sync_msg.header.logMessageInterval = -3; // 125ms = 2^(-3) seconds
            
            // Set basic port identity (simplified for now)
            // In a complete implementation, this would get actual clock ID and port ID
            std::fill(std::begin(sync_msg.header.sourcePortIdentity.clockIdentity.id), 
                     std::end(sync_msg.header.sourcePortIdentity.clockIdentity.id), static_cast<uint8_t>(0));
            sync_msg.header.sourcePortIdentity.portNumber = 1; // Default port
            
            // Set current timestamp
            auto now = std::chrono::high_resolution_clock::now();
            auto ns_since_epoch = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch());
            
            sync_msg.originTimestamp.set_seconds(ns_since_epoch.count() / 1000000000ULL);
            sync_msg.originTimestamp.nanoseconds = ns_since_epoch.count() % 1000000000ULL;
            
            // Store for follow-up message
            last_sync_timestamp_ = sync_msg.originTimestamp;
            last_sync_sequence_ = sync_msg.header.sequenceId;
            
            // Serialize and send via socket
            std::vector<uint8_t> serialized = serialize_sync_message(sync_msg);
            
            if (socket_) {
                // Create gPTP packet for network transmission
                GptpPacket packet;
                packet.ethernet.destination = protocol::GPTP_MULTICAST_MAC;
                // MAC address would be set from port configuration
                packet.ethernet.etherType = htons(protocol::GPTP_ETHERTYPE);
                packet.payload = serialized;
                
                PacketTimestamp timestamp;
                auto result = socket_->send_packet(packet, timestamp);
                if (result.is_success()) {
                    std::cout << "[" << name_ << "] Sync message transmitted via network" << std::endl;
                } else {
                    std::cout << "[" << name_ << "] Failed to transmit sync message: " << static_cast<int>(result.error()) << std::endl;
                }
            } else {
                std::cout << "[" << name_ << "] Sync message prepared (size: " << serialized.size() << " bytes) - no socket available" << std::endl;
            }
            
            // Schedule follow-up message transmission
            schedule_followup_transmission();
            
            process_event(Event::MD_SYNC_SEND, nullptr);
        }

        void MDSyncStateMachine::set_md_sync_receive() {
            std::cout << "[" << name_ << "] MD Sync received" << std::endl;
            process_event(Event::MD_SYNC_RECEIPT, nullptr);
        }

        void MDSyncStateMachine::on_state_entry(int state) {
            switch (state) {
                case State::INITIALIZING:
                    std::cout << "[" << name_ << "] Entered INITIALIZING state" << std::endl;
                    break;
                case State::SEND_MD_SYNC:
                    std::cout << "[" << name_ << "] Entered SEND_MD_SYNC state" << std::endl;
                    break;
                case State::WAITING_FOR_FOLLOW_UP:
                    std::cout << "[" << name_ << "] Entered WAITING_FOR_FOLLOW_UP state" << std::endl;
                    break;
            }
        }

        void MDSyncStateMachine::on_state_exit(int state) {
            // Cleanup when exiting states
        }

        // ============================================================================
        // LinkDelayStateMachine Implementation
        // ============================================================================

        LinkDelayStateMachine::LinkDelayStateMachine(GptpPort* port)
            : StateMachine("LinkDelay")
            , port_(port)
            , pdelay_req_interval_(std::chrono::seconds(1))  // 1 second default
            , pdelay_resp_receipt_timeout_(std::chrono::milliseconds(100))
            , last_pdelay_req_time_(std::chrono::nanoseconds::zero())
            , link_delay_(std::chrono::nanoseconds::zero())
            , neighbor_rate_ratio_(1.0)  // Initialize to 1.0 (perfect rate match)
            , pdelay_req_sequence_id_(0)
            , path_delay_calc_(gptp::path_delay::PathDelayFactory::create_standard_p2p_calculator(0))
        {
        }

        void LinkDelayStateMachine::initialize() {
            transition_to_state(State::NOT_ENABLED);
        }

        void LinkDelayStateMachine::tick(std::chrono::nanoseconds current_time) {
            last_tick_time_ = current_time;
            
            switch (current_state_) {
                case State::NOT_ENABLED:
                    // Wait for port to be enabled
                    break;
                    
                case State::INITIAL_SEND_PDELAY_REQ:
                    send_pdelay_req();
                    transition_to_state(State::WAITING_FOR_PDELAY_RESP);
                    break;
                    
                case State::RESET:
                    // Reset state - clear any pending operations
                    transition_to_state(State::SEND_PDELAY_REQ);
                    break;
                    
                case State::SEND_PDELAY_REQ:
                    // Check if it's time to send another pdelay request
                    if (current_time - last_pdelay_req_time_ >= pdelay_req_interval_) {
                        send_pdelay_req();
                        transition_to_state(State::WAITING_FOR_PDELAY_RESP);
                    }
                    break;
                    
                case State::WAITING_FOR_PDELAY_RESP:
                    // Check for response timeout
                    if (current_time - last_pdelay_req_time_ > pdelay_resp_receipt_timeout_) {
                        std::cout << "[" << name_ << "] Pdelay response timeout" << std::endl;
                        transition_to_state(State::SEND_PDELAY_REQ);
                    }
                    break;
                    
                case State::WAITING_FOR_PDELAY_RESP_FOLLOW_UP:
                    // Check for follow-up timeout
                    if (current_time - last_pdelay_req_time_ > pdelay_resp_receipt_timeout_) {
                        std::cout << "[" << name_ << "] Pdelay response follow-up timeout" << std::endl;
                        transition_to_state(State::SEND_PDELAY_REQ);
                    }
                    break;
            }
        }

        void LinkDelayStateMachine::process_event(int event_type, const void* event_data) {
            Event event = static_cast<Event>(event_type);
            
            switch (event) {
                case Event::PORT_ENABLED:
                    if (current_state_ == State::NOT_ENABLED) {
                        transition_to_state(State::INITIAL_SEND_PDELAY_REQ);
                    }
                    break;
                    
                case Event::PORT_DISABLED:
                    transition_to_state(State::NOT_ENABLED);
                    break;
                    
                case Event::PDELAY_REQ_INTERVAL_TIMER:
                    if (current_state_ == State::SEND_PDELAY_REQ) {
                        send_pdelay_req();
                        transition_to_state(State::WAITING_FOR_PDELAY_RESP);
                    }
                    break;
                    
                case Event::PDELAY_RESP_RECEIPT:
                    if (current_state_ == State::WAITING_FOR_PDELAY_RESP) {
                        const PdelayRespMessage* resp = static_cast<const PdelayRespMessage*>(event_data);
                        if (resp) {
                            process_pdelay_resp(*resp);
                        }
                        transition_to_state(State::WAITING_FOR_PDELAY_RESP_FOLLOW_UP);
                    }
                    break;
                    
                case Event::PDELAY_RESP_FOLLOW_UP_RECEIPT:
                    if (current_state_ == State::WAITING_FOR_PDELAY_RESP_FOLLOW_UP) {
                        const PdelayRespFollowUpMessage* follow_up = 
                            static_cast<const PdelayRespFollowUpMessage*>(event_data);
                        if (follow_up) {
                            process_pdelay_resp_follow_up(*follow_up);
                        }
                        transition_to_state(State::SEND_PDELAY_REQ);
                    }
                    break;
                    
                case Event::PDELAY_RESP_RECEIPT_TIMEOUT:
                    if (current_state_ == State::WAITING_FOR_PDELAY_RESP ||
                        current_state_ == State::WAITING_FOR_PDELAY_RESP_FOLLOW_UP) {
                        transition_to_state(State::SEND_PDELAY_REQ);
                    }
                    break;
            }
        }

        void LinkDelayStateMachine::send_pdelay_req() {
            std::cout << "[" << name_ << "] Sending Pdelay_Req (seq: " 
                      << pdelay_req_sequence_id_ << ")" << std::endl;
            
            // Record transmission timestamp (T1)
            // In a real implementation, this would be captured from hardware
            auto now = std::chrono::high_resolution_clock::now();
            t1_timestamp_.set_seconds(std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count());
            t1_timestamp_.nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count() % 1000000000;
            
            last_pdelay_req_time_ = last_tick_time_;
            pdelay_req_sequence_id_++;
            
            // Create and send Pdelay_Req message via socket
            if (socket_) {
                // Create Pdelay_Req packet using packet builder
                ClockIdentity clock_id;
                // TODO: Get from port configuration
                std::fill(clock_id.id.begin(), clock_id.id.end(), 0x00);
                
                PortIdentity port_id;
                port_id.clockIdentity = clock_id;
                port_id.portNumber = htons(1);
                
                std::array<uint8_t, 6> mac_addr = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
                // TODO: Get actual MAC from socket
                
                auto packet = GptpPacketBuilder::create_pdelay_req_packet(
                    port_id, pdelay_req_sequence_id_, mac_addr);
                
                PacketTimestamp timestamp;
                auto result = socket_->send_packet(packet, timestamp);
                if (result.is_success()) {
                    // Update T1 with actual transmission timestamp
                    auto tx_ns = timestamp.hardware_timestamp;
                    t1_timestamp_.set_seconds(tx_ns.count() / 1000000000);
                    t1_timestamp_.nanoseconds = tx_ns.count() % 1000000000;
                    
                    std::cout << "[" << name_ << "] Pdelay_Req transmitted via network" << std::endl;
                } else {
                    std::cout << "[" << name_ << "] Failed to transmit Pdelay_Req: " << static_cast<int>(result.error()) << std::endl;
                }
            } else {
                std::cout << "[" << name_ << "] Pdelay_Req prepared - no socket available" << std::endl;
            }
            
            std::cout << "[" << name_ << "] T1 timestamp: " << t1_timestamp_.get_seconds() 
                      << "." << t1_timestamp_.nanoseconds << std::endl;
        }

        void LinkDelayStateMachine::process_pdelay_resp(const PdelayRespMessage& resp) {
            std::cout << "[" << name_ << "] Processing Pdelay_Resp" << std::endl;
            
            // Record reception timestamp (T4)
            // In a real implementation, this would be captured from hardware
            auto now = std::chrono::high_resolution_clock::now();
            t4_timestamp_.set_seconds(std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count());
            t4_timestamp_.nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count() % 1000000000;
            
            // Extract T2 timestamp from response message  
            t2_timestamp_ = resp.requestReceiptTimestamp;
            
            std::cout << "[" << name_ << "] T2 from resp: " << t2_timestamp_.get_seconds()
                      << "." << t2_timestamp_.nanoseconds << std::endl;
            std::cout << "[" << name_ << "] T4 timestamp: " << t4_timestamp_.get_seconds() 
                      << "." << t4_timestamp_.nanoseconds << std::endl;
        }

        void LinkDelayStateMachine::process_pdelay_resp_follow_up(const PdelayRespFollowUpMessage& follow_up) {
            std::cout << "[" << name_ << "] Processing Pdelay_Resp_Follow_Up" << std::endl;
            
            // Create timestamps structure for path delay calculation using IEEE 802.1AS equations
            Timestamp t1 = t1_timestamp_;  // Stored from send_pdelay_req
            Timestamp t2 = t2_timestamp_;  // Stored from process_pdelay_resp
            Timestamp t4 = t4_timestamp_;  // Stored from process_pdelay_resp
            
            // Extract T3 from the follow-up message (precise transmission timestamp)
            Timestamp t3 = follow_up.responseOriginTimestamp;
            
            // Calculate path delay using IEEE 802.1AS-2021 equations
            // Equation 16-2: meanLinkDelay = ((t_req4 - t_req1) * r - (t_rsp3 - t_rsp2)) / 2
            
            auto t1_ns = t1.to_nanoseconds();
            auto t2_ns = t2.to_nanoseconds();
            auto t3_ns = t3.to_nanoseconds();
            auto t4_ns = t4.to_nanoseconds();
            
            // Calculate turnaround time and residence time
            auto initiator_turnaround = t4_ns - t1_ns;
            auto responder_residence = t3_ns - t2_ns;
            
            // Apply neighbor rate ratio (for now use 1.0, will be updated periodically)
            auto corrected_turnaround = std::chrono::nanoseconds(
                static_cast<int64_t>(initiator_turnaround.count() * neighbor_rate_ratio_)
            );
            
            // Calculate mean link delay
            auto calculated_delay = (corrected_turnaround - responder_residence) / 2;
            
            // Ensure non-negative delay
            if (calculated_delay >= std::chrono::nanoseconds::zero()) {
                link_delay_ = calculated_delay;
                
                std::cout << "[" << name_ << "] Path delay calculated: " 
                          << link_delay_.count() << " ns, rate ratio: " 
                          << neighbor_rate_ratio_ << std::endl;
                std::cout << "[" << name_ << "] Turnaround: " << initiator_turnaround.count() 
                          << " ns, Residence: " << responder_residence.count() << " ns" << std::endl;
            } else {
                std::cout << "[" << name_ << "] Negative path delay calculated, using fallback" << std::endl;
                link_delay_ = std::chrono::microseconds(10);  // Fallback value
            }
        }

        std::chrono::nanoseconds LinkDelayStateMachine::calculate_link_delay(
            const Timestamp& t1, const Timestamp& t2, const Timestamp& t3, const Timestamp& t4) {
            
            // Create timestamps structure for the path delay calculator
            gptp::path_delay::PdelayTimestamps timestamps;
            timestamps.t1 = t1;
            timestamps.t2 = t2;
            timestamps.t3 = t3;
            timestamps.t4 = t4;
            timestamps.t2_valid = true;
            timestamps.t3_valid = true;
            timestamps.sequence_id = pdelay_req_sequence_id_;
            
            // Use the full IEEE 802.1AS-2021 path delay calculator
            auto result = path_delay_calc_->calculate_path_delay(timestamps);
            
            if (result.valid) {
                // Update neighbor rate ratio from the calculation
                neighbor_rate_ratio_ = result.neighbor_rate_ratio;
                return result.mean_link_delay;
            } else {
                // Fallback to basic calculation if advanced calculation fails
                auto t1_ns = t1.to_nanoseconds();
                auto t2_ns = t2.to_nanoseconds();
                auto t3_ns = t3.to_nanoseconds();
                auto t4_ns = t4.to_nanoseconds();
                
                auto turnaround_time = t4_ns - t1_ns;
                auto residence_time = t3_ns - t2_ns;
                auto link_delay = (turnaround_time - residence_time) / 2;
                
                return std::chrono::nanoseconds(link_delay.count());
            }
        }

        void LinkDelayStateMachine::on_state_entry(int state) {
            switch (state) {
                case State::NOT_ENABLED:
                    std::cout << "[" << name_ << "] Entered NOT_ENABLED state" << std::endl;
                    break;
                case State::INITIAL_SEND_PDELAY_REQ:
                    std::cout << "[" << name_ << "] Entered INITIAL_SEND_PDELAY_REQ state" << std::endl;
                    break;
                case State::RESET:
                    std::cout << "[" << name_ << "] Entered RESET state" << std::endl;
                    break;
                case State::SEND_PDELAY_REQ:
                    std::cout << "[" << name_ << "] Entered SEND_PDELAY_REQ state" << std::endl;
                    break;
                case State::WAITING_FOR_PDELAY_RESP:
                    std::cout << "[" << name_ << "] Entered WAITING_FOR_PDELAY_RESP state" << std::endl;
                    break;
                case State::WAITING_FOR_PDELAY_RESP_FOLLOW_UP:
                    std::cout << "[" << name_ << "] Entered WAITING_FOR_PDELAY_RESP_FOLLOW_UP state" << std::endl;
                    break;
            }
        }

        void LinkDelayStateMachine::on_state_exit(int state) {
            // Cleanup when exiting states
        }

        // ============================================================================
        // SiteSyncSyncStateMachine Implementation
        // ============================================================================

        SiteSyncSyncStateMachine::SiteSyncSyncStateMachine(GptpPort* port)
            : StateMachine("SiteSyncSync")
            , port_(port)
            , waiting_for_follow_up_(false)
        {
        }

        void SiteSyncSyncStateMachine::initialize() {
            transition_to_state(State::INITIALIZING);
        }

        void SiteSyncSyncStateMachine::tick(std::chrono::nanoseconds current_time) {
            last_tick_time_ = current_time;
            
            switch (current_state_) {
                case State::INITIALIZING:
                    if (port_->get_port_state() == PortState::SLAVE) {
                        transition_to_state(State::RECEIVING_SYNC);
                    }
                    break;
                    
                case State::RECEIVING_SYNC:
                    if (port_->get_port_state() != PortState::SLAVE) {
                        transition_to_state(State::INITIALIZING);
                    }
                    break;
            }
        }

        void SiteSyncSyncStateMachine::process_event(int event_type, const void* event_data) {
            Event event = static_cast<Event>(event_type);
            
            switch (event) {
                case Event::PORT_STATE_CHANGE:
                    if (port_->get_port_state() == PortState::SLAVE) {
                        transition_to_state(State::RECEIVING_SYNC);
                    } else {
                        transition_to_state(State::INITIALIZING);
                    }
                    break;
                    
                case Event::SYNC_RECEIPT:
                    if (current_state_ == State::RECEIVING_SYNC) {
                        const SyncMessage* sync = static_cast<const SyncMessage*>(event_data);
                        if (sync) {
                            // TODO: Get actual receipt timestamp
                            Timestamp receipt_time;
                            process_sync_message(*sync, receipt_time);
                        }
                    }
                    break;
                    
                case Event::FOLLOW_UP_RECEIPT:
                    if (current_state_ == State::RECEIVING_SYNC && waiting_for_follow_up_) {
                        const FollowUpMessage* follow_up = static_cast<const FollowUpMessage*>(event_data);
                        if (follow_up) {
                            process_follow_up_message(*follow_up);
                        }
                    }
                    break;
            }
        }

        void SiteSyncSyncStateMachine::process_sync_message(const SyncMessage& sync, const Timestamp& receipt_time) {
            std::cout << "[" << name_ << "] Processing Sync message (seq: " 
                      << sync.header.sequenceId << ")" << std::endl;
            
            // Store sync message for two-step processing
            pending_sync_ = sync;
            sync_receipt_time_ = receipt_time;
            waiting_for_follow_up_ = true;
            
            // Check if this is a one-step sync (no follow-up expected)
            if ((sync.header.flags & 0x0200) == 0) { // twoStep flag not set
                waiting_for_follow_up_ = false;
                perform_clock_synchronization(sync, receipt_time, sync.originTimestamp);
            }
        }

        void SiteSyncSyncStateMachine::process_follow_up_message(const FollowUpMessage& follow_up) {
            std::cout << "[" << name_ << "] Processing Follow_Up message (seq: " 
                      << follow_up.header.sequenceId << ")" << std::endl;
            
            if (waiting_for_follow_up_ && 
                follow_up.header.sequenceId == pending_sync_.header.sequenceId) {
                
                waiting_for_follow_up_ = false;
                
                // Perform two-step synchronization using precise timestamp from follow-up
                perform_clock_synchronization(pending_sync_, sync_receipt_time_, 
                                            follow_up.preciseOriginTimestamp);
                
                std::cout << "[" << name_ << "] Clock synchronization updated" << std::endl;
            }
        }

        void SiteSyncSyncStateMachine::perform_clock_synchronization(const SyncMessage& sync,
                                                                   const Timestamp& receipt_time,
                                                                   const Timestamp& precise_origin) {
            // Create measurement for clock servo
            servo::SyncMeasurement measurement;
            measurement.master_timestamp = precise_origin;
            measurement.local_receipt_time = receipt_time;
            measurement.correction_field = Timestamp(); // Will be extracted from sync.header.correctionField
            
            // Convert correctionField (nanoseconds << 16) to timestamp
            int64_t correction_ns = sync.header.correctionField >> 16;
            measurement.correction_field.nanoseconds = static_cast<uint32_t>(correction_ns % 1000000000);
            measurement.correction_field.set_seconds(correction_ns / 1000000000);
            
            // Get path delay from port's LinkDelay state machine
            measurement.path_delay = port_->get_link_delay();
            measurement.measurement_time = std::chrono::steady_clock::now();
            
            // Get the clock servo from the port and update it
            if (auto* clock = port_->get_clock()) {
                auto* servo = clock->get_servo();
                if (servo) {
                    auto offset_result = servo->calculate_offset(measurement);
                    if (offset_result.valid) {
                        auto freq_result = servo->update_servo(offset_result.offset, 
                                                              measurement.measurement_time);
                        
                        std::cout << "[" << name_ << "] Sync complete - Offset: " 
                                  << offset_result.offset.count() << " ns, "
                                  << "Freq adj: " << freq_result.frequency_adjustment << " ppb, "
                                  << "Locked: " << (freq_result.locked ? "Yes" : "No") << std::endl;
                        
                        // Apply frequency adjustment to local clock
                        clock->adjust_frequency(freq_result.frequency_adjustment);
                        
                        // Apply phase adjustment if needed
                        if (std::abs(freq_result.phase_adjustment) > 1000.0) { // > 1µs
                            clock->adjust_phase(freq_result.phase_adjustment);
                        }
                    }
                }
            }
        }

        void SiteSyncSyncStateMachine::on_state_entry(int state) {
            switch (state) {
                case State::INITIALIZING:
                    std::cout << "[" << name_ << "] Entered INITIALIZING state" << std::endl;
                    waiting_for_follow_up_ = false;
                    break;
                case State::RECEIVING_SYNC:
                    std::cout << "[" << name_ << "] Entered RECEIVING_SYNC state" << std::endl;
                    break;
            }
        }

        void SiteSyncSyncStateMachine::on_state_exit(int state) {
            // Cleanup when exiting states
        }

    } // namespace state_machine

    // ============================================================================
    // GptpPort Implementation
    // ============================================================================

    GptpPort::GptpPort(uint16_t port_number, GptpClock* clock)
        : port_state_(PortState::INITIALIZING)
        , clock_(clock)
        , enabled_(false)
    {
        port_identity_.portNumber = port_number;
        
        // Set clock identity from clock if available
        if (clock_) {
            port_identity_.clockIdentity = clock_->get_clock_identity();
        } else {
            // Generate temporary clock identity if no clock provided
            std::fill(std::begin(port_identity_.clockIdentity.id), 
                     std::end(port_identity_.clockIdentity.id), 0);
        }
        
        // Create state machines
        port_sync_sm_ = std::make_unique<state_machine::PortSyncStateMachine>(this);
        md_sync_sm_ = std::make_unique<state_machine::MDSyncStateMachine>(this);
        link_delay_sm_ = std::make_unique<state_machine::LinkDelayStateMachine>(this);
        site_sync_sm_ = std::make_unique<state_machine::SiteSyncSyncStateMachine>(this);
    }

    void GptpPort::initialize() {
        std::cout << "[Port " << port_identity_.portNumber << "] Initializing" << std::endl;
        
        port_sync_sm_->initialize();
        md_sync_sm_->initialize();
        link_delay_sm_->initialize();
        site_sync_sm_->initialize();
        
        set_port_state(PortState::LISTENING);
    }

    void GptpPort::tick(std::chrono::nanoseconds current_time) {
        if (!enabled_) return;
        
        port_sync_sm_->tick(current_time);
        md_sync_sm_->tick(current_time);
        link_delay_sm_->tick(current_time);
        site_sync_sm_->tick(current_time);
    }

    void GptpPort::enable() {
        std::cout << "[Port " << port_identity_.portNumber << "] Enabled" << std::endl;
        enabled_ = true;
        
        // Notify state machines
        link_delay_sm_->process_event(state_machine::LinkDelayStateMachine::Event::PORT_ENABLED, nullptr);
    }

    void GptpPort::disable() {
        std::cout << "[Port " << port_identity_.portNumber << "] Disabled" << std::endl;
        enabled_ = false;
        
        // Notify state machines
        link_delay_sm_->process_event(state_machine::LinkDelayStateMachine::Event::PORT_DISABLED, nullptr);
    }

    void GptpPort::set_port_state(PortState state) {
        if (state != port_state_) {
            std::cout << "[Port " << port_identity_.portNumber << "] State change: " 
                      << static_cast<int>(port_state_) << " -> " << static_cast<int>(state) << std::endl;
            
            port_state_ = state;
            
            // Notify all state machines of port state change
            port_sync_sm_->process_event(state_machine::PortSyncStateMachine::Event::PORT_STATE_CHANGE, nullptr);
            md_sync_sm_->process_event(state_machine::MDSyncStateMachine::Event::PORT_STATE_CHANGE, nullptr);
            site_sync_sm_->process_event(state_machine::SiteSyncSyncStateMachine::Event::PORT_STATE_CHANGE, nullptr);
        }
    }

    // Message processing methods
    void GptpPort::process_sync_message(const SyncMessage& sync, const Timestamp& receipt_time) {
        site_sync_sm_->process_event(state_machine::SiteSyncSyncStateMachine::Event::SYNC_RECEIPT, &sync);
    }

    void GptpPort::process_follow_up_message(const FollowUpMessage& follow_up) {
        site_sync_sm_->process_event(state_machine::SiteSyncSyncStateMachine::Event::FOLLOW_UP_RECEIPT, &follow_up);
    }

    void GptpPort::process_pdelay_req_message(const PdelayReqMessage& req, const Timestamp& receipt_time) {
        std::cout << "[Port " << port_identity_.portNumber << "] Processing Pdelay_Req (seq: " 
                  << req.header.sequenceId << ")" << std::endl;
        
        // Create Pdelay_Resp message
        PdelayRespMessage resp;
        resp.header.messageType = static_cast<uint8_t>(protocol::MessageType::PDELAY_RESP);
        resp.header.transportSpecific = 1; // IEEE 802.1AS
        resp.header.versionPTP = 2;
        resp.header.messageLength = sizeof(PdelayRespMessage);
        resp.header.domainNumber = 0; // gPTP domain 0
        resp.header.flags = 0x0008; // twoStep flag set
        resp.header.correctionField = 0;
        resp.header.sequenceId = req.header.sequenceId; // Echo sequence ID
        resp.header.controlField = 0x03; // Pdelay_Resp
        resp.header.logMessageInterval = 0; // Not applicable
        
        // Set source port identity
        resp.header.sourcePortIdentity = port_identity_;
        
        // Set T2 timestamp (request receipt time)
        resp.requestReceiptTimestamp = receipt_time;
        
        // Set requesting port identity (from the request)
        resp.requestingPortIdentity = req.header.sourcePortIdentity;
        
        std::cout << "[Port " << port_identity_.portNumber << "] Sending Pdelay_Resp (T2: " 
                  << receipt_time.get_seconds() << "." << receipt_time.nanoseconds << ")" << std::endl;
        
        // This would be sent via socket layer when available
        // For now, just log the response creation
        
        // In a two-step implementation, a follow-up message would also be sent
        // with the precise transmission timestamp (T3)
    }

    void GptpPort::process_pdelay_resp_message(const PdelayRespMessage& resp, const Timestamp& receipt_time) {
        link_delay_sm_->process_event(state_machine::LinkDelayStateMachine::Event::PDELAY_RESP_RECEIPT, &resp);
    }

    void GptpPort::process_pdelay_resp_follow_up_message(const PdelayRespFollowUpMessage& follow_up) {
        link_delay_sm_->process_event(state_machine::LinkDelayStateMachine::Event::PDELAY_RESP_FOLLOW_UP_RECEIPT, &follow_up);
    }

    void GptpPort::process_announce_message(const AnnounceMessage& announce) {
        std::cout << "[Port " << port_identity_.portNumber << "] Processing Announce message" << std::endl;
        
        // Forward to port manager or BMCA coordinator for best master selection
        // The announce message contains priority vectors and clock quality information
        // needed for IEEE 802.1AS-2021 BMCA algorithm
        
        std::cout << "[Port " << port_identity_.portNumber << "] Announce from GM: "
                  << std::hex << announce.grandmasterIdentity.id[0] << ":"
                  << announce.grandmasterIdentity.id[1] << ":" 
                  << announce.grandmasterIdentity.id[2] << ":" 
                  << announce.grandmasterIdentity.id[3] << ":" 
                  << announce.grandmasterIdentity.id[4] << ":" 
                  << announce.grandmasterIdentity.id[5] << ":" 
                  << announce.grandmasterIdentity.id[6] << ":" 
                  << announce.grandmasterIdentity.id[7] << std::dec << std::endl;
        
        std::cout << "[Port " << port_identity_.portNumber << "] GM Priority1: " 
                  << static_cast<int>(announce.grandmasterPriority1)
                  << ", Priority2: " << static_cast<int>(announce.grandmasterPriority2)
                  << ", Steps: " << announce.stepsRemoved << std::endl;
        
        // This would normally trigger BMCA state machine evaluation
        // and potentially change port role (MASTER/SLAVE/PASSIVE)
    }

    std::chrono::nanoseconds GptpPort::get_link_delay() const {
        return link_delay_sm_->get_link_delay();
    }

    // ============================================================================
    // MDSyncStateMachine Helper Methods
    // ============================================================================

    std::vector<uint8_t> state_machine::MDSyncStateMachine::serialize_sync_message(const SyncMessage& sync_msg) {
        std::vector<uint8_t> buffer(sizeof(SyncMessage));
        std::memcpy(buffer.data(), &sync_msg, sizeof(SyncMessage));
        return buffer;
    }

    std::vector<uint8_t> state_machine::MDSyncStateMachine::serialize_followup_message(const FollowUpMessage& followup_msg) {
        std::vector<uint8_t> buffer(sizeof(FollowUpMessage));
        std::memcpy(buffer.data(), &followup_msg, sizeof(FollowUpMessage));
        return buffer;
    }

    void state_machine::MDSyncStateMachine::schedule_followup_transmission() {
        // Create follow-up message with precise timestamp
        FollowUpMessage followup_msg;
        followup_msg.header.messageType = static_cast<uint8_t>(protocol::MessageType::FOLLOW_UP);
        followup_msg.header.transportSpecific = 1; // IEEE 802.1AS
        followup_msg.header.versionPTP = 2;
        followup_msg.header.messageLength = sizeof(FollowUpMessage);
        followup_msg.header.domainNumber = 0; // gPTP domain 0
        followup_msg.header.flags = 0x0000; // No flags for follow-up
        followup_msg.header.correctionField = 0;
        followup_msg.header.sequenceId = last_sync_sequence_;
        followup_msg.header.controlField = 0x02; // Follow_Up
        followup_msg.header.logMessageInterval = -3; // Same as sync

        // Set basic port identity (simplified)
        std::fill(std::begin(followup_msg.header.sourcePortIdentity.clockIdentity.id), 
                 std::end(followup_msg.header.sourcePortIdentity.clockIdentity.id), static_cast<uint8_t>(0));
        followup_msg.header.sourcePortIdentity.portNumber = 1; // Default port

        // Set precise origin timestamp from sync message
        followup_msg.preciseOriginTimestamp = last_sync_timestamp_;

        // Serialize follow-up message
        std::vector<uint8_t> serialized = serialize_followup_message(followup_msg);
        std::cout << "[" << name_ << "] Follow-up message prepared for sequence " 
                  << last_sync_sequence_ << " (size: " << serialized.size() << " bytes)" << std::endl;
    }

} // namespace gptp
