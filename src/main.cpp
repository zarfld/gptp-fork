#include "core/timestamp_provider.hpp"
#include "utils/logger.hpp"
#include <vector>
#include <string>
#include <memory>
#include <exception>

namespace gptp {

    /**
     * @brief Modern gPTP application class with RAII and proper error handling
     */
    class GptpApplication {
    public:
        GptpApplication() = default;
        ~GptpApplication() = default;

        // Delete copy semantics
        GptpApplication(const GptpApplication&) = delete;
        GptpApplication& operator=(const GptpApplication&) = delete;

        // Allow move semantics
        GptpApplication(GptpApplication&&) noexcept = default;
        GptpApplication& operator=(GptpApplication&&) noexcept = default;

        /**
         * @brief Initialize the gPTP application
         * @return ErrorCode indicating success or failure
         */
        ErrorCode initialize() {
            LOG_INFO("Initializing gPTP application...");

            timestamp_provider_ = create_timestamp_provider();
            if (!timestamp_provider_) {
                LOG_FATAL("Failed to create timestamp provider for this platform");
                LOG_INFO("Supported platforms: Windows (complete), Linux (basic support)");
                
#ifdef _WIN32
                LOG_INFO("Current platform: Windows");
#elif defined(__linux__)
                LOG_INFO("Current platform: Linux");
#else
                LOG_INFO("Current platform: Unsupported");
#endif
                
                return ErrorCode::INITIALIZATION_FAILED;
            }

            auto init_result = timestamp_provider_->initialize();
            if (init_result.has_error()) {
                LOG_FATAL("Failed to initialize timestamp provider: {}", 
                         static_cast<int>(init_result.error()));
                return init_result.error();
            }

            LOG_INFO("gPTP application initialized successfully");
            return ErrorCode::SUCCESS;
        }

        /**
         * @brief Run the main application logic
         * @param interface_name Network interface to use (optional)
         * @return ErrorCode indicating success or failure
         */
        ErrorCode run(const std::string& interface_name = "") {
            LOG_INFO("Starting gPTP daemon...");

            if (!interface_name.empty()) {
                return run_for_interface(interface_name);
            } else {
                return run_for_all_interfaces();
            }
        }

        /**
         * @brief Shutdown the application gracefully
         */
        void shutdown() {
            LOG_INFO("Shutting down gPTP application...");
            if (timestamp_provider_) {
                timestamp_provider_->cleanup();
            }
            LOG_INFO("gPTP application shutdown complete");
        }

    private:
        std::unique_ptr<ITimestampProvider> timestamp_provider_;

        ErrorCode run_for_interface(const std::string& interface_name) {
            LOG_INFO("Running gPTP for interface: {}", interface_name);

            auto caps_result = timestamp_provider_->get_timestamp_capabilities(interface_name);
            if (caps_result.has_error()) {
                LOG_ERROR("Failed to get timestamp capabilities for interface {}: {}", 
                         interface_name, static_cast<int>(caps_result.error()));
                return caps_result.error();
            }

            const auto& caps = caps_result.value();
            log_timestamp_capabilities(interface_name, caps);

            // Here you would implement the actual gPTP protocol logic
            LOG_INFO("gPTP protocol would run here for interface {}", interface_name);

            return ErrorCode::SUCCESS;
        }

        ErrorCode run_for_all_interfaces() {
            LOG_INFO("Discovering network interfaces...");

            auto interfaces_result = timestamp_provider_->get_network_interfaces();
            if (interfaces_result.has_error()) {
                LOG_ERROR("Failed to get network interfaces: {}", 
                         static_cast<int>(interfaces_result.error()));
                return interfaces_result.error();
            }

            const auto& interfaces = interfaces_result.value();
            LOG_INFO("Found {} network interfaces", interfaces.size());

            for (const auto& interface : interfaces) {
                LOG_INFO("Interface: {} (MAC: {})", interface.name, interface.mac_address);
                log_timestamp_capabilities(interface.name, interface.capabilities);
            }

            // Here you would implement logic to select the best interface
            // and run the gPTP protocol
            LOG_INFO("gPTP protocol would run here for selected interfaces");

            return ErrorCode::SUCCESS;
        }

        void log_timestamp_capabilities(const std::string& interface_name, 
                                      const TimestampCapabilities& caps) {
            LOG_INFO("Timestamp capabilities for {}:", interface_name);
            LOG_INFO("  Hardware timestamping: {}", caps.hardware_timestamping_supported ? "Yes" : "No");
            LOG_INFO("  Software timestamping: {}", caps.software_timestamping_supported ? "Yes" : "No");
            LOG_INFO("  Transmit timestamping: {}", caps.transmit_timestamping ? "Yes" : "No");
            LOG_INFO("  Receive timestamping: {}", caps.receive_timestamping ? "Yes" : "No");
            LOG_INFO("  Tagged transmit: {}", caps.tagged_transmit ? "Yes" : "No");
            LOG_INFO("  All transmit: {}", caps.all_transmit ? "Yes" : "No");
            LOG_INFO("  All receive: {}", caps.all_receive ? "Yes" : "No");
        }
    };

} // namespace gptp

int main(int argc, char* argv[]) {
    using namespace gptp;

    // Set up logging
    Logger::instance().set_level(LogLevel::INFO);
    
    LOG_INFO("gPTP Daemon v1.0.0");
    LOG_INFO("===================");

    try {
        GptpApplication app;
        
        auto init_result = app.initialize();
        if (init_result != ErrorCode::SUCCESS) {
            LOG_FATAL("Application initialization failed");
            return static_cast<int>(init_result);
        }

        std::string interface_name;
        if (argc > 1) {
            interface_name = argv[1];
            LOG_INFO("Using specified interface: {}", interface_name);
        } else {
            LOG_INFO("No interface specified, will scan all interfaces");
        }

        auto run_result = app.run(interface_name);
        if (run_result != ErrorCode::SUCCESS) {
            LOG_ERROR("Application run failed with error: {}", static_cast<int>(run_result));
            app.shutdown();
            return static_cast<int>(run_result);
        }

        app.shutdown();
        return 0;

    } catch (const std::exception& e) {
        LOG_FATAL("Unhandled exception: {}", e.what());
        return -1;
    } catch (...) {
        LOG_FATAL("Unhandled unknown exception");
        return -1;
    }
}
