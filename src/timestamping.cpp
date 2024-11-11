#include <Iphlpapi.h>
#include <Windows.h>
#include <stdio.h>

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
