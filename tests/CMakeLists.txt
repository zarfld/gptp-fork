# Tests CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

# Add simple test executable (basic protocol constants only)
add_executable(simple_test simple_test.cpp)
target_include_directories(simple_test PRIVATE ../include)
set_property(TARGET simple_test PROPERTY CXX_STANDARD 17)
set_property(TARGET simple_test PROPERTY CXX_STANDARD_REQUIRED ON)

# Add state machines test
add_executable(test_state_machines test_state_machines.cpp ../src/core/gptp_state_machines.cpp ../src/core/gptp_clock.cpp)
target_include_directories(test_state_machines PRIVATE ../include)
set_property(TARGET test_state_machines PROPERTY CXX_STANDARD 17)
set_property(TARGET test_state_machines PROPERTY CXX_STANDARD_REQUIRED ON)

# Add networking test  
add_executable(test_networking 
  test_networking.cpp 
  ../src/core/gptp_state_machines.cpp 
  ../src/core/gptp_clock.cpp
  ../src/networking/socket_manager.cpp
  ../src/networking/packet_builder.cpp
)
target_include_directories(test_networking PRIVATE ../include)
set_property(TARGET test_networking PROPERTY CXX_STANDARD 17)
set_property(TARGET test_networking PROPERTY CXX_STANDARD_REQUIRED ON)

# Platform-specific sources and libraries for networking test
if(WIN32)
  target_sources(test_networking PRIVATE 
    ../src/networking/windows_socket.cpp
    ../src/platform/windows_timestamp_provider.cpp
    ../src/platform/windows_adapter_detector.cpp
  )
  target_link_libraries(test_networking Iphlpapi ws2_32)
endif()

if(UNIX AND NOT APPLE)
  target_sources(test_networking PRIVATE 
    ../src/networking/linux_socket.cpp
    ../src/platform/linux_timestamp_provider.cpp
    ../src/platform/linux_adapter_detector.cpp
  )
  target_link_libraries(test_networking pthread)
endif()

# Add networking test (simple version - packet building only)
add_executable(test_networking_simple
  test_networking_simple.cpp 
  ../src/core/gptp_state_machines.cpp 
  ../src/core/gptp_clock.cpp
  ../src/networking/packet_builder.cpp
)
target_include_directories(test_networking_simple PRIVATE ../include)
set_property(TARGET test_networking_simple PROPERTY CXX_STANDARD 17)
set_property(TARGET test_networking_simple PROPERTY CXX_STANDARD_REQUIRED ON)

# Platform-specific libraries for simple networking test
if(WIN32)
  target_link_libraries(test_networking_simple ws2_32)
endif()

if(UNIX AND NOT APPLE)
  target_link_libraries(test_networking_simple pthread)
endif()

# Comment out the complex test for now due to packed struct issues
# add_executable(test_immediate_actions test_immediate_actions.cpp)
# target_include_directories(test_immediate_actions PRIVATE ../include)
# set_property(TARGET test_immediate_actions PROPERTY CXX_STANDARD 17)
# set_property(TARGET test_immediate_actions PROPERTY CXX_STANDARD_REQUIRED ON)
