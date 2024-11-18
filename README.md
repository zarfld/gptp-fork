# gptp Daemon

This repository contains an example Intel-provided gptp daemon for clock synchronization on Linux and Windows platforms. The daemon communicates with other processes through a named pipe called "gptp-update" and uses a specific message format.

## Building and Running the gptp Daemon

### Linux

#### Build Dependencies

* `cmake`: Install using `sudo apt-get install cmake`
* `doxygen`: Install using `sudo apt-get install doxygen`
* `graphviz`: Install using `sudo apt-get install graphviz`

#### Build Instructions

1. Clone the repository from GitHub.
2. Navigate to the `linux` directory.
3. Execute the build makefile using the appropriate command:
   * For I210: `ARCH=I210 make clean all`
   * For generic Linux: `make clean all`
   * For Intel CE 5100 Platforms: `ARCH=IntelCE make clean all`

#### Run Instructions

To execute the daemon, run `./daemon_cl <interface-name>`, replacing `<interface-name>` with the name of your network interface (e.g., `eth0`).

### Windows

#### Build Dependencies

* `WinPCAP Developer's Pack (WpdPack)`: Download from WinPCAP Developer's Pack
* `CMAKE 3.2.2` or later
* `Microsoft Visual Studio 2022` or later
* Environment variable `WPCAP_DIR` must be defined to the directory where WinPcap is installed
* `WinPCAP` must also be installed on any machine where the daemon runs
* `VSCMD_DEBUG` environment variable set to `3` to enable detailed logging for Visual Studio command-line tools
* `VSCMD_SKIP_SENDTELEMETRY` environment variable set to `1` to disable telemetry data collection by Visual Studio command-line tools

#### Build Instructions

1. Clone the repository from GitHub.
2. Open the project in Microsoft Visual Studio.
3. Configure the project to use the installed dependencies:
   * Set the `WPCAP_DIR` environment variable to the directory where WinPcap is installed.
4. Build the project using Microsoft Visual Studio.

#### Run Instructions

To execute the daemon, run `daemon_cl.exe xx-xx-xx-xx-xx-xx`, replacing `xx-xx-xx-xx-xx-xx` with the MAC address of the local interface.

## GitHub Actions CI Pipeline

A GitHub Actions CI pipeline has been added to compile the code for both Linux and Windows platforms. The CI pipeline sets up the environment, installs dependencies, and runs the build commands for both platforms.

### Workflow File

The workflow file is located in the `.github/workflows` directory.

### Linux CI Pipeline

The pipeline installs `cmake`, `doxygen`, and `graphviz`, and runs the build commands.

### Windows CI Pipeline

The pipeline installs WinPCAP, CMAKE, Visual Studio, sets the `WPCAP_DIR` environment variable, and runs the build commands.

### Updated CI Pipeline Schedule

The `build-windows (x64)` job runs on check-in. Other build jobs run on a weekly interval.

## Cloning GitHub Issues

A script has been added to clone GitHub issues from the original repository to the forked repository using the GitHub API. The script fetches issues from the original repository using the `GET /repos/{owner}/{repo}/issues` endpoint and creates issues in the forked repository using the `POST /repos/{owner}/{repo}/issues` endpoint. The script handles pagination and includes error handling.

## Syncing with the Original Repository

A GitHub Actions workflow has been created to automate syncing the forked repository with the original repository. The workflow runs at regular intervals to pull changes from the original repository and push them to the forked repository. The workflow uses the `actions/checkout` action to pull changes from the original repository and ensures that the forked repository remains consistent with the original repository. The workflow now uses `git pull upstream main` instead of `git merge upstream/main` to pull and merge changes from the `upstream/main` branch into the `main` branch.

## CMakeLists.txt

A `CMakeLists.txt` file has been added to the root directory of the repository. This file includes the necessary configuration for building the project on both Linux and Windows platforms.

### Usage Instructions

#### Linux

1. Clone the repository from GitHub.
2. Navigate to the root directory of the repository.
3. Create a build directory and navigate into it:
   ```sh
   mkdir build
   cd build
   ```
4. Run `cmake` to configure the project:
   ```sh
   cmake ..
   ```
5. Build the project:
   ```sh
   make
   ```

#### Windows

1. Clone the repository from GitHub.
2. Navigate to the root directory of the repository.
3. Create a build directory and navigate into it:
   ```sh
   mkdir build
   cd build
   ```
4. Run `cmake` to configure the project:
   ```sh
   cmake ..
   ```
5. Build the project using Microsoft Visual Studio:
   ```sh
   msbuild /p:Configuration=Release
   ```

The `CMakeLists.txt` file supports different architectures (e.g., `x64`, `x86`, `I210`, `generic`, `IntelCE`) and handles dependencies like `WinPCAP` and environment variables such as `WPCAP_DIR`.

## Using Visual Studio Code Tasks

A `.vscode` directory has been added to the repository, containing a `tasks.json` file that defines tasks for building and testing the project. These tasks can be run directly from Visual Studio Code.

### Available Tasks

The following tasks are available in the `tasks.json` file:

* Build the project using CMake
* Run static code analysis using `cppcheck` and `clang-tidy`
* Run resource management checks using `valgrind` on Linux
* Build the project with IP Helper API on Linux
* Run the gptp daemon on both Linux and Windows platforms

### Running Tasks

To run a task in Visual Studio Code:

1. Open the Command Palette (Ctrl+Shift+P or Cmd+Shift+P on macOS).
2. Type `Tasks: Run Task` and select it.
3. Choose the desired task from the list.

By using these tasks, you can easily build and test the project directly from Visual Studio Code.

### Configuring W32Time for PTP Support on Windows

To configure W32Time for PTP support on Windows, follow these steps:

1. Open a command prompt with administrative privileges.
2. Run the following commands to configure W32Time:
   ```sh
   reg add "HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\W32Time\Config" /v AnnounceFlags /t REG_DWORD /d 5 /f
   reg add "HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\W32Time\TimeProviders\NtpClient" /v Enabled /t REG_DWORD /d 1 /f
   reg add "HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\W32Time\TimeProviders\NtpClient" /v SpecialPollInterval /t REG_DWORD /d 900 /f
   reg add "HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\W32Time\TimeProviders\NtpServer" /v Enabled /t REG_DWORD /d 1 /f
   net stop w32time
   net start w32time
   ```

### Integrating Intel Hardware Timestamping on Windows

To integrate Intel hardware timestamping on Windows, follow these steps:

1. Open a command prompt with administrative privileges.
2. Navigate to the `src` directory.
3. Compile the `timestamping.cpp` file:
   ```sh
   cl /EHsc timestamping.cpp
   ```
4. Run the `timestamping.exe` executable:
   ```sh
   timestamping.exe
   ```

To build and run the `IntegrateIntelHardwareTimestampingWithPacketTimestamping` function, follow these steps:

1. Open a command prompt with administrative privileges.
2. Navigate to the `src` directory.
3. Compile the `timestamping.cpp` file:
   ```sh
   cl /EHsc timestamping.cpp
   ```
4. Run the `timestamping.exe` executable with the `IntegrateIntelHardwareTimestampingWithPacketTimestamping` function:
   ```sh
   timestamping.exe IntegrateIntelHardwareTimestampingWithPacketTimestamping
   ```
