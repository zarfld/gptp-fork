/**
 * @file gptp_state_machines.cpp
 * @brief IEEE 802.1AS state machines implementation
 */

#include "../../include/gptp_state_machines.hpp"
#include "../../include/gptp_clock.hpp"
#include <iostream>

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
            // TODO: Implement actual sync message transmission
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
            , pdelay_req_sequence_id_(0)
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
            t1_timestamp_ = Timestamp();  // TODO: Get actual hardware timestamp
            last_pdelay_req_time_ = last_tick_time_;
            pdelay_req_sequence_id_++;
            
            // TODO: Implement actual message transmission
        }

        void LinkDelayStateMachine::process_pdelay_resp(const PdelayRespMessage& resp) {
            std::cout << "[" << name_ << "] Processing Pdelay_Resp" << std::endl;
            
            // Record reception timestamp (T4)
            t4_timestamp_ = Timestamp();  // TODO: Get actual hardware timestamp
            
            // TODO: Store T2 timestamp from response message
        }

        void LinkDelayStateMachine::process_pdelay_resp_follow_up(const PdelayRespFollowUpMessage& follow_up) {
            std::cout << "[" << name_ << "] Processing Pdelay_Resp_Follow_Up" << std::endl;
            
            // TODO: Get T3 timestamp from follow-up message and calculate link delay
            // For now, set a placeholder value
            link_delay_ = std::chrono::microseconds(10);  // 10µs placeholder
        }

        std::chrono::nanoseconds LinkDelayStateMachine::calculate_link_delay(
            const Timestamp& t1, const Timestamp& t2, const Timestamp& t3, const Timestamp& t4) {
            
            // IEEE 802.1AS-2021 link delay calculation
            // linkDelay = ((t4 - t1) - (t3 - t2)) / 2
            
            auto t1_ns = t1.to_nanoseconds();
            auto t2_ns = t2.to_nanoseconds();
            auto t3_ns = t3.to_nanoseconds();
            auto t4_ns = t4.to_nanoseconds();
            
            auto turnaround_time = t4_ns - t1_ns;
            auto residence_time = t3_ns - t2_ns;
            auto link_delay = (turnaround_time - residence_time) / 2;
            
            return std::chrono::nanoseconds(link_delay.count());
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
            
            // TODO: Implement clock synchronization logic
        }

        void SiteSyncSyncStateMachine::process_follow_up_message(const FollowUpMessage& follow_up) {
            std::cout << "[" << name_ << "] Processing Follow_Up message (seq: " 
                      << follow_up.header.sequenceId << ")" << std::endl;
            
            if (waiting_for_follow_up_ && 
                follow_up.header.sequenceId == pending_sync_.header.sequenceId) {
                
                waiting_for_follow_up_ = false;
                
                // TODO: Perform time synchronization calculation
                // Use follow_up.preciseOriginTimestamp and sync_receipt_time_
                
                std::cout << "[" << name_ << "] Clock synchronization updated" << std::endl;
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
        // TODO: Set clock identity from clock
        
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
        std::cout << "[Port " << port_identity_.portNumber << "] Processing Pdelay_Req" << std::endl;
        // TODO: Send Pdelay_Resp
    }

    void GptpPort::process_pdelay_resp_message(const PdelayRespMessage& resp, const Timestamp& receipt_time) {
        link_delay_sm_->process_event(state_machine::LinkDelayStateMachine::Event::PDELAY_RESP_RECEIPT, &resp);
    }

    void GptpPort::process_pdelay_resp_follow_up_message(const PdelayRespFollowUpMessage& follow_up) {
        link_delay_sm_->process_event(state_machine::LinkDelayStateMachine::Event::PDELAY_RESP_FOLLOW_UP_RECEIPT, &follow_up);
    }

    void GptpPort::process_announce_message(const AnnounceMessage& announce) {
        std::cout << "[Port " << port_identity_.portNumber << "] Processing Announce message" << std::endl;
        // TODO: Implement BMCA processing
    }

    std::chrono::nanoseconds GptpPort::get_link_delay() const {
        return link_delay_sm_->get_link_delay();
    }

} // namespace gptp
