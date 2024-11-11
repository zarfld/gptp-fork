# gptp Daemon

This repository contains an example Intel-provided gptp daemon for clock synchronization on Linux and Windows platforms. The daemon communicates with other processes through a named pipe called "gptp-update" and uses a specific message format.

## Building and Running the gptp Daemon

### Linux

#### Build Dependencies

* `cmake`: Install using `sudo apt-get install cmake`
* `doxygen`: Install using `sudo apt-get install doxygen`
* `graphviz`: Install using `sudo apt-get install graphviz`
* `cppcheck`: Install using `sudo apt-get install cppcheck`
* `clang-tidy`: Install using `sudo apt-get install clang-tidy`

#### Build Instructions

1. Clone the repository from GitHub.
2. Navigate to the `linux` directory.
3. Execute the build makefile using the appropriate command:
   * For I210: `ARCH=I210 make clean all`
   * For generic Linux: `make clean all`
   * For Intel CE 5100 Platforms: `ARCH=IntelCE make clean all`
4. Run static code analysis tools:
   * `make static-analysis`
5. Run resource management checks:
   * `make resource-checks`

#### Run Instructions

To execute the daemon, run `./daemon_cl <interface-name>`, replacing `<interface-name>` with the name of your network interface (e.g., `eth0`).

### Windows

#### Build Dependencies

* `WinPCAP Developer's Pack (WpdPack)`: Download from WinPCAP Developer's Pack
* `CMAKE 3.2.2` or later
* `Microsoft Visual Studio 2013` or later
* Environment variable `WPCAP_DIR` must be defined to the directory where WinPcap is installed
* `WinPCAP` must also be installed on any machine where the daemon runs
* `cppcheck`: Install using `choco install cppcheck`
* `clang-tidy`: Install using `choco install llvm`

#### Build Instructions

1. Clone the repository from GitHub.
2. Open the project in Microsoft Visual Studio.
3. Configure the project to use the installed dependencies:
   * Set the `WPCAP_DIR` environment variable to the directory where WinPcap is installed.
4. Build the project using Microsoft Visual Studio.
5. Run static code analysis tools:
   * `cppcheck .`
   * `clang-tidy .`
6. Run resource management checks:
   * Use Visual Studio's built-in tools for resource management checks.

#### Run Instructions

To execute the daemon, run `daemon_cl.exe xx-xx-xx-xx-xx-xx`, replacing `xx-xx-xx-xx-xx-xx` with the MAC address of the local interface.

## Using IP Helper API for gPTP and Timestamping

The Windows IP Helper API (Iphlpapi) functions provide an extensive list of functions related to network configuration, management, and diagnostics. Here’s an overview of key areas relevant to implementing or extending gPTP or timestamping functionalities:

### Key Function Categories in IP Helper API

1. **Network Configuration and Interface Management**

   * `GetAdaptersInfo` and `GetIfTable`: Useful for retrieving information about network adapters. You could use these functions to detect and configure the Intel i210 or i226 network interfaces when setting up gPTP or timestamping.
   * `SetIfEntry`: Allows modifications to network interface settings, which can be useful for configuring network properties that affect timing or synchronization.

2. **Address Resolution and Routing**

   * `GetIpAddrTable` and `GetIpNetTable`: These functions allow access to IP addresses and ARP table entries, which could be helpful if your implementation requires knowledge of network topology or peer device addresses in the gPTP setup.
   * `GetBestRoute`: Can be used to determine optimal routing for PTP messages to minimize network delay.

3. **Network Statistics and Diagnostic Data**

   * `GetIfEntry` and `GetIfTable2`: Useful for monitoring interface statistics, which can help in diagnostics or logging for gPTP performance evaluation.
   * `GetPerTcpConnectionEStats`: Provides extended statistics for TCP connections, which might help with understanding network delays or traffic issues, though it is less directly related to PTP/gPTP.

