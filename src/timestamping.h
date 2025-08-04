#ifndef TIMESTAMPING_H
#define TIMESTAMPING_H

#include <stdio.h>
#include <cstdint> // Stellt sicher, dass UINT32 definiert ist
#include <cstdlib>

#ifdef _WIN32
#include <Windows.h> // Definiert grundlegende Windows-Datentypen und Funktionen
#include <Iphlpapi.h> // Definiert Netzwerk- und IP-Hilfsfunktionen und -strukturen
#include <winapifamily.h> // Definiert die Windows-API-Familienpartitionen
#include <ipifcons.h> // Definiert Netzwerkschnittstellenkonstanten und -typen
#pragma comment(lib, "Iphlpapi.lib")

DWORD GetNetworkInterfaceTimestampCapabilities(NET_LUID *InterfaceLuid, INTERFACE_TIMESTAMP_CAPABILITIES *TimestampCapabilities);
DWORD IntegrateIntelHardwareTimestampingWithPacketTimestamping(NET_LUID *InterfaceLuid, INTERFACE_TIMESTAMP_CAPABILITIES *TimestampCapabilities);

// Ensure that the NPCAP_DIR environment variable is correctly set for Visual Studio 2022
extern const char* NPCAP_DIR;

// Add the VSCMD_DEBUG environment variable set to 3 to enable detailed logging for Visual Studio command-line tools
extern const char* VSCMD_DEBUG;

// Add the VSCMD_SKIP_SENDTELEMETRY environment variable set to 1 to disable telemetry data collection by Visual Studio command-line tools
extern const char* VSCMD_SKIP_SENDTELEMETRY;
#endif // _WIN32

#endif // TIMESTAMPING_H
