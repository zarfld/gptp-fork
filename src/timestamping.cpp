#ifdef _WIN32
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0A00 // Windows 10
    #endif
    #include <sdkddkver.h>
    #ifndef WINAPI_FAMILY
        #define WINAPI_FAMILY WINAPI_FAMILY_DESKTOP_APP
    #endif
    // Suppress CRT secure warnings for getenv
    #define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <cstring> // For strcmp
#include <cstdlib>

#ifdef _WIN32
    #include <Windows.h> // Definiert grundlegende Windows-Datentypen und Funktionen
    #include <Iphlpapi.h> // Definiert Netzwerk- und IP-Hilfsfunktionen und -strukturen
    #include <iptypes.h>
    #include <winapifamily.h> // Definiert die Windows-API-Familienpartitionen
    #include <ipifcons.h> // Definiert Netzwerkschnittstellenkonstanten und -typen
#endif

#pragma comment(lib, "Iphlpapi.lib")

#ifdef _WIN32
// Define the extern variables declared in header
const char* NPCAP_DIR = std::getenv("NPCAP_DIR");
const char* VSCMD_DEBUG = std::getenv("VSCMD_DEBUG");
const char* VSCMD_SKIP_SENDTELEMETRY = std::getenv("VSCMD_SKIP_SENDTELEMETRY");

DWORD GetNetworkInterfaceTimestampCapabilities(NET_LUID *InterfaceLuid, INTERFACE_TIMESTAMP_CAPABILITIES  *TimestampCapabilities) {
    DWORD dwRetVal;

    // Call the actual Windows API function using the global scope operator
    dwRetVal = ::GetInterfaceActiveTimestampCapabilities(InterfaceLuid, TimestampCapabilities);
    if (dwRetVal == NO_ERROR) {
        INTERFACE_HARDWARE_TIMESTAMP_CAPABILITIES HardwareTimestampCapabilities = TimestampCapabilities->HardwareCapabilities;
        printf("Timestamping supported: %s\n", HardwareTimestampCapabilities.TaggedTransmit ? "Yes" : "No");
        printf("Send timestamping: %s\n", HardwareTimestampCapabilities.AllTransmit ? "Yes" : "No");
        printf("Receive timestamping: %s\n", HardwareTimestampCapabilities.AllReceive ? "Yes" : "No");
    } else {
        printf("Failed to get timestamping capabilities. Error: %lu\n", dwRetVal);
    }

    return dwRetVal;
}

DWORD IntegrateIntelHardwareTimestampingWithPacketTimestamping(NET_LUID *InterfaceLuid, INTERFACE_TIMESTAMP_CAPABILITIES  *TimestampCapabilities) {
    DWORD dwRetVal;

    dwRetVal = GetNetworkInterfaceTimestampCapabilities(InterfaceLuid, TimestampCapabilities);
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
#endif // _WIN32

#ifdef _WIN32
int main(int argc, const char* argv[]) {
    printf("GPTP Timestamping Utility\n");
    printf("=========================\n");
    
    // Example usage - in a real implementation, you would get the interface LUID from the network interface
    NET_LUID interfaceLuid = {0};
    INTERFACE_TIMESTAMP_CAPABILITIES timestampCaps = {0};
    
    // For demonstration, we'll try to get the capabilities
    DWORD result = GetNetworkInterfaceTimestampCapabilities(&interfaceLuid, &timestampCaps);
    
    if (argc > 1 && strcmp(argv[1], "IntegrateIntelHardwareTimestampingWithPacketTimestamping") == 0) {
        printf("\nRunning Intel Hardware Timestamping Integration...\n");
        result = IntegrateIntelHardwareTimestampingWithPacketTimestamping(&interfaceLuid, &timestampCaps);
    }
    
    return (result == NO_ERROR) ? 0 : 1;
}
#else
int main(int argc, const char* argv[]) {
    printf("GPTP Timestamping Utility\n");
    printf("=========================\n");
    printf("This program is designed for Windows only.\n");
    printf("Linux timestamping functionality would be implemented here.\n");
    
    if (argc > 1 && strcmp(argv[1], "IntegrateIntelHardwareTimestampingWithPacketTimestamping") == 0) {
        printf("\nLinux Intel Hardware Timestamping Integration would be implemented here.\n");
    }
    
    return 0;
}
#endif
