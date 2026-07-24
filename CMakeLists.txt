cmake_minimum_required(VERSION 3.10)
project(modbus-iec104-gateway C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Version
set(PROJECT_VERSION_MAJOR 1)
set(PROJECT_VERSION_MINOR 0)
set(PROJECT_VERSION_PATCH 0)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Compiler flags
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_BUILD_TYPE MATCHES Release)
    add_compile_options(-O2 -Wall -Wextra)
else()
    add_compile_options(-g -Wall -Wextra)
endif()

# Find required packages
find_package(PkgConfig QUIET)

# Try to find libmodbus
if(PkgConfig_FOUND)
    pkg_check_modules(MODBUS libmodbus)
endif()

if(NOT MODBUS_FOUND)
    message(WARNING "libmodbus not found via pkg-config, trying manual search")
    find_library(MODBUS_LIBRARIES modbus)
    find_path(MODBUS_INCLUDE_DIRS modbus.h)
    if(MODBUS_LIBRARIES AND MODBUS_INCLUDE_DIRS)
        set(MODBUS_FOUND TRUE)
    endif()
endif()

if(NOT MODBUS_FOUND)
    message(FATAL_ERROR "libmodbus not found. Please install libmodbus-dev")
endif()

message(STATUS "Found libmodbus at ${MODBUS_INCLUDE_DIRS}")

# Source files
set(SOURCES
    src/main.c
    src/modbus_master.c
    src/iec104_protocol.c
    src/iec104_server.c
    src/gateway.c
)

set(HEADERS
    src/modbus_master.h
    src/iec104_protocol.h
    src/iec104_server.h
)

# Create executable
add_executable(gateway ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(gateway 
    ${MODBUS_LIBRARIES}
    m
    pthread
)

# Include directories
target_include_directories(gateway PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${MODBUS_INCLUDE_DIRS}
)

# Installation
install(TARGETS gateway RUNTIME DESTINATION bin)
install(FILES config/gateway.conf.example DESTINATION etc)

# Print build info
message(STATUS "")
message(STATUS "Build Configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "  Modbus Include: ${MODBUS_INCLUDE_DIRS}")
message(STATUS "  Modbus Libraries: ${MODBUS_LIBRARIES}")
message(STATUS "  Sources: ${SOURCES}")
message(STATUS "")
