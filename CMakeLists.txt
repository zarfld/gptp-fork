cmake_minimum_required(VERSION 3.16)

project(gptp VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard to C++17 for modern features
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set default build type to Release
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Enable modern CMake policies
cmake_policy(SET CMP0048 NEW)  # project() command manages VERSION variables
cmake_policy(SET CMP0077 NEW)  # option() honors normal variables

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/include)

# Optional: include dynamically found iphlpapi.h path from GitHub Actions
if(DEFINED ENV{IPHLPAPI_INCLUDE_DIR_WIN})
  message(STATUS "Using dynamic Iphlpapi include path: $ENV{IPHLPAPI_INCLUDE_DIR_WIN}")
  include_directories("$ENV{IPHLPAPI_INCLUDE_DIR_WIN}")
endif()

# Modern C++ compiler flags
if(MSVC)
    # MSVC specific flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /EHsc")
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    # GCC/Clang specific flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
endif()

# Source files - now using modern structure
set(CORE_SOURCES
  src/core/timestamp_provider.cpp
  src/core/gptp_state_machines.cpp
  src/core/gptp_clock.cpp
  src/core/bmca.cpp
  src/core/clock_servo.cpp
)

set(NETWORKING_SOURCES
  src/networking/socket_manager.cpp
  src/networking/packet_builder.cpp
)

set(UTILS_SOURCES
  src/utils/configuration.cpp
)

set(COMMON_SOURCES
  src/main.cpp
  ${CORE_SOURCES}
  ${NETWORKING_SOURCES}
  ${UTILS_SOURCES}
)

# Platform-specific sources
if(WIN32)
  list(APPEND COMMON_SOURCES
    src/platform/windows_timestamp_provider.cpp
    src/platform/windows_adapter_detector.cpp
    src/platform/rme_adapter_detector.cpp
    src/networking/windows_socket.cpp
  )
endif()

if(UNIX AND NOT APPLE)
  # Linux sources
  list(APPEND COMMON_SOURCES
    src/platform/linux_timestamp_provider.cpp
    src/platform/linux_adapter_detector.cpp
    src/networking/linux_socket.cpp
  )
endif()

