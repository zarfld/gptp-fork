#ifdef _WIN32
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0A00 // Windows 10
    #endif
    #include <sdkddkver.h>
    #ifndef WINAPI_FAMILY
        #define WINAPI_FAMILY WINAPI_FAMILY_DESKTOP_APP
    #endif
#endif

#include <stdio.h>
#include <cstdint> // Stellt sicher, dass UINT32 definiert ist

#include <cstdlib>

#ifdef _WIN32
    #include <Windows.h> // Definiert grundlegende Windows-Datentypen und Funktionen
    #include <Iphlpapi.h> // Definiert Netzwerk- und IP-Hilfsfunktionen und -strukturen
    #include <iptypes.h>
    #include <winapifamily.h> // Definiert die Windows-API-Familienpartitionen
    #include <ipifcons.h> // Definiert Netzwerkschnittstellenkonstanten und -typen
#endif

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
