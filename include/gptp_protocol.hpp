/**
 * @file gptp_protocol.hpp
 * @brief IEEE 802.1AS gPTP protocol data structures and constants
 * 
 * This file defines the core data structures, message formats, and constants
 * required for IEEE 802.1AS-2021 (gPTP) protocol implementation.
 */

#pragma once

#include <cstdint>
#include <array>
#include <chrono>

// Cross-platform packed struct attribute
#ifdef _MSC_VER
    #pragma pack(push, 1)
    #define PACKED
#else
    #define PACKED __attribute__((packed))
#endif

namespace gptp {

    // IEEE 802.1AS Protocol Constants
    namespace protocol {
        
        // Message Types (IEEE 802.1AS-2021 Table 10-5)
        enum class MessageType : uint8_t {
            SYNC = 0x0,
            DELAY_REQ = 0x1,
            PDELAY_REQ = 0x2,
            PDELAY_RESP = 0x3,
            FOLLOW_UP = 0x8,
            DELAY_RESP = 0x9,
            PDELAY_RESP_FOLLOW_UP = 0xA,
            ANNOUNCE = 0xB,
            SIGNALING = 0xC,
            MANAGEMENT = 0xD
        };

        // Domain Numbers (IEEE 802.1AS-2021 Table 10-1)
        constexpr uint8_t DEFAULT_DOMAIN = 0;
        
        // Multicast Addresses
        constexpr std::array<uint8_t, 6> GPTP_MULTICAST_MAC = {0x01, 0x80, 0xC2, 0x00, 0x00, 0x0E};
        
        // EtherType for gPTP
        constexpr uint16_t GPTP_ETHERTYPE = 0x88F7;
        
        // Message Intervals (IEEE 802.1AS-2021 compliant)
        constexpr int8_t LOG_SYNC_INTERVAL_125MS = -3;     // 125ms = 2^(-3) seconds
        constexpr int8_t LOG_ANNOUNCE_INTERVAL_1S = 0;     // 1s = 2^0 seconds  
        constexpr int8_t LOG_PDELAY_INTERVAL_1S = 0;       // 1s = 2^0 seconds
        
        // Convert log intervals to milliseconds
        inline constexpr uint32_t log_interval_to_ms(int8_t log_interval) {
            return log_interval >= 0 ? 
                (1000U << log_interval) : 
                (1000U >> (-log_interval));
        }
        
        // Standard intervals in milliseconds
        constexpr uint32_t SYNC_INTERVAL_MS = log_interval_to_ms(LOG_SYNC_INTERVAL_125MS);      // 125ms
        constexpr uint32_t ANNOUNCE_INTERVAL_MS = log_interval_to_ms(LOG_ANNOUNCE_INTERVAL_1S); // 1000ms
        constexpr uint32_t PDELAY_INTERVAL_MS = log_interval_to_ms(LOG_PDELAY_INTERVAL_1S);     // 1000ms
        
        // Clock Accuracy (IEEE 802.1AS-2021 Table 7-2)
        enum class ClockAccuracy : uint8_t {
            WITHIN_25_NS = 0x20,
            WITHIN_100_NS = 0x21,
            WITHIN_250_NS = 0x22,
            WITHIN_1_US = 0x23,
            WITHIN_2_5_US = 0x24,
            WITHIN_10_US = 0x25,
            WITHIN_25_US = 0x26,
            WITHIN_100_US = 0x27,
            WITHIN_250_US = 0x28,
            WITHIN_1_MS = 0x29,
            WITHIN_2_5_MS = 0x2A,
            WITHIN_10_MS = 0x2B,
            WITHIN_25_MS = 0x2C,
            WITHIN_100_MS = 0x2D,
            WITHIN_250_MS = 0x2E,
            WITHIN_1_S = 0x2F,
            WITHIN_10_S = 0x30,
            GREATER_THAN_10_S = 0x31,
            UNKNOWN = 0xFE
        };
        
        // Time Source (IEEE 802.1AS-2021 Table 7-3)
        enum class TimeSource : uint8_t {
            ATOMIC_CLOCK = 0x10,
            GPS = 0x20,
            TERRESTRIAL_RADIO = 0x30,
            PTP = 0x40,
            NTP = 0x50,
            HAND_SET = 0x60,
            OTHER = 0x90,
            INTERNAL_OSCILLATOR = 0xA0
        };
    }

    // Basic IEEE 802.1AS Data Types
    
    /**
     * @brief 8-byte Clock Identity (IEEE 802.1AS-2021 clause 7.5.2.2.2)
     */
    struct ClockIdentity {
        std::array<uint8_t, 8> id;
        
