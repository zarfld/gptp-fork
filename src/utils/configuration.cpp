#include "configuration.hpp"
#include "logger.hpp"
#include <cstdlib>
#include <fstream>

namespace gptp {

    void Configuration::load_defaults() {
        // Defaults are already set in the struct definitions
        LOG_DEBUG("Configuration loaded with default values");
    }

    bool Configuration::load_from_file(const std::string& config_file) {
        std::ifstream file(config_file);
        if (!file.is_open()) {
            LOG_WARN("Could not open configuration file: {}", config_file);
            return false;
        }

        std::string line;
        while (std::getline(file, line)) {
            // Skip comments and empty lines
            if (line.empty() || line[0] == '#') {
                continue;
            }

            // Simple key=value parser
            size_t eq_pos = line.find('=');
            if (eq_pos == std::string::npos) {
                continue;
            }

            std::string key = line.substr(0, eq_pos);
            std::string value = line.substr(eq_pos + 1);

            // Trim whitespace (simple implementation)
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            // Parse configuration values
            if (key == "preferred_interface") {
                network.preferred_interface = value;
            } else if (key == "auto_select_interface") {
                network.auto_select_interface = (value == "true" || value == "1");
            } else if (key == "sync_interval_ms") {
                network.sync_interval_ms = std::stoi(value);
            } else if (key == "announce_interval_ms") {
                network.announce_interval_ms = std::stoi(value);
            } else if (key == "hardware_timestamping_preferred") {
                network.hardware_timestamping_preferred = (value == "true" || value == "1");
            } else if (key == "log_level") {
                logging.log_level = value;
            } else if (key == "console_output") {
                logging.console_output = (value == "true" || value == "1");
            } else if (key == "file_output") {
                logging.file_output = (value == "true" || value == "1");
            } else if (key == "log_file_path") {
                logging.log_file_path = value;
            } else if (key == "run_as_service") {
                system.run_as_service = (value == "true" || value == "1");
            } else if (key == "enable_statistics") {
                system.enable_statistics = (value == "true" || value == "1");
            } else {
                LOG_WARN("Unknown configuration key: {}", key);
            }
        }

        LOG_INFO("Configuration loaded from file: {}", config_file);
        return true;
    }

    bool Configuration::save_to_file(const std::string& config_file) const {
        std::ofstream file(config_file);
        if (!file.is_open()) {
            LOG_ERROR("Could not open configuration file for writing: {}", config_file);
            return false;
        }

        file << "# gPTP Daemon Configuration\n";
        file << "# Generated automatically\n\n";

        file << "# Network Configuration\n";
        file << "preferred_interface=" << network.preferred_interface << "\n";
        file << "auto_select_interface=" << (network.auto_select_interface ? "true" : "false") << "\n";
        file << "sync_interval_ms=" << network.sync_interval_ms << "\n";
        file << "announce_interval_ms=" << network.announce_interval_ms << "\n";
        file << "hardware_timestamping_preferred=" << (network.hardware_timestamping_preferred ? "true" : "false") << "\n";

        file << "\n# Logging Configuration\n";
        file << "log_level=" << logging.log_level << "\n";
        file << "console_output=" << (logging.console_output ? "true" : "false") << "\n";
        file << "file_output=" << (logging.file_output ? "true" : "false") << "\n";
        file << "log_file_path=" << logging.log_file_path << "\n";

        file << "\n# System Configuration\n";
        file << "run_as_service=" << (system.run_as_service ? "true" : "false") << "\n";
        file << "enable_statistics=" << (system.enable_statistics ? "true" : "false") << "\n";

        LOG_INFO("Configuration saved to file: {}", config_file);
        return true;
    }

    void Configuration::load_from_environment() {
        const char* env_value;

        // Check environment variables
        if ((env_value = std::getenv("GPTP_INTERFACE")) != nullptr) {
            network.preferred_interface = env_value;
        }

        if ((env_value = std::getenv("GPTP_LOG_LEVEL")) != nullptr) {
            logging.log_level = env_value;
        }

        if ((env_value = std::getenv("GPTP_SYNC_INTERVAL")) != nullptr) {
            network.sync_interval_ms = std::stoi(env_value);
        }

        if ((env_value = std::getenv("GPTP_HARDWARE_TS")) != nullptr) {
            network.hardware_timestamping_preferred = (std::string(env_value) == "true" || std::string(env_value) == "1");
        }

        LOG_DEBUG("Configuration loaded from environment variables");
    }

    bool Configuration::validate() const {
        bool valid = true;

        // Validate network configuration
        if (network.sync_interval_ms <= 0 || network.sync_interval_ms > 10000) {
            LOG_ERROR("Invalid sync_interval_ms: {}", network.sync_interval_ms);
            valid = false;
        }

        if (network.announce_interval_ms <= 0 || network.announce_interval_ms > 60000) {
            LOG_ERROR("Invalid announce_interval_ms: {}", network.announce_interval_ms);
            valid = false;
        }

        // Validate logging configuration
        if (logging.log_level != "TRACE" && logging.log_level != "DEBUG" && 
            logging.log_level != "INFO" && logging.log_level != "WARN" && 
            logging.log_level != "ERROR" && logging.log_level != "FATAL") {
            LOG_ERROR("Invalid log_level: {}", logging.log_level);
            valid = false;
        }

        if (valid) {
            LOG_DEBUG("Configuration validation passed");
        } else {
            LOG_ERROR("Configuration validation failed");
        }

        return valid;
    }

} // namespace gptp