4. **Packet Timestamping and Protocol Control**

   * `GetTcpStatistics` and `GetUdpStatistics`: These provide insights into TCP/UDP traffic, which can indirectly support gPTP operations by allowing you to monitor the network's load and performance.
   * `GetAdaptersAddresses`: A more advanced version of `GetAdaptersInfo`, which includes additional options for accessing timestamping capabilities when supported by the network adapter.

5. **Advanced IP and Network Functions**

   * `ConvertInterfaceIndexToLuid` and `ConvertInterfaceLuidToIndex`: These functions are useful when handling multiple network adapters, especially if you need to map between logical and physical interface identifiers.
   * `NotifyAddrChange`: Can be used to monitor network changes and handle re-synchronization if an interface is reconfigured or disrupted.

### Using IP Helper API with gPTP and Timestamping

While the IP Helper API itself does not implement gPTP, it provides necessary functions for managing network interfaces and retrieving adapter configurations, which can complement a gPTP driver or application. You might use it to:

* Set up and monitor network interfaces
* Retrieve hardware details and capabilities for specific NICs (such as Intel i210 and i226)
* Monitor network traffic and statistics to optimize time synchronization

### Key Considerations

For gPTP on Windows, the IP Helper API alone will not provide complete support. You would typically use IP Helper for network management while relying on custom drivers or other APIs (e.g., NDIS or PTP API) to handle the precise timestamping and synchronization aspects required by gPTP.

### Necessary Headers and Libraries

To use the IP Helper API, include the following headers in your code:

```c
#include <Iphlpapi.h>
#include <Windows.h>
```

Link against the `Iphlpapi.lib` library.

### Example: Using `GetAdaptersInfo` to Retrieve Network Adapter Information

Here is a brief example of how to use `GetAdaptersInfo` to retrieve information about network adapters, including the Intel i210:

```c
#include <stdio.h>
#include <stdlib.h>
#include <Iphlpapi.h>
#include <Windows.h>

#pragma comment(lib, "Iphlpapi.lib")

int main() {
    DWORD dwSize = 0;
    DWORD dwRetVal = 0;
    IP_ADAPTER_INFO *pAdapterInfo = NULL;
    IP_ADAPTER_INFO *pAdapter = NULL;

    // First call to GetAdaptersInfo to get the size needed
    if (GetAdaptersInfo(pAdapterInfo, &dwSize) == ERROR_BUFFER_OVERFLOW) {
        pAdapterInfo = (IP_ADAPTER_INFO *)malloc(dwSize);
    }

    // Second call to GetAdaptersInfo to get the actual data
    if ((dwRetVal = GetAdaptersInfo(pAdapterInfo, &dwSize)) == NO_ERROR) {
        pAdapter = pAdapterInfo;
        while (pAdapter) {
            printf("Adapter Name: %s\n", pAdapter->AdapterName);
            printf("Adapter Desc: %s\n", pAdapter->Description);
            if (strstr(pAdapter->Description, "Intel(R) Ethernet Connection I210")) {
                printf("Found Intel i210 adapter!\n");
                // Access and configure the Intel i210 adapter as needed
            }
            pAdapter = pAdapter->Next;
        }
    }

    if (pAdapterInfo) {
        free(pAdapterInfo);
    }

    return 0;
}
```

## Using `GetInterfaceActiveTimestampCapabilities` for Timestamping

The `GetInterfaceActiveTimestampCapabilities` function in the Windows IP Helper API checks if a network interface supports hardware timestamping, which is essential for applications like PTP or gPTP that require precise time synchronization.

### Key Points of `GetInterfaceActiveTimestampCapabilities`

1. **Function Purpose**: This function allows you to retrieve active timestamping capabilities for a specific network interface, identifying whether the hardware supports features like precise packet timestamping.

2. **Usage**: The function takes the `NET_LUID` (Locally Unique Identifier) or `InterfaceIndex` of a network adapter and fills a `MIB_INTERFACE_TIMESTAMP_CAPABILITIES` structure, which includes details on whether timestamping is supported and if both send and receive timestamps are available.

