#ifndef TIMESTAMPING_H
#define TIMESTAMPING_H

#include <Iphlpapi.h>
#include <Windows.h>

#pragma comment(lib, "Iphlpapi.lib")

DWORD GetInterfaceActiveTimestampCapabilities(NET_LUID *InterfaceLuid, MIB_INTERFACE_TIMESTAMP_CAPABILITIES *TimestampCapabilities);
DWORD IntegrateIntelHardwareTimestampingWithPacketTimestamping(NET_LUID *InterfaceLuid, MIB_INTERFACE_TIMESTAMP_CAPABILITIES *TimestampCapabilities);

#endif // TIMESTAMPING_H
