#ifndef TIMESTAMPING_H
#define TIMESTAMPING_H

#include <Iphlpapi.h>
#include <Windows.h>

#pragma comment(lib, "Iphlpapi.lib")

DWORD GetInterfaceActiveTimestampCapabilities(NET_LUID *InterfaceLuid, MIB_INTERFACE_TIMESTAMP_CAPABILITIES *TimestampCapabilities);
DWORD IntegrateIntelHardwareTimestampingWithPacketTimestamping(NET_LUID *InterfaceLuid, MIB_INTERFACE_TIMESTAMP_CAPABILITIES *TimestampCapabilities);

// Ensure that the WPCAP_DIR environment variable is correctly set for Visual Studio 2022
#ifdef _WIN32
#include <cstdlib>
const char* WPCAP_DIR = std::getenv("WPCAP_DIR");
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