3. **Important Parameters**:
   * `NET_LUID` or `InterfaceIndex`: Uniquely identifies the network adapter you are querying.
   * `MIB_INTERFACE_TIMESTAMP_CAPABILITIES`: The structure populated with timestamping capability information.

4. **Return Value**: The function returns `NO_ERROR` if successful, or an error code if it fails (e.g., if the specified interface does not support timestamping).

### Example: Using `GetInterfaceActiveTimestampCapabilities`

Here is a brief example of how to use `GetInterfaceActiveTimestampCapabilities` to check if a network interface supports hardware timestamping:

```c
#include <stdio.h>
#include <Iphlpapi.h>
#include <Windows.h>

#pragma comment(lib, "Iphlpapi.lib")

int main() {
    NET_LUID InterfaceLuid;
    MIB_INTERFACE_TIMESTAMP_CAPABILITIES TimestampCapabilities;
    DWORD dwRetVal;

    // Assume InterfaceLuid is already obtained
    // For example, you can use ConvertInterfaceIndexToLuid to get InterfaceLuid from InterfaceIndex

    dwRetVal = GetInterfaceActiveTimestampCapabilities(&InterfaceLuid, &TimestampCapabilities);
    if (dwRetVal == NO_ERROR) {
        printf("Timestamping supported: %s\n", TimestampCapabilities.SupportsHardwareTimestamp ? "Yes" : "No");
        printf("Send timestamping: %s\n", TimestampCapabilities.SupportsTransmitTimestamp ? "Yes" : "No");
        printf("Receive timestamping: %s\n", TimestampCapabilities.SupportsReceiveTimestamp ? "Yes" : "No");
    } else {
        printf("Failed to get timestamping capabilities. Error: %lu\n", dwRetVal);
    }

    return 0;
}
```

### Practical Application

For implementing gPTP or similar protocols that rely on precise timing, `GetInterfaceActiveTimestampCapabilities` is a useful function to determine if a network adapter can provide the necessary hardware timestamping support. You can use this function as part of the initialization process to ensure that the chosen adapter meets the timing requirements of your application.

### See Also

The documentation’s “See Also” section lists other related functions and structures that are relevant for managing and retrieving timestamping and interface capabilities. Here are some of the notable entries:

* `GetAdaptersAddresses`: Retrieves comprehensive information about network adapters, which can help identify interfaces that might support timestamping.
* `ConvertInterfaceLuidToIndex` and `ConvertInterfaceIndexToLuid`: These functions convert between LUID and InterfaceIndex, both used to reference network interfaces.
* `MIB_INTERFACE_TIMESTAMP_CAPABILITIES`: The structure populated by `GetInterfaceActiveTimestampCapabilities`, which provides details on the timestamping abilities of the network interface.

## Configuring and Using W32Time for Improved Time Synchronization

To enable PTP support in W32Time, follow these steps:

1. Open the Registry Editor by typing `regedit` in the Run dialog (Win + R) and pressing Enter.
2. Navigate to the following registry key: `HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\W32Time\Config`.
3. Create or modify the `AnnounceFlags` DWORD value and set it to `5` to enable PTP.
4. Navigate to the following registry key: `HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\W32Time\TimeProviders\NtpClient`.
5. Create or modify the `Enabled` DWORD value and set it to `1` to enable the NTP client.
6. Create or modify the `SpecialPollInterval` DWORD value to set the polling interval in seconds (e.g., `900` for 15 minutes).
7. Navigate to the following registry key: `HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\W32Time\TimeProviders\NtpServer`.
8. Create or modify the `Enabled` DWORD value and set it to `1` to enable the NTP server.
9. Restart the Windows Time service by running the following commands in an elevated Command Prompt:
   * `net stop w32time`
   * `net start w32time`

## Integrating Intel Hardware Timestamping with Packet Timestamping

To integrate Intel hardware timestamping with packet timestamping, follow these steps:

1. Ensure that your network interface supports hardware timestamping by using the `GetInterfaceActiveTimestampCapabilities` function from the Windows IP Helper API. Refer to the implementation in `src/timestamping.cpp` and the corresponding header file `src/timestamping.h` for an example of how to use this function.
2. Enable Intel hardware timestamping on compatible Intel NICs (such as the i210) by following the guidance from Intel. This may involve configuring the network adapter settings and ensuring that the appropriate drivers are installed.
3. Use packet timestamping to timestamp packets as they are sent or received. This can be done by leveraging the Windows IP Helper API functions and integrating them into your custom application or daemon.
4. Combine the hardware timestamps obtained from the Intel NIC with the packet timestamps to calculate network delays and other factors. This can be useful for diagnostics, performance analysis, or synchronization tasks.
5. Regularly calibrate timing against a reliable source to account for packet delay variations and driver limitations that can introduce drift.

## Best Practices for Timing Calibration

To achieve accurate timing calibration, consider the following best practices:

* **Use a reliable time source**: Regularly calibrate your system against a reliable and accurate time source, such as a GPS clock or a high-precision NTP server.
* **Minimize network delays**: Reduce network delays by optimizing network configurations and using high-quality network hardware. This helps in achieving more accurate time synchronization.
* **Leverage hardware timestamping**: Utilize hardware timestamping capabilities of network interfaces, such as Intel NICs, to obtain precise timestamps for sent and received packets. Refer to the implementation in `src/timestamping.cpp` and `src/timestamping.h` for examples.
* **Regularly monitor and adjust**: Continuously monitor the synchronization accuracy and make necessary adjustments to account for any drift or variations in network conditions.
* **Optimize software configurations**: Adjust software configurations, such as W32Time registry settings, to optimize polling intervals and update behavior. Refer to the instructions in the issue discussion for enabling PTP support in W32Time.
* **Combine multiple methods**: Integrate multiple synchronization methods, such as using W32Time for general synchronization and custom logic for precise adjustments, to achieve higher accuracy.
* **Use diagnostic tools**: Employ diagnostic tools and static code analysis to identify and fix potential issues that may affect timing accuracy. Refer to the CI pipeline in `.github/workflows/ci.yml` for examples of static code analysis and resource management checks.

## Developing Custom Sync Logic for gPTP Precision

To develop custom sync logic for gPTP precision, consider the following steps:

1. **Use Intel hardware timestamping**: Enable Intel hardware timestamping on compatible Intel NICs (such as the i210) by following the guidance from Intel. This may involve configuring the network adapter settings and ensuring that the appropriate drivers are installed. Refer to the implementation in `src/timestamping.cpp` and `src/timestamping.h` for examples of how to use the `GetInterfaceActiveTimestampCapabilities` function to check if a network interface supports hardware timestamping.

2. **Leverage packet timestamping**: Use packet timestamping to timestamp packets as they are sent or received. This can be done by leveraging the Windows IP Helper API functions and integrating them into your custom application or daemon. Refer to the `README.md` file for an overview of using the IP Helper API for gPTP and timestamping functionalities.

3. **Develop custom sync logic**: Create or port a gPTP daemon that directly uses hardware timestamping APIs and bypasses W32Time for precise time adjustments. This involves developing custom synchronization logic that calculates and adjusts time offsets within your custom application or daemon. Refer to the `README.md` file for build and run instructions for the gPTP daemon on both Linux and Windows platforms.

4. **Regularly calibrate timing**: Regularly calibrate timing against a reliable source to account for packet delay variations and driver limitations that can introduce drift. Refer to the best practices for timing calibration mentioned in the issue discussion.

5. **Optimize software configurations**: Adjust software configurations, such as W32Time registry settings, to optimize polling intervals and update behavior. Refer to the instructions in the issue discussion for enabling PTP support in W32Time.

6. **Combine multiple methods**: Integrate multiple synchronization methods, such as using W32Time for general synchronization and custom logic for precise adjustments, to achieve higher accuracy.

