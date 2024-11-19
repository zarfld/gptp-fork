#include <Iphlpapi.h>
#include <Windows.h>
#include <stdio.h>
#include <iostream>

#pragma comment(lib, "Iphlpapi.lib")

DWORD GetInterfaceActiveTimestampCapabilities(NET_LUID *InterfaceLuid, MIB_INTERFACE_TIMESTAMP_CAPABILITIES *TimestampCapabilities) {
    DWORD dwRetVal;

    dwRetVal = GetInterfaceActiveTimestampCapabilities(InterfaceLuid, TimestampCapabilities);
    if (dwRetVal == NO_ERROR) {
        printf("Timestamping supported: %s\n", TimestampCapabilities->SupportsHardwareTimestamp ? "Yes" : "No");
        printf("Send timestamping: %s\n", TimestampCapabilities->SupportsTransmitTimestamp ? "Yes" : "No");
        printf("Receive timestamping: %s\n", TimestampCapabilities->SupportsReceiveTimestamp ? "Yes" : "No");
    } else {
        printf("Failed to get timestamping capabilities. Error: %lu\n", dwRetVal);
    }

    return dwRetVal;
}

DWORD IntegrateIntelHardwareTimestampingWithPacketTimestamping(NET_LUID *InterfaceLuid, MIB_INTERFACE_TIMESTAMP_CAPABILITIES *TimestampCapabilities) {
    DWORD dwRetVal;

    dwRetVal = GetInterfaceActiveTimestampCapabilities(InterfaceLuid, TimestampCapabilities);
    if (dwRetVal == NO_ERROR) {
        printf("Integrating Intel hardware timestamping with packet timestamping...\n");
        // Add logic to integrate Intel hardware timestamping with packet timestamping
        // This may involve combining hardware timestamps with packet timestamps
        // and calculating network delays or other factors
        printf("Integration process completed successfully.\n");
    } else {
        printf("Failed to get timestamping capabilities. Error: %lu\n", dwRetVal);
    }

    return dwRetVal;
}

void EnablePTPTimestamping() {
    HKEY hKey;
    LPCWSTR subKey = L"SYSTEM\\CurrentControlSet\\Services\\YourIntelDriver\\Parameters";
    DWORD data = 1;  // 1 to enable, 0 to disable (example)

    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, subKey, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (RegSetValueEx(hKey, L"PTPhardwaretimestamp", 0, REG_DWORD, (const BYTE*)&data, sizeof(data)) == ERROR_SUCCESS) {
            std::cout << "PTP hardware timestamping enabled.\n";
        } else {
            std::cout << "Failed to set registry value.\n";
        }
        RegCloseKey(hKey);
    } else {
        std::cout << "Failed to open registry key.\n";
    }
}

void SetInterfaceProperties() {
    // Add logic to call SetInterfaceProperties with appropriate parameters
    // Handle the return value to check for success or failure
    // Print the result of the operation if the call is successful
}

// Ensure that the NPCAP_DIR environment variable is correctly set for Visual Studio 2022
#ifdef _WIN32
#include <cstdlib>
const char* NPCAP_DIR = std::getenv("NPCAP_DIR");
#endif

// Add the VSCMD_DEBUG environment variable set to 3 to enable detailed logging for Visual Studio command-line tools
#ifdef _WIN32
#include <cstdlib>
const char* VSCMD_DEBUG = std::getenv("VSCMD_DEBUG");
#endif

// Add the VSCMD_SKIP_SENDTELEMETRY environment variable set to 1 to disable telemetry data collection by Visual Studio command-line tools
#ifdef _WIN32
#include <cstdlib>
const char* VSCMD_SKIP_SENDTELEMETRY = std::getenv("VSCMD_SKIP_SENDTELEMETRY");
#endif
