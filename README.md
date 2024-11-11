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

## GitHub Actions CI Pipeline

A GitHub Actions CI pipeline has been added to compile the code for both Linux and Windows platforms. The CI pipeline sets up the environment, installs dependencies, and runs the build commands for both platforms.

### Workflow File

The workflow file is located in the `.github/workflows` directory.

### Linux CI Pipeline

The pipeline installs `cmake`, `doxygen`, `graphviz`, `cppcheck`, and `clang-tidy`, and runs the build commands. The workflow file now uses `lukka/get-cmake@latest` instead of `lukka/get-cmake@v3`.

### Windows CI Pipeline

The pipeline installs WinPCAP, CMAKE, Visual Studio, `cppcheck`, `clang-tidy`, sets the `WPCAP_DIR` environment variable, and runs the build commands. The workflow file now uses `lukka/get-cmake@latest` instead of `lukka/get-cmake@v3`.

## Cloning GitHub Issues

A script has been added to clone GitHub issues from the original repository to the forked repository using the GitHub API. The script fetches issues from the original repository using the `GET /repos/{owner}/{repo}/issues` endpoint and creates issues in the forked repository using the `POST /repos/{owner}/{repo}/issues` endpoint. The script handles pagination and includes error handling.

## Syncing with the Original Repository

A GitHub Actions workflow has been created to automate syncing the forked repository with the original repository. The workflow runs at regular intervals to pull changes from the original repository and push them to the forked repository. The workflow uses the `actions/checkout` action to pull changes from the original repository and ensures that the forked repository remains consistent with the original repository.

## Static Code Analysis Fixes and Resource Management Enhancements

PawelModrzejewski's fork includes several improvements that address issues identified through static code analysis and enhance resource management:

* **Static Code Analysis Fixes**: Fixes for null pointer dereferences, resource management problems, and other issues identified through tools like `cppcheck` and `clang-tidy`.
* **Resource Management Enhancements**: Proper allocation and deallocation of resources, reducing potential memory leaks and improving overall efficiency.
* **Platform-Specific Adjustments**: Modifications tailored for Linux and Windows platforms to enhance compatibility and performance.

By integrating these updates, you can leverage improvements that address known issues and enhance the overall functionality of the gPTP daemon.
