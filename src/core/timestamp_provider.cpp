#include "timestamp_provider.hpp"

#ifdef _WIN32
    #include "../platform/windows_timestamp_provider.hpp"
#endif

// For future Linux implementation
#ifdef __linux__
    // #include "../platform/linux_timestamp_provider.hpp"
#endif

namespace gptp {

    std::unique_ptr<ITimestampProvider> create_timestamp_provider() {
#ifdef _WIN32
        return std::make_unique<WindowsTimestampProvider>();
#elif defined(__linux__)
        // Future Linux implementation
        // return std::make_unique<LinuxTimestampProvider>();
        return nullptr; // Placeholder
#else
        return nullptr; // Unsupported platform
#endif
    }

} // namespace gptp
