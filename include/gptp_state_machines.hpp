/**
 * @file gptp_state_machines.hpp
 * @brief IEEE 802.1AS state machines implementation
 * 
 * This file implements the core state machines required by IEEE 802.1AS-2021:
 * - PortSync state machine (clause 10.2.4)
 * - MDSync state machine (clause 10.2.15) 
 * - LinkDelay state machine (clause 11.2.13)
 * - SiteSyncSync state machine (clause 10.2.8)
 */

#pragma once

#include "gptp_protocol.hpp"
#include <chrono>
#include <memory>
#include <string>

namespace gptp {

    // Forward declarations
    class GptpPort;
    class GptpClock;

    /**
     * @brief Base class for all IEEE 802.1AS state machines
     */
    class StateMachine {
    public:
        StateMachine(const std::string& name) : name_(name), current_state_(0) {}
        virtual ~StateMachine() = default;
        
        virtual void initialize() = 0;
        virtual void tick(std::chrono::nanoseconds current_time) = 0;
        virtual void process_event(int event_type, const void* event_data = nullptr) = 0;
        
        const std::string& name() const { return name_; }
        int current_state() const { return current_state_; }
        
    protected:
        void transition_to_state(int new_state);
        virtual void on_state_entry(int /* state */) {}
        virtual void on_state_exit(int /* state */) {}
        
        std::string name_;
        int current_state_;
        std::chrono::nanoseconds last_tick_time_{0};
    };

    namespace state_machine {
        
        /**
         * @brief PortSync State Machine (IEEE 802.1AS-2021 clause 10.2.4)
         * Manages synchronization state of a port
         */
        class PortSyncStateMachine : public StateMachine {
        public:
            // States from IEEE 802.1AS-2021 Figure 10-2
            enum State {
                DISCARD = 0,
                TRANSMIT = 1
            };
            
            // Events
            enum Event {
                PORT_STATE_CHANGE = 0,
                MASTER_PORT_CHANGE = 1,
                SYNC_RECEIPT_TIMEOUT = 2,
                ASYMMETRY_MEASUREMENT_MODE_CHANGE = 3
            };
            
            explicit PortSyncStateMachine(GptpPort* port);
            
            void initialize() override;
            void tick(std::chrono::nanoseconds current_time) override;
            void process_event(int event_type, const void* event_data = nullptr) override;
            
            // IEEE 802.1AS variables
            bool sync_receipt_timeout_time_interval_expired() const;
            bool port_state_selection_logic() const;
            
        private:
            void on_state_entry(int state) override;
            void on_state_exit(int state) override;
            
            GptpPort* port_;
            std::chrono::nanoseconds sync_receipt_timeout_;
            std::chrono::nanoseconds last_sync_receipt_time_;
        };

        /**
         * @brief MDSync State Machine (IEEE 802.1AS-2021 clause 10.2.15)
         * Manages MD (Media Dependent) synchronization
         */
        class MDSyncStateMachine : public StateMachine {
        public:
            // States from IEEE 802.1AS-2021 Figure 10-11
            enum State {
                INITIALIZING = 0,
                SEND_MD_SYNC = 1,
                WAITING_FOR_FOLLOW_UP = 2
            };
            
            // Events
            enum Event {
                PORT_STATE_CHANGE = 0,
                MD_SYNC_SEND = 1,
                MD_SYNC_RECEIPT = 2,
                FOLLOW_UP_RECEIPT = 3,
                FOLLOW_UP_RECEIPT_TIMEOUT = 4
            };
            
            explicit MDSyncStateMachine(GptpPort* port);
            
            void initialize() override;
            void tick(std::chrono::nanoseconds current_time) override;
            void process_event(int event_type, const void* event_data = nullptr) override;
            
            // State machine functions from IEEE 802.1AS
            void tx_md_sync();
            void set_md_sync_receive();
            
        private:
            void on_state_entry(int state) override;
            void on_state_exit(int state) override;
            
            GptpPort* port_;
            std::chrono::nanoseconds follow_up_receipt_timeout_;
            std::chrono::nanoseconds last_md_sync_time_;
            bool waiting_for_follow_up_;
        };

        /**
         * @brief LinkDelay State Machine (IEEE 802.1AS-2021 clause 11.2.13)
         * Manages peer-to-peer delay measurement
         */
        class LinkDelayStateMachine : public StateMachine {
        public:
            // States from IEEE 802.1AS-2021 Figure 11-8
            enum State {
                NOT_ENABLED = 0,
                INITIAL_SEND_PDELAY_REQ = 1,
                RESET = 2,
                SEND_PDELAY_REQ = 3,
                WAITING_FOR_PDELAY_RESP = 4,
                WAITING_FOR_PDELAY_RESP_FOLLOW_UP = 5
            };
            
            // Events
            enum Event {
                PORT_ENABLED = 0,
                PORT_DISABLED = 1,
                PDELAY_REQ_INTERVAL_TIMER = 2,
                PDELAY_RESP_RECEIPT = 3,
                PDELAY_RESP_FOLLOW_UP_RECEIPT = 4,
                PDELAY_RESP_RECEIPT_TIMEOUT = 5
            };
            
            explicit LinkDelayStateMachine(GptpPort* port);
            
            void initialize() override;
            void tick(std::chrono::nanoseconds current_time) override;
            void process_event(int event_type, const void* event_data = nullptr) override;
            
