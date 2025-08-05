/**
 * @file test_sequence_number_manager.cpp
 * @brief Unit tests for IEEE 802.1AS-2021 Sequence Number Management
 */

#include "../include/sequence_number_manager.hpp"
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>
#include <algorithm>

using namespace gptp::sequence;
using namespace gptp::protocol;

/**
 * @brief Test basic sequence number increment
 */
void test_basic_sequence_increment() {
    std::cout << "Testing basic sequence number increment..." << std::endl;
    
    SequenceNumberPool pool;
    
    // Test initial state
    assert(pool.get_current_sequence() == 0);
    
    // Test increment
    uint16_t seq1 = pool.get_next_sequence();
    assert(seq1 == 0);
    assert(pool.get_current_sequence() == 1);
    
    uint16_t seq2 = pool.get_next_sequence();
    assert(seq2 == 1);
    assert(pool.get_current_sequence() == 2);
    
    std::cout << "✅ Basic sequence increment test passed" << std::endl;
}

/**
 * @brief Test UInteger16 rollover behavior
 */
void test_rollover_behavior() {
    std::cout << "Testing UInteger16 rollover behavior..." << std::endl;
    
    SequenceNumberPool pool;
    
    // Set to near rollover
    pool.reset_sequence(0xFFFE);
    
    uint16_t seq1 = pool.get_next_sequence();
    assert(seq1 == 0xFFFE);
    assert(pool.get_current_sequence() == 0xFFFF);
    
    uint16_t seq2 = pool.get_next_sequence();
    assert(seq2 == 0xFFFF);
    assert(pool.get_current_sequence() == 0);  // Rollover!
    
    uint16_t seq3 = pool.get_next_sequence();
    assert(seq3 == 0);
    assert(pool.get_current_sequence() == 1);
    
    std::cout << "✅ UInteger16 rollover test passed" << std::endl;
}

/**
 * @brief Test port sequence manager
 */
void test_port_sequence_manager() {
    std::cout << "Testing port sequence manager..." << std::endl;
    
    PortSequenceManager port_manager;
    
    // Test different message types have independent sequences
    uint16_t announce1 = port_manager.get_next_announce_sequence();
    uint16_t signaling1 = port_manager.get_next_signaling_sequence();
    uint16_t sync1 = port_manager.get_next_sync_sequence();
    
    assert(announce1 == 0);
    assert(signaling1 == 0);
    assert(sync1 == 0);
    
    // Test increment
    uint16_t announce2 = port_manager.get_next_announce_sequence();
    uint16_t signaling2 = port_manager.get_next_signaling_sequence();
    
    assert(announce2 == 1);
    assert(signaling2 == 1);
    
    // Test status
    auto status = port_manager.get_sequence_status();
    assert(status.announce_sequence == 2);
    assert(status.signaling_sequence == 2);
    assert(status.sync_sequence == 1);
    
    std::cout << "✅ Port sequence manager test passed" << std::endl;
}

/**
 * @brief Test global sequence number manager
 */
void test_global_sequence_manager() {
    std::cout << "Testing global sequence number manager..." << std::endl;
    
    SequenceNumberManager global_manager;
    
    // Test multiple ports
    uint16_t port1_announce1 = global_manager.get_next_sequence(1, MessageType::ANNOUNCE);
    uint16_t port2_announce1 = global_manager.get_next_sequence(2, MessageType::ANNOUNCE);
    
    assert(port1_announce1 == 0);
    assert(port2_announce1 == 0);  // Independent sequences per port
    
    uint16_t port1_announce2 = global_manager.get_next_sequence(1, MessageType::ANNOUNCE);
    uint16_t port1_signaling1 = global_manager.get_next_sequence(1, MessageType::SIGNALING);
    
    assert(port1_announce2 == 1);
    assert(port1_signaling1 == 0);  // Independent sequences per message type
    
    // Test active ports
    auto active_ports = global_manager.get_active_ports();
    assert(active_ports.size() == 2);
    assert(std::find(active_ports.begin(), active_ports.end(), 1) != active_ports.end());
    assert(std::find(active_ports.begin(), active_ports.end(), 2) != active_ports.end());
    
    std::cout << "✅ Global sequence manager test passed" << std::endl;
}

/**
 * @brief Test utility functions
 */
void test_utility_functions() {
    std::cout << "Testing utility functions..." << std::endl;
    
    // Test rollover detection
    assert(utils::is_sequence_rollover(0xFFFF, 0x0000) == true);
    assert(utils::is_sequence_rollover(0xFFFE, 0xFFFF) == false);
    assert(utils::is_sequence_rollover(0x0000, 0x0001) == false);
    
    // Test sequence difference
    assert(utils::sequence_difference(5, 10) == 5);
    assert(utils::sequence_difference(0xFFFF, 0x0002) == 4);  // Rollover case
    assert(utils::sequence_difference(10, 5) == 0xFFFB);     // Backwards (rollover case)
    
    // Test valid sequence progression
    assert(utils::is_valid_sequence_progression(5, 5) == true);
    assert(utils::is_valid_sequence_progression(0xFFFF, 0x0000) == true);  // Rollover
    assert(utils::is_valid_sequence_progression(5, 7) == false);  // Skip
    
    // Test formatting
    std::string formatted = utils::format_sequence(0x1234);
    assert(formatted.find("4660") != std::string::npos);  // Decimal
    assert(formatted.find("0x1234") != std::string::npos); // Hex
    
    std::cout << "✅ Utility functions test passed" << std::endl;
}

/**
 * @brief Test thread safety
 */
