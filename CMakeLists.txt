cmake_minimum_required(VERSION 3.2.2)

project(gptp)

# Set default build type to Release
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src)

# Source files
set(SOURCES
  src/timestamping.cpp
)

# Platform-specific configurations
if(WIN32)
  # Windows-specific configurations
  add_definitions(-DWIN32_LEAN_AND_MEAN)
  add_definitions(-DNOMINMAX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
  set(NPCAP_DIR $ENV{NPCAP_DIR})
  include_directories(${NPCAP_DIR}/Include)
  link_directories(${NPCAP_DIR}/Lib)
  add_executable(gptp ${SOURCES})
  target_link_libraries(gptp Iphlpapi npcap Packet)
else()
  # Linux-specific configurations
  add_executable(gptp ${SOURCES})
  target_link_libraries(gptp pthread)
endif()

# Architecture-specific configurations
if(ARCH STREQUAL "I210")
  add_definitions(-DARCH_I210)
elseif(ARCH STREQUAL "generic")
  add_definitions(-DARCH_GENERIC)
elseif(ARCH STREQUAL "IntelCE")
  add_definitions(-DARCH_INTELCE)
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
  set(CMAKE_VS_INCLUDE_BUILD_TO_DEFAULT_BUILD 1)
 endif()