            // Link delay calculation
            std::chrono::nanoseconds calculate_link_delay(
                const Timestamp& t1,  // Pdelay_Req transmission time
                const Timestamp& t2,  // Pdelay_Req reception time  
                const Timestamp& t3,  // Pdelay_Resp transmission time
                const Timestamp& t4   // Pdelay_Resp reception time
            );
            
            std::chrono::nanoseconds get_link_delay() const { return link_delay_; }
            
        private:
            void on_state_entry(int state) override;
            void on_state_exit(int state) override;
            
            void send_pdelay_req();
            void process_pdelay_resp(const PdelayRespMessage& resp);
            void process_pdelay_resp_follow_up(const PdelayRespFollowUpMessage& follow_up);
            
            GptpPort* port_;
            std::chrono::nanoseconds pdelay_req_interval_;
            std::chrono::nanoseconds pdelay_resp_receipt_timeout_;
            std::chrono::nanoseconds last_pdelay_req_time_;
            std::chrono::nanoseconds link_delay_;
            
            // Temporary storage for delay calculation
            Timestamp t1_timestamp_;  // Pdelay_Req TX time
            Timestamp t4_timestamp_;  // Pdelay_Resp RX time
            uint16_t pdelay_req_sequence_id_;
        };

        /**
         * @brief SiteSyncSync State Machine (IEEE 802.1AS-2021 clause 10.2.8)
         * Manages site-level synchronization
         */
        class SiteSyncSyncStateMachine : public StateMachine {
        public:
            // States from IEEE 802.1AS-2021 Figure 10-4
            enum State {
                INITIALIZING = 0,
                RECEIVING_SYNC = 1
            };
            
            // Events
            enum Event {
                PORT_STATE_CHANGE = 0,
                SYNC_RECEIPT = 1,
                FOLLOW_UP_RECEIPT = 2
            };
            
            explicit SiteSyncSyncStateMachine(GptpPort* port);
            
            void initialize() override;
            void tick(std::chrono::nanoseconds current_time) override;
            void process_event(int event_type, const void* event_data = nullptr) override;
            
            // Synchronization processing
            void process_sync_message(const SyncMessage& sync, const Timestamp& receipt_time);
            void process_follow_up_message(const FollowUpMessage& follow_up);
            
        private:
            void on_state_entry(int state) override;
            void on_state_exit(int state) override;
            
            GptpPort* port_;
            
            // Temporary storage for two-step sync processing
            SyncMessage pending_sync_;
            Timestamp sync_receipt_time_;
            bool waiting_for_follow_up_;
        };

    } // namespace state_machine

    /**
     * @brief IEEE 802.1AS Port implementation
     * Manages all state machines for a single gPTP port
     */
    class GptpPort {
    public:
        explicit GptpPort(uint16_t port_number, GptpClock* clock);
        ~GptpPort() = default;
        
        // Port management
        void initialize();
        void tick(std::chrono::nanoseconds current_time);
        void enable();
        void disable();
        
        // Message processing
        void process_sync_message(const SyncMessage& sync, const Timestamp& receipt_time);
        void process_follow_up_message(const FollowUpMessage& follow_up);
        void process_pdelay_req_message(const PdelayReqMessage& req, const Timestamp& receipt_time);
        void process_pdelay_resp_message(const PdelayRespMessage& resp, const Timestamp& receipt_time);
        void process_pdelay_resp_follow_up_message(const PdelayRespFollowUpMessage& follow_up);
        void process_announce_message(const AnnounceMessage& announce);
        
        // Port state
        PortState get_port_state() const { return port_state_; }
        void set_port_state(PortState state);
        
        // Port identity
        const PortIdentity& get_port_identity() const { return port_identity_; }
        uint16_t get_port_number() const { return port_identity_.portNumber; }
        
        // Link delay
        std::chrono::nanoseconds get_link_delay() const;
        
        // Clock access
        GptpClock* get_clock() const { return clock_; }
        
        // State machine access (for testing/debugging)
        state_machine::PortSyncStateMachine* get_port_sync_sm() const { return port_sync_sm_.get(); }
        state_machine::MDSyncStateMachine* get_md_sync_sm() const { return md_sync_sm_.get(); }
        state_machine::LinkDelayStateMachine* get_link_delay_sm() const { return link_delay_sm_.get(); }
        state_machine::SiteSyncSyncStateMachine* get_site_sync_sm() const { return site_sync_sm_.get(); }
        
    private:
        PortIdentity port_identity_;
        PortState port_state_;
        GptpClock* clock_;
        
        // State machines
        std::unique_ptr<state_machine::PortSyncStateMachine> port_sync_sm_;
        std::unique_ptr<state_machine::MDSyncStateMachine> md_sync_sm_;
        std::unique_ptr<state_machine::LinkDelayStateMachine> link_delay_sm_;
        std::unique_ptr<state_machine::SiteSyncSyncStateMachine> site_sync_sm_;
        
        bool enabled_;
    };

    /**
     * @brief Event types for state machine communication
     */
    struct StateEvent {
        int type;
        std::chrono::nanoseconds timestamp;
        const void* data;
        
        StateEvent(int t, std::chrono::nanoseconds ts, const void* d = nullptr)
            : type(t), timestamp(ts), data(d) {}
    };

} // namespace gptp
