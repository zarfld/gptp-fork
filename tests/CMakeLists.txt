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

# Add BMCA test
add_executable(test_bmca test_bmca.cpp ../src/core/bmca.cpp)
target_include_directories(test_bmca PRIVATE ../include)
set_property(TARGET test_bmca PROPERTY CXX_STANDARD 17)
set_property(TARGET test_bmca PROPERTY CXX_STANDARD_REQUIRED ON)

# Platform-specific libraries for BMCA test
if(WIN32)
  target_link_libraries(test_bmca ws2_32)
endif()

# Add Clock Servo test
add_executable(test_clock_servo test_clock_servo.cpp ../src/core/clock_servo.cpp)
target_include_directories(test_clock_servo PRIVATE ../include)
set_property(TARGET test_clock_servo PROPERTY CXX_STANDARD 17)
set_property(TARGET test_clock_servo PROPERTY CXX_STANDARD_REQUIRED ON)

# Platform-specific libraries for Clock Servo test
if(WIN32)
  target_link_libraries(test_clock_servo ws2_32)
endif()

# Add Network Integration test
add_executable(test_network_integration test_network_integration.cpp ../src/core/bmca.cpp ../src/core/clock_servo.cpp)
target_include_directories(test_network_integration PRIVATE ../include)
set_property(TARGET test_network_integration PROPERTY CXX_STANDARD 17)
set_property(TARGET test_network_integration PROPERTY CXX_STANDARD_REQUIRED ON)

# Platform-specific libraries for Network Integration test
if(WIN32)
  target_link_libraries(test_network_integration ws2_32)
endif()

# Add Phase 5 Network Integration Demo
add_executable(phase5_network_demo phase5_network_integration_demo.cpp ../src/core/bmca.cpp ../src/core/clock_servo.cpp)
target_include_directories(phase5_network_demo PRIVATE ../include)
set_property(TARGET phase5_network_demo PROPERTY CXX_STANDARD 17)
set_property(TARGET phase5_network_demo PROPERTY CXX_STANDARD_REQUIRED ON)

# Platform-specific libraries for Phase 5 demo
if(WIN32)
  target_link_libraries(phase5_network_demo ws2_32)
endif()

# Add Integration test
add_executable(test_integration test_integration.cpp ../src/core/bmca.cpp ../src/core/clock_servo.cpp)
target_include_directories(test_integration PRIVATE ../include)
set_property(TARGET test_integration PROPERTY CXX_STANDARD 17)
set_property(TARGET test_integration PROPERTY CXX_STANDARD_REQUIRED ON)

# Platform-specific libraries for Integration test
if(WIN32)
  target_link_libraries(test_integration ws2_32)
endif()

# Add Port Manager test (Network Integration)
add_executable(test_port_manager test_port_manager.cpp ../src/core/gptp_port_manager.cpp ../src/core/bmca.cpp ../src/core/clock_servo.cpp ../src/core/gptp_state_machines.cpp ../src/core/gptp_clock.cpp)
target_include_directories(test_port_manager PRIVATE ../include)
set_property(TARGET test_port_manager PROPERTY CXX_STANDARD 17)
set_property(TARGET test_port_manager PROPERTY CXX_STANDARD_REQUIRED ON)

# Platform-specific libraries for Port Manager test
if(WIN32)
  target_link_libraries(test_port_manager ws2_32)
endif()

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
