/**
 * @file packet_builder.cpp
 * @brief Implementation of GptpPacketBuilder for creating IEEE 802.1AS packets
 */

#include "../../include/gptp_socket.hpp"
#include "../../include/gptp_protocol.hpp"
#include <chrono>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

namespace gptp {

GptpPacket GptpPacketBuilder::create_sync_packet(const PortIdentity& source_port_identity,
                                                uint16_t sequence_id,
                                                const std::array<uint8_t, 6>& source_mac) {
    GptpPacket packet;
    
    // Set Ethernet header
    packet.ethernet.destination = protocol::GPTP_MULTICAST_MAC;
    packet.ethernet.source = source_mac;
    packet.ethernet.etherType = htons(protocol::GPTP_ETHERTYPE);
    
    // Create Sync message
    SyncMessage sync_msg;
    
    // Set gPTP message header
    sync_msg.header.messageType = static_cast<uint8_t>(protocol::MessageType::SYNC);
    sync_msg.header.transportSpecific = 1; // IEEE 802.1AS
    sync_msg.header.versionPTP = 2;
    sync_msg.header.messageLength = htons(static_cast<uint16_t>(sizeof(SyncMessage)));
    sync_msg.header.domainNumber = protocol::DEFAULT_DOMAIN;
    sync_msg.header.flags = htons(0x0200); // Two-step flag
    sync_msg.header.correctionField = 0;
    sync_msg.header.sourcePortIdentity = source_port_identity;
    sync_msg.header.sequenceId = htons(sequence_id);
    sync_msg.header.controlField = 0x00; // Sync
    sync_msg.header.logMessageInterval = protocol::LOG_SYNC_INTERVAL_125MS; // -3 (125ms)
    
    // Set current timestamp (origin timestamp will be updated with precise timing)
    set_current_timestamp(sync_msg.originTimestamp);
    
    // Serialize message to packet payload
    packet.payload.resize(sizeof(SyncMessage));
    std::memcpy(packet.payload.data(), &sync_msg, sizeof(SyncMessage));
    
    return packet;
}

GptpPacket GptpPacketBuilder::create_followup_packet(const PortIdentity& source_port_identity,
                                                     uint16_t sequence_id,
                                                     const Timestamp& precise_origin_timestamp,
                                                     const std::array<uint8_t, 6>& source_mac) {
    GptpPacket packet;
    
    // Set Ethernet header
    packet.ethernet.destination = protocol::GPTP_MULTICAST_MAC;
    packet.ethernet.source = source_mac;
    packet.ethernet.etherType = htons(protocol::GPTP_ETHERTYPE);
    
    // Create Follow_Up message
    FollowUpMessage follow_up_msg;
    
    // Set gPTP message header
    follow_up_msg.header.messageType = static_cast<uint8_t>(protocol::MessageType::FOLLOW_UP);
    follow_up_msg.header.transportSpecific = 1; // IEEE 802.1AS
    follow_up_msg.header.versionPTP = 2;
    follow_up_msg.header.messageLength = htons(static_cast<uint16_t>(sizeof(FollowUpMessage)));
    follow_up_msg.header.domainNumber = protocol::DEFAULT_DOMAIN;
    follow_up_msg.header.flags = 0;
    follow_up_msg.header.correctionField = 0;
    follow_up_msg.header.sourcePortIdentity = source_port_identity;
    follow_up_msg.header.sequenceId = htons(sequence_id);
    follow_up_msg.header.controlField = 0x02; // Follow_Up
    follow_up_msg.header.logMessageInterval = protocol::LOG_SYNC_INTERVAL_125MS; // -3 (125ms)
    
    // Set precise origin timestamp from hardware timestamping
    follow_up_msg.preciseOriginTimestamp = precise_origin_timestamp;
    
    // Serialize message to packet payload
    packet.payload.resize(sizeof(FollowUpMessage));
    std::memcpy(packet.payload.data(), &follow_up_msg, sizeof(FollowUpMessage));
    
    return packet;
}

GptpPacket GptpPacketBuilder::create_pdelay_req_packet(const PortIdentity& source_port_identity,
                                                      uint16_t sequence_id,
                                                      const std::array<uint8_t, 6>& source_mac) {
    GptpPacket packet;
    
    // Set Ethernet header
    packet.ethernet.destination = protocol::GPTP_MULTICAST_MAC;
    packet.ethernet.source = source_mac;
    packet.ethernet.etherType = htons(protocol::GPTP_ETHERTYPE);
    
    // Create Pdelay_Req message
    PdelayReqMessage pdelay_req_msg;
    
    // Set gPTP message header
    pdelay_req_msg.header.messageType = static_cast<uint8_t>(protocol::MessageType::PDELAY_REQ);
    pdelay_req_msg.header.transportSpecific = 1; // IEEE 802.1AS
    pdelay_req_msg.header.versionPTP = 2;
    pdelay_req_msg.header.messageLength = htons(static_cast<uint16_t>(sizeof(PdelayReqMessage)));
    pdelay_req_msg.header.domainNumber = protocol::DEFAULT_DOMAIN;
    pdelay_req_msg.header.flags = 0;
    pdelay_req_msg.header.correctionField = 0;
    pdelay_req_msg.header.sourcePortIdentity = source_port_identity;
    pdelay_req_msg.header.sequenceId = htons(sequence_id);
    pdelay_req_msg.header.controlField = 0x05; // Other
    pdelay_req_msg.header.logMessageInterval = protocol::LOG_PDELAY_INTERVAL_1S; // 0 (1000ms)
    
    // Set origin timestamp (will be updated with precise timing)
    set_current_timestamp(pdelay_req_msg.originTimestamp);
    
    // Initialize reserved fields
    std::fill(std::begin(pdelay_req_msg.reserved), std::end(pdelay_req_msg.reserved), static_cast<uint8_t>(0));
    
    // Serialize message to packet payload
    packet.payload.resize(sizeof(PdelayReqMessage));
    std::memcpy(packet.payload.data(), &pdelay_req_msg, sizeof(PdelayReqMessage));
    
    return packet;
}

GptpPacket GptpPacketBuilder::create_pdelay_resp_packet(const PortIdentity& source_port_identity,
                                                       uint16_t sequence_id,
                                                       const Timestamp& request_receipt_timestamp,
                                                       const PortIdentity& requesting_port_identity,
                                                       const std::array<uint8_t, 6>& source_mac) {
    GptpPacket packet;
    
    // Set Ethernet header
    packet.ethernet.destination = protocol::GPTP_MULTICAST_MAC;
    packet.ethernet.source = source_mac;
    packet.ethernet.etherType = htons(protocol::GPTP_ETHERTYPE);
    
    // Create Pdelay_Resp message
    PdelayRespMessage pdelay_resp_msg;
    
    // Set gPTP message header
    pdelay_resp_msg.header.messageType = static_cast<uint8_t>(protocol::MessageType::PDELAY_RESP);
    pdelay_resp_msg.header.transportSpecific = 1; // IEEE 802.1AS
    pdelay_resp_msg.header.versionPTP = 2;
    pdelay_resp_msg.header.messageLength = htons(static_cast<uint16_t>(sizeof(PdelayRespMessage)));
    pdelay_resp_msg.header.domainNumber = protocol::DEFAULT_DOMAIN;
    pdelay_resp_msg.header.flags = htons(0x0200); // Two-step flag
    pdelay_resp_msg.header.correctionField = 0;
    pdelay_resp_msg.header.sourcePortIdentity = source_port_identity;
    pdelay_resp_msg.header.sequenceId = htons(sequence_id);
    pdelay_resp_msg.header.controlField = 0x05; // Other
    pdelay_resp_msg.header.logMessageInterval = protocol::LOG_PDELAY_INTERVAL_1S; // 0 (1000ms)
    
    // Set request receipt timestamp
    pdelay_resp_msg.requestReceiptTimestamp = request_receipt_timestamp;
    
    // Set requesting port identity
    pdelay_resp_msg.requestingPortIdentity = requesting_port_identity;
    
    // Serialize message to packet payload
    packet.payload.resize(sizeof(PdelayRespMessage));
    std::memcpy(packet.payload.data(), &pdelay_resp_msg, sizeof(PdelayRespMessage));
    
    return packet;
}

GptpPacket GptpPacketBuilder::create_announce_packet(const PortIdentity& source_port_identity,
                                                    uint16_t sequence_id,
                                                    const ClockIdentity& grandmaster_identity,
                                                    uint8_t grandmaster_priority1,
                                                    uint8_t grandmaster_priority2,
                                                    uint16_t steps_removed,
                                                    const std::array<uint8_t, 6>& source_mac) {
    GptpPacket packet;
    
    // Set Ethernet header
    packet.ethernet.destination = protocol::GPTP_MULTICAST_MAC;
    packet.ethernet.source = source_mac;
    packet.ethernet.etherType = htons(protocol::GPTP_ETHERTYPE);
    
    // Create Announce message
    AnnounceMessage announce_msg;
    
    // Set gPTP message header
    announce_msg.header.messageType = static_cast<uint8_t>(protocol::MessageType::ANNOUNCE);
    announce_msg.header.transportSpecific = 1; // IEEE 802.1AS
    announce_msg.header.versionPTP = 2;
    announce_msg.header.messageLength = htons(static_cast<uint16_t>(sizeof(AnnounceMessage)));
    announce_msg.header.domainNumber = protocol::DEFAULT_DOMAIN;
    announce_msg.header.flags = 0;
    announce_msg.header.correctionField = 0;
    announce_msg.header.sourcePortIdentity = source_port_identity;
    announce_msg.header.sequenceId = htons(sequence_id);
    announce_msg.header.controlField = 0x05; // Other
    announce_msg.header.logMessageInterval = protocol::LOG_ANNOUNCE_INTERVAL_1S; // 0 (1000ms)
    
    // Set current timestamp for origin timestamp
    set_current_timestamp(announce_msg.originTimestamp);
    
    // Set announce message fields
    announce_msg.currentUtcOffset = htons(37); // Current UTC offset (as of 2024)
    announce_msg.reserved = 0;
    announce_msg.grandmasterPriority1 = grandmaster_priority1;
    
    // Set grandmaster clock quality (packed as uint32_t)
    announce_msg.grandmasterClockQuality = 0; // Simplified for now
    
    announce_msg.grandmasterPriority2 = grandmaster_priority2;
    announce_msg.grandmasterIdentity = grandmaster_identity;
    announce_msg.stepsRemoved = htons(steps_removed);
    announce_msg.timeSource = static_cast<uint8_t>(protocol::TimeSource::INTERNAL_OSCILLATOR);
    
    // Serialize message to packet payload
    packet.payload.resize(sizeof(AnnounceMessage));
    std::memcpy(packet.payload.data(), &announce_msg, sizeof(AnnounceMessage));
    
    return packet;
}

void GptpPacketBuilder::set_current_timestamp(Timestamp& timestamp) {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()) % std::chrono::seconds(1);
    
    timestamp.set_seconds(static_cast<uint64_t>(time_t_now));
    timestamp.nanoseconds = static_cast<uint32_t>(nanoseconds.count());
}

} // namespace gptp
