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
  set(WPCAP_DIR $ENV{WPCAP_DIR})
  include_directories(${WPCAP_DIR}/Include)
  link_directories(${WPCAP_DIR}/Lib)
  add_executable(gptp ${SOURCES})
  target_link_libraries(gptp Iphlpapi wpcap Packet)
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
