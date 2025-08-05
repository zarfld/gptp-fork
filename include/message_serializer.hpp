/**
 * @file message_serializer.hpp
 * @brief IEEE 802.1AS-2021 Message Serialization and Deserialization
 * 
 * Proper wire format serialization with:
 * - Network byte order (big-endian) conversion
 * - IEEE 802.1AS field layout compliance
 * - No struct padding dependencies
 * - Cross-platform compatibility
 */

#ifndef GPTP_MESSAGE_SERIALIZER_HPP
#define GPTP_MESSAGE_SERIALIZER_HPP

#include "gptp_protocol.hpp"
#include <vector>
#include <cstdint>
#include <cstring>
#include <stdexcept>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

namespace gptp {
namespace serialization {

/**
 * @brief Binary serialization writer with network byte order
 */
class BinaryWriter {
public:
    BinaryWriter() : data_(), offset_(0) {}
    
    /**
     * @brief Write 8-bit value (no endianness conversion needed)
     */
    void write_uint8(uint8_t value) {
        data_.push_back(value);
        offset_++;
    }
    
    /**
     * @brief Write 16-bit value in network byte order
     */
    void write_uint16(uint16_t value) {
        uint16_t net_value = htons(value);
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&net_value);
        data_.push_back(bytes[0]);
        data_.push_back(bytes[1]);
        offset_ += 2;
    }
    
    /**
     * @brief Write 32-bit value in network byte order
     */
    void write_uint32(uint32_t value) {
        uint32_t net_value = htonl(value);
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&net_value);
        for (int i = 0; i < 4; ++i) {
            data_.push_back(bytes[i]);
        }
        offset_ += 4;
    }
    
    /**
     * @brief Write 64-bit value in network byte order
     */
    void write_uint64(uint64_t value) {
        // Split into two 32-bit values for network byte order
        uint32_t high = static_cast<uint32_t>(value >> 32);
        uint32_t low = static_cast<uint32_t>(value & 0xFFFFFFFF);
        write_uint32(high);
        write_uint32(low);
    }
    
    /**
     * @brief Write signed 64-bit value in network byte order
     */
    void write_int64(int64_t value) {
        write_uint64(static_cast<uint64_t>(value));
    }
    
    /**
     * @brief Write array of bytes
     */
    void write_bytes(const uint8_t* bytes, size_t length) {
        for (size_t i = 0; i < length; ++i) {
            data_.push_back(bytes[i]);
        }
        offset_ += length;
    }
    
    /**
     * @brief Write ClockIdentity (8 bytes, no endianness conversion)
     */
    void write_clock_identity(const ClockIdentity& clock_id) {
        write_bytes(clock_id.id.data(), 8);
    }
    
    /**
     * @brief Write Timestamp in IEEE 802.1AS format
     */
    void write_timestamp(const Timestamp& timestamp) {
        // IEEE 802.1AS timestamp format:
        // 6 bytes: seconds (48-bit, big-endian)
        // 4 bytes: nanoseconds (32-bit, big-endian)
        
        uint64_t seconds = timestamp.get_seconds();
        uint32_t nanoseconds = timestamp.nanoseconds;
        
        // Write 48-bit seconds as 6 bytes (big-endian)
        write_uint8(static_cast<uint8_t>((seconds >> 40) & 0xFF));
        write_uint8(static_cast<uint8_t>((seconds >> 32) & 0xFF));
        write_uint8(static_cast<uint8_t>((seconds >> 24) & 0xFF));
        write_uint8(static_cast<uint8_t>((seconds >> 16) & 0xFF));
        write_uint8(static_cast<uint8_t>((seconds >> 8) & 0xFF));
        write_uint8(static_cast<uint8_t>(seconds & 0xFF));
        
        // Write 32-bit nanoseconds
        write_uint32(nanoseconds);
    }
    
    /**
     * @brief Get serialized data
     */
    const std::vector<uint8_t>& get_data() const {
        return data_;
    }
    
    /**
     * @brief Get current size
     */
    size_t size() const {
        return data_.size();
    }
    
    /**
     * @brief Clear data and reset
     */
    void clear() {
        data_.clear();
        offset_ = 0;
    }

private:
    std::vector<uint8_t> data_;
    size_t offset_;
};

/**
 * @brief Binary deserialization reader with network byte order
 */
class BinaryReader {
public:
    BinaryReader(const std::vector<uint8_t>& data) : data_(data), offset_(0) {}
    BinaryReader(const uint8_t* data, size_t length) : data_(data, data + length), offset_(0) {}
    
    /**
     * @brief Read 8-bit value
     */
    uint8_t read_uint8() {
        if (offset_ >= data_.size()) throw std::runtime_error("Buffer underrun");
        return data_[offset_++];
    }
    
    /**
     * @brief Read 16-bit value from network byte order
     */
    uint16_t read_uint16() {
        if (offset_ + 2 > data_.size()) throw std::runtime_error("Buffer underrun");
        uint16_t net_value;
        std::memcpy(&net_value, &data_[offset_], 2);
        offset_ += 2;
        return ntohs(net_value);
    }
    