void test_thread_safety() {
    std::cout << "Testing thread safety..." << std::endl;
    
    SequenceNumberManager global_manager;
    const int num_threads = 4;
    const int sequences_per_thread = 1000;
    
    std::vector<std::thread> threads;
    std::vector<std::vector<uint16_t>> results(num_threads);
    
    // Start threads that generate sequences concurrently
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&global_manager, &results, i, sequences_per_thread]() {
            for (int j = 0; j < sequences_per_thread; ++j) {
                uint16_t seq = global_manager.get_next_sequence(1, MessageType::ANNOUNCE);
                results[i].push_back(seq);
            }
        });
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify all sequences are unique
    std::vector<uint16_t> all_sequences;
    for (const auto& thread_results : results) {
        all_sequences.insert(all_sequences.end(), thread_results.begin(), thread_results.end());
    }
    
    std::sort(all_sequences.begin(), all_sequences.end());
    
    // Check for duplicates
    for (size_t i = 1; i < all_sequences.size(); ++i) {
        assert(all_sequences[i] != all_sequences[i-1]);
    }
    
    // Check expected range
    assert(all_sequences.size() == num_threads * sequences_per_thread);
    assert(all_sequences[0] == 0);
    assert(all_sequences.back() == (num_threads * sequences_per_thread - 1));
    
    std::cout << "✅ Thread safety test passed" << std::endl;
}

/**
 * @brief Test IEEE 802.1AS compliance scenarios
 */
void test_ieee_compliance() {
    std::cout << "Testing IEEE 802.1AS-2021 compliance scenarios..." << std::endl;
    
    SequenceNumberManager manager;
    
    // Scenario 1: Announce and Signaling have separate pools (Section 10.5.7)
    uint16_t announce_seq1 = manager.get_next_sequence(1, MessageType::ANNOUNCE);
    uint16_t signaling_seq1 = manager.get_next_sequence(1, MessageType::SIGNALING);
    
    assert(announce_seq1 == 0);
    assert(signaling_seq1 == 0);  // Independent pools
    
    // Scenario 2: Each message type increments by 1
    uint16_t announce_seq2 = manager.get_next_sequence(1, MessageType::ANNOUNCE);
    uint16_t announce_seq3 = manager.get_next_sequence(1, MessageType::ANNOUNCE);
    
    assert(announce_seq2 == 1);  // Previous + 1
    assert(announce_seq3 == 2);  // Previous + 1
    
    // Scenario 3: Different ports have independent sequences
    uint16_t port2_announce1 = manager.get_next_sequence(2, MessageType::ANNOUNCE);
    assert(port2_announce1 == 0);  // Independent from port 1
    
    // Scenario 4: UInteger16 rollover constraint
    auto& port3_manager = manager.get_port_manager(3);
    port3_manager.reset_all_sequences();
    
    // Simulate rollover by setting to max value
    for (int i = 0; i < 65536; ++i) {
        manager.get_next_sequence(3, MessageType::ANNOUNCE);
    }
    
    // Should have rolled over back to 0
    uint16_t after_rollover = manager.get_next_sequence(3, MessageType::ANNOUNCE);
    assert(after_rollover == 0);
    
    std::cout << "✅ IEEE 802.1AS compliance test passed" << std::endl;
}

/**
 * @brief Main test function
 */
int main() {
    std::cout << "\n🚀 IEEE 802.1AS-2021 Sequence Number Management Tests" << std::endl;
    std::cout << "=====================================================" << std::endl;
    
    try {
        test_basic_sequence_increment();
        test_rollover_behavior();
        test_port_sequence_manager();
        test_global_sequence_manager();
        test_utility_functions();
        test_thread_safety();
        test_ieee_compliance();
        
        std::cout << "\n✅ ALL SEQUENCE NUMBER MANAGEMENT TESTS PASSED!" << std::endl;
        
        // Demo usage
        std::cout << "\n📊 IEEE 802.1AS Sequence Number Management Demo:" << std::endl;
        SequenceNumberManager demo_manager;
        
        // Simulate message transmission for multiple ports
        for (int port = 1; port <= 2; ++port) {
            std::cout << "\nPort " << port << " message sequence demonstration:" << std::endl;
            
            for (int i = 0; i < 3; ++i) {
                uint16_t announce_seq = demo_manager.get_next_sequence(port, MessageType::ANNOUNCE);
                uint16_t signaling_seq = demo_manager.get_next_sequence(port, MessageType::SIGNALING);
                
                std::cout << "  Iteration " << (i+1) << ": ";
                std::cout << "Announce=" << utils::format_sequence(announce_seq);
                std::cout << ", Signaling=" << utils::format_sequence(signaling_seq) << std::endl;
            }
        }
        
        // Print final status
        auto all_status = demo_manager.get_all_sequence_status();
        std::cout << "\n=== Sequence Number Status ===" << std::endl;
        for (const auto& pair : all_status) {
            std::cout << "Port " << pair.first << " Sequences: ";
            std::cout << "Ann=" << pair.second.announce_sequence;
            std::cout << ", Sig=" << pair.second.signaling_sequence;
            std::cout << ", Sync=" << pair.second.sync_sequence << std::endl;
        }
        
        std::cout << "\n🎯 Implementation Status:" << std::endl;
        std::cout << "  ✅ IEEE 802.1AS-2021 Section 10.5.7 - COMPLETE" << std::endl;
        std::cout << "  ✅ Separate Announce/Signaling pools - COMPLETE" << std::endl;
        std::cout << "  ✅ UInteger16 rollover handling - COMPLETE" << std::endl;
        std::cout << "  ✅ Per-port sequence management - COMPLETE" << std::endl;
        std::cout << "  ✅ Thread-safe implementation - COMPLETE" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
