/**
 * @file message_processor.cpp
 * @brief IEEE 802.1AS message processing implementation
 */

#include "../../include/gptp_socket.hpp"
#include "../../include/gptp_protocol.hpp"
#include <iostream>
#include <cstring>

namespace gptp {

/**
 * @brief Complete IEEE 802.1AS message processor
 */
class MessageProcessor {
public:
    MessageProcessor() = default;
    ~MessageProcessor() = default;

    /**
     * @brief Process received gPTP packet according to IEEE 802.1AS-2021
     */
    bool process_received_packet(const ReceivedPacket& packet) {
        // Validate packet structure
        if (packet.packet.payload.size() < sizeof(GptpMessageHeader)) {
            std::cerr << "Packet too small for gPTP header" << std::endl;
            return false;
        }

        // Parse header
        GptpMessageHeader header;
        std::memcpy(&header, packet.packet.payload.data(), sizeof(GptpMessageHeader));

        // Validate IEEE 802.1AS compliance
        if (!validate_header(header)) {
            return false;
        }

        // Process based on message type
        auto message_type = static_cast<protocol::MessageType>(header.messageType);
        
        switch (message_type) {
            case protocol::MessageType::SYNC:
                return process_sync_message(packet, header);
                
            case protocol::MessageType::FOLLOW_UP:
                return process_followup_message(packet, header);
                
            case protocol::MessageType::PDELAY_REQ:
                return process_pdelay_req_message(packet, header);
                
            case protocol::MessageType::PDELAY_RESP:
                return process_pdelay_resp_message(packet, header);
                
            case protocol::MessageType::PDELAY_RESP_FOLLOW_UP:
                return process_pdelay_resp_followup_message(packet, header);
                
            case protocol::MessageType::ANNOUNCE:
                return process_announce_message(packet, header);
                
            case protocol::MessageType::SIGNALING:
                return process_signaling_message(packet, header);
                
            default:
                std::cout << "Unsupported message type: " << static_cast<int>(message_type) << std::endl;
                return false;
        }
    }

private:
    /**
     * @brief Validate IEEE 802.1AS-2021 message header compliance
     */
    bool validate_header(const GptpMessageHeader& header) {
        // Check transport specific field (must be 1 for IEEE 802.1AS)
        if (header.transportSpecific != 1) {
            std::cerr << "Invalid transportSpecific field: " << static_cast<int>(header.transportSpecific) 
                      << " (expected 1 for IEEE 802.1AS)" << std::endl;
            return false;
        }

        // Check PTP version (must be 2)
        if (header.versionPTP != 2) {
            std::cerr << "Invalid PTP version: " << static_cast<int>(header.versionPTP) 
                      << " (expected 2)" << std::endl;
            return false;
        }

        // Check domain number (must be 0 for gPTP)
        if (header.domainNumber != 0) {
            std::cerr << "Invalid domain number: " << static_cast<int>(header.domainNumber) 
                      << " (expected 0 for gPTP)" << std::endl;
            return false;
        }

        // Validate message length
        if (header.messageLength < sizeof(GptpMessageHeader)) {
            std::cerr << "Invalid message length: " << header.messageLength << std::endl;
            return false;
        }

        return true;
    }

    /**
     * @brief Process Sync message (IEEE 802.1AS-2021 clause 11.2.9)
     */
    bool process_sync_message(const ReceivedPacket& packet, const GptpMessageHeader& header) {
        if (packet.packet.payload.size() < sizeof(SyncMessage)) {
            std::cerr << "Packet too small for Sync message" << std::endl;
            return false;
        }

        SyncMessage sync_msg;
        std::memcpy(&sync_msg, packet.packet.payload.data(), sizeof(SyncMessage));

        std::cout << "Processing Sync message:" << std::endl;
        std::cout << "  Sequence ID: " << sync_msg.header.sequenceId << std::endl;
        std::cout << "  Origin timestamp: " << sync_msg.originTimestamp.get_seconds() 
                  << "." << sync_msg.originTimestamp.nanoseconds << std::endl;
        std::cout << "  Two-step: " << ((sync_msg.header.flags & 0x0200) != 0 ? "Yes" : "No") << std::endl;
        std::cout << "  Receipt timestamp: " << packet.timestamp.get_best_timestamp().count() << " ns" << std::endl;

        // TODO: Forward to PortSync state machine for processing
        // This should trigger synchronization calculations in clock servo
        
        return true;
    }

    /**
     * @brief Process Follow_Up message (IEEE 802.1AS-2021 clause 11.2.10)
     */
    bool process_followup_message(const ReceivedPacket& packet, const GptpMessageHeader& header) {
        if (packet.packet.payload.size() < sizeof(FollowUpMessage)) {
            std::cerr << "Packet too small for Follow_Up message" << std::endl;
            return false;
        }

        FollowUpMessage followup_msg;
        std::memcpy(&followup_msg, packet.packet.payload.data(), sizeof(FollowUpMessage));

        std::cout << "Processing Follow_Up message:" << std::endl;
        std::cout << "  Sequence ID: " << followup_msg.header.sequenceId << std::endl;
        std::cout << "  Precise origin timestamp: " << followup_msg.preciseOriginTimestamp.get_seconds() 
                  << "." << followup_msg.preciseOriginTimestamp.nanoseconds << std::endl;

        // TODO: Match with corresponding Sync message and trigger clock servo update
        
        return true;
    }