        ClockIdentity() : id{0} {}
        
        bool operator==(const ClockIdentity& other) const {
            return id == other.id;
        }
        
        bool operator<(const ClockIdentity& other) const {
            return id < other.id;
        }
    };

    /**
     * @brief Port Identity (IEEE 802.1AS-2021 clause 7.5.2.3)
     */
    struct PortIdentity {
        ClockIdentity clockIdentity;
        uint16_t portNumber;
        
        PortIdentity() : portNumber(0) {}
        
        bool operator==(const PortIdentity& other) const {
            return clockIdentity == other.clockIdentity && portNumber == other.portNumber;
        }
    };

    /**
     * @brief Timestamp structure (IEEE 802.1AS-2021 clause 7.3.2)
     * Represents seconds and nanoseconds since epoch
     */
    struct Timestamp {
        uint64_t seconds_msb : 16;   // Most significant 16 bits of seconds
        uint64_t seconds_lsb : 32;   // Least significant 32 bits of seconds  
        uint32_t nanoseconds;        // Nanoseconds (0-999,999,999)
        
        Timestamp() : seconds_msb(0), seconds_lsb(0), nanoseconds(0) {}
        
        Timestamp(uint64_t sec, uint32_t nsec) 
            : seconds_msb((sec >> 32) & 0xFFFF)
            , seconds_lsb(sec & 0xFFFFFFFF)
            , nanoseconds(nsec) {}
        
        uint64_t get_seconds() const {
            return (static_cast<uint64_t>(seconds_msb) << 32) | seconds_lsb;
        }
        
        void set_seconds(uint64_t seconds) {
            seconds_msb = (seconds >> 32) & 0xFFFF;
            seconds_lsb = seconds & 0xFFFFFFFF;
        }
        
        // Convert to/from std::chrono
        std::chrono::nanoseconds to_nanoseconds() const {
            return std::chrono::seconds(get_seconds()) + std::chrono::nanoseconds(nanoseconds);
        }
        
        void from_nanoseconds(std::chrono::nanoseconds ns) {
            auto secs = std::chrono::duration_cast<std::chrono::seconds>(ns);
            set_seconds(secs.count());
            nanoseconds = (ns - secs).count();
        }
    } PACKED;

    /**
     * @brief UScaledNs - Unsigned Scaled Nanoseconds (IEEE 802.1AS-2021 clause 7.3.3)
     */
    struct UScaledNs {
        uint64_t nanoseconds_msb : 16;
        uint64_t nanoseconds_lsb : 64;
        
        UScaledNs() : nanoseconds_msb(0), nanoseconds_lsb(0) {}
    } PACKED;

    /**
     * @brief Common gPTP Message Header (IEEE 802.1AS-2021 clause 11.2.2)
     */
    struct GptpMessageHeader {
        uint8_t messageType : 4;
        uint8_t transportSpecific : 4;  // Always 1 for 802.1AS
        uint8_t reserved1 : 4;
        uint8_t versionPTP : 4;         // Always 2 for 802.1AS
        uint16_t messageLength;
        uint8_t domainNumber;           // Always 0 for 802.1AS
        uint8_t reserved2;
        uint16_t flags;
        int64_t correctionField;        // nanoseconds << 16
        uint32_t reserved3;
        PortIdentity sourcePortIdentity;
        uint16_t sequenceId;
        uint8_t controlField;
        int8_t logMessageInterval;
        
        GptpMessageHeader() {
            transportSpecific = 1;      // IEEE 802.1AS
            versionPTP = 2;             // PTP version 2
            domainNumber = 0;           // Always 0 for 802.1AS
            reserved1 = 0;
            reserved2 = 0;
            reserved3 = 0;
            flags = 0;
            correctionField = 0;
            sequenceId = 0;
            controlField = 0;
            logMessageInterval = 0;
        }
    } PACKED;

    /**
     * @brief Sync Message (IEEE 802.1AS-2021 clause 11.2.7)
     */
    struct SyncMessage {
        GptpMessageHeader header;
        Timestamp originTimestamp;
        
        SyncMessage() {
            header.messageType = static_cast<uint8_t>(protocol::MessageType::SYNC);
            header.messageLength = sizeof(SyncMessage);
            header.controlField = 0x00;
        }
    } PACKED;

    /**
     * @brief Follow_Up Message (IEEE 802.1AS-2021 clause 11.2.8)
     */
    struct FollowUpMessage {
        GptpMessageHeader header;
        Timestamp preciseOriginTimestamp;
        