# Platform-specific configurations
if(WIN32)
  # Windows-specific configurations
  add_definitions(-DWIN32_LEAN_AND_MEAN)
  add_definitions(-DNOMINMAX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
  
  set(NPCAP_DIR $ENV{NPCAP_DIR})
  if(NPCAP_DIR AND EXISTS "${NPCAP_DIR}/Include" AND EXISTS "${NPCAP_DIR}/Lib")
    include_directories(${NPCAP_DIR}/Include)
    
    # Use x64 libraries for 64-bit build
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(NPCAP_LIB_DIR "${NPCAP_DIR}/Lib/x64")
    else()
      set(NPCAP_LIB_DIR "${NPCAP_DIR}/Lib/x86")
    endif()
    
    link_directories(${NPCAP_LIB_DIR})
    
    # Check if the actual library files exist (wpcap.lib instead of npcap.lib)
    if(EXISTS "${NPCAP_LIB_DIR}/wpcap.lib" AND EXISTS "${NPCAP_LIB_DIR}/Packet.lib")
      set(NPCAP_LIBS wpcap Packet)
      message(STATUS "Using NPCAP directory: ${NPCAP_DIR}")
      message(STATUS "Using NPCAP libraries: wpcap, Packet from ${NPCAP_LIB_DIR}")
    else()
      set(NPCAP_LIBS)
      message(WARNING "NPCAP library files not found in ${NPCAP_LIB_DIR}. Building without NPCAP support.")
    endif()
  else()
    set(NPCAP_LIBS)
    message(WARNING "NPCAP_DIR not set or directory not found. Building without NPCAP support.")
  endif()

  add_executable(gptp ${COMMON_SOURCES})
  
  # Add discovered iphlpapi.h path
  set(IPHLPAPI_INCLUDE_DIR_WIN $ENV{IPHLPAPI_INCLUDE_DIR_WIN})
  if(IPHLPAPI_INCLUDE_DIR_WIN)
  
    # Derive base path from full path (strip trailing '/um')
    get_filename_component(WIN_SDK_BASE "${IPHLPAPI_INCLUDE_DIR_WIN}" DIRECTORY)
  
    message(STATUS "Using Windows SDK base path: ${WIN_SDK_BASE}")
  
    # Include all necessary directories from SDK
    include_directories(
      "${WIN_SDK_BASE}/um"
      "${WIN_SDK_BASE}/shared"
      "${WIN_SDK_BASE}/ucrt"
      "${WIN_SDK_BASE}/winrt"  # optional but safe
    )
  
    target_include_directories(gptp BEFORE PRIVATE ${IPHLPAPI_INCLUDE_DIR_WIN})
  endif()
  message(STATUS "Using Iphlpapi include path: ${IPHLPAPI_INCLUDE_DIR_WIN}")

  # Link Windows-specific libraries
  target_link_libraries(gptp 
    Iphlpapi 
    ws2_32
    ${NPCAP_LIBS}
  )
  
  # Set Windows-specific properties
  set_target_properties(gptp PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  )
else()
  # Linux-specific configurations
  add_executable(gptp ${COMMON_SOURCES})
  
  # Add Linux-specific definitions
  target_compile_definitions(gptp PRIVATE 
    _GNU_SOURCE
    __STDC_FORMAT_MACROS
  )
  
  target_link_libraries(gptp pthread)
  
  # Set Linux-specific properties
  set_target_properties(gptp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
  )
endif()

# Add testing support
option(BUILD_TESTS "Build test suite" OFF)

if(BUILD_TESTS)
  enable_testing()
  
  # Find or download Google Test
  find_package(GTest QUIET)
  if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/03597a01ee50ed33e9fd7188a01d5b69d4e8c6b1.zip
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
  endif()
  
  # Test executable
  add_executable(gptp_tests
    tests/test_main.cpp
    tests/test_timestamp_provider.cpp
    ${CORE_SOURCES}
  )
  
  if(WIN32)
    target_sources(gptp_tests PRIVATE src/platform/windows_timestamp_provider.cpp)
  endif()
  
  if(UNIX AND NOT APPLE)
    target_sources(gptp_tests PRIVATE src/platform/linux_timestamp_provider.cpp)
  endif()
  
  target_include_directories(gptp_tests PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
  )
  
  target_link_libraries(gptp_tests 
    gtest_main
    gtest
  )
  
  if(WIN32)
    target_link_libraries(gptp_tests Iphlpapi ws2_32)
  else()
    target_link_libraries(gptp_tests pthread)
  endif()
  
  include(GoogleTest)
  gtest_discover_tests(gptp_tests)
endif()

# Architecture-specific configurations
if(ARCH STREQUAL "I210")
  add_definitions(-DARCH_I210)
elseif(ARCH STREQUAL "generic")
  add_definitions(-DARCH_GENERIC)
elseif(ARCH STREQUAL "IntelCE")
  add_definitions(-DARCH_INTELCE)
endif()

# Add static analysis targets for Linux builds
if(UNIX)
  # Find static analysis tools
  find_program(CPPCHECK_EXECUTABLE cppcheck)
  find_program(CLANG_TIDY_EXECUTABLE clang-tidy)
  find_program(VALGRIND_EXECUTABLE valgrind)
  
  # Static analysis target
  if(CPPCHECK_EXECUTABLE)
    add_custom_target(static-analysis
      COMMAND ${CPPCHECK_EXECUTABLE} 
        --enable=all 
        --inconclusive 
        --quiet 
        --suppress=missingIncludeSystem 
        --suppress=unusedFunction
        --platform=unix64
        -D__linux__
        -U_WIN32
        ${CMAKE_SOURCE_DIR}/src
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMENT "Running static code analysis with cppcheck"
    )
  else()
    add_custom_target(static-analysis
      COMMAND echo "cppcheck not found, skipping static analysis"
      COMMENT "Static analysis skipped - cppcheck not available"
    )
  endif()
  
  # Resource management checks target
  if(VALGRIND_EXECUTABLE)
    add_custom_target(resource-checks
      COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full ./gptp
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMENT "Running resource management checks with valgrind"
      DEPENDS gptp
    )
  else()
    add_custom_target(resource-checks
      COMMAND echo "valgrind not found, skipping resource checks"
      COMMENT "Resource checks skipped - valgrind not available"
    )
  endif()
endif()

# Environment variables for Visual Studio 2022
if(WIN32)
  set(ENV{VSCMD_DEBUG} 3)
  set(ENV{VSCMD_SKIP_SENDTELEMETRY} 1)
endif()

# Add a target to generate the Visual Studio project files
if(WIN32)
  set(CMAKE_GENERATOR "Visual Studio 17 2022")
  set(CMAKE_GENERATOR_PLATFORM "x64")
  set(CMAKE_GENERATOR_TOOLSET "v142")
  set(CMAKE_CONFIGURATION_TYPES "Release")
  set(CMAKE_VS_INCLUDE_INSTALL_TO_DEFAULT_BUILD 1)
  set(CMAKE_VS_INCLUDE_PACKAGE_TO_DEFAULT_BUILD 1)
  set(CMAKE_VS_INCLUDE_TEST_TO_DEFAULT_BUILD 1)
  set(CMAKE_VS_INCLUDE_DEPLOY_TO_DEFAULT_BUILD 1)
  set(CMAKE_VS_INCLUDE_CLEAN_TO_DEFAULT_BUILD 1)
  set(CMAKE_VS_INCLUDE_REBUILD_TO_DEFAULT_BUILD 1)

# Add tests subdirectory with unique binary directory
add_subdirectory(tests ${CMAKE_BINARY_DIR}/test_build)
  set(CMAKE_VS_INCLUDE_BUILD_TO_DEFAULT_BUILD 1)
 endif()
