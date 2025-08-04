#pragma once

#include <string>
#include <sstream>
#include <iostream>
#include <chrono>
#include <iomanip>

namespace gptp {

    enum class LogLevel {
        TRACE = 0,
        DEBUG = 1,
        INFO = 2,
        WARN = 3,
        ERROR = 4,
        FATAL = 5
    };

    /**
     * @brief Simple, modern logging framework for gPTP
     * 
     * This provides a lightweight logging solution with different log levels,
     * formatted output, and thread-safe operation.
     */
    class Logger {
    public:
        static Logger& instance() {
            static Logger logger;
            return logger;
        }

        void set_level(LogLevel level) {
            current_level_ = level;
        }

        LogLevel get_level() const {
            return current_level_;
        }

        template<typename... Args>
        void log(LogLevel level, const std::string& format, Args&&... args) {
            if (level < current_level_) {
                return;
            }

            std::string message = format_message(format, std::forward<Args>(args)...);
            output_message(level, message);
        }

        // Convenience methods
        template<typename... Args>
        void trace(const std::string& format, Args&&... args) {
            log(LogLevel::TRACE, format, std::forward<Args>(args)...);
        }

        template<typename... Args>
        void debug(const std::string& format, Args&&... args) {
            log(LogLevel::DEBUG, format, std::forward<Args>(args)...);
        }

        template<typename... Args>
        void info(const std::string& format, Args&&... args) {
            log(LogLevel::INFO, format, std::forward<Args>(args)...);
        }

        template<typename... Args>
        void warn(const std::string& format, Args&&... args) {
            log(LogLevel::WARN, format, std::forward<Args>(args)...);
        }

        template<typename... Args>
        void error(const std::string& format, Args&&... args) {
            log(LogLevel::ERROR, format, std::forward<Args>(args)...);
        }

        template<typename... Args>
        void fatal(const std::string& format, Args&&... args) {
            log(LogLevel::FATAL, format, std::forward<Args>(args)...);
        }

    private:
        Logger() : current_level_(LogLevel::INFO) {}
        
        LogLevel current_level_;

        std::string get_timestamp() const {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) % 1000;

            std::stringstream ss;
            ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
            ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
            return ss.str();
        }

        std::string level_to_string(LogLevel level) const {
            switch (level) {
                case LogLevel::TRACE: return "TRACE";
                case LogLevel::DEBUG: return "DEBUG";
                case LogLevel::INFO:  return "INFO ";
                case LogLevel::WARN:  return "WARN ";
                case LogLevel::ERROR: return "ERROR";
                case LogLevel::FATAL: return "FATAL";
                default: return "UNKNOWN";
            }
        }

        template<typename... Args>
        std::string format_message(const std::string& format, Args&&... args) {
            // Simple string formatting - in a real implementation you might want
            // to use fmt library or similar for more advanced formatting
            std::stringstream ss;
            format_impl(ss, format, std::forward<Args>(args)...);
            return ss.str();
        }

        void format_impl(std::stringstream& ss, const std::string& format) {
            ss << format;
        }

        template<typename T, typename... Args>
        void format_impl(std::stringstream& ss, const std::string& format, T&& arg, Args&&... args) {
            size_t pos = format.find("{}");
            if (pos != std::string::npos) {
                ss << format.substr(0, pos) << std::forward<T>(arg);
                format_impl(ss, format.substr(pos + 2), std::forward<Args>(args)...);
            } else {
                ss << format << " " << std::forward<T>(arg);
                format_impl(ss, "", std::forward<Args>(args)...);
            }
        }

        void output_message(LogLevel level, const std::string& message) {
            // Thread-safe output (cout is thread-safe for individual operations)
            std::cout << "[" << get_timestamp() << "] [" << level_to_string(level) 
                      << "] " << message << std::endl;
        }
    };

    // Global logger macros for convenience
    #define LOG_TRACE(...) gptp::Logger::instance().trace(__VA_ARGS__)
    #define LOG_DEBUG(...) gptp::Logger::instance().debug(__VA_ARGS__)
    #define LOG_INFO(...)  gptp::Logger::instance().info(__VA_ARGS__)
    #define LOG_WARN(...)  gptp::Logger::instance().warn(__VA_ARGS__)
    #define LOG_ERROR(...) gptp::Logger::instance().error(__VA_ARGS__)
    #define LOG_FATAL(...) gptp::Logger::instance().fatal(__VA_ARGS__)

} // namespace gptp
