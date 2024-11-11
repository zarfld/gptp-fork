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
