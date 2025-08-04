#pragma once

#include <string>
#include <chrono>

namespace gptp {

    /**
     * @brief Modern configuration system for gPTP daemon
     * 
     * This class handles all configuration parameters using modern C++
     * features and provides type-safe access to configuration values.
     */
    class Configuration {
    public:
        static Configuration& instance() {
            static Configuration config;
            return config;
        }

        // Network configuration
        struct NetworkConfig {
            std::string preferred_interface;
            bool auto_select_interface = true;
            int sync_interval_ms = 125;  // 125ms = 8 per second (standard)
            int announce_interval_ms = 1000;  // 1 second
            int pdelay_req_interval_ms = 1000;  // 1 second
            bool hardware_timestamping_preferred = true;
            int max_interfaces = 10;
        } network;

        // Timing configuration
        struct TimingConfig {
            std::chrono::nanoseconds clock_accuracy{100000}; // 100 microseconds
            std::chrono::nanoseconds offset_scaled_log_variance{0x4E5D}; // Standard value
            int priority1 = 248;  // Default priority
            int priority2 = 248;  // Default priority
            bool grandmaster_capable = false;
            bool two_step_flag = true;
        } timing;

        // Logging configuration
        struct LoggingConfig {
            std::string log_level = "INFO";
            bool console_output = true;
            bool file_output = false;
            std::string log_file_path = "gptp.log";
            int max_log_file_size_mb = 10;
            int max_log_files = 5;
        } logging;

        // System configuration
        struct SystemConfig {
            bool run_as_service = false;
            bool enable_statistics = true;
            int statistics_interval_ms = 5000;
            bool enable_performance_monitoring = false;
        } system;

        /**
         * @brief Load configuration from file
         * @param config_file Path to configuration file
         * @return true if successful, false otherwise
         */
        bool load_from_file(const std::string& config_file);

        /**
         * @brief Save current configuration to file
         * @param config_file Path to configuration file
         * @return true if successful, false otherwise
         */
        bool save_to_file(const std::string& config_file) const;

        /**
         * @brief Load configuration from environment variables
         */
        void load_from_environment();

        /**
         * @brief Validate configuration values
         * @return true if configuration is valid, false otherwise
         */
        bool validate() const;

    private:
        Configuration() {
            load_defaults();
        }

        static void load_defaults();
    };

} // namespace gptp