    /**
     * @brief Read 32-bit value from network byte order
     */
    uint32_t read_uint32() {
        if (offset_ + 4 > data_.size()) throw std::runtime_error("Buffer underrun");
        uint32_t net_value;
        std::memcpy(&net_value, &data_[offset_], 4);
        offset_ += 4;
        return ntohl(net_value);
    }
    
    /**
     * @brief Read 64-bit value from network byte order
     */
    uint64_t read_uint64() {
        uint32_t high = read_uint32();
        uint32_t low = read_uint32();
        return (static_cast<uint64_t>(high) << 32) | low;
    }
    
    /**
     * @brief Read signed 64-bit value from network byte order
     */
    int64_t read_int64() {
        return static_cast<int64_t>(read_uint64());
    }
    
    /**
     * @brief Read array of bytes
     */
    void read_bytes(uint8_t* bytes, size_t length) {
        if (offset_ + length > data_.size()) throw std::runtime_error("Buffer underrun");
        std::memcpy(bytes, &data_[offset_], length);
        offset_ += length;
    }
    
    /**
     * @brief Read ClockIdentity
     */
    ClockIdentity read_clock_identity() {
        ClockIdentity clock_id;
        read_bytes(clock_id.id.data(), 8);
        return clock_id;
    }
    
    /**
     * @brief Read Timestamp in IEEE 802.1AS format
     */
    Timestamp read_timestamp() {
        // Read 48-bit seconds (6 bytes, big-endian)
        uint64_t seconds = 0;
        for (int i = 0; i < 6; ++i) {
            seconds = (seconds << 8) | read_uint8();
        }
        
        // Read 32-bit nanoseconds
        uint32_t nanoseconds = read_uint32();
        
        Timestamp timestamp;
        timestamp.set_seconds(seconds);
        timestamp.nanoseconds = nanoseconds;
        return timestamp;
    }
    
    /**
     * @brief Get remaining bytes
     */
    size_t remaining() const {
        return data_.size() - offset_;
    }
    
    /**
     * @brief Check if at end
     */
    bool at_end() const {
        return offset_ >= data_.size();
    }

private:
    std::vector<uint8_t> data_;
    size_t offset_;
};

/**
 * @brief IEEE 802.1AS Message Serializer
 */
class MessageSerializer {
public:
    /**
     * @brief Serialize gPTP message header
     * IEEE 802.1AS-2021 Section 10.5.2
     */
    static void serialize_header(BinaryWriter& writer, const GptpMessageHeader& header) {
        // Byte 0: messageType (4 bits) | transportSpecific (4 bits)
        uint8_t byte0 = (header.transportSpecific << 4) | (header.messageType & 0x0F);
        writer.write_uint8(byte0);
        
        // Byte 1: reserved1 (4 bits) | versionPTP (4 bits)
        uint8_t byte1 = (header.reserved1 << 4) | (header.versionPTP & 0x0F);
        writer.write_uint8(byte1);
        
        // Bytes 2-3: messageLength (16-bit, network byte order)
        writer.write_uint16(header.messageLength);
        
        // Byte 4: domainNumber
        writer.write_uint8(header.domainNumber);
        
        // Byte 5: reserved2
        writer.write_uint8(header.reserved2);
        
        // Bytes 6-7: flags (16-bit, network byte order)
        writer.write_uint16(header.flags);
        
        // Bytes 8-15: correctionField (64-bit, network byte order)
        writer.write_int64(header.correctionField);
        
        // Bytes 16-19: reserved3 (32-bit, network byte order)
        writer.write_uint32(header.reserved3);
        
        // Bytes 20-29: sourcePortIdentity
        writer.write_clock_identity(header.sourcePortIdentity.clockIdentity);
        writer.write_uint16(header.sourcePortIdentity.portNumber);
        
        // Bytes 30-31: sequenceId (16-bit, network byte order)
        writer.write_uint16(header.sequenceId);
        
        // Byte 32: controlField
        writer.write_uint8(header.controlField);
        
        // Byte 33: logMessageInterval
        writer.write_uint8(static_cast<uint8_t>(header.logMessageInterval));
    }
    
    /**
     * @brief Deserialize gPTP message header
     */
    static GptpMessageHeader deserialize_header(BinaryReader& reader) {
        GptpMessageHeader header;
        
        // Byte 0: messageType | transportSpecific
        uint8_t byte0 = reader.read_uint8();
        header.transportSpecific = (byte0 >> 4) & 0x0F;
        header.messageType = byte0 & 0x0F;
        
        // Byte 1: reserved1 | versionPTP
        uint8_t byte1 = reader.read_uint8();
        header.reserved1 = (byte1 >> 4) & 0x0F;
        header.versionPTP = byte1 & 0x0F;
        
        // Bytes 2-3: messageLength
        header.messageLength = reader.read_uint16();
        
        // Byte 4: domainNumber
        header.domainNumber = reader.read_uint8();
        
        // Byte 5: reserved2
        header.reserved2 = reader.read_uint8();
        
        // Bytes 6-7: flags
        header.flags = reader.read_uint16();
        
        // Bytes 8-15: correctionField
        header.correctionField = reader.read_int64();
        
        // Bytes 16-19: reserved3
        header.reserved3 = reader.read_uint32();
        
        // Bytes 20-29: sourcePortIdentity
        header.sourcePortIdentity.clockIdentity = reader.read_clock_identity();
        header.sourcePortIdentity.portNumber = reader.read_uint16();
        
        // Bytes 30-31: sequenceId
        header.sequenceId = reader.read_uint16();
        
        // Byte 32: controlField
        header.controlField = reader.read_uint8();
        
        // Byte 33: logMessageInterval
        header.logMessageInterval = static_cast<int8_t>(reader.read_uint8());
        
        return header;
    }
    
