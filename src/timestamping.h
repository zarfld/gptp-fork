#ifndef TIMESTAMPING_H
#define TIMESTAMPING_H

#include <stdio.h>
#include <cstdint> // Stellt sicher, dass UINT32 definiert ist
#include <Windows.h> // Definiert grundlegende Windows-Datentypen und Funktionen
#include <Iphlpapi.h> // Definiert Netzwerk- und IP-Hilfsfunktionen und -strukturen
#include <winapifamily.h> // Definiert die Windows-API-Familienpartitionen
#include <ipifcons.h> // Definiert Netzwerkschnittstellenkonstanten und -typen
#include <cstdlib>
#pragma comment(lib, "Iphlpapi.lib")

// Wrapper around the Windows API function to avoid name collision
DWORD GetInterfaceActiveTimestampCapabilitiesWrapper(
    NET_LUID *InterfaceLuid,
    MIB_INTERFACE_TIMESTAMP_CAPABILITIES *TimestampCapabilities);
DWORD IntegrateIntelHardwareTimestampingWithPacketTimestamping(NET_LUID *InterfaceLuid, MIB_INTERFACE_TIMESTAMP_CAPABILITIES *TimestampCapabilities);

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

#endif // TIMESTAMPING_H
