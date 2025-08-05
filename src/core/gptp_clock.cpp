/**
 * @file gptp_clock.cpp
 * @brief Implementation of IEEE 802.1AS Clock management
 */

#include "../../include/gptp_clock.hpp"
#include "../../include/gptp_state_machines.hpp"
#include <random>
#include <chrono>
#include <algorithm>
#include <iostream>

namespace gptp {

GptpClock::GptpClock() 
    : priority1_(255)  // Default priority (lowest)
    , priority2_(255)  // Default priority (lowest)
    , is_grandmaster_(false)
    , current_utc_offset_(37)  // Current UTC offset (seconds)
    , time_source_(protocol::TimeSource::INTERNAL_OSCILLATOR)
    , startup_time_(std::chrono::steady_clock::now())
    , time_offset_(0)
    , servo_(std::make_unique<servo::ClockServo>())
{
    // Generate random clock identity (IEEE EUI-64 format)
    // In production, this should be based on MAC address
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<unsigned int> dis(0, 255);
    
    for (int i = 0; i < 8; ++i) {
        clock_identity_.id[i] = static_cast<uint8_t>(dis(gen));
    }
    
    // Set reasonable default clock quality
    clock_quality_.clockClass = 248;  // Default clock class
    clock_quality_.clockAccuracy = protocol::ClockAccuracy::WITHIN_1_MS;
    clock_quality_.offsetScaledLogVariance = 0x4000;  // Default variance
}

std::chrono::nanoseconds GptpClock::get_current_time() const {
    // Get system time in nanoseconds since epoch
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
}

void GptpClock::set_current_time(std::chrono::nanoseconds time) {
    // In a real implementation, this would adjust the system clock
    // For now, we just store the offset
    (void)time; // Unused parameter
}

void GptpClock::add_port(std::shared_ptr<GptpPort> port) {
    if (port) {
        ports_.push_back(port);
    }
}

void GptpClock::remove_port(uint16_t port_number) {
    ports_.erase(std::remove_if(ports_.begin(), ports_.end(),
        [port_number](const std::shared_ptr<GptpPort>& port) {
            return port && port->get_port_number() == port_number;
        }), ports_.end());
}

std::shared_ptr<GptpPort> GptpClock::get_port(uint16_t port_number) const {
    for (const auto& port : ports_) {
        if (port && port->get_port_number() == port_number) {
            return port;
        }
    }
    return nullptr;
}

void GptpClock::update_from_master(const Timestamp& master_time, 
                                 const Timestamp& local_receipt_time,
                                 std::chrono::nanoseconds path_delay) {
    // Clock synchronization logic would go here
    // For now, just store the offset
    (void)master_time;
    (void)local_receipt_time;
    (void)path_delay;
}

void GptpClock::adjust_frequency(double ppb_adjustment) {
    // In a real implementation, this would adjust the local oscillator frequency
    // For simulation, we just log the adjustment
    std::cout << "[GptpClock] Frequency adjustment: " << ppb_adjustment << " ppb" << std::endl;
    
    // TODO: Apply frequency adjustment to hardware clock or OS time
    // This could involve:
    // - Writing to hardware registers for hardware timestamping NICs
    // - Using adjtimex() system call on Linux
    // - Using SetSystemTimeAdjustment() on Windows
}

void GptpClock::adjust_phase(double nanoseconds_adjustment) {
    // In a real implementation, this would step the local clock
    // For simulation, we just log the adjustment
    std::cout << "[GptpClock] Phase adjustment: " << nanoseconds_adjustment << " ns" << std::endl;
    
    // Apply phase adjustment to our local offset
    time_offset_ += std::chrono::nanoseconds(static_cast<int64_t>(nanoseconds_adjustment));
    
    // TODO: Apply phase step to hardware clock or OS time
    // This could involve:
    // - Immediate time step for large adjustments
    // - Gradual adjustment for small phase corrections
}

} // namespace gptp