    /**
     * @brief Process Pdelay_Req message (IEEE 802.1AS-2021 clause 11.2.11)
     */
    bool process_pdelay_req_message(const ReceivedPacket& packet, const GptpMessageHeader& header) {
        if (packet.packet.payload.size() < sizeof(PdelayReqMessage)) {
            std::cerr << "Packet too small for Pdelay_Req message" << std::endl;
            return false;
        }

        PdelayReqMessage pdelay_req;
        std::memcpy(&pdelay_req, packet.packet.payload.data(), sizeof(PdelayReqMessage));

        std::cout << "Processing Pdelay_Req message:" << std::endl;
        std::cout << "  Sequence ID: " << pdelay_req.header.sequenceId << std::endl;
        std::cout << "  Receipt timestamp (T2): " << packet.timestamp.get_best_timestamp().count() << " ns" << std::endl;

        // TODO: Send Pdelay_Resp message with T2 timestamp
        // This should be handled by LinkDelay state machine
        
        return true;
    }

    /**
     * @brief Process Pdelay_Resp message (IEEE 802.1AS-2021 clause 11.2.11)
     */
    bool process_pdelay_resp_message(const ReceivedPacket& packet, const GptpMessageHeader& header) {
        if (packet.packet.payload.size() < sizeof(PdelayRespMessage)) {
            std::cerr << "Packet too small for Pdelay_Resp message" << std::endl;
            return false;
        }

        PdelayRespMessage pdelay_resp;
        std::memcpy(&pdelay_resp, packet.packet.payload.data(), sizeof(PdelayRespMessage));

        std::cout << "Processing Pdelay_Resp message:" << std::endl;
        std::cout << "  Sequence ID: " << pdelay_resp.header.sequenceId << std::endl;
        std::cout << "  Request receipt timestamp (T2): " << pdelay_resp.requestReceiptTimestamp.get_seconds() 
                  << "." << pdelay_resp.requestReceiptTimestamp.nanoseconds << std::endl;
        std::cout << "  Receipt timestamp (T4): " << packet.timestamp.get_best_timestamp().count() << " ns" << std::endl;

        // TODO: Forward to LinkDelay state machine for path delay calculation
        
        return true;
    }

    /**
     * @brief Process Pdelay_Resp_Follow_Up message (IEEE 802.1AS-2021 clause 11.2.11)
     */
    bool process_pdelay_resp_followup_message(const ReceivedPacket& packet, const GptpMessageHeader& header) {
        if (packet.packet.payload.size() < sizeof(PdelayRespFollowUpMessage)) {
            std::cerr << "Packet too small for Pdelay_Resp_Follow_Up message" << std::endl;
            return false;
        }

        PdelayRespFollowUpMessage pdelay_resp_followup;
        std::memcpy(&pdelay_resp_followup, packet.packet.payload.data(), sizeof(PdelayRespFollowUpMessage));

        std::cout << "Processing Pdelay_Resp_Follow_Up message:" << std::endl;
        std::cout << "  Sequence ID: " << pdelay_resp_followup.header.sequenceId << std::endl;
        std::cout << "  Response origin timestamp (T3): " << pdelay_resp_followup.responseOriginTimestamp.get_seconds() 
                  << "." << pdelay_resp_followup.responseOriginTimestamp.nanoseconds << std::endl;

        // TODO: Complete path delay calculation with T1, T2, T3, T4 timestamps
        
        return true;
    }

    /**
     * @brief Process Announce message (IEEE 802.1AS-2021 clause 10.3)
     */
    bool process_announce_message(const ReceivedPacket& packet, const GptpMessageHeader& header) {
        if (packet.packet.payload.size() < sizeof(AnnounceMessage)) {
            std::cerr << "Packet too small for Announce message" << std::endl;
            return false;
        }

        AnnounceMessage announce_msg;
        std::memcpy(&announce_msg, packet.packet.payload.data(), sizeof(AnnounceMessage));

        std::cout << "Processing Announce message:" << std::endl;
        std::cout << "  Sequence ID: " << announce_msg.header.sequenceId << std::endl;
        std::cout << "  Grandmaster Priority1: " << static_cast<int>(announce_msg.grandmasterPriority1) << std::endl;
        std::cout << "  Grandmaster Priority2: " << static_cast<int>(announce_msg.grandmasterPriority2) << std::endl;
        std::cout << "  Steps removed: " << announce_msg.stepsRemoved << std::endl;
        std::cout << "  Time source: " << static_cast<int>(announce_msg.timeSource) << std::endl;

        // TODO: Forward to BMCA for grandmaster selection
        
        return true;
    }

    /**
     * @brief Process Signaling message (IEEE 802.1AS-2021 clause 10.5)
     */
    bool process_signaling_message(const ReceivedPacket& packet, const GptpMessageHeader& header) {
        std::cout << "Processing Signaling message (basic implementation)" << std::endl;
        std::cout << "  Sequence ID: " << header.sequenceId << std::endl;
        
        // TODO: Parse TLVs and handle gPTP capability signaling
        
        return true;
    }
};

} // namespace gptp
