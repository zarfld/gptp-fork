# Modern gPTP Daemon

This repository contains a **modernized** Intel-provided gPTP daemon for clock synchronization on Linux and Windows platforms. The codebase has been significantly refactored to use modern C++17 standards, improved architecture, and best practices.

> **📋 Note**: This is a modernized fork with substantial improvements in code quality, maintainability, and platform support. See [MODERNIZATION.md](MODERNIZATION.md) for detailed information about the modernization process.

## 🚀 Key Features

- **Modern C++17**: Leveraging modern language features and standards
- **Cross-Platform**: Comprehensive Windows support, Linux-ready architecture
- **Type-Safe Error Handling**: Modern Result<T> pattern replacing error codes
- **RAII Memory Management**: Smart pointers and automatic resource cleanup
- **Modular Architecture**: Clean separation of platform-specific and core logic
- **Modern Logging**: Structured, thread-safe logging with multiple levels
- **Configuration Management**: Type-safe configuration with validation
- **Testing Ready**: Google Test integration for comprehensive testing

## 🏗️ Architecture

```
src/
├── core/                    # Core gPTP logic and interfaces
├── platform/                # Platform-specific implementations
├── networking/              # Network abstraction layer
├── utils/                   # Utility classes (logging, config)
└── main.cpp                 # Modern main application
```

## Building and Running the gPTP Daemon

### Prerequisites

#### Windows
* **Visual Studio 2019 or later** (for C++17 support)
* **CMake 3.16** or later
* **Npcap Developer's Pack (Npcap SDK)**: Download from [Npcap Developer's Pack](https://npcap.com/)
* **Windows 10 or later** (for modern timestamp APIs)

#### Linux (Future Support)
* **GCC 7+** or **Clang 5+** (for C++17 support)
* **CMake 3.16** or later
* **Development packages**: libcap-dev, etc.

### Environment Setup

#### Windows
```powershell
# Set required environment variables
$env:NPCAP_DIR = "C:\path\to\npcap-sdk"
$env:VSCMD_DEBUG = "3"
$env:VSCMD_SKIP_SENDTELEMETRY = "1"
```

### Build Instructions

#### Modern CMake Build (Recommended)
```bash
# Clone the repository
git clone https://github.com/your-repo/gptp-fork.git
cd gptp-fork

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . --config Release

# Optional: Build with tests
cmake .. -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

#### Platform-Specific Builds

**Windows:**
```powershell
# Using Visual Studio
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release

# Or use the provided task
# Run "Build (Windows)" task in VS Code
```

**Linux:** (When implemented)
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Using Visual Studio Project Files

#### Build Instructions

1. Clone the repository from GitHub.
2. Open the `gptp.sln` solution file in Microsoft Visual Studio.
3. Build the project using the `Release` configuration.

#### Run Instructions

### Running the Daemon

#### Basic Usage
```bash
# Scan all interfaces and show capabilities
./gptp

# Use specific interface (when interface selection is implemented)
./gptp eth0                    # Linux
./gptp "Local Area Connection" # Windows
```

#### With Configuration File
```bash
# Create configuration file
cp gptp.conf.example gptp.conf
# Edit configuration as needed
./gptp --config gptp.conf
```

#### Environment Variables
```bash
# Set preferred interface
export GPTP_INTERFACE="eth0"
export GPTP_LOG_LEVEL="DEBUG"
export GPTP_HARDWARE_TS="true"

./gptp
```

## 🧪 Testing

```bash
# Build with tests enabled
cmake .. -DBUILD_TESTS=ON

# Run tests  
ctest --output-on-failure

# Or run directly
./gptp_tests
```

## 📋 Configuration

The daemon supports multiple configuration methods:

1. **Configuration File**: `gptp.conf` (see `gptp.conf` for example)
2. **Environment Variables**: `GPTP_*` prefixed variables
3. **Command Line**: Future implementation

### Configuration Options

- `preferred_interface`: Network interface to use
- `log_level`: TRACE, DEBUG, INFO, WARN, ERROR, FATAL
- `sync_interval_ms`: Sync message interval (default: 125ms)
- `hardware_timestamping_preferred`: Prefer hardware timestamping
- And many more (see `gptp.conf`)

## 🔧 Development

### Project Structure
```
├── src/
│   ├── core/           # Core gPTP interfaces
│   ├── platform/       # Platform implementations  
│   ├── utils/          # Utilities (logging, config)
│   └── main.cpp        # Application entry point
├── tests/              # Unit tests
├── include/            # Public headers
└── legacy/             # Original implementation
```

### Adding Platform Support
1. Implement `ITimestampProvider` interface
2. Add platform-specific source to CMakeLists.txt
3. Update factory in `timestamp_provider.cpp`

## 📖 Documentation

- [MODERNIZATION.md](MODERNIZATION.md) - Detailed modernization information
- [API Documentation] - Generated with Doxygen (future)
- [Development Guide] - Contributing guidelines (future)

## 🤝 Contributing

1. Follow modern C++17 standards
2. Use RAII and smart pointers
3. Include comprehensive tests
4. Update documentation

## 📄 License

Same as original Intel gPTP implementation.

## 🔄 Migration from Original

If migrating from the original implementation:
1. Original code preserved in `legacy/` directory
2. Configuration syntax updated (see `gptp.conf`)
3. Command line arguments may differ
4. Log format modernized

## ⚡ Performance

The modernized version includes:
- Zero-copy networking operations where possible
- Efficient memory management with smart pointers
- Modern compiler optimizations (C++17)
- Reduced system call overhead

## 🚨 Known Limitations

- Linux implementation not yet complete
- Full gPTP protocol implementation in progress
- Service/daemon mode not yet implemented
- Configuration hot-reload not yet supported

For the complete original implementation, see files in the `legacy/` directory.

## GitHub Actions CI Pipeline

A GitHub Actions CI pipeline has been added to compile the code for both Linux and Windows platforms. The CI pipeline sets up the environment, installs dependencies, and runs the build commands for both platforms.

### Workflow File

The workflow file is located in the `.github/workflows` directory.

### Linux CI Pipeline

The pipeline installs `cmake`, `doxygen`, and `graphviz`, and runs the build commands.

### Windows CI Pipeline

The pipeline installs Npcap, CMAKE, Visual Studio, sets the `NPCAP_DIR` environment variable, and runs the build commands.

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

The `CMakeLists.txt` file supports different architectures (e.g., `x64`, `x86`, `I210`, `generic`, `IntelCE`) and handles dependencies like `Npcap` and environment variables such as `NPCAP_DIR`.

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
