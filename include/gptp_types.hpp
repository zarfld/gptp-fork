#pragma once

#include <string>
#include <chrono>
#include <stdexcept>

// For older compilers that don't support std::optional
#if __cplusplus >= 201703L
    #include <optional>
    #define GPTP_OPTIONAL std::optional
#else
    // Simple optional implementation for C++14/11 compatibility
    template<typename T>
    class optional {
    public:
        optional() : has_value_(false) {}
        optional(const T& value) : value_(value), has_value_(true) {}
        optional(T&& value) : value_(std::move(value)), has_value_(true) {}
        
        bool has_value() const noexcept { return has_value_; }
        const T& value() const { 
            if (!has_value_) throw std::runtime_error("bad optional access");
            return value_; 
        }
        T& value() { 
            if (!has_value_) throw std::runtime_error("bad optional access");
            return value_; 
        }
        
    private:
        T value_;
        bool has_value_;
    };
    #define GPTP_OPTIONAL optional
#endif

namespace gptp {

    // Modern type aliases for better readability
    using MacAddress = std::string;
    using InterfaceName = std::string;
    
    // Error handling with std::optional and result types
    enum class ErrorCode {
        SUCCESS = 0,
        INTERFACE_NOT_FOUND,
        TIMESTAMPING_NOT_SUPPORTED,
        NETWORK_ERROR,
        INVALID_PARAMETER,
        INSUFFICIENT_PRIVILEGES,
        INITIALIZATION_FAILED
    };

    template<typename T>
    class Result {
    public:
        explicit Result(T value) : value_(std::move(value)), error_(ErrorCode::SUCCESS) {}
        explicit Result(ErrorCode error) : error_(error) {}

        // Static factory methods
        static Result<T> success(T value) {
            return Result<T>(std::move(value));
        }
        
        static Result<T> error(ErrorCode error_code) {
            return Result<T>(error_code);
        }
        
        // Support string-based errors for compatibility
        static Result<T> error(const std::string& error_message) {
            // Map common error messages to ErrorCode
            if (error_message.find("not found") != std::string::npos || 
                error_message.find("not initialized") != std::string::npos) {
                return Result<T>(ErrorCode::INTERFACE_NOT_FOUND);
            } else if (error_message.find("Failed") != std::string::npos || 
                       error_message.find("Error") != std::string::npos) {
                return Result<T>(ErrorCode::NETWORK_ERROR);
            } else if (error_message.find("Timeout") != std::string::npos) {
                return Result<T>(ErrorCode::NETWORK_ERROR);
            }
            return Result<T>(ErrorCode::NETWORK_ERROR);  // Default fallback
        }

        bool is_success() const noexcept { return error_ == ErrorCode::SUCCESS; }
        bool has_error() const noexcept { return error_ != ErrorCode::SUCCESS; }
        
        ErrorCode error() const noexcept { return error_; }
        
        const T& value() const {
            if (has_error()) {
                throw std::runtime_error("Accessing value of failed result");
            }
            return value_.value();
        }
        
        T& value() {
            if (has_error()) {
                throw std::runtime_error("Accessing value of failed result");
            }
            return value_.value();
        }

    private:
        GPTP_OPTIONAL<T> value_;
        ErrorCode error_;
    };

    // Platform-agnostic timestamp capabilities
    struct TimestampCapabilities {
        bool hardware_timestamping_supported = false;
        bool software_timestamping_supported = false;
        bool transmit_timestamping = false;
        bool receive_timestamping = false;
        bool tagged_transmit = false;
        bool all_transmit = false;
        bool all_receive = false;
    };

    // Network interface information
    struct NetworkInterface {
        InterfaceName name;           // Friendly name (e.g., "Ethernet", "Ethernet 3")
        std::string guid;            // Windows GUID identifier
        std::string description;     // Hardware description
        MacAddress mac_address;
        bool is_active = false;
        TimestampCapabilities capabilities;
    };

} // namespace gptp