        FollowUpMessage() {
            header.messageType = static_cast<uint8_t>(protocol::MessageType::FOLLOW_UP);
            header.messageLength = sizeof(FollowUpMessage);
            header.controlField = 0x02;
        }
    } PACKED;

    /**
     * @brief Pdelay_Req Message (IEEE 802.1AS-2021 clause 11.2.9)
     */
    struct PdelayReqMessage {
        GptpMessageHeader header;
        Timestamp originTimestamp;
        uint8_t reserved[10];
        
        PdelayReqMessage() {
            header.messageType = static_cast<uint8_t>(protocol::MessageType::PDELAY_REQ);
            header.messageLength = sizeof(PdelayReqMessage);
            header.controlField = 0x05;
            std::fill(std::begin(reserved), std::end(reserved), 0);
        }
    } PACKED;

    /**
     * @brief Pdelay_Resp Message (IEEE 802.1AS-2021 clause 11.2.10)
     */
    struct PdelayRespMessage {
        GptpMessageHeader header;
        Timestamp requestReceiptTimestamp;
        PortIdentity requestingPortIdentity;
        
        PdelayRespMessage() {
            header.messageType = static_cast<uint8_t>(protocol::MessageType::PDELAY_RESP);
            header.messageLength = sizeof(PdelayRespMessage);
            header.controlField = 0x03;
        }
    } PACKED;

    /**
     * @brief Pdelay_Resp_Follow_Up Message (IEEE 802.1AS-2021 clause 11.2.11)
     */
    struct PdelayRespFollowUpMessage {
        GptpMessageHeader header;
        Timestamp responseOriginTimestamp;
        PortIdentity requestingPortIdentity;
        
        PdelayRespFollowUpMessage() {
            header.messageType = static_cast<uint8_t>(protocol::MessageType::PDELAY_RESP_FOLLOW_UP);
            header.messageLength = sizeof(PdelayRespFollowUpMessage);
            header.controlField = 0x04;
        }
    } PACKED;

    /**
     * @brief Announce Message (IEEE 802.1AS-2021 clause 11.2.12)
     */
    struct AnnounceMessage {
        GptpMessageHeader header;
        Timestamp originTimestamp;
        int16_t currentUtcOffset;
        uint8_t reserved;
        uint8_t grandmasterPriority1;
        uint32_t grandmasterClockQuality;  // ClockQuality structure packed as uint32_t
        uint8_t grandmasterPriority2;
        ClockIdentity grandmasterIdentity;
        uint16_t stepsRemoved;
        uint8_t timeSource;
        
        AnnounceMessage() {
            header.messageType = static_cast<uint8_t>(protocol::MessageType::ANNOUNCE);
            header.messageLength = sizeof(AnnounceMessage);
            header.controlField = 0x05;
            currentUtcOffset = 0;
            reserved = 0;
            grandmasterPriority1 = 248;  // gPTP default
            grandmasterClockQuality = 0;
            grandmasterPriority2 = 248;  // gPTP default
            stepsRemoved = 0;
            timeSource = static_cast<uint8_t>(protocol::TimeSource::INTERNAL_OSCILLATOR);
        }
    } PACKED;

    /**
     * @brief Clock Quality (IEEE 802.1AS-2021 clause 7.6.2.4)
     */
    struct ClockQuality {
        uint8_t clockClass;
        protocol::ClockAccuracy clockAccuracy;
        uint16_t offsetScaledLogVariance;
        
        ClockQuality() 
            : clockClass(248)  // gPTP default for end station
            , clockAccuracy(protocol::ClockAccuracy::UNKNOWN)
            , offsetScaledLogVariance(0x436A)  // gPTP default
        {}
    } PACKED;

    // Port States (IEEE 802.1AS-2021 clause 10.2.5)
    enum class PortState : uint8_t {
        INITIALIZING = 1,
        FAULTY = 2,
        DISABLED = 3,
        LISTENING = 4,
        PRE_MASTER = 5,
        MASTER = 6,
        PASSIVE = 7,
        UNCALIBRATED = 8,
        SLAVE = 9
    };

    // Link Delay Mechanism (IEEE 802.1AS-2021 clause 7.7.2.4)
    enum class DelayMechanism : uint8_t {
        E2E = 0x01,     // End-to-End (not used in 802.1AS)
        P2P = 0x02,     // Peer-to-Peer (used in 802.1AS)
        DISABLED = 0xFE
    };

} // namespace gptp

#ifdef _MSC_VER
    #pragma pack(pop)
#endif