7. **Use diagnostic tools**: Employ diagnostic tools and static code analysis to identify and fix potential issues that may affect timing accuracy. Refer to the CI pipeline in `.github/workflows/ci.yml` for examples of static code analysis and resource management checks.

## Accessing Intel Hardware Timestamps on Windows

To access Intel hardware timestamps on Windows, you can use the Windows IP Helper API. Here are the steps to achieve this:

1. Include the necessary headers in your code: `#include <Iphlpapi.h>` and `#include <Windows.h>`.
2. Link against the `Iphlpapi.lib` library.
3. Use the `GetInterfaceActiveTimestampCapabilities` function to check if a network interface supports hardware timestamping. This function takes a `NET_LUID` (Locally Unique Identifier) or `InterfaceIndex` of a network adapter and fills a `MIB_INTERFACE_TIMESTAMP_CAPABILITIES` structure with details on whether timestamping is supported and if both send and receive timestamps are available.
4. You can refer to the implementation in `src/timestamping.cpp` and the corresponding header file `src/timestamping.h` for an example of how to use this function.

## Enabling PTP Support in W32Time

To enable PTP support in W32Time, follow these steps:

1. Open the Registry Editor by typing `regedit` in the Run dialog (Win + R) and pressing Enter.
2. Navigate to the following registry key: `HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\W32Time\Config`.
3. Create or modify the `AnnounceFlags` DWORD value and set it to `5` to enable PTP.
4. Navigate to the following registry key: `HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\W32Time\TimeProviders\NtpClient`.
5. Create or modify the `Enabled` DWORD value and set it to `1` to enable the NTP client.
6. Create or modify the `SpecialPollInterval` DWORD value to set the polling interval in seconds (e.g., `900` for 15 minutes).
7. Navigate to the following registry key: `HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\W32Time\TimeProviders\NtpServer`.
8. Create or modify the `Enabled` DWORD value and set it to `1` to enable the NTP server.
9. Restart the Windows Time service by running the following commands in an elevated Command Prompt:
   * `net stop w32time`
   * `net start w32time`

## Limitations of W32Time for gPTP Precision

The limitations of W32Time for gPTP precision include:

* **Driver limitations**: Windows drivers for Intel NICs do not fully support IEEE 802.1AS (gPTP) mechanisms, which limits precise timestamping and clock management needed for sub-microsecond synchronization.
* **Limited PTP implementation**: While PTP can improve synchronization over NTP, W32Time lacks the high-precision adjustments and rapid synchronization necessary for gPTP.
* **Registry configuration**: Adjusting W32Time registry settings can optimize polling intervals and update behavior, but it still does not achieve gPTP precision.
* **General synchronization**: W32Time provides basic synchronization but will not achieve gPTP precision, which is essential for applications requiring sub-microsecond accuracy.

## Performance Impacts of Enabling Hardware Timestamping

Enabling hardware timestamping can have several performance impacts on your system:

* **Increased CPU usage**: Hardware timestamping can increase CPU usage due to the additional processing required to handle and manage the timestamps. This can be observed in the build and run processes defined in the CI pipeline in `.github/workflows/ci.yml`.
* **Network performance**: Enabling hardware timestamping may introduce slight delays in packet processing, as the network interface card (NIC) needs to generate and attach timestamps to packets. This can affect overall network performance, especially in high-throughput environments.
* **Resource management**: Proper resource management is crucial when enabling hardware timestamping. The CI pipeline in `.github/workflows/ci.yml` and the `Makefile` in `linux/Makefile` include resource management checks to ensure efficient use of resources and minimize potential memory leaks.
* **Increased complexity**: Integrating hardware timestamping with packet timestamping and custom synchronization logic can increase the complexity of your application. This is evident in the implementation of the `GetInterfaceActiveTimestampCapabilities` function in `src/timestamping.cpp` and the corresponding header file `src/timestamping.h`.
* **Calibration and synchronization**: Regular calibration and synchronization against a reliable time source are necessary to maintain accurate timing. This can introduce additional overhead and complexity in your application, as mentioned in the best practices for timing calibration in the `README.md`.
