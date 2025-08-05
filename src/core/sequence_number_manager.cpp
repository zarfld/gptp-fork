/**
 * @file sequence_number_manager.cpp
 * @brief IEEE 802.1AS-2021 Sequence Number Management Implementation
 */

#include "../../include/sequence_number_manager.hpp"
#include <iostream>

namespace gptp {
namespace sequence {

// Implementation is mostly header-only due to template and inline nature
// This file exists for any future non-template implementations

namespace utils {

void print_sequence_status(uint16_t port_id, const PortSequenceManager::SequenceStatus& status) {
    std::cout << "Port " << port_id << " Sequence Status:" << std::endl;
    std::cout << "  Announce:    " << format_sequence(status.announce_sequence) << std::endl;
    std::cout << "  Signaling:   " << format_sequence(status.signaling_sequence) << std::endl;
    std::cout << "  Sync:        " << format_sequence(status.sync_sequence) << std::endl;
    std::cout << "  Follow_Up:   " << format_sequence(status.followup_sequence) << std::endl;
    std::cout << "  Pdelay_Req:  " << format_sequence(status.pdelay_req_sequence) << std::endl;
    std::cout << "  Pdelay_Resp: " << format_sequence(status.pdelay_resp_sequence) << std::endl;
}

void print_all_sequence_status(const SequenceNumberManager& manager) {
    auto all_status = manager.get_all_sequence_status();
    
    std::cout << "\n=== IEEE 802.1AS Sequence Number Status ===" << std::endl;
    for (const auto& pair : all_status) {
        print_sequence_status(pair.first, pair.second);
        std::cout << std::endl;
    }
}

bool validate_sequence_number_compliance(uint16_t port_id, 
                                       protocol::MessageType message_type,
                                       uint16_t received_sequence,
                                       uint16_t expected_sequence) {
    bool valid = is_valid_sequence_progression(expected_sequence, received_sequence);
    
    if (!valid) {
        std::cerr << "❌ Sequence number violation on port " << port_id 
                  << " for message type " << static_cast<int>(message_type) << std::endl;
        std::cerr << "   Expected: " << format_sequence(expected_sequence) << std::endl;
        std::cerr << "   Received: " << format_sequence(received_sequence) << std::endl;
    }
    
    return valid;
}

} // namespace utils

} // namespace sequence
} // namespace gptp