    /**
     * @brief Serialize Announce message
     * IEEE 802.1AS-2021 Section 11.2.12
     */
    static std::vector<uint8_t> serialize_announce(const AnnounceMessage& message) {
        BinaryWriter writer;
        
        // Serialize header (34 bytes)
        serialize_header(writer, message.header);
        
        // Serialize announce-specific fields
        writer.write_timestamp(message.originTimestamp);        // 10 bytes
        writer.write_uint16(message.currentUtcOffset);          // 2 bytes
        writer.write_uint8(message.reserved);                   // 1 byte
        writer.write_uint8(message.grandmasterPriority1);       // 1 byte
        writer.write_uint32(message.grandmasterClockQuality);   // 4 bytes (packed ClockQuality)
        writer.write_uint8(message.grandmasterPriority2);       // 1 byte
        writer.write_clock_identity(message.grandmasterIdentity); // 8 bytes
        writer.write_uint16(message.stepsRemoved);              // 2 bytes
        writer.write_uint8(message.timeSource);                 // 1 byte
        
        return writer.get_data();
    }
    
    /**
     * @brief Serialize Sync message
     * IEEE 802.1AS-2021 Section 11.2.9
     */
    static std::vector<uint8_t> serialize_sync(const SyncMessage& message) {
        BinaryWriter writer;
        
        // Serialize header (34 bytes)
        serialize_header(writer, message.header);
        
        // Serialize sync-specific fields
        writer.write_timestamp(message.originTimestamp);        // 10 bytes
        
        return writer.get_data();
    }
    
    /**
     * @brief Serialize Follow_Up message
     * IEEE 802.1AS-2021 Section 11.2.10
     */
    static std::vector<uint8_t> serialize_followup(const FollowUpMessage& message) {
        BinaryWriter writer;
        
        // Serialize header (34 bytes)
        serialize_header(writer, message.header);
        
        // Serialize follow_up-specific fields
        writer.write_timestamp(message.preciseOriginTimestamp); // 10 bytes
        
        return writer.get_data();
    }
    
    /**
     * @brief Serialize Pdelay_Req message
     * IEEE 802.1AS-2021 Section 11.2.5
     */
    static std::vector<uint8_t> serialize_pdelay_req(const PdelayReqMessage& message) {
        BinaryWriter writer;
        
        // Serialize header (34 bytes)
        serialize_header(writer, message.header);
        
        // Serialize pdelay_req-specific fields
        writer.write_timestamp(message.originTimestamp);        // 10 bytes
        writer.write_bytes(message.reserved, 10);               // 10 bytes reserved
        
        return writer.get_data();
    }
    
    /**
     * @brief Serialize Pdelay_Resp message
     * IEEE 802.1AS-2021 Section 11.2.6
     */
    static std::vector<uint8_t> serialize_pdelay_resp(const PdelayRespMessage& message) {
        BinaryWriter writer;
        
        // Serialize header (34 bytes)
        serialize_header(writer, message.header);
        
        // Serialize pdelay_resp-specific fields
        writer.write_timestamp(message.requestReceiptTimestamp); // 10 bytes
        writer.write_clock_identity(message.requestingPortIdentity.clockIdentity); // 8 bytes
        writer.write_uint16(message.requestingPortIdentity.portNumber); // 2 bytes
        
        return writer.get_data();
    }
    
    /**
     * @brief Get expected message size for validation
     */
    static size_t get_expected_size(protocol::MessageType message_type) {
        switch (message_type) {
            case protocol::MessageType::SYNC:
                return 44;  // 34 (header) + 10 (timestamp)
                
            case protocol::MessageType::FOLLOW_UP:
                return 44;  // 34 (header) + 10 (timestamp)
                
            case protocol::MessageType::PDELAY_REQ:
                return 54;  // 34 (header) + 10 (timestamp) + 10 (reserved)
                
            case protocol::MessageType::PDELAY_RESP:
                return 54;  // 34 (header) + 10 (timestamp) + 8 (clock) + 2 (port)
                
            case protocol::MessageType::ANNOUNCE:
                return 64;  // 34 (header) + 30 (announce fields)
                
            default:
                return 0;
        }
    }
};

} // namespace serialization
} // namespace gptp

#endif // GPTP_MESSAGE_SERIALIZER_HPP
